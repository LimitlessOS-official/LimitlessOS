#ifndef LIMITLESS_USERSPACE_H
#define LIMITLESS_USERSPACE_H

#include "interrupts.h"
#include "types.h"

enum userspace_executable_id
{
    USERSPACE_EXECUTABLE_SESSION_SHELL = 1,
    USERSPACE_EXECUTABLE_AUTOMATION_WORKER = 2,
    USERSPACE_EXECUTABLE_LS_UTILITY = 3,
    USERSPACE_EXECUTABLE_CAT_UTILITY = 4,
    USERSPACE_EXECUTABLE_MKDIR_UTILITY = 5,
    USERSPACE_EXECUTABLE_WRITE_UTILITY = 6,
    USERSPACE_EXECUTABLE_STAT_UTILITY = 7,
    USERSPACE_EXECUTABLE_RENAME_UTILITY = 8,
    USERSPACE_EXECUTABLE_APPEND_UTILITY = 9,
    USERSPACE_EXECUTABLE_DELETE_UTILITY = 10,
    USERSPACE_EXECUTABLE_MOVE_UTILITY = 11,
    USERSPACE_EXECUTABLE_ECHO_UTILITY = 12,
    USERSPACE_EXECUTABLE_ASK_UTILITY = 13,
    USERSPACE_EXECUTABLE_TOUCH_UTILITY = 14,
    USERSPACE_EXECUTABLE_COPY_UTILITY = 15
};

void userspace_init(void);
void userspace_enter_session(void);
void userspace_note_syscall(void);
void userspace_note_bootstrap_policy_approved(void);
u32 userspace_executable_count(void);
u32 userspace_package_manifest_count(void);
u32 userspace_spawn_builtin(u32 executable_id);
u32 userspace_launch_executable(u32 executable_id);
u32 userspace_register_endpoint(u32 role, u32 allowed_sender_mask, u32 endpoint_class, u32 delegable);
u32 userspace_lookup_endpoint(u32 owner_process_id, u32 role);
u32 userspace_lookup_endpoint_class(u32 endpoint_class);
u32 userspace_lookup_service_endpoint(u32 endpoint_class);
u32 userspace_revoke_capability(u32 capability_handle);
u32 userspace_delegate_capability(u32 delegated_capability_handle, u32 recipient_endpoint_capability_handle);
u32 userspace_request_policy(u32 policy_capability_handle, u32 request_code);
u32 userspace_register_shared_buffer(u32 buffer_address, u32 byte_length, u32 delegable);
u32 userspace_read_shared_buffer(
    u32 capability_handle,
    u32 buffer_offset,
    u32 local_buffer_address,
    u32 byte_count);
u32 userspace_write_shared_buffer(
    u32 capability_handle,
    u32 buffer_offset,
    u32 local_buffer_address,
    u32 byte_count);
u32 userspace_console_write(
    u32 console_capability_handle,
    u32 buffer_capability_handle,
    u32 buffer_offset,
    u32 byte_count);
u32 userspace_input_read(
    u32 input_capability_handle,
    u32 output_buffer_capability_handle,
    u32 byte_capacity);
u32 userspace_fs_open(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 path_byte_count);
u32 userspace_fs_create(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 path_byte_count,
    u32 node_type);
u32 userspace_fs_list(
    u32 node_capability_handle,
    u32 output_buffer_capability_handle,
    u32 byte_capacity);
u32 userspace_fs_read(
    u32 node_capability_handle,
    u32 output_buffer_capability_handle,
    u32 file_offset,
    u32 byte_count);
u32 userspace_fs_stat(
    u32 node_capability_handle,
    u32 output_buffer_capability_handle,
    u32 byte_capacity);
u32 userspace_fs_rename(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 source_path_byte_count,
    u32 destination_path_byte_count);
u32 userspace_fs_move(
    u32 source_base_capability_handle,
    u32 destination_base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 packed_path_lengths);
u32 userspace_fs_delete(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 path_byte_count);
u32 userspace_fs_write(
    u32 node_capability_handle,
    u32 input_buffer_capability_handle,
    u32 file_offset,
    u32 byte_count);
s32 userspace_send_message(u32 endpoint_capability_handle, u32 type, u32 payload_buffer_address, u32 payload_word_count);
s32 userspace_deliver_message(
    u32 endpoint_id,
    u32 source_endpoint,
    u32 type,
    const u32 *payload_words,
    u32 payload_word_count);
int userspace_is_endpoint(u32 endpoint_id);
struct interrupt_frame *userspace_handle_yield(struct interrupt_frame *frame);
struct interrupt_frame *userspace_handle_sleep(struct interrupt_frame *frame, u32 ticks);
struct interrupt_frame *userspace_handle_wait_message(struct interrupt_frame *frame);
struct interrupt_frame *userspace_handle_input_read(struct interrupt_frame *frame);
struct interrupt_frame *userspace_handle_wait_process(struct interrupt_frame *frame, u32 process_id);
struct interrupt_frame *userspace_handle_exit(struct interrupt_frame *frame, u32 exit_code);
struct interrupt_frame *userspace_handle_timer_tick(struct interrupt_frame *frame);
void userspace_note_input_ready(void);
u32 userspace_syscall_count(void);
u32 userspace_process_count(void);
u32 userspace_total_sleep_count(void);
u32 userspace_total_yield_count(void);
u32 userspace_total_preemption_count(void);
u32 userspace_registered_endpoint_count(void);
u32 userspace_total_ipc_send_count(void);
u32 userspace_total_ipc_denied_count(void);
u32 userspace_total_ipc_wait_count(void);
u32 userspace_total_ipc_message_count(void);
u32 userspace_total_capability_grant_count(void);
u32 userspace_total_capability_revoke_count(void);
u32 userspace_total_capability_delegation_count(void);
u32 userspace_total_capability_expiration_count(void);
u32 userspace_total_policy_denial_count(void);
u32 userspace_total_dispatch_count(void);
u32 userspace_total_latency_pick_count(void);
u32 userspace_total_deadline_pick_count(void);
u32 userspace_total_io_wake_count(void);
u32 userspace_total_budget_throttle_count(void);
u32 userspace_total_capability_admission_denial_count(void);
u32 userspace_total_capability_reuse_count(void);
u32 userspace_total_capability_compaction_count(void);
u32 userspace_total_buffer_registration_count(void);
u32 userspace_total_buffer_copy_count(void);
u32 userspace_total_process_exit_count(void);
u32 userspace_total_console_write_count(void);
u32 userspace_total_input_read_count(void);
u32 userspace_total_fs_open_count(void);
u32 userspace_total_fs_create_count(void);
u32 userspace_total_fs_list_count(void);
u32 userspace_total_fs_read_count(void);
u32 userspace_total_fs_stat_count(void);
u32 userspace_total_fs_rename_count(void);
u32 userspace_total_fs_move_count(void);
u32 userspace_total_fs_delete_count(void);
u32 userspace_total_fs_write_count(void);
u32 userspace_total_package_load_count(void);
u32 userspace_total_package_rejection_count(void);
u32 userspace_total_signer_verification_count(void);
u32 userspace_total_signer_denial_count(void);
u32 userspace_total_manifest_verification_count(void);
u32 userspace_total_manifest_denial_count(void);
u32 userspace_interactive_policy_waiter_count(void);
int userspace_is_active(void);

#endif
