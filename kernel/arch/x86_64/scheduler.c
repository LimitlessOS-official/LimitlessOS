#include "scheduler_x64.h"

#include "interrupts_x64.h"
#include "paging_x64.h"
#include "pit.h"
#include "process_x64.h"

enum
{
    SCHEDULER64_RUNQUEUE_TASKS = 4u,
    SCHEDULER64_SLEEP_QUEUE_TASKS = SCHEDULER64_RUNQUEUE_TASKS,
    SCHEDULER64_SLEEP_SPIN_BUDGET_PER_TICK = 50000u,
    SCHEDULER64_SLEEP_MAX_SPIN_BUDGET = 1000000u
};

struct scheduler64_task
{
    u32 state;
    u32 frame_valid;
    u32 pid;
    u32 runtime_token;
    u32 entry_token;
    u32 dispatches;
    u32 exits;
    u32 result;
    u64 initial_rip;
    u64 initial_rsp;
    u64 saved_rip;
    u64 saved_rsp;
    struct interrupt_frame64 frame;
};

struct scheduler64_runqueue
{
    u32 active;
    u32 task_count;
    u32 current_task;
    u32 attempts;
    u32 exits;
    u32 irqs;
    u32 switches;
    u32 result;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u32 blocks;
    u32 wakes;
    u32 block_denials;
#endif
    u64 cs;
    u64 ss;
    struct scheduler64_task tasks[SCHEDULER64_RUNQUEUE_TASKS];
};

#ifdef LIMITLESS_X64_UEFI_KERNEL
enum
{
    SCHEDULER64_CR3_REASON_START = 0x53544152u,
    SCHEDULER64_CR3_REASON_TIMER = 0x54494D52u,
    SCHEDULER64_CR3_REASON_BLOCKED_SYSCALL = 0x42535953u,
    SCHEDULER64_CR3_REASON_EXIT_RESUME = 0x45585245u,
    SCHEDULER64_CR3_REASON_EXIT_KERNEL = 0x45584B52u,
    SCHEDULER64_CR3_REASON_STOP = 0x53544F50u,
    SCHEDULER64_CR3_REASON_RESET = 0x52534554u
};

struct scheduler64_sleep_entry
{
    u32 active;
    u32 task_id;
    u32 start_tick;
    u32 wake_tick;
    u32 requested_ticks;
    u64 resume_rax;
    scheduler64_sleep_expiry_callback_t expiry_callback;
    u64 callback_cookie;
};
#endif

static struct scheduler64_runqueue g_runqueue;
static u32 g_scheduler64_sleep_count = 0u;
static u32 g_scheduler64_sleep_denial_count = 0u;
static u32 g_scheduler64_sleep_last_requested_ticks = 0u;
static u32 g_scheduler64_sleep_last_elapsed_ticks = 0u;
static u32 g_scheduler64_sleep_last_start_ticks = 0u;
static u32 g_scheduler64_sleep_last_end_ticks = 0u;
#ifdef LIMITLESS_X64_UEFI_KERNEL
static struct scheduler64_sleep_entry g_scheduler64_sleep_queue[SCHEDULER64_SLEEP_QUEUE_TASKS];
static u32 g_scheduler64_sleep_wake_count = 0u;
static u32 g_scheduler64_sleep_last_task_id = SCHEDULER64_INVALID_TASK;
static u32 g_scheduler64_sleep_last_wake_tick = 0u;
#endif

static void scheduler64_cpu_pause(void)
{
    __asm__ __volatile__("pause");
}

static void scheduler64_clear_frame(struct interrupt_frame64 *frame)
{
    frame->r15 = 0u;
    frame->r14 = 0u;
    frame->r13 = 0u;
    frame->r12 = 0u;
    frame->r11 = 0u;
    frame->r10 = 0u;
    frame->r9 = 0u;
    frame->r8 = 0u;
    frame->rdi = 0u;
    frame->rsi = 0u;
    frame->rbp = 0u;
    frame->rbx = 0u;
    frame->rdx = 0u;
    frame->rcx = 0u;
    frame->rax = 0u;
    frame->vector = 0u;
    frame->error_code = 0u;
    frame->rip = 0u;
    frame->cs = 0u;
    frame->rflags = 0u;
    frame->rsp = 0u;
    frame->ss = 0u;
}

static void scheduler64_prepare_user_frame(
    struct interrupt_frame64 *frame,
    u64 rip,
    u64 rsp,
    u64 selectors,
    u64 rflags)
{
    scheduler64_clear_frame(frame);
    frame->rip = rip;
    frame->rsp = rsp;
    frame->rflags = rflags;
    frame->cs = selectors & 0xFFFFull;
    frame->ss = (selectors >> 16) & 0xFFFFull;
}

static void scheduler64_clear_task(struct scheduler64_task *task)
{
    task->state = SCHEDULER64_TASK_UNUSED;
    task->frame_valid = 0u;
    task->pid = 0u;
    task->runtime_token = 0u;
    task->entry_token = 0u;
    task->dispatches = 0u;
    task->exits = 0u;
    task->result = 0u;
    task->initial_rip = 0u;
    task->initial_rsp = 0u;
    task->saved_rip = 0u;
    task->saved_rsp = 0u;
    scheduler64_clear_frame(&task->frame);
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
static void scheduler64_clear_sleep_entry(struct scheduler64_sleep_entry *entry)
{
    entry->active = 0u;
    entry->task_id = SCHEDULER64_INVALID_TASK;
    entry->start_tick = 0u;
    entry->wake_tick = 0u;
    entry->requested_ticks = 0u;
    entry->resume_rax = 0ull;
    entry->expiry_callback = 0;
    entry->callback_cookie = 0ull;
}

static void scheduler64_sleep_queue_reset(void)
{
    u32 index;

    for (index = 0u; index < SCHEDULER64_SLEEP_QUEUE_TASKS; ++index)
    {
        scheduler64_clear_sleep_entry(&g_scheduler64_sleep_queue[index]);
    }
}

static u32 scheduler64_sleep_tick_reached(u32 now_tick, u32 wake_tick)
{
    return (((u32)(now_tick - wake_tick)) < 0x80000000u) ? 1u : 0u;
}

static struct scheduler64_sleep_entry *scheduler64_sleep_free_entry(void)
{
    u32 index;

    for (index = 0u; index < SCHEDULER64_SLEEP_QUEUE_TASKS; ++index)
    {
        if (g_scheduler64_sleep_queue[index].active == 0u)
        {
            return &g_scheduler64_sleep_queue[index];
        }
    }

    return 0;
}

static u32 scheduler64_sleep_task_pending(u32 task_id)
{
    u32 index;

    for (index = 0u; index < SCHEDULER64_SLEEP_QUEUE_TASKS; ++index)
    {
        if ((g_scheduler64_sleep_queue[index].active != 0u)
            && (g_scheduler64_sleep_queue[index].task_id == task_id))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 scheduler64_sleep_wake_expired(u32 now_tick)
{
    u32 index;
    u32 wake_count = 0u;

    for (index = 0u; index < SCHEDULER64_SLEEP_QUEUE_TASKS; ++index)
    {
        struct scheduler64_sleep_entry *entry = &g_scheduler64_sleep_queue[index];
        u32 elapsed;

        if ((entry->active == 0u)
            || (scheduler64_sleep_tick_reached(now_tick, entry->wake_tick) == 0u))
        {
            continue;
        }

        elapsed = (u32)(now_tick - entry->start_tick);
        g_scheduler64_sleep_last_task_id = entry->task_id;
        g_scheduler64_sleep_last_requested_ticks = entry->requested_ticks;
        g_scheduler64_sleep_last_start_ticks = entry->start_tick;
        g_scheduler64_sleep_last_end_ticks = now_tick;
        g_scheduler64_sleep_last_elapsed_ticks = elapsed;
        g_scheduler64_sleep_last_wake_tick = entry->wake_tick;
        if (entry->expiry_callback != 0)
        {
            entry->expiry_callback(entry->task_id, entry->callback_cookie);
        }

        if (scheduler64_runqueue_wake_task_with_result(entry->task_id, entry->resume_rax) != 0u)
        {
            ++g_scheduler64_sleep_wake_count;
            ++wake_count;
        }
        else
        {
            ++g_scheduler64_sleep_denial_count;
        }

        scheduler64_clear_sleep_entry(entry);
    }

    return wake_count;
}
#endif

static u32 scheduler64_next_runnable(u32 current)
{
    u32 offset;

    if (g_runqueue.task_count == 0u)
    {
        return SCHEDULER64_INVALID_TASK;
    }

    for (offset = 1u; offset < g_runqueue.task_count; ++offset)
    {
        u32 candidate = (current + offset) % g_runqueue.task_count;
        if ((g_runqueue.tasks[candidate].state == SCHEDULER64_TASK_READY)
            && (g_runqueue.tasks[candidate].frame_valid != 0u))
        {
            return candidate;
        }
    }

    return SCHEDULER64_INVALID_TASK;
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
static u32 scheduler64_switch_cr3_for_task(
    const struct scheduler64_task *task,
    u32 reason)
{
    if ((task == 0)
        || (task->pid == 0u)
        || (process64_page_root_physical(task->pid) == 0ull))
    {
        return 1u;
    }

    return paging64_switch_to_process_root(task->pid, reason);
}

static void scheduler64_switch_cr3_to_kernel(u32 reason)
{
    (void)paging64_switch_to_kernel_root(reason);
}
#endif

void scheduler64_init(void)
{
    scheduler64_runqueue_reset();
    g_scheduler64_sleep_count = 0u;
    g_scheduler64_sleep_denial_count = 0u;
    g_scheduler64_sleep_last_requested_ticks = 0u;
    g_scheduler64_sleep_last_elapsed_ticks = 0u;
    g_scheduler64_sleep_last_start_ticks = 0u;
    g_scheduler64_sleep_last_end_ticks = 0u;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    g_scheduler64_sleep_wake_count = 0u;
    g_scheduler64_sleep_last_task_id = SCHEDULER64_INVALID_TASK;
    g_scheduler64_sleep_last_wake_tick = 0u;
    scheduler64_sleep_queue_reset();
#endif
}

void scheduler64_runqueue_reset(void)
{
    u32 index;

    g_runqueue.active = 0u;
    g_runqueue.task_count = 0u;
    g_runqueue.current_task = SCHEDULER64_INVALID_TASK;
    g_runqueue.attempts = 0u;
    g_runqueue.exits = 0u;
    g_runqueue.irqs = 0u;
    g_runqueue.switches = 0u;
    g_runqueue.result = 0u;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    g_runqueue.blocks = 0u;
    g_runqueue.wakes = 0u;
    g_runqueue.block_denials = 0u;
#endif
    g_runqueue.cs = 0u;
    g_runqueue.ss = 0u;
    for (index = 0u; index < SCHEDULER64_RUNQUEUE_TASKS; ++index)
    {
        scheduler64_clear_task(&g_runqueue.tasks[index]);
    }
#ifdef LIMITLESS_X64_UEFI_KERNEL
    scheduler64_switch_cr3_to_kernel(SCHEDULER64_CR3_REASON_RESET);
    scheduler64_sleep_queue_reset();
#endif
}

u32 scheduler64_runqueue_register_user_task(u64 rip, u64 rsp, u64 selectors, u64 rflags)
{
    u32 task_id = g_runqueue.task_count;
    struct scheduler64_task *task;

    if (task_id >= SCHEDULER64_RUNQUEUE_TASKS)
    {
        return SCHEDULER64_INVALID_TASK;
    }

    task = &g_runqueue.tasks[task_id];
    scheduler64_clear_task(task);
    task->state = SCHEDULER64_TASK_READY;
    task->frame_valid = 1u;
    task->initial_rip = rip;
    task->initial_rsp = rsp;
    scheduler64_prepare_user_frame(&task->frame, rip, rsp, selectors, rflags);
    ++g_runqueue.task_count;
    return task_id;
}

u32 scheduler64_runqueue_register_process_task(
    u32 pid,
    u32 runtime_token,
    u32 entry_token,
    u64 rip,
    u64 rsp,
    u64 selectors,
    u64 rflags)
{
    u32 task_id;
    struct scheduler64_task *task;

    if ((pid == 0u) || (runtime_token == 0u) || (entry_token == 0u))
    {
        return SCHEDULER64_INVALID_TASK;
    }

    task_id = scheduler64_runqueue_register_user_task(rip, rsp, selectors, rflags);
    if (task_id == SCHEDULER64_INVALID_TASK)
    {
        return SCHEDULER64_INVALID_TASK;
    }

    task = &g_runqueue.tasks[task_id];
    task->pid = pid;
    task->runtime_token = runtime_token;
    task->entry_token = entry_token;
    return task_id;
}

u32 scheduler64_runqueue_register_launched_process_task(
    u32 pid,
    u64 rip,
    u64 rsp,
    u64 rflags)
{
    u32 runtime_token;
    u32 entry_token;
    u64 selectors;

    if (process64_runtime_user_entry_ready(pid) == 0u)
    {
        return SCHEDULER64_INVALID_TASK;
    }

    runtime_token = process64_runtime_token(pid);
    entry_token = process64_runtime_user_entry_token(pid);
    selectors = process64_runtime_user_entry_selectors(pid);

    if (rip == 0u)
    {
        rip = process64_runtime_user_entry_rip(pid);
    }

    if (rsp == 0u)
    {
        rsp = process64_runtime_user_entry_rsp(pid);
    }

    if (rflags == 0u)
    {
        rflags = process64_runtime_user_entry_rflags(pid);
    }

    if ((runtime_token == 0u)
        || (entry_token == 0u)
        || (selectors == 0u)
        || (rip == 0u)
        || (rsp == 0u)
        || (rflags == 0u))
    {
        return SCHEDULER64_INVALID_TASK;
    }

    return scheduler64_runqueue_register_process_task(
        pid,
        runtime_token,
        entry_token,
        rip,
        rsp,
        selectors,
        rflags);
}

u32 scheduler64_runqueue_start(u32 first_task)
{
    struct scheduler64_task *task;

    if ((first_task >= g_runqueue.task_count)
        || (g_runqueue.tasks[first_task].state != SCHEDULER64_TASK_READY))
    {
        return 0u;
    }

    task = &g_runqueue.tasks[first_task];
    g_runqueue.active = 1u;
    g_runqueue.current_task = first_task;
    ++g_runqueue.attempts;
    task->state = SCHEDULER64_TASK_RUNNING;
    ++task->dispatches;
    g_runqueue.cs = task->frame.cs;
    g_runqueue.ss = task->frame.ss;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    if (scheduler64_switch_cr3_for_task(task, SCHEDULER64_CR3_REASON_START) == 0u)
    {
        task->state = SCHEDULER64_TASK_READY;
        g_runqueue.active = 0u;
        g_runqueue.current_task = SCHEDULER64_INVALID_TASK;
        return 0u;
    }
#endif
    return 1u;
}

void scheduler64_runqueue_stop(void)
{
    g_runqueue.active = 0u;
    g_runqueue.current_task = SCHEDULER64_INVALID_TASK;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    scheduler64_switch_cr3_to_kernel(SCHEDULER64_CR3_REASON_STOP);
#endif
}

u32 scheduler64_runqueue_block_task(u32 task_id)
{
#ifndef LIMITLESS_X64_UEFI_KERNEL
    (void)task_id;
    return 0u;
#else
    struct scheduler64_task *task;

    if ((task_id >= g_runqueue.task_count)
        || (g_runqueue.tasks[task_id].frame_valid == 0u)
        || (g_runqueue.tasks[task_id].state == SCHEDULER64_TASK_UNUSED)
        || (g_runqueue.tasks[task_id].state == SCHEDULER64_TASK_EXITED)
        || (g_runqueue.tasks[task_id].state == SCHEDULER64_TASK_BLOCKED))
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        ++g_runqueue.block_denials;
#endif
        return 0u;
    }

    if (g_runqueue.tasks[task_id].state == SCHEDULER64_TASK_RUNNING)
    {
        if ((g_runqueue.active == 0u)
            || (g_runqueue.current_task != task_id)
            || (scheduler64_next_runnable(task_id) == SCHEDULER64_INVALID_TASK))
        {
#ifdef LIMITLESS_X64_UEFI_KERNEL
            ++g_runqueue.block_denials;
#endif
            return 0u;
        }
    }

    task = &g_runqueue.tasks[task_id];
    task->state = SCHEDULER64_TASK_BLOCKED;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    ++g_runqueue.blocks;
#endif
    return 1u;
#endif
}

u32 scheduler64_runqueue_wake_task(u32 task_id)
{
    return scheduler64_runqueue_wake_task_with_result(task_id, 0ull);
}

u32 scheduler64_runqueue_wake_task_with_result(u32 task_id, u64 result)
{
#ifndef LIMITLESS_X64_UEFI_KERNEL
    (void)task_id;
    (void)result;
    return 0u;
#else
    struct scheduler64_task *task;

    if ((task_id >= g_runqueue.task_count)
        || (g_runqueue.tasks[task_id].frame_valid == 0u)
        || (g_runqueue.tasks[task_id].state != SCHEDULER64_TASK_BLOCKED))
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        ++g_runqueue.block_denials;
#endif
        return 0u;
    }

    task = &g_runqueue.tasks[task_id];
    task->frame.rax = result;
    task->state = SCHEDULER64_TASK_READY;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    ++g_runqueue.wakes;
#endif
    return 1u;
#endif
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
static u32 scheduler64_sleep_start_current_task(
    u32 requested_ticks,
    u64 resume_rax,
    scheduler64_sleep_expiry_callback_t expiry_callback,
    u64 callback_cookie)
{
    u32 task_id;
    u32 start_ticks;
    struct scheduler64_sleep_entry *sleep_entry;

    if (requested_ticks == 0u)
    {
        return 0u;
    }

    task_id = scheduler64_runqueue_current_task_id();
    if (task_id == SCHEDULER64_INVALID_TASK)
    {
        return 0u;
    }

    start_ticks = pit_get_ticks();
    sleep_entry = scheduler64_sleep_free_entry();
    if ((sleep_entry == 0)
        || (scheduler64_runqueue_task_state(task_id) != SCHEDULER64_TASK_RUNNING)
        || (scheduler64_sleep_task_pending(task_id) != 0u))
    {
        ++g_scheduler64_sleep_denial_count;
        return 0u;
    }

    sleep_entry->active = 1u;
    sleep_entry->task_id = task_id;
    sleep_entry->start_tick = start_ticks;
    sleep_entry->wake_tick = start_ticks + requested_ticks;
    sleep_entry->requested_ticks = requested_ticks;
    sleep_entry->resume_rax = resume_rax;
    sleep_entry->expiry_callback = expiry_callback;
    sleep_entry->callback_cookie = callback_cookie;
    g_scheduler64_sleep_last_task_id = task_id;
    g_scheduler64_sleep_last_wake_tick = sleep_entry->wake_tick;
    if (scheduler64_runqueue_block_task(task_id) == 0u)
    {
        scheduler64_clear_sleep_entry(sleep_entry);
        ++g_scheduler64_sleep_denial_count;
        return 0u;
    }

    ++g_scheduler64_sleep_count;
    return 1u;
}
#endif

u32 scheduler64_runqueue_on_timer(struct interrupt_frame64 *frame)
{
    u32 current = g_runqueue.current_task;
    u32 next;
    struct scheduler64_task *current_task;
    struct scheduler64_task *next_task;

    if ((g_runqueue.active == 0u)
        || (current >= g_runqueue.task_count)
        || ((frame->cs & 0x3ull) != 0x3ull))
    {
        return 0u;
    }

    ++g_runqueue.irqs;
    current_task = &g_runqueue.tasks[current];
    current_task->frame = *frame;
    current_task->frame_valid = 1u;
    current_task->saved_rip = frame->rip;
    current_task->saved_rsp = frame->rsp;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    if (current_task->state == SCHEDULER64_TASK_RUNNING)
#endif
    {
        current_task->state = SCHEDULER64_TASK_READY;
    }
    g_runqueue.cs = frame->cs;
    g_runqueue.ss = frame->ss;

#ifdef LIMITLESS_X64_UEFI_KERNEL
    (void)scheduler64_sleep_wake_expired(pit_get_ticks());
#endif
    next = scheduler64_next_runnable(current);
    if (next == SCHEDULER64_INVALID_TASK)
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        if (current_task->state == SCHEDULER64_TASK_READY)
#endif
        {
            current_task->state = SCHEDULER64_TASK_RUNNING;
        }
        return 0u;
    }

    next_task = &g_runqueue.tasks[next];
#ifdef LIMITLESS_X64_UEFI_KERNEL
    if (scheduler64_switch_cr3_for_task(next_task, SCHEDULER64_CR3_REASON_TIMER) == 0u)
    {
        if (current_task->state == SCHEDULER64_TASK_READY)
        {
            current_task->state = SCHEDULER64_TASK_RUNNING;
        }
        return 0u;
    }
#endif
    next_task->state = SCHEDULER64_TASK_RUNNING;
    ++next_task->dispatches;
    g_runqueue.current_task = next;
    ++g_runqueue.switches;
    *frame = next_task->frame;
    g_runqueue.cs = frame->cs;
    g_runqueue.ss = frame->ss;
    return 1u;
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_runqueue_on_blocked_syscall(struct interrupt_frame64 *frame)
{
    u32 current = g_runqueue.current_task;
    u32 next;
    struct scheduler64_task *current_task;
    struct scheduler64_task *next_task;

    if ((g_runqueue.active == 0u)
        || (current >= g_runqueue.task_count)
        || (frame == 0))
    {
        return 0u;
    }

    current_task = &g_runqueue.tasks[current];
    if ((current_task->frame_valid == 0u)
        || (current_task->state != SCHEDULER64_TASK_BLOCKED)
        || ((frame->cs & 0x3ull) != 0x3ull))
    {
        return 0u;
    }

    current_task->frame = *frame;
    current_task->frame_valid = 1u;
    current_task->saved_rip = frame->rip;
    current_task->saved_rsp = frame->rsp;
    g_runqueue.cs = frame->cs;
    g_runqueue.ss = frame->ss;

    next = scheduler64_next_runnable(current);
    if (next == SCHEDULER64_INVALID_TASK)
    {
        ++g_runqueue.block_denials;
        return 0u;
    }

    next_task = &g_runqueue.tasks[next];
    /*
     * M22 does not switch CR3 on ordinary syscall entry: every process root
     * shares the higher-half kernel mapping, so syscall handlers can run on
     * the current process CR3. Scheduler-mediated returns switch CR3 only when
     * a different user task is selected.
     */
    if (scheduler64_switch_cr3_for_task(
            next_task,
            SCHEDULER64_CR3_REASON_BLOCKED_SYSCALL) == 0u)
    {
        ++g_runqueue.block_denials;
        return 0u;
    }
    next_task->state = SCHEDULER64_TASK_RUNNING;
    ++next_task->dispatches;
    g_runqueue.current_task = next;
    ++g_runqueue.switches;
    *frame = next_task->frame;
    g_runqueue.cs = frame->cs;
    g_runqueue.ss = frame->ss;
    return 1u;
}
#endif

u32 scheduler64_runqueue_on_exit(struct interrupt_frame64 *frame, u32 result)
{
    u32 current = g_runqueue.current_task;
    u32 next;
    struct scheduler64_task *task;

    if ((g_runqueue.active == 0u) || (current >= g_runqueue.task_count))
    {
        return SCHEDULER64_RUNQUEUE_EXIT_NONE;
    }

    task = &g_runqueue.tasks[current];
    task->state = SCHEDULER64_TASK_EXITED;
    task->result = result;
    ++task->exits;
    ++g_runqueue.exits;
    g_runqueue.cs = frame->cs;
    g_runqueue.ss = frame->ss;

    next = scheduler64_next_runnable(current);
    if (next != SCHEDULER64_INVALID_TASK)
    {
        struct scheduler64_task *next_task = &g_runqueue.tasks[next];
#ifdef LIMITLESS_X64_UEFI_KERNEL
        if (scheduler64_switch_cr3_for_task(next_task, SCHEDULER64_CR3_REASON_EXIT_RESUME) == 0u)
        {
            scheduler64_switch_cr3_to_kernel(SCHEDULER64_CR3_REASON_EXIT_KERNEL);
            g_runqueue.active = 0u;
            g_runqueue.current_task = SCHEDULER64_INVALID_TASK;
            g_runqueue.result = result;
            return SCHEDULER64_RUNQUEUE_EXIT_COMPLETE;
        }
#endif
        next_task->state = SCHEDULER64_TASK_RUNNING;
        ++next_task->dispatches;
        g_runqueue.current_task = next;
        ++g_runqueue.switches;
        *frame = next_task->frame;
        g_runqueue.cs = frame->cs;
        g_runqueue.ss = frame->ss;
        return SCHEDULER64_RUNQUEUE_EXIT_RESUMED;
    }

    g_runqueue.active = 0u;
    g_runqueue.current_task = SCHEDULER64_INVALID_TASK;
    g_runqueue.result = result;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    scheduler64_switch_cr3_to_kernel(SCHEDULER64_CR3_REASON_EXIT_KERNEL);
#endif
    return SCHEDULER64_RUNQUEUE_EXIT_COMPLETE;
}

u32 scheduler64_sleep_for_ticks(u32 requested_ticks)
{
    u64 guard = 0ull;
    u64 guard_limit;
    u32 start_ticks;
    u32 elapsed_ticks;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u32 task_id;
#endif

    start_ticks = pit_get_ticks();
    g_scheduler64_sleep_last_requested_ticks = requested_ticks;
    g_scheduler64_sleep_last_start_ticks = start_ticks;
    g_scheduler64_sleep_last_end_ticks = start_ticks;
    g_scheduler64_sleep_last_elapsed_ticks = 0u;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    g_scheduler64_sleep_last_task_id = SCHEDULER64_INVALID_TASK;
    g_scheduler64_sleep_last_wake_tick = start_ticks;
#endif

    if (requested_ticks == 0u)
    {
        ++g_scheduler64_sleep_count;
        return 1u;
    }

#ifdef LIMITLESS_X64_UEFI_KERNEL
    task_id = scheduler64_runqueue_current_task_id();
    if (task_id != SCHEDULER64_INVALID_TASK)
    {
        (void)task_id;
        return scheduler64_sleep_start_current_task(requested_ticks, 0ull, 0, 0ull);
    }
#endif

    guard_limit =
        ((u64)requested_ticks * (u64)SCHEDULER64_SLEEP_SPIN_BUDGET_PER_TICK)
        + (u64)SCHEDULER64_SLEEP_SPIN_BUDGET_PER_TICK;
    if (guard_limit > (u64)SCHEDULER64_SLEEP_MAX_SPIN_BUDGET)
    {
        guard_limit = (u64)SCHEDULER64_SLEEP_MAX_SPIN_BUDGET;
    }

    interrupts64_enable();
    while ((((u32)(pit_get_ticks() - start_ticks)) < requested_ticks)
        && (guard < guard_limit))
    {
        scheduler64_cpu_pause();
        ++guard;
    }
    interrupts64_disable();

    g_scheduler64_sleep_last_end_ticks = pit_get_ticks();
    elapsed_ticks = (u32)(g_scheduler64_sleep_last_end_ticks - start_ticks);
    g_scheduler64_sleep_last_elapsed_ticks = elapsed_ticks;
    if (elapsed_ticks >= requested_ticks)
    {
        ++g_scheduler64_sleep_count;
        return 1u;
    }

    ++g_scheduler64_sleep_denial_count;
    return 0u;
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_sleep_current_task_for_ticks(
    u32 requested_ticks,
    u64 resume_rax,
    scheduler64_sleep_expiry_callback_t expiry_callback,
    u64 callback_cookie)
{
    g_scheduler64_sleep_last_requested_ticks = requested_ticks;
    g_scheduler64_sleep_last_start_ticks = pit_get_ticks();
    g_scheduler64_sleep_last_end_ticks = g_scheduler64_sleep_last_start_ticks;
    g_scheduler64_sleep_last_elapsed_ticks = 0u;
    g_scheduler64_sleep_last_task_id = SCHEDULER64_INVALID_TASK;
    g_scheduler64_sleep_last_wake_tick = g_scheduler64_sleep_last_start_ticks;
    return scheduler64_sleep_start_current_task(
        requested_ticks,
        resume_rax,
        expiry_callback,
        callback_cookie);
}

u32 scheduler64_sleep_cancel_task(u32 task_id)
{
    u32 index;
    u32 cancelled = 0u;

    for (index = 0u; index < SCHEDULER64_SLEEP_QUEUE_TASKS; ++index)
    {
        if ((g_scheduler64_sleep_queue[index].active != 0u)
            && (g_scheduler64_sleep_queue[index].task_id == task_id))
        {
            scheduler64_clear_sleep_entry(&g_scheduler64_sleep_queue[index]);
            ++cancelled;
        }
    }

    return cancelled;
}
#else
u32 scheduler64_sleep_current_task_for_ticks(
    u32 requested_ticks,
    u64 resume_rax,
    scheduler64_sleep_expiry_callback_t expiry_callback,
    u64 callback_cookie)
{
    (void)requested_ticks;
    (void)resume_rax;
    (void)expiry_callback;
    (void)callback_cookie;
    return 0u;
}

u32 scheduler64_sleep_cancel_task(u32 task_id)
{
    (void)task_id;
    return 0u;
}
#endif

u32 scheduler64_runqueue_current_pid(void)
{
    if ((g_runqueue.active == 0u)
        || (g_runqueue.current_task >= g_runqueue.task_count))
    {
        return 0u;
    }

    return g_runqueue.tasks[g_runqueue.current_task].pid;
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_runqueue_current_task_id(void)
{
    if ((g_runqueue.active == 0u)
        || (g_runqueue.current_task >= g_runqueue.task_count))
    {
        return SCHEDULER64_INVALID_TASK;
    }

    return g_runqueue.current_task;
}
#endif

u32 scheduler64_runqueue_attempts(void)
{
    return g_runqueue.attempts;
}

u32 scheduler64_runqueue_exits(void)
{
    return g_runqueue.exits;
}

u32 scheduler64_runqueue_irqs(void)
{
    return g_runqueue.irqs;
}

u32 scheduler64_runqueue_switches(void)
{
    return g_runqueue.switches;
}

u32 scheduler64_runqueue_result(void)
{
    return g_runqueue.result;
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_runqueue_block_count(void)
{
    return g_runqueue.blocks;
}

u32 scheduler64_runqueue_wake_count(void)
{
    return g_runqueue.wakes;
}

u32 scheduler64_runqueue_block_denial_count(void)
{
    return g_runqueue.block_denials;
}

u32 scheduler64_runqueue_blocked_count(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0u; index < g_runqueue.task_count; ++index)
    {
        if (g_runqueue.tasks[index].state == SCHEDULER64_TASK_BLOCKED)
        {
            ++count;
        }
    }

    return count;
}

u32 scheduler64_runqueue_task_state(u32 task_id)
{
    if (task_id >= g_runqueue.task_count)
    {
        return SCHEDULER64_TASK_UNUSED;
    }

    return g_runqueue.tasks[task_id].state;
}
#endif

u32 scheduler64_runqueue_task_result(u32 task_id)
{
    if (task_id >= g_runqueue.task_count)
    {
        return 0u;
    }

    return g_runqueue.tasks[task_id].result;
}

u32 scheduler64_runqueue_task_pid(u32 task_id)
{
    if (task_id >= g_runqueue.task_count)
    {
        return 0u;
    }

    return g_runqueue.tasks[task_id].pid;
}

u32 scheduler64_runqueue_task_runtime_token(u32 task_id)
{
    if (task_id >= g_runqueue.task_count)
    {
        return 0u;
    }

    return g_runqueue.tasks[task_id].runtime_token;
}

u32 scheduler64_runqueue_task_entry_token(u32 task_id)
{
    if (task_id >= g_runqueue.task_count)
    {
        return 0u;
    }

    return g_runqueue.tasks[task_id].entry_token;
}

u64 scheduler64_runqueue_task_rip(u32 task_id)
{
    if (task_id >= g_runqueue.task_count)
    {
        return 0u;
    }

    if (g_runqueue.tasks[task_id].saved_rip != 0u)
    {
        return g_runqueue.tasks[task_id].saved_rip;
    }

    return g_runqueue.tasks[task_id].initial_rip;
}

u64 scheduler64_runqueue_task_rsp(u32 task_id)
{
    if (task_id >= g_runqueue.task_count)
    {
        return 0u;
    }

    if (g_runqueue.tasks[task_id].saved_rsp != 0u)
    {
        return g_runqueue.tasks[task_id].saved_rsp;
    }

    return g_runqueue.tasks[task_id].initial_rsp;
}

u64 scheduler64_runqueue_cs(void)
{
    return g_runqueue.cs;
}

u64 scheduler64_runqueue_ss(void)
{
    return g_runqueue.ss;
}

u32 scheduler64_sleep_count(void)
{
    return g_scheduler64_sleep_count;
}

u32 scheduler64_sleep_denial_count(void)
{
    return g_scheduler64_sleep_denial_count;
}

u32 scheduler64_sleep_last_requested_ticks(void)
{
    return g_scheduler64_sleep_last_requested_ticks;
}

u32 scheduler64_sleep_last_elapsed_ticks(void)
{
    return g_scheduler64_sleep_last_elapsed_ticks;
}

u32 scheduler64_sleep_last_start_ticks(void)
{
    return g_scheduler64_sleep_last_start_ticks;
}

u32 scheduler64_sleep_last_end_ticks(void)
{
    return g_scheduler64_sleep_last_end_ticks;
}

#ifdef LIMITLESS_X64_UEFI_KERNEL
u32 scheduler64_sleep_pending_count(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0u; index < SCHEDULER64_SLEEP_QUEUE_TASKS; ++index)
    {
        if (g_scheduler64_sleep_queue[index].active != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 scheduler64_sleep_wake_count(void)
{
    return g_scheduler64_sleep_wake_count;
}

u32 scheduler64_sleep_last_task_id(void)
{
    return g_scheduler64_sleep_last_task_id;
}

u32 scheduler64_sleep_last_wake_tick(void)
{
    return g_scheduler64_sleep_last_wake_tick;
}
#endif
