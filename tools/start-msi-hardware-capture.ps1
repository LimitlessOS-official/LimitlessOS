param(
    [string]$EvidenceDir = "",

    [string]$CapturePath = "",

    [string]$OutputDir = "",

    [switch]$AnalyzeIfCaptureExists
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
    $OutputDir = Join-Path $root "dist\msi-hardware-capture-session"
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

function Get-JsonPropertyText
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

$resolvedEvidenceDir = (Resolve-Path $EvidenceDir).Path
$manifestPath = Join-Path $resolvedEvidenceDir "hardware-storage-evidence-manifest.json"
Assert-PathExists -Path $manifestPath -Message "MSI capture start: manifest not found: $manifestPath"

$manifest = Get-Content -Raw -Path $manifestPath | ConvertFrom-Json
$isoName = Get-JsonPropertyText -Object $manifest.iso -Name "path"
$isoPath = Join-Path $resolvedEvidenceDir $isoName
Assert-PathExists -Path $isoPath -Message "MSI capture start: handoff ISO not found: $isoPath"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$resolvedOutputDir = (Resolve-Path $OutputDir).Path
$captureParent = Split-Path -Parent $CapturePath
if (-not [string]::IsNullOrWhiteSpace($captureParent)) {
    New-Item -ItemType Directory -Force -Path $captureParent | Out-Null
}

$verifyOutputDir = Join-Path $resolvedOutputDir "handoff-verification"
$global:LASTEXITCODE = 0
$verifyOutput = & (Join-Path $root "tools\verify-msi-hardware-handoff.ps1") `
    -EvidenceDir $resolvedEvidenceDir `
    -OutputDir $verifyOutputDir `
    -RequireStagedDynamicArtifacts `
    -RequireGuiInteractionTelemetry 2>&1
$verifyExitCode = [int]$LASTEXITCODE
$verifyTranscriptPath = Join-Path $resolvedOutputDir "handoff-verification-console.txt"
$verifyOutput | Set-Content -Path $verifyTranscriptPath -Encoding Ascii
if ($verifyExitCode -ne 0) {
    throw "MSI capture start: handoff verification failed with exit code $verifyExitCode. See $verifyTranscriptPath"
}

$sessionPath = Join-Path $resolvedOutputDir "msi-hardware-capture-session.txt"
$templatePath = Join-Path $resolvedOutputDir "msi-hwval-storage.template.txt"
$reportCommand = ".\tools\report-msi-hardware-capture.ps1 -EvidenceDir .\dist\m133-msi-hardware-handoff-current -CapturePath .\dist\msi-hwval-storage.txt -OutputDir .\dist\msi-hardware-capture-report -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry"

@(
    "LimitlessOS MSI hardware capture session",
    "evidence-dir: $resolvedEvidenceDir",
    "handoff-iso: $isoPath",
    "iso-sha256: $(Get-JsonPropertyText -Object $manifest.iso -Name "sha256")",
    "uefi-image-sha256: $(Get-JsonPropertyText -Object $manifest.uefi_image -Name "sha256")",
    "dynamic-app: /APPS/DYNLDLIMIT",
    "dynamic-app-sha256: $(Get-JsonPropertyText -Object $manifest.dynamic_app -Name "sha256")",
    "dynamic-interpreter: /APPS/LDLIMIT",
    "dynamic-interpreter-sha256: $(Get-JsonPropertyText -Object $manifest.dynamic_interpreter -Name "sha256")",
    "bios-sector-reserve: $(Get-JsonPropertyText -Object $manifest.reserves -Name "bios_sectors")",
    "uefi-byte-reserve: $(Get-JsonPropertyText -Object $manifest.reserves -Name "uefi_bytes")",
    "",
    "Write this ISO to USB:",
    $isoPath,
    "",
    "Boot the MSI laptop through the UEFI USB entry, then run exactly:",
    "hwval",
    "linux /APPS/DYNLDLIMIT",
    "",
    "Save the full transcript here:",
    $CapturePath,
    "",
    "Required transcript lines:",
    "drs-display-readability",
    "drs-ui-polish",
    "drs-cursor-path",
    "drs-gui",
    "drs-nvme-triage with nvme-probe-error,nvme-regs,nvme-cap-low,nvme-cap-high,nvme-vs,nvme-cc,nvme-csts,nvme-dstrd-bytes,nvme-doorbell-page",
    "drs-realbin or drs-realbin-fail for /APPS/DYNLDLIMIT",
    "",
    "After capture, run:",
    $reportCommand,
    "",
    "A report exit code of 2 is still useful: it means the report found the next actionable target.",
    "handoff-verification-console: $verifyTranscriptPath"
) | Set-Content -Path $sessionPath -Encoding Ascii

@(
    "Paste the full MSI laptop transcript below this line.",
    "Required commands on the laptop:",
    "hwval",
    "linux /APPS/DYNLDLIMIT",
    "",
    "---- transcript begins below ----"
) | Set-Content -Path $templatePath -Encoding Ascii

$analysisExitCode = 0
$analysisRan = $false
$analysisOutputDir = Join-Path $root "dist\msi-hardware-capture-report"
if ($AnalyzeIfCaptureExists.IsPresent -and (Test-Path $CapturePath)) {
    $analysisRan = $true
    $global:LASTEXITCODE = 0
    & (Join-Path $root "tools\report-msi-hardware-capture.ps1") `
        -EvidenceDir $resolvedEvidenceDir `
        -CapturePath $CapturePath `
        -OutputDir $analysisOutputDir `
        -RequireStagedDynamicArtifacts `
        -RequireGuiInteractionTelemetry
    $analysisExitCode = [int]$LASTEXITCODE
    if (($analysisExitCode -ne 0) -and ($analysisExitCode -ne 2)) {
        throw "MSI capture start: capture report failed unexpectedly with exit code $analysisExitCode."
    }
}

Write-Host "msi-hardware-capture-session: ready"
Write-Host "  handoff iso: $isoPath"
Write-Host "  session: $sessionPath"
Write-Host "  transcript template: $templatePath"
Write-Host "  capture path: $CapturePath"
Write-Host "  report command: $reportCommand"
Write-Host "  handoff verification: passed"
if ($analysisRan) {
    Write-Host "  capture report exit: $analysisExitCode"
    Write-Host "  capture report output: $analysisOutputDir"
}
