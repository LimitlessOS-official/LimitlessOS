# M16 AI Policy Broker Foundation

M16 adds the Product AI policy foundation only. It does not add an Assistant app, model integration, chat inference, automation, cloud AI, system control, or task execution.

## Product Behavior

- UEFI Product records a system AI principal with no default capabilities.
- The AI principal can only create action requests.
- Each request records an id, action type, resource, requested capability, scope, reason/timestamp/origin status, decision, and result.
- The consent broker requires an explicit user decision before any future action could be considered.
- M16 does not auto-approve requests.
- M16 does not execute requests.
- M16 maps requests to required capabilities but does not grant them.
- Settings and `pkginfo` expose the AI policy state read-only.

## Denial Boundary

M16 verifier telemetry proves:

- no-consent requests are denied
- invalid-scope requests are denied
- AI has no ambient authority
- AI has no filesystem access
- AI has no network access
- AI has no settings access
- AI has no package access
- AI has no secret access
- AI has no cloud access
- zero AI actions execute
- zero default AI capabilities exist

## Audit

The audit log is represented as immutable/queryable policy telemetry in M16. It records the request, decision, scope, result, and timestamp status. The audit state is visible through Settings and `pkginfo`; it is not a writable user or app surface.

## Unavailable

These remain unavailable/non-product:

- Assistant app behavior in M16
- AI inference backend
- generated answers
- AI actions
- AI automation
- cloud AI
- model integration
- chat inference
- task execution
- AI filesystem access
- AI network access
- AI settings mutation
- AI package access
- AI secret access
- AI cloud access

M17 builds on this policy layer by adding an Assistant host and read-only consent/context flow. M17 still does not add action mode, automation, cloud memory, model transport, or generated answers.
