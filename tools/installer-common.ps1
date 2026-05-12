Set-StrictMode -Version Latest

$Script:M5SectorBytes = 512
$Script:M5LimitlessTypeGuid = "4c4f534f-5349-4d49-944c-494d49544c01"
$Script:M5LimitlessBootTypeGuid = "4c4f534f-5349-4d49-944c-494d49544c02"
$Script:M5EfiSystemTypeGuid = "c12a7328-f81f-11d2-ba4b-00a0c93ec93b"
$Script:M5MicrosoftBasicDataGuid = "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7"
$Script:M5MicrosoftReservedGuid = "e3c9e316-0b5c-4db8-817d-f92df00215ae"
$Script:M5WindowsRecoveryGuid = "de94bba4-06d1-4d40-a16a-bfd50179d6ac"
$Script:M5ApprovedLabels = @("LIMITLESS-BOOT", "LIMITLESS-ROOT", "LIMITLESSOS TARGET")
$Script:M5MarkerText = "LIMITLESSOS_TARGET_V1"
$Script:M5ConfirmationPrefix = "INSTALL-LIMITLESSOS-M5:"

function Get-M5Crc32
{
    param([byte[]]$Bytes, [int]$Offset = 0, [int]$Count = -1)

    if ($Count -lt 0) {
        $Count = $Bytes.Length - $Offset
    }

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

function Set-M5U16Le
{
    param([byte[]]$Bytes, [int]$Offset, [uint32]$Value)

    $Bytes[$Offset] = [byte]($Value -band 0xFF)
    $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
}

function Set-M5U32Le
{
    param([byte[]]$Bytes, [int]$Offset, [uint32]$Value)

    $Bytes[$Offset] = [byte]($Value -band 0xFF)
    $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
    $Bytes[$Offset + 2] = [byte](($Value -shr 16) -band 0xFF)
    $Bytes[$Offset + 3] = [byte](($Value -shr 24) -band 0xFF)
}

function Set-M5U64Le
{
    param([byte[]]$Bytes, [int]$Offset, [uint64]$Value)

    Set-M5U32Le -Bytes $Bytes -Offset $Offset -Value ([uint32]($Value -band 4294967295))
    Set-M5U32Le -Bytes $Bytes -Offset ($Offset + 4) -Value ([uint32](($Value -shr 32) -band 4294967295))
}

function Get-M5U16Le
{
    param([byte[]]$Bytes, [int]$Offset)

    return [uint16](([uint32]$Bytes[$Offset]) -bor (([uint32]$Bytes[$Offset + 1]) -shl 8))
}

function Get-M5U32Le
{
    param([byte[]]$Bytes, [int]$Offset)

    return [uint32](([uint32]$Bytes[$Offset]) -bor (([uint32]$Bytes[$Offset + 1]) -shl 8) -bor (([uint32]$Bytes[$Offset + 2]) -shl 16) -bor (([uint32]$Bytes[$Offset + 3]) -shl 24))
}

function Get-M5U64Le
{
    param([byte[]]$Bytes, [int]$Offset)

    [uint64]$low = Get-M5U32Le -Bytes $Bytes -Offset $Offset
    [uint64]$high = Get-M5U32Le -Bytes $Bytes -Offset ($Offset + 4)
    return [uint64]($low -bor ($high -shl 32))
}

function ConvertTo-M5GptGuidBytes
{
    param([string]$Guid)

    return ([Guid]$Guid).ToByteArray()
}

function ConvertFrom-M5GptGuidBytes
{
    param([byte[]]$Bytes, [int]$Offset)

    $guidBytes = New-Object byte[] 16
    [Array]::Copy($Bytes, $Offset, $guidBytes, 0, 16)
    return ([Guid]::new($guidBytes)).ToString()
}

function Set-M5Bytes
{
    param([byte[]]$Bytes, [int]$Offset, [byte[]]$Value)

    [Array]::Copy($Value, 0, $Bytes, $Offset, $Value.Length)
}

function Set-M5Ascii
{
    param([byte[]]$Bytes, [int]$Offset, [string]$Value)

    Set-M5Bytes -Bytes $Bytes -Offset $Offset -Value ([System.Text.Encoding]::ASCII.GetBytes($Value))
}

function Set-M5Utf16Name
{
    param([byte[]]$Bytes, [int]$Offset, [string]$Value)

    $nameBytes = [System.Text.Encoding]::Unicode.GetBytes($Value)
    $length = [Math]::Min($nameBytes.Length, 72)
    [Array]::Copy($nameBytes, 0, $Bytes, $Offset, $length)
}

function New-M5EmptyImageBytes
{
    param([int]$TotalSectors)

    return New-Object byte[] ($TotalSectors * $Script:M5SectorBytes)
}

function New-M5GptDiskImage
{
    param(
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [object[]]$Partitions = @(),
        [int]$TotalSectors = 131072
    )

    $bytes = New-M5EmptyImageBytes -TotalSectors $TotalSectors
    $mbrOffset = 0x1BE
    $bytes[$mbrOffset + 4] = 0xEE
    Set-M5U32Le -Bytes $bytes -Offset ($mbrOffset + 8) -Value 1
    Set-M5U32Le -Bytes $bytes -Offset ($mbrOffset + 12) -Value ([uint32]($TotalSectors - 1))
    $bytes[510] = 0x55
    $bytes[511] = 0xAA

    $entryArrayBytes = 128 * 128
    $entries = New-Object byte[] $entryArrayBytes
    for ($index = 0; $index -lt $Partitions.Count; $index++) {
        $entry = $Partitions[$index]
        $entryOffset = $index * 128
        Set-M5Bytes -Bytes $entries -Offset $entryOffset -Value (ConvertTo-M5GptGuidBytes -Guid $entry.TypeGuid)
        Set-M5Bytes -Bytes $entries -Offset ($entryOffset + 16) -Value (ConvertTo-M5GptGuidBytes -Guid $entry.UniqueGuid)
        Set-M5U64Le -Bytes $entries -Offset ($entryOffset + 32) -Value ([uint64]$entry.FirstLba)
        Set-M5U64Le -Bytes $entries -Offset ($entryOffset + 40) -Value ([uint64]$entry.LastLba)
        Set-M5Utf16Name -Bytes $entries -Offset ($entryOffset + 56) -Value $entry.Name
    }

    $entryArrayCrc = Get-M5Crc32 -Bytes $entries
    Set-M5Bytes -Bytes $bytes -Offset (2 * $Script:M5SectorBytes) -Value $entries

    $header = New-Object byte[] $Script:M5SectorBytes
    Set-M5Ascii -Bytes $header -Offset 0 -Value "EFI PART"
    Set-M5U32Le -Bytes $header -Offset 8 -Value 0x00010000
    Set-M5U32Le -Bytes $header -Offset 12 -Value 92
    Set-M5U64Le -Bytes $header -Offset 24 -Value 1
    Set-M5U64Le -Bytes $header -Offset 32 -Value ([uint64]($TotalSectors - 1))
    Set-M5U64Le -Bytes $header -Offset 40 -Value 34
    Set-M5U64Le -Bytes $header -Offset 48 -Value ([uint64]($TotalSectors - 34))
    Set-M5Bytes -Bytes $header -Offset 56 -Value (ConvertTo-M5GptGuidBytes -Guid "4c4f534f-5349-4d35-8000-000000000001")
    Set-M5U64Le -Bytes $header -Offset 72 -Value 2
    Set-M5U32Le -Bytes $header -Offset 80 -Value 128
    Set-M5U32Le -Bytes $header -Offset 84 -Value 128
    Set-M5U32Le -Bytes $header -Offset 88 -Value $entryArrayCrc
    $headerCrc = Get-M5Crc32 -Bytes $header -Offset 0 -Count 92
    Set-M5U32Le -Bytes $header -Offset 16 -Value $headerCrc
    Set-M5Bytes -Bytes $bytes -Offset $Script:M5SectorBytes -Value $header

    foreach ($entry in $Partitions) {
        $partitionOffset = [int64]$entry.FirstLba * $Script:M5SectorBytes
        switch ($entry.Signature) {
            "FAT32" {
                $bytes[$partitionOffset + 0] = 0xEB
                $bytes[$partitionOffset + 1] = 0x58
                $bytes[$partitionOffset + 2] = 0x90
                Set-M5Ascii -Bytes $bytes -Offset ($partitionOffset + 3) -Value "MSDOS5.0"
                Set-M5U16Le -Bytes $bytes -Offset ($partitionOffset + 11) -Value 512
                $bytes[$partitionOffset + 13] = 1
                Set-M5U16Le -Bytes $bytes -Offset ($partitionOffset + 14) -Value 32
                $bytes[$partitionOffset + 16] = 2
                $bytes[$partitionOffset + 21] = 0xF8
                Set-M5U32Le -Bytes $bytes -Offset ($partitionOffset + 32) -Value ([uint32]($entry.LastLba - $entry.FirstLba + 1))
                Set-M5U32Le -Bytes $bytes -Offset ($partitionOffset + 36) -Value 64
                Set-M5U32Le -Bytes $bytes -Offset ($partitionOffset + 44) -Value 2
                $bytes[$partitionOffset + 66] = 0x29
                $label = ($entry.FileSystemLabel + "           ").Substring(0, 11)
                Set-M5Ascii -Bytes $bytes -Offset ($partitionOffset + 71) -Value $label
                Set-M5Ascii -Bytes $bytes -Offset ($partitionOffset + 82) -Value "FAT32   "
                $bytes[$partitionOffset + 510] = 0x55
                $bytes[$partitionOffset + 511] = 0xAA
            }
            "NTFS" {
                $bytes[$partitionOffset + 0] = 0xEB
                $bytes[$partitionOffset + 1] = 0x52
                $bytes[$partitionOffset + 2] = 0x90
                Set-M5Ascii -Bytes $bytes -Offset ($partitionOffset + 3) -Value "NTFS    "
                $bytes[$partitionOffset + 510] = 0x55
                $bytes[$partitionOffset + 511] = 0xAA
            }
            "LIMITLESS" {
                $marker = "$($Script:M5MarkerText);role=$($entry.Role);label=$($entry.Name)"
                Set-M5Ascii -Bytes $bytes -Offset ($partitionOffset + 512) -Value $marker
            }
        }
    }

    $outputDir = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDir)) {
        New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    }
    [System.IO.File]::WriteAllBytes($OutputPath, $bytes)
}

function Read-M5Bytes
{
    param(
        [Parameter(Mandatory = $true)][System.IO.Stream]$Stream,
        [Parameter(Mandatory = $true)][int64]$Offset,
        [Parameter(Mandatory = $true)][int]$Count
    )

    $buffer = New-Object byte[] $Count
    $Stream.Seek($Offset, [System.IO.SeekOrigin]::Begin) | Out-Null
    $read = 0
    while ($read -lt $Count) {
        $chunk = $Stream.Read($buffer, $read, $Count - $read)
        if ($chunk -le 0) {
            throw "Unexpected end of image while reading $Count bytes at offset $Offset."
        }
        $read += $chunk
    }

    return $buffer
}

function Get-M5FilesystemSignature
{
    param(
        [Parameter(Mandatory = $true)][System.IO.Stream]$Stream,
        [Parameter(Mandatory = $true)][uint64]$FirstLba
    )

    $sector = Read-M5Bytes -Stream $Stream -Offset ([int64]$FirstLba * $Script:M5SectorBytes) -Count $Script:M5SectorBytes
    $oem = [System.Text.Encoding]::ASCII.GetString($sector, 3, 8)
    $fat32 = [System.Text.Encoding]::ASCII.GetString($sector, 82, 8)
    $label = ""
    if ($fat32 -eq "FAT32   ") {
        $label = [System.Text.Encoding]::ASCII.GetString($sector, 71, 11).Trim()
        return [PSCustomObject]@{
            signature = "FAT32"
            label = $label
        }
    }
    if ($oem -eq "NTFS    ") {
        return [PSCustomObject]@{
            signature = "NTFS"
            label = ""
        }
    }

    return [PSCustomObject]@{
        signature = "unknown"
        label = ""
    }
}

function Get-M5LimitlessMarker
{
    param(
        [Parameter(Mandatory = $true)][System.IO.Stream]$Stream,
        [Parameter(Mandatory = $true)][uint64]$FirstLba
    )

    $bytes = Read-M5Bytes -Stream $Stream -Offset (([int64]$FirstLba * $Script:M5SectorBytes) + 512) -Count 128
    $text = [System.Text.Encoding]::ASCII.GetString($bytes).Trim([char]0)
    if ($text.StartsWith($Script:M5MarkerText, [System.StringComparison]::Ordinal)) {
        return $text
    }

    return ""
}

function Get-M5GptPartitions
{
    param([Parameter(Mandatory = $true)][string]$ImagePath)

    if ($ImagePath.StartsWith("\\.\", [System.StringComparison]::Ordinal)) {
        $stream = [System.IO.File]::Open($ImagePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    }
    else {
        $stream = [System.IO.File]::Open($ImagePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    }
    try {
        $header = Read-M5Bytes -Stream $stream -Offset $Script:M5SectorBytes -Count $Script:M5SectorBytes
        if ([System.Text.Encoding]::ASCII.GetString($header, 0, 8) -ne "EFI PART") {
            throw "GPT signature not found."
        }

        $entryLba = Get-M5U64Le -Bytes $header -Offset 72
        $entryCount = Get-M5U32Le -Bytes $header -Offset 80
        $entrySize = Get-M5U32Le -Bytes $header -Offset 84
        if ($entrySize -ne 128) {
            throw "Unsupported GPT entry size $entrySize."
        }

        $entryBytes = Read-M5Bytes -Stream $stream -Offset ([int64]$entryLba * $Script:M5SectorBytes) -Count ([int]($entryCount * $entrySize))
        $partitions = New-Object System.Collections.Generic.List[object]
        for ($index = 0; $index -lt $entryCount; $index++) {
            $entryOffset = $index * $entrySize
            $typeGuid = ConvertFrom-M5GptGuidBytes -Bytes $entryBytes -Offset $entryOffset
            if ($typeGuid -eq "00000000-0000-0000-0000-000000000000") {
                continue
            }

            $firstLba = Get-M5U64Le -Bytes $entryBytes -Offset ($entryOffset + 32)
            $lastLba = Get-M5U64Le -Bytes $entryBytes -Offset ($entryOffset + 40)
            $nameBytes = New-Object byte[] 72
            [Array]::Copy($entryBytes, $entryOffset + 56, $nameBytes, 0, 72)
            $name = [System.Text.Encoding]::Unicode.GetString($nameBytes).Trim([char]0)
            $fs = Get-M5FilesystemSignature -Stream $stream -FirstLba $firstLba
            $marker = Get-M5LimitlessMarker -Stream $stream -FirstLba $firstLba
            $partitions.Add([PSCustomObject]@{
                number = $partitions.Count + 1
                typeGuid = $typeGuid
                firstLba = $firstLba
                lastLba = $lastLba
                sectors = ($lastLba - $firstLba + 1)
                name = $name
                filesystem = $fs.signature
                filesystemLabel = $fs.label
                marker = $marker
            })
        }

        return @($partitions.ToArray())
    }
    finally {
        $stream.Dispose()
    }
}

function Get-M5PartitionClassification
{
    param([Parameter(Mandatory = $true)]$Partition)

    $typeGuid = ([string]$Partition.typeGuid).ToLowerInvariant()
    $name = [string]$Partition.name
    $fs = [string]$Partition.filesystem
    $hasApprovedLabel = $false
    foreach ($label in $Script:M5ApprovedLabels) {
        if ($name.Equals($label, [System.StringComparison]::OrdinalIgnoreCase) -or $Partition.filesystemLabel.Equals($label, [System.StringComparison]::OrdinalIgnoreCase)) {
            $hasApprovedLabel = $true
            break
        }
    }
    $hasMarker = -not [string]::IsNullOrWhiteSpace($Partition.marker)

    if (($typeGuid -eq $Script:M5LimitlessTypeGuid) -or ($typeGuid -eq $Script:M5LimitlessBootTypeGuid) -or $hasApprovedLabel -or $hasMarker) {
        return [PSCustomObject]@{
            class = "safe"
            writable = $true
            reason = "dedicated LimitlessOS target"
        }
    }
    if ($typeGuid -eq $Script:M5EfiSystemTypeGuid) {
        return [PSCustomObject]@{
            class = "forbidden"
            writable = $false
            reason = "Windows EFI System Partition or unknown ESP"
        }
    }
    if ($typeGuid -eq $Script:M5MicrosoftReservedGuid) {
        return [PSCustomObject]@{
            class = "forbidden"
            writable = $false
            reason = "Microsoft Reserved partition"
        }
    }
    if ($typeGuid -eq $Script:M5WindowsRecoveryGuid) {
        return [PSCustomObject]@{
            class = "forbidden"
            writable = $false
            reason = "Windows Recovery partition"
        }
    }
    if ($fs -eq "NTFS") {
        return [PSCustomObject]@{
            class = "forbidden"
            writable = $false
            reason = "NTFS partition"
        }
    }
    if ($fs -eq "FAT32") {
        return [PSCustomObject]@{
            class = "forbidden"
            writable = $false
            reason = "unknown internal FAT32 partition"
        }
    }

    return [PSCustomObject]@{
        class = "unknown"
        writable = $false
        reason = "unknown GPT partition without LimitlessOS marker"
    }
}

function Get-M5ImageHash
{
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}
