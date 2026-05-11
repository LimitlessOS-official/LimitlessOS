#include "interrupts.h"

#include "input.h"
#include "klog.h"
#include "pic.h"
#include "pit.h"
#include "syscall.h"
#include "userspace.h"
#include "x86.h"

enum
{
    IDT_ENTRY_COUNT = 256,
    IDT_TYPE_INTERRUPT_GATE = 0x8E,
    IDT_TYPE_USER_INTERRUPT_GATE = 0xEE,
    IRQ_VECTOR_BASE = 0x20
};

struct idt_entry
{
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 type_attributes;
    u16 offset_high;
} __attribute__((packed));

extern void (*interrupt_exception_table[])(void);
extern void (*interrupt_irq_table[])(void);
extern void syscall_interrupt_stub(void);
extern void user_yield_interrupt_stub(void);

static struct idt_entry idt[IDT_ENTRY_COUNT];

static const char *const EXCEPTION_NAMES[32] = {
    "Divide-by-zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "Control protection exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor injection exception",
    "VMM communication exception",
    "Security exception",
    "Reserved"
};

static void idt_set_gate(u8 vector, void (*handler)(void), u8 type_attributes)
{
    u32 handler_address = (u32)handler;

    idt[vector].offset_low = (u16)(handler_address & 0xFFFF);
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].type_attributes = type_attributes;
    idt[vector].offset_high = (u16)((handler_address >> 16) & 0xFFFF);
}

static void interrupt_log_exception(const struct interrupt_frame *frame)
{
    klog_write_line("");
    klog_write_string("[fault] vector ");
    klog_write_dec_u32(frame->vector);
    klog_write_string(" ");
    klog_write_line(EXCEPTION_NAMES[frame->vector]);

    klog_write_string("[fault] error code ");
    klog_write_hex_u32(frame->error_code);
    klog_newline();

    klog_write_string("[fault] eip ");
    klog_write_hex_u32(frame->eip);
    klog_write_string(" cs ");
    klog_write_hex_u32(frame->cs);
    klog_write_string(" eflags ");
    klog_write_hex_u32(frame->eflags);
    klog_newline();

    if (frame->vector == 14u)
    {
        klog_write_string("[fault] cr2 ");
        klog_write_hex_u32(read_cr2());
        klog_newline();
    }
}

void interrupts_init(void)
{
    u32 index;

    klog_write_line("[interrupts] zeroing IDT");
    for (index = 0; index < IDT_ENTRY_COUNT; ++index)
    {
        idt[index].offset_low = 0;
        idt[index].selector = 0;
        idt[index].zero = 0;
        idt[index].type_attributes = 0;
        idt[index].offset_high = 0;
    }

    klog_write_line("[interrupts] binding exceptions");
    for (index = 0; index < 32; ++index)
    {
        idt_set_gate((u8)index, interrupt_exception_table[index], IDT_TYPE_INTERRUPT_GATE);
    }

    klog_write_line("[interrupts] binding IRQs");
    for (index = 0; index < 16; ++index)
    {
        idt_set_gate((u8)(IRQ_VECTOR_BASE + index), interrupt_irq_table[index], IDT_TYPE_INTERRUPT_GATE);
    }

    klog_write_line("[interrupts] binding syscall gate");
    idt_set_gate(0x80, syscall_interrupt_stub, IDT_TYPE_USER_INTERRUPT_GATE);
    idt_set_gate(0x81, user_yield_interrupt_stub, IDT_TYPE_USER_INTERRUPT_GATE);
    klog_write_line("[interrupts] loading IDT");
    lidt(idt, sizeof(idt) - 1);
    klog_write_line("[interrupts] remapping PIC");
    pic_initialize(0xFC, 0xFF);
    klog_write_line("[interrupts] PIC ready");
}

void interrupts_enable(void)
{
    cpu_sti();
}

void interrupts_disable(void)
{
    cpu_cli();
}

struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame)
{
    u32 vector = frame->vector;

    if (vector < 32)
    {
        interrupt_log_exception(frame);
        cpu_cli();
        cpu_halt_forever();
    }

    if ((vector >= IRQ_VECTOR_BASE) && (vector < (IRQ_VECTOR_BASE + 16)))
    {
        u8 irq = (u8)(vector - IRQ_VECTOR_BASE);

        if (irq == 0)
        {
            pit_handle_interrupt();
            frame = userspace_handle_timer_tick(frame);
        }
        else if (irq == 1)
        {
            input_handle_keyboard_interrupt();
        }

        pic_send_eoi(irq);
        return frame;
    }

    if (vector == 0x80)
    {
        u32 syscall_number = frame->eax;
        int is_user_frame = ((frame->cs & 0x3u) == 0x3u);

        if (is_user_frame)
        {
            userspace_note_syscall();
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_SLEEP_TICKS))
        {
            frame->eax = 0;
            return userspace_handle_sleep(frame, frame->ebx);
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_YIELD))
        {
            frame->eax = 0;
            return userspace_handle_yield(frame);
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_POLICY_REQUEST))
        {
            frame->eax = userspace_request_policy(frame->ebx, frame->ecx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_WAIT_IPC))
        {
            return userspace_handle_wait_message(frame);
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_SEND_IPC))
        {
            frame->eax = (u32)userspace_send_message(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_REGISTER_ENDPOINT))
        {
            frame->eax = userspace_register_endpoint(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_LOOKUP_ENDPOINT))
        {
            frame->eax = userspace_lookup_endpoint(frame->ebx, frame->ecx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_LOOKUP_ENDPOINT_CLASS))
        {
            frame->eax = userspace_lookup_endpoint_class(frame->ebx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT))
        {
            frame->eax = userspace_lookup_service_endpoint(frame->ebx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_REVOKE_CAPABILITY))
        {
            frame->eax = userspace_revoke_capability(frame->ebx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_DELEGATE_CAPABILITY))
        {
            frame->eax = userspace_delegate_capability(frame->ebx, frame->ecx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_REGISTER_SHARED_BUFFER))
        {
            frame->eax = userspace_register_shared_buffer(frame->ebx, frame->ecx, frame->edx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_READ_SHARED_BUFFER))
        {
            frame->eax = userspace_read_shared_buffer(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_WRITE_SHARED_BUFFER))
        {
            frame->eax = userspace_write_shared_buffer(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_EXIT))
        {
            return userspace_handle_exit(frame, frame->ebx);
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_CONSOLE_WRITE))
        {
            frame->eax = userspace_console_write(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_OPEN))
        {
            frame->eax = userspace_fs_open(frame->ebx, frame->ecx, frame->edx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_LIST))
        {
            frame->eax = userspace_fs_list(frame->ebx, frame->ecx, frame->edx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_READ))
        {
            frame->eax = userspace_fs_read(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_STAT))
        {
            frame->eax = userspace_fs_stat(frame->ebx, frame->ecx, frame->edx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_RENAME))
        {
            frame->eax = userspace_fs_rename(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_DELETE))
        {
            frame->eax = userspace_fs_delete(frame->ebx, frame->ecx, frame->edx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_MOVE))
        {
            frame->eax = userspace_fs_move(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_CREATE))
        {
            frame->eax = userspace_fs_create(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_FS_WRITE))
        {
            frame->eax = userspace_fs_write(frame->ebx, frame->ecx, frame->edx, frame->esi);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_LAUNCH_EXECUTABLE))
        {
            frame->eax = userspace_launch_executable(frame->ebx);
            return frame;
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_INPUT_READ))
        {
            return userspace_handle_input_read(frame);
        }

        if (is_user_frame && (syscall_number == SYSCALL_USER_WAIT_PROCESS))
        {
            return userspace_handle_wait_process(frame, frame->ebx);
        }

        frame->eax = syscall_dispatch(syscall_number, frame->ebx, frame->ecx, frame->edx);
        return frame;
    }

    if (vector == 0x81)
    {
        return userspace_handle_yield(frame);
    }

    return frame;
}
