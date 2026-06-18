param(
    [string]$DynamicAppPath = "",
    [string]$DynamicInterpPath = "",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"
$distDir = Join-Path $root "dist"

if ([string]::IsNullOrWhiteSpace($DynamicAppPath)) {
    $DynamicAppPath = Join-Path $root "external\build\DYNLDLIMIT"
}
if ([string]::IsNullOrWhiteSpace($DynamicInterpPath)) {
    $DynamicInterpPath = Join-Path $root "external\build\LDLIMIT"
}

if (-not (Test-Path $DynamicAppPath)) {
    throw "M111 verifier: dynamic app artifact not found: $DynamicAppPath"
}
if (-not (Test-Path $DynamicInterpPath)) {
    throw "M111 verifier: dynamic interpreter artifact not found: $DynamicInterpPath"
}

$resolvedApp = (Resolve-Path $DynamicAppPath).Path
$resolvedInterp = (Resolve-Path $DynamicInterpPath).Path
$appItem = Get-Item $resolvedApp
$interpItem = Get-Item $resolvedInterp
$appSha256 = (Get-FileHash -Algorithm SHA256 -Path $resolvedApp).Hash.ToLowerInvariant()
$interpSha256 = (Get-FileHash -Algorithm SHA256 -Path $resolvedInterp).Hash.ToLowerInvariant()

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$manifestPath = Join-Path $buildDir "hardware-storage-staging-manifest.txt"
$manifest = @(
    "m111-hardware-storage-staging=1",
    "boot-linux-app=/APPS/DYNLDLIMIT",
    "boot-linux-app-source=$resolvedApp",
    "boot-linux-app-bytes=$($appItem.Length)",
    "boot-linux-app-sha256=$appSha256",
    "boot-linux-interp=/APPS/LDLIMIT",
    "boot-linux-interp-source=$resolvedInterp",
    "boot-linux-interp-bytes=$($interpItem.Length)",
    "boot-linux-interp-sha256=$interpSha256",
    "nvme-paths=/APPS/DYNLDLIMIT,/APPS/LDLIMIT",
    "uefi-image=$(Join-Path $distDir "limitlessos-x86_64-uefi.img")",
    "uefi-iso=$(Join-Path $distDir "limitlessos-x86_64.iso")"
)
Set-Content -Path $manifestPath -Value $manifest -Encoding Ascii

Write-Host "M111 hardware storage staging manifest: $manifestPath"
Write-Host "  /APPS/DYNLDLIMIT bytes $($appItem.Length) sha256 $appSha256"
Write-Host "  /APPS/LDLIMIT    bytes $($interpItem.Length) sha256 $interpSha256"

if (-not $SkipBuild.IsPresent) {
    & (Join-Path $root "tools\build.ps1") `
        -Architecture x86_64 `
        -BuildProfile Product `
        -BootLinuxAppPath $resolvedApp `
        -BootLinuxAppName DYNLDLIMIT `
        -BootLinuxInterpPath $resolvedInterp `
        -BootLinuxInterpName LDLIMIT
    if (-not $?) {
        throw "M111 verifier: staged Product build failed."
    }
}

$bootManifestPath = Join-Path $distDir "limitlessos-x86_64-uefi\BOOTMAN.TXT"
if (-not (Test-Path $bootManifestPath)) {
    throw "M111 verifier: BOOTMAN.TXT was not generated: $bootManifestPath"
}
$bootManifest = Get-Content $bootManifestPath
foreach ($expectedLine in @(
        "boot-linux-expected=1",
        "boot-linux-app=/APPS/DYNLDLIMIT",
        "boot-linux-app-bytes=$($appItem.Length)",
        "boot-linux-app-sha256=$appSha256",
        "boot-linux-interp=/APPS/LDLIMIT",
        "boot-linux-interp-bytes=$($interpItem.Length)",
        "boot-linux-interp-sha256=$interpSha256"
    )) {
    if (-not ($bootManifest -contains $expectedLine)) {
        throw "M111 verifier: BOOTMAN.TXT missing expected staging line: $expectedLine"
    }
}

& (Join-Path $root "tools\verify-qemu.ps1") `
    -Architecture x86_64 `
    -BootMedia uefi `
    -BuildProfile Product `
    -HardwareStorageStageGate `
    -ExtraAppPath $resolvedApp `
    -ExtraAppName DYNLDLIMIT `
    -ExtraApp2Path $resolvedInterp `
    -ExtraApp2Name LDLIMIT
if (-not $?) {
    throw "M111 verifier: hardware storage staging gate failed."
}
