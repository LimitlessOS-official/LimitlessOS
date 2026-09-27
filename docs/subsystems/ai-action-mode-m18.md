# M18 AI Consent-Scoped Action Mode

M18 adds the first Product AI action broker foundation without adding inference, a model backend, cloud AI, autonomous behavior, or broad automation.

M18 uses Mode B: deterministic predefined action templates only.

## Allowed Templates

- `assistant-note-write`: create or update `/HOME/ASSIST/NOTE.TXT` through a scoped write and commit grant.
- `installer-dryrun`: run the M15 installer dry-run validation as read-only.
- `open-settings-panel`: navigate to a read-only Settings panel.
- `package-trust-status`: show read-only package trust status.

Each template requires explicit consent. Approved grants are session-bound, action-bound, target-bound, scoped to the exact resource, and expired after completion.

## Denied Actions

M18 denies or leaves unavailable:

- package install or update apply
- settings mutation
- cloud enablement, upload, download, or sync
- secret or token access
- identity credential access
- model or cloud transport
- internal disk install or write authority
- boot-entry changes
- arbitrary filesystem writes
- path traversal outside the Assistant note namespace
- Assistant self-update or self-modification
- autonomous action

Denied actions execute nothing and receive no capability.

## Audit

The action broker records:

- action id
- Assistant principal
- session id
- requested action
- target resource
- requested capability
- consent decision
- grant id/generation
- execution result
- side-effect summary
- grant expiry/revocation
- denial reason

Settings and `pkginfo` expose the Product status read-only.

## Non-Goals

M18 does not add AI-generated answers, chat inference, model integration, cloud memory, automatic task execution, package-manager UX, app-store behavior, public cloud integration, real internal install/write, or M19 work.
