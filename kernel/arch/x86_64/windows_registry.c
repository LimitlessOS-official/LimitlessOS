#include "windows_registry_x64.h"

#include "capability_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "windows_abi_x64.h"
#include "windows_handle_x64.h"

/*
 * L.5 adds the first Windows registry namespace stub. It exposes a small
 * in-memory HKLM tree for Win32 initialization reads, integrates with the
 * scoped NT handle table in windows_handle.c, and returns truthful NTSTATUS
 * denials for unknown keys, values, personas, and unsupported information
 * classes. The scaffold checkpoint proves open/query/create, key-handle
 * ownership, required buffer sizing, and missing-value denial.
 */

typedef struct windows_registry64_value
{
    u32 key_id;
    const char *name;
    const char *data;
} windows_registry64_value_t;

typedef struct windows_registry64_dynamic_key
{
    u32 active;
    u32 key_id;
    u32 path_bytes;
    u8 path[WINDOWS_REGISTRY64_MAX_PATH_BYTES];
} windows_registry64_dynamic_key_t;

static const char g_windows_registry64_codepage_path[] =
    "\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage";
static const char g_windows_registry64_current_version_path[] =
    "\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
static const char g_windows_registry64_environment_path[] =
    "\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";

static const windows_registry64_value_t g_windows_registry64_values[] = {
    {WINDOWS_REGISTRY64_KEY_CODEPAGE, "ACP", "1252"},
    {WINDOWS_REGISTRY64_KEY_CURRENT_VERSION, "CurrentVersion", "10.0"},
    {WINDOWS_REGISTRY64_KEY_CURRENT_VERSION, "BuildLabEx", "LimitlessOS-M21"},
    {WINDOWS_REGISTRY64_KEY_ENVIRONMENT, "SystemRoot", "C:\\Windows"},
    {WINDOWS_REGISTRY64_KEY_ENVIRONMENT, "Path", "C:\\Windows\\System32;C:\\Windows"}
};

static windows_registry64_dynamic_key_t
    g_windows_registry64_dynamic_keys[WINDOWS_REGISTRY64_DYNAMIC_KEY_LIMIT];
static u32 g_windows_registry64_open_count = 0u;
static u32 g_windows_registry64_create_count = 0u;
static u32 g_windows_registry64_query_count = 0u;
static u32 g_windows_registry64_denial_count = 0u;
static u32 g_windows_registry64_last_status = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_registry64_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;
static u32 g_windows_registry64_last_path_hash = 0u;
static u32 g_windows_registry64_last_value_hash = 0u;
static u32 g_windows_registry64_last_data_hash = 0u;
static u32 g_windows_registry64_last_required_bytes = 0u;

static u8 windows_registry64_lower(u8 value)
{
    if ((value >= (u8)'A') && (value <= (u8)'Z'))
    {
        return (u8)(value + ((u8)'a' - (u8)'A'));
    }

    return value;
}

static u32 windows_registry64_hash(const u8 *data, u32 data_bytes)
{
    u32 hash = 2166136261u;
    u32 index;

    if (data == 0)
    {
        return 0u;
    }

    for (index = 0u; index < data_bytes; ++index)
    {
        u8 value = windows_registry64_lower(data[index]);
        if (value == (u8)'/')
        {
            value = (u8)'\\';
        }
        hash ^= value;
        hash *= 16777619u;
    }

    return hash;
}

static u32 windows_registry64_cstr_bytes(const char *text)
{
    u32 bytes = 0u;

    if (text == 0)
    {
        return 0u;
    }
    while (text[bytes] != 0)
    {
        ++bytes;
    }

    return bytes;
}

static u32 windows_registry64_equals(
    const u8 *left,
    u32 left_bytes,
    const u8 *right,
    u32 right_bytes)
{
    u32 index;

    if ((left == 0) || (right == 0) || (left_bytes != right_bytes))
    {
        return 0u;
    }

    for (index = 0u; index < left_bytes; ++index)
    {
        u8 a = windows_registry64_lower(left[index]);
        u8 b = windows_registry64_lower(right[index]);

        if (a == (u8)'/')
        {
            a = (u8)'\\';
        }
        if (b == (u8)'/')
        {
            b = (u8)'\\';
        }
        if (a != b)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 windows_registry64_path_matches(const u8 *path, u32 path_bytes, const char *known_path)
{
    return windows_registry64_equals(
        path,
        path_bytes,
        (const u8 *)known_path,
        windows_registry64_cstr_bytes(known_path));
}

static u32 windows_registry64_find_key(const u8 *path, u32 path_bytes)
{
    u32 index;

    if ((path == 0)
        || (path_bytes == 0u)
        || (path_bytes > WINDOWS_REGISTRY64_MAX_PATH_BYTES))
    {
        return WINDOWS_REGISTRY64_KEY_NONE;
    }
    if (windows_registry64_path_matches(
            path,
            path_bytes,
            g_windows_registry64_codepage_path) != 0u)
    {
        return WINDOWS_REGISTRY64_KEY_CODEPAGE;
    }
    if (windows_registry64_path_matches(
            path,
            path_bytes,
            g_windows_registry64_current_version_path) != 0u)
    {
        return WINDOWS_REGISTRY64_KEY_CURRENT_VERSION;
    }
    if (windows_registry64_path_matches(
            path,
            path_bytes,
            g_windows_registry64_environment_path) != 0u)
    {
        return WINDOWS_REGISTRY64_KEY_ENVIRONMENT;
    }

    for (index = 0u; index < WINDOWS_REGISTRY64_DYNAMIC_KEY_LIMIT; ++index)
    {
        if ((g_windows_registry64_dynamic_keys[index].active != 0u)
            && (windows_registry64_equals(
                path,
                path_bytes,
                g_windows_registry64_dynamic_keys[index].path,
                g_windows_registry64_dynamic_keys[index].path_bytes) != 0u))
        {
            return g_windows_registry64_dynamic_keys[index].key_id;
        }
    }

    return WINDOWS_REGISTRY64_KEY_NONE;
}

static u32 windows_registry64_create_dynamic_key(const u8 *path, u32 path_bytes)
{
    u32 index;
    u32 copy_index;

    if ((path == 0)
        || (path_bytes == 0u)
        || (path_bytes > WINDOWS_REGISTRY64_MAX_PATH_BYTES))
    {
        return WINDOWS_REGISTRY64_KEY_NONE;
    }

    for (index = 0u; index < WINDOWS_REGISTRY64_DYNAMIC_KEY_LIMIT; ++index)
    {
        if (g_windows_registry64_dynamic_keys[index].active == 0u)
        {
            g_windows_registry64_dynamic_keys[index].active = 1u;
            g_windows_registry64_dynamic_keys[index].key_id =
                WINDOWS_REGISTRY64_KEY_DYNAMIC_BASE + index;
            g_windows_registry64_dynamic_keys[index].path_bytes = path_bytes;
            for (copy_index = 0u; copy_index < path_bytes; ++copy_index)
            {
                g_windows_registry64_dynamic_keys[index].path[copy_index] =
                    path[copy_index];
            }
            return g_windows_registry64_dynamic_keys[index].key_id;
        }
    }

    return WINDOWS_REGISTRY64_KEY_NONE;
}

static void windows_registry64_clear_open_result(windows_registry64_open_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->handle = WINDOWS_HANDLE64_INVALID;
    result->status = WINDOWS_ABI64_STATUS_SUCCESS;
    result->key_id = WINDOWS_REGISTRY64_KEY_NONE;
    result->path_hash = 0u;
    result->path_bytes = 0u;
    result->created = 0u;
    result->disposition = 0u;
}

static void windows_registry64_clear_value_result(windows_registry64_value_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->status = WINDOWS_ABI64_STATUS_SUCCESS;
    result->key_id = WINDOWS_REGISTRY64_KEY_NONE;
    result->value_type = 0u;
    result->data_bytes = 0u;
    result->required_bytes = 0u;
    result->value_hash = 0u;
    result->data_hash = 0u;
    result->data_ascii = 0;
}

static u32 windows_registry64_record_denial(u32 status)
{
    ++g_windows_registry64_denial_count;
    g_windows_registry64_last_status = status;
    return status;
}

static u32 windows_registry64_validate_process(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_WINDOWS_PE))
        ? 1u
        : 0u;
}

void windows_registry64_init(void)
{
    u32 index;
    u32 byte_index;

    for (index = 0u; index < WINDOWS_REGISTRY64_DYNAMIC_KEY_LIMIT; ++index)
    {
        g_windows_registry64_dynamic_keys[index].active = 0u;
        g_windows_registry64_dynamic_keys[index].key_id =
            WINDOWS_REGISTRY64_KEY_DYNAMIC_BASE + index;
        g_windows_registry64_dynamic_keys[index].path_bytes = 0u;
        for (byte_index = 0u; byte_index < WINDOWS_REGISTRY64_MAX_PATH_BYTES; ++byte_index)
        {
            g_windows_registry64_dynamic_keys[index].path[byte_index] = 0u;
        }
    }
    g_windows_registry64_open_count = 0u;
    g_windows_registry64_create_count = 0u;
    g_windows_registry64_query_count = 0u;
    g_windows_registry64_denial_count = 0u;
    g_windows_registry64_last_status = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_registry64_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;
    g_windows_registry64_last_path_hash = 0u;
    g_windows_registry64_last_value_hash = 0u;
    g_windows_registry64_last_data_hash = 0u;
    g_windows_registry64_last_required_bytes = 0u;
}

u32 windows_registry64_open_key(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    u32 desired_access,
    windows_registry64_open_result_t *out_result)
{
    u32 key_id;
    u32 rights = CAPABILITY64_RIGHT_QUERY;
    u64 handle;

    windows_registry64_clear_open_result(out_result);
    g_windows_registry64_last_path_hash =
        windows_registry64_hash(path, path_bytes);
    g_windows_registry64_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;

    if ((windows_registry64_validate_process(pid) == 0u)
        || (path == 0)
        || (path_bytes == 0u)
        || (path_bytes > WINDOWS_REGISTRY64_MAX_PATH_BYTES)
        || (out_result == 0))
    {
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
    }
    (void)desired_access;

    key_id = windows_registry64_find_key(path, path_bytes);
    if (key_id == WINDOWS_REGISTRY64_KEY_NONE)
    {
        out_result->path_hash = g_windows_registry64_last_path_hash;
        out_result->path_bytes = path_bytes;
        out_result->status = WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND;
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND);
    }

    handle = windows_handle64_key_create(pid, key_id, rights, 0u);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        out_result->path_hash = g_windows_registry64_last_path_hash;
        out_result->path_bytes = path_bytes;
        out_result->status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_INVALID_HANDLE);
    }

    ++g_windows_registry64_open_count;
    g_windows_registry64_last_status = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_registry64_last_key_id = key_id;
    out_result->handle = handle;
    out_result->status = WINDOWS_ABI64_STATUS_SUCCESS;
    out_result->key_id = key_id;
    out_result->path_hash = g_windows_registry64_last_path_hash;
    out_result->path_bytes = path_bytes;
    out_result->created = 0u;
    out_result->disposition = WINDOWS_REGISTRY64_DISPOSITION_OPENED;
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

u32 windows_registry64_create_key(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    u32 desired_access,
    windows_registry64_open_result_t *out_result)
{
    u32 key_id;
    u32 created = 0u;
    u64 handle;

    windows_registry64_clear_open_result(out_result);
    g_windows_registry64_last_path_hash =
        windows_registry64_hash(path, path_bytes);
    g_windows_registry64_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;

    if ((windows_registry64_validate_process(pid) == 0u)
        || (path == 0)
        || (path_bytes == 0u)
        || (path_bytes > WINDOWS_REGISTRY64_MAX_PATH_BYTES)
        || (out_result == 0))
    {
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
    }
    (void)desired_access;

    key_id = windows_registry64_find_key(path, path_bytes);
    if (key_id == WINDOWS_REGISTRY64_KEY_NONE)
    {
        key_id = windows_registry64_create_dynamic_key(path, path_bytes);
        created = 1u;
    }
    if (key_id == WINDOWS_REGISTRY64_KEY_NONE)
    {
        out_result->path_hash = g_windows_registry64_last_path_hash;
        out_result->path_bytes = path_bytes;
        out_result->status = WINDOWS_ABI64_STATUS_NO_MEMORY;
        return windows_registry64_record_denial(WINDOWS_ABI64_STATUS_NO_MEMORY);
    }

    handle = windows_handle64_key_create(pid, key_id, CAPABILITY64_RIGHT_QUERY, 0u);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        out_result->path_hash = g_windows_registry64_last_path_hash;
        out_result->path_bytes = path_bytes;
        out_result->status = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_INVALID_HANDLE);
    }

    ++g_windows_registry64_create_count;
    g_windows_registry64_last_status = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_registry64_last_key_id = key_id;
    out_result->handle = handle;
    out_result->status = WINDOWS_ABI64_STATUS_SUCCESS;
    out_result->key_id = key_id;
    out_result->path_hash = g_windows_registry64_last_path_hash;
    out_result->path_bytes = path_bytes;
    out_result->created = created;
    out_result->disposition = (created != 0u)
        ? WINDOWS_REGISTRY64_DISPOSITION_CREATED
        : WINDOWS_REGISTRY64_DISPOSITION_OPENED;
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

u32 windows_registry64_query_value(
    u32 pid,
    u64 handle,
    const u8 *value_name,
    u32 value_name_bytes,
    u32 information_class,
    windows_registry64_value_result_t *out_result)
{
    u32 key_id;
    u32 index;
    const windows_registry64_value_t *value = 0;
    u32 data_bytes;
    u32 value_bytes;

    windows_registry64_clear_value_result(out_result);
    g_windows_registry64_last_value_hash =
        windows_registry64_hash(value_name, value_name_bytes);
    g_windows_registry64_last_data_hash = 0u;
    g_windows_registry64_last_required_bytes = 0u;

    if ((windows_registry64_validate_process(pid) == 0u)
        || (handle == WINDOWS_HANDLE64_INVALID)
        || (value_name == 0)
        || (value_name_bytes == 0u)
        || (value_name_bytes > WINDOWS_REGISTRY64_MAX_VALUE_NAME_BYTES)
        || (out_result == 0))
    {
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
    }
    if (information_class != WINDOWS_REGISTRY64_VALUE_INFORMATION_PARTIAL)
    {
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_INVALID_INFO_CLASS);
    }

    key_id = windows_handle64_key_id(pid, handle);
    if (key_id == WINDOWS_REGISTRY64_KEY_NONE)
    {
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_INVALID_HANDLE);
    }
    out_result->key_id = key_id;

    for (index = 0u;
         index < (u32)(sizeof(g_windows_registry64_values) / sizeof(g_windows_registry64_values[0]));
         ++index)
    {
        if (g_windows_registry64_values[index].key_id != key_id)
        {
            continue;
        }

        value_bytes = windows_registry64_cstr_bytes(
            g_windows_registry64_values[index].name);
        if (windows_registry64_equals(
                value_name,
                value_name_bytes,
                (const u8 *)g_windows_registry64_values[index].name,
                value_bytes) != 0u)
        {
            value = &g_windows_registry64_values[index];
            break;
        }
    }
    if (value == 0)
    {
        g_windows_registry64_last_key_id = key_id;
        return windows_registry64_record_denial(
            WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND);
    }

    data_bytes = windows_registry64_cstr_bytes(value->data) * 2u + 2u;
    ++g_windows_registry64_query_count;
    g_windows_registry64_last_status = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_registry64_last_key_id = key_id;
    g_windows_registry64_last_data_hash =
        windows_registry64_hash((const u8 *)value->data, windows_registry64_cstr_bytes(value->data));
    g_windows_registry64_last_required_bytes = 12u + data_bytes;
    out_result->status = WINDOWS_ABI64_STATUS_SUCCESS;
    out_result->key_id = key_id;
    out_result->value_type = WINDOWS_REGISTRY64_REG_SZ;
    out_result->data_bytes = data_bytes;
    out_result->required_bytes = 12u + data_bytes;
    out_result->value_hash = g_windows_registry64_last_value_hash;
    out_result->data_hash = g_windows_registry64_last_data_hash;
    out_result->data_ascii = (const u8 *)value->data;
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

u32 windows_registry64_open_count(void)
{
    return g_windows_registry64_open_count;
}

u32 windows_registry64_create_count(void)
{
    return g_windows_registry64_create_count;
}

u32 windows_registry64_query_count(void)
{
    return g_windows_registry64_query_count;
}

u32 windows_registry64_denial_count(void)
{
    return g_windows_registry64_denial_count;
}

u32 windows_registry64_last_status(void)
{
    return g_windows_registry64_last_status;
}

u32 windows_registry64_last_key_id(void)
{
    return g_windows_registry64_last_key_id;
}

u32 windows_registry64_last_path_hash(void)
{
    return g_windows_registry64_last_path_hash;
}

u32 windows_registry64_last_value_hash(void)
{
    return g_windows_registry64_last_value_hash;
}

u32 windows_registry64_last_data_hash(void)
{
    return g_windows_registry64_last_data_hash;
}

u32 windows_registry64_last_required_bytes(void)
{
    return g_windows_registry64_last_required_bytes;
}

u32 windows_registry64_live_dynamic_count(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0u; index < WINDOWS_REGISTRY64_DYNAMIC_KEY_LIMIT; ++index)
    {
        if (g_windows_registry64_dynamic_keys[index].active != 0u)
        {
            ++count;
        }
    }

    return count;
}
