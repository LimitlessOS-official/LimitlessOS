param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m133-msi-handoff-fixtures"
}
$handoffMilestone = "M133"
$handoffStem = "m133"
$handoffIsoName = "limitlessos-x86_64-$handoffStem-handoff.iso"
$handoffUefiName = "limitlessos-x86_64-$handoffStem-handoff-uefi.img"

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
        "LimitlessOS $handoffMilestone MSI Hardware Handoff Runbook",
        "",
        "1. Write $handoffIsoName to a USB drive using your normal image writer.",
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
        "5. Back on Windows/PowerShell, generate the current MSI capture report from the repository root:"
    )
    if ($Mode -ne "missing-capture-report-runbook") {
        $lines += @(
            "",
            "   .\tools\report-msi-hardware-capture.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <capture-report-output-dir> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry"
        )
    }
    $lines += @(
        "",
        "6. If you need lower-level artifacts, run the raw classifier, handoff verifier, and combined analyzer:",
        "",
        "   .\tools\classify-m134-storage-target.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <m134-target-output-dir> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry",
        "",
        "   .\tools\verify-msi-hardware-handoff.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry",
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
        "The same capture must include the M152 GUI interaction line from hwval:",
        "",
        "   drs-gui ... drs-gui-right-click 1 ... drs-gui-context-action 1 ... drs-gui-scroll ...",
        "",
        "The same capture and generated report must include the M161/M162 NVMe Controller Snapshot from drs-nvme-triage:",
        "",
        "   nvme-probe-error nvme-regs nvme-cap-low nvme-cap-high nvme-vs nvme-cc nvme-csts nvme-dstrd-bytes nvme-doorbell-page",
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

    $isoName = Get-MutationValue -Mutations $Mutations -Name "iso.path" -Default $handoffIsoName
    $uefiName = Get-MutationValue -Mutations $Mutations -Name "uefi_image.path" -Default $handoffUefiName
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
        milestone = Get-MutationValue -Mutations $Mutations -Name "milestone" -Default $handoffMilestone
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
            capture_report = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.capture_report" -Default "tools\\report-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry"
            analyzer = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.analyzer" -Default "tools\\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts"
            storage_target_classifier = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.storage_target_classifier" -Default "tools\\classify-m134-storage-target.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry"
            storage_verifier = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.storage_verifier" -Default "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts"
            boot_media_handoff_verifier = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.boot_media_handoff_verifier" -Default "tools\\verify-boot-media-linux-handoff.ps1"
            required_storage_stage = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.required_storage_stage" -Default "storage-ready"
            required_boot_media_linux_source = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.required_boot_media_linux_source" -Default "2"
            required_gui_interaction_telemetry = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.required_gui_interaction_telemetry" -Default "1"
            required_nvme_controller_snapshot = Get-MutationValue -Mutations $Mutations -Name "expected_hwval.required_nvme_controller_snapshot" -Default "1"
            required_nvme_controller_fields = @(
                "nvme-probe-error",
                "nvme-regs",
                "nvme-cap-low",
                "nvme-cap-high",
                "nvme-vs",
                "nvme-cc",
                "nvme-csts",
                "nvme-dstrd-bytes",
                "nvme-doorbell-page"
            )
        }
    }

    $manifest | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $evidenceDir "hardware-storage-evidence-manifest.json") -Encoding Ascii
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
        "uefi-kernel-byte-reserve=788512"
    ) | Set-Content -Path (Join-Path $evidenceDir "limitlessos-x86_64.size.txt") -Encoding Ascii
    Write-Runbook -Path (Join-Path $evidenceDir "README-HARDWARE-STORAGE.txt") -Mode $RunbookMode

    return $evidenceDir
}

function Write-ReadyCapture
{
    param(
        [string]$Path,
        [string]$DynamicMode
    )

    $storageLine = "[x64] drs-nvme-triage storage-triage 1 nvme-found 1 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions 6 fat32-start 2048 fat32-sectors 8192 gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat 0 busybox-bytes 0 dynldlimit-stat 1 dynldlimit-bytes 15680 ldlimit-stat 1 ldlimit-bytes 16704 boot-staged 1 boot-app-bytes 15680 boot-interp-bytes 16704 boot-status 0 stage-expected 1 dynldlimit-expected 1 ldlimit-expected 1 dynldlimit-match 1 ldlimit-match 1 stage-match 1 token 0x75BC2409"
    $displayLine = "[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 205 direct-cursor-draws 207 token 0xF8C98059"
    $uiLine = "[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 1 token 0xCB1B1C83"
    $guiLine = "[x64] drs-gui drs-gui-interactive 1 drs-gui-click-hit 1 drs-gui-launcher-open 1 drs-gui-terminal-open 1 drs-gui-fileman-open 1 drs-gui-settings-open 1 drs-gui-installer-open 1 drs-gui-right-click 1 drs-gui-context-action 1 drs-gui-scroll 2 terminal-scroll 1 terminal-selection 2 terminal-copy 1 terminal-cursor 1 wm-resize 1 wm-minimize 1 wm-restore 1 wm-z-order 2 fileman-refresh 1 fileman-write 1 fileman-delete 1 fileman-mkdir 1 fileman-copy 1 fileman-rename 1 fileman-move 1 fileman-edit 1 settings-load 1 settings-save 1 settings-export 1 no-ambient-input 1 no-ambient-display 1 no-ambient-fs 1 target-window 1 target-region 1 focus-before 1 focus-after 2 z-before 1 z-after 2 key-target-window 1 unfocused-key-denials 1 input-token 0x494E5054 display-token 0x44495350 fs-token 0x46535041"
    $lines = @(
        $storageLine,
        $displayLine,
        $uiLine,
        $guiLine,
        "xhci mouse endpoint: yes",
        "xhci mouse reports: 2",
        "xhci mouse bytes: 8",
        "xhci error: 0",
        "i2c pointer found: no",
        "i2c pointer reports: 0",
        "i2c pointer error: 0",
        "i2c pointer candidates: 0",
        "mouse packets: 2",
        "ps2 fallback present: yes",
        "ps2 fallback enabled: yes",
        "[x64] $ linux /APPS/DYNLDLIMIT"
    )

    switch ($DynamicMode) {
        "source2-runtime-fail" {
            $lines += "linux: using UEFI boot-media staged file"
            $lines += "[x64] drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage static code 8 boot-media-read 1 boot-media-read-error 0 boot-media-read-bytes 15680 boot-media-read-capacity 4194304"
        }
        "source2-exit0" {
            $lines += "linux: using UEFI boot-media staged file"
            $lines += "[x64] drs-realbin path /APPS/DYNLDLIMIT provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-transfer-started 1 console-bytes 15 exit 0 cleanup 1 page-faults 0"
        }
        "nvme-unavailable" {
            $lines += "linux: NVMe FAT unavailable"
            $lines += "[x64] drs-realbin-unavailable bios 0 nvme 0"
        }
        "wrong-source" {
            $lines += "[x64] drs-realbin path /APPS/DYNLDLIMIT provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 exit 0 cleanup 1"
        }
        default {
            throw "Unknown dynamic capture mode: $DynamicMode"
        }
    }

    $lines | Set-Content -Path $Path -Encoding Ascii
}

$fixtures = @(
    [PSCustomObject]@{
        name = "valid-m133"
        expect_success = $true
        mutations = @{}
        runbook_mode = "valid"
        expected_error = ""
    },
    [PSCustomObject]@{
        name = "stale-milestone"
        expect_success = $false
        mutations = @{ "milestone" = "M121" }
        runbook_mode = "valid"
        expected_error = "manifest milestone must be M133"
    },
    [PSCustomObject]@{
        name = "stale-analyzer"
        expect_success = $false
        mutations = @{ "expected_hwval.analyzer" = "tools\\analyze-hardware-storage-capture.ps1 -RequireStagedDynamicArtifacts" }
        runbook_mode = "valid"
        expected_error = "manifest analyzer mismatch"
    },
    [PSCustomObject]@{
        name = "missing-capture-report"
        expect_success = $false
        mutations = @{ "expected_hwval.capture_report" = "" }
        runbook_mode = "valid"
        expected_error = "manifest capture report mismatch"
    },
    [PSCustomObject]@{
        name = "missing-capture-report-runbook"
        expect_success = $false
        mutations = @{}
        runbook_mode = "missing-capture-report-runbook"
        expected_error = "runbook does not use the M155 MSI capture report"
    },
    [PSCustomObject]@{
        name = "missing-storage-target-classifier"
        expect_success = $false
        mutations = @{ "expected_hwval.storage_target_classifier" = "" }
        runbook_mode = "valid"
        expected_error = "manifest storage target classifier mismatch"
    },
    [PSCustomObject]@{
        name = "missing-gui-requirement"
        expect_success = $false
        mutations = @{ "expected_hwval.required_gui_interaction_telemetry" = "0" }
        runbook_mode = "valid"
        expected_error = "required GUI interaction telemetry mismatch"
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
        mutations = @{ "iso.path" = "limitlessos-x86_64-m121-handoff.iso" }
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
            -RequireStagedDynamicArtifacts `
            -RequireGuiInteractionTelemetry 2>&1
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

$captureFixtures = @(
    [PSCustomObject]@{
        name = "capture-source2-runtime-fail"
        dynamic_mode = "source2-runtime-fail"
        expected_exit_code = 0
        expected_stage = "dynamic-runtime-static"
        expected_dynamic_pass = $true
    },
    [PSCustomObject]@{
        name = "capture-nvme-unavailable"
        dynamic_mode = "nvme-unavailable"
        expected_exit_code = 2
        expected_stage = "dynamic-handoff-nvme-unavailable"
        expected_dynamic_pass = $false
    },
    [PSCustomObject]@{
        name = "capture-wrong-source"
        dynamic_mode = "wrong-source"
        expected_exit_code = 2
        expected_stage = "dynamic-handoff-wrong-source"
        expected_dynamic_pass = $false
    },
    [PSCustomObject]@{
        name = "capture-source2-exit0"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 0
        expected_stage = "dynamic-runtime-exit0"
        expected_dynamic_pass = $true
    },
    [PSCustomObject]@{
        name = "capture-missing-gui"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_stage = "dynamic-runtime-exit0"
        expected_dynamic_pass = $true
        remove_gui = $true
    }
)

foreach ($fixture in $captureFixtures) {
    $evidenceDir = New-EvidenceBundle -Name $fixture.name -Mutations @{} -RunbookMode "valid"
    $fixtureOutputDir = Join-Path $resultRoot $fixture.name
    New-Item -ItemType Directory -Force -Path $fixtureOutputDir | Out-Null

    $capturePath = Join-Path $fixtureOutputDir "capture.txt"
    Write-ReadyCapture -Path $capturePath -DynamicMode $fixture.dynamic_mode
    if (($fixture.PSObject.Properties["remove_gui"] -ne $null) -and [bool]$fixture.remove_gui) {
        $withoutGui = @(Get-Content -Path $capturePath | Where-Object { $_ -notmatch 'drs-gui' })
        $withoutGui | Set-Content -Path $capturePath -Encoding Ascii
    }

    $consoleText = ""
    $exitCode = 0
    try {
        $global:LASTEXITCODE = 0
        $console = & (Join-Path $root "tools\verify-msi-hardware-handoff.ps1") `
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

    $consoleText | Set-Content -Path (Join-Path $fixtureOutputDir "verifier-console.txt") -Encoding Ascii
    $verificationPath = Join-Path $fixtureOutputDir "msi-hardware-handoff-verification.json"
    $actualStage = ""
    $actualDynamicPass = $false
    if (Test-Path $verificationPath) {
        $verification = Get-Content -Raw -Path $verificationPath | ConvertFrom-Json
        $actualStage = [string]$verification.dynamic_handoff_stage
        $actualDynamicPass = [bool]$verification.dynamic_handoff_pass
        if (($fixture.PSObject.Properties["remove_gui"] -ne $null) -and [bool]$fixture.remove_gui) {
            $actualDynamicPass = ($actualDynamicPass -and (-not [bool]$verification.gui_interaction_pass))
        }
    }

    $passed = (([uint32]$exitCode -eq [uint32]$fixture.expected_exit_code) -and
        ($actualStage -eq [string]$fixture.expected_stage) -and
        ($actualDynamicPass -eq [bool]$fixture.expected_dynamic_pass))
    if (-not $passed) {
        $failures += ("{0}: expected exit/stage/pass {1}/{2}/{3}, observed {4}/{5}/{6}" -f $fixture.name, $fixture.expected_exit_code, $fixture.expected_stage, $fixture.expected_dynamic_pass, $exitCode, $actualStage, $actualDynamicPass)
    }

    $results += [PSCustomObject]@{
        name = $fixture.name
        expect_success = ([uint32]$fixture.expected_exit_code -eq 0)
        success = ([uint32]$exitCode -eq 0)
        expected_error = [string]$fixture.expected_stage
        error = ""
        pass = $passed
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-msi-hardware-handoff-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = $results.Count
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
