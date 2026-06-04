# Real Binary And Real Hardware Gate

Effective after M21, new Product progress must be proven with real externally built software or real hardware behavior, not synthetic test processes.

## Rule

A future milestone cannot be accepted as application execution, browser readiness, network readiness, storage readiness, or daily-driver progress if its only evidence is a synthetic process, a repo-built fixture, proof-only telemetry, or hardcoded status text.

Existing scaffold, denial, fixture, and repo-built app checks may remain as regression tests for safety boundaries. They must be labeled as foundation or regression evidence, not as proof that a user-facing capability works.

## What Counts

A real-binary execution claim requires all of the following:

- The executable is an unmodified binary built outside this repository by a normal third-party toolchain, distribution package, or upstream release.
- The binary is loaded from a user-visible path such as `/APPS`, `/HOME/bin`, mounted FAT storage, ISO media, or removable media.
- The loader, process setup, syscall path, filesystem path, and terminal output path are the same paths a user would use interactively.
- The binary's output, exit status, and failure modes are visible through the Product shell or GUI.
- Evidence records provenance: source package or URL, version, SHA-256, `file`/`readelf`/`objdump` metadata when available, the exact LimitlessOS path, the command run, and the observed output.

## First Execution Target

The first acceptable app-execution milestone should be a static Linux x86_64 ELF from outside the repo. A static BusyBox or static coreutils binary is a better first target than a distro `/bin/ls`, because it avoids the dynamic linker while still proving a real ELF loader, real process setup, real syscalls, real filesystem access, and real terminal output.

After a static external ELF runs, the next targets are:

- dynamic Linux ELF with `PT_INTERP`, relocations, libc, environment, and filesystem semantics
- Windows PE console executable with real import resolution and process parameters
- Mach-O executable with a real loader path, only after the Linux and Windows paths have stopped being scaffolds

## Hardware And Daily-Driver Order

The daily-driver path is gated by real hardware:

- Terminal reliability first: fix the focus-clearing bug and the long input delay before using the shell as evidence.
- Network next: wired Ethernet should be attempted before Intel AX1675 Wi-Fi because Wi-Fi requires firmware loading, regulatory handling, scan/auth/association, key management, and a full 802.11 data path.
- Persistent storage next: make the NVMe namespace readable through a user-visible filesystem path, then add safe writes only to an explicitly approved LimitlessOS target.
- External binary execution next: run a real third-party static ELF from user-visible storage.
- Browser last: a browser needs network, persistent storage, a C runtime, dynamic linking, threads, timers, memory mapping, filesystem semantics, certificates, fonts, graphics, input, and real process isolation.

## Non-Claims

The following do not count as future Product capability acceptance by themselves:

- embedded ELF byte arrays
- repo-assembled flat binaries
- synthetic Linux/Windows/Mach-O process records
- syscall stubs that are not exercised by an external binary
- denial-only MMIO or storage chains
- status panels with no underlying driver or runtime behavior
- QEMU-only success when the claim is about the MSI laptop's hardware
