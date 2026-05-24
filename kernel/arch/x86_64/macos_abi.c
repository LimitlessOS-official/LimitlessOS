#include "macos_abi_x64.h"

#include "fd_x64.h"
#include "input_x64.h"
#include "macos_mach_x64.h"
#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * N.1 adds the first macOS BSD syscall switchboard. It integrates with
 * persona_x64.h for MACOS_MACHO process binding, persona_audit_x64.h for
 * translated and unavailable syscall records, fd_x64.h/input_x64.h for
 * brokered descriptor I/O, vma_x64.h/paging_x64.h for mmap/munmap/mprotect
 * and verified user buffers, and pit.h for clock_gettime. The checkpoint
 * proves table geometry, default persona table binding, real fd-backed
 * write/open/fstat/close, anonymous mmap/protect/munmap, getpid, sysctl,
 * audited ENOSYS, bad-persona denial, and exit cleanup without granting
 * ambient authority. N.2 also routes negative macOS syscall numbers to the
 * Mach trap switchboard in macos_mach.c.
 */

static macos_abi64_handler_t g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_LIMIT];
static u32 g_macos_abi64_initialized = 0u;
static u32 g_macos_abi64_dispatch_count = 0u;
static u32 g_macos_abi64_unimplemented_count = 0u;
static u32 g_macos_abi64_read_count = 0u;
static u32 g_macos_abi64_read_byte_count = 0u;
static u32 g_macos_abi64_write_count = 0u;
static u32 g_macos_abi64_write_byte_count = 0u;
static u32 g_macos_abi64_open_count = 0u;
static u32 g_macos_abi64_close_count = 0u;
static u32 g_macos_abi64_stat_count = 0u;
static u32 g_macos_abi64_fstat_count = 0u;
static u32 g_macos_abi64_mmap_count = 0u;
static u32 g_macos_abi64_munmap_count = 0u;
static u32 g_macos_abi64_mprotect_count = 0u;
static u32 g_macos_abi64_exit_count = 0u;
static u32 g_macos_abi64_getpid_count = 0u;
static u32 g_macos_abi64_clock_gettime_count = 0u;
static u32 g_macos_abi64_sysctl_count = 0u;
static u32 g_macos_abi64_denial_count = 0u;
static u32 g_macos_abi64_fault_count = 0u;
static u32 g_macos_abi64_last_syscall = 0u;
static u32 g_macos_abi64_last_result = 0u;
static u32 g_macos_abi64_last_fd = FD64_INVALID_FD;
static u32 g_macos_abi64_last_byte_count = 0u;
static u64 g_macos_abi64_last_address = 0ull;
static u32 g_macos_abi64_last_sysctl_name0 = 0u;
static u32 g_macos_abi64_last_sysctl_name1 = 0u;
static u32 g_macos_abi64_last_sysctl_bytes = 0u;
static u32 g_macos_abi64_last_exit_vma_regions = 0u;
static u32 g_macos_abi64_last_exit_fd_entries = 0u;
static u32 g_macos_abi64_last_exit_persona_released = 0u;
static u32 g_macos_abi64_last_exit_audit_released = 0u;

static u64 macos_abi64_unimplemented_stub(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)pid;
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    (void)rip;

    return MACOS_ABI64_ERROR_RETURN(MACOS_ABI64_ENOSYS);
}

static u32 macos_abi64_normalize_syscall(u32 syscall_number)
{
    if ((syscall_number & MACOS_ABI64_BSD_CLASS_MASK)
        == (MACOS_ABI64_BSD_CLASS_UNIX << MACOS_ABI64_BSD_CLASS_SHIFT))
    {
        return syscall_number & MACOS_ABI64_BSD_NUMBER_MASK;
    }

    return syscall_number;
}

static u32 macos_abi64_range_overflows(u64 address, u64 byte_count)
{
    return ((byte_count != 0ull) && ((address + byte_count) < address)) ? 1u : 0u;
}

static u32 macos_abi64_user_buffer_readable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }

    if ((address == 0ull)
        || (macos_abi64_range_overflows(address, (u64)byte_count) != 0u))
    {
        return 0u;
    }

    cursor = address;
    end = address + (u64)byte_count;
    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 page = cursor & ~((u64)VMA64_PAGE_BYTES - 1ull);
        u64 next_page = page + (u64)VMA64_PAGE_BYTES;
        u64 next = (next_page < end) ? next_page : end;

        if ((region == 0)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end)
            || ((region->prot_flags & VMA64_PROT_READ) == 0u)
            || (paging64_user_page_present(page) == 0u)
            || ((paging64_user_page_protection(page) & PAGING64_USER_PROT_READ) == 0u))
        {
            return 0u;
        }

        if (next > region->virt_end)
        {
            next = region->virt_end;
        }
        if (next <= cursor)
        {
            return 0u;
        }
        cursor = next;
    }

    return 1u;
}

static u32 macos_abi64_user_buffer_writable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }

    if ((address == 0ull)
        || (macos_abi64_range_overflows(address, (u64)byte_count) != 0u))
    {
        return 0u;
    }

    cursor = address;
    end = address + (u64)byte_count;
    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 page = cursor & ~((u64)VMA64_PAGE_BYTES - 1ull);
        u64 next_page = page + (u64)VMA64_PAGE_BYTES;
        u64 next = (next_page < end) ? next_page : end;

        if ((region == 0)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end)
            || ((region->prot_flags & VMA64_PROT_WRITE) == 0u)
            || (paging64_user_page_present(page) == 0u)
            || ((paging64_user_page_protection(page) & PAGING64_USER_PROT_WRITE) == 0u))
        {
            return 0u;
        }

        if (next > region->virt_end)
        {
            next = region->virt_end;
        }
        if (next <= cursor)
        {
            return 0u;
        }
        cursor = next;
    }

    return 1u;
}

static void macos_abi64_copy_to_user(u64 user_buffer, const u8 *source, u32 byte_count)
{
    u32 index;

    if ((user_buffer == 0ull) || (source == 0))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        ((volatile u8 *)(u64)user_buffer)[index] = source[index];
    }
}

static void macos_abi64_copy_from_user(u8 *target, u64 user_buffer, u32 byte_count)
{
    u32 index;

    if ((target == 0) || (user_buffer == 0ull))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = ((volatile const u8 *)(u64)user_buffer)[index];
    }
}

static u32 macos_abi64_copy_user_path(
    u32 pid,
    u64 user_path,
    u8 *path,
    u32 max_path_bytes,
    u32 *path_byte_count)
{
    u32 index;

    if (path_byte_count != 0)
    {
        *path_byte_count = 0u;
    }

    if ((user_path == 0ull)
        || (path == 0)
        || (path_byte_count == 0)
        || (max_path_bytes == 0u))
    {
        return 0u;
    }

    for (index = 0u; index <= max_path_bytes; ++index)
    {
        u8 byte;

        if (macos_abi64_user_buffer_readable(pid, user_path + (u64)index, 1u) == 0u)
        {
            return 0u;
        }

        byte = *((volatile const u8 *)(u64)(user_path + (u64)index));
        if (byte == 0u)
        {
            *path_byte_count = index;
            return 1u;
        }

        if (index == max_path_bytes)
        {
            return 0u;
        }

        path[index] = byte;
    }

    return 0u;
}

static u64 macos_abi64_page_align_up(u64 value)
{
    u64 mask = (u64)VMA64_PAGE_BYTES - 1ull;

    if (value == 0ull)
    {
        return 0ull;
    }

    if ((value + mask) < value)
    {
        return 0ull;
    }

    return (value + mask) & ~mask;
}

static u32 macos_abi64_mmap_prot_to_vma(u64 prot)
{
    u32 vma_prot = 0u;

    if ((prot & ~((u64)MACOS_ABI64_PROT_READ
            | (u64)MACOS_ABI64_PROT_WRITE
            | (u64)MACOS_ABI64_PROT_EXEC)) != 0ull)
    {
        return 0u;
    }

    if ((prot & (u64)MACOS_ABI64_PROT_READ) != 0ull)
    {
        vma_prot |= VMA64_PROT_READ;
    }
    if ((prot & (u64)MACOS_ABI64_PROT_WRITE) != 0ull)
    {
        vma_prot |= VMA64_PROT_WRITE;
    }
    if ((prot & (u64)MACOS_ABI64_PROT_EXEC) != 0ull)
    {
        vma_prot |= VMA64_PROT_EXECUTE;
    }

    return vma_prot;
}

static u32 macos_abi64_mmap_flags_to_vma(u64 flags)
{
    u32 vma_flags = 0u;

    if ((flags & ~((u64)MACOS_ABI64_MAP_SHARED
            | (u64)MACOS_ABI64_MAP_PRIVATE
            | (u64)MACOS_ABI64_MAP_FIXED
            | (u64)MACOS_ABI64_MAP_ANON)) != 0ull)
    {
        return 0u;
    }

    if (((flags & (u64)MACOS_ABI64_MAP_ANON) == 0ull)
        || (((flags & (u64)MACOS_ABI64_MAP_PRIVATE) == 0ull)
            && ((flags & (u64)MACOS_ABI64_MAP_SHARED) == 0ull)))
    {
        return 0u;
    }

    if ((flags & (u64)MACOS_ABI64_MAP_PRIVATE) != 0ull)
    {
        vma_flags |= VMA64_MAP_PRIVATE;
    }
    if ((flags & (u64)MACOS_ABI64_MAP_SHARED) != 0ull)
    {
        vma_flags |= VMA64_MAP_SHARED;
    }
    if ((flags & (u64)MACOS_ABI64_MAP_FIXED) != 0ull)
    {
        vma_flags |= VMA64_MAP_FIXED;
    }

    return vma_flags | VMA64_MAP_ANONYMOUS;
}

static u32 macos_abi64_open_flags_to_fd(u64 macos_flags, u32 *fd_flags_out)
{
    u32 fd_flags = 0u;

    if (fd_flags_out != 0)
    {
        *fd_flags_out = 0u;
    }

    if ((fd_flags_out == 0)
        || ((macos_flags & ~((u64)MACOS_ABI64_O_ACCMODE
                | (u64)MACOS_ABI64_O_NONBLOCK
                | (u64)MACOS_ABI64_O_CLOEXEC)) != 0ull))
    {
        return 0u;
    }

    if ((macos_flags & (u64)MACOS_ABI64_O_NONBLOCK) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_NONBLOCK;
    }
    if ((macos_flags & (u64)MACOS_ABI64_O_CLOEXEC) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_CLOEXEC;
    }

    *fd_flags_out = fd_flags;
    return 1u;
}

static u32 macos_abi64_valid_persona(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_MACOS_MACHO))
        ? 1u
        : 0u;
}

static void macos_abi64_note_result(u32 syscall_number, u32 result)
{
    g_macos_abi64_last_syscall = syscall_number;
    g_macos_abi64_last_result = result;
}

static u64 macos_abi64_deny(
    u32 pid,
    u32 syscall_number,
    u32 result,
    u64 rip,
    u8 event_type)
{
    ++g_macos_abi64_denial_count;
    macos_abi64_note_result(syscall_number, result);
    (void)persona_audit64_record(pid, event_type, (u16)syscall_number, result, rip);
    return MACOS_ABI64_ERROR_RETURN(result);
}

static u64 macos_abi64_fault(u32 pid, u32 syscall_number, u64 rip)
{
    ++g_macos_abi64_fault_count;
    macos_abi64_note_result(syscall_number, MACOS_ABI64_EFAULT);
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)syscall_number,
        MACOS_ABI64_EFAULT,
        rip);
    return MACOS_ABI64_ERROR_RETURN(MACOS_ABI64_EFAULT);
}

static u64 macos_abi64_ok(u32 pid, u32 syscall_number, u64 rip, u64 value)
{
    macos_abi64_note_result(syscall_number, PERSONA_AUDIT64_RESULT_OK);
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return value;
}

static void macos_abi64_zero_stat(macos_abi64_stat_t *stat_buf)
{
    u32 index;

    if (stat_buf == 0)
    {
        return;
    }

    stat_buf->st_dev = 0u;
    stat_buf->st_mode = 0u;
    stat_buf->st_nlink = 0u;
    stat_buf->st_ino = 0ull;
    stat_buf->st_uid = 0u;
    stat_buf->st_gid = 0u;
    stat_buf->st_rdev = 0u;
    stat_buf->st_pad0 = 0u;
    stat_buf->st_atime.tv_sec = 0ull;
    stat_buf->st_atime.tv_nsec = 0ull;
    stat_buf->st_mtime.tv_sec = 0ull;
    stat_buf->st_mtime.tv_nsec = 0ull;
    stat_buf->st_ctime.tv_sec = 0ull;
    stat_buf->st_ctime.tv_nsec = 0ull;
    stat_buf->st_birthtime.tv_sec = 0ull;
    stat_buf->st_birthtime.tv_nsec = 0ull;
    stat_buf->st_size = 0ull;
    stat_buf->st_blocks = 0ull;
    stat_buf->st_blksize = 0u;
    stat_buf->st_flags = 0u;
    stat_buf->st_gen = 0u;
    stat_buf->st_lspare = 0u;
    for (index = 0u; index < 2u; ++index)
    {
        stat_buf->st_qspare[index] = 0ull;
    }
}

static u32 macos_abi64_write_stat_to_user(u32 pid, u64 user_stat, const fd64_stat_t *fd_stat)
{
    macos_abi64_stat_t macos_stat;

    if ((fd_stat == 0)
        || ((u32)sizeof(macos_abi64_stat_t) != MACOS_ABI64_STAT_BYTES)
        || (macos_abi64_user_buffer_writable(pid, user_stat, MACOS_ABI64_STAT_BYTES) == 0u))
    {
        return 0u;
    }

    macos_abi64_zero_stat(&macos_stat);
    macos_stat.st_dev = (u32)(fd_stat->device_id & 0xFFFFFFFFull);
    macos_stat.st_mode = (u16)(fd_stat->mode & 0xFFFFu);
    macos_stat.st_nlink = (u16)((fd_stat->link_count != 0u) ? fd_stat->link_count : 1u);
    macos_stat.st_ino = (fd_stat->inode != 0ull) ? fd_stat->inode : (u64)fd_stat->capability_handle;
    macos_stat.st_uid = fd_stat->owner_id;
    macos_stat.st_gid = 0u;
    macos_stat.st_size = fd_stat->size;
    macos_stat.st_blocks = fd_stat->blocks;
    macos_stat.st_blksize = (fd_stat->block_size != 0u) ? fd_stat->block_size : VMA64_PAGE_BYTES;
    macos_stat.st_atime.tv_sec = fd_stat->mtime;
    macos_stat.st_mtime.tv_sec = fd_stat->mtime;
    macos_stat.st_ctime.tv_sec = fd_stat->mtime;
    macos_abi64_copy_to_user(user_stat, (const u8 *)&macos_stat, MACOS_ABI64_STAT_BYTES);
    return 1u;
}

static u64 macos_abi64_sys_read(u32 pid, u64 fd_number, u64 user_buffer, u64 byte_count, u64 rip)
{
    static u8 read_scratch[MACOS_ABI64_READ_CHUNK_BYTES];
    u32 read_count;
    u32 bytes_read;
    u32 fd_index;
    u32 fd_type;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_READ,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_READ,
            MACOS_ABI64_EBADF,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    if (byte_count == 0ull)
    {
        return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_READ, rip, 0ull);
    }

    read_count = (byte_count > (u64)MACOS_ABI64_READ_CHUNK_BYTES)
        ? MACOS_ABI64_READ_CHUNK_BYTES
        : (u32)byte_count;
    if (macos_abi64_user_buffer_writable(pid, user_buffer, read_count) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_READ, rip);
    }

    fd_index = (u32)fd_number;
    fd_type = fd64_entry_type(pid, fd_index);
    if (fd_type == FD64_TYPE_DEVICE)
    {
        bytes_read = input64_read_kernel(
            fd64_entry_capability(pid, fd_index),
            read_scratch,
            read_count,
            process64_principal(pid));
        if (bytes_read == INPUT64_INVALID_RESULT)
        {
            return macos_abi64_deny(
                pid,
                MACOS_ABI64_SYSCALL_READ,
                MACOS_ABI64_EBADF,
                rip,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
        }
    }
    else
    {
        bytes_read = fd64_read(pid, fd_index, read_scratch, read_count);
        if ((bytes_read == FD64_IO_ERROR) || (bytes_read == FD64_IO_BLOCKED))
        {
            return macos_abi64_deny(
                pid,
                MACOS_ABI64_SYSCALL_READ,
                MACOS_ABI64_EBADF,
                rip,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
        }
    }

    macos_abi64_copy_to_user(user_buffer, read_scratch, bytes_read);
    ++g_macos_abi64_read_count;
    g_macos_abi64_read_byte_count += bytes_read;
    g_macos_abi64_last_fd = fd_index;
    g_macos_abi64_last_byte_count = bytes_read;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_READ, rip, (u64)bytes_read);
}

static u64 macos_abi64_sys_write(u32 pid, u64 fd_number, u64 user_buffer, u64 byte_count, u64 rip)
{
    u32 write_count;
    u32 bytes_written;
    u32 fd_index;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_WRITE,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_WRITE,
            MACOS_ABI64_EBADF,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    if (byte_count == 0ull)
    {
        return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_WRITE, rip, 0ull);
    }

    write_count = (byte_count > (u64)MACOS_ABI64_WRITE_CHUNK_BYTES)
        ? MACOS_ABI64_WRITE_CHUNK_BYTES
        : (u32)byte_count;
    if (macos_abi64_user_buffer_readable(pid, user_buffer, write_count) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_WRITE, rip);
    }

    fd_index = (u32)fd_number;
    bytes_written = fd64_write(pid, fd_index, (const u8 *)(u64)user_buffer, write_count);
    if ((bytes_written == FD64_IO_ERROR) || (bytes_written == FD64_IO_BLOCKED))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_WRITE,
            MACOS_ABI64_EBADF,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    ++g_macos_abi64_write_count;
    g_macos_abi64_write_byte_count += bytes_written;
    g_macos_abi64_last_fd = fd_index;
    g_macos_abi64_last_byte_count = bytes_written;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_WRITE, rip, (u64)bytes_written);
}

static u64 macos_abi64_sys_open(
    u32 pid,
    u64 user_path,
    u64 macos_flags,
    u64 mode,
    u64 rip)
{
    u8 path[MACOS_ABI64_MAX_PATH_BYTES + 1u];
    u32 path_byte_count;
    u32 fd_flags;
    u32 fd_number;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_OPEN,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (macos_abi64_open_flags_to_fd(macos_flags, &fd_flags) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_OPEN,
            MACOS_ABI64_EINVAL,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (macos_abi64_copy_user_path(
            pid,
            user_path,
            path,
            MACOS_ABI64_MAX_PATH_BYTES,
            &path_byte_count) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_OPEN, rip);
    }

    (void)mode;
    fd_number = fd64_open_ramfs(pid, path, path_byte_count, fd_flags, 0u);
    if (fd_number == FD64_INVALID_FD)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_OPEN,
            MACOS_ABI64_ENOENT,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_macos_abi64_open_count;
    g_macos_abi64_last_fd = fd_number;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_OPEN, rip, (u64)fd_number);
}

static u64 macos_abi64_sys_close(u32 pid, u64 fd_number, u64 rip)
{
    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_CLOSE,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((fd_number > 0xFFFFFFFFull) || (fd64_close(pid, (u32)fd_number) == 0u))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_CLOSE,
            MACOS_ABI64_EBADF,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    ++g_macos_abi64_close_count;
    g_macos_abi64_last_fd = (u32)fd_number;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_CLOSE, rip, 0ull);
}

static u64 macos_abi64_sys_fstat(u32 pid, u64 fd_number, u64 user_stat, u64 rip)
{
    fd64_stat_t fd_stat;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_FSTAT,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((fd_number > 0xFFFFFFFFull)
        || (fd64_fstat(pid, (u32)fd_number, &fd_stat) == 0u))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_FSTAT,
            MACOS_ABI64_EBADF,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (macos_abi64_write_stat_to_user(pid, user_stat, &fd_stat) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_FSTAT, rip);
    }

    ++g_macos_abi64_fstat_count;
    g_macos_abi64_last_fd = (u32)fd_number;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_FSTAT, rip, 0ull);
}

static u64 macos_abi64_sys_stat(u32 pid, u64 user_path, u64 user_stat, u64 rip)
{
    u8 path[MACOS_ABI64_MAX_PATH_BYTES + 1u];
    u32 path_byte_count;
    u32 fd_number;
    fd64_stat_t fd_stat;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_STAT,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (macos_abi64_copy_user_path(
            pid,
            user_path,
            path,
            MACOS_ABI64_MAX_PATH_BYTES,
            &path_byte_count) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_STAT, rip);
    }

    fd_number = fd64_open_ramfs(pid, path, path_byte_count, 0u, 0u);
    if (fd_number == FD64_INVALID_FD)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_STAT,
            MACOS_ABI64_ENOENT,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (fd64_fstat(pid, fd_number, &fd_stat) == 0u)
    {
        (void)fd64_close(pid, fd_number);
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_STAT,
            MACOS_ABI64_EBADF,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    (void)fd64_close(pid, fd_number);
    if (macos_abi64_write_stat_to_user(pid, user_stat, &fd_stat) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_STAT, rip);
    }

    ++g_macos_abi64_stat_count;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_STAT, rip, 0ull);
}

static u64 macos_abi64_sys_mmap(
    u32 pid,
    u64 hint_address,
    u64 length,
    u64 prot,
    u64 flags,
    u64 fd_number,
    u64 offset,
    u64 rip)
{
    u64 rounded_length;
    u64 mapped_address;
    u32 vma_prot;
    u32 vma_flags;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_MMAP,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    rounded_length = macos_abi64_page_align_up(length);
    vma_prot = macos_abi64_mmap_prot_to_vma(prot);
    vma_flags = macos_abi64_mmap_flags_to_vma(flags);
    if ((rounded_length == 0ull)
        || (vma_prot == 0u)
        || (vma_flags == 0u)
        || (offset != 0ull)
        || (((flags & (u64)MACOS_ABI64_MAP_ANON) != 0ull)
            && (fd_number != 0xFFFFFFFFFFFFFFFFull))
        || (((vma_flags & VMA64_MAP_FIXED) != 0u)
            && ((hint_address == 0ull)
                || ((hint_address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_MMAP,
            MACOS_ABI64_EINVAL,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    mapped_address = vma64_map_anon(pid, hint_address, rounded_length, vma_prot, vma_flags);
    if (mapped_address == 0ull)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_MMAP,
            MACOS_ABI64_ENOMEM,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_macos_abi64_mmap_count;
    g_macos_abi64_last_address = mapped_address;
    g_macos_abi64_last_byte_count = (u32)rounded_length;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_MMAP, rip, mapped_address);
}

static u64 macos_abi64_sys_mprotect(u32 pid, u64 address, u64 length, u64 prot, u64 rip)
{
    u64 rounded_length;
    u32 vma_prot;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_MPROTECT,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    rounded_length = macos_abi64_page_align_up(length);
    vma_prot = macos_abi64_mmap_prot_to_vma(prot);
    if ((address == 0ull)
        || ((address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (rounded_length == 0ull)
        || ((address + rounded_length) < address)
        || (vma_prot == 0u)
        || (vma64_protect(pid, address, rounded_length, vma_prot) == 0u))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_MPROTECT,
            MACOS_ABI64_EINVAL,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_macos_abi64_mprotect_count;
    g_macos_abi64_last_address = address;
    g_macos_abi64_last_byte_count = (u32)rounded_length;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_MPROTECT, rip, 0ull);
}

static u64 macos_abi64_sys_munmap(u32 pid, u64 address, u64 length, u64 rip)
{
    u64 rounded_length;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_MUNMAP,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    rounded_length = macos_abi64_page_align_up(length);
    if ((address == 0ull)
        || ((address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (rounded_length == 0ull)
        || ((address + rounded_length) < address)
        || (vma64_unmap(pid, address, rounded_length) == 0u))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_MUNMAP,
            MACOS_ABI64_EINVAL,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_macos_abi64_munmap_count;
    g_macos_abi64_last_address = address;
    g_macos_abi64_last_byte_count = (u32)rounded_length;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_MUNMAP, rip, 0ull);
}

static u64 macos_abi64_sys_getpid(u32 pid, u64 rip)
{
    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_GETPID,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    ++g_macos_abi64_getpid_count;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_GETPID, rip, (u64)pid);
}

static u32 macos_abi64_clock_supported(u64 clock_id)
{
    return ((clock_id == (u64)MACOS_ABI64_CLOCK_REALTIME)
        || (clock_id == (u64)MACOS_ABI64_CLOCK_MONOTONIC)
        || (clock_id == (u64)MACOS_ABI64_CLOCK_MONOTONIC_RAW))
        ? 1u
        : 0u;
}

static u64 macos_abi64_sys_clock_gettime(u32 pid, u64 clock_id, u64 user_timespec, u64 rip)
{
    macos_abi64_timespec_t timespec;
    u32 ticks;
    u32 frequency;
    u32 remainder;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_CLOCK_GETTIME,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (macos_abi64_clock_supported(clock_id) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_CLOCK_GETTIME,
            MACOS_ABI64_EINVAL,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (macos_abi64_user_buffer_writable(pid, user_timespec, MACOS_ABI64_TIMESPEC_BYTES) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_CLOCK_GETTIME, rip);
    }

    frequency = pit_get_frequency_hz();
    if (frequency == 0u)
    {
        frequency = 100u;
    }
    ticks = pit_get_ticks();
    remainder = ticks % frequency;
    timespec.tv_sec = (u64)(ticks / frequency);
    timespec.tv_nsec = ((u64)remainder * 1000000000ull) / (u64)frequency;
    macos_abi64_copy_to_user(user_timespec, (const u8 *)&timespec, MACOS_ABI64_TIMESPEC_BYTES);
    ++g_macos_abi64_clock_gettime_count;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_CLOCK_GETTIME, rip, 0ull);
}

static u32 macos_abi64_read_u64(u32 pid, u64 address, u64 *value_out)
{
    if ((value_out == 0)
        || (macos_abi64_user_buffer_readable(pid, address, (u32)sizeof(u64)) == 0u))
    {
        return 0u;
    }

    macos_abi64_copy_from_user((u8 *)value_out, address, (u32)sizeof(u64));
    return 1u;
}

static u32 macos_abi64_write_u64(u32 pid, u64 address, u64 value)
{
    if (macos_abi64_user_buffer_writable(pid, address, (u32)sizeof(u64)) == 0u)
    {
        return 0u;
    }

    macos_abi64_copy_to_user(address, (const u8 *)&value, (u32)sizeof(u64));
    return 1u;
}

static u64 macos_abi64_sys_sysctl(
    u32 pid,
    u64 user_name,
    u64 name_count,
    u64 user_oldp,
    u64 user_oldlenp,
    u64 user_newp,
    u64 newlen,
    u64 rip)
{
    u32 names[2];
    u64 old_capacity;
    u64 needed = 0ull;
    static const u8 ostype[] = {
        (u8)'L', (u8)'i', (u8)'m', (u8)'i', (u8)'t', (u8)'l',
        (u8)'e', (u8)'s', (u8)'s', (u8)'O', (u8)'S', 0u
    };
    u32 pagesize = VMA64_PAGE_BYTES;
    const u8 *source = 0;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_SYSCTL,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((user_newp != 0ull) || (newlen != 0ull))
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_SYSCTL,
            MACOS_ABI64_EPERM,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((name_count != 2ull)
        || (macos_abi64_user_buffer_readable(pid, user_name, 2u * (u32)sizeof(u32)) == 0u)
        || (user_oldlenp == 0ull))
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_SYSCTL, rip);
    }

    macos_abi64_copy_from_user((u8 *)&names[0], user_name, 2u * (u32)sizeof(u32));
    g_macos_abi64_last_sysctl_name0 = names[0];
    g_macos_abi64_last_sysctl_name1 = names[1];

    if ((names[0] == MACOS_ABI64_CTL_KERN) && (names[1] == MACOS_ABI64_KERN_OSTYPE))
    {
        source = ostype;
        needed = (u64)sizeof(ostype);
    }
    else if ((names[0] == MACOS_ABI64_CTL_HW) && (names[1] == MACOS_ABI64_HW_PAGESIZE))
    {
        source = (const u8 *)&pagesize;
        needed = (u64)sizeof(pagesize);
    }
    else
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_SYSCTL,
            MACOS_ABI64_ENOENT,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    if (macos_abi64_read_u64(pid, user_oldlenp, &old_capacity) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_SYSCTL, rip);
    }
    if (macos_abi64_write_u64(pid, user_oldlenp, needed) == 0u)
    {
        return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_SYSCTL, rip);
    }
    if (user_oldp != 0ull)
    {
        if (old_capacity < needed)
        {
            return macos_abi64_deny(
                pid,
                MACOS_ABI64_SYSCALL_SYSCTL,
                MACOS_ABI64_ENOMEM,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }
        if (macos_abi64_user_buffer_writable(pid, user_oldp, (u32)needed) == 0u)
        {
            return macos_abi64_fault(pid, MACOS_ABI64_SYSCALL_SYSCTL, rip);
        }
        macos_abi64_copy_to_user(user_oldp, source, (u32)needed);
    }

    ++g_macos_abi64_sysctl_count;
    g_macos_abi64_last_sysctl_bytes = (u32)needed;
    return macos_abi64_ok(pid, MACOS_ABI64_SYSCALL_SYSCTL, rip, 0ull);
}

static u64 macos_abi64_sys_exit(u32 pid, u64 exit_code, u64 rip)
{
    (void)exit_code;

    if (macos_abi64_valid_persona(pid) == 0u)
    {
        return macos_abi64_deny(
            pid,
            MACOS_ABI64_SYSCALL_EXIT,
            MACOS_ABI64_ESRCH,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    ++g_macos_abi64_exit_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        MACOS_ABI64_SYSCALL_EXIT,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    macos_abi64_note_result(MACOS_ABI64_SYSCALL_EXIT, PERSONA_AUDIT64_RESULT_OK);
    (void)macos_mach64_release_process(pid);
    g_macos_abi64_last_exit_vma_regions = vma64_release_process(pid);
    g_macos_abi64_last_exit_fd_entries = fd64_release_process(pid);
    g_macos_abi64_last_exit_persona_released = persona64_release(pid);
    g_macos_abi64_last_exit_audit_released = persona_audit64_release(pid);
    return 0ull;
}

static u64 macos_abi64_read_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_read(pid, rdi, rsi, rdx, rip);
}

static u64 macos_abi64_write_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_write(pid, rdi, rsi, rdx, rip);
}

static u64 macos_abi64_open_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_open(pid, rdi, rsi, rdx, rip);
}

static u64 macos_abi64_close_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_close(pid, rdi, rip);
}

static u64 macos_abi64_stat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_stat(pid, rdi, rsi, rip);
}

static u64 macos_abi64_fstat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_fstat(pid, rdi, rsi, rip);
}

static u64 macos_abi64_mmap_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    return macos_abi64_sys_mmap(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

static u64 macos_abi64_mprotect_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_mprotect(pid, rdi, rsi, rdx, rip);
}

static u64 macos_abi64_munmap_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_munmap(pid, rdi, rsi, rip);
}

static u64 macos_abi64_getpid_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_getpid(pid, rip);
}

static u64 macos_abi64_clock_gettime_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_clock_gettime(pid, rdi, rsi, rip);
}

static u64 macos_abi64_sysctl_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    return macos_abi64_sys_sysctl(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

static u64 macos_abi64_exit_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_abi64_sys_exit(pid, rdi, rip);
}

void macos_abi64_init(void)
{
    u32 index;

    if (g_macos_abi64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < MACOS_ABI64_SYSCALL_LIMIT; ++index)
    {
        g_macos_abi64_dispatch_table[index] = macos_abi64_unimplemented_stub;
    }

    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_EXIT] = macos_abi64_exit_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_READ] = macos_abi64_read_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_WRITE] = macos_abi64_write_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_OPEN] = macos_abi64_open_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_CLOSE] = macos_abi64_close_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_GETPID] = macos_abi64_getpid_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_MUNMAP] = macos_abi64_munmap_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_MPROTECT] = macos_abi64_mprotect_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_CLOCK_GETTIME] =
        macos_abi64_clock_gettime_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_STAT] = macos_abi64_stat_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_FSTAT] = macos_abi64_fstat_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_MMAP] = macos_abi64_mmap_dispatch;
    g_macos_abi64_dispatch_table[MACOS_ABI64_SYSCALL_SYSCTL] = macos_abi64_sysctl_dispatch;

    g_macos_abi64_initialized = 1u;
}

macos_abi64_handler_t *macos_abi64_dispatch_table(void)
{
    if (g_macos_abi64_initialized == 0u)
    {
        macos_abi64_init();
    }

    return g_macos_abi64_dispatch_table;
}

u64 macos_abi64_dispatch(
    u32 pid,
    u32 syscall_number,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    macos_abi64_handler_t handler;
    u32 normalized_syscall;
    u32 unavailable_result;
    u64 unavailable_return;

    if (g_macos_abi64_initialized == 0u)
    {
        macos_abi64_init();
    }

    if ((syscall_number & 0x80000000u) != 0u)
    {
        return macos_mach64_dispatch(
            pid,
            (s32)syscall_number,
            rdi,
            rsi,
            rdx,
            r10,
            r8,
            r9,
            rip);
    }

    normalized_syscall = macos_abi64_normalize_syscall(syscall_number);
    ++g_macos_abi64_dispatch_count;
    if (normalized_syscall >= MACOS_ABI64_SYSCALL_LIMIT)
    {
        ++g_macos_abi64_unimplemented_count;
        (void)persona64_record_unavailable_syscall(
            pid,
            PERSONA64_TYPE_MACOS_MACHO,
            (u16)(normalized_syscall & 0xFFFFu),
            rip,
            &unavailable_result,
            &unavailable_return);
        macos_abi64_note_result(normalized_syscall, unavailable_result);
        return unavailable_return;
    }

    handler = g_macos_abi64_dispatch_table[normalized_syscall];
    if (handler == macos_abi64_unimplemented_stub)
    {
        ++g_macos_abi64_unimplemented_count;
        (void)persona64_record_unavailable_syscall(
            pid,
            PERSONA64_TYPE_MACOS_MACHO,
            (u16)normalized_syscall,
            rip,
            &unavailable_result,
            &unavailable_return);
        macos_abi64_note_result(normalized_syscall, unavailable_result);
        return unavailable_return;
    }

    return handler(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

u32 macos_abi64_table_size(void)
{
    return MACOS_ABI64_SYSCALL_LIMIT;
}

u32 macos_abi64_unimplemented_entry_count(void)
{
    u32 index;
    u32 count = 0u;

    if (g_macos_abi64_initialized == 0u)
    {
        macos_abi64_init();
    }

    for (index = 0u; index < MACOS_ABI64_SYSCALL_LIMIT; ++index)
    {
        if (g_macos_abi64_dispatch_table[index] == macos_abi64_unimplemented_stub)
        {
            ++count;
        }
    }

    return count;
}

u32 macos_abi64_entry_installed(u32 syscall_number)
{
    u32 normalized_syscall;

    if (g_macos_abi64_initialized == 0u)
    {
        macos_abi64_init();
    }

    normalized_syscall = macos_abi64_normalize_syscall(syscall_number);
    return ((normalized_syscall < MACOS_ABI64_SYSCALL_LIMIT)
        && (g_macos_abi64_dispatch_table[normalized_syscall] != macos_abi64_unimplemented_stub))
        ? 1u
        : 0u;
}

u32 macos_abi64_read_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_READ); }
u32 macos_abi64_write_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_WRITE); }
u32 macos_abi64_open_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_OPEN); }
u32 macos_abi64_close_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_CLOSE); }
u32 macos_abi64_stat_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_STAT); }
u32 macos_abi64_fstat_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_FSTAT); }
u32 macos_abi64_mmap_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_MMAP); }
u32 macos_abi64_munmap_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_MUNMAP); }
u32 macos_abi64_mprotect_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_MPROTECT); }
u32 macos_abi64_exit_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_EXIT); }
u32 macos_abi64_getpid_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_GETPID); }
u32 macos_abi64_clock_gettime_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_CLOCK_GETTIME); }
u32 macos_abi64_sysctl_entry_installed(void) { return macos_abi64_entry_installed(MACOS_ABI64_SYSCALL_SYSCTL); }
u32 macos_abi64_dispatch_count(void) { return g_macos_abi64_dispatch_count; }
u32 macos_abi64_unimplemented_count(void) { return g_macos_abi64_unimplemented_count; }
u32 macos_abi64_read_count(void) { return g_macos_abi64_read_count; }
u32 macos_abi64_read_byte_count(void) { return g_macos_abi64_read_byte_count; }
u32 macos_abi64_write_count(void) { return g_macos_abi64_write_count; }
u32 macos_abi64_write_byte_count(void) { return g_macos_abi64_write_byte_count; }
u32 macos_abi64_open_count(void) { return g_macos_abi64_open_count; }
u32 macos_abi64_close_count(void) { return g_macos_abi64_close_count; }
u32 macos_abi64_stat_count(void) { return g_macos_abi64_stat_count; }
u32 macos_abi64_fstat_count(void) { return g_macos_abi64_fstat_count; }
u32 macos_abi64_mmap_count(void) { return g_macos_abi64_mmap_count; }
u32 macos_abi64_munmap_count(void) { return g_macos_abi64_munmap_count; }
u32 macos_abi64_mprotect_count(void) { return g_macos_abi64_mprotect_count; }
u32 macos_abi64_exit_count(void) { return g_macos_abi64_exit_count; }
u32 macos_abi64_getpid_count(void) { return g_macos_abi64_getpid_count; }
u32 macos_abi64_clock_gettime_count(void) { return g_macos_abi64_clock_gettime_count; }
u32 macos_abi64_sysctl_count(void) { return g_macos_abi64_sysctl_count; }
u32 macos_abi64_denial_count(void) { return g_macos_abi64_denial_count; }
u32 macos_abi64_fault_count(void) { return g_macos_abi64_fault_count; }
u32 macos_abi64_last_syscall(void) { return g_macos_abi64_last_syscall; }
u32 macos_abi64_last_result(void) { return g_macos_abi64_last_result; }
u32 macos_abi64_last_fd(void) { return g_macos_abi64_last_fd; }
u32 macos_abi64_last_byte_count(void) { return g_macos_abi64_last_byte_count; }
u64 macos_abi64_last_address(void) { return g_macos_abi64_last_address; }
u32 macos_abi64_last_sysctl_name0(void) { return g_macos_abi64_last_sysctl_name0; }
u32 macos_abi64_last_sysctl_name1(void) { return g_macos_abi64_last_sysctl_name1; }
u32 macos_abi64_last_sysctl_bytes(void) { return g_macos_abi64_last_sysctl_bytes; }
u32 macos_abi64_last_exit_vma_regions(void) { return g_macos_abi64_last_exit_vma_regions; }
u32 macos_abi64_last_exit_fd_entries(void) { return g_macos_abi64_last_exit_fd_entries; }
u32 macos_abi64_last_exit_persona_released(void) { return g_macos_abi64_last_exit_persona_released; }
u32 macos_abi64_last_exit_audit_released(void) { return g_macos_abi64_last_exit_audit_released; }
