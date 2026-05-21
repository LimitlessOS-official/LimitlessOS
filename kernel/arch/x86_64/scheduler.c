#include "scheduler_x64.h"

#include "interrupts_x64.h"
#include "pit.h"
#include "process_x64.h"

enum
{
    SCHEDULER64_RUNQUEUE_TASKS = 4u,
    SCHEDULER64_TASK_UNUSED = 0u,
    SCHEDULER64_TASK_READY = 1u,
    SCHEDULER64_TASK_RUNNING = 2u,
    SCHEDULER64_TASK_EXITED = 3u,
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
    u64 cs;
    u64 ss;
    struct scheduler64_task tasks[SCHEDULER64_RUNQUEUE_TASKS];
};

static struct scheduler64_runqueue g_runqueue;
static u32 g_scheduler64_sleep_count = 0u;
static u32 g_scheduler64_sleep_denial_count = 0u;
static u32 g_scheduler64_sleep_last_requested_ticks = 0u;
static u32 g_scheduler64_sleep_last_elapsed_ticks = 0u;
static u32 g_scheduler64_sleep_last_start_ticks = 0u;
static u32 g_scheduler64_sleep_last_end_ticks = 0u;

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

void scheduler64_init(void)
{
    scheduler64_runqueue_reset();
    g_scheduler64_sleep_count = 0u;
    g_scheduler64_sleep_denial_count = 0u;
    g_scheduler64_sleep_last_requested_ticks = 0u;
    g_scheduler64_sleep_last_elapsed_ticks = 0u;
    g_scheduler64_sleep_last_start_ticks = 0u;
    g_scheduler64_sleep_last_end_ticks = 0u;
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
    g_runqueue.cs = 0u;
    g_runqueue.ss = 0u;
    for (index = 0u; index < SCHEDULER64_RUNQUEUE_TASKS; ++index)
    {
        scheduler64_clear_task(&g_runqueue.tasks[index]);
    }
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
    return 1u;
}

void scheduler64_runqueue_stop(void)
{
    g_runqueue.active = 0u;
    g_runqueue.current_task = SCHEDULER64_INVALID_TASK;
}

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
    current_task->state = SCHEDULER64_TASK_READY;
    g_runqueue.cs = frame->cs;
    g_runqueue.ss = frame->ss;

    next = scheduler64_next_runnable(current);
    if (next == SCHEDULER64_INVALID_TASK)
    {
        current_task->state = SCHEDULER64_TASK_RUNNING;
        return 0u;
    }

    next_task = &g_runqueue.tasks[next];
    next_task->state = SCHEDULER64_TASK_RUNNING;
    ++next_task->dispatches;
    g_runqueue.current_task = next;
    ++g_runqueue.switches;
    *frame = next_task->frame;
    g_runqueue.cs = frame->cs;
    g_runqueue.ss = frame->ss;
    return 1u;
}

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
    return SCHEDULER64_RUNQUEUE_EXIT_COMPLETE;
}

u32 scheduler64_sleep_for_ticks(u32 requested_ticks)
{
    u64 guard = 0ull;
    u64 guard_limit;
    u32 start_ticks;
    u32 elapsed_ticks;

    start_ticks = pit_get_ticks();
    g_scheduler64_sleep_last_requested_ticks = requested_ticks;
    g_scheduler64_sleep_last_start_ticks = start_ticks;
    g_scheduler64_sleep_last_end_ticks = start_ticks;
    g_scheduler64_sleep_last_elapsed_ticks = 0u;

    if (requested_ticks == 0u)
    {
        ++g_scheduler64_sleep_count;
        return 1u;
    }

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

u32 scheduler64_runqueue_current_pid(void)
{
    if ((g_runqueue.active == 0u)
        || (g_runqueue.current_task >= g_runqueue.task_count))
    {
        return 0u;
    }

    return g_runqueue.tasks[g_runqueue.current_task].pid;
}

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
