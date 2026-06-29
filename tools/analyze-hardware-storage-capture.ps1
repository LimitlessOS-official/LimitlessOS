param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [string]$OutputDir = "",

    [string]$EvidenceManifestPath = "",

    [switch]$RequireStagedDynamicArtifacts,

    [uint32]$ExpectedDynamicAppBytes = 15680,

    [uint32]$ExpectedDynamicInterpBytes = 16704
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $root "dist\m114-hardware-storage-analysis-$stamp"
}

function Assert-FileExists
{
    param(
        [string]$Path,
        [string]$Message
    )

    if (-not (Test-Path $Path)) {
        throw $Message
    }
}

function Get-Field
{
    param(
        [object]$Fields,
        [string]$Name,
        [string]$Default = "0"
    )

    if ($null -eq $Fields) {
        return $Default
    }
    $property = $Fields.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $Default
    }
    return [string]$property.Value
}

function Get-NextTarget
{
    param([string]$Stage)

    switch ($Stage) {
        "storage-ready" { return "Storage is healthy. Next target: run linux /APPS/DYNLDLIMIT on hardware and capture drs-realbin telemetry." }
        "legacy-realbin-unavailable" { return "Capture is from an older image. Next target: boot the M113 staged ISO and run hwval before linux /APPS/DYNLDLIMIT." }
        "missing-storage-triage" { return "No storage triage line was captured. Next target: run hwval on an M110-or-newer Product UEFI image and capture the full output." }
        "pci-storage-discovery" { return "Driver target: PCI enumeration did not expose any storage-class controller; inspect ECAM/legacy config access and firmware storage mode." }
        "pci-nvme-hidden-by-raid" { return "Driver target: direct NVMe is hidden behind a RAID/RST-class storage controller; inspect firmware storage mode and plan a scoped RST/VMD path before NVMe probing." }
        "pci-vmd-bdf" { return "Driver target: VMD candidate count is nonzero but first BDF export failed; inspect VMD candidate cache population and capability owner path." }
        "pci-vmd-identity" { return "Driver target: VMD candidate BDF exists but vendor/device is zero; inspect PCI config reads for the selected VMD candidate." }
        "pci-vmd-class-code" { return "Driver target: VMD candidate class telemetry is inconsistent; inspect system-class/subclass decoding." }
        "pci-vmd-bar0" { return "Driver target: VMD candidate BAR0 is missing or invalid; inspect BAR reading and memory BAR filtering." }
        "pci-vmd-mmio-base" { return "Driver target: VMD BAR exists but MMIO base is invalid; inspect BAR mask, 64-bit BAR pairing, and mapped base selection." }
        "pci-vmd-mmio-span" { return "Driver target: VMD MMIO span is zero; inspect conservative VMD span planning." }
        "pci-vmd-mmio-flags" { return "Driver target: VMD MMIO flags are invalid; inspect VMD MMIO preflight planning and token export." }
        "pci-vmd-nested-plan" { return "Driver target: VMD MMIO preflight exists but no nested-domain plan was exported; inspect VMD nested enumeration telemetry setup." }
        "pci-vmd-nested-enumeration" { return "Driver target: VMD MMIO preflight is valid; implement read-only nested PCI-domain enumeration before binding child NVMe." }
        "pci-vmd-nested-nvme-class" { return "Driver target: VMD nested domain enumeration ran but no child NVMe class device was reported; inspect nested class-code scanning." }
        "pci-vmd-nested-nvme-bind" { return "Driver target: child NVMe exists behind VMD; bind the regular NVMe driver through the nested VMD path." }
        "pci-nvme-hidden-by-vmd" { return "Driver target: direct NVMe is hidden behind an Intel VMD-class candidate; inspect nested PCI domain enumeration before regular NVMe probing." }
        "pci-nvme-hidden-by-intel-system" { return "Driver target: direct NVMe is absent while Intel system-class controller candidates are present; inspect VMD-style controller exposure and nested PCI domain handling." }
        "pci-nvme-other-storage" { return "Driver target: non-AHCI/non-NVMe storage-class hardware is present; inspect the exported class code before choosing AHCI, RAID/RST, or another storage driver path." }
        "pci-nvme-class" { return "Driver target: PCI storage exists but no NVMe class device was found; inspect class-code matching, VMD/RAID mode, and controller hiding." }
        "pci-nvme-bdf" { return "Driver target: NVMe class count is nonzero but first BDF export failed; inspect first-NVMe cache population and capability owner path." }
        "pci-nvme-identity" { return "Driver target: first NVMe BDF exists but vendor/device is zero; inspect PCI config reads for the selected BDF." }
        "pci-nvme-class-code" { return "Driver target: first NVMe class telemetry is inconsistent; inspect class/subclass/prog-if decoding." }
        "pci-nvme-bar0" { return "Driver target: first NVMe BAR0 is missing or invalid; inspect PCI BAR sizing/reading and memory BAR filtering." }
        "pci-nvme-mmio-base" { return "Driver target: NVMe BAR exists but MMIO base is invalid; inspect BAR mask, 64-bit BAR pairing, and mapped base selection." }
        "pci-nvme-mmio-span" { return "Driver target: NVMe MMIO span is zero; inspect BAR size probing or conservative span fallback." }
        "pci-nvme-mmio-flags" { return "Driver target: NVMe MMIO flags are invalid; inspect MMIO candidate planning and capability-token export." }
        "nvme-controller-discovery" { return "Driver target: PCI/NVMe enumeration, class-code match, BAR mapping, and controller register visibility." }
        "nvme-controller-ready" { return "Driver target: NVMe reset/enable sequence and CSTS.RDY timeout handling on the physical controller." }
        "nvme-identify" { return "Driver target: admin queue setup, Identify command submission, PRP buffer mapping, and completion status." }
        "nvme-io-queue" { return "Driver target: IO submission/completion queue creation and queue doorbell programming." }
        "nvme-read-issue" { return "Driver target: first namespace read command construction before submission." }
        "nvme-read-completion" { return "Driver target: completion polling/MSI path for the first namespace read." }
        "nvme-read-status" { return "Driver target: decode NVMe read completion status and namespace/LBA/PRP assumptions." }
        "gpt-signature" { return "Storage target: first sector content or GPT probing; verify the image written to USB/NVMe and LBA reads." }
        "gpt-partition-table" { return "Storage target: GPT partition entry scan and partition type filtering." }
        "fat32-partition" { return "Storage target: locate the FAT32 partition geometry from GPT entries." }
        "fat32-vbr" { return "Filesystem target: FAT32 VBR read and signature/sector-size validation." }
        "fat32-bpb" { return "Filesystem target: FAT32 BPB interpretation, cluster size, FAT/root-cluster fields." }
        "fat32-mount" { return "Filesystem target: FAT mount construction after VBR/BPB acceptance." }
        "fat32-unavailable" { return "Filesystem target: source-availability propagation from FAT mount to shell/Linux launcher." }
        "fat32-error" { return "Filesystem target: decode fat-error and inspect the exact FAT parser rejection." }
        "storage-capability" { return "Authority target: shell must hold scoped NVMe read/write capability before hwval and linux commands." }
        "storage-capability-delegation" { return "Authority target: capability delegation from shell to NVMe FAT reader." }
        "storage-capability-error" { return "Authority target: capability setup error path and owner/token mismatch." }
        "apps-directory-stat" { return "VFS target: /APPS lookup through FAT directory traversal." }
        "apps-directory-type" { return "VFS target: FAT directory entry attribute/type translation for /APPS." }
        "apps-directory-read" { return "VFS target: FAT directory iterator and first dirent read." }
        "boot-media-staging" { return "Boot target: UEFI loader did not stage dynamic artifacts into boot_info; verify the ISO was built with BootLinuxApp/Interp paths." }
        "boot-media-app-size" { return "Boot target: staged DYNLDLIMIT byte count differs from manifest; rebuild evidence bundle from current artifacts." }
        "boot-media-interp-size" { return "Boot target: staged LDLIMIT byte count differs from manifest; rebuild evidence bundle from current artifacts." }
        "nvme-dynldlimit-stat" { return "Staging target: /APPS/DYNLDLIMIT is missing from the NVMe FAT image visible to the kernel." }
        "nvme-dynldlimit-size" { return "Staging target: /APPS/DYNLDLIMIT size mismatch between expected artifact and NVMe FAT." }
        "nvme-ldlimit-stat" { return "Staging target: /APPS/LDLIMIT is missing from the NVMe FAT image visible to the kernel." }
        "nvme-ldlimit-size" { return "Staging target: /APPS/LDLIMIT size mismatch between expected artifact and NVMe FAT." }
        "stage-expected-flag" { return "Telemetry target: kernel did not mark staged artifacts as expected despite manifest inputs." }
        "dynldlimit-match" { return "Staging target: DYNLDLIMIT boot-media and NVMe byte counts disagree." }
        "ldlimit-match" { return "Staging target: LDLIMIT boot-media and NVMe byte counts disagree." }
        "stage-match" { return "Staging target: overall boot-media/NVMe artifact agreement failed." }
        default { return "Unknown stage. Next target: inspect the JSON fields and raw telemetry line." }
    }
}

function New-DiagnosticPlan
{
    param(
        [string]$Stage,
        [object]$Parsed
    )

    $commonKernelFiles = @(
        "kernel/arch/x86_64/scaffold_storage.c",
        "kernel/arch/x86_64/mmio.c",
        "kernel/include/mmio_x64.h"
    )

    switch ($Stage) {
        "storage-ready" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "storage-ready"
                required_fields = @("nvme-found", "nvme-ready", "nvme-identify", "ioq", "read-status", "gpt-signature", "fat-located", "apps-stat", "stage-match")
                first_check = "No storage fix is indicated by this transcript. Move to the next classifier-reported hardware or runtime target."
                kernel_files = @()
                acceptance_signal = "A real hardware transcript keeps storage-ready with stage-match 1 and linux /APPS/DYNLDLIMIT reports source 2."
            }
        }
        "legacy-realbin-unavailable" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "capture-version"
                required_fields = @("drs-nvme-triage", "drs-realbin")
                first_check = "Repeat the capture with the current staged handoff image; this transcript predates the storage triage line."
                kernel_files = @()
                acceptance_signal = "hwval includes one drs-nvme-triage line before linux /APPS/DYNLDLIMIT is run."
            }
        }
        "missing-storage-triage" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "capture-procedure"
                required_fields = @("drs-nvme-triage")
                first_check = "Run hwval on the physical machine and capture the full terminal output before running the Linux command."
                kernel_files = @("kernel/arch/x86_64/scaffold_platform.c", "kernel/arch/x86_64/scaffold_storage.c")
                acceptance_signal = "The capture contains a parseable drs-nvme-triage line."
            }
        }
        "pci-storage-discovery" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-storage-enumeration"
                required_fields = @("pci-storage", "pci-nvme", "nvme-found")
                first_check = "Inspect ECAM/MCFG discovery, fallback PCI config access, and whether firmware exposes the internal storage controller as PCI storage-class hardware."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/scaffold_storage.c")
                acceptance_signal = "pci-storage becomes nonzero in hwval and drs-nvme-triage."
            }
        }
        "pci-nvme-class" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-class-match"
                required_fields = @("pci-storage", "pci-nvme", "pci-raid", "pci-other-storage", "pci-intel-system", "pci-vmd", "nvme-class", "nvme-found")
                first_check = "Inspect class/subclass/prog-if matching and firmware storage mode; Intel VMD or RAID mode may hide the NVMe controller behind a different PCI device."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/scaffold_storage.c")
                acceptance_signal = "pci-nvme becomes nonzero and nvme-class has class 0x0108."
            }
        }
        "pci-nvme-hidden-by-raid" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-raid-rst-class"
                required_fields = @("pci-storage", "pci-nvme", "pci-raid", "pci-other-storage", "other-storage-pci", "other-storage-vendor-device", "other-storage-class")
                first_check = "Confirm whether firmware storage mode is RAID/RST and whether the controller exposes direct NVMe children only behind a vendor storage bridge."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "Either firmware exposes a direct pci-nvme controller, or a bounded RAID/RST/VMD bridge path exports an NVMe namespace without unsafe writes."
            }
        }
        "pci-nvme-hidden-by-vmd" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-intel-vmd-class"
                required_fields = @("pci-storage", "pci-nvme", "pci-vmd", "vmd-pci", "vmd-vendor-device", "vmd-class", "vmd-bar0", "vmd-bar1", "vmd-mmio-low", "vmd-mmio-high", "vmd-mmio-span", "vmd-mmio-flags", "vmd-mmio-token")
                first_check = "Inspect the VMD candidate MMIO preflight fields, then implement read-only nested PCI-domain enumeration before attempting to bind the existing NVMe driver below it."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "The physical transcript reports direct child NVMe identity behind the VMD candidate, or a precise nested-enumeration failure stage after VMD MMIO preflight succeeds."
            }
        }
        "pci-vmd-bdf" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-bdf-export"
                required_fields = @("pci-vmd", "vmd-pci")
                first_check = "Inspect first-VMD candidate cache population and the capability owner used by pci64_first_vmd_candidate_address."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/shell.c")
                acceptance_signal = "vmd-pci is not 0xFFFFFFFF when pci-vmd is nonzero."
            }
        }
        "pci-vmd-identity" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-identity-read"
                required_fields = @("vmd-pci", "vmd-vendor-device")
                first_check = "Inspect PCI config vendor/device reads for the selected VMD candidate and reject all-zero identity before planning MMIO."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "vmd-vendor-device is nonzero for the selected VMD candidate."
            }
        }
        "pci-vmd-class-code" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-class-code"
                required_fields = @("vmd-pci", "vmd-class")
                first_check = "Inspect VMD candidate class code packing and ensure base class 0x08 and subclass 0x80 are preserved in vmd-class."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "vmd-class matches 0x0880xxxx for the selected VMD candidate."
            }
        }
        "pci-vmd-bar0" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-bar0"
                required_fields = @("vmd-pci", "vmd-bar0")
                first_check = "Inspect PCI BAR0 reads, memory-vs-IO BAR filtering, all-ones rejection, and 64-bit BAR pairing for the VMD candidate."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "vmd-bar0 is neither zero nor 0xFFFFFFFF."
            }
        }
        "pci-vmd-mmio-base" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-mmio-base"
                required_fields = @("vmd-bar0", "vmd-bar1", "vmd-mmio-low", "vmd-mmio-high")
                first_check = "Inspect VMD BAR masking, low/high 64-bit base construction, and rejection of zero/all-ones MMIO base values."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/paging.c")
                acceptance_signal = "vmd-mmio-low is a usable nonzero/non-sentinel MMIO base."
            }
        }
        "pci-vmd-mmio-span" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-mmio-span"
                required_fields = @("vmd-mmio-low", "vmd-mmio-span")
                first_check = "Inspect the conservative VMD span hint used before any nested-domain mapping or controller register access is attempted."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "vmd-mmio-span is nonzero."
            }
        }
        "pci-vmd-mmio-flags" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-mmio-flags"
                required_fields = @("vmd-mmio-low", "vmd-mmio-flags", "vmd-mmio-token")
                first_check = "Inspect VMD MMIO preflight flags, no-touch safety flags, nested-enumeration-required flags, and token generation."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/arch/x86_64/scaffold_storage.c", "kernel/include/pci_x64.h")
                acceptance_signal = "vmd-mmio-flags and vmd-mmio-token are nonzero and non-sentinel."
            }
        }
        "pci-vmd-nested-plan" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-nested-plan"
                required_fields = @("pci-vmd", "vmd-mmio-low", "vmd-mmio-flags", "vmd-nested-plan", "vmd-nested-status", "vmd-nested-token")
                first_check = "Inspect VMD nested plan derivation from the no-touch MMIO preflight; a usable VMD candidate should export vmd-nested-plan 1 before any register access."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/shell.c")
                acceptance_signal = "A valid VMD candidate with usable MMIO reports vmd-nested-plan 1 and a nonzero vmd-nested-token."
            }
        }
        "pci-vmd-nested-enumeration" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-nested-enumeration"
                required_fields = @("pci-vmd", "vmd-mmio-low", "vmd-mmio-flags", "vmd-nested-plan", "vmd-nested-enum", "vmd-nested-status", "vmd-nested-token", "vmd-nested-pci", "vmd-nested-vendor-device", "vmd-nested-class", "vmd-nested-scan-buses", "vmd-nested-scan-devices", "vmd-nested-scan-functions", "vmd-nested-scan-windows", "vmd-nested-scan-truncated")
                first_check = "Inspect the capability-scoped, read-only VMD nested PCI-domain scan; it remaps a 64 KiB window across the first nested bus and exports child BDF/class identity without programming storage registers."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/paging.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "The physical transcript advances from vmd-nested-enum 0 to vmd-nested-enum 1 and reports either child NVMe count or a precise nested class-scan failure."
            }
        }
        "pci-vmd-nested-nvme-class" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-nested-nvme-class"
                required_fields = @("vmd-nested-plan", "vmd-nested-enum", "vmd-nested-nvme", "vmd-nested-status", "vmd-nested-token", "vmd-nested-pci", "vmd-nested-vendor-device", "vmd-nested-class", "vmd-nested-scan-buses", "vmd-nested-scan-devices", "vmd-nested-scan-functions", "vmd-nested-scan-windows", "vmd-nested-scan-truncated")
                first_check = "Inspect the bounded read-only nested PCI class-code scan coverage under the VMD domain and compare the first child class/prog-if packing against direct NVMe matching."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "vmd-nested-nvme becomes nonzero when a child NVMe controller is present behind VMD."
            }
        }
        "pci-vmd-nested-nvme-bind" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-vmd-nested-nvme-bind"
                required_fields = @("vmd-nested-plan", "vmd-nested-enum", "vmd-nested-nvme", "vmd-nested-pci", "vmd-nested-vendor-device", "vmd-nested-class", "vmd-nested-bar0", "vmd-nested-bar1", "vmd-nested-scan-buses", "vmd-nested-scan-devices", "vmd-nested-scan-functions", "vmd-nested-scan-windows", "vmd-nested-scan-truncated", "nvme-found")
                first_check = "Bind the existing NVMe controller path to the child controller identity exported by VMD nested enumeration, preserving the scoped storage authority model."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/arch/x86_64/mmio.c", "kernel/include/pci_x64.h", "kernel/include/mmio_x64.h")
                acceptance_signal = "nvme-found 1 appears for a controller reached through VMD and the transcript proceeds to nvme-ready or a precise NVMe controller stage."
            }
        }
        "pci-nvme-hidden-by-intel-system" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-intel-system-vmd-candidate"
                required_fields = @("pci-storage", "pci-nvme", "pci-intel-system", "pci-vmd", "intel-system-pci", "intel-system-vendor-device", "intel-system-class")
                first_check = "Inspect the Intel system-class device identity and BARs; if it is VMD-like, enumerate its downstream domain before attempting the regular NVMe path."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/scaffold_storage.c")
                acceptance_signal = "The physical transcript either shows pci-nvme 1 after nested enumeration or reports a precise unsupported VMD/RST bridge stage with identity fields."
            }
        }
        "pci-nvme-other-storage" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-other-storage-class"
                required_fields = @("pci-storage", "pci-nvme", "pci-other-storage", "other-storage-pci", "other-storage-vendor-device", "other-storage-class")
                first_check = "Decode the non-AHCI/non-NVMe storage subclass and decide whether the next driver target is RAID/RST, storage bridge enumeration, or a different controller family."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/scaffold_storage.c")
                acceptance_signal = "The next capture no longer stops at generic pci-nvme-class and instead reaches direct NVMe, AHCI, or a named bridge-driver stage."
            }
        }
        "pci-nvme-bdf" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-bdf-export"
                required_fields = @("pci-nvme", "nvme-pci", "nvme-found")
                first_check = "Inspect first-NVMe cache population and the capability owner used by pci64_first_nvme_address."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/shell.c")
                acceptance_signal = "nvme-pci is not 0xFFFFFFFF when pci-nvme is nonzero."
            }
        }
        "pci-nvme-identity" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-identity-read"
                required_fields = @("nvme-pci", "nvme-vendor-device")
                first_check = "Inspect PCI config vendor/device reads for the selected BDF and reject all-zero identity before attempting MMIO."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "nvme-vendor-device is nonzero for the selected BDF."
            }
        }
        "pci-nvme-class-code" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-class-code"
                required_fields = @("nvme-pci", "nvme-class")
                first_check = "Inspect class code packing and ensure base class 0x01 and subclass 0x08 are preserved in nvme-class."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "nvme-class matches 0x0108xxxx for the selected BDF."
            }
        }
        "pci-nvme-bar0" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-bar0"
                required_fields = @("nvme-pci", "nvme-bar0")
                first_check = "Inspect PCI BAR0 reads, memory-vs-IO BAR filtering, all-ones rejection, and 64-bit BAR pairing."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "nvme-bar0 is neither zero nor 0xFFFFFFFF."
            }
        }
        "pci-nvme-mmio-base" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-mmio-base"
                required_fields = @("nvme-bar0", "nvme-bar1", "nvme-mmio-low", "nvme-mmio-high")
                first_check = "Inspect BAR masking, low/high 64-bit base construction, and rejection of zero/all-ones MMIO base values."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h", "kernel/arch/x86_64/paging.c")
                acceptance_signal = "nvme-mmio-low is a usable nonzero/non-sentinel MMIO base."
            }
        }
        "pci-nvme-mmio-span" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-mmio-span"
                required_fields = @("nvme-mmio-low", "nvme-mmio-span")
                first_check = "Inspect BAR size probing and the conservative fallback span used when firmware or hardware will not tolerate sizing writes."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/include/pci_x64.h")
                acceptance_signal = "nvme-mmio-span is nonzero."
            }
        }
        "pci-nvme-mmio-flags" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-mmio-flags"
                required_fields = @("nvme-mmio-low", "nvme-mmio-flags", "nvme-mmio-token")
                first_check = "Inspect MMIO candidate planning, BAR validity flags, mapping safety flags, and token generation for the selected NVMe controller."
                kernel_files = @("kernel/arch/x86_64/pci.c", "kernel/arch/x86_64/scaffold_storage.c", "kernel/include/pci_x64.h")
                acceptance_signal = "nvme-mmio-flags and nvme-mmio-token are nonzero and non-sentinel."
            }
        }
        "nvme-controller-discovery" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "pci-nvme-enumeration"
                required_fields = @("nvme-found", "pci-storage", "pci-nvme", "nvme-pci", "nvme-vendor-device", "nvme-class", "nvme-mmio-flags", "storage-triage", "token")
                first_check = "Inspect PCI class/subclass/prog-if matching, BAR0 discovery, MMIO mapping, and whether the controller is hidden behind VMD/RAID firmware mode."
                kernel_files = $commonKernelFiles
                acceptance_signal = "nvme-found 1 appears on the physical transcript."
            }
        }
        "nvme-controller-ready" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "nvme-controller-enable"
                required_fields = @("nvme-found", "nvme-ready")
                first_check = "Trace CC.EN/CSTS.RDY reset and enable sequencing, timeout budget, CAP fields, memory page size selection, and doorbell stride."
                kernel_files = $commonKernelFiles
                acceptance_signal = "nvme-ready 1 appears after nvme-found 1."
            }
        }
        "nvme-identify" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "nvme-admin-identify"
                required_fields = @("nvme-ready", "nvme-identify")
                first_check = "Inspect admin queue allocation, PRP buffer addressability, Identify opcode construction, completion status, and namespace selection."
                kernel_files = $commonKernelFiles
                acceptance_signal = "nvme-identify 1 appears and model/firmware fields are stable in the lower-level probe log."
            }
        }
        "nvme-io-queue" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "nvme-io-queue"
                required_fields = @("nvme-identify", "ioq")
                first_check = "Inspect IO submission/completion queue creation commands, queue IDs, sizes, physical contiguity, and doorbell writes."
                kernel_files = $commonKernelFiles
                acceptance_signal = "ioq 1 appears before any read-issued check."
            }
        }
        "nvme-read-issue" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "nvme-read-submit"
                required_fields = @("ioq", "read-issued")
                first_check = "Inspect namespace read command construction, LBA/count, PRP list selection, queue tail update, and submission doorbell."
                kernel_files = $commonKernelFiles
                acceptance_signal = "read-issued 1 appears."
            }
        }
        "nvme-read-completion" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "nvme-completion-poll"
                required_fields = @("read-issued", "read-completed")
                first_check = "Inspect completion polling, phase tag handling, CQ head update, interrupt masking assumptions, and timeout budget."
                kernel_files = $commonKernelFiles
                acceptance_signal = "read-completed 1 appears."
            }
        }
        "nvme-read-status" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "nvme-read-status"
                required_fields = @("read-completed", "read-status")
                first_check = "Decode the NVMe completion status and verify namespace ID, LBA, transfer length, PRP alignment, and controller data constraints."
                kernel_files = $commonKernelFiles
                acceptance_signal = "read-status 0 appears with read-completed 1."
            }
        }
        "gpt-signature" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "gpt-header"
                required_fields = @("read-status", "gpt-signature")
                first_check = "Verify LBA0/LBA1 content from the physical media, USB image writing mode, sector size assumptions, and GPT header signature parsing."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "gpt-signature 1 appears after read-status 0."
            }
        }
        "gpt-partition-table" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "gpt-partition-scan"
                required_fields = @("gpt-signature", "gpt-partitions")
                first_check = "Inspect GPT entry array location, entry size/count, partition type GUID filtering, and bounds against the reported namespace size."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "gpt-partitions is nonzero."
            }
        }
        "fat32-partition" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat32-partition-selection"
                required_fields = @("gpt-partitions", "fat32-start", "fat32-sectors")
                first_check = "Inspect FAT32 partition type recognition, basic-data GUID fallback, and partition geometry export."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "fat32-start and fat32-sectors are both nonzero."
            }
        }
        "fat32-vbr" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat32-vbr"
                required_fields = @("fat32-start", "gpt-vbr")
                first_check = "Inspect the VBR sector read, 0x55AA signature, jump/OEM tolerance, bytes-per-sector, and FAT32 signature assumptions."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "gpt-vbr 1 appears."
            }
        }
        "fat32-bpb" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat32-bpb"
                required_fields = @("gpt-vbr", "fat-bpb")
                first_check = "Inspect BPB fields: bytes per sector, sectors per cluster, reserved sectors, FAT count, FAT size, root cluster, and total sectors."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "fat-bpb 1 appears."
            }
        }
        "fat32-mount" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat32-mount"
                required_fields = @("fat-bpb", "fat-located")
                first_check = "Inspect FAT geometry construction, root directory cluster read, FAT cache setup, and cluster-to-LBA math."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "fat-located 1 appears."
            }
        }
        "fat32-unavailable" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat-source-availability"
                required_fields = @("fat-located", "fat-unavailable")
                first_check = "Inspect source availability propagation from FAT mount into shell/Linux launcher authority."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/shell.c", "kernel/arch/x86_64/linux_exec.c")
                acceptance_signal = "fat-unavailable 0 appears after fat-located 1."
            }
        }
        "fat32-error" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat-error"
                required_fields = @("fat-error")
                first_check = "Decode fat-error and inspect the exact parser rejection before changing driver behavior."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "fat-error 0 appears."
            }
        }
        "storage-capability" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "storage-authority"
                required_fields = @("rw-cap")
                first_check = "Inspect scoped shell NVMe read/write authority creation; do not add ambient storage access."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/auth.c", "kernel/arch/x86_64/shell.c")
                acceptance_signal = "rw-cap 1 appears."
            }
        }
        "storage-capability-delegation" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "storage-authority-delegation"
                required_fields = @("rw-cap", "rw-delegated")
                first_check = "Inspect capability owner/token delegation from shell scope to the NVMe FAT reader."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/auth.c", "kernel/arch/x86_64/linux_exec.c")
                acceptance_signal = "rw-delegated 1 appears."
            }
        }
        "storage-capability-error" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "storage-authority-error"
                required_fields = @("rw-error")
                first_check = "Decode rw-error and inspect owner/token mismatch, stale capability, revoked capability, or missing delegation."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/auth.c")
                acceptance_signal = "rw-error 0 appears."
            }
        }
        "apps-directory-stat" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat-directory-lookup"
                required_fields = @("apps-stat")
                first_check = "Inspect FAT path normalization, root directory lookup, 8.3/LFN matching, and /APPS staging in the image."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/linux_vfs.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "apps-stat 1 appears."
            }
        }
        "apps-directory-type" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat-directory-type"
                required_fields = @("apps-stat", "apps-type")
                first_check = "Inspect FAT attribute translation so /APPS is reported as a directory."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/linux_vfs.c")
                acceptance_signal = "apps-type 2 appears."
            }
        }
        "apps-directory-read" {
            return [PSCustomObject]@{
                stage = $Stage
                component = "fat-directory-iteration"
                required_fields = @("apps-dirent", "apps-dir-result")
                first_check = "Inspect directory iterator cluster walking, deleted/LFN entry skipping, and first visible dirent export."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/linux_vfs.c", "kernel/arch/x86_64/mmio.c")
                acceptance_signal = "apps-dirent 1 appears."
            }
        }
        default {
            return [PSCustomObject]@{
                stage = $Stage
                component = "staged-artifact-or-unknown"
                required_fields = @("boot-staged", "boot-app-bytes", "boot-interp-bytes", "dynldlimit-stat", "dynldlimit-bytes", "ldlimit-stat", "ldlimit-bytes", "stage-match")
                first_check = "Inspect boot-media staging, NVMe /APPS visibility, expected byte counts, and stage-match fields for this stage."
                kernel_files = @("kernel/arch/x86_64/scaffold_storage.c", "kernel/arch/x86_64/linux_exec.c", "kernel/arch/x86_64/uefi_app.c")
                acceptance_signal = "The failing staged-artifact field flips to its expected value and stage-match becomes 1."
            }
        }
    }
}

function Get-ManifestValue
{
    param(
        [object]$Manifest,
        [string]$ObjectName,
        [string]$PropertyName,
        [string]$Default = ""
    )

    if ($null -eq $Manifest) {
        return $Default
    }
    $objectProperty = $Manifest.PSObject.Properties[$ObjectName]
    if ($null -eq $objectProperty) {
        return $Default
    }
    $valueProperty = $objectProperty.Value.PSObject.Properties[$PropertyName]
    if ($null -eq $valueProperty) {
        return $Default
    }
    return [string]$valueProperty.Value
}

Assert-FileExists -Path $InputPath -Message "Hardware storage analyzer: input file not found: $InputPath"

$manifest = $null
if (-not [string]::IsNullOrWhiteSpace($EvidenceManifestPath)) {
    Assert-FileExists -Path $EvidenceManifestPath -Message "Hardware storage analyzer: evidence manifest not found: $EvidenceManifestPath"
    $manifest = Get-Content -Raw -Path $EvidenceManifestPath | ConvertFrom-Json
    $appBytesText = Get-ManifestValue -Manifest $manifest -ObjectName "dynamic_app" -PropertyName "bytes"
    $interpBytesText = Get-ManifestValue -Manifest $manifest -ObjectName "dynamic_interpreter" -PropertyName "bytes"
    if (-not [string]::IsNullOrWhiteSpace($appBytesText)) {
        $ExpectedDynamicAppBytes = [uint32]$appBytesText
    }
    if (-not [string]::IsNullOrWhiteSpace($interpBytesText)) {
        $ExpectedDynamicInterpBytes = [uint32]$interpBytesText
    }
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$parserJsonPath = Join-Path $OutputDir "hardware-storage-capture.json"
$parserOutputPath = Join-Path $OutputDir "parse-hardware-storage-capture.txt"
$parserArgs = @{
    InputPath = (Resolve-Path $InputPath).Path
    OutputPath = $parserJsonPath
    ExpectedDynamicAppBytes = $ExpectedDynamicAppBytes
    ExpectedDynamicInterpBytes = $ExpectedDynamicInterpBytes
}
if ($RequireStagedDynamicArtifacts.IsPresent) {
    $parserArgs["RequireStagedDynamicArtifacts"] = $true
}

$global:LASTEXITCODE = 0
$parserOutput = & (Join-Path $root "tools\parse-hardware-storage-capture.ps1") @parserArgs 2>&1
$parserExitCode = $LASTEXITCODE
$parserOutput | Set-Content -Path $parserOutputPath -Encoding Ascii
if (($parserExitCode -ne 0) -and ($parserExitCode -ne 2)) {
    throw "Hardware storage analyzer: parser failed unexpectedly with exit code $parserExitCode. See $parserOutputPath"
}
Assert-FileExists -Path $parserJsonPath -Message "Hardware storage analyzer: parser did not produce JSON output."

$parsed = Get-Content -Raw -Path $parserJsonPath | ConvertFrom-Json
$stage = [string]$parsed.classification.stage
$pass = [bool]$parsed.classification.pass
$detail = [string]$parsed.classification.detail
$nextTarget = Get-NextTarget -Stage $stage
$diagnostic = New-DiagnosticPlan -Stage $stage -Parsed $parsed

$analysis = [PSCustomObject]@{
    tool = "analyze-hardware-storage-capture"
    input = (Resolve-Path $InputPath).Path
    parser_exit_code = $parserExitCode
    require_staged_dynamic_artifacts = [bool]$RequireStagedDynamicArtifacts
    expected_dynamic_app_bytes = $ExpectedDynamicAppBytes
    expected_dynamic_interp_bytes = $ExpectedDynamicInterpBytes
    evidence_manifest = if ([string]::IsNullOrWhiteSpace($EvidenceManifestPath)) { "" } else { (Resolve-Path $EvidenceManifestPath).Path }
    pass = $pass
    stage = $stage
    detail = $detail
    next_target = $nextTarget
    diagnostic = $diagnostic
    key_fields = [PSCustomObject]@{
        nvme_found = Get-Field -Fields $parsed.fields -Name "nvme-found"
        pci_storage = Get-Field -Fields $parsed.fields -Name "pci-storage"
        pci_nvme = Get-Field -Fields $parsed.fields -Name "pci-nvme"
        nvme_pci = Get-Field -Fields $parsed.fields -Name "nvme-pci"
        nvme_vendor_device = Get-Field -Fields $parsed.fields -Name "nvme-vendor-device"
        nvme_class = Get-Field -Fields $parsed.fields -Name "nvme-class"
        nvme_mmio_flags = Get-Field -Fields $parsed.fields -Name "nvme-mmio-flags"
        nvme_ready = Get-Field -Fields $parsed.fields -Name "nvme-ready"
        nvme_identify = Get-Field -Fields $parsed.fields -Name "nvme-identify"
        ioq = Get-Field -Fields $parsed.fields -Name "ioq"
        read_completed = Get-Field -Fields $parsed.fields -Name "read-completed"
        read_status = Get-Field -Fields $parsed.fields -Name "read-status"
        gpt_signature = Get-Field -Fields $parsed.fields -Name "gpt-signature"
        fat_located = Get-Field -Fields $parsed.fields -Name "fat-located"
        apps_stat = Get-Field -Fields $parsed.fields -Name "apps-stat"
        dynldlimit_stat = Get-Field -Fields $parsed.fields -Name "dynldlimit-stat"
        ldlimit_stat = Get-Field -Fields $parsed.fields -Name "ldlimit-stat"
        stage_match = Get-Field -Fields $parsed.fields -Name "stage-match"
    }
    raw_line = [string]$parsed.raw_line
}

$analysisJsonPath = Join-Path $OutputDir "hardware-storage-analysis.json"
$analysisTextPath = Join-Path $OutputDir "hardware-storage-analysis.txt"
$analysisMarkdownPath = Join-Path $OutputDir "hardware-storage-analysis.md"

$fieldNvmeFound = Get-Field -Fields $parsed.fields -Name "nvme-found"
$fieldPciStorage = Get-Field -Fields $parsed.fields -Name "pci-storage"
$fieldPciNvme = Get-Field -Fields $parsed.fields -Name "pci-nvme"
$fieldNvmePci = Get-Field -Fields $parsed.fields -Name "nvme-pci"
$fieldNvmeVendorDevice = Get-Field -Fields $parsed.fields -Name "nvme-vendor-device"
$fieldNvmeClass = Get-Field -Fields $parsed.fields -Name "nvme-class"
$fieldNvmeMmioFlags = Get-Field -Fields $parsed.fields -Name "nvme-mmio-flags"
$fieldNvmeReady = Get-Field -Fields $parsed.fields -Name "nvme-ready"
$fieldNvmeIdentify = Get-Field -Fields $parsed.fields -Name "nvme-identify"
$fieldIoQueue = Get-Field -Fields $parsed.fields -Name "ioq"
$fieldReadCompleted = Get-Field -Fields $parsed.fields -Name "read-completed"
$fieldReadStatus = Get-Field -Fields $parsed.fields -Name "read-status"
$fieldGptSignature = Get-Field -Fields $parsed.fields -Name "gpt-signature"
$fieldFatLocated = Get-Field -Fields $parsed.fields -Name "fat-located"
$fieldAppsStat = Get-Field -Fields $parsed.fields -Name "apps-stat"
$fieldDynldlimitStat = Get-Field -Fields $parsed.fields -Name "dynldlimit-stat"
$fieldLdlimitStat = Get-Field -Fields $parsed.fields -Name "ldlimit-stat"
$fieldStageMatch = Get-Field -Fields $parsed.fields -Name "stage-match"
$diagnosticRequiredFieldLines = @($diagnostic.required_fields | ForEach-Object { "- $_" })
$diagnosticKernelFileLines = @($diagnostic.kernel_files | ForEach-Object { "- $_" })
if ($diagnosticKernelFileLines.Count -eq 0) {
    $diagnosticKernelFileLines = @("- none")
}

$analysis | ConvertTo-Json -Depth 6 | Set-Content -Path $analysisJsonPath -Encoding Ascii

@(
    "hardware-storage-analysis: $stage",
    "pass: $pass",
    "detail: $detail",
    "next-target: $nextTarget",
    "parser-exit-code: $parserExitCode",
    "require-staged-dynamic-artifacts: $([bool]$RequireStagedDynamicArtifacts)",
    "expected-dynamic-app-bytes: $ExpectedDynamicAppBytes",
    "expected-dynamic-interp-bytes: $ExpectedDynamicInterpBytes",
    "output-json: $analysisJsonPath",
    "output-report: $analysisMarkdownPath"
) | Set-Content -Path $analysisTextPath -Encoding Ascii

@(
    "# LimitlessOS M114 Hardware Storage Analysis",
    "",
    "- Pass: $pass",
    "- Stage: $stage",
    "- Detail: $detail",
    "- Next target: $nextTarget",
    "- Parser exit code: $parserExitCode",
    "- Staged dynamic artifacts required: $([bool]$RequireStagedDynamicArtifacts)",
    "",
    "## Key Fields",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| nvme-found | $fieldNvmeFound |",
    "| pci-storage | $fieldPciStorage |",
    "| pci-nvme | $fieldPciNvme |",
    "| nvme-pci | $fieldNvmePci |",
    "| nvme-vendor-device | $fieldNvmeVendorDevice |",
    "| nvme-class | $fieldNvmeClass |",
    "| nvme-mmio-flags | $fieldNvmeMmioFlags |",
    "| nvme-ready | $fieldNvmeReady |",
    "| nvme-identify | $fieldNvmeIdentify |",
    "| ioq | $fieldIoQueue |",
    "| read-completed | $fieldReadCompleted |",
    "| read-status | $fieldReadStatus |",
    "| gpt-signature | $fieldGptSignature |",
    "| fat-located | $fieldFatLocated |",
    "| apps-stat | $fieldAppsStat |",
    "| dynldlimit-stat | $fieldDynldlimitStat |",
    "| ldlimit-stat | $fieldLdlimitStat |",
    "| stage-match | $fieldStageMatch |",
    "",
    "## Diagnostic Plan",
    "",
    "| Field | Value |",
    "| --- | --- |",
    "| Component | $($diagnostic.component) |",
    "| First check | $($diagnostic.first_check) |",
    "| Acceptance signal | $($diagnostic.acceptance_signal) |",
    "",
    "### Required Telemetry Fields",
    "",
    $diagnosticRequiredFieldLines,
    "",
    "### Code Areas",
    "",
    $diagnosticKernelFileLines,
    "",
    "## Raw Triage Line",
    "",
    '```text',
    "$($parsed.raw_line)",
    '```'
) | Set-Content -Path $analysisMarkdownPath -Encoding Ascii

Write-Host "hardware-storage-analysis: $stage"
Write-Host "  pass: $pass"
Write-Host "  detail: $detail"
Write-Host "  next target: $nextTarget"
Write-Host "  output: $analysisJsonPath"

if (-not $pass) {
    exit 2
}
