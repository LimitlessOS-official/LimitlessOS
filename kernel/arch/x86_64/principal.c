#include "principal_x64.h"

struct principal64_record
{
    u32 id;
    u32 role_mask;
    u32 active;
    const char *name;
};

enum
{
    PRINCIPAL64_RECORD_COUNT = 11
};

static const struct principal64_record g_principals[PRINCIPAL64_RECORD_COUNT] = {
    {
        PRINCIPAL64_ID_SYSTEM,
        PRINCIPAL64_ROLE_SYSTEM,
        1u,
        "system"
    },
    {
        PRINCIPAL64_ID_POLICY_CLIENT,
        PRINCIPAL64_ROLE_SERVICE_CLIENT | PRINCIPAL64_ROLE_INTERACTIVE | PRINCIPAL64_ROLE_POLICY,
        1u,
        "policy-client"
    },
    {
        PRINCIPAL64_ID_POLICY_WORKER,
        PRINCIPAL64_ROLE_WORKER | PRINCIPAL64_ROLE_SERVICE | PRINCIPAL64_ROLE_POLICY,
        1u,
        "policy-worker"
    },
    {
        PRINCIPAL64_ID_CONSOLE_CLIENT,
        PRINCIPAL64_ROLE_SERVICE_CLIENT | PRINCIPAL64_ROLE_INTERACTIVE | PRINCIPAL64_ROLE_CONSOLE,
        1u,
        "console-client"
    },
    {
        PRINCIPAL64_ID_CONSOLE_WORKER,
        PRINCIPAL64_ROLE_WORKER | PRINCIPAL64_ROLE_SERVICE | PRINCIPAL64_ROLE_CONSOLE,
        1u,
        "console-worker"
    },
    {
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        PRINCIPAL64_ROLE_SYSTEM | PRINCIPAL64_ROLE_SERVICE,
        1u,
        "init-supervisor"
    },
    {
        PRINCIPAL64_ID_DRIVER_HOST,
        PRINCIPAL64_ROLE_WORKER | PRINCIPAL64_ROLE_SERVICE | PRINCIPAL64_ROLE_DRIVER,
        1u,
        "driver-host"
    },
    {
        PRINCIPAL64_ID_RAMFS_WORKER,
        PRINCIPAL64_ROLE_WORKER | PRINCIPAL64_ROLE_SERVICE | PRINCIPAL64_ROLE_STORAGE,
        1u,
        "ramfs-worker"
    },
    {
        PRINCIPAL64_ID_INPUT_WORKER,
        PRINCIPAL64_ROLE_WORKER | PRINCIPAL64_ROLE_SERVICE | PRINCIPAL64_ROLE_INPUT,
        1u,
        "input-worker"
    },
    {
        PRINCIPAL64_ID_TELEMETRY,
        PRINCIPAL64_ROLE_SERVICE | PRINCIPAL64_ROLE_TELEMETRY,
        1u,
        "telemetry"
    },
    {
        PRINCIPAL64_ID_BLOCK_WORKER,
        PRINCIPAL64_ROLE_WORKER | PRINCIPAL64_ROLE_SERVICE | PRINCIPAL64_ROLE_STORAGE | PRINCIPAL64_ROLE_DRIVER,
        1u,
        "block-worker"
    }
};

void principal64_init(void)
{
}

u32 principal64_count(void)
{
    return PRINCIPAL64_RECORD_COUNT;
}

u32 principal64_is_active(u32 principal_id)
{
    u32 index;

    for (index = 0u; index < PRINCIPAL64_RECORD_COUNT; ++index)
    {
        if (g_principals[index].id == principal_id)
        {
            return g_principals[index].active;
        }
    }

    return 0u;
}

u32 principal64_role(u32 principal_id)
{
    u32 index;

    for (index = 0u; index < PRINCIPAL64_RECORD_COUNT; ++index)
    {
        if ((g_principals[index].id == principal_id) && (g_principals[index].active != 0u))
        {
            return g_principals[index].role_mask;
        }
    }

    return 0u;
}

u32 principal64_lookup_by_index(u32 index)
{
    if (index >= PRINCIPAL64_RECORD_COUNT)
    {
        return 0u;
    }

    return g_principals[index].id;
}

const char *principal64_name(u32 principal_id)
{
    u32 index;

    for (index = 0u; index < PRINCIPAL64_RECORD_COUNT; ++index)
    {
        if ((g_principals[index].id == principal_id) && (g_principals[index].active != 0u))
        {
            return g_principals[index].name;
        }
    }

    return "unknown";
}
