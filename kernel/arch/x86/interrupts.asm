[bits 32]

extern _interrupt_dispatch
extern _userspace_handle_yield

section .text

%macro ISR_NO_ERROR 1
global _isr%1
_isr%1:
    push dword 0
    push dword %1
    jmp interrupt_common_stub
%endmacro

%macro ISR_WITH_ERROR 1
global _isr%1
_isr%1:
    push dword %1
    jmp interrupt_common_stub
%endmacro

%macro IRQ_HANDLER 2
global _irq%1
_irq%1:
    push dword 0
    push dword %2
    jmp interrupt_common_stub
%endmacro

global _syscall_interrupt_stub
_syscall_interrupt_stub:
    push dword 0
    push dword 0x80
    jmp interrupt_common_stub

global _user_yield_interrupt_stub
_user_yield_interrupt_stub:
    push dword 0
    push dword 0x81
    jmp interrupt_common_stub

interrupt_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call _interrupt_dispatch
    add esp, 4
    mov esp, eax

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iretd

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_WITH_ERROR 8
ISR_NO_ERROR 9
ISR_WITH_ERROR 10
ISR_WITH_ERROR 11
ISR_WITH_ERROR 12
ISR_WITH_ERROR 13
ISR_WITH_ERROR 14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_WITH_ERROR 17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_WITH_ERROR 21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_WITH_ERROR 29
ISR_WITH_ERROR 30
ISR_NO_ERROR 31

IRQ_HANDLER 0, 0x20
IRQ_HANDLER 1, 0x21
IRQ_HANDLER 2, 0x22
IRQ_HANDLER 3, 0x23
IRQ_HANDLER 4, 0x24
IRQ_HANDLER 5, 0x25
IRQ_HANDLER 6, 0x26
IRQ_HANDLER 7, 0x27
IRQ_HANDLER 8, 0x28
IRQ_HANDLER 9, 0x29
IRQ_HANDLER 10, 0x2A
IRQ_HANDLER 11, 0x2B
IRQ_HANDLER 12, 0x2C
IRQ_HANDLER 13, 0x2D
IRQ_HANDLER 14, 0x2E
IRQ_HANDLER 15, 0x2F

global _interrupt_exception_table
_interrupt_exception_table:
    dd _isr0, _isr1, _isr2, _isr3
    dd _isr4, _isr5, _isr6, _isr7
    dd _isr8, _isr9, _isr10, _isr11
    dd _isr12, _isr13, _isr14, _isr15
    dd _isr16, _isr17, _isr18, _isr19
    dd _isr20, _isr21, _isr22, _isr23
    dd _isr24, _isr25, _isr26, _isr27
    dd _isr28, _isr29, _isr30, _isr31

global _interrupt_irq_table
_interrupt_irq_table:
    dd _irq0, _irq1, _irq2, _irq3
    dd _irq4, _irq5, _irq6, _irq7
    dd _irq8, _irq9, _irq10, _irq11
    dd _irq12, _irq13, _irq14, _irq15
