param(
    [ValidateSet("virtio", "e1000e", "e1000")]
    [string]$NetworkDevice = "virtio"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $scriptDir "build.ps1"
$verifyQemu = Join-Path $scriptDir "verify-qemu.ps1"

$buildLines = @(& $build -Architecture x86_64 -BuildProfile Product 2>&1)
$buildExitCode = $LASTEXITCODE
$buildLines | ForEach-Object { $_ }
if ($buildExitCode -ne 0) {
    exit $buildExitCode
}

$outputLines = @(& $verifyQemu -Architecture x86_64 -BootMedia uefi -NetworkDevice $NetworkDevice -BuildProfile Product 2>&1)
$exitCode = $LASTEXITCODE
$outputLines | ForEach-Object { $_ }
if ($exitCode -ne 0) {
    exit $exitCode
}

$socketLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-socket ' } | Select-Object -First 1
if (-not $socketLine) {
    throw "M19 brokered socket verifier failed: no drs-socket telemetry was observed."
}

$requiredLabels = @(
    "drs-socket-api",
    "drs-socket-service",
    "drs-socket-cap-required",
    "drs-socket-cap-minted",
    "drs-socket-no-cap-denied",
    "drs-socket-wrong-owner-denied",
    "drs-socket-raw-denied",
    "drs-socket-listen-denied",
    "drs-socket-send-denied",
    "drs-socket-connect-attempt",
    "drs-socket-connect-granted",
    "drs-socket-recv-status"
)

foreach ($label in $requiredLabels) {
    if ($socketLine -notmatch ("(^| )" + [regex]::Escape($label) + " 1( |$)")) {
        throw "M19 brokered socket verifier failed: expected '$label 1' in drs-socket telemetry."
    }
}

if ($socketLine -notmatch ' fs-authority 0 storage-authority 0 ambient-authority 0') {
    throw "M19 brokered socket verifier failed: socket authority boundaries were not preserved."
}

Write-Host "M19 brokered socket API verifier passed."
