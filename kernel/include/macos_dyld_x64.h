#ifndef LIMITLESS_MACOS_DYLD_X64_H
#define LIMITLESS_MACOS_DYLD_X64_H

#include "types.h"

#define MACOS_DYLD64_DEFAULT_BASE 0x0000000047310000ull
#define MACOS_DYLD64_IMAGE_BYTES 0x00003000u
#define MACOS_DYLD64_TEXT_RVA 0x00001000u
#define MACOS_DYLD64_RODATA_RVA 0x00002000u
#define MACOS_DYLD64_PAGE_BYTES 0x00001000u
#define MACOS_DYLD64_SYMBOL_COUNT 3u
#define MACOS_DYLD64_IMAGE_COUNT 3u
#define MACOS_DYLD64_TEXT_PATTERN_BYTES 8u
#define MACOS_DYLD64_STRING_LIMIT 96u

#define MACOS_DYLD64_RVA_STUB_BINDER 0x00001000u
#define MACOS_DYLD64_RVA_GET_IMAGE_NAME 0x00001020u
#define MACOS_DYLD64_RVA_IMAGE_COUNT 0x00001040u

#define MACOS_DYLD64_ADDR_STUB_BINDER \
    (MACOS_DYLD64_DEFAULT_BASE + (u64)MACOS_DYLD64_RVA_STUB_BINDER)
#define MACOS_DYLD64_ADDR_GET_IMAGE_NAME \
    (MACOS_DYLD64_DEFAULT_BASE + (u64)MACOS_DYLD64_RVA_GET_IMAGE_NAME)
#define MACOS_DYLD64_ADDR_IMAGE_COUNT \
    (MACOS_DYLD64_DEFAULT_BASE + (u64)MACOS_DYLD64_RVA_IMAGE_COUNT)

#define MACOS_DYLD64_OK 1u
#define MACOS_DYLD64_DENIED 0u

#define MACOS_DYLD64_ERROR_NONE 0u
#define MACOS_DYLD64_ERROR_NULL 1u
#define MACOS_DYLD64_ERROR_PERSONA 2u
#define MACOS_DYLD64_ERROR_BASE 3u
#define MACOS_DYLD64_ERROR_MAP 4u
#define MACOS_DYLD64_ERROR_ALREADY_MAPPED 5u
#define MACOS_DYLD64_ERROR_SYMBOL 6u
#define MACOS_DYLD64_ERROR_FAULT 7u
#define MACOS_DYLD64_ERROR_RANGE 8u
#define MACOS_DYLD64_ERROR_SHIM 9u

#define MACOS_DYLD64_SYMBOL_NONE 0u
#define MACOS_DYLD64_SYMBOL_STUB_BINDER 1u
#define MACOS_DYLD64_SYMBOL_GET_IMAGE_NAME 2u
#define MACOS_DYLD64_SYMBOL_IMAGE_COUNT 3u

typedef struct macos_dyld64_load_result
{
    u64 image_base;
    u64 image_end;
    u64 stub_binder_fn;
    u64 get_image_name_fn;
    u64 image_count_fn;
    u32 image_bytes;
    u32 section_count;
    u32 mapped_count;
    u32 symbol_count;
    u32 image_name_count;
    u32 text_checksum;
    u32 rodata_checksum;
    u32 name_checksum;
    u32 text_protection;
    u32 rodata_protection;
    u32 context_stored;
    u32 error;
} macos_dyld64_load_result_t;

typedef struct macos_dyld64_call_result
{
    u64 value;
    u32 symbol_id;
    u32 error;
    u32 byte_count;
    u32 checksum;
} macos_dyld64_call_result_t;

typedef struct macos_dyld64_lazy_bind_result
{
    u64 slot;
    u64 before;
    u64 after;
    u32 shim_id;
    u32 symbol_length;
    u32 symbol_checksum;
    u32 error;
} macos_dyld64_lazy_bind_result_t;

void macos_dyld64_init(void);
u32 macos_dyld64_load(
    u32 pid,
    u64 image_base,
    macos_dyld64_load_result_t *out_result);
u64 macos_dyld64_resolve_symbol(const char *name, u32 name_length);
u32 macos_dyld64_symbol_id_for_address(u64 address);
u32 macos_dyld64_call(
    u32 pid,
    u64 shim_address,
    u64 arg0,
    u64 arg1,
    u64 arg2,
    u64 arg3,
    u64 arg4,
    u64 arg5,
    u64 rip,
    macos_dyld64_call_result_t *out_result);
u32 macos_dyld64_bind_lazy(
    u32 pid,
    u64 slot,
    u32 shim_id,
    const char *symbol,
    u32 symbol_length,
    macos_dyld64_lazy_bind_result_t *out_result);
u32 macos_dyld64_release_process(u32 pid);
u32 macos_dyld64_symbol_count(void);
u32 macos_dyld64_image_count(void);
u32 macos_dyld64_load_count(void);
u32 macos_dyld64_call_count(void);
u32 macos_dyld64_lazy_bind_count(void);
u32 macos_dyld64_image_query_count(void);
u32 macos_dyld64_denial_count(void);
u32 macos_dyld64_fault_count(void);
u32 macos_dyld64_last_symbol(void);
u32 macos_dyld64_last_error(void);
u64 macos_dyld64_last_result(void);
u32 macos_dyld64_last_byte_count(void);
u32 macos_dyld64_last_checksum(void);

#endif
