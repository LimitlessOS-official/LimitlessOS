param(
    [Parameter(Mandatory = $true)]
    [string]$BundlePath,

    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

function Resolve-BundleDirectory
{
    param([string]$Path)

    $resolved = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    if (Test-Path -LiteralPath $resolved -PathType Container) {
        return $resolved
    }

    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "Bundle path does not exist: $Path"
    }

    $extractRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("limitlessos-windows-hw-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $extractRoot -Force | Out-Null
    Expand-Archive -LiteralPath $resolved -DestinationPath $extractRoot -Force
    return $extractRoot
}

function Read-Json
{
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
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
        [string]$Device.class,
        [string]$Device.friendly_name,
        [string]$Device.instance_id,
        [string]$Device.service,
        [string]$Device.driver_provider,
        [string]$Device.driver_inf_path
    )
    $parts += Get-StringList -Value $Device.hardware_ids
    $parts += Get-StringList -Value $Device.compatible_ids
    $parts += Get-StringList -Value $Device.location_paths
    return (($parts -join " ").ToLowerInvariant())
}

function Select-Devices
{
    param(
        [object[]]$Devices,
        [scriptblock]$Predicate
    )

    return @($Devices | Where-Object $Predicate | Select-Object class,friendly_name,instance_id,service,driver_provider,driver_version,driver_inf_path,location_info,location_paths,hardware_ids,compatible_ids,parent,children)
}

function New-DriverTarget
{
    param(
        [string]$Name,
        [string]$Stage,
        [string]$Why,
        [object[]]$Devices,
        [string]$NextKernelWork
    )

    return [PSCustomObject]@{
        name = $Name
        stage = $Stage
        why = $Why
        count = $Devices.Count
        next_kernel_work = $NextKernelWork
        devices = $Devices
    }
}

$bundleDir = Resolve-BundleDirectory -Path $BundlePath
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path (Split-Path -Parent $bundleDir) "windows-hardware-inventory-summary"
}
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$manifest = Read-Json -Path (Join-Path $bundleDir "manifest.json")
$computer = Read-Json -Path (Join-Path $bundleDir "json\computer-info.json")
$devicesJson = Read-Json -Path (Join-Path $bundleDir "json\pnp-present-relevant-expanded.json")
$focusJson = Read-Json -Path (Join-Path $bundleDir "json\device-focus.json")
$diskJson = Read-Json -Path (Join-Path $bundleDir "json\disk-partition-physical.json")
$platformJson = Read-Json -Path (Join-Path $bundleDir "json\pointing-keyboard-usb-storage-display-network.json")

$devices = @($devicesJson)
$usbMouseDevices = Select-Devices -Devices $devices -Predicate {
    $text = Get-DeviceText -Device $_
    (($text.Contains("hid_device_system_mouse")) -or ($text.Contains("mouse")) -or ($text.Contains("usage_0002")) -or ($text.Contains("mi_") -and $text.Contains("hid"))) -and ($text.Contains("usb") -or $text.Contains("hid\vid_"))
}
$touchpadDevices = Select-Devices -Devices $devices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("touchpad") -or $text.Contains("precision touchpad") -or $text.Contains("elan") -or $text.Contains("synaptics") -or (($text.Contains("i2c") -or $text.Contains("acpi\")) -and $text.Contains("hid"))
}
$i2cControllers = Select-Devices -Devices $devices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("i2c") -or $text.Contains("serial io") -or $text.Contains("lpss")
}
$xhciControllers = Select-Devices -Devices $devices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("xhci") -or ($text.Contains("usb") -and $text.Contains("host controller"))
}
$storageDevices = Select-Devices -Devices $devices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("nvme") -or $text.Contains("vmd") -or $text.Contains("raid") -or $text.Contains("ahci") -or $text.Contains("sata") -or $text.Contains("storage")
}
$displayDevices = Select-Devices -Devices $devices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("display") -or $text.Contains("graphics") -or $text.Contains("vga") -or $text.Contains("monitor")
}
$networkDevices = Select-Devices -Devices $devices -Predicate {
    $text = Get-DeviceText -Device $_
    $text.Contains("network") -or $text.Contains("ethernet") -or $text.Contains("wi-fi") -or $text.Contains("wifi") -or $text.Contains("bluetooth")
}

$targets = @(
    (New-DriverTarget -Name "usb-hid-mouse" -Stage "xHCI HID mouse/interface matching" -Why "Windows can identify the exact USB HID mouse and composite-interface IDs without rebooting into LimitlessOS." -Devices $usbMouseDevices -NextKernelWork "Compare these VID/PID/MI/HID compatible IDs against xhci64 HID interface matching and endpoint probe policy."),
    (New-DriverTarget -Name "i2c-hid-touchpad" -Stage "ACPI/I2C HID touchpad discovery" -Why "Windows exposes the touchpad ACPI/HID parent chain and the Serial IO/I2C controller binding that LimitlessOS must discover generically." -Devices (@($touchpadDevices) + @($i2cControllers)) -NextKernelWork "Use ACPI/PnP IDs and parent location paths to replace hardcoded I2C address probing with ACPI-described I2C HID resource discovery."),
    (New-DriverTarget -Name "storage" -Stage "NVMe/VMD/AHCI storage binding" -Why "Windows identifies whether internal storage is direct NVMe, Intel VMD, RAID/RST, AHCI, or another PCI storage class." -Devices $storageDevices -NextKernelWork "Prioritize a generic PCI storage binding path for the detected class/vendor/device chain, keeping writes authority-gated."),
    (New-DriverTarget -Name "display" -Stage "GOP/native graphics readiness" -Why "Windows identifies GPU and monitor stack; LimitlessOS can use this to choose GOP fixes first and native graphics later." -Devices $displayDevices -NextKernelWork "Keep GOP framebuffer robust for all adapters first; defer native GPU mode-setting until the driver framework is ready."),
    (New-DriverTarget -Name "network" -Stage "wired/wireless network driver selection" -Why "Windows identifies available Ethernet/Wi-Fi/Bluetooth devices and bound drivers." -Devices $networkDevices -NextKernelWork "Implement Ethernet-class support before Wi-Fi firmware policy, using exact PCI/USB IDs as fixtures not special cases.")
)

$summary = [PSCustomObject]@{
    tool = "summarize-windows-hardware-inventory"
    bundle = $bundleDir
    manifest = $manifest
    computer = $computer
    counts = [PSCustomObject]@{
        relevant_devices = $devices.Count
        usb_mouse_devices = $usbMouseDevices.Count
        touchpad_devices = $touchpadDevices.Count
        i2c_controllers = $i2cControllers.Count
        xhci_controllers = $xhciControllers.Count
        storage_devices = $storageDevices.Count
        display_devices = $displayDevices.Count
        network_devices = $networkDevices.Count
    }
    xhci_controllers = $xhciControllers
    driver_targets = $targets
    disk = $diskJson
    platform = $platformJson
    focus = $focusJson
}

$jsonPath = Join-Path $OutputDir "windows-hardware-inventory-summary.json"
$textPath = Join-Path $OutputDir "windows-hardware-inventory-summary.txt"
$summary | ConvertTo-Json -Depth 16 | Set-Content -Path $jsonPath -Encoding UTF8

$lines = @()
$lines += "windows-hardware-inventory-summary"
$lines += "bundle: $bundleDir"
if ($null -ne $computer) {
    $lines += "machine: $($computer.computer_system.Manufacturer) $($computer.computer_system.Model)"
    $lines += "firmware: $($computer.computer_info.BiosFirmwareType) bios $($computer.bios.SMBIOSBIOSVersion)"
}
$lines += "relevant-devices: $($devices.Count)"
$lines += "usb-mouse-devices: $($usbMouseDevices.Count)"
$lines += "touchpad-devices: $($touchpadDevices.Count)"
$lines += "i2c-controllers: $($i2cControllers.Count)"
$lines += "xhci-controllers: $($xhciControllers.Count)"
$lines += "storage-devices: $($storageDevices.Count)"
$lines += "display-devices: $($displayDevices.Count)"
$lines += "network-devices: $($networkDevices.Count)"
$lines += ""
foreach ($target in $targets) {
    $lines += "target: $($target.name)"
    $lines += "  stage: $($target.stage)"
    $lines += "  count: $($target.count)"
    $lines += "  next: $($target.next_kernel_work)"
    foreach ($device in @($target.devices | Select-Object -First 8)) {
        $ids = (Get-StringList -Value $device.hardware_ids) -join "; "
        $compat = (Get-StringList -Value $device.compatible_ids) -join "; "
        $location = (Get-StringList -Value $device.location_paths) -join "; "
        $lines += "  device: $($device.friendly_name)"
        $lines += "    class: $($device.class) service: $($device.service)"
        $lines += "    instance: $($device.instance_id)"
        if (-not [string]::IsNullOrWhiteSpace($ids)) {
            $lines += "    hwids: $ids"
        }
        if (-not [string]::IsNullOrWhiteSpace($compat)) {
            $lines += "    compat: $compat"
        }
        if (-not [string]::IsNullOrWhiteSpace($location)) {
            $lines += "    location: $location"
        }
    }
    $lines += ""
}
$lines += "output-json: $jsonPath"
$lines | Set-Content -Path $textPath -Encoding UTF8

Write-Host "windows-hardware-inventory-summary: complete"
Write-Host "  output: $jsonPath"
Write-Host "  report: $textPath"
Write-Host "  usb mouse devices: $($usbMouseDevices.Count)"
Write-Host "  touchpad devices: $($touchpadDevices.Count)"
Write-Host "  i2c controllers: $($i2cControllers.Count)"
Write-Host "  xhci controllers: $($xhciControllers.Count)"
