# MSI Cyborg 15 A13VE Manual Validation

Status: M10 physical validation pending user evidence.

This checklist is for a real UEFI USB boot of `dist\limitlessos-x86_64.iso` on an MSI Cyborg 15 A13VE. QEMU/QMP evidence is useful, but it is not a substitute for this checklist.

## Safety Rules

- Boot through the UEFI USB entry only.
- Run validation and installer dry-run only.
- Do not start a real internal install.
- Do not format any partition.
- Do not create or modify NVRAM boot entries.
- Do not write the Windows ESP.
- Do not write NTFS partitions.
- Do not write Microsoft Reserved partitions.
- Do not write Recovery partitions.
- Do not write unknown FAT32 partitions.
- Do not write unknown GPT partitions.
- Do not browse or write unsafe internal laptop partitions from File Manager.
- Internal NVMe writes remain disabled by default until a later milestone explicitly approves a safe install path.

## Boot And Desktop Checklist

- [ ] Boot from USB using the UEFI entry.
- [ ] Record Secure Boot state.
- [ ] If first-run setup appears, create the initial local user.
- [ ] Confirm login screen appears before the desktop.
- [ ] Log in with the created local user.
- [ ] Confirm desktop appears.
- [ ] Confirm display resolution.
- [ ] Confirm cursor moves.
- [ ] Confirm click opens launcher.
- [ ] Confirm Terminal opens.
- [ ] Confirm keyboard input works in Terminal.
- [ ] Run `help`.
- [ ] Run `apps`.
- [ ] Run `pkginfo`.
- [ ] Run `hwval`.
- [ ] Run `lock`, then unlock with the correct password and confirm the existing session resumes.
- [ ] Confirm `help`, `apps`, `pkginfo`, and `hwval` remain truthful.
- [ ] Run Product apps in Terminal: `append`, `cat`, `copy`, `delete`, `ls`, `mkdir`, `move`, `rename`, `stat`, `touch`, `write`.
- [ ] Confirm window title-bar drag moves a window.
- [ ] Confirm mouse release exits drag mode.
- [ ] Confirm close button destroys a window and removes it from the taskbar.
- [ ] Confirm taskbar button focuses and raises the corresponding window.

## Product App Checklist

- [ ] Open Settings.
- [ ] Confirm Settings shows package trust panel.
- [ ] Confirm Settings shows read-only system data.
- [ ] Confirm network status appears if the NIC path works.
- [ ] Open File Manager.
- [ ] Confirm File Manager shows only product-safe brokered namespaces.
- [ ] Confirm File Manager does not browse unsafe internal partitions.
- [ ] Confirm File Manager does not write unsafe internal partitions.

## Installer Dry-Run Checklist

- [ ] Run installer dry-run only.
- [ ] Capture full installer dry-run output.
- [ ] Parse dry-run output with `tools\parse-msi-dryrun-evidence.ps1`.
- [ ] Confirm Windows ESP is forbidden.
- [ ] Confirm NTFS is forbidden.
- [ ] Confirm Microsoft Reserved partition is forbidden.
- [ ] Confirm Recovery partition is forbidden.
- [ ] Confirm unknown FAT32 partitions are forbidden.
- [ ] Confirm unknown GPT partitions are forbidden.
- [ ] Confirm internal NVMe writes are disabled by default.
- [ ] Confirm no install/write/format/NVRAM action is available.
- [ ] Confirm no real install is approved.

Suggested read-only command shape from Windows/PowerShell after capturing output:

```powershell
.\tools\parse-msi-dryrun-evidence.ps1 -InputPath .\dist\msi-dryrun.txt -OutputPath .\dist\msi-dryrun-evidence.json
```

## Evidence Record

- Machine model:
- BIOS/firmware version:
- Boot mode:
- Boot media:
- Secure Boot state:
- Build profile:
- ISO filename/checksum:
- BIOS Product bytes/sectors/reserve/checksum:
- UEFI Product bytes/reserve/checksum:
- Input backend used:
- Display resolution:
- Mouse/touchpad result:
- Keyboard result:
- GUI result:
- Terminal result:
- `help` result:
- `apps` result:
- `pkginfo` result:
- `hwval` result:
- Login/setup result:
- Lock/unlock result:
- File Manager result:
- Settings result:
- Network status:
- NVMe detection result:
- Installer dry-run result:
- Internal storage write status:
- Forbidden partition detection status:
- M5 dry-run output filename:
- Parsed dry-run evidence filename:
- Photos/video filenames:
- Tester:
- Date/time:
- Notes:

## Pass Criteria

M10 hardware validation passes only when the checklist above is completed with working login/lock behavior, no unsafe partition access, no untruthful Product surface, no ambient authority exception, and no internal install/write/format/NVRAM action. If any item fails, record the exact failing step and keep real internal install blocked.
