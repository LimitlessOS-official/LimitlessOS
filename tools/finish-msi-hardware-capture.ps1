param(
    [string]$EvidenceDir = "",

    [string]$CapturePath = "",

    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $root "dist\m133-msi-hardware-handoff-current"
}
if ([string]::IsNullOrWhiteSpace($CapturePath)) {
    $CapturePath = Join-Path $root "dist\msi-hwval-storage.txt"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "dist\msi-hardware-capture-report"
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
    param([string]$Path)

    Assert-PathExists -Path $Path -Message "MSI capture finish: expected report JSON missing: $Path"
    return Get-Content -Raw -Path $Path | ConvertFrom-Json
}

function Get-TextProperty
{
    param(
        [object]$Object,
        [string]$Name
    )

    if (($null -eq $Object) -or ($null -eq $Object.PSObject.Properties[$Name])) {
        return ""
    }

    return [string]$Object.PSObject.Properties[$Name].Value
}

Assert-PathExists -Path $EvidenceDir -Message "MSI capture finish: evidence directory not found: $EvidenceDir"
Assert-PathExists -Path $CapturePath -Message "MSI capture finish: capture file not found: $CapturePath"

$captureText = Get-Content -Raw -Path $CapturePath
if ([string]::IsNullOrWhiteSpace($captureText)) {
    throw "MSI capture finish: capture file is empty: $CapturePath"
}
if ($captureText -match "---- transcript begins below ----") {
    throw "MSI capture finish: capture file still contains the blank template marker. Replace it with the real MSI laptop transcript."
}

$hasHwval = ($captureText -match "(?im)^\s*hwval\s*$")
$hasLinuxDyn = ($captureText -match "(?im)linux\s+/APPS/DYNLDLIMIT")
$hasStorageLine = ($captureText -match "drs-nvme-triage")
$hasRealbinLine = (($captureText -match "drs-realbin") -or ($captureText -match "drs-realbin-fail") -or ($captureText -match "drs-realbin-unavailable"))

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$global:LASTEXITCODE = 0
& (Join-Path $root "tools\report-msi-hardware-capture.ps1") `
    -EvidenceDir $EvidenceDir `
    -CapturePath $CapturePath `
    -OutputDir $OutputDir `
    -RequireStagedDynamicArtifacts `
    -RequireGuiInteractionTelemetry
$reportExitCode = [int]$LASTEXITCODE
if (($reportExitCode -ne 0) -and ($reportExitCode -ne 2)) {
    throw "MSI capture finish: report failed unexpectedly with exit code $reportExitCode."
}

$reportJsonPath = Join-Path $OutputDir "msi-hardware-capture-report.json"
$report = Read-JsonFile -Path $reportJsonPath
$summaryPath = Join-Path $OutputDir "msi-hardware-capture-next-target.txt"

$targetKind = Get-TextProperty -Object $report -Name "target_kind"
$targetStage = Get-TextProperty -Object $report -Name "target_stage"
$roadmapTarget = Get-TextProperty -Object $report -Name "roadmap_target"
$nextTarget = Get-TextProperty -Object $report -Name "next_target"

@(
    "msi-hardware-capture-next-target",
    "report-exit-code: $reportExitCode",
    "pass: $([string]$report.pass)",
    "target-kind: $targetKind",
    "target-stage: $targetStage",
    "roadmap-target: $roadmapTarget",
    "next-target: $nextTarget",
    "capture-has-hwval-command: $hasHwval",
    "capture-has-linux-dynldlimit-command: $hasLinuxDyn",
    "capture-has-drs-nvme-triage: $hasStorageLine",
    "capture-has-drs-realbin: $hasRealbinLine",
    "report-json: $reportJsonPath",
    "report-text: $(Join-Path $OutputDir "msi-hardware-capture-report.txt")",
    "report-markdown: $(Join-Path $OutputDir "msi-hardware-capture-report.md")"
) | Set-Content -Path $summaryPath -Encoding Ascii

Write-Host "msi-hardware-capture-finish: complete"
Write-Host "  report exit: $reportExitCode"
Write-Host "  pass: $([string]$report.pass)"
Write-Host "  target kind: $targetKind"
Write-Host "  target stage: $targetStage"
Write-Host "  roadmap target: $roadmapTarget"
Write-Host "  next target: $nextTarget"
Write-Host "  capture has hwval: $hasHwval"
Write-Host "  capture has linux /APPS/DYNLDLIMIT: $hasLinuxDyn"
Write-Host "  capture has drs-nvme-triage: $hasStorageLine"
Write-Host "  capture has drs-realbin: $hasRealbinLine"
Write-Host "  summary: $summaryPath"

exit $reportExitCode
