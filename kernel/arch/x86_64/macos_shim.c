#include "macos_shim_x64.h"

#include "macos_abi_x64.h"
#include "paging_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * N.3 adds the first LimitlessOS-owned libSystem.B.dylib shim surface. It
 * integrates with macho64.c for curated dyld symbol binding, macos_abi.c for
 * BSD syscall-backed read/write/open/close/mmap/mprotect/clock operations,
 * vma_x64.c for mapped shim and heap pages, and persona_x64.c for MACOS_MACHO
 * validation. The checkpoint proves the shim exports the documented symbols,
 * maps RX/RO pages, bridges real syscall operations, performs bounded memory
 * and string helpers, denies unknown symbols truthfully, and releases its VMA
 * footprint without touching the BIOS slice.
 */

typedef struct macos_shim64_export
{
    const char *name;
    u32 name_length;
    u32 symbol_id;
    u32 rva;
} macos_shim64_export_t;

typedef struct macos_shim64_heap
{
    u32 active;
    u32 pid;
    u64 base;
    u64 current;
    u64 end;
} macos_shim64_heap_t;

typedef struct macos_shim64_allocation
{
    u32 active;
    u32 pid;
    u64 address;
    u64 size;
} macos_shim64_allocation_t;

static const macos_shim64_export_t g_macos_shim64_libsystem_exports[
    MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT] = {
    { "_write", 6u, MACOS_SHIM64_SYMBOL_WRITE, MACOS_SHIM64_RVA_WRITE },
    { "_read", 5u, MACOS_SHIM64_SYMBOL_READ, MACOS_SHIM64_RVA_READ },
    { "_open", 5u, MACOS_SHIM64_SYMBOL_OPEN, MACOS_SHIM64_RVA_OPEN },
    { "_close", 6u, MACOS_SHIM64_SYMBOL_CLOSE, MACOS_SHIM64_RVA_CLOSE },
    { "_exit", 5u, MACOS_SHIM64_SYMBOL_EXIT, MACOS_SHIM64_RVA_EXIT },
    { "_mmap", 5u, MACOS_SHIM64_SYMBOL_MMAP, MACOS_SHIM64_RVA_MMAP },
    { "_munmap", 7u, MACOS_SHIM64_SYMBOL_MUNMAP, MACOS_SHIM64_RVA_MUNMAP },
    { "_mprotect", 9u, MACOS_SHIM64_SYMBOL_MPROTECT, MACOS_SHIM64_RVA_MPROTECT },
    { "_malloc", 7u, MACOS_SHIM64_SYMBOL_MALLOC, MACOS_SHIM64_RVA_MALLOC },
    { "_free", 5u, MACOS_SHIM64_SYMBOL_FREE, MACOS_SHIM64_RVA_FREE },
    { "_realloc", 8u, MACOS_SHIM64_SYMBOL_REALLOC, MACOS_SHIM64_RVA_REALLOC },
    { "_memcpy", 7u, MACOS_SHIM64_SYMBOL_MEMCPY, MACOS_SHIM64_RVA_MEMCPY },
    { "_memset", 7u, MACOS_SHIM64_SYMBOL_MEMSET, MACOS_SHIM64_RVA_MEMSET },
    { "_strlen", 7u, MACOS_SHIM64_SYMBOL_STRLEN, MACOS_SHIM64_RVA_STRLEN },
    { "_printf", 7u, MACOS_SHIM64_SYMBOL_PRINTF, MACOS_SHIM64_RVA_PRINTF },
    { "_clock_gettime", 14u, MACOS_SHIM64_SYMBOL_CLOCK_GETTIME, MACOS_SHIM64_RVA_CLOCK_GETTIME }
};

static macos_shim64_heap_t g_macos_shim64_heaps[MACOS_SHIM64_MAX_HEAPS];
static macos_shim64_allocation_t
    g_macos_shim64_allocations[MACOS_SHIM64_MAX_ALLOCATIONS];
static u32 g_macos_shim64_initialized = 0u;
static u32 g_macos_shim64_load_count = 0u;
static u32 g_macos_shim64_call_count = 0u;
static u32 g_macos_shim64_syscall_bridge_count = 0u;
static u32 g_macos_shim64_memory_call_count = 0u;
static u32 g_macos_shim64_denial_count = 0u;
static u32 g_macos_shim64_fault_count = 0u;
static u32 g_macos_shim64_last_symbol = MACOS_SHIM64_SYMBOL_NONE;
static u32 g_macos_shim64_last_error = MACOS_SHIM64_ERROR_NONE;
static u64 g_macos_shim64_last_result = 0ull;
static u32 g_macos_shim64_last_byte_count = 0u;
static u32 g_macos_shim64_last_checksum = 0u;

static u32 macos_shim64_mix_checksum(u32 checksum, u8 value)
{
    checksum ^= (u32)value;
    checksum *= 16777619u;
    return checksum;
}

static u32 macos_shim64_name_matches(
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

static u32 macos_shim64_valid_persona(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_MACOS_MACHO))
        ? 1u
        : 0u;
}

static u32 macos_shim64_range_overflows(u64 address, u64 byte_count)
{
    return ((byte_count != 0ull) && ((address + byte_count) < address)) ? 1u : 0u;
}

static u32 macos_shim64_user_range_ready(
    u32 pid,
    u64 address,
    u64 byte_count,
    u32 require_write)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0ull)
    {
        return 1u;
    }
    if ((address == 0ull) || (macos_shim64_range_overflows(address, byte_count) != 0u))
    {
        return 0u;
    }

    cursor = address;
    end = address + byte_count;
    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 page = cursor & ~((u64)VMA64_PAGE_BYTES - 1ull);
        u64 next_page = page + (u64)VMA64_PAGE_BYTES;
        u64 next = (next_page < end) ? next_page : end;
        u32 protection = paging64_user_page_protection(page);

        if ((region == 0)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end)
            || ((region->prot_flags & VMA64_PROT_READ) == 0u)
            || (paging64_user_page_present(page) == 0u)
            || ((protection & PAGING64_USER_PROT_READ) == 0u))
        {
            return 0u;
        }
        if ((require_write != 0u)
            && (((region->prot_flags & VMA64_PROT_WRITE) == 0u)
                || ((protection & PAGING64_USER_PROT_WRITE) == 0u)))
        {
            return 0u;
        }
        if (next > region->virt_end)
        {
            next = region->virt_end;
        }
        if (next <= cursor)
        {
            return 0u;
        }
        cursor = next;
    }

    return 1u;
}

static void macos_shim64_write_stub(u64 address, u32 symbol_id)
{
    volatile u8 *target = (volatile u8 *)(u64)address;

    target[0] = 0xB8u;
    target[1] = (u8)(symbol_id & 0xFFu);
    target[2] = (u8)((symbol_id >> 8) & 0xFFu);
    target[3] = (u8)((symbol_id >> 16) & 0xFFu);
    target[4] = (u8)((symbol_id >> 24) & 0xFFu);
    target[5] = 0xC3u;
    target[6] = 0xCCu;
    target[7] = 0xCCu;
}

static u32 macos_shim64_checksum_range(u64 address, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = macos_shim64_mix_checksum(
            checksum,
            ((volatile u8 *)(u64)address)[index]);
    }

    return checksum;
}

static void macos_shim64_set_call_result(
    macos_shim64_call_result_t *result,
    u64 value,
    u32 symbol_id,
    u32 error,
    u32 byte_count,
    u32 checksum)
{
    if (result != 0)
    {
        result->value = value;
        result->symbol_id = symbol_id;
        result->error = error;
        result->byte_count = byte_count;
        result->checksum = checksum;
    }

    g_macos_shim64_last_symbol = symbol_id;
    g_macos_shim64_last_error = error;
    g_macos_shim64_last_result = value;
    g_macos_shim64_last_byte_count = byte_count;
    g_macos_shim64_last_checksum = checksum;
}

static u32 macos_shim64_copy_bytes(u64 dst, u64 src, u64 byte_count)
{
    u64 index;
    u32 checksum = 2166136261u;

    for (index = 0ull; index < byte_count; ++index)
    {
        u8 value = ((volatile u8 *)(u64)src)[index];
        ((volatile u8 *)(u64)dst)[index] = value;
        checksum = macos_shim64_mix_checksum(checksum, value);
    }

    return checksum;
}

static u32 macos_shim64_set_bytes(u64 dst, u8 value, u64 byte_count)
{
    u64 index;
    u32 checksum = 2166136261u;

    for (index = 0ull; index < byte_count; ++index)
    {
        ((volatile u8 *)(u64)dst)[index] = value;
        checksum = macos_shim64_mix_checksum(checksum, value);
    }

    return checksum;
}

static u32 macos_shim64_cstring_length(
    u32 pid,
    u64 address,
    u32 limit,
    u32 *out_length,
    u32 *out_checksum)
{
    u32 length;
    u32 checksum = 2166136261u;

    if ((out_length == 0) || (out_checksum == 0) || (address == 0ull))
    {
        return 0u;
    }

    for (length = 0u; length < limit; ++length)
    {
        u8 value;

        if (macos_shim64_user_range_ready(pid, address + (u64)length, 1ull, 0u) == 0u)
        {
            return 0u;
        }

        value = ((volatile u8 *)(u64)(address + (u64)length))[0];
        if (value == 0u)
        {
            *out_length = length;
            *out_checksum = checksum;
            return 1u;
        }
        checksum = macos_shim64_mix_checksum(checksum, value);
    }

    return 0u;
}

static macos_shim64_heap_t *macos_shim64_heap_for_pid(u32 pid, u32 create)
{
    u32 index;
    macos_shim64_heap_t *free_heap = 0;

    for (index = 0u; index < MACOS_SHIM64_MAX_HEAPS; ++index)
    {
        if ((g_macos_shim64_heaps[index].active != 0u)
            && (g_macos_shim64_heaps[index].pid == pid))
        {
            return &g_macos_shim64_heaps[index];
        }
        if ((free_heap == 0) && (g_macos_shim64_heaps[index].active == 0u))
        {
            free_heap = &g_macos_shim64_heaps[index];
        }
    }

    if ((create == 0u) || (free_heap == 0))
    {
        return 0;
    }

    index = (u32)(free_heap - g_macos_shim64_heaps);
    free_heap->base = MACOS_SHIM64_LIBSYSTEM_HEAP_BASE
        + ((u64)index * (u64)MACOS_SHIM64_LIBSYSTEM_HEAP_BYTES);
    if (vma64_map_anon(
            pid,
            free_heap->base,
            MACOS_SHIM64_LIBSYSTEM_HEAP_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != free_heap->base)
    {
        free_heap->base = 0ull;
        return 0;
    }

    free_heap->active = 1u;
    free_heap->pid = pid;
    free_heap->current = free_heap->base;
    free_heap->end = free_heap->base + (u64)MACOS_SHIM64_LIBSYSTEM_HEAP_BYTES;
    return free_heap;
}

static macos_shim64_allocation_t *macos_shim64_find_allocation(u32 pid, u64 address)
{
    u32 index;

    for (index = 0u; index < MACOS_SHIM64_MAX_ALLOCATIONS; ++index)
    {
        if ((g_macos_shim64_allocations[index].active != 0u)
            && (g_macos_shim64_allocations[index].pid == pid)
            && (g_macos_shim64_allocations[index].address == address))
        {
            return &g_macos_shim64_allocations[index];
        }
    }

    return 0;
}

static macos_shim64_allocation_t *macos_shim64_free_allocation_slot(void)
{
    u32 index;

    for (index = 0u; index < MACOS_SHIM64_MAX_ALLOCATIONS; ++index)
    {
        if (g_macos_shim64_allocations[index].active == 0u)
        {
            return &g_macos_shim64_allocations[index];
        }
    }

    return 0;
}

static u64 macos_shim64_heap_alloc(u32 pid, u64 byte_count)
{
    macos_shim64_heap_t *heap;
    macos_shim64_allocation_t *allocation;
    u64 size;
    u64 address;

    if (byte_count == 0ull)
    {
        byte_count = 1ull;
    }
    if (byte_count > 0x1000ull)
    {
        return 0ull;
    }

    size = (byte_count + 15ull) & ~15ull;
    heap = macos_shim64_heap_for_pid(pid, 1u);
    allocation = macos_shim64_free_allocation_slot();
    if ((heap == 0) || (allocation == 0) || ((heap->current + size) > heap->end))
    {
        return 0ull;
    }

    address = heap->current;
    heap->current += size;
    allocation->active = 1u;
    allocation->pid = pid;
    allocation->address = address;
    allocation->size = size;
    return address;
}

static u32 macos_shim64_heap_free(u32 pid, u64 address)
{
    macos_shim64_allocation_t *allocation;

    if (address == 0ull)
    {
        return 1u;
    }

    allocation = macos_shim64_find_allocation(pid, address);
    if (allocation == 0)
    {
        return 0u;
    }

    allocation->active = 0u;
    allocation->pid = 0u;
    allocation->address = 0ull;
    allocation->size = 0ull;
    return 1u;
}

static u64 macos_shim64_heap_realloc(u32 pid, u64 address, u64 new_size)
{
    macos_shim64_allocation_t *old_allocation;
    u64 new_address;
    u64 copy_bytes;

    if (address == 0ull)
    {
        return macos_shim64_heap_alloc(pid, new_size);
    }
    if (new_size == 0ull)
    {
        return (macos_shim64_heap_free(pid, address) != 0u) ? 0ull : 0ull;
    }

    old_allocation = macos_shim64_find_allocation(pid, address);
    if (old_allocation == 0)
    {
        return 0ull;
    }

    new_address = macos_shim64_heap_alloc(pid, new_size);
    if (new_address == 0ull)
    {
        return 0ull;
    }

    copy_bytes = (old_allocation->size < new_size) ? old_allocation->size : new_size;
    if ((macos_shim64_user_range_ready(pid, address, copy_bytes, 0u) == 0u)
        || (macos_shim64_user_range_ready(pid, new_address, copy_bytes, 1u) == 0u))
    {
        return 0ull;
    }

    (void)macos_shim64_copy_bytes(new_address, address, copy_bytes);
    (void)macos_shim64_heap_free(pid, address);
    return new_address;
}

void macos_shim64_init(void)
{
    if (g_macos_shim64_initialized != 0u)
    {
        return;
    }

    g_macos_shim64_initialized = 1u;
}

u32 macos_shim64_load_libsystem(
    u32 pid,
    u64 image_base,
    macos_shim64_libsystem_result_t *out_result)
{
    u32 index;
    u64 text_base;
    u64 rodata_base;
    static const u8 libsystem_name[] = "libSystem.B.dylib";

    macos_shim64_init();

    if (out_result == 0)
    {
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + (u64)MACOS_SHIM64_LIBSYSTEM_IMAGE_BYTES;
    out_result->write_fn = image_base + (u64)MACOS_SHIM64_RVA_WRITE;
    out_result->read_fn = image_base + (u64)MACOS_SHIM64_RVA_READ;
    out_result->open_fn = image_base + (u64)MACOS_SHIM64_RVA_OPEN;
    out_result->close_fn = image_base + (u64)MACOS_SHIM64_RVA_CLOSE;
    out_result->exit_fn = image_base + (u64)MACOS_SHIM64_RVA_EXIT;
    out_result->mmap_fn = image_base + (u64)MACOS_SHIM64_RVA_MMAP;
    out_result->munmap_fn = image_base + (u64)MACOS_SHIM64_RVA_MUNMAP;
    out_result->mprotect_fn = image_base + (u64)MACOS_SHIM64_RVA_MPROTECT;
    out_result->malloc_fn = image_base + (u64)MACOS_SHIM64_RVA_MALLOC;
    out_result->free_fn = image_base + (u64)MACOS_SHIM64_RVA_FREE;
    out_result->realloc_fn = image_base + (u64)MACOS_SHIM64_RVA_REALLOC;
    out_result->memcpy_fn = image_base + (u64)MACOS_SHIM64_RVA_MEMCPY;
    out_result->memset_fn = image_base + (u64)MACOS_SHIM64_RVA_MEMSET;
    out_result->strlen_fn = image_base + (u64)MACOS_SHIM64_RVA_STRLEN;
    out_result->printf_fn = image_base + (u64)MACOS_SHIM64_RVA_PRINTF;
    out_result->clock_gettime_fn = image_base + (u64)MACOS_SHIM64_RVA_CLOCK_GETTIME;
    out_result->image_bytes = MACOS_SHIM64_LIBSYSTEM_IMAGE_BYTES;
    out_result->section_count = 2u;
    out_result->mapped_count = 0u;
    out_result->symbol_count = MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT;
    out_result->text_checksum = 2166136261u;
    out_result->rodata_checksum = 2166136261u;
    out_result->name_checksum = 2166136261u;
    out_result->text_protection = 0u;
    out_result->rodata_protection = 0u;
    out_result->context_stored = (persona64_context_for_process(pid) != 0) ? 1u : 0u;
    out_result->error = MACOS_SHIM64_ERROR_NONE;

    if (macos_shim64_valid_persona(pid) == 0u)
    {
        out_result->error = MACOS_SHIM64_ERROR_PERSONA;
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }
    if ((image_base != MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE)
        || ((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        out_result->error = MACOS_SHIM64_ERROR_BASE;
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }

    text_base = image_base + (u64)MACOS_SHIM64_LIBSYSTEM_TEXT_RVA;
    rodata_base = image_base + (u64)MACOS_SHIM64_LIBSYSTEM_RODATA_RVA;
    if ((vma64_find(pid, text_base) != 0) || (vma64_find(pid, rodata_base) != 0))
    {
        out_result->error = MACOS_SHIM64_ERROR_ALREADY_MAPPED;
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }

    if (vma64_map_anon(
            pid,
            text_base,
            MACOS_SHIM64_LIBSYSTEM_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != text_base)
    {
        out_result->error = MACOS_SHIM64_ERROR_MAP;
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }
    ++out_result->mapped_count;

    for (index = 0u; index < MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT; ++index)
    {
        macos_shim64_write_stub(
            image_base + (u64)g_macos_shim64_libsystem_exports[index].rva,
            g_macos_shim64_libsystem_exports[index].symbol_id);
    }
    out_result->text_checksum = macos_shim64_checksum_range(
        text_base,
        MACOS_SHIM64_TEXT_PATTERN_BYTES * MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT);

    if (vma64_protect(
            pid,
            text_base,
            MACOS_SHIM64_LIBSYSTEM_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_EXECUTE)
        == 0u)
    {
        out_result->error = MACOS_SHIM64_ERROR_MAP;
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }
    out_result->text_protection = paging64_user_page_protection(text_base);

    if (vma64_map_anon(
            pid,
            rodata_base,
            MACOS_SHIM64_LIBSYSTEM_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != rodata_base)
    {
        out_result->error = MACOS_SHIM64_ERROR_MAP;
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }
    ++out_result->mapped_count;

    for (index = 0u; index < ((u32)sizeof(libsystem_name) - 1u); ++index)
    {
        ((volatile u8 *)(u64)rodata_base)[index] = libsystem_name[index];
        out_result->name_checksum =
            macos_shim64_mix_checksum(out_result->name_checksum, libsystem_name[index]);
    }
    out_result->rodata_checksum = macos_shim64_checksum_range(
        rodata_base,
        (u32)sizeof(libsystem_name) - 1u);

    if (vma64_protect(
            pid,
            rodata_base,
            MACOS_SHIM64_LIBSYSTEM_PAGE_BYTES,
            VMA64_PROT_READ)
        == 0u)
    {
        out_result->error = MACOS_SHIM64_ERROR_MAP;
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }
    out_result->rodata_protection = paging64_user_page_protection(rodata_base);
    ++g_macos_shim64_load_count;
    return MACOS_SHIM64_OK;
}

u64 macos_shim64_resolve_libsystem_symbol(const char *name, u32 name_length)
{
    u32 index;

    if (name == 0)
    {
        return 0ull;
    }

    for (index = 0u; index < MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT; ++index)
    {
        const macos_shim64_export_t *record = &g_macos_shim64_libsystem_exports[index];
        if ((macos_shim64_name_matches(name, name_length, record->name, record->name_length) != 0u)
            || (((record->name_length > 1u) && (record->name[0] == '_'))
                && (macos_shim64_name_matches(
                    name,
                    name_length,
                    record->name + 1,
                    record->name_length - 1u) != 0u)))
        {
            return MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)record->rva;
        }
    }

    return 0ull;
}

u32 macos_shim64_symbol_id_for_address(u64 address)
{
    u32 index;

    for (index = 0u; index < MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT; ++index)
    {
        if (address
            == (MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE
                + (u64)g_macos_shim64_libsystem_exports[index].rva))
        {
            return g_macos_shim64_libsystem_exports[index].symbol_id;
        }
    }

    return MACOS_SHIM64_SYMBOL_NONE;
}

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
    macos_shim64_call_result_t *out_result)
{
    u32 symbol_id;
    u64 value = 0ull;
    u32 byte_count = 0u;
    u32 checksum = 2166136261u;

    (void)arg5;

    macos_shim64_init();

    if (out_result == 0)
    {
        ++g_macos_shim64_denial_count;
        return MACOS_SHIM64_DENIED;
    }

    symbol_id = macos_shim64_symbol_id_for_address(shim_address);
    if ((symbol_id == MACOS_SHIM64_SYMBOL_NONE) || (macos_shim64_valid_persona(pid) == 0u))
    {
        ++g_macos_shim64_denial_count;
        macos_shim64_set_call_result(
            out_result,
            0ull,
            symbol_id,
            (symbol_id == MACOS_SHIM64_SYMBOL_NONE)
                ? MACOS_SHIM64_ERROR_SYMBOL
                : MACOS_SHIM64_ERROR_PERSONA,
            0u,
            checksum);
        return MACOS_SHIM64_DENIED;
    }

    ++g_macos_shim64_call_count;

    if (symbol_id == MACOS_SHIM64_SYMBOL_WRITE)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_WRITE,
            arg0,
            arg1,
            arg2,
            0ull,
            0ull,
            0ull,
            rip);
        byte_count = (value <= 0xFFFFFFFFull) ? (u32)value : 0u;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_READ)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_READ,
            arg0,
            arg1,
            arg2,
            0ull,
            0ull,
            0ull,
            rip);
        byte_count = (value <= 0xFFFFFFFFull) ? (u32)value : 0u;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_OPEN)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_OPEN,
            arg0,
            arg1,
            arg2,
            0ull,
            0ull,
            0ull,
            rip);
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_CLOSE)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_CLOSE,
            arg0,
            0ull,
            0ull,
            0ull,
            0ull,
            0ull,
            rip);
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_EXIT)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_EXIT,
            arg0,
            0ull,
            0ull,
            0ull,
            0ull,
            0ull,
            rip);
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_MMAP)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_MMAP,
            arg0,
            arg1,
            arg2,
            arg3,
            arg4,
            arg5,
            rip);
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_MUNMAP)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_MUNMAP,
            arg0,
            arg1,
            0ull,
            0ull,
            0ull,
            0ull,
            rip);
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_MPROTECT)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_MPROTECT,
            arg0,
            arg1,
            arg2,
            0ull,
            0ull,
            0ull,
            rip);
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_CLOCK_GETTIME)
    {
        ++g_macos_shim64_syscall_bridge_count;
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_CLOCK_GETTIME,
            arg0,
            arg1,
            0ull,
            0ull,
            0ull,
            0ull,
            rip);
        byte_count = MACOS_ABI64_TIMESPEC_BYTES;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_MALLOC)
    {
        ++g_macos_shim64_memory_call_count;
        value = macos_shim64_heap_alloc(pid, arg0);
        if (value == 0ull)
        {
            ++g_macos_shim64_denial_count;
            macos_shim64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_SHIM64_ERROR_HEAP,
                0u,
                checksum);
            return MACOS_SHIM64_DENIED;
        }
        byte_count = (arg0 <= 0xFFFFFFFFull) ? (u32)arg0 : 0u;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_FREE)
    {
        ++g_macos_shim64_memory_call_count;
        if (macos_shim64_heap_free(pid, arg0) == 0u)
        {
            ++g_macos_shim64_denial_count;
            macos_shim64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_SHIM64_ERROR_HEAP,
                0u,
                checksum);
            return MACOS_SHIM64_DENIED;
        }
        value = 0ull;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_REALLOC)
    {
        ++g_macos_shim64_memory_call_count;
        value = macos_shim64_heap_realloc(pid, arg0, arg1);
        if ((arg1 != 0ull) && (value == 0ull))
        {
            ++g_macos_shim64_denial_count;
            macos_shim64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_SHIM64_ERROR_HEAP,
                0u,
                checksum);
            return MACOS_SHIM64_DENIED;
        }
        byte_count = (arg1 <= 0xFFFFFFFFull) ? (u32)arg1 : 0u;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_MEMCPY)
    {
        ++g_macos_shim64_memory_call_count;
        if ((arg2 > (u64)MACOS_SHIM64_STRING_LIMIT)
            || (macos_shim64_user_range_ready(pid, arg0, arg2, 1u) == 0u)
            || (macos_shim64_user_range_ready(pid, arg1, arg2, 0u) == 0u))
        {
            ++g_macos_shim64_fault_count;
            macos_shim64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_SHIM64_ERROR_FAULT,
                0u,
                checksum);
            return MACOS_SHIM64_DENIED;
        }
        checksum = macos_shim64_copy_bytes(arg0, arg1, arg2);
        value = arg0;
        byte_count = (u32)arg2;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_MEMSET)
    {
        ++g_macos_shim64_memory_call_count;
        if ((arg2 > (u64)MACOS_SHIM64_STRING_LIMIT)
            || (macos_shim64_user_range_ready(pid, arg0, arg2, 1u) == 0u))
        {
            ++g_macos_shim64_fault_count;
            macos_shim64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_SHIM64_ERROR_FAULT,
                0u,
                checksum);
            return MACOS_SHIM64_DENIED;
        }
        checksum = macos_shim64_set_bytes(arg0, (u8)(arg1 & 0xFFu), arg2);
        value = arg0;
        byte_count = (u32)arg2;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_STRLEN)
    {
        ++g_macos_shim64_memory_call_count;
        if (macos_shim64_cstring_length(
                pid,
                arg0,
                MACOS_SHIM64_STRING_LIMIT,
                &byte_count,
                &checksum) == 0u)
        {
            ++g_macos_shim64_fault_count;
            macos_shim64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_SHIM64_ERROR_FAULT,
                0u,
                checksum);
            return MACOS_SHIM64_DENIED;
        }
        value = (u64)byte_count;
    }
    else if (symbol_id == MACOS_SHIM64_SYMBOL_PRINTF)
    {
        ++g_macos_shim64_memory_call_count;
        ++g_macos_shim64_syscall_bridge_count;
        if (macos_shim64_cstring_length(
                pid,
                arg0,
                MACOS_SHIM64_STRING_LIMIT,
                &byte_count,
                &checksum) == 0u)
        {
            ++g_macos_shim64_fault_count;
            macos_shim64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_SHIM64_ERROR_FAULT,
                0u,
                checksum);
            return MACOS_SHIM64_DENIED;
        }
        value = macos_abi64_dispatch(
            pid,
            MACOS_ABI64_SYSCALL_WRITE,
            1ull,
            arg0,
            (u64)byte_count,
            0ull,
            0ull,
            0ull,
            rip);
    }
    else
    {
        ++g_macos_shim64_denial_count;
        macos_shim64_set_call_result(
            out_result,
            0ull,
            symbol_id,
            MACOS_SHIM64_ERROR_SYMBOL,
            0u,
            checksum);
        return MACOS_SHIM64_DENIED;
    }

    macos_shim64_set_call_result(
        out_result,
        value,
        symbol_id,
        MACOS_SHIM64_ERROR_NONE,
        byte_count,
        checksum);
    return MACOS_SHIM64_OK;
}

u32 macos_shim64_release_process(u32 pid)
{
    u32 released = 0u;
    u32 index;

    if (pid == PROCESS64_INVALID_PID)
    {
        return 0u;
    }

    if (vma64_find(pid, MACOS_SHIM64_ADDR_WRITE) != 0)
    {
        released += vma64_unmap(
            pid,
            MACOS_SHIM64_ADDR_WRITE,
            MACOS_SHIM64_LIBSYSTEM_PAGE_BYTES);
    }
    if (vma64_find(
            pid,
            MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE
                + (u64)MACOS_SHIM64_LIBSYSTEM_RODATA_RVA) != 0)
    {
        released += vma64_unmap(
            pid,
            MACOS_SHIM64_LIBSYSTEM_DEFAULT_BASE + (u64)MACOS_SHIM64_LIBSYSTEM_RODATA_RVA,
            MACOS_SHIM64_LIBSYSTEM_PAGE_BYTES);
    }

    for (index = 0u; index < MACOS_SHIM64_MAX_HEAPS; ++index)
    {
        if ((g_macos_shim64_heaps[index].active != 0u)
            && (g_macos_shim64_heaps[index].pid == pid))
        {
            released += vma64_unmap(
                pid,
                g_macos_shim64_heaps[index].base,
                MACOS_SHIM64_LIBSYSTEM_HEAP_BYTES);
            g_macos_shim64_heaps[index].active = 0u;
            g_macos_shim64_heaps[index].pid = 0u;
            g_macos_shim64_heaps[index].base = 0ull;
            g_macos_shim64_heaps[index].current = 0ull;
            g_macos_shim64_heaps[index].end = 0ull;
        }
    }

    for (index = 0u; index < MACOS_SHIM64_MAX_ALLOCATIONS; ++index)
    {
        if (g_macos_shim64_allocations[index].pid == pid)
        {
            g_macos_shim64_allocations[index].active = 0u;
            g_macos_shim64_allocations[index].pid = 0u;
            g_macos_shim64_allocations[index].address = 0ull;
            g_macos_shim64_allocations[index].size = 0ull;
        }
    }

    return released;
}

u32 macos_shim64_symbol_count(void) { return MACOS_SHIM64_LIBSYSTEM_SYMBOL_COUNT; }
u32 macos_shim64_load_count(void) { return g_macos_shim64_load_count; }
u32 macos_shim64_call_count(void) { return g_macos_shim64_call_count; }
u32 macos_shim64_syscall_bridge_count(void) { return g_macos_shim64_syscall_bridge_count; }
u32 macos_shim64_memory_call_count(void) { return g_macos_shim64_memory_call_count; }
u32 macos_shim64_denial_count(void) { return g_macos_shim64_denial_count; }
u32 macos_shim64_fault_count(void) { return g_macos_shim64_fault_count; }
u32 macos_shim64_last_symbol(void) { return g_macos_shim64_last_symbol; }
u32 macos_shim64_last_error(void) { return g_macos_shim64_last_error; }
u64 macos_shim64_last_result(void) { return g_macos_shim64_last_result; }
u32 macos_shim64_last_byte_count(void) { return g_macos_shim64_last_byte_count; }
u32 macos_shim64_last_checksum(void) { return g_macos_shim64_last_checksum; }
