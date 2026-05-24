#ifndef LIMITLESS_WINDOWS_HANDLE_X64_H
#define LIMITLESS_WINDOWS_HANDLE_X64_H

#include "capability_x64.h"
#include "types.h"

#define WINDOWS_HANDLE64_TABLE_LIMIT 256u
#define WINDOWS_HANDLE64_MAX_TABLES 16u
#define WINDOWS_HANDLE64_INVALID 0ull
#define WINDOWS_HANDLE64_FIRST_DYNAMIC 0x0000000000000040ull
#define WINDOWS_HANDLE64_GRANULARITY 4ull
#define WINDOWS_HANDLE64_PSEUDO_CURRENT_PROCESS 0xFFFFFFFFFFFFFFFFull
#define WINDOWS_HANDLE64_PSEUDO_CURRENT_THREAD 0xFFFFFFFFFFFFFFFEull

#define WINDOWS_HANDLE64_TYPE_EMPTY 0u
#define WINDOWS_HANDLE64_TYPE_FILE 1u
#define WINDOWS_HANDLE64_TYPE_DEVICE 2u
#define WINDOWS_HANDLE64_TYPE_EVENT 3u
#define WINDOWS_HANDLE64_TYPE_MUTANT 4u
#define WINDOWS_HANDLE64_TYPE_PROCESS 5u
#define WINDOWS_HANDLE64_TYPE_THREAD 6u
#define WINDOWS_HANDLE64_TYPE_KEY 7u

#define WINDOWS_HANDLE64_MAX_EVENTS 32u
#define WINDOWS_HANDLE64_EVENT_TYPE_NOTIFICATION 0u
#define WINDOWS_HANDLE64_EVENT_TYPE_SYNCHRONIZATION 1u
#define WINDOWS_HANDLE64_EVENT_WAIT_DENIED 0u
#define WINDOWS_HANDLE64_EVENT_WAIT_SIGNALED 1u
#define WINDOWS_HANDLE64_EVENT_WAIT_TIMEOUT 2u

#define WINDOWS_HANDLE64_MAX_MUTANTS 32u
#define WINDOWS_HANDLE64_MUTANT_WAIT_DENIED 0u
#define WINDOWS_HANDLE64_MUTANT_WAIT_ACQUIRED 1u
#define WINDOWS_HANDLE64_MUTANT_WAIT_TIMEOUT 2u

#define WINDOWS_HANDLE64_FLAG_INHERITABLE 0x00000001u
#define WINDOWS_HANDLE64_FLAG_PROTECT_CLOSE 0x00000002u
#define WINDOWS_HANDLE64_FLAGS_VALID \
    (WINDOWS_HANDLE64_FLAG_INHERITABLE | WINDOWS_HANDLE64_FLAG_PROTECT_CLOSE)

#define WINDOWS_HANDLE64_AUDIT_HANDLE_TABLE 0x004Bu

typedef struct windows_handle64_entry
{
    u64 handle;
    u32 capability_handle;
    u32 object_type;
    u32 rights;
    u32 flags;
    u32 ref_count;
    u32 reserved;
} windows_handle64_entry_t;

typedef struct windows_handle64_table
{
    u32 pid;
    u32 owner_id;
    u32 live_count;
    u32 denial_count;
    u64 high_water_handle;
    windows_handle64_entry_t entries[WINDOWS_HANDLE64_TABLE_LIMIT];
} windows_handle64_table_t;

void windows_handle64_init(void);
u32 windows_handle64_init_process(u32 pid);
u32 windows_handle64_release_process(u32 pid);
windows_handle64_table_t *windows_handle64_table_for_process(u32 pid);
u64 windows_handle64_install(
    u32 pid,
    u32 capability_handle,
    u32 object_type,
    u32 rights,
    u32 flags);
u64 windows_handle64_duplicate(
    u32 source_pid,
    u64 source_handle,
    u32 target_pid,
    u32 flags);
u32 windows_handle64_inherit(u32 source_pid, u32 target_pid);
u32 windows_handle64_close(u32 pid, u64 handle);
u32 windows_handle64_is_pseudo(u64 handle);
u32 windows_handle64_resolve_pseudo(
    u32 pid,
    u64 handle,
    u32 *object_type_out,
    u64 *identity_out);
u32 windows_handle64_live_count(u32 pid);
u32 windows_handle64_denial_count(u32 pid);
u64 windows_handle64_first_handle(u32 pid);
u32 windows_handle64_entry_capability(u32 pid, u64 handle);
u32 windows_handle64_entry_type(u32 pid, u64 handle);
u32 windows_handle64_entry_rights(u32 pid, u64 handle);
u32 windows_handle64_entry_flags(u32 pid, u64 handle);
u32 windows_handle64_entry_ref_count(u32 pid, u64 handle);
u64 windows_handle64_high_water_handle(u32 pid);
u64 windows_handle64_key_create(
    u32 pid,
    u32 key_id,
    u32 rights,
    u32 flags);
u32 windows_handle64_key_id(u32 pid, u64 handle);
u64 windows_handle64_event_create(
    u32 pid,
    u32 manual_reset,
    u32 initial_state,
    u32 flags);
u32 windows_handle64_event_set(
    u32 pid,
    u64 handle,
    u32 *previous_state_out,
    u32 *wake_task_out);
u32 windows_handle64_event_wait(u32 pid, u64 handle);
u32 windows_handle64_event_arm_wait(u32 pid, u64 handle, u32 task_id);
u32 windows_handle64_event_cancel_wait(u32 pid, u64 handle, u32 task_id);
u32 windows_handle64_event_waiter_task(u32 pid, u64 handle);
u32 windows_handle64_event_waiter_pid(u32 pid, u64 handle);
u32 windows_handle64_event_signaled(u32 pid, u64 handle);
u32 windows_handle64_event_manual_reset(u32 pid, u64 handle);
u32 windows_handle64_event_create_count(void);
u32 windows_handle64_event_set_count(void);
u32 windows_handle64_event_wait_count(void);
u32 windows_handle64_event_denial_count(void);
u32 windows_handle64_event_live_count(void);
u64 windows_handle64_event_last_handle(void);
u32 windows_handle64_event_last_state(void);
u64 windows_handle64_mutant_create(
    u32 pid,
    u32 initial_owner,
    u32 flags);
u32 windows_handle64_mutant_wait(u32 pid, u64 handle);
u32 windows_handle64_mutant_arm_wait(u32 pid, u64 handle, u32 task_id);
u32 windows_handle64_mutant_cancel_wait(u32 pid, u64 handle, u32 task_id);
u32 windows_handle64_mutant_release(
    u32 pid,
    u64 handle,
    u32 *previous_count_out,
    u32 *wake_task_out);
u32 windows_handle64_mutant_owner(u32 pid, u64 handle);
u32 windows_handle64_mutant_recursion(u32 pid, u64 handle);
u32 windows_handle64_mutant_waiter_task(u32 pid, u64 handle);
u32 windows_handle64_mutant_waiter_pid(u32 pid, u64 handle);
u32 windows_handle64_mutant_create_count(void);
u32 windows_handle64_mutant_wait_count(void);
u32 windows_handle64_mutant_release_count(void);
u32 windows_handle64_mutant_denial_count(void);
u32 windows_handle64_mutant_live_count(void);
u64 windows_handle64_mutant_last_handle(void);
u32 windows_handle64_mutant_last_owner(void);
u32 windows_handle64_mutant_last_count(void);
u32 windows_handle64_install_count(void);
u32 windows_handle64_duplicate_count(void);
u32 windows_handle64_inherit_count(void);
u32 windows_handle64_close_count(void);
u32 windows_handle64_pseudo_count(void);
u32 windows_handle64_global_denial_count(void);
u64 windows_handle64_last_handle(void);
u32 windows_handle64_last_capability(void);
u32 windows_handle64_last_result(void);

#endif
