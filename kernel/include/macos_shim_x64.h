#ifndef LIMITLESS_MACOS_SHIM_X64_H
#define LIMITLESS_MACOS_SHIM_X64_H

#include "types.h"

#define MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE 0x0000000047300000ull
#define MACOS_SHIM64_LIBSYSTEM_IMAGE_BYTES 0x00003000u
#define MACOS_SHIM64_LIBSYSTEM_TEXT_RVA 0x00001000u
#define MACOS_SHIM64_LIBSYSTEM_RODATA_RVA 0x00002000u
#define MACOS_SHIM64_LIBSYSTEM_PAGE_BYTES 0x00001000u
#define MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT 16u
#define MACOS_SHIM64_LIBSYSTEM_HEAP_BASE 0x0000000047340000ull
#define MACOS_SHIM64_LIBSYSTEM_HEAP_BYTES 0x00004000u
#define MACOS_SHIM64_MAX_HEAPS 8u
#define MACOS_SHIM64_MAX_ALLOCATIONS 32u
#define MACOS_SHIM64_STRING_LIMIT 256u
#define MACOS_SHIM64_TEXT_PATTERN_BYTES 8u

#define MACOS_SHIM64_RVA_WRITE 0x00001000u
#define MACOS_SHIM64_RVA_READ 0x00001020u
#define MACOS_SHIM64_RVA_OPEN 0x00001040u
#define MACOS_SHIM64_RVA_CLOSE 0x00001060u
#define MACOS_SHIM64_RVA_EXIT 0x00001080u
#define MACOS_SHIM64_RVA_MMAP 0x000010A0u
#define MACOS_SHIM64_RVA_MUNMAP 0x000010C0u
#define MACOS_SHIM64_RVA_MPROTECT 0x000010E0u
#define MACOS_SHIM64_RVA_MALLOC 0x00001100u
#define MACOS_SHIM64_RVA_FREE 0x00001120u
#define MACOS_SHIM64_RVA_REALLOC 0x00001140u
#define MACOS_SHIM64_RVA_MEMCPY 0x00001160u
#define MACOS_SHIM64_RVA_MEMSET 0x00001180u
#define MACOS_SHIM64_RVA_STRLEN 0x000011A0u
#define MACOS_SHIM64_RVA_PRINTF 0x000011C0u
#define MACOS_SHIM64_RVA_CLOCK_GETTIME 0x000011E0u

#define MACOS_SHIM64_ADDR_WRITE \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_WRITE)
#define MACOS_SHIM64_ADDR_READ \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_READ)
#define MACOS_SHIM64_ADDR_OPEN \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_OPEN)
#define MACOS_SHIM64_ADDR_CLOSE \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_CLOSE)
#define MACOS_SHIM64_ADDR_EXIT \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_EXIT)
#define MACOS_SHIM64_ADDR_MMAP \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_MMAP)
#define MACOS_SHIM64_ADDR_MUNMAP \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_MUNMAP)
#define MACOS_SHIM64_ADDR_MPROTECT \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_MPROTECT)
#define MACOS_SHIM64_ADDR_MALLOC \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_MALLOC)
#define MACOS_SHIM64_ADDR_FREE \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_FREE)
#define MACOS_SHIM64_ADDR_REALLOC \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_REALLOC)
#define MACOS_SHIM64_ADDR_MEMCPY \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_MEMCPY)
#define MACOS_SHIM64_ADDR_MEMSET \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_MEMSET)
#define MACOS_SHIM64_ADDR_STRLEN \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_STRLEN)
#define MACOS_SHIM64_ADDR_PRINTF \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_PRINTF)
#define MACOS_SHIM64_ADDR_CLOCK_GETTIME \
    (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_RVA_CLOCK_GETTIME)

#define MACOS_SHIM64_OK 1u
#define MACOS_SHIM64_DENIED 0u

#define MACOS_SHIM64_ERROR_NONE 0u
#define MACOS_SHIM64_ERROR_NULL 1u
#define MACOS_SHIM64_ERROR_PERSONA 2u
#define MACOS_SHIM64_ERROR_BASE 3u
#define MACOS_SHIM64_ERROR_MAP 4u
#define MACOS_SHIM64_ERROR_ALREADY_MAPPED 5u
#define MACOS_SHIM64_ERROR_SYMBOL 6u
#define MACOS_SHIM64_ERROR_FAULT 7u
#define MACOS_SHIM64_ERROR_HEAP 8u
#define MACOS_SHIM64_ERROR_RANGE 9u

#define MACOS_SHIM64_SYMBOL_NONE 0u
#define MACOS_SHIM64_SYMBOL_WRITE 1u
#define MACOS_SHIM64_SYMBOL_READ 2u
#define MACOS_SHIM64_SYMBOL_OPEN 3u
#define MACOS_SHIM64_SYMBOL_CLOSE 4u
#define MACOS_SHIM64_SYMBOL_EXIT 5u
#define MACOS_SHIM64_SYMBOL_MMAP 6u
#define MACOS_SHIM64_SYMBOL_MUNMAP 7u
#define MACOS_SHIM64_SYMBOL_MPROTECT 8u
#define MACOS_SHIM64_SYMBOL_MALLOC 9u
#define MACOS_SHIM64_SYMBOL_FREE 10u
#define MACOS_SHIM64_SYMBOL_REALLOC 11u
#define MACOS_SHIM64_SYMBOL_MEMCPY 12u
#define MACOS_SHIM64_SYMBOL_MEMSET 13u
#define MACOS_SHIM64_SYMBOL_STRLEN 14u
#define MACOS_SHIM64_SYMBOL_PRINTF 15u
#define MACOS_SHIM64_SYMBOL_CLOCK_GETTIME 16u

typedef struct macos_shim64_libsystem_result
{
    u64 image_base;
    u64 image_end;
    u64 write_fn;
    u64 read_fn;
    u64 open_fn;
    u64 close_fn;
    u64 exit_fn;
    u64 mmap_fn;
    u64 munmap_fn;
    u64 mprotect_fn;
    u64 malloc_fn;
    u64 free_fn;
    u64 realloc_fn;
    u64 memcpy_fn;
    u64 memset_fn;
    u64 strlen_fn;
    u64 printf_fn;
    u64 clock_gettime_fn;
    u32 image_bytes;
    u32 section_count;
    u32 mapped_count;
    u32 symbol_count;
    u32 text_checksum;
    u32 rodata_checksum;
    u32 name_checksum;
    u32 text_protection;
    u32 rodata_protection;
    u32 context_stored;
    u32 error;
} macos_shim64_libsystem_result_t;

typedef struct macos_shim64_call_result
{
    u64 value;
    u32 symbol_id;
    u32 error;
    u32 byte_count;
    u32 checksum;
} macos_shim64_call_result_t;

void macos_shim64_init(void);
u32 macos_shim64_load_libsystem(
    u32 pid,
    u64 image_base,
    macos_shim64_libsystem_result_t *out_result);
u64 macos_shim64_resolve_libsystem_symbol(const char *name, u32 name_length);
u32 macos_shim64_symbol_id_for_address(u64 address);
u32 macos_shim64_call(
    u32 pid,
    u64 shim_address,
    u64 arg0,
    u64 arg1,
    u64 arg2,
    u64 arg3,
    u64 arg4,
    u64 arg5,
    u64 rip,
    macos_shim64_call_result_t *out_result);
u32 macos_shim64_release_process(u32 pid);
u32 macos_shim64_symbol_count(void);
u32 macos_shim64_load_count(void);
u32 macos_shim64_call_count(void);
u32 macos_shim64_syscall_bridge_count(void);
u32 macos_shim64_memory_call_count(void);
u32 macos_shim64_denial_count(void);
u32 macos_shim64_fault_count(void);
u32 macos_shim64_last_symbol(void);
u32 macos_shim64_last_error(void);
u64 macos_shim64_last_result(void);
u32 macos_shim64_last_byte_count(void);
u32 macos_shim64_last_checksum(void);

#endif
