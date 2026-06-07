param(
    [Parameter(Mandatory = $true)]
    [string]$InputImagePath,

    [Parameter(Mandatory = $true)]
    [string]$OutputIsoPath,

    [ValidateSet("x86", "x86_64")]
    [string]$Architecture = "x86",

    [ValidateSet("bios", "uefi")]
    [string]$BootMode = "bios",

    [string]$BootImagePath,

    [string]$StageSourcePath,

    [string]$VolumeId
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:ComStreamTypeName = "LimitlessOS.Tools.ComStreamWriter"

function Ensure-ComStreamWriter
{
    if ($script:ComStreamTypeName -as [type]) {
        return
    }

    Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

namespace LimitlessOS.Tools
{
    public static class ComStreamWriter
    {
        public static void SaveToFile(object streamObject, string outputPath)
        {
            IStream stream = (IStream)streamObject;
            byte[] buffer = new byte[65536];
            IntPtr bytesReadPtr = Marshal.AllocCoTaskMem(sizeof(int));

            try
            {
                using (FileStream file = new FileStream(outputPath, FileMode.Create, FileAccess.Write))
                {
                    while (true)
                    {
                        Marshal.WriteInt32(bytesReadPtr, 0);
                        stream.Read(buffer, buffer.Length, bytesReadPtr);
                        int bytesRead = Marshal.ReadInt32(bytesReadPtr);
                        if (bytesRead <= 0)
                        {
                            break;
                        }

                        file.Write(buffer, 0, bytesRead);
                    }
                }
            }
            finally
            {
                Marshal.FreeCoTaskMem(bytesReadPtr);
            }
        }
    }
}
"@
}

function Build-ReadmeText
{
    param(
        [string]$ArchitectureName,
        [string]$BootModeName
    )

    $formatLine = if ($BootModeName -eq "uefi") {
        "Image format: UEFI El Torito optical image generated through Windows IMAPI2."
    }
    else {
        "Image format: BIOS El Torito optical image generated through Windows IMAPI2."
    }

    $bootGuidance = if ($BootModeName -eq "uefi") {
        @(
            "- This ISO is intended for modern UEFI-first 64-bit systems.",
            "- This ISO has only a UEFI El Torito boot entry; it is not legacy-BIOS bootable.",
            "- BOOTIMG.IMG is staged for inspection as the UEFI FAT boot image copy and is not wired as a second BIOS El Torito boot entry.",
            "- The matching removable FAT image can still be written directly to USB for firmware-style boot testing."
        )
    }
    else {
        @(
            "- The current optical path is BIOS bootable today.",
            "- A UEFI install-media lane is still planned for modern-only laptops and desktops.",
            "- BOOTIMG.IMG is the BIOS El Torito boot image used by this ISO."
        )
    }

    $lines = @(
        "LimitlessOS boot media",
        "",
        "Architecture: $ArchitectureName",
        "Boot mode: $BootModeName",
        $formatLine,
        "",
        "Media guidance:",
        "- Burn this ISO to DVD or CD with a normal disc-writing tool.",
        "- Write the matching raw .img file directly to USB when you want disk-style media.",
        "",
        "Compatibility notes:"
    )
    $lines += $bootGuidance
    $lines += @(
        "- The long-term product target remains a universal 32-bit and 64-bit installer."
    )

    return (($lines -join "`r`n") + "`r`n")
}

if (-not (Test-Path $InputImagePath)) {
    throw "Input image not found: $InputImagePath"
}

$resolvedInputImagePath = (Resolve-Path $InputImagePath).Path
$resolvedBootImagePath = if ($BootImagePath) {
    if (-not (Test-Path $BootImagePath)) {
        throw "Boot image not found: $BootImagePath"
    }

    (Resolve-Path $BootImagePath).Path
}
else {
    $resolvedInputImagePath
}

if ($StageSourcePath) {
    if (-not (Test-Path $StageSourcePath)) {
        throw "Stage source path not found: $StageSourcePath"
    }

    $resolvedStageSourcePath = (Resolve-Path $StageSourcePath).Path
}

$outputDirectory = Split-Path -Parent $OutputIsoPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$root = Split-Path -Parent $PSScriptRoot
$stageDirectory = Join-Path $root ("build\\iso-stage-" + $Architecture)
$readmePath = Join-Path $stageDirectory "README.TXT"
$bootImageCopyPath = Join-Path $stageDirectory "BOOTIMG.IMG"

if (Test-Path $stageDirectory) {
    Remove-Item -Recurse -Force $stageDirectory
}

New-Item -ItemType Directory -Force -Path $stageDirectory | Out-Null

if (-not $VolumeId) {
    $VolumeId = if ($Architecture -eq "x86") { "LIMITLESSOS_X86" } else { "LIMITLESSOS_X64" }
}

if ($StageSourcePath) {
    foreach ($item in Get-ChildItem -Force -LiteralPath $resolvedStageSourcePath) {
        Copy-Item -Recurse -Force -LiteralPath $item.FullName -Destination $stageDirectory
    }
}

Set-Content -Path $readmePath -Value (Build-ReadmeText -ArchitectureName $Architecture -BootModeName $BootMode) -Encoding Ascii
Copy-Item -Force -LiteralPath $resolvedBootImagePath -Destination $bootImageCopyPath

Ensure-ComStreamWriter

$bootStream = $null
$bootOptions = $null
$fileSystemImage = $null
$resultImage = $null

try {
    $bootStream = New-Object -ComObject ADODB.Stream
    $bootStream.Type = 1
    $bootStream.Open()
    $bootStream.LoadFromFile($resolvedBootImagePath)

    $bootOptions = New-Object -ComObject IMAPI2FS.BootOptions
    $bootOptions.Manufacturer = "LimitlessOS"
    $bootOptions.PlatformId = if ($BootMode -eq "uefi") { 0xEF } else { 0 }
    $bootOptions.Emulation = 0
    $bootOptions.AssignBootImage($bootStream)

    $fileSystemImage = New-Object -ComObject IMAPI2FS.MsftFileSystemImage
    $fileSystemImage.FileSystemsToCreate = 1
    $fileSystemImage.VolumeName = $VolumeId
    $fileSystemImage.UseRestrictedCharacterSet = $true
    $fileSystemImage.Root.AddTree($stageDirectory, $false)
    $fileSystemImage.BootImageOptions = $bootOptions

    $resultImage = $fileSystemImage.CreateResultImage()
    [LimitlessOS.Tools.ComStreamWriter]::SaveToFile($resultImage.ImageStream, $OutputIsoPath)
}
finally {
    if ($bootStream) {
        $bootStream.Close()
    }
}

$isoFile = Get-Item -LiteralPath $OutputIsoPath

Write-Host "Generated ISO image"
Write-Host "  architecture : $Architecture"
Write-Host "  boot mode    : $BootMode"
Write-Host "  input image  : $resolvedInputImagePath"
Write-Host "  boot image   : $resolvedBootImagePath"
Write-Host "  output iso   : $OutputIsoPath"
Write-Host "  volume id    : $VolumeId"
Write-Host "  size         : $($isoFile.Length) bytes"
