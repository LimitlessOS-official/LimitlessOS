param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m123-msi-handoff-fixtures"
}

$fixtureRoot = Join-Path $OutputDir "evidence"
$resultRoot = Join-Path $OutputDir "results"
New-Item -ItemType Directory -Force -Path $fixtureRoot | Out-Null
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

function Get-Sha256
{
    param([string]$Path)

    return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

function Write-BinaryFixture
{
    param(
        [string]$Path,
        [uint32]$Bytes,
        [byte]$Seed
    )

    $data = [byte[]]::new($Bytes)
    for ($i = 0; $i -lt $data.Length; $i++) {
        $data[$i] = [byte](($Seed + $i) -band 0xFF)
    }
    [System.IO.File]::WriteAllBytes($Path, $data)
}

function Get-MutationValue
{
    param(
        [hashtable]$Mutations,
        [string]$Name,
        [string]$Default
    )

    if ($Mutations.ContainsKey($Name)) {
        return [string]$Mutations[$Name]
    }
    return $Default
}

function Write-Runbook
{
    param(
        [string]$Path,
        [string]$Mode
    )

    $lines = @(
        "LimitlessOS M121 MSI Hardware Handoff Runbook",
        "",
        "1. Write limitlessos-x86_64-m121-handoff.iso to a USB drive using your normal image writer.",
        "2. Boot the laptop through the UEFI USB boot entry.",
        "3. At the [x64] shell, run:",
        "",
        "   hwval"
    )
    if ($Mode -ne "missing-linux-command") {
        $lines += "   linux /APPS/DYNLDLIMIT"
    }
    $lines += @(
        "",
        "4. Capture the full transcript to a text file named msi-hwval-storage.txt.",
        "5. Back on Windows/PowerShell, verify this bundle and analyze the capture from the repository root:",
        "",
        "   .\tools\analyze-msi-hardware-capture.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <analysis-output-dir> -RequireStagedDynamicArtifacts",
        "",
        "Pass means the combined analyzer reports:",
        "",
        "   msi-hardware-analysis: msi-hardware-ready",
        "   pass: True",
        "   storage-stage: storage-ready",
        "   display/input-stage: display-input-ready",
        "",
        "If storage is unavailable on the laptop, linux /APPS/DYNLDLIMIT should still prefer the UEFI boot-media staged source when this bundle was written correctly. Capture that command output too. Expected handoff signal:",
        "",
        "   linux: using UEFI boot-media staged file"
    )
    if ($Mode -ne "missing-source2-runbook") {
        $lines += "   drs-realbin ... source 2 ... boot-media-read 1"
    }
    $lines += @(
        "",
        "Before creating or using a bundle, the host-side boot-media handoff verifier can confirm source-2 shell selection in QEMU:",
        "",
        "   .\tools\verify-boot-media-linux-handoff.ps1",
        "",
        "Do not run installer writes, formatting, or NVRAM boot-entry actions during this evidence pass."
    )
    $lines | Set-Content -Path $Path -Encoding Ascii
}

function New-EvidenceBundle
{
    param(
        [string]$Name,
        [hashtable]$Mutations = @{},
        [string]$RunbookMode = "valid"
    )

    $evidenceDir = Join-Path $fixtureRoot $Name
    New-Item -ItemType Directory -Force -Path $evidenceDir | Out-Null

    $isoName = Get-MutationValue -Mutations $Mutations -Name "iso.path" -Default "limitlessos-x86_64-m121-handoff.iso"
    $uefiName = Get-MutationValue -Mutations $Mutations -Name "uefi_image.path" -Default "limitlessos-x86_64-m121-handoff-uefi.img"
    $appFile = "DYNLDLIMIT"
    $interpFile = "LDLIMIT"

    $isoPath = Join-Path $evidenceDir $isoName
    $uefiPath = Join-Path $evidenceDir $uefiName
    $appPath = Join-Path $evidenceDir $appFile
    $interpPath = Join-Path $evidenceDir $interpFile

    Write-BinaryFixture -Path $isoPath -Bytes 4096 -Seed 0x21
    Write-BinaryFixture -Path $uefiPath -Bytes 2048 -Seed 0x32
    Write-BinaryFixture -Path $appPath -Bytes 15680 -Seed 0x43
    Write-BinaryFixture -Path $interpPath -Bytes 16704 -Seed 0x54

    $appSha = Get-Sha256 -Path $appPath
    $interpSha = Get-Sha256 -Path $interpPath

    $manifest = [PSCustomObject]@{
        milestone = Get-MutationValue -Mutations $Mutations -Name "milestone" -Default "M121"
        purpose = Get-MutationValue -Mutations $Mutations -Name "purpose" -Default "MSI hardware handoff evidence bundle"
        generated_utc = "2026-06-18T00:00:00Z"
        git_commit = "fixture"
        iso = [PSCustomObject]@{
            path = $isoName
            bytes = (Get-Item $isoPath).Length
            sha256 = Get-Sha256 -Path $isoPath
        }
        uefi_image = [PSCustomObject]@{
            path = $uefiName
            bytes = (Get-Item $uefiPath).Length
            sha256 = Get-Sha256 -Path $uefiPath
        }
        dynamic_app = [PSCustomObject]@{
            path = Get-MutationValue -Mutations $Mutations -Name "dynamic_app.path" -Default "/APPS/DYNLDLIMIT"
            evidence_file = $appFile
            source = "fixture"
            bytes = (Get-Item $appPath).Length
            sha256 = $appSha
        }
        dynamic_interpreter = [PSCustomObject]@{
            path = Get-MutationValue -Mutations $Mutations -Name "dynamic_interpreter.path" -Default "/APPS/LDLIMIT"
            evidence_file = $interpFile
            source = "fixture"
            bytes = (Get-Item $interpPath).Length
            sha256 = $interpSha
        }
        reserves = [PSCustomObject]@{
            bios_sectors = 101
            uefi_bytes = 788512
        }
        expected_hwval = [PSCustomObject]@{
            command = "hwval"
            required_line = "drs-nvme-triage"
            analyzer = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.analyzer" -Default "tools\\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts"
            storage_verifier = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.storage_verifier" -Default "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts"
            boot_media_handoff_verifier = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.boot_media_handoff_verifier" -Default "tools\\verify-boot-media-linux-handoff.ps1"
            required_storage_stage = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.required_storage_stage" -Default "storage-ready"
            required_boot_media_linux_source = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.required_boot_media_linux_source" -Default "2"
        }
    }

    $manifest | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $evidenceDir "hardware-storage-evidence-manifest.json") -Encoding Ascii
    @(
        "LimitlessOS M121 MSI hardware handoff evidence bundle",
        "dynamic-app-sha256: $appSha",
        "dynamic-interpreter-sha256: $interpSha"
    ) | Set-Content -Path (Join-Path $evidenceDir "hardware-storage-evidence-manifest.txt") -Encoding Ascii
    @(
        "LimitlessOS boot manifest v1",
        "boot-linux-expected=1",
        "boot-linux-app=/APPS/DYNLDLIMIT",
        "boot-linux-app-bytes=15680",
        "boot-linux-app-sha256=$appSha",
        "boot-linux-interp=/APPS/LDLIMIT",
        "boot-linux-interp-bytes=16704",
        "boot-linux-interp-sha256=$interpSha"
    ) | Set-Content -Path (Join-Path $evidenceDir "BOOTMAN.TXT") -Encoding Ascii
    @(
        "LimitlessOS x86_64 size map",
        "bios-sector-reserve=101",
        "uefi-kernel-byte-reserve=788512"
    ) | Set-Content -Path (Join-Path $evidenceDir "limitlessos-x86_64.size.txt") -Encoding Ascii
    Write-Runbook -Path (Join-Path $evidenceDir "README-HARDWARE-STORAGE.txt") -Mode $RunbookMode

    return $evidenceDir
}

$fixtures = @(
    [PSCustomObject]@{
        name = "valid-m121"
        expect_success = $true
        mutations = @{}
        runbook_mode = "valid"
        expected_error = ""
    },
    [PSCustomObject]@{
        name = "stale-milestone"
        expect_success = $false
        mutations = @{ "milestone" = "M113" }
        runbook_mode = "valid"
        expected_error = "manifest milestone must be M121"
    },
    [PSCustomObject]@{
        name = "stale-analyzer"
        expect_success = $false
        mutations = @{ "expected_hwval.analyzer" = "tools\\analyze-hardware-storage-capture.ps1 -RequireStagedDynamicArtifacts" }
        runbook_mode = "valid"
        expected_error = "manifest analyzer mismatch"
    },
    [PSCustomObject]@{
        name = "missing-source2"
        expect_success = $false
        mutations = @{ "expected_hwval.required_boot_media_linux_source" = "1" }
        runbook_mode = "valid"
        expected_error = "required boot-media source mismatch"
    },
    [PSCustomObject]@{
        name = "stale-iso-name"
        expect_success = $false
        mutations = @{ "iso.path" = "limitlessos-x86_64-m113-staged.iso" }
        runbook_mode = "valid"
        expected_error = "ISO path mismatch"
    },
    [PSCustomObject]@{
        name = "missing-linux-command"
        expect_success = $false
        mutations = @{}
        runbook_mode = "missing-linux-command"
        expected_error = "runbook does not instruct the tester to run linux /APPS/DYNLDLIMIT"
    },
    [PSCustomObject]@{
        name = "missing-source2-runbook"
        expect_success = $false
        mutations = @{}
        runbook_mode = "missing-source2-runbook"
        expected_error = "runbook is missing the source-2 boot-media telemetry expectation"
    }
)

$results = @()
$failures = @()
foreach ($fixture in $fixtures) {
    $evidenceDir = New-EvidenceBundle -Name $fixture.name -Mutations $fixture.mutations -RunbookMode $fixture.runbook_mode
    $fixtureOutputDir = Join-Path $resultRoot $fixture.name
    New-Item -ItemType Directory -Force -Path $fixtureOutputDir | Out-Null

    $succeeded = $false
    $errorText = ""
    $consoleText = ""
    try {
        $global:LASTEXITCODE = 0
        $console = & (Join-Path $root "tools\verify-msi-hardware-handoff.ps1") `
            -EvidenceDir $evidenceDir `
            -OutputDir $fixtureOutputDir `
            -RequireStagedDynamicArtifacts 2>&1
        $consoleText = ($console | Out-String)
        $succeeded = ($LASTEXITCODE -eq 0)
    } catch {
        $errorText = $_.Exception.Message
        $consoleText = $errorText
    }

    $consoleText | Set-Content -Path (Join-Path $fixtureOutputDir "verifier-console.txt") -Encoding Ascii

    $passed = $false
    if ([bool]$fixture.expect_success) {
        $passed = $succeeded
    } else {
        $passed = ((-not $succeeded) -and ($consoleText -match [regex]::Escape([string]$fixture.expected_error)))
    }
    if (-not $passed) {
        $failures += ("{0}: expected success {1} / error '{2}', observed success {3} / output '{4}'" -f $fixture.name, $fixture.expect_success, $fixture.expected_error, $succeeded, $consoleText.Trim())
    }

    $results += [PSCustomObject]@{
        name = $fixture.name
        expect_success = [bool]$fixture.expect_success
        success = $succeeded
        expected_error = [string]$fixture.expected_error
        error = $errorText
        pass = $passed
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-msi-hardware-handoff-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = $fixtures.Count
    passed = ($results | Where-Object { $_.pass }).Count
    failed = $failures.Count
    failures = $failures
    results = $results
}

$summaryJsonPath = Join-Path $OutputDir "msi-hardware-handoff-fixtures.json"
$summaryTextPath = Join-Path $OutputDir "msi-hardware-handoff-fixtures.txt"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryJsonPath -Encoding Ascii

@(
    "msi-hardware-handoff-fixtures: $($summary.passed)/$($summary.total)",
    "failed: $($summary.failed)",
    "output-json: $summaryJsonPath"
) + ($results | ForEach-Object {
    "{0}: expected-success {1} observed-success {2} pass {3}" -f $_.name, $_.expect_success, $_.success, $_.pass
}) | Set-Content -Path $summaryTextPath -Encoding Ascii

Write-Host "msi-hardware-handoff-fixtures: $($summary.passed)/$($summary.total)"
Write-Host "  failed: $($summary.failed)"
Write-Host "  output: $summaryJsonPath"

if ($failures.Count -ne 0) {
    foreach ($failure in $failures) {
        Write-Host "  failure: $failure"
    }
    exit 1
}
