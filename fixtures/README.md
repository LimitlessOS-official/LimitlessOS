# Fixtures

Sources for test programs the verifiers run. These are regression and bring-up inputs. Under [docs/real-binary-gate.md](../docs/real-binary-gate.md), repo-authored programs are foundation evidence, never proof that arbitrary third-party software works.

## `linux/`

C and assembly sources for the Linux persona test programs. The built binaries live in the ignored `external/build/` directory under upper-case names (`dynsmoke.c` builds `DYNSMOKE`, `futexsmoke.c` builds `FUTEXSMOKE`, and so on) and are staged onto boot media or the NVMe test image by the build and verifier scripts.

- **Static programs** (`*smoke.c`, `mmapwindow.c`, `zig-musl-smoke.c`) are built as static musl `ET_EXEC` binaries linked at `0x52000000`, for example with `zig cc -target x86_64-linux-musl -static -no-pie -Wl,--image-base=0x52000000`.
- **Dynamic programs** (`dyn*.c`) are built with the musl cross compiler in `external/tools/gcc-linux-musl-x86_64` as dynamic `ET_EXEC` binaries linked at `0x52000000`, with `PT_INTERP` set to `/nvme/apps/ldlimit` and `DT_NEEDED` `libc-x64.so`. The interpreter binary `external/build/LDLIMIT` is a 16 KB static ELF built with GCC 15.2.0; its source is not in this repository or in `external/`, so it cannot currently be rebuilt (see the roadmap).
- `dyndir_import_libc.c` is a minimal libc import surface (`__libc_start_main` and friends) linked into a dynamic directory-listing test; `getdents64_import.c` is the matching `getdents64` import stub (built as `libgetdents64_import.so`).
- `pthread_create_helper.asm` is the reference source for the pthread-create trampoline bytes emitted by `kernel/arch/x86_64/linux_libc.c`.
- `busybox-limitless-ls.config` is the BusyBox 1.35.0 configuration for the `-ls` gate binary.

The exact per-binary build command lines were not recorded when these binaries were produced; each accepted binary's SHA-256, entry point, and link base are recorded in `docs/history/real-binary-gate-log.md`, which is the reference when rebuilding.

## `embedded/`

Sources for programs embedded as byte arrays in the kernel:

| Source | Embedded as | Purpose |
|---|---|---|
| `linux_q1_hello.c` | `kernel/arch/x86_64/linux_q1_hello_elf.inc` | Minimal static Linux ELF for the Linux persona loader checkpoint |
| `windows_k16_hello.c` | `kernel/arch/x86_64/windows_k16_hello_pe.inc` | Minimal PE for the Windows persona `NtWriteFile` path |
| `windows_k17_heap.c` | `kernel/arch/x86_64/windows_k17_heap_pe.inc` | PE exercising the Windows persona heap/virtual-memory path |

Each Windows source carries its compile command in its header comment.
