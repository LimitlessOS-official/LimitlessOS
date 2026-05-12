param(
    [string]$FixtureDir = "",
    [string]$EvidenceDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "installer-common.ps1")

if ([string]::IsNullOrWhiteSpace($FixtureDir)) {
    $FixtureDir = Join-Path $root "dist\m5-installer-fixtures"
}
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $root "dist\m5-installer-verify"
}
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null

$generator = Join-Path $PSScriptRoot "generate-installer-fixtures.ps1"
$installer = Join-Path $PSScriptRoot "limitless-installer.ps1"
& $generator -OutputDir $FixtureDir
if (-not $?) {
    throw "M5 installer verification failed: fixture generation failed."
}

function Fail-M5Installer
{
    param([string]$Message)

    throw "M5 installer verification failed: $Message"
}

function Invoke-M5InstallerCommand
{
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [int]$ExpectedExitCode = 0
    )

    $stdoutPath = Join-Path $EvidenceDir "$Name.stdout.txt"
    $stderrPath = Join-Path $EvidenceDir "$Name.stderr.txt"
    $outputPath = Join-Path $EvidenceDir "$Name.output.txt"
    $process = Start-Process `
        -FilePath "powershell.exe" `
        -ArgumentList (@("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $installer) + $Arguments) `
        -WorkingDirectory $root `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath

    $combined = New-Object System.Collections.Generic.List[string]
    if (Test-Path -LiteralPath $stdoutPath) {
        foreach ($line in Get-Content -LiteralPath $stdoutPath) {
            $combined.Add($line)
        }
    }
    if (Test-Path -LiteralPath $stderrPath) {
        foreach ($line in Get-Content -LiteralPath $stderrPath) {
            $combined.Add($line)
        }
    }
    $combined | Set-Content -LiteralPath $outputPath -Encoding UTF8

    if ($process.ExitCode -ne $ExpectedExitCode) {
        Fail-M5Installer "$Name exited $($process.ExitCode), expected $ExpectedExitCode. See $outputPath"
    }

    return [PSCustomObject]@{
        name = $Name
        exitCode = $process.ExitCode
        outputPath = $outputPath
        lines = @($combined.ToArray())
    }
}

function Assert-Output
{
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$Message
    )

    if (-not ($Lines | Where-Object { $_ -match $Pattern })) {
        Fail-M5Installer $Message
    }
}

function Copy-Fixture
{
    param([string]$Name)

    $source = Join-Path $FixtureDir "$Name.img"
    $copy = Join-Path $EvidenceDir "$Name.work.img"
    Copy-Item -LiteralPath $source -Destination $copy -Force
    return $copy
}

function Get-RepoRelativePath
{
    param([string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $rootPath = [System.IO.Path]::GetFullPath($root).TrimEnd('\')
    if ($fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootPath.Length + 1)
    }

    return $fullPath
}

$records = New-Object System.Collections.Generic.List[object]

$dryRunCommon = @("-Mode", "DryRun", "-GrantHardwareInventoryCapability", "-GrantReadOnlyBlockCapability")

$clean = Copy-Fixture -Name "clean-unallocated-target"
$cleanBefore = Get-M5ImageHash -Path $clean
$record = Invoke-M5InstallerCommand -Name "dryrun-clean-unallocated" -Arguments (@("-ImagePath", $clean) + $dryRunCommon)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "installer-result: dry-run-ok" -Message "clean disk dry-run did not complete."
Assert-Output -Lines $record.lines -Pattern "plan: propose-layout writes 0" -Message "clean disk did not produce a zero-write layout proposal."
if ((Get-M5ImageHash -Path $clean) -ne $cleanBefore) {
    Fail-M5Installer "dry-run modified the clean fixture."
}

$windows = Copy-Fixture -Name "windows-like"
$windowsBefore = Get-M5ImageHash -Path $windows
$record = Invoke-M5InstallerCommand -Name "dryrun-windows-like" -Arguments (@("-ImagePath", $windows) + $dryRunCommon)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "Windows EFI System Partition|unknown ESP" -Message "Windows ESP was not forbidden."
Assert-Output -Lines $record.lines -Pattern "Microsoft Reserved partition" -Message "MSR partition was not forbidden."
Assert-Output -Lines $record.lines -Pattern "NTFS partition" -Message "NTFS partition was not forbidden."
Assert-Output -Lines $record.lines -Pattern "Windows Recovery partition" -Message "Recovery partition was not forbidden."
if ((Get-M5ImageHash -Path $windows) -ne $windowsBefore) {
    Fail-M5Installer "dry-run modified the Windows-like fixture."
}

$unknownFat = Copy-Fixture -Name "unknown-fat32"
$unknownFatBefore = Get-M5ImageHash -Path $unknownFat
$record = Invoke-M5InstallerCommand -Name "dryrun-unknown-fat32" -Arguments (@("-ImagePath", $unknownFat) + $dryRunCommon)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "unknown internal FAT32 partition" -Message "unknown FAT32 partition was not forbidden."
if ((Get-M5ImageHash -Path $unknownFat) -ne $unknownFatBefore) {
    Fail-M5Installer "dry-run modified the unknown FAT32 fixture."
}

$unknownGpt = Copy-Fixture -Name "unknown-gpt"
$unknownGptBefore = Get-M5ImageHash -Path $unknownGpt
$record = Invoke-M5InstallerCommand -Name "dryrun-unknown-gpt" -Arguments (@("-ImagePath", $unknownGpt) + $dryRunCommon)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "unknown GPT partition without LimitlessOS marker" -Message "unknown GPT partition was not refused."
if ((Get-M5ImageHash -Path $unknownGpt) -ne $unknownGptBefore) {
    Fail-M5Installer "dry-run modified the unknown GPT fixture."
}

$valid = Copy-Fixture -Name "valid-limitless-target"
$validDryBefore = Get-M5ImageHash -Path $valid
$record = Invoke-M5InstallerCommand -Name "dryrun-valid-limitless-target" -Arguments (@("-ImagePath", $valid) + $dryRunCommon)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "LIMITLESS-BOOT.*class safe" -Message "Limitless boot target was not accepted as safe."
Assert-Output -Lines $record.lines -Pattern "LIMITLESS-ROOT.*class safe" -Message "Limitless root target was not accepted as safe."
Assert-Output -Lines $record.lines -Pattern "Windows EFI System Partition|unknown ESP" -Message "valid-target fixture did not preserve forbidden Windows ESP classification."
if ((Get-M5ImageHash -Path $valid) -ne $validDryBefore) {
    Fail-M5Installer "dry-run modified the valid target fixture."
}

$missingWrite = Copy-Fixture -Name "valid-limitless-target"
$missingWriteBefore = Get-M5ImageHash -Path $missingWrite
$record = Invoke-M5InstallerCommand -Name "install-missing-write-capability" -ExpectedExitCode 2 -Arguments @(
    "-ImagePath", $missingWrite,
    "-Mode", "Install",
    "-BootPartitionNumber", "4",
    "-RootPartitionNumber", "5",
    "-ConfirmationToken", "INSTALL-LIMITLESSOS-M5:4/5",
    "-GrantHardwareInventoryCapability",
    "-GrantReadOnlyBlockCapability",
    "-GrantFormatCapability"
)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "requires explicit scoped write capability" -Message "missing write capability was not denied."
if ((Get-M5ImageHash -Path $missingWrite) -ne $missingWriteBefore) {
    Fail-M5Installer "missing-write denial modified the fixture."
}

$badConfirm = Copy-Fixture -Name "valid-limitless-target"
$badConfirmBefore = Get-M5ImageHash -Path $badConfirm
$record = Invoke-M5InstallerCommand -Name "install-bad-confirmation" -ExpectedExitCode 2 -Arguments @(
    "-ImagePath", $badConfirm,
    "-Mode", "Install",
    "-BootPartitionNumber", "4",
    "-RootPartitionNumber", "5",
    "-ConfirmationToken", "WRONG",
    "-GrantHardwareInventoryCapability",
    "-GrantReadOnlyBlockCapability",
    "-GrantWriteCapability",
    "-GrantFormatCapability"
)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "confirmation token mismatch" -Message "bad confirmation did not prevent writes."
if ((Get-M5ImageHash -Path $badConfirm) -ne $badConfirmBefore) {
    Fail-M5Installer "bad-confirmation denial modified the fixture."
}

$bootEntryDenied = Copy-Fixture -Name "valid-limitless-target"
$bootEntryBefore = Get-M5ImageHash -Path $bootEntryDenied
$record = Invoke-M5InstallerCommand -Name "install-boot-entry-denied" -ExpectedExitCode 2 -Arguments @(
    "-ImagePath", $bootEntryDenied,
    "-Mode", "Install",
    "-BootPartitionNumber", "4",
    "-RootPartitionNumber", "5",
    "-ConfirmationToken", "INSTALL-LIMITLESSOS-M5:4/5",
    "-GrantHardwareInventoryCapability",
    "-GrantReadOnlyBlockCapability",
    "-GrantWriteCapability",
    "-GrantFormatCapability",
    "-RequestBootEntryChange"
)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "boot entry modification requires explicit firmware/boot-entry capability" -Message "boot-entry request without authority was not denied."
if ((Get-M5ImageHash -Path $bootEntryDenied) -ne $bootEntryBefore) {
    Fail-M5Installer "boot-entry denial modified the fixture."
}

$wrongTarget = Copy-Fixture -Name "windows-like"
$wrongTargetBefore = Get-M5ImageHash -Path $wrongTarget
$record = Invoke-M5InstallerCommand -Name "install-wrong-target-denied" -ExpectedExitCode 2 -Arguments @(
    "-ImagePath", $wrongTarget,
    "-Mode", "Install",
    "-BootPartitionNumber", "1",
    "-RootPartitionNumber", "3",
    "-ConfirmationToken", "INSTALL-LIMITLESSOS-M5:1/3",
    "-GrantHardwareInventoryCapability",
    "-GrantReadOnlyBlockCapability",
    "-GrantWriteCapability",
    "-GrantFormatCapability"
)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "not a dedicated LimitlessOS target" -Message "wrong Windows target was not denied."
if ((Get-M5ImageHash -Path $wrongTarget) -ne $wrongTargetBefore) {
    Fail-M5Installer "wrong-target denial modified the fixture."
}

$installOk = Copy-Fixture -Name "valid-limitless-target"
$installBefore = Get-M5ImageHash -Path $installOk
$record = Invoke-M5InstallerCommand -Name "install-valid-limitless-target" -Arguments @(
    "-ImagePath", $installOk,
    "-Mode", "Install",
    "-BootPartitionNumber", "4",
    "-RootPartitionNumber", "5",
    "-ConfirmationToken", "INSTALL-LIMITLESSOS-M5:4/5",
    "-GrantHardwareInventoryCapability",
    "-GrantReadOnlyBlockCapability",
    "-GrantWriteCapability",
    "-GrantFormatCapability"
)
$records.Add($record)
Assert-Output -Lines $record.lines -Pattern "installer-result: install-ok" -Message "valid LimitlessOS target install did not complete."
Assert-Output -Lines $record.lines -Pattern "installer-verified-boot-files: 1" -Message "install did not verify boot files."
Assert-Output -Lines $record.lines -Pattern "installer-verified-manifests: 1" -Message "install did not verify manifests."
Assert-Output -Lines $record.lines -Pattern "installer-verified-forbidden-unchanged: 1" -Message "install did not prove forbidden partitions unchanged."
Assert-Output -Lines $record.lines -Pattern "installer-boot-entry-modified: 0" -Message "install modified boot entries unexpectedly."
if ((Get-M5ImageHash -Path $installOk) -eq $installBefore) {
    Fail-M5Installer "valid install did not modify the dedicated LimitlessOS target."
}

$summary = [PSCustomObject]@{
    milestone = "M5 Safe Installer + Partition Protection"
    generatedAt = (Get-Date).ToString("o")
    fixtureDir = Get-RepoRelativePath $FixtureDir
    evidenceDir = Get-RepoRelativePath $EvidenceDir
    dryRunNoWrites = $true
    forbiddenPartitionsDetected = $true
    unknownPartitionsRefused = $true
    dedicatedTargetAccepted = $true
    writeRequiresScopedCapability = $true
    bootEntryRequiresSeparateAuthority = $true
    failedConfirmationPreventsWrites = $true
    successfulInstallVerified = $true
    noAmbientAuthority = $true
    records = @($records | ForEach-Object {
        [PSCustomObject]@{
            name = $_.name
            exitCode = $_.exitCode
            output = Get-RepoRelativePath $_.outputPath
        }
    })
}
$summaryPath = Join-Path $EvidenceDir "m5-installer-verification.json"
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "M5 installer verification passed."
Write-Host "  dry-run-no-writes 1"
Write-Host "  forbidden-partitions-detected 1"
Write-Host "  unknown-partitions-refused 1"
Write-Host "  dedicated-limitless-target-accepted 1"
Write-Host "  scoped-write-capability-required 1"
Write-Host "  boot-entry-authority-required 1"
Write-Host "  failed-confirmation-prevents-writes 1"
Write-Host "  successful-install-verifies-manifests 1"
Write-Host "  no-ambient-authority 1"
Write-Host "  evidence $summaryPath"
