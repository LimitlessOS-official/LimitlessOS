#include "boot_media_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

static const struct boot_info *g_boot_media64_boot_info = 0;
static u32 g_boot_media64_last_error = BOOT_MEDIA64_ERROR_UNAVAILABLE;
static u32 g_boot_media64_last_bytes = 0u;
static u32 g_boot_media64_last_capacity = 0u;

static u8 boot_media64_lower(u8 value)
{
    if (value >= (u8)'A' && value <= (u8)'Z')
    {
        return (u8)(value + ((u8)'a' - (u8)'A'));
    }
    return value;
}

static u32 boot_media64_path_equals(const u8 *path, u32 path_bytes, const char *expected)
{
    u32 index;

    if (path == 0 || expected == 0 || path_bytes == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < path_bytes; ++index)
    {
        if (expected[index] == '\0')
        {
            return 0u;
        }
        if (boot_media64_lower(path[index]) != (u8)expected[index])
        {
            return 0u;
        }
    }

    return (expected[path_bytes] == '\0') ? 1u : 0u;
}

static void boot_media64_copy(u8 *destination, const u8 *source, u32 bytes)
{
    u32 index;

    for (index = 0u; index < bytes; ++index)
    {
        destination[index] = source[index];
    }
}

void boot_media64_init(const struct boot_info *boot_info)
{
    g_boot_media64_boot_info = boot_info;
    g_boot_media64_last_error = BOOT_MEDIA64_ERROR_NONE;
    g_boot_media64_last_bytes = 0u;
    g_boot_media64_last_capacity = 0u;
}

u32 boot_media64_available(void)
{
    return (g_boot_media64_boot_info != 0
        && g_boot_media64_boot_info->magic == LIMITLESS_BOOT_INFO_MAGIC
        && (g_boot_media64_boot_info->bootstrap_flags & LIMITLESS_BOOT_FLAG_BOOT_MEDIA_APPS) != 0u
        && g_boot_media64_boot_info->boot_media_app_base != 0ull
        && g_boot_media64_boot_info->boot_media_app_bytes != 0u)
        ? 1u
        : 0u;
}

u32 boot_media64_has_file(const u8 *path, u32 path_bytes)
{
    if (boot_media64_available() == 0u)
    {
        return 0u;
    }
    if (boot_media64_path_equals(path, path_bytes, "/apps/dynldlimit") != 0u)
    {
        return 1u;
    }
    if (g_boot_media64_boot_info->boot_media_interp_base != 0ull
        && g_boot_media64_boot_info->boot_media_interp_bytes != 0u
        && boot_media64_path_equals(path, path_bytes, "/apps/ldlimit") != 0u)
    {
        return 1u;
    }
    return 0u;
}

u32 boot_media64_read_file(const u8 *path, u32 path_bytes, u8 *buffer, u32 capacity, u32 *bytes_read)
{
    const u8 *source = 0;
    u32 source_bytes = 0u;

    g_boot_media64_last_error = BOOT_MEDIA64_ERROR_NONE;
    g_boot_media64_last_bytes = 0u;
    g_boot_media64_last_capacity = capacity;
    if (bytes_read != 0)
    {
        *bytes_read = 0u;
    }

    if (path == 0 || path_bytes == 0u || buffer == 0 || bytes_read == 0)
    {
        g_boot_media64_last_error = BOOT_MEDIA64_ERROR_ARGUMENT;
        return 0u;
    }
    if (boot_media64_available() == 0u)
    {
        g_boot_media64_last_error = BOOT_MEDIA64_ERROR_UNAVAILABLE;
        return 0u;
    }

    if (boot_media64_path_equals(path, path_bytes, "/apps/dynldlimit") != 0u)
    {
        source = (const u8 *)(u64)g_boot_media64_boot_info->boot_media_app_base;
        source_bytes = g_boot_media64_boot_info->boot_media_app_bytes;
    }
    else if (boot_media64_path_equals(path, path_bytes, "/apps/ldlimit") != 0u)
    {
        source = (const u8 *)(u64)g_boot_media64_boot_info->boot_media_interp_base;
        source_bytes = g_boot_media64_boot_info->boot_media_interp_bytes;
    }

    if (source == 0 || source_bytes == 0u)
    {
        g_boot_media64_last_error = BOOT_MEDIA64_ERROR_NOT_FOUND;
        return 0u;
    }
    if (source_bytes > capacity)
    {
        g_boot_media64_last_error = BOOT_MEDIA64_ERROR_CAPACITY;
        g_boot_media64_last_bytes = source_bytes;
        return 0u;
    }

    boot_media64_copy(buffer, source, source_bytes);
    *bytes_read = source_bytes;
    g_boot_media64_last_bytes = source_bytes;
    return 1u;
}

u32 boot_media64_last_error(void) { return g_boot_media64_last_error; }
u32 boot_media64_last_bytes(void) { return g_boot_media64_last_bytes; }
u32 boot_media64_last_capacity(void) { return g_boot_media64_last_capacity; }
u32 boot_media64_app_bytes(void)
{
    return (g_boot_media64_boot_info != 0) ? g_boot_media64_boot_info->boot_media_app_bytes : 0u;
}
u32 boot_media64_interp_bytes(void)
{
    return (g_boot_media64_boot_info != 0) ? g_boot_media64_boot_info->boot_media_interp_bytes : 0u;
}
u32 boot_media64_flags(void)
{
    return (g_boot_media64_boot_info != 0) ? g_boot_media64_boot_info->boot_media_flags : 0u;
}
u32 boot_media64_status(void)
{
    return (g_boot_media64_boot_info != 0) ? g_boot_media64_boot_info->boot_media_status : 0u;
}

#else

void boot_media64_init(const struct boot_info *boot_info) { (void)boot_info; }
u32 boot_media64_available(void) { return 0u; }
u32 boot_media64_has_file(const u8 *path, u32 path_bytes) { (void)path; (void)path_bytes; return 0u; }
u32 boot_media64_read_file(const u8 *path, u32 path_bytes, u8 *buffer, u32 capacity, u32 *bytes_read)
{
    (void)path;
    (void)path_bytes;
    (void)buffer;
    (void)capacity;
    if (bytes_read != 0) { *bytes_read = 0u; }
    return 0u;
}
u32 boot_media64_last_error(void) { return BOOT_MEDIA64_ERROR_UNAVAILABLE; }
u32 boot_media64_last_bytes(void) { return 0u; }
u32 boot_media64_last_capacity(void) { return 0u; }
u32 boot_media64_app_bytes(void) { return 0u; }
u32 boot_media64_interp_bytes(void) { return 0u; }
u32 boot_media64_flags(void) { return 0u; }
u32 boot_media64_status(void) { return 0u; }

#endif
