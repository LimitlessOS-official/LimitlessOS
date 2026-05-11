bits 64
org 0

%assign OWNER_CONSOLE_CLIENT 0x00000201

%assign RIGHT_SEND 0x00000001
%assign RIGHT_QUERY 0x00000004
%assign RIGHT_SEND_QUERY (RIGHT_SEND | RIGHT_QUERY)

%assign SERVICE_RAMFS 5

%assign SYSCALL_CAP_GRANT_SERVICE 31
%assign SYSCALL_USERMODE_PROBE_EXIT 243
%assign SYSCALL_FS_OPEN 248
%assign SYSCALL_FS_LIST 250

%assign INVALID_HANDLE 0xFFFFFFFF
%assign DRS_LOAD_ENTRY_RESULT 0x44524C30
%assign DRS_LOAD_LS_RESULT 0x44524C31
%assign DRS_LOAD_ERROR_RESULT 0x44524C45

mov eax, DRS_LOAD_ENTRY_RESULT
ret

times 0x10 - ($ - $$) db 0

user_start:
    sub rsp, 0x120
    mov byte [rsp], '/'

    mov eax, SYSCALL_CAP_GRANT_SERVICE
    mov ebx, SERVICE_RAMFS
    mov ecx, RIGHT_SEND_QUERY
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
    cmp eax, INVALID_HANDLE
    je .fail

    mov ebx, eax
    mov eax, SYSCALL_FS_OPEN
    mov rcx, rsp
    mov rdx, OWNER_CONSOLE_CLIENT
    shl rdx, 32
    or rdx, 1
    int 0x80
    cmp eax, INVALID_HANDLE
    je .fail

    mov ebx, eax
    mov eax, SYSCALL_FS_LIST
    lea rcx, [rsp + 0x20]
    mov rdx, OWNER_CONSOLE_CLIENT
    shl rdx, 32
    or rdx, 0x100
    int 0x80
    cmp eax, 0
    je .fail
    cmp eax, INVALID_HANDLE
    je .fail

    mov ecx, eax
    mov ebx, DRS_LOAD_LS_RESULT
    add rsp, 0x120
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $

.fail:
    xor ecx, ecx
    mov ebx, DRS_LOAD_ERROR_RESULT
    add rsp, 0x120
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $
