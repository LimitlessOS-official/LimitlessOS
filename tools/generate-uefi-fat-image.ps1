param(
    [Parameter(Mandatory = $true)]
    [string]$InputEfiPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputImagePath,

    [string]$BootManifestPath = "",

    [string]$KernelPayloadPath = "",

    [string]$BootLinuxAppPath = "",

    [string]$BootLinuxAppName = "DYNLDLIMIT",

    [string]$BootLinuxInterpPath = "",

    [string]$BootLinuxInterpName = "LDLIMIT",

    [string]$VolumeLabel = "LIMITLESS64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Set-UInt16LE
{
    param([byte[]]$Buffer, [int]$Offset, [uint32]$Value)
    $Buffer[$Offset] = [byte]($Value -band 0xFF)
    $Buffer[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
}

function Set-UInt32LE
{
    param([byte[]]$Buffer, [int]$Offset, [uint32]$Value)
    $Buffer[$Offset] = [byte]($Value -band 0xFF)
    $Buffer[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
    $Buffer[$Offset + 2] = [byte](($Value -shr 16) -band 0xFF)
    $Buffer[$Offset + 3] = [byte](($Value -shr 24) -band 0xFF)
}

function Set-AsciiPadded
{
    param([byte[]]$Buffer, [int]$Offset, [int]$Length, [string]$Value)
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($Value)
    $copyLength = [Math]::Min($Length, $bytes.Length)
    if ($copyLength -gt 0) {
        [Array]::Copy($bytes, 0, $Buffer, $Offset, $copyLength)
    }
    for ($index = $Offset + $copyLength; $index -lt ($Offset + $Length); $index++) {
        $Buffer[$index] = 0x20
    }
}

function New-DirectoryEntry
{
    param(
        [string]$ShortName,
        [string]$ShortExtension,
        [byte]$Attributes,
        [uint16]$FirstCluster,
        [uint32]$FileSize
    )

    $entry = [byte[]]::new(32)
    Set-AsciiPadded -Buffer $entry -Offset 0 -Length 8 -Value $ShortName
    Set-AsciiPadded -Buffer $entry -Offset 8 -Length 3 -Value $ShortExtension
    $entry[11] = $Attributes
    Set-UInt16LE -Buffer $entry -Offset 26 -Value $FirstCluster
    Set-UInt32LE -Buffer $entry -Offset 28 -Value $FileSize
    return ,$entry
}

function Get-FatShortNameBytes
{
    param(
        [string]$ShortName,
        [string]$ShortExtension
    )

    $bytes = [byte[]]::new(11)
    Set-AsciiPadded -Buffer $bytes -Offset 0 -Length 8 -Value $ShortName
    Set-AsciiPadded -Buffer $bytes -Offset 8 -Length 3 -Value $ShortExtension
    return ,$bytes
}

function Get-LfnChecksum
{
    param([byte[]]$ShortNameBytes)

    [uint32]$sum = 0
    for ($index = 0; $index -lt 11; $index++) {
        $sum = (((($sum -band 1) -shl 7) + ($sum -shr 1) + $ShortNameBytes[$index]) -band 0xFF)
    }
    return [byte]$sum
}

function Set-UInt16LENullable
{
    param([byte[]]$Buffer, [int]$Offset, [int]$Value)

    Set-UInt16LE -Buffer $Buffer -Offset $Offset -Value ([uint32]$Value)
}

function New-LfnEntry
{
    param(
        [string]$Name,
        [int]$ChunkIndex,
        [int]$ChunkCount,
        [byte]$Checksum
    )

    $entry = [byte[]]::new(32)
    $entry[0] = [byte]($ChunkIndex + 1)
    if ($ChunkIndex -eq ($ChunkCount - 1)) {
        $entry[0] = [byte]($entry[0] -bor 0x40)
    }
    $entry[11] = 0x0F
    $entry[12] = 0x00
    $entry[13] = $Checksum
    $entry[26] = 0x00
    $entry[27] = 0x00

    $positions = @(1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30)
    $start = $ChunkIndex * 13
    for ($slot = 0; $slot -lt 13; $slot++) {
        $nameIndex = $start + $slot
        if ($nameIndex -lt $Name.Length) {
            $value = [int][char]$Name[$nameIndex]
        }
        elseif ($nameIndex -eq $Name.Length) {
            $value = 0
        }
        else {
            $value = 0xFFFF
        }
        Set-UInt16LENullable -Buffer $entry -Offset $positions[$slot] -Value $value
    }

    return ,$entry
}

function New-FileDirectoryEntries
{
    param(
        [string]$LongName,
        [string]$ShortName,
        [string]$ShortExtension,
        [byte]$Attributes,
        [uint16]$FirstCluster,
        [uint32]$FileSize
    )

    $entries = @()
    $shortBytes = Get-FatShortNameBytes -ShortName $ShortName -ShortExtension $ShortExtension
    $shortVisible = $ShortName.Trim()
    if ($ShortExtension.Trim().Length -gt 0) {
        $shortVisible += "." + $ShortExtension.Trim()
    }
    $shortVisible = $shortVisible.ToUpperInvariant()
    if ($LongName.Trim().ToUpperInvariant() -ne $shortVisible) {
        $checksum = Get-LfnChecksum -ShortNameBytes $shortBytes
        $chunkCount = [int][Math]::Ceiling($LongName.Length / 13.0)
        for ($chunk = $chunkCount - 1; $chunk -ge 0; $chunk--) {
            $entries += ,(New-LfnEntry -Name $LongName -ChunkIndex $chunk -ChunkCount $chunkCount -Checksum $checksum)
        }
    }
    $entries += ,(New-DirectoryEntry -ShortName $ShortName -ShortExtension $ShortExtension -Attributes $Attributes -FirstCluster $FirstCluster -FileSize $FileSize)
    return ,$entries
}

function Set-Fat12Entry
{
    param(
        [byte[]]$Fat,
        [int]$Cluster,
        [int]$Value
    )

    $offset = [int]([Math]::Floor($Cluster * 3 / 2))

    if (($Cluster % 2) -eq 0) {
        $Fat[$offset] = [byte]($Value -band 0xFF)
        $Fat[$offset + 1] = [byte](($Fat[$offset + 1] -band 0xF0) -bor (($Value -shr 8) -band 0x0F))
    }
    else {
        $Fat[$offset] = [byte](($Fat[$offset] -band 0x0F) -bor (($Value -shl 4) -band 0xF0))
        $Fat[$offset + 1] = [byte](($Value -shr 4) -band 0xFF)
    }
}

function Set-Fat12Chain
{
    param(
        [byte[]]$Fat,
        [int]$FirstCluster,
        [int]$ClusterCount
    )

    if ($ClusterCount -le 0) {
        return
    }

    for ($cluster = 0; $cluster -lt $ClusterCount; $cluster++) {
        $clusterNumber = $FirstCluster + $cluster
        $value = if ($cluster -eq ($ClusterCount - 1)) { 0xFFF } else { $clusterNumber + 1 }
        Set-Fat12Entry -Fat $Fat -Cluster $clusterNumber -Value $value
    }
}

function Get-ClusterCount
{
    param([byte[]]$Bytes)

    if ($Bytes.Length -le 0) {
        return 0
    }

    return [int][Math]::Ceiling($Bytes.Length / [double]$bytesPerSector)
}

function Copy-FileData
{
    param(
        [byte[]]$Image,
        [int]$FirstCluster,
        [byte[]]$Bytes
    )

    if ($Bytes.Length -le 0) {
        return
    }

    $offset = ($dataStartSector + ($FirstCluster - 2)) * $bytesPerSector
    [Array]::Copy($Bytes, 0, $Image, $offset, $Bytes.Length)
}

function Get-BootLinuxStageFile
{
    param(
        [string]$Path,
        [string]$Name,
        [string]$ShortName
    )

    if ($Path.Trim().Length -le 0) {
        return $null
    }
    if (-not (Test-Path $Path)) {
        throw "Boot Linux stage file not found: $Path"
    }
    if ($Name.Trim().Length -le 0) {
        throw "Boot Linux stage file name must not be empty."
    }

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -le 0) {
        throw "Boot Linux stage file is empty: $Path"
    }

    return @{
        Path = (Resolve-Path $Path).Path
        Name = $Name.Trim().ToUpperInvariant()
        ShortName = $ShortName
        ShortExtension = ""
        Bytes = $bytes
        FirstCluster = 0
        ClusterCount = 0
    }
}

if (-not (Test-Path $InputEfiPath)) {
    throw "Input EFI app not found: $InputEfiPath"
}

$efiBytes = [System.IO.File]::ReadAllBytes($InputEfiPath)
$readmeText = "LimitlessOS x86_64 UEFI image`r`nBoot path: EFI\\BOOT\\BOOTX64.EFI`r`n"
$readmeBytes = [System.Text.Encoding]::ASCII.GetBytes($readmeText)

$includeBootManifest = $BootManifestPath.Trim().Length -gt 0
$includeKernelPayload = $KernelPayloadPath.Trim().Length -gt 0

[byte[]]$bootManifestBytes = [byte[]]::new(0)
[byte[]]$kernelPayloadBytes = [byte[]]::new(0)
$bootLinuxFiles = @()

if ($includeBootManifest) {
    if (-not (Test-Path $BootManifestPath)) {
        throw "Boot manifest not found: $BootManifestPath"
    }

    $bootManifestBytes = [System.IO.File]::ReadAllBytes($BootManifestPath)
    if ($bootManifestBytes.Length -le 0) {
        throw "Boot manifest is empty: $BootManifestPath"
    }
}

if ($includeKernelPayload) {
    if (-not (Test-Path $KernelPayloadPath)) {
        throw "Kernel payload not found: $KernelPayloadPath"
    }

    $kernelPayloadBytes = [System.IO.File]::ReadAllBytes($KernelPayloadPath)
    if ($kernelPayloadBytes.Length -le 0) {
        throw "Kernel payload is empty: $KernelPayloadPath"
    }
}

$bootLinuxApp = Get-BootLinuxStageFile -Path $BootLinuxAppPath -Name $BootLinuxAppName -ShortName "DYNLDL~1"
if ($bootLinuxApp -ne $null) {
    $bootLinuxFiles += $bootLinuxApp
}
$bootLinuxInterp = Get-BootLinuxStageFile -Path $BootLinuxInterpPath -Name $BootLinuxInterpName -ShortName "LDLIMIT"
if ($bootLinuxInterp -ne $null) {
    $bootLinuxFiles += $bootLinuxInterp
}

$bytesPerSector = 512
$totalSectors = 2880
$reservedSectors = 1
$fatCount = 2
$rootEntries = 224
$sectorsPerFat = 9
$sectorsPerCluster = 1
$rootDirSectors = [int](($rootEntries * 32) / $bytesPerSector)
$rootDirStartSector = $reservedSectors + ($fatCount * $sectorsPerFat)
$dataStartSector = $rootDirStartSector + $rootDirSectors
$imageSize = $bytesPerSector * $totalSectors

$efiClusterCount = Get-ClusterCount -Bytes $efiBytes
$readmeClusterCount = Get-ClusterCount -Bytes $readmeBytes
$bootManifestClusterCount = Get-ClusterCount -Bytes $bootManifestBytes
$kernelPayloadClusterCount = Get-ClusterCount -Bytes $kernelPayloadBytes
$includeBootLinuxApps = ($bootLinuxFiles.Count -ne 0)
$efiDirCluster = 2
$bootDirCluster = 3
$appsDirCluster = if ($includeBootLinuxApps) { 4 } else { 0 }
$nextFreeCluster = if ($includeBootLinuxApps) { 5 } else { 4 }
$efiFileFirstCluster = $nextFreeCluster
$nextFreeCluster += $efiClusterCount
$readmeFirstCluster = $nextFreeCluster
$nextFreeCluster += $readmeClusterCount
$bootManifestFirstCluster = if ($includeBootManifest) { $nextFreeCluster } else { 0 }
$nextFreeCluster += $bootManifestClusterCount
$kernelPayloadFirstCluster = if ($includeKernelPayload) { $nextFreeCluster } else { 0 }
$nextFreeCluster += $kernelPayloadClusterCount
for ($index = 0; $index -lt $bootLinuxFiles.Count; $index++) {
    $bootLinuxFiles[$index].ClusterCount = Get-ClusterCount -Bytes $bootLinuxFiles[$index].Bytes
    $bootLinuxFiles[$index].FirstCluster = $nextFreeCluster
    $nextFreeCluster += $bootLinuxFiles[$index].ClusterCount
}
$requiredClusters = $nextFreeCluster - 2

$availableClusters = $totalSectors - $dataStartSector
if ($requiredClusters -gt $availableClusters) {
    throw "UEFI FAT image exceeded the 1.44 MiB FAT12 cluster budget."
}

[byte[]]$imageBytes = [byte[]]::new($imageSize)

# Boot sector / BPB
$imageBytes[0] = 0xEB
$imageBytes[1] = 0x3C
$imageBytes[2] = 0x90
Set-AsciiPadded -Buffer $imageBytes -Offset 3 -Length 8 -Value "LIMITLOS"
Set-UInt16LE -Buffer $imageBytes -Offset 11 -Value $bytesPerSector
$imageBytes[13] = [byte]$sectorsPerCluster
Set-UInt16LE -Buffer $imageBytes -Offset 14 -Value $reservedSectors
$imageBytes[16] = [byte]$fatCount
Set-UInt16LE -Buffer $imageBytes -Offset 17 -Value $rootEntries
Set-UInt16LE -Buffer $imageBytes -Offset 19 -Value $totalSectors
$imageBytes[21] = 0xF0
Set-UInt16LE -Buffer $imageBytes -Offset 22 -Value $sectorsPerFat
Set-UInt16LE -Buffer $imageBytes -Offset 24 -Value 18
Set-UInt16LE -Buffer $imageBytes -Offset 26 -Value 2
Set-UInt32LE -Buffer $imageBytes -Offset 28 -Value 0
Set-UInt32LE -Buffer $imageBytes -Offset 32 -Value 0
$imageBytes[36] = 0x00
$imageBytes[37] = 0x00
$imageBytes[38] = 0x29
Set-UInt32LE -Buffer $imageBytes -Offset 39 -Value 0x4C4F5336
Set-AsciiPadded -Buffer $imageBytes -Offset 43 -Length 11 -Value $VolumeLabel
Set-AsciiPadded -Buffer $imageBytes -Offset 54 -Length 8 -Value "FAT12"
$imageBytes[510] = 0x55
$imageBytes[511] = 0xAA

# FAT tables
$fatSizeBytes = $sectorsPerFat * $bytesPerSector
[byte[]]$fat = [byte[]]::new($fatSizeBytes)
$fat[0] = 0xF0
$fat[1] = 0xFF
$fat[2] = 0xFF

Set-Fat12Entry -Fat $fat -Cluster $efiDirCluster -Value 0xFFF
Set-Fat12Entry -Fat $fat -Cluster $bootDirCluster -Value 0xFFF
if ($includeBootLinuxApps) {
    Set-Fat12Entry -Fat $fat -Cluster $appsDirCluster -Value 0xFFF
}

Set-Fat12Chain -Fat $fat -FirstCluster $efiFileFirstCluster -ClusterCount $efiClusterCount
Set-Fat12Chain -Fat $fat -FirstCluster $readmeFirstCluster -ClusterCount $readmeClusterCount
Set-Fat12Chain -Fat $fat -FirstCluster $bootManifestFirstCluster -ClusterCount $bootManifestClusterCount
Set-Fat12Chain -Fat $fat -FirstCluster $kernelPayloadFirstCluster -ClusterCount $kernelPayloadClusterCount
foreach ($stageFile in $bootLinuxFiles) {
    Set-Fat12Chain -Fat $fat -FirstCluster $stageFile.FirstCluster -ClusterCount $stageFile.ClusterCount
}

$fat1Offset = $reservedSectors * $bytesPerSector
$fat2Offset = $fat1Offset + $fatSizeBytes
[Array]::Copy($fat, 0, $imageBytes, $fat1Offset, $fat.Length)
[Array]::Copy($fat, 0, $imageBytes, $fat2Offset, $fat.Length)

# Root directory
$rootOffset = $rootDirStartSector * $bytesPerSector
$rootEntriesBytes = @(
    (New-DirectoryEntry -ShortName "EFI" -ShortExtension "" -Attributes 0x10 -FirstCluster $efiDirCluster -FileSize 0),
    (New-DirectoryEntry -ShortName "README" -ShortExtension "TXT" -Attributes 0x20 -FirstCluster $readmeFirstCluster -FileSize $readmeBytes.Length)
)
if ($includeBootManifest) {
    $rootEntriesBytes += ,(New-DirectoryEntry -ShortName "BOOTMAN" -ShortExtension "TXT" -Attributes 0x20 -FirstCluster $bootManifestFirstCluster -FileSize $bootManifestBytes.Length)
}
if ($includeKernelPayload) {
    $rootEntriesBytes += ,(New-DirectoryEntry -ShortName "KERNEL64" -ShortExtension "BIN" -Attributes 0x20 -FirstCluster $kernelPayloadFirstCluster -FileSize $kernelPayloadBytes.Length)
}
if ($includeBootLinuxApps) {
    $rootEntriesBytes += ,(New-DirectoryEntry -ShortName "APPS" -ShortExtension "" -Attributes 0x10 -FirstCluster $appsDirCluster -FileSize 0)
}

$entryOffset = $rootOffset
foreach ($entry in $rootEntriesBytes) {
    [Array]::Copy($entry, 0, $imageBytes, $entryOffset, $entry.Length)
    $entryOffset += 32
}

# EFI directory cluster
$efiDirOffset = ($dataStartSector + ($efiDirCluster - 2)) * $bytesPerSector
$efiEntries = @(
    (New-DirectoryEntry -ShortName "." -ShortExtension "" -Attributes 0x10 -FirstCluster $efiDirCluster -FileSize 0),
    (New-DirectoryEntry -ShortName ".." -ShortExtension "" -Attributes 0x10 -FirstCluster 0 -FileSize 0),
    (New-DirectoryEntry -ShortName "BOOT" -ShortExtension "" -Attributes 0x10 -FirstCluster $bootDirCluster -FileSize 0)
)
$entryOffset = $efiDirOffset
foreach ($entry in $efiEntries) {
    [Array]::Copy($entry, 0, $imageBytes, $entryOffset, $entry.Length)
    $entryOffset += 32
}

# BOOT directory cluster
$bootDirOffset = ($dataStartSector + ($bootDirCluster - 2)) * $bytesPerSector
$bootEntries = @(
    (New-DirectoryEntry -ShortName "." -ShortExtension "" -Attributes 0x10 -FirstCluster $bootDirCluster -FileSize 0),
    (New-DirectoryEntry -ShortName ".." -ShortExtension "" -Attributes 0x10 -FirstCluster $efiDirCluster -FileSize 0),
    (New-DirectoryEntry -ShortName "BOOTX64" -ShortExtension "EFI" -Attributes 0x20 -FirstCluster $efiFileFirstCluster -FileSize $efiBytes.Length)
)
$entryOffset = $bootDirOffset
foreach ($entry in $bootEntries) {
    [Array]::Copy($entry, 0, $imageBytes, $entryOffset, $entry.Length)
    $entryOffset += 32
}

if ($includeBootLinuxApps) {
    $appsDirOffset = ($dataStartSector + ($appsDirCluster - 2)) * $bytesPerSector
    $appsEntries = @(
        (New-DirectoryEntry -ShortName "." -ShortExtension "" -Attributes 0x10 -FirstCluster $appsDirCluster -FileSize 0),
        (New-DirectoryEntry -ShortName ".." -ShortExtension "" -Attributes 0x10 -FirstCluster 0 -FileSize 0)
    )
    foreach ($stageFile in $bootLinuxFiles) {
        $appsEntries += New-FileDirectoryEntries `
            -LongName $stageFile.Name `
            -ShortName $stageFile.ShortName `
            -ShortExtension $stageFile.ShortExtension `
            -Attributes 0x20 `
            -FirstCluster ([uint16]$stageFile.FirstCluster) `
            -FileSize ([uint32]$stageFile.Bytes.Length)
    }
    $entryOffset = $appsDirOffset
    foreach ($entry in $appsEntries) {
        [Array]::Copy($entry, 0, $imageBytes, $entryOffset, $entry.Length)
        $entryOffset += 32
    }
}

# File data
Copy-FileData -Image $imageBytes -FirstCluster $efiFileFirstCluster -Bytes $efiBytes
Copy-FileData -Image $imageBytes -FirstCluster $readmeFirstCluster -Bytes $readmeBytes
Copy-FileData -Image $imageBytes -FirstCluster $bootManifestFirstCluster -Bytes $bootManifestBytes
Copy-FileData -Image $imageBytes -FirstCluster $kernelPayloadFirstCluster -Bytes $kernelPayloadBytes
foreach ($stageFile in $bootLinuxFiles) {
    Copy-FileData -Image $imageBytes -FirstCluster $stageFile.FirstCluster -Bytes $stageFile.Bytes
}

[System.IO.File]::WriteAllBytes($OutputImagePath, $imageBytes)

Write-Host "Generated UEFI FAT image"
Write-Host "  input efi : $InputEfiPath"
Write-Host "  output img: $OutputImagePath"
Write-Host "  label     : $VolumeLabel"
Write-Host "  size      : $imageSize bytes"
if ($includeBootManifest) {
    Write-Host "  manifest  : $BootManifestPath ($($bootManifestBytes.Length) bytes)"
}
if ($includeKernelPayload) {
    Write-Host "  payload   : $KernelPayloadPath ($($kernelPayloadBytes.Length) bytes)"
}
foreach ($stageFile in $bootLinuxFiles) {
    Write-Host "  boot app  : /APPS/$($stageFile.Name) ($($stageFile.Bytes.Length) bytes from $($stageFile.Path))"
}
