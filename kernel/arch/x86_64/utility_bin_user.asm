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
%assign SYSCALL_FS_CREATE 249
%assign SYSCALL_FS_READ 251
%assign SYSCALL_FS_WRITE 252
%assign SYSCALL_FS_STAT 253
%assign SYSCALL_FS_REVOKE 254
%assign SYSCALL_FS_RENAME 3468
%assign SYSCALL_FS_MOVE 3469

%assign RAMFS_NODE_DIRECTORY 1
%assign RAMFS_NODE_FILE 2

%assign INVALID_HANDLE 0xFFFFFFFF
%assign DRS_LOAD_ENTRY_RESULT 0x44524C30
%assign DRS_LOAD_ERROR_RESULT 0x44524C45

%assign STACK_BYTES 0x300
%assign PATH_OFF 0x00
%assign IO_OFF 0x80
%assign ROOT_CAP_OFF 0x280
%assign FILE_CAP_OFF 0x284
%assign COUNT_OFF 0x288

%macro pack_owner_len 1
    mov rdx, OWNER_CONSOLE_CLIENT
    shl rdx, 32
    or rdx, %1
%endmacro

%macro pack_owner_pair_lengths 2
    mov rdx, OWNER_CONSOLE_CLIENT
    shl rdx, 32
    or rdx, ((%2 << 16) | %1)
%endmacro

%macro copy_literal 3
    lea rsi, [rel %1]
    lea rdi, [rsp + %3]
    mov ecx, %2
    rep movsb
%endmacro

%macro revoke_cap_slot 1
    mov ebx, [rsp + %1]
    cmp ebx, INVALID_HANDLE
    je %%done
    mov eax, SYSCALL_FS_REVOKE
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
    mov dword [rsp + %1], INVALID_HANDLE
%%done:
%endmacro

%macro open_root 0
    copy_literal root_path, root_path_len, PATH_OFF

    mov eax, SYSCALL_CAP_GRANT_SERVICE
    mov ebx, SERVICE_RAMFS
    mov ecx, RIGHT_SEND_QUERY
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
    cmp eax, INVALID_HANDLE
    je .fail

    mov ebx, eax
    mov eax, SYSCALL_FS_OPEN
    lea rcx, [rsp + PATH_OFF]
    pack_owner_len root_path_len
    int 0x80
    cmp eax, INVALID_HANDLE
    je .fail
    mov [rsp + ROOT_CAP_OFF], eax
%endmacro

%macro open_file 2
    revoke_cap_slot FILE_CAP_OFF
    copy_literal %1, %2, PATH_OFF
    mov ebx, [rsp + ROOT_CAP_OFF]
    mov eax, SYSCALL_FS_OPEN
    lea rcx, [rsp + PATH_OFF]
    pack_owner_len %2
    int 0x80
    cmp eax, INVALID_HANDLE
    je .fail
    mov [rsp + FILE_CAP_OFF], eax
%endmacro

%macro create_node 3
    revoke_cap_slot FILE_CAP_OFF
    copy_literal %1, %2, PATH_OFF
    mov ebx, [rsp + ROOT_CAP_OFF]
    mov eax, SYSCALL_FS_CREATE
    lea rcx, [rsp + PATH_OFF]
    pack_owner_len (%2 | (%3 << 16))
    int 0x80
    cmp eax, INVALID_HANDLE
    je .fail
    mov [rsp + FILE_CAP_OFF], eax
%endmacro

%macro exit_with 2
    mov r10d, %1
    mov r11d, %2
    revoke_cap_slot FILE_CAP_OFF
    revoke_cap_slot ROOT_CAP_OFF
    mov ebx, r10d
    mov ecx, r11d
    add rsp, STACK_BYTES
    mov eax, SYSCALL_USERMODE_PROBE_EXIT
    int 0x80
    jmp $
%endmacro

%ifdef UTILITY_CAT
%assign UTILITY_RESULT 0x44524C43
%elifdef UTILITY_STAT
%assign UTILITY_RESULT 0x44524C53
%elifdef UTILITY_MKDIR
%assign UTILITY_RESULT 0x44524C4D
%elifdef UTILITY_WRITE
%assign UTILITY_RESULT 0x44524C57
%elifdef UTILITY_TOUCH
%assign UTILITY_RESULT 0x44524C54
%elifdef UTILITY_APPEND
%assign UTILITY_RESULT 0x44524C41
%elifdef UTILITY_COPY
%assign UTILITY_RESULT 0x44524C50
%elifdef UTILITY_DELETE
%assign UTILITY_RESULT 0x44524C44
%elifdef UTILITY_RENAME
%assign UTILITY_RESULT 0x44524C52
%elifdef UTILITY_MOVE
%assign UTILITY_RESULT 0x44524C56
%else
%error "Define one UTILITY_* symbol"
%endif

mov eax, DRS_LOAD_ENTRY_RESULT
ret

times 0x10 - ($ - $$) db 0

user_start:
    cld
    sub rsp, STACK_BYTES
    mov dword [rsp + ROOT_CAP_OFF], INVALID_HANDLE
    mov dword [rsp + FILE_CAP_OFF], INVALID_HANDLE
    open_root

%ifdef UTILITY_CAT
    open_file readme_path, readme_path_len
    mov ebx, [rsp + FILE_CAP_OFF]
    mov eax, SYSCALL_FS_READ
    lea rcx, [rsp + IO_OFF]
    pack_owner_len 0x100
    int 0x80
    cmp eax, 0
    je .fail
    cmp eax, INVALID_HANDLE
    je .fail
    exit_with UTILITY_RESULT, eax
%endif

%ifdef UTILITY_STAT
    open_file readme_path, readme_path_len
    mov ebx, [rsp + FILE_CAP_OFF]
    mov eax, SYSCALL_FS_STAT
    lea rcx, [rsp + IO_OFF]
    pack_owner_len 0x100
    int 0x80
    cmp eax, 0
    je .fail
    cmp eax, INVALID_HANDLE
    je .fail
    exit_with UTILITY_RESULT, eax
%endif

%ifdef UTILITY_MKDIR
    create_node testdir_path, testdir_path_len, RAMFS_NODE_DIRECTORY
    exit_with UTILITY_RESULT, 1
%endif

%ifdef UTILITY_WRITE
    create_node test_file_path, test_file_path_len, RAMFS_NODE_FILE
    copy_literal write_text, write_text_len, IO_OFF
    mov ebx, [rsp + FILE_CAP_OFF]
    mov eax, SYSCALL_FS_WRITE
    lea rcx, [rsp + IO_OFF]
    pack_owner_len write_text_len
    int 0x80
    cmp eax, write_text_len
    jne .fail
    exit_with UTILITY_RESULT, eax
%endif

%ifdef UTILITY_TOUCH
    create_node touch_file_path, touch_file_path_len, RAMFS_NODE_FILE
    exit_with UTILITY_RESULT, 1
%endif

%ifdef UTILITY_APPEND
    create_node append_file_path, append_file_path_len, RAMFS_NODE_FILE
    copy_literal append_text, append_text_len, IO_OFF
    mov ebx, [rsp + FILE_CAP_OFF]
    mov eax, SYSCALL_FS_WRITE
    lea rcx, [rsp + IO_OFF]
    pack_owner_len append_text_len
    int 0x80
    cmp eax, append_text_len
    jne .fail
    exit_with UTILITY_RESULT, eax
%endif

%ifdef UTILITY_COPY
    open_file readme_path, readme_path_len
    mov ebx, [rsp + FILE_CAP_OFF]
    mov eax, SYSCALL_FS_READ
    lea rcx, [rsp + IO_OFF]
    pack_owner_len 0x100
    int 0x80
    cmp eax, 0
    je .fail
    cmp eax, INVALID_HANDLE
    je .fail
    mov [rsp + COUNT_OFF], eax

    create_node copy_file_path, copy_file_path_len, RAMFS_NODE_FILE
    mov ebx, [rsp + FILE_CAP_OFF]
    mov eax, SYSCALL_FS_WRITE
    lea rcx, [rsp + IO_OFF]
    mov edx, [rsp + COUNT_OFF]
    mov r8, OWNER_CONSOLE_CLIENT
    shl r8, 32
    or rdx, r8
    int 0x80
    cmp eax, [rsp + COUNT_OFF]
    jne .fail
    exit_with UTILITY_RESULT, eax
%endif

%ifdef UTILITY_DELETE
    mov ebx, [rsp + ROOT_CAP_OFF]
    mov eax, SYSCALL_FS_REVOKE
    mov edx, OWNER_CONSOLE_CLIENT
    int 0x80
    cmp eax, 1
    jne .fail
    exit_with UTILITY_RESULT, 1
%endif

%ifdef UTILITY_RENAME
    create_node rename_source_path, rename_source_path_len, RAMFS_NODE_FILE
    copy_literal rename_source_path, rename_source_path_len, PATH_OFF
    mov byte [rsp + PATH_OFF + rename_source_path_len], 0
    copy_literal rename_dest_path, rename_dest_path_len, PATH_OFF + rename_source_path_len + 1
    mov ebx, [rsp + ROOT_CAP_OFF]
    mov eax, SYSCALL_FS_RENAME
    lea rcx, [rsp + PATH_OFF]
    pack_owner_pair_lengths rename_source_path_len, rename_dest_path_len
    int 0x80
    cmp eax, 1
    jne .fail
    open_file rename_dest_path, rename_dest_path_len
    exit_with UTILITY_RESULT, 1
%endif

%ifdef UTILITY_MOVE
    create_node move_source_path, move_source_path_len, RAMFS_NODE_FILE
    copy_literal move_source_path, move_source_path_len, PATH_OFF
    mov byte [rsp + PATH_OFF + move_source_path_len], 0
    copy_literal move_dest_path, move_dest_path_len, PATH_OFF + move_source_path_len + 1
    mov eax, SYSCALL_FS_MOVE
    mov ebx, [rsp + ROOT_CAP_OFF]
    lea rcx, [rsp + PATH_OFF]
    mov r8d, [rsp + ROOT_CAP_OFF]
    shl r8, 32
    or rcx, r8
    pack_owner_pair_lengths move_source_path_len, move_dest_path_len
    int 0x80
    cmp eax, 1
    jne .fail
    open_file move_dest_path, move_dest_path_len
    exit_with UTILITY_RESULT, 1
%endif

.fail:
    exit_with DRS_LOAD_ERROR_RESULT, 0

root_path:
db "/"
root_path_len equ $ - root_path

readme_path:
db "README.TXT"
readme_path_len equ $ - readme_path

testdir_path:
db "TESTDIR"
testdir_path_len equ $ - testdir_path

test_file_path:
db "TEST.TXT"
test_file_path_len equ $ - test_file_path

touch_file_path:
db "TOUCH.TXT"
touch_file_path_len equ $ - touch_file_path

append_file_path:
db "APPEND.TXT"
append_file_path_len equ $ - append_file_path

copy_file_path:
db "COPY.TXT"
copy_file_path_len equ $ - copy_file_path

rename_source_path:
db "RENAME-SRC.TXT"
rename_source_path_len equ $ - rename_source_path

rename_dest_path:
db "RENAMED.TXT"
rename_dest_path_len equ $ - rename_dest_path

move_source_path:
db "MOVE-SRC.TXT"
move_source_path_len equ $ - move_source_path

move_dest_path:
db "MOVED.TXT"
move_dest_path_len equ $ - move_dest_path

write_text:
db "disk write ok", 10
write_text_len equ $ - write_text

append_text:
db "disk append ok", 10
append_text_len equ $ - append_text
