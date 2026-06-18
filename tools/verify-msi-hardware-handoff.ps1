param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDir,

    [string]$CapturePath = "",

    [string]$OutputDir = "",

    [switch]$RequireStagedDynamicArtifacts,

    [switch]$RunBootMediaVerifier
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

function Assert-TextContains
{
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    if ($Text -notmatch $Pattern) {
        throw $Message
    }
}

function Get-ManifestProperty
{
    param(
        [object]$Object,
        [string]$Name,
        [string]$Default = ""
    )

    if ($null -eq $Object) {
        return $Default
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $Default
    }
    return [string]$property.Value
}

Assert-FileExists -Path $EvidenceDir -Message "MSI hardware handoff verifier: evidence directory not found: $EvidenceDir"
$resolvedEvidenceDir = (Resolve-Path $EvidenceDir).Path

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $resolvedEvidenceDir "msi-handoff-verification"
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$manifestPath = Join-Path $resolvedEvidenceDir "hardware-storage-evidence-manifest.json"
$runbookPath = Join-Path $resolvedEvidenceDir "README-HARDWARE-STORAGE.txt"
Assert-FileExists -Path $manifestPath -Message "MSI hardware handoff verifier: JSON manifest missing: $manifestPath"
Assert-FileExists -Path $runbookPath -Message "MSI hardware handoff verifier: runbook missing: $runbookPath"

$storageOutputDir = Join-Path $OutputDir "storage-evidence"
$storageArgs = @{
    EvidenceDir = $resolvedEvidenceDir
    OutputDir = $storageOutputDir
}
if ($RequireStagedDynamicArtifacts.IsPresent) {
    $storageArgs["RequireStagedDynamicArtifacts"] = $true
}
if (-not [string]::IsNullOrWhiteSpace($CapturePath)) {
    $storageArgs["CapturePath"] = $CapturePath
}

$global:LASTEXITCODE = 0
& (Join-Path $root "tools\verify-hardware-storage-evidence.ps1") @storageArgs
$storageExitCode = $LASTEXITCODE
if (($storageExitCode -ne 0) -and ($storageExitCode -ne 2)) {
    throw "MSI hardware handoff verifier: storage evidence verifier failed unexpectedly with exit code $storageExitCode."
}

$storageVerificationPath = Join-Path $storageOutputDir "hardware-storage-evidence-verification.json"
Assert-FileExists -Path $storageVerificationPath -Message "MSI hardware handoff verifier: storage verifier did not write $storageVerificationPath"
$storageVerification = Get-Content -Raw -Path $storageVerificationPath | ConvertFrom-Json

$manifest = Get-Content -Raw -Path $manifestPath | ConvertFrom-Json
$runbook = Get-Content -Raw -Path $runbookPath

$milestone = Get-ManifestProperty -Object $manifest -Name "milestone"
$purpose = Get-ManifestProperty -Object $manifest -Name "purpose"
$isoPath = Get-ManifestProperty -Object $manifest.iso -Name "path"
$uefiImagePath = Get-ManifestProperty -Object $manifest.uefi_image -Name "path"
$dynamicAppPath = Get-ManifestProperty -Object $manifest.dynamic_app -Name "path"
$dynamicInterpPath = Get-ManifestProperty -Object $manifest.dynamic_interpreter -Name "path"
$expectedAnalyzer = Get-ManifestProperty -Object $manifest.expected_hwval -Name "analyzer"
$storageVerifier = Get-ManifestProperty -Object $manifest.expected_hwval -Name "storage_verifier"
$bootMediaVerifier = Get-ManifestProperty -Object $manifest.expected_hwval -Name "boot_media_handoff_verifier"
$requiredStorageStage = Get-ManifestProperty -Object $manifest.expected_hwval -Name "required_storage_stage"
$requiredBootMediaSource = Get-ManifestProperty -Object $manifest.expected_hwval -Name "required_boot_media_linux_source"

if ($milestone -ne "M121") {
    throw "MSI hardware handoff verifier: manifest milestone must be M121, observed '$milestone'."
}
if ($purpose -ne "MSI hardware handoff evidence bundle") {
    throw "MSI hardware handoff verifier: manifest purpose mismatch: '$purpose'."
}
if ($isoPath -ne "limitlessos-x86_64-m121-handoff.iso") {
    throw "MSI hardware handoff verifier: ISO path mismatch: '$isoPath'."
}
if ($uefiImagePath -ne "limitlessos-x86_64-m121-handoff-uefi.img") {
    throw "MSI hardware handoff verifier: UEFI image path mismatch: '$uefiImagePath'."
}
if ($dynamicAppPath -ne "/APPS/DYNLDLIMIT") {
    throw "MSI hardware handoff verifier: dynamic app path mismatch: '$dynamicAppPath'."
}
if ($dynamicInterpPath -ne "/APPS/LDLIMIT") {
    throw "MSI hardware handoff verifier: dynamic interpreter path mismatch: '$dynamicInterpPath'."
}
if ($expectedAnalyzer -ne "tools\\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts") {
    throw "MSI hardware handoff verifier: manifest analyzer mismatch: '$expectedAnalyzer'."
}
if ($storageVerifier -ne "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts") {
    throw "MSI hardware handoff verifier: storage verifier mismatch: '$storageVerifier'."
}
if ($bootMediaVerifier -ne "tools\\verify-boot-media-linux-handoff.ps1") {
    throw "MSI hardware handoff verifier: boot-media verifier mismatch: '$bootMediaVerifier'."
}
if ($requiredStorageStage -ne "storage-ready") {
    throw "MSI hardware handoff verifier: required storage stage mismatch: '$requiredStorageStage'."
}
if ($requiredBootMediaSource -ne "2") {
    throw "MSI hardware handoff verifier: required boot-media source mismatch: '$requiredBootMediaSource'."
}

Assert-TextContains -Text $runbook -Pattern '(?m)^\s*hwval\s*$' -Message "MSI hardware handoff verifier: runbook does not instruct the tester to run hwval."
Assert-TextContains -Text $runbook -Pattern '(?m)^\s*linux /APPS/DYNLDLIMIT\s*$' -Message "MSI hardware handoff verifier: runbook does not instruct the tester to run linux /APPS/DYNLDLIMIT."
Assert-TextContains -Text $runbook -Pattern 'analyze-msi-hardware-capture\.ps1 .* -RequireStagedDynamicArtifacts' -Message "MSI hardware handoff verifier: runbook does not use the combined MSI analyzer."
Assert-TextContains -Text $runbook -Pattern 'linux: using UEFI boot-media staged file' -Message "MSI hardware handoff verifier: runbook is missing the boot-media staged-file signal."
Assert-TextContains -Text $runbook -Pattern 'drs-realbin \.\.\. source 2 \.\.\. boot-media-read 1' -Message "MSI hardware handoff verifier: runbook is missing the source-2 boot-media telemetry expectation."
Assert-TextContains -Text $runbook -Pattern 'verify-boot-media-linux-handoff\.ps1' -Message "MSI hardware handoff verifier: runbook is missing the boot-media handoff verifier."

$combinedAnalysis = $null
$combinedStage = ""
$combinedPass = $false
$combinedOutputDir = ""
$combinedExitCode = 0
if (-not [string]::IsNullOrWhiteSpace($CapturePath)) {
    Assert-FileExists -Path $CapturePath -Message "MSI hardware handoff verifier: capture file not found: $CapturePath"
    $combinedOutputDir = Join-Path $OutputDir "msi-analysis"
    $combinedArgs = @{
        EvidenceDir = $resolvedEvidenceDir
        CapturePath = (Resolve-Path $CapturePath).Path
        OutputDir = $combinedOutputDir
    }
    if ($RequireStagedDynamicArtifacts.IsPresent) {
        $combinedArgs["RequireStagedDynamicArtifacts"] = $true
    }

    $global:LASTEXITCODE = 0
    & (Join-Path $root "tools\analyze-msi-hardware-capture.ps1") @combinedArgs
    $combinedExitCode = $LASTEXITCODE
    if (($combinedExitCode -ne 0) -and ($combinedExitCode -ne 2)) {
        throw "MSI hardware handoff verifier: combined analyzer failed unexpectedly with exit code $combinedExitCode."
    }

    $combinedAnalysisPath = Join-Path $combinedOutputDir "msi-hardware-analysis.json"
    Assert-FileExists -Path $combinedAnalysisPath -Message "MSI hardware handoff verifier: combined analyzer did not write $combinedAnalysisPath"
    $combinedAnalysis = Get-Content -Raw -Path $combinedAnalysisPath | ConvertFrom-Json
    $combinedStage = [string]$combinedAnalysis.stage
    $combinedPass = [bool]$combinedAnalysis.pass
}

$bootMediaVerifierRan = $false
$bootMediaVerifierExitCode = 0
if ($RunBootMediaVerifier.IsPresent) {
    $global:LASTEXITCODE = 0
    & (Join-Path $root "tools\verify-boot-media-linux-handoff.ps1")
    $bootMediaVerifierExitCode = $LASTEXITCODE
    $bootMediaVerifierRan = $true
    if ($bootMediaVerifierExitCode -ne 0) {
        throw "MSI hardware handoff verifier: boot-media Linux handoff verifier failed with exit code $bootMediaVerifierExitCode."
    }
}

$verification = [PSCustomObject]@{
    tool = "verify-msi-hardware-handoff"
    evidence_dir = $resolvedEvidenceDir
    handoff_pass = $true
    storage_bundle_pass = [bool]$storageVerification.bundle_pass
    storage_capture_checked = [bool]$storageVerification.capture_checked
    storage_capture_pass = [bool]$storageVerification.capture_pass
    storage_capture_stage = [string]$storageVerification.capture_stage
    combined_capture_checked = (-not [string]::IsNullOrWhiteSpace($CapturePath))
    combined_capture_pass = $combinedPass
    combined_capture_stage = $combinedStage
    combined_analyzer_exit_code = $combinedExitCode
    boot_media_verifier_ran = $bootMediaVerifierRan
    boot_media_verifier_exit_code = $bootMediaVerifierExitCode
    milestone = $milestone
    expected_boot_media_linux_source = [uint32]$requiredBootMediaSource
    reserves = $storageVerification.reserves
}

$verificationJsonPath = Join-Path $OutputDir "msi-hardware-handoff-verification.json"
$verificationTextPath = Join-Path $OutputDir "msi-hardware-handoff-verification.txt"
$verification | ConvertTo-Json -Depth 6 | Set-Content -Path $verificationJsonPath -Encoding Ascii

@(
    "msi-hardware-handoff: verified",
    "handoff-pass: True",
    "evidence-dir: $resolvedEvidenceDir",
    "milestone: $milestone",
    "source2-required: $requiredBootMediaSource",
    "storage-bundle-pass: $($verification.storage_bundle_pass)",
    "storage-capture-checked: $($verification.storage_capture_checked)",
    "storage-capture-stage: $($verification.storage_capture_stage)",
    "combined-capture-checked: $($verification.combined_capture_checked)",
    "combined-capture-stage: $combinedStage",
    "boot-media-verifier-ran: $bootMediaVerifierRan",
    "bios-sector-reserve: $($storageVerification.reserves.bios_sectors)",
    "uefi-byte-reserve: $($storageVerification.reserves.uefi_bytes)",
    "output-json: $verificationJsonPath"
) | Set-Content -Path $verificationTextPath -Encoding Ascii

Write-Host "msi-hardware-handoff: verified"
Write-Host "  handoff pass: True"
Write-Host "  source2 required: $requiredBootMediaSource"
Write-Host "  bios reserve: $($storageVerification.reserves.bios_sectors) sectors"
Write-Host "  uefi reserve: $($storageVerification.reserves.uefi_bytes) bytes"
if (-not [string]::IsNullOrWhiteSpace($CapturePath)) {
    Write-Host "  combined capture pass: $combinedPass"
    Write-Host "  combined capture stage: $combinedStage"
}
Write-Host "  output: $verificationJsonPath"

if ((-not [string]::IsNullOrWhiteSpace($CapturePath)) -and (-not $combinedPass)) {
    exit 2
}
