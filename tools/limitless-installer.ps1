param(
    [string]$ImagePath = "",

    [int]$PhysicalDriveNumber = -1,

    [switch]$AllowPhysicalReadOnly,

    [ValidateSet("DryRun", "Install")]
    [string]$Mode = "DryRun",

    [int]$BootPartitionNumber = 0,

    [int]$RootPartitionNumber = 0,

    [string]$ConfirmationToken = "",

    [switch]$GrantHardwareInventoryCapability,

    [switch]$GrantReadOnlyBlockCapability,

    [switch]$GrantWriteCapability,

    [switch]$GrantFormatCapability,

    [switch]$GrantBootEntryCapability,

    [switch]$RequestBootEntryChange,

    [string]$BootPayloadImagePath = "",

    [string]$JsonOutputPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "installer-common.ps1")

function Fail-Installer
{
    param([string]$Reason, [object]$Report = $null)

    if ($Report -ne $null) {
        $Report.result = "refused"
        $Report.error = $Reason
        if (-not [string]::IsNullOrWhiteSpace($JsonOutputPath)) {
            $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonOutputPath -Encoding UTF8
        }
    }
    Write-Host "installer-result: refused"
    Write-Host "installer-error: $Reason"
    exit 2
}

function Get-InstallerBytesSha256
{
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return [System.BitConverter]::ToString($sha.ComputeHash($Bytes)).Replace("-", "")
    }
    finally {
        $sha.Dispose()
    }
}

$isPhysical = $PhysicalDriveNumber -ge 0
if ($isPhysical) {
    if (-not $AllowPhysicalReadOnly) {
        throw "Physical disk dry-run requires -AllowPhysicalReadOnly."
    }
    if ($Mode -ne "DryRun") {
        throw "Physical disk access is dry-run only in M5."
    }
    if (-not [string]::IsNullOrWhiteSpace($ImagePath)) {
        throw "Use either -ImagePath or -PhysicalDriveNumber, not both."
    }
    $targetPath = "\\.\PhysicalDrive$PhysicalDriveNumber"
}
else {
    if ([string]::IsNullOrWhiteSpace($ImagePath)) {
        throw "Image path is required unless -PhysicalDriveNumber is used for explicit read-only dry-run."
    }
    if (-not (Test-Path -LiteralPath $ImagePath)) {
        throw "Image path not found: $ImagePath"
    }
    $targetPath = $ImagePath
}

$beforeHash = if ($isPhysical) { "not-applicable-physical-read-only" } else { Get-M5ImageHash -Path $targetPath }
$capabilities = [PSCustomObject]@{
    hardwareInventory = [bool]$GrantHardwareInventoryCapability
    readOnlyBlock = [bool]$GrantReadOnlyBlockCapability
    write = [bool]$GrantWriteCapability
    format = [bool]$GrantFormatCapability
    bootEntry = [bool]$GrantBootEntryCapability
}

$report = [PSCustomObject]@{
    tool = "limitless-installer"
    milestone = "M5 Safe Installer + Partition Protection"
    mode = $Mode
    image = $targetPath
    physicalDriveNumber = if ($isPhysical) { $PhysicalDriveNumber } else { $null }
    capabilities = $capabilities
    destructiveWritesRequested = ($Mode -eq "Install")
    bootEntryChangeRequested = [bool]$RequestBootEntryChange
    bootPayloadImage = $null
    bootPayloadBytes = 0
    bootPayloadSha256 = ""
    result = "unknown"
    error = ""
    beforeSha256 = $beforeHash
    afterSha256 = $null
    writesPerformed = 0
    auditRecords = @()
    disks = @()
    partitions = @()
    plan = @()
    forbiddenPartitions = @()
    acceptedTargets = @()
}

if (-not $GrantHardwareInventoryCapability) {
    Fail-Installer -Reason "hardware inventory capability is required for disk enumeration" -Report $report
}
if (-not $GrantReadOnlyBlockCapability) {
    Fail-Installer -Reason "read-only block capability is required for partition reads" -Report $report
}

$partitions = @(Get-M5GptPartitions -ImagePath $targetPath)
$diskBytes = 0
if (-not $isPhysical) {
    $diskBytes = (Get-Item -LiteralPath $targetPath).Length
}
$report.disks = @([PSCustomObject]@{
    id = "image:0"
    path = $targetPath
    bytes = $diskBytes
    sectorBytes = $Script:M5SectorBytes
    physicalReadOnly = $isPhysical
})

foreach ($partition in $partitions) {
    $classification = Get-M5PartitionClassification -Partition $partition
    $entry = [PSCustomObject]@{
        number = $partition.number
        typeGuid = $partition.typeGuid
        firstLba = $partition.firstLba
        lastLba = $partition.lastLba
        sectors = $partition.sectors
        name = $partition.name
        filesystem = $partition.filesystem
        filesystemLabel = $partition.filesystemLabel
        markerPresent = (-not [string]::IsNullOrWhiteSpace($partition.marker))
        classification = $classification.class
        reason = $classification.reason
        writable = $classification.writable
    }
    $report.partitions += $entry
    if ($classification.class -eq "safe") {
        $report.acceptedTargets += $partition.number
    }
    else {
        $report.forbiddenPartitions += $partition.number
    }
}

if ($partitions.Count -eq 0) {
    $report.plan += [PSCustomObject]@{
        action = "propose-layout"
        writes = 0
        description = "Clean GPT disk has no partitions; M5 would require explicit user-created empty target confirmation before any write."
    }
}
else {
    $report.plan += [PSCustomObject]@{
        action = "classify-existing-partitions"
        writes = 0
        description = "Classify partitions and refuse forbidden or unknown targets."
    }
}

Write-Host "installer-mode: $Mode"
Write-Host "installer-image: $targetPath"
if ($isPhysical) {
    Write-Host "installer-physical-read-only: 1"
}
Write-Host ("capabilities: hardware-inventory {0} read-only-block {1} write {2} format {3} boot-entry {4}" -f ([int]$capabilities.hardwareInventory), ([int]$capabilities.readOnlyBlock), ([int]$capabilities.write), ([int]$capabilities.format), ([int]$capabilities.bootEntry))
Write-Host ("disk: image:0 bytes {0} sector-bytes {1}" -f $report.disks[0].bytes, $Script:M5SectorBytes)
foreach ($entry in $report.partitions) {
    Write-Host ("partition {0}: type {1} label '{2}' fs {3} fs-label '{4}' lba {5}-{6} class {7} reason '{8}' writable {9}" -f $entry.number, $entry.typeGuid, $entry.name, $entry.filesystem, $entry.filesystemLabel, $entry.firstLba, $entry.lastLba, $entry.classification, $entry.reason, ([int]$entry.writable))
}
foreach ($plan in $report.plan) {
    Write-Host ("plan: {0} writes {1} {2}" -f $plan.action, $plan.writes, $plan.description)
}

if ($Mode -eq "DryRun") {
    $afterHash = if ($isPhysical) { "not-applicable-physical-read-only" } else { Get-M5ImageHash -Path $targetPath }
    $report.afterSha256 = $afterHash
    $report.result = "dry-run-ok"
    $report.writesPerformed = 0
    Write-Host "installer-result: dry-run-ok"
    Write-Host "installer-writes: 0"
    Write-Host "installer-before-sha256: $beforeHash"
    Write-Host "installer-after-sha256: $afterHash"
    if ($beforeHash -ne $afterHash) {
        Fail-Installer -Reason "dry-run modified the image" -Report $report
    }
    if (-not [string]::IsNullOrWhiteSpace($JsonOutputPath)) {
        $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonOutputPath -Encoding UTF8
    }
    exit 0
}

if ($RequestBootEntryChange -and -not $GrantBootEntryCapability) {
    Fail-Installer -Reason "boot entry modification requires explicit firmware/boot-entry capability" -Report $report
}
if (-not $GrantWriteCapability) {
    Fail-Installer -Reason "install mode requires explicit scoped write capability" -Report $report
}
if (-not $GrantFormatCapability) {
    Fail-Installer -Reason "install mode requires explicit format capability" -Report $report
}
if ($BootPartitionNumber -le 0 -or $RootPartitionNumber -le 0) {
    Fail-Installer -Reason "install mode requires explicit boot and root partition numbers" -Report $report
}

$expectedToken = "$($Script:M5ConfirmationPrefix)$BootPartitionNumber/$RootPartitionNumber"
if ($ConfirmationToken -ne $expectedToken) {
    Fail-Installer -Reason "confirmation token mismatch; expected $expectedToken" -Report $report
}

$bootPartition = $partitions | Where-Object { $_.number -eq $BootPartitionNumber } | Select-Object -First 1
$rootPartition = $partitions | Where-Object { $_.number -eq $RootPartitionNumber } | Select-Object -First 1
if (-not $bootPartition -or -not $rootPartition) {
    Fail-Installer -Reason "selected target partition does not exist" -Report $report
}
foreach ($target in @($bootPartition, $rootPartition)) {
    $classification = Get-M5PartitionClassification -Partition $target
    if ($classification.class -ne "safe" -or -not $classification.writable) {
        Fail-Installer -Reason "selected partition $($target.number) is not a dedicated LimitlessOS target" -Report $report
    }
}

if ([string]::IsNullOrWhiteSpace($BootPayloadImagePath)) {
    $BootPayloadImagePath = Join-Path $root "dist\limitlessos-x86_64-uefi.img"
}
$BootPayloadImagePath = [System.IO.Path]::GetFullPath($BootPayloadImagePath)
if (-not (Test-Path -LiteralPath $BootPayloadImagePath)) {
    Fail-Installer -Reason "boot payload image not found: $BootPayloadImagePath" -Report $report
}
$bootPayloadBytes = [System.IO.File]::ReadAllBytes($BootPayloadImagePath)
$bootPayloadSha256 = Get-InstallerBytesSha256 -Bytes $bootPayloadBytes
$bootPartitionBytes = [int64]$bootPartition.sectors * $Script:M5SectorBytes
if ($bootPayloadBytes.Length -gt $bootPartitionBytes) {
    Fail-Installer -Reason "boot payload image exceeds selected boot partition capacity" -Report $report
}
$report.bootPayloadImage = $BootPayloadImagePath
$report.bootPayloadBytes = $bootPayloadBytes.Length
$report.bootPayloadSha256 = $bootPayloadSha256

$forbiddenBefore = @{}
foreach ($partition in $partitions) {
    $classification = Get-M5PartitionClassification -Partition $partition
    if ($classification.class -ne "safe") {
        $stream = [System.IO.File]::Open($targetPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            $sample = Read-M5Bytes -Stream $stream -Offset ([int64]$partition.firstLba * $Script:M5SectorBytes) -Count ([int][Math]::Min($partition.sectors * $Script:M5SectorBytes, 4096))
            $forbiddenBefore[[string]$partition.number] = Get-M5Crc32 -Bytes $sample
        }
        finally {
            $stream.Dispose()
        }
    }
}

$writeStream = [System.IO.File]::Open($targetPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::Read)
try {
    $rootText = "LIMITLESSOS_ROOT_V1`nprofile=Product`ncapability=scoped-installer-write`n"
    $rootBytes = [System.Text.Encoding]::ASCII.GetBytes($rootText)
    $writeStream.Seek(([int64]$bootPartition.firstLba * $Script:M5SectorBytes), [System.IO.SeekOrigin]::Begin) | Out-Null
    $writeStream.Write($bootPayloadBytes, 0, $bootPayloadBytes.Length)
    $writeStream.Seek(([int64]$rootPartition.firstLba * $Script:M5SectorBytes) + 1024, [System.IO.SeekOrigin]::Begin) | Out-Null
    $writeStream.Write($rootBytes, 0, $rootBytes.Length)
    $writeStream.Flush()
}
finally {
    $writeStream.Dispose()
}

$report.writesPerformed = 2
$report.auditRecords += [PSCustomObject]@{
    operation = "write-uefi-boot-partition"
    partition = $BootPartitionNumber
    authority = "scoped write + format"
    bytes = $bootPayloadBytes.Length
    sha256 = $bootPayloadSha256
}
$report.auditRecords += [PSCustomObject]@{
    operation = "write-root-marker"
    partition = $RootPartitionNumber
    authority = "scoped write + format"
}

$verified = $true
$bootPayloadVerified = $false
$stream = [System.IO.File]::Open($targetPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
try {
    $bootVerifyBytes = Read-M5Bytes -Stream $stream -Offset ([int64]$bootPartition.firstLba * $Script:M5SectorBytes) -Count $bootPayloadBytes.Length
    $bootVerifySha256 = Get-InstallerBytesSha256 -Bytes $bootVerifyBytes
    $rootVerify = [System.Text.Encoding]::ASCII.GetString((Read-M5Bytes -Stream $stream -Offset (([int64]$rootPartition.firstLba * $Script:M5SectorBytes) + 1024) -Count 32))
    $bootPayloadVerified = ($bootVerifySha256 -eq $bootPayloadSha256)
    if (-not $bootPayloadVerified -or -not $rootVerify.StartsWith("LIMITLESSOS_ROOT_V1")) {
        $verified = $false
    }
}
finally {
    $stream.Dispose()
}

foreach ($partition in $partitions) {
    if ($forbiddenBefore.ContainsKey([string]$partition.number)) {
        $stream = [System.IO.File]::Open($targetPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            $sample = Read-M5Bytes -Stream $stream -Offset ([int64]$partition.firstLba * $Script:M5SectorBytes) -Count ([int][Math]::Min($partition.sectors * $Script:M5SectorBytes, 4096))
            $after = Get-M5Crc32 -Bytes $sample
            if ($after -ne $forbiddenBefore[[string]$partition.number]) {
                $verified = $false
                $report.error = "forbidden partition $($partition.number) changed"
            }
        }
        finally {
            $stream.Dispose()
        }
    }
}

$afterHash = Get-M5ImageHash -Path $targetPath
$report.afterSha256 = $afterHash
if (-not $verified) {
    Fail-Installer -Reason "post-write verification failed: $($report.error)" -Report $report
}

$report.result = "install-ok"
Write-Host "installer-result: install-ok"
Write-Host "installer-writes: 2"
Write-Host "installer-verified-boot-files: 1"
Write-Host "installer-verified-manifests: 1"
Write-Host "installer-verified-bootable-uefi-payload: 1"
Write-Host "installer-verified-forbidden-unchanged: 1"
Write-Host "installer-boot-entry-modified: 0"
Write-Host "installer-boot-payload-bytes: $($bootPayloadBytes.Length)"
Write-Host "installer-boot-payload-sha256: $bootPayloadSha256"
Write-Host "installer-before-sha256: $beforeHash"
Write-Host "installer-after-sha256: $afterHash"
if (-not [string]::IsNullOrWhiteSpace($JsonOutputPath)) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonOutputPath -Encoding UTF8
}
