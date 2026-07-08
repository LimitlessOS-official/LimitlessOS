# Overnight Status

## 2026-07-08 - File Manager edit helper wording

- Task: Visual/UX - replace the File Manager edit-mode helper's technical `Enter commits` wording with a clearer action instruction while preserving the existing edit commit behavior.
- Commit: `940881755b50ddea4f8c2a147132ffdbc479b4e0`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The File Manager edit preview now says `Enter to apply` instead of `Enter commits`, keeping the same edit buffer, Enter handling, NVMe FAT authority, and mutation policy while using a plain user-facing action phrase. This is grounded in the existing File Manager edit-mode surface and UI writing guidance that instructions should use concise, specific verbs and reduce cognitive load. Verified by Product build and the UEFI QEMU hardware/display gate; the default gate reports the File Manager surface but does not enter edit mode, and this remains unverified on physical MSI because it is visible text polish only.

## 2026-07-08 - File Manager delete confirmation wording

- Task: Visual/UX - replace the File Manager's vague destructive-action confirmation hint with explicit delete-confirmation wording while preserving the existing two-step delete behavior.
- Commit: `2b77b1d59a5a0aeb0e9fb4325a2e2067b3fb6bcf`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The File Manager delete guard now shows `Confirm delete` instead of `Click again` after the first delete request, making the destructive action explicit without changing the existing confirmation state machine, storage authority, NVMe FAT behavior, or deletion policy. This is grounded in the existing File Manager two-step delete flow and UI guidance that destructive actions should use concise, specific confirmation text. Verified by Product build and the UEFI QEMU hardware/display gate; the default gate reports the File Manager surface and `fileman-storage-card 1`, but it does not trigger the delete-confirm path, and this remains unverified on physical MSI because it is visible text polish only.

## 2026-07-08 - File Manager status badge wording

- Task: Visual/UX - replace terse File Manager status-card badges with clearer state words while preserving the existing storage-ready and storage-unavailable behavior.
- Commit: `275afde22b3d63a0771dfcd8d1f4d52dbe96e6b9`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The File Manager status card now labels ready storage states as `Ready` and unavailable storage states as `Down` instead of the terse `OK`/`WAIT` pair, keeping the same detection path and card layout while making the visible state easier to scan. This is grounded in the existing Product File Manager status surface and UI writing guidance favoring clear, concise, helpful language. Verified in QEMU through the Product GUI/readiness and hardware/display gate, including `fileman-storage-card 1`; unverified on physical MSI because this is visible text polish and does not change storage, NVMe, USB, ACPI, or input behavior.

## 2026-07-08 - Open command failure error state

- Task: Visual/UX - replace the `open` command's GUI-specific runtime failure copy with a concise command-scoped error state while preserving the existing usage text for invalid targets.
- Commit: `bdb85db833fe2c9ef23c64999f168d3844326528`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Product shell now reports a failed GUI-launch request as `open: not ready` instead of `gui open unavailable`, keeping the runtime failure tied to the command the user ran while leaving `usage: open <terminal|files|settings|installer|assistant>` as the invalid-target path. This is grounded in the existing LimitlessOS `open` command surface and general CLI guidance that usage text and runtime errors should stay distinct and succinct. Verified by Product build and the UEFI QEMU display/hardware gate; the exact `open` failure branch is not exercised by the default gate and remains unverified on physical MSI because this is shell copy only, not hardware behavior.

## 2026-07-08 - Settings hardware terminology polish

- Task: Visual/UX - normalize visible Settings and File Manager hardware terminology so Product panels use standard names such as `NVMe`, `USB`, and `I2C` instead of inconsistent acronym casing.
- Commit: `14633116ba21014b17aa09dba6929bf0b862505b`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The visible Product GUI storage/input copy now uses `NVMe FAT`, `NVMe FAT32`, `USB reports`, and `I2C` consistently in Settings and File Manager surfaces, with the network detail row changed from the awkward `device(s)` wording to `devices`. The change is grounded in the repo's existing terminology and public standards/trademark naming (`NVMe` from NVM Express and `USB` from USB-IF), and it does not change hardware detection, driver binding, capability grants, storage authority, or MSI-specific behavior. Verified in QEMU through the existing Product GUI/readiness telemetry and full UEFI hardware/display gate; unverified on physical MSI because this is visible text polish rather than a hardware-driver change.

## 2026-07-08 - Shell unknown-command error state

- Task: Visual/UX - replace the bare shell `unknown command` response with a concise actionable error state that points users back to the existing `help` discovery surface.
- Commit: `c2c407ced8aada96ebea91306452a99d8f7a5628`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Product shell now prints `unknown: help` from both unknown-command fallbacks instead of the dead-end `unknown command` text, keeping the command surface unchanged while making the error state more intentional and discoverable. This was grounded in the existing LimitlessOS `help` command surface and general CLI guidance that errors should suggest what to do next; the fuller token-echoing version was attempted but abandoned because it exceeded the 1.44 MiB UEFI FAT image budget. Verified in QEMU through the existing mistyped `imitless`, `limitless`, and `exit` transcript paths; unverified on physical MSI because this is shell text behavior rather than hardware-dependent functionality.

## 2026-07-08 - I2C HID GNVS field-offset derivation

- Task: Generality - replace the remaining MSI-specific `TPDB`/`TPDS` GNVS byte-offset constants in the I2C HID ACPI path with a targeted AML `FieldOp` walker that derives those byte offsets from `Field (GNVS, ...)` definitions.
- Commit: `3e8da7a7af048f058a285e66a4228a72af61457f`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The I2C HID ACPI path no longer embeds the MSI-derived GNVS byte offsets `1038` and `1041`; it now parses the DSDT's simple-NameSeg AML field list for `TPDB` and `TPDS`, then reads those derived GNVS byte offsets at boot. A local offline check against the real MSI DSDT dump derived `TPDB=1038` and `TPDS=1041`, matching the previous hardware analysis without keeping those numbers in kernel source. This is still a minimal targeted AML walker, not a full AML interpreter: it is verified on QEMU for the no-I2C path and on the saved MSI DSDT for offset derivation, but the live MSI GNVS read and touchpad bind remain unverified until H1 boots the new image and sends `hwval full`.
