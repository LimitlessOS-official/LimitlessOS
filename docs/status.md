# LimitlessOS Status

Last updated: 2026-09-27. Milestone narratives for M1–M192 are archived in [history/status-log.md](history/status-log.md).

## Current milestone

### M197: Local desktop launcher

`tools\run-desktop.ps1` runs the Product desktop in a QEMU window without writing a USB stick. It rebuilds when any source is newer than the last build, boots the UEFI image on the same virtual hardware as the UEFI gate, and keeps a persistent virtual NVMe disk in `%LOCALAPPDATA%\LimitlessOS`. `tools\install-desktop-shortcut.ps1` adds a desktop icon for it. `tools\run-qemu.ps1` had a PowerShell parse error (a trailing comma in its argument list) and could not run at all; that is fixed.

Accepted verification (2026-09-27): a cold launch rebuilt the image, created the disk, and reached `persistent ring3 shell online` in the QEMU window (first-run fallback account created on the persistent NVMe, `user-store-persistent 1`). A second launch through the desktop shortcut while that VM was open was refused by the single-instance guard. All three scripts parse cleanly.

### M196: Continuous integration

`.github/workflows/verify.yml` builds the x86_64 Product image and runs the UEFI and BIOS disk QEMU gates on every pull request and push to `main` (see [development.md](development.md#continuous-integration)). The M195 failures had gone unnoticed because gates only ran by hand; they now run on every change.

M196 also investigated the BIOS reserve (still 101 sectors). The largest BIOS contributors are the AHCI planning chain in `mmio.c` (about 91 KB for the `driver_read` stages alone) and the syscall dispatch tables in `syscall.c` (about 27 KB of 8-byte function pointers to one-line getters). Neither `--gc-sections` nor alignment flags help: the getters are already 7 bytes with no padding. Recovering 27+ sectors therefore needs a structural change, such as a BIOS-only AHCI planner or 32-bit dispatch tables emitted from assembly, and remains a roadmap item.

### M195: Stale QEMU gates restored (BIOS disk, NVMe storage)

Three QEMU gates had been failing on stale verifier expectations, not kernel bugs. The BIOS disk gate (`verify-qemu.ps1 -Architecture x86_64 -BootMedia disk -BuildProfile Product`) had failed since M190:

- **Keyboard telemetry:** the BIOS assertions read the kernel's pre-shell keyboard snapshot and require nonzero scancodes. Before M190 the harness pressed Enter as soon as QEMU started, and QEMU's PS/2 queue held that key until the kernel's `KEYBOARD WAIT` probe read it. M190 made every non-GUI run wait for the shell before typing, which its hwval-filter gate needed, so the BIOS snapshot always read `scancodes 0`. `Send-QemuKeyboardProbe` now takes `-EarlyBootKeyEnabled`, set only for `-BootMedia disk`; UEFI runs keep the M190 behavior.
- **Help wording:** the July 2026 wording pass updated the `hwval` help and `apps` lines to end in "; hardware evidence pending" but only ran the UEFI gate. The frozen BIOS kernel still prints the shorter lines. The verifier now expects the lane-specific text instead of growing the BIOS kernel.

- **NVMe storage gates:** the `-HardwareStorageGate` and `-HardwareStorageStageGate` NVMe triage assertions predated the M161 controller register fields (`nvme-probe-error` through `nvme-doorbell-page`) and could no longer match. They went unnoticed because handoff bundles were being built with `-SkipQemuGate`. Both patterns now include those fields, requiring `nvme-probe-error 0`, `nvme-regs 1`, and a nonzero version register.

M193's write-up blamed this failure on QEMU key timing on this host. That was wrong: the "untouched baseline" used for comparison already contained M190.

Accepted verification (2026-09-27):

- BIOS disk gate passed.
- `verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- `prepare-hardware-storage-evidence.ps1` passed without `-SkipQemuGate`, including the staged storage gate (`stage-match 1`, both staged files matching their NVMe copies) and handoff self-verification. It produced `dist\m133-msi-hardware-handoff-20260927-191559` (ISO SHA-256 `67bfae9c98101074d3d3a39ce8aede275b5ff136c81f21f676a9740f5d71ba31`, UEFI image SHA-256 `555217475559db56c501f2a4cc6561d8532a7889020afce04c7198aedef25065`).
- No kernel source changed, so every budget is unchanged.

### M194: Repository and documentation reorganization

- README, architecture, roadmap, status, and real-binary-gate docs rewritten to match the code; new [budgets.md](budgets.md), [development.md](development.md), [tools/README.md](../tools/README.md), and [fixtures/README.md](../fixtures/README.md).
- Milestone-numbered subsystem specs moved to `docs/subsystems/`; superseded narratives (old README, status, roadmap, real-binary-gate log, architecture bring-up notes, overnight polish log) archived verbatim in `docs/history/`.
- Linux persona test-program sources moved from the ignored `external/build/` into tracked `fixtures/linux/`; the embedded-program sources moved from `tools/` to `fixtures/embedded/`.
- Stray root files `powershell` (empty) and `stdout` (hex dump) removed. `verify-boot-media-linux-handoff.ps1` now stages in the system temp directory instead of `.codex-stage/`.
- Untracked clutter removed: about 15.5 GB of superseded M1–M18.1 evidence packs and May logs from `dist/` (hardware captures and handoff bundles kept), redundant toolchain archives and the unused zig 0.16.0 from `external/tools/`, and the unused `codex-replay` npm install.
- `tools/toolchain.ps1` locates the MSYS2/QEMU toolchain so builds work without editing PATH.

### M193: UEFI low-window budget and boot-media stage relocation

Fixes the defect M191 left open: with `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` staged, the kernel page-faulted inside `syscall64_dispatch` before login.

- **Root cause:** the loader staged boot-media files at physical `0x100000` and `boot_low_alias_physical()` punched those pages through the kernel's low alias. The kernel runs from a fallback window (fixed `0x10000` placement fails on OVMF), and its `.text` now extends past `0x100000`, so the staged interpreter replaced live kernel code. No budget measured the kernel's in-memory footprint; the tracked UEFI budget only compared file size with the 2 MiB buffer.
- **Fix:** new contract in `kernel/include/boot_info.h`. Staged files go into a 256 KiB stage area at `0xFC0000`–`0x1000000`, inside the loader-owned kernel window and above `__kernel_end`. The alias punch-through was removed. The fixed placement now reserves the whole window (previously only the file's pages, leaving `.bss` unreserved from firmware). `build.ps1` links the kernel before compiling the loader, passes `__kernel_end` in as `LIMITLESS_UEFI_KERNEL_IMAGE_END`, and fails the build on overlap. `boot_media.c` rejects staged ranges outside the stage area.
- **Accepted verification (2026-09-27, gcc 16.2.0):**
  - Product build with and without staged artifacts: no compiler warnings, M1 production-slice gate passed.
  - `verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed with and without staging; with staging, files load at `0xFC0000`/`0xFC4000` and boot reaches login.
  - `verify-qemu.ps1 ... -RealBinaryGate -ExtraShellLine "linux /APPS/DYNLDLIMIT"` passed: source 2 (boot media), interpreter read, last syscall `exit_group` result 0, page faults 0.
  - `verify-boot-media-linux-handoff.ps1` passed (stage bases `0xFC0000`/`0xFC1000`); `verify-private-key-artifacts.ps1` passed.
  - `KERNEL64-BIOS.BIN` is byte-identical to the pre-M193 tree (same SHA-256). The BIOS disk gate failed both before and after M193 because of stale verifier expectations from M190; fixed in M195.
  - Budgets: BIOS 923/1024 sectors (101 reserve); UEFI kernel 1,411,968 / 2,097,152 B; low window ends `0xF3AE40` (545,216 B reserve); FAT image 369/2044 clusters.
- **Not proven:** physical MSI boot of the staged image. In QEMU `stage-match` stays 0 because the test NVMe image carries no copies of the staged files to compare against.

### Recent

- **M192 Product UI typeface:** original 8x16 font for UEFI console, shell, and chrome text; BIOS keeps the 5x7 face; captions keep the 5x7 tier deliberately.
- **M191 UEFI FAT image capacity:** FAT12 boot image grown from 1.44 MB floppy geometry to 8 MiB with a solved FAT size and a printed budget.
- **M190 and earlier:** see the history log.

## What works

Verified under QEMU/OVMF by `verify-qemu.ps1` unless marked otherwise.

- **Boot:** UEFI USB image and ISO; `BOOTX64.EFI` verifies `KERNEL64.BIN` against `BOOTMAN.TXT`, exits boot services, and enters the higher-half kernel. BIOS disk boot of the fallback kernel.
- **Session:** first-run setup, bcrypt login (`/USERDB.TXT` on NVMe), 3-strike lockout, lock/unlock.
- **Desktop:** compositor, window manager (focus, drag, resize, close), Terminal, File Manager, Settings, Installer (dry-run), Assistant (consent-scoped status and action templates; no model backend).
- **Shell builtins (UEFI):** `apps devices dev hwdevices lsdev export exporthw help hwfull hwval hwexport info linux lock net open pkginfo port ports pwd usbscan`. BIOS fallback: `apps help hwval info linux net pkginfo pwd`, with `linux` and `lock` reporting unavailable.
- **Native apps:** `append cat copy delete ls mkdir move nethello rename stat touch write` (signed `.APP` descriptors plus flat binaries in `/APPS`).
- **Linux persona:** static and dynamic musl binaries; see [real-binary-gate.md](real-binary-gate.md).
- **Storage:** NVMe FAT32 read/write with scoped write and commit authority, proven across two boots by `verify-nvme-persistence.ps1`.
- **Network:** DHCP, DNS, TCP, and HTTP over virtio-net or e1000e; brokered TCP-client socket API for apps; raw, listen, and ambient network access are denied.
- **Packages:** Ed25519-signed package store and update index with rollback, replay, tamper, and wrong-key denials; install/apply disabled.

## Physical hardware (MSI Cyborg 15 A13VE)

From user-supplied captures ([hardware/msi-cyborg-15-a13ve.md](hardware/msi-cyborg-15-a13ve.md)):

- Boots the UEFI Product image to the desktop and shell; keyboard works.
- **Open:** touchpad (I2C HID over ACPI) and USB mouse do not yet move the cursor; the internal NVMe sits behind Intel VMD/RST and is not yet readable; the display layout needs a retest with the M192 font.
- The M193 staging fix and the current handoff bundle have not been retested on the laptop.

## Known issues and limits

- **BIOS reserve below warning:** 101 sectors against the 128-sector warning line (hard limit respected). Recovery plan in [budgets.md](budgets.md).
- **Static kernel pools:** at most 8 per-process page-table roots, 32 persona contexts, and 16 pipes; there is no general physical-frame allocator on x86_64.
- **Personas:** Windows PE and macOS Mach-O support is loader and ABI groundwork exercised only by repo-built programs; not Product behavior.
- **Reproducibility gaps:** the `LDLIMIT` interpreter binary has no source in the repository or `external/`; build command lines for the `fixtures/linux/` programs were not recorded.
- **32-bit lane:** `memory_init()` in `kernel/core/memory.c` computes extended memory in `u32` and overflows on machines reporting 4 GiB or more (x86 lane only).

## Persistence proof

`verify-nvme-persistence.ps1` runs two sequential boots against the same NVMe GPT image and checks that content written in boot one is read in boot two. It also requires scoped write and commit authority, wrong-owner, stale, and read-only denials, a nonzero commit counter, the same image reused, and no RAM backing.
