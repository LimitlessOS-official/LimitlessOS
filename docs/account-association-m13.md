# M13 Account Association

M13 adds Product account-association status without changing M10 local authentication or M12 identity transport.

## Product Scope

- Local login remains the only Product login flow.
- The system still has one authenticated local console user.
- The local user record remains `/USERDB.TXT` in the brokered persistent NVMe namespace.
- M12 signed identity-provider descriptor verification remains metadata-only trust input.
- No cloud account is required for boot, login, desktop, shell, or local persistence.
- No public internet dependency is introduced.

## Association Mode

M13 uses Mode B: association policy/status only.

Local association is active and offline-capable. Personal association, enterprise association, cloud association, security-key login, credential transport, token storage, enterprise policy, and remote account authority remain unavailable, planned, or denied. M13 does not implement remote account linking.

## Association Records

The Product UEFI path records three association shapes:

- `local`: active, bound to the authenticated local console user, offline-capable.
- `personal`: planned/unavailable, credential transport denied, token storage denied.
- `enterprise`: planned/unavailable, credential transport denied, token storage denied, enterprise policy unavailable.

Each record carries an association id, local user id, account type, provider id, provider descriptor id, association status, online/offline status, credential transport status, token storage status, security-key status, cloud storage status, enterprise policy status, last verification result, creation generation, and revoked/unlinked flags.

## Authority Model

- The account association broker owns association state.
- Settings receives read-only account status authority.
- `pkginfo` reports read-only account status text and receives no identity mutation authority.
- Local login remains local-only and offline-capable.
- Apps cannot mutate account associations.
- Apps cannot unlink accounts.
- Apps cannot read or store tokens.
- Apps cannot request cloud credentials.
- Apps cannot obtain account-derived filesystem, network, package, or session authority.
- Remote provider descriptors and remote account identities do not grant OS capabilities by themselves.

## Denial Evidence

The M13 verifier requires:

- `drs-account-association-product 1`
- `drs-account-local-active 1`
- `drs-account-personal-unavailable 1`
- `drs-account-enterprise-unavailable 1`
- `drs-account-cloud-unavailable 1`
- `drs-account-security-key-unavailable 1`
- `drs-account-settings-panel 1`
- `drs-account-status-readonly 1`
- `drs-account-mutation-denied 1`
- `drs-account-unlink-denied 1`
- `drs-account-token-storage-denied 1`
- `drs-account-credential-transport-denied 1`
- `drs-account-enterprise-policy-unavailable 1`
- `drs-account-remote-no-ambient-authority 1`
- `drs-no-ambient-account-identity 1`
- `drs-no-ambient-account-network 1`
- `drs-no-ambient-account-secret 1`

## Non-Product

- Personal account login remains unavailable/planned.
- Enterprise account login remains unavailable/planned.
- Account linking remains unavailable/planned.
- Cloud association remains unavailable/planned.
- Cloud storage remains unavailable/planned.
- Security-key login remains unavailable/planned.
- Remote login remains unavailable.
- Encrypted account transport remains unavailable.
- Credential transport remains denied.
- Token storage remains denied while the vault remains Mode B.
- Enterprise policy enrollment remains unavailable/planned.
- Trusted-time expiry enforcement remains unavailable without a trusted time source.
- M14 cloud storage has not started.
