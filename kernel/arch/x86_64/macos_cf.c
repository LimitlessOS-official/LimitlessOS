#include "macos_cf_x64.h"

#include "macos_shim_x64.h"
#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * N.5 adds a minimal LimitlessOS-owned CoreFoundation.framework shim. It
 * integrates with macho64.c for curated CoreFoundation symbol binding,
 * macos_shim.c for CFShow -> libSystem _write, vma_x64.c for RX/RO shim pages
 * and the user scratch page, and persona_audit_x64.c for truthful denials. The
 * checkpoint proves default allocator lookup, bounded CFString create/copy,
 * retain/release lifetime accounting, CFShow through the brokered write path,
 * unknown-symbol denial, and clean page/object teardown outside the BIOS slice.
 */

typedef struct macos_cf64_export
{
    const char *name;
    u32 name_length;
    u32 symbol_id;
    u32 rva;
} macos_cf64_export_t;

typedef struct macos_cf64_object
{
    u32 active;
    u32 pid;
    u32 type;
    u32 retain_count;
    u32 length;
    u32 checksum;
    u8 bytes[MACOS_CF64_STRING_LIMIT];
} macos_cf64_object_t;

#define MACOS_CF64_OBJECT_STRING 1u

static const macos_cf64_export_t g_macos_cf64_exports[MACOS_CF64_SYMBOL_COUNT] = {
    { "CFAllocatorGetDefault", 21u, MACOS_CF64_SYMBOL_ALLOCATOR_GET_DEFAULT, MACOS_CF64_RVA_ALLOCATOR_GET_DEFAULT },
    { "CFRelease", 9u, MACOS_CF64_SYMBOL_RELEASE, MACOS_CF64_RVA_RELEASE },
    { "CFRetain", 8u, MACOS_CF64_SYMBOL_RETAIN, MACOS_CF64_RVA_RETAIN },
    { "CFStringCreateWithCString", 25u, MACOS_CF64_SYMBOL_STRING_CREATE, MACOS_CF64_RVA_STRING_CREATE },
    { "CFStringGetCString", 18u, MACOS_CF64_SYMBOL_STRING_GET_CSTRING, MACOS_CF64_RVA_STRING_GET_CSTRING },
    { "CFShow", 6u, MACOS_CF64_SYMBOL_SHOW, MACOS_CF64_RVA_SHOW }
};

static macos_cf64_object_t g_macos_cf64_objects[MACOS_CF64_OBJECT_POOL_SIZE];
static u32 g_macos_cf64_initialized = 0u;
static u32 g_macos_cf64_load_count = 0u;
static u32 g_macos_cf64_call_count = 0u;
static u32 g_macos_cf64_string_create_count = 0u;
static u32 g_macos_cf64_get_cstring_count = 0u;
static u32 g_macos_cf64_show_count = 0u;
static u32 g_macos_cf64_retain_count = 0u;
static u32 g_macos_cf64_release_count = 0u;
static u32 g_macos_cf64_denial_count = 0u;
static u32 g_macos_cf64_fault_count = 0u;
static u32 g_macos_cf64_scratch_map_count = 0u;
static u32 g_macos_cf64_last_symbol = MACOS_CF64_SYMBOL_NONE;
static u32 g_macos_cf64_last_error = MACOS_CF64_ERROR_NONE;
static u64 g_macos_cf64_last_result = 0ull;
static u32 g_macos_cf64_last_byte_count = 0u;
static u32 g_macos_cf64_last_checksum = 2166136261u;

static u32 macos_cf64_mix_checksum(u32 checksum, u8 value)
{
    checksum ^= (u32)value;
    checksum *= 16777619u;
    return checksum;
}

static u32 macos_cf64_name_matches(
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

static u32 macos_cf64_export_matches(
    const char *name,
    u32 name_length,
    const macos_cf64_export_t *record)
{
    if ((name == 0) || (record == 0))
    {
        return 0u;
    }
    if (macos_cf64_name_matches(name, name_length, record->name, record->name_length) != 0u)
    {
        return 1u;
    }
    if ((name_length > 1u) && (name[0] == (char)'_'))
    {
        return macos_cf64_name_matches(
            name + 1,
            name_length - 1u,
            record->name,
            record->name_length);
    }

    return 0u;
}

static u32 macos_cf64_valid_persona(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_MACOS_MACHO))
        ? 1u
        : 0u;
}

static u32 macos_cf64_range_overflows(u64 address, u64 byte_count)
{
    return ((byte_count != 0ull) && ((address + byte_count) < address)) ? 1u : 0u;
}

static u32 macos_cf64_user_range_ready(
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
    if ((address == 0ull) || (macos_cf64_range_overflows(address, byte_count) != 0u))
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

static void macos_cf64_write_stub(u64 address, u32 symbol_id)
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

static u32 macos_cf64_checksum_range(u64 address, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = macos_cf64_mix_checksum(
            checksum,
            ((volatile const u8 *)(u64)address)[index]);
    }

    return checksum;
}

static u32 macos_cf64_write_bytes(u64 address, const char *bytes, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (bytes == 0)
    {
        return checksum;
    }
    for (index = 0u; index < byte_count; ++index)
    {
        ((volatile u8 *)(u64)address)[index] = (u8)bytes[index];
        checksum = macos_cf64_mix_checksum(checksum, (u8)bytes[index]);
    }
    ((volatile u8 *)(u64)address)[byte_count] = 0u;

    return checksum;
}

static void macos_cf64_set_call_result(
    macos_cf64_call_result_t *result,
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

    g_macos_cf64_last_symbol = symbol_id;
    g_macos_cf64_last_error = error;
    g_macos_cf64_last_result = value;
    g_macos_cf64_last_byte_count = byte_count;
    g_macos_cf64_last_checksum = checksum;
}

static u32 macos_cf64_record_denial(u32 pid, u32 error, u64 rip)
{
    ++g_macos_cf64_denial_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
        (u16)(error & 0xFFFFu),
        PERSONA_AUDIT64_RESULT_DENY,
        rip);
    return MACOS_CF64_DENIED;
}

static u32 macos_cf64_record_fault(u32 pid, u32 error, u64 rip)
{
    ++g_macos_cf64_fault_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
        (u16)(error & 0xFFFFu),
        PERSONA_AUDIT64_RESULT_DENY,
        rip);
    return MACOS_CF64_DENIED;
}

static u32 macos_cf64_cstring_length(
    u32 pid,
    u64 address,
    u32 max_bytes,
    u32 *out_length,
    u32 *out_checksum)
{
    u32 index;
    u32 checksum = 2166136261u;

    if (out_length != 0)
    {
        *out_length = 0u;
    }
    if (out_checksum != 0)
    {
        *out_checksum = checksum;
    }
    if ((address == 0ull) || (max_bytes == 0u))
    {
        return 0u;
    }

    for (index = 0u; index < max_bytes; ++index)
    {
        u64 current = address + (u64)index;
        u8 value;

        if (macos_cf64_user_range_ready(pid, current, 1ull, 0u) == 0u)
        {
            return 0u;
        }
        value = ((volatile const u8 *)(u64)current)[0];
        if (value == 0u)
        {
            if (out_length != 0)
            {
                *out_length = index;
            }
            if (out_checksum != 0)
            {
                *out_checksum = checksum;
            }
            return 1u;
        }
        checksum = macos_cf64_mix_checksum(checksum, value);
    }

    return 0u;
}

static u64 macos_cf64_handle_for_index(u32 index)
{
    return MACOS_CF64_OBJECT_HANDLE_BASE
        + ((u64)index * (u64)MACOS_CF64_OBJECT_HANDLE_STRIDE);
}

static macos_cf64_object_t *macos_cf64_object_for_handle(u32 pid, u64 handle)
{
    u32 index;

    if ((handle < MACOS_CF64_OBJECT_HANDLE_BASE)
        || (((handle - MACOS_CF64_OBJECT_HANDLE_BASE)
            % (u64)MACOS_CF64_OBJECT_HANDLE_STRIDE) != 0ull))
    {
        return 0;
    }

    index = (u32)((handle - MACOS_CF64_OBJECT_HANDLE_BASE)
        / (u64)MACOS_CF64_OBJECT_HANDLE_STRIDE);
    if ((index >= MACOS_CF64_OBJECT_POOL_SIZE)
        || (g_macos_cf64_objects[index].active == 0u)
        || (g_macos_cf64_objects[index].pid != pid))
    {
        return 0;
    }

    return &g_macos_cf64_objects[index];
}

static macos_cf64_object_t *macos_cf64_alloc_object(u32 pid, u32 *out_index)
{
    u32 index;

    if (out_index != 0)
    {
        *out_index = MACOS_CF64_OBJECT_POOL_SIZE;
    }

    for (index = 0u; index < MACOS_CF64_OBJECT_POOL_SIZE; ++index)
    {
        if (g_macos_cf64_objects[index].active == 0u)
        {
            g_macos_cf64_objects[index].active = 1u;
            g_macos_cf64_objects[index].pid = pid;
            g_macos_cf64_objects[index].type = MACOS_CF64_OBJECT_STRING;
            g_macos_cf64_objects[index].retain_count = 1u;
            g_macos_cf64_objects[index].length = 0u;
            g_macos_cf64_objects[index].checksum = 2166136261u;
            if (out_index != 0)
            {
                *out_index = index;
            }
            return &g_macos_cf64_objects[index];
        }
    }

    return 0;
}

static void macos_cf64_clear_object(macos_cf64_object_t *object)
{
    u32 index;

    if (object == 0)
    {
        return;
    }

    object->active = 0u;
    object->pid = 0u;
    object->type = 0u;
    object->retain_count = 0u;
    object->length = 0u;
    object->checksum = 2166136261u;
    for (index = 0u; index < MACOS_CF64_STRING_LIMIT; ++index)
    {
        object->bytes[index] = 0u;
    }
}

static u32 macos_cf64_copy_user_string_into_object(
    u32 pid,
    u64 address,
    macos_cf64_object_t *object)
{
    u32 index;
    u32 length = 0u;
    u32 checksum = 2166136261u;

    if ((object == 0)
        || (macos_cf64_cstring_length(
            pid,
            address,
            MACOS_CF64_STRING_LIMIT,
            &length,
            &checksum) == 0u)
        || (length >= MACOS_CF64_STRING_LIMIT))
    {
        return 0u;
    }

    for (index = 0u; index < length; ++index)
    {
        object->bytes[index] = ((volatile const u8 *)(u64)address)[index];
    }
    object->bytes[length] = 0u;
    object->length = length;
    object->checksum = checksum;
    return 1u;
}

static u32 macos_cf64_copy_object_to_user(
    u32 pid,
    const macos_cf64_object_t *object,
    u64 address,
    u64 capacity)
{
    u32 index;

    if ((object == 0)
        || (capacity == 0ull)
        || (capacity <= (u64)object->length)
        || (capacity > (u64)MACOS_CF64_STRING_LIMIT)
        || (macos_cf64_user_range_ready(pid, address, (u64)object->length + 1ull, 1u) == 0u))
    {
        return 0u;
    }

    for (index = 0u; index < object->length; ++index)
    {
        ((volatile u8 *)(u64)address)[index] = object->bytes[index];
    }
    ((volatile u8 *)(u64)address)[object->length] = 0u;
    return 1u;
}

static u32 macos_cf64_stage_show_bytes(u32 pid, const macos_cf64_object_t *object)
{
    u32 index;
    u64 scratch = MACOS_CF64_SCRATCH_BASE;

    if (object == 0)
    {
        return 0u;
    }
    if (vma64_find(pid, scratch) == 0)
    {
        if (vma64_map_anon(
                pid,
                scratch,
                MACOS_CF64_PAGE_BYTES,
                VMA64_PROT_READ | VMA64_PROT_WRITE,
                VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
            != scratch)
        {
            return 0u;
        }
        ++g_macos_cf64_scratch_map_count;
    }
    if (macos_cf64_user_range_ready(pid, scratch, (u64)object->length, 1u) == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < object->length; ++index)
    {
        ((volatile u8 *)(u64)scratch)[index] = object->bytes[index];
    }

    return 1u;
}

void macos_cf64_init(void)
{
    if (g_macos_cf64_initialized != 0u)
    {
        return;
    }

    g_macos_cf64_initialized = 1u;
}

u32 macos_cf64_load(u32 pid, u64 image_base, macos_cf64_load_result_t *out_result)
{
    u32 index;
    u64 text_base;
    u64 rodata_base;
    static const char cf_name[] = "CoreFoundation.framework";

    macos_cf64_init();

    if (out_result == 0)
    {
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_NULL, 0ull);
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + (u64)MACOS_CF64_IMAGE_BYTES;
    out_result->allocator_get_default_fn =
        image_base + (u64)MACOS_CF64_RVA_ALLOCATOR_GET_DEFAULT;
    out_result->release_fn = image_base + (u64)MACOS_CF64_RVA_RELEASE;
    out_result->retain_fn = image_base + (u64)MACOS_CF64_RVA_RETAIN;
    out_result->string_create_fn = image_base + (u64)MACOS_CF64_RVA_STRING_CREATE;
    out_result->string_get_cstring_fn =
        image_base + (u64)MACOS_CF64_RVA_STRING_GET_CSTRING;
    out_result->show_fn = image_base + (u64)MACOS_CF64_RVA_SHOW;
    out_result->image_bytes = MACOS_CF64_IMAGE_BYTES;
    out_result->section_count = 2u;
    out_result->mapped_count = 0u;
    out_result->symbol_count = MACOS_CF64_SYMBOL_COUNT;
    out_result->text_checksum = 2166136261u;
    out_result->rodata_checksum = 2166136261u;
    out_result->name_checksum = 2166136261u;
    out_result->text_protection = 0u;
    out_result->rodata_protection = 0u;
    out_result->context_stored = (persona64_context_for_process(pid) != 0) ? 1u : 0u;
    out_result->error = MACOS_CF64_ERROR_NONE;

    if (macos_cf64_valid_persona(pid) == 0u)
    {
        out_result->error = MACOS_CF64_ERROR_PERSONA;
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_PERSONA, 0ull);
    }
    if ((image_base != MACOS_CF64_DEFAULT_BASE)
        || ((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        out_result->error = MACOS_CF64_ERROR_BASE;
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_BASE, 0ull);
    }

    text_base = image_base + (u64)MACOS_CF64_TEXT_RVA;
    rodata_base = image_base + (u64)MACOS_CF64_RODATA_RVA;
    if ((vma64_find(pid, text_base) != 0) || (vma64_find(pid, rodata_base) != 0))
    {
        out_result->error = MACOS_CF64_ERROR_ALREADY_MAPPED;
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_ALREADY_MAPPED, 0ull);
    }

    if (vma64_map_anon(
            pid,
            text_base,
            MACOS_CF64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != text_base)
    {
        out_result->error = MACOS_CF64_ERROR_MAP;
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_MAP, 0ull);
    }
    ++out_result->mapped_count;

    for (index = 0u; index < MACOS_CF64_SYMBOL_COUNT; ++index)
    {
        macos_cf64_write_stub(
            image_base + (u64)g_macos_cf64_exports[index].rva,
            g_macos_cf64_exports[index].symbol_id);
    }
    out_result->text_checksum = macos_cf64_checksum_range(
        text_base,
        MACOS_CF64_TEXT_PATTERN_BYTES * MACOS_CF64_SYMBOL_COUNT);

    if (vma64_protect(
            pid,
            text_base,
            MACOS_CF64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_EXECUTE)
        == 0u)
    {
        out_result->error = MACOS_CF64_ERROR_MAP;
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_MAP, 0ull);
    }
    out_result->text_protection = paging64_user_page_protection(text_base);

    if (vma64_map_anon(
            pid,
            rodata_base,
            MACOS_CF64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != rodata_base)
    {
        out_result->error = MACOS_CF64_ERROR_MAP;
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_MAP, 0ull);
    }
    ++out_result->mapped_count;

    out_result->name_checksum = macos_cf64_write_bytes(
        rodata_base,
        cf_name,
        (u32)sizeof(cf_name) - 1u);
    out_result->rodata_checksum = macos_cf64_checksum_range(
        rodata_base,
        (u32)sizeof(cf_name) - 1u);

    if (vma64_protect(pid, rodata_base, MACOS_CF64_PAGE_BYTES, VMA64_PROT_READ) == 0u)
    {
        out_result->error = MACOS_CF64_ERROR_MAP;
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_MAP, 0ull);
    }
    out_result->rodata_protection = paging64_user_page_protection(rodata_base);
    ++g_macos_cf64_load_count;
    return MACOS_CF64_OK;
}

u64 macos_cf64_resolve_symbol(const char *name, u32 name_length)
{
    u32 index;

    if (name == 0)
    {
        return 0ull;
    }

    for (index = 0u; index < MACOS_CF64_SYMBOL_COUNT; ++index)
    {
        const macos_cf64_export_t *record = &g_macos_cf64_exports[index];
        if (macos_cf64_export_matches(name, name_length, record) != 0u)
        {
            return MACOS_CF64_DEFAULT_BASE + (u64)record->rva;
        }
    }

    return 0ull;
}

u32 macos_cf64_symbol_id_for_address(u64 address)
{
    u32 index;

    for (index = 0u; index < MACOS_CF64_SYMBOL_COUNT; ++index)
    {
        if (address == (MACOS_CF64_DEFAULT_BASE + (u64)g_macos_cf64_exports[index].rva))
        {
            return g_macos_cf64_exports[index].symbol_id;
        }
    }

    return MACOS_CF64_SYMBOL_NONE;
}

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
    macos_cf64_call_result_t *out_result)
{
    u32 symbol_id;
    u64 value = 0ull;
    u32 byte_count = 0u;
    u32 checksum = 2166136261u;

    (void)arg4;
    (void)arg5;

    macos_cf64_init();

    if (out_result == 0)
    {
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_NULL, rip);
    }

    symbol_id = macos_cf64_symbol_id_for_address(shim_address);
    if ((symbol_id == MACOS_CF64_SYMBOL_NONE) || (macos_cf64_valid_persona(pid) == 0u))
    {
        u32 error = (symbol_id == MACOS_CF64_SYMBOL_NONE)
            ? MACOS_CF64_ERROR_SYMBOL
            : MACOS_CF64_ERROR_PERSONA;
        macos_cf64_set_call_result(out_result, 0ull, symbol_id, error, 0u, checksum);
        return macos_cf64_record_denial(pid, error, rip);
    }

    ++g_macos_cf64_call_count;

    if (symbol_id == MACOS_CF64_SYMBOL_ALLOCATOR_GET_DEFAULT)
    {
        value = MACOS_CF64_ALLOCATOR_DEFAULT;
    }
    else if (symbol_id == MACOS_CF64_SYMBOL_STRING_CREATE)
    {
        macos_cf64_object_t *object;
        u32 index = MACOS_CF64_OBJECT_POOL_SIZE;

        if (((arg0 != 0ull) && (arg0 != MACOS_CF64_ALLOCATOR_DEFAULT))
            || ((u32)(arg2 & 0xFFFFFFFFu) != MACOS_CF64_ENCODING_UTF8))
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_ENCODING,
                0u,
                checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_ENCODING, rip);
        }

        object = macos_cf64_alloc_object(pid, &index);
        if (object == 0)
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_POOL,
                0u,
                checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_POOL, rip);
        }
        if (macos_cf64_copy_user_string_into_object(pid, arg1, object) == 0u)
        {
            macos_cf64_clear_object(object);
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_FAULT,
                0u,
                checksum);
            return macos_cf64_record_fault(pid, MACOS_CF64_ERROR_FAULT, rip);
        }

        ++g_macos_cf64_string_create_count;
        value = macos_cf64_handle_for_index(index);
        byte_count = object->length;
        checksum = object->checksum;
    }
    else if (symbol_id == MACOS_CF64_SYMBOL_RETAIN)
    {
        macos_cf64_object_t *object = macos_cf64_object_for_handle(pid, arg0);

        if (object == 0)
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_TYPE,
                0u,
                checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_TYPE, rip);
        }

        ++object->retain_count;
        ++g_macos_cf64_retain_count;
        value = arg0;
        byte_count = object->retain_count;
        checksum = object->checksum;
    }
    else if (symbol_id == MACOS_CF64_SYMBOL_RELEASE)
    {
        macos_cf64_object_t *object = macos_cf64_object_for_handle(pid, arg0);

        if (object == 0)
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_TYPE,
                0u,
                checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_TYPE, rip);
        }

        ++g_macos_cf64_release_count;
        if (object->retain_count > 0u)
        {
            --object->retain_count;
        }
        value = (u64)object->retain_count;
        byte_count = object->retain_count;
        checksum = object->checksum;
        if (object->retain_count == 0u)
        {
            macos_cf64_clear_object(object);
        }
    }
    else if (symbol_id == MACOS_CF64_SYMBOL_STRING_GET_CSTRING)
    {
        macos_cf64_object_t *object = macos_cf64_object_for_handle(pid, arg0);

        if ((object == 0) || (object->type != MACOS_CF64_OBJECT_STRING))
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_TYPE,
                0u,
                checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_TYPE, rip);
        }
        if ((u32)(arg3 & 0xFFFFFFFFu) != MACOS_CF64_ENCODING_UTF8)
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_ENCODING,
                0u,
                checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_ENCODING, rip);
        }
        if (macos_cf64_copy_object_to_user(pid, object, arg1, arg2) == 0u)
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_RANGE,
                0u,
                checksum);
            return macos_cf64_record_fault(pid, MACOS_CF64_ERROR_RANGE, rip);
        }

        ++g_macos_cf64_get_cstring_count;
        value = 1ull;
        byte_count = object->length;
        checksum = object->checksum;
    }
    else if (symbol_id == MACOS_CF64_SYMBOL_SHOW)
    {
        macos_cf64_object_t *object = macos_cf64_object_for_handle(pid, arg0);
        macos_shim64_call_result_t write_result;

        if ((object == 0) || (object->type != MACOS_CF64_OBJECT_STRING))
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_TYPE,
                0u,
                checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_TYPE, rip);
        }
        if (macos_cf64_stage_show_bytes(pid, object) == 0u)
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_MAP,
                0u,
                object->checksum);
            return macos_cf64_record_fault(pid, MACOS_CF64_ERROR_MAP, rip);
        }

        if (macos_shim64_call(
                pid,
                MACOS_SHIM64_ADDR_WRITE,
                1ull,
                MACOS_CF64_SCRATCH_BASE,
                (u64)object->length,
                0ull,
                0ull,
                0ull,
                rip,
                &write_result) == MACOS_SHIM64_DENIED)
        {
            macos_cf64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_CF64_ERROR_SHIM,
                0u,
                object->checksum);
            return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_SHIM, rip);
        }

        ++g_macos_cf64_show_count;
        value = write_result.value;
        byte_count = write_result.byte_count;
        checksum = object->checksum;
    }
    else
    {
        macos_cf64_set_call_result(
            out_result,
            0ull,
            symbol_id,
            MACOS_CF64_ERROR_SYMBOL,
            0u,
            checksum);
        return macos_cf64_record_denial(pid, MACOS_CF64_ERROR_SYMBOL, rip);
    }

    macos_cf64_set_call_result(
        out_result,
        value,
        symbol_id,
        MACOS_CF64_ERROR_NONE,
        byte_count,
        checksum);
    return MACOS_CF64_OK;
}

u32 macos_cf64_release_process(u32 pid)
{
    u32 released = 0u;
    u32 index;

    if (pid == PROCESS64_INVALID_PID)
    {
        return 0u;
    }

    if (vma64_find(pid, MACOS_CF64_ADDR_ALLOCATOR_GET_DEFAULT) != 0)
    {
        released += vma64_unmap(
            pid,
            MACOS_CF64_ADDR_ALLOCATOR_GET_DEFAULT,
            MACOS_CF64_PAGE_BYTES);
    }
    if (vma64_find(pid, MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RODATA_RVA) != 0)
    {
        released += vma64_unmap(
            pid,
            MACOS_CF64_DEFAULT_BASE + (u64)MACOS_CF64_RODATA_RVA,
            MACOS_CF64_PAGE_BYTES);
    }
    if (vma64_find(pid, MACOS_CF64_SCRATCH_BASE) != 0)
    {
        released += vma64_unmap(pid, MACOS_CF64_SCRATCH_BASE, MACOS_CF64_PAGE_BYTES);
    }

    for (index = 0u; index < MACOS_CF64_OBJECT_POOL_SIZE; ++index)
    {
        if ((g_macos_cf64_objects[index].active != 0u)
            && (g_macos_cf64_objects[index].pid == pid))
        {
            macos_cf64_clear_object(&g_macos_cf64_objects[index]);
        }
    }

    return released;
}

u32 macos_cf64_symbol_count(void) { return MACOS_CF64_SYMBOL_COUNT; }
u32 macos_cf64_load_count(void) { return g_macos_cf64_load_count; }
u32 macos_cf64_call_count(void) { return g_macos_cf64_call_count; }
u32 macos_cf64_string_create_count(void) { return g_macos_cf64_string_create_count; }
u32 macos_cf64_get_cstring_count(void) { return g_macos_cf64_get_cstring_count; }
u32 macos_cf64_show_count(void) { return g_macos_cf64_show_count; }
u32 macos_cf64_retain_count(void) { return g_macos_cf64_retain_count; }
u32 macos_cf64_release_count(void) { return g_macos_cf64_release_count; }
u32 macos_cf64_denial_count(void) { return g_macos_cf64_denial_count; }
u32 macos_cf64_fault_count(void) { return g_macos_cf64_fault_count; }
u32 macos_cf64_scratch_map_count(void) { return g_macos_cf64_scratch_map_count; }

u32 macos_cf64_live_object_count(u32 pid)
{
    u32 index;
    u32 count = 0u;

    for (index = 0u; index < MACOS_CF64_OBJECT_POOL_SIZE; ++index)
    {
        if ((g_macos_cf64_objects[index].active != 0u)
            && (g_macos_cf64_objects[index].pid == pid))
        {
            ++count;
        }
    }

    return count;
}

u32 macos_cf64_last_symbol(void) { return g_macos_cf64_last_symbol; }
u32 macos_cf64_last_error(void) { return g_macos_cf64_last_error; }
u64 macos_cf64_last_result(void) { return g_macos_cf64_last_result; }
u32 macos_cf64_last_byte_count(void) { return g_macos_cf64_last_byte_count; }
u32 macos_cf64_last_checksum(void) { return g_macos_cf64_last_checksum; }
