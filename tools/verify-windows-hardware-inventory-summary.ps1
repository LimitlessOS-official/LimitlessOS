param(
    [string]$BundlePath = ".\dist\windows-hardware-inventory\windows-hardware-inventory-20260705-081144.zip",
    [string]$OutputDir = ".\build\windows-hardware-inventory-summary-verification",
    [switch]$RequireMsiCyborgEvidence
)

$ErrorActionPreference = "Stop"

function Assert-True
{
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-Text
{
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return [string]$Value
}

$root = Split-Path -Parent $PSScriptRoot
$BundlePath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BundlePath)
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)

Assert-True -Condition (Test-Path -LiteralPath $BundlePath) -Message "Windows hardware inventory bundle not found: $BundlePath"
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

& (Join-Path $root "tools\summarize-windows-hardware-inventory.ps1") -BundlePath $BundlePath -OutputDir $OutputDir
if (($null -ne $LASTEXITCODE) -and ($LASTEXITCODE -ne 0)) {
    throw "summarize-windows-hardware-inventory.ps1 failed with exit code $LASTEXITCODE"
}

$summaryPath = Join-Path $OutputDir "windows-hardware-inventory-summary.json"
$reportPath = Join-Path $OutputDir "windows-hardware-inventory-summary.txt"
Assert-True -Condition (Test-Path -LiteralPath $summaryPath) -Message "summary JSON was not written"
Assert-True -Condition (Test-Path -LiteralPath $reportPath) -Message "summary text report was not written"

$summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
$counts = $summary.counts

Assert-True -Condition ([uint32]$counts.relevant_devices -gt 0) -Message "expected at least one relevant device"
Assert-True -Condition ([uint32]$counts.usb_mouse_devices -gt 0) -Message "expected at least one USB/HID mouse-class device"
Assert-True -Condition ([uint32]$counts.touchpad_devices -gt 0) -Message "expected at least one touchpad/HID pointer-class device"
Assert-True -Condition ([uint32]$counts.storage_devices -gt 0) -Message "expected at least one storage device"
Assert-True -Condition ([uint32]$counts.display_devices -gt 0) -Message "expected at least one display device"

if ($RequireMsiCyborgEvidence) {
    $report = Get-Content -LiteralPath $reportPath -Raw
    Assert-True -Condition $report.Contains("ACPI\ELAN0307") -Message "MSI evidence missing ACPI\ELAN0307 touchpad"
    Assert-True -Condition $report.Contains("HID_DEVICE_SYSTEM_MOUSE") -Message "MSI evidence missing HID system mouse usage"
    Assert-True -Condition $report.Contains("VID_046D&PID_C53F") -Message "MSI evidence missing Logitech LIGHTSPEED receiver"
    Assert-True -Condition $report.Contains("VEN_8086&DEV_A77F") -Message "MSI evidence missing Intel RST VMD controller"
    Assert-True -Condition $report.Contains("VEN_8086&DEV_51ED") -Message "MSI evidence missing Intel xHCI controller"
}

$result = [PSCustomObject]@{
    tool = "verify-windows-hardware-inventory-summary"
    bundle = $BundlePath
    output_dir = $OutputDir
    pass = $true
    counts = $counts
    require_msi_cyborg_evidence = [bool]$RequireMsiCyborgEvidence
    primary_usb_mouse = Get-Text -Value $summary.primary.usb_mouse.instance_id
    primary_xhci_controller = Get-Text -Value $summary.primary.xhci_controller.instance_id
    primary_touchpad = Get-Text -Value $summary.primary.touchpad.instance_id
    primary_storage_controller = Get-Text -Value $summary.primary.storage_controller.instance_id
    primary_display = Get-Text -Value $summary.primary.display.instance_id
}

$resultPath = Join-Path $OutputDir "windows-hardware-inventory-summary-verification.json"
$result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8

Write-Host "windows-hardware-inventory-summary: verified"
Write-Host "  output: $resultPath"
Write-Host "  relevant devices: $($counts.relevant_devices)"
Write-Host "  usb mouse devices: $($counts.usb_mouse_devices)"
Write-Host "  touchpad devices: $($counts.touchpad_devices)"
Write-Host "  xhci controllers: $($counts.xhci_controllers)"
Write-Host "  require MSI evidence: $([bool]$RequireMsiCyborgEvidence)"
