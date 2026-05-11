param(
    [ValidateSet("x86", "x86_64")]
    [string]$Architecture = "x86",

    [ValidateSet("disk", "iso", "uefi")]
    [string]$BootMedia = "disk",

    [ValidateSet("virtio", "e1000e", "e1000")]
    [string]$NetworkDevice = "virtio"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-QemuEdk2CodePath
{
    $candidates = @(
        "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "QEMU x86_64 UEFI firmware was not found. Expected edk2-x86_64-code.fd in the QEMU share directory."
}

function Ensure-NvmeGptImage
{
    param([Parameter(Mandatory = $true)][string]$Root)

    $imagePath = Join-Path $Root "dist\limitlessos-x86_64-nvme-gpt.img"
    $generatorPath = Join-Path $Root "tools\generate-nvme-image.ps1"
    $imageBytes = 16777216

    if (-not (Test-Path $generatorPath)) {
        throw "NVMe GPT image generator not found: $generatorPath"
    }

    & $generatorPath -OutputPath $imagePath
    if (-not $?) {
        throw "Failed to generate NVMe GPT image."
    }
    if ((-not (Test-Path $imagePath)) -or ((Get-Item $imagePath).Length -ne $imageBytes)) {
        throw "NVMe GPT image has an unexpected size."
    }

    return $imagePath
}

$root = Split-Path -Parent $PSScriptRoot
$mediaPath = if ($Architecture -eq "x86") {
    if ($BootMedia -eq "disk") {
        Join-Path $root "dist\\limitlessos.img"
    }
    elseif ($BootMedia -eq "iso") {
        Join-Path $root "dist\\limitlessos.iso"
    }
    else {
        throw "UEFI boot media is only available on the x86_64 lane."
    }
} else {
    if ($BootMedia -eq "disk") {
        Join-Path $root "dist\\limitlessos-x86_64.img"
    }
    elseif ($BootMedia -eq "iso") {
        Join-Path $root "dist\\limitlessos-x86_64.iso"
    }
    else {
        Join-Path $root "dist\\limitlessos-x86_64-uefi.img"
    }
}

if (-not (Test-Path $mediaPath)) {
    throw "Build media not found. Run .\\tools\\build.ps1 -Architecture $Architecture first."
}

$qemu = if ($Architecture -eq "x86") {
    Get-Command qemu-system-i386 -ErrorAction SilentlyContinue
} else {
    Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
}

if (-not $qemu -and ($Architecture -eq "x86")) {
    $qemu = Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
}

if (-not $qemu) {
    $fallbackPath = if ($Architecture -eq "x86") {
        "C:\\Program Files\\qemu\\qemu-system-i386.exe"
    } else {
        "C:\\Program Files\\qemu\\qemu-system-x86_64.exe"
    }
    if (Test-Path $fallbackPath) {
        $qemu = @{ Source = $fallbackPath }
    }
}

if (-not $qemu) {
    throw "QEMU is not installed or not on PATH."
}

$arguments = @("-m", "128M")

if (($Architecture -eq "x86_64") -and ($BootMedia -ne "disk")) {
    $firmwarePath = Get-QemuEdk2CodePath
    $nvmeGptPath = Ensure-NvmeGptImage -Root $root
    $networkDeviceArgument = if ($NetworkDevice -eq "e1000e") {
        "e1000e,netdev=net0,mac=52:54:00:12:34:56"
    }
    elseif ($NetworkDevice -eq "e1000") {
        "e1000,netdev=net0,mac=52:54:00:12:34:56"
    }
    else {
        "virtio-net-pci,netdev=net0,disable-legacy=on,mac=52:54:00:12:34:56"
    }
    $arguments += @(
        "-display", "default",
        "-serial", "none",
        "-debugcon", "stdio",
        "-global", "isa-debugcon.iobase=0xe9",
        "-monitor", "none",
        "-machine", "q35",
        "-drive", "if=pflash,format=raw,readonly=on,file=$firmwarePath",
        "-device", "uefi-vars-x64",
        "-drive", "if=none,id=nvmeprobe,format=raw,snapshot=on,file=$nvmeGptPath",
        "-device", "nvme,drive=nvmeprobe,serial=LIMITLESSOSNVME,bootindex=3",
        "-netdev", "user,id=net0",
        "-device", $networkDeviceArgument,
    )

    if ($BootMedia -eq "uefi") {
        $arguments += @(
            "-device", "qemu-xhci,id=xhci",
            "-device", "usb-kbd,bus=xhci.0",
            "-device", "usb-mouse,bus=xhci.0",
            "-drive", "if=none,id=usbstick,format=raw,file=$mediaPath",
            "-device", "usb-storage,bus=xhci.0,drive=usbstick,removable=true,bootindex=1"
        )
    }
    else {
        $arguments += @(
            "-device", "qemu-xhci,id=xhci",
            "-device", "usb-kbd,bus=xhci.0",
            "-device", "usb-mouse,bus=xhci.0",
            "-drive", "if=none,id=cdrom,media=cdrom,file=$mediaPath",
            "-device", "ide-cd,drive=cdrom,bootindex=1"
        )
    }
}
else {
    $arguments += @(
        "-display", "default",
        "-serial", "stdio",
        "-monitor", "none"
    )

    if ($BootMedia -eq "disk") {
        $arguments += @(
            "-drive", "format=raw,file=$mediaPath,if=ide,index=0"
        )
    }
    else {
        $arguments += @(
            "-boot", "d",
            "-drive", "file=$mediaPath,media=cdrom,if=ide,index=1"
        )
    }
}

& $qemu.Source @arguments
