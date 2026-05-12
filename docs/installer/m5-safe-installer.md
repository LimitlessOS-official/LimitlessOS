# M5 Safe Installer + Partition Protection

Status: implemented as a host-side, raw-image verified safety path. No installer code was added to the BIOS-constrained Product kernel.

M5 is a safety milestone, not a UI milestone. The installer tooling exists to prove partition classification, explicit authority, confirmation, and write fencing before any MSI Cyborg 15 A13VE internal install is product-approved.

## Product Boundary

- M4 Product GUI remains enabled.
- Terminal, File Manager, and Settings remain Product GUI apps.
- No M5 installer code is compiled into the BIOS-constrained kernel while BIOS reserve remains below 128 sectors.
- Internal NVMe writes remain disabled by default on real hardware.
- Real MSI Cyborg 15 A13VE write/install is not product-approved until dry-run output is reviewed.

## Boot Contract

- BIOS remains capped at 1024 sectors, with hard failure below 96 reserve and warning below 128 reserve.
- Current BIOS reserve is 99 sectors, so installer code must stay out of the BIOS-constrained Product kernel.
- UEFI remains governed by `KERNEL64.BIN` byte budget, `BOOTMAN.TXT` byte/checksum correctness, placement/load correctness, and artifact inventory correctness.
- UEFI is not blocked by the BIOS 1024-sector ceiling.

## Capabilities

The M5 installer tool models the required authority explicitly:

- Disk enumeration requires `-GrantHardwareInventoryCapability`.
- GPT/partition reads require `-GrantReadOnlyBlockCapability`.
- Writes require `-GrantWriteCapability`.
- Formatting/target preparation requires `-GrantFormatCapability`.
- Boot-entry changes require `-GrantBootEntryCapability`.
- Physical disks may only be opened in dry-run mode with `-PhysicalDriveNumber N -AllowPhysicalReadOnly`.

Missing authority fails before writes. Confirmation failure fails before writes. Boot-entry requests without boot-entry authority fail before writes.

## Dry-Run

Dry-run lists:

- detected disk/image
- GPT partitions
- partition type GUIDs
- partition labels
- filesystem signatures and labels when available
- LimitlessOS marker presence
- safe, forbidden, or unknown classification
- exact zero-write plan

Dry-run must preserve the input image byte-for-byte.

Example image dry-run:

```powershell
.\tools\limitless-installer.ps1 `
  -ImagePath .\dist\m5-installer-fixtures\windows-like.img `
  -Mode DryRun `
  -GrantHardwareInventoryCapability `
  -GrantReadOnlyBlockCapability
```

Example explicit physical read-only dry-run:

```powershell
.\tools\limitless-installer.ps1 `
  -PhysicalDriveNumber 0 `
  -AllowPhysicalReadOnly `
  -Mode DryRun `
  -GrantHardwareInventoryCapability `
  -GrantReadOnlyBlockCapability
```

Do not run physical install mode in M5. It is deliberately refused.

## Forbidden Targets

The installer refuses:

- Windows EFI System Partition or unknown ESP
- NTFS partitions
- Microsoft Reserved partitions
- Windows Recovery partitions
- unknown internal FAT32 partitions
- unknown GPT partitions
- any partition without a LimitlessOS marker, approved label, or dedicated LimitlessOS GPT type when write mode is requested

## Accepted Targets

Write mode is accepted only for dedicated LimitlessOS targets, currently recognized by:

- LimitlessOS root/data GPT type GUID: `4c4f534f-5349-4d49-944c-494d49544c01`
- LimitlessOS boot GPT type GUID: `4c4f534f-5349-4d49-944c-494d49544c02`
- exact approved labels: `LIMITLESS-BOOT`, `LIMITLESS-ROOT`, `LIMITLESSOS TARGET`
- `LIMITLESSOS_TARGET_V1` marker

The preferred real-hardware path is still: create unallocated space in Windows first, review dry-run output, then create dedicated LimitlessOS targets through a later explicitly approved installer path. M5 does not shrink Windows partitions.

## Safe 50 GB Layout Proposal

For the MSI Cyborg 15 A13VE spare 50 GB target, M5 proposes:

- a dedicated LimitlessOS boot partition, separate from the Windows ESP
- a dedicated LimitlessOS root/data partition
- clear labels: `LIMITLESS-BOOT` and `LIMITLESS-ROOT`
- LimitlessOS magic markers on both partitions
- no modification of existing Windows partitions
- no dependency on the Windows ESP
- no Windows Boot Manager modification
- no NVRAM boot entry unless explicitly requested and separately authorized

The default boot strategy is USB booting LimitlessOS while using the internal LimitlessOS partition as root/data. A dedicated internal LimitlessOS ESP and optional NVRAM boot entry remain separate, explicit-authority operations.

## Two-Phase Commit

The write path uses:

- plan generation
- explicit confirmation token
- write phase
- verification phase
- forbidden-partition unchanged checks
- audit records for each write

The confirmation token format is:

```text
INSTALL-LIMITLESSOS-M5:<boot-partition-number>/<root-partition-number>
```

## Verification

Run:

```powershell
.\tools\verify-installer-m5.ps1
```

The verifier generates fixtures for:

- clean GPT disk with unallocated target
- Windows-like ESP/MSR/NTFS/Recovery disk
- unknown FAT32 partition
- unknown GPT partition
- valid disk with Windows-like forbidden partitions plus dedicated LimitlessOS boot/root targets

It proves:

- dry-run performs no writes
- forbidden partitions are detected and refused
- unknown partitions are refused
- dedicated LimitlessOS targets are accepted
- write requires scoped write capability
- boot-entry modification requires separate explicit authority
- failed confirmation prevents writes
- successful fixture install verifies boot/root markers and manifests
- forbidden partitions remain unchanged
- no ambient authority is introduced
