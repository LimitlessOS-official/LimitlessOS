# LimitlessOS Roadmap

Forward-looking only. What is done is in [status.md](status.md); the full acceptance history (M1–M192) is archived in [history/roadmap-log.md](history/roadmap-log.md) and [history/status-log.md](history/status-log.md).

## North star: native multi-ecosystem execution

LimitlessOS exists in part to break the lock between software and a single operating-system ecosystem. Software from major ecosystems should run as first-class native citizens on one secure, lightweight platform, designed into the kernel, service, package, and security model rather than bolted on.

- **Direction:** OS personas above the hybrid kernel. Each persona parses real executable and package formats and maps its ecosystem's ABI, filesystem expectations, process model, permissions, IPC, display/input, and package conventions onto LimitlessOS services and capabilities.
- **Constraints:** personas are capability-scoped, auditable, optional, and demand-loaded, so the base install stays light enough for older machines. Unsupported APIs fail truthfully.
- **Non-goals:** virtual machines, emulation, or containers as the compatibility strategy; a Wine-style translation layer as the foundation; any "compatible" output without real loading and ABI behavior.

Progress: the Linux persona runs real third-party static and dynamic musl binaries. The Windows (PE) and macOS (Mach-O) personas have loader and ABI groundwork only.

## Experience direction

Distinctive, not a clone: Red Hat-style seriousness, Windows-like discoverability, macOS-like calm and polish, and Unix-level transparency and power. Recommended defaults for new users, visible detail and control for experienced users, and no invented status panels.

## Next milestones

Ordered by what unblocks the most. Each follows [development.md](development.md#milestone-workflow) and the evidence rules in [real-binary-gate.md](real-binary-gate.md).

1. **MSI retest of the M193 image.** Rebuild the handoff bundle (`prepare-hardware-storage-evidence.ps1`) with staged `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT`, boot it on the MSI Cyborg 15 A13VE, and capture `hwval` plus `linux /APPS/DYNLDLIMIT`. Expected first physical run of a dynamic Linux ELF from boot media.
2. **Pointer input on hardware.** Get the ELAN I2C HID touchpad (ACPI `_CRS` path) or the composite USB boot mouse (xHCI) moving the cursor on the laptop; the diagnostics from M149–M190 already localize the stage.
3. **Internal NVMe behind Intel VMD.** Move from the no-touch VMD driver plan (M158–M160) to a real bind of the nested NVMe controller, read-only first, so `/nvme` works on the laptop without the boot-media fallback.
4. **BIOS reserve recovery.** Split the AHCI planning chain in `mmio.c` so the BIOS lane compiles only what a BIOS/IDE boot reaches, bringing the reserve back above 128 sectors without changing the BIOS verifier's observable telemetry. See [budgets.md](budgets.md).
5. **Reproducible fixtures.** Recover or rewrite the `LDLIMIT` interpreter source, and add a script that rebuilds every `fixtures/linux/` program into `external/build/` with recorded command lines and SHA-256 values.
6. **Physical memory allocator.** Replace the static pools (8 process roots, fixed pipe and persona tables) with a frame allocator sized from the firmware memory map, keeping the low-window contract in `boot_info.h`.
7. **Wired Ethernet on physical hardware**, before Wi-Fi.
8. **Plain UEFI gate.** `verify-qemu.ps1 -BootMedia uefi` without `-HardwareDisplayGate` fails on `main` because the GUI probe's File Manager clicks produce no actions (`fileman-actions 0`). Fix the probe or the File Manager hit-testing so the gate that exercises `lock` can join CI.

## Structural work

These make the code match the architecture it describes:

- **Move services out of ring 0.** Drivers, the compositor, the window manager, the shell, and persona services all run in the kernel today. The hybrid model in [architecture.md](architecture.md) keeps only isolation primitives and justified fast paths in ring 0; start with services that already have brokered, capability-checked interfaces.
- **Consolidate the diagnostic syscall surface.** The native ABI has about 3,476 numbered syscalls, most of them single-value telemetry getters. A structured query interface would shrink the kernel and the verifier coupling.
- **Break up the scaffold unity build.** `scaffold_core.c` (about 41K lines) mixes boot sequencing, proof probes, and logging.

## Later tracks

Unordered. Each needs the platform work above first.

- **Functional AI Assistant:** a real inference backend or remote-attested model path; bounded context; explicit per-action consent; audit; never present scripted output as inference.
- **Encrypted vault:** encrypted-at-rest secrets and tokens scoped per session and principal; no plaintext on the physical image.
- **Bare-metal expansion:** broaden `hwval` and validation beyond the MSI laptop across UEFI, xHCI, NVMe, AHCI, framebuffer, input, and network variants.
- **SDK expansion:** external C/assembly apps build into the `.BIN`/`.APP` format with declared capabilities, without kernel edits.
- **Enterprise identity and policy:** managed enrollment and remote-auth integration points, keeping the no-token-without-vault and no-plaintext-credential rules.
- **Package broker:** a native broker owning transactions, with optional `apt`/`dnf`/`apk`/`choco`-style frontends as capability-checked adapters rather than privileged package roots.
- **Trust chain:** UEFI Secure Boot, measured boot, signed kernel and service manifests, and rollback protection.
- **Installer:** real, explicitly approved writes to a LimitlessOS target, boot-entry creation, and 32-bit versus 64-bit image recommendations.
- **Windows and macOS personas:** real third-party PE and Mach-O binaries through the same evidence gate the Linux persona passed.
- **Browser:** last, per the daily-driver order in [real-binary-gate.md](real-binary-gate.md).
