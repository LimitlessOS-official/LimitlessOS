param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m155-msi-capture-report-fixtures"
}

$seedOutputDir = Join-Path $OutputDir "seed-target-fixtures"
$resultRoot = Join-Path $OutputDir "report-results"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

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

$global:LASTEXITCODE = 0
& (Join-Path $root "tools\verify-m134-storage-target-fixtures.ps1") -OutputDir $seedOutputDir
if ($LASTEXITCODE -ne 0) {
    throw "MSI capture report fixtures: seed M134 target fixtures failed with exit code $LASTEXITCODE."
}

$fixtures = @(
    [PSCustomObject]@{
        name = "storage-ready"
        expected_exit_code = 0
        expected_kind = "storage-ready"
        expected_stage = "storage-ready"
        expected_roadmap = "M134"
        expected_pass = $true
    },
    [PSCustomObject]@{
        name = "display-gui-telemetry-missing"
        expected_exit_code = 2
        expected_kind = "display-input"
        expected_stage = "gui-telemetry-missing"
        expected_roadmap = "M152"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "dynamic-after-storage-ready"
        expected_exit_code = 2
        expected_kind = "dynamic-handoff"
        expected_stage = "dynamic-handoff-nvme-unavailable"
        expected_roadmap = "M83+"
        expected_pass = $false
    }
)

$results = @()
$failures = @()
foreach ($fixture in $fixtures) {
    $evidenceDir = Join-Path $seedOutputDir "evidence\$($fixture.name)"
    $capturePath = Join-Path $seedOutputDir "captures\$($fixture.name).txt"
    $fixtureOutputDir = Join-Path $resultRoot $fixture.name
    Assert-PathExists -Path $evidenceDir -Message "MSI capture report fixtures: evidence missing for $($fixture.name): $evidenceDir"
    Assert-PathExists -Path $capturePath -Message "MSI capture report fixtures: capture missing for $($fixture.name): $capturePath"
    New-Item -ItemType Directory -Force -Path $fixtureOutputDir | Out-Null

    $consoleText = ""
    $exitCode = 0
    try {
        $global:LASTEXITCODE = 0
        $console = & (Join-Path $root "tools\report-msi-hardware-capture.ps1") `
            -EvidenceDir $evidenceDir `
            -CapturePath $capturePath `
            -OutputDir $fixtureOutputDir `
            -RequireStagedDynamicArtifacts `
            -RequireGuiInteractionTelemetry 2>&1
        $consoleText = ($console | Out-String)
        $exitCode = [int]$LASTEXITCODE
    } catch {
        $consoleText = $_.Exception.Message
        $exitCode = 99
    }

    $consoleText | Set-Content -Path (Join-Path $fixtureOutputDir "report-console.txt") -Encoding Ascii
    $reportPath = Join-Path $fixtureOutputDir "msi-hardware-capture-report.json"
    $actualKind = ""
    $actualStage = ""
    $actualRoadmap = ""
    $actualPass = $false
    if (Test-Path $reportPath) {
        $report = Get-Content -Raw -Path $reportPath | ConvertFrom-Json
        $actualKind = [string]$report.target_kind
        $actualStage = [string]$report.target_stage
        $actualRoadmap = [string]$report.roadmap_target
        $actualPass = [bool]$report.pass
    }

    $passed = (($exitCode -eq [int]$fixture.expected_exit_code) -and
        ($actualKind -eq [string]$fixture.expected_kind) -and
        ($actualStage -eq [string]$fixture.expected_stage) -and
        ($actualRoadmap -eq [string]$fixture.expected_roadmap) -and
        ($actualPass -eq [bool]$fixture.expected_pass))
    if (-not $passed) {
        $failures += ("{0}: expected exit/kind/stage/roadmap/pass {1}/{2}/{3}/{4}/{5}, observed {6}/{7}/{8}/{9}/{10}" -f $fixture.name, $fixture.expected_exit_code, $fixture.expected_kind, $fixture.expected_stage, $fixture.expected_roadmap, $fixture.expected_pass, $exitCode, $actualKind, $actualStage, $actualRoadmap, $actualPass)
    }

    $results += [PSCustomObject]@{
        name = [string]$fixture.name
        expected_exit_code = [int]$fixture.expected_exit_code
        actual_exit_code = $exitCode
        expected_kind = [string]$fixture.expected_kind
        actual_kind = $actualKind
        expected_stage = [string]$fixture.expected_stage
        actual_stage = $actualStage
        expected_roadmap = [string]$fixture.expected_roadmap
        actual_roadmap = $actualRoadmap
        expected_pass = [bool]$fixture.expected_pass
        actual_pass = $actualPass
        pass = $passed
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-msi-hardware-capture-report-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = @($results).Count
    passed = @($results | Where-Object { $_.pass }).Count
    failed = @($failures).Count
    failures = $failures
    results = $results
}

$summaryJsonPath = Join-Path $OutputDir "msi-hardware-capture-report-fixtures.json"
$summaryTextPath = Join-Path $OutputDir "msi-hardware-capture-report-fixtures.txt"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryJsonPath -Encoding Ascii

@(
    "msi-hardware-capture-report-fixtures: $($summary.passed)/$($summary.total)",
    "failed: $($summary.failed)",
    "output-json: $summaryJsonPath"
) + ($results | ForEach-Object {
    "{0}: expected {1}/{2}/{3}/{4} observed {5}/{6}/{7}/{8} pass {9}" -f $_.name, $_.expected_exit_code, $_.expected_kind, $_.expected_stage, $_.expected_roadmap, $_.actual_exit_code, $_.actual_kind, $_.actual_stage, $_.actual_roadmap, $_.pass
}) | Set-Content -Path $summaryTextPath -Encoding Ascii

Write-Host "msi-hardware-capture-report-fixtures: $($summary.passed)/$($summary.total)"
Write-Host "  failed: $($summary.failed)"
Write-Host "  output: $summaryJsonPath"

if (@($failures).Count -ne 0) {
    foreach ($failure in $failures) {
        Write-Host "  failure: $failure"
    }
    exit 1
}
