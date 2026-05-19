# M21 Native App SDK Foundation

M21 turns the M20 native execution path into a build-time and kernel-side app model that can grow without editing kernel code for each new app.

## Build Input

Native apps are declared as JSON manifests under `apps/native/`. The current manifest is `apps/native/nethello.json`.

Required manifest fields:

- `name`: uppercase app name used for `/APPS/<NAME>.APP` and `/APPS/<NAME>.BIN`
- `executableId`: descriptor executable id
- `source`: assembly source compiled by NASM
- `payloadSlot`: signed package payload slot for the binary
- `authorityMask`: descriptor authority mask
- `argumentPolicy`: descriptor argument policy
- `launchBinding`: descriptor launch binding
- `entryResult`: expected entry checkpoint value
- `successResult`: expected successful app exit value
- `capabilities`: named capability list such as `console` and `network`

The build validates the manifest, assembles the source binary, generates a descriptor, stages both files into `/APPS`, and feeds the binary into package payload signing. The descriptor now carries structured metadata lines:

```text
name=NETHELLO
binary=NETHELLO.BIN
payload-slot=13
entry-result=0x4E484530
success-result=0x4E484531
capabilities=console,network
```

## Kernel Load Path

The UEFI Product path loads native apps by app name:

- `mmio64_stage_app_model_native_app(...)` reads `/APPS/<NAME>.APP` through the shell filesystem authority and `/APPS/<NAME>.BIN` from ISO9660 media.
- `app_model64_stage_native_app(...)` parses the descriptor, validates the structured metadata, checks the package payload record for the declared slot, and rejects mismatched binary names, missing payload metadata, missing entry/success results, or capability/authority mismatches.
- `launch64_stage_disk_flat_binary(...)` maps the verified binary into isolated Ring-3 memory and uses the existing GDT/TSS/page-table user-entry path.
- `app_model64_record_native_launch(...)` records capability denial and brokered socket behavior while the app is active.

The old NETHELLO-specific entry points remain wrappers for compatibility, but the active implementation is the generic app-name path.

## Capability Contract

Descriptor capabilities are not ambient authority. The loader cross-checks declared capabilities against descriptor authority bits before launch:

- `console` requires console authority and lets the app print through delegated console routing.
- `network` requires network authority and lets the app request the M19 network service capability through syscall.
- undeclared filesystem and block service capability requests are denied while the app is active.

The M19 socket surface remains intentionally narrow: `NETHELLO` can open the status-oriented TCP-client socket, observe broker-owned recv status, and close it. App-controlled send remains denied until a future data-plane authority exists.

## Verification

```powershell
.\tools\verify-native-app-sdk-m21.ps1
```

The verifier expects `drs-app-m21` with manifest-generated descriptor metadata, package payload verification, Ring-3 launch, syscall bridge, network capability request/grant, socket open/recv/send-denied/close, denied filesystem/block capability requests, and zero filesystem/storage/ambient authority.

Current non-goals:

- no ELF, PE/COFF, Mach-O, or Linux/Windows/macOS persona loader
- no dynamic linking or relocation model
- no general socket library or arbitrary app network data plane
- no ambient filesystem, storage, package, or network authority
