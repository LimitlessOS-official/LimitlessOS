#include "runtime_image_x64.h"

#include "runtime_image_x64_generated.h"

const void *runtime64_transfer_image_base(void)
{
    return g_runtime64_transfer_image;
}

u32 runtime64_transfer_image_size(void)
{
    return (u32)sizeof(g_runtime64_transfer_image);
}

u32 runtime64_transfer_entry_result(void)
{
    return RUNTIME64_TRANSFER_ENTRY_RESULT;
}

u32 runtime64_transfer_user_entry_offset(void)
{
    return RUNTIME64_TRANSFER_USER_ENTRY_OFFSET;
}

u32 runtime64_transfer_user_syscall_result(void)
{
    return RUNTIME64_TRANSFER_USER_SYSCALL_RESULT;
}

u32 runtime64_transfer_user_preempt_entry_offset(void)
{
    return RUNTIME64_TRANSFER_USER_PREEMPT_ENTRY_OFFSET;
}

u32 runtime64_transfer_user_preempt_result(void)
{
    return RUNTIME64_TRANSFER_USER_PREEMPT_RESULT;
}

u32 runtime64_transfer_user_switch_source_offset(void)
{
    return RUNTIME64_TRANSFER_USER_SWITCH_SOURCE_OFFSET;
}

u32 runtime64_transfer_user_switch_target_offset(void)
{
    return RUNTIME64_TRANSFER_USER_SWITCH_TARGET_OFFSET;
}

u32 runtime64_transfer_user_switch_result(void)
{
    return RUNTIME64_TRANSFER_USER_SWITCH_RESULT;
}

u32 runtime64_transfer_user_switch_target_stack_delta(void)
{
    return RUNTIME64_TRANSFER_USER_SWITCH_TARGET_STACK_DELTA;
}

u32 runtime64_transfer_user_runqueue_source_offset(void)
{
    return RUNTIME64_TRANSFER_USER_RUNQUEUE_SOURCE_OFFSET;
}

u32 runtime64_transfer_user_runqueue_target_offset(void)
{
    return RUNTIME64_TRANSFER_USER_RUNQUEUE_TARGET_OFFSET;
}

u32 runtime64_transfer_user_runqueue_source_result(void)
{
    return RUNTIME64_TRANSFER_USER_RUNQUEUE_SOURCE_RESULT;
}

u32 runtime64_transfer_user_runqueue_target_result(void)
{
    return RUNTIME64_TRANSFER_USER_RUNQUEUE_TARGET_RESULT;
}

u32 runtime64_transfer_user_runqueue_target_stack_delta(void)
{
    return RUNTIME64_TRANSFER_USER_RUNQUEUE_TARGET_STACK_DELTA;
}

u32 runtime64_transfer_user_fs_probe_offset(void)
{
    return RUNTIME64_TRANSFER_USER_FS_PROBE_OFFSET;
}

u32 runtime64_transfer_user_fs_result(void)
{
    return RUNTIME64_TRANSFER_USER_FS_RESULT;
}

u32 runtime64_transfer_user_fs_error_result(void)
{
    return RUNTIME64_TRANSFER_USER_FS_ERROR_RESULT;
}

u32 runtime64_transfer_user_fs_read_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_FS_READ_BYTES;
}

u32 runtime64_transfer_user_keyboard_probe_offset(void)
{
    return RUNTIME64_TRANSFER_USER_KEYBOARD_PROBE_OFFSET;
}

u32 runtime64_transfer_user_keyboard_result(void)
{
    return RUNTIME64_TRANSFER_USER_KEYBOARD_RESULT;
}

u32 runtime64_transfer_user_keyboard_error_result(void)
{
    return RUNTIME64_TRANSFER_USER_KEYBOARD_ERROR_RESULT;
}

u32 runtime64_transfer_user_keyboard_read_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_KEYBOARD_READ_BYTES;
}

u32 runtime64_transfer_user_hardware_shell_probe_offset(void)
{
    return RUNTIME64_TRANSFER_USER_HARDWARE_SHELL_PROBE_OFFSET;
}

u32 runtime64_transfer_user_hardware_shell_result(void)
{
    return RUNTIME64_TRANSFER_USER_HARDWARE_SHELL_RESULT;
}

u32 runtime64_transfer_user_hardware_shell_error_result(void)
{
    return RUNTIME64_TRANSFER_USER_HARDWARE_SHELL_ERROR_RESULT;
}

u32 runtime64_transfer_user_hardware_shell_expected_commands(void)
{
    return RUNTIME64_TRANSFER_USER_HARDWARE_SHELL_EXPECTED_COMMANDS;
}

u32 runtime64_transfer_user_hardware_shell_required_flags(void)
{
    return RUNTIME64_TRANSFER_USER_HARDWARE_SHELL_REQUIRED_FLAGS;
}

u32 runtime64_transfer_user_hardware_shell_exit_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_HARDWARE_SHELL_EXIT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_hardware_shell_bye_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_HARDWARE_SHELL_BYE_BYTES;
}

u32 runtime64_transfer_user_second_page_offset(void)
{
    return RUNTIME64_TRANSFER_USER_SECOND_PAGE_OFFSET;
}

u32 runtime64_transfer_user_second_page_result(void)
{
    return RUNTIME64_TRANSFER_USER_SECOND_PAGE_RESULT;
}

u32 runtime64_transfer_user_second_page_note_path_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SECOND_PAGE_NOTE_PATH_BYTES;
}

u32 runtime64_transfer_user_second_page_note_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SECOND_PAGE_NOTE_BYTES;
}
