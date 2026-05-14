# VirtualBox UEFI Handoff Checklist

M18.1 fixes the UEFI handoff so the loader no longer assumes that firmware leaves fixed low physical pages available.

## Scope

- Boot target: `dist\limitlessos-x86_64.iso`
- Firmware mode: VirtualBox EFI enabled
- Product path: UEFI Product `KERNEL64.BIN`
- Safety: read-only boot validation; no internal disk writes; no installer write/format/NVRAM authority

## Expected Boot Evidence

- `BOOTX64.EFI` starts.
- `KERNEL64.BIN` is read and checksum-matched against `BOOTMAN.TXT`.
- High kernel placement succeeds in firmware-reported conventional memory.
- Linked kernel placement reports either fixed low placement success or fallback placement success.
- Boot handoff tables report `built 1 ready 1`.
- The log reaches `[uefi] exit boot services status 0x0000000000000000`.
- The log reaches `[uefi] firmware services offline; jumping to x64 kernel entry`.
- The log reaches `LimitlessOS x86_64 scaffold`.
- The log reaches `[x64] long mode active`.
- The log reaches login/desktop.
- No freeze occurs at `linked kernel placement`.
- No freeze occurs at `boot handoff tables`.
- No `boot cannot continue` line appears unless it is an intentional fail-fast diagnostic with allocation name, requested address, page count, status, conflict type, fallback status, and selected fallback base.

## Manual Steps

1. Create or reuse a VirtualBox VM with EFI enabled.
2. Attach `dist\limitlessos-x86_64.iso` as optical media.
3. Boot the VM without attaching any host internal disk as a writable target.
4. Confirm the UEFI loader reaches ExitBootServices.
5. Confirm the x64 kernel banner appears.
6. Confirm the login screen or desktop appears.
7. Confirm the boot does not freeze after `KERNEL64.BIN` is loaded.
8. Record a screenshot or serial/debug log if available.

## Evidence Record

Record:

- VirtualBox version:
- Host OS:
- VM firmware mode:
- ISO path:
- KERNEL64.BIN bytes:
- KERNEL64.BIN checksum:
- linked kernel placement line:
- boot handoff table line:
- ExitBootServices line:
- x64 kernel entry line:
- login/desktop result:
- screenshot/log filenames:
- tester notes:
