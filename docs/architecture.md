# LimitlessOS Architecture

This document has two halves: the **design intent** that every change is measured against, and the **as-built** system as of M194. Where they differ, the gap is listed in the roadmap's structural work. The detailed bring-up narrative for the early x86 and x86_64 lanes is archived in [history/architecture-bring-up.md](history/architecture-bring-up.md).

## Design intent

### Goals

Five non-negotiables:

- security first
- fast and stable on both old and modern hardware
- lightweight by default
- native multi-ecosystem software execution as a first-class OS design goal
- transparent AI boundaries

### Product experience direction

LimitlessOS should have its own recognizable identity, drawing on four reference qualities:

- Red Hat-style Linux seriousness: professional, grounded, and credible for workstations, admins, and developers
- Windows-like simplicity: discoverable defaults, predictable app launching, clear setup paths, plain-language choices
- macOS-like cleanliness: calm visual hierarchy, polished spacing, restrained animation, consistent typography
- Linux/Unix power: transparent system state, strong terminal and scripting paths, composable tools, readable logs

These are reference qualities, not permission to clone another system's trade dress or interaction model. Polish must stay lightweight: no mandatory GPU, heavy always-on effects, large background services, or showcase panels. Hardware or system information comes from real detected state or explicitly says unavailable.

### Hybrid kernel model

Neither a pure microkernel nor an everything-in-kernel monolith.

**In the small trusted core** (these define isolation and correctness): thread scheduling and dispatch, virtual address spaces, physical memory ownership, interrupt routing and traps, capability-based IPC, syscall dispatch, secure service launch and policy enforcement, timekeeping.

**In kernel only when the fast path justifies the TCB cost:** page and slab allocator fast paths, VFS and pathname caches, block I/O scheduling, the network packet fast path, cryptographic primitives for storage and verified boot, power-management coordination.

**In user space:** most device drivers, filesystems that tolerate user-space latency, the graphics compositor, the package manager, installer UI, settings panels, the AI policy broker, and search/indexing/assistant tools.

The rationale is to keep isolation primitives small, keep performance-critical paths narrow and explicit, and move failure-prone logic into restartable services.

### Scalability profile

- **Older and low-end hardware:** minimal install image, no mandatory TPM 2.0, software rendering fallback, modular background services, aggressive memory budgeting, optional cloud features instead of always-on local AI.
- **Higher-end systems:** multicore scheduling classes, GPU acceleration where available, isolated high-performance I/O services, richer local caching, optional secure enclaves and attestation.

One common service, package, installer, and policy model spans all targets, with architecture-specific loader and kernel back ends: a legacy 32-bit minimal image, a 64-bit standard image, and a 64-bit image with optional 32-bit compatibility libraries.

### Native multi-ecosystem execution

The target is that LimitlessOS recognizes and executes major application and script formats (PE/COFF executables and scripts, ELF binaries and shell scripts, desktop bundle and package formats) as ordinarily as each ecosystem's own OS does, based on real parsed metadata and documented ABI behavior.

The mechanism is an **OS persona** layer above the hybrid kernel. A persona is a native LimitlessOS execution environment, not a VM, emulator, container, or Wine-like patchwork. It provides one ecosystem's ABI surface, process conventions, filesystem expectations, permission model, windowing/input expectations, IPC/service mappings, and package metadata interpretation. It still uses the LimitlessOS scheduler, memory manager, capability policy, brokered services, and audit model.

Persona requirements:

- foreign permissions and APIs map to explicit LimitlessOS capabilities
- no ambient filesystem, network, input, display, package, firmware, installer, identity, secret, or AI authority
- executable loading validates real headers, signatures, entitlements, manifests, interpreter paths, or package metadata where those exist
- unsupported APIs fail truthfully and audibly
- ecosystem services are restartable and isolated unless they need ring 0
- kernel fast paths only when the ABI cannot be served safely or efficiently from a brokered service
- personas are modular and demand-loaded; unused ecosystems cost nothing in the base install

### Package management

One native package broker owns system package transactions. Familiar `apt`, `dnf`/`yum`, `apk`, and `choco` workflows are optional compatibility frontends that translate into the broker's capability-checked transaction model, with per-frontend trust settings, never separate privileged package roots. The broker contract is shared across 32-bit and 64-bit targets.

### Boot and trust chain

Long-term: UEFI Secure Boot, measured boot, signed kernel and service manifests, and rollback protection for critical components.

### AI placement

The assistant is a sealed platform service, not a user-editable app: a signed UI shell, a policy broker that mediates every privileged action, cloud-backed reasoning for heavier tasks, and an offline fallback that degrades safely instead of pretending to have cloud capability. It gets no blanket authority: it must ask, explain, and log.

## As built (M194)

### Lanes

| Lane | Boot | Kernel | Role |
|---|---|---|---|
| x86 (32-bit) | `boot/boot.asm` BIOS | `kernel/core/*`, `kernel/arch/x86/*` | The original Phase 0/1 bring-up: IDT/PIC/PIT, paging, frame allocator, IPC endpoints with capability handles and delegation, cooperative and preemptive ring-3 tasks. Still builds; not the product. |
| x86_64 BIOS fallback | `boot/boot64.asm` loads `KERNEL64-BIOS.BIN` | `kernel/arch/x86_64/*` minus UEFI-only sources | Frozen, checksum-only fallback under a 1024-sector budget. |
| x86_64 UEFI Product | `BOOTX64.EFI` (`uefi_app.c`) loads `KERNEL64.BIN` | full `kernel/arch/x86_64/*` plus Ed25519 and bcrypt | The product. |

The two x86_64 kernels come from the same sources; `LIMITLESS_X64_UEFI_KERNEL` and `LIMITLESS_X64_BIOS_KERNEL` select features, and `tools/build.ps1` excludes persona, Linux, networking, signing, identity, cloud, installer, and AI sources from the BIOS link.

### Boot flow (UEFI)

1. `BOOTX64.EFI` finds GOP, reads `BOOTMAN.TXT`, loads `KERNEL64.BIN` into a 2 MiB buffer, and checks its byte count and `fnv1a-32` checksum (the manifest's `kernel-sha256` is for external verification).
2. It places the kernel: at the linked physical `0x10000` if the whole 32 MiB kernel window is free, otherwise in a 2 MiB-aligned fallback window (the normal case on OVMF and real firmware).
3. It stages optional boot-media files (`/APPS/DYNLDLIMIT`, `/APPS/LDLIMIT`) into the stage area at the top of that window, discovers ACPI (RSDP/XSDT/MCFG/MADT/FADT/DSDT/SSDTs), and builds low handoff page tables. The first 64 KiB is identity-mapped for tables, boot-info, and trampoline; the rest of the low 16 MiB maps onto the kernel window, with a higher-half alias at `0xFFFFFFFF80000000`.
4. It takes the final memory map, calls `ExitBootServices`, and jumps to `_start` (`entry.asm`), which clears `.bss` and calls `kernel_main64_scaffold()`.
5. The kernel initializes GDT/TSS, IDT, APIC (or the PIC fallback), PIT, and syscalls; runs controlled fault and ring-3 probes; brings up xHCI, PCI/ECAM storage, the framebuffer, I2C HID, and input; runs the login gate; starts the desktop, network, and services; and enters the persistent ring-3 shell.

### Memory layout

| Region | Use |
|---|---|
| `0x0`–`0xFFFF` (identity) | Handoff page tables (`0x1000`), boot-info (`0x9000`), trampoline (`0xA000`), framebuffer PD (`0xB000`) |
| `0x10000`–`__kernel_end` | Kernel `.text`, `.rodata`, `.data`, then `.bss` (starting no lower than `0x100000`; about 14.5 MB of static pools and buffers) |
| `0xFC0000`–`0x1000000` | Boot-media stage area (contract in `kernel/include/boot_info.h`) |
| `0xFFFFFFFF81000000`–`0xFFFFFFFF82000000` | Kernel window extension, higher half only: compositor back buffer (M199) |
| `0x40020000` / `0x41000000` | Ring-3 shell stack top / user image base |
| `0x52000000` | Link base for Linux persona test binaries |
| `0xFFFFFFFF80000000+` | Higher-half kernel alias; MMIO windows are mapped above it on demand |

There is no general physical-frame allocator on x86_64 yet. Process page-table roots (8), persona contexts (32), pipes (16), and similar tables are fixed pools in `.bss`. The low-window budget is enforced by the build; see [budgets.md](budgets.md).

### Kernel organization

- `scaffold.c` is a unity build that includes the `scaffold_*.c` fragments in a fixed order under `LIMITLESS_SCAFFOLD_*` section macros. It holds the boot sequence, the proof probes, and the `log_*_surface()` telemetry emitters.
- Subsystems have their own files: `paging.c`, `scheduler.c`, `process.c`, `launch.c`, `capability.c`, `principal.c`, `services.c`, `syscall.c`, `interrupts.c`, `apic.c`, `pci.c`, `xhci.c`, `i2c_hid.c`, `input.c`, `display.c` (compositor, window manager, GUI apps), `shell.c`, `fs.c`, `fd.c`, `vma.c`, `pipe.c`, `mmio.c` (AHCI/NVMe/VMD brokered MMIO), `virtio_net.c`, `e1000e.c`, `network_socket.c`, `auth.c`, `package_signing.c`, and the persona families `linux_*.c`, `elf64.c`, `windows_*.c`, `pe64.c`, `macos_*.c`, `macho64.c`, `persona*.c`.
- The native syscall ABI (`syscall_x64.h`) has 3,476 numbered calls, reachable through `int 0x80` and the `syscall` instruction; most are read-only telemetry getters consumed by the shell and the verifiers.

### Privilege split today

Everything listed above runs in ring 0, including drivers, the compositor and window manager, shell command execution, and the persona ABIs. Ring 3 holds the persistent shell loop (`runtime_image_user.asm`, which reads keys and submits each line through `SYSCALL_SHELL_EXECUTE_LINE`), the flat utility binaries and native `.APP` programs, and all Linux, Windows, and macOS persona processes.

"Services" are kernel records (principal, endpoint class, scheduler class, capability budget, manifest, and launch token) with capability-checked broker entry points, not separate address spaces. Moving them out of ring 0 is the main gap between the design and the build (see [roadmap.md](roadmap.md)).

### Capability model

`capability.c` implements principal-scoped service handles: grant, delegation with attenuated rights and short leases, second-hop delegation denial, routing with owner checks, revocation with child cascade, endpoint-class drain, runtime-generation tokens that invalidate handles across service restarts, and persona tags. Every broker entry point (console, input, display, RAMFS/FAT/NVMe filesystem, block, network, hardware inventory, installer, AI policy) routes through it, and denials are counted and visible in telemetry.

### Personas

| Persona | Code | Status |
|---|---|---|
| Linux ELF | `linux_exec.c`, `linux_abi.c` (512-entry dispatch table), `linux_vfs.c`, `linux_dynamic.c`, `linux_libc.c`, `linux_vdso.c`, `elf64.c` | Runs third-party static and dynamic musl binaries with fork/exec/wait, pipes, threads, futex, signals, mmap, and VFS over NVMe FAT and boot media |
| Windows PE | `pe64.c`, `windows_abi.c` (NT syscall table), `windows_handle.c`, `windows_vfs.c`, `windows_registry.c`, `windows_seh.c`, `windows_shim.c` | Loader plus about 20 NT syscalls; exercised only by repo-built PEs |
| macOS Mach-O | `macho64.c`, `macos_abi.c`, `macos_mach.c`, `macos_dyld.c`, `macos_cf.c`, `macos_shim.c` | Parsing and ABI groundwork |

`persona.c` holds per-process persona contexts (dispatch table, VMA root, FD table, TLS, brk, audit context, capability attenuation mask) and `persona_audit.c` records per-persona syscall audit.

### Verification model

`tools/verify-qemu.ps1` boots each medium under QEMU/OVMF, injects keyboard and mouse input through QMP, and asserts serial telemetry lines. That is why most kernel features emit structured `drs-*` and `[x64] ...` lines. Physical hardware is validated through the `hwval` command and the capture/analysis tooling in `tools/`.
