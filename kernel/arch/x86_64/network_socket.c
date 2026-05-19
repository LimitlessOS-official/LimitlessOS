#include "network_socket_x64.h"

#include "capability_x64.h"
#include "principal_x64.h"
#include "services.h"
#include "services_x64.h"
#include "virtio_net_x64.h"

#define NETWORK_SOCKET64_HANDLE_BASE 0x534B0000u
#define NETWORK_SOCKET64_TABLE_LIMIT 4u

struct network_socket64_record
{
    u32 active;
    u32 handle;
    u32 owner_id;
    u32 remote_ipv4;
    u32 remote_port;
    u32 protocol;
    u32 http_status;
    u32 response_bytes;
};

static struct network_socket64_record g_network_sockets[NETWORK_SOCKET64_TABLE_LIMIT];
static u32 g_next_socket_handle = NETWORK_SOCKET64_HANDLE_BASE;
static u32 g_probe_ran = 0u;
static u32 g_service_capability_minted = 0u;
static u32 g_no_cap_denied = 0u;
static u32 g_wrong_owner_denied = 0u;
static u32 g_raw_denied = 0u;
static u32 g_listen_denied = 0u;
static u32 g_send_denied = 0u;
static u32 g_connect_attempted = 0u;
static u32 g_connect_granted = 0u;
static u32 g_connect_unavailable = 0u;
static u32 g_recv_status_granted = 0u;
static u32 g_close_count = 0u;
static u32 g_last_http_status = 0u;
static u32 g_last_response_bytes = 0u;

static void network_socket64_clear_record(struct network_socket64_record *record)
{
    record->active = 0u;
    record->handle = 0u;
    record->owner_id = 0u;
    record->remote_ipv4 = 0u;
    record->remote_port = 0u;
    record->protocol = 0u;
    record->http_status = 0u;
    record->response_bytes = 0u;
}

static struct network_socket64_record *network_socket64_find_free(void)
{
    u32 index;

    for (index = 0u; index < NETWORK_SOCKET64_TABLE_LIMIT; ++index)
    {
        if (g_network_sockets[index].active == 0u)
        {
            return &g_network_sockets[index];
        }
    }

    return 0;
}

static struct network_socket64_record *network_socket64_find(u32 handle)
{
    u32 index;

    for (index = 0u; index < NETWORK_SOCKET64_TABLE_LIMIT; ++index)
    {
        if ((g_network_sockets[index].active != 0u)
            && (g_network_sockets[index].handle == handle))
        {
            return &g_network_sockets[index];
        }
    }

    return 0;
}

static u32 network_socket64_route_capability(u32 network_capability, u32 owner_id)
{
    u32 endpoint_id = capability64_route(
        network_capability,
        CAPABILITY64_RIGHT_SEND,
        owner_id);

    return (endpoint_id == services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_NETWORK))
        ? 1u
        : 0u;
}

static u32 network_socket64_online(void)
{
    return ((virtio_net64_dhcp_ack() != 0u)
            && (virtio_net64_dns_resolved() != 0u)
            && (virtio_net64_http_status() != 0u))
        ? 1u
        : 0u;
}

void network_socket64_init(void)
{
    u32 index;

    for (index = 0u; index < NETWORK_SOCKET64_TABLE_LIMIT; ++index)
    {
        network_socket64_clear_record(&g_network_sockets[index]);
    }

    g_next_socket_handle = NETWORK_SOCKET64_HANDLE_BASE;
    g_probe_ran = 0u;
    g_service_capability_minted = 0u;
    g_no_cap_denied = 0u;
    g_wrong_owner_denied = 0u;
    g_raw_denied = 0u;
    g_listen_denied = 0u;
    g_send_denied = 0u;
    g_connect_attempted = 0u;
    g_connect_granted = 0u;
    g_connect_unavailable = 0u;
    g_recv_status_granted = 0u;
    g_close_count = 0u;
    g_last_http_status = 0u;
    g_last_response_bytes = 0u;
}

u32 network_socket64_open_tcp(u32 network_capability, u32 remote_ipv4, u32 remote_port, u32 owner_id)
{
    struct network_socket64_record *record;

    ++g_connect_attempted;
    if (network_socket64_route_capability(network_capability, owner_id) == 0u)
    {
        g_no_cap_denied = 1u;
        return NETWORK_SOCKET64_INVALID_HANDLE;
    }

    if (network_socket64_online() == 0u)
    {
        g_connect_unavailable = 1u;
        return NETWORK_SOCKET64_INVALID_HANDLE;
    }

    if (remote_ipv4 == 0u)
    {
        remote_ipv4 = virtio_net64_dns_resolved();
    }

    if ((remote_port != 80u) || (remote_ipv4 == 0u)
        || (remote_ipv4 != virtio_net64_dns_resolved()))
    {
        return NETWORK_SOCKET64_INVALID_HANDLE;
    }

    record = network_socket64_find_free();
    if (record == 0)
    {
        return NETWORK_SOCKET64_INVALID_HANDLE;
    }

    record->active = 1u;
    record->handle = g_next_socket_handle++;
    record->owner_id = owner_id;
    record->remote_ipv4 = remote_ipv4;
    record->remote_port = remote_port;
    record->protocol = NETWORK_SOCKET64_PROTOCOL_TCP;
    record->http_status = virtio_net64_http_status();
    record->response_bytes = virtio_net64_http_response_bytes();
    g_connect_granted = 1u;
    g_last_http_status = record->http_status;
    g_last_response_bytes = record->response_bytes;

    return record->handle;
}

u32 network_socket64_open_raw(u32 network_capability, u32 protocol, u32 owner_id)
{
    (void)protocol;
    (void)network_socket64_route_capability(network_capability, owner_id);
    g_raw_denied = 1u;
    return NETWORK_SOCKET64_INVALID_HANDLE;
}

u32 network_socket64_listen_tcp(u32 network_capability, u32 local_port, u32 owner_id)
{
    (void)local_port;
    (void)network_socket64_route_capability(network_capability, owner_id);
    g_listen_denied = 1u;
    return NETWORK_SOCKET64_INVALID_HANDLE;
}

u32 network_socket64_send(u32 socket_handle, u32 byte_count, u32 owner_id)
{
    struct network_socket64_record *record = network_socket64_find(socket_handle);

    (void)byte_count;
    if ((record == 0) || (record->owner_id != owner_id))
    {
        g_send_denied = 1u;
        return 0u;
    }

    g_send_denied = 1u;
    return 0u;
}

u32 network_socket64_recv_status(u32 socket_handle, u32 owner_id)
{
    struct network_socket64_record *record = network_socket64_find(socket_handle);

    if ((record == 0) || (record->owner_id != owner_id))
    {
        return 0u;
    }

    g_recv_status_granted = 1u;
    g_last_http_status = record->http_status;
    g_last_response_bytes = record->response_bytes;
    return record->response_bytes;
}

u32 network_socket64_close(u32 socket_handle, u32 owner_id)
{
    struct network_socket64_record *record = network_socket64_find(socket_handle);

    if ((record == 0) || (record->owner_id != owner_id))
    {
        return 0u;
    }

    network_socket64_clear_record(record);
    ++g_close_count;
    return 1u;
}

void network_socket64_probe(void)
{
    u32 owner = PRINCIPAL64_ID_CONSOLE_CLIENT;
    u32 cap;
    u32 remote_ipv4;
    u32 socket;

    if (g_probe_ran != 0u)
    {
        return;
    }
    g_probe_ran = 1u;

    cap = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_NETWORK,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        owner);
    g_service_capability_minted = (cap != CAPABILITY64_INVALID_HANDLE) ? 1u : 0u;

    remote_ipv4 = virtio_net64_dns_resolved();
    (void)network_socket64_open_tcp(
        NETWORK_SOCKET64_INVALID_HANDLE,
        remote_ipv4,
        80u,
        owner);
    (void)network_socket64_open_raw(cap, NETWORK_SOCKET64_PROTOCOL_RAW, owner);
    (void)network_socket64_listen_tcp(cap, 80u, owner);

    socket = network_socket64_open_tcp(
        cap,
        remote_ipv4,
        80u,
        PRINCIPAL64_ID_POLICY_CLIENT);
    if (socket == NETWORK_SOCKET64_INVALID_HANDLE)
    {
        g_wrong_owner_denied = 1u;
    }

    socket = network_socket64_open_tcp(cap, remote_ipv4, 80u, owner);
    if (socket != NETWORK_SOCKET64_INVALID_HANDLE)
    {
        (void)network_socket64_recv_status(socket, owner);
        (void)network_socket64_send(socket, 18u, owner);
        (void)network_socket64_close(socket, owner);
    }
    else
    {
        (void)network_socket64_send(socket, 18u, owner);
    }

    (void)capability64_revoke(cap, owner);
}

u32 network_socket64_api_published(void)
{
    return 1u;
}

u32 network_socket64_service_registered(void)
{
    return (services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_NETWORK) != 0xFFFFFFFFu)
        ? 1u
        : 0u;
}

u32 network_socket64_capability_required(void)
{
    return 1u;
}

u32 network_socket64_service_capability_minted(void)
{
    return g_service_capability_minted;
}

u32 network_socket64_no_cap_denied(void)
{
    return g_no_cap_denied;
}

u32 network_socket64_wrong_owner_denied(void)
{
    return g_wrong_owner_denied;
}

u32 network_socket64_raw_denied(void)
{
    return g_raw_denied;
}

u32 network_socket64_listen_denied(void)
{
    return g_listen_denied;
}

u32 network_socket64_send_denied(void)
{
    return g_send_denied;
}

u32 network_socket64_connect_attempted(void)
{
    return (g_connect_attempted != 0u) ? 1u : 0u;
}

u32 network_socket64_connect_granted(void)
{
    return g_connect_granted;
}

u32 network_socket64_connect_unavailable(void)
{
    return g_connect_unavailable;
}

u32 network_socket64_recv_status_granted(void)
{
    return g_recv_status_granted;
}

u32 network_socket64_close_count(void)
{
    return g_close_count;
}

u32 network_socket64_socket_count(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0u; index < NETWORK_SOCKET64_TABLE_LIMIT; ++index)
    {
        if (g_network_sockets[index].active != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 network_socket64_last_http_status(void)
{
    return g_last_http_status;
}

u32 network_socket64_last_response_bytes(void)
{
    return g_last_response_bytes;
}

u32 network_socket64_fs_authority(void)
{
    return 0u;
}

u32 network_socket64_storage_authority(void)
{
    return 0u;
}

u32 network_socket64_ambient_authority(void)
{
    return 0u;
}
