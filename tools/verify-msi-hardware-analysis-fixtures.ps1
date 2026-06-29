param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m119-msi-hardware-analysis-fixtures"
}

$evidenceDir = Join-Path $OutputDir "evidence"
$captureDir = Join-Path $OutputDir "captures"
$analysisDir = Join-Path $OutputDir "analysis"
New-Item -ItemType Directory -Force -Path $evidenceDir | Out-Null
New-Item -ItemType Directory -Force -Path $captureDir | Out-Null
New-Item -ItemType Directory -Force -Path $analysisDir | Out-Null

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
    $isoPath = Join-Path $evidenceDir "limitlessos-x86_64-m119-fixture.iso"
    $uefiPath = Join-Path $evidenceDir "limitlessos-x86_64-m119-fixture-uefi.img"
    $appPath = Join-Path $evidenceDir "DYNLDLIMIT"
    $interpPath = Join-Path $evidenceDir "LDLIMIT"

    Write-BinaryFixture -Path $isoPath -Bytes 4096 -Seed 0x11
    Write-BinaryFixture -Path $uefiPath -Bytes 2048 -Seed 0x22
    Write-BinaryFixture -Path $appPath -Bytes 15680 -Seed 0x33
    Write-BinaryFixture -Path $interpPath -Bytes 16704 -Seed 0x44

    $appSha = Get-Sha256 -Path $appPath
    $interpSha = Get-Sha256 -Path $interpPath

    $manifest = [PSCustomObject]@{
        milestone = "M119-fixture"
        purpose = "MSI hardware analysis fixture evidence bundle"
        generated_utc = "2026-06-18T00:00:00Z"
        git_commit = "fixture"
        iso = [PSCustomObject]@{
            path = Split-Path -Leaf $isoPath
            bytes = (Get-Item $isoPath).Length
            sha256 = Get-Sha256 -Path $isoPath
        }
        uefi_image = [PSCustomObject]@{
            path = Split-Path -Leaf $uefiPath
            bytes = (Get-Item $uefiPath).Length
            sha256 = Get-Sha256 -Path $uefiPath
        }
        dynamic_app = [PSCustomObject]@{
            path = "/APPS/DYNLDLIMIT"
            evidence_file = Split-Path -Leaf $appPath
            source = "fixture"
            bytes = (Get-Item $appPath).Length
            sha256 = $appSha
        }
        dynamic_interpreter = [PSCustomObject]@{
            path = "/APPS/LDLIMIT"
            evidence_file = Split-Path -Leaf $interpPath
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
            analyzer = "tools\\analyze-hardware-storage-capture.ps1 -RequireStagedDynamicArtifacts"
            verifier = "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts"
            required_pass_stage = "storage-ready"
        }
    }

    $manifest | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $evidenceDir "hardware-storage-evidence-manifest.json") -Encoding Ascii
    @(
        "hardware-storage-evidence: fixture",
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
    @(
        "M119 fixture evidence bundle.",
        "This bundle is synthetic and exists only for host-side analyzer regression tests."
    ) | Set-Content -Path (Join-Path $evidenceDir "README-HARDWARE-STORAGE.txt") -Encoding Ascii
}

$storageOrder = @(
    "storage-triage",
    "nvme-found",
    "nvme-ready",
    "nvme-identify",
    "ioq",
    "read-issued",
    "read-completed",
    "read-status",
    "gpt-signature",
    "gpt-partitions",
    "fat32-start",
    "fat32-sectors",
    "gpt-vbr",
    "fat-bpb",
    "fat-located",
    "fat-unavailable",
    "fat-error",
    "rw-cap",
    "rw-delegated",
    "rw-error",
    "apps-stat",
    "apps-type",
    "apps-dirent",
    "apps-dir-result",
    "busybox-stat",
    "busybox-bytes",
    "dynldlimit-stat",
    "dynldlimit-bytes",
    "ldlimit-stat",
    "ldlimit-bytes",
    "boot-staged",
    "boot-app-bytes",
    "boot-interp-bytes",
    "boot-status",
    "stage-expected",
    "dynldlimit-expected",
    "ldlimit-expected",
    "dynldlimit-match",
    "ldlimit-match",
    "stage-match",
    "token"
)

$baseStorage = @{
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

$displayOrder = @(
    "display-readability",
    "available",
    "width",
    "height",
    "pitch",
    "stride-ok",
    "bounds-ok",
    "scale",
    "viewport-x",
    "viewport-y",
    "viewport-w",
    "viewport-h",
    "columns",
    "rows",
    "fit",
    "readable",
    "clip",
    "cursor-visible",
    "cursor-draws",
    "direct-cursor-draws",
    "token"
)

$uiOrder = @(
    "ui-polish",
    "compositor-active",
    "compositor-direct",
    "font",
    "wm",
    "desktop",
    "taskbar",
    "launcher",
    "windows",
    "cursor-visible",
    "token"
)

$cursorOrder = @(
    "cursor-path",
    "surface-ready",
    "format-supported",
    "compositor-active",
    "compositor-direct",
    "visible",
    "draws",
    "direct-draws",
    "x",
    "y",
    "buttons",
    "in-bounds",
    "rect-w",
    "rect-h",
    "saved",
    "drawn",
    "token"
)

$guiOrder = @(
    "drs-gui-interactive",
    "drs-gui-click-hittest",
    "drs-gui-launcher-opened",
    "drs-gui-terminal-opened",
    "drs-gui-drag-completed",
    "drs-gui-keyboard-routed",
    "drs-gui-close-completed",
    "drs-gui-taskbar-focus",
    "drs-gui-fileman-opened",
    "drs-gui-settings-opened",
    "drs-gui-installer-opened",
    "drs-gui-right-click",
    "drs-gui-context-action",
    "wm-resize",
    "wm-minimize",
    "wm-restore",
    "wm-zorder",
    "drs-gui-scroll",
    "terminal-actions",
    "terminal-scroll",
    "terminal-scroll-offset",
    "terminal-selection",
    "terminal-copy",
    "terminal-copied-bytes",
    "terminal-cursor",
    "fileman-actions",
    "fileman-refresh",
    "fileman-write",
    "fileman-mkdir",
    "fileman-edit",
    "fileman-edit-commit",
    "drs-gui-unfocused-key-denied",
    "drs-gui-no-ambient-input",
    "drs-gui-no-ambient-display",
    "drs-gui-no-ambient-fs",
    "target-window",
    "key-target-window",
    "unfocused-key-denials",
    "input-token",
    "display-token",
    "fs-token"
)

$baseDisplay = @{
    "display-readability" = "1"
    "available" = "1"
    "width" = "1280"
    "height" = "800"
    "pitch" = "1280"
    "stride-ok" = "1"
    "bounds-ok" = "1"
    "scale" = "2"
    "viewport-x" = "40"
    "viewport-y" = "92"
    "viewport-w" = "904"
    "viewport-h" = "516"
    "columns" = "75"
    "rows" = "28"
    "fit" = "1"
    "readable" = "1"
    "clip" = "0"
    "cursor-visible" = "1"
    "cursor-draws" = "205"
    "direct-cursor-draws" = "207"
    "token" = "0xF8C98059"
}

$baseUi = @{
    "ui-polish" = "1"
    "compositor-active" = "1"
    "compositor-direct" = "1"
    "font" = "1"
    "wm" = "1"
    "desktop" = "1"
    "taskbar" = "1"
    "launcher" = "1"
    "windows" = "3"
    "cursor-visible" = "1"
    "token" = "0xCB1B1C83"
}

$baseCursor = @{
    "cursor-path" = "1"
    "surface-ready" = "1"
    "format-supported" = "1"
    "compositor-active" = "1"
    "compositor-direct" = "1"
    "visible" = "1"
    "draws" = "205"
    "direct-draws" = "207"
    "x" = "640"
    "y" = "400"
    "buttons" = "0"
    "in-bounds" = "1"
    "rect-w" = "12"
    "rect-h" = "20"
    "saved" = "1"
    "drawn" = "1"
    "token" = "0xA5197C42"
}

$baseGui = @{
    "drs-gui-interactive" = "1"
    "drs-gui-click-hittest" = "1"
    "drs-gui-launcher-opened" = "1"
    "drs-gui-terminal-opened" = "1"
    "drs-gui-drag-completed" = "1"
    "drs-gui-keyboard-routed" = "1"
    "drs-gui-close-completed" = "1"
    "drs-gui-taskbar-focus" = "1"
    "drs-gui-fileman-opened" = "1"
    "drs-gui-settings-opened" = "1"
    "drs-gui-installer-opened" = "1"
    "drs-gui-right-click" = "1"
    "drs-gui-context-action" = "1"
    "wm-resize" = "1"
    "wm-minimize" = "1"
    "wm-restore" = "1"
    "wm-zorder" = "1"
    "drs-gui-scroll" = "2"
    "terminal-actions" = "2"
    "terminal-scroll" = "1"
    "terminal-scroll-offset" = "512"
    "terminal-selection" = "1"
    "terminal-copy" = "1"
    "terminal-copied-bytes" = "16"
    "terminal-cursor" = "1"
    "fileman-actions" = "1"
    "fileman-refresh" = "1"
    "fileman-write" = "1"
    "fileman-mkdir" = "1"
    "fileman-edit" = "1"
    "fileman-edit-commit" = "1"
    "drs-gui-unfocused-key-denied" = "0"
    "drs-gui-no-ambient-input" = "1"
    "drs-gui-no-ambient-display" = "1"
    "drs-gui-no-ambient-fs" = "1"
    "target-window" = "1"
    "key-target-window" = "1"
    "unfocused-key-denials" = "0"
    "input-token" = "0x494E5054"
    "display-token" = "0x44495350"
    "fs-token" = "0x46535041"
}

$baseInput = @{
    "mouse-packets" = "2"
    "xhci-mouse-endpoint" = "1"
    "xhci-mouse-reports" = "2"
    "xhci-mouse-bytes" = "8"
    "xhci-error" = "0"
    "i2c-pointer-found" = "0"
    "i2c-pointer-reports" = "0"
    "i2c-pointer-error" = "0"
    "i2c-pointer-candidates" = "0"
    "ps2-present" = "1"
    "ps2-enabled" = "1"
}

function Copy-Hashtable
{
    param([hashtable]$Source)

    $copy = @{}
    foreach ($key in $Source.Keys) {
        $copy[$key] = $Source[$key]
    }
    return $copy
}

function New-TelemetryLine
{
    param(
        [string]$Prefix,
        [string[]]$Order,
        [hashtable]$Fields
    )

    $parts = @()
    foreach ($field in $Order) {
        $parts += ("{0} {1}" -f $field, $Fields[$field])
    }
    return "[x64] $Prefix " + ($parts -join " ")
}

function New-Fixture
{
    param(
        [string]$Name,
        [string]$ExpectedStage,
        [uint32]$ExpectedExitCode,
        [hashtable]$Storage = @{},
        [hashtable]$Display = @{},
        [hashtable]$Ui = @{},
        [hashtable]$Cursor = @{},
        [hashtable]$Gui = @{},
        [hashtable]$InputMutations = @{},
        [bool]$OmitStorage = $false,
        [bool]$OmitDisplay = $false,
        [bool]$OmitCursor = $false,
        [bool]$OmitGui = $false
    )

    return [PSCustomObject]@{
        name = $Name
        expected_stage = $ExpectedStage
        expected_exit_code = $ExpectedExitCode
        storage = $Storage
        display = $Display
        ui = $Ui
        cursor = $Cursor
        gui = $Gui
        input = $InputMutations
        omit_storage = $OmitStorage
        omit_display = $OmitDisplay
        omit_cursor = $OmitCursor
        omit_gui = $OmitGui
    }
}

New-EvidenceBundle

$fixtures = @(
    (New-Fixture -Name "all-ready" -ExpectedStage "msi-hardware-ready" -ExpectedExitCode 0),
    (New-Fixture -Name "storage-first" -ExpectedStage "storage-nvme-controller-discovery" -ExpectedExitCode 2 -Storage @{ "nvme-found" = "0" }),
    (New-Fixture -Name "display-after-storage" -ExpectedStage "display-input-pointer-moving-cursor-hidden" -ExpectedExitCode 2 -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -OmitCursor:$true -OmitGui:$true),
    (New-Fixture -Name "display-cursor-draw-not-called" -ExpectedStage "display-input-cursor-draw-not-called" -ExpectedExitCode 2 -Display @{ "cursor-visible" = "0"; "cursor-draws" = "0"; "direct-cursor-draws" = "0" } -Ui @{ "cursor-visible" = "0" } -Cursor @{ "visible" = "0"; "draws" = "0"; "direct-draws" = "0"; "saved" = "0"; "drawn" = "0" }),
    (New-Fixture -Name "display-gui-right-click-unrouted" -ExpectedStage "display-input-gui-right-click-unrouted" -ExpectedExitCode 2 -Gui @{ "drs-gui-right-click" = "0" }),
    (New-Fixture -Name "display-gui-scroll-unrouted" -ExpectedStage "display-input-gui-scroll-unrouted" -ExpectedExitCode 2 -Gui @{ "drs-gui-scroll" = "0" }),
    (New-Fixture -Name "missing-storage-priority" -ExpectedStage "storage-missing-storage-triage" -ExpectedExitCode 2 -OmitStorage:$true -OmitDisplay:$true)
)

$results = @()
$failures = @()
foreach ($fixture in $fixtures) {
    $storage = Copy-Hashtable -Source $baseStorage
    $display = Copy-Hashtable -Source $baseDisplay
    $ui = Copy-Hashtable -Source $baseUi
    $cursor = Copy-Hashtable -Source $baseCursor
    $gui = Copy-Hashtable -Source $baseGui
    $input = Copy-Hashtable -Source $baseInput
    foreach ($key in $fixture.storage.Keys) {
        $storage[$key] = $fixture.storage[$key]
    }
    foreach ($key in $fixture.display.Keys) {
        $display[$key] = $fixture.display[$key]
    }
    foreach ($key in $fixture.ui.Keys) {
        $ui[$key] = $fixture.ui[$key]
    }
    foreach ($key in $fixture.cursor.Keys) {
        $cursor[$key] = $fixture.cursor[$key]
    }
    foreach ($key in $fixture.gui.Keys) {
        $gui[$key] = $fixture.gui[$key]
    }
    foreach ($key in $fixture.input.Keys) {
        $input[$key] = $fixture.input[$key]
    }

    $capturePath = Join-Path $captureDir ($fixture.name + ".txt")
    $lines = @()
    if (-not $fixture.omit_storage) {
        $lines += New-TelemetryLine -Prefix "drs-nvme-triage" -Order $storageOrder -Fields $storage
    }
    if (-not $fixture.omit_display) {
        $lines += New-TelemetryLine -Prefix "drs-display-readability" -Order $displayOrder -Fields $display
        $lines += New-TelemetryLine -Prefix "drs-ui-polish" -Order $uiOrder -Fields $ui
        if (-not $fixture.omit_cursor) {
            $lines += New-TelemetryLine -Prefix "drs-cursor-path" -Order $cursorOrder -Fields $cursor
        }
        if (-not $fixture.omit_gui) {
            $lines += New-TelemetryLine -Prefix "drs-gui" -Order $guiOrder -Fields $gui
        }
    }
    $lines += "xhci mouse endpoint: $(if ($input["xhci-mouse-endpoint"] -eq "1") { "yes" } else { "no" })"
    $lines += "xhci mouse reports: $($input["xhci-mouse-reports"])"
    $lines += "xhci mouse bytes: $($input["xhci-mouse-bytes"])"
    $lines += "xhci error: $($input["xhci-error"])"
    $lines += "i2c pointer found: $(if ($input["i2c-pointer-found"] -eq "1") { "yes" } else { "no" })"
    $lines += "i2c pointer reports: $($input["i2c-pointer-reports"])"
    $lines += "i2c pointer error: $($input["i2c-pointer-error"])"
    $lines += "i2c pointer candidates: $($input["i2c-pointer-candidates"])"
    $lines += "mouse packets: $($input["mouse-packets"])"
    $lines += "ps2 fallback present: $(if ($input["ps2-present"] -eq "1") { "yes" } else { "no" })"
    $lines += "ps2 fallback enabled: $(if ($input["ps2-enabled"] -eq "1") { "yes" } else { "no" })"
    $lines | Set-Content -Path $capturePath -Encoding Ascii

    $fixtureOutputDir = Join-Path $analysisDir $fixture.name
    $global:LASTEXITCODE = 0
    $analyzerOutput = & (Join-Path $root "tools\analyze-msi-hardware-capture.ps1") `
        -EvidenceDir $evidenceDir `
        -CapturePath $capturePath `
        -OutputDir $fixtureOutputDir `
        -RequireStagedDynamicArtifacts 2>&1
    $exitCode = $LASTEXITCODE
    $analyzerOutput | Set-Content -Path (Join-Path $fixtureOutputDir "analyzer-console.txt") -Encoding Ascii

    $analysisPath = Join-Path $fixtureOutputDir "msi-hardware-analysis.json"
    if (-not (Test-Path $analysisPath)) {
        $failures += "$($fixture.name): analyzer did not write msi-hardware-analysis.json"
        continue
    }

    $analysis = Get-Content -Raw -Path $analysisPath | ConvertFrom-Json
    $actualStage = [string]$analysis.stage
    $pass = (($actualStage -eq $fixture.expected_stage) -and ([uint32]$exitCode -eq [uint32]$fixture.expected_exit_code))
    if (-not $pass) {
        $failures += ("{0}: expected stage {1}/exit {2}, observed stage {3}/exit {4}" -f $fixture.name, $fixture.expected_stage, $fixture.expected_exit_code, $actualStage, $exitCode)
    }

    $results += [PSCustomObject]@{
        name = $fixture.name
        expected_stage = $fixture.expected_stage
        actual_stage = $actualStage
        expected_exit_code = [uint32]$fixture.expected_exit_code
        actual_exit_code = [uint32]$exitCode
        pass = $pass
        next_target = [string]$analysis.next_target
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-msi-hardware-analysis-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = $fixtures.Count
    passed = ($results | Where-Object { $_.pass }).Count
    failed = $failures.Count
    failures = $failures
    results = $results
}

$summaryJsonPath = Join-Path $OutputDir "msi-hardware-analysis-fixtures.json"
$summaryTextPath = Join-Path $OutputDir "msi-hardware-analysis-fixtures.txt"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryJsonPath -Encoding Ascii

@(
    "msi-hardware-analysis-fixtures: $($summary.passed)/$($summary.total)",
    "failed: $($summary.failed)",
    "output-json: $summaryJsonPath"
) + ($results | ForEach-Object {
    "{0}: expected {1} observed {2} exit {3} pass {4}" -f $_.name, $_.expected_stage, $_.actual_stage, $_.actual_exit_code, $_.pass
}) | Set-Content -Path $summaryTextPath -Encoding Ascii

Write-Host "msi-hardware-analysis-fixtures: $($summary.passed)/$($summary.total)"
Write-Host "  failed: $($summary.failed)"
Write-Host "  output: $summaryJsonPath"

if ($failures.Count -ne 0) {
    foreach ($failure in $failures) {
        Write-Host "  failure: $failure"
    }
    exit 1
}
