# LimitlessOS Status

Last updated: 2026-05-13

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

M12 is `Trusted Network Identity Transport`.

M12 preserves M10 local login and M11 identity/vault behavior, then adds a Product identity-transport foundation. UEFI Product verifies a signed local identity-provider descriptor fixture, records trusted endpoint metadata, and denies plaintext credential transport, unverified endpoint credential transport, and token storage while the vault remains Mode B. Local is still the only active account type. Personal and enterprise account types, cloud association, cloud storage, encrypted account transport, security-key login, remote login, encrypted secret storage, and token storage remain unavailable/non-product. M12 does not add multiuser account management, password-change UI, PAM/LDAP/remote auth, real internal install, formatting, NVRAM boot-entry changes, package install/apply UX, live public update fetching, auto-install, app-store behavior, AI behavior, browser behavior, or real internal-disk install writes.

## Product Profile

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

Current Product artifacts:

- BIOS kernel: `KERNEL64-BIOS.BIN`
- BIOS kernel bytes: 457088
- BIOS kernel sectors: 893 / 1024
- BIOS reserve: 131
- BIOS checksum: recorded in the generated artifact inventory
- UEFI kernel: `KERNEL64.BIN`
- UEFI kernel bytes: 560352
- UEFI kernel byte limit: 2,097,152 bytes
- UEFI byte reserve: 1,536,800
- UEFI checksum: recorded in the generated artifact inventory; it changes when the build-time package signing key is regenerated
- BIOS sector budget status: ok
- boot contract: split path. BIOS keeps the 1024-sector hard limit and 128-sector warning. UEFI Product uses a 2 MiB `KERNEL64.BIN` file-size contract verified against `BOOTMAN.TXT` byte count and checksum, with no UEFI sector arithmetic.

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
- Product service lifecycle/status query surface
- exactly one authenticated local console session with input, display, filesystem, network-status, and installer-dry-run authority scoped to that session
- brokered persistent file workflow
- NVMe persistence verification path
- UEFI-only Ed25519 package archive and payload admission
- UEFI-only signed update-index verification with rollback denial
- read-only package trust visibility through Settings and `pkginfo`
- read-only hardware validation visibility through `hwval`
- read-only identity/vault status through Settings
- read-only signed identity-provider descriptor and transport-safety status through Settings and `pkginfo`
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
- Product services are policy/security broker, console/shell broker, input broker, display/compositor, window manager/desktop shell, filesystem broker, block/storage broker, hardware inventory broker, network broker, installer dry-run service/tool, and settings/system-info provider
- Settings is the Product-safe status surface for service/session information
- controlled restart verification is limited to the scoped settings/system-info provider path
- stale capabilities from the old generation are denied
- M6 introduced one local console session; M10 authenticates that single local console user but still does not implement full multiuser account management
- M11 associates the authenticated session with a local identity record and vault metadata foundation without adding remote identity, personal account login, enterprise account login, cloud storage, or token storage
- M12 adds signed local identity-provider descriptor verification and read-only identity transport status without adding remote login, account association, encrypted credential transport, or token persistence
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
- installer write/install authority
- package install/update actions
- app store
- auto-install
- live public update fetch
- multiuser account management UI
- password change UI
- PAM/LDAP/remote auth
- AI assistant behavior
- personal account login
- enterprise account login
- cloud account association
- cloud storage
- security-key login
- remote login
- encrypted-at-rest secret storage
- token storage
- encrypted identity transport
- credential transport

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
- GUI/window manager/desktop are Product only for the M4 surface: Terminal, File Manager, Settings, brokered input, focused keyboard routing, and compositor-owned display.
- Network is Product only as a brokered hardware-gated status path. There is no socket API, packet API, or ambient network authority.
- Installer dry-run is Product safety tooling, but installer write/install authority is unavailable.
- M7.1 signed package admission has separate negative fixture coverage, but package-manager UI, app store, auto-install, live public update fetching, and trusted-time expiry enforcement remain unavailable.
- M6 has a local console session model, not full multiuser login/authentication.
- M11 has a local identity model and vault foundation only; personal login, enterprise login, cloud association, cloud storage, encrypted secret storage, and token storage remain unavailable.
- M12 has an identity transport foundation only; encrypted account transport, credential transport, token persistence, account association, remote login, and trusted-time expiry enforcement remain unavailable.
- There is no real AI assistant path.
- Hardware coverage is still QEMU-first plus limited real-hardware debugging; broad laptop validation remains incomplete.
- Source control now exists in this workspace, but dist/build artifacts are intentionally ignored and evidence packs live under ignored `dist/`.
