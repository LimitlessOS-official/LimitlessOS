bits 64
org 0

%assign OWNER_CONSOLE_CLIENT 0x00000201

%assign RIGHT_SEND 0x00000001
%assign RIGHT_QUERY 0x00000004
%assign RIGHT_SEND_QUERY (RIGHT_SEND | RIGHT_QUERY)

%assign SERVICE_CONSOLE 4
%assign SERVICE_RAMFS 5
%assign SERVICE_BLOCK 8
%assign SERVICE_NETWORK 10

%assign SYSCALL_CAP_GRANT_SERVICE 31
%assign SYSCALL_USERMODE_PROBE_EXIT 243
%assign SYSCALL_CONSOLE_WRITE 267
%assign SYSCALL_NET_SOCKET_OPEN_TCP 3471
%assign SYSCALL_NET_SOCKET_RECV_STATUS 3472
%assign SYSCALL_NET_SOCKET_SEND 3473
%assign SYSCALL_NET_SOCKET_CLOSE 3474

%assign INVALID_HANDLE 0xFFFFFFFF
%assign NETHELLO_ENTRY_RESULT 0x4E484530
%assign NETHELLO_RESULT 0x4E484531
%assign NETHELLO_ERROR_RESULT 0x4E484545

%assign STACK_BYTES 0x100
%assign MESSAGE_OFF 0x00
%assign SEND_BYTES 18

%macro copy_literal 3
    lea rsi, [rel %1]
    lea rdi, [rsp + %3]
    mov ecx, %2
    rep movsb
%endmacro

%macro exit_with 2
    mov ebx, %1
    mov ecx, %2
    add rsp, STACK_BYTES
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $
%endmacro

%macro grant_service 1
    mov eax, SYSCALL_CAP_GRANT_SERVICE
    mov ebx, %1
    mov ecx, RIGHT_SEND_QUERY
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
%endmacro

mov eax, NETHELLO_ENTRY_RESULT
ret

times 0x10 - ($ - $$) db 0

user_start:
    cld
    sub rsp, STACK_BYTES

    grant_service SERVICE_CONSOLE
    cmp eax, INVALID_HANDLE
    je .fail
    mov r8d, eax

    copy_literal hello_message, hello_message_len, MESSAGE_OFF
    mov eax, SYSCALL_CONSOLE_WRITE
    mov ebx, r8d
    lea rcx, [rsp + MESSAGE_OFF]
    mov rdx, OWNER_CONSOLE_CLIENT
    shl rdx, 32
    or rdx, hello_message_len
    int 0x80
    cmp eax, hello_message_len
    jne .fail

    grant_service SERVICE_RAMFS
    cmp eax, INVALID_HANDLE
    jne .fail

    grant_service SERVICE_BLOCK
    cmp eax, INVALID_HANDLE
    jne .fail

    grant_service SERVICE_NETWORK
    cmp eax, INVALID_HANDLE
    je .fail
    mov r9d, eax

    mov eax, SYSCALL_NET_SOCKET_OPEN_TCP
    mov ebx, r9d
    xor ecx, ecx
    mov rdx, OWNER_CONSOLE_CLIENT
    shl rdx, 32
    or rdx, 80
    int 0x80
    cmp eax, INVALID_HANDLE
    je .fail
    mov r10d, eax

    mov eax, SYSCALL_NET_SOCKET_RECV_STATUS
    mov ebx, r10d
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
    cmp eax, 0
    je .fail
    mov r11d, eax

    mov eax, SYSCALL_NET_SOCKET_SEND
    mov ebx, r10d
    xor ecx, ecx
    mov rdx, OWNER_CONSOLE_CLIENT
    shl rdx, 32
    or rdx, SEND_BYTES
    int 0x80
    cmp eax, 0
    jne .fail

    mov eax, SYSCALL_NET_SOCKET_CLOSE
    mov ebx, r10d
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
    cmp eax, 1
    jne .fail

    exit_with NETHELLO_RESULT, r11d

.fail:
    exit_with NETHELLO_ERROR_RESULT, 0

hello_message:
    db 'hello from user app', 10
hello_message_len equ $ - hello_message
