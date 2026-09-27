# Real Binary And Real Hardware Gate

Effective after M21: new Product progress must be proven with real, externally built software or real hardware behavior, not synthetic test processes. The per-milestone evidence for M22–M171 (commands, telemetry, SHA-256 values) is archived verbatim in [history/real-binary-gate-log.md](history/real-binary-gate-log.md).

## Rule

A milestone cannot be accepted as application execution, browser readiness, network readiness, storage readiness, or daily-driver progress if its only evidence is a synthetic process, a repo-built fixture, telemetry from a proof-only path, or hardcoded status text.

Existing scaffold, denial, fixture, and repo-built app checks may remain as regression tests for safety boundaries. They must be labeled as foundation or regression evidence, not as proof that a user-facing capability works.

## What counts

A real-binary execution claim requires all of the following:

- The executable is an unmodified binary built outside this repository by a normal third-party toolchain, distribution package, or upstream release.
- The binary is loaded from a user-visible path such as `/APPS`, `/HOME/bin`, mounted FAT storage, ISO media, or removable media.
- The loader, process setup, syscall path, filesystem path, and terminal output path are the same paths a user would use interactively.
- The binary's output, exit status, and failure modes are visible through the Product shell or GUI.
- Evidence records provenance: source package or URL, version, SHA-256, `file`/`readelf`/`objdump` metadata when available, the exact LimitlessOS path, the command run, and the observed output.

A hardware claim additionally requires output captured on the physical device. QEMU success does not count for a claim about the MSI laptop.

## Evidence accepted so far

- **Static third-party ELF (post-M21):** BusyBox 1.35.0 static musl, linked at `0x52000000`, runs from NVMe FAT as `linux /APPS/BUSYBOX ...`: `echo`, `cat /proc/meminfo`, NVMe file reads, `ls`, and an interactive `sh`.
- **Process model (M22–M26):** per-process page tables, `fork`/`wait4`, pipes, Linux VFS path execution, and `execve` inheritance of cwd and pipe fds, all driven by BusyBox ash.
- **Non-BusyBox packages (M27–M61):** suckless sbase 0.1 `echo`, `cat`, `env` built from the upstream tarball; PATH search, `/usr/local/bin` aliases, `execvp`, environment export, and path canonicalization edge cases, including truthful denials.
- **Runtime breadth (M62–M69):** signal delivery, `clone` threads, futex contention, TLS, and bounded file-backed `mmap`.
- **Dynamic ELF (M70–M105):** `PT_INTERP` loading, relocation processing, libc startup, and dynamic programs exercising environment, heap, pthreads/TLS, file I/O, seek, directories, `*at` calls, `fcntl`/`dup`, pipes, and fork+pipe composition.
- **Boot-media staging (M120, M193):** a dynamic app and its interpreter staged by the UEFI loader run from boot media when NVMe FAT is unavailable. Since M193 the stage area sits above the kernel image; `linux /APPS/DYNLDLIMIT` runs to `exit_group` with zero page faults under QEMU.

The proven Linux syscall surface and every artifact's SHA-256 are listed in the history log.

## Hardware and daily-driver order

1. **Terminal and input reliability** on physical hardware (keyboard works on the MSI laptop; touchpad and USB mouse are still open).
2. **Persistent storage:** make the internal NVMe (behind Intel VMD on the MSI laptop) readable through a user-visible path, then add safe writes only to an explicitly approved LimitlessOS target.
3. **Network:** wired Ethernet before the Intel AX1675 Wi-Fi, which needs firmware loading, regulatory handling, scan/auth/association, key management, and a full 802.11 data path.
4. **External binaries:** deepen process semantics with more third-party software rather than more fixtures.
5. **Browser last:** it needs network, storage, a C runtime, dynamic linking, threads, timers, memory mapping, filesystem semantics, certificates, fonts, graphics, input, and real process isolation.

## Non-claims

The following never count as Product capability acceptance by themselves:

- embedded ELF or PE byte arrays
- repo-assembled flat binaries
- synthetic Linux/Windows/Mach-O process records
- syscall stubs that are not exercised by an external binary
- denial-only MMIO or storage chains
- status panels with no underlying driver or runtime behavior
- QEMU-only success when the claim is about physical hardware
