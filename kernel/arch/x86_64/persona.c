#include "persona_x64.h"

#include "linux_abi_x64.h"
#include "macos_abi_x64.h"
#include "process_x64.h"
#include "persona_audit_x64.h"
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
 * test shims. N.1 wires macOS Mach-O persona contexts to the macOS BSD ABI
 * switchboard by default while preserving explicit table injection for
 * focused loader tests. O.3 adds default and configurable resource budgets
 * for persona-owned VMA pages, file descriptors, and pipe objects; the VMA,
 * FD, and pipe subsystems consult this context before extending per-process
 * state, and the scaffold proves budget exhaustion returns ABI-truthful
 * ENOMEM/EMFILE denials without granting ambient authority. O.4 adds
 * persona-owned process groups and direct-signal isolation. Linux kill/tkill
 * consult the persona boundary before mutating signal state, and the scaffold
 * proves a Linux persona cannot directly signal a Windows persona across
 * groups while self-signaling remains intact. O.5 centralizes truthful
 * unavailability for foreign syscalls: each ABI switchboard asks persona.c
 * for the correct native error return and audit payload instead of fabricating
 * success or sharing one OS's error shape with another.
 */

static persona_context_t g_persona64_contexts[PERSONA64_MAX_CONTEXTS];
static u32 g_persona64_context_used[PERSONA64_MAX_CONTEXTS];
static u32 g_persona64_initialized = 0u;
static u32 g_persona64_native_count = 0u;
static u32 g_persona64_denial_count = 0u;

static void persona64_apply_default_budgets(persona_context_t *context)
{
    if (context == 0)
    {
        return;
    }

    context->vma_page_budget = PERSONA64_DEFAULT_VMA_PAGE_BUDGET;
    context->fd_budget = PERSONA64_DEFAULT_FD_BUDGET;
    context->pipe_budget = PERSONA64_DEFAULT_PIPE_BUDGET;
    context->pipe_count = 0u;
    context->budget_denial_count = 0u;
    context->budget_last_kind = PERSONA64_BUDGET_KIND_NONE;
    context->budget_last_requested = 0u;
    context->budget_last_limit = 0u;
}

static void persona64_apply_default_process_group(persona_context_t *context, u32 pid)
{
    if (context == 0)
    {
        return;
    }

    context->process_group_id =
        (pid != PROCESS64_INVALID_PID) ? pid : PERSONA64_PROCESS_GROUP_NONE;
    context->isolation_denial_count = 0u;
    context->isolation_last_source_pid = PROCESS64_INVALID_PID;
    context->isolation_last_target_pid = PROCESS64_INVALID_PID;
    context->isolation_last_source_group = PERSONA64_PROCESS_GROUP_NONE;
    context->isolation_last_target_group = PERSONA64_PROCESS_GROUP_NONE;
    context->isolation_last_result = PERSONA64_ISOLATION_RESULT_OK;
}

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
    context->windows_image_path_ascii_bytes = 0u;
    for (index = 0u; index < PERSONA64_WINDOWS_IMAGE_PATH_MAX_BYTES; ++index)
    {
        context->windows_image_path_ascii[index] = 0u;
    }
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
    context->windows_ntdll_base = 0ull;
    context->windows_ntdll_ldr_initialize_thunk = 0ull;
    context->windows_ntdll_symbol_count = 0u;
    context->windows_ntdll_checksum = 0u;
    context->windows_kernel32_base = 0ull;
    context->windows_kernel32_write_console_a = 0ull;
    context->windows_kernel32_symbol_count = 0u;
    context->windows_kernel32_checksum = 0u;
    context->windows_crt_base = 0ull;
    context->windows_crt_printf = 0ull;
    context->windows_crt_symbol_count = 0u;
    context->windows_crt_checksum = 0u;
    context->windows_handle_table = 0;
    context->linux_dynamic_base = 0ull;
    context->linux_dynamic_dl_start = 0ull;
    context->linux_dynamic_dlsym = 0ull;
    context->linux_dynamic_dlerror = 0ull;
    context->linux_dynamic_symbol_count = 0u;
    context->linux_dynamic_checksum = 0u;
    context->linux_dynamic_needed_count = 0u;
    context->linux_dynamic_missing_count = 0u;
    context->linux_libc_base = 0ull;
    context->linux_libc_write = 0ull;
    context->linux_libc_read = 0ull;
    context->linux_libc_exit = 0ull;
    context->linux_libc_strlen = 0ull;
    context->linux_libc_symbol_count = 0u;
    context->linux_libc_checksum = 0u;
    context->linux_libc_unavailable_count = 0u;
    context->vma_page_budget = 0u;
    context->fd_budget = 0u;
    context->pipe_budget = 0u;
    context->pipe_count = 0u;
    context->budget_denial_count = 0u;
    context->budget_last_kind = PERSONA64_BUDGET_KIND_NONE;
    context->budget_last_requested = 0u;
    context->budget_last_limit = 0u;
    context->process_group_id = PERSONA64_PROCESS_GROUP_NONE;
    context->isolation_denial_count = 0u;
    context->isolation_last_source_pid = PROCESS64_INVALID_PID;
    context->isolation_last_target_pid = PROCESS64_INVALID_PID;
    context->isolation_last_source_group = PERSONA64_PROCESS_GROUP_NONE;
    context->isolation_last_target_group = PERSONA64_PROCESS_GROUP_NONE;
    context->isolation_last_result = PERSONA64_ISOLATION_RESULT_OK;
    context->linux_cwd_length = 0u;
    for (index = 0u; index < PERSONA64_LINUX_CWD_MAX_BYTES; ++index)
    {
        context->linux_cwd[index] = 0u;
    }
    for (index = 0u; index < PERSONA64_LINUX_COMM_BYTES; ++index)
    {
        context->linux_comm[index] = 0u;
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
            persona64_apply_default_budgets(&g_persona64_contexts[index]);
            persona64_apply_default_process_group(&g_persona64_contexts[index], pid);
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
    context->capability_attenuation_mask =
        persona64_capability_mask_for_type(PERSONA64_TYPE_LIMITLESS_NATIVE);
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
    context->windows_ntdll_base = 0ull;
    context->windows_ntdll_ldr_initialize_thunk = 0ull;
    context->windows_ntdll_symbol_count = 0u;
    context->windows_ntdll_checksum = 0u;
    context->windows_kernel32_base = 0ull;
    context->windows_kernel32_write_console_a = 0ull;
    context->windows_kernel32_symbol_count = 0u;
    context->windows_kernel32_checksum = 0u;
    context->windows_crt_base = 0ull;
    context->windows_crt_printf = 0ull;
    context->windows_crt_symbol_count = 0u;
    context->windows_crt_checksum = 0u;

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
    context->capability_attenuation_mask =
        persona64_capability_mask_for_type(PERSONA64_TYPE_LINUX_ELF);
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
    context->windows_ntdll_base = 0ull;
    context->windows_ntdll_ldr_initialize_thunk = 0ull;
    context->windows_ntdll_symbol_count = 0u;
    context->windows_ntdll_checksum = 0u;
    context->windows_kernel32_base = 0ull;
    context->windows_kernel32_write_console_a = 0ull;
    context->windows_kernel32_symbol_count = 0u;
    context->windows_kernel32_checksum = 0u;
    context->windows_crt_base = 0ull;
    context->windows_crt_printf = 0ull;
    context->windows_crt_symbol_count = 0u;
    context->windows_crt_checksum = 0u;
    context->linux_cwd_length = 1u;
    context->linux_cwd[0] = (u8)'/';
    context->linux_cwd[1] = 0u;
    context->linux_comm[0] = (u8)'e';
    context->linux_comm[1] = (u8)'x';
    context->linux_comm[2] = (u8)'e';
    context->linux_comm[3] = 0u;

    if (process64_attach_persona(pid, context) == 0u)
    {
        persona64_release_context(context);
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    return PERSONA64_ATTACH_OK;
}

u32 persona64_fork_linux_elf(u32 parent_pid, u32 child_pid, void *syscall_dispatch_table)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    persona_context_t *parent;
    persona_context_t *child;
    u32 index;

    persona64_init();

    parent = persona64_context_for_process(parent_pid);
    if ((parent_pid == PROCESS64_INVALID_PID)
        || (child_pid == PROCESS64_INVALID_PID)
        || (parent_pid == child_pid)
        || (process64_principal(parent_pid) == 0u)
        || (process64_principal(child_pid) == 0u)
        || (parent == 0)
        || (parent->persona_type != PERSONA64_TYPE_LINUX_ELF)
        || (syscall_dispatch_table == 0)
        || (process64_persona_ctx(child_pid) != 0))
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    child = persona64_acquire_context(child_pid);
    if (child == 0)
    {
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    child->persona_type = PERSONA64_TYPE_LINUX_ELF;
    child->syscall_dispatch_table = syscall_dispatch_table;
    child->vma_root = process64_vma_root(child_pid);
    child->fd_table = process64_fd_table(child_pid);
    child->tls_base = parent->tls_base;
    child->tls_size = parent->tls_size;
    child->clear_child_tid = 0ull;
    child->brk_base = parent->brk_base;
    child->brk_current = parent->brk_current;
    child->heap_cap = parent->heap_cap;
    child->persona_module_handle = parent->persona_module_handle;
    child->audit_context = process64_audit_ctx(child_pid);
    child->capability_attenuation_mask = parent->capability_attenuation_mask;
    child->load_bias_low = parent->load_bias_low;
    child->vma_page_budget = parent->vma_page_budget;
    child->fd_budget = parent->fd_budget;
    child->pipe_budget = parent->pipe_budget;
    child->pipe_count = 0u;
    child->process_group_id = parent->process_group_id;
    child->linux_cwd_length = parent->linux_cwd_length;
    for (index = 0u; index < PERSONA64_LINUX_CWD_MAX_BYTES; ++index)
    {
        child->linux_cwd[index] = parent->linux_cwd[index];
    }
    for (index = 0u; index < PERSONA64_LINUX_COMM_BYTES; ++index)
    {
        child->linux_comm[index] = parent->linux_comm[index];
    }
    child->linux_signal_pending = LINUX_SIGNAL64_PENDING_NONE;
    child->linux_signal_mask = parent->linux_signal_mask;
    for (index = 0u; index < LINUX_SIGNAL64_MAX_SIGNALS; ++index)
    {
        child->linux_sigactions[index] = parent->linux_sigactions[index];
    }

    if (process64_attach_persona(child_pid, child) == 0u)
    {
        persona64_release_context(child);
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    return PERSONA64_ATTACH_OK;
#else
    (void)parent_pid;
    (void)child_pid;
    (void)syscall_dispatch_table;
    return PERSONA64_ATTACH_DENIED;
#endif
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
    context->capability_attenuation_mask =
        persona64_capability_mask_for_type(PERSONA64_TYPE_WINDOWS_PE);
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
    context->windows_ntdll_base = 0ull;
    context->windows_ntdll_ldr_initialize_thunk = 0ull;
    context->windows_ntdll_symbol_count = 0u;
    context->windows_ntdll_checksum = 0u;
    context->windows_kernel32_base = 0ull;
    context->windows_kernel32_write_console_a = 0ull;
    context->windows_kernel32_symbol_count = 0u;
    context->windows_kernel32_checksum = 0u;
    context->windows_crt_base = 0ull;
    context->windows_crt_printf = 0ull;
    context->windows_crt_symbol_count = 0u;
    context->windows_crt_checksum = 0u;

    if (process64_attach_persona(pid, context) == 0u)
    {
        persona64_release_context(context);
        ++g_persona64_denial_count;
        return PERSONA64_ATTACH_DENIED;
    }

    return PERSONA64_ATTACH_OK;
}

u32 persona64_init_macos_macho(u32 pid, void *syscall_dispatch_table)
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
        : (void *)macos_abi64_dispatch_table();

    context->persona_type = PERSONA64_TYPE_MACOS_MACHO;
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
    context->capability_attenuation_mask =
        persona64_capability_mask_for_type(PERSONA64_TYPE_MACOS_MACHO);
    context->load_bias_low = 0u;

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

u32 persona64_capability_mask_for_type(u32 persona_type)
{
    return (persona_type < PERSONA64_TYPE_COUNT) ? (1u << persona_type) : 0u;
}

u32 persona64_capability_mask(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->capability_attenuation_mask : 0u;
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

static u32 persona64_budget_record_denial(
    persona_context_t *context,
    u32 kind,
    u32 requested,
    u32 limit)
{
    if (context == 0)
    {
        return 0u;
    }

    ++context->budget_denial_count;
    context->budget_last_kind = kind;
    context->budget_last_requested = requested;
    context->budget_last_limit = limit;
    return 0u;
}

static u32 persona64_budget_sum(u32 current, u32 additional)
{
    if ((0xFFFFFFFFu - current) < additional)
    {
        return 0xFFFFFFFFu;
    }

    return current + additional;
}

u32 persona64_configure_resource_budgets(
    u32 pid,
    u32 vma_page_budget,
    u32 fd_budget,
    u32 pipe_budget)
{
    persona_context_t *context = persona64_context_for_process(pid);

    if (context == 0)
    {
        ++g_persona64_denial_count;
        return 0u;
    }

    context->vma_page_budget = vma_page_budget;
    context->fd_budget = fd_budget;
    context->pipe_budget = pipe_budget;
    context->budget_last_kind = PERSONA64_BUDGET_KIND_NONE;
    context->budget_last_requested = 0u;
    context->budget_last_limit = 0u;
    return 1u;
}

u32 persona64_budget_check_vma_pages(u32 pid, u32 current_pages, u32 additional_pages)
{
    persona_context_t *context = persona64_context_for_process(pid);
    u32 requested;

    if ((context == 0) || (additional_pages == 0u))
    {
        return 1u;
    }

    requested = persona64_budget_sum(current_pages, additional_pages);
    if ((requested > context->vma_page_budget)
        || (current_pages > context->vma_page_budget))
    {
        return persona64_budget_record_denial(
            context,
            PERSONA64_BUDGET_KIND_VMA_PAGE,
            requested,
            context->vma_page_budget);
    }

    return 1u;
}

u32 persona64_budget_check_fd(u32 pid, u32 current_fds, u32 additional_fds)
{
    persona_context_t *context = persona64_context_for_process(pid);
    u32 requested;

    if ((context == 0) || (additional_fds == 0u))
    {
        return 1u;
    }

    requested = persona64_budget_sum(current_fds, additional_fds);
    if ((requested > context->fd_budget) || (current_fds > context->fd_budget))
    {
        return persona64_budget_record_denial(
            context,
            PERSONA64_BUDGET_KIND_FD,
            requested,
            context->fd_budget);
    }

    return 1u;
}

u32 persona64_budget_check_pipe(u32 pid, u32 additional_pipes)
{
    persona_context_t *context = persona64_context_for_process(pid);
    u32 requested;

    if ((context == 0) || (additional_pipes == 0u))
    {
        return 1u;
    }

    requested = persona64_budget_sum(context->pipe_count, additional_pipes);
    if ((requested > context->pipe_budget) || (context->pipe_count > context->pipe_budget))
    {
        return persona64_budget_record_denial(
            context,
            PERSONA64_BUDGET_KIND_PIPE,
            requested,
            context->pipe_budget);
    }

    return 1u;
}

u32 persona64_budget_commit_pipe(u32 pid, u32 pipe_count)
{
    persona_context_t *context = persona64_context_for_process(pid);

    if (context == 0)
    {
        return 1u;
    }

    if (persona64_budget_check_pipe(pid, pipe_count) == 0u)
    {
        return 0u;
    }

    context->pipe_count = persona64_budget_sum(context->pipe_count, pipe_count);
    return 1u;
}

u32 persona64_budget_release_pipe(u32 pid, u32 pipe_count)
{
    persona_context_t *context = persona64_context_for_process(pid);

    if ((context == 0) || (pipe_count == 0u))
    {
        return 1u;
    }

    if (context->pipe_count >= pipe_count)
    {
        context->pipe_count -= pipe_count;
    }
    else
    {
        context->pipe_count = 0u;
    }

    return 1u;
}

u32 persona64_vma_page_budget(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->vma_page_budget : 0u;
}

u32 persona64_fd_budget(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->fd_budget : 0u;
}

u32 persona64_pipe_budget(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->pipe_budget : 0u;
}

u32 persona64_pipe_count(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->pipe_count : 0u;
}

u32 persona64_budget_denial_count(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->budget_denial_count : 0u;
}

u32 persona64_budget_last_kind(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->budget_last_kind : PERSONA64_BUDGET_KIND_NONE;
}

u32 persona64_budget_last_requested(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->budget_last_requested : 0u;
}

u32 persona64_budget_last_limit(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->budget_last_limit : 0u;
}

u32 persona64_process_group(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->process_group_id : PERSONA64_PROCESS_GROUP_NONE;
}

u32 persona64_set_process_group(u32 pid, u32 process_group_id)
{
    persona_context_t *context = persona64_context_for_process(pid);

    if ((context == 0) || (process_group_id == PERSONA64_PROCESS_GROUP_NONE))
    {
        ++g_persona64_denial_count;
        return 0u;
    }

    context->process_group_id = process_group_id;
    return 1u;
}

static void persona64_record_signal_isolation(
    persona_context_t *source_context,
    u32 source_pid,
    u32 target_pid,
    u32 target_group,
    u32 result)
{
    if (source_context == 0)
    {
        return;
    }

    source_context->isolation_last_source_pid = source_pid;
    source_context->isolation_last_target_pid = target_pid;
    source_context->isolation_last_source_group = source_context->process_group_id;
    source_context->isolation_last_target_group = target_group;
    source_context->isolation_last_result = result;

    if (result != PERSONA64_ISOLATION_RESULT_OK)
    {
        ++source_context->isolation_denial_count;
    }
}

u32 persona64_can_signal(u32 source_pid, u32 target_pid, u32 *result_out)
{
    persona_context_t *source_context;
    persona_context_t *target_context;
    u32 result;
    u32 target_group;
    u32 allowed;

    persona64_init();

    source_context = persona64_context_for_process(source_pid);
    if ((source_pid == PROCESS64_INVALID_PID)
        || (process64_principal(source_pid) == 0u)
        || (source_context == 0))
    {
        if (result_out != 0)
        {
            *result_out = PERSONA64_ISOLATION_RESULT_NO_SOURCE;
        }
        ++g_persona64_denial_count;
        return 0u;
    }

    target_context = persona64_context_for_process(target_pid);
    target_group = (target_context != 0)
        ? target_context->process_group_id
        : PERSONA64_PROCESS_GROUP_NONE;

    if ((target_pid == PROCESS64_INVALID_PID)
        || (process64_principal(target_pid) == 0u)
        || (target_context == 0))
    {
        result = PERSONA64_ISOLATION_RESULT_NO_TARGET;
        persona64_record_signal_isolation(
            source_context,
            source_pid,
            target_pid,
            target_group,
            result);
        if (result_out != 0)
        {
            *result_out = result;
        }
        return 0u;
    }

    allowed =
        ((source_pid == target_pid)
            || ((source_context->persona_type == target_context->persona_type)
                && (source_context->process_group_id != PERSONA64_PROCESS_GROUP_NONE)
                && (source_context->process_group_id == target_context->process_group_id)))
            ? 1u
            : 0u;
    result = (allowed != 0u)
        ? PERSONA64_ISOLATION_RESULT_OK
        : PERSONA64_ISOLATION_RESULT_DENIED;
    persona64_record_signal_isolation(
        source_context,
        source_pid,
        target_pid,
        target_group,
        result);
    if (result_out != 0)
    {
        *result_out = result;
    }

    return allowed;
}

u32 persona64_isolation_denial_count(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->isolation_denial_count : 0u;
}

u32 persona64_isolation_last_source_pid(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->isolation_last_source_pid : PROCESS64_INVALID_PID;
}

u32 persona64_isolation_last_target_pid(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->isolation_last_target_pid : PROCESS64_INVALID_PID;
}

u32 persona64_isolation_last_source_group(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->isolation_last_source_group : PERSONA64_PROCESS_GROUP_NONE;
}

u32 persona64_isolation_last_target_group(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0) ? context->isolation_last_target_group : PERSONA64_PROCESS_GROUP_NONE;
}

u32 persona64_isolation_last_result(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return (context != 0)
        ? context->isolation_last_result
        : PERSONA64_ISOLATION_RESULT_NO_SOURCE;
}

u32 persona64_unavailable_result_for_type(u32 persona_type)
{
    if (persona_type == PERSONA64_TYPE_LINUX_ELF)
    {
        return LINUX_ABI64_ENOSYS;
    }
    if (persona_type == PERSONA64_TYPE_WINDOWS_PE)
    {
        return WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED;
    }
    if (persona_type == PERSONA64_TYPE_MACOS_MACHO)
    {
        return MACOS_ABI64_ENOSYS;
    }

    return PERSONA_AUDIT64_RESULT_DENY;
}

u64 persona64_unavailable_return_for_type(u32 persona_type)
{
    u32 result = persona64_unavailable_result_for_type(persona_type);

    if (persona_type == PERSONA64_TYPE_LINUX_ELF)
    {
        return LINUX_ABI64_ERROR_RETURN(result);
    }
    if (persona_type == PERSONA64_TYPE_WINDOWS_PE)
    {
        return (u64)result;
    }
    if (persona_type == PERSONA64_TYPE_MACOS_MACHO)
    {
        return MACOS_ABI64_ERROR_RETURN(result);
    }

    return (u64)result;
}

u32 persona64_record_unavailable_syscall(
    u32 pid,
    u32 persona_type,
    u16 syscall_number,
    u64 rip,
    u32 *abi_result_out,
    u64 *return_value_out)
{
    u32 abi_result = persona64_unavailable_result_for_type(persona_type);
    u64 return_value = persona64_unavailable_return_for_type(persona_type);

    if (abi_result_out != 0)
    {
        *abi_result_out = abi_result;
    }
    if (return_value_out != 0)
    {
        *return_value_out = return_value;
    }

    if ((persona_type != PERSONA64_TYPE_LINUX_ELF)
        && (persona_type != PERSONA64_TYPE_WINDOWS_PE)
        && (persona_type != PERSONA64_TYPE_MACOS_MACHO))
    {
        ++g_persona64_denial_count;
        return 0u;
    }

    return persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED,
        syscall_number,
        abi_result,
        rip);
}
