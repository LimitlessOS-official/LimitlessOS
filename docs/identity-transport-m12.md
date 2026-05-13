# M12 Trusted Network Identity Transport

M12 adds the Product identity transport foundation needed before any future personal or enterprise account association. It does not add remote login, account linking, cloud storage, OAuth, SSO, security-key login, token persistence, or public identity-provider integration.

## Product Scope

- Local login remains the only Product login flow.
- The M11 identity model remains unchanged: local is active; personal and enterprise are planned/unavailable.
- The vault remains Mode B foundation only.
- No real secrets or tokens are stored.
- No cloud account is required for boot or login.
- No public internet dependency is introduced.

## Transport Mode

M12 uses Mode B: endpoint trust foundation only.

The Product UEFI path verifies a deterministic signed local identity-provider descriptor fixture and records endpoint-trust status. Encrypted account transport is unavailable in M12. Credential transport is denied. Token storage is denied while the vault remains Mode B.

## Provider Descriptor

The signed descriptor records:

- provider id
- provider type
- display name
- descriptor version
- protocol version
- fixture endpoint
- endpoint public key id and fingerprint
- supported auth methods
- required transport security level
- account association status
- token persistence policy
- minimum OS version
- sequence/generation
- trusted-time requirement and expiry metadata status
- signer key id
- Ed25519 signature

The signed byte prefix is fixed for M12 fixtures: `LimitlessOS-M12-idprovider-v1`.

## Verification

The identity transport broker verifies:

- descriptor signature is present
- descriptor signature is valid
- signer key is trusted
- provider type is supported
- protocol version is supported
- sequence/generation is accepted
- rollback descriptors are denied
- unsupported descriptor versions are denied
- tampered descriptors are denied
- wrong-key descriptors are denied

DNS and HTTP success do not establish identity trust. In M12, trust comes only from the signed descriptor, embedded trust root, protocol checks, and capability policy.

## Authority Model

- The network broker owns raw network behavior.
- The identity transport broker receives only scoped identity-network authority.
- Settings receives read-only identity transport status authority.
- `pkginfo` reports read-only status text and receives no install/update/identity mutation authority.
- Apps cannot open identity network transport directly.
- Apps cannot send credentials.
- Apps cannot store tokens.
- Apps cannot request account association.
- A signed provider descriptor never grants OS authority by itself.

## Denial Evidence

The M12 verifier requires:

- `drs-idtransport-product 1`
- `drs-idtransport-provider-descriptor 1`
- `drs-idtransport-descriptor-verified 1`
- `drs-idtransport-descriptor-missing-sig-denied 1`
- `drs-idtransport-descriptor-invalid-sig-denied 1`
- `drs-idtransport-descriptor-wrong-key-denied 1`
- `drs-idtransport-descriptor-tamper-denied 1`
- `drs-idtransport-descriptor-rollback-denied 1`
- `drs-idtransport-descriptor-version-denied 1`
- `drs-idtransport-network-scoped 1`
- `drs-idtransport-no-network-cap-denied 1`
- `drs-idtransport-plaintext-credential-denied 1`
- `drs-idtransport-unverified-endpoint-denied 1`
- `drs-idtransport-token-storage-denied 1`
- `drs-idtransport-personal-unavailable 1`
- `drs-idtransport-enterprise-unavailable 1`
- `drs-idtransport-cloud-association-unavailable 1`
- `drs-idtransport-settings-panel 1`
- `drs-idtransport-status-readonly 1`
- `drs-idtransport-trusted-time-status 1`
- `drs-no-ambient-idtransport-network 1`
- `drs-no-ambient-idtransport-identity 1`
- `drs-no-ambient-idtransport-secret 1`
- `drs-idtransport-encrypted-channel-unavailable 1`
- `drs-idtransport-credential-transport-unavailable 1`

## Non-Product

- Personal login remains unavailable/planned.
- Enterprise login remains unavailable/planned.
- Cloud association remains unavailable/planned.
- Remote login remains unavailable.
- Encrypted account transport remains unavailable.
- Credential transport remains denied.
- Token storage remains denied.
- Trusted-time expiry enforcement remains unavailable without a trusted time source.
- M13 account association has not started.
