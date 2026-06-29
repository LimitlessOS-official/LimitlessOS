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
    $OutputDir = Join-Path $root "dist\m134-storage-target-$stamp"
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

function New-Target
{
    param(
        [bool]$Pass,
        [string]$Kind,
        [string]$Stage,
        [string]$Detail,
        [string]$NextTarget,
        [string]$Milestone
    )

    return [PSCustomObject]@{
        pass = $Pass
        target_kind = $Kind
        target_stage = $Stage
        detail = $Detail
        next_target = $NextTarget
        roadmap_target = $Milestone
    }
}

function Get-DisplayInputRoadmapTarget
{
    param([string]$Stage)

    if ($Stage -like "cursor-*") {
        return "M150"
    }
    if ($Stage -like "gui-*") {
        return "M151"
    }
    return "M149"
}

function Invoke-HandoffVerifier
{
    param(
        [string]$ResolvedEvidenceDir,
        [string]$ResolvedCapturePath,
        [string]$VerifierOutputDir,
        [bool]$RequireStaged,
        [bool]$RequireGuiTelemetry
    )

    $arguments = @{
        EvidenceDir = $ResolvedEvidenceDir
        CapturePath = $ResolvedCapturePath
        OutputDir = $VerifierOutputDir
    }
    if ($RequireStaged) {
        $arguments["RequireStagedDynamicArtifacts"] = $true
    }
    if ($RequireGuiTelemetry) {
        $arguments["RequireGuiInteractionTelemetry"] = $true
    }

    $global:LASTEXITCODE = 0
    & (Join-Path $root "tools\verify-msi-hardware-handoff.ps1") @arguments
    $exitCode = [int]$LASTEXITCODE
    if (($exitCode -ne 0) -and ($exitCode -ne 2)) {
        throw "M134 storage target classifier: MSI handoff verifier failed unexpectedly with exit code $exitCode."
    }
    return $exitCode
}

Assert-PathExists -Path $EvidenceDir -Message "M134 storage target classifier: evidence directory not found: $EvidenceDir"
Assert-PathExists -Path $CapturePath -Message "M134 storage target classifier: capture file not found: $CapturePath"

$resolvedEvidenceDir = (Resolve-Path $EvidenceDir).Path
$resolvedCapturePath = (Resolve-Path $CapturePath).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$handoffOutputDir = Join-Path $OutputDir "handoff"
$handoffExitCode = Invoke-HandoffVerifier `
    -ResolvedEvidenceDir $resolvedEvidenceDir `
    -ResolvedCapturePath $resolvedCapturePath `
    -VerifierOutputDir $handoffOutputDir `
    -RequireStaged ([bool]$RequireStagedDynamicArtifacts) `
    -RequireGuiTelemetry ([bool]$RequireGuiInteractionTelemetry)

$handoffJsonPath = Join-Path $handoffOutputDir "msi-hardware-handoff-verification.json"
$handoff = Read-JsonFile `
    -Path $handoffJsonPath `
    -Message "M134 storage target classifier: handoff verifier did not produce $handoffJsonPath"

$combinedJsonPath = Join-Path $handoffOutputDir "msi-analysis\msi-hardware-analysis.json"
$combined = $null
if (Test-Path $combinedJsonPath) {
    $combined = Read-JsonFile `
        -Path $combinedJsonPath `
        -Message "M134 storage target classifier: combined MSI analyzer did not produce $combinedJsonPath"
}

$storageStage = Get-PropertyText -Object $handoff -Name "storage_capture_stage"
$storagePass = [bool]$handoff.storage_capture_pass
$storageNextTarget = ""
if ($null -ne $combined) {
    $storageNextTarget = Get-PropertyText -Object $combined.storage -Name "next_target"
    if ([string]::IsNullOrWhiteSpace($storageNextTarget)) {
        $storageNextTarget = Get-PropertyText -Object $combined -Name "next_target"
    }
}
$combinedStage = Get-PropertyText -Object $handoff -Name "combined_capture_stage"
$combinedPass = [bool]$handoff.combined_capture_pass
$dynamicStage = Get-PropertyText -Object $handoff -Name "dynamic_handoff_stage"
$dynamicPass = [bool]$handoff.dynamic_handoff_pass
$guiInteractionRequired = [bool]$handoff.gui_interaction_required
$guiInteractionPass = [bool]$handoff.gui_interaction_pass
$guiInteractionStage = Get-PropertyText -Object $handoff -Name "gui_interaction_stage"
$displayStage = ""
$displayPass = $false
$displayNextTarget = ""
if ($null -ne $combined) {
    $displayStage = Get-PropertyText -Object $combined.display_input -Name "stage"
    $displayPass = [bool]$combined.display_input.pass
    $displayNextTarget = Get-PropertyText -Object $combined.display_input -Name "next_target"
}
$vmdHandoff = $null
if (($null -ne $combined) -and ($null -ne $combined.storage)) {
    $vmdHandoff = $combined.storage.vmd_handoff
}

$target = $null
if (-not [bool]$handoff.storage_bundle_pass) {
    $target = New-Target `
        -Pass $false `
        -Kind "evidence-bundle" `
        -Stage "storage-evidence-bundle" `
        -Detail "The M133 handoff bundle failed before physical storage classification could begin." `
        -NextTarget "Rebuild the M133 handoff bundle and verify hashes, BOOTMAN staging, and reserves before hardware testing." `
        -Milestone "M133"
} elseif (-not $storagePass) {
    if ([string]::IsNullOrWhiteSpace($storageNextTarget)) {
        $storageNextTarget = "Storage target: inspect first failing storage stage '$storageStage'."
    }
    $target = New-Target `
        -Pass $false `
        -Kind "storage" `
        -Stage $storageStage `
        -Detail "The first physical hardware blocker is in the NVMe/GPT/FAT storage path." `
        -NextTarget $storageNextTarget `
        -Milestone "M134"
} elseif (-not $combinedPass) {
    if ($combinedStage -like "display-input-*") {
        if ([string]::IsNullOrWhiteSpace($displayNextTarget)) {
            $displayNextTarget = "Display/input target: inspect combined capture stage '$combinedStage'."
        }
        $target = New-Target `
            -Pass $false `
            -Kind "display-input" `
            -Stage $displayStage `
            -Detail "Storage reached storage-ready; the first remaining physical blocker is display/input, not NVMe storage." `
            -NextTarget $displayNextTarget `
            -Milestone (Get-DisplayInputRoadmapTarget -Stage $displayStage)
    } else {
        $target = New-Target `
            -Pass $false `
            -Kind "combined-capture" `
            -Stage $combinedStage `
            -Detail "Storage reached storage-ready, but the combined hardware capture still failed." `
            -NextTarget (Get-PropertyText -Object $combined -Name "next_target" -Default "Inspect the combined MSI capture report.") `
            -Milestone "M134"
    }
} elseif ($guiInteractionRequired -and (-not $guiInteractionPass)) {
    $target = New-Target `
        -Pass $false `
        -Kind "display-input" `
        -Stage $guiInteractionStage `
        -Detail "Storage, display, cursor, and pointer packets are ready, but the current MSI handoff requires hwval drs-gui interaction telemetry and this transcript did not include it." `
        -NextTarget "M152 capture target: rerun hwval on an M152-or-newer Product image and include the drs-gui line with right-click, context action, scroll, and Terminal interaction proofs." `
        -Milestone "M152"
} elseif (-not $dynamicPass) {
    $target = New-Target `
        -Pass $false `
        -Kind "dynamic-handoff" `
        -Stage $dynamicStage `
        -Detail "Storage and display/input are ready; the remaining failure is the dynamic Linux handoff path, not storage hardware." `
        -NextTarget "Dynamic handoff target: inspect '$dynamicStage' in the real hardware transcript." `
        -Milestone "M83+"
} elseif ($dynamicStage -like "dynamic-runtime-*" -and $dynamicStage -ne "dynamic-runtime-exit0") {
    $target = New-Target `
        -Pass $true `
        -Kind "dynamic-runtime" `
        -Stage $dynamicStage `
        -Detail "Storage reached storage-ready and source-2 dynamic handoff succeeded; the next implementation target is dynamic runtime breadth." `
        -NextTarget "Dynamic runtime target: inspect '$dynamicStage' telemetry from linux /APPS/DYNLDLIMIT." `
        -Milestone "M83+"
} else {
    $target = New-Target `
        -Pass $true `
        -Kind "storage-ready" `
        -Stage "storage-ready" `
        -Detail "M134 storage classification is clear: NVMe/GPT/FAT, /APPS, staged artifacts, display/input, and dynamic handoff all passed this transcript." `
        -NextTarget "M134 storage intake is green for this capture. Move to the next failing hardware or runtime class shown by telemetry." `
        -Milestone "M134"
}

$biosReserve = [uint64]$handoff.reserves.bios_sectors
$uefiReserve = [uint64]$handoff.reserves.uefi_bytes

$report = [PSCustomObject]@{
    tool = "classify-m134-storage-target"
    evidence_dir = $resolvedEvidenceDir
    capture_path = $resolvedCapturePath
    pass = [bool]$target.pass
    target_kind = [string]$target.target_kind
    target_stage = [string]$target.target_stage
    detail = [string]$target.detail
    next_target = [string]$target.next_target
    roadmap_target = [string]$target.roadmap_target
    handoff_exit_code = $handoffExitCode
    require_staged_dynamic_artifacts = [bool]$RequireStagedDynamicArtifacts
    require_gui_interaction_telemetry = [bool]$RequireGuiInteractionTelemetry
    storage = [PSCustomObject]@{
        pass = $storagePass
        stage = $storageStage
        next_target = $storageNextTarget
    }
    display_input = [PSCustomObject]@{
        pass = $displayPass
        stage = $displayStage
        next_target = $displayNextTarget
        gui_interaction_required = $guiInteractionRequired
        gui_interaction_pass = $guiInteractionPass
        gui_interaction_stage = $guiInteractionStage
        gui_interaction_line_found = [bool]$handoff.gui_interaction_line_found
    }
    dynamic_handoff = [PSCustomObject]@{
        pass = $dynamicPass
        stage = $dynamicStage
        source = Get-PropertyText -Object $handoff -Name "dynamic_handoff_source"
        boot_media_read = Get-PropertyText -Object $handoff -Name "dynamic_handoff_boot_media_read"
        boot_media_read_error = Get-PropertyText -Object $handoff -Name "dynamic_handoff_boot_media_read_error"
        telemetry = Get-PropertyText -Object $handoff -Name "dynamic_handoff_telemetry"
    }
    vmd_handoff = $vmdHandoff
    reserves = [PSCustomObject]@{
        bios_sectors = $biosReserve
        uefi_bytes = $uefiReserve
    }
    handoff_json = $handoffJsonPath
    combined_json = if (Test-Path $combinedJsonPath) { $combinedJsonPath } else { "" }
}

$reportJsonPath = Join-Path $OutputDir "m134-storage-target.json"
$reportTextPath = Join-Path $OutputDir "m134-storage-target.txt"
$reportMarkdownPath = Join-Path $OutputDir "m134-storage-target.md"
$vmdHandoffKind = ""
$vmdHandoffStage = ""
$vmdRegisterCandidate = ""
$vmdRegisterStatus = ""
$vmdDriverPlanState = ""
$vmdDriverPlanToken = ""
$vmdDriverBindState = ""
$vmdDriverBindToken = ""
$vmdDriverBindCount = ""
$vmdCandidateSource = ""
$vmdCandidateDeferred = ""
if ($null -ne $vmdHandoff) {
    $vmdHandoffKind = [string]$vmdHandoff.kind
    $vmdHandoffStage = [string]$vmdHandoff.stage
    $vmdRegisterCandidate = [string]$vmdHandoff.nested_register_candidate
    $vmdRegisterStatus = [string]$vmdHandoff.nested_register_status
    $vmdDriverPlanState = [string]$vmdHandoff.nested_driver_plan_state
    $vmdDriverPlanToken = [string]$vmdHandoff.nested_driver_plan_token
    $vmdDriverBindState = [string]$vmdHandoff.nested_driver_bind_state
    $vmdDriverBindToken = [string]$vmdHandoff.nested_driver_bind_token
    $vmdDriverBindCount = [string]$vmdHandoff.nested_driver_bind_count
    $vmdCandidateSource = [string]$vmdHandoff.nvme_candidate_source
    $vmdCandidateDeferred = [string]$vmdHandoff.nvme_candidate_deferred
}

$report | ConvertTo-Json -Depth 8 | Set-Content -Path $reportJsonPath -Encoding Ascii

@(
    "m134-storage-target: $($report.target_kind)-$($report.target_stage)",
    "pass: $($report.pass)",
    "target-kind: $($report.target_kind)",
    "target-stage: $($report.target_stage)",
    "roadmap-target: $($report.roadmap_target)",
    "detail: $($report.detail)",
    "next-target: $($report.next_target)",
    "storage-pass: $($report.storage.pass)",
    "storage-stage: $($report.storage.stage)",
    "display-input-pass: $($report.display_input.pass)",
    "display-input-stage: $($report.display_input.stage)",
    "gui-interaction-required: $($report.display_input.gui_interaction_required)",
    "gui-interaction-pass: $($report.display_input.gui_interaction_pass)",
    "gui-interaction-stage: $($report.display_input.gui_interaction_stage)",
    "dynamic-handoff-pass: $($report.dynamic_handoff.pass)",
    "dynamic-handoff-stage: $($report.dynamic_handoff.stage)",
    "vmd-handoff-kind: $vmdHandoffKind",
    "vmd-handoff-stage: $vmdHandoffStage",
    "vmd-handoff-driver-plan-state: $vmdDriverPlanState",
    "vmd-handoff-driver-plan-token: $vmdDriverPlanToken",
    "vmd-handoff-driver-bind-state: $vmdDriverBindState",
    "vmd-handoff-driver-bind-token: $vmdDriverBindToken",
    "vmd-handoff-driver-bind-count: $vmdDriverBindCount",
    "bios-sector-reserve: $biosReserve",
    "uefi-byte-reserve: $uefiReserve",
    "output-json: $reportJsonPath"
) | Set-Content -Path $reportTextPath -Encoding Ascii

@(
    "# M134 Storage Target Classification",
    "",
    "- Pass: $($report.pass)",
    "- Target kind: $($report.target_kind)",
    "- Target stage: $($report.target_stage)",
    "- Roadmap target: $($report.roadmap_target)",
    "- Detail: $($report.detail)",
    "- Next target: $($report.next_target)",
    "- BIOS reserve: $biosReserve sectors",
    "- UEFI reserve: $uefiReserve bytes",
    "",
    "## Storage",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| Pass | $($report.storage.pass) |",
    "| Stage | $($report.storage.stage) |",
    "| Next target | $($report.storage.next_target) |",
    "",
    "## Display/Input",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| Pass | $($report.display_input.pass) |",
    "| Stage | $($report.display_input.stage) |",
    "| Next target | $($report.display_input.next_target) |",
    "| GUI interaction required | $($report.display_input.gui_interaction_required) |",
    "| GUI interaction pass | $($report.display_input.gui_interaction_pass) |",
    "| GUI interaction stage | $($report.display_input.gui_interaction_stage) |",
    "",
    "## Dynamic Handoff",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| Pass | $($report.dynamic_handoff.pass) |",
    "| Stage | $($report.dynamic_handoff.stage) |",
    "| Source | $($report.dynamic_handoff.source) |",
    "| Boot-media read | $($report.dynamic_handoff.boot_media_read) |",
    "| Boot-media read error | $($report.dynamic_handoff.boot_media_read_error) |",
    "",
    "## VMD/NVMe Handoff",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| Kind | $vmdHandoffKind |",
    "| Stage | $vmdHandoffStage |",
    "| Register candidate | $vmdRegisterCandidate |",
    "| Register status | $vmdRegisterStatus |",
    "| Driver plan state | $vmdDriverPlanState |",
    "| Driver plan token | $vmdDriverPlanToken |",
    "| Driver bind state | $vmdDriverBindState |",
    "| Driver bind token | $vmdDriverBindToken |",
    "| Driver bind count | $vmdDriverBindCount |",
    "| Candidate source | $vmdCandidateSource |",
    "| Candidate deferred | $vmdCandidateDeferred |"
) | Set-Content -Path $reportMarkdownPath -Encoding Ascii

Write-Host "m134-storage-target: $($report.target_kind)-$($report.target_stage)"
Write-Host "  pass: $($report.pass)"
Write-Host "  target kind: $($report.target_kind)"
Write-Host "  target stage: $($report.target_stage)"
Write-Host "  roadmap target: $($report.roadmap_target)"
Write-Host "  next target: $($report.next_target)"
Write-Host "  bios reserve: $biosReserve sectors"
Write-Host "  uefi reserve: $uefiReserve bytes"
Write-Host "  output: $reportJsonPath"

if (-not $report.pass) {
    exit 2
}
