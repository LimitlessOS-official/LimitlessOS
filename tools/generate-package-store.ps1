param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string]$PayloadImagePath = "",

    [uint32]$PayloadSlot = 1,

    [string[]]$FlatBinaryImagePath = @(),

    [uint32[]]$FlatBinaryPayloadSlot = @(),

    [string]$OutputSignaturePath = ""
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

function Convert-BytesToHex {
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    $builder = New-Object System.Text.StringBuilder
    foreach ($byte in $Bytes) {
        [void]$builder.AppendFormat("{0:x2}", $byte)
    }
    return $builder.ToString()
}

function Convert-HexToCBytes {
    param([Parameter(Mandatory = $true)][string]$Hex)

    $values = New-Object 'System.Collections.Generic.List[string]'
    for ($i = 0; $i -lt $Hex.Length; $i += 2) {
        $values.Add(("0x{0}" -f $Hex.Substring($i, 2).ToUpperInvariant()))
    }
    return $values
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
$payloadRecordSlots = @{}
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
    $payloadRecordSlots[$payloadSlotValue] = $true
}

foreach ($payloadOverride in @($payloadOverrides.Values | Sort-Object Slot)) {
    $payloadSlotValue = [uint32]$payloadOverride.Slot
    if ($payloadRecordSlots.ContainsKey($payloadSlotValue)) {
        continue
    }

    $payloadRecords += [pscustomobject]@{
        Slot = $payloadSlotValue
        Kind = Resolve-PayloadKind "flat-binary"
        ImageOffset = [uint32]0
        ImageSize = [uint32]$payloadOverride.Size
        ImageChecksum = [uint32]$payloadOverride.Checksum
    }
    $payloadRecordSlots[$payloadSlotValue] = $true
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

if ($OutputSignaturePath.Trim().Length -gt 0) {
    $signatureFullPath = [System.IO.Path]::GetFullPath($OutputSignaturePath)
    $signatureDir = Split-Path -Parent $signatureFullPath
    New-Item -ItemType Directory -Force $signatureDir | Out-Null

    $payloadSigningInputs = @()
    foreach ($payload in $payloadRecords) {
        if ($payloadOverrides.ContainsKey([uint32]$payload.Slot)) {
            $payloadOverride = $payloadOverrides[[uint32]$payload.Slot]
            $payloadSigningInputs += [pscustomobject]@{
                slot = [uint32]$payload.Slot
                size = [uint32]$payload.ImageSize
                checksum = [uint32]$payload.ImageChecksum
                path = $payloadOverride.Path
            }
        }
    }

    $signInput = [pscustomobject]@{
        archiveHex = Convert-BytesToHex -Bytes $archive
        payloads = $payloadSigningInputs
    }
    $signInputPath = Join-Path $outputDir "package_store_sign_input.json"
    $signOutputPath = Join-Path $outputDir "package_store_sign_output.json"
    $signInput | ConvertTo-Json -Depth 6 | Set-Content -Path $signInputPath -Encoding ASCII

    $signScript = @'
import json
import sys
import hashlib
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization

input_path, output_path = sys.argv[1], sys.argv[2]
with open(input_path, "r", encoding="ascii") as handle:
    data = json.load(handle)

private_key = Ed25519PrivateKey.generate()
wrong_key = Ed25519PrivateKey.generate()
public_key = private_key.public_key().public_bytes(
    encoding=serialization.Encoding.Raw,
    format=serialization.PublicFormat.Raw,
)
archive = bytes.fromhex(data["archiveHex"])
payload_results = []
for payload in data["payloads"]:
    with open(payload["path"], "rb") as handle:
        payload_bytes = handle.read()
    prefix = (
        b"LimitlessOS-M7-payload-v1\0"
        + int(payload["slot"]).to_bytes(4, "little")
        + int(payload["size"]).to_bytes(4, "little")
        + int(payload["checksum"]).to_bytes(4, "little")
    )
    payload_results.append({
        "slot": int(payload["slot"]),
        "size": int(payload["size"]),
        "checksum": int(payload["checksum"]),
        "signatureHex": private_key.sign(prefix + payload_bytes).hex(),
    })

current_index = (
    "limitlessos-update-index-v1\n"
    "sequence=7\n"
    "package-count=%d\n" % len(payload_results)
).encode("ascii")
rollback_index = (
    "limitlessos-update-index-v1\n"
    "sequence=6\n"
    "package-count=%d\n" % len(payload_results)
).encode("ascii")
index_prefix = b"LimitlessOS-M7-update-index-v1\0"
descriptor_prefix = b"LimitlessOS-M12-idprovider-v1\0"
cloud_descriptor_prefix = b"LimitlessOS-M14-cloud-provider-v1\0"
public_key_id = int.from_bytes(public_key[:4], "little")
public_key_fingerprint = hashlib.sha256(public_key).hexdigest().upper()

def make_identity_descriptor(sequence, descriptor_version=1, protocol_version=1):
    return (
        "limitlessos-identity-provider-v1\n"
        "provider-id=personal.fixture.limitless\n"
        "provider-type=personal\n"
        "display-name=Limitless Personal Fixture\n"
        "descriptor-version=%d\n"
        "protocol-version=%d\n"
        "endpoint=fixture://identity/personal\n"
        "endpoint-public-key-id=IDP-FIXTURE-01\n"
        "endpoint-public-key-fingerprint=5F1B9F1F67D6B9D3B7F0B8D0B155C9A9F6A2C46E0A477BB4450F21658CFD2B12\n"
        "supported-auth=descriptor-only\n"
        "required-transport-security=encrypted\n"
        "account-association=unavailable\n"
        "token-persistence=denied\n"
        "minimum-os-version=M12\n"
        "sequence=%d\n"
        "trusted-time-required=0\n"
        "expiry=not-enforceable-without-trusted-time\n"
        "signer-key-id=0x%08X\n"
        "signer-fingerprint=%s\n"
    ) % (descriptor_version, protocol_version, sequence, public_key_id, public_key_fingerprint)

identity_descriptor = make_identity_descriptor(12).encode("ascii")
identity_descriptor_rollback = make_identity_descriptor(11).encode("ascii")
identity_descriptor_unsupported = make_identity_descriptor(12, descriptor_version=99).encode("ascii")

def make_cloud_descriptor(sequence, descriptor_version=1, protocol_version=1, include_provider_type=True):
    provider_type_line = "provider-type=cloud-storage\n" if include_provider_type else ""
    return (
        "limitlessos-cloud-provider-v1\n"
        "provider-id=cloud.fixture.limitless\n"
        + provider_type_line +
        "display-name=Limitless Cloud Fixture\n"
        "descriptor-version=%d\n"
        "protocol-version=%d\n"
        "endpoint=fixture://cloud/storage\n"
        "endpoint-public-key-id=CLOUD-FIXTURE-01\n"
        "endpoint-public-key-fingerprint=48B57D1348F32F6A4E19381CB2222D7CBFD2F7C11F84C8C0B05E981B2CEEC2E4\n"
        "supported-modes=descriptor-only\n"
        "token-policy=denied\n"
        "offline-cache-policy=planned-unavailable\n"
        "sync-policy=unavailable\n"
        "required-transport-security=encrypted\n"
        "required-account-association=personal-or-enterprise\n"
        "minimum-os-version=M14\n"
        "sequence=%d\n"
        "trusted-time-required=0\n"
        "expiry=not-enforceable-without-trusted-time\n"
        "signer-key-id=0x%08X\n"
        "signer-fingerprint=%s\n"
    ) % (descriptor_version, protocol_version, sequence, public_key_id, public_key_fingerprint)

cloud_descriptor = make_cloud_descriptor(14).encode("ascii")
cloud_descriptor_rollback = make_cloud_descriptor(13).encode("ascii")
cloud_descriptor_unsupported = make_cloud_descriptor(14, descriptor_version=99).encode("ascii")
cloud_descriptor_malformed = make_cloud_descriptor(14, include_provider_type=False).encode("ascii")
result = {
    "algorithm": "Ed25519",
    "publicKeyHex": public_key.hex(),
    "publicKeyId": public_key_id,
    "publicKeyFingerprint": public_key_fingerprint,
    "archiveSignatureHex": private_key.sign(b"LimitlessOS-M7-archive-v1\0" + archive).hex(),
    "wrongKeyArchiveSignatureHex": wrong_key.sign(b"LimitlessOS-M7-archive-v1\0" + archive).hex(),
    "updateIndexSequence": 7,
    "updateIndexHex": current_index.hex(),
    "updateIndexSignatureHex": private_key.sign(index_prefix + current_index).hex(),
    "wrongKeyUpdateIndexSignatureHex": wrong_key.sign(index_prefix + current_index).hex(),
    "rollbackIndexSequence": 6,
    "rollbackIndexHex": rollback_index.hex(),
    "rollbackIndexSignatureHex": private_key.sign(index_prefix + rollback_index).hex(),
    "identityProviderDescriptorSequence": 12,
    "identityProviderDescriptorHex": identity_descriptor.hex(),
    "identityProviderDescriptorSignatureHex": private_key.sign(descriptor_prefix + identity_descriptor).hex(),
    "wrongKeyIdentityProviderDescriptorSignatureHex": wrong_key.sign(descriptor_prefix + identity_descriptor).hex(),
    "identityProviderDescriptorRollbackSequence": 11,
    "identityProviderDescriptorRollbackHex": identity_descriptor_rollback.hex(),
    "identityProviderDescriptorRollbackSignatureHex": private_key.sign(descriptor_prefix + identity_descriptor_rollback).hex(),
    "identityProviderDescriptorUnsupportedHex": identity_descriptor_unsupported.hex(),
    "identityProviderDescriptorUnsupportedSignatureHex": private_key.sign(descriptor_prefix + identity_descriptor_unsupported).hex(),
    "cloudProviderDescriptorSequence": 14,
    "cloudProviderDescriptorHex": cloud_descriptor.hex(),
    "cloudProviderDescriptorSignatureHex": private_key.sign(cloud_descriptor_prefix + cloud_descriptor).hex(),
    "wrongKeyCloudProviderDescriptorSignatureHex": wrong_key.sign(cloud_descriptor_prefix + cloud_descriptor).hex(),
    "cloudProviderDescriptorRollbackSequence": 13,
    "cloudProviderDescriptorRollbackHex": cloud_descriptor_rollback.hex(),
    "cloudProviderDescriptorRollbackSignatureHex": private_key.sign(cloud_descriptor_prefix + cloud_descriptor_rollback).hex(),
    "cloudProviderDescriptorUnsupportedHex": cloud_descriptor_unsupported.hex(),
    "cloudProviderDescriptorUnsupportedSignatureHex": private_key.sign(cloud_descriptor_prefix + cloud_descriptor_unsupported).hex(),
    "cloudProviderDescriptorMalformedHex": cloud_descriptor_malformed.hex(),
    "cloudProviderDescriptorMalformedSignatureHex": private_key.sign(cloud_descriptor_prefix + cloud_descriptor_malformed).hex(),
    "payloads": payload_results,
}
with open(output_path, "w", encoding="ascii") as handle:
    json.dump(result, handle, indent=2, sort_keys=True)
'@
    $signScriptPath = Join-Path $outputDir "package_store_sign.py"
    Set-Content -Path $signScriptPath -Value $signScript -Encoding ASCII
    & python $signScriptPath $signInputPath $signOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to sign bootstrap package archive."
    }

    $signOutput = Get-Content -Raw -Path $signOutputPath | ConvertFrom-Json
    $sigLines = New-Object 'System.Collections.Generic.List[string]'
    $sigLines.Add("#ifndef LIMITLESS_PACKAGE_STORE_SIGNATURES_GENERATED_H")
    $sigLines.Add("#define LIMITLESS_PACKAGE_STORE_SIGNATURES_GENERATED_H")
    $sigLines.Add("")
    $sigLines.Add("// Generated by tools/generate-package-store.ps1. Contains public-key and signatures only.")
    $sigLines.Add("#define PACKAGE_STORE_SIGNATURE_ALGORITHM_ED25519 1u")
    $sigLines.Add(("#define PACKAGE_STORE_SIGNATURE_PUBLIC_KEY_ID 0x{0:X8}u" -f ([uint32]$signOutput.publicKeyId)))
    $sigLines.Add(("#define PACKAGE_STORE_SIGNATURE_PUBLIC_KEY_FINGERPRINT ""{0}""" -f ([string]$signOutput.publicKeyFingerprint)))
    $sigLines.Add(("#define PACKAGE_STORE_SIGNATURE_PAYLOAD_COUNT {0}u" -f @($signOutput.payloads).Count))
    $sigLines.Add(("#define PACKAGE_STORE_UPDATE_INDEX_SEQUENCE {0}u" -f ([uint32]$signOutput.updateIndexSequence)))
    $sigLines.Add(("#define PACKAGE_STORE_UPDATE_INDEX_ROLLBACK_SEQUENCE {0}u" -f ([uint32]$signOutput.rollbackIndexSequence)))
    $sigLines.Add(("#define PACKAGE_STORE_UPDATE_INDEX_BYTES {0}u" -f (([string]$signOutput.updateIndexHex).Length / 2)))
    $sigLines.Add(("#define PACKAGE_STORE_UPDATE_INDEX_ROLLBACK_BYTES {0}u" -f (([string]$signOutput.rollbackIndexHex).Length / 2)))
    $sigLines.Add(("#define IDENTITY_PROVIDER_DESCRIPTOR_SEQUENCE {0}u" -f ([uint32]$signOutput.identityProviderDescriptorSequence)))
    $sigLines.Add(("#define IDENTITY_PROVIDER_DESCRIPTOR_ROLLBACK_SEQUENCE {0}u" -f ([uint32]$signOutput.identityProviderDescriptorRollbackSequence)))
    $sigLines.Add(("#define IDENTITY_PROVIDER_DESCRIPTOR_BYTES {0}u" -f (([string]$signOutput.identityProviderDescriptorHex).Length / 2)))
    $sigLines.Add(("#define IDENTITY_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES {0}u" -f (([string]$signOutput.identityProviderDescriptorRollbackHex).Length / 2)))
    $sigLines.Add(("#define IDENTITY_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES {0}u" -f (([string]$signOutput.identityProviderDescriptorUnsupportedHex).Length / 2)))
    $sigLines.Add(("#define CLOUD_PROVIDER_DESCRIPTOR_SEQUENCE {0}u" -f ([uint32]$signOutput.cloudProviderDescriptorSequence)))
    $sigLines.Add(("#define CLOUD_PROVIDER_DESCRIPTOR_ROLLBACK_SEQUENCE {0}u" -f ([uint32]$signOutput.cloudProviderDescriptorRollbackSequence)))
    $sigLines.Add(("#define CLOUD_PROVIDER_DESCRIPTOR_BYTES {0}u" -f (([string]$signOutput.cloudProviderDescriptorHex).Length / 2)))
    $sigLines.Add(("#define CLOUD_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES {0}u" -f (([string]$signOutput.cloudProviderDescriptorRollbackHex).Length / 2)))
    $sigLines.Add(("#define CLOUD_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES {0}u" -f (([string]$signOutput.cloudProviderDescriptorUnsupportedHex).Length / 2)))
    $sigLines.Add(("#define CLOUD_PROVIDER_DESCRIPTOR_MALFORMED_BYTES {0}u" -f (([string]$signOutput.cloudProviderDescriptorMalformedHex).Length / 2)))
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_signature_public_key[32] = {")
    $publicKeyValues = Convert-HexToCBytes -Hex ([string]$signOutput.publicKeyHex)
    $sigLines.Add("    " + ($publicKeyValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_signature_archive[64] = {")
    $archiveSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.archiveSignatureHex)
    $sigLines.Add("    " + ($archiveSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_signature_archive_wrong_key[64] = {")
    $wrongKeyArchiveSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.wrongKeyArchiveSignatureHex)
    $sigLines.Add("    " + ($wrongKeyArchiveSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_update_index[PACKAGE_STORE_UPDATE_INDEX_BYTES] = {")
    $updateIndexValues = Convert-HexToCBytes -Hex ([string]$signOutput.updateIndexHex)
    $sigLines.Add("    " + ($updateIndexValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_update_index_signature[64] = {")
    $updateIndexSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.updateIndexSignatureHex)
    $sigLines.Add("    " + ($updateIndexSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_update_index_wrong_key_signature[64] = {")
    $wrongKeyUpdateIndexSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.wrongKeyUpdateIndexSignatureHex)
    $sigLines.Add("    " + ($wrongKeyUpdateIndexSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_update_index_rollback[PACKAGE_STORE_UPDATE_INDEX_ROLLBACK_BYTES] = {")
    $rollbackIndexValues = Convert-HexToCBytes -Hex ([string]$signOutput.rollbackIndexHex)
    $sigLines.Add("    " + ($rollbackIndexValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 package_store_update_index_rollback_signature[64] = {")
    $rollbackIndexSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.rollbackIndexSignatureHex)
    $sigLines.Add("    " + ($rollbackIndexSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 identity_provider_descriptor[IDENTITY_PROVIDER_DESCRIPTOR_BYTES] = {")
    $identityDescriptorValues = Convert-HexToCBytes -Hex ([string]$signOutput.identityProviderDescriptorHex)
    $sigLines.Add("    " + ($identityDescriptorValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 identity_provider_descriptor_signature[64] = {")
    $identityDescriptorSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.identityProviderDescriptorSignatureHex)
    $sigLines.Add("    " + ($identityDescriptorSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 identity_provider_descriptor_wrong_key_signature[64] = {")
    $identityDescriptorWrongKeySignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.wrongKeyIdentityProviderDescriptorSignatureHex)
    $sigLines.Add("    " + ($identityDescriptorWrongKeySignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 identity_provider_descriptor_rollback[IDENTITY_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES] = {")
    $identityDescriptorRollbackValues = Convert-HexToCBytes -Hex ([string]$signOutput.identityProviderDescriptorRollbackHex)
    $sigLines.Add("    " + ($identityDescriptorRollbackValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 identity_provider_descriptor_rollback_signature[64] = {")
    $identityDescriptorRollbackSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.identityProviderDescriptorRollbackSignatureHex)
    $sigLines.Add("    " + ($identityDescriptorRollbackSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 identity_provider_descriptor_unsupported[IDENTITY_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES] = {")
    $identityDescriptorUnsupportedValues = Convert-HexToCBytes -Hex ([string]$signOutput.identityProviderDescriptorUnsupportedHex)
    $sigLines.Add("    " + ($identityDescriptorUnsupportedValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 identity_provider_descriptor_unsupported_signature[64] = {")
    $identityDescriptorUnsupportedSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.identityProviderDescriptorUnsupportedSignatureHex)
    $sigLines.Add("    " + ($identityDescriptorUnsupportedSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor[CLOUD_PROVIDER_DESCRIPTOR_BYTES] = {")
    $cloudDescriptorValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorHex)
    $sigLines.Add("    " + ($cloudDescriptorValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_signature[64] = {")
    $cloudDescriptorSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorSignatureHex)
    $sigLines.Add("    " + ($cloudDescriptorSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_wrong_key_signature[64] = {")
    $cloudDescriptorWrongKeySignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.wrongKeyCloudProviderDescriptorSignatureHex)
    $sigLines.Add("    " + ($cloudDescriptorWrongKeySignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_rollback[CLOUD_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES] = {")
    $cloudDescriptorRollbackValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorRollbackHex)
    $sigLines.Add("    " + ($cloudDescriptorRollbackValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_rollback_signature[64] = {")
    $cloudDescriptorRollbackSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorRollbackSignatureHex)
    $sigLines.Add("    " + ($cloudDescriptorRollbackSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_unsupported[CLOUD_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES] = {")
    $cloudDescriptorUnsupportedValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorUnsupportedHex)
    $sigLines.Add("    " + ($cloudDescriptorUnsupportedValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_unsupported_signature[64] = {")
    $cloudDescriptorUnsupportedSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorUnsupportedSignatureHex)
    $sigLines.Add("    " + ($cloudDescriptorUnsupportedSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_malformed[CLOUD_PROVIDER_DESCRIPTOR_MALFORMED_BYTES] = {")
    $cloudDescriptorMalformedValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorMalformedHex)
    $sigLines.Add("    " + ($cloudDescriptorMalformedValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("static const u8 cloud_provider_descriptor_malformed_signature[64] = {")
    $cloudDescriptorMalformedSignatureValues = Convert-HexToCBytes -Hex ([string]$signOutput.cloudProviderDescriptorMalformedSignatureHex)
    $sigLines.Add("    " + ($cloudDescriptorMalformedSignatureValues -join ", "))
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("struct package_store_payload_signature_generated { u32 slot; u32 size; u32 checksum; u8 signature[64]; };")
    $sigLines.Add("static const struct package_store_payload_signature_generated package_store_payload_signatures[PACKAGE_STORE_SIGNATURE_PAYLOAD_COUNT] = {")
    foreach ($payload in @($signOutput.payloads)) {
        $payloadSignatureValues = Convert-HexToCBytes -Hex ([string]$payload.signatureHex)
        $sigLines.Add(("    {{ {0}u, {1}u, 0x{2:X8}u, {{ {3} }} }}," -f `
            ([uint32]$payload.slot), `
            ([uint32]$payload.size), `
            ([uint32]$payload.checksum), `
            ($payloadSignatureValues -join ", ")))
    }
    $sigLines.Add("};")
    $sigLines.Add("")
    $sigLines.Add("#endif")
    Set-Content -Path $signatureFullPath -Value $sigLines -Encoding ASCII
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
foreach ($payloadOverride in $payloadOverrides.Values) {
    Write-Host ("  payload slot {0}: {1} bytes checksum 0x{2:X8} from {3}" -f `
        $payloadOverride.Slot, `
        $payloadOverride.Size, `
        $payloadOverride.Checksum, `
        $payloadOverride.Path)
}
