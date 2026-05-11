[bits 32]

global _x86_resume_user_frame
global _x86_kernel_idle_loop
global _user_bootstrap_service_start
global _user_bootstrap_service_end

%define SYSCALL_GET_UPTIME_TICKS 0
%define SYSCALL_GET_SERVICE_COUNT 7
%define SYSCALL_GET_DENIED_IPC_COUNT 8
%define SYSCALL_USER_SLEEP_TICKS 13
%define SYSCALL_USER_POLICY_REQUEST 18
%define SYSCALL_USER_WAIT_IPC 19
%define SYSCALL_USER_SEND_IPC 22
%define SYSCALL_USER_REGISTER_ENDPOINT 23
%define SYSCALL_USER_LOOKUP_ENDPOINT 25
%define SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT 26
%define SYSCALL_USER_REVOKE_CAPABILITY 27
%define SYSCALL_USER_LOOKUP_ENDPOINT_CLASS 28
%define SYSCALL_USER_DELEGATE_CAPABILITY 29
%define SYSCALL_USER_REGISTER_SHARED_BUFFER 30
%define SYSCALL_USER_READ_SHARED_BUFFER 31
%define SYSCALL_USER_WRITE_SHARED_BUFFER 32
%define SYSCALL_USER_EXIT 33
%define SYSCALL_USER_CONSOLE_WRITE 34
%define SYSCALL_USER_FS_OPEN 35
%define SYSCALL_USER_FS_LIST 36
%define SYSCALL_USER_FS_READ 37
%define SYSCALL_USER_FS_CREATE 38
%define SYSCALL_USER_FS_WRITE 39
%define SYSCALL_USER_LAUNCH_EXECUTABLE 40
%define SYSCALL_USER_INPUT_READ 41
%define SYSCALL_USER_FS_STAT 42
%define SYSCALL_USER_FS_RENAME 43
%define SYSCALL_USER_FS_DELETE 44
%define SYSCALL_USER_WAIT_PROCESS 45
%define SYSCALL_USER_FS_MOVE 46

%define USER_CAPABILITY_TYPE_ENDPOINT 1
%define USER_CAPABILITY_TYPE_SHARED_BUFFER 2

%define USER_ENDPOINT_ROLE_PEER 1
%define USER_ENDPOINT_ROLE_POLICY 2
%define USER_LAUNCH_ROLE_SESSION_SHELL 1
%define USER_LAUNCH_ROLE_AUTOMATION_WORKER 2
%define USER_LAUNCH_ROLE_LS_UTILITY 3
%define USER_LAUNCH_ROLE_CAT_UTILITY 4
%define USER_LAUNCH_ROLE_MKDIR_UTILITY 5
%define USER_LAUNCH_ROLE_WRITE_UTILITY 6
%define USER_LAUNCH_ROLE_STAT_UTILITY 7
%define USER_LAUNCH_ROLE_RENAME_UTILITY 8
%define USER_LAUNCH_ROLE_APPEND_UTILITY 9
%define USER_LAUNCH_ROLE_DELETE_UTILITY 10
%define USER_LAUNCH_ROLE_MOVE_UTILITY 11
%define USER_LAUNCH_ROLE_ECHO_UTILITY 12
%define USER_LAUNCH_ROLE_ASK_UTILITY 13
%define USER_LAUNCH_ROLE_TOUCH_UTILITY 14
%define USER_LAUNCH_ROLE_COPY_UTILITY 15
%define USERSPACE_EXECUTABLE_LS_UTILITY 3
%define USERSPACE_EXECUTABLE_CAT_UTILITY 4
%define USERSPACE_EXECUTABLE_MKDIR_UTILITY 5
%define USERSPACE_EXECUTABLE_WRITE_UTILITY 6
%define USERSPACE_EXECUTABLE_STAT_UTILITY 7
%define USERSPACE_EXECUTABLE_RENAME_UTILITY 8
%define USERSPACE_EXECUTABLE_APPEND_UTILITY 9
%define USERSPACE_EXECUTABLE_DELETE_UTILITY 10
%define USERSPACE_EXECUTABLE_MOVE_UTILITY 11
%define USERSPACE_EXECUTABLE_ECHO_UTILITY 12
%define USERSPACE_EXECUTABLE_ASK_UTILITY 13
%define USERSPACE_EXECUTABLE_TOUCH_UTILITY 14
%define USERSPACE_EXECUTABLE_COPY_UTILITY 15
%define APP_DESCRIPTOR_FLAG_BUFFER 1
%define APP_DESCRIPTOR_FLAG_BASE 2
%define APP_DESCRIPTOR_FLAG_DEST 4
%define APP_DESCRIPTOR_FLAG_TEXT 8
%define APP_DESCRIPTOR_FLAG_PATH_PAIR 16
%define APP_DESCRIPTOR_MASK_ECHO APP_DESCRIPTOR_FLAG_BUFFER
%define APP_DESCRIPTOR_MASK_PATH (APP_DESCRIPTOR_FLAG_BUFFER + APP_DESCRIPTOR_FLAG_BASE)
%define APP_DESCRIPTOR_MASK_TEXT (APP_DESCRIPTOR_FLAG_BUFFER + APP_DESCRIPTOR_FLAG_BASE + APP_DESCRIPTOR_FLAG_TEXT)
%define APP_DESCRIPTOR_MASK_RENAME (APP_DESCRIPTOR_FLAG_BUFFER + APP_DESCRIPTOR_FLAG_BASE + APP_DESCRIPTOR_FLAG_PATH_PAIR)
%define APP_DESCRIPTOR_MASK_MOVE (APP_DESCRIPTOR_FLAG_BUFFER + APP_DESCRIPTOR_FLAG_BASE + APP_DESCRIPTOR_FLAG_DEST + APP_DESCRIPTOR_FLAG_PATH_PAIR)
%define APP_LAUNCH_POLICY_BUFFER 1
%define APP_LAUNCH_POLICY_PATH 2
%define APP_LAUNCH_POLICY_TEXT 3
%define APP_LAUNCH_POLICY_RENAME 4
%define APP_LAUNCH_POLICY_MOVE 5
%define APP_LAUNCH_FLAG_FOREGROUND 1
%define APP_LAUNCH_FLAG_CONSOLE 2
%define APP_LAUNCH_FLAG_INPUT 4
%define APP_LAUNCH_FLAGS_FOREGROUND_ONLY APP_LAUNCH_FLAG_FOREGROUND
%define APP_LAUNCH_FLAGS_FOREGROUND_CONSOLE (APP_LAUNCH_FLAG_FOREGROUND + APP_LAUNCH_FLAG_CONSOLE)
%define APP_LAUNCH_FLAGS_FOREGROUND_CONSOLE_INPUT (APP_LAUNCH_FLAG_FOREGROUND + APP_LAUNCH_FLAG_CONSOLE + APP_LAUNCH_FLAG_INPUT)
%define USER_SHELL_HISTORY_SLOTS 4
%define USER_SHELL_HISTORY_ENTRY_SIZE 256
%define SERVICE_ENDPOINT_CLASS_INIT 1
%define SERVICE_ENDPOINT_CLASS_AI_POLICY 3
%define SERVICE_ENDPOINT_CLASS_CONSOLE 4
%define SERVICE_ENDPOINT_CLASS_RAMFS 5
%define SERVICE_ENDPOINT_CLASS_INPUT 6
%define RAMFS_NODE_DIRECTORY 1
%define RAMFS_NODE_FILE 2
%define USER_ENDPOINT_CLASS_BOOT_A_PEER 0x00001001
%define USER_ENDPOINT_CLASS_BOOT_A_POLICY 0x00001101
%define USER_ENDPOINT_CLASS_BOOT_B_PEER 0x00001002
%define USER_ENDPOINT_CLASS_BOOT_B_POLICY 0x00001102
%define IPC_MESSAGE_POLICY_APPROVED 0x00000102
%define USER_MESSAGE_CAP_GRANTED 0x00000210
%define USER_MESSAGE_PING 0x00000200
%define USER_MESSAGE_PONG 0x00000201
%define USER_IPC_RX_BUFFER 0
%define USER_IPC_TX_BUFFER 16
%define USER_REVOKE_DONE 32
%define USER_STALE_CAP_SLOT 36
%define USER_POLICY_HANDLE 40
%define USER_POLICY_DELEGATED 44
%define USER_POLICY_RECEIVED 48
%define USER_POLICY_REDELEGATE_TESTED 52
%define USER_NONDELEGATE_TESTED 56
%define USER_POLICY_EXPIRY_TESTED 60
%define USER_POLICY_CLASS_TESTED 64
%define USER_POLICY_REQUEST_INFLIGHT 68
%define USER_POLICY_SERVICE_TESTED 72
%define USER_CAP_ADMISSION_TESTED 76
%define USER_POLICY_BACKPRESSURE_TESTED 80
%define USER_SHARED_BUFFER_LOCAL_CAP 84
%define USER_SHARED_BUFFER_REMOTE_CAP 88
%define USER_SHARED_BUFFER_DELEGATED 92
%define USER_SHARED_BUFFER_TESTED 96
%define USER_SHARED_BUFFER_STALE_TESTED 100
%define USER_LAST_TICK 104
%define USER_CONSOLE_HANDLE 108
%define USER_RAMFS_HANDLE 112
%define USER_SHELL_SCRIPT_HANDLE 116
%define USER_SHELL_SCRIPT_LENGTH 120
%define USER_SHELL_SCRIPT_OFFSET 124
%define USER_SHELL_TEMP_HANDLE 128
%define USER_SHELL_BOOTSTRAP_TESTED 132
%define USER_SHELL_LINE_LENGTH 136
%define USER_SHELL_ARG0_LENGTH 140
%define USER_SHELL_ARG1_OFFSET 144
%define USER_LAUNCH_ROLE 148
%define USER_SHELL_LAUNCH_PID 152
%define USER_SHELL_ENDPOINT_CAP 156
%define USER_INPUT_HANDLE 160
%define USER_CWD_PATH_LENGTH 164
%define USER_CWD_HANDLE 168
%define USER_SHELL_BASE_HANDLE 172
%define USER_SHELL_BASE_TEMP 176
%define USER_SHELL_DEST_HANDLE 180
%define USER_SHELL_DEST_TEMP 184
%define USER_SHELL_DESCRIPTOR_TEMP 188
%define USER_SHELL_LAUNCH_FLAGS 192
%define USER_SHARED_BUFFER_DATA 196
%define USER_SHELL_SCRIPT_DATA 516
%define USER_CWD_PATH_DATA 772
%define USER_SHELL_CURSOR_POS 1028
%define USER_SHELL_INPUT_LENGTH 1032
%define USER_SHELL_HISTORY_COUNT 1036
%define USER_SHELL_HISTORY_ACTIVE 1040
%define USER_SHELL_HISTORY_INDEX 1044
%define USER_SHELL_DRAFT_LENGTH 1048
%define USER_SHELL_HISTORY_LENGTHS 1052
%define USER_SHELL_HISTORY_DATA 1068
%define USER_SHELL_DRAFT_DATA 2092
%define USER_SHELL_DRAFT_SUMMARY 2156
%define USER_SHELL_DRAFT_CATEGORY 2348
%define USER_SHELL_APPS_FILTER_LENGTH 2476
%define USER_SHELL_APPS_CATEGORY_LENGTH 2480
%define USER_SHELL_APPS_FILTER_DATA 2484

section .text

_x86_resume_user_frame:
    mov esp, [esp + 4]
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iretd

_x86_kernel_idle_loop:
.idle:
    hlt
    jmp .idle

_user_bootstrap_service_start:
    sub esp, 2560
    xor eax, eax
    mov [esp + USER_REVOKE_DONE], eax
    mov [esp + USER_STALE_CAP_SLOT], eax
    mov [esp + USER_POLICY_HANDLE], eax
    mov [esp + USER_POLICY_DELEGATED], eax
    mov [esp + USER_POLICY_RECEIVED], eax
    mov [esp + USER_POLICY_REDELEGATE_TESTED], eax
    mov [esp + USER_NONDELEGATE_TESTED], eax
    mov [esp + USER_POLICY_EXPIRY_TESTED], eax
    mov [esp + USER_POLICY_CLASS_TESTED], eax
    mov [esp + USER_POLICY_REQUEST_INFLIGHT], eax
    mov [esp + USER_POLICY_SERVICE_TESTED], eax
    mov [esp + USER_CAP_ADMISSION_TESTED], eax
    mov [esp + USER_POLICY_BACKPRESSURE_TESTED], eax
    mov [esp + USER_SHARED_BUFFER_LOCAL_CAP], eax
    mov [esp + USER_SHARED_BUFFER_REMOTE_CAP], eax
    mov [esp + USER_SHARED_BUFFER_DELEGATED], eax
    mov [esp + USER_SHARED_BUFFER_TESTED], eax
    mov [esp + USER_SHARED_BUFFER_STALE_TESTED], eax
    mov [esp + USER_LAST_TICK], eax
    mov [esp + USER_CONSOLE_HANDLE], eax
    mov [esp + USER_RAMFS_HANDLE], eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_SCRIPT_LENGTH], eax
    mov [esp + USER_SHELL_SCRIPT_OFFSET], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BOOTSTRAP_TESTED], eax
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    mov [esp + USER_SHELL_ARG0_LENGTH], eax
    mov [esp + USER_SHELL_ARG1_OFFSET], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_INPUT_HANDLE], eax
    mov [esp + USER_CWD_PATH_LENGTH], eax
    mov [esp + USER_CWD_HANDLE], eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov [esp + USER_SHELL_DEST_TEMP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_FLAGS], eax
    mov [esp + USER_SHELL_CURSOR_POS], eax
    mov [esp + USER_SHELL_INPUT_LENGTH], eax
    mov [esp + USER_SHELL_HISTORY_COUNT], eax
    mov [esp + USER_SHELL_HISTORY_ACTIVE], eax
    mov [esp + USER_SHELL_HISTORY_INDEX], eax
    mov [esp + USER_SHELL_DRAFT_LENGTH], eax
    mov [esp + USER_SHELL_APPS_FILTER_LENGTH], eax
    mov [esp + USER_SHELL_APPS_CATEGORY_LENGTH], eax
    mov [esp + USER_LAUNCH_ROLE], ecx
    xor ebp, ebp
    mov edi, ebx

    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_LS_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_CAT_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MKDIR_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_WRITE_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_STAT_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_RENAME_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_APPEND_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_DELETE_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ECHO_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_TOUCH_UTILITY
    je .register_utility_peer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    je .register_utility_peer

    cmp edi, 1
    jne .register_boot_b
    mov edx, USER_ENDPOINT_CLASS_BOOT_A_PEER
    jmp .register_peer

.register_boot_b:
    mov edx, USER_ENDPOINT_CLASS_BOOT_B_PEER
    jmp .register_peer

.register_utility_peer:
    xor edx, edx

.register_peer:
    mov eax, SYSCALL_USER_REGISTER_ENDPOINT
    mov ebx, USER_ENDPOINT_ROLE_PEER
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_LS_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_CAT_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MKDIR_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_WRITE_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_STAT_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_RENAME_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_APPEND_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_DELETE_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ECHO_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_TOUCH_UTILITY
    je .register_peer_from_shell
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    je .register_peer_from_shell
    mov ecx, 3
    sub ecx, edi
    jmp .register_peer_issue

.register_peer_from_shell:
    mov ecx, 1

.register_peer_issue:
    xor esi, esi
    int 0x80

    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_LS_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_CAT_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MKDIR_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_WRITE_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_STAT_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_RENAME_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_APPEND_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_DELETE_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ECHO_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_TOUCH_UTILITY
    je .register_utility_policy
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    je .register_utility_policy

    cmp edi, 1
    jne .register_policy_b
    mov edx, USER_ENDPOINT_CLASS_BOOT_A_POLICY
    jmp .register_policy

.register_policy_b:
    mov edx, USER_ENDPOINT_CLASS_BOOT_B_POLICY
    jmp .register_policy

.register_utility_policy:
    xor edx, edx

.register_policy:
    mov eax, SYSCALL_USER_REGISTER_ENDPOINT
    mov ebx, USER_ENDPOINT_ROLE_POLICY
    xor ecx, ecx
    xor esi, esi
    int 0x80

    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_LS_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_CAT_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MKDIR_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_WRITE_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_STAT_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_RENAME_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_APPEND_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_DELETE_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ECHO_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_TOUCH_UTILITY
    je .utility_prepare
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    je .utility_prepare

    cmp edi, 1
    jne .register_worker_buffer
    mov eax, SYSCALL_USER_REGISTER_SHARED_BUFFER
    lea ebx, [esp + USER_SHARED_BUFFER_DATA]
    mov ecx, 256
    mov edx, 1
    int 0x80
    mov [esp + USER_SHARED_BUFFER_LOCAL_CAP], eax
    jmp .loop

.register_worker_buffer:
    cmp edi, 2
    jne .loop
    mov dword [esp + USER_SHARED_BUFFER_DATA], 0xA1700002
    mov dword [esp + USER_SHARED_BUFFER_DATA + 4], 0x42554652
    mov dword [esp + USER_SHARED_BUFFER_DATA + 8], 0x00000010
    mov dword [esp + USER_SHARED_BUFFER_DATA + 12], 0x00000020
    mov eax, SYSCALL_USER_REGISTER_SHARED_BUFFER
    lea ebx, [esp + USER_SHARED_BUFFER_DATA]
    mov ecx, 16
    mov edx, 1
    int 0x80
    mov [esp + USER_SHARED_BUFFER_LOCAL_CAP], eax

.utility_prepare:
    xor eax, eax
    mov [esp + USER_CONSOLE_HANDLE], eax
    mov [esp + USER_CWD_HANDLE], eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_DEST_TEMP], eax
    mov [esp + USER_SHARED_BUFFER_REMOTE_CAP], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_ARG0_LENGTH], eax
    mov [esp + USER_SHELL_ARG1_OFFSET], eax
    mov [esp + USER_SHELL_LINE_LENGTH], eax

.utility_wait_context:
    cmp dword [esp + USER_CONSOLE_HANDLE], 0
    jne .utility_wait_have_console
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_LS_UTILITY
    je .utility_wait_message
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_CAT_UTILITY
    je .utility_wait_message
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_STAT_UTILITY
    je .utility_wait_message
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ECHO_UTILITY
    je .utility_wait_message
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    je .utility_wait_message

.utility_wait_have_console:
    cmp dword [esp + USER_SHARED_BUFFER_REMOTE_CAP], 0
    je .utility_wait_message
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ECHO_UTILITY
    je .utility_read_buffer
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    je .utility_wait_have_input
    cmp dword [esp + USER_CWD_HANDLE], 0
    je .utility_wait_message
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .utility_wait_need_dest
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    jne .utility_read_buffer

.utility_wait_need_dest:
    cmp dword [esp + USER_SHELL_DEST_HANDLE], 0
    jne .utility_read_buffer
    jmp .utility_wait_message

.utility_wait_have_input:
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    jne .utility_read_buffer
    cmp dword [esp + USER_INPUT_HANDLE], 0
    jne .utility_read_buffer
    jmp .utility_wait_message

.utility_wait_message:
    mov eax, SYSCALL_USER_WAIT_IPC
    lea ebx, [esp + USER_IPC_RX_BUFFER]
    mov ecx, 4
    int 0x80
    cmp eax, USER_MESSAGE_CAP_GRANTED
    jne .utility_wait_context
    cmp dword [esp + USER_IPC_RX_BUFFER + 4], USER_CAPABILITY_TYPE_ENDPOINT
    je .utility_store_console
    cmp dword [esp + USER_IPC_RX_BUFFER + 4], USER_CAPABILITY_TYPE_SHARED_BUFFER
    je .utility_store_buffer
    cmp dword [esp + USER_IPC_RX_BUFFER + 4], 3
    je .utility_store_cwd
    jmp .utility_wait_context

.utility_store_console:
    mov eax, [esp + USER_IPC_RX_BUFFER + 8]
    cmp eax, SERVICE_ENDPOINT_CLASS_INPUT
    je .utility_store_input
    mov eax, [esp + USER_IPC_RX_BUFFER]
    mov [esp + USER_CONSOLE_HANDLE], eax
    jmp .utility_wait_context

.utility_store_input:
    mov eax, [esp + USER_IPC_RX_BUFFER]
    mov [esp + USER_INPUT_HANDLE], eax
    jmp .utility_wait_context

.utility_store_buffer:
    mov eax, [esp + USER_IPC_RX_BUFFER]
    mov [esp + USER_SHARED_BUFFER_REMOTE_CAP], eax
    jmp .utility_wait_context

.utility_store_cwd:
    mov eax, [esp + USER_IPC_RX_BUFFER]
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .utility_store_maybe_secondary_base
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    jne .utility_store_primary_base

.utility_store_maybe_secondary_base:
    cmp dword [esp + USER_CWD_HANDLE], 0
    jne .utility_store_secondary_base

.utility_store_primary_base:
    mov [esp + USER_CWD_HANDLE], eax
    jmp .utility_wait_context

.utility_store_secondary_base:
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    jmp .utility_wait_context

.utility_read_buffer:
    mov eax, SYSCALL_USER_READ_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    mov esi, 256
    int 0x80
    test eax, eax
    jne .utility_exit

    xor ecx, ecx
.utility_measure_path:
    cmp ecx, 256
    jae .utility_have_path
    cmp byte [esp + USER_SHARED_BUFFER_DATA + ecx], 0
    je .utility_have_path
    inc ecx
    jmp .utility_measure_path

.utility_have_path:
    cmp ecx, 0
    je .utility_exit
    mov [esp + USER_SHELL_ARG0_LENGTH], ecx

    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_WRITE_UTILITY
    je .utility_prepare_second_arg
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_RENAME_UTILITY
    je .utility_prepare_second_arg
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .utility_prepare_second_arg
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    je .utility_prepare_second_arg
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_APPEND_UTILITY
    jne .utility_dispatch

.utility_prepare_second_arg:
    mov eax, ecx
    inc eax
    cmp eax, 256
    jae .utility_exit
    mov [esp + USER_SHELL_ARG1_OFFSET], eax
    xor esi, esi

.utility_measure_write_text:
    mov ebx, eax
    add ebx, esi
    cmp ebx, 256
    jae .utility_have_second_arg
    cmp byte [esp + USER_SHARED_BUFFER_DATA + ebx], 0
    je .utility_have_second_arg
    inc esi
    jmp .utility_measure_write_text

.utility_have_second_arg:
    mov [esp + USER_SHELL_LINE_LENGTH], esi

.utility_dispatch:

    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_LS_UTILITY
    je .utility_run_ls
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_CAT_UTILITY
    je .utility_run_cat
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MKDIR_UTILITY
    je .utility_run_mkdir
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_WRITE_UTILITY
    je .utility_run_write
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_STAT_UTILITY
    je .utility_run_stat
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_RENAME_UTILITY
    je .utility_run_rename
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_APPEND_UTILITY
    je .utility_run_append
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_DELETE_UTILITY
    je .utility_run_delete
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_MOVE_UTILITY
    je .utility_run_move
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ECHO_UTILITY
    je .utility_run_echo
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_ASK_UTILITY
    je .utility_run_ask
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_TOUCH_UTILITY
    je .utility_run_touch
    cmp dword [esp + USER_LAUNCH_ROLE], USER_LAUNCH_ROLE_COPY_UTILITY
    je .utility_run_copy
    jmp .utility_exit

.utility_run_ls:
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .utility_exit
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_LIST
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, 256
    int 0x80
    test eax, eax
    js .utility_revoke_node
    mov esi, eax
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    int 0x80
    jmp .utility_revoke_node

.utility_run_cat:
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .utility_exit
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    mov esi, 128
    int 0x80
    test eax, eax
    js .utility_revoke_node
    mov esi, eax
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    int 0x80
    jmp .utility_revoke_node

.utility_run_mkdir:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

.utility_mkdir_next_segment:
    mov eax, [esp + USER_SHELL_BASE_TEMP]

.utility_mkdir_skip_slashes:
    cmp eax, [esp + USER_SHELL_ARG0_LENGTH]
    jae .utility_exit
    mov bl, [esp + USER_SHARED_BUFFER_DATA + eax]
    cmp bl, '/'
    jne .utility_mkdir_segment_start
    inc eax
    jmp .utility_mkdir_skip_slashes

.utility_mkdir_segment_start:
    mov [esp + USER_SHELL_DEST_TEMP], eax
    mov ecx, eax

.utility_mkdir_measure_segment:
    cmp ecx, [esp + USER_SHELL_ARG0_LENGTH]
    jae .utility_mkdir_segment_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + ecx]
    cmp bl, '/'
    je .utility_mkdir_segment_ready
    inc ecx
    jmp .utility_mkdir_measure_segment

.utility_mkdir_segment_ready:
    mov [esp + USER_SHELL_BASE_TEMP], ecx
    mov edx, ecx
    sub edx, [esp + USER_SHELL_DEST_TEMP]
    cmp edx, 0
    je .utility_mkdir_next_segment
    cmp edx, 1
    jne .utility_mkdir_write_segment
    mov eax, [esp + USER_SHELL_DEST_TEMP]
    cmp byte [esp + USER_SHARED_BUFFER_DATA + eax], '.'
    je .utility_mkdir_next_segment

.utility_mkdir_write_segment:
    mov eax, SYSCALL_USER_WRITE_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    add edx, [esp + USER_SHELL_DEST_TEMP]
    mov esi, [esp + USER_SHELL_BASE_TEMP]
    sub esi, [esp + USER_SHELL_DEST_TEMP]
    int 0x80
    test eax, eax
    jne .utility_exit

    mov eax, SYSCALL_USER_FS_CREATE
    mov ebx, [esp + USER_SHELL_DEST_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_BASE_TEMP]
    sub edx, [esp + USER_SHELL_DEST_TEMP]
    mov esi, RAMFS_NODE_DIRECTORY
    int 0x80
    test eax, eax
    js .utility_exit
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov ebx, [esp + USER_SHELL_DEST_HANDLE]
    cmp ebx, [esp + USER_CWD_HANDLE]
    je .utility_mkdir_store_handle
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    int 0x80

.utility_mkdir_store_handle:
    mov eax, [esp + USER_SHELL_TEMP_HANDLE]
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    jmp .utility_mkdir_next_segment

.utility_run_stat:
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .utility_exit
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_STAT
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, 64
    int 0x80
    test eax, eax
    js .utility_revoke_node
    mov esi, eax
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    int 0x80
    jmp .utility_revoke_node

.utility_run_rename:
    mov eax, SYSCALL_USER_FS_RENAME
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    mov esi, [esp + USER_SHELL_LINE_LENGTH]
    int 0x80
    test eax, eax
    js .utility_exit
    jmp .utility_exit

.utility_run_move:
    mov eax, [esp + USER_SHELL_LINE_LENGTH]
    shl eax, 16
    mov ebx, [esp + USER_SHELL_ARG0_LENGTH]
    and ebx, 0xFFFF
    or eax, ebx
    mov esi, eax
    mov eax, SYSCALL_USER_FS_MOVE
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHELL_DEST_HANDLE]
    mov edx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    int 0x80
    test eax, eax
    js .utility_exit
    jmp .utility_exit

.utility_run_append:
    mov eax, SYSCALL_USER_FS_CREATE
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    mov esi, RAMFS_NODE_FILE
    int 0x80
    test eax, eax
    js .utility_exit
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_STAT
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, 64
    int 0x80
    test eax, eax
    js .utility_revoke_node

    mov eax, SYSCALL_USER_READ_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHELL_SCRIPT_DATA]
    mov esi, 64
    int 0x80
    test eax, eax
    jne .utility_revoke_node

    xor edx, edx
    mov esi, 15

.utility_parse_append_size:
    cmp esi, 64
    jae .utility_append_size_ready
    mov al, [esp + USER_SHELL_SCRIPT_DATA + esi]
    cmp al, '0'
    jb .utility_append_size_ready
    cmp al, '9'
    ja .utility_append_size_ready
    imul edx, edx, 10
    sub al, '0'
    movzx eax, al
    add edx, eax
    inc esi
    jmp .utility_parse_append_size

.utility_append_size_ready:
    mov [esp + USER_SHELL_ARG0_LENGTH], edx
    mov eax, SYSCALL_USER_WRITE_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    add edx, [esp + USER_SHELL_ARG1_OFFSET]
    mov esi, [esp + USER_SHELL_LINE_LENGTH]
    int 0x80
    test eax, eax
    jne .utility_revoke_node

    mov eax, SYSCALL_USER_FS_WRITE
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    mov esi, [esp + USER_SHELL_LINE_LENGTH]
    int 0x80
    test eax, eax
    js .utility_revoke_node
    jmp .utility_revoke_node

.utility_run_delete:
    mov eax, SYSCALL_USER_FS_DELETE
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .utility_exit
    jmp .utility_exit

.utility_run_touch:
    mov eax, SYSCALL_USER_FS_CREATE
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    mov esi, RAMFS_NODE_FILE
    int 0x80
    test eax, eax
    js .utility_exit
    jmp .utility_exit

.utility_run_copy:
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .utility_exit
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    mov esi, 256
    int 0x80
    test eax, eax
    js .utility_revoke_node
    mov [esp + USER_SHELL_ARG0_LENGTH], eax
    cmp eax, 0
    je .utility_copy_revoke_source

    mov eax, SYSCALL_USER_READ_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHELL_SCRIPT_DATA]
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    jne .utility_revoke_node

.utility_copy_revoke_source:
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_WRITE_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    add edx, [esp + USER_SHELL_ARG1_OFFSET]
    mov esi, [esp + USER_SHELL_LINE_LENGTH]
    int 0x80
    test eax, eax
    jne .utility_exit

    mov eax, SYSCALL_USER_FS_CREATE
    mov ebx, [esp + USER_SHELL_DEST_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, RAMFS_NODE_FILE
    int 0x80
    test eax, eax
    js .utility_exit
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    cmp dword [esp + USER_SHELL_ARG0_LENGTH], 0
    je .utility_revoke_node

    mov eax, SYSCALL_USER_WRITE_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHELL_SCRIPT_DATA]
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    jne .utility_revoke_node

    mov eax, SYSCALL_USER_FS_WRITE
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .utility_revoke_node
    jmp .utility_revoke_node

.utility_run_echo:
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    jmp .utility_exit

.utility_run_ask:
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80

    mov eax, SYSCALL_USER_INPUT_READ
    mov ebx, [esp + USER_INPUT_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, 128
    int 0x80
    test eax, eax
    jz .utility_exit
    mov esi, eax

    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    int 0x80
    jmp .utility_exit

.utility_run_write:
    mov eax, SYSCALL_USER_FS_CREATE
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    mov esi, RAMFS_NODE_FILE
    int 0x80
    test eax, eax
    jns .utility_write_have_file

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_CWD_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .utility_exit

.utility_write_have_file:
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov eax, SYSCALL_USER_WRITE_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    add edx, [esp + USER_SHELL_ARG1_OFFSET]
    mov esi, [esp + USER_SHELL_LINE_LENGTH]
    int 0x80
    test eax, eax
    jne .utility_revoke_node
    mov eax, SYSCALL_USER_FS_WRITE
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor edx, edx
    mov esi, [esp + USER_SHELL_LINE_LENGTH]
    int 0x80
    test eax, eax
    js .utility_revoke_node

.utility_revoke_node:
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.utility_exit:
    cmp dword [esp + USER_SHARED_BUFFER_REMOTE_CAP], 0
    je .utility_exit_cwd
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHARED_BUFFER_REMOTE_CAP], eax

.utility_exit_cwd:
    cmp dword [esp + USER_CWD_HANDLE], 0
    je .utility_exit_dest
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_CWD_HANDLE]
    int 0x80

.utility_exit_dest:
    cmp dword [esp + USER_SHELL_DEST_HANDLE], 0
    je .utility_exit_clear_cwd
    mov eax, [esp + USER_SHELL_DEST_HANDLE]
    cmp eax, [esp + USER_CWD_HANDLE]
    je .utility_exit_dest_clear
    mov ebx, eax
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    int 0x80

.utility_exit_dest_clear:
    xor eax, eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax

.utility_exit_clear_cwd:
    xor eax, eax
    mov [esp + USER_CWD_HANDLE], eax

.utility_exit_input:
    cmp dword [esp + USER_INPUT_HANDLE], 0
    je .utility_exit_console
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_INPUT_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_INPUT_HANDLE], eax

.utility_exit_console:
    cmp dword [esp + USER_CONSOLE_HANDLE], 0
    je .utility_exit_now
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_CONSOLE_HANDLE], eax

.utility_exit_now:
    mov eax, SYSCALL_USER_EXIT
    xor ebx, ebx
    int 0x80

.loop:
    mov eax, SYSCALL_GET_UPTIME_TICKS
    int 0x80
    mov [esp + USER_LAST_TICK], eax

    mov eax, SYSCALL_GET_DENIED_IPC_COUNT
    int 0x80

    mov eax, SYSCALL_GET_SERVICE_COUNT
    int 0x80

    cmp edi, 1
    jne .delay
    cmp dword [esp + USER_SHARED_BUFFER_LOCAL_CAP], 0
    je .delay
    cmp dword [esp + USER_SHELL_BOOTSTRAP_TESTED], 0
    je .shell_session_setup
    jmp .shell_session_poll

.shell_session_setup:
    mov eax, SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT
    mov ebx, SERVICE_ENDPOINT_CLASS_CONSOLE
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .delay
    mov [esp + USER_CONSOLE_HANDLE], eax

    mov eax, SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT
    mov ebx, SERVICE_ENDPOINT_CLASS_RAMFS
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .delay
    mov [esp + USER_RAMFS_HANDLE], eax

    mov eax, SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT
    mov ebx, SERVICE_ENDPOINT_CLASS_INPUT
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .delay
    mov [esp + USER_INPUT_HANDLE], eax

    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .delay
    mov [esp + USER_CWD_HANDLE], eax
    mov dword [esp + USER_CWD_PATH_LENGTH], 1
    mov byte [esp + USER_CWD_PATH_DATA], '/'
    mov byte [esp + USER_CWD_PATH_DATA + 1], 0
    mov dword [esp + USER_SHELL_BOOTSTRAP_TESTED], 1

.shell_session_poll:
    cmp dword [esp + USER_INPUT_HANDLE], 0
    je .delay

.shell_load_input:
    mov eax, SYSCALL_USER_INPUT_READ
    mov ebx, [esp + USER_INPUT_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 255
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .delay
    mov [esp + USER_SHELL_INPUT_LENGTH], eax
    xor ecx, ecx
    xor esi, esi
    mov [esp + USER_SHELL_SCRIPT_OFFSET], ecx
    mov [esp + USER_SHELL_CURSOR_POS], ecx
    mov [esp + USER_SHELL_HISTORY_ACTIVE], ecx
    mov [esp + USER_SHELL_HISTORY_INDEX], ecx
    mov [esp + USER_SHELL_DRAFT_LENGTH], ecx

.shell_copy_script:
    cmp ecx, [esp + USER_SHELL_INPUT_LENGTH]
    jae .shell_copy_script_done
    mov bl, [esp + USER_SHARED_BUFFER_DATA + ecx]
    cmp bl, 10
    je .shell_copy_script_done
    cmp bl, 8
    je .shell_edit_backspace
    cmp bl, 9
    je .shell_edit_tab
    cmp bl, 13
    je .shell_edit_skip
    cmp bl, 27
    je .shell_edit_escape
    jmp .shell_edit_insert_input

.shell_edit_backspace:
    mov eax, [esp + USER_SHELL_CURSOR_POS]
    cmp eax, 0
    je .shell_edit_advance
    dec eax
    mov [esp + USER_SHELL_CURSOR_POS], eax
    mov edx, eax

.shell_edit_backspace_shift:
    mov eax, edx
    inc eax
    cmp eax, esi
    jae .shell_edit_backspace_done
    mov bl, [esp + USER_SHELL_SCRIPT_DATA + edx + 1]
    mov [esp + USER_SHELL_SCRIPT_DATA + edx], bl
    inc edx
    jmp .shell_edit_backspace_shift

.shell_edit_backspace_done:
    dec esi
    jmp .shell_edit_advance

.shell_edit_tab:
    mov byte [esp + USER_SHARED_BUFFER_DATA + ecx], ' '
    jmp .shell_edit_insert_input

.shell_edit_skip:
    jmp .shell_edit_advance

.shell_edit_escape:
    mov eax, ecx
    add eax, 2
    cmp eax, [esp + USER_SHELL_INPUT_LENGTH]
    jae .shell_edit_advance
    cmp byte [esp + USER_SHARED_BUFFER_DATA + ecx + 1], '['
    jne .shell_edit_advance
    mov bl, [esp + USER_SHARED_BUFFER_DATA + ecx + 2]
    cmp bl, 'A'
    je .shell_edit_history_up
    cmp bl, 'B'
    je .shell_edit_history_down
    cmp bl, 'C'
    je .shell_edit_cursor_right
    cmp bl, 'D'
    je .shell_edit_cursor_left
    cmp bl, 'H'
    je .shell_edit_cursor_home
    cmp bl, 'F'
    je .shell_edit_cursor_end
    cmp bl, '3'
    jne .shell_edit_escape_csi_done
    mov eax, ecx
    add eax, 3
    cmp eax, [esp + USER_SHELL_INPUT_LENGTH]
    jae .shell_edit_escape_csi_done
    cmp byte [esp + USER_SHARED_BUFFER_DATA + ecx + 3], '~'
    je .shell_edit_delete

.shell_edit_escape_csi_done:
    add ecx, 3
    jmp .shell_copy_script

.shell_edit_history_up:
    cmp dword [esp + USER_SHELL_HISTORY_COUNT], 0
    je .shell_edit_escape_csi_done
    cmp dword [esp + USER_SHELL_HISTORY_ACTIVE], 0
    jne .shell_edit_history_up_advance
    mov [esp + USER_SHELL_DRAFT_LENGTH], esi
    xor edx, edx

.shell_edit_history_save_draft:
    cmp edx, esi
    jae .shell_edit_history_save_draft_done
    mov bl, [esp + USER_SHELL_SCRIPT_DATA + edx]
    mov [esp + USER_SHELL_DRAFT_DATA + edx], bl
    inc edx
    jmp .shell_edit_history_save_draft

.shell_edit_history_save_draft_done:
    mov dword [esp + USER_SHELL_HISTORY_ACTIVE], 1
    xor eax, eax
    mov [esp + USER_SHELL_HISTORY_INDEX], eax

.shell_edit_history_up_copy:
    jmp .shell_edit_history_copy_selected

.shell_edit_history_up_advance:
    mov eax, [esp + USER_SHELL_HISTORY_INDEX]
    inc eax
    cmp eax, [esp + USER_SHELL_HISTORY_COUNT]
    jae .shell_edit_history_up_copy
    mov [esp + USER_SHELL_HISTORY_INDEX], eax
    jmp .shell_edit_history_up_copy

.shell_edit_history_copy_selected:
    mov eax, [esp + USER_SHELL_HISTORY_INDEX]
    mov ebx, eax
    shl ebx, 2
    mov esi, [esp + USER_SHELL_HISTORY_LENGTHS + ebx]
    mov ebx, eax
    shl ebx, 8
    lea eax, [esp + USER_SHELL_HISTORY_DATA]
    add eax, ebx
    xor edx, edx

.shell_edit_history_copy_line:
    cmp edx, esi
    jae .shell_edit_history_up_done
    mov bl, [eax + edx]
    mov [esp + USER_SHELL_SCRIPT_DATA + edx], bl
    inc edx
    jmp .shell_edit_history_copy_line

.shell_edit_history_up_done:
    mov [esp + USER_SHELL_CURSOR_POS], esi
    add ecx, 3
    jmp .shell_copy_script

.shell_edit_history_down:
    cmp dword [esp + USER_SHELL_HISTORY_ACTIVE], 0
    je .shell_edit_escape_csi_done
    mov eax, [esp + USER_SHELL_HISTORY_INDEX]
    cmp eax, 0
    jne .shell_edit_history_down_older
    mov esi, [esp + USER_SHELL_DRAFT_LENGTH]
    xor edx, edx

.shell_edit_history_restore_draft:
    cmp edx, esi
    jae .shell_edit_history_down_done
    mov bl, [esp + USER_SHELL_DRAFT_DATA + edx]
    mov [esp + USER_SHELL_SCRIPT_DATA + edx], bl
    inc edx
    jmp .shell_edit_history_restore_draft

.shell_edit_history_down_done:
    xor eax, eax
    mov [esp + USER_SHELL_HISTORY_ACTIVE], eax
    mov [esp + USER_SHELL_HISTORY_INDEX], eax
    mov [esp + USER_SHELL_CURSOR_POS], esi
    add ecx, 3
    jmp .shell_copy_script

.shell_edit_history_down_older:
    dec eax
    mov [esp + USER_SHELL_HISTORY_INDEX], eax
    jmp .shell_edit_history_copy_selected

.shell_edit_cursor_right:
    mov eax, [esp + USER_SHELL_CURSOR_POS]
    cmp eax, esi
    jae .shell_edit_escape_csi_done
    inc eax
    mov [esp + USER_SHELL_CURSOR_POS], eax
    add ecx, 3
    jmp .shell_copy_script

.shell_edit_cursor_left:
    mov eax, [esp + USER_SHELL_CURSOR_POS]
    cmp eax, 0
    je .shell_edit_escape_csi_done
    dec eax
    mov [esp + USER_SHELL_CURSOR_POS], eax
    add ecx, 3
    jmp .shell_copy_script

.shell_edit_cursor_home:
    xor eax, eax
    mov [esp + USER_SHELL_CURSOR_POS], eax
    add ecx, 3
    jmp .shell_copy_script

.shell_edit_cursor_end:
    mov [esp + USER_SHELL_CURSOR_POS], esi
    add ecx, 3
    jmp .shell_copy_script

.shell_edit_delete:
    mov eax, [esp + USER_SHELL_CURSOR_POS]
    cmp eax, esi
    jae .shell_edit_delete_done
    mov edx, eax

.shell_edit_delete_shift:
    mov eax, edx
    inc eax
    cmp eax, esi
    jae .shell_edit_delete_done_shift
    mov bl, [esp + USER_SHELL_SCRIPT_DATA + edx + 1]
    mov [esp + USER_SHELL_SCRIPT_DATA + edx], bl
    inc edx
    jmp .shell_edit_delete_shift

.shell_edit_delete_done_shift:
    dec esi

.shell_edit_delete_done:
    add ecx, 4
    jmp .shell_copy_script

.shell_edit_insert_input:
    cmp esi, 255
    jae .shell_edit_advance
    mov eax, [esp + USER_SHELL_CURSOR_POS]
    cmp eax, esi
    jae .shell_edit_append_input
    mov edx, esi

.shell_edit_insert_shift:
    cmp edx, eax
    je .shell_edit_insert_store
    mov bl, [esp + USER_SHELL_SCRIPT_DATA + edx - 1]
    mov [esp + USER_SHELL_SCRIPT_DATA + edx], bl
    dec edx
    jmp .shell_edit_insert_shift

.shell_edit_insert_store:
    mov bl, [esp + USER_SHARED_BUFFER_DATA + ecx]
    mov [esp + USER_SHELL_SCRIPT_DATA + eax], bl
    inc esi
    inc eax
    mov [esp + USER_SHELL_CURSOR_POS], eax
    jmp .shell_edit_advance

.shell_edit_append_input:
    mov bl, [esp + USER_SHARED_BUFFER_DATA + ecx]
    mov [esp + USER_SHELL_SCRIPT_DATA + esi], bl
    inc esi
    mov [esp + USER_SHELL_CURSOR_POS], esi
    jmp .shell_edit_advance

.shell_edit_advance:
    inc ecx
    jmp .shell_copy_script

.shell_copy_script_done:
    mov [esp + USER_SHELL_SCRIPT_LENGTH], esi
    mov byte [esp + USER_SHELL_SCRIPT_DATA + esi], 0
    cmp esi, 0
    je .shell_copy_script_state_reset
    cmp dword [esp + USER_SHELL_HISTORY_COUNT], 0
    je .shell_copy_script_check_history_count
    cmp esi, [esp + USER_SHELL_HISTORY_LENGTHS]
    jne .shell_copy_script_check_history_count
    xor edx, edx

.shell_copy_script_compare_latest:
    cmp edx, esi
    jae .shell_copy_script_state_reset
    mov al, [esp + USER_SHELL_SCRIPT_DATA + edx]
    cmp al, [esp + USER_SHELL_HISTORY_DATA + edx]
    jne .shell_copy_script_check_history_count
    inc edx
    jmp .shell_copy_script_compare_latest

.shell_copy_script_check_history_count:
    mov eax, [esp + USER_SHELL_HISTORY_COUNT]
    cmp eax, 0
    je .shell_copy_script_store_history
    cmp eax, USER_SHELL_HISTORY_SLOTS
    jb .shell_copy_script_history_prepare
    mov eax, USER_SHELL_HISTORY_SLOTS - 2
    jmp .shell_copy_script_shift_history

.shell_copy_script_history_prepare:
    dec eax

.shell_copy_script_shift_history:
    mov ebx, eax
    inc ebx
    mov edx, eax
    shl edx, 2
    mov ecx, [esp + USER_SHELL_HISTORY_LENGTHS + edx]
    mov edx, ebx
    shl edx, 2
    mov [esp + USER_SHELL_HISTORY_LENGTHS + edx], ecx
    mov edx, eax
    shl edx, 8
    lea ecx, [esp + USER_SHELL_HISTORY_DATA]
    add ecx, edx
    mov edx, ebx
    shl edx, 8
    lea ebp, [esp + USER_SHELL_HISTORY_DATA]
    add ebp, edx
    xor edx, edx

.shell_copy_script_shift_history_bytes:
    cmp edx, USER_SHELL_HISTORY_ENTRY_SIZE
    jae .shell_copy_script_shift_history_done
    mov bl, [ecx + edx]
    mov [ebp + edx], bl
    inc edx
    jmp .shell_copy_script_shift_history_bytes

.shell_copy_script_shift_history_done:
    cmp eax, 0
    je .shell_copy_script_store_history
    dec eax
    jmp .shell_copy_script_shift_history

.shell_copy_script_store_history:
    mov [esp + USER_SHELL_HISTORY_LENGTHS], esi
    xor edx, edx

.shell_copy_script_store_history_bytes:
    cmp edx, esi
    jae .shell_copy_script_store_history_done
    mov bl, [esp + USER_SHELL_SCRIPT_DATA + edx]
    mov [esp + USER_SHELL_HISTORY_DATA + edx], bl
    inc edx
    jmp .shell_copy_script_store_history_bytes

.shell_copy_script_store_history_done:
    mov eax, [esp + USER_SHELL_HISTORY_COUNT]
    cmp eax, USER_SHELL_HISTORY_SLOTS
    jae .shell_copy_script_state_reset
    inc eax
    mov [esp + USER_SHELL_HISTORY_COUNT], eax

.shell_copy_script_state_reset:
    xor eax, eax
    mov [esp + USER_SHELL_HISTORY_ACTIVE], eax
    mov [esp + USER_SHELL_HISTORY_INDEX], eax
    mov [esp + USER_SHELL_DRAFT_LENGTH], eax
    jmp .shell_next_line

.shell_next_line:
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    cmp esi, [esp + USER_SHELL_SCRIPT_LENGTH]
    jae .shell_load_input

    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    xor ecx, ecx

.shell_measure_line:
    mov eax, esi
    add eax, ecx
    cmp eax, [esp + USER_SHELL_SCRIPT_LENGTH]
    jae .shell_line_ready
    mov al, [edx + ecx]
    cmp al, 10
    je .shell_line_ready
    inc ecx
    jmp .shell_measure_line

.shell_line_ready:
    cmp ecx, 0
    je .shell_advance_line

    mov byte [esp + USER_SHARED_BUFFER_DATA], '$'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], ' '
    xor eax, eax

.shell_copy_prompt_line:
    cmp eax, ecx
    jae .shell_prompt_ready
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + eax + 2], bl
    inc eax
    jmp .shell_copy_prompt_line

.shell_prompt_ready:
    mov byte [esp + USER_SHARED_BUFFER_DATA + ecx + 2], 10
    mov [esp + USER_SHELL_LINE_LENGTH], ecx
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov esi, ecx
    add esi, 3
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    int 0x80
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

    cmp ecx, 4
    jb .shell_check_apps
    cmp byte [edx], 'h'
    jne .shell_check_apps
    cmp byte [edx + 1], 'e'
    jne .shell_check_apps
    cmp byte [edx + 2], 'l'
    jne .shell_check_apps
    cmp byte [edx + 3], 'p'
    jne .shell_check_apps

    cmp ecx, 4
    je .shell_write_help_list
    cmp byte [edx + 4], ' '
    jne .shell_check_apps
    mov eax, 5

.shell_help_skip_spaces:
    cmp eax, ecx
    jae .shell_write_help_list
    cmp byte [edx + eax], ' '
    jne .shell_help_arg_ready
    inc eax
    jmp .shell_help_skip_spaces

.shell_help_arg_ready:
    mov [esp + USER_SHELL_ARG1_OFFSET], eax
    mov ebx, eax

.shell_help_measure_arg:
    cmp ebx, ecx
    jae .shell_help_measure_done
    cmp byte [edx + ebx], ' '
    je .shell_help_measure_done
    inc ebx
    jmp .shell_help_measure_arg

.shell_help_measure_done:
    sub ebx, eax
    cmp ebx, 0
    je .shell_write_help_list
    mov [esp + USER_SHELL_ARG0_LENGTH], ebx
    cmp ebx, 4
    jne .shell_help_check_pwd_detail
    mov esi, eax
    cmp byte [edx + esi], 'h'
    jne .shell_help_check_pwd_detail
    cmp byte [edx + esi + 1], 'e'
    jne .shell_help_check_pwd_detail
    cmp byte [edx + esi + 2], 'l'
    jne .shell_help_check_pwd_detail
    cmp byte [edx + esi + 3], 'p'
    je .shell_write_help_list

.shell_help_check_pwd_detail:
    cmp ebx, 3
    jne .shell_help_check_cd_detail
    mov esi, eax
    cmp byte [edx + esi], 'p'
    jne .shell_help_check_cd_detail
    cmp byte [edx + esi + 1], 'w'
    jne .shell_help_check_cd_detail
    cmp byte [edx + esi + 2], 'd'
    jne .shell_help_check_cd_detail
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'p'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'w'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '-'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'p'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'u'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 16], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 17], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 18], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 19], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 20], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 21], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 22], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 23], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 24], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 25], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 26], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 27], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 28], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 29], 10
    mov esi, 30
    jmp .shell_help_console

.shell_help_check_cd_detail:
    cmp ebx, 2
    jne .shell_help_check_history_detail
    mov esi, eax
    cmp byte [edx + esi], 'c'
    jne .shell_help_check_history_detail
    cmp byte [edx + esi + 1], 'd'
    jne .shell_help_check_history_detail
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], '<'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], 'p'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], '>'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], '-'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 16], 'g'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 17], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 18], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 19], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 20], 'u'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 21], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 22], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 23], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 24], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 25], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 26], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 27], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 28], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 29], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 30], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 31], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 32], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 33], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 34], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 35], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 36], 10
    mov esi, 37
    jmp .shell_help_console

.shell_help_check_history_detail:
    cmp ebx, 7
    jne .shell_help_check_apps_detail
    mov esi, eax
    cmp byte [edx + esi], 'h'
    jne .shell_help_check_apps_detail
    cmp byte [edx + esi + 1], 'i'
    jne .shell_help_check_apps_detail
    cmp byte [edx + esi + 2], 's'
    jne .shell_help_check_apps_detail
    cmp byte [edx + esi + 3], 't'
    jne .shell_help_check_apps_detail
    cmp byte [edx + esi + 4], 'o'
    jne .shell_help_check_apps_detail
    cmp byte [edx + esi + 5], 'r'
    jne .shell_help_check_apps_detail
    cmp byte [edx + esi + 6], 'y'
    jne .shell_help_check_apps_detail
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], '-'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'w'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 16], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 17], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 18], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 19], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 20], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 21], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 22], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 23], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 24], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 25], 'l'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 26], 'l'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 27], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 28], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 29], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 30], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 31], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 32], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 33], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 34], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 35], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 36], 10
    mov esi, 37
    jmp .shell_help_console

.shell_help_check_apps_detail:
    cmp ebx, 4
    jne .shell_help_check_info_detail
    mov esi, eax
    cmp byte [edx + esi], 'a'
    jne .shell_help_check_info_detail
    cmp byte [edx + esi + 1], 'p'
    jne .shell_help_check_info_detail
    cmp byte [edx + esi + 2], 'p'
    jne .shell_help_check_info_detail
    cmp byte [edx + esi + 3], 's'
    jne .shell_help_check_info_detail
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'p'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'p'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], '['
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'g'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], ']'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 16], '-'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 17], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 18], 'b'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 19], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 20], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 21], 'w'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 22], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 23], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 24], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 25], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 26], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 27], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 28], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 29], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 30], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 31], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 32], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 33], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 34], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 35], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 36], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 37], 'p'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 38], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 39], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 40], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 41], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 42], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 43], 'b'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 44], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 45], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 46], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 47], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 48], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 49], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 50], 'g'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 51], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 52], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 53], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 54], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 55], 'w'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 56], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 57], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 58], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 59], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 60], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 61], 'u'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 62], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 63], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 64], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 65], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 66], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 67], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 68], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 69], 10
    mov esi, 70
    jmp .shell_help_console

.shell_help_check_info_detail:
    cmp ebx, 4
    jne .shell_write_help_detail
    mov esi, eax
    cmp byte [edx + esi], 'i'
    jne .shell_write_help_detail
    cmp byte [edx + esi + 1], 'n'
    jne .shell_write_help_detail
    cmp byte [edx + esi + 2], 'f'
    jne .shell_write_help_detail
    cmp byte [edx + esi + 3], 'o'
    jne .shell_write_help_detail
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'f'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], '<'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], '>'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], '-'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 16], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 17], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 18], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 19], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 20], 'w'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 21], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 22], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 23], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 24], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 25], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 26], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 27], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 28], 'l'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 29], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 30], 'u'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 31], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 32], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 33], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 34], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 35], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 36], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 37], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 38], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 39], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 40], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 41], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 42], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 43], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 44], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 45], 10
    mov esi, 46
    jmp .shell_help_console

.shell_write_help_detail:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_LINE_LENGTH], eax

    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_unknown
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    cmp eax, [esp + USER_CWD_HANDLE]
    je .shell_help_detail_have_root
    mov dword [esp + USER_SHELL_BASE_TEMP], 1

.shell_help_detail_have_root:
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov esi, 5
    xor eax, eax

.shell_help_copy_descriptor_name:
    cmp eax, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_help_descriptor_suffix
    mov ebx, [esp + USER_SHELL_ARG1_OFFSET]
    add ebx, eax
    mov bl, [edx + ebx]
    cmp bl, 'a'
    jb .shell_help_store_descriptor_name
    cmp bl, 'z'
    ja .shell_help_store_descriptor_name
    sub bl, 32

.shell_help_store_descriptor_name:
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_help_copy_descriptor_name

.shell_help_descriptor_suffix:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 1], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 3], 'P'
    add esi, 4
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, esi
    int 0x80
    test eax, eax
    js .shell_help_detail_cleanup_unknown
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 160
    int 0x80
    test eax, eax
    js .shell_help_detail_cleanup_unknown
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    xor esi, esi
    xor ebx, ebx

.shell_help_seek_summary:
    cmp esi, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_help_detail_cleanup_unknown
    mov al, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp al, 10
    jne .shell_help_seek_summary_next
    inc ebx
    cmp ebx, 4
    je .shell_help_summary_ready

.shell_help_seek_summary_next:
    inc esi
    jmp .shell_help_seek_summary

.shell_help_summary_ready:
    inc esi
    xor ecx, ecx

.shell_help_copy_summary:
    mov eax, esi
    add eax, ecx
    cmp eax, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_help_summary_done
    mov al, [esp + USER_SHARED_BUFFER_DATA + eax]
    cmp al, 10
    je .shell_help_summary_done
    mov [esp + USER_SHARED_BUFFER_DATA + ecx], al
    inc ecx
    jmp .shell_help_copy_summary

.shell_help_summary_done:
    cmp ecx, 0
    je .shell_help_detail_cleanup_unknown
    mov byte [esp + USER_SHARED_BUFFER_DATA + ecx], 10
    mov esi, ecx
    inc esi
    jmp .shell_help_detail_cleanup_emit

.shell_write_help_list:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_LINE_LENGTH], eax

    mov byte [esp + USER_SHELL_SCRIPT_DATA], 'h'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 1], 'e'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 2], 'l'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 3], 'p'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 4], ' '
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 5], 'p'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 6], 'w'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 7], 'd'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 8], ' '
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 9], 'c'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 10], 'd'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 11], ' '
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 12], 'h'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 13], 'i'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 14], 's'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 15], 't'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 16], 'o'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 17], 'r'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 18], 'y'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 19], ' '
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 20], 'a'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 21], 'p'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 22], 'p'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 23], 's'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 24], ' '
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 25], 'i'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 26], 'n'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 27], 'f'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 28], 'o'
    mov byte [esp + USER_SHELL_SCRIPT_DATA + 29], ' '
    mov esi, 30

    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_help_write_output
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    cmp eax, [esp + USER_CWD_HANDLE]
    je .shell_help_have_root_handle
    mov dword [esp + USER_SHELL_BASE_TEMP], 1

.shell_help_have_root_handle:

    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 4
    int 0x80
    test eax, eax
    js .shell_help_write_output
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_LIST
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 256
    int 0x80
    test eax, eax
    js .shell_help_write_output
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    xor ecx, ecx

.shell_help_scan_entry:
    cmp ecx, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_help_write_output
    mov ebx, ecx

.shell_help_measure_entry:
    cmp ecx, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_help_entry_ready
    cmp byte [esp + USER_SHARED_BUFFER_DATA + ecx], 10
    je .shell_help_entry_ready
    inc ecx
    jmp .shell_help_measure_entry

.shell_help_entry_ready:
    mov eax, ecx
    sub eax, ebx
    cmp eax, 4
    jb .shell_help_skip_entry
    mov [esp + USER_SHELL_ARG0_LENGTH], eax
    mov eax, ebx
    add eax, [esp + USER_SHELL_ARG0_LENGTH]
    sub eax, 4
    cmp byte [esp + USER_SHARED_BUFFER_DATA + eax], '.'
    jne .shell_help_skip_entry
    inc eax
    cmp byte [esp + USER_SHARED_BUFFER_DATA + eax], 'A'
    jne .shell_help_skip_entry
    inc eax
    cmp byte [esp + USER_SHARED_BUFFER_DATA + eax], 'P'
    jne .shell_help_skip_entry
    inc eax
    cmp byte [esp + USER_SHARED_BUFFER_DATA + eax], 'P'
    jne .shell_help_skip_entry
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    sub eax, 4
    mov [esp + USER_SHELL_ARG0_LENGTH], eax
    xor edx, edx

.shell_help_copy_entry:
    cmp edx, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_help_copy_entry_done
    mov eax, ebx
    add eax, edx
    mov al, [esp + USER_SHARED_BUFFER_DATA + eax]
    cmp al, 'A'
    jb .shell_help_store_entry_char
    cmp al, 'Z'
    ja .shell_help_store_entry_char
    add al, 32

.shell_help_store_entry_char:
    mov [esp + USER_SHELL_SCRIPT_DATA + esi], al
    inc esi
    inc edx
    jmp .shell_help_copy_entry

.shell_help_copy_entry_done:
    mov byte [esp + USER_SHELL_SCRIPT_DATA + esi], ' '
    inc esi

.shell_help_skip_entry:
    cmp ecx, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_help_write_output
    inc ecx
    jmp .shell_help_scan_entry

.shell_help_detail_cleanup_unknown:
    xor esi, esi

.shell_help_detail_cleanup_emit:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_help_detail_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_help_detail_release_root:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_help_detail_emit_ready
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

.shell_help_detail_emit_ready:
    cmp esi, 0
    je .shell_write_unknown
    jmp .shell_help_console

.shell_help_write_output:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_help_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_help_release_root:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_help_finalize
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

.shell_help_finalize:
    cmp esi, 0
    je .shell_help_emit
    cmp byte [esp + USER_SHELL_SCRIPT_DATA + esi - 1], ' '
    jne .shell_help_emit
    dec esi

.shell_help_emit:
    mov byte [esp + USER_SHELL_SCRIPT_DATA + esi], 10
    inc esi
    xor ecx, ecx

.shell_help_copy_output:
    cmp ecx, esi
    jae .shell_help_console
    mov al, [esp + USER_SHELL_SCRIPT_DATA + ecx]
    mov [esp + USER_SHARED_BUFFER_DATA + ecx], al
    inc ecx
    jmp .shell_help_copy_output

.shell_help_console:
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    int 0x80
    jmp .shell_advance_line

.shell_check_apps:
    cmp ecx, 4
    jb .shell_check_info
    cmp byte [edx], 'a'
    jne .shell_check_info
    cmp byte [edx + 1], 'p'
    jne .shell_check_info
    cmp byte [edx + 2], 'p'
    jne .shell_check_info
    cmp byte [edx + 3], 's'
    jne .shell_check_info
    xor eax, eax
    mov [esp + USER_SHELL_APPS_FILTER_LENGTH], eax
    cmp dword [esp + USER_SHELL_LINE_LENGTH], 4
    je .shell_write_apps
    cmp byte [edx + 4], ' '
    jne .shell_check_pwd
    mov eax, 5

.shell_apps_skip_filter_spaces:
    cmp eax, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_write_apps
    cmp byte [edx + eax], ' '
    jne .shell_apps_filter_ready
    inc eax
    jmp .shell_apps_skip_filter_spaces

.shell_apps_filter_ready:
    mov ebx, eax

.shell_apps_measure_filter:
    cmp ebx, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_apps_store_filter
    cmp byte [edx + ebx], ' '
    je .shell_apps_store_filter
    inc ebx
    jmp .shell_apps_measure_filter

.shell_apps_store_filter:
    sub ebx, eax
    cmp ebx, 0
    je .shell_write_apps
    cmp ebx, 75
    jbe .shell_apps_filter_copy_begin
    mov ebx, 75

.shell_apps_filter_copy_begin:
    mov [esp + USER_SHELL_APPS_FILTER_LENGTH], ebx
    xor esi, esi

.shell_apps_filter_copy_loop:
    cmp esi, ebx
    jae .shell_write_apps
    mov ecx, eax
    add ecx, esi
    mov bl, [edx + ecx]
    cmp bl, 'A'
    jb .shell_apps_filter_store_char
    cmp bl, 'Z'
    ja .shell_apps_filter_store_char
    add bl, 32

.shell_apps_filter_store_char:
    mov [esp + USER_SHELL_APPS_FILTER_DATA + esi], bl
    inc esi
    jmp .shell_apps_filter_copy_loop

.shell_write_apps:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_INPUT_LENGTH], eax
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    mov [esp + USER_SHELL_APPS_CATEGORY_LENGTH], eax

    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_apps_cleanup_done
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    cmp eax, [esp + USER_CWD_HANDLE]
    je .shell_apps_have_root
    mov dword [esp + USER_SHELL_BASE_TEMP], 1

.shell_apps_have_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 4
    int 0x80
    test eax, eax
    js .shell_apps_cleanup_done
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_LIST
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 256
    int 0x80
    test eax, eax
    js .shell_apps_cleanup_done
    mov [esp + USER_SHELL_INPUT_LENGTH], eax
    xor ecx, ecx

.shell_apps_copy_listing:
    cmp ecx, [esp + USER_SHELL_INPUT_LENGTH]
    jae .shell_apps_scan_begin
    mov al, [esp + USER_SHARED_BUFFER_DATA + ecx]
    mov [esp + USER_SHELL_SCRIPT_DATA + ecx], al
    inc ecx
    jmp .shell_apps_copy_listing

.shell_apps_scan_begin:
    xor ecx, ecx

.shell_apps_scan_entry:
    cmp ecx, [esp + USER_SHELL_INPUT_LENGTH]
    jae .shell_apps_cleanup_done
    mov ebx, ecx

.shell_apps_measure_entry:
    cmp ecx, [esp + USER_SHELL_INPUT_LENGTH]
    jae .shell_apps_entry_ready
    cmp byte [esp + USER_SHELL_SCRIPT_DATA + ecx], 10
    je .shell_apps_entry_ready
    inc ecx
    jmp .shell_apps_measure_entry

.shell_apps_entry_ready:
    mov [esp + USER_SHELL_CURSOR_POS], ecx
    mov eax, ecx
    sub eax, ebx
    cmp eax, 4
    jb .shell_apps_next_entry
    mov [esp + USER_SHELL_ARG1_OFFSET], eax
    mov eax, ebx
    add eax, [esp + USER_SHELL_ARG1_OFFSET]
    sub eax, 4
    cmp byte [esp + USER_SHELL_SCRIPT_DATA + eax], '.'
    jne .shell_apps_next_entry
    inc eax
    cmp byte [esp + USER_SHELL_SCRIPT_DATA + eax], 'A'
    jne .shell_apps_next_entry
    inc eax
    cmp byte [esp + USER_SHELL_SCRIPT_DATA + eax], 'P'
    jne .shell_apps_next_entry
    inc eax
    cmp byte [esp + USER_SHELL_SCRIPT_DATA + eax], 'P'
    jne .shell_apps_next_entry
    mov eax, [esp + USER_SHELL_ARG1_OFFSET]
    sub eax, 4
    mov [esp + USER_SHELL_ARG0_LENGTH], eax
    xor edx, edx

.shell_apps_copy_name_to_draft:
    cmp edx, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_apps_copy_descriptor_name
    mov eax, ebx
    add eax, edx
    mov al, [esp + USER_SHELL_SCRIPT_DATA + eax]
    cmp al, 'A'
    jb .shell_apps_store_name_char
    cmp al, 'Z'
    ja .shell_apps_store_name_char
    add al, 32

.shell_apps_store_name_char:
    mov [esp + USER_SHELL_DRAFT_DATA + edx], al
    inc edx
    jmp .shell_apps_copy_name_to_draft

.shell_apps_copy_descriptor_name:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    xor edx, edx

.shell_apps_copy_descriptor_name_loop:
    cmp edx, [esp + USER_SHELL_ARG1_OFFSET]
    jae .shell_apps_open_descriptor
    mov eax, ebx
    add eax, edx
    mov al, [esp + USER_SHELL_SCRIPT_DATA + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + edx + 5], al
    inc edx
    jmp .shell_apps_copy_descriptor_name_loop

.shell_apps_open_descriptor:
    mov byte [esp + USER_SHARED_BUFFER_DATA + edx + 5], 0
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, [esp + USER_SHELL_ARG1_OFFSET]
    add edx, 5
    int 0x80
    test eax, eax
    js .shell_apps_next_entry
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 192
    int 0x80
    test eax, eax
    js .shell_apps_release_descriptor
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    xor eax, eax
    mov [esp + USER_SHELL_ARG1_OFFSET], eax
    mov [esp + USER_SHELL_APPS_CATEGORY_LENGTH], eax
    xor esi, esi
    xor edx, edx

.shell_apps_seek_summary:
    cmp esi, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_apps_build_output
    mov al, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp al, 10
    jne .shell_apps_seek_summary_next
    inc edx
    cmp edx, 4
    je .shell_apps_copy_summary_begin

.shell_apps_seek_summary_next:
    inc esi
    jmp .shell_apps_seek_summary

.shell_apps_copy_summary_begin:
    inc esi
    xor edx, edx

.shell_apps_copy_summary_loop:
    mov eax, esi
    add eax, edx
    cmp eax, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_apps_store_summary_length
    mov al, [esp + USER_SHARED_BUFFER_DATA + eax]
    cmp al, 10
    je .shell_apps_store_summary_length
    cmp edx, 191
    jae .shell_apps_store_summary_length
    mov [esp + USER_SHELL_DRAFT_SUMMARY + edx], al
    inc edx
    jmp .shell_apps_copy_summary_loop

.shell_apps_store_summary_length:
    mov [esp + USER_SHELL_ARG1_OFFSET], edx
    mov eax, esi
    add eax, edx
    cmp eax, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_apps_check_filter
    mov al, [esp + USER_SHARED_BUFFER_DATA + eax]
    cmp al, 10
    jne .shell_apps_check_filter
    add esi, edx
    inc esi
    xor edx, edx

.shell_apps_copy_category_loop:
    mov eax, esi
    add eax, edx
    cmp eax, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_apps_store_category_length
    mov al, [esp + USER_SHARED_BUFFER_DATA + eax]
    cmp al, 10
    je .shell_apps_store_category_length
    cmp edx, 127
    jae .shell_apps_store_category_length
    mov [esp + USER_SHELL_DRAFT_CATEGORY + edx], al
    inc edx
    jmp .shell_apps_copy_category_loop

.shell_apps_store_category_length:
    mov [esp + USER_SHELL_APPS_CATEGORY_LENGTH], edx

.shell_apps_check_filter:
    cmp dword [esp + USER_SHELL_APPS_FILTER_LENGTH], 0
    je .shell_apps_build_output
    mov eax, [esp + USER_SHELL_APPS_FILTER_LENGTH]
    cmp eax, [esp + USER_SHELL_APPS_CATEGORY_LENGTH]
    jne .shell_apps_release_descriptor
    xor esi, esi

.shell_apps_compare_filter:
    cmp esi, eax
    jae .shell_apps_build_output
    mov bl, [esp + USER_SHELL_APPS_FILTER_DATA + esi]
    cmp bl, [esp + USER_SHELL_DRAFT_CATEGORY + esi]
    jne .shell_apps_release_descriptor
    inc esi
    jmp .shell_apps_compare_filter

.shell_apps_build_output:
    xor esi, esi
    xor edx, edx

    cmp dword [esp + USER_SHELL_APPS_CATEGORY_LENGTH], 0
    je .shell_apps_copy_output_name
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], '['
    inc esi

.shell_apps_copy_output_category:
    cmp edx, [esp + USER_SHELL_APPS_CATEGORY_LENGTH]
    jae .shell_apps_output_after_category
    mov al, [esp + USER_SHELL_DRAFT_CATEGORY + edx]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], al
    inc edx
    inc esi
    jmp .shell_apps_copy_output_category

.shell_apps_output_after_category:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], ']'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 1], ' '
    add esi, 2
    xor edx, edx

.shell_apps_copy_output_name:
    cmp edx, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_apps_output_after_name
    mov al, [esp + USER_SHELL_DRAFT_DATA + edx]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], al
    inc edx
    inc esi
    jmp .shell_apps_copy_output_name

.shell_apps_output_after_name:
    cmp dword [esp + USER_SHELL_ARG1_OFFSET], 0
    je .shell_apps_emit_line
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 1], '-'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 2], ' '
    add esi, 3
    xor edx, edx

.shell_apps_copy_output_summary:
    cmp edx, [esp + USER_SHELL_ARG1_OFFSET]
    jae .shell_apps_emit_line
    mov al, [esp + USER_SHELL_DRAFT_SUMMARY + edx]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], al
    inc edx
    inc esi
    jmp .shell_apps_copy_output_summary

.shell_apps_emit_line:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 10
    inc esi
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    int 0x80

.shell_apps_release_descriptor:
    cmp dword [esp + USER_SHELL_ENDPOINT_CAP], 0
    je .shell_apps_next_entry
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_apps_next_entry:
    mov ecx, [esp + USER_SHELL_CURSOR_POS]
    cmp ecx, [esp + USER_SHELL_INPUT_LENGTH]
    jae .shell_apps_cleanup_done
    inc ecx
    jmp .shell_apps_scan_entry

.shell_apps_cleanup_done:
    cmp dword [esp + USER_SHELL_ENDPOINT_CAP], 0
    je .shell_apps_cleanup_release_apps
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_apps_cleanup_release_apps:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_apps_cleanup_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_apps_cleanup_release_root:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_advance_line
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_advance_line

.shell_check_info:
    cmp ecx, 4
    jb .shell_check_history
    cmp byte [edx], 'i'
    jne .shell_check_history
    cmp byte [edx + 1], 'n'
    jne .shell_check_history
    cmp byte [edx + 2], 'f'
    jne .shell_check_history
    cmp byte [edx + 3], 'o'
    jne .shell_check_history
    cmp ecx, 4
    je .shell_write_info_usage
    cmp byte [edx + 4], ' '
    jne .shell_check_history
    mov eax, 5

.shell_info_skip_spaces:
    cmp eax, ecx
    jae .shell_write_info_usage
    cmp byte [edx + eax], ' '
    jne .shell_info_arg_ready
    inc eax
    jmp .shell_info_skip_spaces

.shell_info_arg_ready:
    mov [esp + USER_SHELL_ARG1_OFFSET], eax
    mov ebx, eax

.shell_info_measure_arg:
    cmp ebx, ecx
    jae .shell_info_have_arg
    cmp byte [edx + ebx], ' '
    je .shell_info_have_arg
    inc ebx
    jmp .shell_info_measure_arg

.shell_info_have_arg:
    sub ebx, eax
    cmp ebx, 0
    je .shell_write_info_usage
    cmp ebx, 63
    jbe .shell_info_store_arg_length
    mov ebx, 63

.shell_info_store_arg_length:
    mov [esp + USER_SHELL_ARG0_LENGTH], ebx
    xor esi, esi

.shell_info_copy_arg_lower:
    cmp esi, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_write_info
    mov ecx, eax
    add ecx, esi
    mov al, [edx + ecx]
    cmp al, 'A'
    jb .shell_info_store_arg_char
    cmp al, 'Z'
    ja .shell_info_store_arg_char
    add al, 32

.shell_info_store_arg_char:
    mov [esp + USER_SHELL_DRAFT_DATA + esi], al
    inc esi
    jmp .shell_info_copy_arg_lower

.shell_write_info_usage:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'f'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], '<'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], '>'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], '-'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 16], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 17], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 18], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 19], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 20], 'w'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 21], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 22], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 23], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 24], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 25], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 26], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 27], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 28], 'l'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 29], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 30], 'u'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 31], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 32], 'c'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 33], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 34], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 35], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 36], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 37], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 38], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 39], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 40], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 41], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 42], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 43], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 44], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 45], 10
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 46
    int 0x80
    jmp .shell_advance_line

.shell_write_info:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    mov [esp + USER_SHELL_APPS_CATEGORY_LENGTH], eax
    mov [esp + USER_SHELL_CURSOR_POS], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax
    mov [esp + USER_SHELL_LAUNCH_FLAGS], eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax

    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_info_unknown
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    cmp eax, [esp + USER_CWD_HANDLE]
    je .shell_info_have_root
    mov dword [esp + USER_SHELL_BASE_TEMP], 1

.shell_info_have_root:
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov esi, 5
    xor eax, eax

.shell_info_copy_descriptor_name:
    cmp eax, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_info_descriptor_suffix
    mov ebx, [esp + USER_SHELL_ARG1_OFFSET]
    add ebx, eax
    mov bl, [edx + ebx]
    cmp bl, 'a'
    jb .shell_info_store_descriptor_name
    cmp bl, 'z'
    ja .shell_info_store_descriptor_name
    sub bl, 32

.shell_info_store_descriptor_name:
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_info_copy_descriptor_name

.shell_info_descriptor_suffix:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 1], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 3], 'P'
    add esi, 4
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, esi
    int 0x80
    test eax, eax
    js .shell_info_cleanup_unknown
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 192
    int 0x80
    test eax, eax
    js .shell_info_cleanup_unknown
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    xor eax, eax

.shell_info_copy_descriptor:
    cmp eax, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_info_decode_command
    mov bl, [esp + USER_SHARED_BUFFER_DATA + eax]
    mov [esp + USER_SHELL_DRAFT_SUMMARY + eax], bl
    inc eax
    jmp .shell_info_copy_descriptor

.shell_info_decode_command:
    call .shell_info_get_string_base
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov ebp, esp
    xor edi, edi
    xor eax, eax
    call .shell_info_copy_relative_cstring
    mov ecx, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea esi, [esp + USER_SHELL_SCRIPT_DATA + ecx]
    xor ebx, ebx

.shell_info_copy_command_name:
    cmp ebx, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_info_command_name_done
    mov ecx, [esp + USER_SHELL_ARG1_OFFSET]
    add ecx, ebx
    mov al, [esi + ecx]
    cmp al, 'A'
    jb .shell_info_store_command_char
    cmp al, 'Z'
    ja .shell_info_store_command_char
    add al, 32

.shell_info_store_command_char:
    mov [esp + USER_SHARED_BUFFER_DATA + edi], al
    inc edi
    inc ebx
    jmp .shell_info_copy_command_name

.shell_info_command_name_done:
    call .shell_info_emit_line

    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    xor eax, eax
    call .shell_info_parse_decimal_line
    cmp ebx, 0
    je .shell_info_cleanup_unknown
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov eax, edx
    xor edi, edi
    mov eax, shell_info_label_launch - shell_info_label_command
    call .shell_info_copy_relative_cstring
    mov eax, edx
    call .shell_info_copy_exec_name
    call .shell_info_emit_line

    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov eax, [esp + USER_SHELL_DESCRIPTOR_TEMP]
    call .shell_info_parse_decimal_line
    cmp ebx, 0
    je .shell_info_cleanup_unknown
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov eax, edx
    xor edi, edi
    mov eax, shell_info_label_authority - shell_info_label_command
    call .shell_info_copy_relative_cstring
    mov eax, edx
    call .shell_info_copy_authority_words
    call .shell_info_emit_line

    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov eax, [esp + USER_SHELL_DESCRIPTOR_TEMP]
    call .shell_info_parse_decimal_line
    cmp ebx, 0
    je .shell_info_cleanup_unknown
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov eax, edx
    xor edi, edi
    mov eax, shell_info_label_policy - shell_info_label_command
    call .shell_info_copy_relative_cstring
    mov eax, edx
    call .shell_info_copy_policy_name
    call .shell_info_emit_line

    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov eax, [esp + USER_SHELL_DESCRIPTOR_TEMP]
    call .shell_info_parse_decimal_line
    cmp ebx, 0
    je .shell_info_cleanup_unknown
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov eax, edx
    xor edi, edi
    mov eax, shell_info_label_bindings - shell_info_label_command
    call .shell_info_copy_relative_cstring
    mov eax, edx
    call .shell_info_copy_binding_words
    call .shell_info_emit_line

    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov eax, [esp + USER_SHELL_DESCRIPTOR_TEMP]
    mov edx, eax
    xor ebx, ebx

.shell_info_measure_summary:
    cmp eax, ecx
    jae .shell_info_summary_ready
    cmp byte [esi + eax], 10
    je .shell_info_summary_newline
    inc eax
    inc ebx
    jmp .shell_info_measure_summary

.shell_info_summary_newline:
    inc eax

.shell_info_summary_ready:
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    xor edi, edi
    mov eax, shell_info_label_summary - shell_info_label_command
    call .shell_info_copy_relative_cstring
    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    add esi, edx
    mov ecx, ebx
    call .shell_info_copy_bytes
    call .shell_info_emit_line

    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov eax, [esp + USER_SHELL_DESCRIPTOR_TEMP]
    mov edx, eax
    xor ebx, ebx

.shell_info_measure_category:
    cmp eax, ecx
    jae .shell_info_category_ready
    cmp byte [esi + eax], 10
    je .shell_info_category_ready
    inc eax
    inc ebx
    jmp .shell_info_measure_category

.shell_info_category_ready:
    xor edi, edi
    mov eax, shell_info_label_category - shell_info_label_command
    call .shell_info_copy_relative_cstring
    lea esi, [esp + USER_SHELL_DRAFT_SUMMARY]
    add esi, edx
    mov ecx, ebx
    call .shell_info_copy_bytes
    call .shell_info_emit_line
    jmp .shell_info_cleanup_done

.shell_info_cleanup_unknown:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_info_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_info_unknown:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_write_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_write_unknown

.shell_info_cleanup_done:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_info_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_info_release_root:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_advance_line
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_advance_line

.shell_check_history:
    cmp ecx, 7
    jne .shell_check_pwd
    cmp byte [edx], 'h'
    jne .shell_check_pwd
    cmp byte [edx + 1], 'i'
    jne .shell_check_pwd
    cmp byte [edx + 2], 's'
    jne .shell_check_pwd
    cmp byte [edx + 3], 't'
    jne .shell_check_pwd
    cmp byte [edx + 4], 'o'
    jne .shell_check_pwd
    cmp byte [edx + 5], 'r'
    jne .shell_check_pwd
    cmp byte [edx + 6], 'y'
    jne .shell_check_pwd
    cmp dword [esp + USER_SHELL_HISTORY_COUNT], 0
    je .shell_write_history_empty
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

.shell_write_history_entry:
    mov eax, [esp + USER_SHELL_BASE_TEMP]
    cmp eax, [esp + USER_SHELL_HISTORY_COUNT]
    jae .shell_advance_line
    mov ebx, eax
    shl ebx, 2
    mov esi, [esp + USER_SHELL_HISTORY_LENGTHS + ebx]
    mov ebx, eax
    shl ebx, 8
    lea ecx, [esp + USER_SHELL_HISTORY_DATA]
    add ecx, ebx
    xor edx, edx

.shell_copy_history_entry:
    cmp edx, esi
    jae .shell_emit_history_entry
    mov al, [ecx + edx]
    mov [esp + USER_SHARED_BUFFER_DATA + edx], al
    inc edx
    jmp .shell_copy_history_entry

.shell_emit_history_entry:
    mov byte [esp + USER_SHARED_BUFFER_DATA + edx], 10
    inc edx
    mov esi, edx
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    int 0x80
    mov eax, [esp + USER_SHELL_BASE_TEMP]
    inc eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_write_history_entry

.shell_write_history_empty:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'h'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 's'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'r'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], ' '
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'm'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'p'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 't'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'y'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 10
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 14
    int 0x80
    jmp .shell_advance_line

.shell_check_pwd:
    cmp ecx, 3
    jne .shell_check_cd
    cmp byte [edx], 'p'
    jne .shell_check_cd
    cmp byte [edx + 1], 'w'
    jne .shell_check_cd
    cmp byte [edx + 2], 'd'
    jne .shell_check_cd

.shell_write_pwd:
    xor eax, eax
.shell_copy_pwd:
    cmp eax, [esp + USER_CWD_PATH_LENGTH]
    jae .shell_copy_pwd_done
    mov bl, [esp + USER_CWD_PATH_DATA + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + eax], bl
    inc eax
    jmp .shell_copy_pwd

.shell_copy_pwd_done:
    mov byte [esp + USER_SHARED_BUFFER_DATA + eax], 10
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, [esp + USER_CWD_PATH_LENGTH]
    inc esi
    int 0x80
    jmp .shell_advance_line

.shell_check_cd:
    cmp ecx, 4
    jb .shell_check_ls
    cmp byte [edx], 'c'
    jne .shell_check_ls
    cmp byte [edx + 1], 'd'
    jne .shell_check_ls
    cmp byte [edx + 2], ' '
    jne .shell_check_ls

    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 3], '/'
    jne .shell_copy_cd_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_cd_open_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_cd_path

.shell_cd_open_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_cd_path:
    xor eax, eax
.shell_copy_cd_path:
    mov esi, ecx
    sub esi, 3
    cmp eax, esi
    jae .shell_cd_open
    mov bl, [edx + eax + 3]
    mov [esp + USER_SHARED_BUFFER_DATA + eax], bl
    inc eax
    jmp .shell_copy_cd_path

.shell_cd_open:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov [esp + USER_SHELL_ARG0_LENGTH], esi
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, [esp + USER_SHELL_ARG0_LENGTH]
    int 0x80
    test eax, eax
    js .shell_cd_base_error
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_LIST
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_cd_revoke_error

    cmp dword [esp + USER_SHELL_ARG0_LENGTH], 1
    jne .shell_cd_check_absolute
    cmp byte [esp + USER_SHARED_BUFFER_DATA], '.'
    jne .shell_cd_check_absolute
    jmp .shell_cd_commit_handle

.shell_cd_check_absolute:
    cmp byte [esp + USER_SHARED_BUFFER_DATA], '/'
    jne .shell_cd_build_relative
    xor eax, eax

.shell_cd_copy_absolute:
    cmp eax, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_cd_trim
    mov bl, [esp + USER_SHARED_BUFFER_DATA + eax]
    mov [esp + USER_CWD_PATH_DATA + eax], bl
    inc eax
    jmp .shell_cd_copy_absolute

.shell_cd_build_relative:
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_cd_copy_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    jne .shell_cd_copy_base
    mov byte [esp + USER_CWD_PATH_DATA], '/'
    mov eax, 1
    jmp .shell_cd_copy_relative

.shell_cd_copy_base:
    xor eax, eax

.shell_cd_copy_base_loop:
    cmp eax, [esp + USER_CWD_PATH_LENGTH]
    jae .shell_cd_add_separator
    mov bl, [esp + USER_CWD_PATH_DATA + eax]
    mov [esp + USER_CWD_PATH_DATA + eax], bl
    inc eax
    jmp .shell_cd_copy_base_loop

.shell_cd_add_separator:
    mov byte [esp + USER_CWD_PATH_DATA + eax], '/'
    inc eax

.shell_cd_copy_relative:
    xor esi, esi

.shell_cd_copy_relative_loop:
    cmp esi, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_cd_trim
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    mov [esp + USER_CWD_PATH_DATA + eax], bl
    inc eax
    inc esi
    jmp .shell_cd_copy_relative_loop

.shell_cd_trim:
    cmp eax, 1
    jbe .shell_cd_store_length
    cmp byte [esp + USER_CWD_PATH_DATA + eax - 1], '/'
    jne .shell_cd_store_length
    dec eax
    jmp .shell_cd_trim

.shell_cd_store_length:
    mov [esp + USER_CWD_PATH_LENGTH], eax
    mov byte [esp + USER_CWD_PATH_DATA + eax], 0

.shell_cd_commit_handle:
    mov eax, [esp + USER_CWD_HANDLE]
    cmp [esp + USER_SHELL_TEMP_HANDLE], eax
    je .shell_cd_cleanup_base
    cmp eax, 0
    je .shell_cd_store_handle
    mov ebx, eax
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    int 0x80

.shell_cd_store_handle:
    mov eax, [esp + USER_SHELL_TEMP_HANDLE]
    mov [esp + USER_CWD_HANDLE], eax

.shell_cd_cleanup_base:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_cd_keep_handle
    mov eax, [esp + USER_SHELL_BASE_HANDLE]
    cmp eax, [esp + USER_CWD_HANDLE]
    je .shell_cd_clear_base
    mov ebx, eax
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    int 0x80

.shell_cd_clear_base:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

.shell_cd_keep_handle:
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    jmp .shell_advance_line

.shell_cd_revoke_error:
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_cd_base_error:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_write_error
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_write_error

.shell_check_ls:
    cmp ecx, 2
    jne .shell_check_ls_path
    cmp byte [edx], 'l'
    jne .shell_check_ls_path
    cmp byte [edx + 1], 's'
    jne .shell_check_ls_path
    mov dword [esp + USER_SHELL_ARG1_OFFSET], 1
    jmp .shell_resolve_ls_descriptor

.shell_ls_ready_default:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov byte [esp + USER_SHARED_BUFFER_DATA], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 0
    mov esi, 1
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_check_ls_path:
    cmp ecx, 4
    jb .shell_check_cat
    cmp byte [edx], 'l'
    jne .shell_check_cat
    cmp byte [edx + 1], 's'
    jne .shell_check_cat
    cmp byte [edx + 2], ' '
    jne .shell_check_cat
    mov dword [esp + USER_SHELL_ARG1_OFFSET], 2
    jmp .shell_resolve_ls_descriptor

.shell_ls_ready_path:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 3], '/'
    jne .shell_prepare_ls_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_ls_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_ls_path

.shell_open_ls_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_ls_path:
    xor eax, eax
.shell_copy_ls_path:
    mov esi, ecx
    sub esi, 3
    cmp eax, esi
    jae .shell_launch_ls_utility
    mov bl, [edx + eax + 3]
    mov [esp + USER_SHARED_BUFFER_DATA + eax], bl
    inc eax
    jmp .shell_copy_ls_path

.shell_launch_ls_utility:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_ls_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_ls_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_ls_descriptor_root

.shell_open_ls_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_ls_descriptor_unknown
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    jmp .shell_have_ls_descriptor_handle

.shell_have_ls_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax

.shell_have_ls_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'L'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 11
    int 0x80
    test eax, eax
    js .shell_ls_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_ls_descriptor_cleanup_unknown
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_ls_descriptor_exec:
    cmp esi, ecx
    jae .shell_ls_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_ls_descriptor_ready
    cmp bl, '9'
    ja .shell_ls_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_ls_descriptor_exec

.shell_ls_descriptor_ready:
    cmp eax, 0
    je .shell_ls_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_PATH
    jne .shell_ls_descriptor_cleanup_unknown
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_PATH
    jne .shell_ls_descriptor_cleanup_unknown
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_ls_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_ls_descriptor_release_base
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_ls_descriptor_release_base:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_ls_descriptor_continue
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

.shell_ls_descriptor_continue:
    cmp dword [esp + USER_SHELL_ARG1_OFFSET], 1
    je .shell_ls_ready_default
    jmp .shell_ls_ready_path

.shell_ls_descriptor_cleanup_unknown:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_ls_descriptor_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_ls_descriptor_unknown:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_write_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_write_unknown

.shell_check_cat:
    cmp ecx, 5
    jb .shell_check_stat
    cmp byte [edx], 'c'
    jne .shell_check_stat
    cmp byte [edx + 1], 'a'
    jne .shell_check_stat
    cmp byte [edx + 2], 't'
    jne .shell_check_stat
    cmp byte [edx + 3], ' '
    jne .shell_check_stat
    jmp .shell_resolve_cat_descriptor

.shell_cat_ready_path:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 4], '/'
    jne .shell_prepare_cat_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_cat_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_cat_path

.shell_open_cat_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_cat_path:
    xor eax, eax
.shell_copy_cat_path:
    mov esi, ecx
    sub esi, 4
    cmp eax, esi
    jae .shell_launch_cat_utility
    mov bl, [edx + eax + 4]
    mov [esp + USER_SHARED_BUFFER_DATA + eax], bl
    inc eax
    jmp .shell_copy_cat_path

.shell_launch_cat_utility:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_cat_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_cat_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_cat_descriptor_root

.shell_open_cat_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_cat_descriptor_unknown
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    jmp .shell_have_cat_descriptor_handle

.shell_have_cat_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax

.shell_have_cat_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'C'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'T'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 12
    int 0x80
    test eax, eax
    js .shell_cat_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_cat_descriptor_cleanup_unknown
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_cat_descriptor_exec:
    cmp esi, ecx
    jae .shell_cat_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_cat_descriptor_ready
    cmp bl, '9'
    ja .shell_cat_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_cat_descriptor_exec

.shell_cat_descriptor_ready:
    cmp eax, 0
    je .shell_cat_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_PATH
    jne .shell_cat_descriptor_cleanup_unknown
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_PATH
    jne .shell_cat_descriptor_cleanup_unknown
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_cat_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_cat_descriptor_release_base
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_cat_descriptor_release_base:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_cat_ready_path
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_cat_ready_path

.shell_cat_descriptor_cleanup_unknown:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_cat_descriptor_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_cat_descriptor_unknown:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_write_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_write_unknown

.shell_check_stat:
    cmp ecx, 6
    jb .shell_check_move
    cmp byte [edx], 's'
    jne .shell_check_move
    cmp byte [edx + 1], 't'
    jne .shell_check_move
    cmp byte [edx + 2], 'a'
    jne .shell_check_move
    cmp byte [edx + 3], 't'
    jne .shell_check_move
    cmp byte [edx + 4], ' '
    jne .shell_check_move
    jmp .shell_resolve_stat_descriptor

.shell_stat_ready_path:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 5], '/'
    jne .shell_prepare_stat_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_stat_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_stat_path

.shell_open_stat_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_stat_path:
    xor eax, eax
.shell_copy_stat_path:
    mov esi, ecx
    sub esi, 5
    cmp eax, esi
    jae .shell_launch_stat_utility
    mov bl, [edx + eax + 5]
    mov [esp + USER_SHARED_BUFFER_DATA + eax], bl
    inc eax
    jmp .shell_copy_stat_path

.shell_launch_stat_utility:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_stat_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_stat_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_stat_descriptor_root

.shell_open_stat_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_stat_descriptor_unknown
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    jmp .shell_have_stat_descriptor_handle

.shell_have_stat_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax

.shell_have_stat_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'T'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'T'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 13
    int 0x80
    test eax, eax
    js .shell_stat_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_stat_descriptor_cleanup_unknown
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_stat_descriptor_exec:
    cmp esi, ecx
    jae .shell_stat_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_stat_descriptor_ready
    cmp bl, '9'
    ja .shell_stat_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_stat_descriptor_exec

.shell_stat_descriptor_ready:
    cmp eax, 0
    je .shell_stat_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_PATH
    jne .shell_stat_descriptor_cleanup_unknown
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_PATH
    jne .shell_stat_descriptor_cleanup_unknown
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_stat_descriptor_cleanup_unknown
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_stat_descriptor_release_base
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_stat_descriptor_release_base:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_stat_ready_path
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_stat_ready_path

.shell_stat_descriptor_cleanup_unknown:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_stat_descriptor_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_stat_descriptor_unknown:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_write_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_write_unknown

.shell_check_move:
    cmp ecx, 8
    jb .shell_check_rename
    cmp byte [edx], 'm'
    jne .shell_check_rename
    cmp byte [edx + 1], 'o'
    jne .shell_check_rename
    cmp byte [edx + 2], 'v'
    jne .shell_check_rename
    cmp byte [edx + 3], 'e'
    jne .shell_check_rename
    cmp byte [edx + 4], ' '
    jne .shell_check_rename
    jmp .shell_resolve_move_descriptor

.shell_move_ready:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_DEST_TEMP], eax

    cmp byte [edx + 5], '/'
    jne .shell_move_prepare_source_done
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_move_source_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_move_prepare_source_done

.shell_open_move_source_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_move_prepare_source_done:
    mov eax, 5
    xor esi, esi

.shell_copy_move_source:
    cmp eax, ecx
    jae .shell_write_error
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_move_source_done
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_move_source

.shell_move_source_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    inc eax
    cmp eax, ecx
    jae .shell_write_error
    mov [esp + USER_SHELL_ARG1_OFFSET], esi
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    cmp byte [edx + eax], '/'
    jne .shell_move_prepare_dest_done
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_move_dest_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_move_prepare_dest_done

.shell_open_move_dest_root_base:
    movzx ebx, byte [esp + USER_SHARED_BUFFER_DATA]
    mov [esp + USER_SHELL_ENDPOINT_CAP], ebx
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov dword [esp + USER_SHELL_DEST_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov [esp + USER_SHARED_BUFFER_DATA], bl

.shell_move_prepare_dest_done:
    mov eax, [esp + USER_SHELL_TEMP_HANDLE]
    mov esi, [esp + USER_SHELL_ARG1_OFFSET]
.shell_copy_move_dest:
    cmp eax, ecx
    jae .shell_move_dest_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_move_dest

.shell_move_dest_done:
    mov ebx, esi
    sub ebx, [esp + USER_SHELL_ARG1_OFFSET]
    cmp ebx, 0
    je .shell_write_error
    mov [esp + USER_SHELL_LINE_LENGTH], ebx
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_move_descriptor_launch:
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_move_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_move_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_move_descriptor_root

.shell_open_move_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_move_descriptor_cleanup_error
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 1
    jmp .shell_have_move_descriptor_handle

.shell_have_move_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_have_move_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'M'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'O'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'V'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 13
    int 0x80
    test eax, eax
    js .shell_move_descriptor_cleanup_error
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_move_descriptor_cleanup_error
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_move_descriptor_exec:
    cmp esi, ecx
    jae .shell_move_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_move_descriptor_ready
    cmp bl, '9'
    ja .shell_move_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_move_descriptor_exec

.shell_move_descriptor_ready:
    cmp eax, 0
    je .shell_move_descriptor_cleanup_error
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_MOVE
    jne .shell_move_descriptor_cleanup_error
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_MOVE
    jne .shell_move_descriptor_cleanup_error
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_move_descriptor_cleanup_error
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_move_descriptor_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_move_descriptor_release_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_move_descriptor_clear_temps
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_move_descriptor_clear_temps:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_move_ready

.shell_move_descriptor_cleanup_error:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_move_descriptor_cleanup_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_move_descriptor_cleanup_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_move_descriptor_cleanup_done
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_move_descriptor_cleanup_done:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_write_unknown

.shell_check_rename:
    cmp ecx, 10
    jb .shell_check_append
    cmp byte [edx], 'r'
    jne .shell_check_append
    cmp byte [edx + 1], 'e'
    jne .shell_check_append
    cmp byte [edx + 2], 'n'
    jne .shell_check_append
    cmp byte [edx + 3], 'a'
    jne .shell_check_append
    cmp byte [edx + 4], 'm'
    jne .shell_check_append
    cmp byte [edx + 5], 'e'
    jne .shell_check_append
    cmp byte [edx + 6], ' '
    jne .shell_check_mkdir
    jmp .shell_resolve_rename_descriptor

.shell_rename_ready:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 7], '/'
    jne .shell_rename_prepare_done
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_rename_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_rename_prepare_done

.shell_open_rename_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_rename_prepare_done:
    mov eax, 7
    xor esi, esi

.shell_copy_rename_source:
    cmp eax, ecx
    jae .shell_write_error
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_rename_source_done
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_rename_source

.shell_rename_source_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    inc eax
    cmp eax, ecx
    jae .shell_write_error
    mov [esp + USER_SHELL_ARG1_OFFSET], esi

.shell_copy_rename_dest:
    cmp eax, ecx
    jae .shell_rename_dest_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_rename_dest

.shell_rename_dest_done:
    mov ebx, esi
    sub ebx, [esp + USER_SHELL_ARG1_OFFSET]
    cmp ebx, 0
    je .shell_write_error
    mov [esp + USER_SHELL_LINE_LENGTH], ebx
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    cmp byte [esp + USER_SHARED_BUFFER_DATA], '/'
    sete al
    movzx eax, al
    mov ebx, [esp + USER_SHELL_ARG1_OFFSET]
    cmp byte [esp + USER_SHARED_BUFFER_DATA + ebx], '/'
    sete bl
    movzx ebx, bl
    cmp eax, ebx
    jne .shell_write_error
    test eax, eax
    jnz .shell_launch_rename_utility

.shell_launch_rename_utility:
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_rename_descriptor_launch:
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_rename_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_rename_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_rename_descriptor_root

.shell_open_rename_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_rename_descriptor_cleanup_error
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 1
    jmp .shell_have_rename_descriptor_handle

.shell_have_rename_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_have_rename_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'R'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'N'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'M'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 15
    int 0x80
    test eax, eax
    js .shell_rename_descriptor_cleanup_error
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_rename_descriptor_cleanup_error
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_rename_descriptor_exec:
    cmp esi, ecx
    jae .shell_rename_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_rename_descriptor_ready
    cmp bl, '9'
    ja .shell_rename_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_rename_descriptor_exec

.shell_rename_descriptor_ready:
    cmp eax, 0
    je .shell_rename_descriptor_cleanup_error
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_RENAME
    jne .shell_rename_descriptor_cleanup_error
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_RENAME
    jne .shell_rename_descriptor_cleanup_error
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_rename_descriptor_cleanup_error
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_rename_descriptor_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_rename_descriptor_release_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_rename_descriptor_clear_temps
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_rename_descriptor_clear_temps:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_rename_ready

.shell_rename_descriptor_cleanup_error:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_rename_descriptor_cleanup_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_rename_descriptor_cleanup_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_rename_descriptor_cleanup_done
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_rename_descriptor_cleanup_done:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_write_unknown

.shell_check_append:
    cmp ecx, 10
    jb .shell_check_delete
    cmp byte [edx], 'a'
    jne .shell_check_delete
    cmp byte [edx + 1], 'p'
    jne .shell_check_delete
    cmp byte [edx + 2], 'p'
    jne .shell_check_delete
    cmp byte [edx + 3], 'e'
    jne .shell_check_delete
    cmp byte [edx + 4], 'n'
    jne .shell_check_delete
    cmp byte [edx + 5], 'd'
    jne .shell_check_delete
    cmp byte [edx + 6], ' '
    jne .shell_check_mkdir
    jmp .shell_resolve_append_descriptor

.shell_append_ready:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 7], '/'
    jne .shell_prepare_append_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_append_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_append_path

.shell_open_append_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_append_path:
    mov eax, 7
    xor esi, esi

.shell_copy_append_path:
    cmp eax, ecx
    jae .shell_write_error
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_append_path_done
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_append_path

.shell_append_path_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    inc eax
    cmp eax, ecx
    jae .shell_write_error
    mov [esp + USER_SHELL_ARG1_OFFSET], esi

.shell_copy_append_text:
    cmp eax, ecx
    jae .shell_append_text_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_append_text

.shell_append_text_done:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 10
    inc esi
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, esi
    sub eax, [esp + USER_SHELL_ARG1_OFFSET]
    mov [esp + USER_SHELL_LINE_LENGTH], eax
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_append_descriptor_launch:
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_append_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_append_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_append_descriptor_root

.shell_open_append_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_append_descriptor_cleanup_error
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 1
    jmp .shell_have_append_descriptor_handle

.shell_have_append_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_have_append_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'N'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'D'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 15
    int 0x80
    test eax, eax
    js .shell_append_descriptor_cleanup_error
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_append_descriptor_cleanup_error
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_append_descriptor_exec:
    cmp esi, ecx
    jae .shell_append_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_append_descriptor_ready
    cmp bl, '9'
    ja .shell_append_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_append_descriptor_exec

.shell_append_descriptor_ready:
    cmp eax, 0
    je .shell_append_descriptor_cleanup_error
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_TEXT
    jne .shell_append_descriptor_cleanup_error
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_TEXT
    jne .shell_append_descriptor_cleanup_error
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_append_descriptor_cleanup_error
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_append_descriptor_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_append_descriptor_release_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_append_descriptor_clear_temps
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_append_descriptor_clear_temps:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_append_ready

.shell_append_descriptor_cleanup_error:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_append_descriptor_cleanup_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_append_descriptor_cleanup_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_append_descriptor_cleanup_done
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_append_descriptor_cleanup_done:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_write_unknown

.shell_check_delete:
    cmp ecx, 8
    jb .shell_check_mkdir
    cmp byte [edx], 'd'
    jne .shell_check_mkdir
    cmp byte [edx + 1], 'e'
    jne .shell_check_mkdir
    cmp byte [edx + 2], 'l'
    jne .shell_check_mkdir
    cmp byte [edx + 3], 'e'
    jne .shell_check_mkdir
    cmp byte [edx + 4], 't'
    jne .shell_check_mkdir
    cmp byte [edx + 5], 'e'
    jne .shell_check_mkdir
    cmp byte [edx + 6], ' '
    jne .shell_check_mkdir
    jmp .shell_resolve_delete_descriptor

.shell_delete_ready:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 7], '/'
    jne .shell_prepare_delete_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_delete_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_delete_path

.shell_open_delete_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_delete_path:
    xor eax, eax
.shell_copy_delete_path:
    mov esi, ecx
    sub esi, 7
    cmp eax, esi
    jae .shell_launch_delete_utility
    mov bl, [edx + eax + 7]
    mov [esp + USER_SHARED_BUFFER_DATA + eax], bl
    inc eax
    jmp .shell_copy_delete_path

.shell_launch_delete_utility:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_delete_descriptor_launch:
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_delete_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_delete_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_delete_descriptor_root

.shell_open_delete_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_delete_descriptor_cleanup_error
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 1
    jmp .shell_have_delete_descriptor_handle

.shell_have_delete_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_have_delete_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'D'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'L'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'T'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 15], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 15
    int 0x80
    test eax, eax
    js .shell_delete_descriptor_cleanup_error
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_delete_descriptor_cleanup_error
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_delete_descriptor_exec:
    cmp esi, ecx
    jae .shell_delete_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_delete_descriptor_ready
    cmp bl, '9'
    ja .shell_delete_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_delete_descriptor_exec

.shell_delete_descriptor_ready:
    cmp eax, 0
    je .shell_delete_descriptor_cleanup_error
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_PATH
    jne .shell_delete_descriptor_cleanup_error
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_PATH
    jne .shell_delete_descriptor_cleanup_error
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_delete_descriptor_cleanup_error
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_delete_descriptor_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_delete_descriptor_release_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_delete_descriptor_clear_temps
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_delete_descriptor_clear_temps:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_delete_ready

.shell_delete_descriptor_cleanup_error:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_delete_descriptor_cleanup_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_delete_descriptor_cleanup_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_delete_descriptor_cleanup_done
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_delete_descriptor_cleanup_done:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_write_unknown

.shell_check_mkdir:
    cmp ecx, 7
    jb .shell_check_write
    cmp byte [edx], 'm'
    jne .shell_check_write
    cmp byte [edx + 1], 'k'
    jne .shell_check_write
    cmp byte [edx + 2], 'd'
    jne .shell_check_write
    cmp byte [edx + 3], 'i'
    jne .shell_check_write
    cmp byte [edx + 4], 'r'
    jne .shell_check_write
    cmp byte [edx + 5], ' '
    jne .shell_check_write
    jmp .shell_resolve_mkdir_descriptor

.shell_mkdir_ready:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 6], '/'
    jne .shell_prepare_mkdir_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_mkdir_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_mkdir_path

.shell_open_mkdir_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_mkdir_path:
    xor eax, eax
.shell_copy_mkdir_path:
    mov esi, ecx
    sub esi, 6
    cmp eax, esi
    jae .shell_launch_mkdir_utility
    mov bl, [edx + eax + 6]
    mov [esp + USER_SHARED_BUFFER_DATA + eax], bl
    inc eax
    jmp .shell_copy_mkdir_path

.shell_launch_mkdir_utility:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_mkdir_descriptor_launch:
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_mkdir_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_mkdir_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_mkdir_descriptor_root

.shell_open_mkdir_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_mkdir_descriptor_cleanup_error
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 1
    jmp .shell_have_mkdir_descriptor_handle

.shell_have_mkdir_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_have_mkdir_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'M'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'K'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'D'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'I'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'R'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 14
    int 0x80
    test eax, eax
    js .shell_mkdir_descriptor_cleanup_error
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_mkdir_descriptor_cleanup_error
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_mkdir_descriptor_exec:
    cmp esi, ecx
    jae .shell_mkdir_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_mkdir_descriptor_ready
    cmp bl, '9'
    ja .shell_mkdir_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_mkdir_descriptor_exec

.shell_mkdir_descriptor_ready:
    cmp eax, 0
    je .shell_mkdir_descriptor_cleanup_error
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_PATH
    jne .shell_mkdir_descriptor_cleanup_error
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_PATH
    jne .shell_mkdir_descriptor_cleanup_error
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_mkdir_descriptor_cleanup_error
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_mkdir_descriptor_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_mkdir_descriptor_release_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_mkdir_descriptor_clear_temps
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_mkdir_descriptor_clear_temps:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_mkdir_ready

.shell_mkdir_descriptor_cleanup_error:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_mkdir_descriptor_cleanup_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_mkdir_descriptor_cleanup_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_mkdir_descriptor_cleanup_done
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_mkdir_descriptor_cleanup_done:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_write_unknown

.shell_launch_utility:
    mov [esp + USER_SHELL_ARG0_LENGTH], esi
    mov edx, [esp + USER_SHELL_LAUNCH_PID]
    test edx, APP_DESCRIPTOR_FLAG_BUFFER
    jz .shell_release_utility_bases_error
    mov [esp + USER_SHELL_SCRIPT_HANDLE], edx
    mov ebx, eax
    mov eax, SYSCALL_USER_LAUNCH_EXECUTABLE
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .shell_release_utility_bases_error
    mov [esp + USER_SHELL_LAUNCH_PID], eax
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov esi, 4

.shell_wait_utility_peer:
    mov eax, SYSCALL_USER_SLEEP_TICKS
    mov ebx, 1
    int 0x80

    mov eax, SYSCALL_USER_LOOKUP_ENDPOINT
    mov ebx, [esp + USER_SHELL_LAUNCH_PID]
    mov ecx, USER_ENDPOINT_ROLE_PEER
    int 0x80
    cmp eax, 0xFFFFFFFF
    jne .shell_delegate_utility_buffer
    dec esi
    jnz .shell_wait_utility_peer
    jmp .shell_release_utility_bases_error

.shell_delegate_utility_buffer:
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    test eax, eax
    jne .shell_revoke_utility_endpoint_error

    mov edx, [esp + USER_SHELL_SCRIPT_HANDLE]
    test edx, APP_DESCRIPTOR_FLAG_BASE
    jz .shell_check_utility_dest_descriptor
    cmp dword [esp + USER_SHELL_BASE_HANDLE], 0
    je .shell_revoke_utility_endpoint
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    test eax, eax
    jne .shell_revoke_utility_endpoint_error

.shell_check_utility_dest_descriptor:
    mov edx, [esp + USER_SHELL_SCRIPT_HANDLE]
    test edx, APP_DESCRIPTOR_FLAG_DEST
    jz .shell_check_utility_console_descriptor
    cmp dword [esp + USER_SHELL_DEST_HANDLE], 0
    je .shell_revoke_utility_endpoint
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_SHELL_DEST_HANDLE]
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    test eax, eax
    jne .shell_revoke_utility_endpoint_error

.shell_check_utility_console_descriptor:
    mov edx, [esp + USER_SHELL_LAUNCH_FLAGS]
    test edx, APP_LAUNCH_FLAG_CONSOLE
    jz .shell_check_utility_input_descriptor
    cmp dword [esp + USER_CONSOLE_HANDLE], 0
    je .shell_check_utility_input_descriptor
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    test eax, eax
    jne .shell_revoke_utility_endpoint_error

.shell_check_utility_input_descriptor:
    mov edx, [esp + USER_SHELL_LAUNCH_FLAGS]
    test edx, APP_LAUNCH_FLAG_INPUT
    jz .shell_revoke_utility_endpoint
    cmp dword [esp + USER_INPUT_HANDLE], 0
    je .shell_revoke_utility_endpoint
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_INPUT_HANDLE]
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    test eax, eax
    jne .shell_revoke_utility_endpoint_error

.shell_revoke_utility_endpoint:

    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    jmp .shell_release_utility_bases

.shell_revoke_utility_endpoint_error:
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    jmp .shell_release_utility_bases_error

.shell_release_utility_bases:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_release_utility_dest_base
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80

.shell_release_utility_dest_base:
    cmp dword [esp + USER_SHELL_DEST_TEMP], 0
    je .shell_clear_utility_bases
    mov eax, [esp + USER_SHELL_DEST_HANDLE]
    cmp eax, [esp + USER_SHELL_BASE_HANDLE]
    je .shell_clear_utility_bases
    mov ebx, eax
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    int 0x80

.shell_clear_utility_bases:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov [esp + USER_SHELL_DEST_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_FLAGS], eax

    mov eax, SYSCALL_USER_WAIT_PROCESS
    mov ebx, [esp + USER_SHELL_LAUNCH_PID]
    int 0x80
    jmp .shell_advance_line

.shell_release_utility_bases_error:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_release_utility_dest_base_error
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80

.shell_release_utility_dest_base_error:
    cmp dword [esp + USER_SHELL_DEST_TEMP], 0
    je .shell_clear_utility_bases_error
    mov eax, [esp + USER_SHELL_DEST_HANDLE]
    cmp eax, [esp + USER_SHELL_BASE_HANDLE]
    je .shell_clear_utility_bases_error
    mov ebx, eax
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    int 0x80

.shell_clear_utility_bases_error:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov [esp + USER_SHELL_DEST_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_FLAGS], eax
    jmp .shell_write_error

.shell_check_write:
    cmp ecx, 8
    jb .shell_check_app
    cmp byte [edx], 'w'
    jne .shell_check_app
    cmp byte [edx + 1], 'r'
    jne .shell_check_app
    cmp byte [edx + 2], 'i'
    jne .shell_check_app
    cmp byte [edx + 3], 't'
    jne .shell_check_app
    cmp byte [edx + 4], 'e'
    jne .shell_check_app
    cmp byte [edx + 5], ' '
    jne .shell_check_app
    jmp .shell_resolve_write_descriptor

.shell_write_ready:
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    cmp byte [edx + 6], '/'
    jne .shell_prepare_write_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_write_root_base
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_prepare_write_path

.shell_open_write_root_base:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

.shell_prepare_write_path:
    mov eax, 6
    xor esi, esi

.shell_copy_write_path:
    cmp eax, ecx
    jae .shell_write_error
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_write_path_done
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_write_path

.shell_write_path_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    inc eax
    cmp eax, ecx
    jae .shell_write_error
    mov [esp + USER_SHELL_ARG1_OFFSET], esi

.shell_copy_write_text:
    cmp eax, ecx
    jae .shell_write_text_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_write_text

.shell_write_text_done:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 10
    inc esi
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_write_descriptor_launch:
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_resolve_write_descriptor:
    xor eax, eax
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    mov [esp + USER_SHELL_LAUNCH_PID], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_open_write_descriptor_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_have_write_descriptor_root

.shell_open_write_descriptor_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_descriptor_cleanup_error
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 1
    jmp .shell_have_write_descriptor_handle

.shell_have_write_descriptor_root:
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax

.shell_have_write_descriptor_handle:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'W'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'R'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 'I'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 8], 'T'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 9], 'E'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 10], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 11], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 12], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 13], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 14], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 14
    int 0x80
    test eax, eax
    js .shell_write_descriptor_cleanup_error
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_write_descriptor_cleanup_error
    mov ecx, eax

    xor eax, eax
    xor esi, esi

.shell_parse_write_descriptor_exec:
    cmp esi, ecx
    jae .shell_write_descriptor_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_write_descriptor_ready
    cmp bl, '9'
    ja .shell_write_descriptor_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_write_descriptor_exec

.shell_write_descriptor_ready:
    cmp eax, 0
    je .shell_write_descriptor_cleanup_error
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_TEXT
    jne .shell_write_descriptor_cleanup_error
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_TEXT
    jne .shell_write_descriptor_cleanup_error
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_write_descriptor_cleanup_error
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx

    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_write_descriptor_release_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_write_descriptor_release_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_write_descriptor_clear_temps
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_write_descriptor_clear_temps:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_write_ready

.shell_write_descriptor_cleanup_error:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_write_descriptor_cleanup_root
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_write_descriptor_cleanup_root:
    cmp dword [esp + USER_SHELL_DESCRIPTOR_TEMP], 0
    je .shell_write_descriptor_cleanup_done
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_ENDPOINT_CAP]
    int 0x80

.shell_write_descriptor_cleanup_done:
    xor eax, eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], eax
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], eax
    jmp .shell_write_unknown

.shell_check_app:
    cmp ecx, 0
    je .shell_write_unknown

    xor eax, eax

.shell_measure_app_name:
    cmp eax, ecx
    jae .shell_app_name_ready
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_app_name_ready
    inc eax
    jmp .shell_measure_app_name

.shell_app_name_ready:
    cmp eax, 0
    je .shell_write_unknown
    mov [esp + USER_SHELL_ARG0_LENGTH], eax
    mov [esp + USER_SHELL_ENDPOINT_CAP], ecx
    xor eax, eax
    mov eax, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    xor eax, eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov [esp + USER_SHELL_DEST_TEMP], eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_app_open_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_app_have_root

.shell_app_open_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    int 0x80
    test eax, eax
    js .shell_write_unknown
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1

.shell_app_have_root:
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]

    mov byte [esp + USER_SHARED_BUFFER_DATA], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'S'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], '/'
    mov esi, 5
    xor eax, eax

.shell_copy_app_descriptor_name:
    cmp eax, [esp + USER_SHELL_ARG0_LENGTH]
    jae .shell_app_descriptor_suffix
    mov bl, [edx + eax]
    cmp bl, 'a'
    jb .shell_app_descriptor_store
    cmp bl, 'z'
    ja .shell_app_descriptor_store
    sub bl, 32

.shell_app_descriptor_store:
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_copy_app_descriptor_name

.shell_app_descriptor_suffix:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], '.'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 1], 'A'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 2], 'P'
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi + 3], 'P'
    add esi, 4
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0

    mov eax, SYSCALL_USER_FS_OPEN
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, esi
    int 0x80
    test eax, eax
    js .shell_app_cleanup_unknown
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    mov eax, SYSCALL_USER_FS_READ
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 32
    int 0x80
    test eax, eax
    js .shell_app_cleanup_unknown
    mov [esp + USER_SHELL_LINE_LENGTH], eax

    xor eax, eax
    xor esi, esi

.shell_parse_app_exec:
    cmp esi, [esp + USER_SHELL_LINE_LENGTH]
    jae .shell_app_exec_ready
    mov bl, [esp + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_app_exec_ready
    cmp bl, '9'
    ja .shell_app_exec_ready
    imul eax, eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc esi
    jmp .shell_parse_app_exec

.shell_app_exec_ready:
    cmp eax, 0
    je .shell_app_cleanup_unknown
    mov [esp + USER_SHELL_SCRIPT_HANDLE], eax
    mov ecx, [esp + USER_SHELL_LINE_LENGTH]
    call .shell_parse_descriptor_flags
    mov [esp + USER_SHELL_LAUNCH_PID], edx
    call .shell_parse_descriptor_policy
    cmp edx, APP_LAUNCH_POLICY_BUFFER
    je .shell_app_policy_buffer
    cmp edx, APP_LAUNCH_POLICY_PATH
    je .shell_app_policy_path
    cmp edx, APP_LAUNCH_POLICY_TEXT
    je .shell_app_policy_text
    cmp edx, APP_LAUNCH_POLICY_RENAME
    je .shell_app_policy_rename
    cmp edx, APP_LAUNCH_POLICY_MOVE
    je .shell_app_policy_move
    jmp .shell_app_cleanup_unknown

.shell_app_policy_buffer:
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_ECHO
    jne .shell_app_cleanup_unknown
    jmp .shell_app_policy_ready

.shell_app_policy_path:
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_PATH
    jne .shell_app_cleanup_unknown
    jmp .shell_app_policy_ready

.shell_app_policy_text:
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_TEXT
    jne .shell_app_cleanup_unknown
    jmp .shell_app_policy_ready

.shell_app_policy_rename:
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_RENAME
    jne .shell_app_cleanup_unknown
    jmp .shell_app_policy_ready

.shell_app_policy_move:
    cmp dword [esp + USER_SHELL_LAUNCH_PID], APP_DESCRIPTOR_MASK_MOVE
    jne .shell_app_cleanup_unknown

.shell_app_policy_ready:
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], edx
    call .shell_parse_descriptor_launch_flags
    call .shell_validate_descriptor_launch_flags
    test eax, eax
    jz .shell_app_cleanup_unknown
    mov [esp + USER_SHELL_LAUNCH_FLAGS], edx
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]

    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_app_dispatch
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax

.shell_app_dispatch:
    mov eax, [esp + USER_SHELL_DESCRIPTOR_TEMP]
    cmp eax, APP_LAUNCH_POLICY_BUFFER
    je .shell_app_args_restore
    cmp eax, APP_LAUNCH_POLICY_PATH
    je .shell_app_prepare_path
    cmp eax, APP_LAUNCH_POLICY_TEXT
    je .shell_app_prepare_text
    cmp eax, APP_LAUNCH_POLICY_RENAME
    je .shell_app_prepare_rename
    cmp eax, APP_LAUNCH_POLICY_MOVE
    je .shell_app_prepare_move
    jmp .shell_write_unknown

.shell_app_args_restore:
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov esi, [esp + USER_SHELL_ARG0_LENGTH]
    cmp esi, ecx
    jae .shell_app_no_args
    cmp byte [edx + esi], ' '
    jne .shell_app_no_args
    inc esi
    xor ebx, ebx
    jmp .shell_copy_app_args

.shell_app_no_args:
    xor ebx, ebx
    jmp .shell_app_args_done

.shell_copy_app_args:
    cmp esi, ecx
    jae .shell_app_args_done
    mov al, [edx + esi]
    mov [esp + USER_SHARED_BUFFER_DATA + ebx], al
    inc ebx
    inc esi
    jmp .shell_copy_app_args

.shell_app_args_done:
    mov byte [esp + USER_SHARED_BUFFER_DATA + ebx], 10
    inc ebx
    mov byte [esp + USER_SHARED_BUFFER_DATA + ebx], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    mov esi, ebx
    jmp .shell_launch_utility

.shell_app_prepare_path:
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    cmp eax, ecx
    jae .shell_write_error
    cmp byte [edx + eax], ' '
    jne .shell_write_error
    inc eax
    mov ebx, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], ebx
    xor ebx, ebx
    mov [esp + USER_SHELL_BASE_TEMP], ebx
    cmp byte [edx + eax], '/'
    jne .shell_app_copy_path_arg
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_app_open_path_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_app_copy_path_arg

.shell_app_open_path_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    mov eax, SYSCALL_USER_FS_OPEN
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    inc eax

.shell_app_copy_path_arg:
    xor esi, esi

.shell_app_copy_path_arg_loop:
    cmp eax, ecx
    jae .shell_app_path_arg_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_app_copy_path_arg_loop

.shell_app_path_arg_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_app_prepare_text:
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    cmp eax, ecx
    jae .shell_write_error
    cmp byte [edx + eax], ' '
    jne .shell_write_error
    inc eax
    mov ebx, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], ebx
    xor ebx, ebx
    mov [esp + USER_SHELL_BASE_TEMP], ebx
    cmp byte [edx + eax], '/'
    jne .shell_app_copy_text_path
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_app_open_text_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_app_copy_text_path

.shell_app_open_text_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    mov eax, SYSCALL_USER_FS_OPEN
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    inc eax

.shell_app_copy_text_path:
    xor esi, esi

.shell_app_copy_text_path_loop:
    cmp eax, ecx
    jae .shell_write_error
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_app_text_path_done
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_app_copy_text_path_loop

.shell_app_text_path_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    inc eax
    cmp eax, ecx
    jae .shell_write_error

.shell_app_copy_text_payload:
    cmp eax, ecx
    jae .shell_app_text_payload_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_app_copy_text_payload

.shell_app_text_payload_done:
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 10
    inc esi
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_app_prepare_rename:
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    cmp eax, ecx
    jae .shell_write_error
    cmp byte [edx + eax], ' '
    jne .shell_write_error
    inc eax
    mov ebx, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], ebx
    xor ebx, ebx
    mov [esp + USER_SHELL_BASE_TEMP], ebx
    cmp byte [edx + eax], '/'
    jne .shell_app_rename_prepare_done
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_app_open_rename_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_app_rename_prepare_done

.shell_app_open_rename_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    mov eax, SYSCALL_USER_FS_OPEN
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    inc eax

.shell_app_rename_prepare_done:
    xor esi, esi

.shell_app_copy_rename_source_arg:
    cmp eax, ecx
    jae .shell_write_error
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_app_rename_source_done
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_app_copy_rename_source_arg

.shell_app_rename_source_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    inc eax
    cmp eax, ecx
    jae .shell_write_error
    mov [esp + USER_SHELL_ARG1_OFFSET], esi

.shell_app_copy_rename_dest_arg:
    cmp eax, ecx
    jae .shell_app_rename_dest_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_app_copy_rename_dest_arg

.shell_app_rename_dest_done:
    mov ebx, esi
    sub ebx, [esp + USER_SHELL_ARG1_OFFSET]
    cmp ebx, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    cmp byte [esp + USER_SHARED_BUFFER_DATA], '/'
    sete al
    movzx eax, al
    mov ebx, [esp + USER_SHELL_ARG1_OFFSET]
    cmp byte [esp + USER_SHARED_BUFFER_DATA + ebx], '/'
    sete bl
    movzx ebx, bl
    cmp eax, ebx
    jne .shell_write_error
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_app_prepare_move:
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    cmp eax, ecx
    jae .shell_write_error
    cmp byte [edx + eax], ' '
    jne .shell_write_error
    inc eax
    mov ebx, [esp + USER_CWD_HANDLE]
    mov [esp + USER_SHELL_BASE_HANDLE], ebx
    mov [esp + USER_SHELL_DEST_HANDLE], ebx
    xor ebx, ebx
    mov [esp + USER_SHELL_BASE_TEMP], ebx
    mov [esp + USER_SHELL_DEST_TEMP], ebx
    cmp byte [edx + eax], '/'
    jne .shell_app_move_prepare_source_done
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_app_open_move_source_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_app_move_prepare_source_done

.shell_app_open_move_source_root:
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    mov eax, SYSCALL_USER_FS_OPEN
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov dword [esp + USER_SHELL_BASE_TEMP], 1
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_ARG0_LENGTH]
    inc eax

.shell_app_move_prepare_source_done:
    xor esi, esi

.shell_app_copy_move_source_arg:
    cmp eax, ecx
    jae .shell_write_error
    mov bl, [edx + eax]
    cmp bl, ' '
    je .shell_app_move_source_done
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_app_copy_move_source_arg

.shell_app_move_source_done:
    cmp esi, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    inc esi
    inc eax
    cmp eax, ecx
    jae .shell_write_error
    mov [esp + USER_SHELL_ARG1_OFFSET], esi
    mov [esp + USER_SHELL_TEMP_HANDLE], eax
    cmp byte [edx + eax], '/'
    jne .shell_app_move_prepare_dest_done
    cmp dword [esp + USER_CWD_PATH_LENGTH], 1
    jne .shell_app_open_move_dest_root
    cmp byte [esp + USER_CWD_PATH_DATA], '/'
    je .shell_app_move_prepare_dest_done

.shell_app_open_move_dest_root:
    movzx ebx, byte [esp + USER_SHARED_BUFFER_DATA]
    mov [esp + USER_SHELL_DESCRIPTOR_TEMP], ebx
    mov byte [esp + USER_SHARED_BUFFER_DATA], '/'
    mov ebx, [esp + USER_RAMFS_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    mov edx, 1
    mov eax, SYSCALL_USER_FS_OPEN
    int 0x80
    test eax, eax
    js .shell_write_error
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov dword [esp + USER_SHELL_DEST_TEMP], 1
    mov ecx, [esp + USER_SHELL_ENDPOINT_CAP]
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    mov eax, [esp + USER_SHELL_TEMP_HANDLE]
    mov ebx, [esp + USER_SHELL_DESCRIPTOR_TEMP]
    mov [esp + USER_SHARED_BUFFER_DATA], bl

.shell_app_move_prepare_dest_done:
    mov eax, [esp + USER_SHELL_TEMP_HANDLE]
    mov esi, [esp + USER_SHELL_ARG1_OFFSET]

.shell_app_copy_move_dest_arg:
    cmp eax, ecx
    jae .shell_app_move_dest_done
    mov bl, [edx + eax]
    mov [esp + USER_SHARED_BUFFER_DATA + esi], bl
    inc esi
    inc eax
    jmp .shell_app_copy_move_dest_arg

.shell_app_move_dest_done:
    mov ebx, esi
    sub ebx, [esp + USER_SHELL_ARG1_OFFSET]
    cmp ebx, 0
    je .shell_write_error
    mov byte [esp + USER_SHARED_BUFFER_DATA + esi], 0
    mov eax, [esp + USER_SHELL_SCRIPT_HANDLE]
    jmp .shell_launch_utility

.shell_app_cleanup_unknown:
    cmp dword [esp + USER_SHELL_TEMP_HANDLE], 0
    je .shell_app_cleanup_base
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_TEMP_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_TEMP_HANDLE], eax

.shell_app_cleanup_base:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_write_unknown
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    jmp .shell_write_unknown

.shell_validate_descriptor_launch_flags:
    cmp edx, APP_LAUNCH_FLAGS_FOREGROUND_CONSOLE_INPUT
    je .shell_validate_descriptor_launch_flags_console_input
    cmp edx, APP_LAUNCH_FLAGS_FOREGROUND_CONSOLE
    je .shell_validate_descriptor_launch_flags_console
    cmp edx, APP_LAUNCH_FLAGS_FOREGROUND_ONLY
    jne .shell_validate_descriptor_launch_flags_fail
    cmp eax, USERSPACE_EXECUTABLE_MKDIR_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_WRITE_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_RENAME_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_APPEND_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_DELETE_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_MOVE_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_TOUCH_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_COPY_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    jmp .shell_validate_descriptor_launch_flags_fail

.shell_validate_descriptor_launch_flags_console:
    cmp eax, USERSPACE_EXECUTABLE_LS_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_CAT_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_STAT_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    cmp eax, USERSPACE_EXECUTABLE_ECHO_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    jmp .shell_validate_descriptor_launch_flags_fail

.shell_validate_descriptor_launch_flags_console_input:
    cmp eax, USERSPACE_EXECUTABLE_ASK_UTILITY
    je .shell_validate_descriptor_launch_flags_ok
    jmp .shell_validate_descriptor_launch_flags_fail

.shell_validate_descriptor_launch_flags_ok:
    mov eax, 1
    ret

.shell_validate_descriptor_launch_flags_fail:
    xor eax, eax
    ret

.shell_parse_descriptor_flags:
.shell_parse_descriptor_policy:
.shell_parse_descriptor_launch_flags:
    xor edx, edx

.shell_seek_descriptor_flags:
    cmp esi, ecx
    jae .shell_parse_descriptor_flags_fail
    mov bl, [esp + 4 + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_skip_descriptor_flag_byte
    cmp bl, '9'
    jbe .shell_parse_descriptor_flag_digits

.shell_skip_descriptor_flag_byte:
    inc esi
    jmp .shell_seek_descriptor_flags

.shell_parse_descriptor_flag_digits:
    xor edx, edx

.shell_parse_descriptor_flag_loop:
    cmp esi, ecx
    jae .shell_parse_descriptor_flags_done
    mov bl, [esp + 4 + USER_SHARED_BUFFER_DATA + esi]
    cmp bl, '0'
    jb .shell_parse_descriptor_flags_done
    cmp bl, '9'
    ja .shell_parse_descriptor_flags_done
    imul edx, edx, 10
    sub bl, '0'
    movzx ebx, bl
    add edx, ebx
    inc esi
    jmp .shell_parse_descriptor_flag_loop

.shell_parse_descriptor_flags_done:
    test edx, edx
    jnz .shell_parse_descriptor_flags_return

.shell_parse_descriptor_flags_fail:
    xor edx, edx

.shell_parse_descriptor_flags_return:
    ret

.shell_info_parse_decimal_line:
    xor edx, edx
    xor ebx, ebx
    xor edi, edi

.shell_info_parse_decimal_digits:
    cmp eax, ecx
    jae .shell_info_parse_decimal_finish
    mov bl, [esi + eax]
    cmp bl, '0'
    jb .shell_info_parse_decimal_finish
    cmp bl, '9'
    ja .shell_info_parse_decimal_finish
    imul edx, edx, 10
    sub bl, '0'
    movzx ebx, bl
    add edx, ebx
    inc edi
    inc eax
    jmp .shell_info_parse_decimal_digits

.shell_info_parse_decimal_finish:
    cmp eax, ecx
    jae .shell_info_parse_decimal_done
    cmp byte [esi + eax], 10
    jne .shell_info_parse_decimal_done
    inc eax

.shell_info_parse_decimal_done:
    mov ebx, edi
    ret

.shell_info_get_string_base:
    call .shell_info_get_string_base_pop

.shell_info_get_string_base_pop:
    pop eax
    add eax, shell_info_label_command - .shell_info_get_string_base_pop
    ret

.shell_info_copy_relative_cstring:
    mov esi, [ebp + USER_SHELL_ENDPOINT_CAP]
    add esi, eax
    jmp .shell_info_copy_cstring

.shell_info_copy_cstring:
    mov al, [esi]
    cmp al, 0
    je .shell_info_copy_cstring_done
    mov [ebp + USER_SHARED_BUFFER_DATA + edi], al
    inc esi
    inc edi
    jmp .shell_info_copy_cstring

.shell_info_copy_cstring_done:
    ret

.shell_info_copy_bytes:
    xor eax, eax

.shell_info_copy_bytes_loop:
    cmp eax, ecx
    jae .shell_info_copy_bytes_done
    mov bl, [esi + eax]
    mov [ebp + USER_SHARED_BUFFER_DATA + edi], bl
    inc eax
    inc edi
    jmp .shell_info_copy_bytes_loop

.shell_info_copy_bytes_done:
    ret

.shell_info_emit_line:
    mov byte [ebp + USER_SHARED_BUFFER_DATA + edi], 10
    inc edi
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [ebp + USER_CONSOLE_HANDLE]
    mov ecx, [ebp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, edi
    int 0x80
    ret

.shell_info_append_word_separator:
    cmp ecx, 0
    je .shell_info_append_word_separator_done
    mov byte [ebp + USER_SHARED_BUFFER_DATA + edi], ' '
    inc edi

.shell_info_append_word_separator_done:
    ret

.shell_info_copy_exec_name:
    cmp eax, USERSPACE_EXECUTABLE_LS_UTILITY
    je .shell_info_exec_name_ls
    cmp eax, USERSPACE_EXECUTABLE_CAT_UTILITY
    je .shell_info_exec_name_cat
    cmp eax, USERSPACE_EXECUTABLE_MKDIR_UTILITY
    je .shell_info_exec_name_mkdir
    cmp eax, USERSPACE_EXECUTABLE_WRITE_UTILITY
    je .shell_info_exec_name_write
    cmp eax, USERSPACE_EXECUTABLE_STAT_UTILITY
    je .shell_info_exec_name_stat
    cmp eax, USERSPACE_EXECUTABLE_RENAME_UTILITY
    je .shell_info_exec_name_rename
    cmp eax, USERSPACE_EXECUTABLE_APPEND_UTILITY
    je .shell_info_exec_name_append
    cmp eax, USERSPACE_EXECUTABLE_DELETE_UTILITY
    je .shell_info_exec_name_delete
    cmp eax, USERSPACE_EXECUTABLE_MOVE_UTILITY
    je .shell_info_exec_name_move
    cmp eax, USERSPACE_EXECUTABLE_ECHO_UTILITY
    je .shell_info_exec_name_echo
    cmp eax, USERSPACE_EXECUTABLE_ASK_UTILITY
    je .shell_info_exec_name_ask
    cmp eax, USERSPACE_EXECUTABLE_TOUCH_UTILITY
    je .shell_info_exec_name_touch
    cmp eax, USERSPACE_EXECUTABLE_COPY_UTILITY
    je .shell_info_exec_name_copy
    mov eax, shell_info_value_unknown - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_ls:
    mov eax, shell_info_exec_ls - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_cat:
    mov eax, shell_info_exec_cat - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_mkdir:
    mov eax, shell_info_exec_mkdir - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_write:
    mov eax, shell_info_exec_write - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_stat:
    mov eax, shell_info_exec_stat - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_rename:
    mov eax, shell_info_exec_rename - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_append:
    mov eax, shell_info_exec_append - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_delete:
    mov eax, shell_info_exec_delete - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_move:
    mov eax, shell_info_exec_move - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_echo:
    mov eax, shell_info_exec_echo - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_ask:
    mov eax, shell_info_exec_ask - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_touch:
    mov eax, shell_info_exec_touch - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_exec_name_copy:
    mov eax, shell_info_exec_copy - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_copy_policy_name:
    cmp eax, APP_LAUNCH_POLICY_BUFFER
    je .shell_info_policy_name_buffer
    cmp eax, APP_LAUNCH_POLICY_PATH
    je .shell_info_policy_name_path
    cmp eax, APP_LAUNCH_POLICY_TEXT
    je .shell_info_policy_name_text
    cmp eax, APP_LAUNCH_POLICY_RENAME
    je .shell_info_policy_name_rename
    cmp eax, APP_LAUNCH_POLICY_MOVE
    je .shell_info_policy_name_move
    mov eax, shell_info_value_unknown - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_policy_name_buffer:
    mov eax, shell_info_policy_buffer - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_policy_name_path:
    mov eax, shell_info_policy_path - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_policy_name_text:
    mov eax, shell_info_policy_text - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_policy_name_rename:
    mov eax, shell_info_policy_rename - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_policy_name_move:
    mov eax, shell_info_policy_move - shell_info_label_command
    jmp .shell_info_copy_relative_cstring

.shell_info_copy_authority_words:
    mov edx, eax
    xor ecx, ecx
    test edx, APP_DESCRIPTOR_FLAG_BUFFER
    jz .shell_info_authority_check_base
    call .shell_info_append_word_separator
    mov eax, shell_info_auth_buffer - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_authority_check_base:
    test edx, APP_DESCRIPTOR_FLAG_BASE
    jz .shell_info_authority_check_dest
    call .shell_info_append_word_separator
    mov eax, shell_info_auth_base - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_authority_check_dest:
    test edx, APP_DESCRIPTOR_FLAG_DEST
    jz .shell_info_authority_check_text
    call .shell_info_append_word_separator
    mov eax, shell_info_auth_dest - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_authority_check_text:
    test edx, APP_DESCRIPTOR_FLAG_TEXT
    jz .shell_info_authority_check_pair
    call .shell_info_append_word_separator
    mov eax, shell_info_auth_text - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_authority_check_pair:
    test edx, APP_DESCRIPTOR_FLAG_PATH_PAIR
    jz .shell_info_authority_finish
    call .shell_info_append_word_separator
    mov eax, shell_info_auth_pair - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_authority_finish:
    cmp ecx, 0
    jne .shell_info_authority_done
    mov eax, shell_info_value_none - shell_info_label_command
    call .shell_info_copy_relative_cstring

.shell_info_authority_done:
    ret

.shell_info_copy_binding_words:
    mov edx, eax
    xor ecx, ecx
    test edx, APP_LAUNCH_FLAG_FOREGROUND
    jz .shell_info_binding_check_console
    call .shell_info_append_word_separator
    mov eax, shell_info_binding_foreground - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_binding_check_console:
    test edx, APP_LAUNCH_FLAG_CONSOLE
    jz .shell_info_binding_check_input
    call .shell_info_append_word_separator
    mov eax, shell_info_binding_console - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_binding_check_input:
    test edx, APP_LAUNCH_FLAG_INPUT
    jz .shell_info_binding_finish
    call .shell_info_append_word_separator
    mov eax, shell_info_binding_input - shell_info_label_command
    call .shell_info_copy_relative_cstring
    inc ecx

.shell_info_binding_finish:
    cmp ecx, 0
    jne .shell_info_binding_done
    mov eax, shell_info_value_none - shell_info_label_command
    call .shell_info_copy_relative_cstring

.shell_info_binding_done:
    ret

.shell_write_unknown:
.shell_emit_unknown:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'u'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'k'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], 'o'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'w'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 'n'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 7], 10
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 8
    int 0x80
    jmp .shell_advance_line

.shell_write_error:
    cmp dword [esp + USER_SHELL_BASE_TEMP], 0
    je .shell_write_error_dest
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_SHELL_BASE_HANDLE]
    int 0x80

.shell_write_error_dest:
    cmp dword [esp + USER_SHELL_DEST_TEMP], 0
    je .shell_write_error_clear
    mov eax, [esp + USER_SHELL_DEST_HANDLE]
    cmp eax, [esp + USER_SHELL_BASE_HANDLE]
    je .shell_write_error_clear
    mov ebx, eax
    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    int 0x80

.shell_write_error_clear:
    xor eax, eax
    mov [esp + USER_SHELL_BASE_HANDLE], eax
    mov [esp + USER_SHELL_BASE_TEMP], eax
    mov [esp + USER_SHELL_DEST_HANDLE], eax
    mov [esp + USER_SHELL_DEST_TEMP], eax
.shell_emit_error:
    mov byte [esp + USER_SHARED_BUFFER_DATA], 'f'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 1], 'a'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 2], 'i'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 3], 'l'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 4], 'e'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 5], 'd'
    mov byte [esp + USER_SHARED_BUFFER_DATA + 6], 10
    mov eax, SYSCALL_USER_CONSOLE_WRITE
    mov ebx, [esp + USER_CONSOLE_HANDLE]
    mov ecx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    xor edx, edx
    mov esi, 7
    int 0x80

.shell_advance_line:
    mov esi, [esp + USER_SHELL_SCRIPT_OFFSET]
    lea edx, [esp + USER_SHELL_SCRIPT_DATA + esi]
    xor ecx, ecx

.shell_measure_advance:
    mov eax, esi
    add eax, ecx
    cmp eax, [esp + USER_SHELL_SCRIPT_LENGTH]
    jae .shell_advance_ready
    mov al, [edx + ecx]
    cmp al, 10
    je .shell_advance_ready
    inc ecx
    jmp .shell_measure_advance

.shell_advance_ready:
    mov eax, [esp + USER_SHELL_SCRIPT_OFFSET]
    add eax, ecx
    cmp eax, [esp + USER_SHELL_SCRIPT_LENGTH]
    jae .shell_store_offset
    inc eax

.shell_store_offset:
    mov [esp + USER_SHELL_SCRIPT_OFFSET], eax
    jmp .shell_next_line

.shell_bootstrap_done:
    mov dword [esp + USER_SHELL_BOOTSTRAP_TESTED], 1

    mov esi, 0x00008000
.delay:
    dec esi
    jne .delay

    inc ebp
    cmp ebp, 1
    jne .maybe_direct

    mov eax, SYSCALL_USER_LOOKUP_ENDPOINT
    mov ebx, edi
    mov ecx, USER_ENDPOINT_ROLE_PEER
    int 0x80
    mov ebx, eax
    cmp ebx, 0xFFFFFFFF
    je .maybe_profile_class

    mov [esp + USER_IPC_TX_BUFFER], edi
    mov [esp + USER_IPC_TX_BUFFER + 4], ebp
    mov eax, SYSCALL_USER_SEND_IPC
    mov ecx, USER_MESSAGE_PING
    lea edx, [esp + USER_IPC_TX_BUFFER]
    mov esi, 2
    int 0x80

.maybe_profile_class:
    cmp dword [esp + USER_POLICY_CLASS_TESTED], 0
    jne .maybe_direct
    mov eax, SYSCALL_USER_LOOKUP_ENDPOINT_CLASS
    cmp edi, 1
    jne .lookup_boot_a_policy_class
    mov ebx, USER_ENDPOINT_CLASS_BOOT_B_POLICY
    jmp .lookup_policy_class

.lookup_boot_a_policy_class:
    mov ebx, USER_ENDPOINT_CLASS_BOOT_A_POLICY

.lookup_policy_class:
    int 0x80
    mov dword [esp + USER_POLICY_CLASS_TESTED], 1

.maybe_direct:
    test ebp, 0x7FF
    jne .maybe_policy

    mov eax, SYSCALL_USER_LOOKUP_ENDPOINT_CLASS
    cmp edi, 1
    jne .lookup_boot_a_peer
    mov ebx, USER_ENDPOINT_CLASS_BOOT_B_PEER
    jmp .lookup_peer_class

.lookup_boot_a_peer:
    mov ebx, USER_ENDPOINT_CLASS_BOOT_A_PEER

.lookup_peer_class:
    int 0x80
    mov ebx, eax
    cmp ebx, 0xFFFFFFFF
    je .maybe_policy

    mov [esp + USER_STALE_CAP_SLOT], ebx
    cmp edi, 1
    jne .send_direct
    cmp dword [esp + USER_NONDELEGATE_TESTED], 0
    jne .send_direct

    mov ecx, ebx
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_STALE_CAP_SLOT]
    int 0x80
    mov dword [esp + USER_NONDELEGATE_TESTED], 1

.send_direct:
    mov [esp + USER_IPC_TX_BUFFER], edi
    mov [esp + USER_IPC_TX_BUFFER + 4], ebp
    mov eax, SYSCALL_USER_SEND_IPC
    mov ecx, USER_MESSAGE_PING
    lea edx, [esp + USER_IPC_TX_BUFFER]
    mov esi, 2
    int 0x80

    cmp edi, 2
    jne .maybe_shared_buffer
    cmp dword [esp + USER_SHARED_BUFFER_LOCAL_CAP], 0
    je .maybe_shared_buffer
    cmp dword [esp + USER_SHARED_BUFFER_DELEGATED], 0
    jne .maybe_shared_buffer

    mov ecx, [esp + USER_STALE_CAP_SLOT]
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_SHARED_BUFFER_LOCAL_CAP]
    int 0x80
    test eax, eax
    jne .maybe_shared_buffer
    mov dword [esp + USER_SHARED_BUFFER_DELEGATED], 1

.maybe_shared_buffer:
    cmp dword [esp + USER_SHARED_BUFFER_REMOTE_CAP], 0
    je .maybe_policy
    cmp dword [esp + USER_SHARED_BUFFER_TESTED], 0
    jne .maybe_stale_shared_buffer

    mov eax, SYSCALL_USER_READ_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    mov esi, 16
    int 0x80
    test eax, eax
    jne .maybe_policy

    mov dword [esp + USER_SHARED_BUFFER_DATA], 0x53484255
    mov dword [esp + USER_SHARED_BUFFER_DATA + 4], 0x00000001
    mov eax, SYSCALL_USER_WRITE_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    mov esi, 8
    int 0x80
    mov dword [esp + USER_SHARED_BUFFER_TESTED], 1

.maybe_stale_shared_buffer:
    cmp dword [esp + USER_SHARED_BUFFER_STALE_TESTED], 0
    jne .maybe_policy
    cmp dword [esp + USER_LAST_TICK], 150
    jb .maybe_policy

    mov eax, SYSCALL_USER_READ_SHARED_BUFFER
    mov ebx, [esp + USER_SHARED_BUFFER_REMOTE_CAP]
    xor ecx, ecx
    lea edx, [esp + USER_SHARED_BUFFER_DATA]
    mov esi, 8
    int 0x80
    test eax, eax
    je .maybe_policy
    mov dword [esp + USER_SHARED_BUFFER_STALE_TESTED], 1
    xor eax, eax
    mov [esp + USER_SHARED_BUFFER_REMOTE_CAP], eax

    cmp dword [esp + USER_REVOKE_DONE], 0
    jne .maybe_policy

    mov eax, SYSCALL_USER_REVOKE_CAPABILITY
    mov ebx, [esp + USER_STALE_CAP_SLOT]
    int 0x80
    test eax, eax
    jne .maybe_policy

    mov dword [esp + USER_REVOKE_DONE], 1
    mov [esp + USER_IPC_TX_BUFFER], edi
    mov [esp + USER_IPC_TX_BUFFER + 4], ebp
    mov eax, SYSCALL_USER_SEND_IPC
    mov ebx, [esp + USER_STALE_CAP_SLOT]
    mov ecx, USER_MESSAGE_PING
    lea edx, [esp + USER_IPC_TX_BUFFER]
    mov esi, 2
    int 0x80

.maybe_policy:
    test ebp, 0x3FF
    jne .maybe_sleep

    mov ebx, [esp + USER_POLICY_HANDLE]
    cmp ebx, 0
    jne .maybe_lookup_ready

    cmp edi, 1
    jne .test_direct_policy_lookup

    mov eax, SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT
    mov ebx, SERVICE_ENDPOINT_CLASS_AI_POLICY
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .maybe_sleep

    mov [esp + USER_POLICY_HANDLE], eax
    jmp .maybe_lookup_ready

.test_direct_policy_lookup:
    cmp dword [esp + USER_POLICY_SERVICE_TESTED], 0
    jne .wait_for_delegated_policy

    mov eax, SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT
    mov ebx, SERVICE_ENDPOINT_CLASS_AI_POLICY
    int 0x80
    mov dword [esp + USER_POLICY_SERVICE_TESTED], 1
    cmp eax, 0xFFFFFFFF
    je .wait_for_delegated_policy

    mov [esp + USER_POLICY_HANDLE], eax
    jmp .maybe_lookup_ready

.wait_for_delegated_policy:
    mov eax, SYSCALL_USER_WAIT_IPC
    lea ebx, [esp + USER_IPC_RX_BUFFER]
    mov ecx, 4
    int 0x80
    jmp .handle_wait

.maybe_lookup_ready:
.maybe_delegate_policy:
    cmp edi, 1
    jne .maybe_cap_admission
    cmp dword [esp + USER_POLICY_DELEGATED], 0
    jne .maybe_cap_admission

    mov eax, SYSCALL_USER_LOOKUP_ENDPOINT_CLASS
    mov ebx, USER_ENDPOINT_CLASS_BOOT_B_PEER
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .maybe_cap_admission

    mov ecx, eax
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_POLICY_HANDLE]
    int 0x80
    test eax, eax
    jne .maybe_cap_admission

    mov dword [esp + USER_POLICY_DELEGATED], 1

.maybe_cap_admission:
    cmp edi, 1
    jne .do_policy_request
    cmp dword [esp + USER_CAP_ADMISSION_TESTED], 0
    jne .do_policy_request
    cmp dword [esp + USER_POLICY_DELEGATED], 0
    je .do_policy_request

    mov eax, SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT
    mov ebx, SERVICE_ENDPOINT_CLASS_INIT
    int 0x80
    mov dword [esp + USER_CAP_ADMISSION_TESTED], 1

.do_policy_request:
    mov ebx, [esp + USER_POLICY_HANDLE]
    mov eax, SYSCALL_USER_POLICY_REQUEST
    mov ecx, edi
    int 0x80
    test eax, eax
    jne .policy_request_failed
    mov dword [esp + USER_POLICY_REQUEST_INFLIGHT], 1

    cmp edi, 1
    je .wait_policy_reply
    cmp dword [esp + USER_POLICY_BACKPRESSURE_TESTED], 0
    jne .wait_policy_reply

    mov dword [esp + USER_POLICY_BACKPRESSURE_TESTED], 1
    mov ebx, [esp + USER_POLICY_HANDLE]
    mov eax, SYSCALL_USER_POLICY_REQUEST
    mov ecx, edi
    int 0x80

.wait_policy_reply:

    mov eax, SYSCALL_USER_WAIT_IPC
    lea ebx, [esp + USER_IPC_RX_BUFFER]
    mov ecx, 4
    int 0x80
    jmp .handle_wait

.policy_request_failed:
    cmp edi, 1
    je .maybe_sleep
    cmp dword [esp + USER_POLICY_RECEIVED], 0
    je .maybe_sleep
    cmp dword [esp + USER_POLICY_EXPIRY_TESTED], 0
    jne .maybe_sleep

    mov dword [esp + USER_POLICY_HANDLE], 0
    mov dword [esp + USER_POLICY_EXPIRY_TESTED], 1
    mov dword [esp + USER_POLICY_REQUEST_INFLIGHT], 0
    jmp .maybe_sleep

.handle_wait:
    cmp eax, USER_MESSAGE_CAP_GRANTED
    jne .check_ping

    mov eax, [esp + USER_IPC_RX_BUFFER + 4]
    cmp eax, USER_CAPABILITY_TYPE_SHARED_BUFFER
    jne .cap_granted_endpoint

    mov eax, [esp + USER_IPC_RX_BUFFER]
    mov [esp + USER_SHARED_BUFFER_REMOTE_CAP], eax
    jmp .maybe_wait_again

.cap_granted_endpoint:
    mov eax, [esp + USER_IPC_RX_BUFFER]
    mov [esp + USER_POLICY_HANDLE], eax
    mov dword [esp + USER_POLICY_RECEIVED], 1
    cmp edi, 1
    je .maybe_wait_again
    cmp dword [esp + USER_POLICY_REDELEGATE_TESTED], 0
    jne .maybe_wait_again

    mov eax, SYSCALL_USER_LOOKUP_ENDPOINT_CLASS
    mov ebx, USER_ENDPOINT_CLASS_BOOT_A_PEER
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .maybe_wait_again

    mov ecx, eax
    mov eax, SYSCALL_USER_DELEGATE_CAPABILITY
    mov ebx, [esp + USER_POLICY_HANDLE]
    int 0x80
    mov dword [esp + USER_POLICY_REDELEGATE_TESTED], 1

.maybe_policy_pressure:
    cmp edi, 1
    je .maybe_wait_again
    cmp dword [esp + USER_POLICY_BACKPRESSURE_TESTED], 0
    jne .maybe_wait_again

    mov ebx, [esp + USER_POLICY_HANDLE]
    mov eax, SYSCALL_USER_POLICY_REQUEST
    mov ecx, edi
    int 0x80
    test eax, eax
    jne .maybe_wait_again

    mov dword [esp + USER_POLICY_REQUEST_INFLIGHT], 1
    mov dword [esp + USER_POLICY_BACKPRESSURE_TESTED], 1
    mov ebx, [esp + USER_POLICY_HANDLE]
    mov eax, SYSCALL_USER_POLICY_REQUEST
    mov ecx, edi
    int 0x80

.maybe_wait_again:
    cmp dword [esp + USER_POLICY_REQUEST_INFLIGHT], 0
    je .loop

    mov eax, SYSCALL_USER_WAIT_IPC
    lea ebx, [esp + USER_IPC_RX_BUFFER]
    mov ecx, 4
    int 0x80
    jmp .handle_wait

.check_ping:
    cmp eax, USER_MESSAGE_PING
    jne .check_policy_approved

    mov eax, SYSCALL_USER_LOOKUP_ENDPOINT_CLASS
    cmp dword [esp + USER_IPC_RX_BUFFER], 1
    jne .reply_boot_b_peer
    mov ebx, USER_ENDPOINT_CLASS_BOOT_A_PEER
    jmp .reply_lookup_done

.reply_boot_b_peer:
    mov ebx, USER_ENDPOINT_CLASS_BOOT_B_PEER

.reply_lookup_done:
    int 0x80
    cmp eax, 0xFFFFFFFF
    je .loop

    mov ebx, eax
    mov eax, [esp + USER_IPC_RX_BUFFER]
    mov [esp + USER_IPC_TX_BUFFER], eax
    mov [esp + USER_IPC_TX_BUFFER + 4], ebp
    mov eax, SYSCALL_USER_SEND_IPC
    mov ecx, USER_MESSAGE_PONG
    lea edx, [esp + USER_IPC_TX_BUFFER]
    mov esi, 2
    int 0x80

    mov eax, SYSCALL_USER_WAIT_IPC
    lea ebx, [esp + USER_IPC_RX_BUFFER]
    mov ecx, 4
    int 0x80
    jmp .handle_wait

.check_policy_approved:
    cmp eax, IPC_MESSAGE_POLICY_APPROVED
    jne .loop

    mov dword [esp + USER_POLICY_REQUEST_INFLIGHT], 0
    jmp .loop

.maybe_sleep:
    cmp edi, 2
    jne .test_sleep
    cmp dword [esp + USER_SHARED_BUFFER_DELEGATED], 0
    je .test_sleep
    cmp dword [esp + USER_POLICY_RECEIVED], 0
    je .test_sleep
    cmp dword [esp + USER_LAST_TICK], 130
    jb .test_sleep

    mov eax, SYSCALL_USER_EXIT
    xor ebx, ebx
    int 0x80

.test_sleep:
    test ebp, 0x1FF
    jne .loop

    lea ebx, [edi + 2]
    mov eax, SYSCALL_USER_SLEEP_TICKS
    int 0x80
    mov ebx, edi

    jmp .loop

section .text

shell_info_label_command db 'command: ', 0
shell_info_label_launch db 'executable: ', 0
shell_info_label_category db 'category: ', 0
shell_info_label_summary db 'usage: ', 0
shell_info_label_authority db 'authority: ', 0
shell_info_label_policy db 'policy: ', 0
shell_info_label_bindings db 'bindings: ', 0

shell_info_exec_ls db 'ls-utility', 0
shell_info_exec_cat db 'cat-utility', 0
shell_info_exec_mkdir db 'mkdir-utility', 0
shell_info_exec_write db 'write-utility', 0
shell_info_exec_stat db 'stat-utility', 0
shell_info_exec_rename db 'rename-utility', 0
shell_info_exec_append db 'append-utility', 0
shell_info_exec_delete db 'delete-utility', 0
shell_info_exec_move db 'move-utility', 0
shell_info_exec_echo db 'echo-utility', 0
shell_info_exec_ask db 'ask-utility', 0
shell_info_exec_touch db 'touch-utility', 0
shell_info_exec_copy db 'copy-utility', 0

shell_info_policy_buffer db 'buffer', 0
shell_info_policy_path db 'path', 0
shell_info_policy_text db 'path+text', 0
shell_info_policy_rename db 'rename-pair', 0
shell_info_policy_move db 'move-pair', 0

shell_info_auth_buffer db 'buffer', 0
shell_info_auth_base db 'base', 0
shell_info_auth_dest db 'dest', 0
shell_info_auth_text db 'text', 0
shell_info_auth_pair db 'pair', 0

shell_info_binding_foreground db 'foreground', 0
shell_info_binding_console db 'console', 0
shell_info_binding_input db 'input', 0

shell_info_value_unknown db 'unknown', 0
shell_info_value_none db 'none', 0

_user_bootstrap_service_end:
