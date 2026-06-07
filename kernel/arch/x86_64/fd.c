#include "fd_x64.h"

#include "console_x64.h"
#include "process_x64.h"
#include "fs_x64.h"
#include "linux_vfs_x64.h"
#include "persona_x64.h"
#include "pipe_x64.h"
#include "services.h"
#include "services_x64.h"

/*
 * B.1-B.10 add the first bounded file-descriptor table substrate. The code
 * integrates with process_x64.h through PID-based fd attachment and with
 * capability_x64.h for brokered handle revocation; the scaffold checkpoint
 * proves fd 0/1/2 initialization, allocation/free/get/put behavior, denial
 * on an out-of-range descriptor, and RAMFS path open through the existing
 * fs_x64 brokered node-capability surface, descriptor reads with file offset
 * advancement, console/RAMFS writes through the same brokered caps, and dup/
 * dup2 descriptor aliases that preserve the shared capability until the last
 * aliased fd is closed, SEEK_SET/SEEK_CUR/SEEK_END offset updates, and fstat
 * records filled from the RAMFS metadata exposed by a scoped node capability,
 * plus the close-on-exec primitive future exec paths must call. F.31 adds
 * positional RAMFS read/write helpers so Linux pread64/pwrite64 can prove
 * offset-specific I/O without mutating the descriptor's current file_offset.
 */

static fd_table_t g_fd64_tables[FD64_MAX_PROCESS_TABLES];
static u32 g_fd64_table_used[FD64_MAX_PROCESS_TABLES];
static u32 g_fd64_initialized = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_fd64_fork_copy_count = 0u;
static u32 g_fd64_fork_copy_denial_count = 0u;
static u32 g_fd64_fork_copy_last_parent_pid = PROCESS64_INVALID_PID;
static u32 g_fd64_fork_copy_last_child_pid = PROCESS64_INVALID_PID;
static u32 g_fd64_fork_copy_last_entries = 0u;
static u32 g_fd64_fork_copy_last_stage = 0u;
#endif

static void fd64_clear_entry(fd_entry_t *entry, u32 fd_number)
{
    if (entry == 0)
    {
        return;
    }

    entry->fd_number = fd_number;
    entry->capability_handle = CAPABILITY64_INVALID_HANDLE;
    entry->fd_type = FD64_TYPE_EMPTY;
    entry->flags = 0u;
    entry->file_offset = 0ull;
    entry->ref_count = 0u;
    entry->reserved = 0u;
}

static void fd64_clear_table(fd_table_t *table)
{
    u32 index;

    if (table == 0)
    {
        return;
    }

    table->pid = PROCESS64_INVALID_PID;
    table->owner_id = 0u;
    table->live_count = 0u;
    table->high_water_fd = 0u;
    table->denial_count = 0u;
    table->reserved = 0u;

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        fd64_clear_entry(&table->entries[index], index);
    }
}

static u32 fd64_entry_active(const fd_entry_t *entry)
{
    return ((entry != 0)
        && (entry->fd_type != FD64_TYPE_EMPTY)
        && (entry->capability_handle != CAPABILITY64_INVALID_HANDLE)
        && (entry->ref_count != 0u))
        ? 1u
        : 0u;
}

static u32 fd64_valid_type(u32 fd_type)
{
    return ((fd_type == FD64_TYPE_RAMFS_NODE)
        || (fd_type == FD64_TYPE_PIPE_READ)
        || (fd_type == FD64_TYPE_PIPE_WRITE)
        || (fd_type == FD64_TYPE_SOCKET)
        || (fd_type == FD64_TYPE_DEVICE)
        || (fd_type == FD64_TYPE_EVENTFD))
        ? 1u
        : 0u;
}

static u32 fd64_install_entry(
    fd_table_t *table,
    u32 fd_number,
    u32 capability_handle,
    u32 fd_type,
    u32 flags)
{
    fd_entry_t *entry;

    if ((table == 0)
        || (fd_number >= FD64_TABLE_LIMIT)
        || (capability_handle == CAPABILITY64_INVALID_HANDLE)
        || (fd64_valid_type(fd_type) == 0u)
        || ((flags & ~(FD64_FLAG_O_CLOEXEC | FD64_FLAG_O_NONBLOCK)) != 0u))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return 0u;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) != 0u)
    {
        ++table->denial_count;
        return 0u;
    }

    if (persona64_budget_check_fd(table->pid, table->live_count, 1u) == 0u)
    {
        ++table->denial_count;
        return 0u;
    }

    entry->fd_number = fd_number;
    entry->capability_handle = capability_handle;
    entry->fd_type = fd_type;
    entry->flags = flags;
    entry->file_offset = 0ull;
    entry->ref_count = 1u;
    ++table->live_count;
    if (fd_number > table->high_water_fd)
    {
        table->high_water_fd = fd_number;
    }

    return 1u;
}

static u32 fd64_revoke_entry_capability(fd_table_t *table, fd_entry_t *entry)
{
    if ((table == 0) || (entry == 0) || (fd64_entry_active(entry) == 0u))
    {
        return 0u;
    }

    if (entry->fd_type == FD64_TYPE_RAMFS_NODE)
    {
        return fs64_revoke(entry->capability_handle, table->owner_id);
    }

    if ((entry->fd_type == FD64_TYPE_PIPE_READ) || (entry->fd_type == FD64_TYPE_PIPE_WRITE))
    {
        return pipe64_revoke_handle(entry->capability_handle, table->owner_id);
    }

    return capability64_revoke(entry->capability_handle, table->owner_id);
}

static u32 fd64_shared_entry_count(fd_table_t *table, u32 capability_handle, u32 fd_type)
{
    u32 index;
    u32 count = 0u;

    if ((table == 0) || (capability_handle == CAPABILITY64_INVALID_HANDLE))
    {
        return 0u;
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        if ((fd64_entry_active(&table->entries[index]) != 0u)
            && (table->entries[index].capability_handle == capability_handle)
            && (table->entries[index].fd_type == fd_type))
        {
            ++count;
        }
    }

    return count;
}

static void fd64_sync_shared_ref_counts(fd_table_t *table, u32 capability_handle, u32 fd_type)
{
    u32 index;
    u32 count = fd64_shared_entry_count(table, capability_handle, fd_type);

    if (count == 0u)
    {
        return;
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        if ((fd64_entry_active(&table->entries[index]) != 0u)
            && (table->entries[index].capability_handle == capability_handle)
            && (table->entries[index].fd_type == fd_type))
        {
            table->entries[index].ref_count = count;
        }
    }
}

static u32 fd64_prior_shared_entry_exists(fd_table_t *table, u32 fd_number)
{
    u32 index;
    fd_entry_t *entry;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        return 0u;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < fd_number; ++index)
    {
        if ((fd64_entry_active(&table->entries[index]) != 0u)
            && (table->entries[index].capability_handle == entry->capability_handle)
            && (table->entries[index].fd_type == entry->fd_type))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 fd64_parse_file_stat_size(const u8 *stat_bytes, u32 byte_count, u64 *size_out)
{
    static const u8 prefix[] = "type=file size=";
    u32 index;
    u64 value = 0ull;
    u32 saw_digit = 0u;

    if ((stat_bytes == 0) || (size_out == 0) || (byte_count < (sizeof(prefix) - 1u)))
    {
        return 0u;
    }

    for (index = 0u; index < (sizeof(prefix) - 1u); ++index)
    {
        if (stat_bytes[index] != prefix[index])
        {
            return 0u;
        }
    }

    while (index < byte_count)
    {
        if (stat_bytes[index] == (u8)'\n')
        {
            break;
        }

        if ((stat_bytes[index] < (u8)'0') || (stat_bytes[index] > (u8)'9'))
        {
            return 0u;
        }

        value = (value * 10ull) + (u64)(stat_bytes[index] - (u8)'0');
        if (value > 0xFFFFFFFFull)
        {
            return 0u;
        }
        saw_digit = 1u;
        ++index;
    }

    if (saw_digit == 0u)
    {
        return 0u;
    }

    *size_out = value;
    return 1u;
}

static void fd64_zero_stat(fd64_stat_t *stat_buf)
{
    if (stat_buf == 0)
    {
        return;
    }

    stat_buf->size = 0ull;
    stat_buf->mtime = FD64_STAT_MTIME_UNAVAILABLE;
    stat_buf->blocks = 0ull;
    stat_buf->device_id = 0ull;
    stat_buf->inode = 0ull;
    stat_buf->mode = 0u;
    stat_buf->fd_type = FD64_TYPE_EMPTY;
    stat_buf->node_type = FD64_STAT_NODE_UNKNOWN;
    stat_buf->rights = 0u;
    stat_buf->owner_id = 0u;
    stat_buf->link_count = 0u;
    stat_buf->block_size = 0u;
    stat_buf->fd_number = FD64_INVALID_FD;
    stat_buf->capability_handle = CAPABILITY64_INVALID_HANDLE;
}

static u32 fd64_parse_decimal_tail(
    const u8 *stat_bytes,
    u32 byte_count,
    u32 index,
    u64 *value_out)
{
    u64 value = 0ull;
    u32 saw_digit = 0u;

    if ((stat_bytes == 0) || (value_out == 0) || (index >= byte_count))
    {
        return 0u;
    }

    while (index < byte_count)
    {
        if (stat_bytes[index] == (u8)'\n')
        {
            break;
        }

        if ((stat_bytes[index] < (u8)'0') || (stat_bytes[index] > (u8)'9'))
        {
            return 0u;
        }

        value = (value * 10ull) + (u64)(stat_bytes[index] - (u8)'0');
        if (value > 0xFFFFFFFFull)
        {
            return 0u;
        }
        saw_digit = 1u;
        ++index;
    }

    if (saw_digit == 0u)
    {
        return 0u;
    }

    *value_out = value;
    return 1u;
}

static u32 fd64_span_matches(
    const u8 *stat_bytes,
    u32 byte_count,
    const u8 *prefix,
    u32 prefix_count)
{
    u32 index;

    if ((stat_bytes == 0) || (prefix == 0) || (byte_count < prefix_count))
    {
        return 0u;
    }

    for (index = 0u; index < prefix_count; ++index)
    {
        if (stat_bytes[index] != prefix[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 fd64_parse_ramfs_stat(const u8 *stat_bytes, u32 byte_count, fd64_stat_t *stat_buf)
{
    static const u8 file_prefix[] = "type=file size=";
    static const u8 dir_prefix[] = "type=dir entries=";
    u64 value = 0ull;

    if ((stat_bytes == 0) || (stat_buf == 0))
    {
        return 0u;
    }

    if (fd64_span_matches(stat_bytes, byte_count, file_prefix, (u32)(sizeof(file_prefix) - 1u)) != 0u)
    {
        if (fd64_parse_decimal_tail(
                stat_bytes,
                byte_count,
                (u32)(sizeof(file_prefix) - 1u),
                &value) == 0u)
        {
            return 0u;
        }

        stat_buf->node_type = FD64_STAT_NODE_FILE;
        stat_buf->size = value;
        return 1u;
    }

    if (fd64_span_matches(stat_bytes, byte_count, dir_prefix, (u32)(sizeof(dir_prefix) - 1u)) != 0u)
    {
        if (fd64_parse_decimal_tail(
                stat_bytes,
                byte_count,
                (u32)(sizeof(dir_prefix) - 1u),
                &value) == 0u)
        {
            return 0u;
        }

        stat_buf->node_type = FD64_STAT_NODE_DIRECTORY;
        stat_buf->size = 0ull;
        return 1u;
    }

    return 0u;
}

static u32 fd64_mode_from_ramfs_stat(u32 node_type, u32 rights)
{
    u32 mode = 0u;

    if (node_type == FD64_STAT_NODE_FILE)
    {
        mode = FD64_STAT_MODE_FILE;
        if ((rights & (FS64_RIGHT_READ | FS64_RIGHT_STAT)) != 0u)
        {
            mode |= FD64_STAT_MODE_READ;
        }
        if ((rights & FS64_RIGHT_WRITE) != 0u)
        {
            mode |= FD64_STAT_MODE_WRITE;
        }
        return mode;
    }

    if (node_type == FD64_STAT_NODE_DIRECTORY)
    {
        mode = FD64_STAT_MODE_DIR;
        if ((rights & (FS64_RIGHT_LIST | FS64_RIGHT_STAT)) != 0u)
        {
            mode |= FD64_STAT_MODE_READ | FD64_STAT_MODE_EXEC;
        }
        if ((rights & (FS64_RIGHT_CREATE | FS64_RIGHT_WRITE | FS64_RIGHT_DELETE | FS64_RIGHT_RENAME)) != 0u)
        {
            mode |= FD64_STAT_MODE_WRITE;
        }
        return mode;
    }

    return 0u;
}

static u32 fd64_ramfs_file_size(fd_table_t *table, fd_entry_t *entry, u64 *size_out)
{
    u8 stat_bytes[32];
    u32 index;
    u32 stat_count;

    if ((table == 0)
        || (entry == 0)
        || (size_out == 0)
        || (fd64_entry_active(entry) == 0u)
        || (entry->fd_type != FD64_TYPE_RAMFS_NODE))
    {
        return 0u;
    }

    for (index = 0u; index < (u32)sizeof(stat_bytes); ++index)
    {
        stat_bytes[index] = 0u;
    }

    stat_count = fs64_stat_kernel(
        entry->capability_handle,
        stat_bytes,
        (u32)sizeof(stat_bytes),
        table->owner_id);
    if (stat_count == FS64_INVALID_HANDLE)
    {
        return 0u;
    }

    return fd64_parse_file_stat_size(stat_bytes, stat_count, size_out);
}

static u32 fd64_apply_seek_delta(u64 base, s32 offset, u64 *result_out)
{
    u64 delta;

    if (result_out == 0)
    {
        return 0u;
    }

    if (offset < 0)
    {
        delta = (u64)(0u - (u32)offset);
        if (base < delta)
        {
            return 0u;
        }
        *result_out = base - delta;
        return 1u;
    }

    delta = (u64)(u32)offset;
    if (base > (0xFFFFFFFFull - delta))
    {
        return 0u;
    }

    *result_out = base + delta;
    return 1u;
}

void fd64_init(void)
{
    u32 index;

    if (g_fd64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < FD64_MAX_PROCESS_TABLES; ++index)
    {
        g_fd64_table_used[index] = 0u;
        fd64_clear_table(&g_fd64_tables[index]);
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_fd64_fork_copy_count = 0u;
    g_fd64_fork_copy_denial_count = 0u;
    g_fd64_fork_copy_last_parent_pid = PROCESS64_INVALID_PID;
    g_fd64_fork_copy_last_child_pid = PROCESS64_INVALID_PID;
    g_fd64_fork_copy_last_entries = 0u;
    g_fd64_fork_copy_last_stage = 0u;
#endif
    g_fd64_initialized = 1u;
}

fd_table_t *fd64_table_for_process(u32 pid)
{
    fd64_init();
    return (fd_table_t *)process64_fd_table(pid);
}

static fd_table_t *fd64_claim_table(u32 pid, u32 owner_id)
{
    u32 index;

    fd64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (owner_id == 0u)
        || (process64_principal(pid) == 0u))
    {
        return 0;
    }

    if (process64_fd_table(pid) != 0)
    {
        return (fd_table_t *)process64_fd_table(pid);
    }

    for (index = 0u; index < FD64_MAX_PROCESS_TABLES; ++index)
    {
        if (g_fd64_table_used[index] == 0u)
        {
            fd64_clear_table(&g_fd64_tables[index]);
            g_fd64_tables[index].pid = pid;
            g_fd64_tables[index].owner_id = owner_id;
            if (process64_attach_fd(pid, &g_fd64_tables[index]) == 0u)
            {
                fd64_clear_table(&g_fd64_tables[index]);
                return 0;
            }
            g_fd64_table_used[index] = 1u;
            return &g_fd64_tables[index];
        }
    }

    return 0;
}

u32 fd64_init_process(
    u32 pid,
    u32 owner_id,
    u32 stdin_capability,
    u32 stdout_capability,
    u32 stderr_capability)
{
    fd_table_t *table = fd64_claim_table(pid, owner_id);

    if (table == 0)
    {
        return 0u;
    }

    if (table->live_count != 0u)
    {
        return ((fd64_entry_active(&table->entries[FD64_STDIN]) != 0u)
            && (fd64_entry_active(&table->entries[FD64_STDOUT]) != 0u)
            && (fd64_entry_active(&table->entries[FD64_STDERR]) != 0u))
            ? 1u
            : 0u;
    }

    if ((fd64_install_entry(table, FD64_STDIN, stdin_capability, FD64_TYPE_DEVICE, 0u) == 0u)
        || (fd64_install_entry(table, FD64_STDOUT, stdout_capability, FD64_TYPE_DEVICE, 0u) == 0u)
        || (fd64_install_entry(table, FD64_STDERR, stderr_capability, FD64_TYPE_DEVICE, 0u) == 0u))
    {
        (void)fd64_release_process(pid);
        return 0u;
    }

    return 1u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 fd64_fork_copy_capability(
    const fd_entry_t *source,
    u32 parent_owner,
    u32 child_owner)
{
    u32 endpoint_id;
    u32 rights;
    u32 device_type;

    if ((source == 0)
        || (parent_owner == 0u)
        || (child_owner == 0u)
        || (source->capability_handle == CAPABILITY64_INVALID_HANDLE))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }

    if (source->fd_type == FD64_TYPE_DEVICE)
    {
        device_type = linux_vfs64_device_type_from_handle(source->capability_handle);
        if (device_type != LINUX_VFS64_DEVICE_UNKNOWN)
        {
            return source->capability_handle;
        }

        endpoint_id = capability64_target_endpoint(source->capability_handle, parent_owner);
        rights = capability64_rights(source->capability_handle, parent_owner);
        if (rights == 0u)
        {
            return CAPABILITY64_INVALID_HANDLE;
        }

        if (endpoint_id == services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT))
        {
            return capability64_grant_service(SERVICE_ENDPOINT_CLASS_INPUT, rights, child_owner);
        }
        if (endpoint_id == services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE))
        {
            return capability64_grant_service(SERVICE_ENDPOINT_CLASS_CONSOLE, rights, child_owner);
        }
    }

    /*
     * M23 only duplicates provider handles that can be owned independently.
     * RAMFS and pipe handles need provider-level duplication before a forked
     * child can close them without revoking the parent's descriptor.
     */
    return CAPABILITY64_INVALID_HANDLE;
}

u32 fd64_fork_process(u32 parent_pid, u32 child_pid)
{
    fd_table_t *parent_table;
    fd_table_t *child_table;
    u32 parent_owner;
    u32 child_owner;
    u32 index;
    u32 copied = 0u;

    fd64_init();
    g_fd64_fork_copy_last_parent_pid = parent_pid;
    g_fd64_fork_copy_last_child_pid = child_pid;
    g_fd64_fork_copy_last_entries = 0u;
    g_fd64_fork_copy_last_stage = 1u;

    parent_owner = process64_principal(parent_pid);
    child_owner = process64_principal(child_pid);
    parent_table = fd64_table_for_process(parent_pid);
    if ((parent_pid == PROCESS64_INVALID_PID)
        || (child_pid == PROCESS64_INVALID_PID)
        || (parent_pid == child_pid)
        || (parent_owner == 0u)
        || (child_owner == 0u)
        || (parent_table == 0)
        || (process64_fd_table(child_pid) != 0))
    {
        ++g_fd64_fork_copy_denial_count;
        return 0u;
    }

    g_fd64_fork_copy_last_stage = 2u;
    child_table = fd64_claim_table(child_pid, child_owner);
    if ((child_table == 0) || (child_table->live_count != 0u))
    {
        ++g_fd64_fork_copy_denial_count;
        (void)fd64_release_process(child_pid);
        return 0u;
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        fd_entry_t *source = &parent_table->entries[index];
        u32 child_capability;

        if (fd64_entry_active(source) == 0u)
        {
            continue;
        }

        g_fd64_fork_copy_last_stage = 3u;
        child_capability = fd64_fork_copy_capability(source, parent_owner, child_owner);
        if (child_capability == CAPABILITY64_INVALID_HANDLE)
        {
            ++g_fd64_fork_copy_denial_count;
            (void)fd64_release_process(child_pid);
            return 0u;
        }

        if (fd64_install_entry(
                child_table,
                index,
                child_capability,
                source->fd_type,
                source->flags) == 0u)
        {
            if (child_capability != source->capability_handle)
            {
                (void)capability64_revoke(child_capability, child_owner);
            }
            ++g_fd64_fork_copy_denial_count;
            (void)fd64_release_process(child_pid);
            return 0u;
        }
        child_table->entries[index].file_offset = source->file_offset;
        ++copied;
    }

    g_fd64_fork_copy_last_stage = 4u;
    g_fd64_fork_copy_last_entries = copied;
    ++g_fd64_fork_copy_count;
    return 1u;
}
#else
u32 fd64_fork_process(u32 parent_pid, u32 child_pid)
{
    (void)parent_pid;
    (void)child_pid;
    return 0u;
}
#endif

u32 fd64_release_process(u32 pid)
{
    fd_table_t *table = fd64_table_for_process(pid);
    u32 index;
    u32 released = 0u;

    if (table == 0)
    {
        return 0u;
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        if (fd64_entry_active(&table->entries[index]) != 0u)
        {
            if (fd64_prior_shared_entry_exists(table, index) == 0u)
            {
                (void)fd64_revoke_entry_capability(table, &table->entries[index]);
            }
        }
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        if (fd64_entry_active(&table->entries[index]) != 0u)
        {
            fd64_clear_entry(&table->entries[index], index);
            ++released;
        }
    }

    table->live_count = 0u;
    (void)process64_detach_fd(pid);

    for (index = 0u; index < FD64_MAX_PROCESS_TABLES; ++index)
    {
        if (&g_fd64_tables[index] == table)
        {
            fd64_clear_table(table);
            g_fd64_table_used[index] = 0u;
            break;
        }
    }

    return released;
}

u32 fd64_alloc(u32 pid, u32 capability_handle, u32 fd_type, u32 flags)
{
    fd_table_t *table = fd64_table_for_process(pid);
    u32 index;

    if (table == 0)
    {
        return FD64_INVALID_FD;
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        if (fd64_entry_active(&table->entries[index]) == 0u)
        {
            return (fd64_install_entry(table, index, capability_handle, fd_type, flags) != 0u)
                ? index
                : FD64_INVALID_FD;
        }
    }

    ++table->denial_count;
    return FD64_INVALID_FD;
}

u32 fd64_open_ramfs(u32 pid, const u8 *path, u32 path_byte_count, u32 flags, u32 mode)
{
    fd_table_t *table = fd64_table_for_process(pid);
    u32 base_capability;
    u32 node_capability;
    u32 fd_number;

    (void)mode;

    if ((table == 0)
        || (path == 0)
        || (path_byte_count == 0u)
        || ((flags & ~(FD64_FLAG_O_CLOEXEC | FD64_FLAG_O_NONBLOCK)) != 0u))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return FD64_INVALID_FD;
    }

    base_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_RAMFS,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        table->owner_id);
    if (base_capability == CAPABILITY64_INVALID_HANDLE)
    {
        ++table->denial_count;
        return FD64_INVALID_FD;
    }

    node_capability = fs64_open_kernel(base_capability, path, path_byte_count, table->owner_id);
    (void)capability64_revoke(base_capability, table->owner_id);
    if (node_capability == FS64_INVALID_HANDLE)
    {
        ++table->denial_count;
        return FD64_INVALID_FD;
    }

    fd_number = fd64_alloc(pid, node_capability, FD64_TYPE_RAMFS_NODE, flags);
    if (fd_number == FD64_INVALID_FD)
    {
        (void)fs64_revoke(node_capability, table->owner_id);
    }

    return fd_number;
}

u32 fd64_read(u32 pid, u32 fd_number, u8 *output, u32 byte_count)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;
    u32 bytes_read;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return FD64_IO_ERROR;
    }

    if (byte_count == 0u)
    {
        return 0u;
    }

    if (output == 0)
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) == 0u)
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    if (entry->file_offset > 0xFFFFFFFFull)
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    if (entry->fd_type == FD64_TYPE_RAMFS_NODE)
    {
        bytes_read = fs64_read_kernel(
            entry->capability_handle,
            output,
            (u32)entry->file_offset,
            byte_count,
            table->owner_id);
        if (bytes_read == FS64_INVALID_HANDLE)
        {
            ++table->denial_count;
            return FD64_IO_ERROR;
        }

        entry->file_offset += (u64)bytes_read;
        return bytes_read;
    }

    if (entry->fd_type == FD64_TYPE_PIPE_READ)
    {
        bytes_read = pipe64_read(
            entry->capability_handle,
            output,
            byte_count,
            table->owner_id);
        if (bytes_read == PIPE64_IO_BLOCKED)
        {
            return FD64_IO_BLOCKED;
        }
        if (bytes_read == PIPE64_IO_ERROR)
        {
            ++table->denial_count;
            return FD64_IO_ERROR;
        }

        return bytes_read;
    }

    ++table->denial_count;
    return FD64_IO_ERROR;
}

u32 fd64_write(u32 pid, u32 fd_number, const u8 *input, u32 byte_count)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;
    u32 bytes_written;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return FD64_IO_ERROR;
    }

    if (byte_count == 0u)
    {
        return 0u;
    }

    if (input == 0)
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) == 0u)
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    if (entry->fd_type == FD64_TYPE_DEVICE)
    {
        bytes_written = console64_write_kernel(
            entry->capability_handle,
            input,
            byte_count,
            table->owner_id);
        if (bytes_written == CONSOLE64_INVALID_RESULT)
        {
            ++table->denial_count;
            return FD64_IO_ERROR;
        }

        return bytes_written;
    }

    if (entry->fd_type == FD64_TYPE_RAMFS_NODE)
    {
        if (entry->file_offset > 0xFFFFFFFFull)
        {
            ++table->denial_count;
            return FD64_IO_ERROR;
        }

        bytes_written = fs64_write_kernel(
            entry->capability_handle,
            input,
            (u32)entry->file_offset,
            byte_count,
            table->owner_id);
        if (bytes_written == FS64_INVALID_HANDLE)
        {
            ++table->denial_count;
            return FD64_IO_ERROR;
        }

        entry->file_offset += (u64)bytes_written;
        return bytes_written;
    }

    if (entry->fd_type == FD64_TYPE_PIPE_WRITE)
    {
        bytes_written = pipe64_write(
            entry->capability_handle,
            input,
            byte_count,
            table->owner_id);
        if (bytes_written == PIPE64_IO_BLOCKED)
        {
            return FD64_IO_BLOCKED;
        }
        if (bytes_written == PIPE64_IO_ERROR)
        {
            ++table->denial_count;
            return FD64_IO_ERROR;
        }

        return bytes_written;
    }

    ++table->denial_count;
    return FD64_IO_ERROR;
}

u32 fd64_read_at(u32 pid, u32 fd_number, u64 file_offset, u8 *output, u32 byte_count)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;
    u32 bytes_read;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return FD64_IO_ERROR;
    }

    if (byte_count == 0u)
    {
        return 0u;
    }

    if ((output == 0) || (file_offset > 0xFFFFFFFFull))
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    entry = &table->entries[fd_number];
    if ((fd64_entry_active(entry) == 0u)
        || (entry->fd_type != FD64_TYPE_RAMFS_NODE))
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    bytes_read = fs64_read_kernel(
        entry->capability_handle,
        output,
        (u32)file_offset,
        byte_count,
        table->owner_id);
    if (bytes_read == FS64_INVALID_HANDLE)
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    return bytes_read;
}

u32 fd64_write_at(
    u32 pid,
    u32 fd_number,
    u64 file_offset,
    const u8 *input,
    u32 byte_count)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;
    u32 bytes_written;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return FD64_IO_ERROR;
    }

    if (byte_count == 0u)
    {
        return 0u;
    }

    if ((input == 0) || (file_offset > 0xFFFFFFFFull))
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    entry = &table->entries[fd_number];
    if ((fd64_entry_active(entry) == 0u)
        || (entry->fd_type != FD64_TYPE_RAMFS_NODE))
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    bytes_written = fs64_write_kernel(
        entry->capability_handle,
        input,
        (u32)file_offset,
        byte_count,
        table->owner_id);
    if (bytes_written == FS64_INVALID_HANDLE)
    {
        ++table->denial_count;
        return FD64_IO_ERROR;
    }

    return bytes_written;
}

u32 fd64_close(u32 pid, u32 fd_number)
{
    return fd64_free(pid, fd_number);
}

static u32 fd64_dup_into(fd_table_t *table, u32 old_fd_number, u32 new_fd_number)
{
    fd_entry_t source_snapshot;
    fd_entry_t *target;

    if ((table == 0)
        || (old_fd_number >= FD64_TABLE_LIMIT)
        || (new_fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return FD64_INVALID_FD;
    }

    if (fd64_entry_active(&table->entries[old_fd_number]) == 0u)
    {
        ++table->denial_count;
        return FD64_INVALID_FD;
    }

    if (old_fd_number == new_fd_number)
    {
        return new_fd_number;
    }

    source_snapshot = table->entries[old_fd_number];
    if (fd64_entry_active(&table->entries[new_fd_number]) != 0u)
    {
        if (fd64_free(table->pid, new_fd_number) == 0u)
        {
            ++table->denial_count;
            return FD64_INVALID_FD;
        }
    }

    if (persona64_budget_check_fd(table->pid, table->live_count, 1u) == 0u)
    {
        ++table->denial_count;
        return FD64_INVALID_FD;
    }

    target = &table->entries[new_fd_number];
    target->fd_number = new_fd_number;
    target->capability_handle = source_snapshot.capability_handle;
    target->fd_type = source_snapshot.fd_type;
    target->flags = source_snapshot.flags;
    target->file_offset = source_snapshot.file_offset;
    target->ref_count = 1u;
    target->reserved = 0u;
    ++table->live_count;
    if (new_fd_number > table->high_water_fd)
    {
        table->high_water_fd = new_fd_number;
    }

    fd64_sync_shared_ref_counts(table, source_snapshot.capability_handle, source_snapshot.fd_type);
    return new_fd_number;
}

u32 fd64_dup(u32 pid, u32 old_fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);
    u32 index;

    if (table == 0)
    {
        return FD64_INVALID_FD;
    }

    if ((old_fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_active(&table->entries[old_fd_number]) == 0u))
    {
        ++table->denial_count;
        return FD64_INVALID_FD;
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        if (fd64_entry_active(&table->entries[index]) == 0u)
        {
            return fd64_dup_into(table, old_fd_number, index);
        }
    }

    ++table->denial_count;
    return FD64_INVALID_FD;
}

u32 fd64_dup_min(u32 pid, u32 old_fd_number, u32 min_fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);
    u32 index;

    if (table == 0)
    {
        return FD64_INVALID_FD;
    }

    if ((old_fd_number >= FD64_TABLE_LIMIT)
        || (min_fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_active(&table->entries[old_fd_number]) == 0u))
    {
        ++table->denial_count;
        return FD64_INVALID_FD;
    }

    for (index = min_fd_number; index < FD64_TABLE_LIMIT; ++index)
    {
        if (fd64_entry_active(&table->entries[index]) == 0u)
        {
            return fd64_dup_into(table, old_fd_number, index);
        }
    }

    ++table->denial_count;
    return FD64_INVALID_FD;
}

u32 fd64_dup2(u32 pid, u32 old_fd_number, u32 new_fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if (table == 0)
    {
        return FD64_INVALID_FD;
    }

    return fd64_dup_into(table, old_fd_number, new_fd_number);
}

u32 fd64_dup3(u32 pid, u32 old_fd_number, u32 new_fd_number, u32 flags)
{
    fd_table_t *table = fd64_table_for_process(pid);
    u32 duplicated_fd;

    if (table == 0)
    {
        return FD64_INVALID_FD;
    }

    if ((old_fd_number == new_fd_number)
        || ((flags & ~(u32)FD64_FLAG_O_CLOEXEC) != 0u))
    {
        ++table->denial_count;
        return FD64_INVALID_FD;
    }

    duplicated_fd = fd64_dup_into(table, old_fd_number, new_fd_number);
    if (duplicated_fd == FD64_INVALID_FD)
    {
        return FD64_INVALID_FD;
    }

    table->entries[duplicated_fd].flags = flags;
    return duplicated_fd;
}

u32 fd64_set_entry_flags(u32 pid, u32 fd_number, u32 flags)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if ((table == 0)
        || (fd_number >= FD64_TABLE_LIMIT)
        || ((flags & ~(u32)(FD64_FLAG_O_CLOEXEC | FD64_FLAG_O_NONBLOCK)) != 0u))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return 0u;
    }

    if (fd64_entry_active(&table->entries[fd_number]) == 0u)
    {
        ++table->denial_count;
        return 0u;
    }

    table->entries[fd_number].flags = flags;
    return 1u;
}

u64 fd64_seek(u32 pid, u32 fd_number, s32 offset, u32 whence)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;
    u64 base;
    u64 new_offset;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return FD64_SEEK_ERROR;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) == 0u)
    {
        ++table->denial_count;
        return FD64_SEEK_ERROR;
    }

    if (entry->fd_type != FD64_TYPE_RAMFS_NODE)
    {
        ++table->denial_count;
        return FD64_SEEK_ERROR;
    }

    if (whence == FD64_SEEK_SET)
    {
        base = 0ull;
    }
    else if (whence == FD64_SEEK_CUR)
    {
        base = entry->file_offset;
    }
    else if (whence == FD64_SEEK_END)
    {
        if (fd64_ramfs_file_size(table, entry, &base) == 0u)
        {
            ++table->denial_count;
            return FD64_SEEK_ERROR;
        }
    }
    else
    {
        ++table->denial_count;
        return FD64_SEEK_ERROR;
    }

    if (fd64_apply_seek_delta(base, offset, &new_offset) == 0u)
    {
        ++table->denial_count;
        return FD64_SEEK_ERROR;
    }

    entry->file_offset = new_offset;
    return new_offset;
}

u32 fd64_stat(u32 pid, u32 fd_number, fd64_stat_t *stat_buf)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;
    u8 stat_bytes[32];
    u32 index;
    u32 stat_count;
    u32 rights;

    if (stat_buf != 0)
    {
        fd64_zero_stat(stat_buf);
    }

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT) || (stat_buf == 0))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return 0u;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) == 0u)
    {
        ++table->denial_count;
        return 0u;
    }

    if (entry->fd_type != FD64_TYPE_RAMFS_NODE)
    {
        ++table->denial_count;
        return 0u;
    }

    for (index = 0u; index < (u32)sizeof(stat_bytes); ++index)
    {
        stat_bytes[index] = 0u;
    }

    stat_count = fs64_stat_kernel(
        entry->capability_handle,
        stat_bytes,
        (u32)sizeof(stat_bytes),
        table->owner_id);
    if (stat_count == FS64_INVALID_HANDLE)
    {
        ++table->denial_count;
        return 0u;
    }

    if (fd64_parse_ramfs_stat(stat_bytes, stat_count, stat_buf) == 0u)
    {
        ++table->denial_count;
        fd64_zero_stat(stat_buf);
        return 0u;
    }

    rights = fs64_node_rights(entry->capability_handle, table->owner_id);
    if (rights == 0u)
    {
        ++table->denial_count;
        fd64_zero_stat(stat_buf);
        return 0u;
    }

    stat_buf->mode = fd64_mode_from_ramfs_stat(stat_buf->node_type, rights);
    stat_buf->fd_type = entry->fd_type;
    stat_buf->rights = rights;
    stat_buf->owner_id = fs64_node_owner(entry->capability_handle, table->owner_id);
    stat_buf->link_count = 1u;
    stat_buf->block_size = 4096u;
    stat_buf->blocks = (stat_buf->size + 511ull) / 512ull;
    stat_buf->device_id = (((u64)table->owner_id) << 32) | (u64)entry->fd_type;
    stat_buf->inode = (u64)entry->capability_handle;
    stat_buf->fd_number = fd_number;
    stat_buf->capability_handle = entry->capability_handle;
    return 1u;
}

u32 fd64_fstat(u32 pid, u32 fd_number, fd64_stat_t *stat_buf)
{
    return fd64_stat(pid, fd_number, stat_buf);
}

u32 fd64_close_on_exec(u32 pid)
{
    fd_table_t *table = fd64_table_for_process(pid);
    u32 index;
    u32 closed = 0u;

    if (table == 0)
    {
        return FD64_IO_ERROR;
    }

    for (index = 0u; index < FD64_TABLE_LIMIT; ++index)
    {
        if ((fd64_entry_active(&table->entries[index]) != 0u)
            && ((table->entries[index].flags & FD64_FLAG_O_CLOEXEC) != 0u))
        {
            if (fd64_free(pid, index) != 0u)
            {
                ++closed;
            }
        }
    }

    return closed;
}

u32 fd64_free(u32 pid, u32 fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;
    u32 shared_count;
    u32 capability_handle;
    u32 fd_type;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return 0u;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) == 0u)
    {
        ++table->denial_count;
        return 0u;
    }

    capability_handle = entry->capability_handle;
    fd_type = entry->fd_type;
    shared_count = fd64_shared_entry_count(table, capability_handle, fd_type);
    if (shared_count > 1u)
    {
        fd64_clear_entry(entry, fd_number);
        if (table->live_count != 0u)
        {
            --table->live_count;
        }
        fd64_sync_shared_ref_counts(table, capability_handle, fd_type);
        return 1u;
    }

    if (entry->ref_count > 1u)
    {
        --entry->ref_count;
        return 1u;
    }

    (void)fd64_revoke_entry_capability(table, entry);
    fd64_clear_entry(entry, fd_number);
    if (table->live_count != 0u)
    {
        --table->live_count;
    }
    return 1u;
}

fd_entry_t *fd64_get(u32 pid, u32 fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);
    fd_entry_t *entry;

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return 0;
    }

    entry = &table->entries[fd_number];
    if (fd64_entry_active(entry) == 0u)
    {
        ++table->denial_count;
        return 0;
    }

    ++entry->ref_count;
    return entry;
}

u32 fd64_put(u32 pid, fd_entry_t *entry)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if ((table == 0) || (entry == 0))
    {
        if (table != 0)
        {
            ++table->denial_count;
        }
        return 0u;
    }

    if ((entry < &table->entries[0])
        || (entry >= &table->entries[FD64_TABLE_LIMIT])
        || (fd64_entry_active(entry) == 0u))
    {
        ++table->denial_count;
        return 0u;
    }

    if (entry->ref_count > 1u)
    {
        --entry->ref_count;
    }
    return 1u;
}

u32 fd64_live_count(u32 pid)
{
    fd_table_t *table = fd64_table_for_process(pid);

    return (table != 0) ? table->live_count : 0u;
}

u32 fd64_high_water_fd(u32 pid)
{
    fd_table_t *table = fd64_table_for_process(pid);

    return (table != 0) ? table->high_water_fd : 0u;
}

u32 fd64_denial_count(u32 pid)
{
    fd_table_t *table = fd64_table_for_process(pid);

    return (table != 0) ? table->denial_count : 0u;
}

u32 fd64_entry_capability(u32 pid, u32 fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_active(&table->entries[fd_number]) == 0u))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }

    return table->entries[fd_number].capability_handle;
}

u32 fd64_entry_type(u32 pid, u32 fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_active(&table->entries[fd_number]) == 0u))
    {
        return FD64_TYPE_EMPTY;
    }

    return table->entries[fd_number].fd_type;
}

u32 fd64_entry_flags(u32 pid, u32 fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_active(&table->entries[fd_number]) == 0u))
    {
        return 0u;
    }

    return table->entries[fd_number].flags;
}

u64 fd64_entry_offset(u32 pid, u32 fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_active(&table->entries[fd_number]) == 0u))
    {
        return 0ull;
    }

    return table->entries[fd_number].file_offset;
}

u32 fd64_entry_ref_count(u32 pid, u32 fd_number)
{
    fd_table_t *table = fd64_table_for_process(pid);

    if ((table == 0) || (fd_number >= FD64_TABLE_LIMIT)
        || (fd64_entry_active(&table->entries[fd_number]) == 0u))
    {
        return 0u;
    }

    return table->entries[fd_number].ref_count;
}

u32 fd64_fork_copy_count(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_fd64_fork_copy_count;
#else
    return 0u;
#endif
}

u32 fd64_fork_copy_denial_count(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_fd64_fork_copy_denial_count;
#else
    return 0u;
#endif
}

u32 fd64_fork_copy_last_parent_pid(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_fd64_fork_copy_last_parent_pid;
#else
    return PROCESS64_INVALID_PID;
#endif
}

u32 fd64_fork_copy_last_child_pid(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_fd64_fork_copy_last_child_pid;
#else
    return PROCESS64_INVALID_PID;
#endif
}

u32 fd64_fork_copy_last_entries(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_fd64_fork_copy_last_entries;
#else
    return 0u;
#endif
}

u32 fd64_fork_copy_last_stage(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_fd64_fork_copy_last_stage;
#else
    return 0u;
#endif
}
