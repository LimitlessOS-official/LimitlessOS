#ifndef LIMITLESS_SCHEDULER_X64_H
#define LIMITLESS_SCHEDULER_X64_H

#include "interrupts_x64.h"
#include "types.h"

#define SCHEDULER64_INVALID_TASK 0xFFFFFFFFu

typedef void (*scheduler64_sleep_expiry_callback_t)(u32 task_id, u64 cookie);

enum scheduler64_task_state
{
    SCHEDULER64_TASK_UNUSED = 0u,
    SCHEDULER64_TASK_READY = 1u,
    SCHEDULER64_TASK_RUNNING = 2u,
    SCHEDULER64_TASK_EXITED = 3u,
    SCHEDULER64_TASK_BLOCKED = 4u
};

enum scheduler64_runqueue_exit
{
    SCHEDULER64_RUNQUEUE_EXIT_NONE = 0,
    SCHEDULER64_RUNQUEUE_EXIT_RESUMED = 1,
    SCHEDULER64_RUNQUEUE_EXIT_COMPLETE = 2
};

void scheduler64_init(void);
void scheduler64_runqueue_reset(void);
u32 scheduler64_runqueue_register_user_task(u64 rip, u64 rsp, u64 selectors, u64 rflags);
u32 scheduler64_runqueue_register_process_task(
    u32 pid,
    u32 runtime_token,
    u32 entry_token,
    u64 rip,
    u64 rsp,
    u64 selectors,
    u64 rflags);
#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_runqueue_register_process_frame(
    u32 pid,
    u32 runtime_token,
    u32 entry_token,
    const struct interrupt_frame64 *frame);
#endif
u32 scheduler64_runqueue_register_launched_process_task(
    u32 pid,
    u64 rip,
    u64 rsp,
    u64 rflags);
u32 scheduler64_runqueue_start(u32 first_task);
void scheduler64_runqueue_stop(void);
u32 scheduler64_runqueue_block_task(u32 task_id);
u32 scheduler64_runqueue_wake_task(u32 task_id);
u32 scheduler64_runqueue_wake_task_with_result(u32 task_id, u64 result);
#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_runqueue_on_blocked_syscall(struct interrupt_frame64 *frame);
#endif
u32 scheduler64_runqueue_on_timer(struct interrupt_frame64 *frame);
u32 scheduler64_runqueue_on_exit(struct interrupt_frame64 *frame, u32 result);
u32 scheduler64_sleep_for_ticks(u32 requested_ticks);
u32 scheduler64_sleep_current_task_for_ticks(
    u32 requested_ticks,
    u64 resume_rax,
    scheduler64_sleep_expiry_callback_t expiry_callback,
    u64 callback_cookie);
u32 scheduler64_sleep_cancel_task(u32 task_id);

u32 scheduler64_runqueue_current_pid(void);
#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_runqueue_current_task_id(void);
#endif
u32 scheduler64_runqueue_attempts(void);
u32 scheduler64_runqueue_exits(void);
u32 scheduler64_runqueue_irqs(void);
u32 scheduler64_runqueue_switches(void);
u32 scheduler64_runqueue_result(void);
#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_runqueue_block_count(void);
u32 scheduler64_runqueue_wake_count(void);
u32 scheduler64_runqueue_block_denial_count(void);
u32 scheduler64_runqueue_blocked_count(void);
u32 scheduler64_runqueue_task_state(u32 task_id);
#endif
u32 scheduler64_runqueue_task_result(u32 task_id);
u32 scheduler64_runqueue_task_pid(u32 task_id);
u32 scheduler64_runqueue_task_runtime_token(u32 task_id);
u32 scheduler64_runqueue_task_entry_token(u32 task_id);
u64 scheduler64_runqueue_task_rip(u32 task_id);
u64 scheduler64_runqueue_task_rsp(u32 task_id);
u64 scheduler64_runqueue_cs(void);
u64 scheduler64_runqueue_ss(void);
u32 scheduler64_sleep_count(void);
u32 scheduler64_sleep_denial_count(void);
u32 scheduler64_sleep_last_requested_ticks(void);
u32 scheduler64_sleep_last_elapsed_ticks(void);
u32 scheduler64_sleep_last_start_ticks(void);
u32 scheduler64_sleep_last_end_ticks(void);
#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_sleep_pending_count(void);
u32 scheduler64_sleep_wake_count(void);
u32 scheduler64_sleep_last_task_id(void);
u32 scheduler64_sleep_last_wake_tick(void);
#endif

#endif
