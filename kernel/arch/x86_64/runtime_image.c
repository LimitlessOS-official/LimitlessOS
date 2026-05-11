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

u32 runtime64_transfer_user_cli_probe_offset(void)
{
    return RUNTIME64_TRANSFER_USER_CLI_PROBE_OFFSET;
}

u32 runtime64_transfer_user_cli_result(void)
{
    return RUNTIME64_TRANSFER_USER_CLI_RESULT;
}

u32 runtime64_transfer_user_cli_error_result(void)
{
    return RUNTIME64_TRANSFER_USER_CLI_ERROR_RESULT;
}

u32 runtime64_transfer_user_cli_prompt_length(void)
{
    return RUNTIME64_TRANSFER_USER_CLI_PROMPT_LENGTH;
}

u32 runtime64_transfer_user_input_cli_probe_offset(void)
{
    return RUNTIME64_TRANSFER_USER_INPUT_CLI_PROBE_OFFSET;
}

u32 runtime64_transfer_user_input_cli_result(void)
{
    return RUNTIME64_TRANSFER_USER_INPUT_CLI_RESULT;
}

u32 runtime64_transfer_user_input_cli_error_result(void)
{
    return RUNTIME64_TRANSFER_USER_INPUT_CLI_ERROR_RESULT;
}

u32 runtime64_transfer_user_input_cli_prompt_length(void)
{
    return RUNTIME64_TRANSFER_USER_INPUT_CLI_PROMPT_LENGTH;
}

u32 runtime64_transfer_user_input_cli_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_INPUT_CLI_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_probe_offset(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_PROBE_OFFSET;
}

u32 runtime64_transfer_user_shell_stream_result(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_RESULT;
}

u32 runtime64_transfer_user_shell_stream_error_result(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_ERROR_RESULT;
}

u32 runtime64_transfer_user_shell_stream_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_command_capacity(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_COMMAND_CAPACITY;
}

u32 runtime64_transfer_user_shell_stream_expected_commands(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_EXPECTED_COMMANDS;
}

u32 runtime64_transfer_user_shell_stream_expected_unknowns(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_EXPECTED_UNKNOWNS;
}

u32 runtime64_transfer_user_shell_stream_help_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_ls_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_LS_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_cat_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_CAT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_stat_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_STAT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_mkdir_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_MKDIR_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_write_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_WRITE_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_apps_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_APPS_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_pwd_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_PWD_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_ls_root_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_LS_ROOT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_ls_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_LS_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_ls_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_LS_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_cat_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_CAT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_stat_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_STAT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_mkdir_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_MKDIR_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_write_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_WRITE_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_cat_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_CAT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_stat_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_STAT_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_unknown_command_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_UNKNOWN_COMMAND_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_ls_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_LS_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_cat_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_CAT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_stat_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_STAT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_mkdir_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_MKDIR_BYTES;
}

u32 runtime64_transfer_user_shell_stream_help_write_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_HELP_WRITE_BYTES;
}

u32 runtime64_transfer_user_shell_stream_apps_index_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_APPS_INDEX_BYTES;
}

u32 runtime64_transfer_user_shell_stream_ls_app_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_LS_APP_BYTES;
}

u32 runtime64_transfer_user_shell_stream_cat_app_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_CAT_APP_BYTES;
}

u32 runtime64_transfer_user_shell_stream_stat_app_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_STAT_APP_BYTES;
}

u32 runtime64_transfer_user_shell_stream_mkdir_app_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_MKDIR_APP_BYTES;
}

u32 runtime64_transfer_user_shell_stream_write_app_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_WRITE_APP_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_ls_output_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_LS_OUTPUT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_cat_output_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_CAT_OUTPUT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_stat_output_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_STAT_OUTPUT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_mkdir_output_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_MKDIR_OUTPUT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_info_write_output_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_INFO_WRITE_OUTPUT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_pwd_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_PWD_BYTES;
}

u32 runtime64_transfer_user_shell_stream_root_list_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_ROOT_LIST_BYTES;
}

u32 runtime64_transfer_user_shell_stream_list_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_LIST_BYTES;
}

u32 runtime64_transfer_user_shell_stream_cat_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_CAT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_stat_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_STAT_BYTES;
}

u32 runtime64_transfer_user_shell_stream_unknown_message_bytes(void)
{
    return RUNTIME64_TRANSFER_USER_SHELL_STREAM_UNKNOWN_MESSAGE_BYTES;
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
