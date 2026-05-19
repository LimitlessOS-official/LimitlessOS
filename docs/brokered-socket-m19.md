# M19 Brokered Socket API Foundation

M19 turns the existing UEFI-only DHCP/DNS/TCP/HTTP proof into the first app-facing network service contract. The goal is deliberately small: prove that networking can be exposed through a brokered capability boundary without giving applications raw packet access or ambient network authority.

M21 consumes this surface from `NETHELLO`, a manifest-packaged descriptor-backed Ring-3 app that requests the network service capability through syscall and validates the same socket boundaries without ambient filesystem, storage, or network authority.

## Scope

Product behavior:

- UEFI Product registers a `network` service endpoint.
- Apps must hold a scoped network service capability before opening a socket handle.
- The socket surface can represent one status-oriented TCP-client handle over the broker-owned DHCP/DNS/HTTP proof path.
- `recv-status` reports the broker-owned HTTP status and response byte count.
- Close retires the handle and leaves no open sockets after verification.
- Denials are verifier-visible through `drs-socket` telemetry.

Non-product behavior:

- general POSIX-style socket library
- server/listen sockets
- raw packet sockets
- arbitrary app-controlled send/receive data plane
- UDP app sockets
- DNS query API exposed to apps
- filesystem or storage delegation through networking
- ambient network authority

## Service Contract

Service identifiers:

- `SERVICE_ID_NETWORK`
- `SERVICE_ENDPOINT_CLASS_NETWORK`
- `SERVICE_CAP_NETWORK`

UEFI syscall numbers:

- `X64_SYSCALL_NET_SOCKET_OPEN_TCP`
- `X64_SYSCALL_NET_SOCKET_RECV_STATUS`
- `X64_SYSCALL_NET_SOCKET_SEND`
- `X64_SYSCALL_NET_SOCKET_CLOSE`

BIOS fallback does not compile the socket implementation and returns the normal invalid/unavailable syscall behavior for these numbers.

## Capability Rules

The broker accepts only a network service capability routed to the network endpoint class. Verification covers:

- no-capability open denied
- wrong-owner open denied
- raw socket open denied
- listen socket open denied
- send denied until a future broker-owned data-plane authority exists
- recv-status denied for invalid or closed handles
- close retires the handle

The socket service does not mint filesystem, storage, raw packet, or ambient network authority.

## Telemetry

The scaffold emits a single `drs-socket` line after the network proof path initializes. The verifier expects:

- API published
- service registered
- capability required
- service capability minted
- no-cap and wrong-owner denials observed
- raw, listen, and send denials observed
- connect attempted
- connect granted when DHCP/DNS/HTTP proof succeeds
- recv-status granted
- close observed
- socket count returns to zero
- HTTP status and response byte count reported
- filesystem, storage, and ambient authority remain zero

The `net` shell builtin also prints the M19 socket status when UEFI networking is online, and prints a truthful unavailable message when networking is absent.

## Verification

Build:

```powershell
.\tools\build.ps1 -Architecture x86_64 -BuildProfile Product
```

UEFI socket verification:

```powershell
.\tools\verify-network-socket-m19.ps1
```

Optional device variants:

```powershell
.\tools\verify-network-socket-m19.ps1 -NetworkDevice e1000e
.\tools\verify-network-socket-m19.ps1 -NetworkDevice e1000
```

M19 is accepted only when the Product build and the UEFI socket verifier agree with this document.
