# Overnight Status

## 2026-07-08 - Product services help wording

- Task: Visual/UX - replace the Product help line's `service/session` shorthand with clearer service and session wording while preserving the same Settings service/session status and installer-planning write-disabled behavior.
- Commit: `126c74fa766a015aa6f3a7f610d98bcfc7a48b81`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: Product help now says `Product services: Settings shows service and session status; installer planning writes disabled` instead of `Product services: Settings shows service/session status; installer planning writes disabled`, keeping the same Settings visibility, service/session status meaning, installer planning status, and write-disabled behavior. The M1 Product source assertion and UEFI QEMU runtime assertion were updated so both gates prove the revised help output rather than stale copy. This is grounded in Microsoft UI text guidance that users scan interface text and that essential state should be concise without over-communication; verified in QEMU, while physical MSI rendering remains unverified because this pass changes visible shell help copy only.

## 2026-07-08 - Apps installer UX status wording

- Task: Visual/UX - replace the `apps` output Installer UX slash-style `launcher/Settings; dry-run planning only; writes disabled` wording with clearer launcher and Settings wording while preserving the same dry-run planning and disabled-write behavior.
- Commit: `e65591f1d0c5fb89cd64f6f915d25f18dc57c286`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Product `apps` listing now says `Installer UX: launcher and Settings show dry-run planning; writes disabled` instead of `Installer UX: launcher/Settings; dry-run planning only; writes disabled`, keeping the same launcher visibility, Settings visibility, dry-run installer planning, and disabled-write behavior. The M1 Product source assertion, UEFI QEMU runtime assertion, and dedicated M15 verifier expectations were updated so the gates prove the revised `apps` output and the already-polished help wording rather than stale copy. This is grounded in Microsoft UI text guidance that users scan interface text and that essential state should be concise without over-communication; verified in QEMU, while physical MSI rendering remains unverified because this pass changes visible shell copy only.

## 2026-07-08 - Apps AI assistant status wording

- Task: Visual/UX - replace the `apps` output AI Assistant slash-style `launcher/Settings/pkginfo` wording with clearer launcher, Settings, and pkginfo wording while preserving the same consent-scoped action templates and inference-unavailable behavior.
- Commit: `bbfcf8df6180b0a48816ed310af81a6da130f3cf`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Product `apps` listing now says `AI Assistant: launcher, Settings, and pkginfo show consent-scoped action templates; inference unavailable` instead of `AI Assistant: launcher/Settings/pkginfo; consent-scoped action templates; inference unavailable`, keeping the same launcher, Settings, pkginfo, consent-scoped action-template, and inference-unavailable behavior. The M1 Product source assertion and QEMU runtime assertion were updated so both gates prove the revised `apps` output instead of stale copy. This is grounded in Microsoft UI text guidance that users scan interface text and that essential state should be concise without over-communication; verified in QEMU, while physical MSI rendering remains unverified because this pass changes visible shell copy only.

## 2026-07-08 - AI assistant help wording

- Task: Visual/UX - replace the Product help line's slash-style AI Assistant launcher/Settings/pkginfo wording with clearer launcher, Settings, and pkginfo wording while preserving the same consent-scoped action templates and inference-unavailable behavior.
- Commit: `7e7a88e5e720091ec45112dba0fc8c24e08d7e40`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: Product help now says `Product AI assistant: launcher, Settings, and pkginfo show consent-scoped action templates; inference unavailable` instead of `Product AI assistant: launcher/Settings/pkginfo show consent-scoped action templates; inference unavailable`, keeping the same launcher, Settings, pkginfo, consent-scoped action-template, and inference-unavailable behavior. The M1 Product source assertion and QEMU runtime assertion were updated so both gates prove the new user-facing text. This is grounded in Microsoft UI text guidance that users scan interface text and that essential state should be concise without over-communication; verified in QEMU, while physical MSI rendering remains unverified because this pass changes visible shell help copy only.

## 2026-07-08 - Installer UX help wording

- Task: Visual/UX - replace the Product help line's slash-heavy installer wording with clearer launcher/Settings and write/format/boot-entry status text while preserving the same dry-run installer planning behavior and disabled write/format/boot-entry authority.
- Commit: `a54f980c791e7e04c10ad3f3967cbbca16febdb7`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: Product help now says `Product installer UX: launcher and Settings show dry-run planning; writes, formatting, and boot-entry changes disabled` instead of `Product installer UX: launcher/Settings show dry-run planning; writes/format/boot-entry disabled`, keeping the same read-only dry-run installer planning surface, launcher and Settings visibility, and disabled write/format/boot-entry behavior. The M1 Product source assertion and QEMU runtime assertion were updated so both gates prove the new user-facing text. This is grounded in Microsoft UI text guidance that users scan interface text and that essential state should be concise without over-communication; verified in QEMU, while physical MSI rendering remains unverified because this pass changes visible shell help copy only.

## 2026-07-08 - Apps cloud storage status wording

- Task: Visual/UX - replace the `apps` output cloud-storage status line's slash-style `Settings/File Manager; unavailable/planned; no sync` wording with clearer policy-only and sync-unavailable wording while preserving the same cloud broker, Settings/File Manager visibility, and denial behavior.
- Commit: `56080562265752d53b6d4934ee45903aff4fc44a`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Product `apps` listing now says `Cloud storage status: Settings and File Manager; policy only; sync unavailable` instead of `Cloud storage status: Settings/File Manager; unavailable/planned; no sync`, keeping the same read-only cloud broker status surface, Settings and File Manager visibility, cloud sync denial, and Product app list behavior. The M1 Product source assertion and QEMU runtime assertion were updated so both the build gate and the UEFI hardware/display gate prove the new user-facing text rather than stale copy. This is grounded in Microsoft UI text guidance that users scan interface text and that essential state should be concise without over-communication; verified in QEMU, while physical MSI rendering remains unverified because this pass changes visible shell copy only.

## 2026-07-08 - Settings package trust installation wording

- Task: Visual/UX - replace the Settings Package Trust panel's slash-style `Install/apply disabled` line with clearer installation-disabled wording while preserving the same signed-package trust surface and disabled install/apply behavior.
- Commit: `69fe1ae4f863416da613e934df33cf3a89bb6664`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Settings Package Trust panel now says `Installation disabled` instead of `Install/apply disabled`, keeping the same UEFI Ed25519/package-signing status, local fixture index display, auto-install-unavailable line, package install/apply denial telemetry, and read-only Settings behavior. A Product source assertion now checks the GUI wording in `display.c` so the build gate protects this user-facing text as well as the existing package trust telemetry. This is grounded in Microsoft UI text guidance that users scan UI text and that essential state should be concise without over-communication; verified by Product build and the UEFI QEMU hardware/display gate, while physical MSI rendering remains unverified because this pass changes visible copy only.

## 2026-07-08 - Shell package trust help wording

- Task: Visual/UX - replace the Product shell help package-trust line's slash-style install/apply status with clearer installation-disabled wording while preserving the same read-only package trust surface and disabled install/apply behavior.
- Commit: `65c2723484fc78c828b5a674888d83b0f86155b7`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: Product shell help now says `Product package trust: pkginfo and Settings are read-only; installation disabled` instead of `Product package trust: pkginfo and Settings are read-only; install/apply disabled`, keeping the same package trust, pkginfo, Settings, install/apply-denial behavior, and verifier-enforced runtime help checkpoint. The matching M1 source assertion and QEMU help assertion were updated so the gate proves the new user-facing text, not stale copy. This is grounded in Microsoft UI text guidance to keep status text concise and avoid over-communication; verified by Product build and the UEFI QEMU hardware/display gate, while physical MSI rendering remains unverified because this is shell help text rather than a hardware path.

## 2026-07-08 - Shell cloud storage help wording

- Task: Visual/UX - replace the Product shell help cloud-storage line's slash-style sync/upload/download status with clearer sync and transfer wording while keeping the same cloud broker policy and denial behavior.
- Commit: `67c1f0bd1dbfecdee3a68b72900d3444d24c41e2`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Product shell help now says `Product cloud storage: Settings/File Manager show broker policy; sync unavailable; transfers denied` instead of `Product cloud storage: Settings/File Manager show broker policy; sync/upload/download unavailable`, keeping the same cloud broker policy, Settings/File Manager surfaces, and verifier-enforced runtime help checkpoint. The matching M1 source assertion and QEMU help assertion were updated so the gate proves the new user-facing text, not stale copy. This is grounded in Microsoft UI text guidance to keep status text concise and avoid over-communication; verified by Product build and the UEFI QEMU hardware/display gate, while physical MSI rendering remains unverified because this is shell help text rather than a hardware path.

## 2026-07-08 - Settings package trust status wording

- Task: Visual/UX - replace the Settings Package Trust row's slash-style auto-install/public-fetch status with clearer auto-install availability wording while preserving the existing signed-package, local-index, and install/apply disabled behavior.
- Commit: `a40761a7ef99c2233a754068f7c35e2c257aabc3`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Settings Package Trust panel now says `Auto-install unavailable` instead of `No auto-install/public fetch`, keeping the same package signing status, local fixture index, signed-package count, install/apply disabled state, and package trust telemetry. This is grounded in Microsoft UI text guidance to keep interface status text concise and focused on the essential state rather than combining multiple slash-separated implementation labels in one row. Verified by Product build and the UEFI QEMU hardware/display gate; the gate proves the Product GUI/readiness path and package trust surface telemetry, but it does not directly inspect this exact Settings row on the physical MSI, so live MSI rendering remains unverified.

## 2026-07-08 - Settings cloud storage status wording

- Task: Visual/UX - replace the Settings cloud storage row's slash-heavy sync/upload/download status with clearer unavailable/denied wording while preserving the existing cloud broker, transfer denial, and read-only policy behavior.
- Commit: `3e430608c69b6f270625862b2e61bc1e6e7bab59`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Settings cloud storage panel now says `Sync unavailable; transfers denied` instead of `No sync/upload/download; AI denied`, keeping the same `cloud_storage64_init()`, read-only Settings status query, cloud broker foundation state, upload/download/sync denial policy, and telemetry. This is grounded in Microsoft UI text guidance to keep status text concise and focused on the essential state instead of packing several slash-separated implementation labels into one line. Verified by Product build and the UEFI QEMU hardware/display gate; the gate proves the Product GUI/readiness path and Settings hardware/input/readiness telemetry but does not directly open this exact cloud storage row, and physical MSI rendering remains unverified.

## 2026-07-08 - Ambient-authority UI wording

- Task: Visual/UX - replace visible ambient-authority shorthand in the UEFI login badge and Settings AI policy details with clearer user-facing access-state wording while preserving the existing scoped-authority/no-ambient-access behavior.
- Commit: `9e6cdac5c09b5e6bd95395160e211d37bfdb7c94`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The UEFI login panel now says `No file access` instead of `No ambient fs`, and Settings AI policy now says `No automatic system access` instead of `No ambient fs/net/pkg/secret/cloud`. This is grounded in the existing Product authority model and UI writing guidance favoring concise, direct text over implementation shorthand; it changes only visible copy and does not change capability grants, AI policy checks, login flow, filesystem access, package access, secret access, cloud access, or telemetry. Verified by Product build and the UEFI QEMU hardware/display gate; the gate proves the Product GUI/readiness surface but does not visually inspect this exact Settings AI policy row on physical MSI, so MSI rendering remains unverified.

## 2026-07-08 - Settings identity status wording

- Task: Visual/UX - replace the Settings identity row's ambient-authority shorthand with clearer identity-availability wording while preserving the existing local identity and vault metadata behavior.
- Commit: `0bde71ef6dcca39f09f39a98422995e370d171d1`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Settings identity panel now says `Identity unavailable` instead of `No ambient identity/secret`, keeping the same local-active status, vault metadata note, identity panel telemetry, capability policy, and storage behavior. This is grounded in the existing Settings identity surface and UI writing guidance favoring direct, scan-friendly status text over authority-model shorthand. Verified by Product build and the UEFI QEMU hardware/display gate; the default gate reports the Product GUI and Settings hardware/input/readiness surfaces but does not directly open the identity row, and this remains unverified on physical MSI because it is visible text polish only.

## 2026-07-08 - File Manager cloud sync status wording

- Task: Visual/UX - replace the File Manager cloud row's slash-style sync/upload status with clearer availability wording while preserving the existing cloud storage status and read-only policy path.
- Commit: `8ea5c13e4624778b828b248aea61cd5a165e4e62`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The File Manager cloud row now says `Sync unavailable` instead of `No sync/upload`, keeping the same Cloud unavailable state, `cloud_storage64_init()`, read-only cloud status call, File Manager count telemetry, and storage behavior. This is grounded in the existing File Manager cloud surface and UI writing guidance favoring concise, direct status text over slash-style shorthand. Verified by Product build and the UEFI QEMU hardware/display gate; the default gate reports the Product GUI and File Manager storage card but does not open the File Manager cloud row directly, and this remains unverified on physical MSI because it is visible text polish only.

## 2026-07-08 - Assistant generated-reply status wording

- Task: Visual/UX - replace the Assistant panel's implementation-centered generated-response status with clearer user-facing availability wording while preserving the existing Mode B consent and audit behavior.
- Commit: `99211308e51f19b47e5914e75aa7f803f5eb9bb8`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The Assistant panel now says `Generated replies unavailable` instead of `No model call or scripted response`, keeping the same Mode B policy initialization, action probe, consent-scoped templates, denied-capability display, and audit query. This is grounded in the existing Assistant status surface and UI writing guidance favoring crisp, scan-first status text over implementation details. Verified by Product build and the UEFI QEMU hardware/display gate; the default gate reports the Product GUI surface but does not open the Assistant window, and this remains unverified on physical MSI because it is visible text polish only.

## 2026-07-08 - File Manager preview empty-state wording

- Task: Visual/UX - replace the File Manager file-preview empty-state wording with a clearer preview-unavailable message while preserving preview detection and storage behavior.
- Commit: `7e6a521424ec2ead6aa11e44e4ed4513ff52637c`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The File Manager file preview now says `Preview unavailable` instead of `No readable preview bytes` when a selected file has no preview payload to show, keeping the same preview-byte detection, NVMe FAT reads, storage authority, and mutation policy. This is grounded in the existing File Manager preview surface and UI writing guidance favoring concise, glanceable status text. Verified by Product build and the UEFI QEMU hardware/display gate; the default gate reports the File Manager surface but does not select a file with unavailable preview bytes, and this remains unverified on physical MSI because it is visible text polish only.

## 2026-07-08 - File Manager edit status wording

- Task: Visual/UX - replace the File Manager sidebar edit-mode status `Type, Enter` with a clearer path-entry cue while preserving the existing edit-mode and Enter-apply behavior.
- Commit: `e079b687f9de5a1af1606adbac38d1ff66496ed3`
- Build/gate: `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product` passed; `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia uefi -BuildProfile Product -HardwareDisplayGate` passed.
- Summary: The File Manager sidebar now shows `Type path` instead of `Type, Enter` when edit mode is waiting for path input, matching the existing edit preview title and avoiding a terse internal-style instruction. This changes only visible copy: the edit buffer, Enter handling, NVMe FAT authority, mutation denials, and storage behavior are unchanged. Verified by Product build and the UEFI QEMU hardware/display gate; the default gate reports the File Manager surface but does not enter edit mode, and this remains unverified on physical MSI because it is visible text polish only.

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
