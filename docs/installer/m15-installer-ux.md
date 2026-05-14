# M15 Installer UX v2

M15 promotes installer planning into a Product GUI surface without enabling real internal installation.

## Product Behavior

- The desktop launcher includes an Installer app.
- The Installer app opens through the brokered window manager and compositor.
- Settings and `pkginfo` expose read-only installer planning status.
- The UI presents welcome, beginner, advanced, hardware summary, recommendation, component selection, account, cloud, AI, install-plan, and dry-run validation sections.
- The selected fixture profile is `General use`.
- The generated install plan records zero writes, zero formats, zero boot-entry changes, zero package operations, and real install approval false.

## Unavailable In M15

- Real internal install/write.
- Formatting.
- NVRAM boot-entry changes.
- Windows ESP, NTFS, MSR, Recovery, unknown FAT/FAT32, and unknown GPT writes.
- Personal account setup.
- Enterprise account setup.
- Security-key login setup.
- Cloud storage sync or enablement.
- AI-assisted setup. M16 adds request/deny/audit policy only; it still does not add AI setup actions.
- Package install/apply UX.
- App store behavior.
- Browser, gaming stack, and developer toolchain installation.

## Authority Boundary

- Installer UI receives read-only hardware/status authority.
- Installer UI receives read-only package/component authority.
- Installer UI receives read-only account/cloud/AI status authority.
- Installer dry-run receives read-only partition inspection authority.
- Installer plan builder receives no write authority.
- Destructive write, format, firmware boot-entry, package-install, cloud-enable, and AI-enable actions remain denied.

M15 does not modify Windows partitions, does not write internal NVMe by default, does not format anything, and does not alter NVRAM boot entries.

## Verification

The M15 verifier requires:

- `drs-installer-ux-product 1`
- `drs-installer-welcome 1`
- `drs-installer-beginner-mode 1`
- `drs-installer-advanced-mode 1`
- `drs-installer-hardware-summary 1`
- `drs-installer-recommendation 1`
- `drs-installer-component-selection 1`
- `drs-installer-unavailable-components-labeled 1`
- `drs-installer-account-page 1`
- `drs-installer-personal-unavailable 1`
- `drs-installer-enterprise-unavailable 1`
- `drs-installer-cloud-page 1`
- `drs-installer-cloud-sync-unavailable 1`
- `drs-installer-ai-page 1`
- `drs-installer-ai-setup-unavailable 1`
- `drs-installer-plan-generated 1`
- `drs-installer-dryrun-no-writes 1`
- `drs-installer-forbidden-target-denied 1`
- `drs-installer-write-action-denied 1`
- `drs-installer-format-action-denied 1`
- `drs-installer-boot-entry-denied 1`
- `drs-installer-package-install-denied 1`
- `drs-installer-cloud-enable-denied 1`
- `drs-installer-ai-enable-denied 1`
- `drs-no-ambient-installer 1`
- `drs-no-ambient-installer-storage 1`
- `drs-no-ambient-installer-firmware 1`
- `drs-no-ambient-installer-package 1`
- `drs-no-ambient-installer-identity-cloud-secret 1`

Archive M15 evidence with:

```powershell
.\tools\archive-m15-evidence.ps1 -IncludeExperimental
```

M16 is the next accepted planning dependency for future AI setup safety, but M15 itself remains a zero-write installer planning milestone.
