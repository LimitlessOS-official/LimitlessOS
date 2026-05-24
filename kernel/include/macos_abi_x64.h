#ifndef LIMITLESS_MACOS_ABI_X64_H
#define LIMITLESS_MACOS_ABI_X64_H

#include "types.h"

#define MACOS_ABI64_SYSCALL_LIMIT 512u
#define MACOS_ABI64_SYSCALL_EXIT 1u
#define MACOS_ABI64_SYSCALL_READ 3u
#define MACOS_ABI64_SYSCALL_WRITE 4u
#define MACOS_ABI64_SYSCALL_OPEN 5u
#define MACOS_ABI64_SYSCALL_CLOSE 6u
#define MACOS_ABI64_SYSCALL_GETPID 20u
#define MACOS_ABI64_SYSCALL_MUNMAP 73u
#define MACOS_ABI64_SYSCALL_MPROTECT 74u
#define MACOS_ABI64_SYSCALL_CLOCK_GETTIME 116u
#define MACOS_ABI64_SYSCALL_STAT 188u
#define MACOS_ABI64_SYSCALL_FSTAT 189u
#define MACOS_ABI64_SYSCALL_MMAP 197u
#define MACOS_ABI64_SYSCALL_SYSCTL 202u

#define MACOS_ABI64_BSD_CLASS_SHIFT 24u
#define MACOS_ABI64_BSD_CLASS_UNIX 2u
#define MACOS_ABI64_BSD_CLASS_MASK 0xFF000000u
#define MACOS_ABI64_BSD_NUMBER_MASK 0x00FFFFFFu

#define MACOS_ABI64_READ_CHUNK_BYTES 128u
#define MACOS_ABI64_WRITE_CHUNK_BYTES 256u
#define MACOS_ABI64_MAX_PATH_BYTES 128u
#define MACOS_ABI64_STAT_BYTES 144u
#define MACOS_ABI64_TIMESPEC_BYTES 16u

#define MACOS_ABI64_PROT_READ 0x00000001u
#define MACOS_ABI64_PROT_WRITE 0x00000002u
#define MACOS_ABI64_PROT_EXEC 0x00000004u
#define MACOS_ABI64_MAP_SHARED 0x00000001u
#define MACOS_ABI64_MAP_PRIVATE 0x00000002u
#define MACOS_ABI64_MAP_FIXED 0x00000010u
#define MACOS_ABI64_MAP_ANON 0x00001000u
#define MACOS_ABI64_MAP_ANONYMOUS MACOS_ABI64_MAP_ANON

#define MACOS_ABI64_O_RDONLY 0x00000000u
#define MACOS_ABI64_O_WRONLY 0x00000001u
#define MACOS_ABI64_O_RDWR 0x00000002u
#define MACOS_ABI64_O_ACCMODE 0x00000003u
#define MACOS_ABI64_O_NONBLOCK 0x00000004u
#define MACOS_ABI64_O_CREAT 0x00000200u
#define MACOS_ABI64_O_CLOEXEC 0x01000000u

#define MACOS_ABI64_CLOCK_REALTIME 0u
#define MACOS_ABI64_CLOCK_MONOTONIC_RAW 4u
#define MACOS_ABI64_CLOCK_MONOTONIC 6u

#define MACOS_ABI64_CTL_KERN 1u
#define MACOS_ABI64_KERN_OSTYPE 1u
#define MACOS_ABI64_CTL_HW 6u
#define MACOS_ABI64_HW_PAGESIZE 7u

#define MACOS_ABI64_EPERM 1u
#define MACOS_ABI64_ENOENT 2u
#define MACOS_ABI64_ESRCH 3u
#define MACOS_ABI64_EBADF 9u
#define MACOS_ABI64_ENOMEM 12u
#define MACOS_ABI64_EACCES 13u
#define MACOS_ABI64_EFAULT 14u
#define MACOS_ABI64_EINVAL 22u
#define MACOS_ABI64_EMFILE 24u
#define MACOS_ABI64_ENOSYS 78u
#define MACOS_ABI64_ERROR_RETURN(error_code) (0ull - (u64)(error_code))

typedef u64 (*macos_abi64_handler_t)(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip);

typedef struct macos_abi64_timespec
{
    u64 tv_sec;
    u64 tv_nsec;
} macos_abi64_timespec_t;

typedef struct macos_abi64_stat
{
    u32 st_dev;
    u16 st_mode;
    u16 st_nlink;
    u64 st_ino;
    u32 st_uid;
    u32 st_gid;
    u32 st_rdev;
    u32 st_pad0;
    macos_abi64_timespec_t st_atime;
    macos_abi64_timespec_t st_mtime;
    macos_abi64_timespec_t st_ctime;
    macos_abi64_timespec_t st_birthtime;
    u64 st_size;
    u64 st_blocks;
    u32 st_blksize;
    u32 st_flags;
    u32 st_gen;
    u32 st_lspare;
    u64 st_qspare[2];
} macos_abi64_stat_t;

void macos_abi64_init(void);
macos_abi64_handler_t *macos_abi64_dispatch_table(void);
u64 macos_abi64_dispatch(
    u32 pid,
    u32 syscall_number,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip);
u32 macos_abi64_table_size(void);
u32 macos_abi64_unimplemented_entry_count(void);
u32 macos_abi64_entry_installed(u32 syscall_number);
u32 macos_abi64_read_entry_installed(void);
u32 macos_abi64_write_entry_installed(void);
u32 macos_abi64_open_entry_installed(void);
u32 macos_abi64_close_entry_installed(void);
u32 macos_abi64_stat_entry_installed(void);
u32 macos_abi64_fstat_entry_installed(void);
u32 macos_abi64_mmap_entry_installed(void);
u32 macos_abi64_munmap_entry_installed(void);
u32 macos_abi64_mprotect_entry_installed(void);
u32 macos_abi64_exit_entry_installed(void);
u32 macos_abi64_getpid_entry_installed(void);
u32 macos_abi64_clock_gettime_entry_installed(void);
u32 macos_abi64_sysctl_entry_installed(void);
u32 macos_abi64_dispatch_count(void);
u32 macos_abi64_unimplemented_count(void);
u32 macos_abi64_read_count(void);
u32 macos_abi64_read_byte_count(void);
u32 macos_abi64_write_count(void);
u32 macos_abi64_write_byte_count(void);
u32 macos_abi64_open_count(void);
u32 macos_abi64_close_count(void);
u32 macos_abi64_stat_count(void);
u32 macos_abi64_fstat_count(void);
u32 macos_abi64_mmap_count(void);
u32 macos_abi64_munmap_count(void);
u32 macos_abi64_mprotect_count(void);
u32 macos_abi64_exit_count(void);
u32 macos_abi64_getpid_count(void);
u32 macos_abi64_clock_gettime_count(void);
u32 macos_abi64_sysctl_count(void);
u32 macos_abi64_denial_count(void);
u32 macos_abi64_fault_count(void);
u32 macos_abi64_last_syscall(void);
u32 macos_abi64_last_result(void);
u32 macos_abi64_last_fd(void);
u32 macos_abi64_last_byte_count(void);
u64 macos_abi64_last_address(void);
u32 macos_abi64_last_sysctl_name0(void);
u32 macos_abi64_last_sysctl_name1(void);
u32 macos_abi64_last_sysctl_bytes(void);
u32 macos_abi64_last_exit_vma_regions(void);
u32 macos_abi64_last_exit_fd_entries(void);
u32 macos_abi64_last_exit_persona_released(void);
u32 macos_abi64_last_exit_audit_released(void);

#endif
