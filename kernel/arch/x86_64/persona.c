#include "persona_x64.h"

#include "process_x64.h"
#include "syscall_x64.h"
#include "windows_abi_x64.h"

/*
 * D.1-D.3 add the first per-process persona context block and format sniffer.
 * The code
 * integrates with process_x64.h only through PID-based attach/detach/accessor
 * APIs and with syscall_x64.h by binding native processes to the existing
 * LimitlessOS syscall dispatcher. I.1 adds the initial Linux signal state to
 * the persona context: pending and masked signal bitsets plus a 64-slot
 * sigaction table. The scaffold checkpoint proves native context attachment,
 * inherited VMA/FD/audit pointers, clean release, and denial for invalid or
 * duplicate binding without changing process behavior, strict magic-byte
 * detection for ELF, PE, Mach-O, shebang, native APP descriptors, malformed
 * unknown inputs, and zero-initialized Linux signal state for new Linux
 * personas. K.1 wires Windows PE persona contexts to the Windows NT ABI
 * switchboard by default while preserving explicit table injection for later
 * test shims.
 */

static persona_context_t g_persona64_contexts[PERSONA64_MAX_CONTEXTS];
static u32 g_persona64_context_used[PERSONA64_MAX_CONTEXTS];
static u32 g_persona64_initialized = 0u;
static u32 g_persona64_native_count = 0u;
static u32 g_persona64_denial_count = 0u;

static void persona64_clear_context(persona_context_t *context)
{
    u32 index;

    if (context == 0)
    {
        return;
    }

    context->persona_type = PERSONA64_TYPE_COUNT;
    context->pid = PROCESS64_INVALID_PID;
    context->syscall_dispatch_table = 0;
    context->vma_root = 0;
    context->fd_table = 0;
    context->tls_base = PERSONA64_TLS_UNSET;
    context->tls_size = 0ull;
    context->clear_child_tid = 0ull;
    context->brk_base = PERSONA64_BRK_UNSET;
    context->brk_current = PERSONA64_BRK_UNSET;
    context->heap_cap = PERSONA64_HEAP_CAP_NONE;
    context->persona_module_handle = PERSONA64_MODULE_NONE;
    context->audit_context = 0;
    context->capability_attenuation_mask = 0u;
    context->load_bias_low = 0u;
    context->windows_exception_table_base = 0ull;
    context->windows_exception_table_bytes = 0ull;
    context->windows_exception_function_count = 0u;
    context->windows_exception_table_checksum = 0u;
    context->windows_teb_base = 0ull;
    context->windows_peb_base = 0ull;
    context->windows_stack_base = 0ull;
    context->windows_stack_limit = 0ull;
    context->windows_tls_pointer = 0ull;
    context->windows_image_base = 0ull;
    context->windows_process_parameters = 0ull;
    context->windows_os_major = 0u;
    context->windows_os_minor = 0u;
    context->windows_os_build = 0u;
    context->windows_nt_global_flag = 0u;
    context->windows_kuser_shared_data_base = 0ull;
    context->windows_kuser_shared_data_updates = 0u;
    context->windows_kuser_shared_data_checksum = 0u;
    context->windows_security_cookie_address = 0ull;
    context->windows_security_cookie_value = 0ull;
    context->windows_security_cookie_checksum = 0u;
    context->windows_entry_rip = 0ull;
    context->windows_entry_rsp = 0ull;
    context->windows_entry_arg_rcx = 0ull;
    context->windows_entry_arg_rdx = 0ull;
    context->windows_entry_arg_r8 = 0ull;
    context->windows_entry_transfer_ready = 0u;
    context->windows_handle_table = 0;
    context->linux_cwd_length = 0u;
    for (index = 0u; index < PERSONA64_LINUX_CWD_MAX_BYTES; ++index)
    {
        context->linux_cwd[index] = 0u;
    }
    context->linux_signal_pending = LINUX_SIGNAL64_PENDING_NONE;
    context->linux_signal_mask = LINUX_SIGNAL64_MASK_EMPTY;
    for (index = 0u; index < LINUX_SIGNAL64_MAX_SIGNALS; ++index)
    {
        context->linux_sigactions[index].handler = LINUX_SIGNAL64_DEFAULT_HANDLER;
        context->linux_sigactions[index].sa_mask = LINUX_SIGNAL64_MASK_EMPTY;
        context->linux_sigactions[index].sa_flags = LINUX_SIGNAL64_FLAGS_NONE;
    }
}

static persona_context_t *persona64_acquire_context(u32 pid)
{
    u32 index;

    for (index = 0u; index < PERSONA64_MAX_CONTEXTS; ++index)
    {
        if (g_persona64_context_used[index] == 0u)
        {
            g_persona64_context_used[index] = 1u;
            persona64_clear_context(&g_persona64_contexts[index]);
            g_persona64_contexts[index].pid = pid;
            return &g_persona64_contexts[index];
        }
    }

    return 0;
}

static void persona64_release_context(persona_context_t *context)
{
    u32 index;

    if (context == 0)
    {
        return;
    }

    for (index = 0u; index < PERSONA64_MAX_CONTEXTS; ++index)
    {
        if (&g_persona64_contexts[index] == context)
        {
            persona64_clear_context(&g_persona64_contexts[index]);
            g_persona64_context_used[index] = 0u;
            return;
        }
    }
}

static u32 persona64_read_le32(const u8 *data)
{
    return ((u32)data[0])
        | ((u32)data[1] << 8)
        | ((u32)data[2] << 16)
        | ((u32)data[3] << 24);
}

static u32 persona64_read_be32(const u8 *data)
{
    return ((u32)data[3])
        | ((u32)data[2] << 8)
        | ((u32)data[1] << 16)
        | ((u32)data[0] << 24);
}

static u32 persona64_contains_bytes(
    const u8 *data,
    u32 size,
    const char *needle,
    u32 needle_size)
{
    u32 offset;
    u32 index;

    if ((data == 0) || (needle == 0) || (needle_size == 0u) || (size < needle_size))
    {
        return 0u;
    }

    for (offset = 0u; offset <= (size - needle_size); ++offset)
    {
        u32 match = 1u;

        for (index = 0u; index < needle_size; ++index)
        {
            if (data[offset + index] != (u8)needle[index])
            {
                match = 0u;
                break;
            }
        }

        if (match != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 persona64_detect_native_app_descriptor(const u8 *data, u32 size)
{
    return ((persona64_contains_bytes(data, size, "name=", 5u) != 0u)
        && (persona64_contains_bytes(data, size, "binary=", 7u) != 0u)
        && (persona64_contains_bytes(data, size, "payload-slot=", 13u) != 0u))
        ? 1u
        : 0u;
}

void persona64_init(void)
{
    u32 index;

    if (g_persona64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < PERSONA64_MAX_CONTEXTS; ++index)
    {
        g_persona64_context_used[index] = 0u;
        persona64_clear_context(&g_persona64_contexts[index]);
    }

    g_persona64_native_count = 0u;
    g_persona64_denial_count = 0u;
    g_persona64_initialized = 1u;
}

persona_context_t *persona64_context_for_process(u32 pid)
{
    persona64_init();
    return (persona_context_t *)process64_persona_ctx(pid);
}

u32 persona64_init_native(u32 pid)
{
    persona_context_t *context;

    persona64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (process64_persona_ctx(pid) != 0))
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    context = persona64_acquire_context(pid);
    if (context == 0)
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    context->persona_type = PERSONA64_TYPE_LIMITLESS_NATIVE;
    context->syscall_dispatch_table = (void *)syscall64_native_dispatch;
    context->vma_root = process64_vma_root(pid);
    context->fd_table = process64_fd_table(pid);
    context->tls_base = PERSONA64_TLS_UNSET;
    context->tls_size = 0ull;
    context->clear_child_tid = 0ull;
    context->brk_base = PERSONA64_BRK_UNSET;
    context->brk_current = PERSONA64_BRK_UNSET;
    context->heap_cap = PERSONA64_HEAP_CAP_NONE;
    context->persona_module_handle = PERSONA64_MODULE_NONE;
    context->audit_context = process64_audit_ctx(pid);
    context->capability_attenuation_mask = 0u;
    context->load_bias_low = 0u;
    context->windows_exception_table_base = 0ull;
    context->windows_exception_table_bytes = 0ull;
    context->windows_exception_function_count = 0u;
    context->windows_exception_table_checksum = 0u;
    context->windows_teb_base = 0ull;
    context->windows_peb_base = 0ull;
    context->windows_stack_base = 0ull;
    context->windows_stack_limit = 0ull;
    context->windows_tls_pointer = 0ull;
    context->windows_image_base = 0ull;
    context->windows_process_parameters = 0ull;
    context->windows_os_major = 0u;
    context->windows_os_minor = 0u;
    context->windows_os_build = 0u;
    context->windows_nt_global_flag = 0u;
    context->windows_kuser_shared_data_base = 0ull;
    context->windows_kuser_shared_data_updates = 0u;
    context->windows_kuser_shared_data_checksum = 0u;
    context->windows_security_cookie_address = 0ull;
    context->windows_security_cookie_value = 0ull;
    context->windows_security_cookie_checksum = 0u;
    context->windows_entry_rip = 0ull;
    context->windows_entry_rsp = 0ull;
    context->windows_entry_arg_rcx = 0ull;
    context->windows_entry_arg_rdx = 0ull;
    context->windows_entry_arg_r8 = 0ull;
    context->windows_entry_transfer_ready = 0u;

    if (process64_attach_persona(pid, context) == 0u)
    {
        persona64_release_context(context);
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    ++g_persona64_native_count;
    return PERSONA64_ATTACH_OK;
}

u32 persona64_init_linux_elf(u32 pid, void *syscall_dispatch_table)
{
    persona_context_t *context;

    persona64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (syscall_dispatch_table == 0)
        || (process64_persona_ctx(pid) != 0))
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    context = persona64_acquire_context(pid);
    if (context == 0)
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    context->persona_type = PERSONA64_TYPE_LINUX_ELF;
    context->syscall_dispatch_table = syscall_dispatch_table;
    context->vma_root = process64_vma_root(pid);
    context->fd_table = process64_fd_table(pid);
    context->tls_base = PERSONA64_TLS_UNSET;
    context->tls_size = 0ull;
    context->clear_child_tid = 0ull;
    context->brk_base = PERSONA64_BRK_UNSET;
    context->brk_current = PERSONA64_BRK_UNSET;
    context->heap_cap = PERSONA64_HEAP_CAP_NONE;
    context->persona_module_handle = PERSONA64_MODULE_NONE;
    context->audit_context = process64_audit_ctx(pid);
    context->capability_attenuation_mask = 0u;
    context->load_bias_low = 0u;
    context->windows_exception_table_base = 0ull;
    context->windows_exception_table_bytes = 0ull;
    context->windows_exception_function_count = 0u;
    context->windows_exception_table_checksum = 0u;
    context->windows_teb_base = 0ull;
    context->windows_peb_base = 0ull;
    context->windows_stack_base = 0ull;
    context->windows_stack_limit = 0ull;
    context->windows_tls_pointer = 0ull;
    context->windows_image_base = 0ull;
    context->windows_process_parameters = 0ull;
    context->windows_os_major = 0u;
    context->windows_os_minor = 0u;
    context->windows_os_build = 0u;
    context->windows_nt_global_flag = 0u;
    context->windows_kuser_shared_data_base = 0ull;
    context->windows_kuser_shared_data_updates = 0u;
    context->windows_kuser_shared_data_checksum = 0u;
    context->windows_security_cookie_address = 0ull;
    context->windows_security_cookie_value = 0ull;
    context->windows_security_cookie_checksum = 0u;
    context->windows_entry_rip = 0ull;
    context->windows_entry_rsp = 0ull;
    context->windows_entry_arg_rcx = 0ull;
    context->windows_entry_arg_rdx = 0ull;
    context->windows_entry_arg_r8 = 0ull;
    context->windows_entry_transfer_ready = 0u;
    context->linux_cwd_length = 1u;
    context->linux_cwd[0] = (u8)'/';
    context->linux_cwd[1] = 0u;

    if (process64_attach_persona(pid, context) == 0u)
    {
        persona64_release_context(context);
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    return PERSONA64_ATTACH_OK;
}

u32 persona64_init_windows_pe(u32 pid, void *syscall_dispatch_table)
{
    persona_context_t *context;
    void *effective_dispatch_table;

    persona64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (process64_persona_ctx(pid) != 0))
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    context = persona64_acquire_context(pid);
    if (context == 0)
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    effective_dispatch_table = (syscall_dispatch_table != 0)
        ? syscall_dispatch_table
        : (void *)windows_abi64_dispatch_table();

    context->persona_type = PERSONA64_TYPE_WINDOWS_PE;
    context->syscall_dispatch_table = effective_dispatch_table;
    context->vma_root = process64_vma_root(pid);
    context->fd_table = process64_fd_table(pid);
    context->tls_base = PERSONA64_TLS_UNSET;
    context->tls_size = 0ull;
    context->clear_child_tid = 0ull;
    context->brk_base = PERSONA64_BRK_UNSET;
    context->brk_current = PERSONA64_BRK_UNSET;
    context->heap_cap = PERSONA64_HEAP_CAP_NONE;
    context->persona_module_handle = PERSONA64_MODULE_NONE;
    context->audit_context = process64_audit_ctx(pid);
    context->capability_attenuation_mask = 0u;
    context->load_bias_low = 0u;
    context->windows_exception_table_base = 0ull;
    context->windows_exception_table_bytes = 0ull;
    context->windows_exception_function_count = 0u;
    context->windows_exception_table_checksum = 0u;
    context->windows_teb_base = 0ull;
    context->windows_peb_base = 0ull;
    context->windows_stack_base = 0ull;
    context->windows_stack_limit = 0ull;
    context->windows_tls_pointer = 0ull;
    context->windows_image_base = 0ull;
    context->windows_process_parameters = 0ull;
    context->windows_os_major = 0u;
    context->windows_os_minor = 0u;
    context->windows_os_build = 0u;
    context->windows_nt_global_flag = 0u;
    context->windows_kuser_shared_data_base = 0ull;
    context->windows_kuser_shared_data_updates = 0u;
    context->windows_kuser_shared_data_checksum = 0u;
    context->windows_security_cookie_address = 0ull;
    context->windows_security_cookie_value = 0ull;
    context->windows_security_cookie_checksum = 0u;
    context->windows_entry_rip = 0ull;
    context->windows_entry_rsp = 0ull;
    context->windows_entry_arg_rcx = 0ull;
    context->windows_entry_arg_rdx = 0ull;
    context->windows_entry_arg_r8 = 0ull;
    context->windows_entry_transfer_ready = 0u;

    if (process64_attach_persona(pid, context) == 0u)
    {
        persona64_release_context(context);
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    return PERSONA64_ATTACH_OK;
}

u32 persona64_release(u32 pid)
{
    persona_context_t *context;
    void *detached;

    persona64_init();

    if ((pid == PROCESS64_INVALID_PID) || (process64_principal(pid) == 0u))
    {
        ++g_persona64_denial_count;
        return 0u;
    }

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        ++g_persona64_denial_count;
        return 0u;
    }

    detached = process64_detach_persona(pid);
    if (detached != context)
    {
        ++g_persona64_denial_count;
        return 0u;
    }

    if ((context->persona_type == PERSONA64_TYPE_LIMITLESS_NATIVE)
        && (g_persona64_native_count > 0u))
    {
        --g_persona64_native_count;
    }

    persona64_release_context(context);
    return 1u;
}

u32 persona64_type(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->persona_type : PERSONA64_TYPE_COUNT;
}

u32 persona64_context_attached(u32 pid)
{
    return (persona64_context_for_process(pid) != 0) ? 1u : 0u;
}

u32 persona64_native_count(void)
{
    persona64_init();
    return g_persona64_native_count;
}

u32 persona64_denial_count(void)
{
    persona64_init();
    return g_persona64_denial_count;
}

u32 persona64_detect_format(const u8 *binary_data, u32 size)
{
    u32 magic_le;
    u32 magic_be;
    u32 pe_offset;

    if ((binary_data == 0) || (size < 2u))
    {
        return PERSONA64_FORMAT_UNKNOWN;
    }

    if ((binary_data[0] == (u8)'#') && (binary_data[1] == (u8)'!'))
    {
        return PERSONA64_FORMAT_SHEBANG;
    }

    if ((size >= 4u)
        && (binary_data[0] == 0x7Fu)
        && (binary_data[1] == (u8)'E')
        && (binary_data[2] == (u8)'L')
        && (binary_data[3] == (u8)'F'))
    {
        return PERSONA64_FORMAT_ELF;
    }

    if ((size >= 4u) && (persona64_detect_native_app_descriptor(binary_data, size) != 0u))
    {
        return PERSONA64_FORMAT_NATIVE_APP;
    }

    if ((size >= 4u))
    {
        magic_le = persona64_read_le32(binary_data);
        magic_be = persona64_read_be32(binary_data);

        if (magic_le == 0xFEEDFACFull)
        {
            return PERSONA64_FORMAT_MACHO_LE64;
        }
        if (magic_le == 0xFEEDFACEull)
        {
            return PERSONA64_FORMAT_MACHO_LE32;
        }
        if (magic_be == 0xCAFEBABEu)
        {
            return PERSONA64_FORMAT_MACHO_FAT;
        }
        if (magic_be == 0xCAFEBABFu)
        {
            return PERSONA64_FORMAT_MACHO_FAT64;
        }
    }

    if ((size >= 0x40u)
        && (binary_data[0] == (u8)'M')
        && (binary_data[1] == (u8)'Z'))
    {
        pe_offset = persona64_read_le32(binary_data + 0x3Cu);
        if ((pe_offset <= (size - 4u))
            && (binary_data[pe_offset] == (u8)'P')
            && (binary_data[pe_offset + 1u] == (u8)'E')
            && (binary_data[pe_offset + 2u] == 0u)
            && (binary_data[pe_offset + 3u] == 0u))
        {
            return PERSONA64_FORMAT_PE;
        }
    }

    return PERSONA64_FORMAT_UNKNOWN;
}
