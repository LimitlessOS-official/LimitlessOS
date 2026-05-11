#include "ipc.h"

#include "memory.h"
#include "services.h"
#include "userspace.h"

struct ipc_endpoint
{
    u32 id;
    const char *name;
    u32 flags;
    u32 required_send_capabilities;
    struct ipc_message *queue;
    u32 queue_capacity;
    u32 queue_head;
    u32 queue_tail;
    u32 queue_count;
};

enum
{
    IPC_BOOT_ENDPOINTS = 3,
    IPC_BOOT_QUEUE_CAPACITY = 8
};

static struct ipc_endpoint endpoints[IPC_BOOT_ENDPOINTS];

static int ipc_endpoint_has_pending_source(const struct ipc_endpoint *endpoint, u32 source_endpoint)
{
    u32 index;
    u32 queue_index;

    if ((endpoint == NULL) || (source_endpoint == 0u) || (endpoint->queue_count == 0u))
    {
        return 0;
    }

    queue_index = endpoint->queue_head;
    for (index = 0; index < endpoint->queue_count; ++index)
    {
        if (endpoint->queue[queue_index].source_endpoint == source_endpoint)
        {
            return 1;
        }

        queue_index = (queue_index + 1u) % endpoint->queue_capacity;
    }

    return 0;
}

static struct ipc_endpoint *ipc_find_endpoint(u32 endpoint_id)
{
    u32 index;

    for (index = 0; index < IPC_BOOT_ENDPOINTS; ++index)
    {
        if (endpoints[index].id == endpoint_id)
        {
            return &endpoints[index];
        }
    }

    return NULL;
}

static void ipc_configure_endpoint(
    u32 slot,
    u32 id,
    const char *name,
    u32 flags,
    u32 required_send_capabilities)
{
    endpoints[slot].id = id;
    endpoints[slot].name = name;
    endpoints[slot].flags = flags;
    endpoints[slot].required_send_capabilities = required_send_capabilities;
    endpoints[slot].queue_capacity = IPC_BOOT_QUEUE_CAPACITY;
    endpoints[slot].queue = (struct ipc_message *)memory_early_alloc(
        sizeof(struct ipc_message) * IPC_BOOT_QUEUE_CAPACITY,
        16);
}

void ipc_init(void)
{
    ipc_configure_endpoint(0, IPC_ENDPOINT_INIT, "init", IPC_ENDPOINT_FLAG_KERNEL_OWNED,
        SERVICE_CAP_ROUTE_INIT);
    ipc_configure_endpoint(1, IPC_ENDPOINT_DRIVER_HOST, "driver-host", IPC_ENDPOINT_FLAG_KERNEL_OWNED,
        SERVICE_CAP_ROUTE_DRIVER);
    ipc_configure_endpoint(2, IPC_ENDPOINT_AI_POLICY, "ai-policy-broker",
        IPC_ENDPOINT_FLAG_KERNEL_OWNED | IPC_ENDPOINT_FLAG_POLICY_GUARDED,
        SERVICE_CAP_ROUTE_POLICY);
}

u32 ipc_endpoint_count(void)
{
    return IPC_BOOT_ENDPOINTS;
}

const char *ipc_endpoint_name(u32 endpoint_id)
{
    struct ipc_endpoint *endpoint = ipc_find_endpoint(endpoint_id);

    if (endpoint == NULL)
    {
        return "unknown";
    }

    return endpoint->name;
}

s32 ipc_send(u32 endpoint_id, const struct ipc_message *message)
{
    struct ipc_endpoint *endpoint = ipc_find_endpoint(endpoint_id);
    struct ipc_message queued_message;
    u32 backpressure_reason;
    u32 source_endpoint;
    u32 source_capabilities;

    if ((endpoint == NULL) || (endpoint->queue == NULL))
    {
        return IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    source_endpoint = services_current_endpoint();
    if (source_endpoint == 0u)
    {
        source_endpoint = message->source_endpoint;
    }

    source_capabilities = services_current_capabilities();
    if (source_capabilities == 0u)
    {
        source_capabilities = services_capabilities_for_endpoint(source_endpoint);
    }

    if ((source_capabilities == 0u) && userspace_is_endpoint(source_endpoint))
    {
        if ((endpoint_id != IPC_ENDPOINT_AI_POLICY)
            || (message->type != IPC_MESSAGE_POLICY_HANDSHAKE))
        {
            return IPC_STATUS_ACCESS_DENIED;
        }

        source_capabilities = SERVICE_CAP_ROUTE_POLICY;
    }

    if ((endpoint->required_send_capabilities != 0u)
        && ((source_capabilities & endpoint->required_send_capabilities) == 0u))
    {
        services_note_ipc_denied(source_endpoint, endpoint_id);
        return IPC_STATUS_ACCESS_DENIED;
    }

    queued_message = *message;
    queued_message.source_endpoint = source_endpoint;

    if ((queued_message.flags == 0u) && (services_current_message_flags() != 0u))
    {
        queued_message.flags = services_current_message_flags();
    }

    if ((queued_message.dependency_depth == 0u)
        && (services_current_dependency_depth() != 0u))
    {
        queued_message.dependency_depth = services_current_dependency_depth() + 1u;
    }

    if (queued_message.dependency_depth > IPC_DEPENDENCY_DEPTH_LIMIT)
    {
        services_note_dependency_denied(source_endpoint, endpoint_id, queued_message.dependency_depth);
        return IPC_STATUS_ACCESS_DENIED;
    }

    if (endpoint->queue_count == endpoint->queue_capacity)
    {
        if (endpoint_id == IPC_ENDPOINT_AI_POLICY)
        {
            services_note_backpressure(
                source_endpoint,
                endpoint_id,
                endpoint->queue_count,
                endpoint->queue_capacity,
                SERVICE_BACKPRESSURE_REASON_RESERVED);
        }

        return IPC_STATUS_QUEUE_FULL;
    }

    if ((endpoint_id == IPC_ENDPOINT_AI_POLICY)
        && ipc_endpoint_has_pending_source(endpoint, source_endpoint))
    {
        services_note_backpressure(
            source_endpoint,
            endpoint_id,
            endpoint->queue_count,
            endpoint->queue_capacity,
            SERVICE_BACKPRESSURE_REASON_PENDING);
        return IPC_STATUS_QUEUE_FULL;
    }

    if ((endpoint_id == IPC_ENDPOINT_AI_POLICY)
        && ((endpoint->queue_count + 1u) >= endpoint->queue_capacity)
        && ((queued_message.flags & IPC_MESSAGE_FLAG_INTERACTIVE_WAIT) == 0u))
    {
        services_note_backpressure(
            source_endpoint,
            endpoint_id,
            endpoint->queue_count,
            endpoint->queue_capacity,
            SERVICE_BACKPRESSURE_REASON_RESERVED);
        return IPC_STATUS_QUEUE_FULL;
    }

    backpressure_reason = services_backpressure_reason_for_ipc(
        source_endpoint,
        endpoint_id,
        &queued_message,
        endpoint->queue_count,
        endpoint->queue_capacity);
    if (backpressure_reason != 0u)
    {
        services_note_backpressure(
            source_endpoint,
            endpoint_id,
            endpoint->queue_count,
            endpoint->queue_capacity,
            backpressure_reason);
        return IPC_STATUS_QUEUE_FULL;
    }

    endpoint->queue[endpoint->queue_tail] = queued_message;
    endpoint->queue_tail = (endpoint->queue_tail + 1) % endpoint->queue_capacity;
    ++endpoint->queue_count;
    services_note_ipc_routed(
        source_endpoint,
        endpoint_id,
        &queued_message,
        endpoint->queue_count,
        endpoint->queue_capacity);
    services_wake_endpoint_owner(endpoint_id);
    return IPC_STATUS_OK;
}

s32 ipc_receive(u32 endpoint_id, struct ipc_message *message)
{
    struct ipc_endpoint *endpoint = ipc_find_endpoint(endpoint_id);

    if ((endpoint == NULL) || (endpoint->queue == NULL))
    {
        return IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    if (!services_current_can_receive_endpoint(endpoint_id))
    {
        return IPC_STATUS_NOT_OWNER;
    }

    if (endpoint->queue_count == 0)
    {
        return IPC_STATUS_EMPTY;
    }

    *message = endpoint->queue[endpoint->queue_head];
    endpoint->queue_head = (endpoint->queue_head + 1) % endpoint->queue_capacity;
    --endpoint->queue_count;
    return IPC_STATUS_OK;
}
