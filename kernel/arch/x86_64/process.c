#include "process_x64.h"

#include "launch_x64.h"
#include "principal_x64.h"
#include "services.h"
#include "services_x64.h"

/*
 * A.0 adds opaque per-process extension slots for upcoming VMA, FD, persona,
 * and audit state while preserving the private process64_record boundary.
 * It integrates with process_x64.h accessors and the scaffold diagnostic; the
 * checkpoint proves fresh records expose NULL extension slots without changing
 * existing manifest/process behavior.
 */

enum
{
    PROCESS64_RECORD_COUNT = 7,
    PROCESS64_NAME_BYTES = 32
#ifdef LIMITLESS_X64_UEFI_KERNEL
    ,
    PROCESS64_CLONE_RECORD_COUNT = 4
#endif
};

struct process64_record
{
    u32 pid;
    char name[PROCESS64_NAME_BYTES];
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
    void *vma_root;
    void *fd_table;
    void *persona_ctx;
    void *audit_ctx;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u64 page_root_physical;
    u32 page_root_index;
    u32 page_root_token;
#endif
};

struct process64_seed_record
{
    u32 pid;
    char name[PROCESS64_NAME_BYTES];
    u32 principal_id;
    u32 endpoint_class;
    u32 state;
    u32 scheduler_class;
    u32 capability_limit;
};

static const char g_process64_unknown_name[] = "unknown";

static const struct process64_seed_record g_process_seeds[PROCESS64_RECORD_COUNT] = {
    {
        1u,
        "init-supervisor",
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        SERVICE_ENDPOINT_CLASS_INIT,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_SYSTEM,
        16u
    },
    {
        2u,
        "ai-policy",
        PRINCIPAL64_ID_POLICY_WORKER,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_POLICY,
        12u
    },
    {
        3u,
        "driver-host",
        PRINCIPAL64_ID_DRIVER_HOST,
        SERVICE_ENDPOINT_CLASS_DRIVER_HOST,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_IO,
        8u
    },
    {
        4u,
        "console",
        PRINCIPAL64_ID_CONSOLE_WORKER,
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_INTERACTIVE,
        10u
    },
    {
        5u,
        "ramfs",
        PRINCIPAL64_ID_RAMFS_WORKER,
        SERVICE_ENDPOINT_CLASS_RAMFS,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_IO,
        10u
    },
    {
        6u,
        "input",
        PRINCIPAL64_ID_INPUT_WORKER,
        SERVICE_ENDPOINT_CLASS_INPUT,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_INTERACTIVE,
        8u
    },
    {
        7u,
        "telemetry",
        PRINCIPAL64_ID_TELEMETRY,
        SERVICE_ENDPOINT_CLASS_NONE,
        PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_SERVICE | PROCESS64_STATE_READY | PROCESS64_STATE_SEALED,
        PROCESS64_CLASS_BACKGROUND,
        4u
    }
};

static struct process64_record g_processes[PROCESS64_RECORD_COUNT];
#ifdef LIMITLESS_X64_UEFI_KERNEL
static struct process64_record g_process64_clone_records[PROCESS64_CLONE_RECORD_COUNT];
static u32 g_process64_clone_count = 0u;
static u32 g_process64_next_clone_pid = PROCESS64_CLONE_PID_BASE;
static u32 g_process64_page_root_attach_count = 0u;
static u32 g_process64_page_root_clear_count = 0u;
static u32 g_process64_page_root_denial_count = 0u;
#endif
static u32 g_process_init = 0u;
static u32 g_manifest_verified_count = 0u;

static void process64_copy_name(char *target, const char *source)
{
    u32 index;

    for (index = 0u; index < PROCESS64_NAME_BYTES; ++index)
    {
        target[index] = source[index];
        if (source[index] == '\0')
        {
            ++index;
            break;
        }
    }

    while (index < PROCESS64_NAME_BYTES)
    {
        target[index] = '\0';
        ++index;
    }
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
static void process64_clear_record(struct process64_record *record)
{
    if (record == 0)
    {
        return;
    }

    record->pid = PROCESS64_INVALID_PID;
    process64_copy_name(record->name, g_process64_unknown_name);
    record->principal_id = 0u;
    record->endpoint_class = SERVICE_ENDPOINT_CLASS_NONE;
    record->state = 0u;
    record->scheduler_class = 0u;
    record->capability_limit = 0u;
    record->manifest_index = LAUNCH64_INVALID_MANIFEST;
    record->manifest_package_id = 0u;
    record->manifest_executable_id = 0u;
    record->manifest_signer_id = 0u;
    record->manifest_token = 0u;
    record->vma_root = 0;
    record->fd_table = 0;
    record->persona_ctx = 0;
    record->audit_ctx = 0;
    record->page_root_physical = 0ull;
    record->page_root_index = 0xFFFFFFFFu;
    record->page_root_token = 0u;
}

static u32 process64_clone_record_active(const struct process64_record *record)
{
    return ((record != 0)
        && (record->pid != PROCESS64_INVALID_PID)
        && (record->pid != 0u))
        ? 1u
        : 0u;
}

static void process64_clear_clone_records(void)
{
    u32 index;

    for (index = 0u; index < PROCESS64_CLONE_RECORD_COUNT; ++index)
    {
        process64_clear_record(&g_process64_clone_records[index]);
    }

    g_process64_clone_count = 0u;
    g_process64_next_clone_pid = PROCESS64_CLONE_PID_BASE;
}
#endif

static void process64_seed_record(u32 index, u32 preserve_extensions)
{
    void *vma_root = (preserve_extensions != 0u) ? g_processes[index].vma_root : 0;
    void *fd_table = (preserve_extensions != 0u) ? g_processes[index].fd_table : 0;
    void *persona_ctx = (preserve_extensions != 0u) ? g_processes[index].persona_ctx : 0;
    void *audit_ctx = (preserve_extensions != 0u) ? g_processes[index].audit_ctx : 0;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u64 page_root_physical =
        (preserve_extensions != 0u) ? g_processes[index].page_root_physical : 0ull;
    u32 page_root_index =
        (preserve_extensions != 0u) ? g_processes[index].page_root_index : 0xFFFFFFFFu;
    u32 page_root_token =
        (preserve_extensions != 0u) ? g_processes[index].page_root_token : 0u;
#endif

    g_processes[index].pid = g_process_seeds[index].pid;
    process64_copy_name(g_processes[index].name, g_process_seeds[index].name);
    g_processes[index].principal_id = g_process_seeds[index].principal_id;
    g_processes[index].endpoint_class = g_process_seeds[index].endpoint_class;
    g_processes[index].state = g_process_seeds[index].state;
    g_processes[index].scheduler_class = g_process_seeds[index].scheduler_class;
    g_processes[index].capability_limit = g_process_seeds[index].capability_limit;
    g_processes[index].manifest_index = LAUNCH64_INVALID_MANIFEST;
    g_processes[index].manifest_package_id = 0u;
    g_processes[index].manifest_executable_id = 0u;
    g_processes[index].manifest_signer_id = 0u;
    g_processes[index].manifest_token = 0u;
    g_processes[index].vma_root = vma_root;
    g_processes[index].fd_table = fd_table;
    g_processes[index].persona_ctx = persona_ctx;
    g_processes[index].audit_ctx = audit_ctx;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    g_processes[index].page_root_physical = page_root_physical;
    g_processes[index].page_root_index = page_root_index;
    g_processes[index].page_root_token = page_root_token;
#endif
}

static u32 process64_records_valid(void)
{
    return ((g_processes[0].pid == g_process_seeds[0].pid)
        && (g_processes[0].principal_id == g_process_seeds[0].principal_id)
        && (g_processes[PROCESS64_RECORD_COUNT - 1u].pid
            == g_process_seeds[PROCESS64_RECORD_COUNT - 1u].pid)
        && (g_processes[PROCESS64_RECORD_COUNT - 1u].principal_id
            == g_process_seeds[PROCESS64_RECORD_COUNT - 1u].principal_id))
        ? 1u
        : 0u;
}

static void process64_seed_records(u32 preserve_extensions)
{
    u32 index;

    for (index = 0u; index < PROCESS64_RECORD_COUNT; ++index)
    {
        process64_seed_record(index, preserve_extensions);
    }

#ifdef LIMITLESS_X64_UEFI_KERNEL
    if (preserve_extensions == 0u)
    {
        process64_clear_clone_records();
    }
#endif
}

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
    if ((g_process_init != 0u) && (process64_records_valid() != 0u))
    {
        return;
    }

    principal64_init();
    services64_init();
    launch64_init();
    process64_seed_records((g_process_init != 0u) ? 1u : 0u);
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

#ifdef LIMITLESS_X64_UEFI_KERNEL
    for (index = 0u; index < PROCESS64_CLONE_RECORD_COUNT; ++index)
    {
        if ((process64_clone_record_active(&g_process64_clone_records[index]) != 0u)
            && (g_process64_clone_records[index].pid == pid))
        {
            return &g_process64_clone_records[index];
        }
    }
#endif

    return 0;
}

static struct process64_record *process64_find_mutable(u32 pid)
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

#ifdef LIMITLESS_X64_UEFI_KERNEL
    for (index = 0u; index < PROCESS64_CLONE_RECORD_COUNT; ++index)
    {
        if ((process64_clone_record_active(&g_process64_clone_records[index]) != 0u)
            && (g_process64_clone_records[index].pid == pid))
        {
            return &g_process64_clone_records[index];
        }
    }
#endif

    return 0;
}

static u32 process64_attach_slot(void **slot, void *context)
{
    if ((slot == 0) || (context == 0) || (*slot != 0))
    {
        return 0u;
    }

    *slot = context;
    return 1u;
}

static void *process64_detach_slot(void **slot)
{
    void *context;

    if (slot == 0)
    {
        return 0;
    }

    context = *slot;
    *slot = 0;
    return context;
}

void process64_init(void)
{
    process64_ensure_init();
}

u32 process64_count(void)
{
    process64_ensure_init();
#ifdef LIMITLESS_X64_UEFI_KERNEL
    return PROCESS64_RECORD_COUNT + g_process64_clone_count;
#else
    return PROCESS64_RECORD_COUNT;
#endif
}

u32 process64_pid_by_index(u32 index)
{
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u32 clone_index;
    u32 seen;
#endif

    process64_ensure_init();

    if (index >= PROCESS64_RECORD_COUNT)
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        seen = PROCESS64_RECORD_COUNT;
        for (clone_index = 0u; clone_index < PROCESS64_CLONE_RECORD_COUNT; ++clone_index)
        {
            if (process64_clone_record_active(&g_process64_clone_records[clone_index]) != 0u)
            {
                if (seen == index)
                {
                    return g_process64_clone_records[clone_index].pid;
                }
                ++seen;
            }
        }
#endif
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

    return (record != 0) ? record->name : g_process64_unknown_name;
}

u32 process64_attach_vma(u32 pid, void *vma_root)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_attach_slot(&record->vma_root, vma_root) : 0u;
}

void *process64_detach_vma(u32 pid)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_detach_slot(&record->vma_root) : 0;
}

void *process64_vma_root(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->vma_root : 0;
}

u32 process64_attach_fd(u32 pid, void *fd_table)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_attach_slot(&record->fd_table, fd_table) : 0u;
}

void *process64_detach_fd(u32 pid)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_detach_slot(&record->fd_table) : 0;
}

void *process64_fd_table(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->fd_table : 0;
}

u32 process64_attach_persona(u32 pid, void *persona_ctx)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_attach_slot(&record->persona_ctx, persona_ctx) : 0u;
}

void *process64_detach_persona(u32 pid)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_detach_slot(&record->persona_ctx) : 0;
}

void *process64_persona_ctx(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->persona_ctx : 0;
}

u32 process64_attach_audit(u32 pid, void *audit_ctx)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_attach_slot(&record->audit_ctx, audit_ctx) : 0u;
}

void *process64_detach_audit(u32 pid)
{
    struct process64_record *record = process64_find_mutable(pid);

    return (record != 0) ? process64_detach_slot(&record->audit_ctx) : 0;
}

void *process64_audit_ctx(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->audit_ctx : 0;
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 process64_attach_page_root(
    u32 pid,
    u64 root_physical,
    u32 root_index,
    u32 root_token,
    u32 authority_token)
{
    struct process64_record *record = process64_find_mutable(pid);

    if ((record == 0)
        || (root_physical == 0ull)
        || (root_token == 0u)
        || (authority_token == 0u)
        || (record->page_root_physical != 0ull)
        || (record->page_root_token != 0u))
    {
        ++g_process64_page_root_denial_count;
        return 0u;
    }

    record->page_root_physical = root_physical;
    record->page_root_index = root_index;
    record->page_root_token = root_token;
    ++g_process64_page_root_attach_count;
    return 1u;
}

u32 process64_clear_page_root(u32 pid, u32 root_token)
{
    struct process64_record *record = process64_find_mutable(pid);

    if ((record == 0)
        || (root_token == 0u)
        || (record->page_root_token != root_token))
    {
        ++g_process64_page_root_denial_count;
        return 0u;
    }

    record->page_root_physical = 0ull;
    record->page_root_index = 0xFFFFFFFFu;
    record->page_root_token = 0u;
    ++g_process64_page_root_clear_count;
    return 1u;
}

u64 process64_page_root_physical(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->page_root_physical : 0ull;
}

u32 process64_page_root_index(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->page_root_index : 0xFFFFFFFFu;
}

u32 process64_page_root_token(u32 pid)
{
    const struct process64_record *record = process64_find(pid);

    return (record != 0) ? record->page_root_token : 0u;
}

u32 process64_page_root_attach_count(void)
{
    return g_process64_page_root_attach_count;
}

u32 process64_page_root_clear_count(void)
{
    return g_process64_page_root_clear_count;
}

u32 process64_page_root_denial_count(void)
{
    return g_process64_page_root_denial_count;
}

u32 process64_spawn_clone(u32 parent_pid)
{
    const struct process64_record *parent = process64_find(parent_pid);
    struct process64_record *child = 0;
    u32 index;

    if ((parent == 0) || (parent->principal_id == 0u))
    {
        return PROCESS64_INVALID_PID;
    }

    for (index = 0u; index < PROCESS64_CLONE_RECORD_COUNT; ++index)
    {
        if (process64_clone_record_active(&g_process64_clone_records[index]) == 0u)
        {
            child = &g_process64_clone_records[index];
            break;
        }
    }

    if (child == 0)
    {
        return PROCESS64_INVALID_PID;
    }

    process64_clear_record(child);
    child->pid = g_process64_next_clone_pid++;
    if ((g_process64_next_clone_pid == PROCESS64_INVALID_PID)
        || (g_process64_next_clone_pid < PROCESS64_CLONE_PID_BASE))
    {
        g_process64_next_clone_pid = PROCESS64_CLONE_PID_BASE;
    }

    process64_copy_name(child->name, "linux-thread");
    child->principal_id = parent->principal_id;
    child->endpoint_class = SERVICE_ENDPOINT_CLASS_NONE;
    child->state = PROCESS64_STATE_BOOTSTRAPPED | PROCESS64_STATE_READY;
    child->scheduler_class = parent->scheduler_class;
    child->capability_limit = parent->capability_limit;
    child->manifest_index = parent->manifest_index;
    child->manifest_package_id = parent->manifest_package_id;
    child->manifest_executable_id = parent->manifest_executable_id;
    child->manifest_signer_id = parent->manifest_signer_id;
    child->manifest_token = parent->manifest_token;
    ++g_process64_clone_count;
    return child->pid;
}

u32 process64_release_clone(u32 pid)
{
    u32 index;

    process64_ensure_init();

    for (index = 0u; index < PROCESS64_CLONE_RECORD_COUNT; ++index)
    {
        if ((process64_clone_record_active(&g_process64_clone_records[index]) != 0u)
            && (g_process64_clone_records[index].pid == pid))
        {
            process64_clear_record(&g_process64_clone_records[index]);
            if (g_process64_clone_count != 0u)
            {
                --g_process64_clone_count;
            }
            return 1u;
        }
    }

    return 0u;
}

u32 process64_is_clone(u32 pid)
{
    u32 index;

    process64_ensure_init();

    for (index = 0u; index < PROCESS64_CLONE_RECORD_COUNT; ++index)
    {
        if ((process64_clone_record_active(&g_process64_clone_records[index]) != 0u)
            && (g_process64_clone_records[index].pid == pid))
        {
            return 1u;
        }
    }

    return 0u;
}

u32 process64_clone_count(void)
{
    process64_ensure_init();
    return g_process64_clone_count;
}
#else
u32 process64_attach_page_root(
    u32 pid,
    u64 root_physical,
    u32 root_index,
    u32 root_token,
    u32 authority_token)
{
    (void)pid;
    (void)root_physical;
    (void)root_index;
    (void)root_token;
    (void)authority_token;
    return 0u;
}

u32 process64_clear_page_root(u32 pid, u32 root_token)
{
    (void)pid;
    (void)root_token;
    return 0u;
}

u64 process64_page_root_physical(u32 pid) { (void)pid; return 0ull; }
u32 process64_page_root_index(u32 pid) { (void)pid; return 0xFFFFFFFFu; }
u32 process64_page_root_token(u32 pid) { (void)pid; return 0u; }
u32 process64_page_root_attach_count(void) { return 0u; }
u32 process64_page_root_clear_count(void) { return 0u; }
u32 process64_page_root_denial_count(void) { return 0u; }
#endif
