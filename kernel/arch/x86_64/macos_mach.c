#include "macos_mach_x64.h"

#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "services.h"
#include "services_x64.h"
#include "vma_x64.h"

/*
 * N.2 adds the macOS Mach trap switchboard and a first brokered Mach-port
 * substrate. It integrates with macos_abi.c for negative syscall dispatch,
 * persona_x64.h for MACOS_MACHO validation, capability_x64.h/services_x64.h
 * for endpoint-backed port authority, vma_x64.h/paging_x64.h for verified
 * user message buffers, and persona_audit_x64.h for translated, denied, and
 * unavailable trap records. The checkpoint proves task/thread/host/reply
 * self ports, capability-backed send/receive via mach_msg_trap, invalid-port
 * denial, unimplemented trap audit, and full port cleanup.
 */

typedef struct macos_mach64_port
{
    u32 active;
    u32 pid;
    u32 name;
    u32 kind;
    u32 rights;
    u32 endpoint_class;
    u32 endpoint_id;
    u32 capability_handle;
    u32 pending;
    macos_mach64_msg_header_t header;
    u8 body[MACOS_MACH64_INLINE_BODY_BYTES];
    u32 body_bytes;
    u32 checksum;
} macos_mach64_port_t;

static macos_mach64_trap_handler_t
    g_macos_mach64_trap_table[MACOS_MACH64_TRAP_TABLE_SIZE];
static macos_mach64_port_t g_macos_mach64_ports[MACOS_MACH64_MAX_PORTS];
static u32 g_macos_mach64_initialized = 0u;
static u32 g_macos_mach64_next_port_name = MACOS_MACH64_PORT_NAME_BASE;
static u32 g_macos_mach64_dispatch_count = 0u;
static u32 g_macos_mach64_unimplemented_count = 0u;
static u32 g_macos_mach64_mach_msg_count = 0u;
static u32 g_macos_mach64_port_create_count = 0u;
static u32 g_macos_mach64_send_count = 0u;
static u32 g_macos_mach64_receive_count = 0u;
static u32 g_macos_mach64_denial_count = 0u;
static u32 g_macos_mach64_fault_count = 0u;
static u32 g_macos_mach64_last_trap = 0u;
static u32 g_macos_mach64_last_result = MACOS_MACH64_KERN_SUCCESS;
static u32 g_macos_mach64_last_port = 0u;
static u32 g_macos_mach64_last_remote_port = 0u;
static u32 g_macos_mach64_last_local_port = 0u;
static u32 g_macos_mach64_last_message_id = 0u;
static u32 g_macos_mach64_last_message_checksum = 0u;
static u32 g_macos_mach64_last_send_size = 0u;
static u32 g_macos_mach64_last_receive_size = 0u;
static u32 g_macos_mach64_last_backing_endpoint = 0xFFFFFFFFu;
static u32 g_macos_mach64_last_backing_capability = CAPABILITY64_INVALID_HANDLE;

static u32 macos_mach64_trap_index(s32 trap_number)
{
    return (trap_number < 0) ? (u32)(0 - trap_number) : MACOS_MACH64_TRAP_TABLE_SIZE;
}

static void macos_mach64_clear_port(macos_mach64_port_t *port)
{
    u32 index;

    if (port == 0)
    {
        return;
    }

    port->active = 0u;
    port->pid = PROCESS64_INVALID_PID;
    port->name = 0u;
    port->kind = 0u;
    port->rights = 0u;
    port->endpoint_class = SERVICE_ENDPOINT_CLASS_NONE;
    port->endpoint_id = 0xFFFFFFFFu;
    port->capability_handle = CAPABILITY64_INVALID_HANDLE;
    port->pending = 0u;
    port->header.msgh_bits = 0u;
    port->header.msgh_size = 0u;
    port->header.msgh_remote_port = 0u;
    port->header.msgh_local_port = 0u;
    port->header.msgh_voucher_port = 0u;
    port->header.msgh_id = 0;
    port->body_bytes = 0u;
    port->checksum = 0u;
    for (index = 0u; index < MACOS_MACH64_INLINE_BODY_BYTES; ++index)
    {
        port->body[index] = 0u;
    }
}

static u32 macos_mach64_range_overflows(u64 address, u64 byte_count)
{
    return ((byte_count != 0ull) && ((address + byte_count) < address)) ? 1u : 0u;
}

static u32 macos_mach64_user_buffer_readable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }
    if ((address == 0ull)
        || (macos_mach64_range_overflows(address, (u64)byte_count) != 0u))
    {
        return 0u;
    }

    cursor = address;
    end = address + (u64)byte_count;
    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 page = cursor & ~((u64)VMA64_PAGE_BYTES - 1ull);
        u64 next_page = page + (u64)VMA64_PAGE_BYTES;
        u64 next = (next_page < end) ? next_page : end;

        if ((region == 0)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end)
            || ((region->prot_flags & VMA64_PROT_READ) == 0u)
            || (paging64_user_page_present(page) == 0u)
            || ((paging64_user_page_protection(page) & PAGING64_USER_PROT_READ) == 0u))
        {
            return 0u;
        }
        if (next > region->virt_end)
        {
            next = region->virt_end;
        }
        if (next <= cursor)
        {
            return 0u;
        }
        cursor = next;
    }

    return 1u;
}

static u32 macos_mach64_user_buffer_writable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }
    if ((address == 0ull)
        || (macos_mach64_range_overflows(address, (u64)byte_count) != 0u))
    {
        return 0u;
    }

    cursor = address;
    end = address + (u64)byte_count;
    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 page = cursor & ~((u64)VMA64_PAGE_BYTES - 1ull);
        u64 next_page = page + (u64)VMA64_PAGE_BYTES;
        u64 next = (next_page < end) ? next_page : end;

        if ((region == 0)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end)
            || ((region->prot_flags & VMA64_PROT_WRITE) == 0u)
            || (paging64_user_page_present(page) == 0u)
            || ((paging64_user_page_protection(page) & PAGING64_USER_PROT_WRITE) == 0u))
        {
            return 0u;
        }
        if (next > region->virt_end)
        {
            next = region->virt_end;
        }
        if (next <= cursor)
        {
            return 0u;
        }
        cursor = next;
    }

    return 1u;
}

static void macos_mach64_copy_from_user(u8 *target, u64 source, u32 byte_count)
{
    u32 index;

    if ((target == 0) || (source == 0ull))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = ((volatile const u8 *)(u64)source)[index];
    }
}

static void macos_mach64_copy_to_user(u64 target, const u8 *source, u32 byte_count)
{
    u32 index;

    if ((target == 0ull) || (source == 0))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        ((volatile u8 *)(u64)target)[index] = source[index];
    }
}

static u32 macos_mach64_checksum(const u8 *data, u32 byte_count)
{
    u32 checksum = 0x4D414348u;
    u32 index;

    if (data == 0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = (checksum << 5) ^ (checksum >> 2) ^ (u32)data[index];
    }

    return checksum;
}

static void macos_mach64_note_result(s32 trap_number, u32 result)
{
    g_macos_mach64_last_trap = macos_mach64_trap_index(trap_number);
    g_macos_mach64_last_result = result;
}

static u32 macos_mach64_valid_persona(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_MACOS_MACHO))
        ? 1u
        : 0u;
}

static u64 macos_mach64_record_result(
    u32 pid,
    s32 trap_number,
    u32 result,
    u64 rip,
    u8 event_type)
{
    macos_mach64_note_result(trap_number, result);
    (void)persona_audit64_record(
        pid,
        event_type,
        (u16)macos_mach64_trap_index(trap_number),
        result,
        rip);
    return (u64)result;
}

static u64 macos_mach64_deny(u32 pid, s32 trap_number, u32 result, u64 rip)
{
    ++g_macos_mach64_denial_count;
    return macos_mach64_record_result(
        pid,
        trap_number,
        result,
        rip,
        PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED);
}

static u64 macos_mach64_fault(u32 pid, s32 trap_number, u64 rip)
{
    ++g_macos_mach64_fault_count;
    return macos_mach64_record_result(
        pid,
        trap_number,
        MACOS_MACH64_KERN_INVALID_ADDRESS,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED);
}

static u64 macos_mach64_ok(u32 pid, s32 trap_number, u64 value, u64 rip)
{
    macos_mach64_note_result(trap_number, MACOS_MACH64_KERN_SUCCESS);
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)macos_mach64_trap_index(trap_number),
        MACOS_MACH64_KERN_SUCCESS,
        rip);
    return value;
}

static macos_mach64_port_t *macos_mach64_find_port(u32 pid, u32 port_name)
{
    u32 index;

    for (index = 0u; index < MACOS_MACH64_MAX_PORTS; ++index)
    {
        if ((g_macos_mach64_ports[index].active != 0u)
            && (g_macos_mach64_ports[index].pid == pid)
            && (g_macos_mach64_ports[index].name == port_name))
        {
            return &g_macos_mach64_ports[index];
        }
    }

    return 0;
}

static macos_mach64_port_t *macos_mach64_find_kind(u32 pid, u32 kind)
{
    u32 index;

    for (index = 0u; index < MACOS_MACH64_MAX_PORTS; ++index)
    {
        if ((g_macos_mach64_ports[index].active != 0u)
            && (g_macos_mach64_ports[index].pid == pid)
            && (g_macos_mach64_ports[index].kind == kind))
        {
            return &g_macos_mach64_ports[index];
        }
    }

    return 0;
}

static u32 macos_mach64_required_capability_right(u32 mach_right)
{
    if ((mach_right & MACOS_MACH64_PORT_RIGHT_SEND) != 0u)
    {
        return CAPABILITY64_RIGHT_SEND;
    }

    return CAPABILITY64_RIGHT_QUERY;
}

static macos_mach64_port_t *macos_mach64_find_port_with_right(
    u32 pid,
    u32 port_name,
    u32 mach_right)
{
    macos_mach64_port_t *port = macos_mach64_find_port(pid, port_name);
    u32 endpoint;

    if ((port == 0) || ((port->rights & mach_right) != mach_right))
    {
        return 0;
    }

    endpoint = capability64_route(
        port->capability_handle,
        macos_mach64_required_capability_right(mach_right),
        process64_principal(pid));
    if (endpoint == 0xFFFFFFFFu)
    {
        return 0;
    }

    port->endpoint_id = endpoint;
    g_macos_mach64_last_backing_endpoint = endpoint;
    g_macos_mach64_last_backing_capability = port->capability_handle;
    return port;
}

static macos_mach64_port_t *macos_mach64_allocate_port(
    u32 pid,
    u32 kind,
    u32 rights)
{
    macos_mach64_port_t *existing = macos_mach64_find_kind(pid, kind);
    u32 owner;
    u32 index;
    u32 capability_handle;

    if (existing != 0)
    {
        return existing;
    }

    owner = process64_principal(pid);
    if ((owner == 0u) || (rights == 0u))
    {
        return 0;
    }

    capability_handle = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INIT,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        owner);
    if (capability_handle == CAPABILITY64_INVALID_HANDLE)
    {
        return 0;
    }

    for (index = 0u; index < MACOS_MACH64_MAX_PORTS; ++index)
    {
        if (g_macos_mach64_ports[index].active == 0u)
        {
            macos_mach64_clear_port(&g_macos_mach64_ports[index]);
            g_macos_mach64_ports[index].active = 1u;
            g_macos_mach64_ports[index].pid = pid;
            g_macos_mach64_ports[index].name = g_macos_mach64_next_port_name++;
            if (g_macos_mach64_next_port_name < MACOS_MACH64_PORT_NAME_BASE)
            {
                g_macos_mach64_next_port_name = MACOS_MACH64_PORT_NAME_BASE;
            }
            g_macos_mach64_ports[index].kind = kind;
            g_macos_mach64_ports[index].rights = rights;
            g_macos_mach64_ports[index].endpoint_class = SERVICE_ENDPOINT_CLASS_INIT;
            g_macos_mach64_ports[index].endpoint_id =
                services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INIT);
            g_macos_mach64_ports[index].capability_handle = capability_handle;
            ++g_macos_mach64_port_create_count;
            return &g_macos_mach64_ports[index];
        }
    }

    (void)capability64_revoke(capability_handle, owner);
    return 0;
}

static u64 macos_mach64_self_port(
    u32 pid,
    s32 trap_number,
    u32 kind,
    u32 rights,
    u64 rip)
{
    macos_mach64_port_t *port;

    if (macos_mach64_valid_persona(pid) == 0u)
    {
        return macos_mach64_deny(
            pid,
            trap_number,
            MACOS_MACH64_KERN_INVALID_ARGUMENT,
            rip);
    }

    port = macos_mach64_allocate_port(pid, kind, rights);
    if (port == 0)
    {
        return macos_mach64_deny(pid, trap_number, MACOS_MACH64_KERN_NO_SPACE, rip);
    }

    g_macos_mach64_last_port = port->name;
    g_macos_mach64_last_backing_endpoint = port->endpoint_id;
    g_macos_mach64_last_backing_capability = port->capability_handle;
    return macos_mach64_ok(pid, trap_number, (u64)port->name, rip);
}

static u32 macos_mach64_read_header(
    u32 pid,
    u64 user_message,
    u32 send_size,
    macos_mach64_msg_header_t *header_out)
{
    if ((header_out == 0)
        || (send_size < MACOS_MACH64_MSG_HEADER_BYTES)
        || (send_size > MACOS_MACH64_MAX_MESSAGE_BYTES)
        || (macos_mach64_user_buffer_readable(pid, user_message, send_size) == 0u))
    {
        return 0u;
    }

    macos_mach64_copy_from_user(
        (u8 *)header_out,
        user_message,
        MACOS_MACH64_MSG_HEADER_BYTES);
    return (header_out->msgh_size == send_size) ? 1u : 0u;
}

static u32 macos_mach64_send_message(u32 pid, u64 user_message, u32 send_size)
{
    macos_mach64_msg_header_t header;
    macos_mach64_port_t *remote_port;
    u32 body_bytes;

    if (macos_mach64_read_header(pid, user_message, send_size, &header) == 0u)
    {
        return MACOS_MACH64_MACH_SEND_INVALID_DATA;
    }

    remote_port = macos_mach64_find_port_with_right(
        pid,
        header.msgh_remote_port,
        MACOS_MACH64_PORT_RIGHT_SEND);
    if (remote_port == 0)
    {
        return MACOS_MACH64_MACH_SEND_INVALID_DEST;
    }
    if (remote_port->pending != 0u)
    {
        return MACOS_MACH64_MACH_SEND_TIMED_OUT;
    }

    body_bytes = send_size - MACOS_MACH64_MSG_HEADER_BYTES;
    if (body_bytes > MACOS_MACH64_INLINE_BODY_BYTES)
    {
        return MACOS_MACH64_MACH_SEND_INVALID_DATA;
    }

    remote_port->header = header;
    remote_port->body_bytes = body_bytes;
    if (body_bytes != 0u)
    {
        macos_mach64_copy_from_user(
            remote_port->body,
            user_message + (u64)MACOS_MACH64_MSG_HEADER_BYTES,
            body_bytes);
    }
    remote_port->checksum = macos_mach64_checksum((const u8 *)&remote_port->header, sizeof(header));
    remote_port->checksum ^= macos_mach64_checksum(remote_port->body, body_bytes);
    remote_port->pending = 1u;
    ++g_macos_mach64_send_count;
    g_macos_mach64_last_port = remote_port->name;
    g_macos_mach64_last_remote_port = header.msgh_remote_port;
    g_macos_mach64_last_local_port = header.msgh_local_port;
    g_macos_mach64_last_message_id = (u32)header.msgh_id;
    g_macos_mach64_last_message_checksum = remote_port->checksum;
    g_macos_mach64_last_send_size = send_size;
    return MACOS_MACH64_MACH_MSG_SUCCESS;
}

static u32 macos_mach64_receive_message(
    u32 pid,
    u64 user_message,
    u32 receive_size,
    u32 receive_name)
{
    macos_mach64_port_t *port = macos_mach64_find_port_with_right(
        pid,
        receive_name,
        MACOS_MACH64_PORT_RIGHT_RECEIVE);
    u32 total_size;

    if (port == 0)
    {
        return MACOS_MACH64_MACH_RCV_INVALID_NAME;
    }
    if (port->pending == 0u)
    {
        return MACOS_MACH64_MACH_RCV_TIMED_OUT;
    }

    total_size = MACOS_MACH64_MSG_HEADER_BYTES + port->body_bytes;
    if (receive_size < total_size)
    {
        return MACOS_MACH64_MACH_RCV_TOO_LARGE;
    }
    if (macos_mach64_user_buffer_writable(pid, user_message, total_size) == 0u)
    {
        return MACOS_MACH64_KERN_INVALID_ADDRESS;
    }

    port->header.msgh_local_port = receive_name;
    macos_mach64_copy_to_user(
        user_message,
        (const u8 *)&port->header,
        MACOS_MACH64_MSG_HEADER_BYTES);
    if (port->body_bytes != 0u)
    {
        macos_mach64_copy_to_user(
            user_message + (u64)MACOS_MACH64_MSG_HEADER_BYTES,
            port->body,
            port->body_bytes);
    }

    ++g_macos_mach64_receive_count;
    g_macos_mach64_last_port = port->name;
    g_macos_mach64_last_remote_port = port->header.msgh_remote_port;
    g_macos_mach64_last_local_port = port->header.msgh_local_port;
    g_macos_mach64_last_message_id = (u32)port->header.msgh_id;
    g_macos_mach64_last_message_checksum = port->checksum;
    g_macos_mach64_last_receive_size = total_size;
    port->pending = 0u;
    return MACOS_MACH64_MACH_MSG_SUCCESS;
}

static u64 macos_mach64_mach_msg(
    u32 pid,
    u64 user_message,
    u64 option,
    u64 send_size,
    u64 receive_size,
    u64 receive_name,
    u64 timeout,
    u64 rip)
{
    u32 result = MACOS_MACH64_MACH_MSG_SUCCESS;

    (void)timeout;

    if (macos_mach64_valid_persona(pid) == 0u)
    {
        return macos_mach64_deny(
            pid,
            MACOS_MACH64_TRAP_MACH_MSG,
            MACOS_MACH64_KERN_INVALID_ARGUMENT,
            rip);
    }
    if ((option == 0ull)
        || ((option & ~(u64)MACOS_MACH64_MSG_OPTION_ALLOWED) != 0ull)
        || (send_size > 0xFFFFFFFFull)
        || (receive_size > 0xFFFFFFFFull)
        || (receive_name > 0xFFFFFFFFull))
    {
        return macos_mach64_deny(
            pid,
            MACOS_MACH64_TRAP_MACH_MSG,
            MACOS_MACH64_KERN_INVALID_ARGUMENT,
            rip);
    }

    ++g_macos_mach64_mach_msg_count;
    if ((option & (u64)MACOS_MACH64_MSG_OPTION_SEND) != 0ull)
    {
        result = macos_mach64_send_message(pid, user_message, (u32)send_size);
        if (result != MACOS_MACH64_MACH_MSG_SUCCESS)
        {
            return macos_mach64_deny(pid, MACOS_MACH64_TRAP_MACH_MSG, result, rip);
        }
    }
    if ((option & (u64)MACOS_MACH64_MSG_OPTION_RCV) != 0ull)
    {
        result = macos_mach64_receive_message(
            pid,
            user_message,
            (u32)receive_size,
            (u32)receive_name);
        if (result == MACOS_MACH64_KERN_INVALID_ADDRESS)
        {
            return macos_mach64_fault(pid, MACOS_MACH64_TRAP_MACH_MSG, rip);
        }
        if (result != MACOS_MACH64_MACH_MSG_SUCCESS)
        {
            return macos_mach64_deny(pid, MACOS_MACH64_TRAP_MACH_MSG, result, rip);
        }
    }

    return macos_mach64_ok(pid, MACOS_MACH64_TRAP_MACH_MSG, 0ull, rip);
}

static u64 macos_mach64_unimplemented_stub(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_mach64_record_result(
        pid,
        -1,
        MACOS_MACH64_KERN_INVALID_ARGUMENT,
        rip,
        PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED);
}

static u64 macos_mach64_mach_msg_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    return macos_mach64_mach_msg(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

static u64 macos_mach64_task_self_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_mach64_self_port(
        pid,
        MACOS_MACH64_TRAP_TASK_SELF,
        MACOS_MACH64_PORT_KIND_TASK,
        MACOS_MACH64_PORT_RIGHT_SEND | MACOS_MACH64_PORT_RIGHT_QUERY,
        rip);
}

static u64 macos_mach64_thread_self_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_mach64_self_port(
        pid,
        MACOS_MACH64_TRAP_THREAD_SELF,
        MACOS_MACH64_PORT_KIND_THREAD,
        MACOS_MACH64_PORT_RIGHT_SEND | MACOS_MACH64_PORT_RIGHT_QUERY,
        rip);
}

static u64 macos_mach64_host_self_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_mach64_self_port(
        pid,
        MACOS_MACH64_TRAP_HOST_SELF,
        MACOS_MACH64_PORT_KIND_HOST,
        MACOS_MACH64_PORT_RIGHT_SEND | MACOS_MACH64_PORT_RIGHT_QUERY,
        rip);
}

static u64 macos_mach64_reply_port_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return macos_mach64_self_port(
        pid,
        MACOS_MACH64_TRAP_MACH_REPLY_PORT,
        MACOS_MACH64_PORT_KIND_REPLY,
        MACOS_MACH64_PORT_RIGHT_SEND
            | MACOS_MACH64_PORT_RIGHT_RECEIVE
            | MACOS_MACH64_PORT_RIGHT_QUERY,
        rip);
}

void macos_mach64_init(void)
{
    u32 index;

    if (g_macos_mach64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < MACOS_MACH64_TRAP_TABLE_SIZE; ++index)
    {
        g_macos_mach64_trap_table[index] = macos_mach64_unimplemented_stub;
    }
    for (index = 0u; index < MACOS_MACH64_MAX_PORTS; ++index)
    {
        macos_mach64_clear_port(&g_macos_mach64_ports[index]);
    }

    g_macos_mach64_trap_table[macos_mach64_trap_index(MACOS_MACH64_TRAP_MACH_MSG)] =
        macos_mach64_mach_msg_dispatch;
    g_macos_mach64_trap_table[macos_mach64_trap_index(MACOS_MACH64_TRAP_TASK_SELF)] =
        macos_mach64_task_self_dispatch;
    g_macos_mach64_trap_table[macos_mach64_trap_index(MACOS_MACH64_TRAP_THREAD_SELF)] =
        macos_mach64_thread_self_dispatch;
    g_macos_mach64_trap_table[macos_mach64_trap_index(MACOS_MACH64_TRAP_MACH_REPLY_PORT)] =
        macos_mach64_reply_port_dispatch;
    g_macos_mach64_trap_table[macos_mach64_trap_index(MACOS_MACH64_TRAP_HOST_SELF)] =
        macos_mach64_host_self_dispatch;

    g_macos_mach64_next_port_name = MACOS_MACH64_PORT_NAME_BASE;
    g_macos_mach64_initialized = 1u;
}

macos_mach64_trap_handler_t *macos_mach64_trap_table(void)
{
    if (g_macos_mach64_initialized == 0u)
    {
        macos_mach64_init();
    }

    return g_macos_mach64_trap_table;
}

u64 macos_mach64_dispatch(
    u32 pid,
    s32 trap_number,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    u32 index;
    macos_mach64_trap_handler_t handler;

    if (g_macos_mach64_initialized == 0u)
    {
        macos_mach64_init();
    }

    ++g_macos_mach64_dispatch_count;
    index = macos_mach64_trap_index(trap_number);
    g_macos_mach64_last_trap = index;
    if (index >= MACOS_MACH64_TRAP_TABLE_SIZE)
    {
        ++g_macos_mach64_unimplemented_count;
        macos_mach64_note_result(trap_number, MACOS_MACH64_KERN_INVALID_ARGUMENT);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED,
            (u16)(index & 0xFFFFu),
            MACOS_MACH64_KERN_INVALID_ARGUMENT,
            rip);
        return (u64)MACOS_MACH64_KERN_INVALID_ARGUMENT;
    }

    handler = g_macos_mach64_trap_table[index];
    if (handler == macos_mach64_unimplemented_stub)
    {
        ++g_macos_mach64_unimplemented_count;
        macos_mach64_note_result(trap_number, MACOS_MACH64_KERN_INVALID_ARGUMENT);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED,
            (u16)index,
            MACOS_MACH64_KERN_INVALID_ARGUMENT,
            rip);
        return (u64)MACOS_MACH64_KERN_INVALID_ARGUMENT;
    }

    return handler(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

u32 macos_mach64_release_process(u32 pid)
{
    u32 index;
    u32 released = 0u;
    u32 owner = process64_principal(pid);

    if (g_macos_mach64_initialized == 0u)
    {
        macos_mach64_init();
    }

    for (index = 0u; index < MACOS_MACH64_MAX_PORTS; ++index)
    {
        if ((g_macos_mach64_ports[index].active != 0u)
            && (g_macos_mach64_ports[index].pid == pid))
        {
            if ((owner != 0u)
                && (g_macos_mach64_ports[index].capability_handle
                    != CAPABILITY64_INVALID_HANDLE))
            {
                (void)capability64_revoke(g_macos_mach64_ports[index].capability_handle, owner);
            }
            macos_mach64_clear_port(&g_macos_mach64_ports[index]);
            ++released;
        }
    }

    return released;
}

u32 macos_mach64_table_size(void) { return MACOS_MACH64_TRAP_TABLE_SIZE; }

u32 macos_mach64_unimplemented_entry_count(void)
{
    u32 index;
    u32 count = 0u;

    if (g_macos_mach64_initialized == 0u)
    {
        macos_mach64_init();
    }

    for (index = 0u; index < MACOS_MACH64_TRAP_TABLE_SIZE; ++index)
    {
        if (g_macos_mach64_trap_table[index] == macos_mach64_unimplemented_stub)
        {
            ++count;
        }
    }

    return count;
}

u32 macos_mach64_entry_installed(s32 trap_number)
{
    u32 index;

    if (g_macos_mach64_initialized == 0u)
    {
        macos_mach64_init();
    }

    index = macos_mach64_trap_index(trap_number);
    return ((index < MACOS_MACH64_TRAP_TABLE_SIZE)
        && (g_macos_mach64_trap_table[index] != macos_mach64_unimplemented_stub))
        ? 1u
        : 0u;
}

u32 macos_mach64_mach_msg_entry_installed(void)
{
    return macos_mach64_entry_installed(MACOS_MACH64_TRAP_MACH_MSG);
}

u32 macos_mach64_task_self_entry_installed(void)
{
    return macos_mach64_entry_installed(MACOS_MACH64_TRAP_TASK_SELF);
}

u32 macos_mach64_thread_self_entry_installed(void)
{
    return macos_mach64_entry_installed(MACOS_MACH64_TRAP_THREAD_SELF);
}

u32 macos_mach64_host_self_entry_installed(void)
{
    return macos_mach64_entry_installed(MACOS_MACH64_TRAP_HOST_SELF);
}

u32 macos_mach64_reply_port_entry_installed(void)
{
    return macos_mach64_entry_installed(MACOS_MACH64_TRAP_MACH_REPLY_PORT);
}

u32 macos_mach64_live_port_count(u32 pid)
{
    u32 index;
    u32 count = 0u;

    if (g_macos_mach64_initialized == 0u)
    {
        macos_mach64_init();
    }

    for (index = 0u; index < MACOS_MACH64_MAX_PORTS; ++index)
    {
        if ((g_macos_mach64_ports[index].active != 0u)
            && (g_macos_mach64_ports[index].pid == pid))
        {
            ++count;
        }
    }

    return count;
}

u32 macos_mach64_total_live_port_count(void)
{
    u32 index;
    u32 count = 0u;

    if (g_macos_mach64_initialized == 0u)
    {
        macos_mach64_init();
    }

    for (index = 0u; index < MACOS_MACH64_MAX_PORTS; ++index)
    {
        if (g_macos_mach64_ports[index].active != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 macos_mach64_port_backing_endpoint(u32 pid, u32 port_name)
{
    macos_mach64_port_t *port = macos_mach64_find_port(pid, port_name);

    return (port != 0) ? port->endpoint_id : 0xFFFFFFFFu;
}

u32 macos_mach64_port_backing_capability(u32 pid, u32 port_name)
{
    macos_mach64_port_t *port = macos_mach64_find_port(pid, port_name);

    return (port != 0) ? port->capability_handle : CAPABILITY64_INVALID_HANDLE;
}

u32 macos_mach64_port_rights(u32 pid, u32 port_name)
{
    macos_mach64_port_t *port = macos_mach64_find_port(pid, port_name);

    return (port != 0) ? port->rights : 0u;
}

u32 macos_mach64_port_kind(u32 pid, u32 port_name)
{
    macos_mach64_port_t *port = macos_mach64_find_port(pid, port_name);

    return (port != 0) ? port->kind : 0u;
}

u32 macos_mach64_port_pending_count(u32 pid, u32 port_name)
{
    macos_mach64_port_t *port = macos_mach64_find_port(pid, port_name);

    return (port != 0) ? port->pending : 0u;
}

u32 macos_mach64_dispatch_count(void) { return g_macos_mach64_dispatch_count; }
u32 macos_mach64_unimplemented_count(void) { return g_macos_mach64_unimplemented_count; }
u32 macos_mach64_mach_msg_count(void) { return g_macos_mach64_mach_msg_count; }
u32 macos_mach64_port_create_count(void) { return g_macos_mach64_port_create_count; }
u32 macos_mach64_send_count(void) { return g_macos_mach64_send_count; }
u32 macos_mach64_receive_count(void) { return g_macos_mach64_receive_count; }
u32 macos_mach64_denial_count(void) { return g_macos_mach64_denial_count; }
u32 macos_mach64_fault_count(void) { return g_macos_mach64_fault_count; }
u32 macos_mach64_last_trap(void) { return g_macos_mach64_last_trap; }
u32 macos_mach64_last_result(void) { return g_macos_mach64_last_result; }
u32 macos_mach64_last_port(void) { return g_macos_mach64_last_port; }
u32 macos_mach64_last_remote_port(void) { return g_macos_mach64_last_remote_port; }
u32 macos_mach64_last_local_port(void) { return g_macos_mach64_last_local_port; }
u32 macos_mach64_last_message_id(void) { return g_macos_mach64_last_message_id; }
u32 macos_mach64_last_message_checksum(void) { return g_macos_mach64_last_message_checksum; }
u32 macos_mach64_last_send_size(void) { return g_macos_mach64_last_send_size; }
u32 macos_mach64_last_receive_size(void) { return g_macos_mach64_last_receive_size; }
u32 macos_mach64_last_backing_endpoint(void) { return g_macos_mach64_last_backing_endpoint; }
u32 macos_mach64_last_backing_capability(void) { return g_macos_mach64_last_backing_capability; }
