#include "persona_audit_x64.h"

#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"

/*
 * D.5 adds the first lightweight persona audit ring. It integrates with
 * process_x64.h only through the audit attach/detach/accessor APIs, with
 * persona_x64.h to stamp each record with the active persona type, and with
 * pit.h for a monotonic checkpoint token. The scaffold checkpoint proves that
 * valid records are appended in chronological order, denial/unimplemented
 * events carry their real ABI-facing result codes, and release clears the
 * PCB audit slot without granting ambient authority.
 */

static persona_audit64_context_t g_persona_audit64_contexts[PERSONA_AUDIT64_MAX_CONTEXTS];
static u32 g_persona_audit64_context_used[PERSONA_AUDIT64_MAX_CONTEXTS];
static u32 g_persona_audit64_initialized = 0u;
static u32 g_persona_audit64_sequence = 1u;

static void persona_audit64_clear_record(persona_audit64_record_t *record)
{
    if (record == 0)
    {
        return;
    }

    record->timestamp = 0ull;
    record->pid = PROCESS64_INVALID_PID;
    record->persona_type = (u8)PERSONA64_TYPE_COUNT;
    record->event_type = 0u;
    record->event_code = 0u;
    record->result = 0u;
    record->rip = 0ull;
}

static void persona_audit64_clear_context(persona_audit64_context_t *context)
{
    u32 index;

    if (context == 0)
    {
        return;
    }

    context->pid = PROCESS64_INVALID_PID;
    context->write_index = 0u;
    context->count = 0u;
    context->dropped_count = 0u;

    for (index = 0u; index < PERSONA_AUDIT64_RING_CAPACITY; ++index)
    {
        persona_audit64_clear_record(&context->records[index]);
    }
}

static persona_audit64_context_t *persona_audit64_acquire_context(u32 pid)
{
    u32 index;

    for (index = 0u; index < PERSONA_AUDIT64_MAX_CONTEXTS; ++index)
    {
        if (g_persona_audit64_context_used[index] == 0u)
        {
            g_persona_audit64_context_used[index] = 1u;
            persona_audit64_clear_context(&g_persona_audit64_contexts[index]);
            g_persona_audit64_contexts[index].pid = pid;
            return &g_persona_audit64_contexts[index];
        }
    }

    return 0;
}

static void persona_audit64_release_context(persona_audit64_context_t *context)
{
    u32 index;

    if (context == 0)
    {
        return;
    }

    for (index = 0u; index < PERSONA_AUDIT64_MAX_CONTEXTS; ++index)
    {
        if (&g_persona_audit64_contexts[index] == context)
        {
            persona_audit64_clear_context(context);
            g_persona_audit64_context_used[index] = 0u;
            return;
        }
    }
}

static u64 persona_audit64_timestamp(void)
{
    u32 ticks = pit_get_ticks();
    u32 sequence = g_persona_audit64_sequence++;

    if (g_persona_audit64_sequence == 0u)
    {
        g_persona_audit64_sequence = 1u;
    }

    return ((u64)ticks << 32) | (u64)sequence;
}

void persona_audit64_init(void)
{
    u32 index;

    if (g_persona_audit64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < PERSONA_AUDIT64_MAX_CONTEXTS; ++index)
    {
        g_persona_audit64_context_used[index] = 0u;
        persona_audit64_clear_context(&g_persona_audit64_contexts[index]);
    }

    g_persona_audit64_sequence = 1u;
    g_persona_audit64_initialized = 1u;
}

persona_audit64_context_t *persona_audit64_context_for_process(u32 pid)
{
    persona_audit64_init();
    return (persona_audit64_context_t *)process64_audit_ctx(pid);
}

u32 persona_audit64_attach(u32 pid)
{
    persona_audit64_context_t *context;

    persona_audit64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (process64_audit_ctx(pid) != 0))
    {
        return 0u;
    }

    context = persona_audit64_acquire_context(pid);
    if (context == 0)
    {
        return 0u;
    }

    if (process64_attach_audit(pid, context) == 0u)
    {
        persona_audit64_release_context(context);
        return 0u;
    }

    return 1u;
}

u32 persona_audit64_release(u32 pid)
{
    persona_audit64_context_t *context;
    void *detached;

    persona_audit64_init();

    if ((pid == PROCESS64_INVALID_PID) || (process64_principal(pid) == 0u))
    {
        return 0u;
    }

    context = persona_audit64_context_for_process(pid);
    if (context == 0)
    {
        return 0u;
    }

    detached = process64_detach_audit(pid);
    if (detached != context)
    {
        return 0u;
    }

    persona_audit64_release_context(context);
    return 1u;
}

u32 persona_audit64_record(u32 pid, u8 event_type, u16 event_code, u32 result, u64 rip)
{
    persona_audit64_context_t *context;
    persona_audit64_record_t *record;
    u32 persona_type;

    persona_audit64_init();

    context = persona_audit64_context_for_process(pid);
    if (context == 0)
    {
        return 0u;
    }

    record = &context->records[context->write_index];
    persona_type = persona64_type(pid);
    if (persona_type >= PERSONA64_TYPE_COUNT)
    {
        persona_type = PERSONA64_TYPE_LIMITLESS_NATIVE;
    }

    record->timestamp = persona_audit64_timestamp();
    record->pid = pid;
    record->persona_type = (u8)persona_type;
    record->event_type = event_type;
    record->event_code = event_code;
    record->result = result;
    record->rip = rip;

    context->write_index = (context->write_index + 1u) % PERSONA_AUDIT64_RING_CAPACITY;
    if (context->count < PERSONA_AUDIT64_RING_CAPACITY)
    {
        ++context->count;
    }
    else
    {
        ++context->dropped_count;
    }

    return 1u;
}

u32 persona_audit64_count(u32 pid)
{
    persona_audit64_context_t *context = persona_audit64_context_for_process(pid);

    return (context != 0) ? context->count : 0u;
}

u32 persona_audit64_dropped_count(u32 pid)
{
    persona_audit64_context_t *context = persona_audit64_context_for_process(pid);

    return (context != 0) ? context->dropped_count : 0u;
}

u32 persona_audit64_read(u32 pid, u32 index, persona_audit64_record_t *out_record)
{
    persona_audit64_context_t *context;
    u32 start;
    u32 physical_index;

    persona_audit64_init();

    context = persona_audit64_context_for_process(pid);
    if ((context == 0) || (out_record == 0) || (index >= context->count))
    {
        return 0u;
    }

    start = (context->write_index + PERSONA_AUDIT64_RING_CAPACITY - context->count)
        % PERSONA_AUDIT64_RING_CAPACITY;
    physical_index = (start + index) % PERSONA_AUDIT64_RING_CAPACITY;
    *out_record = context->records[physical_index];

    return 1u;
}

u32 persona_audit64_last_event_type(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? (u32)record.event_type
        : 0u;
}

u32 persona_audit64_last_event_code(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? (u32)record.event_code
        : 0u;
}

u32 persona_audit64_last_result(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? record.result
        : 0u;
}

u32 persona_audit64_last_persona_type(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? (u32)record.persona_type
        : PERSONA64_TYPE_COUNT;
}
