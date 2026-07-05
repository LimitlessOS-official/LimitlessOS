param(
    [string]$BundlePath = "",
    [string]$OutputDir = ".\dist\windows-hwval",
    [switch]$Collect,
    [switch]$IncludeSetupApiTail,
    [switch]$IncludeMsInfo,
    [switch]$IncludeRegistrySnapshot,
    [switch]$IncludeAllPnpProperties,
    [switch]$Full
)

$ErrorActionPreference = "Stop"

function Write-Line
{
    param([string]$Text)

    Write-Host $Text
    $script:Lines += $Text
}

function Get-Text
{
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return [string]$Value
}

function Get-StringList
{
    param([object]$Value)

    if ($null -eq $Value) {
        return @()
    }
    if ($Value -is [array]) {
        return @($Value | ForEach-Object { [string]$_ })
    }
    return @([string]$Value)
}

function Get-DeviceText
{
    param([object]$Device)

    $parts = @(
        (Get-Text -Value $Device.class),
        (Get-Text -Value $Device.friendly_name),
        (Get-Text -Value $Device.instance_id),
        (Get-Text -Value $Device.service),
        (Get-Text -Value $Device.driver_provider),
        (Get-Text -Value $Device.driver_inf_path)
    )
    $parts += Get-StringList -Value $Device.hardware_ids
    $parts += Get-StringList -Value $Device.compatible_ids
    $parts += Get-StringList -Value $Device.location_paths
    return (($parts -join " ").ToUpperInvariant())
}

function Select-FirstDevice
{
    param(
        [object[]]$Devices,
        [scriptblock]$Predicate
    )

    $matches = @($Devices | Where-Object $Predicate | Select-Object -First 1)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[0]
}

function Write-DeviceBrief
{
    param(
        [string]$Prefix,
        [object]$Device
    )

    if ($null -eq $Device) {
        Write-Line "$Prefix present 0"
        return
    }

    Write-Line "$Prefix present 1 name `"$((Get-Text -Value $Device.friendly_name).Replace('"', "'"))`" class `"$((Get-Text -Value $Device.class).Replace('"', "'"))`" service `"$((Get-Text -Value $Device.service).Replace('"', "'"))`""
    Write-Line "$Prefix instance $((Get-Text -Value $Device.instance_id))"
    $hwids = @(Get-StringList -Value $Device.hardware_ids)
    if ($hwids.Count -gt 0) {
        Write-Line "$Prefix hwids $($hwids -join '; ')"
    }
    $compat = @(Get-StringList -Value $Device.compatible_ids)
    if ($compat.Count -gt 0) {
        Write-Line "$Prefix compat $($compat -join '; ')"
    }
}

function Get-DriverTargetDevices
{
    param(
        [object]$Summary,
        [string]$Name
    )

    $target = @($Summary.driver_targets | Where-Object { $_.name -eq $Name } | Select-Object -First 1)
    if ($target.Count -eq 0) {
        return @()
    }
    return @($target[0].devices)
}

$root = Split-Path -Parent $PSScriptRoot
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

if ($Collect) {
    $captureDir = Join-Path $OutputDir "capture"
    $collectorArgs = @("-OutputDir", $captureDir)
    if ($IncludeSetupApiTail) { $collectorArgs += "-IncludeSetupApiTail" }
    if ($IncludeMsInfo) { $collectorArgs += "-IncludeMsInfo" }
    if ($IncludeRegistrySnapshot) { $collectorArgs += "-IncludeRegistrySnapshot" }
    if ($IncludeAllPnpProperties) { $collectorArgs += "-IncludeAllPnpProperties" }
    & (Join-Path $root "tools\collect-windows-hardware-inventory.ps1") @collectorArgs
    if (($null -ne $LASTEXITCODE) -and ($LASTEXITCODE -ne 0)) {
        throw "collect-windows-hardware-inventory.ps1 failed with exit code $LASTEXITCODE"
    }
    $BundlePath = Get-ChildItem -LiteralPath $captureDir -Filter "windows-hardware-inventory-*.zip" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if ([string]::IsNullOrWhiteSpace($BundlePath)) {
    $latest = @(Get-ChildItem -LiteralPath (Join-Path $root "dist\windows-hardware-inventory") -Filter "windows-hardware-inventory-*.zip" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1)
    if ($latest.Count -eq 0) {
        throw "No Windows hardware inventory bundle found. Run with -Collect or pass -BundlePath."
    }
    $BundlePath = $latest[0].FullName
}

$BundlePath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BundlePath)
$summaryDir = Join-Path $OutputDir "summary"
& (Join-Path $root "tools\summarize-windows-hardware-inventory.ps1") -BundlePath $BundlePath -OutputDir $summaryDir
if (($null -ne $LASTEXITCODE) -and ($LASTEXITCODE -ne 0)) {
    throw "summarize-windows-hardware-inventory.ps1 failed with exit code $LASTEXITCODE"
}

$summaryPath = Join-Path $summaryDir "windows-hardware-inventory-summary.json"
$summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
$counts = $summary.counts
$script:Lines = @()

$usbDevices = Get-DriverTargetDevices -Summary $summary -Name "usb-hid-mouse"
$touchDevices = Get-DriverTargetDevices -Summary $summary -Name "i2c-hid-touchpad"
$storageDevices = Get-DriverTargetDevices -Summary $summary -Name "storage"
$displayDevices = Get-DriverTargetDevices -Summary $summary -Name "display"
$networkDevices = Get-DriverTargetDevices -Summary $summary -Name "network"

$usbSystemMouse = Select-FirstDevice -Devices $usbDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("HID_DEVICE_SYSTEM_MOUSE") -and $text.Contains("VID_")
}
$usbCompositeReceiver = Select-FirstDevice -Devices $usbDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("VID_") -and $text.Contains("MI_")
}
$xhci = $summary.primary.xhci_controller
$touchpad = Select-FirstDevice -Devices $touchDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("PNP0C50") -or $text.Contains("ACPI\ELAN") -or $text.Contains("HID_DEVICE_UP:000D_U:0005")
}
$i2cHost = Select-FirstDevice -Devices $touchDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("SERIAL IO") -or $text.Contains("LPSS") -or $text.Contains("CC_0C80")
}
$vmdController = Select-FirstDevice -Devices $storageDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("VMD") -or $text.Contains("CC_0104")
}
$nvmeDisk = Select-FirstDevice -Devices $storageDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("NVME")
}
$gpu = Select-FirstDevice -Devices $displayDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("CC_0300") -or $text.Contains("GRAPHICS")
}
$monitor = Select-FirstDevice -Devices $displayDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("MONITOR")
}
$wired = Select-FirstDevice -Devices $networkDevices -Predicate {
    $text = Get-DeviceText -Device $_
    ($text.Contains("ETHERNET") -or $text.Contains("CC_0200")) -and
        (-not $text.Contains("ROOT\")) -and
        (-not $text.Contains("VIRTUAL")) -and
        (-not $text.Contains("BLUETOOTH"))
}
$wireless = Select-FirstDevice -Devices $networkDevices -Predicate {
    $text = Get-DeviceText -Device $_
    ($text.Contains("WI-FI") -or $text.Contains("WIFI") -or $text.Contains("WIRELESS") -or $text.Contains("CC_0280")) -and
        ($text.Contains("CLASS NET") -or $text.Contains("CC_0280") -or $text.Contains("PCI\"))
}
$bluetooth = Select-FirstDevice -Devices $networkDevices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("BLUETOOTH")
}

$machine = ""
$firmware = ""
if ($null -ne $summary.computer) {
    $machine = "$(Get-Text -Value $summary.computer.computer_system.Manufacturer) $(Get-Text -Value $summary.computer.computer_system.Model)".Trim()
    $firmware = "$(Get-Text -Value $summary.computer.computer_info.BiosFirmwareType) $(Get-Text -Value $summary.computer.bios.SMBIOSBIOSVersion)".Trim()
}

Write-Line "windows-hwval"
Write-Line "source windows-pnp bundle $BundlePath"
Write-Line "policy readonly 1 no-internal-write 1 no-format 1 no-nvram 1"
Write-Line "machine `"$machine`" firmware `"$firmware`""
Write-Line "counts relevant $($counts.relevant_devices) usb-mouse $($counts.usb_mouse_devices) touchpad $($counts.touchpad_devices) i2c $($counts.i2c_controllers) xhci $($counts.xhci_controllers) storage $($counts.storage_devices) display $($counts.display_devices) network $($counts.network_devices)"
Write-Line "drs-win-hwval product 1 readonly 1 windows-source 1 relevant $($counts.relevant_devices) usb-mouse $($counts.usb_mouse_devices) touchpad $($counts.touchpad_devices) i2c $($counts.i2c_controllers) xhci $($counts.xhci_controllers) storage $($counts.storage_devices) display $($counts.display_devices) network $($counts.network_devices)"
Write-Line ""

Write-Line "input"
Write-DeviceBrief -Prefix "  xhci" -Device $xhci
Write-DeviceBrief -Prefix "  usb-system-mouse" -Device $usbSystemMouse
Write-DeviceBrief -Prefix "  usb-composite-hid" -Device $usbCompositeReceiver
$usbMouseCandidates = @($usbDevices | Where-Object {
    $text = Get-DeviceText -Device $_
    $text.Contains("HID_DEVICE_SYSTEM_MOUSE") -and $text.Contains("VID_")
} | Select-Object -First 6)
$candidateIndex = 0
foreach ($candidate in $usbMouseCandidates) {
    Write-Line "  usb-mouse-candidate $candidateIndex $((Get-Text -Value $candidate.instance_id))"
    ++$candidateIndex
}
Write-DeviceBrief -Prefix "  i2c-host" -Device $i2cHost
Write-DeviceBrief -Prefix "  touchpad" -Device $touchpad
Write-Line "  next xhci-composite-hid-report-acquisition and acpi-pnp0c50-i2c-hid-resource-discovery"
Write-Line ""

Write-Line "storage"
Write-DeviceBrief -Prefix "  vmd-controller" -Device $vmdController
Write-DeviceBrief -Prefix "  nvme-disk" -Device $nvmeDisk
Write-Line "  next generic-vmd-rst-before-internal-nvme-fat"
Write-Line ""

Write-Line "display"
Write-DeviceBrief -Prefix "  gpu" -Device $gpu
Write-DeviceBrief -Prefix "  monitor" -Device $monitor
Write-Line "  next gop-framebuffer-robustness-before-native-drm-kms"
Write-Line ""

Write-Line "network"
Write-DeviceBrief -Prefix "  wired" -Device $wired
Write-DeviceBrief -Prefix "  wireless" -Device $wireless
Write-DeviceBrief -Prefix "  bluetooth" -Device $bluetooth
Write-Line "  next ethernet-class-first-wifi-after-firmware-policy"

if ($Full) {
    Write-Line ""
    foreach ($target in @($summary.driver_targets)) {
        Write-Line "target $($target.name) stage `"$($target.stage)`" count $($target.count)"
        foreach ($device in @($target.devices)) {
            Write-DeviceBrief -Prefix "  device" -Device $device
        }
    }
}

$textPath = Join-Path $OutputDir "windows-hwval.txt"
$jsonPath = Join-Path $OutputDir "windows-hwval.json"
$script:Lines | Set-Content -Path $textPath -Encoding UTF8
[PSCustomObject]@{
    tool = "hwval-windows"
    bundle = $BundlePath
    summary = $summaryPath
    text = $textPath
    counts = $counts
    machine = $machine
    firmware = $firmware
    xhci = $xhci
    usb_system_mouse = $usbSystemMouse
    usb_composite_hid = $usbCompositeReceiver
    i2c_host = $i2cHost
    touchpad = $touchpad
    vmd_controller = $vmdController
    nvme_disk = $nvmeDisk
    gpu = $gpu
    monitor = $monitor
    wired = $wired
    wireless = $wireless
    bluetooth = $bluetooth
} | ConvertTo-Json -Depth 12 | Set-Content -Path $jsonPath -Encoding UTF8

Write-Host ""
Write-Host "windows-hwval: wrote"
Write-Host "  text: $textPath"
Write-Host "  json: $jsonPath"
