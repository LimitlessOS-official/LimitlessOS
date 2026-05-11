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

M2 is `Product Kernel Boundary + Experimental Quarantine`.

M2 does not add product features. It separates the accepted M1 product slice from proof/demo/experimental surfaces at build time, runtime, verification time, and documentation time.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifact:

- kernel bytes: 460048
- kernel sectors: 899 / 1024
- reserve: 125
- checksum: 0xDB264D1D
- sector budget status: warning, because reserve is below 128
- boot contract: BIOS-compatible 1024-sector loader contract; UEFI remains bound until explicitly changed

Product behavior:

- x86_64 boot through disk, UEFI removable media, and UEFI ISO verification
- persistent ring-3 shell
- truthful `help`, `apps`, and `ls apps` output
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

Unavailable or not product-path in Product:

- ASK, not AI and no consent-gated assistant path
- ECHO
- shell aliases SAY, SHOW, LIST, MAKE, PUT, SWAP, SHIFT
- GUI, compositor, window manager, desktop, File Manager GUI, Settings GUI
- network service
- installer
- package manager
- AI assistant behavior

## Experimental Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental
```

Current Experimental artifact:

- kernel bytes: 468336
- kernel sectors: 915 / 1024
- reserve: 109
- checksum: 0xA58341E8
- sector budget status: warning

Experimental may initialize proof surfaces including GUI/compositor/window-manager/desktop and network proofs. These are not Product behavior and must remain labeled experimental or proof-only in logs and reports.

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

- Product reserve is still below the 128-sector warning threshold.
- Product UEFI is still constrained by the BIOS-era 1024-sector loader contract until the boot contract is intentionally split.
- GUI/window manager/desktop are experimental and not Product.
- Network is experimental and not Product.
- Installer and package manager are unavailable.
- There is no real AI assistant path.
- Hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
