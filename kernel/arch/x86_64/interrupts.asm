BITS 64
default rel

extern interrupts64_dispatch
extern syscall64_native_dispatch

section .text

global usermode64_enter_probe
global usermode64_probe_complete

usermode64_enter_probe:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    lea rax, [rel .resume]
    mov [rel usermode64_probe_resume_rip], rax
    mov [rel usermode64_probe_saved_rsp], rsp
    mov dword [rel usermode64_probe_active], 1
    mov r10, r8
    mov r11, r8
    and r10, 0xFFFF
    shr r11, 16
    and r11, 0xFFFF
    push r11
    push rdx
    push r9
    push r10
    push rcx
    iretq
.resume:
    mov eax, [rel usermode64_probe_exit_result]
    mov dword [rel usermode64_probe_active], 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

usermode64_probe_complete:
    mov [rel usermode64_probe_exit_result], ecx
    mov rsp, [rel usermode64_probe_saved_rsp]
    jmp qword [rel usermode64_probe_resume_rip]

global interrupt64_breakpoint_proof
global interrupt64_breakpoint_proof_resume
interrupt64_breakpoint_proof:
    int3
interrupt64_breakpoint_proof_resume:
    ret

global interrupt64_invalid_opcode_proof
global interrupt64_invalid_opcode_proof_site
global interrupt64_invalid_opcode_proof_resume
interrupt64_invalid_opcode_proof:
interrupt64_invalid_opcode_proof_site:
    ud2
interrupt64_invalid_opcode_proof_resume:
    ret

global interrupt64_page_fault_proof
global interrupt64_page_fault_proof_site
global interrupt64_page_fault_proof_resume
interrupt64_page_fault_proof:
interrupt64_page_fault_proof_site:
    mov eax, dword [abs 0x00400000]
interrupt64_page_fault_proof_resume:
    ret

global syscall64_native_invoke_asm
syscall64_native_invoke_asm:
    push rbx
    push rsi
    mov rax, rcx
    mov rbx, rdx
    mov rdx, r8
    mov rsi, r9
    syscall
    pop rsi
    pop rbx
    ret

global syscall64_native_entry
syscall64_native_entry:
    push r11
    push rcx
    sub rsp, 40
    mov rcx, rax
    mov r8, rdx
    mov r9, rsi
    mov rdx, rbx
    call syscall64_native_dispatch
    add rsp, 40
    pop rcx
    pop r11
    push r11
    popfq
    jmp rcx

%macro ISR64_NO_ERROR 1
global interrupt64_isr%1
interrupt64_isr%1:
    push qword 0
    sub rsp, 8
    mov dword [rsp], %1
    mov dword [rsp + 4], 0
    jmp interrupt64_common_stub
%endmacro

%macro ISR64_WITH_ERROR 1
global interrupt64_isr%1
interrupt64_isr%1:
    sub rsp, 8
    mov dword [rsp], %1
    mov dword [rsp + 4], 0
    jmp interrupt64_common_stub
%endmacro

global interrupt64_probe_stub
interrupt64_probe_stub:
    push qword 0
    sub rsp, 8
    mov dword [rsp], 0x30
    mov dword [rsp + 4], 0
    jmp interrupt64_common_stub

global interrupt64_syscall_stub
interrupt64_syscall_stub:
    push qword 0
    sub rsp, 8
    mov dword [rsp], 0x80
    mov dword [rsp + 4], 0
    jmp interrupt64_common_stub

%macro IRQ64_HANDLER 2
global interrupt64_irq%1
interrupt64_irq%1:
    push qword 0
    sub rsp, 8
    mov dword [rsp], %2
    mov dword [rsp + 4], 0
    jmp interrupt64_common_stub
%endmacro

interrupt64_common_stub:
    cld
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rcx, rsp
    sub rsp, 32
    call interrupts64_dispatch
    add rsp, 32

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16
    iretq

ISR64_NO_ERROR 0
ISR64_NO_ERROR 1
ISR64_NO_ERROR 2
ISR64_NO_ERROR 3
ISR64_NO_ERROR 4
ISR64_NO_ERROR 5
ISR64_NO_ERROR 6
ISR64_NO_ERROR 7
ISR64_WITH_ERROR 8
ISR64_NO_ERROR 9
ISR64_WITH_ERROR 10
ISR64_WITH_ERROR 11
ISR64_WITH_ERROR 12
ISR64_WITH_ERROR 13
ISR64_WITH_ERROR 14
ISR64_NO_ERROR 15
ISR64_NO_ERROR 16
ISR64_WITH_ERROR 17
ISR64_NO_ERROR 18
ISR64_NO_ERROR 19
ISR64_NO_ERROR 20
ISR64_WITH_ERROR 21
ISR64_NO_ERROR 22
ISR64_NO_ERROR 23
ISR64_NO_ERROR 24
ISR64_NO_ERROR 25
ISR64_NO_ERROR 26
ISR64_NO_ERROR 27
ISR64_NO_ERROR 28
ISR64_WITH_ERROR 29
ISR64_WITH_ERROR 30
ISR64_NO_ERROR 31

IRQ64_HANDLER 0, 0x20
IRQ64_HANDLER 1, 0x21
IRQ64_HANDLER 2, 0x22
IRQ64_HANDLER 3, 0x23
IRQ64_HANDLER 4, 0x24
IRQ64_HANDLER 5, 0x25
IRQ64_HANDLER 6, 0x26
IRQ64_HANDLER 7, 0x27
IRQ64_HANDLER 8, 0x28
IRQ64_HANDLER 9, 0x29
IRQ64_HANDLER 10, 0x2A
IRQ64_HANDLER 11, 0x2B
IRQ64_HANDLER 12, 0x2C
IRQ64_HANDLER 13, 0x2D
IRQ64_HANDLER 14, 0x2E
IRQ64_HANDLER 15, 0x2F

section .rdata
global interrupt64_exception_table
interrupt64_exception_table:
    dq interrupt64_isr0, interrupt64_isr1, interrupt64_isr2, interrupt64_isr3
    dq interrupt64_isr4, interrupt64_isr5, interrupt64_isr6, interrupt64_isr7
    dq interrupt64_isr8, interrupt64_isr9, interrupt64_isr10, interrupt64_isr11
    dq interrupt64_isr12, interrupt64_isr13, interrupt64_isr14, interrupt64_isr15
    dq interrupt64_isr16, interrupt64_isr17, interrupt64_isr18, interrupt64_isr19
    dq interrupt64_isr20, interrupt64_isr21, interrupt64_isr22, interrupt64_isr23
    dq interrupt64_isr24, interrupt64_isr25, interrupt64_isr26, interrupt64_isr27
    dq interrupt64_isr28, interrupt64_isr29, interrupt64_isr30, interrupt64_isr31

global interrupt64_irq_table
interrupt64_irq_table:
    dq interrupt64_irq0, interrupt64_irq1, interrupt64_irq2, interrupt64_irq3
    dq interrupt64_irq4, interrupt64_irq5, interrupt64_irq6, interrupt64_irq7
    dq interrupt64_irq8, interrupt64_irq9, interrupt64_irq10, interrupt64_irq11
    dq interrupt64_irq12, interrupt64_irq13, interrupt64_irq14, interrupt64_irq15

section .data
align 8
usermode64_probe_saved_rsp:
    dq 0
usermode64_probe_resume_rip:
    dq 0
usermode64_probe_exit_result:
    dd 0
usermode64_probe_active:
    dd 0
