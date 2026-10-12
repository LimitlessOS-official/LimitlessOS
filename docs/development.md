# Development Guide

## Host requirements

LimitlessOS builds on Windows with PowerShell 7 (`pwsh`) or Windows PowerShell 5.1.

| Tool | Used for | Install |
|---|---|---|
| mingw-w64 gcc + binutils (`gcc`, `ld`, `objcopy`, `nm`, `size`) | Compiling and linking both kernels and `BOOTX64.EFI` (PE/COFF) | `winget install --id MSYS2.MSYS2 -e`, then `C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-nasm"` |
| `nasm` | Boot sectors, entry/interrupt stubs, ring-3 images | same pacman command |
| Python 3 + `cryptography` | Ed25519 signing of the package store at build time | `python -m pip install --user cryptography` |
| QEMU with `share\edk2-x86_64-code.fd` | All `verify-*` QEMU gates (OVMF UEFI firmware ships with QEMU) | `winget install --id SoftwareFreedomConservancy.QEMU -e` |
| IMAPI2 (built into Windows) | ISO generation | none |

You do not need to edit PATH. `tools\toolchain.ps1` is dot-sourced by `build.ps1` and `verify-qemu.ps1`. It adds `C:\msys64\mingw64\bin` (or `$env:LIMITLESS_MINGW_BIN`) and `C:\Program Files\qemu` for the current process when the tools are not already on PATH, and fails with install instructions if something is missing.

Verified toolchain (2026-09-27): gcc 16.2.0 (MSYS2 Rev4), GNU ld 2.47, nasm 3.02, QEMU from winget, Python 3.13.

## Building

```powershell
# Product: the serious bootable slice (BIOS fallback kernel + UEFI kernel, ISO, USB image, NVMe test disk)
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product

# Experimental: enables extra proof-only runtime surfaces; never Product behavior
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental

# Stage a Linux app + interpreter onto the UEFI boot media (used for hardware handoff bundles)
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product `
    -BootLinuxAppPath external\build\DYNLDLIMIT -BootLinuxInterpPath external\build\LDLIMIT

# The original 32-bit BIOS lane
.\tools\build.ps1 -Architecture x86
```

A Product build ends by running `tools\assert-m1-production-slice.ps1`, which checks artifact inventory, ISO contents, shell surface text, absence of private-key material, and the wording rule below. Every build prints the budget summary described in [budgets.md](budgets.md).

## Running the desktop locally

```powershell
.\tools\run-desktop.ps1            # build if sources changed, then boot in a QEMU window
.\tools\run-desktop.ps1 -Fresh     # start over with a new virtual disk (new first-run setup)
.\tools\install-desktop-shortcut.ps1   # one-time: adds a "LimitlessOS" icon to the desktop
```

`run-desktop.ps1` rebuilds automatically when any file under `boot/`, `kernel/`, `apps/`, `packages/`, or `tools/` is newer than the last build. It then boots the UEFI USB image with the same virtual hardware as the UEFI gate (q35, OVMF, xHCI keyboard/mouse, NVMe, virtio-net).

- **Persistence:** the virtual NVMe disk lives in `%LOCALAPPDATA%\LimitlessOS\desktop-nvme.img`, outside the repository and OneDrive, so the local account and files persist between runs. The kernel log is written next to it as `desktop-debug.log`.
- **Mouse:** click inside the window to capture the mouse; Ctrl+Alt+G releases it.
- **First run:** type a username and press Enter, then a password, to create your account; or press Enter on the empty username to use the default account (`limitless` / `limitless`). If there is no keyboard input at all for 60 seconds, the default account is chosen. `-Fresh` brings the first-run screen back.
- **Login and lock:** a created account always requires its password. The default account signs in by itself after 5 idle seconds at the login screen. `lock` (or the lock button in Settings) stays locked until the account password is entered.
- **One VM at a time:** the launcher refuses to start a second VM while one is running, because both would write the same disk.
- **Limits:** QEMU is a development convenience. It does not stand in for physical hardware; touchpad, Intel VMD NVMe, and display-panel behavior on the MSI laptop still need a USB capture.

`run-qemu.ps1` remains for booting the other lanes and media (`-Architecture x86`, `-BootMedia disk` or `iso`) with a throwaway NVMe snapshot.

## Verifying

The main gate, run after any kernel, loader, or tooling change:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

It boots the built image under QEMU/OVMF, drives the login and shell through QMP key injection, and asserts hundreds of serial telemetry lines. One run takes about 2.5 minutes. Other commonly used gates:

| Command | Proves |
|---|---|
| `verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product` | Desktop GUI probe (windows, File Manager, Settings), first-run login, and `lock`/unlock |
| `verify-qemu.ps1 -Architecture x86_64 -BootMedia disk -BuildProfile Product` | BIOS fallback lane |
| `verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product` | UEFI optical media |
| `verify-qemu.ps1 ... -BootMedia uefi -RealBinaryGate -ExtraShellLine "linux /APPS/DYNLDLIMIT"` | A staged dynamic Linux ELF runs from boot media |
| `verify-real-binary-gate.ps1 -BusyBoxPath external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh` | BusyBox/sbase real-binary gate with provenance |
| `verify-boot-media-linux-handoff.ps1` | Loader staging and kernel boot-media fallback |
| `verify-nvme-persistence.ps1` | Two-boot NVMe persistence with scoped write/commit authority |
| `verify-installer-m5.ps1` | Installer dry-run and partition protection fixtures |
| `verify-*-fixtures.ps1` | Host-side hardware capture analyzers (no QEMU) |

`tools\README.md` lists every script by purpose.

### Continuous integration

`.github/workflows/verify.yml` runs on every pull request and every push to `main`, on a `windows-latest` GitHub runner. It installs the same toolchain, runs the Product build (which includes the M1 production-slice gate and budget enforcement), then runs three QEMU gates: UEFI desktop/login/lock (no switches), UEFI `-HardwareDisplayGate`, and BIOS disk. Serial logs and the size report are uploaded as the `verify-logs` artifact. The real-binary, persistence, and hardware-handoff gates depend on the ignored `external/` inputs or physical hardware and still run locally.

## External inputs (`external/`, not in git)

The real-binary gates use binaries built outside this repository, kept under the ignored `external/` directory:

- `external/src/`: upstream source tarballs (BusyBox 1.35.0, sbase 0.1, toybox 0.8.13).
- `external/tools/`: cross toolchains (`gcc-linux-musl-x86_64`, `zig-x86_64-windows-0.15.2`, a portable MSYS2 used for `make`).
- `external/build/`: the built test binaries (`busybox-*`, `sbase-*`, `DYN*`, `LDLIMIT`, and others).

`tools\build-real-busybox-standalone-sh.ps1` rebuilds the standalone-shell BusyBox. Sources for the project-authored Linux test programs live in [`fixtures/linux/`](../fixtures/README.md). Provenance and SHA-256 values for every accepted binary are recorded in [real-binary-gate.md](real-binary-gate.md) and `docs/history/real-binary-gate-log.md`.

## Rules for changes

- **Truthful surfaces.** Shell, GUI, and telemetry must report real detected state or say `unavailable`, `denied`, or `planned`. Never present a scripted result as working behavior. See [real-binary-gate.md](real-binary-gate.md).
- **Wording gate.** `Assert-NoUnlabeledPlaceholders` in `assert-m1-production-slice.ps1` fails the build if README, `docs/`, `kernel/`, `packages/`, or `tools/` contain unfinished-work markers or words that label something as a mock-up or prototype (the exact regex is in that function). Track open work in [roadmap.md](roadmap.md) instead of leaving markers in files.
- **BIOS lane is frozen.** New code goes to the UEFI kernel. If a shared file grows, check `bios-sector-reserve` did not drop.
- **Budgets are part of review.** Report the four budget values from the build summary in the milestone entry.
- **No compiler warnings** on either lane (`-Wall -Wextra`).
- **Loader/kernel contract lives in `kernel/include/boot_info.h`.** Change both sides together.

## Milestone workflow

Work is numbered `M<n>`; the latest is recorded at the top of [status.md](status.md).

1. Pick the next item from [roadmap.md](roadmap.md) and write it at the top of `status.md` under *Current milestone*: scope, root cause or motivation, what changed, and what is explicitly not proven.
2. Build, run the relevant gates, and record the exact commands and budget values under *Accepted verification*.
3. Commit as `M<n> <short imperative summary>`.
4. When a milestone is superseded, move its narrative into `docs/history/` and keep `status.md` current.
