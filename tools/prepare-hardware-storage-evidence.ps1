param(
    [string]$DynamicAppPath = "",

    [string]$DynamicInterpPath = "",

    [string]$EvidenceDir = "",

    [switch]$SkipBuild,

    [switch]$SkipQemuGate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $root "dist"
$buildDir = Join-Path $root "build"

if ([string]::IsNullOrWhiteSpace($DynamicAppPath)) {
    $DynamicAppPath = Join-Path $root "external\build\DYNLDLIMIT"
}
if ([string]::IsNullOrWhiteSpace($DynamicInterpPath)) {
    $DynamicInterpPath = Join-Path $root "external\build\LDLIMIT"
}
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $EvidenceDir = Join-Path $distDir "m113-hardware-storage-$stamp"
}

function Assert-FileExists
{
    param(
        [string]$Path,
        [string]$Message
    )

    if (-not (Test-Path $Path)) {
        throw $Message
    }
}

function Get-Sha256
{
    param([string]$Path)

    return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

function Copy-EvidenceFile
{
    param(
        [string]$Path,
        [string]$Name
    )

    $destination = Join-Path $EvidenceDir $Name
    Copy-Item -Force -Path $Path -Destination $destination
    return $destination
}

Assert-FileExists -Path $DynamicAppPath -Message "M113 evidence prep: dynamic app artifact not found: $DynamicAppPath"
Assert-FileExists -Path $DynamicInterpPath -Message "M113 evidence prep: dynamic interpreter artifact not found: $DynamicInterpPath"

$resolvedApp = (Resolve-Path $DynamicAppPath).Path
$resolvedInterp = (Resolve-Path $DynamicInterpPath).Path
$appItem = Get-Item $resolvedApp
$interpItem = Get-Item $resolvedInterp
$appSha256 = Get-Sha256 -Path $resolvedApp
$interpSha256 = Get-Sha256 -Path $resolvedInterp

if (-not $SkipBuild.IsPresent) {
    & (Join-Path $root "tools\build.ps1") `
        -Architecture x86_64 `
        -BuildProfile Product `
        -BootLinuxAppPath $resolvedApp `
        -BootLinuxAppName DYNLDLIMIT `
        -BootLinuxInterpPath $resolvedInterp `
        -BootLinuxInterpName LDLIMIT
    if (-not $?) {
        throw "M113 evidence prep: staged Product build failed."
    }
}

$isoPath = Join-Path $distDir "limitlessos-x86_64.iso"
$uefiImagePath = Join-Path $distDir "limitlessos-x86_64-uefi.img"
$bootManifestPath = Join-Path $distDir "limitlessos-x86_64-uefi\BOOTMAN.TXT"
$sizeMapPath = Join-Path $distDir "limitlessos-x86_64.size.txt"

Assert-FileExists -Path $isoPath -Message "M113 evidence prep: staged ISO was not generated: $isoPath"
Assert-FileExists -Path $uefiImagePath -Message "M113 evidence prep: staged UEFI image was not generated: $uefiImagePath"
Assert-FileExists -Path $bootManifestPath -Message "M113 evidence prep: BOOTMAN.TXT was not generated: $bootManifestPath"
Assert-FileExists -Path $sizeMapPath -Message "M113 evidence prep: size map was not generated: $sizeMapPath"

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
        throw "M113 evidence prep: BOOTMAN.TXT missing expected staging line: $expectedLine"
    }
}

New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null

if (-not $SkipQemuGate.IsPresent) {
    $qemuOutputPath = Join-Path $EvidenceDir "qemu-storage-stage-gate.txt"
    $qemuOutput = & (Join-Path $root "tools\verify-hardware-storage-staging.ps1") -DynamicAppPath $resolvedApp -DynamicInterpPath $resolvedInterp -SkipBuild 2>&1
    $qemuOutput | Set-Content -Path $qemuOutputPath -Encoding Ascii
    if ($LASTEXITCODE -ne 0) {
        throw "M113 evidence prep: staged QEMU storage gate failed. See $qemuOutputPath"
    }
}

$copiedIso = Copy-EvidenceFile -Path $isoPath -Name "limitlessos-x86_64-m113-staged.iso"
$copiedUefi = Copy-EvidenceFile -Path $uefiImagePath -Name "limitlessos-x86_64-m113-staged-uefi.img"
$copiedManifest = Copy-EvidenceFile -Path $bootManifestPath -Name "BOOTMAN.TXT"
$copiedSizeMap = Copy-EvidenceFile -Path $sizeMapPath -Name "limitlessos-x86_64.size.txt"
$copiedApp = Copy-EvidenceFile -Path $resolvedApp -Name "DYNLDLIMIT"
$copiedInterp = Copy-EvidenceFile -Path $resolvedInterp -Name "LDLIMIT"

$isoItem = Get-Item $copiedIso
$uefiItem = Get-Item $copiedUefi
$sizeMap = @{}
foreach ($line in (Get-Content $sizeMapPath)) {
    if ($line -match '^([^=]+)=(.+)$') {
        $sizeMap[$Matches[1]] = $Matches[2]
    }
}

$manifestObject = [PSCustomObject]@{
    milestone = "M113"
    purpose = "physical hardware storage evidence bundle"
    generated_utc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    git_commit = (& git -C $root rev-parse --short HEAD)
    iso = [PSCustomObject]@{
        path = (Split-Path -Leaf $copiedIso)
        bytes = $isoItem.Length
        sha256 = Get-Sha256 -Path $copiedIso
    }
    uefi_image = [PSCustomObject]@{
        path = (Split-Path -Leaf $copiedUefi)
        bytes = $uefiItem.Length
        sha256 = Get-Sha256 -Path $copiedUefi
    }
    dynamic_app = [PSCustomObject]@{
        path = "/APPS/DYNLDLIMIT"
        evidence_file = (Split-Path -Leaf $copiedApp)
        source = $resolvedApp
        bytes = $appItem.Length
        sha256 = $appSha256
    }
    dynamic_interpreter = [PSCustomObject]@{
        path = "/APPS/LDLIMIT"
        evidence_file = (Split-Path -Leaf $copiedInterp)
        source = $resolvedInterp
        bytes = $interpItem.Length
        sha256 = $interpSha256
    }
    reserves = [PSCustomObject]@{
        bios_sectors = if ($sizeMap.ContainsKey("bios-sector-reserve")) { [uint32]$sizeMap["bios-sector-reserve"] } else { 0 }
        uefi_bytes = if ($sizeMap.ContainsKey("uefi-kernel-byte-reserve")) { [uint32]$sizeMap["uefi-kernel-byte-reserve"] } else { 0 }
    }
    expected_hwval = [PSCustomObject]@{
        command = "hwval"
        required_line = "drs-nvme-triage"
        analyzer = "tools\\analyze-hardware-storage-capture.ps1 -RequireStagedDynamicArtifacts"
        verifier = "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts"
        required_pass_stage = "storage-ready"
    }
}

$manifestJsonPath = Join-Path $EvidenceDir "hardware-storage-evidence-manifest.json"
$manifestTextPath = Join-Path $EvidenceDir "hardware-storage-evidence-manifest.txt"
$runbookPath = Join-Path $EvidenceDir "README-HARDWARE-STORAGE.txt"

$manifestObject | ConvertTo-Json -Depth 6 | Set-Content -Path $manifestJsonPath -Encoding Ascii
@(
    "LimitlessOS M113 hardware storage evidence bundle",
    "git-commit=$($manifestObject.git_commit)",
    "iso=$($manifestObject.iso.path)",
    "iso-bytes=$($manifestObject.iso.bytes)",
    "iso-sha256=$($manifestObject.iso.sha256)",
    "uefi-image=$($manifestObject.uefi_image.path)",
    "uefi-image-bytes=$($manifestObject.uefi_image.bytes)",
    "uefi-image-sha256=$($manifestObject.uefi_image.sha256)",
    "dynamic-app=/APPS/DYNLDLIMIT",
    "dynamic-app-bytes=$($manifestObject.dynamic_app.bytes)",
    "dynamic-app-sha256=$($manifestObject.dynamic_app.sha256)",
    "dynamic-interpreter=/APPS/LDLIMIT",
    "dynamic-interpreter-bytes=$($manifestObject.dynamic_interpreter.bytes)",
    "dynamic-interpreter-sha256=$($manifestObject.dynamic_interpreter.sha256)",
    "bios-sector-reserve=$($manifestObject.reserves.bios_sectors)",
    "uefi-byte-reserve=$($manifestObject.reserves.uefi_bytes)"
) | Set-Content -Path $manifestTextPath -Encoding Ascii

@"
LimitlessOS M113 Hardware Storage Runbook

1. Write limitlessos-x86_64-m113-staged.iso to a USB drive using your normal image writer.
2. Boot the laptop through the UEFI USB boot entry.
3. At the [x64] shell, run:

   hwval

4. Capture the full hwval transcript to a text file named msi-hwval-storage.txt.
5. Back on Windows/PowerShell, verify this bundle and analyze the capture from the repository root:

   .\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -RequireStagedDynamicArtifacts

Pass means the verifier reports:

   hardware-storage-evidence: verified
   capture pass: True
   capture stage: storage-ready

If it fails, use the reported stage as the next kernel/driver target. Do not run installer writes, formatting, or NVRAM boot-entry actions during this evidence pass.
"@ | Set-Content -Path $runbookPath -Encoding Ascii

Write-Host "M113 hardware storage evidence bundle: $EvidenceDir"
Write-Host "  iso sha256       : $($manifestObject.iso.sha256)"
Write-Host "  uefi image sha256: $($manifestObject.uefi_image.sha256)"
Write-Host "  app sha256       : $appSha256"
Write-Host "  interp sha256    : $interpSha256"
Write-Host "  bios reserve     : $($manifestObject.reserves.bios_sectors) sectors"
Write-Host "  uefi reserve     : $($manifestObject.reserves.uefi_bytes) bytes"
