#include "windows_handle_x64.h"

#include "fs_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "scheduler_x64.h"
#include "services.h"
#include "windows_abi_x64.h"

/*
 * K.4 adds the Windows NT object handle table substrate. It attaches a
 * per-process handle table through the Windows persona context, maps NT-style
 * integer handles to scoped LimitlessOS capability handles, supports duplicate
 * and inheritable handle copies, recognizes current-process/current-thread
 * pseudo-handles, and revokes capability handles only when the last table entry
 * for that capability closes. The scaffold checkpoint proves valid handle
 * creation, duplication, inheritance, pseudo-handle resolution, close/revoke
 * behavior, and denied invalid/protected operations without NtCreateFile
 * claiming success before K.5 exists.
 */

static windows_handle64_table_t g_windows_handle64_tables[WINDOWS_HANDLE64_MAX_TABLES];
static u32 g_windows_handle64_table_used[WINDOWS_HANDLE64_MAX_TABLES];
typedef struct windows_handle64_event_object
{
    u32 active;
    u32 pid;
    u32 manual_reset;
    u32 signaled;
    u32 set_count;
    u32 wait_count;
    u32 waiting_pid;
    u32 waiting_task_id;
} windows_handle64_event_object_t;

typedef struct windows_handle64_mutant_object
{
    u32 active;
    u32 creator_pid;
    u32 owner_pid;
    u32 recursion_count;
    u32 wait_count;
    u32 release_count;
    u32 waiting_pid;
    u32 waiting_task_id;
} windows_handle64_mutant_object_t;

static windows_handle64_event_object_t
    g_windows_handle64_events[WINDOWS_HANDLE64_MAX_EVENTS];
static windows_handle64_mutant_object_t
    g_windows_handle64_mutants[WINDOWS_HANDLE64_MAX_MUTANTS];
static u32 g_windows_handle64_initialized = 0u;
static u32 g_windows_handle64_install_count = 0u;
static u32 g_windows_handle64_duplicate_count = 0u;
static u32 g_windows_handle64_inherit_count = 0u;
static u32 g_windows_handle64_close_count = 0u;
static u32 g_windows_handle64_pseudo_count = 0u;
static u32 g_windows_handle64_global_denial_count = 0u;
static u32 g_windows_handle64_event_create_count = 0u;
static u32 g_windows_handle64_event_set_count = 0u;
static u32 g_windows_handle64_event_wait_count = 0u;
static u32 g_windows_handle64_event_denial_count = 0u;
static u32 g_windows_handle64_event_live_count = 0u;
static u64 g_windows_handle64_event_last_handle = WINDOWS_HANDLE64_INVALID;
static u32 g_windows_handle64_event_last_state = 0u;
static u32 g_windows_handle64_mutant_create_count = 0u;
static u32 g_windows_handle64_mutant_wait_count = 0u;
static u32 g_windows_handle64_mutant_release_count = 0u;
static u32 g_windows_handle64_mutant_denial_count = 0u;
static u32 g_windows_handle64_mutant_live_count = 0u;
static u64 g_windows_handle64_mutant_last_handle = WINDOWS_HANDLE64_INVALID;
static u32 g_windows_handle64_mutant_last_owner = PROCESS64_INVALID_PID;
static u32 g_windows_handle64_mutant_last_count = 0u;
static u64 g_windows_handle64_last_handle = WINDOWS_HANDLE64_INVALID;
static u32 g_windows_handle64_last_capability = CAPABILITY64_INVALID_HANDLE;
static u32 g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;

static void windows_handle64_clear_event(windows_handle64_event_object_t *event)
{
    if (event == 0)
    {
        return;
    }

    event->active = 0u;
    event->pid = PROCESS64_INVALID_PID;
    event->manual_reset = 0u;
    event->signaled = 0u;
    event->set_count = 0u;
    event->wait_count = 0u;
    event->waiting_pid = PROCESS64_INVALID_PID;
    event->waiting_task_id = SCHEDULER64_INVALID_TASK;
}

static void windows_handle64_clear_mutant(windows_handle64_mutant_object_t *mutant)
{
    if (mutant == 0)
    {
        return;
    }

    mutant->active = 0u;
    mutant->creator_pid = PROCESS64_INVALID_PID;
    mutant->owner_pid = PROCESS64_INVALID_PID;
    mutant->recursion_count = 0u;
    mutant->wait_count = 0u;
    mutant->release_count = 0u;
    mutant->waiting_pid = PROCESS64_INVALID_PID;
    mutant->waiting_task_id = SCHEDULER64_INVALID_TASK;
}

static void windows_handle64_clear_entry(windows_handle64_entry_t *entry)
{
    if (entry == 0)
    {
        return;
    }

    entry->handle = WINDOWS_HANDLE64_INVALID;
    entry->capability_handle = CAPABILITY64_INVALID_HANDLE;
    entry->object_type = WINDOWS_HANDLE64_TYPE_EMPTY;
    entry->rights = 0u;
    entry->flags = 0u;
    entry->ref_count = 0u;
    entry->reserved = 0u;
}

static void windows_handle64_clear_table(windows_handle64_table_t *table)
{
    u32 index;

    if (table == 0)
    {
        return;
    }

    table->pid = PROCESS64_INVALID_PID;
    table->owner_id = 0u;
    table->live_count = 0u;
    table->denial_count = 0u;
    table->high_water_handle = WINDOWS_HANDLE64_INVALID;
    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        windows_handle64_clear_entry(&table->entries[index]);
    }
}

static u32 windows_handle64_valid_type(u32 object_type)
{
    return ((object_type == WINDOWS_HANDLE64_TYPE_FILE)
        || (object_type == WINDOWS_HANDLE64_TYPE_DEVICE)
        || (object_type == WINDOWS_HANDLE64_TYPE_EVENT)
        || (object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
        || (object_type == WINDOWS_HANDLE64_TYPE_PROCESS)
        || (object_type == WINDOWS_HANDLE64_TYPE_THREAD)
        || (object_type == WINDOWS_HANDLE64_TYPE_KEY))
        ? 1u
        : 0u;
}

static void windows_handle64_note_denial(u32 pid, u32 result)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);

    ++g_windows_handle64_global_denial_count;
    g_windows_handle64_last_result = result;
    if (table != 0)
    {
        ++table->denial_count;
    }
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
        WINDOWS_HANDLE64_AUDIT_HANDLE_TABLE,
        result,
        0ull);
}

static u32 windows_handle64_entry_active(const windows_handle64_entry_t *entry)
{
    return ((entry != 0)
        && (entry->handle != WINDOWS_HANDLE64_INVALID)
        && (entry->capability_handle != CAPABILITY64_INVALID_HANDLE)
        && (entry->object_type != WINDOWS_HANDLE64_TYPE_EMPTY)
        && (entry->ref_count != 0u))
        ? 1u
        : 0u;
}

static u64 windows_handle64_slot_handle(u32 slot)
{
    return WINDOWS_HANDLE64_FIRST_DYNAMIC + ((u64)slot * WINDOWS_HANDLE64_GRANULARITY);
}

static windows_handle64_entry_t *windows_handle64_find_entry(
    windows_handle64_table_t *table,
    u64 handle)
{
    u32 index;

    if ((table == 0) || (handle == WINDOWS_HANDLE64_INVALID))
    {
        return 0;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if ((windows_handle64_entry_active(&table->entries[index]) != 0u)
            && (table->entries[index].handle == handle))
        {
            return &table->entries[index];
        }
    }

    return 0;
}

static u32 windows_handle64_shared_entry_count(
    windows_handle64_table_t *table,
    u32 capability_handle,
    u32 object_type)
{
    u32 index;
    u32 count = 0u;

    if (table == 0)
    {
        return 0u;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if ((windows_handle64_entry_active(&table->entries[index]) != 0u)
            && (table->entries[index].capability_handle == capability_handle)
            && (table->entries[index].object_type == object_type))
        {
            ++count;
        }
    }

    return count;
}

static u32 windows_handle64_global_shared_entry_count(
    u32 capability_handle,
    u32 object_type)
{
    u32 table_index;
    u32 entry_index;
    u32 count = 0u;

    for (table_index = 0u; table_index < WINDOWS_HANDLE64_MAX_TABLES; ++table_index)
    {
        if (g_windows_handle64_table_used[table_index] == 0u)
        {
            continue;
        }

        for (entry_index = 0u; entry_index < WINDOWS_HANDLE64_TABLE_LIMIT; ++entry_index)
        {
            if ((windows_handle64_entry_active(
                    &g_windows_handle64_tables[table_index].entries[entry_index]) != 0u)
                && (g_windows_handle64_tables[table_index].entries[entry_index]
                    .capability_handle == capability_handle)
                && (g_windows_handle64_tables[table_index].entries[entry_index].object_type
                    == object_type))
            {
                ++count;
            }
        }
    }

    return count;
}

static u32 windows_handle64_global_event_entry_count(u32 event_token)
{
    u32 table_index;
    u32 entry_index;
    u32 count = 0u;

    if ((event_token == 0u) || (event_token > WINDOWS_HANDLE64_MAX_EVENTS))
    {
        return 0u;
    }

    for (table_index = 0u; table_index < WINDOWS_HANDLE64_MAX_TABLES; ++table_index)
    {
        if (g_windows_handle64_table_used[table_index] == 0u)
        {
            continue;
        }

        for (entry_index = 0u; entry_index < WINDOWS_HANDLE64_TABLE_LIMIT; ++entry_index)
        {
            if ((windows_handle64_entry_active(
                    &g_windows_handle64_tables[table_index].entries[entry_index]) != 0u)
                && (g_windows_handle64_tables[table_index].entries[entry_index]
                    .object_type == WINDOWS_HANDLE64_TYPE_EVENT)
                && (g_windows_handle64_tables[table_index].entries[entry_index].reserved
                    == event_token))
            {
                ++count;
            }
        }
    }

    return count;
}

static u32 windows_handle64_global_mutant_entry_count(u32 mutant_token)
{
    u32 table_index;
    u32 entry_index;
    u32 count = 0u;

    if ((mutant_token == 0u) || (mutant_token > WINDOWS_HANDLE64_MAX_MUTANTS))
    {
        return 0u;
    }

    for (table_index = 0u; table_index < WINDOWS_HANDLE64_MAX_TABLES; ++table_index)
    {
        if (g_windows_handle64_table_used[table_index] == 0u)
        {
            continue;
        }

        for (entry_index = 0u; entry_index < WINDOWS_HANDLE64_TABLE_LIMIT; ++entry_index)
        {
            if ((windows_handle64_entry_active(
                    &g_windows_handle64_tables[table_index].entries[entry_index]) != 0u)
                && (g_windows_handle64_tables[table_index].entries[entry_index]
                    .object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
                && (g_windows_handle64_tables[table_index].entries[entry_index].reserved
                    == mutant_token))
            {
                ++count;
            }
        }
    }

    return count;
}

static void windows_handle64_release_event_if_unreferenced(u32 event_token)
{
    windows_handle64_event_object_t *event;

    if ((event_token == 0u)
        || (event_token > WINDOWS_HANDLE64_MAX_EVENTS)
        || (windows_handle64_global_event_entry_count(event_token) != 0u))
    {
        return;
    }

    event = &g_windows_handle64_events[event_token - 1u];
    if (event->active != 0u)
    {
        windows_handle64_clear_event(event);
        if (g_windows_handle64_event_live_count != 0u)
        {
            --g_windows_handle64_event_live_count;
        }
    }
}

static void windows_handle64_release_mutant_if_unreferenced(u32 mutant_token)
{
    windows_handle64_mutant_object_t *mutant;

    if ((mutant_token == 0u)
        || (mutant_token > WINDOWS_HANDLE64_MAX_MUTANTS)
        || (windows_handle64_global_mutant_entry_count(mutant_token) != 0u))
    {
        return;
    }

    mutant = &g_windows_handle64_mutants[mutant_token - 1u];
    if (mutant->active != 0u)
    {
        windows_handle64_clear_mutant(mutant);
        if (g_windows_handle64_mutant_live_count != 0u)
        {
            --g_windows_handle64_mutant_live_count;
        }
    }
}

static u32 windows_handle64_backing_is_valid(
    u32 capability_handle,
    u32 owner_id,
    u32 object_type,
    u32 rights)
{
    if ((object_type == WINDOWS_HANDLE64_TYPE_FILE)
        && (fs64_handle_is_node(capability_handle, owner_id) != 0u))
    {
        return 1u;
    }

    return ((capability64_owner(capability_handle, owner_id) == owner_id)
        && ((capability64_rights(capability_handle, owner_id) & rights) == rights))
        ? 1u
        : 0u;
}

static void windows_handle64_revoke_backing(
    u32 capability_handle,
    u32 owner_id,
    u32 object_type)
{
    if ((object_type == WINDOWS_HANDLE64_TYPE_FILE)
        && (fs64_handle_is_node(capability_handle, owner_id) != 0u))
    {
        (void)fs64_revoke(capability_handle, owner_id);
        return;
    }

    (void)capability64_revoke(capability_handle, owner_id);
}

static void windows_handle64_note_event_denial(u32 result)
{
    ++g_windows_handle64_event_denial_count;
    g_windows_handle64_last_result = result;
}

static void windows_handle64_note_mutant_denial(u32 result)
{
    ++g_windows_handle64_mutant_denial_count;
    g_windows_handle64_last_result = result;
}

static windows_handle64_event_object_t *windows_handle64_event_for_handle(
    u32 pid,
    u64 handle,
    u32 required_right)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);
    windows_handle64_entry_t *entry;
    u32 event_token;

    if ((table == 0) || (required_right == 0u))
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0;
    }

    entry = windows_handle64_find_entry(table, handle);
    if ((entry == 0)
        || (entry->object_type != WINDOWS_HANDLE64_TYPE_EVENT)
        || ((entry->rights & required_right) != required_right)
        || (capability64_route(entry->capability_handle, required_right, table->owner_id)
            == CAPABILITY64_INVALID_HANDLE))
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0;
    }

    event_token = entry->reserved;
    if ((event_token == 0u)
        || (event_token > WINDOWS_HANDLE64_MAX_EVENTS)
        || (g_windows_handle64_events[event_token - 1u].active == 0u))
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0;
    }

    g_windows_handle64_event_last_handle = handle;
    return &g_windows_handle64_events[event_token - 1u];
}

static windows_handle64_mutant_object_t *windows_handle64_mutant_for_handle(
    u32 pid,
    u64 handle,
    u32 required_right)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);
    windows_handle64_entry_t *entry;
    u32 mutant_token;

    if ((table == 0) || (required_right == 0u))
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0;
    }

    entry = windows_handle64_find_entry(table, handle);
    if ((entry == 0)
        || (entry->object_type != WINDOWS_HANDLE64_TYPE_MUTANT)
        || ((entry->rights & required_right) != required_right)
        || (capability64_route(entry->capability_handle, required_right, table->owner_id)
            == CAPABILITY64_INVALID_HANDLE))
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0;
    }

    mutant_token = entry->reserved;
    if ((mutant_token == 0u)
        || (mutant_token > WINDOWS_HANDLE64_MAX_MUTANTS)
        || (g_windows_handle64_mutants[mutant_token - 1u].active == 0u))
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0;
    }

    g_windows_handle64_mutant_last_handle = handle;
    return &g_windows_handle64_mutants[mutant_token - 1u];
}

static void windows_handle64_sync_ref_counts(
    windows_handle64_table_t *table,
    u32 capability_handle,
    u32 object_type)
{
    u32 index;
    u32 count = windows_handle64_shared_entry_count(table, capability_handle, object_type);

    if ((table == 0) || (count == 0u))
    {
        return;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if ((windows_handle64_entry_active(&table->entries[index]) != 0u)
            && (table->entries[index].capability_handle == capability_handle)
            && (table->entries[index].object_type == object_type))
        {
            table->entries[index].ref_count = count;
        }
    }
}

static u32 windows_handle64_prior_shared_entry_exists(
    windows_handle64_table_t *table,
    u32 slot)
{
    u32 index;
    windows_handle64_entry_t *entry;

    if ((table == 0) || (slot >= WINDOWS_HANDLE64_TABLE_LIMIT))
    {
        return 0u;
    }

    entry = &table->entries[slot];
    if (windows_handle64_entry_active(entry) == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < slot; ++index)
    {
        if ((windows_handle64_entry_active(&table->entries[index]) != 0u)
            && (table->entries[index].capability_handle == entry->capability_handle)
            && (table->entries[index].object_type == entry->object_type))
        {
            return 1u;
        }
    }

    return 0u;
}

static u64 windows_handle64_install_in_table(
    windows_handle64_table_t *table,
    u32 capability_handle,
    u32 object_type,
    u32 rights,
    u32 flags)
{
    u32 index;
    u64 handle;

    if ((table == 0)
        || (capability_handle == CAPABILITY64_INVALID_HANDLE)
        || (windows_handle64_valid_type(object_type) == 0u)
        || (rights == 0u)
        || ((flags & ~WINDOWS_HANDLE64_FLAGS_VALID) != 0u))
    {
        return WINDOWS_HANDLE64_INVALID;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if (windows_handle64_entry_active(&table->entries[index]) == 0u)
        {
            handle = windows_handle64_slot_handle(index);
            table->entries[index].handle = handle;
            table->entries[index].capability_handle = capability_handle;
            table->entries[index].object_type = object_type;
            table->entries[index].rights = rights;
            table->entries[index].flags = flags;
            table->entries[index].ref_count = 1u;
            table->entries[index].reserved = 0u;
            ++table->live_count;
            if (handle > table->high_water_handle)
            {
                table->high_water_handle = handle;
            }
            windows_handle64_sync_ref_counts(table, capability_handle, object_type);
            return handle;
        }
    }

    return WINDOWS_HANDLE64_INVALID;
}

void windows_handle64_init(void)
{
    u32 index;

    if (g_windows_handle64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_MAX_TABLES; ++index)
    {
        g_windows_handle64_table_used[index] = 0u;
        windows_handle64_clear_table(&g_windows_handle64_tables[index]);
    }
    for (index = 0u; index < WINDOWS_HANDLE64_MAX_EVENTS; ++index)
    {
        windows_handle64_clear_event(&g_windows_handle64_events[index]);
    }
    for (index = 0u; index < WINDOWS_HANDLE64_MAX_MUTANTS; ++index)
    {
        windows_handle64_clear_mutant(&g_windows_handle64_mutants[index]);
    }

    g_windows_handle64_install_count = 0u;
    g_windows_handle64_duplicate_count = 0u;
    g_windows_handle64_inherit_count = 0u;
    g_windows_handle64_close_count = 0u;
    g_windows_handle64_pseudo_count = 0u;
    g_windows_handle64_global_denial_count = 0u;
    g_windows_handle64_event_create_count = 0u;
    g_windows_handle64_event_set_count = 0u;
    g_windows_handle64_event_wait_count = 0u;
    g_windows_handle64_event_denial_count = 0u;
    g_windows_handle64_event_live_count = 0u;
    g_windows_handle64_event_last_handle = WINDOWS_HANDLE64_INVALID;
    g_windows_handle64_event_last_state = 0u;
    g_windows_handle64_mutant_create_count = 0u;
    g_windows_handle64_mutant_wait_count = 0u;
    g_windows_handle64_mutant_release_count = 0u;
    g_windows_handle64_mutant_denial_count = 0u;
    g_windows_handle64_mutant_live_count = 0u;
    g_windows_handle64_mutant_last_handle = WINDOWS_HANDLE64_INVALID;
    g_windows_handle64_mutant_last_owner = PROCESS64_INVALID_PID;
    g_windows_handle64_mutant_last_count = 0u;
    g_windows_handle64_last_handle = WINDOWS_HANDLE64_INVALID;
    g_windows_handle64_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    g_windows_handle64_initialized = 1u;
}

windows_handle64_table_t *windows_handle64_table_for_process(u32 pid)
{
    persona_context_t *context;

    windows_handle64_init();
    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        return 0;
    }

    return (windows_handle64_table_t *)context->windows_handle_table;
}

u32 windows_handle64_init_process(u32 pid)
{
    persona_context_t *context;
    u32 index;

    windows_handle64_init();
    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return 0u;
    }

    if (context->windows_handle_table != 0)
    {
        return 1u;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_MAX_TABLES; ++index)
    {
        if (g_windows_handle64_table_used[index] == 0u)
        {
            g_windows_handle64_table_used[index] = 1u;
            windows_handle64_clear_table(&g_windows_handle64_tables[index]);
            g_windows_handle64_tables[index].pid = pid;
            g_windows_handle64_tables[index].owner_id = process64_principal(pid);
            context->windows_handle_table = &g_windows_handle64_tables[index];
            return 1u;
        }
    }

    windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
    return 0u;
}

u32 windows_handle64_release_process(u32 pid)
{
    persona_context_t *context;
    windows_handle64_table_t *table;
    u32 index;
    u32 released = 0u;

    windows_handle64_init();
    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->windows_handle_table == 0))
    {
        return 0u;
    }

    table = (windows_handle64_table_t *)context->windows_handle_table;
    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if (windows_handle64_entry_active(&table->entries[index]) != 0u)
        {
            u32 table_shared = windows_handle64_shared_entry_count(
                table,
                table->entries[index].capability_handle,
                table->entries[index].object_type);
            u32 global_shared = windows_handle64_global_shared_entry_count(
                table->entries[index].capability_handle,
                table->entries[index].object_type);

            if ((windows_handle64_prior_shared_entry_exists(table, index) == 0u)
                && (global_shared == table_shared))
            {
                windows_handle64_revoke_backing(
                    table->entries[index].capability_handle,
                    table->owner_id,
                    table->entries[index].object_type);
            }
        }
    }

    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if (windows_handle64_entry_active(&table->entries[index]) != 0u)
        {
            u32 object_type = table->entries[index].object_type;
            u32 object_token = table->entries[index].reserved;

            windows_handle64_clear_entry(&table->entries[index]);
            if (object_type == WINDOWS_HANDLE64_TYPE_EVENT)
            {
                windows_handle64_release_event_if_unreferenced(object_token);
            }
            else if (object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
            {
                windows_handle64_release_mutant_if_unreferenced(object_token);
            }
            ++released;
        }
    }
    table->live_count = 0u;
    context->windows_handle_table = 0;

    for (index = 0u; index < WINDOWS_HANDLE64_MAX_TABLES; ++index)
    {
        if (&g_windows_handle64_tables[index] == table)
        {
            windows_handle64_clear_table(table);
            g_windows_handle64_table_used[index] = 0u;
            break;
        }
    }

    return released;
}

u64 windows_handle64_install(
    u32 pid,
    u32 capability_handle,
    u32 object_type,
    u32 rights,
    u32 flags)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);
    u32 owner_id = process64_principal(pid);
    u64 handle;

    if ((table == 0)
        || (owner_id == 0u)
        || (capability_handle == CAPABILITY64_INVALID_HANDLE)
        || (windows_handle64_backing_is_valid(
            capability_handle,
            owner_id,
            object_type,
            rights) == 0u))
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    handle = windows_handle64_install_in_table(table, capability_handle, object_type, rights, flags);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return WINDOWS_HANDLE64_INVALID;
    }

    ++g_windows_handle64_install_count;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_capability = capability_handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return handle;
}

u64 windows_handle64_duplicate(
    u32 source_pid,
    u64 source_handle,
    u32 target_pid,
    u32 flags)
{
    windows_handle64_table_t *source_table = windows_handle64_table_for_process(source_pid);
    windows_handle64_table_t *target_table = windows_handle64_table_for_process(target_pid);
    windows_handle64_entry_t *source_entry;
    u32 source_owner = process64_principal(source_pid);
    u32 target_owner = process64_principal(target_pid);
    u32 target_capability;
    u64 target_handle;

    if (windows_handle64_is_pseudo(source_handle) != 0u)
    {
        ++g_windows_handle64_duplicate_count;
        g_windows_handle64_last_handle = source_handle;
        g_windows_handle64_last_capability = CAPABILITY64_INVALID_HANDLE;
        g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
        return source_handle;
    }

    if ((source_table == 0)
        || (target_table == 0)
        || ((flags & ~WINDOWS_HANDLE64_FLAGS_VALID) != 0u))
    {
        windows_handle64_note_denial(source_pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    source_entry = windows_handle64_find_entry(source_table, source_handle);
    if (source_entry == 0)
    {
        windows_handle64_note_denial(source_pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    target_capability = source_entry->capability_handle;
    if (source_owner != target_owner)
    {
        if ((source_entry->object_type == WINDOWS_HANDLE64_TYPE_FILE)
            && (fs64_handle_is_node(source_entry->capability_handle, source_owner) != 0u))
        {
            windows_handle64_note_denial(source_pid, WINDOWS_ABI64_STATUS_ACCESS_DENIED);
            return WINDOWS_HANDLE64_INVALID;
        }

        target_capability = capability64_delegate_persistent(
            source_entry->capability_handle,
            source_entry->rights,
            CAPABILITY64_CONTEXT(source_owner, target_owner));
        if (target_capability == CAPABILITY64_INVALID_HANDLE)
        {
            windows_handle64_note_denial(source_pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
            return WINDOWS_HANDLE64_INVALID;
        }
    }

    target_handle = windows_handle64_install_in_table(
        target_table,
        target_capability,
        source_entry->object_type,
        source_entry->rights,
        flags);
    if (target_handle == WINDOWS_HANDLE64_INVALID)
    {
        if (source_owner != target_owner)
        {
            windows_handle64_revoke_backing(
                target_capability,
                target_owner,
                source_entry->object_type);
        }
        windows_handle64_note_denial(source_pid, WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return WINDOWS_HANDLE64_INVALID;
    }
    if ((source_entry->object_type == WINDOWS_HANDLE64_TYPE_EVENT)
        || (source_entry->object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
        || (source_entry->object_type == WINDOWS_HANDLE64_TYPE_KEY))
    {
        windows_handle64_entry_t *target_entry =
            windows_handle64_find_entry(target_table, target_handle);

        if (target_entry != 0)
        {
            target_entry->reserved = source_entry->reserved;
        }
    }

    ++g_windows_handle64_duplicate_count;
    g_windows_handle64_last_handle = target_handle;
    g_windows_handle64_last_capability = target_capability;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return target_handle;
}

u32 windows_handle64_inherit(u32 source_pid, u32 target_pid)
{
    windows_handle64_table_t *source_table = windows_handle64_table_for_process(source_pid);
    u32 index;
    u32 inherited = 0u;

    if ((source_table == 0) || (windows_handle64_table_for_process(target_pid) == 0))
    {
        windows_handle64_note_denial(source_pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0u;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if ((windows_handle64_entry_active(&source_table->entries[index]) != 0u)
            && ((source_table->entries[index].flags & WINDOWS_HANDLE64_FLAG_INHERITABLE) != 0u)
            && (windows_handle64_duplicate(
                    source_pid,
                    source_table->entries[index].handle,
                    target_pid,
                    source_table->entries[index].flags) != WINDOWS_HANDLE64_INVALID))
        {
            ++inherited;
        }
    }

    if (inherited != 0u)
    {
        g_windows_handle64_inherit_count += inherited;
    }
    return inherited;
}

u32 windows_handle64_close(u32 pid, u64 handle)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);
    windows_handle64_entry_t *entry;
    u32 capability_handle;
    u32 object_type;
    u32 object_token;
    u32 global_shared_count;

    if ((table == 0) || (windows_handle64_is_pseudo(handle) != 0u))
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0u;
    }

    entry = windows_handle64_find_entry(table, handle);
    if (entry == 0)
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0u;
    }
    if ((entry->flags & WINDOWS_HANDLE64_FLAG_PROTECT_CLOSE) != 0u)
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0u;
    }

    capability_handle = entry->capability_handle;
    object_type = entry->object_type;
    object_token = entry->reserved;
    global_shared_count =
        windows_handle64_global_shared_entry_count(capability_handle, object_type);
    windows_handle64_clear_entry(entry);
    if (object_type == WINDOWS_HANDLE64_TYPE_EVENT)
    {
        windows_handle64_release_event_if_unreferenced(object_token);
    }
    else if (object_type == WINDOWS_HANDLE64_TYPE_MUTANT)
    {
        windows_handle64_release_mutant_if_unreferenced(object_token);
    }
    if (table->live_count != 0u)
    {
        --table->live_count;
    }

    if (global_shared_count > 1u)
    {
        windows_handle64_sync_ref_counts(table, capability_handle, object_type);
    }
    else
    {
        windows_handle64_revoke_backing(capability_handle, table->owner_id, object_type);
    }

    ++g_windows_handle64_close_count;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_capability = capability_handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return 1u;
}

u32 windows_handle64_is_pseudo(u64 handle)
{
    return ((handle == WINDOWS_HANDLE64_PSEUDO_CURRENT_PROCESS)
        || (handle == WINDOWS_HANDLE64_PSEUDO_CURRENT_THREAD))
        ? 1u
        : 0u;
}

u32 windows_handle64_resolve_pseudo(
    u32 pid,
    u64 handle,
    u32 *object_type_out,
    u64 *identity_out)
{
    if ((process64_principal(pid) == 0u)
        || (object_type_out == 0)
        || (identity_out == 0)
        || (windows_handle64_is_pseudo(handle) == 0u))
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0u;
    }

    if (handle == WINDOWS_HANDLE64_PSEUDO_CURRENT_PROCESS)
    {
        *object_type_out = WINDOWS_HANDLE64_TYPE_PROCESS;
        *identity_out = (u64)pid;
    }
    else
    {
        *object_type_out = WINDOWS_HANDLE64_TYPE_THREAD;
        *identity_out = (u64)pid;
    }

    ++g_windows_handle64_pseudo_count;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_capability = CAPABILITY64_INVALID_HANDLE;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return 1u;
}

u32 windows_handle64_live_count(u32 pid)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);

    return (table != 0) ? table->live_count : 0u;
}

u32 windows_handle64_denial_count(u32 pid)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);

    return (table != 0) ? table->denial_count : 0u;
}

u64 windows_handle64_first_handle(u32 pid)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);
    u32 index;

    if (table == 0)
    {
        return WINDOWS_HANDLE64_INVALID;
    }

    for (index = 0u; index < WINDOWS_HANDLE64_TABLE_LIMIT; ++index)
    {
        if (windows_handle64_entry_active(&table->entries[index]) != 0u)
        {
            return table->entries[index].handle;
        }
    }

    return WINDOWS_HANDLE64_INVALID;
}

u32 windows_handle64_entry_capability(u32 pid, u64 handle)
{
    windows_handle64_entry_t *entry =
        windows_handle64_find_entry(windows_handle64_table_for_process(pid), handle);

    return (entry != 0) ? entry->capability_handle : CAPABILITY64_INVALID_HANDLE;
}

u32 windows_handle64_entry_type(u32 pid, u64 handle)
{
    windows_handle64_entry_t *entry =
        windows_handle64_find_entry(windows_handle64_table_for_process(pid), handle);

    return (entry != 0) ? entry->object_type : WINDOWS_HANDLE64_TYPE_EMPTY;
}

u32 windows_handle64_entry_rights(u32 pid, u64 handle)
{
    windows_handle64_entry_t *entry =
        windows_handle64_find_entry(windows_handle64_table_for_process(pid), handle);

    return (entry != 0) ? entry->rights : 0u;
}

u32 windows_handle64_entry_flags(u32 pid, u64 handle)
{
    windows_handle64_entry_t *entry =
        windows_handle64_find_entry(windows_handle64_table_for_process(pid), handle);

    return (entry != 0) ? entry->flags : 0u;
}

u32 windows_handle64_entry_ref_count(u32 pid, u64 handle)
{
    windows_handle64_entry_t *entry =
        windows_handle64_find_entry(windows_handle64_table_for_process(pid), handle);

    return (entry != 0) ? entry->ref_count : 0u;
}

u64 windows_handle64_high_water_handle(u32 pid)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);

    return (table != 0) ? table->high_water_handle : WINDOWS_HANDLE64_INVALID;
}

u64 windows_handle64_key_create(
    u32 pid,
    u32 key_id,
    u32 rights,
    u32 flags)
{
    windows_handle64_table_t *table;
    windows_handle64_entry_t *entry;
    u32 owner_id = process64_principal(pid);
    u32 capability_handle;
    u64 handle;

    windows_handle64_init();
    if ((pid == PROCESS64_INVALID_PID)
        || (owner_id == 0u)
        || (key_id == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || (rights == 0u)
        || ((flags & ~WINDOWS_HANDLE64_FLAGS_VALID) != 0u))
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return WINDOWS_HANDLE64_INVALID;
    }

    if (windows_handle64_table_for_process(pid) == 0)
    {
        (void)windows_handle64_init_process(pid);
    }
    table = windows_handle64_table_for_process(pid);
    if (table == 0)
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    capability_handle = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INIT,
        rights,
        owner_id);
    if (capability_handle == CAPABILITY64_INVALID_HANDLE)
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    handle = windows_handle64_install_in_table(
        table,
        capability_handle,
        WINDOWS_HANDLE64_TYPE_KEY,
        rights,
        flags);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        (void)capability64_revoke(capability_handle, owner_id);
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    entry = windows_handle64_find_entry(table, handle);
    if (entry == 0)
    {
        (void)windows_handle64_close(pid, handle);
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    entry->reserved = key_id;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_capability = capability_handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return handle;
}

u32 windows_handle64_key_id(u32 pid, u64 handle)
{
    windows_handle64_table_t *table = windows_handle64_table_for_process(pid);
    windows_handle64_entry_t *entry;

    if (table == 0)
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0u;
    }

    entry = windows_handle64_find_entry(table, handle);
    if ((entry == 0)
        || (entry->object_type != WINDOWS_HANDLE64_TYPE_KEY)
        || (entry->reserved == 0u)
        || (capability64_route(entry->capability_handle, CAPABILITY64_RIGHT_QUERY, table->owner_id)
            == CAPABILITY64_INVALID_HANDLE))
    {
        windows_handle64_note_denial(pid, WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return 0u;
    }

    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_capability = entry->capability_handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return entry->reserved;
}

u64 windows_handle64_event_create(
    u32 pid,
    u32 manual_reset,
    u32 initial_state,
    u32 flags)
{
    windows_handle64_table_t *table;
    windows_handle64_entry_t *entry;
    u32 owner_id = process64_principal(pid);
    u32 rights = CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY;
    u32 event_index;
    u32 capability_handle;
    u64 handle;

    windows_handle64_init();
    if ((pid == PROCESS64_INVALID_PID)
        || (owner_id == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || ((flags & ~WINDOWS_HANDLE64_FLAGS_VALID) != 0u))
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return WINDOWS_HANDLE64_INVALID;
    }

    if (windows_handle64_table_for_process(pid) == 0)
    {
        (void)windows_handle64_init_process(pid);
    }
    table = windows_handle64_table_for_process(pid);
    if (table == 0)
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    for (event_index = 0u; event_index < WINDOWS_HANDLE64_MAX_EVENTS; ++event_index)
    {
        if (g_windows_handle64_events[event_index].active == 0u)
        {
            break;
        }
    }
    if (event_index >= WINDOWS_HANDLE64_MAX_EVENTS)
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    capability_handle = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INIT,
        rights,
        owner_id);
    if (capability_handle == CAPABILITY64_INVALID_HANDLE)
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    handle = windows_handle64_install_in_table(
        table,
        capability_handle,
        WINDOWS_HANDLE64_TYPE_EVENT,
        rights,
        flags);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        (void)capability64_revoke(capability_handle, owner_id);
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    entry = windows_handle64_find_entry(table, handle);
    if (entry == 0)
    {
        (void)windows_handle64_close(pid, handle);
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    g_windows_handle64_events[event_index].active = 1u;
    g_windows_handle64_events[event_index].pid = pid;
    g_windows_handle64_events[event_index].manual_reset =
        (manual_reset != 0u) ? 1u : 0u;
    g_windows_handle64_events[event_index].signaled =
        (initial_state != 0u) ? 1u : 0u;
    g_windows_handle64_events[event_index].set_count = 0u;
    g_windows_handle64_events[event_index].wait_count = 0u;
    g_windows_handle64_events[event_index].waiting_pid = PROCESS64_INVALID_PID;
    g_windows_handle64_events[event_index].waiting_task_id = SCHEDULER64_INVALID_TASK;
    entry->reserved = event_index + 1u;
    ++g_windows_handle64_event_create_count;
    ++g_windows_handle64_event_live_count;
    g_windows_handle64_event_last_handle = handle;
    g_windows_handle64_event_last_state =
        g_windows_handle64_events[event_index].signaled;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_capability = capability_handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return handle;
}

u32 windows_handle64_event_set(
    u32 pid,
    u64 handle,
    u32 *previous_state_out,
    u32 *wake_task_out)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_SEND);
    u32 previous_state;

    if (wake_task_out != 0)
    {
        *wake_task_out = SCHEDULER64_INVALID_TASK;
    }
    if (event == 0)
    {
        return 0u;
    }

    previous_state = event->signaled;
    event->signaled = 1u;
    if (event->waiting_task_id != SCHEDULER64_INVALID_TASK)
    {
        if (wake_task_out != 0)
        {
            *wake_task_out = event->waiting_task_id;
        }
        event->waiting_pid = PROCESS64_INVALID_PID;
        event->waiting_task_id = SCHEDULER64_INVALID_TASK;
        if (event->manual_reset == 0u)
        {
            event->signaled = 0u;
        }
    }
    ++event->set_count;
    ++g_windows_handle64_event_set_count;
    g_windows_handle64_event_last_state = event->signaled;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    if (previous_state_out != 0)
    {
        *previous_state_out = previous_state;
    }
    return 1u;
}

u32 windows_handle64_event_wait(u32 pid, u64 handle)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    if (event == 0)
    {
        return WINDOWS_HANDLE64_EVENT_WAIT_DENIED;
    }

    ++event->wait_count;
    ++g_windows_handle64_event_wait_count;
    if (event->signaled == 0u)
    {
        g_windows_handle64_event_last_state = 0u;
        g_windows_handle64_last_handle = handle;
        g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_TIMEOUT;
        return WINDOWS_HANDLE64_EVENT_WAIT_TIMEOUT;
    }

    if (event->manual_reset == 0u)
    {
        event->signaled = 0u;
    }
    g_windows_handle64_event_last_state = event->signaled;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return WINDOWS_HANDLE64_EVENT_WAIT_SIGNALED;
}

u32 windows_handle64_event_arm_wait(u32 pid, u64 handle, u32 task_id)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    if ((event == 0)
        || (task_id == SCHEDULER64_INVALID_TASK)
        || (event->signaled != 0u)
        || (event->waiting_task_id != SCHEDULER64_INVALID_TASK))
    {
        windows_handle64_note_event_denial(WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return 0u;
    }

    event->waiting_pid = pid;
    event->waiting_task_id = task_id;
    return 1u;
}

u32 windows_handle64_event_cancel_wait(u32 pid, u64 handle, u32 task_id)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    if ((event == 0)
        || (event->waiting_pid != pid)
        || (event->waiting_task_id != task_id))
    {
        return 0u;
    }

    event->waiting_pid = PROCESS64_INVALID_PID;
    event->waiting_task_id = SCHEDULER64_INVALID_TASK;
    return 1u;
}

u32 windows_handle64_event_waiter_task(u32 pid, u64 handle)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (event != 0) ? event->waiting_task_id : SCHEDULER64_INVALID_TASK;
}

u32 windows_handle64_event_waiter_pid(u32 pid, u64 handle)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (event != 0) ? event->waiting_pid : PROCESS64_INVALID_PID;
}

u32 windows_handle64_event_signaled(u32 pid, u64 handle)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (event != 0) ? event->signaled : 0u;
}

u32 windows_handle64_event_manual_reset(u32 pid, u64 handle)
{
    windows_handle64_event_object_t *event =
        windows_handle64_event_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (event != 0) ? event->manual_reset : 0u;
}

u64 windows_handle64_mutant_create(
    u32 pid,
    u32 initial_owner,
    u32 flags)
{
    windows_handle64_table_t *table;
    windows_handle64_entry_t *entry;
    u32 owner_id = process64_principal(pid);
    u32 rights = CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY;
    u32 mutant_index;
    u32 capability_handle;
    u64 handle;

    windows_handle64_init();
    if ((pid == PROCESS64_INVALID_PID)
        || (owner_id == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE)
        || ((flags & ~WINDOWS_HANDLE64_FLAGS_VALID) != 0u))
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return WINDOWS_HANDLE64_INVALID;
    }

    if (windows_handle64_table_for_process(pid) == 0)
    {
        (void)windows_handle64_init_process(pid);
    }
    table = windows_handle64_table_for_process(pid);
    if (table == 0)
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    for (mutant_index = 0u; mutant_index < WINDOWS_HANDLE64_MAX_MUTANTS; ++mutant_index)
    {
        if (g_windows_handle64_mutants[mutant_index].active == 0u)
        {
            break;
        }
    }
    if (mutant_index >= WINDOWS_HANDLE64_MAX_MUTANTS)
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    capability_handle = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INIT,
        rights,
        owner_id);
    if (capability_handle == CAPABILITY64_INVALID_HANDLE)
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    handle = windows_handle64_install_in_table(
        table,
        capability_handle,
        WINDOWS_HANDLE64_TYPE_MUTANT,
        rights,
        flags);
    if (handle == WINDOWS_HANDLE64_INVALID)
    {
        (void)capability64_revoke(capability_handle, owner_id);
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_NO_MEMORY);
        return WINDOWS_HANDLE64_INVALID;
    }

    entry = windows_handle64_find_entry(table, handle);
    if (entry == 0)
    {
        (void)windows_handle64_close(pid, handle);
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_INVALID_HANDLE);
        return WINDOWS_HANDLE64_INVALID;
    }

    g_windows_handle64_mutants[mutant_index].active = 1u;
    g_windows_handle64_mutants[mutant_index].creator_pid = pid;
    g_windows_handle64_mutants[mutant_index].owner_pid =
        (initial_owner != 0u) ? pid : PROCESS64_INVALID_PID;
    g_windows_handle64_mutants[mutant_index].recursion_count =
        (initial_owner != 0u) ? 1u : 0u;
    g_windows_handle64_mutants[mutant_index].wait_count = 0u;
    g_windows_handle64_mutants[mutant_index].release_count = 0u;
    g_windows_handle64_mutants[mutant_index].waiting_pid = PROCESS64_INVALID_PID;
    g_windows_handle64_mutants[mutant_index].waiting_task_id = SCHEDULER64_INVALID_TASK;
    entry->reserved = mutant_index + 1u;
    ++g_windows_handle64_mutant_create_count;
    ++g_windows_handle64_mutant_live_count;
    g_windows_handle64_mutant_last_handle = handle;
    g_windows_handle64_mutant_last_owner =
        g_windows_handle64_mutants[mutant_index].owner_pid;
    g_windows_handle64_mutant_last_count =
        g_windows_handle64_mutants[mutant_index].recursion_count;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_capability = capability_handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    return handle;
}

u32 windows_handle64_mutant_wait(u32 pid, u64 handle)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    if (mutant == 0)
    {
        return WINDOWS_HANDLE64_MUTANT_WAIT_DENIED;
    }

    ++mutant->wait_count;
    ++g_windows_handle64_mutant_wait_count;
    if ((mutant->owner_pid == PROCESS64_INVALID_PID)
        || (mutant->recursion_count == 0u))
    {
        mutant->owner_pid = pid;
        mutant->recursion_count = 1u;
        g_windows_handle64_mutant_last_owner = mutant->owner_pid;
        g_windows_handle64_mutant_last_count = mutant->recursion_count;
        g_windows_handle64_last_handle = handle;
        g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
        return WINDOWS_HANDLE64_MUTANT_WAIT_ACQUIRED;
    }

    if (mutant->owner_pid == pid)
    {
        ++mutant->recursion_count;
        g_windows_handle64_mutant_last_owner = mutant->owner_pid;
        g_windows_handle64_mutant_last_count = mutant->recursion_count;
        g_windows_handle64_last_handle = handle;
        g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
        return WINDOWS_HANDLE64_MUTANT_WAIT_ACQUIRED;
    }

    g_windows_handle64_mutant_last_owner = mutant->owner_pid;
    g_windows_handle64_mutant_last_count = mutant->recursion_count;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_TIMEOUT;
    return WINDOWS_HANDLE64_MUTANT_WAIT_TIMEOUT;
}

u32 windows_handle64_mutant_arm_wait(u32 pid, u64 handle, u32 task_id)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    if ((mutant == 0)
        || (task_id == SCHEDULER64_INVALID_TASK)
        || (mutant->owner_pid == PROCESS64_INVALID_PID)
        || (mutant->owner_pid == pid)
        || (mutant->recursion_count == 0u)
        || (mutant->waiting_task_id != SCHEDULER64_INVALID_TASK))
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_INVALID_PARAMETER);
        return 0u;
    }

    mutant->waiting_pid = pid;
    mutant->waiting_task_id = task_id;
    return 1u;
}

u32 windows_handle64_mutant_cancel_wait(u32 pid, u64 handle, u32 task_id)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    if ((mutant == 0)
        || (mutant->waiting_pid != pid)
        || (mutant->waiting_task_id != task_id))
    {
        return 0u;
    }

    mutant->waiting_pid = PROCESS64_INVALID_PID;
    mutant->waiting_task_id = SCHEDULER64_INVALID_TASK;
    return 1u;
}

u32 windows_handle64_mutant_release(
    u32 pid,
    u64 handle,
    u32 *previous_count_out,
    u32 *wake_task_out)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_SEND);
    u32 previous_count;

    if (wake_task_out != 0)
    {
        *wake_task_out = SCHEDULER64_INVALID_TASK;
    }
    if (mutant == 0)
    {
        return WINDOWS_ABI64_STATUS_INVALID_HANDLE;
    }
    if ((mutant->owner_pid != pid) || (mutant->recursion_count == 0u))
    {
        windows_handle64_note_mutant_denial(WINDOWS_ABI64_STATUS_MUTANT_NOT_OWNED);
        return WINDOWS_ABI64_STATUS_MUTANT_NOT_OWNED;
    }

    previous_count = mutant->recursion_count;
    --mutant->recursion_count;
    if (mutant->recursion_count == 0u)
    {
        if (mutant->waiting_task_id != SCHEDULER64_INVALID_TASK)
        {
            if (wake_task_out != 0)
            {
                *wake_task_out = mutant->waiting_task_id;
            }
            mutant->owner_pid = mutant->waiting_pid;
            mutant->recursion_count = 1u;
            mutant->waiting_pid = PROCESS64_INVALID_PID;
            mutant->waiting_task_id = SCHEDULER64_INVALID_TASK;
        }
        else
        {
            mutant->owner_pid = PROCESS64_INVALID_PID;
        }
    }
    ++mutant->release_count;
    ++g_windows_handle64_mutant_release_count;
    g_windows_handle64_mutant_last_handle = handle;
    g_windows_handle64_mutant_last_owner = mutant->owner_pid;
    g_windows_handle64_mutant_last_count = mutant->recursion_count;
    g_windows_handle64_last_handle = handle;
    g_windows_handle64_last_result = WINDOWS_ABI64_STATUS_SUCCESS;
    if (previous_count_out != 0)
    {
        *previous_count_out = previous_count;
    }
    return WINDOWS_ABI64_STATUS_SUCCESS;
}

u32 windows_handle64_mutant_owner(u32 pid, u64 handle)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (mutant != 0) ? mutant->owner_pid : PROCESS64_INVALID_PID;
}

u32 windows_handle64_mutant_recursion(u32 pid, u64 handle)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (mutant != 0) ? mutant->recursion_count : 0u;
}

u32 windows_handle64_mutant_waiter_task(u32 pid, u64 handle)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (mutant != 0) ? mutant->waiting_task_id : SCHEDULER64_INVALID_TASK;
}

u32 windows_handle64_mutant_waiter_pid(u32 pid, u64 handle)
{
    windows_handle64_mutant_object_t *mutant =
        windows_handle64_mutant_for_handle(pid, handle, CAPABILITY64_RIGHT_QUERY);

    return (mutant != 0) ? mutant->waiting_pid : PROCESS64_INVALID_PID;
}

u32 windows_handle64_event_create_count(void)
{
    return g_windows_handle64_event_create_count;
}

u32 windows_handle64_event_set_count(void)
{
    return g_windows_handle64_event_set_count;
}

u32 windows_handle64_event_wait_count(void)
{
    return g_windows_handle64_event_wait_count;
}

u32 windows_handle64_event_denial_count(void)
{
    return g_windows_handle64_event_denial_count;
}

u32 windows_handle64_event_live_count(void)
{
    return g_windows_handle64_event_live_count;
}

u64 windows_handle64_event_last_handle(void)
{
    return g_windows_handle64_event_last_handle;
}

u32 windows_handle64_event_last_state(void)
{
    return g_windows_handle64_event_last_state;
}

u32 windows_handle64_mutant_create_count(void)
{
    return g_windows_handle64_mutant_create_count;
}

u32 windows_handle64_mutant_wait_count(void)
{
    return g_windows_handle64_mutant_wait_count;
}

u32 windows_handle64_mutant_release_count(void)
{
    return g_windows_handle64_mutant_release_count;
}

u32 windows_handle64_mutant_denial_count(void)
{
    return g_windows_handle64_mutant_denial_count;
}

u32 windows_handle64_mutant_live_count(void)
{
    return g_windows_handle64_mutant_live_count;
}

u64 windows_handle64_mutant_last_handle(void)
{
    return g_windows_handle64_mutant_last_handle;
}

u32 windows_handle64_mutant_last_owner(void)
{
    return g_windows_handle64_mutant_last_owner;
}

u32 windows_handle64_mutant_last_count(void)
{
    return g_windows_handle64_mutant_last_count;
}

u32 windows_handle64_install_count(void)
{
    return g_windows_handle64_install_count;
}

u32 windows_handle64_duplicate_count(void)
{
    return g_windows_handle64_duplicate_count;
}

u32 windows_handle64_inherit_count(void)
{
    return g_windows_handle64_inherit_count;
}

u32 windows_handle64_close_count(void)
{
    return g_windows_handle64_close_count;
}

u32 windows_handle64_pseudo_count(void)
{
    return g_windows_handle64_pseudo_count;
}

u32 windows_handle64_global_denial_count(void)
{
    return g_windows_handle64_global_denial_count;
}

u64 windows_handle64_last_handle(void)
{
    return g_windows_handle64_last_handle;
}

u32 windows_handle64_last_capability(void)
{
    return g_windows_handle64_last_capability;
}

u32 windows_handle64_last_result(void)
{
    return g_windows_handle64_last_result;
}
