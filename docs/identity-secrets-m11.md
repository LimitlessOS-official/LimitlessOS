# M11 Identity Foundation and Secrets Vault

M11 creates the Product identity foundation without changing M10 login behavior.

## Product Scope

- Local login remains the only Product login flow.
- The system still has one local user record from M10.
- The local user record remains `/USERDB.TXT` in the brokered persistent NVMe namespace.
- Password verification remains bcrypt `$2b$` through the M10 `crypt_blowfish` path.
- The desktop, terminal, launcher, and shell remain blocked until local authentication succeeds.
- Lock and unlock continue to resume the same session.

## Account Model

M11 defines three account types:

- `local`: Product active.
- `personal`: planned/unavailable.
- `enterprise`: planned/unavailable.

The active Product identity is the authenticated local console account. M11 records a local account id, display name, local association status, offline-capable status, credential record type, vault binding status, and creation generation/tick telemetry.

M11 does not add remote login, email/password cloud login, enterprise login, security-key login, account switching, password-change UI, or multiuser account management.

## Account Associations

M11 adds association records only as a foundation:

- Local association: active.
- Personal association: planned/unavailable.
- Enterprise association: planned/unavailable.
- Cloud storage association: planned/unavailable.
- AI memory association: planned/unavailable.

No network dependency is introduced. A remote account cannot grant authority in M11 because remote account association is unavailable.

## Vault Mode

M11 uses vault Mode B: foundation only.

The vault records metadata only. Encrypted-at-rest secret storage is not Product in M11, and no real secrets, cloud tokens, account passwords, API keys, or assistant-memory secrets are stored.

The runtime and inventory state must say:

- Secret vault status: foundation metadata only.
- Encrypted vault status: unavailable/non-product.
- Real secrets or tokens stored: false.
- No plaintext token storage: verified.

## Authority Model

- Login/auth receives credential verification authority.
- Identity/status is read-only after authentication.
- Settings receives read-only identity-status authority.
- No shell identity command is added in M11.
- Apps cannot read identity records unless explicitly granted read-only identity-status authority.
- Apps cannot mutate identity records.
- Apps cannot read or write secrets.
- Apps cannot request cloud tokens.
- Apps cannot mutate account associations.

## Denial Evidence

The M11 verifier requires:

- `drs-identity-foundation 1`
- `drs-identity-local-active 1`
- `drs-identity-personal-unavailable 1`
- `drs-identity-enterprise-unavailable 1`
- `drs-identity-settings-panel 1`
- `drs-identity-status-readonly 1`
- `drs-identity-mutation-denied 1`
- `drs-vault-foundation 1`
- `drs-vault-secret-read-denied 1`
- `drs-vault-secret-write-denied 1`
- `drs-vault-no-plaintext-token 1`
- `drs-cloud-association-unavailable 1`
- `drs-no-ambient-identity 1`
- `drs-no-ambient-secret 1`

M11 explicitly records `encrypted-vault 0` and `secret-storage 0`.

## Non-Product Until Later Milestones

- Personal login.
- Enterprise login.
- Remote authentication.
- Security-key login.
- Trusted network identity transport.
- Cloud storage.
- Token refresh.
- Encrypted secret storage.
- AI assistant behavior.

Those surfaces are future roadmap work and must not appear as active Product behavior in M11.
