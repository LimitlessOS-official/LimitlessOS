#include "windows_vfs_x64.h"

#include "capability_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "services.h"
#include "services_x64.h"
#include "windows_abi_x64.h"
#include "windows_handle_x64.h"

/*
 * K.5 adds the first narrow Windows VFS bridge for NtCreateFile. It resolves
 * only the documented System32 ntdll.dll shim path, mints a scoped RAMFS
 * service capability for the Windows persona shim-provider endpoint, and
 * installs it through windows_handle_x64.c. The verification checkpoint proves
 * a valid NT path produces a real handle and scoped capability while unknown
 * paths and unsupported access modes are denied truthfully.
 */

#define WINDOWS_VFS64_NTDLL_PATH "\\??\\c:\\windows\\system32\\ntdll.dll"
#define WINDOWS_VFS64_ALLOWED_ACCESS 0x001200A9u
#define WINDOWS_VFS64_ALLOWED_OPTIONS 0x00000060u

static u32 g_windows_vfs64_open_count = 0u;
static u32 g_windows_vfs64_denial_count = 0u;
static u32 g_windows_vfs64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_vfs64_last_path_hash = 0u;
static u32 g_windows_vfs64_last_path_bytes = 0u;
static u32 g_windows_vfs64_last_shim_id = 0u;
static u64 g_windows_vfs64_last_handle = 0ull;
static u32 g_windows_vfs64_last_capability = CAPABILITY64_INVALID_HANDLE;
static u32 g_windows_vfs64_last_backend_endpoint = 0u;

static void windows_vfs64_clear_open_result(windows_vfs64_open_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->handle = 0ull;
    result->capability_handle = CAPABILITY64_INVALID_HANDLE;
    result->status = WINDOWS_ABI64_STATUS_SUCCESS;
    result->shim_id = 0u;
    result->path_hash = 0u;
    result->path_bytes = 0u;
    result->backend_endpoint = 0u;
}

static u8 windows_vfs64_lower(u8 value)
{
    if ((value >= (u8)'A') && (value <= (u8)'Z'))
    {
        return (u8)(value + ((u8)'a' - (u8)'A'));
    }

    return value;
}

static u32 windows_vfs64_path_hash(const u8 *path, u32 path_bytes)
{
    u32 hash = 2166136261u;
    u32 index;

    if (path == 0)
    {
        return 0u;
    }

    for (index = 0u; index < path_bytes; ++index)
    {
        u8 value = windows_vfs64_lower(path[index]);
        if (value == (u8)'/')
        {
            value = (u8)'\\';
        }
        hash ^= value;
        hash *= 16777619u;
    }

    return hash;
}

static u32 windows_vfs64_path_is_ntdll(const u8 *path, u32 path_bytes)
{
    const char expected[] = WINDOWS_VFS64_NTDLL_PATH;
    u32 index;

    if ((path == 0) || (path_bytes != (u32)(sizeof(expected) - 1u)))
    {
        return 0u;
    }

    for (index = 0u; index < path_bytes; ++index)
    {
        u8 actual = windows_vfs64_lower(path[index]);
        if (actual == (u8)'/')
        {
            actual = (u8)'\\';
        }
        if (actual != (u8)expected[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 windows_vfs64_access_is_supported(u32 desired_access)
{
    if (desired_access == 0u)
    {
        return 0u;
    }

    return ((desired_access & ~WINDOWS_VFS64_ALLOWED_ACCESS) == 0u) ? 1u : 0u;
}

static u32 windows_vfs64_disposition_is_supported(u32 create_disposition)
{
    return ((create_disposition == WINDOWS_ABI64_FILE_OPEN)
        || (create_disposition == WINDOWS_ABI64_FILE_OPEN_IF))
        ? 1u
        : 0u;
}

static u32 windows_vfs64_options_are_supported(u32 create_options)
{
    return ((create_options & ~WINDOWS_VFS64_ALLOWED_OPTIONS) == 0u) ? 1u : 0u;
}

void windows_vfs64_init(void)
{
    g_windows_vfs64_open_count = 0u;
    g_windows_vfs64_denial_count = 0u;
    g_windows_vfs64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_vfs64_last_path_hash = 0u;
    g_windows_vfs64_last_path_bytes = 0u;
    g_windows_vfs64_last_shim_id = 0u;
    g_windows_vfs64_last_handle = 0ull;
    g_windows_vfs64_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_vfs64_last_backend_endpoint = 0u;
}

u32 windows_vfs64_open_path(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    u32 desired_access,
    u32 create_disposition,
    u32 create_options,
    windows_vfs64_open_result_t *out_result)
{
    u32 owner_id;
    u32 capability_handle;
    u32 rights = CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY;
    u32 status = WINDOWS_ABI64_STATUS_SUCCESS;
    u64 handle;
    u32 endpoint;
    u32 path_hash;

    windows_vfs64_clear_open_result(out_result);
    path_hash = windows_vfs64_path_hash(path, path_bytes);
    g_windows_vfs64_last_path_hash = path_hash;
    g_windows_vfs64_last_path_bytes = path_bytes;
    g_windows_vfs64_last_shim_id = 0u;
    g_windows_vfs64_last_handle = 0ull;
    g_windows_vfs64_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_vfs64_last_backend_endpoint = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (path == 0)
        || (path_bytes == 0u)
        || (path_bytes > WINDOWS_VFS64_MAX_PATH_BYTES)
        || (out_result == 0))
    {
        status = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        if (out_result != 0)
        {
            out_result->status = status;
            out_result->path_hash = path_hash;
            out_result->path_bytes = path_bytes;
        }
        return status;
    }

    out_result->path_hash = path_hash;
    out_result->path_bytes = path_bytes;
    if (windows_vfs64_access_is_supported(desired_access) == 0u)
    {
        status = WINDOWS_ABI64_STATUS_ACCESS_DENIED;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        return status;
    }
    if ((windows_vfs64_disposition_is_supported(create_disposition) == 0u)
        || (windows_vfs64_options_are_supported(create_options) == 0u))
    {
        status = WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        return status;
    }
    if (windows_vfs64_path_is_ntdll(path, path_bytes) == 0u)
    {
        status = WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        return status;
    }

    owner_id = process64_principal(pid);
    endpoint = services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS);
    capability_handle = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_RAMFS,
        rights,
        owner_id);
    if (capability_handle == CAPABILITY64_INVALID_HANDLE)
    {
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        return status;
    }

    handle = windows_handle64_install(
        pid,
        capability_handle,
        WINDOWS_HANDLE64_TYPE_FILE,
        rights,
        0u);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        (void)capability64_revoke(capability_handle, owner_id);
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        return status;
    }

    ++g_windows_vfs64_open_count;
    g_windows_vfs64_last_result = status;
    g_windows_vfs64_last_shim_id = WINDOWS_VFS64_SHIM_ID_NTDLL;
    g_windows_vfs64_last_handle = handle;
    g_windows_vfs64_last_capability = capability_handle;
    g_windows_vfs64_last_backend_endpoint = endpoint;
    out_result->handle = handle;
    out_result->capability_handle = capability_handle;
    out_result->status = status;
    out_result->shim_id = WINDOWS_VFS64_SHIM_ID_NTDLL;
    out_result->backend_endpoint = endpoint;
    return status;
}

u32 windows_vfs64_open_count(void)
{
    return g_windows_vfs64_open_count;
}

u32 windows_vfs64_denial_count(void)
{
    return g_windows_vfs64_denial_count;
}

u32 windows_vfs64_last_result(void)
{
    return g_windows_vfs64_last_result;
}

u32 windows_vfs64_last_path_hash(void)
{
    return g_windows_vfs64_last_path_hash;
}

u32 windows_vfs64_last_path_bytes(void)
{
    return g_windows_vfs64_last_path_bytes;
}

u32 windows_vfs64_last_shim_id(void)
{
    return g_windows_vfs64_last_shim_id;
}

u64 windows_vfs64_last_handle(void)
{
    return g_windows_vfs64_last_handle;
}

u32 windows_vfs64_last_capability(void)
{
    return g_windows_vfs64_last_capability;
}

u32 windows_vfs64_last_backend_endpoint(void)
{
    return g_windows_vfs64_last_backend_endpoint;
}
