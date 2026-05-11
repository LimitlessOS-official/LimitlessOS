#include "scheduler.h"

#include "klog.h"
#include "types.h"

enum
{
    SCHEDULER_MAX_TASKS = 16,
    SCHEDULER_PRIORITY_LEVELS = 5,
    SCHEDULER_PRIORITY_WEIGHT = 4u,
    SCHEDULER_FAIRNESS_MAX_AGE = 15u
};

static struct scheduler_task tasks[SCHEDULER_MAX_TASKS];
static u32 task_count = 0;
static u32 scheduler_ticks = 0;
static u32 total_task_runs = 0;
static u32 total_fairness_boosts = 0;
static u32 total_urgent_wakes = 0;
static u32 priority_run_counts[SCHEDULER_PRIORITY_LEVELS];
static u32 scheduler_cursor = 0;
static struct scheduler_task *current_task = NULL;

static u32 scheduler_priority_sanitize(u32 priority)
{
    if (priority >= SCHEDULER_PRIORITY_LEVELS)
    {
        return SCHEDULER_PRIORITY_NORMAL;
    }

    return priority;
}

static void scheduler_mark_ready(struct scheduler_task *task, u32 next_run_tick)
{
    if (task == NULL)
    {
        return;
    }

    task->state = SCHEDULER_TASK_READY;
    task->wake_tick = 0u;
    task->next_run_tick = next_run_tick;
    task->ready_since_tick = (scheduler_ticks >= next_run_tick)
        ? scheduler_ticks
        : next_run_tick;
    task->urgent_ready = 0u;
}

static u32 scheduler_age_bonus(const struct scheduler_task *task)
{
    u32 age_ticks;

    if ((task == NULL) || (scheduler_ticks <= task->ready_since_tick))
    {
        return 0u;
    }

    age_ticks = scheduler_ticks - task->ready_since_tick;
    if (age_ticks > SCHEDULER_FAIRNESS_MAX_AGE)
    {
        age_ticks = SCHEDULER_FAIRNESS_MAX_AGE;
    }

    return age_ticks;
}

static struct scheduler_task *scheduler_register_common(
    const char *name,
    u32 priority,
    u32 start_delay_ticks,
    u32 period_ticks,
    scheduler_task_callback callback,
    void *context)
{
    struct scheduler_task *task;

    if ((task_count >= SCHEDULER_MAX_TASKS) || (callback == NULL))
    {
        return NULL;
    }

    task = &tasks[task_count];
    task->id = task_count + 1u;
    task->name = name;
    task->priority = scheduler_priority_sanitize(priority);
    task->period_ticks = period_ticks;
    task->next_run_tick = scheduler_ticks + start_delay_ticks;
    task->ready_since_tick = task->next_run_tick;
    task->wake_tick = 0;
    task->run_count = 0;
    task->last_run_tick = 0;
    task->urgent_ready = 0u;
    task->state = SCHEDULER_TASK_READY;
    task->callback = callback;
    task->context = context;
    ++task_count;
    return task;
}

static void scheduler_wake_sleepers(void)
{
    u32 index;

    for (index = 0; index < task_count; ++index)
    {
        if ((tasks[index].state == SCHEDULER_TASK_SLEEPING)
            && (tasks[index].wake_tick != 0u)
            && (tasks[index].wake_tick <= scheduler_ticks))
        {
            scheduler_mark_ready(&tasks[index], scheduler_ticks);
        }
    }
}

static void scheduler_zero(void *address, u32 size)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0; index < size; ++index)
    {
        bytes[index] = 0;
    }
}

void scheduler_init(void)
{
    scheduler_zero(tasks, sizeof(tasks));
    task_count = 0;
    scheduler_ticks = 0;
    total_task_runs = 0;
    total_fairness_boosts = 0;
    total_urgent_wakes = 0;
    scheduler_zero(priority_run_counts, sizeof(priority_run_counts));
    scheduler_cursor = 0;
    current_task = NULL;
}

struct scheduler_task *scheduler_register_periodic_priority(
    const char *name,
    u32 priority,
    u32 start_delay_ticks,
    u32 period_ticks,
    scheduler_task_callback callback,
    void *context)
{
    if (period_ticks == 0u)
    {
        period_ticks = 1u;
    }

    return scheduler_register_common(name, priority, start_delay_ticks, period_ticks, callback, context);
}

struct scheduler_task *scheduler_register_periodic(
    const char *name,
    u32 start_delay_ticks,
    u32 period_ticks,
    scheduler_task_callback callback,
    void *context)
{
    return scheduler_register_periodic_priority(
        name,
        SCHEDULER_PRIORITY_NORMAL,
        start_delay_ticks,
        period_ticks,
        callback,
        context);
}

struct scheduler_task *scheduler_register_event_priority(
    const char *name,
    u32 priority,
    u32 start_delay_ticks,
    scheduler_task_callback callback,
    void *context)
{
    return scheduler_register_common(name, priority, start_delay_ticks, 0u, callback, context);
}

struct scheduler_task *scheduler_register_event(
    const char *name,
    u32 start_delay_ticks,
    scheduler_task_callback callback,
    void *context)
{
    return scheduler_register_event_priority(
        name,
        SCHEDULER_PRIORITY_NORMAL,
        start_delay_ticks,
        callback,
        context);
}

void scheduler_on_tick(void)
{
    ++scheduler_ticks;
    scheduler_wake_sleepers();
}

void scheduler_run_ready(u32 run_budget)
{
    u32 runs = 0;

    while (runs < run_budget)
    {
        u32 index;
        u32 best_index = 0u;
        u32 best_score = 0u;
        u32 best_age_bonus = 0u;
        u32 max_ready_priority = 0u;
        u32 best_urgent_priority = 0u;
        u32 best_urgent_age_bonus = 0u;
        int found = 0;
        int found_urgent = 0;

        for (index = 0; index < task_count; ++index)
        {
            u32 score;
            u32 age_bonus;
            struct scheduler_task *candidate;
            u32 search_index = (scheduler_cursor + index) % task_count;

            candidate = &tasks[search_index];

            if ((candidate->callback == NULL)
                || (candidate->state != SCHEDULER_TASK_READY)
                || (scheduler_ticks < candidate->next_run_tick))
            {
                continue;
            }

            age_bonus = scheduler_age_bonus(candidate);

            if (candidate->priority > max_ready_priority)
            {
                max_ready_priority = candidate->priority;
            }

            if (candidate->urgent_ready != 0u)
            {
                if (!found_urgent
                    || (candidate->priority > best_urgent_priority)
                    || ((candidate->priority == best_urgent_priority)
                        && (age_bonus > best_urgent_age_bonus)))
                {
                    best_index = search_index;
                    best_urgent_priority = candidate->priority;
                    best_urgent_age_bonus = age_bonus;
                    found_urgent = 1;
                }

                continue;
            }

            if (found_urgent)
            {
                continue;
            }

            score = (candidate->priority * SCHEDULER_PRIORITY_WEIGHT) + age_bonus;

            if (!found || (score > best_score))
            {
                best_index = search_index;
                best_score = score;
                best_age_bonus = age_bonus;
                found = 1;
            }
        }

        if (found_urgent)
        {
            found = 1;
        }

        if (found)
        {
            struct scheduler_task *task = &tasks[best_index];

            if ((task->priority < max_ready_priority) && (best_age_bonus != 0u))
            {
                ++total_fairness_boosts;
            }

            scheduler_cursor = (best_index + 1u) % task_count;
            current_task = task;
            current_task->state = SCHEDULER_TASK_RUNNING;
            current_task->urgent_ready = 0u;
            task->run_count += 1u;
            task->last_run_tick = scheduler_ticks;
            ++priority_run_counts[task->priority];

            if (total_task_runs < 6u)
            {
                /* Log the first few picks so boot traces show priority ordering. */
                klog_write_string("[scheduler] run ");
                klog_write_string(task->name);
                klog_write_string(" p");
                klog_write_dec_u32(task->priority);
                klog_write_string(" age ");
                klog_write_dec_u32(found_urgent ? best_urgent_age_bonus : best_age_bonus);
                if (found_urgent)
                {
                    klog_write_string(" urgent");
                }
                klog_newline();
            }

            task->callback(task);

            if (task->state == SCHEDULER_TASK_RUNNING)
            {
                if (task->period_ticks == 0u)
                {
                    task->state = SCHEDULER_TASK_WAITING;
                    task->ready_since_tick = 0u;
                }
                else
                {
                    scheduler_mark_ready(task, scheduler_ticks + task->period_ticks);
                }
            }

            current_task = NULL;
            ++total_task_runs;
            ++runs;
            continue;
        }

        if (!found)
        {
            break;
        }
    }
}

void scheduler_wait_current(void)
{
    if (current_task != NULL)
    {
        current_task->state = SCHEDULER_TASK_WAITING;
        current_task->wake_tick = 0u;
        current_task->ready_since_tick = 0u;
        current_task->urgent_ready = 0u;
    }
}

void scheduler_sleep_current(u32 ticks)
{
    if (current_task == NULL)
    {
        return;
    }

    if (ticks == 0u)
    {
        ticks = 1u;
    }

    current_task->state = SCHEDULER_TASK_SLEEPING;
    current_task->wake_tick = scheduler_ticks + ticks;
    current_task->next_run_tick = current_task->wake_tick;
    current_task->ready_since_tick = current_task->wake_tick;
    current_task->urgent_ready = 0u;
}

void scheduler_wake_task(struct scheduler_task *task)
{
    if ((task == NULL) || (task->callback == NULL) || (task->state == SCHEDULER_TASK_RUNNING))
    {
        return;
    }

    scheduler_mark_ready(task, scheduler_ticks);
}

void scheduler_wake_task_urgent(struct scheduler_task *task)
{
    if ((task == NULL) || (task->callback == NULL) || (task->state == SCHEDULER_TASK_RUNNING))
    {
        return;
    }

    scheduler_mark_ready(task, scheduler_ticks);
    if (task->urgent_ready == 0u)
    {
        ++total_urgent_wakes;
    }

    task->urgent_ready = 1u;
}

u32 scheduler_task_count(void)
{
    return task_count;
}

u32 scheduler_get_tick_count(void)
{
    return scheduler_ticks;
}

u32 scheduler_total_runs(void)
{
    return total_task_runs;
}

u32 scheduler_ready_count(void)
{
    u32 index;
    u32 count = 0;

    for (index = 0; index < task_count; ++index)
    {
        if (tasks[index].state == SCHEDULER_TASK_READY)
        {
            ++count;
        }
    }

    return count;
}

u32 scheduler_waiting_count(void)
{
    u32 index;
    u32 count = 0;

    for (index = 0; index < task_count; ++index)
    {
        if (tasks[index].state == SCHEDULER_TASK_WAITING)
        {
            ++count;
        }
    }

    return count;
}

u32 scheduler_sleeping_count(void)
{
    u32 index;
    u32 count = 0;

    for (index = 0; index < task_count; ++index)
    {
        if (tasks[index].state == SCHEDULER_TASK_SLEEPING)
        {
            ++count;
        }
    }

    return count;
}

u32 scheduler_total_fairness_boosts(void)
{
    return total_fairness_boosts;
}

u32 scheduler_priority_run_count(u32 priority)
{
    if (priority >= SCHEDULER_PRIORITY_LEVELS)
    {
        return 0u;
    }

    return priority_run_counts[priority];
}

u32 scheduler_total_urgent_wakes(void)
{
    return total_urgent_wakes;
}

const struct scheduler_task *scheduler_current_task(void)
{
    return current_task;
}
