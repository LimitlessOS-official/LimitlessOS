param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string]$PayloadImagePath = "",

    [uint32]$PayloadSlot = 1,

    [string[]]$FlatBinaryImagePath = @(),

    [uint32[]]$FlatBinaryPayloadSlot = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Convert-ToUInt32 {
    param([Parameter(Mandatory = $true)]$Value)

    if ($Value -is [byte] -or $Value -is [int16] -or $Value -is [uint16] -or $Value -is [int] -or $Value -is [uint32] -or $Value -is [int64] -or $Value -is [uint64]) {
        return [uint32]$Value
    }

    if ($Value -is [string]) {
        $text = $Value.Trim()
        if ($text.StartsWith("0x") -or $text.StartsWith("0X")) {
            return [Convert]::ToUInt32($text.Substring(2), 16)
        }

        return [Convert]::ToUInt32($text, 10)
    }

    throw "Unsupported numeric value: $Value"
}

function Add-UInt32Le {
    param(
        [AllowEmptyCollection()]
        [Parameter(Mandatory = $true)]
        [System.Collections.Generic.List[byte]]$Bytes,

        [Parameter(Mandatory = $true)]
        [uint32]$Value
    )

    $Bytes.Add([byte]($Value -band 0xFF))
    $Bytes.Add([byte](($Value -shr 8) -band 0xFF))
    $Bytes.Add([byte](($Value -shr 16) -band 0xFF))
    $Bytes.Add([byte](($Value -shr 24) -band 0xFF))
}

function Resolve-PayloadKind {
    param([Parameter(Mandatory = $true)][string]$Kind)

    switch ($Kind) {
        "bootstrap-service" { return [uint32]1 }
        "flat-binary" { return [uint32]2 }
        default { throw "Unsupported payload kind '$Kind'." }
    }
}

function Get-Fnv1aChecksum {
    param(
        [Parameter(Mandatory = $true)]
        [byte[]]$Bytes,

        [Parameter(Mandatory = $true)]
        [int]$ChecksumOffset
    )

    [uint32]$hash = 2166136261

    for ($i = 0; $i -lt $Bytes.Length; $i++) {
        [uint32]$value = $Bytes[$i]
        if (($i -ge $ChecksumOffset) -and ($i -lt ($ChecksumOffset + 4))) {
            $value = 0
        }

        $hash = [uint32](($hash -bxor $value) -band 0xFFFFFFFF)
        $hash = [uint32](([uint64]$hash * [uint64]16777619) % [uint64]4294967296)
    }

    return $hash
}

function Get-Fnv1aDataChecksum {
    param(
        [Parameter(Mandatory = $true)]
        [byte[]]$Bytes
    )

    [uint32]$hash = 2166136261

    for ($i = 0; $i -lt $Bytes.Length; $i++) {
        [uint32]$value = $Bytes[$i]
        $hash = [uint32](($hash -bxor $value) -band 0xFFFFFFFF)
        $hash = [uint32](([uint64]$hash * [uint64]16777619) % [uint64]4294967296)
    }

    if ($hash -eq 0) {
        return [uint32]1
    }

    return $hash
}

$inputFullPath = [System.IO.Path]::GetFullPath($InputPath)
$outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)
$outputDir = Split-Path -Parent $outputFullPath

if (-not (Test-Path $inputFullPath)) {
    throw "Input package spec not found: $inputFullPath"
}

New-Item -ItemType Directory -Force $outputDir | Out-Null

$spec = Get-Content -Raw -Path $inputFullPath | ConvertFrom-Json
$payloadOverrides = @{}
if ($PayloadImagePath.Trim().Length -gt 0) {
    $payloadImageFullPath = [System.IO.Path]::GetFullPath($PayloadImagePath)
    if (-not (Test-Path $payloadImageFullPath)) {
        throw "Payload image not found: $payloadImageFullPath"
    }

    [byte[]]$payloadImageBytes = [System.IO.File]::ReadAllBytes($payloadImageFullPath)
    if ($payloadImageBytes.Length -le 0) {
        throw "Payload image is empty: $payloadImageFullPath"
    }

    $payloadOverrides[[uint32]$PayloadSlot] = [pscustomobject]@{
        Slot = $PayloadSlot
        Size = [uint32]$payloadImageBytes.Length
        Checksum = Get-Fnv1aDataChecksum -Bytes $payloadImageBytes
        Path = $payloadImageFullPath
    }
}
if ($FlatBinaryImagePath.Count -gt 0) {
    if ($FlatBinaryPayloadSlot.Count -ne $FlatBinaryImagePath.Count) {
        throw "Flat binary image and payload slot counts must match."
    }

    for ($flatIndex = 0; $flatIndex -lt $FlatBinaryImagePath.Count; $flatIndex++) {
        if ($FlatBinaryImagePath[$flatIndex].Trim().Length -eq 0) {
            continue
        }

        $flatBinaryImageFullPath = [System.IO.Path]::GetFullPath($FlatBinaryImagePath[$flatIndex])
        if (-not (Test-Path $flatBinaryImageFullPath)) {
            throw "Flat binary image not found: $flatBinaryImageFullPath"
        }

        [byte[]]$flatBinaryImageBytes = [System.IO.File]::ReadAllBytes($flatBinaryImageFullPath)
        if ($flatBinaryImageBytes.Length -le 0) {
            throw "Flat binary image is empty: $flatBinaryImageFullPath"
        }

        $flatBinaryPayloadSlotValue = [uint32]$FlatBinaryPayloadSlot[$flatIndex]
        $payloadOverrides[$flatBinaryPayloadSlotValue] = [pscustomobject]@{
            Slot = $flatBinaryPayloadSlotValue
            Size = [uint32]$flatBinaryImageBytes.Length
            Checksum = Get-Fnv1aDataChecksum -Bytes $flatBinaryImageBytes
            Path = $flatBinaryImageFullPath
        }
    }
}
$stringBytes = New-Object 'System.Collections.Generic.List[byte]'
$stringOffsets = @{}

function Add-ArchiveString {
    param([Parameter(Mandatory = $true)][string]$Text)

    if ($stringOffsets.ContainsKey($Text)) {
        return [uint32]$stringOffsets[$Text]
    }

    [uint32]$offset = [uint32]$stringBytes.Count
    foreach ($byte in [System.Text.Encoding]::ASCII.GetBytes($Text)) {
        $stringBytes.Add($byte)
    }
    $stringBytes.Add([byte]0)
    $stringOffsets[$Text] = $offset
    return $offset
}

$signerRecords = @()
foreach ($signer in @($spec.signers)) {
    $signerRecords += [pscustomobject]@{
        Id = Convert-ToUInt32 $signer.id
        NameOffset = Add-ArchiveString $signer.name
        VerificationToken = Convert-ToUInt32 $signer.verificationToken
    }
}

$payloadRecords = @()
foreach ($payload in @($spec.payloads)) {
    $payloadSlotValue = Convert-ToUInt32 $payload.slot
    $imageSize = Convert-ToUInt32 $payload.imageSize
    $imageChecksum = Convert-ToUInt32 $payload.imageChecksum

    if ($payloadOverrides.ContainsKey($payloadSlotValue)) {
        $payloadOverride = $payloadOverrides[$payloadSlotValue]
        $imageSize = $payloadOverride.Size
        $imageChecksum = $payloadOverride.Checksum
    }

    $payloadRecords += [pscustomobject]@{
        Slot = $payloadSlotValue
        Kind = Resolve-PayloadKind $payload.kind
        ImageOffset = Convert-ToUInt32 $payload.imageOffset
        ImageSize = $imageSize
        ImageChecksum = $imageChecksum
    }
}

$manifestRecords = @()
foreach ($manifest in @($spec.manifests)) {
    $manifestRecords += [pscustomobject]@{
        SourceSlot = Convert-ToUInt32 $manifest.sourceSlot
        PackageId = Convert-ToUInt32 $manifest.packageId
        PackageNameOffset = Add-ArchiveString $manifest.packageName
        PackageVersion = Convert-ToUInt32 $manifest.packageVersion
        SignerId = Convert-ToUInt32 $manifest.signerId
        TrustFlags = Convert-ToUInt32 $manifest.trustFlags
        LaunchAuthorityMask = Convert-ToUInt32 $manifest.launchAuthorityMask
        MaxInstances = Convert-ToUInt32 $manifest.maxInstances
        ExecutableId = Convert-ToUInt32 $manifest.executableId
        ExecutableNameOffset = Add-ArchiveString $manifest.name
        ProcessNameOffset = Add-ArchiveString $manifest.processName
        ProfileNameOffset = Add-ArchiveString $manifest.profileName
        PeerEndpointNameOffset = Add-ArchiveString $manifest.peerEndpointName
        PolicyEndpointNameOffset = Add-ArchiveString $manifest.policyEndpointName
        AllowedEndpointRoleMask = Convert-ToUInt32 $manifest.allowedEndpointRoleMask
        AllowedServiceClassMask = Convert-ToUInt32 $manifest.allowedServiceClassMask
        SchedulerClass = Convert-ToUInt32 $manifest.schedulerClass
        SchedulerWeight = Convert-ToUInt32 $manifest.schedulerWeight
        SchedulerLatencyTargetTicks = Convert-ToUInt32 $manifest.schedulerLatencyTargetTicks
        SchedulerIoWakeupDeadlineTicks = Convert-ToUInt32 $manifest.schedulerIoWakeupDeadlineTicks
        CapabilityAdmissionLimit = Convert-ToUInt32 $manifest.capabilityAdmissionLimit
        LaunchRole = Convert-ToUInt32 $manifest.launchRole
        PayloadSlot = Convert-ToUInt32 $manifest.payloadSlot
    }
}

$archiveBytes = New-Object 'System.Collections.Generic.List[byte]'
Add-UInt32Le $archiveBytes 0x504B4753
Add-UInt32Le $archiveBytes (Convert-ToUInt32 $spec.version)
Add-UInt32Le $archiveBytes ([uint32]$signerRecords.Count)
Add-UInt32Le $archiveBytes ([uint32]$manifestRecords.Count)
Add-UInt32Le $archiveBytes ([uint32]$payloadRecords.Count)
Add-UInt32Le $archiveBytes ([uint32]$stringBytes.Count)
Add-UInt32Le $archiveBytes 0

foreach ($signer in $signerRecords) {
    Add-UInt32Le $archiveBytes $signer.Id
    Add-UInt32Le $archiveBytes $signer.NameOffset
    Add-UInt32Le $archiveBytes $signer.VerificationToken
}

foreach ($manifest in $manifestRecords) {
    Add-UInt32Le $archiveBytes $manifest.SourceSlot
    Add-UInt32Le $archiveBytes $manifest.PackageId
    Add-UInt32Le $archiveBytes $manifest.PackageNameOffset
    Add-UInt32Le $archiveBytes $manifest.PackageVersion
    Add-UInt32Le $archiveBytes $manifest.SignerId
    Add-UInt32Le $archiveBytes $manifest.TrustFlags
    Add-UInt32Le $archiveBytes $manifest.LaunchAuthorityMask
    Add-UInt32Le $archiveBytes $manifest.MaxInstances
    Add-UInt32Le $archiveBytes $manifest.ExecutableId
    Add-UInt32Le $archiveBytes $manifest.ExecutableNameOffset
    Add-UInt32Le $archiveBytes $manifest.ProcessNameOffset
    Add-UInt32Le $archiveBytes $manifest.ProfileNameOffset
    Add-UInt32Le $archiveBytes $manifest.PeerEndpointNameOffset
    Add-UInt32Le $archiveBytes $manifest.PolicyEndpointNameOffset
    Add-UInt32Le $archiveBytes $manifest.AllowedEndpointRoleMask
    Add-UInt32Le $archiveBytes $manifest.AllowedServiceClassMask
    Add-UInt32Le $archiveBytes $manifest.SchedulerClass
    Add-UInt32Le $archiveBytes $manifest.SchedulerWeight
    Add-UInt32Le $archiveBytes $manifest.SchedulerLatencyTargetTicks
    Add-UInt32Le $archiveBytes $manifest.SchedulerIoWakeupDeadlineTicks
    Add-UInt32Le $archiveBytes $manifest.CapabilityAdmissionLimit
    Add-UInt32Le $archiveBytes $manifest.LaunchRole
    Add-UInt32Le $archiveBytes $manifest.PayloadSlot
}

foreach ($payload in $payloadRecords) {
    Add-UInt32Le $archiveBytes $payload.Slot
    Add-UInt32Le $archiveBytes $payload.Kind
    Add-UInt32Le $archiveBytes $payload.ImageOffset
    Add-UInt32Le $archiveBytes $payload.ImageSize
    Add-UInt32Le $archiveBytes $payload.ImageChecksum
}

foreach ($byte in $stringBytes) {
    $archiveBytes.Add($byte)
}

[byte[]]$archive = $archiveBytes.ToArray()
$checksumOffset = 24
[uint32]$checksum = Get-Fnv1aChecksum -Bytes $archive -ChecksumOffset $checksumOffset
[byte[]]$checksumBytes = [System.BitConverter]::GetBytes($checksum)
for ($i = 0; $i -lt 4; $i++) {
    $archive[$checksumOffset + $i] = $checksumBytes[$i]
}

$lines = New-Object 'System.Collections.Generic.List[string]'
$lines.Add("#ifndef LIMITLESS_PACKAGE_STORE_GENERATED_H")
$lines.Add("#define LIMITLESS_PACKAGE_STORE_GENERATED_H")
$lines.Add("")
$lines.Add("// Generated from packages/bootstrap-store.json by tools/generate-package-store.ps1")
$lines.Add(("#define PACKAGE_STORE_GENERATED_ARCHIVE_SIZE {0}u" -f $archive.Length))
$lines.Add(("#define PACKAGE_STORE_GENERATED_ARCHIVE_CHECKSUM 0x{0:X8}u" -f $checksum))
$lines.Add("")
$lines.Add("static const u8 package_store_generated_archive[PACKAGE_STORE_GENERATED_ARCHIVE_SIZE] = {")

for ($index = 0; $index -lt $archive.Length; $index += 16) {
    $chunkEnd = [Math]::Min($index + 15, $archive.Length - 1)
    $chunkValues = New-Object 'System.Collections.Generic.List[string]'
    for ($chunkIndex = $index; $chunkIndex -le $chunkEnd; $chunkIndex++) {
        $chunkValues.Add(("0x{0:X2}" -f $archive[$chunkIndex]))
    }

    $line = "    " + ($chunkValues -join ", ")
    if ($chunkEnd -lt ($archive.Length - 1)) {
        $line += ","
    }

    $lines.Add($line)
}

$lines.Add("};")
$lines.Add("")
$lines.Add("#endif")

Set-Content -Path $outputFullPath -Value $lines -Encoding ASCII
Write-Host "Generated bootstrap package archive: $outputFullPath"
if ($null -ne $payloadOverride) {
    Write-Host ("  payload slot {0}: {1} bytes checksum 0x{2:X8} from {3}" -f `
        $payloadOverride.Slot, `
        $payloadOverride.Size, `
        $payloadOverride.Checksum, `
        $payloadOverride.Path)
}
