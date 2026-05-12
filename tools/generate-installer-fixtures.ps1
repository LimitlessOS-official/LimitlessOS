param(
    [string]$OutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "installer-common.ps1")

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "dist\m5-installer-fixtures"
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function New-GuidString
{
    param([string]$Suffix)

    return "4c4f534f-354d-4500-9000-$Suffix"
}

function New-Part
{
    param(
        [string]$TypeGuid,
        [int]$FirstLba,
        [int]$LastLba,
        [string]$Name,
        [string]$Signature = "NONE",
        [string]$FileSystemLabel = "",
        [string]$Role = ""
    )

    return [PSCustomObject]@{
        TypeGuid = $TypeGuid
        UniqueGuid = [Guid]::NewGuid().ToString()
        FirstLba = $FirstLba
        LastLba = $LastLba
        Name = $Name
        Signature = $Signature
        FileSystemLabel = $FileSystemLabel
        Role = $Role
    }
}

$fixtures = @()

$cleanPath = Join-Path $OutputDir "clean-unallocated-target.img"
New-M5GptDiskImage -OutputPath $cleanPath -TotalSectors 131072 -Partitions @()
$fixtures += [PSCustomObject]@{
    name = "clean-unallocated-target"
    path = $cleanPath
    purpose = "Clean GPT disk with no allocated partitions; dry-run may propose a safe user-created target, but write mode must still require explicit selection and confirmation."
}

$windowsPath = Join-Path $OutputDir "windows-like.img"
New-M5GptDiskImage -OutputPath $windowsPath -TotalSectors 131072 -Partitions @(
    (New-Part -TypeGuid $Script:M5EfiSystemTypeGuid -FirstLba 2048 -LastLba 4095 -Name "SYSTEM" -Signature "FAT32" -FileSystemLabel "SYSTEM"),
    (New-Part -TypeGuid $Script:M5MicrosoftReservedGuid -FirstLba 4096 -LastLba 8191 -Name "Microsoft reserved"),
    (New-Part -TypeGuid $Script:M5MicrosoftBasicDataGuid -FirstLba 8192 -LastLba 65535 -Name "Windows" -Signature "NTFS"),
    (New-Part -TypeGuid $Script:M5WindowsRecoveryGuid -FirstLba 65536 -LastLba 81919 -Name "Windows RE tools" -Signature "NTFS")
)
$fixtures += [PSCustomObject]@{
    name = "windows-like"
    path = $windowsPath
    purpose = "Windows-like ESP/MSR/NTFS/Recovery fixture; every partition must be forbidden."
}

$unknownFatPath = Join-Path $OutputDir "unknown-fat32.img"
New-M5GptDiskImage -OutputPath $unknownFatPath -TotalSectors 131072 -Partitions @(
    (New-Part -TypeGuid $Script:M5MicrosoftBasicDataGuid -FirstLba 2048 -LastLba 32767 -Name "USB-DATA" -Signature "FAT32" -FileSystemLabel "USB-DATA")
)
$fixtures += [PSCustomObject]@{
    name = "unknown-fat32"
    path = $unknownFatPath
    purpose = "Unknown FAT32 fixture; must be forbidden because it lacks a LimitlessOS marker or approved label."
}

$unknownGptPath = Join-Path $OutputDir "unknown-gpt.img"
New-M5GptDiskImage -OutputPath $unknownGptPath -TotalSectors 131072 -Partitions @(
    (New-Part -TypeGuid "01234567-89ab-cdef-8123-456789abcdef" -FirstLba 2048 -LastLba 32767 -Name "UNKNOWN-DATA")
)
$fixtures += [PSCustomObject]@{
    name = "unknown-gpt"
    path = $unknownGptPath
    purpose = "Unknown GPT type fixture; must be refused."
}

$validPath = Join-Path $OutputDir "valid-limitless-target.img"
New-M5GptDiskImage -OutputPath $validPath -TotalSectors 196608 -Partitions @(
    (New-Part -TypeGuid $Script:M5EfiSystemTypeGuid -FirstLba 2048 -LastLba 4095 -Name "SYSTEM" -Signature "FAT32" -FileSystemLabel "SYSTEM"),
    (New-Part -TypeGuid $Script:M5MicrosoftReservedGuid -FirstLba 4096 -LastLba 8191 -Name "Microsoft reserved"),
    (New-Part -TypeGuid $Script:M5MicrosoftBasicDataGuid -FirstLba 8192 -LastLba 65535 -Name "Windows" -Signature "NTFS"),
    (New-Part -TypeGuid $Script:M5LimitlessBootTypeGuid -FirstLba 65536 -LastLba 81919 -Name "LIMITLESS-BOOT" -Signature "LIMITLESS" -Role "boot"),
    (New-Part -TypeGuid $Script:M5LimitlessTypeGuid -FirstLba 81920 -LastLba 180223 -Name "LIMITLESS-ROOT" -Signature "LIMITLESS" -Role "root")
)
$fixtures += [PSCustomObject]@{
    name = "valid-limitless-target"
    path = $validPath
    purpose = "Windows-like disk plus dedicated LimitlessOS boot/root targets; installer may write only partitions 4 and 5 after confirmation and scoped write/format authority."
}

$manifest = [PSCustomObject]@{
    generatedAt = (Get-Date).ToString("o")
    fixtures = @($fixtures | ForEach-Object {
        [PSCustomObject]@{
            name = $_.name
            path = $_.path
            relativePath = $_.path.Substring($root.Length + 1)
            sha256 = Get-M5ImageHash -Path $_.path
            purpose = $_.purpose
        }
    })
    limitlessTypeGuid = $Script:M5LimitlessTypeGuid
    limitlessBootTypeGuid = $Script:M5LimitlessBootTypeGuid
}

$manifestPath = Join-Path $OutputDir "fixtures.json"
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "Generated M5 installer fixtures:"
foreach ($fixture in $manifest.fixtures) {
    Write-Host ("  {0}: {1}" -f $fixture.name, $fixture.relativePath)
}
Write-Host "  manifest: $($manifestPath.Substring($root.Length + 1))"
