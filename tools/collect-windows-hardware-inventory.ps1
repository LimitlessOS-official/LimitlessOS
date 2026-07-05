param(
    [string]$OutputDir = ".\dist\windows-hardware-inventory",
    [switch]$IncludeSetupApiTail,
    [switch]$IncludeMsInfo,
    [switch]$IncludeRegistrySnapshot,
    [switch]$IncludeAllPnpProperties
)

$ErrorActionPreference = "Stop"

function New-CleanDirectory
{
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Write-JsonFile
{
    param(
        [string]$Path,
        [object]$Value,
        [int]$Depth = 10
    )

    $Value | ConvertTo-Json -Depth $Depth | Set-Content -Path $Path -Encoding UTF8
}

function Select-ObjectSafe
{
    param(
        [object]$Value,
        [string[]]$Property
    )

    if ($null -eq $Value) {
        return $null
    }

    return $Value | Select-Object -Property $Property
}

function Invoke-TextCapture
{
    param(
        [string]$Path,
        [string]$Command,
        [string[]]$Arguments
    )

    try {
        $output = & $Command @Arguments 2>&1
        $output | Set-Content -Path $Path -Encoding UTF8
    }
    catch {
        @(
            "capture-failed: $Command $($Arguments -join ' ')",
            "error: $($_.Exception.Message)"
        ) | Set-Content -Path $Path -Encoding UTF8
    }
}

function Get-PnpPropertyMap
{
    param([string]$InstanceId)

    $map = @{}
    if (-not $script:IncludeAllPnpPropertiesForCapture) {
        return $map
    }

    try {
        $properties = @(Get-PnpDeviceProperty -InstanceId $InstanceId -ErrorAction Stop)
        foreach ($property in $properties) {
            if ($null -ne $property.KeyName) {
                $map[[string]$property.KeyName] = $property.Data
            }
        }
    }
    catch {
    }

    return $map
}

function Get-PnpPropertyMapValue
{
    param(
        [hashtable]$Map,
        [string]$KeyName
    )

    if ($Map.ContainsKey($KeyName)) {
        return $Map[$KeyName]
    }

    return $null
}

function Convert-PnpDevice
{
    param([object]$Device)

    $instanceId = [string]$Device.InstanceId
    $properties = Get-PnpPropertyMap -InstanceId $instanceId
    $entity = $null
    $driverRecord = $null
    if ($script:PnpEntityById.ContainsKey($instanceId)) {
        $entity = $script:PnpEntityById[$instanceId]
    }
    if ($script:SignedDriverById.ContainsKey($instanceId)) {
        $driverRecord = $script:SignedDriverById[$instanceId]
    }
    [PSCustomObject]@{
        class = [string]$Device.Class
        friendly_name = [string]$Device.FriendlyName
        instance_id = $instanceId
        problem = [string]$Device.Problem
        status = [string]$Device.Status
        present = [bool]$Device.Present
        manufacturer = if ($null -ne (Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Manufacturer")) { Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Manufacturer" } elseif ($null -ne $entity) { $entity.Manufacturer } else { $null }
        bus_reported_description = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_BusReportedDeviceDesc"
        hardware_ids = if ($null -ne (Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_HardwareIds")) { Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_HardwareIds" } elseif ($null -ne $entity) { $entity.HardwareID } else { $null }
        compatible_ids = if ($null -ne (Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_CompatibleIds")) { Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_CompatibleIds" } elseif ($null -ne $entity) { $entity.CompatibleID } else { $null }
        service = if ($null -ne (Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Service")) { Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Service" } elseif ($null -ne $entity) { $entity.Service } else { $null }
        driver = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Driver"
        driver_version = if ($null -ne (Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverVersion")) { Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverVersion" } elseif ($null -ne $driverRecord) { $driverRecord.DriverVersion } else { $null }
        driver_provider = if ($null -ne (Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverProvider")) { Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverProvider" } elseif ($null -ne $driverRecord) { $driverRecord.DriverProviderName } else { $null }
        driver_inf_path = if ($null -ne (Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverInfPath")) { Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverInfPath" } elseif ($null -ne $driverRecord) { $driverRecord.InfName } else { $null }
        class_guid = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_ClassGuid"
        location_info = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_LocationInfo"
        location_paths = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_LocationPaths"
        parent = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Parent"
        children = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Children"
        siblings = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Siblings"
        address = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Address"
        bus_number = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_BusNumber"
        ui_number = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_UINumber"
        problem_code = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_ProblemCode"
        install_state = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_InstallState"
        removal_policy = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_RemovalPolicy"
        capabilities = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Capabilities"
        power_data = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_PowerData"
        all_properties = $properties
    }
}

function Select-RelevantPnpDevice
{
    param([object]$Device)

    $class = [string]$Device.Class
    $name = [string]$Device.FriendlyName
    $id = [string]$Device.InstanceId
    $haystack = (($class + " " + $name + " " + $id).ToLowerInvariant())

    return (
        ($haystack.Contains("mouse")) -or
        ($haystack.Contains("touchpad")) -or
        ($haystack.Contains("precision touchpad")) -or
        ($haystack.Contains("hid")) -or
        ($haystack.Contains("keyboard")) -or
        ($haystack.Contains("usb")) -or
        ($haystack.Contains("xhci")) -or
        ($haystack.Contains("i2c")) -or
        ($haystack.Contains("serial io")) -or
        ($haystack.Contains("lpss")) -or
        ($haystack.Contains("gpio")) -or
        ($haystack.Contains("spi")) -or
        ($haystack.Contains("acpi")) -or
        ($haystack.Contains("pci")) -or
        ($haystack.Contains("nvme")) -or
        ($haystack.Contains("vmd")) -or
        ($haystack.Contains("raid")) -or
        ($haystack.Contains("ahci")) -or
        ($haystack.Contains("sata")) -or
        ($haystack.Contains("storage")) -or
        ($haystack.Contains("display")) -or
        ($haystack.Contains("graphics")) -or
        ($haystack.Contains("vga")) -or
        ($haystack.Contains("network")) -or
        ($haystack.Contains("ethernet")) -or
        ($haystack.Contains("wi-fi")) -or
        ($haystack.Contains("wifi")) -or
        ($haystack.Contains("bluetooth")) -or
        ($haystack.Contains("audio")) -or
        ($haystack.Contains("battery")) -or
        ($haystack.Contains("thermal"))
    )
}

function Get-RegistrySnapshot
{
    param(
        [string]$RootPath,
        [int]$Depth = 2
    )

    $items = @()
    if (-not (Test-Path -LiteralPath $RootPath)) {
        return $items
    }

    $queue = @([PSCustomObject]@{ path = $RootPath; depth = 0 })
    while ($queue.Count -ne 0) {
        $entry = $queue[0]
        if ($queue.Count -eq 1) {
            $queue = @()
        }
        else {
            $queue = @($queue[1..($queue.Count - 1)])
        }

        try {
            $item = Get-Item -LiteralPath $entry.path -ErrorAction Stop
            $props = Get-ItemProperty -LiteralPath $entry.path -ErrorAction SilentlyContinue
            $plainProps = @{}
            if ($null -ne $props) {
                foreach ($property in $props.PSObject.Properties) {
                    if ($property.Name.StartsWith("PS")) {
                        continue
                    }
                    $plainProps[$property.Name] = $property.Value
                }
            }
            $items += [PSCustomObject]@{
                path = [string]$entry.path
                name = [string]$item.PSChildName
                depth = [int]$entry.depth
                properties = $plainProps
            }
            if ($entry.depth -lt $Depth) {
                foreach ($child in @(Get-ChildItem -LiteralPath $entry.path -ErrorAction SilentlyContinue)) {
                    $queue += [PSCustomObject]@{ path = $child.PSPath; depth = ([int]$entry.depth + 1) }
                }
            }
        }
        catch {
        }
    }

    return $items
}

function New-DeviceFocus
{
    param(
        [object[]]$ExpandedDevices,
        [string]$Name,
        [scriptblock]$Predicate
    )

    return [PSCustomObject]@{
        name = $Name
        count = @($ExpandedDevices | Where-Object $Predicate).Count
        devices = @($ExpandedDevices | Where-Object $Predicate | Select-Object class,friendly_name,instance_id,service,driver_provider,driver_version,location_info,location_paths,hardware_ids,compatible_ids,parent,children)
    }
}

$root = Resolve-Path "."
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
$bundleDir = Join-Path $OutputDir ("capture-" + $timestamp)
$jsonDir = Join-Path $bundleDir "json"
$textDir = Join-Path $bundleDir "text"
$registryDir = Join-Path $bundleDir "registry"
$script:IncludeAllPnpPropertiesForCapture = [bool]$IncludeAllPnpProperties
$script:PnpEntityById = @{}
$script:SignedDriverById = @{}

foreach ($entity in @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue)) {
    if ($null -ne $entity.PNPDeviceID) {
        $script:PnpEntityById[[string]$entity.PNPDeviceID] = $entity
    }
}

foreach ($driverRecord in @(Get-CimInstance Win32_PnPSignedDriver -ErrorAction SilentlyContinue)) {
    if ($null -ne $driverRecord.DeviceID) {
        $script:SignedDriverById[[string]$driverRecord.DeviceID] = $driverRecord
    }
}

New-CleanDirectory -Path $bundleDir
New-Item -ItemType Directory -Path $jsonDir -Force | Out-Null
New-Item -ItemType Directory -Path $textDir -Force | Out-Null
New-Item -ItemType Directory -Path $registryDir -Force | Out-Null

$allPnp = @(Get-PnpDevice)
$presentPnp = @($allPnp | Where-Object { $_.Present })
$relevantPnp = @($presentPnp | Where-Object { Select-RelevantPnpDevice -Device $_ })
$expandedRelevantPnp = @($relevantPnp | ForEach-Object { Convert-PnpDevice -Device $_ })

Write-JsonFile -Path (Join-Path $jsonDir "computer-info.json") -Value ([PSCustomObject]@{
    captured_at = (Get-Date).ToString("o")
    script = "collect-windows-hardware-inventory.ps1"
    repository = $root.Path
    computer_info = Select-ObjectSafe -Value (Get-ComputerInfo) -Property @(
        "WindowsProductName",
        "WindowsVersion",
        "OsHardwareAbstractionLayer",
        "CsManufacturer",
        "CsModel",
        "CsSystemType",
        "BiosVersion",
        "BiosFirmwareType",
        "BiosReleaseDate"
    )
    computer_system = Select-ObjectSafe -Value (Get-CimInstance Win32_ComputerSystem) -Property @("Manufacturer","Model","SystemType","PCSystemType","TotalPhysicalMemory")
    bios = Select-ObjectSafe -Value (Get-CimInstance Win32_BIOS) -Property @("Manufacturer","Name","SMBIOSBIOSVersion","Version","ReleaseDate","SerialNumber")
    baseboard = Select-ObjectSafe -Value (Get-CimInstance Win32_BaseBoard) -Property @("Manufacturer","Product","Version","SerialNumber")
    processor = Select-ObjectSafe -Value (Get-CimInstance Win32_Processor) -Property @("Name","Manufacturer","ProcessorId","Architecture","NumberOfCores","NumberOfLogicalProcessors")
    operating_system = Select-ObjectSafe -Value (Get-CimInstance Win32_OperatingSystem) -Property @("Caption","Version","BuildNumber","OSArchitecture","InstallDate","LastBootUpTime")
})

Write-JsonFile -Path (Join-Path $jsonDir "pnp-present-relevant-expanded.json") -Value $expandedRelevantPnp -Depth 14
Write-JsonFile -Path (Join-Path $jsonDir "pnp-present-summary.json") -Value ($presentPnp | Select-Object Class,FriendlyName,InstanceId,Problem,Status,Present) -Depth 6
Write-JsonFile -Path (Join-Path $jsonDir "pnp-all-summary.json") -Value ($allPnp | Select-Object Class,FriendlyName,InstanceId,Problem,Status,Present) -Depth 6

$focus = @(
    (New-DeviceFocus -ExpandedDevices $expandedRelevantPnp -Name "usb_xhci_and_hubs" -Predicate {
        $text = (($_.class + " " + $_.friendly_name + " " + $_.instance_id + " " + $_.service + " " + ($_.hardware_ids -join " ")) -as [string]).ToLowerInvariant()
        $text.Contains("usb") -or $text.Contains("xhci") -or $text.Contains("hub")
    }),
    (New-DeviceFocus -ExpandedDevices $expandedRelevantPnp -Name "hid_mouse_touchpad_keyboard" -Predicate {
        $text = (($_.class + " " + $_.friendly_name + " " + $_.instance_id + " " + ($_.hardware_ids -join " ") + " " + ($_.compatible_ids -join " ")) -as [string]).ToLowerInvariant()
        $text.Contains("hid") -or $text.Contains("mouse") -or $text.Contains("touchpad") -or $text.Contains("keyboard")
    }),
    (New-DeviceFocus -ExpandedDevices $expandedRelevantPnp -Name "i2c_acpi_gpio_touchpad" -Predicate {
        $text = (($_.class + " " + $_.friendly_name + " " + $_.instance_id + " " + $_.service + " " + ($_.hardware_ids -join " ") + " " + ($_.compatible_ids -join " ")) -as [string]).ToLowerInvariant()
        $text.Contains("i2c") -or $text.Contains("serial io") -or $text.Contains("lpss") -or $text.Contains("gpio") -or $text.Contains("touchpad") -or $text.Contains("acpi")
    }),
    (New-DeviceFocus -ExpandedDevices $expandedRelevantPnp -Name "storage_nvme_vmd_ahci" -Predicate {
        $text = (($_.class + " " + $_.friendly_name + " " + $_.instance_id + " " + $_.service + " " + ($_.hardware_ids -join " ")) -as [string]).ToLowerInvariant()
        $text.Contains("storage") -or $text.Contains("nvme") -or $text.Contains("vmd") -or $text.Contains("raid") -or $text.Contains("ahci") -or $text.Contains("sata")
    }),
    (New-DeviceFocus -ExpandedDevices $expandedRelevantPnp -Name "display_gpu_monitor" -Predicate {
        $text = (($_.class + " " + $_.friendly_name + " " + $_.instance_id + " " + $_.service + " " + ($_.hardware_ids -join " ")) -as [string]).ToLowerInvariant()
        $text.Contains("display") -or $text.Contains("monitor") -or $text.Contains("graphics") -or $text.Contains("vga")
    }),
    (New-DeviceFocus -ExpandedDevices $expandedRelevantPnp -Name "network_bluetooth_audio_power" -Predicate {
        $text = (($_.class + " " + $_.friendly_name + " " + $_.instance_id + " " + $_.service + " " + ($_.hardware_ids -join " ")) -as [string]).ToLowerInvariant()
        $text.Contains("network") -or $text.Contains("ethernet") -or $text.Contains("wi-fi") -or $text.Contains("wifi") -or $text.Contains("bluetooth") -or $text.Contains("audio") -or $text.Contains("battery") -or $text.Contains("thermal")
    })
)
Write-JsonFile -Path (Join-Path $jsonDir "device-focus.json") -Value $focus -Depth 14

Write-JsonFile -Path (Join-Path $jsonDir "pointing-keyboard-usb-storage-display-network.json") -Value ([PSCustomObject]@{
    pointing = @(Get-CimInstance Win32_PointingDevice -ErrorAction SilentlyContinue | Select-Object Name,DeviceID,PNPDeviceID,HardwareType,Manufacturer,Status,NumberOfButtons,PointingType)
    keyboard = @(Get-CimInstance Win32_Keyboard -ErrorAction SilentlyContinue | Select-Object Name,DeviceID,PNPDeviceID,Description,Status)
    usb_controller = @(Get-CimInstance Win32_USBController -ErrorAction SilentlyContinue | Select-Object Name,DeviceID,PNPDeviceID,Manufacturer,Status,ProtocolSupported)
    usb_hub = @(Get-CimInstance Win32_USBHub -ErrorAction SilentlyContinue | Select-Object Name,DeviceID,PNPDeviceID,Description,Status)
    disk_drive = @(Get-CimInstance Win32_DiskDrive -ErrorAction SilentlyContinue | Select-Object Model,InterfaceType,MediaType,SerialNumber,DeviceID,PNPDeviceID,Size,Status)
    pnp_signed_driver = @(Get-CimInstance Win32_PnPSignedDriver -ErrorAction SilentlyContinue | Where-Object {
        $text = (($_.DeviceClass + " " + $_.DeviceName + " " + $_.DeviceID + " " + $_.InfName) -as [string]).ToLowerInvariant()
        ($text.Contains("mouse") -or $text.Contains("touchpad") -or $text.Contains("hid") -or $text.Contains("usb") -or $text.Contains("i2c") -or $text.Contains("storage") -or $text.Contains("nvme") -or $text.Contains("vmd") -or $text.Contains("display") -or $text.Contains("net") -or $text.Contains("bluetooth") -or $text.Contains("audio"))
    })
    video_controller = @(Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object Name,PNPDeviceID,AdapterCompatibility,AdapterRAM,VideoModeDescription,CurrentHorizontalResolution,CurrentVerticalResolution,CurrentBitsPerPixel,DriverVersion,Status)
    network_adapter = @(Get-CimInstance Win32_NetworkAdapter -ErrorAction SilentlyContinue | Where-Object { $_.PhysicalAdapter -eq $true } | Select-Object Name,PNPDeviceID,AdapterType,MACAddress,NetEnabled,Speed,ServiceName,Manufacturer)
    battery = @(Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue | Select-Object Name,DeviceID,PNPDeviceID,BatteryStatus,EstimatedChargeRemaining,Status)
    desktop_monitor = @(Get-CimInstance Win32_DesktopMonitor -ErrorAction SilentlyContinue | Select-Object Name,PNPDeviceID,ScreenWidth,ScreenHeight,Status)
}) -Depth 12

try {
    Write-JsonFile -Path (Join-Path $jsonDir "disk-partition-physical.json") -Value ([PSCustomObject]@{
        disk = @(Get-Disk | Select-Object Number,FriendlyName,SerialNumber,BusType,PartitionStyle,Size,HealthStatus,OperationalStatus,IsBoot,IsSystem)
        partition = @(Get-Partition | Select-Object DiskNumber,PartitionNumber,DriveLetter,Type,GptType,Size,Offset,IsBoot,IsSystem,IsActive)
        physical_disk = @(Get-PhysicalDisk | Select-Object FriendlyName,SerialNumber,MediaType,BusType,CanPool,HealthStatus,OperationalStatus,Size)
        volume = @(Get-Volume | Select-Object DriveLetter,FileSystemLabel,FileSystem,DriveType,HealthStatus,Size,SizeRemaining)
    }) -Depth 10
}
catch {
    Write-JsonFile -Path (Join-Path $jsonDir "disk-partition-physical-error.json") -Value ([PSCustomObject]@{ error = $_.Exception.Message })
}

try {
    Write-JsonFile -Path (Join-Path $jsonDir "allocated-resources-relevant.json") -Value (@(Get-CimInstance Win32_PnPAllocatedResource -ErrorAction Stop | ForEach-Object {
        [PSCustomObject]@{
            antecedent = [string]$_.Antecedent
            dependent = [string]$_.Dependent
        }
    })) -Depth 4
}
catch {
    Write-JsonFile -Path (Join-Path $jsonDir "allocated-resources-error.json") -Value ([PSCustomObject]@{ error = $_.Exception.Message })
}

if ($IncludeRegistrySnapshot) {
    Write-JsonFile -Path (Join-Path $registryDir "enum-usb.json") -Value (Get-RegistrySnapshot -RootPath "HKLM:\SYSTEM\CurrentControlSet\Enum\USB" -Depth 3) -Depth 12
    Write-JsonFile -Path (Join-Path $registryDir "enum-hid.json") -Value (Get-RegistrySnapshot -RootPath "HKLM:\SYSTEM\CurrentControlSet\Enum\HID" -Depth 3) -Depth 12
    Write-JsonFile -Path (Join-Path $registryDir "enum-acpi.json") -Value (Get-RegistrySnapshot -RootPath "HKLM:\SYSTEM\CurrentControlSet\Enum\ACPI" -Depth 3) -Depth 12
    Write-JsonFile -Path (Join-Path $registryDir "enum-pci.json") -Value (Get-RegistrySnapshot -RootPath "HKLM:\SYSTEM\CurrentControlSet\Enum\PCI" -Depth 3) -Depth 12
}

Invoke-TextCapture -Path (Join-Path $textDir "pnputil-connected-ids.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/connected", "/ids")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-connected-relations.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/connected", "/relations")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-connected-drivers.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/connected", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-mouse.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "Mouse", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-hidclass.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "HIDClass", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-keyboard.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "Keyboard", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-usb.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "USB", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-system.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "System", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-display.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "Display", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-net.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "Net", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "driverquery.txt") -Command "driverquery.exe" -Arguments @("/v", "/fo", "csv")
Invoke-TextCapture -Path (Join-Path $textDir "powercfg-devicequery-wake-armed.txt") -Command "powercfg.exe" -Arguments @("/devicequery", "wake_armed")
Invoke-TextCapture -Path (Join-Path $textDir "powercfg-devicequery-all-devices.txt") -Command "powercfg.exe" -Arguments @("/devicequery", "all_devices")

if ($IncludeSetupApiTail) {
    $setupApiPath = Join-Path $env:windir "INF\setupapi.dev.log"
    if (Test-Path -LiteralPath $setupApiPath) {
        Get-Content -LiteralPath $setupApiPath -Tail 12000 | Set-Content -Path (Join-Path $textDir "setupapi.dev.tail.txt") -Encoding UTF8
    }
}

if ($IncludeMsInfo) {
    $msinfoPath = Join-Path $textDir "msinfo32.nfo"
    try {
        Start-Process -FilePath "msinfo32.exe" -ArgumentList @("/nfo", "`"$msinfoPath`"") -Wait -NoNewWindow
    }
    catch {
        "msinfo32 capture failed: $($_.Exception.Message)" | Set-Content -Path (Join-Path $textDir "msinfo32-error.txt") -Encoding UTF8
    }
}

$manifest = [PSCustomObject]@{
    captured_at = (Get-Date).ToString("o")
    bundle_dir = $bundleDir
    relevant_present_pnp_count = $relevantPnp.Count
    present_pnp_count = $presentPnp.Count
    all_pnp_count = $allPnp.Count
    hardware_neutral = $true
    include_registry_snapshot = [bool]$IncludeRegistrySnapshot
    include_all_pnp_properties = [bool]$IncludeAllPnpProperties
    focus = @(
        "USB/xHCI controller, hub, port, interface, and HID mouse parent chains",
        "USB wireless dongle HID/mouse interface IDs and Windows driver binding",
        "Built-in touchpad ACPI/I2C/HID parent chain and Windows driver binding",
        "PCI storage controller identity for NVMe/VMD/AHCI bring-up",
        "Display adapter and monitor/GOP-related hardware identity",
        "Network, Bluetooth, audio, battery, and thermal platform inventory"
    )
    next_step = "Run tools\\summarize-windows-hardware-inventory.ps1 -BundlePath <this capture dir or zip> to generate LimitlessOS driver bring-up targets."
}

Write-JsonFile -Path (Join-Path $bundleDir "manifest.json") -Value $manifest

$zipPath = Join-Path $OutputDir ("windows-hardware-inventory-" + $timestamp + ".zip")
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $bundleDir "*") -DestinationPath $zipPath -Force

Write-Host "windows-hardware-inventory: captured"
Write-Host "  bundle: $bundleDir"
Write-Host "  zip: $zipPath"
Write-Host "  relevant present pnp devices: $($relevantPnp.Count)"
Write-Host "  present pnp devices: $($presentPnp.Count)"
Write-Host "  include setupapi tail: $([bool]$IncludeSetupApiTail)"
Write-Host "  include msinfo: $([bool]$IncludeMsInfo)"
Write-Host "  include registry snapshot: $([bool]$IncludeRegistrySnapshot)"
Write-Host "  include all pnp properties: $([bool]$IncludeAllPnpProperties)"
