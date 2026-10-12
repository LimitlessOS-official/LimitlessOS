# Budgets

LimitlessOS has four hard size/memory budgets. `tools\build.ps1` measures all of them on every x86_64 build, prints them in the `Build complete` summary, writes them to `dist\limitlessos-x86_64.size.txt`, and fails the build when a hard limit is crossed.

## Current values

Measured 2026-09-27 (M193/M194, MSYS2 gcc 16.2.0, binutils 2.47, nasm 3.02), Product profile, with `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` staged:

| Budget | Limit | Used | Reserve | Warning line | State |
|---|---|---|---|---|---|
| BIOS fallback kernel (`KERNEL64-BIOS.BIN`) | 1024 sectors | 923 sectors (472,160 B) | 101 sectors | 128 sectors | Inside hard limit, below warning |
| UEFI kernel file (`KERNEL64.BIN`) | 2,097,152 B | 1,411,968 B | 685,184 B | none | Healthy |
| UEFI low window (kernel image end vs. stage area) | `0xFC0000` | ends at `0xF3AE40` | 545,216 B | 131,072 B | Healthy |
| UEFI FAT12 boot image | 2044 clusters (8 MiB) | 369 clusters | 1675 clusters (6.86 MB) | 131,072 B | Healthy |

Exact byte counts shift slightly with compiler version; the gcc 15-era builds recorded in `docs/history/` measured the UEFI kernel at 1,424,256 bytes.

## BIOS fallback kernel

- **What:** `KERNEL64-BIOS.BIN`, loaded by `boot/boot64.asm` in 127-sector chunks to physical `0x10000`.
- **Hard limit:** 1024 sectors, set by the low-memory stack window the BIOS loader must not overrun. The build throws above 1024 and below a 96-sector reserve.
- **Warning:** reserve below 128 sectors.
- **Policy:** the BIOS kernel is a frozen, checksum-only fallback. New features go into the UEFI kernel only (the build excludes persona, Linux, networking, signing, identity, and installer sources from the BIOS link). The reserve has held at 101 sectors since M107 (June 2026).
- **Where the bytes go:** `mmio.c` contributes about 186 KB of BIOS `.text` (the AHCI planning and denial chain), the scaffold unity build about 107 KB, `syscall.c` about 42 KB. Recovering the 128-sector warning line means splitting the AHCI planner so the BIOS lane compiles only what a BIOS/IDE boot can reach, while keeping the BIOS verifier's `mmio planner` assertions intact. That is tracked as a roadmap item, not a blocker.

## UEFI kernel file

- **What:** `KERNEL64.BIN`, the flat UEFI Product kernel read by `BOOTX64.EFI`.
- **Hard limit:** 2 MiB, the loader's handoff buffer (`LIMITLESS_UEFI_LOADER_BUFFER_BYTES` in `uefi_app.c`). The loader verifies byte count and `fnv1a-32` checksum from `BOOTMAN.TXT`, which also records `kernel-sha256`.

## UEFI low window

Added in M193. This was the budget that actually broke in M191.

- **What:** the kernel is linked at virtual `0x10000` and executes through a 16 MiB low alias (`0x0`–`0x1000000`). The loader maps that alias onto the first 16 MiB of the 32 MiB kernel window it owns (see the extension below): physical `0x0` when the fixed placement succeeds, or a 2 MiB-aligned fallback window elsewhere (the usual case on OVMF and real firmware). The image footprint is `.text + .rodata + .data + .bss` up to the linker symbol `__kernel_end`; `.bss` alone is about 14.5 MB.
- **Stage area:** the top 256 KiB of the window, `LIMITLESS_BOOT_MEDIA_STAGE_BASE` = `0xFC0000` to `0x1000000`, holds boot-media files staged by the loader (the Linux app and interpreter, up to 128 KiB each).
- **Contract:** defined once in `kernel/include/boot_info.h`. The build reads `__kernel_end` from the linked UEFI kernel with `nm`, throws if it crosses the stage base, warns under 128 KiB of reserve, and compiles the loader with `LIMITLESS_UEFI_KERNEL_IMAGE_END`; the loader has a matching `#error`. At run time `boot_media.c` rejects any staged range that is not inside the stage area and above `__kernel_end`.
- **Why it exists:** before M193 the loader staged files at physical `0x100000` and punched those pages through the kernel's low alias. Once `.text` grew past `0x100000`, the staged interpreter replaced `syscall64_dispatch` and the kernel faulted before login. Nothing measured this, because the tracked "UEFI budget" only compared file size with the 2 MiB buffer.

### Kernel window extension (M199)

The loader-owned kernel window is 32 MiB (`LIMITLESS_BOOT_KERNEL_WINDOW_BYTES`). Only the first 16 MiB is aliased low; the upper 16 MiB (`LIMITLESS_BOOT_KERNEL_EXTENSION_BASE` onward) is reachable only through the higher-half alias and holds large kernel buffers. Today that is the compositor back buffer, which needs `width × height × 4` bytes (4 MB at 1280×800, 8.3 MB at 1920×1080; up to 2560×1600 fits).

Before M199 the back buffer was carved from the low window right after `.bss`. With the kernel image ending at `0xF3AE40`, only about 0.8 MB remained, so the allocation always failed and the compositor silently fell back to direct mode, drawing every redraw step straight onto the screen. That was the cause of the flicker, drag artifacts and cursor remnants. In the low window it would also have overlapped the boot-media stage area.

## UEFI FAT12 boot image

- **What:** `dist\limitlessos-x86_64-uefi.img`, the removable-media image also embedded in the ISO.
- **Geometry (M191):** 16384 sectors, 8 sectors per cluster, FAT size solved to a fixpoint. The generator refuses geometries that exceed the 16-bit total-sector field or reach 4085 clusters (FAT16 territory).
- **Reporting:** `tools\generate-uefi-fat-image.ps1` prints a `UEFI FAT12 BUDGET` block (capacity, base content, staged additions, headroom) and fails with an explicit deficit line.

## Checking budgets

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
Get-Content .\dist\limitlessos-x86_64.size.txt
```

Relevant `size.txt` keys: `bios-sector-reserve`, `uefi-kernel-byte-reserve`, `uefi-low-window-kernel-end`, `uefi-low-window-stage-base`, `uefi-low-window-reserve`, plus per-section and top-object sizes for both kernels.
