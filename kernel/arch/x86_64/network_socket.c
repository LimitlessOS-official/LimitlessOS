#include "network_socket_x64.h"

#include "capability_x64.h"
#include "principal_x64.h"
#include "services.h"
#include "services_x64.h"
#include "virtio_net_x64.h"

#define NETWORK_SOCKET64_HANDLE_BASE 0x534B0000u
#define NETWORK_SOCKET64_TABLE_LIMIT 4u
#define NETWORK_SOCKET64_CURL_MAX_BYTES 4096u

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
static u32 g_curl_attempted = 0u;
static u32 g_curl_cap_minted = 0u;
static u32 g_curl_non_delegable_denied = 0u;
static u32 g_curl_dns_resolved = 0u;
static u32 g_curl_tcp_connect = 0u;
static u32 g_curl_http_get = 0u;
static u32 g_curl_response_bytes = 0u;
static u32 g_curl_truncated = 0u;
static u32 g_curl_close = 0u;
static u32 g_curl_cap_destroyed = 0u;
static u32 g_curl_url_denied = 0u;
static u32 g_curl_error = 0u;

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

static u32 network_socket64_text_length(const char *text)
{
    u32 length = 0u;

    if (text == 0)
    {
        return 0u;
    }

    while (text[length] != '\0')
    {
        ++length;
    }

    return length;
}

static u32 network_socket64_url_equals(const u8 *url, u32 url_bytes, const char *expected)
{
    u32 index;
    u32 expected_bytes = network_socket64_text_length(expected);

    if ((url == 0) || (expected == 0) || (url_bytes != expected_bytes))
    {
        return 0u;
    }

    for (index = 0u; index < expected_bytes; ++index)
    {
        if (url[index] != (u8)expected[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 network_socket64_url_allowed(const u8 *url, u32 url_bytes)
{
    return (network_socket64_url_equals(url, url_bytes, "example.com") != 0u)
        || (network_socket64_url_equals(url, url_bytes, "example.com/") != 0u)
        || (network_socket64_url_equals(url, url_bytes, "http://example.com") != 0u)
        || (network_socket64_url_equals(url, url_bytes, "http://example.com/") != 0u);
}

static void network_socket64_reset_curl_state(void)
{
    g_curl_attempted = 0u;
    g_curl_cap_minted = 0u;
    g_curl_non_delegable_denied = 0u;
    g_curl_dns_resolved = 0u;
    g_curl_tcp_connect = 0u;
    g_curl_http_get = 0u;
    g_curl_response_bytes = 0u;
    g_curl_truncated = 0u;
    g_curl_close = 0u;
    g_curl_cap_destroyed = 0u;
    g_curl_url_denied = 0u;
    g_curl_error = 0u;
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
    network_socket64_reset_curl_state();
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

static u32 network_socket64_recv_http_response(
    u32 socket_handle,
    u32 network_capability,
    u32 owner_id,
    u8 *destination,
    u32 destination_capacity,
    u32 *bytes_read)
{
    struct network_socket64_record *record = network_socket64_find(socket_handle);
    u32 copy_capacity;
    u32 copied;

    if (bytes_read != 0)
    {
        *bytes_read = 0u;
    }

    if ((record == 0) || (record->owner_id != owner_id)
        || (network_socket64_route_capability(network_capability, owner_id) == 0u)
        || (destination == 0) || (destination_capacity == 0u))
    {
        return 0u;
    }

    copy_capacity = (destination_capacity < NETWORK_SOCKET64_CURL_MAX_BYTES)
        ? destination_capacity
        : NETWORK_SOCKET64_CURL_MAX_BYTES;
    copied = virtio_net64_http_copy_response(destination, copy_capacity);
    if (copied == 0u)
    {
        return 0u;
    }

    if (bytes_read != 0)
    {
        *bytes_read = copied;
    }
    g_last_http_status = record->http_status;
    g_last_response_bytes = record->response_bytes;

    return 1u;
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

u32 network_socket64_curl_http(
    const u8 *url,
    u32 url_bytes,
    u8 *destination,
    u32 destination_capacity,
    u32 owner_id,
    u32 *bytes_read)
{
    u32 cap;
    u32 delegated;
    u32 socket;
    u32 copied = 0u;
    u32 result = 0u;

    network_socket64_reset_curl_state();
    g_curl_attempted = 1u;
    if (bytes_read != 0)
    {
        *bytes_read = 0u;
    }

    if ((url == 0) || (url_bytes == 0u) || (destination == 0) || (destination_capacity == 0u))
    {
        g_curl_error = 1u;
        return 0u;
    }

    if (network_socket64_url_allowed(url, url_bytes) == 0u)
    {
        g_curl_url_denied = 1u;
        g_curl_error = 2u;
        return 0u;
    }

    cap = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_NETWORK,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        owner_id);
    if (cap == CAPABILITY64_INVALID_HANDLE)
    {
        g_curl_error = 3u;
        return 0u;
    }
    g_curl_cap_minted = 1u;

    delegated = capability64_delegate_persistent(
        cap,
        CAPABILITY64_RIGHT_SEND,
        CAPABILITY64_CONTEXT(owner_id, PRINCIPAL64_ID_POLICY_CLIENT));
    if (delegated == CAPABILITY64_INVALID_HANDLE)
    {
        g_curl_non_delegable_denied = 1u;
    }

    g_curl_dns_resolved = ((virtio_net64_dns_response() != 0u)
            && (virtio_net64_dns_resolved() != 0u))
        ? 1u
        : 0u;

    socket = network_socket64_open_tcp(cap, virtio_net64_dns_resolved(), 80u, owner_id);
    if (socket == NETWORK_SOCKET64_INVALID_HANDLE)
    {
        g_curl_error = 4u;
    }
    else
    {
        g_curl_tcp_connect = 1u;
        g_curl_http_get = ((virtio_net64_http_sent() != 0u)
                && (virtio_net64_http_status() != 0u))
            ? 1u
            : 0u;

        if (network_socket64_recv_http_response(
                socket,
                cap,
                owner_id,
                destination,
                destination_capacity,
                &copied) != 0u)
        {
            if (bytes_read != 0)
            {
                *bytes_read = copied;
            }
            g_curl_response_bytes = copied;
            g_curl_truncated = (virtio_net64_http_response_bytes() > copied) ? 1u : 0u;
            result = 1u;
        }
        else
        {
            g_curl_error = 5u;
        }

        if (network_socket64_close(socket, owner_id) != 0u)
        {
            g_curl_close = 1u;
        }
    }

    if (capability64_revoke(cap, owner_id) != 0u)
    {
        g_curl_cap_destroyed = 1u;
    }

    return result;
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

u32 network_socket64_curl_attempted(void)
{
    return g_curl_attempted;
}

u32 network_socket64_curl_cap_minted(void)
{
    return g_curl_cap_minted;
}

u32 network_socket64_curl_non_delegable_denied(void)
{
    return g_curl_non_delegable_denied;
}

u32 network_socket64_curl_dns_resolved(void)
{
    return g_curl_dns_resolved;
}

u32 network_socket64_curl_tcp_connect(void)
{
    return g_curl_tcp_connect;
}

u32 network_socket64_curl_http_get(void)
{
    return g_curl_http_get;
}

u32 network_socket64_curl_response_bytes(void)
{
    return g_curl_response_bytes;
}

u32 network_socket64_curl_truncated(void)
{
    return g_curl_truncated;
}

u32 network_socket64_curl_close(void)
{
    return g_curl_close;
}

u32 network_socket64_curl_cap_destroyed(void)
{
    return g_curl_cap_destroyed;
}

u32 network_socket64_curl_url_denied(void)
{
    return g_curl_url_denied;
}

u32 network_socket64_curl_error(void)
{
    return g_curl_error;
}
