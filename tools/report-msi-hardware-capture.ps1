param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDir,

    [Parameter(Mandatory = $true)]
    [string]$CapturePath,

    [string]$OutputDir = "",

    [switch]$RequireStagedDynamicArtifacts,

    [switch]$RequireGuiInteractionTelemetry
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "dist\msi-hardware-capture-report-$stamp"
}

function Assert-PathExists
{
    param(
        [string]$Path,
        [string]$Message
    )

    if (-not (Test-Path $Path)) {
        throw $Message
    }
}

function Read-JsonFile
{
    param(
        [string]$Path,
        [string]$Message
    )

    Assert-PathExists -Path $Path -Message $Message
    return Get-Content -Raw -Path $Path | ConvertFrom-Json
}

function Get-PropertyText
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

function Get-PropertyBool
{
    param(
        [object]$Object,
        [string]$Name,
        [bool]$Default = $false
    )

    if ($null -eq $Object) {
        return $Default
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $Default
    }
    return [bool]$property.Value
}

Assert-PathExists -Path $EvidenceDir -Message "MSI capture report: evidence directory not found: $EvidenceDir"
Assert-PathExists -Path $CapturePath -Message "MSI capture report: capture file not found: $CapturePath"

$resolvedEvidenceDir = (Resolve-Path $EvidenceDir).Path
$resolvedCapturePath = (Resolve-Path $CapturePath).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$classifierOutputDir = Join-Path $OutputDir "classifier"
$classifierArgs = @{
    EvidenceDir = $resolvedEvidenceDir
    CapturePath = $resolvedCapturePath
    OutputDir = $classifierOutputDir
}
if ($RequireStagedDynamicArtifacts.IsPresent) {
    $classifierArgs["RequireStagedDynamicArtifacts"] = $true
}
if ($RequireGuiInteractionTelemetry.IsPresent) {
    $classifierArgs["RequireGuiInteractionTelemetry"] = $true
}

$global:LASTEXITCODE = 0
& (Join-Path $root "tools\classify-m134-storage-target.ps1") @classifierArgs
$classifierExitCode = [int]$LASTEXITCODE
if (($classifierExitCode -ne 0) -and ($classifierExitCode -ne 2)) {
    throw "MSI capture report: classifier failed unexpectedly with exit code $classifierExitCode."
}

$classifierJsonPath = Join-Path $classifierOutputDir "m134-storage-target.json"
$classifier = Read-JsonFile `
    -Path $classifierJsonPath `
    -Message "MSI capture report: classifier did not produce $classifierJsonPath"

$handoffJsonPath = Get-PropertyText -Object $classifier -Name "handoff_json"
$combinedJsonPath = Get-PropertyText -Object $classifier -Name "combined_json"
$handoff = $null
$combined = $null
if (-not [string]::IsNullOrWhiteSpace($handoffJsonPath)) {
    $handoff = Read-JsonFile `
        -Path $handoffJsonPath `
        -Message "MSI capture report: handoff verifier did not produce $handoffJsonPath"
}
if (-not [string]::IsNullOrWhiteSpace($combinedJsonPath)) {
    $combined = Read-JsonFile `
        -Path $combinedJsonPath `
        -Message "MSI capture report: combined analyzer did not produce $combinedJsonPath"
}

$manifestPath = Join-Path $resolvedEvidenceDir "hardware-storage-evidence-manifest.json"
$manifest = Read-JsonFile `
    -Path $manifestPath `
    -Message "MSI capture report: evidence manifest missing: $manifestPath"

$targetKind = Get-PropertyText -Object $classifier -Name "target_kind"
$targetStage = Get-PropertyText -Object $classifier -Name "target_stage"
$roadmapTarget = Get-PropertyText -Object $classifier -Name "roadmap_target"
$nextTarget = Get-PropertyText -Object $classifier -Name "next_target"
$capturePass = Get-PropertyBool -Object $classifier -Name "pass"

$report = [PSCustomObject]@{
    tool = "report-msi-hardware-capture"
    evidence_dir = $resolvedEvidenceDir
    capture_path = $resolvedCapturePath
    pass = $capturePass
    target_kind = $targetKind
    target_stage = $targetStage
    roadmap_target = $roadmapTarget
    next_target = $nextTarget
    classifier_exit_code = $classifierExitCode
    require_staged_dynamic_artifacts = [bool]$RequireStagedDynamicArtifacts
    require_gui_interaction_telemetry = [bool]$RequireGuiInteractionTelemetry
    artifacts = [PSCustomObject]@{
        milestone = Get-PropertyText -Object $manifest -Name "milestone"
        iso = Get-PropertyText -Object $manifest.iso -Name "path"
        iso_sha256 = Get-PropertyText -Object $manifest.iso -Name "sha256"
        uefi_image = Get-PropertyText -Object $manifest.uefi_image -Name "path"
        uefi_image_sha256 = Get-PropertyText -Object $manifest.uefi_image -Name "sha256"
        dynamic_app = Get-PropertyText -Object $manifest.dynamic_app -Name "path"
        dynamic_app_sha256 = Get-PropertyText -Object $manifest.dynamic_app -Name "sha256"
        dynamic_interpreter = Get-PropertyText -Object $manifest.dynamic_interpreter -Name "path"
        dynamic_interpreter_sha256 = Get-PropertyText -Object $manifest.dynamic_interpreter -Name "sha256"
    }
    storage = $classifier.storage
    display_input = $classifier.display_input
    dynamic_handoff = $classifier.dynamic_handoff
    vmd_handoff = $classifier.vmd_handoff
    handoff = [PSCustomObject]@{
        pass = if ($null -ne $handoff) { Get-PropertyBool -Object $handoff -Name "handoff_pass" } else { $false }
        storage_bundle_pass = if ($null -ne $handoff) { Get-PropertyBool -Object $handoff -Name "storage_bundle_pass" } else { $false }
        combined_capture_pass = if ($null -ne $handoff) { Get-PropertyBool -Object $handoff -Name "combined_capture_pass" } else { $false }
        combined_capture_stage = if ($null -ne $handoff) { Get-PropertyText -Object $handoff -Name "combined_capture_stage" } else { "" }
        gui_interaction_pass = if ($null -ne $handoff) { Get-PropertyBool -Object $handoff -Name "gui_interaction_pass" } else { $false }
        gui_interaction_stage = if ($null -ne $handoff) { Get-PropertyText -Object $handoff -Name "gui_interaction_stage" } else { "" }
        dynamic_handoff_pass = if ($null -ne $handoff) { Get-PropertyBool -Object $handoff -Name "dynamic_handoff_pass" } else { $false }
        dynamic_handoff_stage = if ($null -ne $handoff) { Get-PropertyText -Object $handoff -Name "dynamic_handoff_stage" } else { "" }
    }
    combined_stage = if ($null -ne $combined) { Get-PropertyText -Object $combined -Name "stage" } else { "" }
    reserves = $classifier.reserves
    classifier_json = $classifierJsonPath
    handoff_json = $handoffJsonPath
    combined_json = $combinedJsonPath
}

$reportJsonPath = Join-Path $OutputDir "msi-hardware-capture-report.json"
$reportTextPath = Join-Path $OutputDir "msi-hardware-capture-report.txt"
$reportMarkdownPath = Join-Path $OutputDir "msi-hardware-capture-report.md"
$reportVmdKind = ""
$reportVmdStage = ""
if ($null -ne $report.vmd_handoff) {
    $reportVmdKind = [string]$report.vmd_handoff.kind
    $reportVmdStage = [string]$report.vmd_handoff.stage
}

$report | ConvertTo-Json -Depth 10 | Set-Content -Path $reportJsonPath -Encoding Ascii

@(
    "msi-hardware-capture-report: $targetKind-$targetStage",
    "pass: $capturePass",
    "roadmap-target: $roadmapTarget",
    "next-target: $nextTarget",
    "storage-pass: $($report.storage.pass)",
    "storage-stage: $($report.storage.stage)",
    "display-input-pass: $($report.display_input.pass)",
    "display-input-stage: $($report.display_input.stage)",
    "gui-interaction-pass: $($report.display_input.gui_interaction_pass)",
    "gui-interaction-stage: $($report.display_input.gui_interaction_stage)",
    "dynamic-handoff-pass: $($report.dynamic_handoff.pass)",
    "dynamic-handoff-stage: $($report.dynamic_handoff.stage)",
    "vmd-handoff-kind: $reportVmdKind",
    "vmd-handoff-stage: $reportVmdStage",
    "bios-sector-reserve: $($report.reserves.bios_sectors)",
    "uefi-byte-reserve: $($report.reserves.uefi_bytes)",
    "classifier-json: $classifierJsonPath",
    "handoff-json: $handoffJsonPath",
    "combined-json: $combinedJsonPath",
    "output-json: $reportJsonPath"
) | Set-Content -Path $reportTextPath -Encoding Ascii

@(
    "# MSI Hardware Capture Report",
    "",
    "- Pass: $capturePass",
    "- Target kind: $targetKind",
    "- Target stage: $targetStage",
    "- Roadmap target: $roadmapTarget",
    "- Next target: $nextTarget",
    "- BIOS reserve: $($report.reserves.bios_sectors) sectors",
    "- UEFI reserve: $($report.reserves.uefi_bytes) bytes",
    "",
    "## Evidence Media",
    "",
    "| Artifact | Value |",
    "| --- | --- |",
    "| ISO | $($report.artifacts.iso) |",
    "| ISO SHA-256 | $($report.artifacts.iso_sha256) |",
    "| UEFI image | $($report.artifacts.uefi_image) |",
    "| UEFI image SHA-256 | $($report.artifacts.uefi_image_sha256) |",
    "| Dynamic app | $($report.artifacts.dynamic_app) |",
    "| Dynamic app SHA-256 | $($report.artifacts.dynamic_app_sha256) |",
    "| Dynamic interpreter | $($report.artifacts.dynamic_interpreter) |",
    "| Dynamic interpreter SHA-256 | $($report.artifacts.dynamic_interpreter_sha256) |",
    "",
    "## Capture Classification",
    "",
    "| Area | Pass | Stage |",
    "| --- | --- | --- |",
    "| Storage | $($report.storage.pass) | $($report.storage.stage) |",
    "| Display/Input | $($report.display_input.pass) | $($report.display_input.stage) |",
    "| GUI interaction | $($report.display_input.gui_interaction_pass) | $($report.display_input.gui_interaction_stage) |",
    "| Dynamic handoff | $($report.dynamic_handoff.pass) | $($report.dynamic_handoff.stage) |",
    "| VMD/NVMe handoff | $reportVmdKind | $reportVmdStage |",
    "",
    "## Outputs",
    "",
    "- Classifier JSON: $classifierJsonPath",
    "- Handoff JSON: $handoffJsonPath",
    "- Combined analyzer JSON: $combinedJsonPath"
) | Set-Content -Path $reportMarkdownPath -Encoding Ascii

Write-Host "msi-hardware-capture-report: $targetKind-$targetStage"
Write-Host "  pass: $capturePass"
Write-Host "  roadmap target: $roadmapTarget"
Write-Host "  next target: $nextTarget"
Write-Host "  output: $reportJsonPath"

if (-not $capturePass) {
    exit 2
}
