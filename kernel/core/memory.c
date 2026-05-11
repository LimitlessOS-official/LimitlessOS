#include "memory.h"

#include "types.h"

extern u8 __kernel_end;

enum
{
    MEMORY_PAGE_SIZE = 4096u,
    MEMORY_INVALID_FRAME = 0xFFFFFFFFu
};

static u32 conventional_memory_bytes = 0;
static u32 extended_memory_bytes = 0;
static u32 early_heap_start = 0;
static u32 early_heap_current = 0;
static u32 early_heap_limit = 0;
static u32 frame_region_start = 0;
static u32 frame_region_limit = 0;
static u32 frame_count = 0;
static u32 free_frame_count = 0;
static u32 frame_bitmap_word_count = 0;
static u32 *frame_bitmap = NULL;

static u32 align_up(u32 value, u32 alignment)
{
    u32 remainder;

    if (alignment <= 1)
    {
        return value;
    }

    remainder = value % alignment;
    if (remainder == 0)
    {
        return value;
    }

    return value + alignment - remainder;
}

static u32 align_down(u32 value, u32 alignment)
{
    if (alignment <= 1)
    {
        return value;
    }

    return value - (value % alignment);
}

static void memory_zero(void *address, u32 size)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0; index < size; ++index)
    {
        bytes[index] = 0;
    }
}

static int frame_test(u32 frame_index);

static int memory_region_is_reserved(u32 start, u32 size)
{
    u32 range_start;
    u32 range_end;
    u32 frame_index;
    u32 first_frame;
    u32 past_last_frame;

    if ((frame_bitmap == NULL) || (size == 0u))
    {
        return 0;
    }

    range_start = start;
    range_end = start + size;

    if ((range_end < range_start) || (range_end <= frame_region_start))
    {
        return 0;
    }

    if (range_start < frame_region_start)
    {
        range_start = frame_region_start;
    }

    if (range_end > frame_region_limit)
    {
        range_end = frame_region_limit;
    }

    if (range_end <= range_start)
    {
        return 0;
    }

    first_frame = (align_down(range_start, MEMORY_PAGE_SIZE) - frame_region_start) / MEMORY_PAGE_SIZE;
    past_last_frame = (align_up(range_end, MEMORY_PAGE_SIZE) - frame_region_start) / MEMORY_PAGE_SIZE;

    if (past_last_frame > frame_count)
    {
        past_last_frame = frame_count;
    }

    for (frame_index = first_frame; frame_index < past_last_frame; ++frame_index)
    {
        if (frame_test(frame_index))
        {
            return 1;
        }
    }

    return 0;
}

static void *memory_early_alloc_raw(u32 size, u32 alignment)
{
    u32 allocation_start = align_up(early_heap_current, alignment);

    while (allocation_start < early_heap_limit)
    {
        u32 allocation_end = allocation_start + size;

        if ((allocation_end < allocation_start) || (allocation_end > early_heap_limit))
        {
            return NULL;
        }

        if (!memory_region_is_reserved(allocation_start, size))
        {
            early_heap_current = allocation_end;
            memory_zero((void *)allocation_start, size);
            return (void *)allocation_start;
        }

        allocation_start = align_up(align_up(allocation_start, MEMORY_PAGE_SIZE) + MEMORY_PAGE_SIZE, alignment);
    }

    return NULL;
}

static void frame_set(u32 frame_index)
{
    u32 word_index = frame_index / 32u;
    u32 bit_index = frame_index % 32u;

    frame_bitmap[word_index] |= (1u << bit_index);
}

static void frame_clear(u32 frame_index)
{
    u32 word_index = frame_index / 32u;
    u32 bit_index = frame_index % 32u;

    frame_bitmap[word_index] &= ~(1u << bit_index);
}

static int frame_test(u32 frame_index)
{
    u32 word_index = frame_index / 32u;
    u32 bit_index = frame_index % 32u;

    return (frame_bitmap[word_index] & (1u << bit_index)) != 0;
}

static void memory_reserve_region(u32 start, u32 size)
{
    u32 range_start;
    u32 range_end;
    u32 frame_index;
    u32 first_frame;
    u32 past_last_frame;

    if ((frame_bitmap == NULL) || (size == 0))
    {
        return;
    }

    range_start = start;
    range_end = start + size;

    if (range_end < range_start)
    {
        return;
    }

    if (range_end <= frame_region_start)
    {
        return;
    }

    if (range_start < frame_region_start)
    {
        range_start = frame_region_start;
    }

    if (range_end > frame_region_limit)
    {
        range_end = frame_region_limit;
    }

    if (range_end <= range_start)
    {
        return;
    }

    first_frame = (align_down(range_start, MEMORY_PAGE_SIZE) - frame_region_start) / MEMORY_PAGE_SIZE;
    past_last_frame = (align_up(range_end, MEMORY_PAGE_SIZE) - frame_region_start) / MEMORY_PAGE_SIZE;

    if (past_last_frame > frame_count)
    {
        past_last_frame = frame_count;
    }

    for (frame_index = first_frame; frame_index < past_last_frame; ++frame_index)
    {
        if (!frame_test(frame_index))
        {
            frame_set(frame_index);
            if (free_frame_count != 0)
            {
                --free_frame_count;
            }
        }
    }
}

void memory_init(const struct boot_info *boot_info)
{
    u32 heap_base;
    u32 heap_limit;
    u32 bitmap_bytes;

    conventional_memory_bytes = boot_info->conventional_memory_kb * 1024u;
    extended_memory_bytes = boot_info->extended_memory_kb * 1024u;

    if (extended_memory_bytes != 0)
    {
        heap_base = 0x00100000u;
        heap_limit = heap_base + extended_memory_bytes;
    }
    else
    {
        heap_base = align_up((u32)&__kernel_end, MEMORY_PAGE_SIZE);
        heap_limit = align_down(conventional_memory_bytes, MEMORY_PAGE_SIZE);
    }

    if (heap_limit < heap_base)
    {
        heap_limit = heap_base;
    }

    early_heap_start = heap_base;
    early_heap_current = heap_base;
    early_heap_limit = heap_limit;

    frame_region_start = align_up(heap_base, MEMORY_PAGE_SIZE);
    frame_region_limit = align_down(heap_limit, MEMORY_PAGE_SIZE);

    if (frame_region_limit > frame_region_start)
    {
        frame_count = (frame_region_limit - frame_region_start) / MEMORY_PAGE_SIZE;
        frame_bitmap_word_count = (frame_count + 31u) / 32u;
        bitmap_bytes = frame_bitmap_word_count * sizeof(u32);
        frame_bitmap = (u32 *)memory_early_alloc_raw(bitmap_bytes, 16);
        free_frame_count = frame_count;
        memory_reserve_region(frame_region_start, early_heap_current - frame_region_start);
    }
}

void *memory_early_alloc(u32 size, u32 alignment)
{
    void *allocation = memory_early_alloc_raw(size, alignment);

    if (allocation != NULL)
    {
        memory_reserve_region((u32)allocation, size);
    }

    return allocation;
}

u32 memory_claim_frame(void)
{
    u32 word_index;
    u32 frame_index;

    if (frame_bitmap == NULL)
    {
        return MEMORY_INVALID_FRAME;
    }

    for (word_index = 0; word_index < frame_bitmap_word_count; ++word_index)
    {
        if (frame_bitmap[word_index] == 0xFFFFFFFFu)
        {
            continue;
        }

        for (frame_index = 0; frame_index < 32u; ++frame_index)
        {
            u32 absolute_index = word_index * 32u + frame_index;

            if (absolute_index >= frame_count)
            {
                break;
            }

            if (!frame_test(absolute_index))
            {
                frame_set(absolute_index);
                if (free_frame_count != 0)
                {
                    --free_frame_count;
                }

                return frame_region_start + (absolute_index * MEMORY_PAGE_SIZE);
            }
        }
    }

    return MEMORY_INVALID_FRAME;
}

void memory_release_frame(u32 physical_address)
{
    u32 frame_index;

    if ((frame_bitmap == NULL)
        || (physical_address < frame_region_start)
        || (physical_address >= frame_region_limit))
    {
        return;
    }

    frame_index = (physical_address - frame_region_start) / MEMORY_PAGE_SIZE;
    if ((frame_index < frame_count) && frame_test(frame_index))
    {
        frame_clear(frame_index);
        ++free_frame_count;
    }
}

u32 memory_get_conventional_bytes(void)
{
    return conventional_memory_bytes;
}

u32 memory_get_extended_bytes(void)
{
    return extended_memory_bytes;
}

u32 memory_get_total_bytes(void)
{
    if (frame_region_limit < frame_region_start)
    {
        return 0;
    }

    return frame_region_limit - frame_region_start;
}

u32 memory_get_free_bytes(void)
{
    return free_frame_count * MEMORY_PAGE_SIZE;
}

u32 memory_get_frame_count(void)
{
    return frame_count;
}

u32 memory_get_free_frame_count(void)
{
    return free_frame_count;
}

u32 memory_get_heap_base(void)
{
    return early_heap_start;
}

u32 memory_get_heap_limit(void)
{
    return early_heap_limit;
}
