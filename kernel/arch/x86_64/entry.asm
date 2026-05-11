BITS 64
default rel

section .text
global _start
extern __bss_start
extern __bss_end
extern kernel_main64_scaffold

_start:
    cli
    cld
    mov rsi, rdi
    lea rdi, [__bss_start]
    lea rcx, [__bss_end]
    sub rcx, rdi
    xor eax, eax
    rep stosb

    lea rsp, [bootstrap_stack_top]
    and rsp, -16
    xor ebp, ebp

    mov rcx, rsi
    sub rsp, 32
    call kernel_main64_scaffold
    add rsp, 32

.halt:
    hlt
    jmp .halt

section .bss
align 16
bootstrap_stack_bottom:
    resb 16384
bootstrap_stack_top:
