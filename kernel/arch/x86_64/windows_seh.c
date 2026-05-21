#include "windows_seh_x64.h"

#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * I.6 adds the first Windows-persona SEH exception dispatch substrate. It
 * integrates with persona_x64.h for per-process Windows PE tagging,
 * vma_x64.h for user-memory validation, persona_audit_x64.h for truthful
 * exception records, and interrupts.c for ring-3 fault handoff. The scaffold
 * checkpoint proves a divide-by-zero frame is redirected to the first handler
 * in the TEB SEH chain, and that invalid chain/user-memory input is denied
 * without fabricating success or bypassing the persona boundary.
 */

static u32 g_windows_seh64_initialized = 0u;
static u32 g_windows_seh64_dispatch_count = 0u;
static u32 g_windows_seh64_denial_count = 0u;
static u32 g_windows_seh64_unhandled_count = 0u;
static u32 g_windows_seh64_last_result = WINDOWS_SEH64_STATUS_SUCCESS;
static u32 g_windows_seh64_last_vector = 0u;
static u32 g_windows_seh64_last_chain_depth = 0u;
static u64 g_windows_seh64_last_teb = 0ull;
static u64 g_windows_seh64_last_registration = 0ull;
static u64 g_windows_seh64_last_handler = 0ull;
static u64 g_windows_seh64_last_frame = 0ull;
static u64 g_windows_seh64_last_exception_record = 0ull;
static u64 g_windows_seh64_last_context_record = 0ull;
static u64 g_windows_seh64_last_dispatcher_context = 0ull;

static void windows_seh64_clear_last(void)
{
    g_windows_seh64_last_result = WINDOWS_SEH64_STATUS_SUCCESS;
    g_windows_seh64_last_vector = 0u;
    g_windows_seh64_last_chain_depth = 0u;
    g_windows_seh64_last_teb = 0ull;
    g_windows_seh64_last_registration = 0ull;
    g_windows_seh64_last_handler = 0ull;
    g_windows_seh64_last_frame = 0ull;
    g_windows_seh64_last_exception_record = 0ull;
    g_windows_seh64_last_context_record = 0ull;
    g_windows_seh64_last_dispatcher_context = 0ull;
}

void windows_seh64_init(void)
{
    if (g_windows_seh64_initialized != 0u)
    {
        return;
    }

    g_windows_seh64_dispatch_count = 0u;
    g_windows_seh64_denial_count = 0u;
    g_windows_seh64_unhandled_count = 0u;
    windows_seh64_clear_last();
    g_windows_seh64_initialized = 1u;
}

static u32 windows_seh64_user_address_ok(u64 address, u32 byte_count)
{
    u64 end;

    if ((address == 0ull) || (byte_count == 0u))
    {
        return 0u;
    }

    end = address + (u64)byte_count;
    if ((end <= address) || (end > WINDOWS_SEH64_USER_TOP))
    {
        return 0u;
    }

    return 1u;
}

static u32 windows_seh64_user_range_allowed(
    u32 pid,
    u64 address,
    u32 byte_count,
    u32 required_prot)
{
    u64 cursor;
    u64 end;

    if (windows_seh64_user_address_ok(address, byte_count) == 0u)
    {
        return 0u;
    }

    cursor = address;
    end = address + (u64)byte_count;
    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 next;

        if ((region == 0)
            || ((region->prot_flags & required_prot) != required_prot)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end))
        {
            return 0u;
        }

        next = (region->virt_end < end) ? region->virt_end : end;
        if (next <= cursor)
        {
            return 0u;
        }
        cursor = next;
    }

    return 1u;
}

static u32 windows_seh64_exception_code_from_vector(u64 vector)
{
    if (vector == 0ull)
    {
        return WINDOWS_SEH64_STATUS_INTEGER_DIVIDE_BY_ZERO;
    }
    if (vector == 6ull)
    {
        return WINDOWS_SEH64_STATUS_ILLEGAL_INSTRUCTION;
    }

    return WINDOWS_SEH64_STATUS_ACCESS_VIOLATION;
}

static void windows_seh64_audit(u32 pid, const struct interrupt_frame64 *frame, u32 result)
{
    u64 rip = (frame != 0) ? frame->rip : 0ull;
    u16 event_code = (frame != 0) ? (u16)frame->vector : WINDOWS_SEH64_EVENT_CODE_DISPATCH;

    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_CRASH,
        event_code,
        result,
        rip);
}

static u32 windows_seh64_deny(
    u32 pid,
    const struct interrupt_frame64 *frame,
    u32 result,
    u32 audited)
{
    ++g_windows_seh64_denial_count;
    g_windows_seh64_last_result = result;
    g_windows_seh64_last_vector = (frame != 0) ? (u32)frame->vector : 0u;
    if (audited != 0u)
    {
        windows_seh64_audit(pid, frame, result);
    }
    return 0u;
}

static void windows_seh64_write_user_frame(
    volatile windows_seh64_user_frame_t *user_frame,
    const struct interrupt_frame64 *frame,
    u64 registration,
    u64 handler,
    u32 exception_code,
    u32 chain_depth)
{
    user_frame->exception_record.code = exception_code;
    user_frame->exception_record.flags = 0u;
    user_frame->exception_record.address = frame->rip;
    user_frame->exception_record.information0 = frame->error_code;
    user_frame->exception_record.information1 = frame->vector;

    user_frame->context_record.rip = frame->rip;
    user_frame->context_record.rsp = frame->rsp;
    user_frame->context_record.rflags = frame->rflags;
    user_frame->context_record.rax = frame->rax;
    user_frame->context_record.rcx = frame->rcx;
    user_frame->context_record.rdx = frame->rdx;
    user_frame->context_record.r8 = frame->r8;
    user_frame->context_record.r9 = frame->r9;
    user_frame->context_record.r10 = frame->r10;
    user_frame->context_record.r11 = frame->r11;

    user_frame->dispatcher_context.registration = registration;
    user_frame->dispatcher_context.handler = handler;
    user_frame->dispatcher_context.exception_vector = (u32)frame->vector;
    user_frame->dispatcher_context.chain_depth = chain_depth;
    user_frame->return_address = 0ull;
}

u32 windows_seh64_dispatch_exception(u32 pid, struct interrupt_frame64 *frame)
{
    persona_context_t *context;
    u64 teb;
    u64 registration;
    u32 depth;

    windows_seh64_init();
    windows_seh64_clear_last();

    if ((pid == PROCESS64_INVALID_PID)
        || (frame == 0)
        || ((frame->cs & 0x3ull) != 0x3ull)
        || (persona64_type(pid) != PERSONA64_TYPE_WINDOWS_PE))
    {
        return windows_seh64_deny(pid, frame, WINDOWS_SEH64_STATUS_NOT_IMPLEMENTED, 0u);
    }

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        return windows_seh64_deny(pid, frame, WINDOWS_SEH64_STATUS_NOT_IMPLEMENTED, 0u);
    }

    teb = (context->windows_teb_base != 0ull)
        ? context->windows_teb_base
        : context->tls_base;
    g_windows_seh64_last_teb = teb;
    if (windows_seh64_user_range_allowed(pid, teb, sizeof(u64), VMA64_PROT_READ) == 0u)
    {
        return windows_seh64_deny(pid, frame, WINDOWS_SEH64_STATUS_INVALID_PARAMETER, 1u);
    }

    registration = *(volatile u64 *)(u64)teb;
    for (depth = 1u; depth <= WINDOWS_SEH64_MAX_CHAIN_DEPTH; ++depth)
    {
        volatile windows_seh64_registration_t *entry;
        u64 handler;

        if ((registration == 0ull) || (registration == WINDOWS_SEH64_CHAIN_END))
        {
            ++g_windows_seh64_unhandled_count;
            g_windows_seh64_last_result = WINDOWS_SEH64_STATUS_UNHANDLED_EXCEPTION;
            g_windows_seh64_last_vector = (u32)frame->vector;
            g_windows_seh64_last_chain_depth = depth - 1u;
            windows_seh64_audit(pid, frame, WINDOWS_SEH64_STATUS_UNHANDLED_EXCEPTION);
            return 0u;
        }

        g_windows_seh64_last_registration = registration;
        g_windows_seh64_last_chain_depth = depth;
        if (windows_seh64_user_range_allowed(
                pid,
                registration,
                (u32)sizeof(windows_seh64_registration_t),
                VMA64_PROT_READ) == 0u)
        {
            return windows_seh64_deny(pid, frame, WINDOWS_SEH64_STATUS_INVALID_PARAMETER, 1u);
        }

        entry = (volatile windows_seh64_registration_t *)(u64)registration;
        handler = entry->handler;
        if ((handler != 0ull)
            && (windows_seh64_user_range_allowed(pid, handler, 1u, VMA64_PROT_EXECUTE) != 0u))
        {
            u32 exception_code = windows_seh64_exception_code_from_vector(frame->vector);
            u64 user_frame_address =
                (frame->rsp - (u64)sizeof(windows_seh64_user_frame_t))
                & ~(WINDOWS_SEH64_FRAME_ALIGN - 1ull);
            volatile windows_seh64_user_frame_t *user_frame;

            if (windows_seh64_user_range_allowed(
                    pid,
                    user_frame_address,
                    (u32)sizeof(windows_seh64_user_frame_t),
                    VMA64_PROT_READ | VMA64_PROT_WRITE) == 0u)
            {
                return windows_seh64_deny(pid, frame, WINDOWS_SEH64_STATUS_ACCESS_VIOLATION, 1u);
            }

            user_frame = (volatile windows_seh64_user_frame_t *)(u64)user_frame_address;
            windows_seh64_write_user_frame(
                user_frame,
                frame,
                registration,
                handler,
                exception_code,
                depth);

            g_windows_seh64_last_result = WINDOWS_SEH64_STATUS_SUCCESS;
            g_windows_seh64_last_vector = (u32)frame->vector;
            g_windows_seh64_last_handler = handler;
            g_windows_seh64_last_frame = user_frame_address;
            g_windows_seh64_last_exception_record = user_frame_address;
            g_windows_seh64_last_context_record =
                user_frame_address + (u64)sizeof(windows_seh64_exception_record_t);
            g_windows_seh64_last_dispatcher_context =
                g_windows_seh64_last_context_record
                + (u64)sizeof(windows_seh64_context_record_t);

            windows_seh64_audit(pid, frame, WINDOWS_SEH64_STATUS_SUCCESS);

            frame->rcx = g_windows_seh64_last_exception_record;
            frame->rdx = registration;
            frame->r8 = g_windows_seh64_last_context_record;
            frame->r9 = g_windows_seh64_last_dispatcher_context;
            frame->rip = handler;
            frame->rsp = user_frame_address;

            ++g_windows_seh64_dispatch_count;
            return 1u;
        }

        registration = entry->next;
    }

    ++g_windows_seh64_unhandled_count;
    g_windows_seh64_last_result = WINDOWS_SEH64_STATUS_UNHANDLED_EXCEPTION;
    g_windows_seh64_last_vector = (u32)frame->vector;
    windows_seh64_audit(pid, frame, WINDOWS_SEH64_STATUS_UNHANDLED_EXCEPTION);
    return 0u;
}

u32 windows_seh64_dispatch_count(void)
{
    windows_seh64_init();
    return g_windows_seh64_dispatch_count;
}

u32 windows_seh64_denial_count(void)
{
    windows_seh64_init();
    return g_windows_seh64_denial_count;
}

u32 windows_seh64_unhandled_count(void)
{
    windows_seh64_init();
    return g_windows_seh64_unhandled_count;
}

u32 windows_seh64_last_result(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_result;
}

u32 windows_seh64_last_vector(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_vector;
}

u32 windows_seh64_last_chain_depth(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_chain_depth;
}

u64 windows_seh64_last_teb(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_teb;
}

u64 windows_seh64_last_registration(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_registration;
}

u64 windows_seh64_last_handler(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_handler;
}

u64 windows_seh64_last_frame(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_frame;
}

u64 windows_seh64_last_exception_record(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_exception_record;
}

u64 windows_seh64_last_context_record(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_context_record;
}

u64 windows_seh64_last_dispatcher_context(void)
{
    windows_seh64_init();
    return g_windows_seh64_last_dispatcher_context;
}
