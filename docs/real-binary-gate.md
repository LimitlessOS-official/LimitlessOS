# Real Binary And Real Hardware Gate

Effective after M21, new Product progress must be proven with real externally built software or real hardware behavior, not synthetic test processes.

Current status: the first static Linux x86_64 ELF execution gate and the M22 per-process page table foundation gate are crossed on the UEFI Product path. The verified commands are `linux /APPS/BUSYBOX echo limitless-real-binary`, `linux /APPS/BUSYBOX cat /proc/meminfo`, `linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt`, `linux /APPS/BUSYBOX ls /nvme/apps`, `linux /APPS/BUSYBOX ls -l /proc/self/exe`, `linux /APPS/BUSYBOX ls -l /proc/self/fd`, `linux /APPS/BUSYBOX ls -l /proc/self`, `linux /APPS/BUSYBOX sh`, and `linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'`, using externally built BusyBox 1.35.0 static musl ET_EXEC binaries staged on the NVMe FAT image at `/APPS/BUSYBOX`.

Current BIOS budget note: the Product BIOS path has 101 reserve sectors, below the 128-sector warning threshold but still inside the hard 1024-sector loader limit. New real-binary work must continue to protect the BIOS path from accidental large buffers or code growth.

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

The first acceptable app-execution milestone is a static Linux x86_64 ELF from outside the repo. A static BusyBox is the accepted first target because it avoids the dynamic linker while still proving a real ELF loader, real process setup, real syscalls, real filesystem access, and real terminal output.

The current proof is recorded by `tools\verify-real-binary-gate.ps1`, direct `tools\verify-qemu.ps1 -RealBinaryGate` runs, and `build\real-binary-gate-provenance.txt` when the provenance verifier is used. It records provenance, SHA-256, file/readelf metadata, `/APPS/BUSYBOX`, the echo command and visible output, the `/proc/meminfo` cat command and visible output, the `/nvme/apps/data/file.txt` cat command and visible NVMe FAT fixture output, the `/nvme/apps` directory listing output, the `sh` banner and `$` prompt, the M22 CR3 isolation echo command, exit codes, positive `drs-realbin` telemetry, NVMe VFS bind/release/read/readdir telemetry, terminal-size ioctl telemetry, cwd/chdir telemetry, relative path canonicalization telemetry, proc symlink telemetry, mixed `/proc/self` directory telemetry, and negative coverage for missing file, dynamic `PT_INTERP`, low-address ET_EXEC binaries, oversized input, and BIOS unavailable behavior. The low-address negative uses a real upstream-default-address BusyBox when available and proves that ET_EXEC loads below `0x01000000` remain denied while the kernel still protects the low compatibility window. CI or release verification can require that proof explicitly with `-RequireLowAddressNegative`; the verifier rejects that switch if negative tests are skipped. The `-RequireShellCwdLoop` mode proves BusyBox ash current-working-directory behavior through `/`, `/nvme/apps`, and back to `/`. The `-RequireRelativePathProof` mode proves relative `.` and `..` canonicalization with `cat nvme/apps/./data/../data/file.txt` and `ls -l nvme/apps/./data/..`. The `-RequireProcSymlinkProof` mode proves `readlink(2)` through BusyBox `ls -l /proc/self/exe`. The `-RequireProcFdProof` mode proves stable proc-fd symlink enumeration for brokered fds 0, 1, and 2 through BusyBox `ls -l /proc/self/fd`; the bounded VFS intentionally skips the directory iterator fd for that directory so the verifier does not publish a link that can disappear before BusyBox's later `readlink` pass. The `-RequireProcSelfProof` mode proves mixed `/proc/self` directory enumeration for regular pseudo-files, the `fd` directory, and the `exe` symlink through BusyBox `ls -l /proc/self`. The `-TraceShellForkBoundary` mode records BusyBox ash's current external-command boundary as typed `fork-enosys` telemetry without requiring fork support. The NVMe file proof uses `/APPS/DATA/FILE.TXT` through the Linux-visible `/nvme/apps/data/file.txt` path because the older `/NVME.TXT` fixture is intentionally mutated by the broker-private `drs-nvme-fat` write checkpoint before the shell runs.

Passing artifacts:

- `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000`: BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, SHA-256 `5CDE8968EB2FEDB62DEA27947CD269BC57AD9C8B142ABFF0C3B1514A0238E8D9`, verified with `echo limitless-real-binary`, `cat /proc/meminfo`, and `cat /nvme/apps/data/file.txt`.
- `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-ls`: BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, and `sh`, SHA-256 `299DE064F51DA04DE99227F26F2EAB60C95F400C1B83731E14E3E28F86695652`, verified with `ls /nvme/apps` and `sh`.
- `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`: BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, `sh`, `FEATURE_SH_STANDALONE`, and `FEATURE_SH_NOFORK`, SHA-256 `05993EEDEFBA765099A989985B1ED7363E2F2F3BC731F7A92CF4D1BFB6628B69`, verified with the default real-binary gate including `echo`, `cat`, `ls`, and the bounded `sh` builtin loop.

Proven syscall surface for those artifacts:

- Linux process startup: `brk`, `mmap`, `mprotect`, `munmap`, `arch_prctl`, `set_tid_address`, `rt_sigaction`, and `rt_sigprocmask`.
- Console and file I/O: `read`, `write`, `readv`, `writev`, `openat`, and `close`; `open`, `openat`, stat-family paths, and `chdir` now canonicalize relative paths against the Linux persona cwd before VFS resolution.
- Metadata: `stat`, `newfstatat`, including `AT_EMPTY_PATH`, and `lstat`.
- Proc symlink readback: `readlink`, with passing `/proc/self/exe` telemetry `readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14`, visible output `lrwxrwxrwx    1        14 /proc/self/exe -> /proc/self/exe`, and `unimplemented 0`; passing `/proc/self/fd` telemetry `getdents64 2 getdents64-entries 3 getdents64-bytes 72 stat 4 stat-denial 0 stat-fault 0 readlink 3 readlink-bytes 33 readlink-denial 0 readlink-fault 0 readlink-last-result 11`, with visible links for `0 -> anon:[fd 0]`, `1 -> anon:[fd 1]`, and `2 -> anon:[fd 2]`; and passing mixed `/proc/self` telemetry `getdents64 2 getdents64-entries 6 getdents64-bytes 168 stat 7 stat-denial 0 stat-fault 0 readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 writev 6 writev-bytes 209`, with visible entries for `environ`, `cmdline`, `status`, `fd`, `exe -> /proc/self/exe`, and `maps`.
- Directory enumeration: `getdents64`, with passing telemetry `getdents64 2 getdents64-entries 2 getdents64-bytes 56`, and visible output `busybox` plus `data`.
- Current working directory: `getcwd` and `chdir`, with passing shell-loop telemetry `getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 chdir 2 fchdir 0 chdir-denial 0 chdir-fault 0`, and visible output `/`, `/nvme/apps`, and `/`.
- Relative path canonicalization: `cat nvme/apps/./data/../data/file.txt`, with passing telemetry `path-relative 1 path-dot 1 path-dotdot 1 path-fault 0`, visible output `Nested FAT32 path fixture`, and NVMe VFS telemetry `vfs-nvme-reads 2 vfs-nvme-bytes 27`; plus `ls -l nvme/apps/./data/..`, with passing telemetry `console-bytes 65`, `getdents64 2 getdents64-entries 2 getdents64-bytes 56`, `stat 3 stat-denial 0 stat-fault 0`, `path-relative 4 path-dot 4 path-dotdot 4 path-fault 0`, `writev 2 writev-bytes 65`, `ioctl 1 ioctl-tty 1`, `vfs-nvme-readdirs 4 vfs-nvme-dirents 2`, and visible long-listing output `-r--r--r--    1    145264 busybox` plus `dr-xr-xr-x    2         0 data`.
- Terminal queries: `ioctl(TIOCGWINSZ)` for brokered terminal fds, with fixed 25x80 `winsize` telemetry `ioctl-tty`.
- Identity shims proven by BusyBox `sh`: `geteuid` returns fixed uid `1000`, and `getppid` returns fixed ppid `1`.
- Process-name shim proven by standalone-shell BusyBox: `prctl(PR_GET_NAME)` and `prctl(PR_SET_NAME)` maintain the 16-byte Linux comm field used by BusyBox applet dispatch; other `prctl` options remain `ENOSYS`.
- Exit: `exit_group`.

Current shell proof: `linux /APPS/BUSYBOX sh` prints the BusyBox ash banner and a `$` prompt through brokered console output, consumes a bounded brokered-stdin mini loop `echo shellloop`, `true`, `echo aftertrue`, then exits 0. Passing telemetry includes `console-bytes 90`, `read 40`, `read-bytes 40`, `write 2`, `write-bytes 20`, `writev 6`, `writev-bytes 70`, `ioctl 3`, `ioctl-tty 3`, `ioctl-enotty 0`, `ioctl-enosys 0`, `prctl 2`, `prctl-set-name 1`, `prctl-get-name 1`, `prctl-enosys 0`, and `unimplemented 0`. Standalone `linux /APPS/BUSYBOX ls /nvme/apps` separately proves visible directory entries `busybox  data` with `getdents64 2`, `getdents64-entries 2`, `getdents64-bytes 56`, `vfs-nvme-readdirs 4`, and `vfs-nvme-dirents 2`.

Current cwd shell proof: `tools\verify-real-binary-gate.ps1 -RequireShellCwdLoop` runs BusyBox ash builtins `pwd`, `cd /nvme/apps`, `pwd`, `cd /`, and `pwd` in the brokered stdin loop. Passing telemetry includes `console-bytes 101`, `exit 0`, `cleanup 1`, `getcwd 1`, `getcwd-bytes 2`, `getcwd-denial 0`, `getcwd-fault 0`, `chdir 2`, `fchdir 0`, `chdir-denial 0`, `chdir-fault 0`, `fork 0`, and `unimplemented 0`.

## M22 Per-Process Page Table Gate

M22 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'`. The command uses the externally built BusyBox 1.35.0 static musl ET_EXEC standalone-shell artifact staged from NVMe FAT at `/APPS/BUSYBOX`, prints `m22-cr3-isolation`, exits 0, and proves the Linux persona runs on a process-owned PML4 instead of the kernel root.

The static pool design is fixed-size and UEFI-only: 4 process roots, 19 pages per root, 4 x 19 x 4096 = 311,296 bytes for PML4/PDPT/PD/PT tables. Each process root has private lower-half user/VMA tables and copied shared higher-half kernel/MMIO mappings. CR3 switches happen at scheduler task start/swap/exit boundaries; there is intentionally no syscall-entry CR3 switch because each process root already shares the higher-half kernel mapping needed by syscall entry and brokered kernel work.

The transitional low identity compatibility mapping remains present and must stay visible as `low-compat 1` in every M22 gate run until the low-window compatibility path is removed. This means M22 proves process-owned roots, private lower-half user/VMA page tables, shared kernel/MMIO mappings, and correct scheduler CR3 switching; it does not yet claim final isolation below the low compatibility window.

M22 final artifact budget: UEFI reserve moved from 852,544 bytes at the first successful pool-stage build to 848,416 bytes at final acceptance. BIOS reserve held at 101 sectors, below the 128-sector warning threshold but still inside the hard 1024-sector loader limit.

M22 acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204A000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 low-compat 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 18 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 1 write-bytes 18 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 0 fork-enosys 0 fork-denial 0 fork-last-rip 0x0000000000000000 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

Current shell external-command gap: the standalone-shell artifact proves BusyBox's `FEATURE_SH_STANDALONE`/`FEATURE_SH_NOFORK` path for NOFORK applets such as `echo` and `true`, but BusyBox marks `ls` and `cat` as NOEXEC rather than NOFORK. Running `ls /nvme/apps` and `cat /nvme/apps/data/file.txt` from inside ash still attempts fork syscall `57`, reports `sh: can't fork: Function not implemented`, and records typed telemetry such as `fork 2 fork-enosys 2 fork-last-rip 0x000000005200EF74`. `tools\verify-real-binary-gate.ps1 -TraceShellForkBoundary` is the reproducible trace mode for this boundary. `tools\verify-real-binary-gate.ps1 -RequireShellApplets` remains the opt-in mode for future fork/process work and requires in-shell `ls`/`cat` output plus nonzero NVMe/getdents telemetry.

After M22, the next target should be M23 `trace-driven fork boundary`: keep UEFI Product scope, use the real BusyBox shell path that currently reports `fork-enosys`, and implement only the bounded process duplication needed to let a shell-spawned external command run under a child process root. The starting acceptance target should be the existing `-TraceShellForkBoundary`/`-RequireShellApplets` path: in-shell `ls /nvme/apps` and `cat /nvme/apps/data/file.txt` should stop reporting `fork-enosys`, should produce visible NVMe directory/file output, and should clean up both parent and child PML4 roots. M23 should cover parent/child PML4 allocation, VMA ownership/copy semantics, FD/persona/audit duplication, child exit cleanup, and parent wait behavior. It should not attempt dynamic linking, glibc, real threading, signal delivery, sockets, broad clone flag compatibility, or broad ioctl/device control.

Later targets are:

- dynamic Linux ELF with `PT_INTERP`, relocations, libc, environment, and filesystem semantics
- Windows PE console executable with real import resolution and process parameters
- Mach-O executable with a real loader path, only after the Linux and Windows paths have stopped being scaffolds

## Hardware And Daily-Driver Order

The daily-driver path is gated by real hardware:

- Terminal reliability first: the QEMU verifier now has a real BusyBox `sh` banner/prompt proof through brokered console output and bounded brokered stdin; hardware-terminal and GUI-focus behavior still need physical-device evidence before broader daily-driver claims.
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
