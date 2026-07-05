param(
    [string]$BundlePath = ".\dist\windows-hardware-inventory\windows-hardware-inventory-20260705-081144.zip",
    [string]$OutputDir = ".\build\windows-hwval-verification",
    [switch]$RequireMsiCyborgEvidence
)

$ErrorActionPreference = "Stop"

function Assert-Contains
{
    param(
        [string]$Text,
        [string]$Needle,
        [string]$Message
    )

    if (-not $Text.Contains($Needle)) {
        throw $Message
    }
}

$root = Split-Path -Parent $PSScriptRoot
$BundlePath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BundlePath)
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)

if (-not (Test-Path -LiteralPath $BundlePath)) {
    throw "Windows hardware inventory bundle not found: $BundlePath"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

& (Join-Path $root "tools\hwval-windows.ps1") -BundlePath $BundlePath -OutputDir $OutputDir
if (($null -ne $LASTEXITCODE) -and ($LASTEXITCODE -ne 0)) {
    throw "hwval-windows.ps1 failed with exit code $LASTEXITCODE"
}

$textPath = Join-Path $OutputDir "windows-hwval.txt"
$jsonPath = Join-Path $OutputDir "windows-hwval.json"
if (-not (Test-Path -LiteralPath $textPath)) {
    throw "windows-hwval text output was not written"
}
if (-not (Test-Path -LiteralPath $jsonPath)) {
    throw "windows-hwval JSON output was not written"
}

$text = Get-Content -LiteralPath $textPath -Raw
$json = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json

Assert-Contains -Text $text -Needle "windows-hwval" -Message "missing windows-hwval header"
Assert-Contains -Text $text -Needle "drs-win-hwval" -Message "missing drs-win-hwval proof line"
Assert-Contains -Text $text -Needle "policy readonly 1" -Message "missing read-only policy proof"
Assert-Contains -Text $text -Needle "input" -Message "missing input section"
Assert-Contains -Text $text -Needle "storage" -Message "missing storage section"
Assert-Contains -Text $text -Needle "display" -Message "missing display section"
Assert-Contains -Text $text -Needle "network" -Message "missing network section"

if ([uint32]$json.counts.relevant_devices -eq 0) {
    throw "expected nonzero relevant device count"
}
if ([uint32]$json.counts.xhci_controllers -eq 0) {
    throw "expected nonzero xHCI controller count"
}
if ([uint32]$json.counts.storage_devices -eq 0) {
    throw "expected nonzero storage device count"
}
if ([uint32]$json.counts.display_devices -eq 0) {
    throw "expected nonzero display device count"
}

if ($RequireMsiCyborgEvidence) {
    Assert-Contains -Text $text -Needle "VEN_8086&DEV_51ED" -Message "MSI evidence missing Intel xHCI controller"
    Assert-Contains -Text $text -Needle "VID_046D&PID_C53F" -Message "MSI evidence missing Logitech receiver HID collection"
    Assert-Contains -Text $text -Needle "ACPI\ELAN0307" -Message "MSI evidence missing ELAN I2C HID touchpad"
    Assert-Contains -Text $text -Needle "PNP0C50" -Message "MSI evidence missing PNP0C50 I2C HID compatibility"
    Assert-Contains -Text $text -Needle "VEN_8086&DEV_A77F" -Message "MSI evidence missing Intel VMD storage controller"
}

$result = [PSCustomObject]@{
    tool = "verify-hwval-windows"
    pass = $true
    bundle = $BundlePath
    output_dir = $OutputDir
    text = $textPath
    json = $jsonPath
    counts = $json.counts
    require_msi_cyborg_evidence = [bool]$RequireMsiCyborgEvidence
}
$resultPath = Join-Path $OutputDir "windows-hwval-verification.json"
$result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8

Write-Host "windows-hwval: verified"
Write-Host "  output: $resultPath"
Write-Host "  relevant devices: $($json.counts.relevant_devices)"
Write-Host "  usb mouse devices: $($json.counts.usb_mouse_devices)"
Write-Host "  touchpad devices: $($json.counts.touchpad_devices)"
Write-Host "  xhci controllers: $($json.counts.xhci_controllers)"
Write-Host "  require MSI evidence: $([bool]$RequireMsiCyborgEvidence)"
