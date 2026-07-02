# MSI Cyborg 15 A13VE Manual Validation

Status: M169 current handoff bundle ready; June 2026 photos show UEFI Product shell reachability with display, touchpad, and NVMe FAT gaps still open, and the latest July 2026 physical report shows a boot freeze after the visible `PIC MASK` marker on the MSI laptop. The current handoff ISO adds UEFI-only boot-stage markers after `PIC MASK` so the next photo/transcript can identify the exact first failing stage. Stale captures missing the M163 controller snapshot still route to `nvme-controller-snapshot-missing` instead of being accepted as storage evidence.

This checklist is for a real UEFI USB boot of the current handoff ISO at `dist\m133-msi-hardware-handoff-current\limitlessos-x86_64-m133-handoff.iso` on an MSI Cyborg 15 A13VE. QEMU/QMP evidence is useful, but it is not a substitute for this checklist.

Post-M21 hardware progress must be based on real device output. Synthetic process tests and QEMU-only driver evidence do not count as MSI laptop network, storage, or daily-driver validation.

## June 2026 Hardware Snapshot

User-provided laptop photos from a real boot show the kernel reaches the persistent `[x64] $` shell and accepts keyboard input. The observed `linux /APPS/DYNLDLIMIT` result is:

```text
linux: NVMe FAT unavailable
drs-realbin-unavailable bios 0 nvme 0
```

Interpretation: this was the UEFI kernel branch (`bios 0`), not the BIOS checksum fallback, from a pre-boot-media-handoff hardware run. Current staged UEFI images should choose the boot-media source for `/APPS/DYNLDLIMIT` before requiring NVMe FAT and print `linux: using UEFI boot-media staged file` when the UEFI loader copied the staged app into `boot_info`.

Known open hardware gaps from the photos:

- Display reaches GOP framebuffer output, but console/window layout is visibly mis-scaled or overlapped on the laptop panel. New UEFI-only `hwval` fields now report framebuffer pitch, format, base, and byte size to diagnose this.
- Keyboard input works through the brokered shell. The shell waiting for a key is expected; seeded startup command replay is intentionally gone.
- Touchpad/mouse does not move. Diagnostics show the PS/2 keyboard path is alive, PS/2 aux mouse is not producing packets, and the LPSS/I2C touch path reports an error. Capture `i2c pointer found`, `i2c pointer reports`, `i2c pointer error`, `i2c pointer candidates`, and `i2c pointer0 flags/base` from `hwval`.
- `linux /APPS/DYNLDLIMIT` now has a UEFI boot-media staged-file fallback for the app and interpreter copied by the UEFI loader. NVMe FAT is still needed for Linux VFS file tests, `/nvme/apps` paths, and staged-artifact agreement checks, but the initial dynamic app source no longer has to come from NVMe.

Next hardware evidence to capture with the current M169-verified handoff bundle:

```text
hwval
linux /APPS/DYNLDLIMIT
```

If the laptop freezes before `hwval` can run, capture a clear photo of the last visible stage marker instead. The M169 diagnostic image adds these UEFI-only markers after the old `PIC MASK` line:

```text
[x64] irq route apic <0|1> pic-disabled <0|1> timer-gsi <n> keyboard-gsi <n> mouse-gsi <n>
PIT INIT
PIT OK
SYSCALL INIT
SYSCALL OK
INT PROBES
INT PROBES OK
```

Interpretation for the next physical run:

- Last visible marker is `PIC MASK` with no IRQ route line: the failure is between the old marker and the first diagnostic print.
- Last visible marker is `PIT INIT`: inspect PIT port programming and legacy timer handoff.
- Last visible marker is `PIT OK` or `SYSCALL INIT`: inspect syscall MSR/descriptor initialization next.
- Last visible marker is `SYSCALL OK` or `INT PROBES`: inspect the controlled exception/probe path.
- Last visible marker is `INT PROBES OK` and the next stall is near `TIMER WAIT`: inspect APIC/IOAPIC/PIT timer interrupt routing.
- If it reaches `KEYBOARD WAIT`, `MOUSE WAIT`, or the Product shell, the previous PIC/PIT cliff is cleared and the cursor/display/input issues should be classified through `hwval`.

The missing mouse cursor is not yet the primary failure if the system is still frozen at the PIC/PIT/syscall/probe stage: the compositor cursor path has not necessarily started. Once the boot reaches the Product shell or `hwval`, capture `drs-cursor-path`, `drs-gui`, USB/PS2 packet counters, and pointer location fields to classify cursor drawing separately from raw pointer packet movement.

Record the full `drs-display-readability`, `drs-ui-polish`, `drs-cursor-path`, `drs-gui`, and `drs-nvme-triage` lines from `hwval`, plus the full `drs-realbin` or `drs-realbin-fail` line from `linux /APPS/DYNLDLIMIT`. The `drs-nvme-triage` line and generated report must include the M161/M162 `NVMe Controller Snapshot` fields: `nvme-probe-error`, `nvme-regs`, `nvme-cap-low`, `nvme-cap-high`, `nvme-vs`, `nvme-cc`, `nvme-csts`, `nvme-dstrd-bytes`, and `nvme-doorbell-page`. If only `drs-realbin-unavailable` appears, the capture is legacy/insufficient and should be repeated with the current staged image. If `drs-gui` is missing or the controller snapshot is missing, the capture is also insufficient for the current MSI handoff and should be repeated with an M163-or-newer Product image. The report now exposes missing controller fields explicitly and uses the stage `nvme-controller-snapshot-missing` for that stale-transcript case.

The current M169 handoff bundle self-verifies with ISO SHA-256 `69e95ab98684eb84112497de603ed9160a4563a0db20b54bddbf42d2f380eec6`, UEFI image SHA-256 `b2cdf962eed8b831d71f590ed646dc4794959211cbc88d5b74ac47db063d13e0`, BIOS reserve `101` sectors, and UEFI reserve `721,280` bytes.

Before writing the USB stick, generate the current capture session handoff from the repository root:

```powershell
.\tools\start-msi-hardware-capture.ps1
```

This verifies the current handoff bundle, writes `dist\msi-hardware-capture-session\msi-hardware-capture-session.txt` with the exact ISO path, hashes, laptop commands, required telemetry lines, capture path, and report command, and writes a blank transcript template at `dist\msi-hardware-capture-session\msi-hwval-storage.template.txt`. It does not create hardware evidence; it only makes the physical run harder to mis-execute.

For hardware builds that need dynamic-linker artifacts available on the USB boot image itself, the x86_64 Product build can now stage two externally built files into the UEFI FAT boot image:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product `
  -BootLinuxAppPath <path-to-DYNLDLIMIT> -BootLinuxAppName DYNLDLIMIT `
  -BootLinuxInterpPath <path-to-LDLIMIT> -BootLinuxInterpName LDLIMIT
```

This creates `/APPS/DYNLDLIMIT` and `/APPS/LDLIMIT` inside the UEFI FAT boot image. The UEFI loader reads those files before `ExitBootServices`, copies them into low mapped handoff pages, records their base/size/token in `boot_info`, and `BOOTMAN.TXT` records expected paths, byte counts, and SHA-256 hashes. In `hwval`, capture `boot media linux staged`, `boot media app bytes`, `boot media interp bytes`, `boot media flags`, `boot media status`, and the full `drs-nvme-triage` line. When this staging is healthy, `linux /APPS/DYNLDLIMIT` should report `source 2` and `boot-media-read 1` in the `drs-realbin` telemetry even if NVMe FAT is unavailable.

Host-side verifier for the boot-media handoff path:

```powershell
.\tools\verify-boot-media-linux-handoff.ps1
```

The verifier intentionally stages tiny invalid ELF-shaped probe payloads. Passing output proves UEFI FAT staging, `boot_info` handoff, shell source selection, and boot-media read telemetry; it does not claim the real dynamic binary completed.

Analyze the evidence bundle and captured transcript from Windows/PowerShell with the M155 capture report first:

```powershell
.\tools\report-msi-hardware-capture.ps1 `
  -EvidenceDir .\dist\m133-msi-hardware-handoff-current `
  -CapturePath .\dist\msi-hwval-storage.txt `
  -OutputDir .\dist\msi-hardware-capture-report `
  -RequireStagedDynamicArtifacts `
  -RequireGuiInteractionTelemetry
```

Start with `msi-hardware-capture-report.md` or `msi-hardware-capture-report.txt`. The report records the target kind, target stage, roadmap target, next target, bundle hashes, reserves, storage/display-input/dynamic-handoff status, and links to the lower-level JSON evidence. A nonzero report exit code can still be the correct result: exit `2` means the capture produced an actionable first-failure stage rather than a clean pass.

The lower-level M134 storage target classifier remains available when you need the raw classification output directly:

```powershell
.\tools\classify-m134-storage-target.ps1 `
  -EvidenceDir .\dist\m133-msi-hardware-handoff-<timestamp> `
  -CapturePath .\dist\msi-hwval-storage.txt `
  -OutputDir .\dist\m134-msi-storage-target `
  -RequireStagedDynamicArtifacts `
  -RequireGuiInteractionTelemetry
```

Use `target-kind`, `target-stage`, and `next-target` as the next implementation target. If `target-kind` is `storage`, continue M134-M140 storage work on that exact storage stage. If it is `display-input` or `dynamic-handoff`, do not guess at NVMe; follow the reported roadmap target instead.

The current report and classifier commands require the `hwval` GUI interaction line. A stale transcript that proves storage/display/dynamic handoff but lacks `drs-gui` reports:

```text
target-kind: display-input
target-stage: gui-telemetry-missing
roadmap-target: M152
```

The older combined M118 intake command remains useful when you need the full storage/display/input breakdown:

```powershell
.\tools\analyze-msi-hardware-capture.ps1 `
  -EvidenceDir .\dist\m133-msi-hardware-handoff-<timestamp> `
  -CapturePath .\dist\msi-hwval-storage.txt `
  -OutputDir .\dist\msi-hardware-analysis `
  -RequireStagedDynamicArtifacts
```

The combined analyzer checks the evidence bundle hashes/reserves first, runs the storage analyzer, runs the display/input analyzer, and writes `msi-hardware-analysis.json`, `.txt`, and `.md`. Use the top-level `stage` and `next-target` as the next implementation target. If storage fails first, the stage names the first failing controller/GPT/FAT/capability/`/APPS`/staged-artifact dependency. If storage passes and display/input fails, the stage names the first framebuffer/compositor/cursor/pointer backend dependency.

Before a physical capture handoff, `tools\verify-msi-hardware-analysis-fixtures.ps1` can be run as a host-side regression check for the combined analyzer ordering.

The M133 handoff verifier also has fixture coverage:

```powershell
.\tools\verify-msi-hardware-handoff-fixtures.ps1
```

This proves stale storage-only packages, old M113 ISO names, missing source-2 requirements, and incomplete runbooks are rejected before a physical tester spends time on them.

Before writing the USB stick, verify the current handoff bundle itself:

```powershell
.\tools\verify-msi-hardware-handoff.ps1 `
  -EvidenceDir .\dist\m133-msi-hardware-handoff-<timestamp> `
  -RequireStagedDynamicArtifacts `
  -RequireGuiInteractionTelemetry
```

This checks the bundle hashes/reserves, the M133 media contract, the current M153 runbook contract, the `hwval` plus `linux /APPS/DYNLDLIMIT` instructions, the required `drs-gui` expectation, and the source-2 boot-media fallback expectation. After a physical capture exists, add `-CapturePath <path-to-transcript>` to run the same combined analyzer against the real laptop transcript.

The preferred post-capture command is now the M168 finish wrapper:

```powershell
.\tools\finish-msi-hardware-capture.ps1
```

It uses `dist\m133-msi-hardware-handoff-current` and `dist\msi-hwval-storage.txt` by default, rejects the blank transcript template, runs the strict report wrapper, writes `dist\msi-hardware-capture-report\msi-hardware-capture-next-target.txt`, and prints the target kind/stage/roadmap/next-target summary. Use `msi-hardware-capture-report.md` as the handoff summary. It delegates to the stricter verifier/classifier path; it does not replace the raw transcript, photos/video, or lower-level JSON artifacts.

When a capture is supplied, the verifier also classifies the dynamic command itself. Use `dynamic-handoff-stage` as the dynamic launch target:

- `dynamic-runtime-*`: boot-media source 2 worked; the next target is the named dynamic runtime stage.
- `dynamic-runtime-exit0`: boot-media source 2 worked and the dynamic binary exited cleanly.
- `dynamic-handoff-nvme-unavailable`: the laptop still followed the old NVMe-unavailable path.
- `dynamic-handoff-wrong-source`: `drs-realbin` appeared, but not from source 2.
- `dynamic-handoff-missing-realbin`: the transcript did not capture usable `drs-realbin` telemetry for `/APPS/DYNLDLIMIT`.
- `dynamic-handoff-boot-media-read`: source 2 was selected but boot-media read success was not proven.

M133 packages this handoff into a timestamped evidence directory:

```powershell
.\tools\prepare-hardware-storage-evidence.ps1
```

The bundle contains the staged ISO, UEFI image, `BOOTMAN.TXT`, size map, `DYNLDLIMIT`, `LDLIMIT`, manifest hashes, `README-HARDWARE-STORAGE.txt`, and an embedded `msi-handoff-verification` result proving the package still matches the current source-2 handoff and M153 GUI telemetry contract. The accepted bundle directory shape remains:

```text
dist\m133-msi-hardware-handoff-<timestamp>
```

For the next physical run, write `limitlessos-x86_64-m133-handoff.iso` from that bundle to USB, boot through the UEFI USB entry, run `hwval`, run `linux /APPS/DYNLDLIMIT`, save the full transcript, then classify it with:

```powershell
.\tools\report-msi-hardware-capture.ps1 `
  -EvidenceDir .\dist\m133-msi-hardware-handoff-current `
  -CapturePath .\dist\msi-hwval-storage.txt `
  -OutputDir .\dist\msi-hardware-capture-report `
  -RequireStagedDynamicArtifacts `
  -RequireGuiInteractionTelemetry
```

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
- [ ] Save the complete `hwval` transcript, including `drs-display-readability`, `drs-ui-polish`, `drs-cursor-path`, `drs-gui`, and `drs-nvme-triage`.
- [ ] Classify the evidence bundle and transcript with `tools\classify-m134-storage-target.ps1 -RequireStagedDynamicArtifacts -RequireGuiInteractionTelemetry`.
- [ ] If a full breakdown is needed, analyze the evidence bundle and transcript with `tools\analyze-msi-hardware-capture.ps1`.
- [ ] If parsing fails at `legacy-realbin-unavailable`, repeat `hwval` using an M111-or-newer staged image.
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
- Parsed storage evidence filename:
- Photos/video filenames:
- Tester:
- Date/time:
- Notes:

## Pass Criteria

M18.1 hardware validation passes only when the checklist above is completed with a real UEFI handoff into the x64 kernel, working login/lock behavior, no unsafe partition access, no untruthful Product surface, no ambient authority exception, and no internal install/write/format/NVRAM action. If any item fails, record the exact failing step and keep real internal install blocked.
