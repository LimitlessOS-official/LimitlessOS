# LimitlessOS Architecture

## Goals

LimitlessOS is designed around four non-negotiables:

- security first
- fast and stable on both old and modern hardware
- lightweight by default
- transparent AI boundaries

## Hybrid kernel model

LimitlessOS uses a hybrid kernel, not a pure microkernel and not a traditional everything-in-kernel monolith.

### Microkernel responsibilities

These stay inside the smallest trusted kernel core because they define isolation and system correctness:

- thread scheduling and core task dispatch
- virtual address space management
- physical memory ownership tracking
- interrupt routing and trap handling
- capability-based IPC
- syscall dispatch
- secure service launch and policy enforcement
- timekeeping primitives

### Monolithic in-kernel responsibilities

These remain in kernel space only when the latency or throughput win is worth the trusted-computing-base cost:

- page allocator fast paths
- slab and object allocators
- virtual filesystem cache and pathname cache
- block I/O scheduler
- network packet fast path
- cryptographic primitives needed for storage and verified boot
- power management coordination

### User-space services

Everything that does not need ring 0 privileges lives outside the kernel:

- most device drivers
- filesystems that can tolerate user-space latency
- graphics compositor
- package manager
- installer UI
- settings panels
- AI policy broker
- search, indexing, and assistant-facing research tools

## Why this split exists

Pure microkernels can become slower or more complex when every hot path crosses protection domains. Fully monolithic kernels increase blast radius when one subsystem fails. LimitlessOS takes a middle path:

- keep the isolation primitives small
- keep performance-critical paths narrow and explicit
- move failure-prone logic to restartable services

## Scalability profile

### Low-end and older hardware

- minimal install image
- no mandatory TPM 2.0 class requirement for core operation
- software rendering fallback
- modular background services
- aggressive memory budgeting
- optional cloud features instead of always-on local AI workloads

### Higher-end systems

- multicore scheduling classes
- GPU acceleration where available
- isolated high-performance I/O services
- richer local caching for AI and developer workflows
- optional secure enclaves and attestation-backed services

## Architecture targets

The current bootstrap code in this repository is a 32-bit x86 BIOS bring-up because it is the shortest path to a real bootable kernel. The product target is wider:

- 32-bit x86 support for older and lower-spec systems
- 64-bit x86 support as the default path on modern hardware
- a shared capability, service, and userspace package model across both targets
- architecture-specific boot, trap, paging, and context-switch code behind that shared model

The repo now has the first explicit code split for that direction:

- `x86` remains the live BIOS boot image and QEMU verification target
- `x86` now also has a verified BIOS optical-media path generated through Windows IMAPI2, so the mature bootstrap lane can be packaged as a burnable ISO in addition to the raw disk image
- `x86_64` now has its own BIOS boot sector with a chunked 127-sector loader, kernel entry path, linker path, generated architecture header, shared boot-info handoff contract, a dedicated kernel-owned GDT/TSS setup with explicit kernel/user selectors and native `syscall` STAR selector proof, a dedicated x64 interrupt path, a minimal timer-IRQ proof path, higher-half kernel execution, controlled breakpoint, invalid-opcode, and page-fault proof telemetry, an interrupt-based scaffold syscall surface, a first native long-mode `syscall` entry, a shared x64 service namespace scaffold with queryable core service classes and capability masks, an active-principal registry with role queries, sealed bootstrap process records bound to principals, endpoints, scheduler classes, capability budgets, verified kernel-service manifests, init-authorized launch request lifecycle records, quiesce preflight, active-capability stop safeguards, and an audited protected-service stop denial path, a first principal-scoped service-capability handle lifecycle with grant, short-lease attenuated delegation, route, revoke with child cascade, unknown-principal denial, wrong-owner denial, expired-handle denial, stale-handle denial, second-hop delegation denial, and over-broad authority denial, a brokered RAMFS syscall bridge with owner-scoped node capabilities, a brokered console syscall bridge, and shared bootstrap package-archive v2 visibility on both the BIOS scaffold and UEFI app paths
- the current `x86_64` lane is still intentionally a small long-mode scaffold rather than a full clone of the x86 runtime, but it now boots, enables 4-level paging with a 16 MiB identity/high-half alias map, aliases that same early mapping into the higher-half kernel window and then transfers execution into that higher-half kernel entry so live code and data are actually running from `0xFFFFFFFF80010000+`, enables the SSE/FPU state expected by the x64 ABI, validates a shared x86/x64 boot-info handoff inside the 64-bit kernel entry, installs a measured long-mode GDT and TSS, exposes kernel selectors `0x18/0x20`, user selectors `0x33/0x2B`, loaded task register `0x38`, and a native `syscall` STAR selector value through direct, interrupt-syscall, and native-syscall telemetry, loads an x64 IDT with exception stubs plus proof and syscall vectors, remaps the PIC, programs the PIT, proves real `sti`/`hlt` timer wakeups, records controlled breakpoint, invalid-opcode, and page-fault exceptions without crashing so the last-fault path is visible in verification, answers early boot plus timer queries through a simple `int 0x80` ABI, now also proves an MSR-backed native `syscall` entry against that same query and fault-report surface instead of only reading local state directly, now exposes a small shared service namespace with queryable `ai-policy`, `console`, `ramfs`, and `input` endpoints plus capability masks over those x64 syscall paths, now exposes a principal table with active and role queries through both syscall paths, now validates kernel-service-authority manifests from the same generated package archive while ignoring user-app manifests, now exposes service process bindings that map sealed bootstrap PIDs to principals, endpoints, scheduler classes, capability budgets, package IDs, executable IDs, signer IDs, launch tokens, and operation-aware init-authorized request lifecycle records, now proves a quiesce preflight can pass only when the target service has zero live capabilities and is denied while active handles still point at that service, now proves protected sealed services cannot be stopped until a future quiesce/revoke teardown path exists, now proves service-capability handles can be principal-scoped, granted, attenuated into short-lease child handles, routed, revoked with child cascade, and denied after unknown-principal, wrong-owner, expired, stale, second-hop, or over-broad use through both x64 syscall paths, and now surfaces the shared generated bootstrap package-archive v2 summary in both the BIOS scaffold log and the UEFI app log
- the x86_64 launch broker now owns a first capability-drain and restart transition: privileged lifecycle callers can ask the broker to revoke live handles targeting one service endpoint class, the request records observed and revoked capability counts, unsafe quiesce remains denied while live handles exist, post-drain quiesce succeeds without exposing a general endpoint-revoke syscall to arbitrary userspace, restart is denied until the service is quiesce-ready, successful restart records a generation count, rekeys the service runtime token, recomputes a runtime image token from verified package payload offset/size/checksum metadata plus signer-backed manifest data, derives a sealed runtime image plan with base, entry, mapped bytes, read/execute/sealed/supervisor-validation rights, and a plan token, then installs real four-page mappings for a sealed 16 KiB persistent-shell bootstrap transfer image. It records page count, PML4/PDPT/PD indexes, map token, protection token, install token, source checksum, controlled entry-probe telemetry, and explicit proof that the validation view is not user-accessible or writable. The broker also installs a separate four-page read/execute user executable mapping at `0x41000000`, validates that it resolves to the same measured transfer bytes, maps a one-page user stack at `RSP 0x40020000`, and only then marks the measured ring-3 user-entry frame transfer-ready with denial `0`, `RIP 0x41000010`, selectors `0x33/0x2B`, and interrupt-masked `RFLAGS 0x00000002`. Verification now covers controlled ring-3 entry, IF-enabled PIT preemption capture, scheduler-owned frame switching, second-page RAMFS/display mutation at `0x41001ED0`, and the default post-scaffold persistent ring-3 shell; disk-sourced descriptors and flat binaries now carry utility command execution that used to live in the sealed image. The reusable x64 run-queue proof is bound to manifest-launched process records, and runtime-bound capability handles reject stale tokens after restart.
- the x86_64 scheduler proof now includes a bounded saved-frame run queue backed by a reusable `scheduler_x64` module: IRQ0 hands the interrupt frame to the scheduler, the scheduler saves PID 2 `ai-policy` task A as a complete interrupt frame, dispatches PID 4 `console` task B at `0x41000140` on `0x4001F800`, records B result `0x52514232`, restores task A from the saved frame, and completes with A result `0x52514131`. Scheduler registration can now accept a launched-process PID, resolve runtime/user-entry tokens plus selectors from `process_x64`, and reject processes whose brokered user-entry frame is not transfer-ready. The process syscall surface now exposes user-entry RIP/RSP/selectors/RFLAGS directly, and the new `fs_x64`, `console_x64`, and `input_x64` syscall bridges prove the 64-bit lane can receive raw command bytes plus broker-normalized line-edited commands, consume shared RAMFS objects, and print user buffers through service capabilities from sealed ring-3 probes before the full shell is ported. The newest x64 proof now preserves the original byte-stream input syscall, adds a line-oriented read syscall for shell command boundaries, loops over brokered input lines, dispatches `help`, `help ls`, `help cat`, `help stat`, `help mkdir`, `help write`, `write SHELL.TXT`, `cat SHELL.TXT`, `apps`, `pwd`, `ls /`, `ls APPS`, `info ls`, `info cat`, `info stat`, `info mkdir`, `info write`, `cat README.TXT`, and `stat README.TXT` through console and RAMFS authority, reads the filesystem-backed `/APPS/INDEX.TXT` descriptor note, reads and validates `/APPS/LS.APP`, `/APPS/CAT.APP`, `/APPS/STAT.APP`, `/APPS/MKDIR.APP`, and `/APPS/WRITE.APP`, prints compact decoded descriptor cards, requires exact command length before command-body matching, handles `helpX` and `noop` as non-fatal unknown commands through brokered console output, records nineteen recognized commands plus two expected unknowns plus broker-side edit telemetry, proves EOF after the command stream is exhausted, and separately proves ring-3 file mutation by creating `USRNOTE.TXT`, writing `ring3 write ok\n`, reading it back to the user stack, and reporting the expected filesystem counter increments.
- the x86_64 input bridge now includes hardware-backed PS/2 keyboard event proofs without turning the shell into an ambient device reader: IRQ1 drains controller bytes into a bounded broker-owned queue, uses translated set-1 scancodes on the BIOS path, can decode set-2 for framebuffer/UEFI handoff, and now auto-falls back to set-1 when QEMU/firmware presents high-bit set-1 release scancodes after a UEFI handoff. It translates basic extended cursor/delete keys into staged input bytes, actively polls during authorized reads, discards stale pending bytes when switching scancode interpretation or dropping overlong fragments so boot-time firmware or QMP noise cannot permanently block later command lines, exposes PS/2 status, IRQ, poll, scancode, translated-byte, pending, drop, last-key, hardware-read, hardware-line, and hardware-line-byte telemetry through the syscall surface, and verification proves brokered keyboard reads require scoped `input` authority.
- that retired sealed hardware-keyboard path has been replaced by disk-sourced shell descriptors and flat utility binaries. Command execution now stays behind the launch broker and avoids duplicating utility bodies inside the sealed bootstrap image while preserving separate scoped `input`, `console`, and `ramfs` capabilities for every boundary the shell crosses.
- the sealed x86_64 ring-3 transfer image is now generated from readable NASM source instead of a hand-maintained C byte table. `tools\build.ps1 -Architecture x86_64` assembles `kernel\arch\x86_64\runtime_image_user.asm` into a page-aligned 16 KiB persistent-shell bootstrap image, emits the generated C header consumed by `runtime_image.c`, and feeds the same binary into the package archive generator so launch-broker payload size/checksum telemetry remains tied to the actual sealed image bytes.
- the current `x86_64` lane now also emits a verified removable-media UEFI FAT image that boots under OVMF in QEMU, locates GOP, reports framebuffer geometry, draws a bounded firmware pixel pattern, reads back a nonzero draw token, records framebuffer metadata in boot-info, maps a 16 MiB identity/high-half alias window, maps that framebuffer through a dedicated low page-directory at `0xB000`, reserves an additional low handoff page at `0xC000` for broker-installed kernel MMIO page tables, opens the boot volume, reads the staged root `README.TXT` with fixed size/checksum/prefix proof, parses `BOOTMAN.TXT`, loads `KERNEL64.BIN` into an aligned 512 KiB handoff buffer until the payload byte count and checksum match the manifest, allocates exact `EfiLoaderData` pages at a 2 MiB-aligned address inside conventional firmware memory, copies and rechecks the kernel bytes there, separately allocates and verifies the linked scaffold payload at physical `0x10000`, captures both pre-placement and post-placement firmware memory-map summaries, builds the low boot-handoff tables plus trampoline, takes one final silent memory-map key, exits firmware boot services, jumps into the x64 kernel, reloads the kernel descriptor state, draws/logs a kernel-owned framebuffer marker, and reaches the compact bootstrap, second-page filesystem/display, real-media storage, and disk-sourced launch proofs
- the current `x86_64` lane now also packages a verified UEFI optical ISO that boots under OVMF in QEMU with the same GOP framebuffer, boot-media file-read, bounded loader-buffer, firmware-backed kernel-placement, linked-base placement, handoff memory-map, boot-handoff table, kernel-jump, kernel-owned framebuffer draw, and x64 userspace proofs, so both USB-style and DVD-style modern-media paths exist before the full x64 userspace stack lands
- the x86_64 lane now exposes read-only PCI configuration-space inventory through a query-only `hardware-inventory` service capability: BIOS boots prove legacy IDE storage discovery, while Q35 UEFI boots prove AHCI controller discovery, decode the AHCI MMIO base/span/safety flags/token, promote it into a separate brokered MMIO planner as a bounded candidate, deny wrong-owner map requests, install a kernel-only/read-only/no-deref page-table view for valid Q35 AHCI candidates at `0xFFFFFFFF90000000`, prove the exact `511/510/128/0` table indices plus cache-disabled/NX entry flags `0x8000000000000019`, report `map-installed 1`, and then allow only brokered read-only snapshots of AHCI HBA `CAP`/`GHC`/`PI`/`VS` plus implemented/active port state (`SSTS`, signature, command, task-file, command-issue, and error). A separate read-only classifier now derives device kind, link detect/speed/power state, busy/DRQ state, command-issue idleness, and a future-read eligibility bit without mutating the controller. The next brokered layers now stage a non-executing AHCI read-plan token, command-layout token, one-page command-memory preflight token, and broker-private table-prep token from that policy, binding selected port, LBA, block count, operation kind, policy/read-plan/command-plan/memory-plan tokens, command header/table sizes, CFIS/PRDT geometry, command opcode, ATAPI packet opcode, transfer byte hint, command-list/header/table/PRDT/bounce-buffer offsets, PRDT byte count, held-zero DMA address, and checksum transition while proving read, command, command-memory, and table-prep state remain unarmed, unissued, unprogrammed, and DMA-unmapped. It still avoids AHCI command issue, DMA setup, controller-visible table publication, port programming, filesystem authority minting, and storage writes.
- the AHCI command-memory preflight now materializes the future command/bounce page as a broker-owned, page-aligned, zeroed kernel page and then prepares a non-issuing AHCI command-table skeleton in that same private page. Verification requires virtual/physical page telemetry, zero-page checksum `0x76EFDDC5`, table checksum `0x3FBFAF45`, header flags `0x00010025`, CFIS command `0xA0`, ATAPI packet opcode `0x28`, PRDT byte count `2047`, `table-written 1`, and continued zero DMA/port/issue evidence; BIOS/no-AHCI media must keep all materialization and table-prep fields zero and unavailable.
- the x86_64 lane now has a brokered display service proof: UEFI GOP metadata is handed to the kernel through boot-info, the service namespace exposes endpoint class `display`, ring-3 code must present delegated display authority before drawing a bounded marker, clearing a kernel-bounded text panel, and rendering a small 5x7-font text line, and the brokered console service can mirror successful ring-3 console writes into a bounded, line-cleared, scrolling GOP framebuffer viewport without exposing direct framebuffer access to shell processes. UEFI verification requires positive draw/pixel/clear/text/console-mirror/line-clear/scroll telemetry, and raw BIOS verification records the same syscall path as explicitly unavailable instead of bypassing the capability model.
- the x86_64 lane now has a first brokered block service proof: the service namespace exposes endpoint class `block`, the kernel probes a read-only ATA PIO path, callers must present a principal-scoped block service capability routed through the generic capability table, wrong-owner reads are denied, and raw BIOS verification requires an authorized LBA0 read with 512 bytes, a nonzero token, and the `0x55AA` boot signature. UEFI removable and ISO paths currently report block status as unavailable on q35/firmware media instead of faking USB, AHCI, or optical persistence support.

That means LimitlessOS should not fork into two different operating systems. The boundary should be:

- one common service and package contract
- one common installer and policy model
- multiple kernel and loader back ends selected by hardware capability

Long term, the installer should be able to recommend:

- legacy 32-bit minimal image
- 64-bit standard image
- 64-bit image with optional 32-bit compatibility libraries for older apps

## Package management model

LimitlessOS should expose package management through a brokered user-space service, not through ambient root package tools.

The design target is:

- one native Limitless package broker that owns system package transactions
- optional compatibility frontends that present familiar workflows such as `apt`, `dnf` or `yum`, `apk`, and `choco`
- repository and manifest translation layers that map those frontend requests into the broker's capability-checked transaction model
- per-frontend policy and trust settings, so imported ecosystems do not silently inherit full system authority

In practice, that means `apt`, `yum`, `apk`, or `choco` style support should be treated as selectable compatibility interfaces, not as separate privileged package roots that bypass the OS policy system.

That package-broker contract should stay common across both architecture targets. The installer may select different default frontend bundles on 32-bit versus 64-bit installs, but the brokered security model should not fork by architecture.

## Boot and trust chain

Phase 1 bootstrap in this repository uses a simple BIOS path because it is the fastest way to create a bare-metal foundation. The long-term product should support:

- UEFI secure boot
- measured boot
- signed kernel and service manifests

The current install-media split is therefore transitional:

- x86 raw image and BIOS ISO are both now real artifacts
- x86_64 raw image is the verified BIOS scaffold path today
- x86_64 removable UEFI image is now also a verified modern-hardware scaffold path with GOP framebuffer geometry, bounded firmware draw/read-back proof, boot-info framebuffer metadata, dedicated `0xB000` framebuffer mapping, kernel-owned framebuffer draw proof, boot-media file read proof, manifest-checked kernel payload loading into an aligned 512 KiB handoff buffer, firmware-backed 2 MiB-aligned kernel placement, pre/post-placement memory-map capture, low boot-handoff tables, firmware exit, kernel jump, descriptor reload, and x64 userspace proofs
- x86_64 optical UEFI media is now also verified with the same GOP framebuffer, kernel framebuffer handoff/draw proof, brokered display marker/panel/text proof, line-cleared scrolling console-to-framebuffer mirror proof, boot-media file-read, loader-buffer, placement, memory-map, firmware-exit, kernel-jump, and x64 userspace proofs, which makes the remaining x64 hardware milestones about real hardware validation, drivers, persistence, a richer display compositor/user-console path on top of the brokered display service, and installer convergence rather than just boot media or firmware teardown
- rollback protection for critical system components

## AI system placement

The assistant is not a normal user-editable app. The design target is a sealed platform service with:

- a signed UI shell
- a policy broker that mediates every privileged action
- cloud-backed reasoning for heavier tasks
- offline fallback that degrades safely instead of pretending to have cloud capability

The assistant does not get blanket authority. It must ask, explain, and log.




