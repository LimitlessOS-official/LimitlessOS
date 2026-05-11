#ifndef LIMITLESS_SHELL_X64_H
#define LIMITLESS_SHELL_X64_H

#include "types.h"

#define SHELL64_INVALID_RESULT 0xFFFFFFFFu

u32 shell64_execute_line(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u64 line_address,
    u32 line_byte_count,
    u32 owner_id);

#endif
