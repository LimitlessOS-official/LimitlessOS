# LimitlessOS Status

Last updated: 2026-06-08

## Accepted Baseline

M1 cleanup-final is accepted. The accepted M1 artifact was archived at `dist/m1-evidence-20260511-140840-cleanup-final` with:

- kernel bytes: 468176
- kernel sectors: 915 / 1024
- reserve: 109
- checksum: 0x53122EE5
- runtime help verified: true
- runtime apps verified: true
- persistence verifier printing authority, denial, commit, same-image, and non-RAM evidence

## Current Milestone

M53 is `absolute localbin missing executable denial proof`; the post-M21 real static Linux ELF gate, the M22 per-process CR3 isolation gate, the M23 bounded fork/exec/wait gate, the M24 Unix pipeline gate, the M25 Linux-visible `/bin`/`/usr/bin` path execution gate, the M26 forked-child `execve` inheritance gate, the M27 independently staged static ET_EXEC gate, the M28 third-party non-BusyBox package gate, the M29 broader third-party utility read/pipeline gate, the M30 third-party `/usr/local/bin` PATH directory gate, the M31 default initial `PATH` environment gate, the M32 bounded shell environment expansion gate, the M33 cwd/PWD synchronization gate, the M34 child-observed environment mutation gate, the M35 non-shell third-party `execvp` handoff gate, the M36 non-shell third-party PATH `execvp` gate, the M37 fully PATH-resolved third-party pipeline gate, the M38 expanded `/usr/local/bin` inspection gate, the M39 absolute `/usr/local/bin` execution gate, the M40 absolute `/usr/local/bin` file-read gate, the M41 cwd-relative absolute-localbin file-read gate, the M42 `..` relative-path absolute-localbin gate, the M43 current-directory `./` absolute-localbin gate, the M44 non-shell current-directory `./` exec handoff gate, the M45 relative executable `..` localbin handoff gate, the M46 absolute executable `..` localbin handoff gate, the M47 absolute executable mixed `.`/`..` localbin handoff gate, the M48 repeated-slash absolute executable localbin handoff gate, the M49 root-clamped absolute executable `..` localbin handoff gate, the M50 over-root absolute executable `..` clamp gate, the M51 trailing-slash executable denial gate, the M52 trailing-slash directory enumeration gate, and the M53 absolute missing-localbin executable denial gate are crossed on the UEFI Product path.

M1 cleanup-final through M20 native app execution pipeline are accepted, and M18.1 closed the UEFI real-firmware handoff compatibility boundary. M19 shifted networking from hardware-gated proof to app-facing brokered service: UEFI Product publishes a network service endpoint plus a narrow syscall-level TCP-client socket contract over the existing broker-owned DHCP/DNS/HTTP path. M21 provides the first manifest-driven native app SDK foundation over that service. Product builds read `apps/native/*.json`, assemble app binaries, generate `.APP` descriptors, stage descriptor/binary pairs into `/APPS`, sign package payload records, and load apps by name through a generic descriptor parser before Ring-3 launch. The post-M21 gate adds a separate UEFI-only `linux <path> [args...]` Product shell path that loads one externally built static Linux x86_64 ELF from NVMe FAT storage and executes it through the Linux persona/syscall path with brokered console output. M22 gives each UEFI Product Linux persona launch its own PML4 from a fixed static pool, keeps kernel/MMIO mappings shared, switches CR3 at scheduler start/swap/exit boundaries, and proves the running hardware CR3 matches the process root while the lower-half user page tables are private. M23 adds bounded Linux `fork(57)` plus blocking `wait4(61)` for the observed BusyBox ash external-command path: the parent and child receive distinct PML4 slots, the child gets a full copied VMA/address-space/FD/persona/VFS/audit state, the parent waits and collects the child exit status, and both process roots are returned to the static pool. M24 adds Linux `pipe(22)` ABI exposure over the existing fixed pipe provider and exact-fd pipe endpoint inheritance across fork, proving that BusyBox ash can compose `fork`, `pipe`, inherited fds, `read`, `write`, and `wait4` in a real pipeline. M25 widens `execve` for real external binaries, switches real exec images to a 64 KiB UEFI-only stack while preserving the old synthetic 4 KiB exec test constants, exposes read-only BusyBox applet aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin`, and proves BusyBox ash can find and execute the real staged NVMe BusyBox binary through ordinary Linux PATH search. M26 proves the M25 exec transfer composes with M22/M23/M24 process semantics: forked BusyBox ash children inherit cwd, VFS authority, and pipe endpoints, then replace themselves through `execve`, move data through the pipe, and return cleanly to the parent through `wait4`. M27 proves the NVMe FAT image generator and QEMU verifier can stage a second static ET_EXEC artifact at `/APPS/SMOKE`, expose it through the Linux-visible `/nvme/apps/smoke` VFS path, and execute it through a forked BusyBox ash pipeline without using the BusyBox alias backend. The M27 implementation also makes Linux task FS-base state scheduler-owned, so `arch_prctl(ARCH_SET_FS)` TLS bases are saved, restored, and seeded across task switches, fork, exec, wait, and pipe scheduling. M28 replaces the local SMOKE evidence with an upstream suckless sbase 0.1 `echo` utility built by the musl cross toolchain as a static non-PIE ET_EXEC at `0x52000000`, staged as `/APPS/SBECHO`, launched directly, and execed through BusyBox ash as `/nvme/apps/sbecho` in a real pipeline. M28 also fixes the real-binary cleanup invariant so programs that legitimately close stdout before exit are accepted when the fd table is fully detached. M29 adds a second upstream suckless sbase 0.1 utility, `cat`, staged as `/APPS/SBCAT`, proves it can directly read `/nvme/apps/data/file.txt` through the NVMe VFS, and proves a two-executable third-party pipeline `/nvme/apps/sbecho | /nvme/apps/sbcat` without relying on the BusyBox alias backend for the data consumer. M30 exposes those third-party sbase utilities through a bounded Linux-visible `/usr/local/bin` directory, proves BusyBox ash can resolve `sbecho` and `sbcat` by PATH name, and keeps the backend tied to the staged NVMe FAT artifacts rather than a BusyBox applet alias. M31 seeds a bounded default Linux environment for real launches with `PATH=/usr/local/bin:/bin:/usr/bin`, proves BusyBox ash can resolve the third-party utilities without command-local PATH assignment, and proves the environment is inherited into forked child `execve` calls. M32 extends that fixed environment with `HOME=/`, `USER=limitless`, and `PWD=/`, proves BusyBox ash expands `$USER:$PWD` correctly, and proves those entries are inherited into forked child `execve` calls. M33 proves BusyBox ash updates `PWD` after `cd /nvme/apps`, passes that updated environment through forked child `execve`, and keeps the Linux persona cwd coherent with relative PATH and pipe execution. M34 adds an upstream suckless sbase 0.1 `env` utility staged as `/APPS/SBENV` and proves a third-party child process can observe a shell-mutated exported `USER=operator` environment through a forked pipeline. M35 proves that same third-party `sbenv` process can call `execvp` with an explicit Linux VFS path, replace itself with another real ET_EXEC image, preserve the explicit `USER=operator` environment, and complete through the inherited pipeline. M36 adds the bounded `/usr/local/bin/sbenv` alias and proves that non-shell third-party `execvp` PATH search can locate and execute another third-party ET_EXEC utility by name. M37 proves the full pipeline can drop all explicit child executable paths: BusyBox ash resolves `sbenv` and `sbcat` by PATH, then the first `sbenv` resolves the second `sbenv` by PATH before the pipeline completes. M38 extends the host NVMe staging verifier to carry all three third-party artifacts at once and proves `/usr/local/bin` visibly enumerates `sbenv`, `sbcat`, and `sbecho` with no stat denials. M39 proves the same bounded third-party namespace works for direct absolute `/usr/local/bin/...` execution, including a third-party `sbenv` process replacing itself through an absolute `/usr/local/bin/sbenv` path and piping to absolute `/usr/local/bin/sbcat`. M40 proves absolute `/usr/local/bin/sbcat` can read a real NVMe FAT file directly and that two forked absolute-localbin `sbcat` children can pipe that file content with clean process, pipe, and VFS cleanup. M41 proves those absolute-localbin child exec paths inherit BusyBox ash cwd after `cd /nvme/apps` and can read cwd-relative `data/file.txt` through the NVMe VFS. M42 proves the same inherited cwd path handles `..` canonicalization from `/nvme/apps/data` back to `/nvme/apps/data/file.txt` in forked absolute-localbin children. M43 makes child `execve` canonicalize relative executable paths before VFS stat/read and proves `./sbcat` resolves from cwd `/usr/local/bin` into the bounded localbin alias table. M44 proves that same current-directory relative exec path composes with a third-party `sbenv` process doing its own explicit `./sbenv` handoff before piping into `./sbcat`. M45 proves relative executable `..` segments from cwd `/usr/local/bin` canonicalize back into the bounded localbin alias table for both shell-launched children and the non-shell `sbenv` handoff. M46 proves absolute executable `..` segments in `/usr/local/bin/../bin/...` canonicalize into the same bounded localbin alias table without depending on cwd changes. M47 proves mixed absolute executable `.` and `..` segments in `/usr/local/./bin/../bin/...` canonicalize into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M48 proves repeated slashes in `/usr//local/bin/...` and `/usr/local//bin/...` canonicalize into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M49 proves bounded upward `..` traversal in `/usr/local/bin/../../local/bin/...` canonicalizes back into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M50 proves over-root `..` traversal in `/../../usr/local/bin/...` and `/../../../usr/local/bin/...` clamps at `/` and canonicalizes into the same bounded localbin alias table for both BusyBox ash-launched children and a non-shell `sbenv` handoff. M51 preserves trailing-slash intent across execve canonicalization and denies slash-suffixed non-directory executable targets before binary read. M52 proves that denial remains scoped to executable targets and does not break trailing-slash directory enumeration. M53 maps failed exec binary stat to `ENOENT`, proving a missing absolute-localbin executable reports `not found` while the bounded alias tables deny cleanly. M21 native apps are still not Linux/Windows app personas, and the Product surface still does not include a server socket API, raw packet API, arbitrary app network data plane, dynamic Linux linking, vfork, broad clone flag compatibility, real threading, signal delivery, sockets, broad ioctl/device control, or broad third-party package compatibility.

Post-M21 acceptance is governed by `docs/real-binary-gate.md`. Synthetic processes, repo-built flat binaries, embedded byte arrays, and denial-only telemetry may remain regression evidence, but they no longer count as forward Product capability evidence. The first execution claim now has real-binary evidence from externally built BusyBox 1.35.0 static musl ET_EXEC artifacts linked at `0x52000000`, staged as `/APPS/BUSYBOX`, loaded from the NVMe FAT image, and proven to print brokered console output with exit code 0. The current BusyBox artifact also proves a minimal `sh` path: ash prints its banner and `$` prompt through brokered console output, consumes verifier-provided shell loops through bounded brokered stdin, and exits cleanly. The M22 acceptance command `linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'` proves that a real externally built static Linux binary now runs with a process-owned root distinct from the kernel root while still using shared higher-half kernel/MMIO mappings. The M23 acceptance command `linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'` proves a real BusyBox shell can fork a child command, run it against the NVMe VFS, wait for exit status 0, and clean up both parent and child PML4 slots while producing visible directory output. The M24 acceptance command `linux /APPS/BUSYBOX sh -c 'echo hello | cat'` proves a real BusyBox shell can create a pipe, fork both pipeline sides, inherit pipe fds into children, transfer bytes through the pipe, reap both children, and release all pipe and PML4 resources. The M25 acceptance commands `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'` and `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'` prove normal BusyBox ash PATH search can resolve Linux-visible applet paths backed by the real staged NVMe BusyBox binary and that `/usr/bin` enumerates the fixed virtual applet directory without stat denials. The M26 acceptance command `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'` proves forked child exec images inherit cwd, relative-path VFS behavior, pipe endpoints, and brokered console output correctly. The M27 acceptance command `linux /APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'` proves a second independently staged static ET_EXEC can be loaded from `/APPS/SMOKE`, resolved through the Linux-visible NVMe VFS, execed by a forked child, piped to another child, and cleaned up with no page faults, no CR3 repair, no pipe leaks, and no PML4 leaks. The M28 acceptance commands `linux /APPS/SBECHO m28-sbase-direct` and `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'` prove a third-party non-BusyBox package utility can launch directly from `/APPS/SBECHO`, then be found through `/nvme/apps/sbecho`, fork/execed by BusyBox ash, piped to `/bin/cat`, and cleaned up without page faults, CR3 repair, pipe leaks, or PML4 leaks. The M29 acceptance commands `linux /APPS/SBCAT /nvme/apps/data/file.txt` and `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'` prove a second upstream sbase utility can read a real NVMe file directly and can consume a pipe as a third-party ET_EXEC child with no BusyBox alias backend involved. The M30 acceptance command `linux /APPS/BUSYBOX sh -c 'PATH=/usr/local/bin:/bin:/usr/bin; sbecho m30-path-pipe | sbcat'` proves third-party ET_EXEC utilities can be discovered by PATH name through `/usr/local/bin`, forked, execed, piped together, and cleaned up while their actual bytes still come from the staged `/APPS/SBECHO` and `/APPS/SBCAT` NVMe FAT artifacts. The M31 acceptance command `linux /APPS/BUSYBOX sh -c 'sbecho m31-default-path | sbcat'` proves the same PATH pipeline now works from the default initial Linux environment, without command-local PATH assignment. The M32 acceptance command `linux /APPS/BUSYBOX sh -c 'sbecho $USER:$PWD | sbcat'` proves BusyBox ash expands the default `USER` and `PWD` variables and carries the four-entry environment through child exec. The M33 acceptance command `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; sbecho $PWD | sbcat'` proves BusyBox ash updates `PWD` after `chdir`, exports it to forked child exec images, and keeps it coherent with the Linux persona cwd used for relative path and PATH pipeline execution. The M34 acceptance command `linux /APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'` proves a third-party child process observes the shell-mutated exported environment through `execve`, pipe, and wait cleanup. The M35 acceptance command `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator /nvme/apps/sbenv | /nvme/apps/sbcat'` proves a third-party process can perform the extra `execvp` handoff itself. The M36 acceptance command `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator sbenv | /nvme/apps/sbcat'` proves that non-shell third-party `execvp` PATH search can find `sbenv` through the bounded `/usr/local/bin` alias table. The M37 acceptance command `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator sbenv | sbcat'` proves BusyBox ash and the non-shell `sbenv` process can all resolve third-party utilities through the default PATH. The M38 acceptance command `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin'` proves the expanded bounded third-party PATH namespace is inspectable and reports all three aliases with no stat denials. The M39 acceptance command `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv USER=operator /usr/local/bin/sbenv | /usr/local/bin/sbcat'` proves direct absolute `/usr/local/bin` execution works for BusyBox ash, non-shell `execvp`, and the pipeline consumer.

Proposed next milestone: M54 `PATH localbin missing executable denial proof`. M54 should exercise a missing command resolved by BusyBox ash through the default `PATH` and prove `/usr/local/bin`, `/bin`, and `/usr/bin` lookup failures report `not found` without touching unintended NVMe backend reads, leaking pipe/PML4 state, or regressing valid alias lookup.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifacts:

- BIOS kernel: `KERNEL64-BIOS.BIN`
- BIOS kernel bytes: 472160
- BIOS kernel sectors: 923 / 1024
- BIOS reserve: 101
- BIOS checksum: recorded in the generated artifact inventory
- UEFI kernel: `KERNEL64.BIN`
- UEFI kernel bytes: 1,269,280
- UEFI kernel byte limit: 2,097,152 bytes
- UEFI byte reserve: 827,872
- UEFI checksum: recorded in the generated artifact inventory; it changes when the build-time package signing key is regenerated
- BIOS sector budget status: warning threshold below 128 reserve sectors, hard 1024-sector loader limit still satisfied
- boot contract: split path. BIOS keeps the 1024-sector hard limit and 128-sector warning. UEFI Product uses a 2 MiB `KERNEL64.BIN` file-size contract verified against `BOOTMAN.TXT` byte count and `fnv1a-32` loader checksum, with no UEFI sector arithmetic. `BOOTMAN.TXT` also records `kernel-sha256` for external artifact verification.
- UEFI handoff: fixed low pages are preferences only. The loader verifies/allocates through the UEFI memory map, supports dynamic linked-kernel and handoff-table fallback, and reports allocation name/status/conflict/fallback diagnostics instead of silently freezing.

Product behavior:

- x86_64 boot through disk, UEFI removable media, and UEFI ISO verification
- UEFI Product login gate before desktop/session access
- first-run setup for one local user when `/USERDB.TXT` is missing
- bcrypt `$2b$` password hash persisted in brokered NVMe FAT storage
- `lock` command and Settings lock path return to the login screen and resume the session after unlock
- persistent ring-3 shell
- Product desktop with brokered compositor/window-manager authority
- truthful `help`, `apps`, and `ls apps` output
- brokered DHCP/DNS/TCP/HTTP network status plus M19 restricted TCP-client socket status through the `net` shell builtin when virtio-net or e1000e hardware is present
- Terminal, File Manager, and Settings GUI apps opened through real click interaction
- Installer GUI app opened through the brokered desktop launcher
- Product service lifecycle/status query surface
- UEFI-only brokered network service endpoint and restricted socket syscall surface
- manifest-driven native app descriptor/binary build path and generic UEFI Product native app loader
- UEFI-only real static Linux ELF launch path through `linux <path> [args...]`, currently verified with `/APPS/BUSYBOX echo limitless-real-binary`, `/APPS/BUSYBOX cat /proc/meminfo`, `/APPS/BUSYBOX cat /nvme/apps/data/file.txt`, `/APPS/BUSYBOX ls /nvme/apps`, `/APPS/BUSYBOX ls -l /proc/self/exe`, `/APPS/BUSYBOX ls -l /proc/self/fd`, `/APPS/BUSYBOX ls -l /proc/self`, `/APPS/BUSYBOX sh`, `/APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'`, `/APPS/BUSYBOX sh -c 'echo hello | cat'`, `/APPS/BUSYBOX sh -c 'busybox echo m25-path-search'`, `/APPS/BUSYBOX sh -c 'ls /usr/bin'`, `/APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'`, `/APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'`, `/APPS/SBECHO m28-sbase-direct`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'`, `/APPS/SBCAT /nvme/apps/data/file.txt`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c 'PATH=/usr/local/bin:/bin:/usr/bin; sbecho m30-path-pipe | sbcat'`, `/APPS/BUSYBOX sh -c 'sbecho m31-default-path | sbcat'`, `/APPS/BUSYBOX sh -c 'sbecho $USER:$PWD | sbcat'`, `/APPS/BUSYBOX sh -c 'cd /nvme/apps; sbecho $PWD | sbcat'`, `/APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator /nvme/apps/sbenv | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator sbenv | /nvme/apps/sbcat'`, `/APPS/BUSYBOX sh -c 'sbenv USER=operator sbenv | sbcat'`, `/APPS/BUSYBOX sh -c 'ls /usr/local/bin'`, `/APPS/BUSYBOX sh -c '/usr/local/bin/sbenv USER=operator /usr/local/bin/sbenv | /usr/local/bin/sbcat'`, `/APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt | /usr/local/bin/sbcat'`, `/APPS/BUSYBOX sh -c 'cd /nvme/apps; /usr/local/bin/sbcat data/file.txt | /usr/local/bin/sbcat'`, and `/APPS/BUSYBOX sh -c 'cd /nvme/apps/data; /usr/local/bin/sbcat ../data/file.txt | /usr/local/bin/sbcat'`
- UEFI-only per-process page tables for Linux persona launches, backed by a fixed 4-root static PML4 pool with 19 pages per root, private lower-half user/VMA tables, shared higher-half kernel/MMIO mappings, scheduler CR3 switches at task start/swap/exit, and a transitional low-identity compatibility mapping reported as `low-compat 1`
- UEFI-only bounded Linux `fork(57)` plus blocking `wait4(61)` for the observed BusyBox ash external-command path, with parent/child PML4 slots allocated from the M22 root pool, full VMA/anonymous-page copy, duplicated FD/persona/VFS/audit state, child scheduler task registration, parent wait/reap, and final `pml4-pool-used-final 0`
- UEFI-only Linux `pipe(22)` over the fixed pipe provider, with exact-fd pipe endpoint inheritance across fork and final `pipe-live-final 0`; the M24 BusyBox ash proof forks both pipeline sides, so the accepted proof shape is `fork 2` and `fd-fork-pipe-copy 3`
- UEFI-only Linux VFS BusyBox applet aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin`, backed by the staged NVMe FAT `/APPS/BUSYBOX` artifact through the Linux-visible `/nvme/apps/busybox` path; `/usr` exposes fixed `bin` and `sbin` directory entries, and the real-binary summary reports `vfs-bin-*` telemetry for alias resolution
- UEFI-only Linux VFS third-party utility aliases under `/usr/local/bin`, currently `sbecho`, `sbcat`, and `sbenv`, backed by staged NVMe FAT `/APPS/SBECHO`, `/APPS/SBCAT`, and `/APPS/SBENV` through Linux-visible `/nvme/apps/sbecho`, `/nvme/apps/sbcat`, and `/nvme/apps/sbenv`; `/usr/local` exposes fixed directory entries, and the real-binary summary reports `vfs-localbin-*` telemetry for alias resolution
- UEFI-only default initial Linux environment for real launches with `PATH=/usr/local/bin:/bin:/usr/bin`, `HOME=/`, `USER=limitless`, and `PWD=/`, reported as `envc 4`; forked child `execve` calls inherit it and report `execve-last-envc 4`
- UEFI-only optional extra-app staging in `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1`, currently proven with `/APPS/SMOKE` staged from `external\build\zig-musl-smoke-imagebase` as a second static ET_EXEC artifact distinct from the BusyBox alias backend, with `/APPS/SBECHO` staged from a third-party suckless sbase 0.1 source build, with simultaneous `/APPS/SBECHO` plus `/APPS/SBCAT` staging through the second extra-app slot, and with simultaneous `/APPS/SBECHO`, `/APPS/SBENV`, and `/APPS/SBCAT` staging through the third extra-app slot
- UEFI-only scheduler FS-base save/restore/set state for Linux tasks, so `arch_prctl(ARCH_SET_FS)` TLS bases are task-local across CR3 switches, forked child exec, wait, and pipe scheduling
- exactly one authenticated local console session with input, display, filesystem, network-status, restricted socket-status, and installer-dry-run authority scoped to that session
- brokered persistent file workflow
- NVMe persistence verification path
- UEFI-only Ed25519 package archive and payload admission
- UEFI-only signed update-index verification with rollback denial
- read-only package trust visibility through Settings and `pkginfo`
- read-only hardware validation visibility through `hwval`
- read-only installer UX planning through the Installer app, Settings status, `pkginfo`, and verifier telemetry
- read-only identity/vault status through Settings
- read-only signed identity-provider descriptor and transport-safety status through Settings and `pkginfo`
- read-only account association status through Settings and `pkginfo`
- read-only cloud-storage broker and cloud-provider descriptor status through Settings, File Manager, and `pkginfo`
- read-only AI policy request/deny/audit status through Settings and `pkginfo`
- Assistant GUI app with consent-scoped read-only context flow and inference unavailable status
- capability denial checks
- no ambient authority

M10 login/authentication behavior:

- implementation: UEFI Product kernel and service layer only
- BIOS fallback: no login gate; runtime help labels UEFI login/session lock as unavailable
- local user store: `/USERDB.TXT` in the brokered persistent NVMe namespace
- password hash: bcrypt `$2b$` cost 04 using the public-domain `crypt_blowfish` verifier path
- first run: creates one local user record before the login screen
- login screen authority: brokered input plus compositor-owned display only
- pre-auth desktop/terminal/launcher access: blocked
- wrong password: denied visibly
- rate limit: three consecutive failures trigger a displayed 30-second lockout
- session authority: granted only after authentication and scoped to the local console session
- lock/unlock: preserves the session and windows, then resumes after the correct password
- unavailable: multiuser account management, password-change UI, PAM/LDAP, and remote auth

M11 identity/vault behavior:

- implementation: UEFI Product identity/status service and Settings surface; BIOS fallback remains lean and truthfully omits UEFI-only identity services
- active account type: local
- supported account types in the model: local, personal, enterprise
- unavailable account types: personal and enterprise
- local association: active and bound to the authenticated local console session
- cloud account association: planned/unavailable
- cloud storage: planned/unavailable
- security-key login: planned/unavailable
- remote login: unavailable
- secret vault status: foundation metadata only
- encrypted-at-rest secret storage: unavailable/non-product in M11
- real secrets/tokens stored: false
- Settings identity panel: read-only status only
- shell identity command: not added in M11; Settings is the Product identity status surface
- denials: identity mutation without authority, secret read without vault authority, secret write without vault authority, token storage, cloud association, personal login, enterprise login, ambient identity, and ambient secret authority

M12 identity transport behavior:

- implementation: UEFI Product identity transport broker state and Settings/pkginfo read-only surfaces; BIOS fallback remains lean and does not expose UEFI-only identity transport as Product behavior
- mode: Mode B endpoint trust foundation only
- provider descriptor: signed deterministic local fixture, no public internet dependency
- descriptor fields: provider id/type/display name, descriptor/protocol version, fixture endpoint, endpoint key id/fingerprint, supported auth methods, required transport security, token persistence policy, minimum OS version, sequence/generation, trusted-time/expiry metadata, signer key id, and signature
- trusted endpoint status: descriptor-verified fixture endpoint only
- encrypted identity transport: unavailable/non-product
- plaintext credential transport: denied
- credential transport to unverified endpoints: denied
- token storage: denied while vault remains Mode B
- account association: planned/unavailable until M13
- trusted time: unavailable/non-product; descriptor expiry metadata is not Product-enforced without trusted time
- denials: missing signature, invalid signature, wrong key, tamper, rollback, unsupported descriptor version, missing identity-network authority, plaintext credential transport, unverified endpoint credential transport, token storage, personal login, enterprise login, cloud association, ambient network, ambient identity, and ambient secret authority

M13 account association behavior:

- implementation: UEFI Product account association state and Settings/pkginfo read-only surfaces; BIOS fallback remains lean and reports account association unavailable
- mode: Mode B association policy/status only
- association records: local, personal, and enterprise
- local association: active/offline-capable and bound to the authenticated local console user
- personal association: planned/unavailable
- enterprise association: planned/unavailable
- cloud association: planned/unavailable
- security-key login: planned/unavailable
- encrypted identity transport: unavailable/non-product
- credential transport: denied
- token storage: denied while the vault remains Mode B
- enterprise policy: planned/unavailable
- remote account authority: none
- Settings account panel: read-only status only
- shell account status: `pkginfo` read-only text; no mutation commands
- denials: account mutation without authority, account unlink without authority, credential transport, token storage, cloud association, enterprise policy, remote ambient authority, ambient account identity, ambient account network, and ambient account secret authority

M14 cloud-storage broker behavior:

- implementation: UEFI Product cloud-storage broker state and read-only Settings/File Manager/pkginfo surfaces; BIOS fallback remains lean and reports cloud storage unavailable
- broker mode: Product foundation active, policy/status only
- provider descriptor: signed deterministic local fixture, no public cloud dependency
- descriptor fields: provider id/type/display name, descriptor/protocol version, fixture endpoint, endpoint key id/fingerprint, supported modes, token policy, offline-cache policy, sync policy, required transport security, required account association, minimum OS version, sequence/generation, trusted-time/expiry metadata, signer key id, and signature
- cloud account status: unavailable/planned
- cloud association status: unavailable/planned
- token storage: denied while vault remains Mode B
- encrypted cloud transport: unavailable/non-product
- sync: unavailable
- upload/download: denied
- automatic upload/download: unavailable
- offline cache: planned/unavailable
- AI cloud access: unavailable
- File Manager cloud surface: read-only unavailable-status area only; no cloud namespace is exposed
- denials: missing signature, invalid signature, wrong key, tamper, rollback, unsupported descriptor version, malformed descriptor, upload, download, sync, Settings mutation, File Manager mutation, app direct cloud authority, AI cloud access, ambient cloud, ambient filesystem, ambient network, ambient identity, and ambient secret authority

M7.1 package behavior:

- implementation: UEFI Product kernel only; BIOS Product remains checksum-only fallback
- package signing: Ed25519 detached signatures over the generated bootstrap archive and payload records
- trusted key: public verification key embedded in the UEFI Product kernel
- private key: generated during build signing only, not committed, not emitted into Product artifacts, and scanned out of kernel/ISO/image artifacts
- admission checks: signed archive, signed payload, checksum match, trusted signing key, manifest integrity, payload integrity, manifest version, package identity/version policy, scoped install token, owner, stale token, and capability policy
- denial telemetry: missing signature, invalid signature, wrong key, manifest tamper, payload tamper, checksum mismatch, unsupported manifest version, duplicate package, downgrade, wrong owner, stale token, denied capability request, malformed package, oversized package, install without install capability, rollback index, replay handling, and no ambient install/update
- update check: signed local update-index fixture verified through Product telemetry; unsigned, tampered, wrong-key, rollback, and replay fixtures are denied or handled deterministically
- live public update fetching: unavailable/non-product
- auto-install: unavailable
- package-manager GUI: unavailable
- trusted time: unavailable; expiry metadata is not Product-enforced without a trusted time source

M8 package trust UX behavior:

- Settings shows a read-only package trust panel
- `pkginfo` shows package mode, format version, trusted public key id/fingerprint, signed package count, signature/hash status, capability request/admission/denial status, local signed update-index status, rollback/replay handling, no-auto-install status, public update fetch status, trusted-time expiry status, and install/update authority status
- Settings and `pkginfo` do not receive install capability, update-apply capability, filesystem write authority, raw network authority, or raw package mutation authority
- install/apply actions are unavailable in M8
- live public update fetching remains unavailable/non-product
- trusted-time expiry remains unavailable/non-product without a trusted time source

M9 real-hardware validation:

- MSI Cyborg 15 A13VE checklist: `docs/hardware/msi-cyborg-15-a13ve.md`
- status: pending user-supplied physical results
- required boot mode: UEFI USB
- unsafe internal partitions must not be browsed or written
- internal NVMe writes remain disabled unless a later safe installer path explicitly enables them through scoped authority
- Product command: `hwval`
- `hwval` is read-only and reports boot path, framebuffer status, input backends, xHCI, PS/2 fallback, APIC, PCI/ECAM, NVMe, AHCI, network, package trust, installer dry-run status, and disabled install/write/format/NVRAM state
- MSI dry-run parser: `tools\parse-msi-dryrun-evidence.ps1`
- parser output records detected disks, GPT partitions, type GUIDs, labels, filesystem signatures, forbidden partitions, LimitlessOS target candidates, write-disabled status, dry-run no-write status, and recommended next step

M5 installer behavior:

- implementation: `tools\limitless-installer.ps1`
- fixtures: `tools\generate-installer-fixtures.ps1`
- verifier: `tools\verify-installer-m5.ps1`
- safety doc: `docs/installer/m5-safe-installer.md`
- dry-run lists disks/images, GPT partitions, type GUIDs, labels, filesystem signatures, markers, classification, and zero-write plan
- explicit physical dry-run path: `-PhysicalDriveNumber N -AllowPhysicalReadOnly -Mode DryRun`
- install/write mode is image-fixture-only in M5
- Windows ESP, NTFS, MSR, Recovery, unknown FAT32, and unknown GPT targets are refused
- writes require hardware inventory, read-only block, write, and format capability flags plus a confirmation token
- boot-entry changes require a separate boot-entry capability and are not performed by the default M5 path
- real MSI internal NVMe writes remain disabled by default until dry-run output is reviewed

M6 service/session behavior:

- design note: `docs/service-session-m6.md`
- service lifecycle states are modeled as declared, admitted, launching, running, degraded, stopping, stopped, crashed, restarting, and denied
- Product services are policy/security broker, console/shell broker, input broker, display/compositor, window manager/desktop shell, filesystem broker, block/storage broker, hardware inventory broker, network broker, brokered socket foundation, native app SDK foundation, installer dry-run service/tool, settings/system-info provider, cloud-storage broker foundation, and installer UX planning
- Settings is the Product-safe status surface for service/session information
- controlled restart verification is limited to the scoped settings/system-info provider path
- stale capabilities from the old generation are denied
- M6 introduced one local console session; M10 authenticates that single local console user but still does not implement full multiuser account management
- M11 associates the authenticated session with a local identity record and vault metadata foundation without adding remote identity, personal account login, enterprise account login, cloud storage, or token storage
- M12 adds signed local identity-provider descriptor verification and read-only identity transport status without adding remote login, account association, encrypted credential transport, or token persistence
- M13 records local account association as active and labels personal, enterprise, cloud, security-key, credential, token, and enterprise-policy paths unavailable without adding account linking or remote login
- M14 records cloud-storage policy state and labels real cloud storage, sync, upload/download, token storage, encrypted transport, offline cache, AI cloud access, and app-direct cloud authority unavailable or denied
- M15 records installer planning state and labels real internal install/write, formatting, boot-entry changes, package install/apply, cloud enablement, and AI-assisted setup unavailable or denied
- wrong-session input, display, and filesystem delivery are denied
- raw input, direct framebuffer, ambient filesystem, and ambient network access remain denied

Product apps:

- APPEND
- CAT
- COPY
- DELETE
- LS
- MKDIR
- MOVE
- NETHELLO
- RENAME
- STAT
- TOUCH
- WRITE

Product builtins:

- apps
- help
- hwval
- info
- lock (UEFI Product only; BIOS fallback reports unavailable)
- net
- pkginfo
- pwd

Product GUI apps:

- Terminal
- File Manager
- Settings
- Installer
- Assistant

Product GUI authority model:

- raw keyboard and mouse enter only through the brokered input path
- input is routed only to the active local console session
- the window manager owns hit-testing, focus, drag, close, taskbar focus, launcher dispatch, and z-order
- the compositor owns physical framebuffer presentation
- Terminal receives keyboard only while focused, with unfocused terminal delivery denied
- apps draw only through delegated window surfaces in their session
- File Manager is limited to RAMFS, boot-media read-only areas, and explicitly brokered persistent namespaces
- Settings receives read-only display/input/network/storage/profile/boot metadata and cannot write configuration
- GUI apps do not receive ambient framebuffer, raw input, filesystem, storage, or network authority

Unavailable or not product-path in Product:

- ASK, not AI and no consent-gated assistant path
- ECHO
- shell aliases SAY, SHOW, LIST, MAKE, PUT, SWAP, SHIFT
- real internal install/write authority
- formatting authority
- boot-entry authority
- package install/update actions
- app store
- auto-install
- live public update fetch
- general socket library
- server sockets
- raw packet API
- arbitrary app network send/receive
- multiuser account management UI
- password change UI
- PAM/LDAP/remote auth
- AI inference backend
- AI-generated answers
- AI model transport
- personal account login
- enterprise account login
- account linking
- cloud account association
- real public cloud storage
- cloud sync
- automatic cloud upload/download
- offline cloud cache
- AI cloud access
- app-direct cloud authority
- security-key login
- remote login
- encrypted-at-rest secret storage
- token storage
- encrypted identity transport
- credential transport
- enterprise policy enrollment

## Experimental Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental
```

Current Experimental artifact:

- produced by `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental`
- recorded in `dist\limitlessos-x86_64.experimental.m6.json` when the Experimental profile is built
- not Product and not a daily-driver surface

Experimental may initialize proof surfaces beyond the M4 Product GUI and broad hardware proof telemetry. These are not Product behavior and must remain labeled experimental or proof-only in logs and reports.

## Verification

Final M2 evidence pack:

- `dist/m2-evidence-20260511-151717/m2-evidence.json`

The pack records command, exit code, elapsed time, output file, build profile, kernel bytes/sectors/reserve/checksum, final ISO path, and artifact inventory JSON for:

- Experimental build
- Experimental UEFI verification
- Product build
- Product M1/M2 production-slice assertion
- Product disk verification
- Product UEFI verification
- Product ISO verification
- Product UEFI e1000e verification
- Product ISO e1000e verification
- Product NVMe persistence verification

All recorded commands exited 0.

M3 evidence pack:

- `dist/m3-evidence-20260511-163350/m3-evidence.json`
- created by `.\tools\archive-m3-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M3 artifact inventory JSON
- all recorded commands exited 0

M4 evidence pack:

- `dist/m4-evidence-20260511-190105/m4-evidence.json`
- generated by `.\tools\archive-m4-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence commands
- records a dedicated Product GUI interactive verification command
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M4 artifact inventory JSON
- all recorded commands exited 0

M4.1 evidence pack:

- `dist/m4-1-evidence-20260511-192857/m4-1-evidence.json`
- generated by `.\tools\archive-m4-1-evidence.ps1`
- archives the accepted M4 verifier outputs instead of rerunning M4
- archives the MSI Cyborg 15 A13VE checklist template
- records current kernel budget and git status
- records physical validation as pending until the user supplies real-hardware results

M5 installer verification:

- evidence pack: `dist/m5-evidence-20260511-194236/m5-evidence.json`
- command: `.\tools\verify-installer-m5.ps1`
- proves dry-run no-writes
- proves Windows-like ESP/MSR/NTFS/Recovery refusal
- proves unknown FAT32 and unknown GPT refusal
- proves dedicated LimitlessOS target acceptance
- proves scoped write/format capability and confirmation-token requirements
- proves boot-entry authority separation
- proves valid fixture install verifies markers/manifests and leaves forbidden partitions unchanged
- all installer verifier predicates must pass before M5 is accepted

M6 evidence pack:

- generated by `.\tools\archive-m6-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence/gui/installer commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M6 artifact inventory JSON and service/session status output
- records git status and current commit at archive time

M7 evidence pack:

- generated by `.\tools\archive-m7-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence/gui/installer/service-session/signing commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M7 artifact inventory JSON, split-kernel budget, package fixture status, private-key artifact scan status, and git status

M7.1 evidence pack:

- generated by `.\tools\archive-m7-1-evidence.ps1 -IncludeExperimental`
- records all preserved M7 commands plus dedicated Product package negative fixture verification, update-index negative fixture verification, and private-key artifact scan
- includes the final M7.1 artifact inventory JSON, split-kernel budget, deterministic fixture status, live public update fetching status, and git status
- live public update fetching remains unavailable/non-product and no auto-install path exists

M8 evidence pack:

- generated by `.\tools\archive-m8-evidence.ps1 -IncludeExperimental`
- records Product package UX/trust-surface verification plus all preserved M7.1 fixture checks
- includes read-only Settings and `pkginfo` package trust surfaces
- install/apply actions, auto-install, public update fetching, and trusted-time expiry remain unavailable

M9 evidence pack:

- generated by `.\tools\archive-m9-evidence.ps1 -IncludeExperimental`
- records Product hardware validation mode, MSI checklist, dry-run parser, and preserved M8 verification matrix
- physical MSI Cyborg 15 A13VE evidence remains pending user-provided results

M10 evidence pack:

- generated by `.\tools\archive-m10-evidence.ps1 -IncludeExperimental`
- records Product login gate, first-run setup, bcrypt `$2b$` user store, wrong-password denial, rate limiting, lock/unlock, and scoped session authority
- BIOS fallback labels login/session lock unavailable

M11 evidence pack:

- generated by `.\tools\archive-m11-evidence.ps1 -IncludeExperimental`
- records Product identity/vault foundation verification plus all preserved M10 verification commands
- proves local account active, personal/enterprise unavailable, Settings identity panel, read-only identity status, identity mutation denial, vault foundation, denied secret read/write, no plaintext token storage, cloud association unavailable, and no ambient identity or secret authority

M12 evidence pack:

- generated by `.\tools\archive-m12-evidence.ps1 -IncludeExperimental`
- records Product identity transport verification plus all preserved M11 verification commands
- proves signed local provider descriptor acceptance, missing/invalid/wrong-key/tampered/rollback/unsupported descriptor denials, scoped network requirement, plaintext credential denial, unverified endpoint denial, token-storage denial while vault is Mode B, Settings identity transport panel visibility, read-only status, trusted-time status, and no ambient identity-transport network/identity/secret authority

M13 evidence pack:

- generated by `.\tools\archive-m13-evidence.ps1 -IncludeExperimental`
- records Product account association verification plus all preserved M12 verification commands
- proves local association active/offline-capable, personal/enterprise/cloud/security-key unavailable, credential transport denied, token storage denied while vault remains Mode B, enterprise policy unavailable, account mutation/unlink denied, remote account no-ambient-authority denial, Settings/pkginfo read-only status, and no ambient account identity/network/secret authority

M14 evidence pack:

- generated by `.\tools\archive-m14-evidence.ps1 -IncludeExperimental`
- records Product cloud-storage broker verification plus all preserved M13 verification commands
- proves signed local cloud-provider descriptor acceptance, missing/invalid/wrong-key/tampered/rollback/unsupported/malformed descriptor denials, Settings/File Manager/pkginfo read-only status, upload/download/sync denial, automatic upload/download unavailability, AI cloud access unavailability, app-direct cloud authority denial, and no ambient cloud/filesystem/network/identity/secret authority

M15 evidence pack:

- generated by `.\tools\archive-m15-evidence.ps1 -IncludeExperimental`
- records Product installer UX verification plus all preserved M14 verification commands
- proves Installer GUI entry, welcome/beginner/advanced/hardware/recommendation/component/account/cloud/AI/plan/dry-run surfaces, zero planned writes/formats/boot-entry/package operations, forbidden target denial, write/format/boot-entry/package/cloud/AI action denial, and no ambient installer/storage/firmware/package/identity-cloud-secret authority

M16 evidence pack:

- generated by `.\tools\archive-m16-evidence.ps1 -IncludeExperimental`
- records Product AI policy verification plus all preserved M15 verification commands
- proves AI principal creation, action request modeling, consent required, no-consent denial, scope validation, invalid-scope denial, immutable/queryable audit visibility, Settings/pkginfo read-only status, zero AI actions executed, zero default AI capabilities, and no ambient AI/filesystem/network/settings/package/secret/cloud authority

M17 evidence pack:

- generated by `.\tools\archive-m17-evidence.ps1 -IncludeExperimental`
- records Product AI Assistant read-only verification plus all preserved M16 verification commands
- proves Assistant app opens after login, pre-auth access is blocked, backend mode is Mode B, default AI capabilities remain zero, context requests require consent, denied requests receive no data, allowed requests receive only scoped read-only status context, invalid/broad filesystem/secret/cloud scopes are denied, file write/settings/package mutation and network/model access are denied or unavailable, stale and wrong-session grants are denied, audit is queryable, Settings shows the AI panel, actions/automation/cloud memory remain unavailable, Assistant self-modification is denied, package integrity is checked, no model call occurs, and no scripted answer is presented as AI output

M18 evidence pack:

- generated by `.\tools\archive-m18-evidence.ps1 -IncludeExperimental`
- records Product AI consent-scoped action verification plus all preserved M17 verification commands
- proves the Mode B action broker creates predefined action requests, requires explicit consent, grants only scoped action-bound/session-bound capabilities, writes and commits `/HOME/ASSIST/NOTE.TXT`, verifies readback, runs installer dry-run with no writes, opens read-only Settings/package status surfaces, denies arbitrary/path-traversal writes, stale grants, wrong-session grants, settings mutation, package install/update, cloud enablement, secret access, self-modification, model calls, generated-answer behavior, autonomy, and ambient AI filesystem/installer/settings/package/cloud/secret/network authority

M18.1 evidence pack:

- generated by `.\tools\archive-m18-1-evidence.ps1 -IncludeExperimental`
- records the UEFI loader compatibility closure plus all preserved M18 verification commands
- proves QEMU UEFI and ISO still reach ExitBootServices, x64 kernel entry, PIT, login shell, and M18 telemetry with dynamic linked-kernel and boot-handoff proof predicates
- records that automated VirtualBox verification is unavailable in this workspace when `VBoxManage` is absent
- includes the manual VirtualBox EFI checklist at `docs/hardware/virtualbox-uefi-m18-1.md`
- keeps MSI physical validation pending user evidence

M19 brokered socket verification:

- generated by `.\tools\verify-network-socket-m19.ps1`
- records Product UEFI network verification with virtio-net by default, or e1000e/e1000 when requested
- proves the network service endpoint is registered, a network service capability is required and minted only through the broker, no-cap and wrong-owner socket opens are denied, raw sockets and listen sockets are denied, send without broker data-plane authority is denied, one TCP-client status handle can be represented over the existing DHCP/DNS/HTTP path, recv-status reports the broker-owned HTTP status/byte count, close retires the handle, no socket remains open after verification, and filesystem/storage/ambient network authority stay zero

M21 native app SDK verification:

- generated by `.\tools\verify-native-app-sdk-m21.ps1`
- reads `apps/native/nethello.json`, assembles the declared native app binary, generates `/APPS/NETHELLO.APP`, stages `/APPS/NETHELLO.BIN`, and signs/verifies payload slot 13 through package metadata
- validates descriptor metadata for `name`, `binary`, `payload-slot`, `entry-result`, `success-result`, and `capabilities`, maps the loaded binary into Ring-3 user memory, and runs it through the native user-entry path
- proves the app can print through delegated console authority, request the declared network service capability, use the M19 brokered socket status path through syscall, observe send denial, close the socket, receive denial for undeclared filesystem/block capabilities, and finish with `drs-app-m21`

Real static Linux ELF gate verification:

- generated by `.\tools\verify-real-binary-gate.ps1 -BusyBoxPath <external-busybox> -BusyBoxSource <source> -BusyBoxVersion <version>`; use `-RequireLowAddressNegative` when the upstream-default-address BusyBox artifact is present and that denial proof must be mandatory, use `-RequireShellCwdLoop` to require BusyBox ash cwd/path traversal proof, use `-RequireRelativePathProof` to require relative `.`/`..` path canonicalization proof, use `-RequireProcSymlinkProof` to require `/proc/self/exe` readlink proof, use `-RequireProcFdProof` to require stable `/proc/self/fd` link proof, use `-RequireProcSelfProof` to require mixed `/proc/self` pseudo-file, directory, and symlink listing proof, and use direct `verify-qemu.ps1 -RealBinaryGate -ExtraShellLine` runs for the M23 fork/wait shell-external-command, M24 pipeline, M25 Linux VFS path-execution, and M26 forked-child exec inheritance acceptance paths
- stages the external binary into the generated NVMe FAT image at `/APPS/BUSYBOX` without committing or embedding the binary in the repository
- records source, version, SHA-256, byte count, `file`, `readelf -h`, `readelf -l`, staged path, command, console output, exit code, the `/proc/meminfo` cat proof, the `/nvme/apps/data/file.txt` NVMe FAT cat proof, optional `nvme/apps/./data/../data/file.txt` relative cat proof, optional `ls -l nvme/apps/./data/..` relative metadata/listing proof, optional `ls -l /proc/self/exe` proc symlink proof, optional `ls -l /proc/self/fd` proc-fd proof, optional `ls -l /proc/self` mixed proc listing proof, the `/nvme/apps` `ls` proof, the `sh` banner/prompt proof, optional cwd-loop proof, M23 direct fork/wait shell telemetry, M24 pipe telemetry, M25 execve/VFS-alias telemetry, M26 forked-child exec inheritance telemetry, and `drs-realbin` telemetry in `build\real-binary-gate-provenance.txt`
- passing artifact 1: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, SHA-256 `5CDE8968EB2FEDB62DEA27947CD269BC57AD9C8B142ABFF0C3B1514A0238E8D9`; verified with `echo limitless-real-binary`, `cat /proc/meminfo`, and `cat /nvme/apps/data/file.txt`
- passing artifact 2: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-ls`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, and `sh`, SHA-256 `299DE064F51DA04DE99227F26F2EAB60C95F400C1B83731E14E3E28F86695652`; verified with `ls /nvme/apps` and `sh`
- passing artifact 3: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, `sh`, `FEATURE_PREFER_APPLETS`, `FEATURE_SH_STANDALONE`, `FEATURE_SH_NOFORK`, and a gate-local `cat` NOEXEC applet patch, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`; verified with the default real-binary gate including `echo`, `cat`, `ls`, the bounded `sh` builtin loop, the M22 CR3 isolation command, the M23 fork/wait external-command shell path, the M24 pipeline path, the M25 Linux VFS path-execution path, and the M26 forked-child exec inheritance path
- proves the UEFI Product shell commands `linux /APPS/BUSYBOX echo limitless-real-binary`, `linux /APPS/BUSYBOX cat /proc/meminfo`, `linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt`, `linux /APPS/BUSYBOX ls /nvme/apps`, `linux /APPS/BUSYBOX ls -l /proc/self/exe`, `linux /APPS/BUSYBOX ls -l /proc/self/fd`, `linux /APPS/BUSYBOX ls -l /proc/self`, `linux /APPS/BUSYBOX sh`, `linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'`, `linux /APPS/BUSYBOX sh -c 'echo hello | cat'`, `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'`, `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'`, and `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'` load the ELF from NVMe, reject `PT_INTERP`/dynamic inputs, map nonzero ELF pages, allocate the 64 KiB real-launch stack, start scheduler tasks, print through the brokered console, read a Linux VFS proc file with EOF behavior, read the nested NVMe FAT fixture through the broker-scoped Linux VFS `/nvme` provider, enumerate `/nvme/apps`, `/nvme/apps/data`, `/proc/self/fd`, mixed `/proc/self` entries, and fixed Linux-visible BusyBox applet directories through `getdents64`, read proc symlink targets through `readlink(2)`, answer terminal-size probes through `TIOCGWINSZ`, consume bounded brokered stdin for `sh`, cross the BusyBox ash fork/wait shell external-command path, create and drain a Unix pipe across two forked pipeline children, execute real ET_EXEC images through Linux VFS PATH aliases, preserve cwd/VFS/pipe state across forked child execs, observe `exit_group`, and clean up VMA/FD/persona/audit/process/PML4/pipe state
- proven syscall surface for the passing artifacts: Linux process startup through `brk`, `mmap`, `mprotect`, `munmap`, `arch_prctl`, `set_tid_address`, `rt_sigaction`, and `rt_sigprocmask`; console and file I/O through `read`, `write`, `readv`, `writev`, `openat`, `close`; stat-family metadata through `stat`, `newfstatat` including `AT_EMPTY_PATH`, and `lstat`; proc symlink readback through `readlink`; directory enumeration through `getdents64`; current-working-directory state through `getcwd` and `chdir`; cwd-relative path canonicalization for `open`, `openat`, stat-family paths, and `chdir`, including `.` and `..`; terminal query `ioctl(TIOCGWINSZ)` for brokered terminal fds; fixed identity shims `geteuid` and `getppid`; BusyBox process-name shim `prctl(PR_GET_NAME)`/`PR_SET_NAME` with other `prctl` options still returning `ENOSYS`; bounded process creation and reaping through `fork(57)` and `wait4(61)` for the observed BusyBox ash external-command and pipeline paths; Unix pipe creation through `pipe(22)` with exact-fd endpoint inheritance across fork; real static ET_EXEC replacement through `execve` on Linux VFS paths backed by the staged NVMe BusyBox artifact; process termination through `exit_group`; unsupported syscall telemetry remains visible
- positive echo telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 239 stack 16 task 0 started 1 console-bytes 22 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0`
- positive NVMe file telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 239 stack 16 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 27`
- positive relative NVMe file telemetry from `-RequireRelativePathProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 27 exit 0 cleanup 1 ... path-relative 1 path-dot 1 path-dotdot 1 path-fault 0 ... read 2 read-bytes 27 write 1 write-bytes 27 ... vfs-nvme-reads 2 vfs-nvme-bytes 27`; visible output was `Nested FAT32 path fixture`
- positive relative NVMe directory metadata telemetry from `-RequireRelativePathProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 65 exit 0 cleanup 1 getdents64 2 getdents64-entries 2 getdents64-bytes 56 stat 3 stat-denial 0 stat-fault 0 ... path-relative 4 path-dot 4 path-dotdot 4 path-fault 0 ... writev 2 writev-bytes 65 ioctl 1 ioctl-tty 1 ... vfs-nvme-readdirs 4 vfs-nvme-dirents 2`; visible output from `ls -l nvme/apps/./data/..` was `-r--r--r--    1    145264 busybox` and `dr-xr-xr-x    2         0 data`
- positive proc symlink telemetry from `-RequireProcSymlinkProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 59 exit 0 cleanup 1 ... stat 1 stat-denial 0 stat-fault 0 ... readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 ... writev 1 writev-bytes 59 ... ioctl 1 ioctl-tty 1 ... unimplemented 0`; visible output from `ls -l /proc/self/exe` was `lrwxrwxrwx    1        14 /proc/self/exe -> /proc/self/exe`
- positive proc-fd telemetry from `-RequireProcFdProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 129 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 72 stat 4 stat-denial 0 stat-fault 0 ... readlink 3 readlink-bytes 33 readlink-denial 0 readlink-fault 0 readlink-last-result 11 ... writev 3 writev-bytes 129 ... unimplemented 0`; visible output from `ls -l /proc/self/fd` was `lrwxrwxrwx    1        11 2 -> anon:[fd 2]`, `lrwxrwxrwx    1        11 1 -> anon:[fd 1]`, and `lrwxrwxrwx    1        11 0 -> anon:[fd 0]`. The bounded proc-fd enumerator skips its own directory iterator fd so it does not publish a link that can disappear before BusyBox's later `readlink` pass.
- positive proc-self telemetry from `-RequireProcSelfProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 209 exit 0 cleanup 1 getdents64 2 getdents64-entries 6 getdents64-bytes 168 stat 7 stat-denial 0 stat-fault 0 ... readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 ... writev 6 writev-bytes 209 ... unimplemented 0`; visible output from `ls -l /proc/self` was `-r--r--r--    1         0 environ`, `-r--r--r--    1         0 cmdline`, `-r--r--r--    1        70 status`, `dr-xr-xr-x    2         0 fd`, `lrwxrwxrwx    1        14 exe -> /proc/self/exe`, and `-r--r--r--    1       384 maps`.
- positive NVMe directory telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 37 stack 16 task 0 started 1 console-bytes 14 exit 0 cleanup 1 getdents64 2 getdents64-entries 2 getdents64-bytes 56 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 4 vfs-nvme-dirents 2 vfs-nvme-bytes 0`; visible console output was `busybox` and `data`
- positive shell telemetry: `drs-realbin path /APPS/BUSYBOX ... console-bytes 90 exit 0 cleanup 1 ... read 40 read-bytes 40 write 2 write-bytes 20 readv 0 readv-bytes 0 writev 6 writev-bytes 70 ... geteuid 1 getppid 1 ioctl 3 ioctl-tty 3 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 ...`; visible console output includes `BusyBox v1.35.0 ... built-in shell (ash)`, `$ shellloop`, `$ $ aftertrue`, and the final `$` prompt
- positive cwd-loop shell telemetry from `-RequireShellCwdLoop`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 101 exit 0 cleanup 1 ... getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 chdir 2 fchdir 0 chdir-denial 0 chdir-fault 0 ... fork 0 fork-enosys 0 fork-denial 0 ...`; visible console output includes `$ shellloop`, `$ /`, `$ $ /nvme/apps`, and `$ $ /`.
- M23 shell external-command proof: `linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'` now crosses the previously traced BusyBox ash fork boundary at `fork-last-rip 0x000000005200EF74`. Visible output contains `busybox  data` and `file.txt`; telemetry reports `fork 1 fork-success 1 fork-enosys 0 fork-child-slot 1 fork-child-root-distinct 1 wait4 1 wait4-reap 1 wait4-last-exit-code 0 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 console-bytes 23 exit 0 page-faults 0 cleanup 1`.
- M23 implementation delta: the kernel/header portion is 19 files changed with 2,112 insertions and 33 deletions; the verifier semicolon injector prerequisite adds 1 line, for a total worktree delta of 20 files changed with 2,113 insertions and 33 deletions.
- M24 closure: the earlier `syscall-root-repair 1` watch item was fixed before the M24 gate and now remains `syscall-root-repair 0`; the accepted pipeline run exercises child-side pipe reads with `read 2 read-bytes 6`.
- M25 path-execution proof: `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'` prints `m25-path-search`, exits 0, and reports `execve 1 execve-denial 0 execve-fault 0 execve-last-binary-bytes 145264 execve-last-transfer-ready 1 vfs-bin-alias 3 vfs-bin-read 1 vfs-bin-denial 0 stat-denial 0 page-faults 0 syscall-root-repair 0 pml4-pool-used-final 0`.
- M25 applet-directory proof: `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'` prints `sh       true     ls       cat      echo     busybox`, exits 0, and reports `getdents64 2 getdents64-entries 6 getdents64-bytes 152 stat 7 stat-denial 0 writev 1 writev-bytes 53 vfs-bin-alias 6 vfs-bin-denial 0 page-faults 0 cleanup 1`.
- M26 forked-child exec inheritance proof: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'` prints `Nested FAT32 path fixture`, exits 0, and reports `chdir 1 path-relative 1 pipe 1 pipe-create 1 fork 2 fork-success 2 execve 2 execve-denial 0 execve-fault 0 wait4 2 wait4-reap 2 read 4 read-bytes 54 write 2 write-bytes 54 pipe-live-final 0 pml4-pool-used-final 0 page-faults 0 syscall-root-repair 0`.
- known real-binary gap: terminal support is intentionally narrow. `ioctl(TIOCGWINSZ)` succeeds for brokered terminal fds, while other terminal queries return `ENOTTY` and unrelated ioctl requests return `ENOSYS`; there is still no broad device-control API.
- `drs-realbin` NVMe VFS telemetry is launch-local: in a same-boot run, `linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt` reports `vfs-nvme-reads 2 ... vfs-nvme-bytes 27`, and a following `linux /APPS/BUSYBOX echo limitless-real-binary` reports `vfs-nvme-reads 0 ... vfs-nvme-bytes 0`
- read-stage failures now include concrete FAT read diagnostics; a missing `/APPS/BUSYBOX` reports `stage read code 5 ... nvme-read-error 5 nvme-read-bytes 0 nvme-read-capacity 4194304 nvme-read-size 0 nvme-read-attr 0x0000000000000000`
- negative coverage: missing `/APPS/BUSYBOX`, dynamic `PT_INTERP`, real upstream-default-address ET_EXEC below the low kernel window, oversized host artifact, and BIOS unavailable path
- low-address ET_EXEC negative telemetry with the upstream BusyBox artifact: `drs-realbin-fail path /APPS/BUSYBOX stage static code 20 ... load-first 0x0000000000400000 load-end 0x0000000000713FDD low-kernel-limit 0x0000000001000000`
- BIOS reserve at gate crossing: 101 sectors, below the 128-sector warning threshold but still inside the hard 1024-sector BIOS loader limit
- x86_64 ISO boot shape: UEFI-only optical media with one UEFI El Torito boot entry. `BOOTIMG.IMG` is a staged UEFI FAT image copy for inspection and is not wired as a legacy BIOS El Torito boot entry.
- UEFI boot manifest integrity: `BOOTMAN.TXT` records `kernel-checksum-algorithm=fnv1a-32`, the loader-enforced `kernel-checksum`, standard external `kernel-sha256`, and `non-product-package-registry-stubs=ASK,ECHO`.

M22 per-process page table verification:

- generated by `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'"`
- visible console output contains `m22-cr3-isolation`, exits with status 0, reports zero page faults, and cleans up the process PML4 slot
- static pool design: 4 UEFI-only process roots, 19 pages per root, 4 x 19 x 4096 = 311,296 bytes reserved for the PML4/PDPT/PD/PT tables; each root has private lower-half user and VMA page tables plus copied shared higher-half kernel/MMIO mappings
- transitional compatibility status: `low-compat 1` remains visible in telemetry while the low identity PDPT mapping is retained for M22 compatibility; this is not a final isolation claim below the low window
- UEFI reserve delta for the M22 implementation: 852,544 bytes at the first successful pool-stage build to 848,416 bytes at final acceptance; BIOS reserve remains 101 sectors
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204A000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 low-compat 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 18 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 1 write-bytes 18 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 0 fork-enosys 0 fork-denial 0 fork-last-rip 0x0000000000000000 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M23 fork/wait verification:

- generated by `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'"`
- visible console output contains `busybox  data` and `file.txt`, exits with status 0, reports zero page faults, and cleans up both parent and child PML4 slots
- implementation scope: bounded UEFI-only `fork(57)` for the observed BusyBox ash path, full child VMA/address-space copy, duplicated FD/persona/VFS/audit state, child scheduler task registration, blocking `wait4(61)` parent reaping, and final root-pool cleanup; no vfork, no broad clone flags, no COW, no dynamic linker, no real threading, and no signal delivery
- implementation delta: 19 kernel/header files changed with 2,112 insertions and 33 deletions; the prerequisite QMP keyboard semicolon injector changed 1 verifier line, for 20 total changed files with 2,113 insertions and 33 deletions
- final artifact budget: UEFI reserve 840,160 bytes; BIOS reserve remains 101 sectors
- M24 resolved the M23 watch item: `syscall-root-repair` is now 0 in the accepted pipeline proof, and child-side data movement is exercised through inherited pipe fds rather than only parent-side console/file output.
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204C000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 1 syscall-root-reload 61 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 2 task 0 started 1 console-bytes 23 exit 0 cleanup 1 getdents64 4 getdents64-entries 3 getdents64-bytes 88 stat 5 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 readv 0 readv-bytes 0 writev 2 writev-bytes 23 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 4 ioctl-tty 4 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 4 prctl-set-name 3 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 1 fork-success 1 fork-enosys 0 fork-denial 0 fork-child-slot 1 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 1 wait4-reap 1 wait4-last-exit-code 0 child-root-cleanup 1 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 7 vfs-nvme-dirents 3 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M24 pipe verification:

- generated by `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'echo hello | cat'"`
- visible console output is exactly `hello`, exits with status 0, reports zero page faults, keeps `syscall-root-repair 0`, and cleans up the parent plus two BusyBox ash pipeline children
- implementation scope: Linux `pipe(22)` ABI exposure over the existing fixed pipe provider, pipe-aware exact-fd inheritance in `fd64_fork_process`, target-fd endpoint grants through `pipe64_grant_endpoint_at`, and real-binary telemetry for pipe creation, denials, faults, provider denials, live pipe count, and fork-time pipe fd copies; `pipe2(293)` observable behavior is unchanged
- fixed pipe pool dimensions remain unchanged: `PIPE64_MAX_OBJECTS 16` and `PIPE64_BUFFER_BYTES 4096`
- BusyBox ash forks both sides of this pipeline, so the accepted proof shape is `fork 2 fork-success 2` and `fd-fork-pipe-copy 3`, not the earlier simplified single-fork expectation
- final artifact budget: UEFI reserve 836,064 bytes; BIOS reserve remains 101 sectors
- implementation delta before docs: 10 files changed with 444 insertions and 17 deletions, including the QMP `|` injector, syscall 22 dispatch, pipe fork inheritance, pipe telemetry, and the external BusyBox standalone-shell build-script provenance update
- passing BusyBox artifact: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, static musl ET_EXEC linked at `0x52000000`
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204D000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 60 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 6 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 6 write 2 write-bytes 12 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M25 Linux VFS path execution verification:

- generated by direct `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraShellLine ...` runs for `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'`, `linux /APPS/BUSYBOX sh -c '/bin/cat /nvme/apps/data/file.txt'`, `linux /APPS/BUSYBOX sh -c '/bin/echo hello | /bin/cat'`, and `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'`
- implementation scope: UEFI-only `execve` widening through Linux VFS file reads, real external ET_EXEC transfer via a consume-once syscall return frame, 64 KiB real-exec stack selection for large real binaries, exec-replaced VMA/FD/audit cleanup handoff, BusyBox applet aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin`, fixed `/usr` directory entries, QEMU verifier log waiting across debug and framebuffer logs, and real-binary telemetry for `execve-*` plus `vfs-bin-*`
- visible output proofs include `m25-path-search`, `Nested FAT32 path fixture`, `hello`, and the `/usr/bin` entries `sh`, `true`, `ls`, `cat`, `echo`, and `busybox`
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- implementation delta before docs: 8 files changed with 751 insertions and 34 deletions
- passing BusyBox artifact: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, static musl ET_EXEC linked at `0x52000000`
- `/usr/bin` proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 41 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 53 exit 0 cleanup 1 getdents64 2 getdents64-entries 6 getdents64-bytes 152 stat 7 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 pipe 0 pipe-create 0 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 0 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 4294967295 readv 0 readv-bytes 0 writev 1 writev-bytes 53 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 2 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 0 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 22 execve-last-binary-bytes 0 execve-last-closed-fds 0 execve-last-fd-live-before 0 execve-last-fd-live-after 0 execve-last-vma-before 0 execve-last-vma-released 0 execve-last-vma-after 0 execve-last-argc 0 execve-last-envc 0 execve-last-transfer-ready 0 execve-last-transfer-rip 0x0000000000000000 execve-last-transfer-rsp 0x0000000000000000 fork 0 fork-success 0 fork-enosys 0 fork-denial 0 fork-child-slot 4294967295 fork-child-root-distinct 0 fork-last-rip 0x0000000000000000 wait4 0 wait4-reap 0 wait4-last-exit-code 45 child-root-cleanup 0 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0 vfs-bin-alias 6 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M26 forked-child exec inheritance verification:

- generated by direct `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'"`
- visible console output is exactly `Nested FAT32 path fixture`
- implementation scope: no kernel code changes after M25; this is an evidence gate proving the M25 exec transfer composes with the M22 process-root pool, M23 fork/wait state duplication, M24 pipe fd inheritance, Linux cwd state, and the NVMe VFS provider
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 0 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 4 prctl-set-name 1 prctl-get-name 3 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 2 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE60 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 4 vfs-bin-open 0 vfs-bin-read 2 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M27 independently staged static ET_EXEC verification:

- generated by direct `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0-cat-noexec" -BusyBoxVersion "1.35.0" -ExtraAppPath .\external\build\zig-musl-smoke-imagebase -ExtraAppName SMOKE -ExtraAppSource "local-zig-musl-smoke" -ExtraAppVersion "0.1" -ExtraShellLine "linux /APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'"`
- visible console output is exactly `zig-musl-smoke`
- implementation scope: optional extra-app staging/provenance parameters in `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1`; scheduler-owned FS-base save/restore/set state for Linux tasks; `arch_prctl(ARCH_SET_FS)` updates to the active scheduler task; fork/clone child task FS-base seeding; and exec-transfer FS-base clearing before the new image sets its own TLS base
- first M27 failure fixed: BusyBox code resumed with the SMOKE TLS base and faulted at `mov %fs:0x0`; task-local FS-base save/restore now reports `fs-save 3 fs-restore 4 fs-set 7`
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- staged SMOKE artifact: `external\build\zig-musl-smoke-imagebase`, SHA-256 `F9F1BD81B69B6C8C13A7E9CAE3DA9C24E45B76124195F7D945F97A7B9BE0F50B`, static ET_EXEC linked at `0x52000000`, staged at `/APPS/SMOKE`
- scope note: the SMOKE artifact is a local ignored external-build product, not a committed repo fixture and not a third-party package; M28 replaces this with an upstream non-BusyBox package utility
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 66 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 15 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 15 write 2 write-bytes 30 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 1 prctl-get-name 2 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 2 vfs-bin-open 0 vfs-bin-read 1 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M28 third-party non-BusyBox package verification:

- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- built artifact: upstream `echo.c` plus upstream sbase `libutf.a`/`libutil.a`, built by the local musl cross toolchain as static non-PIE ET_EXEC at `0x52000000`; staged artifact `external\build\sbase-0.1-echo-x86_64-musl-0x52000000`, SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952
- ELF verification: `readelf -h -l` reports `Type: EXEC`, entry `0x520010d1`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`
- first M28 failure fixed: sbase `echo` legitimately calls `fclose(stdout)` before exit, so the older cleanup invariant `fd_release + exit_fd_release >= 3` falsely rejected the run with `drs-realbin-fail stage cleanup`. The invariant now proves the fd table is detached after cleanup instead of requiring three final fd releases.
- final artifact budget after the fix: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- direct command: `linux /APPS/SBECHO m28-sbase-direct`
- direct visible console output is exactly `m28-sbase-direct`
- direct proof summary: `drs-realbin path /APPS/SBECHO ... mapped 4 pages 11 stack 16 ... console-bytes 17 exit 0 cleanup 1 ... writev 1 writev-bytes 17 ioctl 1 ioctl-tty 1 ... pml4-pool-used-final 0 ... unimplemented 0 page-faults 0`
- forked pipeline command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'`
- forked pipeline visible console output is exactly `m28-sbase-pipeline`
- forked pipeline proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 68 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 19 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 19 write 1 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 19 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 3 prctl-set-name 1 prctl-get-name 2 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 2 vfs-bin-open 0 vfs-bin-read 1 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M29 broader third-party utility read path verification:

- implementation scope: second optional extra-app staging/provenance parameters in `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1`, allowing `/APPS/SBECHO` and `/APPS/SBCAT` to be staged in the same generated NVMe FAT image without embedding either third-party artifact in the repo
- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- new built artifact: upstream `cat.c` plus upstream sbase `libutf.a`/`libutil.a`, built by the local musl cross toolchain as static non-PIE ET_EXEC at `0x52000000`; staged artifact `external\build\sbase-0.1-cat-x86_64-musl-0x52000000`, SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- ELF verification: `readelf -h -l` reports `Type: EXEC`, entry `0x5200117f`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`
- final artifact budget: UEFI reserve 831,968 bytes; BIOS reserve remains 101 sectors
- direct command: `linux /APPS/SBCAT /nvme/apps/data/file.txt`
- direct visible console output is exactly `Nested FAT32 path fixture`
- direct proof summary: `drs-realbin path /APPS/SBCAT ... mapped 4 pages 9 stack 16 ... console-bytes 27 exit 0 cleanup 1 ... read 2 read-bytes 27 write 1 write-bytes 27 vfs-nvme-reads 2 vfs-nvme-bytes 27 pml4-pool-used-final 0 ... unimplemented 0 page-faults 0`
- two-executable pipeline command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'`
- two-executable pipeline visible console output is exactly `m29-sbase-pipe`
- proof note: `vfs-bin-alias 0 vfs-bin-read 0 vfs-bin-denial 0` proves the pipeline consumer did not use the BusyBox alias backend; `execve-last-binary-bytes 36968` and `execve-last-transfer-rip 0x000000005200117F` prove the last child exec was the staged sbase `cat` artifact
- two-executable pipeline proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 67 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 15 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 15 write 1 write-bytes 15 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 15 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE70 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M30 third-party PATH directory proof verification:

- implementation scope: fixed Linux VFS `/usr/local` and `/usr/local/bin` directories, PATH-resolvable third-party aliases for `sbecho` and `sbcat`, `vfs-localbin-*` telemetry, QMP keyboard support for uppercase, `=`, and `:`, and Linux `argv[0]` lowercase canonicalization so uppercase FAT-style launch paths still dispatch BusyBox correctly
- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- staged `sbecho` artifact: `external\build\sbase-0.1-echo-x86_64-musl-0x52000000`, SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952
- staged `sbcat` artifact: `external\build\sbase-0.1-cat-x86_64-musl-0x52000000`, SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve 827,872 bytes; BIOS reserve remains 101 sectors
- PATH pipeline command: `linux /APPS/BUSYBOX sh -c 'PATH=/usr/local/bin:/bin:/usr/bin; sbecho m30-path-pipe | sbcat'`
- PATH pipeline visible console output is exactly `m30-path-pipe`
- proof note: `vfs-localbin-alias 6 vfs-localbin-read 2 vfs-localbin-denial 0` proves both third-party utilities were found through `/usr/local/bin`; `vfs-bin-alias 0 vfs-bin-read 0` proves the BusyBox alias backend was not used for the pipeline children; `execve 2`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the two-child PATH pipeline completed and cleaned up
- directory control: `linux /APPS/BUSYBOX ls /usr/local/bin` prints `sbcat   sbecho` and reports `getdents64 2 getdents64-entries 2 stat 3 stat-denial 0 vfs-localbin-alias 2 vfs-localbin-denial 0 page-faults 0`
- PATH pipeline proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 14 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 14 write 1 write-bytes 14 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 14 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M31 default Linux environment proof verification:

- implementation scope: UEFI-only real Linux launcher now supplies one fixed initial environment entry, `PATH=/usr/local/bin:/bin:/usr/bin`, to `elf64_launch_static`; real-binary telemetry now reports the actual initial stack environment count as `envc`
- staged artifacts are unchanged from M30: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- default-environment command: `linux /APPS/BUSYBOX sh -c 'sbecho m31-default-path | sbcat'`
- visible console output is exactly `m31-default-path`
- proof note: `envc 1` proves the initial BusyBox stack received the default environment; `execve-last-envc 2` proves BusyBox ash passed environment entries into the forked child replacements; `vfs-localbin-alias 6 vfs-localbin-read 2` proves PATH resolution used `/usr/local/bin`; `vfs-bin-alias 0 vfs-bin-read 0` proves the BusyBox alias backend was not used for the third-party pipeline; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- default-environment proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 1 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 17 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 17 write 1 write-bytes 17 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 17 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 2 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE50 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M32 bounded shell environment polish verification:

- implementation scope: UEFI-only real Linux launcher default environment now contains `PATH=/usr/local/bin:/bin:/usr/bin`, `HOME=/`, `USER=limitless`, and `PWD=/`; the QMP keyboard injector now supports literal `$` so shell variable expansion commands are delivered faithfully
- staged artifacts are unchanged from M30: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- shell-expansion command: `linux /APPS/BUSYBOX sh -c 'sbecho $USER:$PWD | sbcat'`
- visible console output is exactly `limitless:/`
- proof note: `envc 4` proves the initial BusyBox stack received all four default environment entries; `execve-last-envc 4` proves BusyBox ash passed them into forked child replacements; `vfs-localbin-alias 6 vfs-localbin-read 2` proves default PATH still resolves `/usr/local/bin` third-party utilities; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- shell-environment proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 12 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 12 write 1 write-bytes 12 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 12 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M33 cwd and PWD synchronization proof verification:

- implementation scope: no kernel, verifier, or artifact changes were required after M32; the M33 trace proves the existing `chdir`, cwd inheritance, environment inheritance, PATH lookup, fork, exec, and pipe paths already compose correctly
- staged artifacts are unchanged from M30: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- cwd/PWD command: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; sbecho $PWD | sbcat'`
- visible console output is exactly `/nvme/apps`
- proof note: `chdir 1` proves BusyBox ash changed the Linux persona cwd; `execve-last-envc 5` proves ash exported an updated child environment after the `cd`; `path-relative 1 path-dot 1` proves relative resolution remained active; `vfs-localbin-alias 6 vfs-localbin-read 2` proves default PATH still resolved the third-party tools from `/usr/local/bin`; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- cwd/PWD proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 11 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 11 write 1 write-bytes 11 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 11 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M34 environment mutation and export proof verification:

- implementation scope: no kernel or verifier changes were required; an additional ignored upstream sbase 0.1 `env` artifact was built by the local musl cross toolchain as static non-PIE ET_EXEC at `0x52000000` and staged as `/APPS/SBENV`
- source package: suckless sbase 0.1 from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, source tarball SHA-256 `86f6bb67bcc7df3ba7a3f11da72eaeb2cf58c23e9a35a7dbcd316395d934c634`
- new staged `sbenv` artifact: `external\build\sbase-0.1-env-x86_64-musl-0x52000000`, SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616
- companion `sbcat` artifact is unchanged from M29: `external\build\sbase-0.1-cat-x86_64-musl-0x52000000`, SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- ELF verification for `sbenv`: `readelf -h -l` reports `Type: EXEC`, entry `0x5200105b`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- environment-observation command: `linux /APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: the visible `USER=operator` line is printed by the forked third-party `sbenv` child, not by BusyBox shell expansion; `execve 2 execve-last-envc 4` proves both third-party children were replaced with a four-entry environment; `vfs-nvme-reads 2` proves the child binaries came from staged NVMe FAT artifacts; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- environment mutation proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 0 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M35 third-party exec handoff proof verification:

- implementation scope: no kernel, verifier, or artifact changes were required after M34; the M35 trace proves the existing third-party `execvp` path already composes with explicit Linux VFS paths, environment replacement, fork, pipe, and wait cleanup
- staged artifacts are unchanged from M34: `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- third-party handoff command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator /nvme/apps/sbenv | /nvme/apps/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3` proves the extra non-shell replacement happened after BusyBox ash forked the pipeline: the shell execed the first `sbenv`, that `sbenv` applied `USER=operator` and execed the second `/nvme/apps/sbenv`, and the consumer child execed `/nvme/apps/sbcat`; `vfs-nvme-reads 3` proves the real ET_EXEC child images were loaded from NVMe FAT; `execve-last-envc 4`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the handoff preserved the bounded environment and cleaned up
- third-party handoff proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 0 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M36 third-party PATH execvp proof verification:

- implementation scope: added a fixed `/usr/local/bin/sbenv` Linux VFS alias backed by the staged NVMe FAT `/APPS/SBENV` artifact through the Linux-visible `/nvme/apps/sbenv` path; no syscall surface, dynamic linking, heap allocation, or arbitrary filesystem population was added
- staged artifacts are unchanged from M34: `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- third-party PATH handoff command: `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator sbenv | /nvme/apps/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3` proves the shell execed the first `/nvme/apps/sbenv`, that third-party process used PATH `execvp` to replace itself with `sbenv`, and the consumer child execed `/nvme/apps/sbcat`; `vfs-localbin-alias 2 vfs-localbin-read 1 vfs-localbin-denial 0` proves the nested non-shell PATH lookup used the bounded `/usr/local/bin/sbenv` alias; `vfs-nvme-reads 3`, `execve-last-envc 4`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean handoff, environment preservation, and cleanup
- third-party PATH handoff proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 2 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M37 all-third-party PATH pipeline proof verification:

- implementation scope: no kernel, verifier, or artifact changes were required after M36; the trace proves the existing default environment, `/usr/local/bin` aliases, fork, pipe, wait, and non-shell `execvp` paths compose when all child executable names are resolved by PATH
- staged artifacts are unchanged from M34: `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- all-third-party PATH command: `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator sbenv | sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `vfs-localbin-alias 8 vfs-localbin-read 3 vfs-localbin-denial 0` proves BusyBox ash found `sbenv` and `sbcat` by PATH and the first `sbenv` found the second `sbenv` by PATH; `vfs-bin-alias 0` proves the BusyBox applet alias backend was not used for the child executables; `execve 3`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the fully PATH-resolved third-party pipeline completed and cleaned up
- all-third-party PATH proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 74 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 8 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M38 expanded localbin directory proof verification:

- implementation scope: extended `tools\generate-nvme-image.ps1` and `tools\verify-qemu.ps1` with a third optional extra-app staging slot so the verifier can stage `/APPS/SBECHO`, `/APPS/SBENV`, and `/APPS/SBCAT` in the same NVMe FAT image; no kernel, syscall, VFS, loader, or artifact changes were required
- first trace result: with only two extra-app slots staged, `ls /usr/local/bin` listed three VFS aliases but failed `stat` on the unstaged `sbecho` backend, printing `ls: /usr/local/bin/sbecho: No such file or directory` and reporting `stat-denial 1 exit 1`
- staged artifacts: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- expanded-localbin command: `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin'`
- visible console output is exactly `sbenv   sbcat   sbecho`
- proof note: `getdents64-entries 3` proves `/usr/local/bin` enumerated the three bounded third-party aliases, `stat-denial 0` proves each listed entry had a present staged backend, and `vfs-localbin-alias 3 vfs-localbin-denial 0` proves the directory/stat path used the localbin alias provider cleanly
- expanded-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 38 syscall-root-denial 0 fs-save 0 fs-restore 1 fs-set 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 23 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 96 stat 6 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 pipe 0 pipe-create 0 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 0 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 4294967295 readv 0 readv-bytes 0 writev 1 writev-bytes 23 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 2 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 0 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 22 execve-last-binary-bytes 0 execve-last-closed-fds 0 execve-last-fd-live-before 0 execve-last-fd-live-after 0 execve-last-vma-before 0 execve-last-vma-released 0 execve-last-vma-after 0 execve-last-argc 0 execve-last-envc 0 execve-last-transfer-ready 0 execve-last-transfer-rip 0x0000000000000000 execve-last-transfer-rsp 0x0000000000000000 fork 0 fork-success 0 fork-enosys 0 fork-denial 0 fork-child-slot 4294967295 fork-child-root-distinct 0 fork-last-rip 0x0000000000000000 wait4 0 wait4-reap 0 wait4-last-exit-code 45 child-root-cleanup 0 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M39 absolute localbin exec proof verification:

- implementation scope: raised the persistent ring-3 shell interactive command capacity in `kernel\arch\x86_64\runtime_image_user.asm` from 96 to 128 bytes so it matches `SHELL64_MAX_LINE_BYTES`; this allows the 106-byte absolute-path proof command to reach `shell64_execute_line` intact. No Linux syscall, VFS alias, loader, or third-party artifact changes were required.
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- absolute localbin command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv USER=operator /usr/local/bin/sbenv | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3`, `vfs-localbin-alias 6`, `vfs-localbin-read 3`, and `vfs-localbin-denial 0` prove BusyBox ash executed absolute `/usr/local/bin/sbenv`, the first `sbenv` replaced itself through absolute `/usr/local/bin/sbenv`, and the consumer executed absolute `/usr/local/bin/sbcat`; `vfs-bin-alias 0` proves no BusyBox applet alias backend was used for the child executables; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove the absolute third-party pipeline completed and cleaned up
- absolute-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M40 absolute localbin file-read proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M39
- first trace note: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt'` passed and printed `Nested FAT32 path fixture`, but BusyBox ash optimized the single final command by replacing itself through `execve`, so that direct proof reports `fork 0`; the accepted M40 pipeline proof below forces forked child exec for both absolute-localbin `sbcat` commands
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- absolute-localbin file pipeline command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `execve 2`, `fork 2`, `wait4-reap 2`, and `pipe-live-final 0` prove BusyBox ash forked both pipeline sides and reaped both children; `read 4 read-bytes 54 write 2 write-bytes 54` proves one child read the real NVMe file and the second child consumed the pipe; `vfs-localbin-alias 4 vfs-localbin-read 2 vfs-localbin-denial 0` proves both executables came from absolute `/usr/local/bin` aliases; `vfs-nvme-reads 4 vfs-nvme-bytes 36968` proves the staged `SBCAT` artifact plus file content were read through the NVMe VFS; `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process isolation and cleanup
- absolute-localbin file-read proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M41 localbin cwd-relative file-read proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M40
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- cwd-relative absolute-localbin command: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /usr/local/bin/sbcat data/file.txt | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `chdir 1` proves BusyBox ash changed cwd to `/nvme/apps`; `path-relative 2` proves the child-side relative path was canonicalized against inherited cwd; `execve-last-envc 5` proves BusyBox ash exported updated `PWD`; `execve 2`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `vfs-localbin-read 2`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove both absolute-localbin children executed, transferred the file bytes, and cleaned up
- cwd-relative absolute-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 2 path-dot 1 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M42 localbin dotdot relative path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M41
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- dotdot relative absolute-localbin command: `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps/data; /usr/local/bin/sbcat ../data/file.txt | /usr/local/bin/sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `chdir 1` proves BusyBox ash changed cwd to `/nvme/apps/data`; `path-relative 2 path-dotdot 1 path-fault 0` proves a forked absolute-localbin child canonicalized `../data/file.txt` back to the real NVMe file; `execve-last-envc 5` proves the updated `PWD` environment crossed exec; `execve 2`, `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `vfs-localbin-read 2`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean execution and cleanup
- dotdot relative absolute-localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 2 path-dot 1 path-dotdot 1 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M43 localbin current-directory path proof verification:

- implementation scope: `kernel/arch/x86_64/linux_abi.c` now canonicalizes `execve` user paths against cwd before VFS stat/read, matching the path behavior already used by open/stat/chdir
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- current-directory localbin command: `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ./sbcat /nvme/apps/data/file.txt | ./sbcat'`
- visible console output is exactly:

```text
Nested FAT32 path fixture
```

- proof note: `chdir 1` proves BusyBox ash changed cwd to `/usr/local/bin`; `path-relative 3 path-dot 3 path-fault 0` proves both `./sbcat` executable paths and the input file path were canonicalized without fault; `execve 2 execve-denial 0` proves the forked children reached real exec transfer; `vfs-localbin-alias 4 vfs-localbin-read 2` proves the normalized `/usr/local/bin/sbcat` paths hit the bounded third-party alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- current-directory localbin proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 3 path-dot 3 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M44 non-shell current-directory execvp proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M43
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- non-shell current-directory execvp command: `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ./sbenv USER=operator ./sbenv | ./sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
OLDPWD=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/usr/local/bin
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `./sbenv`, the first `sbenv` replaced itself with another `./sbenv`, and the pipeline consumer launched `./sbcat`; `path-relative 4 path-dot 4 path-fault 0` proves current-directory executable path canonicalization across all relative executable paths; `vfs-localbin-alias 6 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- non-shell current-directory execvp proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 83 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 4 path-dot 4 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 83 write 1 write-bytes 83 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 83 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M45 localbin executable dotdot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M44
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- relative executable dotdot command: `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ../bin/sbenv USER=operator ../bin/sbenv | ../bin/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
OLDPWD=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/usr/local/bin
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `../bin/sbenv`, the first `sbenv` replaced itself with another `../bin/sbenv`, and the pipeline consumer launched `../bin/sbcat`; `path-relative 4 path-dotdot 3 path-fault 0` proves relative executable `..` canonicalization across all executable paths; `vfs-localbin-alias 6 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- relative executable dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 83 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 4 path-dot 1 path-dotdot 3 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 83 write 1 write-bytes 83 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 83 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M46 absolute localbin executable dotdot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M45
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- absolute executable dotdot command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/../bin/sbenv USER=operator /usr/local/bin/../bin/sbenv | /usr/local/bin/../bin/sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr/local/bin/../bin/sbenv`, the first `sbenv` replaced itself with another `/usr/local/bin/../bin/sbenv`, and the pipeline consumer launched `/usr/local/bin/../bin/sbcat`; `path-relative 1 path-dotdot 3 path-fault 0` proves absolute executable `..` canonicalization without a shell `cd`; `vfs-localbin-alias 6 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- absolute executable dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 3 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M47 absolute localbin mixed-dot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M46
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- mixed-dot command: `linux /APPS/BUSYBOX sh -c '/usr/local/./bin/../bin/sbenv USER=operator /usr/local/./bin/../bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr/local/./bin/../bin/sbenv`, the first `sbenv` replaced itself with another `/usr/local/./bin/../bin/sbenv`, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 3 path-dotdot 2 path-fault 0` proves mixed absolute executable `.` and `..` canonicalization; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- mixed-dot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 3 path-dotdot 2 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M48 absolute localbin repeated-slash path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M47
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- repeated-slash command: `linux /APPS/BUSYBOX sh -c '/usr//local/bin/sbenv USER=operator /usr/local//bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr//local/bin/sbenv`, the first `sbenv` replaced itself with `/usr/local//bin/sbenv`, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 1 path-dotdot 0 path-fault 0` proves repeated slash canonicalization did not require dotdot handling; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- repeated-slash proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M49 absolute localbin root-clamped dotdot path proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M48
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- root-clamped dotdot command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/../../local/bin/sbenv USER=operator /usr/local/bin/../../local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/usr/local/bin/../../local/bin/sbenv`, the first `sbenv` replaced itself with the same root-clamped path, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 1 path-dotdot 4 path-fault 0` proves bounded upward `..` canonicalization did not escape the root or fault; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- root-clamped dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 4 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M50 absolute over-root dotdot clamp proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M49
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- over-root dotdot command: `linux /APPS/BUSYBOX sh -c '/../../usr/local/bin/sbenv USER=operator /../../../usr/local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

- proof note: `execve 3 execve-denial 0` proves BusyBox ash launched `/../../usr/local/bin/sbenv`, the first `sbenv` replaced itself with `/../../../usr/local/bin/sbenv`, and the pipeline consumer resolved `sbcat` through the default PATH; `path-relative 1 path-dot 1 path-dotdot 5 path-fault 0` proves over-root `..` traversal clamped at `/` without escaping or faulting; `vfs-localbin-alias 7 vfs-localbin-read 3` proves each normalized executable path hit the bounded localbin alias table; `fork 2`, `wait4-reap 2`, `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean process, pipe, address-space, and CR3 cleanup
- over-root dotdot proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 5 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M51 absolute localbin trailing-slash executable denial proof verification:

- implementation scope: `kernel/arch/x86_64/linux_abi.c` now preserves original execve trailing-slash intent across canonicalization and denies slash-suffixed non-directory executable targets before the binary read; `kernel/arch/x86_64/linux_exec.c` emits `path-trailing` and `path-trailing-denial` telemetry
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest after the fix: kernel bytes 1,269,280; checksum `0xD4789513`; SHA-256 `fa4be9a316b1888ac9f363ded5b788015470f1849e9255f1ce8d308f776ed628`
- trailing-slash denial command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv/ USER=operator /usr/local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
sh: /usr/local/bin/sbenv/: not found
```

- proof note: `path-trailing 1 path-trailing-denial 1` proves the original terminal slash survived canonicalization as denial intent; `execve 1 execve-denial 1` proves the slash-suffixed `sbenv` was not launched as a file; `vfs-localbin-read 1` proves only the pipeline consumer `sbcat` was read as an executable; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- trailing-slash denial telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 65 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 37 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 1 path-trailing-denial 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 37 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M52 absolute localbin trailing-slash directory/open proof verification:

- implementation scope: no kernel, syscall, VFS, loader, host tooling, or artifact changes were required after M51
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- trailing-slash directory command: `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin/ | sbcat'`
- visible console output is exactly:

```text
sbenv
sbcat
sbecho
```

- proof note: `getdents64 2 getdents64-entries 3` proves `/usr/local/bin/` remains a valid directory path with a terminal slash; `execve-denial 0 path-trailing 0 path-trailing-denial 0` proves the M51 executable-only trailing-slash denial did not fire on directory enumeration; `vfs-localbin-alias 6 vfs-localbin-read 1` proves the alias namespace stayed discoverable and only the pipeline consumer `sbcat` was read as an executable; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove clean cleanup
- trailing-slash directory telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 79 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 19 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 96 stat 7 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 19 write 1 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 19 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 0 ioctl-enotty 2 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M53 absolute localbin missing executable denial proof verification:

- implementation scope: `kernel/arch/x86_64/linux_abi.c` now maps a failed executable binary `stat` in the exec-read path to `ENOENT` instead of collapsing it into `EINVAL`, so BusyBox ash reports a missing absolute-localbin executable as `not found`
- staged artifacts are unchanged from M38: `/APPS/SBECHO` SHA-256 `35da6db9dcf1c7e76ae605eae175318831bcd10bbbf406d5a30a600d2ae4b667`, byte count 46,952; `/APPS/SBENV` SHA-256 `a678597a247cceaede00641b88497bd51f684fa29072a3192adcaabb4aba54f4`, byte count 52,616; `/APPS/SBCAT` SHA-256 `fdc39f6d97f7e7492dae5983732b2e23fd063cc7eac99c5f0114fd93e6a95662`, byte count 36,968
- final artifact budget: UEFI reserve remains 827,872 bytes; BIOS reserve remains 101 sectors
- UEFI manifest after the fix: kernel bytes 1,269,280; checksum `0xD00B97A2`; SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`
- missing executable command: `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbmissing USER=operator /usr/local/bin/sbenv|sbcat'`
- visible console output is exactly:

```text
sh: /usr/local/bin/sbmissing: not found
```

- proof note: `execve 1 execve-denial 1` proves the missing producer was denied as an exec target; `vfs-localbin-denial 2` and `vfs-bin-denial 2` prove the bounded alias tables rejected the missing executable; `vfs-localbin-read 1` and `vfs-nvme-reads 1` prove only the valid pipeline consumer `sbcat` was read from its NVMe-backed alias; `pipe-live-final 0`, `pml4-pool-used-final 0`, `syscall-root-repair 0`, and `page-faults 0` prove denial cleanup stayed clean
- missing executable telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 65 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 40 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 40 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 2 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 2
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## Persistence

Persistence is reboot-surviving in the verifier. `verify-nvme-persistence.ps1` runs two sequential boots against the same NVMe GPT image, observes content written in the first boot from the second boot, and prints:

- scoped write authority required
- commit authority required
- wrong-owner denial observed
- stale/revoked denial observed
- read-only write denial observed
- nonzero commit/flush counter
- same NVMe image reused
- not RAM-backed
- exit code 0

## Remaining Daily-Driver Blockers

- Product BIOS reserve is currently 101 sectors after rebuilding the present source, below the 128-sector warning threshold but still inside the hard 1024-sector loader limit.
- Product UEFI now uses the 2 MiB file-size contract; BIOS remains on the 1024-sector loader contract.
- UEFI remains governed by `KERNEL64.BIN` byte budget, manifest/checksum correctness, placement/load correctness, and artifact inventory correctness. UEFI is not blocked by the BIOS 1024-sector ceiling.
- GUI/window manager/desktop are Product for Terminal, File Manager, Settings, Installer, brokered input, focused keyboard routing, and compositor-owned display.
- Network now has an M19 brokered TCP-client socket foundation over the hardware-gated DHCP/DNS/HTTP path, and M21 packages one manifest-backed native app that consumes it from Ring 3 through generic descriptor validation. There is still no general socket library, server socket API, raw packet API, arbitrary app send/receive data plane, or ambient network authority.
- Real static Linux ELF execution now works on the UEFI Product path for externally built BusyBox static ET_EXEC binaries loaded from NVMe FAT and linked into the supported user VMA window. M22 adds per-process page tables for UEFI Product Linux persona launches with shared kernel/MMIO mappings and scheduler CR3 switching. M23 adds bounded fork/wait for the observed BusyBox ash external-command path. M24 adds Unix pipe composition across two forked BusyBox ash pipeline children. M25 adds Linux-visible BusyBox applet aliases and real external `execve` transfer to a 64 KiB real-binary stack. M26 proves forked child exec images inherit cwd, VFS authority, and pipe endpoints correctly for BusyBox aliases. M27 proves an independently staged static ET_EXEC can run through Linux-visible NVMe VFS child `execve` without using the BusyBox alias backend. M28 proves an upstream non-BusyBox static ET_EXEC utility from suckless sbase can launch directly and through forked BusyBox ash pipeline exec. M29 proves a second upstream sbase static ET_EXEC utility can read NVMe file content directly and consume a pipeline from another third-party ET_EXEC utility without relying on the BusyBox alias backend. M30-M53 prove `/usr/local/bin` PATH discovery, bounded default environment setup, shell-updated `PWD`, child-observed exported environment mutation, non-shell `execvp`, directory enumeration, absolute localbin execution, absolute localbin file reads, cwd-relative file reads, `..` relative path normalization, current-directory `./` localbin execution, non-shell current-directory `./` exec handoff, relative executable `..` localbin handoff, absolute executable `..` localbin handoff, mixed absolute executable `.`/`..` localbin handoff, repeated-slash absolute executable localbin handoff, root-clamped absolute executable `..` localbin handoff, over-root absolute executable `..` clamp handoff, trailing-slash executable denial, trailing-slash directory enumeration, and missing absolute-localbin executable denial for third-party static ET_EXEC utilities. Dynamic Linux ELF, glibc dynamic linking, vfork, broad clone flag compatibility, real threading, signal delivery, sockets in the Linux ABI table, broad third-party package compatibility, and broad distro compatibility remain unavailable.
- Installer UX planning and dry-run are Product, but real internal install/write, formatting, and boot-entry authority remain unavailable.
- M7.1 signed package admission has separate negative fixture coverage, but package-manager UI, app store, auto-install, live public update fetching, and trusted-time expiry enforcement remain unavailable.
- M6 has a local console session model, not full multiuser login/authentication.
- M11 has a local identity model and vault foundation only; personal login, enterprise login, cloud association, cloud storage, encrypted secret storage, and token storage remain unavailable.
- M12 has an identity transport foundation only; encrypted account transport, credential transport, token persistence, remote login, and trusted-time expiry enforcement remain unavailable.
- M13 has an account association status foundation only; local association is active, but personal/enterprise/cloud association, account linking, security-key login, enterprise policy, token persistence, and remote account authority remain unavailable.
- M14 has a cloud-storage broker foundation only; real public cloud storage, sync, automatic upload/download, offline cache, token storage, encrypted cloud transport, AI cloud access, and app-direct cloud authority remain unavailable or denied.
- M15 has installer UX planning only; destructive install actions remain disabled by default and not Product-approved.
- Assistant remains Mode B with read-only context flow and predefined consent-scoped action templates only. Inference backend, model transport, generated answers, broad automation, autonomous action, package install/update, settings mutation, cloud AI, cloud memory, and internal install/write remain unavailable.
- Hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
