#ifndef LIMITLESS_MACOS_CF_X64_H
#define LIMITLESS_MACOS_CF_X64_H

#include "types.h"

#define MACOS_CF64_DEFAULT_BASE 0x0000000047320000ull
#define MACOS_CF64_IMAGE_BYTES 0x00003000u
#define MACOS_CF64_TEXT_RVA 0x00001000u
#define MACOS_CF64_RODATA_RVA 0x00002000u
#define MACOS_CF64_PAGE_BYTES 0x00001000u
#define MACOS_CF64_SYMBOL_COUNT 6u
#define MACOS_CF64_TEXT_PATTERN_BYTES 8u
#define MACOS_CF64_STRING_LIMIT 128u
#define MACOS_CF64_OBJECT_POOL_SIZE 8u

#define MACOS_CF64_RVA_ALLOCATOR_GET_DEFAULT 0x00001000u
#define MACOS_CF64_RVA_RELEASE 0x00001020u
#define MACOS_CF64_RVA_RETAIN 0x00001040u
#define MACOS_CF64_RVA_STRING_CREATE 0x00001060u
#define MACOS_CF64_RVA_STRING_GET_CSTRING 0x00001080u
#define MACOS_CF64_RVA_SHOW 0x000010A0u

#define MACOS_CF64_ADDR_ALLOCATOR_GET_DEFAULT \
    (MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RVA_ALLOCATOR_GET_DEFAULT)
#define MACOS_CF64_ADDR_RELEASE \
    (MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RVA_RELEASE)
#define MACOS_CF64_ADDR_RETAIN \
    (MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RVA_RETAIN)
#define MACOS_CF64_ADDR_STRING_CREATE \
    (MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RVA_STRING_CREATE)
#define MACOS_CF64_ADDR_STRING_GET_CSTRING \
    (MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RVA_STRING_GET_CSTRING)
#define MACOS_CF64_ADDR_SHOW \
    (MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RVA_SHOW)

#define MACOS_CF64_ALLOCATOR_DEFAULT 0x0000000047322000ull
#define MACOS_CF64_OBJECT_HANDLE_BASE 0x0000000047323000ull
#define MACOS_CF64_OBJECT_HANDLE_STRIDE 0x0000000000000040ull
#define MACOS_CF64_SCRATCH_BASE 0x0000000047330000ull
#define MACOS_CF64_ENCODING_UTF8 0x08000100u

#define MACOS_CF64_OK 1u
#define MACOS_CF64_DENIED 0u

#define MACOS_CF64_ERROR_NONE 0u
#define MACOS_CF64_ERROR_NULL 1u
#define MACOS_CF64_ERROR_PERSONA 2u
#define MACOS_CF64_ERROR_BASE 3u
#define MACOS_CF64_ERROR_MAP 4u
#define MACOS_CF64_ERROR_ALREADY_MAPPED 5u
#define MACOS_CF64_ERROR_SYMBOL 6u
#define MACOS_CF64_ERROR_FAULT 7u
#define MACOS_CF64_ERROR_RANGE 8u
#define MACOS_CF64_ERROR_POOL 9u
#define MACOS_CF64_ERROR_TYPE 10u
#define MACOS_CF64_ERROR_ENCODING 11u
#define MACOS_CF64_ERROR_SHIM 12u

#define MACOS_CF64_SYMBOL_NONE 0u
#define MACOS_CF64_SYMBOL_ALLOCATOR_GET_DEFAULT 1u
#define MACOS_CF64_SYMBOL_RELEASE 2u
#define MACOS_CF64_SYMBOL_RETAIN 3u
#define MACOS_CF64_SYMBOL_STRING_CREATE 4u
#define MACOS_CF64_SYMBOL_STRING_GET_CSTRING 5u
#define MACOS_CF64_SYMBOL_SHOW 6u

typedef struct macos_cf64_load_result
{
    u64 image_base;
    u64 image_end;
    u64 allocator_get_default_fn;
    u64 release_fn;
    u64 retain_fn;
    u64 string_create_fn;
    u64 string_get_cstring_fn;
    u64 show_fn;
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
} macos_cf64_load_result_t;

typedef struct macos_cf64_call_result
{
    u64 value;
    u32 symbol_id;
    u32 error;
    u32 byte_count;
    u32 checksum;
} macos_cf64_call_result_t;

void macos_cf64_init(void);
u32 macos_cf64_load(u32 pid, u64 image_base, macos_cf64_load_result_t *out_result);
u64 macos_cf64_resolve_symbol(const char *name, u32 name_length);
u32 macos_cf64_symbol_id_for_address(u64 address);
u32 macos_cf64_call(
    u32 pid,
    u64 shim_address,
    u64 arg0,
    u64 arg1,
    u64 arg2,
    u64 arg3,
    u64 arg4,
    u64 arg5,
    u64 rip,
    macos_cf64_call_result_t *out_result);
u32 macos_cf64_release_process(u32 pid);
u32 macos_cf64_symbol_count(void);
u32 macos_cf64_load_count(void);
u32 macos_cf64_call_count(void);
u32 macos_cf64_string_create_count(void);
u32 macos_cf64_get_cstring_count(void);
u32 macos_cf64_show_count(void);
u32 macos_cf64_retain_count(void);
u32 macos_cf64_release_count(void);
u32 macos_cf64_denial_count(void);
u32 macos_cf64_fault_count(void);
u32 macos_cf64_scratch_map_count(void);
u32 macos_cf64_live_object_count(u32 pid);
u32 macos_cf64_last_symbol(void);
u32 macos_cf64_last_error(void);
u64 macos_cf64_last_result(void);
u32 macos_cf64_last_byte_count(void);
u32 macos_cf64_last_checksum(void);

#endif
