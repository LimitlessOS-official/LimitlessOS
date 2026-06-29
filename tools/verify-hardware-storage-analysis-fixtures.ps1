param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m116-hardware-storage-analysis-fixtures"
}

$captureDir = Join-Path $OutputDir "captures"
$analysisDir = Join-Path $OutputDir "analysis"
New-Item -ItemType Directory -Force -Path $captureDir | Out-Null
New-Item -ItemType Directory -Force -Path $analysisDir | Out-Null

$fieldOrder = @(
    "storage-triage",
    "nvme-found",
    "pci-storage",
    "pci-nvme",
    "pci-raid",
    "pci-other-storage",
    "pci-intel-system",
    "pci-vmd",
    "nvme-pci",
    "nvme-vendor-device",
    "nvme-class",
    "nvme-bar0",
    "nvme-bar1",
    "nvme-mmio-low",
    "nvme-mmio-high",
    "nvme-mmio-span",
    "nvme-mmio-flags",
    "nvme-mmio-token",
    "other-storage-pci",
    "other-storage-vendor-device",
    "other-storage-class",
    "other-storage-bar0",
    "other-storage-bar1",
    "intel-system-pci",
    "intel-system-vendor-device",
    "intel-system-class",
    "intel-system-bar0",
    "intel-system-bar1",
    "vmd-pci",
    "vmd-vendor-device",
    "vmd-class",
    "vmd-bar0",
    "vmd-bar1",
    "vmd-mmio-low",
    "vmd-mmio-high",
    "vmd-mmio-span",
    "vmd-mmio-flags",
    "vmd-mmio-token",
    "vmd-nested-plan",
    "vmd-nested-enum",
    "vmd-nested-nvme",
    "vmd-nested-status",
    "vmd-nested-token",
    "vmd-nested-pci",
    "vmd-nested-vendor-device",
    "vmd-nested-class",
    "vmd-nested-bar0",
    "vmd-nested-bar1",
    "vmd-nested-scan-buses",
    "vmd-nested-scan-devices",
    "vmd-nested-scan-functions",
    "vmd-nested-scan-windows",
    "vmd-nested-scan-truncated",
    "vmd-nested-mmio-low",
    "vmd-nested-mmio-high",
    "vmd-nested-mmio-span",
    "vmd-nested-mmio-flags",
    "vmd-nested-mmio-token",
    "vmd-nested-bind-ready",
    "vmd-nested-bind-status",
    "vmd-nested-bind-token",
    "nvme-ready",
    "nvme-identify",
    "ioq",
    "read-issued",
    "read-completed",
    "read-status",
    "gpt-signature",
    "gpt-partitions",
    "fat32-start",
    "fat32-sectors",
    "gpt-vbr",
    "fat-bpb",
    "fat-located",
    "fat-unavailable",
    "fat-error",
    "rw-cap",
    "rw-delegated",
    "rw-error",
    "apps-stat",
    "apps-type",
    "apps-dirent",
    "apps-dir-result",
    "busybox-stat",
    "busybox-bytes",
    "dynldlimit-stat",
    "dynldlimit-bytes",
    "ldlimit-stat",
    "ldlimit-bytes",
    "boot-staged",
    "boot-app-bytes",
    "boot-interp-bytes",
    "boot-status",
    "stage-expected",
    "dynldlimit-expected",
    "ldlimit-expected",
    "dynldlimit-match",
    "ldlimit-match",
    "stage-match",
    "token"
)

$baseFields = @{
    "storage-triage" = "1"
    "nvme-found" = "1"
    "pci-storage" = "1"
    "pci-nvme" = "1"
    "pci-raid" = "0"
    "pci-other-storage" = "0"
    "pci-intel-system" = "0"
    "pci-vmd" = "0"
    "nvme-pci" = "0x00000400"
    "nvme-vendor-device" = "0x001F1AF4"
    "nvme-class" = "0x01080200"
    "nvme-bar0" = "0xC0008004"
    "nvme-bar1" = "0x00000000"
    "nvme-mmio-low" = "0xC0008000"
    "nvme-mmio-high" = "0x00000000"
    "nvme-mmio-span" = "8192"
    "nvme-mmio-flags" = "0x000001FF"
    "nvme-mmio-token" = "0xA93E3D7A"
    "other-storage-pci" = "0xFFFFFFFF"
    "other-storage-vendor-device" = "0x00000000"
    "other-storage-class" = "0x00000000"
    "other-storage-bar0" = "0x00000000"
    "other-storage-bar1" = "0x00000000"
    "intel-system-pci" = "0xFFFFFFFF"
    "intel-system-vendor-device" = "0x00000000"
    "intel-system-class" = "0x00000000"
    "intel-system-bar0" = "0x00000000"
    "intel-system-bar1" = "0x00000000"
    "vmd-pci" = "0xFFFFFFFF"
    "vmd-vendor-device" = "0x00000000"
    "vmd-class" = "0x00000000"
    "vmd-bar0" = "0x00000000"
    "vmd-bar1" = "0x00000000"
    "vmd-mmio-low" = "0x00000000"
    "vmd-mmio-high" = "0x00000000"
    "vmd-mmio-span" = "0"
    "vmd-mmio-flags" = "0x00000000"
    "vmd-mmio-token" = "0x00000000"
    "vmd-nested-plan" = "0"
    "vmd-nested-enum" = "0"
    "vmd-nested-nvme" = "0"
    "vmd-nested-status" = "0"
    "vmd-nested-token" = "0x00000000"
    "vmd-nested-pci" = "0xFFFFFFFF"
    "vmd-nested-vendor-device" = "0x00000000"
    "vmd-nested-class" = "0x00000000"
    "vmd-nested-bar0" = "0x00000000"
    "vmd-nested-bar1" = "0x00000000"
    "vmd-nested-scan-buses" = "0"
    "vmd-nested-scan-devices" = "0"
    "vmd-nested-scan-functions" = "0"
    "vmd-nested-scan-windows" = "0"
    "vmd-nested-scan-truncated" = "0"
    "vmd-nested-mmio-low" = "0x00000000"
    "vmd-nested-mmio-high" = "0x00000000"
    "vmd-nested-mmio-span" = "0"
    "vmd-nested-mmio-flags" = "0x00000000"
    "vmd-nested-mmio-token" = "0x00000000"
    "vmd-nested-bind-ready" = "0"
    "vmd-nested-bind-status" = "0"
    "vmd-nested-bind-token" = "0x00000000"
    "nvme-ready" = "1"
    "nvme-identify" = "1"
    "ioq" = "1"
    "read-issued" = "1"
    "read-completed" = "1"
    "read-status" = "0"
    "gpt-signature" = "1"
    "gpt-partitions" = "6"
    "fat32-start" = "2048"
    "fat32-sectors" = "8192"
    "gpt-vbr" = "1"
    "fat-bpb" = "1"
    "fat-located" = "1"
    "fat-unavailable" = "0"
    "fat-error" = "0"
    "rw-cap" = "1"
    "rw-delegated" = "1"
    "rw-error" = "0"
    "apps-stat" = "1"
    "apps-type" = "2"
    "apps-dirent" = "1"
    "apps-dir-result" = "1"
    "busybox-stat" = "0"
    "busybox-bytes" = "0"
    "dynldlimit-stat" = "1"
    "dynldlimit-bytes" = "15680"
    "ldlimit-stat" = "1"
    "ldlimit-bytes" = "16704"
    "boot-staged" = "1"
    "boot-app-bytes" = "15680"
    "boot-interp-bytes" = "16704"
    "boot-status" = "0"
    "stage-expected" = "1"
    "dynldlimit-expected" = "1"
    "ldlimit-expected" = "1"
    "dynldlimit-match" = "1"
    "ldlimit-match" = "1"
    "stage-match" = "1"
    "token" = "0x75BC2409"
}

function New-Fields
{
    $copy = @{}
    foreach ($key in $baseFields.Keys) {
        $copy[$key] = $baseFields[$key]
    }
    return $copy
}

function New-TriageLine
{
    param([hashtable]$Fields)

    $parts = @()
    foreach ($field in $fieldOrder) {
        $parts += ("{0} {1}" -f $field, $Fields[$field])
    }
    return "[x64] drs-nvme-triage " + ($parts -join " ")
}

function New-Fixture
{
    param(
        [string]$Name,
        [string]$ExpectedStage,
        [hashtable]$Mutations = @{},
        [string]$Mode = "triage"
    )

    return [PSCustomObject]@{
        name = $Name
        expected_stage = $ExpectedStage
        mutations = $Mutations
        mode = $Mode
    }
}

$fixtures = @(
    (New-Fixture -Name "missing-storage-triage" -ExpectedStage "missing-storage-triage" -Mode "missing"),
    (New-Fixture -Name "legacy-realbin-unavailable" -ExpectedStage "legacy-realbin-unavailable" -Mode "legacy"),
    (New-Fixture -Name "pci-storage-discovery" -ExpectedStage "pci-storage-discovery" -Mutations @{ "pci-storage" = "0"; "pci-nvme" = "0"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-hidden-by-raid" -ExpectedStage "pci-nvme-hidden-by-raid" -Mutations @{ "pci-nvme" = "0"; "pci-raid" = "1"; "pci-other-storage" = "1"; "other-storage-pci" = "0x00001700"; "other-storage-vendor-device" = "0x51D38086"; "other-storage-class" = "0x01040000"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-enumeration" -ExpectedStage "pci-vmd-nested-enumeration" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "1"; "vmd-nested-enum" = "0"; "vmd-nested-nvme" = "0"; "vmd-nested-status" = "1"; "vmd-nested-token" = "0xD204D931"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-bdf" -ExpectedStage "pci-vmd-bdf" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0xFFFFFFFF"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-identity" -ExpectedStage "pci-vmd-identity" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x00000000"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-class-code" -ExpectedStage "pci-vmd-class-code" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x01040000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-bar0" -ExpectedStage "pci-vmd-bar0" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0x00000000"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-mmio-base" -ExpectedStage "pci-vmd-mmio-base" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0x00000000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-mmio-span" -ExpectedStage "pci-vmd-mmio-span" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "0"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-mmio-flags" -ExpectedStage "pci-vmd-mmio-flags" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x00000000"; "vmd-mmio-token" = "0x94D5D769"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-plan" -ExpectedStage "pci-vmd-nested-plan" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "65536"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "0"; "vmd-nested-enum" = "0"; "vmd-nested-nvme" = "0"; "vmd-nested-status" = "0"; "vmd-nested-token" = "0x00000000"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-nvme-class" -ExpectedStage "pci-vmd-nested-nvme-class" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "1048576"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "1"; "vmd-nested-enum" = "1"; "vmd-nested-nvme" = "0"; "vmd-nested-status" = "2"; "vmd-nested-token" = "0xD204D932"; "vmd-nested-pci" = "0x00000000"; "vmd-nested-vendor-device" = "0x467F8086"; "vmd-nested-class" = "0x06040000"; "vmd-nested-bar0" = "0x00000000"; "vmd-nested-bar1" = "0x00000000"; "vmd-nested-scan-buses" = "1"; "vmd-nested-scan-devices" = "32"; "vmd-nested-scan-functions" = "256"; "vmd-nested-scan-windows" = "16"; "vmd-nested-scan-truncated" = "1"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-nvme-mmio-base" -ExpectedStage "pci-vmd-nested-nvme-mmio-base" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "1048576"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "1"; "vmd-nested-enum" = "1"; "vmd-nested-nvme" = "1"; "vmd-nested-status" = "3"; "vmd-nested-token" = "0xD204D933"; "vmd-nested-pci" = "0x00000100"; "vmd-nested-vendor-device" = "0x00101B36"; "vmd-nested-class" = "0x01080202"; "vmd-nested-bar0" = "0x00000000"; "vmd-nested-bar1" = "0x00000000"; "vmd-nested-scan-buses" = "1"; "vmd-nested-scan-devices" = "32"; "vmd-nested-scan-functions" = "256"; "vmd-nested-scan-windows" = "16"; "vmd-nested-scan-truncated" = "1"; "vmd-nested-mmio-low" = "0x00000000"; "vmd-nested-mmio-high" = "0x00000000"; "vmd-nested-mmio-span" = "16384"; "vmd-nested-mmio-flags" = "0x00000183"; "vmd-nested-mmio-token" = "0xA1450CC1"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-nvme-mmio-span" -ExpectedStage "pci-vmd-nested-nvme-mmio-span" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "1048576"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "1"; "vmd-nested-enum" = "1"; "vmd-nested-nvme" = "1"; "vmd-nested-status" = "3"; "vmd-nested-token" = "0xD204D933"; "vmd-nested-pci" = "0x00000100"; "vmd-nested-vendor-device" = "0x00101B36"; "vmd-nested-class" = "0x01080202"; "vmd-nested-bar0" = "0xFE020004"; "vmd-nested-bar1" = "0x00000000"; "vmd-nested-scan-buses" = "1"; "vmd-nested-scan-devices" = "32"; "vmd-nested-scan-functions" = "256"; "vmd-nested-scan-windows" = "16"; "vmd-nested-scan-truncated" = "1"; "vmd-nested-mmio-low" = "0xFE020000"; "vmd-nested-mmio-high" = "0x00000000"; "vmd-nested-mmio-span" = "0"; "vmd-nested-mmio-flags" = "0x000001FF"; "vmd-nested-mmio-token" = "0xA1450CC2"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-nvme-mmio-flags" -ExpectedStage "pci-vmd-nested-nvme-mmio-flags" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "1048576"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "1"; "vmd-nested-enum" = "1"; "vmd-nested-nvme" = "1"; "vmd-nested-status" = "3"; "vmd-nested-token" = "0xD204D933"; "vmd-nested-pci" = "0x00000100"; "vmd-nested-vendor-device" = "0x00101B36"; "vmd-nested-class" = "0x01080202"; "vmd-nested-bar0" = "0xFE020004"; "vmd-nested-bar1" = "0x00000000"; "vmd-nested-scan-buses" = "1"; "vmd-nested-scan-devices" = "32"; "vmd-nested-scan-functions" = "256"; "vmd-nested-scan-windows" = "16"; "vmd-nested-scan-truncated" = "1"; "vmd-nested-mmio-low" = "0xFE020000"; "vmd-nested-mmio-high" = "0x00000000"; "vmd-nested-mmio-span" = "16384"; "vmd-nested-mmio-flags" = "0x00000000"; "vmd-nested-mmio-token" = "0xA1450CC3"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-nvme-bind-ready" -ExpectedStage "pci-vmd-nested-nvme-bind-ready" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "1048576"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "1"; "vmd-nested-enum" = "1"; "vmd-nested-nvme" = "1"; "vmd-nested-status" = "3"; "vmd-nested-token" = "0xD204D933"; "vmd-nested-pci" = "0x00000100"; "vmd-nested-vendor-device" = "0x00101B36"; "vmd-nested-class" = "0x01080202"; "vmd-nested-bar0" = "0xFE020004"; "vmd-nested-bar1" = "0x00000000"; "vmd-nested-scan-buses" = "1"; "vmd-nested-scan-devices" = "32"; "vmd-nested-scan-functions" = "256"; "vmd-nested-scan-windows" = "16"; "vmd-nested-scan-truncated" = "1"; "vmd-nested-mmio-low" = "0xFE020000"; "vmd-nested-mmio-high" = "0x00000000"; "vmd-nested-mmio-span" = "16384"; "vmd-nested-mmio-flags" = "0x000001FF"; "vmd-nested-mmio-token" = "0xA1450CC4"; "vmd-nested-bind-ready" = "0"; "vmd-nested-bind-status" = "4"; "vmd-nested-bind-token" = "0xB1460CC0"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-vmd-nested-nvme-bind" -ExpectedStage "pci-vmd-nested-nvme-bind" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "pci-vmd" = "1"; "vmd-pci" = "0x00000E00"; "vmd-vendor-device" = "0x467F8086"; "vmd-class" = "0x08800000"; "vmd-bar0" = "0xFE010004"; "vmd-bar1" = "0x00000000"; "vmd-mmio-low" = "0xFE010000"; "vmd-mmio-high" = "0x00000000"; "vmd-mmio-span" = "1048576"; "vmd-mmio-flags" = "0x000003FF"; "vmd-mmio-token" = "0x94D5D769"; "vmd-nested-plan" = "1"; "vmd-nested-enum" = "1"; "vmd-nested-nvme" = "1"; "vmd-nested-status" = "3"; "vmd-nested-token" = "0xD204D933"; "vmd-nested-pci" = "0x00000100"; "vmd-nested-vendor-device" = "0x00101B36"; "vmd-nested-class" = "0x01080202"; "vmd-nested-bar0" = "0xFE020004"; "vmd-nested-bar1" = "0x00000000"; "vmd-nested-scan-buses" = "1"; "vmd-nested-scan-devices" = "32"; "vmd-nested-scan-functions" = "256"; "vmd-nested-scan-windows" = "16"; "vmd-nested-scan-truncated" = "1"; "vmd-nested-mmio-low" = "0xFE020000"; "vmd-nested-mmio-high" = "0x00000000"; "vmd-nested-mmio-span" = "16384"; "vmd-nested-mmio-flags" = "0x000001FF"; "vmd-nested-mmio-token" = "0xA1450CC4"; "vmd-nested-bind-ready" = "1"; "vmd-nested-bind-status" = "5"; "vmd-nested-bind-token" = "0xB1460CC4"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-hidden-by-intel-system" -ExpectedStage "pci-nvme-hidden-by-intel-system" -Mutations @{ "pci-nvme" = "0"; "pci-intel-system" = "1"; "intel-system-pci" = "0x00000E00"; "intel-system-vendor-device" = "0x467F8086"; "intel-system-class" = "0x08800000"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-other-storage" -ExpectedStage "pci-nvme-other-storage" -Mutations @{ "pci-nvme" = "0"; "pci-other-storage" = "1"; "other-storage-pci" = "0x00001F00"; "other-storage-vendor-device" = "0x00011234"; "other-storage-class" = "0x01050000"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-class" -ExpectedStage "pci-nvme-class" -Mutations @{ "pci-nvme" = "0"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-bdf" -ExpectedStage "pci-nvme-bdf" -Mutations @{ "nvme-pci" = "0xFFFFFFFF"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-identity" -ExpectedStage "pci-nvme-identity" -Mutations @{ "nvme-vendor-device" = "0x00000000"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-class-code" -ExpectedStage "pci-nvme-class-code" -Mutations @{ "nvme-class" = "0x01060102"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-bar0" -ExpectedStage "pci-nvme-bar0" -Mutations @{ "nvme-bar0" = "0xFFFFFFFF"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-mmio-base" -ExpectedStage "pci-nvme-mmio-base" -Mutations @{ "nvme-mmio-low" = "0x00000000"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-mmio-span" -ExpectedStage "pci-nvme-mmio-span" -Mutations @{ "nvme-mmio-span" = "0"; "nvme-found" = "0" }),
    (New-Fixture -Name "pci-nvme-mmio-flags" -ExpectedStage "pci-nvme-mmio-flags" -Mutations @{ "nvme-mmio-flags" = "0xFFFFFFFF"; "nvme-found" = "0" }),
    (New-Fixture -Name "nvme-controller-discovery" -ExpectedStage "nvme-controller-discovery" -Mutations @{ "nvme-found" = "0" }),
    (New-Fixture -Name "nvme-controller-ready" -ExpectedStage "nvme-controller-ready" -Mutations @{ "nvme-ready" = "0" }),
    (New-Fixture -Name "nvme-identify" -ExpectedStage "nvme-identify" -Mutations @{ "nvme-identify" = "0" }),
    (New-Fixture -Name "nvme-io-queue" -ExpectedStage "nvme-io-queue" -Mutations @{ "ioq" = "0" }),
    (New-Fixture -Name "nvme-read-issue" -ExpectedStage "nvme-read-issue" -Mutations @{ "read-issued" = "0" }),
    (New-Fixture -Name "nvme-read-completion" -ExpectedStage "nvme-read-completion" -Mutations @{ "read-completed" = "0" }),
    (New-Fixture -Name "nvme-read-status" -ExpectedStage "nvme-read-status" -Mutations @{ "read-status" = "7" }),
    (New-Fixture -Name "gpt-signature" -ExpectedStage "gpt-signature" -Mutations @{ "gpt-signature" = "0" }),
    (New-Fixture -Name "gpt-partition-table" -ExpectedStage "gpt-partition-table" -Mutations @{ "gpt-partitions" = "0" }),
    (New-Fixture -Name "fat32-partition" -ExpectedStage "fat32-partition" -Mutations @{ "fat32-start" = "0" }),
    (New-Fixture -Name "fat32-vbr" -ExpectedStage "fat32-vbr" -Mutations @{ "gpt-vbr" = "0" }),
    (New-Fixture -Name "fat32-bpb" -ExpectedStage "fat32-bpb" -Mutations @{ "fat-bpb" = "0" }),
    (New-Fixture -Name "fat32-mount" -ExpectedStage "fat32-mount" -Mutations @{ "fat-located" = "0" }),
    (New-Fixture -Name "fat32-unavailable" -ExpectedStage "fat32-unavailable" -Mutations @{ "fat-unavailable" = "1" }),
    (New-Fixture -Name "fat32-error" -ExpectedStage "fat32-error" -Mutations @{ "fat-error" = "5" }),
    (New-Fixture -Name "storage-capability" -ExpectedStage "storage-capability" -Mutations @{ "rw-cap" = "0" }),
    (New-Fixture -Name "storage-capability-delegation" -ExpectedStage "storage-capability-delegation" -Mutations @{ "rw-delegated" = "0" }),
    (New-Fixture -Name "storage-capability-error" -ExpectedStage "storage-capability-error" -Mutations @{ "rw-error" = "9" }),
    (New-Fixture -Name "apps-directory-stat" -ExpectedStage "apps-directory-stat" -Mutations @{ "apps-stat" = "0" }),
    (New-Fixture -Name "apps-directory-type" -ExpectedStage "apps-directory-type" -Mutations @{ "apps-type" = "1" }),
    (New-Fixture -Name "apps-directory-read" -ExpectedStage "apps-directory-read" -Mutations @{ "apps-dirent" = "0" }),
    (New-Fixture -Name "boot-media-staging" -ExpectedStage "boot-media-staging" -Mutations @{ "boot-staged" = "0" }),
    (New-Fixture -Name "boot-media-app-size" -ExpectedStage "boot-media-app-size" -Mutations @{ "boot-app-bytes" = "1" }),
    (New-Fixture -Name "boot-media-interp-size" -ExpectedStage "boot-media-interp-size" -Mutations @{ "boot-interp-bytes" = "1" }),
    (New-Fixture -Name "nvme-dynldlimit-stat" -ExpectedStage "nvme-dynldlimit-stat" -Mutations @{ "dynldlimit-stat" = "0" }),
    (New-Fixture -Name "nvme-dynldlimit-size" -ExpectedStage "nvme-dynldlimit-size" -Mutations @{ "dynldlimit-bytes" = "1" }),
    (New-Fixture -Name "nvme-ldlimit-stat" -ExpectedStage "nvme-ldlimit-stat" -Mutations @{ "ldlimit-stat" = "0" }),
    (New-Fixture -Name "nvme-ldlimit-size" -ExpectedStage "nvme-ldlimit-size" -Mutations @{ "ldlimit-bytes" = "1" }),
    (New-Fixture -Name "stage-expected-flag" -ExpectedStage "stage-expected-flag" -Mutations @{ "stage-expected" = "0" }),
    (New-Fixture -Name "dynldlimit-match" -ExpectedStage "dynldlimit-match" -Mutations @{ "dynldlimit-match" = "0" }),
    (New-Fixture -Name "ldlimit-match" -ExpectedStage "ldlimit-match" -Mutations @{ "ldlimit-match" = "0" }),
    (New-Fixture -Name "stage-match" -ExpectedStage "stage-match" -Mutations @{ "stage-match" = "0" }),
    (New-Fixture -Name "storage-ready" -ExpectedStage "storage-ready")
)

$results = @()
$failures = @()
foreach ($fixture in $fixtures) {
    $capturePath = Join-Path $captureDir ($fixture.name + ".txt")
    if ($fixture.mode -eq "missing") {
        @(
            "[x64] hwval",
            "[x64] no storage telemetry in this fixture"
        ) | Set-Content -Path $capturePath -Encoding Ascii
    }
    elseif ($fixture.mode -eq "legacy") {
        @(
            "linux: NVMe FAT unavailable",
            "drs-realbin-unavailable bios 0 nvme 0"
        ) | Set-Content -Path $capturePath -Encoding Ascii
    }
    else {
        $fields = New-Fields
        foreach ($mutationKey in $fixture.mutations.Keys) {
            $fields[$mutationKey] = $fixture.mutations[$mutationKey]
        }
        New-TriageLine -Fields $fields | Set-Content -Path $capturePath -Encoding Ascii
    }

    $fixtureOutputDir = Join-Path $analysisDir $fixture.name
    $global:LASTEXITCODE = 0
    $analyzerOutput = & (Join-Path $root "tools\analyze-hardware-storage-capture.ps1") `
        -InputPath $capturePath `
        -OutputDir $fixtureOutputDir `
        -RequireStagedDynamicArtifacts 2>&1
    $exitCode = $LASTEXITCODE
    $analyzerOutput | Set-Content -Path (Join-Path $fixtureOutputDir "analyzer-console.txt") -Encoding Ascii

    $analysisPath = Join-Path $fixtureOutputDir "hardware-storage-analysis.json"
    if (-not (Test-Path $analysisPath)) {
        $failures += "$($fixture.name): analyzer did not write hardware-storage-analysis.json"
        continue
    }

    $analysis = Get-Content -Raw -Path $analysisPath | ConvertFrom-Json
    $actualStage = [string]$analysis.stage
    $expectedExitCode = if ($fixture.expected_stage -eq "storage-ready") { 0 } else { 2 }
    $diagnostic = $analysis.PSObject.Properties["diagnostic"]
    $diagnosticStage = ""
    $diagnosticComponent = ""
    $diagnosticFirstCheck = ""
    $diagnosticAcceptanceSignal = ""
    $diagnosticRequiredFields = @()
    if ($null -ne $diagnostic) {
        $diagnosticStage = [string]$diagnostic.Value.stage
        $diagnosticComponent = [string]$diagnostic.Value.component
        $diagnosticFirstCheck = [string]$diagnostic.Value.first_check
        $diagnosticAcceptanceSignal = [string]$diagnostic.Value.acceptance_signal
        $diagnosticRequiredFields = @($diagnostic.Value.required_fields)
    }

    $diagnosticPass = (($diagnosticStage -eq $fixture.expected_stage) -and
        (-not [string]::IsNullOrWhiteSpace($diagnosticComponent)) -and
        (-not [string]::IsNullOrWhiteSpace($diagnosticFirstCheck)) -and
        (-not [string]::IsNullOrWhiteSpace($diagnosticAcceptanceSignal)) -and
        ($diagnosticRequiredFields.Count -ne 0))
    $pass = (($actualStage -eq $fixture.expected_stage) -and ($exitCode -eq $expectedExitCode) -and $diagnosticPass)
    if (-not $pass) {
        $failures += ("{0}: expected stage {1}/exit {2}/diagnostic true, observed stage {3}/exit {4}/diagnostic {5}" -f $fixture.name, $fixture.expected_stage, $expectedExitCode, $actualStage, $exitCode, $diagnosticPass)
    }

    $results += [PSCustomObject]@{
        name = $fixture.name
        expected_stage = $fixture.expected_stage
        actual_stage = $actualStage
        expected_exit_code = $expectedExitCode
        actual_exit_code = $exitCode
        pass = $pass
        next_target = [string]$analysis.next_target
        diagnostic_component = $diagnosticComponent
        diagnostic_required_fields = $diagnosticRequiredFields.Count
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-hardware-storage-analysis-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = $fixtures.Count
    passed = ($results | Where-Object { $_.pass }).Count
    failed = $failures.Count
    failures = $failures
    results = $results
}

$summaryJsonPath = Join-Path $OutputDir "hardware-storage-analysis-fixtures.json"
$summaryTextPath = Join-Path $OutputDir "hardware-storage-analysis-fixtures.txt"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryJsonPath -Encoding Ascii

@(
    "hardware-storage-analysis-fixtures: $($summary.passed)/$($summary.total)",
    "failed: $($summary.failed)",
    "output-json: $summaryJsonPath"
) + ($results | ForEach-Object {
    "{0}: expected {1} observed {2} exit {3} pass {4}" -f $_.name, $_.expected_stage, $_.actual_stage, $_.actual_exit_code, $_.pass
}) | Set-Content -Path $summaryTextPath -Encoding Ascii

Write-Host "hardware-storage-analysis-fixtures: $($summary.passed)/$($summary.total)"
Write-Host "  failed: $($summary.failed)"
Write-Host "  output: $summaryJsonPath"

if ($failures.Count -ne 0) {
    foreach ($failure in $failures) {
        Write-Host "  failure: $failure"
    }
    exit 1
}
