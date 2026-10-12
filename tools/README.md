# Tools

All scripts are PowerShell and resolve the repository root from their own location, so run them from anywhere. Toolchain discovery is shared through `toolchain.ps1`; see [docs/development.md](../docs/development.md).

## Build and images

| Script | Purpose |
|---|---|
| `build.ps1` | Builds a lane (`-Architecture x86` or `x86_64`) and profile (`Product`/`Experimental`); produces kernels, `BOOTX64.EFI`, BIOS/UEFI/NVMe images, and the ISO; enforces [budgets](../docs/budgets.md). |
| `toolchain.ps1` | Dot-sourced helper that locates MSYS2 MINGW64 and QEMU for the current process. |
| `assert-m1-production-slice.ps1` | Product artifact/source gate, run at the end of every Product build. |
| `generate-package-store.ps1` | Builds and Ed25519-signs the bootstrap package store from `packages/bootstrap-store.json`. |
| `generate-x64-runtime-image.ps1` | Assembles the sealed ring-3 shell image (`runtime_image_user.asm`) and its generated header. |
| `generate-uefi-fat-image.ps1` | Builds the FAT12 UEFI boot image and prints the FAT budget. |
| `generate-iso.ps1` | Builds the UEFI El Torito ISO through Windows IMAPI2. |
| `generate-nvme-image.ps1` | Builds the GPT/FAT32 NVMe test disk used by the QEMU gates. |
| `build-real-busybox-standalone-sh.ps1` | Rebuilds the standalone-shell BusyBox test binary under `external/build`. |

## QEMU verification

| Script | Purpose |
|---|---|
| `verify-qemu.ps1` | Main boot/runtime gate for every lane and boot medium (`-BootMedia disk/iso/uefi`) with optional real-binary and hardware-diagnostic modes. |
| `run-desktop.ps1` | Day-to-day launcher: rebuilds when sources changed, then boots the UEFI desktop in a QEMU window with a persistent virtual disk. |
| `install-desktop-shortcut.ps1` | Adds a "LimitlessOS" desktop icon that runs `run-desktop.ps1`. |
| `run-qemu.ps1` | Boots any built lane/medium interactively with a throwaway NVMe snapshot. |
| `verify-real-binary-gate.ps1` | Real third-party Linux binary gate with provenance recording. |
| `verify-boot-media-linux-handoff.ps1` | Loader staging of `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` plus the kernel boot-media fallback. |
| `verify-nvme-persistence.ps1` | Two-boot NVMe persistence and write/commit authority. |
| `verify-login-m10.ps1`, `verify-identity-m11.ps1`, `verify-identity-transport-m12.ps1`, `verify-account-association-m13.ps1`, `verify-cloud-storage-m14.ps1`, `verify-installer-ux-m15.ps1`, `verify-ai-policy-m16.ps1`, `verify-ai-assistant-m17.ps1`, `verify-ai-action-m18.ps1`, `verify-network-socket-m19.ps1`, `verify-app-model-m20.ps1`, `verify-native-app-sdk-m21.ps1` | Per-subsystem gates (see `docs/subsystems/`). |
| `verify-package-m7-1.ps1`, `verify-package-m8-ux.ps1` | Signed-package admission, negative fixtures, and trust UX. |
| `verify-hardware-validation-m9.ps1` | `hwval` read-only validation surface. |
| `verify-private-key-artifacts.ps1` | Scans docs and artifacts for private signing-key material. |

## Installer (host-side, dry-run first)

| Script | Purpose |
|---|---|
| `limitless-installer.ps1`, `installer-common.ps1` | Safe installer: GPT inspection, protected-partition classification, zero-write dry-run plan, fixture-only writes. |
| `generate-installer-fixtures.ps1`, `verify-installer-m5.ps1` | Installer fixture disks and verification. |
| `parse-msi-dryrun-evidence.ps1`, `verify-msi-dryrun-parser-m9.ps1` | Parse and verify real-hardware installer dry-run transcripts. |

## Physical hardware capture and analysis

Used with the MSI Cyborg 15 A13VE handoff bundle; see [docs/hardware/msi-cyborg-15-a13ve.md](../docs/hardware/msi-cyborg-15-a13ve.md).

| Script | Purpose |
|---|---|
| `prepare-hardware-storage-evidence.ps1` | Builds a self-verifying USB handoff bundle (images, hashes, instructions). |
| `start-msi-hardware-capture.ps1`, `finish-msi-hardware-capture.ps1` | Start a capture session and turn a filled transcript into a next-target report. |
| `parse-hardware-storage-capture.ps1`, `analyze-hardware-storage-capture.ps1`, `analyze-hardware-display-input-capture.ps1`, `analyze-msi-hardware-capture.ps1` | Parse and classify `hwval` transcripts by storage and display/input stage. |
| `classify-m134-storage-target.ps1`, `report-msi-hardware-capture.ps1` | Classify the next storage/display target and write the capture report. |
| `verify-msi-hardware-handoff.ps1`, `verify-hardware-storage-evidence.ps1`, `verify-hardware-storage-staging.ps1` | Verify handoff bundles and staged evidence. |
| `verify-*-fixtures.ps1` (storage, display/input, M134, MSI analysis/report/handoff) | Host-only regression fixtures for all of the analyzers above. |
| `collect-windows-hardware-inventory.ps1`, `collect-msi-windows-hardware-inventory.ps1`, `summarize-windows-hardware-inventory.ps1`, `verify-windows-hardware-inventory-summary.ps1` | Capture and summarize the Windows PnP device graph of a target machine. |
| `hwval-windows.ps1`, `verify-hwval-windows.ps1` | Produce a LimitlessOS-style `hwval` transcript from a Windows inventory for side-by-side comparison. |
| `configure-virtualbox-usb.ps1` | VirtualBox USB passthrough setup for the VirtualBox UEFI checklist. |
