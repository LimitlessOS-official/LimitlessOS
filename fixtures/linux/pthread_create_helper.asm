bits 64
org 0x1880

clear_tid_words equ 0x21e0
clear_tid_count equ 16

start:
    test rdi, rdi
    jnz .thread_ok
    mov eax, 22
    ret

.thread_ok:
    test rdx, rdx
    jnz .start_ok
    mov eax, 22
    ret

.start_ok:
    test rsi, rsi
    jz .attr_ok
    mov eax, 38
    ret

.attr_ok:
    push r12
    sub rsp, 0x20
    mov [rsp], rdi
    mov [rsp + 8], rdx
    mov [rsp + 16], rcx

    call .rip
.rip:
    pop r12
    add r12, clear_tid_words - .rip
    mov ecx, clear_tid_count

.scan:
    cmp dword [r12], 0
    je .slot_found
    add r12, 4
    dec ecx
    jnz .scan
    mov eax, 11
    add rsp, 0x20
    pop r12
    ret

.slot_found:
    mov dword [r12], 1

    xor edi, edi
    mov esi, 0x4000
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
    mov eax, 9
    syscall
    test rax, rax
    js .mmap_error

    lea rsi, [rax + 0x3fc0]
    lea rdi, [rel child_entry]
    mov [rsi + 0x20], rdi
    mov rdi, [rsp + 8]
    mov [rsi + 0x28], rdi
    mov rdi, [rsp + 16]
    mov [rsi + 0x30], rdi

    mov edi, 0x01310f00
    mov rdx, [rsp]
    mov r10, r12
    xor r8d, r8d
    xor r9d, r9d
    mov eax, 56
    syscall
    test rax, rax
    js .clone_error
    jne .parent_success

    add rsp, 0x20
    xor eax, eax
    ret

.parent_success:
    add rsp, 0x20
    pop r12
    xor eax, eax
    ret

.clone_error:
    mov dword [r12], 0
    neg eax
    add rsp, 0x20
    pop r12
    ret

.mmap_error:
    mov dword [r12], 0
    neg eax
    add rsp, 0x20
    pop r12
    ret

child_entry:
    mov rax, [rsp]
    mov rdi, [rsp + 8]
    call rax
    xor edi, edi
    mov eax, 60
    syscall
    hlt
