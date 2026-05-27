#include "windows_abi_x64.h"

#include "capability_x64.h"
#include "console_x64.h"
#include "input_x64.h"
#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"
#include "scheduler_x64.h"
#include "services.h"
#include "vma_x64.h"
#include "windows_handle_x64.h"
#include "windows_registry_x64.h"
#include "windows_vfs_x64.h"

/*
 * K.1 adds the first Windows NT ABI switchboard: a 512-entry SSN-indexed
 * dispatch table with the Windows x64 syscall argument shape. K.2 installs
 * NtWriteFile for the stdout/stderr standard-handle bridge. K.3 adds
 * NtReadFile for the stdin standard-handle bridge. K.5 adds NtCreateFile for
 * the first narrow Windows shim-path open. K.6 adds NtAllocateVirtualMemory as
 * a current-process VMA-backed anonymous allocation wrapper. K.7 adds
 * NtFreeVirtualMemory for exact VMA-backed MEM_RELEASE teardown, K.8 adds
 * NtProtectVirtualMemory for exact VMA-backed page-permission changes, K.9
 * adds kernel event-object syscalls, K.10 adds mutant mutex objects backed by
 * the scoped NT handle table, and the wait path now parks unsignaled event and
 * mutant waits on scheduler BLOCKED tasks. K.10c adds relative timeout waits
 * over the scheduler sleep queue. K.11 adds NtQueryInformationProcess for the
 * current-process pseudo handle. K.12 adds NtQuerySystemInformation for a
 * minimal, truthful system-information surface derived from boot memory and
 * current VMA accounting, and K.17 adds NtClose over the scoped handle table
 * for real CloseHandle teardown. It
 * integrates with persona_audit_x64.h for truthful NTSTATUS audit records,
 * vma_x64.h/paging_x64.h for user-buffer validation, and
 * console_x64.h/input_x64.h/capability_x64.h/windows_vfs_x64.h so reads,
 * writes, and file opens still flow through scoped LimitlessOS broker
 * capabilities. The scaffold checkpoints prove table initialization,
 * not-implemented and invalid-SSN denials, real brokered read/write paths,
 * IO_STATUS_BLOCK output, bad-handle/path denial, and fault reporting.
 */

#define WINDOWS_ABI64_DEFAULT_TICK_HZ 100u
#define WINDOWS_ABI64_MAX_PIT_TICK_HZ 1193182u
#define WINDOWS_ABI64_WAIT_TIMEOUT_RECORDS 4u
#define WINDOWS_ABI64_100NS_PER_SECOND 10000000ull
#define WINDOWS_ABI64_FILE_POSITION_RECORDS 16u

typedef struct windows_abi64_wait_timeout_record
{
    u32 active;
    u32 pid;
    u32 task_id;
    u32 object_type;
    u32 timeout_ticks;
    u32 result;
    u64 handle;
    u64 rip;
} windows_abi64_wait_timeout_record_t;

typedef struct windows_abi64_file_position_record
{
    u32 active;
    u32 pid;
    u64 handle;
    u64 position;
} windows_abi64_file_position_record_t;

static u32 g_windows_abi64_initialized = 0u;
static windows_abi64_handler_t g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_LIMIT];
static windows_abi64_wait_timeout_record_t
    g_windows_abi64_wait_timeout_records[WINDOWS_ABI64_WAIT_TIMEOUT_RECORDS];
static windows_abi64_file_position_record_t
    g_windows_abi64_file_position_records[WINDOWS_ABI64_FILE_POSITION_RECORDS];
static u32 g_windows_abi64_dispatch_count = 0u;
static u32 g_windows_abi64_unimplemented_count = 0u;
static u32 g_windows_abi64_invalid_service_count = 0u;
static u32 g_windows_abi64_last_pid = PROCESS64_INVALID_PID;
static u32 g_windows_abi64_last_syscall = 0u;
static u32 g_windows_abi64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u64 g_windows_abi64_last_rip = 0ull;
static u32 g_windows_abi64_read_count = 0u;
static u32 g_windows_abi64_read_byte_count = 0u;
static u32 g_windows_abi64_read_denial_count = 0u;
static u32 g_windows_abi64_read_fault_count = 0u;
static u32 g_windows_abi64_read_last_handle_low = 0u;
static u32 g_windows_abi64_read_last_byte_count = 0u;
static u32 g_windows_abi64_read_last_capability = CAPABILITY64_INVALID_HANDLE;
static u32 g_windows_abi64_read_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_write_count = 0u;
static u32 g_windows_abi64_write_byte_count = 0u;
static u32 g_windows_abi64_write_denial_count = 0u;
static u32 g_windows_abi64_write_fault_count = 0u;
static u32 g_windows_abi64_write_last_handle_low = 0u;
static u32 g_windows_abi64_write_last_byte_count = 0u;
static u32 g_windows_abi64_write_last_capability = CAPABILITY64_INVALID_HANDLE;
static u32 g_windows_abi64_write_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_allocate_count = 0u;
static u32 g_windows_abi64_allocate_denial_count = 0u;
static u32 g_windows_abi64_allocate_fault_count = 0u;
static u32 g_windows_abi64_allocate_byte_count = 0u;
static u64 g_windows_abi64_allocate_last_base = 0ull;
static u64 g_windows_abi64_allocate_last_size = 0ull;
static u32 g_windows_abi64_allocate_last_protect = 0u;
static u32 g_windows_abi64_allocate_last_type = 0u;
static u32 g_windows_abi64_allocate_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_free_count = 0u;
static u32 g_windows_abi64_free_denial_count = 0u;
static u32 g_windows_abi64_free_fault_count = 0u;
static u32 g_windows_abi64_free_byte_count = 0u;
static u64 g_windows_abi64_free_last_base = 0ull;
static u64 g_windows_abi64_free_last_size = 0ull;
static u32 g_windows_abi64_free_last_type = 0u;
static u32 g_windows_abi64_free_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_protect_count = 0u;
static u32 g_windows_abi64_protect_denial_count = 0u;
static u32 g_windows_abi64_protect_fault_count = 0u;
static u32 g_windows_abi64_protect_byte_count = 0u;
static u64 g_windows_abi64_protect_last_base = 0ull;
static u64 g_windows_abi64_protect_last_size = 0ull;
static u32 g_windows_abi64_protect_last_new_protect = 0u;
static u32 g_windows_abi64_protect_last_old_protect = 0u;
static u32 g_windows_abi64_protect_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_create_count = 0u;
static u32 g_windows_abi64_create_denial_count = 0u;
static u32 g_windows_abi64_create_fault_count = 0u;
static u32 g_windows_abi64_create_last_handle_low = 0u;
static u32 g_windows_abi64_create_last_capability = CAPABILITY64_INVALID_HANDLE;
static u32 g_windows_abi64_create_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_create_last_path_hash = 0u;
static u32 g_windows_abi64_create_last_path_bytes = 0u;
static u32 g_windows_abi64_create_last_shim_id = 0u;
static u32 g_windows_abi64_query_file_count = 0u;
static u32 g_windows_abi64_query_file_denial_count = 0u;
static u32 g_windows_abi64_query_file_fault_count = 0u;
static u32 g_windows_abi64_query_file_last_class = 0u;
static u32 g_windows_abi64_query_file_last_handle_low = 0u;
static u32 g_windows_abi64_query_file_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_query_file_last_return_length = 0u;
static u32 g_windows_abi64_set_file_count = 0u;
static u32 g_windows_abi64_set_file_denial_count = 0u;
static u32 g_windows_abi64_set_file_fault_count = 0u;
static u32 g_windows_abi64_set_file_last_class = 0u;
static u32 g_windows_abi64_set_file_last_handle_low = 0u;
static u32 g_windows_abi64_set_file_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_close_count = 0u;
static u32 g_windows_abi64_close_denial_count = 0u;
static u32 g_windows_abi64_close_last_handle_low = 0u;
static u32 g_windows_abi64_close_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_terminate_count = 0u;
static u32 g_windows_abi64_terminate_denial_count = 0u;
static u32 g_windows_abi64_terminate_last_pid = PROCESS64_INVALID_PID;
static u32 g_windows_abi64_terminate_last_status = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_terminate_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_event_create_count = 0u;
static u32 g_windows_abi64_event_set_count = 0u;
static u32 g_windows_abi64_event_wait_count = 0u;
static u32 g_windows_abi64_event_denial_count = 0u;
static u32 g_windows_abi64_event_fault_count = 0u;
static u32 g_windows_abi64_event_last_handle_low = 0u;
static u32 g_windows_abi64_event_last_previous_state = 0u;
static u32 g_windows_abi64_event_last_state = 0u;
static u32 g_windows_abi64_event_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_mutant_create_count = 0u;
static u32 g_windows_abi64_mutant_wait_count = 0u;
static u32 g_windows_abi64_mutant_release_count = 0u;
static u32 g_windows_abi64_mutant_denial_count = 0u;
static u32 g_windows_abi64_mutant_fault_count = 0u;
static u32 g_windows_abi64_mutant_last_handle_low = 0u;
static u32 g_windows_abi64_mutant_last_previous_count = 0u;
static u32 g_windows_abi64_mutant_last_owner = PROCESS64_INVALID_PID;
static u32 g_windows_abi64_mutant_last_count = 0u;
static u32 g_windows_abi64_mutant_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_wait_timed_count = 0u;
static u32 g_windows_abi64_wait_timeout_count = 0u;
static u32 g_windows_abi64_wait_timeout_denial_count = 0u;
static u32 g_windows_abi64_wait_last_timeout_task = SCHEDULER64_INVALID_TASK;
static u32 g_windows_abi64_wait_last_timeout_ticks = 0u;
static u32 g_windows_abi64_wait_last_timeout_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_wait_last_timeout_handle_low = 0u;
static u32 g_windows_abi64_query_process_count = 0u;
static u32 g_windows_abi64_query_process_denial_count = 0u;
static u32 g_windows_abi64_query_process_fault_count = 0u;
static u32 g_windows_abi64_query_process_last_class = 0u;
static u32 g_windows_abi64_query_process_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u64 g_windows_abi64_query_process_last_peb = 0ull;
static u32 g_windows_abi64_query_process_last_return_length = 0u;
static u32 g_windows_abi64_query_system_count = 0u;
static u32 g_windows_abi64_query_system_denial_count = 0u;
static u32 g_windows_abi64_query_system_fault_count = 0u;
static u32 g_windows_abi64_query_system_last_class = 0u;
static u32 g_windows_abi64_query_system_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_query_system_last_return_length = 0u;
static u32 g_windows_abi64_query_system_last_page_size = VMA64_PAGE_BYTES;
static u32 g_windows_abi64_query_system_last_processor_count = 1u;
static u32 g_windows_abi64_query_system_last_physical_pages = 0u;
static u32 g_windows_abi64_query_system_last_free_pages = 0u;
static u32 g_windows_abi64_registry_open_count = 0u;
static u32 g_windows_abi64_registry_create_count = 0u;
static u32 g_windows_abi64_registry_query_count = 0u;
static u32 g_windows_abi64_registry_denial_count = 0u;
static u32 g_windows_abi64_registry_fault_count = 0u;
static u32 g_windows_abi64_registry_last_syscall = 0u;
static u32 g_windows_abi64_registry_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
static u32 g_windows_abi64_registry_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;
static u32 g_windows_abi64_registry_last_required_bytes = 0u;
static u32 g_windows_abi64_registry_last_value_type = 0u;
static u32 g_windows_abi64_system_processor_count = 1u;
static u32 g_windows_abi64_system_processor_id = 0u;
static u64 g_windows_abi64_system_physical_pages = 0ull;

static u32 windows_abi64_range_overflows(u64 address, u64 byte_count)
{
    return ((byte_count != 0ull) && ((address + byte_count) < address)) ? 1u : 0u;
}

static u32 windows_abi64_user_buffer_readable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }
    if ((address == 0ull)
        || (windows_abi64_range_overflows(address, (u64)byte_count) != 0u))
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

static u32 windows_abi64_user_buffer_writable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }
    if ((address == 0ull)
        || (windows_abi64_range_overflows(address, (u64)byte_count) != 0u))
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

static u32 windows_abi64_effective_tick_frequency(void)
{
    u32 frequency = pit_get_frequency_hz();

    if ((frequency == 0u) || (frequency > WINDOWS_ABI64_MAX_PIT_TICK_HZ))
    {
        return WINDOWS_ABI64_DEFAULT_TICK_HZ;
    }

    return frequency;
}

static void windows_abi64_clear_wait_timeout_record(
    windows_abi64_wait_timeout_record_t *record)
{
    if (record == 0)
    {
        return;
    }

    record->active = 0u;
    record->pid = PROCESS64_INVALID_PID;
    record->task_id = SCHEDULER64_INVALID_TASK;
    record->object_type = WINDOWS_HANDLE64_TYPE_EMPTY;
    record->timeout_ticks = 0u;
    record->result = WINDOWS_ABI64_STATUS_SUCCESS;
    record->handle = WINDOWS_HANDLE64_INVALID;
    record->rip = 0ull;
}

static windows_abi64_wait_timeout_record_t *windows_abi64_wait_timeout_free_record(void)
{
    u32 index;

    for (index = 0u; index < WINDOWS_ABI64_WAIT_TIMEOUT_RECORDS; ++index)
    {
        if (g_windows_abi64_wait_timeout_records[index].active == 0u)
        {
            return &g_windows_abi64_wait_timeout_records[index];
        }
    }

    return 0;
}

static windows_abi64_wait_timeout_record_t *windows_abi64_wait_timeout_for_task(
    u32 task_id)
{
    u32 index;

    for (index = 0u; index < WINDOWS_ABI64_WAIT_TIMEOUT_RECORDS; ++index)
    {
        if ((g_windows_abi64_wait_timeout_records[index].active != 0u)
            && (g_windows_abi64_wait_timeout_records[index].task_id == task_id))
        {
            return &g_windows_abi64_wait_timeout_records[index];
        }
    }

    return 0;
}

static void windows_abi64_clear_file_position_record(
    windows_abi64_file_position_record_t *record)
{
    if (record == 0)
    {
        return;
    }

    record->active = 0u;
    record->pid = PROCESS64_INVALID_PID;
    record->handle = WINDOWS_HANDLE64_INVALID;
    record->position = 0ull;
}

static windows_abi64_file_position_record_t *windows_abi64_file_position_record_for(
    u32 pid,
    u64 handle,
    u32 create_if_missing)
{
    windows_abi64_file_position_record_t *free_record = 0;
    u32 index;

    for (index = 0u; index < WINDOWS_ABI64_FILE_POSITION_RECORDS; ++index)
    {
        windows_abi64_file_position_record_t *record =
            &g_windows_abi64_file_position_records[index];

        if ((record->active != 0u)
            && (record->pid == pid)
            && (record->handle == handle))
        {
            return record;
        }
        if ((free_record == 0) && (record->active == 0u))
        {
            free_record = record;
        }
    }

    if ((create_if_missing == 0u) || (free_record == 0))
    {
        return 0;
    }

    free_record->active = 1u;
    free_record->pid = pid;
    free_record->handle = handle;
    free_record->position = 0ull;
    return free_record;
}

static void windows_abi64_file_position_record_clear_for(u32 pid, u64 handle)
{
    u32 index;

    for (index = 0u; index < WINDOWS_ABI64_FILE_POSITION_RECORDS; ++index)
    {
        windows_abi64_file_position_record_t *record =
            &g_windows_abi64_file_position_records[index];

        if ((record->active != 0u)
            && (record->pid == pid)
            && (record->handle == handle))
        {
            windows_abi64_clear_file_position_record(record);
        }
    }
}

static u32 windows_abi64_is_standard_file_handle(u64 handle)
{
    return ((handle == WINDOWS_ABI64_STDIN_HANDLE)
        || (handle == WINDOWS_ABI64_STDOUT_HANDLE)
        || (handle == WINDOWS_ABI64_STDERR_HANDLE))
        ? 1u
        : 0u;
}

static u32 windows_abi64_file_handle_is_scoped(u32 pid, u64 handle)
{
    if (windows_abi64_is_standard_file_handle(handle) != 0u)
    {
        return 1u;
    }

    return ((windows_handle64_entry_type(pid, handle) == WINDOWS_HANDLE64_TYPE_FILE)
        && (windows_handle64_entry_capability(pid, handle)
            != CAPABILITY64_INVALID_HANDLE))
        ? 1u
        : 0u;
}

static u32 windows_abi64_relative_timeout_to_ticks(
    u64 timeout_value,
    u32 *ticks_out,
    u32 *status_out)
{
    u64 magnitude;
    u64 maximum_magnitude;
    u64 ticks64;
    u32 frequency;

    if ((ticks_out == 0) || (status_out == 0))
    {
        return 0u;
    }

    *ticks_out = 0u;
    *status_out = WINDOWS_ABI64_STATUS_SUCCESS;
    if (timeout_value == 0ull)
    {
        return 1u;
    }
    if ((timeout_value & 0x8000000000000000ull) == 0ull)
    {
        *status_out = WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED;
        return 0u;
    }

    frequency = windows_abi64_effective_tick_frequency();
    magnitude = 0ull - timeout_value;
    maximum_magnitude =
        (((u64)0xFFFFFFFFu) * WINDOWS_ABI64_100NS_PER_SECOND) / (u64)frequency;
    if ((magnitude == 0ull) || (magnitude > maximum_magnitude))
    {
        *status_out = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        return 0u;
    }

    ticks64 =
        ((magnitude * (u64)frequency) + (WINDOWS_ABI64_100NS_PER_SECOND - 1ull))
        / WINDOWS_ABI64_100NS_PER_SECOND;
    if (ticks64 == 0ull)
    {
        ticks64 = 1ull;
    }
    if (ticks64 > 0xFFFFFFFFull)
    {
        *status_out = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        return 0u;
    }

    *ticks_out = (u32)ticks64;
    return 1u;
}

static void windows_abi64_note_wait_timeout(
    u32 task_id,
    u32 object_type,
    u64 handle,
    u32 timeout_ticks,
    u32 result)
{
    ++g_windows_abi64_wait_timeout_count;
    g_windows_abi64_wait_last_timeout_task = task_id;
    g_windows_abi64_wait_last_timeout_ticks = timeout_ticks;
    g_windows_abi64_wait_last_timeout_result = result;
    g_windows_abi64_wait_last_timeout_handle_low = (u32)handle;
    if (object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
    {
        g_windows_abi64_mutant_last_result = result;
    }
    else
    {
        g_windows_abi64_event_last_result = result;
    }
}

static void windows_abi64_wait_timeout_callback(u32 task_id, u64 cookie)
{
    windows_abi64_wait_timeout_record_t *record =
        (windows_abi64_wait_timeout_record_t *)(u64)cookie;

    if ((record == 0)
        || (record->active == 0u)
        || (record->task_id != task_id))
    {
        return;
    }

    if (record->object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
    {
        (void)windows_handle64_mutant_cancel_wait(
            record->pid,
            record->handle,
            task_id);
    }
    else if (record->object_type == WINDOWS_HANDLE64_TYPE_EVENT)
    {
        (void)windows_handle64_event_cancel_wait(
            record->pid,
            record->handle,
            task_id);
    }

    windows_abi64_note_wait_timeout(
        task_id,
        record->object_type,
        record->handle,
        record->timeout_ticks,
        record->result);
    (void)persona_audit64_record(
        record->pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
        record->result,
        record->rip);
    windows_abi64_clear_wait_timeout_record(record);
}

static void windows_abi64_wait_cancel_timeout_task(u32 task_id)
{
    windows_abi64_wait_timeout_record_t *record =
        windows_abi64_wait_timeout_for_task(task_id);

    if (record != 0)
    {
        windows_abi64_clear_wait_timeout_record(record);
    }
    (void)scheduler64_sleep_cancel_task(task_id);
}

static u32 windows_abi64_wait_wake_task_success(u32 task_id)
{
    windows_abi64_wait_cancel_timeout_task(task_id);
    return scheduler64_runqueue_wake_task_with_result(
        task_id,
        WINDOWS_ABI64_STATUS_SUCCESS);
}

static u32 windows_abi64_wait_arm_object(
    u32 pid,
    u32 object_type,
    u64 handle,
    u32 task_id)
{
    if (object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
    {
        return windows_handle64_mutant_arm_wait(pid, handle, task_id);
    }
    if (object_type == WINDOWS_HANDLE64_TYPE_EVENT)
    {
        return windows_handle64_event_arm_wait(pid, handle, task_id);
    }

    return 0u;
}

static void windows_abi64_wait_cancel_object(
    u32 pid,
    u32 object_type,
    u64 handle,
    u32 task_id)
{
    if (object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
    {
        (void)windows_handle64_mutant_cancel_wait(pid, handle, task_id);
    }
    else if (object_type == WINDOWS_HANDLE64_TYPE_EVENT)
    {
        (void)windows_handle64_event_cancel_wait(pid, handle, task_id);
    }
}

static u32 windows_abi64_wait_block_current(
    u32 pid,
    u32 object_type,
    u64 handle,
    u64 timeout_ptr,
    u64 timeout_value,
    u64 rip,
    u32 *task_id_out,
    u32 *result_out)
{
    windows_abi64_wait_timeout_record_t *timeout_record = 0;
    u32 timeout_ticks = 0u;
    u32 timeout_status = WINDOWS_ABI64_STATUS_SUCCESS;
    u32 timed_wait = 0u;
    u32 task_id;

    if ((task_id_out == 0) || (result_out == 0))
    {
        return 0u;
    }
    *task_id_out = SCHEDULER64_INVALID_TASK;
    *result_out = WINDOWS_ABI64_STATUS_SUCCESS;

    if ((timeout_ptr != 0ull) && (timeout_value != 0ull))
    {
        if (windows_abi64_relative_timeout_to_ticks(
                timeout_value,
                &timeout_ticks,
                &timeout_status) == 0u)
        {
            ++g_windows_abi64_wait_timeout_denial_count;
            *result_out = timeout_status;
            return 0u;
        }

        timeout_record = windows_abi64_wait_timeout_free_record();
        if (timeout_record == 0)
        {
            ++g_windows_abi64_wait_timeout_denial_count;
            *result_out = WINDOWS_ABI64_STATUS_NO_MEMORY;
            return 0u;
        }
        timed_wait = 1u;
    }

    task_id = scheduler64_runqueue_current_task_id();
    if ((task_id == SCHEDULER64_INVALID_TASK)
        || (scheduler64_runqueue_task_pid(task_id) != pid)
        || (scheduler64_runqueue_task_state(task_id) != SCHEDULER64_TASK_RUNNING)
        || (windows_abi64_wait_arm_object(pid, object_type, handle, task_id) == 0u))
    {
        *result_out = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        return 0u;
    }

    if (timed_wait != 0u)
    {
        timeout_record->active = 1u;
        timeout_record->pid = pid;
        timeout_record->task_id = task_id;
        timeout_record->object_type = object_type;
        timeout_record->timeout_ticks = timeout_ticks;
        timeout_record->result = WINDOWS_ABI64_STATUS_TIMEOUT;
        timeout_record->handle = handle;
        timeout_record->rip = rip;
        if (scheduler64_sleep_current_task_for_ticks(
                timeout_ticks,
                WINDOWS_ABI64_STATUS_TIMEOUT,
                windows_abi64_wait_timeout_callback,
                (u64)timeout_record) == 0u)
        {
            windows_abi64_wait_cancel_object(pid, object_type, handle, task_id);
            windows_abi64_clear_wait_timeout_record(timeout_record);
            ++g_windows_abi64_wait_timeout_denial_count;
            *result_out = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
            return 0u;
        }
        ++g_windows_abi64_wait_timed_count;
    }
    else if (scheduler64_runqueue_block_task(task_id) == 0u)
    {
        windows_abi64_wait_cancel_object(pid, object_type, handle, task_id);
        *result_out = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        return 0u;
    }

    *task_id_out = task_id;
    return 1u;
}

static void windows_abi64_copy_from_user(u8 *target, u64 user_buffer, u32 byte_count)
{
    volatile const u8 *source = (volatile const u8 *)(u64)user_buffer;
    u32 index;

    if ((target == 0) || (byte_count == 0u))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = source[index];
    }
}

static void windows_abi64_copy_to_user(u64 user_buffer, const u8 *source, u32 byte_count)
{
    volatile u8 *target = (volatile u8 *)(u64)user_buffer;
    u32 index;

    if ((source == 0) || (byte_count == 0u))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = source[index];
    }
}

static void windows_abi64_zero_user(u64 user_buffer, u32 byte_count)
{
    volatile u8 *target = (volatile u8 *)(u64)user_buffer;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = 0u;
    }
}

static void windows_abi64_store_u16(u64 user_buffer, u16 value)
{
    volatile u16 *target = (volatile u16 *)(u64)user_buffer;

    *target = value;
}

static void windows_abi64_store_u32(u64 user_buffer, u32 value)
{
    volatile u32 *target = (volatile u32 *)(u64)user_buffer;

    *target = value;
}

static void windows_abi64_store_u64(u64 user_buffer, u64 value)
{
    volatile u64 *target = (volatile u64 *)(u64)user_buffer;

    *target = value;
}

static u16 windows_abi64_load_u16(const u8 *source)
{
    return (u16)((u16)source[0] | ((u16)source[1] << 8));
}

static u32 windows_abi64_load_u32(const u8 *source)
{
    return (u32)source[0]
        | ((u32)source[1] << 8)
        | ((u32)source[2] << 16)
        | ((u32)source[3] << 24);
}

static u64 windows_abi64_load_u64(const u8 *source)
{
    return (u64)windows_abi64_load_u32(source)
        | ((u64)windows_abi64_load_u32(source + 4) << 32);
}

static u64 windows_abi64_load_user_u64(u64 user_buffer)
{
    volatile const u64 *source = (volatile const u64 *)(u64)user_buffer;

    return *source;
}

void windows_abi64_configure_system_information(
    u64 physical_memory_bytes,
    u32 processor_count,
    u32 processor_id)
{
    g_windows_abi64_system_physical_pages =
        physical_memory_bytes / (u64)VMA64_PAGE_BYTES;
    g_windows_abi64_system_processor_count =
        (processor_count != 0u) ? processor_count : 1u;
    g_windows_abi64_system_processor_id = processor_id;
}

static u32 windows_abi64_store_io_status(
    u32 pid,
    u64 io_status_block,
    u32 status,
    u64 information)
{
    if (windows_abi64_user_buffer_writable(
            pid,
            io_status_block,
            WINDOWS_ABI64_IO_STATUS_BLOCK_BYTES) == 0u)
    {
        return 0u;
    }

    windows_abi64_store_u32(io_status_block, status);
    windows_abi64_store_u32(io_status_block + 4ull, 0u);
    windows_abi64_store_u64(io_status_block + 8ull, information);
    return 1u;
}

static u32 windows_abi64_read_unicode_ascii(
    u32 pid,
    u64 unicode_string_ptr,
    u8 *ascii_out,
    u32 ascii_capacity,
    u32 *ascii_bytes_out)
{
    static u8 unicode_string[WINDOWS_ABI64_UNICODE_STRING_BYTES];
    u64 path_buffer;
    u16 path_length;
    u16 path_maximum_length;
    u32 index;
    u32 path_bytes;

    if (ascii_bytes_out != 0)
    {
        *ascii_bytes_out = 0u;
    }
    if ((unicode_string_ptr == 0ull)
        || (ascii_out == 0)
        || (ascii_capacity == 0u)
        || (ascii_bytes_out == 0)
        || (windows_abi64_user_buffer_readable(
            pid,
            unicode_string_ptr,
            WINDOWS_ABI64_UNICODE_STRING_BYTES) == 0u))
    {
        return WINDOWS_ABI64_STATUS_ACCESS_VIOLATION;
    }

    windows_abi64_copy_from_user(
        unicode_string,
        unicode_string_ptr,
        WINDOWS_ABI64_UNICODE_STRING_BYTES);
    path_length = windows_abi64_load_u16(unicode_string);
    path_maximum_length = windows_abi64_load_u16(unicode_string + 2u);
    path_buffer = windows_abi64_load_u64(unicode_string + 8u);
    if ((path_length == 0u)
        || ((path_length & 1u) != 0u)
        || (path_maximum_length < path_length)
        || (path_buffer == 0ull)
        || (((u32)path_length / 2u) > ascii_capacity)
        || (windows_abi64_user_buffer_readable(pid, path_buffer, (u32)path_length) == 0u))
    {
        return WINDOWS_ABI64_STATUS_ACCESS_VIOLATION;
    }

    path_bytes = ((u32)path_length) / 2u;
    for (index = 0u; index < path_bytes; ++index)
    {
        volatile const u8 *source =
            (volatile const u8 *)(u64)(path_buffer + ((u64)index * 2ull));
        u8 low = source[0];
        u8 high = source[1];

        if ((high != 0u) || (low == 0u))
        {
            return WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        }
        ascii_out[index] = low;
    }
    *ascii_bytes_out = path_bytes;
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

static u32 windows_abi64_read_object_name_ascii(
    u32 pid,
    u64 object_attributes_ptr,
    u8 *ascii_out,
    u32 ascii_capacity,
    u32 *ascii_bytes_out,
    u64 *root_directory_out)
{
    static u8 object_attributes[WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES];
    u32 object_length;
    u64 object_name_ptr;
    u64 root_directory;

    if (root_directory_out != 0)
    {
        *root_directory_out = 0ull;
    }
    if ((ascii_bytes_out != 0))
    {
        *ascii_bytes_out = 0u;
    }
    if ((object_attributes_ptr == 0ull)
        || (ascii_out == 0)
        || (ascii_bytes_out == 0)
        || (root_directory_out == 0)
        || (windows_abi64_user_buffer_readable(
            pid,
            object_attributes_ptr,
            WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES) == 0u))
    {
        return WINDOWS_ABI64_STATUS_ACCESS_VIOLATION;
    }

    windows_abi64_copy_from_user(
        object_attributes,
        object_attributes_ptr,
        WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES);
    object_length = windows_abi64_load_u32(object_attributes);
    root_directory = windows_abi64_load_u64(object_attributes + 8u);
    object_name_ptr = windows_abi64_load_u64(object_attributes + 16u);
    if ((object_length < WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES)
        || (object_name_ptr == 0ull))
    {
        return WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
    }

    *root_directory_out = root_directory;
    return windows_abi64_read_unicode_ascii(
        pid,
        object_name_ptr,
        ascii_out,
        ascii_capacity,
        ascii_bytes_out);
}

static u32 windows_abi64_unimplemented_stub(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    (void)pid;
    (void)rcx;
    (void)rdx;
    (void)r8;
    (void)r9;
    (void)stack_args;
    (void)stack_arg_count;
    (void)rip;

    return WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED;
}

static u32 windows_abi64_ntreadfile_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    static u8 read_scratch[WINDOWS_ABI64_READ_CHUNK_BYTES];
    u64 file_handle = rcx;
    u64 io_status_block;
    u64 user_buffer;
    u64 byte_count_arg;
    u32 read_count;
    u32 owner_id;
    u32 input_capability;
    u32 bytes_read;
    u32 result;

    (void)rdx;
    (void)r8;
    (void)r9;

    g_windows_abi64_read_last_handle_low = (u32)file_handle;
    g_windows_abi64_read_last_byte_count = 0u;
    g_windows_abi64_read_last_capability = CAPABILITY64_INVALID_HANDLE;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_read_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_read_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

    if ((stack_args == 0) || (stack_arg_count < 3u))
    {
        ++g_windows_abi64_read_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        g_windows_abi64_read_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

    io_status_block = stack_args[0];
    user_buffer = stack_args[1];
    byte_count_arg = stack_args[2];

    if (io_status_block == 0ull)
    {
        ++g_windows_abi64_read_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        g_windows_abi64_read_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

    if (file_handle != WINDOWS_ABI64_STDIN_HANDLE)
    {
        ++g_windows_abi64_read_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_read_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

    if (byte_count_arg > 0xFFFFFFFFull)
    {
        ++g_windows_abi64_read_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        g_windows_abi64_read_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

    read_count = (byte_count_arg > (u64)WINDOWS_ABI64_READ_CHUNK_BYTES)
        ? WINDOWS_ABI64_READ_CHUNK_BYTES
        : (u32)byte_count_arg;
    if (read_count == 0u)
    {
        result = WINDOWS_ABI64_STATUS_SUCCESS;
        if (windows_abi64_store_io_status(pid, io_status_block, result, 0ull) == 0u)
        {
            ++g_windows_abi64_read_fault_count;
            result = WINDOWS_ABI64_STATUS_ACCESS_VIOLATION;
        }
        else
        {
            ++g_windows_abi64_read_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                WINDOWS_ABI64_SYSCALL_NTREADFILE,
                result,
                rip);
        }
        g_windows_abi64_read_last_result = result;
        return result;
    }

    if ((windows_abi64_user_buffer_writable(pid, user_buffer, read_count) == 0u)
        || (windows_abi64_user_buffer_writable(
                pid,
                io_status_block,
                WINDOWS_ABI64_IO_STATUS_BLOCK_BYTES) == 0u))
    {
        ++g_windows_abi64_read_fault_count;
        result = WINDOWS_ABI64_STATUS_ACCESS_VIOLATION;
        g_windows_abi64_read_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

    owner_id = process64_principal(pid);
    input_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INPUT,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    g_windows_abi64_read_last_capability = input_capability;
    if (input_capability == CAPABILITY64_INVALID_HANDLE)
    {
        ++g_windows_abi64_read_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_read_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    bytes_read = input64_read_kernel(input_capability, read_scratch, read_count, owner_id);
#else
    bytes_read = INPUT64_INVALID_RESULT;
#endif
    (void)capability64_revoke(input_capability, owner_id);
    if (bytes_read == INPUT64_INVALID_RESULT)
    {
        ++g_windows_abi64_read_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_read_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTREADFILE,
            result,
            rip);
        return result;
    }

    windows_abi64_copy_to_user(user_buffer, read_scratch, bytes_read);
    result = WINDOWS_ABI64_STATUS_SUCCESS;
    (void)windows_abi64_store_io_status(pid, io_status_block, result, (u64)bytes_read);
    ++g_windows_abi64_read_count;
    g_windows_abi64_read_byte_count += bytes_read;
    g_windows_abi64_read_last_byte_count = bytes_read;
    g_windows_abi64_read_last_result = result;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        WINDOWS_ABI64_SYSCALL_NTREADFILE,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntwritefile_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    static u8 write_scratch[WINDOWS_ABI64_WRITE_CHUNK_BYTES];
    u64 file_handle = rcx;
    u64 io_status_block;
    u64 user_buffer;
    u64 byte_count_arg;
    u32 write_count;
    u32 owner_id;
    u32 console_capability;
    u32 bytes_written;
    u32 result;

    (void)rdx;
    (void)r8;
    (void)r9;

    g_windows_abi64_write_last_handle_low = (u32)file_handle;
    g_windows_abi64_write_last_byte_count = 0u;
    g_windows_abi64_write_last_capability = CAPABILITY64_INVALID_HANDLE;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_write_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_write_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    if ((stack_args == 0) || (stack_arg_count < 3u))
    {
        ++g_windows_abi64_write_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        g_windows_abi64_write_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    io_status_block = stack_args[0];
    user_buffer = stack_args[1];
    byte_count_arg = stack_args[2];

    if (io_status_block == 0ull)
    {
        ++g_windows_abi64_write_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        g_windows_abi64_write_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    if ((file_handle != WINDOWS_ABI64_STDOUT_HANDLE)
        && (file_handle != WINDOWS_ABI64_STDERR_HANDLE))
    {
        ++g_windows_abi64_write_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_write_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    if (byte_count_arg > 0xFFFFFFFFull)
    {
        ++g_windows_abi64_write_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        g_windows_abi64_write_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    write_count = (byte_count_arg > (u64)WINDOWS_ABI64_WRITE_CHUNK_BYTES)
        ? WINDOWS_ABI64_WRITE_CHUNK_BYTES
        : (u32)byte_count_arg;
    if (write_count == 0u)
    {
        result = WINDOWS_ABI64_STATUS_SUCCESS;
        if (windows_abi64_store_io_status(pid, io_status_block, result, 0ull) == 0u)
        {
            ++g_windows_abi64_write_fault_count;
            result = WINDOWS_ABI64_STATUS_ACCESS_VIOLATION;
        }
        else
        {
            ++g_windows_abi64_write_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
                result,
                rip);
        }
        g_windows_abi64_write_last_result = result;
        return result;
    }

    if ((windows_abi64_user_buffer_readable(pid, user_buffer, write_count) == 0u)
        || (windows_abi64_user_buffer_writable(
                pid,
                io_status_block,
                WINDOWS_ABI64_IO_STATUS_BLOCK_BYTES) == 0u))
    {
        ++g_windows_abi64_write_fault_count;
        result = WINDOWS_ABI64_STATUS_ACCESS_VIOLATION;
        g_windows_abi64_write_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    windows_abi64_copy_from_user(write_scratch, user_buffer, write_count);
    owner_id = process64_principal(pid);
    console_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    g_windows_abi64_write_last_capability = console_capability;
    if (console_capability == CAPABILITY64_INVALID_HANDLE)
    {
        ++g_windows_abi64_write_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_write_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    bytes_written = console64_write_kernel(
        console_capability,
        write_scratch,
        write_count,
        owner_id);
    (void)capability64_revoke(console_capability, owner_id);
    if (bytes_written == CONSOLE64_INVALID_RESULT)
    {
        ++g_windows_abi64_write_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        g_windows_abi64_write_last_result = result;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
            result,
            rip);
        return result;
    }

    result = WINDOWS_ABI64_STATUS_SUCCESS;
    (void)windows_abi64_store_io_status(pid, io_status_block, result, (u64)bytes_written);
    ++g_windows_abi64_write_count;
    g_windows_abi64_write_byte_count += bytes_written;
    g_windows_abi64_write_last_byte_count = bytes_written;
    g_windows_abi64_write_last_result = result;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        WINDOWS_ABI64_SYSCALL_NTWRITEFILE,
        result,
        rip);
    return result;
}

static u64 windows_abi64_align_up_u64(u64 value, u64 alignment)
{
    u64 mask = alignment - 1ull;

    if ((alignment == 0ull) || ((alignment & mask) != 0ull))
    {
        return 0ull;
    }

    return (value + mask) & ~mask;
}

static u64 windows_abi64_align_down_u64(u64 value, u64 alignment)
{
    u64 mask = alignment - 1ull;

    if ((alignment == 0ull) || ((alignment & mask) != 0ull))
    {
        return 0ull;
    }

    return value & ~mask;
}

static u32 windows_abi64_protect_to_vma(u32 protect)
{
    if (protect == WINDOWS_ABI64_PAGE_READONLY)
    {
        return VMA64_PROT_READ;
    }
    if (protect == WINDOWS_ABI64_PAGE_READWRITE)
    {
        return VMA64_PROT_READ | VMA64_PROT_WRITE;
    }
    if (protect == WINDOWS_ABI64_PAGE_EXECUTE)
    {
        return VMA64_PROT_EXECUTE;
    }
    if (protect == WINDOWS_ABI64_PAGE_EXECUTE_READ)
    {
        return VMA64_PROT_READ | VMA64_PROT_EXECUTE;
    }
    if (protect == WINDOWS_ABI64_PAGE_EXECUTE_READWRITE)
    {
        return VMA64_PROT_READ | VMA64_PROT_WRITE | VMA64_PROT_EXECUTE;
    }

    return 0u;
}

static u32 windows_abi64_vma_to_protect(u32 prot_flags)
{
    u32 normalized = prot_flags
        & (VMA64_PROT_READ | VMA64_PROT_WRITE | VMA64_PROT_EXECUTE);

    if (normalized == VMA64_PROT_READ)
    {
        return WINDOWS_ABI64_PAGE_READONLY;
    }
    if (normalized == (VMA64_PROT_READ | VMA64_PROT_WRITE))
    {
        return WINDOWS_ABI64_PAGE_READWRITE;
    }
    if (normalized == VMA64_PROT_EXECUTE)
    {
        return WINDOWS_ABI64_PAGE_EXECUTE;
    }
    if (normalized == (VMA64_PROT_READ | VMA64_PROT_EXECUTE))
    {
        return WINDOWS_ABI64_PAGE_EXECUTE_READ;
    }
    if (normalized == (VMA64_PROT_READ | VMA64_PROT_WRITE | VMA64_PROT_EXECUTE))
    {
        return WINDOWS_ABI64_PAGE_EXECUTE_READWRITE;
    }

    return 0u;
}

static u32 windows_abi64_allocate_record_result(
    u32 pid,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_allocate_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTALLOCATEVIRTUALMEMORY,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntallocatevirtualmemory_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 process_handle = rcx;
    u64 base_address_ptr = rdx;
    u64 zero_bits = r8;
    u64 region_size_ptr = r9;
    u32 allocation_type;
    u32 protect;
    u32 prot_flags;
    u64 requested_base;
    u64 requested_size;
    u64 rounded_size;
    u64 hint;
    u64 mapped_base;
    u32 map_flags;

    g_windows_abi64_allocate_last_base = 0ull;
    g_windows_abi64_allocate_last_size = 0ull;
    g_windows_abi64_allocate_last_protect = 0u;
    g_windows_abi64_allocate_last_type = 0u;
    g_windows_abi64_allocate_last_result = WINDOWS_ABI64_STATUS_SUCCESS;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (process_handle != WINDOWS_ABI64_CURRENT_PROCESS_HANDLE))
    {
        ++g_windows_abi64_allocate_denial_count;
        return windows_abi64_allocate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_args == 0) || (stack_arg_count < 2u))
    {
        ++g_windows_abi64_allocate_denial_count;
        return windows_abi64_allocate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((base_address_ptr == 0ull)
        || (region_size_ptr == 0ull)
        || (windows_abi64_user_buffer_readable(pid, base_address_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(pid, base_address_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_readable(pid, region_size_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(pid, region_size_ptr, 8u) == 0u)
        || (stack_args[0] > 0xFFFFFFFFull)
        || (stack_args[1] > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_allocate_fault_count;
        return windows_abi64_allocate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    requested_base = windows_abi64_load_user_u64(base_address_ptr);
    requested_size = windows_abi64_load_user_u64(region_size_ptr);
    allocation_type = (u32)stack_args[0];
    protect = (u32)stack_args[1];
    g_windows_abi64_allocate_last_base = requested_base;
    g_windows_abi64_allocate_last_size = requested_size;
    g_windows_abi64_allocate_last_protect = protect;
    g_windows_abi64_allocate_last_type = allocation_type;

    prot_flags = windows_abi64_protect_to_vma(protect);
    if ((zero_bits != 0ull)
        || (requested_size == 0ull)
        || ((allocation_type
                & ~(WINDOWS_ABI64_MEM_COMMIT
                    | WINDOWS_ABI64_MEM_RESERVE
                    | WINDOWS_ABI64_MEM_TOP_DOWN)) != 0u)
        || ((allocation_type & WINDOWS_ABI64_MEM_COMMIT) == 0u)
        || (prot_flags == 0u))
    {
        ++g_windows_abi64_allocate_denial_count;
        return windows_abi64_allocate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    rounded_size = windows_abi64_align_up_u64(requested_size, VMA64_PAGE_BYTES);
    if ((rounded_size == 0ull)
        || (rounded_size < requested_size)
        || ((requested_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        ++g_windows_abi64_allocate_denial_count;
        return windows_abi64_allocate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    map_flags = VMA64_MAP_PRIVATE | VMA64_MAP_ANONYMOUS;
    hint = requested_base;
    if (requested_base != 0ull)
    {
        map_flags |= VMA64_MAP_FIXED;
    }
    else if ((allocation_type & WINDOWS_ABI64_MEM_TOP_DOWN) != 0u)
    {
        hint = (rounded_size <= 0x0000000000200000ull)
            ? (0x0000000044200000ull - rounded_size)
            : 0ull;
    }

    mapped_base = vma64_map_anon(pid, hint, rounded_size, prot_flags, map_flags);
    if (mapped_base == 0ull)
    {
        ++g_windows_abi64_allocate_denial_count;
        return windows_abi64_allocate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_NO_MEMORY,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    if (requested_base == 0ull)
    {
        windows_abi64_store_u64(base_address_ptr, mapped_base);
    }
    if (requested_size != rounded_size)
    {
        windows_abi64_store_u64(region_size_ptr, rounded_size);
    }
    ++g_windows_abi64_allocate_count;
    if (rounded_size <= 0xFFFFFFFFull)
    {
        g_windows_abi64_allocate_byte_count += (u32)rounded_size;
    }
    else
    {
        g_windows_abi64_allocate_byte_count = 0xFFFFFFFFu;
    }
    g_windows_abi64_allocate_last_base = mapped_base;
    g_windows_abi64_allocate_last_size = rounded_size;
    return windows_abi64_allocate_record_result(
        pid,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_free_record_result(
    u32 pid,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_free_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTFREEVIRTUALMEMORY,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntfreevirtualmemory_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 process_handle = rcx;
    u64 base_address_ptr = rdx;
    u64 region_size_ptr = r8;
    u64 free_type_arg = r9;
    u64 requested_base;
    u64 requested_size;
    u64 release_size;
    u32 free_type;
    vma_region_t *region;

    (void)stack_args;
    (void)stack_arg_count;

    g_windows_abi64_free_last_base = 0ull;
    g_windows_abi64_free_last_size = 0ull;
    g_windows_abi64_free_last_type = 0u;
    g_windows_abi64_free_last_result = WINDOWS_ABI64_STATUS_SUCCESS;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (process_handle != WINDOWS_ABI64_CURRENT_PROCESS_HANDLE))
    {
        ++g_windows_abi64_free_denial_count;
        return windows_abi64_free_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (free_type_arg > 0xFFFFFFFFull)
    {
        ++g_windows_abi64_free_denial_count;
        return windows_abi64_free_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((base_address_ptr == 0ull)
        || (region_size_ptr == 0ull)
        || (windows_abi64_user_buffer_readable(pid, base_address_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(pid, base_address_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_readable(pid, region_size_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(pid, region_size_ptr, 8u) == 0u))
    {
        ++g_windows_abi64_free_fault_count;
        return windows_abi64_free_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    requested_base = windows_abi64_load_user_u64(base_address_ptr);
    requested_size = windows_abi64_load_user_u64(region_size_ptr);
    free_type = (u32)free_type_arg;
    g_windows_abi64_free_last_base = requested_base;
    g_windows_abi64_free_last_size = requested_size;
    g_windows_abi64_free_last_type = free_type;

    if (free_type != WINDOWS_ABI64_MEM_RELEASE)
    {
        ++g_windows_abi64_free_denial_count;
        return windows_abi64_free_record_result(
            pid,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (((requested_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (requested_base == 0ull)
        || (requested_size != 0ull))
    {
        ++g_windows_abi64_free_denial_count;
        return windows_abi64_free_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    region = vma64_find(pid, requested_base);
    if ((region == 0)
        || (region->virt_base != requested_base)
        || (region->virt_end <= region->virt_base)
        || (region->backing_type != VMA64_BACKING_ANON))
    {
        ++g_windows_abi64_free_denial_count;
        return windows_abi64_free_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    release_size = region->virt_end - region->virt_base;
    windows_abi64_store_u64(base_address_ptr, requested_base);
    windows_abi64_store_u64(region_size_ptr, release_size);
    if (vma64_unmap(pid, requested_base, release_size) == 0u)
    {
        ++g_windows_abi64_free_denial_count;
        return windows_abi64_free_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_windows_abi64_free_count;
    if (release_size <= 0xFFFFFFFFull)
    {
        g_windows_abi64_free_byte_count += (u32)release_size;
    }
    else
    {
        g_windows_abi64_free_byte_count = 0xFFFFFFFFu;
    }
    g_windows_abi64_free_last_base = requested_base;
    g_windows_abi64_free_last_size = release_size;
    return windows_abi64_free_record_result(
        pid,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_protect_record_result(
    u32 pid,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_protect_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTPROTECTVIRTUALMEMORY,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntprotectvirtualmemory_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 process_handle = rcx;
    u64 base_address_ptr = rdx;
    u64 region_size_ptr = r8;
    u64 new_protect_arg = r9;
    u64 old_protect_ptr;
    u64 requested_base;
    u64 requested_size;
    u64 protect_base;
    u64 protect_end;
    u64 protect_size;
    u32 new_protect;
    u32 new_vma_prot;
    u32 old_protect;
    vma_region_t *region;

    g_windows_abi64_protect_last_base = 0ull;
    g_windows_abi64_protect_last_size = 0ull;
    g_windows_abi64_protect_last_new_protect = 0u;
    g_windows_abi64_protect_last_old_protect = 0u;
    g_windows_abi64_protect_last_result = WINDOWS_ABI64_STATUS_SUCCESS;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (process_handle != WINDOWS_ABI64_CURRENT_PROCESS_HANDLE))
    {
        ++g_windows_abi64_protect_denial_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_args == 0) || (stack_arg_count < 1u) || (new_protect_arg > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_protect_denial_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    old_protect_ptr = stack_args[0];
    if ((base_address_ptr == 0ull)
        || (region_size_ptr == 0ull)
        || (old_protect_ptr == 0ull)
        || (windows_abi64_user_buffer_readable(pid, base_address_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(pid, base_address_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_readable(pid, region_size_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(pid, region_size_ptr, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(pid, old_protect_ptr, 4u) == 0u))
    {
        ++g_windows_abi64_protect_fault_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    requested_base = windows_abi64_load_user_u64(base_address_ptr);
    requested_size = windows_abi64_load_user_u64(region_size_ptr);
    new_protect = (u32)new_protect_arg;
    g_windows_abi64_protect_last_base = requested_base;
    g_windows_abi64_protect_last_size = requested_size;
    g_windows_abi64_protect_last_new_protect = new_protect;

    new_vma_prot = windows_abi64_protect_to_vma(new_protect);
    if ((requested_base == 0ull) || (requested_size == 0ull) || (new_vma_prot == 0u))
    {
        ++g_windows_abi64_protect_denial_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    protect_base = windows_abi64_align_down_u64(requested_base, VMA64_PAGE_BYTES);
    protect_end = windows_abi64_align_up_u64(requested_base + requested_size, VMA64_PAGE_BYTES);
    if ((protect_base == 0ull)
        || (protect_end <= protect_base)
        || ((requested_base + requested_size) < requested_base))
    {
        ++g_windows_abi64_protect_denial_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    protect_size = protect_end - protect_base;

    region = vma64_find(pid, protect_base);
    if ((region == 0)
        || (protect_end > region->virt_end)
        || (region->backing_type != VMA64_BACKING_ANON))
    {
        ++g_windows_abi64_protect_denial_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    old_protect = windows_abi64_vma_to_protect(region->prot_flags);
    if (old_protect == 0u)
    {
        ++g_windows_abi64_protect_denial_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    g_windows_abi64_protect_last_old_protect = old_protect;

    if (vma64_protect(pid, protect_base, protect_size, new_vma_prot) == 0u)
    {
        ++g_windows_abi64_protect_denial_count;
        return windows_abi64_protect_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u64(base_address_ptr, protect_base);
    windows_abi64_store_u64(region_size_ptr, protect_size);
    windows_abi64_store_u32(old_protect_ptr, old_protect);
    ++g_windows_abi64_protect_count;
    if (protect_size <= 0xFFFFFFFFull)
    {
        g_windows_abi64_protect_byte_count += (u32)protect_size;
    }
    else
    {
        g_windows_abi64_protect_byte_count = 0xFFFFFFFFu;
    }
    g_windows_abi64_protect_last_base = protect_base;
    g_windows_abi64_protect_last_size = protect_size;
    return windows_abi64_protect_record_result(
        pid,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_create_record_result(
    u32 pid,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_create_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTCREATEFILE,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntcreatefile_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    static u8 object_attributes[WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES];
    static u8 unicode_string[WINDOWS_ABI64_UNICODE_STRING_BYTES];
    static u8 path_ascii[WINDOWS_ABI64_CREATE_PATH_ASCII_BYTES];
    windows_vfs64_open_result_t open_result;
    u64 file_handle_out = rcx;
    u32 desired_access = (u32)rdx;
    u64 object_attributes_ptr = r8;
    u64 io_status_block = r9;
    u64 object_name_ptr;
    u64 path_buffer;
    u64 allocation_size;
    u64 ea_buffer;
    u32 object_length;
    u32 object_attributes_flags;
    u32 file_attributes;
    u32 share_access;
    u32 create_disposition;
    u32 create_options;
    u32 ea_length;
    u64 root_directory;
    u64 security_descriptor;
    u64 security_quality_of_service;
    u16 path_length;
    u16 path_maximum_length;
    u32 path_index;
    u32 path_bytes;
    u32 result;
    u8 audit_event;

    g_windows_abi64_create_last_handle_low = 0u;
    g_windows_abi64_create_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_abi64_create_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_create_last_path_hash = 0u;
    g_windows_abi64_create_last_path_bytes = 0u;
    g_windows_abi64_create_last_shim_id = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_create_denial_count;
        return windows_abi64_create_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_args == 0) || (stack_arg_count < 7u))
    {
        ++g_windows_abi64_create_denial_count;
        return windows_abi64_create_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((file_handle_out == 0ull)
        || (io_status_block == 0ull)
        || (windows_abi64_user_buffer_writable(pid, file_handle_out, 8u) == 0u)
        || (windows_abi64_user_buffer_writable(
                pid,
                io_status_block,
                WINDOWS_ABI64_IO_STATUS_BLOCK_BYTES) == 0u)
        || (windows_abi64_user_buffer_readable(
                pid,
                object_attributes_ptr,
                WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES) == 0u))
    {
        ++g_windows_abi64_create_fault_count;
        return windows_abi64_create_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_copy_from_user(
        object_attributes,
        object_attributes_ptr,
        WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES);
    object_length = windows_abi64_load_u32(object_attributes);
    root_directory = windows_abi64_load_u64(object_attributes + 8u);
    object_name_ptr = windows_abi64_load_u64(object_attributes + 16u);
    object_attributes_flags = windows_abi64_load_u32(object_attributes + 24u);
    security_descriptor = windows_abi64_load_u64(object_attributes + 32u);
    security_quality_of_service = windows_abi64_load_u64(object_attributes + 40u);

    allocation_size = stack_args[0];
    file_attributes = (u32)stack_args[1];
    share_access = (u32)stack_args[2];
    create_disposition = (u32)stack_args[3];
    create_options = (u32)stack_args[4];
    ea_buffer = stack_args[5];
    ea_length = (u32)stack_args[6];
    (void)object_attributes_flags;
    (void)file_attributes;
    (void)share_access;

    if ((object_length < WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES)
        || (object_name_ptr == 0ull)
        || (rdx > 0xFFFFFFFFull)
        || (stack_args[1] > 0xFFFFFFFFull)
        || (stack_args[2] > 0xFFFFFFFFull)
        || (stack_args[3] > 0xFFFFFFFFull)
        || (stack_args[4] > 0xFFFFFFFFull)
        || (stack_args[6] > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_create_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        return windows_abi64_create_record_result(
            pid,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((root_directory != 0ull)
        || (security_descriptor != 0ull)
        || (security_quality_of_service != 0ull)
        || (allocation_size != 0ull)
        || (ea_buffer != 0ull)
        || (ea_length != 0u))
    {
        ++g_windows_abi64_create_denial_count;
        result = WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        return windows_abi64_create_record_result(
            pid,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (windows_abi64_user_buffer_readable(
            pid,
            object_name_ptr,
            WINDOWS_ABI64_UNICODE_STRING_BYTES) == 0u)
    {
        ++g_windows_abi64_create_fault_count;
        return windows_abi64_create_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_copy_from_user(
        unicode_string,
        object_name_ptr,
        WINDOWS_ABI64_UNICODE_STRING_BYTES);
    path_length = windows_abi64_load_u16(unicode_string);
    path_maximum_length = windows_abi64_load_u16(unicode_string + 2u);
    path_buffer = windows_abi64_load_u64(unicode_string + 8u);
    if ((path_length == 0u)
        || ((path_length & 1u) != 0u)
        || (path_length > WINDOWS_ABI64_CREATE_PATH_UTF16_BYTES)
        || (path_maximum_length < path_length)
        || (path_buffer == 0ull)
        || (windows_abi64_user_buffer_readable(pid, path_buffer, (u32)path_length) == 0u))
    {
        ++g_windows_abi64_create_fault_count;
        return windows_abi64_create_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    path_bytes = ((u32)path_length) / 2u;
    for (path_index = 0u; path_index < path_bytes; ++path_index)
    {
        volatile const u8 *source =
            (volatile const u8 *)(u64)(path_buffer + ((u64)path_index * 2ull));
        u8 low = source[0];
        u8 high = source[1];

        if ((high != 0u) || (low == 0u))
        {
            ++g_windows_abi64_create_denial_count;
            result = WINDOWS_ABI64_STATUS_INVALID_PARAMETER;
            (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
            return windows_abi64_create_record_result(
                pid,
                result,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }
        path_ascii[path_index] = low;
    }

    result = windows_vfs64_open_path(
        pid,
        path_ascii,
        path_bytes,
        desired_access,
        create_disposition,
        create_options,
        &open_result);
    g_windows_abi64_create_last_handle_low = (u32)open_result.handle;
    g_windows_abi64_create_last_capability = open_result.capability_handle;
    g_windows_abi64_create_last_path_hash = open_result.path_hash;
    g_windows_abi64_create_last_path_bytes = open_result.path_bytes;
    g_windows_abi64_create_last_shim_id = open_result.shim_id;
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        ++g_windows_abi64_create_denial_count;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        audit_event =
            ((result == WINDOWS_ABI64_STATUS_ACCESS_DENIED)
                || (result == WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND)
                || (result == WINDOWS_ABI64_STATUS_INVALID_HANDLE))
                ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
                : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED;
        return windows_abi64_create_record_result(pid, result, rip, audit_event);
    }

    windows_abi64_store_u64(file_handle_out, open_result.handle);
    if (windows_abi64_store_io_status(
            pid,
            io_status_block,
            WINDOWS_ABI64_STATUS_SUCCESS,
            WINDOWS_ABI64_FILE_OPENED) == 0u)
    {
        (void)windows_handle64_close(pid, open_result.handle);
        ++g_windows_abi64_create_fault_count;
        return windows_abi64_create_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_windows_abi64_create_count;
    return windows_abi64_create_record_result(
        pid,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_query_file_record_result(
    u32 pid,
    u32 result,
    u32 return_length,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_query_file_last_result = result;
    g_windows_abi64_query_file_last_return_length = return_length;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONFILE,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntqueryinformationfile_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    windows_abi64_file_position_record_t *position_record;
    u64 file_handle = rcx;
    u64 io_status_block = rdx;
    u64 file_information = r8;
    u64 information_length = r9;
    u32 information_class;
    u32 required_length;
    u32 result;

    g_windows_abi64_query_file_last_handle_low = (u32)file_handle;
    g_windows_abi64_query_file_last_class = 0u;
    g_windows_abi64_query_file_last_return_length = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_query_file_denial_count;
        return windows_abi64_query_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_args == 0)
        || (stack_arg_count < 1u)
        || (stack_args[0] > 0xFFFFFFFFull)
        || (information_length > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_query_file_denial_count;
        return windows_abi64_query_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    information_class = (u32)stack_args[0];
    g_windows_abi64_query_file_last_class = information_class;
    if (information_class == WINDOWS_ABI64_FILE_POSITION_INFORMATION_CLASS)
    {
        required_length = WINDOWS_ABI64_FILE_POSITION_INFORMATION_BYTES;
    }
    else if (information_class == WINDOWS_ABI64_FILE_STANDARD_INFORMATION_CLASS)
    {
        required_length = WINDOWS_ABI64_FILE_STANDARD_INFORMATION_BYTES;
    }
    else
    {
        required_length = 0u;
    }

    if ((io_status_block == 0ull)
        || (file_information == 0ull)
        || (required_length == 0u)
        || (windows_abi64_user_buffer_writable(
                pid,
                io_status_block,
                WINDOWS_ABI64_IO_STATUS_BLOCK_BYTES) == 0u))
    {
        if (required_length == 0u)
        {
            ++g_windows_abi64_query_file_denial_count;
            return windows_abi64_query_file_record_result(
                pid,
                WINDOWS_ABI64_STATUS_INVALID_INFO_CLASS,
                0u,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }

        ++g_windows_abi64_query_file_fault_count;
        return windows_abi64_query_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            required_length,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    if (windows_abi64_file_handle_is_scoped(pid, file_handle) == 0u)
    {
        ++g_windows_abi64_query_file_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        return windows_abi64_query_file_record_result(
            pid,
            result,
            required_length,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (information_length < (u64)required_length)
    {
        ++g_windows_abi64_query_file_denial_count;
        result = WINDOWS_ABI64_STATUS_INFO_LENGTH_MISMATCH;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        return windows_abi64_query_file_record_result(
            pid,
            result,
            required_length,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (windows_abi64_user_buffer_writable(pid, file_information, required_length) == 0u)
    {
        ++g_windows_abi64_query_file_fault_count;
        return windows_abi64_query_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            required_length,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    if (information_class == WINDOWS_ABI64_FILE_POSITION_INFORMATION_CLASS)
    {
        position_record =
            windows_abi64_file_position_record_for(pid, file_handle, 0u);
        windows_abi64_store_u64(
            file_information,
            (position_record != 0) ? position_record->position : 0ull);
    }
    else
    {
        if (windows_abi64_is_standard_file_handle(file_handle) == 0u)
        {
            ++g_windows_abi64_query_file_denial_count;
            result = WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED;
            (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
            return windows_abi64_query_file_record_result(
                pid,
                result,
                required_length,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }
        windows_abi64_zero_user(file_information, required_length);
        windows_abi64_store_u32(file_information + 16ull, 1u);
    }

    result = WINDOWS_ABI64_STATUS_SUCCESS;
    (void)windows_abi64_store_io_status(
        pid,
        io_status_block,
        result,
        (u64)required_length);
    ++g_windows_abi64_query_file_count;
    return windows_abi64_query_file_record_result(
        pid,
        result,
        required_length,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_set_file_record_result(
    u32 pid,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_set_file_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTSETINFORMATIONFILE,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntsetinformationfile_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    windows_abi64_file_position_record_t *position_record;
    u64 file_handle = rcx;
    u64 io_status_block = rdx;
    u64 file_information = r8;
    u64 information_length = r9;
    u32 information_class;
    u32 result;

    g_windows_abi64_set_file_last_handle_low = (u32)file_handle;
    g_windows_abi64_set_file_last_class = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_set_file_denial_count;
        return windows_abi64_set_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_args == 0)
        || (stack_arg_count < 1u)
        || (stack_args[0] > 0xFFFFFFFFull)
        || (information_length > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_set_file_denial_count;
        return windows_abi64_set_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    information_class = (u32)stack_args[0];
    g_windows_abi64_set_file_last_class = information_class;
    if (information_class != WINDOWS_ABI64_FILE_POSITION_INFORMATION_CLASS)
    {
        ++g_windows_abi64_set_file_denial_count;
        return windows_abi64_set_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_INFO_CLASS,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((io_status_block == 0ull)
        || (file_information == 0ull)
        || (windows_abi64_user_buffer_writable(
                pid,
                io_status_block,
                WINDOWS_ABI64_IO_STATUS_BLOCK_BYTES) == 0u)
        || (windows_abi64_user_buffer_readable(
                pid,
                file_information,
                WINDOWS_ABI64_FILE_POSITION_INFORMATION_BYTES) == 0u))
    {
        ++g_windows_abi64_set_file_fault_count;
        return windows_abi64_set_file_record_result(
            pid,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (information_length < (u64)WINDOWS_ABI64_FILE_POSITION_INFORMATION_BYTES)
    {
        ++g_windows_abi64_set_file_denial_count;
        result = WINDOWS_ABI64_STATUS_INFO_LENGTH_MISMATCH;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        return windows_abi64_set_file_record_result(
            pid,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (windows_abi64_file_handle_is_scoped(pid, file_handle) == 0u)
    {
        ++g_windows_abi64_set_file_denial_count;
        result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        return windows_abi64_set_file_record_result(
            pid,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    position_record =
        windows_abi64_file_position_record_for(pid, file_handle, 1u);
    if (position_record == 0)
    {
        ++g_windows_abi64_set_file_denial_count;
        result = WINDOWS_ABI64_STATUS_NO_MEMORY;
        (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
        return windows_abi64_set_file_record_result(
            pid,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    position_record->position = windows_abi64_load_user_u64(file_information);
    result = WINDOWS_ABI64_STATUS_SUCCESS;
    (void)windows_abi64_store_io_status(pid, io_status_block, result, 0ull);
    ++g_windows_abi64_set_file_count;
    return windows_abi64_set_file_record_result(
        pid,
        result,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_close_record_result(
    u32 pid,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_close_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTCLOSE,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntclose_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 handle = rcx;
    u32 result;

    (void)rdx;
    (void)r8;
    (void)r9;
    (void)stack_args;
    (void)stack_arg_count;
    g_windows_abi64_close_last_handle_low = (u32)handle;
    g_windows_abi64_close_last_result = WINDOWS_ABI64_STATUS_SUCCESS;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_close_denial_count;
        return windows_abi64_close_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (windows_handle64_close(pid, handle) == 0u)
    {
        ++g_windows_abi64_close_denial_count;
        result = windows_handle64_last_result();
        if (result == WINDOWS_ABI64_STATUS_SUCCESS)
        {
            result = WINDOWS_ABI64_STATUS_INVALID_HANDLE;
        }
        return windows_abi64_close_record_result(
            pid,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    windows_abi64_file_position_record_clear_for(pid, handle);
    ++g_windows_abi64_close_count;
    return windows_abi64_close_record_result(
        pid,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_terminate_record_result(
    u32 pid,
    u32 result,
    u32 status,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_terminate_last_pid = pid;
    g_windows_abi64_terminate_last_status = status;
    g_windows_abi64_terminate_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTTERMINATEPROCESS,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntterminateprocess_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 process_handle = rcx;
    u64 exit_status = rdx;
    u32 status = (u32)exit_status;
    u32 result;

    (void)r8;
    (void)r9;
    (void)stack_args;
    (void)stack_arg_count;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_terminate_denial_count;
        return windows_abi64_terminate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            status,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((process_handle != WINDOWS_ABI64_CURRENT_PROCESS_HANDLE)
        || (exit_status > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_terminate_denial_count;
        return windows_abi64_terminate_record_result(
            pid,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            status,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_windows_abi64_terminate_count;
    result = WINDOWS_ABI64_STATUS_SUCCESS;
    return windows_abi64_terminate_record_result(
        pid,
        result,
        status,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_registry_record_result(
    u32 pid,
    u32 syscall_number,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_registry_last_syscall = syscall_number;
    g_windows_abi64_registry_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        (u16)syscall_number,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntopenkey_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    static u8 key_path[WINDOWS_REGISTRY64_MAX_PATH_BYTES];
    windows_registry64_open_result_t open_result;
    u64 key_handle_out = rcx;
    u64 desired_access = rdx;
    u64 object_attributes = r8;
    u64 root_directory = 0ull;
    u32 key_path_bytes = 0u;
    u32 result;

    (void)r9;
    (void)stack_args;
    (void)stack_arg_count;
    g_windows_abi64_registry_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;
    g_windows_abi64_registry_last_required_bytes = 0u;
    g_windows_abi64_registry_last_value_type = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTOPENKEY,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((key_handle_out == 0ull)
        || (desired_access > 0xFFFFFFFFull)
        || (windows_abi64_user_buffer_writable(pid, key_handle_out, 8u) == 0u))
    {
        ++g_windows_abi64_registry_fault_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTOPENKEY,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    result = windows_abi64_read_object_name_ascii(
        pid,
        object_attributes,
        key_path,
        WINDOWS_REGISTRY64_MAX_PATH_BYTES,
        &key_path_bytes,
        &root_directory);
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        if (result == WINDOWS_ABI64_STATUS_ACCESS_VIOLATION)
        {
            ++g_windows_abi64_registry_fault_count;
        }
        else
        {
            ++g_windows_abi64_registry_denial_count;
        }
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTOPENKEY,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (root_directory != 0ull)
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTOPENKEY,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    result = windows_registry64_open_key(
        pid,
        key_path,
        key_path_bytes,
        (u32)desired_access,
        &open_result);
    g_windows_abi64_registry_last_key_id = open_result.key_id;
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTOPENKEY,
            result,
            rip,
            (result == WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND)
                ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
                : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u64(key_handle_out, open_result.handle);
    ++g_windows_abi64_registry_open_count;
    return windows_abi64_registry_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTOPENKEY,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntcreatekey_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    static u8 key_path[WINDOWS_REGISTRY64_MAX_PATH_BYTES];
    windows_registry64_open_result_t create_result;
    u64 key_handle_out = rcx;
    u64 desired_access = rdx;
    u64 object_attributes = r8;
    u64 title_index = r9;
    u64 class_string = ((stack_args != 0) && (stack_arg_count > 0u))
        ? stack_args[0]
        : 0ull;
    u64 create_options = ((stack_args != 0) && (stack_arg_count > 1u))
        ? stack_args[1]
        : 0ull;
    u64 disposition_out = ((stack_args != 0) && (stack_arg_count > 2u))
        ? stack_args[2]
        : 0ull;
    u64 root_directory = 0ull;
    u32 key_path_bytes = 0u;
    u32 result;

    g_windows_abi64_registry_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;
    g_windows_abi64_registry_last_required_bytes = 0u;
    g_windows_abi64_registry_last_value_type = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEKEY,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_arg_count < 3u)
        || (key_handle_out == 0ull)
        || (desired_access > 0xFFFFFFFFull)
        || (title_index != 0ull)
        || (class_string != 0ull)
        || (create_options != 0ull)
        || (windows_abi64_user_buffer_writable(pid, key_handle_out, 8u) == 0u)
        || ((disposition_out != 0ull)
            && (windows_abi64_user_buffer_writable(pid, disposition_out, 4u) == 0u)))
    {
        ++g_windows_abi64_registry_fault_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEKEY,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    result = windows_abi64_read_object_name_ascii(
        pid,
        object_attributes,
        key_path,
        WINDOWS_REGISTRY64_MAX_PATH_BYTES,
        &key_path_bytes,
        &root_directory);
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        if (result == WINDOWS_ABI64_STATUS_ACCESS_VIOLATION)
        {
            ++g_windows_abi64_registry_fault_count;
        }
        else
        {
            ++g_windows_abi64_registry_denial_count;
        }
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEKEY,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (root_directory != 0ull)
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEKEY,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    result = windows_registry64_create_key(
        pid,
        key_path,
        key_path_bytes,
        (u32)desired_access,
        &create_result);
    g_windows_abi64_registry_last_key_id = create_result.key_id;
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEKEY,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u64(key_handle_out, create_result.handle);
    if (disposition_out != 0ull)
    {
        windows_abi64_store_u32(disposition_out, create_result.disposition);
    }
    ++g_windows_abi64_registry_create_count;
    return windows_abi64_registry_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTCREATEKEY,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntqueryvaluekey_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    static u8 value_name[WINDOWS_REGISTRY64_MAX_VALUE_NAME_BYTES];
    windows_registry64_value_result_t value_result;
    u64 key_handle = rcx;
    u64 value_name_ptr = rdx;
    u64 information_class = r8;
    u64 key_value_information = r9;
    u64 key_value_length = ((stack_args != 0) && (stack_arg_count > 0u))
        ? stack_args[0]
        : 0ull;
    u64 result_length_out = ((stack_args != 0) && (stack_arg_count > 1u))
        ? stack_args[1]
        : 0ull;
    u32 value_name_bytes = 0u;
    u32 result;
    u32 index;

    g_windows_abi64_registry_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;
    g_windows_abi64_registry_last_required_bytes = 0u;
    g_windows_abi64_registry_last_value_type = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_arg_count < 2u)
        || (information_class > 0xFFFFFFFFull)
        || (key_value_length > 0xFFFFFFFFull)
        || ((result_length_out != 0ull)
            && (windows_abi64_user_buffer_writable(pid, result_length_out, 4u) == 0u)))
    {
        ++g_windows_abi64_registry_fault_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    result = windows_abi64_read_unicode_ascii(
        pid,
        value_name_ptr,
        value_name,
        WINDOWS_REGISTRY64_MAX_VALUE_NAME_BYTES,
        &value_name_bytes);
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        if (result == WINDOWS_ABI64_STATUS_ACCESS_VIOLATION)
        {
            ++g_windows_abi64_registry_fault_count;
        }
        else
        {
            ++g_windows_abi64_registry_denial_count;
        }
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    result = windows_registry64_query_value(
        pid,
        key_handle,
        value_name,
        value_name_bytes,
        (u32)information_class,
        &value_result);
    g_windows_abi64_registry_last_key_id = value_result.key_id;
    g_windows_abi64_registry_last_required_bytes = value_result.required_bytes;
    g_windows_abi64_registry_last_value_type = value_result.value_type;
    if (result_length_out != 0ull)
    {
        windows_abi64_store_u32(result_length_out, value_result.required_bytes);
    }
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY,
            result,
            rip,
            (result == WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND)
                ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
                : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (((u32)key_value_length < value_result.required_bytes)
        || (key_value_information == 0ull))
    {
        ++g_windows_abi64_registry_denial_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY,
            WINDOWS_ABI64_STATUS_BUFFER_TOO_SMALL,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (windows_abi64_user_buffer_writable(
            pid,
            key_value_information,
            value_result.required_bytes) == 0u)
    {
        ++g_windows_abi64_registry_fault_count;
        return windows_abi64_registry_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_zero_user(key_value_information, value_result.required_bytes);
    windows_abi64_store_u32(key_value_information, 0u);
    windows_abi64_store_u32(key_value_information + 4ull, value_result.value_type);
    windows_abi64_store_u32(key_value_information + 8ull, value_result.data_bytes);
    for (index = 0u; index < ((value_result.data_bytes - 2u) / 2u); ++index)
    {
        volatile u8 *target = (volatile u8 *)(u64)(
            key_value_information + 12ull + ((u64)index * 2ull));
        target[0] = value_result.data_ascii[index];
        target[1] = 0u;
    }
    windows_abi64_store_u16(
        key_value_information + 12ull + (u64)value_result.data_bytes - 2ull,
        0u);

    ++g_windows_abi64_registry_query_count;
    return windows_abi64_registry_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_event_record_result(
    u32 pid,
    u32 syscall_number,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_event_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        (u16)syscall_number,
        result,
        rip);
    return result;
}

static u32 windows_abi64_mutant_record_result(
    u32 pid,
    u32 syscall_number,
    u32 result,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_mutant_last_result = result;
    (void)persona_audit64_record(
        pid,
        event_type,
        (u16)syscall_number,
        result,
        rip);
    return result;
}

static u32 windows_abi64_ntcreateevent_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 event_handle_out = rcx;
    u64 desired_access = rdx;
    u64 object_attributes = r8;
    u64 event_type_arg = r9;
    u64 initial_state_arg;
    u32 event_type;
    u32 manual_reset;
    u64 handle;
    u32 result;

    g_windows_abi64_event_last_handle_low = 0u;
    g_windows_abi64_event_last_previous_state = 0u;
    g_windows_abi64_event_last_state = 0u;
    g_windows_abi64_event_last_result = WINDOWS_ABI64_STATUS_SUCCESS;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEEVENT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((stack_args == 0)
        || (stack_arg_count < 1u)
        || (desired_access > 0xFFFFFFFFull)
        || (event_type_arg > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEEVENT,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((event_handle_out == 0ull)
        || (windows_abi64_user_buffer_writable(pid, event_handle_out, 8u) == 0u))
    {
        ++g_windows_abi64_event_fault_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEEVENT,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (object_attributes != 0ull)
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEEVENT,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    event_type = (u32)event_type_arg;
    if ((event_type != WINDOWS_ABI64_NOTIFICATION_EVENT)
        && (event_type != WINDOWS_ABI64_SYNCHRONIZATION_EVENT))
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEEVENT,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    manual_reset =
        (event_type == WINDOWS_ABI64_NOTIFICATION_EVENT) ? 1u : 0u;
    initial_state_arg = stack_args[0];
    handle = windows_handle64_event_create(
        pid,
        manual_reset,
        (initial_state_arg != 0ull) ? 1u : 0u,
        0u);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        ++g_windows_abi64_event_denial_count;
        result = windows_handle64_last_result();
        if (result == WINDOWS_ABI64_STATUS_SUCCESS)
        {
            result = WINDOWS_ABI64_STATUS_NO_MEMORY;
        }
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEEVENT,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u64(event_handle_out, handle);
    ++g_windows_abi64_event_create_count;
    g_windows_abi64_event_last_handle_low = (u32)handle;
    g_windows_abi64_event_last_state = windows_handle64_event_signaled(pid, handle);
    return windows_abi64_event_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTCREATEEVENT,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntsetevent_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 event_handle = rcx;
    u64 previous_state_ptr = rdx;
    u32 previous_state = 0u;
    u32 wake_task_id = SCHEDULER64_INVALID_TASK;

    (void)r8;
    (void)r9;
    (void)stack_args;
    (void)stack_arg_count;

    g_windows_abi64_event_last_handle_low = (u32)event_handle;
    g_windows_abi64_event_last_previous_state = 0u;
    g_windows_abi64_event_last_state = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTSETEVENT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((previous_state_ptr != 0ull)
        && (windows_abi64_user_buffer_writable(pid, previous_state_ptr, 4u) == 0u))
    {
        ++g_windows_abi64_event_fault_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTSETEVENT,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    if (windows_handle64_event_set(
            pid,
            event_handle,
            &previous_state,
            &wake_task_id) == 0u)
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTSETEVENT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    if (previous_state_ptr != 0ull)
    {
        windows_abi64_store_u32(previous_state_ptr, previous_state);
    }
    if ((wake_task_id != SCHEDULER64_INVALID_TASK)
        && (windows_abi64_wait_wake_task_success(wake_task_id) == 0u))
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTSETEVENT,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    ++g_windows_abi64_event_set_count;
    g_windows_abi64_event_last_previous_state = previous_state;
    g_windows_abi64_event_last_state = windows_handle64_event_signaled(pid, event_handle);
    return windows_abi64_event_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTSETEVENT,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntcreatemutant_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 mutant_handle_out = rcx;
    u64 desired_access = rdx;
    u64 object_attributes = r8;
    u64 initial_owner = r9;
    u64 handle;
    u32 result;

    (void)stack_args;
    (void)stack_arg_count;

    g_windows_abi64_mutant_last_handle_low = 0u;
    g_windows_abi64_mutant_last_previous_count = 0u;
    g_windows_abi64_mutant_last_owner = PROCESS64_INVALID_PID;
    g_windows_abi64_mutant_last_count = 0u;
    g_windows_abi64_mutant_last_result = WINDOWS_ABI64_STATUS_SUCCESS;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_mutant_denial_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (desired_access > 0xFFFFFFFFull)
    {
        ++g_windows_abi64_mutant_denial_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((mutant_handle_out == 0ull)
        || (windows_abi64_user_buffer_writable(pid, mutant_handle_out, 8u) == 0u))
    {
        ++g_windows_abi64_mutant_fault_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (object_attributes != 0ull)
    {
        ++g_windows_abi64_mutant_denial_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    handle = windows_handle64_mutant_create(
        pid,
        (initial_owner != 0ull) ? 1u : 0u,
        0u);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        ++g_windows_abi64_mutant_denial_count;
        result = windows_handle64_last_result();
        if (result == WINDOWS_ABI64_STATUS_SUCCESS)
        {
            result = WINDOWS_ABI64_STATUS_NO_MEMORY;
        }
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT,
            result,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u64(mutant_handle_out, handle);
    ++g_windows_abi64_mutant_create_count;
    g_windows_abi64_mutant_last_handle_low = (u32)handle;
    g_windows_abi64_mutant_last_owner = windows_handle64_mutant_owner(pid, handle);
    g_windows_abi64_mutant_last_count = windows_handle64_mutant_recursion(pid, handle);
    return windows_abi64_mutant_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntreleasemutant_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 mutant_handle = rcx;
    u64 previous_count_ptr = rdx;
    u32 previous_count = 0u;
    u32 wake_task_id = SCHEDULER64_INVALID_TASK;
    u32 result;

    (void)r8;
    (void)r9;
    (void)stack_args;
    (void)stack_arg_count;

    g_windows_abi64_mutant_last_handle_low = (u32)mutant_handle;
    g_windows_abi64_mutant_last_previous_count = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_mutant_denial_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((previous_count_ptr != 0ull)
        && (windows_abi64_user_buffer_writable(pid, previous_count_ptr, 4u) == 0u))
    {
        ++g_windows_abi64_mutant_fault_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    result = windows_handle64_mutant_release(
        pid,
        mutant_handle,
        &previous_count,
        &wake_task_id);
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        ++g_windows_abi64_mutant_denial_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT,
            result,
            rip,
            (result == WINDOWS_ABI64_STATUS_INVALID_HANDLE)
                ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
                : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    if (previous_count_ptr != 0ull)
    {
        windows_abi64_store_u32(previous_count_ptr, previous_count);
    }
    if ((wake_task_id != SCHEDULER64_INVALID_TASK)
        && (windows_abi64_wait_wake_task_success(wake_task_id) == 0u))
    {
        ++g_windows_abi64_mutant_denial_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    ++g_windows_abi64_mutant_release_count;
    g_windows_abi64_mutant_last_previous_count = previous_count;
    g_windows_abi64_mutant_last_owner =
        windows_handle64_mutant_owner(pid, mutant_handle);
    g_windows_abi64_mutant_last_count =
        windows_handle64_mutant_recursion(pid, mutant_handle);
    return windows_abi64_mutant_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntwaitforsingleobject_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 handle = rcx;
    u64 alertable = rdx;
    u64 timeout_ptr = r8;
    u64 timeout_value = 0ull;
    u32 object_type;
    u32 wait_result;
    u32 task_id;
    u32 block_result;

    (void)r9;
    (void)stack_args;
    (void)stack_arg_count;

    g_windows_abi64_event_last_handle_low = (u32)handle;
    g_windows_abi64_event_last_state = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (alertable != 0ull)
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
            WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (timeout_ptr != 0ull)
    {
        if (windows_abi64_user_buffer_readable(pid, timeout_ptr, 8u) == 0u)
        {
            ++g_windows_abi64_event_fault_count;
            return windows_abi64_event_record_result(
                pid,
                WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
                WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }
        timeout_value = windows_abi64_load_user_u64(timeout_ptr);
    }

    object_type = windows_handle64_entry_type(pid, handle);
    if (object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
    {
        g_windows_abi64_mutant_last_handle_low = (u32)handle;
        wait_result = windows_handle64_mutant_wait(pid, handle);
        g_windows_abi64_mutant_last_owner = windows_handle64_mutant_owner(pid, handle);
        g_windows_abi64_mutant_last_count =
            windows_handle64_mutant_recursion(pid, handle);
        if (wait_result == WINDOWS_HANDLE64_MUTANT_WAIT_DENIED)
        {
            ++g_windows_abi64_mutant_denial_count;
            return windows_abi64_mutant_record_result(
                pid,
                WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
                WINDOWS_ABI64_STATUS_INVALID_HANDLE,
                rip,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
        }
        if (wait_result == WINDOWS_HANDLE64_MUTANT_WAIT_TIMEOUT)
        {
            ++g_windows_abi64_mutant_wait_count;
            if ((timeout_ptr != 0ull) && (timeout_value == 0ull))
            {
                windows_abi64_note_wait_timeout(
                    SCHEDULER64_INVALID_TASK,
                    WINDOWS_HANDLE64_TYPE_MUTANT,
                    handle,
                    0u,
                    WINDOWS_ABI64_STATUS_TIMEOUT);
                return windows_abi64_mutant_record_result(
                    pid,
                    WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
                    WINDOWS_ABI64_STATUS_TIMEOUT,
                    rip,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
            }
            if (windows_abi64_wait_block_current(
                    pid,
                    WINDOWS_HANDLE64_TYPE_MUTANT,
                    handle,
                    timeout_ptr,
                    timeout_value,
                    rip,
                    &task_id,
                    &block_result) == 0u)
            {
                ++g_windows_abi64_mutant_denial_count;
                return windows_abi64_mutant_record_result(
                    pid,
                    WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
                    block_result,
                    rip,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
            }
            return windows_abi64_mutant_record_result(
                pid,
                WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
                WINDOWS_ABI64_STATUS_SUCCESS,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }

        ++g_windows_abi64_mutant_wait_count;
        return windows_abi64_mutant_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
            WINDOWS_ABI64_STATUS_SUCCESS,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (object_type != WINDOWS_HANDLE64_TYPE_EVENT)
    {
        (void)windows_handle64_event_wait(pid, handle);
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }

    wait_result = windows_handle64_event_wait(pid, handle);
    if (wait_result == WINDOWS_HANDLE64_EVENT_WAIT_DENIED)
    {
        ++g_windows_abi64_event_denial_count;
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    g_windows_abi64_event_last_state = windows_handle64_event_signaled(pid, handle);
    if (wait_result == WINDOWS_HANDLE64_EVENT_WAIT_TIMEOUT)
    {
        ++g_windows_abi64_event_wait_count;
        if ((timeout_ptr != 0ull) && (timeout_value == 0ull))
        {
            windows_abi64_note_wait_timeout(
                SCHEDULER64_INVALID_TASK,
                WINDOWS_HANDLE64_TYPE_EVENT,
                handle,
                0u,
                WINDOWS_ABI64_STATUS_TIMEOUT);
            return windows_abi64_event_record_result(
                pid,
                WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
                WINDOWS_ABI64_STATUS_TIMEOUT,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }
        if (windows_abi64_wait_block_current(
                pid,
                WINDOWS_HANDLE64_TYPE_EVENT,
                handle,
                timeout_ptr,
                timeout_value,
                rip,
                &task_id,
                &block_result) == 0u)
        {
            ++g_windows_abi64_event_denial_count;
            return windows_abi64_event_record_result(
                pid,
                WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
                block_result,
                rip,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
        }
        return windows_abi64_event_record_result(
            pid,
            WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
            WINDOWS_ABI64_STATUS_SUCCESS,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    ++g_windows_abi64_event_wait_count;
    return windows_abi64_event_record_result(
        pid,
        WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT,
        WINDOWS_ABI64_STATUS_SUCCESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_query_process_store_return_length(
    u32 pid,
    u64 return_length,
    u32 value)
{
    if (return_length == 0ull)
    {
        return 1u;
    }
    if (windows_abi64_user_buffer_writable(pid, return_length, 4u) == 0u)
    {
        return 0u;
    }

    windows_abi64_store_u32(return_length, value);
    return 1u;
}

static u32 windows_abi64_query_process_record_result(
    u32 pid,
    u32 info_class,
    u32 result,
    u32 return_length,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_query_process_last_class = info_class;
    g_windows_abi64_query_process_last_result = result;
    g_windows_abi64_query_process_last_return_length = return_length;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONPROCESS,
        result,
        rip);
    return result;
}

static u32 windows_abi64_query_process_handle_is_current(u32 pid, u64 handle)
{
    u32 object_type = WINDOWS_HANDLE64_TYPE_EMPTY;
    u64 identity = 0ull;

    if (handle != WINDOWS_ABI64_CURRENT_PROCESS_HANDLE)
    {
        return 0u;
    }
    if (windows_handle64_resolve_pseudo(pid, handle, &object_type, &identity) == 0u)
    {
        return 0u;
    }

    return ((object_type == WINDOWS_HANDLE64_TYPE_PROCESS) && (identity == (u64)pid))
        ? 1u
        : 0u;
}

static u32 windows_abi64_query_process_write_basic(
    u32 pid,
    u64 process_information,
    u64 return_length,
    u32 process_information_length,
    const persona_context_t *context,
    u64 rip)
{
    if (windows_abi64_query_process_store_return_length(
            pid,
            return_length,
            WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES) == 0u)
    {
        ++g_windows_abi64_query_process_fault_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_CLASS,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (process_information_length < WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES)
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_CLASS,
            WINDOWS_ABI64_STATUS_INFO_LENGTH_MISMATCH,
            WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((process_information == 0ull)
        || (windows_abi64_user_buffer_writable(
                pid,
                process_information,
                WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES) == 0u))
    {
        ++g_windows_abi64_query_process_fault_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_CLASS,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u32(process_information, WINDOWS_ABI64_STATUS_SUCCESS);
    windows_abi64_store_u32(process_information + 4ull, 0u);
    windows_abi64_store_u64(process_information + 8ull, context->windows_peb_base);
    windows_abi64_store_u64(process_information + 16ull, 1ull);
    windows_abi64_store_u64(process_information + 24ull, 8ull);
    windows_abi64_store_u64(process_information + 32ull, (u64)pid);
    windows_abi64_store_u64(process_information + 40ull, 0ull);

    ++g_windows_abi64_query_process_count;
    return windows_abi64_query_process_record_result(
        pid,
        WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_CLASS,
        WINDOWS_ABI64_STATUS_SUCCESS,
        WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_query_process_write_debug_port(
    u32 pid,
    u64 process_information,
    u64 return_length,
    u32 process_information_length,
    u64 rip)
{
    if (windows_abi64_query_process_store_return_length(
            pid,
            return_length,
            WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES) == 0u)
    {
        ++g_windows_abi64_query_process_fault_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_DEBUG_PORT_CLASS,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (process_information_length < WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES)
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_DEBUG_PORT_CLASS,
            WINDOWS_ABI64_STATUS_INFO_LENGTH_MISMATCH,
            WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((process_information == 0ull)
        || (windows_abi64_user_buffer_writable(
                pid,
                process_information,
                WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES) == 0u))
    {
        ++g_windows_abi64_query_process_fault_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_DEBUG_PORT_CLASS,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u64(process_information, 0ull);

    ++g_windows_abi64_query_process_count;
    return windows_abi64_query_process_record_result(
        pid,
        WINDOWS_ABI64_PROCESS_DEBUG_PORT_CLASS,
        WINDOWS_ABI64_STATUS_SUCCESS,
        WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_query_process_write_image_name(
    u32 pid,
    u64 process_information,
    u64 return_length,
    u32 process_information_length,
    const persona_context_t *context,
    u64 rip)
{
    u32 source_chars;
    u32 source_bytes;
    u32 required_bytes;
    u32 index;

    source_chars = context->windows_image_path_ascii_bytes;
    if ((source_chars == 0u)
        || (source_chars > PERSONA64_WINDOWS_IMAGE_PATH_MAX_BYTES))
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_IMAGE_FILE_NAME_CLASS,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    source_bytes = source_chars * 2u;
    required_bytes = WINDOWS_ABI64_UNICODE_STRING_BYTES + source_bytes + 2u;
    if (windows_abi64_query_process_store_return_length(
            pid,
            return_length,
            required_bytes) == 0u)
    {
        ++g_windows_abi64_query_process_fault_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_IMAGE_FILE_NAME_CLASS,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            required_bytes,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (process_information_length < required_bytes)
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_IMAGE_FILE_NAME_CLASS,
            WINDOWS_ABI64_STATUS_INFO_LENGTH_MISMATCH,
            required_bytes,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((process_information == 0ull)
        || (windows_abi64_user_buffer_writable(
                pid,
                process_information,
                required_bytes) == 0u))
    {
        ++g_windows_abi64_query_process_fault_count;
        return windows_abi64_query_process_record_result(
            pid,
            WINDOWS_ABI64_PROCESS_IMAGE_FILE_NAME_CLASS,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            required_bytes,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    windows_abi64_store_u16(process_information, (u16)source_bytes);
    windows_abi64_store_u16(process_information + 2ull, (u16)(source_bytes + 2u));
    windows_abi64_store_u32(process_information + 4ull, 0u);
    windows_abi64_store_u64(
        process_information + 8ull,
        process_information + (u64)WINDOWS_ABI64_UNICODE_STRING_BYTES);
    for (index = 0u; index < source_chars; ++index)
    {
        volatile u8 *target = (volatile u8 *)(u64)(
            process_information
            + (u64)WINDOWS_ABI64_UNICODE_STRING_BYTES
            + ((u64)index * 2ull));

        target[0] = context->windows_image_path_ascii[index];
        target[1] = 0u;
    }
    windows_abi64_store_u16(
        process_information
            + (u64)WINDOWS_ABI64_UNICODE_STRING_BYTES
            + (u64)source_bytes,
        0u);

    ++g_windows_abi64_query_process_count;
    return windows_abi64_query_process_record_result(
        pid,
        WINDOWS_ABI64_PROCESS_IMAGE_FILE_NAME_CLASS,
        WINDOWS_ABI64_STATUS_SUCCESS,
        required_bytes,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntqueryinformationprocess_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 process_handle = rcx;
    u64 process_information_class = rdx;
    u64 process_information = r8;
    u64 process_information_length = r9;
    u64 return_length =
        ((stack_args != 0) && (stack_arg_count > 0u)) ? stack_args[0] : 0ull;
    persona_context_t *context;
    u32 info_class;
    u32 info_length;

    g_windows_abi64_query_process_last_peb = 0ull;
    g_windows_abi64_query_process_last_return_length = 0u;
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            0u,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if (windows_abi64_query_process_handle_is_current(pid, process_handle) == 0u)
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            0u,
            WINDOWS_ABI64_STATUS_INVALID_HANDLE,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((process_information_class > 0xFFFFFFFFull)
        || (process_information_length > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            0u,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    info_class = (u32)process_information_class;
    info_length = (u32)process_information_length;
    g_windows_abi64_query_process_last_class = info_class;

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->windows_peb_base == 0ull))
    {
        ++g_windows_abi64_query_process_denial_count;
        return windows_abi64_query_process_record_result(
            pid,
            info_class,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    g_windows_abi64_query_process_last_peb = context->windows_peb_base;

    if (info_class == WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_CLASS)
    {
        return windows_abi64_query_process_write_basic(
            pid,
            process_information,
            return_length,
            info_length,
            context,
            rip);
    }
    if (info_class == WINDOWS_ABI64_PROCESS_DEBUG_PORT_CLASS)
    {
        return windows_abi64_query_process_write_debug_port(
            pid,
            process_information,
            return_length,
            info_length,
            rip);
    }
    if (info_class == WINDOWS_ABI64_PROCESS_IMAGE_FILE_NAME_CLASS)
    {
        return windows_abi64_query_process_write_image_name(
            pid,
            process_information,
            return_length,
            info_length,
            context,
            rip);
    }

    ++g_windows_abi64_query_process_denial_count;
    return windows_abi64_query_process_record_result(
        pid,
        info_class,
        WINDOWS_ABI64_STATUS_INVALID_INFO_CLASS,
        0u,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static void windows_abi64_query_system_cpuid(
    u32 leaf,
    u32 subleaf,
    u32 *eax_out,
    u32 *ebx_out,
    u32 *ecx_out,
    u32 *edx_out)
{
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;

    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(subleaf));

    if (eax_out != 0)
    {
        *eax_out = eax;
    }
    if (ebx_out != 0)
    {
        *ebx_out = ebx;
    }
    if (ecx_out != 0)
    {
        *ecx_out = ecx;
    }
    if (edx_out != 0)
    {
        *edx_out = edx;
    }
}

static u32 windows_abi64_query_system_clamp_u64(u64 value)
{
    return (value > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (u32)value;
}

static u64 windows_abi64_query_system_physical_pages(void)
{
    return (g_windows_abi64_system_physical_pages != 0ull)
        ? g_windows_abi64_system_physical_pages
        : (u64)vma64_anon_total_pages();
}

static u64 windows_abi64_query_system_free_pages(void)
{
    u64 physical_pages = windows_abi64_query_system_physical_pages();
    u64 claimed_pages = (u64)vma64_anon_claimed_pages();

    return (physical_pages > claimed_pages) ? (physical_pages - claimed_pages) : 0ull;
}

static u32 windows_abi64_query_system_store_return_length(
    u32 pid,
    u64 return_length,
    u32 value)
{
    if (return_length == 0ull)
    {
        return 1u;
    }
    if (windows_abi64_user_buffer_writable(pid, return_length, 4u) == 0u)
    {
        return 0u;
    }

    windows_abi64_store_u32(return_length, value);
    return 1u;
}

static u32 windows_abi64_query_system_record_result(
    u32 pid,
    u32 info_class,
    u32 result,
    u32 return_length,
    u64 rip,
    u8 event_type)
{
    g_windows_abi64_query_system_last_class = info_class;
    g_windows_abi64_query_system_last_result = result;
    g_windows_abi64_query_system_last_return_length = return_length;
    (void)persona_audit64_record(
        pid,
        event_type,
        WINDOWS_ABI64_SYSCALL_NTQUERYSYSTEMINFORMATION,
        result,
        rip);
    return result;
}

static u32 windows_abi64_query_system_validate_output(
    u32 pid,
    u64 system_information,
    u64 return_length,
    u32 system_information_length,
    u32 required_length,
    u32 info_class,
    u64 rip)
{
    if (windows_abi64_query_system_store_return_length(
            pid,
            return_length,
            required_length) == 0u)
    {
        ++g_windows_abi64_query_system_fault_count;
        return windows_abi64_query_system_record_result(
            pid,
            info_class,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            required_length,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if (system_information_length < required_length)
    {
        ++g_windows_abi64_query_system_denial_count;
        return windows_abi64_query_system_record_result(
            pid,
            info_class,
            WINDOWS_ABI64_STATUS_INFO_LENGTH_MISMATCH,
            required_length,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }
    if ((system_information == 0ull)
        || (windows_abi64_user_buffer_writable(
                pid,
                system_information,
                required_length) == 0u))
    {
        ++g_windows_abi64_query_system_fault_count;
        return windows_abi64_query_system_record_result(
            pid,
            info_class,
            WINDOWS_ABI64_STATUS_ACCESS_VIOLATION,
            required_length,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    return WINDOWS_ABI64_STATUS_SUCCESS;
}

static u32 windows_abi64_query_system_write_basic(
    u32 pid,
    u64 system_information,
    u64 return_length,
    u32 system_information_length,
    u64 rip)
{
    u64 physical_pages = windows_abi64_query_system_physical_pages();
    u64 free_pages = windows_abi64_query_system_free_pages();
    u64 affinity;
    u32 processor_count = (g_windows_abi64_system_processor_count != 0u)
        ? g_windows_abi64_system_processor_count
        : 1u;
    u32 result;

    result = windows_abi64_query_system_validate_output(
        pid,
        system_information,
        return_length,
        system_information_length,
        WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_BYTES,
        WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_CLASS,
        rip);
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        return result;
    }

    affinity = (processor_count >= 64u)
        ? 0xFFFFFFFFFFFFFFFFull
        : ((1ull << processor_count) - 1ull);
    windows_abi64_zero_user(
        system_information,
        WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_BYTES);
    windows_abi64_store_u32(system_information, 0u);
    windows_abi64_store_u32(system_information + 4ull, 100000u);
    windows_abi64_store_u32(system_information + 8ull, VMA64_PAGE_BYTES);
    windows_abi64_store_u32(
        system_information + 12ull,
        windows_abi64_query_system_clamp_u64(physical_pages));
    windows_abi64_store_u32(system_information + 16ull, 0u);
    windows_abi64_store_u32(
        system_information + 20ull,
        (physical_pages != 0ull)
            ? windows_abi64_query_system_clamp_u64(physical_pages - 1ull)
            : 0u);
    windows_abi64_store_u32(
        system_information + 24ull,
        WINDOWS_ABI64_ALLOCATION_GRANULARITY_BYTES);
    windows_abi64_store_u64(system_information + 32ull, 0x0000000000010000ull);
    windows_abi64_store_u64(system_information + 40ull, 0x00007FFFFFFFFFFFull);
    windows_abi64_store_u64(system_information + 48ull, affinity);
    windows_abi64_store_u32(system_information + 56ull, processor_count);

    g_windows_abi64_query_system_last_page_size = VMA64_PAGE_BYTES;
    g_windows_abi64_query_system_last_processor_count = processor_count;
    g_windows_abi64_query_system_last_physical_pages =
        windows_abi64_query_system_clamp_u64(physical_pages);
    g_windows_abi64_query_system_last_free_pages =
        windows_abi64_query_system_clamp_u64(free_pages);
    ++g_windows_abi64_query_system_count;
    return windows_abi64_query_system_record_result(
        pid,
        WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_CLASS,
        WINDOWS_ABI64_STATUS_SUCCESS,
        WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_BYTES,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_query_system_write_processor(
    u32 pid,
    u64 system_information,
    u64 return_length,
    u32 system_information_length,
    u64 rip)
{
    u32 eax = 0u;
    u32 ecx = 0u;
    u32 edx = 0u;
    u32 level;
    u32 revision;
    u32 feature_bits;
    u32 processor_count = (g_windows_abi64_system_processor_count != 0u)
        ? g_windows_abi64_system_processor_count
        : 1u;
    u32 result;

    result = windows_abi64_query_system_validate_output(
        pid,
        system_information,
        return_length,
        system_information_length,
        WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_BYTES,
        WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_CLASS,
        rip);
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        return result;
    }

    windows_abi64_query_system_cpuid(1u, 0u, &eax, 0, &ecx, &edx);
    level = (eax >> 8) & 0xFu;
    if (level == 0u)
    {
        level = 6u;
    }
    revision = (((eax >> 16) & 0xFu) << 8) | ((eax >> 4) & 0xFu);
    feature_bits = edx ^ (ecx << 1u) ^ (ecx >> 31u);
    if (feature_bits == 0u)
    {
        feature_bits = 1u;
    }

    windows_abi64_zero_user(
        system_information,
        WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_BYTES);
    windows_abi64_store_u16(
        system_information,
        (u16)WINDOWS_ABI64_PROCESSOR_ARCHITECTURE_AMD64);
    windows_abi64_store_u16(system_information + 2ull, (u16)level);
    windows_abi64_store_u16(system_information + 4ull, (u16)revision);
    windows_abi64_store_u16(system_information + 6ull, (u16)processor_count);
    windows_abi64_store_u32(system_information + 8ull, feature_bits);

    g_windows_abi64_query_system_last_processor_count = processor_count;
    ++g_windows_abi64_query_system_count;
    return windows_abi64_query_system_record_result(
        pid,
        WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_CLASS,
        WINDOWS_ABI64_STATUS_SUCCESS,
        WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_BYTES,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_query_system_write_performance(
    u32 pid,
    u64 system_information,
    u64 return_length,
    u32 system_information_length,
    u64 rip)
{
    u64 physical_pages = windows_abi64_query_system_physical_pages();
    u64 free_pages = windows_abi64_query_system_free_pages();
    u64 committed_pages = (physical_pages > free_pages)
        ? (physical_pages - free_pages)
        : 0ull;
    u32 result;

    result = windows_abi64_query_system_validate_output(
        pid,
        system_information,
        return_length,
        system_information_length,
        WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_BYTES,
        WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_CLASS,
        rip);
    if (result != WINDOWS_ABI64_STATUS_SUCCESS)
    {
        return result;
    }

    windows_abi64_zero_user(
        system_information,
        WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_BYTES);
    windows_abi64_store_u32(
        system_information + 48ull,
        windows_abi64_query_system_clamp_u64(free_pages));
    windows_abi64_store_u32(
        system_information + 52ull,
        windows_abi64_query_system_clamp_u64(committed_pages));
    windows_abi64_store_u32(
        system_information + 56ull,
        windows_abi64_query_system_clamp_u64(physical_pages));
    windows_abi64_store_u32(
        system_information + 60ull,
        windows_abi64_query_system_clamp_u64(committed_pages));
    windows_abi64_store_u32(
        system_information + 64ull,
        vma64_anon_claimed_pages());
    windows_abi64_store_u32(
        system_information + 68ull,
        vma64_anon_free_pages());
    windows_abi64_store_u32(
        system_information + 72ull,
        windows_abi64_query_system_clamp_u64(physical_pages));
    windows_abi64_store_u32(system_information + 76ull, VMA64_PAGE_BYTES);

    g_windows_abi64_query_system_last_page_size = VMA64_PAGE_BYTES;
    g_windows_abi64_query_system_last_physical_pages =
        windows_abi64_query_system_clamp_u64(physical_pages);
    g_windows_abi64_query_system_last_free_pages =
        windows_abi64_query_system_clamp_u64(free_pages);
    ++g_windows_abi64_query_system_count;
    return windows_abi64_query_system_record_result(
        pid,
        WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_CLASS,
        WINDOWS_ABI64_STATUS_SUCCESS,
        WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_BYTES,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u32 windows_abi64_ntquerysysteminformation_dispatch(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    u64 system_information_class = rcx;
    u64 system_information = rdx;
    u64 system_information_length = r8;
    u64 return_length = r9;
    u32 info_class;
    u32 info_length;

    (void)stack_args;
    (void)stack_arg_count;

    g_windows_abi64_query_system_last_return_length = 0u;
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        ++g_windows_abi64_query_system_denial_count;
        return windows_abi64_query_system_record_result(
            pid,
            0u,
            WINDOWS_ABI64_STATUS_ACCESS_DENIED,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
    }
    if ((system_information_class > 0xFFFFFFFFull)
        || (system_information_length > 0xFFFFFFFFull))
    {
        ++g_windows_abi64_query_system_denial_count;
        return windows_abi64_query_system_record_result(
            pid,
            0u,
            WINDOWS_ABI64_STATUS_INVALID_PARAMETER,
            0u,
            rip,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
    }

    info_class = (u32)system_information_class;
    info_length = (u32)system_information_length;
    g_windows_abi64_query_system_last_class = info_class;

    if (info_class == WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_CLASS)
    {
        return windows_abi64_query_system_write_basic(
            pid,
            system_information,
            return_length,
            info_length,
            rip);
    }
    if (info_class == WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_CLASS)
    {
        return windows_abi64_query_system_write_processor(
            pid,
            system_information,
            return_length,
            info_length,
            rip);
    }
    if (info_class == WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_CLASS)
    {
        return windows_abi64_query_system_write_performance(
            pid,
            system_information,
            return_length,
            info_length,
            rip);
    }

    ++g_windows_abi64_query_system_denial_count;
    return windows_abi64_query_system_record_result(
        pid,
        info_class,
        WINDOWS_ABI64_STATUS_INVALID_INFO_CLASS,
        0u,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

void windows_abi64_init(void)
{
    u32 index;

    for (index = 0u; index < WINDOWS_ABI64_SYSCALL_LIMIT; ++index)
    {
        g_windows_abi64_dispatch_table[index] = windows_abi64_unimplemented_stub;
    }
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTREADFILE] =
        windows_abi64_ntreadfile_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTWRITEFILE] =
        windows_abi64_ntwritefile_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTSETINFORMATIONFILE] =
        windows_abi64_ntsetinformationfile_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCLOSE] =
        windows_abi64_ntclose_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONFILE] =
        windows_abi64_ntqueryinformationfile_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTOPENKEY] =
        windows_abi64_ntopenkey_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY] =
        windows_abi64_ntqueryvaluekey_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTALLOCATEVIRTUALMEMORY] =
        windows_abi64_ntallocatevirtualmemory_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONPROCESS] =
        windows_abi64_ntqueryinformationprocess_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEKEY] =
        windows_abi64_ntcreatekey_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTFREEVIRTUALMEMORY] =
        windows_abi64_ntfreevirtualmemory_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTTERMINATEPROCESS] =
        windows_abi64_ntterminateprocess_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTQUERYSYSTEMINFORMATION] =
        windows_abi64_ntquerysysteminformation_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTPROTECTVIRTUALMEMORY] =
        windows_abi64_ntprotectvirtualmemory_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEFILE] =
        windows_abi64_ntcreatefile_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT] =
        windows_abi64_ntcreatemutant_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT] =
        windows_abi64_ntreleasemutant_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT] =
        windows_abi64_ntwaitforsingleobject_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEEVENT] =
        windows_abi64_ntcreateevent_dispatch;
    g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTSETEVENT] =
        windows_abi64_ntsetevent_dispatch;

    windows_vfs64_init();
    windows_registry64_init();
    for (index = 0u; index < WINDOWS_ABI64_WAIT_TIMEOUT_RECORDS; ++index)
    {
        windows_abi64_clear_wait_timeout_record(
            &g_windows_abi64_wait_timeout_records[index]);
    }
    for (index = 0u; index < WINDOWS_ABI64_FILE_POSITION_RECORDS; ++index)
    {
        windows_abi64_clear_file_position_record(
            &g_windows_abi64_file_position_records[index]);
    }
    g_windows_abi64_dispatch_count = 0u;
    g_windows_abi64_unimplemented_count = 0u;
    g_windows_abi64_invalid_service_count = 0u;
    g_windows_abi64_last_pid = PROCESS64_INVALID_PID;
    g_windows_abi64_last_syscall = 0u;
    g_windows_abi64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_last_rip = 0ull;
    g_windows_abi64_read_count = 0u;
    g_windows_abi64_read_byte_count = 0u;
    g_windows_abi64_read_denial_count = 0u;
    g_windows_abi64_read_fault_count = 0u;
    g_windows_abi64_read_last_handle_low = 0u;
    g_windows_abi64_read_last_byte_count = 0u;
    g_windows_abi64_read_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_abi64_read_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_write_count = 0u;
    g_windows_abi64_write_byte_count = 0u;
    g_windows_abi64_write_denial_count = 0u;
    g_windows_abi64_write_fault_count = 0u;
    g_windows_abi64_write_last_handle_low = 0u;
    g_windows_abi64_write_last_byte_count = 0u;
    g_windows_abi64_write_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_abi64_write_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_allocate_count = 0u;
    g_windows_abi64_allocate_denial_count = 0u;
    g_windows_abi64_allocate_fault_count = 0u;
    g_windows_abi64_allocate_byte_count = 0u;
    g_windows_abi64_allocate_last_base = 0ull;
    g_windows_abi64_allocate_last_size = 0ull;
    g_windows_abi64_allocate_last_protect = 0u;
    g_windows_abi64_allocate_last_type = 0u;
    g_windows_abi64_allocate_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_free_count = 0u;
    g_windows_abi64_free_denial_count = 0u;
    g_windows_abi64_free_fault_count = 0u;
    g_windows_abi64_free_byte_count = 0u;
    g_windows_abi64_free_last_base = 0ull;
    g_windows_abi64_free_last_size = 0ull;
    g_windows_abi64_free_last_type = 0u;
    g_windows_abi64_free_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_protect_count = 0u;
    g_windows_abi64_protect_denial_count = 0u;
    g_windows_abi64_protect_fault_count = 0u;
    g_windows_abi64_protect_byte_count = 0u;
    g_windows_abi64_protect_last_base = 0ull;
    g_windows_abi64_protect_last_size = 0ull;
    g_windows_abi64_protect_last_new_protect = 0u;
    g_windows_abi64_protect_last_old_protect = 0u;
    g_windows_abi64_protect_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_create_count = 0u;
    g_windows_abi64_create_denial_count = 0u;
    g_windows_abi64_create_fault_count = 0u;
    g_windows_abi64_create_last_handle_low = 0u;
    g_windows_abi64_create_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_abi64_create_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_create_last_path_hash = 0u;
    g_windows_abi64_create_last_path_bytes = 0u;
    g_windows_abi64_create_last_shim_id = 0u;
    g_windows_abi64_query_file_count = 0u;
    g_windows_abi64_query_file_denial_count = 0u;
    g_windows_abi64_query_file_fault_count = 0u;
    g_windows_abi64_query_file_last_class = 0u;
    g_windows_abi64_query_file_last_handle_low = 0u;
    g_windows_abi64_query_file_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_query_file_last_return_length = 0u;
    g_windows_abi64_set_file_count = 0u;
    g_windows_abi64_set_file_denial_count = 0u;
    g_windows_abi64_set_file_fault_count = 0u;
    g_windows_abi64_set_file_last_class = 0u;
    g_windows_abi64_set_file_last_handle_low = 0u;
    g_windows_abi64_set_file_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_close_count = 0u;
    g_windows_abi64_close_denial_count = 0u;
    g_windows_abi64_close_last_handle_low = 0u;
    g_windows_abi64_close_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_terminate_count = 0u;
    g_windows_abi64_terminate_denial_count = 0u;
    g_windows_abi64_terminate_last_pid = PROCESS64_INVALID_PID;
    g_windows_abi64_terminate_last_status = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_terminate_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_event_create_count = 0u;
    g_windows_abi64_event_set_count = 0u;
    g_windows_abi64_event_wait_count = 0u;
    g_windows_abi64_event_denial_count = 0u;
    g_windows_abi64_event_fault_count = 0u;
    g_windows_abi64_event_last_handle_low = 0u;
    g_windows_abi64_event_last_previous_state = 0u;
    g_windows_abi64_event_last_state = 0u;
    g_windows_abi64_event_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_mutant_create_count = 0u;
    g_windows_abi64_mutant_wait_count = 0u;
    g_windows_abi64_mutant_release_count = 0u;
    g_windows_abi64_mutant_denial_count = 0u;
    g_windows_abi64_mutant_fault_count = 0u;
    g_windows_abi64_mutant_last_handle_low = 0u;
    g_windows_abi64_mutant_last_previous_count = 0u;
    g_windows_abi64_mutant_last_owner = PROCESS64_INVALID_PID;
    g_windows_abi64_mutant_last_count = 0u;
    g_windows_abi64_mutant_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_wait_timed_count = 0u;
    g_windows_abi64_wait_timeout_count = 0u;
    g_windows_abi64_wait_timeout_denial_count = 0u;
    g_windows_abi64_wait_last_timeout_task = SCHEDULER64_INVALID_TASK;
    g_windows_abi64_wait_last_timeout_ticks = 0u;
    g_windows_abi64_wait_last_timeout_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_wait_last_timeout_handle_low = 0u;
    g_windows_abi64_query_process_count = 0u;
    g_windows_abi64_query_process_denial_count = 0u;
    g_windows_abi64_query_process_fault_count = 0u;
    g_windows_abi64_query_process_last_class = 0u;
    g_windows_abi64_query_process_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_query_process_last_peb = 0ull;
    g_windows_abi64_query_process_last_return_length = 0u;
    g_windows_abi64_query_system_count = 0u;
    g_windows_abi64_query_system_denial_count = 0u;
    g_windows_abi64_query_system_fault_count = 0u;
    g_windows_abi64_query_system_last_class = 0u;
    g_windows_abi64_query_system_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_query_system_last_return_length = 0u;
    g_windows_abi64_query_system_last_page_size = VMA64_PAGE_BYTES;
    g_windows_abi64_query_system_last_processor_count =
        (g_windows_abi64_system_processor_count != 0u)
            ? g_windows_abi64_system_processor_count
            : 1u;
    g_windows_abi64_query_system_last_physical_pages =
        windows_abi64_query_system_clamp_u64(
            windows_abi64_query_system_physical_pages());
    g_windows_abi64_query_system_last_free_pages =
        windows_abi64_query_system_clamp_u64(
            windows_abi64_query_system_free_pages());
    g_windows_abi64_registry_open_count = 0u;
    g_windows_abi64_registry_create_count = 0u;
    g_windows_abi64_registry_query_count = 0u;
    g_windows_abi64_registry_denial_count = 0u;
    g_windows_abi64_registry_fault_count = 0u;
    g_windows_abi64_registry_last_syscall = 0u;
    g_windows_abi64_registry_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_abi64_registry_last_key_id = WINDOWS_REGISTRY64_KEY_NONE;
    g_windows_abi64_registry_last_required_bytes = 0u;
    g_windows_abi64_registry_last_value_type = 0u;
    g_windows_abi64_initialized = 1u;
}

windows_abi64_handler_t *windows_abi64_dispatch_table(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return g_windows_abi64_dispatch_table;
}

u32 windows_abi64_dispatch(
    u32 pid,
    u32 syscall_number,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip)
{
    windows_abi64_handler_t handler;
    u32 result;
    u64 unavailable_return;

    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    ++g_windows_abi64_dispatch_count;
    g_windows_abi64_last_pid = pid;
    g_windows_abi64_last_syscall = syscall_number;
    g_windows_abi64_last_rip = rip;

    if (syscall_number >= WINDOWS_ABI64_SYSCALL_LIMIT)
    {
        ++g_windows_abi64_invalid_service_count;
        result = WINDOWS_ABI64_STATUS_INVALID_SYSTEM_SERVICE;
        g_windows_abi64_last_result = result;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED,
            (u16)syscall_number,
            result,
            rip);
        return result;
    }

    handler = g_windows_abi64_dispatch_table[syscall_number];
    if (handler == 0)
    {
        handler = windows_abi64_unimplemented_stub;
    }

    if (handler == windows_abi64_unimplemented_stub)
    {
        ++g_windows_abi64_unimplemented_count;
        (void)persona64_record_unavailable_syscall(
            pid,
            PERSONA64_TYPE_WINDOWS_PE,
            (u16)syscall_number,
            rip,
            &result,
            &unavailable_return);
        g_windows_abi64_last_result = result;
        return (u32)unavailable_return;
    }

    result = handler(pid, rcx, rdx, r8, r9, stack_args, stack_arg_count, rip);
    g_windows_abi64_last_result = result;

    return result;
}

u32 windows_abi64_table_size(void)
{
    return WINDOWS_ABI64_SYSCALL_LIMIT;
}

u32 windows_abi64_table_ready(void)
{
    u32 index;

    if (g_windows_abi64_initialized == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < WINDOWS_ABI64_SYSCALL_LIMIT; ++index)
    {
        if (g_windows_abi64_dispatch_table[index] == 0)
        {
            return 0u;
        }
    }

    return 1u;
}

u32 windows_abi64_unimplemented_entry_count(void)
{
    u32 index;
    u32 count = 0u;

    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    for (index = 0u; index < WINDOWS_ABI64_SYSCALL_LIMIT; ++index)
    {
        if (g_windows_abi64_dispatch_table[index] == windows_abi64_unimplemented_stub)
        {
            ++count;
        }
    }

    return count;
}

u32 windows_abi64_entry_is_unimplemented(u32 syscall_number)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    if (syscall_number >= WINDOWS_ABI64_SYSCALL_LIMIT)
    {
        return 0u;
    }

    return (g_windows_abi64_dispatch_table[syscall_number]
        == windows_abi64_unimplemented_stub)
        ? 1u
        : 0u;
}

u32 windows_abi64_dispatch_count(void)
{
    return g_windows_abi64_dispatch_count;
}

u32 windows_abi64_unimplemented_count(void)
{
    return g_windows_abi64_unimplemented_count;
}

u32 windows_abi64_invalid_service_count(void)
{
    return g_windows_abi64_invalid_service_count;
}

u32 windows_abi64_last_pid(void)
{
    return g_windows_abi64_last_pid;
}

u32 windows_abi64_last_syscall(void)
{
    return g_windows_abi64_last_syscall;
}

u32 windows_abi64_last_result(void)
{
    return g_windows_abi64_last_result;
}

u64 windows_abi64_last_rip(void)
{
    return g_windows_abi64_last_rip;
}

u32 windows_abi64_read_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTREADFILE]
        == windows_abi64_ntreadfile_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_read_count(void)
{
    return g_windows_abi64_read_count;
}

u32 windows_abi64_read_byte_count(void)
{
    return g_windows_abi64_read_byte_count;
}

u32 windows_abi64_read_denial_count(void)
{
    return g_windows_abi64_read_denial_count;
}

u32 windows_abi64_read_fault_count(void)
{
    return g_windows_abi64_read_fault_count;
}

u32 windows_abi64_read_last_handle_low(void)
{
    return g_windows_abi64_read_last_handle_low;
}

u32 windows_abi64_read_last_byte_count(void)
{
    return g_windows_abi64_read_last_byte_count;
}

u32 windows_abi64_read_last_capability(void)
{
    return g_windows_abi64_read_last_capability;
}

u32 windows_abi64_read_last_result(void)
{
    return g_windows_abi64_read_last_result;
}

u32 windows_abi64_write_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTWRITEFILE]
        == windows_abi64_ntwritefile_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_write_count(void)
{
    return g_windows_abi64_write_count;
}

u32 windows_abi64_write_byte_count(void)
{
    return g_windows_abi64_write_byte_count;
}

u32 windows_abi64_write_denial_count(void)
{
    return g_windows_abi64_write_denial_count;
}

u32 windows_abi64_write_fault_count(void)
{
    return g_windows_abi64_write_fault_count;
}

u32 windows_abi64_write_last_handle_low(void)
{
    return g_windows_abi64_write_last_handle_low;
}

u32 windows_abi64_write_last_byte_count(void)
{
    return g_windows_abi64_write_last_byte_count;
}

u32 windows_abi64_write_last_capability(void)
{
    return g_windows_abi64_write_last_capability;
}

u32 windows_abi64_write_last_result(void)
{
    return g_windows_abi64_write_last_result;
}

u32 windows_abi64_allocate_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[
        WINDOWS_ABI64_SYSCALL_NTALLOCATEVIRTUALMEMORY]
        == windows_abi64_ntallocatevirtualmemory_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_allocate_count(void)
{
    return g_windows_abi64_allocate_count;
}

u32 windows_abi64_allocate_denial_count(void)
{
    return g_windows_abi64_allocate_denial_count;
}

u32 windows_abi64_allocate_fault_count(void)
{
    return g_windows_abi64_allocate_fault_count;
}

u32 windows_abi64_allocate_byte_count(void)
{
    return g_windows_abi64_allocate_byte_count;
}

u64 windows_abi64_allocate_last_base(void)
{
    return g_windows_abi64_allocate_last_base;
}

u64 windows_abi64_allocate_last_size(void)
{
    return g_windows_abi64_allocate_last_size;
}

u32 windows_abi64_allocate_last_protect(void)
{
    return g_windows_abi64_allocate_last_protect;
}

u32 windows_abi64_allocate_last_type(void)
{
    return g_windows_abi64_allocate_last_type;
}

u32 windows_abi64_allocate_last_result(void)
{
    return g_windows_abi64_allocate_last_result;
}

u32 windows_abi64_free_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[
        WINDOWS_ABI64_SYSCALL_NTFREEVIRTUALMEMORY]
        == windows_abi64_ntfreevirtualmemory_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_free_count(void)
{
    return g_windows_abi64_free_count;
}

u32 windows_abi64_free_denial_count(void)
{
    return g_windows_abi64_free_denial_count;
}

u32 windows_abi64_free_fault_count(void)
{
    return g_windows_abi64_free_fault_count;
}

u32 windows_abi64_free_byte_count(void)
{
    return g_windows_abi64_free_byte_count;
}

u64 windows_abi64_free_last_base(void)
{
    return g_windows_abi64_free_last_base;
}

u64 windows_abi64_free_last_size(void)
{
    return g_windows_abi64_free_last_size;
}

u32 windows_abi64_free_last_type(void)
{
    return g_windows_abi64_free_last_type;
}

u32 windows_abi64_free_last_result(void)
{
    return g_windows_abi64_free_last_result;
}

u32 windows_abi64_protect_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[
        WINDOWS_ABI64_SYSCALL_NTPROTECTVIRTUALMEMORY]
        == windows_abi64_ntprotectvirtualmemory_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_protect_count(void)
{
    return g_windows_abi64_protect_count;
}

u32 windows_abi64_protect_denial_count(void)
{
    return g_windows_abi64_protect_denial_count;
}

u32 windows_abi64_protect_fault_count(void)
{
    return g_windows_abi64_protect_fault_count;
}

u32 windows_abi64_protect_byte_count(void)
{
    return g_windows_abi64_protect_byte_count;
}

u64 windows_abi64_protect_last_base(void)
{
    return g_windows_abi64_protect_last_base;
}

u64 windows_abi64_protect_last_size(void)
{
    return g_windows_abi64_protect_last_size;
}

u32 windows_abi64_protect_last_new_protect(void)
{
    return g_windows_abi64_protect_last_new_protect;
}

u32 windows_abi64_protect_last_old_protect(void)
{
    return g_windows_abi64_protect_last_old_protect;
}

u32 windows_abi64_protect_last_result(void)
{
    return g_windows_abi64_protect_last_result;
}

u32 windows_abi64_wait_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[
        WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT]
        == windows_abi64_ntwaitforsingleobject_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_wait_timed_count(void)
{
    return g_windows_abi64_wait_timed_count;
}

u32 windows_abi64_wait_timeout_count(void)
{
    return g_windows_abi64_wait_timeout_count;
}

u32 windows_abi64_wait_timeout_denial_count(void)
{
    return g_windows_abi64_wait_timeout_denial_count;
}

u32 windows_abi64_wait_last_timeout_task(void)
{
    return g_windows_abi64_wait_last_timeout_task;
}

u32 windows_abi64_wait_last_timeout_ticks(void)
{
    return g_windows_abi64_wait_last_timeout_ticks;
}

u32 windows_abi64_wait_last_timeout_result(void)
{
    return g_windows_abi64_wait_last_timeout_result;
}

u32 windows_abi64_wait_last_timeout_handle_low(void)
{
    return g_windows_abi64_wait_last_timeout_handle_low;
}

u32 windows_abi64_create_event_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEEVENT]
        == windows_abi64_ntcreateevent_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_set_event_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTSETEVENT]
        == windows_abi64_ntsetevent_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_event_create_count(void)
{
    return g_windows_abi64_event_create_count;
}

u32 windows_abi64_event_set_count(void)
{
    return g_windows_abi64_event_set_count;
}

u32 windows_abi64_event_wait_count(void)
{
    return g_windows_abi64_event_wait_count;
}

u32 windows_abi64_event_denial_count(void)
{
    return g_windows_abi64_event_denial_count;
}

u32 windows_abi64_event_fault_count(void)
{
    return g_windows_abi64_event_fault_count;
}

u32 windows_abi64_event_last_handle_low(void)
{
    return g_windows_abi64_event_last_handle_low;
}

u32 windows_abi64_event_last_previous_state(void)
{
    return g_windows_abi64_event_last_previous_state;
}

u32 windows_abi64_event_last_state(void)
{
    return g_windows_abi64_event_last_state;
}

u32 windows_abi64_event_last_result(void)
{
    return g_windows_abi64_event_last_result;
}

u32 windows_abi64_create_mutant_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT]
        == windows_abi64_ntcreatemutant_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_release_mutant_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT]
        == windows_abi64_ntreleasemutant_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_mutant_create_count(void)
{
    return g_windows_abi64_mutant_create_count;
}

u32 windows_abi64_mutant_wait_count(void)
{
    return g_windows_abi64_mutant_wait_count;
}

u32 windows_abi64_mutant_release_count(void)
{
    return g_windows_abi64_mutant_release_count;
}

u32 windows_abi64_mutant_denial_count(void)
{
    return g_windows_abi64_mutant_denial_count;
}

u32 windows_abi64_mutant_fault_count(void)
{
    return g_windows_abi64_mutant_fault_count;
}

u32 windows_abi64_mutant_last_handle_low(void)
{
    return g_windows_abi64_mutant_last_handle_low;
}

u32 windows_abi64_mutant_last_previous_count(void)
{
    return g_windows_abi64_mutant_last_previous_count;
}

u32 windows_abi64_mutant_last_owner(void)
{
    return g_windows_abi64_mutant_last_owner;
}

u32 windows_abi64_mutant_last_count(void)
{
    return g_windows_abi64_mutant_last_count;
}

u32 windows_abi64_mutant_last_result(void)
{
    return g_windows_abi64_mutant_last_result;
}

u32 windows_abi64_query_process_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[
        WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONPROCESS]
        == windows_abi64_ntqueryinformationprocess_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_query_process_count(void)
{
    return g_windows_abi64_query_process_count;
}

u32 windows_abi64_query_process_denial_count(void)
{
    return g_windows_abi64_query_process_denial_count;
}

u32 windows_abi64_query_process_fault_count(void)
{
    return g_windows_abi64_query_process_fault_count;
}

u32 windows_abi64_query_process_last_class(void)
{
    return g_windows_abi64_query_process_last_class;
}

u32 windows_abi64_query_process_last_result(void)
{
    return g_windows_abi64_query_process_last_result;
}

u64 windows_abi64_query_process_last_peb(void)
{
    return g_windows_abi64_query_process_last_peb;
}

u32 windows_abi64_query_process_last_return_length(void)
{
    return g_windows_abi64_query_process_last_return_length;
}

u32 windows_abi64_query_system_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[
        WINDOWS_ABI64_SYSCALL_NTQUERYSYSTEMINFORMATION]
        == windows_abi64_ntquerysysteminformation_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_query_system_count(void)
{
    return g_windows_abi64_query_system_count;
}

u32 windows_abi64_query_system_denial_count(void)
{
    return g_windows_abi64_query_system_denial_count;
}

u32 windows_abi64_query_system_fault_count(void)
{
    return g_windows_abi64_query_system_fault_count;
}

u32 windows_abi64_query_system_last_class(void)
{
    return g_windows_abi64_query_system_last_class;
}

u32 windows_abi64_query_system_last_result(void)
{
    return g_windows_abi64_query_system_last_result;
}

u32 windows_abi64_query_system_last_return_length(void)
{
    return g_windows_abi64_query_system_last_return_length;
}

u32 windows_abi64_query_system_last_page_size(void)
{
    return g_windows_abi64_query_system_last_page_size;
}

u32 windows_abi64_query_system_last_processor_count(void)
{
    return g_windows_abi64_query_system_last_processor_count;
}

u32 windows_abi64_query_system_last_physical_pages(void)
{
    return g_windows_abi64_query_system_last_physical_pages;
}

u32 windows_abi64_query_system_last_free_pages(void)
{
    return g_windows_abi64_query_system_last_free_pages;
}

u32 windows_abi64_open_key_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTOPENKEY]
        == windows_abi64_ntopenkey_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_create_key_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEKEY]
        == windows_abi64_ntcreatekey_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_query_value_key_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY]
        == windows_abi64_ntqueryvaluekey_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_registry_open_count(void)
{
    return g_windows_abi64_registry_open_count;
}

u32 windows_abi64_registry_create_count(void)
{
    return g_windows_abi64_registry_create_count;
}

u32 windows_abi64_registry_query_count(void)
{
    return g_windows_abi64_registry_query_count;
}

u32 windows_abi64_registry_denial_count(void)
{
    return g_windows_abi64_registry_denial_count;
}

u32 windows_abi64_registry_fault_count(void)
{
    return g_windows_abi64_registry_fault_count;
}

u32 windows_abi64_registry_last_syscall(void)
{
    return g_windows_abi64_registry_last_syscall;
}

u32 windows_abi64_registry_last_result(void)
{
    return g_windows_abi64_registry_last_result;
}

u32 windows_abi64_registry_last_key_id(void)
{
    return g_windows_abi64_registry_last_key_id;
}

u32 windows_abi64_registry_last_required_bytes(void)
{
    return g_windows_abi64_registry_last_required_bytes;
}

u32 windows_abi64_registry_last_value_type(void)
{
    return g_windows_abi64_registry_last_value_type;
}

u32 windows_abi64_create_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCREATEFILE]
        == windows_abi64_ntcreatefile_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_create_count(void)
{
    return g_windows_abi64_create_count;
}

u32 windows_abi64_create_denial_count(void)
{
    return g_windows_abi64_create_denial_count;
}

u32 windows_abi64_create_fault_count(void)
{
    return g_windows_abi64_create_fault_count;
}

u32 windows_abi64_create_last_handle_low(void)
{
    return g_windows_abi64_create_last_handle_low;
}

u32 windows_abi64_create_last_capability(void)
{
    return g_windows_abi64_create_last_capability;
}

u32 windows_abi64_create_last_result(void)
{
    return g_windows_abi64_create_last_result;
}

u32 windows_abi64_create_last_path_hash(void)
{
    return g_windows_abi64_create_last_path_hash;
}

u32 windows_abi64_create_last_path_bytes(void)
{
    return g_windows_abi64_create_last_path_bytes;
}

u32 windows_abi64_create_last_shim_id(void)
{
    return g_windows_abi64_create_last_shim_id;
}

u32 windows_abi64_query_file_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[
        WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONFILE]
        == windows_abi64_ntqueryinformationfile_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_query_file_count(void)
{
    return g_windows_abi64_query_file_count;
}

u32 windows_abi64_query_file_denial_count(void)
{
    return g_windows_abi64_query_file_denial_count;
}

u32 windows_abi64_query_file_fault_count(void)
{
    return g_windows_abi64_query_file_fault_count;
}

u32 windows_abi64_query_file_last_class(void)
{
    return g_windows_abi64_query_file_last_class;
}

u32 windows_abi64_query_file_last_handle_low(void)
{
    return g_windows_abi64_query_file_last_handle_low;
}

u32 windows_abi64_query_file_last_result(void)
{
    return g_windows_abi64_query_file_last_result;
}

u32 windows_abi64_query_file_last_return_length(void)
{
    return g_windows_abi64_query_file_last_return_length;
}

u32 windows_abi64_set_file_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTSETINFORMATIONFILE]
        == windows_abi64_ntsetinformationfile_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_set_file_count(void)
{
    return g_windows_abi64_set_file_count;
}

u32 windows_abi64_set_file_denial_count(void)
{
    return g_windows_abi64_set_file_denial_count;
}

u32 windows_abi64_set_file_fault_count(void)
{
    return g_windows_abi64_set_file_fault_count;
}

u32 windows_abi64_set_file_last_class(void)
{
    return g_windows_abi64_set_file_last_class;
}

u32 windows_abi64_set_file_last_handle_low(void)
{
    return g_windows_abi64_set_file_last_handle_low;
}

u32 windows_abi64_set_file_last_result(void)
{
    return g_windows_abi64_set_file_last_result;
}

u32 windows_abi64_close_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTCLOSE]
        == windows_abi64_ntclose_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_close_count(void)
{
    return g_windows_abi64_close_count;
}

u32 windows_abi64_close_denial_count(void)
{
    return g_windows_abi64_close_denial_count;
}

u32 windows_abi64_close_last_handle_low(void)
{
    return g_windows_abi64_close_last_handle_low;
}

u32 windows_abi64_close_last_result(void)
{
    return g_windows_abi64_close_last_result;
}

u32 windows_abi64_terminate_entry_installed(void)
{
    if (g_windows_abi64_initialized == 0u)
    {
        windows_abi64_init();
    }

    return (g_windows_abi64_dispatch_table[WINDOWS_ABI64_SYSCALL_NTTERMINATEPROCESS]
        == windows_abi64_ntterminateprocess_dispatch)
        ? 1u
        : 0u;
}

u32 windows_abi64_terminate_count(void)
{
    return g_windows_abi64_terminate_count;
}

u32 windows_abi64_terminate_denial_count(void)
{
    return g_windows_abi64_terminate_denial_count;
}

u32 windows_abi64_terminate_last_pid(void)
{
    return g_windows_abi64_terminate_last_pid;
}

u32 windows_abi64_terminate_last_status(void)
{
    return g_windows_abi64_terminate_last_status;
}

u32 windows_abi64_terminate_last_result(void)
{
    return g_windows_abi64_terminate_last_result;
}
