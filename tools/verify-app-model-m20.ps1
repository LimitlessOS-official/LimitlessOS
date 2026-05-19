param(
    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $root "tools\build.ps1"
$verifyQemuScript = Join-Path $root "tools\verify-qemu.ps1"

Write-Host "Building x86_64 $BuildProfile media for M20/M21 app-model verification"
& $buildScript -Architecture x86_64 -BuildProfile $BuildProfile
if (-not $?) {
    throw "M20/M21 build failed."
}

Write-Host "Running x86_64 UEFI QEMU verification for the native app-model path"
& $verifyQemuScript -Architecture x86_64 -BootMedia uefi -BuildProfile $BuildProfile
if (-not $?) {
    throw "M20/M21 QEMU verification failed."
}

Write-Host "Native app-model verification passed. Current builds emit the M21 SDK checkpoint."
