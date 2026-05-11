#ifndef LIMITLESS_SERVICES_H
#define LIMITLESS_SERVICES_H

#include "scheduler.h"
#include "types.h"

struct ipc_message;

enum service_id
{
    SERVICE_ID_NONE = 0,
    SERVICE_ID_INIT = 1,
    SERVICE_ID_DRIVER_HOST = 2,
    SERVICE_ID_AI_POLICY = 3,
    SERVICE_ID_TELEMETRY = 4,
    SERVICE_ID_CONSOLE = 5,
    SERVICE_ID_RAMFS = 6,
    SERVICE_ID_INPUT = 7,
    SERVICE_ID_DISPLAY = 8,
    SERVICE_ID_BLOCK = 9,
    SERVICE_ID_HARDWARE = 10
};

enum service_endpoint_class
{
    SERVICE_ENDPOINT_CLASS_NONE = 0,
    SERVICE_ENDPOINT_CLASS_INIT = 1,
    SERVICE_ENDPOINT_CLASS_DRIVER_HOST = 2,
    SERVICE_ENDPOINT_CLASS_AI_POLICY = 3,
    SERVICE_ENDPOINT_CLASS_CONSOLE = 4,
    SERVICE_ENDPOINT_CLASS_RAMFS = 5,
    SERVICE_ENDPOINT_CLASS_INPUT = 6,
    SERVICE_ENDPOINT_CLASS_DISPLAY = 7,
    SERVICE_ENDPOINT_CLASS_BLOCK = 8,
    SERVICE_ENDPOINT_CLASS_HARDWARE = 9
};

enum service_capability
{
    SERVICE_CAP_ROUTE_INIT = 0x00000001u,
    SERVICE_CAP_ROUTE_DRIVER = 0x00000002u,
    SERVICE_CAP_ROUTE_POLICY = 0x00000004u,
    SERVICE_CAP_SUPERVISE = 0x00000008u,
    SERVICE_CAP_READ_TELEMETRY = 0x00000010u,
    SERVICE_CAP_AUDIT = 0x00000020u,
    SERVICE_CAP_CONSOLE = 0x00000040u,
    SERVICE_CAP_RAMFS = 0x00000080u,
    SERVICE_CAP_INPUT = 0x00000100u,
    SERVICE_CAP_DISPLAY = 0x00000200u,
    SERVICE_CAP_BLOCK = 0x00000400u,
    SERVICE_CAP_HARDWARE = 0x00000800u
};

#define SERVICE_BACKPRESSURE_REASON_PENDING 1u
#define SERVICE_BACKPRESSURE_REASON_RESERVED 2u
#define SERVICE_BACKPRESSURE_REASON_BURST 3u

void services_init(void);
u32 services_core_count(void);
u32 services_service_count(void);
u32 services_current_service_id(void);
u32 services_current_endpoint(void);
u32 services_current_capabilities(void);
const char *services_current_name(void);
u32 services_resolve_endpoint_class(u32 endpoint_class);
int services_endpoint_exists(u32 endpoint_id);
const char *services_endpoint_name(u32 endpoint_id);
int services_endpoint_is_delegable(u32 endpoint_id);
u32 services_capabilities_for_endpoint(u32 endpoint_id);
u32 services_total_denied_ipc_count(void);
u32 services_denied_ipc_count_for_endpoint(u32 endpoint_id);
int services_current_can_receive_endpoint(u32 endpoint_id);
void services_note_ipc_denied(u32 source_endpoint, u32 destination_endpoint);
void services_note_dependency_denied(u32 source_endpoint, u32 destination_endpoint, u32 dependency_depth);
u32 services_total_dependency_denial_count(void);
void services_note_ipc_routed(
    u32 source_endpoint,
    u32 destination_endpoint,
    const struct ipc_message *message,
    u32 queue_depth,
    u32 queue_capacity);
u32 services_backpressure_reason_for_ipc(
    u32 source_endpoint,
    u32 destination_endpoint,
    const struct ipc_message *message,
    u32 queue_depth,
    u32 queue_capacity);
void services_note_backpressure(
    u32 source_endpoint,
    u32 destination_endpoint,
    u32 queue_depth,
    u32 queue_capacity,
    u32 reason);
u32 services_total_backpressure_count(void);
u32 services_total_graph_edge_count(void);
u32 services_max_dependency_depth_seen(void);
u32 services_policy_queue_high_water(void);
void services_begin_message_context(const struct ipc_message *message);
void services_clear_message_context(void);
u32 services_current_dependency_depth(void);
u32 services_current_message_flags(void);
void services_wake_endpoint_owner(u32 endpoint_id);

#endif
