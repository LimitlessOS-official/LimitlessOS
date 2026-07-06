param(
    [Parameter(Mandatory = $true)]
    [string]$VmName,

    [string]$DeviceNameContains = "",

    [string]$DeviceUuid = "",

    [string]$VendorId = "",

    [string]$ProductId = "",

    [switch]$ListDevices,

    [switch]$Status,

    [switch]$ClearFilters,

    [switch]$NoFilter,

    [switch]$Usb3Only,

    [switch]$ForceReattach,

    [switch]$AttachNow
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Find-VBoxManage
{
    $candidates = @(
        "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe",
        "C:\Program Files\VirtualBox\VBoxManage.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $fromPath = Get-Command VBoxManage.exe -ErrorAction SilentlyContinue
    if ($null -ne $fromPath) {
        return $fromPath.Source
    }

    throw "VBoxManage.exe was not found. Install VirtualBox or add VBoxManage.exe to PATH."
}

function Parse-UsbHostDevices
{
    param([string[]]$Lines)

    $devices = @()
    $current = @{}

    foreach ($line in $Lines) {
        if ($line.Trim().Length -eq 0) {
            if ($current.Count -ne 0) {
                $devices += [pscustomobject]$current
                $current = @{}
            }
            continue
        }

        $match = [regex]::Match($line, "^\s*([^:]+):\s*(.*)$")
        if ($match.Success) {
            $key = ($match.Groups[1].Value.Trim() -replace "\s+", "")
            $current[$key] = $match.Groups[2].Value.Trim()
        }
    }

    if ($current.Count -ne 0) {
        $devices += [pscustomobject]$current
    }

    return $devices
}

function Get-UsbDeviceProperty
{
    param(
        [Parameter(Mandatory = $true)]$Device,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if ($Device.PSObject.Properties.Name -contains $Name) {
        return [string]$Device.$Name
    }

    return ""
}

function Get-MapValue
{
    param(
        [Parameter(Mandatory = $true)]$Map,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if ($Map.ContainsKey($Name)) {
        return [string]$Map[$Name]
    }

    return ""
}

function Normalize-UsbId
{
    param([string]$Value)

    $normalized = $Value.Trim()
    $normalized = $normalized -replace "^\s*0x", ""
    $normalized = $normalized -replace "\s*\(.*\)\s*$", ""
    return $normalized.ToLowerInvariant()
}

function Test-UsbDeviceTextMatch
{
    param(
        [Parameter(Mandatory = $true)]$Device,
        [Parameter(Mandatory = $true)][string]$Needle
    )

    foreach ($property in $Device.PSObject.Properties) {
        $value = [string]$property.Value
        if ($value.IndexOf($Needle, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            return $true
        }
    }

    return $false
}

function Get-VBoxMachineReadable
{
    param([Parameter(Mandatory = $true)][string]$Name)

    $info = & $script:vbox showvminfo $Name --machinereadable
    $map = @{}
    foreach ($line in $info) {
        $match = [regex]::Match($line, '^([^=]+)="?(.*?)"?$')
        if ($match.Success) {
            $map[$match.Groups[1].Value] = $match.Groups[2].Value
        }
    }
    return $map
}

function Write-UsbFilters
{
    param([Parameter(Mandatory = $true)]$MachineInfo)

    $indices = @()
    foreach ($key in $MachineInfo.Keys) {
        $match = [regex]::Match([string]$key, "^USBFilterName(\d+)$")
        if ($match.Success) {
            $indices += [int]$match.Groups[1].Value
        }
    }

    if ($indices.Count -eq 0) {
        Write-Host "  filters: none"
        return
    }

    foreach ($index in ($indices | Sort-Object)) {
        $name = [string]$MachineInfo["USBFilterName$index"]
        $active = [string]$MachineInfo["USBFilterActive$index"]
        $vendor = [string]$MachineInfo["USBFilterVendorId$index"]
        $product = [string]$MachineInfo["USBFilterProductId$index"]
        Write-Host ("  filter {0}: active {1} vendor {2} product {3} name '{4}'" -f $index, $active, $vendor, $product, $name)
    }
}

function Write-RecentUsbLogLines
{
    param([Parameter(Mandatory = $true)][string]$LogFolder)

    if ((Test-Path $LogFolder) -eq $false) {
        Write-Host "  recent-usb-log: unavailable"
        return
    }

    $log = Get-ChildItem -Path $LogFolder -Filter "VBox.log*" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $log) {
        Write-Host "  recent-usb-log: unavailable"
        return
    }

    Write-Host ("  recent-usb-log-file: {0}" -f $log.FullName)
    $matches = Select-String -Path $log.FullName -Pattern "USB|xhci|XHCI|EHCI|OHCI|0781|5581|SanDisk|Ultra" -CaseSensitive:$false -ErrorAction SilentlyContinue |
        Select-Object -Last 20
    if ($null -eq $matches) {
        Write-Host "  recent-usb-log-lines: none"
        return
    }

    foreach ($line in $matches) {
        Write-Host ("  log: {0}" -f $line.Line.Trim())
    }
}

function Get-UsbFilterIndices
{
    param([Parameter(Mandatory = $true)]$MachineInfo)

    $indices = @()
    foreach ($key in $MachineInfo.Keys) {
        $match = [regex]::Match([string]$key, "^USBFilterName(\d+)$")
        if ($match.Success) {
            $indices += [int]$match.Groups[1].Value
        }
    }

    return ($indices | Sort-Object -Descending)
}

function Invoke-VBoxManageNonTerminating
{
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & $script:vbox @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $output
        Text = ($output -join "`n")
    }
}

$vbox = Find-VBoxManage
$script:vbox = $vbox
$usbConfigWarning = ""

if ($ListDevices) {
    $raw = & $vbox list usbhost
    $devices = Parse-UsbHostDevices -Lines $raw
    if ($devices.Count -eq 0) {
        Write-Host "virtualbox-usb: no host USB devices reported by VBoxManage"
        exit 0
    }

    Write-Host "virtualbox-usb-host-devices:"
    foreach ($device in $devices) {
        $name = Get-UsbDeviceProperty -Device $device -Name "Product"
        $manufacturer = Get-UsbDeviceProperty -Device $device -Name "Manufacturer"
        $uuid = Get-UsbDeviceProperty -Device $device -Name "UUID"
        $vendorId = Get-UsbDeviceProperty -Device $device -Name "VendorId"
        $productId = Get-UsbDeviceProperty -Device $device -Name "ProductId"
        $state = Get-UsbDeviceProperty -Device $device -Name "CurrentState"

        Write-Host ("  product '{0}' manufacturer '{1}' vendor {2} product-id {3} uuid {4} state {5}" -f $name, $manufacturer, $vendorId, $productId, $uuid, $state)
    }
    exit 0
}

if ($Status) {
    $machineInfo = Get-VBoxMachineReadable -Name $VmName
    $vmState = Get-MapValue -Map $machineInfo -Name "VMState"
    $requestedVendorId = $VendorId
    $requestedProductId = $ProductId
    Write-Host "virtualbox-usb-status:"
    Write-Host ("  vm: {0}" -f $VmName)
    Write-Host ("  state: {0}" -f $vmState)
    Write-Host ("  usb: {0}" -f (Get-MapValue -Map $machineInfo -Name "usb"))
    Write-Host ("  ohci: {0}" -f (Get-MapValue -Map $machineInfo -Name "ohci"))
    Write-Host ("  ehci: {0}" -f (Get-MapValue -Map $machineInfo -Name "ehci"))
    Write-Host ("  xhci: {0}" -f (Get-MapValue -Map $machineInfo -Name "xhci"))
    Write-UsbFilters -MachineInfo $machineInfo

    $raw = & $vbox list usbhost
    $devices = Parse-UsbHostDevices -Lines $raw
    $matchedRequestedDeviceCount = 0
    $matchedRequestedCapturedCount = 0
    Write-Host "  host-devices:"
    foreach ($device in $devices) {
        $name = Get-UsbDeviceProperty -Device $device -Name "Product"
        $manufacturer = Get-UsbDeviceProperty -Device $device -Name "Manufacturer"
        $uuid = Get-UsbDeviceProperty -Device $device -Name "UUID"
        $deviceVendorId = Get-UsbDeviceProperty -Device $device -Name "VendorId"
        $deviceProductId = Get-UsbDeviceProperty -Device $device -Name "ProductId"
        $state = Get-UsbDeviceProperty -Device $device -Name "CurrentState"
        Write-Host ("    product '{0}' manufacturer '{1}' vendor {2} product-id {3} uuid {4} state {5}" -f $name, $manufacturer, $deviceVendorId, $deviceProductId, $uuid, $state)

        if (($requestedVendorId.Trim().Length -ne 0) -and ($requestedProductId.Trim().Length -ne 0)) {
            if (((Normalize-UsbId $deviceVendorId) -eq (Normalize-UsbId $requestedVendorId)) -and
                ((Normalize-UsbId $deviceProductId) -eq (Normalize-UsbId $requestedProductId))) {
                ++$matchedRequestedDeviceCount
                if ($state.Equals("Captured", [StringComparison]::OrdinalIgnoreCase)) {
                    ++$matchedRequestedCapturedCount
                }
            }
        }
    }

    if (($requestedVendorId.Trim().Length -ne 0) -and ($requestedProductId.Trim().Length -ne 0)) {
        Write-Host ("  requested-device-matches: {0}" -f $matchedRequestedDeviceCount)
        Write-Host ("  requested-device-captured: {0}" -f $matchedRequestedCapturedCount)
        if (($vmState.Equals("poweroff", [StringComparison]::OrdinalIgnoreCase)) -and ($matchedRequestedCapturedCount -ne 0)) {
            Write-Host "  stale-capture: yes"
            Write-Host "  stale-capture-action: close all VirtualBox windows, unplug the USB device, wait 10 seconds, replug it, then rerun -Status"
            Write-Host "  stale-capture-note: a powered-off VM cannot expose a captured device to guest xHCI ports"
        } else {
            Write-Host "  stale-capture: no"
        }
    }

    if ($machineInfo.ContainsKey("LogFldr")) {
        Write-RecentUsbLogLines -LogFolder ([string]$machineInfo["LogFldr"])
    }
    exit 0
}

if ($ClearFilters) {
    $machineInfo = Get-VBoxMachineReadable -Name $VmName
    $indices = Get-UsbFilterIndices -MachineInfo $machineInfo
    foreach ($index in $indices) {
        $removeResult = Invoke-VBoxManageNonTerminating -Arguments @("usbfilter", "remove", $index.ToString(), "--target", $VmName)
        if ($removeResult.ExitCode -ne 0) {
            throw $removeResult.Text
        }
    }

    Write-Host "virtualbox-usb: filters cleared"
    Write-Host ("  vm: {0}" -f $VmName)
    Write-Host ("  filters-removed: {0}" -f $indices.Count)
    exit 0
}

$usbConfigured = $false
if ($AttachNow -eq $false) {
    if ($Usb3Only) {
        $modifyOutput = & $vbox modifyvm $VmName --usb on --usbehci off --usbxhci on 2>&1
    } else {
        $modifyOutput = & $vbox modifyvm $VmName --usb on --usbxhci on 2>&1
    }
    if ($LASTEXITCODE -ne 0) {
        $usbConfigWarning = ($modifyOutput -join " ")
    } else {
        $usbConfigured = $true
    }
}

$filterAdded = $false
$hasTextFilter = ($DeviceNameContains.Trim().Length -ne 0)
$hasUuidFilter = ($DeviceUuid.Trim().Length -ne 0)
$hasVidPidFilter = (($VendorId.Trim().Length -ne 0) -and ($ProductId.Trim().Length -ne 0))
$shouldAddFilter = (($NoFilter -eq $false) -and ($AttachNow -eq $false) -and ($hasTextFilter -or $hasUuidFilter -or $hasVidPidFilter))
$shouldResolveDevice = (($AttachNow -or $shouldAddFilter) -and ($hasTextFilter -or $hasUuidFilter -or $hasVidPidFilter))
$attachedNow = $false
if ($shouldResolveDevice) {
    $raw = & $vbox list usbhost
    $devices = Parse-UsbHostDevices -Lines $raw
    $match = $null

    foreach ($device in $devices) {
        $deviceUuidValue = Get-UsbDeviceProperty -Device $device -Name "UUID"
        $deviceVendorId = Get-UsbDeviceProperty -Device $device -Name "VendorId"
        $deviceProductId = Get-UsbDeviceProperty -Device $device -Name "ProductId"
        $uuidMatches = ($hasUuidFilter -and $deviceUuidValue.Equals($DeviceUuid, [StringComparison]::OrdinalIgnoreCase))
        $vidPidMatches = ($hasVidPidFilter `
            -and ((Normalize-UsbId $deviceVendorId) -eq (Normalize-UsbId $VendorId)) `
            -and ((Normalize-UsbId $deviceProductId) -eq (Normalize-UsbId $ProductId)))
        $textMatches = ($hasTextFilter -and (Test-UsbDeviceTextMatch -Device $device -Needle $DeviceNameContains))

        if ($uuidMatches) {
            $match = $device
            break
        }

        if ($vidPidMatches) {
            $match = $device
            break
        }

        if ($textMatches) {
            $match = $device
            break
        }
    }

    if ($null -eq $match) {
        throw "No USB host device matched the requested filter. Run with -ListDevices, then use -DeviceNameContains, -DeviceUuid, or -VendorId/-ProductId."
    }

    $vendorId = Normalize-UsbId (Get-UsbDeviceProperty -Device $match -Name "VendorId")
    $productId = Normalize-UsbId (Get-UsbDeviceProperty -Device $match -Name "ProductId")
    $matchedUuid = Get-UsbDeviceProperty -Device $match -Name "UUID"
    $productName = $DeviceNameContains
    if ((Get-UsbDeviceProperty -Device $match -Name "Product").Length -ne 0) {
        $productName = Get-UsbDeviceProperty -Device $match -Name "Product"
    }

    if (($vendorId.Length -eq 0) -or ($productId.Length -eq 0)) {
        throw "Matched USB device '$productName' did not expose VendorId/ProductId."
    }

    if ($shouldAddFilter) {
        $filterName = "LimitlessOS $productName"
        $machineInfo = & $vbox showvminfo $VmName --machinereadable
        $filterExists = $false
        for ($i = 0; $i -lt $machineInfo.Count; ++$i) {
            if (($machineInfo[$i] -match "^USBFilterVendorId\d+=""$vendorId""$") -or
                ($machineInfo[$i] -match "^USBFilterVendorId\d+=""0x$vendorId""$")) {
                for ($j = 0; $j -lt $machineInfo.Count; ++$j) {
                    if (($machineInfo[$j] -match "^USBFilterProductId\d+=""$productId""$") -or
                        ($machineInfo[$j] -match "^USBFilterProductId\d+=""0x$productId""$")) {
                        $filterExists = $true
                        break
                    }
                }
            }
            if ($filterExists) {
                break
            }
        }

        if ($filterExists -eq $false) {
            $filterOutput = & $vbox usbfilter add 0 --target $VmName --name $filterName --vendorid $vendorId --productid $productId 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw ($filterOutput -join "`n")
            }
            $filterAdded = $true
        }
    }

    if ($AttachNow) {
        if ($matchedUuid.Length -eq 0) {
            throw "Matched USB device '$productName' did not expose a UUID for live attach."
        }
        $matchedState = Get-UsbDeviceProperty -Device $match -Name "CurrentState"
        if ($matchedState.Equals("Captured", [StringComparison]::OrdinalIgnoreCase) -and ($ForceReattach -eq $false)) {
            $attachedNow = $true
        } else {
            if ($matchedState.Equals("Captured", [StringComparison]::OrdinalIgnoreCase)) {
                $detachResult = Invoke-VBoxManageNonTerminating -Arguments @("controlvm", $VmName, "usbdetach", $matchedUuid)
                if ($detachResult.ExitCode -ne 0) {
                    if ($detachResult.Text.IndexOf("is not attached to this machine", [StringComparison]::OrdinalIgnoreCase) -lt 0) {
                        throw $detachResult.Text
                    }
                }
                Start-Sleep -Milliseconds 750
            }

            $attachResult = $null
            for ($attempt = 1; $attempt -le 8; ++$attempt) {
                $attachResult = Invoke-VBoxManageNonTerminating -Arguments @("controlvm", $VmName, "usbattach", $matchedUuid)
                if ($attachResult.ExitCode -eq 0) {
                    break
                }
                if ($attachResult.Text.IndexOf("busy with a previous request", [StringComparison]::OrdinalIgnoreCase) -lt 0) {
                    break
                }
                Start-Sleep -Milliseconds (500 * $attempt)
            }
            if (($null -eq $attachResult) -or ($attachResult.ExitCode -ne 0)) {
                throw $attachResult.Text
            }
            $attachedNow = $true
        }
    }
}

Write-Host "virtualbox-usb: configured"
Write-Host ("  vm: {0}" -f $VmName)
Write-Host "  usb: enabled"
Write-Host "  controller: xHCI / USB 3.0"
if ($usbConfigWarning.Length -ne 0) {
    Write-Host ("  usb-config-warning: {0}" -f $usbConfigWarning)
}
Write-Host ("  filter-added: {0}" -f $filterAdded)
Write-Host ("  attached-now: {0}" -f $attachedNow)
Write-Host "LimitlessOS check after boot:"
Write-Host "  ports"
Write-Host "  hwval full"
Write-Host "Expected when VirtualBox exposes the controller:"
Write-Host "  xhci present: yes"
Write-Host "  xhci root ports: >0"
