#ifndef LIMITLESS_MEMORY_H
#define LIMITLESS_MEMORY_H

#include "boot_info.h"
#include "types.h"

void memory_init(const struct boot_info *boot_info);
void *memory_early_alloc(u32 size, u32 alignment);
u32 memory_claim_frame(void);
void memory_release_frame(u32 physical_address);
u32 memory_get_conventional_bytes(void);
u32 memory_get_extended_bytes(void);
u32 memory_get_total_bytes(void);
u32 memory_get_free_bytes(void);
u32 memory_get_frame_count(void);
u32 memory_get_free_frame_count(void);
u32 memory_get_heap_base(void);
u32 memory_get_heap_limit(void);

#endif
