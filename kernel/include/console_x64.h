#ifndef LIMITLESS_CONSOLE_X64_H
#define LIMITLESS_CONSOLE_X64_H

#include "types.h"

#define CONSOLE64_INVALID_RESULT 0xFFFFFFFFu

void console64_init(void);
u32 console64_write(u32 console_capability_handle, u64 input_address, u32 byte_count, u32 owner_id);
u32 console64_write_kernel(
    u32 console_capability_handle,
    const u8 *input,
    u32 byte_count,
    u32 owner_id);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 console64_capture_begin(
    u32 console_capability_handle,
    u32 owner_id,
    u8 *buffer,
    u32 byte_capacity);
u32 console64_capture_end(
    u32 console_capability_handle,
    u32 owner_id,
    u32 *bytes_captured,
    u32 *truncated);
#endif
u32 console64_write_count(void);
u32 console64_byte_count(void);
u32 console64_denial_count(void);

#endif
