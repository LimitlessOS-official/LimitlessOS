# M14 Cloud Storage Broker Foundation

M14 adds a Product cloud-storage broker foundation without enabling real cloud storage.

## Product Scope

- The cloud-storage broker foundation is active in the UEFI Product path.
- A signed deterministic local cloud-provider descriptor fixture is verified.
- Settings shows read-only cloud policy/status.
- File Manager shows only read-only unavailable cloud status.
- `pkginfo` reports read-only cloud policy/status.
- BIOS Product remains the lean checksum-only fallback and reports cloud storage unavailable.

## Non-Product Scope

- Public cloud storage is unavailable.
- Cloud account association is unavailable.
- Cloud sync is unavailable.
- Automatic upload is unavailable.
- Automatic download is unavailable.
- Offline cache is planned/unavailable.
- Token storage is denied while the vault remains Mode B.
- Encrypted cloud transport is unavailable.
- AI cloud access is unavailable.
- Apps do not receive direct cloud authority.

## Descriptor Verification

The signed cloud-provider descriptor records provider id/type, descriptor and protocol versions, fixture endpoint metadata, endpoint key id/fingerprint, supported modes, token policy, offline-cache policy, sync policy, required transport security, required account association, minimum OS version, sequence/generation, trusted-time metadata, signer key id, and signature.

Verification requires:

- signature present
- signature valid
- trusted signer
- cloud-storage provider type
- supported descriptor/protocol version
- accepted sequence/generation
- valid required fields

Denied cases are verifier-visible:

- missing signature
- invalid signature
- wrong key
- tampered descriptor
- rollback/older sequence
- unsupported version
- malformed descriptor

## Authority Model

- The cloud-storage broker owns cloud policy state.
- Settings receives read-only cloud-status authority only.
- File Manager receives read-only cloud-status authority only.
- `pkginfo` receives read-only cloud-status authority only.
- Apps cannot initiate cloud network transport.
- Apps cannot upload, download, sync, cache, store tokens, or access AI cloud memory.
- Provider descriptor trust never grants capabilities by itself.
- Local session identity does not grant cloud authority automatically.

## Verification

The M14 verifier requires `drs-cloud-*` telemetry for descriptor verification, denial cases, Settings/File Manager read-only status, upload/download/sync denial, automatic upload/download unavailability, AI cloud access unavailability, app-direct authority denial, and no ambient cloud/filesystem/network/identity/secret authority.

M14 evidence is archived with:

```powershell
.\tools\archive-m14-evidence.ps1 -IncludeExperimental
```

M15 must not start until the M14 archive is clean.
