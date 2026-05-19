#ifndef LIMITLESS_NETWORK_SOCKET_X64_H
#define LIMITLESS_NETWORK_SOCKET_X64_H

#include "types.h"

#define NETWORK_SOCKET64_INVALID_HANDLE 0xFFFFFFFFu
#define NETWORK_SOCKET64_PROTOCOL_TCP 6u
#define NETWORK_SOCKET64_PROTOCOL_RAW 255u

void network_socket64_init(void);
u32 network_socket64_open_tcp(u32 network_capability, u32 remote_ipv4, u32 remote_port, u32 owner_id);
u32 network_socket64_open_raw(u32 network_capability, u32 protocol, u32 owner_id);
u32 network_socket64_listen_tcp(u32 network_capability, u32 local_port, u32 owner_id);
u32 network_socket64_send(u32 socket_handle, u32 byte_count, u32 owner_id);
u32 network_socket64_recv_status(u32 socket_handle, u32 owner_id);
u32 network_socket64_close(u32 socket_handle, u32 owner_id);
void network_socket64_probe(void);

u32 network_socket64_api_published(void);
u32 network_socket64_service_registered(void);
u32 network_socket64_capability_required(void);
u32 network_socket64_service_capability_minted(void);
u32 network_socket64_no_cap_denied(void);
u32 network_socket64_wrong_owner_denied(void);
u32 network_socket64_raw_denied(void);
u32 network_socket64_listen_denied(void);
u32 network_socket64_send_denied(void);
u32 network_socket64_connect_attempted(void);
u32 network_socket64_connect_granted(void);
u32 network_socket64_connect_unavailable(void);
u32 network_socket64_recv_status_granted(void);
u32 network_socket64_close_count(void);
u32 network_socket64_socket_count(void);
u32 network_socket64_last_http_status(void);
u32 network_socket64_last_response_bytes(void);
u32 network_socket64_fs_authority(void);
u32 network_socket64_storage_authority(void);
u32 network_socket64_ambient_authority(void);

#endif
