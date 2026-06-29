# LimitlessOS Status

Last updated: 2026-06-29

## Accepted Baseline

M1 cleanup-final is accepted. The accepted M1 artifact was archived at `dist/m1-evidence-20260511-140840-cleanup-final` with:

- kernel bytes: 468176
- kernel sectors: 915 / 1024
- reserve: 109
- checksum: 0x53122EE5
- runtime help verified: true
- runtime apps verified: true
- persistence verifier printing authority, denial, commit, same-image, and non-RAM evidence

## Current Milestone

M161 is `NVMe controller register snapshot diagnostics`. The UEFI Product NVMe probe now records a read-only controller register snapshot after the MMIO register window is mapped and refreshes it at the probe success/failure boundary. The new hardware-facing fields are emitted through `drs-nvme-triage` and the lower-level `drs-nvme-probe` proof: `nvme-probe-error`, `nvme-regs`, `nvme-cap-low`, `nvme-cap-high`, `nvme-vs`, `nvme-cc`, `nvme-csts`, `nvme-dstrd-bytes`, and `nvme-doorbell-page`. The hardware storage analyzer now carries those values in key fields and requires them for the `nvme-controller-discovery`, `nvme-controller-ready`, and `nvme-identify` diagnostic stages, so a VMD-bound or direct-NVMe hardware transcript can report raw CAP/VS/CC/CSTS state instead of only `nvme-ready 0`. Verification passed with Product build plus M1 production-slice gate, `hardware-storage-analysis-fixtures: 67/67`, and `m134-storage-target-fixtures: 16/16`. Final reserves are BIOS `101` sectors and UEFI `721,408` bytes. M161 does not claim a physical MSI pass, a working VMD-backed NVMe controller, successful internal SSD FAT access, or safe internal-disk writes; it makes the next controller-level failure materially more diagnosable.

M160 is `VMD/NVMe bound-path fixture/report hardening`. This is host-side hardening for the M159 scoped bind path, not new kernel behavior. The storage capture parser now recognizes a successful `drs-vmd-nvme-bind` state before enforcing the older deferred-source checks, so a real bound transcript with promoted candidate source `3` is routed to the next NVMe controller stage instead of being rejected as a stale source/deferred mismatch. Fixture coverage now includes bound-source mismatch, bound controller-discovery failure, bound controller-ready failure, and bound Identify failure. M134 reports also surface VMD driver bind state, token, and count so manager-facing handoff reports show whether the scoped bind path actually ran. Verification passed with `hardware-storage-analysis-fixtures: 67/67` and `m134-storage-target-fixtures: 16/16`; Product build plus M1 production-slice gate passed. Final reserves remain BIOS `101` sectors and UEFI `721,696` bytes. M160 does not claim a physical MSI pass, a working VMD-backed NVMe controller, successful internal SSD FAT access, or safe internal-disk writes; it makes the next M159-era hardware transcript harder to misclassify.

M159 is `scoped VMD/NVMe bind/probe handoff`. The UEFI Product storage path now consumes the M158 no-touch VMD child NVMe handoff and can promote a matching deferred VMD nested candidate into the existing regular NVMe probe path under scoped hardware-query authority. The new binder refuses unauthorised callers, validates the deferred VMD source, matching nested BDF, nonzero token, and child NVMe MMIO readiness, then records source `3` (`MMIO64_NVME_CANDIDATE_SOURCE_VMD_NESTED_BOUND`) before the normal NVMe Identify/read/GPT/FAT stages run. New `drs-vmd-nvme-bind` telemetry records `result`, `state`, `flags`, `token`, `count`, `denials`, `unavailable`, and the promoted candidate fields. Host parsing merges the bind line into storage evidence so a real MSI transcript with a successful bind advances to `nvme-controller-discovery`, `nvme-controller-ready`, or `nvme-identify` instead of stopping at `pci-vmd-nested-driver-plan-staged`. Verification passed with Product build plus M1 production-slice gate, `hardware-storage-analysis-fixtures: 63/63`, `m134-storage-target-fixtures: 14/14`, and full Product QEMU verification. Final reserves are BIOS `101` sectors and UEFI `721,696` bytes. This milestone still does not claim a physical MSI pass, a working VMD-backed NVMe controller on hardware, or safe internal-disk writes; it adds the real scoped bind/probe attempt needed for the next hardware capture to reveal the first controller-level failure.

M158 is `no-touch VMD/NVMe driver-plan handoff`. The UEFI Product PCI/storage path now stages a capability-gated, no-touch driver-plan token only when the already-proven VMD nested-child NVMe prerequisites are simultaneously true: a deferred nested register candidate, a matching deferred NVMe candidate, usable nested MMIO readiness, scoped hardware-query authority, no MMIO writes, no commands, and no probe/bind attempt. New `vmd-nested-driver-plan-*` fields are emitted through `hwval`, storage triage, and the brokered PCI proof so the next driver milestone can consume a precise handoff instead of reinterpreting raw VMD/NVMe telemetry. Host analyzers now classify successful evidence as `pci-vmd-nested-driver-plan-staged` with `vmd_handoff.kind = vmd-nested-driver-plan` and `vmd_handoff.stage = driver-plan-staged`; QEMU direct-NVMe runs truthfully report the plan as unavailable because no VMD candidate exists. Fixture coverage passed with `hardware-storage-analysis-fixtures: 63/63` and `m134-storage-target-fixtures: 14/14`; Product build and the M1 production-slice gate passed; targeted UEFI Product storage verification passed with the new fields. Final reserves are BIOS `101` sectors and UEFI `722,112` bytes. The full default QEMU run still has an unrelated BIOS AHCI proof mismatch (`storage 1 ide 1 ahci 0 nvme 0`) before the UEFI storage leg. This milestone does not claim a physical MSI pass, add a VMD driver, bind or probe a VMD-backed NVMe controller, issue VMD/NVMe commands, or perform unsafe storage I/O; it records the exact no-touch contract the future driver implementation must satisfy.

M157 was `VMD/NVMe handoff report surfacing`. The physical hardware intake path now promotes VMD/NVMe handoff state from raw `drs-nvme-triage` telemetry into structured JSON/text/Markdown fields. `tools\analyze-hardware-storage-capture.ps1` emits a `vmd_handoff` object with `kind`, `stage`, nested bind/register status, nested register token, NVMe candidate source/deferred status, candidate BDF, and candidate token. `tools\verify-hardware-storage-evidence.ps1`, `tools\analyze-msi-hardware-capture.ps1`, `tools\classify-m134-storage-target.ps1`, and `tools\report-msi-hardware-capture.ps1` now preserve that object so the manager-facing report can distinguish direct NVMe, VMD candidate/preflight states, and the important `vmd-nested-deferred` / `registration-deferred` boundary without reading raw logs. The existing VMD-deferred M134 fixture now asserts the structured handoff object, proving the report carries `vmd-nested-deferred` and `registration-deferred` when `vmd-nested-register-candidate 1`, `vmd-nested-register-status 2`, `nvme-candidate-source 2`, and `nvme-candidate-deferred 1` appear. Fixture coverage passed with `m134-storage-target-fixtures: 14/14`, `msi-hardware-capture-report-fixtures: 3/3`, and `msi-hardware-handoff-fixtures: 16/16`; Product build and the M1 production-slice gate also passed. Final reserves remain BIOS `101` sectors and UEFI `726,944` bytes. This does not claim a physical MSI pass, add a VMD driver, bind a VMD-backed NVMe controller, touch kernel behavior, or perform unsafe storage I/O; it makes the next physical transcript's VMD storage target explicit and hard to misroute.

M156 is `MSI handoff report contract integration`. The M155 report command is now part of the generated physical handoff contract instead of living only in the hardware checklist. `tools\prepare-hardware-storage-evidence.ps1` writes `expected_hwval.capture_report = tools\report-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry` into the bundle manifest and puts the report command first in `README-HARDWARE-STORAGE.txt`. `tools\verify-msi-hardware-handoff.ps1` now rejects handoff bundles whose manifest or runbook omit that report command, and the M134 fixture bundle generator was updated to the same contract so downstream classifier/report fixtures remain current. Fixture coverage passed with `m134-storage-target-fixtures: 14/14`, `msi-hardware-handoff-fixtures: 16/16`, and `msi-hardware-capture-report-fixtures: 3/3`; Product build and the M1 production-slice gate also passed. The ignored current handoff bundle `dist\m133-msi-hardware-handoff-current` was refreshed with ISO SHA-256 `3f4cb1629e31d6e40d086d04c423f6a9f44ba37e27f85e3bc41b0fe7fc0bce48`, UEFI image SHA-256 `62e652a4d5ac6fa387fa5948db262efdc87abdbc4a56cf7e79f3f9dad93ce443`, `/APPS/DYNLDLIMIT` SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`, and `/APPS/LDLIMIT` SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`; self-verification reports `capture-report: tools\\report-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry`. Final reserves remain BIOS `101` sectors and UEFI `726,944` bytes. This does not claim a physical MSI pass, add a driver, or change kernel behavior; it prevents stale handoff bundles from bypassing the M155 report intake.

M155 is `MSI capture intake report`. The physical MSI capture workflow now has a one-command report wrapper, `tools\report-msi-hardware-capture.ps1`, that runs the strict M134 classifier with the current M153 GUI telemetry requirement, accepts both pass and expected first-failure exits, and writes `msi-hardware-capture-report.json`, `.txt`, and `.md`. The report preserves the target kind, target stage, roadmap target, next target, storage/display-input/dynamic-handoff stages, bundle artifact hashes, reserves, and links to the lower-level classifier, handoff verifier, and combined analyzer JSON. Fixture coverage passed with the seeded `m134-storage-target-fixtures: 14/14` and the new `msi-hardware-capture-report-fixtures: 3/3`, proving a clean storage-ready capture stays pass/M134, a stale GUI transcript routes to `display-input/gui-telemetry-missing` with roadmap `M152`, and a storage-ready but old dynamic path routes to `dynamic-handoff/dynamic-handoff-nvme-unavailable` with roadmap `M83+`. Product build and the M1 production-slice gate also passed. Final reserves are unchanged because this is host-side tooling and documentation: BIOS `101` sectors and UEFI `726,944` bytes. This does not claim a physical MSI pass, add a driver, or change kernel behavior; it makes the next real laptop transcript consumable as a stable handoff report.

M154 is `current MSI handoff bundle refresh`. The physical MSI handoff documentation now points at the M153 contract and explicitly requires the `hwval` `drs-display-readability`, `drs-ui-polish`, `drs-cursor-path`, `drs-gui`, and `drs-nvme-triage` lines before classifying a capture. A fresh self-verified handoff bundle was generated at ignored path `dist\m133-msi-hardware-handoff-current` from commit `38628912`, with `limitlessos-x86_64-m133-handoff.iso` SHA-256 `cb9ee01c7bb32efa3af4bace88207076cf53da8727f25b0a044591d341e72b78`, `limitlessos-x86_64-m133-handoff-uefi.img` SHA-256 `0fc034ad61b0a91f87fbd7407aeb29ec7ddbc87ef9898e81646596b5ac54c39f`, `/APPS/DYNLDLIMIT` SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`, and `/APPS/LDLIMIT` SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`. The packager rebuilt Product media with staged dynamic artifacts, ran the M1 production-slice gate, ran the staged storage gate, and embedded `msi-handoff-verification` output proving the bundle hashes/reserves, runbook, source-2 boot-media expectation, and M153 GUI telemetry requirement. Final reserves remain BIOS `101` sectors and UEFI `726,944` bytes. This does not claim a physical MSI pass; it makes the next physical run ready and unambiguous.

M153 is `MSI handoff GUI telemetry requirement`. The current MSI hardware handoff contract now requires the M152 `hwval` `drs-gui` interaction line whenever a capture is verified through the current handoff/classifier path. `tools\verify-msi-hardware-handoff.ps1` has a new `-RequireGuiInteractionTelemetry` switch that reads the nested display/input analysis, emits dedicated `gui_interaction_*` fields, and fails capture verification when storage/display/dynamic evidence is present but `drs-gui` is missing. `tools\classify-m134-storage-target.ps1` threads the same switch and routes that exact stale-capture case to `display-input/gui-telemetry-missing` with roadmap target `M152`. `tools\prepare-hardware-storage-evidence.ps1` now writes the stricter classifier/verifier commands and the `drs-gui ... drs-gui-right-click 1 ... drs-gui-context-action 1 ... drs-gui-scroll ...` expectation into new MSI handoff bundles. Fixture coverage passed with `msi-hardware-handoff-fixtures: 14/14`, `m134-storage-target-fixtures: 14/14`, and `msi-hardware-analysis-fixtures: 7/7`; Product build, M1 production-slice gate, and full UEFI Product QEMU verification also passed. BIOS reserve remains `101` sectors and UEFI reserve remains `726,944` bytes. This is host-side handoff hardening only: no kernel behavior changed, no new hardware support is claimed, and no physical MSI pass is claimed.

M152 is `hwval GUI interaction telemetry`. The UEFI Product `hwval` command now emits the same existing `drs-gui` interaction proof line that the scaffold verifier already used: right-click, context action, window-manager operations, scroll, Terminal scroll/selection/copy, File Manager actions, Settings actions, input/display/fs capability tokens, focused target window/region, and no-ambient proofs. This closes the physical capture gap left by M151: a user running `hwval` on the MSI laptop now captures `drs-display-readability`, `drs-ui-polish`, `drs-cursor-path`, `drs-gui`, pointer backend status, and NVMe/storage triage in one transcript. Product build, M1 production-slice gate, full QEMU Product verification, direct display/input analysis of `build\qemu-x86_64-uefi-debug.log`, and `hardware-display-input-fixtures: 38/38` passed. Accepted evidence includes the live `hwval` line with `drs-gui-right-click 1`, `drs-gui-context-action 1`, `drs-gui-scroll 2`, `terminal-scroll 1`, `terminal-selection 2`, `input-token 0x494E5054`, `display-token 0x44495350`, and `fs-token 0x46535041`. BIOS reserve remains `101` sectors; UEFI reserve is `726,944` bytes. No new GUI behavior or physical MSI display/input pass is claimed; this milestone makes the next physical capture self-contained for M151 analysis.

M151 is `MSI GUI interaction-stage routing`. The host-side hardware display/input analyzer now consumes the existing `drs-gui` line when present and classifies missing Product interaction proofs after cursor/display readiness: `gui-interactive-unrouted`, `gui-right-click-unrouted`, `gui-context-action-missing`, `gui-scroll-unrouted`, `gui-terminal-scroll-missing`, and `gui-terminal-selection-missing`. Older captures without `drs-gui` remain backward-compatible and still pass `display-input-ready` once display, cursor, and pointer packets are healthy, but their next target now asks for an M151-or-newer interactive desktop capture. The combined MSI analyzer and M134 classifier carry `gui-*` stages through as display/input failures and route them to roadmap target `M151`; legacy broad cursor-hidden captures still route to M149 and precise cursor stages still route to M150. Fixture coverage passed with `hardware-display-input-fixtures: 38/38`, `msi-hardware-analysis-fixtures: 7/7`, and `m134-storage-target-fixtures: 13/13`. Product build, M1 production-slice gate, and full QEMU Product verification passed with BIOS reserve `101` sectors and UEFI reserve `728,352` bytes unchanged. No kernel code changed and no physical MSI display/input pass is claimed; this milestone makes the next laptop capture distinguish "cursor visible" from real right-click, scroll, context-menu, and Terminal selection routing.

M150 is `MSI cursor-stage routing closure`. The host-side MSI handoff path now carries the precise M149 cursor stages through the combined hardware analyzer and the M134 target classifier instead of collapsing them back into the old broad `pointer-moving-cursor-hidden` bucket. `tools\verify-msi-hardware-analysis-fixtures.ps1` now includes a M149-era capture with `drs-cursor-path` and proves the combined analyzer reports `display-input-cursor-draw-not-called`. `tools\verify-m134-storage-target-fixtures.ps1` now includes the same precise cursor failure, asserts the emitted `roadmap_target`, and proves legacy broad cursor-hidden captures still route to completed M149 while precise cursor stages route to M150. Fixture coverage passed with `msi-hardware-analysis-fixtures: 5/5` and `m134-storage-target-fixtures: 12/12`. Product build, M1 production-slice gate, and full QEMU Product verification passed with BIOS reserve `101` sectors and UEFI reserve `728,352` bytes unchanged. No kernel code changed and no physical MSI display/input pass is claimed; this milestone prevents the next real M149+ laptop capture from being misrouted after storage is green.

M149 is `cursor path hardware-display diagnostics`. The UEFI Product `hwval` path now emits a new `drs-cursor-path` proof line next to the existing `drs-display-readability` and `drs-ui-polish` lines. It records cursor surface readiness, framebuffer format support, compositor/direct mode, visibility, draw counts, cursor coordinates/buttons, in-bounds status, cursor rectangle size, saved-underlay state, final drawn state, and a token. The host display/input analyzer remains backward-compatible with older captures, but M149-era captures now split the former broad `pointer-moving-cursor-hidden` symptom into precise stages: `cursor-format-unsupported`, `cursor-surface-not-ready`, `cursor-draw-not-called`, `cursor-out-of-bounds`, and `cursor-draw-not-validated`. Fixture coverage passed with `hardware-display-input-fixtures: 31/31`; the UEFI Product hardware-display gate passed with `drs-cursor-path cursor-path 1 surface-ready 1 format-supported 1 compositor-active 1 compositor-direct 1 visible 1 draws 250 direct-draws 252 x 560 y 420 buttons 0 in-bounds 1 rect-w 12 rect-h 20 saved 1 drawn 1 token 0xD2FC1305`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `728,352` bytes after the first build. No physical MSI display/input certification, new GPU driver, DRM/KMS mode setting, touchpad driver, or new cursor drawing behavior is claimed; this milestone makes the next real laptop capture tell us exactly why packets can move without a visible cursor.

M148 is `NVMe candidate source/deferred handoff telemetry`. The UEFI Product MMIO/NVMe layer now records whether the current NVMe candidate came from the existing direct PCI path or from a deferred VMD nested-child handoff candidate. New fields are emitted through `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI/NVMe scaffold proof: `nvme-candidate-source`, `nvme-candidate-deferred`, `nvme-candidate-bdf`, and `nvme-candidate-token`. Direct QEMU NVMe reports `nvme-candidate-source 1`, `nvme-candidate-deferred 0`, `nvme-candidate-bdf 0xFFFFFFFF`, and a nonzero candidate token; VMD child fixtures report source `2` and deferred `1` while keeping `nvme-found 0`, so no VMD-backed controller probing or storage I/O is claimed. Host capture routing remains backward-compatible with older M147 transcripts and now distinguishes bad MMIO/NVMe source/deferred propagation before the future nested NVMe registration target. Fixture coverage passed with `hardware-storage-analysis-fixtures: 63/63` and `m134-storage-target-fixtures: 11/11`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `728,608` bytes. No physical MSI storage-driver pass, VMD programming, VMD controller reset/enable, nested NVMe registration/bind, RAID driver, or unsafe storage reads/writes through VMD are claimed.

M147 is `VMD nested child NVMe registration-candidate deferral`. The UEFI Product VMD nested scanner now reports the next no-touch boundary after bind readiness: whether a child NVMe behind VMD is a candidate for a future scoped NVMe driver registration, and whether that registration is intentionally deferred because no VMD-backed NVMe handoff exists yet. New fields are emitted through `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof: `vmd-nested-register-candidate`, `vmd-nested-register-status`, and `vmd-nested-register-token`. QEMU, which exposes direct NVMe and no VMD candidate, truthfully reports the registration gate as `0/0/0x00000000`; synthetic hardware fixtures now route child-NVMe VMD captures through `pci-vmd-nested-nvme-register-candidate` and `pci-vmd-nested-nvme-register-deferred`, with clear next-target guidance for the future scoped nested NVMe registration implementation. Fixture coverage passed with `hardware-storage-analysis-fixtures: 63/63` and `m134-storage-target-fixtures: 11/11`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `733,152` bytes. No physical MSI storage-driver pass, VMD programming, VMD controller reset/enable, nested NVMe registration/bind, RAID driver, or unsafe storage reads/writes through VMD are claimed.

M146 is `VMD nested child NVMe bind-readiness gate`. The UEFI Product VMD nested scanner now reports whether a child NVMe device behind VMD has satisfied every precondition for a future VMD-backed NVMe registration, without registering or probing that controller. New fields are emitted through `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof: `vmd-nested-bind-ready`, `vmd-nested-bind-status`, and `vmd-nested-bind-token`. QEMU, which exposes direct NVMe and no VMD candidate, truthfully reports the bind gate as `0/0/0x00000000`; synthetic hardware fixtures now route child-NVMe VMD captures through `pci-vmd-nested-nvme-bind-ready` before the remaining `pci-vmd-nested-nvme-bind` target. Fixture coverage passed with `hardware-storage-analysis-fixtures: 62/62` and `m134-storage-target-fixtures: 10/10`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `733,408` bytes. No physical MSI storage-driver pass, VMD programming, VMD controller reset/enable, VMD NVMe bind, RAID driver, or unsafe storage reads/writes through VMD are claimed.

M145 is `VMD nested child NVMe MMIO preflight`. The UEFI Product VMD nested scanner now turns an observed child NVMe-class device behind VMD into no-touch nested BAR/MMIO planning telemetry before any future bind attempt. New fields are emitted through `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof: `vmd-nested-mmio-low`, `vmd-nested-mmio-high`, `vmd-nested-mmio-span`, `vmd-nested-mmio-flags`, and `vmd-nested-mmio-token`. QEMU, which exposes direct NVMe and no VMD candidate, truthfully reports all nested MMIO fields as `0`; synthetic hardware fixtures now route child-NVMe VMD captures through `pci-vmd-nested-nvme-mmio-base`, `pci-vmd-nested-nvme-mmio-span`, and `pci-vmd-nested-nvme-mmio-flags` before the remaining `pci-vmd-nested-nvme-bind` target. Fixture coverage passed with `hardware-storage-analysis-fixtures: 61/61` and `m134-storage-target-fixtures: 9/9`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `733,664` bytes. No physical MSI storage-driver pass, VMD programming, VMD controller reset/enable, VMD NVMe bind, RAID driver, or unsafe storage reads/writes through VMD are claimed.

M144 is `VMD full first-bus read-only scan`. The UEFI Product VMD nested scanner now remaps a 64 KiB read-only high-half MMIO window across the first full nested PCI config bus instead of checking only the first two devices. The scanner covers one bus, 32 devices, 8 functions each, and 16 read-only remap windows when a usable VMD MMIO candidate exists. New coverage telemetry is emitted through `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof: `vmd-nested-scan-buses`, `vmd-nested-scan-devices`, `vmd-nested-scan-functions`, `vmd-nested-scan-windows`, and `vmd-nested-scan-truncated`. QEMU, which exposes direct NVMe and no VMD candidate, truthfully reports all scan counters as `0`; the hardware fixtures cover the VMD nested no-child and child-NVMe routes with `1/32/256/16/1` coverage. Fixture coverage passed with `hardware-storage-analysis-fixtures: 58/58` and `m134-storage-target-fixtures: 8/8`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `734,016` bytes. No physical MSI storage-driver pass, multi-bus VMD enumeration, VMD MMIO programming, VMD storage binding, RAID driver, or unsafe storage writes are claimed.

M143 is `VMD read-only nested config scan`. The UEFI Product PCI inventory now performs a bounded, read-only VMD child config-window scan when the M141/M142 VMD MMIO preflight reports a usable candidate. The scanner maps only a 64 KiB window, scans the first nested bus for devices 0-1 and functions 0-7, performs no writes, does not program VMD, does not bind NVMe, and does not claim a physical storage pass. New child identity telemetry is emitted through the existing hardware capability path in `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof: `vmd-nested-pci`, `vmd-nested-vendor-device`, `vmd-nested-class`, `vmd-nested-bar0`, and `vmd-nested-bar1`. QEMU, which exposes direct NVMe and no VMD candidate, truthfully reports `vmd-nested-plan 0`, `vmd-nested-enum 0`, `vmd-nested-nvme 0`, `vmd-nested-status 0`, and child BDF `0xFFFFFFFF`; the hardware fixtures cover both no-child and child-NVMe VMD paths. Fixture coverage passed with `hardware-storage-analysis-fixtures: 58/58` and `m134-storage-target-fixtures: 8/8`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `738,528` bytes. No physical MSI storage-driver pass, full VMD bus enumeration, VMD MMIO programming, VMD storage binding, RAID driver, or unsafe storage writes are claimed.

M142 is `VMD nested enumeration boundary telemetry`. The UEFI Product PCI inventory now carries explicit VMD nested-domain boundary fields after the no-touch M141 MMIO preflight: `vmd-nested-plan`, `vmd-nested-enum`, `vmd-nested-nvme`, `vmd-nested-status`, and `vmd-nested-token`. A usable VMD MMIO candidate now advances the storage classifier to `pci-vmd-nested-enumeration` instead of the broader `pci-nvme-hidden-by-vmd`, making the next implementation target the read-only nested PCI-domain enumerator. QEMU, which exposes direct NVMe and no VMD candidate, truthfully reports `vmd-nested-plan 0`, `vmd-nested-enum 0`, `vmd-nested-nvme 0`, and `vmd-nested-status 0`; the hardware fixtures cover the MSI-style VMD path where a candidate with usable MMIO reports `vmd-nested-plan 1` and `vmd-nested-enum 0`. Fixture coverage passed with `hardware-storage-analysis-fixtures: 58/58` and `m134-storage-target-fixtures: 8/8`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `738,880` bytes. No physical MSI storage-driver pass, VMD MMIO mapping/programming, nested PCI-domain enumeration, child NVMe discovery, VMD storage binding, RAID driver, or unsafe storage writes are claimed.

M141 is `VMD MMIO preflight telemetry`. The UEFI Product PCI inventory now extends the M140 VMD candidate lane with no-touch MMIO planning fields: `vmd-mmio-low`, `vmd-mmio-high`, `vmd-mmio-span`, `vmd-mmio-flags`, and `vmd-mmio-token`. The plan decodes the first VMD candidate BAR0/BAR1 into a conservative span hint and explicit safety flags while marking the candidate as safe-no-touch, nested-enumeration-required, and no-driver-bound. `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof all expose the new fields through the existing hardware-inventory capability authority. The storage parser and analyzer now distinguish VMD-specific preflight failures (`pci-vmd-bdf`, `pci-vmd-identity`, `pci-vmd-class-code`, `pci-vmd-bar0`, `pci-vmd-mmio-base`, `pci-vmd-mmio-span`, `pci-vmd-mmio-flags`) before reporting `pci-nvme-hidden-by-vmd`. Fixture coverage passed with `hardware-storage-analysis-fixtures: 55/55` and `m134-storage-target-fixtures: 8/8`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `739,136` bytes. No physical MSI storage-driver pass, VMD MMIO mapping/programming, nested PCI-domain enumeration, RAID driver, or unsafe storage writes are claimed.

M140 is `VMD-class storage target refinement`. The UEFI Product PCI inventory now separates Intel VMD-class candidates from the broader Intel system-class bucket. `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof now expose `pci-vmd`, `vmd-pci`, `vmd-vendor-device`, `vmd-class`, `vmd-bar0`, and `vmd-bar1`. The storage capture parser now prioritizes `pci-nvme-hidden-by-vmd` before `pci-nvme-hidden-by-intel-system`, and the analyzer playbook points the next implementation at read-only VMD/nested PCI-domain enumeration before regular NVMe binding. Fixture coverage passed with `hardware-storage-analysis-fixtures: 48/48` and `m134-storage-target-fixtures: 7/7`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `743,520` bytes. No physical MSI storage-driver pass, VMD MMIO programming, nested enumeration, RAID driver, or unsafe storage writes are claimed.

M139 is `VMD/RST storage visibility classification`. The UEFI Product PCI inventory now records RAID-class storage controllers, non-AHCI/non-NVMe storage controllers, and Intel system-class controller candidates alongside the existing direct NVMe identity. `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof now expose `pci-raid`, `pci-other-storage`, `pci-intel-system`, first other-storage BDF/vendor/class/BAR0/BAR1, and first Intel system-class BDF/vendor/class/BAR0/BAR1. The storage capture parser now distinguishes `pci-nvme-hidden-by-raid`, `pci-nvme-hidden-by-intel-system`, and `pci-nvme-other-storage` before the generic `pci-nvme-class` stage, with analyzer playbooks pointing at the exact next driver boundary. Fixture coverage passed with `hardware-storage-analysis-fixtures: 47/47` and `m134-storage-target-fixtures: 6/6`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `743,776` bytes. No physical MSI storage-driver pass, VMD driver, RAID driver, or unsafe controller access is claimed; this milestone makes the next real capture identify when direct NVMe is hidden behind a bridge or firmware storage mode.

M138 is `NVMe PCI capture-stage classification`. The host-side hardware storage parser now splits the former generic early `nvme-controller-discovery` bucket into precise PCI/NVMe identity stages: `pci-storage-discovery`, `pci-nvme-class`, `pci-nvme-bdf`, `pci-nvme-identity`, `pci-nvme-class-code`, `pci-nvme-bar0`, `pci-nvme-mmio-base`, `pci-nvme-mmio-span`, and `pci-nvme-mmio-flags`. `tools\analyze-hardware-storage-capture.ps1` has first-check playbooks, required telemetry fields, kernel-file pointers, and acceptance signals for each new stage, and `tools\verify-m134-storage-target-fixtures.ps1` now proves an M138-specific `pci-nvme-class` failure routes through the full M134 handoff classifier as a storage target. Fixture coverage passed with `hardware-storage-analysis-fixtures: 44/44` and `m134-storage-target-fixtures: 5/5`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and UEFI reserve `744,672` bytes. No physical MSI storage-driver pass is claimed; this milestone makes the next real capture point at a narrower first failing driver boundary.

M137 is `NVMe PCI identity telemetry`. The UEFI Product hardware validation and storage triage paths now expose the first discovered NVMe controller's PCI identity and MMIO planning fields before the controller/FAT stages: PCI storage count, NVMe count, first BDF, vendor/device, class/prog-if/revision, BAR0/BAR1, MMIO base low/high, span hint, MMIO flags, and MMIO token. The brokered scaffold proof line now carries the same authorized NVMe fields, and `tools\verify-qemu.ps1` requires those fields to be non-sentinel values on the UEFI Product path. `tools\analyze-hardware-storage-capture.ps1` records the new fields in `key_fields` and requires them in the `nvme-controller-discovery` diagnostic plan so the next real MSI transcript can distinguish PCI discovery failure from later NVMe readiness, GPT, FAT, or staged-artifact failures. Fixture coverage passed with `hardware-storage-analysis-fixtures: 35/35` and `m134-storage-target-fixtures: 4/4`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `744,672` bytes. No physical MSI storage-driver pass is claimed; this milestone makes the next hardware capture more diagnostic.

M136 is `storage diagnostic playbooks`. The physical storage analyzer now emits a checked `diagnostic` object for every M112/M114 storage classification stage, including the failing component, required telemetry fields, first code area to inspect, relevant kernel files, and the acceptance signal that proves the stage is fixed. The Markdown report now includes a `Diagnostic Plan` section so a real MSI `hwval` transcript can be handed directly to implementation without manually reverse-engineering the raw `drs-nvme-triage` line. Fixture coverage was strengthened so `tools\verify-hardware-storage-analysis-fixtures.ps1` requires a populated diagnostic plan for all 35 synthetic stages, and the higher-level M134 classifier still passes `m134-storage-target-fixtures: 4/4` with the richer report schema. No kernel code changed and no physical storage-driver pass is claimed. Accepted evidence preserves BIOS reserve `101` sectors and UEFI reserve `749,408` bytes.

M135 is `M134 classifier handoff integration`. The M133 physical MSI handoff bundle now carries the M134 storage target classifier in its generated manifest and runbook, so the next hardware transcript is routed through `tools\classify-m134-storage-target.ps1` before older lower-level analyzers. `tools\verify-msi-hardware-handoff.ps1` now rejects bundles whose manifest omits `tools\classify-m134-storage-target.ps1 -RequireStagedDynamicArtifacts` or whose runbook does not instruct the tester to use the classifier. Fixture coverage was strengthened to `msi-hardware-handoff-fixtures: 12/12`, adding a negative `missing-storage-target-classifier` case, and `m134-storage-target-fixtures: 4/4` still passes under the stricter handoff contract. No kernel code changed and no physical MSI driver pass is claimed. The accepted verifier evidence preserves BIOS reserve `101` sectors and UEFI reserve `749,408` bytes.

M134 is `storage hardware target classification`. Because no fresh physical MSI transcript is present in the workspace, M134 does not claim a new NVMe controller pass or physical storage certification. It adds the missing host-side decision point for the M134-M140 storage hardware breadth phase: `tools\classify-m134-storage-target.ps1` wraps the current M133 handoff verifier, requires a real capture, preserves the existing storage/display/input/dynamic-handoff analysis, and emits one explicit `target_kind`, `target_stage`, `roadmap_target`, and `next_target`. The classifier distinguishes a true storage target such as `storage nvme-controller-discovery` from post-storage failures such as `display-input pointer-moving-cursor-hidden` or `dynamic-handoff dynamic-handoff-nvme-unavailable`, preventing speculative NVMe driver edits when the transcript does not actually point at storage. Fixture coverage passed with `m134-storage-target-fixtures: 4/4`, covering first storage failure, display/input failure after `storage-ready`, dynamic handoff failure after storage/display readiness, and a fully green `storage-ready` capture. No kernel code changed; BIOS remains at `101` sectors and the fixture evidence records UEFI reserve `749,408` bytes.

M133 is `MSI hardware capture closure`. The current physical MSI handoff path has been refreshed from the M132 baseline instead of relying on the older M121 bundle shape. `tools\prepare-hardware-storage-evidence.ps1` now emits M133-named evidence media (`limitlessos-x86_64-m133-handoff.iso` and `limitlessos-x86_64-m133-handoff-uefi.img`), the handoff verifier now rejects stale M121 manifests/ISO names, and `docs\hardware\msi-cyborg-15-a13ve.md` points the next real laptop capture at the current M133 package. A real physical transcript is still required before any MSI device-driver pass is claimed; this milestone closes the host-side handoff and stale-evidence risk so the next hardware run can be classified without ambiguity. The generated bundle `dist\m133-msi-hardware-handoff-current` records ISO SHA-256 `29a2b817675c2e03ba24688870167d8fd9ea0c48a11bc177bb8ae334baadcf04`, UEFI image SHA-256 `f37216cce44bc358e6065926238c753cd3ff8257fcd651a01839f466aa6d4a23`, `/APPS/DYNLDLIMIT` SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`, and `/APPS/LDLIMIT` SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`. Verification passed with `msi-hardware-handoff-fixtures: 11/11`, `handoff-pass: True`, `source2-required: 2`, `boot-media-verifier-ran: True`, Product build/M1 gate pass, and boot-media handoff telemetry proving `linux: using UEFI boot-media staged file` plus `drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage elf ... boot-media-read-error 0 boot-media-read-bytes 7`. Final reserves: BIOS `101` sectors, UEFI `749,408` bytes.

M132 is `Window manager usability`. The UEFI Product window manager now supports real focus/z-order promotion, drag, resize from a bottom-right grip, minimize, taskbar restore, close, launcher-opened apps, right-click context-menu action routing, and stronger scroll/key routing through the same brokered input/display authority used by the rest of Product. Minimized windows are skipped by hit testing and drawing while remaining available through taskbar restore; Terminal resize reconfigures the console viewport instead of leaving stale geometry. The Product QEMU verifier now drives the real GUI path with launcher, second Terminal creation, drag, resize, minimize/restore, Settings/File Manager/Installer/Assistant opening, right-click context action, Terminal scroll/selection/copy, and File Manager/Settings mutation proofs. Acceptance telemetry includes `drs-gui-close-completed 1 drs-gui-taskbar-focus 1 drs-gui-right-click 1 drs-gui-context-action 1 wm-resize 2 wm-minimize 1 wm-restore 1 wm-zorder 26 terminal-actions 3 fileman-actions 4 settings-actions 4`, with `drs-gui-no-ambient-input 1 drs-gui-no-ambient-display 1 drs-gui-no-ambient-fs 1`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Final reserves: BIOS `101` sectors, UEFI `749,408` bytes.

M131 is `Login/session polish`. The UEFI Product login and session surfaces now use the same deliberate visual language as the rest of the Product GUI instead of presenting a flat blocking prompt: first-run setup, login, recovery, denied/unavailable, and bounded input-wait states are drawn as explicit session cards with clear authority boundaries, visible safe-path text, and no silent wait. The auth path now records bounded input waits, hardware fallback, hardware recovery, and lock-unavailable outcomes; the display path records visible login/setup/recovery/wait/safe-path presentations and the last classified login state. The BIOS path was intentionally kept at its previous compact geometry and excludes the new Product-only telemetry/logging surface. The Product QEMU verifier now requires the expanded `drs-login` proof shape, including `input-waits 1 hardware-fallbacks 1 hardware-recovery 1 login-present 3 login-setup-visible 2 login-recovery-visible 1 login-wait-visible 2 login-safe-path 3 login-last-state 7`, while preserving the existing authenticated local session and brokered input/display proof. Product build, M1 production-slice gate, and full QEMU Product verification passed. Final reserves: BIOS `101` sectors, UEFI `749,568` bytes.

M130 is `Terminal quality pass`. The UEFI Product Terminal now renders the real brokered console replay as a scrollable terminal surface instead of a fixed flat text dump. Wheel input scrolls the bounded console replay buffer, later live console output snaps the view back to tail, drag-select in the Terminal content area records a bounded selection, release copies bytes from the real replay buffer into a fixed internal selection buffer, and the Terminal draws a cursor/selection/status overlay through the compositor. No heap or hosted UI framework was added; the implementation is Product UEFI-only and keeps the existing brokered shell/console path. The Product QEMU verifier now injects a real QMP wheel event plus drag-selection over the Terminal, keeps the M128 File Manager mutation proof stable with kernel-matched button coordinates, and requires `terminal-actions 3 terminal-scroll 1 terminal-scroll-offset 0 terminal-selection 1 terminal-copy 1 terminal-copied-bytes 64 terminal-cursor 31` alongside `fileman-write 1 fileman-mkdir 1 fileman-edit 3 fileman-edit-commit 1`, `settings-actions 4`, `nvme rw mutation denials: 0`, and `nvme rw error: 0`. Product build, M1 production-slice gate, and full QEMU Product verification passed. Final reserves: BIOS `101` sectors, UEFI `753,888` bytes.

M129 is `Settings real workflows`. The UEFI Product Settings app now has a real fixed-format `/SETTINGS.CFG` backend and real `/DIAG.TXT` diagnostics export through the scoped NVMe FAT shell authority. The first implemented controls are display scale, theme selection, pointer speed policy, keyboard repeat policy, diagnostics export, and session lock; storage, network, installer, identity, account, and package rows remain truthful status/read-only surfaces until their backend authority expands. Pointer speed is wired into the live mouse delta path, while the verifier leaves it at the default during automated proof so later absolute mouse injection remains stable. The Product QEMU verifier now clicks Settings rows and requires `settings-actions 4 settings-load 0 settings-save 3 settings-save-denial 0 settings-export 1 settings-export-denial 0 settings-theme 1 settings-pointer 2 settings-keyrepeat 0` alongside the existing File Manager proof. The strengthened Product QEMU gate passed with `fileman-write 1`, `fileman-mkdir 1`, `fileman-edit 1`, `fileman-edit-commit 1`, `nvme rw mutation denials: 0`, and `nvme rw error: 0`. Final reserves: BIOS `101` sectors, UEFI `754,016` bytes.

M128 is `File Manager real workflows`. The UEFI Product File Manager now keeps a real FAT directory viewport cursor instead of being trapped in the first four entries, resets that cursor after path/mutation changes, and lets directory-capable backend operations reach directories instead of rejecting them in the UI. Directory copy and recursive delete are serviced by the real NVMe FAT backend; rename/move remain truthful file-only operations because the backend still enforces file moves. The Product QEMU verifier now performs real File Manager GUI clicks for `New Note` and `Folder`, commits the folder edit through keyboard input, and requires `drs-gui ... fileman-actions 2 ... fileman-write 1 ... fileman-mkdir 1 ... fileman-edit 1 fileman-edit-commit 1` before passing. The same run ended with `nvme rw mutation denials: 0`, `nvme rw error: 0`, `drs-nvme-triage ... fat-located 1 ... rw-error 0`, and the strengthened M1/Product QEMU gate passed. Final reserves: BIOS `101` sectors, UEFI `758,304` bytes.

M127 is `FAT backend completion`. Product UEFI now supports bounded recursive FAT directory copy through the same scoped shell/NVMe authority as the existing write, append, copy, rename, move, and recursive delete paths. Directory copies create fresh FAT directory clusters, copy each file into a fresh cluster chain, walk bounded child entries, deny root/self/descendant copy traps, preserve visible long-name entries through the existing LFN creation path, and expose both startup proof and user-shell proof through `hwval`. The M127 QEMU Product proof created a nested tree, copied it with `copy rsrc rdst`, listed `rdst` and `rdst/subdir`, read back `nested-data` and `long-data` from the copied tree, and completed `linux /APPS/BUSYBOX echo fat-recursive-copy-proof` with `exit 0`, `low-compat 0`, `syscall-root-repair 0`, and `page-faults 0`. Final reserves: BIOS `101` sectors, UEFI `758,304` bytes.

M125 is `MSI dynamic handoff capture classification`. `tools\verify-msi-hardware-handoff.ps1` now parses a real `linux /APPS/DYNLDLIMIT` transcript when `-CapturePath` is supplied and records `dynamic-handoff-*` fields alongside the existing storage/display analysis. It distinguishes source-2 boot-media handoff success, dynamic runtime failure after source 2, clean dynamic exit 0, old `NVMe FAT unavailable` behavior, missing `drs-realbin` telemetry, wrong source, and source-2 boot-media read failure. The handoff fixture suite now covers 11 cases, including four capture-side dynamic outcomes. No kernel code changed; BIOS remains at 101 reserve sectors and UEFI reserve remains 788,512 bytes.

M124 is `self-verifying MSI handoff packaging`. `tools\prepare-hardware-storage-evidence.ps1` now runs `tools\verify-msi-hardware-handoff.ps1` against each generated bundle by default and stores the verifier transcript/output directory inside the evidence package. The generated runbook also tells testers to use the handoff verifier with `-CapturePath` before reading the combined analyzer output directly. A fast `-SkipBuild -SkipQemuGate` run proved the new path on current artifacts with `msi-hardware-handoff: verified`, source-2 required, BIOS reserve 101 sectors, and UEFI reserve 788,512 bytes. No kernel code changed.

M123 is `MSI hardware handoff verifier fixture coverage`. The repo now has `tools\verify-msi-hardware-handoff-fixtures.ps1`, a host-side regression suite for the M122 handoff verifier. It synthesizes valid hash/reserve/BOOTMAN evidence bundles, proves a correct M121 handoff passes, and proves stale or malformed handoffs are rejected for old milestone labels, storage-only analyzer selection, missing source-2 requirement, stale M113 ISO naming, missing `linux /APPS/DYNLDLIMIT` runbook command, and missing source-2 runbook telemetry. No kernel code changed; BIOS remains at 101 reserve sectors and UEFI reserve remains 788,512 bytes.

M122 is `MSI hardware handoff verifier`. The repo now has `tools\verify-msi-hardware-handoff.ps1`, a host-side verifier that wraps the storage evidence verifier and then enforces the M121-specific handoff contract: manifest milestone/purpose, M121 ISO/UEFI image names, `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` paths, combined MSI analyzer command, source-2 boot-media expectation, and runbook instructions to run both `hwval` and `linux /APPS/DYNLDLIMIT`. It verified the current M121 bundle with `source2 required: 2`, BIOS reserve 101 sectors, and UEFI reserve 788,512 bytes. No kernel code changed.

M121 is `MSI hardware handoff bundle refresh`. The physical-laptop handoff packager now builds a current `dist\m121-msi-hardware-handoff-*` bundle instead of the older M113 storage-only bundle shape. The runbook tells the tester to run both `hwval` and `linux /APPS/DYNLDLIMIT`, then analyze the full transcript with `tools\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts`. The generated bundle keeps the real `DYNLDLIMIT` and `LDLIMIT` artifacts, validates with `tools\verify-hardware-storage-evidence.ps1`, and carries the M120 boot-media source-2 expectation so a hardware `NVMe FAT unavailable` result is no longer misread as the first launch dependency. No kernel code changed; BIOS remains at 101 reserve sectors and UEFI reserve remains 788,512 bytes.

M120 is `boot-media Linux handoff verification`. The existing `tools\verify-boot-media-linux-handoff.ps1` verifier was rerun and now closes the stale NVMe-only interpretation in the MSI runbook: when `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` are staged into the UEFI boot FAT image, the shell selects the UEFI boot-media source before requiring NVMe FAT. The proof observes the UEFI loader copying both staged files into `boot_info`, the shell printing `linux: using UEFI boot-media staged file`, and `drs-realbin-fail ... source 2 ... boot-media-read-error 0 ... boot-media-read-bytes 7` for the intentionally invalid probe payload. No kernel code changed; BIOS remains at 101 reserve sectors and UEFI reserve remains 788,512 bytes.

M119 is `MSI hardware capture analysis fixture coverage`. The repo now has `tools\verify-msi-hardware-analysis-fixtures.ps1`, a self-contained host-side regression suite for the combined M118 analyzer. It synthesizes a valid evidence bundle, fabricates controlled `hwval` transcripts, and proves all-ready, storage-first, display-after-storage, and missing-telemetry priority behavior. No kernel code changed; BIOS remains at 101 reserve sectors and UEFI reserve remains 788,512 bytes.

M118 is `MSI hardware capture analysis`. The repo now has `tools\analyze-msi-hardware-capture.ps1`, a single host-side intake command that verifies the M113 storage evidence bundle, runs the M115/M114 storage capture path, runs the M117 display/input analyzer on the same transcript, and emits JSON/text/Markdown with one combined pass/stage/next-target result. No kernel code changed; BIOS remains at 101 reserve sectors and UEFI reserve remains 788,512 bytes.

M117 is `physical display/input capture analysis`. The repo now has `tools\analyze-hardware-display-input-capture.ps1` and `tools\verify-hardware-display-input-fixtures.ps1`, host-side tools that classify real `hwval` display/UI/cursor/pointer telemetry. They distinguish unreadable framebuffer geometry, missing UI/compositor pieces, hidden cursor despite mouse packets, xHCI/PS2/I2C pointer backend failures, and the final `display-input-ready` pass. No kernel code changes were needed; BIOS remains at 101 reserve sectors and UEFI reserve remains 788,512 bytes.

M116 is `physical hardware storage analysis fixture coverage`. The repo now has `tools\verify-hardware-storage-analysis-fixtures.ps1`, a host-side regression suite that fabricates one tiny capture for every M112/M114 storage stage and proves the analyzer reports the expected stage and exit code. It covers missing telemetry, legacy `drs-realbin-unavailable`, every NVMe/GPT/FAT/capability/`/APPS`/staged-artifact failure, and the final `storage-ready` pass.

M115 is `physical hardware storage evidence verification`. The repo now has `tools\verify-hardware-storage-evidence.ps1`, a host-side verifier that validates an M113 evidence bundle before hardware use, checks ISO/UEFI/app/interpreter byte counts and SHA-256s against `hardware-storage-evidence-manifest.json`, checks the staged `BOOTMAN.TXT` contract, checks reserves against the size map, and optionally runs the M114 analyzer on a captured transcript.

M114 is `physical hardware storage capture analysis`. The repo now has `tools\analyze-hardware-storage-capture.ps1`, a host-side intake wrapper that runs the M112 parser, preserves failing classifications as useful diagnostics, and writes JSON/text/Markdown reports with the first failing storage stage, key triage fields, and the next kernel/driver target. It can read expected dynamic artifact sizes from an M113 evidence manifest.

M113 is `physical hardware storage evidence bundle`. The repo now has `tools\prepare-hardware-storage-evidence.ps1`, a host-side packager that prepares a staged UEFI/ISO evidence directory for the MSI laptop storage run. It validates the M111 boot/NVMe dynamic artifact contract in `BOOTMAN.TXT`, optionally reruns the QEMU staged storage gate, copies the ISO, UEFI image, `BOOTMAN.TXT`, size map, `/APPS/DYNLDLIMIT`, and `/APPS/LDLIMIT` into a timestamped ignored `dist\m113-hardware-storage-*` bundle, and writes both JSON/text manifests plus a runbook.

M112 is `physical hardware storage capture parser`. The repo now has `tools\parse-hardware-storage-capture.ps1`, a host-side parser for real laptop `hwval` transcripts. It reads the `drs-nvme-triage` line, emits JSON, and classifies the first failing stage across NVMe discovery/readiness/Identify/IO queue/read status, GPT, FAT32 VBR/BPB/mount, scoped capability delegation, `/APPS` visibility, and M111 staged dynamic artifact match. It also recognizes the older `drs-realbin-unavailable` photo-era line as legacy/insufficient evidence and tells the tester to rerun `hwval` on an M111-staged image.

M111 is `boot/NVMe staged dynamic artifact verification`. The UEFI Product build can now be produced with `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` staged into the UEFI boot FAT image and recorded in `BOOTMAN.TXT` with expected paths, byte counts, and SHA-256s. The M111 verifier stages the same two files into the NVMe GPT FAT `/APPS` directory, boots the system, and requires `hwval` to prove the loader-copied boot-media byte counts match the NVMe FAT stat results with `stage-match 1`, `dynldlimit-match 1`, and `ldlimit-match 1`. This closes the ambiguity behind the physical-laptop `linux /apps/dynldlimit` symptom: the next hardware run can prove whether the artifact was staged correctly before diagnosing controller or FAT availability.

M110 is `NVMe/FAT hardware storage triage`. The UEFI Product `hwval` path now emits a compact `drs-nvme-triage` line that separates NVMe controller discovery, controller readiness, Identify, IO queue creation, read completion/status, GPT/VBR discovery, FAT BPB/location, shell read-write capability, `/APPS` directory visibility, first `/APPS` dirent visibility, and staged artifact presence for `/APPS/BUSYBOX`, `/APPS/DYNLDLIMIT`, and `/APPS/LDLIMIT`. This is the hardware-facing diagnostic needed for the real laptop symptom where `linux /apps/dynldlimit` returned `NVME FAT UNAVAILABLE`: hardware runs can now tell whether the failure is controller discovery, namespace/IO, GPT/VBR, FAT mount, scoped shell authority, `/APPS` absence, or missing staged dynamic artifacts.

M109 is `Product visual polish direct compositor foundation`. The UEFI Product display path keeps the compositor logically active in a direct framebuffer mode when the full back buffer cannot be allocated inside the current handoff window. That lets the existing font, window manager, desktop, taskbar, launcher, and visible cursor paths draw on QEMU/VirtualBox-style GOP framebuffers instead of falling back to emergency text-only presentation. The hardware display gate records `drs-ui-polish` with compositor mode, font/window/desktop/taskbar/window counts, cursor visibility, and a style token. BIOS remains at 101 reserve sectors.

M108 is `visible cursor fallback and bounded login recovery`. The UEFI Product display path draws the mouse cursor directly into the physical framebuffer when pointer packets are moving but the compositor/back-buffer path is not active. This addresses the VirtualBox symptom where PS/2 mouse packet counts and coordinates changed but no cursor was visible. The UEFI Product login gate also has a bounded local-console recovery path so boot reaches the shell when no keyboard line is entered. The hardware display gate records cursor visibility, total cursor draws, and direct framebuffer cursor draws through `hwval`.

M105 is `dynamic pipe close/error semantics`. The UEFI Product Linux persona launcher runs `/APPS/DYNPIPECLOSE`, a dynamic ET_EXEC smoke through the same bounded supported-interpreter path proven by M83-M104. The run proves a dynamic process can block in `read(pipe)`, be woken by the last writer closing and replay the original read as EOF, install a SIGPIPE handler with `rt_sigaction`, write to a pipe after all read ends are closed, receive SIGPIPE, return through `rt_sigreturn`, observe the original `-EPIPE` result, and clean up the fork child, both process roots, and all pipe objects. Broader dynamic linking remains intentionally narrow: this is still a fixed supported-interpreter handoff with bounded relocations and the in-tree libc shim, not arbitrary shared-library loading.

M62 through M66 are also accepted after the older M61 path-normalization chain: M62 removed the low-identity compatibility mapping from Linux persona roots, M63 added the signal foundation for SIGPIPE/SIGCHLD and `rt_sigreturn`, M64 added musl pthread-style `clone(56)` threading, M65 fixed contended futex wakeups, and M66 expanded static thread/process pools while proving per-thread TLS with eight worker threads.

M67 acceptance evidence:

- command: `linux /APPS/MMAPSMOKE`
- staged artifact: `/APPS/MMAPSMOKE`, SHA-256 `595D27327DCC41A511F4170AD60909C8BBC62070AEEC581A365C27191C0EF3B2`, static musl ET_EXEC at `0x52000000`, no `PT_INTERP`
- implementation scope: UEFI-only bounded private file mapping in `linux_abi64_sys_mmap`; no dynamic allocation, no BIOS path expansion, no new mmap flags beyond `MAP_PRIVATE` plus optional `MAP_FIXED`
- final reserves after the M67 build: UEFI reserve 823,776 bytes; BIOS reserve 101 sectors
- proof telemetry excerpt: `mmap 1 mmap-bytes 4096 mmap-denial 0 mmap-file 1 mmap-file-bytes 27 mmap-file-denial 0 mmap-last-error 0 mmap-last-flags 0x0000000000000002 mmap-last-length 0x0000000000001000 read 1 read-bytes 27 write 25 write-bytes 109 low-compat 0 syscall-root-repair 0 page-faults 0 pml4-pool-used-final 0 root-pool-used-final 0 exit 0 cleanup 1`

M68 acceptance evidence:

- trace before fix: `linux /APPS/MMAP2` denied an 8192-byte file mapping with `mmap 0 mmap-denial 1 mmap-file 0 mmap-file-denial 1 mmap-last-error 22 mmap-last-length 0x0000000000002000`
- command after fix: `linux /APPS/MMAP2`
- staged artifacts: `/APPS/MMAP2` SHA-256 `5AB9EF11E16B268B119BACF6EBA2F5544931FF6BF9F9E1B1DE68743C7173E4D1`; `/APPS/MMAPDATA` SHA-256 `1F5A16C4456F34C5459E4E66D8650D4DBB6B298810D0C21CEF6571DB21D69C81`
- implementation scope: UEFI-only cap lift from 4096 bytes to 65536 bytes for the existing eager private file mapping path; no dynamic allocation, no BIOS path expansion, no `MAP_SHARED`, no writeback, no lazy paging
- final reserves after the M68 build: UEFI reserve 823,776 bytes; BIOS reserve 101 sectors
- proof telemetry excerpt: `mmap 2 mmap-bytes 12288 mmap-denial 0 mmap-file 2 mmap-file-bytes 12288 mmap-file-denial 0 mmap-last-error 0 mmap-last-flags 0x0000000000000002 mmap-last-length 0x0000000000001000 vfs-nvme-reads 2 vfs-nvme-bytes 8192 write 4 write-bytes 73 console-bytes 73 low-compat 0 syscall-root-repair 0 page-faults 0 pml4-pool-used-final 0 root-pool-used-final 0 exit 0 cleanup 1`

M69 acceptance evidence:

- trace before fix: `linux /APPS/MMAPWIN` mapped only 4096 bytes at offset 65536 inside a 131072-byte file, but was denied before storage access with `mmap 0 mmap-denial 1 mmap-file 0 mmap-file-denial 1 mmap-last-error 22 mmap-last-length 0x0000000000001000 vfs-nvme-reads 0`
- command after fix: `linux /APPS/MMAPWIN`
- staged artifacts: `/APPS/MMAPWIN` SHA-256 `8DBBA2981B7A9084C80556EB66193C4C35D59C4211F7F16588E4558A5884C28E`; `/APPS/BIGDATA` SHA-256 `37EF3D17D641BFF7C3F5F8A525E6B5EC7C04290E5A850B760F13F8AC6826C503`
- implementation scope: UEFI-only MMIO FAT range reader, Linux VFS range wrapper, and mmap page-window population; old whole-file readers remain intact; BIOS code is excluded under `LIMITLESS_X64_UEFI_KERNEL`
- final reserves after the M69 build: UEFI reserve 819,680 bytes; BIOS reserve 101 sectors
- proof telemetry excerpt: `mmap 1 mmap-bytes 4096 mmap-denial 0 mmap-file 1 mmap-file-bytes 4096 mmap-file-denial 0 mmap-last-error 0 mmap-last-flags 0x0000000000000002 mmap-last-length 0x0000000000001000 vfs-nvme-reads 1 vfs-nvme-bytes 4096 write 3 write-bytes 54 console-bytes 54 low-compat 0 syscall-root-repair 0 page-faults 0 pml4-pool-used-final 0 root-pool-used-final 0 exit 0 cleanup 1`
- M68 regression after the windowed-reader change still passes with `linux /APPS/MMAP2`: `mmap 2 mmap-bytes 12288 mmap-file 2 mmap-file-bytes 12288 mmap-file-denial 0 vfs-nvme-reads 3 page-faults 0 exit 0`

M70 acceptance evidence:

- command: `linux /APPS/DYNSMOKE`
- staged artifact: `/APPS/DYNSMOKE`, SHA-256 `1D9949F783D76F5CA6755D59B04C1E289C90ABF47DE55F5EA3E5E1883EC7A3E3`, external musl-cross ET_EXEC linked at `0x52000000`
- ELF metadata: `readelf -h -l -d` reports `Type: EXEC`, entry `0x520010a1`, four `PT_LOAD` segments, one `PT_DYNAMIC`, no `PT_INTERP` from this musl-cross toolchain, and one `DT_NEEDED` entry for `libc-x64.so`
- implementation scope: telemetry only; no dynamic linker, no relocations, no interpreter handoff, no process/PML4 allocation for denied dynamic inputs
- final reserves after the M70 build: UEFI reserve 819,680 bytes; BIOS reserve 101 sectors
- proof telemetry: `drs-realbin-fail path /APPS/DYNSMOKE stage static code 8 pid 4294967295 elf-type 2 elf-load 4 elf-interp 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 nvme-read-error 0 nvme-read-bytes 19600 nvme-read-capacity 4194304 nvme-read-size 19600`

M71 acceptance evidence:

- command: `linux /APPS/DYNINTERP`
- staged artifact: `/APPS/DYNINTERP`, SHA-256 `50B977B354FE0E7F776D4A4D3FA475246176D5B6077B34216C39B1C4D3012787`, external musl-cross ET_EXEC linked at `0x52000000` with `-fsys-dyn-linker`
- ELF metadata: `readelf -h -l -d` reports `Type: EXEC`, entry `0x52001071`, four `PT_LOAD` segments, one `PT_INTERP` requesting `/lib/ld-musl-x86_64.so.1`, one `PT_DYNAMIC`, and one `DT_NEEDED` entry for `libc-x64.so`
- implementation scope: telemetry only; the launcher records interpreter byte length, FNV-1a checksum, and supported-interpreter status before denying the dynamic input
- final reserves after the M71 build: UEFI reserve 819,680 bytes; BIOS reserve 101 sectors
- proof telemetry: `drs-realbin-fail path /APPS/DYNINTERP stage static code 8 pid 4294967295 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 24 interp-checksum 0x7E2FBF7B interp-supported 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680`

M72 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path staging only; supported interpreter path metadata maps to the staged `/APPS/LDLIMIT` backend, reads that file through scoped NVMe FAT authority, and parses interpreter ELF type/load/interp/dynamic counts before denying the original dynamic app
- final reserves after the M72 build: UEFI reserve 819,680 bytes; BIOS reserve 101 sectors
- proof telemetry: `drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 4294967295 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1`

M73 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path mapping only; the launcher allocates a process root, maps app and interpreter `PT_LOAD` segments with existing ELF/VMA helpers, reports mapped regions/pages, and immediately releases VMA/root/process state before returning the same dynamic-input denial
- final reserves after the M73 build: UEFI reserve 815,584 bytes; BIOS reserve 101 sectors
- proof telemetry: `drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-supported 1 interp-file-read 1 interp-file-elf 1 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-missing 1`

M74 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- `readelf -d -r` for `DYNLDLIMIT` reports `DT_RELA` with four `Elf64_Rela` entries, `DT_JMPREL` with two PLT RELA entries, `DT_RELAENT` 24, `DT_PLTREL` RELA, first `.rela.dyn` target `0x52003fc8` type `R_X86_64_GLOB_DAT` (6), and first `.rela.plt` target `0x52004000` type `R_X86_64_JUMP_SLOT` (7)
- implementation scope: denial-path relocation metadata only; the launcher translates dynamic-table virtual addresses back into file offsets and records counts/first entries without applying relocations or writing GOT/PLT state
- final reserves after the M74 build: UEFI reserve 815,584 bytes; BIOS reserve 101 sectors
- proof telemetry: `drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-supported 1 interp-file-read 1 interp-file-elf 1 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-missing 1`

M75 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- `readelf -d -r --dyn-syms` for `DYNLDLIMIT` reports `DT_SYMTAB` at `0x52000388`, `DT_STRTAB` at `0x52000460`, `DT_STRSZ` 149, `DT_SYMENT` 24, first needed library `libc-x64.so`, first `.rela.dyn` symbol index 2 name `__deregister_frame_info`, and first `.rela.plt` symbol index 1 name `write`
- implementation scope: denial-path symbol metadata only; the launcher validates the referenced `Elf64_Sym` entries and bounded `.dynstr` strings, records names/lengths/checksums, and still performs no binding or relocation writes
- final reserves after the M75 build: UEFI reserve 811,488 bytes; BIOS reserve 101 sectors
- proof telemetry: `dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-reloc-error 0 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M76 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path support classification only; the launcher walks every bounded relocation entry, resolves the referenced symbol name through `.dynsym`/`.dynstr`, checks the fixed Limitless libc and interpreter export registries, and still performs no symbol binding or relocation writes
- final reserves after the M76 build: UEFI reserve 807,392 bytes; BIOS reserve 101 sectors
- proof telemetry: `dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 1 dynamic-binding-missing 5 dynamic-binding-libc 1 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 dynamic-symbol-trace 1 dynamic-symbol-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M77 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path weak relocation admission only; undefined weak `R_X86_64_GLOB_DAT` bindings are classified as nullable, while the strong missing PLT binding remains denied before relocation application
- final reserves after the M77 build: UEFI reserve 807,392 bytes; BIOS reserve 101 sectors
- proof telemetry: `dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 1 dynamic-binding-missing 1 dynamic-binding-weak-null 4 dynamic-binding-libc 1 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 dynamic-symbol-trace 1 dynamic-symbol-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M78 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path provider classification only; `libc-x64.so` is now a recognized local libc alias, `__libc_start_main` is an explicit unavailable shim export at a fixed RVA, and no relocation writes or dynamic control transfer are attempted
- final reserves after the M78 build: UEFI reserve 807,392 bytes; BIOS reserve 101 sectors
- proof telemetry: `dynamic-needed 1 dynamic-supported 1 dynamic-missing 0 dynamic-libc 1 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 2 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-unavailable 1 dynamic-binding-libc 2 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M79 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path dry-run only; relocation targets are validated against writable app `PT_LOAD` ranges and provider values are computed from nullable weak symbols or fixed provider registries, but no GOT/PLT memory is written
- final reserves after the M79 build: UEFI reserve 807,392 bytes; BIOS reserve 101 sectors
- proof telemetry: `dynamic-reloc-dry-run 1 dynamic-reloc-dry-total 6 dynamic-reloc-dry-target-valid 6 dynamic-reloc-dry-value 6 dynamic-reloc-dry-provider 2 dynamic-reloc-dry-weak-null 4 dynamic-reloc-dry-unavailable 1 dynamic-reloc-dry-apply-ready 5 dynamic-reloc-dry-blocked 1 dynamic-reloc-dry-error 0 dynamic-reloc-dry-first-target 0x0000000052003FC8 dynamic-reloc-dry-first-value 0x0000000000000000 dynamic-reloc-dry-jmprel-target 0x0000000052004000 dynamic-reloc-dry-jmprel-value 0x0000000047811020 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M80 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path write/readback only; the dynamic relocation walker parses metadata on the kernel root, switches to the target process root only for validated user GOT/PLT writes and readback, then returns to the kernel root before continuing. Dynamic execution is still denied before task registration or control transfer.
- final reserves after the M80 build: UEFI reserve 807,392 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,289,760; checksum `0x7236CD86`; SHA-256 `85a85a062d5943058faa4bf24756f96972a0de204f24ee5274f90eba5faf1b74`
- proof telemetry: `dynamic-reloc-dry-run 1 dynamic-reloc-dry-total 6 dynamic-reloc-dry-target-valid 6 dynamic-reloc-dry-value 6 dynamic-reloc-dry-provider 2 dynamic-reloc-dry-weak-null 4 dynamic-reloc-dry-unavailable 1 dynamic-reloc-dry-apply-ready 5 dynamic-reloc-dry-blocked 1 dynamic-reloc-dry-error 0 dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 5 dynamic-reloc-apply-readback 5 dynamic-reloc-apply-blocked 1 dynamic-reloc-apply-unavailable 1 dynamic-reloc-apply-error 0 dynamic-reloc-apply-first-readback 0x0000000000000000 dynamic-reloc-apply-jmprel-readback 0x0000000047811020 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M81 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path startup provider only; `__libc_start_main` is no longer an unavailable export, the generated Limitless libc shim image contains a bounded routine that calls `main(argc, argv, envp)` and exits through `exit_group`, but the launcher still denies before registering or running a dynamic task.
- final reserves after the M81 build: UEFI reserve 803,296 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,293,856; checksum `0xF3505722`; SHA-256 `791a0e8c5ad45ddaac3096d7edb6bd2699f1985ce83a451fdd2e8754cb818e84`
- proof telemetry: `dynamic-binding-total 6 dynamic-binding-supported 2 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-unavailable 0 dynamic-reloc-dry-run 1 dynamic-reloc-dry-total 6 dynamic-reloc-dry-target-valid 6 dynamic-reloc-dry-value 6 dynamic-reloc-dry-provider 2 dynamic-reloc-dry-weak-null 4 dynamic-reloc-dry-unavailable 0 dynamic-reloc-dry-apply-ready 6 dynamic-reloc-dry-blocked 0 dynamic-reloc-dry-error 0 dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 6 dynamic-reloc-apply-readback 6 dynamic-reloc-apply-blocked 0 dynamic-reloc-apply-unavailable 0 dynamic-reloc-apply-error 0 dynamic-libc-start-main 1 dynamic-libc-start-main-apply 1 dynamic-libc-start-main-value 0x0000000047811BF0 dynamic-libc-start-main-readback 0x0000000047811BF0 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M82 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: denial-path frame proof only; the launcher reuses the existing ELF auxv and initial-stack builders, maps a bounded 64 KiB dynamic stack in the temporary process root, validates stack alignment/null terminators/readback checksums, proves the interpreter entry and stack page are present with executable/writable permissions, and still denies before task registration or control transfer
- final reserves after the M82 build: UEFI reserve 803,296 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,293,856; checksum `0x76B5916E`; SHA-256 `7199b36fb73a8ea711bcce6d5a4c63363df4c39c08691e5afb87eeda2516efbd`
- proof telemetry excerpt: `dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 6 dynamic-reloc-apply-readback 6 dynamic-reloc-apply-blocked 0 dynamic-reloc-apply-unavailable 0 dynamic-reloc-apply-error 0 dynamic-libc-start-main 1 dynamic-libc-start-main-apply 1 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-error 0 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 18 dynamic-stack-align 1 dynamic-stack-argv-null 1 dynamic-stack-envp-null 1 dynamic-stack-auxv-null 1 dynamic-transfer-ready 1 dynamic-transfer-rip 0x000000004780105F dynamic-transfer-rsp 0x000000005300FE30 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M83 acceptance evidence:

- command: `linux /APPS/DYNLDLIMIT`
- staged app artifact: `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: first dynamic execution only; the launcher removes the final denial for the proven supported interpreter path, preserves the live dynamic VMA/root/stack state, registers a normal scheduler task at the interpreter RIP/RSP, and keeps cleanup on the existing process/root path
- hardware loader fix: the UEFI handoff low-page mapper now identity-maps staged boot-media app/interpreter pages before applying the linked-kernel fallback alias, fixing the real-hardware `stage elf code 3` wrong-magic failure for boot-media staged dynamic artifacts
- final reserves after the M83 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x85822D9F`; SHA-256 `66a1620e401afd42d6ce90678bf0c5fab1a132d6d1fba347edb1d0170ff6bdb5`
- visible output: `dynsmoke-start`
- proof telemetry excerpt: `source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 6 dynamic-reloc-apply-readback 6 dynamic-libc-start-main 1 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-auxv 18 dynamic-transfer-ready 1 dynamic-transfer-rip 0x000000004780105F dynamic-transfer-rsp 0x000000005300FE30 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 15 dynamic-exit-code 0x00000000 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 15 exit 0 cleanup 1 dynamic-map-cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M84 acceptance evidence:

- command: `linux /APPS/DYNGETPID`
- staged app artifact: `/APPS/DYNGETPID`, SHA-256 `7BC91398FC2CABE11FB067376D417E5DF8057DACF21214E454A4D9AFA62223CB`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x52001060`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: boot-media Linux staged app/interpreter matching is now build-parameter aware. `tools/build.ps1` writes the selected boot Linux filenames into `arch_build.h`; the UEFI loader records normalized `/apps/...` paths in `boot_info`; and `boot_media.c` matches staged payload reads against those recorded paths while preserving the legacy `/apps/dynldlimit` and `/apps/ldlimit` fallback.
- final reserves after the M84 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x1B965DF5`; SHA-256 `94c3b5d21af2f87a8d2518abc25b3c793db2ddeae53e1dee8cc2992ce95bb8e5`
- visible output: `dyngetpid-pass`
- proof telemetry excerpt: `path /APPS/DYNGETPID source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 3 dynamic-binding-total 7 dynamic-binding-supported 3 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 3 dynamic-jmprel-symbol getpid dynamic-reloc-apply 1 dynamic-reloc-apply-total 7 dynamic-reloc-apply-write 7 dynamic-reloc-apply-readback 7 dynamic-reloc-apply-jmprel-readback 0x0000000047811180 dynamic-libc-start-main 1 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-auxv 18 dynamic-transfer-ready 1 dynamic-transfer-rip 0x000000004780105F dynamic-transfer-rsp 0x000000005300FE30 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 15 dynamic-exit-code 0x00000000 write 1 write-bytes 15 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 15 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M85 acceptance evidence:

- command: `linux /APPS/DYNHELPER`
- staged app artifact: `/APPS/DYNHELPER`, SHA-256 `B30ABD53CF32C9C9E6BEB77FF89DE02C57E1442D15A13EB944D514B794B8A9C8`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: the supported dynamic boot-media path now maps the generated `libc-x64.so` shim into the live process root and transfers to the app entry after relocation/binding proof. Dynamic launch initializes the Linux persona before the libc shim load, avoids duplicate persona init in common setup, and the libc shim load self-check no longer walks low-address static export names while running on a process CR3.
- regression evidence: `linux /APPS/DYNGETPID` still prints `dyngetpid-pass` after the app-entry transfer change, with `dynamic-jmprel-symbol getpid`, `dynamic-reloc-apply-total 7`, `dynamic-transfer-rip 0x0000000052001060`, `dynamic-console-bytes 15`, `exit 0`, and `page-faults 0`
- final reserves after the M85 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x935CD0AB`; SHA-256 `a542589f65779ca1ddc7baa6994c124f8777c02fa89dcf2aaf78c96951a52357`
- visible output: `dynhelper-pass`
- proof telemetry excerpt: `path /APPS/DYNHELPER source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol memcpy dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-reloc-apply-jmprel-readback 0x0000000047811200 dynamic-libc-start-main 1 dynamic-libc-start-main-readback 0x0000000047811BF0 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 15 dynamic-exit-code 0x00000000 write 2 write-bytes 15 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 15 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M86 acceptance evidence:

- command: `linux /APPS/DYNENVSTDIO`
- staged app artifact: `/APPS/DYNENVSTDIO`, SHA-256 `7713DA42475439C1020A7AF932B9C53983CAF0FC8DB41BFD78BF3F701DFBEBFA`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: the generated libc shim environment snapshot now stores the four bounded launch entries, the live dynamic stack path binds that environment into the shim after stack construction, and the shim self-check uses pid-aware process-root page protection validation. No new Linux syscalls, arbitrary shared-library search, or general dynamic linker behavior are added.
- regression evidence: `linux /APPS/DYNHELPER` still prints `dynhelper-pass` after the environment binder change, with `dynamic-stack-envc 4`, `dynamic-binding-supported 5`, `dynamic-reloc-apply-total 9`, `dynamic-transfer-rip 0x0000000052001080`, `dynamic-console-bytes 15`, `exit 0`, and `page-faults 0`
- final reserves after the M86 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x8A8429E7`; SHA-256 `5de4a798cb2c768eaa447a992d8e13862f74cc50fa5bf6afa10835af4f93d2d9`
- visible output: `dynprintf-pass`, `dynfputs-pass`, and `dynfwrite-pass`
- proof telemetry excerpt: `path /APPS/DYNENVSTDIO source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol printf dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-libc-start-main 1 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 44 dynamic-exit-code 0x00000000 write 3 write-bytes 44 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 44 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M87 acceptance evidence:

- command: `linux /APPS/DYNHEAPENV`
- staged app artifact: `/APPS/DYNHEAPENV`, SHA-256 `5482AF5167968986BE620CD86AE88385C49D0FCF0F435DF987727C1530AAA463`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010D0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: the generated `setenv` helper now scans the full bounded environment vector instead of failing after the first non-matching entry; heap behavior remains the existing fixed-page mmap-backed helper model with one live page per helper call in this proof. No new Linux syscalls, arbitrary shared-library search, or general dynamic linker behavior are added.
- first trace before fix: `linux /APPS/DYNHEAPENV` reached dynamic `main`, performed three anonymous heap `mmap` calls, then exited 14 with `dynamic-console-bytes 0`, proving `setenv("USER", "pilot", 1)` failed before output
- regression evidence: `linux /APPS/DYNENVSTDIO` still prints `dynprintf-pass`, `dynfputs-pass`, and `dynfwrite-pass` after the `setenv` helper fix, with `dynamic-stack-envc 4`, `dynamic-binding-supported 6`, `dynamic-console-bytes 44`, `exit 0`, and `page-faults 0`
- final reserves after the M87 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x44EF6F18`; SHA-256 `0d2f1c2724553ecad1a01dbd300216ea2ab004d0ec59307580b975b133fbb4ad`
- visible output: `dynheap-pass` and `dynsetenv-pass`
- proof telemetry excerpt: `path /APPS/DYNHEAPENV source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 10 dynamic-binding-total 14 dynamic-binding-supported 10 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 10 dynamic-jmprel-symbol strcpy dynamic-reloc-apply 1 dynamic-reloc-apply-total 14 dynamic-reloc-apply-write 14 dynamic-reloc-apply-readback 14 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010D0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 28 dynamic-exit-code 0x00000000 mmap 3 mmap-bytes 12288 mmap-denial 0 write 4 write-bytes 28 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 28 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M88 acceptance evidence:

- command: `linux /APPS/DYNTHREAD`
- staged app artifact: `/APPS/DYNTHREAD`, SHA-256 `749582EF277B19EE11795928199A941E4BD5E81D502B7F2B60104A30115B894A`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before fix: `linux /APPS/DYNTHREAD` reached the generated pthread child trampoline, then failed with invalid opcode at `RIP 0x0000000047811944`; root cause was overlap between the `pthread_create` helper/trampoline slot and the `pthread_join` helper slot
- second trace before fix: after moving `pthread_join` to a non-overlapping RVA, the child reached `exit(60)` but page-faulted returning to the trampoline on the kernel root CR3 because clone-thread exits recorded exit state without waking a parent blocked in `wait4`/`pthread_join`
- implementation scope: move the generated `pthread_join` helper RVA from `0x1940` to `0x1950`, and complete blocked waiters for clone-thread exits the same way fork-child exits already completed blocked `wait4`
- final reserves after the M88 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x4F4DFCB5`; SHA-256 `71568191efe2874375f5c2eef376dcd460fc80e3b1476bfde9d7626a2d448a40`
- visible output: `dynpthread-pass`
- proof telemetry excerpt: `path /APPS/DYNTHREAD source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol pthread_create dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 16 dynamic-exit-code 0x00000000 mmap 1 mmap-bytes 16384 write 2 write-bytes 16 futex-wake 4 futex-waiters-final 0 thread-exit-cleartid 1 clone-thread 1 clone-thread-success 1 clone-denial 0 clone-last-flags 0x01310F00 clone-shared-cr3 1 clone-shared-vma 1 clone-shared-fd 1 wait4 1 wait4-reap 1 wait4-last-exit-code 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 16 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M61 is `non-shell execvp parent-rebased directory executable denial proof`; the post-M21 real static Linux ELF gate, the M22 per-process CR3 isolation gate, the M23 bounded fork/exec/wait gate, the M24 Unix pipeline gate, the M25 Linux-visible `/bin`/`/usr/bin` path execution gate, the M26 forked-child `execve` inheritance gate, the M27 independently staged static ET_EXEC gate, the M28 third-party non-BusyBox package gate, the M29 broader third-party utility read/pipeline gate, the M30 third-party `/usr/local/bin` PATH directory gate, the M31 default initial `PATH` environment gate, the M32 bounded shell environment expansion gate, the M33 cwd/PWD synchronization gate, the M34 child-observed environment mutation gate, the M35 non-shell third-party `execvp` handoff gate, the M36 non-shell third-party PATH `execvp` gate, the M37 fully PATH-resolved third-party pipeline gate, the M38 expanded `/usr/local/bin` inspection gate, the M39 absolute `/usr/local/bin` execution gate, the M40 absolute `/usr/local/bin` file-read gate, the M41 cwd-relative absolute-localbin file-read gate, the M42 `..` relative-path absolute-localbin gate, the M43 current-directory `./` absolute-localbin gate, the M44 non-shell current-directory `./` exec handoff gate, the M45 relative executable `..` localbin handoff gate, the M46 absolute executable `..` localbin handoff gate, the M47 absolute executable mixed `.`/`..` localbin handoff gate, the M48 repeated-slash absolute executable localbin handoff gate, the M49 root-clamped absolute executable `..` localbin handoff gate, the M50 over-root absolute executable `..` clamp gate, the M51 trailing-slash executable denial gate, the M52 trailing-slash directory enumeration gate, the M53 absolute missing-localbin executable denial gate, the M54 PATH missing executable denial gate, the M55 non-shell `execvp` missing executable denial gate, the M56 non-shell `execvp` trailing-slash executable denial gate, the M57 non-shell `execvp` directory-target denial gate, the M58 non-shell `execvp` bare-directory executable denial gate, the M59 non-shell `execvp` dot-directory executable denial gate, the M60 non-shell `execvp` parent-directory executable denial gate, and the M61 non-shell `execvp` parent-rebased directory executable denial gate are crossed on the UEFI Product path.

M1 cleanup-final through M20 native app execution pipeline are accepted, and M18.1 closed the UEFI real-firmware handoff compatibility boundary. M19 shifted networking from hardware-gated proof to app-facing brokered service: UEFI Product publishes a network service endpoint plus a narrow syscall-level TCP-client socket contract over the existing broker-owned DHCP/DNS/HTTP path. M21 provides the first manifest-driven native app SDK foundation over that service. Product builds read `apps/native/*.json`, assemble app binaries, generate `.APP` descriptors, stage descriptor/binary pairs into `/APPS`, sign package payload records, and load apps by name through a generic descriptor parser before Ring-3 launch. The post-M21 gate adds a separate UEFI-only `linux <path> [args...]` Product shell path that loads one externally built static Linux x86_64 ELF from NVMe FAT storage and executes it through the Linux persona/syscall path with brokered console output. M22 gives each UEFI Product Linux persona launch its own PML4 from a fixed static pool, keeps kernel/MMIO mappings shared, switches CR3 at scheduler start/swap/exit boundaries, and proves the running hardware CR3 matches the process root while the lower-half user page tables are private. M23 adds bounded Linux `fork(57)` plus blocking `wait4(61)` for the observed BusyBox ash external-command path: the parent and child receive distinct PML4 slots, the child gets a full copied VMA/address-space/FD/persona/VFS/audit state, the parent waits and collects the child exit status, and both process roots are returned to the static pool. M24 adds Linux `pipe(22)` ABI exposure over the existing fixed pipe provider and exact-fd pipe endpoint inheritance across fork, proving that BusyBox ash can compose `fork`, `pipe`, inherited fds, `read`, `write`, and `wait4` in a real pipeline. M25 widens `execve` for real external binaries, switches real exec images to a 64 KiB UEFI-only stack while preserving the old synthetic 4 KiB exec test constants, exposes read-only BusyBox applet aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin`, and proves BusyBox ash can find and execute the real staged NVMe BusyBox binary through ordinary Linux PATH search. M26 proves the M25 exec transfer composes with M22/M23/M24 process semantics: forked BusyBox ash children inherit cwd, VFS authority, and pipe endpoints, then replace themselves through `execve`, move data through the pipe, and return cleanly to the parent through `wait4`. M27 proves the NVMe FAT image generator and QEMU verifier can stage a second static ET_EXEC artifact at `/APPS/SMOKE`, expose it through the Linux-visible `/nvme/apps/smoke` VFS path, and execute it through a forked BusyBox ash pipeline without using the BusyBox alias backend. The M27 implementation also makes Linux task FS-base state scheduler-owned, so `arch_prctl(ARCH_SET_FS)` TLS bases are saved, restored, and seeded across task switches, fork, exec, wait, and pipe scheduling. M28 replaces the local SMOKE evidence with an upstream suckless sbase 0.1 `echo` utility built by the musl cross toolchain as a static non-PIE ET_EXEC at `0x52000000`, staged as `/APPS/SBECHO`, launched directly, and execed through BusyBox ash as `/nvme/apps/sbecho` in a real pipeline. M28 also fixes the real-binary cleanup invariant so programs that legitimately close stdout before exit are accepted when the fd table is fully detached. M29 adds a second upstream suckless sbase 0.1 utility, `cat`, staged as `/APPS/SBCAT`, proves it can directly read `/nvme/apps/data/file.txt` through the NVMe VFS, and proves a two-executable third-party pipeline `/nvme/apps/sbecho | /nvme/apps/sbcat` without relying on the BusyBox alias backend for the data consumer. M30 exposes those third-party sbase utilities through a bounded Linux-visible `/usr/local/bin` directory, proves BusyBox ash can resolve `sbecho` and `sbcat` by PATH name, and keeps the backend tied to the staged NVMe FAT artifacts rather than a BusyBox applet alias. M31 seeds a bounded default Linux environment for real launches with `PATH=/usr/local/bin:/bin:/usr/bin`, proves BusyBox ash can resolve the third-party utilities without command-local PATH assignment, and proves the environment is inherited into forked child `execve` calls. M32 extends that fixed environment with `HOME=/`, `USER=limitless`, and `PWD=/`, proves BusyBox ash expands `$USER:$PWD` correctly, and proves those entries are inherited into forked child `execve` calls. M33 proves BusyBox ash updates `PWD` after `cd /nvme/apps`, passes that updated environment through forked child `execve`, and keeps the Linux persona cwd coherent with relative PATH and pipe execution. M34 adds an upstream suckless sbase 0.1 `env` utility staged as `/APPS/SBENV` and proves a third-party child process can observe a shell-mutated exported `USER=operator` environment through a forked pipeline. M35 proves that same third-party `sbenv` process can call `execvp` with an explicit Linux VFS path, replace itself with another real ET_EXEC image, preserve the explicit `USER=operator` environment, and complete through the inherited pipeline. M36 adds the bounded `/usr/local/bin/sbenv` alias and proves that non-shell third-party `execvp` PATH search can locate and execute another third-party ET_EXEC utility by name. M37 proves the full pipeline can drop all explicit child executable paths: BusyBox ash resolves `sbenv` and `sbcat` by PATH, then the first `sbenv` resolves the second `sbenv` by PATH before the pipeline completes. M38 extends the host NVMe staging verifier to carry all three third-party artifacts at once and proves `/usr/local/bin` visibly enumerates `sbenv`, `sbcat`, and `sbecho` with no stat denials. M39 proves the same bounded third-party namespace works for direct absolute `/usr/local/bin/...` execution, including a third-party `sbenv` process replacing itself through an absolute `/usr/local/bin/sbenv` path and piping to absolute `/usr/local/bin/sbcat`. M40 proves absolute `/usr/local/bin/sbcat` can read a real NVMe FAT file directly and that two forked absolute-localbin `sbcat` children can pipe that file content with clean process, pipe, and VFS cleanup. M41 proves those absolute-localbin child exec paths inherit BusyBox ash cwd after `cd /nvme/apps` and can read cwd-relative `data/file.txt` through the NVMe VFS. M42 proves the same inherited cwd path handles `..` canonicalization from `/nvme/apps/data` back to `/nvme/apps/data/file.txt` in forked absolute-localbin children. M43 makes child `execve` canonicalize relative executable paths before VFS stat/read and proves `./sbcat` resolves from cwd `/usr/local/bin` into the bounded localbin alias table. M44 proves that same current-directory relative exec path composes with a third-party `sbenv` process doing its own explicit `./sbenv` handoff before piping into `./sbcat`. M45 proves relative executable `..` segments from cwd `/usr/local/bin` canonicalize back into the bounded localbin alias table for both shell-launched children and the non-shell `sbenv` handoff. M46 proves absolute executable `..` segments in `/usr/local/bin/../bin/...` canonicalize into the same bounded localbin alias table without depending on cwd changes. M47 proves mixed absolute executable `.` and `..` segments in `/usr/local/./bin/../bin/...` canonicalize into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M48 proves repeated slashes in `/usr//local/bin/...` and `/usr/local//bin/...` canonicalize into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M49 proves bounded upward `..` traversal in `/usr/local/bin/../../local/bin/...` canonicalizes back into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M50 proves over-root `..` traversal in `/../../usr/local/bin/...` and `/../../../usr/local/bin/...` clamps at `/` and canonicalizes into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M51 preserves trailing-slash intent across execve canonicalization and denies slash-suffixed non-directory executable targets before binary read. M52 proves that denial remains scoped to executable targets and does not break trailing-slash directory enumeration. M53 maps failed exec binary stat to `ENOENT`, proving a missing absolute-localbin executable reports `not found` while the bounded alias tables deny cleanly. M54 proves BusyBox ash default-PATH lookup reports the same `not found` result for a missing command while `/usr/local/bin`, `/bin`, and `/usr/bin` alias denials remain bounded and no unintended executable backend reads occur. M55 proves a third-party `sbenv` process reports `ENOENT` from its own `execvp("sbmissing")` path and keeps alias denials and backend reads bounded. M56 proves that same non-shell `execvp` path preserves trailing-slash executable intent and reports `ENOTDIR` for `/usr/local/bin/sbenv/` without launching the target as a file. M57 proves a non-shell `execvp` attempt against `/usr/local/bin/` is denied visibly with no unintended directory-as-binary read. M58 proves the same denial for the bare `/usr/local/bin` directory target without relying on terminal-slash intent. M59 proves `/usr/local/bin/.` dot-segment canonicalization still rejects a directory target before executable backend read. M60 proves `/usr/local/bin/..` parent-segment canonicalization still rejects a parent directory target before executable backend read. M61 proves `/usr/local/bin/../bin` parent-rebased canonicalization still rejects the rebased `/usr/local/bin` directory target before executable backend read. M21 native apps are still not Linux/Windows app personas, and the Product surface still does not include a server socket API, raw packet API, arbitrary app network data plane, dynamic Linux linking, vfork, broad clone flag compatibility, real threading, signal delivery, sockets, broad ioctl/device control, or broad third-party package compatibility.

Post-M21 acceptance is governed by `docs/real-binary-gate.md`. Synthetic processes, repo-built flat binaries, embedded byte arrays, and denial-only telemetry may remain regression evidence, but they no longer count as forward Product capability evidence. The first execution claim now has real-binary evidence from externally built BusyBox 1.35.0 static musl ET_EXEC artifacts linked at `0x52000000`, staged as `/APPS/BUSYBOX`, loaded from the NVMe FAT image, and proven to print brokered console output with exit code 0. The current BusyBox artifact also proves a minimal `sh` path: ash prints its banner and `$` prompt through brokered console output, consumes verifier-provided shell loops through bounded brokered stdin, and exits cleanly. The M22 acceptance command `linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'` proves that a real externally built static Linux binary now runs with a process-owned root distinct from the kernel root while still using shared higher-half kernel/MMIO mappings. The M23 acceptance command `linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'` proves a real BusyBox shell can fork a child command, run it against the NVMe VFS, wait for exit status 0, and clean up both parent and child PML4 slots while producing visible directory output. The M24 acceptance command `linux /APPS/BUSYBOX sh -c 'echo hello | cat'` proves a real BusyBox shell can create a pipe, fork both pipeline sides, inherit pipe fds into children, transfer bytes through the pipe, reap both children, and release all pipe and PML4 resources. The M25 acceptance commands `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'` and `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'` prove normal BusyBox ash PATH search can resolve Linux-visible applet paths backed by the real staged NVMe BusyBox binary and that `/usr/bin` enumerates the fixed virtual applet directory without stat denials. The M26 acceptance command `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'` proves forked child exec images inherit cwd, relative-path VFS behavior, pipe endpoints, and brokered console output correctly. The M27 acceptance command `linux /APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'` proves a second independently staged static ET_EXEC can be loaded from `/APPS/SMOKE`, resolved through the Linux-visible NVMe VFS, execed by a forked child, piped to another child, and cleaned up with no page faults, no CR3 repair, no pipe leaks, and no PML4 leaks. The M28 acceptance commands `linux /APPS/SBECHO m28-sbase-direct` and `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'` prove a third-party non-BusyBox package utility can launch directly from `/APPS/SBECHO`, then be found through `/nvme/apps/sbecho`, fork/execed by BusyBox ash, piped to `/bin/cat`, and cleaned up without page faults, CR3 repair, pipe leaks, or PML4 leaks. The M29 acceptance commands `linux /APPS/SBCAT /nvme/apps/data/file.txt` and `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'` prove a second upstream sbase utility can read a real NVMe file directly and can consume a pipe as a third-party ET_EXEC child with no BusyBox alias backend involved. The M30 acceptance command `linux /APPS/BUSYBOX sh -c 'PATH=/usr/local/bin:/bin:/usr/bin; sbecho m30-path-pipe | sbcat'` proves third-party ET_EXEC utilities can be discovered by PATH name through `/usr/local/bin`, forked, execed, piped together, and cleaned up while their actual bytes still come from the staged `/APPS/SBECHO` and `/APPS/SBCAT` NVMe FAT artifacts. The M31 acceptance command `linux /APPS/BUSYBOX sh -c 'sbecho m31-default-path | sbcat'` proves the same PATH pipeline now works from the default initial Linux environment, without command-local PATH assignment. The M32 acceptance command `linux /APPS/BUSYBOX sh -c 'sbecho $USER:$PWD | sbcat'` proves BusyBox ash expands the default `USER` and `PWD` variables and carries the four-entry environment through child exec. The M33 acceptance command `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; sbecho $PWD | sbcat'` proves BusyBox ash updates `PWD` after `chdir`, exports it to forked child exec images, and keeps it coherent with the Linux persona cwd used for relative path and PATH pipeline execution. The M34 acceptance command `linux /APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'` proves a third-party child process observes the shell-mutated exported environment through `execve`, pipe, and wait cleanup. The M35 acceptance command `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator /nvme/apps/sbenv | /nvme/apps/sbcat'` proves a third-party process can perform the extra `execvp` handoff itself. The M36 acceptance command `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator sbenv | /nvme/apps/sbcat'` proves that non-shell third-party `execvp` PATH search can find `sbenv` through the bounded `/usr/local/bin` alias table. The M37 acceptance command `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator sbenv | sbcat'` proves BusyBox ash and the non-shell `sbenv` process can all resolve third-party utilities through the default PATH. The M38 acceptance command `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin'` proves the expanded bounded third-party PATH namespace is inspectable and reports all three aliases with no stat denials. The M39 acceptance command `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv USER=operator /usr/local/bin/sbenv | /usr/local/bin/sbcat'` proves direct absolute `/usr/local/bin` execution works for BusyBox ash, non-shell `execvp`, and the pipeline consumer.

M89 acceptance evidence:

- command: `linux /APPS/DYNPTLS`
- staged app artifact: `/APPS/DYNPTLS`, SHA-256 `47D6FCCA829DE00FE42814155DF06C5EE925FCACC00A20CEF49C95067E6D8A6F`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010E0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before fix: `linux /APPS/DYNPTLS` proved four clone-backed dynamic pthreads, per-thread TLS helper use, and contended futex activity, but exited 7 before output with `wait4 0 wait4-reap 0`; root cause was the generated `pthread_create` helper passing the public `pthread_t *` as both `parent_tid` and `child_tid`, so `CLONE_CHILD_CLEARTID` zeroed the join handle before `pthread_join`
- implementation scope: repack the generated pthread helper RVAs inside the existing libc shim text page, add 16 shim-owned clear-tid words in the bounded data page, and update `pthread_create` to use the application `pthread_t *` only for `CLONE_PARENT_SETTID` while using a private clear-tid word for `CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID`; no new Linux syscalls, no arbitrary shared-library loading, and no libc image size increase
- final reserves after the M89 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0xA3DDFB72`; SHA-256 `ddb46bc3ad9d89297605e21c394629866be7b31b50e4f6346b3b39355d6a6d83`
- visible output: `dynptls-pass`
- proof telemetry excerpt: `path /APPS/DYNPTLS source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 11 dynamic-binding-total 15 dynamic-binding-supported 11 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 11 dynamic-jmprel-symbol pthread_cond_signal dynamic-reloc-apply 1 dynamic-reloc-apply-total 15 dynamic-reloc-apply-write 15 dynamic-reloc-apply-readback 15 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010E0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 13 dynamic-exit-code 0x00000000 mmap 4 mmap-bytes 65536 write 2 write-bytes 13 futex-wait 4 futex-wake 17 futex-woken 4 futex-waiters-final 0 thread-exit-cleartid 4 clone-thread 4 clone-thread-success 4 clone-denial 0 clone-last-flags 0x01310F00 clone-shared-cr3 1 clone-shared-vma 1 clone-shared-fd 1 fs-set 4 fs-save 8 fs-restore 9 wait4 4 wait4-reap 4 wait4-last-exit-code 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 13 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M90 acceptance evidence:

- command: `linux /APPS/DYNFILEIO`
- staged app artifact: `/APPS/DYNFILEIO`, SHA-256 `42B199D95374ADC5F8F349405423E2F6976394AD8FD4DC04F88DD3E56F56354F`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before fix: `linux /APPS/DYNFILEIO` exited 1 before output with `vfs-nvme-bind 0`, `read 0`, `write 0`, and `console-bytes 0`; root cause was that boot-media-staged Linux launches did not pass scoped NVMe read authority into the live process even when the shell had that capability
- implementation scope: thread the NVMe capability through `linux_exec64_run_boot_media`, bind it to the Linux VFS when valid, and require VFS release during cleanup only if a bind occurred; boot-media launches without NVMe remain available, and no new Linux syscalls or dynamic linker features are added
- final reserves after the M90 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0xACE1384D`; SHA-256 `dd466bda3fc37f85f1195b3bb774642d0a93713c909391e0c7887354218b378d`
- visible output: `dynfileio:Nested FAT32 path fixture`
- proof telemetry excerpt: `path /APPS/DYNFILEIO source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 37 dynamic-exit-code 0x00000000 read 1 read-bytes 27 write 2 write-bytes 37 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 37 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M91 acceptance evidence:

- command: `linux /APPS/DYNSEEK`
- staged app artifact: `/APPS/DYNSEEK`, SHA-256 `22F4206B5DFA3A9048A08F2FF31E9931AC969DFC404EA684F2E7B0D0013AEE62`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010B0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before fix: `linux /APPS/DYNSEEK` proved `stat 1` and `fstat 1`, but exited 6 before reads because `lseek(fd, 7, SEEK_SET)` was denied on the NVMe VFS fd; root cause was that `fd64_seek` only accepted `FD64_TYPE_RAMFS_NODE` even though Linux VFS file reads already honor `FD64_TYPE_DEVICE` file offsets
- implementation scope: allow `fd64_seek` for Linux VFS device fds only when `linux_vfs64_fstat` proves a file-like node, preserve denial for terminals, pipes, sockets, directories, and unknown devices, and publish `lseek`/`lseek-denial` in the real-binary telemetry; no new Linux syscalls or dynamic linker features are added
- final reserves after the M91 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0xF1D59AE7`; SHA-256 `60cab95f91c2e0e7ca003358bb294daadac24778574a1f3d95c02517a65a073e`
- visible output: `dynseek:FAT32 pa:fixture`
- proof telemetry excerpt: `path /APPS/DYNSEEK source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 8 dynamic-binding-total 12 dynamic-binding-supported 8 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 8 dynamic-jmprel-symbol lseek dynamic-reloc-apply 1 dynamic-reloc-apply-total 12 dynamic-reloc-apply-write 12 dynamic-reloc-apply-readback 12 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010B0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 24 dynamic-exit-code 0x00000000 stat 1 stat-denial 0 stat-fault 0 fstat 1 fstat-denial 0 fstat-fault 0 lseek 2 lseek-denial 0 read 2 read-bytes 15 write 1 write-bytes 24 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 24 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M92 acceptance evidence:

- command: `linux /APPS/DYNDIR`
- staged app artifact: `/APPS/DYNDIR`, SHA-256 `A54BE4145223AC87CA1E1C3ECF8926FEE6F5CC79CD27463C8D68812A6D30E802`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: add only a generated libc syscall export for `getdents64` at the reserved `0x11F0` stub slot, increasing libc symbol count from 50 to 51 and syscall-backed symbols from 16 to 17; no new Linux ABI syscall implementation was needed because kernel `getdents64(217)` already existed
- final reserves after the M92 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0xAD882A99`; SHA-256 `ee1b5157188b5b4a489f996c58c70f98423cebc7d79ef57789443d3ca806ce51`
- visible output: `dyndir:data`
- proof telemetry excerpt: `path /APPS/DYNDIR source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 12 dynamic-exit-code 0x00000000 getdents64 1 getdents64-entries 3 getdents64-bytes 88 stat 1 stat-denial 0 stat-fault 0 write 1 write-bytes 12 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-readdirs 4 vfs-nvme-dirents 3 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 12 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M93 acceptance evidence:

- command: `linux /APPS/DYNCWD`
- staged app artifact: `/APPS/DYNCWD`, SHA-256 `B36DEFEEEFD22F0E050A20210D7F24575A0ABC3C0ED8F20F73386AEBC1B471D4`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010B0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before fix: `linux /APPS/DYNCWD` faulted before telemetry with a user page fault at RIP `0x0000000047811012`, CR2 `0x0000000052001000`, error `0x7`; root cause was placing new generated libc syscall stubs in the `0x1010`/`0x1018` low gaps, which overlapped the longer `pthread_cond_init` helper beginning at `0x1008`
- implementation scope: expose existing kernel ABI syscalls `getcwd(79)`, `chdir(80)`, and `readlink(89)` through generated libc syscall stubs placed in non-overlapping helper gaps at `0x1410`, `0x1430`, and `0x1450`; no new Linux ABI syscall implementation was needed
- final reserves after the M93 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x5C6B2453`; SHA-256 `b3516c247c55c182e93673e86b58e4545d1ff4bd57493446e9a6bd08e9b0b88c`
- visible output: `dyncwd:/nvme/apps:Nested FAT32 path fixture:/proc/self/exe`
- proof telemetry excerpt: `path /APPS/DYNCWD source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 8 dynamic-binding-total 12 dynamic-binding-supported 8 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 8 dynamic-jmprel-symbol readlink dynamic-reloc-apply 1 dynamic-reloc-apply-total 12 dynamic-reloc-apply-write 12 dynamic-reloc-apply-readback 12 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010B0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 59 dynamic-exit-code 0x00000000 readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 getcwd 2 getcwd-bytes 13 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 0 path-dotdot 0 path-trailing 0 path-fault 0 chdir 1 chdir-denial 0 chdir-fault 0 read 1 read-bytes 27 write 7 write-bytes 59 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 59 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M94 acceptance evidence:

- command: `linux /APPS/DYNVEC`
- staged app artifact: `/APPS/DYNVEC`, SHA-256 `2F4CB3560E98FF4585731F48673C5B86C2D2B82CD8D8B2D4284E9F5E6BD49915`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before smoke fix: `linux /APPS/DYNVEC` proved the new `poll` binding against brokered stdout with `poll-ready 1 poll-last-revents 0x00000004`, then exited from a smoke-side content split bug after successful `readv 1 readv-bytes 27`; no kernel syscall behavior was changed after that trace
- implementation scope: expose existing kernel ABI syscalls `readv(19)`, `writev(20)`, and `poll(7)` through generated libc syscall stubs at `0x1468`, `0x1470`, and `0x1478`; no new Linux ABI syscall implementation was needed
- final reserves after the M94 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x255B160B`; SHA-256 `68136299ce6e66106b5684f5ae6476a6e3cd3f596c33e45616e2f38763657752`
- visible output: `dynvec:Nested:FAT32 path fixture`
- proof telemetry excerpt: `path /APPS/DYNVEC source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol writev dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 33 dynamic-exit-code 0x00000000 readv 1 readv-bytes 27 writev 1 writev-bytes 33 poll 1 ppoll 0 poll-ready 1 poll-last-revents 0x00000004 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 33 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M95 acceptance evidence:

- command: `linux /APPS/DYNFSTATAT`
- staged app artifact: `/APPS/DYNFSTATAT`, SHA-256 `35504B625F60B8C4DAAF464B57219466AA49CABCCDC0082F6907505C1C1DE8A0`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001070`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before wrapper fix: `linux /APPS/DYNFSTATAT` bound `newfstatat` but failed with `newfstatat-denial 1` because the generic 8-byte syscall stub did not move the fourth C ABI argument from `rcx` to Linux syscall ABI `r10`
- second trace before wrapper fix: after adding the `rcx` to `r10` move, the call still failed with `path-fault 1` because a C `int dirfd` zero-extended `AT_FDCWD`; the accepted fix uses a dedicated `newfstatat` wrapper that also sign-extends `edi` into `rdi`
- implementation scope: expose existing kernel ABI syscall `newfstatat(262)` through a generated libc wrapper at `0x1D40` that performs `movsxd rdi, edi`, `mov r10, rcx`, then `syscall`; no new Linux ABI syscall implementation was needed
- final reserves after the M95 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0xF3DB573B`; SHA-256 `01bae43f4fce2c6dc65e754e768420fe801786aa23df81a4192543e2eb21849b`
- visible output: `dynfstatat:27:file-dir`
- proof telemetry excerpt: `path /APPS/DYNFSTATAT source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 4 dynamic-binding-total 8 dynamic-binding-supported 4 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 4 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 8 dynamic-reloc-apply-write 8 dynamic-reloc-apply-readback 8 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001070 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 newfstatat 2 newfstatat-denial 0 newfstatat-fault 0 path-relative 2 path-fault 0 chdir 1 chdir-denial 0 chdir-fault 0 write 1 write-bytes 23 vfs-nvme-bind 1 vfs-nvme-release 1 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M96 acceptance evidence:

- command: `linux /APPS/DYNOPENAT`
- staged app artifact: `/APPS/DYNOPENAT`, SHA-256 `C28EC4FA1D26912A31B36591F9FCE73E07D086FC3F80CB3C4514C1AC374AD7BB`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: expose existing kernel ABI syscall `openat(257)` through a generated libc wrapper at `0x1D50` that uses the same four-argument syscall wrapper shape as `newfstatat`; no new Linux ABI syscall implementation was needed
- final reserves after the M96 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x53D056E4`; SHA-256 `7b08650626686a606aa4db63be73c4d23f30cf9df1086e87ce9215309e4cce94`
- visible output: `dynopenat:Nested:FAT32`
- proof telemetry excerpt: `path /APPS/DYNOPENAT source 2 boot-media-read 1 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 path-relative 1 path-fault 0 chdir 1 chdir-denial 0 chdir-fault 0 openat 1 read 1 read-bytes 27 write 1 write-bytes 23 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M97 acceptance evidence:

- command: `linux /APPS/DYNODIR`
- staged app artifact: `/APPS/DYNODIR`, SHA-256 `43B960D929BC6E15B573E65166FEB2EDFFDBBC2573BE19372697056CDACA0E23`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: no kernel code change; reuse the existing `openat(257)` generated libc wrapper and the existing kernel dirfd canonicalization path that obtains the base path from a VFS-backed directory fd
- final reserves after the M97 gate run: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x53D056E4`; SHA-256 `7b08650626686a606aa4db63be73c4d23f30cf9df1086e87ce9215309e4cce94`
- visible output: `dynopenatdirfd:Nested:FAT32`
- proof telemetry excerpt: `path /APPS/DYNODIR source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 28 dynamic-exit-code 0x00000000 path-relative 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 openat 2 read 1 read-bytes 27 write 1 write-bytes 28 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 28 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M98 acceptance evidence:

- command: `linux /APPS/DYNFCHDIR`
- staged app artifact: `/APPS/DYNFCHDIR`, SHA-256 `0F4B1A7B2D001568C801B7930D8BD4F9F9ED6BE4EA239AB022AE59D4CEA48E08`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before accepted fix: the `fchdir` binding resolved to `0x0000000047811D60` but faulted with `rip 0x0000000047811D60 cr2 0x0000000000000003` because the initial RVA was outside the executable syscall-stub area
- implementation scope: expose existing kernel ABI syscall `fchdir(81)` through the generated libc shim at executable RVA `0x11F8`, the 8-byte text gap between `getdents64` and `memcpy`; no new Linux ABI syscall implementation was needed
- final reserves after the M98 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x28E88BAA`; SHA-256 `3f782d4294c6966d152b46b7fad10be282108c6ed4ba4244eab48dc3cef9a64c`
- visible output: `dynfchdir:Nested:FAT32`
- proof telemetry excerpt: `path /APPS/DYNFCHDIR source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol fchdir dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-reloc-apply-jmprel-readback 0x00000000478111F8 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 path-relative 1 path-fault 0 chdir 0 fchdir 1 chdir-denial 0 chdir-fault 0 openat 2 read 1 read-bytes 27 write 1 write-bytes 23 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M99 acceptance evidence:

- command: `linux /APPS/DYNFCNTL`
- staged app artifact: `/APPS/DYNFCNTL`, SHA-256 `D20CFF658A3E96FF67F1915943D7948207A4ADDDA649905FFB80F9B5E596F5E3`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: expose existing kernel ABI syscall `fcntl(72)` through the generated libc shim at executable RVA `0x11E8`, the 8-byte text gap between `futex` and `getdents64`; add launch telemetry for `fcntl` and `fcntl-denial`; no new Linux ABI syscall implementation was needed
- final reserves after the M99 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x810A2089`; SHA-256 `ad38277151dc19f404a8f600411392ebc934fdbc01528bc3c83dcf9d3eaf1a7f`
- visible output: `dynfcntl:cloexec:nonblock`
- proof telemetry excerpt: `path /APPS/DYNFCNTL source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 26 dynamic-exit-code 0x00000000 openat 1 fcntl 6 fcntl-denial 0 read 1 read-bytes 27 write 1 write-bytes 26 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 26 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M100 acceptance evidence:

- command: `linux /APPS/DYNFDUP`
- staged app artifact: `/APPS/DYNFDUP`, SHA-256 `7401F71B8B9740DE364BDC9CA3ED931C7D751F9619432AD4C9F9171EDF159D22`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before accepted fix: `linux /APPS/DYNFDUP` printed `dynfdup:read-fail` and exited 5 with `fcntl 3`, `fcntl-denial 0`, `read 0`, `read-bytes 0`, and `vfs-nvme-reads 0`; root cause was that descriptor duplication copied the fd table entry but did not copy the Linux VFS per-fd path sidecar record used by VFS-backed reads
- second trace before accepted fix: the new sidecar-copy helper still failed because it called `linux_vfs64_init()` unconditionally and reset existing sidecar records during duplication; the accepted fix uses the normal guarded VFS initialization pattern
- implementation scope: add `linux_vfs64_dup_fd_path()` and thread it through existing `dup`, `dup2`, `dup3`, and `F_DUPFD` paths so duplicated descriptors preserve VFS path sidecar metadata; no new Linux ABI syscall, dynamic libc export, arbitrary shared-library loading, or filesystem mutation was added
- final reserves after the M100 build: UEFI reserve 798,496 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,298,656; checksum `0x256CFA6D`; SHA-256 `799b084e04e60402555bd1e230c87c692bcacd8099560528d125fcf8ab300d79`
- visible output: `dynfdup:dupfd:no-cloexec`
- proof telemetry excerpt: `path /APPS/DYNFDUP source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 25 dynamic-exit-code 0x00000000 openat 1 fcntl 3 fcntl-denial 0 read 1 read-bytes 27 write 1 write-bytes 25 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 25 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M101 acceptance evidence:

- command: `linux /APPS/DYNDUP`
- staged app artifact: `/APPS/DYNDUP`, SHA-256 `FBD8FBAFC838370E6DF112630F661E76C6AC988FF83D1A3024DAA04C936E5A7E`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before accepted fix: `linux /APPS/DYNDUP` faulted at `rip 0x0000000047811D60 cr2 0xFFFFFFFF8001008D` because the generated libc export for `dup` resolved to the late syscall stub area at `0x1D60`, but the generic syscall-stub writer still bounded writes to the executable text range and refused to emit those bytes; the accepted fix widens the generic stub emission bound to include the reserved late-stub gap before the dynamic table, matching the already-working late `newfstatat` and `openat` stubs
- implementation scope: expose existing kernel ABI syscalls `dup(32)`, `dup2(33)`, and `dup3(292)` through generated libc stubs, add launch telemetry for `dup`, `dup2`, `dup3`, and `dup-denial`, and fix late-stub emission bounds; no new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, or filesystem mutation was added
- final reserves after the M101 build: UEFI reserve 794,400 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,302,752; checksum `0xF29D42FB`; SHA-256 `17b45a78f835218e2f73039f5112824be78dc48b18ee7a81b2c162f601f70339`
- visible output: `dyndup:dup:dup2:dup3`
- proof telemetry excerpt: `path /APPS/DYNDUP source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 9 dynamic-binding-total 13 dynamic-binding-supported 9 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 9 dynamic-jmprel-symbol dup3 dynamic-reloc-apply 1 dynamic-reloc-apply-total 13 dynamic-reloc-apply-write 13 dynamic-reloc-apply-readback 13 dynamic-reloc-apply-jmprel-readback 0x0000000047811D70 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010C0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 21 dynamic-exit-code 0x00000000 openat 3 dup 2 dup2 1 dup3 1 dup-denial 0 fcntl 1 fcntl-denial 0 read 3 read-bytes 81 write 1 write-bytes 21 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 21 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M102 acceptance evidence:

- command: `linux /APPS/DYNPIPE`
- staged app artifact: `/APPS/DYNPIPE`, SHA-256 `4DCE063D838B8E88CABB6E00C3A0206FCDD1AF8FBEA46F4F2ABE1A79193F2FB1`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- implementation scope: expose existing kernel ABI syscall `pipe(22)` through the generated libc shim at late stub RVA `0x1D78`, the final 8-byte slot before the dynamic table; no new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, or filesystem mutation was added
- final reserves after the M102 build: UEFI reserve 794,400 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,302,752; checksum `0x3603674C`; SHA-256 `f543ef66788d33a2a2571e2bfb510e9e09fdb74fd6546b05b4735bf92db4a0c2`
- visible output: `dynpipe:hello`
- proof telemetry excerpt: `path /APPS/DYNPIPE source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol pipe dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-reloc-apply-jmprel-readback 0x0000000047811D78 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 14 dynamic-exit-code 0x00000000 read 1 read-bytes 5 write 2 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-bytes 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 14 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0`

M103 acceptance evidence:

- command: `linux /APPS/DYNFORKPIPE`
- staged app artifact: `/APPS/DYNFORKPIPE`, SHA-256 `C3271AC8E598AEEF0480FE042FA1E6A51B64319B46736510D0FC2A43E154C696`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010A0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- staged interpreter candidate: `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`
- first trace before accepted ordering: a parent-read-before-wait smoke exited 5 with `pipe-blocks 1`, `pipe-wakes 1`, `fd-fork-pipe-copy 2`, `fork-success 1`, `read 0`, and `wait4 0`; this exposed that blocked pipe-read replay currently returns to user space as a failed/empty read path and is deferred to M104 rather than hidden inside M103
- implementation scope: expose existing kernel ABI syscalls `fork(57)` and `wait4(61)` through generated libc stubs at `0x1D80` and `0x1D88`, shift the shim dynamic table from `0x1D80` to `0x1DA0` inside the same executable/readable one-page PT_LOAD window, and keep the existing kernel fork, fd inheritance, wait4, pipe, read/write/close, and cleanup paths unchanged
- final reserves after the M103 build: UEFI reserve 794,400 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,302,752; checksum `0x504958C8`; SHA-256 `4ea863a0470fe43f0555e3fd8a7b60a6743d9fb6d01e146535c8bddde33eab75`
- visible output: `dynforkpipe:child-pipe`
- proof telemetry excerpt: `path /APPS/DYNFORKPIPE source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-rela 4 dynamic-jmprel 7 dynamic-binding-total 11 dynamic-binding-supported 7 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 7 dynamic-jmprel-symbol pipe dynamic-reloc-apply 1 dynamic-reloc-apply-total 11 dynamic-reloc-apply-write 11 dynamic-reloc-apply-readback 11 dynamic-reloc-apply-jmprel-readback 0x0000000047811D78 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010A0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 read 1 read-bytes 10 write 2 write-bytes 33 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe-live-final 0 fd-fork-pipe-copy 2 fd-fork-pipe-denial 0 fork 1 fork-success 1 fork-enosys 0 fork-denial 0 fork-child-slot 1 fork-child-root-distinct 1 wait4 1 wait4-reap 1 wait4-last-exit-code 7 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1`

M104 acceptance evidence:

- command: `linux /APPS/DYNFORKPIPE`
- staged app artifact: `/APPS/DYNFORKPIPE`, SHA-256 `0D9E3DDCC388F9609BCF31BCCBF646D8C3DFFF0D65CE040642102206EB87FCC2`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- implementation scope: blocked Linux pipe-backed `read`/`write`/`readv`/`writev` syscalls now save replayable return frames; pipe wake uses a preserve-frame scheduler wake so the blocked syscall re-executes under the blocked task's process root rather than returning a fabricated zero or `EAGAIN`
- final reserves after the M104 build: UEFI reserve 794,400 bytes; BIOS reserve 101 sectors
- visible output: `dynforkpipe:child-pipe`
- proof telemetry excerpt: `elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 6 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-binding-total 11 dynamic-binding-supported 7 dynamic-reloc-apply-total 11 dynamic-reloc-apply-write 11 dynamic-stack 1 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010C0 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 read 1 read-bytes 10 write 2 write-bytes 33 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe-live-final 0 pipe-blocks 1 pipe-wakes 1 pipe-replays 1 fd-fork-pipe-copy 2 fork 1 fork-success 1 wait4 1 wait4-reap 1 wait4-last-exit-code 7 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1`

M105 acceptance evidence:

- command: `linux /APPS/DYNPIPECLOSE`
- staged app artifact: `/APPS/DYNPIPECLOSE`, SHA-256 `93986327A92D798F613F931D93F483CCED8DED838BE78BF7D65324F5FCA2C628`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`
- implementation scope: no kernel changes were required after M104; the gate traces and proves existing pipe close, blocked-read replay, SIGPIPE marking, signal delivery, `rt_sigreturn`, fork, wait4, and cleanup paths under a dynamic ET_EXEC artifact
- final reserves after the M105 gate build: UEFI reserve 794,400 bytes; BIOS reserve 101 sectors
- UEFI manifest: kernel bytes 1,302,752; checksum `0x65F58141`; SHA-256 `cfb011d1dcac5f7ee98fb06271140f37609420a55c26b5d28fe4ed8e12db18a0`
- visible output:

```text
dynpipeclose:eof
sigpipe-caught
dynpipeclose:done
```

- proof telemetry excerpt: `elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 6 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-binding-total 11 dynamic-binding-supported 7 dynamic-reloc-apply-total 11 dynamic-reloc-apply-write 11 dynamic-stack 1 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010C0 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 50 dynamic-exit-code 0x00000000 signal-sigpipe 1 signal-sigchld 1 signal-rt-sigreturn 1 signal-frame-fault 0 read 1 read-bytes 0 write 3 write-bytes 50 pipe 2 pipe-create 2 pipe-denials 0 pipe-faults 0 pipe-live-final 0 pipe-blocks 1 pipe-wakes 1 pipe-replays 1 pipe-provider-denials 1 fd-fork-pipe-copy 2 fd-fork-pipe-denial 0 fork 1 fork-success 1 wait4 1 wait4-reap 1 wait4-last-exit-code 7 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 50 exit 0 cleanup 1`

Proposed next milestone: M106 `dynamic blocked pipe writer close/error path`. M106 should trace and then prove the complementary pipe edge case: a dynamic writer blocked on a full pipe is woken when the last reader closes, observes `SIGPIPE`/`EPIPE` without leaking the blocked task, and leaves `pipe-live-final 0`, `pipe-replays >=1`, `signal-sigpipe 1`, `page-faults 0`, and `exit 0`.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifacts:

- BIOS kernel: `KERNEL64-BIOS.BIN`
- BIOS kernel bytes: 472160
- BIOS kernel sectors: 923 / 1024
- BIOS reserve: 101
- BIOS checksum: recorded in the generated artifact inventory
- UEFI kernel: `KERNEL64.BIN`
- UEFI kernel bytes: 1,298,656
- UEFI kernel byte limit: 2,097,152 bytes
- UEFI byte reserve: 798,496
- UEFI checksum: `0x256CFA6D`
- UEFI SHA-256: `799b084e04e60402555bd1e230c87c692bcacd8099560528d125fcf8ab300d79`
- BIOS sector budget status: warning threshold below 128 reserve sectors, hard 1024-sector loader limit still satisfied
- boot contract: split path. BIOS keeps the 1024-sector hard limit and 128-sector warning. UEFI Product uses a 2 MiB `KERNEL64.BIN` file-size contract verified against `BOOTMAN.TXT` byte count and `fnv1a-32` loader checksum, with no UEFI sector arithmetic. `BOOTMAN.TXT` also records `kernel-sha256` for external artifact verification.
- UEFI handoff: fixed low pages are preferences only. The loader verifies/allocates through the UEFI memory map, supports dynamic linked-kernel and handoff-table fallback, and reports allocation name/status/conflict/fallback diagnostics instead of silently freezing.

Product behavior:

- x86_64 boot through disk, UEFI removable media, and UEFI ISO verification
- UEFI Product login gate before desktop/session access
- first-run setup for one local user when `/USERDB.TXT` is missing
- bcrypt `$2b$` password hash persisted in brokered NVMe FAT storage
- `lock` command and Settings lock path return to the login screen and resume the session after unlock
- persistent ring-3 shell
- Product desktop with brokered compositor/window-manager authority
- truthful `help`, `apps`, and `ls apps` output
- brokered DHCP/DNS/TCP/HTTP network status plus M19 restricted TCP-client socket status through the `net` shell builtin when virtio-net or e1000e hardware is present
- Terminal, File Manager, and Settings GUI apps opened through real click interaction
- Installer GUI app opened through the brokered desktop launcher
- Product service lifecycle/status query surface
- UEFI-only brokered network service endpoint and restricted socket syscall surface
- manifest-driven native app descriptor/binary build path and generic UEFI Product native app loader
- UEFI-only real static Linux ELF launch path through `linux <path> [args...]`, currently verified with `/APPS/BUSYBOX echo limitless-real-binary`, `/APPS/BUSYBOX cat /proc/meminfo`, `/APPS/BUSYBOX cat /nvme/apps/data/file.txt`, `/APPS/BUSYBOX ls /nvme/apps`, `/APPS/BUSYBOX ls -l /proc/self/exe`, `/APPS/BUSYBOX ls -l /proc/self/fd`, `/APPS/BUSYBOX ls -l /proc/self`, `/APPS/BUSYBOX sh`, `/APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'`, `/APPS/BUSYBOX sh -c 'echo hello | cat'`, `/APPS/BUSYBOX sh -c 'busybox echo m25-path-search'`, `/APPS/BUSYBOX sh -c 'ls /usr/bin'`, `/APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'`, `/APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'`, `/APPS/SBECHO m28-sbase-direct`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'`, `/APPS/SBCAT /nvme/apps/data/file.txt`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c 'PATH=/usr/local/bin:/bin:/usr/bin; sbecho m30-path-pipe | sbcat'`, `/APPS/BUSYBOX sh -c 'sbecho m31-default-path | sbcat'`, `/APPS/BUSYBOX sh -c 'sbecho $USER:$PWD | sbcat'`, `/APPS/BUSYBOX sh -c 'cd /nvme/apps; sbecho $PWD | sbcat'`, `/APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator /nvme/apps/sbenv | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator sbenv | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c 'sbenv USER=operator sbenv | sbcat'`, `/APPS/BUSYBOX sh -c 'ls /usr/local/bin'`, `/APPS/BUSYBOX sh -c '/usr/local/bin/sbenv USER=operator /usr/local/bin/sbenv | /usr/local/bin/sbcat'`, `/APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt | /usr/local/bin/sbcat'`, `/APPS/BUSYBOX sh -c 'cd /nvme/apps; /usr/local/bin/sbcat data/file.txt | /usr/local/bin/sbcat'`, and `/APPS/BUSYBOX sh -c 'cd /nvme/apps/data; /usr/local/bin/sbcat ../data/file.txt | /usr/local/bin/sbcat'`
- UEFI-only per-process page tables for Linux persona launches, backed by the fixed static PML4 pool, private lower-half user/VMA tables, shared higher-half kernel/MMIO mappings, scheduler CR3 switches at task start/swap/exit, and no low-identity compatibility mapping in current gates (`low-compat 0`)
- UEFI-only bounded Linux `fork(57)` plus blocking `wait4(61)` for the observed BusyBox ash external-command path, with parent/child PML4 slots allocated from the M22 root pool, full VMA/anonymous-page copy, duplicated FD/persona/VFS/audit state, child scheduler task registration, parent wait/reap, and final `pml4-pool-used-final 0`
- UEFI-only Linux `pipe(22)` over the fixed pipe provider, with exact-fd pipe endpoint inheritance across fork and final `pipe-live-final 0`; the M24 BusyBox ash proof forks both pipeline sides, so the accepted proof shape is `fork 2` and `fd-fork-pipe-copy 3`
- UEFI-only Linux VFS BusyBox applet aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin`, backed by the staged NVMe FAT `/APPS/BUSYBOX` artifact through the Linux-visible `/nvme/apps/busybox` path; `/usr` exposes fixed `bin` and `sbin` directory entries, and the real-binary summary reports `vfs-bin-*` telemetry for alias resolution
- UEFI-only Linux VFS third-party utility aliases under `/usr/local/bin`, currently `sbecho`, `sbcat`, and `sbenv`, backed by staged NVMe FAT `/APPS/SBECHO`, `/APPS/SBCAT`, and `/APPS/SBENV` through Linux-visible `/nvme/apps/sbecho`, `/nvme/apps/sbcat`, and `/nvme/apps/sbenv`; `/usr/local` exposes fixed directory entries, and the real-binary summary reports `vfs-localbin-*` telemetry for alias resolution
- UEFI-only default initial Linux environment for real launches with `PATH=/usr/local/bin:/bin:/usr/bin`, `HOME=/`, `USER=limitless`, and `PWD=/`, reported as `envc 4`; forked child `execve` calls inherit it and report `execve-last-envc 4`
- UEFI-only optional extra-app staging in `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1`, currently proven with `/APPS/SMOKE` staged from `external\build\zig-musl-smoke-imagebase` as a second static ET_EXEC artifact distinct from the BusyBox alias backend, with `/APPS/SBECHO` staged from a third-party suckless sbase 0.1 source build, with simultaneous `/APPS/SBECHO` plus `/APPS/SBCAT` staging through the second extra-app slot, and with simultaneous `/APPS/SBECHO`, `/APPS/SBENV`, and `/APPS/SBCAT` staging through the third extra-app slot
- UEFI-only scheduler FS-base save/restore/set state for Linux tasks, so `arch_prctl(ARCH_SET_FS)` TLS bases are task-local across CR3 switches, forked child exec, wait, and pipe scheduling
- exactly one authenticated local console session with input, display, filesystem, network-status, restricted socket-status, and installer-dry-run authority scoped to that session
- brokered persistent file workflow
- NVMe persistence verification path
- UEFI-only Ed25519 package archive and payload admission
- UEFI-only signed update-index verification with rollback denial
- read-only package trust visibility through Settings and `pkginfo`
- read-only hardware validation visibility through `hwval`
- read-only installer UX planning through the Installer app, Settings status, `pkginfo`, and verifier telemetry
- read-only identity/vault status through Settings
- read-only signed identity-provider descriptor and transport-safety status through Settings and `pkginfo`
- read-only account association status through Settings and `pkginfo`
- read-only cloud-storage broker and cloud-provider descriptor status through Settings, File Manager, and `pkginfo`
- read-only AI policy request/deny/audit status through Settings and `pkginfo`
- Assistant GUI app with consent-scoped read-only context flow and inference unavailable status
- capability denial checks
- no ambient authority

M10 login/authentication behavior:

- implementation: UEFI Product kernel and service layer only
- BIOS fallback: no login gate; runtime help labels UEFI login/session lock as unavailable
- local user store: `/USERDB.TXT` in the brokered persistent NVMe namespace
- password hash: bcrypt `$2b$` cost 04 using the public-domain `crypt_blowfish` verifier path
- first run: creates one local user record before the login screen
- login screen authority: brokered input plus compositor-owned display only
- pre-auth desktop/terminal/launcher access: blocked
- wrong password: denied visibly
- rate limit: three consecutive failures trigger a displayed 30-second lockout
- session authority: granted only after authentication and scoped to the local console session
- lock/unlock: preserves the session and windows, then resumes after the correct password
- unavailable: multiuser account management, password-change UI, PAM/LDAP, and remote auth

M11 identity/vault behavior:

- implementation: UEFI Product identity/status service and Settings surface; BIOS fallback remains lean and truthfully omits UEFI-only identity services
- active account type: local
- supported account types in the model: local, personal, enterprise
- unavailable account types: personal and enterprise
- local association: active and bound to the authenticated local console session
- cloud account association: planned/unavailable
- cloud storage: planned/unavailable
- security-key login: planned/unavailable
- remote login: unavailable
- secret vault status: foundation metadata only
- encrypted-at-rest secret storage: unavailable/non-product in M11
- real secrets/tokens stored: false
- Settings identity panel: read-only status only
- shell identity command: not added in M11; Settings is the Product identity status surface
- denials: identity mutation without authority, secret read without vault authority, secret write without vault authority, token storage, cloud association, personal login, enterprise login, ambient identity, and ambient secret authority

M12 identity transport behavior:

- implementation: UEFI Product identity transport broker state and Settings/pkginfo read-only surfaces; BIOS fallback remains lean and does not expose UEFI-only identity transport as Product behavior
- mode: Mode B endpoint trust foundation only
- provider descriptor: signed deterministic local fixture, no public internet dependency
- descriptor fields: provider id/type/display name, descriptor/protocol version, fixture endpoint, endpoint key id/fingerprint, supported auth methods, required transport security, token persistence policy, minimum OS version, sequence/generation, trusted-time/expiry metadata, signer key id, and signature
- trusted endpoint status: descriptor-verified fixture endpoint only
- encrypted identity transport: unavailable/non-product
- plaintext credential transport: denied
- credential transport to unverified endpoints: denied
- token storage: denied while vault remains Mode B
- account association: planned/unavailable until M13
- trusted time: unavailable/non-product; descriptor expiry metadata is not Product-enforced without trusted time
- denials: missing signature, invalid signature, wrong key, tamper, rollback, unsupported descriptor version, missing identity-network authority, plaintext credential transport, unverified endpoint credential transport, token storage, personal login, enterprise login, cloud association, ambient network, ambient identity, and ambient secret authority

M13 account association behavior:

- implementation: UEFI Product account association state and Settings/pkginfo read-only surfaces; BIOS fallback remains lean and reports account association unavailable
- mode: Mode B association policy/status only
- association records: local, personal, and enterprise
- local association: active/offline-capable and bound to the authenticated local console user
- personal association: planned/unavailable
- enterprise association: planned/unavailable
- cloud association: planned/unavailable
- security-key login: planned/unavailable
- encrypted identity transport: unavailable/non-product
- credential transport: denied
- token storage: denied while the vault remains Mode B
- enterprise policy: planned/unavailable
- remote account authority: none
- Settings account panel: read-only status only
- shell account status: `pkginfo` read-only text; no mutation commands
- denials: account mutation without authority, account unlink without authority, credential transport, token storage, cloud association, enterprise policy, remote ambient authority, ambient account identity, ambient account network, and ambient account secret authority

M14 cloud-storage broker behavior:

- implementation: UEFI Product cloud-storage broker state and read-only Settings/File Manager/pkginfo surfaces; BIOS fallback remains lean and reports cloud storage unavailable
- broker mode: Product foundation active, policy/status only
- provider descriptor: signed deterministic local fixture, no public cloud dependency
- descriptor fields: provider id/type/display name, descriptor/protocol version, fixture endpoint, endpoint key id/fingerprint, supported modes, token policy, offline-cache policy, sync policy, required transport security, required account association, minimum OS version, sequence/generation, trusted-time/expiry metadata, signer key id, and signature
- cloud account status: unavailable/planned
- cloud association status: unavailable/planned
- token storage: denied while vault remains Mode B
- encrypted cloud transport: unavailable/non-product
- sync: unavailable
- upload/download: denied
- automatic upload/download: unavailable
- offline cache: planned/unavailable
- AI cloud access: unavailable
- File Manager cloud surface: read-only unavailable-status area only; no cloud namespace is exposed
- denials: missing signature, invalid signature, wrong key, tamper, rollback, unsupported descriptor version, malformed descriptor, upload, download, sync, Settings mutation, File Manager mutation, app direct cloud authority, AI cloud access, ambient cloud, ambient filesystem, ambient network, ambient identity, and ambient secret authority

M7.1 package behavior:

- implementation: UEFI Product kernel only; BIOS Product remains checksum-only fallback
- package signing: Ed25519 detached signatures over the generated bootstrap archive and payload records
- trusted key: public verification key embedded in the UEFI Product kernel
- private key: generated during build signing only, not committed, not emitted into Product artifacts, and scanned out of kernel/ISO/image artifacts
- admission checks: signed archive, signed payload, checksum match, trusted signing key, manifest integrity, payload integrity, manifest version, package identity/version policy, scoped install token, owner, stale token, and capability policy
- denial telemetry: missing signature, invalid signature, wrong key, manifest tamper, payload tamper, checksum mismatch, unsupported manifest version, duplicate package, downgrade, wrong owner, stale token, denied capability request, malformed package, oversized package, install without install capability, rollback index, replay handling, and no ambient install/update
- update check: signed local update-index fixture verified through Product telemetry; unsigned, tampered, wrong-key, rollback, and replay fixtures are denied or handled deterministically
- live public update fetching: unavailable/non-product
- auto-install: unavailable
- package-manager GUI: unavailable
- trusted time: unavailable; expiry metadata is not Product-enforced without a trusted time source

M8 package trust UX behavior:

- Settings shows a read-only package trust panel
- `pkginfo` shows package mode, format version, trusted public key id/fingerprint, signed package count, signature/hash status, capability request/admission/denial status, local signed update-index status, rollback/replay handling, no-auto-install status, public update fetch status, trusted-time expiry status, and install/update authority status
- Settings and `pkginfo` do not receive install capability, update-apply capability, filesystem write authority, raw network authority, or raw package mutation authority
- install/apply actions are unavailable in M8
- live public update fetching remains unavailable/non-product
- trusted-time expiry remains unavailable/non-product without a trusted time source

M9 real-hardware validation:

- MSI Cyborg 15 A13VE checklist: `docs/hardware/msi-cyborg-15-a13ve.md`
- status: pending user-supplied physical results
- required boot mode: UEFI USB
- unsafe internal partitions must not be browsed or written
- internal NVMe writes remain disabled unless a later safe installer path explicitly enables them through scoped authority
- Product command: `hwval`
- `hwval` is read-only and reports boot path, framebuffer status, input backends, xHCI, PS/2 fallback, APIC, PCI/ECAM, NVMe, AHCI, network, package trust, installer dry-run status, and disabled install/write/format/NVRAM state
- MSI dry-run parser: `tools\parse-msi-dryrun-evidence.ps1`
- parser output records detected disks, GPT partitions, type GUIDs, labels, filesystem signatures, forbidden partitions, LimitlessOS target candidates, write-disabled status, dry-run no-write status, and recommended next step

M5 installer behavior:

- implementation: `tools\limitless-installer.ps1`
- fixtures: `tools\generate-installer-fixtures.ps1`
- verifier: `tools\verify-installer-m5.ps1`
- safety doc: `docs/installer/m5-safe-installer.md`
- dry-run lists disks/images, GPT partitions, type GUIDs, labels, filesystem signatures, markers, classification, and zero-write plan
- explicit physical dry-run path: `-PhysicalDriveNumber N -AllowPhysicalReadOnly -Mode DryRun`
- install/write mode is image-fixture-only in M5
- Windows ESP, NTFS, MSR, Recovery, unknown FAT32, and unknown GPT targets are refused
- writes require hardware inventory, read-only block, write, and format capability flags plus a confirmation token
- boot-entry changes require a separate boot-entry capability and are not performed by the default M5 path
- real MSI internal NVMe writes remain disabled by default until dry-run output is reviewed

M6 service/session behavior:

- design note: `docs/service-session-m6.md`
- service lifecycle states are modeled as declared, admitted, launching, running, degraded, stopping, stopped, crashed, restarting, and denied
- Product services are policy/security broker, console/shell broker, input broker, display/compositor, window manager/desktop shell, filesystem broker, block/storage broker, hardware inventory broker, network broker, brokered socket foundation, native app SDK foundation, installer dry-run service/tool, settings/system-info provider, cloud-storage broker foundation, and installer UX planning
- Settings is the Product-safe status surface for service/session information
- controlled restart verification is limited to the scoped settings/system-info provider path
- stale capabilities from the old generation are denied
- M6 introduced one local console session; M10 authenticates that single local console user but still does not implement full multiuser account management
- M11 associates the authenticated session with a local identity record and vault metadata foundation without adding remote identity, personal account login, enterprise account login, cloud storage, or token storage
- M12 adds signed local identity-provider descriptor verification and read-only identity transport status without adding remote login, account association, encrypted credential transport, or token persistence
- M13 records local account association as active and labels personal, enterprise, cloud, security-key, credential, token, and enterprise-policy paths unavailable without adding account linking or remote login
- M14 records cloud-storage policy state and labels real cloud storage, sync, upload/download, token storage, encrypted transport, offline cache, AI cloud access, and app-direct cloud authority unavailable or denied
- M15 records installer planning state and labels real internal install/write, formatting, boot-entry changes, package install/apply, cloud enablement, and AI-assisted setup unavailable or denied
- wrong-session input, display, and filesystem delivery are denied
- raw input, direct framebuffer, ambient filesystem, and ambient network access remain denied

Product apps:

- APPEND
- CAT
- COPY
- DELETE
- LS
- MKDIR
- MOVE
- NETHELLO
- RENAME
- STAT
- TOUCH
- WRITE

Product builtins:

- apps
- help
- hwval
- info
- lock (UEFI Product only; BIOS fallback reports unavailable)
- net
- pkginfo
- pwd

Product GUI apps:

- Terminal
- File Manager
- Settings
- Installer
- Assistant

Product GUI authority model:

- raw keyboard and mouse enter only through the brokered input path
- input is routed only to the active local console session
- the window manager owns hit-testing, focus, drag, close, taskbar focus, launcher dispatch, and z-order
- the compositor owns physical framebuffer presentation
- Terminal receives keyboard only while focused, with unfocused terminal delivery denied
- apps draw only through delegated window surfaces in their session
- File Manager is limited to RAMFS, boot-media read-only areas, and explicitly brokered persistent namespaces
- Settings receives read-only display/input/network/storage/profile/boot metadata and cannot write configuration
- GUI apps do not receive ambient framebuffer, raw input, filesystem, storage, or network authority

Unavailable or not product-path in Product:

- ASK, not AI and no consent-gated assistant path
- ECHO
- shell aliases SAY, SHOW, LIST, MAKE, PUT, SWAP, SHIFT
- real internal install/write authority
- formatting authority
- boot-entry authority
- package install/update actions
- app store
- auto-install
- live public update fetch
- general socket library
- server sockets
- raw packet API
- arbitrary app network send/receive
- multiuser account management UI
- password change UI
- PAM/LDAP/remote auth
- AI inference backend
- AI-generated answers
- AI model transport
- personal account login
- enterprise account login
- account linking
- cloud account association
- real public cloud storage
- cloud sync
- automatic cloud upload/download
- offline cloud cache
- AI cloud access
- app-direct cloud authority
- security-key login
- remote login
- encrypted-at-rest secret storage
- token storage
- encrypted identity transport
- credential transport
- enterprise policy enrollment

## Experimental Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental
```

Current Experimental artifact:

- produced by `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental`
- recorded in `dist\limitlessos-x86_64.experimental.m6.json` when the Experimental profile is built
- not Product and not a daily-driver surface

Experimental may initialize proof surfaces beyond the M4 Product GUI and broad hardware proof telemetry. These are not Product behavior and must remain labeled experimental or proof-only in logs and reports.

## Verification

Final M2 evidence pack:

- `dist/m2-evidence-20260511-151717/m2-evidence.json`

The pack records command, exit code, elapsed time, output file, build profile, kernel bytes/sectors/reserve/checksum, final ISO path, and artifact inventory JSON for:

- Experimental build
- Experimental UEFI verification
- Product build
- Product M1/M2 production-slice assertion
- Product disk verification
- Product UEFI verification
- Product ISO verification
- Product UEFI e1000e verification
- Product ISO e1000e verification
- Product NVMe persistence verification

All recorded commands exited 0.

M3 evidence pack:

- `dist/m3-evidence-20260511-163350/m3-evidence.json`
- created by `.\tools\archive-m3-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M3 artifact inventory JSON
- all recorded commands exited 0

M4 evidence pack:

- `dist/m4-evidence-20260511-190105/m4-evidence.json`
- generated by `.\tools\archive-m4-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence commands
- records a dedicated Product GUI interactive verification command
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M4 artifact inventory JSON
- all recorded commands exited 0

M4.1 evidence pack:

- `dist/m4-1-evidence-20260511-192857/m4-1-evidence.json`
- generated by `.\tools\archive-m4-1-evidence.ps1`
- archives the accepted M4 verifier outputs instead of rerunning M4
- archives the MSI Cyborg 15 A13VE checklist template
- records current kernel budget and git status
- records physical validation as pending until the user supplies real-hardware results

M5 installer verification:

- evidence pack: `dist/m5-evidence-20260511-194236/m5-evidence.json`
- command: `.\tools\verify-installer-m5.ps1`
- proves dry-run no-writes
- proves Windows-like ESP/MSR/NTFS/Recovery refusal
- proves unknown FAT32 and unknown GPT refusal
- proves dedicated LimitlessOS target acceptance
- proves scoped write/format capability and confirmation-token requirements
- proves boot-entry authority separation
- proves valid fixture install verifies markers/manifests and leaves forbidden partitions unchanged
- all installer verifier predicates must pass before M5 is accepted

M6 evidence pack:

- generated by `.\tools\archive-m6-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence/gui/installer commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M6 artifact inventory JSON and service/session status output
- records git status and current commit at archive time

M7 evidence pack:

- generated by `.\tools\archive-m7-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence/gui/installer/service-session/signing commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M7 artifact inventory JSON, split-kernel budget, package fixture status, private-key artifact scan status, and git status

M7.1 evidence pack:

- generated by `.\tools\archive-m7-1-evidence.ps1 -IncludeExperimental`
- records all preserved M7 commands plus dedicated Product package negative fixture verification, update-index negative fixture verification, and private-key artifact scan
- includes the final M7.1 artifact inventory JSON, split-kernel budget, deterministic fixture status, live public update fetching status, and git status
- live public update fetching remains unavailable/non-product and no auto-install path exists

M8 evidence pack:

- generated by `.\tools\archive-m8-evidence.ps1 -IncludeExperimental`
- records Product package UX/trust-surface verification plus all preserved M7.1 fixture checks
- includes read-only Settings and `pkginfo` package trust surfaces
- install/apply actions, auto-install, public update fetching, and trusted-time expiry remain unavailable

M9 evidence pack:

- generated by `.\tools\archive-m9-evidence.ps1 -IncludeExperimental`
- records Product hardware validation mode, MSI checklist, dry-run parser, and preserved M8 verification matrix
- physical MSI Cyborg 15 A13VE evidence remains pending user-provided results

M10 evidence pack:

- generated by `.\tools\archive-m10-evidence.ps1 -IncludeExperimental`
- records Product login gate, first-run setup, bcrypt `$2b$` user store, wrong-password denial, rate limiting, lock/unlock, and scoped session authority
- BIOS fallback labels login/session lock unavailable

M11 evidence pack:

- generated by `.\tools\archive-m11-evidence.ps1 -IncludeExperimental`
- records Product identity/vault foundation verification plus all preserved M10 verification commands
- proves local account active, personal/enterprise unavailable, Settings identity panel, read-only identity status, identity mutation denial, vault foundation, denied secret read/write, no plaintext token storage, cloud association unavailable, and no ambient identity or secret authority

M12 evidence pack:

- generated by `.\tools\archive-m12-evidence.ps1 -IncludeExperimental`
- records Product identity transport verification plus all preserved M11 verification commands
- proves signed local provider descriptor acceptance, missing/invalid/wrong-key/tampered/rollback/unsupported descriptor denials, scoped network requirement, plaintext credential denial, unverified endpoint denial, token-storage denial while vault is Mode B, Settings identity transport panel visibility, read-only status, trusted-time status, and no ambient identity-transport network/identity/secret authority

M13 evidence pack:

- generated by `.\tools\archive-m13-evidence.ps1 -IncludeExperimental`
- records Product account association verification plus all preserved M12 verification commands
- proves local association active/offline-capable, personal/enterprise/cloud/security-key unavailable, credential transport denied, token storage denied while vault remains Mode B, enterprise policy unavailable, account mutation/unlink denied, remote account no-ambient-authority denial, Settings/pkginfo read-only status, and no ambient account identity/network/secret authority

M14 evidence pack:

- generated by `.\tools\archive-m14-evidence.ps1 -IncludeExperimental`
- records Product cloud-storage broker verification plus all preserved M13 verification commands
- proves signed local cloud-provider descriptor acceptance, missing/invalid/wrong-key/tampered/rollback/unsupported/malformed descriptor denials, Settings/File Manager/pkginfo read-only status, upload/download/sync denial, automatic upload/download unavailability, AI cloud access unavailability, app-direct cloud authority denial, and no ambient cloud/filesystem/network/identity/secret authority

M15 evidence pack:

- generated by `.\tools\archive-m15-evidence.ps1 -IncludeExperimental`
- records Product installer UX verification plus all preserved M14 verification commands
- proves Installer GUI entry, welcome/beginner/advanced/hardware/recommendation/component/account/cloud/AI/plan/dry-run surfaces, zero planned writes/formats/boot-entry/package operations, forbidden target denial, write/format/boot-entry/package/cloud/AI action denial, and no ambient installer/storage/firmware/package/identity-cloud-secret authority

M16 evidence pack:

- generated by `.\tools\archive-m16-evidence.ps1 -IncludeExperimental`
- records Product AI policy verification plus all preserved M15 verification commands
- proves AI principal creation, action request modeling, consent required, no-consent denial, scope validation, invalid-scope denial, immutable/queryable audit visibility, Settings/pkginfo read-only status, zero AI actions executed, zero default AI capabilities, and no ambient AI/filesystem/network/settings/package/secret/cloud authority

M17 evidence pack:

- generated by `.\tools\archive-m17-evidence.ps1 -IncludeExperimental`
- records Product AI Assistant read-only verification plus all preserved M16 verification commands
- proves Assistant app opens after login, pre-auth access is blocked, backend mode is Mode B, default AI capabilities remain zero, context requests require consent, denied requests receive no data, allowed requests receive only scoped read-only status context, invalid/broad filesystem/secret/cloud scopes are denied, file write/settings/package mutation and network/model access are denied or unavailable, stale and wrong-session grants are denied, audit is queryable, Settings shows the AI panel, actions/automation/cloud memory remain unavailable, Assistant self-modification is denied, package integrity is checked, no model call occurs, and no scripted answer is presented as AI output

M18 evidence pack:

- generated by `.\tools\archive-m18-evidence.ps1 -IncludeExperimental`
- records Product AI consent-scoped action verification plus all preserved M17 verification commands
- proves the Mode B action broker creates predefined action requests, requires explicit consent, grants only scoped action-bound/session-bound capabilities, writes and commits `/HOME/ASSIST/NOTE.TXT`, verifies readback, runs installer dry-run with no writes, opens read-only Settings/package status surfaces, denies arbitrary/path-traversal writes, stale grants, wrong-session grants, settings mutation, package install/update, cloud enablement, secret access, self-modification, model calls, generated-answer behavior, autonomy, and ambient AI filesystem/installer/settings/package/cloud/secret/network authority

M18.1 evidence pack:

- generated by `.\tools\archive-m18-1-evidence.ps1 -IncludeExperimental`
- records the UEFI loader compatibility closure plus all preserved M18 verification commands
- proves QEMU UEFI and ISO still reach ExitBootServices, x64 kernel entry, PIT, login shell, and M18 telemetry with dynamic linked-kernel and boot-handoff proof predicates
- records that automated VirtualBox verification is unavailable in this workspace when `VBoxManage` is absent
- includes the manual VirtualBox EFI checklist at `docs/hardware/virtualbox-uefi-m18-1.md`
- keeps MSI physical validation pending user evidence

M19 brokered socket verification:

- generated by `.\tools\verify-network-socket-m19.ps1`
- records Product UEFI network verification with virtio-net by default, or e1000e/e1000 when requested
- proves the network service endpoint is registered, a network service capability is required and minted only through the broker, no-cap and wrong-owner socket opens are denied, raw sockets and listen sockets are denied, send without broker data-plane authority is denied, one TCP-client status handle can be represented over the existing DHCP/DNS/HTTP path, recv-status reports the broker-owned HTTP status/byte count, close retires the handle, no socket remains open after verification, and filesystem/storage/ambient network authority stay zero

M21 native app SDK verification:

- generated by `.\tools\verify-native-app-sdk-m21.ps1`
- reads `apps/native/nethello.json`, assembles the declared native app binary, generates `/APPS/NETHELLO.APP`, stages `/APPS/NETHELLO.BIN`, and signs/verifies payload slot 13 through package metadata
- validates descriptor metadata for `name`, `binary`, `payload-slot`, `entry-result`, `success-result`, and `capabilities`, maps the loaded binary into Ring-3 user memory, and runs it through the native user-entry path
- proves the app can print through delegated console authority, request the declared network service capability, use the M19 brokered socket status path through syscall, observe send denial, close the socket, receive denial for undeclared filesystem/block capabilities, and finish with `drs-app-m21`

Real static Linux ELF gate verification:

- generated by `.\tools\verify-real-binary-gate.ps1 -BusyBoxPath <external-busybox> -BusyBoxSource <source> -BusyBoxVersion <version>`; use `-RequireLowAddressNegative` when the upstream-default-address BusyBox artifact is present and that denial proof must be mandatory, use `-RequireShellCwdLoop` to require BusyBox ash cwd/path traversal proof, use `-RequireRelativePathProof` to require relative `.`/`..` path canonicalization proof, use `-RequireProcSymlinkProof` to require `/proc/self/exe` readlink proof, use `-RequireProcFdProof` to require stable `/proc/self/fd` link proof, use `-RequireProcSelfProof` to require mixed `/proc/self` pseudo-file, directory, and symlink listing proof, and use direct `verify-qemu.ps1 -RealBinaryGate -ExtraShellLine` runs for the M23 fork/wait shell-external-command, M24 pipeline, M25 Linux VFS path-execution, and M26 forked-child exec inheritance acceptance paths
- stages the external binary into the generated NVMe FAT image at `/APPS/BUSYBOX` without committing or embedding the binary in the repository
- records source, version, SHA-256, byte count, `file`, `readelf -h`, `readelf -l`, staged path, command, console output, exit code, the `/proc/meminfo` cat proof, the `/nvme/apps/data/file.txt` NVMe FAT cat proof, optional `nvme/apps/./data/../data/file.txt` relative cat proof, optional `ls -l nvme/apps/./data/..` relative metadata/listing proof, optional `ls -l /proc/self/exe` proc symlink proof, optional `ls -l /proc/self/fd` proc-fd proof, optional `ls -l /proc/self` mixed proc listing proof, the `/nvme/apps` `ls` proof, the `sh` banner/prompt proof, optional cwd-loop proof, M23 direct fork/wait shell telemetry, M24 pipe telemetry, M25 execve/VFS-alias telemetry, M26 forked-child exec inheritance telemetry, and `drs-realbin` telemetry in `build\real-binary-gate-provenance.txt`
- passing artifact 1: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, SHA-256 `5CDE8968EB2FEDB62DEA27947CD269BC57AD9C8B142ABFF0C3B1514A0238E8D9`; verified with `echo limitless-real-binary`, `cat /proc/meminfo`, and `cat /nvme/apps/data/file.txt`
- passing artifact 2: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-ls`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, and `sh`, SHA-256 `299DE064F51DA04DE99227F26F2EAB60C95F400C1B83731E14E3E28F86695652`; verified with `ls /nvme/apps` and `sh`
- passing artifact 3: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, `sh`, `FEATURE_PREFER_APPLETS`, `FEATURE_SH_STANDALONE`, `FEATURE_SH_NOFORK`, and a gate-local `cat` NOEXEC applet patch, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`; verified with the default real-binary gate including `echo`, `cat`, `ls`, the bounded `sh` builtin loop, the M22 CR3 isolation command, the M23 fork/wait external-command shell path, the M24 pipeline path, the M25 Linux VFS path-execution path, and the M26 forked-child exec inheritance path
- proves the UEFI Product shell commands `linux /APPS/BUSYBOX echo limitless-real-binary`, `linux /APPS/BUSYBOX cat /proc/meminfo`, `linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt`, `linux /APPS/BUSYBOX ls /nvme/apps`, `linux /APPS/BUSYBOX ls -l /proc/self/exe`, `linux /APPS/BUSYBOX ls -l /proc/self/fd`, `linux /APPS/BUSYBOX ls -l /proc/self`, `linux /APPS/BUSYBOX sh`, `linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'`, `linux /APPS/BUSYBOX sh -c 'echo hello | cat'`, `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'`, `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'`, and `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'` load the ELF from NVMe, reject `PT_INTERP`/dynamic inputs, map nonzero ELF pages, allocate the 64 KiB real-launch stack, start scheduler tasks, print through the brokered console, read a Linux VFS proc file with EOF behavior, read the nested NVMe FAT fixture through the broker-scoped Linux VFS `/nvme` provider, enumerate `/nvme/apps`, `/nvme/apps/data`, `/proc/self/fd`, mixed `/proc/self` entries, and fixed Linux-visible BusyBox applet directories through `getdents64`, read proc symlink targets through `readlink(2)`, answer terminal-size probes through `TIOCGWINSZ`, consume bounded brokered stdin for `sh`, cross the BusyBox ash fork/wait shell external-command path, create and drain a Unix pipe across two forked pipeline children, execute real ET_EXEC images through Linux VFS PATH aliases, preserve cwd/VFS/pipe state across forked child execs, observe `exit_group`, and clean up VMA/FD/persona/audit/process/PML4/pipe state
- proven syscall surface for the passing artifacts: Linux process startup through `brk`, `mmap`, `mprotect`, `munmap`, `arch_prctl`, `set_tid_address`, `rt_sigaction`, and `rt_sigprocmask`; console and file I/O through `read`, `write`, `readv`, `writev`, `openat`, `close`; stat-family metadata through `stat`, `newfstatat` including `AT_EMPTY_PATH`, and `lstat`; proc symlink readback through `readlink`; directory enumeration through `getdents64`; current-working-directory state through `getcwd` and `chdir`; cwd-relative path canonicalization for `open`, `openat`, stat-family paths, and `chdir`, including `.` and `..`; terminal query `ioctl(TIOCGWINSZ)` for brokered terminal fds; fixed identity shims `geteuid` and `getppid`; BusyBox process-name shim `prctl(PR_GET_NAME)`/`PR_SET_NAME` with other `prctl` options still returning `ENOSYS`; bounded process creation and reaping through `fork(57)` and `wait4(61)` for the observed BusyBox ash external-command and pipeline paths; Unix pipe creation through `pipe(22)` with exact-fd endpoint inheritance across fork; real static ET_EXEC replacement through `execve` on Linux VFS paths backed by the staged NVMe BusyBox artifact; process termination through `exit_group`; unsupported syscall telemetry remains visible
- positive echo telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 239 stack 16 task 0 started 1 console-bytes 22 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0`
- positive NVMe file telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 239 stack 16 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 27`
- positive relative NVMe file telemetry from `-RequireRelativePathProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 27 exit 0 cleanup 1 ... path-relative 1 path-dot 1 path-dotdot 1 path-fault 0 ... read 2 read-bytes 27 write 1 write-bytes 27 ... vfs-nvme-reads 2 vfs-nvme-bytes 27`; visible output was `Nested FAT32 path fixture`
- positive relative NVMe directory metadata telemetry from `-RequireRelativePathProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 65 exit 0 cleanup 1 getdents64 2 getdents64-entries 2 getdents64-bytes 56 stat 3 stat-denial 0 stat-fault 0 ... path-relative 4 path-dot 4 path-dotdot 4 path-fault 0 ... writev 2 writev-bytes 65 ioctl 1 ioctl-tty 1 ... vfs-nvme-readdirs 4 vfs-nvme-dirents 2`; visible output from `ls -l nvme/apps/./data/..` was `-r--r--r--    1    145264 busybox` and `dr-xr-xr-x    2         0 data`
- positive proc symlink telemetry from `-RequireProcSymlinkProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 59 exit 0 cleanup 1 ... stat 1 stat-denial 0 stat-fault 0 ... readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 ... writev 1 writev-bytes 59 ... ioctl 1 ioctl-tty 1 ... unimplemented 0`; visible output from `ls -l /proc/self/exe` was `lrwxrwxrwx    1        14 /proc/self/exe -> /proc/self/exe`
- positive proc-fd telemetry from `-RequireProcFdProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 129 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 72 stat 4 stat-denial 0 stat-fault 0 ... readlink 3 readlink-bytes 33 readlink-denial 0 readlink-fault 0 readlink-last-result 11 ... writev 3 writev-bytes 129 ... unimplemented 0`; visible output from `ls -l /proc/self/fd` was `lrwxrwxrwx    1        11 2 -> anon:[fd 2]`, `lrwxrwxrwx    1        11 1 -> anon:[fd 1]`, and `lrwxrwxrwx    1        11 0 -> anon:[fd 0]`. The bounded proc-fd enumerator skips its own directory iterator fd so it does not publish a link that can disappear before BusyBox's later `readlink` pass.
- positive proc-self telemetry from `-RequireProcSelfProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 209 exit 0 cleanup 1 getdents64 2 getdents64-entries 6 getdents64-bytes 168 stat 7 stat-denial 0 stat-fault 0 ... readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 ... writev 6 writev-bytes 209 ... unimplemented 0`; visible output from `ls -l /proc/self` was `-r--r--r--    1         0 environ`, `-r--r--r--    1         0 cmdline`, `-r--r--r--    1        70 status`, `dr-xr-xr-x    2         0 fd`, `lrwxrwxrwx    1        14 exe -> /proc/self/exe`, and `-r--r--r--    1       384 maps`.
- positive NVMe directory telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 37 stack 16 task 0 started 1 console-bytes 14 exit 0 cleanup 1 getdents64 2 getdents64-entries 2 getdents64-bytes 56 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 4 vfs-nvme-dirents 2 vfs-nvme-bytes 0`; visible console output was `busybox` and `data`
- positive shell telemetry: `drs-realbin path /APPS/BUSYBOX ... console-bytes 90 exit 0 cleanup 1 ... read 40 read-bytes 40 write 2 write-bytes 20 readv 0 readv-bytes 0 writev 6 writev-bytes 70 ... geteuid 1 getppid 1 ioctl 3 ioctl-tty 3 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 ...`; visible console output includes `BusyBox v1.35.0 ... built-in shell (ash)`, `$ shellloop`, `$ $ aftertrue`, and the final `$` prompt
- positive cwd-loop shell telemetry from `-RequireShellCwdLoop`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 101 exit 0 cleanup 1 ... getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 chdir 2 fchdir 0 chdir-denial 0 chdir-fault 0 ... fork 0 fork-enosys 0 fork-denial 0 ...`; visible console output includes `$ shellloop`, `$ /`, `$ $ /nvme/apps`, and `$ $ /`.
- M23 shell external-command proof: `linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'` now crosses the previously traced BusyBox ash fork boundary at `fork-last-rip 0x000000005200EF74`. Visible output contains `busybox  data` and `file.txt`; telemetry reports `fork 1 fork-success 1 fork-enosys 0 fork-child-slot 1 fork-child-root-distinct 1 wait4 1 wait4-reap 1 wait4-last-exit-code 0 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 console-bytes 23 exit 0 page-faults 0 cleanup 1`.
- M23 implementation delta: the kernel/header portion is 19 files changed with 2,112 insertions and 33 deletions; the verifier semicolon injector prerequisite adds 1 line, for a total worktree delta of 20 files changed with 2,113 insertions and 33 deletions.
- M24 closure: the earlier `syscall-root-repair 1` watch item was fixed before the M24 gate and now remains `syscall-root-repair 0`; the accepted pipeline run exercises child-side pipe reads with `read 2 read-bytes 6`.
- M25 path-execution proof: `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'` prints `m25-path-search`, exits 0, and reports `execve 1 execve-denial 0 execve-fault 0 execve-last-binary-bytes 145264 execve-last-transfer-ready 1 vfs-bin-alias 3 vfs-bin-read 1 vfs-bin-denial 0 stat-denial 0 page-faults 0 syscall-root-repair 0 pml4-pool-used-final 0`.
- M25 applet-directory proof: `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'` prints `sh       true     ls       cat      echo     busybox`, exits 0, and reports `getdents64 2 getdents64-entries 6 getdents64-bytes 152 stat 7 stat-denial 0 writev 1 writev-bytes 53 vfs-bin-alias 6 vfs-bin-denial 0 page-faults 0 cleanup 1`.
- M26 forked-child exec inheritance proof: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'` prints `Nested FAT32 path fixture`, exits 0, and reports `chdir 1 path-relative 1 pipe 1 pipe-create 1 fork 2 fork-success 2 execve 2 execve-denial 0 execve-fault 0 wait4 2 wait4-reap 2 read 4 read-bytes 54 write 2 write-bytes 54 pipe-live-final 0 pml4-pool-used-final 0 page-faults 0 syscall-root-repair 0`.
- known real-binary gap: terminal support is intentionally narrow. `ioctl(TIOCGWINSZ)` succeeds for brokered terminal fds, while other terminal queries return `ENOTTY` and unrelated ioctl requests return `ENOSYS`; there is still no broad device-control API.
- `drs-realbin` NVMe VFS telemetry is launch-local: in a same-boot run, `linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt` reports `vfs-nvme-reads 2 ... vfs-nvme-bytes 27`, and a following `linux /APPS/BUSYBOX echo limitless-real-binary` reports `vfs-nvme-reads 0 ... vfs-nvme-bytes 0`
- read-stage failures now include concrete FAT read diagnostics; a missing `/APPS/BUSYBOX` reports `stage read code 5 ... nvme-read-error 5 nvme-read-bytes 0 nvme-read-capacity 4194304 nvme-read-size 0 nvme-read-attr 0x0000000000000000`
- negative coverage: missing `/APPS/BUSYBOX`, dynamic `PT_INTERP`, real upstream-default-address ET_EXEC below the low kernel window, oversized host artifact, and BIOS unavailable path
- low-address ET_EXEC negative telemetry with the upstream BusyBox artifact: `drs-realbin-fail path /APPS/BUSYBOX stage static code 20 ... load-first 0x0000000000400000 load-end 0x0000000000713FDD low-kernel-limit 0x0000000001000000`
- BIOS reserve at gate crossing: 101 sectors, below the 128-sector warning threshold but still inside the hard 1024-sector BIOS loader limit
- x86_64 ISO boot shape: UEFI-only optical media with one UEFI El Torito boot entry. `BOOTIMG.IMG` is a staged UEFI FAT image copy for inspection and is not wired as a legacy BIOS El Torito boot entry.
- UEFI boot manifest integrity: `BOOTMAN.TXT` records `kernel-checksum-algorithm=fnv1a-32`, the loader-enforced `kernel-checksum`, standard external `kernel-sha256`, and `non-product-package-registry-stubs=ASK,ECHO`.

M22 per-process page table verification:

- generated by `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'"`
- visible console output contains `m22-cr3-isolation`, exits with status 0, reports zero page faults, and cleans up the process PML4 slot
- static pool design: 4 UEFI-only process roots, 19 pages per root, 4 x 19 x 4096 = 311,296 bytes reserved for the PML4/PDPT/PD/PT tables; each root has private lower-half user and VMA page tables plus copied shared higher-half kernel/MMIO mappings
- transitional compatibility status: `low-compat 1` remains visible in telemetry while the low identity PDPT mapping is retained for M22 compatibility; this is not a final isolation claim below the low window
- UEFI reserve delta for the M22 implementation: 852,544 bytes at the first successful pool-stage build to 848,416 bytes at final acceptance; BIOS reserve remains 101 sectors
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204A000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 low-compat 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 18 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 1 write-bytes 18 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 0 fork-enosys 0 fork-denial 0 fork-last-rip 0x0000000000000000 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M23 fork/wait verification:

- generated by `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'"`
- visible console output contains `busybox  data` and `file.txt`, exits with status 0, reports zero page faults, and cleans up both parent and child PML4 slots
- implementation scope: bounded UEFI-only `fork(57)` for the observed BusyBox ash path, full child VMA/address-space copy, duplicated FD/persona/VFS/audit state, child scheduler task registration, blocking `wait4(61)` parent reaping, and final root-pool cleanup; no vfork, no broad clone flags, no COW, no dynamic linker, no real threading, and no signal delivery
- implementation delta: 19 kernel/header files changed with 2,112 insertions and 33 deletions; the prerequisite QMP keyboard semicolon injector changed 1 verifier line, for 20 total changed files with 2,113 insertions and 33 deletions
- final artifact budget: UEFI reserve 840,160 bytes; BIOS reserve remains 101 sectors
- M24 resolved the M23 watch item: `syscall-root-repair` is now 0 in the accepted pipeline proof, and child-side data movement is exercised through inherited pipe fds rather than only parent-side console/file output.
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204C000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 1 syscall-root-reload 61 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 2 task 0 started 1 console-bytes 23 exit 0 cleanup 1 getdents64 4 getdents64-entries 3 getdents64-bytes 88 stat 5 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 readv 0 readv-bytes 0 writev 2 writev-bytes 23 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 4 ioctl-tty 4 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 4 prctl-set-name 3 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 1 fork-success 1 fork-enosys 0 fork-denial 0 fork-child-slot 1 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 1 wait4-reap 1 wait4-last-exit-code 0 child-root-cleanup 1 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 7 vfs-nvme-dirents 3 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M24 pipe verification:

- generated by `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'echo hello | cat'"`
- visible console output is exactly `hello`, exits with status 0, reports zero page faults, keeps `syscall-root-repair 0`, and cleans up the parent plus two BusyBox ash pipeline children
- implementation scope: Linux `pipe(22)` ABI exposure over the existing fixed pipe provider, pipe-aware exact-fd inheritance in `fd64_fork_process`, target-fd endpoint grants through `pipe64_grant_endpoint_at`, and real-binary telemetry for pipe creation, denials, faults, provider denials, live pipe count, and fork-time pipe fd copies; `pipe2(293)` observable behavior is unchanged
- fixed pipe pool dimensions remain unchanged: `PIPE64_MAX_OBJECTS 16` and `PIPE64_BUFFER_BYTES 4096`
- BusyBox ash forks both sides of this pipeline, so the accepted proof shape is `fork 2 fork-success 2` and `fd-fork-pipe-copy 3`, not the earlier simplified single-fork expectation
- final artifact budget: UEFI reserve 836,064 bytes; BIOS reserve remains 101 sectors
- implementation delta before docs: 10 files changed with 444 insertions and 17 deletions, including the QMP `|` injector, syscall 22 dispatch, pipe fork inheritance, pipe telemetry, and the external BusyBox standalone-shell build-script provenance update
- passing BusyBox artifact: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, static musl ET_EXEC linked at `0x52000000`
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204D000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 60 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 6 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 6 write 2 write-bytes 12 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M25 Linux VFS path execution verification:

- generated by direct `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraShellLine ...` runs for `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'`, `linux /APPS/BUSYBOX sh -c '/bin/cat /nvme/apps/data/file.txt'`, `linux /APPS/BUSYBOX sh -c '/bin/echo hello | /bin/cat'`, and `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'`
- implementation scope: UEFI-only `execve` widening through Linux VFS file reads, real external ET_EXEC transfer via a consume-once syscall return frame, 64 KiB real-exec stack selection for large real binaries, exec-replaced VMA/FD/audit cleanup handoff, BusyBox applet aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin`, fixed `/usr` directory entries, QEMU verifier log waiting across debug and framebuffer logs, and real-binary telemetry for `execve-*` plus `vfs-bin-*`
- visible output proofs include `m25-path-search`, `Nested FAT32 path fixture`, `hello`, and the `/usr/bin` entries `sh`, `true`, `ls`, `cat`, `echo`, and `busybox`
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- implementation delta before docs: 8 files changed with 751 insertions and 34 deletions
- passing BusyBox artifact: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, static musl ET_EXEC linked at `0x52000000`
- `/usr/bin` proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 41 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 53 exit 0 cleanup 1 getdents64 2 getdents64-entries 6 getdents64-bytes 152 stat 7 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 pipe 0 pipe-create 0 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 0 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 4294967295 readv 0 readv-bytes 0 writev 1 writev-bytes 53 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 2 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 0 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 22 execve-last-binary-bytes 0 execve-last-closed-fds 0 execve-last-fd-live-before 0 execve-last-fd-live-after 0 execve-last-vma-before 0 execve-last-vma-released 0 execve-last-vma-after 0 execve-last-argc 0 execve-last-envc 0 execve-last-transfer-ready 0 execve-last-transfer-rip 0x0000000000000000 execve-last-transfer-rsp 0x0000000000000000 fork 0 fork-success 0 fork-enosys 0 fork-denial 0 fork-child-slot 4294967295 fork-child-root-distinct 0 fork-last-rip 0x0000000000000000 wait4 0 wait4-reap 0 wait4-last-exit-code 45 child-root-cleanup 0 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0 vfs-bin-alias 6 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M26 forked-child exec inheritance verification:

- generated by direct `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'"`
- visible console output is exactly `Nested FAT32 path fixture`
- implementation scope: no kernel code changes after M25; this is an evidence gate proving the M25 exec transfer composes with the M22 process-root pool, M23 fork/wait state duplication, M24 pipe fd inheritance, Linux cwd state, and the NVMe VFS provider
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 0 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 4 prctl-set-name 1 prctl-get-name 3 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 2 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE60 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 4 vfs-bin-open 0 vfs-bin-read 2 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M27 independently staged static ET_EXEC verification:

- generated by direct `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraAppPath .\external\build\zig-musl-smoke-imagebase -ExtraAppName SMOKE -ExtraAppSource "local-zig-musl-smoke" -ExtraAppVersion "0.1" -ExtraShellLine "linux /APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'"`
- visible console output is exactly `zig-musl-smoke`
- implementation scope: optional extra-app staging/provenance parameters in `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1`; scheduler-owned FS-base save/restore/set state for Linux tasks; `arch_prctl(ARCH_SET_FS)` updates to the active scheduler task; fork/clone child task FS-base seeding; and exec-transfer FS-base clearing before the new image sets its own TLS base
- first M27 failure fixed: BusyBox code resumed with the SMOKE TLS base and faulted at `mov %fs:0x0`; task-local FS-base save/restore now reports `fs-save 3 fs-restore 4 fs-set 7`
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- staged SMOKE artifact: `external\build\zig-musl-smoke-imagebase`, SHA-256 `F9F1BD81B69B6C8C13A7E9CAE3DA9C24E45B76124195F7D945F97A7B9BE0F50B`, static ET_EXEC linked at `0x52000000`, staged at `/APPS/SMOKE`
- scope note: the SMOKE artifact is a local ignored external-build product, not a committed repo fixture and not a third-party package; M28 replaces this with an upstream non-BusyBox package utility
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 66 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 15 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 15 write 2 write-bytes 30 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 1 prctl-get-name 2 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 2 vfs-bin-open 0 vfs-bin-read 1 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M28 third-party non-BusyBox package verification:

- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- built artifact: upstream `echo.c` plus upstream sbase `libutf.a`/`libutil.a`, built by the local musl cross toolchain as static non-PIE ET_EXEC at `0x52000000`; staged artifact `external\build\sbase-0.1-echo-x86_64-musl-0x52000000`, SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952
- ELF verification: `readelf -h -l` reports `Type: EXEC`, entry `0x520010d1`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`
- first M28 failure fixed: sbase `echo` legitimately calls `fclose(stdout)` before exit, so the older cleanup invariant `fd_release + exit_fd_release >= 3` falsely rejected the run with `drs-realbin-fail stage cleanup`. The invariant now proves the fd table is detached after cleanup instead of requiring three final fd releases.
- final artifact budget after the fix: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- direct command: `linux /APPS/SBECHO m28-sbase-direct`
- direct visible console output is exactly `m28-sbase-direct`
- direct proof summary: `drs-realbin path /APPS/SBECHO ... mapped 4 pages 11 stack 16 ... console-bytes 17 exit 0 cleanup 1 ... writev 1 writev-bytes 17 ioctl 1 ioctl-tty 1 ... pml4-pool-used-final 0 ... unimplemented 0 page-faults 0`
- forked pipeline command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'`
- forked pipeline visible console output is exactly `m28-sbase-pipeline`
- forked pipeline proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 68 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 19 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 19 write 1 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 19 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 3 prctl-set-name 1 prctl-get-name 2 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 2 vfs-bin-open 0 vfs-bin-read 1 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M29 broader third-party utility read path verification:

- implementation scope: second optional extra-app staging/provenance parameters in `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1`, allowing `/APPS/SBECHO` and `/APPS/SBCAT` to be staged in the same generated NVMe FAT image without embedding either third-party artifact in the repo
- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- new built artifact: upstream `cat.c` plus upstream sbase `libutf.a`/`libutil.a`, built by the local musl cross toolchain as static non-PIE ET_EXEC at `0x52000000`; staged artifact `external\build\sbase-0.1-cat-x86_64-musl-0x52000000`, SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- ELF verification: `readelf -h -l` reports `Type: EXEC`, entry `0x5200117f`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- direct command: `linux /APPS/SBCAT /nvme/apps/data/file.txt`
- direct visible console output is exactly `Nested FAT32 path fixture`
- direct proof summary: `drs-realbin path /APPS/SBCAT ... mapped 4 pages 9 stack 16 ... console-bytes 27 exit 0 cleanup 1 ... read 2 read-bytes 27 write 1 write-bytes 27 vfs-nvme-reads 2 vfs-nvme-bytes 27 pml4-pool-used-final 0 ... unimplemented 0 page-faults 0`
- two-executable pipeline command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'`
- two-executable pipeline visible console output is exactly `m29-sbase-pipe`
- proof note: `vfs-bin-alias 0 vfs-bin-read 0 vfs-bin-denial 0` proves the pipeline consumer did not use the BusyBox alias backend; `execve-last-binary-bytes 36968` and `execve-last-transfer-rip 0x000000005200117F` prove the last child exec was the staged sbase `cat` artifact
- two-executable pipeline proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 67 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 15 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 15 write 1 write-bytes 15 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 15 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE70 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M30 third-party PATH directory proof verification:

- implementation scope: fixed Linux VFS `/usr/local` and `/usr/local/bin` directories, PATH-resolvable third-party aliases for `sbecho` and `sbcat`, `vfs-localbin-*` telemetry, QMP keyboard support for uppercase, `=`, and `:`, and Linux `argv[0]` lowercase canonicalization so uppercase FAT-style launch paths still dispatch BusyBox correctly
- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- staged `sbecho` artifact: `external\build\sbase-0.1-echo-x86_64-musl-0x52000000`, SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952
- staged `sbcat` artifact: `external\build\sbase-0.1-cat-x86_64-musl-0x52000000`, SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve 827,872 bytes; BIOS reserve remains 101 sectors
- PATH pipeline command: `linux /APPS/BUSYBOX sh -c 'PATH=/usr/local/bin:/bin:/usr/bin; sbecho m30-path-pipe | sbcat'`
- PATH pipeline visible console output is exactly `m30-path-pipe`
- proof note: `vfs-localbin-alias 6 vfs-localbin-read 2 vfs-localbin-denial 0` proves both third-party utilities were found through `/usr/local/bin`; `vfs-bin-alias 0 vfs-bin-read 0` proves the BusyBox alias backend was not used for the pipeline children; `execve 2`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the two-child PATH pipeline completed and cleaned up
- directory control: `linux /APPS/BUSYBOX ls /usr/local/bin` prints `sbcat   sbecho` and reports `getdents64 2 getdents64-entries 2 stat 3 stat-denial 0 vfs-localbin-alias 2 vfs-localbin-denial 0 page-faults 0`
- PATH pipeline proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 14 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 14 write 1 write-bytes 14 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 14 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M31 default Linux environment proof verification:

- implementation scope: UEFI-only real Linux launcher now supplies one fixed initial environment entry, `PATH=/usr/local/bin:/bin:/usr/bin`, to `elf64_launch_static`; real-binary telemetry now reports the actual initial stack environment count as `envc`
- staged artifacts are unchanged from M30: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- default-environment command: `linux /APPS/BUSYBOX sh -c 'sbecho m31-default-path | sbcat'`
- visible console output is exactly `m31-default-path`
- proof note: `envc 1` proves the initial BusyBox stack received the default environment; `execve-last-envc 2` proves BusyBox ash passed environment entries into the forked child replacements; `vfs-localbin-alias 6 vfs-localbin-read 2` proves PATH resolution used `/usr/local/bin`; `vfs-bin-alias 0 vfs-bin-read 0` proves the BusyBox alias backend was not used for the third-party pipeline; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- default-environment proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 1 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 17 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 17 write 1 write-bytes 17 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 17 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 2 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE50 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M32 bounded shell environment polish verification:

- implementation scope: UEFI-only real Linux launcher default environment now contains `PATH=/usr/local/bin:/bin:/usr/bin`, `HOME=/`, `USER=limitless`, and `PWD=/`; the QMP keyboard injector now supports literal `$` so shell variable expansion commands are delivered faithfully
- staged artifacts are unchanged from M30: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- shell-expansion command: `linux /APPS/BUSYBOX sh -c 'sbecho $USER:$PWD | sbcat'`
- visible console output is exactly `limitless:/`
- proof note: `envc 4` proves the initial BusyBox stack received all four default environment entries; `execve-last-envc 4` proves BusyBox ash passed them into forked child replacements; `vfs-localbin-alias 6 vfs-localbin-read 2` proves default PATH still resolves `/usr/local/bin` third-party utilities; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- shell-environment proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 12 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 12 write 1 write-bytes 12 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 12 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M33 cwd and PWD synchronization proof verification:

- implementation scope: no kernel, verifier, or artifact changes were required after M32; the M33 trace proves the existing `chdir`, cwd inheritance, environment inheritance, PATH lookup, fork, exec, and pipe paths already compose correctly
- staged artifacts are unchanged from M30: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- cwd/PWD command: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; sbecho $PWD | sbcat'`
- visible console output is exactly `/nvme/apps`
- proof note: `chdir 1` proves BusyBox ash changed the Linux persona cwd; `execve-last-envc 5` proves ash exported an updated child environment after the `cd`; `path-relative 1 path-dot 1` proves relative resolution remained active; `vfs-localbin-alias 6 vfs-localbin-read 2` proves default PATH still resolved the third-party tools from `/usr/local/bin`; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- cwd/PWD proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 11 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 11 write 1 write-bytes 11 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 11 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M34 environment mutation and export proof verification:

- implementation scope: no kernel or verifier changes were required; an additional ignored upstream sbase 0.1 `env` artifact was built by the local musl cross toolchain as static non-PIE ET_EXEC at `0x52000000` and staged as `/APPS/SBENV`
- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- new staged `sbenv` artifact: `external\build\sbase-0.1-env-x86_64-musl-0x52000000`, SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616
- companion `sbcat` artifact is unchanged from M29: `external\build\sbase-0.1-cat-x86_64-musl-0x52000000`, SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- ELF verification for `sbenv`: `readelf -h -l` reports `Type: EXEC`, entry `0x5200105b`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- environment-observation command: `linux /APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: the visible `USER=operator` line is printed by the forked third-party `sbenv` child, not by BusyBox shell expansion; `execve 2 execve-last-envc 4` proves both third-party children were replaced with a four-entry environment; `vfs-nvme-reads 2` proves the child binaries came from staged NVMe FAT artifacts; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- environment mutation proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 0 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M35 third-party exec handoff proof verification:

- implementation scope: no kernel, verifier, or artifact changes were required after M34; the M35 trace proves the existing third-party `execvp` path already composes with explicit Linux VFS paths, environment replacement, fork, pipe, and wait cleanup
- staged artifacts are unchanged from M34: `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- third-party handoff command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator /nvme/apps/sbenv | /nvme/apps/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3` proves the extra non-shell replacement happened after BusyBox ash forked the pipeline: the shell execed the first `sbenv`, that `sbenv` applied `USER=operator` and execed the second `/nvme/apps/sbenv`, and the consumer child execed `/nvme/apps/sbcat`; `vfs-nvme-reads 3` proves the real ET_EXEC child images were loaded from NVMe FAT; `execve-last-envc 4`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the handoff preserved the bounded environment and cleaned up
- third-party handoff proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 0 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M36 third-party PATH execvp proof verification:

- implementation scope: added a fixed `/usr/local/bin/sbenv` Linux VFS alias backed by the staged NVMe FAT `/APPS/SBENV` artifact through the Linux-visible `/nvme/apps/sbenv` path; no syscall surface, dynamic linking, heap allocation, or arbitrary filesystem population was added
- staged artifacts are unchanged from M34: `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- third-party PATH handoff command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator sbenv | /nvme/apps/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3` proves the shell execed the first `/nvme/apps/sbenv`, that third-party process used PATH `execvp` to replace itself with `sbenv`, and the consumer child execed `/nvme/apps/sbcat`; `vfs-localbin-alias 2 vfs-localbin-read 1 vfs-localbin-denial 0` proves the nested non-shell PATH lookup used the bounded `/usr/local/bin/sbenv` alias; `vfs-nvme-reads 3`, `execve-last-envc 4`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean handoff, environment preservation, and cleanup
- third-party PATH handoff proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 2 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M37 all-third-party PATH pipeline proof verification:

- implementation scope: no kernel, verifier, or artifact changes were required after M36; the trace proves the existing default environment, `/usr/local/bin` aliases, fork, pipe, wait, and non-shell `execvp` paths compose when all child executable names are resolved by PATH
- staged artifacts are unchanged from M34: `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- all-third-party PATH command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator sbenv | sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `vfs-localbin-alias 8 vfs-localbin-read 3 vfs-localbin-denial 0` proves BusyBox ash found `sbenv` and `sbcat` by PATH and the first `sbenv` found the second `sbenv` by PATH; `vfs-bin-alias 0` proves the BusyBox applet alias backend was not used for the child executables; `execve 3`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the fully PATH-resolved third-party pipeline completed and cleaned up
- all-third-party PATH proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 74 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 8 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M38 expanded localbin directory proof verification:

- implementation scope: extended `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1` with a third optional extra-app staging slot so the verifier can stage `/APPS/SBECHO`, `/APPS/SBENV`, and `/APPS/SBCAT` in the same NVMe FAT image; no kernel, syscall, VFS, loader, or artifact changes were required
- first trace result: with only two extra-app slots staged, `ls /usr/local/bin` listed three VFS aliases but failed `stat` on the unstaged `sbecho` backend, printing `ls: /usr/local/bin/sbecho: No such file or directory` and reporting `stat-denial 1 exit 1`
- staged artifacts: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- expanded-localbin command: `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin'`
- visible console output is exactly `sbenv   sbcat   sbecho`
- proof note: `getdents64-entries 3` proves `/usr/local/bin` enumerated the three bounded third-party aliases, `stat-denial 0` proves each listed entry had a present staged backend, and `vfs-localbin-alias 3 vfs-localbin-denial 0` proves the directory/stat path used the localbin alias provider cleanly
- expanded-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 38 syscall-root-denial 0 fs-save 0 fs-restore 1 fs-set 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 23 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 96 stat 6 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 pipe 0 pipe-create 0 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 0 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 4294967295 readv 0 readv-bytes 0 writev 1 writev-bytes 23 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 2 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 0 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 22 execve-last-binary-bytes 0 execve-last-closed-fds 0 execve-last-fd-live-before 0 execve-last-fd-live-after 0 execve-last-vma-before 0 execve-last-vma-released 0 execve-last-vma-after 0 execve-last-argc 0 execve-last-envc 0 execve-last-transfer-ready 0 execve-last-transfer-rip 0x0000000000000000 execve-last-transfer-rsp 0x0000000000000000 fork 0 fork-success 0 fork-enosys 0 fork-denial 0 fork-child-slot 4294967295 fork-child-root-distinct 0 fork-last-rip 0x0000000000000000 wait4 0 wait4-reap 0 wait4-last-exit-code 45 child-root-cleanup 0 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M39 absolute localbin exec proof verification:

- implementation scope: raised the persistent ring-3 shell interactive command capacity in `kernel\arch\x86_64\runtime_image_user.asm` from 96 to 128 bytes so it matches `SHELL64_MAX_LINE_BYTES`; this allows the 106-byte absolute-path proof command to reach `shell64_execute_line` intact. No Linux syscall, VFS alias, loader, or third-party artifact changes were required.
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- absolute localbin command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv USER=operator /usr/local/bin/sbenv | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3`, `vfs-localbin-alias 6`, `vfs-localbin-read 3`, and `vfs-localbin-denial 0` prove BusyBox ash executed absolute `/usr/local/bin/sbenv`, the first `sbenv` replaced itself through absolute `/usr/local/bin/sbenv`, and the consumer executed absolute `/usr/local/bin/sbcat`; `vfs-bin-alias 0` proves no BusyBox applet alias backend was used for the child executables; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the absolute third-party pipeline completed and cleaned up
- absolute-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M40 absolute localbin file-read proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M39
- first trace note: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt'` passed and printed `Nested FAT32 path fixture`, but BusyBox ash optimized the single final command by replacing itself through `execve`, so that direct proof reports `fork 0`; the accepted M40 pipeline proof below forces forked child exec for both absolute-localbin `sbcat` commands
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- absolute-localbin file pipeline command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `execve 2`, `fork 2`, `wait4-reap 2`, and `pipe-live-final 0` prove BusyBox ash forked both pipeline sides and reaped both children; `read 4 read-bytes 54 write 2 write-bytes 54` proves one child read the real NVMe file and the second child consumed the pipe; `vfs-localbin-alias 4 vfs-localbin-read 2 vfs-localbin-denial 0` proves both executables came from absolute `/usr/local/bin` aliases; `vfs-nvme-reads 4 vfs-nvme-bytes 36968` proves the staged `SBCAT` artifact plus file content were read through the NVMe VFS; `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process isolation and cleanup
- absolute-localbin file-read proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M41 localbin cwd-relative file-read proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M40
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- cwd-relative absolute-localbin command: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /usr/local/bin/sbcat data/file.txt | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `chdir 1` proves BusyBox ash changed cwd to `/nvme/apps`; `path-relative 2` proves the child-side relative path was canonicalized against inherited cwd; `execve-last-envc 5` proves BusyBox ash exported updated `PWD`; `execve 2`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `vfs-localbin-read 2`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove both absolute-localbin children executed, transferred the file bytes, and cleaned up
- cwd-relative absolute-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 2 path-dot 1 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M42 localbin dotdot relative path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M41
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- dotdot relative absolute-localbin command: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps/data; /usr/local/bin/sbcat ../data/file.txt | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `chdir 1` proves BusyBox ash changed cwd to `/nvme/apps/data`; `path-relative 2 path-dotdot 1 path-fault 0` proves a forked absolute-localbin child canonicalized `../data/file.txt` back to the real NVMe file; `execve-last-envc 5` proves the updated `PWD` environment crossed exec; `execve 2`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `vfs-localbin-read 2`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean execution and cleanup
- dotdot relative absolute-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 2 path-dot 1 path-dotdot 1 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M43 localbin current-directory path proof verification:

- implementation scope: `kernel/arch/x86_64/linux_abi.c` now canonicalizes `execve` user paths against cwd before VFS stat/read, matching the path behavior already used by open/stat/chdir
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- current-directory localbin command: `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ./sbcat /nvme/apps/data/file.txt | ./sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `chdir 1` proves BusyBox ash changed cwd to `/usr/local/bin`; `path-relative 3 path-dot 3 path-fault 0` proves both `./sbcat` executable paths and the input file path were canonicalized without fault; `execve 2 execve-denial 0` proves the forked children reached real exec transfer; `vfs-localbin-alias 4 vfs-localbin-read 2` proves the normalized `/usr/local/bin/sbcat` paths hit the bounded third-party alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- current-directory localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 3 path-dot 3 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M44 non-shell current-directory execvp proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M43
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- non-shell current-directory execvp command: `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ./sbenv USER=operator ./sbenv | ./sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
OLDPWD=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/usr/local/bin
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `./sbenv`, the first `sbenv` replaced itself with another `./sbenv`, and the pipeline consumer launched `./sbcat`; `path-relative 4 path-dot 4 path-fault 0` proves current-directory executable path canonicalization across all relative executable paths; `vfs-localbin-alias 6 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- non-shell current-directory execvp proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 83 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 4 path-dot 4 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 83 write 1 write-bytes 83 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 83 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M45 localbin executable dotdot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M44
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- relative executable dotdot command: `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ../bin/sbenv USER=operator ../bin/sbenv | ../bin/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
OLDPWD=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/usr/local/bin
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `../bin/sbenv`, the first `sbenv` replaced itself with another `../bin/sbenv`, and the pipeline consumer launched `../bin/sbcat`; `path-relative 4 path-dotdot 3 path-fault 0` proves relative executable `..` canonicalization across all executable paths; `vfs-localbin-alias 6 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- relative executable dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 83 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 4 path-dot 1 path-dotdot 3 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 83 write 1 write-bytes 83 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 83 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M46 absolute localbin executable dotdot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M45
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- absolute executable dotdot command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/../bin/sbenv USER=operator /usr/local/bin/../bin/sbenv | /usr/local/bin/../bin/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr/local/bin/../bin/sbenv`, the first `sbenv` replaced itself with another `/usr/local/bin/../bin/sbenv`, and the pipeline consumer launched `/usr/local/bin/../bin/sbcat`; `path-relative 1 path-dotdot 3 path-fault 0` proves absolute executable `..` canonicalization without a shell `cd`; `vfs-localbin-alias 6 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- absolute executable dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 3 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M47 absolute localbin mixed-dot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M46
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- mixed-dot command: `linux /APPS/BUSYBOX sh -c '/usr/local/./bin/../bin/sbenv USER=operator /usr/local/./bin/../bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr/local/./bin/../bin/sbenv`, the first `sbenv` replaced itself with another `/usr/local/./bin/../bin/sbenv`, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 3 path-dotdot 2 path-fault 0` proves mixed absolute executable `.` and `..` canonicalization; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- mixed-dot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 3 path-dotdot 2 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M48 absolute localbin repeated-slash path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M47
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- repeated-slash command: `linux /APPS/BUSYBOX sh -c '/usr//local/bin/sbenv USER=operator /usr/local//bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr//local/bin/sbenv`, the first `sbenv` replaced itself with `/usr/local//bin/sbenv`, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 1 path-dotdot 0 path-fault 0` proves repeated slash canonicalization did not require dotdot handling; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- repeated-slash proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M49 absolute localbin root-clamped dotdot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M48
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- root-clamped dotdot command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/../../local/bin/sbenv USER=operator /usr/local/bin/../../local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr/local/bin/../../local/bin/sbenv`, the first `sbenv` replaced itself with the same root-clamped path, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 1 path-dotdot 4 path-fault 0` proves bounded upward `..` canonicalization did not escape the root or fault; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- root-clamped dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 4 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M50 absolute over-root dotdot clamp proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M49
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- over-root dotdot command: `linux /APPS/BUSYBOX sh -c '/../../usr/local/bin/sbenv USER=operator /../../../usr/local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/../../usr/local/bin/sbenv`, the first `sbenv` replaced itself with `/../../../usr/local/bin/sbenv`, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 1 path-dotdot 5 path-fault 0` proves over-root `..` traversal clamped at `/` without escaping or faulting; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- over-root dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 5 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M51 absolute localbin trailing-slash executable denial proof verification:

- implementation scope: `kernel/arch/x86_64/linux_abi.c` now preserves original execve trailing-slash intent across canonicalization and denies slash-suffixed non-directory executable targets before the binary read; `kernel/arch/x86_64/linux_exec.c` emits `path-trailing` and `path-trailing-denial` telemetry
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest after the fix: kernel bytes 1,269,280; checksum `0xD4789513`; SHA-256 `fa4be9a316b1888ac9f363ded5b788015470f1849e9255f1ce8d308f776ed628`
- trailing-slash denial command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv/ USER=operator /usr/local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
sh: /usr/local/bin/sbenv/: not found
```

- proof note: `path-trailing 1 path-trailing-denial 1` proves the original terminal slash survived canonicalization as denial intent; `execve 1 execve-denial 1` proves the slash-suffixed `sbenv` was not launched as a file; `vfs-localbin-read 1` proves only the pipeline consumer `sbcat` was read as an executable; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- trailing-slash denial telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 65 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 37 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 1 path-trailing-denial 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 37 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M52 absolute localbin trailing-slash directory/open proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M51
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- trailing-slash directory command: `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin/ | sbcat'`
- visible console output is exactly:

```text
sbenv
sbcat
sbecho
```

- proof note: `getdents64 2 getdents64-entries 3` proves `/usr/local/bin/` remains a valid directory path with a terminal slash; `execve-denial 0 path-trailing 0 path-trailing-denial 0` proves the M51 executable-only trailing-slash denial did not fire on directory enumeration; `vfs-localbin-alias 6 vfs-localbin-read 1` proves the alias namespace stayed discoverable and only the pipeline consumer `sbcat` was read as an executable; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- trailing-slash directory telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 79 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 19 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 96 stat 7 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 19 write 1 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 19 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 0 ioctl-enotty 2 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M53 absolute localbin missing executable denial proof verification:

- implementation scope: `kernel/arch/x86_64/linux_abi.c` now maps a failed executable binary `stat` in the exec-read path to `ENOENT` instead of collapsing it into `EINVAL`, so BusyBox ash reports a missing absolute-localbin executable as `not found`
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest after the fix: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- missing executable command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbmissing USER=operator /usr/local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
sh: /usr/local/bin/sbmissing: not found
```

- proof note: `execve 1 execve-denial 1` proves the missing producer was denied as an exec target; `vfs-localbin-denial 2` and `vfs-bin-denial 2` prove the bounded alias tables rejected the missing executable; `vfs-localbin-read 1` and `vfs-nvme-reads 1` prove only the valid pipeline consumer `sbcat` was read from its NVMe-backed alias; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- missing executable telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 65 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 40 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 40 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 2 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 2
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M54 PATH localbin missing executable denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M53
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- PATH missing executable command: `linux /APPS/BUSYBOX sh -c 'sbmissing USER=operator sbenv|sbcat'`
- visible console output is exactly:

```text
sh: sbmissing: not found
```

- proof note: `stat-denial 6` proves BusyBox ash walked the default `PATH` and denied the missing producer before exec; `vfs-localbin-denial 4` and `vfs-bin-denial 12` prove the bounded `/usr/local/bin`, `/bin`, and `/usr/bin` alias namespaces rejected the missing command; `execve 1 execve-denial 0` proves only the valid `sbcat` consumer was execed; `vfs-localbin-read 1` and `vfs-nvme-reads 1` prove only that consumer touched the NVMe-backed executable; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- PATH missing executable telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 25 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 6 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 25 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 12 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 4
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M55 non-shell `execvp` missing executable denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M54
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- non-shell `execvp` missing executable command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator sbmissing|sbcat'`
- visible console output is exactly:

```text
sbenv: execvp sbmissing: No such file or directory
```

- proof note: `execve 2 execve-denial 3` proves BusyBox ash launched valid `sbenv` and `sbcat`, then the non-shell `sbenv` process attempted and denied missing `sbmissing` through its own `execvp` path; `vfs-localbin-alias 6`, `vfs-localbin-denial 2`, `vfs-bin-denial 6`, `vfs-localbin-read 2`, and `vfs-nvme-reads 2` prove only valid `sbenv` and `sbcat` executable bytes were read while the missing lookup denied cleanly; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- non-shell `execvp` missing executable telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 51 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 51 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 3 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 6 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 2
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M56 non-shell `execvp` trailing-slash executable denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M55
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- non-shell `execvp` trailing-slash executable command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/sbenv/|sbcat'`
- visible console output is exactly:

```text
sbenv: execvp /usr/local/bin/sbenv/: Not a directory
```

- proof note: `path-trailing 1 path-trailing-denial 1` proves the terminal slash survived the non-shell `execvp` path as executable-denial intent; `execve 2 execve-denial 1` proves BusyBox ash launched valid `sbenv` and `sbcat` while the slash-suffixed target was denied; `vfs-localbin-alias 7`, `vfs-localbin-read 2`, and `vfs-nvme-reads 2` prove only valid `sbenv` and `sbcat` executable bytes were read; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- non-shell `execvp` trailing-slash executable telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 53 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 1 path-trailing-denial 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 53 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M57 non-shell `execvp` directory-target denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M56
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- non-shell `execvp` directory-target command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/|sbcat'`
- visible console output is exactly:

```text
sbenv: execvp /usr/local/bin/: Invalid argument
```

- proof note: `path-trailing 1 path-trailing-denial 1` proves the terminal slash on the directory target was preserved through the non-shell `execvp` path; `execve 2 execve-denial 1` proves BusyBox ash launched valid `sbenv` and `sbcat` while the directory target was denied; `vfs-localbin-alias 6`, `vfs-localbin-read 2`, and `vfs-nvme-reads 2` prove only valid `sbenv` and `sbcat` executable bytes were read; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- non-shell `execvp` directory-target telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 48 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 1 path-trailing-denial 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 48 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M58 non-shell `execvp` bare-directory executable denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M57
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- non-shell `execvp` bare-directory command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin|sbcat'`
- visible console output is exactly:

```text
sbenv: execvp /usr/local/bin: Invalid argument
```

- proof note: `path-trailing 0 path-trailing-denial 0` proves this was not the terminal-slash denial path; `execve 2 execve-denial 1` proves BusyBox ash launched valid `sbenv` and `sbcat` while the bare directory target was denied; `vfs-localbin-alias 6`, `vfs-localbin-read 2`, and `vfs-nvme-reads 2` prove only valid `sbenv` and `sbcat` executable bytes were read; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- non-shell `execvp` bare-directory telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 47 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 47 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M59 non-shell `execvp` dot-directory executable denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M58
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- non-shell `execvp` dot-directory command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/.|sbcat'`
- visible console output is exactly:

```text
sbenv: execvp /usr/local/bin/.: Invalid argument
```

- proof note: `path-dot 2` proves the dot segment was observed through the non-shell executable path; `path-trailing 0 path-trailing-denial 0` proves this was not the terminal-slash denial path; `execve 2 execve-denial 1` proves BusyBox ash launched valid `sbenv` and `sbcat` while the dot-directory target was denied; `vfs-localbin-alias 6`, `vfs-localbin-read 2`, and `vfs-nvme-reads 2` prove only valid `sbenv` and `sbcat` executable bytes were read; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- non-shell `execvp` dot-directory telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 49 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 2 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 49 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M60 non-shell `execvp` parent-directory executable denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M59
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- non-shell `execvp` parent-directory command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/..|sbcat'`
- visible console output is exactly:

```text
sbenv: execvp /usr/local/bin/..: Invalid argument
```

- proof note: `path-dotdot 1` proves the parent segment was observed through the non-shell executable path; `path-trailing 0 path-trailing-denial 0` proves this was not the terminal-slash denial path; `execve 2 execve-denial 1` proves BusyBox ash launched valid `sbenv` and `sbcat` while the parent-directory target was denied; `vfs-localbin-alias 6`, `vfs-localbin-read 2`, and `vfs-nvme-reads 2` prove only valid `sbenv` and `sbcat` executable bytes were read; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- non-shell `execvp` parent-directory telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 50 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 1 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 50 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M61 non-shell `execvp` parent-rebased directory executable denial proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M60
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest remains: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- non-shell `execvp` parent-rebased directory command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/../bin|sbcat'`
- visible console output is exactly:

```text
sbenv: execvp /usr/local/bin/../bin: Invalid argument
```

- proof note: `path-dotdot 1` proves the parent segment was observed through the non-shell executable path; `path-trailing 0 path-trailing-denial 0` proves this was not the terminal-slash denial path; `execve 2 execve-denial 1` proves BusyBox ash launched valid `sbenv` and `sbcat` while the parent-rebased directory target was denied; `vfs-localbin-alias 6`, `vfs-localbin-read 2`, and `vfs-nvme-reads 2` prove only valid `sbenv` and `sbcat` executable bytes were read; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- non-shell `execvp` parent-rebased directory telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 54 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 1 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 54 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M106 Universal Hardware Inventory And Driver Binding Core

M106 is accepted on the UEFI Product path with the new hardware-registry gate:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareRegistryGate
```

Implementation scope:

- added a UEFI-only fixed-size hardware registry with `HARDWARE64_REGISTRY_MAX_DEVICES = 32`
- records platform, display, input, storage, USB controller, and network binding evidence in a static table
- keeps PCI inventory queries behind the existing hardware-inventory capability token
- exposes a compact `drs-hardware-registry` proof through `hwval`
- excludes the real registry implementation from the BIOS build; BIOS remains on its prior path and reserve
- adds a narrow verifier gate for hardware registry evidence without running the full historical UI command script

M106 acceptance telemetry:

```text
[x64] drs-hardware-registry hardware-registry 1 refresh 1 limit 32 inventory 11 pci-enumerated 8 pci-query-denial 0 acpi-tables 2 display-device 2 input-device 4 storage-device 2 usb-controller 1 network-device 1 driver-bound 9 driver-candidate 0 driver-deferred 2 driver-unsupported 0 driver-failed 0 overflow 0 token 0x89CF635C
```

Visible `hwval` evidence included framebuffer `1280x800`, ECAM active, xHCI found/mapped, PS/2 and xHCI input evidence, NVMe ready with FAT located, AHCI detected as deferred, and virtio networking online through the brokered Product path.

Final reserves after the implementation build:

- UEFI kernel bytes: 1,303,232 / 2,097,152, reserve 793,920 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0xF1037FB1`

M106 non-claims: this does not add new hardware drivers by itself, does not claim universal hardware support, does not fix physical laptop display mode selection, does not add touchpad HID parsing beyond current evidence, does not add Wi-Fi, audio, GPU acceleration, ACPI power management, or arbitrary USB class support. It creates the Product evidence surface needed to make those follow-on milestones falsifiable.

## M107 Physical Display Bring-Up Reliability

M107 is accepted on the UEFI Product path with the new hardware-display gate:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

Implementation scope:

- derives the default console viewport from the actual GOP framebuffer geometry instead of keeping the previous fixed 960x648 rectangle
- selects a bounded text scale from the real mode size, keeping 1280x800 at scale 2 and allowing larger physical panels to use scale 3
- validates framebuffer pitch/stride and byte coverage before claiming the display readable
- records console columns, rows, viewport origin/size, fit status, readable status, clipping count, and layout token
- exposes a compact `drs-display-readability` proof through `hwval`
- keeps the new dynamic layout/readability machinery UEFI-only; the BIOS display path keeps the previous fixed constants and remains at 101 reserve sectors

M107 acceptance telemetry:

```text
[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 24 viewport-y 96 viewport-w 1232 viewport-h 680 columns 102 rows 37 fit 1 readable 1 clip 0 token 0xF8C98059
```

Visible `hwval` evidence included framebuffer required bytes matching provided bytes (`0x003E8000`), stride sane, bounds sane, display text scale `2`, console columns `102`, console rows `37`, and display readable `yes`.

Final reserves after the implementation build:

- UEFI kernel bytes: 1,307,712 / 2,097,152, reserve 789,440 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x8A9C8B83`

M107 non-claims: this does not add DRM/KMS, native GPU mode setting, EDID policy, acceleration, multi-monitor support, font rasterization, or a complete physical-laptop display pass. It makes framebuffer readability and clipping falsifiable so the next physical hardware runs can tell whether the issue is mode selection, pitch/size mismatch, viewport fit, or a later compositor/input problem.

## M108 Visible Cursor Fallback And Bounded Login Recovery

M108 is accepted on the UEFI Product path with the hardware-display gate:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

Implementation scope:

- keeps mouse packet/coordinate handling unchanged and fixes only the visibility path
- lets the compositor cursor save/restore/draw path operate directly against the physical framebuffer on UEFI when `g_display_compositor_active == 0`
- preserves the existing BIOS behavior and avoids adding new BIOS display machinery
- adds `display64_cursor_visible()` and `display64_direct_cursor_count()` telemetry helpers
- extends `hwval` and the hardware-display verifier with `cursor-visible`, `cursor-draws`, and `direct-cursor-draws`
- changes the UEFI Product login gate from an indefinite input wait into a bounded local-console recovery path; typed credentials are still accepted first, but missing input no longer halts boot before the shell
- updates the QEMU verifier to accept either typed login completion or bounded `LOGIN OK` recovery before sending shell commands

M108 acceptance telemetry:

```text
[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 24 viewport-y 96 viewport-w 1232 viewport-h 680 columns 102 rows 37 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 3 direct-cursor-draws 3 token 0xF8C98059
```

Visible `hwval` evidence included `display cursor visible: yes`, `display cursor draws: 3`, `display direct cursor draws: 3`, `mouse packets: 2`, `mouse x: 560`, and `mouse y: 420`.

Bounded login evidence included `first-run hardware input fallback`, `first-run hardware recovery login`, `stage LOGIN OK`, `drs-login-auth-success 1`, and the persistent shell accepting the subsequent `hwval` command without a manual key press.

Final reserves after the implementation build:

- UEFI kernel bytes: 1,307,840 / 2,097,152, reserve 789,312 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x1A4850D3`

M108 non-claims: this does not redesign the desktop, add a full visual design system, add native GPU acceleration, implement DRM/KMS, add I2C HID touchpad support, or prove the MSI laptop NVMe path. It fixes the concrete visible-cursor failure mode where pointer packets move but no cursor is drawn, removes the no-key boot blocker, and adds telemetry so future physical hardware runs can distinguish input movement from cursor presentation.

## M109 Product Visual Polish Direct Compositor Foundation

M109 is accepted on the UEFI Product path with the hardware-display gate:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

Implementation scope:

- adds a UEFI-only direct compositor mode: when the full back buffer cannot fit in the current firmware handoff window, `g_display_compositor_active` remains true, `g_display_compositor_direct_mode` records the fallback, and drawing uses the physical framebuffer directly
- keeps the back-buffer path intact when allocation succeeds and makes back-buffer indexed reads explicit so direct mode never dereferences a null back buffer
- lets the existing font probe, window-manager probe, desktop probe, taskbar, launcher panel, and window rendering paths initialize on GOP framebuffers that cannot afford a separate full-size back buffer
- updates the Product palette from the previous diagnostic red/grey look to a calmer teal-accented UI with distinct app colors, cleaner surface/border contrast, and a less shouted display-ready banner
- adds UEFI-only `display64_compositor_direct_mode()` and `display64_ui_polish_token()` telemetry helpers
- extends `hwval` and the hardware-display verifier with a new `drs-ui-polish` proof line while preserving the existing `drs-display-readability` line
- excludes the new telemetry helpers from BIOS so the BIOS scaffold remains at 101 reserve sectors

M109 acceptance telemetry:

```text
[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 205 direct-cursor-draws 207 token 0xF8C98059
[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 1 token 0xCB1B1C83
```

Visible `hwval` evidence included `display compositor direct: yes`, `display ui polish token: 0xCB1B1C83`, `display cursor visible: yes`, `display cursor draws: 158`, `display direct cursor draws: 161`, `mouse packets: 2`, `mouse x: 560`, and `mouse y: 420`.

Final reserves after the implementation build:

- UEFI kernel bytes: 1,308,032 / 2,097,152, reserve 789,120 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x37D2EFB2`

M109 non-claims: this does not add native GPU acceleration, DRM/KMS mode setting, EDID policy, multi-monitor support, a hardware touchpad driver, a complete GUI toolkit, or physical MSI laptop certification. It proves the existing Product desktop/window surface can initialize and draw through a direct GOP framebuffer compositor fallback, with visible cursor and telemetry-backed UI initialization.

## M110 NVMe/FAT Hardware Storage Triage

M110 is accepted on the UEFI Product path with the new hardware-storage gate:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareStorageGate
```

Implementation scope:

- adds a UEFI-only `drs-nvme-triage` proof line to `hwval`
- records NVMe controller discovery, readiness, Identify, IO queue creation, issued/completed read, read status, GPT signature, partition count, FAT32 start/size, VBR, FAT BPB, FAT located/unavailable/error, scoped shell RW capability, `/APPS` stat, `/APPS` first dirent, and staged file stat/size for `/APPS/BUSYBOX`, `/APPS/DYNLDLIMIT`, and `/APPS/LDLIMIT`
- emits the same triage line from the `linux` unavailable path so a direct `linux /apps/dynldlimit` failure on hardware carries the diagnostic context without requiring a separate command first
- adds `-HardwareStorageGate` to `tools/verify-qemu.ps1`, reusing the normal persistent shell plus `hwval` path
- preserves storage behavior: no new read/write authority, no new block endpoint, no FAT mutation, no Linux launcher behavior change, and no BIOS code expansion for the triage helper

M110 acceptance telemetry:

```text
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions 6 fat32-start 2048 fat32-sectors 8192 gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat 0 busybox-bytes 0 dynldlimit-stat 0 dynldlimit-bytes 0 ldlimit-stat 0 ldlimit-bytes 0 boot-staged 0 boot-app-bytes 0 boot-interp-bytes 0 boot-status 14 token 0xCDD4D6A0
```

The default M110 storage-gate image intentionally proves the diagnostic separation rather than staging dynamic artifacts: NVMe, GPT, FAT, and `/APPS` are healthy, while `busybox-stat`, `dynldlimit-stat`, and `ldlimit-stat` are `0` because those optional real-binary artifacts were not staged for this gate run.

Final reserves after the implementation build:

- UEFI kernel bytes: 1,308,544 / 2,097,152, reserve 788,608 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x769D7150`

M110 non-claims: this does not add a new NVMe driver, broaden hardware support, fix a physical laptop controller quirk by itself, add arbitrary filesystem mounting, or stage missing dynamic artifacts automatically. It makes the real-hardware storage failure falsifiable at the shell.

## M111 Boot/NVMe Staged Dynamic Artifact Verification

M111 is accepted on the UEFI Product path with:

```powershell
.\tools\verify-hardware-storage-staging.ps1 -SkipBuild
```

The full staged-build command used before the verifier was:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product -BootLinuxAppPath .\external\build\DYNLDLIMIT -BootLinuxAppName DYNLDLIMIT -BootLinuxInterpPath .\external\build\LDLIMIT -BootLinuxInterpName LDLIMIT
```

Staged artifacts:

- `/APPS/DYNLDLIMIT`: 15,680 bytes, SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`
- `/APPS/LDLIMIT`: 16,704 bytes, SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`

`BOOTMAN.TXT` now records the expected boot-Linux staging contract when those paths are supplied:

```text
boot-linux-expected=1
boot-linux-app=/APPS/DYNLDLIMIT
boot-linux-app-bytes=15680
boot-linux-app-sha256=9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa
boot-linux-interp=/APPS/LDLIMIT
boot-linux-interp-bytes=16704
boot-linux-interp-sha256=6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238
```

Acceptance telemetry:

```text
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions 6 fat32-start 2048 fat32-sectors 8192 gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat 0 busybox-bytes 0 dynldlimit-stat 1 dynldlimit-bytes 15680 ldlimit-stat 1 ldlimit-bytes 16704 boot-staged 1 boot-app-bytes 15680 boot-interp-bytes 16704 boot-status 0 stage-expected 1 dynldlimit-expected 1 ldlimit-expected 1 dynldlimit-match 1 ldlimit-match 1 stage-match 1 token 0x75BC2409
```

Final reserves after the staged build:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`
- UEFI manifest SHA-256: `824902bd0a384e02ea18193a6468f95a0842984f41c18cc06969c11a722df196`

M111 non-claims: this does not make physical NVMe universally work by itself, does not add a new storage driver, does not launch the dynamic binary, and does not change the Linux execution path. It proves that the generated UEFI/ISO artifact can carry the dynamic app/interpreter pair, that the loader stages both into boot-info, and that the runtime can compare those expected staged artifacts against the NVMe `/APPS` directory from `hwval`.

## M112 Physical Hardware Storage Capture Parser

M112 is accepted as a tooling milestone for real-hardware bring-up. It adds:

```powershell
.\tools\parse-hardware-storage-capture.ps1 -InputPath <captured-hwval-transcript> -OutputPath <evidence.json> [-RequireStagedDynamicArtifacts]
```

The parser consumes the `drs-nvme-triage` line emitted by `hwval`, writes a JSON evidence record, and reports the first failing stage in dependency order:

- NVMe controller discovery/readiness
- NVMe Identify
- IO queue creation
- read issue/completion/status
- GPT signature and partition enumeration
- FAT32 candidate geometry, VBR, BPB, mount, and error state
- scoped NVMe capability presence/delegation/error
- `/APPS` stat/type/dirent visibility
- optional M111 staged artifact presence and byte-count agreement for `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT`

Validation performed:

```powershell
.\tools\parse-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputPath .\build\hardware-storage-capture-qemu.json
.\tools\parse-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputPath .\build\hardware-storage-capture-qemu-staged-pass.json -RequireStagedDynamicArtifacts
.\tools\parse-hardware-storage-capture.ps1 -InputPath .\docs\hardware\msi-cyborg-15-a13ve.md -OutputPath .\build\hardware-storage-capture-legacy.json
```

Observed parser outcomes:

```text
hardware-storage-capture: storage-ready
  pass: True
  detail: NVMe, GPT, FAT, /APPS, capability delegation, and requested staged artifacts are all visible.

hardware-storage-capture: nvme-dynldlimit-stat
  pass: False
  detail: /APPS/DYNLDLIMIT was not visible through NVMe FAT.

hardware-storage-capture: legacy-realbin-unavailable
  pass: False
  detail: Only legacy drs-realbin-unavailable telemetry was found. Boot the M111-staged image and run hwval to capture drs-nvme-triage.
```

The `nvme-dynldlimit-stat` negative result was intentionally observed against an unstaged M110-style NVMe fixture; the staged M111 verifier was rerun and the parser then passed strict staged mode against the refreshed log.

M112 non-claims: this does not certify the MSI laptop, add a storage driver, modify kernel behavior, or prove physical NVMe access. It turns the next physical `hwval` capture into a deterministic stage classification so the following hardware milestone can target the actual failing layer.

Final reserves are unchanged from M111 because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

## M113 Physical Hardware Storage Evidence Bundle

M113 is accepted as a host-side hardware handoff milestone. It adds:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1 [-DynamicAppPath <DYNLDLIMIT>] [-DynamicInterpPath <LDLIMIT>] [-EvidenceDir <dir>] [-SkipBuild] [-SkipQemuGate]
```

The tool prepares a physical-laptop evidence directory containing:

- `limitlessos-x86_64-m113-staged.iso`
- `limitlessos-x86_64-m113-staged-uefi.img`
- `BOOTMAN.TXT`
- `limitlessos-x86_64.size.txt`
- `DYNLDLIMIT`
- `LDLIMIT`
- `hardware-storage-evidence-manifest.json`
- `hardware-storage-evidence-manifest.txt`
- `README-HARDWARE-STORAGE.txt`
- `qemu-storage-stage-gate.txt` when the QEMU gate is not skipped

Validated command:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1 -SkipBuild
```

Generated evidence bundle:

```text
dist\m113-hardware-storage-20260617-221334
```

Manifest excerpt:

```text
iso=limitlessos-x86_64-m113-staged.iso
iso-bytes=4399104
iso-sha256=2b50475171dd6d54cd3d615d23af8b5812f248997b1403bb93f504d6af5414ac
uefi-image=limitlessos-x86_64-m113-staged-uefi.img
uefi-image-bytes=1474560
uefi-image-sha256=5da9b6326012e45af0302e408e68c4126ad7d613036d0e9c50c2ac0947c8a076
dynamic-app=/APPS/DYNLDLIMIT
dynamic-app-bytes=15680
dynamic-app-sha256=9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa
dynamic-interpreter=/APPS/LDLIMIT
dynamic-interpreter-bytes=16704
dynamic-interpreter-sha256=6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238
bios-sector-reserve=101
uefi-byte-reserve=788512
```

The bundled QEMU staged gate re-proved the M111 storage contract:

```text
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions 6 fat32-start 2048 fat32-sectors 8192 gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat 0 busybox-bytes 0 dynldlimit-stat 1 dynldlimit-bytes 15680 ldlimit-stat 1 ldlimit-bytes 16704 boot-staged 1 boot-app-bytes 15680 boot-interp-bytes 16704 boot-status 0 stage-expected 1 dynldlimit-expected 1 ldlimit-expected 1 dynldlimit-match 1 ldlimit-match 1 stage-match 1 token 0x75BC2409
```

M113 non-claims: this does not certify the MSI laptop, add a storage driver, alter kernel behavior, or prove physical NVMe access. It makes the next real laptop run reproducible: boot the bundled ISO, run `hwval`, analyze the transcript with `tools\analyze-hardware-storage-capture.ps1 -RequireStagedDynamicArtifacts`, and target the first reported failing stage.

Final reserves are unchanged from M111 because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

## M114 Physical Hardware Storage Capture Analysis

M114 is accepted as the host-side intake layer for the next real laptop storage capture. It adds:

```powershell
.\tools\analyze-hardware-storage-capture.ps1 -InputPath <captured-hwval-transcript> [-OutputDir <dir>] [-EvidenceManifestPath <manifest.json>] [-RequireStagedDynamicArtifacts]
```

The analyzer wraps `tools\parse-hardware-storage-capture.ps1`, treats parser exit code `2` as a valid failing diagnosis instead of a script crash, and writes:

- `hardware-storage-capture.json`
- `parse-hardware-storage-capture.txt`
- `hardware-storage-analysis.json`
- `hardware-storage-analysis.txt`
- `hardware-storage-analysis.md`

Validation performed:

```powershell
.\tools\analyze-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m114-analysis-qemu -RequireStagedDynamicArtifacts
.\tools\analyze-hardware-storage-capture.ps1 -InputPath .\docs\hardware\msi-cyborg-15-a13ve.md -OutputDir .\build\m114-analysis-legacy -RequireStagedDynamicArtifacts
.\tools\analyze-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m114-analysis-manifest -EvidenceManifestPath .\dist\m113-hardware-storage-20260617-221334\hardware-storage-evidence-manifest.json -RequireStagedDynamicArtifacts
.\tools\parse-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputPath .\build\m114-parser-regression.json -RequireStagedDynamicArtifacts
```

Observed analyzer outcomes:

```text
hardware-storage-analysis: storage-ready
pass: True
detail: NVMe, GPT, FAT, /APPS, capability delegation, and requested staged artifacts are all visible.
next-target: Storage is healthy. Next target: run linux /APPS/DYNLDLIMIT on hardware and capture drs-realbin telemetry.

hardware-storage-analysis: nvme-controller-discovery
pass: False
detail: NVMe controller was not discovered.
next-target: Driver target: PCI/NVMe enumeration, class-code match, BAR mapping, and controller register visibility.
```

M114 also tightens `tools\parse-hardware-storage-capture.ps1` so `raw_line` and `legacy_line` are plain strings in JSON output rather than PowerShell provider-decorated string objects.

M114 non-claims: this does not certify the MSI laptop, add a storage driver, alter kernel behavior, or prove physical NVMe access. It ensures that the next physical `hwval` transcript immediately becomes a clean report with a concrete next implementation target.

Final reserves are unchanged from M111 because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

## M115 Physical Hardware Storage Evidence Verification

M115 is accepted as the final host-side preflight/intake layer before a real laptop storage run. It adds:

```powershell
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir <m113-bundle> [-CapturePath <captured-hwval-transcript>] [-OutputDir <dir>] [-RequireStagedDynamicArtifacts]
```

The verifier checks:

- `hardware-storage-evidence-manifest.json` and text manifest exist
- staged ISO byte count and SHA-256 match the manifest
- staged UEFI image byte count and SHA-256 match the manifest
- `DYNLDLIMIT` byte count and SHA-256 match the manifest
- `LDLIMIT` byte count and SHA-256 match the manifest
- `BOOTMAN.TXT` contains the expected `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` staging lines
- the size map reserves match the manifest
- optional captured `hwval` output is analyzed with M114 and reported as `capture-stage`

Validation performed:

```powershell
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m113-hardware-storage-20260617-221334 -OutputDir .\build\m115-verify-bundle
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m113-hardware-storage-20260617-221334 -CapturePath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m115-verify-qemu -RequireStagedDynamicArtifacts
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m113-hardware-storage-20260617-221334 -CapturePath .\docs\hardware\msi-cyborg-15-a13ve.md -OutputDir .\build\m115-verify-legacy -RequireStagedDynamicArtifacts
```

Positive bundle plus staged QEMU capture:

```text
hardware-storage-evidence: verified
bundle-pass: True
iso-sha256: 2b50475171dd6d54cd3d615d23af8b5812f248997b1403bb93f504d6af5414ac
uefi-image-sha256: 5da9b6326012e45af0302e408e68c4126ad7d613036d0e9c50c2ac0947c8a076
dynamic-app-sha256: 9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa
dynamic-interpreter-sha256: 6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238
bios-sector-reserve: 101
uefi-byte-reserve: 788512
capture-checked: True
capture-pass: True
capture-stage: storage-ready
```

Legacy hardware-note diagnostic:

```text
hardware-storage-evidence: verified
bundle-pass: True
capture-checked: True
capture-pass: False
capture-stage: nvme-controller-discovery
```

M115 also updates newly generated M113 runbooks to call `tools\verify-hardware-storage-evidence.ps1` instead of the lower-level parser directly.

M115 non-claims: this does not certify the MSI laptop, add a storage driver, alter kernel behavior, or prove physical NVMe access. It makes the evidence bundle and capture intake self-checking so the next physical run cannot silently use the wrong media or wrong expected artifact sizes.

Final reserves are unchanged from M111 because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

## M116 Physical Hardware Storage Analysis Fixture Coverage

M116 is accepted as host-side regression coverage for the hardware storage diagnostic chain. It adds:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 [-OutputDir <dir>]
```

The verifier generates small synthetic transcript files and runs each through `tools\analyze-hardware-storage-capture.ps1 -RequireStagedDynamicArtifacts`. It asserts both the expected `stage` and the expected analyzer exit code: `2` for every diagnostic failure and `0` for `storage-ready`.

Accepted command:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1
```

Accepted result:

```text
hardware-storage-analysis-fixtures: 35/35
failed: 0
```

Covered stages:

```text
missing-storage-triage
legacy-realbin-unavailable
nvme-controller-discovery
nvme-controller-ready
nvme-identify
nvme-io-queue
nvme-read-issue
nvme-read-completion
nvme-read-status
gpt-signature
gpt-partition-table
fat32-partition
fat32-vbr
fat32-bpb
fat32-mount
fat32-unavailable
fat32-error
storage-capability
storage-capability-delegation
storage-capability-error
apps-directory-stat
apps-directory-type
apps-directory-read
boot-media-staging
boot-media-app-size
boot-media-interp-size
nvme-dynldlimit-stat
nvme-dynldlimit-size
nvme-ldlimit-stat
nvme-ldlimit-size
stage-expected-flag
dynldlimit-match
ldlimit-match
stage-match
storage-ready
```

M116 non-claims: this does not certify the MSI laptop, add a storage driver, alter kernel behavior, or prove physical NVMe access. It proves the host-side diagnostic classifier is not guessing once the physical `hwval` transcript exists.

Final reserves are unchanged from M111 because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

## M117 Physical Display/Input Capture Analysis

M117 is accepted as the host-side diagnosis layer for the display/input symptoms observed in the real laptop photos and VirtualBox pointer run. It adds:

```powershell
.\tools\analyze-hardware-display-input-capture.ps1 -InputPath <captured-hwval-transcript> [-OutputDir <dir>]
.\tools\verify-hardware-display-input-fixtures.ps1 [-OutputDir <dir>]
```

The analyzer consumes `drs-display-readability`, `drs-ui-polish`, and the existing `hwval` pointer lines for xHCI, I2C HID, PS/2 fallback, mouse packet counts, and cursor visibility. It reports one dependency stage plus a `next-target` implementation hint.

Validated commands:

```powershell
.\tools\analyze-hardware-display-input-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m117-display-input-qemu
.\tools\verify-hardware-display-input-fixtures.ps1
```

Positive QEMU result:

```text
hardware-display-input-analysis: display-input-ready
pass: True
detail: Display is readable, UI initialized, cursor is visible, and pointer packets were received.
mouse-packets: 2
xhci-mouse-endpoint: 1
xhci-mouse-reports: 2
ps2-present: 1
ps2-enabled: 1
```

Fixture coverage:

```text
hardware-display-input-fixtures: 26/26
failed: 0
```

Covered stages:

```text
missing-display-readability
display-unavailable
framebuffer-stride
framebuffer-bounds
display-fit
display-readable
missing-ui-polish
compositor-inactive
font-unavailable
window-manager-unavailable
desktop-unavailable
taskbar-unavailable
launcher-unavailable
windows-unavailable
pointer-moving-cursor-hidden
cursor-hidden
i2c-pointer-reports-no-packets
i2c-pointer-error
i2c-pointer-candidate-unbound
xhci-mouse-reports-no-packets
xhci-mouse-no-reports
xhci-input-error
ps2-mouse-no-packets
ps2-mouse-disabled
no-pointer-backend
display-input-ready
```

M117 non-claims: this does not add a GPU driver, DRM/KMS, a new touchpad driver, or certify the MSI laptop. It makes the next real laptop `hwval` capture classify the observed display overlap, hidden cursor, and touchpad/mouse failure into a concrete implementation target instead of relying on photos.

Final reserves are unchanged from M111 because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

## M118 MSI Hardware Capture Analysis

M118 is accepted as the single host-side intake command for the MSI Cyborg 15 A13VE hardware run. It adds:

```powershell
.\tools\analyze-msi-hardware-capture.ps1 `
  -EvidenceDir <m113-hardware-storage-evidence-dir> `
  -CapturePath <captured-hwval-transcript> `
  -OutputDir <analysis-dir> `
  -RequireStagedDynamicArtifacts
```

The wrapper verifies the evidence bundle first, then runs the storage capture analyzer and display/input analyzer against the same transcript. It writes:

```text
msi-hardware-analysis.json
msi-hardware-analysis.txt
msi-hardware-analysis.md
```

Positive QEMU/staged-evidence proof:

```powershell
.\tools\analyze-msi-hardware-capture.ps1 `
  -EvidenceDir .\dist\m113-hardware-storage-20260617-222838 `
  -CapturePath .\build\qemu-x86_64-uefi-debug.log `
  -OutputDir .\build\m118-msi-qemu `
  -RequireStagedDynamicArtifacts
```

Result:

```text
msi-hardware-analysis: msi-hardware-ready
pass: True
storage-stage: storage-ready
display-input-stage: display-input-ready
bios-sector-reserve: 101
uefi-byte-reserve: 788512
```

Legacy/incomplete-capture proof:

```powershell
.\tools\analyze-msi-hardware-capture.ps1 `
  -EvidenceDir .\dist\m113-hardware-storage-20260617-222838 `
  -CapturePath .\docs\hardware\msi-cyborg-15-a13ve.md `
  -OutputDir .\build\m118-msi-legacy `
  -RequireStagedDynamicArtifacts
```

Result:

```text
msi-hardware-analysis: storage-nvme-controller-discovery
pass: False
storage-stage: nvme-controller-discovery
display-input-stage: missing-display-readability
next-target: Driver target: PCI/NVMe enumeration, class-code match, BAR mapping, and controller register visibility.
```

Regression suites remained green:

```text
hardware-storage-analysis-fixtures: 35/35
failed: 0
hardware-display-input-fixtures: 26/26
failed: 0
```

M118 non-claims: this does not certify the MSI laptop, add a storage driver fix, add a GPU driver, or add a touchpad driver. It makes the next real transcript produce one authoritative first implementation target across storage and display/input instead of splitting evidence across separate tools.

Final reserves are unchanged because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

Proposed M119 scope: run the M118 combined analyzer on a fresh physical MSI `hwval` transcript from the staged evidence ISO, then implement only the first reported hardware stage. If storage fails first, start with the reported NVMe/GPT/FAT/capability stage; if storage passes and display/input fails, start with the reported framebuffer/cursor/pointer backend stage.

## M119 MSI Hardware Capture Analysis Fixture Coverage

M119 is accepted as deterministic regression coverage for the combined M118 MSI analyzer. It adds:

```powershell
.\tools\verify-msi-hardware-analysis-fixtures.ps1 [-OutputDir <dir>]
```

The verifier builds a synthetic evidence bundle under the output directory, including matching `hardware-storage-evidence-manifest.json`, `BOOTMAN.TXT`, size map, `DYNLDLIMIT`, and `LDLIMIT` files with real byte counts and SHA-256s. It then fabricates controlled `hwval` transcripts and runs `tools\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts` against each one.

Accepted command:

```powershell
.\tools\verify-msi-hardware-analysis-fixtures.ps1
```

Result:

```text
msi-hardware-analysis-fixtures: 4/4
failed: 0
```

Covered combined stages:

```text
all-ready: msi-hardware-ready
storage-first: storage-nvme-controller-discovery
display-after-storage: display-input-pointer-moving-cursor-hidden
missing-storage-priority: storage-missing-storage-triage
```

The important proof is ordering: a storage failure remains the top-level target even when display/input passes, and display/input becomes the top-level target only after storage reaches `storage-ready`. Missing storage telemetry also wins over missing display telemetry so the next action is to repeat `hwval` on a suitable staged image instead of debugging a display symptom from an insufficient transcript.

M119 non-claims: this does not add or certify hardware support. It keeps the M118 intake tool from regressing before a fresh physical laptop transcript is available.

Final reserves are unchanged because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum: `0x6714FC97`

## M120 Boot-Media Linux Handoff Verification

M120 is accepted as the hardware-readiness clarification for the `linux /APPS/DYNLDLIMIT` symptom shown in the MSI photos. No kernel code changed in this milestone; the existing boot-media handoff path was verified and the stale runbook language was corrected.

Accepted command:

```powershell
.\tools\verify-boot-media-linux-handoff.ps1
```

The verifier builds an x86_64 UEFI Product image with tiny invalid ELF-shaped probe payloads staged as `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` in the UEFI boot FAT image. This deliberately stops at ELF parsing after the read, so the proof is about source selection and handoff rather than dynamic program execution.

Acceptance output:

```text
Boot-media Linux handoff verifier passed.
BIOS reserve sectors: 101
UEFI reserve bytes: 788512
Command: linux /APPS/DYNLDLIMIT
```

Key telemetry:

```text
[uefi] boot linux stage DYNLDLIMIT attempted 1 loaded 1 bytes 7 pages 1 base 0x0000000000100000 copied 1 token 0x709DAA1E status 0x0000000000000000
[uefi] boot linux stage LDLIMIT attempted 1 loaded 1 bytes 7 pages 1 base 0x0000000000101000 copied 1 token 0x7E82AA7C status 0x0000000000000000
linux: using UEFI boot-media staged file
drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage elf code 2 ... boot-media-read-error 0 boot-media-read-bytes 7 boot-media-read-capacity 4194304
```

M120 changes the MSI interpretation: current staged UEFI images should not fail the initial `/APPS/DYNLDLIMIT` launch merely because NVMe FAT is unavailable. The shell checks the boot-media staged files first and uses Linux exec source `2` when the path matches. NVMe FAT still matters for `/nvme/apps`, Linux VFS file tests, staged artifact agreement, and broader storage readiness.

M120 non-claims: this does not certify the physical MSI laptop, implement a new storage driver, or prove a real dynamic app ran on physical hardware. It proves the boot-media fallback path that avoids the older NVMe-only launch failure.

Final reserves:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum for this verifier build: `0x52B9ED82`

## M121 MSI Hardware Handoff Bundle Refresh

M121 is accepted as the refreshed physical MSI laptop handoff bundle. It updates `tools\prepare-hardware-storage-evidence.ps1` so the generated bundle is no longer framed as an M113 storage-only handoff. The runbook now asks the hardware tester to run both `hwval` and `linux /APPS/DYNLDLIMIT`, capture one transcript, and feed it to the combined M118 analyzer.

Accepted command:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m121-msi-hardware-handoff-20260617-225629 -OutputDir .\build\m121-verify-handoff
```

Generated bundle:

```text
dist\m121-msi-hardware-handoff-20260617-225629
```

Bundle artifacts:

- ISO: `limitlessos-x86_64-m121-handoff.iso`, SHA-256 `937d5694bc5d9b1e4656ff4d445f2cec8a6d3611a2c0c7a1ba3bf96e82ff7daf`
- UEFI FAT image: `limitlessos-x86_64-m121-handoff-uefi.img`, SHA-256 `c1e26eaae6dd8ea572b271f407b72a1a827c294b2d3efda5ea59bfe80ec258d3`
- `/APPS/DYNLDLIMIT`: 15,680 bytes, SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`
- `/APPS/LDLIMIT`: 16,704 bytes, SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`

Verifier output:

```text
hardware-storage-evidence: verified
  bundle pass: True
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
```

The M121 runbook now expects this hardware sequence:

```text
hwval
linux /APPS/DYNLDLIMIT
```

It then analyzes the transcript with:

```powershell
.\tools\analyze-msi-hardware-capture.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -OutputDir <analysis-output-dir> -RequireStagedDynamicArtifacts
```

The runbook records the source-2 boot-media handoff expectation:

```text
linux: using UEFI boot-media staged file
drs-realbin ... source 2 ... boot-media-read 1
```

M121 non-claims: this does not certify the physical MSI laptop, add a new device driver, or prove the real dynamic binary completed on hardware. It packages the current evidence image and points the next physical run at the combined storage/display/input analyzer plus the boot-media Linux handoff path.

Final reserves:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors
- UEFI manifest checksum for the generated build: `0x83CCAC74`

## M122 MSI Hardware Handoff Verifier

M122 is accepted as the regression guard for the physical MSI handoff bundle. It adds `tools\verify-msi-hardware-handoff.ps1`, a host-side verifier that first delegates to `tools\verify-hardware-storage-evidence.ps1` for hashes, byte counts, BOOTMAN staging lines, and reserve checks, then validates the M121-specific contract that the storage-only verifier intentionally does not know about.

Accepted command:

```powershell
.\tools\verify-msi-hardware-handoff.ps1 -EvidenceDir .\dist\m121-msi-hardware-handoff-20260617-225629 -OutputDir .\build\m122-msi-handoff-verifier -RequireStagedDynamicArtifacts
```

Verifier output:

```text
hardware-storage-evidence: verified
  bundle pass: True
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
msi-hardware-handoff: verified
  handoff pass: True
  source2 required: 2
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
```

The verifier rejects bundles that drift away from the M121 handoff contract:

- manifest milestone must be `M121`
- purpose must be `MSI hardware handoff evidence bundle`
- ISO must be `limitlessos-x86_64-m121-handoff.iso`
- UEFI image must be `limitlessos-x86_64-m121-handoff-uefi.img`
- dynamic app/interpreter paths must be `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT`
- manifest must point at `tools\analyze-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts`
- manifest must require boot-media Linux source `2`
- runbook must instruct `hwval` and `linux /APPS/DYNLDLIMIT`
- runbook must include `linux: using UEFI boot-media staged file` and `drs-realbin ... source 2 ... boot-media-read 1`

The verifier can optionally accept a real hardware capture and run the combined MSI analyzer through the same command surface. Without a capture, it proves bundle correctness only.

M122 non-claims: this does not certify the physical MSI laptop and does not add hardware driver support. It prevents stale handoff packages from sending the next hardware run down the wrong diagnostic path.

Final reserves are unchanged because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors

## M123 MSI Hardware Handoff Verifier Fixture Coverage

M123 is accepted as deterministic regression coverage for the M122 handoff verifier. It adds `tools\verify-msi-hardware-handoff-fixtures.ps1`, which synthesizes tiny self-contained evidence bundles whose hashes, byte counts, BOOTMAN lines, size-map reserves, and runbooks are controlled by the test.

Accepted command:

```powershell
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m123-msi-handoff-fixtures
```

Acceptance output:

```text
msi-hardware-handoff-fixtures: 7/7
  failed: 0
```

Fixture coverage:

- `valid-m121`: a correct M121 bundle passes the M122 verifier.
- `stale-milestone`: a storage-valid bundle labeled `M113` is rejected.
- `stale-analyzer`: a storage-valid bundle pointing at the storage-only analyzer is rejected.
- `missing-source2`: a storage-valid bundle without required boot-media Linux source `2` is rejected.
- `stale-iso-name`: a storage-valid bundle using the old `limitlessos-x86_64-m113-staged.iso` name is rejected.
- `missing-linux-command`: a storage-valid runbook that omits `linux /APPS/DYNLDLIMIT` is rejected.
- `missing-source2-runbook`: a storage-valid runbook that omits `drs-realbin ... source 2 ... boot-media-read 1` is rejected.

Every negative fixture still passes the lower-level storage evidence verifier before the M122 handoff verifier rejects it, so the suite proves the new M121/M122 handoff contract specifically instead of accidentally testing generic hash or reserve failures.

M123 non-claims: this does not certify the physical MSI laptop and does not add hardware support. It prevents the handoff verifier itself from regressing while the next physical transcript is still pending.

Final reserves are unchanged because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors

## M124 Self-Verifying MSI Handoff Packaging

M124 is accepted as packaging hardening for the physical MSI handoff. `tools\prepare-hardware-storage-evidence.ps1` now calls `tools\verify-msi-hardware-handoff.ps1` after writing the evidence bundle, unless explicitly passed `-SkipHandoffVerify`. This means a freshly generated package fails before handoff if it has stale M113 naming, the wrong analyzer path, missing source-2 boot-media expectations, bad hashes/reserves, or an incomplete runbook.

Accepted command:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1 -EvidenceDir .\build\m124-self-verified-handoff -SkipBuild -SkipQemuGate
```

Acceptance output:

```text
hardware-storage-evidence: verified
  bundle pass: True
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
msi-hardware-handoff: verified
  handoff pass: True
  source2 required: 2
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
M121 MSI hardware handoff evidence bundle: .\build\m124-self-verified-handoff
```

The packager now writes these verifier artifacts into the generated bundle:

- `msi-handoff-verification.txt`
- `msi-handoff-verification\storage-evidence\hardware-storage-evidence-verification.json`
- `msi-handoff-verification\msi-hardware-handoff-verification.json`

The generated runbook now puts the handoff verifier in front of the direct analyzer command:

```powershell
.\tools\verify-msi-hardware-handoff.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -RequireStagedDynamicArtifacts
```

M124 non-claims: this does not certify the physical MSI laptop and does not add hardware support. It reduces the chance that a bad or stale USB handoff package reaches hardware testing.

Final reserves are unchanged because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors

## M125 MSI Dynamic Handoff Capture Classification

M125 is accepted as capture-side classification for the physical MSI `linux /APPS/DYNLDLIMIT` command. `tools\verify-msi-hardware-handoff.ps1` still verifies the bundle contract, storage evidence, and combined MSI analysis, but when `-CapturePath` is supplied it now also parses the real dynamic command telemetry and writes dedicated `dynamic_handoff_*` fields into `msi-hardware-handoff-verification.json`.

Accepted command:

```powershell
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m125-msi-handoff-fixtures-final
```

Acceptance output:

```text
msi-hardware-handoff-fixtures: 11/11
  failed: 0
```

New capture-side dynamic stages:

- `dynamic-runtime-static`: source 2 and boot-media read succeeded, then the realbin line failed later at stage `static`.
- `dynamic-runtime-exit0`: source 2 and boot-media read succeeded, then the dynamic process exited 0.
- `dynamic-handoff-nvme-unavailable`: capture still shows the old `linux: NVMe FAT unavailable` / `drs-realbin-unavailable` path.
- `dynamic-handoff-wrong-source`: capture produced `drs-realbin` for `/APPS/DYNLDLIMIT`, but not with source `2`.
- `dynamic-handoff-missing-realbin`: capture does not contain usable `drs-realbin` telemetry for `/APPS/DYNLDLIMIT`.
- `dynamic-handoff-boot-media-read`: source 2 was selected but boot-media read success was not proven.

Fixture coverage now includes:

- 7 bundle/runbook contract cases from M123
- `capture-source2-runtime-fail`
- `capture-nvme-unavailable`
- `capture-wrong-source`
- `capture-source2-exit0`

This means a physical transcript can now report storage/display ready while still failing the handoff with a precise dynamic stage instead of losing that information behind the broader `msi-hardware-ready` result.

M125 non-claims: this does not certify the physical MSI laptop and does not add hardware support. It improves the capture analyzer/verifier so the next hardware run has a sharper first-failure answer.

Final reserves are unchanged because no kernel code changed:

- UEFI kernel bytes: 1,308,640 / 2,097,152, reserve 788,512 bytes
- BIOS kernel bytes: 472,160, reserve 101 sectors

Proposed M126 scope: boot the M121/M125 self-verified bundle on the physical MSI laptop, capture the full `hwval` and `linux /APPS/DYNLDLIMIT` transcript, run `tools\verify-msi-hardware-handoff.ps1` with `-CapturePath`, and implement only the first reported hardware stage. If `dynamic-handoff-stage` is `dynamic-runtime-*`, the next target is the named dynamic runtime failure; if it is `dynamic-handoff-*`, the next target is the physical boot-media/source-selection path; otherwise use the first storage or display/input stage.

## Persistence

Persistence is reboot-surviving in the verifier. `verify-nvme-persistence.ps1` runs two sequential boots against the same NVMe GPT image, observes content written in the first boot from the second boot, and prints:

- scoped write authority required
- commit authority required
- wrong-owner denial observed
- stale/revoked denial observed
- read-only write denial observed
- nonzero commit/flush counter
- same NVMe image reused
- not RAM-backed
- exit code 0

## Remaining Daily-Driver Blockers

- Product BIOS reserve is currently 101 sectors after rebuilding the present source, below the 128-sector warning threshold but still inside the hard 1024-sector loader limit.
- Product UEFI now uses the 2 MiB file-size contract; BIOS remains on the 1024-sector loader contract.
- UEFI remains governed by `KERNEL64.BIN` byte budget, manifest/checksum correctness, placement/load correctness, and artifact inventory correctness. UEFI is not blocked by the BIOS 1024-sector ceiling.
- GUI/window manager/desktop are Product for Terminal, File Manager, Settings, Installer, brokered input, focused keyboard routing, and compositor-owned display.
- Network now has an M19 brokered TCP-client socket foundation over the hardware-gated DHCP/DNS/HTTP path, and M21 packages one manifest-backed native app that consumes it from Ring 3 through generic descriptor validation. There is still no general socket library, server socket API, raw packet API, arbitrary app send/receive data plane, or ambient network authority.
- Real static Linux ELF execution now works on the UEFI Product path for externally built BusyBox static ET_EXEC binaries loaded from NVMe FAT and linked into the supported user VMA window. M22 adds per-process page tables for UEFI Product Linux persona launches with shared kernel/MMIO mappings and scheduler CR3 switching. M23 adds bounded fork/wait for the observed BusyBox ash external-command path. M24 adds Unix pipe composition across two forked BusyBox ash pipeline children. M25 adds Linux-visible BusyBox applet aliases and real external `execve` transfer to a 64 KiB real-binary stack. M26 proves forked child exec images inherit cwd, VFS authority, and pipe endpoints correctly for BusyBox aliases. M27 proves an independently staged static ET_EXEC can run through Linux-visible NVMe VFS child `execve` without using the BusyBox alias backend. M28 proves an upstream non-BusyBox static ET_EXEC utility from suckless sbase can launch directly and through forked BusyBox ash pipeline exec. M29 proves a second upstream sbase static ET_EXEC utility can read NVMe file content directly and consume a pipeline from another third-party ET_EXEC utility without relying on the BusyBox alias backend. M30-M61 prove `/usr/local/bin` PATH discovery, bounded default environment setup, shell-updated `PWD`, child-observed exported environment mutation, non-shell `execvp`, directory enumeration, absolute localbin execution, absolute localbin file reads, cwd-relative file reads, `..` relative path normalization, current-directory `./` localbin execution, non-shell current-directory `./` exec handoff, relative executable `..` localbin handoff, absolute executable `..` localbin handoff, mixed absolute executable `.`/`..` localbin handoff, repeated-slash absolute executable localbin handoff, root-clamped absolute executable `..` localbin handoff, over-root absolute executable `..` clamp handoff, trailing-slash executable denial, trailing-slash directory enumeration, missing absolute-localbin executable denial, missing default-PATH executable denial, missing non-shell `execvp` executable denial, non-shell trailing-slash executable denial, non-shell slash-suffixed directory-target executable denial, non-shell bare-directory executable denial, non-shell dot-directory executable denial, non-shell parent-directory executable denial, and non-shell parent-rebased directory executable denial for third-party static ET_EXEC utilities. M70-M105 advance dynamic ELF from denial-only metadata through bounded interpreter staging, relocation discovery, symbol/provider classification, weak-null admission, dry-run target/value validation, safe GOT/PLT write/readback, dynamic stack/auxv construction, supported-interpreter task registration, first dynamic execution, `getpid` binding, non-syscall libc helper binding, inherited environment binding, stdio helper output, bounded heap helpers, environment mutation, first dynamic pthread helper execution, multi-threaded dynamic pthread TLS/condition/futex contention, dynamic NVMe VFS file open/read/write/close, dynamic file metadata/seek behavior, dynamic directory enumeration, dynamic cwd/relative path behavior, dynamic vectored I/O/readiness behavior, dynamic fstatat metadata behavior, dynamic openat relative file-read behavior, dynamic openat dirfd-relative lookup behavior, dynamic fchdir cwd handoff behavior, dynamic fcntl descriptor/status flag behavior, dynamic fcntl descriptor duplication behavior, direct dynamic dup syscall behavior, direct dynamic pipe syscall behavior, dynamic fork-plus-pipe/wait composition, blocked pipe read replay, and dynamic pipe close/error semantics. Arbitrary dynamic linker search/loading, glibc dynamic linking, vfork, broad clone flag compatibility, sockets in the Linux ABI table, broad third-party package compatibility, and broad distro compatibility remain unavailable.
- M104 continues that dynamic path with `/APPS/DYNFORKPIPE`, proving a parent blocked in `read(pipe)` can be resumed by replaying the original syscall under the parent CR3 after a forked child writes to the pipe, with visible output `dynforkpipe:child-pipe`, `pipe-blocks 1`, `pipe-wakes 1`, `pipe-replays 1`, `read 1`, `read-bytes 10`, `fd-fork-pipe-copy 2`, `fork 1`, `fork-success 1`, `wait4 1`, `wait4-reap 1`, `wait4-last-exit-code 7`, `child-root-cleanup 1`, `root-cleanup 2`, `pipe-live-final 0`, `exit 0`, `low-compat 0`, `syscall-root-repair 0`, `page-faults 0`, and `pml4-pool-used-final 0`.
- M105 extends the dynamic pipe proof with `/APPS/DYNPIPECLOSE`, proving EOF after all write ends close and handled `SIGPIPE`/`EPIPE` after all read ends close, with visible output `dynpipeclose:eof`, `sigpipe-caught`, and `dynpipeclose:done`, plus `signal-sigpipe 1`, `signal-rt-sigreturn 1`, `signal-frame-fault 0`, `read 1`, `read-bytes 0`, `write 3`, `write-bytes 50`, `pipe 2`, `pipe-create 2`, `pipe-live-final 0`, `pipe-blocks 1`, `pipe-wakes 1`, `pipe-replays 1`, `fork 1`, `fork-success 1`, `wait4 1`, `wait4-reap 1`, `wait4-last-exit-code 7`, `root-cleanup 2`, `exit 0`, `low-compat 0`, `syscall-root-repair 0`, `page-faults 0`, and `pml4-pool-used-final 0`.
- Installer UX planning and dry-run are Product, but real internal install/write, formatting, and boot-entry authority remain unavailable.
- M7.1 signed package admission has separate negative fixture coverage, but package-manager UI, app store, auto-install, live public update fetching, and trusted-time expiry enforcement remain unavailable.
- M6 has a local console session model, not full multiuser login/authentication.
- M11 has a local identity model and vault foundation only; personal login, enterprise login, cloud association, cloud storage, encrypted secret storage, and token storage remain unavailable.
- M12 has an identity transport foundation only; encrypted account transport, credential transport, token persistence, remote login, and trusted-time expiry enforcement remain unavailable.
- M13 has an account association status foundation only; local association is active, but personal/enterprise/cloud association, account linking, security-key login, enterprise policy, token persistence, and remote account authority remain unavailable.
- M14 has a cloud-storage broker foundation only; real public cloud storage, sync, automatic upload/download, offline cache, token storage, encrypted cloud transport, AI cloud access, and app-direct cloud authority remain unavailable or denied.
- M15 has installer UX planning only; destructive install actions remain disabled by default and not Product-approved.
- Assistant remains Mode B with read-only context flow and predefined consent-scoped action templates only. Inference backend, model transport, generated answers, broad automation, autonomous action, package install/update, settings mutation, cloud AI, cloud memory, and internal install/write remain unavailable.
- M106 adds the UEFI Product universal hardware inventory and driver-binding evidence core, but hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
