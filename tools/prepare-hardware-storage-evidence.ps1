param(
    [string]$DynamicAppPath = "",

    [string]$DynamicInterpPath = "",

    [string]$EvidenceDir = "",

    [switch]$SkipBuild,

    [switch]$SkipQemuGate,

    [switch]$SkipHandoffVerify
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $root "dist"
$buildDir = Join-Path $root "build"
$handoffMilestone = "M133"
$handoffStem = "m133"
$handoffIsoName = "limitlessos-x86_64-$handoffStem-handoff.iso"
$handoffUefiName = "limitlessos-x86_64-$handoffStem-handoff-uefi.img"

if ([string]::IsNullOrWhiteSpace($DynamicAppPath)) {
    $DynamicAppPath = Join-Path $root "external\build\DYNLDLIMIT"
}
if ([string]::IsNullOrWhiteSpace($DynamicInterpPath)) {
    $DynamicInterpPath = Join-Path $root "external\build\LDLIMIT"
}
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $EvidenceDir = Join-Path $distDir "$handoffStem-msi-hardware-handoff-$stamp"
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

Assert-FileExists -Path $DynamicAppPath -Message "$handoffMilestone handoff prep: dynamic app artifact not found: $DynamicAppPath"
Assert-FileExists -Path $DynamicInterpPath -Message "$handoffMilestone handoff prep: dynamic interpreter artifact not found: $DynamicInterpPath"

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
        throw "$handoffMilestone handoff prep: staged Product build failed."
    }
}

$isoPath = Join-Path $distDir "limitlessos-x86_64.iso"
$uefiImagePath = Join-Path $distDir "limitlessos-x86_64-uefi.img"
$bootManifestPath = Join-Path $distDir "limitlessos-x86_64-uefi\BOOTMAN.TXT"
$sizeMapPath = Join-Path $distDir "limitlessos-x86_64.size.txt"

Assert-FileExists -Path $isoPath -Message "$handoffMilestone handoff prep: staged ISO was not generated: $isoPath"
Assert-FileExists -Path $uefiImagePath -Message "$handoffMilestone handoff prep: staged UEFI image was not generated: $uefiImagePath"
Assert-FileExists -Path $bootManifestPath -Message "$handoffMilestone handoff prep: BOOTMAN.TXT was not generated: $bootManifestPath"
Assert-FileExists -Path $sizeMapPath -Message "$handoffMilestone handoff prep: size map was not generated: $sizeMapPath"

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
        throw "$handoffMilestone handoff prep: BOOTMAN.TXT missing expected staging line: $expectedLine"
    }
}

New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null

if (-not $SkipQemuGate.IsPresent) {
    $qemuOutputPath = Join-Path $EvidenceDir "qemu-storage-stage-gate.txt"
    $qemuOutput = & (Join-Path $root "tools\verify-hardware-storage-staging.ps1") -DynamicAppPath $resolvedApp -DynamicInterpPath $resolvedInterp -SkipBuild 2>&1
    $qemuOutput | Set-Content -Path $qemuOutputPath -Encoding Ascii
    if ($LASTEXITCODE -ne 0) {
        throw "$handoffMilestone handoff prep: staged QEMU storage gate failed. See $qemuOutputPath"
    }
}

$copiedIso = Copy-EvidenceFile -Path $isoPath -Name $handoffIsoName
$copiedUefi = Copy-EvidenceFile -Path $uefiImagePath -Name $handoffUefiName
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
    milestone = $handoffMilestone
    purpose = "MSI hardware handoff evidence bundle"
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
        analyzer = "tools\\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts"
        storage_target_classifier = "tools\\classify-m134-storage-target.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry"
        storage_verifier = "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts"
        boot_media_handoff_verifier = "tools\\verify-boot-media-linux-handoff.ps1"
        required_storage_stage = "storage-ready"
        required_boot_media_linux_source = 2
        required_gui_interaction_telemetry = 1
    }
}

$manifestJsonPath = Join-Path $EvidenceDir "hardware-storage-evidence-manifest.json"
$manifestTextPath = Join-Path $EvidenceDir "hardware-storage-evidence-manifest.txt"
$runbookPath = Join-Path $EvidenceDir "README-HARDWARE-STORAGE.txt"

$manifestObject | ConvertTo-Json -Depth 6 | Set-Content -Path $manifestJsonPath -Encoding Ascii
@(
    "LimitlessOS $handoffMilestone MSI hardware handoff evidence bundle",
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
LimitlessOS $handoffMilestone MSI Hardware Handoff Runbook

1. Write $handoffIsoName to a USB drive using your normal image writer.
2. Boot the laptop through the UEFI USB boot entry.
3. At the [x64] shell, run:

   hwval
   linux /APPS/DYNLDLIMIT

4. Capture the full transcript to a text file named msi-hwval-storage.txt.
5. Back on Windows/PowerShell, verify this bundle and analyze the capture from the repository root:

   .\tools\classify-m134-storage-target.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <m134-target-output-dir> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry

   .\tools\verify-msi-hardware-handoff.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry

   .\tools\analyze-msi-hardware-capture.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <analysis-output-dir> -RequireStagedDynamicArtifacts

Use the M134 classifier output first. If it reports target-kind `storage`, implement the reported M134 storage stage. If it reports `display-input` or `dynamic-handoff`, do not guess at NVMe; follow that roadmap target instead.

Pass means the combined analyzer reports:

   msi-hardware-analysis: msi-hardware-ready
   pass: True
   storage-stage: storage-ready
   display/input-stage: display-input-ready

The same capture must include the M152 GUI interaction line from hwval:

   drs-gui ... drs-gui-right-click 1 ... drs-gui-context-action 1 ... drs-gui-scroll ...

If storage is unavailable on the laptop, linux /APPS/DYNLDLIMIT should still prefer the UEFI boot-media staged source when this bundle was written correctly. Capture that command output too. Expected handoff signal:

   linux: using UEFI boot-media staged file
   drs-realbin ... source 2 ... boot-media-read 1

If the command still prints NVMe FAT unavailable before source 2 telemetry, the first target is boot-media staging/handoff rather than the dynamic linker.

The lower-level storage-only verifier remains available when you only need the storage stage:

   .\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -RequireStagedDynamicArtifacts

Pass means the storage verifier reports:

   hardware-storage-evidence: verified
   capture pass: True
   capture stage: storage-ready

Before creating or using a bundle, the host-side boot-media handoff verifier can confirm source-2 shell selection in QEMU:

   .\tools\verify-boot-media-linux-handoff.ps1

Do not run installer writes, formatting, or NVRAM boot-entry actions during this evidence pass.
"@ | Set-Content -Path $runbookPath -Encoding Ascii

if (-not $SkipHandoffVerify.IsPresent) {
    $handoffOutputPath = Join-Path $EvidenceDir "msi-handoff-verification.txt"
    $handoffOutputDir = Join-Path $EvidenceDir "msi-handoff-verification"
    & (Join-Path $root "tools\verify-msi-hardware-handoff.ps1") `
        -EvidenceDir $EvidenceDir `
        -OutputDir $handoffOutputDir `
        -RequireStagedDynamicArtifacts `
        -RequireGuiInteractionTelemetry
    if ($LASTEXITCODE -ne 0) {
        throw "$handoffMilestone handoff prep: MSI hardware handoff verifier failed. See $handoffOutputPath"
    }
    $storageSummaryPath = Join-Path $handoffOutputDir "storage-evidence\hardware-storage-evidence-verification.txt"
    $handoffSummaryPath = Join-Path $handoffOutputDir "msi-hardware-handoff-verification.txt"
    Assert-FileExists -Path $storageSummaryPath -Message "$handoffMilestone handoff prep: storage handoff verification summary missing: $storageSummaryPath"
    Assert-FileExists -Path $handoffSummaryPath -Message "$handoffMilestone handoff prep: MSI handoff verification summary missing: $handoffSummaryPath"
    @(
        "$handoffMilestone MSI hardware handoff self-verification",
        "",
        "storage evidence:",
        (Get-Content -Path $storageSummaryPath),
        "",
        "msi handoff:",
        (Get-Content -Path $handoffSummaryPath)
    ) | Set-Content -Path $handoffOutputPath -Encoding Ascii
}

Write-Host "$handoffMilestone MSI hardware handoff evidence bundle: $EvidenceDir"
Write-Host "  iso sha256       : $($manifestObject.iso.sha256)"
Write-Host "  uefi image sha256: $($manifestObject.uefi_image.sha256)"
Write-Host "  app sha256       : $appSha256"
Write-Host "  interp sha256    : $interpSha256"
Write-Host "  bios reserve     : $($manifestObject.reserves.bios_sectors) sectors"
Write-Host "  uefi reserve     : $($manifestObject.reserves.uefi_bytes) bytes"
