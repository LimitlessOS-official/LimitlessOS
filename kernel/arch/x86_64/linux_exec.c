#include "linux_exec_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

#include "boot_media_x64.h"
#include "capability_x64.h"
#include "console_x64.h"
#include "descriptors_x64.h"
#include "elf64_x64.h"
#include "fd_x64.h"
#include "interrupts_x64.h"
#include "linux_abi_x64.h"
#include "linux_dynamic_x64.h"
#include "mmio_x64.h"
#include "linux_vfs_x64.h"
#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "pipe_x64.h"
#include "principal_x64.h"
#include "process_x64.h"
#include "scheduler_x64.h"
#include "services.h"
#include "services_x64.h"
#include "syscall_x64.h"
#include "vma_x64.h"

typedef enum linux_exec64_stage
{
    LINUX_EXEC64_STAGE_NONE = 0u,
    LINUX_EXEC64_STAGE_ARGUMENT = 1u,
    LINUX_EXEC64_STAGE_CAPABILITY = 2u,
    LINUX_EXEC64_STAGE_READ = 3u,
    LINUX_EXEC64_STAGE_ELF = 4u,
    LINUX_EXEC64_STAGE_STATIC = 5u,
    LINUX_EXEC64_STAGE_PROCESS = 6u,
    LINUX_EXEC64_STAGE_LAUNCH = 7u,
    LINUX_EXEC64_STAGE_TASK = 8u,
    LINUX_EXEC64_STAGE_RUN = 9u,
    LINUX_EXEC64_STAGE_CLEANUP = 10u,
    LINUX_EXEC64_STAGE_DYNAMIC = 11u
} linux_exec64_stage_t;

#define LINUX_EXEC64_DEFAULT_ENV_COUNT 4u
#define LINUX_EXEC64_DT_PLTRELSZ 2ull
#define LINUX_EXEC64_DT_RELA 7ull
#define LINUX_EXEC64_DT_RELASZ 8ull
#define LINUX_EXEC64_DT_RELAENT 9ull
#define LINUX_EXEC64_DT_STRTAB 5ull
#define LINUX_EXEC64_DT_SYMTAB 6ull
#define LINUX_EXEC64_DT_STRSZ 10ull
#define LINUX_EXEC64_DT_SYMENT 11ull
#define LINUX_EXEC64_DT_PLTREL 20ull
#define LINUX_EXEC64_DT_JMPREL 23ull
#define LINUX_EXEC64_DT_RELA_VALUE 7ull
#define LINUX_EXEC64_RELA_ENTRY_BYTES 24ull
#define LINUX_EXEC64_SYMBOL_ENTRY_BYTES 24ull
#define LINUX_EXEC64_SYMBOL_TOKEN_BYTES 64u
#define LINUX_EXEC64_RELOC_TYPE_GLOB_DAT 6u
#define LINUX_EXEC64_RELOC_TYPE_JUMP_SLOT 7u
#define LINUX_EXEC64_SYMBOL_BIND_WEAK 2u
#define LINUX_EXEC64_SYMBOL_SECTION_UNDEF 0u
#define LINUX_EXEC64_SOURCE_NVME 1u
#define LINUX_EXEC64_SOURCE_BOOT_MEDIA 2u
#define LINUX_EXEC64_DYNAMIC_NONE 0u
#define LINUX_EXEC64_DYNAMIC_FAILED 1u
#define LINUX_EXEC64_DYNAMIC_READY 2u

static const char *const g_linux_exec64_default_envp[LINUX_EXEC64_DEFAULT_ENV_COUNT] = {
    "PATH=/usr/local/bin:/bin:/usr/bin",
    "HOME=/",
    "USER=limitless",
    "PWD=/"
};
static char g_linux_exec64_staged_argv[LINUX_EXEC64_ARG_MAX][ELF64_STACK_MAX_STRING_BYTES];
static const char *g_linux_exec64_staged_argv_ptrs[LINUX_EXEC64_ARG_MAX];
static char g_linux_exec64_staged_envp[LINUX_EXEC64_DEFAULT_ENV_COUNT][ELF64_STACK_MAX_STRING_BYTES];
static const char *g_linux_exec64_staged_envp_ptrs[LINUX_EXEC64_DEFAULT_ENV_COUNT];

typedef struct linux_exec64_telemetry
{
    linux_exec64_stage_t stage;
    u32 source;
    u32 nvme_read;
    u32 boot_media_read;
    u32 boot_media_read_error;
    u32 boot_media_read_bytes;
    u32 boot_media_read_capacity;
    u32 elf;
    u32 static_elf;
    u32 elf_type;
    u32 elf_load_count;
    u32 elf_interp_count;
    u32 interp_path_bytes;
    u32 interp_path_checksum;
    u32 interp_supported;
    u32 interp_file_attempt;
    u32 interp_file_read;
    u32 interp_file_bytes;
    u32 interp_file_elf;
    u32 interp_file_type;
    u32 interp_file_load_count;
    u32 interp_file_interp_count;
    u32 interp_file_dynamic_count;
    u32 interp_file_error;
    u32 interp_file_nvme_error;
    u32 dynamic_map_attempt;
    u32 dynamic_process;
    u32 dynamic_app_mapped;
    u32 dynamic_app_pages;
    u32 dynamic_interp_mapped;
    u32 dynamic_interp_pages;
    u32 dynamic_map_cleanup;
    u32 dynamic_map_error;
    u32 dynamic_reloc;
    u32 dynamic_rela_count;
    u32 dynamic_jmprel_count;
    u32 dynamic_relaent;
    u32 dynamic_pltrel;
    u32 dynamic_reloc_first_type;
    u32 dynamic_jmprel_first_type;
    u32 dynamic_reloc_error;
    u32 dynamic_symbol_trace;
    u32 dynamic_symtab;
    u32 dynamic_strtab;
    u32 dynamic_syment;
    u32 dynamic_reloc_symbol_index;
    u32 dynamic_jmprel_symbol_index;
    u32 dynamic_reloc_symbol_bytes;
    u32 dynamic_jmprel_symbol_bytes;
    u32 dynamic_needed_name_bytes;
    u32 dynamic_reloc_symbol_checksum;
    u32 dynamic_jmprel_symbol_checksum;
    u32 dynamic_needed_name_checksum;
    u32 dynamic_symbol_error;
    u32 dynamic_binding_walk;
    u32 dynamic_binding_total;
    u32 dynamic_binding_supported;
    u32 dynamic_binding_missing;
    u32 dynamic_binding_weak_null;
    u32 dynamic_binding_unavailable;
    u32 dynamic_binding_libc;
    u32 dynamic_binding_interp;
    u32 dynamic_binding_glob_dat;
    u32 dynamic_binding_jump_slot;
    u32 dynamic_binding_other;
    u32 dynamic_binding_error;
    u32 dynamic_reloc_dry_run;
    u32 dynamic_reloc_dry_total;
    u32 dynamic_reloc_dry_target_valid;
    u32 dynamic_reloc_dry_value;
    u32 dynamic_reloc_dry_provider;
    u32 dynamic_reloc_dry_weak_null;
    u32 dynamic_reloc_dry_unavailable;
    u32 dynamic_reloc_dry_apply_ready;
    u32 dynamic_reloc_dry_blocked;
    u32 dynamic_reloc_dry_error;
    u32 dynamic_reloc_dry_first_set;
    u32 dynamic_reloc_apply;
    u32 dynamic_reloc_apply_total;
    u32 dynamic_reloc_apply_write;
    u32 dynamic_reloc_apply_readback;
    u32 dynamic_reloc_apply_blocked;
    u32 dynamic_reloc_apply_unavailable;
    u32 dynamic_reloc_apply_error;
    u32 dynamic_libc_start_main;
    u32 dynamic_libc_start_main_apply;
    u32 dynamic_stack;
    u32 dynamic_stack_pages;
    u32 dynamic_stack_error;
    u32 dynamic_stack_argc;
    u32 dynamic_stack_envc;
    u32 dynamic_stack_auxv;
    u32 dynamic_stack_align;
    u32 dynamic_stack_argv_null;
    u32 dynamic_stack_envp_null;
    u32 dynamic_stack_auxv_null;
    u32 dynamic_stack_random_checksum;
    u32 dynamic_stack_platform_checksum;
    u32 dynamic_transfer_ready;
    u64 dynamic_reloc_first_target;
    u64 dynamic_jmprel_first_target;
    u64 dynamic_reloc_dry_first_target;
    u64 dynamic_reloc_dry_first_value;
    u64 dynamic_reloc_dry_jmprel_target;
    u64 dynamic_reloc_dry_jmprel_value;
    u64 dynamic_reloc_apply_first_readback;
    u64 dynamic_reloc_apply_jmprel_readback;
    u64 dynamic_libc_start_main_value;
    u64 dynamic_libc_start_main_readback;
    u64 dynamic_stack_initial_rsp;
    u64 dynamic_stack_auxv_address;
    u64 dynamic_auxv_phdr;
    u64 dynamic_auxv_base;
    u64 dynamic_auxv_entry;
    u64 dynamic_transfer_rip;
    u64 dynamic_transfer_rsp;
    u32 dynamic_task_registered;
    u32 dynamic_transfer_started;
    u32 dynamic_first_syscall;
    u32 dynamic_console_bytes;
    u32 dynamic_exit_code;
    u32 elf_dynamic_count;
    u32 dynamic_needed;
    u32 dynamic_supported;
    u32 dynamic_missing;
    u32 dynamic_libc;
    u32 dynamic_pthread;
    u32 dynamic_first_needed_checksum;
    u32 dynamic_last_needed_checksum;
    u32 mapped_pages;
    u32 mapped_regions;
    u32 stack_pages;
    u32 envc;
    u32 pml4;
    u32 pml4_pool;
    u32 pml4_slot;
    u32 scheduler_denial;
    u64 root_physical;
    u64 kernel_root_physical;
    u32 root_distinct;
    u32 high_copy;
    u32 mmio_shared;
    u32 pool_mapped;
    u32 low_compat;
    u32 low_pdpt_present;
    u32 syscall_entry_high;
    u32 idt_high;
    u32 descriptor_high;
    u32 kernel_entry_high_ready;
    u32 kernel_cr3_entry;
    u32 syscall_root_repair;
    u32 syscall_root_reload;
    u32 syscall_root_denial;
    u32 fs_save;
    u32 fs_restore;
    u32 fs_set;
    u32 user_pdpt_private;
    u32 vma_pt_private;
    u32 cr3_start;
    u32 cr3_exit;
    u32 cr3_syscall_entry;
    u32 active_cr3_match;
    u32 root_cleanup;
    u32 task;
    u32 started;
    u32 console_bytes;
    u32 exit_code;
    u32 cleanup;
    u32 signal_sigpipe;
    u32 signal_sigchld;
    u32 signal_rt_sigreturn;
    u32 signal_frame_fault;
    u32 mmap_calls;
    u32 mmap_bytes;
    u32 mmap_denial;
    u32 mmap_file_calls;
    u32 mmap_file_bytes;
    u32 mmap_file_denial;
    u32 mmap_last_error;
    u64 mmap_last_flags;
    u64 mmap_last_length;
    u32 futex_wait;
    u32 futex_wake;
    u32 futex_woken;
    u32 futex_waiters_final;
    u32 thread_exit_cleartid;
    u32 thread_exit_cleartid_fault;
    u32 nvme_vfs_bind;
    u32 nvme_vfs_release;
    u32 nvme_vfs_reads;
    u32 nvme_vfs_readdirs;
    u32 nvme_vfs_dirents;
    u32 nvme_vfs_last_bytes;
    u32 bin_vfs_aliases;
    u32 bin_vfs_opens;
    u32 bin_vfs_reads;
    u32 bin_vfs_denials;
    u32 localbin_vfs_aliases;
    u32 localbin_vfs_opens;
    u32 localbin_vfs_reads;
    u32 localbin_vfs_denials;
    u32 getdents64_calls;
    u32 getdents64_entries;
    u32 getdents64_bytes;
    u32 stat_calls;
    u32 stat_denial;
    u32 stat_fault;
    u32 fstat_calls;
    u32 fstat_denial;
    u32 fstat_fault;
    u32 newfstatat_calls;
    u32 newfstatat_denial;
    u32 newfstatat_fault;
    u32 lseek_calls;
    u32 lseek_denial;
    u32 dup_calls;
    u32 dup2_calls;
    u32 dup3_calls;
    u32 dup_denial;
    u32 fcntl_calls;
    u32 fcntl_denial;
    u32 readlink_calls;
    u32 readlink_bytes;
    u32 readlink_denial;
    u32 readlink_fault;
    u32 readlink_last_result;
    u32 getcwd_calls;
    u32 getcwd_bytes;
    u32 getcwd_denial;
    u32 getcwd_fault;
    u32 path_relative;
    u32 path_dot;
    u32 path_dotdot;
    u32 path_trailing;
    u32 path_trailing_denial;
    u32 path_fault;
    u32 chdir_calls;
    u32 fchdir_calls;
    u32 chdir_denial;
    u32 chdir_fault;
    u32 openat_calls;
    u32 read_calls;
    u32 read_bytes;
    u32 write_calls;
    u32 write_bytes;
    u32 pipe_calls;
    u32 pipe_denial;
    u32 pipe_fault;
    u32 pipe2_calls;
    u32 pipe2_denial;
    u32 pipe2_fault;
    u32 pipe_live_final;
    u32 pipe_blocks;
    u32 pipe_wakes;
    u32 pipe_replays;
    u32 pipe_provider_denial;
    u32 fd_fork_pipe_copy;
    u32 fd_fork_pipe_denial;
    u32 fd_fork_pipe_last_fd;
    u32 readv_calls;
    u32 readv_bytes;
    u32 writev_calls;
    u32 writev_bytes;
    u32 poll_calls;
    u32 ppoll_calls;
    u32 poll_ready;
    u32 poll_last_revents;
    u32 geteuid_calls;
    u32 getppid_calls;
    u32 ioctl_calls;
    u32 ioctl_tty;
    u32 ioctl_enotty;
    u32 ioctl_enosys;
    u32 ioctl_last_request;
    u32 ioctl_last_result;
    u32 prctl_calls;
    u32 prctl_set_name;
    u32 prctl_get_name;
    u32 prctl_enosys;
    u32 prctl_last_option;
    u32 prctl_last_result;
    u32 execve_calls;
    u32 execveat_calls;
    u32 execve_denial;
    u32 execve_fault;
    u32 execve_last_error;
    u32 execve_last_binary_bytes;
    u32 execve_last_closed_fds;
    u32 execve_last_fd_live_before;
    u32 execve_last_fd_live_after;
    u32 execve_last_vma_before;
    u32 execve_last_vma_released;
    u32 execve_last_vma_after;
    u32 execve_last_argc;
    u32 execve_last_envc;
    u32 execve_last_transfer_ready;
    u64 execve_last_transfer_rip;
    u64 execve_last_transfer_rsp;
    u32 fork_calls;
    u32 fork_success;
    u32 fork_enosys;
    u32 fork_denial;
    u32 fork_child_slot;
    u32 fork_child_root_distinct;
    u64 fork_last_rip;
    u32 clone_thread;
    u32 clone_thread_success;
    u32 clone_denial;
    u32 clone_last_flags;
    u32 clone_unsupported_flags;
    u32 clone_shared_cr3;
    u32 clone_shared_vma;
    u32 clone_shared_fd;
    u32 clone_last_task;
    u64 clone_last_tls_base;
    u32 wait4_calls;
    u32 wait4_reap;
    u32 wait4_last_exit_code;
    u32 child_root_cleanup;
    u32 pml4_pool_used_final;
    u32 nvme_read_error;
    u32 nvme_read_bytes;
    u32 nvme_read_capacity;
    u32 nvme_read_size;
    u32 nvme_read_attr;
    u32 failure_code;
    u32 pid;
    u32 syscall_last;
    u32 syscall_last_result;
    u32 syscall_unimplemented_delta;
    u32 syscall_unimplemented_last;
    u64 syscall_unimplemented_last_rip;
    u32 page_fault_delta;
    u64 page_fault_rip;
    u64 load_first_vaddr;
    u64 load_max_end;
    u64 low_kernel_vma_limit;
} linux_exec64_telemetry_t;

static u8 g_linux_exec64_binary[LINUX_EXEC64_STAGING_BUFFER_BYTES]
    __attribute__((aligned(VMA64_PAGE_BYTES)));
static elf64_launch_result_t g_linux_exec64_launch;
static linux_exec64_telemetry_t g_linux_exec64_telemetry;
static char g_linux_exec64_dynamic_needed_name[LINUX_EXEC64_SYMBOL_TOKEN_BYTES];
static char g_linux_exec64_dynamic_reloc_symbol[LINUX_EXEC64_SYMBOL_TOKEN_BYTES];
static char g_linux_exec64_dynamic_jmprel_symbol[LINUX_EXEC64_SYMBOL_TOKEN_BYTES];

static u32 linux_exec64_strlen(const char *text)
{
    u32 length = 0u;

    if (text == 0)
    {
        return 0u;
    }

    while (text[length] != '\0')
    {
        ++length;
    }
    return length;
}

static void linux_exec64_zero(void *target, u32 byte_count)
{
    u8 *bytes = (u8 *)target;
    u32 index;

    if (target == 0)
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static u32 linux_exec64_copy_bounded_cstr(char *target, u32 target_bytes, const char *source)
{
    u32 index;

    if ((target == 0) || (target_bytes == 0u) || (source == 0))
    {
        return 0u;
    }

    for (index = 0u; index < target_bytes; ++index)
    {
        target[index] = source[index];
        if (source[index] == '\0')
        {
            return 1u;
        }
    }

    target[target_bytes - 1u] = '\0';
    return 0u;
}

static u32 linux_exec64_stage_launch_strings(const char *const *argv, u32 argc)
{
    u32 index;

    if ((argv == 0) || (argc == 0u) || (argc > LINUX_EXEC64_ARG_MAX))
    {
        return 0u;
    }

    linux_exec64_zero(g_linux_exec64_staged_argv, sizeof(g_linux_exec64_staged_argv));
    linux_exec64_zero(g_linux_exec64_staged_argv_ptrs, sizeof(g_linux_exec64_staged_argv_ptrs));
    linux_exec64_zero(g_linux_exec64_staged_envp, sizeof(g_linux_exec64_staged_envp));
    linux_exec64_zero(g_linux_exec64_staged_envp_ptrs, sizeof(g_linux_exec64_staged_envp_ptrs));

    for (index = 0u; index < argc; ++index)
    {
        if (linux_exec64_copy_bounded_cstr(
                g_linux_exec64_staged_argv[index],
                ELF64_STACK_MAX_STRING_BYTES,
                argv[index]) == 0u)
        {
            return 0u;
        }
        g_linux_exec64_staged_argv_ptrs[index] = g_linux_exec64_staged_argv[index];
    }

    for (index = 0u; index < LINUX_EXEC64_DEFAULT_ENV_COUNT; ++index)
    {
        if (linux_exec64_copy_bounded_cstr(
                g_linux_exec64_staged_envp[index],
                ELF64_STACK_MAX_STRING_BYTES,
                g_linux_exec64_default_envp[index]) == 0u)
        {
            return 0u;
        }
        g_linux_exec64_staged_envp_ptrs[index] = g_linux_exec64_staged_envp[index];
    }

    return 1u;
}

static u32 linux_exec64_read_nvme_source(
    const u8 *path,
    u32 path_bytes,
    u8 *buffer,
    u32 capacity,
    u32 owner_id,
    u32 *bytes_read)
{
    u32 result = mmio64_nvme_fat_shell_read_file(path, path_bytes, buffer, capacity, owner_id, bytes_read);

    g_linux_exec64_telemetry.nvme_read_error = mmio64_nvme_fat_shell_read_last_error();
    g_linux_exec64_telemetry.nvme_read_bytes = mmio64_nvme_fat_shell_read_last_bytes();
    g_linux_exec64_telemetry.nvme_read_capacity = mmio64_nvme_fat_shell_read_last_capacity();
    g_linux_exec64_telemetry.nvme_read_size = mmio64_nvme_fat_shell_read_last_size();
    g_linux_exec64_telemetry.nvme_read_attr = mmio64_nvme_fat_shell_read_last_attr();
    if (result != 0u)
    {
        g_linux_exec64_telemetry.nvme_read = 1u;
    }
    return result;
}

static u32 linux_exec64_read_source(
    u32 source,
    const u8 *path,
    u32 path_bytes,
    u8 *buffer,
    u32 capacity,
    u32 owner_id,
    u32 *bytes_read)
{
    if (source == LINUX_EXEC64_SOURCE_BOOT_MEDIA)
    {
        u32 result = boot_media64_read_file(path, path_bytes, buffer, capacity, bytes_read);

        g_linux_exec64_telemetry.boot_media_read_error = boot_media64_last_error();
        g_linux_exec64_telemetry.boot_media_read_bytes = boot_media64_last_bytes();
        g_linux_exec64_telemetry.boot_media_read_capacity = boot_media64_last_capacity();
        if (result != 0u)
        {
            g_linux_exec64_telemetry.boot_media_read = 1u;
        }
        (void)owner_id;
        return result;
    }

    return linux_exec64_read_nvme_source(path, path_bytes, buffer, capacity, owner_id, bytes_read);
}

static u32 linux_exec64_write(
    u32 console_capability,
    u32 owner_id,
    const u8 *bytes,
    u32 byte_count)
{
    return console64_write_kernel(console_capability, bytes, byte_count, owner_id);
}

static u32 linux_exec64_write_text(
    u32 console_capability,
    u32 owner_id,
    const char *text)
{
    return linux_exec64_write(
        console_capability,
        owner_id,
        (const u8 *)text,
        linux_exec64_strlen(text));
}

static void linux_exec64_write_token(
    u32 console_capability,
    u32 owner_id,
    const char *text,
    u32 text_bytes)
{
    u32 index;

    if ((text == 0) || (text_bytes == 0u))
    {
        (void)linux_exec64_write_text(console_capability, owner_id, "<none>");
        return;
    }

    for (index = 0u; index < text_bytes; ++index)
    {
        u8 value = (u8)text[index];
        if (((value >= (u8)'a') && (value <= (u8)'z'))
            || ((value >= (u8)'A') && (value <= (u8)'Z'))
            || ((value >= (u8)'0') && (value <= (u8)'9'))
            || (value == (u8)'_')
            || (value == (u8)'.')
            || (value == (u8)'-')
            || (value == (u8)'@')
            || (value == (u8)'$'))
        {
            (void)linux_exec64_write(console_capability, owner_id, &value, 1u);
        }
        else
        {
            value = (u8)'?';
            (void)linux_exec64_write(console_capability, owner_id, &value, 1u);
        }
    }
}

static void linux_exec64_write_path_canonical(
    u32 console_capability,
    u32 owner_id,
    const u8 *path,
    u32 path_bytes)
{
    u32 index;

    if ((path == 0) || (path_bytes == 0u))
    {
        (void)linux_exec64_write_text(console_capability, owner_id, "<none>");
        return;
    }

    for (index = 0u; index < path_bytes; ++index)
    {
        u8 value = path[index];
        if ((value >= (u8)'a') && (value <= (u8)'z'))
        {
            value = (u8)(value - ((u8)'a' - (u8)'A'));
        }
        (void)linux_exec64_write(console_capability, owner_id, &value, 1u);
    }
}

static void linux_exec64_write_dec_u32(
    u32 console_capability,
    u32 owner_id,
    u32 value)
{
    char reverse[10];
    char output[10];
    u32 reverse_count = 0u;
    u32 output_count = 0u;

    if (value == 0u)
    {
        (void)linux_exec64_write_text(console_capability, owner_id, "0");
        return;
    }

    while ((value != 0u) && (reverse_count < sizeof(reverse)))
    {
        reverse[reverse_count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (reverse_count != 0u)
    {
        output[output_count++] = reverse[--reverse_count];
    }
    (void)linux_exec64_write(console_capability, owner_id, (const u8 *)output, output_count);
}

static void linux_exec64_write_hex_u32(
    u32 console_capability,
    u32 owner_id,
    u32 value)
{
    static const char hex[] = "0123456789ABCDEF";
    char output[10];
    u32 index;

    output[0] = '0';
    output[1] = 'x';
    for (index = 0u; index < 8u; ++index)
    {
        output[2u + index] = hex[(value >> (28u - (index * 4u))) & 0x0Fu];
    }
    (void)linux_exec64_write(console_capability, owner_id, (const u8 *)output, sizeof(output));
}

static void linux_exec64_write_hex_u64(
    u32 console_capability,
    u32 owner_id,
    u64 value)
{
    static const char hex[] = "0123456789ABCDEF";
    char output[18];
    u32 index;

    output[0] = '0';
    output[1] = 'x';
    for (index = 0u; index < 16u; ++index)
    {
        output[2u + index] = hex[(value >> (60u - (index * 4u))) & 0x0Full];
    }
    (void)linux_exec64_write(console_capability, owner_id, (const u8 *)output, sizeof(output));
}

static const char *linux_exec64_stage_name(linux_exec64_stage_t stage)
{
    switch (stage)
    {
    case LINUX_EXEC64_STAGE_ARGUMENT:
        return "argument";
    case LINUX_EXEC64_STAGE_CAPABILITY:
        return "capability";
    case LINUX_EXEC64_STAGE_READ:
        return "read";
    case LINUX_EXEC64_STAGE_ELF:
        return "elf";
    case LINUX_EXEC64_STAGE_STATIC:
        return "static";
    case LINUX_EXEC64_STAGE_PROCESS:
        return "process";
    case LINUX_EXEC64_STAGE_LAUNCH:
        return "launch";
    case LINUX_EXEC64_STAGE_TASK:
        return "task";
    case LINUX_EXEC64_STAGE_RUN:
        return "run";
    case LINUX_EXEC64_STAGE_CLEANUP:
        return "cleanup";
    case LINUX_EXEC64_STAGE_DYNAMIC:
        return "dynamic";
    default:
        return "none";
    }
}

static u32 linux_exec64_mix_checksum(u32 checksum, u8 value)
{
    checksum ^= (u32)value;
    checksum *= 16777619u;
    return checksum;
}

static u32 linux_exec64_range_available(u32 size, u64 offset, u64 bytes)
{
    if (bytes == 0ull)
    {
        return 1u;
    }
    if ((offset > (u64)size) || (bytes > ((u64)size - offset)))
    {
        return 0u;
    }
    return 1u;
}

static u64 linux_exec64_read_le64(const u8 *bytes)
{
    return ((u64)bytes[0])
        | (((u64)bytes[1]) << 8u)
        | (((u64)bytes[2]) << 16u)
        | (((u64)bytes[3]) << 24u)
        | (((u64)bytes[4]) << 32u)
        | (((u64)bytes[5]) << 40u)
        | (((u64)bytes[6]) << 48u)
        | (((u64)bytes[7]) << 56u);
}

static u32 linux_exec64_read_le32(const u8 *bytes)
{
    return ((u32)bytes[0])
        | (((u32)bytes[1]) << 8u)
        | (((u32)bytes[2]) << 16u)
        | (((u32)bytes[3]) << 24u);
}

static u32 linux_exec64_read_le16(const u8 *bytes)
{
    return ((u32)bytes[0])
        | (((u32)bytes[1]) << 8u);
}

static u32 linux_exec64_vaddr_to_file_offset(
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 vaddr,
    u64 bytes,
    u64 *out_offset)
{
    u32 index;
    u64 vaddr_end;

    if (out_offset != 0)
    {
        *out_offset = 0ull;
    }
    if ((phdrs == 0)
        || (out_offset == 0)
        || (phdr_count > ELF64_MAX_PROGRAM_HEADERS)
        || (bytes == 0ull))
    {
        return 0u;
    }
    vaddr_end = vaddr + bytes;
    if (vaddr_end < vaddr)
    {
        return 0u;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 file_vaddr_end;
        u64 relative;
        u64 file_offset;

        if ((phdr->type != ELF64_PT_LOAD) || (phdr->filesz == 0ull))
        {
            continue;
        }
        file_vaddr_end = phdr->vaddr + phdr->filesz;
        if (file_vaddr_end < phdr->vaddr)
        {
            continue;
        }
        if ((vaddr < phdr->vaddr) || (vaddr_end > file_vaddr_end))
        {
            continue;
        }
        relative = vaddr - phdr->vaddr;
        file_offset = phdr->offset + relative;
        if (file_offset < phdr->offset)
        {
            return 0u;
        }
        *out_offset = file_offset;
        return 1u;
    }

    return 0u;
}

static u32 linux_exec64_copy_dynamic_string(
    const u8 *strtab,
    u32 strtab_bytes,
    u32 name_offset,
    char *target,
    u32 target_bytes,
    u32 *out_length,
    u32 *out_checksum)
{
    u32 index;
    u32 checksum = 2166136261u;
    u32 copy_bytes = 0u;

    if (out_length != 0)
    {
        *out_length = 0u;
    }
    if (out_checksum != 0)
    {
        *out_checksum = 0u;
    }
    if ((strtab == 0)
        || (target == 0)
        || (target_bytes == 0u)
        || (out_length == 0)
        || (out_checksum == 0)
        || (name_offset >= strtab_bytes))
    {
        return 0u;
    }

    target[0] = (char)0;
    for (index = name_offset; index < strtab_bytes; ++index)
    {
        u8 value = strtab[index];

        if (value == 0u)
        {
            if (copy_bytes < target_bytes)
            {
                target[copy_bytes] = (char)0;
            }
            else
            {
                target[target_bytes - 1u] = (char)0;
            }
            *out_length = copy_bytes;
            *out_checksum = checksum;
            return 1u;
        }
        checksum = linux_exec64_mix_checksum(checksum, value);
        if ((copy_bytes + 1u) >= target_bytes)
        {
            target[target_bytes - 1u] = (char)0;
            return 0u;
        }
        if ((copy_bytes + 1u) < target_bytes)
        {
            target[copy_bytes] = (char)value;
        }
        ++copy_bytes;
    }

    target[target_bytes - 1u] = (char)0;
    return 0u;
}

static u32 linux_exec64_record_dynamic_symbol_name(
    const u8 *binary,
    u32 binary_bytes,
    u64 symtab_offset,
    u64 syment,
    const u8 *strtab,
    u32 strtab_bytes,
    u32 symbol_index,
    char *target,
    u32 target_bytes,
    u32 *out_length,
    u32 *out_checksum)
{
    u64 symbol_offset;
    u64 symbol_entry_offset;
    u32 name_offset;

    if ((binary == 0)
        || (strtab == 0)
        || (target == 0)
        || (syment != LINUX_EXEC64_SYMBOL_ENTRY_BYTES)
        || (out_length == 0)
        || (out_checksum == 0))
    {
        return 0u;
    }

    symbol_offset = ((u64)symbol_index) * syment;
    if ((symbol_index != 0u) && ((symbol_offset / (u64)symbol_index) != syment))
    {
        return 0u;
    }
    symbol_entry_offset = symtab_offset + symbol_offset;
    if ((symbol_entry_offset < symtab_offset)
        || (linux_exec64_range_available(
                binary_bytes,
                symbol_entry_offset,
                LINUX_EXEC64_SYMBOL_ENTRY_BYTES) == 0u))
    {
        return 0u;
    }

    name_offset = linux_exec64_read_le32(binary + symbol_entry_offset);
    return linux_exec64_copy_dynamic_string(
        strtab,
        strtab_bytes,
        name_offset,
        target,
        target_bytes,
        out_length,
        out_checksum);
}

static u32 linux_exec64_dynamic_symbol_is_undefined_weak(
    const u8 *binary,
    u32 binary_bytes,
    u64 symtab_offset,
    u64 syment,
    u32 symbol_index,
    u32 *out_undefined_weak)
{
    u64 symbol_offset;
    u64 symbol_entry_offset;
    u32 bind;
    u32 section_index;

    if (out_undefined_weak != 0)
    {
        *out_undefined_weak = 0u;
    }
    if ((binary == 0)
        || (out_undefined_weak == 0)
        || (symbol_index == 0u)
        || (syment != LINUX_EXEC64_SYMBOL_ENTRY_BYTES))
    {
        return 0u;
    }

    symbol_offset = ((u64)symbol_index) * syment;
    if ((symbol_offset / (u64)symbol_index) != syment)
    {
        return 0u;
    }
    symbol_entry_offset = symtab_offset + symbol_offset;
    if ((symbol_entry_offset < symtab_offset)
        || (linux_exec64_range_available(
                binary_bytes,
                symbol_entry_offset,
                LINUX_EXEC64_SYMBOL_ENTRY_BYTES) == 0u))
    {
        return 0u;
    }

    bind = ((u32)binary[symbol_entry_offset + 4ull]) >> 4u;
    section_index = linux_exec64_read_le16(binary + symbol_entry_offset + 6ull);
    if ((bind == LINUX_EXEC64_SYMBOL_BIND_WEAK)
        && (section_index == LINUX_EXEC64_SYMBOL_SECTION_UNDEF))
    {
        *out_undefined_weak = 1u;
    }
    return 1u;
}

static u32 linux_exec64_symbol_binding_supported(const char *name, u32 length, u32 *out_provider)
{
    if (out_provider != 0)
    {
        *out_provider = 0u;
    }
    if ((name == 0) || (length == 0u))
    {
        return 0u;
    }
    if (linux_libc64_symbol_supported(name, length) != 0u)
    {
        if (out_provider != 0)
        {
            *out_provider = 1u;
        }
        return 1u;
    }
    if (linux_dynamic64_symbol_supported(name, length) != 0u)
    {
        if (out_provider != 0)
        {
            *out_provider = 2u;
        }
        return 1u;
    }
    return 0u;
}

static u32 linux_exec64_vaddr_is_writable_load(
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 vaddr,
    u64 bytes)
{
    u32 index;
    u64 vaddr_end;

    if ((phdrs == 0)
        || (phdr_count > ELF64_MAX_PROGRAM_HEADERS)
        || (bytes == 0ull))
    {
        return 0u;
    }
    vaddr_end = vaddr + bytes;
    if (vaddr_end < vaddr)
    {
        return 0u;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        u64 load_end;

        if ((phdrs[index].type != ELF64_PT_LOAD)
            || ((phdrs[index].flags & ELF64_PF_W) == 0u))
        {
            continue;
        }
        load_end = phdrs[index].vaddr + phdrs[index].memsz;
        if (load_end < phdrs[index].vaddr)
        {
            return 0u;
        }
        if ((vaddr >= phdrs[index].vaddr) && (vaddr_end <= load_end))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 linux_exec64_symbol_binding_value(
    const char *name,
    u32 length,
    u32 provider,
    u64 *out_value,
    u32 *out_unavailable)
{
    if (out_value != 0)
    {
        *out_value = 0ull;
    }
    if (out_unavailable != 0)
    {
        *out_unavailable = 0u;
    }
    if ((name == 0)
        || (length == 0u)
        || (out_value == 0)
        || (out_unavailable == 0))
    {
        return 0u;
    }
    if (provider == 1u)
    {
        return linux_libc64_symbol_default_address(name, length, out_value, out_unavailable);
    }
    if (provider == 2u)
    {
        *out_unavailable = 0u;
        return linux_dynamic64_symbol_default_address(name, length, out_value);
    }
    return 0u;
}

static void linux_exec64_write_le64_volatile(u64 address, u64 value)
{
    volatile u8 *target = (volatile u8 *)(u64)address;

    target[0] = (u8)(value & 0xFFull);
    target[1] = (u8)((value >> 8u) & 0xFFull);
    target[2] = (u8)((value >> 16u) & 0xFFull);
    target[3] = (u8)((value >> 24u) & 0xFFull);
    target[4] = (u8)((value >> 32u) & 0xFFull);
    target[5] = (u8)((value >> 40u) & 0xFFull);
    target[6] = (u8)((value >> 48u) & 0xFFull);
    target[7] = (u8)((value >> 56u) & 0xFFull);
}

static u64 linux_exec64_read_le64_volatile(u64 address)
{
    volatile u8 *source = (volatile u8 *)(u64)address;

    return ((u64)source[0])
        | (((u64)source[1]) << 8u)
        | (((u64)source[2]) << 16u)
        | (((u64)source[3]) << 24u)
        | (((u64)source[4]) << 32u)
        | (((u64)source[5]) << 40u)
        | (((u64)source[6]) << 48u)
        | (((u64)source[7]) << 56u);
}

static u32 linux_exec64_name_matches(
    const char *left,
    u32 left_bytes,
    const char *right,
    u32 right_bytes);

static u32 linux_exec64_walk_dynamic_bindings(
    u32 process_pid,
    const u8 *binary,
    u32 binary_bytes,
    u64 table_offset,
    u32 table_count,
    u64 symtab_offset,
    u64 syment,
    const u8 *strtab,
    u32 strtab_bytes,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u32 table_kind,
    u32 apply_safe)
{
    char symbol_name[LINUX_EXEC64_SYMBOL_TOKEN_BYTES];
    u32 index;

    if ((binary == 0)
        || (strtab == 0)
        || (phdrs == 0)
        || (table_count == 0u)
        || (syment != LINUX_EXEC64_SYMBOL_ENTRY_BYTES)
        || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        return 0u;
    }

    for (index = 0u; index < table_count; ++index)
    {
        u64 entry_offset = table_offset + ((u64)index * LINUX_EXEC64_RELA_ENTRY_BYTES);
        u64 target;
        u64 info;
        u64 value = 0ull;
        u32 reloc_type;
        u32 symbol_index;
        u32 symbol_bytes = 0u;
        u32 symbol_checksum = 0u;
        u32 provider = 0u;
        u32 undefined_weak = 0u;
        u32 unavailable = 0u;
        u32 dry_value_ready = 0u;
        u32 target_valid = 0u;
        u32 apply_ready = 0u;
        u32 start_main = 0u;

        if ((entry_offset < table_offset)
            || (linux_exec64_range_available(
                    binary_bytes,
                    entry_offset,
                    LINUX_EXEC64_RELA_ENTRY_BYTES) == 0u))
        {
            return 0u;
        }

        target = linux_exec64_read_le64(binary + entry_offset);
        info = linux_exec64_read_le64(binary + entry_offset + 8ull);
        reloc_type = (u32)info;
        symbol_index = (u32)(info >> 32);
        ++g_linux_exec64_telemetry.dynamic_binding_total;
        ++g_linux_exec64_telemetry.dynamic_reloc_dry_total;
        if (apply_safe != 0u)
        {
            ++g_linux_exec64_telemetry.dynamic_reloc_apply_total;
        }
        target_valid = linux_exec64_vaddr_is_writable_load(phdrs, phdr_count, target, 8ull);
        if (target_valid != 0u)
        {
            ++g_linux_exec64_telemetry.dynamic_reloc_dry_target_valid;
        }
        else
        {
            g_linux_exec64_telemetry.dynamic_reloc_dry_error = 3u;
        }
        if (g_linux_exec64_telemetry.dynamic_reloc_dry_first_target == 0ull)
        {
            g_linux_exec64_telemetry.dynamic_reloc_dry_first_target = target;
        }
        if ((table_kind == 2u) && (g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_target == 0ull))
        {
            g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_target = target;
        }
        if (reloc_type == LINUX_EXEC64_RELOC_TYPE_GLOB_DAT)
        {
            ++g_linux_exec64_telemetry.dynamic_binding_glob_dat;
        }
        else if (reloc_type == LINUX_EXEC64_RELOC_TYPE_JUMP_SLOT)
        {
            ++g_linux_exec64_telemetry.dynamic_binding_jump_slot;
        }
        else
        {
            ++g_linux_exec64_telemetry.dynamic_binding_other;
            g_linux_exec64_telemetry.dynamic_reloc_dry_error = 4u;
        }

        if (symbol_index == 0u)
        {
            ++g_linux_exec64_telemetry.dynamic_binding_missing;
            continue;
        }
        linux_exec64_zero(symbol_name, sizeof(symbol_name));
        if (linux_exec64_record_dynamic_symbol_name(
                binary,
                binary_bytes,
                symtab_offset,
                syment,
                strtab,
                strtab_bytes,
                symbol_index,
                symbol_name,
                sizeof(symbol_name),
                &symbol_bytes,
                &symbol_checksum) == 0u)
        {
            ++g_linux_exec64_telemetry.dynamic_binding_missing;
            continue;
        }
        (void)symbol_checksum;
        if (linux_exec64_dynamic_symbol_is_undefined_weak(
                binary,
                binary_bytes,
                symtab_offset,
                syment,
                symbol_index,
                &undefined_weak) == 0u)
        {
            ++g_linux_exec64_telemetry.dynamic_binding_missing;
            continue;
        }
        start_main = linux_exec64_name_matches(
            symbol_name,
            symbol_bytes,
            "__libc_start_main",
            17u);
        if (linux_exec64_symbol_binding_supported(symbol_name, symbol_bytes, &provider) != 0u)
        {
            ++g_linux_exec64_telemetry.dynamic_binding_supported;
            if (linux_exec64_symbol_binding_value(
                    symbol_name,
                    symbol_bytes,
                    provider,
                    &value,
                    &unavailable) != 0u)
            {
                dry_value_ready = 1u;
                ++g_linux_exec64_telemetry.dynamic_reloc_dry_provider;
                if (unavailable != 0u)
                {
                    ++g_linux_exec64_telemetry.dynamic_reloc_dry_unavailable;
                }
                if ((provider == 1u) && (start_main != 0u))
                {
                    g_linux_exec64_telemetry.dynamic_libc_start_main = 1u;
                    g_linux_exec64_telemetry.dynamic_libc_start_main_value = value;
                }
            }
            else
            {
                g_linux_exec64_telemetry.dynamic_reloc_dry_error = 5u;
            }
            if (provider == 1u)
            {
                ++g_linux_exec64_telemetry.dynamic_binding_libc;
                if (unavailable != 0u)
                {
                    ++g_linux_exec64_telemetry.dynamic_binding_unavailable;
                }
            }
            else if (provider == 2u)
            {
                ++g_linux_exec64_telemetry.dynamic_binding_interp;
            }
        }
        else
        {
            if ((reloc_type == LINUX_EXEC64_RELOC_TYPE_GLOB_DAT) && (undefined_weak != 0u))
            {
                ++g_linux_exec64_telemetry.dynamic_binding_weak_null;
                ++g_linux_exec64_telemetry.dynamic_reloc_dry_weak_null;
                value = 0ull;
                dry_value_ready = 1u;
            }
            else
            {
                ++g_linux_exec64_telemetry.dynamic_binding_missing;
                g_linux_exec64_telemetry.dynamic_reloc_dry_error = 6u;
            }
        }
        if (dry_value_ready != 0u)
        {
            ++g_linux_exec64_telemetry.dynamic_reloc_dry_value;
            if (g_linux_exec64_telemetry.dynamic_reloc_dry_first_set == 0u)
            {
                g_linux_exec64_telemetry.dynamic_reloc_dry_first_value = value;
                g_linux_exec64_telemetry.dynamic_reloc_dry_first_set = 1u;
            }
            if ((table_kind == 2u) && (g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_value == 0ull))
            {
                g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_value = value;
            }
            if ((unavailable == 0u)
                && ((reloc_type == LINUX_EXEC64_RELOC_TYPE_GLOB_DAT)
                    || (reloc_type == LINUX_EXEC64_RELOC_TYPE_JUMP_SLOT))
                && (target_valid != 0u))
            {
                apply_ready = 1u;
                ++g_linux_exec64_telemetry.dynamic_reloc_dry_apply_ready;
            }
            else
            {
                ++g_linux_exec64_telemetry.dynamic_reloc_dry_blocked;
                if (apply_safe != 0u)
                {
                    ++g_linux_exec64_telemetry.dynamic_reloc_apply_blocked;
                    if (unavailable != 0u)
                    {
                        ++g_linux_exec64_telemetry.dynamic_reloc_apply_unavailable;
                    }
                }
            }
            if ((apply_safe != 0u) && (apply_ready != 0u))
            {
                u64 readback;
                u32 switch_ok;
                u32 restore_ok;

                switch_ok = paging64_switch_to_process_root(process_pid, 0x44594E57u);
                if (switch_ok == 0u)
                {
                    g_linux_exec64_telemetry.dynamic_reloc_apply_error = 2u;
                    return 0u;
                }
                linux_exec64_write_le64_volatile(target, value);
                ++g_linux_exec64_telemetry.dynamic_reloc_apply_write;
                readback = linux_exec64_read_le64_volatile(target);
                restore_ok = paging64_switch_to_kernel_root(0x44594E58u);
                if (restore_ok == 0u)
                {
                    g_linux_exec64_telemetry.dynamic_reloc_apply_error = 3u;
                    return 0u;
                }
                if (readback == value)
                {
                    ++g_linux_exec64_telemetry.dynamic_reloc_apply_readback;
                    if (target == g_linux_exec64_telemetry.dynamic_reloc_dry_first_target)
                    {
                        g_linux_exec64_telemetry.dynamic_reloc_apply_first_readback = readback;
                    }
                    if (target == g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_target)
                    {
                        g_linux_exec64_telemetry.dynamic_reloc_apply_jmprel_readback = readback;
                    }
                    if (start_main != 0u)
                    {
                        g_linux_exec64_telemetry.dynamic_libc_start_main_apply = 1u;
                        g_linux_exec64_telemetry.dynamic_libc_start_main_readback = readback;
                    }
                }
                else
                {
                    g_linux_exec64_telemetry.dynamic_reloc_apply_error = 1u;
                }
            }
        }
    }

    return 1u;
}

static u32 linux_exec64_name_matches(
    const char *left,
    u32 left_bytes,
    const char *right,
    u32 right_bytes)
{
    u32 index;

    if ((left == 0) || (right == 0) || (left_bytes != right_bytes))
    {
        return 0u;
    }
    for (index = 0u; index < left_bytes; ++index)
    {
        if ((u8)left[index] != (u8)right[index])
        {
            return 0u;
        }
    }
    return 1u;
}

static u32 linux_exec64_interp_path_supported(const u8 *path, u32 path_bytes)
{
    static const char preferred_path[] = "/lib/ld-limitless.so";
    static const char lib64_path[] = "/lib64/ld-limitless.so";
    static const char nvme_path[] = "/nvme/apps/ldlimit";

    if (path == 0)
    {
        return 0u;
    }
    if (linux_exec64_name_matches(
            (const char *)path,
            path_bytes,
            preferred_path,
            (u32)sizeof(preferred_path) - 1u) != 0u)
    {
        return 1u;
    }
    if (linux_exec64_name_matches(
            (const char *)path,
            path_bytes,
            nvme_path,
            (u32)sizeof(nvme_path) - 1u) != 0u)
    {
        return 1u;
    }
    return linux_exec64_name_matches(
        (const char *)path,
        path_bytes,
        lib64_path,
        (u32)sizeof(lib64_path) - 1u);
}

static u32 linux_exec64_interp_backend_path(
    const u8 *path,
    u32 path_bytes,
    const u8 **backend_path,
    u32 *backend_path_bytes)
{
    static const char preferred_path[] = "/lib/ld-limitless.so";
    static const char lib64_path[] = "/lib64/ld-limitless.so";
    static const char nvme_path[] = "/nvme/apps/ldlimit";
    static const u8 backend[] = "/APPS/LDLIMIT";

    if (backend_path != 0)
    {
        *backend_path = 0;
    }
    if (backend_path_bytes != 0)
    {
        *backend_path_bytes = 0u;
    }
    if ((path == 0) || (backend_path == 0) || (backend_path_bytes == 0))
    {
        return 0u;
    }
    if ((linux_exec64_name_matches(
             (const char *)path,
             path_bytes,
             preferred_path,
             (u32)sizeof(preferred_path) - 1u) == 0u)
        && (linux_exec64_name_matches(
             (const char *)path,
             path_bytes,
             lib64_path,
             (u32)sizeof(lib64_path) - 1u) == 0u)
        && (linux_exec64_name_matches(
             (const char *)path,
             path_bytes,
             nvme_path,
             (u32)sizeof(nvme_path) - 1u) == 0u))
    {
        return 0u;
    }

    *backend_path = backend;
    *backend_path_bytes = (u32)sizeof(backend) - 1u;
    return 1u;
}

static u32 linux_exec64_record_interp_metadata(
    const u8 *binary,
    u32 binary_bytes,
    const elf64_program_header_t *phdrs,
    u32 phdr_count)
{
    u32 index;

    if ((binary == 0) || (phdrs == 0))
    {
        return 0u;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u32 checksum = 2166136261u;
        u32 byte_index;

        if (phdr->type != ELF64_PT_INTERP)
        {
            continue;
        }
        if ((phdr->filesz == 0ull)
            || (phdr->filesz > LINUX_DYNAMIC64_INTERP_PATH_MAX)
            || (linux_exec64_range_available(binary_bytes, phdr->offset, phdr->filesz) == 0u))
        {
            return 0u;
        }

        for (byte_index = 0u; byte_index < (u32)phdr->filesz; ++byte_index)
        {
            u8 value = binary[phdr->offset + byte_index];
            if (value == 0u)
            {
                g_linux_exec64_telemetry.interp_path_bytes = byte_index;
                g_linux_exec64_telemetry.interp_path_checksum = checksum;
                g_linux_exec64_telemetry.interp_supported = linux_exec64_interp_path_supported(
                    binary + phdr->offset,
                    byte_index);
                return 1u;
            }
            checksum = linux_exec64_mix_checksum(checksum, value);
        }
        return 0u;
    }

    return 0u;
}

static void linux_exec64_record_interpreter_file_metadata(u8 *binary, u32 binary_bytes)
{
    elf64_header_t header;
    elf64_program_header_t phdrs[ELF64_MAX_PROGRAM_HEADERS];
    elf64_phdr_summary_t summary;
    u32 index;

    g_linux_exec64_telemetry.interp_file_bytes = binary_bytes;
    if ((binary == 0) || (binary_bytes == 0u))
    {
        g_linux_exec64_telemetry.interp_file_error = ELF64_ERROR_NULL;
        return;
    }

    if (elf64_parse_header(binary, binary_bytes, &header) != ELF64_OK)
    {
        g_linux_exec64_telemetry.interp_file_error = header.error;
        return;
    }
    for (index = 0u; index < ELF64_MAX_PROGRAM_HEADERS; ++index)
    {
        phdrs[index].type = 0u;
        phdrs[index].flags = 0u;
        phdrs[index].offset = 0ull;
        phdrs[index].vaddr = 0ull;
        phdrs[index].paddr = 0ull;
        phdrs[index].filesz = 0ull;
        phdrs[index].memsz = 0ull;
        phdrs[index].align = 0ull;
    }
    if (elf64_parse_phdrs(
            binary,
            binary_bytes,
            &header,
            phdrs,
            ELF64_MAX_PROGRAM_HEADERS,
            &summary) != ELF64_OK)
    {
        g_linux_exec64_telemetry.interp_file_error = summary.error;
        return;
    }

    g_linux_exec64_telemetry.interp_file_elf = 1u;
    g_linux_exec64_telemetry.interp_file_type = (u32)header.type;
    g_linux_exec64_telemetry.interp_file_load_count = summary.load_count;
    g_linux_exec64_telemetry.interp_file_interp_count = summary.interp_count;
    g_linux_exec64_telemetry.interp_file_dynamic_count = summary.dynamic_count;
    g_linux_exec64_telemetry.interp_file_error = ELF64_ERROR_NONE;
}

static u32 linux_exec64_probe_interpreter_file(
    u32 owner_id,
    const u8 *interp_path,
    u32 interp_path_bytes,
    u32 source)
{
    const u8 *backend_path;
    u32 backend_path_bytes;
    u32 bytes_read = 0u;

    if ((g_linux_exec64_telemetry.interp_supported == 0u)
        || (linux_exec64_interp_backend_path(
                interp_path,
                interp_path_bytes,
                &backend_path,
                &backend_path_bytes) == 0u))
    {
        return 0u;
    }

    g_linux_exec64_telemetry.interp_file_attempt = 1u;
    if (linux_exec64_read_source(
            source,
            backend_path,
            backend_path_bytes,
            g_linux_exec64_binary,
            LINUX_EXEC64_STAGING_BUFFER_BYTES,
            owner_id,
            &bytes_read) == 0u)
    {
        g_linux_exec64_telemetry.interp_file_nvme_error =
            (source == LINUX_EXEC64_SOURCE_NVME) ? mmio64_nvme_fat_shell_read_last_error() : boot_media64_last_error();
        g_linux_exec64_telemetry.interp_file_error = g_linux_exec64_telemetry.interp_file_nvme_error;
        return 0u;
    }

    g_linux_exec64_telemetry.interp_file_read = 1u;
    g_linux_exec64_telemetry.interp_file_nvme_error =
        (source == LINUX_EXEC64_SOURCE_NVME) ? mmio64_nvme_fat_shell_read_last_error() : boot_media64_last_error();
    linux_exec64_record_interpreter_file_metadata(g_linux_exec64_binary, bytes_read);
    return (g_linux_exec64_telemetry.interp_file_elf != 0u) ? 1u : 0u;
}

static void linux_exec64_record_dynamic_relocations(
    u32 process_pid,
    const u8 *binary,
    u32 binary_bytes,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u32 apply_safe)
{
    u32 index;
    u64 rela_vaddr = 0ull;
    u64 rela_bytes = 0ull;
    u64 relaent = 0ull;
    u64 jmprel_vaddr = 0ull;
    u64 pltrel_bytes = 0ull;
    u64 pltrel = 0ull;
    u64 symtab_vaddr = 0ull;
    u64 strtab_vaddr = 0ull;
    u64 strtab_bytes = 0ull;
    u64 syment = 0ull;
    u64 first_needed_offset = 0ull;
    u64 rela_table_offset = 0ull;
    u64 jmprel_table_offset = 0ull;
    u32 first_needed_seen = 0u;

    g_linux_exec64_telemetry.dynamic_reloc = 1u;
    g_linux_exec64_telemetry.dynamic_symbol_trace = 1u;
    linux_exec64_zero(g_linux_exec64_dynamic_needed_name, sizeof(g_linux_exec64_dynamic_needed_name));
    linux_exec64_zero(g_linux_exec64_dynamic_reloc_symbol, sizeof(g_linux_exec64_dynamic_reloc_symbol));
    linux_exec64_zero(g_linux_exec64_dynamic_jmprel_symbol, sizeof(g_linux_exec64_dynamic_jmprel_symbol));
    if ((binary == 0)
        || (phdrs == 0)
        || (binary_bytes == 0u)
        || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        g_linux_exec64_telemetry.dynamic_reloc_error = 1u;
        return;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 cursor;
        u64 end;

        if (phdr->type != ELF64_PT_DYNAMIC)
        {
            continue;
        }
        if ((phdr->filesz == 0ull)
            || ((phdr->filesz & 15ull) != 0ull)
            || (linux_exec64_range_available(binary_bytes, phdr->offset, phdr->filesz) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_reloc_error = 2u;
            return;
        }

        cursor = phdr->offset;
        end = phdr->offset + phdr->filesz;
        if (end < phdr->offset)
        {
            g_linux_exec64_telemetry.dynamic_reloc_error = 3u;
            return;
        }
        while (cursor < end)
        {
            u64 tag = linux_exec64_read_le64(binary + cursor);
            u64 value = linux_exec64_read_le64(binary + cursor + 8ull);

            if (tag == 0ull)
            {
                break;
            }
            if (tag == LINUX_EXEC64_DT_RELA)
            {
                rela_vaddr = value;
            }
            else if (tag == LINUX_EXEC64_DT_RELASZ)
            {
                rela_bytes = value;
            }
            else if (tag == LINUX_EXEC64_DT_RELAENT)
            {
                relaent = value;
            }
            else if (tag == LINUX_EXEC64_DT_SYMTAB)
            {
                symtab_vaddr = value;
            }
            else if (tag == LINUX_EXEC64_DT_STRTAB)
            {
                strtab_vaddr = value;
            }
            else if (tag == LINUX_EXEC64_DT_STRSZ)
            {
                strtab_bytes = value;
            }
            else if (tag == LINUX_EXEC64_DT_SYMENT)
            {
                syment = value;
            }
            else if ((tag == LINUX_DYNAMIC64_DT_NEEDED) && (first_needed_seen == 0u))
            {
                first_needed_offset = value;
                first_needed_seen = 1u;
            }
            else if (tag == LINUX_EXEC64_DT_JMPREL)
            {
                jmprel_vaddr = value;
            }
            else if (tag == LINUX_EXEC64_DT_PLTRELSZ)
            {
                pltrel_bytes = value;
            }
            else if (tag == LINUX_EXEC64_DT_PLTREL)
            {
                pltrel = value;
            }
            cursor += 16ull;
        }
        break;
    }

    if (index == phdr_count)
    {
        g_linux_exec64_telemetry.dynamic_reloc_error = 4u;
        return;
    }
    if (relaent == 0ull)
    {
        relaent = LINUX_EXEC64_RELA_ENTRY_BYTES;
    }
    if (relaent != LINUX_EXEC64_RELA_ENTRY_BYTES)
    {
        g_linux_exec64_telemetry.dynamic_reloc_error = 5u;
        return;
    }
    g_linux_exec64_telemetry.dynamic_relaent = (u32)relaent;
    g_linux_exec64_telemetry.dynamic_pltrel = (u32)pltrel;
    g_linux_exec64_telemetry.dynamic_syment = (u32)syment;

    if (rela_vaddr != 0ull)
    {
        u64 rela_info;

        if ((rela_bytes == 0ull)
            || ((rela_bytes % relaent) != 0ull)
            || (linux_exec64_vaddr_to_file_offset(
                    phdrs,
                    phdr_count,
                    rela_vaddr,
                    rela_bytes,
                    &rela_table_offset) == 0u)
            || (linux_exec64_range_available(binary_bytes, rela_table_offset, rela_bytes) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_reloc_error = 6u;
            return;
        }
        g_linux_exec64_telemetry.dynamic_rela_count = (u32)(rela_bytes / relaent);
        g_linux_exec64_telemetry.dynamic_reloc_first_target =
            linux_exec64_read_le64(binary + rela_table_offset);
        rela_info = linux_exec64_read_le64(binary + rela_table_offset + 8ull);
        g_linux_exec64_telemetry.dynamic_reloc_first_type =
            (u32)rela_info;
        g_linux_exec64_telemetry.dynamic_reloc_symbol_index = (u32)(rela_info >> 32);
    }

    if (jmprel_vaddr != 0ull)
    {
        u64 jmprel_info;

        if ((pltrel_bytes == 0ull)
            || ((pltrel_bytes % relaent) != 0ull)
            || ((pltrel != 0ull) && (pltrel != LINUX_EXEC64_DT_RELA_VALUE))
            || (linux_exec64_vaddr_to_file_offset(
                    phdrs,
                    phdr_count,
                    jmprel_vaddr,
                    pltrel_bytes,
                    &jmprel_table_offset) == 0u)
            || (linux_exec64_range_available(binary_bytes, jmprel_table_offset, pltrel_bytes) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_reloc_error = 7u;
            return;
        }
        g_linux_exec64_telemetry.dynamic_jmprel_count = (u32)(pltrel_bytes / relaent);
        g_linux_exec64_telemetry.dynamic_jmprel_first_target =
            linux_exec64_read_le64(binary + jmprel_table_offset);
        jmprel_info = linux_exec64_read_le64(binary + jmprel_table_offset + 8ull);
        g_linux_exec64_telemetry.dynamic_jmprel_first_type =
            (u32)jmprel_info;
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_index = (u32)(jmprel_info >> 32);
    }

    if ((g_linux_exec64_telemetry.dynamic_rela_count == 0u)
        && (g_linux_exec64_telemetry.dynamic_jmprel_count == 0u))
    {
        g_linux_exec64_telemetry.dynamic_reloc_error = 8u;
        return;
    }

    if ((symtab_vaddr == 0ull)
        || (strtab_vaddr == 0ull)
        || (strtab_bytes == 0ull)
        || (strtab_bytes > 4096ull)
        || (syment == 0ull))
    {
        g_linux_exec64_telemetry.dynamic_symbol_error = 1u;
        return;
    }
    {
        u64 symtab_offset;
        u64 strtab_offset;

        if ((linux_exec64_vaddr_to_file_offset(
                phdrs,
                phdr_count,
                symtab_vaddr,
                syment,
                &symtab_offset) == 0u)
            || (linux_exec64_vaddr_to_file_offset(
                phdrs,
                phdr_count,
                strtab_vaddr,
                strtab_bytes,
                &strtab_offset) == 0u)
            || (linux_exec64_range_available(binary_bytes, strtab_offset, strtab_bytes) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_symbol_error = 2u;
            return;
        }

        g_linux_exec64_telemetry.dynamic_symtab = 1u;
        g_linux_exec64_telemetry.dynamic_strtab = (u32)strtab_bytes;
        if ((first_needed_seen == 0u)
            || (first_needed_offset > 0xFFFFFFFFull)
            || (linux_exec64_copy_dynamic_string(
                    binary + strtab_offset,
                    (u32)strtab_bytes,
                    (u32)first_needed_offset,
                    g_linux_exec64_dynamic_needed_name,
                    sizeof(g_linux_exec64_dynamic_needed_name),
                    &g_linux_exec64_telemetry.dynamic_needed_name_bytes,
                    &g_linux_exec64_telemetry.dynamic_needed_name_checksum) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_symbol_error = 3u;
            return;
        }
        if ((g_linux_exec64_telemetry.dynamic_reloc_symbol_index != 0u)
            && (linux_exec64_record_dynamic_symbol_name(
                    binary,
                    binary_bytes,
                    symtab_offset,
                    syment,
                    binary + strtab_offset,
                    (u32)strtab_bytes,
                    g_linux_exec64_telemetry.dynamic_reloc_symbol_index,
                    g_linux_exec64_dynamic_reloc_symbol,
                    sizeof(g_linux_exec64_dynamic_reloc_symbol),
                    &g_linux_exec64_telemetry.dynamic_reloc_symbol_bytes,
                    &g_linux_exec64_telemetry.dynamic_reloc_symbol_checksum) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_symbol_error = 4u;
            return;
        }
        if ((g_linux_exec64_telemetry.dynamic_jmprel_symbol_index != 0u)
            && (linux_exec64_record_dynamic_symbol_name(
                    binary,
                    binary_bytes,
                    symtab_offset,
                    syment,
                    binary + strtab_offset,
                    (u32)strtab_bytes,
                    g_linux_exec64_telemetry.dynamic_jmprel_symbol_index,
                    g_linux_exec64_dynamic_jmprel_symbol,
                    sizeof(g_linux_exec64_dynamic_jmprel_symbol),
                    &g_linux_exec64_telemetry.dynamic_jmprel_symbol_bytes,
                    &g_linux_exec64_telemetry.dynamic_jmprel_symbol_checksum) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_symbol_error = 5u;
        }
        g_linux_exec64_telemetry.dynamic_binding_walk = 1u;
        g_linux_exec64_telemetry.dynamic_reloc_dry_run = 1u;
        if (apply_safe != 0u)
        {
            g_linux_exec64_telemetry.dynamic_reloc_apply = 1u;
        }
        if ((g_linux_exec64_telemetry.dynamic_rela_count != 0u)
            && (linux_exec64_walk_dynamic_bindings(
                    process_pid,
                    binary,
                    binary_bytes,
                    rela_table_offset,
                    g_linux_exec64_telemetry.dynamic_rela_count,
                    symtab_offset,
                    syment,
                    binary + strtab_offset,
                    (u32)strtab_bytes,
                    phdrs,
                    phdr_count,
                    1u,
                    apply_safe) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_binding_error = 1u;
            return;
        }
        if ((g_linux_exec64_telemetry.dynamic_jmprel_count != 0u)
            && (linux_exec64_walk_dynamic_bindings(
                    process_pid,
                    binary,
                    binary_bytes,
                    jmprel_table_offset,
                    g_linux_exec64_telemetry.dynamic_jmprel_count,
                    symtab_offset,
                    syment,
                    binary + strtab_offset,
                    (u32)strtab_bytes,
                    phdrs,
                    phdr_count,
                    2u,
                    apply_safe) == 0u))
        {
            g_linux_exec64_telemetry.dynamic_binding_error = 2u;
        }
    }
}

static void linux_exec64_record_elf_metadata(
    const elf64_header_t *header,
    const elf64_phdr_summary_t *summary,
    const u8 *binary,
    u32 binary_bytes,
    const elf64_program_header_t *phdrs)
{
    linux_dynamic64_needed_result_t needed;

    if (header != 0)
    {
        g_linux_exec64_telemetry.elf_type = (u32)header->type;
    }
    if (summary != 0)
    {
        g_linux_exec64_telemetry.elf_load_count = summary->load_count;
        g_linux_exec64_telemetry.elf_interp_count = summary->interp_count;
        g_linux_exec64_telemetry.elf_dynamic_count = summary->dynamic_count;
    }
    if ((header == 0)
        || (summary == 0)
        || (binary == 0)
        || (phdrs == 0)
        || (binary_bytes == 0u)
        || ((summary->interp_count == 0u) && (summary->dynamic_count == 0u)))
    {
        return;
    }

    if (summary->interp_count != 0u)
    {
        (void)linux_exec64_record_interp_metadata(
            binary,
            binary_bytes,
            phdrs,
            header->phnum);
    }

    if (summary->dynamic_count == 0u)
    {
        return;
    }

    if (linux_dynamic64_analyze_needed(
            binary,
            binary_bytes,
            header,
            phdrs,
            header->phnum,
            &needed) == LINUX_DYNAMIC64_OK)
    {
        g_linux_exec64_telemetry.dynamic_needed = needed.needed_count;
        g_linux_exec64_telemetry.dynamic_supported = needed.supported_count;
        g_linux_exec64_telemetry.dynamic_missing = needed.missing_count;
        g_linux_exec64_telemetry.dynamic_libc = needed.libc_needed_count;
        g_linux_exec64_telemetry.dynamic_pthread = needed.pthread_needed_count;
        g_linux_exec64_telemetry.dynamic_first_needed_checksum = needed.first_needed_checksum;
        g_linux_exec64_telemetry.dynamic_last_needed_checksum = needed.last_needed_checksum;
    }
}

static void linux_exec64_emit_summary(
    u32 console_capability,
    u32 owner_id,
    const u8 *path,
    u32 path_bytes)
{
    (void)linux_exec64_write_text(console_capability, owner_id, "drs-realbin path ");
    linux_exec64_write_path_canonical(console_capability, owner_id, path, path_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " provenance 1 source ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.source);
    (void)linux_exec64_write_text(console_capability, owner_id, " nvme-read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_read);
    (void)linux_exec64_write_text(console_capability, owner_id, " boot-media-read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.boot_media_read);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf);
    (void)linux_exec64_write_text(console_capability, owner_id, " static ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.static_elf);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-type ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-load ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_load_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-interp ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_interp_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_path_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-checksum ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_path_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-supported ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_supported);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-attempt ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_attempt);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_read);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-elf ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_elf);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-type ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-load ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_load_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-interp ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_interp_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-dynamic ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_dynamic_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-map-attempt ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_map_attempt);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-process ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_process);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-app-mapped ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_app_mapped);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-app-pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_app_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-interp-mapped ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_interp_mapped);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-interp-pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_interp_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-map-cleanup ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_map_cleanup);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-rela ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_rela_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_jmprel_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-relaent ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_relaent);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-pltrel ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_pltrel);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-first ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_first_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-type ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_first_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-first ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_first_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-type ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_first_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-symbol-trace ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_symbol_trace);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-symtab ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_symtab);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-strtab ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_strtab);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-syment ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_syment);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed-name ");
    linux_exec64_write_token(
        console_capability,
        owner_id,
        g_linux_exec64_dynamic_needed_name,
        g_linux_exec64_telemetry.dynamic_needed_name_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed-name-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_needed_name_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed-name-checksum ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_needed_name_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol-index ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_index);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol ");
    linux_exec64_write_token(
        console_capability,
        owner_id,
        g_linux_exec64_dynamic_reloc_symbol,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol-bytes ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol-checksum ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol-index ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_index);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol ");
    linux_exec64_write_token(
        console_capability,
        owner_id,
        g_linux_exec64_dynamic_jmprel_symbol,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol-bytes ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol-checksum ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-symbol-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_symbol_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-walk ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_walk);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-total ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_total);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-supported ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_supported);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-missing ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_missing);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-weak-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_weak_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-unavailable ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_unavailable);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-libc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_libc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-interp ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_interp);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-glob-dat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_glob_dat);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-jump-slot ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_jump_slot);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-other ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_other);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-run ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_run);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-total ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_total);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-target-valid ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_target_valid);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-value ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-provider ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_provider);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-weak-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_weak_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-unavailable ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_unavailable);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-apply-ready ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_apply_ready);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-blocked ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_blocked);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-first-target ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_first_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-first-value ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_first_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-jmprel-target ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-jmprel-value ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-total ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_total);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-write ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_write);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-readback ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-blocked ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_blocked);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-unavailable ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_unavailable);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-first-readback ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_apply_first_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-jmprel-readback ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_apply_jmprel_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_libc_start_main);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main-apply ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_libc_start_main_apply);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main-value ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_libc_start_main_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main-readback ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_libc_start_main_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-argc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_argc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-envc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_envc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-auxv ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_auxv);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-align ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_align);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-argv-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_argv_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-envp-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_envp_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-auxv-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_auxv_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-random ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_random_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-platform ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_platform_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-rsp ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_initial_rsp);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-auxv-address ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_auxv_address);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-auxv-phdr ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_auxv_phdr);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-auxv-base ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_auxv_base);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-auxv-entry ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_auxv_entry);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-ready ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_transfer_ready);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-rip ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_transfer_rip);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-rsp ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_transfer_rsp);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-task-registered ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_task_registered);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-started ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_transfer_started);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-first-syscall ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_first_syscall);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-console-bytes ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_console_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-exit-code ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_exit_code);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-dynamic ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_dynamic_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_needed);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-supported ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_supported);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-missing ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_missing);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_libc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-pthread ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_pthread);
    (void)linux_exec64_write_text(console_capability, owner_id, " mapped ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mapped_regions);
    (void)linux_exec64_write_text(console_capability, owner_id, " pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mapped_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " stack ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.stack_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " envc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.envc);
    (void)linux_exec64_write_text(console_capability, owner_id, " pml4 ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pml4);
    (void)linux_exec64_write_text(console_capability, owner_id, " pml4-pool ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pml4_pool);
    (void)linux_exec64_write_text(console_capability, owner_id, " root-pool-limit ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pml4_pool);
    (void)linux_exec64_write_text(console_capability, owner_id, " pml4-slot ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pml4_slot);
    (void)linux_exec64_write_text(console_capability, owner_id, " root ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.root_physical);
    (void)linux_exec64_write_text(console_capability, owner_id, " kernel-root ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.kernel_root_physical);
    (void)linux_exec64_write_text(console_capability, owner_id, " root-distinct ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.root_distinct);
    (void)linux_exec64_write_text(console_capability, owner_id, " high-copy ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.high_copy);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmio-shared ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mmio_shared);
    (void)linux_exec64_write_text(console_capability, owner_id, " pool-mapped ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pool_mapped);
    (void)linux_exec64_write_text(console_capability, owner_id, " low-compat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.low_compat);
    (void)linux_exec64_write_text(console_capability, owner_id, " low-pdpt-present ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.low_pdpt_present);
    (void)linux_exec64_write_text(console_capability, owner_id, " syscall-entry-high ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.syscall_entry_high);
    (void)linux_exec64_write_text(console_capability, owner_id, " idt-high ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.idt_high);
    (void)linux_exec64_write_text(console_capability, owner_id, " descriptor-high ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.descriptor_high);
    (void)linux_exec64_write_text(console_capability, owner_id, " kernel-entry-high-ready ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.kernel_entry_high_ready);
    (void)linux_exec64_write_text(console_capability, owner_id, " kernel-cr3-entry ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.kernel_cr3_entry);
    (void)linux_exec64_write_text(console_capability, owner_id, " syscall-root-repair ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_root_repair);
    (void)linux_exec64_write_text(console_capability, owner_id, " syscall-root-reload ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_root_reload);
    (void)linux_exec64_write_text(console_capability, owner_id, " syscall-root-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_root_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " fs-save ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fs_save);
    (void)linux_exec64_write_text(console_capability, owner_id, " fs-restore ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fs_restore);
    (void)linux_exec64_write_text(console_capability, owner_id, " fs-set ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fs_set);
    (void)linux_exec64_write_text(console_capability, owner_id, " user-pdpt-private ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.user_pdpt_private);
    (void)linux_exec64_write_text(console_capability, owner_id, " vma-pt-private ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.vma_pt_private);
    (void)linux_exec64_write_text(console_capability, owner_id, " cr3-start ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.cr3_start);
    (void)linux_exec64_write_text(console_capability, owner_id, " cr3-exit ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.cr3_exit);
    (void)linux_exec64_write_text(console_capability, owner_id, " cr3-syscall-entry ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.cr3_syscall_entry);
    (void)linux_exec64_write_text(console_capability, owner_id, " active-cr3-match ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.active_cr3_match);
    (void)linux_exec64_write_text(console_capability, owner_id, " root-cleanup ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.root_cleanup);
    (void)linux_exec64_write_text(console_capability, owner_id, " task ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.task);
    (void)linux_exec64_write_text(console_capability, owner_id, " started ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.started);
    (void)linux_exec64_write_text(console_capability, owner_id, " scheduler-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.scheduler_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " console-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.console_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " exit ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.exit_code);
    (void)linux_exec64_write_text(console_capability, owner_id, " cleanup ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.cleanup);
    (void)linux_exec64_write_text(console_capability, owner_id, " signal-sigpipe ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.signal_sigpipe);
    (void)linux_exec64_write_text(console_capability, owner_id, " signal-sigchld ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.signal_sigchld);
    (void)linux_exec64_write_text(console_capability, owner_id, " signal-rt-sigreturn ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.signal_rt_sigreturn);
    (void)linux_exec64_write_text(console_capability, owner_id, " signal-frame-fault ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.signal_frame_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mmap_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mmap_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mmap_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-file ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.mmap_file_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-file-bytes ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.mmap_file_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-file-denial ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.mmap_file_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-last-error ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.mmap_last_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-last-flags ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.mmap_last_flags);
    (void)linux_exec64_write_text(console_capability, owner_id, " mmap-last-length ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.mmap_last_length);
    (void)linux_exec64_write_text(console_capability, owner_id, " futex-wait ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.futex_wait);
    (void)linux_exec64_write_text(console_capability, owner_id, " futex-wake ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.futex_wake);
    (void)linux_exec64_write_text(console_capability, owner_id, " futex-woken ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.futex_woken);
    (void)linux_exec64_write_text(console_capability, owner_id, " futex-waiters-final ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.futex_waiters_final);
    (void)linux_exec64_write_text(console_capability, owner_id, " thread-exit-cleartid ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.thread_exit_cleartid);
    (void)linux_exec64_write_text(console_capability, owner_id, " thread-exit-cleartid-fault ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.thread_exit_cleartid_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " getdents64 ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getdents64_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " getdents64-entries ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getdents64_entries);
    (void)linux_exec64_write_text(console_capability, owner_id, " getdents64-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getdents64_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " stat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.stat_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " stat-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.stat_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " stat-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.stat_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " fstat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fstat_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " fstat-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fstat_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " fstat-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fstat_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " newfstatat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.newfstatat_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " newfstatat-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.newfstatat_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " newfstatat-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.newfstatat_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " lseek ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.lseek_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " lseek-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.lseek_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " dup ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dup_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " dup2 ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dup2_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " dup3 ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dup3_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " dup-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dup_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " fcntl ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fcntl_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " fcntl-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fcntl_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " readlink ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.readlink_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " readlink-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.readlink_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " readlink-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.readlink_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " readlink-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.readlink_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " readlink-last-result ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.readlink_last_result);
    (void)linux_exec64_write_text(console_capability, owner_id, " getcwd ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getcwd_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " getcwd-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getcwd_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " getcwd-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getcwd_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " getcwd-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getcwd_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " path-relative ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.path_relative);
    (void)linux_exec64_write_text(console_capability, owner_id, " path-dot ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.path_dot);
    (void)linux_exec64_write_text(console_capability, owner_id, " path-dotdot ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.path_dotdot);
    (void)linux_exec64_write_text(console_capability, owner_id, " path-trailing ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.path_trailing);
    (void)linux_exec64_write_text(console_capability, owner_id, " path-trailing-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.path_trailing_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " path-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.path_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " chdir ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.chdir_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " fchdir ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fchdir_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " chdir-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.chdir_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " chdir-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.chdir_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " openat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.openat_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.read_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " read-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.read_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " write ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.write_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " write-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.write_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-create ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-denials ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-faults ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe2 ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe2_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe2-denials ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe2_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe2-faults ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe2_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-live-final ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_live_final);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-blocks ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_blocks);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-wakes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_wakes);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-replays ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pipe_replays);
    (void)linux_exec64_write_text(console_capability, owner_id, " pipe-provider-denials ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.pipe_provider_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " fd-fork-pipe-copy ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fd_fork_pipe_copy);
    (void)linux_exec64_write_text(console_capability, owner_id, " fd-fork-pipe-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fd_fork_pipe_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " fd-fork-pipe-last-fd ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fd_fork_pipe_last_fd);
    (void)linux_exec64_write_text(console_capability, owner_id, " readv ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.readv_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " readv-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.readv_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " writev ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.writev_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " writev-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.writev_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " poll ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.poll_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " ppoll ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.ppoll_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " poll-ready ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.poll_ready);
    (void)linux_exec64_write_text(console_capability, owner_id, " poll-last-revents ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.poll_last_revents);
    (void)linux_exec64_write_text(console_capability, owner_id, " geteuid ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.geteuid_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " getppid ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.getppid_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " ioctl ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.ioctl_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " ioctl-tty ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.ioctl_tty);
    (void)linux_exec64_write_text(console_capability, owner_id, " ioctl-enotty ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.ioctl_enotty);
    (void)linux_exec64_write_text(console_capability, owner_id, " ioctl-enosys ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.ioctl_enosys);
    (void)linux_exec64_write_text(console_capability, owner_id, " ioctl-last-request ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.ioctl_last_request);
    (void)linux_exec64_write_text(console_capability, owner_id, " ioctl-last-result ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.ioctl_last_result);
    (void)linux_exec64_write_text(console_capability, owner_id, " prctl ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.prctl_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " prctl-set-name ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.prctl_set_name);
    (void)linux_exec64_write_text(console_capability, owner_id, " prctl-get-name ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.prctl_get_name);
    (void)linux_exec64_write_text(console_capability, owner_id, " prctl-enosys ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.prctl_enosys);
    (void)linux_exec64_write_text(console_capability, owner_id, " prctl-last-option ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.prctl_last_option);
    (void)linux_exec64_write_text(console_capability, owner_id, " prctl-last-result ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.prctl_last_result);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " execveat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execveat_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-fault ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_fault);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-binary-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_binary_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-closed-fds ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_closed_fds);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-fd-live-before ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_fd_live_before);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-fd-live-after ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_fd_live_after);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-vma-before ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_vma_before);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-vma-released ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_vma_released);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-vma-after ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_vma_after);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-argc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_argc);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-envc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_envc);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-transfer-ready ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_transfer_ready);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-transfer-rip ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_transfer_rip);
    (void)linux_exec64_write_text(console_capability, owner_id, " execve-last-transfer-rsp ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.execve_last_transfer_rsp);
    (void)linux_exec64_write_text(console_capability, owner_id, " fork ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fork_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " fork-success ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fork_success);
    (void)linux_exec64_write_text(console_capability, owner_id, " fork-enosys ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fork_enosys);
    (void)linux_exec64_write_text(console_capability, owner_id, " fork-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fork_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " fork-child-slot ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.fork_child_slot);
    (void)linux_exec64_write_text(console_capability, owner_id, " fork-child-root-distinct ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.fork_child_root_distinct);
    (void)linux_exec64_write_text(console_capability, owner_id, " fork-last-rip ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.fork_last_rip);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-thread ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.clone_thread);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-thread-success ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.clone_thread_success);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.clone_denial);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-last-flags ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.clone_last_flags);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-unsupported-flags ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.clone_unsupported_flags);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-shared-cr3 ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.clone_shared_cr3);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-shared-vma ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.clone_shared_vma);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-shared-fd ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.clone_shared_fd);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-last-task ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.clone_last_task);
    (void)linux_exec64_write_text(console_capability, owner_id, " clone-last-tls ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.clone_last_tls_base);
    (void)linux_exec64_write_text(console_capability, owner_id, " wait4 ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.wait4_calls);
    (void)linux_exec64_write_text(console_capability, owner_id, " wait4-reap ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.wait4_reap);
    (void)linux_exec64_write_text(console_capability, owner_id, " wait4-last-exit-code ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.wait4_last_exit_code);
    (void)linux_exec64_write_text(console_capability, owner_id, " child-root-cleanup ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.child_root_cleanup);
    (void)linux_exec64_write_text(console_capability, owner_id, " pml4-pool-used-final ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.pml4_pool_used_final);
    (void)linux_exec64_write_text(console_capability, owner_id, " root-pool-used-final ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.pml4_pool_used_final);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-nvme-bind ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_vfs_bind);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-nvme-release ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_vfs_release);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-nvme-reads ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_vfs_reads);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-nvme-readdirs ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_vfs_readdirs);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-nvme-dirents ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_vfs_dirents);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-nvme-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_vfs_last_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-bin-alias ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.bin_vfs_aliases);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-bin-open ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.bin_vfs_opens);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-bin-read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.bin_vfs_reads);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-bin-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.bin_vfs_denials);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-localbin-alias ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.localbin_vfs_aliases);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-localbin-open ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.localbin_vfs_opens);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-localbin-read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.localbin_vfs_reads);
    (void)linux_exec64_write_text(console_capability, owner_id, " vfs-localbin-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.localbin_vfs_denials);
    (void)linux_exec64_write_text(console_capability, owner_id, "\n");

    (void)linux_exec64_write_text(console_capability, owner_id, "drs-realbin-syscall-last number ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_last);
    (void)linux_exec64_write_text(console_capability, owner_id, " result ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_last_result);
    (void)linux_exec64_write_text(console_capability, owner_id, " unimplemented ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_unimplemented_delta);
    (void)linux_exec64_write_text(console_capability, owner_id, " unimplemented-last ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_unimplemented_last);
    (void)linux_exec64_write_text(console_capability, owner_id, " unimplemented-rip ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.syscall_unimplemented_last_rip);
    (void)linux_exec64_write_text(console_capability, owner_id, " page-faults ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.page_fault_delta);
    (void)linux_exec64_write_text(console_capability, owner_id, " page-fault-rip ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.page_fault_rip);
    (void)linux_exec64_write_text(console_capability, owner_id, "\n");
}

static void linux_exec64_emit_failure(
    u32 console_capability,
    u32 owner_id,
    const u8 *path,
    u32 path_bytes)
{
    (void)linux_exec64_write_text(console_capability, owner_id, "drs-realbin-fail path ");
    linux_exec64_write_path_canonical(console_capability, owner_id, path, path_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " source ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.source);
    (void)linux_exec64_write_text(console_capability, owner_id, " stage ");
    (void)linux_exec64_write_text(
        console_capability,
        owner_id,
        linux_exec64_stage_name(g_linux_exec64_telemetry.stage));
    (void)linux_exec64_write_text(console_capability, owner_id, " code ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.failure_code);
    (void)linux_exec64_write_text(console_capability, owner_id, " pid ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pid);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-type ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-load ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_load_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-interp ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_interp_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_path_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-checksum ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_path_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-supported ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_supported);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-attempt ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_attempt);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_read);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-elf ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_elf);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-type ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-load ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_load_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-interp ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_interp_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-dynamic ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_dynamic_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " interp-file-nvme-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.interp_file_nvme_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-map-attempt ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_map_attempt);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-process ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_process);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-app-mapped ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_app_mapped);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-app-pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_app_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-interp-mapped ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_interp_mapped);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-interp-pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_interp_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-map-cleanup ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_map_cleanup);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-map-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_map_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-rela ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_rela_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_jmprel_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-relaent ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_relaent);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-pltrel ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_pltrel);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-first ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_first_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-type ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_first_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-first ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_first_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-type ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_first_type);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-symbol-trace ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_symbol_trace);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-symtab ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_symtab);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-strtab ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_strtab);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-syment ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_syment);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed-name ");
    linux_exec64_write_token(
        console_capability,
        owner_id,
        g_linux_exec64_dynamic_needed_name,
        g_linux_exec64_telemetry.dynamic_needed_name_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed-name-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_needed_name_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed-name-checksum ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_needed_name_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol-index ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_index);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol ");
    linux_exec64_write_token(
        console_capability,
        owner_id,
        g_linux_exec64_dynamic_reloc_symbol,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol-bytes ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-symbol-checksum ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_symbol_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol-index ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_index);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol ");
    linux_exec64_write_token(
        console_capability,
        owner_id,
        g_linux_exec64_dynamic_jmprel_symbol,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol-bytes ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-jmprel-symbol-checksum ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_jmprel_symbol_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-symbol-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_symbol_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-walk ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_walk);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-total ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_total);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-supported ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_supported);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-missing ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_missing);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-weak-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_weak_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-unavailable ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_unavailable);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-libc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_libc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-interp ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_interp);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-glob-dat ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_glob_dat);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-jump-slot ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_jump_slot);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-other ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_other);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-binding-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_binding_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-run ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_run);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-total ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_total);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-target-valid ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_target_valid);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-value ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-provider ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_provider);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-weak-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_weak_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-unavailable ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_unavailable);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-apply-ready ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_apply_ready);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-blocked ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_blocked);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_dry_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-first-target ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_first_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-first-value ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_first_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-jmprel-target ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_target);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-dry-jmprel-value ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_dry_jmprel_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-total ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_total);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-write ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_write);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-readback ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-blocked ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_blocked);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-unavailable ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_unavailable);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_reloc_apply_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-first-readback ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_apply_first_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-reloc-apply-jmprel-readback ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_reloc_apply_jmprel_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_libc_start_main);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main-apply ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_libc_start_main_apply);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main-value ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_libc_start_main_value);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc-start-main-readback ");
    linux_exec64_write_hex_u64(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_libc_start_main_readback);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-argc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_argc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-envc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_envc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-auxv ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_auxv);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-align ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_align);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-argv-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_argv_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-envp-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_envp_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-auxv-null ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_auxv_null);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-random ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_random_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-platform ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_platform_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-rsp ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_initial_rsp);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-stack-auxv-address ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_stack_auxv_address);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-auxv-phdr ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_auxv_phdr);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-auxv-base ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_auxv_base);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-auxv-entry ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_auxv_entry);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-ready ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_transfer_ready);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-rip ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_transfer_rip);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-rsp ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_transfer_rsp);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-task-registered ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_task_registered);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-transfer-started ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_transfer_started);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-first-syscall ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_first_syscall);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-console-bytes ");
    linux_exec64_write_dec_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_console_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-exit-code ");
    linux_exec64_write_hex_u32(
        console_capability,
        owner_id,
        g_linux_exec64_telemetry.dynamic_exit_code);
    (void)linux_exec64_write_text(console_capability, owner_id, " root-cleanup ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.root_cleanup);
    (void)linux_exec64_write_text(console_capability, owner_id, " pml4-pool-used-final ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pml4_pool_used_final);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-dynamic ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf_dynamic_count);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-needed ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_needed);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-supported ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_supported);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-missing ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_missing);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-libc ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_libc);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-pthread ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_pthread);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-first ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_first_needed_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " dynamic-last ");
    linux_exec64_write_hex_u32(console_capability, owner_id, g_linux_exec64_telemetry.dynamic_last_needed_checksum);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_launch.error);
    (void)linux_exec64_write_text(console_capability, owner_id, " load-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_launch.load_result.error);
    (void)linux_exec64_write_text(console_capability, owner_id, " stack-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_launch.stack_result.error);
    (void)linux_exec64_write_text(console_capability, owner_id, " load-first ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.load_first_vaddr);
    (void)linux_exec64_write_text(console_capability, owner_id, " load-end ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.load_max_end);
    (void)linux_exec64_write_text(console_capability, owner_id, " low-kernel-limit ");
    linux_exec64_write_hex_u64(console_capability, owner_id, g_linux_exec64_telemetry.low_kernel_vma_limit);
    (void)linux_exec64_write_text(console_capability, owner_id, " nvme-read-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_read_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " nvme-read-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_read_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " nvme-read-capacity ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_read_capacity);
    (void)linux_exec64_write_text(console_capability, owner_id, " nvme-read-size ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_read_size);
    (void)linux_exec64_write_text(console_capability, owner_id, " nvme-read-attr ");
    linux_exec64_write_hex_u64(console_capability, owner_id, (u64)g_linux_exec64_telemetry.nvme_read_attr);
    (void)linux_exec64_write_text(console_capability, owner_id, " boot-media-read-error ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.boot_media_read_error);
    (void)linux_exec64_write_text(console_capability, owner_id, " boot-media-read-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.boot_media_read_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " boot-media-read-capacity ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.boot_media_read_capacity);
    (void)linux_exec64_write_text(console_capability, owner_id, "\n");
}

static u32 linux_exec64_parent_pid_for_owner(u32 owner_id)
{
    u32 pid = process64_pid_for_principal(owner_id);

    if ((pid == PROCESS64_INVALID_PID) && (owner_id == PRINCIPAL64_ID_CONSOLE_CLIENT))
    {
        pid = process64_pid_for_principal(PRINCIPAL64_ID_CONSOLE_WORKER);
    }
    if (pid == PROCESS64_INVALID_PID)
    {
        pid = process64_pid_for_principal(PRINCIPAL64_ID_INIT_SUPERVISOR);
    }
    return pid;
}

static void linux_exec64_release_failed_process(u32 pid)
{
    u32 root_token;

    if (pid == PROCESS64_INVALID_PID)
    {
        return;
    }

    (void)vma64_release_process(pid);
    root_token = process64_page_root_token(pid);
    if (root_token != 0u)
    {
        (void)paging64_process_root_release(pid, root_token);
        (void)process64_clear_page_root(pid, root_token);
    }
    (void)linux_vfs64_release_nvme_read(pid);
    (void)fd64_release_process(pid);
    (void)persona64_release(pid);
    (void)persona_audit64_release(pid);
    (void)process64_release_clone(pid);
}

static void linux_exec64_record_process_root_telemetry(u32 pid)
{
    g_linux_exec64_telemetry.pml4 = 1u;
    g_linux_exec64_telemetry.pml4_pool = paging64_process_root_pool_limit();
    g_linux_exec64_telemetry.pml4_slot = paging64_process_root_slot(pid);
    g_linux_exec64_telemetry.root_physical = paging64_process_root_physical(pid);
    g_linux_exec64_telemetry.kernel_root_physical = paging64_kernel_root_physical();
    g_linux_exec64_telemetry.root_distinct =
        (g_linux_exec64_telemetry.root_physical != g_linux_exec64_telemetry.kernel_root_physical)
            ? 1u
            : 0u;
    g_linux_exec64_telemetry.high_copy = paging64_process_root_last_high_copy();
    g_linux_exec64_telemetry.mmio_shared = paging64_process_root_last_mmio_shared();
    g_linux_exec64_telemetry.pool_mapped = paging64_process_root_last_pool_mapped();
    g_linux_exec64_telemetry.low_compat = paging64_process_root_last_low_compat();
    g_linux_exec64_telemetry.low_pdpt_present =
        paging64_process_root_last_low_pdpt_present();
    g_linux_exec64_telemetry.syscall_entry_high = syscall64_native_lstar_high();
    g_linux_exec64_telemetry.idt_high =
        ((interrupts64_idt_high_targets() != 0u)
            && (interrupts64_idt_high_base() != 0u))
            ? 1u
            : 0u;
    g_linux_exec64_telemetry.descriptor_high =
        ((descriptors64_gdt_high_base() != 0u)
            && (descriptors64_tss_high_base() != 0u)
            && (descriptors64_tss_rsp0_high() != 0u))
            ? 1u
            : 0u;
    g_linux_exec64_telemetry.kernel_entry_high_ready =
        ((g_linux_exec64_telemetry.syscall_entry_high != 0u)
            && (g_linux_exec64_telemetry.idt_high != 0u)
            && (g_linux_exec64_telemetry.descriptor_high != 0u))
            ? 1u
            : 0u;
    g_linux_exec64_telemetry.user_pdpt_private =
        paging64_process_root_last_user_pdpt_private();
}

static u64 linux_exec64_program_header_vaddr(
    const elf64_header_t *header,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 load_bias)
{
    u32 index;

    if ((header == 0) || (phdrs == 0) || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        return 0ull;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        if (phdr->type != ELF64_PT_LOAD)
        {
            continue;
        }
        if ((header->phoff >= phdr->offset)
            && (header->phoff < (phdr->offset + phdr->filesz)))
        {
            return load_bias + phdr->vaddr + (header->phoff - phdr->offset);
        }
    }

    return 0ull;
}

static u64 linux_exec64_first_load_vaddr(
    const elf64_program_header_t *phdrs,
    u32 phdr_count)
{
    u64 first = 0ull;
    u32 index;

    if ((phdrs == 0) || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        return 0ull;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        if ((phdr->type != ELF64_PT_LOAD) || (phdr->memsz == 0ull))
        {
            continue;
        }
        if ((first == 0ull) || (phdr->vaddr < first))
        {
            first = phdr->vaddr;
        }
    }

    return first;
}

static u32 linux_exec64_build_dynamic_stack_preview(
    u32 pid,
    const elf64_header_t *app_header,
    const elf64_program_header_t *app_phdrs,
    const elf64_header_t *interp_header,
    const elf64_program_header_t *interp_phdrs,
    u32 interp_phdr_count,
    u32 argc)
{
    elf64_auxv_t auxv;
    elf64_stack_result_t stack_result;
    u64 app_entry;
    u64 app_phdr_vaddr;
    u64 interp_base;
    u64 transfer_rip;
    u32 stack_ok;

    if ((pid == PROCESS64_INVALID_PID)
        || (app_header == 0)
        || (app_phdrs == 0)
        || (interp_header == 0)
        || (interp_phdrs == 0)
        || (argc == 0u))
    {
        g_linux_exec64_telemetry.dynamic_stack_error = ELF64_ERROR_LAUNCH_ARGUMENT;
        return 0u;
    }

    app_entry = app_header->entry;
    app_phdr_vaddr = linux_exec64_program_header_vaddr(
        app_header,
        app_phdrs,
        app_header->phnum,
        0ull);
    interp_base = linux_exec64_first_load_vaddr(interp_phdrs, interp_phdr_count);
    transfer_rip = app_entry;

    g_linux_exec64_telemetry.dynamic_auxv_phdr = app_phdr_vaddr;
    g_linux_exec64_telemetry.dynamic_auxv_base = interp_base;
    g_linux_exec64_telemetry.dynamic_auxv_entry = app_entry;
    g_linux_exec64_telemetry.dynamic_transfer_rip = transfer_rip;

    if ((app_entry == 0ull)
        || (app_phdr_vaddr == 0ull)
        || (interp_base == 0ull)
        || (transfer_rip == 0ull))
    {
        g_linux_exec64_telemetry.dynamic_stack_error = ELF64_ERROR_AUX_ARGUMENT;
        return 0u;
    }

    if (vma64_map_anon(
            pid,
            LINUX_EXEC64_REAL_STACK_BASE,
            LINUX_EXEC64_REAL_STACK_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS) != LINUX_EXEC64_REAL_STACK_BASE)
    {
        g_linux_exec64_telemetry.dynamic_stack_error = ELF64_ERROR_LAUNCH_STACK_MAP;
        return 0u;
    }

    if (elf64_build_auxv(
            pid,
            app_entry,
            app_phdr_vaddr,
            (u32)app_header->phnum,
            interp_base,
            &auxv) != ELF64_OK)
    {
        g_linux_exec64_telemetry.dynamic_stack_error = auxv.error;
        return 0u;
    }

    stack_ok = elf64_build_initial_stack(
        pid,
        LINUX_EXEC64_REAL_STACK_BASE,
        LINUX_EXEC64_REAL_STACK_BASE + (u64)LINUX_EXEC64_REAL_STACK_BYTES,
        argc,
        g_linux_exec64_staged_argv_ptrs,
        LINUX_EXEC64_DEFAULT_ENV_COUNT,
        g_linux_exec64_staged_envp_ptrs,
        &auxv,
        &stack_result);
    g_linux_exec64_telemetry.dynamic_stack_error = stack_result.error;
    if (stack_ok != ELF64_OK)
    {
        return 0u;
    }

    g_linux_exec64_telemetry.dynamic_stack = 1u;
    g_linux_exec64_telemetry.dynamic_stack_pages =
        LINUX_EXEC64_REAL_STACK_BYTES / VMA64_PAGE_BYTES;
    g_linux_exec64_telemetry.dynamic_stack_argc = stack_result.argc;
    g_linux_exec64_telemetry.dynamic_stack_envc = stack_result.envc;
    g_linux_exec64_telemetry.dynamic_stack_auxv = stack_result.auxv_entry_count;
    g_linux_exec64_telemetry.dynamic_stack_align = stack_result.alignment_ok;
    g_linux_exec64_telemetry.dynamic_stack_argv_null = stack_result.argv_null_ok;
    g_linux_exec64_telemetry.dynamic_stack_envp_null = stack_result.envp_null_ok;
    g_linux_exec64_telemetry.dynamic_stack_auxv_null = stack_result.auxv_null_ok;
    g_linux_exec64_telemetry.dynamic_stack_random_checksum = stack_result.random_checksum;
    g_linux_exec64_telemetry.dynamic_stack_platform_checksum = stack_result.platform_checksum;
    g_linux_exec64_telemetry.dynamic_stack_initial_rsp = stack_result.initial_rsp;
    g_linux_exec64_telemetry.dynamic_stack_auxv_address = stack_result.auxv_address;
    g_linux_exec64_telemetry.dynamic_transfer_rsp = stack_result.initial_rsp;

    if (linux_libc64_bind_environment(
            pid,
            stack_result.envp_address,
            stack_result.envc,
            g_linux_exec64_staged_envp_ptrs) != LINUX_LIBC64_OK)
    {
        g_linux_exec64_telemetry.dynamic_stack_error = LINUX_LIBC64_ERROR_ENVIRONMENT;
        return 0u;
    }

    g_linux_exec64_telemetry.dynamic_transfer_ready =
        ((paging64_user_page_present_for_process(pid, transfer_rip & ~((u64)VMA64_PAGE_BYTES - 1ull)) != 0u)
            && ((paging64_user_page_protection_for_process(pid, transfer_rip & ~((u64)VMA64_PAGE_BYTES - 1ull))
                    & PAGING64_USER_PROT_EXECUTE) != 0u)
            && (paging64_user_page_present_for_process(pid, LINUX_EXEC64_REAL_STACK_BASE) != 0u)
            && ((paging64_user_page_protection_for_process(pid, LINUX_EXEC64_REAL_STACK_BASE)
                    & PAGING64_USER_PROT_WRITE) != 0u))
            ? 1u
            : 0u;

    if (g_linux_exec64_telemetry.dynamic_transfer_ready == 0u)
    {
        g_linux_exec64_telemetry.dynamic_stack_error = ELF64_ERROR_LAUNCH_TRANSFER;
        return 0u;
    }

    return 1u;
}

static u32 linux_exec64_try_dynamic_mapping(
    u32 owner_id,
    u32 nvme_fs_capability,
    u32 source_authority,
    u32 source,
    const elf64_header_t *header,
    const elf64_program_header_t *phdrs,
    const elf64_phdr_summary_t *summary,
    u32 app_bytes,
    u32 argc,
    u32 keep_live,
    u32 *out_pid)
{
    elf64_load_result_t app_load;
    elf64_header_t interp_header;
    elf64_program_header_t interp_phdrs[ELF64_MAX_PROGRAM_HEADERS];
    elf64_phdr_summary_t interp_summary;
    elf64_load_result_t interp_load;
    linux_libc64_load_result_t libc_load;
    u32 index;
    u32 parent_pid;
    u32 pid;
    u32 process_owner;
    u32 root_authority;
    u32 process_root_token;
    u32 app_switch;
    u32 app_restore;
    u32 interp_switch;
    u32 interp_restore;
    u32 vma_release;
    u32 root_release = 0u;
    u32 root_clear = 0u;
    u32 clone_release;

    if (out_pid != 0)
    {
        *out_pid = PROCESS64_INVALID_PID;
    }

    if ((header == 0)
        || (phdrs == 0)
        || (summary == 0)
        || (header->type != ELF64_TYPE_EXEC)
        || (summary->load_count == 0u)
        || (summary->interp_count != 1u)
        || (summary->dynamic_count != 1u)
        || (g_linux_exec64_telemetry.interp_supported == 0u))
    {
        return LINUX_EXEC64_DYNAMIC_NONE;
    }

    g_linux_exec64_telemetry.dynamic_map_attempt = 1u;
    parent_pid = linux_exec64_parent_pid_for_owner(owner_id);
    pid = (parent_pid != PROCESS64_INVALID_PID) ? process64_spawn_clone(parent_pid) : PROCESS64_INVALID_PID;
    g_linux_exec64_telemetry.pid = pid;
    if (pid == PROCESS64_INVALID_PID)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 3u;
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }

    process_owner = process64_principal(pid);
    root_authority = process64_runtime_token(pid);
    if (root_authority == 0u)
    {
        root_authority =
            (source == LINUX_EXEC64_SOURCE_BOOT_MEDIA) ? source_authority : nvme_fs_capability;
    }
    if (paging64_process_root_alloc(pid, process_owner, root_authority) == 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 11u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    process_root_token = paging64_process_root_token(pid);
    if ((process_root_token == 0u)
        || (process64_attach_page_root(
                pid,
                paging64_process_root_physical(pid),
                paging64_process_root_slot(pid),
                process_root_token,
                root_authority) == 0u))
    {
        g_linux_exec64_telemetry.dynamic_map_error = 12u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    g_linux_exec64_telemetry.dynamic_process = 1u;
    linux_exec64_record_process_root_telemetry(pid);

    if (vma64_init_process(pid) == 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 4u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    if (persona64_init_linux_elf(pid, linux_abi64_dispatch_table()) != PERSONA64_ATTACH_OK)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 22u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }

    app_switch = paging64_switch_to_process_root(pid, 0x44594E41u);
    if (app_switch == 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 13u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    if (elf64_map_load_segments(
            pid,
            phdrs,
            header->phnum,
            g_linux_exec64_binary,
            app_bytes,
            0ull,
            &app_load) == ELF64_OK)
    {
        g_linux_exec64_telemetry.dynamic_app_mapped = app_load.mapped_count;
        g_linux_exec64_telemetry.dynamic_app_pages =
            (u32)(app_load.total_map_bytes / VMA64_PAGE_BYTES);
    }
    else
    {
        g_linux_exec64_telemetry.dynamic_map_error = app_load.error;
    }
    app_restore = paging64_switch_to_kernel_root(0x44594E4Bu);
    if ((app_restore == 0u) || (g_linux_exec64_telemetry.dynamic_app_mapped == 0u))
    {
        if (g_linux_exec64_telemetry.dynamic_map_error == 0u)
        {
            g_linux_exec64_telemetry.dynamic_map_error = 14u;
        }
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    linux_exec64_record_dynamic_relocations(
        pid,
        g_linux_exec64_binary,
        app_bytes,
        phdrs,
        header->phnum,
        1u);

    for (index = 0u; index < header->phnum; ++index)
    {
        if ((phdrs[index].type == ELF64_PT_INTERP)
            && (phdrs[index].filesz != 0ull)
            && (phdrs[index].filesz <= LINUX_DYNAMIC64_INTERP_PATH_MAX)
            && (linux_exec64_range_available(app_bytes, phdrs[index].offset, phdrs[index].filesz) != 0u))
        {
            (void)linux_exec64_probe_interpreter_file(
                owner_id,
                g_linux_exec64_binary + phdrs[index].offset,
                g_linux_exec64_telemetry.interp_path_bytes,
                source);
            break;
        }
    }
    if ((g_linux_exec64_telemetry.interp_file_read == 0u)
        || (g_linux_exec64_telemetry.interp_file_elf == 0u))
    {
        g_linux_exec64_telemetry.dynamic_map_error = 15u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }

    if (elf64_parse_header(g_linux_exec64_binary, g_linux_exec64_telemetry.interp_file_bytes, &interp_header) != ELF64_OK)
    {
        g_linux_exec64_telemetry.dynamic_map_error = interp_header.error;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    for (index = 0u; index < ELF64_MAX_PROGRAM_HEADERS; ++index)
    {
        interp_phdrs[index].type = 0u;
        interp_phdrs[index].flags = 0u;
        interp_phdrs[index].offset = 0ull;
        interp_phdrs[index].vaddr = 0ull;
        interp_phdrs[index].paddr = 0ull;
        interp_phdrs[index].filesz = 0ull;
        interp_phdrs[index].memsz = 0ull;
        interp_phdrs[index].align = 0ull;
    }
    if (elf64_parse_phdrs(
            g_linux_exec64_binary,
            g_linux_exec64_telemetry.interp_file_bytes,
            &interp_header,
            interp_phdrs,
            ELF64_MAX_PROGRAM_HEADERS,
            &interp_summary) != ELF64_OK)
    {
        g_linux_exec64_telemetry.dynamic_map_error = interp_summary.error;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }

    interp_switch = paging64_switch_to_process_root(pid, 0x44594E49u);
    if (interp_switch == 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 16u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    if (elf64_map_load_segments(
            pid,
            interp_phdrs,
            interp_header.phnum,
            g_linux_exec64_binary,
            g_linux_exec64_telemetry.interp_file_bytes,
            0ull,
            &interp_load) == ELF64_OK)
    {
        g_linux_exec64_telemetry.dynamic_interp_mapped = interp_load.mapped_count;
        g_linux_exec64_telemetry.dynamic_interp_pages =
            (u32)(interp_load.total_map_bytes / VMA64_PAGE_BYTES);
    }
    else
    {
        g_linux_exec64_telemetry.dynamic_map_error = interp_load.error;
    }
    interp_restore = paging64_switch_to_kernel_root(0x44594E4Cu);
    if ((interp_restore == 0u) || (g_linux_exec64_telemetry.dynamic_interp_mapped == 0u))
    {
        if (g_linux_exec64_telemetry.dynamic_map_error == 0u)
        {
            g_linux_exec64_telemetry.dynamic_map_error = 17u;
        }
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }

    interp_switch = paging64_switch_to_process_root(pid, 0x44594E43u);
    if (interp_switch == 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 18u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    if (linux_libc64_load(pid, LINUX_LIBC64_DEFAULT_BASE, &libc_load) != LINUX_LIBC64_OK)
    {
        g_linux_exec64_telemetry.dynamic_map_error =
            (libc_load.error != 0u) ? libc_load.error : 18u;
    }
    interp_restore = paging64_switch_to_kernel_root(0x44594E55u);
    if ((interp_restore == 0u) || (g_linux_exec64_telemetry.dynamic_map_error != 0u))
    {
        if (g_linux_exec64_telemetry.dynamic_map_error == 0u)
        {
            g_linux_exec64_telemetry.dynamic_map_error = 18u;
        }
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }

    interp_switch = paging64_switch_to_process_root(pid, 0x44594E53u);
    if (interp_switch == 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 19u;
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }
    if (linux_exec64_build_dynamic_stack_preview(
            pid,
            header,
            phdrs,
            &interp_header,
            interp_phdrs,
            interp_header.phnum,
            argc) == 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_error = 20u;
    }
    interp_restore = paging64_switch_to_kernel_root(0x44594E54u);
    if ((interp_restore == 0u) || (g_linux_exec64_telemetry.dynamic_stack == 0u))
    {
        if (g_linux_exec64_telemetry.dynamic_map_error == 0u)
        {
            g_linux_exec64_telemetry.dynamic_map_error = 21u;
        }
        linux_exec64_release_failed_process(pid);
        return LINUX_EXEC64_DYNAMIC_FAILED;
    }

    if (keep_live != 0u)
    {
        if (out_pid != 0)
        {
            *out_pid = pid;
        }
        g_linux_exec64_telemetry.mapped_regions =
            g_linux_exec64_telemetry.dynamic_app_mapped
            + g_linux_exec64_telemetry.dynamic_interp_mapped;
        g_linux_exec64_telemetry.mapped_pages =
            g_linux_exec64_telemetry.dynamic_app_pages
            + g_linux_exec64_telemetry.dynamic_interp_pages;
        g_linux_exec64_telemetry.stack_pages = g_linux_exec64_telemetry.dynamic_stack_pages;
        g_linux_exec64_telemetry.envc = g_linux_exec64_telemetry.dynamic_stack_envc;
        g_linux_exec64_telemetry.vma_pt_private = paging64_process_root_last_vma_pt_private();
        g_linux_exec64_launch.transfer_rip = g_linux_exec64_telemetry.dynamic_transfer_rip;
        g_linux_exec64_launch.transfer_rsp = g_linux_exec64_telemetry.dynamic_transfer_rsp;
        g_linux_exec64_launch.initial_rsp = g_linux_exec64_telemetry.dynamic_transfer_rsp;
        g_linux_exec64_launch.transfer_selectors =
            ((u32)DESCRIPTORS64_USER_DATA_SELECTOR << 16)
            | (u32)DESCRIPTORS64_USER_CODE_SELECTOR;
        g_linux_exec64_launch.transfer_ready = g_linux_exec64_telemetry.dynamic_transfer_ready;
        g_linux_exec64_telemetry.dynamic_map_error = 0u;
        return LINUX_EXEC64_DYNAMIC_READY;
    }

    vma_release = vma64_release_process(pid);
    process_root_token = process64_page_root_token(pid);
    if (process_root_token != 0u)
    {
        root_release = paging64_process_root_release(pid, process_root_token);
        root_clear = process64_clear_page_root(pid, process_root_token);
    }
    clone_release = process64_release_clone(pid);
    g_linux_exec64_telemetry.root_cleanup =
        ((root_release != 0u)
            && (root_clear != 0u)
            && (paging64_process_root_physical(pid) == 0ull)
            && (process64_page_root_token(pid) == 0u))
            ? 1u
            : 0u;
    g_linux_exec64_telemetry.pml4_pool_used_final = paging64_process_root_pool_used();
    g_linux_exec64_telemetry.dynamic_map_cleanup =
        ((vma_release
                >= (g_linux_exec64_telemetry.dynamic_app_mapped
                    + g_linux_exec64_telemetry.dynamic_interp_mapped))
            && (g_linux_exec64_telemetry.root_cleanup != 0u)
            && (clone_release != 0u)
            && (g_linux_exec64_telemetry.pml4_pool_used_final == 0u))
            ? 1u
            : 0u;
    g_linux_exec64_telemetry.dynamic_map_error =
        (g_linux_exec64_telemetry.dynamic_map_cleanup != 0u) ? 0u : 18u;
    return (g_linux_exec64_telemetry.dynamic_map_cleanup != 0u)
        ? LINUX_EXEC64_DYNAMIC_READY
        : LINUX_EXEC64_DYNAMIC_FAILED;
}

static u32 linux_exec64_static_loads_overlap_low_kernel_window(
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 *first_vaddr,
    u64 *max_end)
{
    u32 index;
    u64 first = 0ull;
    u64 end = 0ull;

    if ((phdrs == 0) || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        return 1u;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 load_end;

        if ((phdr->type != ELF64_PT_LOAD) || (phdr->memsz == 0ull))
        {
            continue;
        }
        load_end = phdr->vaddr + phdr->memsz;
        if (load_end < phdr->vaddr)
        {
            return 1u;
        }
        if ((first == 0ull) || (phdr->vaddr < first))
        {
            first = phdr->vaddr;
        }
        if (load_end > end)
        {
            end = load_end;
        }
    }

    if (first_vaddr != 0)
    {
        *first_vaddr = first;
    }
    if (max_end != 0)
    {
        *max_end = end;
    }
    return ((first < LINUX_EXEC64_LOW_KERNEL_VMA_LIMIT)
        && (end > 0ull))
        ? 1u
        : 0u;
}

static u32 linux_exec64_run_source(
    const u8 *path,
    u32 path_bytes,
    const char *const *argv,
    u32 argc,
    u32 owner_id,
    u32 nvme_fs_capability,
    u32 console_capability,
    u32 source)
{
    elf64_header_t header;
    elf64_program_header_t phdrs[ELF64_MAX_PROGRAM_HEADERS];
    elf64_phdr_summary_t phdr_summary;
    void *vma_ctx = 0;
    void *fd_ctx = 0;
    void *audit_ctx = 0;
    u32 parent_pid;
    u32 pid = PROCESS64_INVALID_PID;
    u32 process_owner = 0u;
    u32 stdin_capability;
    u32 stdout_capability;
    u32 stderr_capability;
    u32 bytes_read = 0u;
    u32 index;
    u32 launch_result = ELF64_OK;
    u32 task;
    u32 runqueue_started;
    u32 transfer_result;
    u32 dynamic_result;
    u32 dynamic_launch = 0u;
    u32 console_bytes_before;
    u32 console_bytes_after;
    u32 unimplemented_before;
    u32 unimplemented_after;
    u32 page_faults_before;
    u32 page_faults_after;
    u32 getdents64_calls_before;
    u32 getdents64_calls_after;
    u32 getdents64_entries_before;
    u32 getdents64_entries_after;
    u32 getdents64_bytes_before;
    u32 getdents64_bytes_after;
    u32 stat_before;
    u32 stat_after;
    u32 stat_denial_before;
    u32 stat_denial_after;
    u32 stat_fault_before;
    u32 stat_fault_after;
    u32 fstat_before;
    u32 fstat_after;
    u32 fstat_denial_before;
    u32 fstat_denial_after;
    u32 fstat_fault_before;
    u32 fstat_fault_after;
    u32 newfstatat_before;
    u32 newfstatat_after;
    u32 newfstatat_denial_before;
    u32 newfstatat_denial_after;
    u32 newfstatat_fault_before;
    u32 newfstatat_fault_after;
    u32 lseek_before;
    u32 lseek_after;
    u32 lseek_denial_before;
    u32 lseek_denial_after;
    u32 dup_before;
    u32 dup_after;
    u32 dup2_before;
    u32 dup2_after;
    u32 dup3_before;
    u32 dup3_after;
    u32 dup_denial_before;
    u32 dup_denial_after;
    u32 fcntl_before;
    u32 fcntl_after;
    u32 fcntl_denial_before;
    u32 fcntl_denial_after;
    u32 readlink_before;
    u32 readlink_after;
    u32 readlink_bytes_before;
    u32 readlink_bytes_after;
    u32 readlink_denial_before;
    u32 readlink_denial_after;
    u32 readlink_fault_before;
    u32 readlink_fault_after;
    u32 getcwd_before;
    u32 getcwd_after;
    u32 getcwd_bytes_before;
    u32 getcwd_bytes_after;
    u32 getcwd_denial_before;
    u32 getcwd_denial_after;
    u32 getcwd_fault_before;
    u32 getcwd_fault_after;
    u32 path_relative_before;
    u32 path_relative_after;
    u32 path_dot_before;
    u32 path_dot_after;
    u32 path_dotdot_before;
    u32 path_dotdot_after;
    u32 path_trailing_before;
    u32 path_trailing_after;
    u32 path_trailing_denial_before;
    u32 path_trailing_denial_after;
    u32 path_fault_before;
    u32 path_fault_after;
    u32 chdir_before;
    u32 chdir_after;
    u32 fchdir_before;
    u32 fchdir_after;
    u32 chdir_denial_before;
    u32 chdir_denial_after;
    u32 chdir_fault_before;
    u32 chdir_fault_after;
    u32 openat_before;
    u32 openat_after;
    u32 read_calls_before;
    u32 read_calls_after;
    u32 read_bytes_before;
    u32 read_bytes_after;
    u32 write_calls_before;
    u32 write_calls_after;
    u32 write_bytes_before;
    u32 write_bytes_after;
    u32 signal_sigpipe_before;
    u32 signal_sigpipe_after;
    u32 signal_sigchld_before;
    u32 signal_sigchld_after;
    u32 signal_rt_sigreturn_before;
    u32 signal_rt_sigreturn_after;
    u32 signal_frame_fault_before;
    u32 signal_frame_fault_after;
    u32 mmap_calls_before;
    u32 mmap_calls_after;
    u32 mmap_bytes_before;
    u32 mmap_bytes_after;
    u32 mmap_denial_before;
    u32 mmap_denial_after;
    u32 mmap_file_before;
    u32 mmap_file_after;
    u32 mmap_file_bytes_before;
    u32 mmap_file_bytes_after;
    u32 mmap_file_denial_before;
    u32 mmap_file_denial_after;
    u32 futex_wait_before;
    u32 futex_wait_after;
    u32 futex_wake_before;
    u32 futex_wake_after;
    u32 futex_woken_before;
    u32 futex_woken_after;
    u32 thread_exit_cleartid_before;
    u32 thread_exit_cleartid_after;
    u32 thread_exit_cleartid_fault_before;
    u32 thread_exit_cleartid_fault_after;
    u32 pipe_calls_before;
    u32 pipe_calls_after;
    u32 pipe_denial_before;
    u32 pipe_denial_after;
    u32 pipe_fault_before;
    u32 pipe_fault_after;
    u32 pipe2_calls_before;
    u32 pipe2_calls_after;
    u32 pipe2_denial_before;
    u32 pipe2_denial_after;
    u32 pipe2_fault_before;
    u32 pipe2_fault_after;
    u32 pipe_blocks_before;
    u32 pipe_blocks_after;
    u32 pipe_wakes_before;
    u32 pipe_wakes_after;
    u32 pipe_replays_before;
    u32 pipe_replays_after;
    u32 pipe_provider_denial_before;
    u32 pipe_provider_denial_after;
    u32 fd_fork_pipe_copy_before;
    u32 fd_fork_pipe_copy_after;
    u32 fd_fork_pipe_denial_before;
    u32 fd_fork_pipe_denial_after;
    u32 readv_calls_before;
    u32 readv_calls_after;
    u32 readv_bytes_before;
    u32 readv_bytes_after;
    u32 writev_calls_before;
    u32 writev_calls_after;
    u32 writev_bytes_before;
    u32 writev_bytes_after;
    u32 poll_calls_before;
    u32 poll_calls_after;
    u32 ppoll_calls_before;
    u32 ppoll_calls_after;
    u32 poll_ready_before;
    u32 poll_ready_after;
    u32 geteuid_before;
    u32 geteuid_after;
    u32 getppid_before;
    u32 getppid_after;
    u32 ioctl_before;
    u32 ioctl_after;
    u32 ioctl_tty_before;
    u32 ioctl_tty_after;
    u32 ioctl_enotty_before;
    u32 ioctl_enotty_after;
    u32 ioctl_enosys_before;
    u32 ioctl_enosys_after;
    u32 prctl_before;
    u32 prctl_after;
    u32 prctl_set_name_before;
    u32 prctl_set_name_after;
    u32 prctl_get_name_before;
    u32 prctl_get_name_after;
    u32 prctl_enosys_before;
    u32 prctl_enosys_after;
    u32 execve_before;
    u32 execve_after;
    u32 execveat_before;
    u32 execveat_after;
    u32 execve_denial_before;
    u32 execve_denial_after;
    u32 execve_fault_before;
    u32 execve_fault_after;
    u32 syscall_root_repair_before;
    u32 syscall_root_repair_after;
    u32 syscall_root_reload_before;
    u32 syscall_root_reload_after;
    u32 syscall_root_denial_before;
    u32 syscall_root_denial_after;
    u32 fs_save_before;
    u32 fs_save_after;
    u32 fs_restore_before;
    u32 fs_restore_after;
    u32 fs_set_before;
    u32 fs_set_after;
    u32 scheduler_denial_before;
    u32 scheduler_denial_after;
    u32 fork_before;
    u32 fork_after;
    u32 fork_success_before;
    u32 fork_success_after;
    u32 fork_enosys_before;
    u32 fork_enosys_after;
    u32 fork_denial_before;
    u32 fork_denial_after;
    u32 clone_thread_before;
    u32 clone_thread_after;
    u32 clone_denial_before;
    u32 clone_denial_after;
    u32 wait4_before;
    u32 wait4_after;
    u32 wait4_reap_before;
    u32 wait4_reap_after;
    u32 child_root_cleanup_before;
    u32 child_root_cleanup_after;
    u32 nvme_vfs_reads_before;
    u32 nvme_vfs_reads_after;
    u32 nvme_vfs_readdirs_before;
    u32 nvme_vfs_readdirs_after;
    u32 nvme_vfs_dirents_before;
    u32 nvme_vfs_dirents_after;
    u32 bin_vfs_alias_before;
    u32 bin_vfs_alias_after;
    u32 bin_vfs_open_before;
    u32 bin_vfs_open_after;
    u32 bin_vfs_read_before;
    u32 bin_vfs_read_after;
    u32 bin_vfs_denial_before;
    u32 bin_vfs_denial_after;
    u32 localbin_vfs_alias_before;
    u32 localbin_vfs_alias_after;
    u32 localbin_vfs_open_before;
    u32 localbin_vfs_open_after;
    u32 localbin_vfs_read_before;
    u32 localbin_vfs_read_after;
    u32 localbin_vfs_denial_before;
    u32 localbin_vfs_denial_after;
    u32 exit_probe_arm;
    u32 exit_probe_clear = 0u;
    u32 reattach_vma = 0u;
    u32 reattach_fd = 0u;
    u32 reattach_audit = 0u;
    u32 vma_release = 0u;
    u32 audit_release = 0u;
    u32 exit_vma_release = 0u;
    u32 exit_audit_release = 0u;
    u32 clone_release = 0u;
    u32 fd_cleanup = 0u;
    u32 nvme_vfs_release = 0u;
    u32 process_root_token = 0u;
    u32 root_authority = 0u;
    u32 root_release = 0u;
    u32 root_clear = 0u;
    u32 load_cr3_switch = 0u;
    u32 load_cr3_restore = 0u;
    u32 cr3_process_switch_before = 0u;
    u32 cr3_process_switch_after = 0u;
    u32 cr3_kernel_switch_before = 0u;
    u32 cr3_kernel_switch_after = 0u;
    u32 entry_cr3_restore;
    u64 entry_kernel_root;

    entry_kernel_root = paging64_kernel_root_physical();
    entry_cr3_restore = paging64_switch_to_kernel_root(0x4C58454Eu);
    linux_exec64_zero(&g_linux_exec64_telemetry, sizeof(g_linux_exec64_telemetry));
    linux_exec64_zero(&g_linux_exec64_launch, sizeof(g_linux_exec64_launch));
    linux_exec64_zero(g_linux_exec64_binary, sizeof(g_linux_exec64_binary));
    linux_exec64_zero(g_linux_exec64_dynamic_needed_name, sizeof(g_linux_exec64_dynamic_needed_name));
    linux_exec64_zero(g_linux_exec64_dynamic_reloc_symbol, sizeof(g_linux_exec64_dynamic_reloc_symbol));
    linux_exec64_zero(g_linux_exec64_dynamic_jmprel_symbol, sizeof(g_linux_exec64_dynamic_jmprel_symbol));
    g_linux_exec64_telemetry.task = SCHEDULER64_INVALID_TASK;
    g_linux_exec64_telemetry.pml4_slot = 0xFFFFFFFFu;
    g_linux_exec64_telemetry.fork_child_slot = 0xFFFFFFFFu;
    g_linux_exec64_telemetry.pid = PROCESS64_INVALID_PID;
    g_linux_exec64_telemetry.syscall_last = 0xFFFFFFFFu;
    g_linux_exec64_telemetry.syscall_last_result = 0u;
    g_linux_exec64_telemetry.low_kernel_vma_limit = LINUX_EXEC64_LOW_KERNEL_VMA_LIMIT;
    g_linux_exec64_telemetry.source = source;
    g_linux_exec64_telemetry.kernel_cr3_entry =
        ((entry_cr3_restore != 0u)
            && (paging64_current_root_physical() == (entry_kernel_root & 0xFFFFFFFFFFFFF000ull)))
            ? 1u
            : 0u;

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_ARGUMENT;
    if ((path == 0)
        || (path_bytes == 0u)
        || (argv == 0)
        || (argc == 0u)
        || (argc > LINUX_EXEC64_ARG_MAX)
        || (principal64_is_active(owner_id) == 0u))
    {
        g_linux_exec64_telemetry.failure_code = 22u;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    if (linux_exec64_stage_launch_strings(argv, argc) == 0u)
    {
        g_linux_exec64_telemetry.failure_code = 23u;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_CAPABILITY;
    if ((console_capability == CAPABILITY64_INVALID_HANDLE)
        || (capability64_route(console_capability, CAPABILITY64_RIGHT_SEND, owner_id)
            != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE)))
    {
        g_linux_exec64_telemetry.failure_code = 1u;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    if (source == LINUX_EXEC64_SOURCE_NVME)
    {
        if ((nvme_fs_capability == CAPABILITY64_INVALID_HANDLE)
            || (nvme_fs_capability != mmio64_nvme_rw_capability()))
        {
            g_linux_exec64_telemetry.failure_code = 1u;
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }
    }
    else if ((source != LINUX_EXEC64_SOURCE_BOOT_MEDIA) || (boot_media64_has_file(path, path_bytes) == 0u))
    {
        g_linux_exec64_telemetry.failure_code = 1u;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_READ;
    if (linux_exec64_read_source(
            source,
            path,
            path_bytes,
            g_linux_exec64_binary,
            LINUX_EXEC64_STAGING_BUFFER_BYTES,
            owner_id,
            &bytes_read) == 0u)
    {
        g_linux_exec64_telemetry.failure_code =
            (source == LINUX_EXEC64_SOURCE_BOOT_MEDIA)
                ? g_linux_exec64_telemetry.boot_media_read_error
                : g_linux_exec64_telemetry.nvme_read_error;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_ELF;
    if (elf64_parse_header(g_linux_exec64_binary, bytes_read, &header) != ELF64_OK)
    {
        g_linux_exec64_telemetry.failure_code = header.error;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    for (index = 0u; index < ELF64_MAX_PROGRAM_HEADERS; ++index)
    {
        phdrs[index].type = 0u;
        phdrs[index].flags = 0u;
        phdrs[index].offset = 0ull;
        phdrs[index].vaddr = 0ull;
        phdrs[index].paddr = 0ull;
        phdrs[index].filesz = 0ull;
        phdrs[index].memsz = 0ull;
        phdrs[index].align = 0ull;
    }
    if (elf64_parse_phdrs(
            g_linux_exec64_binary,
            bytes_read,
            &header,
            phdrs,
            ELF64_MAX_PROGRAM_HEADERS,
            &phdr_summary) != ELF64_OK)
    {
        g_linux_exec64_telemetry.failure_code = phdr_summary.error;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    linux_exec64_record_elf_metadata(&header, &phdr_summary, g_linux_exec64_binary, bytes_read, phdrs);
    g_linux_exec64_telemetry.elf = 1u;
    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_DYNAMIC;
    dynamic_result = linux_exec64_try_dynamic_mapping(
        owner_id,
        nvme_fs_capability,
        console_capability,
        source,
        &header,
        phdrs,
        &phdr_summary,
        bytes_read,
        argc,
        1u,
        &pid);
    if (dynamic_result == LINUX_EXEC64_DYNAMIC_FAILED)
    {
        g_linux_exec64_telemetry.failure_code =
            (g_linux_exec64_telemetry.dynamic_map_error != 0u)
                ? g_linux_exec64_telemetry.dynamic_map_error
                : 8u;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    dynamic_launch = (dynamic_result == LINUX_EXEC64_DYNAMIC_READY) ? 1u : 0u;

    if (dynamic_launch == 0u)
    {
        g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_STATIC;
        if ((header.type != ELF64_TYPE_EXEC)
            || (phdr_summary.load_count == 0u)
            || (phdr_summary.interp_count != 0u)
            || (phdr_summary.dynamic_count != 0u))
        {
            g_linux_exec64_telemetry.failure_code = 8u;
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }
        if (linux_exec64_static_loads_overlap_low_kernel_window(
                phdrs,
                header.phnum,
                &g_linux_exec64_telemetry.load_first_vaddr,
                &g_linux_exec64_telemetry.load_max_end) != 0u)
        {
            g_linux_exec64_telemetry.failure_code = ELF64_ERROR_LOAD_ADDRESS;
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }
        g_linux_exec64_telemetry.static_elf = 1u;

        g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_PROCESS;
        parent_pid = linux_exec64_parent_pid_for_owner(owner_id);
        pid = (parent_pid != PROCESS64_INVALID_PID)
            ? process64_spawn_clone(parent_pid)
            : PROCESS64_INVALID_PID;
        g_linux_exec64_telemetry.pid = pid;
        if (pid == PROCESS64_INVALID_PID)
        {
            g_linux_exec64_telemetry.failure_code = 3u;
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }

        process_owner = process64_principal(pid);
        root_authority = process64_runtime_token(pid);
        if (root_authority == 0u)
        {
            root_authority =
                (source == LINUX_EXEC64_SOURCE_BOOT_MEDIA) ? console_capability : nvme_fs_capability;
        }
        if (paging64_process_root_alloc(pid, process_owner, root_authority) == 0u)
        {
            g_linux_exec64_telemetry.failure_code = 11u;
            linux_exec64_release_failed_process(pid);
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }
        process_root_token = paging64_process_root_token(pid);
        if ((process_root_token == 0u)
            || (process64_attach_page_root(
                    pid,
                    paging64_process_root_physical(pid),
                    paging64_process_root_slot(pid),
                    process_root_token,
                    root_authority) == 0u))
        {
            if (process_root_token != 0u)
            {
                (void)paging64_process_root_release(pid, process_root_token);
            }
            g_linux_exec64_telemetry.failure_code = 12u;
            (void)process64_release_clone(pid);
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }
        linux_exec64_record_process_root_telemetry(pid);
    }

    process_owner = process64_principal(pid);
    root_authority = process64_runtime_token(pid);
    process_root_token = process64_page_root_token(pid);

    stdin_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INPUT,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        process_owner);
    stdout_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        process_owner);
    stderr_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        process_owner);

    if (((dynamic_launch == 0u) && (vma64_init_process(pid) == 0u))
        || (persona_audit64_attach(pid) == 0u)
        || (stdin_capability == CAPABILITY64_INVALID_HANDLE)
        || (stdout_capability == CAPABILITY64_INVALID_HANDLE)
        || (stderr_capability == CAPABILITY64_INVALID_HANDLE)
        || (fd64_init_process(
                pid,
                process_owner,
                stdin_capability,
                stdout_capability,
                stderr_capability) == 0u)
        || ((dynamic_launch == 0u)
            && (persona64_init_linux_elf(pid, linux_abi64_dispatch_table()) != PERSONA64_ATTACH_OK))
        || ((nvme_fs_capability != CAPABILITY64_INVALID_HANDLE)
            && (linux_vfs64_bind_nvme_read(pid, owner_id, nvme_fs_capability) == 0u)))
    {
        g_linux_exec64_telemetry.failure_code = 4u;
        linux_exec64_release_failed_process(pid);
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    g_linux_exec64_telemetry.nvme_vfs_bind =
        (nvme_fs_capability != CAPABILITY64_INVALID_HANDLE) ? 1u : 0u;

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_LAUNCH;
    if (dynamic_launch == 0u)
    {
        load_cr3_switch = paging64_switch_to_process_root(pid, 0x4C4F4144u);
        if (load_cr3_switch == 0u)
        {
            g_linux_exec64_telemetry.failure_code = 13u;
            linux_exec64_release_failed_process(pid);
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }
        launch_result = elf64_launch_static(
            pid,
            g_linux_exec64_binary,
            bytes_read,
            0ull,
            LINUX_EXEC64_REAL_STACK_BASE,
            LINUX_EXEC64_REAL_STACK_BYTES,
            argc,
            g_linux_exec64_staged_argv_ptrs,
            LINUX_EXEC64_DEFAULT_ENV_COUNT,
            g_linux_exec64_staged_envp_ptrs,
            0u,
            &g_linux_exec64_launch);
        load_cr3_restore = paging64_switch_to_kernel_root(0x4C4F414Bu);
        g_linux_exec64_telemetry.mapped_regions = g_linux_exec64_launch.load_result.mapped_count;
        g_linux_exec64_telemetry.mapped_pages =
            (u32)(g_linux_exec64_launch.load_result.total_map_bytes / VMA64_PAGE_BYTES);
        g_linux_exec64_telemetry.stack_pages = LINUX_EXEC64_REAL_STACK_BYTES / VMA64_PAGE_BYTES;
        g_linux_exec64_telemetry.envc = g_linux_exec64_launch.stack_result.envc;
        g_linux_exec64_telemetry.vma_pt_private = paging64_process_root_last_vma_pt_private();
        if ((launch_result != ELF64_OK)
            || (g_linux_exec64_launch.transfer_ready == 0u)
            || (load_cr3_restore == 0u))
        {
            g_linux_exec64_telemetry.failure_code = g_linux_exec64_launch.error;
            linux_exec64_release_failed_process(pid);
            linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
            return LINUX_EXEC64_RESULT_FAILED;
        }
    }
    else if (g_linux_exec64_launch.transfer_ready == 0u)
    {
        g_linux_exec64_telemetry.failure_code = ELF64_ERROR_LAUNCH_TRANSFER;
        linux_exec64_release_failed_process(pid);
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }

    vma_ctx = process64_vma_root(pid);
    fd_ctx = process64_fd_table(pid);
    audit_ctx = process64_audit_ctx(pid);

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_TASK;
    scheduler64_runqueue_reset();
    exit_probe_arm = syscall64_native_arm_linux_exit_probe(pid, LINUX_EXEC64_EXIT_PROBE_RESULT);
    task = scheduler64_runqueue_register_process_task(
        pid,
        process64_runtime_token(pid),
        process64_runtime_user_entry_token(pid),
        g_linux_exec64_launch.transfer_rip,
        g_linux_exec64_launch.initial_rsp,
        (u64)g_linux_exec64_launch.transfer_selectors,
        (u64)process64_runtime_user_entry_rflags(pid));
    g_linux_exec64_telemetry.task = task;
    g_linux_exec64_telemetry.dynamic_task_registered =
        ((dynamic_launch != 0u) && (task != SCHEDULER64_INVALID_TASK)) ? 1u : 0u;
    if ((exit_probe_arm == 0u) || (task == SCHEDULER64_INVALID_TASK))
    {
        g_linux_exec64_telemetry.failure_code = 5u;
        (void)syscall64_native_clear_linux_exit_probe(pid);
        scheduler64_runqueue_stop();
        scheduler64_runqueue_reset();
        linux_exec64_release_failed_process(pid);
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_RUN;
    console_bytes_before = console64_byte_count();
    unimplemented_before = linux_abi64_unimplemented_count();
    page_faults_before = interrupts64_page_fault_count();
    getdents64_calls_before = linux_abi64_getdents64_count();
    getdents64_entries_before = linux_abi64_getdents64_entry_count();
    getdents64_bytes_before = linux_abi64_getdents64_byte_count();
    stat_before = linux_abi64_stat_count();
    stat_denial_before = linux_abi64_stat_denial_count();
    stat_fault_before = linux_abi64_stat_fault_count();
    fstat_before = linux_abi64_fstat_count();
    fstat_denial_before = linux_abi64_fstat_denial_count();
    fstat_fault_before = linux_abi64_fstat_fault_count();
    newfstatat_before = linux_abi64_newfstatat_count();
    newfstatat_denial_before = linux_abi64_newfstatat_denial_count();
    newfstatat_fault_before = linux_abi64_newfstatat_fault_count();
    lseek_before = linux_abi64_lseek_count();
    lseek_denial_before = linux_abi64_lseek_denial_count();
    dup_before = linux_abi64_dup_count();
    dup2_before = linux_abi64_dup2_count();
    dup3_before = linux_abi64_dup3_count();
    dup_denial_before = linux_abi64_dup_denial_count();
    fcntl_before = linux_abi64_fcntl_count();
    fcntl_denial_before = linux_abi64_fcntl_denial_count();
    readlink_before = linux_abi64_readlink_count();
    readlink_bytes_before = linux_abi64_readlink_byte_count();
    readlink_denial_before = linux_abi64_readlink_denial_count();
    readlink_fault_before = linux_abi64_readlink_fault_count();
    getcwd_before = linux_abi64_getcwd_count();
    getcwd_bytes_before = linux_abi64_getcwd_byte_count();
    getcwd_denial_before = linux_abi64_getcwd_denial_count();
    getcwd_fault_before = linux_abi64_getcwd_fault_count();
    path_relative_before = linux_abi64_path_relative_count();
    path_dot_before = linux_abi64_path_dot_count();
    path_dotdot_before = linux_abi64_path_dotdot_count();
    path_trailing_before = linux_abi64_path_trailing_count();
    path_trailing_denial_before = linux_abi64_path_trailing_denial_count();
    path_fault_before = linux_abi64_path_fault_count();
    chdir_before = linux_abi64_chdir_count();
    fchdir_before = linux_abi64_fchdir_count();
    chdir_denial_before = linux_abi64_chdir_denial_count();
    chdir_fault_before = linux_abi64_chdir_fault_count();
    openat_before = linux_abi64_openat_count();
    read_calls_before = linux_abi64_read_count();
    read_bytes_before = linux_abi64_read_byte_count();
    write_calls_before = linux_abi64_write_count();
    write_bytes_before = linux_abi64_write_byte_count();
    signal_sigpipe_before = linux_abi64_signal_sigpipe_count();
    signal_sigchld_before = linux_abi64_signal_sigchld_count();
    signal_rt_sigreturn_before = linux_abi64_rt_sigreturn_count();
    signal_frame_fault_before = linux_abi64_signal_delivery_fault_count()
        + linux_abi64_rt_sigreturn_fault_count();
    mmap_calls_before = linux_abi64_mmap_count();
    mmap_bytes_before = linux_abi64_mmap_byte_count();
    mmap_denial_before = linux_abi64_mmap_denial_count();
    mmap_file_before = linux_abi64_mmap_file_count();
    mmap_file_bytes_before = linux_abi64_mmap_file_byte_count();
    mmap_file_denial_before = linux_abi64_mmap_file_denial_count();
    futex_wait_before = linux_abi64_futex_wait_count();
    futex_wake_before = linux_abi64_futex_wake_count();
    futex_woken_before = linux_abi64_futex_woken_count();
    thread_exit_cleartid_before = linux_abi64_thread_exit_cleartid_count();
    thread_exit_cleartid_fault_before = linux_abi64_thread_exit_cleartid_fault_count();
    pipe_calls_before = linux_abi64_pipe_count();
    pipe_denial_before = linux_abi64_pipe_denial_count();
    pipe_fault_before = linux_abi64_pipe_fault_count();
    pipe2_calls_before = linux_abi64_pipe2_count();
    pipe2_denial_before = linux_abi64_pipe2_denial_count();
    pipe2_fault_before = linux_abi64_pipe2_fault_count();
    pipe_blocks_before = pipe64_block_count();
    pipe_wakes_before = pipe64_wake_count();
    pipe_replays_before = pipe64_replay_wake_count();
    pipe_provider_denial_before = pipe64_denial_count();
    fd_fork_pipe_copy_before = fd64_fork_pipe_copy_count();
    fd_fork_pipe_denial_before = fd64_fork_pipe_denial_count();
    readv_calls_before = linux_abi64_readv_count();
    readv_bytes_before = linux_abi64_readv_byte_count();
    writev_calls_before = linux_abi64_writev_count();
    writev_bytes_before = linux_abi64_writev_byte_count();
    poll_calls_before = linux_abi64_poll_count();
    ppoll_calls_before = linux_abi64_ppoll_count();
    poll_ready_before = linux_abi64_poll_ready_count();
    geteuid_before = linux_abi64_geteuid_count();
    getppid_before = linux_abi64_getppid_count();
    ioctl_before = linux_abi64_ioctl_count();
    ioctl_tty_before = linux_abi64_ioctl_tty_count();
    ioctl_enotty_before = linux_abi64_ioctl_enotty_count();
    ioctl_enosys_before = linux_abi64_ioctl_enosys_count();
    prctl_before = linux_abi64_prctl_count();
    prctl_set_name_before = linux_abi64_prctl_set_name_count();
    prctl_get_name_before = linux_abi64_prctl_get_name_count();
    prctl_enosys_before = linux_abi64_prctl_enosys_count();
    execve_before = linux_abi64_execve_count();
    execveat_before = linux_abi64_execveat_count();
    execve_denial_before = linux_abi64_execve_denial_count();
    execve_fault_before = linux_abi64_execve_fault_count();
    syscall_root_repair_before = linux_abi64_dispatch_root_repair_count();
    syscall_root_reload_before = linux_abi64_dispatch_root_reload_count();
    syscall_root_denial_before = linux_abi64_dispatch_root_denial_count();
    fs_save_before = scheduler64_runqueue_fs_save_count();
    fs_restore_before = scheduler64_runqueue_fs_restore_count();
    fs_set_before = scheduler64_runqueue_fs_set_count();
    scheduler_denial_before = scheduler64_runqueue_block_denial_count();
    fork_before = linux_abi64_fork_count();
    fork_success_before = linux_abi64_fork_success_count();
    fork_enosys_before = linux_abi64_fork_enosys_count();
    fork_denial_before = linux_abi64_fork_denial_count();
    clone_thread_before = linux_abi64_clone_thread_count();
    clone_denial_before = linux_abi64_clone_denial_count();
    wait4_before = linux_abi64_wait4_count();
    wait4_reap_before = linux_abi64_wait4_reap_count();
    child_root_cleanup_before = linux_abi64_child_root_cleanup_count();
    nvme_vfs_reads_before = linux_vfs64_nvme_read_count();
    nvme_vfs_readdirs_before = linux_vfs64_nvme_readdir_count();
    nvme_vfs_dirents_before = linux_vfs64_nvme_dirent_count();
    bin_vfs_alias_before = linux_vfs64_bin_alias_count();
    bin_vfs_open_before = linux_vfs64_bin_open_count();
    bin_vfs_read_before = linux_vfs64_bin_read_count();
    bin_vfs_denial_before = linux_vfs64_bin_denial_count();
    localbin_vfs_alias_before = linux_vfs64_localbin_alias_count();
    localbin_vfs_open_before = linux_vfs64_localbin_open_count();
    localbin_vfs_read_before = linux_vfs64_localbin_read_count();
    localbin_vfs_denial_before = linux_vfs64_localbin_denial_count();
    cr3_process_switch_before = paging64_process_root_switch_count();
    cr3_kernel_switch_before = paging64_process_root_kernel_switch_count();
    runqueue_started = scheduler64_runqueue_start(task);
    cr3_process_switch_after = paging64_process_root_switch_count();
    g_linux_exec64_telemetry.dynamic_transfer_started =
        ((dynamic_launch != 0u) && (runqueue_started != 0u)) ? 1u : 0u;
    g_linux_exec64_telemetry.cr3_start =
        ((runqueue_started != 0u) && (cr3_process_switch_after > cr3_process_switch_before))
            ? 1u
            : 0u;
    g_linux_exec64_telemetry.active_cr3_match =
        ((runqueue_started != 0u)
            && (paging64_current_root_physical()
                == (g_linux_exec64_telemetry.root_physical & 0xFFFFFFFFFFFFF000ull)))
            ? 1u
            : 0u;
    transfer_result =
        (runqueue_started != 0u)
            ? interrupts64_trigger_user_entry_probe(
                g_linux_exec64_launch.transfer_rip,
                g_linux_exec64_launch.initial_rsp,
                (u64)g_linux_exec64_launch.transfer_selectors,
                (u64)process64_runtime_user_entry_rflags(pid))
            : 0u;
    if (runqueue_started != 0u)
    {
        (void)paging64_switch_to_kernel_root(0x4C584558u);
    }
    cr3_kernel_switch_after = paging64_process_root_kernel_switch_count();
    g_linux_exec64_telemetry.cr3_exit =
        (cr3_kernel_switch_after > cr3_kernel_switch_before) ? 1u : 0u;
    console_bytes_after = console64_byte_count();
    unimplemented_after = linux_abi64_unimplemented_count();
    page_faults_after = interrupts64_page_fault_count();
    getdents64_calls_after = linux_abi64_getdents64_count();
    getdents64_entries_after = linux_abi64_getdents64_entry_count();
    getdents64_bytes_after = linux_abi64_getdents64_byte_count();
    stat_after = linux_abi64_stat_count();
    stat_denial_after = linux_abi64_stat_denial_count();
    stat_fault_after = linux_abi64_stat_fault_count();
    fstat_after = linux_abi64_fstat_count();
    fstat_denial_after = linux_abi64_fstat_denial_count();
    fstat_fault_after = linux_abi64_fstat_fault_count();
    newfstatat_after = linux_abi64_newfstatat_count();
    newfstatat_denial_after = linux_abi64_newfstatat_denial_count();
    newfstatat_fault_after = linux_abi64_newfstatat_fault_count();
    lseek_after = linux_abi64_lseek_count();
    lseek_denial_after = linux_abi64_lseek_denial_count();
    dup_after = linux_abi64_dup_count();
    dup2_after = linux_abi64_dup2_count();
    dup3_after = linux_abi64_dup3_count();
    dup_denial_after = linux_abi64_dup_denial_count();
    fcntl_after = linux_abi64_fcntl_count();
    fcntl_denial_after = linux_abi64_fcntl_denial_count();
    readlink_after = linux_abi64_readlink_count();
    readlink_bytes_after = linux_abi64_readlink_byte_count();
    readlink_denial_after = linux_abi64_readlink_denial_count();
    readlink_fault_after = linux_abi64_readlink_fault_count();
    getcwd_after = linux_abi64_getcwd_count();
    getcwd_bytes_after = linux_abi64_getcwd_byte_count();
    getcwd_denial_after = linux_abi64_getcwd_denial_count();
    getcwd_fault_after = linux_abi64_getcwd_fault_count();
    path_relative_after = linux_abi64_path_relative_count();
    path_dot_after = linux_abi64_path_dot_count();
    path_dotdot_after = linux_abi64_path_dotdot_count();
    path_trailing_after = linux_abi64_path_trailing_count();
    path_trailing_denial_after = linux_abi64_path_trailing_denial_count();
    path_fault_after = linux_abi64_path_fault_count();
    chdir_after = linux_abi64_chdir_count();
    fchdir_after = linux_abi64_fchdir_count();
    chdir_denial_after = linux_abi64_chdir_denial_count();
    chdir_fault_after = linux_abi64_chdir_fault_count();
    openat_after = linux_abi64_openat_count();
    read_calls_after = linux_abi64_read_count();
    read_bytes_after = linux_abi64_read_byte_count();
    write_calls_after = linux_abi64_write_count();
    write_bytes_after = linux_abi64_write_byte_count();
    signal_sigpipe_after = linux_abi64_signal_sigpipe_count();
    signal_sigchld_after = linux_abi64_signal_sigchld_count();
    signal_rt_sigreturn_after = linux_abi64_rt_sigreturn_count();
    signal_frame_fault_after = linux_abi64_signal_delivery_fault_count()
        + linux_abi64_rt_sigreturn_fault_count();
    mmap_calls_after = linux_abi64_mmap_count();
    mmap_bytes_after = linux_abi64_mmap_byte_count();
    mmap_denial_after = linux_abi64_mmap_denial_count();
    mmap_file_after = linux_abi64_mmap_file_count();
    mmap_file_bytes_after = linux_abi64_mmap_file_byte_count();
    mmap_file_denial_after = linux_abi64_mmap_file_denial_count();
    futex_wait_after = linux_abi64_futex_wait_count();
    futex_wake_after = linux_abi64_futex_wake_count();
    futex_woken_after = linux_abi64_futex_woken_count();
    thread_exit_cleartid_after = linux_abi64_thread_exit_cleartid_count();
    thread_exit_cleartid_fault_after = linux_abi64_thread_exit_cleartid_fault_count();
    pipe_calls_after = linux_abi64_pipe_count();
    pipe_denial_after = linux_abi64_pipe_denial_count();
    pipe_fault_after = linux_abi64_pipe_fault_count();
    pipe2_calls_after = linux_abi64_pipe2_count();
    pipe2_denial_after = linux_abi64_pipe2_denial_count();
    pipe2_fault_after = linux_abi64_pipe2_fault_count();
    pipe_blocks_after = pipe64_block_count();
    pipe_wakes_after = pipe64_wake_count();
    pipe_replays_after = pipe64_replay_wake_count();
    pipe_provider_denial_after = pipe64_denial_count();
    fd_fork_pipe_copy_after = fd64_fork_pipe_copy_count();
    fd_fork_pipe_denial_after = fd64_fork_pipe_denial_count();
    readv_calls_after = linux_abi64_readv_count();
    readv_bytes_after = linux_abi64_readv_byte_count();
    writev_calls_after = linux_abi64_writev_count();
    writev_bytes_after = linux_abi64_writev_byte_count();
    poll_calls_after = linux_abi64_poll_count();
    ppoll_calls_after = linux_abi64_ppoll_count();
    poll_ready_after = linux_abi64_poll_ready_count();
    geteuid_after = linux_abi64_geteuid_count();
    getppid_after = linux_abi64_getppid_count();
    ioctl_after = linux_abi64_ioctl_count();
    ioctl_tty_after = linux_abi64_ioctl_tty_count();
    ioctl_enotty_after = linux_abi64_ioctl_enotty_count();
    ioctl_enosys_after = linux_abi64_ioctl_enosys_count();
    prctl_after = linux_abi64_prctl_count();
    prctl_set_name_after = linux_abi64_prctl_set_name_count();
    prctl_get_name_after = linux_abi64_prctl_get_name_count();
    prctl_enosys_after = linux_abi64_prctl_enosys_count();
    execve_after = linux_abi64_execve_count();
    execveat_after = linux_abi64_execveat_count();
    execve_denial_after = linux_abi64_execve_denial_count();
    execve_fault_after = linux_abi64_execve_fault_count();
    syscall_root_repair_after = linux_abi64_dispatch_root_repair_count();
    syscall_root_reload_after = linux_abi64_dispatch_root_reload_count();
    syscall_root_denial_after = linux_abi64_dispatch_root_denial_count();
    fs_save_after = scheduler64_runqueue_fs_save_count();
    fs_restore_after = scheduler64_runqueue_fs_restore_count();
    fs_set_after = scheduler64_runqueue_fs_set_count();
    scheduler_denial_after = scheduler64_runqueue_block_denial_count();
    fork_after = linux_abi64_fork_count();
    fork_success_after = linux_abi64_fork_success_count();
    fork_enosys_after = linux_abi64_fork_enosys_count();
    fork_denial_after = linux_abi64_fork_denial_count();
    clone_thread_after = linux_abi64_clone_thread_count();
    clone_denial_after = linux_abi64_clone_denial_count();
    wait4_after = linux_abi64_wait4_count();
    wait4_reap_after = linux_abi64_wait4_reap_count();
    child_root_cleanup_after = linux_abi64_child_root_cleanup_count();
    nvme_vfs_reads_after = linux_vfs64_nvme_read_count();
    nvme_vfs_readdirs_after = linux_vfs64_nvme_readdir_count();
    nvme_vfs_dirents_after = linux_vfs64_nvme_dirent_count();
    bin_vfs_alias_after = linux_vfs64_bin_alias_count();
    bin_vfs_open_after = linux_vfs64_bin_open_count();
    bin_vfs_read_after = linux_vfs64_bin_read_count();
    bin_vfs_denial_after = linux_vfs64_bin_denial_count();
    localbin_vfs_alias_after = linux_vfs64_localbin_alias_count();
    localbin_vfs_open_after = linux_vfs64_localbin_open_count();
    localbin_vfs_read_after = linux_vfs64_localbin_read_count();
    localbin_vfs_denial_after = linux_vfs64_localbin_denial_count();

    g_linux_exec64_telemetry.started = (runqueue_started != 0u) ? 1u : 0u;
    g_linux_exec64_telemetry.console_bytes =
        (console_bytes_after >= console_bytes_before)
            ? (console_bytes_after - console_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.dynamic_console_bytes =
        (dynamic_launch != 0u) ? g_linux_exec64_telemetry.console_bytes : 0u;
    g_linux_exec64_telemetry.signal_sigpipe =
        (signal_sigpipe_after >= signal_sigpipe_before)
            ? (signal_sigpipe_after - signal_sigpipe_before)
            : 0u;
    g_linux_exec64_telemetry.signal_sigchld =
        (signal_sigchld_after >= signal_sigchld_before)
            ? (signal_sigchld_after - signal_sigchld_before)
            : 0u;
    g_linux_exec64_telemetry.signal_rt_sigreturn =
        (signal_rt_sigreturn_after >= signal_rt_sigreturn_before)
            ? (signal_rt_sigreturn_after - signal_rt_sigreturn_before)
            : 0u;
    g_linux_exec64_telemetry.signal_frame_fault =
        (signal_frame_fault_after >= signal_frame_fault_before)
            ? (signal_frame_fault_after - signal_frame_fault_before)
            : 0u;
    g_linux_exec64_telemetry.mmap_calls =
        (mmap_calls_after >= mmap_calls_before)
            ? (mmap_calls_after - mmap_calls_before)
            : 0u;
    g_linux_exec64_telemetry.mmap_bytes =
        (mmap_bytes_after >= mmap_bytes_before)
            ? (mmap_bytes_after - mmap_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.mmap_denial =
        (mmap_denial_after >= mmap_denial_before)
            ? (mmap_denial_after - mmap_denial_before)
            : 0u;
    g_linux_exec64_telemetry.mmap_file_calls =
        (mmap_file_after >= mmap_file_before)
            ? (mmap_file_after - mmap_file_before)
            : 0u;
    g_linux_exec64_telemetry.mmap_file_bytes =
        (mmap_file_bytes_after >= mmap_file_bytes_before)
            ? (mmap_file_bytes_after - mmap_file_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.mmap_file_denial =
        (mmap_file_denial_after >= mmap_file_denial_before)
            ? (mmap_file_denial_after - mmap_file_denial_before)
            : 0u;
    g_linux_exec64_telemetry.mmap_last_error = linux_abi64_mmap_last_error();
    g_linux_exec64_telemetry.mmap_last_flags = linux_abi64_mmap_last_flags();
    g_linux_exec64_telemetry.mmap_last_length = linux_abi64_mmap_last_length();
    g_linux_exec64_telemetry.futex_wait =
        (futex_wait_after >= futex_wait_before)
            ? (futex_wait_after - futex_wait_before)
            : 0u;
    g_linux_exec64_telemetry.futex_wake =
        (futex_wake_after >= futex_wake_before)
            ? (futex_wake_after - futex_wake_before)
            : 0u;
    g_linux_exec64_telemetry.futex_woken =
        (futex_woken_after >= futex_woken_before)
            ? (futex_woken_after - futex_woken_before)
            : 0u;
    g_linux_exec64_telemetry.futex_waiters_final = linux_abi64_futex_waiter_count();
    g_linux_exec64_telemetry.thread_exit_cleartid =
        (thread_exit_cleartid_after >= thread_exit_cleartid_before)
            ? (thread_exit_cleartid_after - thread_exit_cleartid_before)
            : 0u;
    g_linux_exec64_telemetry.thread_exit_cleartid_fault =
        (thread_exit_cleartid_fault_after >= thread_exit_cleartid_fault_before)
            ? (thread_exit_cleartid_fault_after - thread_exit_cleartid_fault_before)
            : 0u;
    g_linux_exec64_telemetry.syscall_unimplemented_delta =
        (unimplemented_after >= unimplemented_before)
            ? (unimplemented_after - unimplemented_before)
            : 0u;
    g_linux_exec64_telemetry.syscall_unimplemented_last = linux_abi64_unimplemented_last_syscall();
    g_linux_exec64_telemetry.syscall_unimplemented_last_rip = linux_abi64_unimplemented_last_rip();
    g_linux_exec64_telemetry.page_fault_delta =
        (page_faults_after >= page_faults_before)
            ? (page_faults_after - page_faults_before)
            : 0u;
    g_linux_exec64_telemetry.getdents64_calls =
        (getdents64_calls_after >= getdents64_calls_before)
            ? (getdents64_calls_after - getdents64_calls_before)
            : 0u;
    g_linux_exec64_telemetry.getdents64_entries =
        (getdents64_entries_after >= getdents64_entries_before)
            ? (getdents64_entries_after - getdents64_entries_before)
            : 0u;
    g_linux_exec64_telemetry.getdents64_bytes =
        (getdents64_bytes_after >= getdents64_bytes_before)
            ? (getdents64_bytes_after - getdents64_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.stat_calls =
        (stat_after >= stat_before) ? (stat_after - stat_before) : 0u;
    g_linux_exec64_telemetry.stat_denial =
        (stat_denial_after >= stat_denial_before)
            ? (stat_denial_after - stat_denial_before)
            : 0u;
    g_linux_exec64_telemetry.stat_fault =
        (stat_fault_after >= stat_fault_before)
            ? (stat_fault_after - stat_fault_before)
            : 0u;
    g_linux_exec64_telemetry.fstat_calls =
        (fstat_after >= fstat_before) ? (fstat_after - fstat_before) : 0u;
    g_linux_exec64_telemetry.fstat_denial =
        (fstat_denial_after >= fstat_denial_before)
            ? (fstat_denial_after - fstat_denial_before)
            : 0u;
    g_linux_exec64_telemetry.fstat_fault =
        (fstat_fault_after >= fstat_fault_before)
            ? (fstat_fault_after - fstat_fault_before)
            : 0u;
    g_linux_exec64_telemetry.newfstatat_calls =
        (newfstatat_after >= newfstatat_before)
            ? (newfstatat_after - newfstatat_before)
            : 0u;
    g_linux_exec64_telemetry.newfstatat_denial =
        (newfstatat_denial_after >= newfstatat_denial_before)
            ? (newfstatat_denial_after - newfstatat_denial_before)
            : 0u;
    g_linux_exec64_telemetry.newfstatat_fault =
        (newfstatat_fault_after >= newfstatat_fault_before)
            ? (newfstatat_fault_after - newfstatat_fault_before)
            : 0u;
    g_linux_exec64_telemetry.lseek_calls =
        (lseek_after >= lseek_before) ? (lseek_after - lseek_before) : 0u;
    g_linux_exec64_telemetry.lseek_denial =
        (lseek_denial_after >= lseek_denial_before)
            ? (lseek_denial_after - lseek_denial_before)
            : 0u;
    g_linux_exec64_telemetry.dup_calls =
        (dup_after >= dup_before) ? (dup_after - dup_before) : 0u;
    g_linux_exec64_telemetry.dup2_calls =
        (dup2_after >= dup2_before) ? (dup2_after - dup2_before) : 0u;
    g_linux_exec64_telemetry.dup3_calls =
        (dup3_after >= dup3_before) ? (dup3_after - dup3_before) : 0u;
    g_linux_exec64_telemetry.dup_denial =
        (dup_denial_after >= dup_denial_before)
            ? (dup_denial_after - dup_denial_before)
            : 0u;
    g_linux_exec64_telemetry.fcntl_calls =
        (fcntl_after >= fcntl_before) ? (fcntl_after - fcntl_before) : 0u;
    g_linux_exec64_telemetry.fcntl_denial =
        (fcntl_denial_after >= fcntl_denial_before)
            ? (fcntl_denial_after - fcntl_denial_before)
            : 0u;
    g_linux_exec64_telemetry.readlink_calls =
        (readlink_after >= readlink_before)
            ? (readlink_after - readlink_before)
            : 0u;
    g_linux_exec64_telemetry.readlink_bytes =
        (readlink_bytes_after >= readlink_bytes_before)
            ? (readlink_bytes_after - readlink_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.readlink_denial =
        (readlink_denial_after >= readlink_denial_before)
            ? (readlink_denial_after - readlink_denial_before)
            : 0u;
    g_linux_exec64_telemetry.readlink_fault =
        (readlink_fault_after >= readlink_fault_before)
            ? (readlink_fault_after - readlink_fault_before)
            : 0u;
    g_linux_exec64_telemetry.readlink_last_result = linux_abi64_readlink_last_result();
    g_linux_exec64_telemetry.getcwd_calls =
        (getcwd_after >= getcwd_before)
            ? (getcwd_after - getcwd_before)
            : 0u;
    g_linux_exec64_telemetry.getcwd_bytes =
        (getcwd_bytes_after >= getcwd_bytes_before)
            ? (getcwd_bytes_after - getcwd_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.getcwd_denial =
        (getcwd_denial_after >= getcwd_denial_before)
            ? (getcwd_denial_after - getcwd_denial_before)
            : 0u;
    g_linux_exec64_telemetry.getcwd_fault =
        (getcwd_fault_after >= getcwd_fault_before)
            ? (getcwd_fault_after - getcwd_fault_before)
            : 0u;
    g_linux_exec64_telemetry.path_relative =
        (path_relative_after >= path_relative_before)
            ? (path_relative_after - path_relative_before)
            : 0u;
    g_linux_exec64_telemetry.path_dot =
        (path_dot_after >= path_dot_before)
            ? (path_dot_after - path_dot_before)
            : 0u;
    g_linux_exec64_telemetry.path_dotdot =
        (path_dotdot_after >= path_dotdot_before)
            ? (path_dotdot_after - path_dotdot_before)
            : 0u;
    g_linux_exec64_telemetry.path_trailing =
        (path_trailing_after >= path_trailing_before)
            ? (path_trailing_after - path_trailing_before)
            : 0u;
    g_linux_exec64_telemetry.path_trailing_denial =
        (path_trailing_denial_after >= path_trailing_denial_before)
            ? (path_trailing_denial_after - path_trailing_denial_before)
            : 0u;
    g_linux_exec64_telemetry.path_fault =
        (path_fault_after >= path_fault_before)
            ? (path_fault_after - path_fault_before)
            : 0u;
    g_linux_exec64_telemetry.chdir_calls =
        (chdir_after >= chdir_before)
            ? (chdir_after - chdir_before)
            : 0u;
    g_linux_exec64_telemetry.fchdir_calls =
        (fchdir_after >= fchdir_before)
            ? (fchdir_after - fchdir_before)
            : 0u;
    g_linux_exec64_telemetry.chdir_denial =
        (chdir_denial_after >= chdir_denial_before)
            ? (chdir_denial_after - chdir_denial_before)
            : 0u;
    g_linux_exec64_telemetry.chdir_fault =
        (chdir_fault_after >= chdir_fault_before)
            ? (chdir_fault_after - chdir_fault_before)
            : 0u;
    g_linux_exec64_telemetry.openat_calls =
        (openat_after >= openat_before) ? (openat_after - openat_before) : 0u;
    g_linux_exec64_telemetry.read_calls =
        (read_calls_after >= read_calls_before)
            ? (read_calls_after - read_calls_before)
            : 0u;
    g_linux_exec64_telemetry.read_bytes =
        (read_bytes_after >= read_bytes_before)
            ? (read_bytes_after - read_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.write_calls =
        (write_calls_after >= write_calls_before)
            ? (write_calls_after - write_calls_before)
            : 0u;
    g_linux_exec64_telemetry.write_bytes =
        (write_bytes_after >= write_bytes_before)
            ? (write_bytes_after - write_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.pipe_calls =
        (pipe_calls_after >= pipe_calls_before)
            ? (pipe_calls_after - pipe_calls_before)
            : 0u;
    g_linux_exec64_telemetry.pipe_denial =
        (pipe_denial_after >= pipe_denial_before)
            ? (pipe_denial_after - pipe_denial_before)
            : 0u;
    g_linux_exec64_telemetry.pipe_fault =
        (pipe_fault_after >= pipe_fault_before)
            ? (pipe_fault_after - pipe_fault_before)
            : 0u;
    g_linux_exec64_telemetry.pipe2_calls =
        (pipe2_calls_after >= pipe2_calls_before)
            ? (pipe2_calls_after - pipe2_calls_before)
            : 0u;
    g_linux_exec64_telemetry.pipe2_denial =
        (pipe2_denial_after >= pipe2_denial_before)
            ? (pipe2_denial_after - pipe2_denial_before)
            : 0u;
    g_linux_exec64_telemetry.pipe2_fault =
        (pipe2_fault_after >= pipe2_fault_before)
            ? (pipe2_fault_after - pipe2_fault_before)
            : 0u;
    g_linux_exec64_telemetry.pipe_live_final = pipe64_live_count();
    g_linux_exec64_telemetry.pipe_blocks =
        (pipe_blocks_after >= pipe_blocks_before)
            ? (pipe_blocks_after - pipe_blocks_before)
            : 0u;
    g_linux_exec64_telemetry.pipe_wakes =
        (pipe_wakes_after >= pipe_wakes_before)
            ? (pipe_wakes_after - pipe_wakes_before)
            : 0u;
    g_linux_exec64_telemetry.pipe_replays =
        (pipe_replays_after >= pipe_replays_before)
            ? (pipe_replays_after - pipe_replays_before)
            : 0u;
    g_linux_exec64_telemetry.pipe_provider_denial =
        (pipe_provider_denial_after >= pipe_provider_denial_before)
            ? (pipe_provider_denial_after - pipe_provider_denial_before)
            : 0u;
    g_linux_exec64_telemetry.fd_fork_pipe_copy =
        (fd_fork_pipe_copy_after >= fd_fork_pipe_copy_before)
            ? (fd_fork_pipe_copy_after - fd_fork_pipe_copy_before)
            : 0u;
    g_linux_exec64_telemetry.fd_fork_pipe_denial =
        (fd_fork_pipe_denial_after >= fd_fork_pipe_denial_before)
            ? (fd_fork_pipe_denial_after - fd_fork_pipe_denial_before)
            : 0u;
    g_linux_exec64_telemetry.fd_fork_pipe_last_fd = fd64_fork_pipe_last_fd();
    g_linux_exec64_telemetry.readv_calls =
        (readv_calls_after >= readv_calls_before)
            ? (readv_calls_after - readv_calls_before)
            : 0u;
    g_linux_exec64_telemetry.readv_bytes =
        (readv_bytes_after >= readv_bytes_before)
            ? (readv_bytes_after - readv_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.writev_calls =
        (writev_calls_after >= writev_calls_before)
            ? (writev_calls_after - writev_calls_before)
            : 0u;
    g_linux_exec64_telemetry.writev_bytes =
        (writev_bytes_after >= writev_bytes_before)
            ? (writev_bytes_after - writev_bytes_before)
            : 0u;
    g_linux_exec64_telemetry.poll_calls =
        (poll_calls_after >= poll_calls_before)
            ? (poll_calls_after - poll_calls_before)
            : 0u;
    g_linux_exec64_telemetry.ppoll_calls =
        (ppoll_calls_after >= ppoll_calls_before)
            ? (ppoll_calls_after - ppoll_calls_before)
            : 0u;
    g_linux_exec64_telemetry.poll_ready =
        (poll_ready_after >= poll_ready_before)
            ? (poll_ready_after - poll_ready_before)
            : 0u;
    g_linux_exec64_telemetry.poll_last_revents = linux_abi64_poll_last_revents();
    g_linux_exec64_telemetry.geteuid_calls =
        (geteuid_after >= geteuid_before)
            ? (geteuid_after - geteuid_before)
            : 0u;
    g_linux_exec64_telemetry.getppid_calls =
        (getppid_after >= getppid_before)
            ? (getppid_after - getppid_before)
            : 0u;
    g_linux_exec64_telemetry.ioctl_calls =
        (ioctl_after >= ioctl_before)
            ? (ioctl_after - ioctl_before)
            : 0u;
    g_linux_exec64_telemetry.ioctl_tty =
        (ioctl_tty_after >= ioctl_tty_before)
            ? (ioctl_tty_after - ioctl_tty_before)
            : 0u;
    g_linux_exec64_telemetry.ioctl_enotty =
        (ioctl_enotty_after >= ioctl_enotty_before)
            ? (ioctl_enotty_after - ioctl_enotty_before)
            : 0u;
    g_linux_exec64_telemetry.ioctl_enosys =
        (ioctl_enosys_after >= ioctl_enosys_before)
            ? (ioctl_enosys_after - ioctl_enosys_before)
            : 0u;
    g_linux_exec64_telemetry.ioctl_last_request = linux_abi64_ioctl_last_request();
    g_linux_exec64_telemetry.ioctl_last_result = linux_abi64_ioctl_last_result();
    g_linux_exec64_telemetry.prctl_calls =
        (prctl_after >= prctl_before)
            ? (prctl_after - prctl_before)
            : 0u;
    g_linux_exec64_telemetry.prctl_set_name =
        (prctl_set_name_after >= prctl_set_name_before)
            ? (prctl_set_name_after - prctl_set_name_before)
            : 0u;
    g_linux_exec64_telemetry.prctl_get_name =
        (prctl_get_name_after >= prctl_get_name_before)
            ? (prctl_get_name_after - prctl_get_name_before)
            : 0u;
    g_linux_exec64_telemetry.prctl_enosys =
        (prctl_enosys_after >= prctl_enosys_before)
            ? (prctl_enosys_after - prctl_enosys_before)
            : 0u;
    g_linux_exec64_telemetry.prctl_last_option = linux_abi64_prctl_last_option();
    g_linux_exec64_telemetry.prctl_last_result = linux_abi64_prctl_last_result();
    g_linux_exec64_telemetry.execve_calls =
        (execve_after >= execve_before)
            ? (execve_after - execve_before)
            : 0u;
    g_linux_exec64_telemetry.execveat_calls =
        (execveat_after >= execveat_before)
            ? (execveat_after - execveat_before)
            : 0u;
    g_linux_exec64_telemetry.execve_denial =
        (execve_denial_after >= execve_denial_before)
            ? (execve_denial_after - execve_denial_before)
            : 0u;
    g_linux_exec64_telemetry.execve_fault =
        (execve_fault_after >= execve_fault_before)
            ? (execve_fault_after - execve_fault_before)
            : 0u;
    g_linux_exec64_telemetry.execve_last_error = linux_abi64_execve_last_error();
    g_linux_exec64_telemetry.execve_last_binary_bytes = linux_abi64_execve_last_binary_bytes();
    g_linux_exec64_telemetry.execve_last_closed_fds = linux_abi64_execve_last_closed_fds();
    g_linux_exec64_telemetry.execve_last_fd_live_before = linux_abi64_execve_last_fd_live_before();
    g_linux_exec64_telemetry.execve_last_fd_live_after = linux_abi64_execve_last_fd_live_after();
    g_linux_exec64_telemetry.execve_last_vma_before = linux_abi64_execve_last_vma_before();
    g_linux_exec64_telemetry.execve_last_vma_released = linux_abi64_execve_last_vma_released();
    g_linux_exec64_telemetry.execve_last_vma_after = linux_abi64_execve_last_vma_after();
    g_linux_exec64_telemetry.execve_last_argc = linux_abi64_execve_last_argc();
    g_linux_exec64_telemetry.execve_last_envc = linux_abi64_execve_last_envc();
    g_linux_exec64_telemetry.execve_last_transfer_ready = linux_abi64_execve_last_transfer_ready();
    g_linux_exec64_telemetry.execve_last_transfer_rip = linux_abi64_execve_last_transfer_rip();
    g_linux_exec64_telemetry.execve_last_transfer_rsp = linux_abi64_execve_last_transfer_rsp();
    g_linux_exec64_telemetry.syscall_root_repair =
        (syscall_root_repair_after >= syscall_root_repair_before)
            ? (syscall_root_repair_after - syscall_root_repair_before)
            : 0u;
    g_linux_exec64_telemetry.syscall_root_reload =
        (syscall_root_reload_after >= syscall_root_reload_before)
            ? (syscall_root_reload_after - syscall_root_reload_before)
            : 0u;
    g_linux_exec64_telemetry.syscall_root_denial =
        (syscall_root_denial_after >= syscall_root_denial_before)
            ? (syscall_root_denial_after - syscall_root_denial_before)
            : 0u;
    g_linux_exec64_telemetry.fs_save =
        (fs_save_after >= fs_save_before) ? (fs_save_after - fs_save_before) : 0u;
    g_linux_exec64_telemetry.fs_restore =
        (fs_restore_after >= fs_restore_before)
            ? (fs_restore_after - fs_restore_before)
            : 0u;
    g_linux_exec64_telemetry.fs_set =
        (fs_set_after >= fs_set_before) ? (fs_set_after - fs_set_before) : 0u;
    g_linux_exec64_telemetry.scheduler_denial =
        (scheduler_denial_after >= scheduler_denial_before)
            ? (scheduler_denial_after - scheduler_denial_before)
            : 0u;
    g_linux_exec64_telemetry.fork_calls =
        (fork_after >= fork_before)
            ? (fork_after - fork_before)
            : 0u;
    g_linux_exec64_telemetry.fork_success =
        (fork_success_after >= fork_success_before)
            ? (fork_success_after - fork_success_before)
            : 0u;
    g_linux_exec64_telemetry.fork_enosys =
        (fork_enosys_after >= fork_enosys_before)
            ? (fork_enosys_after - fork_enosys_before)
            : 0u;
    g_linux_exec64_telemetry.fork_denial =
        (fork_denial_after >= fork_denial_before)
            ? (fork_denial_after - fork_denial_before)
            : 0u;
    g_linux_exec64_telemetry.fork_child_slot = linux_abi64_fork_last_child_slot();
    g_linux_exec64_telemetry.fork_child_root_distinct =
        linux_abi64_fork_last_child_root_distinct();
    g_linux_exec64_telemetry.fork_last_rip = linux_abi64_fork_last_rip();
    g_linux_exec64_telemetry.clone_thread =
        (clone_thread_after >= clone_thread_before)
            ? (clone_thread_after - clone_thread_before)
            : 0u;
    g_linux_exec64_telemetry.clone_thread_success = g_linux_exec64_telemetry.clone_thread;
    g_linux_exec64_telemetry.clone_denial =
        (clone_denial_after >= clone_denial_before)
            ? (clone_denial_after - clone_denial_before)
            : 0u;
    g_linux_exec64_telemetry.clone_last_flags = linux_abi64_clone_last_flags();
    g_linux_exec64_telemetry.clone_unsupported_flags =
        linux_abi64_clone_last_unsupported_flags();
    g_linux_exec64_telemetry.clone_shared_cr3 = linux_abi64_clone_last_shared_cr3();
    g_linux_exec64_telemetry.clone_shared_vma = linux_abi64_clone_last_shared_vma();
    g_linux_exec64_telemetry.clone_shared_fd = linux_abi64_clone_last_shared_fd();
    g_linux_exec64_telemetry.clone_last_task = linux_abi64_clone_last_task_id();
    g_linux_exec64_telemetry.clone_last_tls_base = linux_abi64_clone_last_tls_base();
    g_linux_exec64_telemetry.wait4_calls =
        (wait4_after >= wait4_before)
            ? (wait4_after - wait4_before)
            : 0u;
    g_linux_exec64_telemetry.wait4_reap =
        (wait4_reap_after >= wait4_reap_before)
            ? (wait4_reap_after - wait4_reap_before)
            : 0u;
    g_linux_exec64_telemetry.wait4_last_exit_code = linux_abi64_wait4_last_exit_code();
    g_linux_exec64_telemetry.child_root_cleanup =
        (child_root_cleanup_after >= child_root_cleanup_before)
            ? (child_root_cleanup_after - child_root_cleanup_before)
            : 0u;
    g_linux_exec64_telemetry.nvme_vfs_reads =
        (nvme_vfs_reads_after >= nvme_vfs_reads_before)
            ? (nvme_vfs_reads_after - nvme_vfs_reads_before)
            : 0u;
    g_linux_exec64_telemetry.nvme_vfs_readdirs =
        (nvme_vfs_readdirs_after >= nvme_vfs_readdirs_before)
            ? (nvme_vfs_readdirs_after - nvme_vfs_readdirs_before)
            : 0u;
    g_linux_exec64_telemetry.nvme_vfs_dirents =
        (nvme_vfs_dirents_after >= nvme_vfs_dirents_before)
            ? (nvme_vfs_dirents_after - nvme_vfs_dirents_before)
            : 0u;
    g_linux_exec64_telemetry.nvme_vfs_last_bytes =
        (g_linux_exec64_telemetry.nvme_vfs_reads != 0u)
            ? linux_vfs64_nvme_last_bytes()
            : 0u;
    g_linux_exec64_telemetry.bin_vfs_aliases =
        (bin_vfs_alias_after >= bin_vfs_alias_before)
            ? (bin_vfs_alias_after - bin_vfs_alias_before)
            : 0u;
    g_linux_exec64_telemetry.bin_vfs_opens =
        (bin_vfs_open_after >= bin_vfs_open_before)
            ? (bin_vfs_open_after - bin_vfs_open_before)
            : 0u;
    g_linux_exec64_telemetry.bin_vfs_reads =
        (bin_vfs_read_after >= bin_vfs_read_before)
            ? (bin_vfs_read_after - bin_vfs_read_before)
            : 0u;
    g_linux_exec64_telemetry.bin_vfs_denials =
        (bin_vfs_denial_after >= bin_vfs_denial_before)
            ? (bin_vfs_denial_after - bin_vfs_denial_before)
            : 0u;
    g_linux_exec64_telemetry.localbin_vfs_aliases =
        (localbin_vfs_alias_after >= localbin_vfs_alias_before)
            ? (localbin_vfs_alias_after - localbin_vfs_alias_before)
            : 0u;
    g_linux_exec64_telemetry.localbin_vfs_opens =
        (localbin_vfs_open_after >= localbin_vfs_open_before)
            ? (localbin_vfs_open_after - localbin_vfs_open_before)
            : 0u;
    g_linux_exec64_telemetry.localbin_vfs_reads =
        (localbin_vfs_read_after >= localbin_vfs_read_before)
            ? (localbin_vfs_read_after - localbin_vfs_read_before)
            : 0u;
    g_linux_exec64_telemetry.localbin_vfs_denials =
        (localbin_vfs_denial_after >= localbin_vfs_denial_before)
            ? (localbin_vfs_denial_after - localbin_vfs_denial_before)
            : 0u;
    g_linux_exec64_telemetry.page_fault_rip = interrupts64_last_exception_rip();

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_CLEANUP;
    exit_probe_clear = syscall64_native_clear_linux_exit_probe(pid);
    scheduler64_runqueue_stop();
    scheduler64_runqueue_reset();
    nvme_vfs_release = linux_vfs64_release_nvme_read(pid);
    if (linux_abi64_last_exit_pid() == pid)
    {
        if (linux_abi64_last_exit_detached_vma() != 0)
        {
            vma_ctx = linux_abi64_last_exit_detached_vma();
        }
        if (linux_abi64_last_exit_detached_fd() != 0)
        {
            fd_ctx = linux_abi64_last_exit_detached_fd();
        }
        if (linux_abi64_last_exit_detached_audit() != 0)
        {
            audit_ctx = linux_abi64_last_exit_detached_audit();
        }
    }

    reattach_vma =
        ((vma_ctx != 0) && (process64_vma_root(pid) == 0))
            ? process64_attach_vma(pid, vma_ctx)
            : 0u;
    reattach_fd =
        ((fd_ctx != 0) && (process64_fd_table(pid) == 0))
            ? process64_attach_fd(pid, fd_ctx)
            : 0u;
    reattach_audit =
        ((audit_ctx != 0) && (process64_audit_ctx(pid) == 0))
            ? process64_attach_audit(pid, audit_ctx)
            : 0u;
    if ((reattach_audit != 0u) || (process64_audit_ctx(pid) != 0))
    {
        g_linux_exec64_telemetry.syscall_last = persona_audit64_last_event_code(pid);
        g_linux_exec64_telemetry.syscall_last_result = persona_audit64_last_result(pid);
        if (dynamic_launch != 0u)
        {
            g_linux_exec64_telemetry.dynamic_first_syscall =
                g_linux_exec64_telemetry.syscall_last;
        }
    }

    g_linux_exec64_telemetry.exit_code =
        (linux_abi64_process_exited(pid) != 0u)
            ? linux_abi64_exit_code(pid)
            : 0xFFFFFFFFu;
    g_linux_exec64_telemetry.dynamic_exit_code =
        (dynamic_launch != 0u) ? g_linux_exec64_telemetry.exit_code : 0u;
    if (linux_abi64_last_exit_pid() == pid)
    {
        exit_vma_release = linux_abi64_last_exit_vma_regions();
        exit_audit_release = linux_abi64_last_exit_audit_released();
    }
    vma_release = ((reattach_vma != 0u) || (process64_vma_root(pid) != 0))
        ? vma64_release_process(pid)
        : 0u;
    process_root_token = process64_page_root_token(pid);
    if (process_root_token != 0u)
    {
        root_release = paging64_process_root_release(pid, process_root_token);
        root_clear = process64_clear_page_root(pid, process_root_token);
    }
    g_linux_exec64_telemetry.root_cleanup =
        (((root_release != 0u)
            && (root_clear != 0u)
            && (paging64_process_root_physical(pid) == 0ull)
            && (process64_page_root_token(pid) == 0u))
            ? 1u
            : 0u)
        + g_linux_exec64_telemetry.child_root_cleanup;
    g_linux_exec64_telemetry.pml4_pool_used_final = paging64_process_root_pool_used();
    if (dynamic_launch != 0u)
    {
        g_linux_exec64_telemetry.dynamic_map_cleanup =
            (((vma_release + exit_vma_release)
                    >= (g_linux_exec64_telemetry.dynamic_app_mapped
                        + g_linux_exec64_telemetry.dynamic_interp_mapped
                        + 1u))
                && (g_linux_exec64_telemetry.root_cleanup != 0u)
                && (g_linux_exec64_telemetry.pml4_pool_used_final == 0u))
                ? 1u
                : 0u;
    }
    if ((reattach_fd != 0u) || (process64_fd_table(pid) != 0))
    {
        (void)fd64_release_process(pid);
    }
    fd_cleanup =
        (((linux_abi64_last_exit_pid() != pid)
            || (linux_abi64_last_exit_detached_fd() == 0)
            || (reattach_fd != 0u))
            && (process64_fd_table(pid) == 0))
            ? 1u
            : 0u;
    if (process64_persona_ctx(pid) != 0)
    {
        (void)persona64_release(pid);
    }
    audit_release = ((reattach_audit != 0u) || (process64_audit_ctx(pid) != 0))
        ? persona_audit64_release(pid)
        : 0u;
    clone_release = process64_release_clone(pid);
    g_linux_exec64_telemetry.nvme_vfs_release = nvme_vfs_release;

    g_linux_exec64_telemetry.cleanup =
        ((exit_probe_clear != 0u)
            && ((g_linux_exec64_telemetry.nvme_vfs_bind == 0u) || (nvme_vfs_release != 0u))
            && ((vma_release + exit_vma_release)
                >= (g_linux_exec64_telemetry.mapped_regions + 1u))
            && (g_linux_exec64_telemetry.root_cleanup != 0u)
            && (fd_cleanup != 0u)
            && ((audit_release + exit_audit_release) != 0u)
            && (clone_release != 0u))
            ? 1u
            : 0u;
    if ((transfer_result != LINUX_EXEC64_EXIT_PROBE_RESULT)
        || (g_linux_exec64_telemetry.exit_code != 0u)
        || (g_linux_exec64_telemetry.cleanup == 0u))
    {
        g_linux_exec64_telemetry.failure_code = transfer_result;
        linux_exec64_emit_summary(console_capability, owner_id, path, path_bytes);
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }

    linux_exec64_emit_summary(console_capability, owner_id, path, path_bytes);
    return LINUX_EXEC64_RESULT_OK;
}

u32 linux_exec64_run_nvme(
    const u8 *path,
    u32 path_bytes,
    const char *const *argv,
    u32 argc,
    u32 owner_id,
    u32 nvme_fs_capability,
    u32 console_capability)
{
    return linux_exec64_run_source(
        path,
        path_bytes,
        argv,
        argc,
        owner_id,
        nvme_fs_capability,
        console_capability,
        LINUX_EXEC64_SOURCE_NVME);
}

u32 linux_exec64_run_boot_media(
    const u8 *path,
    u32 path_bytes,
    const char *const *argv,
    u32 argc,
    u32 owner_id,
    u32 nvme_fs_capability,
    u32 console_capability)
{
    return linux_exec64_run_source(
        path,
        path_bytes,
        argv,
        argc,
        owner_id,
        nvme_fs_capability,
        console_capability,
        LINUX_EXEC64_SOURCE_BOOT_MEDIA);
}

#endif
