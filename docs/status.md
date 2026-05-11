# LimitlessOS Status

Last updated: 2026-05-11

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

M3 is `Product Boot Contract + Network Promotion`.

M3 splits the legacy BIOS loader ceiling from the UEFI Product boot contract and promotes the brokered DHCP/DNS/TCP/HTTP status path to Product. Networking remains hardware-gated and does not expose sockets or ambient packet authority.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifact:

- kernel bytes: 468688
- BIOS kernel sectors: 916 / 1024
- BIOS reserve: 108
- UEFI kernel byte limit: 2,097,152 bytes
- UEFI byte reserve: 1,628,464
- checksum: 0x90E2FA90
- BIOS sector budget status: warning, because reserve is below 128
- boot contract: dual path. BIOS keeps the 1024-sector hard limit and 128-sector warning. UEFI Product uses a 2 MiB `KERNEL64.BIN` file-size contract verified against `BOOTMAN.TXT` byte count and checksum, with no UEFI sector arithmetic.

Product behavior:

- x86_64 boot through disk, UEFI removable media, and UEFI ISO verification
- persistent ring-3 shell
- truthful `help`, `apps`, and `ls apps` output
- brokered DHCP/DNS/TCP/HTTP network status through the `net` shell builtin when virtio-net or e1000e hardware is present
- brokered persistent file workflow
- NVMe persistence verification path
- capability denial checks
- no ambient authority

Product apps:

- APPEND
- CAT
- COPY
- DELETE
- LS
- MKDIR
- MOVE
- RENAME
- STAT
- TOUCH
- WRITE

Product builtins:

- apps
- help
- info
- net
- pwd

Unavailable or not product-path in Product:

- ASK, not AI and no consent-gated assistant path
- ECHO
- shell aliases SAY, SHOW, LIST, MAKE, PUT, SWAP, SHIFT
- GUI, compositor, window manager, desktop, File Manager GUI, Settings GUI
- installer
- package manager
- AI assistant behavior

## Experimental Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental
```

Current Experimental artifact:

- produced by `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental`
- recorded in `dist\limitlessos-x86_64.experimental.m3.json` when the Experimental profile is built
- not Product and not a daily-driver surface

Experimental may initialize proof surfaces including GUI/compositor/window-manager/desktop and broad hardware proof telemetry. These are not Product behavior and must remain labeled experimental or proof-only in logs and reports.

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

- Product BIOS reserve is still below the 128-sector warning threshold.
- Product UEFI now uses the 2 MiB file-size contract; BIOS remains on the 1024-sector loader contract.
- GUI/window manager/desktop are experimental and not Product.
- Network is Product only as a brokered hardware-gated status path. There is no socket API, packet API, or ambient network authority.
- Installer and package manager are unavailable.
- There is no real AI assistant path.
- Hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
