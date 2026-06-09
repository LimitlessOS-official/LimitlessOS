#include "interrupts_x64.h"

#include "apic_x64.h"
#include "descriptors_x64.h"
#include "input_x64.h"
#ifdef LIMITLESS_X64_UEFI_KERNEL
#include "linux_abi_x64.h"
#include "pe64_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "windows_seh_x64.h"
#endif
#include "pic.h"
#include "pit.h"
#include "process_x64.h"
#include "scheduler_x64.h"
#include "syscall_x64.h"
#include "x64.h"

enum
{
    IDT_ENTRY_COUNT = 256,
    IDT_TYPE_INTERRUPT_GATE = 0x8Eu,
    IDT_TYPE_USER_INTERRUPT_GATE = 0xEEu,
    IRQ_VECTOR_BASE = 0x20u,
    X64_PROBE_VECTOR = 0x30u,
    X64_SYSCALL_VECTOR = 0x80u,
    X64_HIGH_HALF_BASE = 0xFFFFFFFF80000000ull,
    X64_USER_PROBE_BASIC = 1u,
    X64_USER_PROBE_PREEMPT = 2u,
    X64_USER_PROBE_SWITCH = 3u,
    X64_USER_PROBE_RUNQUEUE = 4u
};

struct idt_entry64
{
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 type_attributes;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} __attribute__((packed));

extern void (*interrupt64_exception_table[])(void);
extern void (*interrupt64_irq_table[])(void);
extern void interrupt64_probe_stub(void);
extern void interrupt64_syscall_stub(void);
extern void interrupt64_breakpoint_proof(void);
extern void interrupt64_invalid_opcode_proof(void);
extern void interrupt64_page_fault_proof(void);
extern u32 usermode64_enter_probe(u64 rip, u64 rsp, u64 selectors, u64 rflags);
extern u32 usermode64_enter_probe_args(
    u64 rip,
    u64 rsp,
    u64 selectors,
    u64 rflags,
    u64 arg_rcx,
    u64 arg_rdx,
    u64 arg_r8);
extern void usermode64_probe_complete(u64 result);
extern u8 interrupt64_breakpoint_proof_resume;
extern u8 interrupt64_invalid_opcode_proof_site;
extern u8 interrupt64_invalid_opcode_proof_resume;
extern u8 interrupt64_page_fault_proof_site;
extern u8 interrupt64_page_fault_proof_resume;

static struct idt_entry64 g_idt[IDT_ENTRY_COUNT];
static volatile u32 g_exception_count = 0u;
static volatile u32 g_breakpoint_count = 0u;
static volatile u32 g_invalid_opcode_count = 0u;
static volatile u32 g_page_fault_count = 0u;
static volatile u32 g_probe_count = 0u;
static volatile u32 g_irq_count = 0u;
static volatile u32 g_syscall_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static volatile u32 g_idt_high_targets = 0u;
static volatile u32 g_idt_high_base = 0u;
#endif
static volatile u64 g_last_syscall_code = 0u;
static volatile u64 g_last_exception_vector = 0u;
static volatile u64 g_last_exception_error = 0u;
static volatile u64 g_last_exception_rip = 0u;
static volatile u64 g_last_exception_cr2 = 0u;
static volatile u32 g_user_entry_probe_attempts = 0u;
static volatile u32 g_user_entry_probe_exits = 0u;
static volatile u32 g_user_entry_probe_result = 0u;
static volatile u32 g_user_entry_probe_aux = 0u;
static volatile u64 g_user_entry_probe_rip = 0u;
static volatile u64 g_user_entry_probe_rsp = 0u;
static volatile u64 g_user_entry_probe_cs = 0u;
static volatile u64 g_user_entry_probe_ss = 0u;
static volatile u32 g_user_entry_probe_mode = 0u;
static volatile u32 g_user_preempt_probe_attempts = 0u;
static volatile u32 g_user_preempt_probe_exits = 0u;
static volatile u32 g_user_preempt_probe_result = 0u;
static volatile u32 g_user_preempt_probe_irqs = 0u;
static volatile u64 g_user_preempt_probe_rip = 0u;
static volatile u64 g_user_preempt_probe_rsp = 0u;
static volatile u64 g_user_preempt_probe_cs = 0u;
static volatile u64 g_user_preempt_probe_ss = 0u;
static volatile u32 g_user_switch_probe_attempts = 0u;
static volatile u32 g_user_switch_probe_exits = 0u;
static volatile u32 g_user_switch_probe_result = 0u;
static volatile u32 g_user_switch_probe_irqs = 0u;
static volatile u32 g_user_switch_probe_switches = 0u;
static volatile u64 g_user_switch_source_rip = 0u;
static volatile u64 g_user_switch_source_rsp = 0u;
static volatile u64 g_user_switch_target_rip = 0u;
static volatile u64 g_user_switch_target_rsp = 0u;
static volatile u64 g_user_switch_probe_cs = 0u;
static volatile u64 g_user_switch_probe_ss = 0u;
static volatile u64 g_user_switch_pending_target_rip = 0u;
static volatile u64 g_user_switch_pending_target_rsp = 0u;
static volatile u64 g_user_switch_pending_rflags = 0u;
static volatile u64 g_user_switch_pending_selectors = 0u;

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u64 interrupt64_high_half_alias(u64 low_address);
#endif

static void debug_write_char(char character)
{
    outb(0x00E9u, (u8)character);
}

static void debug_write_string(const char *text)
{
    while (*text != '\0')
    {
        debug_write_char(*text);
        ++text;
    }
}

static void debug_write_line(const char *text)
{
    debug_write_string(text);
    debug_write_char('\n');
}

static void interrupt64_zero_user_registers(struct interrupt_frame64 *frame)
{
    frame->rax = 0u;
    frame->rbx = 0u;
    frame->rcx = 0u;
    frame->rdx = 0u;
    frame->rsi = 0u;
    frame->rdi = 0u;
    frame->rbp = 0u;
    frame->r8 = 0u;
    frame->r9 = 0u;
    frame->r10 = 0u;
    frame->r11 = 0u;
    frame->r12 = 0u;
    frame->r13 = 0u;
    frame->r14 = 0u;
    frame->r15 = 0u;
}

static void debug_write_hex_digit(u8 value)
{
    if (value < 10u)
    {
        debug_write_char((char)('0' + value));
        return;
    }

    debug_write_char((char)('A' + (value - 10u)));
}

static void debug_write_hex_u64(u64 value)
{
    s32 shift;

    debug_write_string("0x");
    for (shift = 60; shift >= 0; shift -= 4)
    {
        debug_write_hex_digit((u8)((value >> shift) & 0x0Fu));
    }
}

static void debug_write_dec_u64(u64 value)
{
    char digits[20];
    u32 count = 0u;

    if (value == 0u)
    {
        debug_write_char('0');
        return;
    }

    while ((value > 0u) && (count < 20u))
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count > 0u)
    {
        --count;
        debug_write_char(digits[count]);
    }
}

static void idt64_set_gate(u8 vector, void (*handler)(void), u8 type_attributes)
{
    u64 address = (u64)handler;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    address = interrupt64_high_half_alias(address);
    if (address >= X64_HIGH_HALF_BASE)
    {
        g_idt_high_targets = 1u;
    }
#endif

    g_idt[vector].offset_low = (u16)(address & 0xFFFFu);
    g_idt[vector].selector = descriptors64_kernel_code_selector();
    g_idt[vector].ist = 0u;
    g_idt[vector].type_attributes = type_attributes;
    g_idt[vector].offset_mid = (u16)((address >> 16) & 0xFFFFu);
    g_idt[vector].offset_high = (u32)((address >> 32) & 0xFFFFFFFFu);
    g_idt[vector].zero = 0u;
}

static void interrupt64_log_exception(const struct interrupt_frame64 *frame)
{
    const char *exception_name = EXCEPTION_NAMES[frame->vector];

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    exception_name = (const char *)interrupt64_high_half_alias((u64)exception_name);
#endif

    debug_write_line("");
    debug_write_string("[x64-fault] vector ");
    debug_write_dec_u64(frame->vector);
    debug_write_string(" ");
    debug_write_line(exception_name);
    debug_write_string("[x64-fault] error ");
    debug_write_hex_u64(frame->error_code);
    debug_write_string(" rip ");
    debug_write_hex_u64(frame->rip);
    debug_write_string(" cs ");
    debug_write_hex_u64(frame->cs);
    debug_write_string(" rflags ");
    debug_write_hex_u64(frame->rflags);
    debug_write_char('\n');

    if (frame->vector == 14u)
    {
        debug_write_string("[x64-fault] cr2 ");
        debug_write_hex_u64(read_cr2_64());
        debug_write_string(" cr3 ");
        debug_write_hex_u64(read_cr3_64());
        debug_write_char('\n');
    }
}

static void interrupt64_record_exception(const struct interrupt_frame64 *frame)
{
    ++g_exception_count;
    g_last_exception_vector = frame->vector;
    g_last_exception_error = frame->error_code;
    g_last_exception_rip = frame->rip;
    g_last_exception_cr2 = (frame->vector == 14u) ? read_cr2_64() : 0ull;

    if (frame->vector == 3u)
    {
        ++g_breakpoint_count;
    }
    else if (frame->vector == 6u)
    {
        ++g_invalid_opcode_count;
    }
    else if (frame->vector == 14u)
    {
        ++g_page_fault_count;
    }
}

static u64 interrupt64_high_half_alias(u64 low_address)
{
    if (low_address >= X64_HIGH_HALF_BASE)
    {
        return low_address;
    }

    return X64_HIGH_HALF_BASE + low_address;
}

static int interrupt64_matches_site(u64 rip, const void *site)
{
    u64 low_site = (u64)(const void *)site;
    u64 high_site = interrupt64_high_half_alias(low_site);

    return (rip == low_site) || (rip == high_site);
}

static u64 interrupt64_resume_address(u64 rip, const void *site)
{
    u64 low_site = (u64)(const void *)site;

    if (rip >= X64_HIGH_HALF_BASE)
    {
        return interrupt64_high_half_alias(low_site);
    }

    return low_site;
}

static int interrupt64_try_recover_exception(struct interrupt_frame64 *frame)
{
    if ((frame->vector == 3u) &&
        interrupt64_matches_site(frame->rip, (const void *)&interrupt64_breakpoint_proof_resume))
    {
        return 1;
    }

    if ((frame->vector == 6u) &&
        interrupt64_matches_site(frame->rip, (const void *)&interrupt64_invalid_opcode_proof_site))
    {
        frame->rip = interrupt64_resume_address(
            frame->rip,
            (const void *)&interrupt64_invalid_opcode_proof_resume);
        return 1;
    }

    if ((frame->vector == 14u) &&
        interrupt64_matches_site(frame->rip, (const void *)&interrupt64_page_fault_proof_site))
    {
        frame->rip = interrupt64_resume_address(
            frame->rip,
            (const void *)&interrupt64_page_fault_proof_resume);
        frame->rax = 0u;
        return 1;
    }

    return 0;
}

void interrupts64_init(void)
{
    u32 index;

    debug_write_line("[x64] zeroing IDT");
    for (index = 0u; index < IDT_ENTRY_COUNT; ++index)
    {
        g_idt[index].offset_low = 0u;
        g_idt[index].selector = 0u;
        g_idt[index].ist = 0u;
        g_idt[index].type_attributes = 0u;
        g_idt[index].offset_mid = 0u;
        g_idt[index].offset_high = 0u;
        g_idt[index].zero = 0u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_idt_high_targets = 0u;
    g_idt_high_base = 0u;
#endif

    debug_write_line("[x64] binding exceptions");
    for (index = 0u; index < 32u; ++index)
    {
        idt64_set_gate((u8)index, interrupt64_exception_table[index], IDT_TYPE_INTERRUPT_GATE);
    }

    debug_write_line("[x64] binding IRQs");
    for (index = 0u; index < 16u; ++index)
    {
        idt64_set_gate((u8)(IRQ_VECTOR_BASE + index), interrupt64_irq_table[index], IDT_TYPE_INTERRUPT_GATE);
    }

    debug_write_line("[x64] binding proof gates");
    idt64_set_gate(X64_PROBE_VECTOR, interrupt64_probe_stub, IDT_TYPE_INTERRUPT_GATE);
    idt64_set_gate(X64_SYSCALL_VECTOR, interrupt64_syscall_stub, IDT_TYPE_USER_INTERRUPT_GATE);

    g_exception_count = 0u;
    g_breakpoint_count = 0u;
    g_invalid_opcode_count = 0u;
    g_page_fault_count = 0u;
    g_probe_count = 0u;
    g_irq_count = 0u;
    g_syscall_count = 0u;
    g_last_syscall_code = 0u;
    g_last_exception_vector = 0u;
    g_last_exception_error = 0u;
    g_last_exception_rip = 0u;
    g_last_exception_cr2 = 0u;
    g_user_entry_probe_attempts = 0u;
    g_user_entry_probe_exits = 0u;
    g_user_entry_probe_result = 0u;
    g_user_entry_probe_aux = 0u;
    g_user_entry_probe_rip = 0u;
    g_user_entry_probe_rsp = 0u;
    g_user_entry_probe_cs = 0u;
    g_user_entry_probe_ss = 0u;
    g_user_entry_probe_mode = 0u;
    g_user_preempt_probe_attempts = 0u;
    g_user_preempt_probe_exits = 0u;
    g_user_preempt_probe_result = 0u;
    g_user_preempt_probe_irqs = 0u;
    g_user_preempt_probe_rip = 0u;
    g_user_preempt_probe_rsp = 0u;
    g_user_preempt_probe_cs = 0u;
    g_user_preempt_probe_ss = 0u;
    g_user_switch_probe_attempts = 0u;
    g_user_switch_probe_exits = 0u;
    g_user_switch_probe_result = 0u;
    g_user_switch_probe_irqs = 0u;
    g_user_switch_probe_switches = 0u;
    g_user_switch_source_rip = 0u;
    g_user_switch_source_rsp = 0u;
    g_user_switch_target_rip = 0u;
    g_user_switch_target_rsp = 0u;
    g_user_switch_probe_cs = 0u;
    g_user_switch_probe_ss = 0u;
    g_user_switch_pending_target_rip = 0u;
    g_user_switch_pending_target_rsp = 0u;
    g_user_switch_pending_rflags = 0u;
    g_user_switch_pending_selectors = 0u;
    scheduler64_init();

    debug_write_line("[x64] loading IDT");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    lidt64(
        (const void *)interrupt64_high_half_alias((u64)g_idt),
        (u16)(sizeof(g_idt) - 1u));
    g_idt_high_base = 1u;
#else
    lidt64(g_idt, (u16)(sizeof(g_idt) - 1u));
#endif
    debug_write_line("[x64] IDT loaded");
}

void interrupts64_enable(void)
{
    cpu_sti();
}

void interrupts64_disable(void)
{
    cpu_cli();
}

void interrupts64_trigger_probe(void)
{
    __asm__ __volatile__("int $0x30" : : : "memory");
}

void interrupts64_trigger_breakpoint_proof(void)
{
    interrupt64_breakpoint_proof();
}

void interrupts64_trigger_invalid_opcode_proof(void)
{
    interrupt64_invalid_opcode_proof();
}

void interrupts64_trigger_page_fault_proof(void)
{
    interrupt64_page_fault_proof();
}

void interrupts64_trigger_syscall_probe(u64 code)
{
    __asm__ __volatile__(
        "mov %0, %%rax\n\t"
        "int $0x80"
        :
        : "r"(code)
        : "rax", "memory");
}

u32 interrupts64_trigger_user_entry_probe(u64 rip, u64 rsp, u64 selectors, u64 rflags)
{
    u32 result;

    ++g_user_entry_probe_attempts;
    g_user_entry_probe_mode = X64_USER_PROBE_BASIC;
    g_user_entry_probe_rip = rip;
    g_user_entry_probe_rsp = rsp;
    g_user_entry_probe_cs = selectors & 0xFFFFull;
    g_user_entry_probe_ss = (selectors >> 16) & 0xFFFFull;
    result = usermode64_enter_probe(rip, rsp, selectors, rflags);
    g_user_entry_probe_mode = 0u;
    return result;
}

u32 interrupts64_trigger_user_entry_probe_args(
    u64 rip,
    u64 rsp,
    u64 selectors,
    u64 rflags,
    u64 arg_rcx,
    u64 arg_rdx,
    u64 arg_r8,
    u32 *out_aux)
{
    u32 result;
    u32 saved_syscall_count = g_syscall_count;
    u64 saved_last_syscall_code = g_last_syscall_code;
    u32 saved_attempts = g_user_entry_probe_attempts;
    u32 saved_exits = g_user_entry_probe_exits;
    u32 saved_result = g_user_entry_probe_result;
    u32 saved_aux = g_user_entry_probe_aux;
    u64 saved_rip = g_user_entry_probe_rip;
    u64 saved_rsp = g_user_entry_probe_rsp;
    u64 saved_cs = g_user_entry_probe_cs;
    u64 saved_ss = g_user_entry_probe_ss;
    u32 saved_mode = g_user_entry_probe_mode;

    if (out_aux != 0)
    {
        *out_aux = 0u;
    }
    ++g_user_entry_probe_attempts;
    g_user_entry_probe_mode = X64_USER_PROBE_BASIC;
    g_user_entry_probe_rip = rip;
    g_user_entry_probe_rsp = rsp;
    g_user_entry_probe_cs = selectors & 0xFFFFull;
    g_user_entry_probe_ss = (selectors >> 16) & 0xFFFFull;
    result = usermode64_enter_probe_args(
        rip,
        rsp,
        selectors,
        rflags,
        arg_rcx,
        arg_rdx,
        arg_r8);
    if (out_aux != 0)
    {
        *out_aux = g_user_entry_probe_aux;
    }
    g_syscall_count = saved_syscall_count;
    g_last_syscall_code = saved_last_syscall_code;
    g_user_entry_probe_attempts = saved_attempts;
    g_user_entry_probe_exits = saved_exits;
    g_user_entry_probe_result = saved_result;
    g_user_entry_probe_aux = saved_aux;
    g_user_entry_probe_rip = saved_rip;
    g_user_entry_probe_rsp = saved_rsp;
    g_user_entry_probe_cs = saved_cs;
    g_user_entry_probe_ss = saved_ss;
    g_user_entry_probe_mode = saved_mode;
    return result;
}

u32 interrupts64_trigger_user_preempt_probe(u64 rip, u64 rsp, u64 selectors, u64 rflags)
{
    u32 result;

    ++g_user_preempt_probe_attempts;
    g_user_entry_probe_mode = X64_USER_PROBE_PREEMPT;
    g_user_preempt_probe_rip = rip;
    g_user_preempt_probe_rsp = rsp;
    g_user_preempt_probe_cs = selectors & 0xFFFFull;
    g_user_preempt_probe_ss = (selectors >> 16) & 0xFFFFull;
    result = usermode64_enter_probe(rip, rsp, selectors, rflags);
    g_user_entry_probe_mode = 0u;
    return result;
}

u32 interrupts64_trigger_user_switch_probe(
    u64 source_rip,
    u64 source_rsp,
    u64 target_rip,
    u64 target_rsp,
    u64 selectors,
    u64 rflags)
{
    u32 result;

    ++g_user_switch_probe_attempts;
    g_user_entry_probe_mode = X64_USER_PROBE_SWITCH;
    g_user_switch_source_rip = source_rip;
    g_user_switch_source_rsp = source_rsp;
    g_user_switch_target_rip = target_rip;
    g_user_switch_target_rsp = target_rsp;
    g_user_switch_probe_cs = selectors & 0xFFFFull;
    g_user_switch_probe_ss = (selectors >> 16) & 0xFFFFull;
    g_user_switch_pending_target_rip = target_rip;
    g_user_switch_pending_target_rsp = target_rsp;
    g_user_switch_pending_rflags = rflags;
    g_user_switch_pending_selectors = selectors;
    result = usermode64_enter_probe(source_rip, source_rsp, selectors, rflags);
    g_user_entry_probe_mode = 0u;
    g_user_switch_pending_target_rip = 0u;
    g_user_switch_pending_target_rsp = 0u;
    g_user_switch_pending_rflags = 0u;
    g_user_switch_pending_selectors = 0u;
    return result;
}

u32 interrupts64_trigger_user_runqueue_probe(
    u32 source_pid,
    u64 source_rip,
    u64 source_rsp,
    u32 target_pid,
    u64 target_rip,
    u64 target_rsp,
    u64 rflags)
{
    u32 result;
    u32 source_task;
    u32 target_task;

    scheduler64_runqueue_reset();
    source_task = scheduler64_runqueue_register_launched_process_task(
        source_pid,
        source_rip,
        source_rsp,
        rflags);
    target_task = scheduler64_runqueue_register_launched_process_task(
        target_pid,
        target_rip,
        target_rsp,
        rflags);
    if ((source_task == SCHEDULER64_INVALID_TASK)
        || (target_task == SCHEDULER64_INVALID_TASK)
        || (scheduler64_runqueue_start(source_task) == 0u))
    {
        return 0u;
    }

    g_user_entry_probe_mode = X64_USER_PROBE_RUNQUEUE;
    result = usermode64_enter_probe(
        source_rip,
        source_rsp,
        (u64)process64_runtime_user_entry_selectors(source_pid),
        rflags);
    g_user_entry_probe_mode = 0u;
    scheduler64_runqueue_stop();
    return result;
}

u32 interrupts64_complete_user_entry_probe(u32 result, u32 aux)
{
    if (g_user_entry_probe_mode != X64_USER_PROBE_BASIC)
    {
        return 0u;
    }

    ++g_user_entry_probe_exits;
    g_user_entry_probe_result = result;
    g_user_entry_probe_aux = aux;
    usermode64_probe_complete((u64)result);
    return 1u;
}

u32 interrupts64_exception_count(void)
{
    return g_exception_count;
}

u32 interrupts64_breakpoint_count(void)
{
    return g_breakpoint_count;
}

u32 interrupts64_invalid_opcode_count(void)
{
    return g_invalid_opcode_count;
}

u32 interrupts64_page_fault_count(void)
{
    return g_page_fault_count;
}

u32 interrupts64_probe_count(void)
{
    return g_probe_count;
}

u32 interrupts64_irq_count(void)
{
    return g_irq_count;
}

u32 interrupts64_syscall_count(void)
{
    return g_syscall_count;
}

u32 interrupts64_idt_high_targets(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_idt_high_targets;
#else
    return 0u;
#endif
}

u32 interrupts64_idt_high_base(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_idt_high_base;
#else
    return 0u;
#endif
}

u64 interrupts64_last_syscall_code(void)
{
    return g_last_syscall_code;
}

u64 interrupts64_last_exception_vector(void)
{
    return g_last_exception_vector;
}

u64 interrupts64_last_exception_error(void)
{
    return g_last_exception_error;
}

u64 interrupts64_last_exception_rip(void)
{
    return g_last_exception_rip;
}

u64 interrupts64_last_exception_cr2(void)
{
    return g_last_exception_cr2;
}

u32 interrupts64_user_entry_probe_attempts(void)
{
    return g_user_entry_probe_attempts;
}

u32 interrupts64_user_entry_probe_exits(void)
{
    return g_user_entry_probe_exits;
}

u32 interrupts64_user_entry_probe_result(void)
{
    return g_user_entry_probe_result;
}

u32 interrupts64_user_entry_probe_aux(void)
{
    return g_user_entry_probe_aux;
}

u64 interrupts64_user_entry_probe_rip(void)
{
    return g_user_entry_probe_rip;
}

u64 interrupts64_user_entry_probe_rsp(void)
{
    return g_user_entry_probe_rsp;
}

u64 interrupts64_user_entry_probe_cs(void)
{
    return g_user_entry_probe_cs;
}

u64 interrupts64_user_entry_probe_ss(void)
{
    return g_user_entry_probe_ss;
}

u32 interrupts64_user_preempt_probe_attempts(void)
{
    return g_user_preempt_probe_attempts;
}

u32 interrupts64_user_preempt_probe_exits(void)
{
    return g_user_preempt_probe_exits;
}

u32 interrupts64_user_preempt_probe_result(void)
{
    return g_user_preempt_probe_result;
}

u32 interrupts64_user_preempt_probe_irqs(void)
{
    return g_user_preempt_probe_irqs;
}

u64 interrupts64_user_preempt_probe_rip(void)
{
    return g_user_preempt_probe_rip;
}

u64 interrupts64_user_preempt_probe_rsp(void)
{
    return g_user_preempt_probe_rsp;
}

u64 interrupts64_user_preempt_probe_cs(void)
{
    return g_user_preempt_probe_cs;
}

u64 interrupts64_user_preempt_probe_ss(void)
{
    return g_user_preempt_probe_ss;
}

u32 interrupts64_user_switch_probe_attempts(void)
{
    return g_user_switch_probe_attempts;
}

u32 interrupts64_user_switch_probe_exits(void)
{
    return g_user_switch_probe_exits;
}

u32 interrupts64_user_switch_probe_result(void)
{
    return g_user_switch_probe_result;
}

u32 interrupts64_user_switch_probe_irqs(void)
{
    return g_user_switch_probe_irqs;
}

u32 interrupts64_user_switch_probe_switches(void)
{
    return g_user_switch_probe_switches;
}

u64 interrupts64_user_switch_source_rip(void)
{
    return g_user_switch_source_rip;
}

u64 interrupts64_user_switch_source_rsp(void)
{
    return g_user_switch_source_rsp;
}

u64 interrupts64_user_switch_target_rip(void)
{
    return g_user_switch_target_rip;
}

u64 interrupts64_user_switch_target_rsp(void)
{
    return g_user_switch_target_rsp;
}

u64 interrupts64_user_switch_probe_cs(void)
{
    return g_user_switch_probe_cs;
}

u64 interrupts64_user_switch_probe_ss(void)
{
    return g_user_switch_probe_ss;
}

u32 interrupts64_user_runqueue_probe_attempts(void)
{
    return scheduler64_runqueue_attempts();
}

u32 interrupts64_user_runqueue_probe_exits(void)
{
    return scheduler64_runqueue_exits();
}

u32 interrupts64_user_runqueue_probe_irqs(void)
{
    return scheduler64_runqueue_irqs();
}

u32 interrupts64_user_runqueue_probe_switches(void)
{
    return scheduler64_runqueue_switches();
}

u32 interrupts64_user_runqueue_source_result(void)
{
    return scheduler64_runqueue_task_result(0u);
}

u32 interrupts64_user_runqueue_target_result(void)
{
    return scheduler64_runqueue_task_result(1u);
}

u32 interrupts64_user_runqueue_source_pid(void)
{
    return scheduler64_runqueue_task_pid(0u);
}

u32 interrupts64_user_runqueue_target_pid(void)
{
    return scheduler64_runqueue_task_pid(1u);
}

u32 interrupts64_user_runqueue_source_runtime_token(void)
{
    return scheduler64_runqueue_task_runtime_token(0u);
}

u32 interrupts64_user_runqueue_target_runtime_token(void)
{
    return scheduler64_runqueue_task_runtime_token(1u);
}

u32 interrupts64_user_runqueue_source_entry_token(void)
{
    return scheduler64_runqueue_task_entry_token(0u);
}

u32 interrupts64_user_runqueue_target_entry_token(void)
{
    return scheduler64_runqueue_task_entry_token(1u);
}

u64 interrupts64_user_runqueue_source_rip(void)
{
    return scheduler64_runqueue_task_rip(0u);
}

u64 interrupts64_user_runqueue_source_rsp(void)
{
    return scheduler64_runqueue_task_rsp(0u);
}

u64 interrupts64_user_runqueue_target_rip(void)
{
    return scheduler64_runqueue_task_rip(1u);
}

u64 interrupts64_user_runqueue_target_rsp(void)
{
    return scheduler64_runqueue_task_rsp(1u);
}

u64 interrupts64_user_runqueue_probe_cs(void)
{
    return scheduler64_runqueue_cs();
}

u64 interrupts64_user_runqueue_probe_ss(void)
{
    return scheduler64_runqueue_ss();
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
static void interrupt64_deliver_linux_signals_if_ready(struct interrupt_frame64 *frame)
{
    u32 pid;

    if ((frame == 0) || ((frame->cs & 0x3ull) != 0x3ull))
    {
        return;
    }

    pid = scheduler64_runqueue_current_pid();
    if ((pid != 0u) && (persona64_type(pid) == PERSONA64_TYPE_LINUX_ELF))
    {
        (void)linux_abi64_signal_deliver_pending(pid, frame);
    }
}

static void interrupt64_record_persona_crash_if_ready(const struct interrupt_frame64 *frame)
{
    u32 pid;
    u32 persona_type;
    u64 fault_address;

    if ((frame == 0) || ((frame->cs & 0x3ull) != 0x3ull))
    {
        return;
    }

    pid = scheduler64_runqueue_current_pid();
    persona_type = persona64_type(pid);
    if ((persona_type != PERSONA64_TYPE_LINUX_ELF)
        && (persona_type != PERSONA64_TYPE_WINDOWS_PE)
        && (persona_type != PERSONA64_TYPE_MACOS_MACHO))
    {
        return;
    }

    fault_address = (frame->vector == PERSONA_AUDIT64_CRASH_VECTOR_PAGE_FAULT)
        ? read_cr2_64()
        : 0ull;
    (void)persona_audit64_record_crash(
        pid,
        (u32)frame->vector,
        frame->error_code,
        frame->rip,
        frame->rsp,
        fault_address);
}
#endif

void interrupts64_dispatch(struct interrupt_frame64 *frame)
{
    if (frame->vector < 32u)
    {
        interrupt64_record_exception(frame);
        if (interrupt64_try_recover_exception(frame))
        {
            return;
        }

#ifdef LIMITLESS_X64_UEFI_KERNEL
        if (((frame->cs & 0x3ull) == 0x3ull)
            && (windows_seh64_dispatch_exception(scheduler64_runqueue_current_pid(), frame) != 0u))
        {
            return;
        }
        interrupt64_record_persona_crash_if_ready(frame);
#endif

        interrupt64_log_exception(frame);
        cpu_cli();
        cpu_halt_forever();
    }

    if (frame->vector == X64_PROBE_VECTOR)
    {
        ++g_probe_count;
        return;
    }

    if ((frame->vector >= IRQ_VECTOR_BASE) && (frame->vector < (IRQ_VECTOR_BASE + 16u)))
    {
        u8 irq = (u8)(frame->vector - IRQ_VECTOR_BASE);

        ++g_irq_count;
        if (irq == 0u)
        {
            pit_handle_interrupt();
#ifdef LIMITLESS_X64_UEFI_KERNEL
            pe64_kuser_tick_update_all();
#endif
            if (((frame->cs & 0x3ull) == 0x3ull)
                && (g_user_entry_probe_mode == X64_USER_PROBE_PREEMPT))
            {
                ++g_user_preempt_probe_irqs;
                g_user_preempt_probe_rip = frame->rip;
                g_user_preempt_probe_rsp = frame->rsp;
                g_user_preempt_probe_cs = frame->cs;
                g_user_preempt_probe_ss = frame->ss;
            }
            else if (((frame->cs & 0x3ull) == 0x3ull)
                && (g_user_entry_probe_mode == X64_USER_PROBE_SWITCH))
            {
                ++g_user_switch_probe_irqs;
                if (g_user_switch_probe_switches == 0u)
                {
                    ++g_user_switch_probe_switches;
                    g_user_switch_source_rip = frame->rip;
                    g_user_switch_source_rsp = frame->rsp;
                    g_user_switch_probe_cs = frame->cs;
                    g_user_switch_probe_ss = frame->ss;
                    interrupt64_zero_user_registers(frame);
                    frame->rip = g_user_switch_pending_target_rip;
                    frame->rsp = g_user_switch_pending_target_rsp;
                    frame->rflags = g_user_switch_pending_rflags;
                    frame->cs = g_user_switch_pending_selectors & 0xFFFFull;
                    frame->ss = (g_user_switch_pending_selectors >> 16) & 0xFFFFull;
                    g_user_switch_target_rip = frame->rip;
                    g_user_switch_target_rsp = frame->rsp;
                }
            }
            else if (((frame->cs & 0x3ull) == 0x3ull)
                && (g_user_entry_probe_mode == X64_USER_PROBE_RUNQUEUE))
            {
                (void)scheduler64_runqueue_on_timer(frame);
            }
        }
        else if (irq == 1u)
        {
            input64_handle_keyboard_interrupt();
        }
        else if (irq == 12u)
        {
            input64_handle_mouse_interrupt();
        }

        if (apic64_enabled() != 0u)
        {
            apic64_send_eoi(irq);
        }
        else
        {
            pic_send_eoi(irq);
        }
#ifdef LIMITLESS_X64_UEFI_KERNEL
        interrupt64_deliver_linux_signals_if_ready(frame);
#endif
        return;
    }

    if (frame->vector == X64_SYSCALL_VECTOR)
    {
        ++g_syscall_count;
        g_last_syscall_code = frame->rax;
        if ((frame->rax == X64_SYSCALL_USERMODE_PROBE_EXIT)
            && ((frame->cs & 0x3ull) == 0x3ull))
        {
            if (g_user_entry_probe_mode == X64_USER_PROBE_PREEMPT)
            {
                ++g_user_preempt_probe_exits;
                g_user_preempt_probe_result = (u32)frame->rbx;
                g_user_preempt_probe_rsp = frame->rsp;
                g_user_preempt_probe_cs = frame->cs;
                g_user_preempt_probe_ss = frame->ss;
            }
            else if (g_user_entry_probe_mode == X64_USER_PROBE_SWITCH)
            {
                ++g_user_switch_probe_exits;
                g_user_switch_probe_result = (u32)frame->rbx;
                g_user_switch_target_rsp = frame->rsp;
                g_user_switch_probe_cs = frame->cs;
                g_user_switch_probe_ss = frame->ss;
            }
            else if (g_user_entry_probe_mode == X64_USER_PROBE_RUNQUEUE)
            {
                u32 exit_state = scheduler64_runqueue_on_exit(frame, (u32)frame->rbx);
                if (exit_state == SCHEDULER64_RUNQUEUE_EXIT_RESUMED)
                {
                    return;
                }
            }
            else
            {
                ++g_user_entry_probe_exits;
                g_user_entry_probe_result = (u32)frame->rbx;
                g_user_entry_probe_aux = (u32)frame->rcx;
                g_user_entry_probe_rsp = frame->rsp;
                g_user_entry_probe_cs = frame->cs;
                g_user_entry_probe_ss = frame->ss;
            }
            usermode64_probe_complete(frame->rbx);
        }

        frame->rax = syscall64_dispatch(frame->rax, frame->rbx, frame->rcx, frame->rdx);
#ifdef LIMITLESS_X64_UEFI_KERNEL
        interrupt64_deliver_linux_signals_if_ready(frame);
#endif
    }
}
