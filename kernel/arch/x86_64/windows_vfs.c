#include "windows_vfs_x64.h"

#include "capability_x64.h"
#include "fs_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "ramfs.h"
#include "services.h"
#include "services_x64.h"
#include "windows_abi_x64.h"
#include "windows_handle_x64.h"

/*
 * K.5 adds the first narrow Windows VFS bridge for NtCreateFile. L.1 adds the
 * NT object namespace router above that bridge: DOS C: paths, UNC paths,
 * ConDrv, and Null are classified into LimitlessOS backing namespaces before
 * an open is attempted. L.2 maps C:\Windows\System32 shim DLL names onto the
 * embedded Windows persona shim set. The router does not mint authority. Open
 * still mints scoped handles through the capability broker; recognized
 * namespaces without backing semantics return truthful denial status. L.3 adds
 * a per-process C:\Users\LimitlessUser profile namespace backed by RAMFS and
 * created through the FS service capability path before a Windows file handle
 * is exposed. L.4 routes the Windows TEMP/TMP path under that profile to a
 * separate per-process RAMFS scratch tree and proves create/write/read/delete
 * with scoped FS node handles.
 */

#define WINDOWS_VFS64_DOS_C_PREFIX "\\??\\c:\\"
#define WINDOWS_VFS64_SYSTEM32_PREFIX "\\??\\c:\\windows\\system32\\"
#define WINDOWS_VFS64_USERS_PREFIX "\\??\\c:\\users\\"
#define WINDOWS_VFS64_USER_PROFILE_PREFIX "\\??\\c:\\users\\limitlessuser\\"
#define WINDOWS_VFS64_TEMP_PREFIX "\\??\\c:\\users\\limitlessuser\\appdata\\local\\temp\\"
#define WINDOWS_VFS64_UNC_PREFIX "\\??\\unc\\"
#define WINDOWS_VFS64_CONDRV_PREFIX "\\device\\condrv\\"
#define WINDOWS_VFS64_NULL_PATH "\\device\\null"
#define WINDOWS_VFS64_TARGET_WINROOT "/WINROOT/"
#define WINDOWS_VFS64_TARGET_WINUSER "/WINUSER/"
#define WINDOWS_VFS64_TARGET_WINTMP "/WINTMP/"
#define WINDOWS_VFS64_TARGET_UNC "/NET/UNC/"
#define WINDOWS_VFS64_TARGET_CONSOLE "/DEV/CONSOLE"
#define WINDOWS_VFS64_TARGET_NULL "/DEV/NULL"
#define WINDOWS_VFS64_ALLOWED_ACCESS 0x001200A9u
#define WINDOWS_VFS64_ALLOWED_OPTIONS 0x00000060u
#define WINDOWS_VFS64_PROFILE_PATH_BYTES 192u
#define WINDOWS_VFS64_PROFILE_MASK_ROOT 0x00000001u
#define WINDOWS_VFS64_PROFILE_MASK_PID 0x00000002u
#define WINDOWS_VFS64_PROFILE_MASK_USER 0x00000004u
#define WINDOWS_VFS64_PROFILE_MASK_APPDATA 0x00000008u
#define WINDOWS_VFS64_PROFILE_MASK_LOCAL 0x00000010u
#define WINDOWS_VFS64_PROFILE_MASK_ROAMING 0x00000020u
#define WINDOWS_VFS64_PROFILE_MASK_DESKTOP 0x00000040u
#define WINDOWS_VFS64_PROFILE_MASK_DOCUMENTS 0x00000080u
#define WINDOWS_VFS64_TEMP_MASK_ROOT 0x00000001u
#define WINDOWS_VFS64_TEMP_MASK_PID 0x00000002u

static u32 g_windows_vfs64_route_count = 0u;
static u32 g_windows_vfs64_route_denial_count = 0u;
static u32 g_windows_vfs64_open_count = 0u;
static u32 g_windows_vfs64_denial_count = 0u;
static u32 g_windows_vfs64_profile_create_count = 0u;
static u32 g_windows_vfs64_profile_denial_count = 0u;
static u32 g_windows_vfs64_profile_last_node = 0u;
static u32 g_windows_vfs64_profile_last_dir_mask = 0u;
static u32 g_windows_vfs64_temp_create_count = 0u;
static u32 g_windows_vfs64_temp_write_count = 0u;
static u32 g_windows_vfs64_temp_read_count = 0u;
static u32 g_windows_vfs64_temp_delete_count = 0u;
static u32 g_windows_vfs64_temp_denial_count = 0u;
static u32 g_windows_vfs64_temp_last_node = 0u;
static u32 g_windows_vfs64_temp_last_dir_mask = 0u;
static u32 g_windows_vfs64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_vfs64_last_path_hash = 0u;
static u32 g_windows_vfs64_last_path_bytes = 0u;
static u32 g_windows_vfs64_last_route_type = WINDOWS_VFS64_ROUTE_UNKNOWN;
static u32 g_windows_vfs64_last_target_hash = 0u;
static u32 g_windows_vfs64_last_target_bytes = 0u;
static u32 g_windows_vfs64_last_device_id = WINDOWS_VFS64_DEVICE_NONE;
static u32 g_windows_vfs64_last_route_unavailable = 0u;
static u32 g_windows_vfs64_last_shim_id = 0u;
static u64 g_windows_vfs64_last_handle = 0ull;
static u32 g_windows_vfs64_last_capability = CAPABILITY64_INVALID_HANDLE;
static u32 g_windows_vfs64_last_backend_endpoint = 0u;

static void windows_vfs64_clear_route_result(windows_vfs64_route_result_t *result)
{
    u32 index;

    if (result == 0)
    {
        return;
    }

    result->status = WINDOWS_ABI64_STATUS_SUCCESS;
    result->route_type = WINDOWS_VFS64_ROUTE_UNKNOWN;
    result->device_id = WINDOWS_VFS64_DEVICE_NONE;
    result->unavailable = 0u;
    result->path_hash = 0u;
    result->path_bytes = 0u;
    result->target_hash = 0u;
    result->target_bytes = 0u;
    result->backend_endpoint = 0u;
    for (index = 0u; index < WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES; ++index)
    {
        result->target_path[index] = 0u;
    }
}

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
    result->node_id = 0u;
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

static u32 windows_vfs64_prefix_matches(
    const u8 *path,
    u32 path_bytes,
    const char *prefix,
    u32 prefix_bytes)
{
    u32 index;

    if ((path == 0) || (prefix == 0) || (path_bytes < prefix_bytes))
    {
        return 0u;
    }

    for (index = 0u; index < prefix_bytes; ++index)
    {
        u8 actual = windows_vfs64_lower(path[index]);
        u8 expected = windows_vfs64_lower((u8)prefix[index]);

        if (actual == (u8)'/')
        {
            actual = (u8)'\\';
        }
        if (expected == (u8)'/')
        {
            expected = (u8)'\\';
        }
        if (actual != expected)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 windows_vfs64_path_equals(
    const u8 *path,
    u32 path_bytes,
    const char *expected_path)
{
    u32 expected_bytes = 0u;

    if (expected_path == 0)
    {
        return 0u;
    }
    while (expected_path[expected_bytes] != 0)
    {
        ++expected_bytes;
    }

    return (path_bytes == expected_bytes)
        ? windows_vfs64_prefix_matches(path, path_bytes, expected_path, expected_bytes)
        : 0u;
}

static u32 windows_vfs64_target_hash(const u8 *target, u32 target_bytes)
{
    u32 hash = 2166136261u;
    u32 index;

    if (target == 0)
    {
        return 0u;
    }

    for (index = 0u; index < target_bytes; ++index)
    {
        hash ^= (u32)target[index];
        hash *= 16777619u;
    }

    return hash;
}

static void windows_vfs64_route_append_literal(
    windows_vfs64_route_result_t *result,
    const char *literal)
{
    u32 index = 0u;

    if ((result == 0) || (literal == 0))
    {
        return;
    }

    while ((literal[index] != 0)
        && (result->target_bytes < WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES))
    {
        result->target_path[result->target_bytes] = (u8)literal[index];
        ++result->target_bytes;
        ++index;
    }
}

static void windows_vfs64_route_append_nt_tail(
    windows_vfs64_route_result_t *result,
    const u8 *path,
    u32 path_bytes,
    u32 offset)
{
    u32 index;

    if ((result == 0) || (path == 0) || (offset > path_bytes))
    {
        return;
    }

    for (index = offset;
        (index < path_bytes)
            && (result->target_bytes < WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES);
        ++index)
    {
        u8 value = windows_vfs64_lower(path[index]);

        if (value == (u8)'\\')
        {
            value = (u8)'/';
        }
        result->target_path[result->target_bytes] = value;
        ++result->target_bytes;
    }
}

static void windows_vfs64_route_append_nt_tail_preserve(
    windows_vfs64_route_result_t *result,
    const u8 *path,
    u32 path_bytes,
    u32 offset)
{
    u32 index;

    if ((result == 0) || (path == 0) || (offset > path_bytes))
    {
        return;
    }

    for (index = offset;
        (index < path_bytes)
            && (result->target_bytes < WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES);
        ++index)
    {
        u8 value = path[index];

        if (value == (u8)'\\')
        {
            value = (u8)'/';
        }
        result->target_path[result->target_bytes] = value;
        ++result->target_bytes;
    }
}

static u32 windows_vfs64_append_literal_to_path(
    u8 *path,
    u32 capacity,
    u32 *path_bytes,
    const char *literal)
{
    u32 index = 0u;

    if ((path == 0) || (path_bytes == 0) || (literal == 0))
    {
        return 0u;
    }

    while (literal[index] != 0)
    {
        if (*path_bytes >= capacity)
        {
            return 0u;
        }
        path[*path_bytes] = (u8)literal[index];
        ++(*path_bytes);
        ++index;
    }

    return 1u;
}

static u32 windows_vfs64_append_decimal_to_path(
    u8 *path,
    u32 capacity,
    u32 *path_bytes,
    u32 value)
{
    u8 digits[10];
    u32 digit_count = 0u;
    u32 index;

    if ((path == 0) || (path_bytes == 0))
    {
        return 0u;
    }

    do
    {
        digits[digit_count] = (u8)('0' + (value % 10u));
        value /= 10u;
        ++digit_count;
    } while ((value != 0u) && (digit_count < 10u));

    for (index = 0u; index < digit_count; ++index)
    {
        if (*path_bytes >= capacity)
        {
            return 0u;
        }
        path[*path_bytes] = digits[digit_count - 1u - index];
        ++(*path_bytes);
    }

    return 1u;
}

static u32 windows_vfs64_build_profile_base_path(
    u32 pid,
    u8 *path,
    u32 capacity,
    u32 *path_bytes)
{
    u32 index;

    if ((path == 0) || (path_bytes == 0))
    {
        return 0u;
    }

    for (index = 0u; index < capacity; ++index)
    {
        path[index] = 0u;
    }
    *path_bytes = 0u;

    return (windows_vfs64_append_literal_to_path(
                path,
                capacity,
                path_bytes,
                WINDOWS_VFS64_TARGET_WINUSER) != 0u)
        && (windows_vfs64_append_literal_to_path(path, capacity, path_bytes, "P") != 0u)
        && (windows_vfs64_append_decimal_to_path(path, capacity, path_bytes, pid) != 0u)
        && (windows_vfs64_append_literal_to_path(
                path,
                capacity,
                path_bytes,
                "/LimitlessUser") != 0u);
}

static u32 windows_vfs64_build_profile_child_path(
    u32 pid,
    const char *child,
    u8 *path,
    u32 capacity,
    u32 *path_bytes)
{
    if (windows_vfs64_build_profile_base_path(pid, path, capacity, path_bytes) == 0u)
    {
        return 0u;
    }

    return (windows_vfs64_append_literal_to_path(path, capacity, path_bytes, child) != 0u)
        ? 1u
        : 0u;
}

static u32 windows_vfs64_build_temp_base_path(
    u32 pid,
    u8 *path,
    u32 capacity,
    u32 *path_bytes)
{
    u32 index;

    if ((path == 0) || (path_bytes == 0))
    {
        return 0u;
    }

    for (index = 0u; index < capacity; ++index)
    {
        path[index] = 0u;
    }
    *path_bytes = 0u;

    return (windows_vfs64_append_literal_to_path(
                path,
                capacity,
                path_bytes,
                WINDOWS_VFS64_TARGET_WINTMP) != 0u)
        && (windows_vfs64_append_literal_to_path(path, capacity, path_bytes, "P") != 0u)
        && (windows_vfs64_append_decimal_to_path(path, capacity, path_bytes, pid) != 0u);
}

static u32 windows_vfs64_create_profile_directory(
    u32 base_capability,
    u32 owner_id,
    const u8 *path,
    u32 path_bytes,
    u32 mask_bit,
    u32 *mask)
{
    u32 node_capability;

    if ((path == 0) || (path_bytes == 0u) || (mask == 0))
    {
        return 0u;
    }

    node_capability = fs64_create_kernel(
        base_capability,
        path,
        path_bytes,
        RAMFS_NODE_DIRECTORY,
        owner_id);
    if (node_capability == FS64_INVALID_HANDLE)
    {
        return 0u;
    }

    (void)fs64_revoke(node_capability, owner_id);
    *mask |= mask_bit;
    return 1u;
}

static u32 windows_vfs64_prepare_profile_tree(
    u32 pid,
    u32 owner_id,
    u32 base_capability,
    u32 *dir_mask_out)
{
    u8 path[WINDOWS_VFS64_PROFILE_PATH_BYTES];
    u32 path_bytes;
    u32 dir_mask = 0u;

    if (dir_mask_out == 0)
    {
        return 0u;
    }
    *dir_mask_out = 0u;

    if (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            (const u8 *)"/WINUSER",
            8u,
            WINDOWS_VFS64_PROFILE_MASK_ROOT,
            &dir_mask) == 0u)
    {
        return 0u;
    }

    path_bytes = 0u;
    if ((windows_vfs64_append_literal_to_path(path, WINDOWS_VFS64_PROFILE_PATH_BYTES, &path_bytes, "/WINUSER/P") == 0u)
        || (windows_vfs64_append_decimal_to_path(path, WINDOWS_VFS64_PROFILE_PATH_BYTES, &path_bytes, pid) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_PROFILE_MASK_PID,
            &dir_mask) == 0u)
        || (windows_vfs64_build_profile_base_path(
            pid,
            path,
            WINDOWS_VFS64_PROFILE_PATH_BYTES,
            &path_bytes) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_PROFILE_MASK_USER,
            &dir_mask) == 0u)
        || (windows_vfs64_build_profile_child_path(
            pid,
            "/AppData",
            path,
            WINDOWS_VFS64_PROFILE_PATH_BYTES,
            &path_bytes) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_PROFILE_MASK_APPDATA,
            &dir_mask) == 0u)
        || (windows_vfs64_build_profile_child_path(
            pid,
            "/AppData/Local",
            path,
            WINDOWS_VFS64_PROFILE_PATH_BYTES,
            &path_bytes) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_PROFILE_MASK_LOCAL,
            &dir_mask) == 0u)
        || (windows_vfs64_build_profile_child_path(
            pid,
            "/AppData/Roaming",
            path,
            WINDOWS_VFS64_PROFILE_PATH_BYTES,
            &path_bytes) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_PROFILE_MASK_ROAMING,
            &dir_mask) == 0u)
        || (windows_vfs64_build_profile_child_path(
            pid,
            "/Desktop",
            path,
            WINDOWS_VFS64_PROFILE_PATH_BYTES,
            &path_bytes) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_PROFILE_MASK_DESKTOP,
            &dir_mask) == 0u)
        || (windows_vfs64_build_profile_child_path(
            pid,
            "/Documents",
            path,
            WINDOWS_VFS64_PROFILE_PATH_BYTES,
            &path_bytes) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_PROFILE_MASK_DOCUMENTS,
            &dir_mask) == 0u))
    {
        *dir_mask_out = dir_mask;
        return 0u;
    }

    *dir_mask_out = dir_mask;
    return (dir_mask == WINDOWS_VFS64_PROFILE_DIR_MASK_COMPLETE) ? 1u : 0u;
}

static u32 windows_vfs64_prepare_temp_tree(
    u32 pid,
    u32 owner_id,
    u32 base_capability,
    u32 *dir_mask_out)
{
    u8 path[WINDOWS_VFS64_PROFILE_PATH_BYTES];
    u32 path_bytes;
    u32 dir_mask = 0u;

    if (dir_mask_out == 0)
    {
        return 0u;
    }
    *dir_mask_out = 0u;

    if (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            (const u8 *)"/WINTMP",
            7u,
            WINDOWS_VFS64_TEMP_MASK_ROOT,
            &dir_mask) == 0u)
    {
        return 0u;
    }

    if ((windows_vfs64_build_temp_base_path(
            pid,
            path,
            WINDOWS_VFS64_PROFILE_PATH_BYTES,
            &path_bytes) == 0u)
        || (windows_vfs64_create_profile_directory(
            base_capability,
            owner_id,
            path,
            path_bytes,
            WINDOWS_VFS64_TEMP_MASK_PID,
            &dir_mask) == 0u))
    {
        *dir_mask_out = dir_mask;
        return 0u;
    }

    *dir_mask_out = dir_mask;
    return (dir_mask == WINDOWS_VFS64_TEMP_DIR_MASK_COMPLETE) ? 1u : 0u;
}

static u32 windows_vfs64_finish_route(
    windows_vfs64_route_result_t *result,
    u32 status,
    u32 route_counted,
    u32 denied)
{
    if (result != 0)
    {
        result->status = status;
        result->target_hash =
            (result->target_bytes != 0u)
                ? windows_vfs64_target_hash(result->target_path, result->target_bytes)
                : 0u;
        g_windows_vfs64_last_route_type = result->route_type;
        g_windows_vfs64_last_target_hash = result->target_hash;
        g_windows_vfs64_last_target_bytes = result->target_bytes;
        g_windows_vfs64_last_device_id = result->device_id;
        g_windows_vfs64_last_route_unavailable = result->unavailable;
        g_windows_vfs64_last_backend_endpoint = result->backend_endpoint;
    }
    else
    {
        g_windows_vfs64_last_route_type = WINDOWS_VFS64_ROUTE_UNKNOWN;
        g_windows_vfs64_last_target_hash = 0u;
        g_windows_vfs64_last_target_bytes = 0u;
        g_windows_vfs64_last_device_id = WINDOWS_VFS64_DEVICE_NONE;
        g_windows_vfs64_last_route_unavailable = 0u;
        g_windows_vfs64_last_backend_endpoint = 0u;
    }

    if (route_counted != 0u)
    {
        ++g_windows_vfs64_route_count;
    }
    if (denied != 0u)
    {
        ++g_windows_vfs64_route_denial_count;
    }

    g_windows_vfs64_last_result = status;
    return status;
}

static u32 windows_vfs64_name_equals(
    const u8 *path,
    u32 path_bytes,
    u32 offset,
    const char *expected_name)
{
    u32 expected_bytes = 0u;
    u32 index;

    if ((path == 0) || (expected_name == 0) || (offset > path_bytes))
    {
        return 0u;
    }
    while (expected_name[expected_bytes] != 0)
    {
        ++expected_bytes;
    }
    if ((path_bytes - offset) != expected_bytes)
    {
        return 0u;
    }

    for (index = 0u; index < expected_bytes; ++index)
    {
        u8 actual = windows_vfs64_lower(path[offset + index]);
        u8 expected = windows_vfs64_lower((u8)expected_name[index]);
        if (actual == (u8)'/')
        {
            actual = (u8)'\\';
        }
        if (expected == (u8)'/')
        {
            expected = (u8)'\\';
        }
        if (actual != expected)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 windows_vfs64_shim_id_for_path(const u8 *path, u32 path_bytes)
{
    u32 system32_prefix_bytes =
        (u32)(sizeof(WINDOWS_VFS64_SYSTEM32_PREFIX) - 1u);

    if (windows_vfs64_prefix_matches(
            path,
            path_bytes,
            WINDOWS_VFS64_SYSTEM32_PREFIX,
            system32_prefix_bytes) == 0u)
    {
        return 0u;
    }
    if (windows_vfs64_name_equals(path, path_bytes, system32_prefix_bytes, "ntdll.dll") != 0u)
    {
        return WINDOWS_VFS64_SHIM_ID_NTDLL;
    }
    if (windows_vfs64_name_equals(path, path_bytes, system32_prefix_bytes, "kernel32.dll") != 0u)
    {
        return WINDOWS_VFS64_SHIM_ID_KERNEL32;
    }
    if (windows_vfs64_name_equals(path, path_bytes, system32_prefix_bytes, "msvcrt.dll") != 0u)
    {
        return WINDOWS_VFS64_SHIM_ID_MSVCRT;
    }
    if (windows_vfs64_name_equals(path, path_bytes, system32_prefix_bytes, "ucrtbase.dll") != 0u)
    {
        return WINDOWS_VFS64_SHIM_ID_UCRTBASE;
    }

    return 0u;
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
        || (create_disposition == WINDOWS_ABI64_FILE_OPEN_IF)
        || (create_disposition == WINDOWS_ABI64_FILE_OVERWRITE_IF))
        ? 1u
        : 0u;
}

static u32 windows_vfs64_options_are_supported(u32 create_options)
{
    return ((create_options & ~WINDOWS_VFS64_ALLOWED_OPTIONS) == 0u) ? 1u : 0u;
}

void windows_vfs64_init(void)
{
    g_windows_vfs64_route_count = 0u;
    g_windows_vfs64_route_denial_count = 0u;
    g_windows_vfs64_open_count = 0u;
    g_windows_vfs64_denial_count = 0u;
    g_windows_vfs64_profile_create_count = 0u;
    g_windows_vfs64_profile_denial_count = 0u;
    g_windows_vfs64_profile_last_node = 0u;
    g_windows_vfs64_profile_last_dir_mask = 0u;
    g_windows_vfs64_temp_create_count = 0u;
    g_windows_vfs64_temp_write_count = 0u;
    g_windows_vfs64_temp_read_count = 0u;
    g_windows_vfs64_temp_delete_count = 0u;
    g_windows_vfs64_temp_denial_count = 0u;
    g_windows_vfs64_temp_last_node = 0u;
    g_windows_vfs64_temp_last_dir_mask = 0u;
    g_windows_vfs64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_vfs64_last_path_hash = 0u;
    g_windows_vfs64_last_path_bytes = 0u;
    g_windows_vfs64_last_route_type = WINDOWS_VFS64_ROUTE_UNKNOWN;
    g_windows_vfs64_last_target_hash = 0u;
    g_windows_vfs64_last_target_bytes = 0u;
    g_windows_vfs64_last_device_id = WINDOWS_VFS64_DEVICE_NONE;
    g_windows_vfs64_last_route_unavailable = 0u;
    g_windows_vfs64_last_shim_id = 0u;
    g_windows_vfs64_last_handle = 0ull;
    g_windows_vfs64_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_vfs64_last_backend_endpoint = 0u;
}

u32 windows_vfs64_route_path(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    windows_vfs64_route_result_t *out_result)
{
    u32 path_hash;
    u32 dos_prefix_bytes = (u32)(sizeof(WINDOWS_VFS64_DOS_C_PREFIX) - 1u);
    u32 temp_prefix_bytes = (u32)(sizeof(WINDOWS_VFS64_TEMP_PREFIX) - 1u);
    u32 users_prefix_bytes = (u32)(sizeof(WINDOWS_VFS64_USERS_PREFIX) - 1u);
    u32 profile_prefix_bytes =
        (u32)(sizeof(WINDOWS_VFS64_USER_PROFILE_PREFIX) - 1u);
    u32 unc_prefix_bytes = (u32)(sizeof(WINDOWS_VFS64_UNC_PREFIX) - 1u);

    windows_vfs64_clear_route_result(out_result);
    path_hash = windows_vfs64_path_hash(path, path_bytes);
    g_windows_vfs64_last_path_hash = path_hash;
    g_windows_vfs64_last_path_bytes = path_bytes;
    if (out_result != 0)
    {
        out_result->path_hash = path_hash;
        out_result->path_bytes = path_bytes;
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (path == 0)
        || (path_bytes == 0u)
        || (path_bytes > WINDOWS_VFS64_MAX_PATH_BYTES)
        || (out_result == 0))
    {
        return windows_vfs64_finish_route(
            out_result,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            0u,
            1u);
    }

    if (windows_vfs64_prefix_matches(
            path,
            path_bytes,
            WINDOWS_VFS64_TEMP_PREFIX,
            temp_prefix_bytes) != 0u)
    {
        out_result->route_type = WINDOWS_VFS64_ROUTE_TEMP;
        out_result->backend_endpoint =
            services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS);
        if (windows_vfs64_build_temp_base_path(
                pid,
                out_result->target_path,
                WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES,
                &out_result->target_bytes) == 0u)
        {
            return windows_vfs64_finish_route(
                out_result,
                WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
                0u,
                1u);
        }
        if (temp_prefix_bytes < path_bytes)
        {
            windows_vfs64_route_append_literal(out_result, "/");
            windows_vfs64_route_append_nt_tail_preserve(
                out_result,
                path,
                path_bytes,
                temp_prefix_bytes);
        }
        return windows_vfs64_finish_route(
            out_result,
            WINDOWS_ABI64_STATUS_SUCCESS,
            1u,
            0u);
    }

    if (windows_vfs64_prefix_matches(
            path,
            path_bytes,
            WINDOWS_VFS64_USERS_PREFIX,
            users_prefix_bytes) != 0u)
    {
        out_result->route_type = WINDOWS_VFS64_ROUTE_USERS;
        out_result->backend_endpoint =
            services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS);
        if (windows_vfs64_prefix_matches(
                path,
                path_bytes,
                WINDOWS_VFS64_USER_PROFILE_PREFIX,
                profile_prefix_bytes) == 0u)
        {
            windows_vfs64_route_append_literal(out_result, WINDOWS_VFS64_TARGET_WINUSER);
            return windows_vfs64_finish_route(
                out_result,
                WINDOWS_ABI64_STATUS_ACCESS_DENIED,
                1u,
                1u);
        }

        if (windows_vfs64_build_profile_base_path(
                pid,
                out_result->target_path,
                WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES,
                &out_result->target_bytes) == 0u)
        {
            return windows_vfs64_finish_route(
                out_result,
                WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
                0u,
                1u);
        }
        if (profile_prefix_bytes < path_bytes)
        {
            windows_vfs64_route_append_literal(out_result, "/");
            windows_vfs64_route_append_nt_tail_preserve(
                out_result,
                path,
                path_bytes,
                profile_prefix_bytes);
        }
        return windows_vfs64_finish_route(
            out_result,
            WINDOWS_ABI64_STATUS_SUCCESS,
            1u,
            0u);
    }

    if (windows_vfs64_prefix_matches(
            path,
            path_bytes,
            WINDOWS_VFS64_DOS_C_PREFIX,
            dos_prefix_bytes) != 0u)
    {
        out_result->route_type = WINDOWS_VFS64_ROUTE_DOS_C;
        out_result->backend_endpoint =
            services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS);
        windows_vfs64_route_append_literal(out_result, WINDOWS_VFS64_TARGET_WINROOT);
        windows_vfs64_route_append_nt_tail(out_result, path, path_bytes, dos_prefix_bytes);
        return windows_vfs64_finish_route(
            out_result,
            WINDOWS_ABI64_STATUS_SUCCESS,
            1u,
            0u);
    }

    if (windows_vfs64_prefix_matches(
            path,
            path_bytes,
            WINDOWS_VFS64_UNC_PREFIX,
            unc_prefix_bytes) != 0u)
    {
        out_result->route_type = WINDOWS_VFS64_ROUTE_UNC;
        out_result->unavailable = 1u;
        out_result->backend_endpoint =
            services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_NETWORK);
        windows_vfs64_route_append_literal(out_result, WINDOWS_VFS64_TARGET_UNC);
        windows_vfs64_route_append_nt_tail(out_result, path, path_bytes, unc_prefix_bytes);
        return windows_vfs64_finish_route(
            out_result,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            1u,
            1u);
    }

    if (windows_vfs64_prefix_matches(
            path,
            path_bytes,
            WINDOWS_VFS64_CONDRV_PREFIX,
            (u32)(sizeof(WINDOWS_VFS64_CONDRV_PREFIX) - 1u)) != 0u)
    {
        out_result->route_type = WINDOWS_VFS64_ROUTE_CONDRV;
        out_result->device_id = WINDOWS_VFS64_DEVICE_CONSOLE;
        out_result->backend_endpoint =
            services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE);
        windows_vfs64_route_append_literal(out_result, WINDOWS_VFS64_TARGET_CONSOLE);
        return windows_vfs64_finish_route(
            out_result,
            WINDOWS_ABI64_STATUS_SUCCESS,
            1u,
            0u);
    }

    if (windows_vfs64_path_equals(path, path_bytes, WINDOWS_VFS64_NULL_PATH) != 0u)
    {
        out_result->route_type = WINDOWS_VFS64_ROUTE_NULL;
        out_result->device_id = WINDOWS_VFS64_DEVICE_NULL;
        windows_vfs64_route_append_literal(out_result, WINDOWS_VFS64_TARGET_NULL);
        return windows_vfs64_finish_route(
            out_result,
            WINDOWS_ABI64_STATUS_SUCCESS,
            1u,
            0u);
    }

    return windows_vfs64_finish_route(
        out_result,
        WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND,
        0u,
        1u);
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
    u32 shim_id;
    u32 node_id = 0u;
    u32 dir_mask = 0u;
    u32 fs_node_handle;
    windows_vfs64_route_result_t route_result;

    windows_vfs64_clear_open_result(out_result);
    path_hash = windows_vfs64_path_hash(path, path_bytes);
    g_windows_vfs64_last_path_hash = path_hash;
    g_windows_vfs64_last_path_bytes = path_bytes;
    g_windows_vfs64_last_shim_id = 0u;
    g_windows_vfs64_last_handle = 0ull;
    g_windows_vfs64_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_vfs64_last_backend_endpoint = 0u;
    g_windows_vfs64_profile_last_node = 0u;
    g_windows_vfs64_temp_last_node = 0u;

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
    status = windows_vfs64_route_path(pid, path, path_bytes, &route_result);
    if (status != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        ++g_windows_vfs64_denial_count;
        if (route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
        {
            ++g_windows_vfs64_profile_denial_count;
        }
        else if (route_result.route_type == WINDOWS_VFS64_ROUTE_TEMP)
        {
            ++g_windows_vfs64_temp_denial_count;
        }
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        out_result->path_hash = route_result.path_hash;
        out_result->path_bytes = route_result.path_bytes;
        out_result->backend_endpoint = route_result.backend_endpoint;
        return status;
    }
    if ((route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
        || (route_result.route_type == WINDOWS_VFS64_ROUTE_TEMP))
    {
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
            if (route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
            {
                ++g_windows_vfs64_profile_denial_count;
            }
            else
            {
                ++g_windows_vfs64_temp_denial_count;
            }
            g_windows_vfs64_last_result = status;
            out_result->status = status;
            return status;
        }

        if (((route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
                && (windows_vfs64_prepare_profile_tree(
                    pid,
                    owner_id,
                    capability_handle,
                    &dir_mask) == 0u))
            || ((route_result.route_type == WINDOWS_VFS64_ROUTE_TEMP)
                && (windows_vfs64_prepare_temp_tree(
                    pid,
                    owner_id,
                    capability_handle,
                    &dir_mask) == 0u)))
        {
            (void)capability64_revoke(capability_handle, owner_id);
            status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
            ++g_windows_vfs64_denial_count;
            if (route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
            {
                ++g_windows_vfs64_profile_denial_count;
                g_windows_vfs64_profile_last_dir_mask = dir_mask;
            }
            else
            {
                ++g_windows_vfs64_temp_denial_count;
                g_windows_vfs64_temp_last_dir_mask = dir_mask;
            }
            g_windows_vfs64_last_result = status;
            out_result->status = status;
            return status;
        }
        if (route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
        {
            g_windows_vfs64_profile_last_dir_mask = dir_mask;
        }
        else
        {
            g_windows_vfs64_temp_last_dir_mask = dir_mask;
        }

        if (create_disposition == WINDOWS_ABI64_FILE_OPEN)
        {
            fs_node_handle = fs64_open_kernel(
                capability_handle,
                route_result.target_path,
                route_result.target_bytes,
                owner_id);
        }
        else
        {
            fs_node_handle = fs64_create_kernel(
                capability_handle,
                route_result.target_path,
                route_result.target_bytes,
                RAMFS_NODE_FILE,
                owner_id);
        }
        if (fs_node_handle == FS64_INVALID_HANDLE)
        {
            (void)capability64_revoke(capability_handle, owner_id);
            status = (create_disposition == WINDOWS_ABI64_FILE_OPEN)
                ? WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND
                : WINDOWS_ABI64_STATUS_INVALID_HANDLE;
            ++g_windows_vfs64_denial_count;
            if (route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
            {
                ++g_windows_vfs64_profile_denial_count;
            }
            else
            {
                ++g_windows_vfs64_temp_denial_count;
            }
            g_windows_vfs64_last_result = status;
            out_result->status = status;
            out_result->backend_endpoint = endpoint;
            return status;
        }

        (void)ramfs_open(
            ramfs_root_node(),
            route_result.target_path,
            route_result.target_bytes,
            &node_id);

        handle = windows_handle64_install(
            pid,
            fs_node_handle,
            WINDOWS_HANDLE64_TYPE_FILE,
            rights,
            0u);
        if (handle == WINDOWS_HANDLE64_INVALID)
        {
            (void)capability64_revoke(capability_handle, owner_id);
            (void)fs64_revoke(fs_node_handle, owner_id);
            status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
            ++g_windows_vfs64_denial_count;
            if (route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
            {
                ++g_windows_vfs64_profile_denial_count;
            }
            else
            {
                ++g_windows_vfs64_temp_denial_count;
            }
            g_windows_vfs64_last_result = status;
            out_result->status = status;
            out_result->backend_endpoint = endpoint;
            return status;
        }

        ++g_windows_vfs64_open_count;
        if (route_result.route_type == WINDOWS_VFS64_ROUTE_USERS)
        {
            ++g_windows_vfs64_profile_create_count;
            g_windows_vfs64_profile_last_node = node_id;
        }
        else
        {
            ++g_windows_vfs64_temp_create_count;
            g_windows_vfs64_temp_last_node = node_id;
        }
        (void)capability64_revoke(capability_handle, owner_id);
        g_windows_vfs64_last_result = status;
        g_windows_vfs64_last_handle = handle;
        g_windows_vfs64_last_capability = fs_node_handle;
        g_windows_vfs64_last_backend_endpoint = endpoint;
        out_result->handle = handle;
        out_result->capability_handle = fs_node_handle;
        out_result->status = status;
        out_result->node_id = node_id;
        out_result->backend_endpoint = endpoint;
        return status;
    }
    if (route_result.route_type != WINDOWS_VFS64_ROUTE_DOS_C)
    {
        status = WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        out_result->path_hash = route_result.path_hash;
        out_result->path_bytes = route_result.path_bytes;
        out_result->backend_endpoint = route_result.backend_endpoint;
        return status;
    }
    shim_id = windows_vfs64_shim_id_for_path(path, path_bytes);
    if (shim_id == 0u)
    {
        status = WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        g_windows_vfs64_last_backend_endpoint = 0u;
        out_result->status = status;
        out_result->backend_endpoint = 0u;
        return status;
    }
    if (create_disposition == WINDOWS_ABI64_FILE_OVERWRITE_IF)
    {
        status = WINDOWS_ABI64_STATUS_ACCESS_DENIED;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        out_result->status = status;
        out_result->backend_endpoint = route_result.backend_endpoint;
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
    g_windows_vfs64_last_shim_id = shim_id;
    g_windows_vfs64_last_handle = handle;
    g_windows_vfs64_last_capability = capability_handle;
    g_windows_vfs64_last_backend_endpoint = endpoint;
    out_result->handle = handle;
    out_result->capability_handle = capability_handle;
    out_result->status = status;
    out_result->shim_id = shim_id;
    out_result->backend_endpoint = endpoint;
    return status;
}

u32 windows_vfs64_write_file(
    u32 pid,
    u64 handle,
    const u8 *input,
    u32 byte_count,
    u32 file_offset,
    u32 *bytes_written_out)
{
    u32 owner_id;
    u32 node_capability;
    u32 bytes_written;
    u32 status;

    if (bytes_written_out != 0)
    {
        *bytes_written_out = 0u;
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (handle == WINDOWS_HANDLE64_INVALID)
        || ((input == 0) && (byte_count != 0u))
        || (bytes_written_out == 0))
    {
        status = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    owner_id = process64_principal(pid);
    node_capability = windows_handle64_entry_capability(pid, handle);
    if ((windows_handle64_entry_type(pid, handle) != WINDOWS_HANDLE64_TYPE_FILE)
        || (fs64_handle_is_node(node_capability, owner_id) == 0u))
    {
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    bytes_written = fs64_write_kernel(
        node_capability,
        input,
        file_offset,
        byte_count,
        owner_id);
    if (bytes_written == FS64_INVALID_HANDLE)
    {
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    *bytes_written_out = bytes_written;
    ++g_windows_vfs64_temp_write_count;
    g_windows_vfs64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

u32 windows_vfs64_read_file(
    u32 pid,
    u64 handle,
    u8 *output,
    u32 byte_count,
    u32 file_offset,
    u32 *bytes_read_out)
{
    u32 owner_id;
    u32 node_capability;
    u32 bytes_read;
    u32 status;

    if (bytes_read_out != 0)
    {
        *bytes_read_out = 0u;
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (handle == WINDOWS_HANDLE64_INVALID)
        || ((output == 0) && (byte_count != 0u))
        || (bytes_read_out == 0))
    {
        status = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    owner_id = process64_principal(pid);
    node_capability = windows_handle64_entry_capability(pid, handle);
    if ((windows_handle64_entry_type(pid, handle) != WINDOWS_HANDLE64_TYPE_FILE)
        || (fs64_handle_is_node(node_capability, owner_id) == 0u))
    {
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    bytes_read = fs64_read_kernel(
        node_capability,
        output,
        file_offset,
        byte_count,
        owner_id);
    if (bytes_read == FS64_INVALID_HANDLE)
    {
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    *bytes_read_out = bytes_read;
    ++g_windows_vfs64_temp_read_count;
    g_windows_vfs64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

u32 windows_vfs64_delete_path(
    u32 pid,
    const u8 *path,
    u32 path_bytes)
{
    u32 owner_id;
    u32 base_capability;
    u32 status;
    u32 dir_mask = 0u;
    u32 deleted;
    u32 rights = CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY;
    windows_vfs64_route_result_t route_result;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (path == 0)
        || (path_bytes == 0u)
        || (path_bytes > WINDOWS_VFS64_MAX_PATH_BYTES))
    {
        status = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    status = windows_vfs64_route_path(pid, path, path_bytes, &route_result);
    if (status != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        ++g_windows_vfs64_denial_count;
        if (route_result.route_type == WINDOWS_VFS64_ROUTE_TEMP)
        {
            ++g_windows_vfs64_temp_denial_count;
        }
        g_windows_vfs64_last_result = status;
        return status;
    }
    if (route_result.route_type != WINDOWS_VFS64_ROUTE_TEMP)
    {
        status = WINDOWS_ABI64_STATUS_ACCESS_DENIED;
        ++g_windows_vfs64_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    owner_id = process64_principal(pid);
    base_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_RAMFS,
        rights,
        owner_id);
    if (base_capability == CAPABILITY64_INVALID_HANDLE)
    {
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    if (windows_vfs64_prepare_temp_tree(pid, owner_id, base_capability, &dir_mask) == 0u)
    {
        (void)capability64_revoke(base_capability, owner_id);
        status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_temp_last_dir_mask = dir_mask;
        g_windows_vfs64_last_result = status;
        return status;
    }

    deleted = fs64_delete_kernel(
        base_capability,
        route_result.target_path,
        route_result.target_bytes,
        owner_id);
    (void)capability64_revoke(base_capability, owner_id);
    g_windows_vfs64_temp_last_dir_mask = dir_mask;
    if (deleted == 0u)
    {
        status = WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND;
        ++g_windows_vfs64_denial_count;
        ++g_windows_vfs64_temp_denial_count;
        g_windows_vfs64_last_result = status;
        return status;
    }

    ++g_windows_vfs64_temp_delete_count;
    g_windows_vfs64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

u32 windows_vfs64_open_count(void)
{
    return g_windows_vfs64_open_count;
}

u32 windows_vfs64_route_count(void)
{
    return g_windows_vfs64_route_count;
}

u32 windows_vfs64_route_denial_count(void)
{
    return g_windows_vfs64_route_denial_count;
}

u32 windows_vfs64_last_route_type(void)
{
    return g_windows_vfs64_last_route_type;
}

u32 windows_vfs64_last_target_hash(void)
{
    return g_windows_vfs64_last_target_hash;
}

u32 windows_vfs64_last_target_bytes(void)
{
    return g_windows_vfs64_last_target_bytes;
}

u32 windows_vfs64_last_device_id(void)
{
    return g_windows_vfs64_last_device_id;
}

u32 windows_vfs64_last_route_unavailable(void)
{
    return g_windows_vfs64_last_route_unavailable;
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

u32 windows_vfs64_profile_create_count(void)
{
    return g_windows_vfs64_profile_create_count;
}

u32 windows_vfs64_profile_denial_count(void)
{
    return g_windows_vfs64_profile_denial_count;
}

u32 windows_vfs64_profile_last_node(void)
{
    return g_windows_vfs64_profile_last_node;
}

u32 windows_vfs64_profile_last_dir_mask(void)
{
    return g_windows_vfs64_profile_last_dir_mask;
}

u32 windows_vfs64_temp_create_count(void)
{
    return g_windows_vfs64_temp_create_count;
}

u32 windows_vfs64_temp_write_count(void)
{
    return g_windows_vfs64_temp_write_count;
}

u32 windows_vfs64_temp_read_count(void)
{
    return g_windows_vfs64_temp_read_count;
}

u32 windows_vfs64_temp_delete_count(void)
{
    return g_windows_vfs64_temp_delete_count;
}

u32 windows_vfs64_temp_denial_count(void)
{
    return g_windows_vfs64_temp_denial_count;
}

u32 windows_vfs64_temp_last_node(void)
{
    return g_windows_vfs64_temp_last_node;
}

u32 windows_vfs64_temp_last_dir_mask(void)
{
    return g_windows_vfs64_temp_last_dir_mask;
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
