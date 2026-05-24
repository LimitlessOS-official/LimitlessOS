#include "macho64_x64.h"
#include "macos_cf_x64.h"
#include "macos_dyld_x64.h"
#include "macos_shim_x64.h"
#include "paging_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#include "vma_x64.h"
#include "x64.h"

/*
 * M.1-M.8 add the first LimitlessOS-owned Mach-O parser and launch-shape
 * surface. The code
 * integrates with macho64_x64.h and the persona format detector by validating
 * thin little-endian x86-64 Mach-O headers and slicing big-endian FAT_MAGIC
 * universal binaries before walking LC_SEGMENT_64 commands into VMA-backed user
 * mappings, resolving LC_MAIN to a ring-3 entry RIP plus initial stack, and
 * recording LC_LOAD_DYLIB dependencies against a curated macOS shim registry,
 * then applying a bounded LC_DYLD_INFO_ONLY rebase/bind stream and setting up
 * macOS thread-local storage from documented TLV section records, and building
 * a macOS-style initial stack with argc, argv, envp, Apple strings, and bounded
 * auxiliary entries. It integrates with vma_x64.c for fixed anonymous mappings
 * and final permission updates.
 * The scaffold checkpoints prove __TEXT, __DATA, and __LINKEDIT are mapped
 * with bytes copied, BSS zeroed, R/W/X permissions applied, LC_MAIN entryoff
 * lands inside __TEXT, libSystem is recognized as a dependency, weak missing
 * dylibs are absent without failing load prep, malformed or required-unshimmed
 * entries are denied, one base-relative pointer is rebased, and a libSystem
 * write symbol slot is filled from the shim registry, a macOS persona can read
 * a valid TLS pointer through GS:0, and the initial stack layout has truthful
 * null terminators, checksums, and denial telemetry.
 */

typedef struct macho64_segment_runtime
{
    u32 present;
    u64 vmaddr;
    u64 vmsize;
} macho64_segment_runtime_t;

static void macho64_clear_header(macho64_header_t *header)
{
    if (header == 0)
    {
        return;
    }

    header->magic = 0u;
    header->cpu_type = 0u;
    header->cpu_subtype = 0u;
    header->filetype = 0u;
    header->ncmds = 0u;
    header->sizeofcmds = 0u;
    header->flags = 0u;
    header->reserved = 0u;
    header->load_command_offset = 0u;
    header->load_command_end = 0u;
    header->error = MACHO64_ERROR_NONE;
}

static void macho64_set_header_error(macho64_header_t *header, u32 error)
{
    if (header != 0)
    {
        header->error = error;
    }
}

static u32 macho64_read_le32(const u8 *data)
{
    return ((u32)data[0])
        | ((u32)data[1] << 8)
        | ((u32)data[2] << 16)
        | ((u32)data[3] << 24);
}

static u64 macho64_read_le64(const u8 *data)
{
    return ((u64)macho64_read_le32(data))
        | ((u64)macho64_read_le32(data + 4u) << 32);
}

static u32 macho64_read_be32(const u8 *data)
{
    return ((u32)data[0] << 24)
        | ((u32)data[1] << 16)
        | ((u32)data[2] << 8)
        | ((u32)data[3]);
}

static u32 macho64_range_available(u32 size, u32 offset, u32 bytes)
{
    if (offset > size)
    {
        return 0u;
    }
    if (bytes > (size - offset))
    {
        return 0u;
    }
    return 1u;
}

static u32 macho64_range64_available(u32 size, u64 offset, u64 bytes)
{
    if (offset > (u64)size)
    {
        return 0u;
    }

    return (bytes <= ((u64)size - offset)) ? 1u : 0u;
}

static u64 macho64_align_down(u64 value, u64 alignment)
{
    return value & ~(alignment - 1ull);
}

static u64 macho64_align_up(u64 value, u64 alignment)
{
    return (value + alignment - 1ull) & ~(alignment - 1ull);
}

static u32 macho64_mix_checksum(u32 checksum, u8 value)
{
    return ((checksum ^ (u32)value) * 16777619u) + 0x9E3779B9u;
}

static u32 macho64_vma_prot_from_initprot(u32 initprot, u32 maxprot)
{
    u32 prot = 0u;

    if ((initprot == 0u)
        || ((initprot & ~(MACHO64_PROT_READ | MACHO64_PROT_WRITE | MACHO64_PROT_EXECUTE)) != 0u)
        || ((maxprot & ~(MACHO64_PROT_READ | MACHO64_PROT_WRITE | MACHO64_PROT_EXECUTE)) != 0u)
        || ((initprot & ~maxprot) != 0u))
    {
        return 0u;
    }

    if ((initprot & MACHO64_PROT_READ) != 0u)
    {
        prot |= VMA64_PROT_READ;
    }
    if ((initprot & MACHO64_PROT_WRITE) != 0u)
    {
        prot |= VMA64_PROT_WRITE;
    }
    if ((initprot & MACHO64_PROT_EXECUTE) != 0u)
    {
        prot |= VMA64_PROT_EXECUTE;
    }

    return prot;
}

static u32 macho64_segment_name_matches(const u8 *name, const char *expected)
{
    u32 index;

    if ((name == 0) || (expected == 0))
    {
        return 0u;
    }

    for (index = 0u; index < MACHO64_SEGMENT_NAME_BYTES; ++index)
    {
        if (expected[index] == 0)
        {
            return (name[index] == 0u) ? 1u : 0u;
        }
        if (name[index] != (u8)expected[index])
        {
            return 0u;
        }
    }

    return (expected[MACHO64_SEGMENT_NAME_BYTES] == 0) ? 1u : 0u;
}

static void macho64_copy_to_user(
    u64 destination,
    const u8 *source,
    u64 byte_count,
    u32 *source_checksum,
    u32 *mapped_checksum)
{
    volatile u8 *target = (volatile u8 *)(u64)destination;
    u64 index;
    u32 source_sum = (source_checksum != 0) ? *source_checksum : 0u;
    u32 mapped_sum = (mapped_checksum != 0) ? *mapped_checksum : 0u;

    for (index = 0ull; index < byte_count; ++index)
    {
        u8 value = source[index];
        target[index] = value;
        source_sum = macho64_mix_checksum(source_sum, value);
        mapped_sum = macho64_mix_checksum(mapped_sum, target[index]);
    }

    if (source_checksum != 0)
    {
        *source_checksum = source_sum;
    }
    if (mapped_checksum != 0)
    {
        *mapped_checksum = mapped_sum;
    }
}

static u32 macho64_zero_bss(u64 destination, u64 byte_count)
{
    volatile u8 *target = (volatile u8 *)(u64)destination;
    u64 index;
    u32 nonzero_count = 0u;

    for (index = 0ull; index < byte_count; ++index)
    {
        target[index] = 0u;
        if (target[index] != 0u)
        {
            ++nonzero_count;
        }
    }

    return nonzero_count;
}

static void macho64_clear_fat_slice(macho64_fat_slice_t *slice)
{
    if (slice == 0)
    {
        return;
    }

    slice->magic = 0u;
    slice->arch_count = 0u;
    slice->selected_index = 0xFFFFFFFFu;
    slice->cpu_type = 0u;
    slice->cpu_subtype = 0u;
    slice->offset = 0u;
    slice->size = 0u;
    slice->align = 0u;
    slice->error = MACHO64_ERROR_NONE;
}

static void macho64_set_fat_error(macho64_fat_slice_t *slice, u32 error)
{
    if (slice != 0)
    {
        slice->error = error;
    }
}

static void macho64_clear_segment_map_result(macho64_segment_map_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->segment_count = 0u;
    result->mapped_count = 0u;
    result->text_mapped = 0u;
    result->data_mapped = 0u;
    result->linkedit_mapped = 0u;
    result->text_prot = 0u;
    result->data_prot = 0u;
    result->linkedit_prot = 0u;
    result->total_map_bytes = 0ull;
    result->total_file_bytes = 0ull;
    result->total_bss_bytes = 0ull;
    result->first_mapped_vaddr = 0ull;
    result->max_mapped_end = 0ull;
    result->source_checksum = 0u;
    result->mapped_checksum = 0u;
    result->bss_nonzero_count = 0u;
    result->error = MACHO64_ERROR_NONE;
}

static void macho64_set_segment_map_error(macho64_segment_map_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void macho64_clear_main_result(macho64_main_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->main_count = 0u;
    result->text_found = 0u;
    result->stack_defaulted = 0u;
    result->stack_mapped = 0u;
    result->entry_within_text = 0u;
    result->entry_page_present = 0u;
    result->entry_page_prot = 0u;
    result->stack_page_present = 0u;
    result->stack_page_prot = 0u;
    result->error = MACHO64_ERROR_NONE;
    result->entryoff = 0ull;
    result->stack_size = 0ull;
    result->stack_mapped_bytes = 0ull;
    result->text_vmaddr = 0ull;
    result->text_vmsize = 0ull;
    result->entry_rip = 0ull;
    result->stack_base = 0ull;
    result->stack_top = 0ull;
    result->initial_rsp = 0ull;
}

static void macho64_set_main_error(macho64_main_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void macho64_clear_dylib_dependency(macho64_dylib_dependency_t *dependency)
{
    u32 index;

    if (dependency == 0)
    {
        return;
    }

    dependency->present = 0u;
    dependency->weak = 0u;
    dependency->shim_found = 0u;
    dependency->shim_id = MACHO64_SHIM_NONE;
    dependency->name_offset = 0u;
    dependency->path_length = 0u;
    dependency->path_checksum = 0u;
    dependency->timestamp = 0u;
    dependency->current_version = 0u;
    dependency->compatibility_version = 0u;
    for (index = 0u; index < MACHO64_MAX_DYLIB_NAME_BYTES; ++index)
    {
        dependency->path[index] = 0;
    }
}

static void macho64_clear_dylib_result(macho64_dylib_result_t *result)
{
    u32 index;

    if (result == 0)
    {
        return;
    }

    result->load_command_count = 0u;
    result->weak_command_count = 0u;
    result->recorded_count = 0u;
    result->shim_found_count = 0u;
    result->weak_absent_count = 0u;
    result->required_missing_count = 0u;
    result->first_shim_id = MACHO64_SHIM_NONE;
    result->first_path_checksum = 0u;
    result->error = MACHO64_ERROR_NONE;
    for (index = 0u; index < MACHO64_MAX_DYLIBS; ++index)
    {
        macho64_clear_dylib_dependency(&result->dependencies[index]);
    }
}

static void macho64_set_dylib_error(macho64_dylib_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static u32 macho64_path_matches(const char *path, u32 path_length, const char *expected)
{
    u32 index;

    if ((path == 0) || (expected == 0))
    {
        return 0u;
    }

    for (index = 0u; index < path_length; ++index)
    {
        if ((expected[index] == 0) || (path[index] != expected[index]))
        {
            return 0u;
        }
    }

    return (expected[path_length] == 0) ? 1u : 0u;
}

static u32 macho64_lookup_dylib_shim(const char *path, u32 path_length)
{
    if (macho64_path_matches(path, path_length, "/usr/lib/libSystem.B.dylib") != 0u)
    {
        return MACHO64_SHIM_LIBSYSTEM;
    }
    if (macho64_path_matches(path, path_length, "/usr/lib/system/libdyld.dylib") != 0u)
    {
        return MACHO64_SHIM_LIBDYLD;
    }
    if (macho64_path_matches(
            path,
            path_length,
            "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation") != 0u)
    {
        return MACHO64_SHIM_COREFOUNDATION;
    }

    return MACHO64_SHIM_NONE;
}

static u32 macho64_copy_dylib_name(
    const u8 *command,
    u32 command_size,
    u32 name_offset,
    macho64_dylib_dependency_t *dependency)
{
    u32 index;
    u32 path_length = 0u;
    u32 checksum = 0u;

    if ((command == 0) || (dependency == 0))
    {
        return MACHO64_ERROR_NULL;
    }
    if ((command_size < MACHO64_DYLIB_COMMAND_BYTES)
        || (name_offset < MACHO64_DYLIB_COMMAND_BYTES)
        || (name_offset >= command_size))
    {
        return MACHO64_ERROR_DYLIB_NAME;
    }

    for (index = name_offset; index < command_size; ++index)
    {
        if (command[index] == 0u)
        {
            break;
        }
        if (path_length >= (MACHO64_MAX_DYLIB_NAME_BYTES - 1u))
        {
            return MACHO64_ERROR_DYLIB_NAME;
        }

        dependency->path[path_length] = (char)command[index];
        checksum = macho64_mix_checksum(checksum, command[index]);
        ++path_length;
    }

    if ((index >= command_size) || (path_length == 0u))
    {
        return MACHO64_ERROR_DYLIB_NAME;
    }

    dependency->path[path_length] = 0;
    dependency->path_length = path_length;
    dependency->path_checksum = checksum;
    return MACHO64_ERROR_NONE;
}

static void macho64_clear_dyld_info_result(macho64_dyld_info_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->dyld_info_found = 0u;
    result->exports_trie_found = 0u;
    result->rebase_count = 0u;
    result->bind_count = 0u;
    result->rebase_type = 0u;
    result->bind_type = 0u;
    result->bind_ordinal = 0u;
    result->bind_shim_id = MACHO64_SHIM_NONE;
    result->bind_symbol_length = 0u;
    result->bind_symbol_checksum = 0u;
    result->error = MACHO64_ERROR_NONE;
    result->rebase_target = 0ull;
    result->rebase_before = 0ull;
    result->rebase_after = 0ull;
    result->bind_target = 0ull;
    result->bind_value = 0ull;
    result->rebase_off = 0u;
    result->rebase_size = 0u;
    result->bind_off = 0u;
    result->bind_size = 0u;
    result->exports_trie_off = 0u;
    result->exports_trie_size = 0u;
}

static void macho64_set_dyld_error(macho64_dyld_info_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void macho64_clear_tls_result(macho64_tls_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->section_count = 0u;
    result->variables_count = 0u;
    result->regular_count = 0u;
    result->zerofill_count = 0u;
    result->error = MACHO64_ERROR_NONE;
    result->variables_addr = 0ull;
    result->variables_bytes = 0ull;
    result->regular_addr = 0ull;
    result->regular_bytes = 0ull;
    result->zerofill_addr = 0ull;
    result->zerofill_bytes = 0ull;
    result->tls_block_base = 0ull;
    result->tls_block_bytes = 0ull;
    result->tls_template_base = 0ull;
    result->tls_template_bytes = 0ull;
    result->gs_base_before = 0ull;
    result->gs_base_after = 0ull;
    result->gs_zero_value = 0ull;
    result->template_checksum = 0u;
    result->block_checksum = 0u;
    result->zero_nonzero_count = 0u;
    result->first_template_word = 0u;
    result->page_present = 0u;
    result->page_protection = 0u;
    result->context_stored = 0u;
}

static void macho64_set_tls_error(macho64_tls_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void macho64_clear_stack_result(macho64_stack_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->error = MACHO64_ERROR_NONE;
    result->argc = 0u;
    result->envc = 0u;
    result->apple_count = 0u;
    result->aux_entry_count = 0u;
    result->pointer_slot_count = 0u;
    result->string_bytes = 0u;
    result->layout_bytes = 0u;
    result->alignment_ok = 0u;
    result->argv_null_ok = 0u;
    result->envp_null_ok = 0u;
    result->apple_null_ok = 0u;
    result->aux_null_ok = 0u;
    result->stack_page_present = 0u;
    result->stack_page_protection = 0u;
    result->exec_path_checksum = 0u;
    result->persona_string_checksum = 0u;
    result->stack_checksum = 0u;
    result->stack_base = 0ull;
    result->stack_top = 0ull;
    result->initial_rsp = 0ull;
    result->argc_address = 0ull;
    result->argv_address = 0ull;
    result->envp_address = 0ull;
    result->apple_address = 0ull;
    result->auxv_address = 0ull;
    result->strings_base = 0ull;
    result->argv0_address = 0ull;
    result->env0_address = 0ull;
    result->apple_exec_path_address = 0ull;
    result->apple_persona_address = 0ull;
    result->first_aux_type = 0ull;
    result->first_aux_value = 0ull;
}

static void macho64_set_stack_error(macho64_stack_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static u32 macho64_strlen_bounded(const char *text, u32 max_bytes)
{
    u32 index;

    if (text == 0)
    {
        return 0u;
    }

    for (index = 0u; index < max_bytes; ++index)
    {
        if (text[index] == 0)
        {
            return index + 1u;
        }
    }

    return 0u;
}

static void macho64_stack_write_bytes(u64 address, const u8 *data, u32 byte_count)
{
    volatile u8 *target = (volatile u8 *)(u64)address;
    u32 index;

    if ((address == 0ull) || (data == 0))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = data[index];
    }
}

static u32 macho64_checksum_user_bytes(u64 address, u32 byte_count)
{
    volatile const u8 *source = (volatile const u8 *)(u64)address;
    u32 index;
    u32 checksum = 0u;

    if (address == 0ull)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = macho64_mix_checksum(checksum, source[index]);
    }

    return checksum;
}

static u32 macho64_auxv_entry_count(
    const macho64_stack_aux_entry_t *auxv,
    u32 *out_count)
{
    u32 index;

    if ((auxv == 0) || (out_count == 0))
    {
        return 0u;
    }

    for (index = 0u; index < MACHO64_STACK_MAX_AUX_ENTRIES; ++index)
    {
        if (auxv[index].type == MACHO64_STACK_AUX_NULL)
        {
            *out_count = index + 1u;
            return (auxv[index].value == 0ull) ? 1u : 0u;
        }
    }

    return 0u;
}

static void macho64_write_user_le64(u64 address, u64 value)
{
    volatile u8 *target = (volatile u8 *)(u64)address;
    u32 index;

    for (index = 0u; index < 8u; ++index)
    {
        target[index] = (u8)(value >> (index * 8u));
    }
}

static u32 macho64_read_user_le32(u64 address)
{
    volatile const u8 *source = (volatile const u8 *)(u64)address;

    return ((u32)source[0])
        | ((u32)source[1] << 8)
        | ((u32)source[2] << 16)
        | ((u32)source[3] << 24);
}

static u32 macho64_user_range_ready(u64 address, u64 byte_count, u32 required_prot)
{
    u64 cursor;
    u64 end;

    if ((address == 0ull)
        || (byte_count == 0ull)
        || ((address + byte_count) < address))
    {
        return 0u;
    }

    cursor = macho64_align_down(address, VMA64_PAGE_BYTES);
    end = macho64_align_down(address + byte_count - 1ull, VMA64_PAGE_BYTES);
    while (cursor <= end)
    {
        u32 protection;

        if (paging64_user_page_present(cursor) == 0u)
        {
            return 0u;
        }
        protection = paging64_user_page_protection(cursor);
        if ((protection & required_prot) != required_prot)
        {
            return 0u;
        }
        if ((cursor + (u64)VMA64_PAGE_BYTES) <= cursor)
        {
            return 0u;
        }
        cursor += (u64)VMA64_PAGE_BYTES;
    }

    return 1u;
}

static u32 macho64_read_uleb128(const u8 *data, u32 end, u32 *cursor, u64 *out_value)
{
    u64 value = 0ull;
    u32 shift = 0u;

    if ((data == 0) || (cursor == 0) || (out_value == 0))
    {
        return 0u;
    }

    while (*cursor < end)
    {
        u8 byte = data[*cursor];
        ++(*cursor);

        if (shift >= 64u)
        {
            return 0u;
        }
        value |= ((u64)(byte & 0x7Fu)) << shift;
        if ((byte & 0x80u) == 0u)
        {
            *out_value = value;
            return 1u;
        }
        shift += 7u;
    }

    return 0u;
}

static u32 macho64_copy_bind_symbol(
    const u8 *data,
    u32 end,
    u32 *cursor,
    char *symbol,
    u32 *symbol_length,
    u32 *symbol_checksum)
{
    u32 length = 0u;
    u32 checksum = 0u;

    if ((data == 0) || (cursor == 0) || (symbol == 0)
        || (symbol_length == 0) || (symbol_checksum == 0))
    {
        return 0u;
    }

    while (*cursor < end)
    {
        u8 value = data[*cursor];
        ++(*cursor);
        if (value == 0u)
        {
            symbol[length] = 0;
            *symbol_length = length;
            *symbol_checksum = checksum;
            return (length != 0u) ? 1u : 0u;
        }
        if (length >= (MACHO64_MAX_BIND_SYMBOL_BYTES - 1u))
        {
            return 0u;
        }

        symbol[length] = (char)value;
        checksum = macho64_mix_checksum(checksum, value);
        ++length;
    }

    return 0u;
}

static u32 macho64_pointer_slot_ready(u64 address)
{
    u64 page_base;
    u64 page_end;
    u32 protection;

    if ((address & 7ull) != 0ull)
    {
        return 0u;
    }
    page_base = macho64_align_down(address, VMA64_PAGE_BYTES);
    page_end = macho64_align_down(address + 7ull, VMA64_PAGE_BYTES);
    if ((paging64_user_page_present(page_base) == 0u)
        || (paging64_user_page_present(page_end) == 0u))
    {
        return 0u;
    }

    protection = paging64_user_page_protection(page_base);
    return ((protection & VMA64_PROT_WRITE) != 0u) ? 1u : 0u;
}

static u64 macho64_read_user_u64(u64 address)
{
    volatile const u64 *slot = (volatile const u64 *)(u64)address;
    return *slot;
}

static void macho64_write_user_u64(u64 address, u64 value)
{
    volatile u64 *slot = (volatile u64 *)(u64)address;
    *slot = value;
}

static u64 macho64_resolve_shim_symbol(
    const macho64_dylib_result_t *dylibs,
    u32 ordinal,
    const char *symbol,
    u32 symbol_length,
    u32 *out_shim_id)
{
    u32 shim_id;

    if (out_shim_id != 0)
    {
        *out_shim_id = MACHO64_SHIM_NONE;
    }
    if ((dylibs == 0) || (symbol == 0) || (ordinal == 0u)
        || (ordinal > dylibs->recorded_count))
    {
        return 0ull;
    }

    shim_id = dylibs->dependencies[ordinal - 1u].shim_id;
    if (out_shim_id != 0)
    {
        *out_shim_id = shim_id;
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

static void macho64_unmap_recorded_segments(
    u32 pid,
    const u64 *mapped_bases,
    const u64 *mapped_lengths,
    u32 mapped_count)
{
    u32 index;

    if ((mapped_bases == 0) || (mapped_lengths == 0))
    {
        return;
    }

    for (index = 0u; index < mapped_count; ++index)
    {
        if ((mapped_bases[index] != 0ull) && (mapped_lengths[index] != 0ull))
        {
            (void)vma64_unmap(pid, mapped_bases[index], mapped_lengths[index]);
        }
    }
}

u32 macho64_parse_header(const u8 *data, u32 size, macho64_header_t *out_header)
{
    u32 minimum_command_bytes;

    macho64_clear_header(out_header);

    if ((data == 0) || (out_header == 0))
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_NULL);
        return MACHO64_DENIED;
    }
    if (size < MACHO64_HEADER_BYTES)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_SHORT_HEADER);
        return MACHO64_DENIED;
    }

    out_header->magic = macho64_read_le32(data);
    out_header->cpu_type = macho64_read_le32(data + 4u);
    out_header->cpu_subtype = macho64_read_le32(data + 8u);
    out_header->filetype = macho64_read_le32(data + 12u);
    out_header->ncmds = macho64_read_le32(data + 16u);
    out_header->sizeofcmds = macho64_read_le32(data + 20u);
    out_header->flags = macho64_read_le32(data + 24u);
    out_header->reserved = macho64_read_le32(data + 28u);
    out_header->load_command_offset = MACHO64_HEADER_BYTES;
    out_header->load_command_end = MACHO64_HEADER_BYTES + out_header->sizeofcmds;

    if (out_header->magic != MACHO64_MAGIC_LE64)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_MAGIC);
        return MACHO64_DENIED;
    }
    if (out_header->cpu_type != MACHO64_CPU_TYPE_X86_64)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_CPU_TYPE);
        return MACHO64_DENIED;
    }
    if (out_header->cpu_subtype != MACHO64_CPU_SUBTYPE_X86_64_ALL)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_CPU_SUBTYPE);
        return MACHO64_DENIED;
    }
    if ((out_header->filetype != MACHO64_FILETYPE_EXECUTE)
        && (out_header->filetype != MACHO64_FILETYPE_DYLIB))
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_FILETYPE);
        return MACHO64_DENIED;
    }
    if ((out_header->ncmds == 0u) || (out_header->ncmds > MACHO64_MAX_LOAD_COMMANDS))
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_LOAD_COMMAND_COUNT);
        return MACHO64_DENIED;
    }
    if (out_header->sizeofcmds == 0u)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_LOAD_COMMAND_SIZE);
        return MACHO64_DENIED;
    }
    if ((out_header->sizeofcmds & 7u) != 0u)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_LOAD_COMMAND_ALIGN);
        return MACHO64_DENIED;
    }

    minimum_command_bytes = out_header->ncmds * MACHO64_LOAD_COMMAND_MIN_BYTES;
    if (minimum_command_bytes > out_header->sizeofcmds)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_LOAD_COMMAND_SIZE);
        return MACHO64_DENIED;
    }
    if (macho64_range_available(
            size,
            MACHO64_HEADER_BYTES,
            out_header->sizeofcmds) == 0u)
    {
        macho64_set_header_error(out_header, MACHO64_ERROR_LOAD_COMMAND_RANGE);
        return MACHO64_DENIED;
    }

    out_header->error = MACHO64_ERROR_NONE;
    return MACHO64_OK;
}

u32 macho64_slice_fat_x86_64(const u8 *data, u32 size, macho64_fat_slice_t *out_slice)
{
    u32 arch_index;
    u32 arch_offset;
    u32 arch_table_bytes;
    u32 arch_table_end;
    u32 cpu_type;
    u32 cpu_subtype;
    u32 slice_offset;
    u32 slice_size;
    u32 slice_align;

    macho64_clear_fat_slice(out_slice);

    if ((data == 0) || (out_slice == 0))
    {
        macho64_set_fat_error(out_slice, MACHO64_ERROR_NULL);
        return MACHO64_DENIED;
    }
    if (size < MACHO64_FAT_HEADER_BYTES)
    {
        macho64_set_fat_error(out_slice, MACHO64_ERROR_FAT_SHORT_HEADER);
        return MACHO64_DENIED;
    }

    out_slice->magic = macho64_read_be32(data);
    out_slice->arch_count = macho64_read_be32(data + 4u);

    if (out_slice->magic != MACHO64_FAT_MAGIC)
    {
        macho64_set_fat_error(out_slice, MACHO64_ERROR_FAT_MAGIC);
        return MACHO64_DENIED;
    }
    if ((out_slice->arch_count == 0u) || (out_slice->arch_count > MACHO64_MAX_FAT_ARCHES))
    {
        macho64_set_fat_error(out_slice, MACHO64_ERROR_FAT_ARCH_COUNT);
        return MACHO64_DENIED;
    }

    arch_table_bytes = out_slice->arch_count * MACHO64_FAT_ARCH_BYTES;
    if (macho64_range_available(size, MACHO64_FAT_HEADER_BYTES, arch_table_bytes) == 0u)
    {
        macho64_set_fat_error(out_slice, MACHO64_ERROR_FAT_ARCH_TABLE);
        return MACHO64_DENIED;
    }
    arch_table_end = MACHO64_FAT_HEADER_BYTES + arch_table_bytes;

    for (arch_index = 0u; arch_index < out_slice->arch_count; ++arch_index)
    {
        arch_offset = MACHO64_FAT_HEADER_BYTES + (arch_index * MACHO64_FAT_ARCH_BYTES);
        cpu_type = macho64_read_be32(data + arch_offset);
        cpu_subtype = macho64_read_be32(data + arch_offset + 4u);
        slice_offset = macho64_read_be32(data + arch_offset + 8u);
        slice_size = macho64_read_be32(data + arch_offset + 12u);
        slice_align = macho64_read_be32(data + arch_offset + 16u);

        if (cpu_type != MACHO64_CPU_TYPE_X86_64)
        {
            continue;
        }

        out_slice->selected_index = arch_index;
        out_slice->cpu_type = cpu_type;
        out_slice->cpu_subtype = cpu_subtype;
        out_slice->offset = slice_offset;
        out_slice->size = slice_size;
        out_slice->align = slice_align;

        if ((slice_offset < arch_table_end)
            || (slice_size < MACHO64_HEADER_BYTES)
            || (macho64_range_available(size, slice_offset, slice_size) == 0u))
        {
            macho64_set_fat_error(out_slice, MACHO64_ERROR_FAT_SLICE_RANGE);
            return MACHO64_DENIED;
        }

        out_slice->error = MACHO64_ERROR_NONE;
        return MACHO64_OK;
    }

    macho64_set_fat_error(out_slice, MACHO64_ERROR_FAT_NO_X86_64);
    return MACHO64_DENIED;
}

u32 macho64_map_segments(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    u64 base_offset,
    macho64_segment_map_result_t *out_result)
{
    u64 mapped_bases[MACHO64_MAX_SEGMENTS];
    u64 mapped_lengths[MACHO64_MAX_SEGMENTS];
    u64 command_offset;
    u64 command_limit;
    u32 command_index;
    u32 mapped_count = 0u;
    u32 index;

    macho64_clear_segment_map_result(out_result);

    if ((binary_data == 0) || (header == 0))
    {
        macho64_set_segment_map_error(out_result, MACHO64_ERROR_NULL);
        return MACHO64_DENIED;
    }
    if (header->error != MACHO64_ERROR_NONE)
    {
        macho64_set_segment_map_error(out_result, header->error);
        return MACHO64_DENIED;
    }
    if ((header->ncmds == 0u)
        || (header->ncmds > MACHO64_MAX_LOAD_COMMANDS)
        || (header->load_command_offset < MACHO64_HEADER_BYTES))
    {
        macho64_set_segment_map_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
        return MACHO64_DENIED;
    }

    command_offset = (u64)header->load_command_offset;
    command_limit = (u64)header->load_command_end;
    if ((command_limit < command_offset)
        || (macho64_range64_available(binary_size, command_offset, command_limit - command_offset) == 0u))
    {
        macho64_set_segment_map_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }

    for (index = 0u; index < MACHO64_MAX_SEGMENTS; ++index)
    {
        mapped_bases[index] = 0ull;
        mapped_lengths[index] = 0ull;
    }

    for (command_index = 0u; command_index < header->ncmds; ++command_index)
    {
        u32 cmd;
        u32 cmdsize;
        u64 command_end;

        if ((command_offset > command_limit)
            || ((command_limit - command_offset) < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || (macho64_range64_available(binary_size, command_offset, MACHO64_LOAD_COMMAND_MIN_BYTES) == 0u))
        {
            macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            macho64_set_segment_map_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
            return MACHO64_DENIED;
        }

        cmd = macho64_read_le32(binary_data + command_offset);
        cmdsize = macho64_read_le32(binary_data + command_offset + 4u);
        command_end = command_offset + (u64)cmdsize;
        if ((cmdsize < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || ((cmdsize & 7u) != 0u)
            || (command_end < command_offset)
            || (command_end > command_limit))
        {
            macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            macho64_set_segment_map_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
            return MACHO64_DENIED;
        }

        if (cmd == MACHO64_LC_SEGMENT_64)
        {
            const u8 *record = binary_data + command_offset;
            const u8 *segname = record + 8u;
            u64 vmaddr;
            u64 vmsize;
            u64 fileoff;
            u64 filesize;
            u64 load_vaddr;
            u64 load_end;
            u64 map_base;
            u64 map_offset;
            u64 map_bytes;
            u32 maxprot;
            u32 initprot;
            u32 nsects;
            u32 final_prot;
            u32 map_result;

            if (cmdsize < MACHO64_LC_SEGMENT_64_BYTES)
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_SIZE);
                return MACHO64_DENIED;
            }

            vmaddr = macho64_read_le64(record + 24u);
            vmsize = macho64_read_le64(record + 32u);
            fileoff = macho64_read_le64(record + 40u);
            filesize = macho64_read_le64(record + 48u);
            maxprot = macho64_read_le32(record + 56u);
            initprot = macho64_read_le32(record + 60u);
            nsects = macho64_read_le32(record + 64u);

            if (out_result != 0)
            {
                ++out_result->segment_count;
            }
            if ((out_result != 0) && (out_result->segment_count > MACHO64_MAX_SEGMENTS))
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_COUNT);
                return MACHO64_DENIED;
            }
            if ((nsects > ((cmdsize - MACHO64_LC_SEGMENT_64_BYTES) / MACHO64_SECTION_64_BYTES))
                || (filesize > vmsize))
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_SIZE);
                return MACHO64_DENIED;
            }
            if ((filesize != 0ull)
                && (macho64_range64_available(binary_size, fileoff, filesize) == 0u))
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_RANGE);
                return MACHO64_DENIED;
            }
            if (vmsize == 0ull)
            {
                command_offset = command_end;
                continue;
            }
            if (mapped_count >= MACHO64_MAX_SEGMENTS)
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_COUNT);
                return MACHO64_DENIED;
            }

            load_vaddr = vmaddr + base_offset;
            if (load_vaddr < vmaddr)
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_ADDRESS);
                return MACHO64_DENIED;
            }

            load_end = load_vaddr + vmsize;
            if (load_end < load_vaddr)
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_ADDRESS);
                return MACHO64_DENIED;
            }

            map_base = macho64_align_down(load_vaddr, VMA64_PAGE_BYTES);
            map_offset = load_vaddr - map_base;
            map_bytes = macho64_align_up(map_offset + vmsize, VMA64_PAGE_BYTES);
            if ((map_bytes == 0ull) || ((map_base + map_bytes) < map_base))
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_ADDRESS);
                return MACHO64_DENIED;
            }

            final_prot = macho64_vma_prot_from_initprot(initprot, maxprot);
            if (final_prot == 0u)
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_PROTECT);
                return MACHO64_DENIED;
            }

            map_result = (vma64_map_anon(
                    pid,
                    map_base,
                    map_bytes,
                    VMA64_PROT_READ | VMA64_PROT_WRITE,
                    VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS) == map_base)
                ? 1u
                : 0u;
            if (map_result == 0u)
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_MAP);
                return MACHO64_DENIED;
            }

            mapped_bases[mapped_count] = map_base;
            mapped_lengths[mapped_count] = map_bytes;
            ++mapped_count;

            if (filesize != 0ull)
            {
                macho64_copy_to_user(
                    load_vaddr,
                    binary_data + fileoff,
                    filesize,
                    (out_result != 0) ? &out_result->source_checksum : 0,
                    (out_result != 0) ? &out_result->mapped_checksum : 0);
            }
            if (out_result != 0)
            {
                out_result->bss_nonzero_count += macho64_zero_bss(
                    load_vaddr + filesize,
                    vmsize - filesize);
            }
            else
            {
                (void)macho64_zero_bss(load_vaddr + filesize, vmsize - filesize);
            }

            if (vma64_protect(pid, map_base, map_bytes, final_prot) == 0u)
            {
                macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
                macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_PROTECT);
                return MACHO64_DENIED;
            }

            if (out_result != 0)
            {
                if (out_result->mapped_count == 0u)
                {
                    out_result->first_mapped_vaddr = map_base;
                }
                ++out_result->mapped_count;
                out_result->total_map_bytes += map_bytes;
                out_result->total_file_bytes += filesize;
                out_result->total_bss_bytes += vmsize - filesize;
                if ((map_base + map_bytes) > out_result->max_mapped_end)
                {
                    out_result->max_mapped_end = map_base + map_bytes;
                }
                if (macho64_segment_name_matches(segname, "__TEXT") != 0u)
                {
                    out_result->text_mapped = 1u;
                    out_result->text_prot = final_prot;
                }
                else if (macho64_segment_name_matches(segname, "__DATA") != 0u)
                {
                    out_result->data_mapped = 1u;
                    out_result->data_prot = final_prot;
                }
                else if (macho64_segment_name_matches(segname, "__LINKEDIT") != 0u)
                {
                    out_result->linkedit_mapped = 1u;
                    out_result->linkedit_prot = final_prot;
                }
            }
        }

        command_offset = command_end;
    }

    if (command_offset != command_limit)
    {
        macho64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
        macho64_set_segment_map_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }
    if ((out_result != 0) && (out_result->mapped_count == 0u))
    {
        macho64_set_segment_map_error(out_result, MACHO64_ERROR_SEGMENT_COUNT);
        return MACHO64_DENIED;
    }

    macho64_set_segment_map_error(out_result, MACHO64_ERROR_NONE);
    return MACHO64_OK;
}

u32 macho64_prepare_main_entry(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    u64 base_offset,
    u64 stack_top,
    macho64_main_result_t *out_result)
{
    u64 command_offset;
    u64 command_limit;
    u32 command_index;
    u64 requested_stack_size = 0ull;
    u64 entry_page;
    u64 stack_bytes;
    u64 stack_commit_base;

    macho64_clear_main_result(out_result);

    if ((binary_data == 0) || (header == 0) || (out_result == 0))
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_NULL);
        return MACHO64_DENIED;
    }
    if (header->error != MACHO64_ERROR_NONE)
    {
        macho64_set_main_error(out_result, header->error);
        return MACHO64_DENIED;
    }
    if ((header->ncmds == 0u)
        || (header->ncmds > MACHO64_MAX_LOAD_COMMANDS)
        || (header->load_command_offset < MACHO64_HEADER_BYTES))
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
        return MACHO64_DENIED;
    }

    command_offset = (u64)header->load_command_offset;
    command_limit = (u64)header->load_command_end;
    if ((command_limit < command_offset)
        || (macho64_range64_available(binary_size, command_offset, command_limit - command_offset) == 0u))
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }

    for (command_index = 0u; command_index < header->ncmds; ++command_index)
    {
        u32 cmd;
        u32 cmdsize;
        u64 command_end;

        if ((command_offset > command_limit)
            || ((command_limit - command_offset) < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || (macho64_range64_available(binary_size, command_offset, MACHO64_LOAD_COMMAND_MIN_BYTES) == 0u))
        {
            macho64_set_main_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
            return MACHO64_DENIED;
        }

        cmd = macho64_read_le32(binary_data + command_offset);
        cmdsize = macho64_read_le32(binary_data + command_offset + 4u);
        command_end = command_offset + (u64)cmdsize;
        if ((cmdsize < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || ((cmdsize & 7u) != 0u)
            || (command_end < command_offset)
            || (command_end > command_limit))
        {
            macho64_set_main_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
            return MACHO64_DENIED;
        }

        if (cmd == MACHO64_LC_SEGMENT_64)
        {
            const u8 *record = binary_data + command_offset;
            const u8 *segname = record + 8u;
            u64 vmaddr;
            u64 vmsize;
            u64 text_vmaddr;

            if (cmdsize < MACHO64_LC_SEGMENT_64_BYTES)
            {
                macho64_set_main_error(out_result, MACHO64_ERROR_SEGMENT_SIZE);
                return MACHO64_DENIED;
            }
            if (macho64_segment_name_matches(segname, "__TEXT") != 0u)
            {
                vmaddr = macho64_read_le64(record + 24u);
                vmsize = macho64_read_le64(record + 32u);
                text_vmaddr = vmaddr + base_offset;
                if ((vmsize == 0ull)
                    || (text_vmaddr < vmaddr)
                    || ((text_vmaddr + vmsize) <= text_vmaddr))
                {
                    macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_TEXT);
                    return MACHO64_DENIED;
                }

                out_result->text_found = 1u;
                out_result->text_vmaddr = text_vmaddr;
                out_result->text_vmsize = vmsize;
            }
        }
        else if (cmd == MACHO64_LC_MAIN)
        {
            if (cmdsize < MACHO64_LC_MAIN_BYTES)
            {
                macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_SIZE);
                return MACHO64_DENIED;
            }
            ++out_result->main_count;
            if (out_result->main_count > 1u)
            {
                macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_COUNT);
                return MACHO64_DENIED;
            }

            out_result->entryoff = macho64_read_le64(binary_data + command_offset + 8u);
            requested_stack_size = macho64_read_le64(binary_data + command_offset + 16u);
        }

        command_offset = command_end;
    }

    if (command_offset != command_limit)
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }
    if (out_result->main_count == 0u)
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_MISSING);
        return MACHO64_DENIED;
    }
    if (out_result->text_found == 0u)
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_TEXT);
        return MACHO64_DENIED;
    }
    if (out_result->entryoff >= out_result->text_vmsize)
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_RANGE);
        return MACHO64_DENIED;
    }

    out_result->entry_rip = out_result->text_vmaddr + out_result->entryoff;
    if ((out_result->entry_rip < out_result->text_vmaddr)
        || (out_result->entry_rip >= (out_result->text_vmaddr + out_result->text_vmsize)))
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_RANGE);
        return MACHO64_DENIED;
    }
    out_result->entry_within_text = 1u;
    entry_page = macho64_align_down(out_result->entry_rip, VMA64_PAGE_BYTES);
    out_result->entry_page_present = paging64_user_page_present(entry_page);
    out_result->entry_page_prot = paging64_user_page_protection(entry_page);

    stack_bytes = requested_stack_size;
    if (stack_bytes == 0ull)
    {
        stack_bytes = MACHO64_DEFAULT_STACK_BYTES;
        out_result->stack_defaulted = 1u;
    }
    stack_bytes = macho64_align_up(stack_bytes, VMA64_PAGE_BYTES);
    if ((stack_bytes == 0ull)
        || (stack_bytes < MACHO64_STACK_COMMIT_BYTES)
        || ((stack_top & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (stack_top <= stack_bytes)
        || (stack_top > 0x0000800000000000ull))
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_STACK);
        return MACHO64_DENIED;
    }

    out_result->stack_size = stack_bytes;
    out_result->stack_base = stack_top - stack_bytes;
    out_result->stack_top = stack_top;
    out_result->initial_rsp = stack_top & ~0xFull;
    out_result->stack_mapped_bytes = MACHO64_STACK_COMMIT_BYTES;
    stack_commit_base = stack_top - MACHO64_STACK_COMMIT_BYTES;

    if (vma64_map_anon(
            pid,
            stack_commit_base,
            MACHO64_STACK_COMMIT_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS) != stack_commit_base)
    {
        macho64_set_main_error(out_result, MACHO64_ERROR_MAIN_STACK);
        return MACHO64_DENIED;
    }

    out_result->stack_mapped = 1u;
    out_result->stack_page_present = paging64_user_page_present(stack_commit_base);
    out_result->stack_page_prot = paging64_user_page_protection(stack_commit_base);
    out_result->error = MACHO64_ERROR_NONE;
    return MACHO64_OK;
}

u32 macho64_walk_dylib_dependencies(
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    macho64_dylib_result_t *out_result)
{
    u64 command_offset;
    u64 command_limit;
    u32 command_index;

    macho64_clear_dylib_result(out_result);

    if ((binary_data == 0) || (header == 0) || (out_result == 0))
    {
        macho64_set_dylib_error(out_result, MACHO64_ERROR_NULL);
        return MACHO64_DENIED;
    }
    if (header->error != MACHO64_ERROR_NONE)
    {
        macho64_set_dylib_error(out_result, header->error);
        return MACHO64_DENIED;
    }
    if ((header->ncmds == 0u)
        || (header->ncmds > MACHO64_MAX_LOAD_COMMANDS)
        || (header->load_command_offset < MACHO64_HEADER_BYTES))
    {
        macho64_set_dylib_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
        return MACHO64_DENIED;
    }

    command_offset = (u64)header->load_command_offset;
    command_limit = (u64)header->load_command_end;
    if ((command_limit < command_offset)
        || (macho64_range64_available(binary_size, command_offset, command_limit - command_offset) == 0u))
    {
        macho64_set_dylib_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }

    for (command_index = 0u; command_index < header->ncmds; ++command_index)
    {
        u32 cmd;
        u32 cmdsize;
        u64 command_end;
        u32 weak;

        if ((command_offset > command_limit)
            || ((command_limit - command_offset) < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || (macho64_range64_available(binary_size, command_offset, MACHO64_LOAD_COMMAND_MIN_BYTES) == 0u))
        {
            macho64_set_dylib_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
            return MACHO64_DENIED;
        }

        cmd = macho64_read_le32(binary_data + command_offset);
        cmdsize = macho64_read_le32(binary_data + command_offset + 4u);
        command_end = command_offset + (u64)cmdsize;
        if ((cmdsize < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || ((cmdsize & 7u) != 0u)
            || (command_end < command_offset)
            || (command_end > command_limit))
        {
            macho64_set_dylib_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
            return MACHO64_DENIED;
        }

        if ((cmd == MACHO64_LC_LOAD_DYLIB) || (cmd == MACHO64_LC_LOAD_WEAK_DYLIB))
        {
            macho64_dylib_dependency_t dependency;
            u32 name_error;
            u32 shim_id;

            if (cmdsize < MACHO64_DYLIB_COMMAND_BYTES)
            {
                macho64_set_dylib_error(out_result, MACHO64_ERROR_DYLIB_SIZE);
                return MACHO64_DENIED;
            }

            weak = (cmd == MACHO64_LC_LOAD_WEAK_DYLIB) ? 1u : 0u;
            ++out_result->load_command_count;
            if (weak != 0u)
            {
                ++out_result->weak_command_count;
            }

            macho64_clear_dylib_dependency(&dependency);
            dependency.present = 1u;
            dependency.weak = weak;
            dependency.name_offset = macho64_read_le32(binary_data + command_offset + 8u);
            dependency.timestamp = macho64_read_le32(binary_data + command_offset + 12u);
            dependency.current_version = macho64_read_le32(binary_data + command_offset + 16u);
            dependency.compatibility_version = macho64_read_le32(binary_data + command_offset + 20u);

            name_error = macho64_copy_dylib_name(
                binary_data + command_offset,
                cmdsize,
                dependency.name_offset,
                &dependency);
            if (name_error != MACHO64_ERROR_NONE)
            {
                macho64_set_dylib_error(out_result, name_error);
                return MACHO64_DENIED;
            }

            shim_id = macho64_lookup_dylib_shim(dependency.path, dependency.path_length);
            if (shim_id == MACHO64_SHIM_NONE)
            {
                if (weak != 0u)
                {
                    ++out_result->weak_absent_count;
                    command_offset = command_end;
                    continue;
                }

                ++out_result->required_missing_count;
                macho64_set_dylib_error(out_result, MACHO64_ERROR_DYLIB_REQUIRED);
                return MACHO64_DENIED;
            }

            if (out_result->recorded_count >= MACHO64_MAX_DYLIBS)
            {
                macho64_set_dylib_error(out_result, MACHO64_ERROR_DYLIB_COUNT);
                return MACHO64_DENIED;
            }

            dependency.shim_found = 1u;
            dependency.shim_id = shim_id;
            out_result->dependencies[out_result->recorded_count] = dependency;
            ++out_result->recorded_count;
            ++out_result->shim_found_count;
            if (out_result->first_shim_id == MACHO64_SHIM_NONE)
            {
                out_result->first_shim_id = shim_id;
                out_result->first_path_checksum = dependency.path_checksum;
            }
        }

        command_offset = command_end;
    }

    if (command_offset != command_limit)
    {
        macho64_set_dylib_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }

    macho64_set_dylib_error(out_result, MACHO64_ERROR_NONE);
    return MACHO64_OK;
}

u32 macho64_apply_dyld_fixups(
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    const macho64_dylib_result_t *dylibs,
    u64 slide,
    macho64_dyld_info_result_t *out_result)
{
    macho64_segment_runtime_t segments[MACHO64_MAX_SEGMENTS];
    u64 command_offset;
    u64 command_limit;
    u32 command_index;
    u32 segment_count = 0u;
    u32 index;

    macho64_clear_dyld_info_result(out_result);

    if ((binary_data == 0) || (header == 0) || (dylibs == 0) || (out_result == 0))
    {
        macho64_set_dyld_error(out_result, MACHO64_ERROR_NULL);
        return MACHO64_DENIED;
    }
    if (header->error != MACHO64_ERROR_NONE)
    {
        macho64_set_dyld_error(out_result, header->error);
        return MACHO64_DENIED;
    }
    if ((header->ncmds == 0u)
        || (header->ncmds > MACHO64_MAX_LOAD_COMMANDS)
        || (header->load_command_offset < MACHO64_HEADER_BYTES))
    {
        macho64_set_dyld_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
        return MACHO64_DENIED;
    }

    for (index = 0u; index < MACHO64_MAX_SEGMENTS; ++index)
    {
        segments[index].present = 0u;
        segments[index].vmaddr = 0ull;
        segments[index].vmsize = 0ull;
    }

    command_offset = (u64)header->load_command_offset;
    command_limit = (u64)header->load_command_end;
    if ((command_limit < command_offset)
        || (macho64_range64_available(binary_size, command_offset, command_limit - command_offset) == 0u))
    {
        macho64_set_dyld_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }

    for (command_index = 0u; command_index < header->ncmds; ++command_index)
    {
        const u8 *record;
        u32 cmd;
        u32 cmdsize;
        u64 command_end;

        if ((command_offset > command_limit)
            || ((command_limit - command_offset) < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || (macho64_range64_available(binary_size, command_offset, MACHO64_LOAD_COMMAND_MIN_BYTES) == 0u))
        {
            macho64_set_dyld_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
            return MACHO64_DENIED;
        }

        record = binary_data + command_offset;
        cmd = macho64_read_le32(record);
        cmdsize = macho64_read_le32(record + 4u);
        command_end = command_offset + (u64)cmdsize;
        if ((cmdsize < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || ((cmdsize & 7u) != 0u)
            || (command_end < command_offset)
            || (command_end > command_limit))
        {
            macho64_set_dyld_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
            return MACHO64_DENIED;
        }

        if (cmd == MACHO64_LC_SEGMENT_64)
        {
            if (cmdsize < MACHO64_LC_SEGMENT_64_BYTES)
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_SEGMENT_SIZE);
                return MACHO64_DENIED;
            }
            if (segment_count >= MACHO64_MAX_SEGMENTS)
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_SEGMENT_COUNT);
                return MACHO64_DENIED;
            }
            segments[segment_count].present = 1u;
            segments[segment_count].vmaddr = macho64_read_le64(record + 24u);
            segments[segment_count].vmsize = macho64_read_le64(record + 32u);
            ++segment_count;
        }
        else if (cmd == MACHO64_LC_DYLD_INFO_ONLY)
        {
            if (cmdsize < MACHO64_DYLD_INFO_COMMAND_BYTES)
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_INFO_SIZE);
                return MACHO64_DENIED;
            }
            if (out_result->dyld_info_found != 0u)
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_INFO_SIZE);
                return MACHO64_DENIED;
            }

            out_result->dyld_info_found = 1u;
            out_result->rebase_off = macho64_read_le32(record + 8u);
            out_result->rebase_size = macho64_read_le32(record + 12u);
            out_result->bind_off = macho64_read_le32(record + 16u);
            out_result->bind_size = macho64_read_le32(record + 20u);
            if (((out_result->rebase_size != 0u)
                    && (macho64_range_available(
                        binary_size,
                        out_result->rebase_off,
                        out_result->rebase_size) == 0u))
                || ((out_result->bind_size != 0u)
                    && (macho64_range_available(
                        binary_size,
                        out_result->bind_off,
                        out_result->bind_size) == 0u)))
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_INFO_RANGE);
                return MACHO64_DENIED;
            }
        }
        else if (cmd == MACHO64_LC_DYLD_EXPORTS_TRIE)
        {
            if (cmdsize < MACHO64_LINKEDIT_DATA_COMMAND_BYTES)
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_INFO_SIZE);
                return MACHO64_DENIED;
            }
            out_result->exports_trie_found = 1u;
            out_result->exports_trie_off = macho64_read_le32(record + 8u);
            out_result->exports_trie_size = macho64_read_le32(record + 12u);
            if ((out_result->exports_trie_size != 0u)
                && (macho64_range_available(
                    binary_size,
                    out_result->exports_trie_off,
                    out_result->exports_trie_size) == 0u))
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_INFO_RANGE);
                return MACHO64_DENIED;
            }
        }

        command_offset = command_end;
    }

    if (command_offset != command_limit)
    {
        macho64_set_dyld_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }
    if (out_result->dyld_info_found == 0u)
    {
        macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_INFO_MISSING);
        return MACHO64_DENIED;
    }

    if (out_result->rebase_size != 0u)
    {
        u32 cursor = out_result->rebase_off;
        u32 end = out_result->rebase_off + out_result->rebase_size;
        u32 rebase_type = 0u;
        u32 segment_index = 0xFFFFFFFFu;
        u64 segment_offset = 0ull;

        while (cursor < end)
        {
            u8 byte = binary_data[cursor];
            u32 opcode = ((u32)byte) & MACHO64_REBASE_OPCODE_MASK;
            u32 immediate = ((u32)byte) & MACHO64_REBASE_IMMEDIATE_MASK;
            ++cursor;

            if (opcode == MACHO64_REBASE_OPCODE_DONE)
            {
                break;
            }
            if (opcode == MACHO64_REBASE_OPCODE_SET_TYPE_IMM)
            {
                rebase_type = immediate;
                out_result->rebase_type = rebase_type;
            }
            else if (opcode == MACHO64_REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB)
            {
                u64 parsed_offset;
                segment_index = immediate;
                if ((segment_index >= segment_count)
                    || (segments[segment_index].present == 0u)
                    || (macho64_read_uleb128(binary_data, end, &cursor, &parsed_offset) == 0u))
                {
                    macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_SEGMENT);
                    return MACHO64_DENIED;
                }
                segment_offset = parsed_offset;
            }
            else if (opcode == MACHO64_REBASE_OPCODE_ADD_ADDR_ULEB)
            {
                u64 addend;
                if (macho64_read_uleb128(binary_data, end, &cursor, &addend) == 0u)
                {
                    macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_REBASE_OPCODE);
                    return MACHO64_DENIED;
                }
                segment_offset += addend;
            }
            else if (opcode == MACHO64_REBASE_OPCODE_DO_REBASE_IMM_TIMES)
            {
                u32 count;
                for (count = 0u; count < immediate; ++count)
                {
                    u64 target;
                    u64 before;
                    u64 after;

                    if ((rebase_type != MACHO64_REBASE_TYPE_POINTER)
                        || (segment_index >= segment_count)
                        || ((segment_offset + 8ull) > segments[segment_index].vmsize))
                    {
                        macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_REBASE_OPCODE);
                        return MACHO64_DENIED;
                    }

                    target = segments[segment_index].vmaddr + segment_offset;
                    if ((target < segments[segment_index].vmaddr)
                        || (macho64_pointer_slot_ready(target) == 0u))
                    {
                        macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_POINTER);
                        return MACHO64_DENIED;
                    }

                    before = macho64_read_user_u64(target);
                    after = before + slide;
                    if (after < before)
                    {
                        macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_POINTER);
                        return MACHO64_DENIED;
                    }
                    macho64_write_user_u64(target, after);
                    out_result->rebase_target = target;
                    out_result->rebase_before = before;
                    out_result->rebase_after = after;
                    ++out_result->rebase_count;
                    segment_offset += 8ull;
                }
            }
            else
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_REBASE_OPCODE);
                return MACHO64_DENIED;
            }
        }
    }

    if (out_result->bind_size != 0u)
    {
        u32 cursor = out_result->bind_off;
        u32 end = out_result->bind_off + out_result->bind_size;
        u32 bind_type = 0u;
        u32 ordinal = 0u;
        u32 segment_index = 0xFFFFFFFFu;
        u64 segment_offset = 0ull;
        char symbol[MACHO64_MAX_BIND_SYMBOL_BYTES];
        u32 symbol_length = 0u;
        u32 symbol_checksum = 0u;

        for (index = 0u; index < MACHO64_MAX_BIND_SYMBOL_BYTES; ++index)
        {
            symbol[index] = 0;
        }

        while (cursor < end)
        {
            u8 byte = binary_data[cursor];
            u32 opcode = ((u32)byte) & MACHO64_BIND_OPCODE_MASK;
            u32 immediate = ((u32)byte) & MACHO64_BIND_IMMEDIATE_MASK;
            ++cursor;

            if (opcode == MACHO64_BIND_OPCODE_DONE)
            {
                break;
            }
            if (opcode == MACHO64_BIND_OPCODE_SET_DYLIB_ORDINAL_IMM)
            {
                ordinal = immediate;
                out_result->bind_ordinal = ordinal;
            }
            else if (opcode == MACHO64_BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM)
            {
                (void)immediate;
                if (macho64_copy_bind_symbol(
                        binary_data,
                        end,
                        &cursor,
                        symbol,
                        &symbol_length,
                        &symbol_checksum) == 0u)
                {
                    macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_SYMBOL);
                    return MACHO64_DENIED;
                }
                out_result->bind_symbol_length = symbol_length;
                out_result->bind_symbol_checksum = symbol_checksum;
            }
            else if (opcode == MACHO64_BIND_OPCODE_SET_TYPE_IMM)
            {
                bind_type = immediate;
                out_result->bind_type = bind_type;
            }
            else if (opcode == MACHO64_BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB)
            {
                u64 parsed_offset;
                segment_index = immediate;
                if ((segment_index >= segment_count)
                    || (segments[segment_index].present == 0u)
                    || (macho64_read_uleb128(binary_data, end, &cursor, &parsed_offset) == 0u))
                {
                    macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_SEGMENT);
                    return MACHO64_DENIED;
                }
                segment_offset = parsed_offset;
            }
            else if (opcode == MACHO64_BIND_OPCODE_DO_BIND)
            {
                u64 target;
                u64 value;
                u32 shim_id = MACHO64_SHIM_NONE;

                if ((bind_type != MACHO64_BIND_TYPE_POINTER)
                    || (ordinal == 0u)
                    || (symbol_length == 0u)
                    || (segment_index >= segment_count)
                    || ((segment_offset + 8ull) > segments[segment_index].vmsize))
                {
                    macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_BIND_OPCODE);
                    return MACHO64_DENIED;
                }

                target = segments[segment_index].vmaddr + segment_offset;
                if ((target < segments[segment_index].vmaddr)
                    || (macho64_pointer_slot_ready(target) == 0u))
                {
                    macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_POINTER);
                    return MACHO64_DENIED;
                }

                value = macho64_resolve_shim_symbol(
                    dylibs,
                    ordinal,
                    symbol,
                    symbol_length,
                    &shim_id);
                if (value == 0ull)
                {
                    macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_DYLIB);
                    return MACHO64_DENIED;
                }

                macho64_write_user_u64(target, value);
                out_result->bind_target = target;
                out_result->bind_value = value;
                out_result->bind_shim_id = shim_id;
                ++out_result->bind_count;
                segment_offset += 8ull;
            }
            else
            {
                macho64_set_dyld_error(out_result, MACHO64_ERROR_DYLD_BIND_OPCODE);
                return MACHO64_DENIED;
            }
        }
    }

    macho64_set_dyld_error(out_result, MACHO64_ERROR_NONE);
    return MACHO64_OK;
}

u32 macho64_setup_tls(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    u64 tls_block_base,
    macho64_tls_result_t *out_result)
{
    persona_context_t *context;
    u64 command_offset;
    u64 command_limit;
    u32 command_index;
    u64 template_bytes = 0ull;
    u64 zerofill_bytes = 0ull;
    u64 template_cursor = 0ull;
    u64 map_bytes;
    u32 found_tls = 0u;
    u32 index;

    macho64_clear_tls_result(out_result);

    if ((pid == PROCESS64_INVALID_PID)
        || (binary_data == 0)
        || (header == 0)
        || (out_result == 0))
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_NULL);
        return MACHO64_DENIED;
    }
    if (header->error != MACHO64_ERROR_NONE)
    {
        macho64_set_tls_error(out_result, header->error);
        return MACHO64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_MACOS_MACHO))
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_PERSONA);
        return MACHO64_DENIED;
    }

    if ((header->ncmds == 0u)
        || (header->ncmds > MACHO64_MAX_LOAD_COMMANDS)
        || (header->load_command_offset < MACHO64_HEADER_BYTES))
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
        return MACHO64_DENIED;
    }

    command_offset = (u64)header->load_command_offset;
    command_limit = (u64)header->load_command_end;
    if ((command_limit < command_offset)
        || (macho64_range64_available(binary_size, command_offset, command_limit - command_offset) == 0u))
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }

    for (command_index = 0u; command_index < header->ncmds; ++command_index)
    {
        const u8 *record;
        u32 cmd;
        u32 cmdsize;
        u64 command_end;

        if ((command_offset > command_limit)
            || ((command_limit - command_offset) < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || (macho64_range64_available(binary_size, command_offset, MACHO64_LOAD_COMMAND_MIN_BYTES) == 0u))
        {
            macho64_set_tls_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
            return MACHO64_DENIED;
        }

        record = binary_data + command_offset;
        cmd = macho64_read_le32(record);
        cmdsize = macho64_read_le32(record + 4u);
        command_end = command_offset + (u64)cmdsize;
        if ((cmdsize < MACHO64_LOAD_COMMAND_MIN_BYTES)
            || ((cmdsize & 7u) != 0u)
            || (command_end < command_offset)
            || (command_end > command_limit))
        {
            macho64_set_tls_error(out_result, MACHO64_ERROR_COMMAND_SIZE);
            return MACHO64_DENIED;
        }

        if (cmd == MACHO64_LC_SEGMENT_64)
        {
            u64 section_offset;
            u64 section_end;
            u32 section_index;
            u32 nsects;

            if (cmdsize < MACHO64_LC_SEGMENT_64_BYTES)
            {
                macho64_set_tls_error(out_result, MACHO64_ERROR_SEGMENT_SIZE);
                return MACHO64_DENIED;
            }
            nsects = macho64_read_le32(record + 64u);
            if ((nsects > MACHO64_MAX_SEGMENTS)
                || (cmdsize < (MACHO64_LC_SEGMENT_64_BYTES
                    + (nsects * MACHO64_SECTION_64_BYTES))))
            {
                macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_SECTION);
                return MACHO64_DENIED;
            }

            section_offset = command_offset + (u64)MACHO64_LC_SEGMENT_64_BYTES;
            section_end = section_offset + ((u64)nsects * (u64)MACHO64_SECTION_64_BYTES);
            if ((section_end < section_offset) || (section_end > command_end))
            {
                macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_SECTION);
                return MACHO64_DENIED;
            }

            for (section_index = 0u; section_index < nsects; ++section_index)
            {
                const u8 *section;
                u64 section_addr;
                u64 section_size;
                u32 section_fileoff;
                u32 section_flags;
                u32 section_type;

                section = binary_data + section_offset
                    + ((u64)section_index * (u64)MACHO64_SECTION_64_BYTES);
                section_addr = macho64_read_le64(section + 32u);
                section_size = macho64_read_le64(section + 40u);
                section_fileoff = macho64_read_le32(section + 48u);
                section_flags = macho64_read_le32(section + 64u);
                section_type = section_flags & MACHO64_SECTION_TYPE_MASK;

                if ((section_type != MACHO64_SECTION_THREAD_LOCAL_VARIABLES)
                    && (section_type != MACHO64_SECTION_THREAD_LOCAL_REGULAR)
                    && (section_type != MACHO64_SECTION_THREAD_LOCAL_ZEROFILL))
                {
                    continue;
                }

                ++out_result->section_count;
                found_tls = 1u;

                if ((section_size == 0ull)
                    || ((section_addr + section_size) < section_addr)
                    || (macho64_user_range_ready(section_addr, section_size, VMA64_PROT_READ) == 0u))
                {
                    macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_SECTION);
                    return MACHO64_DENIED;
                }

                if (section_type == MACHO64_SECTION_THREAD_LOCAL_VARIABLES)
                {
                    ++out_result->variables_count;
                    out_result->variables_addr = section_addr;
                    out_result->variables_bytes = section_size;
                    continue;
                }

                if (section_type == MACHO64_SECTION_THREAD_LOCAL_REGULAR)
                {
                    ++out_result->regular_count;
                    out_result->regular_addr = section_addr;
                    out_result->regular_bytes = section_size;
                    if ((section_size > (u64)MACHO64_TLS_MAX_BLOCK_BYTES)
                        || (section_fileoff > binary_size)
                        || (section_size > ((u64)binary_size - (u64)section_fileoff))
                        || ((template_bytes + section_size) < template_bytes)
                        || ((template_bytes + section_size)
                            > (u64)(MACHO64_TLS_MAX_BLOCK_BYTES - MACHO64_TLS_SELF_POINTER_BYTES)))
                    {
                        macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_SOURCE);
                        return MACHO64_DENIED;
                    }
                    template_bytes += section_size;
                }
                else
                {
                    ++out_result->zerofill_count;
                    out_result->zerofill_addr = section_addr;
                    out_result->zerofill_bytes += section_size;
                    zerofill_bytes += section_size;
                    if ((zerofill_bytes > (u64)MACHO64_TLS_MAX_BLOCK_BYTES)
                        || ((template_bytes + zerofill_bytes)
                            > (u64)(MACHO64_TLS_MAX_BLOCK_BYTES - MACHO64_TLS_SELF_POINTER_BYTES)))
                    {
                        macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_SIZE);
                        return MACHO64_DENIED;
                    }
                }
            }
        }

        command_offset = command_end;
    }

    if (command_offset != command_limit)
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_COMMAND_RANGE);
        return MACHO64_DENIED;
    }
    if (found_tls == 0u)
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_NONE);
        return MACHO64_OK;
    }
    if ((tls_block_base == 0ull)
        || ((tls_block_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (tls_block_base >= 0x0000800000000000ull))
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_ADDRESS);
        return MACHO64_DENIED;
    }

    map_bytes = macho64_align_up(
        (u64)MACHO64_TLS_SELF_POINTER_BYTES + template_bytes + zerofill_bytes,
        VMA64_PAGE_BYTES);
    if ((map_bytes == 0ull) || (map_bytes > (u64)MACHO64_TLS_MAX_BLOCK_BYTES))
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_SIZE);
        return MACHO64_DENIED;
    }
    if (vma64_map_anon(
            pid,
            tls_block_base,
            map_bytes,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != tls_block_base)
    {
        macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_MAP);
        return MACHO64_DENIED;
    }

    out_result->tls_block_base = tls_block_base;
    out_result->tls_block_bytes = map_bytes;
    out_result->tls_template_base = tls_block_base + (u64)MACHO64_TLS_SELF_POINTER_BYTES;
    out_result->tls_template_bytes = template_bytes;
    macho64_write_user_le64(tls_block_base, tls_block_base);

    command_offset = (u64)header->load_command_offset;
    for (command_index = 0u; command_index < header->ncmds; ++command_index)
    {
        const u8 *record = binary_data + command_offset;
        u32 cmd = macho64_read_le32(record);
        u32 cmdsize = macho64_read_le32(record + 4u);
        u64 command_end = command_offset + (u64)cmdsize;

        if (cmd == MACHO64_LC_SEGMENT_64)
        {
            u32 nsects = macho64_read_le32(record + 64u);
            u64 section_offset = command_offset + (u64)MACHO64_LC_SEGMENT_64_BYTES;
            u32 section_index;

            for (section_index = 0u; section_index < nsects; ++section_index)
            {
                const u8 *section = binary_data + section_offset
                    + ((u64)section_index * (u64)MACHO64_SECTION_64_BYTES);
                u64 section_size = macho64_read_le64(section + 40u);
                u32 section_fileoff = macho64_read_le32(section + 48u);
                u32 section_type = macho64_read_le32(section + 64u)
                    & MACHO64_SECTION_TYPE_MASK;

                if (section_type == MACHO64_SECTION_THREAD_LOCAL_REGULAR)
                {
                    volatile u8 *target = (volatile u8 *)(u64)(out_result->tls_template_base
                        + template_cursor);
                    const u8 *source = binary_data + section_fileoff;

                    for (index = 0u; index < (u32)section_size; ++index)
                    {
                        target[index] = source[index];
                        out_result->template_checksum =
                            macho64_mix_checksum(out_result->template_checksum, source[index]);
                    }
                    template_cursor += section_size;
                }
            }
        }

        command_offset = command_end;
    }

    for (index = 0u;
        index < (u32)((u64)MACHO64_TLS_SELF_POINTER_BYTES + template_bytes + zerofill_bytes);
        ++index)
    {
        u8 value = *((volatile const u8 *)(u64)(tls_block_base + (u64)index));
        out_result->block_checksum = macho64_mix_checksum(out_result->block_checksum, value);
        if ((index >= ((u32)MACHO64_TLS_SELF_POINTER_BYTES + (u32)template_bytes))
            && (value != 0u))
        {
            ++out_result->zero_nonzero_count;
        }
    }

    out_result->first_template_word = (template_bytes >= 4ull)
        ? macho64_read_user_le32(out_result->tls_template_base)
        : 0u;
    out_result->page_present = paging64_user_page_present(tls_block_base);
    out_result->page_protection = paging64_user_page_protection(tls_block_base);

    out_result->gs_base_before = read_gs_base64();
    write_gs_base64(tls_block_base);
    out_result->gs_base_after = read_gs_base64();
    out_result->gs_zero_value = macho64_read_user_u64(tls_block_base);
    if ((out_result->gs_base_after != tls_block_base)
        || (out_result->gs_zero_value != tls_block_base))
    {
        (void)vma64_unmap(pid, tls_block_base, map_bytes);
        macho64_set_tls_error(out_result, MACHO64_ERROR_TLS_MAP);
        return MACHO64_DENIED;
    }

    context->tls_base = tls_block_base;
    context->tls_size = map_bytes;
    out_result->context_stored = 1u;
    macho64_set_tls_error(out_result, MACHO64_ERROR_NONE);
    return MACHO64_OK;
}

u32 macho64_build_initial_stack(
    u32 pid,
    u64 stack_base,
    u64 stack_top,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    const char *exec_path,
    const macho64_stack_aux_entry_t *auxv,
    macho64_stack_result_t *out_result)
{
    static const char exec_prefix[] = "exec_path=";
    static const char persona_string[] = "limitless_persona=macos";
    u64 argv_ptrs[MACHO64_STACK_MAX_ARGC];
    u64 envp_ptrs[MACHO64_STACK_MAX_ENVC];
    u64 apple_ptrs[MACHO64_STACK_APPLE_COUNT];
    persona_context_t *context;
    u64 cursor;
    u64 pointer_bytes;
    u64 initial_rsp;
    u64 write_cursor;
    u32 aux_entry_count = 0u;
    u32 exec_path_len;
    u32 exec_prefix_len;
    u32 exec_record_bytes;
    u32 persona_record_bytes;
    u32 string_length;
    u32 stack_bytes;
    u32 index;

    if (out_result == 0)
    {
        return MACHO64_DENIED;
    }

    macho64_clear_stack_result(out_result);
    out_result->stack_base = stack_base;
    out_result->stack_top = stack_top;
    out_result->argc = argc;
    out_result->envc = envc;
    out_result->apple_count = MACHO64_STACK_APPLE_COUNT;

    if ((pid == PROCESS64_INVALID_PID)
        || (argc > MACHO64_STACK_MAX_ARGC)
        || (envc > MACHO64_STACK_MAX_ENVC)
        || ((argc != 0u) && (argv == 0))
        || ((envc != 0u) && (envp == 0))
        || (exec_path == 0)
        || (auxv == 0))
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_ARGUMENT);
        return MACHO64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_MACOS_MACHO))
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_PERSONA);
        return MACHO64_DENIED;
    }

    if (macho64_auxv_entry_count(auxv, &aux_entry_count) == 0u)
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_ARGUMENT);
        return MACHO64_DENIED;
    }
    out_result->aux_entry_count = aux_entry_count;

    if ((stack_base >= stack_top)
        || ((stack_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_top & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_top - stack_base) > (u64)MACHO64_STACK_MAX_BUILD_BYTES)
        || (macho64_user_range_ready(
            stack_base,
            stack_top - stack_base,
            VMA64_PROT_WRITE) == 0u))
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_RANGE);
        return MACHO64_DENIED;
    }

    for (index = 0u; index < argc; ++index)
    {
        if (macho64_strlen_bounded(argv[index], MACHO64_STACK_MAX_STRING_BYTES) == 0u)
        {
            macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_ARGUMENT);
            return MACHO64_DENIED;
        }
    }
    for (index = 0u; index < envc; ++index)
    {
        if (macho64_strlen_bounded(envp[index], MACHO64_STACK_MAX_STRING_BYTES) == 0u)
        {
            macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_ARGUMENT);
            return MACHO64_DENIED;
        }
    }

    exec_path_len = macho64_strlen_bounded(exec_path, MACHO64_STACK_MAX_STRING_BYTES);
    exec_prefix_len = (u32)sizeof(exec_prefix) - 1u;
    if ((exec_path_len == 0u)
        || (exec_path_len > (MACHO64_STACK_MAX_STRING_BYTES - exec_prefix_len)))
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_ARGUMENT);
        return MACHO64_DENIED;
    }
    exec_record_bytes = exec_prefix_len + exec_path_len;
    persona_record_bytes = (u32)sizeof(persona_string);

    stack_bytes = (u32)(stack_top - stack_base);
    for (index = 0u; index < stack_bytes; ++index)
    {
        ((volatile u8 *)(u64)stack_base)[index] = 0u;
    }

    cursor = stack_top;
    if ((cursor - stack_base) < (u64)persona_record_bytes)
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_OVERFLOW);
        return MACHO64_DENIED;
    }
    cursor -= (u64)persona_record_bytes;
    apple_ptrs[1] = cursor;
    out_result->apple_persona_address = cursor;
    out_result->string_bytes += persona_record_bytes;
    macho64_stack_write_bytes(cursor, (const u8 *)persona_string, persona_record_bytes);

    if ((cursor - stack_base) < (u64)exec_record_bytes)
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_OVERFLOW);
        return MACHO64_DENIED;
    }
    cursor -= (u64)exec_record_bytes;
    apple_ptrs[0] = cursor;
    out_result->apple_exec_path_address = cursor;
    out_result->string_bytes += exec_record_bytes;
    macho64_stack_write_bytes(cursor, (const u8 *)exec_prefix, exec_prefix_len);
    macho64_stack_write_bytes(cursor + (u64)exec_prefix_len, (const u8 *)exec_path, exec_path_len);

    index = envc;
    while (index > 0u)
    {
        --index;
        string_length = macho64_strlen_bounded(envp[index], MACHO64_STACK_MAX_STRING_BYTES);
        if ((string_length == 0u) || ((cursor - stack_base) < (u64)string_length))
        {
            macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_OVERFLOW);
            return MACHO64_DENIED;
        }

        cursor -= (u64)string_length;
        envp_ptrs[index] = cursor;
        out_result->string_bytes += string_length;
        macho64_stack_write_bytes(cursor, (const u8 *)envp[index], string_length);
    }

    index = argc;
    while (index > 0u)
    {
        --index;
        string_length = macho64_strlen_bounded(argv[index], MACHO64_STACK_MAX_STRING_BYTES);
        if ((string_length == 0u) || ((cursor - stack_base) < (u64)string_length))
        {
            macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_OVERFLOW);
            return MACHO64_DENIED;
        }

        cursor -= (u64)string_length;
        argv_ptrs[index] = cursor;
        out_result->string_bytes += string_length;
        macho64_stack_write_bytes(cursor, (const u8 *)argv[index], string_length);
    }

    out_result->strings_base = cursor;
    out_result->argv0_address = (argc != 0u) ? argv_ptrs[0] : 0ull;
    out_result->env0_address = (envc != 0u) ? envp_ptrs[0] : 0ull;
    out_result->exec_path_checksum = macho64_checksum_user_bytes(
        out_result->apple_exec_path_address,
        exec_record_bytes);
    out_result->persona_string_checksum = macho64_checksum_user_bytes(
        out_result->apple_persona_address,
        persona_record_bytes);

    out_result->pointer_slot_count =
        1u
        + argc + 1u
        + envc + 1u
        + MACHO64_STACK_APPLE_COUNT + 1u
        + (aux_entry_count * 2u);
    pointer_bytes = (u64)out_result->pointer_slot_count * sizeof(u64);
    if ((cursor < stack_base) || ((cursor - stack_base) < pointer_bytes))
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_OVERFLOW);
        return MACHO64_DENIED;
    }

    initial_rsp = (cursor - pointer_bytes) & ~0xFull;
    if ((initial_rsp < stack_base) || ((initial_rsp + pointer_bytes) > cursor))
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_OVERFLOW);
        return MACHO64_DENIED;
    }

    out_result->initial_rsp = initial_rsp;
    out_result->argc_address = initial_rsp;
    out_result->argv_address = initial_rsp + sizeof(u64);
    out_result->envp_address = out_result->argv_address + ((u64)(argc + 1u) * sizeof(u64));
    out_result->apple_address =
        out_result->envp_address + ((u64)(envc + 1u) * sizeof(u64));
    out_result->auxv_address =
        out_result->apple_address
        + ((u64)(MACHO64_STACK_APPLE_COUNT + 1u) * sizeof(u64));
    out_result->layout_bytes = (u32)(stack_top - initial_rsp);
    out_result->alignment_ok = ((initial_rsp & 0xFull) == 0ull) ? 1u : 0u;

    write_cursor = initial_rsp;
    macho64_write_user_u64(write_cursor, (u64)argc);
    write_cursor += sizeof(u64);
    for (index = 0u; index < argc; ++index)
    {
        macho64_write_user_u64(write_cursor, argv_ptrs[index]);
        write_cursor += sizeof(u64);
    }
    macho64_write_user_u64(write_cursor, 0ull);
    write_cursor += sizeof(u64);
    for (index = 0u; index < envc; ++index)
    {
        macho64_write_user_u64(write_cursor, envp_ptrs[index]);
        write_cursor += sizeof(u64);
    }
    macho64_write_user_u64(write_cursor, 0ull);
    write_cursor += sizeof(u64);
    for (index = 0u; index < MACHO64_STACK_APPLE_COUNT; ++index)
    {
        macho64_write_user_u64(write_cursor, apple_ptrs[index]);
        write_cursor += sizeof(u64);
    }
    macho64_write_user_u64(write_cursor, 0ull);
    write_cursor += sizeof(u64);
    for (index = 0u; index < aux_entry_count; ++index)
    {
        macho64_write_user_u64(write_cursor, auxv[index].type);
        write_cursor += sizeof(u64);
        macho64_write_user_u64(write_cursor, auxv[index].value);
        write_cursor += sizeof(u64);
    }

    if (write_cursor != (initial_rsp + pointer_bytes))
    {
        macho64_set_stack_error(out_result, MACHO64_ERROR_STACK_WRITE);
        return MACHO64_DENIED;
    }

    out_result->argv_null_ok =
        (macho64_read_user_u64(out_result->argv_address + ((u64)argc * sizeof(u64))) == 0ull)
        ? 1u
        : 0u;
    out_result->envp_null_ok =
        (macho64_read_user_u64(out_result->envp_address + ((u64)envc * sizeof(u64))) == 0ull)
        ? 1u
        : 0u;
    out_result->apple_null_ok =
        (macho64_read_user_u64(
            out_result->apple_address
            + ((u64)MACHO64_STACK_APPLE_COUNT * sizeof(u64))) == 0ull)
        ? 1u
        : 0u;
    out_result->aux_null_ok =
        ((macho64_read_user_u64(
                out_result->auxv_address + ((u64)(aux_entry_count - 1u) * 16ull))
                == MACHO64_STACK_AUX_NULL)
            && (macho64_read_user_u64(
                out_result->auxv_address + ((u64)(aux_entry_count - 1u) * 16ull) + 8ull)
                == 0ull))
        ? 1u
        : 0u;
    out_result->first_aux_type = macho64_read_user_u64(out_result->auxv_address);
    out_result->first_aux_value = macho64_read_user_u64(out_result->auxv_address + 8ull);
    out_result->stack_page_present = paging64_user_page_present(stack_base);
    out_result->stack_page_protection = paging64_user_page_protection(stack_base);
    out_result->stack_checksum =
        macho64_checksum_user_bytes(initial_rsp, out_result->layout_bytes);

    macho64_set_stack_error(out_result, MACHO64_ERROR_NONE);
    return MACHO64_OK;
}
