#include "linux_abi_x64.h"

#include "elf64_x64.h"
#include "fd_x64.h"
#include "input_x64.h"
#include "interrupts_x64.h"
#include "linux_exec_x64.h"
#include "linux_vfs_x64.h"
#include "paging_x64.h"
#include "persona_x64.h"
#include "persona_audit_x64.h"
#include "pit.h"
#include "pipe_x64.h"
#include "process_x64.h"
#include "scheduler_x64.h"
#include "vma_x64.h"
#include "x64.h"

/*
 * F.0-F.16 add the first Linux ABI wrappers and dispatch table. The code
 * integrates with vma_x64.h for brk and readable user-buffer validation,
 * fd_x64.h and linux_vfs_x64.h for brokered descriptor/path I/O,
 * input_x64.h for brokered stdin, persona_x64.h for LINUX_ELF binding, and
 * persona_audit_x64.h so translated and denied Linux syscalls are visible.
 * The scaffold checkpoint proves brk, table geometry, audited ENOSYS denial,
 * sys_write through fd/console, sys_read through fd/input,
 * sys_exit/sys_exit_group teardown of attached VMA, fd, persona, and audit
 * state, anonymous mmap/munmap/mprotect through the VMA subsystem, and
 * sys_open/sys_openat through the Linux VFS path namespace, sys_close through
 * fd_x64 descriptor revocation, sys_lseek through fd_x64 file offsets, and
 * stat/fstat/newfstatat through fd_x64 metadata copied into a Linux stat frame.
 * F.13 adds PID/TID identity queries using process_x64.h accessors only,
 * F.14 adds arch_prctl FS-base TLS setup through persona_x64.h and x64.h,
 * F.15 records clear_child_tid in the Linux persona context, and F.16 maps
 * clock_gettime to the kernel PIT timekeeping primitive and writes a Linux
 * timespec to a verified user VMA. F.17 validates Linux nanosleep request
 * frames and routes the timed wait through scheduler_x64.h. F.18 adds
 * budget-capped getrlimit/setrlimit state for Linux persona processes. F.19
 * adds pipe2 by translating Linux flags into fd_x64 flags and creating a
 * brokered pipe_x64 read/write endpoint pair. F.20 wires dup/dup2/dup3 to
 * the fd_x64 duplication primitives and audits both translated duplication
 * and truthful EBADF/EINVAL denials. F.21 adds fcntl command translation for
 * F_DUPFD/F_GETFD/F_SETFD/F_GETFL/F_SETFL using fd_x64 helpers so fd table
 * internals remain private to the fd subsystem. F.22 returns the Linux
 * persona cwd from persona_x64 process state through a verified user buffer.
 * F.23 updates that cwd through chdir/fchdir after validating VFS-backed
     * directory paths and fd-owned path metadata. F.24 adds getdents64 by
     * formatting Linux dirent64 records from the Linux VFS directory enumerator.
     * F.25 adds the first futex WAIT/WAKE substrate: a bounded kernel wait
     * table keyed by Linux persona PID and user futex word, with VMA-validated
     * user reads, scheduler-backed BLOCKED/READY transitions, audited
     * EAGAIN/EFAULT/EINVAL denials, wake consumption telemetry, and a timed
     * WAIT path that binds a scheduler sleep expiry back to the futex waiter
     * so timeout cleanup and ETIMEDOUT resume state are not fabricated. F.26 adds
     * thread-style clone(2) for shared VM/FD/persona
     * state, routes the child into scheduler_x64.h, and truthfully returns
     * ENOSYS for fork-style clone requests until per-process page tables exist.
     * F.27 adds execve/execveat by copying path/argv/envp before teardown,
     * reading the new image through linux_vfs_x64.h and fd_x64.h, validating a
     * static ELF through elf64_x64.h, resetting process VMA state through
     * vma_x64.h, preserving non-CLOEXEC descriptors, closing CLOEXEC
     * descriptors, and exposing the prepared ring-3 transfer frame to the
     * syscall-return reframe path and deterministic telemetry.
     * F.28 adds wait4 over the current clone-child substrate: exited child
     * records remain waitable zombies until the parent collects their encoded
     * status, with ECHILD/EFAULT/ENOSYS denials audited instead of fabricated
     * blocking or resource-usage success.
     * F.29 installs truthful kill/tkill syscall stubs: until Phase I signal
     * delivery exists, valid requests return audited ENOSYS and invalid signal
     * numbers return audited EINVAL without mutating process state.
     * F.30 adds getrandom by filling verified Linux persona user buffers from
     * a kernel-owned runtime entropy mixer, auditing real byte production and
     * truthful EINVAL/EFAULT denials. F.31 adds pread64/pwrite64 through
     * fd_x64 positional helpers, proving offset-specific RAMFS I/O without
     * mutating the descriptor's current file offset. F.32 adds bounded
     * readv/writev scatter-gather translation over fd_x64, validating Linux
     * iovec frames and user buffers before routing the operation through the
     * existing brokered descriptor path. F.33 adds synchronous poll/ppoll
     * readiness over the fd_x64 descriptor table and pipe_x64 endpoint state,
     * returning truthful immediate readiness only while leaving blocking and
     * signal-mask behavior for later scheduler/signal phases. I.2 wires
     * rt_sigaction(2) to the Linux persona signal table so handlers can be
     * installed and queried before later phases add signal injection. I.3 adds
     * rt_sigprocmask(2) over the same persona state, preserving unmaskable
     * SIGKILL/SIGSTOP semantics and auditing both mask changes and denials.
     * I.4 turns kill/tkill into pending-signal mutation and adds a return-frame
     * injector that writes a compact siginfo/ucontext frame onto a verified
     * user stack before redirecting RIP to the installed handler. I.5 adds
     * rt_sigreturn(2): it reads that verified frame back from the handler stack,
     * restores the interrupted register frame and previous signal mask, and
     * audits malformed or inaccessible return frames instead of resuming them.
     */

#define LINUX_ABI64_MAX_EXIT_RECORDS 16u
#define LINUX_ABI64_MAX_RLIMIT_RECORDS 16u
#define LINUX_ABI64_MAX_FUTEX_WAITERS 4u
#define LINUX_ABI64_MAX_CLONE_RECORDS 4u
#define LINUX_ABI64_USER_CANONICAL_LIMIT 0x0000800000000000ull
#define LINUX_ABI64_DEFAULT_TICK_HZ 100u
#define LINUX_ABI64_MAX_PIT_TICK_HZ 1193182u
typedef struct linux_abi64_exit_record
{
    u32 pid;
    u32 exit_code;
    u32 exited;
    u32 reserved;
} linux_abi64_exit_record_t;

typedef struct linux_abi64_rlimit_record
{
    u32 pid;
    u32 initialized;
    u64 current[LINUX_ABI64_RLIMIT_COUNT];
    u64 maximum[LINUX_ABI64_RLIMIT_COUNT];
} linux_abi64_rlimit_record_t;

typedef struct linux_abi64_futex_waiter
{
    u32 active;
    u32 pid;
    u32 task_id;
    u32 timed;
    u32 expected_value;
    u32 timeout_ticks;
    u32 timeout_result;
    u32 reserved;
    u64 user_address;
    u64 rip;
} linux_abi64_futex_waiter_t;

typedef struct linux_abi64_clone_record
{
    u32 active;
    u32 parent_pid;
    u32 child_pid;
    u32 flags;
    u32 task_id;
    u32 shared_vma;
    u32 shared_fd;
    u32 shared_audit;
    u32 wait_blocked;
    u32 wait_task_id;
    u64 child_stack;
    u64 tls_base;
    u64 wait_status_user;
    u64 wait_rip;
} linux_abi64_clone_record_t;

typedef struct linux_abi64_exec_validation
{
    elf64_header_t header;
    elf64_phdr_summary_t summary;
    u32 ok;
    u32 error;
} linux_abi64_exec_validation_t;

static u32 g_linux_abi64_initialized = 0u;
static linux_abi64_handler_t g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_LIMIT];
static linux_abi64_exit_record_t g_linux_abi64_exit_records[LINUX_ABI64_MAX_EXIT_RECORDS];
static linux_abi64_rlimit_record_t g_linux_abi64_rlimit_records[LINUX_ABI64_MAX_RLIMIT_RECORDS];
static linux_abi64_futex_waiter_t g_linux_abi64_futex_waiters[LINUX_ABI64_MAX_FUTEX_WAITERS];
static linux_abi64_clone_record_t g_linux_abi64_clone_records[LINUX_ABI64_MAX_CLONE_RECORDS];
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define LINUX_ABI64_EXEC_STAGING_BYTES LINUX_EXEC64_REAL_BINARY_MAX_BYTES
#else
#define LINUX_ABI64_EXEC_STAGING_BYTES LINUX_ABI64_EXEC_BINARY_MAX_BYTES
#endif

static u8 g_linux_abi64_exec_binary[LINUX_ABI64_EXEC_STAGING_BYTES];
static char g_linux_abi64_exec_path[LINUX_VFS64_MAX_PATH_BYTES + 1u];
static char g_linux_abi64_exec_argv_storage[LINUX_ABI64_EXEC_ARG_MAX][ELF64_STACK_MAX_STRING_BYTES];
static char g_linux_abi64_exec_envp_storage[LINUX_ABI64_EXEC_ENV_MAX][ELF64_STACK_MAX_STRING_BYTES];
static const char *g_linux_abi64_exec_argv[LINUX_ABI64_EXEC_ARG_MAX];
static const char *g_linux_abi64_exec_envp[LINUX_ABI64_EXEC_ENV_MAX];
static elf64_launch_result_t g_linux_abi64_exec_launch;
static u32 g_linux_abi64_dispatch_count = 0u;
static u32 g_linux_abi64_dispatch_root_repair_count = 0u;
static u32 g_linux_abi64_dispatch_root_reload_count = 0u;
static u32 g_linux_abi64_dispatch_root_denial_count = 0u;
static u32 g_linux_abi64_unimplemented_count = 0u;
static u32 g_linux_abi64_unimplemented_last_syscall = 0u;
static u64 g_linux_abi64_unimplemented_last_rip = 0ull;
static u32 g_linux_abi64_read_count = 0u;
static u32 g_linux_abi64_read_byte_count = 0u;
static u32 g_linux_abi64_read_denial_count = 0u;
static u32 g_linux_abi64_read_fault_count = 0u;
static u32 g_linux_abi64_write_count = 0u;
static u32 g_linux_abi64_write_byte_count = 0u;
static u32 g_linux_abi64_write_denial_count = 0u;
static u32 g_linux_abi64_write_fault_count = 0u;
static u32 g_linux_abi64_pread64_count = 0u;
static u32 g_linux_abi64_pwrite64_count = 0u;
static u32 g_linux_abi64_pread64_byte_count = 0u;
static u32 g_linux_abi64_pwrite64_byte_count = 0u;
static u32 g_linux_abi64_positioned_denial_count = 0u;
static u32 g_linux_abi64_positioned_fault_count = 0u;
static u32 g_linux_abi64_positioned_last_syscall = 0u;
static u32 g_linux_abi64_positioned_last_fd = FD64_INVALID_FD;
static u32 g_linux_abi64_positioned_last_byte_count = 0u;
static u64 g_linux_abi64_positioned_last_offset = 0ull;
static u32 g_linux_abi64_positioned_last_result = 0u;
static u32 g_linux_abi64_readv_count = 0u;
static u32 g_linux_abi64_writev_count = 0u;
static u32 g_linux_abi64_readv_byte_count = 0u;
static u32 g_linux_abi64_writev_byte_count = 0u;
static u32 g_linux_abi64_vector_denial_count = 0u;
static u32 g_linux_abi64_vector_fault_count = 0u;
static u32 g_linux_abi64_vector_last_syscall = 0u;
static u32 g_linux_abi64_vector_last_fd = FD64_INVALID_FD;
static u32 g_linux_abi64_vector_last_iov_count = 0u;
static u32 g_linux_abi64_vector_last_byte_count = 0u;
static u32 g_linux_abi64_vector_last_result = 0u;
static u32 g_linux_abi64_poll_count = 0u;
static u32 g_linux_abi64_ppoll_count = 0u;
static u32 g_linux_abi64_poll_ready_count = 0u;
static u32 g_linux_abi64_poll_denial_count = 0u;
static u32 g_linux_abi64_poll_fault_count = 0u;
static u32 g_linux_abi64_poll_last_syscall = 0u;
static u32 g_linux_abi64_poll_last_fd_count = 0u;
static u32 g_linux_abi64_poll_last_ready = 0u;
static u32 g_linux_abi64_poll_last_revents = 0u;
static u32 g_linux_abi64_poll_last_result = 0u;
static u32 g_linux_abi64_open_count = 0u;
static u32 g_linux_abi64_openat_count = 0u;
static u32 g_linux_abi64_open_denial_count = 0u;
static u32 g_linux_abi64_close_count = 0u;
static u32 g_linux_abi64_close_denial_count = 0u;
static u32 g_linux_abi64_lseek_count = 0u;
static u32 g_linux_abi64_lseek_denial_count = 0u;
static u32 g_linux_abi64_stat_count = 0u;
static u32 g_linux_abi64_stat_denial_count = 0u;
static u32 g_linux_abi64_stat_fault_count = 0u;
static u32 g_linux_abi64_fstat_count = 0u;
static u32 g_linux_abi64_fstat_denial_count = 0u;
static u32 g_linux_abi64_fstat_fault_count = 0u;
static u32 g_linux_abi64_newfstatat_count = 0u;
static u32 g_linux_abi64_newfstatat_denial_count = 0u;
static u32 g_linux_abi64_newfstatat_fault_count = 0u;
static u32 g_linux_abi64_readlink_count = 0u;
static u32 g_linux_abi64_readlink_byte_count = 0u;
static u32 g_linux_abi64_readlink_denial_count = 0u;
static u32 g_linux_abi64_readlink_fault_count = 0u;
static u32 g_linux_abi64_readlink_last_result = 0u;
static u32 g_linux_abi64_mmap_count = 0u;
static u32 g_linux_abi64_mmap_byte_count = 0u;
static u32 g_linux_abi64_mmap_denial_count = 0u;
static u32 g_linux_abi64_mprotect_count = 0u;
static u32 g_linux_abi64_mprotect_byte_count = 0u;
static u32 g_linux_abi64_mprotect_denial_count = 0u;
static u32 g_linux_abi64_munmap_count = 0u;
static u32 g_linux_abi64_munmap_byte_count = 0u;
static u32 g_linux_abi64_munmap_denial_count = 0u;
static u32 g_linux_abi64_brk_query_count = 0u;
static u32 g_linux_abi64_brk_extend_count = 0u;
static u32 g_linux_abi64_brk_denial_count = 0u;
static u32 g_linux_abi64_rt_sigaction_count = 0u;
static u32 g_linux_abi64_rt_sigaction_query_count = 0u;
static u32 g_linux_abi64_rt_sigaction_denial_count = 0u;
static u32 g_linux_abi64_rt_sigaction_fault_count = 0u;
static u32 g_linux_abi64_rt_sigaction_last_signal = 0u;
static u64 g_linux_abi64_rt_sigaction_last_handler = 0ull;
static u64 g_linux_abi64_rt_sigaction_last_old_handler = 0ull;
static u64 g_linux_abi64_rt_sigaction_last_mask = 0ull;
static u64 g_linux_abi64_rt_sigaction_last_flags = 0ull;
static u32 g_linux_abi64_rt_sigaction_last_result = 0u;
static u32 g_linux_abi64_rt_sigprocmask_count = 0u;
static u32 g_linux_abi64_rt_sigprocmask_query_count = 0u;
static u32 g_linux_abi64_rt_sigprocmask_denial_count = 0u;
static u32 g_linux_abi64_rt_sigprocmask_fault_count = 0u;
static u32 g_linux_abi64_rt_sigprocmask_last_how = 0u;
static u64 g_linux_abi64_rt_sigprocmask_last_set = 0ull;
static u64 g_linux_abi64_rt_sigprocmask_last_old_mask = 0ull;
static u64 g_linux_abi64_rt_sigprocmask_last_mask = 0ull;
static u32 g_linux_abi64_rt_sigprocmask_last_result = 0u;
static u32 g_linux_abi64_nanosleep_count = 0u;
static u32 g_linux_abi64_nanosleep_denial_count = 0u;
static u32 g_linux_abi64_nanosleep_fault_count = 0u;
static u32 g_linux_abi64_nanosleep_interrupted_count = 0u;
static u32 g_linux_abi64_getrlimit_count = 0u;
static u32 g_linux_abi64_setrlimit_count = 0u;
static u32 g_linux_abi64_rlimit_denial_count = 0u;
static u32 g_linux_abi64_rlimit_fault_count = 0u;
static u32 g_linux_abi64_pipe_count = 0u;
static u32 g_linux_abi64_pipe_denial_count = 0u;
static u32 g_linux_abi64_pipe_fault_count = 0u;
static u32 g_linux_abi64_pipe2_count = 0u;
static u32 g_linux_abi64_pipe2_denial_count = 0u;
static u32 g_linux_abi64_pipe2_fault_count = 0u;
static u32 g_linux_abi64_dup_count = 0u;
static u32 g_linux_abi64_dup2_count = 0u;
static u32 g_linux_abi64_dup3_count = 0u;
static u32 g_linux_abi64_dup_denial_count = 0u;
static u32 g_linux_abi64_fcntl_count = 0u;
static u32 g_linux_abi64_fcntl_denial_count = 0u;
static u32 g_linux_abi64_getcwd_count = 0u;
static u32 g_linux_abi64_getcwd_byte_count = 0u;
static u32 g_linux_abi64_getcwd_denial_count = 0u;
static u32 g_linux_abi64_getcwd_fault_count = 0u;
static u32 g_linux_abi64_path_relative_count = 0u;
static u32 g_linux_abi64_path_dot_count = 0u;
static u32 g_linux_abi64_path_dotdot_count = 0u;
static u32 g_linux_abi64_path_trailing_count = 0u;
static u32 g_linux_abi64_path_trailing_denial_count = 0u;
static u32 g_linux_abi64_path_fault_count = 0u;
static u32 g_linux_abi64_chdir_count = 0u;
static u32 g_linux_abi64_fchdir_count = 0u;
static u32 g_linux_abi64_chdir_denial_count = 0u;
static u32 g_linux_abi64_chdir_fault_count = 0u;
static u32 g_linux_abi64_getdents64_count = 0u;
static u32 g_linux_abi64_getdents64_entry_count = 0u;
static u32 g_linux_abi64_getdents64_byte_count = 0u;
static u32 g_linux_abi64_getdents64_denial_count = 0u;
static u32 g_linux_abi64_getdents64_fault_count = 0u;
static u32 g_linux_abi64_futex_wait_count = 0u;
static u32 g_linux_abi64_futex_wake_count = 0u;
static u32 g_linux_abi64_futex_woken_count = 0u;
static u32 g_linux_abi64_futex_eagain_count = 0u;
static u32 g_linux_abi64_futex_denial_count = 0u;
static u32 g_linux_abi64_futex_fault_count = 0u;
static u32 g_linux_abi64_futex_timed_wait_count = 0u;
static u32 g_linux_abi64_futex_timeout_count = 0u;
static u32 g_linux_abi64_futex_last_wait_pid = PROCESS64_INVALID_PID;
static u64 g_linux_abi64_futex_last_wait_address = 0ull;
static u32 g_linux_abi64_futex_last_wait_value = 0u;
static u32 g_linux_abi64_futex_last_wait_task_id = SCHEDULER64_INVALID_TASK;
static u32 g_linux_abi64_futex_last_wake_count = 0u;
static u32 g_linux_abi64_futex_last_timeout_task_id = SCHEDULER64_INVALID_TASK;
static u32 g_linux_abi64_futex_last_timeout_ticks = 0u;
static u32 g_linux_abi64_futex_last_timeout_result = 0u;
static u32 g_linux_abi64_clone_count = 0u;
static u32 g_linux_abi64_clone_thread_count = 0u;
static u32 g_linux_abi64_clone_denial_count = 0u;
static u32 g_linux_abi64_clone_fork_denial_count = 0u;
static u32 g_linux_abi64_clone_scheduler_count = 0u;
static u32 g_linux_abi64_fork_count = 0u;
static u32 g_linux_abi64_fork_success_count = 0u;
static u32 g_linux_abi64_fork_enosys_count = 0u;
static u32 g_linux_abi64_fork_denial_count = 0u;
static u64 g_linux_abi64_fork_last_rip = 0ull;
static u32 g_linux_abi64_fork_last_child_pid = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_fork_last_child_slot = 0xFFFFFFFFu;
static u32 g_linux_abi64_fork_last_child_root_distinct = 0u;
static u32 g_linux_abi64_fork_last_task_id = SCHEDULER64_INVALID_TASK;
static u32 g_linux_abi64_child_root_cleanup_count = 0u;
static u32 g_linux_abi64_clone_last_parent_pid = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_clone_last_child_pid = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_clone_last_flags = 0u;
static u32 g_linux_abi64_clone_last_task_id = SCHEDULER64_INVALID_TASK;
static u32 g_linux_abi64_clone_last_shared_vma = 0u;
static u32 g_linux_abi64_clone_last_shared_fd = 0u;
static u32 g_linux_abi64_clone_last_shared_audit = 0u;
static u64 g_linux_abi64_clone_last_child_stack = 0ull;
static u64 g_linux_abi64_clone_last_tls_base = 0ull;
static u32 g_linux_abi64_execve_count = 0u;
static u32 g_linux_abi64_execveat_count = 0u;
static u32 g_linux_abi64_execve_denial_count = 0u;
static u32 g_linux_abi64_execve_fault_count = 0u;
static u32 g_linux_abi64_execve_last_error = 0u;
static u32 g_linux_abi64_execve_last_path_checksum = 0u;
static u32 g_linux_abi64_execve_last_binary_bytes = 0u;
static u32 g_linux_abi64_execve_last_closed_fds = 0u;
static u32 g_linux_abi64_execve_last_fd_live_before = 0u;
static u32 g_linux_abi64_execve_last_fd_live_after = 0u;
static u32 g_linux_abi64_execve_last_vma_before = 0u;
static u32 g_linux_abi64_execve_last_vma_released = 0u;
static u32 g_linux_abi64_execve_last_vma_after = 0u;
static u32 g_linux_abi64_execve_last_argc = 0u;
static u32 g_linux_abi64_execve_last_envc = 0u;
static u32 g_linux_abi64_execve_last_transfer_ready = 0u;
static u64 g_linux_abi64_execve_last_transfer_rip = 0ull;
static u64 g_linux_abi64_execve_last_transfer_rsp = 0ull;
static u32 g_linux_abi64_execve_last_entry_prot = 0u;
static u32 g_linux_abi64_execve_last_stack_prot = 0u;
static u32 g_linux_abi64_execve_transfer_pending = 0u;
static u32 g_linux_abi64_execve_transfer_pid = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_wait4_count = 0u;
static u32 g_linux_abi64_wait4_reap_count = 0u;
static u32 g_linux_abi64_wait4_nohang_count = 0u;
static u32 g_linux_abi64_wait4_denial_count = 0u;
static u32 g_linux_abi64_wait4_fault_count = 0u;
static u32 g_linux_abi64_wait4_last_parent_pid = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_wait4_last_child_pid = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_wait4_last_exit_code = 0u;
static u32 g_linux_abi64_wait4_last_status = 0u;
static u32 g_linux_abi64_wait4_last_status_written = 0u;
static u32 g_linux_abi64_wait4_last_options = 0u;
static u32 g_linux_abi64_wait4_last_process_release = 0u;
static u32 g_linux_abi64_wait4_last_clone_release = 0u;
static u32 g_linux_abi64_kill_count = 0u;
static u32 g_linux_abi64_tkill_count = 0u;
static u32 g_linux_abi64_kill_unavailable_count = 0u;
static u32 g_linux_abi64_kill_denial_count = 0u;
static u32 g_linux_abi64_kill_last_syscall = 0u;
static u32 g_linux_abi64_kill_last_target = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_kill_last_signal = 0u;
static u32 g_linux_abi64_kill_last_result = 0u;
static u32 g_linux_abi64_signal_pending_count = 0u;
static u32 g_linux_abi64_signal_delivery_count = 0u;
static u32 g_linux_abi64_signal_masked_count = 0u;
static u32 g_linux_abi64_signal_delivery_denial_count = 0u;
static u32 g_linux_abi64_signal_delivery_fault_count = 0u;
static u32 g_linux_abi64_signal_delivery_last_signal = 0u;
static u64 g_linux_abi64_signal_delivery_last_handler = 0ull;
static u64 g_linux_abi64_signal_delivery_last_frame = 0ull;
static u64 g_linux_abi64_signal_delivery_last_saved_rip = 0ull;
static u64 g_linux_abi64_signal_delivery_last_saved_rsp = 0ull;
static u64 g_linux_abi64_signal_delivery_last_mask = 0ull;
static u32 g_linux_abi64_signal_delivery_last_result = 0u;
static u32 g_linux_abi64_rt_sigreturn_count = 0u;
static u32 g_linux_abi64_rt_sigreturn_denial_count = 0u;
static u32 g_linux_abi64_rt_sigreturn_fault_count = 0u;
static u64 g_linux_abi64_rt_sigreturn_last_frame = 0ull;
static u64 g_linux_abi64_rt_sigreturn_last_rip = 0ull;
static u64 g_linux_abi64_rt_sigreturn_last_rsp = 0ull;
static u64 g_linux_abi64_rt_sigreturn_last_mask = 0ull;
static u64 g_linux_abi64_rt_sigreturn_last_rax = 0ull;
static u32 g_linux_abi64_rt_sigreturn_last_result = 0u;
static u64 g_linux_abi64_getrandom_state = 0x6C696D69746C6573ull;
static u32 g_linux_abi64_getrandom_count = 0u;
static u32 g_linux_abi64_getrandom_byte_count = 0u;
static u32 g_linux_abi64_getrandom_denial_count = 0u;
static u32 g_linux_abi64_getrandom_fault_count = 0u;
static u32 g_linux_abi64_getrandom_last_byte_count = 0u;
static u32 g_linux_abi64_getrandom_last_checksum = 0u;
static u32 g_linux_abi64_getrandom_last_flags = 0u;
static u32 g_linux_abi64_getrandom_last_result = 0u;
static u32 g_linux_abi64_getpid_count = 0u;
static u32 g_linux_abi64_getpid_denial_count = 0u;
static u32 g_linux_abi64_geteuid_count = 0u;
static u32 g_linux_abi64_geteuid_denial_count = 0u;
static u32 g_linux_abi64_getppid_count = 0u;
static u32 g_linux_abi64_getppid_denial_count = 0u;
static u32 g_linux_abi64_gettid_count = 0u;
static u32 g_linux_abi64_gettid_denial_count = 0u;
static u32 g_linux_abi64_ioctl_count = 0u;
static u32 g_linux_abi64_ioctl_tty_count = 0u;
static u32 g_linux_abi64_ioctl_enotty_count = 0u;
static u32 g_linux_abi64_ioctl_enosys_count = 0u;
static u32 g_linux_abi64_ioctl_denial_count = 0u;
static u32 g_linux_abi64_ioctl_last_fd = 0u;
static u32 g_linux_abi64_ioctl_last_request = 0u;
static u32 g_linux_abi64_ioctl_last_result = 0u;
static u32 g_linux_abi64_prctl_count = 0u;
static u32 g_linux_abi64_prctl_set_name_count = 0u;
static u32 g_linux_abi64_prctl_get_name_count = 0u;
static u32 g_linux_abi64_prctl_enosys_count = 0u;
static u32 g_linux_abi64_prctl_denial_count = 0u;
static u32 g_linux_abi64_prctl_fault_count = 0u;
static u32 g_linux_abi64_prctl_last_option = 0u;
static u32 g_linux_abi64_prctl_last_result = 0u;
static u32 g_linux_abi64_arch_prctl_count = 0u;
static u32 g_linux_abi64_arch_prctl_set_count = 0u;
static u32 g_linux_abi64_arch_prctl_get_count = 0u;
static u32 g_linux_abi64_arch_prctl_denial_count = 0u;
static u32 g_linux_abi64_arch_prctl_fault_count = 0u;
static u32 g_linux_abi64_set_tid_address_count = 0u;
static u32 g_linux_abi64_set_tid_address_denial_count = 0u;
static u32 g_linux_abi64_clock_gettime_count = 0u;
static u32 g_linux_abi64_clock_gettime_denial_count = 0u;
static u32 g_linux_abi64_clock_gettime_fault_count = 0u;
static u32 g_linux_abi64_exit_count = 0u;
static u32 g_linux_abi64_exit_group_count = 0u;

static u32 linux_abi64_effective_tick_frequency(void);
static u32 g_linux_abi64_exit_denial_count = 0u;
static u32 g_linux_abi64_last_exit_pid = PROCESS64_INVALID_PID;
static u32 g_linux_abi64_last_exit_code = 0u;
static u32 g_linux_abi64_last_exit_vma_regions = 0u;
static u32 g_linux_abi64_last_exit_fd_entries = 0u;
static u32 g_linux_abi64_last_exit_persona_released = 0u;
static u32 g_linux_abi64_last_exit_audit_released = 0u;
static u32 g_linux_abi64_last_exit_audit_recorded = 0u;
static void *g_linux_abi64_last_exit_detached_vma = 0;
static void *g_linux_abi64_last_exit_detached_fd = 0;
static void *g_linux_abi64_last_exit_detached_audit = 0;

extern volatile u64 syscall64_native_linux_rdi;
extern volatile u64 syscall64_native_linux_rsi;
extern volatile u64 syscall64_native_linux_rdx;
extern volatile u64 syscall64_native_linux_r10;
extern volatile u64 syscall64_native_linux_r8;
extern volatile u64 syscall64_native_linux_r9;
extern volatile u64 syscall64_native_user_rsp;
extern volatile u64 syscall64_native_user_rbx;
extern volatile u64 syscall64_native_user_rbp;
extern volatile u64 syscall64_native_user_r12;
extern volatile u64 syscall64_native_user_r13;
extern volatile u64 syscall64_native_user_r14;
extern volatile u64 syscall64_native_user_r15;

static u64 linux_abi64_unimplemented_stub(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)pid;
    (void)rdi;
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    (void)rip;

    return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOSYS);
}

static u64 linux_abi64_brk_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    (void)rip;

    return linux_abi64_sys_brk(pid, rdi);
}

static u64 linux_abi64_rt_sigaction_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r8;
    (void)r9;

    return linux_abi64_sys_rt_sigaction(pid, rdi, rsi, rdx, r10, rip);
}

static u64 linux_abi64_rt_sigprocmask_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r8;
    (void)r9;

    return linux_abi64_sys_rt_sigprocmask(pid, rdi, rsi, rdx, r10, rip);
}

static u64 linux_abi64_rt_sigreturn_dispatch(
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

    ++g_linux_abi64_rt_sigreturn_denial_count;
    g_linux_abi64_rt_sigreturn_last_frame = 0ull;
    g_linux_abi64_rt_sigreturn_last_rip = 0ull;
    g_linux_abi64_rt_sigreturn_last_rsp = 0ull;
    g_linux_abi64_rt_sigreturn_last_mask = 0ull;
    g_linux_abi64_rt_sigreturn_last_rax = 0ull;
    g_linux_abi64_rt_sigreturn_last_result = LINUX_ABI64_EINVAL;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_RT_SIGRETURN,
        LINUX_ABI64_EINVAL,
        rip);
    return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
}

static u64 linux_abi64_getpid_dispatch(
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

    return linux_abi64_sys_getpid(pid, rip);
}

static u64 linux_abi64_geteuid_dispatch(
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

    return linux_abi64_sys_geteuid(pid, rip);
}

static u64 linux_abi64_getppid_dispatch(
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

    return linux_abi64_sys_getppid(pid, rip);
}

static u64 linux_abi64_gettid_dispatch(
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

    return linux_abi64_sys_gettid(pid, rip);
}

static u64 linux_abi64_nanosleep_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_nanosleep(pid, rdi, rsi, rip);
}

static u64 linux_abi64_getrlimit_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_getrlimit(pid, rdi, rsi, rip);
}

static u64 linux_abi64_setrlimit_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_setrlimit(pid, rdi, rsi, rip);
}

static u64 linux_abi64_pipe2_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_pipe2(pid, rdi, rsi, rip);
}

static u64 linux_abi64_pipe_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_pipe(pid, rdi, rip);
}

static u64 linux_abi64_dup_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_dup(pid, rdi, rip);
}

static u64 linux_abi64_dup2_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_dup2(pid, rdi, rsi, rip);
}

static u64 linux_abi64_dup3_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_dup3(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_fcntl_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_fcntl(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_getcwd_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_getcwd(pid, rdi, rsi, rip);
}

static u64 linux_abi64_chdir_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_chdir(pid, rdi, rip);
}

static u64 linux_abi64_fchdir_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_fchdir(pid, rdi, rip);
}

static u64 linux_abi64_getdents64_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_getdents64(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_futex_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    return linux_abi64_sys_futex(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

static u64 linux_abi64_clone_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r9;

    return linux_abi64_sys_clone(pid, rdi, rsi, rdx, r10, r8, rip);
}

static u64 linux_abi64_fork_dispatch(
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

    return linux_abi64_sys_fork(pid, rip);
}

static u64 linux_abi64_execve_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_execve(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_execveat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r9;

    return linux_abi64_sys_execveat(pid, rdi, rsi, rdx, r10, r8, rip);
}

static u64 linux_abi64_wait4_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r8;
    (void)r9;

    return linux_abi64_sys_wait4(pid, rdi, rsi, rdx, r10, rip);
}

static u64 linux_abi64_kill_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_kill(pid, rdi, rsi, rip);
}

static u64 linux_abi64_tkill_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_tkill(pid, rdi, rsi, rip);
}

static u64 linux_abi64_getrandom_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_getrandom(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_arch_prctl_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_arch_prctl(pid, rdi, rsi, rip);
}

static u64 linux_abi64_prctl_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r9;

    return linux_abi64_sys_prctl(pid, rdi, rsi, rdx, r10, r8, rip);
}

static u64 linux_abi64_set_tid_address_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_set_tid_address(pid, rdi, rip);
}

static u64 linux_abi64_clock_gettime_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_clock_gettime(pid, rdi, rsi, rip);
}

static u64 linux_abi64_write_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_write(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_read_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_read(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_pread64_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r8;
    (void)r9;

    return linux_abi64_sys_pread64(pid, rdi, rsi, rdx, r10, rip);
}

static u64 linux_abi64_pwrite64_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r8;
    (void)r9;

    return linux_abi64_sys_pwrite64(pid, rdi, rsi, rdx, r10, rip);
}

static u64 linux_abi64_readv_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_readv(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_writev_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_writev(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_poll_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_poll(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_ppoll_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r9;

    return linux_abi64_sys_ppoll(pid, rdi, rsi, rdx, r10, r8, rip);
}

static u64 linux_abi64_ioctl_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_ioctl(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_open_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_open(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_close_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_close(pid, rdi, rip);
}

static u64 linux_abi64_lseek_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_lseek(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_stat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_stat(pid, rdi, rsi, rip);
}

static u64 linux_abi64_lstat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_lstat(pid, rdi, rsi, rip);
}

static u64 linux_abi64_fstat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_fstat(pid, rdi, rsi, rip);
}

static u64 linux_abi64_readlink_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_readlink(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_newfstatat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r8;
    (void)r9;

    return linux_abi64_sys_newfstatat(pid, rdi, rsi, rdx, r10, rip);
}

static u64 linux_abi64_mmap_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    return linux_abi64_sys_mmap(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

static u64 linux_abi64_mprotect_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r10;
    (void)r8;
    (void)r9;
    return linux_abi64_sys_mprotect(pid, rdi, rsi, rdx, rip);
}

static u64 linux_abi64_munmap_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;
    return linux_abi64_sys_munmap(pid, rdi, rsi, rip);
}

static u64 linux_abi64_exit_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_exit(pid, rdi, rip);
}

static u64 linux_abi64_exit_group_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)rsi;
    (void)rdx;
    (void)r10;
    (void)r8;
    (void)r9;

    return linux_abi64_sys_exit_group(pid, rdi, rip);
}

static u64 linux_abi64_openat_dispatch(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    (void)r8;
    (void)r9;

    return linux_abi64_sys_openat(pid, rdi, rsi, rdx, r10, rip);
}

static void linux_abi64_clear_exit_record(linux_abi64_exit_record_t *record)
{
    if (record == 0)
    {
        return;
    }

    record->pid = PROCESS64_INVALID_PID;
    record->exit_code = 0u;
    record->exited = 0u;
    record->reserved = 0u;
}

static u64 linux_abi64_rlimit_budget(u32 resource)
{
    switch (resource)
    {
    case LINUX_ABI64_RLIMIT_DATA:
        return LINUX_ABI64_RLIMIT_DATA_BYTES;
    case LINUX_ABI64_RLIMIT_STACK:
        return LINUX_ABI64_RLIMIT_STACK_BYTES;
    case LINUX_ABI64_RLIMIT_NPROC:
    case LINUX_ABI64_RLIMIT_SIGPENDING:
        return 64ull;
    case LINUX_ABI64_RLIMIT_NOFILE:
        return LINUX_ABI64_RLIMIT_NOFILE_COUNT;
    case LINUX_ABI64_RLIMIT_MEMLOCK:
        return 65536ull;
    case LINUX_ABI64_RLIMIT_MSGQUEUE:
        return 819200ull;
    case LINUX_ABI64_RLIMIT_NICE:
    case LINUX_ABI64_RLIMIT_RTPRIO:
        return 0ull;
    default:
        return LINUX_ABI64_RLIM_INFINITY;
    }
}

static u64 linux_abi64_rlimit_default_current(u32 resource)
{
    switch (resource)
    {
    case LINUX_ABI64_RLIMIT_DATA:
    case LINUX_ABI64_RLIMIT_STACK:
    case LINUX_ABI64_RLIMIT_NPROC:
    case LINUX_ABI64_RLIMIT_NOFILE:
    case LINUX_ABI64_RLIMIT_MEMLOCK:
    case LINUX_ABI64_RLIMIT_SIGPENDING:
    case LINUX_ABI64_RLIMIT_MSGQUEUE:
    case LINUX_ABI64_RLIMIT_NICE:
    case LINUX_ABI64_RLIMIT_RTPRIO:
        return linux_abi64_rlimit_budget(resource);
    default:
        return LINUX_ABI64_RLIM_INFINITY;
    }
}

static void linux_abi64_clear_rlimit_record(linux_abi64_rlimit_record_t *record)
{
    u32 index;

    if (record == 0)
    {
        return;
    }

    record->pid = PROCESS64_INVALID_PID;
    record->initialized = 0u;
    for (index = 0u; index < LINUX_ABI64_RLIMIT_COUNT; ++index)
    {
        record->current[index] = 0ull;
        record->maximum[index] = 0ull;
    }
}

static void linux_abi64_clear_futex_waiter(linux_abi64_futex_waiter_t *waiter)
{
    if (waiter == 0)
    {
        return;
    }

    waiter->active = 0u;
    waiter->pid = PROCESS64_INVALID_PID;
    waiter->task_id = SCHEDULER64_INVALID_TASK;
    waiter->timed = 0u;
    waiter->expected_value = 0u;
    waiter->timeout_ticks = 0u;
    waiter->timeout_result = 0u;
    waiter->reserved = 0u;
    waiter->user_address = 0ull;
    waiter->rip = 0ull;
}

static u32 linux_abi64_futex_active_waiters(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0u; index < LINUX_ABI64_MAX_FUTEX_WAITERS; ++index)
    {
        if (g_linux_abi64_futex_waiters[index].active != 0u)
        {
            ++count;
        }
    }

    return count;
}

static linux_abi64_futex_waiter_t *linux_abi64_futex_waiter_for(
    u32 pid,
    u64 user_address)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_MAX_FUTEX_WAITERS; ++index)
    {
        if ((g_linux_abi64_futex_waiters[index].active != 0u)
            && (g_linux_abi64_futex_waiters[index].pid == pid)
            && (g_linux_abi64_futex_waiters[index].user_address == user_address))
        {
            return &g_linux_abi64_futex_waiters[index];
        }
    }

    return 0;
}

static linux_abi64_futex_waiter_t *linux_abi64_futex_free_waiter(void)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_MAX_FUTEX_WAITERS; ++index)
    {
        if (g_linux_abi64_futex_waiters[index].active == 0u)
        {
            return &g_linux_abi64_futex_waiters[index];
        }
    }

    return 0;
}

static u32 linux_abi64_release_futex_waiters(u32 pid)
{
    u32 index;
    u32 released = 0u;

    for (index = 0u; index < LINUX_ABI64_MAX_FUTEX_WAITERS; ++index)
    {
        if ((g_linux_abi64_futex_waiters[index].active != 0u)
            && (g_linux_abi64_futex_waiters[index].pid == pid))
        {
            if (g_linux_abi64_futex_waiters[index].timed != 0u)
            {
                (void)scheduler64_sleep_cancel_task(g_linux_abi64_futex_waiters[index].task_id);
            }
            linux_abi64_clear_futex_waiter(&g_linux_abi64_futex_waiters[index]);
            ++released;
        }
    }

    return released;
}

static void linux_abi64_futex_timeout_callback(u32 task_id, u64 cookie)
{
    linux_abi64_futex_waiter_t *waiter = (linux_abi64_futex_waiter_t *)(u64)cookie;

    if ((waiter == 0)
        || (waiter->active == 0u)
        || (waiter->task_id != task_id)
        || (waiter->timed == 0u))
    {
        return;
    }

    ++g_linux_abi64_futex_timeout_count;
    g_linux_abi64_futex_last_timeout_task_id = task_id;
    g_linux_abi64_futex_last_timeout_ticks = waiter->timeout_ticks;
    g_linux_abi64_futex_last_timeout_result = LINUX_ABI64_ETIMEDOUT;
    (void)persona_audit64_record(
        waiter->pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_FUTEX,
        LINUX_ABI64_ETIMEDOUT,
        waiter->rip);
    linux_abi64_clear_futex_waiter(waiter);
}

static void linux_abi64_clear_clone_record(linux_abi64_clone_record_t *record)
{
    if (record == 0)
    {
        return;
    }

    record->active = 0u;
    record->parent_pid = PROCESS64_INVALID_PID;
    record->child_pid = PROCESS64_INVALID_PID;
    record->flags = 0u;
    record->task_id = SCHEDULER64_INVALID_TASK;
    record->shared_vma = 0u;
    record->shared_fd = 0u;
    record->shared_audit = 0u;
    record->wait_blocked = 0u;
    record->wait_task_id = SCHEDULER64_INVALID_TASK;
    record->child_stack = 0ull;
    record->tls_base = 0ull;
    record->wait_status_user = 0ull;
    record->wait_rip = 0ull;
}

static linux_abi64_clone_record_t *linux_abi64_clone_free_record(void)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_MAX_CLONE_RECORDS; ++index)
    {
        if (g_linux_abi64_clone_records[index].active == 0u)
        {
            return &g_linux_abi64_clone_records[index];
        }
    }

    return 0;
}

static linux_abi64_clone_record_t *linux_abi64_clone_record_for_child(u32 child_pid)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_MAX_CLONE_RECORDS; ++index)
    {
        if ((g_linux_abi64_clone_records[index].active != 0u)
            && (g_linux_abi64_clone_records[index].child_pid == child_pid))
        {
            return &g_linux_abi64_clone_records[index];
        }
    }

    return 0;
}

static linux_abi64_clone_record_t *linux_abi64_clone_record_for_parent_wait(
    u32 parent_pid,
    u64 requested_pid,
    u32 *matched_child_count)
{
    u32 index;

    if (matched_child_count != 0)
    {
        *matched_child_count = 0u;
    }

    for (index = 0u; index < LINUX_ABI64_MAX_CLONE_RECORDS; ++index)
    {
        linux_abi64_clone_record_t *record = &g_linux_abi64_clone_records[index];

        if ((record->active == 0u) || (record->parent_pid != parent_pid))
        {
            continue;
        }
        if ((requested_pid != LINUX_ABI64_WAIT_ANY)
            && (requested_pid != (u64)record->child_pid))
        {
            continue;
        }

        if (matched_child_count != 0)
        {
            ++(*matched_child_count);
        }
        if (linux_abi64_process_exited(record->child_pid) != 0u)
        {
            return record;
        }
    }

    return 0;
}

static linux_abi64_clone_record_t *linux_abi64_clone_record_for_parent_any(
    u32 parent_pid,
    u64 requested_pid)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_MAX_CLONE_RECORDS; ++index)
    {
        linux_abi64_clone_record_t *record = &g_linux_abi64_clone_records[index];

        if ((record->active == 0u) || (record->parent_pid != parent_pid))
        {
            continue;
        }
        if ((requested_pid != LINUX_ABI64_WAIT_ANY)
            && (requested_pid != (u64)record->child_pid))
        {
            continue;
        }
        return record;
    }

    return 0;
}

static void linux_abi64_clone_detach_shared_state(u32 child_pid)
{
    if (child_pid == PROCESS64_INVALID_PID)
    {
        return;
    }

    (void)process64_detach_vma(child_pid);
    (void)process64_detach_fd(child_pid);
    (void)process64_detach_audit(child_pid);
}

static void linux_abi64_fork_release_setup(u32 child_pid)
{
    u32 root_token;

    if (child_pid == PROCESS64_INVALID_PID)
    {
        return;
    }

    (void)linux_vfs64_release_process(child_pid);
    if (process64_fd_table(child_pid) != 0)
    {
        (void)fd64_release_process(child_pid);
    }
    if (process64_vma_root(child_pid) != 0)
    {
        (void)vma64_release_process(child_pid);
    }
    if (process64_persona_ctx(child_pid) != 0)
    {
        (void)persona64_release(child_pid);
    }
    if (process64_audit_ctx(child_pid) != 0)
    {
        (void)persona_audit64_release(child_pid);
    }
    root_token = process64_page_root_token(child_pid);
    if (root_token != 0u)
    {
        (void)paging64_process_root_release(child_pid, root_token);
        (void)process64_clear_page_root(child_pid, root_token);
    }
    (void)process64_release_clone(child_pid);
}

static void linux_abi64_init_rlimit_record(linux_abi64_rlimit_record_t *record, u32 pid)
{
    u32 index;

    if (record == 0)
    {
        return;
    }

    record->pid = pid;
    record->initialized = 1u;
    for (index = 0u; index < LINUX_ABI64_RLIMIT_COUNT; ++index)
    {
        record->current[index] = linux_abi64_rlimit_default_current(index);
        record->maximum[index] = linux_abi64_rlimit_budget(index);
    }
}

static linux_abi64_rlimit_record_t *linux_abi64_rlimit_record_for_pid(u32 pid)
{
    u32 index;
    linux_abi64_rlimit_record_t *free_record = 0;

    for (index = 0u; index < LINUX_ABI64_MAX_RLIMIT_RECORDS; ++index)
    {
        if ((g_linux_abi64_rlimit_records[index].initialized != 0u)
            && (g_linux_abi64_rlimit_records[index].pid == pid))
        {
            return &g_linux_abi64_rlimit_records[index];
        }
        if ((free_record == 0)
            && (g_linux_abi64_rlimit_records[index].initialized == 0u))
        {
            free_record = &g_linux_abi64_rlimit_records[index];
        }
    }

    if (free_record != 0)
    {
        linux_abi64_init_rlimit_record(free_record, pid);
    }

    return free_record;
}

static void linux_abi64_release_rlimit_record(u32 pid)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_MAX_RLIMIT_RECORDS; ++index)
    {
        if ((g_linux_abi64_rlimit_records[index].initialized != 0u)
            && (g_linux_abi64_rlimit_records[index].pid == pid))
        {
            linux_abi64_clear_rlimit_record(&g_linux_abi64_rlimit_records[index]);
            return;
        }
    }
}

static void linux_abi64_sync_persona_brk(u32 pid, u64 brk_value)
{
    persona_context_t *context;

    if (brk_value == 0ull)
    {
        return;
    }

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        return;
    }

    if (context->brk_base == PERSONA64_BRK_UNSET)
    {
        context->brk_base = brk_value;
    }
    context->brk_current = brk_value;
}

void linux_abi64_init(void)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_SYSCALL_LIMIT; ++index)
    {
        g_linux_abi64_dispatch_table[index] = linux_abi64_unimplemented_stub;
    }
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_READ] = linux_abi64_read_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_WRITE] = linux_abi64_write_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PREAD64] = linux_abi64_pread64_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PWRITE64] = linux_abi64_pwrite64_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_READV] = linux_abi64_readv_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_WRITEV] = linux_abi64_writev_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PIPE] = linux_abi64_pipe_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_POLL] = linux_abi64_poll_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PPOLL] = linux_abi64_ppoll_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_IOCTL] = linux_abi64_ioctl_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_OPEN] = linux_abi64_open_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CLOSE] = linux_abi64_close_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_STAT] = linux_abi64_stat_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_LSTAT] = linux_abi64_lstat_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FSTAT] = linux_abi64_fstat_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_READLINK] = linux_abi64_readlink_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_LSEEK] = linux_abi64_lseek_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_MMAP] = linux_abi64_mmap_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_MPROTECT] = linux_abi64_mprotect_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_MUNMAP] = linux_abi64_munmap_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_BRK] = linux_abi64_brk_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_RT_SIGACTION] =
        linux_abi64_rt_sigaction_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_RT_SIGPROCMASK] =
        linux_abi64_rt_sigprocmask_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_RT_SIGRETURN] =
        linux_abi64_rt_sigreturn_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_DUP] = linux_abi64_dup_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_DUP2] = linux_abi64_dup2_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_NANOSLEEP] =
        linux_abi64_nanosleep_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETRLIMIT] =
        linux_abi64_getrlimit_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_SETRLIMIT] =
        linux_abi64_setrlimit_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FCNTL] = linux_abi64_fcntl_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETCWD] = linux_abi64_getcwd_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CHDIR] = linux_abi64_chdir_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FCHDIR] = linux_abi64_fchdir_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETPID] = linux_abi64_getpid_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETEUID] = linux_abi64_geteuid_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETPPID] = linux_abi64_getppid_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CLONE] = linux_abi64_clone_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FORK] = linux_abi64_fork_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXECVE] = linux_abi64_execve_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_WAIT4] = linux_abi64_wait4_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_KILL] = linux_abi64_kill_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_TKILL] = linux_abi64_tkill_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FUTEX] = linux_abi64_futex_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETDENTS64] =
        linux_abi64_getdents64_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXIT] = linux_abi64_exit_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PRCTL] = linux_abi64_prctl_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_ARCH_PRCTL] = linux_abi64_arch_prctl_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETTID] = linux_abi64_gettid_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_SET_TID_ADDRESS] =
        linux_abi64_set_tid_address_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CLOCK_GETTIME] =
        linux_abi64_clock_gettime_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXIT_GROUP] = linux_abi64_exit_group_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_OPENAT] = linux_abi64_openat_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_NEWFSTATAT] = linux_abi64_newfstatat_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_DUP3] = linux_abi64_dup3_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PIPE2] = linux_abi64_pipe2_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETRANDOM] = linux_abi64_getrandom_dispatch;
    g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXECVEAT] = linux_abi64_execveat_dispatch;

    for (index = 0u; index < LINUX_ABI64_MAX_EXIT_RECORDS; ++index)
    {
        linux_abi64_clear_exit_record(&g_linux_abi64_exit_records[index]);
    }
    for (index = 0u; index < LINUX_ABI64_MAX_RLIMIT_RECORDS; ++index)
    {
        linux_abi64_clear_rlimit_record(&g_linux_abi64_rlimit_records[index]);
    }
    for (index = 0u; index < LINUX_ABI64_MAX_FUTEX_WAITERS; ++index)
    {
        linux_abi64_clear_futex_waiter(&g_linux_abi64_futex_waiters[index]);
    }
    for (index = 0u; index < LINUX_ABI64_MAX_CLONE_RECORDS; ++index)
    {
        linux_abi64_clear_clone_record(&g_linux_abi64_clone_records[index]);
    }

    g_linux_abi64_dispatch_count = 0u;
    g_linux_abi64_dispatch_root_repair_count = 0u;
    g_linux_abi64_dispatch_root_reload_count = 0u;
    g_linux_abi64_dispatch_root_denial_count = 0u;
    g_linux_abi64_unimplemented_count = 0u;
    g_linux_abi64_unimplemented_last_syscall = 0u;
    g_linux_abi64_unimplemented_last_rip = 0ull;
    g_linux_abi64_read_count = 0u;
    g_linux_abi64_read_byte_count = 0u;
    g_linux_abi64_read_denial_count = 0u;
    g_linux_abi64_read_fault_count = 0u;
    g_linux_abi64_write_count = 0u;
    g_linux_abi64_write_byte_count = 0u;
    g_linux_abi64_write_denial_count = 0u;
    g_linux_abi64_write_fault_count = 0u;
    g_linux_abi64_pread64_count = 0u;
    g_linux_abi64_pwrite64_count = 0u;
    g_linux_abi64_pread64_byte_count = 0u;
    g_linux_abi64_pwrite64_byte_count = 0u;
    g_linux_abi64_positioned_denial_count = 0u;
    g_linux_abi64_positioned_fault_count = 0u;
    g_linux_abi64_positioned_last_syscall = 0u;
    g_linux_abi64_positioned_last_fd = FD64_INVALID_FD;
    g_linux_abi64_positioned_last_byte_count = 0u;
    g_linux_abi64_positioned_last_offset = 0ull;
    g_linux_abi64_positioned_last_result = 0u;
    g_linux_abi64_readv_count = 0u;
    g_linux_abi64_writev_count = 0u;
    g_linux_abi64_readv_byte_count = 0u;
    g_linux_abi64_writev_byte_count = 0u;
    g_linux_abi64_vector_denial_count = 0u;
    g_linux_abi64_vector_fault_count = 0u;
    g_linux_abi64_vector_last_syscall = 0u;
    g_linux_abi64_vector_last_fd = FD64_INVALID_FD;
    g_linux_abi64_vector_last_iov_count = 0u;
    g_linux_abi64_vector_last_byte_count = 0u;
    g_linux_abi64_vector_last_result = 0u;
    g_linux_abi64_poll_count = 0u;
    g_linux_abi64_ppoll_count = 0u;
    g_linux_abi64_poll_ready_count = 0u;
    g_linux_abi64_poll_denial_count = 0u;
    g_linux_abi64_poll_fault_count = 0u;
    g_linux_abi64_poll_last_syscall = 0u;
    g_linux_abi64_poll_last_fd_count = 0u;
    g_linux_abi64_poll_last_ready = 0u;
    g_linux_abi64_poll_last_revents = 0u;
    g_linux_abi64_poll_last_result = 0u;
    g_linux_abi64_open_count = 0u;
    g_linux_abi64_openat_count = 0u;
    g_linux_abi64_open_denial_count = 0u;
    g_linux_abi64_close_count = 0u;
    g_linux_abi64_close_denial_count = 0u;
    g_linux_abi64_lseek_count = 0u;
    g_linux_abi64_lseek_denial_count = 0u;
    g_linux_abi64_stat_count = 0u;
    g_linux_abi64_stat_denial_count = 0u;
    g_linux_abi64_stat_fault_count = 0u;
    g_linux_abi64_fstat_count = 0u;
    g_linux_abi64_fstat_denial_count = 0u;
    g_linux_abi64_fstat_fault_count = 0u;
    g_linux_abi64_newfstatat_count = 0u;
    g_linux_abi64_newfstatat_denial_count = 0u;
    g_linux_abi64_newfstatat_fault_count = 0u;
    g_linux_abi64_readlink_count = 0u;
    g_linux_abi64_readlink_byte_count = 0u;
    g_linux_abi64_readlink_denial_count = 0u;
    g_linux_abi64_readlink_fault_count = 0u;
    g_linux_abi64_readlink_last_result = 0u;
    g_linux_abi64_mmap_count = 0u;
    g_linux_abi64_mmap_byte_count = 0u;
    g_linux_abi64_mmap_denial_count = 0u;
    g_linux_abi64_mprotect_count = 0u;
    g_linux_abi64_mprotect_byte_count = 0u;
    g_linux_abi64_mprotect_denial_count = 0u;
    g_linux_abi64_munmap_count = 0u;
    g_linux_abi64_munmap_byte_count = 0u;
    g_linux_abi64_munmap_denial_count = 0u;
    g_linux_abi64_brk_query_count = 0u;
    g_linux_abi64_brk_extend_count = 0u;
    g_linux_abi64_brk_denial_count = 0u;
    g_linux_abi64_rt_sigaction_count = 0u;
    g_linux_abi64_rt_sigaction_query_count = 0u;
    g_linux_abi64_rt_sigaction_denial_count = 0u;
    g_linux_abi64_rt_sigaction_fault_count = 0u;
    g_linux_abi64_rt_sigaction_last_signal = 0u;
    g_linux_abi64_rt_sigaction_last_handler = 0ull;
    g_linux_abi64_rt_sigaction_last_old_handler = 0ull;
    g_linux_abi64_rt_sigaction_last_mask = 0ull;
    g_linux_abi64_rt_sigaction_last_flags = 0ull;
    g_linux_abi64_rt_sigaction_last_result = 0u;
    g_linux_abi64_rt_sigprocmask_count = 0u;
    g_linux_abi64_rt_sigprocmask_query_count = 0u;
    g_linux_abi64_rt_sigprocmask_denial_count = 0u;
    g_linux_abi64_rt_sigprocmask_fault_count = 0u;
    g_linux_abi64_rt_sigprocmask_last_how = 0u;
    g_linux_abi64_rt_sigprocmask_last_set = 0ull;
    g_linux_abi64_rt_sigprocmask_last_old_mask = 0ull;
    g_linux_abi64_rt_sigprocmask_last_mask = 0ull;
    g_linux_abi64_rt_sigprocmask_last_result = 0u;
    g_linux_abi64_nanosleep_count = 0u;
    g_linux_abi64_nanosleep_denial_count = 0u;
    g_linux_abi64_nanosleep_fault_count = 0u;
    g_linux_abi64_nanosleep_interrupted_count = 0u;
    g_linux_abi64_getrlimit_count = 0u;
    g_linux_abi64_setrlimit_count = 0u;
    g_linux_abi64_rlimit_denial_count = 0u;
    g_linux_abi64_rlimit_fault_count = 0u;
    g_linux_abi64_pipe_count = 0u;
    g_linux_abi64_pipe_denial_count = 0u;
    g_linux_abi64_pipe_fault_count = 0u;
    g_linux_abi64_pipe2_count = 0u;
    g_linux_abi64_pipe2_denial_count = 0u;
    g_linux_abi64_pipe2_fault_count = 0u;
    g_linux_abi64_dup_count = 0u;
    g_linux_abi64_dup2_count = 0u;
    g_linux_abi64_dup3_count = 0u;
    g_linux_abi64_dup_denial_count = 0u;
    g_linux_abi64_fcntl_count = 0u;
    g_linux_abi64_fcntl_denial_count = 0u;
    g_linux_abi64_getcwd_count = 0u;
    g_linux_abi64_getcwd_byte_count = 0u;
    g_linux_abi64_getcwd_denial_count = 0u;
    g_linux_abi64_getcwd_fault_count = 0u;
    g_linux_abi64_path_relative_count = 0u;
    g_linux_abi64_path_dot_count = 0u;
    g_linux_abi64_path_dotdot_count = 0u;
    g_linux_abi64_path_trailing_count = 0u;
    g_linux_abi64_path_trailing_denial_count = 0u;
    g_linux_abi64_path_fault_count = 0u;
    g_linux_abi64_chdir_count = 0u;
    g_linux_abi64_fchdir_count = 0u;
    g_linux_abi64_chdir_denial_count = 0u;
    g_linux_abi64_chdir_fault_count = 0u;
    g_linux_abi64_getdents64_count = 0u;
    g_linux_abi64_getdents64_entry_count = 0u;
    g_linux_abi64_getdents64_byte_count = 0u;
    g_linux_abi64_getdents64_denial_count = 0u;
    g_linux_abi64_getdents64_fault_count = 0u;
    g_linux_abi64_futex_wait_count = 0u;
    g_linux_abi64_futex_wake_count = 0u;
    g_linux_abi64_futex_woken_count = 0u;
    g_linux_abi64_futex_eagain_count = 0u;
    g_linux_abi64_futex_denial_count = 0u;
    g_linux_abi64_futex_fault_count = 0u;
    g_linux_abi64_futex_timed_wait_count = 0u;
    g_linux_abi64_futex_timeout_count = 0u;
    g_linux_abi64_futex_last_wait_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_futex_last_wait_address = 0ull;
    g_linux_abi64_futex_last_wait_value = 0u;
    g_linux_abi64_futex_last_wait_task_id = SCHEDULER64_INVALID_TASK;
    g_linux_abi64_futex_last_wake_count = 0u;
    g_linux_abi64_futex_last_timeout_task_id = SCHEDULER64_INVALID_TASK;
    g_linux_abi64_futex_last_timeout_ticks = 0u;
    g_linux_abi64_futex_last_timeout_result = 0u;
    g_linux_abi64_clone_count = 0u;
    g_linux_abi64_clone_thread_count = 0u;
    g_linux_abi64_clone_denial_count = 0u;
    g_linux_abi64_clone_fork_denial_count = 0u;
    g_linux_abi64_clone_scheduler_count = 0u;
    g_linux_abi64_fork_count = 0u;
    g_linux_abi64_fork_success_count = 0u;
    g_linux_abi64_fork_enosys_count = 0u;
    g_linux_abi64_fork_denial_count = 0u;
    g_linux_abi64_fork_last_rip = 0ull;
    g_linux_abi64_fork_last_child_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_fork_last_child_slot = 0xFFFFFFFFu;
    g_linux_abi64_fork_last_child_root_distinct = 0u;
    g_linux_abi64_fork_last_task_id = SCHEDULER64_INVALID_TASK;
    g_linux_abi64_child_root_cleanup_count = 0u;
    g_linux_abi64_clone_last_parent_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_clone_last_child_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_clone_last_flags = 0u;
    g_linux_abi64_clone_last_task_id = SCHEDULER64_INVALID_TASK;
    g_linux_abi64_clone_last_shared_vma = 0u;
    g_linux_abi64_clone_last_shared_fd = 0u;
    g_linux_abi64_clone_last_shared_audit = 0u;
    g_linux_abi64_clone_last_child_stack = 0ull;
    g_linux_abi64_clone_last_tls_base = 0ull;
    g_linux_abi64_execve_count = 0u;
    g_linux_abi64_execveat_count = 0u;
    g_linux_abi64_execve_denial_count = 0u;
    g_linux_abi64_execve_fault_count = 0u;
    g_linux_abi64_execve_last_error = 0u;
    g_linux_abi64_execve_last_path_checksum = 0u;
    g_linux_abi64_execve_last_binary_bytes = 0u;
    g_linux_abi64_execve_last_closed_fds = 0u;
    g_linux_abi64_execve_last_fd_live_before = 0u;
    g_linux_abi64_execve_last_fd_live_after = 0u;
    g_linux_abi64_execve_last_vma_before = 0u;
    g_linux_abi64_execve_last_vma_released = 0u;
    g_linux_abi64_execve_last_vma_after = 0u;
    g_linux_abi64_execve_last_argc = 0u;
    g_linux_abi64_execve_last_envc = 0u;
    g_linux_abi64_execve_last_transfer_ready = 0u;
    g_linux_abi64_execve_last_transfer_rip = 0ull;
    g_linux_abi64_execve_last_transfer_rsp = 0ull;
    g_linux_abi64_execve_last_entry_prot = 0u;
    g_linux_abi64_execve_last_stack_prot = 0u;
    g_linux_abi64_execve_transfer_pending = 0u;
    g_linux_abi64_execve_transfer_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_wait4_count = 0u;
    g_linux_abi64_wait4_reap_count = 0u;
    g_linux_abi64_wait4_nohang_count = 0u;
    g_linux_abi64_wait4_denial_count = 0u;
    g_linux_abi64_wait4_fault_count = 0u;
    g_linux_abi64_wait4_last_parent_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_wait4_last_child_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_wait4_last_exit_code = 0u;
    g_linux_abi64_wait4_last_status = 0u;
    g_linux_abi64_wait4_last_status_written = 0u;
    g_linux_abi64_wait4_last_options = 0u;
    g_linux_abi64_wait4_last_process_release = 0u;
    g_linux_abi64_wait4_last_clone_release = 0u;
    g_linux_abi64_kill_count = 0u;
    g_linux_abi64_tkill_count = 0u;
    g_linux_abi64_kill_unavailable_count = 0u;
    g_linux_abi64_kill_denial_count = 0u;
    g_linux_abi64_kill_last_syscall = 0u;
    g_linux_abi64_kill_last_target = PROCESS64_INVALID_PID;
    g_linux_abi64_kill_last_signal = 0u;
    g_linux_abi64_kill_last_result = 0u;
    g_linux_abi64_signal_pending_count = 0u;
    g_linux_abi64_signal_delivery_count = 0u;
    g_linux_abi64_signal_masked_count = 0u;
    g_linux_abi64_signal_delivery_denial_count = 0u;
    g_linux_abi64_signal_delivery_fault_count = 0u;
    g_linux_abi64_signal_delivery_last_signal = 0u;
    g_linux_abi64_signal_delivery_last_handler = 0ull;
    g_linux_abi64_signal_delivery_last_frame = 0ull;
    g_linux_abi64_signal_delivery_last_saved_rip = 0ull;
    g_linux_abi64_signal_delivery_last_saved_rsp = 0ull;
    g_linux_abi64_signal_delivery_last_mask = 0ull;
    g_linux_abi64_signal_delivery_last_result = 0u;
    g_linux_abi64_rt_sigreturn_count = 0u;
    g_linux_abi64_rt_sigreturn_denial_count = 0u;
    g_linux_abi64_rt_sigreturn_fault_count = 0u;
    g_linux_abi64_rt_sigreturn_last_frame = 0ull;
    g_linux_abi64_rt_sigreturn_last_rip = 0ull;
    g_linux_abi64_rt_sigreturn_last_rsp = 0ull;
    g_linux_abi64_rt_sigreturn_last_mask = 0ull;
    g_linux_abi64_rt_sigreturn_last_rax = 0ull;
    g_linux_abi64_rt_sigreturn_last_result = 0u;
    g_linux_abi64_getrandom_state = 0x6C696D69746C6573ull;
    g_linux_abi64_getrandom_count = 0u;
    g_linux_abi64_getrandom_byte_count = 0u;
    g_linux_abi64_getrandom_denial_count = 0u;
    g_linux_abi64_getrandom_fault_count = 0u;
    g_linux_abi64_getrandom_last_byte_count = 0u;
    g_linux_abi64_getrandom_last_checksum = 0u;
    g_linux_abi64_getrandom_last_flags = 0u;
    g_linux_abi64_getrandom_last_result = 0u;
    g_linux_abi64_getpid_count = 0u;
    g_linux_abi64_getpid_denial_count = 0u;
    g_linux_abi64_geteuid_count = 0u;
    g_linux_abi64_geteuid_denial_count = 0u;
    g_linux_abi64_getppid_count = 0u;
    g_linux_abi64_getppid_denial_count = 0u;
    g_linux_abi64_gettid_count = 0u;
    g_linux_abi64_gettid_denial_count = 0u;
    g_linux_abi64_ioctl_count = 0u;
    g_linux_abi64_ioctl_tty_count = 0u;
    g_linux_abi64_ioctl_enotty_count = 0u;
    g_linux_abi64_ioctl_enosys_count = 0u;
    g_linux_abi64_ioctl_denial_count = 0u;
    g_linux_abi64_ioctl_last_fd = 0u;
    g_linux_abi64_ioctl_last_request = 0u;
    g_linux_abi64_ioctl_last_result = 0u;
    g_linux_abi64_prctl_count = 0u;
    g_linux_abi64_prctl_set_name_count = 0u;
    g_linux_abi64_prctl_get_name_count = 0u;
    g_linux_abi64_prctl_enosys_count = 0u;
    g_linux_abi64_prctl_denial_count = 0u;
    g_linux_abi64_prctl_fault_count = 0u;
    g_linux_abi64_prctl_last_option = 0u;
    g_linux_abi64_prctl_last_result = 0u;
    g_linux_abi64_arch_prctl_count = 0u;
    g_linux_abi64_arch_prctl_set_count = 0u;
    g_linux_abi64_arch_prctl_get_count = 0u;
    g_linux_abi64_arch_prctl_denial_count = 0u;
    g_linux_abi64_arch_prctl_fault_count = 0u;
    g_linux_abi64_set_tid_address_count = 0u;
    g_linux_abi64_set_tid_address_denial_count = 0u;
    g_linux_abi64_clock_gettime_count = 0u;
    g_linux_abi64_clock_gettime_denial_count = 0u;
    g_linux_abi64_clock_gettime_fault_count = 0u;
    g_linux_abi64_exit_count = 0u;
    g_linux_abi64_exit_group_count = 0u;
    g_linux_abi64_exit_denial_count = 0u;
    g_linux_abi64_last_exit_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_last_exit_code = 0u;
    g_linux_abi64_last_exit_vma_regions = 0u;
    g_linux_abi64_last_exit_fd_entries = 0u;
    g_linux_abi64_last_exit_persona_released = 0u;
    g_linux_abi64_last_exit_audit_released = 0u;
    g_linux_abi64_last_exit_audit_recorded = 0u;
    g_linux_abi64_last_exit_detached_vma = 0;
    g_linux_abi64_last_exit_detached_fd = 0;
    g_linux_abi64_last_exit_detached_audit = 0;
    g_linux_abi64_initialized = 1u;
}

linux_abi64_handler_t *linux_abi64_dispatch_table(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return g_linux_abi64_dispatch_table;
}

static u32 linux_abi64_repair_dispatch_root(u32 pid)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u64 root = paging64_process_root_physical(pid);

    if (root == 0ull)
    {
        return 1u;
    }
    if (paging64_current_root_physical() == (root & 0xFFFFFFFFFFFFF000ull))
    {
        if (paging64_reload_current_root() == 0u)
        {
            ++g_linux_abi64_dispatch_root_denial_count;
            return 0u;
        }
        ++g_linux_abi64_dispatch_root_reload_count;
        return 1u;
    }
    if (paging64_switch_to_process_root(pid, 0x4C585352u) == 0u)
    {
        ++g_linux_abi64_dispatch_root_denial_count;
        return 0u;
    }

    ++g_linux_abi64_dispatch_root_repair_count;
    return 1u;
#else
    (void)pid;
    return 1u;
#endif
}

static u32 linux_abi64_restore_return_root(u32 pid)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return linux_abi64_repair_dispatch_root(pid);
#else
    (void)pid;
    return 1u;
#endif
}

u64 linux_abi64_dispatch(
    u32 pid,
    u32 syscall_number,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip)
{
    linux_abi64_handler_t handler;
    u32 unavailable_result;
    u64 unavailable_return;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    ++g_linux_abi64_dispatch_count;
    if (linux_abi64_repair_dispatch_root(pid) == 0u)
    {
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if (syscall_number >= LINUX_ABI64_SYSCALL_LIMIT)
    {
        ++g_linux_abi64_unimplemented_count;
        g_linux_abi64_unimplemented_last_syscall = syscall_number;
        g_linux_abi64_unimplemented_last_rip = rip;
        (void)persona64_record_unavailable_syscall(
            pid,
            PERSONA64_TYPE_LINUX_ELF,
            (u16)syscall_number,
            rip,
            &unavailable_result,
            &unavailable_return);
        (void)unavailable_result;
        return unavailable_return;
    }

    handler = g_linux_abi64_dispatch_table[syscall_number];
    if (handler == linux_abi64_unimplemented_stub)
    {
        ++g_linux_abi64_unimplemented_count;
        g_linux_abi64_unimplemented_last_syscall = syscall_number;
        g_linux_abi64_unimplemented_last_rip = rip;
        (void)persona64_record_unavailable_syscall(
            pid,
            PERSONA64_TYPE_LINUX_ELF,
            (u16)syscall_number,
            rip,
            &unavailable_result,
            &unavailable_return);
        (void)unavailable_result;
        return unavailable_return;
    }

    return handler(pid, rdi, rsi, rdx, r10, r8, r9, rip);
}

static u32 linux_abi64_range_overflows(u64 address, u64 byte_count)
{
    return ((byte_count != 0ull) && ((address + byte_count) < address)) ? 1u : 0u;
}

static u32 linux_abi64_user_page_present(u32 pid, u64 page)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (paging64_process_root_physical(pid) != 0ull)
    {
        return paging64_user_page_present_for_process(pid, page);
    }
#else
    (void)pid;
#endif
    return paging64_user_page_present(page);
}

static u32 linux_abi64_user_page_protection(u32 pid, u64 page)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (paging64_process_root_physical(pid) != 0ull)
    {
        return paging64_user_page_protection_for_process(pid, page);
    }
#else
    (void)pid;
#endif
    return paging64_user_page_protection(page);
}

static u32 linux_abi64_user_buffer_readable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }

    if ((address == 0ull)
        || (linux_abi64_range_overflows(address, (u64)byte_count) != 0u))
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
            || (linux_abi64_user_page_present(pid, page) == 0u)
            || ((linux_abi64_user_page_protection(pid, page) & PAGING64_USER_PROT_READ) == 0u))
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

static u32 linux_abi64_user_buffer_writable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }

    if ((address == 0ull)
        || (linux_abi64_range_overflows(address, (u64)byte_count) != 0u))
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
            || (linux_abi64_user_page_present(pid, page) == 0u)
            || ((linux_abi64_user_page_protection(pid, page) & PAGING64_USER_PROT_WRITE) == 0u))
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

static u32 linux_abi64_copy_user_path(
    u32 pid,
    u64 user_path,
    u8 *path,
    u32 max_path_bytes,
    u32 *path_byte_count)
{
    u32 index;

    if (path_byte_count != 0)
    {
        *path_byte_count = 0u;
    }

    if ((user_path == 0ull)
        || (path == 0)
        || (path_byte_count == 0)
        || (max_path_bytes == 0u))
    {
        return 0u;
    }

    for (index = 0u; index <= max_path_bytes; ++index)
    {
        u8 byte;

        if (linux_abi64_user_buffer_readable(pid, user_path + (u64)index, 1u) == 0u)
        {
            return 0u;
        }

        byte = *((volatile const u8 *)(u64)(user_path + (u64)index));
        if (byte == 0u)
        {
            *path_byte_count = index;
            return 1u;
        }

        if (index == max_path_bytes)
        {
            return 0u;
        }

        path[index] = byte;
    }

    return 0u;
}

static u32 linux_abi64_append_path_byte(
    u8 *output,
    u32 output_capacity,
    u32 *output_bytes,
    u8 byte)
{
    if ((output == 0) || (output_bytes == 0) || (*output_bytes >= output_capacity))
    {
        return 0u;
    }

    output[*output_bytes] = byte;
    ++(*output_bytes);
    return 1u;
}

static u32 linux_abi64_append_path_bytes(
    u8 *output,
    u32 output_capacity,
    u32 *output_bytes,
    const u8 *input,
    u32 input_bytes)
{
    u32 index;

    if ((output == 0) || (output_bytes == 0) || (input == 0))
    {
        return 0u;
    }

    for (index = 0u; index < input_bytes; ++index)
    {
        if (linux_abi64_append_path_byte(
                output,
                output_capacity,
                output_bytes,
                input[index]) == 0u)
        {
            return 0u;
        }
    }
    return 1u;
}

static u32 linux_abi64_path_segment_is_dot(const u8 *segment, u32 segment_bytes)
{
    return ((segment != 0) && (segment_bytes == 1u) && (segment[0] == (u8)'.')) ? 1u : 0u;
}

static u32 linux_abi64_path_segment_is_dotdot(const u8 *segment, u32 segment_bytes)
{
    return ((segment != 0)
        && (segment_bytes == 2u)
        && (segment[0] == (u8)'.')
        && (segment[1] == (u8)'.')) ? 1u : 0u;
}

static u32 linux_abi64_path_has_trailing_slash(const u8 *path, u32 path_bytes)
{
    u32 index;
    u32 non_slash_seen = 0u;

    if ((path == 0) || (path_bytes <= 1u) || (path[path_bytes - 1u] != (u8)'/'))
    {
        return 0u;
    }

    for (index = 0u; index < path_bytes; ++index)
    {
        if (path[index] != (u8)'/')
        {
            non_slash_seen = 1u;
            break;
        }
    }

    return non_slash_seen;
}

static u32 linux_abi64_normalize_absolute_path(
    const u8 *input,
    u32 input_bytes,
    u8 *output,
    u32 output_capacity,
    u32 *output_bytes)
{
    u32 input_index = 0u;
    u32 out_bytes = 0u;

    if (output_bytes != 0)
    {
        *output_bytes = 0u;
    }

    if ((input == 0)
        || (output == 0)
        || (output_bytes == 0)
        || (input_bytes == 0u)
        || (output_capacity == 0u)
        || (input[0] != (u8)'/'))
    {
        ++g_linux_abi64_path_fault_count;
        return 0u;
    }

    if (linux_abi64_append_path_byte(output, output_capacity, &out_bytes, (u8)'/') == 0u)
    {
        ++g_linux_abi64_path_fault_count;
        return 0u;
    }

    while (input_index < input_bytes)
    {
        const u8 *segment;
        u32 segment_bytes = 0u;

        while ((input_index < input_bytes) && (input[input_index] == (u8)'/'))
        {
            ++input_index;
        }
        if (input_index >= input_bytes)
        {
            break;
        }

        segment = &input[input_index];
        while ((input_index < input_bytes) && (input[input_index] != (u8)'/'))
        {
            ++input_index;
            ++segment_bytes;
        }

        if (linux_abi64_path_segment_is_dot(segment, segment_bytes) != 0u)
        {
            ++g_linux_abi64_path_dot_count;
            continue;
        }

        if (linux_abi64_path_segment_is_dotdot(segment, segment_bytes) != 0u)
        {
            ++g_linux_abi64_path_dotdot_count;
            if (out_bytes > 1u)
            {
                u32 cursor = out_bytes;
                while ((cursor > 1u) && (output[cursor - 1u] != (u8)'/'))
                {
                    --cursor;
                }
                out_bytes = (cursor > 1u) ? (cursor - 1u) : 1u;
            }
            continue;
        }

        if ((out_bytes > 1u)
            && (linux_abi64_append_path_byte(output, output_capacity, &out_bytes, (u8)'/') == 0u))
        {
            ++g_linux_abi64_path_fault_count;
            return 0u;
        }

        if (linux_abi64_append_path_bytes(
                output,
                output_capacity,
                &out_bytes,
                segment,
                segment_bytes) == 0u)
        {
            ++g_linux_abi64_path_fault_count;
            return 0u;
        }
    }

    *output_bytes = out_bytes;
    return 1u;
}

static u32 linux_abi64_canonicalize_path(
    u32 pid,
    u64 dirfd,
    const u8 *input,
    u32 input_bytes,
    u8 *output,
    u32 output_capacity,
    u32 *output_bytes)
{
    persona_context_t *context;
    u8 combined[LINUX_VFS64_MAX_PATH_BYTES];
    u32 combined_bytes = 0u;
    u8 fd_base[LINUX_VFS64_MAX_PATH_BYTES];
    u32 fd_base_bytes = 0u;
    const u8 *base_path = 0;
    u32 base_bytes = 0u;

    if (output_bytes != 0)
    {
        *output_bytes = 0u;
    }

    if ((input == 0)
        || (input_bytes == 0u)
        || (output == 0)
        || (output_bytes == 0)
        || (output_capacity == 0u)
        || (input_bytes > LINUX_VFS64_MAX_PATH_BYTES))
    {
        ++g_linux_abi64_path_fault_count;
        return 0u;
    }

    if (input[0] == (u8)'/')
    {
        return linux_abi64_normalize_absolute_path(
            input,
            input_bytes,
            output,
            output_capacity,
            output_bytes);
    }

    ++g_linux_abi64_path_relative_count;
    if (dirfd == LINUX_ABI64_AT_FDCWD)
    {
        context = persona64_context_for_process(pid);
        if ((context == 0)
            || (context->persona_type != PERSONA64_TYPE_LINUX_ELF)
            || (context->linux_cwd_length == 0u)
            || (context->linux_cwd_length >= PERSONA64_LINUX_CWD_MAX_BYTES)
            || (context->linux_cwd[0] != (u8)'/'))
        {
            ++g_linux_abi64_path_fault_count;
            return 0u;
        }
        base_path = &context->linux_cwd[0];
        base_bytes = context->linux_cwd_length;
    }
    else
    {
        if ((dirfd >= (u64)FD64_TABLE_LIMIT)
            || (linux_vfs64_fd_path(
                    pid,
                    (u32)dirfd,
                    fd_base,
                    (u32)sizeof(fd_base),
                    &fd_base_bytes) == 0u)
            || (linux_vfs64_path_is_directory(pid, fd_base, fd_base_bytes) == 0u))
        {
            ++g_linux_abi64_path_fault_count;
            return 0u;
        }
        base_path = fd_base;
        base_bytes = fd_base_bytes;
    }

    if (linux_abi64_append_path_bytes(
            combined,
            (u32)sizeof(combined),
            &combined_bytes,
            base_path,
            base_bytes) == 0u)
    {
        ++g_linux_abi64_path_fault_count;
        return 0u;
    }
    if ((combined_bytes > 1u)
        && (linux_abi64_append_path_byte(
                combined,
                (u32)sizeof(combined),
                &combined_bytes,
                (u8)'/') == 0u))
    {
        ++g_linux_abi64_path_fault_count;
        return 0u;
    }
    if (linux_abi64_append_path_bytes(
            combined,
            (u32)sizeof(combined),
            &combined_bytes,
            input,
            input_bytes) == 0u)
    {
        ++g_linux_abi64_path_fault_count;
        return 0u;
    }

    return linux_abi64_normalize_absolute_path(
        combined,
        combined_bytes,
        output,
        output_capacity,
        output_bytes);
}

static u32 linux_abi64_checksum_bytes(const u8 *bytes, u32 byte_count)
{
    u32 index;
    u32 checksum = 2166136261u;

    if (bytes == 0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum ^= (u32)bytes[index];
        checksum *= 16777619u;
    }

    return checksum;
}

static u32 linux_abi64_copy_user_exec_string(
    u32 pid,
    u64 user_string,
    char *target,
    u32 target_bytes)
{
    u32 index;

    if ((user_string == 0ull) || (target == 0) || (target_bytes == 0u))
    {
        return 0u;
    }

    for (index = 0u; index < target_bytes; ++index)
    {
        u8 byte;

        if (linux_abi64_user_buffer_readable(pid, user_string + (u64)index, 1u) == 0u)
        {
            return 0u;
        }

        byte = *((volatile const u8 *)(u64)(user_string + (u64)index));
        target[index] = (char)byte;
        if (byte == 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 linux_abi64_copy_user_string_vector(
    u32 pid,
    u64 user_vector,
    char storage[][ELF64_STACK_MAX_STRING_BYTES],
    const char **out_strings,
    u32 max_entries,
    u32 *out_count)
{
    u32 index;

    if (out_count != 0)
    {
        *out_count = 0u;
    }
    if ((storage == 0) || (out_strings == 0) || (out_count == 0) || (max_entries == 0u))
    {
        return LINUX_ABI64_EINVAL;
    }
    if (user_vector == 0ull)
    {
        return 0u;
    }

    for (index = 0u; index <= max_entries; ++index)
    {
        u64 user_string;

        if (linux_abi64_user_buffer_readable(pid, user_vector + ((u64)index * 8ull), 8u) == 0u)
        {
            return LINUX_ABI64_EFAULT;
        }

        user_string = *((volatile const u64 *)(u64)(user_vector + ((u64)index * 8ull)));
        if (user_string == 0ull)
        {
            *out_count = index;
            return 0u;
        }

        if (index == max_entries)
        {
            return LINUX_ABI64_E2BIG;
        }

        if (linux_abi64_copy_user_exec_string(
                pid,
                user_string,
                storage[index],
                ELF64_STACK_MAX_STRING_BYTES) == 0u)
        {
            return LINUX_ABI64_EFAULT;
        }
        out_strings[index] = storage[index];
    }

    return LINUX_ABI64_E2BIG;
}

static u32 linux_abi64_read_exec_binary(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u8 *binary,
    u32 binary_capacity,
    u32 *out_binary_bytes)
{
    fd64_stat_t stat;
    u32 bytes_read;
#if !defined(LIMITLESS_X64_UEFI_KERNEL) || !LIMITLESS_X64_UEFI_KERNEL
    u32 fd_number;
    u32 close_ok;
#endif

    if (out_binary_bytes != 0)
    {
        *out_binary_bytes = 0u;
    }
    if ((path == 0)
        || (path_byte_count == 0u)
        || (binary == 0)
        || (binary_capacity == 0u)
        || (out_binary_bytes == 0))
    {
        return LINUX_ABI64_EINVAL;
    }

    stat.size = 0ull;
    if ((linux_vfs64_stat(pid, path, path_byte_count, &stat) == 0u)
        || (stat.size == 0ull)
        || (stat.size > (u64)binary_capacity)
        || (stat.size > 0xFFFFFFFFull))
    {
        return (stat.size > (u64)binary_capacity) ? LINUX_ABI64_E2BIG : LINUX_ABI64_EINVAL;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if ((linux_vfs64_read_file_all(
            pid,
            path,
            path_byte_count,
            binary,
            (u32)stat.size,
            &bytes_read) == 0u)
        || (bytes_read != (u32)stat.size))
    {
        return LINUX_ABI64_EINVAL;
    }
#else
    fd_number = linux_vfs64_open(pid, path, path_byte_count, 0u, 0u);
    if (fd_number == FD64_INVALID_FD)
    {
        return LINUX_ABI64_ENOENT;
    }

    bytes_read = linux_vfs64_read_fd(pid, fd_number, binary, (u32)stat.size);
    close_ok = fd64_close(pid, fd_number);
    if ((bytes_read == LINUX_VFS64_INVALID_RESULT)
        || (bytes_read != (u32)stat.size)
        || (close_ok == 0u))
    {
        return LINUX_ABI64_EINVAL;
    }
#endif

    *out_binary_bytes = bytes_read;
    return 0u;
}

static u32 linux_abi64_validate_static_exec(
    const u8 *binary,
    u32 binary_bytes,
    linux_abi64_exec_validation_t *validation)
{
    elf64_program_header_t phdrs[ELF64_MAX_PROGRAM_HEADERS];
    u32 index;

    if (validation != 0)
    {
        validation->ok = 0u;
        validation->error = ELF64_ERROR_NULL;
    }
    if ((binary == 0) || (binary_bytes == 0u) || (validation == 0))
    {
        return LINUX_ABI64_EINVAL;
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

    if (elf64_parse_header(binary, binary_bytes, &validation->header) != ELF64_OK)
    {
        validation->error = validation->header.error;
        return LINUX_ABI64_EINVAL;
    }
    if (elf64_parse_phdrs(
            binary,
            binary_bytes,
            &validation->header,
            phdrs,
            ELF64_MAX_PROGRAM_HEADERS,
            &validation->summary) != ELF64_OK)
    {
        validation->error = validation->summary.error;
        return LINUX_ABI64_EINVAL;
    }

    if ((validation->header.type != ELF64_TYPE_EXEC)
        || (validation->summary.load_count == 0u)
        || (validation->summary.interp_count != 0u)
        || (validation->summary.dynamic_count != 0u))
    {
        validation->error = ELF64_ERROR_LAUNCH_DYNAMIC;
        return LINUX_ABI64_ENOSYS;
    }

    validation->ok = 1u;
    validation->error = ELF64_ERROR_NONE;
    return 0u;
}

static u64 linux_abi64_execve_error_return(
    u32 pid,
    u16 syscall_number,
    u32 error_code,
    u64 rip,
    u32 fault)
{
    ++g_linux_abi64_execve_denial_count;
    if (fault != 0u)
    {
        ++g_linux_abi64_execve_fault_count;
    }
    g_linux_abi64_execve_last_error = error_code;
    (void)persona_audit64_record(
        pid,
        (error_code == LINUX_ABI64_ESRCH)
            ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
            : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        syscall_number,
        error_code,
        rip);
    return LINUX_ABI64_ERROR_RETURN(error_code);
}

static void linux_abi64_copy_to_user(u64 user_buffer, const u8 *source, u32 byte_count)
{
    volatile u8 *target = (volatile u8 *)(u64)user_buffer;
    u32 index;

    if ((source == 0) || (byte_count == 0u))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = source[index];
    }
}

static void linux_abi64_copy_from_user(u8 *target, u64 user_buffer, u32 byte_count)
{
    volatile const u8 *source = (volatile const u8 *)(u64)user_buffer;
    u32 index;

    if ((target == 0) || (byte_count == 0u))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = source[index];
    }
}

static u32 linux_abi64_copy_iovec_from_user(
    u32 pid,
    u64 user_iov,
    u32 iov_index,
    linux_abi64_iovec_t *out_iov)
{
    u64 entry_address;
    volatile const u64 *entry;

    if (out_iov != 0)
    {
        out_iov->iov_base = 0ull;
        out_iov->iov_len = 0ull;
    }
    if ((out_iov == 0)
        || (iov_index >= LINUX_ABI64_IOV_MAX)
        || (user_iov > (0xFFFFFFFFFFFFFFFFull - ((u64)iov_index * LINUX_ABI64_IOVEC_BYTES))))
    {
        return 0u;
    }

    entry_address = user_iov + ((u64)iov_index * LINUX_ABI64_IOVEC_BYTES);
    if (linux_abi64_user_buffer_readable(pid, entry_address, LINUX_ABI64_IOVEC_BYTES) == 0u)
    {
        return 0u;
    }

    entry = (volatile const u64 *)(u64)entry_address;
    out_iov->iov_base = entry[0];
    out_iov->iov_len = entry[1];
    return 1u;
}

static u64 linux_abi64_page_align_up(u64 value)
{
    u64 mask = (u64)VMA64_PAGE_BYTES - 1ull;

    if ((value + mask) < value)
    {
        return 0ull;
    }

    return (value + mask) & ~mask;
}

static u32 linux_abi64_mmap_prot_to_vma(u64 prot)
{
    u32 vma_prot = 0u;

    if ((prot & ~((u64)LINUX_ABI64_PROT_READ
            | (u64)LINUX_ABI64_PROT_WRITE
            | (u64)LINUX_ABI64_PROT_EXEC)) != 0ull)
    {
        return 0u;
    }
    if ((prot & (u64)LINUX_ABI64_PROT_READ) != 0ull)
    {
        vma_prot |= VMA64_PROT_READ;
    }
    if ((prot & (u64)LINUX_ABI64_PROT_WRITE) != 0ull)
    {
        vma_prot |= VMA64_PROT_WRITE;
    }
    if ((prot & (u64)LINUX_ABI64_PROT_EXEC) != 0ull)
    {
        vma_prot |= VMA64_PROT_EXECUTE;
    }

    return vma_prot;
}

static u32 linux_abi64_mmap_flags_to_vma(u64 flags)
{
    u32 map_type = (u32)(flags & 0x3ull);
    u32 vma_flags = VMA64_MAP_ANONYMOUS;

    if ((flags & ~(u64)LINUX_ABI64_MAP_SUPPORTED_MASK) != 0ull)
    {
        return 0u;
    }
    if ((flags & (u64)LINUX_ABI64_MAP_ANONYMOUS) == 0ull)
    {
        return 0u;
    }
    if (map_type == LINUX_ABI64_MAP_PRIVATE)
    {
        vma_flags |= VMA64_MAP_PRIVATE;
    }
    else if (map_type == LINUX_ABI64_MAP_SHARED)
    {
        vma_flags |= VMA64_MAP_SHARED;
    }
    else
    {
        return 0u;
    }
    if ((flags & (u64)LINUX_ABI64_MAP_FIXED) != 0ull)
    {
        vma_flags |= VMA64_MAP_FIXED;
    }

    return vma_flags;
}

static u32 linux_abi64_open_flags_to_fd(u64 linux_flags, u32 *fd_flags_out)
{
    u32 access_mode = (u32)(linux_flags & (u64)LINUX_ABI64_O_ACCMODE);
    u32 fd_flags = 0u;

    if (fd_flags_out != 0)
    {
        *fd_flags_out = 0u;
    }

    if ((fd_flags_out == 0)
        || ((linux_flags & ~((u64)LINUX_ABI64_O_ACCMODE
                | (u64)LINUX_ABI64_O_CREAT
                | (u64)LINUX_ABI64_O_NOFOLLOW
                | (u64)LINUX_ABI64_O_NONBLOCK
                | (u64)LINUX_ABI64_O_LARGEFILE
                | (u64)LINUX_ABI64_O_DIRECTORY
                | (u64)LINUX_ABI64_O_CLOEXEC)) != 0ull)
        || (access_mode == LINUX_ABI64_O_ACCMODE))
    {
        return 0u;
    }

    if ((linux_flags & (u64)LINUX_ABI64_O_CLOEXEC) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_CLOEXEC;
    }
    if ((linux_flags & (u64)LINUX_ABI64_O_CREAT) != 0ull)
    {
        fd_flags |= LINUX_VFS64_OPEN_CREATE;
    }
    if ((linux_flags & (u64)LINUX_ABI64_O_NOFOLLOW) != 0ull)
    {
        fd_flags |= LINUX_VFS64_OPEN_NOFOLLOW;
    }
    if ((linux_flags & (u64)LINUX_ABI64_O_NONBLOCK) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_NONBLOCK;
    }

    *fd_flags_out = fd_flags;
    return 1u;
}

static u32 linux_abi64_pipe_flags_to_fd(u64 linux_flags, u32 *fd_flags_out)
{
    u32 fd_flags = 0u;

    if (fd_flags_out != 0)
    {
        *fd_flags_out = 0u;
    }

    if ((fd_flags_out == 0)
        || ((linux_flags & ~((u64)LINUX_ABI64_O_NONBLOCK
                | (u64)LINUX_ABI64_O_CLOEXEC)) != 0ull))
    {
        return 0u;
    }

    if ((linux_flags & (u64)LINUX_ABI64_O_CLOEXEC) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_CLOEXEC;
    }
    if ((linux_flags & (u64)LINUX_ABI64_O_NONBLOCK) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_NONBLOCK;
    }

    *fd_flags_out = fd_flags;
    return 1u;
}

static u32 linux_abi64_dup3_flags_to_fd(u64 linux_flags, u32 *fd_flags_out)
{
    u32 fd_flags = 0u;

    if (fd_flags_out != 0)
    {
        *fd_flags_out = 0u;
    }

    if ((fd_flags_out == 0)
        || ((linux_flags & ~(u64)LINUX_ABI64_O_CLOEXEC) != 0ull))
    {
        return 0u;
    }

    if ((linux_flags & (u64)LINUX_ABI64_O_CLOEXEC) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_CLOEXEC;
    }

    *fd_flags_out = fd_flags;
    return 1u;
}

static u64 linux_abi64_fcntl_descriptor_flags_from_fd(u32 fd_flags)
{
    return ((fd_flags & FD64_FLAG_O_CLOEXEC) != 0u)
        ? (u64)LINUX_ABI64_FD_CLOEXEC
        : 0ull;
}

static u64 linux_abi64_fcntl_status_flags_from_fd(u32 fd_flags)
{
    return ((fd_flags & FD64_FLAG_O_NONBLOCK) != 0u)
        ? (u64)LINUX_ABI64_O_NONBLOCK
        : 0ull;
}

static u32 linux_abi64_fcntl_setfl_to_fd(u64 linux_flags, u32 current_flags, u32 *fd_flags_out)
{
    u32 fd_flags;

    if (fd_flags_out != 0)
    {
        *fd_flags_out = 0u;
    }

    if ((fd_flags_out == 0)
        || ((linux_flags & ~((u64)LINUX_ABI64_O_ACCMODE
                | (u64)LINUX_ABI64_O_NONBLOCK)) != 0ull))
    {
        return 0u;
    }

    fd_flags = current_flags & FD64_FLAG_O_CLOEXEC;
    if ((linux_flags & (u64)LINUX_ABI64_O_NONBLOCK) != 0ull)
    {
        fd_flags |= FD64_FLAG_O_NONBLOCK;
    }

    *fd_flags_out = fd_flags;
    return 1u;
}

static u32 linux_abi64_decode_lseek_offset(u64 raw_offset, s32 *offset_out)
{
    if (offset_out == 0)
    {
        return 0u;
    }

    if (raw_offset <= 0x000000007FFFFFFFull)
    {
        *offset_out = (s32)raw_offset;
        return 1u;
    }

    if (raw_offset >= 0xFFFFFFFF80000000ull)
    {
        *offset_out = (s32)(u32)raw_offset;
        return 1u;
    }

    *offset_out = 0;
    return 0u;
}

static void linux_abi64_note_stat_success(u32 syscall_number)
{
    if (syscall_number == LINUX_ABI64_SYSCALL_FSTAT)
    {
        ++g_linux_abi64_fstat_count;
    }
    else if (syscall_number == LINUX_ABI64_SYSCALL_NEWFSTATAT)
    {
        ++g_linux_abi64_newfstatat_count;
    }
    else
    {
        ++g_linux_abi64_stat_count;
    }
}

static void linux_abi64_note_stat_denial(u32 syscall_number)
{
    if (syscall_number == LINUX_ABI64_SYSCALL_FSTAT)
    {
        ++g_linux_abi64_fstat_denial_count;
    }
    else if (syscall_number == LINUX_ABI64_SYSCALL_NEWFSTATAT)
    {
        ++g_linux_abi64_newfstatat_denial_count;
    }
    else
    {
        ++g_linux_abi64_stat_denial_count;
    }
}

static void linux_abi64_note_stat_fault(u32 syscall_number)
{
    if (syscall_number == LINUX_ABI64_SYSCALL_FSTAT)
    {
        ++g_linux_abi64_fstat_fault_count;
    }
    else if (syscall_number == LINUX_ABI64_SYSCALL_NEWFSTATAT)
    {
        ++g_linux_abi64_newfstatat_fault_count;
    }
    else
    {
        ++g_linux_abi64_stat_fault_count;
    }
}

static void linux_abi64_zero_stat(linux_abi64_stat_t *stat_buf)
{
    u32 index;

    if (stat_buf == 0)
    {
        return;
    }

    stat_buf->st_dev = 0ull;
    stat_buf->st_ino = 0ull;
    stat_buf->st_nlink = 0ull;
    stat_buf->st_mode = 0u;
    stat_buf->st_uid = 0u;
    stat_buf->st_gid = 0u;
    stat_buf->__pad0 = 0u;
    stat_buf->st_rdev = 0ull;
    stat_buf->st_size = 0ull;
    stat_buf->st_blksize = 0ull;
    stat_buf->st_blocks = 0ull;
    stat_buf->st_atime = 0ull;
    stat_buf->st_atime_nsec = 0ull;
    stat_buf->st_mtime = 0ull;
    stat_buf->st_mtime_nsec = 0ull;
    stat_buf->st_ctime = 0ull;
    stat_buf->st_ctime_nsec = 0ull;
    for (index = 0u; index < 3u; ++index)
    {
        stat_buf->__unused[index] = 0ull;
    }
}

static u32 linux_abi64_write_stat_to_user(u32 pid, u64 user_stat, const fd64_stat_t *fd_stat)
{
    linux_abi64_stat_t linux_stat;

    if ((fd_stat == 0)
        || ((u32)sizeof(linux_abi64_stat_t) != LINUX_ABI64_STAT_BYTES)
        || (linux_abi64_user_buffer_writable(pid, user_stat, LINUX_ABI64_STAT_BYTES) == 0u))
    {
        return 0u;
    }

    linux_abi64_zero_stat(&linux_stat);
    linux_stat.st_dev = (fd_stat->device_id != 0ull)
        ? fd_stat->device_id
        : ((((u64)fd_stat->owner_id) << 32) | (u64)fd_stat->fd_type);
    linux_stat.st_ino = (fd_stat->inode != 0ull)
        ? fd_stat->inode
        : (u64)fd_stat->capability_handle;
    linux_stat.st_nlink = (fd_stat->link_count != 0u) ? (u64)fd_stat->link_count : 1ull;
    linux_stat.st_mode = fd_stat->mode;
    linux_stat.st_uid = fd_stat->owner_id;
    linux_stat.st_gid = 0u;
    linux_stat.st_rdev = 0ull;
    linux_stat.st_size = fd_stat->size;
    linux_stat.st_blksize = (fd_stat->block_size != 0u) ? (u64)fd_stat->block_size : 4096ull;
    linux_stat.st_blocks = fd_stat->blocks;
    linux_stat.st_atime = fd_stat->mtime;
    linux_stat.st_mtime = fd_stat->mtime;
    linux_stat.st_ctime = fd_stat->mtime;

    linux_abi64_copy_to_user(user_stat, (const u8 *)&linux_stat, LINUX_ABI64_STAT_BYTES);
    return 1u;
}

static u32 linux_abi64_ioctl_is_terminal_query(u32 request)
{
    return ((request == LINUX_ABI64_TCGETS)
        || (request == LINUX_ABI64_TIOCGETD)
        || (request == LINUX_ABI64_TIOCGETP)
        || (request == LINUX_ABI64_TIOCGETC)
        || (request == LINUX_ABI64_TIOCGWINSZ)
        || (request == LINUX_ABI64_TIOCGPGRP)
        || (request == LINUX_ABI64_TIOCOUTQ)
        || (request == LINUX_ABI64_TIOCINQ)
        || (request == LINUX_ABI64_TIOCGSID))
        ? 1u
        : 0u;
}

static void linux_abi64_cpu_pause(void)
{
    __asm__ __volatile__("pause");
}

static u32 linux_abi64_ioctl_fd_is_terminal(u32 pid, u32 fd_number)
{
    u32 fd_type;
    u32 capability;

    fd_type = fd64_entry_type(pid, fd_number);
    if (fd_type != FD64_TYPE_DEVICE)
    {
        return 0u;
    }

    capability = fd64_entry_capability(pid, fd_number);
    return (linux_vfs64_device_type_from_handle(capability) == LINUX_VFS64_DEVICE_UNKNOWN)
        ? 1u
        : 0u;
}

static u64 linux_abi64_ioctl_winsize(
    u32 pid,
    u32 fd_number,
    u64 argument,
    u64 rip)
{
    u8 winsize[LINUX_ABI64_WINSIZE_BYTES];

    if ((fd64_entry_type(pid, fd_number) == FD64_TYPE_EMPTY)
        || (linux_abi64_ioctl_fd_is_terminal(pid, fd_number) == 0u))
    {
        ++g_linux_abi64_ioctl_enotty_count;
        g_linux_abi64_ioctl_last_result = LINUX_ABI64_ENOTTY;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_IOCTL,
            LINUX_ABI64_ENOTTY,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOTTY);
    }

    if (linux_abi64_user_buffer_writable(pid, argument, LINUX_ABI64_WINSIZE_BYTES) == 0u)
    {
        ++g_linux_abi64_ioctl_denial_count;
        g_linux_abi64_ioctl_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_IOCTL,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    winsize[0] = (u8)(LINUX_ABI64_TERMINAL_ROWS & 0xFFu);
    winsize[1] = (u8)((LINUX_ABI64_TERMINAL_ROWS >> 8) & 0xFFu);
    winsize[2] = (u8)(LINUX_ABI64_TERMINAL_COLUMNS & 0xFFu);
    winsize[3] = (u8)((LINUX_ABI64_TERMINAL_COLUMNS >> 8) & 0xFFu);
    winsize[4] = 0u;
    winsize[5] = 0u;
    winsize[6] = 0u;
    winsize[7] = 0u;
    linux_abi64_copy_to_user(argument, winsize, LINUX_ABI64_WINSIZE_BYTES);

    ++g_linux_abi64_ioctl_tty_count;
    g_linux_abi64_ioctl_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_IOCTL,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_ioctl(u32 pid, u64 fd_number, u64 request, u64 argument, u64 rip)
{
    u32 request32 = (u32)(request & 0xFFFFFFFFull);
    u32 result;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_ioctl_last_fd = (u32)(fd_number & 0xFFFFFFFFull);
    g_linux_abi64_ioctl_last_request = request32;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_ioctl_denial_count;
        g_linux_abi64_ioctl_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_IOCTL,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    ++g_linux_abi64_ioctl_count;
    if ((request32 == LINUX_ABI64_TIOCGWINSZ) && (fd_number <= 0xFFFFFFFFull))
    {
        return linux_abi64_ioctl_winsize(pid, (u32)fd_number, argument, rip);
    }

    if (linux_abi64_ioctl_is_terminal_query(request32) != 0u)
    {
        result = LINUX_ABI64_ENOTTY;
        ++g_linux_abi64_ioctl_enotty_count;
    }
    else
    {
        result = LINUX_ABI64_ENOSYS;
        ++g_linux_abi64_ioctl_enosys_count;
    }

    g_linux_abi64_ioctl_last_result = result;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_IOCTL,
        result,
        rip);
    return LINUX_ABI64_ERROR_RETURN(result);
}

static u64 linux_abi64_open_common(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 linux_flags,
    u64 mode,
    u64 rip,
    u32 syscall_number)
{
    u8 user_path_bytes[LINUX_VFS64_MAX_PATH_BYTES + 1u];
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 user_path_byte_count;
    u32 path_byte_count;
    u32 fd_flags;
    u32 fd_number;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_open_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (linux_abi64_open_flags_to_fd(linux_flags, &fd_flags) == 0u)
    {
        ++g_linux_abi64_open_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_copy_user_path(
            pid,
            user_path,
            user_path_bytes,
            LINUX_VFS64_MAX_PATH_BYTES,
            &user_path_byte_count) == 0u)
    {
        ++g_linux_abi64_open_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }
    if (linux_abi64_canonicalize_path(
            pid,
            dirfd,
            user_path_bytes,
            user_path_byte_count,
            path,
            (u32)sizeof(path),
            &path_byte_count) == 0u)
    {
        ++g_linux_abi64_open_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            LINUX_ABI64_ENOENT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOENT);
    }

    fd_number = linux_vfs64_open(pid, path, path_byte_count, fd_flags, (u32)mode);
    if (fd_number == FD64_INVALID_FD)
    {
        ++g_linux_abi64_open_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            LINUX_ABI64_ENOENT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOENT);
    }

    if (syscall_number == LINUX_ABI64_SYSCALL_OPENAT)
    {
        ++g_linux_abi64_openat_count;
    }
    else
    {
        ++g_linux_abi64_open_count;
    }
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)fd_number;
}

u64 linux_abi64_sys_open(u32 pid, u64 user_path, u64 flags, u64 mode, u64 rip)
{
    return linux_abi64_open_common(
        pid,
        LINUX_ABI64_AT_FDCWD,
        user_path,
        flags,
        mode,
        rip,
        LINUX_ABI64_SYSCALL_OPEN);
}

u64 linux_abi64_sys_openat(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 flags,
    u64 mode,
    u64 rip)
{
    return linux_abi64_open_common(
        pid,
        dirfd,
        user_path,
        flags,
        mode,
        rip,
        LINUX_ABI64_SYSCALL_OPENAT);
}

u64 linux_abi64_sys_close(u32 pid, u64 fd_number, u64 rip)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_close_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLOSE,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_close(pid, (u32)fd_number) == 0u))
    {
        ++g_linux_abi64_close_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLOSE,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    (void)linux_vfs64_forget_fd_path(pid, (u32)fd_number);
    ++g_linux_abi64_close_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_CLOSE,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_lseek(u32 pid, u64 fd_number, u64 offset, u64 whence, u64 rip)
{
    s32 decoded_offset;
    u32 fd_index;
    u64 new_offset;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_lseek_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_LSEEK,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_lseek_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_LSEEK,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    if (((whence != (u64)LINUX_ABI64_SEEK_SET)
            && (whence != (u64)LINUX_ABI64_SEEK_CUR)
            && (whence != (u64)LINUX_ABI64_SEEK_END))
        || (linux_abi64_decode_lseek_offset(offset, &decoded_offset) == 0u))
    {
        ++g_linux_abi64_lseek_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_LSEEK,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    fd_index = (u32)fd_number;
    new_offset = fd64_seek(pid, fd_index, decoded_offset, (u32)whence);
    if (new_offset == FD64_SEEK_ERROR)
    {
        ++g_linux_abi64_lseek_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_LSEEK,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    ++g_linux_abi64_lseek_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_LSEEK,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return new_offset;
}

static u64 linux_abi64_fstat_common(
    u32 pid,
    u64 fd_number,
    u64 user_stat,
    u64 rip,
    u32 syscall_number)
{
    fd64_stat_t fd_stat;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        linux_abi64_note_stat_denial(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (linux_abi64_user_buffer_writable(pid, user_stat, LINUX_ABI64_STAT_BYTES) == 0u)
    {
        linux_abi64_note_stat_fault(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            (u16)syscall_number,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if ((fd_number >= (u64)FD64_TABLE_LIMIT)
        || (linux_vfs64_fstat(pid, (u32)fd_number, &fd_stat) == 0u))
    {
        linux_abi64_note_stat_denial(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)syscall_number,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    if (linux_abi64_write_stat_to_user(pid, user_stat, &fd_stat) == 0u)
    {
        linux_abi64_note_stat_fault(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            (u16)syscall_number,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    linux_abi64_note_stat_success(syscall_number);
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

static u64 linux_abi64_stat_path_common(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 user_stat,
    u64 rip,
    u32 syscall_number,
    u32 nofollow)
{
    fd64_stat_t fd_stat;
    u8 user_path_bytes[LINUX_VFS64_MAX_PATH_BYTES + 1u];
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 user_path_byte_count;
    u32 path_byte_count;
    u32 stat_ok;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        linux_abi64_note_stat_denial(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (linux_abi64_user_buffer_writable(pid, user_stat, LINUX_ABI64_STAT_BYTES) == 0u)
    {
        linux_abi64_note_stat_fault(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            (u16)syscall_number,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if (linux_abi64_copy_user_path(
            pid,
            user_path,
            user_path_bytes,
            LINUX_VFS64_MAX_PATH_BYTES,
            &user_path_byte_count) == 0u)
    {
        linux_abi64_note_stat_fault(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            (u16)syscall_number,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }
    if (linux_abi64_canonicalize_path(
            pid,
            dirfd,
            user_path_bytes,
            user_path_byte_count,
            path,
            (u32)sizeof(path),
            &path_byte_count) == 0u)
    {
        linux_abi64_note_stat_denial(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            (u16)syscall_number,
            LINUX_ABI64_ENOENT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOENT);
    }

    stat_ok = (nofollow != 0u)
        ? linux_vfs64_lstat(pid, path, path_byte_count, &fd_stat)
        : linux_vfs64_stat(pid, path, path_byte_count, &fd_stat);
    if (stat_ok == 0u)
    {
        linux_abi64_note_stat_denial(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            (u16)syscall_number,
            LINUX_ABI64_ENOENT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOENT);
    }

    if (linux_abi64_write_stat_to_user(pid, user_stat, &fd_stat) == 0u)
    {
        linux_abi64_note_stat_fault(syscall_number);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            (u16)syscall_number,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    linux_abi64_note_stat_success(syscall_number);
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_stat(u32 pid, u64 user_path, u64 user_stat, u64 rip)
{
    return linux_abi64_stat_path_common(
        pid,
        LINUX_ABI64_AT_FDCWD,
        user_path,
        user_stat,
        rip,
        LINUX_ABI64_SYSCALL_STAT,
        0u);
}

u64 linux_abi64_sys_lstat(u32 pid, u64 user_path, u64 user_stat, u64 rip)
{
    return linux_abi64_stat_path_common(
        pid,
        LINUX_ABI64_AT_FDCWD,
        user_path,
        user_stat,
        rip,
        LINUX_ABI64_SYSCALL_LSTAT,
        1u);
}

u64 linux_abi64_sys_fstat(u32 pid, u64 fd_number, u64 user_stat, u64 rip)
{
    return linux_abi64_fstat_common(
        pid,
        fd_number,
        user_stat,
        rip,
        LINUX_ABI64_SYSCALL_FSTAT);
}

u64 linux_abi64_sys_readlink(
    u32 pid,
    u64 user_path,
    u64 user_buffer,
    u64 byte_count,
    u64 rip)
{
    u8 user_path_bytes[LINUX_VFS64_MAX_PATH_BYTES + 1u];
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
    u8 target[LINUX_VFS64_MAX_PATH_BYTES];
    u32 user_path_byte_count = 0u;
    u32 path_byte_count = 0u;
    u32 copy_capacity;
    u32 copied;
    u32 index;

    g_linux_abi64_readlink_last_result = 0u;

    if ((byte_count == 0ull) || (byte_count > (u64)LINUX_VFS64_MAX_PATH_BYTES))
    {
        ++g_linux_abi64_readlink_denial_count;
        g_linux_abi64_readlink_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READLINK,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    copy_capacity = (u32)byte_count;
    if ((linux_abi64_copy_user_path(
            pid,
            user_path,
            user_path_bytes,
            LINUX_VFS64_MAX_PATH_BYTES,
            &user_path_byte_count) == 0u)
        || (linux_abi64_user_buffer_writable(pid, user_buffer, copy_capacity) == 0u))
    {
        ++g_linux_abi64_readlink_fault_count;
        g_linux_abi64_readlink_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READLINK,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if (linux_abi64_canonicalize_path(
            pid,
            LINUX_ABI64_AT_FDCWD,
            user_path_bytes,
            user_path_byte_count,
            path,
            (u32)sizeof(path),
            &path_byte_count) == 0u)
    {
        ++g_linux_abi64_readlink_denial_count;
        g_linux_abi64_readlink_last_result = LINUX_ABI64_ENOENT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READLINK,
            LINUX_ABI64_ENOENT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOENT);
    }

    copied = linux_vfs64_readlink(pid, path, path_byte_count, target, copy_capacity);
    if (copied == LINUX_VFS64_INVALID_RESULT)
    {
        ++g_linux_abi64_readlink_denial_count;
        g_linux_abi64_readlink_last_result = LINUX_ABI64_ENOENT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READLINK,
            LINUX_ABI64_ENOENT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOENT);
    }

    for (index = 0u; index < copied; ++index)
    {
        *((volatile u8 *)(u64)(user_buffer + (u64)index)) = target[index];
    }

    ++g_linux_abi64_readlink_count;
    g_linux_abi64_readlink_byte_count += copied;
    g_linux_abi64_readlink_last_result = copied;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_READLINK,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)copied;
}

u64 linux_abi64_sys_newfstatat(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 user_stat,
    u64 flags,
    u64 rip)
{
    if ((flags
            & ~((u64)LINUX_ABI64_AT_SYMLINK_NOFOLLOW
                | (u64)LINUX_ABI64_AT_EMPTY_PATH))
        != 0ull)
    {
        linux_abi64_note_stat_denial(LINUX_ABI64_SYSCALL_NEWFSTATAT);
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_NEWFSTATAT,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if ((flags & (u64)LINUX_ABI64_AT_EMPTY_PATH) != 0ull)
    {
        u8 path_byte;

        if ((dirfd == LINUX_ABI64_AT_FDCWD)
            || (linux_abi64_user_buffer_readable(pid, user_path, 1u) == 0u))
        {
            linux_abi64_note_stat_fault(LINUX_ABI64_SYSCALL_NEWFSTATAT);
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_NEWFSTATAT,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        path_byte = *((volatile const u8 *)(u64)user_path);
        if (path_byte == 0u)
        {
            return linux_abi64_fstat_common(
                pid,
                dirfd,
                user_stat,
                rip,
                LINUX_ABI64_SYSCALL_NEWFSTATAT);
        }
    }

    if ((dirfd != LINUX_ABI64_AT_FDCWD)
        && (linux_abi64_user_buffer_readable(pid, user_path, 1u) != 0u)
        && (*((volatile const u8 *)(u64)user_path) != (u8)'/'))
    {
        return linux_abi64_stat_path_common(
            pid,
            dirfd,
            user_path,
            user_stat,
            rip,
            LINUX_ABI64_SYSCALL_NEWFSTATAT,
            ((flags & (u64)LINUX_ABI64_AT_SYMLINK_NOFOLLOW) != 0ull) ? 1u : 0u);
    }

    return linux_abi64_stat_path_common(
        pid,
        LINUX_ABI64_AT_FDCWD,
        user_path,
        user_stat,
        rip,
        LINUX_ABI64_SYSCALL_NEWFSTATAT,
        ((flags & (u64)LINUX_ABI64_AT_SYMLINK_NOFOLLOW) != 0ull) ? 1u : 0u);
}

u64 linux_abi64_sys_mmap(
    u32 pid,
    u64 hint_address,
    u64 length,
    u64 prot,
    u64 flags,
    u64 fd_number,
    u64 offset,
    u64 rip)
{
    u64 rounded_length;
    u64 mapped_address;
    u32 vma_prot;
    u32 vma_flags;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_mmap_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_MMAP,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    rounded_length = linux_abi64_page_align_up(length);
    vma_prot = linux_abi64_mmap_prot_to_vma(prot);
    vma_flags = linux_abi64_mmap_flags_to_vma(flags);
    (void)fd_number;
    if ((rounded_length == 0ull)
        || (vma_prot == 0u)
        || (vma_flags == 0u)
        || (offset != 0ull)
        || (((vma_flags & VMA64_MAP_FIXED) != 0u)
            && ((hint_address == 0ull)
                || ((hint_address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))))
    {
        ++g_linux_abi64_mmap_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_MMAP,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    mapped_address = vma64_map_anon(pid, hint_address, rounded_length, vma_prot, vma_flags);
    if (mapped_address == 0ull)
    {
        ++g_linux_abi64_mmap_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_MMAP,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    ++g_linux_abi64_mmap_count;
    g_linux_abi64_mmap_byte_count += (u32)rounded_length;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_MMAP,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return mapped_address;
}

u64 linux_abi64_sys_mprotect(u32 pid, u64 address, u64 length, u64 prot, u64 rip)
{
    u64 rounded_length;
    u32 vma_prot;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_mprotect_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_MPROTECT,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    rounded_length = linux_abi64_page_align_up(length);
    vma_prot = linux_abi64_mmap_prot_to_vma(prot);
    if ((address == 0ull)
        || ((address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (rounded_length == 0ull)
        || ((address + rounded_length) < address)
        || (vma_prot == 0u)
        || (vma64_protect(pid, address, rounded_length, vma_prot) == 0u))
    {
        ++g_linux_abi64_mprotect_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_MPROTECT,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    ++g_linux_abi64_mprotect_count;
    g_linux_abi64_mprotect_byte_count += (u32)rounded_length;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_MPROTECT,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_munmap(u32 pid, u64 address, u64 length, u64 rip)
{
    u64 rounded_length;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_munmap_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_MUNMAP,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    rounded_length = linux_abi64_page_align_up(length);
    if ((address == 0ull)
        || ((address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (rounded_length == 0ull)
        || ((address + rounded_length) < address)
        || (vma64_unmap(pid, address, rounded_length) == 0u))
    {
        ++g_linux_abi64_munmap_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_MUNMAP,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    ++g_linux_abi64_munmap_count;
    g_linux_abi64_munmap_byte_count += (u32)rounded_length;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_MUNMAP,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

static u32 linux_abi64_terminal_read_kernel(
    u32 pid,
    u32 fd_number,
    u32 fd_capability,
    u8 *output,
    u32 byte_count)
{
    u32 bytes_read;
    u32 start_ticks;
    u32 guard = 0u;

    bytes_read = input64_read_kernel(
        fd_capability,
        output,
        byte_count,
        process64_principal(pid));
    if ((bytes_read != 0u)
        || (bytes_read == INPUT64_INVALID_RESULT)
        || (fd_number != FD64_STDIN)
        || ((fd64_entry_flags(pid, fd_number) & FD64_FLAG_O_NONBLOCK) != 0u))
    {
        return bytes_read;
    }

    start_ticks = pit_get_ticks();
    while ((bytes_read == 0u)
        && (((u32)(pit_get_ticks() - start_ticks)) < LINUX_ABI64_TERMINAL_READ_WAIT_TICKS)
        && (guard < LINUX_ABI64_TERMINAL_READ_SPIN_BUDGET))
    {
        input64_poll_keyboard();
        linux_abi64_cpu_pause();
        bytes_read = input64_read_kernel(
            fd_capability,
            output,
            byte_count,
            process64_principal(pid));
        ++guard;
    }

    return bytes_read;
}

u64 linux_abi64_sys_read(u32 pid, u64 fd_number, u64 user_buffer, u64 byte_count, u64 rip)
{
    static u8 read_scratch[LINUX_ABI64_READ_CHUNK_BYTES];
    u32 read_count;
    u32 bytes_read;
    u32 fd_index;
    u32 fd_type;
    u32 fd_capability;
    u32 linux_device_type;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_read_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_READ,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }
    fd_index = (u32)fd_number;

    if (byte_count == 0ull)
    {
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READ,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    read_count = (byte_count > (u64)LINUX_ABI64_READ_CHUNK_BYTES)
        ? LINUX_ABI64_READ_CHUNK_BYTES
        : (u32)byte_count;

    if (linux_abi64_user_buffer_writable(pid, user_buffer, read_count) == 0u)
    {
        ++g_linux_abi64_read_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READ,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    fd_type = fd64_entry_type(pid, fd_index);
    if (fd_type == FD64_TYPE_DEVICE)
    {
        fd_capability = fd64_entry_capability(pid, fd_index);
        linux_device_type = linux_vfs64_device_type_from_handle(fd_capability);
        if (linux_device_type != LINUX_VFS64_DEVICE_UNKNOWN)
        {
            bytes_read = linux_vfs64_read_fd(pid, fd_index, read_scratch, read_count);
            if (bytes_read == LINUX_VFS64_INVALID_RESULT)
            {
                ++g_linux_abi64_read_denial_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                    LINUX_ABI64_SYSCALL_READ,
                    LINUX_ABI64_EBADF,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
            }
        }
        else
        {
            bytes_read = linux_abi64_terminal_read_kernel(
                pid,
                fd_index,
                fd_capability,
                read_scratch,
                read_count);
            if (bytes_read == INPUT64_INVALID_RESULT)
            {
                ++g_linux_abi64_read_denial_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                    LINUX_ABI64_SYSCALL_READ,
                    LINUX_ABI64_EBADF,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
            }
        }
    }
    else
    {
        bytes_read = fd64_read(pid, fd_index, read_scratch, read_count);
        if (bytes_read == FD64_IO_BLOCKED)
        {
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_READ,
                LINUX_ABI64_EAGAIN,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
        }
        if (bytes_read == FD64_IO_ERROR)
        {
            ++g_linux_abi64_read_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                LINUX_ABI64_SYSCALL_READ,
                LINUX_ABI64_EBADF,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
        }
    }

    linux_abi64_copy_to_user(user_buffer, read_scratch, bytes_read);
    ++g_linux_abi64_read_count;
    g_linux_abi64_read_byte_count += bytes_read;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_READ,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)bytes_read;
}

u64 linux_abi64_sys_write(u32 pid, u64 fd_number, u64 user_buffer, u64 byte_count, u64 rip)
{
    u32 write_count;
    u32 bytes_written;
    u32 fd_index;
    u32 fd_type;
    u32 fd_capability;
    u32 linux_device_type;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_write_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_WRITE,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }
    fd_index = (u32)fd_number;

    if (byte_count == 0ull)
    {
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITE,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    write_count = (byte_count > (u64)LINUX_ABI64_WRITE_CHUNK_BYTES)
        ? LINUX_ABI64_WRITE_CHUNK_BYTES
        : (u32)byte_count;

    if (linux_abi64_user_buffer_readable(pid, user_buffer, write_count) == 0u)
    {
        ++g_linux_abi64_write_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITE,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    fd_type = fd64_entry_type(pid, fd_index);
    if (fd_type == FD64_TYPE_DEVICE)
    {
        fd_capability = fd64_entry_capability(pid, fd_index);
        linux_device_type = linux_vfs64_device_type_from_handle(fd_capability);
        bytes_written = (linux_device_type != LINUX_VFS64_DEVICE_UNKNOWN)
            ? linux_vfs64_write_fd(pid, fd_index, (const u8 *)(u64)user_buffer, write_count)
            : fd64_write(pid, fd_index, (const u8 *)(u64)user_buffer, write_count);
    }
    else
    {
        bytes_written = fd64_write(pid, fd_index, (const u8 *)(u64)user_buffer, write_count);
    }
    if (bytes_written == FD64_IO_BLOCKED)
    {
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITE,
            LINUX_ABI64_EAGAIN,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
    }
    if ((bytes_written == FD64_IO_ERROR) || (bytes_written == LINUX_VFS64_INVALID_RESULT))
    {
        ++g_linux_abi64_write_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_WRITE,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    ++g_linux_abi64_write_count;
    g_linux_abi64_write_byte_count += bytes_written;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_WRITE,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)bytes_written;
}

u64 linux_abi64_sys_pread64(
    u32 pid,
    u64 fd_number,
    u64 user_buffer,
    u64 byte_count,
    u64 file_offset,
    u64 rip)
{
    static u8 read_scratch[LINUX_ABI64_READ_CHUNK_BYTES];
    u32 read_count;
    u32 bytes_read;
    u32 fd_index;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_positioned_last_syscall = LINUX_ABI64_SYSCALL_PREAD64;
    g_linux_abi64_positioned_last_fd =
        (fd_number <= 0xFFFFFFFFull) ? (u32)fd_number : FD64_INVALID_FD;
    g_linux_abi64_positioned_last_byte_count = 0u;
    g_linux_abi64_positioned_last_offset = file_offset;
    g_linux_abi64_positioned_last_result = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_PREAD64,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_PREAD64,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    if (file_offset > LINUX_ABI64_POSITIONAL_OFFSET_MAX)
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PREAD64,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (byte_count == 0ull)
    {
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PREAD64,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    read_count = (byte_count > (u64)LINUX_ABI64_READ_CHUNK_BYTES)
        ? LINUX_ABI64_READ_CHUNK_BYTES
        : (u32)byte_count;

    if (linux_abi64_user_buffer_writable(pid, user_buffer, read_count) == 0u)
    {
        ++g_linux_abi64_positioned_fault_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PREAD64,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    fd_index = (u32)fd_number;
    bytes_read = fd64_read_at(pid, fd_index, file_offset, read_scratch, read_count);
    if (bytes_read == FD64_IO_ERROR)
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_PREAD64,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    linux_abi64_copy_to_user(user_buffer, read_scratch, bytes_read);
    ++g_linux_abi64_pread64_count;
    g_linux_abi64_pread64_byte_count += bytes_read;
    g_linux_abi64_positioned_last_byte_count = bytes_read;
    g_linux_abi64_positioned_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_PREAD64,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)bytes_read;
}

u64 linux_abi64_sys_pwrite64(
    u32 pid,
    u64 fd_number,
    u64 user_buffer,
    u64 byte_count,
    u64 file_offset,
    u64 rip)
{
    static u8 write_scratch[LINUX_ABI64_WRITE_CHUNK_BYTES];
    u32 write_count;
    u32 bytes_written;
    u32 fd_index;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_positioned_last_syscall = LINUX_ABI64_SYSCALL_PWRITE64;
    g_linux_abi64_positioned_last_fd =
        (fd_number <= 0xFFFFFFFFull) ? (u32)fd_number : FD64_INVALID_FD;
    g_linux_abi64_positioned_last_byte_count = 0u;
    g_linux_abi64_positioned_last_offset = file_offset;
    g_linux_abi64_positioned_last_result = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_PWRITE64,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_PWRITE64,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    if (file_offset > LINUX_ABI64_POSITIONAL_OFFSET_MAX)
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PWRITE64,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (byte_count == 0ull)
    {
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PWRITE64,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    write_count = (byte_count > (u64)LINUX_ABI64_WRITE_CHUNK_BYTES)
        ? LINUX_ABI64_WRITE_CHUNK_BYTES
        : (u32)byte_count;

    if (linux_abi64_user_buffer_readable(pid, user_buffer, write_count) == 0u)
    {
        ++g_linux_abi64_positioned_fault_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PWRITE64,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    linux_abi64_copy_from_user(write_scratch, user_buffer, write_count);
    fd_index = (u32)fd_number;
    bytes_written = fd64_write_at(pid, fd_index, file_offset, write_scratch, write_count);
    if (bytes_written == FD64_IO_ERROR)
    {
        ++g_linux_abi64_positioned_denial_count;
        g_linux_abi64_positioned_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_PWRITE64,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    ++g_linux_abi64_pwrite64_count;
    g_linux_abi64_pwrite64_byte_count += bytes_written;
    g_linux_abi64_positioned_last_byte_count = bytes_written;
    g_linux_abi64_positioned_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_PWRITE64,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)bytes_written;
}

u64 linux_abi64_sys_readv(u32 pid, u64 fd_number, u64 user_iov, u64 iov_count, u64 rip)
{
    static linux_abi64_iovec_t iovecs[LINUX_ABI64_IOV_MAX];
    static u8 read_scratch[LINUX_ABI64_READ_CHUNK_BYTES];
    u32 fd_index;
    u32 index;
    u32 used_iovs;
    u32 requested_bytes;
    u32 bytes_read;
    u32 copied_bytes;
    u32 fd_type;
    u32 fd_capability;
    u32 linux_device_type;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_vector_last_syscall = LINUX_ABI64_SYSCALL_READV;
    g_linux_abi64_vector_last_fd =
        (fd_number <= 0xFFFFFFFFull) ? (u32)fd_number : FD64_INVALID_FD;
    g_linux_abi64_vector_last_iov_count =
        (iov_count <= 0xFFFFFFFFull) ? (u32)iov_count : 0xFFFFFFFFu;
    g_linux_abi64_vector_last_byte_count = 0u;
    g_linux_abi64_vector_last_result = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_READV,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_READV,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }
    fd_index = (u32)fd_number;

    if (iov_count == 0ull)
    {
        g_linux_abi64_vector_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READV,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    if (iov_count > (u64)LINUX_ABI64_IOV_MAX)
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READV,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_readable(
            pid,
            user_iov,
            (u32)iov_count * LINUX_ABI64_IOVEC_BYTES) == 0u)
    {
        ++g_linux_abi64_vector_fault_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READV,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    used_iovs = 0u;
    requested_bytes = 0u;
    for (index = 0u; index < (u32)iov_count; ++index)
    {
        linux_abi64_iovec_t raw_iov;
        u32 segment_bytes;
        u32 remaining_bytes;

        if (linux_abi64_copy_iovec_from_user(pid, user_iov, index, &raw_iov) == 0u)
        {
            ++g_linux_abi64_vector_fault_count;
            g_linux_abi64_vector_last_result = LINUX_ABI64_EFAULT;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_READV,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        if ((raw_iov.iov_len == 0ull) || (requested_bytes >= LINUX_ABI64_READ_CHUNK_BYTES))
        {
            continue;
        }

        remaining_bytes = LINUX_ABI64_READ_CHUNK_BYTES - requested_bytes;
        segment_bytes =
            (raw_iov.iov_len > (u64)remaining_bytes) ? remaining_bytes : (u32)raw_iov.iov_len;

        if (linux_abi64_user_buffer_writable(pid, raw_iov.iov_base, segment_bytes) == 0u)
        {
            ++g_linux_abi64_vector_fault_count;
            g_linux_abi64_vector_last_result = LINUX_ABI64_EFAULT;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_READV,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        iovecs[used_iovs].iov_base = raw_iov.iov_base;
        iovecs[used_iovs].iov_len = (u64)segment_bytes;
        ++used_iovs;
        requested_bytes += segment_bytes;
    }

    if (requested_bytes == 0u)
    {
        g_linux_abi64_vector_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READV,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    fd_type = fd64_entry_type(pid, fd_index);
    if (fd_type == FD64_TYPE_DEVICE)
    {
        fd_capability = fd64_entry_capability(pid, fd_index);
        linux_device_type = linux_vfs64_device_type_from_handle(fd_capability);
        if (linux_device_type != LINUX_VFS64_DEVICE_UNKNOWN)
        {
            bytes_read = linux_vfs64_read_fd(pid, fd_index, read_scratch, requested_bytes);
        }
        else
        {
            bytes_read = linux_abi64_terminal_read_kernel(
                pid,
                fd_index,
                fd_capability,
                read_scratch,
                requested_bytes);
        }
    }
    else
    {
        bytes_read = fd64_read(pid, fd_index, read_scratch, requested_bytes);
    }
    if (bytes_read == FD64_IO_BLOCKED)
    {
        g_linux_abi64_vector_last_result = LINUX_ABI64_EAGAIN;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_READV,
            LINUX_ABI64_EAGAIN,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
    }
    if ((bytes_read == FD64_IO_ERROR)
        || (bytes_read == INPUT64_INVALID_RESULT)
        || (bytes_read == LINUX_VFS64_INVALID_RESULT))
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_READV,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    copied_bytes = 0u;
    for (index = 0u; (index < used_iovs) && (copied_bytes < bytes_read); ++index)
    {
        u32 segment_bytes = (u32)iovecs[index].iov_len;
        u32 available_bytes = bytes_read - copied_bytes;

        if (segment_bytes > available_bytes)
        {
            segment_bytes = available_bytes;
        }

        linux_abi64_copy_to_user(
            iovecs[index].iov_base,
            &read_scratch[copied_bytes],
            segment_bytes);
        copied_bytes += segment_bytes;
    }

    ++g_linux_abi64_readv_count;
    g_linux_abi64_readv_byte_count += bytes_read;
    g_linux_abi64_vector_last_byte_count = bytes_read;
    g_linux_abi64_vector_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_READV,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)bytes_read;
}

u64 linux_abi64_sys_writev(u32 pid, u64 fd_number, u64 user_iov, u64 iov_count, u64 rip)
{
    static u8 write_scratch[LINUX_ABI64_WRITE_CHUNK_BYTES];
    u32 fd_index;
    u32 index;
    u32 requested_bytes;
    u32 bytes_written;
    u32 fd_type;
    u32 fd_capability;
    u32 linux_device_type;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_vector_last_syscall = LINUX_ABI64_SYSCALL_WRITEV;
    g_linux_abi64_vector_last_fd =
        (fd_number <= 0xFFFFFFFFull) ? (u32)fd_number : FD64_INVALID_FD;
    g_linux_abi64_vector_last_iov_count =
        (iov_count <= 0xFFFFFFFFull) ? (u32)iov_count : 0xFFFFFFFFu;
    g_linux_abi64_vector_last_byte_count = 0u;
    g_linux_abi64_vector_last_result = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_WRITEV,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number > 0xFFFFFFFFull)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_WRITEV,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }
    fd_index = (u32)fd_number;

    if (iov_count == 0ull)
    {
        g_linux_abi64_vector_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITEV,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    if (iov_count > (u64)LINUX_ABI64_IOV_MAX)
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITEV,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_readable(
            pid,
            user_iov,
            (u32)iov_count * LINUX_ABI64_IOVEC_BYTES) == 0u)
    {
        ++g_linux_abi64_vector_fault_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITEV,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    requested_bytes = 0u;
    for (index = 0u; index < (u32)iov_count; ++index)
    {
        linux_abi64_iovec_t raw_iov;
        u32 segment_bytes;
        u32 remaining_bytes;

        if (linux_abi64_copy_iovec_from_user(pid, user_iov, index, &raw_iov) == 0u)
        {
            ++g_linux_abi64_vector_fault_count;
            g_linux_abi64_vector_last_result = LINUX_ABI64_EFAULT;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_WRITEV,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        if ((raw_iov.iov_len == 0ull) || (requested_bytes >= LINUX_ABI64_WRITE_CHUNK_BYTES))
        {
            continue;
        }

        remaining_bytes = LINUX_ABI64_WRITE_CHUNK_BYTES - requested_bytes;
        segment_bytes =
            (raw_iov.iov_len > (u64)remaining_bytes) ? remaining_bytes : (u32)raw_iov.iov_len;

        if (linux_abi64_user_buffer_readable(pid, raw_iov.iov_base, segment_bytes) == 0u)
        {
            ++g_linux_abi64_vector_fault_count;
            g_linux_abi64_vector_last_result = LINUX_ABI64_EFAULT;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_WRITEV,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        linux_abi64_copy_from_user(&write_scratch[requested_bytes], raw_iov.iov_base, segment_bytes);
        requested_bytes += segment_bytes;
    }

    if (requested_bytes == 0u)
    {
        g_linux_abi64_vector_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITEV,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    fd_type = fd64_entry_type(pid, fd_index);
    if (fd_type == FD64_TYPE_DEVICE)
    {
        fd_capability = fd64_entry_capability(pid, fd_index);
        linux_device_type = linux_vfs64_device_type_from_handle(fd_capability);
        bytes_written = (linux_device_type != LINUX_VFS64_DEVICE_UNKNOWN)
            ? linux_vfs64_write_fd(pid, fd_index, write_scratch, requested_bytes)
            : fd64_write(pid, fd_index, write_scratch, requested_bytes);
    }
    else
    {
        bytes_written = fd64_write(pid, fd_index, write_scratch, requested_bytes);
    }
    if (bytes_written == FD64_IO_BLOCKED)
    {
        g_linux_abi64_vector_last_result = LINUX_ABI64_EAGAIN;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WRITEV,
            LINUX_ABI64_EAGAIN,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
    }
    if ((bytes_written == FD64_IO_ERROR) || (bytes_written == LINUX_VFS64_INVALID_RESULT))
    {
        ++g_linux_abi64_vector_denial_count;
        g_linux_abi64_vector_last_result = LINUX_ABI64_EBADF;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_WRITEV,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    ++g_linux_abi64_writev_count;
    g_linux_abi64_writev_byte_count += bytes_written;
    g_linux_abi64_vector_last_byte_count = bytes_written;
    g_linux_abi64_vector_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_WRITEV,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)bytes_written;
}

static u32 linux_abi64_copy_pollfd_from_user(
    u32 pid,
    u64 user_fds,
    u32 fd_index,
    linux_abi64_pollfd_t *out_pollfd)
{
    u64 entry_address;

    if (out_pollfd != 0)
    {
        out_pollfd->fd = -1;
        out_pollfd->events = 0u;
        out_pollfd->revents = 0u;
    }
    if ((out_pollfd == 0)
        || (fd_index >= LINUX_ABI64_POLL_MAX_FDS)
        || (user_fds > (0xFFFFFFFFFFFFFFFFull - ((u64)fd_index * LINUX_ABI64_POLLFD_BYTES))))
    {
        return 0u;
    }

    entry_address = user_fds + ((u64)fd_index * LINUX_ABI64_POLLFD_BYTES);
    if (linux_abi64_user_buffer_readable(pid, entry_address, LINUX_ABI64_POLLFD_BYTES) == 0u)
    {
        return 0u;
    }

    linux_abi64_copy_from_user((u8 *)out_pollfd, entry_address, LINUX_ABI64_POLLFD_BYTES);
    return 1u;
}

static u32 linux_abi64_copy_pollfd_revents_to_user(
    u32 pid,
    u64 user_fds,
    u32 fd_index,
    u16 revents)
{
    u64 revents_address;

    if ((fd_index >= LINUX_ABI64_POLL_MAX_FDS)
        || (user_fds > (0xFFFFFFFFFFFFFFFFull - ((u64)fd_index * LINUX_ABI64_POLLFD_BYTES)))
        || (user_fds + ((u64)fd_index * LINUX_ABI64_POLLFD_BYTES)
            > (0xFFFFFFFFFFFFFFFFull - 6ull)))
    {
        return 0u;
    }

    revents_address = user_fds + ((u64)fd_index * LINUX_ABI64_POLLFD_BYTES) + 6ull;
    if (linux_abi64_user_buffer_writable(pid, revents_address, (u32)sizeof(u16)) == 0u)
    {
        return 0u;
    }

    linux_abi64_copy_to_user(revents_address, (const u8 *)&revents, (u32)sizeof(u16));
    return 1u;
}

static u16 linux_abi64_pollfd_revents(u32 pid, const linux_abi64_pollfd_t *pollfd)
{
    u32 owner;
    u32 fd_number;
    u32 fd_type;
    u32 capability;
    u32 available;
    u16 requested;
    u16 revents = 0u;

    if (pollfd == 0)
    {
        return (u16)LINUX_ABI64_POLLNVAL;
    }

    if (pollfd->fd < 0)
    {
        return 0u;
    }

    fd_number = (u32)pollfd->fd;
    fd_type = fd64_entry_type(pid, fd_number);
    if (fd_type == FD64_TYPE_EMPTY)
    {
        return (u16)LINUX_ABI64_POLLNVAL;
    }

    requested = pollfd->events;
    capability = fd64_entry_capability(pid, fd_number);
    owner = process64_principal(pid);

    if (fd_type == FD64_TYPE_PIPE_READ)
    {
        available = pipe64_bytes_available(capability, owner);
        if (available == PIPE64_INVALID_HANDLE)
        {
            return (u16)LINUX_ABI64_POLLERR;
        }
        if ((available != 0u) && ((requested & LINUX_ABI64_POLLIN) != 0u))
        {
            revents |= (u16)LINUX_ABI64_POLLIN;
        }
        if (pipe64_writer_closed(capability, owner) != 0u)
        {
            revents |= (u16)LINUX_ABI64_POLLHUP;
        }
    }
    else if (fd_type == FD64_TYPE_PIPE_WRITE)
    {
        if (pipe64_reader_closed(capability, owner) != 0u)
        {
            revents |= (u16)(LINUX_ABI64_POLLERR | LINUX_ABI64_POLLHUP);
        }
        else if ((requested & LINUX_ABI64_POLLOUT) != 0u)
        {
            revents |= (u16)LINUX_ABI64_POLLOUT;
        }
    }
    else if (fd_type == FD64_TYPE_RAMFS_NODE)
    {
        if ((requested & LINUX_ABI64_POLLIN) != 0u)
        {
            revents |= (u16)LINUX_ABI64_POLLIN;
        }
        if ((requested & LINUX_ABI64_POLLOUT) != 0u)
        {
            revents |= (u16)LINUX_ABI64_POLLOUT;
        }
    }
    else if (fd_type == FD64_TYPE_DEVICE)
    {
        if (((fd_number == FD64_STDOUT) || (fd_number == FD64_STDERR))
            && ((requested & LINUX_ABI64_POLLOUT) != 0u))
        {
            revents |= (u16)LINUX_ABI64_POLLOUT;
        }
    }

    return revents;
}

static u64 linux_abi64_poll_error_return(
    u32 pid,
    u16 syscall_number,
    u32 error_code,
    u64 rip,
    u32 fault)
{
    ++g_linux_abi64_poll_denial_count;
    if (fault != 0u)
    {
        ++g_linux_abi64_poll_fault_count;
    }
    g_linux_abi64_poll_last_result = error_code;
    (void)persona_audit64_record(
        pid,
        (error_code == LINUX_ABI64_ESRCH)
            ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
            : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        syscall_number,
        error_code,
        rip);
    return LINUX_ABI64_ERROR_RETURN(error_code);
}

static u64 linux_abi64_poll_core(
    u32 pid,
    u64 user_fds,
    u64 fd_count,
    u16 syscall_number,
    u64 rip)
{
    u32 index;
    u32 ready_count = 0u;
    u32 revents_or = 0u;

    g_linux_abi64_poll_last_syscall = (u32)syscall_number;
    g_linux_abi64_poll_last_fd_count =
        (fd_count <= 0xFFFFFFFFull) ? (u32)fd_count : 0xFFFFFFFFu;
    g_linux_abi64_poll_last_ready = 0u;
    g_linux_abi64_poll_last_revents = 0u;
    g_linux_abi64_poll_last_result = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        return linux_abi64_poll_error_return(pid, syscall_number, LINUX_ABI64_ESRCH, rip, 0u);
    }

    if (fd_count == 0ull)
    {
        if (syscall_number == LINUX_ABI64_SYSCALL_PPOLL)
        {
            ++g_linux_abi64_ppoll_count;
        }
        else
        {
            ++g_linux_abi64_poll_count;
        }
        g_linux_abi64_poll_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    if (fd_count > (u64)LINUX_ABI64_POLL_MAX_FDS)
    {
        return linux_abi64_poll_error_return(pid, syscall_number, LINUX_ABI64_EINVAL, rip, 0u);
    }

    if ((linux_abi64_user_buffer_readable(
            pid,
            user_fds,
            (u32)fd_count * LINUX_ABI64_POLLFD_BYTES) == 0u)
        || (linux_abi64_user_buffer_writable(
            pid,
            user_fds,
            (u32)fd_count * LINUX_ABI64_POLLFD_BYTES) == 0u))
    {
        return linux_abi64_poll_error_return(pid, syscall_number, LINUX_ABI64_EFAULT, rip, 1u);
    }

    for (index = 0u; index < (u32)fd_count; ++index)
    {
        linux_abi64_pollfd_t pollfd;
        u16 revents;

        if (linux_abi64_copy_pollfd_from_user(pid, user_fds, index, &pollfd) == 0u)
        {
            return linux_abi64_poll_error_return(
                pid,
                syscall_number,
                LINUX_ABI64_EFAULT,
                rip,
                1u);
        }

        revents = linux_abi64_pollfd_revents(pid, &pollfd);
        if (linux_abi64_copy_pollfd_revents_to_user(pid, user_fds, index, revents) == 0u)
        {
            return linux_abi64_poll_error_return(
                pid,
                syscall_number,
                LINUX_ABI64_EFAULT,
                rip,
                1u);
        }

        if (revents != 0u)
        {
            ++ready_count;
            revents_or |= (u32)revents;
        }
    }

    if (syscall_number == LINUX_ABI64_SYSCALL_PPOLL)
    {
        ++g_linux_abi64_ppoll_count;
    }
    else
    {
        ++g_linux_abi64_poll_count;
    }
    g_linux_abi64_poll_ready_count += ready_count;
    g_linux_abi64_poll_last_ready = ready_count;
    g_linux_abi64_poll_last_revents = revents_or;
    g_linux_abi64_poll_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)ready_count;
}

u64 linux_abi64_sys_poll(u32 pid, u64 user_fds, u64 fd_count, u64 timeout_ms, u64 rip)
{
    (void)timeout_ms;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return linux_abi64_poll_core(pid, user_fds, fd_count, LINUX_ABI64_SYSCALL_POLL, rip);
}

u64 linux_abi64_sys_ppoll(
    u32 pid,
    u64 user_fds,
    u64 fd_count,
    u64 user_timeout,
    u64 user_sigmask,
    u64 sigset_size,
    u64 rip)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_poll_last_syscall = LINUX_ABI64_SYSCALL_PPOLL;
    g_linux_abi64_poll_last_fd_count =
        (fd_count <= 0xFFFFFFFFull) ? (u32)fd_count : 0xFFFFFFFFu;
    g_linux_abi64_poll_last_ready = 0u;
    g_linux_abi64_poll_last_revents = 0u;
    g_linux_abi64_poll_last_result = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        return linux_abi64_poll_error_return(
            pid,
            LINUX_ABI64_SYSCALL_PPOLL,
            LINUX_ABI64_ESRCH,
            rip,
            0u);
    }

    if ((user_timeout != 0ull)
        && (linux_abi64_user_buffer_readable(pid, user_timeout, LINUX_ABI64_TIMESPEC_BYTES) == 0u))
    {
        return linux_abi64_poll_error_return(
            pid,
            LINUX_ABI64_SYSCALL_PPOLL,
            LINUX_ABI64_EFAULT,
            rip,
            1u);
    }

    if (user_sigmask != 0ull)
    {
        if ((sigset_size != LINUX_ABI64_SIGSET_BYTES)
            || (linux_abi64_user_buffer_readable(pid, user_sigmask, LINUX_ABI64_SIGSET_BYTES) == 0u))
        {
            return linux_abi64_poll_error_return(
                pid,
                LINUX_ABI64_SYSCALL_PPOLL,
                (sigset_size == LINUX_ABI64_SIGSET_BYTES) ? LINUX_ABI64_EFAULT : LINUX_ABI64_EINVAL,
                rip,
                (sigset_size == LINUX_ABI64_SIGSET_BYTES) ? 1u : 0u);
        }
    }

    return linux_abi64_poll_core(pid, user_fds, fd_count, LINUX_ABI64_SYSCALL_PPOLL, rip);
}

u64 linux_abi64_sys_brk(u32 pid, u64 requested_brk)
{
    u64 current;
    u64 updated;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID) || (process64_principal(pid) == 0u))
    {
        ++g_linux_abi64_brk_denial_count;
        return 0ull;
    }

    current = vma64_brk_query(pid);
    if (current == 0ull)
    {
        ++g_linux_abi64_brk_denial_count;
        return 0ull;
    }

    if (requested_brk == 0ull)
    {
        ++g_linux_abi64_brk_query_count;
        linux_abi64_sync_persona_brk(pid, current);
        return current;
    }

    updated = vma64_brk_extend(pid, requested_brk);
    if (updated == 0ull)
    {
        ++g_linux_abi64_brk_denial_count;
        linux_abi64_sync_persona_brk(pid, current);
        return current;
    }

    ++g_linux_abi64_brk_extend_count;
    linux_abi64_sync_persona_brk(pid, updated);
    return updated;
}

static u64 linux_abi64_rt_sigaction_error(
    u32 pid,
    u32 error_code,
    u64 rip,
    u32 fault)
{
    ++g_linux_abi64_rt_sigaction_denial_count;
    if (fault != 0u)
    {
        ++g_linux_abi64_rt_sigaction_fault_count;
    }
    g_linux_abi64_rt_sigaction_last_result = error_code;
    (void)persona_audit64_record(
        pid,
        (error_code == LINUX_ABI64_ESRCH)
            ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
            : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_RT_SIGACTION,
        error_code,
        rip);
    return LINUX_ABI64_ERROR_RETURN(error_code);
}

u64 linux_abi64_sys_rt_sigaction(
    u32 pid,
    u64 signal_number,
    u64 user_act,
    u64 user_oldact,
    u64 sigset_size,
    u64 rip)
{
    persona_context_t *context;
    linux_signal64_sigaction_t old_action;
    linux_signal64_sigaction_t new_action;
    u32 signal_index;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_rt_sigaction_last_signal =
        (signal_number <= 0x00000000FFFFFFFFull) ? (u32)signal_number : 0xFFFFFFFFu;
    g_linux_abi64_rt_sigaction_last_handler = 0ull;
    g_linux_abi64_rt_sigaction_last_old_handler = 0ull;
    g_linux_abi64_rt_sigaction_last_mask = 0ull;
    g_linux_abi64_rt_sigaction_last_flags = 0ull;
    g_linux_abi64_rt_sigaction_last_result = 0u;

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        return linux_abi64_rt_sigaction_error(pid, LINUX_ABI64_ESRCH, rip, 0u);
    }

    if ((signal_number == 0ull)
        || (signal_number > (u64)LINUX_SIGNAL64_MAX_SIGNALS)
        || (sigset_size != LINUX_ABI64_SIGSET_BYTES))
    {
        return linux_abi64_rt_sigaction_error(pid, LINUX_ABI64_EINVAL, rip, 0u);
    }

    if ((user_act != 0ull)
        && ((signal_number == (u64)LINUX_SIGNAL64_SIGKILL)
            || (signal_number == (u64)LINUX_SIGNAL64_SIGSTOP)))
    {
        return linux_abi64_rt_sigaction_error(pid, LINUX_ABI64_EINVAL, rip, 0u);
    }

    if ((user_oldact != 0ull)
        && (linux_abi64_user_buffer_writable(
                pid,
                user_oldact,
                LINUX_SIGNAL64_SIGACTION_BYTES) == 0u))
    {
        return linux_abi64_rt_sigaction_error(pid, LINUX_ABI64_EFAULT, rip, 1u);
    }

    if ((user_act != 0ull)
        && (linux_abi64_user_buffer_readable(
                pid,
                user_act,
                LINUX_SIGNAL64_SIGACTION_BYTES) == 0u))
    {
        return linux_abi64_rt_sigaction_error(pid, LINUX_ABI64_EFAULT, rip, 1u);
    }

    signal_index = (u32)(signal_number - 1ull);
    old_action = context->linux_sigactions[signal_index];
    if (user_oldact != 0ull)
    {
        linux_abi64_copy_to_user(
            user_oldact,
            (const u8 *)&old_action,
            LINUX_SIGNAL64_SIGACTION_BYTES);
        g_linux_abi64_rt_sigaction_last_old_handler = old_action.handler;
    }

    if (user_act != 0ull)
    {
        linux_abi64_copy_from_user(
            (u8 *)&new_action,
            user_act,
            LINUX_SIGNAL64_SIGACTION_BYTES);
        context->linux_sigactions[signal_index] = new_action;
        g_linux_abi64_rt_sigaction_last_handler = new_action.handler;
        g_linux_abi64_rt_sigaction_last_mask = new_action.sa_mask;
        g_linux_abi64_rt_sigaction_last_flags = new_action.sa_flags;
    }
    else
    {
        ++g_linux_abi64_rt_sigaction_query_count;
        g_linux_abi64_rt_sigaction_last_handler = old_action.handler;
        g_linux_abi64_rt_sigaction_last_mask = old_action.sa_mask;
        g_linux_abi64_rt_sigaction_last_flags = old_action.sa_flags;
    }

    ++g_linux_abi64_rt_sigaction_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_RT_SIGACTION,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

static u64 linux_abi64_rt_sigprocmask_error(
    u32 pid,
    u32 error_code,
    u64 rip,
    u32 fault)
{
    ++g_linux_abi64_rt_sigprocmask_denial_count;
    if (fault != 0u)
    {
        ++g_linux_abi64_rt_sigprocmask_fault_count;
    }
    g_linux_abi64_rt_sigprocmask_last_result = error_code;
    (void)persona_audit64_record(
        pid,
        (error_code == LINUX_ABI64_ESRCH)
            ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
            : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_RT_SIGPROCMASK,
        error_code,
        rip);
    return LINUX_ABI64_ERROR_RETURN(error_code);
}

u64 linux_abi64_sys_rt_sigprocmask(
    u32 pid,
    u64 how,
    u64 user_set,
    u64 user_oldset,
    u64 sigset_size,
    u64 rip)
{
    persona_context_t *context;
    u64 requested_mask;
    u64 old_mask;
    u64 new_mask;
    u32 how32;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    how32 = (how <= 0x00000000FFFFFFFFull) ? (u32)how : 0xFFFFFFFFu;
    requested_mask = 0ull;
    g_linux_abi64_rt_sigprocmask_last_how = how32;
    g_linux_abi64_rt_sigprocmask_last_set = 0ull;
    g_linux_abi64_rt_sigprocmask_last_old_mask = 0ull;
    g_linux_abi64_rt_sigprocmask_last_mask = 0ull;
    g_linux_abi64_rt_sigprocmask_last_result = 0u;

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        return linux_abi64_rt_sigprocmask_error(pid, LINUX_ABI64_ESRCH, rip, 0u);
    }

    if (sigset_size != LINUX_ABI64_SIGSET_BYTES)
    {
        return linux_abi64_rt_sigprocmask_error(pid, LINUX_ABI64_EINVAL, rip, 0u);
    }

    if ((user_oldset != 0ull)
        && (linux_abi64_user_buffer_writable(
                pid,
                user_oldset,
                LINUX_ABI64_SIGSET_BYTES) == 0u))
    {
        return linux_abi64_rt_sigprocmask_error(pid, LINUX_ABI64_EFAULT, rip, 1u);
    }

    if (user_set != 0ull)
    {
        if ((how32 != LINUX_SIGNAL64_SIG_BLOCK)
            && (how32 != LINUX_SIGNAL64_SIG_UNBLOCK)
            && (how32 != LINUX_SIGNAL64_SIG_SETMASK))
        {
            return linux_abi64_rt_sigprocmask_error(pid, LINUX_ABI64_EINVAL, rip, 0u);
        }

        if (linux_abi64_user_buffer_readable(
                pid,
                user_set,
                LINUX_ABI64_SIGSET_BYTES) == 0u)
        {
            return linux_abi64_rt_sigprocmask_error(pid, LINUX_ABI64_EFAULT, rip, 1u);
        }

        linux_abi64_copy_from_user((u8 *)&requested_mask, user_set, LINUX_ABI64_SIGSET_BYTES);
        requested_mask &= ~LINUX_SIGNAL64_UNBLOCKABLE_MASK;
    }
    else
    {
        ++g_linux_abi64_rt_sigprocmask_query_count;
    }

    old_mask = context->linux_signal_mask & ~LINUX_SIGNAL64_UNBLOCKABLE_MASK;
    context->linux_signal_mask = old_mask;
    if (user_oldset != 0ull)
    {
        linux_abi64_copy_to_user(user_oldset, (const u8 *)&old_mask, LINUX_ABI64_SIGSET_BYTES);
    }

    if (user_set == 0ull)
    {
        new_mask = old_mask;
    }
    else if (how32 == LINUX_SIGNAL64_SIG_BLOCK)
    {
        new_mask = old_mask | requested_mask;
    }
    else if (how32 == LINUX_SIGNAL64_SIG_UNBLOCK)
    {
        new_mask = old_mask & ~requested_mask;
    }
    else
    {
        new_mask = requested_mask;
    }

    context->linux_signal_mask = new_mask & ~LINUX_SIGNAL64_UNBLOCKABLE_MASK;
    g_linux_abi64_rt_sigprocmask_last_set = requested_mask;
    g_linux_abi64_rt_sigprocmask_last_old_mask = old_mask;
    g_linux_abi64_rt_sigprocmask_last_mask = context->linux_signal_mask;
    ++g_linux_abi64_rt_sigprocmask_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_RT_SIGPROCMASK,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

static u64 linux_abi64_pid_common(u32 pid, u64 rip, u32 syscall_number)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        if (syscall_number == LINUX_ABI64_SYSCALL_GETTID)
        {
            ++g_linux_abi64_gettid_denial_count;
        }
        else
        {
            ++g_linux_abi64_getpid_denial_count;
        }
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (syscall_number == LINUX_ABI64_SYSCALL_GETTID)
    {
        ++g_linux_abi64_gettid_count;
    }
    else
    {
        ++g_linux_abi64_getpid_count;
    }
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)pid;
}

u64 linux_abi64_sys_getpid(u32 pid, u64 rip)
{
    return linux_abi64_pid_common(pid, rip, LINUX_ABI64_SYSCALL_GETPID);
}

static u64 linux_abi64_fixed_identity_common(
    u32 pid,
    u64 rip,
    u32 syscall_number,
    u32 fixed_value)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        if (syscall_number == LINUX_ABI64_SYSCALL_GETEUID)
        {
            ++g_linux_abi64_geteuid_denial_count;
        }
        else
        {
            ++g_linux_abi64_getppid_denial_count;
        }
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (syscall_number == LINUX_ABI64_SYSCALL_GETEUID)
    {
        ++g_linux_abi64_geteuid_count;
    }
    else
    {
        ++g_linux_abi64_getppid_count;
    }
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)fixed_value;
}

u64 linux_abi64_sys_geteuid(u32 pid, u64 rip)
{
    return linux_abi64_fixed_identity_common(
        pid,
        rip,
        LINUX_ABI64_SYSCALL_GETEUID,
        LINUX_ABI64_FIXED_UID);
}

u64 linux_abi64_sys_getppid(u32 pid, u64 rip)
{
    return linux_abi64_fixed_identity_common(
        pid,
        rip,
        LINUX_ABI64_SYSCALL_GETPPID,
        LINUX_ABI64_FIXED_PPID);
}

u64 linux_abi64_sys_gettid(u32 pid, u64 rip)
{
    return linux_abi64_pid_common(pid, rip, LINUX_ABI64_SYSCALL_GETTID);
}

u64 linux_abi64_sys_prctl(
    u32 pid,
    u64 option,
    u64 arg2,
    u64 arg3,
    u64 arg4,
    u64 arg5,
    u64 rip)
{
    persona_context_t *context;
    u8 comm[LINUX_ABI64_PR_NAME_BYTES];
    u32 option32 = (u32)(option & 0xFFFFFFFFull);
    u32 index;
    u32 done = 0u;

    (void)arg3;
    (void)arg4;
    (void)arg5;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_prctl_last_option = option32;

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_prctl_denial_count;
        g_linux_abi64_prctl_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_PRCTL,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    ++g_linux_abi64_prctl_count;

    if (option32 == LINUX_ABI64_PR_GET_NAME)
    {
        if (linux_abi64_user_buffer_writable(pid, arg2, LINUX_ABI64_PR_NAME_BYTES) == 0u)
        {
            ++g_linux_abi64_prctl_fault_count;
            g_linux_abi64_prctl_last_result = LINUX_ABI64_EFAULT;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_PRCTL,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        linux_abi64_copy_to_user(arg2, &context->linux_comm[0], LINUX_ABI64_PR_NAME_BYTES);
        ++g_linux_abi64_prctl_get_name_count;
        g_linux_abi64_prctl_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PRCTL,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    if (option32 == LINUX_ABI64_PR_SET_NAME)
    {
        for (index = 0u; index < LINUX_ABI64_PR_NAME_BYTES; ++index)
        {
            comm[index] = 0u;
        }

        for (index = 0u; index < LINUX_ABI64_PR_NAME_BYTES; ++index)
        {
            if (linux_abi64_user_buffer_readable(pid, arg2 + (u64)index, 1u) == 0u)
            {
                ++g_linux_abi64_prctl_fault_count;
                g_linux_abi64_prctl_last_result = LINUX_ABI64_EFAULT;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                    LINUX_ABI64_SYSCALL_PRCTL,
                    LINUX_ABI64_EFAULT,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
            }

            linux_abi64_copy_from_user(&comm[index], arg2 + (u64)index, 1u);
            if (comm[index] == 0u)
            {
                done = 1u;
                break;
            }
        }

        if (done == 0u)
        {
            comm[LINUX_ABI64_PR_NAME_BYTES - 1u] = 0u;
        }

        for (index = 0u; index < LINUX_ABI64_PR_NAME_BYTES; ++index)
        {
            context->linux_comm[index] = comm[index];
        }

        ++g_linux_abi64_prctl_set_name_count;
        g_linux_abi64_prctl_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_PRCTL,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    ++g_linux_abi64_prctl_enosys_count;
    g_linux_abi64_prctl_last_result = LINUX_ABI64_ENOSYS;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_PRCTL,
        LINUX_ABI64_ENOSYS,
        rip);
    return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOSYS);
}

u64 linux_abi64_sys_arch_prctl(u32 pid, u64 code, u64 address, u64 rip)
{
    persona_context_t *context;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_arch_prctl_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_ARCH_PRCTL,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (code == (u64)LINUX_ABI64_ARCH_SET_FS)
    {
        if (address >= LINUX_ABI64_USER_CANONICAL_LIMIT)
        {
            ++g_linux_abi64_arch_prctl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_ARCH_PRCTL,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        context->tls_base = address;
        context->tls_size = 0ull;
        write_fs_base64(address);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)scheduler64_runqueue_set_current_fs_base(address);
#endif
        ++g_linux_abi64_arch_prctl_count;
        ++g_linux_abi64_arch_prctl_set_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_ARCH_PRCTL,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    if (code == (u64)LINUX_ABI64_ARCH_GET_FS)
    {
        volatile u64 *target;

        if (linux_abi64_user_buffer_writable(pid, address, (u32)sizeof(u64)) == 0u)
        {
            ++g_linux_abi64_arch_prctl_fault_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_ARCH_PRCTL,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        target = (volatile u64 *)(u64)address;
        *target = context->tls_base;
        ++g_linux_abi64_arch_prctl_count;
        ++g_linux_abi64_arch_prctl_get_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_ARCH_PRCTL,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    ++g_linux_abi64_arch_prctl_denial_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_ARCH_PRCTL,
        LINUX_ABI64_EINVAL,
        rip);
    return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
}

u64 linux_abi64_sys_set_tid_address(u32 pid, u64 clear_child_tid, u64 rip)
{
    persona_context_t *context;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_set_tid_address_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_SET_TID_ADDRESS,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (clear_child_tid >= LINUX_ABI64_USER_CANONICAL_LIMIT)
    {
        ++g_linux_abi64_set_tid_address_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_SET_TID_ADDRESS,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    context->clear_child_tid = clear_child_tid;
    ++g_linux_abi64_set_tid_address_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_SET_TID_ADDRESS,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)pid;
}

static u32 linux_abi64_clock_supported(u64 clock_id)
{
    return ((clock_id == (u64)LINUX_ABI64_CLOCK_REALTIME)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_MONOTONIC)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_MONOTONIC_RAW)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_REALTIME_COARSE)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_MONOTONIC_COARSE)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_BOOTTIME))
        ? 1u
        : 0u;
}

u64 linux_abi64_sys_clock_gettime(u32 pid, u64 clock_id, u64 user_timespec, u64 rip)
{
    persona_context_t *context;
    linux_abi64_timespec_t timespec;
    u32 ticks;
    u32 frequency;
    u32 remainder;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_clock_gettime_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLOCK_GETTIME,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (linux_abi64_clock_supported(clock_id) == 0u)
    {
        ++g_linux_abi64_clock_gettime_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLOCK_GETTIME,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_writable(
            pid,
            user_timespec,
            LINUX_ABI64_TIMESPEC_BYTES) == 0u)
    {
        ++g_linux_abi64_clock_gettime_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLOCK_GETTIME,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    frequency = linux_abi64_effective_tick_frequency();

    ticks = pit_get_ticks();
    remainder = ticks % frequency;
    timespec.tv_sec = (u64)(ticks / frequency);
    timespec.tv_nsec = ((u64)remainder * 1000000000ull) / (u64)frequency;
    linux_abi64_copy_to_user(
        user_timespec,
        (const u8 *)&timespec,
        LINUX_ABI64_TIMESPEC_BYTES);

    ++g_linux_abi64_clock_gettime_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_CLOCK_GETTIME,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

static u32 linux_abi64_timespec_is_valid(const linux_abi64_timespec_t *timespec)
{
    if (timespec == 0)
    {
        return 0u;
    }

    return (((timespec->tv_sec & 0x8000000000000000ull) == 0ull)
        && (timespec->tv_nsec < 1000000000ull))
        ? 1u
        : 0u;
}

static u32 linux_abi64_effective_tick_frequency(void)
{
    u32 frequency = pit_get_frequency_hz();

    if ((frequency == 0u) || (frequency > LINUX_ABI64_MAX_PIT_TICK_HZ))
    {
        return LINUX_ABI64_DEFAULT_TICK_HZ;
    }

    return frequency;
}

static u32 linux_abi64_timespec_to_ticks(
    const linux_abi64_timespec_t *timespec,
    u32 frequency,
    u32 *out_ticks)
{
    u64 sec_ticks;
    u64 nsec_ticks;
    u64 total_ticks;

    if ((timespec == 0) || (out_ticks == 0) || (frequency == 0u))
    {
        return 0u;
    }

    if (timespec->tv_sec > (0xFFFFFFFFull / (u64)frequency))
    {
        return 0u;
    }

    sec_ticks = timespec->tv_sec * (u64)frequency;
    nsec_ticks =
        ((timespec->tv_nsec * (u64)frequency) + 999999999ull) / 1000000000ull;
    total_ticks = sec_ticks + nsec_ticks;
    if (total_ticks > 0xFFFFFFFFull)
    {
        return 0u;
    }

    *out_ticks = (u32)total_ticks;
    return 1u;
}

static void linux_abi64_ticks_to_timespec(
    u32 ticks,
    u32 frequency,
    linux_abi64_timespec_t *out_timespec)
{
    u32 remainder;

    if ((frequency == 0u) || (out_timespec == 0))
    {
        return;
    }

    remainder = ticks % frequency;
    out_timespec->tv_sec = (u64)(ticks / frequency);
    out_timespec->tv_nsec = ((u64)remainder * 1000000000ull) / (u64)frequency;
}

u64 linux_abi64_sys_nanosleep(u32 pid, u64 user_request, u64 user_remain, u64 rip)
{
    persona_context_t *context;
    linux_abi64_timespec_t request;
    linux_abi64_timespec_t remain;
    const volatile linux_abi64_timespec_t *source;
    u32 frequency;
    u32 requested_ticks;
    u32 elapsed_ticks;
    u32 remaining_ticks;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_nanosleep_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_NANOSLEEP,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (linux_abi64_user_buffer_readable(pid, user_request, LINUX_ABI64_TIMESPEC_BYTES) == 0u)
    {
        ++g_linux_abi64_nanosleep_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_NANOSLEEP,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    source = (const volatile linux_abi64_timespec_t *)(u64)user_request;
    request.tv_sec = source->tv_sec;
    request.tv_nsec = source->tv_nsec;
    if (linux_abi64_timespec_is_valid(&request) == 0u)
    {
        ++g_linux_abi64_nanosleep_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_NANOSLEEP,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    frequency = linux_abi64_effective_tick_frequency();

    if (linux_abi64_timespec_to_ticks(&request, frequency, &requested_ticks) == 0u)
    {
        ++g_linux_abi64_nanosleep_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_NANOSLEEP,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }
    if (scheduler64_sleep_for_ticks(requested_ticks) != 0u)
    {
        ++g_linux_abi64_nanosleep_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_NANOSLEEP,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    ++g_linux_abi64_nanosleep_interrupted_count;
    if (user_remain != 0ull)
    {
        if (linux_abi64_user_buffer_writable(
                pid,
                user_remain,
                LINUX_ABI64_TIMESPEC_BYTES) == 0u)
        {
            ++g_linux_abi64_nanosleep_fault_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_NANOSLEEP,
                LINUX_ABI64_EFAULT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }

        elapsed_ticks = scheduler64_sleep_last_elapsed_ticks();
        remaining_ticks = (elapsed_ticks < requested_ticks)
            ? (requested_ticks - elapsed_ticks)
            : 0u;
        linux_abi64_ticks_to_timespec(remaining_ticks, frequency, &remain);
        linux_abi64_copy_to_user(
            user_remain,
            (const u8 *)&remain,
            LINUX_ABI64_TIMESPEC_BYTES);
    }

    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_NANOSLEEP,
        LINUX_ABI64_EINTR,
        rip);
    return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINTR);
}

u64 linux_abi64_sys_getrlimit(u32 pid, u64 resource, u64 user_rlimit, u64 rip)
{
    persona_context_t *context;
    linux_abi64_rlimit_record_t *record;
    linux_abi64_rlimit_t limit;
    u32 resource_id;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_rlimit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_GETRLIMIT,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (resource >= (u64)LINUX_ABI64_RLIMIT_COUNT)
    {
        ++g_linux_abi64_rlimit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETRLIMIT,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_writable(pid, user_rlimit, LINUX_ABI64_RLIMIT_BYTES) == 0u)
    {
        ++g_linux_abi64_rlimit_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETRLIMIT,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    record = linux_abi64_rlimit_record_for_pid(pid);
    if (record == 0)
    {
        ++g_linux_abi64_rlimit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_GETRLIMIT,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    resource_id = (u32)resource;
    limit.rlim_cur = record->current[resource_id];
    limit.rlim_max = record->maximum[resource_id];
    linux_abi64_copy_to_user(user_rlimit, (const u8 *)&limit, LINUX_ABI64_RLIMIT_BYTES);

    ++g_linux_abi64_getrlimit_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_GETRLIMIT,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_setrlimit(u32 pid, u64 resource, u64 user_rlimit, u64 rip)
{
    persona_context_t *context;
    linux_abi64_rlimit_record_t *record;
    linux_abi64_rlimit_t requested;
    const volatile linux_abi64_rlimit_t *source;
    u32 resource_id;
    u64 budget;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_rlimit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_SETRLIMIT,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (resource >= (u64)LINUX_ABI64_RLIMIT_COUNT)
    {
        ++g_linux_abi64_rlimit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_SETRLIMIT,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_readable(pid, user_rlimit, LINUX_ABI64_RLIMIT_BYTES) == 0u)
    {
        ++g_linux_abi64_rlimit_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_SETRLIMIT,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    source = (const volatile linux_abi64_rlimit_t *)(u64)user_rlimit;
    requested.rlim_cur = source->rlim_cur;
    requested.rlim_max = source->rlim_max;
    resource_id = (u32)resource;
    budget = linux_abi64_rlimit_budget(resource_id);
    if ((requested.rlim_cur > requested.rlim_max)
        || (requested.rlim_cur > budget)
        || (requested.rlim_max > budget))
    {
        ++g_linux_abi64_rlimit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_SETRLIMIT,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    record = linux_abi64_rlimit_record_for_pid(pid);
    if (record == 0)
    {
        ++g_linux_abi64_rlimit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_SETRLIMIT,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    record->current[resource_id] = requested.rlim_cur;
    record->maximum[resource_id] = requested.rlim_max;
    ++g_linux_abi64_setrlimit_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_SETRLIMIT,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

static u64 linux_abi64_sys_pipe_common(
    u32 pid,
    u64 user_pipefd,
    u64 flags,
    u32 syscall_number,
    u32 *success_count,
    u32 *denial_count,
    u32 *fault_count,
    u64 rip)
{
    persona_context_t *context;
    u32 fd_flags;
    u32 read_fd = FD64_INVALID_FD;
    u32 write_fd = FD64_INVALID_FD;
    u32 pipefd[2];

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF)
        || (fd64_table_for_process(pid) == 0))
    {
        if (denial_count != 0)
        {
            ++(*denial_count);
        }
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (linux_abi64_pipe_flags_to_fd(flags, &fd_flags) == 0u)
    {
        if (denial_count != 0)
        {
            ++(*denial_count);
        }
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_writable(pid, user_pipefd, 2u * (u32)sizeof(u32)) == 0u)
    {
        if (fault_count != 0)
        {
            ++(*fault_count);
        }
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if (pipe64_create_flags(pid, fd_flags, &read_fd, &write_fd) == 0u)
    {
        if (denial_count != 0)
        {
            ++(*denial_count);
        }
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_EMFILE,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EMFILE);
    }

    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    linux_abi64_copy_to_user(user_pipefd, (const u8 *)&pipefd[0], 2u * (u32)sizeof(u32));

    if (success_count != 0)
    {
        ++(*success_count);
    }
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_pipe2(u32 pid, u64 user_pipefd, u64 flags, u64 rip)
{
    return linux_abi64_sys_pipe_common(
        pid,
        user_pipefd,
        flags,
        LINUX_ABI64_SYSCALL_PIPE2,
        &g_linux_abi64_pipe2_count,
        &g_linux_abi64_pipe2_denial_count,
        &g_linux_abi64_pipe2_fault_count,
        rip);
}

u64 linux_abi64_sys_pipe(u32 pid, u64 user_pipefd, u64 rip)
{
    return linux_abi64_sys_pipe_common(
        pid,
        user_pipefd,
        0ull,
        LINUX_ABI64_SYSCALL_PIPE,
        &g_linux_abi64_pipe_count,
        &g_linux_abi64_pipe_denial_count,
        &g_linux_abi64_pipe_fault_count,
        rip);
}

static u32 linux_abi64_dup_persona_ready(u32 pid)
{
    persona_context_t *context = persona64_context_for_process(pid);

    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (context != 0)
        && (context->persona_type == PERSONA64_TYPE_LINUX_ELF)
        && (fd64_table_for_process(pid) != 0))
        ? 1u
        : 0u;
}

u64 linux_abi64_sys_dup(u32 pid, u64 old_fd_number, u64 rip)
{
    u32 new_fd;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if (linux_abi64_dup_persona_ready(pid) == 0u)
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((old_fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, (u32)old_fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    new_fd = fd64_dup(pid, (u32)old_fd_number);
    if (new_fd == FD64_INVALID_FD)
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP,
            LINUX_ABI64_EMFILE,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EMFILE);
    }

    ++g_linux_abi64_dup_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_DUP,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)new_fd;
}

u64 linux_abi64_sys_dup2(u32 pid, u64 old_fd_number, u64 new_fd_number, u64 rip)
{
    u32 duplicated_fd;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if (linux_abi64_dup_persona_ready(pid) == 0u)
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP2,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((old_fd_number >= (u64)FD64_TABLE_LIMIT)
        || (new_fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, (u32)old_fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP2,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    duplicated_fd = fd64_dup2(pid, (u32)old_fd_number, (u32)new_fd_number);
    if (duplicated_fd == FD64_INVALID_FD)
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP2,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    ++g_linux_abi64_dup2_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_DUP2,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)duplicated_fd;
}

u64 linux_abi64_sys_dup3(
    u32 pid,
    u64 old_fd_number,
    u64 new_fd_number,
    u64 flags,
    u64 rip)
{
    u32 fd_flags;
    u32 duplicated_fd;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if (linux_abi64_dup_persona_ready(pid) == 0u)
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP3,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((old_fd_number >= (u64)FD64_TABLE_LIMIT)
        || (new_fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, (u32)old_fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP3,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    if ((old_fd_number == new_fd_number)
        || (linux_abi64_dup3_flags_to_fd(flags, &fd_flags) == 0u))
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_DUP3,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    duplicated_fd = fd64_dup3(pid, (u32)old_fd_number, (u32)new_fd_number, fd_flags);
    if (duplicated_fd == FD64_INVALID_FD)
    {
        ++g_linux_abi64_dup_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_DUP3,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    ++g_linux_abi64_dup3_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_DUP3,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)duplicated_fd;
}

u64 linux_abi64_sys_fcntl(u32 pid, u64 fd_number, u64 command, u64 argument, u64 rip)
{
    u32 fd;
    u32 current_flags;
    u32 new_flags;
    u32 duplicated_fd;
    u64 return_value = 0ull;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if (linux_abi64_dup_persona_ready(pid) == 0u)
    {
        ++g_linux_abi64_fcntl_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FCNTL,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_fcntl_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FCNTL,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    fd = (u32)fd_number;
    current_flags = fd64_entry_flags(pid, fd);

    if (command == (u64)LINUX_ABI64_F_DUPFD)
    {
        if (argument >= (u64)FD64_TABLE_LIMIT)
        {
            ++g_linux_abi64_fcntl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FCNTL,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        duplicated_fd = fd64_dup_min(pid, fd, (u32)argument);
        if (duplicated_fd == FD64_INVALID_FD)
        {
            ++g_linux_abi64_fcntl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                LINUX_ABI64_SYSCALL_FCNTL,
                LINUX_ABI64_EMFILE,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EMFILE);
        }

        new_flags = fd64_entry_flags(pid, duplicated_fd) & ~((u32)FD64_FLAG_O_CLOEXEC);
        if (fd64_set_entry_flags(pid, duplicated_fd, new_flags) == 0u)
        {
            (void)fd64_close(pid, duplicated_fd);
            ++g_linux_abi64_fcntl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                LINUX_ABI64_SYSCALL_FCNTL,
                LINUX_ABI64_EBADF,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
        }

        return_value = (u64)duplicated_fd;
    }
    else if (command == (u64)LINUX_ABI64_F_GETFD)
    {
        return_value = linux_abi64_fcntl_descriptor_flags_from_fd(current_flags);
    }
    else if (command == (u64)LINUX_ABI64_F_SETFD)
    {
        if ((argument & ~(u64)LINUX_ABI64_FD_CLOEXEC) != 0ull)
        {
            ++g_linux_abi64_fcntl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FCNTL,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        new_flags = current_flags & ~((u32)FD64_FLAG_O_CLOEXEC);
        if ((argument & (u64)LINUX_ABI64_FD_CLOEXEC) != 0ull)
        {
            new_flags |= FD64_FLAG_O_CLOEXEC;
        }
        if (fd64_set_entry_flags(pid, fd, new_flags) == 0u)
        {
            ++g_linux_abi64_fcntl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                LINUX_ABI64_SYSCALL_FCNTL,
                LINUX_ABI64_EBADF,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
        }
    }
    else if (command == (u64)LINUX_ABI64_F_GETFL)
    {
        return_value = linux_abi64_fcntl_status_flags_from_fd(current_flags);
    }
    else if (command == (u64)LINUX_ABI64_F_SETFL)
    {
        if (linux_abi64_fcntl_setfl_to_fd(argument, current_flags, &new_flags) == 0u)
        {
            ++g_linux_abi64_fcntl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FCNTL,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        if (fd64_set_entry_flags(pid, fd, new_flags) == 0u)
        {
            ++g_linux_abi64_fcntl_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                LINUX_ABI64_SYSCALL_FCNTL,
                LINUX_ABI64_EBADF,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
        }
    }
    else
    {
        ++g_linux_abi64_fcntl_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FCNTL,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    ++g_linux_abi64_fcntl_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_FCNTL,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return return_value;
}

static u32 linux_abi64_set_cwd(
    persona_context_t *context,
    const u8 *path,
    u32 path_byte_count)
{
    u32 index;

    if ((context == 0)
        || (path == 0)
        || (path_byte_count == 0u)
        || (path_byte_count >= PERSONA64_LINUX_CWD_MAX_BYTES))
    {
        return 0u;
    }

    context->linux_cwd_length = path_byte_count;
    for (index = 0u; index < path_byte_count; ++index)
    {
        context->linux_cwd[index] = path[index];
    }
    for (; index < PERSONA64_LINUX_CWD_MAX_BYTES; ++index)
    {
        context->linux_cwd[index] = 0u;
    }

    return 1u;
}

u64 linux_abi64_sys_getcwd(u32 pid, u64 user_buffer, u64 byte_count, u64 rip)
{
    persona_context_t *context;
    u8 terminator = 0u;
    u32 required_bytes;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_getcwd_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_GETCWD,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((context->linux_cwd_length == 0u)
        || (context->linux_cwd_length >= PERSONA64_LINUX_CWD_MAX_BYTES))
    {
        ++g_linux_abi64_getcwd_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETCWD,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    required_bytes = context->linux_cwd_length + 1u;
    if (byte_count < (u64)required_bytes)
    {
        ++g_linux_abi64_getcwd_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETCWD,
            LINUX_ABI64_ERANGE,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ERANGE);
    }

    if (linux_abi64_user_buffer_writable(pid, user_buffer, required_bytes) == 0u)
    {
        ++g_linux_abi64_getcwd_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETCWD,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    linux_abi64_copy_to_user(user_buffer, &context->linux_cwd[0], context->linux_cwd_length);
    linux_abi64_copy_to_user(
        user_buffer + (u64)context->linux_cwd_length,
        &terminator,
        1u);

    ++g_linux_abi64_getcwd_count;
    g_linux_abi64_getcwd_byte_count += required_bytes;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_GETCWD,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)required_bytes;
}

u64 linux_abi64_sys_chdir(u32 pid, u64 user_path, u64 rip)
{
    persona_context_t *context;
    u8 user_path_bytes[LINUX_VFS64_MAX_PATH_BYTES + 1u];
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 user_path_byte_count;
    u32 path_byte_count;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CHDIR,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (linux_abi64_copy_user_path(
            pid,
            user_path,
            user_path_bytes,
            LINUX_VFS64_MAX_PATH_BYTES,
            &user_path_byte_count) == 0u)
    {
        ++g_linux_abi64_chdir_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CHDIR,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }
    if (linux_abi64_canonicalize_path(
            pid,
            LINUX_ABI64_AT_FDCWD,
            user_path_bytes,
            user_path_byte_count,
            path,
            (u32)sizeof(path),
            &path_byte_count) == 0u)
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CHDIR,
            LINUX_ABI64_ENOENT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOENT);
    }

    if (linux_vfs64_path_is_directory(pid, path, path_byte_count) == 0u)
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CHDIR,
            LINUX_ABI64_ENOTDIR,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOTDIR);
    }

    if (linux_abi64_set_cwd(context, path, path_byte_count) == 0u)
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CHDIR,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    ++g_linux_abi64_chdir_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_CHDIR,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_fchdir(u32 pid, u64 fd_number, u64 rip)
{
    persona_context_t *context;
    u8 path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 path_byte_count;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FCHDIR,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FCHDIR,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    if ((linux_vfs64_fd_path(
                pid,
                (u32)fd_number,
                path,
                LINUX_VFS64_MAX_PATH_BYTES,
                &path_byte_count) == 0u)
        || (linux_vfs64_path_is_directory(pid, path, path_byte_count) == 0u))
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FCHDIR,
            LINUX_ABI64_ENOTDIR,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOTDIR);
    }

    if (linux_abi64_set_cwd(context, path, path_byte_count) == 0u)
    {
        ++g_linux_abi64_chdir_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FCHDIR,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    ++g_linux_abi64_fchdir_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_FCHDIR,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

static u32 linux_abi64_dirent64_record_length(u32 name_byte_count)
{
    u32 raw_length = LINUX_ABI64_DIRENT64_HEADER_BYTES + name_byte_count + 1u;
    u32 mask = LINUX_ABI64_DIRENT64_ALIGN_BYTES - 1u;

    return (raw_length + mask) & ~mask;
}

static void linux_abi64_put_le16(u8 *buffer, u32 offset, u16 value)
{
    buffer[offset] = (u8)(value & 0xFFu);
    buffer[offset + 1u] = (u8)((value >> 8) & 0xFFu);
}

static void linux_abi64_put_le64(u8 *buffer, u32 offset, u64 value)
{
    u32 index;

    for (index = 0u; index < 8u; ++index)
    {
        buffer[offset + index] = (u8)((value >> (index * 8u)) & 0xFFull);
    }
}

static void linux_abi64_zero_bytes(u8 *buffer, u32 byte_count)
{
    u32 index;

    if (buffer == 0)
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        buffer[index] = 0u;
    }
}

static u32 linux_abi64_format_dirent64(
    const linux_vfs64_dirent_t *entry,
    u8 *record,
    u32 record_capacity,
    u32 *record_length_out)
{
    u32 record_length;
    u32 index;

    if (record_length_out != 0)
    {
        *record_length_out = 0u;
    }

    if ((entry == 0)
        || (record == 0)
        || (record_length_out == 0)
        || (entry->name_byte_count == 0u)
        || (entry->name_byte_count >= LINUX_VFS64_DIRENT_NAME_MAX))
    {
        return 0u;
    }

    record_length = linux_abi64_dirent64_record_length(entry->name_byte_count);
    if ((record_length > record_capacity)
        || (record_length > 0xFFFFu))
    {
        return 0u;
    }

    linux_abi64_zero_bytes(record, record_capacity);
    linux_abi64_put_le64(record, 0u, entry->inode);
    linux_abi64_put_le64(record, 8u, (u64)entry->next_offset);
    linux_abi64_put_le16(record, 16u, (u16)record_length);
    record[18] = entry->entry_type;
    for (index = 0u; index < entry->name_byte_count; ++index)
    {
        record[LINUX_ABI64_DIRENT64_HEADER_BYTES + index] = entry->name[index];
    }
    record[LINUX_ABI64_DIRENT64_HEADER_BYTES + entry->name_byte_count] = 0u;
    *record_length_out = record_length;
    return 1u;
}

u64 linux_abi64_sys_getdents64(u32 pid, u64 fd_number, u64 user_dirents, u64 byte_count, u64 rip)
{
    persona_context_t *context;
    linux_vfs64_dirent_t entry;
    u8 record[LINUX_ABI64_DIRENT64_MAX_RECORD_BYTES];
    u32 cursor;
    u32 bytes_written = 0u;
    u32 entries_written = 0u;
    u32 available;
    u32 read_result;
    u32 record_length;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_getdents64_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_GETDENTS64,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((fd_number >= (u64)FD64_TABLE_LIMIT)
        || (fd64_entry_type(pid, (u32)fd_number) == FD64_TYPE_EMPTY))
    {
        ++g_linux_abi64_getdents64_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_GETDENTS64,
            LINUX_ABI64_EBADF,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EBADF);
    }

    if ((byte_count == 0ull) || (byte_count > 0xFFFFFFFFull))
    {
        ++g_linux_abi64_getdents64_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETDENTS64,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_writable(pid, user_dirents, (u32)byte_count) == 0u)
    {
        ++g_linux_abi64_getdents64_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETDENTS64,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if (linux_vfs64_fd_dir_cursor(pid, (u32)fd_number, &cursor) == 0u)
    {
        ++g_linux_abi64_getdents64_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETDENTS64,
            LINUX_ABI64_ENOTDIR,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOTDIR);
    }

    while (bytes_written < (u32)byte_count)
    {
        read_result = linux_vfs64_read_dirent(pid, (u32)fd_number, cursor, &entry);
        if (read_result == LINUX_VFS64_READDIR_EOF)
        {
            break;
        }
        if (read_result != LINUX_VFS64_READDIR_OK)
        {
            if (entries_written == 0u)
            {
                ++g_linux_abi64_getdents64_denial_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                    LINUX_ABI64_SYSCALL_GETDENTS64,
                    LINUX_ABI64_ENOTDIR,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOTDIR);
            }
            break;
        }

        if (linux_abi64_format_dirent64(
                &entry,
                record,
                LINUX_ABI64_DIRENT64_MAX_RECORD_BYTES,
                &record_length) == 0u)
        {
            ++g_linux_abi64_getdents64_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_GETDENTS64,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        available = (u32)byte_count - bytes_written;
        if (record_length > available)
        {
            if (entries_written == 0u)
            {
                ++g_linux_abi64_getdents64_denial_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                    LINUX_ABI64_SYSCALL_GETDENTS64,
                    LINUX_ABI64_EINVAL,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
            }
            break;
        }

        linux_abi64_copy_to_user(user_dirents + (u64)bytes_written, record, record_length);
        bytes_written += record_length;
        ++entries_written;
        ++cursor;
    }

    if (linux_vfs64_set_fd_dir_cursor(pid, (u32)fd_number, cursor) == 0u)
    {
        ++g_linux_abi64_getdents64_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETDENTS64,
            LINUX_ABI64_ENOTDIR,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOTDIR);
    }

    ++g_linux_abi64_getdents64_count;
    g_linux_abi64_getdents64_entry_count += entries_written;
    g_linux_abi64_getdents64_byte_count += bytes_written;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_GETDENTS64,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)bytes_written;
}

u64 linux_abi64_sys_futex(
    u32 pid,
    u64 user_address,
    u64 futex_op,
    u64 value,
    u64 timeout,
    u64 user_address2,
    u64 value3,
    u64 rip)
{
    persona_context_t *context;
    u32 op32;
    u32 command;
    u32 current_value;
    const volatile u32 *user_word;
    linux_abi64_futex_waiter_t *waiter;
    u32 index;
    u32 wake_limit;
    u32 wake_count = 0u;
    u32 task_id;
    u32 wake_task_id;
    linux_abi64_timespec_t timeout_value;
    const volatile linux_abi64_timespec_t *timeout_source;
    u32 timeout_ticks = 0u;
    u32 timed_wait = 0u;

    (void)user_address2;
    (void)value3;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_futex_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FUTEX,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    op32 = (u32)(futex_op & 0xFFFFFFFFull);
    if ((op32 & ~(LINUX_ABI64_FUTEX_CMD_MASK | LINUX_ABI64_FUTEX_PRIVATE_FLAG)) != 0u)
    {
        ++g_linux_abi64_futex_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FUTEX,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    command = op32 & LINUX_ABI64_FUTEX_CMD_MASK;
    if ((user_address == 0ull) || ((user_address & 0x3ull) != 0ull))
    {
        ++g_linux_abi64_futex_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FUTEX,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_readable(pid, user_address, (u32)sizeof(u32)) == 0u)
    {
        ++g_linux_abi64_futex_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FUTEX,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    user_word = (const volatile u32 *)(u64)user_address;
    current_value = *user_word;

    if (command == LINUX_ABI64_FUTEX_WAIT)
    {
        if (timeout != 0ull)
        {
            if (linux_abi64_user_buffer_readable(
                    pid,
                    timeout,
                    LINUX_ABI64_TIMESPEC_BYTES) == 0u)
            {
                ++g_linux_abi64_futex_fault_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                    LINUX_ABI64_SYSCALL_FUTEX,
                    LINUX_ABI64_EFAULT,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
            }

            timeout_source = (const volatile linux_abi64_timespec_t *)(u64)timeout;
            timeout_value.tv_sec = timeout_source->tv_sec;
            timeout_value.tv_nsec = timeout_source->tv_nsec;
            if ((linux_abi64_timespec_is_valid(&timeout_value) == 0u)
                || (linux_abi64_timespec_to_ticks(
                    &timeout_value,
                    linux_abi64_effective_tick_frequency(),
                    &timeout_ticks) == 0u))
            {
                ++g_linux_abi64_futex_denial_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                    LINUX_ABI64_SYSCALL_FUTEX,
                    LINUX_ABI64_EINVAL,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
            }

            timed_wait = 1u;
        }

        if (current_value != (u32)(value & 0xFFFFFFFFull))
        {
            ++g_linux_abi64_futex_eagain_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FUTEX,
                LINUX_ABI64_EAGAIN,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
        }

        if ((timed_wait != 0u) && (timeout_ticks == 0u))
        {
            ++g_linux_abi64_futex_timeout_count;
            g_linux_abi64_futex_last_timeout_task_id = SCHEDULER64_INVALID_TASK;
            g_linux_abi64_futex_last_timeout_ticks = 0u;
            g_linux_abi64_futex_last_timeout_result = LINUX_ABI64_ETIMEDOUT;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FUTEX,
                LINUX_ABI64_ETIMEDOUT,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ETIMEDOUT);
        }

        if (linux_abi64_futex_waiter_for(pid, user_address) != 0)
        {
            ++g_linux_abi64_futex_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FUTEX,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        task_id = scheduler64_runqueue_current_task_id();
        if ((task_id == SCHEDULER64_INVALID_TASK)
            || (scheduler64_runqueue_task_pid(task_id) != pid)
            || (scheduler64_runqueue_task_state(task_id) != SCHEDULER64_TASK_RUNNING))
        {
            ++g_linux_abi64_futex_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FUTEX,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        waiter = linux_abi64_futex_free_waiter();
        if (waiter == 0)
        {
            ++g_linux_abi64_futex_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
                LINUX_ABI64_SYSCALL_FUTEX,
                LINUX_ABI64_ENOMEM,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
        }

        waiter->active = 1u;
        waiter->pid = pid;
        waiter->task_id = task_id;
        waiter->timed = timed_wait;
        waiter->expected_value = current_value;
        waiter->timeout_ticks = timeout_ticks;
        waiter->timeout_result = (timed_wait != 0u) ? LINUX_ABI64_ETIMEDOUT : 0u;
        waiter->user_address = user_address;
        waiter->rip = rip;
        if (timed_wait != 0u)
        {
            if (scheduler64_sleep_current_task_for_ticks(
                    timeout_ticks,
                    LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ETIMEDOUT),
                    linux_abi64_futex_timeout_callback,
                    (u64)waiter) == 0u)
            {
                linux_abi64_clear_futex_waiter(waiter);
                ++g_linux_abi64_futex_denial_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                    LINUX_ABI64_SYSCALL_FUTEX,
                    LINUX_ABI64_EINVAL,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
            }

            ++g_linux_abi64_futex_timed_wait_count;
        }
        else if (scheduler64_runqueue_block_task(task_id) == 0u)
        {
            linux_abi64_clear_futex_waiter(waiter);
            ++g_linux_abi64_futex_denial_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_FUTEX,
                LINUX_ABI64_EINVAL,
                rip);
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
        }

        ++g_linux_abi64_futex_wait_count;
        g_linux_abi64_futex_last_wait_pid = pid;
        g_linux_abi64_futex_last_wait_address = user_address;
        g_linux_abi64_futex_last_wait_value = current_value;
        g_linux_abi64_futex_last_wait_task_id = task_id;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FUTEX,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    if (command == LINUX_ABI64_FUTEX_WAKE)
    {
        wake_limit = (value > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (u32)value;
        for (index = 0u;
            (index < LINUX_ABI64_MAX_FUTEX_WAITERS) && (wake_count < wake_limit);
            ++index)
        {
            if ((g_linux_abi64_futex_waiters[index].active != 0u)
                && (g_linux_abi64_futex_waiters[index].pid == pid)
                && (g_linux_abi64_futex_waiters[index].user_address == user_address))
            {
                wake_task_id = g_linux_abi64_futex_waiters[index].task_id;
                if (g_linux_abi64_futex_waiters[index].timed != 0u)
                {
                    (void)scheduler64_sleep_cancel_task(wake_task_id);
                }

                if (scheduler64_runqueue_wake_task_with_result(wake_task_id, 0ull) != 0u)
                {
                    ++wake_count;
                }
                else
                {
                    ++g_linux_abi64_futex_denial_count;
                }
                linux_abi64_clear_futex_waiter(&g_linux_abi64_futex_waiters[index]);
            }
        }

        ++g_linux_abi64_futex_wake_count;
        g_linux_abi64_futex_woken_count += wake_count;
        g_linux_abi64_futex_last_wake_count = wake_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FUTEX,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return (u64)wake_count;
    }

    ++g_linux_abi64_futex_denial_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_FUTEX,
        LINUX_ABI64_EINVAL,
        rip);
    return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
}

u64 linux_abi64_sys_clone(
    u32 pid,
    u64 flags,
    u64 child_stack,
    u64 parent_tid,
    u64 child_tid,
    u64 tls,
    u64 rip)
{
    persona_context_t *parent_context;
    persona_context_t *child_context;
    linux_abi64_clone_record_t *record;
    void *parent_vma;
    void *parent_fd;
    void *parent_audit;
    u32 flags32 = (u32)(flags & 0xFFFFFFFFull);
    u32 child_pid = PROCESS64_INVALID_PID;
    u32 task_id;
    u32 runtime_token;
    u32 entry_token;
    u64 selectors;
    u64 rflags;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    parent_context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (parent_context == 0)
        || (parent_context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((flags32 & ~LINUX_ABI64_CLONE_SUPPORTED_MASK) != 0u)
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if ((flags32 & LINUX_ABI64_CLONE_THREAD_REQUIRED)
        != LINUX_ABI64_CLONE_THREAD_REQUIRED)
    {
        ++g_linux_abi64_clone_denial_count;
        ++g_linux_abi64_clone_fork_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED,
            LINUX_ABI64_SYSCALL_CLONE,
            PERSONA_AUDIT64_RESULT_ENOSYS,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOSYS);
    }

    if ((child_stack == 0ull)
        || (child_stack >= LINUX_ABI64_USER_CANONICAL_LIMIT)
        || (child_stack < 8ull))
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (linux_abi64_user_buffer_writable(pid, child_stack - 8ull, 8u) == 0u)
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if (((flags32 & LINUX_ABI64_CLONE_PARENT_SETTID) != 0u)
        && (linux_abi64_user_buffer_writable(pid, parent_tid, (u32)sizeof(u32)) == 0u))
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    if (((flags32 & (LINUX_ABI64_CLONE_CHILD_SETTID | LINUX_ABI64_CLONE_CHILD_CLEARTID)) != 0u)
        && (child_tid != 0ull)
        && (linux_abi64_user_buffer_writable(pid, child_tid, (u32)sizeof(u32)) == 0u))
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    parent_vma = process64_vma_root(pid);
    parent_fd = process64_fd_table(pid);
    parent_audit = process64_audit_ctx(pid);
    if ((parent_vma == 0) || (parent_fd == 0) || (parent_audit == 0))
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    runtime_token = process64_runtime_token(pid);
    entry_token = process64_runtime_user_entry_token(pid);
    selectors = (u64)process64_runtime_user_entry_selectors(pid);
    rflags = (u64)process64_runtime_user_entry_rflags(pid);
    if ((runtime_token == 0u)
        || (entry_token == 0u)
        || (selectors == 0ull)
        || (rflags == 0ull)
        || (rip == 0ull))
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    record = linux_abi64_clone_free_record();
    if (record == 0)
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    child_pid = process64_spawn_clone(pid);
    if (child_pid == PROCESS64_INVALID_PID)
    {
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    if ((process64_attach_vma(child_pid, parent_vma) == 0u)
        || (process64_attach_fd(child_pid, parent_fd) == 0u)
        || (process64_attach_audit(child_pid, parent_audit) == 0u)
        || (persona64_init_linux_elf(child_pid, linux_abi64_dispatch_table())
            != PERSONA64_ATTACH_OK))
    {
        (void)persona64_release(child_pid);
        linux_abi64_clone_detach_shared_state(child_pid);
        (void)process64_release_clone(child_pid);
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    child_context = persona64_context_for_process(child_pid);
    if (child_context == 0)
    {
        linux_abi64_clone_detach_shared_state(child_pid);
        (void)process64_release_clone(child_pid);
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    child_context->tls_base =
        ((flags32 & LINUX_ABI64_CLONE_SETTLS) != 0u) ? tls : parent_context->tls_base;
    child_context->tls_size = parent_context->tls_size;
    child_context->clear_child_tid =
        ((flags32 & LINUX_ABI64_CLONE_CHILD_CLEARTID) != 0u) ? child_tid : 0ull;
    child_context->brk_base = parent_context->brk_base;
    child_context->brk_current = parent_context->brk_current;
    child_context->heap_cap = parent_context->heap_cap;
    child_context->persona_module_handle = parent_context->persona_module_handle;
    child_context->capability_attenuation_mask = parent_context->capability_attenuation_mask;
    child_context->load_bias_low = parent_context->load_bias_low;
    child_context->process_group_id = parent_context->process_group_id;
    child_context->linux_cwd_length = parent_context->linux_cwd_length;
    for (task_id = 0u; task_id < PERSONA64_LINUX_CWD_MAX_BYTES; ++task_id)
    {
        child_context->linux_cwd[task_id] = parent_context->linux_cwd[task_id];
    }

    task_id = scheduler64_runqueue_register_process_task(
        child_pid,
        runtime_token,
        entry_token,
        rip,
        child_stack,
        selectors,
        rflags);
    if (task_id == SCHEDULER64_INVALID_TASK)
    {
        (void)persona64_release(child_pid);
        linux_abi64_clone_detach_shared_state(child_pid);
        (void)process64_release_clone(child_pid);
        ++g_linux_abi64_clone_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_CLONE,
            LINUX_ABI64_EAGAIN,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)scheduler64_runqueue_set_task_fs_base(task_id, child_context->tls_base);
#endif

    if ((flags32 & LINUX_ABI64_CLONE_PARENT_SETTID) != 0u)
    {
        *((volatile u32 *)(u64)parent_tid) = child_pid;
    }
    if (((flags32 & LINUX_ABI64_CLONE_CHILD_SETTID) != 0u) && (child_tid != 0ull))
    {
        *((volatile u32 *)(u64)child_tid) = child_pid;
    }

    record->active = 1u;
    record->parent_pid = pid;
    record->child_pid = child_pid;
    record->flags = flags32;
    record->task_id = task_id;
    record->shared_vma = (process64_vma_root(child_pid) == parent_vma) ? 1u : 0u;
    record->shared_fd = (process64_fd_table(child_pid) == parent_fd) ? 1u : 0u;
    record->shared_audit = (process64_audit_ctx(child_pid) == parent_audit) ? 1u : 0u;
    record->child_stack = child_stack;
    record->tls_base = child_context->tls_base;

    ++g_linux_abi64_clone_count;
    ++g_linux_abi64_clone_thread_count;
    ++g_linux_abi64_clone_scheduler_count;
    g_linux_abi64_clone_last_parent_pid = pid;
    g_linux_abi64_clone_last_child_pid = child_pid;
    g_linux_abi64_clone_last_flags = flags32;
    g_linux_abi64_clone_last_task_id = task_id;
    g_linux_abi64_clone_last_shared_vma = record->shared_vma;
    g_linux_abi64_clone_last_shared_fd = record->shared_fd;
    g_linux_abi64_clone_last_shared_audit = record->shared_audit;
    g_linux_abi64_clone_last_child_stack = child_stack;
    g_linux_abi64_clone_last_tls_base = child_context->tls_base;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_CLONE,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)child_pid;
}

u64 linux_abi64_sys_fork(u32 pid, u64 rip)
{
    persona_context_t *context;
    linux_abi64_clone_record_t *record;
    u32 child_pid = PROCESS64_INVALID_PID;
    u32 child_owner;
    u32 root_authority;
    u32 child_root_token;
    u32 runtime_token;
    u32 entry_token;
    u64 selectors;
    u64 rflags;
    struct interrupt_frame64 child_frame;
    u32 task_id;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_fork_last_rip = rip;
    g_linux_abi64_fork_last_child_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_fork_last_child_slot = 0xFFFFFFFFu;
    g_linux_abi64_fork_last_child_root_distinct = 0u;
    g_linux_abi64_fork_last_task_id = SCHEDULER64_INVALID_TASK;
    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_fork_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FORK,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    runtime_token = process64_runtime_token(pid);
    entry_token = process64_runtime_user_entry_token(pid);
    selectors = (u64)process64_runtime_user_entry_selectors(pid);
    rflags = (u64)process64_runtime_user_entry_rflags(pid);
    root_authority = process64_page_root_token(pid);
    if ((runtime_token == 0u)
        || (entry_token == 0u)
        || (selectors == 0ull)
        || (rflags == 0ull)
        || (root_authority == 0u)
        || (rip == 0ull)
        || (syscall64_native_user_rsp == 0ull))
    {
        ++g_linux_abi64_fork_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_FORK,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    record = linux_abi64_clone_free_record();
    if (record == 0)
    {
        ++g_linux_abi64_fork_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FORK,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    ++g_linux_abi64_fork_count;
    child_pid = process64_spawn_fork(pid);
    child_owner = process64_principal(child_pid);
    if ((child_pid == PROCESS64_INVALID_PID) || (child_owner == 0u))
    {
        ++g_linux_abi64_fork_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FORK,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    if ((paging64_process_root_fork_alloc(pid, child_pid, child_owner, root_authority) == 0u)
        || ((child_root_token = paging64_process_root_token(child_pid)) == 0u)
        || (process64_attach_page_root(
                child_pid,
                paging64_process_root_physical(child_pid),
                paging64_process_root_slot(child_pid),
                child_root_token,
                root_authority) == 0u)
        || (vma64_fork_copy_process(pid, child_pid) == 0u)
        || (persona_audit64_attach(child_pid) == 0u)
        || (fd64_fork_process(pid, child_pid) == 0u)
        || (persona64_fork_linux_elf(pid, child_pid, linux_abi64_dispatch_table())
            != PERSONA64_ATTACH_OK)
        || (linux_vfs64_fork_process(pid, child_pid) == 0u))
    {
        linux_abi64_fork_release_setup(child_pid);
        ++g_linux_abi64_fork_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FORK,
            LINUX_ABI64_ENOMEM,
            rip);
        if (linux_abi64_restore_return_root(pid) == 0u)
        {
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    child_frame.r15 = syscall64_native_user_r15;
    child_frame.r14 = syscall64_native_user_r14;
    child_frame.r13 = syscall64_native_user_r13;
    child_frame.r12 = syscall64_native_user_r12;
    child_frame.r11 = 0ull;
    child_frame.r10 = syscall64_native_linux_r10;
    child_frame.r9 = syscall64_native_linux_r9;
    child_frame.r8 = syscall64_native_linux_r8;
    child_frame.rdi = syscall64_native_linux_rdi;
    child_frame.rsi = syscall64_native_linux_rsi;
    child_frame.rbp = syscall64_native_user_rbp;
    child_frame.rbx = syscall64_native_user_rbx;
    child_frame.rdx = syscall64_native_linux_rdx;
    child_frame.rcx = rip;
    child_frame.rax = 0ull;
    child_frame.vector = 0ull;
    child_frame.error_code = 0ull;
    child_frame.rip = rip;
    child_frame.cs = selectors & 0xFFFFull;
    child_frame.rflags = rflags;
    child_frame.rsp = syscall64_native_user_rsp;
    child_frame.ss = (selectors >> 16) & 0xFFFFull;

    task_id = scheduler64_runqueue_register_process_frame(
        child_pid,
        runtime_token,
        entry_token,
        &child_frame);
    if (task_id == SCHEDULER64_INVALID_TASK)
    {
        linux_abi64_fork_release_setup(child_pid);
        ++g_linux_abi64_fork_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_FORK,
            LINUX_ABI64_EAGAIN,
            rip);
        if (linux_abi64_restore_return_root(pid) == 0u)
        {
            return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
        }
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)scheduler64_runqueue_set_task_fs_base(task_id, context->tls_base);
#endif

    record->active = 1u;
    record->parent_pid = pid;
    record->child_pid = child_pid;
    record->flags = 0u;
    record->task_id = task_id;
    record->shared_vma = 0u;
    record->shared_fd = 0u;
    record->shared_audit = 0u;
    record->child_stack = syscall64_native_user_rsp;
    record->tls_base = context->tls_base;

    ++g_linux_abi64_fork_success_count;
    ++g_linux_abi64_clone_scheduler_count;
    g_linux_abi64_fork_last_child_pid = child_pid;
    g_linux_abi64_fork_last_child_slot = paging64_process_root_fork_last_child_slot();
    g_linux_abi64_fork_last_child_root_distinct =
        paging64_process_root_fork_last_child_root_distinct();
    g_linux_abi64_fork_last_task_id = task_id;
    g_linux_abi64_clone_last_parent_pid = pid;
    g_linux_abi64_clone_last_child_pid = child_pid;
    g_linux_abi64_clone_last_flags = 0u;
    g_linux_abi64_clone_last_task_id = task_id;
    g_linux_abi64_clone_last_shared_vma = 0u;
    g_linux_abi64_clone_last_shared_fd = 0u;
    g_linux_abi64_clone_last_shared_audit = 0u;
    g_linux_abi64_clone_last_child_stack = syscall64_native_user_rsp;
    g_linux_abi64_clone_last_tls_base = context->tls_base;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_FORK,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    if (linux_abi64_restore_return_root(pid) == 0u)
    {
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }
    return (u64)child_pid;
}

static u64 linux_abi64_sys_exec_common(
    u32 pid,
    u64 user_path,
    u64 user_argv,
    u64 user_envp,
    u64 rip,
    u16 syscall_number)
{
    persona_context_t *context;
    linux_abi64_exec_validation_t validation;
    u8 user_path_bytes[LINUX_VFS64_MAX_PATH_BYTES + 1u];
    u8 canonical_path[LINUX_VFS64_MAX_PATH_BYTES];
    u32 path_byte_count = 0u;
    u32 canonical_path_byte_count = 0u;
    u32 path_index;
    u32 path_trailing_slash = 0u;
    u32 binary_bytes = 0u;
    u32 argc = 0u;
    u32 envc = 0u;
    u64 exec_stack_base = LINUX_ABI64_EXEC_STACK_BASE;
    u64 exec_stack_bytes = LINUX_ABI64_EXEC_STACK_BYTES;
    u32 error;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_execve_last_error = 0u;
    g_linux_abi64_execve_last_path_checksum = 0u;
    g_linux_abi64_execve_last_binary_bytes = 0u;
    g_linux_abi64_execve_last_closed_fds = 0u;
    g_linux_abi64_execve_last_fd_live_before = 0u;
    g_linux_abi64_execve_last_fd_live_after = 0u;
    g_linux_abi64_execve_last_vma_before = 0u;
    g_linux_abi64_execve_last_vma_released = 0u;
    g_linux_abi64_execve_last_vma_after = 0u;
    g_linux_abi64_execve_last_argc = 0u;
    g_linux_abi64_execve_last_envc = 0u;
    g_linux_abi64_execve_last_transfer_ready = 0u;
    g_linux_abi64_execve_last_transfer_rip = 0ull;
    g_linux_abi64_execve_last_transfer_rsp = 0ull;
    g_linux_abi64_execve_last_entry_prot = 0u;
    g_linux_abi64_execve_last_stack_prot = 0u;
    g_linux_abi64_execve_transfer_pending = 0u;
    g_linux_abi64_execve_transfer_pid = PROCESS64_INVALID_PID;

    context = persona64_context_for_process(pid);
    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (context == 0)
        || (context->persona_type != PERSONA64_TYPE_LINUX_ELF)
        || (process64_vma_root(pid) == 0)
        || (process64_fd_table(pid) == 0))
    {
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip,
            0u);
    }

    if (linux_abi64_copy_user_path(
            pid,
            user_path,
            user_path_bytes,
            LINUX_VFS64_MAX_PATH_BYTES,
            &path_byte_count) == 0u)
    {
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            LINUX_ABI64_EFAULT,
            rip,
            1u);
    }
    g_linux_abi64_execve_last_path_checksum =
        linux_abi64_checksum_bytes(user_path_bytes, path_byte_count);
    path_trailing_slash = linux_abi64_path_has_trailing_slash(user_path_bytes, path_byte_count);
    if (path_trailing_slash != 0u)
    {
        ++g_linux_abi64_path_trailing_count;
    }

    if (linux_abi64_canonicalize_path(
            pid,
            LINUX_ABI64_AT_FDCWD,
            user_path_bytes,
            path_byte_count,
            canonical_path,
            (u32)sizeof(canonical_path),
            &canonical_path_byte_count) == 0u)
    {
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            LINUX_ABI64_ENOENT,
            rip,
            0u);
    }

    for (path_index = 0u; path_index < canonical_path_byte_count; ++path_index)
    {
        g_linux_abi64_exec_path[path_index] = (char)canonical_path[path_index];
    }
    g_linux_abi64_exec_path[canonical_path_byte_count] = 0;
    path_byte_count = canonical_path_byte_count;

    if (path_trailing_slash != 0u)
    {
        fd64_stat_t path_stat;

        if (linux_vfs64_stat(
                pid,
                (const u8 *)g_linux_abi64_exec_path,
                path_byte_count,
                &path_stat) == 0u)
        {
            ++g_linux_abi64_path_trailing_denial_count;
            return linux_abi64_execve_error_return(
                pid,
                syscall_number,
                LINUX_ABI64_ENOENT,
                rip,
                0u);
        }

        if (path_stat.node_type != FD64_STAT_NODE_DIRECTORY)
        {
            ++g_linux_abi64_path_trailing_denial_count;
            return linux_abi64_execve_error_return(
                pid,
                syscall_number,
                LINUX_ABI64_ENOTDIR,
                rip,
                0u);
        }

        ++g_linux_abi64_path_trailing_denial_count;
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            LINUX_ABI64_EINVAL,
            rip,
            0u);
    }

    error = linux_abi64_copy_user_string_vector(
        pid,
        user_argv,
        g_linux_abi64_exec_argv_storage,
        g_linux_abi64_exec_argv,
        LINUX_ABI64_EXEC_ARG_MAX,
        &argc);
    if (error != 0u)
    {
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            error,
            rip,
            (error == LINUX_ABI64_EFAULT) ? 1u : 0u);
    }

    error = linux_abi64_copy_user_string_vector(
        pid,
        user_envp,
        g_linux_abi64_exec_envp_storage,
        g_linux_abi64_exec_envp,
        LINUX_ABI64_EXEC_ENV_MAX,
        &envc);
    if (error != 0u)
    {
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            error,
            rip,
            (error == LINUX_ABI64_EFAULT) ? 1u : 0u);
    }

    error = linux_abi64_read_exec_binary(
        pid,
        (const u8 *)g_linux_abi64_exec_path,
        path_byte_count,
        g_linux_abi64_exec_binary,
        LINUX_ABI64_EXEC_STAGING_BYTES,
        &binary_bytes);
    if (error != 0u)
    {
        return linux_abi64_execve_error_return(pid, syscall_number, error, rip, 0u);
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (binary_bytes > LINUX_ABI64_EXEC_BINARY_MAX_BYTES)
    {
        exec_stack_base = LINUX_ABI64_REAL_EXEC_STACK_BASE;
        exec_stack_bytes = LINUX_ABI64_REAL_EXEC_STACK_BYTES;
    }
#endif

    error = linux_abi64_validate_static_exec(
        g_linux_abi64_exec_binary,
        binary_bytes,
        &validation);
    if (error != 0u)
    {
        g_linux_abi64_execve_last_error = validation.error;
        return linux_abi64_execve_error_return(pid, syscall_number, error, rip, 0u);
    }

    g_linux_abi64_execve_last_vma_before = vma64_region_count(pid);
    g_linux_abi64_execve_last_fd_live_before = fd64_live_count(pid);
    g_linux_abi64_execve_last_vma_released = vma64_release_process(pid);
    if (vma64_init_process(pid) == 0u)
    {
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            LINUX_ABI64_ENOMEM,
            rip,
            0u);
    }

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        return linux_abi64_execve_error_return(
            pid,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip,
            0u);
    }
    context->vma_root = process64_vma_root(pid);
    context->fd_table = process64_fd_table(pid);
    context->audit_context = process64_audit_ctx(pid);
    context->tls_base = PERSONA64_TLS_UNSET;
    context->tls_size = 0ull;
    context->clear_child_tid = 0ull;
    context->brk_base = PERSONA64_BRK_UNSET;
    context->brk_current = PERSONA64_BRK_UNSET;
    context->heap_cap = PERSONA64_HEAP_CAP_NONE;
    context->load_bias_low = 0u;

    g_linux_abi64_execve_last_closed_fds = fd64_close_on_exec(pid);
    g_linux_abi64_execve_last_fd_live_after = fd64_live_count(pid);
    g_linux_abi64_execve_last_argc = argc;
    g_linux_abi64_execve_last_envc = envc;
    g_linux_abi64_execve_last_binary_bytes = binary_bytes;

    if (elf64_launch_static(
            pid,
            g_linux_abi64_exec_binary,
            binary_bytes,
            0ull,
            exec_stack_base,
            exec_stack_bytes,
            argc,
            g_linux_abi64_exec_argv,
            envc,
            g_linux_abi64_exec_envp,
            0u,
            &g_linux_abi64_exec_launch) != ELF64_OK)
    {
        g_linux_abi64_execve_last_error = g_linux_abi64_exec_launch.error;
        if ((g_linux_abi64_exec_launch.error == ELF64_ERROR_LAUNCH_LOAD)
            && (g_linux_abi64_exec_launch.load_result.error != ELF64_ERROR_NONE))
        {
            g_linux_abi64_execve_last_error = g_linux_abi64_exec_launch.load_result.error;
        }
        ++g_linux_abi64_execve_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_ENOMEM,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOMEM);
    }

    g_linux_abi64_execve_last_vma_after = vma64_region_count(pid);
    g_linux_abi64_execve_last_transfer_ready = g_linux_abi64_exec_launch.transfer_ready;
    g_linux_abi64_execve_last_transfer_rip = g_linux_abi64_exec_launch.transfer_rip;
    g_linux_abi64_execve_last_transfer_rsp = g_linux_abi64_exec_launch.transfer_rsp;
    g_linux_abi64_execve_last_entry_prot = g_linux_abi64_exec_launch.entry_page_prot;
    g_linux_abi64_execve_last_stack_prot = g_linux_abi64_exec_launch.stack_page_prot;
    g_linux_abi64_execve_last_error = g_linux_abi64_exec_launch.error;
    g_linux_abi64_execve_transfer_pending = 1u;
    g_linux_abi64_execve_transfer_pid = pid;

    (void)linux_vfs64_proc_set_identity(
        pid,
        (const u8 *)g_linux_abi64_exec_path,
        path_byte_count,
        (const u8 *)g_linux_abi64_exec_path,
        path_byte_count,
        0,
        0u);

    ++g_linux_abi64_execve_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_execve(u32 pid, u64 user_path, u64 user_argv, u64 user_envp, u64 rip)
{
    return linux_abi64_sys_exec_common(
        pid,
        user_path,
        user_argv,
        user_envp,
        rip,
        LINUX_ABI64_SYSCALL_EXECVE);
}

u64 linux_abi64_sys_execveat(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 user_argv,
    u64 user_envp,
    u64 flags,
    u64 rip)
{
    if ((dirfd != LINUX_ABI64_AT_FDCWD) || (flags != 0ull))
    {
        return linux_abi64_execve_error_return(
            pid,
            LINUX_ABI64_SYSCALL_EXECVEAT,
            LINUX_ABI64_EINVAL,
            rip,
            0u);
    }

    ++g_linux_abi64_execveat_count;
    return linux_abi64_sys_exec_common(
        pid,
        user_path,
        user_argv,
        user_envp,
        rip,
        LINUX_ABI64_SYSCALL_EXECVEAT);
}

static linux_abi64_exit_record_t *linux_abi64_exit_record_for_pid(u32 pid)
{
    u32 index;
    linux_abi64_exit_record_t *free_record = 0;

    for (index = 0u; index < LINUX_ABI64_MAX_EXIT_RECORDS; ++index)
    {
        if (g_linux_abi64_exit_records[index].pid == pid)
        {
            return &g_linux_abi64_exit_records[index];
        }
        if ((free_record == 0) && (g_linux_abi64_exit_records[index].pid == PROCESS64_INVALID_PID))
        {
            free_record = &g_linux_abi64_exit_records[index];
        }
    }

    return free_record;
}

static linux_abi64_exit_record_t *linux_abi64_exit_record_existing(u32 pid)
{
    u32 index;

    for (index = 0u; index < LINUX_ABI64_MAX_EXIT_RECORDS; ++index)
    {
        if (g_linux_abi64_exit_records[index].pid == pid)
        {
            return &g_linux_abi64_exit_records[index];
        }
    }

    return 0;
}

static u32 linux_abi64_wait4_selector_supported(u64 requested_pid)
{
    if (requested_pid == LINUX_ABI64_WAIT_ANY)
    {
        return 1u;
    }

    return ((requested_pid != 0ull) && (requested_pid <= 0x000000007FFFFFFFull)) ? 1u : 0u;
}

static u32 linux_abi64_wait4_status_for_exit(u32 exit_code)
{
    return (exit_code & 0x000000FFu) << 8;
}

static void linux_abi64_wait4_complete_blocked_child(
    u32 child_pid,
    linux_abi64_exit_record_t *exit_record)
{
    linux_abi64_clone_record_t *clone_record;
    u32 status_word;

    clone_record = linux_abi64_clone_record_for_child(child_pid);
    if ((clone_record == 0)
        || (exit_record == 0)
        || (exit_record->exited == 0u)
        || (clone_record->wait_blocked == 0u)
        || (clone_record->wait_task_id == SCHEDULER64_INVALID_TASK))
    {
        return;
    }

    status_word = linux_abi64_wait4_status_for_exit(exit_record->exit_code);
    if (clone_record->wait_status_user != 0ull)
    {
        if (paging64_switch_to_process_root(
                clone_record->parent_pid,
                0x57545354u) != 0u)
        {
            *((volatile u32 *)(u64)clone_record->wait_status_user) = status_word;
            g_linux_abi64_wait4_last_status_written = 1u;
        }
        else
        {
            ++g_linux_abi64_wait4_fault_count;
        }
    }

    g_linux_abi64_wait4_last_parent_pid = clone_record->parent_pid;
    g_linux_abi64_wait4_last_child_pid = clone_record->child_pid;
    g_linux_abi64_wait4_last_exit_code = exit_record->exit_code;
    g_linux_abi64_wait4_last_status = status_word;
    g_linux_abi64_wait4_last_process_release = process64_release_clone(clone_record->child_pid);
    g_linux_abi64_wait4_last_clone_release =
        (g_linux_abi64_wait4_last_process_release != 0u) ? 1u : 0u;
    if (scheduler64_runqueue_wake_task_with_result(
            clone_record->wait_task_id,
            (u64)clone_record->child_pid) == 0u)
    {
        ++g_linux_abi64_wait4_denial_count;
    }
    linux_abi64_clear_clone_record(clone_record);
    linux_abi64_clear_exit_record(exit_record);
    ++g_linux_abi64_wait4_reap_count;
}

static u64 linux_abi64_sys_exit_common(u32 pid, u64 exit_code, u64 rip, u32 syscall_number)
{
    linux_abi64_exit_record_t *record;
    u32 code32 = (u32)(exit_code & 0xFFFFFFFFull);
    u32 fork_child;
    u32 root_token;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_exit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    record = linux_abi64_exit_record_for_pid(pid);
    if (record == 0)
    {
        ++g_linux_abi64_exit_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (syscall_number == LINUX_ABI64_SYSCALL_EXIT_GROUP)
    {
        ++g_linux_abi64_exit_group_count;
    }
    else
    {
        ++g_linux_abi64_exit_count;
    }

    g_linux_abi64_last_exit_audit_recorded = persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        (u16)syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    fork_child = process64_is_fork_child(pid);
    (void)linux_abi64_release_futex_waiters(pid);
    linux_abi64_release_rlimit_record(pid);
    if (process64_is_clone(pid) != 0u)
    {
        g_linux_abi64_last_exit_vma_regions = 0u;
        g_linux_abi64_last_exit_fd_entries = 0u;
        g_linux_abi64_last_exit_persona_released = persona64_release(pid);
        g_linux_abi64_last_exit_audit_released = 0u;
        g_linux_abi64_last_exit_detached_vma = process64_detach_vma(pid);
        g_linux_abi64_last_exit_detached_fd = process64_detach_fd(pid);
        g_linux_abi64_last_exit_detached_audit = process64_detach_audit(pid);
    }
    else
    {
        g_linux_abi64_last_exit_detached_vma = 0;
        g_linux_abi64_last_exit_detached_fd = 0;
        g_linux_abi64_last_exit_detached_audit = 0;
        g_linux_abi64_last_exit_vma_regions = vma64_release_process(pid);
        g_linux_abi64_last_exit_fd_entries = fd64_release_process(pid);
        g_linux_abi64_last_exit_persona_released = persona64_release(pid);
        g_linux_abi64_last_exit_audit_released = persona_audit64_release(pid);
    }
    if (fork_child != 0u)
    {
        (void)linux_vfs64_release_process(pid);
        root_token = process64_page_root_token(pid);
        if ((root_token != 0u)
            && (paging64_process_root_release(pid, root_token) != 0u)
            && (process64_clear_page_root(pid, root_token) != 0u))
        {
            ++g_linux_abi64_child_root_cleanup_count;
        }
    }
    g_linux_abi64_last_exit_pid = pid;
    g_linux_abi64_last_exit_code = code32;

    record->pid = pid;
    record->exit_code = code32;
    record->exited = 1u;
    record->reserved = 0u;
    if (fork_child != 0u)
    {
        (void)process64_mark_child_exited(pid, code32);
        linux_abi64_wait4_complete_blocked_child(pid, record);
    }

    return 0ull;
}

u64 linux_abi64_sys_exit(u32 pid, u64 exit_code, u64 rip)
{
    return linux_abi64_sys_exit_common(pid, exit_code, rip, LINUX_ABI64_SYSCALL_EXIT);
}

u64 linux_abi64_sys_exit_group(u32 pid, u64 exit_code, u64 rip)
{
    return linux_abi64_sys_exit_common(pid, exit_code, rip, LINUX_ABI64_SYSCALL_EXIT_GROUP);
}

u64 linux_abi64_sys_wait4(
    u32 pid,
    u64 wait_pid,
    u64 user_status,
    u64 options,
    u64 user_rusage,
    u64 rip)
{
    linux_abi64_clone_record_t *clone_record;
    linux_abi64_clone_record_t *pending_record;
    linux_abi64_exit_record_t *exit_record;
    u32 matched_children = 0u;
    u32 options32 = (u32)(options & 0xFFFFFFFFull);
    u32 status_word;
    u32 task_id;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_wait4_last_parent_pid = pid;
    g_linux_abi64_wait4_last_child_pid = PROCESS64_INVALID_PID;
    g_linux_abi64_wait4_last_exit_code = 0u;
    g_linux_abi64_wait4_last_status = 0u;
    g_linux_abi64_wait4_last_status_written = 0u;
    g_linux_abi64_wait4_last_options = options32;
    g_linux_abi64_wait4_last_process_release = 0u;
    g_linux_abi64_wait4_last_clone_release = 0u;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF))
    {
        ++g_linux_abi64_wait4_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_WAIT4,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((linux_abi64_wait4_selector_supported(wait_pid) == 0u)
        || ((options & 0xFFFFFFFF00000000ull) != 0ull)
        || ((options32 & ~LINUX_ABI64_WAIT_SUPPORTED_OPTIONS) != 0u))
    {
        ++g_linux_abi64_wait4_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WAIT4,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (user_rusage != 0ull)
    {
        ++g_linux_abi64_wait4_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WAIT4,
            PERSONA_AUDIT64_RESULT_ENOSYS,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOSYS);
    }

    if ((user_status != 0ull)
        && (linux_abi64_user_buffer_writable(pid, user_status, (u32)sizeof(u32)) == 0u))
    {
        ++g_linux_abi64_wait4_denial_count;
        ++g_linux_abi64_wait4_fault_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WAIT4,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    clone_record = linux_abi64_clone_record_for_parent_wait(pid, wait_pid, &matched_children);
    if (clone_record == 0)
    {
        if ((matched_children != 0u) && ((options32 & LINUX_ABI64_WAIT_WNOHANG) != 0u))
        {
            ++g_linux_abi64_wait4_count;
            ++g_linux_abi64_wait4_nohang_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_WAIT4,
                PERSONA_AUDIT64_RESULT_OK,
                rip);
            return 0ull;
        }

        pending_record = linux_abi64_clone_record_for_parent_any(pid, wait_pid);
        if (pending_record != 0)
        {
            task_id = scheduler64_runqueue_current_task_id();
            if ((task_id == SCHEDULER64_INVALID_TASK)
                || (scheduler64_runqueue_task_pid(task_id) != pid)
                || (scheduler64_runqueue_task_state(task_id) != SCHEDULER64_TASK_RUNNING)
                || (pending_record->wait_blocked != 0u)
                || (scheduler64_runqueue_block_task(task_id) == 0u))
            {
                ++g_linux_abi64_wait4_denial_count;
                (void)persona_audit64_record(
                    pid,
                    PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                    LINUX_ABI64_SYSCALL_WAIT4,
                    LINUX_ABI64_EAGAIN,
                    rip);
                return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
            }

            pending_record->wait_blocked = 1u;
            pending_record->wait_task_id = task_id;
            pending_record->wait_status_user = user_status;
            pending_record->wait_rip = rip;
            ++g_linux_abi64_wait4_count;
            (void)persona_audit64_record(
                pid,
                PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
                LINUX_ABI64_SYSCALL_WAIT4,
                PERSONA_AUDIT64_RESULT_OK,
                rip);
            return 0ull;
        }

        ++g_linux_abi64_wait4_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WAIT4,
            (matched_children != 0u) ? LINUX_ABI64_EAGAIN : LINUX_ABI64_ECHILD,
            rip);
        return (matched_children != 0u)
            ? LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN)
            : LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ECHILD);
    }

    exit_record = linux_abi64_exit_record_existing(clone_record->child_pid);
    if ((exit_record == 0) || (exit_record->exited == 0u))
    {
        ++g_linux_abi64_wait4_denial_count;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_WAIT4,
            LINUX_ABI64_EAGAIN,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EAGAIN);
    }

    status_word = linux_abi64_wait4_status_for_exit(exit_record->exit_code);
    if (user_status != 0ull)
    {
        *((volatile u32 *)(u64)user_status) = status_word;
        g_linux_abi64_wait4_last_status_written = 1u;
    }

    g_linux_abi64_wait4_last_child_pid = clone_record->child_pid;
    g_linux_abi64_wait4_last_exit_code = exit_record->exit_code;
    g_linux_abi64_wait4_last_status = status_word;
    g_linux_abi64_wait4_last_process_release = process64_release_clone(clone_record->child_pid);
    g_linux_abi64_wait4_last_clone_release =
        (g_linux_abi64_wait4_last_process_release != 0u) ? 1u : 0u;
    linux_abi64_clear_clone_record(clone_record);
    linux_abi64_clear_exit_record(exit_record);

    ++g_linux_abi64_wait4_count;
    ++g_linux_abi64_wait4_reap_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_WAIT4,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)g_linux_abi64_wait4_last_child_pid;
}

static u32 linux_abi64_signal_persona_ready(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_LINUX_ELF))
        ? 1u
        : 0u;
}

static u64 linux_abi64_signal_mask_for(u32 signal_number)
{
    if ((signal_number == 0u) || (signal_number > LINUX_SIGNAL64_MAX_SIGNALS))
    {
        return 0ull;
    }

    return 1ull << (signal_number - 1u);
}

static u32 linux_abi64_signal_first_unmasked(u64 pending, u64 mask)
{
    u32 signal_number;
    u64 deliverable = pending & ~mask;

    for (signal_number = 1u;
        signal_number <= LINUX_SIGNAL64_MAX_SIGNALS;
        ++signal_number)
    {
        if ((deliverable & linux_abi64_signal_mask_for(signal_number)) != 0ull)
        {
            return signal_number;
        }
    }

    return 0u;
}

u32 linux_abi64_signal_deliver_pending(u32 pid, struct interrupt_frame64 *frame)
{
    persona_context_t *context;
    linux_signal64_sigaction_t action;
    linux_signal64_delivery_frame_t signal_frame;
    u32 signal_number;
    u64 signal_mask;
    u64 old_mask;
    u64 frame_address;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_signal_delivery_last_signal = 0u;
    g_linux_abi64_signal_delivery_last_handler = 0ull;
    g_linux_abi64_signal_delivery_last_frame = 0ull;
    g_linux_abi64_signal_delivery_last_saved_rip = 0ull;
    g_linux_abi64_signal_delivery_last_saved_rsp = 0ull;
    g_linux_abi64_signal_delivery_last_mask = 0ull;
    g_linux_abi64_signal_delivery_last_result = 0u;

    if ((frame == 0) || (linux_abi64_signal_persona_ready(pid) == 0u))
    {
        ++g_linux_abi64_signal_delivery_denial_count;
        g_linux_abi64_signal_delivery_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_KILL,
            LINUX_ABI64_ESRCH,
            (frame != 0) ? frame->rip : 0ull);
        return 0u;
    }

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        ++g_linux_abi64_signal_delivery_denial_count;
        g_linux_abi64_signal_delivery_last_result = LINUX_ABI64_ESRCH;
        return 0u;
    }

    old_mask = context->linux_signal_mask & ~LINUX_SIGNAL64_UNBLOCKABLE_MASK;
    context->linux_signal_mask = old_mask;
    signal_number = linux_abi64_signal_first_unmasked(context->linux_signal_pending, old_mask);
    if (signal_number == 0u)
    {
        if (context->linux_signal_pending != LINUX_SIGNAL64_PENDING_NONE)
        {
            ++g_linux_abi64_signal_masked_count;
        }
        return 0u;
    }

    signal_mask = linux_abi64_signal_mask_for(signal_number);
    action = context->linux_sigactions[signal_number - 1u];
    if (action.handler == LINUX_SIGNAL64_DEFAULT_HANDLER)
    {
        ++g_linux_abi64_signal_masked_count;
        g_linux_abi64_signal_delivery_last_signal = signal_number;
        g_linux_abi64_signal_delivery_last_result = LINUX_ABI64_EAGAIN;
        return 0u;
    }

    if ((frame->cs & 0x3ull) != 0x3ull)
    {
        ++g_linux_abi64_signal_delivery_denial_count;
        g_linux_abi64_signal_delivery_last_signal = signal_number;
        g_linux_abi64_signal_delivery_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            signal_number,
            LINUX_ABI64_EINVAL,
            frame->rip);
        return 0u;
    }

    if (frame->rsp < (u64)(sizeof(linux_signal64_delivery_frame_t)
            + LINUX_SIGNAL64_DELIVERY_ALIGN))
    {
        ++g_linux_abi64_signal_delivery_fault_count;
        g_linux_abi64_signal_delivery_last_signal = signal_number;
        g_linux_abi64_signal_delivery_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            signal_number,
            LINUX_ABI64_EFAULT,
            frame->rip);
        return 0u;
    }

    frame_address =
        (frame->rsp - (u64)sizeof(linux_signal64_delivery_frame_t))
            & ~(u64)(LINUX_SIGNAL64_DELIVERY_ALIGN - 1u);
    if (linux_abi64_user_buffer_writable(
            pid,
            frame_address,
            (u32)sizeof(linux_signal64_delivery_frame_t)) == 0u)
    {
        ++g_linux_abi64_signal_delivery_fault_count;
        g_linux_abi64_signal_delivery_last_signal = signal_number;
        g_linux_abi64_signal_delivery_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            signal_number,
            LINUX_ABI64_EFAULT,
            frame->rip);
        return 0u;
    }

    signal_frame.restorer = 0ull;
    signal_frame.siginfo.signo = signal_number;
    signal_frame.siginfo.errno_value = 0u;
    signal_frame.siginfo.code = 0u;
    signal_frame.siginfo.sender_pid = pid;
    signal_frame.siginfo.sender_uid = process64_principal(pid);
    signal_frame.siginfo.reserved0 = 0u;
    for (u32 index = 0u; index < 13u; ++index)
    {
        signal_frame.siginfo.reserved[index] = 0ull;
    }
    signal_frame.ucontext.flags = 0ull;
    signal_frame.ucontext.link = 0ull;
    signal_frame.ucontext.stack_sp = frame->rsp;
    signal_frame.ucontext.stack_flags = 0ull;
    signal_frame.ucontext.stack_size = 0ull;
    signal_frame.ucontext.signal_mask = old_mask;
    signal_frame.ucontext.rip = frame->rip;
    signal_frame.ucontext.rsp = frame->rsp;
    signal_frame.ucontext.rflags = frame->rflags;
    signal_frame.ucontext.cs = frame->cs;
    signal_frame.ucontext.ss = frame->ss;
    signal_frame.ucontext.rax = frame->rax;
    signal_frame.ucontext.rbx = frame->rbx;
    signal_frame.ucontext.rcx = frame->rcx;
    signal_frame.ucontext.rdx = frame->rdx;
    signal_frame.ucontext.rbp = frame->rbp;
    signal_frame.ucontext.rsi = frame->rsi;
    signal_frame.ucontext.rdi = frame->rdi;
    signal_frame.ucontext.r8 = frame->r8;
    signal_frame.ucontext.r9 = frame->r9;
    signal_frame.ucontext.r10 = frame->r10;
    signal_frame.ucontext.r11 = frame->r11;
    signal_frame.ucontext.r12 = frame->r12;
    signal_frame.ucontext.r13 = frame->r13;
    signal_frame.ucontext.r14 = frame->r14;
    signal_frame.ucontext.r15 = frame->r15;
    linux_abi64_copy_to_user(
        frame_address,
        (const u8 *)&signal_frame,
        (u32)sizeof(signal_frame));

    context->linux_signal_pending &= ~signal_mask;
    context->linux_signal_mask =
        (old_mask | signal_mask | action.sa_mask) & ~LINUX_SIGNAL64_UNBLOCKABLE_MASK;
    frame->rdi = signal_number;
    frame->rsi = frame_address + (u64)sizeof(u64);
    frame->rdx = frame->rsi + (u64)sizeof(linux_signal64_siginfo_t);
    frame->rip = action.handler;
    frame->rsp = frame_address;

    ++g_linux_abi64_signal_delivery_count;
    g_linux_abi64_signal_delivery_last_signal = signal_number;
    g_linux_abi64_signal_delivery_last_handler = action.handler;
    g_linux_abi64_signal_delivery_last_frame = frame_address;
    g_linux_abi64_signal_delivery_last_saved_rip = signal_frame.ucontext.rip;
    g_linux_abi64_signal_delivery_last_saved_rsp = signal_frame.ucontext.rsp;
    g_linux_abi64_signal_delivery_last_mask = old_mask;
    g_linux_abi64_signal_delivery_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        signal_number,
        PERSONA_AUDIT64_RESULT_OK,
        signal_frame.ucontext.rip);
    return 1u;
}

static u64 linux_abi64_rt_sigreturn_error(
    u32 pid,
    u32 error_code,
    u64 rip,
    u32 fault)
{
    ++g_linux_abi64_rt_sigreturn_denial_count;
    if (fault != 0u)
    {
        ++g_linux_abi64_rt_sigreturn_fault_count;
    }
    g_linux_abi64_rt_sigreturn_last_result = error_code;
    (void)persona_audit64_record(
        pid,
        (error_code == LINUX_ABI64_ESRCH)
            ? PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED
            : PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_RT_SIGRETURN,
        error_code,
        rip);
    return LINUX_ABI64_ERROR_RETURN(error_code);
}

static u32 linux_abi64_rt_sigreturn_context_valid(
    const linux_signal64_ucontext_t *ucontext)
{
    if (ucontext == 0)
    {
        return 0u;
    }

    if (((ucontext->cs & 0x3ull) != 0x3ull)
        || ((ucontext->ss & 0x3ull) != 0x3ull))
    {
        return 0u;
    }

    if ((ucontext->rip == 0ull)
        || (ucontext->rip >= LINUX_ABI64_USER_CANONICAL_LIMIT)
        || (ucontext->rsp == 0ull)
        || (ucontext->rsp >= LINUX_ABI64_USER_CANONICAL_LIMIT)
        || ((ucontext->rflags & LINUX_SIGNAL64_RFLAGS_FORBIDDEN_MASK) != 0ull))
    {
        return 0u;
    }

    return 1u;
}

u64 linux_abi64_sys_rt_sigreturn(u32 pid, struct interrupt_frame64 *frame)
{
    persona_context_t *context;
    linux_signal64_delivery_frame_t signal_frame;
    u64 frame_address;
    u64 syscall_rip;
    u64 restored_rflags;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_rt_sigreturn_last_frame = (frame != 0) ? frame->rsp : 0ull;
    g_linux_abi64_rt_sigreturn_last_rip = 0ull;
    g_linux_abi64_rt_sigreturn_last_rsp = 0ull;
    g_linux_abi64_rt_sigreturn_last_mask = 0ull;
    g_linux_abi64_rt_sigreturn_last_rax = 0ull;
    g_linux_abi64_rt_sigreturn_last_result = 0u;

    if ((frame == 0) || (linux_abi64_signal_persona_ready(pid) == 0u))
    {
        return linux_abi64_rt_sigreturn_error(
            pid,
            LINUX_ABI64_ESRCH,
            (frame != 0) ? frame->rip : 0ull,
            0u);
    }

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        return linux_abi64_rt_sigreturn_error(pid, LINUX_ABI64_ESRCH, frame->rip, 0u);
    }

    syscall_rip = frame->rip;
    if ((frame->cs & 0x3ull) != 0x3ull)
    {
        return linux_abi64_rt_sigreturn_error(pid, LINUX_ABI64_EINVAL, syscall_rip, 0u);
    }

    frame_address = frame->rsp;
    g_linux_abi64_rt_sigreturn_last_frame = frame_address;
    if (linux_abi64_user_buffer_readable(
            pid,
            frame_address,
            (u32)sizeof(linux_signal64_delivery_frame_t)) == 0u)
    {
        return linux_abi64_rt_sigreturn_error(pid, LINUX_ABI64_EFAULT, syscall_rip, 1u);
    }

    linux_abi64_copy_from_user(
        (u8 *)&signal_frame,
        frame_address,
        (u32)sizeof(signal_frame));
    if (linux_abi64_rt_sigreturn_context_valid(&signal_frame.ucontext) == 0u)
    {
        return linux_abi64_rt_sigreturn_error(pid, LINUX_ABI64_EINVAL, syscall_rip, 0u);
    }

    context->linux_signal_mask =
        signal_frame.ucontext.signal_mask & ~LINUX_SIGNAL64_UNBLOCKABLE_MASK;
    restored_rflags =
        (signal_frame.ucontext.rflags | LINUX_SIGNAL64_RFLAGS_FIXED_BIT)
            & ~LINUX_SIGNAL64_RFLAGS_FORBIDDEN_MASK;

    frame->r15 = signal_frame.ucontext.r15;
    frame->r14 = signal_frame.ucontext.r14;
    frame->r13 = signal_frame.ucontext.r13;
    frame->r12 = signal_frame.ucontext.r12;
    frame->r11 = signal_frame.ucontext.r11;
    frame->r10 = signal_frame.ucontext.r10;
    frame->r9 = signal_frame.ucontext.r9;
    frame->r8 = signal_frame.ucontext.r8;
    frame->rdi = signal_frame.ucontext.rdi;
    frame->rsi = signal_frame.ucontext.rsi;
    frame->rbp = signal_frame.ucontext.rbp;
    frame->rbx = signal_frame.ucontext.rbx;
    frame->rdx = signal_frame.ucontext.rdx;
    frame->rcx = signal_frame.ucontext.rcx;
    frame->rax = signal_frame.ucontext.rax;
    frame->rip = signal_frame.ucontext.rip;
    frame->cs = signal_frame.ucontext.cs;
    frame->rflags = restored_rflags;
    frame->rsp = signal_frame.ucontext.rsp;
    frame->ss = signal_frame.ucontext.ss;

    ++g_linux_abi64_rt_sigreturn_count;
    g_linux_abi64_rt_sigreturn_last_rip = frame->rip;
    g_linux_abi64_rt_sigreturn_last_rsp = frame->rsp;
    g_linux_abi64_rt_sigreturn_last_mask = context->linux_signal_mask;
    g_linux_abi64_rt_sigreturn_last_rax = frame->rax;
    g_linux_abi64_rt_sigreturn_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_RT_SIGRETURN,
        PERSONA_AUDIT64_RESULT_OK,
        syscall_rip);
    return frame->rax;
}

static u64 linux_abi64_signal_translate(
    u32 pid,
    u16 syscall_number,
    u64 target,
    u64 signal_number,
    u64 rip)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_kill_last_syscall = syscall_number;
    g_linux_abi64_kill_last_target =
        (target <= 0x00000000FFFFFFFFull) ? (u32)target : PROCESS64_INVALID_PID;
    g_linux_abi64_kill_last_signal =
        (signal_number <= 0x00000000FFFFFFFFull) ? (u32)signal_number : 0xFFFFFFFFu;
    g_linux_abi64_kill_last_result = 0u;

    persona_context_t *target_context;
    u32 target_pid;
    u32 isolation_result;
    u64 signal_mask;

    if (linux_abi64_signal_persona_ready(pid) == 0u)
    {
        ++g_linux_abi64_kill_denial_count;
        g_linux_abi64_kill_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (signal_number > (u64)LINUX_ABI64_SIGNAL_MAX)
    {
        ++g_linux_abi64_kill_denial_count;
        g_linux_abi64_kill_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            syscall_number,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    if (target == 0ull)
    {
        target_pid = pid;
    }
    else
    {
        target_pid = (target <= 0x00000000FFFFFFFFull) ? (u32)target : PROCESS64_INVALID_PID;
    }

    target_context = persona64_context_for_process(target_pid);
    if ((target_pid == PROCESS64_INVALID_PID)
        || (process64_principal(target_pid) == 0u)
        || (target_context == 0))
    {
        ++g_linux_abi64_kill_denial_count;
        g_linux_abi64_kill_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    isolation_result = PERSONA64_ISOLATION_RESULT_OK;
    if (persona64_can_signal(pid, target_pid, &isolation_result) == 0u)
    {
        (void)isolation_result;
        ++g_linux_abi64_kill_denial_count;
        g_linux_abi64_kill_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (target_context->persona_type != PERSONA64_TYPE_LINUX_ELF)
    {
        ++g_linux_abi64_kill_denial_count;
        g_linux_abi64_kill_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            syscall_number,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if (syscall_number == LINUX_ABI64_SYSCALL_TKILL)
    {
        ++g_linux_abi64_tkill_count;
    }
    else
    {
        ++g_linux_abi64_kill_count;
    }

    signal_mask = linux_abi64_signal_mask_for((u32)signal_number);
    if (signal_mask != 0ull)
    {
        target_context->linux_signal_pending |= signal_mask;
        ++g_linux_abi64_signal_pending_count;
    }

    g_linux_abi64_kill_last_target = target_pid;
    g_linux_abi64_kill_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        syscall_number,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return 0ull;
}

u64 linux_abi64_sys_kill(u32 pid, u64 target_pid, u64 signal_number, u64 rip)
{
    return linux_abi64_signal_translate(
        pid,
        LINUX_ABI64_SYSCALL_KILL,
        target_pid,
        signal_number,
        rip);
}

u64 linux_abi64_sys_tkill(u32 pid, u64 target_tid, u64 signal_number, u64 rip)
{
    return linux_abi64_signal_translate(
        pid,
        LINUX_ABI64_SYSCALL_TKILL,
        target_tid,
        signal_number,
        rip);
}

static u64 linux_abi64_entropy_mix64(u64 state, u64 value)
{
    state ^= value + 0x9E3779B97F4A7C15ull + (state << 6) + (state >> 2);
    state ^= state >> 30;
    state *= 0xBF58476D1CE4E5B9ull;
    state ^= state >> 27;
    state *= 0x94D049BB133111EBull;
    state ^= state >> 31;
    return state;
}

static u64 linux_abi64_entropy_next64(u64 *state)
{
    u64 value;

    if ((state == 0) || (*state == 0ull))
    {
        return 0xA5A5A5A55A5A5A5Aull;
    }

    value = *state;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    *state = value;
    return value * 0x2545F4914F6CDD1Dull;
}

static u64 linux_abi64_getrandom_seed(u32 pid, u64 user_buffer, u32 byte_count, u32 flags, u64 rip)
{
    u64 seed = g_linux_abi64_getrandom_state;

    seed = linux_abi64_entropy_mix64(seed, (u64)pid);
    seed = linux_abi64_entropy_mix64(seed, (u64)process64_principal(pid));
    seed = linux_abi64_entropy_mix64(seed, (u64)process64_manifest_token(pid));
    seed = linux_abi64_entropy_mix64(seed, (u64)process64_runtime_token(pid));
    seed = linux_abi64_entropy_mix64(seed, (u64)process64_runtime_image_token(pid));
    seed = linux_abi64_entropy_mix64(seed, user_buffer);
    seed = linux_abi64_entropy_mix64(seed, (u64)byte_count);
    seed = linux_abi64_entropy_mix64(seed, (u64)flags);
    seed = linux_abi64_entropy_mix64(seed, rip);
    seed = linux_abi64_entropy_mix64(seed, (u64)pit_get_ticks());
    seed = linux_abi64_entropy_mix64(seed, (u64)pit_get_frequency_hz());
    return (seed != 0ull) ? seed : 0x9E3779B97F4A7C15ull;
}

static u32 linux_abi64_getrandom_persona_ready(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_LINUX_ELF))
        ? 1u
        : 0u;
}

u64 linux_abi64_sys_getrandom(u32 pid, u64 user_buffer, u64 byte_count, u64 flags, u64 rip)
{
    volatile u8 *target = (volatile u8 *)(u64)user_buffer;
    u64 state;
    u64 word = 0ull;
    u32 actual_count;
    u32 index;
    u32 checksum = 2166136261u;
    u32 flags32 = (flags <= 0x00000000FFFFFFFFull) ? (u32)flags : 0xFFFFFFFFu;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    g_linux_abi64_getrandom_last_byte_count = 0u;
    g_linux_abi64_getrandom_last_checksum = 0u;
    g_linux_abi64_getrandom_last_flags = flags32;
    g_linux_abi64_getrandom_last_result = 0u;

    if (linux_abi64_getrandom_persona_ready(pid) == 0u)
    {
        ++g_linux_abi64_getrandom_denial_count;
        g_linux_abi64_getrandom_last_result = LINUX_ABI64_ESRCH;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            LINUX_ABI64_SYSCALL_GETRANDOM,
            LINUX_ABI64_ESRCH,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }

    if ((flags > 0x00000000FFFFFFFFull)
        || ((flags32 & ~LINUX_ABI64_GETRANDOM_SUPPORTED_FLAGS) != 0u))
    {
        ++g_linux_abi64_getrandom_denial_count;
        g_linux_abi64_getrandom_last_result = LINUX_ABI64_EINVAL;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETRANDOM,
            LINUX_ABI64_EINVAL,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }

    actual_count = (byte_count > (u64)LINUX_ABI64_GETRANDOM_MAX_BYTES)
        ? LINUX_ABI64_GETRANDOM_MAX_BYTES
        : (u32)byte_count;
    if (actual_count == 0u)
    {
        ++g_linux_abi64_getrandom_count;
        g_linux_abi64_getrandom_last_result = PERSONA_AUDIT64_RESULT_OK;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETRANDOM,
            PERSONA_AUDIT64_RESULT_OK,
            rip);
        return 0ull;
    }

    if (linux_abi64_user_buffer_writable(pid, user_buffer, actual_count) == 0u)
    {
        ++g_linux_abi64_getrandom_denial_count;
        ++g_linux_abi64_getrandom_fault_count;
        g_linux_abi64_getrandom_last_result = LINUX_ABI64_EFAULT;
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
            LINUX_ABI64_SYSCALL_GETRANDOM,
            LINUX_ABI64_EFAULT,
            rip);
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    state = linux_abi64_getrandom_seed(pid, user_buffer, actual_count, flags32, rip);
    for (index = 0u; index < actual_count; ++index)
    {
        u8 byte;

        if ((index & 7u) == 0u)
        {
            word = linux_abi64_entropy_next64(&state);
        }
        byte = (u8)(word >> ((index & 7u) * 8u));
        if (byte == 0u)
        {
            byte = (u8)(0xA5u ^ (u8)index);
        }
        target[index] = byte;
        checksum ^= (u32)byte;
        checksum *= 16777619u;
    }

    g_linux_abi64_getrandom_state =
        linux_abi64_entropy_mix64(state, ((u64)checksum << 32) ^ (u64)actual_count);
    ++g_linux_abi64_getrandom_count;
    g_linux_abi64_getrandom_byte_count += actual_count;
    g_linux_abi64_getrandom_last_byte_count = actual_count;
    g_linux_abi64_getrandom_last_checksum = checksum;
    g_linux_abi64_getrandom_last_result = PERSONA_AUDIT64_RESULT_OK;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED,
        LINUX_ABI64_SYSCALL_GETRANDOM,
        PERSONA_AUDIT64_RESULT_OK,
        rip);
    return (u64)actual_count;
}

u32 linux_abi64_table_size(void)
{
    return LINUX_ABI64_SYSCALL_LIMIT;
}

u32 linux_abi64_unimplemented_entry_count(void)
{
    u32 index;
    u32 count = 0u;

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    for (index = 0u; index < LINUX_ABI64_SYSCALL_LIMIT; ++index)
    {
        if (g_linux_abi64_dispatch_table[index] == linux_abi64_unimplemented_stub)
        {
            ++count;
        }
    }

    return count;
}

u32 linux_abi64_read_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_READ]
        == linux_abi64_read_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_write_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_WRITE]
        == linux_abi64_write_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_pread64_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PREAD64]
        == linux_abi64_pread64_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_pwrite64_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PWRITE64]
        == linux_abi64_pwrite64_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_readv_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_READV]
        == linux_abi64_readv_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_writev_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_WRITEV]
        == linux_abi64_writev_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_poll_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_POLL]
        == linux_abi64_poll_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_ppoll_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PPOLL]
        == linux_abi64_ppoll_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_open_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_OPEN]
        == linux_abi64_open_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_close_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CLOSE]
        == linux_abi64_close_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_lseek_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_LSEEK]
        == linux_abi64_lseek_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_stat_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_STAT]
        == linux_abi64_stat_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_fstat_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FSTAT]
        == linux_abi64_fstat_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_newfstatat_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_NEWFSTATAT]
        == linux_abi64_newfstatat_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_mmap_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_MMAP]
        == linux_abi64_mmap_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_mprotect_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_MPROTECT]
        == linux_abi64_mprotect_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_munmap_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_MUNMAP]
        == linux_abi64_munmap_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_brk_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_BRK]
        == linux_abi64_brk_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_rt_sigaction_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_RT_SIGACTION]
        == linux_abi64_rt_sigaction_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_rt_sigprocmask_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_RT_SIGPROCMASK]
        == linux_abi64_rt_sigprocmask_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_rt_sigreturn_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_RT_SIGRETURN]
        == linux_abi64_rt_sigreturn_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_nanosleep_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_NANOSLEEP]
        == linux_abi64_nanosleep_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_getrlimit_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETRLIMIT]
        == linux_abi64_getrlimit_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_setrlimit_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_SETRLIMIT]
        == linux_abi64_setrlimit_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_pipe2_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PIPE2]
        == linux_abi64_pipe2_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_pipe_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_PIPE]
        == linux_abi64_pipe_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_dup_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_DUP]
        == linux_abi64_dup_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_dup2_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_DUP2]
        == linux_abi64_dup2_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_dup3_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_DUP3]
        == linux_abi64_dup3_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_fcntl_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FCNTL]
        == linux_abi64_fcntl_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_getcwd_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETCWD]
        == linux_abi64_getcwd_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_chdir_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CHDIR]
        == linux_abi64_chdir_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_fchdir_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FCHDIR]
        == linux_abi64_fchdir_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_getdents64_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETDENTS64]
        == linux_abi64_getdents64_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_futex_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_FUTEX]
        == linux_abi64_futex_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_clone_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CLONE]
        == linux_abi64_clone_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_execve_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXECVE]
        == linux_abi64_execve_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_execveat_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXECVEAT]
        == linux_abi64_execveat_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_wait4_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_WAIT4]
        == linux_abi64_wait4_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_kill_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_KILL]
        == linux_abi64_kill_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_tkill_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_TKILL]
        == linux_abi64_tkill_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_getrandom_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETRANDOM]
        == linux_abi64_getrandom_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_getpid_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETPID]
        == linux_abi64_getpid_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_gettid_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_GETTID]
        == linux_abi64_gettid_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_arch_prctl_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_ARCH_PRCTL]
        == linux_abi64_arch_prctl_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_set_tid_address_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_SET_TID_ADDRESS]
        == linux_abi64_set_tid_address_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_clock_gettime_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_CLOCK_GETTIME]
        == linux_abi64_clock_gettime_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_exit_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXIT]
        == linux_abi64_exit_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_exit_group_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_EXIT_GROUP]
        == linux_abi64_exit_group_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_openat_entry_installed(void)
{
    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
    }

    return (g_linux_abi64_dispatch_table[LINUX_ABI64_SYSCALL_OPENAT]
        == linux_abi64_openat_dispatch)
        ? 1u
        : 0u;
}

u32 linux_abi64_dispatch_count(void)
{
    return g_linux_abi64_dispatch_count;
}

u32 linux_abi64_dispatch_root_repair_count(void)
{
    return g_linux_abi64_dispatch_root_repair_count;
}

u32 linux_abi64_dispatch_root_reload_count(void)
{
    return g_linux_abi64_dispatch_root_reload_count;
}

u32 linux_abi64_dispatch_root_denial_count(void)
{
    return g_linux_abi64_dispatch_root_denial_count;
}

u32 linux_abi64_unimplemented_count(void)
{
    return g_linux_abi64_unimplemented_count;
}

u32 linux_abi64_unimplemented_last_syscall(void)
{
    return g_linux_abi64_unimplemented_last_syscall;
}

u64 linux_abi64_unimplemented_last_rip(void)
{
    return g_linux_abi64_unimplemented_last_rip;
}

u32 linux_abi64_read_count(void)
{
    return g_linux_abi64_read_count;
}

u32 linux_abi64_read_byte_count(void)
{
    return g_linux_abi64_read_byte_count;
}

u32 linux_abi64_read_denial_count(void)
{
    return g_linux_abi64_read_denial_count;
}

u32 linux_abi64_read_fault_count(void)
{
    return g_linux_abi64_read_fault_count;
}

u32 linux_abi64_write_count(void)
{
    return g_linux_abi64_write_count;
}

u32 linux_abi64_write_byte_count(void)
{
    return g_linux_abi64_write_byte_count;
}

u32 linux_abi64_write_denial_count(void)
{
    return g_linux_abi64_write_denial_count;
}

u32 linux_abi64_write_fault_count(void)
{
    return g_linux_abi64_write_fault_count;
}

u32 linux_abi64_pread64_count(void)
{
    return g_linux_abi64_pread64_count;
}

u32 linux_abi64_pwrite64_count(void)
{
    return g_linux_abi64_pwrite64_count;
}

u32 linux_abi64_pread64_byte_count(void)
{
    return g_linux_abi64_pread64_byte_count;
}

u32 linux_abi64_pwrite64_byte_count(void)
{
    return g_linux_abi64_pwrite64_byte_count;
}

u32 linux_abi64_positioned_denial_count(void)
{
    return g_linux_abi64_positioned_denial_count;
}

u32 linux_abi64_positioned_fault_count(void)
{
    return g_linux_abi64_positioned_fault_count;
}

u32 linux_abi64_positioned_last_syscall(void)
{
    return g_linux_abi64_positioned_last_syscall;
}

u32 linux_abi64_positioned_last_fd(void)
{
    return g_linux_abi64_positioned_last_fd;
}

u32 linux_abi64_positioned_last_byte_count(void)
{
    return g_linux_abi64_positioned_last_byte_count;
}

u64 linux_abi64_positioned_last_offset(void)
{
    return g_linux_abi64_positioned_last_offset;
}

u32 linux_abi64_positioned_last_result(void)
{
    return g_linux_abi64_positioned_last_result;
}

u32 linux_abi64_readv_count(void)
{
    return g_linux_abi64_readv_count;
}

u32 linux_abi64_writev_count(void)
{
    return g_linux_abi64_writev_count;
}

u32 linux_abi64_readv_byte_count(void)
{
    return g_linux_abi64_readv_byte_count;
}

u32 linux_abi64_writev_byte_count(void)
{
    return g_linux_abi64_writev_byte_count;
}

u32 linux_abi64_vector_denial_count(void)
{
    return g_linux_abi64_vector_denial_count;
}

u32 linux_abi64_vector_fault_count(void)
{
    return g_linux_abi64_vector_fault_count;
}

u32 linux_abi64_vector_last_syscall(void)
{
    return g_linux_abi64_vector_last_syscall;
}

u32 linux_abi64_vector_last_fd(void)
{
    return g_linux_abi64_vector_last_fd;
}

u32 linux_abi64_vector_last_iov_count(void)
{
    return g_linux_abi64_vector_last_iov_count;
}

u32 linux_abi64_vector_last_byte_count(void)
{
    return g_linux_abi64_vector_last_byte_count;
}

u32 linux_abi64_vector_last_result(void)
{
    return g_linux_abi64_vector_last_result;
}

u32 linux_abi64_poll_count(void)
{
    return g_linux_abi64_poll_count;
}

u32 linux_abi64_ppoll_count(void)
{
    return g_linux_abi64_ppoll_count;
}

u32 linux_abi64_poll_ready_count(void)
{
    return g_linux_abi64_poll_ready_count;
}

u32 linux_abi64_poll_denial_count(void)
{
    return g_linux_abi64_poll_denial_count;
}

u32 linux_abi64_poll_fault_count(void)
{
    return g_linux_abi64_poll_fault_count;
}

u32 linux_abi64_poll_last_syscall(void)
{
    return g_linux_abi64_poll_last_syscall;
}

u32 linux_abi64_poll_last_fd_count(void)
{
    return g_linux_abi64_poll_last_fd_count;
}

u32 linux_abi64_poll_last_ready(void)
{
    return g_linux_abi64_poll_last_ready;
}

u32 linux_abi64_poll_last_revents(void)
{
    return g_linux_abi64_poll_last_revents;
}

u32 linux_abi64_poll_last_result(void)
{
    return g_linux_abi64_poll_last_result;
}

u32 linux_abi64_open_count(void)
{
    return g_linux_abi64_open_count;
}

u32 linux_abi64_openat_count(void)
{
    return g_linux_abi64_openat_count;
}

u32 linux_abi64_open_denial_count(void)
{
    return g_linux_abi64_open_denial_count;
}

u32 linux_abi64_close_count(void)
{
    return g_linux_abi64_close_count;
}

u32 linux_abi64_close_denial_count(void)
{
    return g_linux_abi64_close_denial_count;
}

u32 linux_abi64_lseek_count(void)
{
    return g_linux_abi64_lseek_count;
}

u32 linux_abi64_lseek_denial_count(void)
{
    return g_linux_abi64_lseek_denial_count;
}

u32 linux_abi64_stat_count(void)
{
    return g_linux_abi64_stat_count;
}

u32 linux_abi64_stat_denial_count(void)
{
    return g_linux_abi64_stat_denial_count;
}

u32 linux_abi64_stat_fault_count(void)
{
    return g_linux_abi64_stat_fault_count;
}

u32 linux_abi64_fstat_count(void)
{
    return g_linux_abi64_fstat_count;
}

u32 linux_abi64_fstat_denial_count(void)
{
    return g_linux_abi64_fstat_denial_count;
}

u32 linux_abi64_fstat_fault_count(void)
{
    return g_linux_abi64_fstat_fault_count;
}

u32 linux_abi64_newfstatat_count(void)
{
    return g_linux_abi64_newfstatat_count;
}

u32 linux_abi64_newfstatat_denial_count(void)
{
    return g_linux_abi64_newfstatat_denial_count;
}

u32 linux_abi64_newfstatat_fault_count(void)
{
    return g_linux_abi64_newfstatat_fault_count;
}

u32 linux_abi64_readlink_count(void)
{
    return g_linux_abi64_readlink_count;
}

u32 linux_abi64_readlink_byte_count(void)
{
    return g_linux_abi64_readlink_byte_count;
}

u32 linux_abi64_readlink_denial_count(void)
{
    return g_linux_abi64_readlink_denial_count;
}

u32 linux_abi64_readlink_fault_count(void)
{
    return g_linux_abi64_readlink_fault_count;
}

u32 linux_abi64_readlink_last_result(void)
{
    return g_linux_abi64_readlink_last_result;
}

u32 linux_abi64_mmap_count(void)
{
    return g_linux_abi64_mmap_count;
}

u32 linux_abi64_mmap_byte_count(void)
{
    return g_linux_abi64_mmap_byte_count;
}

u32 linux_abi64_mmap_denial_count(void)
{
    return g_linux_abi64_mmap_denial_count;
}

u32 linux_abi64_mprotect_count(void)
{
    return g_linux_abi64_mprotect_count;
}

u32 linux_abi64_mprotect_byte_count(void)
{
    return g_linux_abi64_mprotect_byte_count;
}

u32 linux_abi64_mprotect_denial_count(void)
{
    return g_linux_abi64_mprotect_denial_count;
}

u32 linux_abi64_munmap_count(void)
{
    return g_linux_abi64_munmap_count;
}

u32 linux_abi64_munmap_byte_count(void)
{
    return g_linux_abi64_munmap_byte_count;
}

u32 linux_abi64_munmap_denial_count(void)
{
    return g_linux_abi64_munmap_denial_count;
}

u32 linux_abi64_brk_query_count(void)
{
    return g_linux_abi64_brk_query_count;
}

u32 linux_abi64_brk_extend_count(void)
{
    return g_linux_abi64_brk_extend_count;
}

u32 linux_abi64_brk_denial_count(void)
{
    return g_linux_abi64_brk_denial_count;
}

u32 linux_abi64_rt_sigaction_count(void)
{
    return g_linux_abi64_rt_sigaction_count;
}

u32 linux_abi64_rt_sigaction_query_count(void)
{
    return g_linux_abi64_rt_sigaction_query_count;
}

u32 linux_abi64_rt_sigaction_denial_count(void)
{
    return g_linux_abi64_rt_sigaction_denial_count;
}

u32 linux_abi64_rt_sigaction_fault_count(void)
{
    return g_linux_abi64_rt_sigaction_fault_count;
}

u32 linux_abi64_rt_sigaction_last_signal(void)
{
    return g_linux_abi64_rt_sigaction_last_signal;
}

u64 linux_abi64_rt_sigaction_last_handler(void)
{
    return g_linux_abi64_rt_sigaction_last_handler;
}

u64 linux_abi64_rt_sigaction_last_old_handler(void)
{
    return g_linux_abi64_rt_sigaction_last_old_handler;
}

u64 linux_abi64_rt_sigaction_last_mask(void)
{
    return g_linux_abi64_rt_sigaction_last_mask;
}

u64 linux_abi64_rt_sigaction_last_flags(void)
{
    return g_linux_abi64_rt_sigaction_last_flags;
}

u32 linux_abi64_rt_sigaction_last_result(void)
{
    return g_linux_abi64_rt_sigaction_last_result;
}

u32 linux_abi64_rt_sigprocmask_count(void)
{
    return g_linux_abi64_rt_sigprocmask_count;
}

u32 linux_abi64_rt_sigprocmask_query_count(void)
{
    return g_linux_abi64_rt_sigprocmask_query_count;
}

u32 linux_abi64_rt_sigprocmask_denial_count(void)
{
    return g_linux_abi64_rt_sigprocmask_denial_count;
}

u32 linux_abi64_rt_sigprocmask_fault_count(void)
{
    return g_linux_abi64_rt_sigprocmask_fault_count;
}

u32 linux_abi64_rt_sigprocmask_last_how(void)
{
    return g_linux_abi64_rt_sigprocmask_last_how;
}

u64 linux_abi64_rt_sigprocmask_last_set(void)
{
    return g_linux_abi64_rt_sigprocmask_last_set;
}

u64 linux_abi64_rt_sigprocmask_last_old_mask(void)
{
    return g_linux_abi64_rt_sigprocmask_last_old_mask;
}

u64 linux_abi64_rt_sigprocmask_last_mask(void)
{
    return g_linux_abi64_rt_sigprocmask_last_mask;
}

u32 linux_abi64_rt_sigprocmask_last_result(void)
{
    return g_linux_abi64_rt_sigprocmask_last_result;
}

u32 linux_abi64_nanosleep_count(void)
{
    return g_linux_abi64_nanosleep_count;
}

u32 linux_abi64_nanosleep_denial_count(void)
{
    return g_linux_abi64_nanosleep_denial_count;
}

u32 linux_abi64_nanosleep_fault_count(void)
{
    return g_linux_abi64_nanosleep_fault_count;
}

u32 linux_abi64_nanosleep_interrupted_count(void)
{
    return g_linux_abi64_nanosleep_interrupted_count;
}

u32 linux_abi64_getrlimit_count(void)
{
    return g_linux_abi64_getrlimit_count;
}

u32 linux_abi64_setrlimit_count(void)
{
    return g_linux_abi64_setrlimit_count;
}

u32 linux_abi64_rlimit_denial_count(void)
{
    return g_linux_abi64_rlimit_denial_count;
}

u32 linux_abi64_rlimit_fault_count(void)
{
    return g_linux_abi64_rlimit_fault_count;
}

u32 linux_abi64_pipe2_count(void)
{
    return g_linux_abi64_pipe2_count;
}

u32 linux_abi64_pipe_count(void)
{
    return g_linux_abi64_pipe_count;
}

u32 linux_abi64_pipe_denial_count(void)
{
    return g_linux_abi64_pipe_denial_count;
}

u32 linux_abi64_pipe_fault_count(void)
{
    return g_linux_abi64_pipe_fault_count;
}

u32 linux_abi64_pipe2_denial_count(void)
{
    return g_linux_abi64_pipe2_denial_count;
}

u32 linux_abi64_pipe2_fault_count(void)
{
    return g_linux_abi64_pipe2_fault_count;
}

u32 linux_abi64_dup_count(void)
{
    return g_linux_abi64_dup_count;
}

u32 linux_abi64_dup2_count(void)
{
    return g_linux_abi64_dup2_count;
}

u32 linux_abi64_dup3_count(void)
{
    return g_linux_abi64_dup3_count;
}

u32 linux_abi64_dup_denial_count(void)
{
    return g_linux_abi64_dup_denial_count;
}

u32 linux_abi64_fcntl_count(void)
{
    return g_linux_abi64_fcntl_count;
}

u32 linux_abi64_fcntl_denial_count(void)
{
    return g_linux_abi64_fcntl_denial_count;
}

u32 linux_abi64_getcwd_count(void)
{
    return g_linux_abi64_getcwd_count;
}

u32 linux_abi64_getcwd_byte_count(void)
{
    return g_linux_abi64_getcwd_byte_count;
}

u32 linux_abi64_getcwd_denial_count(void)
{
    return g_linux_abi64_getcwd_denial_count;
}

u32 linux_abi64_getcwd_fault_count(void)
{
    return g_linux_abi64_getcwd_fault_count;
}

u32 linux_abi64_path_relative_count(void)
{
    return g_linux_abi64_path_relative_count;
}

u32 linux_abi64_path_dot_count(void)
{
    return g_linux_abi64_path_dot_count;
}

u32 linux_abi64_path_dotdot_count(void)
{
    return g_linux_abi64_path_dotdot_count;
}

u32 linux_abi64_path_trailing_count(void)
{
    return g_linux_abi64_path_trailing_count;
}

u32 linux_abi64_path_trailing_denial_count(void)
{
    return g_linux_abi64_path_trailing_denial_count;
}

u32 linux_abi64_path_fault_count(void)
{
    return g_linux_abi64_path_fault_count;
}

u32 linux_abi64_chdir_count(void)
{
    return g_linux_abi64_chdir_count;
}

u32 linux_abi64_fchdir_count(void)
{
    return g_linux_abi64_fchdir_count;
}

u32 linux_abi64_chdir_denial_count(void)
{
    return g_linux_abi64_chdir_denial_count;
}

u32 linux_abi64_chdir_fault_count(void)
{
    return g_linux_abi64_chdir_fault_count;
}

u32 linux_abi64_getdents64_count(void)
{
    return g_linux_abi64_getdents64_count;
}

u32 linux_abi64_getdents64_entry_count(void)
{
    return g_linux_abi64_getdents64_entry_count;
}

u32 linux_abi64_getdents64_byte_count(void)
{
    return g_linux_abi64_getdents64_byte_count;
}

u32 linux_abi64_getdents64_denial_count(void)
{
    return g_linux_abi64_getdents64_denial_count;
}

u32 linux_abi64_getdents64_fault_count(void)
{
    return g_linux_abi64_getdents64_fault_count;
}

u32 linux_abi64_futex_wait_count(void)
{
    return g_linux_abi64_futex_wait_count;
}

u32 linux_abi64_futex_wake_count(void)
{
    return g_linux_abi64_futex_wake_count;
}

u32 linux_abi64_futex_woken_count(void)
{
    return g_linux_abi64_futex_woken_count;
}

u32 linux_abi64_futex_waiter_count(void)
{
    return linux_abi64_futex_active_waiters();
}

u32 linux_abi64_futex_eagain_count(void)
{
    return g_linux_abi64_futex_eagain_count;
}

u32 linux_abi64_futex_denial_count(void)
{
    return g_linux_abi64_futex_denial_count;
}

u32 linux_abi64_futex_fault_count(void)
{
    return g_linux_abi64_futex_fault_count;
}

u32 linux_abi64_futex_timed_wait_count(void)
{
    return g_linux_abi64_futex_timed_wait_count;
}

u32 linux_abi64_futex_timeout_count(void)
{
    return g_linux_abi64_futex_timeout_count;
}

u32 linux_abi64_futex_last_wait_pid(void)
{
    return g_linux_abi64_futex_last_wait_pid;
}

u64 linux_abi64_futex_last_wait_address(void)
{
    return g_linux_abi64_futex_last_wait_address;
}

u32 linux_abi64_futex_last_wait_value(void)
{
    return g_linux_abi64_futex_last_wait_value;
}

u32 linux_abi64_futex_last_wait_task_id(void)
{
    return g_linux_abi64_futex_last_wait_task_id;
}

u32 linux_abi64_futex_last_wake_count(void)
{
    return g_linux_abi64_futex_last_wake_count;
}

u32 linux_abi64_futex_last_timeout_task_id(void)
{
    return g_linux_abi64_futex_last_timeout_task_id;
}

u32 linux_abi64_futex_last_timeout_ticks(void)
{
    return g_linux_abi64_futex_last_timeout_ticks;
}

u32 linux_abi64_futex_last_timeout_result(void)
{
    return g_linux_abi64_futex_last_timeout_result;
}

u32 linux_abi64_clone_count(void)
{
    return g_linux_abi64_clone_count;
}

u32 linux_abi64_clone_thread_count(void)
{
    return g_linux_abi64_clone_thread_count;
}

u32 linux_abi64_clone_denial_count(void)
{
    return g_linux_abi64_clone_denial_count;
}

u32 linux_abi64_clone_fork_denial_count(void)
{
    return g_linux_abi64_clone_fork_denial_count;
}

u32 linux_abi64_clone_scheduler_count(void)
{
    return g_linux_abi64_clone_scheduler_count;
}

u32 linux_abi64_fork_count(void)
{
    return g_linux_abi64_fork_count;
}

u32 linux_abi64_fork_success_count(void)
{
    return g_linux_abi64_fork_success_count;
}

u32 linux_abi64_fork_enosys_count(void)
{
    return g_linux_abi64_fork_enosys_count;
}

u32 linux_abi64_fork_denial_count(void)
{
    return g_linux_abi64_fork_denial_count;
}

u64 linux_abi64_fork_last_rip(void)
{
    return g_linux_abi64_fork_last_rip;
}

u32 linux_abi64_fork_last_child_pid(void)
{
    return g_linux_abi64_fork_last_child_pid;
}

u32 linux_abi64_fork_last_child_slot(void)
{
    return g_linux_abi64_fork_last_child_slot;
}

u32 linux_abi64_fork_last_child_root_distinct(void)
{
    return g_linux_abi64_fork_last_child_root_distinct;
}

u32 linux_abi64_fork_last_task_id(void)
{
    return g_linux_abi64_fork_last_task_id;
}

u32 linux_abi64_child_root_cleanup_count(void)
{
    return g_linux_abi64_child_root_cleanup_count;
}

u32 linux_abi64_clone_last_parent_pid(void)
{
    return g_linux_abi64_clone_last_parent_pid;
}

u32 linux_abi64_clone_last_child_pid(void)
{
    return g_linux_abi64_clone_last_child_pid;
}

u32 linux_abi64_clone_last_flags(void)
{
    return g_linux_abi64_clone_last_flags;
}

u32 linux_abi64_clone_last_task_id(void)
{
    return g_linux_abi64_clone_last_task_id;
}

u32 linux_abi64_clone_last_shared_vma(void)
{
    return g_linux_abi64_clone_last_shared_vma;
}

u32 linux_abi64_clone_last_shared_fd(void)
{
    return g_linux_abi64_clone_last_shared_fd;
}

u32 linux_abi64_clone_last_shared_audit(void)
{
    return g_linux_abi64_clone_last_shared_audit;
}

u64 linux_abi64_clone_last_child_stack(void)
{
    return g_linux_abi64_clone_last_child_stack;
}

u64 linux_abi64_clone_last_tls_base(void)
{
    return g_linux_abi64_clone_last_tls_base;
}

u32 linux_abi64_release_clone(u32 child_pid)
{
    linux_abi64_clone_record_t *record = linux_abi64_clone_record_for_child(child_pid);
    linux_abi64_exit_record_t *exit_record = linux_abi64_exit_record_existing(child_pid);

    if (record == 0)
    {
        return 0u;
    }

    (void)persona64_release(child_pid);
    linux_abi64_clone_detach_shared_state(child_pid);
    if (process64_release_clone(child_pid) == 0u)
    {
        return 0u;
    }
    linux_abi64_clear_clone_record(record);
    linux_abi64_clear_exit_record(exit_record);
    return 1u;
}

u32 linux_abi64_execve_count(void)
{
    return g_linux_abi64_execve_count;
}

u32 linux_abi64_execveat_count(void)
{
    return g_linux_abi64_execveat_count;
}

u32 linux_abi64_execve_denial_count(void)
{
    return g_linux_abi64_execve_denial_count;
}

u32 linux_abi64_execve_fault_count(void)
{
    return g_linux_abi64_execve_fault_count;
}

u32 linux_abi64_execve_last_error(void)
{
    return g_linux_abi64_execve_last_error;
}

u32 linux_abi64_execve_last_path_checksum(void)
{
    return g_linux_abi64_execve_last_path_checksum;
}

u32 linux_abi64_execve_last_binary_bytes(void)
{
    return g_linux_abi64_execve_last_binary_bytes;
}

u32 linux_abi64_execve_last_closed_fds(void)
{
    return g_linux_abi64_execve_last_closed_fds;
}

u32 linux_abi64_execve_last_fd_live_before(void)
{
    return g_linux_abi64_execve_last_fd_live_before;
}

u32 linux_abi64_execve_last_fd_live_after(void)
{
    return g_linux_abi64_execve_last_fd_live_after;
}

u32 linux_abi64_execve_last_vma_before(void)
{
    return g_linux_abi64_execve_last_vma_before;
}

u32 linux_abi64_execve_last_vma_released(void)
{
    return g_linux_abi64_execve_last_vma_released;
}

u32 linux_abi64_execve_last_vma_after(void)
{
    return g_linux_abi64_execve_last_vma_after;
}

u32 linux_abi64_execve_last_argc(void)
{
    return g_linux_abi64_execve_last_argc;
}

u32 linux_abi64_execve_last_envc(void)
{
    return g_linux_abi64_execve_last_envc;
}

u32 linux_abi64_execve_last_transfer_ready(void)
{
    return g_linux_abi64_execve_last_transfer_ready;
}

u64 linux_abi64_execve_last_transfer_rip(void)
{
    return g_linux_abi64_execve_last_transfer_rip;
}

u64 linux_abi64_execve_last_transfer_rsp(void)
{
    return g_linux_abi64_execve_last_transfer_rsp;
}

u32 linux_abi64_execve_last_entry_prot(void)
{
    return g_linux_abi64_execve_last_entry_prot;
}

u32 linux_abi64_execve_last_stack_prot(void)
{
    return g_linux_abi64_execve_last_stack_prot;
}

u32 linux_abi64_execve_consume_transfer(u32 pid, u64 *rip_out, u64 *rsp_out)
{
    if (rip_out != 0)
    {
        *rip_out = 0ull;
    }
    if (rsp_out != 0)
    {
        *rsp_out = 0ull;
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (rip_out == 0)
        || (rsp_out == 0)
        || (g_linux_abi64_execve_transfer_pending == 0u)
        || (g_linux_abi64_execve_transfer_pid != pid)
        || (g_linux_abi64_execve_last_transfer_ready == 0u)
        || (g_linux_abi64_execve_last_transfer_rip == 0ull)
        || (g_linux_abi64_execve_last_transfer_rsp == 0ull))
    {
        return 0u;
    }

    *rip_out = g_linux_abi64_execve_last_transfer_rip;
    *rsp_out = g_linux_abi64_execve_last_transfer_rsp;
    g_linux_abi64_execve_transfer_pending = 0u;
    g_linux_abi64_execve_transfer_pid = PROCESS64_INVALID_PID;
    return 1u;
}

u32 linux_abi64_wait4_count(void)
{
    return g_linux_abi64_wait4_count;
}

u32 linux_abi64_wait4_reap_count(void)
{
    return g_linux_abi64_wait4_reap_count;
}

u32 linux_abi64_wait4_nohang_count(void)
{
    return g_linux_abi64_wait4_nohang_count;
}

u32 linux_abi64_wait4_denial_count(void)
{
    return g_linux_abi64_wait4_denial_count;
}

u32 linux_abi64_wait4_fault_count(void)
{
    return g_linux_abi64_wait4_fault_count;
}

u32 linux_abi64_wait4_last_parent_pid(void)
{
    return g_linux_abi64_wait4_last_parent_pid;
}

u32 linux_abi64_wait4_last_child_pid(void)
{
    return g_linux_abi64_wait4_last_child_pid;
}

u32 linux_abi64_wait4_last_exit_code(void)
{
    return g_linux_abi64_wait4_last_exit_code;
}

u32 linux_abi64_wait4_last_status(void)
{
    return g_linux_abi64_wait4_last_status;
}

u32 linux_abi64_wait4_last_status_written(void)
{
    return g_linux_abi64_wait4_last_status_written;
}

u32 linux_abi64_wait4_last_options(void)
{
    return g_linux_abi64_wait4_last_options;
}

u32 linux_abi64_wait4_last_process_release(void)
{
    return g_linux_abi64_wait4_last_process_release;
}

u32 linux_abi64_wait4_last_clone_release(void)
{
    return g_linux_abi64_wait4_last_clone_release;
}

u32 linux_abi64_kill_count(void)
{
    return g_linux_abi64_kill_count;
}

u32 linux_abi64_tkill_count(void)
{
    return g_linux_abi64_tkill_count;
}

u32 linux_abi64_kill_unavailable_count(void)
{
    return g_linux_abi64_kill_unavailable_count;
}

u32 linux_abi64_kill_denial_count(void)
{
    return g_linux_abi64_kill_denial_count;
}

u32 linux_abi64_kill_last_syscall(void)
{
    return g_linux_abi64_kill_last_syscall;
}

u32 linux_abi64_kill_last_target(void)
{
    return g_linux_abi64_kill_last_target;
}

u32 linux_abi64_kill_last_signal(void)
{
    return g_linux_abi64_kill_last_signal;
}

u32 linux_abi64_kill_last_result(void)
{
    return g_linux_abi64_kill_last_result;
}

u32 linux_abi64_signal_pending_count(void)
{
    return g_linux_abi64_signal_pending_count;
}

u32 linux_abi64_signal_delivery_count(void)
{
    return g_linux_abi64_signal_delivery_count;
}

u32 linux_abi64_signal_masked_count(void)
{
    return g_linux_abi64_signal_masked_count;
}

u32 linux_abi64_signal_delivery_denial_count(void)
{
    return g_linux_abi64_signal_delivery_denial_count;
}

u32 linux_abi64_signal_delivery_fault_count(void)
{
    return g_linux_abi64_signal_delivery_fault_count;
}

u32 linux_abi64_signal_delivery_last_signal(void)
{
    return g_linux_abi64_signal_delivery_last_signal;
}

u64 linux_abi64_signal_delivery_last_handler(void)
{
    return g_linux_abi64_signal_delivery_last_handler;
}

u64 linux_abi64_signal_delivery_last_frame(void)
{
    return g_linux_abi64_signal_delivery_last_frame;
}

u64 linux_abi64_signal_delivery_last_saved_rip(void)
{
    return g_linux_abi64_signal_delivery_last_saved_rip;
}

u64 linux_abi64_signal_delivery_last_saved_rsp(void)
{
    return g_linux_abi64_signal_delivery_last_saved_rsp;
}

u64 linux_abi64_signal_delivery_last_mask(void)
{
    return g_linux_abi64_signal_delivery_last_mask;
}

u32 linux_abi64_signal_delivery_last_result(void)
{
    return g_linux_abi64_signal_delivery_last_result;
}

u32 linux_abi64_rt_sigreturn_count(void)
{
    return g_linux_abi64_rt_sigreturn_count;
}

u32 linux_abi64_rt_sigreturn_denial_count(void)
{
    return g_linux_abi64_rt_sigreturn_denial_count;
}

u32 linux_abi64_rt_sigreturn_fault_count(void)
{
    return g_linux_abi64_rt_sigreturn_fault_count;
}

u64 linux_abi64_rt_sigreturn_last_frame(void)
{
    return g_linux_abi64_rt_sigreturn_last_frame;
}

u64 linux_abi64_rt_sigreturn_last_rip(void)
{
    return g_linux_abi64_rt_sigreturn_last_rip;
}

u64 linux_abi64_rt_sigreturn_last_rsp(void)
{
    return g_linux_abi64_rt_sigreturn_last_rsp;
}

u64 linux_abi64_rt_sigreturn_last_mask(void)
{
    return g_linux_abi64_rt_sigreturn_last_mask;
}

u64 linux_abi64_rt_sigreturn_last_rax(void)
{
    return g_linux_abi64_rt_sigreturn_last_rax;
}

u32 linux_abi64_rt_sigreturn_last_result(void)
{
    return g_linux_abi64_rt_sigreturn_last_result;
}

u32 linux_abi64_getrandom_count(void)
{
    return g_linux_abi64_getrandom_count;
}

u32 linux_abi64_getrandom_byte_count(void)
{
    return g_linux_abi64_getrandom_byte_count;
}

u32 linux_abi64_getrandom_denial_count(void)
{
    return g_linux_abi64_getrandom_denial_count;
}

u32 linux_abi64_getrandom_fault_count(void)
{
    return g_linux_abi64_getrandom_fault_count;
}

u32 linux_abi64_getrandom_last_byte_count(void)
{
    return g_linux_abi64_getrandom_last_byte_count;
}

u32 linux_abi64_getrandom_last_checksum(void)
{
    return g_linux_abi64_getrandom_last_checksum;
}

u32 linux_abi64_getrandom_last_flags(void)
{
    return g_linux_abi64_getrandom_last_flags;
}

u32 linux_abi64_getrandom_last_result(void)
{
    return g_linux_abi64_getrandom_last_result;
}

u32 linux_abi64_getpid_count(void)
{
    return g_linux_abi64_getpid_count;
}

u32 linux_abi64_getpid_denial_count(void)
{
    return g_linux_abi64_getpid_denial_count;
}

u32 linux_abi64_geteuid_count(void)
{
    return g_linux_abi64_geteuid_count;
}

u32 linux_abi64_geteuid_denial_count(void)
{
    return g_linux_abi64_geteuid_denial_count;
}

u32 linux_abi64_getppid_count(void)
{
    return g_linux_abi64_getppid_count;
}

u32 linux_abi64_getppid_denial_count(void)
{
    return g_linux_abi64_getppid_denial_count;
}

u32 linux_abi64_gettid_count(void)
{
    return g_linux_abi64_gettid_count;
}

u32 linux_abi64_gettid_denial_count(void)
{
    return g_linux_abi64_gettid_denial_count;
}

u32 linux_abi64_ioctl_count(void)
{
    return g_linux_abi64_ioctl_count;
}

u32 linux_abi64_ioctl_tty_count(void)
{
    return g_linux_abi64_ioctl_tty_count;
}

u32 linux_abi64_ioctl_enotty_count(void)
{
    return g_linux_abi64_ioctl_enotty_count;
}

u32 linux_abi64_ioctl_enosys_count(void)
{
    return g_linux_abi64_ioctl_enosys_count;
}

u32 linux_abi64_ioctl_denial_count(void)
{
    return g_linux_abi64_ioctl_denial_count;
}

u32 linux_abi64_ioctl_last_fd(void)
{
    return g_linux_abi64_ioctl_last_fd;
}

u32 linux_abi64_ioctl_last_request(void)
{
    return g_linux_abi64_ioctl_last_request;
}

u32 linux_abi64_ioctl_last_result(void)
{
    return g_linux_abi64_ioctl_last_result;
}

u32 linux_abi64_prctl_count(void)
{
    return g_linux_abi64_prctl_count;
}

u32 linux_abi64_prctl_set_name_count(void)
{
    return g_linux_abi64_prctl_set_name_count;
}

u32 linux_abi64_prctl_get_name_count(void)
{
    return g_linux_abi64_prctl_get_name_count;
}

u32 linux_abi64_prctl_enosys_count(void)
{
    return g_linux_abi64_prctl_enosys_count;
}

u32 linux_abi64_prctl_denial_count(void)
{
    return g_linux_abi64_prctl_denial_count;
}

u32 linux_abi64_prctl_fault_count(void)
{
    return g_linux_abi64_prctl_fault_count;
}

u32 linux_abi64_prctl_last_option(void)
{
    return g_linux_abi64_prctl_last_option;
}

u32 linux_abi64_prctl_last_result(void)
{
    return g_linux_abi64_prctl_last_result;
}

u32 linux_abi64_arch_prctl_count(void)
{
    return g_linux_abi64_arch_prctl_count;
}

u32 linux_abi64_arch_prctl_set_count(void)
{
    return g_linux_abi64_arch_prctl_set_count;
}

u32 linux_abi64_arch_prctl_get_count(void)
{
    return g_linux_abi64_arch_prctl_get_count;
}

u32 linux_abi64_arch_prctl_denial_count(void)
{
    return g_linux_abi64_arch_prctl_denial_count;
}

u32 linux_abi64_arch_prctl_fault_count(void)
{
    return g_linux_abi64_arch_prctl_fault_count;
}

u32 linux_abi64_set_tid_address_count(void)
{
    return g_linux_abi64_set_tid_address_count;
}

u32 linux_abi64_set_tid_address_denial_count(void)
{
    return g_linux_abi64_set_tid_address_denial_count;
}

u32 linux_abi64_clock_gettime_count(void)
{
    return g_linux_abi64_clock_gettime_count;
}

u32 linux_abi64_clock_gettime_denial_count(void)
{
    return g_linux_abi64_clock_gettime_denial_count;
}

u32 linux_abi64_clock_gettime_fault_count(void)
{
    return g_linux_abi64_clock_gettime_fault_count;
}

u32 linux_abi64_exit_count(void)
{
    return g_linux_abi64_exit_count;
}

u32 linux_abi64_exit_group_count(void)
{
    return g_linux_abi64_exit_group_count;
}

u32 linux_abi64_exit_denial_count(void)
{
    return g_linux_abi64_exit_denial_count;
}

u32 linux_abi64_process_exited(u32 pid)
{
    linux_abi64_exit_record_t *record = linux_abi64_exit_record_for_pid(pid);

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
        record = linux_abi64_exit_record_for_pid(pid);
    }

    return ((record != 0) && (record->pid == pid) && (record->exited != 0u)) ? 1u : 0u;
}

u32 linux_abi64_exit_code(u32 pid)
{
    linux_abi64_exit_record_t *record = linux_abi64_exit_record_for_pid(pid);

    if (g_linux_abi64_initialized == 0u)
    {
        linux_abi64_init();
        record = linux_abi64_exit_record_for_pid(pid);
    }

    return ((record != 0) && (record->pid == pid) && (record->exited != 0u))
        ? record->exit_code
        : 0u;
}

u32 linux_abi64_last_exit_pid(void)
{
    return g_linux_abi64_last_exit_pid;
}

u32 linux_abi64_last_exit_code(void)
{
    return g_linux_abi64_last_exit_code;
}

u32 linux_abi64_last_exit_vma_regions(void)
{
    return g_linux_abi64_last_exit_vma_regions;
}

u32 linux_abi64_last_exit_fd_entries(void)
{
    return g_linux_abi64_last_exit_fd_entries;
}

u32 linux_abi64_last_exit_persona_released(void)
{
    return g_linux_abi64_last_exit_persona_released;
}

u32 linux_abi64_last_exit_audit_released(void)
{
    return g_linux_abi64_last_exit_audit_released;
}

u32 linux_abi64_last_exit_audit_recorded(void)
{
    return g_linux_abi64_last_exit_audit_recorded;
}

void *linux_abi64_last_exit_detached_vma(void)
{
    return g_linux_abi64_last_exit_detached_vma;
}

void *linux_abi64_last_exit_detached_fd(void)
{
    return g_linux_abi64_last_exit_detached_fd;
}

void *linux_abi64_last_exit_detached_audit(void)
{
    return g_linux_abi64_last_exit_detached_audit;
}
