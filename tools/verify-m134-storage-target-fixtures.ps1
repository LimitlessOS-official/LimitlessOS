param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m134-storage-target-fixtures"
}

$fixtureRoot = Join-Path $OutputDir "evidence"
$captureRoot = Join-Path $OutputDir "captures"
$resultRoot = Join-Path $OutputDir "results"
New-Item -ItemType Directory -Force -Path $fixtureRoot | Out-Null
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$handoffMilestone = "M133"
$handoffStem = "m133"
$handoffIsoName = "limitlessos-x86_64-$handoffStem-handoff.iso"
$handoffUefiName = "limitlessos-x86_64-$handoffStem-handoff-uefi.img"

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

function New-EvidenceBundle
{
    param([string]$Name)

    $evidenceDir = Join-Path $fixtureRoot $Name
    New-Item -ItemType Directory -Force -Path $evidenceDir | Out-Null

    $isoPath = Join-Path $evidenceDir $handoffIsoName
    $uefiPath = Join-Path $evidenceDir $handoffUefiName
    $appPath = Join-Path $evidenceDir "DYNLDLIMIT"
    $interpPath = Join-Path $evidenceDir "LDLIMIT"

    Write-BinaryFixture -Path $isoPath -Bytes 4096 -Seed 0x21
    Write-BinaryFixture -Path $uefiPath -Bytes 2048 -Seed 0x32
    Write-BinaryFixture -Path $appPath -Bytes 15680 -Seed 0x43
    Write-BinaryFixture -Path $interpPath -Bytes 16704 -Seed 0x54

    $appSha = Get-Sha256 -Path $appPath
    $interpSha = Get-Sha256 -Path $interpPath

    [PSCustomObject]@{
        milestone = $handoffMilestone
        purpose = "MSI hardware handoff evidence bundle"
        generated_utc = "2026-06-28T00:00:00Z"
        git_commit = "fixture"
        iso = [PSCustomObject]@{
            path = $handoffIsoName
            bytes = (Get-Item $isoPath).Length
            sha256 = Get-Sha256 -Path $isoPath
        }
        uefi_image = [PSCustomObject]@{
            path = $handoffUefiName
            bytes = (Get-Item $uefiPath).Length
            sha256 = Get-Sha256 -Path $uefiPath
        }
        dynamic_app = [PSCustomObject]@{
            path = "/APPS/DYNLDLIMIT"
            evidence_file = "DYNLDLIMIT"
            source = "fixture"
            bytes = (Get-Item $appPath).Length
            sha256 = $appSha
        }
        dynamic_interpreter = [PSCustomObject]@{
            path = "/APPS/LDLIMIT"
            evidence_file = "LDLIMIT"
            source = "fixture"
            bytes = (Get-Item $interpPath).Length
            sha256 = $interpSha
        }
        reserves = [PSCustomObject]@{
            bios_sectors = 101
            uefi_bytes = 749408
        }
        expected_hwval = [PSCustomObject]@{
            command = "hwval"
            required_line = "drs-nvme-triage"
            analyzer = "tools\\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts"
            storage_verifier = "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts"
            boot_media_handoff_verifier = "tools\\verify-boot-media-linux-handoff.ps1"
            required_storage_stage = "storage-ready"
            required_boot_media_linux_source = "2"
        }
    } | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $evidenceDir "hardware-storage-evidence-manifest.json") -Encoding Ascii

    @(
        "LimitlessOS $handoffMilestone MSI hardware handoff evidence bundle",
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
        "uefi-kernel-byte-reserve=749408"
    ) | Set-Content -Path (Join-Path $evidenceDir "limitlessos-x86_64.size.txt") -Encoding Ascii

    @(
        "LimitlessOS $handoffMilestone MSI Hardware Handoff Runbook",
        "",
        "Run these commands on the physical laptop:",
        "",
        "hwval",
        "linux /APPS/DYNLDLIMIT",
        "",
        "Analyze with:",
        "",
        ".\tools\analyze-msi-hardware-capture.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <analysis-output-dir> -RequireStagedDynamicArtifacts",
        "",
        "Expected handoff signal:",
        "",
        "linux: using UEFI boot-media staged file",
        "drs-realbin ... source 2 ... boot-media-read 1",
        "",
        ".\tools\verify-boot-media-linux-handoff.ps1"
    ) | Set-Content -Path (Join-Path $evidenceDir "README-HARDWARE-STORAGE.txt") -Encoding Ascii

    return $evidenceDir
}

function New-StorageLine
{
    param([hashtable]$Mutations = @{})

    $fields = [ordered]@{
        "storage-triage" = "1"
        "nvme-found" = "1"
        "nvme-ready" = "1"
        "nvme-identify" = "1"
        "ioq" = "1"
        "read-issued" = "1"
        "read-completed" = "1"
        "read-status" = "0"
        "gpt-signature" = "1"
        "gpt-partitions" = "6"
        "fat32-start" = "2048"
        "fat32-sectors" = "8192"
        "gpt-vbr" = "1"
        "fat-bpb" = "1"
        "fat-located" = "1"
        "fat-unavailable" = "0"
        "fat-error" = "0"
        "rw-cap" = "1"
        "rw-delegated" = "1"
        "rw-error" = "0"
        "apps-stat" = "1"
        "apps-type" = "2"
        "apps-dirent" = "1"
        "apps-dir-result" = "1"
        "busybox-stat" = "0"
        "busybox-bytes" = "0"
        "dynldlimit-stat" = "1"
        "dynldlimit-bytes" = "15680"
        "ldlimit-stat" = "1"
        "ldlimit-bytes" = "16704"
        "boot-staged" = "1"
        "boot-app-bytes" = "15680"
        "boot-interp-bytes" = "16704"
        "boot-status" = "0"
        "stage-expected" = "1"
        "dynldlimit-expected" = "1"
        "ldlimit-expected" = "1"
        "dynldlimit-match" = "1"
        "ldlimit-match" = "1"
        "stage-match" = "1"
        "token" = "0x75BC2409"
    }
    foreach ($key in $Mutations.Keys) {
        $fields[$key] = [string]$Mutations[$key]
    }

    $parts = @()
    foreach ($key in $fields.Keys) {
        $parts += ("{0} {1}" -f $key, $fields[$key])
    }
    return "[x64] drs-nvme-triage " + ($parts -join " ")
}

function Write-Capture
{
    param(
        [string]$Path,
        [hashtable]$StorageMutations = @{},
        [string]$DisplayMode = "ready",
        [string]$DynamicMode = "source2-exit0"
    )

    $lines = @()
    $lines += New-StorageLine -Mutations $StorageMutations

    if ($DisplayMode -eq "ready") {
        $lines += "[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 205 direct-cursor-draws 207 token 0xF8C98059"
        $lines += "[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 1 token 0xCB1B1C83"
        $lines += "xhci mouse endpoint: yes"
        $lines += "xhci mouse reports: 2"
        $lines += "xhci mouse bytes: 8"
        $lines += "xhci error: 0"
        $lines += "i2c pointer found: no"
        $lines += "i2c pointer reports: 0"
        $lines += "i2c pointer error: 0"
        $lines += "i2c pointer candidates: 0"
        $lines += "mouse packets: 2"
        $lines += "ps2 fallback present: yes"
        $lines += "ps2 fallback enabled: yes"
    } elseif ($DisplayMode -eq "cursor-hidden") {
        $lines += "[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 0 cursor-draws 0 direct-cursor-draws 0 token 0xF8C98059"
        $lines += "[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 0 token 0xCB1B1C83"
        $lines += "xhci mouse endpoint: yes"
        $lines += "xhci mouse reports: 2"
        $lines += "xhci mouse bytes: 8"
        $lines += "xhci error: 0"
        $lines += "i2c pointer found: no"
        $lines += "i2c pointer reports: 0"
        $lines += "i2c pointer error: 0"
        $lines += "i2c pointer candidates: 0"
        $lines += "mouse packets: 2"
        $lines += "ps2 fallback present: yes"
        $lines += "ps2 fallback enabled: yes"
    } else {
        throw "Unknown display mode: $DisplayMode"
    }

    $lines += "[x64] $ linux /APPS/DYNLDLIMIT"
    switch ($DynamicMode) {
        "source2-exit0" {
            $lines += "linux: using UEFI boot-media staged file"
            $lines += "[x64] drs-realbin path /APPS/DYNLDLIMIT provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-transfer-started 1 console-bytes 15 exit 0 cleanup 1 page-faults 0"
        }
        "source2-runtime-fail" {
            $lines += "linux: using UEFI boot-media staged file"
            $lines += "[x64] drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage static code 8 boot-media-read 1 boot-media-read-error 0 boot-media-read-bytes 15680 boot-media-read-capacity 4194304"
        }
        "nvme-unavailable" {
            $lines += "linux: NVMe FAT unavailable"
            $lines += "[x64] drs-realbin-unavailable bios 0 nvme 0"
        }
        default {
            throw "Unknown dynamic mode: $DynamicMode"
        }
    }

    $lines | Set-Content -Path $Path -Encoding Ascii
}

$fixtures = @(
    [PSCustomObject]@{
        name = "storage-nvme-controller-discovery"
        storage_mutations = @{ "nvme-found" = "0" }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "nvme-controller-discovery"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "display-after-storage-ready"
        storage_mutations = @{}
        display_mode = "cursor-hidden"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "display-input"
        expected_stage = "pointer-moving-cursor-hidden"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "dynamic-after-storage-ready"
        storage_mutations = @{}
        display_mode = "ready"
        dynamic_mode = "nvme-unavailable"
        expected_exit_code = 2
        expected_kind = "dynamic-handoff"
        expected_stage = "dynamic-handoff-nvme-unavailable"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-ready"
        storage_mutations = @{}
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 0
        expected_kind = "storage-ready"
        expected_stage = "storage-ready"
        expected_pass = $true
    }
)

$results = @()
$failures = @()
foreach ($fixture in $fixtures) {
    $evidenceDir = New-EvidenceBundle -Name $fixture.name
    $capturePath = Join-Path $captureRoot ($fixture.name + ".txt")
    Write-Capture `
        -Path $capturePath `
        -StorageMutations $fixture.storage_mutations `
        -DisplayMode $fixture.display_mode `
        -DynamicMode $fixture.dynamic_mode

    $fixtureOutputDir = Join-Path $resultRoot $fixture.name
    New-Item -ItemType Directory -Force -Path $fixtureOutputDir | Out-Null

    $consoleText = ""
    $exitCode = 0
    try {
        $global:LASTEXITCODE = 0
        $console = & (Join-Path $root "tools\classify-m134-storage-target.ps1") `
            -EvidenceDir $evidenceDir `
            -CapturePath $capturePath `
            -OutputDir $fixtureOutputDir `
            -RequireStagedDynamicArtifacts 2>&1
        $consoleText = ($console | Out-String)
        $exitCode = [int]$LASTEXITCODE
    } catch {
        $consoleText = $_.Exception.Message
        $exitCode = 99
    }

    $consoleText | Set-Content -Path (Join-Path $fixtureOutputDir "classifier-console.txt") -Encoding Ascii
    $reportPath = Join-Path $fixtureOutputDir "m134-storage-target.json"
    $actualKind = ""
    $actualStage = ""
    $actualPass = $false
    if (Test-Path $reportPath) {
        $report = Get-Content -Raw -Path $reportPath | ConvertFrom-Json
        $actualKind = [string]$report.target_kind
        $actualStage = [string]$report.target_stage
        $actualPass = [bool]$report.pass
    }

    $passed = (($exitCode -eq [int]$fixture.expected_exit_code) -and
        ($actualKind -eq [string]$fixture.expected_kind) -and
        ($actualStage -eq [string]$fixture.expected_stage) -and
        ($actualPass -eq [bool]$fixture.expected_pass))
    if (-not $passed) {
        $failures += ("{0}: expected exit/kind/stage/pass {1}/{2}/{3}/{4}, observed {5}/{6}/{7}/{8}" -f $fixture.name, $fixture.expected_exit_code, $fixture.expected_kind, $fixture.expected_stage, $fixture.expected_pass, $exitCode, $actualKind, $actualStage, $actualPass)
    }

    $results += [PSCustomObject]@{
        name = $fixture.name
        expected_exit_code = [int]$fixture.expected_exit_code
        actual_exit_code = $exitCode
        expected_kind = [string]$fixture.expected_kind
        actual_kind = $actualKind
        expected_stage = [string]$fixture.expected_stage
        actual_stage = $actualStage
        expected_pass = [bool]$fixture.expected_pass
        actual_pass = $actualPass
        pass = $passed
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-m134-storage-target-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = $results.Count
    passed = ($results | Where-Object { $_.pass }).Count
    failed = $failures.Count
    failures = $failures
    results = $results
}

$summaryJsonPath = Join-Path $OutputDir "m134-storage-target-fixtures.json"
$summaryTextPath = Join-Path $OutputDir "m134-storage-target-fixtures.txt"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryJsonPath -Encoding Ascii

@(
    "m134-storage-target-fixtures: $($summary.passed)/$($summary.total)",
    "failed: $($summary.failed)",
    "output-json: $summaryJsonPath"
) + ($results | ForEach-Object {
    "{0}: expected {1}/{2}/{3} observed {4}/{5}/{6} pass {7}" -f $_.name, $_.expected_exit_code, $_.expected_kind, $_.expected_stage, $_.actual_exit_code, $_.actual_kind, $_.actual_stage, $_.pass
}) | Set-Content -Path $summaryTextPath -Encoding Ascii

Write-Host "m134-storage-target-fixtures: $($summary.passed)/$($summary.total)"
Write-Host "  failed: $($summary.failed)"
Write-Host "  output: $summaryJsonPath"

if ($failures.Count -ne 0) {
    foreach ($failure in $failures) {
        Write-Host "  failure: $failure"
    }
    exit 1
}
