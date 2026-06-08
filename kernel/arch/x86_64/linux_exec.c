#include "linux_exec_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

#include "capability_x64.h"
#include "console_x64.h"
#include "elf64_x64.h"
#include "fd_x64.h"
#include "interrupts_x64.h"
#include "linux_abi_x64.h"
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
    LINUX_EXEC64_STAGE_CLEANUP = 10u
} linux_exec64_stage_t;

typedef struct linux_exec64_telemetry
{
    linux_exec64_stage_t stage;
    u32 nvme_read;
    u32 elf;
    u32 static_elf;
    u32 mapped_pages;
    u32 mapped_regions;
    u32 stack_pages;
    u32 pml4;
    u32 pml4_pool;
    u32 pml4_slot;
    u64 root_physical;
    u64 kernel_root_physical;
    u32 root_distinct;
    u32 high_copy;
    u32 mmio_shared;
    u32 pool_mapped;
    u32 low_compat;
    u32 kernel_cr3_entry;
    u32 syscall_root_repair;
    u32 syscall_root_reload;
    u32 syscall_root_denial;
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
    u32 path_fault;
    u32 chdir_calls;
    u32 fchdir_calls;
    u32 chdir_denial;
    u32 chdir_fault;
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
    default:
        return "none";
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
    (void)linux_exec64_write_text(console_capability, owner_id, " provenance 1 nvme-read ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.nvme_read);
    (void)linux_exec64_write_text(console_capability, owner_id, " elf ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.elf);
    (void)linux_exec64_write_text(console_capability, owner_id, " static ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.static_elf);
    (void)linux_exec64_write_text(console_capability, owner_id, " mapped ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mapped_regions);
    (void)linux_exec64_write_text(console_capability, owner_id, " pages ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.mapped_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " stack ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.stack_pages);
    (void)linux_exec64_write_text(console_capability, owner_id, " pml4 ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pml4);
    (void)linux_exec64_write_text(console_capability, owner_id, " pml4-pool ");
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
    (void)linux_exec64_write_text(console_capability, owner_id, " kernel-cr3-entry ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.kernel_cr3_entry);
    (void)linux_exec64_write_text(console_capability, owner_id, " syscall-root-repair ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_root_repair);
    (void)linux_exec64_write_text(console_capability, owner_id, " syscall-root-reload ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_root_reload);
    (void)linux_exec64_write_text(console_capability, owner_id, " syscall-root-denial ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.syscall_root_denial);
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
    (void)linux_exec64_write_text(console_capability, owner_id, " console-bytes ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.console_bytes);
    (void)linux_exec64_write_text(console_capability, owner_id, " exit ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.exit_code);
    (void)linux_exec64_write_text(console_capability, owner_id, " cleanup ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.cleanup);
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
    (void)linux_exec64_write_text(console_capability, owner_id, " stage ");
    (void)linux_exec64_write_text(
        console_capability,
        owner_id,
        linux_exec64_stage_name(g_linux_exec64_telemetry.stage));
    (void)linux_exec64_write_text(console_capability, owner_id, " code ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.failure_code);
    (void)linux_exec64_write_text(console_capability, owner_id, " pid ");
    linux_exec64_write_dec_u32(console_capability, owner_id, g_linux_exec64_telemetry.pid);
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

u32 linux_exec64_run_nvme(
    const u8 *path,
    u32 path_bytes,
    const char *const *argv,
    u32 argc,
    u32 owner_id,
    u32 nvme_fs_capability,
    u32 console_capability)
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
    u32 launch_result;
    u32 task;
    u32 runqueue_started;
    u32 transfer_result;
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
    u32 read_calls_before;
    u32 read_calls_after;
    u32 read_bytes_before;
    u32 read_bytes_after;
    u32 write_calls_before;
    u32 write_calls_after;
    u32 write_bytes_before;
    u32 write_bytes_after;
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
    u32 fork_before;
    u32 fork_after;
    u32 fork_success_before;
    u32 fork_success_after;
    u32 fork_enosys_before;
    u32 fork_enosys_after;
    u32 fork_denial_before;
    u32 fork_denial_after;
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
    u32 exit_probe_arm;
    u32 exit_probe_clear = 0u;
    u32 reattach_vma = 0u;
    u32 reattach_fd = 0u;
    u32 reattach_audit = 0u;
    u32 vma_release = 0u;
    u32 fd_release = 0u;
    u32 audit_release = 0u;
    u32 exit_vma_release = 0u;
    u32 exit_fd_release = 0u;
    u32 exit_audit_release = 0u;
    u32 clone_release = 0u;
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
    g_linux_exec64_telemetry.task = SCHEDULER64_INVALID_TASK;
    g_linux_exec64_telemetry.pml4_slot = 0xFFFFFFFFu;
    g_linux_exec64_telemetry.fork_child_slot = 0xFFFFFFFFu;
    g_linux_exec64_telemetry.pid = PROCESS64_INVALID_PID;
    g_linux_exec64_telemetry.syscall_last = 0xFFFFFFFFu;
    g_linux_exec64_telemetry.syscall_last_result = 0u;
    g_linux_exec64_telemetry.low_kernel_vma_limit = LINUX_EXEC64_LOW_KERNEL_VMA_LIMIT;
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

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_CAPABILITY;
    if ((nvme_fs_capability == CAPABILITY64_INVALID_HANDLE)
        || (nvme_fs_capability != mmio64_nvme_rw_capability())
        || (console_capability == CAPABILITY64_INVALID_HANDLE)
        || (capability64_route(console_capability, CAPABILITY64_RIGHT_SEND, owner_id)
            != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE)))
    {
        g_linux_exec64_telemetry.failure_code = 1u;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_READ;
    if (mmio64_nvme_fat_shell_read_file(
            path,
            path_bytes,
            g_linux_exec64_binary,
            LINUX_EXEC64_STAGING_BUFFER_BYTES,
            owner_id,
            &bytes_read) == 0u)
    {
        g_linux_exec64_telemetry.nvme_read_error = mmio64_nvme_fat_shell_read_last_error();
        g_linux_exec64_telemetry.nvme_read_bytes = mmio64_nvme_fat_shell_read_last_bytes();
        g_linux_exec64_telemetry.nvme_read_capacity = mmio64_nvme_fat_shell_read_last_capacity();
        g_linux_exec64_telemetry.nvme_read_size = mmio64_nvme_fat_shell_read_last_size();
        g_linux_exec64_telemetry.nvme_read_attr = mmio64_nvme_fat_shell_read_last_attr();
        g_linux_exec64_telemetry.failure_code = g_linux_exec64_telemetry.nvme_read_error;
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    g_linux_exec64_telemetry.nvme_read_error = mmio64_nvme_fat_shell_read_last_error();
    g_linux_exec64_telemetry.nvme_read_bytes = mmio64_nvme_fat_shell_read_last_bytes();
    g_linux_exec64_telemetry.nvme_read_capacity = mmio64_nvme_fat_shell_read_last_capacity();
    g_linux_exec64_telemetry.nvme_read_size = mmio64_nvme_fat_shell_read_last_size();
    g_linux_exec64_telemetry.nvme_read_attr = mmio64_nvme_fat_shell_read_last_attr();
    g_linux_exec64_telemetry.nvme_read = 1u;

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
    g_linux_exec64_telemetry.elf = 1u;

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
    pid = (parent_pid != PROCESS64_INVALID_PID) ? process64_spawn_clone(parent_pid) : PROCESS64_INVALID_PID;
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
        root_authority = nvme_fs_capability;
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
    g_linux_exec64_telemetry.user_pdpt_private =
        paging64_process_root_last_user_pdpt_private();

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

    if ((vma64_init_process(pid) == 0u)
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
        || (persona64_init_linux_elf(pid, linux_abi64_dispatch_table()) != PERSONA64_ATTACH_OK)
        || (linux_vfs64_bind_nvme_read(pid, owner_id, nvme_fs_capability) == 0u))
    {
        g_linux_exec64_telemetry.failure_code = 4u;
        linux_exec64_release_failed_process(pid);
        linux_exec64_emit_failure(console_capability, owner_id, path, path_bytes);
        return LINUX_EXEC64_RESULT_FAILED;
    }
    g_linux_exec64_telemetry.nvme_vfs_bind = 1u;

    g_linux_exec64_telemetry.stage = LINUX_EXEC64_STAGE_LAUNCH;
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
        argv,
        0u,
        0,
        0u,
        &g_linux_exec64_launch);
    load_cr3_restore = paging64_switch_to_kernel_root(0x4C4F414Bu);
    g_linux_exec64_telemetry.mapped_regions = g_linux_exec64_launch.load_result.mapped_count;
    g_linux_exec64_telemetry.mapped_pages =
        (u32)(g_linux_exec64_launch.load_result.total_map_bytes / VMA64_PAGE_BYTES);
    g_linux_exec64_telemetry.stack_pages = LINUX_EXEC64_REAL_STACK_BYTES / VMA64_PAGE_BYTES;
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
    path_fault_before = linux_abi64_path_fault_count();
    chdir_before = linux_abi64_chdir_count();
    fchdir_before = linux_abi64_fchdir_count();
    chdir_denial_before = linux_abi64_chdir_denial_count();
    chdir_fault_before = linux_abi64_chdir_fault_count();
    read_calls_before = linux_abi64_read_count();
    read_bytes_before = linux_abi64_read_byte_count();
    write_calls_before = linux_abi64_write_count();
    write_bytes_before = linux_abi64_write_byte_count();
    pipe_calls_before = linux_abi64_pipe_count();
    pipe_denial_before = linux_abi64_pipe_denial_count();
    pipe_fault_before = linux_abi64_pipe_fault_count();
    pipe2_calls_before = linux_abi64_pipe2_count();
    pipe2_denial_before = linux_abi64_pipe2_denial_count();
    pipe2_fault_before = linux_abi64_pipe2_fault_count();
    pipe_blocks_before = pipe64_block_count();
    pipe_wakes_before = pipe64_wake_count();
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
    fork_before = linux_abi64_fork_count();
    fork_success_before = linux_abi64_fork_success_count();
    fork_enosys_before = linux_abi64_fork_enosys_count();
    fork_denial_before = linux_abi64_fork_denial_count();
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
    cr3_process_switch_before = paging64_process_root_switch_count();
    cr3_kernel_switch_before = paging64_process_root_kernel_switch_count();
    runqueue_started = scheduler64_runqueue_start(task);
    cr3_process_switch_after = paging64_process_root_switch_count();
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
    path_fault_after = linux_abi64_path_fault_count();
    chdir_after = linux_abi64_chdir_count();
    fchdir_after = linux_abi64_fchdir_count();
    chdir_denial_after = linux_abi64_chdir_denial_count();
    chdir_fault_after = linux_abi64_chdir_fault_count();
    read_calls_after = linux_abi64_read_count();
    read_bytes_after = linux_abi64_read_byte_count();
    write_calls_after = linux_abi64_write_count();
    write_bytes_after = linux_abi64_write_byte_count();
    pipe_calls_after = linux_abi64_pipe_count();
    pipe_denial_after = linux_abi64_pipe_denial_count();
    pipe_fault_after = linux_abi64_pipe_fault_count();
    pipe2_calls_after = linux_abi64_pipe2_count();
    pipe2_denial_after = linux_abi64_pipe2_denial_count();
    pipe2_fault_after = linux_abi64_pipe2_fault_count();
    pipe_blocks_after = pipe64_block_count();
    pipe_wakes_after = pipe64_wake_count();
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
    fork_after = linux_abi64_fork_count();
    fork_success_after = linux_abi64_fork_success_count();
    fork_enosys_after = linux_abi64_fork_enosys_count();
    fork_denial_after = linux_abi64_fork_denial_count();
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

    g_linux_exec64_telemetry.started = (runqueue_started != 0u) ? 1u : 0u;
    g_linux_exec64_telemetry.console_bytes =
        (console_bytes_after >= console_bytes_before)
            ? (console_bytes_after - console_bytes_before)
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
    }

    g_linux_exec64_telemetry.exit_code =
        (linux_abi64_process_exited(pid) != 0u)
            ? linux_abi64_exit_code(pid)
            : 0xFFFFFFFFu;
    if (linux_abi64_last_exit_pid() == pid)
    {
        exit_vma_release = linux_abi64_last_exit_vma_regions();
        exit_fd_release = linux_abi64_last_exit_fd_entries();
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
    fd_release = ((reattach_fd != 0u) || (process64_fd_table(pid) != 0))
        ? fd64_release_process(pid)
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
            && (nvme_vfs_release != 0u)
            && ((vma_release + exit_vma_release)
                >= (g_linux_exec64_telemetry.mapped_regions + 1u))
            && (g_linux_exec64_telemetry.root_cleanup != 0u)
            && ((fd_release + exit_fd_release) >= 3u)
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

#endif
