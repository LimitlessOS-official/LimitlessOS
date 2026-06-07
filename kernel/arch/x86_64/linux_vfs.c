#include "linux_vfs_x64.h"

#include "fs_x64.h"
#include "mmio_x64.h"
#include "process_x64.h"
#include "ramfs.h"
#include "services.h"
#include "vma_x64.h"

/*
 * G.1-G.8 add the first Linux-persona VFS path router, read-only /proc views,
 * and a per-process writable /tmp namespace. This code integrates with
 * fd_x64.h for descriptor state, process_x64.h for PID-based ownership checks,
 * vma_x64.h for /proc maps, and fs_x64.h for brokered RAMFS capabilities. The
 * scaffold checkpoint proves mount resolution, device files, proc maps derived
 * from real VMAs, proc identity payloads, fd symlink enumeration, meminfo values
 * derived from the VMA allocator, read-only proc denial, and create/write/read/
 * delete behavior in /tmp, symlink readlink/lstat/nofollow path walking, and
 * stat/lstat metadata for every current VFS provider without granting ambient
 * storage authority.
 */

static const u8 g_linux_vfs64_prefix_root[] = "/";
static const u8 g_linux_vfs64_prefix_proc[] = "/proc";
static const u8 g_linux_vfs64_prefix_dev[] = "/dev";
static const u8 g_linux_vfs64_prefix_tmp[] = "/tmp";
static const u8 g_linux_vfs64_prefix_nvme[] = "/nvme";

static const linux_vfs64_mount_t g_linux_vfs64_mounts[LINUX_VFS64_MAX_MOUNTS] = {
    { g_linux_vfs64_prefix_root, 1u, LINUX_VFS64_PROVIDER_RAMFS, LINUX_VFS64_OPEN_READ },
    { g_linux_vfs64_prefix_proc, 5u, LINUX_VFS64_PROVIDER_PROC, LINUX_VFS64_OPEN_READ },
    { g_linux_vfs64_prefix_dev, 4u, LINUX_VFS64_PROVIDER_DEV, LINUX_VFS64_OPEN_READ | LINUX_VFS64_OPEN_WRITE },
    { g_linux_vfs64_prefix_tmp, 4u, LINUX_VFS64_PROVIDER_RAMFS, LINUX_VFS64_OPEN_READ | LINUX_VFS64_OPEN_WRITE },
    { g_linux_vfs64_prefix_nvme, 5u, LINUX_VFS64_PROVIDER_NVME, LINUX_VFS64_OPEN_READ }
};

#define LINUX_VFS64_MAX_FD_PATH_RECORDS 16u

typedef struct linux_vfs64_fd_path_record
{
    u32 active;
    u32 pid;
    u32 fd_number;
    u32 path_byte_count;
    u32 dir_cursor;
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
} linux_vfs64_fd_path_record_t;

typedef struct linux_vfs64_dirent_template
{
    const char *name;
    u8 entry_type;
    u64 inode;
} linux_vfs64_dirent_template_t;

static const linux_vfs64_dirent_template_t g_linux_vfs64_root_dirents[] = {
    { "proc", LINUX_VFS64_DIRENT_TYPE_DIR, 0x1001ull },
    { "dev", LINUX_VFS64_DIRENT_TYPE_DIR, 0x1002ull },
    { "tmp", LINUX_VFS64_DIRENT_TYPE_DIR, 0x1003ull },
    { "nvme", LINUX_VFS64_DIRENT_TYPE_DIR, 0x1004ull }
};

static const linux_vfs64_dirent_template_t g_linux_vfs64_dev_dirents[] = {
    { "null", LINUX_VFS64_DIRENT_TYPE_CHR, 0x2001ull },
    { "zero", LINUX_VFS64_DIRENT_TYPE_CHR, 0x2002ull },
    { "urandom", LINUX_VFS64_DIRENT_TYPE_CHR, 0x2003ull },
    { "stdin", LINUX_VFS64_DIRENT_TYPE_CHR, 0x2004ull },
    { "stdout", LINUX_VFS64_DIRENT_TYPE_CHR, 0x2005ull },
    { "stderr", LINUX_VFS64_DIRENT_TYPE_CHR, 0x2006ull }
};

static const linux_vfs64_dirent_template_t g_linux_vfs64_proc_dirents[] = {
    { "self", LINUX_VFS64_DIRENT_TYPE_DIR, 0x3001ull },
    { "meminfo", LINUX_VFS64_DIRENT_TYPE_REG, 0x3002ull }
};

static const linux_vfs64_dirent_template_t g_linux_vfs64_proc_self_dirents[] = {
    { "maps", LINUX_VFS64_DIRENT_TYPE_REG, 0x3101ull },
    { "exe", LINUX_VFS64_DIRENT_TYPE_LNK, 0x3102ull },
    { "fd", LINUX_VFS64_DIRENT_TYPE_DIR, 0x3103ull },
    { "status", LINUX_VFS64_DIRENT_TYPE_REG, 0x3104ull },
    { "cmdline", LINUX_VFS64_DIRENT_TYPE_REG, 0x3105ull },
    { "environ", LINUX_VFS64_DIRENT_TYPE_REG, 0x3106ull }
};

#define LINUX_VFS64_MAX_PROC_RECORDS 16u
#define LINUX_VFS64_MAX_TMP_RECORDS 16u
#define LINUX_VFS64_PROC_FD_SHIFT 8u
#define LINUX_VFS64_PROC_FD_MASK 0x000000FFu
#define LINUX_VFS64_PROC_SCRATCH_BYTES 384u
#define LINUX_VFS64_TMP_BACKEND_BYTES LINUX_VFS64_MAX_PATH_BYTES
#define LINUX_VFS64_SYMLINK_MAX_DEPTH 4u
#define LINUX_VFS64_MAX_NVME_BINDINGS 4u
#define LINUX_VFS64_NVME_FILE_BYTES 4096u

typedef struct linux_vfs64_proc_record
{
    u32 active;
    u32 pid;
    u32 exe_path_bytes;
    u32 cmdline_bytes;
    u32 environ_bytes;
    u8 exe_path[LINUX_VFS64_PROC_IDENTITY_BYTES];
    u8 cmdline[LINUX_VFS64_PROC_PAYLOAD_BYTES];
    u8 environ[LINUX_VFS64_PROC_PAYLOAD_BYTES];
} linux_vfs64_proc_record_t;

typedef struct linux_vfs64_tmp_record
{
    u32 active;
    u32 pid;
    u32 node_type;
    u32 name_byte_count;
    u32 target_path_byte_count;
    u8 name[LINUX_VFS64_DIRENT_NAME_MAX];
    u8 target_path[LINUX_VFS64_MAX_PATH_BYTES];
} linux_vfs64_tmp_record_t;

typedef struct linux_vfs64_nvme_binding
{
    u32 active;
    u32 pid;
    u32 owner_id;
    u32 capability;
} linux_vfs64_nvme_binding_t;

static u32 g_linux_vfs64_initialized = 0u;
static u32 g_linux_vfs64_denial_count = 0u;
static u32 g_linux_vfs64_open_count = 0u;
static u32 g_linux_vfs64_read_count = 0u;
static u32 g_linux_vfs64_write_count = 0u;
static u32 g_linux_vfs64_proc_read_count = 0u;
static u32 g_linux_vfs64_proc_denial_count = 0u;
static u32 g_linux_vfs64_proc_last_maps_regions = 0u;
static u64 g_linux_vfs64_proc_last_maps_bytes = 0ull;
static u32 g_linux_vfs64_proc_last_exe_bytes = 0u;
static u32 g_linux_vfs64_proc_last_status_bytes = 0u;
static u32 g_linux_vfs64_proc_last_cmdline_bytes = 0u;
static u32 g_linux_vfs64_proc_last_environ_bytes = 0u;
static u32 g_linux_vfs64_proc_last_fd_entries = 0u;
static u32 g_linux_vfs64_proc_last_fd_target = FD64_INVALID_FD;
static u32 g_linux_vfs64_proc_last_meminfo_bytes = 0u;
static u32 g_linux_vfs64_proc_last_mem_total_kib = 0u;
static u32 g_linux_vfs64_proc_last_mem_free_kib = 0u;
static u32 g_linux_vfs64_proc_last_mem_available_kib = 0u;
static u32 g_linux_vfs64_proc_last_mem_claimed_kib = 0u;
static u32 g_linux_vfs64_tmp_create_count = 0u;
static u32 g_linux_vfs64_tmp_delete_count = 0u;
static u32 g_linux_vfs64_tmp_denial_count = 0u;
static u32 g_linux_vfs64_tmp_last_dir_entries = 0u;
static u32 g_linux_vfs64_tmp_last_backend_path_bytes = 0u;
static u32 g_linux_vfs64_tmp_last_namespace_pid = PROCESS64_INVALID_PID;
static u32 g_linux_vfs64_symlink_create_count = 0u;
static u32 g_linux_vfs64_symlink_follow_count = 0u;
static u32 g_linux_vfs64_symlink_readlink_count = 0u;
static u32 g_linux_vfs64_symlink_lstat_count = 0u;
static u32 g_linux_vfs64_symlink_nofollow_denial_count = 0u;
static u32 g_linux_vfs64_symlink_last_target_bytes = 0u;
static u32 g_linux_vfs64_nvme_bind_count = 0u;
static u32 g_linux_vfs64_nvme_release_count = 0u;
static u32 g_linux_vfs64_nvme_read_count = 0u;
static u32 g_linux_vfs64_nvme_readdir_count = 0u;
static u32 g_linux_vfs64_nvme_dirent_count = 0u;
static u32 g_linux_vfs64_nvme_denial_count = 0u;
static u32 g_linux_vfs64_nvme_last_bytes = 0u;
static u32 g_linux_vfs64_random_counter = 0x5A17A001u;
static linux_vfs64_fd_path_record_t g_linux_vfs64_fd_paths[LINUX_VFS64_MAX_FD_PATH_RECORDS];
static linux_vfs64_proc_record_t g_linux_vfs64_proc_records[LINUX_VFS64_MAX_PROC_RECORDS];
static linux_vfs64_tmp_record_t g_linux_vfs64_tmp_records[LINUX_VFS64_MAX_TMP_RECORDS];
static linux_vfs64_nvme_binding_t g_linux_vfs64_nvme_bindings[LINUX_VFS64_MAX_NVME_BINDINGS];

static u32 linux_vfs64_bytes_equal(const u8 *left, const u8 *right, u32 byte_count);

static void linux_vfs64_zero_result(linux_vfs64_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->provider = LINUX_VFS64_PROVIDER_NONE;
    result->node_type = LINUX_VFS64_NODE_UNKNOWN;
    result->device_type = LINUX_VFS64_DEVICE_UNKNOWN;
    result->capability_handle = CAPABILITY64_INVALID_HANDLE;
    result->mount_index = LINUX_VFS64_INVALID_RESULT;
    result->path_token = 0u;
    result->error = LINUX_VFS64_ERROR_NONE;
    result->denied = 0u;
}

static u32 linux_vfs64_deny(linux_vfs64_result_t *result, u32 error)
{
    ++g_linux_vfs64_denial_count;
    if (result != 0)
    {
        result->error = error;
        result->denied = 1u;
    }
    return 0u;
}

static u32 linux_vfs64_process_is_valid(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID) && (process64_principal(pid) != 0u)) ? 1u : 0u;
}

static void linux_vfs64_clear_nvme_binding(linux_vfs64_nvme_binding_t *binding)
{
    if (binding == 0)
    {
        return;
    }

    binding->active = 0u;
    binding->pid = PROCESS64_INVALID_PID;
    binding->owner_id = 0u;
    binding->capability = CAPABILITY64_INVALID_HANDLE;
}

static linux_vfs64_nvme_binding_t *linux_vfs64_find_nvme_binding(u32 pid)
{
    u32 index;

    for (index = 0u; index < LINUX_VFS64_MAX_NVME_BINDINGS; ++index)
    {
        if ((g_linux_vfs64_nvme_bindings[index].active != 0u)
            && (g_linux_vfs64_nvme_bindings[index].pid == pid))
        {
            return &g_linux_vfs64_nvme_bindings[index];
        }
    }

    return 0;
}

static u32 linux_vfs64_nvme_path_to_fat(
    const u8 *path,
    u32 path_byte_count,
    const u8 **fat_path_out,
    u32 *fat_path_bytes_out)
{
    if (fat_path_out != 0)
    {
        *fat_path_out = 0;
    }
    if (fat_path_bytes_out != 0)
    {
        *fat_path_bytes_out = 0u;
    }

    if ((path == 0)
        || (fat_path_out == 0)
        || (fat_path_bytes_out == 0)
        || (path_byte_count <= 6u)
        || (linux_vfs64_bytes_equal(path, g_linux_vfs64_prefix_nvme, 5u) == 0u)
        || (path[5] != (u8)'/'))
    {
        return 0u;
    }

    *fat_path_out = &path[5];
    *fat_path_bytes_out = path_byte_count - 5u;
    return 1u;
}

static u32 linux_vfs64_nvme_path_to_fat_rootable(
    const u8 *path,
    u32 path_byte_count,
    const u8 **fat_path_out,
    u32 *fat_path_bytes_out)
{
    static const u8 root_path[] = "/";

    if (fat_path_out != 0)
    {
        *fat_path_out = 0;
    }
    if (fat_path_bytes_out != 0)
    {
        *fat_path_bytes_out = 0u;
    }

    if ((path == 0)
        || (fat_path_out == 0)
        || (fat_path_bytes_out == 0)
        || (path_byte_count < 5u)
        || (linux_vfs64_bytes_equal(path, g_linux_vfs64_prefix_nvme, 5u) == 0u))
    {
        return 0u;
    }

    if (path_byte_count == 5u)
    {
        *fat_path_out = root_path;
        *fat_path_bytes_out = 1u;
        return 1u;
    }

    if (path[5] != (u8)'/')
    {
        return 0u;
    }

    *fat_path_out = &path[5];
    *fat_path_bytes_out = path_byte_count - 5u;
    return 1u;
}

static u32 linux_vfs64_nvme_read_all(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u8 *buffer,
    u32 buffer_bytes,
    u32 *bytes_out)
{
    const u8 *fat_path;
    u32 fat_path_bytes;
    linux_vfs64_nvme_binding_t *binding;

    if (bytes_out != 0)
    {
        *bytes_out = 0u;
    }

    binding = linux_vfs64_find_nvme_binding(pid);
    if ((binding == 0)
        || (buffer == 0)
        || (buffer_bytes == 0u)
        || (bytes_out == 0)
        || (binding->capability != mmio64_nvme_rw_capability())
        || (linux_vfs64_nvme_path_to_fat(
                path,
                path_byte_count,
                &fat_path,
                &fat_path_bytes) == 0u)
        || (mmio64_nvme_fat_shell_read_file(
                fat_path,
                fat_path_bytes,
                buffer,
                buffer_bytes,
                binding->owner_id,
                bytes_out) == 0u))
    {
        ++g_linux_vfs64_nvme_denial_count;
        return 0u;
    }

    ++g_linux_vfs64_nvme_read_count;
    g_linux_vfs64_nvme_last_bytes = *bytes_out;
    return 1u;
}

static u32 linux_vfs64_nvme_stat(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    mmio64_nvme_fat_stat_t *stat_out)
{
    const u8 *fat_path;
    u32 fat_path_bytes;
    linux_vfs64_nvme_binding_t *binding;

    if (stat_out != 0)
    {
        stat_out->entry_type = MMIO64_NVME_FAT_DIRENT_TYPE_UNKNOWN;
        stat_out->attr = 0u;
        stat_out->cluster = 0u;
        stat_out->byte_count = 0u;
    }

    binding = linux_vfs64_find_nvme_binding(pid);
    if ((binding == 0)
        || (stat_out == 0)
        || (binding->capability != mmio64_nvme_rw_capability())
        || (linux_vfs64_nvme_path_to_fat_rootable(
                path,
                path_byte_count,
                &fat_path,
                &fat_path_bytes) == 0u)
        || (mmio64_nvme_fat_shell_stat_path(
                fat_path,
                fat_path_bytes,
                binding->owner_id,
                stat_out) == 0u))
    {
        ++g_linux_vfs64_nvme_denial_count;
        return 0u;
    }

    return 1u;
}

static u32 linux_vfs64_copy_mmio_dirent(
    const mmio64_nvme_fat_dirent_t *source,
    linux_vfs64_dirent_t *target)
{
    u32 index;

    if ((source == 0)
        || (target == 0)
        || (source->name_byte_count == 0u)
        || (source->name_byte_count >= LINUX_VFS64_DIRENT_NAME_MAX))
    {
        return 0u;
    }

    target->inode = (source->cluster != 0u)
        ? (u64)source->cluster
        : (u64)source->next_cursor;
    target->next_offset = source->next_cursor;
    target->name_byte_count = source->name_byte_count;
    target->entry_type = (source->entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY)
        ? LINUX_VFS64_DIRENT_TYPE_DIR
        : LINUX_VFS64_DIRENT_TYPE_REG;
    for (index = 0u; index < source->name_byte_count; ++index)
    {
        target->name[index] = source->name[index];
    }
    for (; index < LINUX_VFS64_DIRENT_NAME_MAX; ++index)
    {
        target->name[index] = 0u;
    }
    return 1u;
}

static u32 linux_vfs64_read_nvme_dirent(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u32 cursor,
    linux_vfs64_dirent_t *entry_out)
{
    const u8 *fat_path;
    u32 fat_path_bytes;
    linux_vfs64_nvme_binding_t *binding;
    mmio64_nvme_fat_dirent_t mmio_entry;
    u32 result;

    binding = linux_vfs64_find_nvme_binding(pid);
    if ((binding == 0)
        || (entry_out == 0)
        || (binding->capability != mmio64_nvme_rw_capability())
        || (linux_vfs64_nvme_path_to_fat_rootable(
                path,
                path_byte_count,
                &fat_path,
                &fat_path_bytes) == 0u))
    {
        ++g_linux_vfs64_nvme_denial_count;
        return LINUX_VFS64_READDIR_NOT_DIRECTORY;
    }

    result = mmio64_nvme_fat_shell_read_dirent(
        fat_path,
        fat_path_bytes,
        cursor,
        binding->owner_id,
        &mmio_entry);
    if (result == MMIO64_NVME_FAT_READDIR_EOF)
    {
        ++g_linux_vfs64_nvme_readdir_count;
        return LINUX_VFS64_READDIR_EOF;
    }
    if ((result != MMIO64_NVME_FAT_READDIR_OK)
        || (linux_vfs64_copy_mmio_dirent(&mmio_entry, entry_out) == 0u))
    {
        ++g_linux_vfs64_nvme_denial_count;
        return LINUX_VFS64_READDIR_NOT_DIRECTORY;
    }

    ++g_linux_vfs64_nvme_readdir_count;
    ++g_linux_vfs64_nvme_dirent_count;
    return LINUX_VFS64_READDIR_OK;
}

static u32 linux_vfs64_bytes_equal(const u8 *left, const u8 *right, u32 byte_count)
{
    u32 index;

    if ((left == 0) || (right == 0))
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        if (left[index] != right[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 linux_vfs64_path_token(const u8 *path, u32 path_byte_count)
{
    u32 hash = 2166136261u;
    u32 index;

    if (path == 0)
    {
        return 0u;
    }

    for (index = 0u; index < path_byte_count; ++index)
    {
        hash ^= (u32)path[index];
        hash *= 16777619u;
    }

    return hash;
}

static u32 linux_vfs64_prefix_matches(
    const u8 *path,
    u32 path_byte_count,
    const linux_vfs64_mount_t *mount)
{
    if ((path == 0)
        || (mount == 0)
        || (mount->prefix == 0)
        || (mount->prefix_byte_count == 0u)
        || (path_byte_count < mount->prefix_byte_count))
    {
        return 0u;
    }

    if (linux_vfs64_bytes_equal(path, mount->prefix, mount->prefix_byte_count) == 0u)
    {
        return 0u;
    }

    if (mount->prefix_byte_count == 1u)
    {
        return 1u;
    }

    if (path_byte_count == mount->prefix_byte_count)
    {
        return 1u;
    }

    return (path[mount->prefix_byte_count] == (u8)'/') ? 1u : 0u;
}

static u32 linux_vfs64_find_mount(const u8 *path, u32 path_byte_count)
{
    u32 index;
    u32 best_index = LINUX_VFS64_INVALID_RESULT;
    u32 best_prefix = 0u;

    for (index = 0u; index < LINUX_VFS64_MAX_MOUNTS; ++index)
    {
        if ((linux_vfs64_prefix_matches(path, path_byte_count, &g_linux_vfs64_mounts[index]) != 0u)
            && (g_linux_vfs64_mounts[index].prefix_byte_count > best_prefix))
        {
            best_prefix = g_linux_vfs64_mounts[index].prefix_byte_count;
            best_index = index;
        }
    }

    return best_index;
}

static u32 linux_vfs64_path_is_exact(const u8 *path, u32 path_byte_count, const char *literal)
{
    u32 index = 0u;

    if ((path == 0) || (literal == 0))
    {
        return 0u;
    }

    while (literal[index] != '\0')
    {
        if ((index >= path_byte_count) || (path[index] != (u8)literal[index]))
        {
            return 0u;
        }
        ++index;
    }

    return (index == path_byte_count) ? 1u : 0u;
}

static u32 linux_vfs64_parse_proc_fd_path(const u8 *path, u32 path_byte_count, u32 *fd_number_out)
{
    const char prefix[] = "/proc/self/fd/";
    u32 index;
    u32 fd_number = 0u;

    if (fd_number_out != 0)
    {
        *fd_number_out = FD64_INVALID_FD;
    }

    if ((path == 0) || (fd_number_out == 0))
    {
        return 0u;
    }

    for (index = 0u; prefix[index] != '\0'; ++index)
    {
        if ((index >= path_byte_count) || (path[index] != (u8)prefix[index]))
        {
            return 0u;
        }
    }

    if (index >= path_byte_count)
    {
        return 0u;
    }

    for (; index < path_byte_count; ++index)
    {
        if ((path[index] < (u8)'0') || (path[index] > (u8)'9'))
        {
            return 0u;
        }
        fd_number = (fd_number * 10u) + (u32)(path[index] - (u8)'0');
        if (fd_number >= FD64_TABLE_LIMIT)
        {
            return 0u;
        }
    }

    *fd_number_out = fd_number;
    return 1u;
}

static void linux_vfs64_clear_fd_path_record(linux_vfs64_fd_path_record_t *record)
{
    u32 index;

    if (record == 0)
    {
        return;
    }

    record->active = 0u;
    record->pid = PROCESS64_INVALID_PID;
    record->fd_number = FD64_INVALID_FD;
    record->path_byte_count = 0u;
    record->dir_cursor = 0u;
    for (index = 0u; index < LINUX_VFS64_MAX_PATH_BYTES; ++index)
    {
        record->path[index] = 0u;
    }
}

static void linux_vfs64_clear_proc_record(linux_vfs64_proc_record_t *record)
{
    u32 index;

    if (record == 0)
    {
        return;
    }

    record->active = 0u;
    record->pid = PROCESS64_INVALID_PID;
    record->exe_path_bytes = 0u;
    record->cmdline_bytes = 0u;
    record->environ_bytes = 0u;
    for (index = 0u; index < LINUX_VFS64_PROC_IDENTITY_BYTES; ++index)
    {
        record->exe_path[index] = 0u;
    }
    for (index = 0u; index < LINUX_VFS64_PROC_PAYLOAD_BYTES; ++index)
    {
        record->cmdline[index] = 0u;
        record->environ[index] = 0u;
    }
}

static void linux_vfs64_clear_tmp_record(linux_vfs64_tmp_record_t *record)
{
    u32 index;

    if (record == 0)
    {
        return;
    }

    record->active = 0u;
    record->pid = PROCESS64_INVALID_PID;
    record->node_type = LINUX_VFS64_DIRENT_TYPE_UNKNOWN;
    record->name_byte_count = 0u;
    record->target_path_byte_count = 0u;
    for (index = 0u; index < LINUX_VFS64_DIRENT_NAME_MAX; ++index)
    {
        record->name[index] = 0u;
    }
    for (index = 0u; index < LINUX_VFS64_MAX_PATH_BYTES; ++index)
    {
        record->target_path[index] = 0u;
    }
}

static linux_vfs64_proc_record_t *linux_vfs64_find_proc_record(u32 pid)
{
    u32 index;

    for (index = 0u; index < LINUX_VFS64_MAX_PROC_RECORDS; ++index)
    {
        if ((g_linux_vfs64_proc_records[index].active != 0u)
            && (g_linux_vfs64_proc_records[index].pid == pid))
        {
            return &g_linux_vfs64_proc_records[index];
        }
    }

    return 0;
}

static linux_vfs64_proc_record_t *linux_vfs64_acquire_proc_record(u32 pid)
{
    u32 index;
    linux_vfs64_proc_record_t *record = linux_vfs64_find_proc_record(pid);

    if (record != 0)
    {
        return record;
    }

    for (index = 0u; index < LINUX_VFS64_MAX_PROC_RECORDS; ++index)
    {
        if (g_linux_vfs64_proc_records[index].active == 0u)
        {
            linux_vfs64_clear_proc_record(&g_linux_vfs64_proc_records[index]);
            g_linux_vfs64_proc_records[index].active = 1u;
            g_linux_vfs64_proc_records[index].pid = pid;
            return &g_linux_vfs64_proc_records[index];
        }
    }

    return 0;
}

static u32 linux_vfs64_record_fd_path(
    u32 pid,
    u32 fd_number,
    const u8 *path,
    u32 path_byte_count)
{
    u32 index;
    u32 byte_index;
    linux_vfs64_fd_path_record_t *free_record = 0;

    if ((path == 0)
        || (path_byte_count == 0u)
        || (path_byte_count > LINUX_VFS64_MAX_PATH_BYTES)
        || (fd_number >= FD64_TABLE_LIMIT))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_VFS64_MAX_FD_PATH_RECORDS; ++index)
    {
        if ((g_linux_vfs64_fd_paths[index].active != 0u)
            && (g_linux_vfs64_fd_paths[index].pid == pid)
            && (g_linux_vfs64_fd_paths[index].fd_number == fd_number))
        {
            free_record = &g_linux_vfs64_fd_paths[index];
            break;
        }
        if ((free_record == 0)
            && (g_linux_vfs64_fd_paths[index].active == 0u))
        {
            free_record = &g_linux_vfs64_fd_paths[index];
        }
    }

    if (free_record == 0)
    {
        return 0u;
    }

    free_record->active = 1u;
    free_record->pid = pid;
    free_record->fd_number = fd_number;
    free_record->path_byte_count = path_byte_count;
    free_record->dir_cursor = 0u;
    for (byte_index = 0u; byte_index < path_byte_count; ++byte_index)
    {
        free_record->path[byte_index] = path[byte_index];
    }
    for (; byte_index < LINUX_VFS64_MAX_PATH_BYTES; ++byte_index)
    {
        free_record->path[byte_index] = 0u;
    }

    return 1u;
}

static u32 linux_vfs64_copy_bounded(u8 *target, u32 target_bytes, const u8 *source, u32 source_bytes)
{
    u32 index;

    if ((target == 0) || (source == 0) || (source_bytes > target_bytes))
    {
        return 0u;
    }

    for (index = 0u; index < source_bytes; ++index)
    {
        target[index] = source[index];
    }
    for (; index < target_bytes; ++index)
    {
        target[index] = 0u;
    }

    return 1u;
}

static u32 linux_vfs64_append_byte(u8 *buffer, u32 capacity, u32 *length, u8 value)
{
    if ((buffer == 0) || (length == 0) || (*length >= capacity))
    {
        return 0u;
    }

    buffer[*length] = value;
    ++(*length);
    return 1u;
}

static u32 linux_vfs64_append_bytes(
    u8 *buffer,
    u32 capacity,
    u32 *length,
    const u8 *bytes,
    u32 byte_count)
{
    u32 index;

    if ((bytes == 0) && (byte_count != 0u))
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        if (linux_vfs64_append_byte(buffer, capacity, length, bytes[index]) == 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 linux_vfs64_append_cstr(u8 *buffer, u32 capacity, u32 *length, const char *text)
{
    u32 index = 0u;

    if (text == 0)
    {
        return 0u;
    }

    while (text[index] != '\0')
    {
        if (linux_vfs64_append_byte(buffer, capacity, length, (u8)text[index]) == 0u)
        {
            return 0u;
        }
        ++index;
    }

    return 1u;
}

static u32 linux_vfs64_append_dec(u8 *buffer, u32 capacity, u32 *length, u64 value)
{
    u8 digits[20];
    u32 count = 0u;

    if (value == 0ull)
    {
        return linux_vfs64_append_byte(buffer, capacity, length, (u8)'0');
    }

    while ((value != 0ull) && (count < (u32)sizeof(digits)))
    {
        digits[count] = (u8)('0' + (u8)(value % 10ull));
        value /= 10ull;
        ++count;
    }

    while (count != 0u)
    {
        --count;
        if (linux_vfs64_append_byte(buffer, capacity, length, digits[count]) == 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 linux_vfs64_append_hex64(u8 *buffer, u32 capacity, u32 *length, u64 value)
{
    s32 shift;
    u8 nybble;
    u8 digit;

    for (shift = 60; shift >= 0; shift -= 4)
    {
        nybble = (u8)((value >> (u32)shift) & 0x0Full);
        digit = (nybble < 10u)
            ? (u8)((u8)'0' + nybble)
            : (u8)((u8)'A' + (nybble - 10u));
        if (linux_vfs64_append_byte(
                buffer,
                capacity,
                length,
                digit) == 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 linux_vfs64_tmp_child_name(
    const u8 *path,
    u32 path_byte_count,
    const u8 **name_out,
    u32 *name_byte_count_out)
{
    u32 index;
    u32 name_bytes;

    if (name_out != 0)
    {
        *name_out = 0;
    }
    if (name_byte_count_out != 0)
    {
        *name_byte_count_out = 0u;
    }

    if ((path == 0)
        || (path_byte_count <= 5u)
        || (path[0] != (u8)'/')
        || (path[1] != (u8)'t')
        || (path[2] != (u8)'m')
        || (path[3] != (u8)'p')
        || (path[4] != (u8)'/'))
    {
        return 0u;
    }

    name_bytes = path_byte_count - 5u;
    if ((name_bytes == 0u) || (name_bytes > LINUX_VFS64_DIRENT_NAME_MAX))
    {
        return 0u;
    }

    for (index = 5u; index < path_byte_count; ++index)
    {
        if (path[index] == (u8)'/')
        {
            return 0u;
        }
    }

    if (((name_bytes == 1u) && (path[5u] == (u8)'.'))
        || ((name_bytes == 2u) && (path[5u] == (u8)'.') && (path[6u] == (u8)'.')))
    {
        return 0u;
    }

    if (name_out != 0)
    {
        *name_out = &path[5u];
    }
    if (name_byte_count_out != 0)
    {
        *name_byte_count_out = name_bytes;
    }
    return 1u;
}

static linux_vfs64_tmp_record_t *linux_vfs64_find_tmp_record(
    u32 pid,
    const u8 *name,
    u32 name_byte_count)
{
    u32 index;

    if ((name == 0) || (name_byte_count == 0u))
    {
        return 0;
    }

    for (index = 0u; index < LINUX_VFS64_MAX_TMP_RECORDS; ++index)
    {
        if ((g_linux_vfs64_tmp_records[index].active != 0u)
            && (g_linux_vfs64_tmp_records[index].pid == pid)
            && (g_linux_vfs64_tmp_records[index].name_byte_count == name_byte_count)
            && (linux_vfs64_bytes_equal(
                    g_linux_vfs64_tmp_records[index].name,
                    name,
                    name_byte_count) != 0u))
        {
            return &g_linux_vfs64_tmp_records[index];
        }
    }

    return 0;
}

static u32 linux_vfs64_record_tmp_file(u32 pid, const u8 *name, u32 name_byte_count)
{
    u32 index;
    u32 byte_index;
    linux_vfs64_tmp_record_t *record;

    if ((name == 0) || (name_byte_count == 0u) || (name_byte_count > LINUX_VFS64_DIRENT_NAME_MAX))
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    record = linux_vfs64_find_tmp_record(pid, name, name_byte_count);
    if ((record != 0) && (record->node_type == LINUX_VFS64_DIRENT_TYPE_LNK))
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }
    if (record == 0)
    {
        for (index = 0u; index < LINUX_VFS64_MAX_TMP_RECORDS; ++index)
        {
            if (g_linux_vfs64_tmp_records[index].active == 0u)
            {
                record = &g_linux_vfs64_tmp_records[index];
                break;
            }
        }
    }

    if (record == 0)
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    linux_vfs64_clear_tmp_record(record);
    record->active = 1u;
    record->pid = pid;
    record->node_type = LINUX_VFS64_DIRENT_TYPE_REG;
    record->name_byte_count = name_byte_count;
    for (byte_index = 0u; byte_index < name_byte_count; ++byte_index)
    {
        record->name[byte_index] = name[byte_index];
    }
    return 1u;
}

static u32 linux_vfs64_forget_tmp_file(u32 pid, const u8 *name, u32 name_byte_count)
{
    linux_vfs64_tmp_record_t *record = linux_vfs64_find_tmp_record(pid, name, name_byte_count);

    if (record == 0)
    {
        return 0u;
    }

    linux_vfs64_clear_tmp_record(record);
    return 1u;
}

static u32 linux_vfs64_record_tmp_symlink(
    u32 pid,
    const u8 *name,
    u32 name_byte_count,
    const u8 *target_path,
    u32 target_path_byte_count)
{
    u32 index;
    u32 byte_index;
    linux_vfs64_tmp_record_t *record;

    if ((name == 0)
        || (target_path == 0)
        || (name_byte_count == 0u)
        || (name_byte_count > LINUX_VFS64_DIRENT_NAME_MAX)
        || (target_path_byte_count == 0u)
        || (target_path_byte_count > LINUX_VFS64_MAX_PATH_BYTES)
        || (target_path[0] != (u8)'/'))
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    record = linux_vfs64_find_tmp_record(pid, name, name_byte_count);
    if (record != 0)
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    for (index = 0u; index < LINUX_VFS64_MAX_TMP_RECORDS; ++index)
    {
        if (g_linux_vfs64_tmp_records[index].active == 0u)
        {
            record = &g_linux_vfs64_tmp_records[index];
            break;
        }
    }

    if (record == 0)
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    linux_vfs64_clear_tmp_record(record);
    record->active = 1u;
    record->pid = pid;
    record->node_type = LINUX_VFS64_DIRENT_TYPE_LNK;
    record->name_byte_count = name_byte_count;
    record->target_path_byte_count = target_path_byte_count;
    for (byte_index = 0u; byte_index < name_byte_count; ++byte_index)
    {
        record->name[byte_index] = name[byte_index];
    }
    for (byte_index = 0u; byte_index < target_path_byte_count; ++byte_index)
    {
        record->target_path[byte_index] = target_path[byte_index];
    }
    g_linux_vfs64_symlink_last_target_bytes = target_path_byte_count;
    return 1u;
}

static linux_vfs64_tmp_record_t *linux_vfs64_find_tmp_symlink_path(
    u32 pid,
    const u8 *path,
    u32 path_byte_count)
{
    const u8 *name;
    u32 name_bytes;
    linux_vfs64_tmp_record_t *record;

    if (linux_vfs64_tmp_child_name(path, path_byte_count, &name, &name_bytes) == 0u)
    {
        return 0;
    }

    record = linux_vfs64_find_tmp_record(pid, name, name_bytes);
    if ((record == 0) || (record->node_type != LINUX_VFS64_DIRENT_TYPE_LNK))
    {
        return 0;
    }

    return record;
}

static u32 linux_vfs64_copy_path_bytes(
    u8 *target,
    u32 target_capacity,
    const u8 *source,
    u32 source_bytes,
    u32 *target_bytes)
{
    u32 index;

    if (target_bytes != 0)
    {
        *target_bytes = 0u;
    }

    if ((target == 0)
        || (target_bytes == 0)
        || (source == 0)
        || (source_bytes == 0u)
        || (source_bytes > target_capacity))
    {
        return 0u;
    }

    for (index = 0u; index < source_bytes; ++index)
    {
        target[index] = source[index];
    }
    for (; index < target_capacity; ++index)
    {
        target[index] = 0u;
    }

    *target_bytes = source_bytes;
    return 1u;
}

static u32 linux_vfs64_resolve_tmp_symlink_walk(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u32 flags,
    u8 *resolved_path,
    u32 resolved_capacity,
    u32 *resolved_path_byte_count,
    u32 *followed_out)
{
    u8 current_path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 current_path_bytes;
    u32 depth;

    if (followed_out != 0)
    {
        *followed_out = 0u;
    }

    if ((resolved_path == 0)
        || (resolved_path_byte_count == 0)
        || (followed_out == 0)
        || (linux_vfs64_copy_path_bytes(
                current_path,
                (u32)sizeof(current_path),
                path,
                path_byte_count,
                &current_path_bytes) == 0u))
    {
        ++g_linux_vfs64_denial_count;
        return 0u;
    }

    for (depth = 0u; depth < LINUX_VFS64_SYMLINK_MAX_DEPTH; ++depth)
    {
        linux_vfs64_tmp_record_t *record = linux_vfs64_find_tmp_symlink_path(
            pid,
            current_path,
            current_path_bytes);
        if (record == 0)
        {
            return linux_vfs64_copy_path_bytes(
                resolved_path,
                resolved_capacity,
                current_path,
                current_path_bytes,
                resolved_path_byte_count);
        }

        if ((depth == 0u) && ((flags & LINUX_VFS64_OPEN_NOFOLLOW) != 0u))
        {
            ++g_linux_vfs64_denial_count;
            ++g_linux_vfs64_tmp_denial_count;
            ++g_linux_vfs64_symlink_nofollow_denial_count;
            return 0u;
        }

        if (linux_vfs64_copy_path_bytes(
                current_path,
                (u32)sizeof(current_path),
                record->target_path,
                record->target_path_byte_count,
                &current_path_bytes) == 0u)
        {
            ++g_linux_vfs64_denial_count;
            ++g_linux_vfs64_tmp_denial_count;
            return 0u;
        }

        *followed_out = 1u;
        ++g_linux_vfs64_symlink_follow_count;
        g_linux_vfs64_symlink_last_target_bytes = record->target_path_byte_count;
    }

    ++g_linux_vfs64_denial_count;
    ++g_linux_vfs64_tmp_denial_count;
    ++g_linux_vfs64_symlink_nofollow_denial_count;
    return 0u;
}

static u32 linux_vfs64_tmp_root_backend_path(
    u32 pid,
    u8 *backend_path,
    u32 backend_capacity,
    u32 *backend_path_bytes)
{
    u32 length = 0u;

    if (backend_path_bytes != 0)
    {
        *backend_path_bytes = 0u;
    }

    if ((backend_path == 0) || (backend_path_bytes == 0) || (backend_capacity == 0u))
    {
        return 0u;
    }

    if ((linux_vfs64_append_cstr(backend_path, backend_capacity, &length, "/tmp/p") == 0u)
        || (linux_vfs64_append_dec(backend_path, backend_capacity, &length, (u64)pid) == 0u))
    {
        return 0u;
    }

    *backend_path_bytes = length;
    return 1u;
}

static u32 linux_vfs64_tmp_backend_path(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u8 *backend_path,
    u32 backend_capacity,
    u32 *backend_path_bytes)
{
    const u8 *name;
    u32 name_bytes;
    u32 index;
    u32 length = 0u;

    if (backend_path_bytes != 0)
    {
        *backend_path_bytes = 0u;
    }

    if ((backend_path == 0)
        || (backend_path_bytes == 0)
        || (linux_vfs64_tmp_child_name(path, path_byte_count, &name, &name_bytes) == 0u)
        || (linux_vfs64_tmp_root_backend_path(pid, backend_path, backend_capacity, &length) == 0u)
        || (linux_vfs64_append_byte(backend_path, backend_capacity, &length, (u8)'/') == 0u))
    {
        return 0u;
    }

    for (index = 0u; index < name_bytes; ++index)
    {
        if (linux_vfs64_append_byte(backend_path, backend_capacity, &length, name[index]) == 0u)
        {
            return 0u;
        }
    }

    *backend_path_bytes = length;
    g_linux_vfs64_tmp_last_backend_path_bytes = length;
    g_linux_vfs64_tmp_last_namespace_pid = pid;
    return 1u;
}

static u32 linux_vfs64_tmp_base_capability(u32 pid, u32 *owner_out, u32 *base_capability_out)
{
    u32 owner_id;
    u32 base_capability;

    if (owner_out != 0)
    {
        *owner_out = 0u;
    }
    if (base_capability_out != 0)
    {
        *base_capability_out = CAPABILITY64_INVALID_HANDLE;
    }

    if ((owner_out == 0)
        || (base_capability_out == 0)
        || (linux_vfs64_process_is_valid(pid) == 0u))
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    owner_id = process64_principal(pid);
    base_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_RAMFS,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        owner_id);
    if (base_capability == CAPABILITY64_INVALID_HANDLE)
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    *owner_out = owner_id;
    *base_capability_out = base_capability;
    return 1u;
}

static u32 linux_vfs64_tmp_ensure_namespace(u32 pid)
{
    u8 backend_path[LINUX_VFS64_TMP_BACKEND_BYTES];
    u32 backend_path_bytes;
    u32 owner_id;
    u32 base_capability;
    u32 tmp_capability;
    u32 pid_capability;
    u32 ok;

    if (linux_vfs64_tmp_base_capability(pid, &owner_id, &base_capability) == 0u)
    {
        return 0u;
    }

    tmp_capability = fs64_create_kernel(
        base_capability,
        (const u8 *)"/tmp",
        4u,
        RAMFS_NODE_DIRECTORY,
        owner_id);
    ok = (tmp_capability != FS64_INVALID_HANDLE) ? 1u : 0u;
    if (tmp_capability != FS64_INVALID_HANDLE)
    {
        (void)fs64_revoke(tmp_capability, owner_id);
    }

    if ((ok != 0u)
        && (linux_vfs64_tmp_root_backend_path(
                pid,
                backend_path,
                (u32)sizeof(backend_path),
                &backend_path_bytes) != 0u))
    {
        pid_capability = fs64_create_kernel(
            base_capability,
            backend_path,
            backend_path_bytes,
            RAMFS_NODE_DIRECTORY,
            owner_id);
        ok = (pid_capability != FS64_INVALID_HANDLE) ? 1u : 0u;
        if (pid_capability != FS64_INVALID_HANDLE)
        {
            (void)fs64_revoke(pid_capability, owner_id);
        }
    }
    else
    {
        ok = 0u;
    }

    (void)capability64_revoke(base_capability, owner_id);
    if (ok == 0u)
    {
        ++g_linux_vfs64_tmp_denial_count;
    }
    return ok;
}

static u32 linux_vfs64_tmp_create_file(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u32 mode)
{
    u8 backend_path[LINUX_VFS64_TMP_BACKEND_BYTES];
    const u8 *name;
    u32 name_bytes;
    u32 backend_path_bytes;
    u32 owner_id;
    u32 base_capability;
    u32 node_capability;

    (void)mode;

    if ((linux_vfs64_tmp_child_name(path, path_byte_count, &name, &name_bytes) == 0u)
        || (linux_vfs64_tmp_ensure_namespace(pid) == 0u)
        || (linux_vfs64_tmp_backend_path(
                pid,
                path,
                path_byte_count,
                backend_path,
                (u32)sizeof(backend_path),
                &backend_path_bytes) == 0u)
        || (linux_vfs64_tmp_base_capability(pid, &owner_id, &base_capability) == 0u))
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    node_capability = fs64_create_kernel(
        base_capability,
        backend_path,
        backend_path_bytes,
        RAMFS_NODE_FILE,
        owner_id);
    (void)capability64_revoke(base_capability, owner_id);
    if (node_capability == FS64_INVALID_HANDLE)
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    (void)fs64_revoke(node_capability, owner_id);
    if (linux_vfs64_record_tmp_file(pid, name, name_bytes) == 0u)
    {
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    ++g_linux_vfs64_tmp_create_count;
    return 1u;
}

static u32 linux_vfs64_fill_tmp_dirent(
    linux_vfs64_dirent_t *entry,
    const linux_vfs64_tmp_record_t *record,
    u32 record_index)
{
    u32 index;

    if ((entry == 0) || (record == 0) || (record->active == 0u))
    {
        return 0u;
    }

    entry->inode = 0x4000ull + (u64)record_index;
    entry->next_offset = record_index + 1u;
    entry->name_byte_count = record->name_byte_count;
    entry->entry_type = (u8)((record->node_type == LINUX_VFS64_DIRENT_TYPE_LNK)
        ? LINUX_VFS64_DIRENT_TYPE_LNK
        : LINUX_VFS64_DIRENT_TYPE_REG);
    for (index = 0u; index < record->name_byte_count; ++index)
    {
        entry->name[index] = record->name[index];
    }
    for (; index < LINUX_VFS64_DIRENT_NAME_MAX; ++index)
    {
        entry->name[index] = 0u;
    }

    return 1u;
}

static u32 linux_vfs64_read_tmp_dirent(u32 pid, u32 cursor, linux_vfs64_dirent_t *entry_out)
{
    u32 index;

    if (cursor == 0u)
    {
        g_linux_vfs64_tmp_last_dir_entries = 0u;
    }

    for (index = cursor; index < LINUX_VFS64_MAX_TMP_RECORDS; ++index)
    {
        if ((g_linux_vfs64_tmp_records[index].active != 0u)
            && (g_linux_vfs64_tmp_records[index].pid == pid))
        {
            ++g_linux_vfs64_tmp_last_dir_entries;
            return (linux_vfs64_fill_tmp_dirent(entry_out, &g_linux_vfs64_tmp_records[index], index) != 0u)
                ? LINUX_VFS64_READDIR_OK
                : LINUX_VFS64_READDIR_NOT_DIRECTORY;
        }
    }

    return LINUX_VFS64_READDIR_EOF;
}

static u32 linux_vfs64_copy_proc_payload(
    const u8 *payload,
    u32 payload_bytes,
    u64 file_offset,
    u8 *output,
    u32 byte_count)
{
    u32 index;
    u32 available;
    u32 copied;

    if ((output == 0) || ((payload == 0) && (payload_bytes != 0u)))
    {
        return LINUX_VFS64_INVALID_RESULT;
    }

    if (file_offset >= (u64)payload_bytes)
    {
        ++g_linux_vfs64_read_count;
        ++g_linux_vfs64_proc_read_count;
        return 0u;
    }

    available = payload_bytes - (u32)file_offset;
    copied = (available < byte_count) ? available : byte_count;
    for (index = 0u; index < copied; ++index)
    {
        output[index] = payload[(u32)file_offset + index];
    }

    ++g_linux_vfs64_read_count;
    ++g_linux_vfs64_proc_read_count;
    return copied;
}

static u32 linux_vfs64_proc_handle(u32 device_type, u32 fd_number)
{
    if (device_type != LINUX_VFS64_DEVICE_PROC_FD_LINK)
    {
        return linux_vfs64_device_handle(device_type);
    }

    if ((fd_number >= FD64_TABLE_LIMIT) || (fd_number > LINUX_VFS64_PROC_FD_MASK))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }

    return LINUX_VFS64_DEVICE_HANDLE_TAG
        | LINUX_VFS64_DEVICE_PROC_FD_LINK
        | ((fd_number & LINUX_VFS64_PROC_FD_MASK) << LINUX_VFS64_PROC_FD_SHIFT);
}

static u32 linux_vfs64_proc_fd_from_handle(u32 handle)
{
    return (handle >> LINUX_VFS64_PROC_FD_SHIFT) & LINUX_VFS64_PROC_FD_MASK;
}

static u32 linux_vfs64_string_length(const char *text)
{
    u32 length = 0u;

    if (text == 0)
    {
        return 0u;
    }

    while ((text[length] != '\0') && (length < LINUX_VFS64_DIRENT_NAME_MAX))
    {
        ++length;
    }

    return length;
}

static u32 linux_vfs64_fill_dirent(
    linux_vfs64_dirent_t *entry,
    const linux_vfs64_dirent_template_t *source,
    u32 next_offset)
{
    u32 index;
    u32 name_length;

    if ((entry == 0) || (source == 0) || (source->name == 0))
    {
        return 0u;
    }

    name_length = linux_vfs64_string_length(source->name);
    if (name_length == 0u)
    {
        return 0u;
    }

    entry->inode = source->inode;
    entry->next_offset = next_offset;
    entry->name_byte_count = name_length;
    entry->entry_type = source->entry_type;
    for (index = 0u; index < name_length; ++index)
    {
        entry->name[index] = (u8)source->name[index];
    }
    for (; index < LINUX_VFS64_DIRENT_NAME_MAX; ++index)
    {
        entry->name[index] = 0u;
    }

    return 1u;
}

static u32 linux_vfs64_dirent_from_table(
    const linux_vfs64_dirent_template_t *table,
    u32 table_count,
    u32 cursor,
    linux_vfs64_dirent_t *entry_out)
{
    if (cursor >= table_count)
    {
        return LINUX_VFS64_READDIR_EOF;
    }

    return (linux_vfs64_fill_dirent(entry_out, &table[cursor], cursor + 1u) != 0u)
        ? LINUX_VFS64_READDIR_OK
        : LINUX_VFS64_READDIR_NOT_DIRECTORY;
}

static u32 linux_vfs64_fill_fd_dirent(
    linux_vfs64_dirent_t *entry,
    u32 fd_number,
    u32 next_offset)
{
    u8 digits[10];
    u32 count = 0u;
    u32 value = fd_number;
    u32 index;

    if (entry == 0)
    {
        return 0u;
    }

    if (value == 0u)
    {
        digits[count] = (u8)'0';
        ++count;
    }
    else
    {
        while ((value != 0u) && (count < (u32)sizeof(digits)))
        {
            digits[count] = (u8)('0' + (u8)(value % 10u));
            value /= 10u;
            ++count;
        }
    }

    entry->inode = 0x3200ull + (u64)fd_number;
    entry->next_offset = next_offset;
    entry->name_byte_count = count;
    entry->entry_type = LINUX_VFS64_DIRENT_TYPE_LNK;
    for (index = 0u; index < count; ++index)
    {
        entry->name[index] = digits[count - index - 1u];
    }
    for (; index < LINUX_VFS64_DIRENT_NAME_MAX; ++index)
    {
        entry->name[index] = 0u;
    }

    return 1u;
}

static u32 linux_vfs64_read_proc_fd_dirent(
    u32 pid,
    u32 cursor,
    u32 iterator_fd,
    linux_vfs64_dirent_t *entry_out)
{
    u32 fd_number;

    for (fd_number = cursor; fd_number <= LINUX_VFS64_PROC_FD_MASK; ++fd_number)
    {
        if (fd_number == iterator_fd)
        {
            continue;
        }
        if (fd64_entry_type(pid, fd_number) != FD64_TYPE_EMPTY)
        {
            ++g_linux_vfs64_proc_last_fd_entries;
            return (linux_vfs64_fill_fd_dirent(entry_out, fd_number, fd_number + 1u) != 0u)
                ? LINUX_VFS64_READDIR_OK
                : LINUX_VFS64_READDIR_NOT_DIRECTORY;
        }
    }

    return LINUX_VFS64_READDIR_EOF;
}

static u32 linux_vfs64_result_is_directory(
    const linux_vfs64_result_t *result,
    const u8 *path,
    u32 path_byte_count)
{
    const linux_vfs64_mount_t *mount;

    if ((result == 0) || (path == 0))
    {
        return 0u;
    }

    if ((result->node_type == LINUX_VFS64_NODE_PROC_DIR)
        || (result->node_type == LINUX_VFS64_NODE_DEV_DIR)
        || (result->node_type == LINUX_VFS64_NODE_NVME_DIR))
    {
        return 1u;
    }

    if ((result->provider != LINUX_VFS64_PROVIDER_RAMFS)
        || (result->mount_index >= LINUX_VFS64_MAX_MOUNTS))
    {
        return 0u;
    }

    mount = &g_linux_vfs64_mounts[result->mount_index];
    return ((mount->prefix_byte_count == path_byte_count)
        && (linux_vfs64_bytes_equal(path, mount->prefix, path_byte_count) != 0u))
        ? 1u
        : 0u;
}

static u32 linux_vfs64_append_proc_prot(u8 *buffer, u32 capacity, u32 *length, u32 prot, u32 map_flags)
{
    return (linux_vfs64_append_byte(buffer, capacity, length, ((prot & VMA64_PROT_READ) != 0u) ? (u8)'r' : (u8)'-') != 0u)
        && (linux_vfs64_append_byte(buffer, capacity, length, ((prot & VMA64_PROT_WRITE) != 0u) ? (u8)'w' : (u8)'-') != 0u)
        && (linux_vfs64_append_byte(buffer, capacity, length, ((prot & VMA64_PROT_EXECUTE) != 0u) ? (u8)'x' : (u8)'-') != 0u)
        && (linux_vfs64_append_byte(buffer, capacity, length, ((map_flags & VMA64_MAP_SHARED) != 0u) ? (u8)'s' : (u8)'p') != 0u)
            ? 1u
            : 0u;
}

static u32 linux_vfs64_build_proc_maps(u32 pid, u8 *buffer, u32 capacity, u32 *length_out)
{
    const vma_region_t *region;
    u32 length = 0u;

    if (length_out != 0)
    {
        *length_out = 0u;
    }

    if ((buffer == 0) || (length_out == 0))
    {
        return 0u;
    }

    g_linux_vfs64_proc_last_maps_regions = vma64_region_count(pid);
    g_linux_vfs64_proc_last_maps_bytes = vma64_mapped_bytes(pid);
    region = vma64_first_region(pid);
    while (region != 0)
    {
        if ((linux_vfs64_append_hex64(buffer, capacity, &length, region->virt_base) == 0u)
            || (linux_vfs64_append_byte(buffer, capacity, &length, (u8)'-') == 0u)
            || (linux_vfs64_append_hex64(buffer, capacity, &length, region->virt_end) == 0u)
            || (linux_vfs64_append_byte(buffer, capacity, &length, (u8)' ') == 0u)
            || (linux_vfs64_append_proc_prot(buffer, capacity, &length, region->prot_flags, region->map_flags) == 0u)
            || (linux_vfs64_append_cstr(buffer, capacity, &length, " anon token ") == 0u)
            || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)region->vma_token) == 0u)
            || (linux_vfs64_append_byte(buffer, capacity, &length, (u8)'\n') == 0u))
        {
            break;
        }
        region = vma64_next_region(region);
    }

    *length_out = length;
    return 1u;
}

static u32 linux_vfs64_build_proc_status(u32 pid, u8 *buffer, u32 capacity, u32 *length_out)
{
    u32 length = 0u;

    if (length_out != 0)
    {
        *length_out = 0u;
    }

    if ((buffer == 0) || (length_out == 0))
    {
        return 0u;
    }

    if ((linux_vfs64_append_cstr(buffer, capacity, &length, "Name:\tlimitless\nPid:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)pid) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, "\nPPid:\t0\nVmSize:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, vma64_mapped_bytes(pid)) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, "\nFDSize:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)fd64_live_count(pid)) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, "\nThreads:\t1\n") == 0u))
    {
        return 0u;
    }

    *length_out = length;
    return 1u;
}

static u32 linux_vfs64_build_proc_meminfo(u8 *buffer, u32 capacity, u32 *length_out)
{
    u32 length = 0u;
    u32 total_kib;
    u32 free_kib;
    u32 claimed_kib;

    if (length_out != 0)
    {
        *length_out = 0u;
    }

    if ((buffer == 0) || (length_out == 0))
    {
        return 0u;
    }

    total_kib = (vma64_anon_total_pages() * VMA64_PAGE_BYTES) / 1024u;
    free_kib = (vma64_anon_free_pages() * VMA64_PAGE_BYTES) / 1024u;
    claimed_kib = (vma64_anon_claimed_pages() * VMA64_PAGE_BYTES) / 1024u;
    g_linux_vfs64_proc_last_mem_total_kib = total_kib;
    g_linux_vfs64_proc_last_mem_free_kib = free_kib;
    g_linux_vfs64_proc_last_mem_available_kib = free_kib;
    g_linux_vfs64_proc_last_mem_claimed_kib = claimed_kib;

    if ((linux_vfs64_append_cstr(buffer, capacity, &length, "MemTotal:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)total_kib) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, " kB\nMemFree:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)free_kib) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, " kB\nMemAvailable:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)free_kib) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, " kB\nAnonPages:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)claimed_kib) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, " kB\nPageTables:\t") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)VMA64_PAGE_BYTES / 1024ull) == 0u)
        || (linux_vfs64_append_cstr(buffer, capacity, &length, " kB\n") == 0u))
    {
        return 0u;
    }

    g_linux_vfs64_proc_last_meminfo_bytes = length;
    *length_out = length;
    return 1u;
}

static u32 linux_vfs64_build_proc_fd_link(
    u32 pid,
    u32 target_fd,
    u8 *buffer,
    u32 capacity,
    u32 *length_out)
{
    u32 length = 0u;

    if (length_out != 0)
    {
        *length_out = 0u;
    }

    if ((buffer == 0) || (length_out == 0) || (target_fd >= FD64_TABLE_LIMIT))
    {
        return 0u;
    }

    g_linux_vfs64_proc_last_fd_target = target_fd;
    if (linux_vfs64_fd_path(pid, target_fd, buffer, capacity, &length) != 0u)
    {
        *length_out = length;
        return 1u;
    }

    if ((linux_vfs64_append_cstr(buffer, capacity, &length, "anon:[fd ") == 0u)
        || (linux_vfs64_append_dec(buffer, capacity, &length, (u64)target_fd) == 0u)
        || (linux_vfs64_append_byte(buffer, capacity, &length, (u8)']') == 0u))
    {
        return 0u;
    }

    *length_out = length;
    return 1u;
}

static u32 linux_vfs64_read_proc_payload(
    u32 pid,
    u32 device_type,
    u32 handle,
    u8 *output,
    u32 byte_count,
    u64 file_offset)
{
    static u8 proc_scratch[LINUX_VFS64_PROC_SCRATCH_BYTES];
    linux_vfs64_proc_record_t *record = linux_vfs64_find_proc_record(pid);
    u32 payload_bytes = 0u;
    u32 target_fd;

    if (device_type == LINUX_VFS64_DEVICE_PROC_MAPS)
    {
        if (linux_vfs64_build_proc_maps(
                pid,
                proc_scratch,
                (u32)sizeof(proc_scratch),
                &payload_bytes) == 0u)
        {
            ++g_linux_vfs64_proc_denial_count;
            return LINUX_VFS64_INVALID_RESULT;
        }
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_EXE)
    {
        if ((record == 0) || (record->exe_path_bytes == 0u))
        {
            if (linux_vfs64_append_cstr(
                    proc_scratch,
                    (u32)sizeof(proc_scratch),
                    &payload_bytes,
                    "/proc/self/exe") == 0u)
            {
                ++g_linux_vfs64_proc_denial_count;
                return LINUX_VFS64_INVALID_RESULT;
            }
        }
        else if (linux_vfs64_append_bytes(
                proc_scratch,
                (u32)sizeof(proc_scratch),
                &payload_bytes,
                record->exe_path,
                record->exe_path_bytes) == 0u)
        {
            ++g_linux_vfs64_proc_denial_count;
            return LINUX_VFS64_INVALID_RESULT;
        }
        g_linux_vfs64_proc_last_exe_bytes = payload_bytes;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_STATUS)
    {
        if (linux_vfs64_build_proc_status(pid, proc_scratch, (u32)sizeof(proc_scratch), &payload_bytes) == 0u)
        {
            ++g_linux_vfs64_proc_denial_count;
            return LINUX_VFS64_INVALID_RESULT;
        }
        g_linux_vfs64_proc_last_status_bytes = payload_bytes;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_MEMINFO)
    {
        if (linux_vfs64_build_proc_meminfo(proc_scratch, (u32)sizeof(proc_scratch), &payload_bytes) == 0u)
        {
            ++g_linux_vfs64_proc_denial_count;
            return LINUX_VFS64_INVALID_RESULT;
        }
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_CMDLINE)
    {
        if ((record != 0) && (record->cmdline_bytes != 0u))
        {
            g_linux_vfs64_proc_last_cmdline_bytes = record->cmdline_bytes;
            return linux_vfs64_copy_proc_payload(
                record->cmdline,
                record->cmdline_bytes,
                file_offset,
                output,
                byte_count);
        }
        if ((record != 0) && (record->exe_path_bytes != 0u))
        {
            g_linux_vfs64_proc_last_cmdline_bytes = record->exe_path_bytes;
            return linux_vfs64_copy_proc_payload(
                record->exe_path,
                record->exe_path_bytes,
                file_offset,
                output,
                byte_count);
        }
        g_linux_vfs64_proc_last_cmdline_bytes = 0u;
        return 0u;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_ENVIRON)
    {
        if ((record != 0) && (record->environ_bytes != 0u))
        {
            g_linux_vfs64_proc_last_environ_bytes = record->environ_bytes;
            return linux_vfs64_copy_proc_payload(
                record->environ,
                record->environ_bytes,
                file_offset,
                output,
                byte_count);
        }
        g_linux_vfs64_proc_last_environ_bytes = 0u;
        return 0u;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_FD_LINK)
    {
        target_fd = linux_vfs64_proc_fd_from_handle(handle);
        if (linux_vfs64_build_proc_fd_link(
                pid,
                target_fd,
                proc_scratch,
                (u32)sizeof(proc_scratch),
                &payload_bytes) == 0u)
        {
            ++g_linux_vfs64_proc_denial_count;
            return LINUX_VFS64_INVALID_RESULT;
        }
    }
    else
    {
        ++g_linux_vfs64_proc_denial_count;
        return LINUX_VFS64_INVALID_RESULT;
    }

    return linux_vfs64_copy_proc_payload(proc_scratch, payload_bytes, file_offset, output, byte_count);
}

static u32 linux_vfs64_resolve_dev(
    const u8 *path,
    u32 path_byte_count,
    linux_vfs64_result_t *result)
{
    result->provider = LINUX_VFS64_PROVIDER_DEV;

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev") != 0u)
    {
        result->node_type = LINUX_VFS64_NODE_DEV_DIR;
        return 1u;
    }

    result->node_type = LINUX_VFS64_NODE_DEV_CHAR;
    if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev/null") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_NULL;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev/zero") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_ZERO;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev/urandom") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_URANDOM;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev/stdin") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_STDIN;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev/stdout") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_STDOUT;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev/stderr") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_STDERR;
    }
    else
    {
        return linux_vfs64_deny(result, LINUX_VFS64_ERROR_NOT_FOUND);
    }

    result->capability_handle = linux_vfs64_device_handle(result->device_type);
    return 1u;
}

static u32 linux_vfs64_resolve_proc(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    linux_vfs64_result_t *result)
{
    u32 fd_number;

    result->provider = LINUX_VFS64_PROVIDER_PROC;

    if ((linux_vfs64_path_is_exact(path, path_byte_count, "/proc") != 0u)
        || (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self") != 0u)
        || (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/fd") != 0u))
    {
        result->node_type = LINUX_VFS64_NODE_PROC_DIR;
        if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/fd") != 0u)
        {
            result->device_type = LINUX_VFS64_DEVICE_PROC_FD_DIR;
            result->capability_handle = linux_vfs64_device_handle(LINUX_VFS64_DEVICE_PROC_FD_DIR);
        }
        return 1u;
    }

    result->node_type = LINUX_VFS64_NODE_PROC_FILE;
    if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/maps") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_PROC_MAPS;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/status") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_PROC_STATUS;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/meminfo") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_PROC_MEMINFO;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/cmdline") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_PROC_CMDLINE;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/environ") != 0u)
    {
        result->device_type = LINUX_VFS64_DEVICE_PROC_ENVIRON;
    }
    else if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/exe") != 0u)
    {
        result->node_type = LINUX_VFS64_NODE_PROC_SYMLINK;
        result->device_type = LINUX_VFS64_DEVICE_PROC_EXE;
    }
    else if (linux_vfs64_parse_proc_fd_path(path, path_byte_count, &fd_number) != 0u)
    {
        result->node_type = LINUX_VFS64_NODE_PROC_SYMLINK;
        result->device_type = LINUX_VFS64_DEVICE_PROC_FD_LINK;
        result->capability_handle = linux_vfs64_proc_handle(result->device_type, fd_number);
        return ((result->capability_handle == CAPABILITY64_INVALID_HANDLE)
            || (fd64_entry_type(pid, fd_number) == FD64_TYPE_EMPTY))
            ? linux_vfs64_deny(result, LINUX_VFS64_ERROR_NOT_FOUND)
            : 1u;
    }
    else
    {
        return linux_vfs64_deny(result, LINUX_VFS64_ERROR_NOT_FOUND);
    }

    result->capability_handle = linux_vfs64_device_handle(result->device_type);
    return 1u;
}

static u32 linux_vfs64_resolve_ramfs(linux_vfs64_result_t *result)
{
    result->provider = LINUX_VFS64_PROVIDER_RAMFS;
    result->node_type = LINUX_VFS64_NODE_RAMFS_PATH;
    return 1u;
}

static u32 linux_vfs64_resolve_nvme(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    linux_vfs64_result_t *result)
{
    mmio64_nvme_fat_stat_t fat_stat;

    result->provider = LINUX_VFS64_PROVIDER_NVME;
    if (linux_vfs64_path_is_exact(path, path_byte_count, "/nvme") != 0u)
    {
        result->node_type = LINUX_VFS64_NODE_NVME_DIR;
        result->device_type = LINUX_VFS64_DEVICE_DIRECTORY;
        result->capability_handle = linux_vfs64_device_handle(LINUX_VFS64_DEVICE_DIRECTORY);
        return 1u;
    }

    if (linux_vfs64_nvme_stat(pid, path, path_byte_count, &fat_stat) == 0u)
    {
        return linux_vfs64_deny(result, LINUX_VFS64_ERROR_NOT_FOUND);
    }

    if (fat_stat.entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY)
    {
        result->node_type = LINUX_VFS64_NODE_NVME_DIR;
        result->device_type = LINUX_VFS64_DEVICE_DIRECTORY;
        result->capability_handle = linux_vfs64_device_handle(LINUX_VFS64_DEVICE_DIRECTORY);
        return 1u;
    }

    if (fat_stat.entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
    {
        return linux_vfs64_deny(result, LINUX_VFS64_ERROR_NOT_FOUND);
    }

    result->node_type = LINUX_VFS64_NODE_NVME_FILE;
    result->device_type = LINUX_VFS64_DEVICE_NVME_FILE;
    result->capability_handle = linux_vfs64_device_handle(LINUX_VFS64_DEVICE_NVME_FILE);
    return 1u;
}

static u32 linux_vfs64_fd_is_linux_device(u32 pid, u32 fd_number, u32 *device_type_out)
{
    u32 handle;
    u32 device_type;

    if (device_type_out != 0)
    {
        *device_type_out = LINUX_VFS64_DEVICE_UNKNOWN;
    }

    if (fd64_entry_type(pid, fd_number) != FD64_TYPE_DEVICE)
    {
        return 0u;
    }

    handle = fd64_entry_capability(pid, fd_number);
    device_type = linux_vfs64_device_type_from_handle(handle);
    if (device_type == LINUX_VFS64_DEVICE_UNKNOWN)
    {
        return 0u;
    }

    if (device_type_out != 0)
    {
        *device_type_out = device_type;
    }
    return 1u;
}

void linux_vfs64_init(void)
{
    u32 index;

    g_linux_vfs64_denial_count = 0u;
    g_linux_vfs64_open_count = 0u;
    g_linux_vfs64_read_count = 0u;
    g_linux_vfs64_write_count = 0u;
    g_linux_vfs64_proc_read_count = 0u;
    g_linux_vfs64_proc_denial_count = 0u;
    g_linux_vfs64_proc_last_maps_regions = 0u;
    g_linux_vfs64_proc_last_maps_bytes = 0ull;
    g_linux_vfs64_proc_last_exe_bytes = 0u;
    g_linux_vfs64_proc_last_status_bytes = 0u;
    g_linux_vfs64_proc_last_cmdline_bytes = 0u;
    g_linux_vfs64_proc_last_environ_bytes = 0u;
    g_linux_vfs64_proc_last_fd_entries = 0u;
    g_linux_vfs64_proc_last_fd_target = FD64_INVALID_FD;
    g_linux_vfs64_proc_last_meminfo_bytes = 0u;
    g_linux_vfs64_proc_last_mem_total_kib = 0u;
    g_linux_vfs64_proc_last_mem_free_kib = 0u;
    g_linux_vfs64_proc_last_mem_available_kib = 0u;
    g_linux_vfs64_proc_last_mem_claimed_kib = 0u;
    g_linux_vfs64_tmp_create_count = 0u;
    g_linux_vfs64_tmp_delete_count = 0u;
    g_linux_vfs64_tmp_denial_count = 0u;
    g_linux_vfs64_tmp_last_dir_entries = 0u;
    g_linux_vfs64_tmp_last_backend_path_bytes = 0u;
    g_linux_vfs64_tmp_last_namespace_pid = PROCESS64_INVALID_PID;
    g_linux_vfs64_symlink_create_count = 0u;
    g_linux_vfs64_symlink_follow_count = 0u;
    g_linux_vfs64_symlink_readlink_count = 0u;
    g_linux_vfs64_symlink_lstat_count = 0u;
    g_linux_vfs64_symlink_nofollow_denial_count = 0u;
    g_linux_vfs64_symlink_last_target_bytes = 0u;
    g_linux_vfs64_nvme_bind_count = 0u;
    g_linux_vfs64_nvme_release_count = 0u;
    g_linux_vfs64_nvme_read_count = 0u;
    g_linux_vfs64_nvme_readdir_count = 0u;
    g_linux_vfs64_nvme_dirent_count = 0u;
    g_linux_vfs64_nvme_denial_count = 0u;
    g_linux_vfs64_nvme_last_bytes = 0u;
    g_linux_vfs64_random_counter = 0x5A17A001u;
    for (index = 0u; index < LINUX_VFS64_MAX_FD_PATH_RECORDS; ++index)
    {
        linux_vfs64_clear_fd_path_record(&g_linux_vfs64_fd_paths[index]);
    }
    for (index = 0u; index < LINUX_VFS64_MAX_PROC_RECORDS; ++index)
    {
        linux_vfs64_clear_proc_record(&g_linux_vfs64_proc_records[index]);
    }
    for (index = 0u; index < LINUX_VFS64_MAX_TMP_RECORDS; ++index)
    {
        linux_vfs64_clear_tmp_record(&g_linux_vfs64_tmp_records[index]);
    }
    for (index = 0u; index < LINUX_VFS64_MAX_NVME_BINDINGS; ++index)
    {
        linux_vfs64_clear_nvme_binding(&g_linux_vfs64_nvme_bindings[index]);
    }
    g_linux_vfs64_initialized = 1u;
}

u32 linux_vfs64_mount_count(void)
{
    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    return LINUX_VFS64_MAX_MOUNTS;
}

const linux_vfs64_mount_t *linux_vfs64_mount_at(u32 index)
{
    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    return (index < LINUX_VFS64_MAX_MOUNTS) ? &g_linux_vfs64_mounts[index] : 0;
}

u32 linux_vfs64_bind_nvme_read(u32 pid, u32 owner_id, u32 nvme_capability)
{
    linux_vfs64_nvme_binding_t *binding;
    u32 index;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if ((linux_vfs64_process_is_valid(pid) == 0u)
        || (owner_id == 0u)
        || (nvme_capability == CAPABILITY64_INVALID_HANDLE)
        || (nvme_capability != mmio64_nvme_rw_capability()))
    {
        ++g_linux_vfs64_nvme_denial_count;
        return 0u;
    }

    binding = linux_vfs64_find_nvme_binding(pid);
    if (binding == 0)
    {
        for (index = 0u; index < LINUX_VFS64_MAX_NVME_BINDINGS; ++index)
        {
            if (g_linux_vfs64_nvme_bindings[index].active == 0u)
            {
                binding = &g_linux_vfs64_nvme_bindings[index];
                break;
            }
        }
    }
    if (binding == 0)
    {
        ++g_linux_vfs64_nvme_denial_count;
        return 0u;
    }

    binding->active = 1u;
    binding->pid = pid;
    binding->owner_id = owner_id;
    binding->capability = nvme_capability;
    ++g_linux_vfs64_nvme_bind_count;
    return 1u;
}

u32 linux_vfs64_release_nvme_read(u32 pid)
{
    linux_vfs64_nvme_binding_t *binding;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    binding = linux_vfs64_find_nvme_binding(pid);
    if (binding == 0)
    {
        ++g_linux_vfs64_nvme_denial_count;
        return 0u;
    }

    linux_vfs64_clear_nvme_binding(binding);
    ++g_linux_vfs64_nvme_release_count;
    return 1u;
}

u32 linux_vfs64_resolve(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u32 flags,
    linux_vfs64_result_t *result)
{
    u32 mount_index;
    const linux_vfs64_mount_t *mount;
    u32 resolved;

    (void)flags;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    linux_vfs64_zero_result(result);
    if (result == 0)
    {
        return linux_vfs64_deny(0, LINUX_VFS64_ERROR_ARGUMENT);
    }

    if (linux_vfs64_process_is_valid(pid) == 0u)
    {
        return linux_vfs64_deny(result, LINUX_VFS64_ERROR_NO_PROCESS);
    }

    if ((path == 0)
        || (path_byte_count == 0u)
        || (path_byte_count > LINUX_VFS64_MAX_PATH_BYTES)
        || (path[0] != (u8)'/'))
    {
        return linux_vfs64_deny(result, LINUX_VFS64_ERROR_PATH);
    }

    result->path_token = linux_vfs64_path_token(path, path_byte_count);
    mount_index = linux_vfs64_find_mount(path, path_byte_count);
    if (mount_index == LINUX_VFS64_INVALID_RESULT)
    {
        return linux_vfs64_deny(result, LINUX_VFS64_ERROR_NO_MOUNT);
    }

    result->mount_index = mount_index;
    mount = &g_linux_vfs64_mounts[mount_index];
    if (mount->provider == LINUX_VFS64_PROVIDER_DEV)
    {
        resolved = linux_vfs64_resolve_dev(path, path_byte_count, result);
    }
    else if (mount->provider == LINUX_VFS64_PROVIDER_PROC)
    {
        resolved = linux_vfs64_resolve_proc(pid, path, path_byte_count, result);
    }
    else if (mount->provider == LINUX_VFS64_PROVIDER_RAMFS)
    {
        resolved = linux_vfs64_resolve_ramfs(result);
    }
    else if (mount->provider == LINUX_VFS64_PROVIDER_NVME)
    {
        resolved = linux_vfs64_resolve_nvme(pid, path, path_byte_count, result);
    }
    else
    {
        resolved = linux_vfs64_deny(result, LINUX_VFS64_ERROR_UNSUPPORTED);
    }

    return resolved;
}

u32 linux_vfs64_open(u32 pid, const u8 *path, u32 path_byte_count, u32 flags, u32 mode)
{
    u8 resolved_path[LINUX_VFS64_MAX_PATH_BYTES];
    u8 backend_path[LINUX_VFS64_TMP_BACKEND_BYTES];
    const u8 *effective_path = path;
    u32 effective_path_bytes = path_byte_count;
    u32 symlink_followed = 0u;
    u32 backend_path_bytes;
    linux_vfs64_result_t result;
    u32 fd_number;
    u32 fd_flags = flags & LINUX_VFS64_OPEN_FD_FLAG_MASK;
    u32 create_requested = ((flags & LINUX_VFS64_OPEN_CREATE) != 0u) ? 1u : 0u;

    if ((flags & ~LINUX_VFS64_OPEN_SUPPORTED_FLAGS) != 0u)
    {
        ++g_linux_vfs64_denial_count;
        return FD64_INVALID_FD;
    }

    if (linux_vfs64_resolve_tmp_symlink_walk(
            pid,
            path,
            path_byte_count,
            flags,
            resolved_path,
            (u32)sizeof(resolved_path),
            &effective_path_bytes,
            &symlink_followed) == 0u)
    {
        return FD64_INVALID_FD;
    }
    if (symlink_followed != 0u)
    {
        effective_path = resolved_path;
    }

    if (linux_vfs64_resolve(pid, effective_path, effective_path_bytes, flags, &result) == 0u)
    {
        return FD64_INVALID_FD;
    }

    if (((flags & LINUX_VFS64_OPEN_NOFOLLOW) != 0u)
        && (result.node_type == LINUX_VFS64_NODE_PROC_SYMLINK))
    {
        ++g_linux_vfs64_denial_count;
        ++g_linux_vfs64_symlink_nofollow_denial_count;
        return FD64_INVALID_FD;
    }

    if ((result.provider == LINUX_VFS64_PROVIDER_DEV)
        && (result.node_type == LINUX_VFS64_NODE_DEV_CHAR)
        && (result.capability_handle != CAPABILITY64_INVALID_HANDLE))
    {
            fd_number = fd64_alloc(pid, result.capability_handle, FD64_TYPE_DEVICE, fd_flags);
        if (fd_number != FD64_INVALID_FD)
        {
            (void)linux_vfs64_record_fd_path(pid, fd_number, effective_path, effective_path_bytes);
            ++g_linux_vfs64_open_count;
        }
        return fd_number;
    }

    if (linux_vfs64_result_is_directory(&result, path, path_byte_count) != 0u)
    {
        fd_number = fd64_alloc(
            pid,
            linux_vfs64_device_handle(LINUX_VFS64_DEVICE_DIRECTORY),
            FD64_TYPE_DEVICE,
            fd_flags);
        if (fd_number != FD64_INVALID_FD)
        {
            (void)linux_vfs64_record_fd_path(pid, fd_number, effective_path, effective_path_bytes);
            ++g_linux_vfs64_open_count;
        }
        return fd_number;
    }

    if ((result.provider == LINUX_VFS64_PROVIDER_PROC)
        && ((result.node_type == LINUX_VFS64_NODE_PROC_FILE)
            || (result.node_type == LINUX_VFS64_NODE_PROC_SYMLINK))
        && (result.capability_handle != CAPABILITY64_INVALID_HANDLE))
    {
        fd_number = fd64_alloc(pid, result.capability_handle, FD64_TYPE_DEVICE, fd_flags);
        if (fd_number != FD64_INVALID_FD)
        {
            (void)linux_vfs64_record_fd_path(pid, fd_number, effective_path, effective_path_bytes);
            ++g_linux_vfs64_open_count;
        }
        return fd_number;
    }

    if ((result.provider == LINUX_VFS64_PROVIDER_NVME)
        && (result.node_type == LINUX_VFS64_NODE_NVME_FILE)
        && (result.capability_handle != CAPABILITY64_INVALID_HANDLE)
        && (create_requested == 0u))
    {
        fd_number = fd64_alloc(pid, result.capability_handle, FD64_TYPE_DEVICE, fd_flags);
        if (fd_number != FD64_INVALID_FD)
        {
            (void)linux_vfs64_record_fd_path(pid, fd_number, effective_path, effective_path_bytes);
            ++g_linux_vfs64_open_count;
        }
        return fd_number;
    }

    if (result.provider == LINUX_VFS64_PROVIDER_RAMFS)
    {
        const u8 *tmp_name = 0;
        u32 tmp_name_bytes = 0u;
        if (linux_vfs64_tmp_child_name(effective_path, effective_path_bytes, &tmp_name, &tmp_name_bytes) != 0u)
        {
            if ((create_requested == 0u)
                && (linux_vfs64_find_tmp_record(pid, tmp_name, tmp_name_bytes) == 0))
            {
                ++g_linux_vfs64_denial_count;
                ++g_linux_vfs64_tmp_denial_count;
                return FD64_INVALID_FD;
            }

            if (((create_requested != 0u)
                    && (linux_vfs64_tmp_create_file(pid, effective_path, effective_path_bytes, mode) == 0u))
                || (linux_vfs64_tmp_ensure_namespace(pid) == 0u)
                || (linux_vfs64_tmp_backend_path(
                        pid,
                        effective_path,
                        effective_path_bytes,
                        backend_path,
                        (u32)sizeof(backend_path),
                        &backend_path_bytes) == 0u))
            {
                ++g_linux_vfs64_denial_count;
                return FD64_INVALID_FD;
            }

            fd_number = fd64_open_ramfs(pid, backend_path, backend_path_bytes, fd_flags, mode);
        }
        else
        {
            if (create_requested != 0u)
            {
                ++g_linux_vfs64_denial_count;
                return FD64_INVALID_FD;
            }
            fd_number = fd64_open_ramfs(pid, effective_path, effective_path_bytes, fd_flags, mode);
        }
        if (fd_number != FD64_INVALID_FD)
        {
            (void)linux_vfs64_record_fd_path(pid, fd_number, effective_path, effective_path_bytes);
            ++g_linux_vfs64_open_count;
        }
        return fd_number;
    }

    ++g_linux_vfs64_denial_count;
    return FD64_INVALID_FD;
}

u32 linux_vfs64_read_fd(u32 pid, u32 fd_number, u8 *output, u32 byte_count)
{
    u32 device_type;
    u32 handle;
    u32 index;
    u32 state;
    u32 bytes_read;
    fd_entry_t *entry;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if (output == 0)
    {
        ++g_linux_vfs64_denial_count;
        return LINUX_VFS64_INVALID_RESULT;
    }

    if (linux_vfs64_fd_is_linux_device(pid, fd_number, &device_type) == 0u)
    {
        return fd64_read(pid, fd_number, output, byte_count);
    }

    if (device_type == LINUX_VFS64_DEVICE_NULL)
    {
        ++g_linux_vfs64_read_count;
        return 0u;
    }

    if (device_type == LINUX_VFS64_DEVICE_ZERO)
    {
        for (index = 0u; index < byte_count; ++index)
        {
            output[index] = 0u;
        }
        ++g_linux_vfs64_read_count;
        return byte_count;
    }

    if (device_type == LINUX_VFS64_DEVICE_URANDOM)
    {
        state = g_linux_vfs64_random_counter
            ^ (pid * 1103515245u)
            ^ (fd_number * 2654435761u)
            ^ byte_count;
        for (index = 0u; index < byte_count; ++index)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            output[index] = (u8)(state & 0xFFu);
            if (output[index] == 0u)
            {
                output[index] = (u8)(0xA5u ^ (u8)index);
            }
        }
        g_linux_vfs64_random_counter = state + 0x9E3779B9u;
        ++g_linux_vfs64_read_count;
        return byte_count;
    }

    if (device_type == LINUX_VFS64_DEVICE_NVME_FILE)
    {
        static u8 nvme_scratch[LINUX_VFS64_NVME_FILE_BYTES];
        u8 path[LINUX_VFS64_MAX_PATH_BYTES];
        u32 path_byte_count;
        u32 file_bytes = 0u;
        u32 available;
        u32 copied;
        fd_entry_t *nvme_entry;

        nvme_entry = fd64_get(pid, fd_number);
        if ((nvme_entry == 0)
            || (linux_vfs64_fd_path(
                    pid,
                    fd_number,
                    path,
                    LINUX_VFS64_MAX_PATH_BYTES,
                    &path_byte_count) == 0u)
            || (linux_vfs64_nvme_read_all(
                    pid,
                    path,
                    path_byte_count,
                    nvme_scratch,
                    (u32)sizeof(nvme_scratch),
                    &file_bytes) == 0u))
        {
            if (nvme_entry != 0)
            {
                (void)fd64_put(pid, nvme_entry);
            }
            ++g_linux_vfs64_denial_count;
            return LINUX_VFS64_INVALID_RESULT;
        }

        if (nvme_entry->file_offset >= (u64)file_bytes)
        {
            (void)fd64_put(pid, nvme_entry);
            ++g_linux_vfs64_read_count;
            return 0u;
        }

        available = file_bytes - (u32)nvme_entry->file_offset;
        copied = (available < byte_count) ? available : byte_count;
        for (index = 0u; index < copied; ++index)
        {
            output[index] = nvme_scratch[(u32)nvme_entry->file_offset + index];
        }
        nvme_entry->file_offset += (u64)copied;
        (void)fd64_put(pid, nvme_entry);
        ++g_linux_vfs64_read_count;
        return copied;
    }

    if ((device_type == LINUX_VFS64_DEVICE_PROC_MAPS)
        || (device_type == LINUX_VFS64_DEVICE_PROC_EXE)
        || (device_type == LINUX_VFS64_DEVICE_PROC_STATUS)
        || (device_type == LINUX_VFS64_DEVICE_PROC_CMDLINE)
        || (device_type == LINUX_VFS64_DEVICE_PROC_ENVIRON)
        || (device_type == LINUX_VFS64_DEVICE_PROC_FD_LINK)
        || (device_type == LINUX_VFS64_DEVICE_PROC_MEMINFO))
    {
        entry = fd64_get(pid, fd_number);
        if (entry == 0)
        {
            ++g_linux_vfs64_denial_count;
            return LINUX_VFS64_INVALID_RESULT;
        }

        handle = entry->capability_handle;
        bytes_read = linux_vfs64_read_proc_payload(
            pid,
            device_type,
            handle,
            output,
            byte_count,
            entry->file_offset);
        if (bytes_read != LINUX_VFS64_INVALID_RESULT)
        {
            entry->file_offset += (u64)bytes_read;
        }
        (void)fd64_put(pid, entry);
        return bytes_read;
    }

    ++g_linux_vfs64_denial_count;
    return LINUX_VFS64_INVALID_RESULT;
}

u32 linux_vfs64_write_fd(u32 pid, u32 fd_number, const u8 *input, u32 byte_count)
{
    u32 device_type;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if (input == 0)
    {
        ++g_linux_vfs64_denial_count;
        return LINUX_VFS64_INVALID_RESULT;
    }

    if (linux_vfs64_fd_is_linux_device(pid, fd_number, &device_type) == 0u)
    {
        return fd64_write(pid, fd_number, input, byte_count);
    }

    if ((device_type == LINUX_VFS64_DEVICE_NULL)
        || (device_type == LINUX_VFS64_DEVICE_ZERO)
        || (device_type == LINUX_VFS64_DEVICE_URANDOM))
    {
        ++g_linux_vfs64_write_count;
        return byte_count;
    }

    if (device_type == LINUX_VFS64_DEVICE_NVME_FILE)
    {
        ++g_linux_vfs64_nvme_denial_count;
        ++g_linux_vfs64_denial_count;
        return LINUX_VFS64_INVALID_RESULT;
    }

    if ((device_type == LINUX_VFS64_DEVICE_PROC_MAPS)
        || (device_type == LINUX_VFS64_DEVICE_PROC_EXE)
        || (device_type == LINUX_VFS64_DEVICE_PROC_STATUS)
        || (device_type == LINUX_VFS64_DEVICE_PROC_CMDLINE)
        || (device_type == LINUX_VFS64_DEVICE_PROC_ENVIRON)
        || (device_type == LINUX_VFS64_DEVICE_PROC_FD_LINK)
        || (device_type == LINUX_VFS64_DEVICE_PROC_MEMINFO))
    {
        ++g_linux_vfs64_proc_denial_count;
    }

    ++g_linux_vfs64_denial_count;
    return LINUX_VFS64_INVALID_RESULT;
}

u32 linux_vfs64_delete(u32 pid, const u8 *path, u32 path_byte_count)
{
    u8 backend_path[LINUX_VFS64_TMP_BACKEND_BYTES];
    const u8 *name;
    u32 name_bytes;
    u32 backend_path_bytes;
    u32 owner_id;
    u32 base_capability;
    u32 deleted;
    linux_vfs64_tmp_record_t *record;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if ((linux_vfs64_tmp_child_name(path, path_byte_count, &name, &name_bytes) == 0u)
        || ((record = linux_vfs64_find_tmp_record(pid, name, name_bytes)) == 0)
        || (linux_vfs64_tmp_backend_path(
                pid,
                path,
                path_byte_count,
                backend_path,
                (u32)sizeof(backend_path),
                &backend_path_bytes) == 0u)
        || (linux_vfs64_tmp_base_capability(pid, &owner_id, &base_capability) == 0u))
    {
        ++g_linux_vfs64_denial_count;
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    if (record->node_type == LINUX_VFS64_DIRENT_TYPE_LNK)
    {
        (void)capability64_revoke(base_capability, owner_id);
        linux_vfs64_clear_tmp_record(record);
        ++g_linux_vfs64_tmp_delete_count;
        return 1u;
    }

    deleted = fs64_delete_kernel(base_capability, backend_path, backend_path_bytes, owner_id);
    (void)capability64_revoke(base_capability, owner_id);
    if (deleted == 0u)
    {
        ++g_linux_vfs64_denial_count;
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    (void)linux_vfs64_forget_tmp_file(pid, name, name_bytes);
    ++g_linux_vfs64_tmp_delete_count;
    return 1u;
}

u32 linux_vfs64_symlink(
    u32 pid,
    const u8 *target_path,
    u32 target_path_byte_count,
    const u8 *link_path,
    u32 link_path_byte_count)
{
    const u8 *link_name;
    u32 link_name_bytes;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if ((linux_vfs64_process_is_valid(pid) == 0u)
        || (target_path == 0)
        || (target_path_byte_count == 0u)
        || (target_path_byte_count > LINUX_VFS64_MAX_PATH_BYTES)
        || (target_path[0] != (u8)'/')
        || (linux_vfs64_tmp_child_name(
                link_path,
                link_path_byte_count,
                &link_name,
                &link_name_bytes) == 0u))
    {
        ++g_linux_vfs64_denial_count;
        ++g_linux_vfs64_tmp_denial_count;
        return 0u;
    }

    if ((linux_vfs64_tmp_ensure_namespace(pid) == 0u)
        || (linux_vfs64_record_tmp_symlink(
                pid,
                link_name,
                link_name_bytes,
                target_path,
                target_path_byte_count) == 0u))
    {
        ++g_linux_vfs64_denial_count;
        return 0u;
    }

    ++g_linux_vfs64_symlink_create_count;
    return 1u;
}

u32 linux_vfs64_readlink(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u8 *output,
    u32 byte_capacity)
{
    linux_vfs64_tmp_record_t *tmp_symlink;
    linux_vfs64_result_t result;
    u32 copied;
    u32 index;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if ((output == 0) || (byte_capacity == 0u))
    {
        ++g_linux_vfs64_denial_count;
        return LINUX_VFS64_INVALID_RESULT;
    }

    tmp_symlink = linux_vfs64_find_tmp_symlink_path(pid, path, path_byte_count);
    if (tmp_symlink != 0)
    {
        copied = (tmp_symlink->target_path_byte_count < byte_capacity)
            ? tmp_symlink->target_path_byte_count
            : byte_capacity;
        for (index = 0u; index < copied; ++index)
        {
            output[index] = tmp_symlink->target_path[index];
        }
        ++g_linux_vfs64_symlink_readlink_count;
        g_linux_vfs64_symlink_last_target_bytes = tmp_symlink->target_path_byte_count;
        return copied;
    }

    if ((linux_vfs64_resolve(pid, path, path_byte_count, LINUX_VFS64_OPEN_NOFOLLOW, &result) != 0u)
        && (result.node_type == LINUX_VFS64_NODE_PROC_SYMLINK)
        && (result.capability_handle != CAPABILITY64_INVALID_HANDLE))
    {
        copied = linux_vfs64_read_proc_payload(
            pid,
            result.device_type,
            result.capability_handle,
            output,
            byte_capacity,
            0ull);
        if (copied != LINUX_VFS64_INVALID_RESULT)
        {
            ++g_linux_vfs64_symlink_readlink_count;
            g_linux_vfs64_symlink_last_target_bytes = copied;
        }
        return copied;
    }

    ++g_linux_vfs64_denial_count;
    return LINUX_VFS64_INVALID_RESULT;
}

static void linux_vfs64_zero_fd_stat(fd64_stat_t *stat_out)
{
    if (stat_out == 0)
    {
        return;
    }

    stat_out->size = 0ull;
    stat_out->mtime = FD64_STAT_MTIME_UNAVAILABLE;
    stat_out->blocks = 0ull;
    stat_out->device_id = 0ull;
    stat_out->inode = 0ull;
    stat_out->mode = 0u;
    stat_out->fd_type = FD64_TYPE_EMPTY;
    stat_out->node_type = FD64_STAT_NODE_UNKNOWN;
    stat_out->rights = 0u;
    stat_out->owner_id = 0u;
    stat_out->link_count = 0u;
    stat_out->block_size = 0u;
    stat_out->fd_number = FD64_INVALID_FD;
    stat_out->capability_handle = CAPABILITY64_INVALID_HANDLE;
}

static u32 linux_vfs64_fill_symlink_stat(
    u32 pid,
    u32 target_path_byte_count,
    u32 path_token,
    fd64_stat_t *stat_out)
{
    if ((stat_out == 0) || (linux_vfs64_process_is_valid(pid) == 0u))
    {
        return 0u;
    }

    linux_vfs64_zero_fd_stat(stat_out);
    stat_out->size = (u64)target_path_byte_count;
    stat_out->blocks = 1ull;
    stat_out->mode = FD64_STAT_MODE_SYMLINK | FD64_STAT_MODE_READ | FD64_STAT_MODE_WRITE | FD64_STAT_MODE_EXEC;
    stat_out->fd_type = FD64_TYPE_RAMFS_NODE;
    stat_out->node_type = FD64_STAT_NODE_SYMLINK;
    stat_out->rights = FS64_RIGHT_READ | FS64_RIGHT_STAT;
    stat_out->owner_id = process64_principal(pid);
    stat_out->link_count = 1u;
    stat_out->block_size = 4096u;
    stat_out->capability_handle = path_token;
    stat_out->device_id = (((u64)process64_principal(pid)) << 32)
        | ((u64)LINUX_VFS64_PROVIDER_RAMFS << 16)
        | (u64)LINUX_VFS64_NODE_TMP_SYMLINK;
    stat_out->inode = (u64)path_token;
    return 1u;
}

static u64 linux_vfs64_stat_device_id(u32 pid, u32 provider, u32 node_type)
{
    return (((u64)process64_principal(pid)) << 32)
        | ((u64)provider << 16)
        | (u64)node_type;
}

static u64 linux_vfs64_stat_inode(const u8 *path, u32 path_byte_count, u32 fallback)
{
    u32 token = linux_vfs64_path_token(path, path_byte_count);

    if (token == 0u)
    {
        token = fallback;
    }
    if (token == 0u)
    {
        token = 1u;
    }

    return (u64)token;
}

static u32 linux_vfs64_fill_basic_stat(
    u32 pid,
    u32 provider,
    u32 vfs_node_type,
    u32 fd_type,
    u32 stat_node_type,
    u32 mode,
    u32 rights,
    u64 size,
    u32 link_count,
    u32 inode_fallback,
    const u8 *path,
    u32 path_byte_count,
    fd64_stat_t *stat_out)
{
    if ((stat_out == 0) || (linux_vfs64_process_is_valid(pid) == 0u))
    {
        return 0u;
    }

    linux_vfs64_zero_fd_stat(stat_out);
    stat_out->size = size;
    stat_out->mtime = FD64_STAT_MTIME_UNAVAILABLE;
    stat_out->blocks = (size + 511ull) / 512ull;
    stat_out->device_id = linux_vfs64_stat_device_id(pid, provider, vfs_node_type);
    stat_out->inode = linux_vfs64_stat_inode(path, path_byte_count, inode_fallback);
    stat_out->mode = mode;
    stat_out->fd_type = fd_type;
    stat_out->node_type = stat_node_type;
    stat_out->rights = rights;
    stat_out->owner_id = process64_principal(pid);
    stat_out->link_count = (link_count != 0u) ? link_count : 1u;
    stat_out->block_size = 4096u;
    stat_out->capability_handle = (u32)stat_out->inode;
    return 1u;
}

static u32 linux_vfs64_dir_mode(const linux_vfs64_result_t *result)
{
    u32 mode = FD64_STAT_MODE_DIR | FD64_STAT_MODE_READ | FD64_STAT_MODE_EXEC;
    const linux_vfs64_mount_t *mount;

    if ((result != 0)
        && (result->provider == LINUX_VFS64_PROVIDER_RAMFS)
        && (result->mount_index < LINUX_VFS64_MAX_MOUNTS))
    {
        mount = &g_linux_vfs64_mounts[result->mount_index];
        if ((mount->flags & LINUX_VFS64_OPEN_WRITE) != 0u)
        {
            mode |= FD64_STAT_MODE_WRITE;
        }
    }

    return mode;
}

static u32 linux_vfs64_dir_rights(const linux_vfs64_result_t *result)
{
    u32 rights = FS64_RIGHT_LIST | FS64_RIGHT_READ | FS64_RIGHT_STAT;
    const linux_vfs64_mount_t *mount;

    if ((result != 0)
        && (result->provider == LINUX_VFS64_PROVIDER_RAMFS)
        && (result->mount_index < LINUX_VFS64_MAX_MOUNTS))
    {
        mount = &g_linux_vfs64_mounts[result->mount_index];
        if ((mount->flags & LINUX_VFS64_OPEN_WRITE) != 0u)
        {
            rights |= FS64_RIGHT_CREATE | FS64_RIGHT_WRITE | FS64_RIGHT_DELETE | FS64_RIGHT_RENAME;
        }
    }

    return rights;
}

static u32 linux_vfs64_dev_char_mode(u32 device_type)
{
    u32 mode = FD64_STAT_MODE_CHAR;

    if ((device_type == LINUX_VFS64_DEVICE_NULL)
        || (device_type == LINUX_VFS64_DEVICE_ZERO)
        || (device_type == LINUX_VFS64_DEVICE_URANDOM)
        || (device_type == LINUX_VFS64_DEVICE_STDIN))
    {
        mode |= FD64_STAT_MODE_READ;
    }
    if ((device_type == LINUX_VFS64_DEVICE_NULL)
        || (device_type == LINUX_VFS64_DEVICE_STDOUT)
        || (device_type == LINUX_VFS64_DEVICE_STDERR))
    {
        mode |= FD64_STAT_MODE_WRITE;
    }

    return mode;
}

static u32 linux_vfs64_dev_char_rights(u32 device_type)
{
    u32 rights = FS64_RIGHT_STAT;

    if ((device_type == LINUX_VFS64_DEVICE_NULL)
        || (device_type == LINUX_VFS64_DEVICE_ZERO)
        || (device_type == LINUX_VFS64_DEVICE_URANDOM)
        || (device_type == LINUX_VFS64_DEVICE_STDIN))
    {
        rights |= FS64_RIGHT_READ;
    }
    if ((device_type == LINUX_VFS64_DEVICE_NULL)
        || (device_type == LINUX_VFS64_DEVICE_STDOUT)
        || (device_type == LINUX_VFS64_DEVICE_STDERR))
    {
        rights |= FS64_RIGHT_WRITE;
    }

    return rights;
}

static u32 linux_vfs64_proc_payload_byte_count(
    u32 pid,
    u32 device_type,
    u32 handle,
    u32 *byte_count_out)
{
    static u8 proc_scratch[LINUX_VFS64_PROC_SCRATCH_BYTES];
    linux_vfs64_proc_record_t *record = linux_vfs64_find_proc_record(pid);
    u32 payload_bytes = 0u;

    if (byte_count_out != 0)
    {
        *byte_count_out = 0u;
    }
    if (byte_count_out == 0)
    {
        return 0u;
    }

    if (device_type == LINUX_VFS64_DEVICE_PROC_MAPS)
    {
        if (linux_vfs64_build_proc_maps(pid, proc_scratch, (u32)sizeof(proc_scratch), &payload_bytes) == 0u)
        {
            return 0u;
        }
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_EXE)
    {
        if ((record != 0) && (record->exe_path_bytes != 0u))
        {
            payload_bytes = record->exe_path_bytes;
        }
        else if (linux_vfs64_append_cstr(
                proc_scratch,
                (u32)sizeof(proc_scratch),
                &payload_bytes,
                "/proc/self/exe") == 0u)
        {
            return 0u;
        }
        g_linux_vfs64_proc_last_exe_bytes = payload_bytes;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_STATUS)
    {
        if (linux_vfs64_build_proc_status(pid, proc_scratch, (u32)sizeof(proc_scratch), &payload_bytes) == 0u)
        {
            return 0u;
        }
        g_linux_vfs64_proc_last_status_bytes = payload_bytes;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_MEMINFO)
    {
        if (linux_vfs64_build_proc_meminfo(proc_scratch, (u32)sizeof(proc_scratch), &payload_bytes) == 0u)
        {
            return 0u;
        }
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_CMDLINE)
    {
        if ((record != 0) && (record->cmdline_bytes != 0u))
        {
            payload_bytes = record->cmdline_bytes;
        }
        else if ((record != 0) && (record->exe_path_bytes != 0u))
        {
            payload_bytes = record->exe_path_bytes;
        }
        g_linux_vfs64_proc_last_cmdline_bytes = payload_bytes;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_ENVIRON)
    {
        payload_bytes = ((record != 0) && (record->environ_bytes != 0u))
            ? record->environ_bytes
            : 0u;
        g_linux_vfs64_proc_last_environ_bytes = payload_bytes;
    }
    else if (device_type == LINUX_VFS64_DEVICE_PROC_FD_LINK)
    {
        if (linux_vfs64_build_proc_fd_link(
                pid,
                linux_vfs64_proc_fd_from_handle(handle),
                proc_scratch,
                (u32)sizeof(proc_scratch),
                &payload_bytes) == 0u)
        {
            return 0u;
        }
    }
    else
    {
        return 0u;
    }

    *byte_count_out = payload_bytes;
    return 1u;
}

static u32 linux_vfs64_fill_resolved_stat(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    const linux_vfs64_result_t *result,
    fd64_stat_t *stat_out)
{
    u32 payload_bytes;
    u32 mode;
    u32 rights;

    if ((result == 0) || (stat_out == 0))
    {
        return 0u;
    }

    if (linux_vfs64_result_is_directory(result, path, path_byte_count) != 0u)
    {
        return linux_vfs64_fill_basic_stat(
            pid,
            result->provider,
            result->node_type,
            FD64_TYPE_DEVICE,
            FD64_STAT_NODE_DIRECTORY,
            linux_vfs64_dir_mode(result),
            linux_vfs64_dir_rights(result),
            0ull,
            2u,
            result->path_token,
            path,
            path_byte_count,
            stat_out);
    }

    if ((result->provider == LINUX_VFS64_PROVIDER_DEV)
        && (result->node_type == LINUX_VFS64_NODE_DEV_CHAR))
    {
        return linux_vfs64_fill_basic_stat(
            pid,
            result->provider,
            result->node_type,
            FD64_TYPE_DEVICE,
            FD64_STAT_NODE_CHAR,
            linux_vfs64_dev_char_mode(result->device_type),
            linux_vfs64_dev_char_rights(result->device_type),
            0ull,
            1u,
            result->capability_handle,
            path,
            path_byte_count,
            stat_out);
    }

    if ((result->provider == LINUX_VFS64_PROVIDER_PROC)
        && (result->node_type == LINUX_VFS64_NODE_PROC_FILE))
    {
        if (linux_vfs64_proc_payload_byte_count(
                pid,
                result->device_type,
                result->capability_handle,
                &payload_bytes) == 0u)
        {
            return 0u;
        }
        return linux_vfs64_fill_basic_stat(
            pid,
            result->provider,
            result->node_type,
            FD64_TYPE_DEVICE,
            FD64_STAT_NODE_FILE,
            FD64_STAT_MODE_FILE | FD64_STAT_MODE_READ,
            FS64_RIGHT_READ | FS64_RIGHT_STAT,
            (u64)payload_bytes,
            1u,
            result->capability_handle,
            path,
            path_byte_count,
            stat_out);
    }

    if ((result->provider == LINUX_VFS64_PROVIDER_PROC)
        && (result->node_type == LINUX_VFS64_NODE_PROC_SYMLINK))
    {
        u8 target[LINUX_VFS64_MAX_PATH_BYTES];
        u32 target_bytes = linux_vfs64_readlink(
            pid,
            path,
            path_byte_count,
            target,
            (u32)sizeof(target));
        if (target_bytes == LINUX_VFS64_INVALID_RESULT)
        {
            return 0u;
        }
        return linux_vfs64_fill_symlink_stat(
            pid,
            target_bytes,
            linux_vfs64_path_token(path, path_byte_count),
            stat_out);
    }

    if ((result->provider == LINUX_VFS64_PROVIDER_NVME)
        && (result->node_type == LINUX_VFS64_NODE_NVME_FILE))
    {
        mmio64_nvme_fat_stat_t fat_stat;

        if ((linux_vfs64_nvme_stat(
                pid,
                path,
                path_byte_count,
                &fat_stat) == 0u)
            || (fat_stat.entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_FILE))
        {
            return 0u;
        }
        return linux_vfs64_fill_basic_stat(
            pid,
            result->provider,
            result->node_type,
            FD64_TYPE_DEVICE,
            FD64_STAT_NODE_FILE,
            FD64_STAT_MODE_FILE | FD64_STAT_MODE_READ,
            FS64_RIGHT_READ | FS64_RIGHT_STAT,
            (u64)fat_stat.byte_count,
            1u,
            result->path_token,
            path,
            path_byte_count,
            stat_out);
    }

    mode = 0u;
    rights = 0u;
    (void)mode;
    (void)rights;
    return 0u;
}

u32 linux_vfs64_lstat(u32 pid, const u8 *path, u32 path_byte_count, fd64_stat_t *stat_out)
{
    linux_vfs64_tmp_record_t *tmp_symlink;
    linux_vfs64_result_t result;
    u32 fd_number;
    u32 ok;
    u32 resolved_ok;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if (stat_out == 0)
    {
        ++g_linux_vfs64_denial_count;
        return 0u;
    }
    linux_vfs64_zero_fd_stat(stat_out);

    tmp_symlink = linux_vfs64_find_tmp_symlink_path(pid, path, path_byte_count);
    if (tmp_symlink != 0)
    {
        if (linux_vfs64_fill_symlink_stat(
                pid,
                tmp_symlink->target_path_byte_count,
                linux_vfs64_path_token(path, path_byte_count),
                stat_out) == 0u)
        {
            ++g_linux_vfs64_denial_count;
            return 0u;
        }
        ++g_linux_vfs64_symlink_lstat_count;
        g_linux_vfs64_symlink_last_target_bytes = tmp_symlink->target_path_byte_count;
        return 1u;
    }

    resolved_ok = linux_vfs64_resolve(pid, path, path_byte_count, LINUX_VFS64_OPEN_NOFOLLOW, &result);
    if ((resolved_ok != 0u) && (result.node_type == LINUX_VFS64_NODE_PROC_SYMLINK))
    {
        u8 target[LINUX_VFS64_MAX_PATH_BYTES];
        u32 target_bytes = linux_vfs64_readlink(
            pid,
            path,
            path_byte_count,
            target,
            (u32)sizeof(target));
        if ((target_bytes == LINUX_VFS64_INVALID_RESULT)
            || (linux_vfs64_fill_symlink_stat(
                    pid,
                    target_bytes,
                    linux_vfs64_path_token(path, path_byte_count),
                    stat_out) == 0u))
        {
            ++g_linux_vfs64_denial_count;
            return 0u;
        }
        ++g_linux_vfs64_symlink_lstat_count;
        return 1u;
    }

    if ((resolved_ok != 0u)
        && (result.provider != LINUX_VFS64_PROVIDER_NONE)
        && (linux_vfs64_fill_resolved_stat(pid, path, path_byte_count, &result, stat_out) != 0u))
    {
        return 1u;
    }

    fd_number = linux_vfs64_open(pid, path, path_byte_count, 0u, 0u);
    if (fd_number == FD64_INVALID_FD)
    {
        return 0u;
    }

    ok = fd64_fstat(pid, fd_number, stat_out);
    (void)linux_vfs64_forget_fd_path(pid, fd_number);
    (void)fd64_close(pid, fd_number);
    return ok;
}

u32 linux_vfs64_stat(u32 pid, const u8 *path, u32 path_byte_count, fd64_stat_t *stat_out)
{
    u8 current_path[LINUX_VFS64_MAX_PATH_BYTES];
    u8 target_path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 current_bytes;
    u32 target_bytes;
    u32 depth;
    u32 index;
    fd64_stat_t temp_stat;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if ((stat_out == 0)
        || (path == 0)
        || (path_byte_count == 0u)
        || (path_byte_count > LINUX_VFS64_MAX_PATH_BYTES))
    {
        ++g_linux_vfs64_denial_count;
        return 0u;
    }

    for (index = 0u; index < path_byte_count; ++index)
    {
        current_path[index] = path[index];
    }
    current_bytes = path_byte_count;

    for (depth = 0u; depth < LINUX_VFS64_SYMLINK_MAX_DEPTH; ++depth)
    {
        if (linux_vfs64_lstat(pid, current_path, current_bytes, &temp_stat) == 0u)
        {
            return 0u;
        }

        if (temp_stat.node_type != FD64_STAT_NODE_SYMLINK)
        {
            *stat_out = temp_stat;
            return 1u;
        }

        target_bytes = linux_vfs64_readlink(
            pid,
            current_path,
            current_bytes,
            target_path,
            (u32)sizeof(target_path));
        if ((target_bytes == LINUX_VFS64_INVALID_RESULT)
            || (target_bytes == 0u)
            || ((target_bytes == current_bytes)
                && (linux_vfs64_bytes_equal(target_path, current_path, current_bytes) != 0u)))
        {
            ++g_linux_vfs64_denial_count;
            return 0u;
        }

        for (index = 0u; index < target_bytes; ++index)
        {
            current_path[index] = target_path[index];
        }
        current_bytes = target_bytes;
    }

    ++g_linux_vfs64_denial_count;
    return 0u;
}

u32 linux_vfs64_fstat(u32 pid, u32 fd_number, fd64_stat_t *stat_out)
{
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 path_byte_count;
    u32 fd_type;
    u32 device_type;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if ((stat_out == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        ++g_linux_vfs64_denial_count;
        return 0u;
    }

    fd_type = fd64_entry_type(pid, fd_number);
    if (fd_type == FD64_TYPE_RAMFS_NODE)
    {
        return fd64_fstat(pid, fd_number, stat_out);
    }

    if (fd_type != FD64_TYPE_DEVICE)
    {
        ++g_linux_vfs64_denial_count;
        return 0u;
    }

    if (linux_vfs64_fd_path(pid, fd_number, path, LINUX_VFS64_MAX_PATH_BYTES, &path_byte_count) != 0u)
    {
        return linux_vfs64_lstat(pid, path, path_byte_count, stat_out);
    }

    device_type = linux_vfs64_device_type_from_handle(fd64_entry_capability(pid, fd_number));
    if (device_type == LINUX_VFS64_DEVICE_UNKNOWN)
    {
        ++g_linux_vfs64_denial_count;
        return 0u;
    }

    if (device_type == LINUX_VFS64_DEVICE_DIRECTORY)
    {
        return linux_vfs64_fill_basic_stat(
            pid,
            LINUX_VFS64_PROVIDER_NONE,
            LINUX_VFS64_NODE_UNKNOWN,
            FD64_TYPE_DEVICE,
            FD64_STAT_NODE_DIRECTORY,
            FD64_STAT_MODE_DIR | FD64_STAT_MODE_READ | FD64_STAT_MODE_EXEC,
            FS64_RIGHT_LIST | FS64_RIGHT_READ | FS64_RIGHT_STAT,
            0ull,
            2u,
            fd64_entry_capability(pid, fd_number),
            (const u8 *)"/",
            1u,
            stat_out);
    }

    return linux_vfs64_fill_basic_stat(
        pid,
        LINUX_VFS64_PROVIDER_DEV,
        LINUX_VFS64_NODE_DEV_CHAR,
        FD64_TYPE_DEVICE,
        FD64_STAT_NODE_CHAR,
        linux_vfs64_dev_char_mode(device_type),
        linux_vfs64_dev_char_rights(device_type),
        0ull,
        1u,
        fd64_entry_capability(pid, fd_number),
        (const u8 *)"/dev",
        4u,
        stat_out);
}

u32 linux_vfs64_path_is_directory(u32 pid, const u8 *path, u32 path_byte_count)
{
    linux_vfs64_result_t result;

    if (linux_vfs64_resolve(pid, path, path_byte_count, LINUX_VFS64_OPEN_READ, &result) == 0u)
    {
        return 0u;
    }

    return linux_vfs64_result_is_directory(&result, path, path_byte_count);
}

u32 linux_vfs64_fd_path(
    u32 pid,
    u32 fd_number,
    u8 *path_out,
    u32 max_path_bytes,
    u32 *path_byte_count)
{
    u32 index;
    u32 byte_index;

    if (path_byte_count != 0)
    {
        *path_byte_count = 0u;
    }

    if ((path_out == 0)
        || (path_byte_count == 0)
        || (max_path_bytes == 0u)
        || (fd_number >= FD64_TABLE_LIMIT))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_VFS64_MAX_FD_PATH_RECORDS; ++index)
    {
        if ((g_linux_vfs64_fd_paths[index].active != 0u)
            && (g_linux_vfs64_fd_paths[index].pid == pid)
            && (g_linux_vfs64_fd_paths[index].fd_number == fd_number))
        {
            if (g_linux_vfs64_fd_paths[index].path_byte_count > max_path_bytes)
            {
                return 0u;
            }
            *path_byte_count = g_linux_vfs64_fd_paths[index].path_byte_count;
            for (byte_index = 0u; byte_index < *path_byte_count; ++byte_index)
            {
                path_out[byte_index] = g_linux_vfs64_fd_paths[index].path[byte_index];
            }
            return 1u;
        }
    }

    return 0u;
}

u32 linux_vfs64_forget_fd_path(u32 pid, u32 fd_number)
{
    u32 index;

    for (index = 0u; index < LINUX_VFS64_MAX_FD_PATH_RECORDS; ++index)
    {
        if ((g_linux_vfs64_fd_paths[index].active != 0u)
            && (g_linux_vfs64_fd_paths[index].pid == pid)
            && (g_linux_vfs64_fd_paths[index].fd_number == fd_number))
        {
            linux_vfs64_clear_fd_path_record(&g_linux_vfs64_fd_paths[index]);
            return 1u;
        }
    }

    return 0u;
}

u32 linux_vfs64_fd_dir_cursor(u32 pid, u32 fd_number, u32 *cursor_out)
{
    u32 index;

    if (cursor_out != 0)
    {
        *cursor_out = 0u;
    }

    if ((cursor_out == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_VFS64_MAX_FD_PATH_RECORDS; ++index)
    {
        if ((g_linux_vfs64_fd_paths[index].active != 0u)
            && (g_linux_vfs64_fd_paths[index].pid == pid)
            && (g_linux_vfs64_fd_paths[index].fd_number == fd_number))
        {
            *cursor_out = g_linux_vfs64_fd_paths[index].dir_cursor;
            return 1u;
        }
    }

    return 0u;
}

u32 linux_vfs64_set_fd_dir_cursor(u32 pid, u32 fd_number, u32 cursor)
{
    u32 index;

    if (fd_number >= FD64_TABLE_LIMIT)
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_VFS64_MAX_FD_PATH_RECORDS; ++index)
    {
        if ((g_linux_vfs64_fd_paths[index].active != 0u)
            && (g_linux_vfs64_fd_paths[index].pid == pid)
            && (g_linux_vfs64_fd_paths[index].fd_number == fd_number))
        {
            g_linux_vfs64_fd_paths[index].dir_cursor = cursor;
            return 1u;
        }
    }

    return 0u;
}

u32 linux_vfs64_read_dirent(u32 pid, u32 fd_number, u32 cursor, linux_vfs64_dirent_t *entry_out)
{
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 path_byte_count;

    if ((entry_out == 0)
        || (fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, fd_number) == FD64_TYPE_EMPTY)
        || (linux_vfs64_fd_path(
                pid,
                fd_number,
                path,
                LINUX_VFS64_MAX_PATH_BYTES,
                &path_byte_count) == 0u)
        || (linux_vfs64_path_is_directory(pid, path, path_byte_count) == 0u))
    {
        return LINUX_VFS64_READDIR_NOT_DIRECTORY;
    }

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/") != 0u)
    {
        return linux_vfs64_dirent_from_table(
            &g_linux_vfs64_root_dirents[0],
            (u32)(sizeof(g_linux_vfs64_root_dirents) / sizeof(g_linux_vfs64_root_dirents[0])),
            cursor,
            entry_out);
    }

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/dev") != 0u)
    {
        return linux_vfs64_dirent_from_table(
            &g_linux_vfs64_dev_dirents[0],
            (u32)(sizeof(g_linux_vfs64_dev_dirents) / sizeof(g_linux_vfs64_dev_dirents[0])),
            cursor,
            entry_out);
    }

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc") != 0u)
    {
        return linux_vfs64_dirent_from_table(
            &g_linux_vfs64_proc_dirents[0],
            (u32)(sizeof(g_linux_vfs64_proc_dirents) / sizeof(g_linux_vfs64_proc_dirents[0])),
            cursor,
            entry_out);
    }

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self") != 0u)
    {
        return linux_vfs64_dirent_from_table(
            &g_linux_vfs64_proc_self_dirents[0],
            (u32)(sizeof(g_linux_vfs64_proc_self_dirents) / sizeof(g_linux_vfs64_proc_self_dirents[0])),
            cursor,
            entry_out);
    }

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/proc/self/fd") != 0u)
    {
        if (cursor == 0u)
        {
            g_linux_vfs64_proc_last_fd_entries = 0u;
        }
        return linux_vfs64_read_proc_fd_dirent(pid, cursor, fd_number, entry_out);
    }

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/tmp") != 0u)
    {
        return linux_vfs64_read_tmp_dirent(pid, cursor, entry_out);
    }

    if (linux_vfs64_path_is_exact(path, path_byte_count, "/nvme") != 0u)
    {
        return linux_vfs64_read_nvme_dirent(
            pid,
            path,
            path_byte_count,
            cursor,
            entry_out);
    }

    if ((path_byte_count > 6u)
        && (linux_vfs64_bytes_equal(path, g_linux_vfs64_prefix_nvme, 5u) != 0u)
        && (path[5] == (u8)'/'))
    {
        return linux_vfs64_read_nvme_dirent(
            pid,
            path,
            path_byte_count,
            cursor,
            entry_out);
    }

    return LINUX_VFS64_READDIR_NOT_DIRECTORY;
}

u32 linux_vfs64_device_handle(u32 device_type)
{
    if ((device_type == LINUX_VFS64_DEVICE_UNKNOWN)
        || (device_type > LINUX_VFS64_DEVICE_LAST))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }

    return LINUX_VFS64_DEVICE_HANDLE_TAG | device_type;
}

u32 linux_vfs64_device_type_from_handle(u32 handle)
{
    u32 device_type;

    if ((handle & LINUX_VFS64_DEVICE_HANDLE_MASK) != LINUX_VFS64_DEVICE_HANDLE_TAG)
    {
        return LINUX_VFS64_DEVICE_UNKNOWN;
    }

    device_type = handle & LINUX_VFS64_DEVICE_HANDLE_KIND_MASK;
    return (device_type <= LINUX_VFS64_DEVICE_LAST) ? device_type : LINUX_VFS64_DEVICE_UNKNOWN;
}

u32 linux_vfs64_proc_set_identity(
    u32 pid,
    const u8 *exe_path,
    u32 exe_path_bytes,
    const u8 *cmdline,
    u32 cmdline_bytes,
    const u8 *environ,
    u32 environ_bytes)
{
    linux_vfs64_proc_record_t *record;

    if (g_linux_vfs64_initialized == 0u)
    {
        linux_vfs64_init();
    }

    if ((linux_vfs64_process_is_valid(pid) == 0u)
        || (exe_path == 0)
        || (exe_path_bytes == 0u)
        || (exe_path_bytes > LINUX_VFS64_PROC_IDENTITY_BYTES)
        || (cmdline_bytes > LINUX_VFS64_PROC_PAYLOAD_BYTES)
        || (environ_bytes > LINUX_VFS64_PROC_PAYLOAD_BYTES)
        || ((cmdline == 0) && (cmdline_bytes != 0u))
        || ((environ == 0) && (environ_bytes != 0u)))
    {
        ++g_linux_vfs64_proc_denial_count;
        return 0u;
    }

    record = linux_vfs64_acquire_proc_record(pid);
    if (record == 0)
    {
        ++g_linux_vfs64_proc_denial_count;
        return 0u;
    }

    record->exe_path_bytes = exe_path_bytes;
    record->cmdline_bytes = cmdline_bytes;
    record->environ_bytes = environ_bytes;
    (void)linux_vfs64_copy_bounded(
        record->exe_path,
        LINUX_VFS64_PROC_IDENTITY_BYTES,
        exe_path,
        exe_path_bytes);
    (void)linux_vfs64_copy_bounded(
        record->cmdline,
        LINUX_VFS64_PROC_PAYLOAD_BYTES,
        (cmdline != 0) ? cmdline : (const u8 *)"",
        cmdline_bytes);
    (void)linux_vfs64_copy_bounded(
        record->environ,
        LINUX_VFS64_PROC_PAYLOAD_BYTES,
        (environ != 0) ? environ : (const u8 *)"",
        environ_bytes);
    return 1u;
}

u32 linux_vfs64_proc_clear_identity(u32 pid)
{
    linux_vfs64_proc_record_t *record = linux_vfs64_find_proc_record(pid);

    if (record == 0)
    {
        return 0u;
    }

    linux_vfs64_clear_proc_record(record);
    return 1u;
}

u32 linux_vfs64_denial_count(void)
{
    return g_linux_vfs64_denial_count;
}

u32 linux_vfs64_open_count(void)
{
    return g_linux_vfs64_open_count;
}

u32 linux_vfs64_read_count(void)
{
    return g_linux_vfs64_read_count;
}

u32 linux_vfs64_write_count(void)
{
    return g_linux_vfs64_write_count;
}

u32 linux_vfs64_proc_read_count(void)
{
    return g_linux_vfs64_proc_read_count;
}

u32 linux_vfs64_proc_denial_count(void)
{
    return g_linux_vfs64_proc_denial_count;
}

u32 linux_vfs64_proc_last_maps_regions(void)
{
    return g_linux_vfs64_proc_last_maps_regions;
}

u64 linux_vfs64_proc_last_maps_bytes(void)
{
    return g_linux_vfs64_proc_last_maps_bytes;
}

u32 linux_vfs64_proc_last_exe_bytes(void)
{
    return g_linux_vfs64_proc_last_exe_bytes;
}

u32 linux_vfs64_proc_last_status_bytes(void)
{
    return g_linux_vfs64_proc_last_status_bytes;
}

u32 linux_vfs64_proc_last_cmdline_bytes(void)
{
    return g_linux_vfs64_proc_last_cmdline_bytes;
}

u32 linux_vfs64_proc_last_environ_bytes(void)
{
    return g_linux_vfs64_proc_last_environ_bytes;
}

u32 linux_vfs64_proc_last_fd_entries(void)
{
    return g_linux_vfs64_proc_last_fd_entries;
}

u32 linux_vfs64_proc_last_fd_target(void)
{
    return g_linux_vfs64_proc_last_fd_target;
}

u32 linux_vfs64_proc_last_meminfo_bytes(void)
{
    return g_linux_vfs64_proc_last_meminfo_bytes;
}

u32 linux_vfs64_proc_last_mem_total_kib(void)
{
    return g_linux_vfs64_proc_last_mem_total_kib;
}

u32 linux_vfs64_proc_last_mem_free_kib(void)
{
    return g_linux_vfs64_proc_last_mem_free_kib;
}

u32 linux_vfs64_proc_last_mem_available_kib(void)
{
    return g_linux_vfs64_proc_last_mem_available_kib;
}

u32 linux_vfs64_proc_last_mem_claimed_kib(void)
{
    return g_linux_vfs64_proc_last_mem_claimed_kib;
}

u32 linux_vfs64_tmp_create_count(void)
{
    return g_linux_vfs64_tmp_create_count;
}

u32 linux_vfs64_tmp_delete_count(void)
{
    return g_linux_vfs64_tmp_delete_count;
}

u32 linux_vfs64_tmp_denial_count(void)
{
    return g_linux_vfs64_tmp_denial_count;
}

u32 linux_vfs64_tmp_last_dir_entries(void)
{
    return g_linux_vfs64_tmp_last_dir_entries;
}

u32 linux_vfs64_tmp_last_backend_path_bytes(void)
{
    return g_linux_vfs64_tmp_last_backend_path_bytes;
}

u32 linux_vfs64_tmp_last_namespace_pid(void)
{
    return g_linux_vfs64_tmp_last_namespace_pid;
}

u32 linux_vfs64_symlink_create_count(void)
{
    return g_linux_vfs64_symlink_create_count;
}

u32 linux_vfs64_symlink_follow_count(void)
{
    return g_linux_vfs64_symlink_follow_count;
}

u32 linux_vfs64_symlink_readlink_count(void)
{
    return g_linux_vfs64_symlink_readlink_count;
}

u32 linux_vfs64_symlink_lstat_count(void)
{
    return g_linux_vfs64_symlink_lstat_count;
}

u32 linux_vfs64_symlink_nofollow_denial_count(void)
{
    return g_linux_vfs64_symlink_nofollow_denial_count;
}

u32 linux_vfs64_symlink_last_target_bytes(void)
{
    return g_linux_vfs64_symlink_last_target_bytes;
}

u32 linux_vfs64_nvme_bind_count(void)
{
    return g_linux_vfs64_nvme_bind_count;
}

u32 linux_vfs64_nvme_release_count(void)
{
    return g_linux_vfs64_nvme_release_count;
}

u32 linux_vfs64_nvme_read_count(void)
{
    return g_linux_vfs64_nvme_read_count;
}

u32 linux_vfs64_nvme_readdir_count(void)
{
    return g_linux_vfs64_nvme_readdir_count;
}

u32 linux_vfs64_nvme_dirent_count(void)
{
    return g_linux_vfs64_nvme_dirent_count;
}

u32 linux_vfs64_nvme_denial_count(void)
{
    return g_linux_vfs64_nvme_denial_count;
}

u32 linux_vfs64_nvme_last_bytes(void)
{
    return g_linux_vfs64_nvme_last_bytes;
}
