#ifndef LIMITLESS_LINUX_ABI_X64_H
#define LIMITLESS_LINUX_ABI_X64_H

#include "types.h"

struct interrupt_frame64;

#define LINUX_ABI64_SYSCALL_LIMIT 512u
#define LINUX_ABI64_SYSCALL_READ 0u
#define LINUX_ABI64_SYSCALL_WRITE 1u
#define LINUX_ABI64_SYSCALL_OPEN 2u
#define LINUX_ABI64_SYSCALL_CLOSE 3u
#define LINUX_ABI64_SYSCALL_STAT 4u
#define LINUX_ABI64_SYSCALL_FSTAT 5u
#define LINUX_ABI64_SYSCALL_LSTAT 6u
#define LINUX_ABI64_SYSCALL_POLL 7u
#define LINUX_ABI64_SYSCALL_LSEEK 8u
#define LINUX_ABI64_SYSCALL_MMAP 9u
#define LINUX_ABI64_SYSCALL_MPROTECT 10u
#define LINUX_ABI64_SYSCALL_MUNMAP 11u
#define LINUX_ABI64_SYSCALL_BRK 12u
#define LINUX_ABI64_SYSCALL_RT_SIGACTION 13u
#define LINUX_ABI64_SYSCALL_RT_SIGPROCMASK 14u
#define LINUX_ABI64_SYSCALL_RT_SIGRETURN 15u
#define LINUX_ABI64_SYSCALL_IOCTL 16u
#define LINUX_ABI64_SYSCALL_PREAD64 17u
#define LINUX_ABI64_SYSCALL_PWRITE64 18u
#define LINUX_ABI64_SYSCALL_READV 19u
#define LINUX_ABI64_SYSCALL_WRITEV 20u
#define LINUX_ABI64_SYSCALL_DUP 32u
#define LINUX_ABI64_SYSCALL_DUP2 33u
#define LINUX_ABI64_SYSCALL_NANOSLEEP 35u
#define LINUX_ABI64_SYSCALL_GETPID 39u
#define LINUX_ABI64_SYSCALL_CLONE 56u
#define LINUX_ABI64_SYSCALL_FORK 57u
#define LINUX_ABI64_SYSCALL_EXECVE 59u
#define LINUX_ABI64_SYSCALL_EXIT 60u
#define LINUX_ABI64_SYSCALL_WAIT4 61u
#define LINUX_ABI64_SYSCALL_KILL 62u
#define LINUX_ABI64_SYSCALL_FCNTL 72u
#define LINUX_ABI64_SYSCALL_GETCWD 79u
#define LINUX_ABI64_SYSCALL_CHDIR 80u
#define LINUX_ABI64_SYSCALL_FCHDIR 81u
#define LINUX_ABI64_SYSCALL_READLINK 89u
#define LINUX_ABI64_SYSCALL_GETRLIMIT 97u
#define LINUX_ABI64_SYSCALL_GETEUID 107u
#define LINUX_ABI64_SYSCALL_GETPPID 110u
#define LINUX_ABI64_SYSCALL_PRCTL 157u
#define LINUX_ABI64_SYSCALL_ARCH_PRCTL 158u
#define LINUX_ABI64_SYSCALL_SETRLIMIT 160u
#define LINUX_ABI64_SYSCALL_GETTID 186u
#define LINUX_ABI64_SYSCALL_TKILL 200u
#define LINUX_ABI64_SYSCALL_FUTEX 202u
#define LINUX_ABI64_SYSCALL_GETDENTS64 217u
#define LINUX_ABI64_SYSCALL_SET_TID_ADDRESS 218u
#define LINUX_ABI64_SYSCALL_CLOCK_GETTIME 228u
#define LINUX_ABI64_SYSCALL_EXIT_GROUP 231u
#define LINUX_ABI64_SYSCALL_OPENAT 257u
#define LINUX_ABI64_SYSCALL_NEWFSTATAT 262u
#define LINUX_ABI64_SYSCALL_PPOLL 271u
#define LINUX_ABI64_SYSCALL_DUP3 292u
#define LINUX_ABI64_SYSCALL_PIPE2 293u
#define LINUX_ABI64_SYSCALL_GETRANDOM 318u
#define LINUX_ABI64_SYSCALL_EXECVEAT 322u
#define LINUX_ABI64_ARCH_SET_FS 0x00001002u
#define LINUX_ABI64_ARCH_GET_FS 0x00001003u
#define LINUX_ABI64_CLOCK_REALTIME 0u
#define LINUX_ABI64_CLOCK_MONOTONIC 1u
#define LINUX_ABI64_CLOCK_MONOTONIC_RAW 4u
#define LINUX_ABI64_CLOCK_REALTIME_COARSE 5u
#define LINUX_ABI64_CLOCK_MONOTONIC_COARSE 6u
#define LINUX_ABI64_CLOCK_BOOTTIME 7u
#define LINUX_ABI64_READ_CHUNK_BYTES 128u
#define LINUX_ABI64_WRITE_CHUNK_BYTES 256u
#define LINUX_ABI64_IOVEC_BYTES 16u
#define LINUX_ABI64_IOV_MAX 8u
#define LINUX_ABI64_POLLFD_BYTES 8u
#define LINUX_ABI64_POLL_MAX_FDS 8u
#define LINUX_ABI64_SIGSET_BYTES 8u
#define LINUX_ABI64_POLLIN 0x0001u
#define LINUX_ABI64_POLLOUT 0x0004u
#define LINUX_ABI64_POLLERR 0x0008u
#define LINUX_ABI64_POLLHUP 0x0010u
#define LINUX_ABI64_POLLNVAL 0x0020u
#define LINUX_ABI64_STAT_BYTES 144u
#define LINUX_ABI64_TIMESPEC_BYTES 16u
#define LINUX_ABI64_RLIMIT_BYTES 16u
#define LINUX_ABI64_WINSIZE_BYTES 8u
#define LINUX_ABI64_TERMINAL_ROWS 25u
#define LINUX_ABI64_TERMINAL_COLUMNS 80u
#define LINUX_ABI64_TERMINAL_READ_WAIT_TICKS 300u
#define LINUX_ABI64_TERMINAL_READ_SPIN_BUDGET 40000000u
#define LINUX_ABI64_DIRENT64_HEADER_BYTES 19u
#define LINUX_ABI64_DIRENT64_ALIGN_BYTES 8u
#define LINUX_ABI64_DIRENT64_MAX_RECORD_BYTES 64u
#define LINUX_ABI64_EXEC_BINARY_MAX_BYTES 256u
#define LINUX_ABI64_EXEC_STACK_BASE 0x00000000441E0000ull
#define LINUX_ABI64_EXEC_STACK_BYTES 0x00001000u
#define LINUX_ABI64_EXEC_ARG_MAX 8u
#define LINUX_ABI64_EXEC_ENV_MAX 8u
#define LINUX_ABI64_GETRANDOM_MAX_BYTES 256u
#define LINUX_ABI64_FIXED_UID 1000u
#define LINUX_ABI64_FIXED_PPID 1u
#define LINUX_ABI64_PR_SET_NAME 15u
#define LINUX_ABI64_PR_GET_NAME 16u
#define LINUX_ABI64_PR_NAME_BYTES 16u
#define LINUX_ABI64_TCGETS 0x00005401u
#define LINUX_ABI64_TIOCGETD 0x00005424u
#define LINUX_ABI64_TIOCGETP 0x00005408u
#define LINUX_ABI64_TIOCGETC 0x00005412u
#define LINUX_ABI64_TIOCGWINSZ 0x00005413u
#define LINUX_ABI64_TIOCGPGRP 0x0000540Fu
#define LINUX_ABI64_TIOCOUTQ 0x00005411u
#define LINUX_ABI64_TIOCINQ 0x0000541Bu
#define LINUX_ABI64_TIOCGSID 0x00005429u
#define LINUX_ABI64_GETRANDOM_SUPPORTED_FLAGS 0u
#define LINUX_ABI64_POSITIONAL_OFFSET_MAX 0x00000000FFFFFFFFull
#define LINUX_ABI64_FUTEX_WAIT 0u
#define LINUX_ABI64_FUTEX_WAKE 1u
#define LINUX_ABI64_FUTEX_CMD_MASK 0x0000007Fu
#define LINUX_ABI64_FUTEX_PRIVATE_FLAG 0x00000080u
#define LINUX_ABI64_CLONE_VM 0x00000100u
#define LINUX_ABI64_CLONE_FS 0x00000200u
#define LINUX_ABI64_CLONE_FILES 0x00000400u
#define LINUX_ABI64_CLONE_SIGHAND 0x00000800u
#define LINUX_ABI64_CLONE_THREAD 0x00010000u
#define LINUX_ABI64_CLONE_SETTLS 0x00080000u
#define LINUX_ABI64_CLONE_PARENT_SETTID 0x00100000u
#define LINUX_ABI64_CLONE_CHILD_CLEARTID 0x00200000u
#define LINUX_ABI64_CLONE_CHILD_SETTID 0x01000000u
#define LINUX_ABI64_CLONE_THREAD_REQUIRED \
    (LINUX_ABI64_CLONE_VM \
        | LINUX_ABI64_CLONE_FS \
        | LINUX_ABI64_CLONE_FILES \
        | LINUX_ABI64_CLONE_SIGHAND \
        | LINUX_ABI64_CLONE_THREAD)
#define LINUX_ABI64_CLONE_SUPPORTED_MASK \
    (LINUX_ABI64_CLONE_THREAD_REQUIRED \
        | LINUX_ABI64_CLONE_SETTLS \
        | LINUX_ABI64_CLONE_PARENT_SETTID \
        | LINUX_ABI64_CLONE_CHILD_CLEARTID \
        | LINUX_ABI64_CLONE_CHILD_SETTID)
#define LINUX_ABI64_WAIT_ANY 0xFFFFFFFFFFFFFFFFull
#define LINUX_ABI64_WAIT_WNOHANG 0x00000001u
#define LINUX_ABI64_WAIT_SUPPORTED_OPTIONS LINUX_ABI64_WAIT_WNOHANG
#define LINUX_ABI64_SIGNAL_MAX 64u
#define LINUX_ABI64_RLIM_INFINITY 0xFFFFFFFFFFFFFFFFull
#define LINUX_ABI64_RLIMIT_CPU 0u
#define LINUX_ABI64_RLIMIT_FSIZE 1u
#define LINUX_ABI64_RLIMIT_DATA 2u
#define LINUX_ABI64_RLIMIT_STACK 3u
#define LINUX_ABI64_RLIMIT_CORE 4u
#define LINUX_ABI64_RLIMIT_RSS 5u
#define LINUX_ABI64_RLIMIT_NPROC 6u
#define LINUX_ABI64_RLIMIT_NOFILE 7u
#define LINUX_ABI64_RLIMIT_MEMLOCK 8u
#define LINUX_ABI64_RLIMIT_AS 9u
#define LINUX_ABI64_RLIMIT_LOCKS 10u
#define LINUX_ABI64_RLIMIT_SIGPENDING 11u
#define LINUX_ABI64_RLIMIT_MSGQUEUE 12u
#define LINUX_ABI64_RLIMIT_NICE 13u
#define LINUX_ABI64_RLIMIT_RTPRIO 14u
#define LINUX_ABI64_RLIMIT_RTTIME 15u
#define LINUX_ABI64_RLIMIT_COUNT 16u
#define LINUX_ABI64_RLIMIT_STACK_BYTES (8ull * 1024ull * 1024ull)
#define LINUX_ABI64_RLIMIT_DATA_BYTES (64ull * 1024ull * 1024ull)
#define LINUX_ABI64_RLIMIT_NOFILE_COUNT 1024ull
#define LINUX_ABI64_AT_FDCWD 0xFFFFFFFFFFFFFF9Cull
#define LINUX_ABI64_AT_EMPTY_PATH 0x00001000u
#define LINUX_ABI64_AT_SYMLINK_NOFOLLOW 0x00000100u
#define LINUX_ABI64_O_ACCMODE 0x00000003u
#define LINUX_ABI64_O_RDONLY 0x00000000u
#define LINUX_ABI64_O_WRONLY 0x00000001u
#define LINUX_ABI64_O_RDWR 0x00000002u
#define LINUX_ABI64_O_CREAT 0x00000040u
#define LINUX_ABI64_O_NONBLOCK 0x00000800u
#define LINUX_ABI64_O_LARGEFILE 0x00008000u
#define LINUX_ABI64_O_DIRECTORY 0x00010000u
#define LINUX_ABI64_O_NOFOLLOW 0x00020000u
#define LINUX_ABI64_O_CLOEXEC 0x00080000u
#define LINUX_ABI64_F_DUPFD 0u
#define LINUX_ABI64_F_GETFD 1u
#define LINUX_ABI64_F_SETFD 2u
#define LINUX_ABI64_F_GETFL 3u
#define LINUX_ABI64_F_SETFL 4u
#define LINUX_ABI64_FD_CLOEXEC 0x00000001u
#define LINUX_ABI64_SEEK_SET 0u
#define LINUX_ABI64_SEEK_CUR 1u
#define LINUX_ABI64_SEEK_END 2u
#define LINUX_ABI64_PROT_READ 0x00000001u
#define LINUX_ABI64_PROT_WRITE 0x00000002u
#define LINUX_ABI64_PROT_EXEC 0x00000004u
#define LINUX_ABI64_MAP_SHARED 0x00000001u
#define LINUX_ABI64_MAP_PRIVATE 0x00000002u
#define LINUX_ABI64_MAP_FIXED 0x00000010u
#define LINUX_ABI64_MAP_ANONYMOUS 0x00000020u
#define LINUX_ABI64_MAP_SUPPORTED_MASK \
    (LINUX_ABI64_MAP_SHARED \
        | LINUX_ABI64_MAP_PRIVATE \
        | LINUX_ABI64_MAP_FIXED \
        | LINUX_ABI64_MAP_ANONYMOUS)
#define LINUX_ABI64_ENOENT 2u
#define LINUX_ABI64_ESRCH 3u
#define LINUX_ABI64_EINTR 4u
#define LINUX_ABI64_E2BIG 7u
#define LINUX_ABI64_EBADF 9u
#define LINUX_ABI64_ECHILD 10u
#define LINUX_ABI64_EAGAIN 11u
#define LINUX_ABI64_ENOMEM 12u
#define LINUX_ABI64_EFAULT 14u
#define LINUX_ABI64_ENOTDIR 20u
#define LINUX_ABI64_EINVAL 22u
#define LINUX_ABI64_EMFILE 24u
#define LINUX_ABI64_ENOTTY 25u
#define LINUX_ABI64_ERANGE 34u
#define LINUX_ABI64_ENOSYS 38u
#define LINUX_ABI64_ETIMEDOUT 110u
#define LINUX_ABI64_ERROR_RETURN(error_code) (0ull - (u64)(error_code))

typedef u64 (*linux_abi64_handler_t)(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip);

typedef struct linux_abi64_stat
{
    u64 st_dev;
    u64 st_ino;
    u64 st_nlink;
    u32 st_mode;
    u32 st_uid;
    u32 st_gid;
    u32 __pad0;
    u64 st_rdev;
    u64 st_size;
    u64 st_blksize;
    u64 st_blocks;
    u64 st_atime;
    u64 st_atime_nsec;
    u64 st_mtime;
    u64 st_mtime_nsec;
    u64 st_ctime;
    u64 st_ctime_nsec;
    u64 __unused[3];
} linux_abi64_stat_t;

typedef struct linux_abi64_timespec
{
    u64 tv_sec;
    u64 tv_nsec;
} linux_abi64_timespec_t;

typedef struct linux_abi64_rlimit
{
    u64 rlim_cur;
    u64 rlim_max;
} linux_abi64_rlimit_t;

typedef struct linux_abi64_iovec
{
    u64 iov_base;
    u64 iov_len;
} linux_abi64_iovec_t;

typedef struct linux_abi64_pollfd
{
    s32 fd;
    u16 events;
    u16 revents;
} linux_abi64_pollfd_t;

void linux_abi64_init(void);
linux_abi64_handler_t *linux_abi64_dispatch_table(void);
u64 linux_abi64_dispatch(
    u32 pid,
    u32 syscall_number,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip);
u64 linux_abi64_sys_read(u32 pid, u64 fd_number, u64 user_buffer, u64 byte_count, u64 rip);
u64 linux_abi64_sys_write(u32 pid, u64 fd_number, u64 user_buffer, u64 byte_count, u64 rip);
u64 linux_abi64_sys_pread64(
    u32 pid,
    u64 fd_number,
    u64 user_buffer,
    u64 byte_count,
    u64 file_offset,
    u64 rip);
u64 linux_abi64_sys_pwrite64(
    u32 pid,
    u64 fd_number,
    u64 user_buffer,
    u64 byte_count,
    u64 file_offset,
    u64 rip);
u64 linux_abi64_sys_readv(u32 pid, u64 fd_number, u64 user_iov, u64 iov_count, u64 rip);
u64 linux_abi64_sys_writev(u32 pid, u64 fd_number, u64 user_iov, u64 iov_count, u64 rip);
u64 linux_abi64_sys_poll(u32 pid, u64 user_fds, u64 fd_count, u64 timeout_ms, u64 rip);
u64 linux_abi64_sys_ppoll(
    u32 pid,
    u64 user_fds,
    u64 fd_count,
    u64 user_timeout,
    u64 user_sigmask,
    u64 sigset_size,
    u64 rip);
u64 linux_abi64_sys_ioctl(u32 pid, u64 fd_number, u64 request, u64 argument, u64 rip);
u64 linux_abi64_sys_open(u32 pid, u64 user_path, u64 flags, u64 mode, u64 rip);
u64 linux_abi64_sys_close(u32 pid, u64 fd_number, u64 rip);
u64 linux_abi64_sys_lseek(u32 pid, u64 fd_number, u64 offset, u64 whence, u64 rip);
u64 linux_abi64_sys_stat(u32 pid, u64 user_path, u64 user_stat, u64 rip);
u64 linux_abi64_sys_lstat(u32 pid, u64 user_path, u64 user_stat, u64 rip);
u64 linux_abi64_sys_fstat(u32 pid, u64 fd_number, u64 user_stat, u64 rip);
u64 linux_abi64_sys_readlink(
    u32 pid,
    u64 user_path,
    u64 user_buffer,
    u64 byte_count,
    u64 rip);
u64 linux_abi64_sys_newfstatat(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 user_stat,
    u64 flags,
    u64 rip);
u64 linux_abi64_sys_openat(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 flags,
    u64 mode,
    u64 rip);
u64 linux_abi64_sys_mmap(
    u32 pid,
    u64 hint_address,
    u64 length,
    u64 prot,
    u64 flags,
    u64 fd_number,
    u64 offset,
    u64 rip);
u64 linux_abi64_sys_mprotect(u32 pid, u64 address, u64 length, u64 prot, u64 rip);
u64 linux_abi64_sys_munmap(u32 pid, u64 address, u64 length, u64 rip);
u64 linux_abi64_sys_brk(u32 pid, u64 requested_brk);
u64 linux_abi64_sys_rt_sigaction(
    u32 pid,
    u64 signal_number,
    u64 user_act,
    u64 user_oldact,
    u64 sigset_size,
    u64 rip);
u64 linux_abi64_sys_rt_sigprocmask(
    u32 pid,
    u64 how,
    u64 user_set,
    u64 user_oldset,
    u64 sigset_size,
    u64 rip);
u64 linux_abi64_sys_rt_sigreturn(u32 pid, struct interrupt_frame64 *frame);
u64 linux_abi64_sys_getpid(u32 pid, u64 rip);
u64 linux_abi64_sys_geteuid(u32 pid, u64 rip);
u64 linux_abi64_sys_getppid(u32 pid, u64 rip);
u64 linux_abi64_sys_gettid(u32 pid, u64 rip);
u64 linux_abi64_sys_prctl(u32 pid, u64 option, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 rip);
u64 linux_abi64_sys_arch_prctl(u32 pid, u64 code, u64 address, u64 rip);
u64 linux_abi64_sys_set_tid_address(u32 pid, u64 clear_child_tid, u64 rip);
u64 linux_abi64_sys_clock_gettime(u32 pid, u64 clock_id, u64 user_timespec, u64 rip);
u64 linux_abi64_sys_nanosleep(u32 pid, u64 user_request, u64 user_remain, u64 rip);
u64 linux_abi64_sys_getrlimit(u32 pid, u64 resource, u64 user_rlimit, u64 rip);
u64 linux_abi64_sys_setrlimit(u32 pid, u64 resource, u64 user_rlimit, u64 rip);
u64 linux_abi64_sys_pipe2(u32 pid, u64 user_pipefd, u64 flags, u64 rip);
u64 linux_abi64_sys_dup(u32 pid, u64 old_fd_number, u64 rip);
u64 linux_abi64_sys_dup2(u32 pid, u64 old_fd_number, u64 new_fd_number, u64 rip);
u64 linux_abi64_sys_dup3(u32 pid, u64 old_fd_number, u64 new_fd_number, u64 flags, u64 rip);
u64 linux_abi64_sys_fcntl(u32 pid, u64 fd_number, u64 command, u64 argument, u64 rip);
u64 linux_abi64_sys_getcwd(u32 pid, u64 user_buffer, u64 byte_count, u64 rip);
u64 linux_abi64_sys_chdir(u32 pid, u64 user_path, u64 rip);
u64 linux_abi64_sys_fchdir(u32 pid, u64 fd_number, u64 rip);
u64 linux_abi64_sys_getdents64(u32 pid, u64 fd_number, u64 user_dirents, u64 byte_count, u64 rip);
u64 linux_abi64_sys_futex(
    u32 pid,
    u64 user_address,
    u64 futex_op,
    u64 value,
    u64 timeout,
    u64 user_address2,
    u64 value3,
    u64 rip);
u64 linux_abi64_sys_clone(
    u32 pid,
    u64 flags,
    u64 child_stack,
    u64 parent_tid,
    u64 child_tid,
    u64 tls,
    u64 rip);
u64 linux_abi64_sys_fork(u32 pid, u64 rip);
u64 linux_abi64_sys_execve(u32 pid, u64 user_path, u64 user_argv, u64 user_envp, u64 rip);
u64 linux_abi64_sys_execveat(
    u32 pid,
    u64 dirfd,
    u64 user_path,
    u64 user_argv,
    u64 user_envp,
    u64 flags,
    u64 rip);
u64 linux_abi64_sys_wait4(
    u32 pid,
    u64 wait_pid,
    u64 user_status,
    u64 options,
    u64 user_rusage,
    u64 rip);
u64 linux_abi64_sys_kill(u32 pid, u64 target_pid, u64 signal_number, u64 rip);
u64 linux_abi64_sys_tkill(u32 pid, u64 target_tid, u64 signal_number, u64 rip);
u32 linux_abi64_signal_deliver_pending(u32 pid, struct interrupt_frame64 *frame);
u64 linux_abi64_sys_getrandom(u32 pid, u64 user_buffer, u64 byte_count, u64 flags, u64 rip);
u64 linux_abi64_sys_exit(u32 pid, u64 exit_code, u64 rip);
u64 linux_abi64_sys_exit_group(u32 pid, u64 exit_code, u64 rip);
u32 linux_abi64_table_size(void);
u32 linux_abi64_unimplemented_entry_count(void);
u32 linux_abi64_read_entry_installed(void);
u32 linux_abi64_write_entry_installed(void);
u32 linux_abi64_pread64_entry_installed(void);
u32 linux_abi64_pwrite64_entry_installed(void);
u32 linux_abi64_readv_entry_installed(void);
u32 linux_abi64_writev_entry_installed(void);
u32 linux_abi64_poll_entry_installed(void);
u32 linux_abi64_ppoll_entry_installed(void);
u32 linux_abi64_open_entry_installed(void);
u32 linux_abi64_close_entry_installed(void);
u32 linux_abi64_lseek_entry_installed(void);
u32 linux_abi64_stat_entry_installed(void);
u32 linux_abi64_fstat_entry_installed(void);
u32 linux_abi64_newfstatat_entry_installed(void);
u32 linux_abi64_mmap_entry_installed(void);
u32 linux_abi64_mprotect_entry_installed(void);
u32 linux_abi64_munmap_entry_installed(void);
u32 linux_abi64_brk_entry_installed(void);
u32 linux_abi64_rt_sigaction_entry_installed(void);
u32 linux_abi64_rt_sigprocmask_entry_installed(void);
u32 linux_abi64_rt_sigreturn_entry_installed(void);
u32 linux_abi64_nanosleep_entry_installed(void);
u32 linux_abi64_getrlimit_entry_installed(void);
u32 linux_abi64_setrlimit_entry_installed(void);
u32 linux_abi64_pipe2_entry_installed(void);
u32 linux_abi64_dup_entry_installed(void);
u32 linux_abi64_dup2_entry_installed(void);
u32 linux_abi64_dup3_entry_installed(void);
u32 linux_abi64_fcntl_entry_installed(void);
u32 linux_abi64_getcwd_entry_installed(void);
u32 linux_abi64_chdir_entry_installed(void);
u32 linux_abi64_fchdir_entry_installed(void);
u32 linux_abi64_getdents64_entry_installed(void);
u32 linux_abi64_futex_entry_installed(void);
u32 linux_abi64_clone_entry_installed(void);
u32 linux_abi64_execve_entry_installed(void);
u32 linux_abi64_execveat_entry_installed(void);
u32 linux_abi64_wait4_entry_installed(void);
u32 linux_abi64_kill_entry_installed(void);
u32 linux_abi64_tkill_entry_installed(void);
u32 linux_abi64_getrandom_entry_installed(void);
u32 linux_abi64_getpid_entry_installed(void);
u32 linux_abi64_gettid_entry_installed(void);
u32 linux_abi64_arch_prctl_entry_installed(void);
u32 linux_abi64_set_tid_address_entry_installed(void);
u32 linux_abi64_clock_gettime_entry_installed(void);
u32 linux_abi64_exit_entry_installed(void);
u32 linux_abi64_exit_group_entry_installed(void);
u32 linux_abi64_openat_entry_installed(void);
u32 linux_abi64_dispatch_count(void);
u32 linux_abi64_unimplemented_count(void);
u32 linux_abi64_unimplemented_last_syscall(void);
u64 linux_abi64_unimplemented_last_rip(void);
u32 linux_abi64_read_count(void);
u32 linux_abi64_read_byte_count(void);
u32 linux_abi64_read_denial_count(void);
u32 linux_abi64_read_fault_count(void);
u32 linux_abi64_write_count(void);
u32 linux_abi64_write_byte_count(void);
u32 linux_abi64_write_denial_count(void);
u32 linux_abi64_write_fault_count(void);
u32 linux_abi64_pread64_count(void);
u32 linux_abi64_pwrite64_count(void);
u32 linux_abi64_pread64_byte_count(void);
u32 linux_abi64_pwrite64_byte_count(void);
u32 linux_abi64_positioned_denial_count(void);
u32 linux_abi64_positioned_fault_count(void);
u32 linux_abi64_positioned_last_syscall(void);
u32 linux_abi64_positioned_last_fd(void);
u32 linux_abi64_positioned_last_byte_count(void);
u64 linux_abi64_positioned_last_offset(void);
u32 linux_abi64_positioned_last_result(void);
u32 linux_abi64_readv_count(void);
u32 linux_abi64_writev_count(void);
u32 linux_abi64_readv_byte_count(void);
u32 linux_abi64_writev_byte_count(void);
u32 linux_abi64_vector_denial_count(void);
u32 linux_abi64_vector_fault_count(void);
u32 linux_abi64_vector_last_syscall(void);
u32 linux_abi64_vector_last_fd(void);
u32 linux_abi64_vector_last_iov_count(void);
u32 linux_abi64_vector_last_byte_count(void);
u32 linux_abi64_vector_last_result(void);
u32 linux_abi64_poll_count(void);
u32 linux_abi64_ppoll_count(void);
u32 linux_abi64_poll_ready_count(void);
u32 linux_abi64_poll_denial_count(void);
u32 linux_abi64_poll_fault_count(void);
u32 linux_abi64_poll_last_syscall(void);
u32 linux_abi64_poll_last_fd_count(void);
u32 linux_abi64_poll_last_ready(void);
u32 linux_abi64_poll_last_revents(void);
u32 linux_abi64_poll_last_result(void);
u32 linux_abi64_open_count(void);
u32 linux_abi64_openat_count(void);
u32 linux_abi64_open_denial_count(void);
u32 linux_abi64_close_count(void);
u32 linux_abi64_close_denial_count(void);
u32 linux_abi64_lseek_count(void);
u32 linux_abi64_lseek_denial_count(void);
u32 linux_abi64_stat_count(void);
u32 linux_abi64_stat_denial_count(void);
u32 linux_abi64_stat_fault_count(void);
u32 linux_abi64_fstat_count(void);
u32 linux_abi64_fstat_denial_count(void);
u32 linux_abi64_fstat_fault_count(void);
u32 linux_abi64_newfstatat_count(void);
u32 linux_abi64_newfstatat_denial_count(void);
u32 linux_abi64_newfstatat_fault_count(void);
u32 linux_abi64_readlink_count(void);
u32 linux_abi64_readlink_byte_count(void);
u32 linux_abi64_readlink_denial_count(void);
u32 linux_abi64_readlink_fault_count(void);
u32 linux_abi64_readlink_last_result(void);
u32 linux_abi64_mmap_count(void);
u32 linux_abi64_mmap_byte_count(void);
u32 linux_abi64_mmap_denial_count(void);
u32 linux_abi64_mprotect_count(void);
u32 linux_abi64_mprotect_byte_count(void);
u32 linux_abi64_mprotect_denial_count(void);
u32 linux_abi64_munmap_count(void);
u32 linux_abi64_munmap_byte_count(void);
u32 linux_abi64_munmap_denial_count(void);
u32 linux_abi64_brk_query_count(void);
u32 linux_abi64_brk_extend_count(void);
u32 linux_abi64_brk_denial_count(void);
u32 linux_abi64_rt_sigaction_count(void);
u32 linux_abi64_rt_sigaction_query_count(void);
u32 linux_abi64_rt_sigaction_denial_count(void);
u32 linux_abi64_rt_sigaction_fault_count(void);
u32 linux_abi64_rt_sigaction_last_signal(void);
u64 linux_abi64_rt_sigaction_last_handler(void);
u64 linux_abi64_rt_sigaction_last_old_handler(void);
u64 linux_abi64_rt_sigaction_last_mask(void);
u64 linux_abi64_rt_sigaction_last_flags(void);
u32 linux_abi64_rt_sigaction_last_result(void);
u32 linux_abi64_rt_sigprocmask_count(void);
u32 linux_abi64_rt_sigprocmask_query_count(void);
u32 linux_abi64_rt_sigprocmask_denial_count(void);
u32 linux_abi64_rt_sigprocmask_fault_count(void);
u32 linux_abi64_rt_sigprocmask_last_how(void);
u64 linux_abi64_rt_sigprocmask_last_set(void);
u64 linux_abi64_rt_sigprocmask_last_old_mask(void);
u64 linux_abi64_rt_sigprocmask_last_mask(void);
u32 linux_abi64_rt_sigprocmask_last_result(void);
u32 linux_abi64_nanosleep_count(void);
u32 linux_abi64_nanosleep_denial_count(void);
u32 linux_abi64_nanosleep_fault_count(void);
u32 linux_abi64_nanosleep_interrupted_count(void);
u32 linux_abi64_getrlimit_count(void);
u32 linux_abi64_setrlimit_count(void);
u32 linux_abi64_rlimit_denial_count(void);
u32 linux_abi64_rlimit_fault_count(void);
u32 linux_abi64_pipe2_count(void);
u32 linux_abi64_pipe2_denial_count(void);
u32 linux_abi64_pipe2_fault_count(void);
u32 linux_abi64_dup_count(void);
u32 linux_abi64_dup2_count(void);
u32 linux_abi64_dup3_count(void);
u32 linux_abi64_dup_denial_count(void);
u32 linux_abi64_fcntl_count(void);
u32 linux_abi64_fcntl_denial_count(void);
u32 linux_abi64_getcwd_count(void);
u32 linux_abi64_getcwd_byte_count(void);
u32 linux_abi64_getcwd_denial_count(void);
u32 linux_abi64_getcwd_fault_count(void);
u32 linux_abi64_path_relative_count(void);
u32 linux_abi64_path_dot_count(void);
u32 linux_abi64_path_dotdot_count(void);
u32 linux_abi64_path_fault_count(void);
u32 linux_abi64_chdir_count(void);
u32 linux_abi64_fchdir_count(void);
u32 linux_abi64_chdir_denial_count(void);
u32 linux_abi64_chdir_fault_count(void);
u32 linux_abi64_getdents64_count(void);
u32 linux_abi64_getdents64_entry_count(void);
u32 linux_abi64_getdents64_byte_count(void);
u32 linux_abi64_getdents64_denial_count(void);
u32 linux_abi64_getdents64_fault_count(void);
u32 linux_abi64_futex_wait_count(void);
u32 linux_abi64_futex_wake_count(void);
u32 linux_abi64_futex_woken_count(void);
u32 linux_abi64_futex_waiter_count(void);
u32 linux_abi64_futex_eagain_count(void);
u32 linux_abi64_futex_denial_count(void);
u32 linux_abi64_futex_fault_count(void);
u32 linux_abi64_futex_timed_wait_count(void);
u32 linux_abi64_futex_timeout_count(void);
u32 linux_abi64_futex_last_wait_pid(void);
u64 linux_abi64_futex_last_wait_address(void);
u32 linux_abi64_futex_last_wait_value(void);
u32 linux_abi64_futex_last_wait_task_id(void);
u32 linux_abi64_futex_last_wake_count(void);
u32 linux_abi64_futex_last_timeout_task_id(void);
u32 linux_abi64_futex_last_timeout_ticks(void);
u32 linux_abi64_futex_last_timeout_result(void);
u32 linux_abi64_clone_count(void);
u32 linux_abi64_clone_thread_count(void);
u32 linux_abi64_clone_denial_count(void);
u32 linux_abi64_clone_fork_denial_count(void);
u32 linux_abi64_clone_scheduler_count(void);
u32 linux_abi64_fork_count(void);
u32 linux_abi64_fork_enosys_count(void);
u32 linux_abi64_fork_denial_count(void);
u64 linux_abi64_fork_last_rip(void);
u32 linux_abi64_clone_last_parent_pid(void);
u32 linux_abi64_clone_last_child_pid(void);
u32 linux_abi64_clone_last_flags(void);
u32 linux_abi64_clone_last_task_id(void);
u32 linux_abi64_clone_last_shared_vma(void);
u32 linux_abi64_clone_last_shared_fd(void);
u32 linux_abi64_clone_last_shared_audit(void);
u64 linux_abi64_clone_last_child_stack(void);
u64 linux_abi64_clone_last_tls_base(void);
u32 linux_abi64_release_clone(u32 child_pid);
u32 linux_abi64_execve_count(void);
u32 linux_abi64_execveat_count(void);
u32 linux_abi64_execve_denial_count(void);
u32 linux_abi64_execve_fault_count(void);
u32 linux_abi64_execve_last_error(void);
u32 linux_abi64_execve_last_path_checksum(void);
u32 linux_abi64_execve_last_binary_bytes(void);
u32 linux_abi64_execve_last_closed_fds(void);
u32 linux_abi64_execve_last_fd_live_before(void);
u32 linux_abi64_execve_last_fd_live_after(void);
u32 linux_abi64_execve_last_vma_before(void);
u32 linux_abi64_execve_last_vma_released(void);
u32 linux_abi64_execve_last_vma_after(void);
u32 linux_abi64_execve_last_argc(void);
u32 linux_abi64_execve_last_envc(void);
u32 linux_abi64_execve_last_transfer_ready(void);
u64 linux_abi64_execve_last_transfer_rip(void);
u64 linux_abi64_execve_last_transfer_rsp(void);
u32 linux_abi64_execve_last_entry_prot(void);
u32 linux_abi64_execve_last_stack_prot(void);
u32 linux_abi64_wait4_count(void);
u32 linux_abi64_wait4_reap_count(void);
u32 linux_abi64_wait4_nohang_count(void);
u32 linux_abi64_wait4_denial_count(void);
u32 linux_abi64_wait4_fault_count(void);
u32 linux_abi64_wait4_last_parent_pid(void);
u32 linux_abi64_wait4_last_child_pid(void);
u32 linux_abi64_wait4_last_exit_code(void);
u32 linux_abi64_wait4_last_status(void);
u32 linux_abi64_wait4_last_status_written(void);
u32 linux_abi64_wait4_last_options(void);
u32 linux_abi64_wait4_last_process_release(void);
u32 linux_abi64_wait4_last_clone_release(void);
u32 linux_abi64_kill_count(void);
u32 linux_abi64_tkill_count(void);
u32 linux_abi64_kill_unavailable_count(void);
u32 linux_abi64_kill_denial_count(void);
u32 linux_abi64_kill_last_syscall(void);
u32 linux_abi64_kill_last_target(void);
u32 linux_abi64_kill_last_signal(void);
u32 linux_abi64_kill_last_result(void);
u32 linux_abi64_signal_pending_count(void);
u32 linux_abi64_signal_delivery_count(void);
u32 linux_abi64_signal_masked_count(void);
u32 linux_abi64_signal_delivery_denial_count(void);
u32 linux_abi64_signal_delivery_fault_count(void);
u32 linux_abi64_signal_delivery_last_signal(void);
u64 linux_abi64_signal_delivery_last_handler(void);
u64 linux_abi64_signal_delivery_last_frame(void);
u64 linux_abi64_signal_delivery_last_saved_rip(void);
u64 linux_abi64_signal_delivery_last_saved_rsp(void);
u64 linux_abi64_signal_delivery_last_mask(void);
u32 linux_abi64_signal_delivery_last_result(void);
u32 linux_abi64_rt_sigreturn_count(void);
u32 linux_abi64_rt_sigreturn_denial_count(void);
u32 linux_abi64_rt_sigreturn_fault_count(void);
u64 linux_abi64_rt_sigreturn_last_frame(void);
u64 linux_abi64_rt_sigreturn_last_rip(void);
u64 linux_abi64_rt_sigreturn_last_rsp(void);
u64 linux_abi64_rt_sigreturn_last_mask(void);
u64 linux_abi64_rt_sigreturn_last_rax(void);
u32 linux_abi64_rt_sigreturn_last_result(void);
u32 linux_abi64_getrandom_count(void);
u32 linux_abi64_getrandom_byte_count(void);
u32 linux_abi64_getrandom_denial_count(void);
u32 linux_abi64_getrandom_fault_count(void);
u32 linux_abi64_getrandom_last_byte_count(void);
u32 linux_abi64_getrandom_last_checksum(void);
u32 linux_abi64_getrandom_last_flags(void);
u32 linux_abi64_getrandom_last_result(void);
u32 linux_abi64_getpid_count(void);
u32 linux_abi64_getpid_denial_count(void);
u32 linux_abi64_geteuid_count(void);
u32 linux_abi64_geteuid_denial_count(void);
u32 linux_abi64_getppid_count(void);
u32 linux_abi64_getppid_denial_count(void);
u32 linux_abi64_gettid_count(void);
u32 linux_abi64_gettid_denial_count(void);
u32 linux_abi64_ioctl_count(void);
u32 linux_abi64_ioctl_tty_count(void);
u32 linux_abi64_ioctl_enotty_count(void);
u32 linux_abi64_ioctl_enosys_count(void);
u32 linux_abi64_ioctl_denial_count(void);
u32 linux_abi64_ioctl_last_fd(void);
u32 linux_abi64_ioctl_last_request(void);
u32 linux_abi64_ioctl_last_result(void);
u32 linux_abi64_prctl_count(void);
u32 linux_abi64_prctl_set_name_count(void);
u32 linux_abi64_prctl_get_name_count(void);
u32 linux_abi64_prctl_enosys_count(void);
u32 linux_abi64_prctl_denial_count(void);
u32 linux_abi64_prctl_fault_count(void);
u32 linux_abi64_prctl_last_option(void);
u32 linux_abi64_prctl_last_result(void);
u32 linux_abi64_arch_prctl_count(void);
u32 linux_abi64_arch_prctl_set_count(void);
u32 linux_abi64_arch_prctl_get_count(void);
u32 linux_abi64_arch_prctl_denial_count(void);
u32 linux_abi64_arch_prctl_fault_count(void);
u32 linux_abi64_set_tid_address_count(void);
u32 linux_abi64_set_tid_address_denial_count(void);
u32 linux_abi64_clock_gettime_count(void);
u32 linux_abi64_clock_gettime_denial_count(void);
u32 linux_abi64_clock_gettime_fault_count(void);
u32 linux_abi64_exit_count(void);
u32 linux_abi64_exit_group_count(void);
u32 linux_abi64_exit_denial_count(void);
u32 linux_abi64_process_exited(u32 pid);
u32 linux_abi64_exit_code(u32 pid);
u32 linux_abi64_last_exit_pid(void);
u32 linux_abi64_last_exit_code(void);
u32 linux_abi64_last_exit_vma_regions(void);
u32 linux_abi64_last_exit_fd_entries(void);
u32 linux_abi64_last_exit_persona_released(void);
u32 linux_abi64_last_exit_audit_released(void);
u32 linux_abi64_last_exit_audit_recorded(void);

#endif
