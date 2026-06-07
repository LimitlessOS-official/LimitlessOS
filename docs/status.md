# LimitlessOS Status

Last updated: 2026-06-07

## Accepted Baseline

M1 cleanup-final is accepted. The accepted M1 artifact was archived at `dist/m1-evidence-20260511-140840-cleanup-final` with:

- kernel bytes: 468176
- kernel sectors: 915 / 1024
- reserve: 109
- checksum: 0x53122EE5
- runtime help verified: true
- runtime apps verified: true
- persistence verifier printing authority, denial, commit, same-image, and non-RAM evidence

## Current Milestone

M22 is `per-process page table foundation`; the post-M21 real static Linux ELF gate and the M22 per-process CR3 isolation gate are crossed on the UEFI Product path.

M1 cleanup-final through M20 native app execution pipeline are accepted, and M18.1 closed the UEFI real-firmware handoff compatibility boundary. M19 shifted networking from hardware-gated proof to app-facing brokered service: UEFI Product publishes a network service endpoint plus a narrow syscall-level TCP-client socket contract over the existing broker-owned DHCP/DNS/HTTP path. M21 provides the first manifest-driven native app SDK foundation over that service. Product builds read `apps/native/*.json`, assemble app binaries, generate `.APP` descriptors, stage descriptor/binary pairs into `/APPS`, sign package payload records, and load apps by name through a generic descriptor parser before Ring-3 launch. The post-M21 gate adds a separate UEFI-only `linux <path> [args...]` Product shell path that loads one externally built static Linux x86_64 ELF from NVMe FAT storage and executes it through the Linux persona/syscall path with brokered console output. M22 gives each UEFI Product Linux persona launch its own PML4 from a fixed static pool, keeps kernel/MMIO mappings shared, switches CR3 at scheduler start/swap/exit boundaries, and proves the running hardware CR3 matches the process root while the lower-half user page tables are private. M21 native apps are still not Linux/Windows app personas, and the Product surface still does not include a server socket API, raw packet API, arbitrary app network data plane, dynamic Linux linking, fork-style clone, threading, or signal delivery.

Post-M21 acceptance is governed by `docs/real-binary-gate.md`. Synthetic processes, repo-built flat binaries, embedded byte arrays, and denial-only telemetry may remain regression evidence, but they no longer count as forward Product capability evidence. The first execution claim now has real-binary evidence from externally built BusyBox 1.35.0 static musl ET_EXEC artifacts linked at `0x52000000`, staged as `/APPS/BUSYBOX`, loaded from the NVMe FAT image, and proven to print brokered console output with exit code 0. The current BusyBox artifact also proves a minimal `sh` path: ash prints its banner and `$` prompt through brokered console output, consumes verifier-provided shell loops through bounded brokered stdin, and exits cleanly. The M22 acceptance command `linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'` proves that a real externally built static Linux binary now runs with a process-owned root distinct from the kernel root while still using shared higher-half kernel/MMIO mappings.

Proposed next milestone: M23 `trace-driven fork boundary`. M23 should use the next real BusyBox shell path that actually attempts fork, not a synthetic fork fixture, and add only the bounded process duplication needed to move the current `fork-enosys` shell-app boundary forward. Scope should include parent/child PML4 allocation from the M22 pool, VMA ownership/copy semantics, FD/persona/audit duplication, child exit cleanup, and parent wait behavior. M23 should not take on dynamic linking, glibc, real threading, signal delivery, sockets in the Linux ABI table, or broad clone flag compatibility.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifacts:

- BIOS kernel: `KERNEL64-BIOS.BIN`
- BIOS kernel bytes: 472160
- BIOS kernel sectors: 923 / 1024
- BIOS reserve: 101
- BIOS checksum: recorded in the generated artifact inventory
- UEFI kernel: `KERNEL64.BIN`
- UEFI kernel bytes: 1,248,736
- UEFI kernel byte limit: 2,097,152 bytes
- UEFI byte reserve: 848,416
- UEFI checksum: recorded in the generated artifact inventory; it changes when the build-time package signing key is regenerated
- BIOS sector budget status: warning threshold below 128 reserve sectors, hard 1024-sector loader limit still satisfied
- boot contract: split path. BIOS keeps the 1024-sector hard limit and 128-sector warning. UEFI Product uses a 2 MiB `KERNEL64.BIN` file-size contract verified against `BOOTMAN.TXT` byte count and `fnv1a-32` loader checksum, with no UEFI sector arithmetic. `BOOTMAN.TXT` also records `kernel-sha256` for external artifact verification.
- UEFI handoff: fixed low pages are preferences only. The loader verifies/allocates through the UEFI memory map, supports dynamic linked-kernel and handoff-table fallback, and reports allocation name/status/conflict/fallback diagnostics instead of silently freezing.

Product behavior:

- x86_64 boot through disk, UEFI removable media, and UEFI ISO verification
- UEFI Product login gate before desktop/session access
- first-run setup for one local user when `/USERDB.TXT` is missing
- bcrypt `$2b$` password hash persisted in brokered NVMe FAT storage
- `lock` command and Settings lock path return to the login screen and resume the session after unlock
- persistent ring-3 shell
- Product desktop with brokered compositor/window-manager authority
- truthful `help`, `apps`, and `ls apps` output
- brokered DHCP/DNS/TCP/HTTP network status plus M19 restricted TCP-client socket status through the `net` shell builtin when virtio-net or e1000e hardware is present
- Terminal, File Manager, and Settings GUI apps opened through real click interaction
- Installer GUI app opened through the brokered desktop launcher
- Product service lifecycle/status query surface
- UEFI-only brokered network service endpoint and restricted socket syscall surface
- manifest-driven native app descriptor/binary build path and generic UEFI Product native app loader
- UEFI-only real static Linux ELF launch path through `linux <path> [args...]`, currently verified with `/APPS/BUSYBOX echo limitless-real-binary`, `/APPS/BUSYBOX cat /proc/meminfo`, `/APPS/BUSYBOX cat /nvme/apps/data/file.txt`, `/APPS/BUSYBOX ls /nvme/apps`, `/APPS/BUSYBOX ls -l /proc/self/exe`, `/APPS/BUSYBOX ls -l /proc/self/fd`, `/APPS/BUSYBOX ls -l /proc/self`, and `/APPS/BUSYBOX sh`
- UEFI-only per-process page tables for Linux persona launches, backed by a fixed 4-root static PML4 pool with 19 pages per root, private lower-half user/VMA tables, shared higher-half kernel/MMIO mappings, scheduler CR3 switches at task start/swap/exit, and a transitional low-identity compatibility mapping reported as `low-compat 1`
- exactly one authenticated local console session with input, display, filesystem, network-status, restricted socket-status, and installer-dry-run authority scoped to that session
- brokered persistent file workflow
- NVMe persistence verification path
- UEFI-only Ed25519 package archive and payload admission
- UEFI-only signed update-index verification with rollback denial
- read-only package trust visibility through Settings and `pkginfo`
- read-only hardware validation visibility through `hwval`
- read-only installer UX planning through the Installer app, Settings status, `pkginfo`, and verifier telemetry
- read-only identity/vault status through Settings
- read-only signed identity-provider descriptor and transport-safety status through Settings and `pkginfo`
- read-only account association status through Settings and `pkginfo`
- read-only cloud-storage broker and cloud-provider descriptor status through Settings, File Manager, and `pkginfo`
- read-only AI policy request/deny/audit status through Settings and `pkginfo`
- Assistant GUI app with consent-scoped read-only context flow and inference unavailable status
- capability denial checks
- no ambient authority

M10 login/authentication behavior:

- implementation: UEFI Product kernel and service layer only
- BIOS fallback: no login gate; runtime help labels UEFI login/session lock as unavailable
- local user store: `/USERDB.TXT` in the brokered persistent NVMe namespace
- password hash: bcrypt `$2b$` cost 04 using the public-domain `crypt_blowfish` verifier path
- first run: creates one local user record before the login screen
- login screen authority: brokered input plus compositor-owned display only
- pre-auth desktop/terminal/launcher access: blocked
- wrong password: denied visibly
- rate limit: three consecutive failures trigger a displayed 30-second lockout
- session authority: granted only after authentication and scoped to the local console session
- lock/unlock: preserves the session and windows, then resumes after the correct password
- unavailable: multiuser account management, password-change UI, PAM/LDAP, and remote auth

M11 identity/vault behavior:

- implementation: UEFI Product identity/status service and Settings surface; BIOS fallback remains lean and truthfully omits UEFI-only identity services
- active account type: local
- supported account types in the model: local, personal, enterprise
- unavailable account types: personal and enterprise
- local association: active and bound to the authenticated local console session
- cloud account association: planned/unavailable
- cloud storage: planned/unavailable
- security-key login: planned/unavailable
- remote login: unavailable
- secret vault status: foundation metadata only
- encrypted-at-rest secret storage: unavailable/non-product in M11
- real secrets/tokens stored: false
- Settings identity panel: read-only status only
- shell identity command: not added in M11; Settings is the Product identity status surface
- denials: identity mutation without authority, secret read without vault authority, secret write without vault authority, token storage, cloud association, personal login, enterprise login, ambient identity, and ambient secret authority

M12 identity transport behavior:

- implementation: UEFI Product identity transport broker state and Settings/pkginfo read-only surfaces; BIOS fallback remains lean and does not expose UEFI-only identity transport as Product behavior
- mode: Mode B endpoint trust foundation only
- provider descriptor: signed deterministic local fixture, no public internet dependency
- descriptor fields: provider id/type/display name, descriptor/protocol version, fixture endpoint, endpoint key id/fingerprint, supported auth methods, required transport security, token persistence policy, minimum OS version, sequence/generation, trusted-time/expiry metadata, signer key id, and signature
- trusted endpoint status: descriptor-verified fixture endpoint only
- encrypted identity transport: unavailable/non-product
- plaintext credential transport: denied
- credential transport to unverified endpoints: denied
- token storage: denied while vault remains Mode B
- account association: planned/unavailable until M13
- trusted time: unavailable/non-product; descriptor expiry metadata is not Product-enforced without trusted time
- denials: missing signature, invalid signature, wrong key, tamper, rollback, unsupported descriptor version, missing identity-network authority, plaintext credential transport, unverified endpoint credential transport, token storage, personal login, enterprise login, cloud association, ambient network, ambient identity, and ambient secret authority

M13 account association behavior:

- implementation: UEFI Product account association state and Settings/pkginfo read-only surfaces; BIOS fallback remains lean and reports account association unavailable
- mode: Mode B association policy/status only
- association records: local, personal, and enterprise
- local association: active/offline-capable and bound to the authenticated local console user
- personal association: planned/unavailable
- enterprise association: planned/unavailable
- cloud association: planned/unavailable
- security-key login: planned/unavailable
- encrypted identity transport: unavailable/non-product
- credential transport: denied
- token storage: denied while the vault remains Mode B
- enterprise policy: planned/unavailable
- remote account authority: none
- Settings account panel: read-only status only
- shell account status: `pkginfo` read-only text; no mutation commands
- denials: account mutation without authority, account unlink without authority, credential transport, token storage, cloud association, enterprise policy, remote ambient authority, ambient account identity, ambient account network, and ambient account secret authority

M14 cloud-storage broker behavior:

- implementation: UEFI Product cloud-storage broker state and read-only Settings/File Manager/pkginfo surfaces; BIOS fallback remains lean and reports cloud storage unavailable
- broker mode: Product foundation active, policy/status only
- provider descriptor: signed deterministic local fixture, no public cloud dependency
- descriptor fields: provider id/type/display name, descriptor/protocol version, fixture endpoint, endpoint key id/fingerprint, supported modes, token policy, offline-cache policy, sync policy, required transport security, required account association, minimum OS version, sequence/generation, trusted-time/expiry metadata, signer key id, and signature
- cloud account status: unavailable/planned
- cloud association status: unavailable/planned
- token storage: denied while vault remains Mode B
- encrypted cloud transport: unavailable/non-product
- sync: unavailable
- upload/download: denied
- automatic upload/download: unavailable
- offline cache: planned/unavailable
- AI cloud access: unavailable
- File Manager cloud surface: read-only unavailable-status area only; no cloud namespace is exposed
- denials: missing signature, invalid signature, wrong key, tamper, rollback, unsupported descriptor version, malformed descriptor, upload, download, sync, Settings mutation, File Manager mutation, app direct cloud authority, AI cloud access, ambient cloud, ambient filesystem, ambient network, ambient identity, and ambient secret authority

M7.1 package behavior:

- implementation: UEFI Product kernel only; BIOS Product remains checksum-only fallback
- package signing: Ed25519 detached signatures over the generated bootstrap archive and payload records
- trusted key: public verification key embedded in the UEFI Product kernel
- private key: generated during build signing only, not committed, not emitted into Product artifacts, and scanned out of kernel/ISO/image artifacts
- admission checks: signed archive, signed payload, checksum match, trusted signing key, manifest integrity, payload integrity, manifest version, package identity/version policy, scoped install token, owner, stale token, and capability policy
- denial telemetry: missing signature, invalid signature, wrong key, manifest tamper, payload tamper, checksum mismatch, unsupported manifest version, duplicate package, downgrade, wrong owner, stale token, denied capability request, malformed package, oversized package, install without install capability, rollback index, replay handling, and no ambient install/update
- update check: signed local update-index fixture verified through Product telemetry; unsigned, tampered, wrong-key, rollback, and replay fixtures are denied or handled deterministically
- live public update fetching: unavailable/non-product
- auto-install: unavailable
- package-manager GUI: unavailable
- trusted time: unavailable; expiry metadata is not Product-enforced without a trusted time source

M8 package trust UX behavior:

- Settings shows a read-only package trust panel
- `pkginfo` shows package mode, format version, trusted public key id/fingerprint, signed package count, signature/hash status, capability request/admission/denial status, local signed update-index status, rollback/replay handling, no-auto-install status, public update fetch status, trusted-time expiry status, and install/update authority status
- Settings and `pkginfo` do not receive install capability, update-apply capability, filesystem write authority, raw network authority, or raw package mutation authority
- install/apply actions are unavailable in M8
- live public update fetching remains unavailable/non-product
- trusted-time expiry remains unavailable/non-product without a trusted time source

M9 real-hardware validation:

- MSI Cyborg 15 A13VE checklist: `docs/hardware/msi-cyborg-15-a13ve.md`
- status: pending user-supplied physical results
- required boot mode: UEFI USB
- unsafe internal partitions must not be browsed or written
- internal NVMe writes remain disabled unless a later safe installer path explicitly enables them through scoped authority
- Product command: `hwval`
- `hwval` is read-only and reports boot path, framebuffer status, input backends, xHCI, PS/2 fallback, APIC, PCI/ECAM, NVMe, AHCI, network, package trust, installer dry-run status, and disabled install/write/format/NVRAM state
- MSI dry-run parser: `tools\parse-msi-dryrun-evidence.ps1`
- parser output records detected disks, GPT partitions, type GUIDs, labels, filesystem signatures, forbidden partitions, LimitlessOS target candidates, write-disabled status, dry-run no-write status, and recommended next step

M5 installer behavior:

- implementation: `tools\limitless-installer.ps1`
- fixtures: `tools\generate-installer-fixtures.ps1`
- verifier: `tools\verify-installer-m5.ps1`
- safety doc: `docs/installer/m5-safe-installer.md`
- dry-run lists disks/images, GPT partitions, type GUIDs, labels, filesystem signatures, markers, classification, and zero-write plan
- explicit physical dry-run path: `-PhysicalDriveNumber N -AllowPhysicalReadOnly -Mode DryRun`
- install/write mode is image-fixture-only in M5
- Windows ESP, NTFS, MSR, Recovery, unknown FAT32, and unknown GPT targets are refused
- writes require hardware inventory, read-only block, write, and format capability flags plus a confirmation token
- boot-entry changes require a separate boot-entry capability and are not performed by the default M5 path
- real MSI internal NVMe writes remain disabled by default until dry-run output is reviewed

M6 service/session behavior:

- design note: `docs/service-session-m6.md`
- service lifecycle states are modeled as declared, admitted, launching, running, degraded, stopping, stopped, crashed, restarting, and denied
- Product services are policy/security broker, console/shell broker, input broker, display/compositor, window manager/desktop shell, filesystem broker, block/storage broker, hardware inventory broker, network broker, brokered socket foundation, native app SDK foundation, installer dry-run service/tool, settings/system-info provider, cloud-storage broker foundation, and installer UX planning
- Settings is the Product-safe status surface for service/session information
- controlled restart verification is limited to the scoped settings/system-info provider path
- stale capabilities from the old generation are denied
- M6 introduced one local console session; M10 authenticates that single local console user but still does not implement full multiuser account management
- M11 associates the authenticated session with a local identity record and vault metadata foundation without adding remote identity, personal account login, enterprise account login, cloud storage, or token storage
- M12 adds signed local identity-provider descriptor verification and read-only identity transport status without adding remote login, account association, encrypted credential transport, or token persistence
- M13 records local account association as active and labels personal, enterprise, cloud, security-key, credential, token, and enterprise-policy paths unavailable without adding account linking or remote login
- M14 records cloud-storage policy state and labels real cloud storage, sync, upload/download, token storage, encrypted transport, offline cache, AI cloud access, and app-direct cloud authority unavailable or denied
- M15 records installer planning state and labels real internal install/write, formatting, boot-entry changes, package install/apply, cloud enablement, and AI-assisted setup unavailable or denied
- wrong-session input, display, and filesystem delivery are denied
- raw input, direct framebuffer, ambient filesystem, and ambient network access remain denied

Product apps:

- APPEND
- CAT
- COPY
- DELETE
- LS
- MKDIR
- MOVE
- NETHELLO
- RENAME
- STAT
- TOUCH
- WRITE

Product builtins:

- apps
- help
- hwval
- info
- lock (UEFI Product only; BIOS fallback reports unavailable)
- net
- pkginfo
- pwd

Product GUI apps:

- Terminal
- File Manager
- Settings
- Installer
- Assistant

Product GUI authority model:

- raw keyboard and mouse enter only through the brokered input path
- input is routed only to the active local console session
- the window manager owns hit-testing, focus, drag, close, taskbar focus, launcher dispatch, and z-order
- the compositor owns physical framebuffer presentation
- Terminal receives keyboard only while focused, with unfocused terminal delivery denied
- apps draw only through delegated window surfaces in their session
- File Manager is limited to RAMFS, boot-media read-only areas, and explicitly brokered persistent namespaces
- Settings receives read-only display/input/network/storage/profile/boot metadata and cannot write configuration
- GUI apps do not receive ambient framebuffer, raw input, filesystem, storage, or network authority

Unavailable or not product-path in Product:

- ASK, not AI and no consent-gated assistant path
- ECHO
- shell aliases SAY, SHOW, LIST, MAKE, PUT, SWAP, SHIFT
- real internal install/write authority
- formatting authority
- boot-entry authority
- package install/update actions
- app store
- auto-install
- live public update fetch
- general socket library
- server sockets
- raw packet API
- arbitrary app network send/receive
- multiuser account management UI
- password change UI
- PAM/LDAP/remote auth
- AI inference backend
- AI-generated answers
- AI model transport
- personal account login
- enterprise account login
- account linking
- cloud account association
- real public cloud storage
- cloud sync
- automatic cloud upload/download
- offline cloud cache
- AI cloud access
- app-direct cloud authority
- security-key login
- remote login
- encrypted-at-rest secret storage
- token storage
- encrypted identity transport
- credential transport
- enterprise policy enrollment

## Experimental Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental
```

Current Experimental artifact:

- produced by `.\tools\build.ps1 -Architecture x86_64 -BuildProfile Experimental`
- recorded in `dist\limitlessos-x86_64.experimental.m6.json` when the Experimental profile is built
- not Product and not a daily-driver surface

Experimental may initialize proof surfaces beyond the M4 Product GUI and broad hardware proof telemetry. These are not Product behavior and must remain labeled experimental or proof-only in logs and reports.

## Verification

Final M2 evidence pack:

- `dist/m2-evidence-20260511-151717/m2-evidence.json`

The pack records command, exit code, elapsed time, output file, build profile, kernel bytes/sectors/reserve/checksum, final ISO path, and artifact inventory JSON for:

- Experimental build
- Experimental UEFI verification
- Product build
- Product M1/M2 production-slice assertion
- Product disk verification
- Product UEFI verification
- Product ISO verification
- Product UEFI e1000e verification
- Product ISO e1000e verification
- Product NVMe persistence verification

All recorded commands exited 0.

M3 evidence pack:

- `dist/m3-evidence-20260511-163350/m3-evidence.json`
- created by `.\tools\archive-m3-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M3 artifact inventory JSON
- all recorded commands exited 0

M4 evidence pack:

- `dist/m4-evidence-20260511-190105/m4-evidence.json`
- generated by `.\tools\archive-m4-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence commands
- records a dedicated Product GUI interactive verification command
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M4 artifact inventory JSON
- all recorded commands exited 0

M4.1 evidence pack:

- `dist/m4-1-evidence-20260511-192857/m4-1-evidence.json`
- generated by `.\tools\archive-m4-1-evidence.ps1`
- archives the accepted M4 verifier outputs instead of rerunning M4
- archives the MSI Cyborg 15 A13VE checklist template
- records current kernel budget and git status
- records physical validation as pending until the user supplies real-hardware results

M5 installer verification:

- evidence pack: `dist/m5-evidence-20260511-194236/m5-evidence.json`
- command: `.\tools\verify-installer-m5.ps1`
- proves dry-run no-writes
- proves Windows-like ESP/MSR/NTFS/Recovery refusal
- proves unknown FAT32 and unknown GPT refusal
- proves dedicated LimitlessOS target acceptance
- proves scoped write/format capability and confirmation-token requirements
- proves boot-entry authority separation
- proves valid fixture install verifies markers/manifests and leaves forbidden partitions unchanged
- all installer verifier predicates must pass before M5 is accepted

M6 evidence pack:

- generated by `.\tools\archive-m6-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence/gui/installer commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M6 artifact inventory JSON and service/session status output
- records git status and current commit at archive time

M7 evidence pack:

- generated by `.\tools\archive-m7-evidence.ps1 -IncludeExperimental`
- records Product build/assert/disk/UEFI/ISO/e1000e/persistence/gui/installer/service-session/signing commands
- records Experimental build and UEFI verification when `-IncludeExperimental` is supplied
- includes the final M7 artifact inventory JSON, split-kernel budget, package fixture status, private-key artifact scan status, and git status

M7.1 evidence pack:

- generated by `.\tools\archive-m7-1-evidence.ps1 -IncludeExperimental`
- records all preserved M7 commands plus dedicated Product package negative fixture verification, update-index negative fixture verification, and private-key artifact scan
- includes the final M7.1 artifact inventory JSON, split-kernel budget, deterministic fixture status, live public update fetching status, and git status
- live public update fetching remains unavailable/non-product and no auto-install path exists

M8 evidence pack:

- generated by `.\tools\archive-m8-evidence.ps1 -IncludeExperimental`
- records Product package UX/trust-surface verification plus all preserved M7.1 fixture checks
- includes read-only Settings and `pkginfo` package trust surfaces
- install/apply actions, auto-install, public update fetching, and trusted-time expiry remain unavailable

M9 evidence pack:

- generated by `.\tools\archive-m9-evidence.ps1 -IncludeExperimental`
- records Product hardware validation mode, MSI checklist, dry-run parser, and preserved M8 verification matrix
- physical MSI Cyborg 15 A13VE evidence remains pending user-provided results

M10 evidence pack:

- generated by `.\tools\archive-m10-evidence.ps1 -IncludeExperimental`
- records Product login gate, first-run setup, bcrypt `$2b$` user store, wrong-password denial, rate limiting, lock/unlock, and scoped session authority
- BIOS fallback labels login/session lock unavailable

M11 evidence pack:

- generated by `.\tools\archive-m11-evidence.ps1 -IncludeExperimental`
- records Product identity/vault foundation verification plus all preserved M10 verification commands
- proves local account active, personal/enterprise unavailable, Settings identity panel, read-only identity status, identity mutation denial, vault foundation, denied secret read/write, no plaintext token storage, cloud association unavailable, and no ambient identity or secret authority

M12 evidence pack:

- generated by `.\tools\archive-m12-evidence.ps1 -IncludeExperimental`
- records Product identity transport verification plus all preserved M11 verification commands
- proves signed local provider descriptor acceptance, missing/invalid/wrong-key/tampered/rollback/unsupported descriptor denials, scoped network requirement, plaintext credential denial, unverified endpoint denial, token-storage denial while vault is Mode B, Settings identity transport panel visibility, read-only status, trusted-time status, and no ambient identity-transport network/identity/secret authority

M13 evidence pack:

- generated by `.\tools\archive-m13-evidence.ps1 -IncludeExperimental`
- records Product account association verification plus all preserved M12 verification commands
- proves local association active/offline-capable, personal/enterprise/cloud/security-key unavailable, credential transport denied, token storage denied while vault remains Mode B, enterprise policy unavailable, account mutation/unlink denied, remote account no-ambient-authority denial, Settings/pkginfo read-only status, and no ambient account identity/network/secret authority

M14 evidence pack:

- generated by `.\tools\archive-m14-evidence.ps1 -IncludeExperimental`
- records Product cloud-storage broker verification plus all preserved M13 verification commands
- proves signed local cloud-provider descriptor acceptance, missing/invalid/wrong-key/tampered/rollback/unsupported/malformed descriptor denials, Settings/File Manager/pkginfo read-only status, upload/download/sync denial, automatic upload/download unavailability, AI cloud access unavailability, app-direct cloud authority denial, and no ambient cloud/filesystem/network/identity/secret authority

M15 evidence pack:

- generated by `.\tools\archive-m15-evidence.ps1 -IncludeExperimental`
- records Product installer UX verification plus all preserved M14 verification commands
- proves Installer GUI entry, welcome/beginner/advanced/hardware/recommendation/component/account/cloud/AI/plan/dry-run surfaces, zero planned writes/formats/boot-entry/package operations, forbidden target denial, write/format/boot-entry/package/cloud/AI action denial, and no ambient installer/storage/firmware/package/identity-cloud-secret authority

M16 evidence pack:

- generated by `.\tools\archive-m16-evidence.ps1 -IncludeExperimental`
- records Product AI policy verification plus all preserved M15 verification commands
- proves AI principal creation, action request modeling, consent required, no-consent denial, scope validation, invalid-scope denial, immutable/queryable audit visibility, Settings/pkginfo read-only status, zero AI actions executed, zero default AI capabilities, and no ambient AI/filesystem/network/settings/package/secret/cloud authority

M17 evidence pack:

- generated by `.\tools\archive-m17-evidence.ps1 -IncludeExperimental`
- records Product AI Assistant read-only verification plus all preserved M16 verification commands
- proves Assistant app opens after login, pre-auth access is blocked, backend mode is Mode B, default AI capabilities remain zero, context requests require consent, denied requests receive no data, allowed requests receive only scoped read-only status context, invalid/broad filesystem/secret/cloud scopes are denied, file write/settings/package mutation and network/model access are denied or unavailable, stale and wrong-session grants are denied, audit is queryable, Settings shows the AI panel, actions/automation/cloud memory remain unavailable, Assistant self-modification is denied, package integrity is checked, no model call occurs, and no scripted answer is presented as AI output

M18 evidence pack:

- generated by `.\tools\archive-m18-evidence.ps1 -IncludeExperimental`
- records Product AI consent-scoped action verification plus all preserved M17 verification commands
- proves the Mode B action broker creates predefined action requests, requires explicit consent, grants only scoped action-bound/session-bound capabilities, writes and commits `/HOME/ASSIST/NOTE.TXT`, verifies readback, runs installer dry-run with no writes, opens read-only Settings/package status surfaces, denies arbitrary/path-traversal writes, stale grants, wrong-session grants, settings mutation, package install/update, cloud enablement, secret access, self-modification, model calls, generated-answer behavior, autonomy, and ambient AI filesystem/installer/settings/package/cloud/secret/network authority

M18.1 evidence pack:

- generated by `.\tools\archive-m18-1-evidence.ps1 -IncludeExperimental`
- records the UEFI loader compatibility closure plus all preserved M18 verification commands
- proves QEMU UEFI and ISO still reach ExitBootServices, x64 kernel entry, PIT, login shell, and M18 telemetry with dynamic linked-kernel and boot-handoff proof predicates
- records that automated VirtualBox verification is unavailable in this workspace when `VBoxManage` is absent
- includes the manual VirtualBox EFI checklist at `docs/hardware/virtualbox-uefi-m18-1.md`
- keeps MSI physical validation pending user evidence

M19 brokered socket verification:

- generated by `.\tools\verify-network-socket-m19.ps1`
- records Product UEFI network verification with virtio-net by default, or e1000e/e1000 when requested
- proves the network service endpoint is registered, a network service capability is required and minted only through the broker, no-cap and wrong-owner socket opens are denied, raw sockets and listen sockets are denied, send without broker data-plane authority is denied, one TCP-client status handle can be represented over the existing DHCP/DNS/HTTP path, recv-status reports the broker-owned HTTP status/byte count, close retires the handle, no socket remains open after verification, and filesystem/storage/ambient network authority stay zero

M21 native app SDK verification:

- generated by `.\tools\verify-native-app-sdk-m21.ps1`
- reads `apps/native/nethello.json`, assembles the declared native app binary, generates `/APPS/NETHELLO.APP`, stages `/APPS/NETHELLO.BIN`, and signs/verifies payload slot 13 through package metadata
- validates descriptor metadata for `name`, `binary`, `payload-slot`, `entry-result`, `success-result`, and `capabilities`, maps the loaded binary into Ring-3 user memory, and runs it through the native user-entry path
- proves the app can print through delegated console authority, request the declared network service capability, use the M19 brokered socket status path through syscall, observe send denial, close the socket, receive denial for undeclared filesystem/block capabilities, and finish with `drs-app-m21`

Real static Linux ELF gate verification:

- generated by `.\tools\verify-real-binary-gate.ps1 -BusyBoxPath <external-busybox> -BusyBoxSource <source> -BusyBoxVersion <version>`; use `-RequireLowAddressNegative` when the upstream-default-address BusyBox artifact is present and that denial proof must be mandatory, use `-RequireShellCwdLoop` to require BusyBox ash cwd/path traversal proof, use `-RequireRelativePathProof` to require relative `.`/`..` path canonicalization proof, use `-RequireProcSymlinkProof` to require `/proc/self/exe` readlink proof, use `-RequireProcFdProof` to require stable `/proc/self/fd` link proof, use `-RequireProcSelfProof` to require mixed `/proc/self` pseudo-file, directory, and symlink listing proof, and use `-TraceShellForkBoundary` to make the current BusyBox ash external-command fork boundary reproducible without requiring fork support
- stages the external binary into the generated NVMe FAT image at `/APPS/BUSYBOX` without committing or embedding the binary in the repository
- records source, version, SHA-256, byte count, `file`, `readelf -h`, `readelf -l`, staged path, command, console output, exit code, the `/proc/meminfo` cat proof, the `/nvme/apps/data/file.txt` NVMe FAT cat proof, optional `nvme/apps/./data/../data/file.txt` relative cat proof, optional `ls -l nvme/apps/./data/..` relative metadata/listing proof, optional `ls -l /proc/self/exe` proc symlink proof, optional `ls -l /proc/self/fd` proc-fd proof, optional `ls -l /proc/self` mixed proc listing proof, the `/nvme/apps` `ls` proof, the `sh` banner/prompt proof, optional cwd-loop proof, and `drs-realbin` telemetry in `build\real-binary-gate-provenance.txt`
- passing artifact 1: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, SHA-256 `5CDE8968EB2FEDB62DEA27947CD269BC57AD9C8B142ABFF0C3B1514A0238E8D9`; verified with `echo limitless-real-binary`, `cat /proc/meminfo`, and `cat /nvme/apps/data/file.txt`
- passing artifact 2: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-ls`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, and `sh`, SHA-256 `299DE064F51DA04DE99227F26F2EAB60C95F400C1B83731E14E3E28F86695652`; verified with `ls /nvme/apps` and `sh`
- passing artifact 3: `external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh`, BusyBox 1.35.0 static musl ET_EXEC linked at `0x52000000`, configured with `echo`, `cat`, `ls`, `true`, `sh`, `FEATURE_SH_STANDALONE`, and `FEATURE_SH_NOFORK`, SHA-256 `05993EEDEFBA765099A989985B1ED7363E2F2F3BC731F7A92CF4D1BFB6628B69`; verified with the default real-binary gate including `echo`, `cat`, `ls`, and the bounded `sh` builtin loop
- proves the UEFI Product shell commands `linux /APPS/BUSYBOX echo limitless-real-binary`, `linux /APPS/BUSYBOX cat /proc/meminfo`, `linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt`, `linux /APPS/BUSYBOX ls /nvme/apps`, `linux /APPS/BUSYBOX ls -l /proc/self/exe`, `linux /APPS/BUSYBOX ls -l /proc/self/fd`, `linux /APPS/BUSYBOX ls -l /proc/self`, and `linux /APPS/BUSYBOX sh` load the ELF from NVMe, reject `PT_INTERP`/dynamic inputs, map nonzero ELF pages, allocate the 64 KiB real-launch stack, start one scheduler task, print through the brokered console, read a Linux VFS proc file with EOF behavior, read the nested NVMe FAT fixture through the broker-scoped Linux VFS `/nvme` provider, enumerate `/nvme/apps`, `/proc/self/fd`, and mixed `/proc/self` entries through `getdents64`, read proc symlink targets through `readlink(2)`, answer terminal-size probes through `TIOCGWINSZ`, consume bounded brokered stdin for `sh`, observe `exit_group`, and clean up VMA/FD/persona/audit/process state
- proven syscall surface for the passing artifacts: Linux process startup through `brk`, `mmap`, `mprotect`, `munmap`, `arch_prctl`, `set_tid_address`, `rt_sigaction`, and `rt_sigprocmask`; console and file I/O through `read`, `write`, `readv`, `writev`, `openat`, `close`; stat-family metadata through `stat`, `newfstatat` including `AT_EMPTY_PATH`, and `lstat`; proc symlink readback through `readlink`; directory enumeration through `getdents64`; current-working-directory state through `getcwd` and `chdir`; cwd-relative path canonicalization for `open`, `openat`, stat-family paths, and `chdir`, including `.` and `..`; terminal query `ioctl(TIOCGWINSZ)` for brokered terminal fds; fixed identity shims `geteuid` and `getppid`; BusyBox process-name shim `prctl(PR_GET_NAME)`/`prctl(PR_SET_NAME)` with other `prctl` options still returning `ENOSYS`; process termination through `exit_group`; unsupported syscall telemetry remains visible
- positive echo telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 239 stack 16 task 0 started 1 console-bytes 22 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0`
- positive NVMe file telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 239 stack 16 task 0 started 1 console-bytes 27 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 2 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 27`
- positive relative NVMe file telemetry from `-RequireRelativePathProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 27 exit 0 cleanup 1 ... path-relative 1 path-dot 1 path-dotdot 1 path-fault 0 ... read 2 read-bytes 27 write 1 write-bytes 27 ... vfs-nvme-reads 2 vfs-nvme-bytes 27`; visible output was `Nested FAT32 path fixture`
- positive relative NVMe directory metadata telemetry from `-RequireRelativePathProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 65 exit 0 cleanup 1 getdents64 2 getdents64-entries 2 getdents64-bytes 56 stat 3 stat-denial 0 stat-fault 0 ... path-relative 4 path-dot 4 path-dotdot 4 path-fault 0 ... writev 2 writev-bytes 65 ioctl 1 ioctl-tty 1 ... vfs-nvme-readdirs 4 vfs-nvme-dirents 2`; visible output from `ls -l nvme/apps/./data/..` was `-r--r--r--    1    145264 busybox` and `dr-xr-xr-x    2         0 data`
- positive proc symlink telemetry from `-RequireProcSymlinkProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 59 exit 0 cleanup 1 ... stat 1 stat-denial 0 stat-fault 0 ... readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 ... writev 1 writev-bytes 59 ... ioctl 1 ioctl-tty 1 ... unimplemented 0`; visible output from `ls -l /proc/self/exe` was `lrwxrwxrwx    1        14 /proc/self/exe -> /proc/self/exe`
- positive proc-fd telemetry from `-RequireProcFdProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 129 exit 0 cleanup 1 getdents64 2 getdents64-entries 3 getdents64-bytes 72 stat 4 stat-denial 0 stat-fault 0 ... readlink 3 readlink-bytes 33 readlink-denial 0 readlink-fault 0 readlink-last-result 11 ... writev 3 writev-bytes 129 ... unimplemented 0`; visible output from `ls -l /proc/self/fd` was `lrwxrwxrwx    1        11 2 -> anon:[fd 2]`, `lrwxrwxrwx    1        11 1 -> anon:[fd 1]`, and `lrwxrwxrwx    1        11 0 -> anon:[fd 0]`. The bounded proc-fd enumerator skips its own directory iterator fd so it does not publish a link that can disappear before BusyBox's later `readlink` pass.
- positive proc-self telemetry from `-RequireProcSelfProof`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 209 exit 0 cleanup 1 getdents64 2 getdents64-entries 6 getdents64-bytes 168 stat 7 stat-denial 0 stat-fault 0 ... readlink 1 readlink-bytes 14 readlink-denial 0 readlink-fault 0 readlink-last-result 14 ... writev 6 writev-bytes 209 ... unimplemented 0`; visible output from `ls -l /proc/self` was `-r--r--r--    1         0 environ`, `-r--r--r--    1         0 cmdline`, `-r--r--r--    1        70 status`, `dr-xr-xr-x    2         0 fd`, `lrwxrwxrwx    1        14 exe -> /proc/self/exe`, and `-r--r--r--    1       384 maps`.
- positive NVMe directory telemetry: `drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 37 stack 16 task 0 started 1 console-bytes 14 exit 0 cleanup 1 getdents64 2 getdents64-entries 2 getdents64-bytes 56 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 4 vfs-nvme-dirents 2 vfs-nvme-bytes 0`; visible console output was `busybox` and `data`
- positive shell telemetry: `drs-realbin path /APPS/BUSYBOX ... console-bytes 90 exit 0 cleanup 1 ... read 40 read-bytes 40 write 2 write-bytes 20 readv 0 readv-bytes 0 writev 6 writev-bytes 70 ... geteuid 1 getppid 1 ioctl 3 ioctl-tty 3 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00005413 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 ...`; visible console output includes `BusyBox v1.35.0 ... built-in shell (ash)`, `$ shellloop`, `$ $ aftertrue`, and the final `$` prompt
- positive cwd-loop shell telemetry from `-RequireShellCwdLoop`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 101 exit 0 cleanup 1 ... getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 chdir 2 fchdir 0 chdir-denial 0 chdir-fault 0 ... fork 0 fork-enosys 0 fork-denial 0 ...`; visible console output includes `$ shellloop`, `$ /`, `$ $ /nvme/apps`, and `$ $ /`.
- current shell external-command gap: the standalone-shell artifact proves BusyBox's `FEATURE_SH_STANDALONE`/`FEATURE_SH_NOFORK` path for NOFORK applets such as `echo` and `true`, but BusyBox marks `ls` and `cat` as NOEXEC rather than NOFORK. In-shell `ls /nvme/apps` and `cat /nvme/apps/data/file.txt` still attempt fork syscall `57`, report `sh: can't fork: Function not implemented`, and record typed telemetry such as `fork 2 fork-enosys 2 fork-last-rip 0x000000005200EF74`; `tools\verify-real-binary-gate.ps1 -TraceShellForkBoundary` is the reproducible trace mode for this boundary, while `-RequireShellApplets` remains the opt-in verifier mode for future fork/process work and requires in-shell `ls`/`cat` output plus nonzero NVMe/getdents telemetry.
- fork-boundary trace telemetry from `-TraceShellForkBoundary`: `drs-realbin path /APPS/BUSYBOX ... console-bytes 164 exit 2 cleanup 1 ... read 68 read-bytes 68 write 1 write-bytes 10 writev 13 writev-bytes 154 ... geteuid 1 getppid 1 ioctl 3 ioctl-tty 3 ... prctl 2 prctl-set-name 1 prctl-get-name 1 ... fork 2 fork-enosys 2 fork-denial 0 fork-last-rip 0x000000005200EF74 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0`; visible shell output includes `$ shellloop` and two `sh: can't fork: Function not implemented` lines.
- known real-binary gap: terminal support is intentionally narrow. `ioctl(TIOCGWINSZ)` succeeds for brokered terminal fds, while other terminal queries return `ENOTTY` and unrelated ioctl requests return `ENOSYS`; there is still no broad device-control API.
- `drs-realbin` NVMe VFS telemetry is launch-local: in a same-boot run, `linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt` reports `vfs-nvme-reads 2 ... vfs-nvme-bytes 27`, and a following `linux /APPS/BUSYBOX echo limitless-real-binary` reports `vfs-nvme-reads 0 ... vfs-nvme-bytes 0`
- read-stage failures now include concrete FAT read diagnostics; a missing `/APPS/BUSYBOX` reports `stage read code 5 ... nvme-read-error 5 nvme-read-bytes 0 nvme-read-capacity 4194304 nvme-read-size 0 nvme-read-attr 0x0000000000000000`
- negative coverage: missing `/APPS/BUSYBOX`, dynamic `PT_INTERP`, real upstream-default-address ET_EXEC below the low kernel window, oversized host artifact, and BIOS unavailable path
- low-address ET_EXEC negative telemetry with the upstream BusyBox artifact: `drs-realbin-fail path /APPS/BUSYBOX stage static code 20 ... load-first 0x0000000000400000 load-end 0x0000000000713FDD low-kernel-limit 0x0000000001000000`
- BIOS reserve at gate crossing: 101 sectors, below the 128-sector warning threshold but still inside the hard 1024-sector BIOS loader limit
- x86_64 ISO boot shape: UEFI-only optical media with one UEFI El Torito boot entry. `BOOTIMG.IMG` is a staged UEFI FAT image copy for inspection and is not wired as a legacy BIOS El Torito boot entry.
- UEFI boot manifest integrity: `BOOTMAN.TXT` records `kernel-checksum-algorithm=fnv1a-32`, the loader-enforced `kernel-checksum`, standard external `kernel-sha256`, and `non-product-package-registry-stubs=ASK,ECHO`.

M22 per-process page table verification:

- generated by `.\tools\verify-qemu.ps1 -Architecture x86_64 -BootMedia iso -BuildProfile Product -RealBinaryGate -BusyBoxPath .\external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh -BusyBoxSource "local-musl-cross-busybox-1.35.0" -BusyBoxVersion "1.35.0" -ExtraShellLine "linux /APPS/BUSYBOX sh -c 'echo m22-cr3-isolation'"`
- visible console output contains `m22-cr3-isolation`, exits with status 0, reports zero page faults, and cleans up the process PML4 slot
- static pool design: 4 UEFI-only process roots, 19 pages per root, 4 x 19 x 4096 = 311,296 bytes reserved for the PML4/PDPT/PD/PT tables; each root has private lower-half user and VMA page tables plus copied shared higher-half kernel/MMIO mappings
- transitional compatibility status: `low-compat 1` remains visible in telemetry while the low identity PDPT mapping is retained for M22 compatibility; this is not a final isolation claim below the low window
- UEFI reserve delta for the M22 implementation: 852,544 bytes at the first successful pool-stage build to 848,416 bytes at final acceptance; BIOS reserve remains 101 sectors
- proof telemetry:

```text
drs-realbin path /APPS/BUSYBOX provenance 1 nvme-read 1 elf 1 static 1 mapped 4 pages 38 stack 16 pml4 1 pml4-pool 4 pml4-slot 0 root 0x000000000204A000 kernel-root 0x0000000000001000 root-distinct 1 high-copy 1 mmio-shared 1 low-compat 1 user-pdpt-private 1 vma-pt-private 1 cr3-start 1 cr3-exit 1 cr3-syscall-entry 0 active-cr3-match 1 root-cleanup 1 task 0 started 1 console-bytes 18 exit 0 cleanup 1 getdents64 0 getdents64-entries 0 getdents64-bytes 0 stat 0 stat-denial 0 stat-fault 0 fstat 0 fstat-denial 0 fstat-fault 0 newfstatat 0 newfstatat-denial 0 newfstatat-fault 0 readlink 0 readlink-bytes 0 readlink-denial 0 readlink-fault 0 readlink-last-result 0 getcwd 1 getcwd-bytes 2 getcwd-denial 0 getcwd-fault 0 path-relative 0 path-dot 0 path-dotdot 0 path-fault 0 chdir 0 fchdir 0 chdir-denial 0 chdir-fault 0 read 0 read-bytes 0 write 1 write-bytes 18 readv 0 readv-bytes 0 writev 0 writev-bytes 0 poll 0 ppoll 0 poll-ready 0 poll-last-revents 0x00000000 geteuid 1 getppid 1 ioctl 0 ioctl-tty 0 ioctl-enotty 0 ioctl-enosys 0 ioctl-last-request 0x00000000 ioctl-last-result 0x00000000 prctl 2 prctl-set-name 1 prctl-get-name 1 prctl-enosys 0 prctl-last-option 0x0000000F prctl-last-result 0x00000000 fork 0 fork-enosys 0 fork-denial 0 fork-last-rip 0x0000000000000000 vfs-nvme-bind 1 vfs-nvme-release 1 vfs-nvme-reads 0 vfs-nvme-readdirs 0 vfs-nvme-dirents 0 vfs-nvme-bytes 0
drs-realbin-syscall-last number 231 result 0x00000000 unimplemented 0 unimplemented-last 511 unimplemented-rip 0x00000000F05001FF page-faults 0 page-fault-rip 0xFFFFFFFF8001016E
```

## Persistence

Persistence is reboot-surviving in the verifier. `verify-nvme-persistence.ps1` runs two sequential boots against the same NVMe GPT image, observes content written in the first boot from the second boot, and prints:

- scoped write authority required
- commit authority required
- wrong-owner denial observed
- stale/revoked denial observed
- read-only write denial observed
- nonzero commit/flush counter
- same NVMe image reused
- not RAM-backed
- exit code 0

## Remaining Daily-Driver Blockers

- Product BIOS reserve is currently 101 sectors after rebuilding the present source, below the 128-sector warning threshold but still inside the hard 1024-sector loader limit.
- Product UEFI now uses the 2 MiB file-size contract; BIOS remains on the 1024-sector loader contract.
- UEFI remains governed by `KERNEL64.BIN` byte budget, manifest/checksum correctness, placement/load correctness, and artifact inventory correctness. UEFI is not blocked by the BIOS 1024-sector ceiling.
- GUI/window manager/desktop are Product for Terminal, File Manager, Settings, Installer, brokered input, focused keyboard routing, and compositor-owned display.
- Network now has an M19 brokered TCP-client socket foundation over the hardware-gated DHCP/DNS/HTTP path, and M21 packages one manifest-backed native app that consumes it from Ring 3 through generic descriptor validation. There is still no general socket library, server socket API, raw packet API, arbitrary app send/receive data plane, or ambient network authority.
- Real static Linux ELF execution now works on the UEFI Product path for one externally built static ET_EXEC binary loaded from NVMe FAT and linked into the supported user VMA window. M22 adds per-process page tables for UEFI Product Linux persona launches with shared kernel/MMIO mappings and scheduler CR3 switching. Dynamic Linux ELF, glibc dynamic linking, fork-style clone, real threading, signal delivery, sockets in the Linux ABI table, and broad distro compatibility remain unavailable.
- Installer UX planning and dry-run are Product, but real internal install/write, formatting, and boot-entry authority remain unavailable.
- M7.1 signed package admission has separate negative fixture coverage, but package-manager UI, app store, auto-install, live public update fetching, and trusted-time expiry enforcement remain unavailable.
- M6 has a local console session model, not full multiuser login/authentication.
- M11 has a local identity model and vault foundation only; personal login, enterprise login, cloud association, cloud storage, encrypted secret storage, and token storage remain unavailable.
- M12 has an identity transport foundation only; encrypted account transport, credential transport, token persistence, remote login, and trusted-time expiry enforcement remain unavailable.
- M13 has an account association status foundation only; local association is active, but personal/enterprise/cloud association, account linking, security-key login, enterprise policy, token persistence, and remote account authority remain unavailable.
- M14 has a cloud-storage broker foundation only; real public cloud storage, sync, automatic upload/download, offline cache, token storage, encrypted cloud transport, AI cloud access, and app-direct cloud authority remain unavailable or denied.
- M15 has installer UX planning only; destructive install actions remain disabled by default and not Product-approved.
- Assistant remains Mode B with read-only context flow and predefined consent-scoped action templates only. Inference backend, model transport, generated answers, broad automation, autonomous action, package install/update, settings mutation, cloud AI, cloud memory, and internal install/write remain unavailable.
- Hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
