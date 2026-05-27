#ifndef LIMITLESS_INTERRUPTS_X64_H
#define LIMITLESS_INTERRUPTS_X64_H

#include "types.h"

struct interrupt_frame64
{
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rdi;
    u64 rsi;
    u64 rbp;
    u64 rbx;
    u64 rdx;
    u64 rcx;
    u64 rax;
    u64 vector;
    u64 error_code;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

void interrupts64_init(void);
void interrupts64_enable(void);
void interrupts64_disable(void);
void interrupts64_trigger_probe(void);
void interrupts64_trigger_breakpoint_proof(void);
void interrupts64_trigger_invalid_opcode_proof(void);
void interrupts64_trigger_page_fault_proof(void);
void interrupts64_trigger_syscall_probe(u64 code);
u32 interrupts64_trigger_user_entry_probe(u64 rip, u64 rsp, u64 selectors, u64 rflags);
u32 interrupts64_trigger_user_entry_probe_args(
    u64 rip,
    u64 rsp,
    u64 selectors,
    u64 rflags,
    u64 arg_rcx,
    u64 arg_rdx,
    u64 arg_r8,
    u32 *out_aux);
u32 interrupts64_trigger_user_preempt_probe(u64 rip, u64 rsp, u64 selectors, u64 rflags);
u32 interrupts64_trigger_user_switch_probe(
    u64 source_rip,
    u64 source_rsp,
    u64 target_rip,
    u64 target_rsp,
    u64 selectors,
    u64 rflags);
u32 interrupts64_trigger_user_runqueue_probe(
    u32 source_pid,
    u64 source_rip,
    u64 source_rsp,
    u32 target_pid,
    u64 target_rip,
    u64 target_rsp,
    u64 rflags);
u32 interrupts64_complete_user_entry_probe(u32 result, u32 aux);
u32 interrupts64_exception_count(void);
u32 interrupts64_breakpoint_count(void);
u32 interrupts64_invalid_opcode_count(void);
u32 interrupts64_page_fault_count(void);
u32 interrupts64_probe_count(void);
u32 interrupts64_irq_count(void);
u32 interrupts64_syscall_count(void);
u64 interrupts64_last_syscall_code(void);
u64 interrupts64_last_exception_vector(void);
u64 interrupts64_last_exception_error(void);
u64 interrupts64_last_exception_rip(void);
u64 interrupts64_last_exception_cr2(void);
u32 interrupts64_user_entry_probe_attempts(void);
u32 interrupts64_user_entry_probe_exits(void);
u32 interrupts64_user_entry_probe_result(void);
u32 interrupts64_user_entry_probe_aux(void);
u64 interrupts64_user_entry_probe_rip(void);
u64 interrupts64_user_entry_probe_rsp(void);
u64 interrupts64_user_entry_probe_cs(void);
u64 interrupts64_user_entry_probe_ss(void);
u32 interrupts64_user_preempt_probe_attempts(void);
u32 interrupts64_user_preempt_probe_exits(void);
u32 interrupts64_user_preempt_probe_result(void);
u32 interrupts64_user_preempt_probe_irqs(void);
u64 interrupts64_user_preempt_probe_rip(void);
u64 interrupts64_user_preempt_probe_rsp(void);
u64 interrupts64_user_preempt_probe_cs(void);
u64 interrupts64_user_preempt_probe_ss(void);
u32 interrupts64_user_switch_probe_attempts(void);
u32 interrupts64_user_switch_probe_exits(void);
u32 interrupts64_user_switch_probe_result(void);
u32 interrupts64_user_switch_probe_irqs(void);
u32 interrupts64_user_switch_probe_switches(void);
u64 interrupts64_user_switch_source_rip(void);
u64 interrupts64_user_switch_source_rsp(void);
u64 interrupts64_user_switch_target_rip(void);
u64 interrupts64_user_switch_target_rsp(void);
u64 interrupts64_user_switch_probe_cs(void);
u64 interrupts64_user_switch_probe_ss(void);
u32 interrupts64_user_runqueue_probe_attempts(void);
u32 interrupts64_user_runqueue_probe_exits(void);
u32 interrupts64_user_runqueue_probe_irqs(void);
u32 interrupts64_user_runqueue_probe_switches(void);
u32 interrupts64_user_runqueue_source_result(void);
u32 interrupts64_user_runqueue_target_result(void);
u32 interrupts64_user_runqueue_source_pid(void);
u32 interrupts64_user_runqueue_target_pid(void);
u32 interrupts64_user_runqueue_source_runtime_token(void);
u32 interrupts64_user_runqueue_target_runtime_token(void);
u32 interrupts64_user_runqueue_source_entry_token(void);
u32 interrupts64_user_runqueue_target_entry_token(void);
u64 interrupts64_user_runqueue_source_rip(void);
u64 interrupts64_user_runqueue_source_rsp(void);
u64 interrupts64_user_runqueue_target_rip(void);
u64 interrupts64_user_runqueue_target_rsp(void);
u64 interrupts64_user_runqueue_probe_cs(void);
u64 interrupts64_user_runqueue_probe_ss(void);
void interrupts64_dispatch(struct interrupt_frame64 *frame);

#endif
