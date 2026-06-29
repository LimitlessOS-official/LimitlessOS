param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "dist\m117-hardware-display-input-analysis-$stamp"
}

function Assert-FileExists
{
    param(
        [string]$Path,
        [string]$Message
    )

    if (-not (Test-Path $Path)) {
        throw $Message
    }
}

function Convert-TokenValue
{
    param([string]$Value)

    if ($Value -match '^0x([0-9A-Fa-f]+)$') {
        return [Convert]::ToUInt64($Matches[1], 16)
    }
    return [Convert]::ToUInt64($Value, 10)
}

function Parse-TelemetryFields
{
    param([string]$Line)

    $fields = @{}
    $matches = [regex]::Matches($Line, '(?<!\S)([A-Za-z][A-Za-z0-9-]*)\s+(0x[0-9A-Fa-f]+|[0-9]+)(?!\S)')
    foreach ($match in $matches) {
        $fields[$match.Groups[1].Value] = (Convert-TokenValue -Value $match.Groups[2].Value)
    }
    return $fields
}

function Get-FieldValue
{
    param(
        [hashtable]$Fields,
        [string]$Name,
        [uint64]$Default = 0
    )

    if ($Fields.ContainsKey($Name)) {
        return [uint64]$Fields[$Name]
    }
    return $Default
}

function Get-LineDecimal
{
    param(
        [string[]]$Lines,
        [string]$Prefix,
        [uint64]$Default = 0
    )

    $line = @($Lines | Where-Object { $_ -like "$Prefix*" } | Select-Object -Last 1)
    if ($line.Count -eq 0) {
        return $Default
    }
    if ([string]$line[0] -match ':\s*([0-9]+)\s*$') {
        return [uint64]$Matches[1]
    }
    return $Default
}

function Get-LineBoolean
{
    param(
        [string[]]$Lines,
        [string]$Prefix,
        [uint64]$Default = 0
    )

    $line = @($Lines | Where-Object { $_ -like "$Prefix*" } | Select-Object -Last 1)
    if ($line.Count -eq 0) {
        return $Default
    }
    if ([string]$line[0] -match ':\s*yes\s*$') {
        return 1
    }
    if ([string]$line[0] -match ':\s*no\s*$') {
        return 0
    }
    return $Default
}

function New-Classification
{
    param(
        [string]$Stage,
        [string]$Detail,
        [string]$NextTarget,
        [bool]$Pass = $false
    )

    return [PSCustomObject]@{
        pass = $Pass
        stage = $Stage
        detail = $Detail
        next_target = $NextTarget
    }
}

function Classify-HardwareDisplayInput
{
    param(
        [hashtable]$DisplayFields,
        [hashtable]$UiFields,
        [hashtable]$CursorFields,
        [bool]$DisplayFound,
        [bool]$UiFound,
        [bool]$CursorFound,
        [uint64]$MousePackets,
        [uint64]$XhciMouseEndpoint,
        [uint64]$XhciMouseReports,
        [uint64]$XhciMouseBytes,
        [uint64]$XhciError,
        [uint64]$I2cPointerFound,
        [uint64]$I2cPointerReports,
        [uint64]$I2cPointerError,
        [uint64]$I2cPointerCandidates,
        [uint64]$Ps2Present,
        [uint64]$Ps2Enabled
    )

    if (-not $DisplayFound) {
        return New-Classification -Stage "missing-display-readability" -Detail "No drs-display-readability line was found." -NextTarget "Run hwval on an M107-or-newer UEFI Product image and capture the full display telemetry."
    }
    if ((Get-FieldValue -Fields $DisplayFields -Name "available") -ne 1) {
        return New-Classification -Stage "display-unavailable" -Detail "Framebuffer/display service was not available." -NextTarget "Display target: inspect UEFI GOP handoff and framebuffer availability."
    }
    if ((Get-FieldValue -Fields $DisplayFields -Name "stride-ok") -ne 1) {
        return New-Classification -Stage "framebuffer-stride" -Detail "Framebuffer pitch/stride sanity failed." -NextTarget "Display target: fix GOP pitch handling and bytes-per-scanline calculations."
    }
    if ((Get-FieldValue -Fields $DisplayFields -Name "bounds-ok") -ne 1) {
        return New-Classification -Stage "framebuffer-bounds" -Detail "Framebuffer bounds sanity failed." -NextTarget "Display target: fix framebuffer byte-size and viewport bounds validation."
    }
    if ((Get-FieldValue -Fields $DisplayFields -Name "fit") -ne 1) {
        return New-Classification -Stage "display-fit" -Detail "Console viewport does not fit the framebuffer." -NextTarget "Display target: revise viewport scale/origin/extent selection for this GOP mode."
    }
    if ((Get-FieldValue -Fields $DisplayFields -Name "readable") -ne 1) {
        return New-Classification -Stage "display-readable" -Detail "Display telemetry says the console is not readable." -NextTarget "Display target: tune text scale, viewport geometry, and clipping policy."
    }
    if (-not $UiFound) {
        return New-Classification -Stage "missing-ui-polish" -Detail "No drs-ui-polish line was found." -NextTarget "UI target: boot an M109-or-newer UEFI Product image and capture compositor telemetry."
    }
    if ((Get-FieldValue -Fields $UiFields -Name "compositor-active") -ne 1) {
        return New-Classification -Stage "compositor-inactive" -Detail "The Product compositor did not initialize." -NextTarget "UI target: inspect compositor initialization and framebuffer/back-buffer fallback."
    }
    if ((Get-FieldValue -Fields $UiFields -Name "font") -ne 1) {
        return New-Classification -Stage "font-unavailable" -Detail "The UI font path did not initialize." -NextTarget "UI target: inspect bitmap font initialization before desktop rendering."
    }
    if ((Get-FieldValue -Fields $UiFields -Name "wm") -ne 1) {
        return New-Classification -Stage "window-manager-unavailable" -Detail "The window manager did not initialize." -NextTarget "UI target: inspect window-manager startup and display authority."
    }
    if ((Get-FieldValue -Fields $UiFields -Name "desktop") -ne 1) {
        return New-Classification -Stage "desktop-unavailable" -Detail "The desktop did not initialize." -NextTarget "UI target: inspect desktop startup after window-manager initialization."
    }
    if ((Get-FieldValue -Fields $UiFields -Name "taskbar") -eq 0) {
        return New-Classification -Stage "taskbar-unavailable" -Detail "No taskbar draw/init was recorded." -NextTarget "UI target: inspect taskbar layout and drawing path."
    }
    if ((Get-FieldValue -Fields $UiFields -Name "launcher") -eq 0) {
        return New-Classification -Stage "launcher-unavailable" -Detail "No launcher draw/init was recorded." -NextTarget "UI target: inspect launcher panel layout and drawing path."
    }
    if ((Get-FieldValue -Fields $UiFields -Name "windows") -eq 0) {
        return New-Classification -Stage "windows-unavailable" -Detail "No Product windows were created." -NextTarget "UI target: inspect initial Terminal/File Manager/Settings window creation."
    }

    $cursorVisible = Get-FieldValue -Fields $DisplayFields -Name "cursor-visible"
    if (($MousePackets -ne 0) -and ($cursorVisible -ne 1)) {
        if ($CursorFound) {
            if ((Get-FieldValue -Fields $CursorFields -Name "format-supported") -ne 1) {
                return New-Classification -Stage "cursor-format-unsupported" -Detail "Mouse packets are moving, but the framebuffer format is not cursor-draw compatible." -NextTarget "Cursor target: add framebuffer-format support or a safe conversion path for this GOP mode."
            }
            if ((Get-FieldValue -Fields $CursorFields -Name "surface-ready") -ne 1) {
                return New-Classification -Stage "cursor-surface-not-ready" -Detail "Mouse packets are moving, but cursor drawing does not have a validated framebuffer surface." -NextTarget "Cursor target: inspect framebuffer base, stride, bounds, and direct draw eligibility."
            }
            if ((Get-FieldValue -Fields $CursorFields -Name "draws") -eq 0) {
                return New-Classification -Stage "cursor-draw-not-called" -Detail "Mouse packets are moving, but no cursor draw was recorded." -NextTarget "Cursor target: route brokered pointer packets into compositor/direct cursor update on this hardware path."
            }
            if ((Get-FieldValue -Fields $CursorFields -Name "in-bounds") -ne 1) {
                return New-Classification -Stage "cursor-out-of-bounds" -Detail "Mouse packets are moving, but the cursor rectangle is outside the framebuffer." -NextTarget "Cursor target: fix cursor coordinate clamping and hardware-pointer coordinate scaling."
            }
            if ((Get-FieldValue -Fields $CursorFields -Name "drawn") -ne 1) {
                return New-Classification -Stage "cursor-draw-not-validated" -Detail "Mouse packets are moving and draw attempts exist, but no final drawn cursor state was recorded." -NextTarget "Cursor target: inspect cursor save/draw completion and framebuffer write path."
            }
        }
        return New-Classification -Stage "pointer-moving-cursor-hidden" -Detail "Mouse packets are moving but the cursor is not visible." -NextTarget "Cursor target: fix direct/compositor cursor draw path for this framebuffer mode."
    }
    if ($cursorVisible -ne 1) {
        if ($CursorFound) {
            if ((Get-FieldValue -Fields $CursorFields -Name "draws") -eq 0) {
                return New-Classification -Stage "cursor-draw-not-called" -Detail "No cursor draw was recorded." -NextTarget "Cursor target: inspect compositor/direct cursor startup draw and pointer routing."
            }
            if ((Get-FieldValue -Fields $CursorFields -Name "in-bounds") -ne 1) {
                return New-Classification -Stage "cursor-out-of-bounds" -Detail "The cursor rectangle is outside the framebuffer." -NextTarget "Cursor target: fix cursor coordinate clamping and boot-time cursor placement."
            }
        }
        return New-Classification -Stage "cursor-hidden" -Detail "No visible cursor was reported." -NextTarget "Cursor target: inspect cursor draw fallback and compositor cursor visibility."
    }
    if ($MousePackets -ne 0) {
        return New-Classification -Stage "display-input-ready" -Detail "Display is readable, UI initialized, cursor is visible, and pointer packets were received." -NextTarget "Hardware input/display ready. Next target: run interactive desktop focus/drag/click validation on the laptop." -Pass $true
    }

    if (($I2cPointerFound -ne 0) -and ($I2cPointerReports -ne 0)) {
        return New-Classification -Stage "i2c-pointer-reports-no-packets" -Detail "I2C pointer reports were observed but no brokered mouse packets reached the input queue." -NextTarget "Input target: bridge I2C HID reports into the brokered pointer queue."
    }
    if (($I2cPointerFound -ne 0) -and ($I2cPointerError -ne 0)) {
        return New-Classification -Stage "i2c-pointer-error" -Detail ("I2C HID pointer was found but reported error {0}." -f $I2cPointerError) -NextTarget "Input target: decode the I2C HID error and fix descriptor/report acquisition."
    }
    if (($I2cPointerCandidates -ne 0) -and ($I2cPointerFound -eq 0)) {
        return New-Classification -Stage "i2c-pointer-candidate-unbound" -Detail "LPSS/I2C pointer candidates exist but no I2C HID pointer is bound." -NextTarget "Input target: bind the ACPI/PCI LPSS I2C candidate to the HID touchpad probe."
    }
    if (($XhciMouseEndpoint -ne 0) -and ($XhciMouseReports -ne 0)) {
        return New-Classification -Stage "xhci-mouse-reports-no-packets" -Detail "xHCI mouse reports were received but no brokered mouse packets were produced." -NextTarget "Input target: decode xHCI HID mouse report format and enqueue brokered pointer packets."
    }
    if (($XhciMouseEndpoint -ne 0) -and ($XhciMouseReports -eq 0)) {
        return New-Classification -Stage "xhci-mouse-no-reports" -Detail "xHCI mouse endpoint exists but no reports were received." -NextTarget "Input target: inspect interrupt transfer queueing/completion for the xHCI mouse endpoint."
    }
    if ($XhciError -ne 0) {
        return New-Classification -Stage "xhci-input-error" -Detail ("xHCI input path reported error {0}." -f $XhciError) -NextTarget "Input target: decode xHCI error and fix controller/event-ring handling."
    }
    if (($Ps2Present -ne 0) -and ($Ps2Enabled -ne 0)) {
        return New-Classification -Stage "ps2-mouse-no-packets" -Detail "PS/2 fallback is present and enabled but no mouse packets were received." -NextTarget "Input target: inspect 8042 second-port enable, IRQ12 routing, and PS/2 packet parser."
    }
    if (($Ps2Present -ne 0) -and ($Ps2Enabled -eq 0)) {
        return New-Classification -Stage "ps2-mouse-disabled" -Detail "PS/2 fallback is present but not enabled." -NextTarget "Input target: inspect 8042 configuration byte and second-port enable sequence."
    }

    return New-Classification -Stage "no-pointer-backend" -Detail "Display/UI/cursor are ready, but no pointer backend produced packets or reports." -NextTarget "Input target: enumerate PS/2, xHCI HID, and LPSS/I2C touchpad candidates on this hardware."
}

Assert-FileExists -Path $InputPath -Message "Hardware display/input analyzer: input file not found: $InputPath"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$lines = @(Get-Content -Path $InputPath)
$displayLine = @($lines | Where-Object { $_ -match 'drs-display-readability' } | Select-Object -Last 1)
$uiLine = @($lines | Where-Object { $_ -match 'drs-ui-polish' } | Select-Object -Last 1)
$cursorLine = @($lines | Where-Object { $_ -match 'drs-cursor-path' } | Select-Object -Last 1)
$displayFields = @{}
$uiFields = @{}
$cursorFields = @{}
if ($displayLine.Count -ne 0) {
    $displayFields = Parse-TelemetryFields -Line ([string]$displayLine[0])
}
if ($uiLine.Count -ne 0) {
    $uiFields = Parse-TelemetryFields -Line ([string]$uiLine[0])
}
if ($cursorLine.Count -ne 0) {
    $cursorFields = Parse-TelemetryFields -Line ([string]$cursorLine[0])
}

$mousePackets = Get-LineDecimal -Lines $lines -Prefix "mouse packets"
$xhciMouseEndpoint = Get-LineBoolean -Lines $lines -Prefix "xhci mouse endpoint"
$xhciMouseReports = Get-LineDecimal -Lines $lines -Prefix "xhci mouse reports"
$xhciMouseBytes = Get-LineDecimal -Lines $lines -Prefix "xhci mouse bytes"
$xhciError = Get-LineDecimal -Lines $lines -Prefix "xhci error"
$i2cPointerFound = Get-LineBoolean -Lines $lines -Prefix "i2c pointer found"
$i2cPointerReports = Get-LineDecimal -Lines $lines -Prefix "i2c pointer reports"
$i2cPointerError = Get-LineDecimal -Lines $lines -Prefix "i2c pointer error"
$i2cPointerCandidates = Get-LineDecimal -Lines $lines -Prefix "i2c pointer candidates"
$ps2Present = Get-LineBoolean -Lines $lines -Prefix "ps2 fallback present"
$ps2Enabled = Get-LineBoolean -Lines $lines -Prefix "ps2 fallback enabled"

$classification = Classify-HardwareDisplayInput `
    -DisplayFields $displayFields `
    -UiFields $uiFields `
    -CursorFields $cursorFields `
    -DisplayFound ($displayLine.Count -ne 0) `
    -UiFound ($uiLine.Count -ne 0) `
    -CursorFound ($cursorLine.Count -ne 0) `
    -MousePackets $mousePackets `
    -XhciMouseEndpoint $xhciMouseEndpoint `
    -XhciMouseReports $xhciMouseReports `
    -XhciMouseBytes $xhciMouseBytes `
    -XhciError $xhciError `
    -I2cPointerFound $i2cPointerFound `
    -I2cPointerReports $i2cPointerReports `
    -I2cPointerError $i2cPointerError `
    -I2cPointerCandidates $i2cPointerCandidates `
    -Ps2Present $ps2Present `
    -Ps2Enabled $ps2Enabled

$analysis = [PSCustomObject]@{
    tool = "analyze-hardware-display-input-capture"
    input = (Resolve-Path $InputPath).Path
    pass = [bool]$classification.pass
    stage = [string]$classification.stage
    detail = [string]$classification.detail
    next_target = [string]$classification.next_target
    display_line_found = ($displayLine.Count -ne 0)
    ui_line_found = ($uiLine.Count -ne 0)
    cursor_line_found = ($cursorLine.Count -ne 0)
    display = [PSCustomObject]$displayFields
    ui = [PSCustomObject]$uiFields
    cursor = [PSCustomObject]$cursorFields
    input_state = [PSCustomObject]@{
        mouse_packets = $mousePackets
        xhci_mouse_endpoint = $xhciMouseEndpoint
        xhci_mouse_reports = $xhciMouseReports
        xhci_mouse_bytes = $xhciMouseBytes
        xhci_error = $xhciError
        i2c_pointer_found = $i2cPointerFound
        i2c_pointer_reports = $i2cPointerReports
        i2c_pointer_error = $i2cPointerError
        i2c_pointer_candidates = $i2cPointerCandidates
        ps2_present = $ps2Present
        ps2_enabled = $ps2Enabled
    }
    raw_display_line = if ($displayLine.Count -ne 0) { [string]$displayLine[0] } else { "" }
    raw_ui_line = if ($uiLine.Count -ne 0) { [string]$uiLine[0] } else { "" }
    raw_cursor_line = if ($cursorLine.Count -ne 0) { [string]$cursorLine[0] } else { "" }
}

$analysisJsonPath = Join-Path $OutputDir "hardware-display-input-analysis.json"
$analysisTextPath = Join-Path $OutputDir "hardware-display-input-analysis.txt"
$analysis | ConvertTo-Json -Depth 6 | Set-Content -Path $analysisJsonPath -Encoding Ascii

@(
    "hardware-display-input-analysis: $($analysis.stage)",
    "pass: $($analysis.pass)",
    "detail: $($analysis.detail)",
    "next-target: $($analysis.next_target)",
    "display-line-found: $($analysis.display_line_found)",
    "ui-line-found: $($analysis.ui_line_found)",
    "cursor-line-found: $($analysis.cursor_line_found)",
    "mouse-packets: $mousePackets",
    "xhci-mouse-endpoint: $xhciMouseEndpoint",
    "xhci-mouse-reports: $xhciMouseReports",
    "i2c-pointer-found: $i2cPointerFound",
    "i2c-pointer-reports: $i2cPointerReports",
    "i2c-pointer-error: $i2cPointerError",
    "i2c-pointer-candidates: $i2cPointerCandidates",
    "ps2-present: $ps2Present",
    "ps2-enabled: $ps2Enabled",
    "output-json: $analysisJsonPath"
) | Set-Content -Path $analysisTextPath -Encoding Ascii

Write-Host "hardware-display-input-analysis: $($analysis.stage)"
Write-Host "  pass: $($analysis.pass)"
Write-Host "  detail: $($analysis.detail)"
Write-Host "  next target: $($analysis.next_target)"
Write-Host "  output: $analysisJsonPath"

if (-not $analysis.pass) {
    exit 2
}
