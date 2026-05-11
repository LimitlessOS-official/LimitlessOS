#ifndef LIMITLESS_SCHEDULER_H
#define LIMITLESS_SCHEDULER_H

#include "types.h"

struct scheduler_task;

typedef void (*scheduler_task_callback)(struct scheduler_task *task);

enum scheduler_task_state
{
    SCHEDULER_TASK_READY = 0,
    SCHEDULER_TASK_RUNNING = 1,
    SCHEDULER_TASK_WAITING = 2,
    SCHEDULER_TASK_SLEEPING = 3
};

enum scheduler_task_priority
{
    SCHEDULER_PRIORITY_BACKGROUND = 0,
    SCHEDULER_PRIORITY_LOW = 1,
    SCHEDULER_PRIORITY_NORMAL = 2,
    SCHEDULER_PRIORITY_HIGH = 3,
    SCHEDULER_PRIORITY_CRITICAL = 4
};

struct scheduler_task
{
    u32 id;
    const char *name;
    u32 priority;
    u32 period_ticks;
    u32 next_run_tick;
    u32 ready_since_tick;
    u32 wake_tick;
    u32 run_count;
    u32 last_run_tick;
    u32 urgent_ready;
    u32 state;
    scheduler_task_callback callback;
    void *context;
};

void scheduler_init(void);
struct scheduler_task *scheduler_register_periodic_priority(
    const char *name,
    u32 priority,
    u32 start_delay_ticks,
    u32 period_ticks,
    scheduler_task_callback callback,
    void *context);
struct scheduler_task *scheduler_register_periodic(
    const char *name,
    u32 start_delay_ticks,
    u32 period_ticks,
    scheduler_task_callback callback,
    void *context);
struct scheduler_task *scheduler_register_event_priority(
    const char *name,
    u32 priority,
    u32 start_delay_ticks,
    scheduler_task_callback callback,
    void *context);
struct scheduler_task *scheduler_register_event(
    const char *name,
    u32 start_delay_ticks,
    scheduler_task_callback callback,
    void *context);
void scheduler_on_tick(void);
void scheduler_run_ready(u32 run_budget);
void scheduler_wait_current(void);
void scheduler_sleep_current(u32 ticks);
void scheduler_wake_task(struct scheduler_task *task);
void scheduler_wake_task_urgent(struct scheduler_task *task);
u32 scheduler_task_count(void);
u32 scheduler_get_tick_count(void);
u32 scheduler_total_runs(void);
u32 scheduler_ready_count(void);
u32 scheduler_waiting_count(void);
u32 scheduler_sleeping_count(void);
u32 scheduler_total_fairness_boosts(void);
u32 scheduler_priority_run_count(u32 priority);
u32 scheduler_total_urgent_wakes(void);
const struct scheduler_task *scheduler_current_task(void);

#endif
