# M6 Service Manager + User/Session Model

Status: Product-path hardening. M6 does not add new broad features, AI behavior, package management, browser behavior, or internal-disk install writes.

## Product Services

M6 treats these services as Product active and status-tracked:

- policy/security broker
- console/shell broker
- input broker
- display/compositor
- window manager / desktop shell
- filesystem broker
- block/storage broker
- hardware inventory broker
- network broker
- installer dry-run service/tool
- settings/system-info provider

Each service record in the M6 artifact inventory includes a service id, name, principal, manifest id, lifecycle state, generation, restart count, granted capabilities, health, and session binding when applicable.

## Lifecycle States

The Product lifecycle vocabulary is:

- declared
- admitted
- launching
- running
- degraded
- stopping
- stopped
- crashed
- restarting
- denied

The M6 verifier proves a controlled crash/restart path for the scoped settings/system-info provider. That proof is deliberately narrow: it records the crash, applies a checked restart policy, increments generation, denies stale and wrong-owner capabilities, and confirms no additional capabilities are granted.

## Session Model

M6 creates exactly one local console session:

- session id: 1
- user id: local-console
- seat id: 0
- state: active
- input scope: active-session-only
- display scope: session-window-namespace
- filesystem grants: RAMFS, boot-media read-only, brokered persistent namespace
- network grants: read-only network status
- installer grants: dry-run read-only inventory

M10 layers a minimal UEFI Product login gate on top of this single session: one local console user is created on first run, authenticated before desktop access, and can lock/unlock the same session. This is still not full multiuser account management, password-change UI, PAM/LDAP, or remote authentication.

## Authority Rules

- Raw input belongs to the input broker.
- The input broker routes events only to the active session.
- The window manager routes events only to windows in that session.
- The focused window receives keyboard events; unfocused windows are denied.
- The compositor owns physical framebuffer presentation.
- Apps draw only through delegated window surfaces.
- File Manager is limited to safe brokered namespaces.
- Installer dry-run may inspect partitions through read-only authority only.
- Installer write, format, and boot-entry authority remain disabled by default.
- The network broker owns DHCP/DNS/TCP/HTTP status behavior; apps receive no sockets or packet API in M6.

## Product Visibility

Settings is the Product-safe status surface. It reports service/session status alongside the existing display, input, network, storage, profile, and boot information. It remains read-only.

## Verification

M6 verifier checkpoints include:

- drs-service-manager-product
- drs-service-declared
- drs-service-running
- drs-service-status-query
- drs-service-controlled-crash
- drs-service-restart
- drs-service-generation-increment
- drs-service-stale-cap-denied
- drs-session-created
- drs-session-active
- drs-session-input-bound
- drs-session-display-bound
- drs-session-fs-bound
- drs-session-network-bound
- drs-wrong-session-input-denied
- drs-wrong-session-display-denied
- drs-wrong-session-fs-denied
- drs-no-ambient-input
- drs-no-ambient-display
- drs-no-ambient-fs
- drs-no-ambient-network
- drs-installer-write-disabled
- drs-installer-dryrun-no-writes

## Kernel Budget

M5 Product baseline was 473488 bytes, 925 / 1024 BIOS sectors, 99 reserve, checksum 0x5D996177.

M6 Product is 474528 bytes, 927 / 1024 BIOS sectors, 97 reserve, checksum 0xAFCCC5B6.

The two-sector reserve regression is from adding Product service/session status and verifier telemetry to the BIOS-constrained kernel. Reserve remains above the 96-sector hard floor but below the 128-sector warning threshold. M7 or installer write/install code must not be added to the BIOS-constrained Product kernel until reserve is recovered or the boot contract is intentionally changed.
