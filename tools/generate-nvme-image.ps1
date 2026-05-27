param(
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root "dist\nvme-gpt.raw"
}

$sectorBytes = 512
$totalSectors = 32768
$partitionStart = 2048
$partitionSectors = 8192
$partitionLast = $partitionStart + $partitionSectors - 1
$fatReservedSectors = 32
$fatCount = 2
$fatSectors = 64
$sectorsPerCluster = 2
$rootCluster = 2
$nvmeFileCluster = 3
$longFileCluster = 4
$appsDirectoryCluster = 5
$dataDirectoryCluster = 6
$subdirFileCluster = 7
$multiFileClusters = @(8, 9, 10)
$unicodeFileCluster = 11
$deleteFileCluster = 14
$homeDirectoryCluster = 15
$assistantDirectoryCluster = 16
$assistantNoteCluster = 17
$dataStart = $partitionStart + $fatReservedSectors + ($fatCount * $fatSectors)
$rootDirectoryLba = $dataStart + (($rootCluster - 2) * $sectorsPerCluster)
$fileContentLba = $dataStart + (($nvmeFileCluster - 2) * $sectorsPerCluster)
$longFileLba = $dataStart + (($longFileCluster - 2) * $sectorsPerCluster)
$appsDirectoryLba = $dataStart + (($appsDirectoryCluster - 2) * $sectorsPerCluster)
$dataDirectoryLba = $dataStart + (($dataDirectoryCluster - 2) * $sectorsPerCluster)
$subdirFileLba = $dataStart + (($subdirFileCluster - 2) * $sectorsPerCluster)
$multiFileLba = $dataStart + (($multiFileClusters[0] - 2) * $sectorsPerCluster)
$unicodeFileLba = $dataStart + (($unicodeFileCluster - 2) * $sectorsPerCluster)
$deleteFileLba = $dataStart + (($deleteFileCluster - 2) * $sectorsPerCluster)
$homeDirectoryLba = $dataStart + (($homeDirectoryCluster - 2) * $sectorsPerCluster)
$assistantDirectoryLba = $dataStart + (($assistantDirectoryCluster - 2) * $sectorsPerCluster)
$assistantNoteLba = $dataStart + (($assistantNoteCluster - 2) * $sectorsPerCluster)
$fileContent = [System.Text.Encoding]::ASCII.GetBytes("LimitlessOS NVMe GPT fixture`r`n")
$longFileName = "Limitless Long Name.txt"
$longFileContent = [System.Text.Encoding]::ASCII.GetBytes("LimitlessOS long filename fixture`r`n")
$unicodeFileName = "Caf$([char]0x00E9).txt"
$unicodeFileMetaPath = "/Caf\u00E9.txt"
$unicodeFileContent = [System.Text.Encoding]::ASCII.GetBytes("Unicode FAT32 LFN fixture`r`n")
$subdirFileContent = [System.Text.Encoding]::ASCII.GetBytes("Nested FAT32 path fixture`r`n")
$deleteFileContent = [System.Text.Encoding]::ASCII.GetBytes("Delete me through FAT32 proof`r`n")
$assistantNoteContent = [System.Text.Encoding]::ASCII.GetBytes("Assistant action note initial`r`n")
$multiFileContent = New-Object byte[] 2500
for ($index = 0; $index -lt $multiFileContent.Length; $index++) {
    $multiFileContent[$index] = [byte](($index * 17 + 23) -band 0xFF)
}

function Set-U16Le
{
    param([byte[]]$Bytes, [int]$Offset, [uint32]$Value)

    $Bytes[$Offset] = [byte]($Value -band 0xFF)
    $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
}

function Set-U32Le
{
    param([byte[]]$Bytes, [int]$Offset, [uint32]$Value)

    $Bytes[$Offset] = [byte]($Value -band 0xFF)
    $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
    $Bytes[$Offset + 2] = [byte](($Value -shr 16) -band 0xFF)
    $Bytes[$Offset + 3] = [byte](($Value -shr 24) -band 0xFF)
}

function Set-U64Le
{
    param([byte[]]$Bytes, [int]$Offset, [uint64]$Value)

    Set-U32Le -Bytes $Bytes -Offset $Offset -Value ([uint32]($Value -band 4294967295))
    Set-U32Le -Bytes $Bytes -Offset ($Offset + 4) -Value ([uint32](($Value -shr 32) -band 4294967295))
}

function Set-Bytes
{
    param([byte[]]$Bytes, [int]$Offset, [byte[]]$Value)

    [Array]::Copy($Value, 0, $Bytes, $Offset, $Value.Length)
}

function Get-Crc32
{
    param([byte[]]$Bytes, [int]$Offset, [int]$Count)

    [uint32]$crc = 4294967295
    [uint32]$poly = 3988292384

    for ($index = 0; $index -lt $Count; $index++) {
        $crc = [uint32](($crc -bxor [uint32]$Bytes[$Offset + $index]) -band 4294967295)
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 1) -ne 0) {
                $crc = [uint32]((($crc -shr 1) -bxor $poly) -band 4294967295)
            }
            else {
                $crc = [uint32](($crc -shr 1) -band 4294967295)
            }
        }
    }

    return [uint32](($crc -bxor 4294967295) -band 4294967295)
}

$imageBytes = New-Object byte[] ($totalSectors * $sectorBytes)

# Protective MBR.
$mbrOffset = 0x1BE
$imageBytes[$mbrOffset + 4] = 0xEE
Set-U32Le -Bytes $imageBytes -Offset ($mbrOffset + 8) -Value 1
Set-U32Le -Bytes $imageBytes -Offset ($mbrOffset + 12) -Value ([uint32]($totalSectors - 1))
$imageBytes[510] = 0x55
$imageBytes[511] = 0xAA

# GPT partition entry array. Entry 0 remains the Microsoft Basic Data FAT32
# fixture used by the runtime NVMe file tests. Later entries model an M5-style
# installer target mix without granting kernel write authority to them.
$entryArrayBytes = 128 * 128
$entries = New-Object byte[] $entryArrayBytes
$basicDataGuid = [byte[]]@(0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)
$efiSystemGuid = [byte[]]@(0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B)
$microsoftReservedGuid = [byte[]]@(0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE)
$limitlessBootGuid = [byte[]]@(0x4F, 0x53, 0x4F, 0x4C, 0x49, 0x53, 0x49, 0x4D, 0x94, 0x4C, 0x49, 0x4D, 0x49, 0x54, 0x4C, 0x02)
$limitlessRootGuid = [byte[]]@(0x4F, 0x53, 0x4F, 0x4C, 0x49, 0x53, 0x49, 0x4D, 0x94, 0x4C, 0x49, 0x4D, 0x49, 0x54, 0x4C, 0x01)

function Set-GptEntry
{
    param(
        [byte[]]$Entries,
        [int]$Index,
        [byte[]]$TypeGuid,
        [byte[]]$UniqueGuid,
        [uint64]$FirstLba,
        [uint64]$LastLba,
        [string]$Name
    )

    $offset = $Index * 128
    Set-Bytes -Bytes $Entries -Offset $offset -Value $TypeGuid
    Set-Bytes -Bytes $Entries -Offset ($offset + 16) -Value $UniqueGuid
    Set-U64Le -Bytes $Entries -Offset ($offset + 32) -Value $FirstLba
    Set-U64Le -Bytes $Entries -Offset ($offset + 40) -Value $LastLba
    Set-Bytes -Bytes $Entries -Offset ($offset + 56) -Value ([System.Text.Encoding]::Unicode.GetBytes($Name))
}

Set-GptEntry -Entries $entries -Index 0 -TypeGuid $basicDataGuid -UniqueGuid ([byte[]]@(0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x80, 0x57, 0x4E, 0x56, 0x4D, 0x45, 0x30, 0x31)) -FirstLba ([uint64]$partitionStart) -LastLba ([uint64]$partitionLast) -Name "LIMITLESS NVME"
Set-GptEntry -Entries $entries -Index 1 -TypeGuid $efiSystemGuid -UniqueGuid ([byte[]]@(0x21, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x80, 0x57, 0x45, 0x53, 0x50, 0x30, 0x30, 0x31)) -FirstLba 12288 -LastLba 12799 -Name "SYSTEM"
Set-GptEntry -Entries $entries -Index 2 -TypeGuid $microsoftReservedGuid -UniqueGuid ([byte[]]@(0x22, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x80, 0x57, 0x4D, 0x53, 0x52, 0x30, 0x30, 0x31)) -FirstLba 12800 -LastLba 13055 -Name "MSR"
Set-GptEntry -Entries $entries -Index 3 -TypeGuid $basicDataGuid -UniqueGuid ([byte[]]@(0x23, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x80, 0x57, 0x57, 0x49, 0x4E, 0x30, 0x30, 0x31)) -FirstLba 13056 -LastLba 16383 -Name "Windows"
Set-GptEntry -Entries $entries -Index 4 -TypeGuid $limitlessBootGuid -UniqueGuid ([byte[]]@(0x24, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x80, 0x57, 0x4C, 0x4F, 0x53, 0x42, 0x54, 0x31)) -FirstLba 16384 -LastLba 20479 -Name "Limitless Boot"
Set-GptEntry -Entries $entries -Index 5 -TypeGuid $limitlessRootGuid -UniqueGuid ([byte[]]@(0x25, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x80, 0x57, 0x4C, 0x4F, 0x53, 0x52, 0x54, 0x31)) -FirstLba 20480 -LastLba 32733 -Name "Limitless Root"
$entryArrayCrc = Get-Crc32 -Bytes $entries -Offset 0 -Count $entries.Length
Set-Bytes -Bytes $imageBytes -Offset (2 * $sectorBytes) -Value $entries

# Primary GPT header at LBA 1.
$header = New-Object byte[] $sectorBytes
Set-Bytes -Bytes $header -Offset 0 -Value ([System.Text.Encoding]::ASCII.GetBytes("EFI PART"))
Set-U32Le -Bytes $header -Offset 8 -Value 0x00010000
Set-U32Le -Bytes $header -Offset 12 -Value 92
Set-U64Le -Bytes $header -Offset 24 -Value 1
Set-U64Le -Bytes $header -Offset 32 -Value ([uint64]($totalSectors - 1))
Set-U64Le -Bytes $header -Offset 40 -Value 34
Set-U64Le -Bytes $header -Offset 48 -Value ([uint64]($totalSectors - 34))
Set-Bytes -Bytes $header -Offset 56 -Value ([byte[]]@(0x4C, 0x49, 0x4D, 0x49, 0x54, 0x4C, 0x45, 0x53, 0x53, 0x4F, 0x53, 0x4E, 0x56, 0x4D, 0x45, 0x31))
Set-U64Le -Bytes $header -Offset 72 -Value 2
Set-U32Le -Bytes $header -Offset 80 -Value 128
Set-U32Le -Bytes $header -Offset 84 -Value 128
Set-U32Le -Bytes $header -Offset 88 -Value $entryArrayCrc
$headerCrc = Get-Crc32 -Bytes $header -Offset 0 -Count 92
Set-U32Le -Bytes $header -Offset 16 -Value $headerCrc
Set-Bytes -Bytes $imageBytes -Offset $sectorBytes -Value $header

# FAT32 VBR and deterministic file fixtures.
$vbrOffset = $partitionStart * $sectorBytes
$imageBytes[$vbrOffset + 0] = 0xEB
$imageBytes[$vbrOffset + 1] = 0x58
$imageBytes[$vbrOffset + 2] = 0x90
Set-Bytes -Bytes $imageBytes -Offset ($vbrOffset + 3) -Value ([System.Text.Encoding]::ASCII.GetBytes("MSDOS5.0"))
Set-U16Le -Bytes $imageBytes -Offset ($vbrOffset + 11) -Value $sectorBytes
$imageBytes[$vbrOffset + 13] = [byte]$sectorsPerCluster
Set-U16Le -Bytes $imageBytes -Offset ($vbrOffset + 14) -Value $fatReservedSectors
$imageBytes[$vbrOffset + 16] = [byte]$fatCount
Set-U32Le -Bytes $imageBytes -Offset ($vbrOffset + 32) -Value ([uint32]$partitionSectors)
$imageBytes[$vbrOffset + 21] = 0xF8
Set-U32Le -Bytes $imageBytes -Offset ($vbrOffset + 36) -Value ([uint32]$fatSectors)
Set-U16Le -Bytes $imageBytes -Offset ($vbrOffset + 40) -Value 0
Set-U16Le -Bytes $imageBytes -Offset ($vbrOffset + 42) -Value 0
Set-U32Le -Bytes $imageBytes -Offset ($vbrOffset + 44) -Value $rootCluster
Set-U16Le -Bytes $imageBytes -Offset ($vbrOffset + 48) -Value 1
Set-U16Le -Bytes $imageBytes -Offset ($vbrOffset + 50) -Value 6
Set-U32Le -Bytes $imageBytes -Offset ($vbrOffset + 28) -Value ([uint32]$partitionStart)
$imageBytes[$vbrOffset + 64] = 0x80
$imageBytes[$vbrOffset + 66] = 0x29
Set-U32Le -Bytes $imageBytes -Offset ($vbrOffset + 67) -Value 0x4C4F534E
Set-Bytes -Bytes $imageBytes -Offset ($vbrOffset + 71) -Value ([System.Text.Encoding]::ASCII.GetBytes("LIMITLESSOS "))
Set-Bytes -Bytes $imageBytes -Offset ($vbrOffset + 82) -Value ([System.Text.Encoding]::ASCII.GetBytes("FAT32   "))
$imageBytes[$vbrOffset + 510] = 0x55
$imageBytes[$vbrOffset + 511] = 0xAA

function Get-ClusterLba
{
    param([int]$Cluster)

    return $dataStart + (($Cluster - 2) * $sectorsPerCluster)
}

function Set-FatEntry
{
    param([int]$Cluster, [uint32]$Value)

    foreach ($fatIndex in 0..1) {
        $fatOffset = ($partitionStart + $fatReservedSectors + ($fatIndex * $fatSectors)) * $sectorBytes
        Set-U32Le -Bytes $imageBytes -Offset ($fatOffset + ($Cluster * 4)) -Value $Value
    }
}

function Get-ShortNameBytes
{
    param([string]$ShortName)

    $bytes = [System.Text.Encoding]::ASCII.GetBytes($ShortName)
    if ($bytes.Length -ne 11) {
        throw "FAT short name must be exactly 11 bytes: $ShortName"
    }

    return $bytes
}

function Get-LfnChecksum
{
    param([byte[]]$ShortNameBytes)

    [int]$sum = 0
    foreach ($nameByte in $ShortNameBytes) {
        $sum = (((($sum -band 1) * 0x80) + ($sum -shr 1) + [int]$nameByte) -band 0xFF)
    }

    return [byte]$sum
}

function Set-DirectoryEntry
{
    param(
        [int]$DirectoryCluster,
        [int]$EntryIndex,
        [string]$ShortName,
        [byte]$Attributes,
        [int]$StartCluster,
        [int]$Size
    )

    $entryOffset = ((Get-ClusterLba -Cluster $DirectoryCluster) * $sectorBytes) + ($EntryIndex * 32)
    Set-Bytes -Bytes $imageBytes -Offset $entryOffset -Value (Get-ShortNameBytes -ShortName $ShortName)
    $imageBytes[$entryOffset + 11] = $Attributes
    Set-U16Le -Bytes $imageBytes -Offset ($entryOffset + 20) -Value (($StartCluster -shr 16) -band 0xFFFF)
    Set-U16Le -Bytes $imageBytes -Offset ($entryOffset + 26) -Value ($StartCluster -band 0xFFFF)
    Set-U32Le -Bytes $imageBytes -Offset ($entryOffset + 28) -Value ([uint32]$Size)
}

function Set-DeletedDirectorySlot
{
    param(
        [int]$DirectoryCluster,
        [int]$EntryIndex
    )

    $entryOffset = ((Get-ClusterLba -Cluster $DirectoryCluster) * $sectorBytes) + ($EntryIndex * 32)
    $imageBytes[$entryOffset] = 0xE5
}

function Set-LfnEntry
{
    param(
        [int]$DirectoryCluster,
        [int]$EntryIndex,
        [int]$Ordinal,
        [bool]$IsLast,
        [string]$LongName,
        [byte]$Checksum
    )

    $entryOffset = ((Get-ClusterLba -Cluster $DirectoryCluster) * $sectorBytes) + ($EntryIndex * 32)
    $charOffsets = @(1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30)
    $sequence = $Ordinal
    if ($IsLast) {
        $sequence = $sequence -bor 0x40
    }
    $imageBytes[$entryOffset] = [byte]$sequence
    $imageBytes[$entryOffset + 11] = 0x0F
    $imageBytes[$entryOffset + 12] = 0
    $imageBytes[$entryOffset + 13] = $Checksum
    Set-U16Le -Bytes $imageBytes -Offset ($entryOffset + 26) -Value 0

    for ($slot = 0; $slot -lt 13; $slot++) {
        $nameIndex = (($Ordinal - 1) * 13) + $slot
        if ($nameIndex -lt $LongName.Length) {
            $value = [uint32][char]$LongName[$nameIndex]
        }
        elseif ($nameIndex -eq $LongName.Length) {
            $value = 0
        }
        else {
            $value = 0xFFFF
        }

        Set-U16Le -Bytes $imageBytes -Offset ($entryOffset + $charOffsets[$slot]) -Value $value
    }
}

function Set-LongFileEntry
{
    param(
        [int]$DirectoryCluster,
        [int]$EntryIndex,
        [string]$LongName,
        [string]$ShortName,
        [int]$StartCluster,
        [int]$Size
    )

    $shortBytes = Get-ShortNameBytes -ShortName $ShortName
    $checksum = Get-LfnChecksum -ShortNameBytes $shortBytes
    $entryCount = [Math]::Ceiling($LongName.Length / 13.0)
    for ($ordinal = $entryCount; $ordinal -ge 1; $ordinal--) {
        Set-LfnEntry `
            -DirectoryCluster $DirectoryCluster `
            -EntryIndex $EntryIndex `
            -Ordinal $ordinal `
            -IsLast ($ordinal -eq $entryCount) `
            -LongName $LongName `
            -Checksum $checksum
        $EntryIndex++
    }

    Set-DirectoryEntry `
        -DirectoryCluster $DirectoryCluster `
        -EntryIndex $EntryIndex `
        -ShortName $ShortName `
        -Attributes 0x20 `
        -StartCluster $StartCluster `
        -Size $Size
}

Set-FatEntry -Cluster 0 -Value 0x0FFFFFF8
Set-FatEntry -Cluster 1 -Value 4294967295
Set-FatEntry -Cluster $rootCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $nvmeFileCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $longFileCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $appsDirectoryCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $dataDirectoryCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $subdirFileCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $multiFileClusters[0] -Value ([uint32]$multiFileClusters[1])
Set-FatEntry -Cluster $multiFileClusters[1] -Value ([uint32]$multiFileClusters[2])
Set-FatEntry -Cluster $multiFileClusters[2] -Value 0x0FFFFFFF
Set-FatEntry -Cluster $unicodeFileCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $deleteFileCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $homeDirectoryCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $assistantDirectoryCluster -Value 0x0FFFFFFF
Set-FatEntry -Cluster $assistantNoteCluster -Value 0x0FFFFFFF

Set-DirectoryEntry -DirectoryCluster $rootCluster -EntryIndex 0 -ShortName "NVME    TXT" -Attributes 0x20 -StartCluster $nvmeFileCluster -Size $fileContent.Length
Set-LongFileEntry -DirectoryCluster $rootCluster -EntryIndex 1 -LongName $longFileName -ShortName "LIMITL~1TXT" -StartCluster $longFileCluster -Size $longFileContent.Length
Set-DirectoryEntry -DirectoryCluster $rootCluster -EntryIndex 4 -ShortName "APPS       " -Attributes 0x10 -StartCluster $appsDirectoryCluster -Size 0
Set-DirectoryEntry -DirectoryCluster $rootCluster -EntryIndex 5 -ShortName "MULTI   BIN" -Attributes 0x20 -StartCluster $multiFileClusters[0] -Size $multiFileContent.Length
Set-LongFileEntry -DirectoryCluster $rootCluster -EntryIndex 6 -LongName $unicodeFileName -ShortName "CAFE~1  TXT" -StartCluster $unicodeFileCluster -Size $unicodeFileContent.Length
Set-DeletedDirectorySlot -DirectoryCluster $rootCluster -EntryIndex 8
Set-DeletedDirectorySlot -DirectoryCluster $rootCluster -EntryIndex 9
Set-DeletedDirectorySlot -DirectoryCluster $rootCluster -EntryIndex 10
Set-DirectoryEntry -DirectoryCluster $rootCluster -EntryIndex 11 -ShortName "REMOVE  ME " -Attributes 0x20 -StartCluster $deleteFileCluster -Size $deleteFileContent.Length
Set-DirectoryEntry -DirectoryCluster $rootCluster -EntryIndex 12 -ShortName "HOME       " -Attributes 0x10 -StartCluster $homeDirectoryCluster -Size 0
Set-DirectoryEntry -DirectoryCluster $appsDirectoryCluster -EntryIndex 0 -ShortName "DATA       " -Attributes 0x10 -StartCluster $dataDirectoryCluster -Size 0
Set-DirectoryEntry -DirectoryCluster $dataDirectoryCluster -EntryIndex 0 -ShortName "FILE    TXT" -Attributes 0x20 -StartCluster $subdirFileCluster -Size $subdirFileContent.Length
Set-DirectoryEntry -DirectoryCluster $homeDirectoryCluster -EntryIndex 0 -ShortName "ASSIST     " -Attributes 0x10 -StartCluster $assistantDirectoryCluster -Size 0
Set-DirectoryEntry -DirectoryCluster $assistantDirectoryCluster -EntryIndex 0 -ShortName "NOTE    TXT" -Attributes 0x20 -StartCluster $assistantNoteCluster -Size $assistantNoteContent.Length

Set-Bytes -Bytes $imageBytes -Offset ($fileContentLba * $sectorBytes) -Value $fileContent
Set-Bytes -Bytes $imageBytes -Offset ($longFileLba * $sectorBytes) -Value $longFileContent
Set-Bytes -Bytes $imageBytes -Offset ($unicodeFileLba * $sectorBytes) -Value $unicodeFileContent
Set-Bytes -Bytes $imageBytes -Offset ($subdirFileLba * $sectorBytes) -Value $subdirFileContent
Set-Bytes -Bytes $imageBytes -Offset ($deleteFileLba * $sectorBytes) -Value $deleteFileContent
Set-Bytes -Bytes $imageBytes -Offset ($assistantNoteLba * $sectorBytes) -Value $assistantNoteContent
$multiOffset = $multiFileLba * $sectorBytes
for ($index = 0; $index -lt $multiFileContent.Length; $index++) {
    $imageBytes[$multiOffset + $index] = $multiFileContent[$index]
}

$outputDir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}
[System.IO.File]::WriteAllBytes($OutputPath, $imageBytes)

$metaPath = "$OutputPath.meta"
$contentChecksum = Get-Crc32 -Bytes $fileContent -Offset 0 -Count $fileContent.Length
$longFileChecksum = Get-Crc32 -Bytes $longFileContent -Offset 0 -Count $longFileContent.Length
$unicodeFileChecksum = Get-Crc32 -Bytes $unicodeFileContent -Offset 0 -Count $unicodeFileContent.Length
$subdirFileChecksum = Get-Crc32 -Bytes $subdirFileContent -Offset 0 -Count $subdirFileContent.Length
$multiFileChecksum = Get-Crc32 -Bytes $multiFileContent -Offset 0 -Count $multiFileContent.Length
$assistantNoteChecksum = Get-Crc32 -Bytes $assistantNoteContent -Offset 0 -Count $assistantNoteContent.Length
$meta = @(
    "sector-bytes=$sectorBytes",
    "sectors-per-cluster=$sectorsPerCluster",
    "total-sectors=$totalSectors",
    "fat32-start-lba=$partitionStart",
    "fat32-sectors=$partitionSectors",
    "m5-safe-targets=2",
    "m5-forbidden-targets=4",
    "m5-unknown-targets=0",
    "m5-boot-partition=5",
    "m5-root-partition=6",
    "m5-boot-start-lba=16384",
    "m5-root-start-lba=20480",
    "fat32-vbr-signature=0x55AA",
    "nvme-file-path=/NVME.TXT",
    "nvme-file-lba=$fileContentLba",
    "nvme-file-bytes=$($fileContent.Length)",
    ("nvme-file-crc32=0x{0:X8}" -f $contentChecksum),
    "long-file-path=/$longFileName",
    "long-file-lba=$longFileLba",
    "long-file-bytes=$($longFileContent.Length)",
    ("long-file-crc32=0x{0:X8}" -f $longFileChecksum),
    "unicode-file-path=$unicodeFileMetaPath",
    "unicode-file-lba=$unicodeFileLba",
    "unicode-file-bytes=$($unicodeFileContent.Length)",
    ("unicode-file-crc32=0x{0:X8}" -f $unicodeFileChecksum),
    "subdir-file-path=/APPS/DATA/FILE.TXT",
    "subdir-file-lba=$subdirFileLba",
    "subdir-file-bytes=$($subdirFileContent.Length)",
    ("subdir-file-crc32=0x{0:X8}" -f $subdirFileChecksum),
    "multi-file-path=/MULTI.BIN",
    "multi-file-lba=$multiFileLba",
    "multi-file-bytes=$($multiFileContent.Length)",
    ("multi-file-crc32=0x{0:X8}" -f $multiFileChecksum),
    "delete-file-path=/REMOVE.ME",
    "delete-file-lba=$deleteFileLba",
    "delete-file-cluster=$deleteFileCluster",
    "assistant-note-path=/HOME/ASSIST/NOTE.TXT",
    "assistant-note-lba=$assistantNoteLba",
    "assistant-note-bytes=$($assistantNoteContent.Length)",
    ("assistant-note-crc32=0x{0:X8}" -f $assistantNoteChecksum),
    "first-free-cluster=12",
    "second-free-cluster=13",
    ("gpt-header-crc32=0x{0:X8}" -f $headerCrc)
)
Set-Content -Path $metaPath -Value $meta -Encoding Ascii
Write-Host "Generated NVMe GPT image: $OutputPath"
Write-Host "  fat32-start-lba : $partitionStart"
Write-Host "  sectors/cluster : $sectorsPerCluster"
Write-Host "  nvme-file-lba   : $fileContentLba"
Write-Host "  multi-file-lba  : $multiFileLba"
