# Runs the LimitlessOS Product desktop in a QEMU window for day-to-day use.
#
# - Rebuilds first when any source under boot/, kernel/, apps/, packages/, or
#   tools/ is newer than the last build (or when -Rebuild is given).
# - Boots the UEFI USB image with the same virtual hardware the UEFI QEMU gate
#   uses: q35, OVMF, xHCI keyboard/mouse, NVMe, and virtio networking.
# - Keeps a persistent NVMe disk in %LOCALAPPDATA%\LimitlessOS so the local
#   account and files survive between runs. -Fresh starts over with a new disk.
#
# Click inside the window to capture the mouse; press Ctrl+Alt+G to release it.
# QEMU is a development convenience: it does not replace validation on physical
# hardware (see docs/real-binary-gate.md).

param(
    [switch]$Rebuild,
    [switch]$Fresh,
    [switch]$NoBuild,
    [ValidateRange(128, 8192)]
    [int]$MemoryMiB = 512,
    [ValidateSet("virtio", "e1000e")]
    [string]$NetworkDevice = "virtio"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "toolchain.ps1")

$root = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $root "dist"
$usbImage = Join-Path $distDir "limitlessos-x86_64-uefi.img"
$kernelPath = Join-Path $distDir "KERNEL64.BIN"
$stateDir = Join-Path $env:LOCALAPPDATA "LimitlessOS"
$diskPath = Join-Path $stateDir "desktop-nvme.img"
$logPath = Join-Path $stateDir "desktop-debug.log"
$firmwarePath = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"

function Test-BuildIsStale
{
    if (-not (Test-Path $usbImage) -or -not (Test-Path $kernelPath)) {
        return $true
    }

    $builtAt = (Get-Item $kernelPath).LastWriteTimeUtc
    $sourceDirs = @("boot", "kernel", "apps", "packages", "tools") | ForEach-Object { Join-Path $root $_ }
    $newer = Get-ChildItem -Path $sourceDirs -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTimeUtc -gt $builtAt } |
        Select-Object -First 1
    return ($null -ne $newer)
}

# Two VMs writing the same NVMe disk image would corrupt it.
$running = Get-Process qemu-system-x86_64 -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowTitle -like "*LimitlessOS*" }
if ($running) {
    Write-Host "LimitlessOS is already running (QEMU process $($running[0].Id)). Close that window first."
    return
}

if (-not $NoBuild -and ($Rebuild -or (Test-BuildIsStale))) {
    Write-Host "Sources changed since the last build; building LimitlessOS (about a minute)..."
    & (Join-Path $PSScriptRoot "build.ps1") -Architecture x86_64 -BuildProfile Product
    if (-not $?) {
        throw "Build failed; fix the errors above and run again."
    }
}

if (-not (Test-Path $usbImage)) {
    throw "No UEFI image at $usbImage. Run without -NoBuild to build it."
}
if (-not (Test-Path $firmwarePath)) {
    throw "QEMU UEFI firmware not found at $firmwarePath. Install QEMU: winget install --id SoftwareFreedomConservancy.QEMU -e"
}
$qemu = Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
if (-not $qemu) {
    throw "qemu-system-x86_64 not found. Install QEMU: winget install --id SoftwareFreedomConservancy.QEMU -e"
}

New-Item -ItemType Directory -Force -Path $stateDir | Out-Null
if ($Fresh -or -not (Test-Path $diskPath)) {
    Write-Host "Creating a fresh LimitlessOS disk at $diskPath"
    & (Join-Path $PSScriptRoot "generate-nvme-image.ps1") -OutputPath $diskPath | Out-Null
    if (-not (Test-Path $diskPath)) {
        throw "Failed to create the desktop NVMe disk."
    }
}

$networkDeviceArgument = if ($NetworkDevice -eq "e1000e") {
    "e1000e,netdev=net0,mac=52:54:00:12:34:56"
}
else {
    "virtio-net-pci,netdev=net0,disable-legacy=on,mac=52:54:00:12:34:56"
}

# QEMU opens the USB image read-write; boot from a per-run copy so a running
# desktop never locks or modifies the build output in dist/.
$bootCopy = Join-Path $stateDir "desktop-boot.img"
Copy-Item -Force $usbImage $bootCopy

$arguments = @(
    "-name", "LimitlessOS",
    "-m", "$($MemoryMiB)M",
    "-machine", "q35",
    "-display", "default",
    "-monitor", "none",
    "-serial", "none",
    "-debugcon", "file:$logPath",
    "-global", "isa-debugcon.iobase=0xe9",
    "-drive", "if=pflash,format=raw,readonly=on,file=$firmwarePath",
    "-device", "uefi-vars-x64",
    "-drive", "if=none,id=nvmedisk,format=raw,file=$diskPath",
    "-device", "nvme,drive=nvmedisk,serial=LIMITLESSOSNVME,bootindex=3",
    "-netdev", "user,id=net0",
    "-device", $networkDeviceArgument,
    "-device", "qemu-xhci,id=xhci",
    "-device", "usb-kbd,bus=xhci.0",
    "-device", "usb-mouse,bus=xhci.0",
    "-drive", "if=none,id=usbstick,format=raw,file=$bootCopy",
    "-device", "usb-storage,bus=xhci.0,drive=usbstick,removable=true,bootindex=1"
)

Write-Host "Starting LimitlessOS. Click the window to capture the mouse; Ctrl+Alt+G releases it."
Write-Host "Kernel log: $logPath"
Start-Process -FilePath $qemu.Source -ArgumentList ($arguments | ForEach-Object { if ($_ -match '\s') { '"{0}"' -f $_ } else { $_ } })
