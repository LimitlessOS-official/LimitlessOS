#include "services_x64.h"

#include "bootstrap_catalog.h"
#include "ipc.h"
#include "services.h"

struct service64_record
{
    u32 id;
    const char *name;
    u32 endpoint_class;
    u32 endpoint_id;
    u32 delegable;
    u32 capability_mask;
};

enum
{
    SERVICE64_RECORD_COUNT = 10
};

static const struct service64_record g_service_records[SERVICE64_RECORD_COUNT] = {
    {
        SERVICE_ID_INIT,
        "init-supervisor",
        SERVICE_ENDPOINT_CLASS_INIT,
        IPC_ENDPOINT_INIT,
        0u,
        SERVICE_CAP_ROUTE_POLICY | SERVICE_CAP_ROUTE_DRIVER | SERVICE_CAP_SUPERVISE | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_DRIVER_HOST,
        "driver-host",
        SERVICE_ENDPOINT_CLASS_DRIVER_HOST,
        IPC_ENDPOINT_DRIVER_HOST,
        0u,
        SERVICE_CAP_ROUTE_DRIVER
    },
    {
        SERVICE_ID_AI_POLICY,
        "ai-policy",
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        IPC_ENDPOINT_AI_POLICY,
        1u,
        SERVICE_CAP_ROUTE_POLICY | SERVICE_CAP_ROUTE_INIT | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_TELEMETRY,
        "telemetry",
        SERVICE_ENDPOINT_CLASS_NONE,
        0u,
        0u,
        SERVICE_CAP_READ_TELEMETRY | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_CONSOLE,
        "console",
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        4u,
        1u,
        SERVICE_CAP_CONSOLE | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_RAMFS,
        "ramfs",
        SERVICE_ENDPOINT_CLASS_RAMFS,
        5u,
        0u,
        SERVICE_CAP_RAMFS | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_INPUT,
        "input",
        SERVICE_ENDPOINT_CLASS_INPUT,
        6u,
        1u,
        SERVICE_CAP_INPUT | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_DISPLAY,
        "display",
        SERVICE_ENDPOINT_CLASS_DISPLAY,
        7u,
        1u,
        SERVICE_CAP_DISPLAY | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_BLOCK,
        "block",
        SERVICE_ENDPOINT_CLASS_BLOCK,
        8u,
        1u,
        SERVICE_CAP_BLOCK | SERVICE_CAP_AUDIT
    },
    {
        SERVICE_ID_HARDWARE,
        "hardware-inventory",
        SERVICE_ENDPOINT_CLASS_HARDWARE,
        9u,
        0u,
        SERVICE_CAP_HARDWARE | SERVICE_CAP_AUDIT
    }
};

static u32 g_service_init = 0u;
static struct bootstrap_catalog_summary g_catalog_summary;
static u32 g_catalog_valid = 0u;

static void services64_ensure_init(void)
{
    if (g_service_init != 0u)
    {
        return;
    }

    g_catalog_valid = 0u;
    if (bootstrap_catalog_read_summary(&g_catalog_summary))
    {
        g_catalog_valid = bootstrap_catalog_is_valid(&g_catalog_summary) ? 1u : 0u;
    }

    g_service_init = 1u;
}

void services64_init(void)
{
    services64_ensure_init();
}

u32 services64_count(void)
{
    services64_ensure_init();
    return SERVICE64_RECORD_COUNT;
}

u32 services64_resolve_endpoint_class(u32 endpoint_class)
{
    u32 index;

    services64_ensure_init();

    for (index = 0u; index < SERVICE64_RECORD_COUNT; ++index)
    {
        if (g_service_records[index].endpoint_class == endpoint_class)
        {
            return g_service_records[index].endpoint_id;
        }
    }

    return 0xFFFFFFFFu;
}

const char *services64_endpoint_name(u32 endpoint_id)
{
    u32 index;

    services64_ensure_init();

    for (index = 0u; index < SERVICE64_RECORD_COUNT; ++index)
    {
        if (g_service_records[index].endpoint_id == endpoint_id)
        {
            return g_service_records[index].name;
        }
    }

    return NULL;
}

u32 services64_capabilities_for_endpoint(u32 endpoint_id)
{
    u32 index;

    services64_ensure_init();

    for (index = 0u; index < SERVICE64_RECORD_COUNT; ++index)
    {
        if (g_service_records[index].endpoint_id == endpoint_id)
        {
            return g_service_records[index].capability_mask;
        }
    }

    return 0u;
}

u32 services64_endpoint_is_delegable(u32 endpoint_id)
{
    u32 index;

    services64_ensure_init();

    for (index = 0u; index < SERVICE64_RECORD_COUNT; ++index)
    {
        if (g_service_records[index].endpoint_id == endpoint_id)
        {
            return g_service_records[index].delegable;
        }
    }

    return 0u;
}

u32 services64_package_version(void)
{
    services64_ensure_init();
    return g_catalog_valid ? g_catalog_summary.version : 0u;
}

u32 services64_package_signer_count(void)
{
    services64_ensure_init();
    return g_catalog_valid ? g_catalog_summary.signer_count : 0u;
}

u32 services64_package_manifest_count(void)
{
    services64_ensure_init();
    return g_catalog_valid ? g_catalog_summary.manifest_count : 0u;
}

u32 services64_package_payload_count(void)
{
    services64_ensure_init();
    return g_catalog_valid ? g_catalog_summary.payload_count : 0u;
}

u32 services64_package_checksum(void)
{
    services64_ensure_init();
    return g_catalog_valid ? g_catalog_summary.archive_checksum : 0u;
}

u32 services64_package_valid(void)
{
    services64_ensure_init();
    return g_catalog_valid;
}
