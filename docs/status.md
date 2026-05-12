# LimitlessOS Status

Last updated: 2026-05-11

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

M6 is `Service Manager + User/Session Model`.

M6 promotes service lifecycle/status and local session authority into the Product path. It keeps the accepted M5 safe installer dry-run boundary, does not enable real internal-disk install writes, and does not add AI, package manager, browser, app store, cloud sync, or M7 installer functionality.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifact:

- kernel bytes: 474528
- BIOS kernel sectors: 927 / 1024
- BIOS reserve: 97
- UEFI kernel byte limit: 2,097,152 bytes
- UEFI byte reserve: 1,622,624
- checksum: 0xAFCCC5B6
- BIOS sector budget status: warning, because reserve is below 128
- boot contract: dual path. BIOS keeps the 1024-sector hard limit and 128-sector warning. UEFI Product uses a 2 MiB `KERNEL64.BIN` file-size contract verified against `BOOTMAN.TXT` byte count and checksum, with no UEFI sector arithmetic.

Product behavior:

- x86_64 boot through disk, UEFI removable media, and UEFI ISO verification
- persistent ring-3 shell
- Product desktop with brokered compositor/window-manager authority
- truthful `help`, `apps`, and `ls apps` output
- brokered DHCP/DNS/TCP/HTTP network status through the `net` shell builtin when virtio-net or e1000e hardware is present
- Terminal, File Manager, and Settings GUI apps opened through real click interaction
- Product service lifecycle/status query surface
- exactly one local console session with input, display, filesystem, network-status, and installer-dry-run authority scoped to that session
- brokered persistent file workflow
- NVMe persistence verification path
- capability denial checks
- no ambient authority

M4.1 real-hardware validation:

- MSI Cyborg 15 A13VE checklist: `docs/hardware/msi-cyborg-15-a13ve.md`
- status: pending user-supplied physical results
- required boot mode: UEFI USB
- unsafe internal partitions must not be browsed or written
- internal NVMe writes remain disabled unless a later safe installer path explicitly enables them through scoped authority

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
- Product services are policy/security broker, console/shell broker, input broker, display/compositor, window manager/desktop shell, filesystem broker, block/storage broker, hardware inventory broker, network broker, installer dry-run service/tool, and settings/system-info provider
- Settings is the Product-safe status surface for service/session information
- controlled restart verification is limited to the scoped settings/system-info provider path
- stale capabilities from the old generation are denied
- M6 has one local console session, not full multiuser login/authentication
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
- RENAME
- STAT
- TOUCH
- WRITE

Product builtins:

- apps
- help
- info
- net
- pwd

Product GUI apps:

- Terminal
- File Manager
- Settings

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
- installer write/install authority
- package manager
- AI assistant behavior

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

- Product BIOS reserve is still below the 128-sector warning threshold.
- BIOS reserve is only 97 sectors, which is 1 sector above the 96-sector hard floor. M6 worsened reserve by two sectors to add Product service/session status and verification telemetry. Installer write/install code and M7 work must not be added to the BIOS-constrained Product kernel while reserve remains below 128 sectors unless the boot contract is intentionally changed.
- Product UEFI now uses the 2 MiB file-size contract; BIOS remains on the 1024-sector loader contract.
- UEFI remains governed by `KERNEL64.BIN` byte budget, manifest/checksum correctness, placement/load correctness, and artifact inventory correctness. UEFI is not blocked by the BIOS 1024-sector ceiling.
- GUI/window manager/desktop are Product only for the M4 surface: Terminal, File Manager, Settings, brokered input, focused keyboard routing, and compositor-owned display.
- Network is Product only as a brokered hardware-gated status path. There is no socket API, packet API, or ambient network authority.
- Installer dry-run is Product safety tooling, but installer write/install authority and package manager behavior are unavailable.
- M6 has a local console session model, not full multiuser login/authentication.
- There is no real AI assistant path.
- Hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
