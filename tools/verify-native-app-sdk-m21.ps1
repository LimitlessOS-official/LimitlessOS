param(
    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $root "tools\build.ps1"
$verifyQemuScript = Join-Path $root "tools\verify-qemu.ps1"

Write-Host "Building x86_64 $BuildProfile media for M21 native app SDK verification"
& $buildScript -Architecture x86_64 -BuildProfile $BuildProfile
if (-not $?) {
    throw "M21 build failed."
}

Write-Host "Running x86_64 UEFI QEMU verification for M21 native app SDK"
& $verifyQemuScript -Architecture x86_64 -BootMedia uefi -BuildProfile $BuildProfile
if (-not $?) {
    throw "M21 QEMU verification failed."
}

Write-Host "M21 native app SDK verification passed."
