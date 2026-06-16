param(
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"
$stageDir = Join-Path $root ".codex-stage\boot-media-linux-handoff"
$appPath = Join-Path $stageDir "dynldlimit-invalid-elf.bin"
$interpPath = Join-Path $stageDir "ldlimit-invalid-elf.bin"
$verifyQemuPath = Join-Path $root "tools\verify-qemu.ps1"
$buildPath = Join-Path $root "tools\build.ps1"
$budgetPath = Join-Path $root "dist\limitlessos-x86_64.size.txt"
$commandLine = "linux /APPS/DYNLDLIMIT"

function Assert-FileExists
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not (Test-Path $Path)) {
        throw $Message
    }
}

function Assert-Contains
{
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if ($Text -notmatch $Pattern) {
        throw $Message
    }
}

function Get-BootReserveSectors
{
    param([Parameter(Mandatory = $true)][string]$BudgetPath)

    $budget = Get-Content -Path $BudgetPath -Raw
    if ($budget -match 'bios-sector-reserve=([0-9]+)') {
        return [int]$Matches[1]
    }
    if ($budget -match 'bios-reserve-sectors=([0-9]+)') {
        return [int]$Matches[1]
    }
    if ($budget -match 'reserve-sectors=([0-9]+)') {
        return [int]$Matches[1]
    }
    throw "Boot-media Linux handoff verifier: BIOS reserve was not found in the x86_64 size report."
}

function Get-UefiReserveBytes
{
    param([Parameter(Mandatory = $true)][string]$BudgetPath)

    $budget = Get-Content -Path $BudgetPath -Raw
    if ($budget -match 'uefi-kernel-byte-reserve=([0-9]+)') {
        return [int]$Matches[1]
    }
    if ($budget -match 'uefi-reserve-bytes=([0-9]+)') {
        return [int]$Matches[1]
    }
    if ($budget -match 'kernel-byte-reserve=([0-9]+)') {
        return [int]$Matches[1]
    }
    if ($budget -match 'uefi-kernel-reserve=([0-9]+)') {
        return [int]$Matches[1]
    }
    return 0
}

New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

# Deliberately invalid ELF-shaped probe payloads. They prove UEFI FAT staging,
# boot-info handoff, shell fallback, and source-2 reads without pretending
# the real dynamic-loader artifacts are present.
[System.IO.File]::WriteAllBytes($appPath, [byte[]](0x7F, 0x45, 0x4C, 0x46, 0x44, 0x59, 0x4E))
[System.IO.File]::WriteAllBytes($interpPath, [byte[]](0x7F, 0x45, 0x4C, 0x46, 0x49, 0x4E, 0x54))

if (-not $SkipBuild) {
    & $buildPath `
        -Architecture x86_64 `
        -BuildProfile Product `
        -BootLinuxAppPath $appPath `
        -BootLinuxAppName DYNLDLIMIT `
        -BootLinuxInterpPath $interpPath `
        -BootLinuxInterpName LDLIMIT
    if ($LASTEXITCODE -ne 0) {
        throw "Boot-media Linux handoff verifier: build failed."
    }
}

Assert-FileExists -Path $budgetPath -Message "Boot-media Linux handoff verifier: x86_64 size report was not generated."
$biosReserve = Get-BootReserveSectors -BudgetPath $budgetPath
$uefiReserve = Get-UefiReserveBytes -BudgetPath $budgetPath
if ($biosReserve -lt 101) {
    throw "Boot-media Linux handoff verifier: BIOS reserve dropped below 101 sectors ($biosReserve)."
}

& $verifyQemuPath `
    -Architecture x86_64 `
    -BootMedia uefi `
    -BuildProfile Product `
    -RealBinaryGate `
    -ExtraShellLine $commandLine
if ($LASTEXITCODE -ne 0) {
    throw "Boot-media Linux handoff verifier: QEMU run failed."
}

$logPath = Join-Path $root "build\qemu-x86_64-uefi-debug.log"
Assert-FileExists -Path $logPath -Message "Boot-media Linux handoff verifier: QEMU log was not captured."
$logText = Get-Content -Path $logPath -Raw

Assert-Contains `
    -Text $logText `
    -Pattern '\[uefi\] boot linux stage DYNLDLIMIT attempted 1 loaded 1 bytes 7 pages 1 base 0x(?!0000000000000000)[0-9A-F]+ copied 1 token 0x[0-9A-F]+ status 0x0000000000000000' `
    -Message "Boot-media Linux handoff verifier: staged DYNLDLIMIT UEFI loader proof was not observed."
Assert-Contains `
    -Text $logText `
    -Pattern '\[uefi\] boot linux stage LDLIMIT attempted 1 loaded 1 bytes 7 pages 1 base 0x(?!0000000000000000)[0-9A-F]+ copied 1 token 0x[0-9A-F]+ status 0x0000000000000000' `
    -Message "Boot-media Linux handoff verifier: staged LDLIMIT UEFI loader proof was not observed."
Assert-Contains `
    -Text $logText `
    -Pattern 'linux: using UEFI boot-media staged file' `
    -Message "Boot-media Linux handoff verifier: shell did not select the UEFI boot-media fallback."
Assert-Contains `
    -Text $logText `
    -Pattern 'drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage elf' `
    -Message "Boot-media Linux handoff verifier: source-2 ELF failure proof was not observed."
Assert-Contains `
    -Text $logText `
    -Pattern 'drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage elf .* boot-media-read-error 0 boot-media-read-bytes 7 boot-media-read-capacity 4194304' `
    -Message "Boot-media Linux handoff verifier: boot-media read telemetry was not observed."

$telemetry = @(
    (Get-Content -Path $logPath) |
        Where-Object { ($_ -match 'boot linux stage') -or ($_ -match 'linux: using UEFI boot-media staged file') -or ($_ -match 'drs-realbin') }
)

Write-Host "Boot-media Linux handoff verifier passed."
Write-Host "BIOS reserve sectors: $biosReserve"
if ($uefiReserve -ne 0) {
    Write-Host "UEFI reserve bytes: $uefiReserve"
}
Write-Host "Command: $commandLine"
Write-Host "Telemetry:"
foreach ($line in $telemetry) {
    Write-Host "  $line"
}
