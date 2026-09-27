# LimitlessOS Status

Last updated: 2026-09-27. Milestone narratives for M1–M192 are archived in [history/status-log.md](history/status-log.md).

## Current milestone

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
  - `KERNEL64-BIOS.BIN` is byte-identical to the pre-M193 tree (same SHA-256). The BIOS disk gate fails on this host both before and after M193; see Known issues.
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
- **BIOS disk gate fails on the current host:** `verify-qemu.ps1 -Architecture x86_64 -BootMedia disk -BuildProfile Product` stops at "x64 PS/2 keyboard input telemetry proof was not observed". The kernel's 20-tick keyboard probe window records `scancodes 0` because the QMP-injected keys arrive outside it. It reproduces identically on the untouched pre-M193 tree with a byte-identical BIOS kernel, so it is a harness timing issue with the QEMU build installed on 2026-09-27, not a kernel regression. The UEFI gates are unaffected (their keyboard probe is optional).
- **Static kernel pools:** at most 8 per-process page-table roots, 32 persona contexts, and 16 pipes; there is no general physical-frame allocator on x86_64.
- **Personas:** Windows PE and macOS Mach-O support is loader and ABI groundwork exercised only by repo-built programs; not Product behavior.
- **Reproducibility gaps:** the `LDLIMIT` interpreter binary has no source in the repository or `external/`; build command lines for the `fixtures/linux/` programs were not recorded.
- **32-bit lane:** `memory_init()` in `kernel/core/memory.c` computes extended memory in `u32` and overflows on machines reporting 4 GiB or more (x86 lane only).

## Persistence proof

`verify-nvme-persistence.ps1` runs two sequential boots against the same NVMe GPT image and checks that content written in boot one is read in boot two. It also requires scoped write and commit authority, wrong-owner, stale, and read-only denials, a nonzero commit counter, the same image reused, and no RAM backing.
