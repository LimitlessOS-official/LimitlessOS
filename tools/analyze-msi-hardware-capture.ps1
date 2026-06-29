param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDir,

    [Parameter(Mandatory = $true)]
    [string]$CapturePath,

    [string]$OutputDir = "",

    [switch]$RequireStagedDynamicArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "dist\m118-msi-hardware-analysis-$stamp"
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

function Invoke-AnalysisScript
{
    param(
        [string]$ScriptPath,
        [hashtable]$Arguments,
        [string]$Label
    )

    $global:LASTEXITCODE = 0
    & $ScriptPath @Arguments
    $exitCode = $LASTEXITCODE
    if (($exitCode -ne 0) -and ($exitCode -ne 2)) {
        throw "MSI hardware analyzer: $Label failed unexpectedly with exit code $exitCode."
    }
    return [int]$exitCode
}

Assert-PathExists -Path $EvidenceDir -Message "MSI hardware analyzer: evidence directory not found: $EvidenceDir"
Assert-PathExists -Path $CapturePath -Message "MSI hardware analyzer: capture file not found: $CapturePath"

$resolvedEvidenceDir = (Resolve-Path $EvidenceDir).Path
$resolvedCapturePath = (Resolve-Path $CapturePath).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$storageOutputDir = Join-Path $OutputDir "storage"
$displayInputOutputDir = Join-Path $OutputDir "display-input"

$storageArgs = @{
    EvidenceDir = $resolvedEvidenceDir
    CapturePath = $resolvedCapturePath
    OutputDir = $storageOutputDir
}
if ($RequireStagedDynamicArtifacts.IsPresent) {
    $storageArgs["RequireStagedDynamicArtifacts"] = $true
}

$storageExitCode = Invoke-AnalysisScript `
    -ScriptPath (Join-Path $root "tools\verify-hardware-storage-evidence.ps1") `
    -Arguments $storageArgs `
    -Label "storage evidence verifier"

$storageJsonPath = Join-Path $storageOutputDir "hardware-storage-evidence-verification.json"
$storageVerification = Read-JsonFile `
    -Path $storageJsonPath `
    -Message "MSI hardware analyzer: storage verifier did not produce $storageJsonPath"

$storageCaptureAnalysis = $null
$storageCaptureStage = Get-PropertyText -Object $storageVerification -Name "capture_stage"
$storageCapturePass = [bool]$storageVerification.capture_pass
$storageNextTarget = Get-PropertyText -Object $storageVerification -Name "capture_next_target"
$storageCaptureAnalysisDir = Get-PropertyText -Object $storageVerification -Name "capture_analysis_dir"
if (-not [string]::IsNullOrWhiteSpace($storageCaptureAnalysisDir)) {
    $nestedStorageJsonPath = Join-Path $storageCaptureAnalysisDir "hardware-storage-analysis.json"
    $storageCaptureAnalysis = Read-JsonFile `
        -Path $nestedStorageJsonPath `
        -Message "MSI hardware analyzer: nested storage analyzer did not produce $nestedStorageJsonPath"
    if ([string]::IsNullOrWhiteSpace($storageNextTarget)) {
        $storageNextTarget = Get-PropertyText -Object $storageCaptureAnalysis -Name "next_target"
    }
}

$displayArgs = @{
    InputPath = $resolvedCapturePath
    OutputDir = $displayInputOutputDir
}

$displayExitCode = Invoke-AnalysisScript `
    -ScriptPath (Join-Path $root "tools\analyze-hardware-display-input-capture.ps1") `
    -Arguments $displayArgs `
    -Label "display/input analyzer"

$displayJsonPath = Join-Path $displayInputOutputDir "hardware-display-input-analysis.json"
$displayAnalysis = Read-JsonFile `
    -Path $displayJsonPath `
    -Message "MSI hardware analyzer: display/input analyzer did not produce $displayJsonPath"

$displayStage = Get-PropertyText -Object $displayAnalysis -Name "stage"
$displayPass = [bool]$displayAnalysis.pass
$displayNextTarget = Get-PropertyText -Object $displayAnalysis -Name "next_target"

$bundlePass = [bool]$storageVerification.bundle_pass
$pass = ($bundlePass -and $storageCapturePass -and $displayPass)
$stage = "msi-hardware-ready"
$detail = "Storage evidence, storage capture, and display/input capture are all passing."
$nextTarget = "Hardware storage and display/input ready. Next target: run interactive desktop validation and linux /APPS/DYNLDLIMIT on the physical laptop."

if (-not $bundlePass) {
    $stage = "storage-evidence-bundle"
    $detail = "The evidence bundle did not verify."
    $nextTarget = "Evidence target: rebuild the M113 staged hardware storage evidence bundle from current artifacts."
} elseif (-not $storageCapturePass) {
    $stage = "storage-$storageCaptureStage"
    $detail = "The evidence bundle verified, but the captured storage path did not reach storage-ready."
    if ([string]::IsNullOrWhiteSpace($storageNextTarget)) {
        $nextTarget = "Storage target: inspect storage capture stage $storageCaptureStage."
    } else {
        $nextTarget = $storageNextTarget
    }
} elseif (-not $displayPass) {
    $stage = "display-input-$displayStage"
    $detail = "Storage is verified, but display/input did not reach display-input-ready."
    $nextTarget = $displayNextTarget
}

$biosReserve = [uint64]$storageVerification.reserves.bios_sectors
$uefiReserve = [uint64]$storageVerification.reserves.uefi_bytes

$report = [PSCustomObject]@{
    tool = "analyze-msi-hardware-capture"
    evidence_dir = $resolvedEvidenceDir
    capture_path = $resolvedCapturePath
    pass = [bool]$pass
    stage = $stage
    detail = $detail
    next_target = $nextTarget
    require_staged_dynamic_artifacts = [bool]$RequireStagedDynamicArtifacts
    storage = [PSCustomObject]@{
        exit_code = $storageExitCode
        bundle_pass = $bundlePass
        capture_pass = $storageCapturePass
        capture_stage = $storageCaptureStage
        next_target = $storageNextTarget
        vmd_handoff = $storageVerification.capture_vmd_handoff
        nvme_controller = $storageVerification.capture_nvme_controller
        verification_json = $storageJsonPath
        analysis_dir = $storageCaptureAnalysisDir
    }
    display_input = [PSCustomObject]@{
        exit_code = $displayExitCode
        pass = $displayPass
        stage = $displayStage
        detail = Get-PropertyText -Object $displayAnalysis -Name "detail"
        next_target = $displayNextTarget
        analysis_json = $displayJsonPath
    }
    reserves = [PSCustomObject]@{
        bios_sectors = $biosReserve
        uefi_bytes = $uefiReserve
    }
}

$reportJsonPath = Join-Path $OutputDir "msi-hardware-analysis.json"
$reportTextPath = Join-Path $OutputDir "msi-hardware-analysis.txt"
$reportMarkdownPath = Join-Path $OutputDir "msi-hardware-analysis.md"
$storageVmdKind = ""
$storageVmdStage = ""
$storageVmdDriverPlanState = ""
$storageVmdDriverPlanToken = ""
$storageNvmeProbeError = ""
$storageNvmeRegs = ""
$storageNvmeCapLow = ""
$storageNvmeCapHigh = ""
$storageNvmeVersion = ""
$storageNvmeCc = ""
$storageNvmeCsts = ""
if ($null -ne $storageVerification.capture_vmd_handoff) {
    $storageVmdKind = [string]$storageVerification.capture_vmd_handoff.kind
    $storageVmdStage = [string]$storageVerification.capture_vmd_handoff.stage
    $storageVmdDriverPlanState = [string]$storageVerification.capture_vmd_handoff.nested_driver_plan_state
    $storageVmdDriverPlanToken = [string]$storageVerification.capture_vmd_handoff.nested_driver_plan_token
}
if ($null -ne $storageVerification.capture_nvme_controller) {
    $storageNvmeProbeError = [string]$storageVerification.capture_nvme_controller.probe_error
    $storageNvmeRegs = [string]$storageVerification.capture_nvme_controller.regs
    $storageNvmeCapLow = [string]$storageVerification.capture_nvme_controller.cap_low
    $storageNvmeCapHigh = [string]$storageVerification.capture_nvme_controller.cap_high
    $storageNvmeVersion = [string]$storageVerification.capture_nvme_controller.vs
    $storageNvmeCc = [string]$storageVerification.capture_nvme_controller.cc
    $storageNvmeCsts = [string]$storageVerification.capture_nvme_controller.csts
}

$report | ConvertTo-Json -Depth 8 | Set-Content -Path $reportJsonPath -Encoding Ascii

@(
    "msi-hardware-analysis: $stage",
    "pass: $pass",
    "detail: $detail",
    "next-target: $nextTarget",
    "storage-bundle-pass: $bundlePass",
    "storage-capture-pass: $storageCapturePass",
    "storage-stage: $storageCaptureStage",
    "storage-vmd-handoff-kind: $storageVmdKind",
    "storage-vmd-handoff-stage: $storageVmdStage",
    "storage-vmd-driver-plan-state: $storageVmdDriverPlanState",
    "storage-vmd-driver-plan-token: $storageVmdDriverPlanToken",
    "storage-nvme-probe-error: $storageNvmeProbeError",
    "storage-nvme-regs: $storageNvmeRegs",
    "storage-nvme-cap-low: $storageNvmeCapLow",
    "storage-nvme-cap-high: $storageNvmeCapHigh",
    "storage-nvme-vs: $storageNvmeVersion",
    "storage-nvme-cc: $storageNvmeCc",
    "storage-nvme-csts: $storageNvmeCsts",
    "display-input-pass: $displayPass",
    "display-input-stage: $displayStage",
    "bios-sector-reserve: $biosReserve",
    "uefi-byte-reserve: $uefiReserve",
    "output-json: $reportJsonPath",
    "output-report: $reportMarkdownPath"
) | Set-Content -Path $reportTextPath -Encoding Ascii

@(
    "# LimitlessOS M118 MSI Hardware Analysis",
    "",
    "- Pass: $pass",
    "- Stage: $stage",
    "- Detail: $detail",
    "- Next target: $nextTarget",
    "- BIOS reserve: $biosReserve sectors",
    "- UEFI reserve: $uefiReserve bytes",
    "- Staged dynamic artifacts required: $([bool]$RequireStagedDynamicArtifacts)",
    "",
    "## Storage",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| Bundle pass | $bundlePass |",
    "| Capture pass | $storageCapturePass |",
    "| Capture stage | $storageCaptureStage |",
    "| Next target | $storageNextTarget |",
    "| VMD handoff | $storageVmdKind |",
    "| VMD handoff stage | $storageVmdStage |",
    "| VMD driver plan state | $storageVmdDriverPlanState |",
    "| VMD driver plan token | $storageVmdDriverPlanToken |",
    "| NVMe probe error | $storageNvmeProbeError |",
    "| NVMe register snapshot | $storageNvmeRegs |",
    "| NVMe CAP low | $storageNvmeCapLow |",
    "| NVMe CAP high | $storageNvmeCapHigh |",
    "| NVMe VS | $storageNvmeVersion |",
    "| NVMe CC | $storageNvmeCc |",
    "| NVMe CSTS | $storageNvmeCsts |",
    "| Verification JSON | $storageJsonPath |",
    "",
    "## Display/Input",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| Pass | $displayPass |",
    "| Stage | $displayStage |",
    "| Detail | $(Get-PropertyText -Object $displayAnalysis -Name "detail") |",
    "| Next target | $displayNextTarget |",
    "| Analysis JSON | $displayJsonPath |"
) | Set-Content -Path $reportMarkdownPath -Encoding Ascii

Write-Host "msi-hardware-analysis: $stage"
Write-Host "  pass: $pass"
Write-Host "  storage stage: $storageCaptureStage"
Write-Host "  display/input stage: $displayStage"
Write-Host "  next target: $nextTarget"
Write-Host "  bios reserve: $biosReserve sectors"
Write-Host "  uefi reserve: $uefiReserve bytes"
Write-Host "  output: $reportJsonPath"

if (-not $pass) {
    exit 2
}
