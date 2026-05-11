[bits 32]

global _start
extern _kernel_main

section .text
_start:
    cli
    mov esp, 0x09F000
    xor ebp, ebp
    push eax
    call _kernel_main
    add esp, 4

.halt:
    hlt
    jmp .halt
