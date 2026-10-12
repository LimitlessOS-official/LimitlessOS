# LimitlessOS

LimitlessOS is a clean-room x86 operating system built around five non-negotiables: security first, fast and stable on old and new hardware, lightweight by default, native multi-ecosystem software execution, and transparent AI boundaries.

Its long-term purpose is to run software from several major OS ecosystems as first-class native citizens through **OS personas** layered on a capability-based hybrid kernel, not through virtual machines, emulation, containers, or a Wine-style translation layer. The desktop aims to combine Red Hat-style seriousness, Windows-like discoverability, macOS-like polish, and Unix-level transparency without copying any of them.

## Where it stands

Current milestone: **M194** (see [docs/status.md](docs/status.md) for the full picture).

| Area | State |
|---|---|
| Boot | UEFI USB/ISO boot to an authenticated desktop and ring-3 shell (QEMU/OVMF, and the MSI Cyborg 15 A13VE laptop). BIOS boot remains as a lean, checksum-only fallback. |
| Kernel | x86_64 long mode, higher-half kernel, APIC/PIT, per-process page tables, preemptive scheduler, capability-scoped syscalls and brokered services. |
| Desktop | Compositor, window manager, login/lock, Terminal, File Manager, Settings, Installer (dry-run), Assistant (policy/consent only, no model). |
| Linux persona | Runs unmodified static musl BusyBox and suckless sbase utilities plus dynamically linked musl programs: fork/wait, pipes, threads, futex, mmap, signals, VFS. |
| Windows / macOS personas | Loader and ABI groundwork only (PE and Mach-O parsing, a small NT syscall set). Not Product behavior. |
| Storage / network | NVMe FAT32 read/write with reboot persistence; DHCP/DNS/TCP/HTTP over virtio-net and e1000e behind a brokered socket API. |
| Security | Principal-scoped capabilities with attenuated delegation and cascading revoke, Ed25519-signed packages, bcrypt login, no ambient authority. |

Honesty rule: every surface reports real detected state or says `unavailable`. See [docs/real-binary-gate.md](docs/real-binary-gate.md) for what counts as evidence.

## Quick start (Windows host)

One-time toolchain setup (details in [docs/development.md](docs/development.md)):

```powershell
winget install --id MSYS2.MSYS2 -e
C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-nasm"
winget install --id SoftwareFreedomConservancy.QEMU -e
python -m pip install --user cryptography
```

Build and verify the Product image:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

Run the desktop in a window, rebuilding first whenever sources changed:

```powershell
.\tools\run-desktop.ps1                 # or create a desktop icon once:
.\tools\install-desktop-shortcut.ps1
```

The build prints every budget (BIOS sectors, UEFI kernel bytes, low-window reserve, FAT image headroom) and fails if a hard limit is crossed. See [docs/budgets.md](docs/budgets.md).

Outputs land in `dist/`: `limitlessos-x86_64.iso` (UEFI ISO), `limitlessos-x86_64-uefi.img` (USB image), `limitlessos-x86_64.img` (BIOS disk image), and `limitlessos-x86_64-nvme-gpt.img` (test NVMe disk).

## Repository layout

| Path | Contents |
|---|---|
| `boot/` | BIOS boot sectors (`boot.asm` for the 32-bit lane, `boot64.asm` for the x86_64 BIOS fallback). |
| `kernel/arch/x86_64/` | The x86_64 kernel, UEFI loader (`uefi_app.c`), drivers, compositor, shell, and personas. `scaffold.c` is a unity build of the `scaffold_*.c` fragments that hold the boot sequence. |
| `kernel/arch/x86/`, `kernel/core/` | The original 32-bit BIOS lane (`kernel/core/ramfs.c` is shared with x86_64). |
| `kernel/include/` | Headers, including the loader/kernel contract in `boot_info.h`. |
| `apps/native/` | Manifests for native LimitlessOS `.APP` packages. |
| `packages/` | Bootstrap package store specification (signed at build time). |
| `fixtures/` | Sources for test programs: `linux/` (Linux persona programs built out-of-tree), `embedded/` (programs embedded as byte arrays in the kernel). |
| `third_party/` | Ed25519 reference verifier and crypt_blowfish (bcrypt). |
| `tools/` | Build, image generation, QEMU verification, hardware capture, and installer tooling. See [tools/README.md](tools/README.md). |
| `docs/` | Design, status, roadmap, budgets, and development guides. |
| `external/` (ignored) | Third-party sources, cross toolchains, and externally built test binaries. |
| `build/`, `dist/` (ignored) | Intermediates and artifacts. |

## Documentation

- [docs/status.md](docs/status.md): what works today, current budgets, known defects.
- [docs/roadmap.md](docs/roadmap.md): where the project is going and the next milestones.
- [docs/architecture.md](docs/architecture.md): design intent and the as-built system.
- [docs/budgets.md](docs/budgets.md): every size/memory budget, how it is measured and enforced.
- [docs/development.md](docs/development.md): toolchain, build, verification, and contribution rules.
- [docs/real-binary-gate.md](docs/real-binary-gate.md): evidence rules for execution and hardware claims.
- [docs/security-model.md](docs/security-model.md), [docs/package-format.md](docs/package-format.md), [docs/installer.md](docs/installer.md): subsystem design.
- [docs/subsystems/](docs/subsystems/): per-subsystem specs (identity, cloud, AI policy, sockets, SDK, and more).
- [docs/hardware/](docs/hardware/): physical validation checklists (MSI Cyborg 15 A13VE, VirtualBox).
- [docs/history/](docs/history/): archived milestone narratives (M1–M192).
