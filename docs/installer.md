# Installer Experience

## Design principles

- welcoming for first-time users
- fast for experienced users
- minimal friction on low-spec hardware
- clear explanation before any destructive disk action

## Welcome screen

The installer starts with a polished split-path welcome view:

- beginner path with plain language and recommended defaults
- advanced path with immediate access to storage, package, and service controls

The welcome screen should present:

- a short system promise focused on speed, privacy, and control
- live hardware summary
- recommended installation profile
- visible path switch between guided and advanced modes

## Hardware detection

The installer supports:

- automatic hardware detection by default
- manual override for GPU, storage mode, networking, and boot options

Detected signals feed recommendation logic:

- CPU class
- 32-bit versus 64-bit capability
- memory size
- storage type and free space
- GPU capability
- network availability

## Recommendation engine

The installer suggests settings based on:

- detected system specs
- declared intent: gaming, development, creator, general use, recovery, kiosk
- power and thermal profile where available

Examples:

- older laptop: lightweight desktop, reduced background services, local-only setup
- gaming desktop: performance profile, GPU stack, game compatibility layer
- developer workstation: dev tools, containers, SSH, local package caches

## Architecture selection

The installer should explicitly support both legacy 32-bit and modern 64-bit targets.

Behavior:

- auto-detect whether the machine is 32-bit only or 64-bit capable
- recommend the 64-bit image on modern systems
- recommend the 32-bit minimal image on truly old hardware
- optionally offer a 64-bit install with 32-bit compatibility libraries when the user wants maximum app compatibility

The architecture page should explain the tradeoff:

- 32-bit: best fit for older hardware, tighter memory limits, smaller runtime footprint
- 64-bit: preferred for modern systems, larger address space, stronger long-term platform support
- 64-bit plus compatibility: larger install, broader legacy app support

## Install media

The installer should treat boot media as a first-class choice rather than an afterthought.

Current bootstrap direction:

- raw disk images for direct USB imaging
- BIOS-bootable ISO media for optical-disc workflows
- a verified x86_64 removable-media UEFI image for modern hardware and direct USB-style boot testing, now with GOP framebuffer geometry, draw/read-back proof, boot-media file read proof, parsed boot manifest, bounded kernel payload loading into an aligned 512 KiB handoff buffer, firmware-backed kernel placement, pre/post-placement memory-map proof, and successful `ExitBootServices` teardown
- a verified x86_64 UEFI optical ISO path for broader burnable-media workflows with the same firmware framebuffer, boot-media file-read, loader-buffer, kernel-placement, handoff memory-map, and firmware-exit proofs

That matters directly for devices like the MSI Cyborg 15 A13VE:

- a modern 64-bit laptop should ultimately prefer the 64-bit UEFI installer path
- the current repository can already package BIOS ISO media for the mature x86 lane
- the x86_64 lane now has verified removable-media UEFI and UEFI optical scaffold paths with firmware framebuffer draw proof, boot-media file read proof, manifest-checked kernel payload loading into an aligned handoff buffer, exact firmware-page kernel placement, post-placement memory-map capture, and successful firmware boot-services exit, which makes it a realistic first-device bring-up lane before full installer UI and richer hardware-driver convergence land

The installer welcome flow should therefore expose:

- which architecture image was selected
- whether the current media is BIOS, UEFI, or hybrid
- whether the image includes legacy compatibility components

## Installation modes

### Beginner mode

- guided setup
- recommended partitioning
- small number of clear choices
- safe defaults for security and updates

### Advanced mode

- manual partition editor
- package and component matrix
- filesystem selection
- service presets
- encrypted install tuning
- dual-boot and recovery options

## Component selection

The installer exposes a component page where users can include or exclude:

- browser
- office tools
- media tools
- developer tools
- compatibility layers
- local AI support packages
- language packs

## Package manager selection

The installer should also expose a package-manager compatibility page.

The model should be:

- the native Limitless package broker is always present
- users may additionally install one or more compatibility frontends
- each selected frontend is configured as a brokered adapter, not as an unmanaged privileged package root

Selectable frontend examples:

- `apt` compatibility tools
- `dnf` or `yum` compatibility tools
- `apk` compatibility tools
- `choco` compatibility tools where the selected app-compat stack makes that workflow meaningful

The page should let users choose:

- which frontend CLIs to install
- which frontend should be the default shell-facing package experience
- whether to import compatible repository metadata or keep the frontend local-only until later
- whether to install cross-architecture compatibility libraries up front

That selection should behave consistently across both installer architecture paths:

- 32-bit installs can offer only the frontend adapters that make sense for the lighter compatibility profile
- 64-bit installs can additionally offer multilib-oriented adapters and compatibility bundles
- neither path should ever bypass the same native brokered transaction model underneath

Beginner mode should present a recommended package profile. Advanced mode should let users mix multiple frontends with clear warnings about repository trust, disk cost, and compatibility assumptions.

## AI-assisted installation

AI assistance is optional and only becomes available after networking exists or after an offline model package is explicitly selected.

The assistant can:

- explain tradeoffs
- recommend presets
- automate repetitive configuration steps with approval
- generate a post-install checklist

It cannot silently repartition disks or enable telemetry without explicit consent.
