# Real Binary And Real Hardware Gate

Effective after M21, new Product progress must be proven with real externally built software or real hardware behavior, not synthetic test processes.

Current status: the first static Linux x86_64 ELF execution gate, the M22 per-process page table foundation gate, the M23 bounded fork/wait gate, the M24 Unix pipeline gate, the M25 Linux VFS path execution gate, the M26 forked-child execve inheritance gate, M27-M61 third-party static ET_EXEC path/cwd/env/execvp/canonicalization gates, M62 low-compat removal, M63 signal foundation, M64 pthread-style clone threading, M65 contended futex wakeups, M66 TLS/pool expansion, M67-M69 bounded file-backed mmap, M70-M105 dynamic ELF progression from denial-path telemetry through first supported-interpreter execution, multiple dynamic ET_EXEC runtime breadth proofs, libc-helper breadth, inherited environment binding, stdio helper output, bounded heap helpers, environment mutation, first dynamic pthread helper execution, multi-threaded dynamic pthread TLS/condition/futex contention, dynamic NVMe VFS file open/read/write/close, dynamic file metadata/seek behavior, dynamic directory enumeration, dynamic cwd/relative path behavior, dynamic vectored I/O/readiness behavior, dynamic fstatat metadata behavior, dynamic openat relative file-read behavior, dynamic openat dirfd-relative lookup behavior, dynamic fchdir cwd handoff behavior, dynamic fcntl descriptor/status flag behavior, dynamic fcntl descriptor duplication behavior, direct dynamic dup syscall behavior, direct dynamic pipe syscall behavior, dynamic fork-plus-pipe/wait composition, blocked pipe read replay, dynamic pipe close/error semantics, M106 universal hardware inventory/driver-binding evidence, M107 physical display readability, M108 visible cursor fallback, M109 Product visual polish direct compositor foundation, M110 NVMe/FAT hardware storage triage, M111 boot/NVMe staged dynamic artifact verification, M112 physical hardware storage capture parsing, M113 physical hardware storage evidence bundling, M114 physical hardware storage capture analysis, M115 physical hardware storage evidence verification, M116 physical hardware storage analysis fixture coverage, M117 physical display/input capture analysis, M118 MSI hardware capture analysis, M119 MSI hardware capture analysis fixture coverage, M120 boot-media Linux handoff verification, M121 MSI hardware handoff bundle refresh, M122 MSI hardware handoff verifier, M123 MSI hardware handoff verifier fixture coverage, M124 self-verifying MSI handoff packaging, M125 MSI dynamic handoff capture classification, M127 FAT backend completion, M128 File Manager real workflows, M129 Settings real workflows, M130 Terminal quality pass, M131 Login/session polish, M132 Window manager usability, M133 MSI hardware capture closure, M134 storage hardware target classification, M135 M134 classifier handoff integration, M136 storage diagnostic playbooks, M137 NVMe PCI identity telemetry, M138 NVMe PCI capture-stage classification, M139-M148 storage hardware target refinement through NVMe candidate-source/deferred handoff telemetry, M149 cursor path hardware-display diagnostics, M150 MSI cursor-stage routing closure, M151 MSI GUI interaction-stage routing, M152 hwval GUI interaction telemetry, M153 MSI handoff GUI telemetry requirement, M154 current MSI handoff bundle refresh, M155 MSI capture intake report, M156 MSI handoff report contract integration, M157 VMD/NVMe handoff report surfacing, M158 no-touch VMD/NVMe driver-plan handoff, and M159 scoped VMD/NVMe bind/probe handoff are crossed on the UEFI Product path. Detailed command evidence and milestone telemetry are recorded below.

Current BIOS budget note: the Product BIOS path has 101 reserve sectors, below the 128-sector warning threshold but still inside the hard 1024-sector loader limit. New real-binary work must continue to protect the BIOS path from accidental large buffers or code growth.

## M159 Scoped VMD/NVMe Bind/Probe Handoff

M159 is accepted as the first real code path that consumes the staged VMD child NVMe handoff. Under UEFI Product and scoped hardware-query authority, `mmio64_bind_vmd_nested_nvme_candidate()` validates the deferred VMD nested candidate source, matching nested BDF, nonzero handoff token, and child NVMe MMIO readiness, then promotes the candidate source to `MMIO64_NVME_CANDIDATE_SOURCE_VMD_NESTED_BOUND` (`3`) so the existing NVMe probe/read/GPT/FAT path can attempt the controller normally. The path records authorization denials, unavailable prerequisites, successful bind count, flags, result token, and promoted candidate state through a new `drs-vmd-nvme-bind` line in both brokered storage proof output and shell hardware/storage diagnostics.

Host tooling remains backward-compatible with M158 transcripts and now merges `drs-vmd-nvme-bind` into parsed storage evidence. A successful VMD bind advances classification to the existing NVMe probe stages (`nvme-controller-discovery`, `nvme-controller-ready`, `nvme-identify`, and later storage stages) instead of continuing to classify the capture as only `pci-vmd-nested-driver-plan-staged`. Accepted verification: Product build plus M1 production-slice gate, `hardware-storage-analysis-fixtures: 63/63`, `m134-storage-target-fixtures: 14/14`, and full Product QEMU verification. Final reserves: BIOS `101` sectors and UEFI `721,696` bytes. M159 does not claim a physical MSI pass, a working VMD-backed NVMe device, successful internal SSD FAT access, or safe internal-disk writes; it makes the next hardware transcript falsifiable at the controller-probe level.

## M158 No-Touch VMD/NVMe Driver-Plan Handoff

M158 is accepted as a hardware-handoff milestone, not as a storage-driver milestone. The UEFI Product PCI/storage path now emits `vmd-nested-driver-plan-result`, `vmd-nested-driver-plan-state`, `vmd-nested-driver-plan-flags`, `vmd-nested-driver-plan-token`, `vmd-nested-driver-plan-stage-count`, `vmd-nested-driver-plan-denials`, and `vmd-nested-driver-plan-unavailable` through `hwval`, storage triage, and the brokered PCI proof. A staged plan requires scoped hardware-query authority, deferred VMD nested register evidence, matching deferred NVMe candidate evidence, nested MMIO readiness, and explicit no-touch flags for no MMIO writes, no commands, and no probe/bind attempt.

Host evidence tools classify a complete synthetic VMD-deferred transcript as `pci-vmd-nested-driver-plan-staged` and preserve a structured `vmd_handoff` object with `kind = vmd-nested-driver-plan` and `stage = driver-plan-staged`. Direct QEMU NVMe correctly reports the plan unavailable because there is no VMD candidate. Accepted verification: `hardware-storage-analysis-fixtures: 63/63`, `m134-storage-target-fixtures: 14/14`, Product build plus M1 production-slice gate, and targeted UEFI Product storage verification. Final reserves: BIOS `101` sectors and UEFI `722,112` bytes. M158 does not claim a physical MSI pass, a VMD driver, a VMD-backed NVMe bind, VMD/NVMe commands, or unsafe storage I/O.

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

The current proof is recorded by `tools\verify-real-binary-gate.ps1`, direct `tools\verify-qemu.ps1 -RealBinaryGate` runs, and `build\real-binary-gate-provenance.txt` when the provenance verifier is used. It records provenance, SHA-256, file/readelf metadata, `/APPS/BUSYBOX`, the echo command and visible output, the `/proc/meminfo` cat command and visible output, the `/nvme/apps/data/file.txt` cat command and visible NVMe FAT fixture output, the `/nvme/apps` directory listing output, the `sh` banner and `$` prompt, the M22 CR3 isolation echo command, the M23 fork/wait shell external-command output, the M24 `echo hello | cat` pipeline output, the M25 BusyBox PATH-search command, the M25 `/usr/bin` fixed applet directory listing, the M26 forked child exec pipeline with inherited cwd and pipe fds, the M27 independently staged SMOKE pipeline, the M28 direct `/APPS/SBECHO` launch, the M28 `/nvme/apps/sbecho` forked pipeline, the M29 direct `/APPS/SBCAT` NVMe file read, the M29 `/nvme/apps/sbecho | /nvme/apps/sbcat` two-executable pipeline, the M30 `/usr/local/bin` PATH pipeline, the M31 default PATH pipeline, the M32 default `USER`/`PWD` expansion pipeline, the M33 post-`chdir` `PWD` pipeline, the M34 child-observed exported environment pipeline, the M35 third-party `execvp` handoff pipeline, the M36 third-party PATH `execvp` handoff pipeline, the M37 all-third-party PATH pipeline, the M38 expanded `/usr/local/bin` directory listing, the M39 absolute `/usr/local/bin` exec pipeline, the M40 absolute `/usr/local/bin/sbcat` file-read pipeline, the M41 cwd-relative absolute-localbin file-read pipeline, the M42 `..` relative absolute-localbin file-read pipeline, the M43 current-directory `./sbcat` file-read pipeline, the M51 trailing-slash executable denial, the M52 trailing-slash directory enumeration, the M53 missing absolute-localbin executable denial, the M54 PATH missing executable denial, the M55 non-shell `execvp` missing executable denial, the M56 non-shell `execvp` trailing-slash executable denial, the M57 non-shell `execvp` directory-target denial, the M58 non-shell `execvp` bare-directory executable denial, the M59 non-shell `execvp` dot-directory executable denial, the M60 non-shell `execvp` parent-directory executable denial, the M61 non-shell `execvp` parent-rebased directory executable denial, exit codes, positive `drs-realbin` telemetry, NVMe VFS bind/release/read/readdir telemetry, Linux VFS BusyBox alias telemetry, terminal-size ioctl telemetry, cwd/chdir telemetry, relative path canonicalization telemetry, proc symlink telemetry, mixed `/proc/self` directory telemetry, pipe/fork/wait/execve telemetry, and negative coverage for missing file, dynamic `PT_INTERP`, low-address ET_EXEC binaries, oversized input, and BIOS unavailable behavior. The low-address negative uses a real upstream-default-address BusyBox when available and proves that ET_EXEC loads below `0x01000000` remain denied while the kernel still protects the low compatibility window. CI or release verification can require that proof explicitly with `-RequireLowAddressNegative`; the verifier rejects that switch if negative tests are skipped. The `-RequireShellCwdLoop` mode proves BusyBox ash current-working-directory behavior through `/`, `/nvme/apps`, and back to `/`. The `-RequireRelativePathProof` mode proves relative `.` and `..` canonicalization with `cat nvme/apps/./data/../data/file.txt` and `ls -l nvme/apps/./data/..`. The `-RequireProcSymlinkProof` mode proves `readlink(2)` through BusyBox `ls -l /proc/self/exe`. The `-RequireProcFdProof` mode proves stable proc-fd symlink enumeration for brokered fds 0, 1, and 2 through BusyBox `ls -l /proc/self/fd`; the bounded VFS intentionally skips the directory iterator fd for that directory so the verifier does not publish a link that can disappear before BusyBox's later `readlink` pass. The `-RequireProcSelfProof` mode proves mixed `/proc/self` directory enumeration for regular pseudo-files, the `fd` directory, and the `exe` symlink through BusyBox `ls -l /proc/self`. The historical `-TraceShellForkBoundary` mode records the BusyBox ash external-command boundary that M23 implemented. The NVMe file proof uses `/APPS/DATA/FILE.TXT` through the Linux-visible `/nvme/apps/data/file.txt` path because the older `/NVME.TXT` fixture is intentionally mutated by the broker-private `drs-nvme-fat` write checkpoint before the shell runs.

Passing artifacts:

- `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000`: BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, SHA-256 `5CDE8968EB2FEDB62DEA27947CD269BC57AD9C8B142ABFF0C3B1514A0238E8D9`, verified with `echo limitless-real-binary`, `cat /proc/meminfo`, and `cat /nvme/apps/data/file.txt`.
- `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-ls`: BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, and `sh`, SHA-256 `299DE064F51DA04DE99227F26F2EAB60C95F400C1B83731E14E3E28F86695652`, verified with `ls /nvme/apps` and `sh`.
- `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`: BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, `sh`, `FEATURE_PREFER_APPLETS`, `FEATURE_SH_STANDALONE`, `FEATURE_SH_NOFORK`, and a gate-local `cat` NOEXEC applet patch, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, verified with the default real-binary gate including `echo`, `cat`, `ls`, the bounded `sh` builtin loop, the M22 CR3 isolation command, the M23 fork/wait external-command shell path, the M24 pipeline path, the M25 PATH-search command, the M25 Linux-visible `/usr/bin` applet directory, and the M26 forked-child exec inheritance pipeline.
- `external\build\zig-musl-smoke-imagebase`: locally built ignored static ET_EXEC linked at `0x52000000`, SHA-256 `F9F1BD81B69B6C8C13A7E9CAE3DA9C24E45B76124195F7D945F97A7B9BE0F50B`, staged at `/APPS/SMOKE` by the optional extra-app path and verified by M27 with `linux /APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'`. This is a generic staging/VFS/exec proof, not a third-party package proof.
- `external\build\sbase-0.1-echo-x86_64-musl-0x52000000`: suckless sbase 0.1 `echo` built from upstream source package `https://dl.suckless.org/sbase/sbase-0.1.tar.gz` with tarball SHA-256 `86F6BB67BCC7DF3BA7A3F11DA72EAEB2CF58C23E9A35A7DBCD316395D934C634`, static musl ET_EXEC linked at `0x52000000`, SHA-256 `35DA6DB9DCF1C7E76AE605EAE175318831BCD10BBBF406D5A30A600D2AE4B667`, staged at `/APPS/SBECHO`, verified directly with `linux /APPS/SBECHO m28-sbase-direct`, and verified through forked BusyBox ash child exec with `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'`.
- `external\build\DYNGETPID`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001060`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `7BC91398FC2CABE11FB067376D417E5DF8057DACF21214E454A4D9AFA62223CB`, staged through the boot-media Linux app path as `/APPS/DYNGETPID`, and verified by M84 with `linux /APPS/DYNGETPID`.
- `external\build\DYNHELPER`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `B30ABD53CF32C9C9E6BEB77FF89DE02C57E1442D15A13EB944D514B794B8A9C8`, staged through the boot-media Linux app path as `/APPS/DYNHELPER`, and verified by M85 with `linux /APPS/DYNHELPER`.
- `external\build\DYNENVSTDIO`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `7713DA42475439C1020A7AF932B9C53983CAF0FC8DB41BFD78BF3F701DFBEBFA`, staged through the boot-media Linux app path as `/APPS/DYNENVSTDIO`, and verified by M86 with `linux /APPS/DYNENVSTDIO`.
- `external\build\DYNHEAPENV`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010D0`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `5482AF5167968986BE620CD86AE88385C49D0FCF0F435DF987727C1530AAA463`, staged through the boot-media Linux app path as `/APPS/DYNHEAPENV`, and verified by M87 with `linux /APPS/DYNHEAPENV`.
- `external\build\DYNTHREAD`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `749582EF277B19EE11795928199A941E4BD5E81D502B7F2B60104A30115B894A`, staged through the boot-media Linux app path as `/APPS/DYNTHREAD`, and verified by M88 with `linux /APPS/DYNTHREAD`.
- `external\build\DYNPTLS`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010E0`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `47D6FCCA829DE00FE42814155DF06C5EE925FCACC00A20CEF49C95067E6D8A6F`, staged through the boot-media Linux app path as `/APPS/DYNPTLS`, and verified by M89 with `linux /APPS/DYNPTLS`.
- `external\build\DYNFILEIO`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `42B199D95374ADC5F8F349405423E2F6976394AD8FD4DC04F88DD3E56F56354F`, staged through the boot-media Linux app path as `/APPS/DYNFILEIO`, and verified by M90 with `linux /APPS/DYNFILEIO`.
- `external\build\DYNSEEK`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010B0`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `22F4206B5DFA3A9048A08F2FF31E9931AC969DFC404EA684F2E7B0D0013AEE62`, staged through the boot-media Linux app path as `/APPS/DYNSEEK`, and verified by M91 with `linux /APPS/DYNSEEK`.
- `external\build\DYNVEC`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `2F4CB3560E98FF4585731F48673C5B86C2D2B82CD8D8B2D4284E9F5E6BD49915`, staged through the boot-media Linux app path as `/APPS/DYNVEC`, and verified by M94 with `linux /APPS/DYNVEC`.
- `external\build\DYNFSTATAT`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001070`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `35504B625F60B8C4DAAF464B57219466AA49CABCCDC0082F6907505C1C1DE8A0`, staged through the boot-media Linux app path as `/APPS/DYNFSTATAT`, and verified by M95 with `linux /APPS/DYNFSTATAT`.
- `external\build\DYNOPENAT`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `C28EC4FA1D26912A31B36591F9FCE73E07D086FC3F80CB3C4514C1AC374AD7BB`, staged through the boot-media Linux app path as `/APPS/DYNOPENAT`, and verified by M96 with `linux /APPS/DYNOPENAT`.
- `external\build\DYNODIR`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `43B960D929BC6E15B573E65166FEB2EDFFDBBC2573BE19372697056CDACA0E23`, staged through the NVMe FAT extra-app path as `/APPS/DYNODIR`, and verified by M97 with `linux /APPS/DYNODIR`.
- `external\build\DYNFCHDIR`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `0F4B1A7B2D001568C801B7930D8BD4F9F9ED6BE4EA239AB022AE59D4CEA48E08`, staged through the NVMe FAT extra-app path as `/APPS/DYNFCHDIR`, and verified by M98 with `linux /APPS/DYNFCHDIR`.
- `external\build\DYNFCNTL`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `D20CFF658A3E96FF67F1915943D7948207A4ADDDA649905FFB80F9B5E596F5E3`, staged through the NVMe FAT extra-app path as `/APPS/DYNFCNTL`, and verified by M99 with `linux /APPS/DYNFCNTL`.
- `external\build\DYNFDUP`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `7401F71B8B9740DE364BDC9CA3ED931C7D751F9619432AD4C9F9171EDF159D22`, staged through the NVMe FAT extra-app path as `/APPS/DYNFDUP`, and verified by M100 with `linux /APPS/DYNFDUP`.
- `external\build\DYNDUP`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `FBD8FBAFC838370E6DF112630F661E76C6AC988FF83D1A3024DAA04C936E5A7E`, staged through the NVMe FAT extra-app path as `/APPS/DYNDUP`, and verified by M101 with `linux /APPS/DYNDUP`.
- `external\build\DYNPIPE`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `4DCE063D838B8E88CABB6E00C3A0206FCDD1AF8FBEA46F4F2ABE1A79193F2FB1`, staged through the NVMe FAT extra-app path as `/APPS/DYNPIPE`, and verified by M102 with `linux /APPS/DYNPIPE`.
- `external\build\DYNFORKPIPE`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010A0`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `C3271AC8E598AEEF0480FE042FA1E6A51B64319B46736510D0FC2A43E154C696`, staged through the NVMe FAT extra-app path as `/APPS/DYNFORKPIPE`, and verified by M103 with `linux /APPS/DYNFORKPIPE`.
- `external\build\DYNFORKPIPE`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `0D9E3DDCC388F9609BCF31BCCBF646D8C3DFFF0D65CE040642102206EB87FCC2`, staged through the NVMe FAT extra-app path as `/APPS/DYNFORKPIPE`, and verified by M104 with the parent-read-before-wait `linux /APPS/DYNFORKPIPE` blocked pipe replay smoke.
- `external\build\DYNPIPECLOSE`: external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, `PT_INTERP` `/nvme/apps/ldlimit`, `DT_NEEDED` `libc-x64.so`, SHA-256 `93986327A92D798F613F931D93F483CCED8DED838BE78BF7D65324F5FCA2C628`, staged through the NVMe FAT extra-app path as `/APPS/DYNPIPECLOSE`, and verified by M105 with `linux /APPS/DYNPIPECLOSE`.
- `external\build\sbase-0.1-cat-x86_64-musl-0x52000000`: suckless sbase 0.1 `cat` built from upstream source package `https://dl.suckless.org/sbase/sbase-0.1.tar.gz` with tarball SHA-256 `86F6BB67BCC7DF3BA7A3F11DA72EAEB2CF58C23E9A35A7DBCD316395D934C634`, static musl ET_EXEC linked at `0x52000000`, SHA-256 `FDC39F6D97F7E7492DAE5983732B2E23FD063CC7EAC99C5F0114FD93E6A95662`, staged at `/APPS/SBCAT`, verified directly with `linux /APPS/SBCAT /nvme/apps/data/file.txt`, and verified through forked BusyBox ash child exec with `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'`.
- `external\build\sbase-0.1-env-x86_64-musl-0x52000000`: suckless sbase 0.1 `env` built from upstream source package `https://dl.suckless.org/sbase/sbase-0.1.tar.gz` with tarball SHA-256 `86F6BB67BCC7DF3BA7A3F11DA72EAEB2CF58C23E9A35A7DBCD316395D934C634`, static musl ET_EXEC linked at `0x52000000`, SHA-256 `A678597A247CCEAEDE00641B88497BD51F684FA29072A3192ADCAABB4ABA54F4`, staged at `/APPS/SBENV`, and verified through forked BusyBox ash child exec with `linux /APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'`.

Proven syscall surface for those artifacts:

- Linux process startup: `brk`, `mmap`, `mprotect`, `munmap`, `arch_prctl`, `set_tid_address`, `rt_sigaction`, and `rt_sigprocmask`.
- Console and file I/O: `read`, `write`, `readv`, `writev`, `openat`, and `close`; `open`, `openat`, stat-family paths, and `chdir` now canonicalize relative paths against the Linux persona cwd before VFS resolution.
- Readiness: `poll` currently proves brokered stdout `POLLOUT` readiness for dynamic ET_EXEC programs with passing M94 telemetry `poll 1 poll-ready 1 poll-last-revents 0x00000004`; file `POLLIN`, sockets, epoll, and broad terminal readiness are not claimed.
- Metadata and offsets: `stat`, `fstat`, `newfstatat`, including `AT_EMPTY_PATH`, `lstat`, and `lseek` on seekable file-like fds.
- Descriptor flags and duplication: `fcntl(72)` currently proves `F_GETFD`, `F_SETFD` with `FD_CLOEXEC`, `F_GETFL`, and the supported `F_SETFL` subset for `O_NONBLOCK` on real VFS fds, with passing M99 telemetry `fcntl 6 fcntl-denial 0`. M100 additionally proves `F_DUPFD` through the dynamic `fcntl` wrapper on a real VFS fd, with passing telemetry `fcntl 3 fcntl-denial 0 read 1 read-bytes 27`, duplicate descriptors not inheriting `FD_CLOEXEC`, and VFS sidecar metadata preserved across duplication. M101 proves direct generated libc wrappers for `dup(32)`, `dup2(33)`, and `dup3(292)` on brokered stdout and real VFS fds, with passing telemetry `dup 2 dup2 1 dup3 1 dup-denial 0 fcntl 1 fcntl-denial 0 read 3 read-bytes 81`.
- Pipe fd pairs: `pipe(22)` was first proven by static BusyBox ash pipeline children in M24. M102 proves the direct generated dynamic libc wrapper for `pipe(22)` in a single dynamic ET_EXEC process, with passing telemetry `pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 read 1 read-bytes 5 write 2 write-bytes 19 pipe-live-final 0`.
- Dynamic fork/wait composition: M103 proves generated dynamic libc wrappers for `fork(57)` and `wait4(61)` with a pipe fd pair inherited into a child process, with passing telemetry `fork 1 fork-success 1 fd-fork-pipe-copy 2 wait4 1 wait4-reap 1 wait4-last-exit-code 7 pipe-live-final 0 root-cleanup 2 pml4-pool-used-final 0`. M104 proves blocked pipe reads replay correctly after wake with `pipe-blocks 1 pipe-wakes 1 pipe-replays 1 read 1 read-bytes 10 wait4-reap 1 pipe-live-final 0`.
- Proc symlink readback: `readlink`, with passing `/proc/self/exe` telemetry `readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14`, visible output `lrwxrwxrwx    1        14 /proc/self/exe -> /proc/self/exe`, and `unimplemented 0`; passing `/proc/self/fd` telemetry `getdents64 2 getdents64-entries 3 getdents64-bytes 72 stat 4 stat-denial 0 stat-fault 0 readlink 3 readlink-bytes 33 readlink-denial 0 readlink-fault 0 readlink-last-result 11`, with visible links for `0 -> anon:[fd 0]`, `1 -> anon:[fd 1]`, and `2 -> anon:[fd 2]`; and passing mixed `/proc/self` telemetry `getdents64 2 getdents64-entries 6 getdents64-bytes 168 stat 7 stat-denial 0 stat-fault 0 readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 writev 6 writev-bytes 209`, with visible entries for `environ`, `cmdline`, `status`, `fd`, `exe -> /proc/self/exe`, and `maps`.
- Directory enumeration: `getdents64`, with passing telemetry `getdents64 2 getdents64-entries 2 getdents64-bytes 56`, and visible output `busybox` plus `data`.
- Current working directory: `getcwd` and `chdir`, with passing shell-loop telemetry `getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 chdir 2 fchdir 0 chdir-denial 0 chdir-fault 0`, and visible output `/`, `/nvme/apps`, and `/`.
- Relative path canonicalization: `cat nvme/apps/./data/../data/file.txt`, with passing telemetry `path-relative 1 path-dot 1 path-dotdot 1 path-fault 0`, visible output `Nested FAT32 path fixture`, and NVMe VFS telemetry `vfs-nvme-reads 2 vfs-nvme-bytes 27`; plus `ls -l nvme/apps/./data/..`, with passing telemetry `console-bytes 65`, `getdents64 2 getdents64-entries 2 getdents64-bytes 56`, `stat 3 stat-denial 0 stat-fault 0`, `path-relative 4 path-dot 4 path-dotdot 4 path-fault 0`, `writev 2 writev-bytes 65`, `ioctl 1 ioctl-tty 1`, `vfs-nvme-readdirs 4 vfs-nvme-dirents 2`, and visible long-listing output `-r--r--r--    1    145264 busybox` plus `dr-xr-xr-x    2         0 data`.
- Terminal queries: `ioctl(TIOCGWINSZ)` for brokered terminal fds, with fixed 25x80 `winsize` telemetry `ioctl-tty`.
- Identity shims proven by BusyBox `sh`: `geteuid` returns fixed uid `1000`, and `getppid` returns fixed ppid `1`.
- Process-name shim proven by standalone-shell BusyBox: `prctl(PR_GET_NAME)` and `prctl(PR_SET_NAME)` maintain the 16-byte Linux comm field used by BusyBox applet dispatch; other `prctl` options remain `ENOSYS`.
- Process creation and reaping: bounded `fork(57)` full-copies the current Linux persona process root, lower-half VMA/address-space mappings, FD table, persona state, VFS state, and audit state for the observed BusyBox ash external-command and pipeline paths; the child returns 0, the parent receives the child pid, blocking `wait4(61)` reaps the child exit status, and all PML4 slots are returned to the static pool. Broad `clone` flags, `vfork`, COW, threading, and signal delivery remain outside this gate.
- Pipes: `pipe(22)` creates brokered read/write fd pairs backed by the existing fixed pipe pool, and fork preserves exact pipe fd numbers in children. `PIPE64_MAX_OBJECTS` remains 16 and `PIPE64_BUFFER_BYTES` remains 4096. `pipe2(293)` behavior is unchanged.
- Exec and path aliases: `execve` can load a real static ET_EXEC binary through the Linux VFS path resolver using a UEFI-only 2 MiB staging buffer and a 64 KiB real-exec user stack; the older synthetic 4 KiB exec stack constants remain reserved for regression tests. Read-only BusyBox applet aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin` resolve to the staged NVMe FAT `/APPS/BUSYBOX` binary through the Linux-visible `/nvme/apps/busybox` backend path. `/usr` exposes fixed `bin` and `sbin` entries. M26 proves child processes created by BusyBox ash can inherit cwd, VFS authority, and pipe fds across fork, then replace themselves through `execve` and return cleanly to the parent shell via `wait4`. M27 proves the same child `execve` path can load an independently staged static ET_EXEC at `/APPS/SMOKE` through `/nvme/apps/smoke`, distinct from the BusyBox alias backend. M28 proves an upstream non-BusyBox package utility can be staged at `/APPS/SBECHO`, loaded directly, resolved through `/nvme/apps/sbecho`, fork/execed by BusyBox ash, piped to `/bin/cat`, and cleaned up. M29 proves a second upstream non-BusyBox package utility can be staged at `/APPS/SBCAT`, read a real NVMe FAT file directly, and consume a pipe from `/nvme/apps/sbecho` with `vfs-bin-alias 0`. M34 proves an upstream non-BusyBox `env` utility can be staged at `/APPS/SBENV`, execed by BusyBox ash, and observe shell-mutated exported environment content. M36 adds a bounded `/usr/local/bin/sbenv` alias backed by `/nvme/apps/sbenv` and proves non-shell third-party `execvp` PATH search through that alias. M37 proves BusyBox ash and the non-shell `sbenv` process can both resolve third-party utilities by PATH name in the same pipeline. M38 proves the expanded `/usr/local/bin` namespace enumerates all three third-party aliases with staged backends and no stat denials. M39 proves BusyBox ash and non-shell `execvp` can execute the same third-party aliases through absolute `/usr/local/bin/...` paths. M40 proves absolute `/usr/local/bin/sbcat` can read real NVMe FAT file content and pipe it to another absolute-localbin `sbcat` child. M41 proves absolute-localbin children inherit BusyBox ash cwd and can read cwd-relative NVMe file paths. M42 proves the same path handles `..` segment normalization in forked third-party children. M43 proves `execve` canonicalizes relative executable paths before VFS reads, allowing current-directory `./sbcat` execution from `/usr/local/bin`. M44 proves the same current-directory relative executable path works when a third-party `sbenv` process performs its own explicit `./sbenv` handoff. M45 proves relative executable `..` segments from cwd `/usr/local/bin` normalize back into `/usr/local/bin` for both shell-launched children and non-shell handoffs. M46 proves absolute executable `..` segments in `/usr/local/bin/../bin/...` normalize back into `/usr/local/bin` without a cwd change. M47 proves mixed absolute executable `.` and `..` segments in `/usr/local/./bin/../bin/...` normalize back into `/usr/local/bin` for both shell-launched children and a non-shell `sbenv` handoff. M48 proves repeated slashes in `/usr//local/bin/...` and `/usr/local//bin/...` normalize back into `/usr/local/bin` for both shell-launched children and a non-shell `sbenv` handoff. M49 proves bounded upward `..` traversal in `/usr/local/bin/../../local/bin/...` normalizes back into `/usr/local/bin` for both shell-launched children and a non-shell `sbenv` handoff. M50 proves over-root `..` traversal in `/../../usr/local/bin/...` and `/../../../usr/local/bin/...` clamps at `/` and normalizes into `/usr/local/bin` for both shell-launched children and a non-shell `sbenv` handoff. M51 proves trailing slash intent is preserved across execve canonicalization so `/usr/local/bin/sbenv/` is denied as a non-directory executable target instead of being normalized into `/usr/local/bin/sbenv`. M52 proves `/usr/local/bin/` remains enumerable as a directory with a terminal slash. M53 proves a missing absolute-localbin executable path reports `not found` and increments alias-denial telemetry without unintended backend reads. M54 proves BusyBox ash default-PATH lookup reports the same `not found` result for a missing command while the bounded alias tables reject each searched directory without unintended executable backend reads. M55 proves a non-shell third-party `sbenv` process gets the same clean `ENOENT` result from its own `execvp("sbmissing")` path while only valid `sbenv` and `sbcat` executable bytes are read. M56 proves that same non-shell path preserves trailing-slash executable intent and reports `ENOTDIR` for `/usr/local/bin/sbenv/` without reading it as a file. M57 proves a non-shell `execvp` attempt against `/usr/local/bin/` is denied visibly with no unintended directory-as-binary read. M58 proves the same denial for bare `/usr/local/bin` without terminal-slash intent. M59 proves `/usr/local/bin/.` dot-segment canonicalization still rejects a directory target before executable backend read. M60 proves `/usr/local/bin/..` parent-segment canonicalization still rejects a parent directory target before executable backend read. M61 proves `/usr/local/bin/../bin` parent-rebased canonicalization still rejects the rebased `/usr/local/bin` directory target before executable backend read. Dynamic linking, broad interpreter lookup, and arbitrary distro `/bin` population remain outside this gate.
- Scheduler TLS context: Linux task FS base is now scheduler-owned on UEFI Product. `arch_prctl(ARCH_SET_FS)` updates the running task, fork/clone seed child task FS base from persona TLS state, exec transfer clears stale FS before the new image initializes TLS, and task switches save/restore the IA32_FS_BASE MSR. M27 telemetry proves this with `fs-save 3 fs-restore 4 fs-set 7` and `page-faults 0`.
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

## M23 Fork/Wait Gate

M23 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'ls /nvme/apps; ls /nvme/apps/data'`. This is the previously traced BusyBox ash external-command boundary: BusyBox issues syscall `57` at `fork-last-rip 0x000000005200EF74`, the parent receives a child pid, the child runs the command under a distinct PML4 slot, `wait4(61)` collects exit code 0, and both parent and child process roots are cleaned up.

Visible console output:

```text
busybox  data
file.txt
```

M23 implementation delta: 19 kernel/header files changed with 2,112 insertions and 33 deletions; the prerequisite QMP keyboard semicolon injector changed 1 verifier line, for 20 total changed files with 2,113 insertions and 33 deletions. Final reserves are UEFI 840,160 bytes and BIOS 101 sectors.

M23 acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204C000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 1 syscall-root-reload 61 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 2 task 0 started 1 console-bytes 23 exit 0 cleanup 1 getdents64 4 getdents64-entries 3 getdents64-bytes 88 stat 5 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 readv 0 readv-bytes 0 writev 2 writev-bytes 23 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 4 ioctl-tty 4 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 4 prctl-set-name 3 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 1 fork-success 1 fork-enosys 0 fork-denial 0 fork-child-slot 1 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 1 wait4-reap 1 wait4-last-exit-code 0 child-root-cleanup 1 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 7 vfs-nvme-dirents 3 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M24 Pipe Gate

M24 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'echo hello | cat'`. BusyBox ash forks both pipeline sides, so the correct proof shape is `fork 2 fork-success 2` and `fd-fork-pipe-copy 3`: the producer and consumer children both inherit pipe-related state, the producer writes `hello\n`, the consumer reads and writes it to the brokered console, and the parent reaps both children.

Visible console output:

```text
hello
```

M24 implementation scope: Linux `pipe(22)` ABI exposure over the existing fixed pipe provider, exact-fd pipe endpoint inheritance in `fd64_fork_process`, `pipe64_grant_endpoint_at` for fork semantics, QMP `|` keyboard injection, and real-binary telemetry for pipe calls, pipe denials/faults, live pipe count, pipe provider denials, and fd fork pipe copies. `pipe2(293)` observable behavior remains unchanged. `PIPE64_MAX_OBJECTS` remains 16 and `PIPE64_BUFFER_BYTES` remains 4096.

Final reserves are UEFI 836,064 bytes and BIOS 101 sectors. The passing artifact is `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, static musl ET_EXEC linked at `0x52000000`.

M24 acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204D000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 60 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 6 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 6 write 2 write-bytes 12 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M25 Linux VFS Path Execution Gate

M25 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'` and `linux /APPS/BUSYBOX sh -c 'ls /usr/bin'`. The first command proves BusyBox ash PATH search can find a Linux-visible executable alias backed by the real staged NVMe BusyBox binary, load it with `execve`, transfer control to the new static ET_EXEC image, and return with exit status 0. The second command proves `/usr/bin` is a directory in the Linux VFS with stable applet entries and no stat denials.

Visible `/usr/bin` console output:

```text
sh       true     ls       cat      echo     busybox
```

M25 implementation scope: UEFI-only real-binary `execve` widening over the Linux VFS, a 64 KiB real-exec stack for real external ET_EXEC images, consume-once syscall return-frame transfer to the new ELF entry/RSP, release and cleanup handoff for exec-replaced persona state, read-only BusyBox aliases under `/bin`, `/sbin`, `/usr/bin`, and `/usr/sbin`, fixed `/usr` directory entries, and `vfs-bin-*` plus `execve-*` telemetry. It does not add dynamic linking, glibc support, vfork, broad clone flags, threading, signal delivery, sockets, broad ioctl/device control, or arbitrary host-package filesystem population.

Final reserves are UEFI 831,968 bytes and BIOS 101 sectors. The passing artifact is `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, static musl ET_EXEC linked at `0x52000000`. The final implementation delta before docs is 8 files changed with 751 insertions and 34 deletions.

M25 `/usr/bin` acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 41 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 53 exit 0 cleanup 1 getdents64 2 getdents64-entries 6 getdents64-bytes 152 stat 7 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 pipe 0 pipe-create 0 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 0 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 4294967295 readv 0 readv-bytes 0 writev 1 writev-bytes 53 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 2 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 0 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 22 execve-last-binary-bytes 0 execve-last-closed-fds 0 execve-last-fd-live-before 0 execve-last-fd-live-after 0 execve-last-vma-before 0 execve-last-vma-released 0 execve-last-vma-after 0 execve-last-argc 0 execve-last-envc 0 execve-last-transfer-ready 0 execve-last-transfer-rip 0x0000000000000000 execve-last-transfer-rsp 0x0000000000000000 fork 0 fork-success 0 fork-enosys 0 fork-denial 0 fork-child-slot 4294967295 fork-child-root-distinct 0 fork-last-rip 0x0000000000000000 wait4 0 wait4-reap 0 wait4-last-exit-code 45 child-root-cleanup 0 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0 vfs-bin-alias 6 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

M25 path-search proof summary: `linux /APPS/BUSYBOX sh -c 'busybox echo m25-path-search'` prints `m25-path-search`, exits 0, reports `execve 1 execve-denial 0 execve-fault 0 execve-last-binary-bytes 145264 execve-last-transfer-ready 1 vfs-bin-alias 3 vfs-bin-read 1 vfs-bin-denial 0 stat-denial 0 page-faults 0 syscall-root-repair 0 pml4-pool-used-final 0`.

## M26 Forked Child Exec Inheritance Gate

M26 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /bin/cat data/file.txt | /bin/cat'`. This forces BusyBox ash to change cwd, create a pipe, fork both pipeline sides, have both children `execve` Linux-visible `/bin/cat` aliases backed by the real staged NVMe BusyBox binary, read a cwd-relative NVMe FAT file in the producer child, transfer the bytes through the inherited pipe endpoints, print through the brokered console in the consumer child, and have the parent reap both children.

Visible console output:

```text
Nested FAT32 path fixture
```

M26 implementation scope: no kernel code changes beyond M25 were needed. The milestone is an evidence gate proving that the M25 exec transfer composes with the M22 process-root pool, M23 fork/wait state duplication, M24 pipe fd inheritance, Linux cwd state, and the NVMe VFS provider.

Final reserves remain UEFI 831,968 bytes and BIOS 101 sectors. The passing artifact is `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, SHA-256 `15ACD328B182BB8CA23133AFA36DD9BB0ECBD607E551EBFE2F7E13DB3A8283F2`, static musl ET_EXEC linked at `0x52000000`.

M26 acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 0 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 4 prctl-set-name 1 prctl-get-name 3 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 2 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE60 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 4 vfs-bin-open 0 vfs-bin-read 2 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M27 Independently Staged Static ET_EXEC Gate

M27 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/nvme/apps/smoke | /bin/cat'`. The command stages `/APPS/SMOKE` as an optional extra app, resolves it as `/nvme/apps/smoke`, forks both BusyBox ash pipeline children, execs SMOKE in the producer child, execs `/bin/cat` in the consumer child, moves SMOKE output through the pipe, and reaps both children.

Visible console output:

```text
zig-musl-smoke
```

M27 final reserves are UEFI 831,968 bytes and BIOS 101 sectors. The first failure was a page fault at BusyBox `mov %fs:0x0` after a SMOKE child exec changed IA32_FS_BASE and another task resumed without FS-base restore. The fix is scheduler-owned FS-base save/restore/set state, with task seeding on fork/clone and stale-FS clearing on exec transfer.

M27 acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 66 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 15 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 15 write 2 write-bytes 30 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 1 prctl-get-name 2 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 2 vfs-bin-open 0 vfs-bin-read 1 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M28 Third-Party Non-BusyBox Package Gate

M28 is accepted on the UEFI Product path with `linux /APPS/SBECHO m28-sbase-direct` and `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m28-sbase-pipeline | /bin/cat'`. The artifact is the upstream suckless sbase 0.1 `echo` utility, built outside the repo source tree by the local musl cross toolchain from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`.

Visible direct output:

```text
m28-sbase-direct
```

Visible forked pipeline output:

```text
m28-sbase-pipeline
```

The sbase source tarball SHA-256 is `86F6BB67BCC7DF3BA7A3F11DA72EAEB2CF58C23E9A35A7DBCD316395D934C634`. The staged `/APPS/SBECHO` artifact SHA-256 is `35DA6DB9DCF1C7E76AE605EAE175318831BCD10BBBF406D5A30A600D2AE4B667`, byte count 46,952. `readelf -h -l` reports `Type: EXEC`, entry `0x520010d1`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`.

M28 also fixed a cleanup-accounting bug exposed by sbase: `echo` calls `fclose(stdout)` before exit, so the previous final cleanup invariant falsely required three fd releases after the program had already closed fd 1. The invariant now proves that the process fd table is detached after cleanup instead of assuming all three stdio fds remain live until exit.

Final reserves remain UEFI 831,968 bytes and BIOS 101 sectors.

M28 forked pipeline acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 68 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 19 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 19 write 1 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 19 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 3 prctl-set-name 1 prctl-get-name 2 prctl-enosys 0 prctl-last-option 0x00000010 prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 145264 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x0000000052010497 execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 145264 vfs-bin-alias 2 vfs-bin-open 0 vfs-bin-read 1 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M29 Broader Third-Party Utility Read Path Gate

M29 is accepted on the UEFI Product path with `linux /APPS/SBCAT /nvme/apps/data/file.txt` and `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbecho m29-sbase-pipe | /nvme/apps/sbcat'`. The new artifact is the upstream suckless sbase 0.1 `cat` utility, built outside the repo source tree by the local musl cross toolchain from `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`.

Visible direct output:

```text
Nested FAT32 path fixture
```

Visible two-executable pipeline output:

```text
m29-sbase-pipe
```

The sbase source tarball SHA-256 is `86F6BB67BCC7DF3BA7A3F11DA72EAEB2CF58C23E9A35A7DBCD316395D934C634`. The staged `/APPS/SBCAT` artifact SHA-256 is `FDC39F6D97F7E7492DAE5983732B2E23FD063CC7EAC99C5F0114FD93E6A95662`, byte count 36,968. `readelf -h -l` reports `Type: EXEC`, entry `0x5200117f`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`.

M29 also extends the host-side NVMe image and verifier staging path with a second optional extra-app slot so `/APPS/SBECHO` and `/APPS/SBCAT` can be staged together with independent provenance metadata. The implementation changes are host tooling only; final reserves remain UEFI 831,968 bytes and BIOS 101 sectors.

The direct `/APPS/SBCAT` proof reports `console-bytes 27 exit 0 cleanup 1 read 2 read-bytes 27 write 1 write-bytes 27 vfs-nvme-reads 2 vfs-nvme-bytes 27 pml4-pool-used-final 0 unimplemented 0 page-faults 0`.

M29 two-executable pipeline acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224E000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 67 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 15 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 15 write 1 write-bytes 15 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 15 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE70 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M30 Third-Party PATH Directory Proof

M30 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'PATH=/usr/local/bin:/bin:/usr/bin; sbecho m30-path-pipe | sbcat'`. The command stages upstream suckless sbase 0.1 `echo` and `cat` artifacts as `/APPS/SBECHO` and `/APPS/SBCAT`, exposes them as Linux-visible `/usr/local/bin/sbecho` and `/usr/local/bin/sbcat`, lets BusyBox ash find both utilities by PATH name, forks both pipeline sides, execs both third-party ET_EXEC children, moves bytes through the pipe, and reaps both children.

Visible PATH pipeline output:

```text
m30-path-pipe
```

The source tarball remains `https://dl.suckless.org/sbase/sbase-0.1.tar.gz`, SHA-256 `86F6BB67BCC7DF3BA7A3F11DA72EAEB2CF58C23E9A35A7DBCD316395D934C634`. The staged `/APPS/SBECHO` artifact SHA-256 is `35DA6DB9DCF1C7E76AE605EAE175318831BCD10BBBF406D5A30A600D2AE4B667`, byte count 46,952. The staged `/APPS/SBCAT` artifact SHA-256 is `FDC39F6D97F7E7492DAE5983732B2E23FD063CC7EAC99C5F0114FD93E6A95662`, byte count 36,968.

M30 extends the bounded Linux VFS with fixed `/usr/local` and `/usr/local/bin` directory entries plus `sbecho` and `sbcat` aliases backed by the existing NVMe FAT `/nvme/apps/sbecho` and `/nvme/apps/sbcat` providers. It also extends real-binary telemetry with `vfs-localbin-*` counters and fixes the QMP keyboard map for uppercase, `=`, and `:` so PATH-setting commands are injected faithfully. Because uppercase FAT-style `/APPS/BUSYBOX` now reaches the guest as uppercase text, the shell launcher canonicalizes only Linux `argv[0]` to lowercase while preserving the actual NVMe read path; this keeps BusyBox multicall dispatch compatible with the documented Product command.

Supporting directory control: `linux /APPS/BUSYBOX ls /usr/local/bin` prints `sbcat   sbecho` and reports `getdents64 2 getdents64-entries 2 stat 3 stat-denial 0 vfs-localbin-alias 2 vfs-localbin-denial 0 page-faults 0`.

Final reserves are UEFI 827,872 bytes and BIOS 101 sectors. The implementation delta before docs is 5 files changed with 283 insertions and 13 deletions.

M30 PATH pipeline acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 14 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 14 write 1 write-bytes 14 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 14 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 1 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE80 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M31 Default Linux Environment Proof

M31 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbecho m31-default-path | sbcat'`. The launcher now seeds a bounded default initial Linux environment containing `PATH=/usr/local/bin:/bin:/usr/bin`; BusyBox ash receives that environment on its initial stack, resolves the third-party `sbecho` and `sbcat` names without command-local PATH assignment, forks both pipeline sides, and passes the environment through the forked child `execve` calls.

Visible default-PATH pipeline output:

```text
m31-default-path
```

M31 adds no new syscall surface and no new staged artifacts. The staged `/APPS/SBECHO` artifact remains SHA-256 `35DA6DB9DCF1C7E76AE605EAE175318831BCD10BBBF406D5A30A600D2AE4B667`, byte count 46,952; the staged `/APPS/SBCAT` artifact remains SHA-256 `FDC39F6D97F7E7492DAE5983732B2E23FD063CC7EAC99C5F0114FD93E6A95662`, byte count 36,968. The implementation adds one static default `envp` entry in the UEFI-only launcher path and reports the actual initial stack environment count as `envc`.

Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The implementation delta before docs is 1 file changed with 9 insertions and 2 deletions.

M31 default-environment acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 1 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 17 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 17 write 1 write-bytes 17 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 17 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 2 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE50 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M32 Bounded Shell Environment Polish

M32 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbecho $USER:$PWD | sbcat'`. The launcher now seeds four fixed initial Linux environment entries: `PATH=/usr/local/bin:/bin:/usr/bin`, `HOME=/`, `USER=limitless`, and `PWD=/`. The verifier also learned to inject a literal `$` through QMP, so shell variable expansion is tested instead of being host-expanded or dropped.

Visible shell-expansion pipeline output:

```text
limitless:/
```

M32 adds no syscall surface and no new staged artifacts. The staged `/APPS/SBECHO` artifact remains SHA-256 `35DA6DB9DCF1C7E76AE605EAE175318831BCD10BBBF406D5A30A600D2AE4B667`, byte count 46,952; the staged `/APPS/SBCAT` artifact remains SHA-256 `FDC39F6D97F7E7492DAE5983732B2E23FD063CC7EAC99C5F0114FD93E6A95662`, byte count 36,968.

Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The implementation delta before docs is 2 files changed with 9 insertions and 2 deletions.

M32 shell-environment acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 12 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 12 write 1 write-bytes 12 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 12 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M33 Cwd And PWD Synchronization Proof

M33 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; sbecho $PWD | sbcat'`. No kernel, verifier, or artifact changes were required after M32; the trace proved that the existing `chdir`, Linux persona cwd, BusyBox ash `PWD` update, forked child `execve` environment inheritance, PATH lookup, and pipe paths already compose.

Visible cwd/PWD pipeline output:

```text
/nvme/apps
```

M33 adds no syscall surface and no new staged artifacts. The staged `/APPS/SBECHO` artifact remains SHA-256 `35DA6DB9DCF1C7E76AE605EAE175318831BCD10BBBF406D5A30A600D2AE4B667`, byte count 46,952; the staged `/APPS/SBCAT` artifact remains SHA-256 `FDC39F6D97F7E7492DAE5983732B2E23FD063CC7EAC99C5F0114FD93E6A95662`, byte count 36,968.

Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The implementation delta before docs is zero files changed.

M33 cwd/PWD acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 11 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 11 write 1 write-bytes 11 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 1 writev-bytes 11 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M34 Environment Mutation And Export Proof

M34 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'USER=operator; export USER; /nvme/apps/sbenv | /nvme/apps/sbcat'`. The command stages upstream suckless sbase 0.1 `env` as `/APPS/SBENV` and `cat` as `/APPS/SBCAT`; BusyBox ash mutates and exports `USER`, forks both pipeline children, execs both third-party ET_EXEC binaries from NVMe FAT, lets the `sbenv` child inspect its inherited environment, moves that output through the pipe, and reaps both children.

Visible environment pipeline output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

The `sbenv` artifact is built from upstream source package `https://dl.suckless.org/sbase/sbase-0.1.tar.gz` with source tarball SHA-256 `86F6BB67BCC7DF3BA7A3F11DA72EAEB2CF58C23E9A35A7DBCD316395D934C634`. The staged `/APPS/SBENV` artifact SHA-256 is `A678597A247CCEAEDE00641B88497BD51F684FA29072A3192ADCAABB4ABA54F4`, byte count 52,616. `readelf -h -l` reports `Type: EXEC`, entry `0x5200105b`, four `PT_LOAD` segments beginning at `0x52000000`, no `PT_INTERP`, and no `PT_DYNAMIC`.

Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The implementation delta before docs is zero kernel/verifier files changed; the only new local artifact is the ignored external `sbenv` build.

M34 environment mutation acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 69 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 0 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M35 Third-Party Exec Handoff Proof

M35 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator /nvme/apps/sbenv | /nvme/apps/sbcat'`. The command has BusyBox ash fork the pipeline, exec the first `sbenv` child, then has that non-shell third-party process call `execvp` on an explicit Linux VFS path to replace itself with a second real `sbenv` image while carrying `USER=operator`.

Visible third-party handoff output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M35 adds no syscall surface, no kernel changes, no verifier changes, and no new staged artifacts beyond the M34 `sbenv` and `sbcat` pair. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M35 third-party handoff acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 0 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M36 Third-Party PATH Execvp Proof

M36 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/nvme/apps/sbenv USER=operator sbenv | /nvme/apps/sbcat'`. The command has BusyBox ash fork the pipeline, exec the first `sbenv` child by explicit NVMe VFS path, then has that non-shell third-party process call `execvp("sbenv", ...)` so PATH search resolves a second third-party ET_EXEC through `/usr/local/bin/sbenv`.

Visible third-party PATH handoff output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M36 adds a single fixed Linux VFS alias, `/usr/local/bin/sbenv`, backed by `/nvme/apps/sbenv` and the staged NVMe FAT `/APPS/SBENV` artifact. It adds no syscall surface, dynamic linking, heap allocation, broad interpreter lookup, or arbitrary distro filesystem population. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M36 third-party PATH handoff acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 2 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M37 All-Third-Party PATH Pipeline Proof

M37 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator sbenv | sbcat'`. The command has BusyBox ash resolve both pipeline children through the default `PATH`, then has the first third-party `sbenv` process call `execvp("sbenv", ...)` so the replacement image is also resolved by PATH rather than an explicit `/nvme/apps` path.

Visible all-third-party PATH output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M37 adds no kernel, verifier, syscall, or artifact changes after M36. It proves the existing bounded `/usr/local/bin` aliases, default environment, fork, pipe, wait, and non-shell `execvp` paths compose when every child executable name is PATH-resolved. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M37 all-third-party PATH acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 74 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 8 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M38 Expanded Localbin Directory Proof

M38 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin'`. The first trace listed the three VFS aliases but failed `stat` on `sbecho` because the verifier could only stage two extra third-party artifacts at once. M38 extends the host NVMe image generator and QEMU verifier with a third optional extra-app slot, then proves the expanded bounded `/usr/local/bin` namespace is inspectable with all three real staged backends present.

Visible expanded-localbin output:

```text
sbenv   sbcat   sbecho
```

M38 adds no kernel, syscall, VFS, loader, or artifact changes. The host tooling now stages `/APPS/SBECHO`, `/APPS/SBENV`, and `/APPS/SBCAT` simultaneously for verifier runs that need the full third-party alias namespace. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M38 expanded-localbin acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 38 syscall-root-denial 0 fs-save 0 fs-restore 1 fs-set 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 23 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 96 stat 6 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 0 write-bytes 0 pipe 0 pipe-create 0 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 0 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 4294967295 readv 0 readv-bytes 0 writev 1 writev-bytes 23 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 2 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 0 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 22 execve-last-binary-bytes 0 execve-last-closed-fds 0 execve-last-fd-live-before 0 execve-last-fd-live-after 0 execve-last-vma-before 0 execve-last-vma-released 0 execve-last-vma-after 0 execve-last-argc 0 execve-last-envc 0 execve-last-transfer-ready 0 execve-last-transfer-rip 0x0000000000000000 execve-last-transfer-rsp 0x0000000000000000 fork 0 fork-success 0 fork-enosys 0 fork-denial 0 fork-child-slot 4294967295 fork-child-root-distinct 0 fork-last-rip 0x0000000000000000 wait4 0 wait4-reap 0 wait4-last-exit-code 45 child-root-cleanup 0 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 0 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M39 Absolute Localbin Exec Proof

M39 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv USER=operator /usr/local/bin/sbenv | /usr/local/bin/sbcat'`. The first trace was blocked before Linux execution because the persistent ring-3 shell accepted only 96 interactive bytes while the proof command is 106 bytes. M39 raises that runtime-shell command capacity to 128 bytes, matching the kernel `SHELL64_MAX_LINE_BYTES`, then proves the existing bounded localbin VFS aliases execute by absolute path.

Visible absolute-localbin output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M39 does not add Linux syscalls, loader behavior, VFS aliases, or third-party artifacts. It proves BusyBox ash can execute `/usr/local/bin/sbenv`, that `sbenv` can replace itself through absolute `/usr/local/bin/sbenv`, and that the pipeline consumer can execute `/usr/local/bin/sbcat`. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M39 absolute-localbin acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M40 Absolute Localbin File-Read Proof

M40 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt | /usr/local/bin/sbcat'`. A preliminary direct trace, `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbcat /nvme/apps/data/file.txt'`, also passed and printed the file content, but BusyBox ash optimized the single final command by replacing itself through `execve`, so the accepted proof uses a pipeline to force two forked child exec paths.

Visible absolute-localbin file-read output:

```text
Nested FAT32 path fixture
```

M40 adds no kernel, syscall, VFS, loader, host-tooling, or artifact changes after M39. It proves absolute `/usr/local/bin/sbcat` can read a real NVMe FAT file directly, and that two forked absolute-localbin `sbcat` children can pipe that file content and clean up all resources. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M40 absolute-localbin file-read acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE20 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M41 Localbin Cwd-Relative File-Read Proof

M41 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps; /usr/local/bin/sbcat data/file.txt | /usr/local/bin/sbcat'`. This keeps the executable paths absolute while making the input file path relative to BusyBox ash's changed cwd, proving cwd inheritance and relative file canonicalization inside forked third-party children.

Visible cwd-relative absolute-localbin output:

```text
Nested FAT32 path fixture
```

M41 adds no kernel, syscall, VFS, loader, host-tooling, or artifact changes after M40. It proves `chdir` state is inherited into absolute-localbin child exec paths, that the first `sbcat` resolves `data/file.txt` relative to `/nvme/apps`, and that the second `sbcat` consumes the pipe with no leaks or faults. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M41 cwd-relative absolute-localbin acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 2 path-dot 1 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M42 Localbin Dotdot Relative Path Proof

M42 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'cd /nvme/apps/data; /usr/local/bin/sbcat ../data/file.txt | /usr/local/bin/sbcat'`. This keeps the executable paths absolute, changes cwd into the data directory, and requires the first forked third-party child to normalize `..` before opening the real NVMe file.

Visible dotdot relative absolute-localbin output:

```text
Nested FAT32 path fixture
```

M42 adds no kernel, syscall, VFS, loader, host-tooling, or artifact changes after M41. It proves inherited cwd plus `..` path normalization works for forked third-party absolute-localbin children. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M42 dotdot relative absolute-localbin acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 2 path-dot 1 path-dotdot 1 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M43 Localbin Current-Directory Path Proof

M43 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ./sbcat /nvme/apps/data/file.txt | ./sbcat'`. The first trace failed with `sh: ./sbcat: Invalid argument`, `execve-denial 2`, `execve-last-error 22`, and `vfs-localbin-alias 0`, proving that child `execve` still read raw executable paths instead of canonicalizing them against cwd.

Visible current-directory localbin output after the fix:

```text
Nested FAT32 path fixture
```

M43 changes `kernel/arch/x86_64/linux_abi.c` so `execve` canonicalizes the copied user path before VFS stat/read, matching open/stat/chdir path behavior. Final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M43 current-directory localbin acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 3 path-dot 3 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 4 read-bytes 54 write 2 write-bytes 54 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 4 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M44 Non-Shell Current-Directory Execvp Proof

M44 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ./sbenv USER=operator ./sbenv | ./sbcat'`. It required no code changes after M43 because `execve` canonicalization already covers BusyBox ash launching `./sbenv`, the first `sbenv` replacing itself with another `./sbenv`, and the pipeline consumer launching `./sbcat`.

Visible non-shell current-directory execvp output:

```text
USER=operator
HOME=/
OLDPWD=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/usr/local/bin
```

M44 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M44 non-shell current-directory execvp acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 83 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 4 path-dot 4 path-dotdot 0 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 83 write 1 write-bytes 83 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 83 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M45 Localbin Executable Dotdot Path Proof

M45 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'cd /usr/local/bin; ../bin/sbenv USER=operator ../bin/sbenv | ../bin/sbcat'`. It required no code changes after M44 because `execve` canonicalization already handles relative executable `..` segments before VFS stat/read.

Visible relative executable dotdot output:

```text
USER=operator
HOME=/
OLDPWD=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/usr/local/bin
```

M45 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M45 relative executable dotdot acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 83 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 4 path-dot 1 path-dotdot 3 path-fault 0 chdir 1 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 83 write 1 write-bytes 83 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 83 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 5 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE00 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M46 Absolute Localbin Executable Dotdot Path Proof

M46 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr/local/bin/../bin/sbenv USER=operator /usr/local/bin/../bin/sbenv | /usr/local/bin/../bin/sbcat'`. It required no code changes after M45 because absolute-path normalization already handles executable `..` segments before VFS stat/read.

Visible absolute executable dotdot output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M46 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M46 absolute executable dotdot acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 72 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 2 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 3 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE10 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M47 Absolute Localbin Mixed-Dot Path Proof

M47 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr/local/./bin/../bin/sbenv USER=operator /usr/local/./bin/../bin/sbenv|sbcat'`. It required no code changes after M46 because absolute-path normalization already handles mixed executable `.` and `..` segments before VFS stat/read.

The originally proposed all-absolute consumer variant was useful as a verifier stress trace but exceeded reliable QMP shell-line injection and arrived truncated in the guest as an unterminated quote. The accepted compact variant preserves the kernel proof target: BusyBox ash launches `sbenv` through a mixed absolute path, that `sbenv` performs a non-shell handoff to another mixed absolute `sbenv`, and the pipeline consumer resolves `sbcat` through the default PATH.

Visible output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M47 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M47 mixed-dot acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 3 path-dotdot 2 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M48 Absolute Localbin Repeated-Slash Path Proof

M48 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr//local/bin/sbenv USER=operator /usr/local//bin/sbenv|sbcat'`. It required no code changes after M47 because absolute-path normalization already collapses repeated slash segments before VFS stat/read.

Visible output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M48 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M48 repeated-slash acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M49 Absolute Localbin Root-Clamped Dotdot Path Proof

M49 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr/local/bin/../../local/bin/sbenv USER=operator /usr/local/bin/../../local/bin/sbenv|sbcat'`. It required no code changes after M48 because absolute-path normalization already handles bounded upward `..` traversal before VFS stat/read.

Visible output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M49 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M49 root-clamped dotdot acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 4 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M50 Absolute Over-Root Dotdot Clamp Proof

M50 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/../../usr/local/bin/sbenv USER=operator /../../../usr/local/bin/sbenv|sbcat'`. It required no code changes after M49 because absolute-path normalization already clamps over-root `..` traversal at `/` before VFS stat/read.

Visible output:

```text
USER=operator
HOME=/
PATH=/usr/local/bin:/bin:/usr/bin
PWD=/
```

M50 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M50 over-root dotdot acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 9 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 61 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 5 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 61 write 1 write-bytes 61 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 61 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 1 ioctl-tty 0 ioctl-enotty 1 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 3 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 3 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M51 Absolute Localbin Trailing-Slash Executable Denial Proof

M51 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbenv/ USER=operator /usr/local/bin/sbenv|sbcat'`. The first trace exposed a semantic gap: execve canonicalization stripped the terminal slash and launched `/usr/local/bin/sbenv` as a file. The fix preserves the original trailing-slash bit and denies slash-suffixed non-directory executable targets before the executable binary read.

Visible output:

```text
sh: /usr/local/bin/sbenv/: not found
```

M51 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest records kernel bytes 1,269,280, checksum `0xD4789513`, and SHA-256 `fa4be9a316b1888ac9f363ded5b788015470f1849e9255f1ce8d308f776ed628`.

M51 trailing-slash denial acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 65 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 37 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 1 path-trailing-denial 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 37 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 4 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M52 Absolute Localbin Trailing-Slash Directory/Open Proof

M52 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'ls /usr/local/bin/ | sbcat'`. It required no code changes after M51 because the M51 trailing-slash preservation is scoped to execve file targets; directory enumeration still normalizes and opens `/usr/local/bin/` as a directory.

Visible output:

```text
sbenv
sbcat
sbecho
```

M52 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors.

M52 trailing-slash directory acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 79 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 19 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 96 stat 7 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 2 read-bytes 19 write 1 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 2 writev-bytes 19 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 2 ioctl-tty 0 ioctl-enotty 2 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000019 prctl 3 prctl-set-name 2 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M53 Absolute Localbin Missing Executable Denial Proof

M53 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c '/usr/local/bin/sbmissing USER=operator /usr/local/bin/sbenv|sbcat'`. The first trace reached the right denial counters but returned `EINVAL`, making BusyBox ash print `Invalid argument`; the fix maps failed executable binary `stat` in the exec-read path to `ENOENT` while leaving invalid ELF and oversized-image behavior unchanged.

Visible output:

```text
sh: /usr/local/bin/sbmissing: not found
```

M53 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest records kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M53 missing executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 65 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 40 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 40 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 2 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 2
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M54 PATH Localbin Missing Executable Denial Proof

M54 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbmissing USER=operator sbenv|sbcat'`. It required no code changes after M53 because BusyBox ash already reports PATH-search misses as `not found`; this proof records the bounded `/usr/local/bin`, `/bin`, and `/usr/bin` alias-denial shape for an unqualified missing command while the valid `sbcat` pipeline consumer still execs normally.

Visible output:

```text
sh: sbmissing: not found
```

M54 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M54 PATH missing executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 70 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 5 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 25 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 3 stat-denial 6 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 3 writev-bytes 25 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 1 execveat 0 execve-denial 0 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 12 vfs-localbin-alias 3 vfs-localbin-open 0 vfs-localbin-read 1 vfs-localbin-denial 4
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M55 Non-Shell Execvp Missing Executable Denial Proof

M55 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator sbmissing|sbcat'`. It required no code changes after M54 because the M53 `ENOENT` mapping also covers the third-party `execvp` handoff path. The proof records that a non-shell `sbenv` process can try to replace itself with a missing PATH command, report the real `No such file or directory` error, and leave the valid `sbcat` pipeline consumer and all process/pipe/PML4 cleanup invariants intact.

Visible output:

```text
sbenv: execvp sbmissing: No such file or directory
```

M55 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M55 non-shell `execvp` missing executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 73 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 51 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 51 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 3 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 6 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 2
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M56 Non-Shell Execvp Trailing-Slash Executable Denial Proof

M56 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/sbenv/|sbcat'`. It required no code changes after M55 because the existing trailing-slash executable denial path already applies when a third-party process performs its own `execvp` handoff. The proof records that the non-shell caller sees `ENOTDIR` as `Not a directory` and that the slash-suffixed executable target is not normalized into a file launch.

Visible output:

```text
sbenv: execvp /usr/local/bin/sbenv/: Not a directory
```

M56 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M56 non-shell `execvp` trailing-slash executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 53 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 1 path-trailing-denial 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 53 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 7 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M57 Non-Shell Execvp Directory-Target Denial Proof

M57 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/|sbcat'`. It required no code changes after M56. The proof records that a third-party `sbenv` process attempting to `execvp` the bounded localbin directory target is denied visibly as `Invalid argument` while the valid `sbenv` producer wrapper and `sbcat` pipeline consumer still run and clean up.

Visible output:

```text
sbenv: execvp /usr/local/bin/: Invalid argument
```

M57 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M57 non-shell `execvp` directory-target acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 48 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 1 path-trailing-denial 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 48 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M58 Non-Shell Execvp Bare-Directory Executable Denial Proof

M58 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin|sbcat'`. It required no code changes after M57. The proof records that a third-party `sbenv` process attempting to `execvp` the bounded localbin directory target without a terminal slash is denied visibly as `Invalid argument` while the valid `sbenv` producer wrapper and `sbcat` pipeline consumer still run and clean up.

Visible output:

```text
sbenv: execvp /usr/local/bin: Invalid argument
```

M58 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M58 non-shell `execvp` bare-directory executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 47 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 47 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M59 Non-Shell Execvp Dot-Directory Executable Denial Proof

M59 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/.|sbcat'`. It required no code changes after M58. The proof records that a third-party `sbenv` process attempting to `execvp` the bounded localbin dot-directory target is denied visibly as `Invalid argument` while the valid `sbenv` producer wrapper and `sbcat` pipeline consumer still run and clean up.

Visible output:

```text
sbenv: execvp /usr/local/bin/.: Invalid argument
```

M59 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M59 non-shell `execvp` dot-directory executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 49 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 2 path-dotdot 0 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 49 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M60 Non-Shell Execvp Parent-Directory Executable Denial Proof

M60 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/..|sbcat'`. It required no code changes after M59. The proof records that a third-party `sbenv` process attempting to `execvp` the bounded localbin parent-directory target is denied visibly as `Invalid argument` while the valid `sbenv` producer wrapper and `sbcat` pipeline consumer still run and clean up.

Visible output:

```text
sbenv: execvp /usr/local/bin/..: Invalid argument
```

M60 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M60 non-shell `execvp` parent-directory executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 50 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 1 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 50 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M61 Non-Shell Execvp Parent-Rebased Directory Executable Denial Proof

M61 is accepted on the UEFI Product path with `linux /APPS/BUSYBOX sh -c 'sbenv USER=operator /usr/local/bin/../bin|sbcat'`. It required no code changes after M60. The proof records that a third-party `sbenv` process attempting to `execvp` the bounded localbin parent-rebased directory target is denied visibly as `Invalid argument` while the valid `sbenv` producer wrapper and `sbcat` pipeline consumer still run and clean up.

Visible output:

```text
sbenv: execvp /usr/local/bin/../bin: Invalid argument
```

M61 final reserves remain UEFI 827,872 bytes and BIOS 101 sectors. The UEFI manifest remains kernel bytes 1,269,280, checksum `0xD00B97A2`, and SHA-256 `2908fc9957dfb3270e618f2e8db747f589815b882506ff69cc75a4a534ba12de`.

M61 non-shell `execvp` parent-rebased directory executable acceptance telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 envc 4 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000224F000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 1 kernel-cr3-entry 1 syscall-root-repair 0 syscall-root-reload 71 syscall-root-denial 0 fs-save 3 fs-restore 4 fs-set 7 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 3 task 0 started 1 console-bytes 54 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 4 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 0 getcwd-bytes 0 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 1 path-dotdot 1 path-trailing 0 path-trailing-denial 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 1 read-bytes 0 write 0 write-bytes 0 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe2 0 pipe2-denials 0 pipe2-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 pipe-provider-denials 0 fd-fork-pipe-copy 3 fd-fork-pipe-denial 0 fd-fork-pipe-last-fd 3 readv 0 readv-bytes 0 writev 5 writev-bytes 54 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 execve 2 execveat 0 execve-denial 1 execve-fault 0 execve-last-error 0 execve-last-binary-bytes 36968 execve-last-closed-fds 0 execve-last-fd-live-before 3 execve-last-fd-live-after 3 execve-last-vma-before 11 execve-last-vma-released 11 execve-last-vma-after 7 execve-last-argc 1 execve-last-envc 4 execve-last-transfer-ready 1 execve-last-transfer-rip 0x000000005200117F execve-last-transfer-rsp 0x000000004420FE30 fork 2 fork-success 2 fork-enosys 0 fork-denial 0 fork-child-slot 2 fork-child-root-distinct 1 fork-last-rip 0x000000005200EF74 wait4 2 wait4-reap 2 wait4-last-exit-code 0 child-root-cleanup 2 pml4-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 36968 vfs-bin-alias 0 vfs-bin-open 0 vfs-bin-read 0 vfs-bin-denial 0 vfs-localbin-alias 6 vfs-localbin-open 0 vfs-localbin-read 2 vfs-localbin-denial 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## M67 Bounded File-Backed Mmap Proof

M67 is accepted on the UEFI Product path with `linux /APPS/MMAPSMOKE`. `MMAPSMOKE` is a local static musl ET_EXEC proof binary linked at `0x52000000`, staged as `/APPS/MMAPSMOKE`, and verified with SHA-256 `595D27327DCC41A511F4170AD60909C8BBC62070AEEC581A365C27191C0EF3B2`.

The proof opens `/nvme/apps/data/file.txt`, reads it once through the Linux VFS, then maps the same fd with `mmap(..., PROT_READ, MAP_PRIVATE, fd, 0)`. The kernel eagerly copies the bounded file content into a private anonymous VMA and restores the requested protection. This proves the first regular-file mmap path needed by small static utilities without claiming full lazy file-backed VM.

M67 acceptance telemetry excerpt:

```text
drs-realbin path /APPS/MMAPSMOKE provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 5 stack 16 envc 4 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 low-pdpt-present 0 syscall-entry-high 1 idt-high 1 descriptor-high 1 kernel-entry-high-ready 1 syscall-root-repair 0 fs-restore 1 fs-set 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 scheduler-denial 0 console-bytes 109 exit 0 cleanup 1 mmap 1 mmap-bytes 4096 mmap-denial 0 mmap-file 1 mmap-file-bytes 27 mmap-file-denial 0 mmap-last-error 0 mmap-last-flags 0x0000000000000002 mmap-last-length 0x0000000000001000 read 1 read-bytes 27 write 25 write-bytes 109 pml4-pool-used-final 0 root-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-bytes 27 page-faults 0
```

M67 non-claims: `MAP_SHARED`, writeback, mappings larger than the then-current `LINUX_ABI64_MMAP_FILE_COPY_BYTES` 4096-byte cap, page-fault-driven file population, dynamic linking, and BIOS file-backed mmap remain unavailable. The accepted implementation is a UEFI-only, fixed-buffer, no-dynamic-allocation private copy path.

## M68 Bounded Multi-Page File-Backed Mmap Proof

M68 is accepted on the UEFI Product path with `linux /APPS/MMAP2`. The trace before the fix proved the M67 one-page cap by denying an 8192-byte mapping with `mmap-last-error 22`. The accepted M68 change raises the UEFI-only fixed file-mmap copy cap to 65536 bytes while preserving the same eager private-copy implementation.

Staged artifacts:

- `/APPS/MMAP2`, static musl ET_EXEC at `0x52000000`, SHA-256 `5AB9EF11E16B268B119BACF6EBA2F5544931FF6BF9F9E1B1DE68743C7173E4D1`
- `/APPS/MMAPDATA`, deterministic 8192-byte payload, SHA-256 `1F5A16C4456F34C5459E4E66D8650D4DBB6B298810D0C21CEF6571DB21D69C81`

The proof maps `/nvme/apps/mmapdata` twice: first as an 8192-byte `PROT_READ|PROT_EXEC` private mapping from offset 0, then as a 4096-byte `PROT_READ` private mapping from offset 4096. The smoke program validates every byte against the deterministic pattern before printing its done marker.

M68 acceptance telemetry excerpt:

```text
drs-realbin path /APPS/MMAP2 provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 5 stack 16 envc 4 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 low-pdpt-present 0 syscall-entry-high 1 idt-high 1 descriptor-high 1 kernel-entry-high-ready 1 syscall-root-repair 0 fs-restore 1 fs-set 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 scheduler-denial 0 console-bytes 73 exit 0 cleanup 1 mmap 2 mmap-bytes 12288 mmap-denial 0 mmap-file 2 mmap-file-bytes 12288 mmap-file-denial 0 mmap-last-error 0 mmap-last-flags 0x0000000000000002 mmap-last-length 0x0000000000001000 write 4 write-bytes 73 pml4-pool-used-final 0 root-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-bytes 8192 page-faults 0
```

M68 non-claims: `MAP_SHARED`, writeback, mappings larger than `LINUX_ABI64_MMAP_FILE_COPY_BYTES` 65536 bytes, page-fault-driven file population, dynamic linking, and BIOS file-backed mmap remain unavailable. M68 is still an eager private-copy bridge, not a general VM object cache.

## M69 Windowed File-Backed Mmap Proof

M69 is accepted on the UEFI Product path with `linux /APPS/MMAPWIN`. The trace before the fix proved the M68 file-size coupling: mapping 4096 bytes from offset 65536 inside a 131072-byte file failed with `mmap-last-error 22` and `vfs-nvme-reads 0` because the old helper tried to read the entire file into the 65536-byte mmap scratch buffer before copying the requested window.

Staged artifacts:

- `/APPS/MMAPWIN`, static musl ET_EXEC at `0x52000000`, SHA-256 `8DBBA2981B7A9084C80556EB66193C4C35D59C4211F7F16588E4558A5884C28E`
- `/APPS/BIGDATA`, deterministic 131072-byte payload, SHA-256 `37EF3D17D641BFF7C3F5F8A525E6B5EC7C04290E5A850B760F13F8AC6826C503`

The accepted implementation adds a UEFI-only MMIO FAT range reader, a Linux VFS range wrapper, and mmap population through the fixed 4096-byte `LINUX_ABI64_MMAP_FILE_WINDOW_BYTES` scratch page. The mapping cap remains `LINUX_ABI64_MMAP_FILE_COPY_BYTES` 65536 bytes, but file size is no longer coupled to the scratch buffer size.

M69 acceptance telemetry excerpt:

```text
drs-realbin path /APPS/MMAPWIN provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 5 stack 16 envc 4 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 low-pdpt-present 0 syscall-entry-high 1 idt-high 1 descriptor-high 1 kernel-entry-high-ready 1 syscall-root-repair 0 fs-restore 1 fs-set 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 scheduler-denial 0 console-bytes 54 exit 0 cleanup 1 mmap 1 mmap-bytes 4096 mmap-denial 0 mmap-file 1 mmap-file-bytes 4096 mmap-file-denial 0 mmap-last-error 0 mmap-last-flags 0x0000000000000002 mmap-last-length 0x0000000000001000 write 3 write-bytes 54 pml4-pool-used-final 0 root-pool-used-final 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 4096 page-faults 0
```

M69 also reran the M68 two-map proof after the windowed-reader change. `linux /APPS/MMAP2` still passes with `mmap 2 mmap-bytes 12288 mmap-file 2 mmap-file-bytes 12288 mmap-file-denial 0 vfs-nvme-reads 3 page-faults 0 exit 0`.

M69 non-claims: population is still eager at mmap time, not page-fault-driven; mappings larger than 65536 bytes still fail; `MAP_SHARED`, writeback, a file-backed VMA object cache, dynamic linking, and BIOS file-backed mmap remain unavailable.

## M70 Real Dynamic ELF Boundary Telemetry

M70 is accepted on the UEFI Product path with `linux /APPS/DYNSMOKE`. The launcher still denies dynamic ELF before process allocation, but the denial now records parsed ELF type, `PT_LOAD`, `PT_INTERP`, `PT_DYNAMIC`, and bounded `DT_NEEDED` analysis instead of reporting only `stage static code 8`.

Staged artifact:

- `/APPS/DYNSMOKE`, external musl-cross ET_EXEC linked at `0x52000000`, SHA-256 `1D9949F783D76F5CA6755D59B04C1E289C90ABF47DE55F5EA3E5E1883EC7A3E3`

`readelf -h -l -d` reports `Type: EXEC`, entry `0x520010a1`, four `PT_LOAD` segments, one `PT_DYNAMIC`, no `PT_INTERP` from this musl-cross toolchain, and one `DT_NEEDED` entry for `libc-x64.so`. This milestone is telemetry only: no dynamic linker, relocation processing, interpreter handoff, process/PML4 allocation, or execution is added for denied dynamic inputs.

M70 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNSMOKE stage static code 8 pid 4294967295 elf-type 2 elf-load 4 elf-interp 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 19600 nvme-read-capacity 4194304 nvme-read-size 19600 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 819,680 bytes and BIOS 101 sectors.

M70 non-claims: dynamic execution, `PT_INTERP` handoff, relocation processing, dynamic symbol resolution, shared-library loading, and glibc compatibility remain unavailable.

## M71 PT_INTERP Dynamic ELF Boundary Trace

M71 is accepted on the UEFI Product path with `linux /APPS/DYNINTERP`. The artifact is built by the local musl cross toolchain with `-fsys-dyn-linker`, producing a real `PT_INTERP` program header instead of the M70 toolchain-default `-no-dynamic-linker` output.

Staged artifact:

- `/APPS/DYNINTERP`, external musl-cross ET_EXEC linked at `0x52000000`, SHA-256 `50B977B354FE0E7F776D4A4D3FA475246176D5B6077B34216C39B1C4D3012787`

`readelf -h -l -d` reports `Type: EXEC`, entry `0x52001071`, four `PT_LOAD` segments, one `PT_INTERP` requesting `/lib/ld-musl-x86_64.so.1`, one `PT_DYNAMIC`, and one `DT_NEEDED` entry for `libc-x64.so`.

M71 adds denial-path telemetry for the requested interpreter path: bounded byte count, FNV-1a checksum, and whether the path matches the currently supported Limitless interpreter names. The accepted artifact reports `interp-supported 0`, which is correct because `/lib/ld-musl-x86_64.so.1` is not yet staged or loadable by the Product dynamic path.

M71 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNINTERP stage static code 8 pid 4294967295 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 24 interp-checksum 0x7E2FBF7B interp-supported 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves remain UEFI 819,680 bytes and BIOS 101 sectors.

M71 non-claims: interpreter file resolution, interpreter ELF loading, relocation processing, dynamic symbol resolution, shared-library loading, glibc compatibility, and dynamic execution remain unavailable.

## M72 Bounded Interpreter File Staging

M72 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. The dynamic app requests the Linux-visible interpreter path `/nvme/apps/ldlimit`, which the denial path maps to the staged NVMe FAT backend `/APPS/LDLIMIT`. The interpreter candidate is read through the same broker-scoped NVMe authority used by the real-binary launcher and parsed for ELF metadata, but it is not mapped, relocated, or executed.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, external musl-cross ET_EXEC linked at `0x52000000`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`
- `/APPS/LDLIMIT`, static musl ET_EXEC linked at `0x47800000`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`

`readelf -h -l -d` for `DYNLDLIMIT` reports `Type: EXEC`, entry `0x52001071`, four `PT_LOAD` segments, one `PT_INTERP` requesting `/nvme/apps/ldlimit`, one `PT_DYNAMIC`, and one `DT_NEEDED` entry for `libc-x64.so`. `readelf -h -l` for `LDLIMIT` reports `Type: EXEC`, entry `0x4780105f`, four `PT_LOAD` segments, no `PT_INTERP`, and no `PT_DYNAMIC`.

M72 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 4294967295 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves remain UEFI 819,680 bytes and BIOS 101 sectors.

M72 non-claims: interpreter `PT_LOAD` mapping, dynamic app mapping, relocation processing, dynamic symbol resolution, `DT_NEEDED` library loading, shared-library search paths, glibc compatibility, and dynamic execution remain unavailable.

## M73 Bounded Dynamic App And Interpreter Mapping

M73 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. The same M72 app/interpreter artifacts are used, but the supported dynamic input now allocates a bounded Linux persona, creates a private process root, maps the app `PT_LOAD` segments, reads the staged interpreter candidate, maps the interpreter `PT_LOAD` segments, and then denies before relocation processing, symbol binding, stack/auxv setup, task registration, or transfer of control.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, external musl-cross ET_EXEC linked at `0x52000000`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`
- `/APPS/LDLIMIT`, static musl ET_EXEC linked at `0x47800000`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`

M73 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 815,584 bytes and BIOS 101 sectors.

M73 non-claims: relocation enumeration/application, GOT/PLT binding, `DT_NEEDED` library loading, dynamic-linker ABI handoff, dynamic stack/auxv construction, task registration, and dynamic execution remain unavailable.

## M74 Dynamic Relocation Table Trace

M74 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps the M73 denial-path process/root allocation and app/interpreter `PT_LOAD` mapping proof, then inspects the dynamic app relocation metadata before the interpreter probe overwrites the staging buffer. The launcher translates bounded dynamic-table virtual addresses back into file offsets and reports relocation table shape without applying relocations, writing GOT/PLT entries, binding symbols, or transferring control.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

`readelf -d -r` for `DYNLDLIMIT` reports `DT_RELA` with four entries, `DT_JMPREL` with two PLT entries, `DT_RELAENT` 24, `DT_PLTREL` RELA, first `.rela.dyn` target `0x52003fc8` type `R_X86_64_GLOB_DAT` (6), and first `.rela.plt` target `0x52004000` type `R_X86_64_JUMP_SLOT` (7).

M74 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves remain UEFI 815,584 bytes and BIOS 101 sectors.

M74 non-claims: relocation application, GOT/PLT writes, dynamic symbol lookup, `DT_NEEDED` library loading, dynamic-linker ABI handoff, dynamic stack/auxv construction, task registration, and dynamic execution remain unavailable.

## M75 Dynamic Relocation Symbol Trace

M75 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps the M74 relocation-table census and adds a bounded dynamic-symbol pass over `DT_SYMTAB`, `DT_STRTAB`, `DT_STRSZ`, and `DT_SYMENT`. The denial path now reports the first `DT_NEEDED` dependency name and the symbol names referenced by the first RELA and PLT relocation entries, while still denying before any binding or relocation write.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

`readelf -d -r --dyn-syms` for `DYNLDLIMIT` reports `DT_SYMTAB` at `0x52000388`, `DT_STRTAB` at `0x52000460`, `DT_STRSZ` 149, `DT_SYMENT` 24, first needed library `libc-x64.so`, first `.rela.dyn` symbol index 2 name `__deregister_frame_info`, and first `.rela.plt` symbol index 1 name `write`.

M75 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 811,488 bytes and BIOS 101 sectors.

M75 non-claims: walking every relocation entry, symbol-provider classification, relocation application, GOT/PLT writes, dynamic symbol binding, `DT_NEEDED` library loading, dynamic-linker ABI handoff, dynamic stack/auxv construction, task registration, and dynamic execution remain unavailable.

## M76 Dynamic Relocation Support Matrix

M76 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps the M75 first-symbol trace and adds a bounded walk over every RELA and PLT relocation entry. For each referenced symbol, the denial path resolves the `.dynsym` entry and `.dynstr` name, checks the fixed Limitless libc and interpreter export registries, counts supported versus missing bindings, and still denies before applying relocations or writing GOT/PLT state.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The accepted support matrix matches `readelf -r --dyn-syms`: four `R_X86_64_GLOB_DAT` entries and two `R_X86_64_JUMP_SLOT` entries are walked. The current Limitless libc registry can provide `write`; the interpreter registry provides none of this app's referenced symbols; the five missing entries are the frame/ITM startup hooks plus `__libc_start_main`.

M76 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 1 dynamic-binding-missing 5 dynamic-binding-libc 1 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 807,392 bytes and BIOS 101 sectors.

M76 non-claims: weak-symbol admission, `__libc_start_main`, relocation application, GOT/PLT writes, dynamic symbol binding, `DT_NEEDED` library loading, dynamic-linker ABI handoff, dynamic stack/auxv construction, task registration, and dynamic execution remain unavailable.

## M77 Weak Relocation Admission

M77 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps the M76 bounded support-matrix walk and adds the first ABI-aware admission rule for dynamic relocation classification: undefined weak `R_X86_64_GLOB_DAT` symbols are counted as nullable bindings instead of hard missing providers. Strong unresolved bindings remain denied before relocation application, GOT/PLT writes, or control transfer.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The accepted support matrix still walks six relocations: four `R_X86_64_GLOB_DAT` entries and two `R_X86_64_JUMP_SLOT` entries. One `JUMP_SLOT` binding is supported by the Limitless libc registry (`write`), four undefined weak `GLOB_DAT` bindings are now classified as nullable, and one strong startup binding remains hard-missing. The dynamic dependency itself is still unsupported as `libc-x64.so`, so the launcher still returns the same dynamic-input denial after cleanup.

M77 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 1 dynamic-binding-missing 1 dynamic-binding-weak-null 4 dynamic-binding-libc 1 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 0 dynamic-missing 1 dynamic-libc 0 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 807,392 bytes and BIOS 101 sectors.

M77 non-claims: `__libc_start_main`, dependency aliasing for `libc-x64.so`, relocation application, GOT/PLT writes, dynamic symbol binding, `DT_NEEDED` library loading, dynamic-linker ABI handoff, dynamic stack/auxv construction, task registration, and dynamic execution remain unavailable.

## M78 Libc Startup Provider Trace

M78 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps dynamic execution denied, but resolves the two remaining classification gaps from M77: `libc-x64.so` is admitted as a local libc dependency alias, and `__libc_start_main` is represented in the fixed libc export registry as an unavailable startup provider. This proves the denial path can account for every relocation symbol without applying relocations or jumping into a dynamic program.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The accepted support matrix still walks six relocations. Four undefined weak `GLOB_DAT` bindings remain nullable, `write` is provided by the libc registry, and `__libc_start_main` is now counted as a libc-backed unavailable provider rather than an unknown hard miss. The `dynamic-binding-unavailable 1` field is intentional: it prevents this milestone from being mistaken for runnable dynamic startup.

M78 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 2 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-unavailable 1 dynamic-binding-libc 2 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 1 dynamic-missing 0 dynamic-libc 1 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 807,392 bytes and BIOS 101 sectors.

M78 non-claims: real `__libc_start_main` behavior, relocation application, GOT/PLT writes, dynamic stack/auxv construction, dynamic-linker ABI handoff, task registration, control transfer, and dynamic execution remain unavailable.

## M79 Dynamic Relocation Dry-Run

M79 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps dynamic execution denied, but computes the relocation target/value pairs that would be written for the bounded `R_X86_64_GLOB_DAT` and `R_X86_64_JUMP_SLOT` entries. Targets are validated against writable app `PT_LOAD` ranges; values come from nullable weak symbols, fixed libc provider addresses, or fixed interpreter provider addresses. No GOT/PLT bytes are written in M79.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The accepted dry-run validates all six relocation targets and computes all six values. Five entries are apply-ready: four undefined weak `GLOB_DAT` entries with value `0`, plus the `write` PLT relocation to fixed libc address `0x47811020`. One entry remains blocked: `__libc_start_main` resolves to the unavailable startup provider introduced in M78, so the launcher still denies before relocation writes or dynamic transfer.

M79 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 2 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-unavailable 1 dynamic-binding-libc 2 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 dynamic-reloc-dry-run 1 dynamic-reloc-dry-total 6 dynamic-reloc-dry-target-valid 6 dynamic-reloc-dry-value 6 dynamic-reloc-dry-provider 2 dynamic-reloc-dry-weak-null 4 dynamic-reloc-dry-unavailable 1 dynamic-reloc-dry-apply-ready 5 dynamic-reloc-dry-blocked 1 dynamic-reloc-dry-error 0 dynamic-reloc-dry-first-target 0x0000000052003FC8 dynamic-reloc-dry-first-value 0x0000000000000000 dynamic-reloc-dry-jmprel-target 0x0000000052004000 dynamic-reloc-dry-jmprel-value 0x0000000047811020 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 1 dynamic-missing 0 dynamic-libc 1 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 807,392 bytes and BIOS 101 sectors.

M79 non-claims: relocation writes, GOT/PLT mutation, real `__libc_start_main` behavior, dynamic stack/auxv construction, dynamic-linker ABI handoff, task registration, control transfer, and dynamic execution remain unavailable.

## M80 Safe Relocation Write/Readback

M80 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps dynamic execution denied, but applies and reads back only the five M79 apply-ready relocations while the target Linux process CR3 is active. The relocation walker still parses dynamic metadata on the kernel root so low-address kernel staging buffers are not exposed to process roots after M62; it switches to the process root only for each validated user GOT/PLT store/load pair, then returns to the kernel root before continuing.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The accepted run inspects six relocations, validates all six targets, computes six values, writes and reads back five safe entries, and blocks the one unavailable `__libc_start_main` startup relocation. The first nullable weak relocation reads back `0`; the first PLT relocation for `write` reads back the fixed provider value `0x47811020`. Cleanup returns the temporary process root to the static pool.

M80 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 2 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-unavailable 1 dynamic-binding-libc 2 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 dynamic-reloc-dry-run 1 dynamic-reloc-dry-total 6 dynamic-reloc-dry-target-valid 6 dynamic-reloc-dry-value 6 dynamic-reloc-dry-provider 2 dynamic-reloc-dry-weak-null 4 dynamic-reloc-dry-unavailable 1 dynamic-reloc-dry-apply-ready 5 dynamic-reloc-dry-blocked 1 dynamic-reloc-dry-error 0 dynamic-reloc-dry-first-target 0x0000000052003FC8 dynamic-reloc-dry-first-value 0x0000000000000000 dynamic-reloc-dry-jmprel-target 0x0000000052004000 dynamic-reloc-dry-jmprel-value 0x0000000047811020 dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 5 dynamic-reloc-apply-readback 5 dynamic-reloc-apply-blocked 1 dynamic-reloc-apply-unavailable 1 dynamic-reloc-apply-error 0 dynamic-reloc-apply-first-readback 0x0000000000000000 dynamic-reloc-apply-jmprel-readback 0x0000000047811020 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 1 dynamic-missing 0 dynamic-libc 1 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 807,392 bytes and BIOS 101 sectors. The M80 UEFI manifest reports kernel bytes 1,289,760, checksum `0x7236CD86`, and SHA-256 `85a85a062d5943058faa4bf24756f96972a0de204f24ee5274f90eba5faf1b74`.

M80 non-claims: real `__libc_start_main` behavior, dynamic stack/auxv construction, dynamic-linker ABI handoff, task registration, control transfer, and dynamic execution remain unavailable.

## M81 Bounded Libc Startup Shim

M81 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps dynamic execution denied, but replaces the unavailable `__libc_start_main` provider with a bounded in-tree Limitless libc startup shim. The generated shim routine calls `main(argc, argv, envp)` and exits through `exit_group`; M81 proves the dynamic relocation path can bind and write that provider address, not that the dynamic task is runnable yet.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The accepted run inspects six relocations, validates all six targets, computes all six values, writes and reads back all six entries, and records no unavailable startup binding. `__libc_start_main` resolves to `0x47811BF0`, is applied, and reads back exactly. Cleanup returns the temporary process root to the static pool.

M81 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 2 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-unavailable 0 dynamic-binding-libc 2 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 dynamic-reloc-dry-run 1 dynamic-reloc-dry-total 6 dynamic-reloc-dry-target-valid 6 dynamic-reloc-dry-value 6 dynamic-reloc-dry-provider 2 dynamic-reloc-dry-weak-null 4 dynamic-reloc-dry-unavailable 0 dynamic-reloc-dry-apply-ready 6 dynamic-reloc-dry-blocked 0 dynamic-reloc-dry-error 0 dynamic-reloc-dry-first-target 0x0000000052003FC8 dynamic-reloc-dry-first-value 0x0000000000000000 dynamic-reloc-dry-jmprel-target 0x0000000052004000 dynamic-reloc-dry-jmprel-value 0x0000000047811020 dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 6 dynamic-reloc-apply-readback 6 dynamic-reloc-apply-blocked 0 dynamic-reloc-apply-unavailable 0 dynamic-reloc-apply-error 0 dynamic-reloc-apply-first-readback 0x0000000000000000 dynamic-reloc-apply-jmprel-readback 0x0000000047811020 dynamic-libc-start-main 1 dynamic-libc-start-main-apply 1 dynamic-libc-start-main-value 0x0000000047811BF0 dynamic-libc-start-main-readback 0x0000000047811BF0 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 1 dynamic-missing 0 dynamic-libc 1 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 803,296 bytes and BIOS 101 sectors. The M81 UEFI manifest reports kernel bytes 1,293,856, checksum `0xF3505722`, and SHA-256 `791a0e8c5ad45ddaac3096d7edb6bd2699f1985ce83a451fdd2e8754cb818e84`.

M81 non-claims: dynamic stack/auxv construction, dynamic-linker ABI handoff, task registration, control transfer, and dynamic execution remain unavailable.

## M82 Dynamic Stack And Auxv Frame Proof

M82 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It keeps dynamic execution denied, but the supported `PT_INTERP` denial path now constructs the initial dynamic launch stack for the already mapped app/interpreter pair and proves the interpreter transfer frame is ready. The stack is built with the shared ELF auxv and initial-stack helpers, carries `argc`, `argv`, the four-entry default environment, base SysV auxv entries, `AT_RANDOM`, `AT_PLATFORM`, and validated null terminators, and is then released with the temporary process root before the same dynamic-input denial is returned.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

M82 acceptance telemetry:

```text
drs-realbin-fail path /APPS/DYNLDLIMIT stage static code 8 pid 8241 elf-type 2 elf-load 4 elf-interp 1 interp-bytes 18 interp-checksum 0x8F7B800D interp-supported 1 interp-file-attempt 1 interp-file-read 1 interp-file-bytes 16704 interp-file-elf 1 interp-file-type 2 interp-file-load 4 interp-file-interp 0 interp-file-dynamic 0 interp-file-error 0 interp-file-nvme-error 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-relaent 24 dynamic-pltrel 7 dynamic-reloc-first 0x0000000052003FC8 dynamic-reloc-type 6 dynamic-jmprel-first 0x0000000052004000 dynamic-jmprel-type 7 dynamic-reloc-error 0 dynamic-symbol-trace 1 dynamic-symtab 1 dynamic-strtab 149 dynamic-syment 24 dynamic-needed-name libc-x64.so dynamic-needed-name-bytes 11 dynamic-needed-name-checksum 0xC92FC296 dynamic-reloc-symbol-index 2 dynamic-reloc-symbol __deregister_frame_info dynamic-reloc-symbol-bytes 23 dynamic-reloc-symbol-checksum 0x58F2B978 dynamic-jmprel-symbol-index 1 dynamic-jmprel-symbol write dynamic-jmprel-symbol-bytes 5 dynamic-jmprel-symbol-checksum 0xBE269F5C dynamic-symbol-error 0 dynamic-binding-walk 1 dynamic-binding-total 6 dynamic-binding-supported 2 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-unavailable 0 dynamic-binding-libc 2 dynamic-binding-interp 0 dynamic-binding-glob-dat 4 dynamic-binding-jump-slot 2 dynamic-binding-other 0 dynamic-binding-error 0 dynamic-reloc-dry-run 1 dynamic-reloc-dry-total 6 dynamic-reloc-dry-target-valid 6 dynamic-reloc-dry-value 6 dynamic-reloc-dry-provider 2 dynamic-reloc-dry-weak-null 4 dynamic-reloc-dry-unavailable 0 dynamic-reloc-dry-apply-ready 6 dynamic-reloc-dry-blocked 0 dynamic-reloc-dry-error 0 dynamic-reloc-dry-first-target 0x0000000052003FC8 dynamic-reloc-dry-first-value 0x0000000000000000 dynamic-reloc-dry-jmprel-target 0x0000000052004000 dynamic-reloc-dry-jmprel-value 0x0000000047811020 dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 6 dynamic-reloc-apply-readback 6 dynamic-reloc-apply-blocked 0 dynamic-reloc-apply-unavailable 0 dynamic-reloc-apply-error 0 dynamic-reloc-apply-first-readback 0x0000000000000000 dynamic-reloc-apply-jmprel-readback 0x0000000047811020 dynamic-libc-start-main 1 dynamic-libc-start-main-apply 1 dynamic-libc-start-main-value 0x0000000047811BF0 dynamic-libc-start-main-readback 0x0000000047811BF0 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-error 0 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 18 dynamic-stack-align 1 dynamic-stack-argv-null 1 dynamic-stack-envp-null 1 dynamic-stack-auxv-null 1 dynamic-stack-random 0x74A83CFF dynamic-stack-platform 0x66090E2B dynamic-stack-rsp 0x000000005300FE30 dynamic-stack-auxv-address 0x000000005300FE70 dynamic-auxv-phdr 0x0000000052000040 dynamic-auxv-base 0x0000000047800000 dynamic-auxv-entry 0x0000000052001071 dynamic-transfer-ready 1 dynamic-transfer-rip 0x000000004780105F dynamic-transfer-rsp 0x000000005300FE30 root-cleanup 1 pml4-pool-used-final 0 elf-dynamic 1 dynamic-needed 1 dynamic-supported 1 dynamic-missing 0 dynamic-libc 1 dynamic-pthread 0 dynamic-first 0xC92FC296 dynamic-last 0xC92FC296 elf-error 0 load-error 0 stack-error 0 load-first 0x0000000000000000 load-end 0x0000000000000000 low-kernel-limit 0x0000000001000000 nvme-read-error 0 nvme-read-bytes 15680 nvme-read-capacity 4194304 nvme-read-size 15680 nvme-read-attr 0x0000000000000020
```

Final reserves are UEFI 803,296 bytes and BIOS 101 sectors. The M82 UEFI manifest reports kernel bytes 1,293,856, checksum `0x76B5916E`, and SHA-256 `7199b36fb73a8ea711bcce6d5a4c63363df4c39c08691e5afb87eeda2516efbd`.

M82 non-claims: dynamic-linker ABI handoff, task registration, control transfer into the interpreter, and dynamic execution remain unavailable.

## M83 Dynamic Task Registration And First Execution

M83 is accepted on the UEFI Product path with `linux /APPS/DYNLDLIMIT`. It removes the final dynamic-execution denial for the exact supported M80-M82 path, keeps the dynamic app/interpreter mappings and 64 KiB initial stack live in the process root, registers the process with the normal scheduler at interpreter RIP `0x000000004780105F` and RSP `0x000000005300FE30`, transfers control through the standard CR3/syscall boundary, prints visible brokered console output, exits through `exit_group(231)`, and returns the process root to the pool.

Staged artifacts:

- `/APPS/DYNLDLIMIT`, SHA-256 `9F6EB9C05B3065D39BC59D24DEFE9361267B34CEFD4DE78F568DDB00497238FA`, external musl-cross ET_EXEC linked at `0x52000000` with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

M83 also fixes the UEFI boot-media staging alias used on real hardware: low pages containing staged app/interpreter payloads are now identity-mapped before the linked-kernel fallback alias is applied. This resolves the hardware path where boot-media read reported the correct byte count but ELF parsing saw the wrong magic at `stage elf code 3`.

M83 acceptance telemetry:

```text
drs-realbin path /APPS/DYNLDLIMIT provenance 1 source 2 nvme-read 0 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-map-cleanup 1 dynamic-map-error 0 dynamic-reloc 1 dynamic-rela 4 dynamic-jmprel 2 dynamic-reloc-apply 1 dynamic-reloc-apply-total 6 dynamic-reloc-apply-write 6 dynamic-reloc-apply-readback 6 dynamic-reloc-apply-blocked 0 dynamic-reloc-apply-unavailable 0 dynamic-reloc-apply-error 0 dynamic-libc-start-main 1 dynamic-libc-start-main-apply 1 dynamic-libc-start-main-value 0x0000000047811BF0 dynamic-libc-start-main-readback 0x0000000047811BF0 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-error 0 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 18 dynamic-stack-align 1 dynamic-transfer-ready 1 dynamic-transfer-rip 0x000000004780105F dynamic-transfer-rsp 0x000000005300FE30 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 15 dynamic-exit-code 0x00000000 mapped 8 pages 10 stack 16 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 low-pdpt-present 0 syscall-entry-high 1 idt-high 1 kernel-entry-high-ready 1 syscall-root-repair 0 cr3-start 1 cr3-exit 1 task 0 started 1 console-bytes 15 exit 0 cleanup 1 write 1 write-bytes 15 page-faults 0 pml4-pool-used-final 0
```

Visible output:

```text
dynsmoke-start
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M83 UEFI manifest reports kernel bytes 1,298,656, checksum `0x85822D9F`, and SHA-256 `66a1620e401afd42d6ce90678bf0c5fab1a132d6d1fba347edb1d0170ff6bdb5`.

M83 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, TLS for dynamically linked libraries, and general dynamic linker parity remain unavailable. The accepted claim is intentionally narrow: one bounded dynamic ET_EXEC path with fixed supported interpreter, fixed relocation set, fixed startup shim, normal task registration, visible output, and clean exit.

## M84 Dynamic Getpid Breadth Proof

M84 is accepted on the UEFI Product path with `linux /APPS/DYNGETPID`. It runs a second dynamic ET_EXEC artifact through the same supported interpreter and proves one new dynamic runtime surface: the app has three PLT relocations, including `getpid`, and the relocation walker binds all three supported libc providers before normal dynamic execution. The app calls `getpid()`, writes `dyngetpid-pass`, exits through `exit_group(231)`, and leaves no process-root or page-fault residue.

Staged artifacts:

- `/APPS/DYNGETPID`, SHA-256 `7BC91398FC2CABE11FB067376D417E5DF8057DACF21214E454A4D9AFA62223CB`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x52001060`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

M84 also generalizes the UEFI boot-media Linux staging path. `tools/build.ps1` now writes the selected staged app/interpreter filenames into `arch_build.h`, the UEFI loader records normalized lowercase slash paths in `boot_info`, and `boot_media.c` matches boot-media reads against those recorded paths while keeping the legacy `/apps/dynldlimit` and `/apps/ldlimit` compatibility fallback. This fixes the earlier hard-coded boot-media fallback that recognized only `DYNLDLIMIT`.

M84 acceptance telemetry:

```text
drs-realbin path /APPS/DYNGETPID provenance 1 source 2 nvme-read 0 boot-media-read 1 elf 1 static 0 elf-type 2 elf-load 4 elf-interp 1 interp-supported 1 interp-file-read 1 interp-file-elf 1 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 3 dynamic-binding-total 7 dynamic-binding-supported 3 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 3 dynamic-jmprel-symbol getpid dynamic-jmprel-symbol-checksum 0x7CC21180 dynamic-reloc-dry-apply-ready 7 dynamic-reloc-apply 1 dynamic-reloc-apply-total 7 dynamic-reloc-apply-write 7 dynamic-reloc-apply-readback 7 dynamic-reloc-apply-jmprel-readback 0x0000000047811180 dynamic-libc-start-main 1 dynamic-libc-start-main-readback 0x0000000047811BF0 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-error 0 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 18 dynamic-transfer-ready 1 dynamic-transfer-rip 0x000000004780105F dynamic-transfer-rsp 0x000000005300FE30 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 15 dynamic-exit-code 0x00000000 mapped 8 pages 10 stack 16 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 low-pdpt-present 0 syscall-entry-high 1 idt-high 1 kernel-entry-high-ready 1 syscall-root-repair 0 syscall-root-reload 4 cr3-start 1 cr3-exit 1 task 0 started 1 console-bytes 15 exit 0 cleanup 1 write 1 write-bytes 15 page-faults 0 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dyngetpid-pass
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M84 UEFI manifest reports kernel bytes 1,298,656, checksum `0x1B965DF5`, and SHA-256 `94c3b5d21af2f87a8d2518abc25b3c793db2ddeae53e1dee8cc2992ce95bb8e5`.

M84 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic TLS, and general dynamic linker parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can now run more than one app artifact and bind a broader libc provider set including `getpid`.

## M85 Dynamic Libc Helper Breadth Proof

M85 is accepted on the UEFI Product path with `linux /APPS/DYNHELPER`. It runs a third dynamic ET_EXEC artifact through the same supported interpreter and proves the app-entry dynamic path can bind and execute non-syscall libc helpers. The program calls `strlen`, `memcpy`, `strcmp`, and `puts`, writes `dynhelper-pass`, exits through `exit_group(231)`, and leaves no process-root or page-fault residue.

Staged artifacts:

- `/APPS/DYNHELPER`, SHA-256 `B30ABD53CF32C9C9E6BEB77FF89DE02C57E1442D15A13EB944D514B794B8A9C8`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

M85 also corrects the supported dynamic transfer target from the staged interpreter smoke entry to the relocated app entry and maps the generated `libc-x64.so` shim into the live process root before transfer. The dynamic launcher initializes the Linux persona before libc-shim mapping and skips duplicate persona initialization in the common attach path. The libc shim load self-check now validates the fixed addresses already computed in the load result instead of walking low-address static export names while executing on a process CR3 after M62 low-compat removal.

M85 acceptance telemetry:

```text
drs-realbin path /APPS/DYNHELPER provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol memcpy dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-reloc-apply-jmprel-readback 0x0000000047811200 dynamic-libc-start-main 1 dynamic-libc-start-main-readback 0x0000000047811BF0 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 15 dynamic-exit-code 0x00000000 mapped 8 pages 10 stack 16 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 low-pdpt-present 0 syscall-entry-high 1 idt-high 1 kernel-entry-high-ready 1 syscall-root-repair 0 cr3-start 1 cr3-exit 1 task 0 started 1 console-bytes 15 exit 0 cleanup 1 write 2 write-bytes 15 page-faults 0 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynhelper-pass
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M85 UEFI manifest reports kernel bytes 1,298,656, checksum `0x935CD0AB`, and SHA-256 `a542589f65779ca1ddc7baa6994c124f8777c02fa89dcf2aaf78c96951a52357`.

M85 regression: `linux /APPS/DYNGETPID` still prints `dyngetpid-pass` with `dynamic-jmprel-symbol getpid`, `dynamic-transfer-rip 0x0000000052001060`, `dynamic-console-bytes 15`, `exit 0`, and `page-faults 0`.

M85 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic TLS, and general dynamic linker parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can now run multiple app artifacts and bind a broader libc provider set including non-syscall helpers.

## M86 Dynamic Environment And Stdio Breadth

M86 is accepted on the UEFI Product path with `linux /APPS/DYNENVSTDIO`. It runs a fourth dynamic ET_EXEC artifact through the same supported interpreter and proves the app-entry dynamic path can bind the inherited four-entry launch environment into the generated libc shim, then use `getenv`, `strcmp`, `printf`, `fputs`, and `fwrite` from dynamic `main`. The program verifies `argc`, `argv`, `envp`, `PATH`, `USER`, `HOME`, and `PWD`, writes three visible output lines, exits through `exit_group(231)`, and leaves no process-root or page-fault residue.

Staged artifacts:

- `/APPS/DYNENVSTDIO`, SHA-256 `7713DA42475439C1020A7AF932B9C53983CAF0FC8DB41BFD78BF3F701DFBEBFA`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

M86 expands the generated libc shim's environment snapshot from one entry to four entries, stores up to 256 bytes of environment text, and binds the live dynamic stack's `envp` content after stack construction. The shim environment binder now validates writable data and text protection against the target process root with pid-aware page protection lookup, which matters after dynamic launch bounces between the kernel root and process root. The supported path remains bounded: no new Linux syscalls, arbitrary shared-library loading, lazy binding, glibc compatibility, or general dynamic linker behavior are introduced.

M86 acceptance telemetry:

```text
drs-realbin path /APPS/DYNENVSTDIO provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol printf dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-libc-start-main 1 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-stack-error 0 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 44 dynamic-exit-code 0x00000000 mapped 8 pages 10 stack 16 envc 4 root-distinct 1 low-compat 0 syscall-root-repair 0 console-bytes 44 exit 0 cleanup 1 write 3 write-bytes 44 page-faults 0 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynprintf-pass
dynfputs-pass
dynfwrite-pass
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M86 UEFI manifest reports kernel bytes 1,298,656, checksum `0x8A8429E7`, and SHA-256 `5de4a798cb2c768eaa447a992d8e13862f74cc50fa5bf6afa10835af4f93d2d9`.

M86 regression: `linux /APPS/DYNHELPER` still prints `dynhelper-pass` with `dynamic-stack-envc 4`, `dynamic-binding-supported 5`, `dynamic-transfer-rip 0x0000000052001080`, `dynamic-console-bytes 15`, `exit 0`, and `page-faults 0`.

M86 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic TLS, general dynamic linker parity, and unbounded environment mutation remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind the bounded inherited launch environment and exercise a wider generated-libc stdio/helper surface.

## M87 Dynamic Heap And Environment Mutation

M87 is accepted on the UEFI Product path with `linux /APPS/DYNHEAPENV`. It runs a fifth dynamic ET_EXEC artifact through the same supported interpreter and proves the app-entry dynamic path can bind and execute the generated libc shim's bounded heap and environment mutation helpers. The program calls `malloc`, writes and compares heap content, calls `realloc` and verifies the copied prefix, calls `free`, calls `calloc` and verifies zero-filled memory, mutates the inherited `USER` entry with `setenv`, reads it back with `getenv`, writes visible output, exits through `exit_group(231)`, and leaves no process-root or page-fault residue.

Staged artifacts:

- `/APPS/DYNHEAPENV`, SHA-256 `5482AF5167968986BE620CD86AE88385C49D0FCF0F435DF987727C1530AAA463`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x520010D0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M87 trace was useful: `DYNHEAPENV` reached dynamic `main`, completed the heap portion far enough to report `mmap 3 mmap-bytes 12288`, then exited 14 with no console output because `setenv("USER", "pilot", 1)` returned failure. Root cause was confined to the generated `setenv` helper: it compared the requested name against the first env entry and returned `-ENOMEM` on the first mismatch instead of advancing through the bounded environment vector. The fix keeps the same 188-byte helper slot, the same patch offsets, and the same bounded 48-byte per-entry mutation model, but scans all entries before reporting no slot.

M87 acceptance telemetry:

```text
drs-realbin path /APPS/DYNHEAPENV provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 10 dynamic-binding-total 14 dynamic-binding-supported 10 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 10 dynamic-jmprel-symbol strcpy dynamic-reloc-apply 1 dynamic-reloc-apply-total 14 dynamic-reloc-apply-write 14 dynamic-reloc-apply-readback 14 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-stack-error 0 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010D0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 28 dynamic-exit-code 0x00000000 mmap 3 mmap-bytes 12288 mmap-denial 0 write 4 write-bytes 28 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 28 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynheap-pass
dynsetenv-pass
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M87 UEFI manifest reports kernel bytes 1,298,656, checksum `0x44EF6F18`, and SHA-256 `0d2f1c2724553ecad1a01dbd300216ea2ab004d0ec59307580b975b133fbb4ad`.

M87 regression: `linux /APPS/DYNENVSTDIO` still prints `dynprintf-pass`, `dynfputs-pass`, and `dynfwrite-pass` with `dynamic-stack-envc 4`, `dynamic-binding-supported 6`, `dynamic-transfer-rip 0x0000000052001090`, `dynamic-console-bytes 44`, `exit 0`, and `page-faults 0`.

M87 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic TLS, general dynamic linker parity, and a real general-purpose allocator remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use the generated libc shim's existing bounded page-backed heap helpers and mutate an existing inherited environment entry.

## M88 Dynamic Pthread Helper Smoke

M88 is accepted on the UEFI Product path with `linux /APPS/DYNTHREAD`. It runs a sixth dynamic ET_EXEC artifact through the same supported interpreter and proves the app-entry dynamic path can bind and execute generated libc shim pthread helpers on top of the existing M64-M66 clone/thread/wait/futex foundation. The program locks a mutex in the main thread, creates one pthread, lets the worker lock the same mutex, mutates shared process memory, joins the worker, verifies the final shared value, writes `dynpthread-pass`, exits through `exit_group(231)`, and leaves no process-root, waiter, futex, or page-fault residue.

Staged artifacts:

- `/APPS/DYNTHREAD`, SHA-256 `749582EF277B19EE11795928199A941E4BD5E81D502B7F2B60104A30115B894A`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M88 trace exposed a helper layout bug: the generated `pthread_join` helper at RVA `0x1940` overlapped the tail of the `pthread_create` helper/trampoline slot, causing an invalid opcode at `RIP 0x0000000047811944` when the child returned through the trampoline. The fix moves `pthread_join` to RVA `0x1950`. The second trace exposed the real scheduler/ABI edge: the child reached `exit(60)`, but clone-thread exits recorded exit state without waking the parent blocked in `wait4`/`pthread_join`, leaving no runnable task and falling through toward a user return on the kernel root CR3. The fix completes blocked waiters for clone-thread exits the same way fork-child exits already complete blocked `wait4`.

M88 acceptance telemetry:

```text
drs-realbin path /APPS/DYNTHREAD provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol pthread_create dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 16 dynamic-exit-code 0x00000000 mmap 1 mmap-bytes 16384 write 2 write-bytes 16 futex-wake 4 futex-waiters-final 0 thread-exit-cleartid 1 clone-thread 1 clone-thread-success 1 clone-denial 0 clone-last-flags 0x01310F00 clone-shared-cr3 1 clone-shared-vma 1 clone-shared-fd 1 wait4 1 wait4-reap 1 wait4-last-exit-code 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 16 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynpthread-pass
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M88 UEFI manifest reports kernel bytes 1,298,656, checksum `0x4F4DFCB5`, and SHA-256 `71568191efe2874375f5c2eef376dcd460fc80e3b1476bfde9d7626a2d448a40`.

M88 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic TLS relocation models, pthread condition variables, contended dynamic pthread mutexes, and general dynamic linker parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind the generated pthread helpers and execute one joined dynamic pthread with shared address-space state and clean clone-thread exit/wait cleanup.

## M89 Dynamic Pthread Contention And TLS Breadth

M89 is accepted on the UEFI Product path with `linux /APPS/DYNPTLS`. It runs a seventh dynamic ET_EXEC artifact through the same supported interpreter and proves the app-entry dynamic path can bind and execute the generated libc shim condition-variable and pthread TLS helpers on top of the existing clone/thread/wait/futex foundation. The program creates four pthreads, gives each worker a distinct `pthread_key_create`/`pthread_setspecific` value, has the workers signal a condition variable while the main thread blocks in `pthread_cond_wait`, joins all workers after they have exited, verifies the accumulated result, writes `dynptls-pass`, exits through `exit_group(231)`, and leaves no process-root, waiter, futex, or page-fault residue.

Staged artifacts:

- `/APPS/DYNPTLS`, SHA-256 `47D6FCCA829DE00FE42814155DF06C5EE925FCACC00A20CEF49C95067E6D8A6F`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x520010E0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M89 trace proved the new dynamic helper breadth but failed before output with exit 7 and `wait4 0 wait4-reap 0`. The root cause was a generated libc shim semantic bug: `pthread_create` passed the public `pthread_t *` as both `CLONE_PARENT_SETTID` and `CLONE_CHILD_CLEARTID`, so thread exit correctly zeroed the same handle that later `pthread_join` needed. The fix adds 16 shim-owned clear-tid words in the bounded libc data page, repacks helper RVAs inside the existing text page, and passes the application `pthread_t *` only as the parent tid while using a private clear-tid word for child tid clear/wake.

M89 acceptance telemetry:

```text
drs-realbin path /APPS/DYNPTLS provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 11 dynamic-binding-total 15 dynamic-binding-supported 11 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 11 dynamic-jmprel-symbol pthread_cond_signal dynamic-reloc-apply 1 dynamic-reloc-apply-total 15 dynamic-reloc-apply-write 15 dynamic-reloc-apply-readback 15 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010E0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 13 dynamic-exit-code 0x00000000 mmap 4 mmap-bytes 65536 write 2 write-bytes 13 futex-wait 4 futex-wake 17 futex-woken 4 futex-waiters-final 0 thread-exit-cleartid 4 clone-thread 4 clone-thread-success 4 clone-denial 0 clone-last-flags 0x01310F00 clone-shared-cr3 1 clone-shared-vma 1 clone-shared-fd 1 fs-set 4 fs-save 8 fs-restore 9 wait4 4 wait4-reap 4 wait4-last-exit-code 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 13 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynptls-pass
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M89 UEFI manifest reports kernel bytes 1,298,656, checksum `0xA3DDFB72`, and SHA-256 `ddb46bc3ad9d89297605e21c394629866be7b31b50e4f6346b3b39355d6a6d83`.

M89 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic TLS relocation models beyond the bounded shim helper table, full pthread attribute support, timed condition waits, and general dynamic linker parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind generated pthread condition/TLS helpers and run four joined dynamic pthreads with contended futex wait/wake and stable join handles.

## M90 Dynamic Linux VFS File I/O Breadth

M90 is accepted on the UEFI Product path with `linux /APPS/DYNFILEIO`. It runs an eighth dynamic ET_EXEC artifact through the same supported interpreter and proves the boot-media dynamic path can receive scoped NVMe VFS read authority, open `/nvme/apps/data/file.txt`, read FAT-backed file bytes through generated libc `open`/`read`/`write`/`close` syscall stubs, write the result through the brokered console, close the fd, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNFILEIO`, SHA-256 `42B199D95374ADC5F8F349405423E2F6976394AD8FD4DC04F88DD3E56F56354F`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M90 trace failed before visible output with exit 1, `vfs-nvme-bind 0`, `read 0`, `write 0`, and `console-bytes 0`. The root cause was that boot-media-staged Linux launches did not thread the shell's scoped NVMe read capability into the live Linux process, so the dynamic app could execute but could not bind the Linux VFS `/nvme` backend. The fix threads that capability through the boot-media launch API, binds the Linux VFS whenever a valid NVMe capability is present, and releases it on cleanup only if the process actually acquired the binding. Boot-media dynamic launches still remain available without NVMe; they simply report no VFS bind.

M90 acceptance telemetry:

```text
drs-realbin path /APPS/DYNFILEIO provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 37 dynamic-exit-code 0x00000000 read 1 read-bytes 27 write 2 write-bytes 37 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 37 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynfileio:Nested FAT32 path fixture
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M90 UEFI manifest reports kernel bytes 1,298,656, checksum `0xACE1384D`, and SHA-256 `dd466bda3fc37f85f1195b3bb774642d0a93713c909391e0c7887354218b378d`.

M90 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic writes to NVMe files, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind scoped NVMe VFS read authority and use generated libc file-I/O stubs to open, read, write to stdout, and close one real FAT-backed file cleanly.

## M91 Dynamic File Metadata And Seek Breadth

M91 is accepted on the UEFI Product path with `linux /APPS/DYNSEEK`. It runs a ninth dynamic ET_EXEC artifact through the same supported interpreter and proves the boot-media dynamic path can bind scoped NVMe VFS read authority, call generated libc `stat`, `fstat`, `lseek`, `read`, `write`, and `close` bindings against `/nvme/apps/data/file.txt`, seek to deterministic offsets in a FAT-backed file, read the expected file slices, print them through the brokered console, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNSEEK`, SHA-256 `22F4206B5DFA3A9048A08F2FF31E9931AC969DFC404EA684F2E7B0D0013AEE62`, external musl-cross ET_EXEC linked at `0x52000000`, entry `0x520010B0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M91 trace proved dynamic `stat` and `fstat` immediately but exited 6 before any read because `lseek(fd, 7, SEEK_SET)` was denied on the NVMe VFS fd. The root cause was that `fd64_seek` only accepted `FD64_TYPE_RAMFS_NODE`; Linux VFS files are represented as `FD64_TYPE_DEVICE` handles even though their reads already honor `entry->file_offset`. The fix allows seek on Linux VFS device fds only when `linux_vfs64_fstat` proves a file-like node, preserving denial for terminals, pipes, sockets, directories, and unknown devices. The real-binary telemetry now also emits `lseek` and `lseek-denial` counters so this path is directly auditable.

M91 acceptance telemetry:

```text
drs-realbin path /APPS/DYNSEEK provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 8 dynamic-binding-total 12 dynamic-binding-supported 8 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 8 dynamic-jmprel-symbol lseek dynamic-reloc-apply 1 dynamic-reloc-apply-total 12 dynamic-reloc-apply-write 12 dynamic-reloc-apply-readback 12 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-stack-auxv 19 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010B0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 24 dynamic-exit-code 0x00000000 stat 1 stat-denial 0 stat-fault 0 fstat 1 fstat-denial 0 fstat-fault 0 lseek 2 lseek-denial 0 read 2 read-bytes 15 write 1 write-bytes 24 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 24 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynseek:FAT32 pa:fixture
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M91 UEFI manifest reports kernel bytes 1,298,656, checksum `0xF1D59AE7`, and SHA-256 `60cab95f91c2e0e7ca003358bb294daadac24778574a1f3d95c02517a65a073e`.

M91 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic writes to NVMe files, directory enumeration from dynamic libc, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind scoped NVMe VFS read authority and use generated libc metadata, offset, and file-I/O stubs against one real FAT-backed file cleanly.

## M92 Dynamic Directory Enumeration Breadth

M92 is accepted on the UEFI Product path with `linux /APPS/DYNDIR`. It runs a tenth dynamic ET_EXEC artifact through the same supported interpreter and proves the boot-media dynamic path can bind scoped NVMe VFS read authority, resolve a generated libc `getdents64` syscall stub, open `/nvme/apps`, enumerate raw Linux `dirent64` records from the FAT-backed VFS, stat the returned `data` directory entry, write `dyndir:data`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNDIR`, SHA-256 `A54BE4145223AC87CA1E1C3ECF8926FEE6F5CC79CD27463C8D68812A6D30E802`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

Implementation scope: add only the generated libc syscall export for `getdents64` at reserved RVA `0x000011F0`. The kernel ABI syscall implementation, VFS directory cursor path, and real-binary telemetry already existed; M92 exposes that existing syscall to dynamic ET_EXEC programs through the in-tree libc shim. No new Linux syscall behavior, arbitrary shared-library loading, lazy binding, glibc compatibility, or BIOS path expansion is claimed.

M92 acceptance telemetry:

```text
drs-realbin path /APPS/DYNDIR provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 12 dynamic-exit-code 0x00000000 getdents64 1 getdents64-entries 3 getdents64-bytes 88 stat 1 stat-denial 0 stat-fault 0 write 1 write-bytes 12 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-readdirs 4 vfs-nvme-dirents 3 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 12 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dyndir:data
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M92 UEFI manifest reports kernel bytes 1,298,656, checksum `0xAD882A99`, and SHA-256 `ee1b5157188b5b4a489f996c58c70f98423cebc7d79ef57789443d3ca806ce51`.

M92 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, dynamic writes to NVMe files, libc `opendir`/`readdir` wrappers, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind scoped NVMe VFS read authority and use a generated libc `getdents64` syscall stub to enumerate a real FAT-backed directory and stat one returned entry cleanly.

## M93 Dynamic Cwd And Relative Path Breadth

M93 is accepted on the UEFI Product path with `linux /APPS/DYNCWD`. It runs an eleventh dynamic ET_EXEC artifact through the same supported interpreter and proves the boot-media dynamic path can bind scoped NVMe VFS read authority, resolve generated libc `getcwd`, `chdir`, and `readlink` syscall stubs, update cwd to `/nvme/apps`, open `data/file.txt` by relative path, read FAT-backed file content, read `/proc/self/exe` symlink metadata, write `dyncwd:/nvme/apps:Nested FAT32 path fixture:/proc/self/exe`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNCWD`, SHA-256 `B36DEFEEEFD22F0E050A20210D7F24575A0ABC3C0ED8F20F73386AEBC1B471D4`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010B0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M93 trace faulted before real-binary telemetry with RIP `0x0000000047811012`, CR2 `0x0000000052001000`, and page-fault error `0x7`. The root cause was a generated libc layout mistake: the new syscall stubs were initially placed at `0x1010` and `0x1018`, overlapping the longer `pthread_cond_init` helper that begins at `0x1008`. The accepted fix moves the new stubs into non-overlapping helper gaps at `0x1410`, `0x1430`, and `0x1450`.

Implementation scope: add only generated libc syscall exports for existing kernel ABI syscalls `getcwd(79)`, `chdir(80)`, and `readlink(89)`. The kernel ABI syscall implementations, cwd state, path canonicalization, proc symlink provider, VFS read path, and telemetry already existed. No new Linux syscall behavior, arbitrary shared-library loading, lazy binding, glibc compatibility, or BIOS path expansion is claimed.

M93 acceptance telemetry:

```text
drs-realbin path /APPS/DYNCWD provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 8 dynamic-binding-total 12 dynamic-binding-supported 8 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 8 dynamic-jmprel-symbol readlink dynamic-reloc-apply 1 dynamic-reloc-apply-total 12 dynamic-reloc-apply-write 12 dynamic-reloc-apply-readback 12 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010B0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 59 dynamic-exit-code 0x00000000 readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 getcwd 2 getcwd-bytes 13 getcwd-denial 0 getcwd-fault 0 path-relative 1 path-dot 0 path-dotdot 0 path-trailing 0 path-fault 0 chdir 1 chdir-denial 0 chdir-fault 0 read 1 read-bytes 27 write 7 write-bytes 59 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 59 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dyncwd:/nvme/apps:Nested FAT32 path fixture:/proc/self/exe
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M93 UEFI manifest reports kernel bytes 1,298,656, checksum `0x5C6B2453`, and SHA-256 `b3516c247c55c182e93673e86b58e4545d1ff4bd57493446e9a6bd08e9b0b88c`.

M93 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, `readlinkat`, dynamic writes to NVMe files, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind scoped NVMe VFS read authority and use generated libc cwd/path syscall stubs to resolve relative paths and proc symlink metadata cleanly.

## M94 Dynamic Vectored I/O And Readiness Breadth

M94 is accepted on the UEFI Product path with `linux /APPS/DYNVEC`. It runs a twelfth dynamic ET_EXEC artifact through the same supported interpreter and proves the boot-media dynamic path can bind scoped NVMe VFS read authority, resolve generated libc `readv`, `writev`, and `poll` syscall stubs, observe brokered stdout readiness with `POLLOUT`, read FAT-backed file content into multiple iovecs, write `dynvec:Nested:FAT32 path fixture` through vectored console output, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNVEC`, SHA-256 `2F4CB3560E98FF4585731F48673C5B86C2D2B82CD8D8B2D4284E9F5E6BD49915`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M94 trace proved the new dynamic `poll` binding against brokered stdout with `poll-ready 1 poll-last-revents 0x00000004`, then exited 4 after successful `readv 1 readv-bytes 27` because the smoke program split the fixture string as `Nested F` plus `AT32...`. That was a smoke-artifact assertion bug, not a kernel failure. The accepted artifact reads the same 27 bytes as `Nested` plus ` FAT32 path fixture` and writes the proof line through `writev`.

Implementation scope: add only generated libc syscall exports for existing kernel ABI syscalls `readv(19)`, `writev(20)`, and `poll(7)` at reserved RVAs `0x1468`, `0x1470`, and `0x1478`. The kernel ABI syscall implementations, VFS read path, brokered stdout write path, readiness telemetry, and cleanup path already existed. No new Linux syscall behavior, arbitrary shared-library loading, lazy binding, glibc compatibility, file `POLLIN`, sockets, epoll, or BIOS path expansion is claimed.

M94 acceptance telemetry:

```text
drs-realbin path /APPS/DYNVEC provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol writev dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 33 dynamic-exit-code 0x00000000 readv 1 readv-bytes 27 writev 1 writev-bytes 33 poll 1 ppoll 0 poll-ready 1 poll-last-revents 0x00000004 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 33 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynvec:Nested:FAT32 path fixture
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M94 UEFI manifest reports kernel bytes 1,298,656, checksum `0x255B160B`, and SHA-256 `68136299ce6e66106b5684f5ae6476a6e3cd3f596c33e45616e2f38763657752`.

M94 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, file-descriptor readiness beyond brokered stdout `POLLOUT`, sockets, epoll, dynamic writes to NVMe files, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind scoped NVMe VFS read authority and use generated libc vectored I/O and readiness syscall stubs against one real FAT-backed file and brokered stdout cleanly.

## M95 Dynamic Fstatat Metadata Breadth

M95 is accepted on the UEFI Product path with `linux /APPS/DYNFSTATAT`. It runs a thirteenth dynamic ET_EXEC artifact through the same supported interpreter and proves the boot-media dynamic path can bind scoped NVMe VFS read authority, resolve a generated libc `newfstatat` wrapper, update cwd to `/nvme/apps`, stat relative file and directory paths through `newfstatat(AT_FDCWD, ...)`, validate the FAT-backed file size and regular-file/directory mode bits, write `dynfstatat:27:file-dir`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNFSTATAT`, SHA-256 `35504B625F60B8C4DAAF464B57219466AA49CABCCDC0082F6907505C1C1DE8A0`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001070`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M95 trace proved symbol binding but failed with `newfstatat-denial 1` because the generic 8-byte syscall stub did not move the fourth C ABI argument from `rcx` to Linux syscall ABI `r10`. The second trace moved `rcx` to `r10` but still failed with `path-fault 1` because a C `int dirfd` zero-extended `AT_FDCWD`. The accepted fix gives `newfstatat` a dedicated wrapper at `0x1D40` that sign-extends `edi` into `rdi`, moves `rcx` into `r10`, then issues syscall 262.

Implementation scope: add only the generated libc wrapper/export for existing kernel ABI syscall `newfstatat(262)`. The kernel ABI syscall implementation, cwd state, path canonicalization, VFS stat path, stat telemetry, and cleanup path already existed. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, `readlinkat`, file mutation, or BIOS path expansion is claimed.

M95 acceptance telemetry:

```text
drs-realbin path /APPS/DYNFSTATAT provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 4 dynamic-binding-total 8 dynamic-binding-supported 4 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 4 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 8 dynamic-reloc-apply-write 8 dynamic-reloc-apply-readback 8 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001070 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 newfstatat 2 newfstatat-denial 0 newfstatat-fault 0 path-relative 2 path-fault 0 chdir 1 chdir-denial 0 chdir-fault 0 write 1 write-bytes 23 vfs-nvme-bind 1 vfs-nvme-release 1 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynfstatat:27:file-dir
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M95 UEFI manifest reports kernel bytes 1,298,656, checksum `0xF3DB573B`, and SHA-256 `01bae43f4fce2c6dc65e754e768420fe801786aa23df81a4192543e2eb21849b`.

M95 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, `readlinkat`, filesystem mutation, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind scoped NVMe VFS read authority and use a generated libc `newfstatat` wrapper against relative file and directory paths cleanly.

## M96 Dynamic Openat Relative File-Read Breadth

M96 is accepted on the UEFI Product path with `linux /APPS/DYNOPENAT`. It runs a fourteenth dynamic ET_EXEC artifact through the same supported interpreter and proves the boot-media dynamic path can bind scoped NVMe VFS read authority, resolve a generated libc `openat` wrapper, update cwd to `/nvme/apps`, open a relative FAT-backed file through `openat(AT_FDCWD, "data/file.txt", O_RDONLY, 0)`, read the file content, write `dynopenat:Nested:FAT32`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNOPENAT`, SHA-256 `C28EC4FA1D26912A31B36591F9FCE73E07D086FC3F80CB3C4514C1AC374AD7BB`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

Implementation scope: add only the generated libc wrapper/export for existing kernel ABI syscall `openat(257)` at `0x1D50`, using the same four-argument wrapper shape as `newfstatat` so the fourth C ABI argument is moved from `rcx` to Linux syscall ABI `r10`. The kernel ABI syscall implementation, cwd state, path canonicalization, VFS open/read path, fd close path, and cleanup path already existed. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, file mutation, or BIOS path expansion is claimed.

M96 acceptance telemetry:

```text
drs-realbin path /APPS/DYNOPENAT provenance 1 source 2 boot-media-read 1 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 path-relative 1 path-fault 0 chdir 1 chdir-denial 0 chdir-fault 0 openat 1 read 1 read-bytes 27 write 1 write-bytes 23 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynopenat:Nested:FAT32
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M96 UEFI manifest reports kernel bytes 1,298,656, checksum `0x53D056E4`, and SHA-256 `7b08650626686a606aa4db63be73c4d23f30cf9df1086e87ce9215309e4cce94`.

M96 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can bind scoped NVMe VFS read authority and use a generated libc `openat` wrapper against a cwd-relative file path cleanly.

## M97 Dynamic Openat Dirfd-Relative Lookup Breadth

M97 is accepted on the UEFI Product path with `linux /APPS/DYNODIR`. It runs a fifteenth dynamic ET_EXEC artifact through the same supported interpreter and proves the dynamic path can bind scoped NVMe VFS read authority, resolve a generated libc `openat` wrapper, open `/nvme/apps/data` as a VFS-backed directory fd, resolve `file.txt` relative to that directory fd through `openat(datafd, "file.txt", O_RDONLY, 0)`, read the file content, write `dynopenatdirfd:Nested:FAT32`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNODIR`, SHA-256 `43B960D929BC6E15B573E65166FEB2EDFFDBBC2573BE19372697056CDACA0E23`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

Implementation scope: no kernel code change. M97 reuses the generated libc `openat(257)` wrapper added in M96 and the existing kernel path canonicalization branch that derives the base path from a VFS-backed directory fd. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, file mutation, or BIOS path expansion is claimed.

M97 acceptance telemetry:

```text
drs-realbin path /APPS/DYNODIR provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 28 dynamic-exit-code 0x00000000 path-relative 1 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 openat 2 read 1 read-bytes 27 write 1 write-bytes 28 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 28 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynopenatdirfd:Nested:FAT32
```

Final reserves remain UEFI 798,496 bytes and BIOS 101 sectors. The M97 UEFI manifest is unchanged from M96 and reports kernel bytes 1,298,656, checksum `0x53D056E4`, and SHA-256 `7b08650626686a606aa4db63be73c4d23f30cf9df1086e87ce9215309e4cce94`.

M97 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use a generated libc `openat` wrapper to resolve a file relative to a real VFS-backed directory fd.

## M98 Dynamic Fchdir Directory-Fd Cwd Handoff

M98 is accepted on the UEFI Product path with `linux /APPS/DYNFCHDIR`. It runs a sixteenth dynamic ET_EXEC artifact through the same supported interpreter and proves the dynamic path can bind scoped NVMe VFS read authority, resolve a generated libc `fchdir` wrapper, open `/nvme/apps/data` as a VFS-backed directory fd, call `fchdir(datafd)` to update the Linux persona cwd, resolve `file.txt` relative to that new cwd through `openat(AT_FDCWD, "file.txt", O_RDONLY, 0)`, read the file content, write `dynfchdir:Nested:FAT32`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNFCHDIR`, SHA-256 `0F4B1A7B2D001568C801B7930D8BD4F9F9ED6BE4EA239AB022AE59D4CEA48E08`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

The first M98 trace proved the dynamic binding but faulted at `rip 0x0000000047811D60 cr2 0x0000000000000003` because the first chosen `fchdir` RVA was outside the executable syscall-stub area and generic syscall-stub emission correctly refused to write it. The accepted fix places `fchdir` at `0x11F8`, the 8-byte executable text gap between `getdents64` and `memcpy`.

Implementation scope: add only the generated libc wrapper/export for existing kernel ABI syscall `fchdir(81)`. The kernel ABI syscall implementation, cwd state, VFS-backed fd path lookup, path canonicalization, open/read path, fd close path, and cleanup path already existed. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, file mutation, or BIOS path expansion is claimed.

M98 acceptance telemetry:

```text
drs-realbin path /APPS/DYNFCHDIR provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol fchdir dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-reloc-apply-jmprel-readback 0x00000000478111F8 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 path-relative 1 path-fault 0 chdir 0 fchdir 1 chdir-denial 0 chdir-fault 0 openat 2 read 1 read-bytes 27 write 1 write-bytes 23 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynfchdir:Nested:FAT32
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M98 UEFI manifest reports kernel bytes 1,298,656, checksum `0x28E88BAA`, and SHA-256 `3f782d4294c6966d152b46b7fad10be282108c6ed4ba4244eab48dc3cef9a64c`.

M98 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use a generated libc `fchdir` wrapper to update cwd from a real VFS-backed directory fd, then read a cwd-relative file cleanly.

## M99 Dynamic Fcntl Descriptor Flag Breadth

M99 is accepted on the UEFI Product path with `linux /APPS/DYNFCNTL`. It runs a seventeenth dynamic ET_EXEC artifact through the same supported interpreter and proves the dynamic path can bind scoped NVMe VFS read authority, resolve a generated libc `fcntl` wrapper, open `/nvme/apps/data/file.txt` as a real VFS fd, use `F_GETFD`, `F_SETFD`, `F_GETFL`, and the supported `F_SETFL` subset, read the file content after the flag updates, write `dynfcntl:cloexec:nonblock`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNFCNTL`, SHA-256 `D20CFF658A3E96FF67F1915943D7948207A4ADDDA649905FFB80F9B5E596F5E3`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

Implementation scope: add only the generated libc wrapper/export for existing kernel ABI syscall `fcntl(72)` at `0x11E8`, the 8-byte executable text gap between `futex` and `getdents64`, and add `fcntl`/`fcntl-denial` launch telemetry. The kernel ABI syscall implementation, VFS fd table, descriptor flag storage, status flag storage, open/read path, fd close path, and cleanup path already existed. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, file mutation, `F_DUPFD` dynamic proof, or BIOS path expansion is claimed.

M99 acceptance telemetry:

```text
drs-realbin path /APPS/DYNFCNTL provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 26 dynamic-exit-code 0x00000000 openat 1 fcntl 6 fcntl-denial 0 read 1 read-bytes 27 write 1 write-bytes 26 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 26 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynfcntl:cloexec:nonblock
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M99 UEFI manifest reports kernel bytes 1,298,656, checksum `0x810A2089`, and SHA-256 `ad38277151dc19f404a8f600411392ebc934fdbc01528bc3c83dcf9d3eaf1a7f`.

M99 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, broad Linux VFS parity, and dynamic proof for descriptor duplication remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use a generated libc `fcntl` wrapper to observe and update descriptor/status flags on one real VFS fd cleanly.

## M100 Dynamic Fcntl Descriptor Duplication Breadth

M100 is accepted on the UEFI Product path with `linux /APPS/DYNFDUP`. It runs an eighteenth dynamic ET_EXEC artifact through the same supported interpreter and proves the dynamic path can bind scoped NVMe VFS read authority, resolve generated libc `fcntl`, `openat`, `read`, `close`, and `write` wrappers, open `/nvme/apps/data/file.txt` as a real VFS fd, set `FD_CLOEXEC` on the original fd, duplicate it with `F_DUPFD`, confirm the duplicate did not inherit `FD_CLOEXEC`, read the same FAT-backed file through the duplicate descriptor, close both descriptors, write `dynfdup:dupfd:no-cloexec`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNFDUP`, SHA-256 `7401F71B8B9740DE364BDC9CA3ED931C7D751F9619432AD4C9F9171EDF159D22`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001090`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

First trace before accepted fix: `linux /APPS/DYNFDUP` printed `dynfdup:read-fail` and exited 5 with `fcntl 3`, `fcntl-denial 0`, `read 0`, `read-bytes 0`, and `vfs-nvme-reads 0`. The descriptor table duplication succeeded, and the duplicate correctly did not inherit `FD_CLOEXEC`, but the VFS-backed read failed before NVMe access because the Linux VFS per-fd path sidecar record had not been copied to the duplicated fd.

Second trace before accepted fix: the new VFS sidecar-copy helper still failed because it called `linux_vfs64_init()` unconditionally and reset existing sidecar records during duplication. The accepted fix uses the normal guarded initialization pattern and copies the path sidecar only when the source fd has one.

Implementation scope: add `linux_vfs64_dup_fd_path()` and call it from the existing `dup`, `dup2`, `dup3`, and `F_DUPFD` paths so duplicated descriptors preserve Linux VFS path sidecar metadata. No new Linux ABI syscall, generated libc export, arbitrary shared-library loading, lazy binding, glibc compatibility, filesystem mutation, or BIOS path expansion is claimed.

M100 acceptance telemetry:

```text
drs-realbin path /APPS/DYNFDUP provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 6 dynamic-binding-total 10 dynamic-binding-supported 6 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 6 dynamic-jmprel-symbol write dynamic-reloc-apply 1 dynamic-reloc-apply-total 10 dynamic-reloc-apply-write 10 dynamic-reloc-apply-readback 10 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001090 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 25 dynamic-exit-code 0x00000000 openat 1 fcntl 3 fcntl-denial 0 read 1 read-bytes 27 write 1 write-bytes 25 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 1 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 25 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynfdup:dupfd:no-cloexec
```

Final reserves are UEFI 798,496 bytes and BIOS 101 sectors. The M100 UEFI manifest reports kernel bytes 1,298,656, checksum `0x256CFA6D`, and SHA-256 `799b084e04e60402555bd1e230c87c692bcacd8099560528d125fcf8ab300d79`.

M100 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, broad Linux VFS parity, and direct dynamic proof for `dup(32)`, `dup2(33)`, or `dup3(292)` remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use generated libc `fcntl(F_DUPFD)` to duplicate one real VFS fd cleanly while preserving the sidecar metadata needed for reads through the duplicate.

## M101 Dynamic Dup Syscall Descriptor Duplication Breadth

M101 is accepted on the UEFI Product path with `linux /APPS/DYNDUP`. It runs a nineteenth dynamic ET_EXEC artifact through the same supported interpreter and proves the dynamic path can bind scoped NVMe VFS read authority, resolve generated libc `dup`, `dup2`, `dup3`, `fcntl`, `openat`, `read`, `close`, and `write` wrappers, duplicate brokered stdout, duplicate real VFS fds by direct syscalls, replace an exact fd with `dup2`, set `FD_CLOEXEC` with `dup3(..., O_CLOEXEC)`, verify the flag with `fcntl(F_GETFD)`, read the same FAT-backed file through each duplicate, close all descriptors, write `dyndup:dup:dup2:dup3`, exit through `exit_group(231)`, and release the Linux VFS binding cleanly.

Staged artifacts:

- `/APPS/DYNDUP`, SHA-256 `FBD8FBAFC838370E6DF112630F661E76C6AC988FF83D1A3024DAA04C936E5A7E`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

First trace before accepted fix: `linux /APPS/DYNDUP` faulted at `rip 0x0000000047811D60 cr2 0xFFFFFFFF8001008D`. The `dup` export resolved to the late syscall-stub area at `0x1D60`, but the generic syscall-stub writer still bounded writes to the executable text range and refused to emit the stub bytes. The accepted fix widens the generic syscall stub emission bound to include the reserved late-stub gap before the dynamic table, matching the already-working late `newfstatat` and `openat` stubs.

Implementation scope: add generated libc wrappers/exports for existing kernel ABI syscalls `dup(32)`, `dup2(33)`, and `dup3(292)` at late stub RVAs `0x1D60`, `0x1D68`, and `0x1D70`; add launch telemetry for `dup`, `dup2`, `dup3`, and `dup-denial`; and fix late-stub emission bounds. The kernel ABI syscall implementations, VFS fd table, VFS sidecar-copy helper from M100, descriptor flag storage, open/read/close paths, and cleanup path already existed. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, filesystem mutation, or BIOS path expansion is claimed.

M101 acceptance telemetry:

```text
drs-realbin path /APPS/DYNDUP provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 9 dynamic-binding-total 13 dynamic-binding-supported 9 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 9 dynamic-jmprel-symbol dup3 dynamic-reloc-apply 1 dynamic-reloc-apply-total 13 dynamic-reloc-apply-write 13 dynamic-reloc-apply-readback 13 dynamic-reloc-apply-jmprel-readback 0x0000000047811D70 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010C0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 21 dynamic-exit-code 0x00000000 openat 3 dup 2 dup2 1 dup3 1 dup-denial 0 fcntl 1 fcntl-denial 0 read 3 read-bytes 81 write 1 write-bytes 21 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 3 vfs-nvme-bytes 27 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 21 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dyndup:dup:dup2:dup3
```

Final reserves are UEFI 794,400 bytes and BIOS 101 sectors. The M101 UEFI manifest reports kernel bytes 1,302,752, checksum `0xF29D42FB`, and SHA-256 `17b45a78f835218e2f73039f5112824be78dc48b18ee7a81b2c162f601f70339`.

M101 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, broad Linux VFS parity, and dynamic proof for pipe creation remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use generated libc `dup`, `dup2`, and `dup3` wrappers to duplicate brokered and real VFS descriptors cleanly while preserving sidecar metadata and descriptor flags.

## M102 Dynamic Pipe Syscall Breadth

M102 is accepted on the UEFI Product path with `linux /APPS/DYNPIPE`. It runs a twentieth dynamic ET_EXEC artifact through the same supported interpreter and proves the dynamic path can resolve generated libc `pipe`, `read`, `write`, and `close` wrappers, create a kernel pipe fd pair through `pipe(22)`, write bytes to the pipe write end, close the write end, read the same bytes from the pipe read end, close the read end, write `dynpipe:hello`, exit through `exit_group(231)`, and leave no live pipe object behind.

Staged artifacts:

- `/APPS/DYNPIPE`, SHA-256 `4DCE063D838B8E88CABB6E00C3A0206FCDD1AF8FBEA46F4F2ABE1A79193F2FB1`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x52001080`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

Implementation scope: add only the generated libc wrapper/export for existing kernel ABI syscall `pipe(22)` at late stub RVA `0x1D78`, the final 8-byte slot before the dynamic table. The kernel ABI pipe syscall implementation, fd table, pipe object pool, pipe read/write/close paths, and pipe cleanup telemetry already existed. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, filesystem mutation, or BIOS path expansion is claimed.

M102 acceptance telemetry:

```text
drs-realbin path /APPS/DYNPIPE provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 5 dynamic-binding-total 9 dynamic-binding-supported 5 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 5 dynamic-jmprel-symbol pipe dynamic-reloc-apply 1 dynamic-reloc-apply-total 9 dynamic-reloc-apply-write 9 dynamic-reloc-apply-readback 9 dynamic-reloc-apply-jmprel-readback 0x0000000047811D78 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x0000000052001080 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 14 dynamic-exit-code 0x00000000 read 1 read-bytes 5 write 2 write-bytes 19 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe-live-final 0 pipe-blocks 0 pipe-wakes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-bytes 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 14 exit 0 cleanup 1 root-cleanup 1 pml4-pool-used-final 0
```

Visible output:

```text
dynpipe:hello
```

Final reserves are UEFI 794,400 bytes and BIOS 101 sectors. The M102 UEFI manifest reports kernel bytes 1,302,752, checksum `0x3603674C`, and SHA-256 `f543ef66788d33a2a2571e2bfb510e9e09fdb74fd6546b05b4735bf92db4a0c2`.

M102 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, broad Linux VFS parity, and dynamic fork-plus-pipe composition remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use a generated libc `pipe` wrapper in one process and clean up both endpoints correctly.

## M103 Dynamic Fork-Plus-Pipe Composition

M103 is accepted on the UEFI Product path with `linux /APPS/DYNFORKPIPE`. It runs a twenty-first dynamic ET_EXEC artifact through the same supported interpreter and proves the dynamic path can resolve generated libc `fork`, `wait4`, `pipe`, `read`, `write`, and `close` wrappers, create a kernel pipe fd pair, fork a child process, copy both pipe endpoints into the child fd table, have the child write `child-pipe` through the inherited pipe write end, reap the child with `wait4`, read the child payload from the parent pipe read end, write `dynforkpipe:child-pipe`, exit through `exit_group(231)`, and leave no live pipe object or process root behind.

Staged artifacts:

- `/APPS/DYNFORKPIPE`, SHA-256 `C3271AC8E598AEEF0480FE042FA1E6A51B64319B46736510D0FC2A43E154C696`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010A0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit`
- `/APPS/LDLIMIT`, SHA-256 `6F713105878C30D817B7ADD4A7ED5D4EE8E01FB6EAB2C80BA10ACEE059C72238`, static musl ET_EXEC linked at `0x47800000`

First trace before accepted ordering: a parent-read-before-wait smoke exited 5 with `pipe-blocks 1`, `pipe-wakes 1`, `fd-fork-pipe-copy 2`, `fork-success 1`, `read 0`, and `wait4 0`. This exposed that blocked pipe-read replay currently returns to user space as a failed/empty read path instead of transparently completing the original read after wake. That behavior is not fixed or claimed by M103; it is the proposed M104 target.

Implementation scope: add generated libc wrappers/exports for existing kernel ABI syscalls `fork(57)` and `wait4(61)` at RVAs `0x1D80` and `0x1D88`, shift the shim dynamic table from `0x1D80` to `0x1DA0` inside the same executable/readable one-page PT_LOAD window, and keep the existing kernel fork, fd inheritance, wait4, pipe, read/write/close, and cleanup paths unchanged. No new Linux ABI syscall implementation, arbitrary shared-library loading, lazy binding, glibc compatibility, filesystem mutation, blocked pipe-read replay, or BIOS path expansion is claimed.

M103 acceptance telemetry:

```text
drs-realbin path /APPS/DYNFORKPIPE provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 5 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-rela 4 dynamic-jmprel 7 dynamic-binding-total 11 dynamic-binding-supported 7 dynamic-binding-missing 0 dynamic-binding-weak-null 4 dynamic-binding-libc 7 dynamic-jmprel-symbol pipe dynamic-reloc-apply 1 dynamic-reloc-apply-total 11 dynamic-reloc-apply-write 11 dynamic-reloc-apply-readback 11 dynamic-reloc-apply-jmprel-readback 0x0000000047811D78 dynamic-stack 1 dynamic-stack-pages 16 dynamic-stack-argc 1 dynamic-stack-envc 4 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010A0 dynamic-transfer-rsp 0x000000005300FE20 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 dynamic-exit-code 0x00000000 read 1 read-bytes 10 write 2 write-bytes 33 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe-live-final 0 fd-fork-pipe-copy 2 fd-fork-pipe-denial 0 fork 1 fork-success 1 fork-enosys 0 fork-denial 0 fork-child-slot 1 fork-child-root-distinct 1 wait4 1 wait4-reap 1 wait4-last-exit-code 7 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 low-compat 0 syscall-root-repair 0 page-faults 0 console-bytes 23 exit 0 cleanup 1
```

Visible output:

```text
dynforkpipe:child-pipe
```

Final reserves are UEFI 794,400 bytes and BIOS 101 sectors. The M103 UEFI manifest reports kernel bytes 1,302,752, checksum `0x504958C8`, and SHA-256 `4ea863a0470fe43f0555e3fd8a7b60a6743d9fb6d01e146535c8bddde33eab75`.

M103 non-claims: arbitrary `PT_INTERP`, arbitrary shared-library search/loading, glibc compatibility, broad relocation families, lazy binding, filesystem mutation, broad Linux VFS parity, `vfork`, dynamic `execve`, and blocked pipe-read replay remain unavailable. The accepted claim is intentionally narrow: the fixed supported-interpreter dynamic path can use generated libc `fork` and `wait4` wrappers with inherited pipe fds and clean up all involved kernel objects.

## M104 Blocked Pipe Read Replay

M104 is accepted on the UEFI Product path with the original parent-read-before-wait `linux /APPS/DYNFORKPIPE` smoke. It proves a parent process can block in `read(pipe)`, let the forked child run and write `child-pipe` through the inherited pipe write end, wake the parent, replay the blocked syscall under the reader's own CR3, copy the pipe payload into the parent user buffer, reap the child with `wait4`, write `dynforkpipe:child-pipe`, exit through `exit_group(231)`, and leave no live pipe object or process root behind.

Passing artifact:

- `/APPS/DYNFORKPIPE`, SHA-256 `0D9E3DDCC388F9609BCF31BCCBF646D8C3DFFF0D65CE040642102206EB87FCC2`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`

Visible console output:

```text
dynforkpipe:child-pipe
```

M104 acceptance telemetry:

```text
drs-realbin path /APPS/DYNFORKPIPE provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 elf-type 2 elf-load 4 elf-interp 1 interp-supported 1 interp-file-read 1 interp-file-elf 1 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 6 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-reloc 1 dynamic-rela 5 dynamic-jmprel 6 dynamic-binding-total 11 dynamic-binding-supported 7 dynamic-binding-missing 0 dynamic-reloc-apply 1 dynamic-reloc-apply-total 11 dynamic-reloc-apply-write 11 dynamic-reloc-apply-readback 11 dynamic-libc-start-main 1 dynamic-stack 1 dynamic-stack-pages 16 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010C0 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 23 mapped 8 pages 11 stack 16 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 syscall-entry-high 1 idt-high 1 descriptor-high 1 kernel-entry-high-ready 1 kernel-cr3-entry 1 syscall-root-repair 0 read 1 read-bytes 10 write 2 write-bytes 33 pipe 1 pipe-create 1 pipe-denials 0 pipe-faults 0 pipe-live-final 0 pipe-blocks 1 pipe-wakes 1 pipe-replays 1 pipe-provider-denials 0 fd-fork-pipe-copy 2 fd-fork-pipe-denial 0 fork 1 fork-success 1 fork-enosys 0 fork-denial 0 fork-child-slot 1 fork-child-root-distinct 1 wait4 1 wait4-reap 1 wait4-last-exit-code 7 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 root-pool-used-final 0 console-bytes 23 exit 0 cleanup 1 page-faults 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

Final reserves are UEFI 794,400 bytes and BIOS 101 sectors. The M104 UEFI manifest reports kernel bytes 1,302,752 and checksum `0x65F58141`.

M104 non-claims: arbitrary dynamic-linker search/loading, glibc compatibility, broad pipe edge cases, `vfork`, dynamic `execve`, sockets, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: blocked pipe-backed descriptor syscalls can replay after wake without leaking a fabricated result to userspace.

## M105 Dynamic Pipe Close/Error Semantics

M105 is accepted on the UEFI Product path with `linux /APPS/DYNPIPECLOSE`. It runs a twenty-third dynamic ET_EXEC artifact through the same supported interpreter and proves two pipe close/error paths that dynamic libc users expect: a parent blocked in `read(pipe)` wakes and replays as EOF when the last writer closes, and a process with a SIGPIPE handler receives SIGPIPE plus the original `-EPIPE` write result when it writes after all read ends close. The run also proves `rt_sigaction` plus `rt_sigreturn` across the dynamic path, child exit collection with `wait4`, and complete cleanup of pipe objects and process roots.

Passing artifact:

- `/APPS/DYNPIPECLOSE`, SHA-256 `93986327A92D798F613F931D93F483CCED8DED838BE78BF7D65324F5FCA2C628`, external musl-cross dynamic ET_EXEC linked at `0x52000000`, entry `0x520010C0`, with `PT_INTERP` requesting `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`

Visible console output:

```text
dynpipeclose:eof
sigpipe-caught
dynpipeclose:done
```

M105 acceptance telemetry:

```text
drs-realbin path /APPS/DYNPIPECLOSE provenance 1 source 1 nvme-read 1 boot-media-read 0 elf 1 static 0 elf-type 2 elf-load 4 elf-interp 1 interp-supported 1 interp-file-read 1 interp-file-elf 1 dynamic-map-attempt 1 dynamic-process 1 dynamic-app-mapped 4 dynamic-app-pages 6 dynamic-interp-mapped 4 dynamic-interp-pages 5 dynamic-reloc 1 dynamic-rela 5 dynamic-jmprel 6 dynamic-binding-total 11 dynamic-binding-supported 7 dynamic-binding-missing 0 dynamic-reloc-apply 1 dynamic-reloc-apply-total 11 dynamic-reloc-apply-write 11 dynamic-reloc-apply-readback 11 dynamic-libc-start-main 1 dynamic-stack 1 dynamic-stack-pages 16 dynamic-transfer-ready 1 dynamic-transfer-rip 0x00000000520010C0 dynamic-task-registered 1 dynamic-transfer-started 1 dynamic-first-syscall 231 dynamic-console-bytes 50 dynamic-exit-code 0x00000000 mapped 8 pages 11 stack 16 pml4 1 pml4-pool 8 root-pool-limit 8 root-distinct 1 high-copy 1 mmio-shared 1 pool-mapped 1 low-compat 0 syscall-entry-high 1 idt-high 1 descriptor-high 1 kernel-entry-high-ready 1 kernel-cr3-entry 1 syscall-root-repair 0 signal-sigpipe 1 signal-sigchld 1 signal-rt-sigreturn 1 signal-frame-fault 0 read 1 read-bytes 0 write 3 write-bytes 50 pipe 2 pipe-create 2 pipe-denials 0 pipe-faults 0 pipe-live-final 0 pipe-blocks 1 pipe-wakes 1 pipe-replays 1 pipe-provider-denials 1 fd-fork-pipe-copy 2 fd-fork-pipe-denial 0 fork 1 fork-success 1 fork-enosys 0 fork-denial 0 fork-child-slot 1 fork-child-root-distinct 1 wait4 1 wait4-reap 1 wait4-last-exit-code 7 child-root-cleanup 1 root-cleanup 2 pml4-pool-used-final 0 root-pool-used-final 0 console-bytes 50 exit 0 cleanup 1 page-faults 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

Final reserves are UEFI 794,400 bytes and BIOS 101 sectors. The M105 UEFI manifest reports kernel bytes 1,302,752, checksum `0x65F58141`, and SHA-256 `cfb011d1dcac5f7ee98fb06271140f37609420a55c26b5d28fe4ed8e12db18a0`.

M105 non-claims: arbitrary dynamic-linker search/loading, glibc compatibility, broad pipe capacity/backpressure behavior, `vfork`, dynamic `execve`, sockets, and broad Linux VFS parity remain unavailable. The accepted claim is intentionally narrow: dynamic pipe close/error semantics work for EOF after writer close and handled SIGPIPE/EPIPE after reader close without leaking pipe objects, blocked tasks, or process roots.

## M106 Universal Hardware Inventory And Driver Binding Core

M106 is accepted on the UEFI Product path with:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareRegistryGate
```

It adds a UEFI-only fixed-size `HARDWARE64_REGISTRY_MAX_DEVICES = 32` registry that snapshots platform, display, input, storage, USB controller, and network device evidence behind the existing hardware-inventory capability token. The BIOS build excludes the real registry implementation and stays at 101 reserve sectors.

Acceptance telemetry:

```text
[x64] drs-hardware-registry hardware-registry 1 refresh 1 limit 32 inventory 11 pci-enumerated 8 pci-query-denial 0 acpi-tables 2 display-device 2 input-device 4 storage-device 2 usb-controller 1 network-device 1 driver-bound 9 driver-candidate 0 driver-deferred 2 driver-unsupported 0 driver-failed 0 overflow 0 token 0x89CF635C
```

Final reserves are UEFI 793,920 bytes and BIOS 101 sectors. The M106 UEFI manifest reports kernel bytes 1,303,232 and checksum `0xF1037FB1`.

M106 non-claims: no new physical hardware driver is claimed by the registry alone; real laptop display, pointer, NVMe, USB class, Wi-Fi, audio, power, and GPU support remain follow-on hardware milestones.

## M107 Physical Display Bring-Up Reliability

M107 is accepted on the UEFI Product path with:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

It adds UEFI-only derived display layout/readability telemetry to `hwval`: framebuffer stride sanity, byte coverage, selected text scale, console viewport origin/size, columns, rows, fit status, readable status, clipping count, and a layout token. The default console viewport now comes from the GOP mode geometry instead of the previous fixed QEMU-sized rectangle. Larger physical modes can select scale 3; the verified QEMU 1280x800 mode keeps scale 2.

Acceptance telemetry:

```text
[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 24 viewport-y 96 viewport-w 1232 viewport-h 680 columns 102 rows 37 fit 1 readable 1 clip 0 token 0xF8C98059
```

Final reserves are UEFI 789,440 bytes and BIOS 101 sectors. The M107 UEFI manifest reports kernel bytes 1,307,712 and checksum `0x8A9C8B83`.

M107 non-claims: no native GPU driver, DRM/KMS mode setting, EDID policy, acceleration, multi-monitor support, or complete physical laptop display certification is claimed. The accepted claim is the narrower foundation needed for real hardware bring-up: framebuffer geometry, pitch, bounds, scale, viewport fit, and clipping are now visible and gated.

## M108 Visible Cursor Fallback And Bounded Login Recovery

M108 is accepted on the UEFI Product path with:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

It fixes the observed VirtualBox class of pointer failure where mouse packets and coordinates update but no cursor is visible. On UEFI, the compositor cursor save/restore/draw path can now operate directly on the physical framebuffer when the compositor/back-buffer path is inactive. `hwval` and the hardware-display gate now expose cursor visibility, total cursor draws, and direct framebuffer cursor draws. The same milestone removes the Product boot blocker where the login gate could wait indefinitely for keyboard input; the typed credential path remains first, but missing input now reaches the existing bounded local-console recovery session.

Acceptance telemetry:

```text
[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 24 viewport-y 96 viewport-w 1232 viewport-h 680 columns 102 rows 37 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 3 direct-cursor-draws 3 token 0xF8C98059
```

Bounded login evidence included `first-run hardware input fallback`, `first-run hardware recovery login`, `stage LOGIN OK`, `drs-login-auth-success 1`, and the persistent shell accepting the subsequent `hwval` command without a manual key press.

Final reserves are UEFI 789,312 bytes and BIOS 101 sectors. The M108 UEFI manifest reports kernel bytes 1,307,840 and checksum `0x1A4850D3`.

M108 non-claims: no full GUI redesign, native GPU driver, DRM/KMS mode setting, acceleration, I2C HID touchpad driver, or broad laptop certification is claimed. The accepted claim is narrow and falsifiable: pointer movement can now produce a visible cursor through a direct framebuffer fallback, no-key Product boot can reach the shell through bounded recovery, and future hardware runs can report whether the cursor was actually drawn.

## M109 Product Visual Polish Direct Compositor Foundation

M109 is accepted on the UEFI Product path with:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
```

It turns the M108 direct cursor fallback into a real direct compositor fallback for UEFI Product. If the full back buffer cannot be allocated in the current firmware handoff window, the compositor remains active, records `compositor-direct 1`, and routes existing desktop/window/font drawing to the physical GOP framebuffer. The existing back-buffered path remains intact when allocation succeeds. The Product palette was also cleaned up from diagnostic red/grey into a calmer teal-accented surface with distinct app colors and clearer surface/border contrast.

Acceptance telemetry:

```text
[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 205 direct-cursor-draws 207 token 0xF8C98059
[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 1 token 0xCB1B1C83
```

Visible `hwval` evidence included `display compositor direct: yes`, `display ui polish token: 0xCB1B1C83`, `display cursor visible: yes`, `display cursor draws: 158`, `display direct cursor draws: 161`, `mouse packets: 2`, `mouse x: 560`, and `mouse y: 420`.

Final reserves are UEFI 789,120 bytes and BIOS 101 sectors. The M109 UEFI manifest reports kernel bytes 1,308,032 and checksum `0x37D2EFB2`.

M109 non-claims: no native GPU driver, DRM/KMS mode setting, EDID policy, acceleration, multi-monitor support, I2C HID touchpad driver, complete GUI toolkit, or broad physical laptop certification is claimed. The accepted claim is narrower: Product desktop/window/font/taskbar rendering can initialize and draw through a direct GOP framebuffer compositor fallback, with cursor visibility and UI initialization telemetry gated by `hwval`.

## M110 NVMe/FAT Hardware Storage Triage

M110 is accepted on the UEFI Product path with:

```powershell
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareStorageGate
```

It adds hardware-facing storage diagnostics for the observed real-laptop symptom where `linux /apps/dynldlimit` reported `NVME FAT UNAVAILABLE`. The new UEFI-only `drs-nvme-triage` line distinguishes controller discovery, controller readiness, Identify, IO queue creation, read completion/status, GPT signature, FAT32 partition geometry, VBR, FAT BPB/location/error, scoped shell RW capability, `/APPS` directory visibility, first `/APPS` dirent visibility, and staged artifact presence for `/APPS/BUSYBOX`, `/APPS/DYNLDLIMIT`, and `/APPS/LDLIMIT`. The same triage line is emitted from the `linux` unavailable path so direct launch failures carry the diagnostic context.

Acceptance telemetry:

```text
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions 6 fat32-start 2048 fat32-sectors 8192 gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat 0 busybox-bytes 0 dynldlimit-stat 0 dynldlimit-bytes 0 ldlimit-stat 0 ldlimit-bytes 0 boot-staged 0 boot-app-bytes 0 boot-interp-bytes 0 boot-status 14 token 0xCDD4D6A0
```

The default storage-gate image proves the diagnostic separation: NVMe/GPT/FAT and `/APPS` are healthy, while optional real-binary artifacts are absent unless a staging verifier explicitly adds them.

Final reserves are UEFI 788,608 bytes and BIOS 101 sectors. The M110 UEFI manifest reports kernel bytes 1,308,544 and checksum `0x769D7150`.

M110 non-claims: no new NVMe driver, broad hardware fix, arbitrary filesystem mounting, automatic artifact staging, or physical laptop pass is claimed. The accepted claim is that real-hardware storage failures are now stage-specific and visible from `hwval` or from the `linux` unavailable path.

## M111 Boot/NVMe Staged Dynamic Artifact Verification

M111 is accepted with:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product -BootLinuxAppPath .\external\build\DYNLDLIMIT -BootLinuxAppName DYNLDLIMIT -BootLinuxInterpPath .\external\build\LDLIMIT -BootLinuxInterpName LDLIMIT
.\tools\verify-hardware-storage-staging.ps1 -SkipBuild
```

The gate stages the dynamic app/interpreter pair into both the UEFI boot FAT image and the NVMe FAT `/APPS` directory, then requires `hwval` to prove the boot-media byte counts match the NVMe stat results:

```text
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions 6 fat32-start 2048 fat32-sectors 8192 gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat 0 busybox-bytes 0 dynldlimit-stat 1 dynldlimit-bytes 15680 ldlimit-stat 1 ldlimit-bytes 16704 boot-staged 1 boot-app-bytes 15680 boot-interp-bytes 16704 boot-status 0 stage-expected 1 dynldlimit-expected 1 ldlimit-expected 1 dynldlimit-match 1 ldlimit-match 1 stage-match 1 token 0x75BC2409
```

Staged artifacts:

- `/APPS/DYNLDLIMIT`: SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`, 15,680 bytes
- `/APPS/LDLIMIT`: SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`, 16,704 bytes

`BOOTMAN.TXT` now records `boot-linux-expected=1`, the expected `/APPS` paths, byte counts, and SHA-256s whenever the staged boot-Linux app/interpreter paths are supplied to the build.

Final reserves are UEFI 788,512 bytes and BIOS 101 sectors. The M111 UEFI manifest reports kernel bytes 1,308,640, checksum `0x6714FC97`, and SHA-256 `824902bd0a384e02ea18193a6468f95a0842984f41c18cc06969c11a722df196`.

M111 non-claims: this does not execute `/APPS/DYNLDLIMIT`, add broad dynamic linker search/loading, or fix an unknown physical NVMe controller quirk. It makes physical media staging falsifiable before the next real-hardware storage trace.

## M112 Physical Hardware Storage Capture Parser

M112 adds `tools\parse-hardware-storage-capture.ps1` so real laptop `hwval` transcripts can be classified without guessing from photos. The parser reads `drs-nvme-triage`, writes JSON evidence, and reports the first failing dependency stage across NVMe discovery/readiness/Identify/IO queue/read status, GPT/FAT parsing, scoped capability delegation, `/APPS` visibility, and optional M111 staged dynamic artifact agreement.

Validation covered:

```powershell
.\tools\parse-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputPath .\build\hardware-storage-capture-qemu-staged-pass.json -RequireStagedDynamicArtifacts
.\tools\parse-hardware-storage-capture.ps1 -InputPath .\docs\hardware\msi-cyborg-15-a13ve.md -OutputPath .\build\hardware-storage-capture-legacy.json
```

Positive staged QEMU capture reports `storage-ready`; the legacy photo-era hardware note reports `legacy-realbin-unavailable` and instructs the tester to rerun `hwval` on an M111-staged image. Final reserves are unchanged from M111: UEFI 788,512 bytes and BIOS 101 sectors.

## M113 Physical Hardware Storage Evidence Bundle

M113 adds `tools\prepare-hardware-storage-evidence.ps1`, a host-side handoff tool for the next physical laptop run. It validates the staged dynamic artifact contract, optionally reruns the staged QEMU storage gate, and produces an ignored `dist\m113-hardware-storage-*` directory containing the staged ISO, UEFI image, `BOOTMAN.TXT`, size map, `DYNLDLIMIT`, `LDLIMIT`, JSON/text manifests, and a runbook.

Accepted command:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1 -SkipBuild
```

Accepted bundle:

```text
dist\m113-hardware-storage-20260617-221334
```

Evidence manifest:

```text
iso-sha256=2b50475171dd6d54cd3d615d23af8b5812f248997b1403bb93f504d6af5414ac
uefi-image-sha256=5da9b6326012e45af0302e408e68c4126ad7d613036d0e9c50c2ac0947c8a076
dynamic-app=/APPS/DYNLDLIMIT
dynamic-app-bytes=15680
dynamic-app-sha256=9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa
dynamic-interpreter=/APPS/LDLIMIT
dynamic-interpreter-bytes=16704
dynamic-interpreter-sha256=6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238
bios-sector-reserve=101
uefi-byte-reserve=788512
```

Bundled QEMU staged gate proof:

```text
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions 6 fat32-start 2048 fat32-sectors 8192 gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat 0 busybox-bytes 0 dynldlimit-stat 1 dynldlimit-bytes 15680 ldlimit-stat 1 ldlimit-bytes 16704 boot-staged 1 boot-app-bytes 15680 boot-interp-bytes 16704 boot-status 0 stage-expected 1 dynldlimit-expected 1 ldlimit-expected 1 dynldlimit-match 1 ldlimit-match 1 stage-match 1 token 0x75BC2409
```

M113 non-claims: this does not certify the MSI laptop or add a storage driver. It packages the exact staged media, hashes, and runbook needed for the next real hardware `hwval` capture. Proposed M114 scope is to boot this bundle on the MSI laptop, parse the captured transcript with `-RequireStagedDynamicArtifacts`, and implement only the first failing stage reported by the parser.

## M114 Physical Hardware Storage Capture Analysis

M114 adds `tools\analyze-hardware-storage-capture.ps1`, a host-side report generator for raw physical `hwval` transcripts. It invokes the M112 parser, treats failing parse classifications as expected diagnostic output, and writes JSON/text/Markdown analysis with the first failing stage, important storage fields, and the next implementation target. It can also read expected artifact byte counts from the M113 evidence manifest.

Accepted commands:

```powershell
.\tools\analyze-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m114-analysis-qemu -RequireStagedDynamicArtifacts
.\tools\analyze-hardware-storage-capture.ps1 -InputPath .\docs\hardware\msi-cyborg-15-a13ve.md -OutputDir .\build\m114-analysis-legacy -RequireStagedDynamicArtifacts
.\tools\analyze-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m114-analysis-manifest -EvidenceManifestPath .\dist\m113-hardware-storage-20260617-221334\hardware-storage-evidence-manifest.json -RequireStagedDynamicArtifacts
.\tools\parse-hardware-storage-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputPath .\build\m114-parser-regression.json -RequireStagedDynamicArtifacts
```

Positive staged capture result:

```text
hardware-storage-analysis: storage-ready
pass: True
detail: NVMe, GPT, FAT, /APPS, capability delegation, and requested staged artifacts are all visible.
next-target: Storage is healthy. Next target: run linux /APPS/DYNLDLIMIT on hardware and capture drs-realbin telemetry.
```

Legacy hardware-note result:

```text
hardware-storage-analysis: nvme-controller-discovery
pass: False
detail: NVMe controller was not discovered.
next-target: Driver target: PCI/NVMe enumeration, class-code match, BAR mapping, and controller register visibility.
```

M114 also fixes `tools\parse-hardware-storage-capture.ps1` to serialize `raw_line` and `legacy_line` as plain strings, so downstream JSON reports no longer inherit PowerShell provider metadata. M114 non-claims: no hardware driver or physical certification is claimed. It makes the next real `hwval` transcript actionable without manual interpretation.

## M115 Physical Hardware Storage Evidence Verification

M115 adds `tools\verify-hardware-storage-evidence.ps1`, a one-command verifier for the M113 evidence bundle and optional M114 capture analysis. It checks the bundle manifest, ISO/UEFI image hashes, staged `DYNLDLIMIT`/`LDLIMIT` hashes, `BOOTMAN.TXT` staging lines, size-map reserves, and optional captured `hwval` analysis.

Accepted commands:

```powershell
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m113-hardware-storage-20260617-221334 -OutputDir .\build\m115-verify-bundle
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m113-hardware-storage-20260617-221334 -CapturePath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m115-verify-qemu -RequireStagedDynamicArtifacts
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m113-hardware-storage-20260617-221334 -CapturePath .\docs\hardware\msi-cyborg-15-a13ve.md -OutputDir .\build\m115-verify-legacy -RequireStagedDynamicArtifacts
```

Positive staged QEMU proof:

```text
hardware-storage-evidence: verified
bundle-pass: True
iso-sha256: 2b50475171dd6d54cd3d615d23af8b5812f248997b1403bb93f504d6af5414ac
uefi-image-sha256: 5da9b6326012e45af0302e408e68c4126ad7d613036d0e9c50c2ac0947c8a076
dynamic-app-sha256: 9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa
dynamic-interpreter-sha256: 6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238
bios-sector-reserve: 101
uefi-byte-reserve: 788512
capture-pass: True
capture-stage: storage-ready
```

Legacy hardware-note proof:

```text
hardware-storage-evidence: verified
bundle-pass: True
capture-pass: False
capture-stage: nvme-controller-discovery
```

M115 non-claims: no new kernel behavior, hardware certification, or storage driver is claimed. It makes the next real hardware capture self-checking before any driver work begins.

## M116 Physical Hardware Storage Analysis Fixture Coverage

M116 adds `tools\verify-hardware-storage-analysis-fixtures.ps1`, a host-side regression suite for the M112/M114 classifier. It fabricates one transcript per dependency-stage outcome and confirms the analyzer reports the expected stage and exit code.

Accepted command:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1
```

Accepted proof:

```text
hardware-storage-analysis-fixtures: 35/35
failed: 0
```

The fixture set covers missing telemetry, legacy `drs-realbin-unavailable`, every NVMe controller/readiness/Identify/queue/read failure, GPT/FAT discovery and mount failures, capability failures, `/APPS` lookup failures, staged boot/NVMe artifact mismatches, and the final `storage-ready` pass. M116 non-claims: it does not add or certify hardware support; it proves the host-side diagnosis layer is deterministic before the next real laptop capture.

## M117 Physical Display/Input Capture Analysis

M117 adds `tools\analyze-hardware-display-input-capture.ps1` and `tools\verify-hardware-display-input-fixtures.ps1`. These host-side tools classify `hwval` display/UI/cursor/pointer telemetry into concrete hardware stages.

Accepted commands:

```powershell
.\tools\analyze-hardware-display-input-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m117-display-input-qemu
.\tools\verify-hardware-display-input-fixtures.ps1
```

Positive QEMU proof:

```text
hardware-display-input-analysis: display-input-ready
pass: True
detail: Display is readable, UI initialized, cursor is visible, and pointer packets were received.
mouse-packets: 2
xhci-mouse-endpoint: 1
xhci-mouse-reports: 2
ps2-present: 1
ps2-enabled: 1
```

Fixture proof:

```text
hardware-display-input-fixtures: 26/26
failed: 0
```

The fixture set covers missing display/UI telemetry, framebuffer stride/bounds/fit/readability failures, compositor/font/window-manager/desktop/taskbar/launcher/window failures, cursor-hidden and pointer-moving-cursor-hidden cases, I2C HID touchpad errors/candidate binding/report bridging, xHCI mouse report failures, PS/2 fallback failures, no pointer backend, and final display/input readiness. M117 non-claims: it does not add a GPU driver, DRM/KMS, a new touchpad driver, or physical MSI certification; it makes the next hardware capture actionable.

## M118 MSI Hardware Capture Analysis

M118 adds `tools\analyze-msi-hardware-capture.ps1`, a single host-side MSI intake wrapper that runs the M115 storage evidence verifier, the M114 storage capture analyzer, and the M117 display/input analyzer against one captured transcript.

Accepted command:

```powershell
.\tools\analyze-msi-hardware-capture.ps1 `
  -EvidenceDir .\dist\m113-hardware-storage-20260617-222838 `
  -CapturePath .\build\qemu-x86_64-uefi-debug.log `
  -OutputDir .\build\m118-msi-qemu `
  -RequireStagedDynamicArtifacts
```

Positive proof:

```text
msi-hardware-analysis: msi-hardware-ready
pass: True
storage-stage: storage-ready
display/input-stage: display-input-ready
bios reserve: 101 sectors
uefi reserve: 788512 bytes
```

The wrapper also preserves incomplete hardware evidence as a concrete implementation target. The legacy/photo-era MSI doc capture verifies the storage evidence bundle but fails the transcript analysis with:

```text
msi-hardware-analysis: storage-nvme-controller-discovery
pass: False
storage-stage: nvme-controller-discovery
display/input-stage: missing-display-readability
next target: Driver target: PCI/NVMe enumeration, class-code match, BAR mapping, and controller register visibility.
```

M118 non-claims: this does not certify physical laptop readiness, add a GPU/touchpad/storage driver fix, or claim that `/APPS/DYNLDLIMIT` runs on hardware. It makes the next MSI `hwval` capture produce one ordered first-failure target across storage and display/input.

## M119 MSI Hardware Capture Analysis Fixture Coverage

M119 adds `tools\verify-msi-hardware-analysis-fixtures.ps1`, a self-contained fixture suite for the combined M118 MSI analyzer. It synthesizes a matching storage evidence bundle and controlled `hwval` transcripts, then proves the combined analyzer preserves the intended ordering.

Accepted command:

```powershell
.\tools\verify-msi-hardware-analysis-fixtures.ps1
```

Acceptance output:

```text
msi-hardware-analysis-fixtures: 4/4
failed: 0
```

Covered top-level stages:

```text
msi-hardware-ready
storage-nvme-controller-discovery
display-input-pointer-moving-cursor-hidden
storage-missing-storage-triage
```

This proves that storage failures remain the first implementation target, display/input failures are promoted only after storage passes, and insufficient captures point back to a fresh `hwval` run instead of producing a misleading hardware-driver target. M119 is host-side regression coverage only; it does not certify the physical MSI laptop or add new kernel/device support.

## M120 Boot-Media Linux Handoff Verification

M120 records the verified UEFI boot-media fallback for the hardware-facing `linux /APPS/DYNLDLIMIT` path. The accepted verifier stages tiny invalid ELF-shaped probe payloads into the UEFI boot FAT image, boots QEMU, and proves that the shell chooses the staged boot-media source before requiring NVMe FAT.

Accepted command:

```powershell
.\tools\verify-boot-media-linux-handoff.ps1
```

Acceptance output:

```text
Boot-media Linux handoff verifier passed.
BIOS reserve sectors: 101
UEFI reserve bytes: 788512
Command: linux /APPS/DYNLDLIMIT
```

Key telemetry:

```text
[uefi] boot linux stage DYNLDLIMIT attempted 1 loaded 1 bytes 7 pages 1 base 0x0000000000100000 copied 1 token 0x709DAA1E status 0x0000000000000000
[uefi] boot linux stage LDLIMIT attempted 1 loaded 1 bytes 7 pages 1 base 0x0000000000101000 copied 1 token 0x7E82AA7C status 0x0000000000000000
linux: using UEFI boot-media staged file
drs-realbin-fail path /APPS/DYNLDLIMIT source 2 stage elf code 2 ... boot-media-read-error 0 boot-media-read-bytes 7 boot-media-read-capacity 4194304
```

M120 non-claims: the probe payload is intentionally invalid, so this does not claim dynamic execution or physical MSI certification. It proves the source-selection and read path needed to avoid the older NVMe-only unavailable failure when `/APPS/DYNLDLIMIT` is staged by the UEFI loader.

## M121 MSI Hardware Handoff Bundle Refresh

M121 records the refreshed physical MSI handoff bundle for real hardware validation. `tools\prepare-hardware-storage-evidence.ps1` now emits an M121 handoff bundle and runbook that asks the tester to capture both `hwval` and `linux /APPS/DYNLDLIMIT`, then run the combined MSI analyzer.

Accepted commands:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1
.\tools\verify-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m121-msi-hardware-handoff-20260617-225629 -OutputDir .\build\m121-verify-handoff
```

Generated bundle:

```text
dist\m121-msi-hardware-handoff-20260617-225629
```

Bundle artifacts:

- `limitlessos-x86_64-m121-handoff.iso`: SHA-256 `937d5694bc5d9b1e4656ff4d445f2cec8a6d3611a2c0c7a1ba3bf96e82ff7daf`
- `limitlessos-x86_64-m121-handoff-uefi.img`: SHA-256 `c1e26eaae6dd8ea572b271f407b72a1a827c294b2d3efda5ea59bfe80ec258d3`
- `/APPS/DYNLDLIMIT`: 15,680 bytes, SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`
- `/APPS/LDLIMIT`: 16,704 bytes, SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`

Verifier output:

```text
hardware-storage-evidence: verified
  bundle pass: True
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
```

The runbook's hardware command pair is:

```text
hwval
linux /APPS/DYNLDLIMIT
```

The expected boot-media handoff signal remains:

```text
linux: using UEFI boot-media staged file
drs-realbin ... source 2 ... boot-media-read 1
```

M121 non-claims: this does not certify the physical MSI laptop or add new hardware support. It gives the next physical run a current, hash-verified bundle and a single combined storage/display/input/dynamic-handoff analysis path.

## M122 MSI Hardware Handoff Verifier

M122 records a host-side verifier for the physical MSI handoff contract. `tools\verify-msi-hardware-handoff.ps1` wraps the storage evidence verifier, then checks the M121-only pieces that keep the next physical run honest: combined MSI analyzer selection, `hwval` plus `linux /APPS/DYNLDLIMIT` runbook instructions, and source-2 boot-media handoff expectations.

Accepted command:

```powershell
.\tools\verify-msi-hardware-handoff.ps1 -EvidenceDir .\dist\m121-msi-hardware-handoff-20260617-225629 -OutputDir .\build\m122-msi-handoff-verifier -RequireStagedDynamicArtifacts
```

Verifier output:

```text
hardware-storage-evidence: verified
  bundle pass: True
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
msi-hardware-handoff: verified
  handoff pass: True
  source2 required: 2
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
```

M122 non-claims: this is not physical hardware certification and does not add a driver. It is a regression guard for the package that will be used for the next real hardware run.

## M123 MSI Hardware Handoff Verifier Fixture Coverage

M123 records fixture coverage for the M122 handoff verifier. `tools\verify-msi-hardware-handoff-fixtures.ps1` synthesizes storage-valid evidence bundles and proves the handoff verifier accepts the valid M121 shape while rejecting stale or incomplete handoff contracts.

Accepted command:

```powershell
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m123-msi-handoff-fixtures
```

Verifier output:

```text
msi-hardware-handoff-fixtures: 7/7
  failed: 0
```

The negative fixtures cover stale milestone labels, storage-only analyzer selection, missing source-2 requirement, old M113 ISO naming, missing `linux /APPS/DYNLDLIMIT`, and missing source-2 runbook telemetry. M123 is host-side regression coverage only; it does not certify the physical MSI laptop.

## M124 Self-Verifying MSI Handoff Packaging

M124 records that the MSI handoff packager now verifies its own output. `tools\prepare-hardware-storage-evidence.ps1` calls `tools\verify-msi-hardware-handoff.ps1` after generating the bundle and writes the verifier artifacts into the evidence directory, unless `-SkipHandoffVerify` is explicitly used.

Accepted command:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1 -EvidenceDir .\build\m124-self-verified-handoff -SkipBuild -SkipQemuGate
```

Verifier output:

```text
msi-hardware-handoff: verified
  handoff pass: True
  source2 required: 2
  bios reserve: 101 sectors
  uefi reserve: 788512 bytes
```

The generated runbook now routes captured physical transcripts through:

```powershell
.\tools\verify-msi-hardware-handoff.ps1 -EvidenceDir <path-to-this-bundle> -CapturePath <path-to-msi-hwval-storage.txt> -RequireStagedDynamicArtifacts
```

M124 non-claims: this is not physical hardware certification and does not add a driver. It makes stale or malformed handoff packages fail at generation time.

## M125 MSI Dynamic Handoff Capture Classification

M125 records dynamic command classification in the MSI handoff verifier. With `-CapturePath`, `tools\verify-msi-hardware-handoff.ps1` now parses the captured `linux /APPS/DYNLDLIMIT` output and emits `dynamic_handoff_*` fields so source-2 handoff success, dynamic runtime failure, stale NVMe-unavailable behavior, wrong-source telemetry, missing realbin telemetry, and boot-media read failure are distinct.

Accepted command:

```powershell
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m125-msi-handoff-fixtures-final
```

Verifier output:

```text
msi-hardware-handoff-fixtures: 11/11
  failed: 0
```

The capture-side fixtures prove `dynamic-runtime-static`, `dynamic-runtime-exit0`, `dynamic-handoff-nvme-unavailable`, and `dynamic-handoff-wrong-source`. M125 is host-side capture classification only; it does not certify the physical MSI laptop.

## M134 Storage Hardware Target Classification

M134 adds `tools\classify-m134-storage-target.ps1`, a host-side classifier for the M134-M140 storage hardware breadth phase. It requires an M133 evidence bundle plus a real capture path, runs `tools\verify-msi-hardware-handoff.ps1`, and then emits a single machine-readable target: `target_kind`, `target_stage`, `roadmap_target`, and `next_target`.

Accepted command:

```powershell
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m134-storage-target-fixtures
```

Verifier output:

```text
m134-storage-target-fixtures: 4/4
failed: 0
```

The fixture suite proves the classifier keeps the storage roadmap honest:

- `storage-nvme-controller-discovery`: reports `target_kind storage`, `target_stage nvme-controller-discovery`, and `roadmap_target M134`.
- `display-after-storage-ready`: reports `target_kind display-input`, `target_stage pointer-moving-cursor-hidden`, and `roadmap_target M149` after storage has already reached `storage-ready`.
- `dynamic-after-storage-ready`: reports `target_kind dynamic-handoff`, `target_stage dynamic-handoff-nvme-unavailable`, and `roadmap_target M83+` after storage and display/input are ready.
- `storage-ready`: reports `target_kind storage-ready`, `target_stage storage-ready`, and `roadmap_target M134` when storage, display/input, and source-2 dynamic handoff are all green.

M134 non-claims: no kernel code changed, no NVMe controller quirk was fixed, and no physical MSI storage certification is claimed. This milestone prevents speculative storage-driver work until a real transcript names a storage stage as the first blocker.

## M135 M134 Classifier Handoff Integration

M135 threads the M134 classifier into the generated physical MSI handoff package. `tools\prepare-hardware-storage-evidence.ps1` now records `tools\classify-m134-storage-target.ps1 -RequireStagedDynamicArtifacts` in `expected_hwval.storage_target_classifier` and writes the classifier command into `README-HARDWARE-STORAGE.txt` ahead of the older combined analyzer. `docs\hardware\msi-cyborg-15-a13ve.md` now tells the tester to use `target-kind`, `target-stage`, and `next-target` as the first implementation target.

Accepted commands:

```powershell
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m135-msi-handoff-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m135-storage-target-fixtures
```

Verifier output:

```text
msi-hardware-handoff-fixtures: 12/12
failed: 0

m134-storage-target-fixtures: 4/4
failed: 0
```

The handoff verifier now rejects a storage-valid bundle whose manifest omits the M134 classifier, using the new `missing-storage-target-classifier` fixture. The M134 classifier fixtures still prove true storage, display/input-after-storage, dynamic-handoff-after-storage, and fully green storage-ready captures under the stricter handoff contract.

M135 non-claims: no kernel code changed, no physical hardware transcript was added, and no new storage/display/input driver support is claimed. This milestone makes the physical handoff package harder to misuse.

## M136 Storage Diagnostic Playbooks

M136 strengthens the physical storage analysis report from a stage name into an implementation-ready diagnostic plan. `tools\analyze-hardware-storage-capture.ps1` now emits a checked `diagnostic` object for each storage stage with the failing component, required telemetry fields, first check, relevant kernel files, and acceptance signal. The generated Markdown report includes the same information under `Diagnostic Plan`.

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m136-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m136-storage-target-fixtures
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 35/35
failed: 0

m134-storage-target-fixtures: 4/4
failed: 0
```

The strengthened fixture suite requires every synthetic storage classification to carry a populated diagnostic plan, so missing telemetry, legacy captures, NVMe discovery/readiness/Identify/queue/read failures, GPT/FAT failures, capability failures, `/APPS` failures, staged artifact mismatches, and `storage-ready` all remain deterministic.

M136 non-claims: no kernel code changed, no physical MSI transcript was newly supplied, and no storage hardware support is certified. It makes the next real transcript more directly actionable while preserving the rule that hardware claims require hardware evidence. Accepted evidence preserves BIOS reserve `101` sectors and UEFI reserve `749,408` bytes.

## M137 NVMe PCI Identity Telemetry

M137 adds the missing PCI identity layer to the hardware storage evidence path. The UEFI Product `hwval`, `drs-nvme-pci`, and `drs-nvme-triage` outputs now include PCI storage count, NVMe count, first NVMe BDF, vendor/device, class/prog-if/revision, BAR0/BAR1, MMIO base low/high, span, flags, and token. The brokered PCI scaffold line also proves those values through the driver-host hardware capability, and the QEMU verifier rejects sentinel NVMe identity values on the UEFI Product path.

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m137-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m137-storage-target-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 35/35
failed: 0

m134-storage-target-fixtures: 4/4
failed: 0
```

Representative QEMU telemetry:

```text
[x64] drs-nvme-pci nvme-pci-diag 1 pci-storage 2 pci-nvme 1 nvme-pci 0x00000200 nvme-vendor-device 0x00101B36 nvme-class 0x01080202 nvme-bar0 0x00008004 nvme-bar1 0x000000C0 nvme-mmio-low 0x00008000 nvme-mmio-high 0x000000C0 nvme-mmio-span 16384 nvme-mmio-flags 0x000001BF nvme-mmio-token 0x51B13EF6
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage 2 pci-nvme 1 nvme-pci 0x00000200 nvme-vendor-device 0x00101B36 nvme-class 0x01080202 nvme-bar0 0x00008004 nvme-bar1 0x000000C0 nvme-mmio-low 0x00008000 nvme-mmio-high 0x000000C0 nvme-mmio-span 16384 nvme-mmio-flags 0x000001BF nvme-mmio-token 0x51B13EF6 nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 ...
```

The storage capture analyzer now carries the new PCI fields in `key_fields`, and its `nvme-controller-discovery` diagnostic plan requires them so the next real MSI transcript can identify whether the first failure is PCI enumeration, NVMe controller readiness, GPT/FAT, scoped filesystem authority, `/APPS` visibility, or staged dynamic artifacts.

M137 non-claims: no physical MSI transcript was newly supplied, and no new real hardware storage-driver pass is certified. This milestone adds precise UEFI Product telemetry for the next hardware run. Accepted evidence preserves BIOS reserve `101` sectors and UEFI reserve `744,672` bytes.

## M138 NVMe PCI Capture-Stage Classification

M138 turns the raw M137 PCI identity fields into actionable first-failure classifications. `tools\parse-hardware-storage-capture.ps1` now classifies these early stages before generic NVMe controller readiness:

- `pci-storage-discovery`
- `pci-nvme-class`
- `pci-nvme-bdf`
- `pci-nvme-identity`
- `pci-nvme-class-code`
- `pci-nvme-bar0`
- `pci-nvme-mmio-base`
- `pci-nvme-mmio-span`
- `pci-nvme-mmio-flags`

`tools\analyze-hardware-storage-capture.ps1` has matching diagnostic playbooks with required telemetry fields, first checks, kernel-file pointers, and acceptance signals. `tools\verify-m134-storage-target-fixtures.ps1` now includes a full handoff/classifier fixture proving `pci-nvme-class` routes as `target_kind storage` and `target_stage pci-nvme-class`.

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m138-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m138-storage-target-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 44/44
failed: 0

m134-storage-target-fixtures: 5/5
failed: 0
```

M138 non-claims: no physical MSI transcript was newly supplied, and no new real hardware storage-driver pass is certified. This milestone narrows the next hardware failure report so implementation starts at the exact PCI/NVMe boundary rather than a broad discovery bucket. Accepted evidence preserves BIOS reserve `101` sectors and UEFI reserve `744,672` bytes.

## M139 VMD/RST Storage Visibility Classification

M139 extends the M137/M138 hardware storage intake so the next physical MSI transcript can distinguish plain direct-NVMe absence from likely bridge/firmware hiding. The UEFI Product PCI inventory now records:

- direct NVMe controller count and first NVMe identity, unchanged
- RAID-class storage controller count as `pci-raid`
- non-AHCI/non-NVMe storage controller count and first identity as `pci-other-storage`, `other-storage-pci`, `other-storage-vendor-device`, `other-storage-class`, `other-storage-bar0`, and `other-storage-bar1`
- Intel system-class controller candidate count and first identity as `pci-intel-system`, `intel-system-pci`, `intel-system-vendor-device`, `intel-system-class`, `intel-system-bar0`, and `intel-system-bar1`

These fields are emitted by `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold line under the same hardware-inventory capability authority as the existing PCI fields. The implementation does not touch VMD/RST controller registers and does not claim a VMD, RAID, or nested PCI-domain driver.

The hardware storage parser now classifies three new first-failure stages before falling back to the generic `pci-nvme-class` bucket:

- `pci-nvme-hidden-by-raid`
- `pci-nvme-hidden-by-intel-system`
- `pci-nvme-other-storage`

`tools\analyze-hardware-storage-capture.ps1` has diagnostic playbooks for each stage, including required telemetry fields, first checks, kernel-file pointers, and acceptance signals. `tools\verify-m134-storage-target-fixtures.ps1` now proves a full handoff/classifier run routes `pci-nvme-hidden-by-intel-system` as a storage target.

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m139-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m139-storage-target-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 47/47
failed: 0

m134-storage-target-fixtures: 6/6
failed: 0
```

Representative QEMU telemetry:

```text
[x64] drs-nvme-pci nvme-pci-diag 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 nvme-pci 0x00000200 nvme-vendor-device 0x00101B36 nvme-class 0x01080202 nvme-bar0 0x00008004 nvme-bar1 0x000000C0 nvme-mmio-low 0x00008000 nvme-mmio-high 0x000000C0 nvme-mmio-span 16384 nvme-mmio-flags 0x000001BF nvme-mmio-token 0x51B13EF6 other-storage-pci 0xFFFFFFFF other-storage-vendor-device 0x00000000 other-storage-class 0x00000000 other-storage-bar0 0x00000000 other-storage-bar1 0x00000000 intel-system-pci 0xFFFFFFFF intel-system-vendor-device 0x00000000 intel-system-class 0x00000000 intel-system-bar0 0x00000000 intel-system-bar1 0x00000000
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 ... nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 ... stage-match 1 token 0xE385EA53
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `743,776` bytes. M139 non-claims: no physical MSI transcript was newly supplied, no internal SSD storage-driver pass is certified, no VMD/RST bridge is programmed, and no unsafe storage writes are introduced.

## M140 VMD-Class Storage Target Refinement

M140 narrows the broad M139 Intel system-class evidence into a VMD-class candidate lane. The UEFI Product PCI inventory now records Intel system/other class candidates separately as:

- `pci-vmd`
- `vmd-pci`
- `vmd-vendor-device`
- `vmd-class`
- `vmd-bar0`
- `vmd-bar1`

These fields are emitted by `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof through the existing hardware-inventory capability authority. The kernel still does not program the VMD candidate, does not enumerate a nested PCI domain, and does not bind the NVMe driver below it.

The hardware storage parser now prioritizes `pci-nvme-hidden-by-vmd` before the broader `pci-nvme-hidden-by-intel-system` stage. The analyzer playbook for that stage points the next driver work at read-only VMD candidate BAR/class validation and nested PCI-domain enumeration before attempting regular NVMe probing.

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m140-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m140-storage-target-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 48/48
failed: 0

m134-storage-target-fixtures: 7/7
failed: 0
```

Representative QEMU telemetry:

```text
[x64] drs-nvme-pci nvme-pci-diag 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 nvme-pci 0x00000200 nvme-vendor-device 0x00101B36 nvme-class 0x01080202 ... vmd-pci 0xFFFFFFFF vmd-vendor-device 0x00000000 vmd-class 0x00000000 vmd-bar0 0x00000000 vmd-bar1 0x00000000
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 ... nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 ... stage-match 1 token 0x870F0BB5
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `743,520` bytes. M140 non-claims: no physical MSI transcript was newly supplied, no VMD storage-driver pass is certified, no nested PCI-domain enumeration is implemented, no RAID driver is implemented, and no unsafe storage writes are introduced.

## M141 VMD MMIO Preflight Telemetry

M141 extends the M140 VMD candidate lane with bounded MMIO preflight telemetry. The kernel still does not map, program, or bind the VMD candidate; it only decodes the first candidate BAR0/BAR1 into a no-touch plan that future nested-enumeration work can consume.

New fields are:

- `vmd-mmio-low`
- `vmd-mmio-high`
- `vmd-mmio-span`
- `vmd-mmio-flags`
- `vmd-mmio-token`

The flag word records whether the candidate is present, whether BAR0 is a memory BAR, whether it is 64-bit, whether the base is nonzero and page-aligned, whether a future mapping would be required, whether the candidate is below 4 GiB, and the explicit pre-driver safety state: safe-no-touch, nested-enumeration-required, and no-driver-bound. These fields are emitted by `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof through the existing hardware-inventory capability authority.

The hardware storage parser now reports precise VMD preflight stages before the broader hidden-by-VMD result:

- `pci-vmd-bdf`
- `pci-vmd-identity`
- `pci-vmd-class-code`
- `pci-vmd-bar0`
- `pci-vmd-mmio-base`
- `pci-vmd-mmio-span`
- `pci-vmd-mmio-flags`
- `pci-nvme-hidden-by-vmd`

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m141-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m141-storage-target-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 55/55
failed: 0

m134-storage-target-fixtures: 8/8
failed: 0
```

Representative QEMU telemetry:

```text
pci vmd first bdf: 0xFFFFFFFF
pci vmd vendor/device: 0x00000000
pci vmd class: 0x00000000
pci vmd bar0: 0x00000000
pci vmd bar1: 0x00000000
pci vmd mmio base low: 0x00000000
pci vmd mmio base high: 0x00000000
pci vmd mmio span: 0
pci vmd mmio flags: 0x00000380
pci vmd mmio token: 0x1036CD8E
[x64] drs-nvme-pci nvme-pci-diag 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 nvme-pci 0x00000200 nvme-vendor-device 0x00101B36 nvme-class 0x01080202 nvme-bar0 0x00008004 nvme-bar1 0x000000C0 nvme-mmio-low 0x00008000 nvme-mmio-high 0x000000C0 nvme-mmio-span 16384 nvme-mmio-flags 0x000001BF nvme-mmio-token 0x51B13EF6 other-storage-pci 0xFFFFFFFF other-storage-vendor-device 0x00000000 other-storage-class 0x00000000 other-storage-bar0 0x00000000 other-storage-bar1 0x00000000 intel-system-pci 0xFFFFFFFF intel-system-vendor-device 0x00000000 intel-system-class 0x00000000 intel-system-bar0 0x00000000 intel-system-bar1 0x00000000 vmd-pci 0xFFFFFFFF vmd-vendor-device 0x00000000 vmd-class 0x00000000 vmd-bar0 0x00000000 vmd-bar1 0x00000000 vmd-mmio-low 0x00000000 vmd-mmio-high 0x00000000 vmd-mmio-span 0 vmd-mmio-flags 0x00000380 vmd-mmio-token 0x1036CD8E
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 ... vmd-pci 0xFFFFFFFF vmd-vendor-device 0x00000000 vmd-class 0x00000000 vmd-bar0 0x00000000 vmd-bar1 0x00000000 vmd-mmio-low 0x00000000 vmd-mmio-high 0x00000000 vmd-mmio-span 0 vmd-mmio-flags 0x00000380 vmd-mmio-token 0x1036CD8E ... stage-match 1 token 0x6B0E9592
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `739,136` bytes. M141 non-claims: no physical MSI transcript was newly supplied, no VMD storage-driver pass is certified, no VMD MMIO mapping/programming is implemented, no nested PCI-domain enumeration is implemented, no RAID driver is implemented, and no unsafe storage writes are introduced.

## M142 VMD Nested Enumeration Boundary Telemetry

M142 adds the next diagnostic boundary after M141's VMD MMIO preflight. It does not enumerate the VMD child PCI domain yet; it exposes whether the kernel has enough safe, no-touch information to plan that future enumerator and makes the storage classifier report that exact missing step.

New fields are:

- `vmd-nested-plan`
- `vmd-nested-enum`
- `vmd-nested-nvme`
- `vmd-nested-status`
- `vmd-nested-token`

The kernel derives `vmd-nested-plan` from the existing VMD MMIO preflight state. If no VMD candidate is present or the candidate lacks usable MMIO telemetry, the plan remains `0` and status remains `0`. If a VMD candidate has usable no-touch MMIO preflight telemetry, the plan becomes `1`, enumeration remains `0`, child NVMe count remains `0`, and status reports the explicit nested-enumeration-unavailable boundary. The fields are emitted by `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof through the existing hardware-inventory capability authority.

The hardware storage parser now routes a usable VMD MMIO candidate to the narrower nested-domain stages:

- `pci-vmd-nested-plan`
- `pci-vmd-nested-enumeration`
- `pci-vmd-nested-nvme-class`
- `pci-vmd-nested-nvme-bind`

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m142-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m142-storage-target-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 58/58
failed: 0

m134-storage-target-fixtures: 8/8
failed: 0
```

Representative QEMU telemetry:

```text
pci vmd nested plan: 0
pci vmd nested enum: 0
pci vmd nested nvme: 0
pci vmd nested status: 0
pci vmd nested token: 0xDCF21C01
[x64] drs-nvme-pci nvme-pci-diag 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 ... vmd-mmio-low 0x00000000 vmd-mmio-high 0x00000000 vmd-mmio-span 0 vmd-mmio-flags 0x00000380 vmd-mmio-token 0x1036CD8E vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-token 0xDCF21C01
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 ... vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-token 0xDCF21C01 ... nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 ... stage-match 1 token 0x99F3A89D
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `738,880` bytes. M142 non-claims: no physical MSI transcript was newly supplied, no VMD storage-driver pass is certified, no VMD MMIO mapping/programming is implemented, no nested PCI-domain enumeration is implemented, no child NVMe discovery behind VMD is implemented, no VMD storage binding is implemented, no RAID driver is implemented, and no unsafe storage writes are introduced.

## M143 VMD Read-Only Nested Config Scan

M143 implements the first bounded VMD nested-domain scan. When the VMD MMIO preflight reports a usable candidate, the kernel maps a 64 KiB VMD config-window slice and performs read-only PCI config reads across the first nested bus, devices 0-1, functions 0-7. The scan records whether enumeration ran, whether a child NVMe-class device was observed, and the first child identity fields. It performs no VMD programming, no controller reset, no NVMe binding, no storage read/write through VMD, and no unsafe hardware writes.

New fields added to `hwval`, `drs-nvme-pci`, `drs-nvme-triage`, and the brokered PCI scaffold proof:

- `vmd-nested-pci`
- `vmd-nested-vendor-device`
- `vmd-nested-class`
- `vmd-nested-bar0`
- `vmd-nested-bar1`

The storage analyzer now requires child identity fields for the nested VMD stages so hardware captures can distinguish "scan ran but no child NVMe" from "child NVMe exists but binding is still missing".

Accepted commands:

```powershell
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m143-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m143-storage-target-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-storage-analysis-fixtures: 58/58
failed: 0

m134-storage-target-fixtures: 8/8
failed: 0
```

Representative QEMU telemetry:

```text
pci vmd nested plan: 0
pci vmd nested enum: 0
pci vmd nested nvme: 0
pci vmd nested status: 0
pci vmd nested token: 0xBC821C7A
pci vmd nested first bdf: 0xFFFFFFFF
pci vmd nested vendor/device: 0x00000000
pci vmd nested class: 0x00000000
pci vmd nested bar0: 0x00000000
pci vmd nested bar1: 0x00000000
[x64] drs-nvme-pci nvme-pci-diag 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 ... vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-token 0xBC821C7A vmd-nested-pci 0xFFFFFFFF vmd-nested-vendor-device 0x00000000 vmd-nested-class 0x00000000 vmd-nested-bar0 0x00000000 vmd-nested-bar1 0x00000000
[x64] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage 2 pci-nvme 1 pci-raid 0 pci-other-storage 0 pci-intel-system 0 pci-vmd 0 ... vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-token 0xBC821C7A vmd-nested-pci 0xFFFFFFFF vmd-nested-vendor-device 0x00000000 vmd-nested-class 0x00000000 vmd-nested-bar0 0x00000000 vmd-nested-bar1 0x00000000 ... stage-match 1 token 0x1DF0A4DF
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `738,528` bytes. M143 non-claims: no physical MSI transcript was newly supplied, no full VMD bus enumeration is claimed, no VMD MMIO programming is implemented, no child NVMe binding is implemented, no RAID driver is implemented, and no unsafe storage writes are introduced.

## M144 VMD Full First-Bus Read-Only Scan

M144 expands the M143 VMD nested config scan from the first two devices to the first full nested PCI config bus. The scanner still uses a single 64 KiB high-half virtual MMIO window, but remaps it across 16 physical chunks of the VMD config aperture to cover one bus, 32 devices, and 8 functions per device. The path remains read-only: it does not program VMD registers, does not reset or enable controllers, does not bind NVMe, and does not perform storage reads/writes through VMD.

New scan-coverage telemetry:

- `vmd-nested-scan-buses`
- `vmd-nested-scan-devices`
- `vmd-nested-scan-functions`
- `vmd-nested-scan-windows`
- `vmd-nested-scan-truncated`

The VMD nested synthetic fixtures now prove two covered paths:

- no child NVMe after a full first-bus scan: `vmd-nested-scan-buses 1`, `vmd-nested-scan-devices 32`, `vmd-nested-scan-functions 256`, `vmd-nested-scan-windows 16`, `vmd-nested-scan-truncated 1`, `vmd-nested-nvme 0`
- child NVMe observed but not bound: the same coverage fields plus `vmd-nested-nvme 1`, with the classifier stopping at `pci-vmd-nested-nvme-bind`

Accepted commands:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m144-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m144-storage-target-fixtures
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
M1 production-slice gate passed for x86_64 (Product profile).

hardware-storage-analysis-fixtures: 58/58
failed: 0

m134-storage-target-fixtures: 8/8
failed: 0
```

Representative QEMU telemetry:

```text
pci vmd nested plan: 0
pci vmd nested enum: 0
pci vmd nested nvme: 0
pci vmd nested status: 0
pci vmd nested first bdf: 0xFFFFFFFF
pci vmd nested scan buses: 0
pci vmd nested scan devices: 0
pci vmd nested scan functions: 0
pci vmd nested scan windows: 0
pci vmd nested scan truncated: 0
[x64] drs-nvme-pci nvme-pci-diag 1 ... vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-token 0xFAE086EE vmd-nested-pci 0xFFFFFFFF vmd-nested-vendor-device 0x00000000 vmd-nested-class 0x00000000 vmd-nested-bar0 0x00000000 vmd-nested-bar1 0x00000000 vmd-nested-scan-buses 0 vmd-nested-scan-devices 0 vmd-nested-scan-functions 0 vmd-nested-scan-windows 0 vmd-nested-scan-truncated 0
[x64] drs-nvme-triage storage-triage 1 ... vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-token 0xFAE086EE vmd-nested-pci 0xFFFFFFFF vmd-nested-vendor-device 0x00000000 vmd-nested-class 0x00000000 vmd-nested-bar0 0x00000000 vmd-nested-bar1 0x00000000 vmd-nested-scan-buses 0 vmd-nested-scan-devices 0 vmd-nested-scan-functions 0 vmd-nested-scan-windows 0 vmd-nested-scan-truncated 0 ... stage-match 1 token 0xEF62E7C7
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `734,016` bytes. M144 non-claims: no physical MSI transcript was newly supplied, no multi-bus VMD enumeration is claimed, no VMD MMIO programming is implemented, no child NVMe binding is implemented, no RAID driver is implemented, and no unsafe storage writes are introduced.

## M145 VMD Nested Child NVMe MMIO Preflight

M145 adds the next no-touch boundary after M144's full first-bus VMD child scan. When that scan observes a child NVMe-class function, the kernel now derives conservative BAR/MMIO preflight fields for the first nested child NVMe before reporting that binding is still missing. This is still a diagnostic and planning milestone: it performs no VMD register programming, no controller reset, no controller enable, no NVMe queue setup, and no storage reads/writes through VMD.

New child-NVMe MMIO preflight telemetry:

- `vmd-nested-mmio-low`
- `vmd-nested-mmio-high`
- `vmd-nested-mmio-span`
- `vmd-nested-mmio-flags`
- `vmd-nested-mmio-token`

The hardware storage parser now routes child-NVMe VMD captures through narrower pre-bind stages:

- `pci-vmd-nested-nvme-mmio-base`
- `pci-vmd-nested-nvme-mmio-span`
- `pci-vmd-nested-nvme-mmio-flags`
- `pci-vmd-nested-nvme-bind`

The positive synthetic VMD bind fixture now requires the nested child MMIO fields before it can reach `pci-vmd-nested-nvme-bind`, so a physical MSI transcript with a child NVMe behind VMD will identify whether the next missing piece is BAR base derivation, span telemetry, safety flags, or the actual VMD/NVMe bind path.

Accepted commands:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m145-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m145-storage-target-fixtures
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
M1 production-slice gate passed for x86_64 (Product profile).

hardware-storage-analysis-fixtures: 61/61
failed: 0

m134-storage-target-fixtures: 9/9
failed: 0
```

Representative QEMU telemetry:

```text
pci vmd nested plan: 0
pci vmd nested enum: 0
pci vmd nested nvme: 0
pci vmd nested status: 0
pci vmd nested first bdf: 0xFFFFFFFF
pci vmd nested scan buses: 0
pci vmd nested scan devices: 0
pci vmd nested scan functions: 0
pci vmd nested scan windows: 0
pci vmd nested scan truncated: 0
pci vmd nested mmio low: 0x00000000
pci vmd nested mmio high: 0x00000000
pci vmd nested mmio span: 0
pci vmd nested mmio flags: 0x00000000
pci vmd nested mmio token: 0x00000000
[x64] drs-nvme-pci nvme-pci-diag 1 ... vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-pci 0xFFFFFFFF vmd-nested-scan-buses 0 vmd-nested-scan-devices 0 vmd-nested-scan-functions 0 vmd-nested-scan-windows 0 vmd-nested-scan-truncated 0 vmd-nested-mmio-low 0x00000000 vmd-nested-mmio-high 0x00000000 vmd-nested-mmio-span 0 vmd-nested-mmio-flags 0x00000000 vmd-nested-mmio-token 0x00000000
[x64] drs-nvme-triage storage-triage 1 ... vmd-nested-plan 0 vmd-nested-enum 0 vmd-nested-nvme 0 vmd-nested-status 0 vmd-nested-pci 0xFFFFFFFF vmd-nested-scan-buses 0 vmd-nested-scan-devices 0 vmd-nested-scan-functions 0 vmd-nested-scan-windows 0 vmd-nested-scan-truncated 0 vmd-nested-mmio-low 0x00000000 vmd-nested-mmio-high 0x00000000 vmd-nested-mmio-span 0 vmd-nested-mmio-flags 0x00000000 vmd-nested-mmio-token 0x00000000 ... nvme-ready 1 nvme-identify 1 read-status 0 fat-located 1 stage-match 1
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `733,664` bytes. M145 non-claims: no physical MSI transcript was newly supplied, no VMD register programming is implemented, no VMD controller reset or enable is implemented, no nested NVMe bind is implemented, no RAID driver is implemented, and no unsafe storage reads/writes through VMD are introduced.

## M146 VMD Nested Child NVMe Bind-Readiness Gate

M146 adds the final no-touch diagnostic boundary before a real VMD-backed NVMe bind. When a VMD nested scan reports a child NVMe and M145 child MMIO preflight is valid, the kernel now derives bind-readiness telemetry. The readiness gate says whether child identity, class, BAR-derived MMIO base/span/flags, and token state are sufficient for a future registration path. It does not register the child with the existing NVMe driver and performs no controller access.

New fields:

- `vmd-nested-bind-ready`
- `vmd-nested-bind-status`
- `vmd-nested-bind-token`

Status values:

- `0`: not applicable
- `1`: enumeration pending
- `2`: no child NVMe
- `3`: child identity/class invalid
- `4`: child MMIO invalid
- `5`: ready but unbound

Host capture routing now includes:

- `pci-vmd-nested-nvme-bind-ready`
- `pci-vmd-nested-nvme-bind`

Accepted commands:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m146-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m146-storage-target-fixtures
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
M1 production-slice gate passed for x86_64 (Product profile).

hardware-storage-analysis-fixtures: 62/62
failed: 0

m134-storage-target-fixtures: 10/10
failed: 0
```

Representative QEMU telemetry:

```text
pci vmd nested bind ready: 0
pci vmd nested bind status: 0
pci vmd nested bind token: 0x00000000
[x64] drs-nvme-pci ... vmd-nested-bind-ready 0 vmd-nested-bind-status 0 vmd-nested-bind-token 0x00000000
[x64] drs-nvme-triage ... vmd-nested-bind-ready 0 vmd-nested-bind-status 0 vmd-nested-bind-token 0x00000000 ... nvme-ready 1 nvme-identify 1 read-status 0 fat-located 1 stage-match 1
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `733,408` bytes. M146 non-claims: no physical MSI transcript was newly supplied, no VMD register programming is implemented, no VMD controller reset or enable is implemented, no nested NVMe registration/bind is implemented, no RAID driver is implemented, and no unsafe storage reads/writes through VMD are introduced.

## M147 VMD Nested Child NVMe Registration-Candidate Deferral

M147 adds the next no-touch diagnostic boundary after M146 bind readiness. When a VMD nested scan reports a child NVMe and the bind-readiness gate is green, the kernel now reports whether that child is a registration candidate for a future scoped NVMe driver handoff. The current Product path intentionally defers that handoff and performs no VMD-backed controller probing or storage I/O.

New fields:

- `vmd-nested-register-candidate`
- `vmd-nested-register-status`
- `vmd-nested-register-token`

Status values:

- `0`: not applicable
- `1`: waiting for bind-readiness
- `2`: registration candidate exported, driver handoff deferred

Host capture routing now includes:

- `pci-vmd-nested-nvme-register-candidate`
- `pci-vmd-nested-nvme-register-deferred`

Accepted commands:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m147-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m147-storage-target-fixtures
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
M1 production-slice gate passed for x86_64 (Product profile).

hardware-storage-analysis-fixtures: 63/63
failed: 0

m134-storage-target-fixtures: 11/11
failed: 0
```

Representative QEMU telemetry:

```text
pci vmd nested register candidate: 0
pci vmd nested register status: 0
pci vmd nested register token: 0x00000000
[x64] drs-nvme-pci ... vmd-nested-bind-ready 0 vmd-nested-bind-status 0 vmd-nested-bind-token 0x00000000 vmd-nested-register-candidate 0 vmd-nested-register-status 0 vmd-nested-register-token 0x00000000
[x64] drs-nvme-triage ... vmd-nested-register-candidate 0 vmd-nested-register-status 0 vmd-nested-register-token 0x00000000 ... nvme-ready 1 nvme-identify 1 read-status 0 fat-located 1 stage-match 1
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `733,152` bytes. M147 non-claims: no physical MSI transcript was newly supplied, no VMD register programming is implemented, no VMD controller reset or enable is implemented, no nested NVMe registration/bind is implemented, no RAID driver is implemented, and no unsafe storage reads/writes through VMD are introduced.

## M148 NVMe Candidate Source/Deferred Handoff Telemetry

M148 adds an explicit MMIO/NVMe candidate-source lane after the M147 VMD registration-candidate deferral. The regular direct PCI NVMe path remains the only active controller registration/probe source. A future VMD nested child can now be recorded as a deferred NVMe candidate source without creating an active probe plan, without setting `nvme-found`, and without touching the VMD controller or child NVMe registers.

New fields:

- `nvme-candidate-source`
- `nvme-candidate-deferred`
- `nvme-candidate-bdf`
- `nvme-candidate-token`

Source values:

- `0`: no NVMe candidate source
- `1`: direct PCI NVMe candidate
- `2`: VMD nested child NVMe candidate, deferred

The direct PCI registration path records source `1`, deferred `0`, sentinel BDF `0xFFFFFFFF`, and a nonzero token while preserving the existing direct NVMe probe/read/FAT flow. The VMD nested registration-deferral path records source `2`, deferred `1`, the child BDF, and a nonzero token only when no direct PCI NVMe plan exists; it deliberately leaves `g_nvme_plan_count` at zero and keeps `nvme-found 0`.

Host capture routing remains compatible with older M147 transcripts, but when the new fields are present it can now distinguish source/deferred propagation failures before reporting the intentional future handoff target:

- `pci-vmd-nested-nvme-mmio-source`
- `pci-vmd-nested-nvme-mmio-deferred`
- `pci-vmd-nested-nvme-register-deferred`

Accepted commands:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-hardware-storage-analysis-fixtures.ps1 -OutputDir .\build\m148-hardware-storage-analysis-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m148-storage-target-fixtures
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
M1 production-slice gate passed for x86_64 (Product profile).

hardware-storage-analysis-fixtures: 63/63
failed: 0

m134-storage-target-fixtures: 11/11
failed: 0
```

Representative QEMU telemetry:

```text
nvme candidate source: 1
nvme candidate deferred: 0
nvme candidate bdf: 0xFFFFFFFF
nvme candidate token: 0x87F22324
[x64] drs-nvme-pci ... nvme-mmio-token 0x51B13EF6 nvme-candidate-source 1 nvme-candidate-deferred 0 nvme-candidate-bdf 0xFFFFFFFF nvme-candidate-token 0x87F22324 ... vmd-nested-register-candidate 0 vmd-nested-register-status 0 vmd-nested-register-token 0x00000000
[x64] drs-nvme-triage ... nvme-mmio-token 0x51B13EF6 nvme-candidate-source 1 nvme-candidate-deferred 0 nvme-candidate-bdf 0xFFFFFFFF nvme-candidate-token 0x87F22324 ... nvme-ready 1 nvme-identify 1 read-status 0 fat-located 1 stage-match 1
[x64] drs-nvme-probe ... drs-nvme-probe-found 1 drs-nvme-probe-bar0 0x000000C000008000 drs-nvme-candidate-source 1 drs-nvme-candidate-deferred 0 drs-nvme-candidate-bdf 0xFFFFFFFF drs-nvme-candidate-token 0x87F22324 drs-nvme-probe-ready 1
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `728,608` bytes. M148 non-claims: no physical MSI transcript was newly supplied, no VMD register programming is implemented, no VMD controller reset or enable is implemented, no nested NVMe registration/bind is implemented, no RAID driver is implemented, and no unsafe storage reads/writes through VMD are introduced.

## M149 Cursor Path Hardware-Display Diagnostics

M149 adds one UEFI Product `hwval` proof line that makes the existing physical display/input capture path actionable for the real laptop symptom where mouse packets and coordinates move but the cursor is not visible. The old `drs-display-readability` and `drs-ui-polish` lines are preserved. The new line is `drs-cursor-path`, with fields for surface readiness, framebuffer format support, compositor/direct mode, cursor visibility, draw counts, cursor position/buttons, bounds, cursor rectangle size, saved-underlay state, final drawn state, and a token.

The host-side analyzer remains backward-compatible with older M117-M148 captures. When `drs-cursor-path` exists, the former broad `pointer-moving-cursor-hidden` failure is split into:

- `cursor-format-unsupported`
- `cursor-surface-not-ready`
- `cursor-draw-not-called`
- `cursor-out-of-bounds`
- `cursor-draw-not-validated`

Accepted commands:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-hardware-display-input-fixtures.ps1
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
M1 production-slice gate passed for x86_64 (Product profile).

hardware-display-input-fixtures: 31/31
failed: 0
```

Representative QEMU telemetry:

```text
display cursor visible: yes
display cursor draws: 151
display direct cursor draws: 154
display cursor x: 560
display cursor y: 420
display cursor in bounds: yes
display cursor surface ready: yes
[x64] drs-display-readability display-readability 1 available 1 width 1280 height 800 pitch 1280 stride-ok 1 bounds-ok 1 scale 2 viewport-x 40 viewport-y 92 viewport-w 904 viewport-h 516 columns 75 rows 28 fit 1 readable 1 clip 0 cursor-visible 1 cursor-draws 210 direct-cursor-draws 212 token 0xF8C98059
[x64] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct 1 font 1 wm 1 desktop 1 taskbar 1 launcher 1 windows 3 cursor-visible 1 token 0x99B377DB
[x64] drs-cursor-path cursor-path 1 surface-ready 1 format-supported 1 compositor-active 1 compositor-direct 1 visible 1 draws 250 direct-draws 252 x 560 y 420 buttons 0 in-bounds 1 rect-w 12 rect-h 20 saved 1 drawn 1 token 0xD2FC1305
```

Accepted evidence preserves BIOS reserve `101` sectors and records UEFI reserve `728,352` bytes after the first build. The full UEFI Product QEMU verifier also passed after the hardware-display gate. M149 non-claims: no physical MSI laptop display/input certification is claimed, no native GPU driver or DRM/KMS path is added, no touchpad driver is added, and cursor drawing behavior is not changed. The milestone makes the next real hardware capture distinguish the exact cursor/display failure before any hardware-specific drawing or input changes are attempted.

## M150 MSI Cursor-Stage Routing Closure

M150 closes the host-side handoff gap after M149. The lower display/input analyzer can now report precise cursor stages, and the higher-level MSI tools now preserve those stages through the combined analysis and M134 target classifier instead of routing every display/input failure back to completed M149 work.

Routing behavior:

- Legacy broad captures without `drs-cursor-path` keep reporting `pointer-moving-cursor-hidden` and roadmap target `M149`, which tells the tester to rerun with an M149-or-newer image for sharper cursor evidence.
- M149-era precise cursor captures such as `cursor-draw-not-called` report `target_kind display-input`, `target_stage cursor-draw-not-called`, and roadmap target `M150`.
- Storage-first failures continue to route to `M134`.
- Dynamic-handoff failures continue to route to `M83+`.

Accepted commands:

```powershell
.\tools\verify-msi-hardware-analysis-fixtures.ps1
.\tools\verify-m134-storage-target-fixtures.ps1
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
msi-hardware-analysis-fixtures: 5/5
failed: 0

m134-storage-target-fixtures: 12/12
failed: 0
```

Representative fixture routing:

```text
msi-hardware-analysis: display-input-cursor-draw-not-called
display/input stage: cursor-draw-not-called
next target: Cursor target: route brokered pointer packets into compositor/direct cursor update on this hardware path.

m134-storage-target: display-input-cursor-draw-not-called
target kind: display-input
target stage: cursor-draw-not-called
roadmap target: M150
```

Product build, M1 production-slice gate, and full UEFI Product QEMU verification passed after the fixture routing checks. Accepted evidence preserves BIOS reserve `101` sectors and UEFI reserve `728,352` bytes.

M150 changes only host-side routing and fixture coverage; it does not change kernel behavior and does not claim a physical MSI display/input pass. The purpose is to make the next real M149+ hardware transcript land on the exact cursor implementation target rather than a stale milestone bucket.

## M151 MSI GUI Interaction-Stage Routing

M151 extends the host-side physical display/input analyzer past "cursor visible" into real Product desktop interaction evidence. The existing kernel `drs-gui` line already reports right-click, context-menu action, scroll, Terminal scrollback, Terminal selection/copy, window-manager, and capability-routing counters. M151 makes that line actionable in the hardware analyzer when present while keeping older captures backward-compatible.

Routing behavior:

- Captures without `drs-gui` remain valid legacy display/input captures. If display, cursor, and pointer packets are healthy, they still report `display-input-ready`, but their next target asks for an M151-or-newer interactive desktop capture.
- Captures with `drs-gui` must prove Product interaction routing after cursor readiness. Missing interaction evidence now reports precise stages: `gui-interactive-unrouted`, `gui-right-click-unrouted`, `gui-context-action-missing`, `gui-scroll-unrouted`, `gui-terminal-scroll-missing`, and `gui-terminal-selection-missing`.
- The combined MSI analyzer preserves those stages as `display-input-gui-*`.
- The M134 classifier routes `gui-*` display/input failures to roadmap target `M151`, while legacy broad cursor-hidden captures stay on M149 and precise cursor-path captures stay on M150.

Accepted commands:

```powershell
.\tools\verify-hardware-display-input-fixtures.ps1
.\tools\verify-msi-hardware-analysis-fixtures.ps1
.\tools\verify-m134-storage-target-fixtures.ps1
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
hardware-display-input-fixtures: 38/38
failed: 0

msi-hardware-analysis-fixtures: 7/7
failed: 0

m134-storage-target-fixtures: 13/13
failed: 0
```

Product build, M1 production-slice gate, and full UEFI Product QEMU verification passed. BIOS reserve remained 101 sectors and UEFI reserve remained 728,352 bytes.

Representative fixture routing:

```text
hardware-display-input-analysis: gui-right-click-unrouted
detail: Pointer packets and cursor drawing are ready, but no right-click event reached the window manager.

msi-hardware-analysis: display-input-gui-right-click-unrouted
display/input stage: gui-right-click-unrouted

m134-storage-target: display-input-gui-right-click-unrouted
target kind: display-input
target stage: gui-right-click-unrouted
roadmap target: M151
```

M151 changes only host-side analysis, roadmap routing, and fixtures. It does not change kernel behavior and does not claim a physical MSI display/input pass. The milestone makes the next real laptop capture distinguish basic pointer/cursor readiness from actual right-click, scroll, context-menu, and Terminal interaction routing.

## M152 hwval GUI Interaction Telemetry

M152 closes the capture-production side of M151. Before this milestone, the kernel already emitted `drs-gui` during the scaffold/QEMU proof path, and the M151 host analyzer could consume that line when present. A physical handoff transcript produced by the user-visible `hwval` command, however, did not include the `drs-gui` line. M152 adds the existing GUI interaction counters to `hwval` so the MSI laptop capture can be self-contained.

The new `hwval` line includes:

- Product interaction counters: `drs-gui-interactive`, click hit-testing, launcher/Terminal/File Manager/Settings/Installer opens, right-click, context action, scroll, Terminal scroll, Terminal selection/copy, and Terminal cursor.
- Window-manager counters: resize, minimize, restore, and z-order.
- Backend workflow counters: File Manager refresh/write/delete/mkdir/copy/rename/move/edit and Settings load/save/export controls.
- Capability and routing proofs: no ambient input/display/fs, target window/region, focus and z-order before/after, key target window, unfocused-key denials, and input/display/fs tokens.

Accepted commands:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
.\tools\analyze-hardware-display-input-capture.ps1 -InputPath .\build\qemu-x86_64-uefi-debug.log -OutputDir .\build\m152-hwval-gui-qemu-analysis
.\tools\verify-hardware-display-input-fixtures.ps1
```

Verifier output:

```text
M1 production-slice gate passed for x86_64 (Product profile).
hardware-display-input-analysis: display-input-ready
pass: True
hardware-display-input-fixtures: 38/38
failed: 0
```

Accepted `hwval` proof excerpt from the full QEMU Product run:

```text
[x64] drs-gui drs-gui-interactive 1 ... drs-gui-right-click 1 drs-gui-context-action 1 ... drs-gui-scroll 2 ... terminal-scroll 1 ... terminal-selection 2 ... input-token 0x494E5054 display-token 0x44495350 fs-token 0x46535041
```

Final reserves after the M152 build:

```text
bios sectors : 923 / 1024 sectors (101 reserve)
uefi budget  : 1370208 / 2097152 bytes (726944 reserve)
```

M152 does not add new GUI behavior and does not claim a physical MSI display/input pass. It makes the next physical `hwval` capture carry the evidence needed for M151's right-click, scroll, context-menu, and Terminal routing analysis.

## M153 MSI Handoff GUI Telemetry Requirement

M153 closes the host-side contract around M152. The MSI handoff verifier and M134 classifier can now require that a current physical transcript include the `drs-gui` line from `hwval` instead of accepting a display/input-ready capture that only proves readable display, visible cursor, and pointer packets.

New verifier/classifier behavior:

- `tools\verify-msi-hardware-handoff.ps1 -RequireGuiInteractionTelemetry` reads the nested display/input analyzer JSON and emits `gui_interaction_checked`, `gui_interaction_required`, `gui_interaction_pass`, `gui_interaction_stage`, `gui_interaction_line_found`, and the key right-click/context/scroll/Terminal fields.
- A capture with storage ready, display/input ready, and dynamic source-2 ready but no `drs-gui` line now fails handoff verification with `gui_interaction_pass False` and `gui_interaction_stage gui-telemetry-missing`.
- `tools\classify-m134-storage-target.ps1 -RequireGuiInteractionTelemetry` routes that exact stale-capture case to `target_kind display-input`, `target_stage gui-telemetry-missing`, and roadmap target `M152`.
- `tools\prepare-hardware-storage-evidence.ps1` now writes the stricter classifier/verifier commands and records `required_gui_interaction_telemetry = 1` in the generated handoff manifest.

Accepted commands:

```powershell
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m153-msi-handoff-fixtures
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m153-storage-target-fixtures-final
.\tools\verify-msi-hardware-analysis-fixtures.ps1 -OutputDir .\build\m153-msi-analysis-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product
```

Verifier output:

```text
msi-hardware-handoff-fixtures: 14/14
failed: 0

m134-storage-target-fixtures: 14/14
failed: 0

msi-hardware-analysis-fixtures: 7/7
failed: 0

M1 production-slice gate passed for x86_64 (Product profile).
```

Representative M153 stale-capture routing:

```text
msi-hardware-handoff:
  combined capture pass: True
  combined capture stage: msi-hardware-ready
  gui interaction pass: False
  gui interaction stage: gui-telemetry-missing
  dynamic handoff pass: True
  dynamic handoff stage: dynamic-runtime-exit0

m134-storage-target: display-input-gui-telemetry-missing
target kind: display-input
target stage: gui-telemetry-missing
roadmap target: M152
```

Final reserves are unchanged from M152 because this is host-side tooling and docs:

```text
bios sectors : 923 / 1024 sectors (101 reserve)
uefi budget  : 1370208 / 2097152 bytes (726944 reserve)
```

M153 non-claims: no kernel behavior changed, no new GUI behavior is claimed, no hardware driver support is added, and no physical MSI pass is claimed. The milestone ensures the next current MSI capture cannot accidentally pass handoff verification without the GUI interaction telemetry that M152 made available.

## M154 Current MSI Handoff Bundle Refresh

M154 refreshes the actual physical handoff media after the M153 verifier contract change. `tools\prepare-hardware-storage-evidence.ps1` generated a fresh self-verified bundle at ignored path `dist\m133-msi-hardware-handoff-current`, using the current M153 runbook and manifest contract while preserving the M133 media naming consumed by the existing hardware checklist.

Accepted command:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m133-msi-hardware-handoff-current
```

Generated bundle:

```text
dist\m133-msi-hardware-handoff-current
```

Bundle artifacts:

- ISO: `limitlessos-x86_64-m133-handoff.iso`, SHA-256 `cb9ee01c7bb32efa3af4bace88207076cf53da8727f25b0a044591d341e72b78`
- UEFI FAT image: `limitlessos-x86_64-m133-handoff-uefi.img`, SHA-256 `0fc034ad61b0a91f87fbd7407aeb29ec7ddbc87ef9898e81646596b5ac54c39f`
- `/APPS/DYNLDLIMIT`: 15,680 bytes, SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`
- `/APPS/LDLIMIT`: 16,704 bytes, SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`

The packager rebuilt Product media with the dynamic app/interpreter staged into the UEFI FAT image, ran the M1 production-slice gate, ran the staged storage gate, and embedded `msi-handoff-verification` output. Bundle-only handoff verification proves the hashes, reserves, runbook, source-2 boot-media expectation, and that the current capture verifier will require GUI interaction telemetry when a transcript is supplied.

Accepted output:

```text
hardware-storage-evidence: verified
  bundle pass: True
  bios reserve: 101 sectors
  uefi reserve: 726944 bytes

msi-hardware-handoff: verified
  handoff pass: True
  source2 required: 2
  bios reserve: 101 sectors
  uefi reserve: 726944 bytes
```

Current hardware classification command:

```powershell
.\tools\classify-m134-storage-target.ps1 `
  -EvidenceDir .\dist\m133-msi-hardware-handoff-current `
  -CapturePath .\dist\msi-hwval-storage.txt `
  -OutputDir .\dist\m134-msi-storage-target `
  -RequireStagedDynamicArtifacts `
  -RequireGuiInteractionTelemetry
```

Final reserves:

```text
bios sectors : 923 / 1024 sectors (101 reserve)
uefi budget  : 1370208 / 2097152 bytes (726944 reserve)
```

M154 non-claims: this does not certify the physical MSI laptop, add a new driver, prove real hardware storage/display/input behavior, or prove `/APPS/DYNLDLIMIT` exits successfully on hardware. It packages the current evidence media and ensures the next physical transcript is classified under the M153 GUI telemetry requirement.

## M155 MSI Capture Intake Report

M155 adds a single host-side intake report wrapper for the current physical MSI transcript workflow. `tools\report-msi-hardware-capture.ps1` runs the strict M134 classifier with the same staged dynamic artifact and M153 GUI telemetry requirements, preserves expected classifier exit code `2` as an actionable first-failure result, and writes:

- `msi-hardware-capture-report.json`
- `msi-hardware-capture-report.txt`
- `msi-hardware-capture-report.md`

The report records target kind, target stage, roadmap target, next target, storage/display-input/dynamic-handoff stages, GUI interaction status, evidence artifact hashes, BIOS/UEFI reserves, and paths to the lower-level classifier, handoff verifier, and combined analyzer JSON. This gives the project manager and hardware tester one stable report to read before diving into raw analyzer artifacts.

Accepted command:

```powershell
.\tools\verify-msi-hardware-capture-report-fixtures.ps1 -OutputDir .\build\m155-msi-capture-report-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Fixture output:

```text
m134-storage-target-fixtures: 14/14
failed: 0

msi-hardware-capture-report-fixtures: 3/3
  failed: 0

M1 production-slice gate passed for x86_64 (Product profile).
```

The report fixtures prove:

- `storage-ready` exits `0` and reports `target_kind storage-ready`, `target_stage storage-ready`, roadmap target `M134`, pass `True`.
- `display-gui-telemetry-missing` exits `2` and reports `target_kind display-input`, `target_stage gui-telemetry-missing`, roadmap target `M152`, pass `False`.
- `dynamic-after-storage-ready` exits `2` and reports `target_kind dynamic-handoff`, `target_stage dynamic-handoff-nvme-unavailable`, roadmap target `M83+`, pass `False`.

Current report command for a physical transcript:

```powershell
.\tools\report-msi-hardware-capture.ps1 `
  -EvidenceDir .\dist\m133-msi-hardware-handoff-current `
  -CapturePath .\dist\msi-hwval-storage.txt `
  -OutputDir .\dist\msi-hardware-capture-report `
  -RequireStagedDynamicArtifacts `
  -RequireGuiInteractionTelemetry
```

Final reserves are unchanged because this is host-side tooling and documentation:

```text
bios sectors : 923 / 1024 sectors (101 reserve)
uefi budget  : 1370208 / 2097152 bytes (726944 reserve)
```

M155 non-claims: this does not certify the physical MSI laptop, add a new driver, change kernel behavior, prove real hardware storage/display/input behavior, or prove `/APPS/DYNLDLIMIT` exits successfully on hardware. It makes the next real transcript produce one manager-readable, artifact-linked first-failure report.

## M156 MSI Handoff Report Contract Integration

M156 threads the M155 report wrapper into the generated physical handoff contract. `tools\prepare-hardware-storage-evidence.ps1` now records the report command in the bundle manifest:

```text
expected_hwval.capture_report = tools\report-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry
```

The generated `README-HARDWARE-STORAGE.txt` now tells the hardware tester to run `tools\report-msi-hardware-capture.ps1` first and read `msi-hardware-capture-report.md` or `.txt` before consulting the lower-level classifier/verifier/analyzer artifacts. `tools\verify-msi-hardware-handoff.ps1` rejects handoff bundles that omit the report command from the manifest or runbook, so stale packages cannot bypass the current M155 intake path. `tools\verify-m134-storage-target-fixtures.ps1` was updated to generate the same M156-compatible evidence shape for all downstream classifier and report fixtures.

Accepted commands:

```powershell
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m156-storage-target-fixtures
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m156-msi-handoff-report-contract-fixtures
.\tools\verify-msi-hardware-capture-report-fixtures.ps1 -OutputDir .\build\m156-msi-capture-report-fixtures
.\tools\prepare-hardware-storage-evidence.ps1 -EvidenceDir .\dist\m133-msi-hardware-handoff-current -SkipBuild -SkipQemuGate
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Verifier output:

```text
m134-storage-target-fixtures: 14/14
failed: 0

msi-hardware-handoff-fixtures: 16/16
failed: 0

msi-hardware-capture-report-fixtures: 3/3
failed: 0

msi-hardware-handoff: verified
capture-report: tools\\report-msi-hardware-capture.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry
bios-sector-reserve: 101
uefi-byte-reserve: 726944

M1 production-slice gate passed for x86_64 (Product profile).
```

The current ignored physical handoff bundle was refreshed at:

```text
dist\m133-msi-hardware-handoff-current
```

Bundle artifacts:

- ISO: `limitlessos-x86_64-m133-handoff.iso`, SHA-256 `3f4cb1629e31d6e40d086d04c423f6a9f44ba37e27f85e3bc41b0fe7fc0bce48`
- UEFI FAT image: `limitlessos-x86_64-m133-handoff-uefi.img`, SHA-256 `62e652a4d5ac6fa387fa5948db262efdc87abdbc4a56cf7e79f3f9dad93ce443`
- `/APPS/DYNLDLIMIT`: 15,680 bytes, SHA-256 `9f6eb9c05b3065d39bc59d24defe9361267b34cefd4de78f568ddb00497238fa`
- `/APPS/LDLIMIT`: 16,704 bytes, SHA-256 `6f713105878c30d817b7add4a7ed5d4ee8e01fb6eab2c80ba10acee059c72238`

M156 non-claims: this does not certify the physical MSI laptop, add hardware support, change kernel behavior, prove real storage/display/input behavior, or prove `/APPS/DYNLDLIMIT` exits successfully on hardware. It closes the package-contract loop so the next real transcript is always fed through the M155 report path first.

## M157 VMD/NVMe Handoff Report Surfacing

M157 is accepted as host-side report surfacing for the current VMD/NVMe storage handoff boundary. It does not add a VMD driver; it makes the existing no-touch VMD state explicit in every MSI intake report.

Accepted commands:

```powershell
.\tools\verify-m134-storage-target-fixtures.ps1 -OutputDir .\build\m157-storage-target-fixtures
.\tools\verify-msi-hardware-capture-report-fixtures.ps1 -OutputDir .\build\m157-msi-capture-report-fixtures
.\tools\verify-msi-hardware-handoff-fixtures.ps1 -OutputDir .\build\m157-msi-handoff-fixtures
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Accepted output:

```text
m134-storage-target-fixtures: 14/14
failed: 0

msi-hardware-capture-report-fixtures: 3/3
failed: 0

msi-hardware-handoff-fixtures: 16/16
failed: 0

M1 production-slice gate passed for x86_64 (Product profile).
```

New report contract:

- `tools\analyze-hardware-storage-capture.ps1` emits `vmd_handoff.kind` and `vmd_handoff.stage` plus nested bind/register status and NVMe candidate source/deferred fields.
- `tools\verify-hardware-storage-evidence.ps1`, `tools\analyze-msi-hardware-capture.ps1`, `tools\classify-m134-storage-target.ps1`, and `tools\report-msi-hardware-capture.ps1` preserve the same object through the full report chain.
- The existing `storage-pci-vmd-nested-nvme-register-deferred` fixture now asserts `vmd_handoff.kind = vmd-nested-deferred` and `vmd_handoff.stage = registration-deferred`.
- The deferred proof corresponds to raw telemetry `vmd-nested-register-candidate 1`, `vmd-nested-register-status 2`, `nvme-candidate-source 2`, `nvme-candidate-deferred 1`, and `nvme-found 0`.

Final reserves are unchanged because this is host-side tooling and documentation:

```text
bios sectors : 923 / 1024 sectors (101 reserve)
uefi budget  : 1370208 / 2097152 bytes (726944 reserve)
```

M157 non-claims: this does not certify the physical MSI laptop, add a VMD driver, bind a VMD-backed NVMe controller, change kernel behavior, prove real storage/display/input behavior, or perform unsafe storage I/O. It makes the next physical VMD transcript route to the correct scoped driver target instead of hiding the boundary in raw telemetry.

Later targets are:

- dynamic Linux ELF with `PT_INTERP`, relocations, libc, environment, and filesystem semantics
- Windows PE console executable with real import resolution and process parameters
- Mach-O executable with a real loader path, only after the Linux and Windows paths have stopped being scaffolds

## Hardware And Daily-Driver Order

The daily-driver path is gated by real hardware:

- Terminal reliability first: the QEMU verifier now has a real BusyBox `sh` banner/prompt proof through brokered console output and bounded brokered stdin; hardware-terminal and GUI-focus behavior still need physical-device evidence before broader daily-driver claims.
- Network next: wired Ethernet should be attempted before Intel AX1675 Wi-Fi because Wi-Fi requires firmware loading, regulatory handling, scan/auth/association, key management, and a full 802.11 data path.
- Persistent storage next: make the NVMe namespace readable through a user-visible filesystem path, then add safe writes only to an explicitly approved LimitlessOS target.
- External binary execution: the UEFI Product path now runs a real third-party static ELF from user-visible NVMe storage; next external-binary work should deepen process semantics rather than count synthetic or fixture binaries as Product progress.
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
