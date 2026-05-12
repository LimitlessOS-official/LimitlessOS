# LimitlessOS Package Format

## Current bootstrap shape

LimitlessOS now has a bootstrap package archive plus manifest path for built-in user-space services and small user-space utilities. The current v2 format is intentionally small, build-generated, and, in the UEFI Product kernel, admitted through Ed25519 signature verification:

- `package_id`
- `package_name`
- `package_version`
- `source_slot`
- `signer_id`
- `signature_token`
- `trust_flags`
- `launch_authority_mask`
- `launch_role`
- `max_instances`
- `expected_image_size`
- `expected_image_checksum`
- payload `imageOffset`, `imageSize`, and `imageChecksum` records that identify the actual bootstrap image byte range behind a manifest payload slot
- runtime policy fields for scheduler class, service discovery scope, and capability admission
- M7 Ed25519 archive and payload signatures generated during the build
- signed update-index fixture metadata with monotonic sequence anti-rollback

The source of truth is now [packages/bootstrap-store.json](/C:/Users/h1nuz/Documents/Codex/2026-04-25/architecture-performance-use-a-hybrid-kernel-2/packages/bootstrap-store.json). During build, [tools/generate-package-store.ps1](/C:/Users/h1nuz/Documents/Codex/2026-04-25/architecture-performance-use-a-hybrid-kernel-2/tools/generate-package-store.ps1) serializes that spec into a generated archive header consumed by the kernel package-store parser.

## Bootstrap store behavior

At boot, the kernel:

- parses a generated serialized bootstrap archive
- validates the archive header, layout, and checksum before trusting any record
- loads package candidates from that archive
- checks each candidate against the trusted signer registry
- validates the candidate's manifest envelope token against that signer
- accepts only verified candidates into the runnable package catalog
- rejects untrusted candidates before `init` can request a launch

In the UEFI Product kernel, M7 additionally verifies:

- a detached Ed25519 archive signature over the generated bootstrap archive bytes
- detached Ed25519 payload signatures before disk-sourced utility payload admission
- payload checksum and size agreement after signature validation
- scoped install capability, owner, and stale-token policy for install-style admission attempts
- signed update-index fixture authenticity and monotonic sequence anti-rollback

The BIOS Product kernel intentionally keeps the checksum-only bootstrap fallback to preserve the 1024-sector boot contract. BIOS does not compile the Ed25519 verifier.

This means "available in the store" and "launchable by policy" are now distinct states.

The current archive already uses that split for both long-lived services and shell-launched tools:

- `bootstrap-session` and `bootstrap-worker` are launched by `init`
- Product utilities are launchable only from the session shell authority path and only through descriptor-declared capability delegation

## Filesystem launcher descriptors

The bootstrap shell now also consumes tiny filesystem-backed launcher descriptors from `/APPS/*.APP`. These are not package manifests; they are small ramfs files that let userspace discover which verified package to launch and what authority shape that app expects.

The current descriptor layout is:

- first line: decimal executable id
- second line: decimal authority mask
- third line: decimal argument policy
- fourth line: decimal launch flags
- fifth line: human-readable usage/help summary
- sixth line: lowercase category tag for discovery views

The current authority bits are:

- `1`: shared buffer
- `2`: base directory capability
- `4`: destination directory capability
- `8`: text payload semantics
- `16`: paired-path semantics

The current argument-policy values are:

- `1`: raw buffer payload
- `2`: single path argument
- `3`: path plus text payload
- `4`: paired rename paths
- `5`: paired move paths

The current launch-flag bits are:

- `1`: foreground launch with shell wait
- `2`: delegated console binding required
- `4`: delegated input binding required

In practice, the shell now enforces the authority bits for capability delegation, validates the separate argument-policy field before it decides how to marshal arguments into the delegated shared buffer, honors the launch flags before deciding whether the app should receive delegated `console` or `input` capabilities, uses the fifth-line summary to answer `help <command>` without keeping a second per-app usage table in shell code, uses the sixth-line category tag to power Product app discovery, and decodes the same six-line record through `info <command>` so descriptor inspection stays inside the same file-capability path as launch discovery while remaining human-readable from ring 3. Product shell output hides internal notes and labels unavailable surfaces instead of presenting raw `/APPS` internals as finished apps. `/APPS/INDEX.TXT` remains an internal note, not a Product app. For example:

- `LS.APP`, `CAT.APP`, and `STAT.APP` request buffer plus base directory authority with policy `2` and launch flags `3` so they receive both filesystem and `console` authority
- `MKDIR.APP`, `TOUCH.APP`, and `DELETE.APP` request buffer plus base directory authority with policy `2` and launch flags `1` so they stay foreground-only without ambient output authority, and `MKDIR.APP` now interprets nested paths in userspace by repeatedly applying that same delegated base-directory authority one segment at a time
- `WRITE.APP` and `APPEND.APP` request buffer plus base directory authority with policy `3` and launch flags `1`
- `RENAME.APP` requests buffer plus base directory authority with policy `4` and launch flags `1`
- `MOVE.APP` and `COPY.APP` request buffer plus both base and destination directory authority with policy `5` and launch flags `1`
- `ASK.APP`, `ECHO.APP`, and alias descriptors remain unavailable/non-product in the Product shell unless promoted by a later milestone with matching descriptors, binaries, docs, and verification

## Archive layout

The current generated archive is a compact byte stream with:

- a fixed header carrying magic, version, record counts, string-table size, and archive checksum
- signer records with string offsets and verification tokens
- manifest records with string offsets plus launch and scheduling policy
- payload descriptors that map manifest payload slots onto kernel-known bootstrap payload kinds plus image offset, image size, and image checksum metadata
- a null-terminated string table

This keeps the runtime loader parser-oriented instead of depending on C-initialized manifest structs.

## What the kernel verifies today

Before `init` launches a bootstrap user-space service, the kernel verifies:

- the executable exists in the trusted bootstrap catalog
- the measured image size matches the manifest
- the measured image digest matches the manifest
- the archive payload byte range matches the real linker-provided bootstrap image region before payload reads are allowed
- the x64 launch broker can derive a sealed runtime image plan, install a supervisor-only transfer-image mapping, expose a protection profile/token proving the current view is not user-accessible or writable, and expose broker-owned image-map, install, and entry-transfer tokens before a service is treated as replacement-ready
- the required bootstrap policy approval has already happened when the manifest asks for it
- the calling launch authority is allowed by the manifest
- the instance cap for that package has not already been reached
- on UEFI Product builds, the generated package archive signature and relevant payload signatures validate against the embedded Ed25519 public key
- on UEFI Product builds, missing signatures, invalid signatures, checksum mismatch, wrong-owner install attempts, stale install tokens, and rollback update-index attempts are denied with telemetry

If any check fails, launch is denied and counted in telemetry.

## What this is not yet

This is not yet a replaceable general-purpose app store or auto-update system. The current bootstrap format is:

- measured
- kernel-enforced
- build-generated from an external manifest spec
- archive-validated
- Ed25519-signed on UEFI Product builds
- store-loaded
- signer-gated
- policy-gated

M7 proves signed archive/payload admission and signed update-index anti-rollback. It does not yet provide Product package-manager UI, auto-install, persistent downloaded package installation, live public update fetching, or expiry enforcement without a trusted time source. Signature proves origin and integrity; capability policy still controls what authority a package can receive.
