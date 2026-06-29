param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDir,

    [string]$CapturePath = "",

    [string]$OutputDir = "",

    [switch]$RequireStagedDynamicArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

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

function Get-ManifestValue
{
    param(
        [object]$Manifest,
        [string]$ObjectName,
        [string]$PropertyName,
        [string]$Default = ""
    )

    if ($null -eq $Manifest) {
        return $Default
    }
    $objectProperty = $Manifest.PSObject.Properties[$ObjectName]
    if ($null -eq $objectProperty) {
        return $Default
    }
    $valueProperty = $objectProperty.Value.PSObject.Properties[$PropertyName]
    if ($null -eq $valueProperty) {
        return $Default
    }
    return [string]$valueProperty.Value
}

function Assert-ManifestFile
{
    param(
        [object]$Manifest,
        [string]$ObjectName,
        [string]$PathProperty,
        [string]$BytesProperty,
        [string]$ShaProperty,
        [string]$Label
    )

    $fileName = Get-ManifestValue -Manifest $Manifest -ObjectName $ObjectName -PropertyName $PathProperty
    if ([string]::IsNullOrWhiteSpace($fileName)) {
        throw "Hardware storage evidence verifier: $Label missing manifest path property $ObjectName.$PathProperty."
    }

    $path = Join-Path $resolvedEvidenceDir $fileName
    Assert-FileExists -Path $path -Message "Hardware storage evidence verifier: $Label missing from evidence bundle: $path"

    $item = Get-Item $path
    $expectedBytes = [uint64](Get-ManifestValue -Manifest $Manifest -ObjectName $ObjectName -PropertyName $BytesProperty)
    $expectedSha = (Get-ManifestValue -Manifest $Manifest -ObjectName $ObjectName -PropertyName $ShaProperty).ToLowerInvariant()
    $actualSha = Get-Sha256 -Path $path

    if ([uint64]$item.Length -ne $expectedBytes) {
        throw "Hardware storage evidence verifier: $Label byte mismatch. Expected $expectedBytes, observed $($item.Length)."
    }
    if ($actualSha -ne $expectedSha) {
        throw "Hardware storage evidence verifier: $Label SHA-256 mismatch. Expected $expectedSha, observed $actualSha."
    }

    return [PSCustomObject]@{
        path = $path
        bytes = [uint64]$item.Length
        sha256 = $actualSha
    }
}

function Get-SizeMapValue
{
    param(
        [hashtable]$SizeMap,
        [string]$Name,
        [uint64]$Default = 0
    )

    if ($SizeMap.ContainsKey($Name)) {
        return [uint64]$SizeMap[$Name]
    }
    return $Default
}

Assert-FileExists -Path $EvidenceDir -Message "Hardware storage evidence verifier: evidence directory not found: $EvidenceDir"
$resolvedEvidenceDir = (Resolve-Path $EvidenceDir).Path

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $resolvedEvidenceDir "verification"
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$manifestPath = Join-Path $resolvedEvidenceDir "hardware-storage-evidence-manifest.json"
$manifestTextPath = Join-Path $resolvedEvidenceDir "hardware-storage-evidence-manifest.txt"
$bootManifestPath = Join-Path $resolvedEvidenceDir "BOOTMAN.TXT"
$sizeMapPath = Join-Path $resolvedEvidenceDir "limitlessos-x86_64.size.txt"
$runbookPath = Join-Path $resolvedEvidenceDir "README-HARDWARE-STORAGE.txt"

Assert-FileExists -Path $manifestPath -Message "Hardware storage evidence verifier: JSON manifest missing: $manifestPath"
Assert-FileExists -Path $manifestTextPath -Message "Hardware storage evidence verifier: text manifest missing: $manifestTextPath"
Assert-FileExists -Path $bootManifestPath -Message "Hardware storage evidence verifier: BOOTMAN.TXT missing: $bootManifestPath"
Assert-FileExists -Path $sizeMapPath -Message "Hardware storage evidence verifier: size map missing: $sizeMapPath"
Assert-FileExists -Path $runbookPath -Message "Hardware storage evidence verifier: runbook missing: $runbookPath"

$manifest = Get-Content -Raw -Path $manifestPath | ConvertFrom-Json
$isoProof = Assert-ManifestFile -Manifest $manifest -ObjectName "iso" -PathProperty "path" -BytesProperty "bytes" -ShaProperty "sha256" -Label "ISO"
$uefiProof = Assert-ManifestFile -Manifest $manifest -ObjectName "uefi_image" -PathProperty "path" -BytesProperty "bytes" -ShaProperty "sha256" -Label "UEFI image"
$appProof = Assert-ManifestFile -Manifest $manifest -ObjectName "dynamic_app" -PathProperty "evidence_file" -BytesProperty "bytes" -ShaProperty "sha256" -Label "DYNLDLIMIT"
$interpProof = Assert-ManifestFile -Manifest $manifest -ObjectName "dynamic_interpreter" -PathProperty "evidence_file" -BytesProperty "bytes" -ShaProperty "sha256" -Label "LDLIMIT"

$bootManifest = Get-Content -Path $bootManifestPath
$appBytes = [uint64](Get-ManifestValue -Manifest $manifest -ObjectName "dynamic_app" -PropertyName "bytes")
$interpBytes = [uint64](Get-ManifestValue -Manifest $manifest -ObjectName "dynamic_interpreter" -PropertyName "bytes")
$appSha = Get-ManifestValue -Manifest $manifest -ObjectName "dynamic_app" -PropertyName "sha256"
$interpSha = Get-ManifestValue -Manifest $manifest -ObjectName "dynamic_interpreter" -PropertyName "sha256"

foreach ($expectedLine in @(
        "boot-linux-expected=1",
        "boot-linux-app=/APPS/DYNLDLIMIT",
        "boot-linux-app-bytes=$appBytes",
        "boot-linux-app-sha256=$appSha",
        "boot-linux-interp=/APPS/LDLIMIT",
        "boot-linux-interp-bytes=$interpBytes",
        "boot-linux-interp-sha256=$interpSha"
    )) {
    if (-not ($bootManifest -contains $expectedLine)) {
        throw "Hardware storage evidence verifier: BOOTMAN.TXT missing expected line: $expectedLine"
    }
}

$sizeMap = @{}
foreach ($line in (Get-Content -Path $sizeMapPath)) {
    if ($line -match '^([^=]+)=(.+)$') {
        $sizeMap[$Matches[1]] = $Matches[2]
    }
}

$biosReserve = Get-SizeMapValue -SizeMap $sizeMap -Name "bios-sector-reserve"
$uefiReserve = Get-SizeMapValue -SizeMap $sizeMap -Name "uefi-kernel-byte-reserve"
$manifestBiosReserve = [uint64](Get-ManifestValue -Manifest $manifest -ObjectName "reserves" -PropertyName "bios_sectors" -Default "0")
$manifestUefiReserve = [uint64](Get-ManifestValue -Manifest $manifest -ObjectName "reserves" -PropertyName "uefi_bytes" -Default "0")

if ($biosReserve -ne $manifestBiosReserve) {
    throw "Hardware storage evidence verifier: BIOS reserve mismatch. Manifest $manifestBiosReserve, size map $biosReserve."
}
if ($uefiReserve -ne $manifestUefiReserve) {
    throw "Hardware storage evidence verifier: UEFI reserve mismatch. Manifest $manifestUefiReserve, size map $uefiReserve."
}

$captureAnalysis = $null
$captureStage = ""
$capturePass = $false
$captureOutputDir = ""
$analysisExitCode = 0
if (-not [string]::IsNullOrWhiteSpace($CapturePath)) {
    Assert-FileExists -Path $CapturePath -Message "Hardware storage evidence verifier: capture file not found: $CapturePath"
    $captureOutputDir = Join-Path $OutputDir "capture-analysis"
    $analyzerArgs = @{
        InputPath = (Resolve-Path $CapturePath).Path
        OutputDir = $captureOutputDir
        EvidenceManifestPath = $manifestPath
    }
    if ($RequireStagedDynamicArtifacts.IsPresent) {
        $analyzerArgs["RequireStagedDynamicArtifacts"] = $true
    }

    $global:LASTEXITCODE = 0
    & (Join-Path $root "tools\analyze-hardware-storage-capture.ps1") @analyzerArgs
    $analysisExitCode = $LASTEXITCODE
    if (($analysisExitCode -ne 0) -and ($analysisExitCode -ne 2)) {
        throw "Hardware storage evidence verifier: analyzer failed unexpectedly with exit code $analysisExitCode."
    }
    $captureAnalysisPath = Join-Path $captureOutputDir "hardware-storage-analysis.json"
    Assert-FileExists -Path $captureAnalysisPath -Message "Hardware storage evidence verifier: analyzer did not produce $captureAnalysisPath"
    $captureAnalysis = Get-Content -Raw -Path $captureAnalysisPath | ConvertFrom-Json
    $captureStage = [string]$captureAnalysis.stage
    $capturePass = [bool]$captureAnalysis.pass
}

$verification = [PSCustomObject]@{
    tool = "verify-hardware-storage-evidence"
    evidence_dir = $resolvedEvidenceDir
    manifest = $manifestPath
    bundle_pass = $true
    capture_checked = (-not [string]::IsNullOrWhiteSpace($CapturePath))
    capture_pass = $capturePass
    capture_stage = $captureStage
    capture_analysis_dir = $captureOutputDir
    capture_vmd_handoff = if ($null -ne $captureAnalysis) { $captureAnalysis.vmd_handoff } else { $null }
    analyzer_exit_code = $analysisExitCode
    iso = $isoProof
    uefi_image = $uefiProof
    dynamic_app = $appProof
    dynamic_interpreter = $interpProof
    reserves = [PSCustomObject]@{
        bios_sectors = $biosReserve
        uefi_bytes = $uefiReserve
    }
}

$verificationJsonPath = Join-Path $OutputDir "hardware-storage-evidence-verification.json"
$verificationTextPath = Join-Path $OutputDir "hardware-storage-evidence-verification.txt"
$verification | ConvertTo-Json -Depth 6 | Set-Content -Path $verificationJsonPath -Encoding Ascii
$captureVmdKind = ""
$captureVmdStage = ""
$captureVmdDriverPlanState = ""
$captureVmdDriverPlanToken = ""
if ($null -ne $captureAnalysis) {
    $captureVmdKind = [string]$captureAnalysis.vmd_handoff.kind
    $captureVmdStage = [string]$captureAnalysis.vmd_handoff.stage
    $captureVmdDriverPlanState = [string]$captureAnalysis.vmd_handoff.nested_driver_plan_state
    $captureVmdDriverPlanToken = [string]$captureAnalysis.vmd_handoff.nested_driver_plan_token
}

@(
    "hardware-storage-evidence: verified",
    "bundle-pass: True",
    "evidence-dir: $resolvedEvidenceDir",
    "iso-sha256: $($isoProof.sha256)",
    "uefi-image-sha256: $($uefiProof.sha256)",
    "dynamic-app-sha256: $($appProof.sha256)",
    "dynamic-interpreter-sha256: $($interpProof.sha256)",
    "bios-sector-reserve: $biosReserve",
    "uefi-byte-reserve: $uefiReserve",
    "capture-checked: $(-not [string]::IsNullOrWhiteSpace($CapturePath))",
    "capture-pass: $capturePass",
    "capture-stage: $captureStage",
    "capture-vmd-handoff-kind: $captureVmdKind",
    "capture-vmd-handoff-stage: $captureVmdStage",
    "capture-vmd-driver-plan-state: $captureVmdDriverPlanState",
    "capture-vmd-driver-plan-token: $captureVmdDriverPlanToken",
    "output-json: $verificationJsonPath"
) | Set-Content -Path $verificationTextPath -Encoding Ascii

Write-Host "hardware-storage-evidence: verified"
Write-Host "  bundle pass: True"
Write-Host "  bios reserve: $biosReserve sectors"
Write-Host "  uefi reserve: $uefiReserve bytes"
if (-not [string]::IsNullOrWhiteSpace($CapturePath)) {
    Write-Host "  capture pass: $capturePass"
    Write-Host "  capture stage: $captureStage"
}
Write-Host "  output: $verificationJsonPath"

if ((-not [string]::IsNullOrWhiteSpace($CapturePath)) -and (-not $capturePass)) {
    exit 2
}
