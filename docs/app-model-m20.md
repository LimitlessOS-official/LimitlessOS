# M20 Native App Execution Pipeline

M20 is the historical Ring-3 native execution checkpoint that turned the M19 brokered socket foundation into a user-space app validation. M21 supersedes the one-app staging path with a manifest-driven SDK foundation documented in `docs/native-app-sdk-m21.md`.

The shipped app is `NETHELLO`:

- `/APPS/NETHELLO.APP` declares console plus network authority.
- `/APPS/NETHELLO.BIN` is a flat x86_64 user binary signed through package payload slot 13.
- The loader parses the descriptor from the dynamic ISO `/APPS` scan, reads the binary from the same media, verifies size/checksum/signature through the package store, maps the app at the disk-user image base, and jumps to Ring 3 through the existing GDT/TSS/page-table user-entry path.
- While the app is active, service capability grants are descriptor-gated. Console and network are allowed because the descriptor declares them; RAMFS and block service requests are denied.
- The app writes `hello from user app`, requests a network service capability, opens the M19 brokered TCP-client status socket, observes recv-status, confirms send remains denied without broker data-plane authority, closes the socket, and exits with `0x4E484531`.

Historical verification:

```powershell
.\tools\verify-app-model-m20.ps1
```

The M20 verifier expected `drs-app-m20` with descriptor parse, binary verification, Ring-3 launch, syscall bridge, network capability request/grant, socket open/recv/send-denied/close, denied filesystem/block capability requests, and zero filesystem/storage/ambient authority. Current builds emit the M21 `drs-app-m21` checkpoint through the manifest-generated descriptor path.
