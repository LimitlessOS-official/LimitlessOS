#include "process_x64.h"

#include "launch_x64.h"
#include "principal_x64.h"
#include "services.h"
#include "services_x64.h"

struct process64_record
{
    u32 pid;
    const char *name;
    u32 principal_id;
    u32 endpoint_class;
    u32 state;
    u32 scheduler_class;
    u32 capability_limit;
    u32 manifest_index;
    u32 manifest_package_id;
    u32 manifest_executable_id;
    u32 manifest_signer_id;
    u32 manifest_token;
};

enum
{
    PROCESS64_RECORD_COUNT = 7
};

static struct process64_record g_processes[PROCESS64_RECORD_COUNT] = {
    {
        1u,
        "init-supervisor",
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        SERVICE_ENDPOINT_CLASS_INIT,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_SYSTEM,
        16u,
        LAUNCH64_INVALID_MANIFEST,
        0u,
        0u,
        0u,
        0u
    },
    {
        2u,
        "ai-policy",
        PRINCIPAL64_ID_POLICY_WORKER,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_POLICY,
        12u,
        LAUNCH64_INVALID_MANIFEST,
        0u,
        0u,
        0u,
        0u
    },
    {
        3u,
        "driver-host",
        PRINCIPAL64_ID_DRIVER_HOST,
        SERVICE_ENDPOINT_CLASS_DRIVER_HOST,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_IO,
        8u,
        LAUNCH64_INVALID_MANIFEST,
        0u,
        0u,
        0u,
        0u
    },
    {
        4u,
        "console",
        PRINCIPAL64_ID_CONSOLE_WORKER,
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_INTERACTIVE,
        10u,
        LAUNCH64_INVALID_MANIFEST,
        0u,
        0u,
        0u,
        0u
    },
    {
        5u,
        "ramfs",
        PRINCIPAL64_ID_RAMFS_WORKER,
        SERVICE_ENDPOINT_CLASS_RAMFS,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_IO,
        10u,
        LAUNCH64_INVALID_MANIFEST,
        0u,
        0u,
        0u,
        0u
    },
    {
        6u,
        "input",
        PRINCIPAL64_ID_INPUT_WORKER,
        SERVICE_ENDPOINT_CLASS_INPUT,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_INTERACTIVE,
        8u,
        LAUNCH64_INVALID_MANIFEST,
        0u,
        0u,
        0u,
        0u
    },
    {
        7u,
        "telemetry",
        PRINCIPAL64_ID_TELEMETRY,
        SERVICE_ENDPOINT_CLASS_NONE,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_BACKGROUND,
        4u,
        LAUNCH64_INVALID_MANIFEST,
        0u,
        0u,
        0u,
        0u
    }
};

static u32 g_process_init = 0u;
static u32 g_manifest_verified_count = 0u;

static void process64_bind_manifests(void)
{
    u32 index;

    g_manifest_verified_count = 0u;

    for (index = 0u; index < PROCESS64_RECORD_COUNT; ++index)
    {
        u32 manifest_index = launch64_manifest_by_process(g_processes[index].name);

        g_processes[index].manifest_index = manifest_index;
        g_processes[index].manifest_package_id = 0u;
        g_processes[index].manifest_executable_id = 0u;
        g_processes[index].manifest_signer_id = 0u;
        g_processes[index].manifest_token = 0u;
        g_processes[index].state &= ~(PROCESS64_STATE_MANIFEST_VERIFIED | PROCESS64_STATE_LAUNCH_STARTED);

        if (manifest_index == LAUNCH64_INVALID_MANIFEST)
        {
            continue;
        }

        if ((launch64_manifest_scheduler_class(manifest_index) != g_processes[index].scheduler_class)
            || (launch64_manifest_capability_limit(manifest_index) != g_processes[index].capability_limit))
        {
            continue;
        }

        if (principal64_is_active(g_processes[index].principal_id) == 0u)
        {
            continue;
        }

        if (launch64_request_service_start(
                PRINCIPAL64_ID_INIT_SUPERVISOR,
                manifest_index,
                g_processes[index].pid,
                g_processes[index].principal_id,
                g_processes[index].endpoint_class,
                g_processes[index].scheduler_class,
                g_processes[index].capability_limit) == 0u)
        {
            continue;
        }

        g_processes[index].manifest_package_id = launch64_manifest_package_id(manifest_index);
        g_processes[index].manifest_executable_id = launch64_manifest_executable_id(manifest_index);
        g_processes[index].manifest_signer_id = launch64_manifest_signer_id(manifest_index);
        g_processes[index].manifest_token = launch64_manifest_token(manifest_index);
        g_processes[index].state |= PROCESS64_STATE_MANIFEST_VERIFIED | PROCESS64_STATE_LAUNCH_STARTED;
        ++g_manifest_verified_count;
    }
}

static void process64_ensure_init(void)
{
    if (g_process_init != 0u)
    {
        return;
    }

    principal64_init();
    services64_init();
    launch64_init();
    process64_bind_manifests();
    g_process_init = 1u;
}

static const struct process64_record *process64_find(u32 pid)
{
    u32 index;

    process64_ensure_init();

    for (index = 0u; index < PROCESS64_RECORD_COUNT; ++index)
    {
        if (g_processes[index].pid == pid)
        {
            return &g_processes[index];
        }
    }

    return 0;
}

void process64_init(void)
{
    process64_ensure_init();
}

u32 process64_count(void)
{
    process64_ensure_init();
    return PROCESS64_RECORD_COUNT;
}

u32 process64_pid_by_index(u32 index)
{
    process64_ensure_init();

    if (index >= PROCESS64_RECORD_COUNT)
    {
        return PROCESS64_INVALID_PID;
    }

    return g_processes[index].pid;
}

u32 process64_pid_for_principal(u32 principal_id)
{
    u32 index;

    process64_ensure_init();

    if (principal64_is_active(principal_id) == 0u)
    {
        return PROCESS64_INVALID_PID;
    }

    for (index = 0u; index < PROCESS64_RECORD_COUNT; ++index)
    {
        if (g_processes[index].principal_id == principal_id)
        {
            return g_processes[index].pid;
        }
    }

    return PROCESS64_INVALID_PID;
}

u32 process64_principal(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->principal_id : 0u;
}

u32 process64_endpoint(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->endpoint_class == SERVICE_ENDPOINT_CLASS_NONE))
    {
        return 0xFFFFFFFFu;
    }

    return services64_resolve_endpoint_class(record->endpoint_class);
}

u32 process64_endpoint_class(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->endpoint_class : SERVICE_ENDPOINT_CLASS_NONE;
}

u32 process64_state(u32 pid)
{
    const struct process64_record *record = process64_find(pid);
    u32 state;

    if (record == 0)
    {
        return 0u;
    }

    state = record->state;
    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_generation(record->manifest_index) > 1u))
    {
        state |= PROCESS64_STATE_RESTARTED;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_image_plan_token(record->manifest_index) != 0u))
    {
        state |= PROCESS64_STATE_IMAGE_PLANNED;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_image_map_token(record->manifest_index) != 0u))
    {
        state |= PROCESS64_STATE_IMAGE_MAPPED;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_entry_transfer_token(record->manifest_index) != 0u))
    {
        state |= PROCESS64_STATE_ENTRY_READY;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_image_map_installed(record->manifest_index) != 0u))
    {
        state |= PROCESS64_STATE_IMAGE_MAP_INSTALLED;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_image_protection_token(record->manifest_index) != 0u))
    {
        state |= PROCESS64_STATE_IMAGE_PROTECTED;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_user_entry_token(record->manifest_index) != 0u))
    {
        state |= PROCESS64_STATE_USER_ENTRY_PLANNED;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && ((launch64_manifest_runtime_user_entry_state(record->manifest_index)
                & LAUNCH64_USER_ENTRY_BLOCKED) != 0u))
    {
        state |= PROCESS64_STATE_USER_ENTRY_BLOCKED;
    }

    if ((record->manifest_index != LAUNCH64_INVALID_MANIFEST)
        && ((launch64_manifest_runtime_user_entry_state(record->manifest_index)
                & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        state |= PROCESS64_STATE_USER_ENTRY_READY;
    }

    return state;
}

u32 process64_scheduler_class(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->scheduler_class : 0u;
}

u32 process64_capability_limit(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->capability_limit : 0u;
}

u32 process64_manifest_index(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->manifest_index : LAUNCH64_INVALID_MANIFEST;
}

u32 process64_manifest_package_id(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->manifest_package_id : 0u;
}

u32 process64_manifest_executable_id(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->manifest_executable_id : 0u;
}

u32 process64_manifest_signer_id(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->manifest_signer_id : 0u;
}

u32 process64_manifest_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->manifest_token : 0u;
}

u32 process64_runtime_generation(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_generation(record->manifest_index);
}

u32 process64_runtime_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_token(record->manifest_index);
}

u32 process64_runtime_image_generation(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_generation(record->manifest_index);
}

u32 process64_runtime_image_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_token(record->manifest_index);
}

u32 process64_runtime_image_base(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_base(record->manifest_index);
}

u32 process64_runtime_image_entry(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_entry(record->manifest_index);
}

u32 process64_runtime_image_mapped_bytes(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_mapped_bytes(record->manifest_index);
}

u32 process64_runtime_image_rights(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_rights(record->manifest_index);
}

u32 process64_runtime_image_plan_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_plan_token(record->manifest_index);
}

u32 process64_runtime_image_map_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_map_token(record->manifest_index);
}

u32 process64_runtime_image_page_count(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_page_count(record->manifest_index);
}

u32 process64_runtime_image_pml4_index(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_pml4_index(record->manifest_index);
}

u32 process64_runtime_image_pdpt_index(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_pdpt_index(record->manifest_index);
}

u32 process64_runtime_image_pd_index(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_pd_index(record->manifest_index);
}

u32 process64_runtime_entry_transfer_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_entry_transfer_token(record->manifest_index);
}

u32 process64_runtime_image_install_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_install_token(record->manifest_index);
}

u32 process64_runtime_image_source_checksum(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_source_checksum(record->manifest_index);
}

u32 process64_runtime_image_entry_probe(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_entry_probe(record->manifest_index);
}

u32 process64_runtime_image_map_installed(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_map_installed(record->manifest_index);
}

u32 process64_runtime_image_protection_flags(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_protection_flags(record->manifest_index);
}

u32 process64_runtime_image_protection_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_image_protection_token(record->manifest_index);
}

u32 process64_runtime_user_entry_state(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_user_entry_state(record->manifest_index);
}

u32 process64_runtime_user_entry_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_user_entry_token(record->manifest_index);
}

u32 process64_runtime_user_entry_rip(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_user_entry_rip(record->manifest_index);
}

u32 process64_runtime_user_entry_rsp(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_user_entry_rsp(record->manifest_index);
}

u32 process64_runtime_user_entry_selectors(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_user_entry_selectors(record->manifest_index);
}

u32 process64_runtime_user_entry_rflags(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_user_entry_rflags(record->manifest_index);
}

u32 process64_runtime_user_entry_denial(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return LAUNCH64_USER_ENTRY_DENY_DESCRIPTOR_STATE;
    }

    return launch64_manifest_runtime_user_entry_denial(record->manifest_index);
}

u32 process64_runtime_user_entry_ready(u32 pid)
{
    return ((process64_runtime_user_entry_state(pid) & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u)
        ? 1u
        : 0u;
}

u32 process64_runtime_payload_offset(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_payload_offset(record->manifest_index);
}

u32 process64_runtime_payload_size(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_payload_size(record->manifest_index);
}

u32 process64_runtime_payload_checksum(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    if ((record == 0) || (record->manifest_index == LAUNCH64_INVALID_MANIFEST))
    {
        return 0u;
    }

    return launch64_manifest_runtime_payload_checksum(record->manifest_index);
}

u32 process64_manifest_verified_count(void)
{
    process64_ensure_init();
    return g_manifest_verified_count;
}

const char *process64_name(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->name : "unknown";
}
