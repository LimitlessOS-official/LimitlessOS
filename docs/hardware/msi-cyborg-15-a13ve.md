# MSI Cyborg 15 A13VE Manual Validation

Status: pending physical validation.

This checklist is for the M4.1 closure pass. It must be filled from a real UEFI USB boot of `dist\limitlessos-x86_64.iso` on an MSI Cyborg 15 A13VE or equivalent confirmed model. Do not treat QEMU/QMP evidence as a substitute for this checklist.

## Safety Rules

- Boot through UEFI USB only.
- Do not start installer work during M4.1.
- Do not browse or write unsafe internal partitions.
- Do not write the Windows ESP, NTFS partitions, Microsoft Reserved partitions, Recovery partitions, unknown FAT32 partitions, or unknown internal NVMe targets.
- No internal NVMe writes are allowed unless a later safe installer path explicitly enables them through scoped authority.
- Product GUI must preserve the M4 authority model: brokered input, compositor-owned display, window-manager-owned focus/hit-testing, scoped filesystem authority, no ambient storage, no ambient network, and no direct app access to raw framebuffer or raw input.

## Boot Checklist

- [ ] Boot via UEFI USB.
- [ ] Secure Boot state recorded.
- [ ] LimitlessOS desktop appears.
- [ ] Display resolution recorded.
- [ ] Cursor moves visibly.
- [ ] Launcher opens on click.
- [ ] Terminal opens from launcher.
- [ ] Terminal accepts keyboard input.
- [ ] `help` output remains truthful.
- [ ] `apps` output remains truthful.
- [ ] Product apps run in Terminal: `append`, `cat`, `copy`, `delete`, `ls`, `mkdir`, `move`, `rename`, `stat`, `touch`, `write`.
- [ ] Window title-bar drag moves the window.
- [ ] Mouse release exits drag mode.
- [ ] Close button destroys the window and removes it from the taskbar.
- [ ] Taskbar button focuses and raises the corresponding window.
- [ ] File Manager opens.
- [ ] File Manager shows only product-safe brokered namespaces.
- [ ] File Manager does not browse unsafe internal partitions.
- [ ] File Manager does not write unsafe internal partitions.
- [ ] Settings opens.
- [ ] Settings shows real read-only data: display, input backend, network status, storage status, build profile, boot mode, Product/Experimental state.
- [ ] No ambient input authority observed.
- [ ] No ambient display/framebuffer authority observed.
- [ ] No ambient filesystem/storage authority observed.
- [ ] No internal NVMe writes occurred.
- [ ] M5 installer dry-run, if executed, used explicit read-only mode only.
- [ ] M5 installer dry-run output reviewed before any future write/install approval.

## Evidence Record

- Machine model:
- BIOS/firmware version:
- Boot mode:
- Boot media:
- Secure Boot state:
- Build profile:
- ISO filename/checksum:
- Kernel bytes:
- BIOS sectors/reserve:
- UEFI byte budget:
- Kernel checksum:
- Input backend used:
- Display resolution:
- Mouse/touchpad result:
- Keyboard result:
- Terminal result:
- File Manager result:
- Settings result:
- Internal storage write status:
- M5 dry-run output filename:
- M5 dry-run forbidden partition summary:
- Network status:
- Product app test notes:
- Photos/video filenames:
- Tester:
- Date/time:
- Notes:

## Pass Criteria

M4.1 hardware validation passes only when the checklist above is completed with no unsafe partition access, no untruthful Product surface, and no ambient authority exception. If any item fails, keep M4 accepted as QEMU/QMP-verified only and record the exact failing step here before attempting fixes.
