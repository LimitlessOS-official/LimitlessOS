#include "linux_libc_x64.h"

#include "linux_abi_x64.h"
#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * P.2 adds the first LimitlessOS-owned Linux libc shim substrate. It provides
 * a compact in-tree ET_DYN libc.so image, a bounded symbol registry, direct
 * syscall-backed user-mode stubs for the kernel ABI calls that already exist,
 * small byte-string helpers for early runtime code, minimal fd-backed puts,
 * fputs, fwrite and literal-only printf helpers, one-page malloc/calloc/realloc/free
 * helpers backed by mmap and munmap, getenv/setenv helpers bound to a bounded
 * user-visible environment snapshot, a process-local writable errno cell
 * returned by __errno_location, a bounded abort shim that issues exit_group(134)
 * through the Linux ABI, a native pthread_create shim that allocates a stack
 * with mmap, registers a real clone-backed thread task, and uses shim-owned
 * clear-tid words so the public pthread_t join handle remains stable, a bounded
 * pthread_join(thread, NULL) helper that reaps exited clone-backed children
 * through wait4, a bounded pthread_exit(NULL) path backed by sys_exit,
 * futex-backed single-word pthread_mutex lock/unlock helpers, futex-wake-backed
 * pthread_cond_init/destroy helpers for the shim's single-word condition
 * variable representation, futex-wake-backed pthread_cond_signal and
 * pthread_cond_broadcast helpers, a futex-blocking pthread_cond_wait helper
 * that unlocks and relocks the mutex around the wait, bounded
 * pthread_key_create/setspecific/getspecific helpers backed by a four-entry
 * per-TID value table, and libpthread.so / libpthread.so.0 dependency aliases
 * backed by the same bounded shim image.
 * It integrates with elf64_x64.c for mapping, persona_x64.c for per-process
 * shim state, and persona_audit_x64.c for truthful denial records.
 * The checkpoint proves libc.so dependency admission, image mapping, export
 * lookup, unavailable symbol accounting, bad-base denial, and cleanup without
 * claiming that a full dynamic musl/glibc program can run before the remaining
 * P.2 runtime work.
 */

#define LINUX_LIBC64_KIND_SYSCALL 1u
#define LINUX_LIBC64_KIND_MEMORY 2u
#define LINUX_LIBC64_KIND_UNAVAILABLE 3u
#define LINUX_LIBC64_KIND_STRING 4u
#define LINUX_LIBC64_KIND_STDIO 5u
#define LINUX_LIBC64_KIND_HEAP 6u
#define LINUX_LIBC64_KIND_ENV 7u
#define LINUX_LIBC64_KIND_ERRNO 8u
#define LINUX_LIBC64_KIND_ABORT 9u
#define LINUX_LIBC64_KIND_PTHREAD_MUTEX 10u
#define LINUX_LIBC64_KIND_PTHREAD_CREATE 11u
#define LINUX_LIBC64_KIND_PTHREAD_JOIN 12u
#define LINUX_LIBC64_KIND_PTHREAD_EXIT 13u
#define LINUX_LIBC64_KIND_PTHREAD_COND 14u
#define LINUX_LIBC64_KIND_PTHREAD_COND_WAIT 15u
#define LINUX_LIBC64_KIND_PTHREAD_COND_INIT 16u
#define LINUX_LIBC64_KIND_PTHREAD_TLS 17u
#define LINUX_LIBC64_KIND_STARTUP 18u

#define LINUX_LIBC64_RVA_HELPER_STRNCPY 0x00001500u
#define LINUX_LIBC64_RVA_HELPER_STRCMP 0x00001530u
#define LINUX_LIBC64_RVA_HELPER_STRNCMP 0x00001550u
#define LINUX_LIBC64_RVA_HELPER_MEMMOVE 0x00001580u
#define LINUX_LIBC64_RVA_HELPER_PUTS 0x000015B0u
#define LINUX_LIBC64_RVA_HELPER_PRINTF 0x00001600u
#define LINUX_LIBC64_RVA_HELPER_REALLOC 0x00001640u
#define LINUX_LIBC64_RVA_HELPER_GETENV 0x000016A0u
#define LINUX_LIBC64_RVA_HELPER_SETENV 0x00001700u
#define LINUX_LIBC64_RVA_HELPER_PTHREAD_MUTEX_LOCK 0x000017C0u
#define LINUX_LIBC64_RVA_HELPER_PTHREAD_MUTEX_UNLOCK 0x00001820u
#define LINUX_LIBC64_RVA_HELPER_PTHREAD_CREATE 0x00001880u
#define LINUX_LIBC64_RVA_HELPER_PTHREAD_JOIN 0x0000199Eu
#define LINUX_LIBC64_RVA_HELPER_PTHREAD_COND_WAIT 0x00001B47u
#define LINUX_LIBC64_GETENV_ENVP_IMM_OFFSET 2u
#define LINUX_LIBC64_GETENV_ENVC_IMM_OFFSET 12u
#define LINUX_LIBC64_SETENV_ENVP_IMM_OFFSET 2u
#define LINUX_LIBC64_SETENV_ENVC_IMM_OFFSET 12u
#define LINUX_LIBC64_RVA_HELPER_MALLOC LINUX_LIBC64_RVA_MALLOC
#define LINUX_LIBC64_RVA_HELPER_FREE LINUX_LIBC64_RVA_FREE
#define LINUX_LIBC64_RVA_HELPER_CALLOC LINUX_LIBC64_RVA_CALLOC

typedef struct linux_libc64_export
{
    const char *name;
    u32 length;
    u32 rva;
    u32 kind;
    u32 syscall_number;
} linux_libc64_export_t;

static const linux_libc64_export_t g_linux_libc64_exports[LINUX_LIBC64_SYMBOL_COUNT] = {
    { "read", 4u, LINUX_LIBC64_RVA_READ, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_READ },
    { "write", 5u, LINUX_LIBC64_RVA_WRITE, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_WRITE },
    { "open", 4u, LINUX_LIBC64_RVA_OPEN, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_OPEN },
    { "close", 5u, LINUX_LIBC64_RVA_CLOSE, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_CLOSE },
    { "lseek", 5u, LINUX_LIBC64_RVA_LSEEK, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_LSEEK },
    { "stat", 4u, LINUX_LIBC64_RVA_STAT, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_STAT },
    { "fstat", 5u, LINUX_LIBC64_RVA_FSTAT, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_FSTAT },
    { "mmap", 4u, LINUX_LIBC64_RVA_MMAP, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_MMAP },
    { "munmap", 6u, LINUX_LIBC64_RVA_MUNMAP, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_MUNMAP },
    { "mprotect", 8u, LINUX_LIBC64_RVA_MPROTECT, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_MPROTECT },
    { "brk", 3u, LINUX_LIBC64_RVA_BRK, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_BRK },
    { "exit", 4u, LINUX_LIBC64_RVA_EXIT, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_EXIT },
    { "getpid", 6u, LINUX_LIBC64_RVA_GETPID, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_GETPID },
    { "gettid", 6u, LINUX_LIBC64_RVA_GETTID, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_GETTID },
    { "getrandom", 9u, LINUX_LIBC64_RVA_GETRANDOM, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_GETRANDOM },
    { "futex", 5u, LINUX_LIBC64_RVA_FUTEX, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_FUTEX },
    { "getdents64", 10u, LINUX_LIBC64_RVA_GETDENTS64, LINUX_LIBC64_KIND_SYSCALL, LINUX_ABI64_SYSCALL_GETDENTS64 },
    { "memcpy", 6u, LINUX_LIBC64_RVA_MEMCPY, LINUX_LIBC64_KIND_MEMORY, 0u },
    { "memset", 6u, LINUX_LIBC64_RVA_MEMSET, LINUX_LIBC64_KIND_MEMORY, 0u },
    { "strlen", 6u, LINUX_LIBC64_RVA_STRLEN, LINUX_LIBC64_KIND_MEMORY, 0u },
    { "strcpy", 6u, LINUX_LIBC64_RVA_STRCPY, LINUX_LIBC64_KIND_STRING, 0u },
    { "strncpy", 7u, LINUX_LIBC64_RVA_STRNCPY, LINUX_LIBC64_KIND_STRING, 0u },
    { "strcmp", 6u, LINUX_LIBC64_RVA_STRCMP, LINUX_LIBC64_KIND_STRING, 0u },
    { "strncmp", 7u, LINUX_LIBC64_RVA_STRNCMP, LINUX_LIBC64_KIND_STRING, 0u },
    { "memmove", 7u, LINUX_LIBC64_RVA_MEMMOVE, LINUX_LIBC64_KIND_STRING, 0u },
    { "printf", 6u, LINUX_LIBC64_RVA_PRINTF, LINUX_LIBC64_KIND_STDIO, 0u },
    { "puts", 4u, LINUX_LIBC64_RVA_PUTS, LINUX_LIBC64_KIND_STDIO, 0u },
    { "fputs", 5u, LINUX_LIBC64_RVA_FPUTS, LINUX_LIBC64_KIND_STDIO, 0u },
    { "fwrite", 6u, LINUX_LIBC64_RVA_FWRITE, LINUX_LIBC64_KIND_STDIO, 0u },
    { "malloc", 6u, LINUX_LIBC64_RVA_MALLOC, LINUX_LIBC64_KIND_HEAP, 0u },
    { "free", 4u, LINUX_LIBC64_RVA_FREE, LINUX_LIBC64_KIND_HEAP, 0u },
    { "realloc", 7u, LINUX_LIBC64_RVA_REALLOC, LINUX_LIBC64_KIND_HEAP, 0u },
    { "calloc", 6u, LINUX_LIBC64_RVA_CALLOC, LINUX_LIBC64_KIND_HEAP, 0u },
    { "abort", 5u, LINUX_LIBC64_RVA_ABORT, LINUX_LIBC64_KIND_ABORT, 0u },
    { "getenv", 6u, LINUX_LIBC64_RVA_GETENV, LINUX_LIBC64_KIND_ENV, 0u },
    { "setenv", 6u, LINUX_LIBC64_RVA_SETENV, LINUX_LIBC64_KIND_ENV, 0u },
    { "__errno_location", 16u, LINUX_LIBC64_RVA_ERRNO_LOCATION, LINUX_LIBC64_KIND_ERRNO, 0u },
    { "pthread_create", 14u, LINUX_LIBC64_RVA_PTHREAD_CREATE, LINUX_LIBC64_KIND_PTHREAD_CREATE, 0u },
    { "pthread_join", 12u, LINUX_LIBC64_RVA_PTHREAD_JOIN, LINUX_LIBC64_KIND_PTHREAD_JOIN, 0u },
    { "pthread_mutex_lock", 18u, LINUX_LIBC64_RVA_PTHREAD_MUTEX_LOCK, LINUX_LIBC64_KIND_PTHREAD_MUTEX, 0u },
    { "pthread_mutex_unlock", 20u, LINUX_LIBC64_RVA_PTHREAD_MUTEX_UNLOCK, LINUX_LIBC64_KIND_PTHREAD_MUTEX, 0u },
    { "pthread_exit", 12u, LINUX_LIBC64_RVA_PTHREAD_EXIT, LINUX_LIBC64_KIND_PTHREAD_EXIT, LINUX_ABI64_SYSCALL_EXIT },
    { "pthread_cond_init", 17u, LINUX_LIBC64_RVA_PTHREAD_COND_INIT, LINUX_LIBC64_KIND_PTHREAD_COND_INIT, 0u },
    { "pthread_cond_destroy", 20u, LINUX_LIBC64_RVA_PTHREAD_COND_DESTROY, LINUX_LIBC64_KIND_PTHREAD_COND_INIT, 0u },
    { "pthread_cond_wait", 17u, LINUX_LIBC64_RVA_PTHREAD_COND_WAIT, LINUX_LIBC64_KIND_PTHREAD_COND_WAIT, 0u },
    { "pthread_cond_signal", 19u, LINUX_LIBC64_RVA_PTHREAD_COND_SIGNAL, LINUX_LIBC64_KIND_PTHREAD_COND, 0u },
    { "pthread_cond_broadcast", 22u, LINUX_LIBC64_RVA_PTHREAD_COND_BROADCAST, LINUX_LIBC64_KIND_PTHREAD_COND, 0u },
    { "pthread_key_create", 18u, LINUX_LIBC64_RVA_PTHREAD_KEY_CREATE, LINUX_LIBC64_KIND_PTHREAD_TLS, 0u },
    { "pthread_setspecific", 19u, LINUX_LIBC64_RVA_PTHREAD_SETSPECIFIC, LINUX_LIBC64_KIND_PTHREAD_TLS, 0u },
    { "pthread_getspecific", 19u, LINUX_LIBC64_RVA_PTHREAD_GETSPECIFIC, LINUX_LIBC64_KIND_PTHREAD_TLS, 0u },
    { "__libc_start_main", 17u, LINUX_LIBC64_RVA_LIBC_START_MAIN, LINUX_LIBC64_KIND_STARTUP, 0u }
};

static u8 g_linux_libc64_image[LINUX_LIBC64_FILE_BYTES];
static u32 g_linux_libc64_image_ready = 0u;
static u32 g_linux_libc64_dependency_supported_count = 0u;
static u32 g_linux_libc64_load_count = 0u;
static u32 g_linux_libc64_denial_count = 0u;
static u32 g_linux_libc64_last_error = LINUX_LIBC64_ERROR_NONE;

static void linux_libc64_write_le16(u8 *data, u32 offset, u16 value)
{
    data[offset] = (u8)(value & 0xFFu);
    data[offset + 1u] = (u8)((value >> 8) & 0xFFu);
}

static void linux_libc64_write_le32(u8 *data, u32 offset, u32 value)
{
    data[offset] = (u8)(value & 0xFFu);
    data[offset + 1u] = (u8)((value >> 8) & 0xFFu);
    data[offset + 2u] = (u8)((value >> 16) & 0xFFu);
    data[offset + 3u] = (u8)((value >> 24) & 0xFFu);
}

static void linux_libc64_write_le64(u8 *data, u32 offset, u64 value)
{
    linux_libc64_write_le32(data, offset, (u32)(value & 0xFFFFFFFFull));
    linux_libc64_write_le32(data, offset + 4u, (u32)(value >> 32));
}

static u32 linux_libc64_mix_checksum(u32 checksum, u8 value)
{
    checksum ^= (u32)value;
    checksum *= 16777619u;
    return checksum;
}

static u32 linux_libc64_checksum_bytes(const u8 *data, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (data == 0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = linux_libc64_mix_checksum(checksum, data[index]);
    }

    return checksum;
}

static u32 linux_libc64_name_matches(
    const char *left,
    u32 left_length,
    const char *right,
    u32 right_length)
{
    u32 index;

    if ((left == 0) || (right == 0) || (left_length != right_length))
    {
        return 0u;
    }

    for (index = 0u; index < left_length; ++index)
    {
        if (left[index] != right[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 linux_libc64_cstring_length(const char *text, u32 max_bytes)
{
    u32 index;

    if (text == 0)
    {
        return 0u;
    }

    for (index = 0u; index < max_bytes; ++index)
    {
        if (text[index] == (char)0)
        {
            return index;
        }
    }

    return max_bytes;
}

static void linux_libc64_copy_bytes(u8 *target, const u8 *source, u32 byte_count)
{
    u32 index;

    if ((target == 0) || (source == 0))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = source[index];
    }
}

static u32 linux_libc64_env_string_valid(const char *source, u32 max_bytes)
{
    u32 index;
    u32 saw_equals = 0u;

    if ((source == 0) || (max_bytes < 2u))
    {
        return 0u;
    }

    for (index = 0u; index < max_bytes; ++index)
    {
        if (source[index] == (char)'=')
        {
            saw_equals = 1u;
        }
        if (source[index] == (char)0)
        {
            return ((index != 0u) && (saw_equals != 0u)) ? 1u : 0u;
        }
    }

    return 0u;
}

static u32 linux_libc64_copy_env_string(
    volatile u8 *target,
    const char *source,
    u32 max_bytes)
{
    u32 index;
    u32 saw_equals = 0u;

    if ((target == 0) || (source == 0) || (max_bytes < 2u))
    {
        return 0u;
    }

    for (index = 0u; index < (max_bytes - 1u); ++index)
    {
        u8 value = (u8)source[index];
        target[index] = value;
        if (value == (u8)'=')
        {
            saw_equals = 1u;
        }
        if (value == 0u)
        {
            return ((index != 0u) && (saw_equals != 0u)) ? 1u : 0u;
        }
    }

    target[max_bytes - 1u] = 0u;
    return 0u;
}

static void linux_libc64_write_le32_volatile(volatile u8 *target, u32 value)
{
    if (target == 0)
    {
        return;
    }

    target[0] = (u8)(value & 0xFFu);
    target[1] = (u8)((value >> 8) & 0xFFu);
    target[2] = (u8)((value >> 16) & 0xFFu);
    target[3] = (u8)((value >> 24) & 0xFFu);
}

static void linux_libc64_write_le64_volatile(volatile u8 *target, u64 value)
{
    if (target == 0)
    {
        return;
    }

    linux_libc64_write_le32_volatile(target, (u32)(value & 0xFFFFFFFFull));
    linux_libc64_write_le32_volatile(target + 4u, (u32)(value >> 32));
}

static u32 linux_libc64_read_le32_volatile(const volatile u8 *source)
{
    if (source == 0)
    {
        return 0u;
    }

    return ((u32)source[0])
        | ((u32)source[1] << 8)
        | ((u32)source[2] << 16)
        | ((u32)source[3] << 24);
}

static u64 linux_libc64_read_le64_volatile(const volatile u8 *source)
{
    if (source == 0)
    {
        return 0ull;
    }

    return ((u64)linux_libc64_read_le32_volatile(source))
        | ((u64)linux_libc64_read_le32_volatile(source + 4u) << 32);
}

static void linux_libc64_write_phdr(
    u32 offset,
    u32 type,
    u32 flags,
    u64 file_offset,
    u64 vaddr,
    u64 filesz,
    u64 memsz,
    u64 align)
{
    linux_libc64_write_le32(g_linux_libc64_image, offset, type);
    linux_libc64_write_le32(g_linux_libc64_image, offset + 4u, flags);
    linux_libc64_write_le64(g_linux_libc64_image, offset + 8u, file_offset);
    linux_libc64_write_le64(g_linux_libc64_image, offset + 16u, vaddr);
    linux_libc64_write_le64(g_linux_libc64_image, offset + 24u, vaddr);
    linux_libc64_write_le64(g_linux_libc64_image, offset + 32u, filesz);
    linux_libc64_write_le64(g_linux_libc64_image, offset + 40u, memsz);
    linux_libc64_write_le64(g_linux_libc64_image, offset + 48u, align);
}

static u32 linux_libc64_rva_to_offset(u32 rva)
{
    return LINUX_LIBC64_TEXT_FILE_OFFSET + (rva - LINUX_LIBC64_TEXT_RVA);
}

static void linux_libc64_write_syscall_stub(u32 rva, u32 syscall_number)
{
    u32 offset = linux_libc64_rva_to_offset(rva);

    if ((rva < LINUX_LIBC64_TEXT_RVA)
        || ((offset + 8u) > (LINUX_LIBC64_TEXT_FILE_OFFSET + LINUX_LIBC64_TEXT_FILE_BYTES)))
    {
        return;
    }

    g_linux_libc64_image[offset] = 0xB8u;
    linux_libc64_write_le32(g_linux_libc64_image, offset + 1u, syscall_number);
    g_linux_libc64_image[offset + 5u] = 0x0Fu;
    g_linux_libc64_image[offset + 6u] = 0x05u;
    g_linux_libc64_image[offset + 7u] = 0xC3u;
}

static void linux_libc64_write_abort_stub(u32 rva)
{
    u32 offset = linux_libc64_rva_to_offset(rva);

    if ((rva < LINUX_LIBC64_TEXT_RVA)
        || ((offset + 13u) > (LINUX_LIBC64_TEXT_FILE_OFFSET + LINUX_LIBC64_TEXT_FILE_BYTES)))
    {
        return;
    }

    g_linux_libc64_image[offset] = 0xBFu;
    linux_libc64_write_le32(g_linux_libc64_image, offset + 1u, LINUX_LIBC64_ABORT_EXIT_CODE);
    g_linux_libc64_image[offset + 5u] = 0xB8u;
    linux_libc64_write_le32(
        g_linux_libc64_image,
        offset + 6u,
        LINUX_ABI64_SYSCALL_EXIT_GROUP);
    g_linux_libc64_image[offset + 10u] = 0x0Fu;
    g_linux_libc64_image[offset + 11u] = 0x05u;
    g_linux_libc64_image[offset + 12u] = 0xC3u;
}

static void linux_libc64_write_unavailable_stub(u32 rva)
{
    u32 offset = linux_libc64_rva_to_offset(rva);

    if ((rva < LINUX_LIBC64_TEXT_RVA)
        || ((offset + 8u) > (LINUX_LIBC64_TEXT_FILE_OFFSET + LINUX_LIBC64_TEXT_FILE_BYTES)))
    {
        return;
    }

    g_linux_libc64_image[offset] = 0x48u;
    g_linux_libc64_image[offset + 1u] = 0xC7u;
    g_linux_libc64_image[offset + 2u] = 0xC0u;
    g_linux_libc64_image[offset + 3u] = 0xDAu;
    g_linux_libc64_image[offset + 4u] = 0xFFu;
    g_linux_libc64_image[offset + 5u] = 0xFFu;
    g_linux_libc64_image[offset + 6u] = 0xFFu;
    g_linux_libc64_image[offset + 7u] = 0xC3u;
}

static void linux_libc64_write_errno_location_stub(u32 rva)
{
    u32 offset = linux_libc64_rva_to_offset(rva);
    s32 relative = (s32)LINUX_LIBC64_ERRNO_RVA - (s32)(rva + 7u);

    if ((rva < LINUX_LIBC64_TEXT_RVA)
        || ((offset + 8u) > (LINUX_LIBC64_TEXT_FILE_OFFSET + LINUX_LIBC64_TEXT_FILE_BYTES)))
    {
        return;
    }

    g_linux_libc64_image[offset] = 0x48u;
    g_linux_libc64_image[offset + 1u] = 0x8Du;
    g_linux_libc64_image[offset + 2u] = 0x05u;
    linux_libc64_write_le32(g_linux_libc64_image, offset + 3u, (u32)relative);
    g_linux_libc64_image[offset + 7u] = 0xC3u;
}

static void linux_libc64_write_jump_stub(u32 rva, u32 target_rva)
{
    u32 offset = linux_libc64_rva_to_offset(rva);
    s32 relative = (s32)target_rva - (s32)(rva + 5u);

    if ((rva < LINUX_LIBC64_TEXT_RVA)
        || ((offset + 5u) > (LINUX_LIBC64_TEXT_FILE_OFFSET + LINUX_LIBC64_TEXT_FILE_BYTES)))
    {
        return;
    }

    g_linux_libc64_image[offset] = 0xE9u;
    linux_libc64_write_le32(g_linux_libc64_image, offset + 1u, (u32)relative);
}

static void linux_libc64_write_memcpy_stub(u32 rva)
{
    static const u8 stub[] = {
        0x48u, 0x89u, 0xF8u,
        0x48u, 0x85u, 0xD2u,
        0x74u, 0x0Fu,
        0x8Au, 0x0Eu,
        0x88u, 0x0Fu,
        0x48u, 0xFFu, 0xC6u,
        0x48u, 0xFFu, 0xC7u,
        0x48u, 0xFFu, 0xCAu,
        0x75u, 0xF1u,
        0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        stub,
        (u32)sizeof(stub));
}

static void linux_libc64_write_memset_stub(u32 rva)
{
    static const u8 stub[] = {
        0x48u, 0x89u, 0xF8u,
        0x48u, 0x85u, 0xD2u,
        0x74u, 0x0Bu,
        0x40u, 0x88u, 0x37u,
        0x48u, 0xFFu, 0xC7u,
        0x48u, 0xFFu, 0xCAu,
        0x75u, 0xF5u,
        0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        stub,
        (u32)sizeof(stub));
}

static void linux_libc64_write_strlen_stub(u32 rva)
{
    static const u8 stub[] = {
        0x48u, 0x31u, 0xC0u,
        0x80u, 0x3Cu, 0x07u, 0x00u,
        0x74u, 0x05u,
        0x48u, 0xFFu, 0xC0u,
        0xEBu, 0xF5u,
        0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        stub,
        (u32)sizeof(stub));
}

static void linux_libc64_write_strcpy_stub(u32 rva)
{
    static const u8 stub[] = {
        0x48u, 0x89u, 0xF8u,
        0x8Au, 0x0Eu,
        0x88u, 0x0Fu,
        0x48u, 0xFFu, 0xC6u,
        0x48u, 0xFFu, 0xC7u,
        0x84u, 0xC9u,
        0x75u, 0xF2u,
        0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        stub,
        (u32)sizeof(stub));
}

static void linux_libc64_write_strncpy_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x89u, 0xF8u, 0x48u, 0x85u, 0xD2u, 0x74u, 0x26u,
        0x8Au, 0x0Eu, 0x88u, 0x0Fu, 0x48u, 0xFFu, 0xC6u, 0x48u,
        0xFFu, 0xC7u, 0x48u, 0xFFu, 0xCAu, 0x84u, 0xC9u, 0x75u,
        0x10u, 0x48u, 0x85u, 0xD2u, 0x74u, 0x10u, 0xC6u, 0x07u,
        0x00u, 0x48u, 0xFFu, 0xC7u, 0x48u, 0xFFu, 0xCAu, 0xEBu,
        0xF0u, 0x48u, 0x85u, 0xD2u, 0x75u, 0xDAu, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_STRNCPY)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_strcmp_helper(void)
{
    static const u8 helper[] = {
        0x8Au, 0x07u, 0x8Au, 0x16u, 0x38u, 0xD0u, 0x75u, 0x0Cu,
        0x84u, 0xC0u, 0x74u, 0x11u, 0x48u, 0xFFu, 0xC7u, 0x48u,
        0xFFu, 0xC6u, 0xEBu, 0xECu, 0x0Fu, 0xB6u, 0xC0u, 0x0Fu,
        0xB6u, 0xD2u, 0x29u, 0xD0u, 0xC3u, 0x31u, 0xC0u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_STRCMP)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_strncmp_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xD2u, 0x74u, 0x17u, 0x8Au, 0x07u, 0x8Au,
        0x0Eu, 0x38u, 0xC8u, 0x75u, 0x12u, 0x84u, 0xC0u, 0x74u,
        0x0Bu, 0x48u, 0xFFu, 0xC7u, 0x48u, 0xFFu, 0xC6u, 0x48u,
        0xFFu, 0xCAu, 0x75u, 0xE9u, 0x31u, 0xC0u, 0xC3u, 0x0Fu,
        0xB6u, 0xC0u, 0x0Fu, 0xB6u, 0xC9u, 0x29u, 0xC8u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_STRNCMP)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_memmove_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x89u, 0xF8u, 0x48u, 0x85u, 0xD2u, 0x74u, 0x26u,
        0x48u, 0x39u, 0xF7u, 0x76u, 0x1Bu, 0x48u, 0x8Du, 0x0Cu,
        0x16u, 0x48u, 0x39u, 0xCFu, 0x73u, 0x12u, 0x48u, 0x8Du,
        0x74u, 0x16u, 0xFFu, 0x48u, 0x8Du, 0x7Cu, 0x17u, 0xFFu,
        0x48u, 0x89u, 0xD1u, 0xFDu, 0xF3u, 0xA4u, 0xFCu, 0xC3u,
        0x48u, 0x89u, 0xD1u, 0xFCu, 0xF3u, 0xA4u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_MEMMOVE)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_printf_helper(void)
{
    static const u8 helper[] = {
        0x49u, 0x89u, 0xF8u,
        0x31u, 0xD2u,
        0x41u, 0x8Au, 0x04u, 0x10u,
        0x84u, 0xC0u,
        0x74u, 0x09u,
        0x3Cu, 0x25u,
        0x74u, 0x15u,
        0x48u, 0xFFu, 0xC2u,
        0xEBu, 0xEFu,
        0xB8u, 0x01u, 0x00u, 0x00u, 0x00u,
        0xBFu, 0x01u, 0x00u, 0x00u, 0x00u,
        0x4Cu, 0x89u, 0xC6u,
        0x0Fu, 0x05u,
        0xC3u,
        0x48u, 0xC7u, 0xC0u, 0xDAu, 0xFFu, 0xFFu, 0xFFu,
        0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_PRINTF)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_malloc_helper(void)
{
    static const u8 helper[] = {
        0xBFu, 0x00u, 0x00u, 0x9Du, 0x47u, 0x68u, 0x00u, 0x10u,
        0x00u, 0x00u, 0x5Eu, 0x6Au, 0x03u, 0x5Au, 0x6Au, 0x32u,
        0x41u, 0x5Au, 0x6Au, 0xFFu, 0x41u, 0x58u, 0x45u, 0x31u,
        0xC9u, 0x6Au, 0x09u, 0x58u, 0x0Fu, 0x05u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_MALLOC)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_free_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x74u, 0x0Cu, 0x68u, 0x00u, 0x10u,
        0x00u, 0x00u, 0x5Eu, 0x6Au, 0x0Bu, 0x58u, 0x0Fu, 0x05u,
        0xC3u, 0x31u, 0xC0u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_FREE)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_realloc_helper(void)
{
    static const u8 helper[] = {
        0x57u, 0xBFu, 0x00u, 0x00u, 0x9Eu, 0x47u, 0x68u, 0x00u,
        0x10u, 0x00u, 0x00u, 0x5Eu, 0x6Au, 0x03u, 0x5Au, 0x6Au,
        0x32u, 0x41u, 0x5Au, 0x6Au, 0xFFu, 0x41u, 0x58u, 0x45u,
        0x31u, 0xC9u, 0x6Au, 0x09u, 0x58u, 0x0Fu, 0x05u, 0x41u,
        0x5Bu, 0x48u, 0x85u, 0xC0u, 0x78u, 0x27u, 0x4Du, 0x85u,
        0xDBu, 0x74u, 0x22u, 0x49u, 0x89u, 0xC2u, 0x48u, 0x89u,
        0xC7u, 0x4Cu, 0x89u, 0xDEu, 0xB9u, 0x40u, 0x00u, 0x00u,
        0x00u, 0xFCu, 0xF3u, 0xA4u, 0x41u, 0x52u, 0x4Cu, 0x89u,
        0xDFu, 0x68u, 0x00u, 0x10u, 0x00u, 0x00u, 0x5Eu, 0x6Au,
        0x0Bu, 0x58u, 0x0Fu, 0x05u, 0x58u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_REALLOC)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_calloc_helper(void)
{
    static const u8 helper[] = {
        0xBFu, 0x00u, 0x00u, 0x9Du, 0x47u, 0x68u, 0x00u, 0x10u,
        0x00u, 0x00u, 0x5Eu, 0x6Au, 0x03u, 0x5Au, 0x6Au, 0x32u,
        0x41u, 0x5Au, 0x6Au, 0xFFu, 0x41u, 0x58u, 0x45u, 0x31u,
        0xC9u, 0x6Au, 0x09u, 0x58u, 0x0Fu, 0x05u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_CALLOC)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_puts_helper(void)
{
    static const u8 helper[] = {
        0x41u, 0x54u, 0x49u, 0x89u, 0xF8u, 0x31u, 0xD2u, 0x41u,
        0x80u, 0x3Cu, 0x10u, 0x00u, 0x74u, 0x05u, 0x48u, 0xFFu,
        0xC2u, 0xEBu, 0xF4u, 0xB8u, 0x01u, 0x00u, 0x00u, 0x00u,
        0xBFu, 0x01u, 0x00u, 0x00u, 0x00u, 0x4Cu, 0x89u, 0xC6u,
        0x0Fu, 0x05u, 0x48u, 0x85u, 0xC0u, 0x78u, 0x23u, 0x49u,
        0x89u, 0xC4u, 0xB8u, 0x01u, 0x00u, 0x00u, 0x00u, 0xBFu,
        0x01u, 0x00u, 0x00u, 0x00u, 0x48u, 0x8Du, 0x35u, 0x12u,
        0x00u, 0x00u, 0x00u, 0xBAu, 0x01u, 0x00u, 0x00u, 0x00u,
        0x0Fu, 0x05u, 0x48u, 0x85u, 0xC0u, 0x78u, 0x03u, 0x4Cu,
        0x01u, 0xE0u, 0x41u, 0x5Cu, 0xC3u, 0x0Au
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_PUTS)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_fputs_stub(u32 rva)
{
    static const u8 stub[] = {
        0x49u, 0x89u, 0xF8u, 0x31u, 0xD2u, 0x41u, 0x80u, 0x3Cu,
        0x10u, 0x00u, 0x74u, 0x05u, 0x48u, 0xFFu, 0xC2u, 0xEBu,
        0xF4u, 0xB8u, 0x01u, 0x00u, 0x00u, 0x00u, 0x89u, 0xF7u,
        0x4Cu, 0x89u, 0xC6u, 0x0Fu, 0x05u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        stub,
        (u32)sizeof(stub));
}

static void linux_libc64_write_fwrite_stub(u32 rva)
{
    static const u8 stub[] = {
        0x49u, 0x89u, 0xF8u, 0x48u, 0x0Fu, 0xAFu, 0xD6u, 0x48u,
        0x89u, 0xCFu, 0x4Cu, 0x89u, 0xC6u, 0xB8u, 0x01u, 0x00u,
        0x00u, 0x00u, 0x0Fu, 0x05u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        stub,
        (u32)sizeof(stub));
}

static void linux_libc64_write_getenv_helper(void)
{
    static const u8 helper[] = {
        0x49u, 0xB8u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x41u, 0xB9u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x48u, 0x85u, 0xFFu, 0x74u, 0x3Au, 0x4Du, 0x85u, 0xC0u,
        0x74u, 0x35u, 0x45u, 0x85u, 0xC9u, 0x74u, 0x30u, 0x49u,
        0x8Bu, 0x30u, 0x48u, 0x85u, 0xF6u, 0x74u, 0x28u, 0x48u,
        0x89u, 0xFAu, 0x8Au, 0x02u, 0x84u, 0xC0u, 0x74u, 0x0Cu,
        0x3Au, 0x06u, 0x75u, 0x12u, 0x48u, 0xFFu, 0xC2u, 0x48u,
        0xFFu, 0xC6u, 0xEBu, 0xEEu, 0x80u, 0x3Eu, 0x3Du, 0x75u,
        0x05u, 0x48u, 0x8Du, 0x46u, 0x01u, 0xC3u, 0x49u, 0x83u,
        0xC0u, 0x08u, 0x41u, 0xFFu, 0xC9u, 0x75u, 0xD0u, 0x31u,
        0xC0u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_GETENV)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_setenv_helper(void)
{
    static const u8 helper[] = {
        0x49u, 0xB8u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x41u, 0xB9u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x48u, 0x85u, 0xFFu, 0x0Fu, 0x84u, 0x9Bu, 0x00u, 0x00u,
        0x00u, 0x48u, 0x85u, 0xF6u, 0x0Fu, 0x84u, 0x92u, 0x00u,
        0x00u, 0x00u, 0x80u, 0x3Fu, 0x00u, 0x0Fu, 0x84u, 0x89u,
        0x00u, 0x00u, 0x00u, 0x45u, 0x85u, 0xC9u, 0x74u, 0x7Cu,
        0x4Du, 0x8Bu, 0x10u, 0x4Du, 0x85u, 0xD2u, 0x74u, 0x74u,
        0x49u, 0x89u, 0xFBu, 0x41u, 0x8Au, 0x03u, 0x84u, 0xC0u,
        0x74u, 0x11u, 0x3Cu, 0x3Du, 0x74u, 0x6Eu, 0x41u, 0x38u,
        0x02u, 0x75u, 0x58u, 0x49u, 0xFFu, 0xC3u, 0x49u, 0xFFu,
        0xC2u, 0xEBu, 0xE8u, 0x41u, 0x80u, 0x3Au, 0x3Du, 0x75u,
        0x4Au, 0x85u, 0xD2u, 0x75u, 0x03u, 0x31u, 0xC0u, 0xC3u,
        0x49u, 0xFFu, 0xC2u, 0x4Du, 0x8Bu, 0x18u, 0x49u, 0x83u,
        0xC3u, 0x30u, 0x48u, 0x89u, 0xF1u, 0x4Du, 0x39u, 0xDAu,
        0x73u, 0x3Au, 0x8Au, 0x01u, 0x84u, 0xC0u, 0x74u, 0x08u,
        0x48u, 0xFFu, 0xC1u, 0x49u, 0xFFu, 0xC2u, 0xEBu, 0xEDu,
        0x4Du, 0x8Bu, 0x10u, 0x41u, 0x80u, 0x3Au, 0x3Du, 0x74u,
        0x05u, 0x49u, 0xFFu, 0xC2u, 0xEBu, 0xF5u, 0x49u, 0xFFu,
        0xC2u, 0x8Au, 0x06u, 0x41u, 0x88u, 0x02u, 0x48u, 0xFFu,
        0xC6u, 0x49u, 0xFFu, 0xC2u, 0x84u, 0xC0u, 0x75u, 0xF1u,
        0x31u, 0xC0u, 0xC3u, 0x49u, 0x83u, 0xC0u, 0x08u, 0x41u,
        0xFFu, 0xC9u, 0x75u, 0x84u, 0x48u, 0xC7u, 0xC0u, 0xF4u,
        0xFFu, 0xFFu, 0xFFu, 0xC3u, 0x48u, 0xC7u, 0xC0u, 0xEAu,
        0xFFu, 0xFFu, 0xFFu, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_SETENV)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_mutex_lock_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x74u, 0x39u, 0x31u, 0xC0u, 0xBAu,
        0x01u, 0x00u, 0x00u, 0x00u, 0xF0u, 0x0Fu, 0xB1u, 0x17u,
        0x74u, 0x29u, 0xB8u, 0xCAu, 0x00u, 0x00u, 0x00u, 0xBEu,
        0x80u, 0x00u, 0x00u, 0x00u, 0xBAu, 0x01u, 0x00u, 0x00u,
        0x00u, 0x45u, 0x31u, 0xD2u, 0x45u, 0x31u, 0xC0u, 0x45u,
        0x31u, 0xC9u, 0x0Fu, 0x05u, 0x48u, 0x85u, 0xC0u, 0x74u,
        0xD4u, 0x48u, 0x83u, 0xF8u, 0xF5u, 0x74u, 0xCEu, 0x48u,
        0xF7u, 0xD8u, 0xC3u, 0x31u, 0xC0u, 0xC3u, 0xB8u, 0x16u,
        0x00u, 0x00u, 0x00u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_PTHREAD_MUTEX_LOCK)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_mutex_unlock_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x74u, 0x2Eu, 0x31u, 0xC0u, 0x87u,
        0x07u, 0x85u, 0xC0u, 0x74u, 0x26u, 0xB8u, 0xCAu, 0x00u,
        0x00u, 0x00u, 0xBEu, 0x81u, 0x00u, 0x00u, 0x00u, 0xBAu,
        0x01u, 0x00u, 0x00u, 0x00u, 0x45u, 0x31u, 0xD2u, 0x45u,
        0x31u, 0xC0u, 0x45u, 0x31u, 0xC9u, 0x0Fu, 0x05u, 0x48u,
        0x85u, 0xC0u, 0x78u, 0x03u, 0x31u, 0xC0u, 0xC3u, 0x48u,
        0xF7u, 0xD8u, 0xC3u, 0xB8u, 0x16u, 0x00u, 0x00u, 0x00u,
        0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_PTHREAD_MUTEX_UNLOCK)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_create_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x75u, 0x06u, 0xB8u, 0x16u, 0x00u,
        0x00u, 0x00u, 0xC3u, 0x48u, 0x85u, 0xD2u, 0x75u, 0x06u,
        0xB8u, 0x16u, 0x00u, 0x00u, 0x00u, 0xC3u, 0x48u, 0x85u,
        0xF6u, 0x74u, 0x06u, 0xB8u, 0x26u, 0x00u, 0x00u, 0x00u,
        0xC3u, 0x41u, 0x54u, 0x48u, 0x83u, 0xECu, 0x20u, 0x48u,
        0x89u, 0x3Cu, 0x24u, 0x48u, 0x89u, 0x54u, 0x24u, 0x08u,
        0x48u, 0x89u, 0x4Cu, 0x24u, 0x10u, 0xE8u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x41u, 0x5Cu, 0x49u, 0x81u, 0xC4u, 0x26u,
        0x09u, 0x00u, 0x00u, 0xB9u, 0x10u, 0x00u, 0x00u, 0x00u,
        0x41u, 0x83u, 0x3Cu, 0x24u, 0x00u, 0x74u, 0x14u, 0x49u,
        0x83u, 0xC4u, 0x04u, 0xFFu, 0xC9u, 0x75u, 0xF1u, 0xB8u,
        0x0Bu, 0x00u, 0x00u, 0x00u, 0x48u, 0x83u, 0xC4u, 0x20u,
        0x41u, 0x5Cu, 0xC3u, 0x41u, 0xC7u, 0x04u, 0x24u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x31u, 0xFFu, 0xBEu, 0x00u, 0x40u,
        0x00u, 0x00u, 0xBAu, 0x03u, 0x00u, 0x00u, 0x00u, 0x41u,
        0xBAu, 0x22u, 0x00u, 0x00u, 0x00u, 0x49u, 0xC7u, 0xC0u,
        0xFFu, 0xFFu, 0xFFu, 0xFFu, 0x45u, 0x31u, 0xC9u, 0xB8u,
        0x09u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x05u, 0x48u, 0x85u,
        0xC0u, 0x78u, 0x65u, 0x48u, 0x8Du, 0xB0u, 0xC0u, 0x3Fu,
        0x00u, 0x00u, 0x48u, 0x8Du, 0x3Du, 0x68u, 0x00u, 0x00u,
        0x00u, 0x48u, 0x89u, 0x7Eu, 0x20u, 0x48u, 0x8Bu, 0x7Cu,
        0x24u, 0x08u, 0x48u, 0x89u, 0x7Eu, 0x28u, 0x48u, 0x8Bu,
        0x7Cu, 0x24u, 0x10u, 0x48u, 0x89u, 0x7Eu, 0x30u, 0xBFu,
        0x00u, 0x0Fu, 0x31u, 0x01u, 0x48u, 0x8Bu, 0x14u, 0x24u,
        0x4Du, 0x89u, 0xE2u, 0x45u, 0x31u, 0xC0u, 0x45u, 0x31u,
        0xC9u, 0xB8u, 0x38u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x05u,
        0x48u, 0x85u, 0xC0u, 0x78u, 0x12u, 0x75u, 0x07u, 0x48u,
        0x83u, 0xC4u, 0x20u, 0x31u, 0xC0u, 0xC3u, 0x48u, 0x83u,
        0xC4u, 0x20u, 0x41u, 0x5Cu, 0x31u, 0xC0u, 0xC3u, 0x41u,
        0xC7u, 0x04u, 0x24u, 0x00u, 0x00u, 0x00u, 0x00u, 0xF7u,
        0xD8u, 0x48u, 0x83u, 0xC4u, 0x20u, 0x41u, 0x5Cu, 0xC3u,
        0x41u, 0xC7u, 0x04u, 0x24u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xF7u, 0xD8u, 0x48u, 0x83u, 0xC4u, 0x20u, 0x41u, 0x5Cu,
        0xC3u, 0x48u, 0x8Bu, 0x04u, 0x24u, 0x48u, 0x8Bu, 0x7Cu,
        0x24u, 0x08u, 0xFFu, 0xD0u, 0x31u, 0xFFu, 0xB8u, 0x3Cu,
        0x00u, 0x00u, 0x00u, 0x0Fu, 0x05u, 0xF4u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_PTHREAD_CREATE)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_join_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x75u, 0x06u, 0xB8u, 0x16u, 0x00u,
        0x00u, 0x00u, 0xC3u, 0x48u, 0x85u, 0xF6u, 0x74u, 0x06u,
        0xB8u, 0x26u, 0x00u, 0x00u, 0x00u, 0xC3u, 0x31u, 0xF6u,
        0x31u, 0xD2u, 0x45u, 0x31u, 0xD2u, 0xB8u, 0x3Du, 0x00u,
        0x00u, 0x00u, 0x0Fu, 0x05u, 0x48u, 0x85u, 0xC0u, 0x78u,
        0x03u, 0x31u, 0xC0u, 0xC3u, 0xF7u, 0xD8u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_PTHREAD_JOIN)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_cond_init_helper(u32 rva)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x74u, 0x09u, 0xC7u, 0x07u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x31u, 0xC0u, 0xC3u, 0xB8u, 0x16u,
        0x00u, 0x00u, 0x00u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_cond_wake_helper(u32 rva, u32 wake_count)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x74u, 0x26u, 0xB8u, 0xCAu, 0x00u,
        0x00u, 0x00u, 0xBEu, 0x81u, 0x00u, 0x00u, 0x00u, 0xBAu,
        0x00u, 0x00u, 0x00u, 0x00u, 0x45u, 0x31u, 0xD2u, 0x45u,
        0x31u, 0xC0u, 0x45u, 0x31u, 0xC9u, 0x0Fu, 0x05u, 0x48u,
        0x85u, 0xC0u, 0x78u, 0x03u, 0x31u, 0xC0u, 0xC3u, 0x48u,
        0xF7u, 0xD8u, 0xC3u, 0xB8u, 0x16u, 0x00u, 0x00u, 0x00u,
        0xC3u
    };
    u32 offset = linux_libc64_rva_to_offset(rva);

    linux_libc64_copy_bytes(
        &g_linux_libc64_image[offset],
        helper,
        (u32)sizeof(helper));
    linux_libc64_write_le32(g_linux_libc64_image, offset + 16u, wake_count);
}

static void linux_libc64_write_pthread_cond_wait_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x0Fu, 0x84u, 0x9Fu, 0x00u, 0x00u,
        0x00u, 0x48u, 0x85u, 0xF6u, 0x0Fu, 0x84u, 0x96u, 0x00u,
        0x00u, 0x00u, 0x53u, 0x41u, 0x54u, 0x49u, 0x89u, 0xFCu,
        0x48u, 0x89u, 0xF3u, 0x31u, 0xC0u, 0x87u, 0x03u, 0x85u,
        0xC0u, 0x74u, 0x1Du, 0xB8u, 0xCAu, 0x00u, 0x00u, 0x00u,
        0x48u, 0x89u, 0xDFu, 0xBEu, 0x81u, 0x00u, 0x00u, 0x00u,
        0xBAu, 0x01u, 0x00u, 0x00u, 0x00u, 0x45u, 0x31u, 0xD2u,
        0x45u, 0x31u, 0xC0u, 0x45u, 0x31u, 0xC9u, 0x0Fu, 0x05u,
        0xB8u, 0xCAu, 0x00u, 0x00u, 0x00u, 0x4Cu, 0x89u, 0xE7u,
        0xBEu, 0x80u, 0x00u, 0x00u, 0x00u, 0x31u, 0xD2u, 0x45u,
        0x31u, 0xD2u, 0x45u, 0x31u, 0xC0u, 0x45u, 0x31u, 0xC9u,
        0x0Fu, 0x05u, 0x49u, 0x89u, 0xC4u, 0x31u, 0xC0u, 0xBAu,
        0x01u, 0x00u, 0x00u, 0x00u, 0xF0u, 0x0Fu, 0xB1u, 0x13u,
        0x74u, 0x2Bu, 0xB8u, 0xCAu, 0x00u, 0x00u, 0x00u, 0x48u,
        0x89u, 0xDFu, 0xBEu, 0x80u, 0x00u, 0x00u, 0x00u, 0xBAu,
        0x01u, 0x00u, 0x00u, 0x00u, 0x45u, 0x31u, 0xD2u, 0x45u,
        0x31u, 0xC0u, 0x45u, 0x31u, 0xC9u, 0x0Fu, 0x05u, 0x48u,
        0x85u, 0xC0u, 0x74u, 0xD1u, 0x48u, 0x83u, 0xF8u, 0xF5u,
        0x74u, 0xCBu, 0x49u, 0x89u, 0xC4u, 0x4Cu, 0x89u, 0xE0u,
        0x48u, 0x85u, 0xC0u, 0x79u, 0x05u, 0x48u, 0xF7u, 0xD8u,
        0xEBu, 0x02u, 0x31u, 0xC0u, 0x41u, 0x5Cu, 0x5Bu, 0xC3u,
        0xB8u, 0x16u, 0x00u, 0x00u, 0x00u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_HELPER_PTHREAD_COND_WAIT)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_key_create_helper(void)
{
    static const u8 helper[] = {
        0x48u, 0x85u, 0xFFu, 0x74u, 0x21u, 0x48u, 0x8Du, 0x05u,
        0x74u, 0x06u, 0x00u, 0x00u, 0x83u, 0x38u, 0x00u, 0x75u,
        0x0Fu, 0xC7u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0xC7u,
        0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x31u, 0xC0u, 0xC3u,
        0xB8u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0xC3u, 0xB8u, 0x16u,
        0x00u, 0x00u, 0x00u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_PTHREAD_KEY_CREATE)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_setspecific_helper(void)
{
    static const u8 helper[] = {
        0x85u, 0xFFu, 0x75u, 0x75u, 0x83u, 0x3Du, 0x35u, 0x06u,
        0x00u, 0x00u, 0x01u, 0x75u, 0x6Cu, 0x53u, 0x48u, 0x89u,
        0xF3u, 0xB8u, 0xBAu, 0x00u, 0x00u, 0x00u, 0x0Fu, 0x05u,
        0x48u, 0x85u, 0xC0u, 0x78u, 0x57u, 0x89u, 0xC1u, 0x4Cu,
        0x8Du, 0x05u, 0x2Au, 0x06u, 0x00u, 0x00u, 0xBAu, 0x04u,
        0x00u, 0x00u, 0x00u, 0x41u, 0x83u, 0x78u, 0x04u, 0x00u,
        0x74u, 0x05u, 0x41u, 0x39u, 0x08u, 0x74u, 0x2Au, 0x49u,
        0x83u, 0xC0u, 0x10u, 0xFFu, 0xCAu, 0x75u, 0xECu, 0x4Cu,
        0x8Du, 0x05u, 0x0Au, 0x06u, 0x00u, 0x00u, 0xBAu, 0x04u,
        0x00u, 0x00u, 0x00u, 0x41u, 0x83u, 0x78u, 0x04u, 0x00u,
        0x74u, 0x0Fu, 0x49u, 0x83u, 0xC0u, 0x10u, 0xFFu, 0xCAu,
        0x75u, 0xF1u, 0xB8u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x5Bu,
        0xC3u, 0x41u, 0x89u, 0x08u, 0x41u, 0xC7u, 0x40u, 0x04u,
        0x01u, 0x00u, 0x00u, 0x00u, 0x49u, 0x89u, 0x58u, 0x08u,
        0x31u, 0xC0u, 0x5Bu, 0xC3u, 0x48u, 0xF7u, 0xD8u, 0x5Bu,
        0xC3u, 0xB8u, 0x16u, 0x00u, 0x00u, 0x00u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_PTHREAD_SETSPECIFIC)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_pthread_getspecific_helper(void)
{
    static const u8 helper[] = {
        0x85u, 0xFFu, 0x75u, 0x37u, 0x83u, 0x3Du, 0xA5u, 0x05u,
        0x00u, 0x00u, 0x01u, 0x75u, 0x2Eu, 0xB8u, 0xBAu, 0x00u,
        0x00u, 0x00u, 0x0Fu, 0x05u, 0x48u, 0x85u, 0xC0u, 0x78u,
        0x22u, 0x89u, 0xC1u, 0x4Cu, 0x8Du, 0x05u, 0x9Eu, 0x05u,
        0x00u, 0x00u, 0xBAu, 0x04u, 0x00u, 0x00u, 0x00u, 0x41u,
        0x83u, 0x78u, 0x04u, 0x00u, 0x74u, 0x05u, 0x41u, 0x39u,
        0x08u, 0x74u, 0x0Bu, 0x49u, 0x83u, 0xC0u, 0x10u, 0xFFu,
        0xCAu, 0x75u, 0xECu, 0x31u, 0xC0u, 0xC3u, 0x49u, 0x8Bu,
        0x40u, 0x08u, 0xC3u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(LINUX_LIBC64_RVA_PTHREAD_GETSPECIFIC)],
        helper,
        (u32)sizeof(helper));
}

static void linux_libc64_write_libc_start_main_stub(u32 rva)
{
    static const u8 stub[] = {
        0x57u,
        0x48u, 0x89u, 0xF7u,
        0x48u, 0x89u, 0xD6u,
        0x48u, 0x8Du, 0x54u, 0xFEu, 0x08u,
        0xFFu, 0x14u, 0x24u,
        0x89u, 0xC7u,
        0xB8u, 0xE7u, 0x00u, 0x00u, 0x00u,
        0x0Fu, 0x05u,
        0xF4u
    };
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[linux_libc64_rva_to_offset(rva)],
        stub,
        (u32)sizeof(stub));
}

static void linux_libc64_build_image(void)
{
    static const u8 image_name[] = "libc.so";
    static const u8 unavailable[] = "libc: shim symbol unavailable";
    u32 index;

    if (g_linux_libc64_image_ready != 0u)
    {
        return;
    }

    for (index = 0u; index < LINUX_LIBC64_FILE_BYTES; ++index)
    {
        g_linux_libc64_image[index] = 0u;
    }

    g_linux_libc64_image[0] = 0x7Fu;
    g_linux_libc64_image[1] = (u8)'E';
    g_linux_libc64_image[2] = (u8)'L';
    g_linux_libc64_image[3] = (u8)'F';
    g_linux_libc64_image[ELF64_EI_CLASS] = ELF64_CLASS_64;
    g_linux_libc64_image[ELF64_EI_DATA] = ELF64_DATA_LSB;
    g_linux_libc64_image[ELF64_EI_VERSION] = ELF64_VERSION_CURRENT;
    g_linux_libc64_image[ELF64_EI_OSABI] = ELF64_OSABI_LINUX;
    linux_libc64_write_le16(g_linux_libc64_image, 16u, (u16)ELF64_TYPE_DYN);
    linux_libc64_write_le16(g_linux_libc64_image, 18u, (u16)ELF64_MACHINE_X86_64);
    linux_libc64_write_le32(g_linux_libc64_image, 20u, ELF64_VERSION_CURRENT);
    linux_libc64_write_le64(g_linux_libc64_image, 24u, LINUX_LIBC64_RVA_EXIT);
    linux_libc64_write_le64(g_linux_libc64_image, 32u, 0x40ull);
    linux_libc64_write_le16(g_linux_libc64_image, 52u, (u16)ELF64_EHDR_BYTES);
    linux_libc64_write_le16(g_linux_libc64_image, 54u, (u16)ELF64_PHDR_BYTES);
    linux_libc64_write_le16(g_linux_libc64_image, 56u, 2u);

    linux_libc64_write_phdr(
        0x40u,
        ELF64_PT_LOAD,
        ELF64_PF_R | ELF64_PF_X,
        LINUX_LIBC64_TEXT_FILE_OFFSET,
        LINUX_LIBC64_TEXT_RVA,
        LINUX_LIBC64_TEXT_FILE_BYTES + LINUX_LIBC64_RODATA_FILE_BYTES + LINUX_LIBC64_DYNAMIC_BYTES,
        VMA64_PAGE_BYTES,
        0x100ull);
    linux_libc64_write_phdr(
        0x40u + ELF64_PHDR_BYTES,
        ELF64_PT_DYNAMIC,
        ELF64_PF_R,
        LINUX_LIBC64_DYNAMIC_FILE_OFFSET,
        LINUX_LIBC64_DYNAMIC_RVA,
        LINUX_LIBC64_DYNAMIC_BYTES,
        LINUX_LIBC64_DYNAMIC_BYTES,
        8ull);

    for (index = 0u; index < LINUX_LIBC64_SYMBOL_COUNT; ++index)
    {
        const linux_libc64_export_t *export = &g_linux_libc64_exports[index];
        if (export->kind == LINUX_LIBC64_KIND_SYSCALL)
        {
            linux_libc64_write_syscall_stub(export->rva, export->syscall_number);
        }
        else if (export->rva == LINUX_LIBC64_RVA_MEMCPY)
        {
            linux_libc64_write_memcpy_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_MEMSET)
        {
            linux_libc64_write_memset_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_STRLEN)
        {
            linux_libc64_write_strlen_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_STRCPY)
        {
            linux_libc64_write_strcpy_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_STRNCPY)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_STRNCPY);
            linux_libc64_write_strncpy_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_STRCMP)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_STRCMP);
            linux_libc64_write_strcmp_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_STRNCMP)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_STRNCMP);
            linux_libc64_write_strncmp_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_MEMMOVE)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_MEMMOVE);
            linux_libc64_write_memmove_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PRINTF)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_PRINTF);
            linux_libc64_write_printf_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PUTS)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_PUTS);
            linux_libc64_write_puts_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_FPUTS)
        {
            linux_libc64_write_fputs_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_FWRITE)
        {
            linux_libc64_write_fwrite_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_GETENV)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_GETENV);
            linux_libc64_write_getenv_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_SETENV)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_SETENV);
            linux_libc64_write_setenv_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_ERRNO_LOCATION)
        {
            linux_libc64_write_errno_location_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_CREATE)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_PTHREAD_CREATE);
            linux_libc64_write_pthread_create_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_JOIN)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_PTHREAD_JOIN);
            linux_libc64_write_pthread_join_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_MUTEX_LOCK)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_PTHREAD_MUTEX_LOCK);
            linux_libc64_write_pthread_mutex_lock_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_MUTEX_UNLOCK)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_PTHREAD_MUTEX_UNLOCK);
            linux_libc64_write_pthread_mutex_unlock_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_EXIT)
        {
            linux_libc64_write_syscall_stub(export->rva, LINUX_ABI64_SYSCALL_EXIT);
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_COND_INIT)
        {
            linux_libc64_write_pthread_cond_init_helper(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_COND_WAIT)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_PTHREAD_COND_WAIT);
            linux_libc64_write_pthread_cond_wait_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_COND_SIGNAL)
        {
            linux_libc64_write_pthread_cond_wake_helper(export->rva, 1u);
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_COND_BROADCAST)
        {
            linux_libc64_write_pthread_cond_wake_helper(export->rva, 0x7FFFFFFFu);
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_KEY_CREATE)
        {
            linux_libc64_write_pthread_key_create_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_SETSPECIFIC)
        {
            linux_libc64_write_pthread_setspecific_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_PTHREAD_GETSPECIFIC)
        {
            linux_libc64_write_pthread_getspecific_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_LIBC_START_MAIN)
        {
            linux_libc64_write_libc_start_main_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_ABORT)
        {
            linux_libc64_write_abort_stub(export->rva);
        }
        else if (export->rva == LINUX_LIBC64_RVA_MALLOC)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_MALLOC);
            linux_libc64_write_malloc_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_FREE)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_FREE);
            linux_libc64_write_free_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_REALLOC)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_REALLOC);
            linux_libc64_write_realloc_helper();
        }
        else if (export->rva == LINUX_LIBC64_RVA_CALLOC)
        {
            linux_libc64_write_jump_stub(export->rva, LINUX_LIBC64_RVA_HELPER_CALLOC);
            linux_libc64_write_calloc_helper();
        }
        else
        {
            linux_libc64_write_unavailable_stub(export->rva);
        }
    }

    linux_libc64_copy_bytes(
        &g_linux_libc64_image[LINUX_LIBC64_RODATA_FILE_OFFSET],
        image_name,
        (u32)sizeof(image_name));
    linux_libc64_copy_bytes(
        &g_linux_libc64_image[LINUX_LIBC64_RODATA_FILE_OFFSET + 0x20u],
        unavailable,
        (u32)sizeof(unavailable));

    linux_libc64_write_le64(
        g_linux_libc64_image,
        LINUX_LIBC64_DYNAMIC_FILE_OFFSET,
        5ull);
    linux_libc64_write_le64(
        g_linux_libc64_image,
        LINUX_LIBC64_DYNAMIC_FILE_OFFSET + 8u,
        LINUX_LIBC64_RODATA_RVA);
    linux_libc64_write_le64(
        g_linux_libc64_image,
        LINUX_LIBC64_DYNAMIC_FILE_OFFSET + 16u,
        10ull);
    linux_libc64_write_le64(
        g_linux_libc64_image,
        LINUX_LIBC64_DYNAMIC_FILE_OFFSET + 24u,
        0x60ull);
    linux_libc64_write_le64(
        g_linux_libc64_image,
        LINUX_LIBC64_DYNAMIC_FILE_OFFSET + 32u,
        0ull);
    linux_libc64_write_le64(
        g_linux_libc64_image,
        LINUX_LIBC64_DYNAMIC_FILE_OFFSET + 40u,
        0ull);

    g_linux_libc64_image_ready = 1u;
}

static void linux_libc64_clear_load(linux_libc64_load_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->image_base = 0ull;
    result->image_end = 0ull;
    result->read_fn = 0ull;
    result->write_fn = 0ull;
    result->open_fn = 0ull;
    result->close_fn = 0ull;
    result->exit_fn = 0ull;
    result->memcpy_fn = 0ull;
    result->memset_fn = 0ull;
    result->strlen_fn = 0ull;
    result->puts_fn = 0ull;
    result->printf_fn = 0ull;
    result->malloc_fn = 0ull;
    result->abort_fn = 0ull;
    result->getenv_fn = 0ull;
    result->setenv_fn = 0ull;
    result->errno_location_fn = 0ull;
    result->errno_cell = 0ull;
    result->pthread_create_fn = 0ull;
    result->pthread_join_fn = 0ull;
    result->pthread_exit_fn = 0ull;
    result->pthread_mutex_lock_fn = 0ull;
    result->pthread_mutex_unlock_fn = 0ull;
    result->pthread_cond_init_fn = 0ull;
    result->pthread_cond_destroy_fn = 0ull;
    result->pthread_cond_wait_fn = 0ull;
    result->pthread_cond_signal_fn = 0ull;
    result->pthread_cond_broadcast_fn = 0ull;
    result->pthread_key_create_fn = 0ull;
    result->pthread_setspecific_fn = 0ull;
    result->pthread_getspecific_fn = 0ull;
    result->file_bytes = 0u;
    result->image_bytes = 0u;
    result->symbol_count = 0u;
    result->syscall_symbol_count = 0u;
    result->memory_symbol_count = 0u;
    result->string_symbol_count = 0u;
    result->stdio_symbol_count = 0u;
    result->heap_symbol_count = 0u;
    result->abort_symbol_count = 0u;
    result->env_symbol_count = 0u;
    result->unavailable_symbol_count = 0u;
    result->errno_symbol_count = 0u;
    result->pthread_create_symbol_count = 0u;
    result->pthread_join_symbol_count = 0u;
    result->pthread_exit_symbol_count = 0u;
    result->pthread_mutex_symbol_count = 0u;
    result->pthread_cond_symbol_count = 0u;
    result->pthread_tls_symbol_count = 0u;
    result->errno_page_mapped = 0u;
    result->image_checksum = 0u;
    result->text_checksum = 0u;
    result->rodata_checksum = 0u;
    result->name_checksum = 0u;
    result->text_protection = 0u;
    result->context_stored = 0u;
    result->error = LINUX_LIBC64_ERROR_NONE;
}

static u32 linux_libc64_valid_persona(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_LINUX_ELF))
        ? 1u
        : 0u;
}

static u32 linux_libc64_record_denial(u32 pid, u32 error, u64 rip)
{
    ++g_linux_libc64_denial_count;
    g_linux_libc64_last_error = error;
    if (pid != PROCESS64_INVALID_PID)
    {
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)error,
            LINUX_ABI64_ENOSYS,
            rip);
    }
    return LINUX_LIBC64_DENIED;
}

void linux_libc64_init(void)
{
    linux_libc64_build_image();
}

u32 linux_libc64_dependency_supported(const char *name, u32 length)
{
    static const char libc_so[] = "libc.so";
    static const char libc_so_6[] = "libc.so.6";
    static const char libc_x64_so[] = "libc-x64.so";
    static const char musl_so[] = "libc.musl-x86_64.so.1";
    u32 supported;

    supported =
        ((linux_libc64_name_matches(name, length, libc_so, (u32)sizeof(libc_so) - 1u) != 0u)
            || (linux_libc64_name_matches(name, length, libc_so_6, (u32)sizeof(libc_so_6) - 1u) != 0u)
            || (linux_libc64_name_matches(name, length, libc_x64_so, (u32)sizeof(libc_x64_so) - 1u) != 0u)
            || (linux_libc64_name_matches(name, length, musl_so, (u32)sizeof(musl_so) - 1u) != 0u)
            || (linux_libc64_pthread_dependency_supported(name, length) != 0u))
            ? 1u
            : 0u;
    if (supported != 0u)
    {
        ++g_linux_libc64_dependency_supported_count;
    }
    return supported;
}

u32 linux_libc64_pthread_dependency_supported(const char *name, u32 length)
{
    static const char pthread_so[] = "libpthread.so";
    static const char pthread_so_0[] = "libpthread.so.0";

    return ((linux_libc64_name_matches(name, length, pthread_so, (u32)sizeof(pthread_so) - 1u) != 0u)
        || (linux_libc64_name_matches(name, length, pthread_so_0, (u32)sizeof(pthread_so_0) - 1u) != 0u))
        ? 1u
        : 0u;
}

u32 linux_libc64_symbol_supported(const char *name, u32 length)
{
    u32 index;

    if ((name == 0) || (length == 0u) || (length > LINUX_LIBC64_STRING_LIMIT))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_LIBC64_SYMBOL_COUNT; ++index)
    {
        if (linux_libc64_name_matches(
                name,
                length,
                g_linux_libc64_exports[index].name,
                g_linux_libc64_exports[index].length) != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

u32 linux_libc64_symbol_unavailable(const char *name, u32 length)
{
    u32 index;

    if ((name == 0) || (length == 0u) || (length > LINUX_LIBC64_STRING_LIMIT))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_LIBC64_SYMBOL_COUNT; ++index)
    {
        if ((g_linux_libc64_exports[index].kind == LINUX_LIBC64_KIND_UNAVAILABLE)
            && (linux_libc64_name_matches(
                    name,
                    length,
                    g_linux_libc64_exports[index].name,
                    g_linux_libc64_exports[index].length) != 0u))
        {
            return 1u;
        }
    }

    return 0u;
}

u32 linux_libc64_symbol_default_address(
    const char *name,
    u32 length,
    u64 *out_address,
    u32 *out_unavailable)
{
    u32 index;

    if (out_address != 0)
    {
        *out_address = 0ull;
    }
    if (out_unavailable != 0)
    {
        *out_unavailable = 0u;
    }
    if ((name == 0)
        || (length == 0u)
        || (length > LINUX_LIBC64_STRING_LIMIT)
        || (out_address == 0)
        || (out_unavailable == 0))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_LIBC64_SYMBOL_COUNT; ++index)
    {
        if (linux_libc64_name_matches(
                name,
                length,
                g_linux_libc64_exports[index].name,
                g_linux_libc64_exports[index].length) != 0u)
        {
            *out_address = LINUX_LIBC64_DEFAULT_BASE + (u64)g_linux_libc64_exports[index].rva;
            *out_unavailable =
                (g_linux_libc64_exports[index].kind == LINUX_LIBC64_KIND_UNAVAILABLE) ? 1u : 0u;
            return 1u;
        }
    }

    return 0u;
}

u32 linux_libc64_load(
    u32 pid,
    u64 image_base,
    linux_libc64_load_result_t *out_result)
{
    elf64_program_header_t phdrs[ELF64_MAX_PROGRAM_HEADERS];
    persona_context_t *context;
    u64 errno_page;

    linux_libc64_clear_load(out_result);
    if (out_result == 0)
    {
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_NULL, 0ull);
    }
    if (linux_libc64_valid_persona(pid) == 0u)
    {
        out_result->error = LINUX_LIBC64_ERROR_PERSONA;
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_PERSONA, 0ull);
    }
    if ((image_base == 0ull) || ((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        out_result->error = LINUX_LIBC64_ERROR_BASE;
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_BASE, 0ull);
    }
    context = persona64_context_for_process(pid);
    if ((context != 0) && (context->linux_libc_base != 0ull))
    {
        out_result->error = LINUX_LIBC64_ERROR_ALREADY_MAPPED;
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_ALREADY_MAPPED, 0ull);
    }

    linux_libc64_init();
    if (elf64_parse_header(
            g_linux_libc64_image,
            LINUX_LIBC64_FILE_BYTES,
            &out_result->header) != ELF64_OK)
    {
        out_result->error = LINUX_LIBC64_ERROR_HEADER;
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_HEADER, 0ull);
    }
    if (elf64_parse_phdrs(
            g_linux_libc64_image,
            LINUX_LIBC64_FILE_BYTES,
            &out_result->header,
            phdrs,
            ELF64_MAX_PROGRAM_HEADERS,
            &out_result->phdr_summary) != ELF64_OK)
    {
        out_result->error = LINUX_LIBC64_ERROR_PHDR;
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_PHDR, 0ull);
    }
    if ((out_result->phdr_summary.load_count != 1u)
        || (out_result->phdr_summary.dynamic_count != 1u)
        || (elf64_map_load_segments(
                pid,
                phdrs,
                out_result->header.phnum,
                g_linux_libc64_image,
                LINUX_LIBC64_FILE_BYTES,
                image_base,
                &out_result->load_result) != ELF64_OK))
    {
        out_result->error = LINUX_LIBC64_ERROR_MAP;
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_MAP, 0ull);
    }
    errno_page = vma64_map_anon(
        pid,
        image_base + LINUX_LIBC64_DATA_RVA,
        VMA64_PAGE_BYTES,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS);
    if (errno_page != (image_base + LINUX_LIBC64_DATA_RVA))
    {
        (void)vma64_unmap(pid, image_base + LINUX_LIBC64_TEXT_RVA, VMA64_PAGE_BYTES);
        out_result->error = LINUX_LIBC64_ERROR_MAP;
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_MAP, 0ull);
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + LINUX_LIBC64_IMAGE_BYTES;
    out_result->read_fn = image_base + LINUX_LIBC64_RVA_READ;
    out_result->write_fn = image_base + LINUX_LIBC64_RVA_WRITE;
    out_result->open_fn = image_base + LINUX_LIBC64_RVA_OPEN;
    out_result->close_fn = image_base + LINUX_LIBC64_RVA_CLOSE;
    out_result->exit_fn = image_base + LINUX_LIBC64_RVA_EXIT;
    out_result->memcpy_fn = image_base + LINUX_LIBC64_RVA_MEMCPY;
    out_result->memset_fn = image_base + LINUX_LIBC64_RVA_MEMSET;
    out_result->strlen_fn = image_base + LINUX_LIBC64_RVA_STRLEN;
    out_result->puts_fn = image_base + LINUX_LIBC64_RVA_PUTS;
    out_result->printf_fn = image_base + LINUX_LIBC64_RVA_PRINTF;
    out_result->malloc_fn = image_base + LINUX_LIBC64_RVA_MALLOC;
    out_result->abort_fn = image_base + LINUX_LIBC64_RVA_ABORT;
    out_result->getenv_fn = image_base + LINUX_LIBC64_RVA_GETENV;
    out_result->setenv_fn = image_base + LINUX_LIBC64_RVA_SETENV;
    out_result->errno_location_fn = image_base + LINUX_LIBC64_RVA_ERRNO_LOCATION;
    out_result->errno_cell = image_base + LINUX_LIBC64_ERRNO_RVA;
    out_result->pthread_create_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_CREATE;
    out_result->pthread_join_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_JOIN;
    out_result->pthread_exit_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_EXIT;
    out_result->pthread_mutex_lock_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_MUTEX_LOCK;
    out_result->pthread_mutex_unlock_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_MUTEX_UNLOCK;
    out_result->pthread_cond_init_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_COND_INIT;
    out_result->pthread_cond_destroy_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_COND_DESTROY;
    out_result->pthread_cond_wait_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_COND_WAIT;
    out_result->pthread_cond_signal_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_COND_SIGNAL;
    out_result->pthread_cond_broadcast_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_COND_BROADCAST;
    out_result->pthread_key_create_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_KEY_CREATE;
    out_result->pthread_setspecific_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_SETSPECIFIC;
    out_result->pthread_getspecific_fn = image_base + LINUX_LIBC64_RVA_PTHREAD_GETSPECIFIC;
    out_result->file_bytes = LINUX_LIBC64_FILE_BYTES;
    out_result->image_bytes = LINUX_LIBC64_IMAGE_BYTES;
    out_result->symbol_count = LINUX_LIBC64_SYMBOL_COUNT;
    out_result->syscall_symbol_count = LINUX_LIBC64_SYSCALL_SYMBOL_COUNT;
    out_result->memory_symbol_count = LINUX_LIBC64_MEMORY_SYMBOL_COUNT;
    out_result->string_symbol_count = LINUX_LIBC64_STRING_SYMBOL_COUNT;
    out_result->stdio_symbol_count = LINUX_LIBC64_STDIO_SYMBOL_COUNT;
    out_result->heap_symbol_count = LINUX_LIBC64_HEAP_SYMBOL_COUNT;
    out_result->abort_symbol_count = LINUX_LIBC64_ABORT_SYMBOL_COUNT;
    out_result->env_symbol_count = LINUX_LIBC64_ENV_SYMBOL_COUNT;
    out_result->unavailable_symbol_count = LINUX_LIBC64_UNAVAILABLE_SYMBOL_COUNT;
    out_result->errno_symbol_count = LINUX_LIBC64_ERRNO_SYMBOL_COUNT;
    out_result->pthread_create_symbol_count = LINUX_LIBC64_PTHREAD_CREATE_SYMBOL_COUNT;
    out_result->pthread_join_symbol_count = LINUX_LIBC64_PTHREAD_JOIN_SYMBOL_COUNT;
    out_result->pthread_exit_symbol_count = LINUX_LIBC64_PTHREAD_EXIT_SYMBOL_COUNT;
    out_result->pthread_mutex_symbol_count = LINUX_LIBC64_PTHREAD_MUTEX_SYMBOL_COUNT;
    out_result->pthread_cond_symbol_count = LINUX_LIBC64_PTHREAD_COND_SYMBOL_COUNT;
    out_result->pthread_tls_symbol_count = LINUX_LIBC64_PTHREAD_TLS_SYMBOL_COUNT;
    out_result->errno_page_mapped = 1u;
    out_result->image_checksum = linux_libc64_checksum_bytes(
        g_linux_libc64_image,
        LINUX_LIBC64_FILE_BYTES);
    out_result->text_checksum = linux_libc64_checksum_bytes(
        g_linux_libc64_image + LINUX_LIBC64_TEXT_FILE_OFFSET,
        LINUX_LIBC64_TEXT_FILE_BYTES);
    out_result->rodata_checksum = linux_libc64_checksum_bytes(
        g_linux_libc64_image + LINUX_LIBC64_RODATA_FILE_OFFSET,
        LINUX_LIBC64_RODATA_FILE_BYTES);
    out_result->name_checksum = linux_libc64_checksum_bytes(
        g_linux_libc64_image + LINUX_LIBC64_RODATA_FILE_OFFSET,
        (u32)sizeof("libc.so") - 1u);
    out_result->text_protection = paging64_user_page_protection(image_base + LINUX_LIBC64_TEXT_RVA);

    if (context != 0)
    {
        context->linux_libc_base = image_base;
        context->linux_libc_write = out_result->write_fn;
        context->linux_libc_read = out_result->read_fn;
        context->linux_libc_exit = out_result->exit_fn;
        context->linux_libc_strlen = out_result->strlen_fn;
        context->linux_libc_envp = 0ull;
        context->linux_libc_envc = 0u;
        context->linux_libc_environment_bound = 0u;
        context->linux_libc_symbol_count = LINUX_LIBC64_SYMBOL_COUNT;
        context->linux_libc_checksum = out_result->image_checksum;
        context->linux_libc_unavailable_count = LINUX_LIBC64_UNAVAILABLE_SYMBOL_COUNT;
        out_result->context_stored = 1u;
    }

    if ((out_result->write_fn == 0ull)
        || (out_result->strlen_fn == 0ull)
        || (out_result->printf_fn == 0ull))
    {
        out_result->error = LINUX_LIBC64_ERROR_SYMBOL;
        (void)linux_libc64_release_process(pid);
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_SYMBOL, 0ull);
    }

    ++g_linux_libc64_load_count;
    g_linux_libc64_last_error = LINUX_LIBC64_ERROR_NONE;
    out_result->error = LINUX_LIBC64_ERROR_NONE;
    return LINUX_LIBC64_OK;
}

u32 linux_libc64_bind_environment(
    u32 pid,
    u64 envp_address,
    u32 envc,
    const char *const *envp_source)
{
    persona_context_t *context;
    u64 text_page;
    u64 patch_address;
    u64 setenv_patch_address;
    u64 snapshot_envp;
    u64 snapshot_string;
    volatile u8 *patch;
    volatile u8 *count_patch;
    volatile u8 *setenv_patch;
    volatile u8 *setenv_count_patch;
    volatile u8 *snapshot_vector;
    volatile u8 *snapshot_bytes;
    u64 readback;
    u64 setenv_readback;
    u64 vector_readback;
    u64 vector_terminator;
    u32 count_readback;
    u32 setenv_count_readback;
    u32 snapshot_count;
    u32 snapshot_offset;
    u32 vector_bytes;
    u32 restore_result;
    u32 index;

    if (linux_libc64_valid_persona(pid) == 0u)
    {
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_ENVIRONMENT, 0ull);
    }
    if ((envc != 0u) && ((envp_address == 0ull) || (envp_source == 0)))
    {
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_ENVIRONMENT, 0ull);
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->linux_libc_base == 0ull))
    {
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_ENVIRONMENT, 0ull);
    }

    text_page = context->linux_libc_base + LINUX_LIBC64_TEXT_RVA;
    snapshot_envp = context->linux_libc_base + LINUX_LIBC64_RVA_ENV_VECTOR;
    snapshot_string = context->linux_libc_base + LINUX_LIBC64_RVA_ENV_STRING;
    snapshot_count =
        (envc > LINUX_LIBC64_ENV_SNAPSHOT_COUNT)
            ? LINUX_LIBC64_ENV_SNAPSHOT_COUNT
            : envc;
    vector_bytes = (LINUX_LIBC64_ENV_SNAPSHOT_COUNT + 1u) * 8u;
    patch_address =
        context->linux_libc_base
        + LINUX_LIBC64_RVA_HELPER_GETENV
        + LINUX_LIBC64_GETENV_ENVP_IMM_OFFSET;
    setenv_patch_address =
        context->linux_libc_base
        + LINUX_LIBC64_RVA_HELPER_SETENV
        + LINUX_LIBC64_SETENV_ENVP_IMM_OFFSET;
    if (vma64_protect(
            pid,
            text_page,
            VMA64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE | VMA64_PROT_EXECUTE) == 0u)
    {
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_ENVIRONMENT, 0ull);
    }

    patch = (volatile u8 *)(u64)patch_address;
    count_patch = (volatile u8 *)(u64)(
        context->linux_libc_base
        + LINUX_LIBC64_RVA_HELPER_GETENV
        + LINUX_LIBC64_GETENV_ENVC_IMM_OFFSET);
    setenv_patch = (volatile u8 *)(u64)setenv_patch_address;
    setenv_count_patch = (volatile u8 *)(u64)(
        context->linux_libc_base
        + LINUX_LIBC64_RVA_HELPER_SETENV
        + LINUX_LIBC64_SETENV_ENVC_IMM_OFFSET);
    snapshot_vector = (volatile u8 *)(u64)snapshot_envp;
    snapshot_bytes = (volatile u8 *)(u64)snapshot_string;

    for (index = 0u; index < vector_bytes; ++index)
    {
        snapshot_vector[index] = 0u;
    }
    for (index = 0u; index < LINUX_LIBC64_ENV_SNAPSHOT_BYTES; ++index)
    {
        snapshot_bytes[index] = 0u;
    }
    snapshot_offset = 0u;
    for (index = 0u; index < snapshot_count; ++index)
    {
        u32 remaining = LINUX_LIBC64_ENV_SNAPSHOT_BYTES - snapshot_offset;
        u32 length;

        if ((snapshot_offset >= LINUX_LIBC64_ENV_SNAPSHOT_BYTES)
            || (envp_source[index] == 0)
            || (linux_libc64_env_string_valid(envp_source[index], remaining) == 0u)
            || (linux_libc64_copy_env_string(
                    snapshot_bytes + snapshot_offset,
                    envp_source[index],
                    remaining) == 0u))
        {
            restore_result = vma64_protect(
                pid,
                text_page,
                VMA64_PAGE_BYTES,
                VMA64_PROT_READ | VMA64_PROT_EXECUTE);
            return linux_libc64_record_denial(
                pid,
                LINUX_LIBC64_ERROR_ENVIRONMENT,
                0ull);
        }

        linux_libc64_write_le64_volatile(
            snapshot_vector + (index * 8u),
            snapshot_string + (u64)snapshot_offset);
        length = linux_libc64_cstring_length(envp_source[index], remaining);
        snapshot_offset += length + 1u;
    }

    linux_libc64_write_le64_volatile(snapshot_vector + (snapshot_count * 8u), 0ull);
    linux_libc64_write_le64_volatile(patch, snapshot_envp);
    linux_libc64_write_le32_volatile(count_patch, snapshot_count);
    linux_libc64_write_le64_volatile(setenv_patch, snapshot_envp);
    linux_libc64_write_le32_volatile(setenv_count_patch, snapshot_count);

    readback = linux_libc64_read_le64_volatile(patch);
    count_readback = linux_libc64_read_le32_volatile(count_patch);
    setenv_readback = linux_libc64_read_le64_volatile(setenv_patch);
    setenv_count_readback = linux_libc64_read_le32_volatile(setenv_count_patch);
    vector_readback = linux_libc64_read_le64_volatile(snapshot_vector);
    vector_terminator =
        linux_libc64_read_le64_volatile(snapshot_vector + (snapshot_count * 8u));
    restore_result = vma64_protect(
        pid,
        text_page,
        VMA64_PAGE_BYTES,
        VMA64_PROT_READ | VMA64_PROT_EXECUTE);
    if ((restore_result == 0u)
        || (readback != snapshot_envp)
        || (count_readback != snapshot_count)
        || (setenv_readback != snapshot_envp)
        || (setenv_count_readback != snapshot_count)
        || (vector_readback != ((snapshot_count != 0u) ? snapshot_string : 0ull))
        || (vector_terminator != 0ull)
        || (paging64_user_page_protection_for_process(pid, text_page)
            != (PAGING64_USER_PROT_READ | PAGING64_USER_PROT_EXECUTE)))
    {
        return linux_libc64_record_denial(pid, LINUX_LIBC64_ERROR_ENVIRONMENT, 0ull);
    }

    context->linux_libc_envp = snapshot_envp;
    context->linux_libc_envc = snapshot_count;
    context->linux_libc_environment_bound = 1u;
    return LINUX_LIBC64_OK;
}

u64 linux_libc64_export(u32 pid, const char *name)
{
    persona_context_t *context;
    u32 length;
    u32 index;
    u64 base;

    if (name == 0)
    {
        return 0ull;
    }

    context = persona64_context_for_process(pid);
    base = (context != 0) ? context->linux_libc_base : 0ull;
    if (base == 0ull)
    {
        return 0ull;
    }

    length = linux_libc64_cstring_length(name, LINUX_LIBC64_STRING_LIMIT);
    for (index = 0u; index < LINUX_LIBC64_SYMBOL_COUNT; ++index)
    {
        if (linux_libc64_name_matches(
                name,
                length,
                g_linux_libc64_exports[index].name,
                g_linux_libc64_exports[index].length) != 0u)
        {
            return base + (u64)g_linux_libc64_exports[index].rva;
        }
    }

    return 0ull;
}

u32 linux_libc64_release_process(u32 pid)
{
    persona_context_t *context;
    u64 base;
    u32 released = 0u;

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        return 0u;
    }

    base = context->linux_libc_base;
    if ((base != 0ull) && (vma64_find(pid, base + LINUX_LIBC64_TEXT_RVA) != 0))
    {
        released += vma64_unmap(pid, base + LINUX_LIBC64_TEXT_RVA, VMA64_PAGE_BYTES);
    }
    if ((base != 0ull) && (vma64_find(pid, base + LINUX_LIBC64_DATA_RVA) != 0))
    {
        released += vma64_unmap(pid, base + LINUX_LIBC64_DATA_RVA, VMA64_PAGE_BYTES);
    }

    context->linux_libc_base = 0ull;
    context->linux_libc_write = 0ull;
    context->linux_libc_read = 0ull;
    context->linux_libc_exit = 0ull;
    context->linux_libc_strlen = 0ull;
    context->linux_libc_envp = 0ull;
    context->linux_libc_envc = 0u;
    context->linux_libc_environment_bound = 0u;
    context->linux_libc_symbol_count = 0u;
    context->linux_libc_checksum = 0u;
    context->linux_libc_unavailable_count = 0u;
    return released;
}

const u8 *linux_libc64_image(void)
{
    linux_libc64_init();
    return g_linux_libc64_image;
}

u32 linux_libc64_file_bytes(void)
{
    return LINUX_LIBC64_FILE_BYTES;
}

u32 linux_libc64_symbol_count(void)
{
    return LINUX_LIBC64_SYMBOL_COUNT;
}

u32 linux_libc64_syscall_symbol_count(void)
{
    return LINUX_LIBC64_SYSCALL_SYMBOL_COUNT;
}

u32 linux_libc64_memory_symbol_count(void)
{
    return LINUX_LIBC64_MEMORY_SYMBOL_COUNT;
}

u32 linux_libc64_string_symbol_count(void)
{
    return LINUX_LIBC64_STRING_SYMBOL_COUNT;
}

u32 linux_libc64_stdio_symbol_count(void)
{
    return LINUX_LIBC64_STDIO_SYMBOL_COUNT;
}

u32 linux_libc64_heap_symbol_count(void)
{
    return LINUX_LIBC64_HEAP_SYMBOL_COUNT;
}

u32 linux_libc64_abort_symbol_count(void)
{
    return LINUX_LIBC64_ABORT_SYMBOL_COUNT;
}

u32 linux_libc64_env_symbol_count(void)
{
    return LINUX_LIBC64_ENV_SYMBOL_COUNT;
}

u32 linux_libc64_errno_symbol_count(void)
{
    return LINUX_LIBC64_ERRNO_SYMBOL_COUNT;
}

u32 linux_libc64_pthread_create_symbol_count(void)
{
    return LINUX_LIBC64_PTHREAD_CREATE_SYMBOL_COUNT;
}

u32 linux_libc64_pthread_join_symbol_count(void)
{
    return LINUX_LIBC64_PTHREAD_JOIN_SYMBOL_COUNT;
}

u32 linux_libc64_pthread_exit_symbol_count(void)
{
    return LINUX_LIBC64_PTHREAD_EXIT_SYMBOL_COUNT;
}

u32 linux_libc64_pthread_mutex_symbol_count(void)
{
    return LINUX_LIBC64_PTHREAD_MUTEX_SYMBOL_COUNT;
}

u32 linux_libc64_pthread_cond_symbol_count(void)
{
    return LINUX_LIBC64_PTHREAD_COND_SYMBOL_COUNT;
}

u32 linux_libc64_pthread_tls_symbol_count(void)
{
    return LINUX_LIBC64_PTHREAD_TLS_SYMBOL_COUNT;
}

u32 linux_libc64_unavailable_symbol_count(void)
{
    return LINUX_LIBC64_UNAVAILABLE_SYMBOL_COUNT;
}

u32 linux_libc64_dependency_supported_count(void)
{
    return g_linux_libc64_dependency_supported_count;
}

u32 linux_libc64_load_count(void)
{
    return g_linux_libc64_load_count;
}

u32 linux_libc64_denial_count(void)
{
    return g_linux_libc64_denial_count;
}

u32 linux_libc64_last_error(void)
{
    return g_linux_libc64_last_error;
}
