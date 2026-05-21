#ifndef LIMITLESS_PERSONA_X64_H
#define LIMITLESS_PERSONA_X64_H

#include "linux_signal_x64.h"
#include "types.h"

#define PERSONA64_TYPE_LIMITLESS_NATIVE 0u
#define PERSONA64_TYPE_LINUX_ELF 1u
#define PERSONA64_TYPE_WINDOWS_PE 2u
#define PERSONA64_TYPE_MACOS_MACHO 3u
#define PERSONA64_TYPE_COUNT 4u

#define PERSONA64_MAX_CONTEXTS 16u
#define PERSONA64_MODULE_NONE 0xFFFFFFFFu
#define PERSONA64_HEAP_CAP_NONE 0xFFFFFFFFu
#define PERSONA64_TLS_UNSET 0ull
#define PERSONA64_BRK_UNSET 0ull
#define PERSONA64_LINUX_CWD_MAX_BYTES 128u
#define PERSONA64_ATTACH_OK 1u
#define PERSONA64_ATTACH_DENIED 0u

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
    void *windows_handle_table;
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
u32 persona64_release(u32 pid);
u32 persona64_type(u32 pid);
u32 persona64_context_attached(u32 pid);
u32 persona64_native_count(void);
u32 persona64_denial_count(void);
u32 persona64_detect_format(const u8 *binary_data, u32 size);

#endif
