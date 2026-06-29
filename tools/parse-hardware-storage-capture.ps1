param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [string]$OutputPath = "",

    [switch]$RequireStagedDynamicArtifacts,

    [uint32]$ExpectedDynamicAppBytes = 15680,

    [uint32]$ExpectedDynamicInterpBytes = 16704
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root "dist\hardware-storage-capture.json"
}
$InvalidU32 = [uint64]4294967295

function Get-FieldValue
{
    param(
        [hashtable]$Fields,
        [string]$Name,
        [uint64]$Default = 0
    )

    if ($Fields.ContainsKey($Name)) {
        return [uint64]$Fields[$Name]
    }
    return $Default
}

function Has-Field
{
    param(
        [hashtable]$Fields,
        [string]$Name
    )

    return $Fields.ContainsKey($Name)
}

function Convert-TokenValue
{
    param([string]$Value)

    if ($Value -match '^0x([0-9A-Fa-f]+)$') {
        return [Convert]::ToUInt64($Matches[1], 16)
    }
    return [Convert]::ToUInt64($Value, 10)
}

function Parse-TelemetryFields
{
    param([string]$Line)

    $fields = @{}
    $matches = [regex]::Matches($Line, '(?<!\S)([A-Za-z][A-Za-z0-9-]*)\s+(0x[0-9A-Fa-f]+|[0-9]+)(?!\S)')
    foreach ($match in $matches) {
        $name = $match.Groups[1].Value
        $value = Convert-TokenValue -Value $match.Groups[2].Value
        $fields[$name] = $value
    }
    return $fields
}

function New-Classification
{
    param(
        [string]$Stage,
        [string]$Detail,
        [bool]$Pass = $false
    )

    return [PSCustomObject]@{
        pass = $Pass
        stage = $Stage
        detail = $Detail
    }
}

function Classify-StorageCapture
{
    param(
        [hashtable]$Fields,
        [bool]$RequireStage,
        [uint32]$ExpectedAppBytes,
    [uint32]$ExpectedInterpBytes
    )

    if ((Has-Field -Fields $Fields -Name "pci-storage") -and ((Get-FieldValue -Fields $Fields -Name "pci-storage") -eq 0)) {
        return New-Classification -Stage "pci-storage-discovery" -Detail "No PCI storage-class controller was discovered."
    }
    if ((Has-Field -Fields $Fields -Name "pci-nvme") -and ((Get-FieldValue -Fields $Fields -Name "pci-nvme") -eq 0)) {
        if ((Has-Field -Fields $Fields -Name "pci-raid") -and ((Get-FieldValue -Fields $Fields -Name "pci-raid") -ne 0)) {
            return New-Classification -Stage "pci-nvme-hidden-by-raid" -Detail "PCI storage exists, but direct NVMe is hidden behind a RAID/RST-class storage controller."
        }
        if ((Has-Field -Fields $Fields -Name "pci-vmd") -and ((Get-FieldValue -Fields $Fields -Name "pci-vmd") -ne 0)) {
            if ((Has-Field -Fields $Fields -Name "vmd-pci") -and ((Get-FieldValue -Fields $Fields -Name "vmd-pci" -Default $InvalidU32) -eq $InvalidU32)) {
                return New-Classification -Stage "pci-vmd-bdf" -Detail "A VMD-class candidate exists, but its BDF was not exported."
            }
            if ((Has-Field -Fields $Fields -Name "vmd-vendor-device") -and ((Get-FieldValue -Fields $Fields -Name "vmd-vendor-device") -eq 0)) {
                return New-Classification -Stage "pci-vmd-identity" -Detail "A VMD-class candidate exists, but its vendor/device ID was not exported."
            }
            if ((Has-Field -Fields $Fields -Name "vmd-class") -and (((Get-FieldValue -Fields $Fields -Name "vmd-class") -band 0xFFFF0000) -ne 0x08800000)) {
                return New-Classification -Stage "pci-vmd-class-code" -Detail ("The VMD-class candidate reported unexpected class telemetry 0x{0:X8}." -f (Get-FieldValue -Fields $Fields -Name "vmd-class"))
            }
            if ((Has-Field -Fields $Fields -Name "vmd-bar0") -and (((Get-FieldValue -Fields $Fields -Name "vmd-bar0") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "vmd-bar0") -eq $InvalidU32))) {
                return New-Classification -Stage "pci-vmd-bar0" -Detail "The VMD-class candidate did not expose a usable BAR0."
            }
            if ((Has-Field -Fields $Fields -Name "vmd-mmio-low") -and (((Get-FieldValue -Fields $Fields -Name "vmd-mmio-low") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "vmd-mmio-low") -eq $InvalidU32))) {
                return New-Classification -Stage "pci-vmd-mmio-base" -Detail "The VMD-class candidate did not expose a usable MMIO base."
            }
            if ((Has-Field -Fields $Fields -Name "vmd-mmio-span") -and ((Get-FieldValue -Fields $Fields -Name "vmd-mmio-span") -eq 0)) {
                return New-Classification -Stage "pci-vmd-mmio-span" -Detail "The VMD-class candidate MMIO span hint was zero."
            }
            if ((Has-Field -Fields $Fields -Name "vmd-mmio-flags") -and (((Get-FieldValue -Fields $Fields -Name "vmd-mmio-flags") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "vmd-mmio-flags") -eq $InvalidU32))) {
                return New-Classification -Stage "pci-vmd-mmio-flags" -Detail "The VMD-class candidate MMIO flags were missing or invalid."
            }
            if (Has-Field -Fields $Fields -Name "vmd-nested-plan") {
                if ((Get-FieldValue -Fields $Fields -Name "vmd-nested-plan") -ne 1) {
                    return New-Classification -Stage "pci-vmd-nested-plan" -Detail "VMD MMIO preflight succeeded enough to identify the candidate, but no nested-enumeration plan was exported."
                }
                if ((Get-FieldValue -Fields $Fields -Name "vmd-nested-enum") -ne 1) {
                    return New-Classification -Stage "pci-vmd-nested-enumeration" -Detail "VMD MMIO preflight is available, but the nested PCI domain has not been enumerated."
                }
                if ((Get-FieldValue -Fields $Fields -Name "vmd-nested-nvme") -eq 0) {
                    return New-Classification -Stage "pci-vmd-nested-nvme-class" -Detail "The VMD nested PCI domain was enumerated, but no child NVMe controller was reported."
                }
                if ((Has-Field -Fields $Fields -Name "vmd-nested-mmio-low") -and (((Get-FieldValue -Fields $Fields -Name "vmd-nested-mmio-low") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "vmd-nested-mmio-low") -eq $InvalidU32))) {
                    return New-Classification -Stage "pci-vmd-nested-nvme-mmio-base" -Detail "A child NVMe controller was reported behind VMD, but its BAR did not yield a usable MMIO base."
                }
                if ((Has-Field -Fields $Fields -Name "vmd-nested-mmio-span") -and ((Get-FieldValue -Fields $Fields -Name "vmd-nested-mmio-span") -eq 0)) {
                    return New-Classification -Stage "pci-vmd-nested-nvme-mmio-span" -Detail "A child NVMe controller was reported behind VMD, but its MMIO span hint was zero."
                }
                if ((Has-Field -Fields $Fields -Name "vmd-nested-mmio-flags") -and (((Get-FieldValue -Fields $Fields -Name "vmd-nested-mmio-flags") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "vmd-nested-mmio-flags") -eq $InvalidU32))) {
                    return New-Classification -Stage "pci-vmd-nested-nvme-mmio-flags" -Detail "A child NVMe controller was reported behind VMD, but its MMIO flags were missing or invalid."
                }
                if ((Has-Field -Fields $Fields -Name "vmd-nested-bind-ready") -and ((Get-FieldValue -Fields $Fields -Name "vmd-nested-bind-ready") -ne 1)) {
                    return New-Classification -Stage "pci-vmd-nested-nvme-bind-ready" -Detail "A child NVMe controller was reported behind VMD with MMIO preflight, but bind readiness was not proven."
                }
                if ((Has-Field -Fields $Fields -Name "vmd-nested-register-candidate") -and ((Get-FieldValue -Fields $Fields -Name "vmd-nested-register-candidate") -ne 1)) {
                    return New-Classification -Stage "pci-vmd-nested-nvme-register-candidate" -Detail "A child NVMe controller was reported behind VMD with bind readiness, but no registration candidate was exported."
                }
                if ((Has-Field -Fields $Fields -Name "vmd-nested-register-status") -and ((Get-FieldValue -Fields $Fields -Name "vmd-nested-register-status") -eq 2)) {
                    $vmdBindSucceeded = ((Has-Field -Fields $Fields -Name "vmd-nvme-bind-state") -and ((Get-FieldValue -Fields $Fields -Name "vmd-nvme-bind-state") -eq 2))
                    if ((-not $vmdBindSucceeded) -and (Has-Field -Fields $Fields -Name "nvme-candidate-source") -and ((Get-FieldValue -Fields $Fields -Name "nvme-candidate-source") -ne 2)) {
                        return New-Classification -Stage "pci-vmd-nested-nvme-mmio-source" -Detail "A VMD child NVMe registration candidate was deferred, but the MMIO/NVMe layer did not record it as the deferred VMD source."
                    }
                    if ((-not $vmdBindSucceeded) -and (Has-Field -Fields $Fields -Name "nvme-candidate-deferred") -and ((Get-FieldValue -Fields $Fields -Name "nvme-candidate-deferred") -ne 1)) {
                        return New-Classification -Stage "pci-vmd-nested-nvme-mmio-deferred" -Detail "A VMD child NVMe registration candidate was deferred, but the MMIO/NVMe layer did not mark the source as deferred."
                    }
                    if (Has-Field -Fields $Fields -Name "vmd-nested-driver-plan-state") {
                        if ((Get-FieldValue -Fields $Fields -Name "vmd-nested-driver-plan-state") -ne 2) {
                            return New-Classification -Stage "pci-vmd-nested-driver-plan" -Detail "A VMD child NVMe registration candidate was deferred, but the no-touch VMD-backed driver plan was not staged."
                        }
                        if ((Has-Field -Fields $Fields -Name "vmd-nested-driver-plan-flags") -and (((Get-FieldValue -Fields $Fields -Name "vmd-nested-driver-plan-flags") -band 0xFF) -ne 0xFF)) {
                            return New-Classification -Stage "pci-vmd-nested-driver-plan" -Detail "A VMD child NVMe no-touch driver plan exists, but its safety/readiness flags are incomplete."
                        }
                        if ((Has-Field -Fields $Fields -Name "vmd-nested-driver-plan-token") -and (((Get-FieldValue -Fields $Fields -Name "vmd-nested-driver-plan-token") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "vmd-nested-driver-plan-token") -eq $InvalidU32))) {
                            return New-Classification -Stage "pci-vmd-nested-driver-plan" -Detail "A VMD child NVMe no-touch driver plan exists, but its handoff token is invalid."
                        }
                        if ($vmdBindSucceeded) {
                            if ((Has-Field -Fields $Fields -Name "nvme-candidate-source") -and ((Get-FieldValue -Fields $Fields -Name "nvme-candidate-source") -ne 3)) {
                                return New-Classification -Stage "pci-vmd-nested-nvme-bind" -Detail "A VMD child NVMe bind reported success, but the MMIO/NVMe candidate source was not promoted to the bound VMD source."
                            }
                            if ((Get-FieldValue -Fields $Fields -Name "nvme-found") -ne 1) {
                                return New-Classification -Stage "nvme-controller-discovery" -Detail "A VMD child NVMe candidate was bound, but the regular NVMe probe did not discover a controller."
                            }
                            if ((Get-FieldValue -Fields $Fields -Name "nvme-ready") -ne 1) {
                                return New-Classification -Stage "nvme-controller-ready" -Detail "A VMD child NVMe candidate was bound and discovered, but the controller did not become ready."
                            }
                            if ((Get-FieldValue -Fields $Fields -Name "nvme-identify") -ne 1) {
                                return New-Classification -Stage "nvme-identify" -Detail "A VMD child NVMe candidate was bound, but Identify did not complete."
                            }
                        }
                        return New-Classification -Stage "pci-vmd-nested-driver-plan-staged" -Detail "A child NVMe controller behind VMD has a capability-gated no-touch driver plan staged; actual VMD-backed NVMe binding remains the next implementation target."
                    }
                    return New-Classification -Stage "pci-vmd-nested-nvme-register-deferred" -Detail "A child NVMe controller behind VMD is a registration candidate, but the VMD-backed NVMe driver handoff is intentionally deferred."
                }
                return New-Classification -Stage "pci-vmd-nested-nvme-bind" -Detail "A child NVMe controller was reported behind VMD, but the regular NVMe driver has not been bound through the nested path."
            }
            return New-Classification -Stage "pci-nvme-hidden-by-vmd" -Detail "PCI storage exists, but direct NVMe is absent and an Intel VMD-class candidate is present."
        }
        if ((Has-Field -Fields $Fields -Name "pci-intel-system") -and ((Get-FieldValue -Fields $Fields -Name "pci-intel-system") -ne 0)) {
            return New-Classification -Stage "pci-nvme-hidden-by-intel-system" -Detail "PCI storage exists, but direct NVMe is absent and Intel system-class controller candidates are present."
        }
        if ((Has-Field -Fields $Fields -Name "pci-other-storage") -and ((Get-FieldValue -Fields $Fields -Name "pci-other-storage") -ne 0)) {
            return New-Classification -Stage "pci-nvme-other-storage" -Detail "PCI storage exists, but direct NVMe is absent and only non-AHCI/non-NVMe storage-class controllers were exported."
        }
        return New-Classification -Stage "pci-nvme-class" -Detail "PCI storage controllers were discovered, but none matched the NVMe class/prog-if."
    }
    if ((Has-Field -Fields $Fields -Name "nvme-pci") -and ((Get-FieldValue -Fields $Fields -Name "nvme-pci" -Default $InvalidU32) -eq $InvalidU32)) {
        return New-Classification -Stage "pci-nvme-bdf" -Detail "NVMe class telemetry exists, but the first NVMe BDF was not exported."
    }
    if ((Has-Field -Fields $Fields -Name "nvme-vendor-device") -and ((Get-FieldValue -Fields $Fields -Name "nvme-vendor-device") -eq 0)) {
        return New-Classification -Stage "pci-nvme-identity" -Detail "The first NVMe controller did not expose a nonzero vendor/device ID."
    }
    if ((Has-Field -Fields $Fields -Name "nvme-class") -and (((Get-FieldValue -Fields $Fields -Name "nvme-class") -band 0xFFFF0000) -ne 0x01080000)) {
        return New-Classification -Stage "pci-nvme-class-code" -Detail ("The first NVMe controller reported unexpected class telemetry 0x{0:X8}." -f (Get-FieldValue -Fields $Fields -Name "nvme-class"))
    }
    if ((Has-Field -Fields $Fields -Name "nvme-bar0") -and (((Get-FieldValue -Fields $Fields -Name "nvme-bar0") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "nvme-bar0") -eq $InvalidU32))) {
        return New-Classification -Stage "pci-nvme-bar0" -Detail "The first NVMe controller did not expose a usable BAR0."
    }
    if ((Has-Field -Fields $Fields -Name "nvme-mmio-low") -and (((Get-FieldValue -Fields $Fields -Name "nvme-mmio-low") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "nvme-mmio-low") -eq $InvalidU32))) {
        return New-Classification -Stage "pci-nvme-mmio-base" -Detail "The first NVMe controller did not expose a usable MMIO base."
    }
    if ((Has-Field -Fields $Fields -Name "nvme-mmio-span") -and ((Get-FieldValue -Fields $Fields -Name "nvme-mmio-span") -eq 0)) {
        return New-Classification -Stage "pci-nvme-mmio-span" -Detail "The first NVMe controller MMIO span was zero."
    }
    if ((Has-Field -Fields $Fields -Name "nvme-mmio-flags") -and (((Get-FieldValue -Fields $Fields -Name "nvme-mmio-flags") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "nvme-mmio-flags") -eq $InvalidU32))) {
        return New-Classification -Stage "pci-nvme-mmio-flags" -Detail "The first NVMe controller MMIO flags were missing or invalid."
    }
    if ((Get-FieldValue -Fields $Fields -Name "nvme-found") -ne 1) {
        return New-Classification -Stage "nvme-controller-discovery" -Detail "NVMe controller was not discovered."
    }
    if ((Get-FieldValue -Fields $Fields -Name "nvme-ready") -ne 1) {
        return New-Classification -Stage "nvme-controller-ready" -Detail "NVMe controller was discovered but did not become ready."
    }
    if ((Get-FieldValue -Fields $Fields -Name "nvme-identify") -ne 1) {
        return New-Classification -Stage "nvme-identify" -Detail "NVMe Identify did not complete."
    }
    if ((Get-FieldValue -Fields $Fields -Name "ioq") -ne 1) {
        return New-Classification -Stage "nvme-io-queue" -Detail "NVMe IO queue creation failed."
    }
    if ((Get-FieldValue -Fields $Fields -Name "read-issued") -ne 1) {
        return New-Classification -Stage "nvme-read-issue" -Detail "The first NVMe read was not issued."
    }
    if ((Get-FieldValue -Fields $Fields -Name "read-completed") -ne 1) {
        return New-Classification -Stage "nvme-read-completion" -Detail "The first NVMe read did not complete."
    }
    if ((Get-FieldValue -Fields $Fields -Name "read-status") -ne 0) {
        return New-Classification -Stage "nvme-read-status" -Detail ("NVMe read completed with status {0}." -f (Get-FieldValue -Fields $Fields -Name "read-status"))
    }
    if ((Get-FieldValue -Fields $Fields -Name "gpt-signature") -ne 1) {
        return New-Classification -Stage "gpt-signature" -Detail "GPT signature was not found."
    }
    if ((Get-FieldValue -Fields $Fields -Name "gpt-partitions") -eq 0) {
        return New-Classification -Stage "gpt-partition-table" -Detail "GPT was found but no partitions were enumerated."
    }
    if (((Get-FieldValue -Fields $Fields -Name "fat32-start") -eq 0) -or ((Get-FieldValue -Fields $Fields -Name "fat32-sectors") -eq 0)) {
        return New-Classification -Stage "fat32-partition" -Detail "No candidate FAT32 partition geometry was recorded."
    }
    if ((Get-FieldValue -Fields $Fields -Name "gpt-vbr") -ne 1) {
        return New-Classification -Stage "fat32-vbr" -Detail "The FAT32 partition VBR was not accepted."
    }
    if ((Get-FieldValue -Fields $Fields -Name "fat-bpb") -ne 1) {
        return New-Classification -Stage "fat32-bpb" -Detail "The FAT32 BPB was not accepted."
    }
    if ((Get-FieldValue -Fields $Fields -Name "fat-located") -ne 1) {
        return New-Classification -Stage "fat32-mount" -Detail "FAT32 was not located after GPT/VBR/BPB probing."
    }
    if ((Get-FieldValue -Fields $Fields -Name "fat-unavailable") -ne 0) {
        return New-Classification -Stage "fat32-unavailable" -Detail "The kernel marked the FAT source unavailable."
    }
    if ((Get-FieldValue -Fields $Fields -Name "fat-error") -ne 0) {
        return New-Classification -Stage "fat32-error" -Detail ("FAT probing recorded error {0}." -f (Get-FieldValue -Fields $Fields -Name "fat-error"))
    }
    if ((Get-FieldValue -Fields $Fields -Name "rw-cap") -ne 1) {
        return New-Classification -Stage "storage-capability" -Detail "The shell did not hold a scoped NVMe read/write capability."
    }
    if ((Get-FieldValue -Fields $Fields -Name "rw-delegated") -ne 1) {
        return New-Classification -Stage "storage-capability-delegation" -Detail "The NVMe read/write capability was not delegated."
    }
    if ((Get-FieldValue -Fields $Fields -Name "rw-error") -ne 0) {
        return New-Classification -Stage "storage-capability-error" -Detail ("NVMe capability setup recorded error {0}." -f (Get-FieldValue -Fields $Fields -Name "rw-error"))
    }
    if ((Get-FieldValue -Fields $Fields -Name "apps-stat") -ne 1) {
        return New-Classification -Stage "apps-directory-stat" -Detail "/APPS could not be statted through NVMe FAT."
    }
    if ((Get-FieldValue -Fields $Fields -Name "apps-type") -ne 2) {
        return New-Classification -Stage "apps-directory-type" -Detail "/APPS exists but is not reported as a directory."
    }
    if ((Get-FieldValue -Fields $Fields -Name "apps-dirent") -ne 1) {
        return New-Classification -Stage "apps-directory-read" -Detail "The first /APPS directory entry could not be read."
    }

    if ($RequireStage) {
        if ((Get-FieldValue -Fields $Fields -Name "boot-staged") -ne 1) {
            return New-Classification -Stage "boot-media-staging" -Detail "UEFI boot-media dynamic artifacts were not staged into boot_info."
        }
        if ((Get-FieldValue -Fields $Fields -Name "boot-app-bytes") -ne $ExpectedAppBytes) {
            return New-Classification -Stage "boot-media-app-size" -Detail ("Expected boot app {0} bytes, observed {1}." -f $ExpectedAppBytes, (Get-FieldValue -Fields $Fields -Name "boot-app-bytes"))
        }
        if ((Get-FieldValue -Fields $Fields -Name "boot-interp-bytes") -ne $ExpectedInterpBytes) {
            return New-Classification -Stage "boot-media-interp-size" -Detail ("Expected boot interpreter {0} bytes, observed {1}." -f $ExpectedInterpBytes, (Get-FieldValue -Fields $Fields -Name "boot-interp-bytes"))
        }
        if ((Get-FieldValue -Fields $Fields -Name "dynldlimit-stat") -ne 1) {
            return New-Classification -Stage "nvme-dynldlimit-stat" -Detail "/APPS/DYNLDLIMIT was not visible through NVMe FAT."
        }
        if ((Get-FieldValue -Fields $Fields -Name "dynldlimit-bytes") -ne $ExpectedAppBytes) {
            return New-Classification -Stage "nvme-dynldlimit-size" -Detail ("Expected /APPS/DYNLDLIMIT {0} bytes, observed {1}." -f $ExpectedAppBytes, (Get-FieldValue -Fields $Fields -Name "dynldlimit-bytes"))
        }
        if ((Get-FieldValue -Fields $Fields -Name "ldlimit-stat") -ne 1) {
            return New-Classification -Stage "nvme-ldlimit-stat" -Detail "/APPS/LDLIMIT was not visible through NVMe FAT."
        }
        if ((Get-FieldValue -Fields $Fields -Name "ldlimit-bytes") -ne $ExpectedInterpBytes) {
            return New-Classification -Stage "nvme-ldlimit-size" -Detail ("Expected /APPS/LDLIMIT {0} bytes, observed {1}." -f $ExpectedInterpBytes, (Get-FieldValue -Fields $Fields -Name "ldlimit-bytes"))
        }
        if ((Get-FieldValue -Fields $Fields -Name "stage-expected") -ne 1) {
            return New-Classification -Stage "stage-expected-flag" -Detail "Kernel did not mark staged dynamic artifacts as expected."
        }
        if ((Get-FieldValue -Fields $Fields -Name "dynldlimit-match") -ne 1) {
            return New-Classification -Stage "dynldlimit-match" -Detail "Boot-media DYNLDLIMIT bytes do not match NVMe /APPS/DYNLDLIMIT bytes."
        }
        if ((Get-FieldValue -Fields $Fields -Name "ldlimit-match") -ne 1) {
            return New-Classification -Stage "ldlimit-match" -Detail "Boot-media LDLIMIT bytes do not match NVMe /APPS/LDLIMIT bytes."
        }
        if ((Get-FieldValue -Fields $Fields -Name "stage-match") -ne 1) {
            return New-Classification -Stage "stage-match" -Detail "Overall staged artifact match failed."
        }
    }

    return New-Classification -Stage "storage-ready" -Detail "NVMe, GPT, FAT, /APPS, capability delegation, and requested staged artifacts are all visible." -Pass $true
}

if (-not (Test-Path $InputPath)) {
    throw "Hardware storage capture parser: input file not found: $InputPath"
}

$lines = @(Get-Content $InputPath)
$triageLine = @($lines | Where-Object { $_ -match 'drs-nvme-triage' } | Select-Object -Last 1)
$vmdBindLine = @($lines | Where-Object { $_ -match 'drs-vmd-nvme-bind' } | Select-Object -Last 1)
$legacyUnavailableLine = @($lines | Where-Object { $_ -match 'drs-realbin-unavailable' } | Select-Object -Last 1)

if ($triageLine.Count -eq 0) {
    $classification = if ($legacyUnavailableLine.Count -ne 0) {
        New-Classification -Stage "legacy-realbin-unavailable" -Detail "Only legacy drs-realbin-unavailable telemetry was found. Boot the M111-staged image and run hwval to capture drs-nvme-triage."
    }
    else {
        New-Classification -Stage "missing-storage-triage" -Detail "No drs-nvme-triage line was found in the capture."
    }
    $result = [PSCustomObject]@{
        tool = "parse-hardware-storage-capture"
        input = (Resolve-Path $InputPath).Path
        telemetry_found = 0
        require_staged_dynamic_artifacts = [bool]$RequireStagedDynamicArtifacts
        classification = $classification
        raw_line = ""
        legacy_line = if ($legacyUnavailableLine.Count -ne 0) { [string]$legacyUnavailableLine[0] } else { "" }
        fields = [PSCustomObject]@{}
    }
}
else {
    $fields = Parse-TelemetryFields -Line $triageLine[0]
    if ($vmdBindLine.Count -ne 0) {
        $bindFields = Parse-TelemetryFields -Line $vmdBindLine[0]
        foreach ($key in $bindFields.Keys) {
            if (($key -eq "candidate-source") -or ($key -eq "candidate-deferred") -or ($key -eq "candidate-bdf") -or ($key -eq "candidate-token")) {
                $fields["nvme-$key"] = $bindFields[$key]
            }
            else {
                $fields["vmd-nvme-bind-$key"] = $bindFields[$key]
            }
        }
    }
    $classification = Classify-StorageCapture `
        -Fields $fields `
        -RequireStage ([bool]$RequireStagedDynamicArtifacts) `
        -ExpectedAppBytes $ExpectedDynamicAppBytes `
        -ExpectedInterpBytes $ExpectedDynamicInterpBytes
    $result = [PSCustomObject]@{
        tool = "parse-hardware-storage-capture"
        input = (Resolve-Path $InputPath).Path
        telemetry_found = 1
        require_staged_dynamic_artifacts = [bool]$RequireStagedDynamicArtifacts
        expected_dynamic_app_bytes = $ExpectedDynamicAppBytes
        expected_dynamic_interp_bytes = $ExpectedDynamicInterpBytes
        classification = $classification
        raw_line = [string]$triageLine[0]
        legacy_line = if ($legacyUnavailableLine.Count -ne 0) { [string]$legacyUnavailableLine[0] } else { "" }
        fields = [PSCustomObject]$fields
    }
}

$outputDir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}
$result | ConvertTo-Json -Depth 6 | Set-Content -Path $OutputPath -Encoding Ascii

Write-Host "hardware-storage-capture: $($result.classification.stage)"
Write-Host "  pass: $($result.classification.pass)"
Write-Host "  detail: $($result.classification.detail)"
Write-Host "  output: $OutputPath"

if (-not $result.classification.pass) {
    exit 2
}
