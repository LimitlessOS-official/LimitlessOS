# LimitlessOS Status

Last updated: 2026-09-28. Milestone narratives for M1–M192 are archived in [history/status-log.md](history/status-log.md).

## Current milestone

### M201: Full verifier sweep, real clock, shell basics, power control

Every `tools/verify-*.ps1` script plus the ISO, x86, and e1000e QEMU lanes was run. The failures and the gaps found along the way were fixed:

- **Wall clock.** New CMOS RTC driver (`rtc.c`). The status bar shows the date and time and the taskbar shows `HH:MM` (UTC, as the firmware keeps it), refreshed each minute from the shell's input wait. Linux `CLOCK_REALTIME`, `gettimeofday`, and `time` return Unix time; before, "real time" was seconds since boot.
- **Shell.** New builtins: `cd` (with `.` and `..`; `pwd` and bare `ls` follow it), `echo` (also `echo text > file`), `clear`, `date`, `uptime`, `whoami`, `reboot`, and `shutdown`. Unknown commands are named (`unknown command: foo (type help)`) instead of the fixed `unknown: help`, and `exit` explains how to leave the session.
- **Power.** New `power.c`: `shutdown` enters ACPI S5 through PM1a/PM1b_CNT with the `_S5_` sleep type read from the DSDT; `reboot` tries the FADT reset register, then port `0xCF9`, then the keyboard controller.
- **Randomness.** Linux `getrandom` was seeded from a constant and the tick count, and replaced every zero byte (biased output). It now draws from a shared kernel entropy pool (`entropy.c`: RDRAND when present, the TSC, and login keystroke timing), which also produces the login salts.
- **Linux persona.** Added `uname`, `getuid`, `getgid`, `getegid`, `gettimeofday`, and `time`, which common programs call at startup.
- **ACPI tables were unreadable after APIC setup.** The ACPI GNVS and DSDT windows (`0x90210000`, `0x90220000`), which the I2C HID touchpad search uses, sit in the same 2 MiB region as the LAPIC/IOAPIC mapping. The MMIO mapper refused any window whose page-directory slot already pointed at another table, so once the APIC was mapped (always, on hardware) every ACPI table mapping failed. Kernel windows in that region now use the APIC page table's free entries, and APIC setup no longer wipes them. This is a likely contributor to the touchpad not working on the MSI laptop, to be confirmed on hardware.
- **PCI ECAM could read stale mappings.** The windows in the `0x90000000` region share one page table, so a later mapping could replace entries PCI ECAM had cached for its bus. PCI now remaps ECAM whenever another mapping was installed since.
- **Boot-time GUI probe.** Every UEFI boot waited up to 60 s of PIT time in a desktop input probe before starting the shell. It now ends when every probe step is seen, 20 s after the last input, or 30 s after boot with no input at all.
- **Tooling fixes.** The installer (`installer-common.ps1`) hashed with `Get-FileHash`, which does not load in Windows PowerShell started from PowerShell 7, so M5 and the M9 dry-run parser failed; it now hashes through .NET. `verify-boot-media-linux-handoff` left its deliberately invalid payloads staged in `dist/`, which broke every later UEFI run until a rebuild; it now restores the normal image. `verify-nvme-persistence` waited for a login prompt that first-run setup never shows. M8, M9, M16, and M17 patterns had drifted from the current help and `pkginfo` wording. The Settings export click scrolls to the top first so a lost wheel notch cannot shift the row.

### M200: Login echo, per-account password salts, desktop cleanup, and the plain UEFI gate

- **Password storage.** Every install used to hash passwords with bcrypt cost 4 and one fixed salt, so identical passwords produced identical hashes everywhere. New records use cost 10 and a random 16-byte salt per account. The salt comes from a pool that mixes the TSC at each login keystroke with RDRAND when the CPU has it (salts need to be unique, not secret). Verification now hashes with the stored hash as its own setting, so older records still verify. On the next successful sign-in (password, lock screen, or default-account sign-in) a pre-M200 record is rewritten with a fresh salt and the new cost. The boot-time wrong-password probe hashes once instead of three times, so the higher cost does not slow boot.
- **Login echo.** The login, first-run, and lock screens now show what is being typed: username characters in clear, and one `*` per password character, with backspace applied. The reader peeks at the pending keyboard line (`input64_keyboard_peek_line`) without consuming it and redraws only the active field (`display64_login_field_draw`). The fixed `********` mask is gone, and the username chosen at first run stays in its field while the password is typed.
- **Boot remnants on the desktop.** Boot-stage lines (the `SHELL` marker) were drawn straight onto the framebuffer after the desktop was up, and early ring 3 output landed outside the terminal window; both stayed on screen until something forced a full repaint. Stage lines now stop once the desktop owns the screen, and the desktop repaints once as the persistent shell starts.
- **Minimized side panels lingered** after the desktop probe; the probe now finishes with a full redraw.
- **Plain UEFI gate fixed and added to CI.** `verify-qemu.ps1 -BootMedia uefi` (the gate that drives the GUI probe, first-run login, and `lock`) had failed since the Product desktop layout landed, because the verifier still computed window positions for the older layout. Its File Manager and Settings clicks landed on the terminal, its close click missed the terminal's close button by a pixel, and the untouched shell window kept keyboard focus away from the commands that followed. The probe now uses the Product geometry (side windows against the right edge, the 832 px shell terminal at 1280 wide, title-button centres) and scrolls Settings to reach the diagnostics-export row. The `help` builtins assertion was updated to the current list. CI runs it as a third QEMU step.
- **Terminal overlay.** The `Scrollback` badge only appears while the view is scrolled back. The `Copied` label read the badge coordinates even when the badge was not drawn (uninitialized values); it now has its own position.

Accepted verification (2026-09-27): a first-run account was stored as `$2b$10$` with a random salt and signed in again on the next boot, and `lock` rejected a wrong password and accepted the right one. A default-account record written by the previous build (`$2b$04$` with the fixed salt) was rewritten to `$2b$10$` with a new salt on the first boot of this build, then signed in without another rewrite. Login-to-prompt time stayed at about 0.3 s under QEMU TCG. QMP captures show `alice` in the username field after typing `alicex` and Backspace, `****` after four password keys, and the name kept in its field on the password step; the desktop's first frame after login has no stage strip or stray text. Build had no warnings; BIOS 923/1024 sectors unchanged. `verify-qemu.ps1 -BootMedia uefi` (twice), `verify-qemu.ps1 -BootMedia uefi -HardwareDisplayGate`, `verify-qemu.ps1 -BootMedia disk`, and both first-run tests (Enter alone, no input) passed; the plain gate reports `fileman-write 1`, `fileman-mkdir 1`, `fileman-edit-commit 1`, `settings-export 1`, and `drs-gui-close-completed 1`.

### M199: Desktop back buffer, smooth drag, and terminal scrollback

Reported from the local desktop: dragging windows was glitchy, and scrolling or clicking left cursor remnants and a broken scroll. Reproduced headless with QMP input and `screendump` captures.

- **Root cause: no back buffer.** The compositor allocated its back buffer right after `.bss` inside the 16 MiB low window. The kernel image now ends at `0xF3AE40`, so the 4 MB buffer for 1280x800 never fit, and the compositor silently fell back to direct mode, drawing every redraw step straight onto the screen. Captures showed blank frames mid-drag, half-drawn windows, and a black box under the cursor. The loader now allocates a 32 MiB kernel window whose upper 16 MiB is mapped only in the higher half (`LIMITLESS_BOOT_KERNEL_WINDOW_BYTES` in `boot_info.h`), and the back buffer lives there. The low alias, the boot-media stage area, and the low-window budget are unchanged.
- **Terminal scroll wiped the window.** Scrollback trimmed bytes from the end of the history, so on a short history each wheel notch erased the newest lines. Scrolling is now by whole lines (3 per notch) and stops once the first line of the history reaches the top; wrapped lines count by the rows they occupy.
- **Scroll cost.** The console scroll marked the dirty region once per pixel (about half a million calls per scrolled line); it now marks the shifted region once.
- **Windows under the taskbar.** Dragging could push a window under the taskbar and status strip; windows now stop above the taskbar.

Accepted verification (2026-09-27): captures after the change show a clean cursor on the first desktop frame, fully drawn windows throughout a title-bar drag, no cursor remnants while scrolling and moving the mouse, and scrollback that reveals the start of `help` output and returns to the latest output. Build had no warnings; BIOS 923/1024 sectors and low-window reserve 545,216 B are unchanged. The loader allocated the 32 MiB window (`allocation-pages 8192`). `verify-qemu.ps1 -BootMedia uefi -HardwareDisplayGate`, `verify-qemu.ps1 -BootMedia disk`, `verify-boot-media-linux-handoff.ps1`, and the Enter-alone first-run test all passed. Not proven: physical MSI display behavior.

### M198: First-run account choice and real login/lock

The first-run screen now offers an explicit choice: type a username (Enter) and then a password to create an account, or press Enter on the empty username to use the default account `limitless` / `limitless`. The default is picked automatically only after 60 seconds with no keyboard input at all, which keeps the M108 protection for machines whose keyboard driver is not working yet.

Fixed along the way (all found by exercising the real typed paths for the first time):

- **Login bypass:** every login and first-run read timed out after 10 ticks (100 ms) and fell back to a "hardware recovery session", so in practice nobody could type credentials and any machine signed in on its own. A created account now always waits for its password; only the default account (public password) signs in after 5 idle seconds.
- **Account swap:** after first-run creation, the session was restarted as the default account, so the user's own password did not unlock. The session now keeps the created account.
- **Lock did not lock:** `lock` timed out, printed "lock unavailable on this boot path", and returned to the shell. It now stays locked until the correct password is entered, with the existing failure delay and lockout.
- **Keystrokes swallowed by desktop windows:** with the desktop active, keys typed at the login or lock screen were consumed as shortcuts by the focused File Manager or Settings window. `display64_wm_process_keyboard_event` now leaves the keyboard alone while the auth screen is reading (`auth64_keyboard_capture_active`).
- **Keystrokes dropped by the PS/2 mouse IRQ:** the IRQ12 drain in `input.c` consumed every pending controller byte and discarded it when a native (USB/I2C) pointer was active, so keyboard scancodes queued at that moment were lost mid-word. Bytes without the AUX tag now go to the keyboard in that case; the controller quirk handling is unchanged when the PS/2 mouse is the pointer. This likely affects the MSI laptop (PS/2 keyboard with an I2C touchpad).
- **GUI probe window length:** `collect_gui_interactive_probe_input` ended on a raw spin count, so its duration depended on loop speed; with the login no longer leaving typed bytes in the queue it closed before any GUI input arrived. The guard now counts only spins without timer progress.
- **Stale verifier patterns:** two more assertions predated later telemetry fields (M161 NVMe registers in the UEFI NVMe identify line; `input-diag-suppressed`/`mouse-diag-suppressed` in the GUI line).

The login screen shows the two first-run options and the real account name on the login/lock screens.

Accepted verification (2026-09-27):

- Product build: no warnings; BIOS 923/1024 sectors (unchanged).
- CI gates passed: `verify-qemu.ps1 -BootMedia uefi -HardwareDisplayGate` and `verify-qemu.ps1 -BootMedia disk`. The UEFI gate now creates the account through the typed path (`first-run account creation`, `hardware-fallbacks 0`).
- Enter-alone first run: `first-run default account chosen`, account stored on NVMe (`user-store-persistent 1`).
- No-input first run: `first-run no keyboard input; default account selected` after the kernel measured 6000 ticks (60 s at 100 Hz; QEMU's emulated clock ran faster than wall time).
- Lock: the plain UEFI gate typed `lock` then the password; the transcript shows `session unlocked` on the first attempt and the next commands running normally.

Not proven: the plain UEFI gate (without `-HardwareDisplayGate`) still fails at its GUI assertion because the verifier's File Manager clicks produce no actions (`fileman-actions 0`). The same failure occurs on `main` before this change; see the roadmap. Physical MSI behavior is untested.

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
- **Desktop clock:** date and time from the CMOS RTC (UTC) in the status bar and taskbar.
- **Shell builtins (UEFI):** `apps cd clear date devices dev echo hwdevices lsdev export exporthw help hwfull hwval hwexport info linux lock net open pkginfo port ports pwd reboot shutdown uptime usbscan whoami`. BIOS fallback: `apps help hwval info linux net pkginfo pwd`, with `linux` and `lock` reporting unavailable.
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
