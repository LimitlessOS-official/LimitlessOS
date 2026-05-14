# LimitlessOS Status

Last updated: 2026-05-14

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

M18.1 is `UEFI real-firmware handoff compatibility`.

M18.1 preserves M18 Product behavior and fixes the UEFI loader compatibility boundary. The loader no longer assumes fixed low physical pages are available for linked-kernel placement, page-root/handoff tables, boot-info, or trampoline. It checks firmware memory-map ownership, falls back to conventional-memory allocations, records actual handoff addresses, and stops with an explicit fail-fast diagnostic if no safe handoff can be built. M18.1 does not add post-M18 features.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifacts:

- BIOS kernel: `KERNEL64-BIOS.BIN`
- BIOS kernel bytes: 458400
- BIOS kernel sectors: 896 / 1024
- BIOS reserve: 128
- BIOS checksum: recorded in the generated artifact inventory
- UEFI kernel: `KERNEL64.BIN`
- UEFI kernel bytes: 587904
- UEFI kernel byte limit: 2,097,152 bytes
- UEFI byte reserve: 1,509,248
- UEFI checksum: recorded in the generated artifact inventory; it changes when the build-time package signing key is regenerated
- BIOS sector budget status: ok
- boot contract: split path. BIOS keeps the 1024-sector hard limit and 128-sector warning. UEFI Product uses a 2 MiB `KERNEL64.BIN` file-size contract verified against `BOOTMAN.TXT` byte count and checksum, with no UEFI sector arithmetic.
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
- brokered DHCP/DNS/TCP/HTTP network status through the `net` shell builtin when virtio-net or e1000e hardware is present
- Terminal, File Manager, and Settings GUI apps opened through real click interaction
- Installer GUI app opened through the brokered desktop launcher
- Product service lifecycle/status query surface
- exactly one authenticated local console session with input, display, filesystem, network-status, and installer-dry-run authority scoped to that session
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
- Product services are policy/security broker, console/shell broker, input broker, display/compositor, window manager/desktop shell, filesystem broker, block/storage broker, hardware inventory broker, network broker, installer dry-run service/tool, settings/system-info provider, cloud-storage broker foundation, and installer UX planning
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

- Product BIOS reserve is back above the 128-sector warning threshold after the BIOS/UEFI kernel split.
- Product UEFI now uses the 2 MiB file-size contract; BIOS remains on the 1024-sector loader contract.
- UEFI remains governed by `KERNEL64.BIN` byte budget, manifest/checksum correctness, placement/load correctness, and artifact inventory correctness. UEFI is not blocked by the BIOS 1024-sector ceiling.
- GUI/window manager/desktop are Product for Terminal, File Manager, Settings, Installer, brokered input, focused keyboard routing, and compositor-owned display.
- Network is Product only as a brokered hardware-gated status path. There is no socket API, packet API, or ambient network authority.
- Installer UX planning and dry-run are Product, but real internal install/write, formatting, and boot-entry authority remain unavailable.
- M7.1 signed package admission has separate negative fixture coverage, but package-manager UI, app store, auto-install, live public update fetching, and trusted-time expiry enforcement remain unavailable.
- M6 has a local console session model, not full multiuser login/authentication.
- M11 has a local identity model and vault foundation only; personal login, enterprise login, cloud association, cloud storage, encrypted secret storage, and token storage remain unavailable.
- M12 has an identity transport foundation only; encrypted account transport, credential transport, token persistence, remote login, and trusted-time expiry enforcement remain unavailable.
- M13 has an account association status foundation only; local association is active, but personal/enterprise/cloud association, account linking, security-key login, enterprise policy, token persistence, and remote account authority remain unavailable.
- M14 has a cloud-storage broker foundation only; real public cloud storage, sync, automatic upload/download, offline cache, token storage, encrypted cloud transport, AI cloud access, and app-direct cloud authority remain unavailable or denied.
- M15 has installer UX planning only; destructive install actions remain disabled by default and not Product-approved.
- M18 has an Assistant host, read-only context flow, and predefined consent-scoped action templates only. Inference backend, model transport, generated answers, broad automation, autonomous action, package install/update, settings mutation, cloud AI, cloud memory, and internal install/write remain unavailable.
- Hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
