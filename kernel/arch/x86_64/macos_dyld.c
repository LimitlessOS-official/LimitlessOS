#include "macos_dyld_x64.h"

#include "macos_cf_x64.h"
#include "macos_shim_x64.h"
#include "macho64_x64.h"
#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * N.4 adds the LimitlessOS-owned libdyld.dylib shim surface. It integrates
 * with macho64.c for curated dyld symbol binding, macos_shim.c for resolving
 * libSystem lazy targets, vma_x64.c for RX/RO shim pages and writable lazy
 * pointer slots, and persona_audit_x64.c for truthful denial records. The
 * checkpoint proves dyld_stub_binder performs a real on-demand bind into a
 * writable user slot, _dyld_image_count and _dyld_get_image_name expose a
 * bounded image table, unknown symbols are denied and audited, and all mapped
 * pages release cleanly without entering the BIOS build slice.
 */

typedef struct macos_dyld64_export
{
    const char *name;
    u32 name_length;
    u32 symbol_id;
    u32 rva;
} macos_dyld64_export_t;

typedef struct macos_dyld64_image
{
    const char *name;
    u32 name_length;
    u32 rodata_offset;
} macos_dyld64_image_t;

static const macos_dyld64_export_t g_macos_dyld64_exports[
    MACOS_DYLD64_SYMBOL_COUNT] = {
    { "dyld_stub_binder", 16u, MACOS_DYLD64_SYMBOL_STUB_BINDER, MACOS_DYLD64_RVA_STUB_BINDER },
    { "_dyld_get_image_name", 20u, MACOS_DYLD64_SYMBOL_GET_IMAGE_NAME, MACOS_DYLD64_RVA_GET_IMAGE_NAME },
    { "_dyld_image_count", 17u, MACOS_DYLD64_SYMBOL_IMAGE_COUNT, MACOS_DYLD64_RVA_IMAGE_COUNT }
};

static const macos_dyld64_image_t g_macos_dyld64_images[
    MACOS_DYLD64_IMAGE_COUNT] = {
    { "main", 4u, 0x00000080u },
    { "/usr/lib/libSystem.B.dylib", 26u, 0x000000C0u },
    { "/usr/lib/system/libdyld.dylib", 30u, 0x00000100u }
};

static u32 g_macos_dyld64_initialized = 0u;
static u32 g_macos_dyld64_load_count = 0u;
static u32 g_macos_dyld64_call_count = 0u;
static u32 g_macos_dyld64_lazy_bind_count = 0u;
static u32 g_macos_dyld64_image_query_count = 0u;
static u32 g_macos_dyld64_denial_count = 0u;
static u32 g_macos_dyld64_fault_count = 0u;
static u32 g_macos_dyld64_last_symbol = MACOS_DYLD64_SYMBOL_NONE;
static u32 g_macos_dyld64_last_error = MACOS_DYLD64_ERROR_NONE;
static u64 g_macos_dyld64_last_result = 0ull;
static u32 g_macos_dyld64_last_byte_count = 0u;
static u32 g_macos_dyld64_last_checksum = 0u;

static u32 macos_dyld64_mix_checksum(u32 checksum, u8 value)
{
    checksum ^= (u32)value;
    checksum *= 16777619u;
    return checksum;
}

static u32 macos_dyld64_name_matches(
    const char *left,
    u32 left_length,
    const char *right,
    u32 right_length)
{
    u32 index;

    if ((left == 0) || (right == 0))
    {
        return 0u;
    }
    if (left_length != right_length)
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

static u32 macos_dyld64_export_matches(
    const char *name,
    u32 name_length,
    const macos_dyld64_export_t *record)
{
    if ((name == 0) || (record == 0))
    {
        return 0u;
    }

    if (macos_dyld64_name_matches(name, name_length, record->name, record->name_length) != 0u)
    {
        return 1u;
    }
    if ((record->name_length > 1u) && (record->name[0] == (char)'_'))
    {
        return macos_dyld64_name_matches(
            name,
            name_length,
            record->name + 1,
            record->name_length - 1u);
    }
    if ((name_length > 1u) && (name[0] == (char)'_'))
    {
        return macos_dyld64_name_matches(
            name + 1,
            name_length - 1u,
            record->name,
            record->name_length);
    }

    return 0u;
}

static u32 macos_dyld64_valid_persona(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_MACOS_MACHO))
        ? 1u
        : 0u;
}

static u32 macos_dyld64_range_overflows(u64 address, u64 byte_count)
{
    return ((byte_count != 0ull) && ((address + byte_count) < address)) ? 1u : 0u;
}

static u32 macos_dyld64_user_range_ready(
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
    if ((address == 0ull) || (macos_dyld64_range_overflows(address, byte_count) != 0u))
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

static u32 macos_dyld64_cstring_length(
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

        if (macos_dyld64_user_range_ready(pid, current, 1ull, 0u) == 0u)
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
        checksum = macos_dyld64_mix_checksum(checksum, value);
    }

    return 0u;
}

static void macos_dyld64_copy_user_string(
    u64 address,
    char *out,
    u32 length)
{
    u32 index;

    if (out == 0)
    {
        return;
    }
    for (index = 0u; index < length; ++index)
    {
        out[index] = (char)((volatile const u8 *)(u64)address)[index];
    }
}

static void macos_dyld64_write_stub(u64 address, u32 symbol_id)
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

static u32 macos_dyld64_checksum_range(u64 address, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = macos_dyld64_mix_checksum(
            checksum,
            ((volatile const u8 *)(u64)address)[index]);
    }

    return checksum;
}

static u32 macos_dyld64_checksum_bytes(const char *bytes, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (bytes == 0)
    {
        return checksum;
    }
    for (index = 0u; index < byte_count; ++index)
    {
        checksum = macos_dyld64_mix_checksum(checksum, (u8)bytes[index]);
    }

    return checksum;
}

static u32 macos_dyld64_write_bytes(u64 address, const char *bytes, u32 byte_count)
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
        checksum = macos_dyld64_mix_checksum(checksum, (u8)bytes[index]);
    }
    ((volatile u8 *)(u64)address)[byte_count] = 0u;

    return checksum;
}

static void macos_dyld64_set_call_result(
    macos_dyld64_call_result_t *result,
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

    g_macos_dyld64_last_symbol = symbol_id;
    g_macos_dyld64_last_error = error;
    g_macos_dyld64_last_result = value;
    g_macos_dyld64_last_byte_count = byte_count;
    g_macos_dyld64_last_checksum = checksum;
}

static u32 macos_dyld64_record_denial(u32 pid, u32 error, u64 rip)
{
    ++g_macos_dyld64_denial_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
        (u16)(error & 0xFFFFu),
        PERSONA_AUDIT64_RESULT_DENY,
        rip);
    return MACOS_DYLD64_DENIED;
}

static u32 macos_dyld64_record_fault(u32 pid, u32 error, u64 rip)
{
    ++g_macos_dyld64_fault_count;
    (void)persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
        (u16)(error & 0xFFFFu),
        PERSONA_AUDIT64_RESULT_DENY,
        rip);
    return MACOS_DYLD64_DENIED;
}

static u64 macos_dyld64_resolve_runtime_symbol(
    u32 shim_id,
    const char *symbol,
    u32 symbol_length)
{
    if ((symbol == 0) || (symbol_length == 0u))
    {
        return 0ull;
    }
    if (shim_id == MACHO64_SHIM_LIBSYSTEM)
    {
        return macos_shim64_resolve_libsystem_symbol(symbol, symbol_length);
    }
    if (shim_id == MACHO64_SHIM_LIBDYLD)
    {
        return macos_dyld64_resolve_symbol(symbol, symbol_length);
    }
    if (shim_id == MACHO64_SHIM_COREFOUNDATION)
    {
        return macos_cf64_resolve_symbol(symbol, symbol_length);
    }

    return 0ull;
}

void macos_dyld64_init(void)
{
    if (g_macos_dyld64_initialized != 0u)
    {
        return;
    }

    g_macos_dyld64_initialized = 1u;
}

u32 macos_dyld64_load(
    u32 pid,
    u64 image_base,
    macos_dyld64_load_result_t *out_result)
{
    u32 index;
    u64 text_base;
    u64 rodata_base;
    u32 name_checksum = 2166136261u;

    macos_dyld64_init();

    if (out_result == 0)
    {
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_NULL, 0ull);
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + (u64)MACOS_DYLD64_IMAGE_BYTES;
    out_result->stub_binder_fn = image_base + (u64)MACOS_DYLD64_RVA_STUB_BINDER;
    out_result->get_image_name_fn = image_base + (u64)MACOS_DYLD64_RVA_GET_IMAGE_NAME;
    out_result->image_count_fn = image_base + (u64)MACOS_DYLD64_RVA_IMAGE_COUNT;
    out_result->image_bytes = MACOS_DYLD64_IMAGE_BYTES;
    out_result->section_count = 2u;
    out_result->mapped_count = 0u;
    out_result->symbol_count = MACOS_DYLD64_SYMBOL_COUNT;
    out_result->image_name_count = MACOS_DYLD64_IMAGE_COUNT;
    out_result->text_checksum = 2166136261u;
    out_result->rodata_checksum = 2166136261u;
    out_result->name_checksum = 2166136261u;
    out_result->text_protection = 0u;
    out_result->rodata_protection = 0u;
    out_result->context_stored = (persona64_context_for_process(pid) != 0) ? 1u : 0u;
    out_result->error = MACOS_DYLD64_ERROR_NONE;

    if (macos_dyld64_valid_persona(pid) == 0u)
    {
        out_result->error = MACOS_DYLD64_ERROR_PERSONA;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_PERSONA, 0ull);
    }
    if ((image_base != MACOS_DYLD64_DEFAULT_BASE)
        || ((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        out_result->error = MACOS_DYLD64_ERROR_BASE;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_BASE, 0ull);
    }

    text_base = image_base + (u64)MACOS_DYLD64_TEXT_RVA;
    rodata_base = image_base + (u64)MACOS_DYLD64_RODATA_RVA;
    if ((vma64_find(pid, text_base) != 0) || (vma64_find(pid, rodata_base) != 0))
    {
        out_result->error = MACOS_DYLD64_ERROR_ALREADY_MAPPED;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_ALREADY_MAPPED, 0ull);
    }

    if (vma64_map_anon(
            pid,
            text_base,
            MACOS_DYLD64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != text_base)
    {
        out_result->error = MACOS_DYLD64_ERROR_MAP;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_MAP, 0ull);
    }
    ++out_result->mapped_count;

    for (index = 0u; index < MACOS_DYLD64_SYMBOL_COUNT; ++index)
    {
        macos_dyld64_write_stub(
            image_base + (u64)g_macos_dyld64_exports[index].rva,
            g_macos_dyld64_exports[index].symbol_id);
    }
    out_result->text_checksum = macos_dyld64_checksum_range(
        text_base,
        MACOS_DYLD64_TEXT_PATTERN_BYTES * MACOS_DYLD64_SYMBOL_COUNT);

    if (vma64_protect(
            pid,
            text_base,
            MACOS_DYLD64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_EXECUTE)
        == 0u)
    {
        out_result->error = MACOS_DYLD64_ERROR_MAP;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_MAP, 0ull);
    }
    out_result->text_protection = paging64_user_page_protection(text_base);

    if (vma64_map_anon(
            pid,
            rodata_base,
            MACOS_DYLD64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != rodata_base)
    {
        out_result->error = MACOS_DYLD64_ERROR_MAP;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_MAP, 0ull);
    }
    ++out_result->mapped_count;

    for (index = 0u; index < MACOS_DYLD64_IMAGE_COUNT; ++index)
    {
        u32 checksum = macos_dyld64_write_bytes(
            rodata_base + (u64)g_macos_dyld64_images[index].rodata_offset,
            g_macos_dyld64_images[index].name,
            g_macos_dyld64_images[index].name_length);
        name_checksum ^= checksum;
        name_checksum *= 16777619u;
    }
    out_result->name_checksum = name_checksum;
    out_result->rodata_checksum = macos_dyld64_checksum_range(rodata_base, 0x00000120u);

    if (vma64_protect(pid, rodata_base, MACOS_DYLD64_PAGE_BYTES, VMA64_PROT_READ) == 0u)
    {
        out_result->error = MACOS_DYLD64_ERROR_MAP;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_MAP, 0ull);
    }
    out_result->rodata_protection = paging64_user_page_protection(rodata_base);
    ++g_macos_dyld64_load_count;
    return MACOS_DYLD64_OK;
}

u64 macos_dyld64_resolve_symbol(const char *name, u32 name_length)
{
    u32 index;

    if (name == 0)
    {
        return 0ull;
    }

    for (index = 0u; index < MACOS_DYLD64_SYMBOL_COUNT; ++index)
    {
        const macos_dyld64_export_t *record = &g_macos_dyld64_exports[index];
        if (macos_dyld64_export_matches(name, name_length, record) != 0u)
        {
            return MACOS_DYLD64_DEFAULT_BASE + (u64)record->rva;
        }
    }

    return 0ull;
}

u32 macos_dyld64_symbol_id_for_address(u64 address)
{
    u32 index;

    for (index = 0u; index < MACOS_DYLD64_SYMBOL_COUNT; ++index)
    {
        if (address == (MACOS_DYLD64_DEFAULT_BASE + (u64)g_macos_dyld64_exports[index].rva))
        {
            return g_macos_dyld64_exports[index].symbol_id;
        }
    }

    return MACOS_DYLD64_SYMBOL_NONE;
}

u32 macos_dyld64_bind_lazy(
    u32 pid,
    u64 slot,
    u32 shim_id,
    const char *symbol,
    u32 symbol_length,
    macos_dyld64_lazy_bind_result_t *out_result)
{
    u64 value;
    u32 checksum = 2166136261u;
    u32 index;

    macos_dyld64_init();

    if (out_result != 0)
    {
        out_result->slot = slot;
        out_result->before = 0ull;
        out_result->after = 0ull;
        out_result->shim_id = shim_id;
        out_result->symbol_length = symbol_length;
        out_result->symbol_checksum = checksum;
        out_result->error = MACOS_DYLD64_ERROR_NONE;
    }
    if ((out_result == 0) || (symbol == 0) || (symbol_length == 0u)
        || (symbol_length > MACOS_DYLD64_STRING_LIMIT))
    {
        if (out_result != 0)
        {
            out_result->error = MACOS_DYLD64_ERROR_NULL;
        }
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_NULL, 0ull);
    }
    if (macos_dyld64_valid_persona(pid) == 0u)
    {
        out_result->error = MACOS_DYLD64_ERROR_PERSONA;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_PERSONA, 0ull);
    }
    if (macos_dyld64_user_range_ready(pid, slot, 8ull, 1u) == 0u)
    {
        out_result->error = MACOS_DYLD64_ERROR_FAULT;
        return macos_dyld64_record_fault(pid, MACOS_DYLD64_ERROR_FAULT, 0ull);
    }

    for (index = 0u; index < symbol_length; ++index)
    {
        checksum = macos_dyld64_mix_checksum(checksum, (u8)symbol[index]);
    }
    out_result->symbol_checksum = checksum;

    value = macos_dyld64_resolve_runtime_symbol(shim_id, symbol, symbol_length);
    if (value == 0ull)
    {
        out_result->error = MACOS_DYLD64_ERROR_SYMBOL;
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_SYMBOL, 0ull);
    }

    out_result->before = *((volatile const u64 *)(u64)slot);
    *((volatile u64 *)(u64)slot) = value;
    out_result->after = *((volatile const u64 *)(u64)slot);
    ++g_macos_dyld64_lazy_bind_count;
    return MACOS_DYLD64_OK;
}

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
    macos_dyld64_call_result_t *out_result)
{
    u32 symbol_id;
    u64 value = 0ull;
    u32 byte_count = 0u;
    u32 checksum = 2166136261u;

    (void)arg4;
    (void)arg5;
    (void)arg3;

    macos_dyld64_init();

    if (out_result == 0)
    {
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_NULL, rip);
    }

    symbol_id = macos_dyld64_symbol_id_for_address(shim_address);
    if ((symbol_id == MACOS_DYLD64_SYMBOL_NONE) || (macos_dyld64_valid_persona(pid) == 0u))
    {
        u32 error = (symbol_id == MACOS_DYLD64_SYMBOL_NONE)
            ? MACOS_DYLD64_ERROR_SYMBOL
            : MACOS_DYLD64_ERROR_PERSONA;
        macos_dyld64_set_call_result(out_result, 0ull, symbol_id, error, 0u, checksum);
        return macos_dyld64_record_denial(pid, error, rip);
    }

    ++g_macos_dyld64_call_count;

    if (symbol_id == MACOS_DYLD64_SYMBOL_STUB_BINDER)
    {
        char symbol_name[MACOS_DYLD64_STRING_LIMIT];
        macos_dyld64_lazy_bind_result_t lazy_result;
        u32 symbol_length = 0u;

        if (macos_dyld64_cstring_length(
                pid,
                arg2,
                MACOS_DYLD64_STRING_LIMIT,
                &symbol_length,
                &checksum) == 0u)
        {
            macos_dyld64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                MACOS_DYLD64_ERROR_FAULT,
                0u,
                checksum);
            return macos_dyld64_record_fault(pid, MACOS_DYLD64_ERROR_FAULT, rip);
        }
        macos_dyld64_copy_user_string(arg2, symbol_name, symbol_length);
        if (macos_dyld64_bind_lazy(
                pid,
                arg0,
                (u32)(arg1 & 0xFFFFFFFFu),
                symbol_name,
                symbol_length,
                &lazy_result) == MACOS_DYLD64_DENIED)
        {
            macos_dyld64_set_call_result(
                out_result,
                0ull,
                symbol_id,
                lazy_result.error,
                0u,
                lazy_result.symbol_checksum);
            return MACOS_DYLD64_DENIED;
        }
        value = lazy_result.after;
        byte_count = 8u;
        checksum = lazy_result.symbol_checksum;
    }
    else if (symbol_id == MACOS_DYLD64_SYMBOL_IMAGE_COUNT)
    {
        ++g_macos_dyld64_image_query_count;
        value = (u64)MACOS_DYLD64_IMAGE_COUNT;
        byte_count = MACOS_DYLD64_IMAGE_COUNT;
    }
    else if (symbol_id == MACOS_DYLD64_SYMBOL_GET_IMAGE_NAME)
    {
        ++g_macos_dyld64_image_query_count;
        if (arg0 >= (u64)MACOS_DYLD64_IMAGE_COUNT)
        {
            value = 0ull;
        }
        else
        {
            const macos_dyld64_image_t *image = &g_macos_dyld64_images[(u32)arg0];
            value = MACOS_DYLD64_DEFAULT_BASE
                + (u64)MACOS_DYLD64_RODATA_RVA
                + (u64)image->rodata_offset;
            byte_count = image->name_length;
            checksum = macos_dyld64_checksum_bytes(image->name, image->name_length);
        }
    }
    else
    {
        macos_dyld64_set_call_result(
            out_result,
            0ull,
            symbol_id,
            MACOS_DYLD64_ERROR_SYMBOL,
            0u,
            checksum);
        return macos_dyld64_record_denial(pid, MACOS_DYLD64_ERROR_SYMBOL, rip);
    }

    macos_dyld64_set_call_result(
        out_result,
        value,
        symbol_id,
        MACOS_DYLD64_ERROR_NONE,
        byte_count,
        checksum);
    return MACOS_DYLD64_OK;
}

u32 macos_dyld64_release_process(u32 pid)
{
    u32 released = 0u;

    if (pid == PROCESS64_INVALID_PID)
    {
        return 0u;
    }

    if (vma64_find(pid, MACOS_DYLD64_ADDR_STUB_BINDER) != 0)
    {
        released += vma64_unmap(pid, MACOS_DYLD64_ADDR_STUB_BINDER, MACOS_DYLD64_PAGE_BYTES);
    }
    if (vma64_find(pid, MACOS_DYLD64_DEFAULT_BASE + (u64)MACOS_DYLD64_RODATA_RVA) != 0)
    {
        released += vma64_unmap(
            pid,
            MACOS_DYLD64_DEFAULT_BASE + (u64)MACOS_DYLD64_RODATA_RVA,
            MACOS_DYLD64_PAGE_BYTES);
    }

    return released;
}

u32 macos_dyld64_symbol_count(void) { return MACOS_DYLD64_SYMBOL_COUNT; }
u32 macos_dyld64_image_count(void) { return MACOS_DYLD64_IMAGE_COUNT; }
u32 macos_dyld64_load_count(void) { return g_macos_dyld64_load_count; }
u32 macos_dyld64_call_count(void) { return g_macos_dyld64_call_count; }
u32 macos_dyld64_lazy_bind_count(void) { return g_macos_dyld64_lazy_bind_count; }
u32 macos_dyld64_image_query_count(void) { return g_macos_dyld64_image_query_count; }
u32 macos_dyld64_denial_count(void) { return g_macos_dyld64_denial_count; }
u32 macos_dyld64_fault_count(void) { return g_macos_dyld64_fault_count; }
u32 macos_dyld64_last_symbol(void) { return g_macos_dyld64_last_symbol; }
u32 macos_dyld64_last_error(void) { return g_macos_dyld64_last_error; }
u64 macos_dyld64_last_result(void) { return g_macos_dyld64_last_result; }
u32 macos_dyld64_last_byte_count(void) { return g_macos_dyld64_last_byte_count; }
u32 macos_dyld64_last_checksum(void) { return g_macos_dyld64_last_checksum; }
