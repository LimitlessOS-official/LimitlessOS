param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m117-hardware-display-input-fixtures"
}

$captureDir = Join-Path $OutputDir "captures"
$analysisDir = Join-Path $OutputDir "analysis"
New-Item -ItemType Directory -Force -Path $captureDir | Out-Null
New-Item -ItemType Directory -Force -Path $analysisDir | Out-Null

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
        [hashtable]$Display = @{},
        [hashtable]$Ui = @{},
        [hashtable]$Cursor = @{},
        [hashtable]$Gui = @{},
        [hashtable]$InputMutations = @{},
        [bool]$OmitDisplay = $false,
        [bool]$OmitUi = $false,
        [bool]$OmitCursor = $false,
        [bool]$OmitGui = $false
    )

    return [PSCustomObject]@{
        name = $Name
        expected_stage = $ExpectedStage
        display = $Display
        ui = $Ui
        cursor = $Cursor
        gui = $Gui
        input = $InputMutations
        omit_display = $OmitDisplay
        omit_ui = $OmitUi
        omit_cursor = $OmitCursor
        omit_gui = $OmitGui
    }
}

$fixtures = @(
    (New-Fixture -Name "missing-display-readability" -ExpectedStage "missing-display-readability" -OmitDisplay:$true),
    (New-Fixture -Name "display-unavailable" -ExpectedStage "display-unavailable" -Display @{ "available" = "0" }),
    (New-Fixture -Name "framebuffer-stride" -ExpectedStage "framebuffer-stride" -Display @{ "stride-ok" = "0" }),
    (New-Fixture -Name "framebuffer-bounds" -ExpectedStage "framebuffer-bounds" -Display @{ "bounds-ok" = "0" }),
    (New-Fixture -Name "display-fit" -ExpectedStage "display-fit" -Display @{ "fit" = "0" }),
    (New-Fixture -Name "display-readable" -ExpectedStage "display-readable" -Display @{ "readable" = "0" }),
    (New-Fixture -Name "missing-ui-polish" -ExpectedStage "missing-ui-polish" -OmitUi:$true),
    (New-Fixture -Name "compositor-inactive" -ExpectedStage "compositor-inactive" -Ui @{ "compositor-active" = "0" }),
    (New-Fixture -Name "font-unavailable" -ExpectedStage "font-unavailable" -Ui @{ "font" = "0" }),
    (New-Fixture -Name "window-manager-unavailable" -ExpectedStage "window-manager-unavailable" -Ui @{ "wm" = "0" }),
    (New-Fixture -Name "desktop-unavailable" -ExpectedStage "desktop-unavailable" -Ui @{ "desktop" = "0" }),
    (New-Fixture -Name "taskbar-unavailable" -ExpectedStage "taskbar-unavailable" -Ui @{ "taskbar" = "0" }),
    (New-Fixture -Name "launcher-unavailable" -ExpectedStage "launcher-unavailable" -Ui @{ "launcher" = "0" }),
    (New-Fixture -Name "windows-unavailable" -ExpectedStage "windows-unavailable" -Ui @{ "windows" = "0" }),
    (New-Fixture -Name "pointer-moving-cursor-hidden" -ExpectedStage "pointer-moving-cursor-hidden" -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -OmitCursor:$true),
    (New-Fixture -Name "cursor-hidden" -ExpectedStage "cursor-hidden" -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -InputMutations @{ "mouse-packets" = "0" } -OmitCursor:$true),
    (New-Fixture -Name "cursor-format-unsupported" -ExpectedStage "cursor-format-unsupported" -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -Cursor @{ "visible" = "0"; "format-supported" = "0"; "drawn" = "0" }),
    (New-Fixture -Name "cursor-surface-not-ready" -ExpectedStage "cursor-surface-not-ready" -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -Cursor @{ "visible" = "0"; "surface-ready" = "0"; "drawn" = "0" }),
    (New-Fixture -Name "cursor-draw-not-called" -ExpectedStage "cursor-draw-not-called" -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -Cursor @{ "visible" = "0"; "draws" = "0"; "direct-draws" = "0"; "drawn" = "0"; "saved" = "0" }),
    (New-Fixture -Name "cursor-out-of-bounds" -ExpectedStage "cursor-out-of-bounds" -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -Cursor @{ "visible" = "0"; "in-bounds" = "0"; "rect-w" = "0"; "rect-h" = "0"; "drawn" = "0" }),
    (New-Fixture -Name "cursor-draw-not-validated" -ExpectedStage "cursor-draw-not-validated" -Display @{ "cursor-visible" = "0" } -Ui @{ "cursor-visible" = "0" } -Cursor @{ "visible" = "0"; "drawn" = "0"; "saved" = "0" }),
    (New-Fixture -Name "i2c-pointer-reports-no-packets" -ExpectedStage "i2c-pointer-reports-no-packets" -InputMutations @{ "mouse-packets" = "0"; "i2c-pointer-found" = "1"; "i2c-pointer-reports" = "2" }),
    (New-Fixture -Name "i2c-pointer-error" -ExpectedStage "i2c-pointer-error" -InputMutations @{ "mouse-packets" = "0"; "i2c-pointer-found" = "1"; "i2c-pointer-error" = "3" }),
    (New-Fixture -Name "i2c-pointer-candidate-unbound" -ExpectedStage "i2c-pointer-candidate-unbound" -InputMutations @{ "mouse-packets" = "0"; "i2c-pointer-candidates" = "1" }),
    (New-Fixture -Name "xhci-mouse-reports-no-packets" -ExpectedStage "xhci-mouse-reports-no-packets" -InputMutations @{ "mouse-packets" = "0"; "xhci-mouse-endpoint" = "1"; "xhci-mouse-reports" = "2" }),
    (New-Fixture -Name "xhci-mouse-no-reports" -ExpectedStage "xhci-mouse-no-reports" -InputMutations @{ "mouse-packets" = "0"; "xhci-mouse-endpoint" = "1"; "xhci-mouse-reports" = "0" }),
    (New-Fixture -Name "xhci-input-error" -ExpectedStage "xhci-input-error" -InputMutations @{ "mouse-packets" = "0"; "xhci-mouse-endpoint" = "0"; "xhci-mouse-reports" = "0"; "xhci-error" = "7" }),
    (New-Fixture -Name "ps2-mouse-no-packets" -ExpectedStage "ps2-mouse-no-packets" -InputMutations @{ "mouse-packets" = "0"; "xhci-mouse-endpoint" = "0"; "xhci-mouse-reports" = "0"; "ps2-present" = "1"; "ps2-enabled" = "1" }),
    (New-Fixture -Name "ps2-mouse-disabled" -ExpectedStage "ps2-mouse-disabled" -InputMutations @{ "mouse-packets" = "0"; "xhci-mouse-endpoint" = "0"; "xhci-mouse-reports" = "0"; "ps2-present" = "1"; "ps2-enabled" = "0" }),
    (New-Fixture -Name "no-pointer-backend" -ExpectedStage "no-pointer-backend" -InputMutations @{ "mouse-packets" = "0"; "xhci-mouse-endpoint" = "0"; "xhci-mouse-reports" = "0"; "ps2-present" = "0"; "ps2-enabled" = "0" }),
    (New-Fixture -Name "display-input-ready-legacy-no-gui-line" -ExpectedStage "display-input-ready" -OmitGui:$true),
    (New-Fixture -Name "gui-interactive-unrouted" -ExpectedStage "gui-interactive-unrouted" -Gui @{ "drs-gui-interactive" = "0" }),
    (New-Fixture -Name "gui-right-click-unrouted" -ExpectedStage "gui-right-click-unrouted" -Gui @{ "drs-gui-right-click" = "0" }),
    (New-Fixture -Name "gui-context-action-missing" -ExpectedStage "gui-context-action-missing" -Gui @{ "drs-gui-context-action" = "0" }),
    (New-Fixture -Name "gui-scroll-unrouted" -ExpectedStage "gui-scroll-unrouted" -Gui @{ "drs-gui-scroll" = "0" }),
    (New-Fixture -Name "gui-terminal-scroll-missing" -ExpectedStage "gui-terminal-scroll-missing" -Gui @{ "terminal-scroll" = "0" }),
    (New-Fixture -Name "gui-terminal-selection-missing" -ExpectedStage "gui-terminal-selection-missing" -Gui @{ "terminal-selection" = "0" }),
    (New-Fixture -Name "display-input-ready" -ExpectedStage "display-input-ready")
)

$results = @()
$failures = @()
foreach ($fixture in $fixtures) {
    $display = Copy-Hashtable -Source $baseDisplay
    $ui = Copy-Hashtable -Source $baseUi
    $cursor = Copy-Hashtable -Source $baseCursor
    $gui = Copy-Hashtable -Source $baseGui
    $input = Copy-Hashtable -Source $baseInput
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
    if (-not $fixture.omit_display) {
        $lines += New-TelemetryLine -Prefix "drs-display-readability" -Order $displayOrder -Fields $display
    }
    if (-not $fixture.omit_ui) {
        $lines += New-TelemetryLine -Prefix "drs-ui-polish" -Order $uiOrder -Fields $ui
    }
    if (-not $fixture.omit_cursor) {
        $lines += New-TelemetryLine -Prefix "drs-cursor-path" -Order $cursorOrder -Fields $cursor
    }
    if (-not $fixture.omit_gui) {
        $lines += New-TelemetryLine -Prefix "drs-gui" -Order $guiOrder -Fields $gui
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
    $analyzerOutput = & (Join-Path $root "tools\analyze-hardware-display-input-capture.ps1") `
        -InputPath $capturePath `
        -OutputDir $fixtureOutputDir 2>&1
    $exitCode = $LASTEXITCODE
    $analyzerOutput | Set-Content -Path (Join-Path $fixtureOutputDir "analyzer-console.txt") -Encoding Ascii

    $analysisPath = Join-Path $fixtureOutputDir "hardware-display-input-analysis.json"
    if (-not (Test-Path $analysisPath)) {
        $failures += "$($fixture.name): analyzer did not write hardware-display-input-analysis.json"
        continue
    }

    $analysis = Get-Content -Raw -Path $analysisPath | ConvertFrom-Json
    $actualStage = [string]$analysis.stage
    $expectedExitCode = if ($fixture.expected_stage -eq "display-input-ready") { 0 } else { 2 }
    $pass = (($actualStage -eq $fixture.expected_stage) -and ($exitCode -eq $expectedExitCode))
    if (-not $pass) {
        $failures += ("{0}: expected stage {1}/exit {2}, observed stage {3}/exit {4}" -f $fixture.name, $fixture.expected_stage, $expectedExitCode, $actualStage, $exitCode)
    }

    $results += [PSCustomObject]@{
        name = $fixture.name
        expected_stage = $fixture.expected_stage
        actual_stage = $actualStage
        expected_exit_code = $expectedExitCode
        actual_exit_code = $exitCode
        pass = $pass
        next_target = [string]$analysis.next_target
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-hardware-display-input-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = $fixtures.Count
    passed = ($results | Where-Object { $_.pass }).Count
    failed = $failures.Count
    failures = $failures
    results = $results
}

$summaryJsonPath = Join-Path $OutputDir "hardware-display-input-fixtures.json"
$summaryTextPath = Join-Path $OutputDir "hardware-display-input-fixtures.txt"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryJsonPath -Encoding Ascii

@(
    "hardware-display-input-fixtures: $($summary.passed)/$($summary.total)",
    "failed: $($summary.failed)",
    "output-json: $summaryJsonPath"
) + ($results | ForEach-Object {
    "{0}: expected {1} observed {2} exit {3} pass {4}" -f $_.name, $_.expected_stage, $_.actual_stage, $_.actual_exit_code, $_.pass
}) | Set-Content -Path $summaryTextPath -Encoding Ascii

Write-Host "hardware-display-input-fixtures: $($summary.passed)/$($summary.total)"
Write-Host "  failed: $($summary.failed)"
Write-Host "  output: $summaryJsonPath"

if ($failures.Count -ne 0) {
    foreach ($failure in $failures) {
        Write-Host "  failure: $failure"
    }
    exit 1
}
