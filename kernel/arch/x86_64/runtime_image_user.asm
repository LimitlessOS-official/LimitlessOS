bits 64
org 0

%assign USER_BASE 0x41000000
%assign OWNER_CONSOLE_CLIENT 0x00000201

%assign RIGHT_SEND 0x00000001
%assign RIGHT_QUERY 0x00000004
%assign RIGHT_SEND_QUERY (RIGHT_SEND | RIGHT_QUERY)

%assign SERVICE_CONSOLE 4
%assign SERVICE_RAMFS 5
%assign SERVICE_INPUT 6
%assign SERVICE_DISPLAY 7

%assign SYSCALL_CAP_GRANT_SERVICE 31
%assign SYSCALL_USERMODE_PROBE_EXIT 243
%assign SYSCALL_FS_OPEN 248
%assign SYSCALL_FS_CREATE 249
%assign SYSCALL_FS_LIST 250
%assign SYSCALL_FS_READ 251
%assign SYSCALL_FS_WRITE 252
%assign SYSCALL_FS_STAT 253
%assign SYSCALL_FS_REVOKE 254
%assign SYSCALL_CONSOLE_WRITE 267
%assign SYSCALL_INPUT_READ 271
%assign SYSCALL_INPUT_READ_LINE 276
%assign SYSCALL_DISPLAY_DRAW_MARKER 278
%assign SYSCALL_DISPLAY_WRITE_TEXT 287
%assign SYSCALL_DISPLAY_CLEAR_TEXT_PANEL 290
%assign SYSCALL_INPUT_READ_KEYBOARD 307
%assign SYSCALL_INPUT_READ_KEYBOARD_LINE 310
%assign SYSCALL_SHELL_EXECUTE_LINE 3470

%assign ENTRY_RESULT 0x36534F4C
%assign PREEMPT_RESULT 0x5052454D
%assign SWITCH_RESULT 0x53574348
%assign SWITCH_TIMEOUT_RESULT 0x544F5554
%assign RUNQUEUE_SOURCE_RESULT 0x52514131
%assign RUNQUEUE_TARGET_RESULT 0x52514232
%assign FS_RESULT 0x46535233
%assign FS_ERROR_RESULT 0x46455252
%assign CLI_RESULT 0x434C4931
%assign CLI_ERROR_RESULT 0x43455252
%assign INPUT_CLI_RESULT 0x49434C31
%assign INPUT_CLI_ERROR_RESULT 0x49455252
%assign SHELL_STREAM_RESULT 0x53484C31
%assign SHELL_STREAM_ERROR_RESULT 0x53455252
%assign KEYBOARD_RESULT 0x4B455931
%assign KEYBOARD_ERROR_RESULT 0x4B455252
%assign HARDWARE_SHELL_RESULT 0x48534831
%assign HARDWARE_SHELL_ERROR_RESULT 0x48534552
%assign SECOND_PAGE_RESULT 0x32504752

%assign RUNTIME_IMAGE_BYTES 16384
%assign PREEMPT_SPIN_COUNT 0x00400000
%assign SWITCH_SPIN_COUNT 0x00080000
%assign RUNQUEUE_SPIN_COUNT 0x00080000
%assign FS_READ_BYTES 0x20
%assign RAMFS_NODE_FILE 2
%assign README_PATH_LEN 10
%assign SECOND_PAGE_NOTE_PATH_LEN 11
%assign SECOND_PAGE_NOTE_TEXT_LEN 15
%assign CLI_PROMPT_LEN 28
%assign NEWLINE_LEN 1
%assign INPUT_PROMPT_LEN 14
%assign INPUT_COMMAND_LEN 14
%assign SHELL_STREAM_HELP_COMMAND_LEN 4
%assign SHELL_STREAM_HELP_LS_COMMAND_LEN 7
%assign SHELL_STREAM_HELP_CAT_COMMAND_LEN 8
%assign SHELL_STREAM_HELP_STAT_COMMAND_LEN 9
%assign SHELL_STREAM_HELP_MKDIR_COMMAND_LEN 10
%assign SHELL_STREAM_HELP_WRITE_COMMAND_LEN 10
%assign SHELL_STREAM_APPS_COMMAND_LEN 4
%assign SHELL_STREAM_PWD_COMMAND_LEN 3
%assign SHELL_STREAM_LS_ROOT_COMMAND_LEN 4
%assign SHELL_STREAM_LS_COMMAND_LEN 7
%assign SHELL_STREAM_INFO_LS_COMMAND_LEN 7
%assign SHELL_STREAM_INFO_CAT_COMMAND_LEN 8
%assign SHELL_STREAM_INFO_STAT_COMMAND_LEN 9
%assign SHELL_STREAM_INFO_MKDIR_COMMAND_LEN 10
%assign SHELL_STREAM_INFO_WRITE_COMMAND_LEN 10
%assign SHELL_STREAM_CAT_SHELL_COMMAND_LEN 13
%assign SHELL_STREAM_CAT_COMMAND_LEN 14
%assign SHELL_STREAM_STAT_COMMAND_LEN 15
%assign SHELL_STREAM_WRITE_SHELL_COMMAND_LEN 15
%assign SHELL_STREAM_COMMAND_LEN (SHELL_STREAM_HELP_COMMAND_LEN + SHELL_STREAM_HELP_LS_COMMAND_LEN + SHELL_STREAM_HELP_CAT_COMMAND_LEN + SHELL_STREAM_HELP_STAT_COMMAND_LEN + SHELL_STREAM_HELP_MKDIR_COMMAND_LEN + SHELL_STREAM_HELP_WRITE_COMMAND_LEN + SHELL_STREAM_WRITE_SHELL_COMMAND_LEN + SHELL_STREAM_CAT_SHELL_COMMAND_LEN + SHELL_STREAM_APPS_COMMAND_LEN + SHELL_STREAM_PWD_COMMAND_LEN + SHELL_STREAM_LS_ROOT_COMMAND_LEN + SHELL_STREAM_LS_COMMAND_LEN + SHELL_STREAM_INFO_LS_COMMAND_LEN + SHELL_STREAM_INFO_CAT_COMMAND_LEN + SHELL_STREAM_INFO_STAT_COMMAND_LEN + SHELL_STREAM_INFO_MKDIR_COMMAND_LEN + SHELL_STREAM_INFO_WRITE_COMMAND_LEN + SHELL_STREAM_CAT_COMMAND_LEN + SHELL_STREAM_STAT_COMMAND_LEN)
%assign SHELL_STREAM_COMMAND_CAPACITY 32
%assign SHELL_STREAM_EXPECTED_COMMANDS 19
%assign SHELL_STREAM_EXPECTED_UNKNOWNS 2
%assign SHELL_STREAM_HELP_BYTES 168
%assign SHELL_STREAM_HELP_LS_OUTPUT_BYTES 60
%assign SHELL_STREAM_HELP_CAT_OUTPUT_BYTES 33
%assign SHELL_STREAM_HELP_STAT_OUTPUT_BYTES 46
%assign SHELL_STREAM_HELP_MKDIR_OUTPUT_BYTES 39
%assign SHELL_STREAM_HELP_WRITE_OUTPUT_BYTES 44
%assign SHELL_STREAM_APPS_INDEX_BYTES 168
%assign SHELL_STREAM_LS_APP_BYTES 79
%assign SHELL_STREAM_CAT_APP_BYTES 52
%assign SHELL_STREAM_STAT_APP_BYTES 65
%assign SHELL_STREAM_MKDIR_APP_BYTES 58
%assign SHELL_STREAM_WRITE_APP_BYTES 64
%assign SHELL_STREAM_INFO_LS_OUTPUT_BYTES 96
%assign SHELL_STREAM_INFO_CAT_OUTPUT_BYTES 99
%assign SHELL_STREAM_INFO_STAT_OUTPUT_BYTES 102
%assign SHELL_STREAM_INFO_MKDIR_OUTPUT_BYTES 105
%assign SHELL_STREAM_INFO_WRITE_OUTPUT_BYTES 113
%assign SHELL_STREAM_PWD_BYTES 1
%assign SHELL_STREAM_ROOT_LIST_BYTES 52
%assign SHELL_STREAM_LIST_BYTES 168
%assign SHELL_STREAM_CAT_BYTES FS_READ_BYTES
%assign SHELL_STREAM_STAT_BYTES 19
%assign SHELL_STREAM_UNKNOWN_COMMAND_BYTES 9
%assign SHELL_STREAM_UNKNOWN_MESSAGE_LEN 15
%assign HARDWARE_SHELL_COMMAND_CAPACITY 96
%assign HARDWARE_SHELL_EXPECTED_COMMANDS 13
%assign HARDWARE_SHELL_LINE_WAIT_SPINS 0x01000000
%assign HARDWARE_SHELL_EXIT_COMMAND_LEN 4
%assign HARDWARE_SHELL_BYE_BYTES 4
%assign HARDWARE_SHELL_LS_APPS_COMMAND_LEN 7
%assign HARDWARE_SHELL_HELP_CAT_COMMAND_LEN 8
%assign HARDWARE_SHELL_HELP_WRITE_COMMAND_LEN 10
%assign HARDWARE_SHELL_INFO_WRITE_COMMAND_LEN 10
%assign HARDWARE_SHELL_WRITE_COMMAND_LEN 12
%assign HARDWARE_SHELL_CAT_HW_COMMAND_LEN 10
%assign HARDWARE_SHELL_WRITE_PATH_LEN 6
%assign HARDWARE_SHELL_WRITE_TEXT_LEN 12
%assign HARDWARE_SHELL_WRITE_OK_BYTES 13
%assign HARDWARE_SHELL_FLAG_HELP 0x00000001
%assign HARDWARE_SHELL_FLAG_HELP_CAT 0x00000002
%assign HARDWARE_SHELL_FLAG_HELP_WRITE 0x00000004
%assign HARDWARE_SHELL_FLAG_APPS 0x00000008
%assign HARDWARE_SHELL_FLAG_PWD 0x00000010
%assign HARDWARE_SHELL_FLAG_LS_ROOT 0x00000020
%assign HARDWARE_SHELL_FLAG_LS_APPS 0x00000040
%assign HARDWARE_SHELL_FLAG_INFO_WRITE 0x00000080
%assign HARDWARE_SHELL_FLAG_CAT_README 0x00000100
%assign HARDWARE_SHELL_FLAG_STAT_README 0x00000200
%assign HARDWARE_SHELL_FLAG_WRITE_HW 0x00000400
%assign HARDWARE_SHELL_FLAG_CAT_HW 0x00000800
%assign HARDWARE_SHELL_REQUIRED_FLAGS 0x00000FFF
%assign ROOT_PATH_LEN 1
%assign APPS_PATH_LEN 4
%assign APPS_INDEX_PATH_LEN 14
%assign LS_APP_PATH_LEN 11
%assign CAT_APP_PATH_LEN 12
%assign STAT_APP_PATH_LEN 13
%assign MKDIR_APP_PATH_LEN 14
%assign WRITE_APP_PATH_LEN 14
%assign SHELL_WRITE_PATH_LEN 9
%assign SHELL_WRITE_TEXT_LEN 15
%assign KEYBOARD_READ_LEN 2
%assign INTERACTIVE_SHELL_STACK_BYTES 0x100
%assign INTERACTIVE_SHELL_LINE_OFF 0x00
%assign INTERACTIVE_SHELL_BYTE_OFF 0x80
%assign INTERACTIVE_SHELL_CONSOLE_CAP_OFF 0x90
%assign INTERACTIVE_SHELL_INPUT_CAP_OFF 0x94
%assign INTERACTIVE_SHELL_RAMFS_CAP_OFF 0x98
%assign INTERACTIVE_SHELL_ROOT_CAP_OFF 0x9C
%assign INTERACTIVE_SHELL_LINE_LEN_OFF 0xA0
%assign INTERACTIVE_SHELL_LINE_CAPACITY 96

%assign USER_ENTRY_OFFSET 0x10
%assign USER_PREEMPT_OFFSET 0x40
%assign USER_SWITCH_SOURCE_OFFSET 0x80
%assign USER_SWITCH_TARGET_OFFSET 0xC0
%assign USER_RUNQUEUE_SOURCE_OFFSET 0x100
%assign USER_RUNQUEUE_TARGET_OFFSET 0x140
%assign USER_FS_PROBE_OFFSET 0x180
%assign README_PATH_OFFSET 0x240
%assign USER_CLI_PROBE_OFFSET 0x280
%assign USER_CLI_PROMPT_OFFSET 0x360
%assign USER_CLI_NEWLINE_OFFSET 0x380
%assign USER_INPUT_PROMPT_OFFSET 0x390
%assign USER_INPUT_CLI_PROBE_OFFSET 0x3C0
%assign APPS_PATH_OFFSET 0x520
%assign ROOT_PATH_OFFSET 0x530
%assign APPS_INDEX_PATH_OFFSET 0x532
%assign USER_SHELL_STREAM_PROBE_OFFSET 0x540
%assign USER_SECOND_PAGE_PROBE_OFFSET 0x1ED0
%assign USER_KEYBOARD_PROBE_OFFSET 0x2D80
%assign USER_HARDWARE_SHELL_PROBE_OFFSET 0x3000

%macro pad_to 1
    times (%1 - ($ - $$)) db 0
%endmacro

%macro exit_with 1
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    mov ebx, %1
    int 0x80
    jmp $
%endmacro

%macro spin_then_exit 2
    mov ecx, %1
%%spin:
    pause
    sub ecx, 1
    jne %%spin
    exit_with %2
%endmacro

%macro grant_service 1
    mov eax, SYSCALL_CAP_GRANT_SERVICE
    mov ebx, %1
    mov ecx, RIGHT_SEND_QUERY
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_readme 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + README_PATH_OFFSET
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | README_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_apps 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + APPS_PATH_OFFSET
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | APPS_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_apps_index 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + APPS_INDEX_PATH_OFFSET
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | APPS_INDEX_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_ls_app 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + shell_ls_app_path
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | LS_APP_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_cat_app 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + shell_cat_app_path
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | CAT_APP_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_stat_app 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + shell_stat_app_path
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | STAT_APP_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_mkdir_app 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + shell_mkdir_app_path
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | MKDIR_APP_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_write_app 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + shell_write_app_path
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | WRITE_APP_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_hardware_shell_note 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + hardware_shell_write_path
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | HARDWARE_SHELL_WRITE_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_open_root 0
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + ROOT_PATH_OFFSET
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | ROOT_PATH_LEN
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_create_file 2
    mov eax, SYSCALL_FS_CREATE
    mov ecx, %1
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | (RAMFS_NODE_FILE << 16) | %2
    int 0x80
    mov rbx, rax
%endmacro

%macro fs_read_stack 1
    mov eax, SYSCALL_FS_READ
    mov rcx, rsp
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %1
    int 0x80
%endmacro

%macro fs_read_stack_at 2
    mov eax, SYSCALL_FS_READ
    lea rcx, [rsp + %1]
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %2
    int 0x80
%endmacro

%macro fs_write_address 2
    mov eax, SYSCALL_FS_WRITE
    mov ecx, %1
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %2
    int 0x80
%endmacro

%macro fs_list_stack_at 2
    mov eax, SYSCALL_FS_LIST
    lea rcx, [rsp + %1]
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %2
    int 0x80
%endmacro

%macro fs_stat_stack_at 2
    mov eax, SYSCALL_FS_STAT
    lea rcx, [rsp + %1]
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %2
    int 0x80
%endmacro

%macro fs_revoke_current 0
    mov eax, SYSCALL_FS_REVOKE
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
%endmacro

%macro console_write_address 2
    mov eax, SYSCALL_CONSOLE_WRITE
    mov ecx, %1
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %2
    int 0x80
%endmacro

%macro console_write_stack_at 2
    mov eax, SYSCALL_CONSOLE_WRITE
    lea rcx, [rsp + %1]
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %2
    int 0x80
%endmacro

%macro console_write_stack 1
    mov eax, SYSCALL_CONSOLE_WRITE
    mov rcx, rsp
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | %1
    int 0x80
%endmacro

; supervisor-side validation entry: mov eax, ENTRY_RESULT; ret
mov eax, ENTRY_RESULT
ret

pad_to USER_ENTRY_OFFSET
exit_with ENTRY_RESULT

pad_to USER_PREEMPT_OFFSET
spin_then_exit PREEMPT_SPIN_COUNT, PREEMPT_RESULT

pad_to USER_SWITCH_SOURCE_OFFSET
spin_then_exit SWITCH_SPIN_COUNT, SWITCH_TIMEOUT_RESULT

pad_to USER_SWITCH_TARGET_OFFSET
exit_with SWITCH_RESULT

pad_to USER_RUNQUEUE_SOURCE_OFFSET
spin_then_exit RUNQUEUE_SPIN_COUNT, RUNQUEUE_SOURCE_RESULT

pad_to USER_RUNQUEUE_TARGET_OFFSET
exit_with RUNQUEUE_TARGET_RESULT

pad_to USER_FS_PROBE_OFFSET
    sub rsp, 0x40
    grant_service SERVICE_RAMFS
    fs_open_readme
    fs_read_stack FS_READ_BYTES
    cmp rax, FS_READ_BYTES
    jne .fs_fail
    mov ebx, FS_RESULT
    jmp .fs_exit
.fs_fail:
    mov ebx, FS_ERROR_RESULT
.fs_exit:
    add rsp, 0x40
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $

pad_to README_PATH_OFFSET
db "README.TXT"

pad_to USER_CLI_PROBE_OFFSET
    sub rsp, 0x40
    grant_service SERVICE_RAMFS
    fs_open_readme
    fs_read_stack FS_READ_BYTES
    cmp rax, FS_READ_BYTES
    jne .cli_fail
    grant_service SERVICE_CONSOLE
    console_write_address USER_BASE + USER_CLI_PROMPT_OFFSET, CLI_PROMPT_LEN
    console_write_stack FS_READ_BYTES
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .cli_fail
    mov ebx, CLI_RESULT
    jmp .cli_exit
.cli_fail:
    mov ebx, CLI_ERROR_RESULT
.cli_exit:
    add rsp, 0x40
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $

pad_to USER_CLI_PROMPT_OFFSET
db "[x64:user] $ cat README.TXT", 10

pad_to USER_CLI_NEWLINE_OFFSET
db 10

pad_to USER_INPUT_PROMPT_OFFSET
db "[x64:input] $ "

pad_to USER_INPUT_CLI_PROBE_OFFSET
    sub rsp, 0x40
    grant_service SERVICE_INPUT
    mov eax, SYSCALL_INPUT_READ
    mov rcx, rsp
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | INPUT_COMMAND_LEN
    int 0x80
    cmp rax, INPUT_COMMAND_LEN
    jne .input_fail

    grant_service SERVICE_CONSOLE
    mov r8, rax
    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .input_fail
    mov rbx, r8
    console_write_stack INPUT_COMMAND_LEN
    cmp rax, INPUT_COMMAND_LEN
    jne .input_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .input_fail

    grant_service SERVICE_RAMFS
    fs_open_readme
    fs_read_stack FS_READ_BYTES
    cmp rax, FS_READ_BYTES
    jne .input_fail
    mov rbx, r8
    console_write_stack FS_READ_BYTES
    cmp rax, FS_READ_BYTES
    jne .input_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .input_fail
    mov ebx, INPUT_CLI_RESULT
    jmp .input_exit
.input_fail:
    mov ebx, INPUT_CLI_ERROR_RESULT
.input_exit:
    add rsp, 0x40
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $

pad_to APPS_PATH_OFFSET
db "APPS"

pad_to ROOT_PATH_OFFSET
db "/"

pad_to APPS_INDEX_PATH_OFFSET
db "APPS/INDEX.TXT"

pad_to USER_SHELL_STREAM_PROBE_OFFSET
    sub rsp, 0x100
    mov dword [rsp + 0xF0], 0
    mov dword [rsp + 0xF4], 0

    grant_service SERVICE_INPUT
    mov r9, rax
    grant_service SERVICE_CONSOLE
    mov r8, rax
    grant_service SERVICE_RAMFS
    mov r12, rax

.stream_loop:
    mov rbx, r9
    mov eax, SYSCALL_INPUT_READ_LINE
    mov rcx, rsp
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | SHELL_STREAM_COMMAND_CAPACITY
    int 0x80
    cmp rax, 0
    je .stream_eof
    mov dword [rsp + 0xF8], eax
    cmp rax, SHELL_STREAM_HELP_COMMAND_LEN
    je .stream_check_len4
    cmp rax, SHELL_STREAM_PWD_COMMAND_LEN
    je .stream_check_pwd
    cmp rax, SHELL_STREAM_LS_COMMAND_LEN
    je .stream_check_len7
    cmp rax, SHELL_STREAM_HELP_CAT_COMMAND_LEN
    je .stream_check_len8
    cmp rax, SHELL_STREAM_HELP_STAT_COMMAND_LEN
    je .stream_check_len9
    cmp rax, SHELL_STREAM_HELP_MKDIR_COMMAND_LEN
    je .stream_check_len10
    cmp rax, SHELL_STREAM_CAT_SHELL_COMMAND_LEN
    je .stream_check_cat_shell
    cmp rax, SHELL_STREAM_CAT_COMMAND_LEN
    je .stream_check_cat
    cmp rax, SHELL_STREAM_STAT_COMMAND_LEN
    je .stream_check_stat
    jmp .stream_unknown
.stream_check_len4:
    cmp dword [rsp], 0x706C6568
    je .stream_do_help
    cmp dword [rsp], 0x73707061
    je .stream_do_apps
    cmp dword [rsp], 0x2F20736C
    je .stream_do_ls_root
    jmp .stream_unknown

.stream_do_help:
    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_HELP_COMMAND_LEN
    cmp rax, SHELL_STREAM_HELP_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + shell_m1_inventory_message, SHELL_STREAM_HELP_BYTES
    cmp rax, SHELL_STREAM_HELP_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_do_apps:
    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_APPS_COMMAND_LEN
    cmp rax, SHELL_STREAM_APPS_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r8
    console_write_address USER_BASE + shell_m1_inventory_message, SHELL_STREAM_APPS_INDEX_BYTES
    cmp rax, SHELL_STREAM_APPS_INDEX_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_pwd:
    cmp word [rsp], 0x7770
    jne .stream_unknown
    cmp byte [rsp + 2], 0x64
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_PWD_COMMAND_LEN
    cmp rax, SHELL_STREAM_PWD_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + shell_pwd_message, SHELL_STREAM_PWD_BYTES
    cmp rax, SHELL_STREAM_PWD_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_do_ls_root:
    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_LS_ROOT_COMMAND_LEN
    cmp rax, SHELL_STREAM_LS_ROOT_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_root
    fs_list_stack_at 0x20, SHELL_STREAM_ROOT_LIST_BYTES
    cmp rax, SHELL_STREAM_ROOT_LIST_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_stack_at 0x20, SHELL_STREAM_ROOT_LIST_BYTES
    cmp rax, SHELL_STREAM_ROOT_LIST_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_len7:
    cmp dword [rsp], 0x706C6568
    je .stream_check_help_ls_tail
    cmp dword [rsp], 0x4120736C
    je .stream_check_ls_tail
    cmp dword [rsp], 0x6F666E69
    je .stream_check_info_ls_tail
    jmp .stream_unknown

.stream_check_help_ls_tail:
    cmp word [rsp + 4], 0x6C20
    jne .stream_unknown
    cmp byte [rsp + 6], 0x73
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_HELP_LS_COMMAND_LEN
    cmp rax, SHELL_STREAM_HELP_LS_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_ls_app
    fs_read_stack_at 0x20, SHELL_STREAM_LS_APP_BYTES
    cmp rax, SHELL_STREAM_LS_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A33
    jne .stream_fail

    mov rbx, r8
    console_write_stack_at 0x28, SHELL_STREAM_HELP_LS_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_HELP_LS_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_len8:
    cmp dword [rsp], 0x706C6568
    je .stream_check_help_cat_tail
    cmp dword [rsp], 0x6F666E69
    je .stream_check_info_cat_tail
    jmp .stream_unknown

.stream_check_help_cat_tail:
    cmp dword [rsp + 4], 0x74616320
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_HELP_CAT_COMMAND_LEN
    cmp rax, SHELL_STREAM_HELP_CAT_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_cat_app
    fs_read_stack_at 0x20, SHELL_STREAM_CAT_APP_BYTES
    cmp rax, SHELL_STREAM_CAT_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A34
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A33
    jne .stream_fail

    mov rbx, r8
    console_write_stack_at 0x28, SHELL_STREAM_HELP_CAT_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_HELP_CAT_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_info_cat_tail:
    cmp dword [rsp + 4], 0x74616320
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_INFO_CAT_COMMAND_LEN
    cmp rax, SHELL_STREAM_INFO_CAT_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_cat_app
    fs_read_stack_at 0x20, SHELL_STREAM_CAT_APP_BYTES
    cmp rax, SHELL_STREAM_CAT_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A34
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A33
    jne .stream_fail

    mov rbx, r8
    console_write_address USER_BASE + shell_info_cat_decoded_message, SHELL_STREAM_INFO_CAT_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_INFO_CAT_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_len9:
    cmp dword [rsp], 0x706C6568
    je .stream_check_help_stat_tail
    cmp dword [rsp], 0x6F666E69
    je .stream_check_info_stat_tail
    jmp .stream_unknown

.stream_check_help_stat_tail:
    cmp dword [rsp + 4], 0x61747320
    jne .stream_unknown
    cmp byte [rsp + 8], 0x74
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_HELP_STAT_COMMAND_LEN
    cmp rax, SHELL_STREAM_HELP_STAT_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_stat_app
    fs_read_stack_at 0x20, SHELL_STREAM_STAT_APP_BYTES
    cmp rax, SHELL_STREAM_STAT_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A37
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A33
    jne .stream_fail

    mov rbx, r8
    console_write_stack_at 0x28, SHELL_STREAM_HELP_STAT_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_HELP_STAT_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_info_stat_tail:
    cmp dword [rsp + 4], 0x61747320
    jne .stream_unknown
    cmp byte [rsp + 8], 0x74
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_INFO_STAT_COMMAND_LEN
    cmp rax, SHELL_STREAM_INFO_STAT_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_stat_app
    fs_read_stack_at 0x20, SHELL_STREAM_STAT_APP_BYTES
    cmp rax, SHELL_STREAM_STAT_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A37
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A33
    jne .stream_fail

    mov rbx, r8
    console_write_address USER_BASE + shell_info_stat_decoded_message, SHELL_STREAM_INFO_STAT_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_INFO_STAT_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_len10:
    cmp dword [rsp], 0x706C6568
    je .stream_check_help_mkdir_or_write_tail
    cmp dword [rsp], 0x6F666E69
    je .stream_check_info_mkdir_or_write_tail
    jmp .stream_unknown

.stream_check_help_mkdir_or_write_tail:
    cmp dword [rsp + 4], 0x646B6D20
    je .stream_check_help_mkdir_suffix
    cmp dword [rsp + 4], 0x69727720
    je .stream_check_help_write_suffix
    jmp .stream_unknown

.stream_check_help_mkdir_suffix:
    cmp word [rsp + 8], 0x7269
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_HELP_MKDIR_COMMAND_LEN
    cmp rax, SHELL_STREAM_HELP_MKDIR_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_mkdir_app
    fs_read_stack_at 0x20, SHELL_STREAM_MKDIR_APP_BYTES
    cmp rax, SHELL_STREAM_MKDIR_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A35
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A31
    jne .stream_fail

    mov rbx, r8
    console_write_stack_at 0x28, SHELL_STREAM_HELP_MKDIR_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_HELP_MKDIR_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_help_write_suffix:
    cmp word [rsp + 8], 0x6574
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_HELP_WRITE_COMMAND_LEN
    cmp rax, SHELL_STREAM_HELP_WRITE_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_write_app
    fs_read_stack_at 0x20, SHELL_STREAM_WRITE_APP_BYTES
    cmp rax, SHELL_STREAM_WRITE_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A36
    jne .stream_fail
    cmp word [rsp + 0x22], 0x3131
    jne .stream_fail
    cmp word [rsp + 0x24], 0x330A
    jne .stream_fail
    cmp word [rsp + 0x26], 0x310A
    jne .stream_fail
    cmp byte [rsp + 0x28], 0x0A
    jne .stream_fail

    mov rbx, r8
    console_write_stack_at 0x29, SHELL_STREAM_HELP_WRITE_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_HELP_WRITE_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_info_mkdir_or_write_tail:
    cmp dword [rsp + 4], 0x646B6D20
    je .stream_check_info_mkdir_suffix
    cmp dword [rsp + 4], 0x69727720
    je .stream_check_info_write_suffix
    jmp .stream_unknown

.stream_check_info_mkdir_suffix:
    cmp word [rsp + 8], 0x7269
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_INFO_MKDIR_COMMAND_LEN
    cmp rax, SHELL_STREAM_INFO_MKDIR_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_mkdir_app
    fs_read_stack_at 0x20, SHELL_STREAM_MKDIR_APP_BYTES
    cmp rax, SHELL_STREAM_MKDIR_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A35
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A31
    jne .stream_fail

    mov rbx, r8
    console_write_address USER_BASE + shell_info_mkdir_decoded_message, SHELL_STREAM_INFO_MKDIR_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_INFO_MKDIR_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_info_write_suffix:
    cmp word [rsp + 8], 0x6574
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_INFO_WRITE_COMMAND_LEN
    cmp rax, SHELL_STREAM_INFO_WRITE_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_write_app
    fs_read_stack_at 0x20, SHELL_STREAM_WRITE_APP_BYTES
    cmp rax, SHELL_STREAM_WRITE_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A36
    jne .stream_fail
    cmp word [rsp + 0x22], 0x3131
    jne .stream_fail
    cmp word [rsp + 0x24], 0x330A
    jne .stream_fail
    cmp word [rsp + 0x26], 0x310A
    jne .stream_fail
    cmp byte [rsp + 0x28], 0x0A
    jne .stream_fail

    mov rbx, r8
    console_write_address USER_BASE + shell_info_write_decoded_message, SHELL_STREAM_INFO_WRITE_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_INFO_WRITE_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_ls_tail:
    cmp word [rsp + 4], 0x5050
    jne .stream_unknown
    cmp byte [rsp + 6], 0x53
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_LS_COMMAND_LEN
    cmp rax, SHELL_STREAM_LS_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r8
    console_write_address USER_BASE + shell_m1_inventory_message, SHELL_STREAM_LIST_BYTES
    cmp rax, SHELL_STREAM_LIST_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_info_ls_tail:
    cmp word [rsp + 4], 0x6C20
    jne .stream_unknown
    cmp byte [rsp + 6], 0x73
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_INFO_LS_COMMAND_LEN
    cmp rax, SHELL_STREAM_INFO_LS_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_ls_app
    fs_read_stack_at 0x20, SHELL_STREAM_LS_APP_BYTES
    cmp rax, SHELL_STREAM_LS_APP_BYTES
    jne .stream_fail
    cmp word [rsp + 0x20], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x22], 0x0A33
    jne .stream_fail
    cmp word [rsp + 0x24], 0x0A32
    jne .stream_fail
    cmp word [rsp + 0x26], 0x0A33
    jne .stream_fail

    mov rbx, r8
    console_write_address USER_BASE + shell_info_ls_decoded_message, SHELL_STREAM_INFO_LS_OUTPUT_BYTES
    cmp rax, SHELL_STREAM_INFO_LS_OUTPUT_BYTES
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_cat_shell:
    cmp dword [rsp], 0x20746163
    jne .stream_unknown
    cmp dword [rsp + 4], 0x4C454853
    jne .stream_unknown
    cmp dword [rsp + 8], 0x58542E4C
    jne .stream_unknown
    cmp byte [rsp + 12], 0x54
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_CAT_SHELL_COMMAND_LEN
    cmp rax, SHELL_STREAM_CAT_SHELL_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    mov eax, SYSCALL_FS_OPEN
    mov ecx, USER_BASE + shell_write_path
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | SHELL_WRITE_PATH_LEN
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .stream_fail
    mov rbx, rax
    fs_read_stack_at 0xA0, SHELL_WRITE_TEXT_LEN
    cmp rax, SHELL_WRITE_TEXT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack_at 0xA0, SHELL_WRITE_TEXT_LEN
    cmp rax, SHELL_WRITE_TEXT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_cat:
    cmp dword [rsp], 0x20746163
    jne .stream_unknown
    cmp dword [rsp + 4], 0x44414552
    jne .stream_unknown
    cmp dword [rsp + 8], 0x542E454D
    jne .stream_unknown
    cmp word [rsp + 12], 0x5458
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_CAT_COMMAND_LEN
    cmp rax, SHELL_STREAM_CAT_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_readme
    fs_read_stack_at 0xA0, SHELL_STREAM_CAT_BYTES
    cmp rax, SHELL_STREAM_CAT_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_stack_at 0xA0, SHELL_STREAM_CAT_BYTES
    cmp rax, SHELL_STREAM_CAT_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_stat:
    cmp dword [rsp], 0x74617473
    jne .stream_check_write_shell
    cmp dword [rsp + 4], 0x41455220
    jne .stream_unknown
    cmp dword [rsp + 8], 0x2E454D44
    jne .stream_unknown
    cmp word [rsp + 12], 0x5854
    jne .stream_unknown
    cmp byte [rsp + 14], 0x54
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_STAT_COMMAND_LEN
    cmp rax, SHELL_STREAM_STAT_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_open_readme
    fs_stat_stack_at 0x40, SHELL_STREAM_STAT_BYTES
    cmp rax, SHELL_STREAM_STAT_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_stack_at 0x40, SHELL_STREAM_STAT_BYTES
    cmp rax, SHELL_STREAM_STAT_BYTES
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_check_write_shell:
    cmp dword [rsp], 0x74697277
    jne .stream_unknown
    cmp dword [rsp + 4], 0x48532065
    jne .stream_unknown
    cmp dword [rsp + 8], 0x2E4C4C45
    jne .stream_unknown
    cmp word [rsp + 12], 0x5854
    jne .stream_unknown
    cmp byte [rsp + 14], 0x54
    jne .stream_unknown

    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_stack SHELL_STREAM_WRITE_SHELL_COMMAND_LEN
    cmp rax, SHELL_STREAM_WRITE_SHELL_COMMAND_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    mov rbx, r12
    fs_create_file USER_BASE + shell_write_path, SHELL_WRITE_PATH_LEN
    cmp eax, 0xFFFFFFFF
    je .stream_fail
    mov r13, rax
    mov rbx, r13
    fs_write_address USER_BASE + shell_write_text, SHELL_WRITE_TEXT_LEN
    cmp rax, SHELL_WRITE_TEXT_LEN
    jne .stream_fail

    add dword [rsp + 0xF0], 1
    jmp .stream_loop

.stream_unknown:
    mov rbx, r8
    console_write_address USER_BASE + USER_INPUT_PROMPT_OFFSET, INPUT_PROMPT_LEN
    cmp rax, INPUT_PROMPT_LEN
    jne .stream_fail
    mov rbx, r8
    mov eax, SYSCALL_CONSOLE_WRITE
    mov rcx, rsp
    mov edx, dword [rsp + 0xF8]
    mov r11, OWNER_CONSOLE_CLIENT
    shl r11, 32
    or rdx, r11
    int 0x80
    cmp eax, dword [rsp + 0xF8]
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + shell_unknown_message, SHELL_STREAM_UNKNOWN_MESSAGE_LEN
    cmp rax, SHELL_STREAM_UNKNOWN_MESSAGE_LEN
    jne .stream_fail
    mov rbx, r8
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    cmp rax, NEWLINE_LEN
    jne .stream_fail

    add dword [rsp + 0xF4], 1
    jmp .stream_loop

.stream_eof:
    cmp dword [rsp + 0xF0], SHELL_STREAM_EXPECTED_COMMANDS
    jne .stream_fail
    cmp dword [rsp + 0xF4], SHELL_STREAM_EXPECTED_UNKNOWNS
    jne .stream_fail

    mov ebx, SHELL_STREAM_RESULT
    jmp .stream_exit
.stream_fail:
    mov ebx, SHELL_STREAM_ERROR_RESULT
.stream_exit:
    add rsp, 0x100
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $

shell_unknown_message:
db "unknown command"

shell_m1_inventory_message:
db "Builtins apps help info pwd", 10
db "Product apps: append cat copy delete ls mkdir move rename stat touch write", 10
db "Unavailable ASK-not-AI ECHO aliases;HELLO.TXT INDEX.TXT internal", 10

shell_pwd_message:
db "/"

shell_info_ls_decoded_message:
db "cmd=ls exec=utility-ls auth=buf+base policy=path bind=fg+console", 10
db "usage=ls [path] cat=filesystem", 10

shell_info_cat_decoded_message:
db "cmd=cat exec=utility-cat auth=buf+base policy=path bind=fg+console", 10
db "usage=cat <path> cat=filesystem", 10

shell_info_stat_decoded_message:
db "cmd=stat exec=utility-stat auth=buf+base policy=path bind=fg+console", 10
db "usage=stat <path> cat=filesystem", 10

shell_info_mkdir_decoded_message:
db "cmd=mkdir exec=utility-mkdir auth=buf+base policy=path bind=fg+console", 10
db "usage=mkdir <path> cat=filesystem", 10

shell_info_write_decoded_message:
db "cmd=write exec=utility-write auth=buf+base policy=write bind=fg+console", 10
db "usage=write <path> <text> cat=filesystem", 10

shell_ls_app_path:
db "APPS/LS.APP"

shell_cat_app_path:
db "APPS/CAT.APP"

shell_stat_app_path:
db "APPS/STAT.APP"

shell_mkdir_app_path:
db "APPS/MKDIR.APP"

shell_write_app_path:
db "APPS/WRITE.APP"

shell_write_path:
db "SHELL.TXT"

shell_write_text:
db "shell write ok", 10

pad_to USER_SECOND_PAGE_PROBE_OFFSET
    sub rsp, 0x40
    grant_service SERVICE_RAMFS
    fs_create_file USER_BASE + second_page_note_path, SECOND_PAGE_NOTE_PATH_LEN
    cmp eax, 0xFFFFFFFF
    je .second_page_fail
    mov r12, rax

    mov rbx, r12
    fs_write_address USER_BASE + second_page_note_text, SECOND_PAGE_NOTE_TEXT_LEN
    cmp rax, SECOND_PAGE_NOTE_TEXT_LEN
    jne .second_page_fail

    mov rbx, r12
    fs_read_stack_at 0x20, SECOND_PAGE_NOTE_TEXT_LEN
    cmp rax, SECOND_PAGE_NOTE_TEXT_LEN
    jne .second_page_fail
    cmp dword [rsp + 0x20], 0x676E6972
    jne .second_page_fail
    cmp dword [rsp + 0x24], 0x72772033
    jne .second_page_fail
    cmp dword [rsp + 0x28], 0x20657469
    jne .second_page_fail
    cmp word [rsp + 0x2C], 0x6B6F
    jne .second_page_fail
    cmp byte [rsp + 0x2E], 0x0A
    jne .second_page_fail

    grant_service SERVICE_DISPLAY
    mov eax, SYSCALL_DISPLAY_DRAW_MARKER
    mov ecx, (64 << 16) | 24
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | 0x00FFC857
    int 0x80
    mov eax, SYSCALL_DISPLAY_CLEAR_TEXT_PANEL
    int 0x80
    mov eax, SYSCALL_DISPLAY_WRITE_TEXT
    mov ecx, USER_BASE + second_page_note_text
    mov dx, SECOND_PAGE_NOTE_TEXT_LEN
    int 0x80

    mov ebx, SECOND_PAGE_RESULT
    jmp .second_page_exit
.second_page_fail:
    mov ebx, SHELL_STREAM_ERROR_RESULT
.second_page_exit:
    add rsp, 0x40
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $

second_page_note_path:
db "USRNOTE.TXT"

second_page_note_text:
db "ring3 write ok", 10

pad_to USER_HARDWARE_SHELL_PROBE_OFFSET
    cld
    sub rsp, INTERACTIVE_SHELL_STACK_BYTES

    grant_service SERVICE_CONSOLE
    cmp eax, 0xFFFFFFFF
    je .interactive_shell_halt
    mov [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF], eax

    grant_service SERVICE_INPUT
    cmp eax, 0xFFFFFFFF
    je .interactive_shell_halt
    mov [rsp + INTERACTIVE_SHELL_INPUT_CAP_OFF], eax

    grant_service SERVICE_RAMFS
    cmp eax, 0xFFFFFFFF
    je .interactive_shell_halt
    mov [rsp + INTERACTIVE_SHELL_RAMFS_CAP_OFF], eax

    mov ebx, [rsp + INTERACTIVE_SHELL_RAMFS_CAP_OFF]
    fs_open_root
    cmp eax, 0xFFFFFFFF
    je .interactive_shell_halt
    mov [rsp + INTERACTIVE_SHELL_ROOT_CAP_OFF], eax

    mov ebx, [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF]
    console_write_address USER_BASE + interactive_shell_banner, interactive_shell_banner_len

.interactive_shell_new_line:
    mov dword [rsp + INTERACTIVE_SHELL_LINE_LEN_OFF], 0
    mov ebx, [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF]
    console_write_address USER_BASE + interactive_shell_prompt, interactive_shell_prompt_len

.interactive_shell_read:
    mov eax, SYSCALL_INPUT_READ_KEYBOARD
    mov ebx, [rsp + INTERACTIVE_SHELL_INPUT_CAP_OFF]
    lea rcx, [rsp + INTERACTIVE_SHELL_BYTE_OFF]
    mov rdx, (OWNER_CONSOLE_CLIENT << 32) | 1
    int 0x80
    cmp eax, 0
    jne .interactive_shell_byte
    pause
    jmp .interactive_shell_read

.interactive_shell_byte:
    mov al, [rsp + INTERACTIVE_SHELL_BYTE_OFF]
    cmp al, 13
    je .interactive_shell_read
    cmp al, 10
    je .interactive_shell_execute
    cmp al, 8
    je .interactive_shell_backspace
    cmp al, 0x7F
    je .interactive_shell_backspace
    cmp al, 0x20
    jb .interactive_shell_read
    cmp al, 0x7E
    ja .interactive_shell_read

    mov ecx, [rsp + INTERACTIVE_SHELL_LINE_LEN_OFF]
    cmp ecx, INTERACTIVE_SHELL_LINE_CAPACITY
    jae .interactive_shell_read
    mov [rsp + INTERACTIVE_SHELL_LINE_OFF + rcx], al
    add ecx, 1
    mov [rsp + INTERACTIVE_SHELL_LINE_LEN_OFF], ecx
    mov ebx, [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF]
    console_write_stack_at INTERACTIVE_SHELL_BYTE_OFF, 1
    jmp .interactive_shell_read

.interactive_shell_backspace:
    mov ecx, [rsp + INTERACTIVE_SHELL_LINE_LEN_OFF]
    cmp ecx, 0
    je .interactive_shell_read
    sub ecx, 1
    mov [rsp + INTERACTIVE_SHELL_LINE_LEN_OFF], ecx
    mov ebx, [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF]
    console_write_address USER_BASE + interactive_shell_backspace, interactive_shell_backspace_len
    jmp .interactive_shell_read

.interactive_shell_execute:
    mov ebx, [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF]
    console_write_address USER_BASE + USER_CLI_NEWLINE_OFFSET, NEWLINE_LEN
    mov eax, SYSCALL_SHELL_EXECUTE_LINE
    mov ebx, [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF]
    mov r8d, [rsp + INTERACTIVE_SHELL_ROOT_CAP_OFF]
    shl r8, 32
    or rbx, r8
    lea rcx, [rsp + INTERACTIVE_SHELL_LINE_OFF]
    mov edx, [rsp + INTERACTIVE_SHELL_LINE_LEN_OFF]
    mov r8, OWNER_CONSOLE_CLIENT
    shl r8, 32
    or rdx, r8
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .interactive_shell_denied
    jmp .interactive_shell_new_line

.interactive_shell_denied:
    mov ebx, [rsp + INTERACTIVE_SHELL_CONSOLE_CAP_OFF]
    console_write_address USER_BASE + interactive_shell_denied, interactive_shell_denied_len
    jmp .interactive_shell_new_line

.interactive_shell_halt:
    pause
    jmp .interactive_shell_halt

interactive_shell_banner:
db 10, "[x64:shell] persistent ring3 shell online", 10
interactive_shell_banner_len equ $ - interactive_shell_banner

interactive_shell_prompt:
db "[x64] $ "
interactive_shell_prompt_len equ $ - interactive_shell_prompt

interactive_shell_backspace:
db 8, " ", 8
interactive_shell_backspace_len equ $ - interactive_shell_backspace

interactive_shell_denied:
db "[x64:shell] command denied", 10
interactive_shell_denied_len equ $ - interactive_shell_denied

times RUNTIME_IMAGE_BYTES - ($ - $$) db 0
