#include "pipe_x64.h"

#include "fd_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "scheduler_x64.h"

/*
 * C.1-C.2 introduce the anonymous pipe object substrate. The implementation
 * integrates with fd_x64.h by installing scoped read/write pipe endpoint
 * handles into the per-process fd table; the scaffold checkpoint proves a
 * fresh 4 KiB circular buffer is created empty, both endpoint fds are present,
 * and closing the fds tears the object down without touching BIOS builds.
 */

static pipe64_buffer_t g_pipe64_objects[PIPE64_MAX_OBJECTS];
static u32 g_pipe64_initialized = 0u;
static u32 g_pipe64_denials = 0u;
static u32 g_pipe64_block_count = 0u;
static u32 g_pipe64_wake_count = 0u;

static void pipe64_zero_bytes(u8 *bytes, u32 byte_count)
{
    u32 index;

    if (bytes == 0)
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static void pipe64_clear_object(pipe64_buffer_t *pipe)
{
    u32 generation;

    if (pipe == 0)
    {
        return;
    }

    if ((pipe->live != 0u) && (pipe->owner_pid != PROCESS64_INVALID_PID))
    {
        (void)persona64_budget_release_pipe(pipe->owner_pid, 1u);
    }

    generation = pipe->generation;
    pipe->live = 0u;
    pipe->owner_pid = PROCESS64_INVALID_PID;
    pipe->owner_id = 0u;
    pipe->read_owner_id = 0u;
    pipe->write_owner_id = 0u;
    pipe->read_grantee_owner_id = 0u;
    pipe->write_grantee_owner_id = 0u;
    pipe->read_ref_count = 0u;
    pipe->write_ref_count = 0u;
    pipe->generation = generation;
    pipe->read_handle = PIPE64_INVALID_HANDLE;
    pipe->write_handle = PIPE64_INVALID_HANDLE;
    pipe->read_fd = FD64_INVALID_FD;
    pipe->write_fd = FD64_INVALID_FD;
    pipe->capacity = PIPE64_BUFFER_BYTES;
    pipe->max_capacity = PIPE64_MAX_BUFFER_BYTES;
    pipe->read_index = 0u;
    pipe->write_index = 0u;
    pipe->byte_count = 0u;
    pipe->byte_count_semaphore = 0u;
    pipe->writer_closed = 0u;
    pipe->reader_closed = 0u;
    pipe->blocked_reader_task_id = SCHEDULER64_INVALID_TASK;
    pipe->blocked_writer_task_id = SCHEDULER64_INVALID_TASK;
    pipe64_zero_bytes(pipe->bytes, PIPE64_BUFFER_BYTES);
}

void pipe64_init(void)
{
    u32 index;

    if (g_pipe64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < PIPE64_MAX_OBJECTS; ++index)
    {
        g_pipe64_objects[index].generation = 1u;
        pipe64_clear_object(&g_pipe64_objects[index]);
    }

    g_pipe64_initialized = 1u;
}

static u32 pipe64_current_task_for_owner(u32 owner_id)
{
#ifndef LIMITLESS_X64_UEFI_KERNEL
    (void)owner_id;
    return SCHEDULER64_INVALID_TASK;
#else
    u32 task_id = scheduler64_runqueue_current_task_id();
    u32 task_pid;

    if ((task_id == SCHEDULER64_INVALID_TASK)
        || (scheduler64_runqueue_task_state(task_id) != SCHEDULER64_TASK_RUNNING))
    {
        return SCHEDULER64_INVALID_TASK;
    }

    task_pid = scheduler64_runqueue_task_pid(task_id);
    if ((task_pid == PROCESS64_INVALID_PID)
        || (process64_principal(task_pid) != owner_id))
    {
        return SCHEDULER64_INVALID_TASK;
    }

    return task_id;
#endif
}

static u32 pipe64_block_current_task(u32 owner_id, u32 *blocked_task_slot)
{
#ifndef LIMITLESS_X64_UEFI_KERNEL
    (void)owner_id;
    (void)blocked_task_slot;
    ++g_pipe64_denials;
    return 0u;
#else
    u32 task_id;

    if ((blocked_task_slot == 0)
        || (*blocked_task_slot != SCHEDULER64_INVALID_TASK))
    {
        ++g_pipe64_denials;
        return 0u;
    }

    task_id = pipe64_current_task_for_owner(owner_id);
    if (task_id == SCHEDULER64_INVALID_TASK)
    {
        ++g_pipe64_denials;
        return 0u;
    }

    *blocked_task_slot = task_id;
    if (scheduler64_runqueue_block_task(task_id) == 0u)
    {
        *blocked_task_slot = SCHEDULER64_INVALID_TASK;
        ++g_pipe64_denials;
        return 0u;
    }

    ++g_pipe64_block_count;
    return 1u;
#endif
}

static u32 pipe64_wake_blocked_task(u32 *blocked_task_slot)
{
#ifndef LIMITLESS_X64_UEFI_KERNEL
    (void)blocked_task_slot;
    return 0u;
#else
    u32 task_id;

    if ((blocked_task_slot == 0)
        || (*blocked_task_slot == SCHEDULER64_INVALID_TASK))
    {
        return 0u;
    }

    task_id = *blocked_task_slot;
    *blocked_task_slot = SCHEDULER64_INVALID_TASK;
    if (scheduler64_runqueue_wake_task(task_id) == 0u)
    {
        ++g_pipe64_denials;
        return 0u;
    }

    ++g_pipe64_wake_count;
    return 1u;
#endif
}

static u32 pipe64_make_handle(u32 index, u32 generation, u32 kind)
{
    if ((index >= PIPE64_MAX_OBJECTS)
        || (generation == 0u)
        || ((kind != PIPE64_HANDLE_KIND_READ) && (kind != PIPE64_HANDLE_KIND_WRITE)))
    {
        return PIPE64_INVALID_HANDLE;
    }

    return PIPE64_HANDLE_BASE
        | ((generation << PIPE64_HANDLE_GENERATION_SHIFT) & PIPE64_HANDLE_GENERATION_MASK)
        | ((index << PIPE64_HANDLE_INDEX_SHIFT) & PIPE64_HANDLE_INDEX_MASK)
        | kind;
}

static u32 pipe64_handle_index(u32 pipe_handle)
{
    return (pipe_handle & PIPE64_HANDLE_INDEX_MASK) >> PIPE64_HANDLE_INDEX_SHIFT;
}

static u32 pipe64_handle_generation(u32 pipe_handle)
{
    return (pipe_handle & PIPE64_HANDLE_GENERATION_MASK) >> PIPE64_HANDLE_GENERATION_SHIFT;
}

static u32 pipe64_endpoint_owner_matches(pipe64_buffer_t *pipe, u32 kind, u32 owner_id)
{
    if ((pipe == 0) || (owner_id == 0u))
    {
        return 0u;
    }

    if (kind == PIPE64_HANDLE_KIND_READ)
    {
        return ((pipe->read_owner_id == owner_id)
            || ((pipe->read_grantee_owner_id != 0u) && (pipe->read_grantee_owner_id == owner_id)))
            ? 1u
            : 0u;
    }

    if (kind == PIPE64_HANDLE_KIND_WRITE)
    {
        return ((pipe->write_owner_id == owner_id)
            || ((pipe->write_grantee_owner_id != 0u) && (pipe->write_grantee_owner_id == owner_id)))
            ? 1u
            : 0u;
    }

    return 0u;
}

u32 pipe64_handle_kind(u32 pipe_handle)
{
    if ((pipe_handle & PIPE64_HANDLE_BASE_MASK) != PIPE64_HANDLE_BASE)
    {
        return PIPE64_INVALID_HANDLE;
    }

    return pipe_handle & PIPE64_HANDLE_KIND_MASK;
}

static pipe64_buffer_t *pipe64_find(u32 pipe_handle, u32 owner_id)
{
    u32 index;
    pipe64_buffer_t *pipe;
    u32 kind;

    pipe64_init();

    if (((pipe_handle & PIPE64_HANDLE_BASE_MASK) != PIPE64_HANDLE_BASE)
        || (owner_id == 0u))
    {
        ++g_pipe64_denials;
        return 0;
    }

    index = pipe64_handle_index(pipe_handle);
    kind = pipe64_handle_kind(pipe_handle);
    if (index >= PIPE64_MAX_OBJECTS)
    {
        ++g_pipe64_denials;
        return 0;
    }

    pipe = &g_pipe64_objects[index];
    if ((pipe->live == 0u)
        || (pipe->generation != pipe64_handle_generation(pipe_handle))
        || ((pipe_handle != pipe->read_handle) && (pipe_handle != pipe->write_handle))
        || (pipe64_endpoint_owner_matches(pipe, kind, owner_id) == 0u))
    {
        ++g_pipe64_denials;
        return 0;
    }

    return pipe;
}

u32 pipe64_create_flags(u32 pid, u32 flags, u32 *read_fd_out, u32 *write_fd_out)
{
    u32 owner_id;
    u32 index;
    pipe64_buffer_t *pipe = 0;
    u32 read_fd;
    u32 write_fd;

    pipe64_init();

    if ((read_fd_out == 0)
        || (write_fd_out == 0)
        || (pid == PROCESS64_INVALID_PID)
        || (fd64_table_for_process(pid) == 0)
        || ((flags & ~(FD64_FLAG_O_CLOEXEC | FD64_FLAG_O_NONBLOCK)) != 0u))
    {
        ++g_pipe64_denials;
        return 0u;
    }

    owner_id = process64_principal(pid);
    if (owner_id == 0u)
    {
        ++g_pipe64_denials;
        return 0u;
    }

    if (persona64_budget_check_pipe(pid, 1u) == 0u)
    {
        ++g_pipe64_denials;
        return 0u;
    }

    for (index = 0u; index < PIPE64_MAX_OBJECTS; ++index)
    {
        if (g_pipe64_objects[index].live == 0u)
        {
            pipe = &g_pipe64_objects[index];
            break;
        }
    }

    if (pipe == 0)
    {
        ++g_pipe64_denials;
        return 0u;
    }

    pipe64_clear_object(pipe);
    ++pipe->generation;
    if (pipe->generation == 0u)
    {
        pipe->generation = 1u;
    }
    pipe->live = 1u;
    pipe->owner_pid = pid;
    pipe->owner_id = owner_id;
    pipe->read_owner_id = owner_id;
    pipe->write_owner_id = owner_id;
    pipe->read_ref_count = 1u;
    pipe->write_ref_count = 1u;
    pipe->read_handle = pipe64_make_handle(index, pipe->generation, PIPE64_HANDLE_KIND_READ);
    pipe->write_handle = pipe64_make_handle(index, pipe->generation, PIPE64_HANDLE_KIND_WRITE);
    if ((pipe->read_handle == PIPE64_INVALID_HANDLE)
        || (pipe->write_handle == PIPE64_INVALID_HANDLE))
    {
        pipe64_clear_object(pipe);
        ++g_pipe64_denials;
        return 0u;
    }

    read_fd = fd64_alloc(pid, pipe->read_handle, FD64_TYPE_PIPE_READ, flags);
    if (read_fd == FD64_INVALID_FD)
    {
        pipe64_clear_object(pipe);
        ++g_pipe64_denials;
        return 0u;
    }

    write_fd = fd64_alloc(pid, pipe->write_handle, FD64_TYPE_PIPE_WRITE, flags);
    if (write_fd == FD64_INVALID_FD)
    {
        (void)fd64_free(pid, read_fd);
        pipe64_clear_object(pipe);
        ++g_pipe64_denials;
        return 0u;
    }

    pipe->read_fd = read_fd;
    pipe->write_fd = write_fd;
    if (persona64_budget_commit_pipe(pid, 1u) == 0u)
    {
        (void)fd64_free(pid, write_fd);
        (void)fd64_free(pid, read_fd);
        pipe64_clear_object(pipe);
        ++g_pipe64_denials;
        return 0u;
    }
    *read_fd_out = read_fd;
    *write_fd_out = write_fd;
    return 1u;
}

u32 pipe64_create(u32 pid, u32 *read_fd_out, u32 *write_fd_out)
{
    return pipe64_create_flags(pid, 0u, read_fd_out, write_fd_out);
}

static u32 pipe64_grant_endpoint_inner(
    u32 source_pid,
    u32 source_fd,
    u32 target_pid,
    u32 requested_target_fd,
    u32 *target_fd_out)
{
    u32 source_owner;
    u32 target_owner;
    u32 pipe_handle;
    u32 fd_type;
    u32 kind;
    u32 target_fd;
    pipe64_buffer_t *pipe;

    pipe64_init();

    if (target_fd_out != 0)
    {
        *target_fd_out = FD64_INVALID_FD;
    }

    if ((target_fd_out == 0)
        || (source_pid == PROCESS64_INVALID_PID)
        || (target_pid == PROCESS64_INVALID_PID)
        || (source_pid == target_pid)
        || (fd64_table_for_process(source_pid) == 0)
        || (fd64_table_for_process(target_pid) == 0)
        || ((requested_target_fd != FD64_INVALID_FD) && (requested_target_fd >= FD64_TABLE_LIMIT)))
    {
        ++g_pipe64_denials;
        return 0u;
    }

    source_owner = process64_principal(source_pid);
    target_owner = process64_principal(target_pid);
    pipe_handle = fd64_entry_capability(source_pid, source_fd);
    fd_type = fd64_entry_type(source_pid, source_fd);
    kind = pipe64_handle_kind(pipe_handle);
    if ((source_owner == 0u)
        || (target_owner == 0u)
        || (pipe_handle == PIPE64_INVALID_HANDLE)
        || (((fd_type == FD64_TYPE_PIPE_READ) && (kind != PIPE64_HANDLE_KIND_READ))
            || ((fd_type == FD64_TYPE_PIPE_WRITE) && (kind != PIPE64_HANDLE_KIND_WRITE)))
        || ((fd_type != FD64_TYPE_PIPE_READ) && (fd_type != FD64_TYPE_PIPE_WRITE)))
    {
        ++g_pipe64_denials;
        return 0u;
    }

    pipe = pipe64_find(pipe_handle, source_owner);
    if (pipe == 0)
    {
        return 0u;
    }

    if (kind == PIPE64_HANDLE_KIND_READ)
    {
        if ((pipe->reader_closed != 0u)
            || ((pipe->read_grantee_owner_id != 0u) && (pipe->read_grantee_owner_id != target_owner)))
        {
            ++g_pipe64_denials;
            return 0u;
        }

        target_fd = (requested_target_fd == FD64_INVALID_FD)
            ? fd64_alloc(
                target_pid,
                pipe_handle,
                FD64_TYPE_PIPE_READ,
                fd64_entry_flags(source_pid, source_fd))
            : fd64_alloc_at(
                target_pid,
                requested_target_fd,
                pipe_handle,
                FD64_TYPE_PIPE_READ,
                fd64_entry_flags(source_pid, source_fd));
        if (target_fd == FD64_INVALID_FD)
        {
            ++g_pipe64_denials;
            return 0u;
        }

        pipe->read_grantee_owner_id = target_owner;
        ++pipe->read_ref_count;
    }
    else
    {
        if ((pipe->writer_closed != 0u)
            || ((pipe->write_grantee_owner_id != 0u) && (pipe->write_grantee_owner_id != target_owner)))
        {
            ++g_pipe64_denials;
            return 0u;
        }

        target_fd = (requested_target_fd == FD64_INVALID_FD)
            ? fd64_alloc(
                target_pid,
                pipe_handle,
                FD64_TYPE_PIPE_WRITE,
                fd64_entry_flags(source_pid, source_fd))
            : fd64_alloc_at(
                target_pid,
                requested_target_fd,
                pipe_handle,
                FD64_TYPE_PIPE_WRITE,
                fd64_entry_flags(source_pid, source_fd));
        if (target_fd == FD64_INVALID_FD)
        {
            ++g_pipe64_denials;
            return 0u;
        }

        pipe->write_grantee_owner_id = target_owner;
        ++pipe->write_ref_count;
    }

    *target_fd_out = target_fd;
    return 1u;
}

u32 pipe64_grant_endpoint(u32 source_pid, u32 source_fd, u32 target_pid, u32 *target_fd_out)
{
    return pipe64_grant_endpoint_inner(
        source_pid,
        source_fd,
        target_pid,
        FD64_INVALID_FD,
        target_fd_out);
}

u32 pipe64_grant_endpoint_at(u32 source_pid, u32 source_fd, u32 target_pid, u32 target_fd)
{
    u32 granted_fd = FD64_INVALID_FD;

    if (pipe64_grant_endpoint_inner(source_pid, source_fd, target_pid, target_fd, &granted_fd) == 0u)
    {
        return 0u;
    }

    return (granted_fd == target_fd) ? 1u : 0u;
}

u32 pipe64_write(u32 pipe_handle, const u8 *input, u32 byte_count, u32 owner_id)
{
    pipe64_buffer_t *pipe;
    u32 bytes_written = 0u;
    u32 spin_count;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if (input == 0)
    {
        ++g_pipe64_denials;
        return PIPE64_IO_ERROR;
    }

    pipe = pipe64_find(pipe_handle, owner_id);
    if ((pipe == 0)
        || (pipe64_handle_kind(pipe_handle) != PIPE64_HANDLE_KIND_WRITE)
        || (pipe->writer_closed != 0u)
        || (pipe->reader_closed != 0u))
    {
        ++g_pipe64_denials;
        return PIPE64_IO_ERROR;
    }

    while (bytes_written < byte_count)
    {
        spin_count = 0u;
        while (pipe->byte_count >= pipe->capacity)
        {
            if (bytes_written != 0u)
            {
                return bytes_written;
            }
#ifdef LIMITLESS_X64_UEFI_KERNEL
            if (pipe64_block_current_task(owner_id, &pipe->blocked_writer_task_id) != 0u)
            {
                return PIPE64_IO_BLOCKED;
            }
#endif
            if ((pipe->reader_closed != 0u) || (spin_count >= PIPE64_SPIN_WAIT_LIMIT))
            {
                return (bytes_written != 0u) ? bytes_written : PIPE64_IO_ERROR;
            }
            ++spin_count;
        }

        pipe->bytes[pipe->write_index] = input[bytes_written];
        pipe->write_index = (pipe->write_index + 1u) % pipe->capacity;
        ++pipe->byte_count;
        pipe->byte_count_semaphore = pipe->byte_count;
        ++bytes_written;
    }

    if (bytes_written != 0u)
    {
        (void)pipe64_wake_blocked_task(&pipe->blocked_reader_task_id);
    }

    return bytes_written;
}

u32 pipe64_read(u32 pipe_handle, u8 *output, u32 byte_count, u32 owner_id)
{
    pipe64_buffer_t *pipe;
    u32 bytes_read = 0u;
    u32 spin_count;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if (output == 0)
    {
        ++g_pipe64_denials;
        return PIPE64_IO_ERROR;
    }

    pipe = pipe64_find(pipe_handle, owner_id);
    if ((pipe == 0)
        || (pipe64_handle_kind(pipe_handle) != PIPE64_HANDLE_KIND_READ)
        || (pipe->reader_closed != 0u))
    {
        ++g_pipe64_denials;
        return PIPE64_IO_ERROR;
    }

    while (bytes_read < byte_count)
    {
        spin_count = 0u;
        while (pipe->byte_count == 0u)
        {
            if (pipe->writer_closed != 0u)
            {
                return bytes_read;
            }

            if (bytes_read != 0u)
            {
                return bytes_read;
            }
#ifdef LIMITLESS_X64_UEFI_KERNEL
            if (pipe64_block_current_task(owner_id, &pipe->blocked_reader_task_id) != 0u)
            {
                return PIPE64_IO_BLOCKED;
            }
#endif
            if (spin_count >= PIPE64_SPIN_WAIT_LIMIT)
            {
                return (bytes_read != 0u) ? bytes_read : PIPE64_IO_ERROR;
            }
            ++spin_count;
        }

        output[bytes_read] = pipe->bytes[pipe->read_index];
        pipe->read_index = (pipe->read_index + 1u) % pipe->capacity;
        --pipe->byte_count;
        pipe->byte_count_semaphore = pipe->byte_count;
        ++bytes_read;
    }

    if ((bytes_read != 0u) && (pipe->byte_count < pipe->capacity))
    {
        (void)pipe64_wake_blocked_task(&pipe->blocked_writer_task_id);
    }

    return bytes_read;
}

u32 pipe64_revoke_handle(u32 pipe_handle, u32 owner_id)
{
    pipe64_buffer_t *pipe = pipe64_find(pipe_handle, owner_id);
    u32 kind;

    if (pipe == 0)
    {
        return 0u;
    }

    kind = pipe64_handle_kind(pipe_handle);
    if (kind == PIPE64_HANDLE_KIND_READ)
    {
        if (pipe->read_ref_count != 0u)
        {
            --pipe->read_ref_count;
        }
        if (pipe->read_ref_count == 0u)
        {
            pipe->reader_closed = 1u;
            (void)pipe64_wake_blocked_task(&pipe->blocked_writer_task_id);
        }
    }
    else if (kind == PIPE64_HANDLE_KIND_WRITE)
    {
        if (pipe->write_ref_count != 0u)
        {
            --pipe->write_ref_count;
        }
        if (pipe->write_ref_count == 0u)
        {
            pipe->writer_closed = 1u;
            (void)pipe64_wake_blocked_task(&pipe->blocked_reader_task_id);
        }
    }
    else
    {
        ++g_pipe64_denials;
        return 0u;
    }

    if ((pipe->reader_closed != 0u) && (pipe->writer_closed != 0u))
    {
        pipe64_clear_object(pipe);
    }

    return 1u;
}

u32 pipe64_live_count(void)
{
    u32 index;
    u32 count = 0u;

    pipe64_init();

    for (index = 0u; index < PIPE64_MAX_OBJECTS; ++index)
    {
        if (g_pipe64_objects[index].live != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 pipe64_bytes_available(u32 pipe_handle, u32 owner_id)
{
    pipe64_buffer_t *pipe = pipe64_find(pipe_handle, owner_id);

    return (pipe != 0) ? pipe->byte_count : PIPE64_INVALID_HANDLE;
}

u32 pipe64_capacity(u32 pipe_handle, u32 owner_id)
{
    pipe64_buffer_t *pipe = pipe64_find(pipe_handle, owner_id);

    return (pipe != 0) ? pipe->capacity : 0u;
}

u32 pipe64_reader_closed(u32 pipe_handle, u32 owner_id)
{
    pipe64_buffer_t *pipe = pipe64_find(pipe_handle, owner_id);

    return (pipe != 0) ? pipe->reader_closed : 0u;
}

u32 pipe64_writer_closed(u32 pipe_handle, u32 owner_id)
{
    pipe64_buffer_t *pipe = pipe64_find(pipe_handle, owner_id);

    return (pipe != 0) ? pipe->writer_closed : 0u;
}

u32 pipe64_blocked_reader_task(u32 pipe_handle, u32 owner_id)
{
    pipe64_buffer_t *pipe = pipe64_find(pipe_handle, owner_id);

    return (pipe != 0) ? pipe->blocked_reader_task_id : SCHEDULER64_INVALID_TASK;
}

u32 pipe64_blocked_writer_task(u32 pipe_handle, u32 owner_id)
{
    pipe64_buffer_t *pipe = pipe64_find(pipe_handle, owner_id);

    return (pipe != 0) ? pipe->blocked_writer_task_id : SCHEDULER64_INVALID_TASK;
}

u32 pipe64_block_count(void)
{
    pipe64_init();
    return g_pipe64_block_count;
}

u32 pipe64_wake_count(void)
{
    pipe64_init();
    return g_pipe64_wake_count;
}

u32 pipe64_denial_count(void)
{
    pipe64_init();
    return g_pipe64_denials;
}
