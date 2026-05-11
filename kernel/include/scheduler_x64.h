#ifndef LIMITLESS_SCHEDULER_X64_H
#define LIMITLESS_SCHEDULER_X64_H

#include "interrupts_x64.h"
#include "types.h"

#define SCHEDULER64_INVALID_TASK 0xFFFFFFFFu

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
u32 scheduler64_runqueue_register_launched_process_task(
    u32 pid,
    u64 rip,
    u64 rsp,
    u64 rflags);
u32 scheduler64_runqueue_start(u32 first_task);
void scheduler64_runqueue_stop(void);
u32 scheduler64_runqueue_on_timer(struct interrupt_frame64 *frame);
u32 scheduler64_runqueue_on_exit(struct interrupt_frame64 *frame, u32 result);

u32 scheduler64_runqueue_attempts(void);
u32 scheduler64_runqueue_exits(void);
u32 scheduler64_runqueue_irqs(void);
u32 scheduler64_runqueue_switches(void);
u32 scheduler64_runqueue_result(void);
u32 scheduler64_runqueue_task_result(u32 task_id);
u32 scheduler64_runqueue_task_pid(u32 task_id);
u32 scheduler64_runqueue_task_runtime_token(u32 task_id);
u32 scheduler64_runqueue_task_entry_token(u32 task_id);
u64 scheduler64_runqueue_task_rip(u32 task_id);
u64 scheduler64_runqueue_task_rsp(u32 task_id);
u64 scheduler64_runqueue_cs(void);
u64 scheduler64_runqueue_ss(void);

#endif
