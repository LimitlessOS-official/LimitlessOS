#ifndef LIMITLESS_LINUX_LIBC_X64_H
#define LIMITLESS_LINUX_LIBC_X64_H

#include "elf64_x64.h"
#include "types.h"

#define LINUX_LIBC64_DEFAULT_BASE 0x0000000047810000ull
#define LINUX_LIBC64_IMAGE_BYTES 0x00003000u
#define LINUX_LIBC64_FILE_BYTES 0x00001000u
#define LINUX_LIBC64_TEXT_RVA 0x00001000u
#define LINUX_LIBC64_RODATA_RVA 0x00001D00u
#define LINUX_LIBC64_DYNAMIC_RVA 0x00001D80u
#define LINUX_LIBC64_DATA_RVA 0x00002000u
#define LINUX_LIBC64_ERRNO_RVA LINUX_LIBC64_DATA_RVA
#define LINUX_LIBC64_ERRNO_BYTES 4u
#define LINUX_LIBC64_RVA_ENV_VECTOR 0x00002020u
#define LINUX_LIBC64_RVA_ENV_STRING 0x000020D0u
#define LINUX_LIBC64_ENV_SNAPSHOT_BYTES 0x00000100u
#define LINUX_LIBC64_ENV_SNAPSHOT_COUNT 4u
#define LINUX_LIBC64_RVA_PTHREAD_TLS_ALLOC 0x00002080u
#define LINUX_LIBC64_RVA_PTHREAD_TLS_ENTRIES 0x00002090u
#define LINUX_LIBC64_PTHREAD_TLS_ENTRY_COUNT 4u
#define LINUX_LIBC64_PTHREAD_TLS_ENTRY_BYTES 16u
#define LINUX_LIBC64_RVA_PTHREAD_CLEAR_TID_WORDS 0x000021E0u
#define LINUX_LIBC64_PTHREAD_CLEAR_TID_COUNT 16u
#define LINUX_LIBC64_TEXT_FILE_OFFSET 0x00000200u
#define LINUX_LIBC64_RODATA_FILE_OFFSET 0x00000F00u
#define LINUX_LIBC64_DYNAMIC_FILE_OFFSET 0x00000F80u
#define LINUX_LIBC64_TEXT_FILE_BYTES 0x00000D00u
#define LINUX_LIBC64_RODATA_FILE_BYTES 0x00000080u
#define LINUX_LIBC64_DYNAMIC_BYTES 0x00000040u
#define LINUX_LIBC64_SYMBOL_COUNT 65u
#define LINUX_LIBC64_SYSCALL_SYMBOL_COUNT 31u
#define LINUX_LIBC64_MEMORY_SYMBOL_COUNT 3u
#define LINUX_LIBC64_STRING_SYMBOL_COUNT 5u
#define LINUX_LIBC64_STDIO_SYMBOL_COUNT 4u
#define LINUX_LIBC64_HEAP_SYMBOL_COUNT 4u
#define LINUX_LIBC64_ABORT_SYMBOL_COUNT 1u
#define LINUX_LIBC64_ENV_SYMBOL_COUNT 2u
#define LINUX_LIBC64_ERRNO_SYMBOL_COUNT 1u
#define LINUX_LIBC64_PTHREAD_CREATE_SYMBOL_COUNT 1u
#define LINUX_LIBC64_PTHREAD_JOIN_SYMBOL_COUNT 1u
#define LINUX_LIBC64_PTHREAD_EXIT_SYMBOL_COUNT 1u
#define LINUX_LIBC64_PTHREAD_MUTEX_SYMBOL_COUNT 2u
#define LINUX_LIBC64_PTHREAD_COND_SYMBOL_COUNT 5u
#define LINUX_LIBC64_PTHREAD_TLS_SYMBOL_COUNT 3u
#define LINUX_LIBC64_STARTUP_SYMBOL_COUNT 1u
#define LINUX_LIBC64_PTHREAD_DEPENDENCY_ALIAS_COUNT 2u
#define LINUX_LIBC64_UNAVAILABLE_SYMBOL_COUNT \
    (LINUX_LIBC64_SYMBOL_COUNT \
        - LINUX_LIBC64_SYSCALL_SYMBOL_COUNT \
        - LINUX_LIBC64_MEMORY_SYMBOL_COUNT \
        - LINUX_LIBC64_STRING_SYMBOL_COUNT \
        - LINUX_LIBC64_STDIO_SYMBOL_COUNT \
        - LINUX_LIBC64_HEAP_SYMBOL_COUNT \
        - LINUX_LIBC64_ABORT_SYMBOL_COUNT \
        - LINUX_LIBC64_ENV_SYMBOL_COUNT \
        - LINUX_LIBC64_ERRNO_SYMBOL_COUNT \
        - LINUX_LIBC64_PTHREAD_CREATE_SYMBOL_COUNT \
        - LINUX_LIBC64_PTHREAD_JOIN_SYMBOL_COUNT \
        - LINUX_LIBC64_PTHREAD_EXIT_SYMBOL_COUNT \
        - LINUX_LIBC64_PTHREAD_MUTEX_SYMBOL_COUNT \
        - LINUX_LIBC64_PTHREAD_COND_SYMBOL_COUNT \
        - LINUX_LIBC64_PTHREAD_TLS_SYMBOL_COUNT \
        - LINUX_LIBC64_STARTUP_SYMBOL_COUNT)
#define LINUX_LIBC64_STRING_LIMIT 128u
#define LINUX_LIBC64_ABORT_EXIT_CODE 134u

#define LINUX_LIBC64_RVA_READ 0x00001000u
#define LINUX_LIBC64_RVA_PTHREAD_COND_INIT 0x00001008u
#define LINUX_LIBC64_RVA_PTHREAD_COND_DESTROY LINUX_LIBC64_RVA_PTHREAD_COND_INIT
#define LINUX_LIBC64_RVA_WRITE 0x00001020u
#define LINUX_LIBC64_RVA_OPEN 0x00001040u
#define LINUX_LIBC64_RVA_CLOSE 0x00001060u
#define LINUX_LIBC64_RVA_LSEEK 0x00001080u
#define LINUX_LIBC64_RVA_STAT 0x000010A0u
#define LINUX_LIBC64_RVA_FSTAT 0x000010C0u
#define LINUX_LIBC64_RVA_MMAP 0x000010E0u
#define LINUX_LIBC64_RVA_MUNMAP 0x00001100u
#define LINUX_LIBC64_RVA_MPROTECT 0x00001120u
#define LINUX_LIBC64_RVA_BRK 0x00001140u
#define LINUX_LIBC64_RVA_EXIT 0x00001160u
#define LINUX_LIBC64_RVA_GETPID 0x00001180u
#define LINUX_LIBC64_RVA_GETTID 0x000011A0u
#define LINUX_LIBC64_RVA_GETRANDOM 0x000011C0u
#define LINUX_LIBC64_RVA_FUTEX 0x000011E0u
#define LINUX_LIBC64_RVA_FCNTL 0x000011E8u
#define LINUX_LIBC64_RVA_GETDENTS64 0x000011F0u
#define LINUX_LIBC64_RVA_FCHDIR 0x000011F8u
#define LINUX_LIBC64_RVA_MEMCPY 0x00001200u
#define LINUX_LIBC64_RVA_MEMSET 0x00001220u
#define LINUX_LIBC64_RVA_STRLEN 0x00001240u
#define LINUX_LIBC64_RVA_STRCPY 0x00001260u
#define LINUX_LIBC64_RVA_STRNCPY 0x00001280u
#define LINUX_LIBC64_RVA_STRCMP 0x000012A0u
#define LINUX_LIBC64_RVA_STRNCMP 0x000012C0u
#define LINUX_LIBC64_RVA_MEMMOVE 0x000012E0u
#define LINUX_LIBC64_RVA_PRINTF 0x00001300u
#define LINUX_LIBC64_RVA_PUTS 0x00001320u
#define LINUX_LIBC64_RVA_FPUTS 0x00001340u
#define LINUX_LIBC64_RVA_FWRITE 0x00001360u
#define LINUX_LIBC64_RVA_MALLOC 0x00001380u
#define LINUX_LIBC64_RVA_FREE 0x000013A0u
#define LINUX_LIBC64_RVA_REALLOC 0x000013C0u
#define LINUX_LIBC64_RVA_CALLOC 0x000013E0u
#define LINUX_LIBC64_RVA_ABORT 0x00001400u
#define LINUX_LIBC64_RVA_GETCWD 0x00001410u
#define LINUX_LIBC64_RVA_GETENV 0x00001420u
#define LINUX_LIBC64_RVA_CHDIR 0x00001430u
#define LINUX_LIBC64_RVA_SETENV 0x00001440u
#define LINUX_LIBC64_RVA_READLINK 0x00001450u
#define LINUX_LIBC64_RVA_ERRNO_LOCATION 0x00001460u
#define LINUX_LIBC64_RVA_READV 0x00001468u
#define LINUX_LIBC64_RVA_WRITEV 0x00001470u
#define LINUX_LIBC64_RVA_POLL 0x00001478u
#define LINUX_LIBC64_RVA_PTHREAD_CREATE 0x00001480u
#define LINUX_LIBC64_RVA_PTHREAD_JOIN 0x000014A0u
#define LINUX_LIBC64_RVA_PTHREAD_MUTEX_LOCK 0x000014C0u
#define LINUX_LIBC64_RVA_PTHREAD_MUTEX_UNLOCK 0x000014E0u
#define LINUX_LIBC64_RVA_PTHREAD_EXIT 0x000014F0u
#define LINUX_LIBC64_RVA_PTHREAD_COND_WAIT 0x000014F8u
#define LINUX_LIBC64_RVA_PTHREAD_COND_SIGNAL 0x000019CDu
#define LINUX_LIBC64_RVA_PTHREAD_COND_BROADCAST 0x00001B15u
#define LINUX_LIBC64_RVA_PTHREAD_KEY_CREATE 0x00001A00u
#define LINUX_LIBC64_RVA_PTHREAD_SETSPECIFIC 0x00001A40u
#define LINUX_LIBC64_RVA_PTHREAD_GETSPECIFIC 0x00001AD0u
#define LINUX_LIBC64_RVA_LIBC_START_MAIN 0x00001BF0u
#define LINUX_LIBC64_RVA_NEWFSTATAT 0x00001D40u
#define LINUX_LIBC64_RVA_OPENAT 0x00001D50u
#define LINUX_LIBC64_RVA_DUP 0x00001D60u
#define LINUX_LIBC64_RVA_DUP2 0x00001D68u
#define LINUX_LIBC64_RVA_DUP3 0x00001D70u
#define LINUX_LIBC64_RVA_PIPE 0x00001D78u

#define LINUX_LIBC64_OK 1u
#define LINUX_LIBC64_DENIED 0u

#define LINUX_LIBC64_ERROR_NONE 0u
#define LINUX_LIBC64_ERROR_NULL 1u
#define LINUX_LIBC64_ERROR_PERSONA 2u
#define LINUX_LIBC64_ERROR_BASE 3u
#define LINUX_LIBC64_ERROR_HEADER 4u
#define LINUX_LIBC64_ERROR_PHDR 5u
#define LINUX_LIBC64_ERROR_MAP 6u
#define LINUX_LIBC64_ERROR_SYMBOL 7u
#define LINUX_LIBC64_ERROR_ALREADY_MAPPED 8u
#define LINUX_LIBC64_ERROR_ENVIRONMENT 9u

typedef struct linux_libc64_load_result
{
    elf64_header_t header;
    elf64_phdr_summary_t phdr_summary;
    elf64_load_result_t load_result;
    u64 image_base;
    u64 image_end;
    u64 read_fn;
    u64 write_fn;
    u64 open_fn;
    u64 close_fn;
    u64 exit_fn;
    u64 memcpy_fn;
    u64 memset_fn;
    u64 strlen_fn;
    u64 puts_fn;
    u64 printf_fn;
    u64 malloc_fn;
    u64 abort_fn;
    u64 getenv_fn;
    u64 setenv_fn;
    u64 errno_location_fn;
    u64 errno_cell;
    u64 pthread_create_fn;
    u64 pthread_join_fn;
    u64 pthread_exit_fn;
    u64 pthread_mutex_lock_fn;
    u64 pthread_mutex_unlock_fn;
    u64 pthread_cond_init_fn;
    u64 pthread_cond_destroy_fn;
    u64 pthread_cond_wait_fn;
    u64 pthread_cond_signal_fn;
    u64 pthread_cond_broadcast_fn;
    u64 pthread_key_create_fn;
    u64 pthread_setspecific_fn;
    u64 pthread_getspecific_fn;
    u32 file_bytes;
    u32 image_bytes;
    u32 symbol_count;
    u32 syscall_symbol_count;
    u32 memory_symbol_count;
    u32 string_symbol_count;
    u32 stdio_symbol_count;
    u32 heap_symbol_count;
    u32 abort_symbol_count;
    u32 env_symbol_count;
    u32 unavailable_symbol_count;
    u32 errno_symbol_count;
    u32 pthread_create_symbol_count;
    u32 pthread_join_symbol_count;
    u32 pthread_exit_symbol_count;
    u32 pthread_mutex_symbol_count;
    u32 pthread_cond_symbol_count;
    u32 pthread_tls_symbol_count;
    u32 errno_page_mapped;
    u32 image_checksum;
    u32 text_checksum;
    u32 rodata_checksum;
    u32 name_checksum;
    u32 text_protection;
    u32 context_stored;
    u32 error;
} linux_libc64_load_result_t;

void linux_libc64_init(void);
u32 linux_libc64_dependency_supported(const char *name, u32 length);
u32 linux_libc64_pthread_dependency_supported(const char *name, u32 length);
u32 linux_libc64_symbol_supported(const char *name, u32 length);
u32 linux_libc64_symbol_unavailable(const char *name, u32 length);
u32 linux_libc64_symbol_default_address(
    const char *name,
    u32 length,
    u64 *out_address,
    u32 *out_unavailable);
u32 linux_libc64_load(
    u32 pid,
    u64 image_base,
    linux_libc64_load_result_t *out_result);
u32 linux_libc64_bind_environment(
    u32 pid,
    u64 envp_address,
    u32 envc,
    const char *const *envp_source);
u64 linux_libc64_export(u32 pid, const char *name);
u32 linux_libc64_release_process(u32 pid);
const u8 *linux_libc64_image(void);
u32 linux_libc64_file_bytes(void);
u32 linux_libc64_symbol_count(void);
u32 linux_libc64_syscall_symbol_count(void);
u32 linux_libc64_memory_symbol_count(void);
u32 linux_libc64_string_symbol_count(void);
u32 linux_libc64_stdio_symbol_count(void);
u32 linux_libc64_heap_symbol_count(void);
u32 linux_libc64_abort_symbol_count(void);
u32 linux_libc64_env_symbol_count(void);
u32 linux_libc64_errno_symbol_count(void);
u32 linux_libc64_pthread_create_symbol_count(void);
u32 linux_libc64_pthread_join_symbol_count(void);
u32 linux_libc64_pthread_exit_symbol_count(void);
u32 linux_libc64_pthread_mutex_symbol_count(void);
u32 linux_libc64_pthread_cond_symbol_count(void);
u32 linux_libc64_pthread_tls_symbol_count(void);
u32 linux_libc64_unavailable_symbol_count(void);
u32 linux_libc64_dependency_supported_count(void);
u32 linux_libc64_load_count(void);
u32 linux_libc64_denial_count(void);
u32 linux_libc64_last_error(void);

#endif
