#ifndef LIMITLESS_PERSONA_X64_H
#define LIMITLESS_PERSONA_X64_H

#include "linux_signal_x64.h"
#include "types.h"

#define PERSONA64_TYPE_LIMITLESS_NATIVE 0u
#define PERSONA64_TYPE_LINUX_ELF 1u
#define PERSONA64_TYPE_WINDOWS_PE 2u
#define PERSONA64_TYPE_MACOS_MACHO 3u
#define PERSONA64_TYPE_COUNT 4u

#define PERSONA64_CAPABILITY_MASK_NATIVE (1u << PERSONA64_TYPE_LIMITLESS_NATIVE)
#define PERSONA64_CAPABILITY_MASK_LINUX (1u << PERSONA64_TYPE_LINUX_ELF)
#define PERSONA64_CAPABILITY_MASK_WINDOWS (1u << PERSONA64_TYPE_WINDOWS_PE)
#define PERSONA64_CAPABILITY_MASK_MACOS (1u << PERSONA64_TYPE_MACOS_MACHO)
#define PERSONA64_CAPABILITY_MASK_ALL \
    (PERSONA64_CAPABILITY_MASK_NATIVE \
        | PERSONA64_CAPABILITY_MASK_LINUX \
        | PERSONA64_CAPABILITY_MASK_WINDOWS \
        | PERSONA64_CAPABILITY_MASK_MACOS)

#define PERSONA64_MAX_CONTEXTS 16u
#define PERSONA64_MODULE_NONE 0xFFFFFFFFu
#define PERSONA64_HEAP_CAP_NONE 0xFFFFFFFFu
#define PERSONA64_TLS_UNSET 0ull
#define PERSONA64_BRK_UNSET 0ull
#define PERSONA64_LINUX_CWD_MAX_BYTES 128u
#define PERSONA64_WINDOWS_IMAGE_PATH_MAX_BYTES 96u
#define PERSONA64_ATTACH_OK 1u
#define PERSONA64_ATTACH_DENIED 0u
#define PERSONA64_DEFAULT_VMA_PAGE_BUDGET 16384u
#define PERSONA64_DEFAULT_FD_BUDGET 1024u
#define PERSONA64_DEFAULT_PIPE_BUDGET 64u
#define PERSONA64_BUDGET_KIND_NONE 0u
#define PERSONA64_BUDGET_KIND_VMA_PAGE 1u
#define PERSONA64_BUDGET_KIND_FD 2u
#define PERSONA64_BUDGET_KIND_PIPE 3u
#define PERSONA64_PROCESS_GROUP_NONE 0u
#define PERSONA64_ISOLATION_RESULT_OK 0u
#define PERSONA64_ISOLATION_RESULT_DENIED 1u
#define PERSONA64_ISOLATION_RESULT_NO_SOURCE 2u
#define PERSONA64_ISOLATION_RESULT_NO_TARGET 3u

#define PERSONA64_FORMAT_UNKNOWN 0u
#define PERSONA64_FORMAT_NATIVE_APP 1u
#define PERSONA64_FORMAT_ELF 2u
#define PERSONA64_FORMAT_PE 3u
#define PERSONA64_FORMAT_MACHO_LE64 4u
#define PERSONA64_FORMAT_MACHO_LE32 5u
#define PERSONA64_FORMAT_MACHO_FAT 6u
#define PERSONA64_FORMAT_MACHO_FAT64 7u
#define PERSONA64_FORMAT_SHEBANG 8u

typedef struct persona_context
{
    u32 persona_type;
    u32 pid;
    void *syscall_dispatch_table;
    void *vma_root;
    void *fd_table;
    u64 tls_base;
    u64 tls_size;
    u64 clear_child_tid;
    u64 brk_base;
    u64 brk_current;
    u32 heap_cap;
    u32 persona_module_handle;
    void *audit_context;
    u32 capability_attenuation_mask;
    u32 load_bias_low;
    u64 windows_exception_table_base;
    u64 windows_exception_table_bytes;
    u32 windows_exception_function_count;
    u32 windows_exception_table_checksum;
    u64 windows_teb_base;
    u64 windows_peb_base;
    u64 windows_stack_base;
    u64 windows_stack_limit;
    u64 windows_tls_pointer;
    u64 windows_image_base;
    u64 windows_process_parameters;
    u32 windows_image_path_ascii_bytes;
    u8 windows_image_path_ascii[PERSONA64_WINDOWS_IMAGE_PATH_MAX_BYTES];
    u32 windows_os_major;
    u32 windows_os_minor;
    u32 windows_os_build;
    u32 windows_nt_global_flag;
    u64 windows_kuser_shared_data_base;
    u32 windows_kuser_shared_data_updates;
    u32 windows_kuser_shared_data_checksum;
    u64 windows_security_cookie_address;
    u64 windows_security_cookie_value;
    u32 windows_security_cookie_checksum;
    u64 windows_entry_rip;
    u64 windows_entry_rsp;
    u64 windows_entry_arg_rcx;
    u64 windows_entry_arg_rdx;
    u64 windows_entry_arg_r8;
    u32 windows_entry_transfer_ready;
    u64 windows_ntdll_base;
    u64 windows_ntdll_ldr_initialize_thunk;
    u32 windows_ntdll_symbol_count;
    u32 windows_ntdll_checksum;
    u64 windows_kernel32_base;
    u64 windows_kernel32_write_console_a;
    u32 windows_kernel32_symbol_count;
    u32 windows_kernel32_checksum;
    u64 windows_crt_base;
    u64 windows_crt_printf;
    u32 windows_crt_symbol_count;
    u32 windows_crt_checksum;
    void *windows_handle_table;
    u64 linux_dynamic_base;
    u64 linux_dynamic_dl_start;
    u64 linux_dynamic_dlsym;
    u64 linux_dynamic_dlerror;
    u32 linux_dynamic_symbol_count;
    u32 linux_dynamic_checksum;
    u32 linux_dynamic_needed_count;
    u32 linux_dynamic_missing_count;
    u64 linux_libc_base;
    u64 linux_libc_write;
    u64 linux_libc_read;
    u64 linux_libc_exit;
    u64 linux_libc_strlen;
    u64 linux_libc_envp;
    u32 linux_libc_envc;
    u32 linux_libc_environment_bound;
    u32 linux_libc_symbol_count;
    u32 linux_libc_checksum;
    u32 linux_libc_unavailable_count;
    u32 vma_page_budget;
    u32 fd_budget;
    u32 pipe_budget;
    u32 pipe_count;
    u32 budget_denial_count;
    u32 budget_last_kind;
    u32 budget_last_requested;
    u32 budget_last_limit;
    u32 process_group_id;
    u32 isolation_denial_count;
    u32 isolation_last_source_pid;
    u32 isolation_last_target_pid;
    u32 isolation_last_source_group;
    u32 isolation_last_target_group;
    u32 isolation_last_result;
    u32 linux_cwd_length;
    u8 linux_cwd[PERSONA64_LINUX_CWD_MAX_BYTES];
    u64 linux_signal_pending;
    u64 linux_signal_mask;
    linux_signal64_sigaction_t linux_sigactions[LINUX_SIGNAL64_MAX_SIGNALS];
} persona_context_t;

void persona64_init(void);
persona_context_t *persona64_context_for_process(u32 pid);
u32 persona64_init_native(u32 pid);
u32 persona64_init_linux_elf(u32 pid, void *syscall_dispatch_table);
u32 persona64_init_windows_pe(u32 pid, void *syscall_dispatch_table);
u32 persona64_init_macos_macho(u32 pid, void *syscall_dispatch_table);
u32 persona64_release(u32 pid);
u32 persona64_type(u32 pid);
u32 persona64_capability_mask_for_type(u32 persona_type);
u32 persona64_capability_mask(u32 pid);
u32 persona64_context_attached(u32 pid);
u32 persona64_native_count(void);
u32 persona64_denial_count(void);
u32 persona64_detect_format(const u8 *binary_data, u32 size);
u32 persona64_configure_resource_budgets(
    u32 pid,
    u32 vma_page_budget,
    u32 fd_budget,
    u32 pipe_budget);
u32 persona64_budget_check_vma_pages(u32 pid, u32 current_pages, u32 additional_pages);
u32 persona64_budget_check_fd(u32 pid, u32 current_fds, u32 additional_fds);
u32 persona64_budget_check_pipe(u32 pid, u32 additional_pipes);
u32 persona64_budget_commit_pipe(u32 pid, u32 pipe_count);
u32 persona64_budget_release_pipe(u32 pid, u32 pipe_count);
u32 persona64_vma_page_budget(u32 pid);
u32 persona64_fd_budget(u32 pid);
u32 persona64_pipe_budget(u32 pid);
u32 persona64_pipe_count(u32 pid);
u32 persona64_budget_denial_count(u32 pid);
u32 persona64_budget_last_kind(u32 pid);
u32 persona64_budget_last_requested(u32 pid);
u32 persona64_budget_last_limit(u32 pid);
u32 persona64_process_group(u32 pid);
u32 persona64_set_process_group(u32 pid, u32 process_group_id);
u32 persona64_can_signal(u32 source_pid, u32 target_pid, u32 *result_out);
u32 persona64_isolation_denial_count(u32 pid);
u32 persona64_isolation_last_source_pid(u32 pid);
u32 persona64_isolation_last_target_pid(u32 pid);
u32 persona64_isolation_last_source_group(u32 pid);
u32 persona64_isolation_last_target_group(u32 pid);
u32 persona64_isolation_last_result(u32 pid);
u32 persona64_unavailable_result_for_type(u32 persona_type);
u64 persona64_unavailable_return_for_type(u32 persona_type);
u32 persona64_record_unavailable_syscall(
    u32 pid,
    u32 persona_type,
    u16 syscall_number,
    u64 rip,
    u32 *abi_result_out,
    u64 *return_value_out);

#endif
