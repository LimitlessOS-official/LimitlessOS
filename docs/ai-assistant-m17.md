# M17 AI Assistant Read-Only Mode

M17 adds the first Product Assistant surface without adding an inference backend or action mode.

## Product Mode

M17 uses Mode B: Assistant host plus consent/context foundation only.

- The Assistant app opens from the Product desktop launcher after successful local login.
- The Assistant is blocked before authentication.
- The Assistant principal still has zero default capabilities.
- The Assistant may create read-only context requests only through the M16 policy broker.
- Consent is required before any context is delivered.
- Denied requests receive no data.
- Allowed requests receive only bounded, scoped, read-only status context.
- Every request, decision, scope, result, backend mode, and data-egress status is recorded in audit telemetry.
- Settings and `pkginfo` expose Assistant status read-only.

## Context Scope

Supported M17 context is intentionally narrow:

- system status
- package trust status
- installer plan/status
- account association status
- cloud storage policy/status
- hardware validation status
- selected file or selected audit/log status only when explicitly scoped and allowed

M17 does not grant recursive filesystem access, whole-disk access, raw block access, secret access, token access, cloud-file access, settings mutation, package mutation, or network/model access.

## Inference Status

Inference is unavailable in M17.

- no model call is made
- no backend transport is used
- no cloud memory is used
- no generated answer is claimed
- no scripted text is presented as model output
- no context leaves the system

The Assistant window must say this plainly. A Product build must not describe M17 as a working chat assistant.

## Authority Model

- The AI principal has no default capabilities.
- The Assistant app receives display/input only through the normal focused-window GUI path.
- The Assistant requests context through the policy broker.
- The policy broker requires consent before any read-only context grant.
- Context grants are bounded to the approved scope and session.
- Stale grants and wrong-session grants are denied.
- The Assistant cannot call filesystem, network, settings, package, cloud, or secret APIs directly.
- The Assistant cannot request mutation authority in M17.

## Package Integrity

The Assistant is treated as a signed Product component.

- Product package policy records Assistant package integrity.
- Assistant self-modification is denied.
- A signature proves component origin and integrity only; it does not grant authority.

## Unavailable

These are unavailable or denied in M17:

- inference backend
- model transport
- generated answers
- AI actions
- automation
- cloud memory
- filesystem writes
- settings changes
- package install/update
- secret or token access
- cloud storage access
- internal install/write

M18 now builds on M17 with a small consent-scoped action broker foundation. The M17 read-only context flow remains Product and unchanged; M18 action templates remain separate, predefined, audited, and still do not add inference, model transport, generated answers, or autonomy.
