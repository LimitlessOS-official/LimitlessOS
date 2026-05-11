#ifndef LIMITLESS_BLOCK_X64_H
#define LIMITLESS_BLOCK_X64_H

#include "types.h"

#define BLOCK64_INVALID_RESULT 0xFFFFFFFFu
#define BLOCK64_SECTOR_BYTES 512u

void block64_init(void);
u32 block64_available(void);
u32 block64_last_status(void);
u32 block64_read_sector(u32 block_capability_handle, u32 lba, u64 output_address, u32 owner_id);
u32 block64_read_count(void);
u32 block64_byte_count(void);
u32 block64_denial_count(void);
u32 block64_unavailable_count(void);
u32 block64_last_lba(void);
u32 block64_last_token(void);

#endif
