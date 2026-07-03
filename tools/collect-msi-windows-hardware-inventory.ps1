param(
    [string]$OutputDir = ".\dist\msi-windows-hardware-inventory",
    [switch]$IncludeSetupApiTail
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
        [int]$Depth = 8
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
    [PSCustomObject]@{
        class = [string]$Device.Class
        friendly_name = [string]$Device.FriendlyName
        instance_id = $instanceId
        problem = [string]$Device.Problem
        status = [string]$Device.Status
        present = [bool]$Device.Present
        manufacturer = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Manufacturer"
        bus_reported_description = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_BusReportedDeviceDesc"
        hardware_ids = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_HardwareIds"
        compatible_ids = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_CompatibleIds"
        service = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Service"
        driver = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Driver"
        driver_version = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverVersion"
        driver_provider = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverProvider"
        driver_inf_path = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_DriverInfPath"
        class_guid = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_ClassGuid"
        location_info = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_LocationInfo"
        location_paths = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_LocationPaths"
        parent = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Parent"
        address = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_Address"
        ui_number = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_UINumber"
        problem_code = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_ProblemCode"
        install_state = Get-PnpPropertyMapValue -Map $properties -KeyName "DEVPKEY_Device_InstallState"
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
        ($haystack.Contains("hid")) -or
        ($haystack.Contains("keyboard")) -or
        ($haystack.Contains("usb")) -or
        ($haystack.Contains("xhci")) -or
        ($haystack.Contains("i2c")) -or
        ($haystack.Contains("serial io")) -or
        ($haystack.Contains("lpss")) -or
        ($haystack.Contains("pci")) -or
        ($haystack.Contains("nvme")) -or
        ($haystack.Contains("vmd")) -or
        ($haystack.Contains("raid")) -or
        ($haystack.Contains("storage")) -or
        ($haystack.Contains("display")) -or
        ($haystack.Contains("network")) -or
        ($haystack.Contains("bluetooth"))
    )
}

$root = Resolve-Path "."
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
$bundleDir = Join-Path $OutputDir ("capture-" + $timestamp)
$jsonDir = Join-Path $bundleDir "json"
$textDir = Join-Path $bundleDir "text"

New-CleanDirectory -Path $bundleDir
New-Item -ItemType Directory -Path $jsonDir -Force | Out-Null
New-Item -ItemType Directory -Path $textDir -Force | Out-Null

$allPnp = @(Get-PnpDevice)
$presentPnp = @($allPnp | Where-Object { $_.Present })
$relevantPnp = @($presentPnp | Where-Object { Select-RelevantPnpDevice -Device $_ })

Write-JsonFile -Path (Join-Path $jsonDir "computer-info.json") -Value ([PSCustomObject]@{
    captured_at = (Get-Date).ToString("o")
    script = "collect-msi-windows-hardware-inventory.ps1"
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

Write-JsonFile -Path (Join-Path $jsonDir "pnp-present-relevant-expanded.json") -Value (@($relevantPnp | ForEach-Object { Convert-PnpDevice -Device $_ })) -Depth 12
Write-JsonFile -Path (Join-Path $jsonDir "pnp-present-summary.json") -Value ($presentPnp | Select-Object Class,FriendlyName,InstanceId,Problem,Status,Present) -Depth 6
Write-JsonFile -Path (Join-Path $jsonDir "pnp-all-summary.json") -Value ($allPnp | Select-Object Class,FriendlyName,InstanceId,Problem,Status,Present) -Depth 6

Write-JsonFile -Path (Join-Path $jsonDir "pointing-keyboard-usb-storage-display-network.json") -Value ([PSCustomObject]@{
    pointing = @(Get-CimInstance Win32_PointingDevice -ErrorAction SilentlyContinue)
    keyboard = @(Get-CimInstance Win32_Keyboard -ErrorAction SilentlyContinue)
    usb_controller = @(Get-CimInstance Win32_USBController -ErrorAction SilentlyContinue)
    usb_hub = @(Get-CimInstance Win32_USBHub -ErrorAction SilentlyContinue)
    disk_drive = @(Get-CimInstance Win32_DiskDrive -ErrorAction SilentlyContinue)
    pnp_signed_driver = @(Get-CimInstance Win32_PnPSignedDriver -ErrorAction SilentlyContinue | Where-Object {
        $text = (($_.DeviceClass + " " + $_.DeviceName + " " + $_.DeviceID + " " + $_.InfName) -as [string]).ToLowerInvariant()
        ($text.Contains("mouse") -or $text.Contains("hid") -or $text.Contains("usb") -or $text.Contains("i2c") -or $text.Contains("storage") -or $text.Contains("nvme") -or $text.Contains("vmd") -or $text.Contains("display") -or $text.Contains("net"))
    })
    video_controller = @(Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue)
    network_adapter = @(Get-CimInstance Win32_NetworkAdapter -ErrorAction SilentlyContinue | Where-Object { $_.PhysicalAdapter -eq $true })
})

try {
    Write-JsonFile -Path (Join-Path $jsonDir "disk-partition-physical.json") -Value ([PSCustomObject]@{
        disk = @(Get-Disk)
        partition = @(Get-Partition)
        physical_disk = @(Get-PhysicalDisk)
        volume = @(Get-Volume)
    }) -Depth 8
}
catch {
    Write-JsonFile -Path (Join-Path $jsonDir "disk-partition-physical-error.json") -Value ([PSCustomObject]@{ error = $_.Exception.Message })
}

try {
    Write-JsonFile -Path (Join-Path $jsonDir "allocated-resources-relevant.json") -Value (@(Get-CimInstance Win32_PnPAllocatedResource -ErrorAction Stop | Select-Object Antecedent,Dependent)) -Depth 6
}
catch {
    Write-JsonFile -Path (Join-Path $jsonDir "allocated-resources-error.json") -Value ([PSCustomObject]@{ error = $_.Exception.Message })
}

Invoke-TextCapture -Path (Join-Path $textDir "pnputil-connected-ids.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/connected", "/ids")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-connected-relations.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/connected", "/relations")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-connected-drivers.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/connected", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-mouse.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "Mouse", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-hidclass.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "HIDClass", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-keyboard.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "Keyboard", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-usb.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "USB", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "pnputil-class-system.txt") -Command "pnputil.exe" -Arguments @("/enum-devices", "/class", "System", "/connected", "/ids", "/relations", "/drivers")
Invoke-TextCapture -Path (Join-Path $textDir "driverquery.txt") -Command "driverquery.exe" -Arguments @("/v", "/fo", "csv")

if ($IncludeSetupApiTail) {
    $setupApiPath = Join-Path $env:windir "INF\setupapi.dev.log"
    if (Test-Path -LiteralPath $setupApiPath) {
        Get-Content -LiteralPath $setupApiPath -Tail 6000 | Set-Content -Path (Join-Path $textDir "setupapi.dev.tail.txt") -Encoding UTF8
    }
}

$manifest = [PSCustomObject]@{
    captured_at = (Get-Date).ToString("o")
    bundle_dir = $bundleDir
    relevant_present_pnp_count = $relevantPnp.Count
    present_pnp_count = $presentPnp.Count
    all_pnp_count = $allPnp.Count
    focus = @(
        "USB xHCI controller and port/device parent chains",
        "Wireless USB dongle HID/mouse interfaces",
        "Built-in touchpad I2C HID/ACPI path",
        "VMD/NVMe/storage controller identity",
        "Display adapter and GOP-related hardware identity"
    )
    next_step = "Send this zip bundle back to Codex, then compare Windows hardware IDs/location paths with LimitlessOS hwval xHCI/I2C/NVMe telemetry."
}

Write-JsonFile -Path (Join-Path $bundleDir "manifest.json") -Value $manifest

$zipPath = Join-Path $OutputDir ("msi-windows-hardware-inventory-" + $timestamp + ".zip")
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $bundleDir "*") -DestinationPath $zipPath -Force

Write-Host "msi-windows-hardware-inventory: captured"
Write-Host "  bundle: $bundleDir"
Write-Host "  zip: $zipPath"
Write-Host "  relevant present pnp devices: $($relevantPnp.Count)"
Write-Host "  present pnp devices: $($presentPnp.Count)"
Write-Host "  include setupapi tail: $([bool]$IncludeSetupApiTail)"
