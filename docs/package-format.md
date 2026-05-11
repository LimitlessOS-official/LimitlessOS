# LimitlessOS Package Format

## Current bootstrap shape

LimitlessOS now has a bootstrap package archive plus manifest path for built-in user-space services and small user-space utilities. The current v2 format is intentionally small, but it is no longer handwritten directly in kernel code:

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

This means "available in the store" and "launchable by policy" are now distinct states.

The current archive already uses that split for both long-lived services and shell-launched tools:

- `bootstrap-session` and `bootstrap-worker` are launched by `init`
- `utility-echo`, `utility-ls`, and `utility-cat` are launchable only from the session shell authority path

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

In practice, the shell now enforces the authority bits for capability delegation, validates the separate argument-policy field before it decides how to marshal arguments into the delegated shared buffer, honors the launch flags before deciding whether the app should receive delegated `console` or `input` capabilities, uses the fifth-line summary to answer `help <command>` without keeping a second per-app usage table in shell code, uses the sixth-line category tag to power `apps` plus `apps <category>` discovery views from the same launcher files, and now decodes the same six-line record through `info <command>` so descriptor inspection stays inside the same file-capability path as launch discovery while remaining human-readable from ring 3. `/APPS/INDEX.TXT` is also seeded as a plain text note that explains the directory at a glance. For example:

- `ECHO.APP` and `SAY.APP` are buffer-only with policy `1` and launch flags `3` so the shell waits and delegates `console`
- `LS.APP`, `CAT.APP`, and `STAT.APP` request buffer plus base directory authority with policy `2` and launch flags `3` so they receive both filesystem and `console` authority
- `MKDIR.APP`, `TOUCH.APP`, and `DELETE.APP` request buffer plus base directory authority with policy `2` and launch flags `1` so they stay foreground-only without ambient output authority, and `MKDIR.APP` now interprets nested paths in userspace by repeatedly applying that same delegated base-directory authority one segment at a time
- `WRITE.APP` and `APPEND.APP` request buffer plus base directory authority with policy `3` and launch flags `1`
- `RENAME.APP` requests buffer plus base directory authority with policy `4` and launch flags `1`
- `MOVE.APP` and `COPY.APP` request buffer plus both base and destination directory authority with policy `5` and launch flags `1`
- `ASK.APP` is buffer-only with policy `1` and launch flags `7` so the shell delegates both `console` and `input`, proving that interactive apps can launch through the same descriptor contract
- `SHOW.APP`, `LIST.APP`, `MAKE.APP`, `PUT.APP`, `SWAP.APP`, and `SHIFT.APP` are generic alias descriptors that reuse those same verified utility packages while proving that launch discovery, authority shape, and console binding all come from the descriptor file, not from a hardcoded shell command table

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

If any check fails, launch is denied and counted in telemetry.

## What this is not yet

This is not yet a replaceable on-disk package system and it is not yet a true asymmetric signature pipeline. The current bootstrap format is:

- measured
- kernel-enforced
- build-generated from an external manifest spec
- archive-validated
- store-loaded
- signer-gated
- policy-gated

The next stage is to move this into signed package artifacts with persistent storage and measured loading from a real package source instead of a build-generated bootstrap archive.
