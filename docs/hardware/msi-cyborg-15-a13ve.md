# MSI Cyborg 15 A13VE Manual Validation

Status: M18.1 physical validation in progress; June 2026 photos show UEFI Product shell reachability with display, touchpad, and NVMe FAT gaps still open.

This checklist is for a real UEFI USB boot of `dist\limitlessos-x86_64.iso` on an MSI Cyborg 15 A13VE. QEMU/QMP evidence is useful, but it is not a substitute for this checklist.

Post-M21 hardware progress must be based on real device output. Synthetic process tests and QEMU-only driver evidence do not count as MSI laptop network, storage, or daily-driver validation.

## June 2026 Hardware Snapshot

User-provided laptop photos from a real boot show the kernel reaches the persistent `[x64] $` shell and accepts keyboard input. The observed `linux /APPS/DYNLDLIMIT` result is:

```text
linux: NVMe FAT unavailable
drs-realbin-unavailable bios 0 nvme 0
```

Interpretation: this is the UEFI kernel branch (`bios 0`), not the BIOS checksum fallback. The real-binary launcher refuses before ELF parsing because the UEFI storage path did not expose the NVMe FAT source/capability used by the QEMU real-binary gate. Dynamic linker state is not being exercised on this hardware run yet.

Known open hardware gaps from the photos:

- Display reaches GOP framebuffer output, but console/window layout is visibly mis-scaled or overlapped on the laptop panel. New UEFI-only `hwval` fields now report framebuffer pitch, format, base, and byte size to diagnose this.
- Keyboard input works through the brokered shell. The shell waiting for a key is expected; seeded startup command replay is intentionally gone.
- Touchpad/mouse does not move. Diagnostics show the PS/2 keyboard path is alive, PS/2 aux mouse is not producing packets, and the LPSS/I2C touch path reports an error. Capture `i2c pointer found`, `i2c pointer reports`, `i2c pointer error`, `i2c pointer candidates`, and `i2c pointer0 flags/base` from `hwval`.
- `linux /APPS/DYNLDLIMIT` cannot run until a real hardware-accessible Linux-binary source exists. Current QEMU gates stage Linux binaries in the separate NVMe GPT/FAT fixture; the physical USB/ISO boot-media `/APPS` descriptor path is a different read-only route and is not yet a large ELF source for `linux`.

Next hardware evidence to capture with a build containing commit `179a5b34` or later:

```text
hwval
linux /APPS/DYNLDLIMIT
```

Record the full `drs-realbin-unavailable` line, especially `nvme-probe`, `nvme-ready`, `nvme-cap`, `fat-located`, `fat-unavailable`, `fat-error`, `rw-delegated`, and `rw-error`.

For hardware builds that need dynamic-linker artifacts available on the USB boot image itself, the x86_64 Product build can now stage two externally built files into the UEFI FAT boot image:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product `
  -BootLinuxAppPath <path-to-DYNLDLIMIT> -BootLinuxAppName DYNLDLIMIT `
  -BootLinuxInterpPath <path-to-LDLIMIT> -BootLinuxInterpName LDLIMIT
```

This creates `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` inside the UEFI FAT boot image. It is staging groundwork only: `linux /APPS/DYNLDLIMIT` on hardware still requires a kernel boot-media read source or loader handoff path, because the current `linux` command reads from the QEMU NVMe FAT fixture.

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
- [ ] Confirm the UEFI loader does not freeze at linked kernel placement.
- [ ] Confirm the UEFI loader reaches ExitBootServices or prints an intentional `boot cannot continue` fail-fast diagnostic.
- [ ] If a fail-fast diagnostic appears, record allocation name, requested address, page count, EFI status, conflict type, fallback attempt status, and selected fallback base.
- [ ] Confirm the boot reaches `LimitlessOS x86_64 scaffold`.
- [ ] Confirm the boot reaches `[x64] long mode active`.
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
- [ ] Run `pwd`.
- [ ] Run `ls /`.
- [ ] Run `pkginfo`.
- [ ] Run `hwval`.
- [ ] Run `net`.
- [ ] Record the exact shell prompt text.
- [ ] Record whether Terminal input pauses for roughly 10 seconds before commands run.
- [ ] Record whether clicking/focusing Terminal clears or drops typed input.
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
- Linked kernel placement line:
- Boot handoff table line:
- ExitBootServices or fail-fast diagnostic:
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
- Network controller hardware IDs:
- `net` output:
- NVMe detection result:
- User-visible storage path result:
- Terminal delay/focus result:
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

M18.1 hardware validation passes only when the checklist above is completed with a real UEFI handoff into the x64 kernel, working login/lock behavior, no unsafe partition access, no untruthful Product surface, no ambient authority exception, and no internal install/write/format/NVRAM action. If any item fails, record the exact failing step and keep real internal install blocked.
