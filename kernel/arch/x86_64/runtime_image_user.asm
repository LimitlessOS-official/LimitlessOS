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
%assign SYSCALL_DISPLAY_DRAW_MARKER 278
%assign SYSCALL_DISPLAY_WRITE_TEXT 287
%assign SYSCALL_DISPLAY_CLEAR_TEXT_PANEL 290
%assign SYSCALL_INPUT_READ_KEYBOARD 307
%assign SYSCALL_SHELL_EXECUTE_LINE 3470

%assign ENTRY_RESULT 0x36534F4C
%assign PREEMPT_RESULT 0x5052454D
%assign SWITCH_RESULT 0x53574348
%assign SWITCH_TIMEOUT_RESULT 0x544F5554
%assign RUNQUEUE_SOURCE_RESULT 0x52514131
%assign RUNQUEUE_TARGET_RESULT 0x52514232
%assign FS_RESULT 0x46535233
%assign FS_ERROR_RESULT 0x46455252
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
%assign NEWLINE_LEN 1
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
%assign USER_NEWLINE_OFFSET 0x280
%assign ROOT_PATH_OFFSET 0x530
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

pad_to USER_NEWLINE_OFFSET
db 10

pad_to ROOT_PATH_OFFSET
db "/"

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
    mov ebx, FS_ERROR_RESULT
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
    console_write_address USER_BASE + USER_NEWLINE_OFFSET, NEWLINE_LEN
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
