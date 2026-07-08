# Overnight Status

## 2026-07-08 - I2C HID GNVS field-offset derivation

- Task: Generality - replace the remaining MSI-specific `TPDB`/`TPDS` GNVS byte-offset constants in the I2C HID ACPI path with a targeted AML `FieldOp` walker that derives those byte offsets from `Field (GNVS, ...)` definitions.
- Commit: `3e8da7a7af048f058a285e66a4228a72af61457f`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The I2C HID ACPI path no longer embeds the MSI-derived GNVS byte offsets `1038` and `1041`; it now parses the DSDT's simple-NameSeg AML field list for `TPDB` and `TPDS`, then reads those derived GNVS byte offsets at boot. A local offline check against the real MSI DSDT dump derived `TPDB=1038` and `TPDS=1041`, matching the previous hardware analysis without keeping those numbers in kernel source. This is still a minimal targeted AML walker, not a full AML interpreter: it is verified on QEMU for the no-I2C path and on the saved MSI DSDT for offset derivation, but the live MSI GNVS read and touchpad bind remain unverified until H1 boots the new image and sends `hwval full`.
