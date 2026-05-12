param(
    [string]$InputPath = "",
    [string]$OutputPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "installer-common.ps1")

if ([string]::IsNullOrWhiteSpace($InputPath)) {
    $inputText = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($inputText)) {
        throw "Provide MSI installer dry-run output with -InputPath or stdin."
    }
    $lines = @($inputText -split "`r?`n")
}
else {
    if (-not (Test-Path -LiteralPath $InputPath)) {
        throw "Dry-run output not found: $InputPath"
    }
    $lines = @(Get-Content -LiteralPath $InputPath)
}

$partitions = New-Object System.Collections.Generic.List[object]
$disks = New-Object System.Collections.Generic.List[object]
$plan = New-Object System.Collections.Generic.List[object]
$result = ""
$writes = $null
$writeDisabled = $true
$dryRunNoWrite = $false

foreach ($line in $lines) {
    if ($line -match '^installer-result:\s*(?<result>.+)$') {
        $result = $Matches["result"].Trim()
    }
    elseif ($line -match '^installer-writes:\s*(?<writes>[0-9]+)$') {
        $writes = [int]$Matches["writes"]
        $dryRunNoWrite = ($writes -eq 0)
    }
    elseif ($line -match '^capabilities:\s*hardware-inventory\s*(?<hw>[01])\s*read-only-block\s*(?<ro>[01])\s*write\s*(?<wr>[01])\s*format\s*(?<fmt>[01])\s*boot-entry\s*(?<boot>[01])') {
        $writeDisabled = ([int]$Matches["wr"] -eq 0) -and ([int]$Matches["fmt"] -eq 0) -and ([int]$Matches["boot"] -eq 0)
    }
    elseif ($line -match '^disk:\s*(?<id>\S+)\s*bytes\s*(?<bytes>[0-9]+)\s*sector-bytes\s*(?<sector>[0-9]+)') {
        $disks.Add([pscustomobject]@{
            id = $Matches["id"]
            bytes = [int64]$Matches["bytes"]
            sectorBytes = [int]$Matches["sector"]
        })
    }
    elseif ($line -match "^partition\s+(?<number>[0-9]+):\s+type\s+(?<type>\S+)\s+label\s+'(?<label>[^']*)'\s+fs\s+(?<fs>\S+)\s+fs-label\s+'(?<fslabel>[^']*)'\s+lba\s+(?<first>[0-9]+)-(?<last>[0-9]+)\s+class\s+(?<class>\S+)\s+reason\s+'(?<reason>[^']*)'\s+writable\s+(?<writable>[01])") {
        $typeGuid = $Matches["type"].ToLowerInvariant()
        $fs = $Matches["fs"]
        $reason = $Matches["reason"]
        $isWindowsEsp = ($typeGuid -eq $Script:M5EfiSystemTypeGuid) -or ($reason -match 'EFI System|unknown ESP')
        $isMsr = ($typeGuid -eq $Script:M5MicrosoftReservedGuid) -or ($reason -match 'Microsoft Reserved')
        $isRecovery = ($typeGuid -eq $Script:M5WindowsRecoveryGuid) -or ($reason -match 'Recovery')
        $isNtfs = ($fs -eq "NTFS") -or ($reason -match 'NTFS')
        $isUnknownFat = (($fs -eq "FAT32") -or ($reason -match 'FAT32')) -and -not $isWindowsEsp
        $isLimitlessTarget = (($typeGuid -eq $Script:M5LimitlessTypeGuid) -or ($typeGuid -eq $Script:M5LimitlessBootTypeGuid) -or ($reason -match 'LimitlessOS target'))
        $partitions.Add([pscustomobject]@{
            number = [int]$Matches["number"]
            typeGuid = $typeGuid
            label = $Matches["label"]
            filesystem = $fs
            filesystemLabel = $Matches["fslabel"]
            firstLba = [int64]$Matches["first"]
            lastLba = [int64]$Matches["last"]
            classification = $Matches["class"]
            reason = $reason
            writable = ([int]$Matches["writable"] -eq 1)
            windowsEsp = $isWindowsEsp
            ntfs = $isNtfs
            microsoftReserved = $isMsr
            recovery = $isRecovery
            unknownFat32 = $isUnknownFat
            limitlessTargetCandidate = $isLimitlessTarget
        })
    }
    elseif ($line -match '^plan:\s*(?<action>\S+)\s+writes\s+(?<writes>[0-9]+)\s+(?<description>.+)$') {
        $plan.Add([pscustomobject]@{
            action = $Matches["action"]
            writes = [int]$Matches["writes"]
            description = $Matches["description"]
        })
    }
}

$partitionArray = @($partitions.ToArray())
$forbidden = @($partitionArray | Where-Object { $_.classification -ne "safe" })
$targets = @($partitionArray | Where-Object { $_.limitlessTargetCandidate -and $_.writable })
$detectedUnallocated = @($plan.ToArray() | Where-Object { $_.action -eq "propose-layout" }).Count -gt 0
$recommendedNextStep = if ($targets.Count -gt 0) {
    "Review dedicated LimitlessOS target candidates; do not write until explicit future install authority is approved."
}
elseif ($detectedUnallocated) {
    "Unallocated/empty target appears available in fixture output; M9 remains dry-run only pending review."
}
else {
    "No approved LimitlessOS target found; keep internal writes disabled and review partition plan."
}

$report = [pscustomobject][ordered]@{
    tool = "parse-msi-dryrun-evidence"
    milestone = "M9 Bare-Metal Validation + MSI Dry-Run Evidence"
    source = if ([string]::IsNullOrWhiteSpace($InputPath)) { "stdin" } else { $InputPath }
    disksDetected = @($disks.ToArray())
    partitionTableType = if ($partitionArray.Count -gt 0) { "GPT" } else { "unknown-or-empty" }
    partitions = $partitionArray
    detectedWindowsEsp = (@($partitionArray | Where-Object windowsEsp).Count -gt 0)
    detectedNtfs = (@($partitionArray | Where-Object ntfs).Count -gt 0)
    detectedMicrosoftReserved = (@($partitionArray | Where-Object microsoftReserved).Count -gt 0)
    detectedRecovery = (@($partitionArray | Where-Object recovery).Count -gt 0)
    detectedUnknownFat32 = (@($partitionArray | Where-Object unknownFat32).Count -gt 0)
    detectedUnallocatedSpace = $detectedUnallocated
    limitlessTargetCandidates = @($targets | ForEach-Object { $_.number })
    forbiddenPartitions = @($forbidden | ForEach-Object { $_.number })
    writeDisabledStatus = $writeDisabled
    dryRunNoWriteStatus = ($dryRunNoWrite -or ($writes -eq 0))
    installerResult = $result
    realInstallApprovalStatus = $false
    recommendedNextStep = $recommendedNextStep
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $parent = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
}

$report | ConvertTo-Json -Depth 12
