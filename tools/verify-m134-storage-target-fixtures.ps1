param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "build\m134-storage-target-fixtures"
}

$fixtureRoot = Join-Path $OutputDir "evidence"
$captureRoot = Join-Path $OutputDir "captures"
$resultRoot = Join-Path $OutputDir "results"
New-Item -ItemType Directory -Force -Path $fixtureRoot | Out-Null
New-Item -ItemType Directory -Force -Path $captureRoot | Out-Null
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$handoffMilestone = "M133"
$handoffStem = "m133"
$handoffIsoName = "limitlessos-x86_64-$handoffStem-handoff.iso"
$handoffUefiName = "limitlessos-x86_64-$handoffStem-handoff-uefi.img"

function Get-Sha256
{
    param([string]$Path)

    return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

function Write-BinaryFixture
{
    param(
        [string]$Path,
        [uint32]$Bytes,
        [byte]$Seed
    )

    $data = [byte[]]::new($Bytes)
    for ($i = 0; $i -lt $data.Length; $i++) {
        $data[$i] = [byte](($Seed + $i) -band 0xFF)
    }
    [System.IO.File]::WriteAllBytes($Path, $data)
}

function New-EvidenceBundle
{
    param([string]$Name)

    $evidenceDir = Join-Path $fixtureRoot $Name
    New-Item -ItemType Directory -Force -Path $evidenceDir | Out-Null

    $isoPath = Join-Path $evidenceDir $handoffIsoName
    $uefiPath = Join-Path $evidenceDir $handoffUefiName
    $appPath = Join-Path $evidenceDir "DYNLDLIMIT"
    $interpPath = Join-Path $evidenceDir "LDLIMIT"

    Write-BinaryFixture -Path $isoPath -Bytes 4096 -Seed 0x21
    Write-BinaryFixture -Path $uefiPath -Bytes 2048 -Seed 0x32
    Write-BinaryFixture -Path $appPath -Bytes 15680 -Seed 0x43
    Write-BinaryFixture -Path $interpPath -Bytes 16704 -Seed 0x54

    $appSha = Get-Sha256 -Path $appPath
    $interpSha = Get-Sha256 -Path $interpPath

    [PSCustomObject]@{
        milestone = $handoffMilestone
        purpose = "MSI hardware handoff evidence bundle"
        generated_utc = "2026-06-28T00:00:00Z"
        git_commit = "fixture"
        iso = [PSCustomObject]@{
            path = $handoffIsoName
            bytes = (Get-Item $isoPath).Length
            sha256 = Get-Sha256 -Path $isoPath
        }
        uefi_image = [PSCustomObject]@{
            path = $handoffUefiName
            bytes = (Get-Item $uefiPath).Length
            sha256 = Get-Sha256 -Path $uefiPath
        }
        dynamic_app = [PSCustomObject]@{
            path = "/APPS/DYNLDLIMIT"
            evidence_file = "DYNLDLIMIT"
            source = "fixture"
            bytes = (Get-Item $appPath).Length
            sha256 = $appSha
        }
        dynamic_interpreter = [PSCustomObject]@{
            path = "/APPS/LDLIMIT"
            evidence_file = "LDLIMIT"
            source = "fixture"
            bytes = (Get-Item $interpPath).Length
            sha256 = $interpSha
        }
        reserves = [PSCustomObject]@{
            bios_sectors = 101
            uefi_bytes = 749408
        }
        expected_hwval = [PSCustomObject]@{
            command = "hwval"
            required_line = "drs-nvme-triage"
            capture_report = "tools\\report-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry"
            analyzer = "tools\\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts"
            storage_target_classifier = "tools\\classify-m134-storage-target.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry"
            storage_verifier = "tools\\verify-hardware-storage-evidence.ps1 -RequireStagedDynamicArtifacts"
            boot_media_handoff_verifier = "tools\\verify-boot-media-linux-handoff.ps1"
            required_storage_stage = "storage-ready"
            required_boot_media_linux_source = "2"
            required_gui_interaction_telemetry = "1"
        }
    } | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $evidenceDir "hardware-storage-evidence-manifest.json") -Encoding Ascii

    @(
        "LimitlessOS $handoffMilestone MSI hardware handoff evidence bundle",
        "dynamic-app-sha256: $appSha",
        "dynamic-interpreter-sha256: $interpSha"
    ) | Set-Content -Path (Join-Path $evidenceDir "hardware-storage-evidence-manifest.txt") -Encoding Ascii

    @(
        "LimitlessOS boot manifest v1",
        "boot-linux-expected=1",
        "boot-linux-app=/APPS/DYNLDLIMIT",
        "boot-linux-app-bytes=15680",
        "boot-linux-app-sha256=$appSha",
        "boot-linux-interp=/APPS/LDLIMIT",
        "boot-linux-interp-bytes=16704",
        "boot-linux-interp-sha256=$interpSha"
    ) | Set-Content -Path (Join-Path $evidenceDir "BOOTMAN.TXT") -Encoding Ascii

    @(
        "LimitlessOS x86_64 size map",
        "bios-sector-reserve=101",
        "uefi-kernel-byte-reserve=749408"
    ) | Set-Content -Path (Join-Path $evidenceDir "limitlessos-x86_64.size.txt") -Encoding Ascii

    @(
        "LimitlessOS $handoffMilestone MSI Hardware Handoff Runbook",
        "",
        "Run these commands on the physical laptop:",
        "",
        "hwval",
        "linux /APPS/DYNLDLIMIT",
        "",
        "Analyze with:",
        "",
        ".\tools\report-msi-hardware-capture.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <capture-report-output-dir> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry",
        "",
        ".\tools\classify-m134-storage-target.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <m134-target-output-dir> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry",
        "",
        ".\tools\verify-msi-hardware-handoff.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry",
        "",
        ".\tools\analyze-msi-hardware-capture.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <analysis-output-dir> -RequireStagedDynamicArtifacts",
        "",
        "Expected handoff signal:",
        "",
        "drs-gui ... drs-gui-right-click 1 ... drs-gui-context-action 1 ... drs-gui-scroll ...",
        "",
        "",
        "linux: using UEFI boot-media staged file",
        "drs-realbin ... source 2 ... boot-media-read 1",
        "",
        ".\tools\verify-boot-media-linux-handoff.ps1"
    ) | Set-Content -Path (Join-Path $evidenceDir "README-HARDWARE-STORAGE.txt") -Encoding Ascii

    return $evidenceDir
}

function New-StorageLine
{
    param([hashtable]$Mutations = @{})

    $fields = [ordered]@{
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
        "nvme-candidate-source" = "1"
        "nvme-candidate-deferred" = "0"
        "nvme-candidate-bdf" = "0xFFFFFFFF"
        "nvme-candidate-token" = "0xC1480001"
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
        "vmd-nested-register-candidate" = "0"
        "vmd-nested-register-status" = "0"
        "vmd-nested-register-token" = "0x00000000"
        "vmd-nested-driver-plan-result" = "0xFFFFFFFF"
        "vmd-nested-driver-plan-state" = "0"
        "vmd-nested-driver-plan-flags" = "0x00000000"
        "vmd-nested-driver-plan-token" = "0x00000000"
        "vmd-nested-driver-plan-stage-count" = "0"
        "vmd-nested-driver-plan-denials" = "0"
        "vmd-nested-driver-plan-unavailable" = "0"
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
    foreach ($key in $Mutations.Keys) {
        $fields[$key] = [string]$Mutations[$key]
    }

    $parts = @()
    foreach ($key in $fields.Keys) {
        $parts += ("{0} {1}" -f $key, $fields[$key])
    }
    return "[x64] drs-nvme-triage " + ($parts -join " ")
}

function Write-Capture
{
    param(
        [string]$Path,
        [hashtable]$StorageMutations = @{},
        [string]$DisplayMode = "ready",
        [string]$DynamicMode = "source2-exit0"
    )

    $lines = @()
    $lines += New-StorageLine -Mutations $StorageMutations

    if (($DisplayMode -eq "ready") -or ($DisplayMode -eq "ready-no-gui")) {
        $lines += "[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 205 direct-cursor-draws 207 token 0xF8C98059"
        $lines += "[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 1 token 0xCB1B1C83"
        $lines += "[x64] drs-cursor-path cursor-path 1 surface-ready 1 format-supported 1 compositor-active 1 compositor-direct 1 visible 1 draws 205 direct-draws 207 x 640 y 400 buttons 0 in-bounds 1 rect-w 12 rect-h 20 saved 1 drawn 1 token 0xA5197C42"
        if ($DisplayMode -eq "ready") {
            $lines += "[x64] drs-gui drs-gui-interactive 1 drs-gui-click-hittest 1 drs-gui-launcher-opened 1 drs-gui-terminal-opened 1 drs-gui-drag-completed 1 drs-gui-keyboard-routed 1 drs-gui-close-completed 1 drs-gui-taskbar-focus 1 drs-gui-fileman-opened 1 drs-gui-settings-opened 1 drs-gui-installer-opened 1 drs-gui-right-click 1 drs-gui-context-action 1 wm-resize 1 wm-minimize 1 wm-restore 1 wm-zorder 1 drs-gui-scroll 2 terminal-actions 2 terminal-scroll 1 terminal-scroll-offset 512 terminal-selection 1 terminal-copy 1 terminal-copied-bytes 16 terminal-cursor 1 fileman-actions 1 fileman-refresh 1 fileman-write 1 fileman-mkdir 1 fileman-edit 1 fileman-edit-commit 1 drs-gui-unfocused-key-denied 0 drs-gui-no-ambient-input 1 drs-gui-no-ambient-display 1 drs-gui-no-ambient-fs 1 target-window 1 key-target-window 1 unfocused-key-denials 0 input-token 0x494E5054 display-token 0x44495350 fs-token 0x46535041"
        }
        $lines += "xhci mouse endpoint: yes"
        $lines += "xhci mouse reports: 2"
        $lines += "xhci mouse bytes: 8"
        $lines += "xhci error: 0"
        $lines += "i2c pointer found: no"
        $lines += "i2c pointer reports: 0"
        $lines += "i2c pointer error: 0"
        $lines += "i2c pointer candidates: 0"
        $lines += "mouse packets: 2"
        $lines += "ps2 fallback present: yes"
        $lines += "ps2 fallback enabled: yes"
    } elseif ($DisplayMode -eq "cursor-draw-not-called") {
        $lines += "[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 0 cursor-draws 0 direct-cursor-draws 0 token 0xF8C98059"
        $lines += "[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 0 token 0xCB1B1C83"
        $lines += "[x64] drs-cursor-path cursor-path 1 surface-ready 1 format-supported 1 compositor-active 1 compositor-direct 1 visible 0 draws 0 direct-draws 0 x 640 y 400 buttons 0 in-bounds 1 rect-w 12 rect-h 20 saved 0 drawn 0 token 0xA5197C43"
        $lines += "xhci mouse endpoint: yes"
        $lines += "xhci mouse reports: 2"
        $lines += "xhci mouse bytes: 8"
        $lines += "xhci error: 0"
        $lines += "i2c pointer found: no"
        $lines += "i2c pointer reports: 0"
        $lines += "i2c pointer error: 0"
        $lines += "i2c pointer candidates: 0"
        $lines += "mouse packets: 2"
        $lines += "ps2 fallback present: yes"
        $lines += "ps2 fallback enabled: yes"
    } elseif ($DisplayMode -eq "gui-right-click-unrouted") {
        $lines += "[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 205 direct-cursor-draws 207 token 0xF8C98059"
        $lines += "[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 1 token 0xCB1B1C83"
        $lines += "[x64] drs-cursor-path cursor-path 1 surface-ready 1 format-supported 1 compositor-active 1 compositor-direct 1 visible 1 draws 205 direct-draws 207 x 640 y 400 buttons 0 in-bounds 1 rect-w 12 rect-h 20 saved 1 drawn 1 token 0xA5197C42"
        $lines += "[x64] drs-gui drs-gui-interactive 1 drs-gui-click-hittest 1 drs-gui-launcher-opened 1 drs-gui-terminal-opened 1 drs-gui-drag-completed 1 drs-gui-keyboard-routed 1 drs-gui-close-completed 1 drs-gui-taskbar-focus 1 drs-gui-fileman-opened 1 drs-gui-settings-opened 1 drs-gui-installer-opened 1 drs-gui-right-click 0 drs-gui-context-action 0 wm-resize 1 wm-minimize 1 wm-restore 1 wm-zorder 1 drs-gui-scroll 2 terminal-actions 2 terminal-scroll 1 terminal-scroll-offset 512 terminal-selection 1 terminal-copy 1 terminal-copied-bytes 16 terminal-cursor 1 fileman-actions 1 fileman-refresh 1 fileman-write 1 fileman-mkdir 1 fileman-edit 1 fileman-edit-commit 1 drs-gui-unfocused-key-denied 0 drs-gui-no-ambient-input 1 drs-gui-no-ambient-display 1 drs-gui-no-ambient-fs 1 target-window 1 key-target-window 1 unfocused-key-denials 0 input-token 0x494E5054 display-token 0x44495350 fs-token 0x46535041"
        $lines += "xhci mouse endpoint: yes"
        $lines += "xhci mouse reports: 2"
        $lines += "xhci mouse bytes: 8"
        $lines += "xhci error: 0"
        $lines += "i2c pointer found: no"
        $lines += "i2c pointer reports: 0"
        $lines += "i2c pointer error: 0"
        $lines += "i2c pointer candidates: 0"
        $lines += "mouse packets: 2"
        $lines += "ps2 fallback present: yes"
        $lines += "ps2 fallback enabled: yes"
    } elseif ($DisplayMode -eq "cursor-hidden") {
        $lines += "[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 0 cursor-draws 0 direct-cursor-draws 0 token 0xF8C98059"
        $lines += "[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 0 token 0xCB1B1C83"
        $lines += "xhci mouse endpoint: yes"
        $lines += "xhci mouse reports: 2"
        $lines += "xhci mouse bytes: 8"
        $lines += "xhci error: 0"
        $lines += "i2c pointer found: no"
        $lines += "i2c pointer reports: 0"
        $lines += "i2c pointer error: 0"
        $lines += "i2c pointer candidates: 0"
        $lines += "mouse packets: 2"
        $lines += "ps2 fallback present: yes"
        $lines += "ps2 fallback enabled: yes"
    } else {
        throw "Unknown display mode: $DisplayMode"
    }

    $lines += "[x64] $ linux /APPS/DYNLDLIMIT"
    switch ($DynamicMode) {
        "source2-exit0" {
            $lines += "linux: using UEFI boot-media staged file"
            $lines += "[x64] drs-realbin path /APPS/DYNLDLIMIT provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-transfer-started 1 console-bytes 15 exit 0 cleanup 1 page-faults 0"
        }
        "source2-runtime-fail" {
            $lines += "linux: using UEFI boot-media staged file"
            $lines += "[x64] drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage static code 8 boot-media-read 1 boot-media-read-error 0 boot-media-read-bytes 15680 boot-media-read-capacity 4194304"
        }
        "nvme-unavailable" {
            $lines += "linux: NVMe FAT unavailable"
            $lines += "[x64] drs-realbin-unavailable bios 0 nvme 0"
        }
        default {
            throw "Unknown dynamic mode: $DynamicMode"
        }
    }

    $lines | Set-Content -Path $Path -Encoding Ascii
}

$fixtures = @(
    [PSCustomObject]@{
        name = "storage-nvme-controller-discovery"
        storage_mutations = @{ "nvme-found" = "0" }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "nvme-controller-discovery"
        expected_roadmap = "M134"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-pci-nvme-class"
        storage_mutations = @{ "pci-nvme" = "0"; "nvme-found" = "0" }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "pci-nvme-class"
        expected_roadmap = "M134"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-pci-nvme-hidden-by-vmd"
        storage_mutations = @{
            "pci-nvme" = "0";
            "pci-intel-system" = "1";
            "pci-vmd" = "1";
            "vmd-pci" = "0x00000E00";
            "vmd-vendor-device" = "0x467F8086";
            "vmd-class" = "0x08800000";
            "vmd-bar0" = "0xFE010004";
            "vmd-bar1" = "0x00000000";
            "vmd-mmio-low" = "0xFE010000";
            "vmd-mmio-high" = "0x00000000";
            "vmd-mmio-span" = "65536";
            "vmd-mmio-flags" = "0x000003FF";
            "vmd-mmio-token" = "0x94D5D769";
            "vmd-nested-plan" = "1";
            "vmd-nested-enum" = "0";
            "vmd-nested-nvme" = "0";
            "vmd-nested-status" = "1";
            "vmd-nested-token" = "0xD204D931";
            "nvme-found" = "0"
        }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "pci-vmd-nested-enumeration"
        expected_roadmap = "M134"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-pci-vmd-mmio-base"
        storage_mutations = @{
            "pci-nvme" = "0";
            "pci-intel-system" = "1";
            "pci-vmd" = "1";
            "vmd-pci" = "0x00000E00";
            "vmd-vendor-device" = "0x467F8086";
            "vmd-class" = "0x08800000";
            "vmd-bar0" = "0xFE010004";
            "vmd-bar1" = "0x00000000";
            "vmd-mmio-low" = "0x00000000";
            "vmd-mmio-high" = "0x00000000";
            "vmd-mmio-span" = "65536";
            "vmd-mmio-flags" = "0x000003FF";
            "vmd-mmio-token" = "0x94D5D769";
            "nvme-found" = "0"
        }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "pci-vmd-mmio-base"
        expected_roadmap = "M134"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-pci-vmd-nested-nvme-mmio-base"
        storage_mutations = @{
            "pci-nvme" = "0";
            "pci-intel-system" = "1";
            "pci-vmd" = "1";
            "vmd-pci" = "0x00000E00";
            "vmd-vendor-device" = "0x467F8086";
            "vmd-class" = "0x08800000";
            "vmd-bar0" = "0xFE010004";
            "vmd-bar1" = "0x00000000";
            "vmd-mmio-low" = "0xFE010000";
            "vmd-mmio-high" = "0x00000000";
            "vmd-mmio-span" = "1048576";
            "vmd-mmio-flags" = "0x000003FF";
            "vmd-mmio-token" = "0x94D5D769";
            "vmd-nested-plan" = "1";
            "vmd-nested-enum" = "1";
            "vmd-nested-nvme" = "1";
            "vmd-nested-status" = "3";
            "vmd-nested-token" = "0xD204D933";
            "vmd-nested-pci" = "0x00000100";
            "vmd-nested-vendor-device" = "0x00101B36";
            "vmd-nested-class" = "0x01080202";
            "vmd-nested-bar0" = "0x00000000";
            "vmd-nested-bar1" = "0x00000000";
            "vmd-nested-scan-buses" = "1";
            "vmd-nested-scan-devices" = "32";
            "vmd-nested-scan-functions" = "256";
            "vmd-nested-scan-windows" = "16";
            "vmd-nested-scan-truncated" = "1";
            "vmd-nested-mmio-low" = "0x00000000";
            "vmd-nested-mmio-high" = "0x00000000";
            "vmd-nested-mmio-span" = "16384";
            "vmd-nested-mmio-flags" = "0x00000183";
            "vmd-nested-mmio-token" = "0xA1450CC1";
            "nvme-found" = "0"
        }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "pci-vmd-nested-nvme-mmio-base"
        expected_roadmap = "M134"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-pci-nvme-hidden-by-intel-system"
        storage_mutations = @{
            "pci-nvme" = "0";
            "pci-intel-system" = "1";
            "intel-system-pci" = "0x00000E00";
            "intel-system-vendor-device" = "0x467F8086";
            "intel-system-class" = "0x08800000";
            "nvme-found" = "0"
        }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "pci-nvme-hidden-by-intel-system"
        expected_roadmap = "M134"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-pci-vmd-nested-nvme-bind-ready"
        storage_mutations = @{
            "pci-nvme" = "0";
            "pci-intel-system" = "1";
            "pci-vmd" = "1";
            "vmd-pci" = "0x00000E00";
            "vmd-vendor-device" = "0x467F8086";
            "vmd-class" = "0x08800000";
            "vmd-bar0" = "0xFE010004";
            "vmd-bar1" = "0x00000000";
            "vmd-mmio-low" = "0xFE010000";
            "vmd-mmio-high" = "0x00000000";
            "vmd-mmio-span" = "1048576";
            "vmd-mmio-flags" = "0x000003FF";
            "vmd-mmio-token" = "0x94D5D769";
            "vmd-nested-plan" = "1";
            "vmd-nested-enum" = "1";
            "vmd-nested-nvme" = "1";
            "vmd-nested-status" = "3";
            "vmd-nested-token" = "0xD204D933";
            "vmd-nested-pci" = "0x00000100";
            "vmd-nested-vendor-device" = "0x00101B36";
            "vmd-nested-class" = "0x01080202";
            "vmd-nested-bar0" = "0xFE020004";
            "vmd-nested-bar1" = "0x00000000";
            "vmd-nested-scan-buses" = "1";
            "vmd-nested-scan-devices" = "32";
            "vmd-nested-scan-functions" = "256";
            "vmd-nested-scan-windows" = "16";
            "vmd-nested-scan-truncated" = "1";
            "vmd-nested-mmio-low" = "0xFE020000";
            "vmd-nested-mmio-high" = "0x00000000";
            "vmd-nested-mmio-span" = "16384";
            "vmd-nested-mmio-flags" = "0x000001FF";
            "vmd-nested-mmio-token" = "0xA1450CC4";
            "vmd-nested-bind-ready" = "0";
            "vmd-nested-bind-status" = "4";
            "vmd-nested-bind-token" = "0xB1460CC0";
            "vmd-nested-register-candidate" = "0";
            "vmd-nested-register-status" = "1";
            "vmd-nested-register-token" = "0xC1470CC0";
            "nvme-found" = "0"
        }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "pci-vmd-nested-nvme-bind-ready"
        expected_roadmap = "M134"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-pci-vmd-nested-nvme-register-deferred"
        storage_mutations = @{
            "pci-nvme" = "0";
            "pci-intel-system" = "1";
            "pci-vmd" = "1";
            "vmd-pci" = "0x00000E00";
            "vmd-vendor-device" = "0x467F8086";
            "vmd-class" = "0x08800000";
            "vmd-bar0" = "0xFE010004";
            "vmd-bar1" = "0x00000000";
            "vmd-mmio-low" = "0xFE010000";
            "vmd-mmio-high" = "0x00000000";
            "vmd-mmio-span" = "1048576";
            "vmd-mmio-flags" = "0x000003FF";
            "vmd-mmio-token" = "0x94D5D769";
            "vmd-nested-plan" = "1";
            "vmd-nested-enum" = "1";
            "vmd-nested-nvme" = "1";
            "vmd-nested-status" = "3";
            "vmd-nested-token" = "0xD204D933";
            "vmd-nested-pci" = "0x00000100";
            "vmd-nested-vendor-device" = "0x00101B36";
            "vmd-nested-class" = "0x01080202";
            "vmd-nested-bar0" = "0xFE020004";
            "vmd-nested-bar1" = "0x00000000";
            "vmd-nested-scan-buses" = "1";
            "vmd-nested-scan-devices" = "32";
            "vmd-nested-scan-functions" = "256";
            "vmd-nested-scan-windows" = "16";
            "vmd-nested-scan-truncated" = "1";
            "vmd-nested-mmio-low" = "0xFE020000";
            "vmd-nested-mmio-high" = "0x00000000";
            "vmd-nested-mmio-span" = "16384";
            "vmd-nested-mmio-flags" = "0x000001FF";
            "vmd-nested-mmio-token" = "0xA1450CC4";
            "vmd-nested-bind-ready" = "1";
            "vmd-nested-bind-status" = "5";
            "vmd-nested-bind-token" = "0xB1460CC4";
            "vmd-nested-register-candidate" = "1";
            "vmd-nested-register-status" = "2";
            "vmd-nested-register-token" = "0xC1470CC4";
            "vmd-nested-driver-plan-result" = "0xD1580CC4";
            "vmd-nested-driver-plan-state" = "2";
            "vmd-nested-driver-plan-flags" = "0x000000FF";
            "vmd-nested-driver-plan-token" = "0xD1580CC4";
            "vmd-nested-driver-plan-stage-count" = "1";
            "vmd-nested-driver-plan-denials" = "0";
            "vmd-nested-driver-plan-unavailable" = "0";
            "nvme-candidate-source" = "2";
            "nvme-candidate-deferred" = "1";
            "nvme-candidate-bdf" = "0x00000100";
            "nvme-candidate-token" = "0xC1480CC4";
            "nvme-found" = "0"
        }
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "storage"
        expected_stage = "pci-vmd-nested-driver-plan-staged"
        expected_roadmap = "M134"
        expected_pass = $false
        expected_vmd_kind = "vmd-nested-driver-plan"
        expected_vmd_stage = "driver-plan-staged"
    },
    [PSCustomObject]@{
        name = "display-after-storage-ready"
        storage_mutations = @{}
        display_mode = "cursor-hidden"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "display-input"
        expected_stage = "pointer-moving-cursor-hidden"
        expected_roadmap = "M149"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "display-cursor-draw-not-called"
        storage_mutations = @{}
        display_mode = "cursor-draw-not-called"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "display-input"
        expected_stage = "cursor-draw-not-called"
        expected_roadmap = "M150"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "display-gui-right-click-unrouted"
        storage_mutations = @{}
        display_mode = "gui-right-click-unrouted"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "display-input"
        expected_stage = "gui-right-click-unrouted"
        expected_roadmap = "M151"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "dynamic-after-storage-ready"
        storage_mutations = @{}
        display_mode = "ready"
        dynamic_mode = "nvme-unavailable"
        expected_exit_code = 2
        expected_kind = "dynamic-handoff"
        expected_stage = "dynamic-handoff-nvme-unavailable"
        expected_roadmap = "M83+"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "display-gui-telemetry-missing"
        storage_mutations = @{}
        display_mode = "ready-no-gui"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 2
        expected_kind = "display-input"
        expected_stage = "gui-telemetry-missing"
        expected_roadmap = "M152"
        expected_pass = $false
    },
    [PSCustomObject]@{
        name = "storage-ready"
        storage_mutations = @{}
        display_mode = "ready"
        dynamic_mode = "source2-exit0"
        expected_exit_code = 0
        expected_kind = "storage-ready"
        expected_stage = "storage-ready"
        expected_roadmap = "M134"
        expected_pass = $true
    }
)

$results = @()
$failures = @()
foreach ($fixture in $fixtures) {
    $evidenceDir = New-EvidenceBundle -Name $fixture.name
    $capturePath = Join-Path $captureRoot ($fixture.name + ".txt")
    Write-Capture `
        -Path $capturePath `
        -StorageMutations $fixture.storage_mutations `
        -DisplayMode $fixture.display_mode `
        -DynamicMode $fixture.dynamic_mode

    $fixtureOutputDir = Join-Path $resultRoot $fixture.name
    New-Item -ItemType Directory -Force -Path $fixtureOutputDir | Out-Null

    $consoleText = ""
    $exitCode = 0
    try {
        $global:LASTEXITCODE = 0
        $console = & (Join-Path $root "tools\classify-m134-storage-target.ps1") `
            -EvidenceDir $evidenceDir `
            -CapturePath $capturePath `
            -OutputDir $fixtureOutputDir `
            -RequireStagedDynamicArtifacts `
            -RequireGuiInteractionTelemetry 2>&1
        $consoleText = ($console | Out-String)
        $exitCode = [int]$LASTEXITCODE
    } catch {
        $consoleText = $_.Exception.Message
        $exitCode = 99
    }

    $consoleText | Set-Content -Path (Join-Path $fixtureOutputDir "classifier-console.txt") -Encoding Ascii
    $reportPath = Join-Path $fixtureOutputDir "m134-storage-target.json"
    $actualKind = ""
    $actualStage = ""
    $actualRoadmap = ""
    $actualPass = $false
    $actualVmdKind = ""
    $actualVmdStage = ""
    if (Test-Path $reportPath) {
        $report = Get-Content -Raw -Path $reportPath | ConvertFrom-Json
        $actualKind = [string]$report.target_kind
        $actualStage = [string]$report.target_stage
        $actualRoadmap = [string]$report.roadmap_target
        $actualPass = [bool]$report.pass
        if ($null -ne $report.vmd_handoff) {
            $actualVmdKind = [string]$report.vmd_handoff.kind
            $actualVmdStage = [string]$report.vmd_handoff.stage
        }
    }

    $passed = (($exitCode -eq [int]$fixture.expected_exit_code) -and
        ($actualKind -eq [string]$fixture.expected_kind) -and
        ($actualStage -eq [string]$fixture.expected_stage) -and
        ($actualRoadmap -eq [string]$fixture.expected_roadmap) -and
        ($actualPass -eq [bool]$fixture.expected_pass))
    if ($fixture.PSObject.Properties["expected_vmd_kind"]) {
        $passed = ($passed -and
            ($actualVmdKind -eq [string]$fixture.expected_vmd_kind) -and
            ($actualVmdStage -eq [string]$fixture.expected_vmd_stage))
    }
    if (-not $passed) {
        $failures += ("{0}: expected exit/kind/stage/roadmap/pass/vmd {1}/{2}/{3}/{4}/{5}/{6}/{7}, observed {8}/{9}/{10}/{11}/{12}/{13}/{14}" -f $fixture.name, $fixture.expected_exit_code, $fixture.expected_kind, $fixture.expected_stage, $fixture.expected_roadmap, $fixture.expected_pass, $(if ($fixture.PSObject.Properties["expected_vmd_kind"]) { $fixture.expected_vmd_kind } else { "" }), $(if ($fixture.PSObject.Properties["expected_vmd_stage"]) { $fixture.expected_vmd_stage } else { "" }), $exitCode, $actualKind, $actualStage, $actualRoadmap, $actualPass, $actualVmdKind, $actualVmdStage)
    }

    $results += [PSCustomObject]@{
        name = $fixture.name
        expected_exit_code = [int]$fixture.expected_exit_code
        actual_exit_code = $exitCode
        expected_kind = [string]$fixture.expected_kind
        actual_kind = $actualKind
        expected_stage = [string]$fixture.expected_stage
        actual_stage = $actualStage
        expected_roadmap = [string]$fixture.expected_roadmap
        actual_roadmap = $actualRoadmap
        expected_pass = [bool]$fixture.expected_pass
        actual_pass = $actualPass
        expected_vmd_kind = if ($fixture.PSObject.Properties["expected_vmd_kind"]) { [string]$fixture.expected_vmd_kind } else { "" }
        actual_vmd_kind = $actualVmdKind
        expected_vmd_stage = if ($fixture.PSObject.Properties["expected_vmd_stage"]) { [string]$fixture.expected_vmd_stage } else { "" }
        actual_vmd_stage = $actualVmdStage
        pass = $passed
    }
}

$summary = [PSCustomObject]@{
    tool = "verify-m134-storage-target-fixtures"
    output_dir = (Resolve-Path $OutputDir).Path
    total = @($results).Count
    passed = @($results | Where-Object { $_.pass }).Count
    failed = @($failures).Count
    failures = $failures
    results = $results
}

$summaryJsonPath = Join-Path $OutputDir "m134-storage-target-fixtures.json"
$summaryTextPath = Join-Path $OutputDir "m134-storage-target-fixtures.txt"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryJsonPath -Encoding Ascii

@(
    "m134-storage-target-fixtures: $($summary.passed)/$($summary.total)",
    "failed: $($summary.failed)",
    "output-json: $summaryJsonPath"
) + ($results | ForEach-Object {
    "{0}: expected {1}/{2}/{3}/{4} observed {5}/{6}/{7}/{8} pass {9}" -f $_.name, $_.expected_exit_code, $_.expected_kind, $_.expected_stage, $_.expected_roadmap, $_.actual_exit_code, $_.actual_kind, $_.actual_stage, $_.actual_roadmap, $_.pass
}) | Set-Content -Path $summaryTextPath -Encoding Ascii

Write-Host "m134-storage-target-fixtures: $($summary.passed)/$($summary.total)"
Write-Host "  failed: $($summary.failed)"
Write-Host "  output: $summaryJsonPath"

if (@($failures).Count -ne 0) {
    foreach ($failure in $failures) {
        Write-Host "  failure: $failure"
    }
    exit 1
}
