#include "services.h"

#include "ipc.h"
#include "klog.h"
#include "memory.h"
#include "paging.h"
#include "pit.h"
#include "scheduler.h"
#include "userspace.h"

struct service_state
{
    u32 heartbeat_count;
    u32 request_log_count;
    u32 approval_log_count;
    u32 dependency_probe_sent;
    u32 user_services_spawned;
};

struct service_record
{
    u32 id;
    const char *name;
    u32 endpoint_class;
    u32 endpoint_id;
    u32 delegable;
    u32 capability_mask;
    u32 denied_ipc_count;
    struct scheduler_task *task;
};

struct service_graph_edge
{
    u32 source_endpoint;
    u32 destination_endpoint;
    u32 count;
    u32 max_dependency_depth;
    u32 flags_seen;
};

struct policy_source_window
{
    u32 source_endpoint;
    u32 last_accept_tick;
};

enum
{
    SERVICE_RECORD_COUNT = 7,
    SERVICE_GRAPH_EDGE_LIMIT = 16,
    POLICY_SOURCE_WINDOW_LIMIT = 8,
    POLICY_SOURCE_BURST_TICKS = 3
};

static struct service_state init_state;
static struct service_state driver_state;
static struct service_state ai_policy_state;
static struct service_state telemetry_state;
static struct service_record services[SERVICE_RECORD_COUNT];
static struct service_graph_edge service_graph_edges[SERVICE_GRAPH_EDGE_LIMIT];
static struct policy_source_window policy_source_windows[POLICY_SOURCE_WINDOW_LIMIT];
static u32 core_service_count = 0;
static u32 broker_inversion_boosts = 0;
static u32 broker_backpressure_denials = 0;
static u32 dependency_chain_denials = 0;
static u32 service_graph_edge_count = 0;
static u32 service_graph_max_dependency_depth = 0;
static u32 policy_queue_high_water = 0;
static u32 current_message_flags = 0;
static u32 current_message_dependency_depth = 0;

static struct service_record *services_find_by_endpoint(u32 endpoint_id)
{
    u32 index;

    for (index = 0; index < SERVICE_RECORD_COUNT; ++index)
    {
        if (services[index].endpoint_id == endpoint_id)
        {
            return &services[index];
        }
    }

    return NULL;
}

static struct service_record *services_find_by_class(u32 endpoint_class)
{
    u32 index;

    for (index = 0; index < SERVICE_RECORD_COUNT; ++index)
    {
        if (services[index].endpoint_class == endpoint_class)
        {
            return &services[index];
        }
    }

    return NULL;
}

static const struct service_record *services_find_by_task(const struct scheduler_task *task)
{
    u32 index;

    if (task == NULL)
    {
        return NULL;
    }

    for (index = 0; index < SERVICE_RECORD_COUNT; ++index)
    {
        if (services[index].task == task)
        {
            return &services[index];
        }
    }

    return NULL;
}

static const struct service_record *services_current_record(void)
{
    return services_find_by_task(scheduler_current_task());
}

static struct policy_source_window *services_find_policy_source_window(u32 source_endpoint)
{
    u32 index;
    struct policy_source_window *available = NULL;

    for (index = 0; index < POLICY_SOURCE_WINDOW_LIMIT; ++index)
    {
        if (policy_source_windows[index].source_endpoint == source_endpoint)
        {
            return &policy_source_windows[index];
        }

        if ((available == NULL) && (policy_source_windows[index].source_endpoint == 0u))
        {
            available = &policy_source_windows[index];
        }
    }

    return available;
}

static struct service_graph_edge *services_find_graph_edge(u32 source_endpoint, u32 destination_endpoint)
{
    u32 index;
    struct service_graph_edge *available = NULL;

    for (index = 0; index < SERVICE_GRAPH_EDGE_LIMIT; ++index)
    {
        if ((service_graph_edges[index].source_endpoint == source_endpoint)
            && (service_graph_edges[index].destination_endpoint == destination_endpoint))
        {
            return &service_graph_edges[index];
        }

        if ((available == NULL)
            && (service_graph_edges[index].source_endpoint == 0u)
            && (service_graph_edges[index].destination_endpoint == 0u)
            && (service_graph_edges[index].count == 0u))
        {
            available = &service_graph_edges[index];
        }
    }

    return available;
}

static const char *services_backpressure_reason_name(u32 reason)
{
    switch (reason)
    {
        case SERVICE_BACKPRESSURE_REASON_PENDING:
            return "pending";

        case SERVICE_BACKPRESSURE_REASON_RESERVED:
            return "reserve";

        case SERVICE_BACKPRESSURE_REASON_BURST:
            return "burst";

        default:
            return "unknown";
    }
}

static void services_register(
    u32 slot,
    u32 id,
    const char *name,
    u32 priority,
    u32 endpoint_class,
    u32 endpoint_id,
    u32 delegable,
    u32 capability_mask,
    u32 start_delay_ticks,
    u32 period_ticks,
    scheduler_task_callback callback,
    void *context)
{
    services[slot].id = id;
    services[slot].name = name;
    services[slot].endpoint_class = endpoint_class;
    services[slot].endpoint_id = endpoint_id;
    services[slot].delegable = delegable;
    services[slot].capability_mask = capability_mask;
    services[slot].denied_ipc_count = 0;
    services[slot].task = scheduler_register_periodic_priority(
        name,
        priority,
        start_delay_ticks,
        period_ticks,
        callback,
        context);
}

static void services_register_event(
    u32 slot,
    u32 id,
    const char *name,
    u32 priority,
    u32 endpoint_class,
    u32 endpoint_id,
    u32 delegable,
    u32 capability_mask,
    u32 start_delay_ticks,
    scheduler_task_callback callback,
    void *context)
{
    services[slot].id = id;
    services[slot].name = name;
    services[slot].endpoint_class = endpoint_class;
    services[slot].endpoint_id = endpoint_id;
    services[slot].delegable = delegable;
    services[slot].capability_mask = capability_mask;
    services[slot].denied_ipc_count = 0;
    services[slot].task = scheduler_register_event_priority(
        name,
        priority,
        start_delay_ticks,
        callback,
        context);
}

static void services_register_static(
    u32 slot,
    u32 id,
    const char *name,
    u32 endpoint_class,
    u32 endpoint_id,
    u32 delegable,
    u32 capability_mask)
{
    services[slot].id = id;
    services[slot].name = name;
    services[slot].endpoint_class = endpoint_class;
    services[slot].endpoint_id = endpoint_id;
    services[slot].delegable = delegable;
    services[slot].capability_mask = capability_mask;
    services[slot].denied_ipc_count = 0;
    services[slot].task = NULL;
}

static void service_init_supervisor(struct scheduler_task *task)
{
    struct service_state *state = (struct service_state *)task->context;
    struct ipc_message message;
    u32 policy_endpoint;
    ++state->heartbeat_count;

    if (state->heartbeat_count == 1u)
    {
        klog_write_string("[service:init] supervising ");
        klog_write_dec_u32(services_core_count());
        klog_write_line(" bootstrap services");

        message.source_endpoint = 0;
        message.type = IPC_MESSAGE_POLICY_HANDSHAKE;
        message.arg0 = 1;
        message.arg1 = 0;
        message.flags = 0u;
        message.dependency_depth = 1u;
        policy_endpoint = services_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_AI_POLICY);

        if ((policy_endpoint != 0xFFFFFFFFu)
            && (ipc_send(policy_endpoint, &message) == IPC_STATUS_OK))
        {
            klog_write_line("[service:init] queued policy handshake");
        }
    }

    while (ipc_receive(IPC_ENDPOINT_INIT, &message) == IPC_STATUS_OK)
    {
        services_begin_message_context(&message);

        if (message.type == IPC_MESSAGE_POLICY_APPROVED)
        {
            klog_write_line("[service:init] policy handshake approved");

            if (state->user_services_spawned == 0u)
            {
                u32 process_id;

                userspace_note_bootstrap_policy_approved();

                process_id = userspace_spawn_builtin(USERSPACE_EXECUTABLE_SESSION_SHELL);
                if (process_id != 0xFFFFFFFFu)
                {
                    klog_write_string("[service:init] launched session-shell pid ");
                    klog_write_dec_u32(process_id);
                    klog_newline();
                }

                process_id = userspace_spawn_builtin(USERSPACE_EXECUTABLE_AUTOMATION_WORKER);
                if (process_id != 0xFFFFFFFFu)
                {
                    klog_write_string("[service:init] launched automation-worker pid ");
                    klog_write_dec_u32(process_id);
                    klog_newline();
                }

                process_id = userspace_spawn_builtin(USERSPACE_EXECUTABLE_SESSION_SHELL);
                if (process_id == 0xFFFFFFFFu)
                {
                    klog_write_line("[service:init] duplicate session-shell denied");
                }

                state->user_services_spawned = 1u;
            }

            if (state->dependency_probe_sent == 0u)
            {
                message.source_endpoint = 0u;
                message.type = IPC_MESSAGE_POLICY_HANDSHAKE;
                message.arg0 = 3u;
                message.arg1 = 0u;
                message.flags = IPC_MESSAGE_FLAG_DEPENDENCY_PROBE;
                message.dependency_depth = IPC_DEPENDENCY_DEPTH_LIMIT;
                policy_endpoint = services_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_AI_POLICY);

                if ((policy_endpoint != 0xFFFFFFFFu)
                    && (ipc_send(policy_endpoint, &message) == IPC_STATUS_OK))
                {
                    state->dependency_probe_sent = 1u;
                    klog_write_string("[service:init] queued dependency probe depth ");
                    klog_write_dec_u32(IPC_DEPENDENCY_DEPTH_LIMIT);
                    klog_newline();
                }
            }
        }

        services_clear_message_context();
    }

    scheduler_wait_current();
}

static void service_driver_host(struct scheduler_task *task)
{
    struct service_state *state = (struct service_state *)task->context;
    struct ipc_message message;
    u32 policy_endpoint;
    ++state->heartbeat_count;

    if (state->heartbeat_count == 1u)
    {
        klog_write_line("[service:drivers] isolated driver host broker active");

        message.source_endpoint = 0;
        message.type = IPC_MESSAGE_POLICY_HANDSHAKE;
        message.arg0 = 2;
        message.arg1 = 0;
        message.flags = 0u;
        message.dependency_depth = 1u;
        policy_endpoint = services_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_AI_POLICY);

        if ((policy_endpoint != 0xFFFFFFFFu)
            && (ipc_send(policy_endpoint, &message) == IPC_STATUS_ACCESS_DENIED))
        {
            klog_write_line("[service:drivers] policy escalation denied");
        }
    }

    scheduler_wait_current();
}

static void service_ai_policy(struct scheduler_task *task)
{
    struct service_state *state = (struct service_state *)task->context;
    struct ipc_message message;
    u32 payload_words[1];

    ++state->heartbeat_count;
    if (state->heartbeat_count == 1u)
    {
        klog_write_line("[service:ai-policy] consent gate active");
    }

    while (ipc_receive(IPC_ENDPOINT_AI_POLICY, &message) == 0)
    {
        services_begin_message_context(&message);

        if (!userspace_is_endpoint(message.source_endpoint) || (state->request_log_count < 4u))
        {
            klog_write_string("[service:ai-policy] request from endpoint ");
            klog_write_dec_u32(message.source_endpoint);
            if (message.dependency_depth != 0u)
            {
                klog_write_string(" depth ");
                klog_write_dec_u32(message.dependency_depth);
            }
            klog_write_line(" queued for approval");
        }

        if (userspace_is_endpoint(message.source_endpoint))
        {
            ++state->request_log_count;
        }

        if (message.type == IPC_MESSAGE_POLICY_HANDSHAKE)
        {
            if (userspace_is_endpoint(message.source_endpoint))
            {
                payload_words[0] = message.arg0;
                if (userspace_deliver_message(
                        message.source_endpoint,
                        IPC_ENDPOINT_AI_POLICY,
                        IPC_MESSAGE_POLICY_APPROVED,
                        payload_words,
                        1u) == IPC_STATUS_OK)
                {
                    if (state->approval_log_count < 4u)
                    {
                        klog_write_string("[service:ai-policy] approved request for endpoint ");
                        klog_write_dec_u32(message.source_endpoint);
                        klog_newline();
                    }

                    ++state->approval_log_count;
                }
            }
            else
            {
                struct ipc_message response;
                s32 status;

                response.source_endpoint = 0;
                response.type = IPC_MESSAGE_POLICY_APPROVED;
                response.arg0 = message.source_endpoint;
                response.arg1 = 0;
                response.flags = 0u;
                response.dependency_depth = 0u;

                status = ipc_send(IPC_ENDPOINT_INIT, &response);
                if (status == IPC_STATUS_OK)
                {
                    klog_write_string("[service:ai-policy] approved request for endpoint ");
                    klog_write_dec_u32(message.source_endpoint);
                    klog_newline();
                }
                else if (status == IPC_STATUS_ACCESS_DENIED)
                {
                    klog_write_string("[service:ai-policy] denied chain depth ");
                    klog_write_dec_u32(message.dependency_depth + 1u);
                    klog_write_string(" -> init");
                    klog_newline();
                }
            }
        }

        services_clear_message_context();
    }

    scheduler_wait_current();
}

static void service_telemetry(struct scheduler_task *task)
{
    struct service_state *state = (struct service_state *)task->context;

    ++state->heartbeat_count;

    if (state->heartbeat_count <= 3u)
    {
        klog_write_string("[telemetry] uptime ");
        klog_write_dec_u32(pit_get_uptime_seconds());
        klog_write_string("s tasks ");
        klog_write_dec_u32(scheduler_task_count());
        klog_write_string(" runs ");
        klog_write_dec_u32(scheduler_total_runs());
        klog_write_string(" fair ");
        klog_write_dec_u32(scheduler_total_fairness_boosts());
        klog_write_string(" urgent ");
        klog_write_dec_u32(scheduler_total_urgent_wakes());
        klog_write_string(" ready ");
        klog_write_dec_u32(scheduler_ready_count());
        klog_write_string(" waiting ");
        klog_write_dec_u32(scheduler_waiting_count());
        klog_write_string(" p4 ");
        klog_write_dec_u32(scheduler_priority_run_count(SCHEDULER_PRIORITY_CRITICAL));
        klog_write_string(" p3 ");
        klog_write_dec_u32(scheduler_priority_run_count(SCHEDULER_PRIORITY_HIGH));
        klog_write_string(" p2 ");
        klog_write_dec_u32(scheduler_priority_run_count(SCHEDULER_PRIORITY_NORMAL));
        klog_write_string(" p0 ");
        klog_write_dec_u32(scheduler_priority_run_count(SCHEDULER_PRIORITY_BACKGROUND));
        klog_write_string(" denied-ipc ");
        klog_write_dec_u32(services_total_denied_ipc_count());
        klog_write_string(" chain-denials ");
        klog_write_dec_u32(services_total_dependency_denial_count());
        klog_write_string(" user-syscalls ");
        klog_write_dec_u32(userspace_syscall_count());
        klog_write_string(" user-execs ");
        klog_write_dec_u32(userspace_executable_count());
        klog_write_string(" user-manifests ");
        klog_write_dec_u32(userspace_package_manifest_count());
        klog_write_string(" pkg-loads ");
        klog_write_dec_u32(userspace_total_package_load_count());
        klog_write_string(" pkg-rejects ");
        klog_write_dec_u32(userspace_total_package_rejection_count());
        klog_write_string(" signer-verifies ");
        klog_write_dec_u32(userspace_total_signer_verification_count());
        klog_write_string(" signer-denials ");
        klog_write_dec_u32(userspace_total_signer_denial_count());
        klog_write_string(" manifest-verifies ");
        klog_write_dec_u32(userspace_total_manifest_verification_count());
        klog_write_string(" manifest-denials ");
        klog_write_dec_u32(userspace_total_manifest_denial_count());
        klog_write_string(" user-procs ");
        klog_write_dec_u32(userspace_process_count());
        klog_write_string(" user-dispatches ");
        klog_write_dec_u32(userspace_total_dispatch_count());
        klog_write_string(" user-latency-picks ");
        klog_write_dec_u32(userspace_total_latency_pick_count());
        klog_write_string(" user-deadline-picks ");
        klog_write_dec_u32(userspace_total_deadline_pick_count());
        klog_write_string(" user-io-wakes ");
        klog_write_dec_u32(userspace_total_io_wake_count());
        klog_write_string(" user-budget-throttles ");
        klog_write_dec_u32(userspace_total_budget_throttle_count());
        klog_write_string(" interactive-waits ");
        klog_write_dec_u32(userspace_interactive_policy_waiter_count());
        klog_write_string(" broker-boosts ");
        klog_write_dec_u32(broker_inversion_boosts);
        klog_write_string(" broker-backpressure ");
        klog_write_dec_u32(broker_backpressure_denials);
        klog_write_string(" graph-edges ");
        klog_write_dec_u32(service_graph_edge_count);
        klog_write_string(" graph-depth ");
        klog_write_dec_u32(service_graph_max_dependency_depth);
        klog_write_string(" broker-qmax ");
        klog_write_dec_u32(policy_queue_high_water);
        klog_write_string(" user-endpoints ");
        klog_write_dec_u32(userspace_registered_endpoint_count());
        klog_write_string(" user-sleeps ");
        klog_write_dec_u32(userspace_total_sleep_count());
        klog_write_string(" user-ipc-sends ");
        klog_write_dec_u32(userspace_total_ipc_send_count());
        klog_write_string(" user-ipc-denied ");
        klog_write_dec_u32(userspace_total_ipc_denied_count());
        klog_write_string(" user-ipc-waits ");
        klog_write_dec_u32(userspace_total_ipc_wait_count());
        klog_write_string(" user-ipc-msgs ");
        klog_write_dec_u32(userspace_total_ipc_message_count());
        klog_write_string(" user-cap-grants ");
        klog_write_dec_u32(userspace_total_capability_grant_count());
        klog_write_string(" user-cap-revokes ");
        klog_write_dec_u32(userspace_total_capability_revoke_count());
        klog_write_string(" user-cap-delegations ");
        klog_write_dec_u32(userspace_total_capability_delegation_count());
        klog_write_string(" user-cap-expirations ");
        klog_write_dec_u32(userspace_total_capability_expiration_count());
        klog_write_string(" user-cap-admission-denials ");
        klog_write_dec_u32(userspace_total_capability_admission_denial_count());
        klog_write_string(" user-cap-reuses ");
        klog_write_dec_u32(userspace_total_capability_reuse_count());
        klog_write_string(" user-cap-compactions ");
        klog_write_dec_u32(userspace_total_capability_compaction_count());
        klog_write_string(" user-buffers ");
        klog_write_dec_u32(userspace_total_buffer_registration_count());
        klog_write_string(" user-buffer-copies ");
        klog_write_dec_u32(userspace_total_buffer_copy_count());
        klog_write_string(" user-exits ");
        klog_write_dec_u32(userspace_total_process_exit_count());
        klog_write_string(" user-console-writes ");
        klog_write_dec_u32(userspace_total_console_write_count());
        klog_write_string(" user-input-reads ");
        klog_write_dec_u32(userspace_total_input_read_count());
        klog_write_string(" user-fs-opens ");
        klog_write_dec_u32(userspace_total_fs_open_count());
        klog_write_string(" user-fs-creates ");
        klog_write_dec_u32(userspace_total_fs_create_count());
        klog_write_string(" user-fs-lists ");
        klog_write_dec_u32(userspace_total_fs_list_count());
        klog_write_string(" user-fs-reads ");
        klog_write_dec_u32(userspace_total_fs_read_count());
        klog_write_string(" user-fs-stats ");
        klog_write_dec_u32(userspace_total_fs_stat_count());
        klog_write_string(" user-fs-renames ");
        klog_write_dec_u32(userspace_total_fs_rename_count());
        klog_write_string(" user-fs-moves ");
        klog_write_dec_u32(userspace_total_fs_move_count());
        klog_write_string(" user-fs-deletes ");
        klog_write_dec_u32(userspace_total_fs_delete_count());
        klog_write_string(" user-fs-writes ");
        klog_write_dec_u32(userspace_total_fs_write_count());
        klog_write_string(" user-policy-denials ");
        klog_write_dec_u32(userspace_total_policy_denial_count());
        klog_write_string(" user-yields ");
        klog_write_dec_u32(userspace_total_yield_count());
        klog_write_string(" user-preempts ");
        klog_write_dec_u32(userspace_total_preemption_count());
        klog_write_string(" free-frames ");
        klog_write_dec_u32(memory_get_free_frame_count());
        klog_write_string(" mapped ");
        klog_write_dec_u32(paging_get_mapped_bytes() / (1024u * 1024u));
        klog_write_line(" MiB");
    }
}

void services_init(void)
{
    services_register_event(0, SERVICE_ID_INIT, "init-supervisor", SCHEDULER_PRIORITY_HIGH,
        SERVICE_ENDPOINT_CLASS_INIT, IPC_ENDPOINT_INIT, 0u,
        SERVICE_CAP_ROUTE_POLICY | SERVICE_CAP_ROUTE_DRIVER | SERVICE_CAP_SUPERVISE | SERVICE_CAP_AUDIT,
        0, service_init_supervisor, &init_state);
    services_register_event(1, SERVICE_ID_DRIVER_HOST, "driver-host", SCHEDULER_PRIORITY_NORMAL,
        SERVICE_ENDPOINT_CLASS_DRIVER_HOST, IPC_ENDPOINT_DRIVER_HOST, 0u,
        SERVICE_CAP_ROUTE_DRIVER, 0, service_driver_host, &driver_state);
    services_register_event(2, SERVICE_ID_AI_POLICY, "ai-policy", SCHEDULER_PRIORITY_CRITICAL,
        SERVICE_ENDPOINT_CLASS_AI_POLICY, IPC_ENDPOINT_AI_POLICY, 1u,
        SERVICE_CAP_ROUTE_POLICY | SERVICE_CAP_ROUTE_INIT | SERVICE_CAP_AUDIT,
        0, service_ai_policy, &ai_policy_state);
    services_register(3, SERVICE_ID_TELEMETRY, "telemetry", SCHEDULER_PRIORITY_BACKGROUND,
        SERVICE_ENDPOINT_CLASS_NONE, 0, 0u,
        SERVICE_CAP_READ_TELEMETRY | SERVICE_CAP_AUDIT, 100, 100, service_telemetry, &telemetry_state);
    services_register_static(4, SERVICE_ID_CONSOLE, "console", SERVICE_ENDPOINT_CLASS_CONSOLE, 4u, 1u,
        SERVICE_CAP_CONSOLE | SERVICE_CAP_AUDIT);
    services_register_static(5, SERVICE_ID_RAMFS, "ramfs", SERVICE_ENDPOINT_CLASS_RAMFS, 5u, 0u,
        SERVICE_CAP_RAMFS | SERVICE_CAP_AUDIT);
    services_register_static(6, SERVICE_ID_INPUT, "input", SERVICE_ENDPOINT_CLASS_INPUT, 6u, 1u,
        SERVICE_CAP_INPUT | SERVICE_CAP_AUDIT);
    core_service_count = scheduler_task_count();
}

u32 services_core_count(void)
{
    return core_service_count;
}

u32 services_service_count(void)
{
    return SERVICE_RECORD_COUNT;
}

u32 services_current_service_id(void)
{
    const struct service_record *record = services_current_record();
    return (record == NULL) ? SERVICE_ID_NONE : record->id;
}

u32 services_current_endpoint(void)
{
    const struct service_record *record = services_current_record();
    return (record == NULL) ? 0u : record->endpoint_id;
}

u32 services_current_capabilities(void)
{
    const struct service_record *record = services_current_record();
    return (record == NULL) ? 0u : record->capability_mask;
}

const char *services_current_name(void)
{
    const struct service_record *record = services_current_record();
    return (record == NULL) ? "kernel" : record->name;
}

u32 services_resolve_endpoint_class(u32 endpoint_class)
{
    const struct service_record *record = services_find_by_class(endpoint_class);

    if ((record == NULL) || (record->endpoint_id == 0u))
    {
        return 0xFFFFFFFFu;
    }

    return record->endpoint_id;
}

int services_endpoint_exists(u32 endpoint_id)
{
    return services_find_by_endpoint(endpoint_id) != NULL;
}

const char *services_endpoint_name(u32 endpoint_id)
{
    const struct service_record *record = services_find_by_endpoint(endpoint_id);
    return (record == NULL) ? "unknown-service-endpoint" : record->name;
}

int services_endpoint_is_delegable(u32 endpoint_id)
{
    const struct service_record *record = services_find_by_endpoint(endpoint_id);

    return (record != NULL) && (record->delegable != 0u);
}

u32 services_capabilities_for_endpoint(u32 endpoint_id)
{
    struct service_record *record = services_find_by_endpoint(endpoint_id);
    return (record == NULL) ? 0u : record->capability_mask;
}

u32 services_total_denied_ipc_count(void)
{
    u32 index;
    u32 total = 0;

    for (index = 0; index < SERVICE_RECORD_COUNT; ++index)
    {
        total += services[index].denied_ipc_count;
    }

    return total;
}

u32 services_total_dependency_denial_count(void)
{
    return dependency_chain_denials;
}

u32 services_total_backpressure_count(void)
{
    return broker_backpressure_denials;
}

u32 services_total_graph_edge_count(void)
{
    return service_graph_edge_count;
}

u32 services_max_dependency_depth_seen(void)
{
    return service_graph_max_dependency_depth;
}

u32 services_policy_queue_high_water(void)
{
    return policy_queue_high_water;
}

u32 services_denied_ipc_count_for_endpoint(u32 endpoint_id)
{
    struct service_record *record = services_find_by_endpoint(endpoint_id);
    return (record == NULL) ? 0u : record->denied_ipc_count;
}

int services_current_can_receive_endpoint(u32 endpoint_id)
{
    const struct service_record *record = services_current_record();

    if (record == NULL)
    {
        return 1;
    }

    if (record->endpoint_id == endpoint_id)
    {
        return 1;
    }

    return (record->capability_mask & SERVICE_CAP_SUPERVISE) != 0u;
}

void services_note_ipc_denied(u32 source_endpoint, u32 destination_endpoint)
{
    struct service_record *record = services_find_by_endpoint(source_endpoint);

    (void)destination_endpoint;

    if (record != NULL)
    {
        ++record->denied_ipc_count;
    }
}

void services_note_dependency_denied(u32 source_endpoint, u32 destination_endpoint, u32 dependency_depth)
{
    (void)source_endpoint;
    (void)destination_endpoint;
    (void)dependency_depth;
    ++dependency_chain_denials;

    if (dependency_depth > service_graph_max_dependency_depth)
    {
        service_graph_max_dependency_depth = dependency_depth;
    }
}

u32 services_backpressure_reason_for_ipc(
    u32 source_endpoint,
    u32 destination_endpoint,
    const struct ipc_message *message,
    u32 queue_depth,
    u32 queue_capacity)
{
    struct policy_source_window *window;
    u32 now;

    (void)queue_depth;
    (void)queue_capacity;

    if ((message == NULL)
        || (destination_endpoint != IPC_ENDPOINT_AI_POLICY)
        || !userspace_is_endpoint(source_endpoint)
        || ((message->flags & IPC_MESSAGE_FLAG_INTERACTIVE_WAIT) != 0u)
        || ((message->flags & IPC_MESSAGE_FLAG_DEPENDENCY_PROBE) != 0u))
    {
        return 0u;
    }

    window = services_find_policy_source_window(source_endpoint);
    if ((window == NULL) || (window->source_endpoint == 0u))
    {
        return 0u;
    }

    now = pit_get_ticks();
    if ((now - window->last_accept_tick) < POLICY_SOURCE_BURST_TICKS)
    {
        return SERVICE_BACKPRESSURE_REASON_BURST;
    }

    return 0u;
}

void services_note_ipc_routed(
    u32 source_endpoint,
    u32 destination_endpoint,
    const struct ipc_message *message,
    u32 queue_depth,
    u32 queue_capacity)
{
    struct service_graph_edge *edge;
    struct policy_source_window *window;
    u32 dependency_depth = (message == NULL) ? 0u : message->dependency_depth;
    u32 flags = (message == NULL) ? 0u : message->flags;

    if ((destination_endpoint == IPC_ENDPOINT_AI_POLICY) && (queue_depth > policy_queue_high_water))
    {
        policy_queue_high_water = queue_depth;

        if (policy_queue_high_water <= 3u)
        {
            klog_write_string("[service:ai-policy] queue high-water ");
            klog_write_dec_u32(queue_depth);
            klog_write_string("/");
            klog_write_dec_u32(queue_capacity);
            klog_newline();
        }
    }

    if (dependency_depth > service_graph_max_dependency_depth)
    {
        service_graph_max_dependency_depth = dependency_depth;
    }

    if ((destination_endpoint == IPC_ENDPOINT_AI_POLICY)
        && userspace_is_endpoint(source_endpoint)
        && ((flags & IPC_MESSAGE_FLAG_INTERACTIVE_WAIT) == 0u)
        && ((flags & IPC_MESSAGE_FLAG_DEPENDENCY_PROBE) == 0u))
    {
        window = services_find_policy_source_window(source_endpoint);
        if (window != NULL)
        {
            window->source_endpoint = source_endpoint;
            window->last_accept_tick = pit_get_ticks();
        }
    }

    edge = services_find_graph_edge(source_endpoint, destination_endpoint);
    if (edge == NULL)
    {
        return;
    }

    if (edge->count == 0u)
    {
        edge->source_endpoint = source_endpoint;
        edge->destination_endpoint = destination_endpoint;
        ++service_graph_edge_count;

        if (service_graph_edge_count <= 6u)
        {
            klog_write_string("[graph] route ");
            klog_write_dec_u32(source_endpoint);
            klog_write_string(" -> ");
            klog_write_dec_u32(destination_endpoint);
            if (dependency_depth != 0u)
            {
                klog_write_string(" depth ");
                klog_write_dec_u32(dependency_depth);
            }
            klog_newline();
        }
    }

    ++edge->count;
    if (dependency_depth > edge->max_dependency_depth)
    {
        edge->max_dependency_depth = dependency_depth;
    }

    edge->flags_seen |= flags;
}

void services_note_backpressure(
    u32 source_endpoint,
    u32 destination_endpoint,
    u32 queue_depth,
    u32 queue_capacity,
    u32 reason)
{
    (void)destination_endpoint;
    ++broker_backpressure_denials;

    if (broker_backpressure_denials <= 6u)
    {
        klog_write_string("[service:ai-policy] backpressure source ");
        klog_write_dec_u32(source_endpoint);
        klog_write_string(" reason ");
        klog_write_string(services_backpressure_reason_name(reason));
        klog_write_string(" queue ");
        klog_write_dec_u32(queue_depth);
        klog_write_string("/");
        klog_write_dec_u32(queue_capacity);
        klog_newline();
    }
}

void services_begin_message_context(const struct ipc_message *message)
{
    if (message == NULL)
    {
        current_message_flags = 0u;
        current_message_dependency_depth = 0u;
        return;
    }

    current_message_flags = message->flags;
    current_message_dependency_depth = message->dependency_depth;
}

void services_clear_message_context(void)
{
    current_message_flags = 0u;
    current_message_dependency_depth = 0u;
}

u32 services_current_dependency_depth(void)
{
    return current_message_dependency_depth;
}

u32 services_current_message_flags(void)
{
    return current_message_flags;
}

void services_wake_endpoint_owner(u32 endpoint_id)
{
    struct service_record *record = services_find_by_endpoint(endpoint_id);

    if ((record != NULL) && (record->task != NULL))
    {
        if ((endpoint_id == IPC_ENDPOINT_AI_POLICY)
            && (userspace_interactive_policy_waiter_count() != 0u))
        {
            scheduler_wake_task_urgent(record->task);
            ++broker_inversion_boosts;

            if (broker_inversion_boosts <= 4u)
            {
                klog_write_string("[service:ai-policy] inversion boost waiters ");
                klog_write_dec_u32(userspace_interactive_policy_waiter_count());
                klog_newline();
            }

            return;
        }

        scheduler_wake_task(record->task);
    }
}
