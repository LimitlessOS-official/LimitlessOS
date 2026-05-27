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

function ConvertTo-M5QemuArgumentString
{
    param([string[]]$Arguments)

    return (($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"' + ($_ -replace '"', '\"') + '"'
        }
        else {
            $_
        }
    }) -join " ")
}

function Get-M5QemuCommand
{
    $qemu = Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
    if ($qemu) {
        return $qemu.Source
    }

    $fallback = "C:\Program Files\qemu\qemu-system-x86_64.exe"
    if (Test-Path -LiteralPath $fallback) {
        return $fallback
    }

    Fail-M5Installer "QEMU is not installed or not on PATH."
}

function Get-M5QemuEdk2CodePath
{
    $candidates = @(
        (Join-Path $root "tools\ovmf\edk2-x86_64-code.fd"),
        "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    Fail-M5Installer "QEMU EDK2 x86_64 firmware was not found."
}

function Invoke-M5WrittenImageBootCheck
{
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$ImagePath
    )

    $qemu = Get-M5QemuCommand
    $firmwarePath = Get-M5QemuEdk2CodePath
    $debugPath = Join-Path $EvidenceDir "$Name.debug.log"
    $serialPath = Join-Path $EvidenceDir "$Name.serial.log"
    $stderrPath = Join-Path $EvidenceDir "$Name.stderr.txt"
    $outputPath = Join-Path $EvidenceDir "$Name.output.txt"
    Remove-Item -LiteralPath $debugPath, $serialPath, $stderrPath, $outputPath -Force -ErrorAction SilentlyContinue

    $arguments = @(
        "-display", "none",
        "-monitor", "none",
        "-no-reboot",
        "-no-shutdown",
        "-serial", "file:$serialPath",
        "-debugcon", "file:$debugPath",
        "-global", "isa-debugcon.iobase=0xe9",
        "-machine", "q35",
        "-drive", "if=pflash,format=raw,readonly=on,file=$firmwarePath",
        "-device", "uefi-vars-x64",
        "-drive", "if=none,id=installtarget,format=raw,snapshot=on,file=$ImagePath",
        "-device", "nvme,drive=installtarget,serial=LIMITLESSINSTALL,bootindex=1"
    )
    $process = Start-Process `
        -FilePath $qemu `
        -ArgumentList (ConvertTo-M5QemuArgumentString -Arguments $arguments) `
        -WorkingDirectory $root `
        -WindowStyle Hidden `
        -PassThru `
        -RedirectStandardError $stderrPath

    $booted = $false
    try {
        $deadline = (Get-Date).AddSeconds(240)
        while ((Get-Date) -lt $deadline) {
            $debugText = if (Test-Path -LiteralPath $debugPath) { Get-Content -LiteralPath $debugPath -Raw } else { "" }
            $serialText = if (Test-Path -LiteralPath $serialPath) { Get-Content -LiteralPath $serialPath -Raw } else { "" }
            $combinedText = "$debugText`n$serialText"
            $loaderMatched = $combinedText -match '\[uefi\] loader payload read KERNEL64\.BIN bytes [1-9][0-9]* .* match 1'
            $loginMatched = $combinedText -match '\[x64\] drs-login drs-login-screen 1'
            if ($loaderMatched -and $loginMatched) {
                $booted = $true
                break
            }
            Start-Sleep -Milliseconds 1000
        }
    }
    finally {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }
    }

    $combined = New-Object System.Collections.Generic.List[string]
    foreach ($path in @($debugPath, $serialPath, $stderrPath)) {
        if (Test-Path -LiteralPath $path) {
            foreach ($line in Get-Content -LiteralPath $path) {
                $combined.Add($line)
            }
        }
    }
    $combined | Set-Content -LiteralPath $outputPath -Encoding UTF8

    if (-not $booted) {
        Fail-M5Installer "$Name did not boot the written NVMe image to the login proof. See $outputPath"
    }

    return [PSCustomObject]@{
        name = $Name
        exitCode = 0
        outputPath = $outputPath
        lines = @($combined.ToArray())
    }
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
Assert-Output -Lines $record.lines -Pattern "installer-verified-bootable-uefi-payload: 1" -Message "install did not verify the bootable UEFI payload."
Assert-Output -Lines $record.lines -Pattern "installer-boot-payload-bytes: [1-9][0-9]*" -Message "install did not report boot payload bytes."
Assert-Output -Lines $record.lines -Pattern "installer-boot-payload-sha256: [0-9A-F]{64}" -Message "install did not report boot payload checksum."
Assert-Output -Lines $record.lines -Pattern "installer-verified-forbidden-unchanged: 1" -Message "install did not prove forbidden partitions unchanged."
Assert-Output -Lines $record.lines -Pattern "installer-boot-entry-modified: 0" -Message "install modified boot entries unexpectedly."
if ((Get-M5ImageHash -Path $installOk) -eq $installBefore) {
    Fail-M5Installer "valid install did not modify the dedicated LimitlessOS target."
}
$bootRecord = Invoke-M5WrittenImageBootCheck -Name "boot-installed-valid-limitless-target" -ImagePath $installOk
$records.Add($bootRecord)
Assert-Output -Lines $bootRecord.lines -Pattern "\[x64\] drs-login drs-login-screen 1" -Message "written install image did not reach the login screen."

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
    bootableWrittenImageVerified = $true
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
Write-Host "  bootable-written-image-verified 1"
Write-Host "  no-ambient-authority 1"
Write-Host "  evidence $summaryPath"
