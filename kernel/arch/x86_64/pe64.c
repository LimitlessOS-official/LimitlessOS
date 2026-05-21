#include "pe64_x64.h"
#include "descriptors_x64.h"
#include "interrupts_x64.h"
#include "launch_x64.h"
#include "paging_x64.h"
#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"
#include "vma_x64.h"
#include "x64.h"

/*
 * J.1-J.12 add the first LimitlessOS-owned PE32+ metadata and section mapping
 * primitives. They integrate with pe64_x64.h, process_x64.h, and vma_x64.h:
 * PE headers and section tables are validated before IMAGE_SCN sections are
 * mapped as brokered anonymous VMAs, filled from the package bytes, zero-filled
 * for BSS, protected according to PE section characteristics, and adjusted by
 * PE base relocation blocks when ASLR places the image at a non-preferred base.
 * The import resolver then walks the Import Directory Table against an explicit
 * LimitlessOS shim registry and patches IAT slots without granting ambient DLL
 * or filesystem authority. The TLS handler reads the PE32+ TLS directory,
 * allocates a per-process TLS block, copies the template, writes the TLS index,
 * records the block in the persona context when available, dispatches each
 * initializer through an explicit loader callback hook, and registers the
 * PE exception directory table into the Windows persona context for later
 * unwinding. J.8 adds deterministic TEB page setup, stores the Windows persona
 * TEB/PEB/stack/TLS pointers, and writes IA32_GS_BASE so GS-relative reads use
 * that TEB without executing or emulating foreign code. J.9 fills a minimal
 * PEB and process-parameter block inside that same brokered TEB page with the
 * image base, version fields, and bounded UTF-16 process strings. J.10 maps
 * the fixed Windows KUSER_SHARED_DATA page, fills broker-owned time/system
 * fields, protects it read-only for ring-3 consumers, and refreshes its
 * time counters from the PIT tick path without executing or emulating foreign
 * code. J.11 reads the Load Config Directory security-cookie pointer and
 * initializes the pointed image slot with a nonzero kernel-mixed cookie while
 * preserving page protections. J.12 prepares the first PE entry transfer frame,
 * proves the Windows-persona DLL argument contract with a ring-3 probe, and
 * truthfully denies executable launch until the ntdll LdrInitializeThunk shim
 * exists in Phase K.
 * The scaffold checkpoints prove valid AMD64 PE metadata is accepted, sections
 * map with deterministic bytes and R/W/X page protections, BSS is actually
 * zero, DIR64 relocations update mapped pointer slots while restoring read-only
 * targets, imports resolve only when the requested DLL/symbol exists in the shim
 * registry, TLS callbacks are dispatched before the future entry handoff marker,
 * PE .pdata entries are readable from the persona context, the PEB image base
 * matches the mapped PE image base, KUSER time advances after a real timer
 * tick, and malformed metadata, short raw data, missing relocation sections,
 * missing imports, invalid TLS ranges, invalid exception tables, invalid
 * TEB/PEB/KUSER setup, malformed Load Config security-cookie metadata,
 * unavailable executable thunk launch, and capacity errors are denied with specific
 * structured codes.
 */

#define PE64_KUSER_REGISTERED_MAX PERSONA64_MAX_CONTEXTS

static u32 g_pe64_kuser_registered_pids[PE64_KUSER_REGISTERED_MAX];
static u32 g_pe64_kuser_update_sequence = 0u;
static u64 g_pe64_security_cookie_state = 0xD1B54A32D192ED03ull;
static u32 g_pe64_security_cookie_sequence = 0u;

static void pe64_clear_header(pe64_header_t *header)
{
    if (header == 0)
    {
        return;
    }

    header->pe_offset = 0u;
    header->machine = 0u;
    header->number_of_sections = 0u;
    header->time_date_stamp = 0u;
    header->pointer_to_symbol_table = 0u;
    header->number_of_symbols = 0u;
    header->size_of_optional_header = 0u;
    header->characteristics = 0u;
    header->optional_magic = 0u;
    header->address_of_entry_point = 0u;
    header->image_base = 0ull;
    header->section_alignment = 0u;
    header->file_alignment = 0u;
    header->size_of_image = 0u;
    header->size_of_headers = 0u;
    header->subsystem = 0u;
    header->dll_characteristics = 0u;
    header->size_of_stack_reserve = 0ull;
    header->size_of_stack_commit = 0ull;
    header->size_of_heap_reserve = 0ull;
    header->size_of_heap_commit = 0ull;
    header->number_of_rva_and_sizes = 0u;
    header->import_directory_rva = 0u;
    header->import_directory_size = 0u;
    header->exception_directory_rva = 0u;
    header->exception_directory_size = 0u;
    header->tls_directory_rva = 0u;
    header->tls_directory_size = 0u;
    header->load_config_directory_rva = 0u;
    header->load_config_directory_size = 0u;
    header->error = PE64_ERROR_NONE;
}

static void pe64_set_header_error(pe64_header_t *header, u32 error)
{
    if (header != 0)
    {
        header->error = error;
    }
}

static void pe64_clear_section(pe64_section_t *section)
{
    u32 index;

    if (section == 0)
    {
        return;
    }

    for (index = 0u; index < PE64_SECTION_NAME_BYTES; ++index)
    {
        section->name[index] = 0u;
    }
    section->virtual_size = 0u;
    section->virtual_address = 0u;
    section->size_of_raw_data = 0u;
    section->pointer_to_raw_data = 0u;
    section->pointer_to_relocations = 0u;
    section->pointer_to_linenumbers = 0u;
    section->number_of_relocations = 0u;
    section->number_of_linenumbers = 0u;
    section->characteristics = 0u;
    section->prot_flags = 0u;
}

static void pe64_clear_section_summary(pe64_section_summary_t *summary)
{
    if (summary == 0)
    {
        return;
    }

    summary->section_count = 0u;
    summary->readable_count = 0u;
    summary->writable_count = 0u;
    summary->executable_count = 0u;
    summary->code_count = 0u;
    summary->initialized_data_count = 0u;
    summary->uninitialized_data_count = 0u;
    summary->total_virtual_bytes = 0ull;
    summary->total_raw_bytes = 0ull;
    summary->first_virtual_address = 0u;
    summary->max_virtual_end = 0u;
    summary->name_checksum = 2166136261u;
    summary->error = PE64_ERROR_NONE;
}

static void pe64_set_section_summary_error(pe64_section_summary_t *summary, u32 error)
{
    if (summary != 0)
    {
        summary->error = error;
    }
}

static void pe64_clear_map_result(pe64_map_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->mapped_count = 0u;
    result->section_count = 0u;
    result->actual_base = 0ull;
    result->total_map_bytes = 0ull;
    result->total_file_bytes = 0ull;
    result->total_bss_bytes = 0ull;
    result->first_mapped_vaddr = 0ull;
    result->max_mapped_end = 0ull;
    result->source_checksum = 0u;
    result->mapped_checksum = 0u;
    result->bss_nonzero_count = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_map_error(pe64_map_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_reloc_result(pe64_reloc_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->block_count = 0u;
    result->entry_count = 0u;
    result->applied_count = 0u;
    result->skipped_count = 0u;
    result->preferred_base = 0ull;
    result->actual_base = 0ull;
    result->delta = 0ull;
    result->reloc_section_rva = 0u;
    result->reloc_section_bytes = 0u;
    result->first_fixup_rva = 0u;
    result->last_fixup_rva = 0u;
    result->before_checksum = 0u;
    result->after_checksum = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_reloc_error(pe64_reloc_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_import_result(pe64_import_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->descriptor_count = 0u;
    result->thunk_count = 0u;
    result->name_import_count = 0u;
    result->ordinal_import_count = 0u;
    result->resolved_count = 0u;
    result->import_directory_rva = 0u;
    result->import_directory_bytes = 0u;
    result->dll_name_rva = 0u;
    result->first_thunk_rva = 0u;
    result->last_thunk_rva = 0u;
    result->first_function = 0ull;
    result->last_function = 0ull;
    result->dll_checksum = 2166136261u;
    result->symbol_checksum = 2166136261u;
    result->iat_checksum = 2166136261u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_import_error(pe64_import_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_tls_result(pe64_tls_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->tls_directory_rva = 0u;
    result->tls_directory_bytes = 0u;
    result->raw_start_va = 0ull;
    result->raw_end_va = 0ull;
    result->index_va = 0ull;
    result->callbacks_va = 0ull;
    result->tls_block_base = 0ull;
    result->tls_block_bytes = 0ull;
    result->template_bytes = 0u;
    result->zero_fill_bytes = 0u;
    result->index_value = 0u;
    result->index_written = 0u;
    result->callback_count = 0u;
    result->invoked_count = 0u;
    result->first_callback = 0ull;
    result->last_callback = 0ull;
    result->template_checksum = 2166136261u;
    result->block_checksum = 2166136261u;
    result->callback_checksum = 2166136261u;
    result->zero_nonzero_count = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_tls_error(pe64_tls_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_exception_result(pe64_exception_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->exception_directory_rva = 0u;
    result->exception_directory_bytes = 0u;
    result->function_count = 0u;
    result->registered_count = 0u;
    result->table_base = 0ull;
    result->table_bytes = 0ull;
    result->first_begin_rva = 0u;
    result->last_end_rva = 0u;
    result->first_unwind_rva = 0u;
    result->last_unwind_rva = 0u;
    result->table_checksum = 2166136261u;
    result->persona_stored = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_exception_error(pe64_exception_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_teb_result(pe64_teb_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->teb_base = 0ull;
    result->peb_base = 0ull;
    result->stack_base = 0ull;
    result->stack_limit = 0ull;
    result->tls_pointer = 0ull;
    result->gs_base_before = 0ull;
    result->gs_base_after = 0ull;
    result->exception_list_value = 0ull;
    result->stack_base_value = 0ull;
    result->stack_limit_value = 0ull;
    result->self_value = 0ull;
    result->tls_pointer_value = 0ull;
    result->peb_value = 0ull;
    result->page_bytes = 0ull;
    result->page_protection = 0u;
    result->page_checksum = 2166136261u;
    result->context_stored = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_teb_error(pe64_teb_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_peb_result(pe64_peb_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->peb_base = 0ull;
    result->process_parameters = 0ull;
    result->image_base = 0ull;
    result->image_base_value = 0ull;
    result->process_parameters_value = 0ull;
    result->image_path_buffer = 0ull;
    result->command_line_buffer = 0ull;
    result->environment_buffer = 0ull;
    result->os_major = 0u;
    result->os_minor = 0u;
    result->os_build = 0u;
    result->nt_global_flag = 0u;
    result->os_major_value = 0u;
    result->os_minor_value = 0u;
    result->os_build_value = 0u;
    result->nt_global_flag_value = 0u;
    result->image_path_bytes = 0u;
    result->command_line_bytes = 0u;
    result->environment_bytes = 0u;
    result->image_path_checksum = 2166136261u;
    result->command_line_checksum = 2166136261u;
    result->environment_checksum = 2166136261u;
    result->context_stored = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_peb_error(pe64_peb_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_kuser_result(pe64_kuser_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->base = 0ull;
    result->page_bytes = 0ull;
    result->system_time_100ns = 0ull;
    result->tick_count = 0ull;
    result->system_time_low_value = 0u;
    result->tick_count_low_value = 0u;
    result->system_root_bytes = 0u;
    result->nt_product_type_value = 0u;
    result->processor_feature_checksum = 2166136261u;
    result->page_protection = 0u;
    result->page_checksum = 2166136261u;
    result->update_count = 0u;
    result->context_stored = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_kuser_error(pe64_kuser_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_security_cookie_result(pe64_security_cookie_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->load_config_directory_rva = 0u;
    result->load_config_directory_bytes = 0u;
    result->load_config_base = 0ull;
    result->security_cookie_field = 0ull;
    result->security_cookie_address = 0ull;
    result->cookie_before = 0ull;
    result->cookie_value = 0ull;
    result->cookie_after = 0ull;
    result->cookie_checksum = 2166136261u;
    result->page_protection = 0u;
    result->context_stored = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_security_cookie_error(
    pe64_security_cookie_result_t *result,
    u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void pe64_clear_entry_result(pe64_entry_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->entry_rip = 0ull;
    result->transfer_rip = 0ull;
    result->stack_base = 0ull;
    result->stack_top = 0ull;
    result->initial_rsp = 0ull;
    result->arg_rcx = 0ull;
    result->arg_rdx = 0ull;
    result->arg_r8 = 0ull;
    result->ldr_initialize_thunk = 0ull;
    result->transfer_selectors = 0u;
    result->transfer_rflags = 0u;
    result->entry_page_present = 0u;
    result->entry_page_prot = 0u;
    result->stack_page_present = 0u;
    result->stack_page_prot = 0u;
    result->dll_entry = 0u;
    result->transfer_ready = 0u;
    result->transfer_executed = 0u;
    result->transfer_result = 0u;
    result->transfer_aux = 0u;
    result->context_stored = 0u;
    result->error = PE64_ERROR_NONE;
}

static void pe64_set_entry_error(pe64_entry_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static u16 pe64_read_le16(const u8 *data)
{
    return (u16)(((u16)data[0]) | ((u16)data[1] << 8));
}

static u32 pe64_read_le32(const u8 *data)
{
    return ((u32)data[0])
        | ((u32)data[1] << 8)
        | ((u32)data[2] << 16)
        | ((u32)data[3] << 24);
}

static u64 pe64_read_le64(const u8 *data)
{
    return ((u64)pe64_read_le32(data))
        | ((u64)pe64_read_le32(data + 4u) << 32);
}

static u32 pe64_read_user_le32(u64 address)
{
    volatile u8 *data = (volatile u8 *)(u64)address;

    return ((u32)data[0])
        | ((u32)data[1] << 8)
        | ((u32)data[2] << 16)
        | ((u32)data[3] << 24);
}

static u64 pe64_read_user_le64(u64 address)
{
    return ((u64)pe64_read_user_le32(address))
        | ((u64)pe64_read_user_le32(address + 4ull) << 32);
}

static void pe64_write_user_le16(u64 address, u16 value)
{
    volatile u8 *data = (volatile u8 *)(u64)address;

    data[0] = (u8)value;
    data[1] = (u8)(value >> 8);
}

static void pe64_write_user_le32(u64 address, u32 value)
{
    volatile u8 *data = (volatile u8 *)(u64)address;
    u32 index;

    for (index = 0u; index < 4u; ++index)
    {
        data[index] = (u8)(value >> (index * 8u));
    }
}

static void pe64_write_user_le64(u64 address, u64 value)
{
    volatile u8 *data = (volatile u8 *)(u64)address;
    u32 index;

    for (index = 0u; index < 8u; ++index)
    {
        data[index] = (u8)(value >> (index * 8u));
    }
}

static u32 pe64_range_available(u32 size, u32 offset, u32 bytes)
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

static u32 pe64_section_prot_flags(u32 characteristics)
{
    u32 prot = 0u;

    if ((characteristics & PE64_SCN_MEM_READ) != 0u)
    {
        prot |= PE64_SECTION_PROT_READ;
    }
    if ((characteristics & PE64_SCN_MEM_WRITE) != 0u)
    {
        prot |= PE64_SECTION_PROT_WRITE;
    }
    if ((characteristics & PE64_SCN_MEM_EXECUTE) != 0u)
    {
        prot |= PE64_SECTION_PROT_EXECUTE;
    }

    return prot;
}

static u32 pe64_section_span(const pe64_section_t *section)
{
    if (section->virtual_size != 0u)
    {
        return section->virtual_size;
    }
    return section->size_of_raw_data;
}

static u32 pe64_checksum_step(u32 checksum, u32 value)
{
    checksum ^= value;
    checksum *= 16777619u;
    return checksum;
}

static u32 pe64_checksum_u64(u32 checksum, u64 value)
{
    checksum = pe64_checksum_step(checksum, (u32)value);
    checksum = pe64_checksum_step(checksum, (u32)(value >> 32));
    return checksum;
}

static u64 pe64_entropy_mix64(u64 state, u64 value)
{
    state ^= value + 0x9E3779B97F4A7C15ull + (state << 6) + (state >> 2);
    state ^= state >> 33;
    state *= 0xFF51AFD7ED558CCDull;
    state ^= state >> 33;
    state *= 0xC4CEB9FE1A85EC53ull;
    state ^= state >> 33;
    return state;
}

static u64 pe64_security_cookie_next(u32 pid, u64 actual_base, u64 cookie_address)
{
    u64 state = g_pe64_security_cookie_state;

    ++g_pe64_security_cookie_sequence;
    state = pe64_entropy_mix64(state, (u64)g_pe64_security_cookie_sequence);
    state = pe64_entropy_mix64(state, (u64)pid);
    state = pe64_entropy_mix64(state, (u64)process64_principal(pid));
    state = pe64_entropy_mix64(state, (u64)process64_manifest_token(pid));
    state = pe64_entropy_mix64(state, (u64)process64_runtime_token(pid));
    state = pe64_entropy_mix64(state, (u64)process64_runtime_image_token(pid));
    state = pe64_entropy_mix64(state, actual_base);
    state = pe64_entropy_mix64(state, cookie_address);
    state = pe64_entropy_mix64(state, (u64)pit_get_ticks());
    state = pe64_entropy_mix64(state, (u64)pit_get_frequency_hz());
    state = pe64_entropy_mix64(state, (u64)(void *)&g_pe64_security_cookie_state);

    if (state == 0ull)
    {
        state = 0xBB40E64E2F11A7B5ull ^ ((u64)pid << 32) ^ cookie_address;
    }
    if (state == 0ull)
    {
        state = 0x2B992DDFA23249D6ull;
    }

    g_pe64_security_cookie_state = pe64_entropy_mix64(state, 0xA5A55A5AC3C33C3Cull);
    return state;
}

static u64 pe64_align_down(u64 value, u64 alignment)
{
    return value & ~(alignment - 1ull);
}

static u64 pe64_align_up(u64 value, u64 alignment)
{
    u64 mask = alignment - 1ull;

    if ((value & mask) == 0ull)
    {
        return value;
    }
    if ((value + mask) < value)
    {
        return 0ull;
    }

    return (value + mask) & ~mask;
}

static u32 pe64_vma_prot_from_section(u32 prot_flags)
{
    u32 vma_prot = 0u;

    if ((prot_flags & PE64_SECTION_PROT_READ) != 0u)
    {
        vma_prot |= VMA64_PROT_READ;
    }
    if ((prot_flags & PE64_SECTION_PROT_WRITE) != 0u)
    {
        vma_prot |= VMA64_PROT_WRITE;
    }
    if ((prot_flags & PE64_SECTION_PROT_EXECUTE) != 0u)
    {
        vma_prot |= VMA64_PROT_EXECUTE;
    }

    return vma_prot;
}

static void pe64_unmap_recorded_sections(
    u32 pid,
    const u64 *mapped_bases,
    const u64 *mapped_lengths,
    u32 mapped_count)
{
    u32 index;

    for (index = 0u; index < mapped_count; ++index)
    {
        if ((mapped_bases[index] != 0ull) && (mapped_lengths[index] != 0ull))
        {
            (void)vma64_unmap(pid, mapped_bases[index], mapped_lengths[index]);
        }
    }
}

static u32 pe64_copy_to_user(
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
        source_sum = pe64_checksum_step(source_sum, (u32)value);
        mapped_sum = pe64_checksum_step(mapped_sum, (u32)target[index]);
    }

    if (source_checksum != 0)
    {
        *source_checksum = source_sum;
    }
    if (mapped_checksum != 0)
    {
        *mapped_checksum = mapped_sum;
    }

    return 1u;
}

static u32 pe64_zero_user(u64 destination, u64 byte_count)
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

static u32 pe64_checksum_user_bytes(u64 source, u64 byte_count)
{
    volatile u8 *bytes = (volatile u8 *)(u64)source;
    u64 index;
    u32 checksum = 2166136261u;

    for (index = 0ull; index < byte_count; ++index)
    {
        checksum = pe64_checksum_step(checksum, (u32)bytes[index]);
    }

    return (checksum != 0u) ? checksum : 1u;
}

static u32 pe64_ascii_length_bounded(const char *text, u32 max_chars, u32 *out_length)
{
    u32 index;

    if ((text == 0) || (out_length == 0))
    {
        return 0u;
    }

    for (index = 0u; index <= max_chars; ++index)
    {
        u8 value = (u8)text[index];

        if (value == 0u)
        {
            *out_length = index;
            return 1u;
        }
        if ((value < 0x20u) || (value > 0x7Eu))
        {
            return 0u;
        }
    }

    return 0u;
}

static u32 pe64_write_utf16le_string(
    u64 destination,
    const char *text,
    u32 max_bytes,
    u32 *out_bytes,
    u32 *out_checksum)
{
    volatile u8 *target = (volatile u8 *)(u64)destination;
    u32 max_chars;
    u32 char_count;
    u32 checksum = 2166136261u;
    u32 index;

    if ((max_bytes < 2u) || ((max_bytes & 1u) != 0u))
    {
        return 0u;
    }

    max_chars = (max_bytes / 2u) - 1u;
    if (pe64_ascii_length_bounded(text, max_chars, &char_count) == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < char_count; ++index)
    {
        u8 value = (u8)text[index];

        target[index * 2u] = value;
        target[(index * 2u) + 1u] = 0u;
        checksum = pe64_checksum_step(checksum, (u32)value);
        checksum = pe64_checksum_step(checksum, 0u);
    }

    target[char_count * 2u] = 0u;
    target[(char_count * 2u) + 1u] = 0u;

    if (out_bytes != 0)
    {
        *out_bytes = char_count * 2u;
    }
    if (out_checksum != 0)
    {
        *out_checksum = (checksum != 0u) ? checksum : 1u;
    }

    return 1u;
}

static u32 pe64_utf16le_string_fits(const char *text, u32 max_bytes)
{
    u32 max_chars;
    u32 char_count;

    if ((max_bytes < 2u) || ((max_bytes & 1u) != 0u))
    {
        return 0u;
    }

    max_chars = (max_bytes / 2u) - 1u;
    return pe64_ascii_length_bounded(text, max_chars, &char_count);
}

static void pe64_write_unicode_descriptor(u64 descriptor, u64 buffer, u32 byte_count)
{
    pe64_write_user_le16(descriptor, (u16)byte_count);
    pe64_write_user_le16(
        descriptor + 2ull,
        (u16)PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES);
    pe64_write_user_le32(descriptor + 4ull, 0u);
    pe64_write_user_le64(descriptor + 8ull, buffer);
}

static u64 pe64_kuser_system_time_100ns(void)
{
    u32 frequency = pit_get_frequency_hz();

    if (frequency == 0u)
    {
        frequency = 100u;
    }

    return (((u64)pit_get_ticks()) * 10000000ull) / (u64)frequency;
}

static void pe64_kuser_write_time_fields(u64 base, u64 system_time_100ns, u64 tick_count)
{
    u32 system_low = (u32)system_time_100ns;
    u32 system_high = (u32)(system_time_100ns >> 32);

    pe64_write_user_le32(
        base + (u64)PE64_KUSER_OFFSET_SYSTEM_TIME_HIGH1,
        system_high);
    pe64_write_user_le32(
        base + (u64)PE64_KUSER_OFFSET_SYSTEM_TIME_LOW,
        system_low);
    pe64_write_user_le32(
        base + (u64)PE64_KUSER_OFFSET_SYSTEM_TIME_HIGH2,
        system_high);
    pe64_write_user_le32(
        base + (u64)PE64_KUSER_OFFSET_TICK_COUNT_LOW,
        (u32)tick_count);
    pe64_write_user_le32(
        base + (u64)PE64_KUSER_OFFSET_TICK_COUNT_HIGH,
        (u32)(tick_count >> 32));
}

static void pe64_kuser_fill_result(
    pe64_kuser_result_t *result,
    u64 base,
    persona_context_t *context)
{
    u64 system_time = pe64_kuser_system_time_100ns();
    u64 tick_count = (u64)pit_get_ticks();

    if (result == 0)
    {
        return;
    }

    result->base = base;
    result->page_bytes = (u64)PE64_KUSER_SHARED_DATA_BYTES;
    result->system_time_100ns = system_time;
    result->tick_count = tick_count;
    result->system_time_low_value =
        pe64_read_user_le32(base + (u64)PE64_KUSER_OFFSET_SYSTEM_TIME_LOW);
    result->tick_count_low_value =
        pe64_read_user_le32(base + (u64)PE64_KUSER_OFFSET_TICK_COUNT_LOW);
    result->nt_product_type_value =
        pe64_read_user_le32(base + (u64)PE64_KUSER_OFFSET_NT_PRODUCT_TYPE);
    result->processor_feature_checksum = pe64_checksum_user_bytes(
        base + (u64)PE64_KUSER_OFFSET_PROCESSOR_FEATURES,
        PE64_KUSER_PROCESSOR_FEATURE_BYTES);
    result->page_protection = paging64_user_page_protection(base);
    result->page_checksum = pe64_checksum_user_bytes(
        base,
        PE64_KUSER_SHARED_DATA_BYTES);
    if (context != 0)
    {
        result->update_count = context->windows_kuser_shared_data_updates;
        result->context_stored =
            ((context->windows_kuser_shared_data_base == base)
                && (context->windows_kuser_shared_data_checksum
                    == result->page_checksum))
                ? 1u
                : 0u;
    }
    pe64_set_kuser_error(result, PE64_ERROR_NONE);
}

static u32 pe64_kuser_register_pid(u32 pid)
{
    u32 index;

    for (index = 0u; index < PE64_KUSER_REGISTERED_MAX; ++index)
    {
        if (g_pe64_kuser_registered_pids[index] == pid)
        {
            return 1u;
        }
    }

    for (index = 0u; index < PE64_KUSER_REGISTERED_MAX; ++index)
    {
        persona_context_t *context =
            persona64_context_for_process(g_pe64_kuser_registered_pids[index]);

        if ((g_pe64_kuser_registered_pids[index] == 0u)
            || (context == 0)
            || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
            || (context->windows_kuser_shared_data_base
                != PE64_KUSER_SHARED_DATA_BASE))
        {
            g_pe64_kuser_registered_pids[index] = pid;
            return 1u;
        }
    }

    return 0u;
}

static u32 pe64_name_is_reloc(const pe64_section_t *section)
{
    return ((section != 0)
        && (section->name[0] == (u8)'.')
        && (section->name[1] == (u8)'r')
        && (section->name[2] == (u8)'e')
        && (section->name[3] == (u8)'l')
        && (section->name[4] == (u8)'o')
        && (section->name[5] == (u8)'c'))
        ? 1u
        : 0u;
}

static const pe64_section_t *pe64_find_reloc_section(
    const pe64_section_t *sections,
    u32 section_count)
{
    u32 index;

    if ((sections == 0) || (section_count > PE64_MAX_SECTIONS))
    {
        return 0;
    }

    for (index = 0u; index < section_count; ++index)
    {
        if (pe64_name_is_reloc(&sections[index]) != 0u)
        {
            return &sections[index];
        }
    }

    return 0;
}

static u32 pe64_rva_in_sections(
    const pe64_section_t *sections,
    u32 section_count,
    u32 rva,
    u32 byte_count)
{
    u32 index;

    if ((sections == 0)
        || (section_count == 0u)
        || (section_count > PE64_MAX_SECTIONS)
        || (byte_count == 0u)
        || ((rva + byte_count) < rva))
    {
        return 0u;
    }

    for (index = 0u; index < section_count; ++index)
    {
        u32 span = pe64_section_span(&sections[index]);
        u32 start = sections[index].virtual_address;
        u32 end = start + span;

        if ((span == 0u) || (end < start))
        {
            continue;
        }
        if ((rva >= start) && ((rva + byte_count) <= end))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 pe64_apply_one_dir64(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u64 delta,
    u32 target_rva,
    pe64_reloc_result_t *out_result)
{
    u64 target_address;
    u64 target_page;
    u64 old_value;
    u64 new_value;
    u32 restore_required = 0u;
    u32 old_prot;
    vma_region_t *region;
    volatile u64 *slot;

    if (pe64_rva_in_sections(sections, section_count, target_rva, 8u) == 0u)
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_TARGET_RANGE);
        return PE64_DENIED;
    }

    target_address = actual_base + (u64)target_rva;
    if ((target_address < actual_base)
        || ((target_address & ((u64)VMA64_PAGE_BYTES - 1ull)) > ((u64)VMA64_PAGE_BYTES - 8ull)))
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_TARGET_RANGE);
        return PE64_DENIED;
    }

    region = vma64_find(pid, target_address);
    if ((region == 0)
        || ((target_address + 8ull) < target_address)
        || ((target_address + 8ull) > region->virt_end))
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_TARGET_MAP);
        return PE64_DENIED;
    }

    old_prot = region->prot_flags;
    target_page = pe64_align_down(target_address, VMA64_PAGE_BYTES);
    if ((old_prot & VMA64_PROT_WRITE) == 0u)
    {
        if (vma64_protect(
                pid,
                target_page,
                VMA64_PAGE_BYTES,
                old_prot | VMA64_PROT_WRITE) == 0u)
        {
            pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_TARGET_MAP);
            return PE64_DENIED;
        }
        restore_required = 1u;
    }

    slot = (volatile u64 *)(u64)target_address;
    old_value = *slot;
    new_value = old_value + delta;
    *slot = new_value;

    if (restore_required != 0u)
    {
        if (vma64_protect(pid, target_page, VMA64_PAGE_BYTES, old_prot) == 0u)
        {
            pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_TARGET_MAP);
            return PE64_DENIED;
        }
    }

    if (out_result != 0)
    {
        if (out_result->applied_count == 0u)
        {
            out_result->first_fixup_rva = target_rva;
        }
        out_result->last_fixup_rva = target_rva;
        out_result->before_checksum = pe64_checksum_u64(out_result->before_checksum, old_value);
        out_result->after_checksum = pe64_checksum_u64(out_result->after_checksum, new_value);
        ++out_result->applied_count;
    }

    return PE64_OK;
}

static u32 pe64_va_to_rva(u64 actual_base, u64 virtual_address, u32 *out_rva)
{
    u64 delta;

    if (out_rva != 0)
    {
        *out_rva = 0u;
    }
    if ((actual_base == 0ull)
        || (virtual_address < actual_base)
        || (out_rva == 0))
    {
        return 0u;
    }

    delta = virtual_address - actual_base;
    if (delta > 0xFFFFFFFFull)
    {
        return 0u;
    }

    *out_rva = (u32)delta;
    return 1u;
}

static u32 pe64_mapped_rva_range(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u32 rva,
    u32 byte_count,
    u32 required_prot)
{
    u64 cursor;
    u64 end;

    if ((byte_count == 0u)
        || (pe64_rva_in_sections(sections, section_count, rva, byte_count) == 0u))
    {
        return 0u;
    }

    cursor = actual_base + (u64)rva;
    end = cursor + (u64)byte_count;
    if ((cursor < actual_base) || (end <= cursor))
    {
        return 0u;
    }

    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 next;

        if ((region == 0)
            || ((region->prot_flags & required_prot) != required_prot)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end))
        {
            return 0u;
        }

        next = (region->virt_end < end) ? region->virt_end : end;
        if (next <= cursor)
        {
            return 0u;
        }
        cursor = next;
    }

    return 1u;
}

static u32 pe64_entry_transfer_ready(
    u32 pid,
    u64 entry_rip,
    u64 stack_base,
    u64 initial_rsp,
    pe64_entry_result_t *result)
{
    u64 entry_page;
    u64 stack_page;
    u32 entry_present;
    u32 entry_prot;
    u32 stack_present;
    u32 stack_prot;

    if ((pid == PROCESS64_INVALID_PID)
        || (entry_rip == 0ull)
        || (stack_base == 0ull)
        || (initial_rsp <= stack_base)
        || (vma64_find(pid, entry_rip) == 0)
        || (vma64_find(pid, stack_base) == 0)
        || (vma64_find(pid, initial_rsp - 1ull) == 0))
    {
        return 0u;
    }

    entry_page = pe64_align_down(entry_rip, VMA64_PAGE_BYTES);
    stack_page = pe64_align_down(stack_base, VMA64_PAGE_BYTES);
    entry_present = paging64_user_page_present(entry_page);
    entry_prot = paging64_user_page_protection(entry_page);
    stack_present = paging64_user_page_present(stack_page);
    stack_prot = paging64_user_page_protection(stack_page);

    if (result != 0)
    {
        result->entry_page_present = entry_present;
        result->entry_page_prot = entry_prot;
        result->stack_page_present = stack_present;
        result->stack_page_prot = stack_prot;
    }

    return ((entry_present != 0u)
        && ((entry_prot & PAGING64_USER_PROT_EXECUTE) != 0u)
        && (stack_present != 0u)
        && ((stack_prot & PAGING64_USER_PROT_WRITE) != 0u))
        ? 1u
        : 0u;
}

static u32 pe64_write_u32_to_va(
    u32 pid,
    u64 actual_base,
    u64 target_va,
    u32 value,
    pe64_tls_result_t *out_result)
{
    u64 target_page;
    u32 old_prot;
    u32 restore_required = 0u;
    vma_region_t *region;
    volatile u32 *slot;

    if ((actual_base == 0ull)
        || (target_va < actual_base)
        || ((target_va & ((u64)VMA64_PAGE_BYTES - 1ull)) > ((u64)VMA64_PAGE_BYTES - 4ull)))
    {
        pe64_set_tls_error(out_result, PE64_ERROR_TLS_INDEX_MAP);
        return PE64_DENIED;
    }

    region = vma64_find(pid, target_va);
    if ((region == 0)
        || ((target_va + 4ull) < target_va)
        || ((target_va + 4ull) > region->virt_end))
    {
        pe64_set_tls_error(out_result, PE64_ERROR_TLS_INDEX_MAP);
        return PE64_DENIED;
    }

    old_prot = region->prot_flags;
    target_page = pe64_align_down(target_va, VMA64_PAGE_BYTES);
    if ((old_prot & VMA64_PROT_WRITE) == 0u)
    {
        if (vma64_protect(pid, target_page, VMA64_PAGE_BYTES, old_prot | VMA64_PROT_WRITE) == 0u)
        {
            pe64_set_tls_error(out_result, PE64_ERROR_TLS_INDEX_MAP);
            return PE64_DENIED;
        }
        restore_required = 1u;
    }

    slot = (volatile u32 *)(u64)target_va;
    *slot = value;

    if (restore_required != 0u)
    {
        if (vma64_protect(pid, target_page, VMA64_PAGE_BYTES, old_prot) == 0u)
        {
            pe64_set_tls_error(out_result, PE64_ERROR_TLS_INDEX_MAP);
            return PE64_DENIED;
        }
    }

    return PE64_OK;
}

static u32 pe64_write_cookie_u64_to_va(
    u32 pid,
    u64 target_va,
    u64 value,
    pe64_security_cookie_result_t *out_result)
{
    u64 target_page;
    u32 old_prot;
    u32 restore_required = 0u;
    vma_region_t *region;

    if ((target_va == 0ull)
        || ((target_va + 8ull) < target_va)
        || ((target_va & ((u64)VMA64_PAGE_BYTES - 1ull))
            > ((u64)VMA64_PAGE_BYTES - 8ull)))
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_MAP);
        return PE64_DENIED;
    }

    region = vma64_find(pid, target_va);
    if ((region == 0)
        || ((target_va + 8ull) > region->virt_end)
        || (target_va < region->virt_base))
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_MAP);
        return PE64_DENIED;
    }

    old_prot = region->prot_flags;
    target_page = pe64_align_down(target_va, VMA64_PAGE_BYTES);
    if ((old_prot & VMA64_PROT_WRITE) == 0u)
    {
        if (vma64_protect(
                pid,
                target_page,
                VMA64_PAGE_BYTES,
                old_prot | VMA64_PROT_WRITE) == 0u)
        {
            pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_WRITE);
            return PE64_DENIED;
        }
        restore_required = 1u;
    }

    pe64_write_user_le64(target_va, value);

    if (restore_required != 0u)
    {
        if (vma64_protect(pid, target_page, VMA64_PAGE_BYTES, old_prot) == 0u)
        {
            pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_WRITE);
            return PE64_DENIED;
        }
    }

    if (out_result != 0)
    {
        out_result->page_protection = paging64_user_page_protection(target_va);
    }

    return PE64_OK;
}

static u32 pe64_copy_tls_template(
    u64 source_va,
    u64 target_va,
    u32 template_bytes,
    u32 zero_fill_bytes,
    pe64_tls_result_t *out_result)
{
    volatile u8 *source = (volatile u8 *)(u64)source_va;
    volatile u8 *target = (volatile u8 *)(u64)target_va;
    u32 index;

    for (index = 0u; index < template_bytes; ++index)
    {
        u8 value = source[index];

        target[index] = value;
        if (out_result != 0)
        {
            out_result->template_checksum =
                pe64_checksum_step(out_result->template_checksum, (u32)value);
            out_result->block_checksum =
                pe64_checksum_step(out_result->block_checksum, (u32)target[index]);
        }
    }

    for (index = 0u; index < zero_fill_bytes; ++index)
    {
        target[template_bytes + index] = 0u;
        if (out_result != 0)
        {
            out_result->block_checksum =
                pe64_checksum_step(out_result->block_checksum, 0u);
            if (target[template_bytes + index] != 0u)
            {
                ++out_result->zero_nonzero_count;
            }
        }
    }

    return PE64_OK;
}

static u8 pe64_ascii_lower(u8 value)
{
    if ((value >= (u8)'A') && (value <= (u8)'Z'))
    {
        return (u8)(value + ((u8)'a' - (u8)'A'));
    }

    return value;
}

static u32 pe64_cstring_from_rva_checksum(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u32 rva,
    u32 *out_checksum)
{
    u32 index;
    u32 checksum = 2166136261u;

    if (out_checksum != 0)
    {
        *out_checksum = checksum;
    }
    if ((sections == 0) || (actual_base == 0ull))
    {
        return 0u;
    }

    for (index = 0u; index < PE64_IMPORT_NAME_MAX_BYTES; ++index)
    {
        u32 byte_rva = rva + index;
        u64 address;
        u8 value;

        if ((byte_rva < rva)
            || (pe64_rva_in_sections(sections, section_count, byte_rva, 1u) == 0u))
        {
            return 0u;
        }

        address = actual_base + (u64)byte_rva;
        if ((address < actual_base) || (vma64_find(pid, address) == 0))
        {
            return 0u;
        }

        value = *((volatile u8 *)(u64)address);
        checksum = pe64_checksum_step(checksum, (u32)value);
        if (value == 0u)
        {
            if (out_checksum != 0)
            {
                *out_checksum = checksum;
            }
            return 1u;
        }
    }

    return 0u;
}

static u32 pe64_user_cstring_equals_casefold(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u32 rva,
    const char *expected)
{
    u32 index;

    if ((sections == 0) || (actual_base == 0ull) || (expected == 0))
    {
        return 0u;
    }

    for (index = 0u; index < PE64_IMPORT_NAME_MAX_BYTES; ++index)
    {
        u32 byte_rva = rva + index;
        u64 address;
        u8 actual_char;
        u8 expected_char = (u8)expected[index];

        if ((byte_rva < rva)
            || (pe64_rva_in_sections(sections, section_count, byte_rva, 1u) == 0u))
        {
            return 0u;
        }

        address = actual_base + (u64)byte_rva;
        if ((address < actual_base) || (vma64_find(pid, address) == 0))
        {
            return 0u;
        }

        actual_char = *((volatile u8 *)(u64)address);
        if (pe64_ascii_lower(actual_char) != pe64_ascii_lower(expected_char))
        {
            return 0u;
        }
        if ((actual_char == 0u) && (expected_char == 0u))
        {
            return 1u;
        }
    }

    return 0u;
}

static const pe64_shim_library_t *pe64_find_shim_library(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u32 name_rva,
    const pe64_shim_registry_t *registry,
    pe64_import_result_t *out_result)
{
    u32 index;
    u32 name_checksum = 0u;

    if (pe64_cstring_from_rva_checksum(
            pid,
            sections,
            section_count,
            actual_base,
            name_rva,
            &name_checksum) == 0u)
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_DLL_NAME);
        return 0;
    }
    if ((registry == 0)
        || (registry->libraries == 0)
        || (registry->library_count == 0u)
        || (registry->library_count > PE64_IMPORT_DESCRIPTOR_MAX_COUNT))
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_LIBRARY);
        return 0;
    }

    if (out_result != 0)
    {
        out_result->dll_checksum =
            pe64_checksum_step(out_result->dll_checksum, name_checksum);
    }

    for (index = 0u; index < registry->library_count; ++index)
    {
        const pe64_shim_library_t *library = &registry->libraries[index];

        if ((library->dll_name != 0)
            && (pe64_user_cstring_equals_casefold(
                pid,
                sections,
                section_count,
                actual_base,
                name_rva,
                library->dll_name) != 0u))
        {
            return library;
        }
    }

    pe64_set_import_error(out_result, PE64_ERROR_IMPORT_LIBRARY);
    return 0;
}

static const pe64_shim_symbol_t *pe64_find_shim_symbol_by_name(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u32 import_name_rva,
    const pe64_shim_library_t *library,
    pe64_import_result_t *out_result)
{
    u32 index;
    u32 symbol_name_rva = import_name_rva + 2u;
    u32 name_checksum = 0u;

    if ((library == 0)
        || (library->symbols == 0)
        || (library->symbol_count == 0u)
        || (library->symbol_count > PE64_IMPORT_THUNK_MAX_COUNT)
        || (symbol_name_rva < import_name_rva)
        || (pe64_rva_in_sections(sections, section_count, import_name_rva, 2u) == 0u)
        || (pe64_cstring_from_rva_checksum(
            pid,
            sections,
            section_count,
            actual_base,
            symbol_name_rva,
            &name_checksum) == 0u))
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_SYMBOL_NAME);
        return 0;
    }

    if (out_result != 0)
    {
        out_result->symbol_checksum =
            pe64_checksum_step(out_result->symbol_checksum, name_checksum);
    }

    for (index = 0u; index < library->symbol_count; ++index)
    {
        const pe64_shim_symbol_t *symbol = &library->symbols[index];

        if ((symbol->name != 0)
            && (symbol->address != 0ull)
            && (pe64_user_cstring_equals_casefold(
                pid,
                sections,
                section_count,
                actual_base,
                symbol_name_rva,
                symbol->name) != 0u))
        {
            return symbol;
        }
    }

    pe64_set_import_error(out_result, PE64_ERROR_IMPORT_SYMBOL);
    return 0;
}

static const pe64_shim_symbol_t *pe64_find_shim_symbol_by_ordinal(
    u16 ordinal,
    const pe64_shim_library_t *library,
    pe64_import_result_t *out_result)
{
    u32 index;

    if ((library == 0)
        || (library->symbols == 0)
        || (library->symbol_count == 0u)
        || (library->symbol_count > PE64_IMPORT_THUNK_MAX_COUNT))
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_SYMBOL);
        return 0;
    }

    if (out_result != 0)
    {
        out_result->symbol_checksum =
            pe64_checksum_step(out_result->symbol_checksum, (u32)ordinal);
    }

    for (index = 0u; index < library->symbol_count; ++index)
    {
        const pe64_shim_symbol_t *symbol = &library->symbols[index];

        if ((symbol->ordinal == ordinal) && (symbol->address != 0ull))
        {
            return symbol;
        }
    }

    pe64_set_import_error(out_result, PE64_ERROR_IMPORT_SYMBOL);
    return 0;
}

static u32 pe64_write_iat_slot(
    u32 pid,
    u64 actual_base,
    u32 iat_rva,
    u64 function_address,
    pe64_import_result_t *out_result)
{
    u64 target_address = actual_base + (u64)iat_rva;
    u64 target_page;
    u32 old_prot;
    u32 restore_required = 0u;
    vma_region_t *region;
    volatile u64 *slot;

    if ((actual_base == 0ull)
        || (target_address < actual_base)
        || ((target_address & ((u64)VMA64_PAGE_BYTES - 1ull)) > ((u64)VMA64_PAGE_BYTES - 8ull)))
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_THUNK_RANGE);
        return PE64_DENIED;
    }

    region = vma64_find(pid, target_address);
    if ((region == 0)
        || ((target_address + 8ull) < target_address)
        || ((target_address + 8ull) > region->virt_end))
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_IAT_MAP);
        return PE64_DENIED;
    }

    old_prot = region->prot_flags;
    target_page = pe64_align_down(target_address, VMA64_PAGE_BYTES);
    if ((old_prot & VMA64_PROT_WRITE) == 0u)
    {
        if (vma64_protect(pid, target_page, VMA64_PAGE_BYTES, old_prot | VMA64_PROT_WRITE) == 0u)
        {
            pe64_set_import_error(out_result, PE64_ERROR_IMPORT_WRITE);
            return PE64_DENIED;
        }
        restore_required = 1u;
    }

    slot = (volatile u64 *)(u64)target_address;
    *slot = function_address;

    if (restore_required != 0u)
    {
        if (vma64_protect(pid, target_page, VMA64_PAGE_BYTES, old_prot) == 0u)
        {
            pe64_set_import_error(out_result, PE64_ERROR_IMPORT_WRITE);
            return PE64_DENIED;
        }
    }

    if (out_result != 0)
    {
        out_result->iat_checksum = pe64_checksum_u64(out_result->iat_checksum, function_address);
    }

    return PE64_OK;
}

u32 pe64_parse_header(const u8 *data, u32 size, pe64_header_t *out_header)
{
    u32 pe_offset;
    u32 optional_offset;
    u32 section_table_offset;
    u32 section_table_bytes;
    u32 import_directory_offset;
    u32 exception_directory_offset;
    u32 tls_directory_offset;
    u32 load_config_directory_offset;

    pe64_clear_header(out_header);

    if ((data == 0) || (out_header == 0))
    {
        pe64_set_header_error(out_header, PE64_ERROR_NULL);
        return PE64_DENIED;
    }
    if (size < PE64_DOS_HEADER_MIN_BYTES)
    {
        pe64_set_header_error(out_header, PE64_ERROR_SHORT_DOS);
        return PE64_DENIED;
    }
    if (pe64_read_le16(data) != PE64_DOS_MAGIC_MZ)
    {
        pe64_set_header_error(out_header, PE64_ERROR_DOS_MAGIC);
        return PE64_DENIED;
    }

    pe_offset = pe64_read_le32(data + PE64_DOS_LFANEW_OFFSET);
    out_header->pe_offset = pe_offset;
    if (pe_offset < PE64_DOS_HEADER_MIN_BYTES)
    {
        pe64_set_header_error(out_header, PE64_ERROR_LFANEW_RANGE);
        return PE64_DENIED;
    }
    if (pe64_range_available(
            size,
            pe_offset,
            PE64_NT_SIGNATURE_BYTES + PE64_COFF_HEADER_BYTES) == 0u)
    {
        pe64_set_header_error(out_header, PE64_ERROR_LFANEW_RANGE);
        return PE64_DENIED;
    }
    if (pe64_read_le32(data + pe_offset) != PE64_NT_SIGNATURE)
    {
        pe64_set_header_error(out_header, PE64_ERROR_SIGNATURE);
        return PE64_DENIED;
    }

    out_header->machine = pe64_read_le16(data + pe_offset + 4u);
    out_header->number_of_sections = pe64_read_le16(data + pe_offset + 6u);
    out_header->time_date_stamp = pe64_read_le32(data + pe_offset + 8u);
    out_header->pointer_to_symbol_table = pe64_read_le32(data + pe_offset + 12u);
    out_header->number_of_symbols = pe64_read_le32(data + pe_offset + 16u);
    out_header->size_of_optional_header = pe64_read_le16(data + pe_offset + 20u);
    out_header->characteristics = pe64_read_le16(data + pe_offset + 22u);

    if (out_header->machine != PE64_MACHINE_AMD64)
    {
        pe64_set_header_error(out_header, PE64_ERROR_MACHINE);
        return PE64_DENIED;
    }
    if (out_header->number_of_sections == 0u)
    {
        pe64_set_header_error(out_header, PE64_ERROR_SECTION_COUNT);
        return PE64_DENIED;
    }
    if (out_header->size_of_optional_header != PE64_OPTIONAL_HEADER_PE32_PLUS_BYTES)
    {
        pe64_set_header_error(out_header, PE64_ERROR_OPTIONAL_SIZE);
        return PE64_DENIED;
    }

    optional_offset = pe_offset + PE64_NT_SIGNATURE_BYTES + PE64_COFF_HEADER_BYTES;
    if (pe64_range_available(
            size,
            optional_offset,
            (u32)out_header->size_of_optional_header) == 0u)
    {
        pe64_set_header_error(out_header, PE64_ERROR_OPTIONAL_SIZE);
        return PE64_DENIED;
    }

    out_header->optional_magic = pe64_read_le16(data + optional_offset);
    if (out_header->optional_magic != PE64_OPTIONAL_MAGIC_PE32_PLUS)
    {
        pe64_set_header_error(out_header, PE64_ERROR_OPTIONAL_MAGIC);
        return PE64_DENIED;
    }

    out_header->address_of_entry_point = pe64_read_le32(data + optional_offset + 0x10u);
    out_header->image_base = pe64_read_le64(data + optional_offset + 0x18u);
    out_header->section_alignment = pe64_read_le32(data + optional_offset + 0x20u);
    out_header->file_alignment = pe64_read_le32(data + optional_offset + 0x24u);
    out_header->size_of_image = pe64_read_le32(data + optional_offset + 0x38u);
    out_header->size_of_headers = pe64_read_le32(data + optional_offset + 0x3Cu);
    out_header->subsystem = pe64_read_le16(data + optional_offset + 0x44u);
    out_header->dll_characteristics = pe64_read_le16(data + optional_offset + 0x46u);
    out_header->size_of_stack_reserve = pe64_read_le64(data + optional_offset + 0x48u);
    out_header->size_of_stack_commit = pe64_read_le64(data + optional_offset + 0x50u);
    out_header->size_of_heap_reserve = pe64_read_le64(data + optional_offset + 0x58u);
    out_header->size_of_heap_commit = pe64_read_le64(data + optional_offset + 0x60u);
    out_header->number_of_rva_and_sizes = pe64_read_le32(data + optional_offset + 0x6Cu);
    if (out_header->number_of_rva_and_sizes > PE64_DIRECTORY_IMPORT_INDEX)
    {
        import_directory_offset = optional_offset
            + 0x70u
            + (PE64_DIRECTORY_IMPORT_INDEX * PE64_DATA_DIRECTORY_BYTES);
        if (pe64_range_available(size, import_directory_offset, PE64_DATA_DIRECTORY_BYTES) == 0u)
        {
            pe64_set_header_error(out_header, PE64_ERROR_OPTIONAL_SIZE);
            return PE64_DENIED;
        }
        out_header->import_directory_rva = pe64_read_le32(data + import_directory_offset);
        out_header->import_directory_size = pe64_read_le32(data + import_directory_offset + 4u);
    }
    if (out_header->number_of_rva_and_sizes > PE64_DIRECTORY_EXCEPTION_INDEX)
    {
        exception_directory_offset = optional_offset
            + 0x70u
            + (PE64_DIRECTORY_EXCEPTION_INDEX * PE64_DATA_DIRECTORY_BYTES);
        if (pe64_range_available(size, exception_directory_offset, PE64_DATA_DIRECTORY_BYTES) == 0u)
        {
            pe64_set_header_error(out_header, PE64_ERROR_OPTIONAL_SIZE);
            return PE64_DENIED;
        }
        out_header->exception_directory_rva = pe64_read_le32(data + exception_directory_offset);
        out_header->exception_directory_size =
            pe64_read_le32(data + exception_directory_offset + 4u);
    }
    if (out_header->number_of_rva_and_sizes > PE64_DIRECTORY_TLS_INDEX)
    {
        tls_directory_offset = optional_offset
            + 0x70u
            + (PE64_DIRECTORY_TLS_INDEX * PE64_DATA_DIRECTORY_BYTES);
        if (pe64_range_available(size, tls_directory_offset, PE64_DATA_DIRECTORY_BYTES) == 0u)
        {
            pe64_set_header_error(out_header, PE64_ERROR_OPTIONAL_SIZE);
            return PE64_DENIED;
        }
        out_header->tls_directory_rva = pe64_read_le32(data + tls_directory_offset);
        out_header->tls_directory_size = pe64_read_le32(data + tls_directory_offset + 4u);
    }
    if (out_header->number_of_rva_and_sizes > PE64_DIRECTORY_LOAD_CONFIG_INDEX)
    {
        load_config_directory_offset = optional_offset
            + 0x70u
            + (PE64_DIRECTORY_LOAD_CONFIG_INDEX * PE64_DATA_DIRECTORY_BYTES);
        if (pe64_range_available(
                size,
                load_config_directory_offset,
                PE64_DATA_DIRECTORY_BYTES) == 0u)
        {
            pe64_set_header_error(out_header, PE64_ERROR_OPTIONAL_SIZE);
            return PE64_DENIED;
        }
        out_header->load_config_directory_rva =
            pe64_read_le32(data + load_config_directory_offset);
        out_header->load_config_directory_size =
            pe64_read_le32(data + load_config_directory_offset + 4u);
    }

    if ((out_header->section_alignment == 0u) || (out_header->file_alignment == 0u))
    {
        pe64_set_header_error(out_header, PE64_ERROR_ALIGNMENT);
        return PE64_DENIED;
    }
    if ((out_header->size_of_image == 0u)
        || ((out_header->size_of_image % out_header->section_alignment) != 0u))
    {
        pe64_set_header_error(out_header, PE64_ERROR_IMAGE_SIZE);
        return PE64_DENIED;
    }
    if ((out_header->size_of_headers == 0u)
        || (out_header->size_of_headers > out_header->size_of_image))
    {
        pe64_set_header_error(out_header, PE64_ERROR_HEADER_SIZE);
        return PE64_DENIED;
    }
    if (out_header->address_of_entry_point >= out_header->size_of_image)
    {
        pe64_set_header_error(out_header, PE64_ERROR_ENTRY_RANGE);
        return PE64_DENIED;
    }

    section_table_offset = optional_offset + (u32)out_header->size_of_optional_header;
    section_table_bytes = ((u32)out_header->number_of_sections) * PE64_SECTION_HEADER_BYTES;
    if (pe64_range_available(size, section_table_offset, section_table_bytes) == 0u)
    {
        pe64_set_header_error(out_header, PE64_ERROR_SECTION_TABLE_RANGE);
        return PE64_DENIED;
    }

    out_header->error = PE64_ERROR_NONE;
    return PE64_OK;
}

u32 pe64_parse_sections(
    const u8 *data,
    u32 size,
    const pe64_header_t *header,
    pe64_section_t *out_sections,
    u32 max_sections,
    pe64_section_summary_t *out_summary)
{
    u32 index;
    u32 name_index;
    u32 section_table_offset;
    u32 section_table_bytes;

    pe64_clear_section_summary(out_summary);

    if (out_sections != 0)
    {
        for (index = 0u; index < max_sections; ++index)
        {
            pe64_clear_section(&out_sections[index]);
        }
    }

    if ((data == 0) || (header == 0) || (out_sections == 0) || (out_summary == 0))
    {
        pe64_set_section_summary_error(out_summary, PE64_ERROR_NULL);
        return PE64_DENIED;
    }
    if (header->error != PE64_ERROR_NONE)
    {
        pe64_set_section_summary_error(out_summary, header->error);
        return PE64_DENIED;
    }
    if (header->number_of_sections == 0u)
    {
        pe64_set_section_summary_error(out_summary, PE64_ERROR_SECTION_COUNT);
        return PE64_DENIED;
    }
    if (header->number_of_sections > max_sections)
    {
        pe64_set_section_summary_error(out_summary, PE64_ERROR_OUTPUT_CAPACITY);
        return PE64_DENIED;
    }

    section_table_offset = header->pe_offset
        + PE64_NT_SIGNATURE_BYTES
        + PE64_COFF_HEADER_BYTES
        + (u32)header->size_of_optional_header;
    section_table_bytes = ((u32)header->number_of_sections) * PE64_SECTION_HEADER_BYTES;
    if (pe64_range_available(size, section_table_offset, section_table_bytes) == 0u)
    {
        pe64_set_section_summary_error(out_summary, PE64_ERROR_SECTION_TABLE_RANGE);
        return PE64_DENIED;
    }

    for (index = 0u; index < header->number_of_sections; ++index)
    {
        const u8 *record = data + section_table_offset + (index * PE64_SECTION_HEADER_BYTES);
        pe64_section_t *section = &out_sections[index];
        u32 virtual_span;
        u32 virtual_end;

        for (name_index = 0u; name_index < PE64_SECTION_NAME_BYTES; ++name_index)
        {
            section->name[name_index] = record[name_index];
        }
        section->virtual_size = pe64_read_le32(record + 8u);
        section->virtual_address = pe64_read_le32(record + 12u);
        section->size_of_raw_data = pe64_read_le32(record + 16u);
        section->pointer_to_raw_data = pe64_read_le32(record + 20u);
        section->pointer_to_relocations = pe64_read_le32(record + 24u);
        section->pointer_to_linenumbers = pe64_read_le32(record + 28u);
        section->number_of_relocations = pe64_read_le16(record + 32u);
        section->number_of_linenumbers = pe64_read_le16(record + 34u);
        section->characteristics = pe64_read_le32(record + 36u);
        section->prot_flags = pe64_section_prot_flags(section->characteristics);

        virtual_span = pe64_section_span(section);
        if (virtual_span == 0u)
        {
            pe64_set_section_summary_error(out_summary, PE64_ERROR_SECTION_SIZE);
            return PE64_DENIED;
        }
        if (section->virtual_address >= header->size_of_image)
        {
            pe64_set_section_summary_error(out_summary, PE64_ERROR_SECTION_TABLE_RANGE);
            return PE64_DENIED;
        }
        if (virtual_span > (header->size_of_image - section->virtual_address))
        {
            pe64_set_section_summary_error(out_summary, PE64_ERROR_SECTION_TABLE_RANGE);
            return PE64_DENIED;
        }

        virtual_end = section->virtual_address + virtual_span;
        if ((index == 0u) || (section->virtual_address < out_summary->first_virtual_address))
        {
            out_summary->first_virtual_address = section->virtual_address;
        }
        if (virtual_end > out_summary->max_virtual_end)
        {
            out_summary->max_virtual_end = virtual_end;
        }

        out_summary->total_virtual_bytes += (u64)virtual_span;
        out_summary->total_raw_bytes += (u64)section->size_of_raw_data;
        if ((section->characteristics & PE64_SCN_MEM_READ) != 0u)
        {
            ++out_summary->readable_count;
        }
        if ((section->characteristics & PE64_SCN_MEM_WRITE) != 0u)
        {
            ++out_summary->writable_count;
        }
        if ((section->characteristics & PE64_SCN_MEM_EXECUTE) != 0u)
        {
            ++out_summary->executable_count;
        }
        if ((section->characteristics & PE64_SCN_CNT_CODE) != 0u)
        {
            ++out_summary->code_count;
        }
        if ((section->characteristics & PE64_SCN_CNT_INITIALIZED_DATA) != 0u)
        {
            ++out_summary->initialized_data_count;
        }
        if ((section->characteristics & PE64_SCN_CNT_UNINITIALIZED_DATA) != 0u)
        {
            ++out_summary->uninitialized_data_count;
        }

        for (name_index = 0u; name_index < PE64_SECTION_NAME_BYTES; ++name_index)
        {
            out_summary->name_checksum =
                pe64_checksum_step(out_summary->name_checksum, (u32)section->name[name_index]);
        }
        out_summary->name_checksum =
            pe64_checksum_step(out_summary->name_checksum, section->virtual_address);
        out_summary->name_checksum =
            pe64_checksum_step(out_summary->name_checksum, section->virtual_size);
        out_summary->name_checksum =
            pe64_checksum_step(out_summary->name_checksum, section->characteristics);
    }

    out_summary->section_count = (u32)header->number_of_sections;
    out_summary->error = PE64_ERROR_NONE;
    return PE64_OK;
}

u32 pe64_map_sections(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    const u8 *binary_data,
    u32 binary_size,
    u64 actual_base,
    pe64_map_result_t *out_result)
{
    u64 mapped_bases[PE64_MAX_SECTIONS];
    u64 mapped_lengths[PE64_MAX_SECTIONS];
    u32 index;
    u32 mapped_count = 0u;

    pe64_clear_map_result(out_result);

    for (index = 0u; index < PE64_MAX_SECTIONS; ++index)
    {
        mapped_bases[index] = 0ull;
        mapped_lengths[index] = 0ull;
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (header == 0)
        || (sections == 0)
        || (binary_data == 0)
        || (out_result == 0))
    {
        pe64_set_map_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }
    if (header->error != PE64_ERROR_NONE)
    {
        pe64_set_map_error(out_result, header->error);
        return PE64_DENIED;
    }
    if ((section_count == 0u)
        || (section_count != (u32)header->number_of_sections)
        || (section_count > PE64_MAX_SECTIONS))
    {
        pe64_set_map_error(out_result, PE64_ERROR_SECTION_COUNT);
        return PE64_DENIED;
    }
    if ((actual_base == 0ull)
        || ((actual_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        pe64_set_map_error(out_result, PE64_ERROR_SECTION_ADDRESS);
        return PE64_DENIED;
    }

    out_result->section_count = section_count;
    out_result->actual_base = actual_base;

    for (index = 0u; index < section_count; ++index)
    {
        const pe64_section_t *section = &sections[index];
        u64 virtual_span = (u64)pe64_section_span(section);
        u64 section_start;
        u64 section_end;
        u64 map_base;
        u64 map_offset;
        u64 map_bytes;
        u64 copy_bytes;
        u32 final_prot;

        if (virtual_span == 0ull)
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_SIZE);
            return PE64_DENIED;
        }
        if ((section->size_of_raw_data != 0u)
            && (pe64_range_available(
                binary_size,
                section->pointer_to_raw_data,
                section->size_of_raw_data) == 0u))
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_RAW_RANGE);
            return PE64_DENIED;
        }

        section_start = actual_base + (u64)section->virtual_address;
        if (section_start < actual_base)
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_ADDRESS);
            return PE64_DENIED;
        }
        section_end = section_start + virtual_span;
        if (section_end < section_start)
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_ADDRESS);
            return PE64_DENIED;
        }

        map_base = pe64_align_down(section_start, VMA64_PAGE_BYTES);
        map_offset = section_start - map_base;
        map_bytes = pe64_align_up(map_offset + virtual_span, VMA64_PAGE_BYTES);
        if ((map_bytes == 0ull) || ((map_base + map_bytes) < map_base))
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_ADDRESS);
            return PE64_DENIED;
        }

        final_prot = pe64_vma_prot_from_section(section->prot_flags);
        if (final_prot == 0u)
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_PROTECT);
            return PE64_DENIED;
        }

        if (vma64_map_anon(
                pid,
                map_base,
                map_bytes,
                VMA64_PROT_READ | VMA64_PROT_WRITE,
                VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
            != map_base)
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_MAP);
            return PE64_DENIED;
        }

        mapped_bases[mapped_count] = map_base;
        mapped_lengths[mapped_count] = map_bytes;
        ++mapped_count;

        copy_bytes = (u64)section->size_of_raw_data;
        if (copy_bytes > virtual_span)
        {
            copy_bytes = virtual_span;
        }
        if (copy_bytes != 0ull)
        {
            (void)pe64_copy_to_user(
                section_start,
                binary_data + section->pointer_to_raw_data,
                copy_bytes,
                &out_result->source_checksum,
                &out_result->mapped_checksum);
        }
        out_result->bss_nonzero_count += pe64_zero_user(
            section_start + copy_bytes,
            virtual_span - copy_bytes);

        if (vma64_protect(pid, map_base, map_bytes, final_prot) == 0u)
        {
            pe64_unmap_recorded_sections(pid, mapped_bases, mapped_lengths, mapped_count);
            pe64_set_map_error(out_result, PE64_ERROR_SECTION_PROTECT);
            return PE64_DENIED;
        }

        if (out_result->mapped_count == 0u)
        {
            out_result->first_mapped_vaddr = map_base;
        }
        ++out_result->mapped_count;
        out_result->total_map_bytes += map_bytes;
        out_result->total_file_bytes += copy_bytes;
        out_result->total_bss_bytes += virtual_span - copy_bytes;
        if ((map_base + map_bytes) > out_result->max_mapped_end)
        {
            out_result->max_mapped_end = map_base + map_bytes;
        }
    }

    pe64_set_map_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_apply_relocations(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 preferred_base,
    u64 actual_base,
    pe64_reloc_result_t *out_result)
{
    const pe64_section_t *reloc_section;
    u64 delta;
    u64 reloc_base;
    u32 reloc_bytes;
    u32 cursor = 0u;

    pe64_clear_reloc_result(out_result);

    if ((pid == PROCESS64_INVALID_PID)
        || (sections == 0)
        || (out_result == 0)
        || (section_count == 0u)
        || (section_count > PE64_MAX_SECTIONS)
        || (preferred_base == 0ull)
        || (actual_base == 0ull))
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }

    delta = actual_base - preferred_base;
    out_result->preferred_base = preferred_base;
    out_result->actual_base = actual_base;
    out_result->delta = delta;

    if (delta == 0ull)
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_NONE);
        return PE64_OK;
    }

    reloc_section = pe64_find_reloc_section(sections, section_count);
    if (reloc_section == 0)
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_SECTION);
        return PE64_DENIED;
    }

    reloc_bytes = pe64_section_span(reloc_section);
    if (reloc_bytes < PE64_RELOC_BLOCK_HEADER_BYTES)
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_BLOCK_RANGE);
        return PE64_DENIED;
    }

    reloc_base = actual_base + (u64)reloc_section->virtual_address;
    if ((reloc_base < actual_base) || (vma64_find(pid, reloc_base) == 0))
    {
        pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_SECTION);
        return PE64_DENIED;
    }

    out_result->reloc_section_rva = reloc_section->virtual_address;
    out_result->reloc_section_bytes = reloc_bytes;

    while (cursor < reloc_bytes)
    {
        volatile u8 *block = (volatile u8 *)(u64)(reloc_base + (u64)cursor);
        u32 page_rva;
        u32 block_size;
        u32 entry_offset;

        if ((reloc_bytes - cursor) < PE64_RELOC_BLOCK_HEADER_BYTES)
        {
            pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_BLOCK_RANGE);
            return PE64_DENIED;
        }

        page_rva = ((u32)block[0])
            | ((u32)block[1] << 8)
            | ((u32)block[2] << 16)
            | ((u32)block[3] << 24);
        block_size = ((u32)block[4])
            | ((u32)block[5] << 8)
            | ((u32)block[6] << 16)
            | ((u32)block[7] << 24);

        if ((page_rva == 0u) && (block_size == 0u))
        {
            break;
        }
        if ((block_size < PE64_RELOC_BLOCK_HEADER_BYTES)
            || ((block_size & 1u) != 0u))
        {
            pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_BLOCK_SIZE);
            return PE64_DENIED;
        }
        if (block_size > (reloc_bytes - cursor))
        {
            pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_BLOCK_RANGE);
            return PE64_DENIED;
        }

        ++out_result->block_count;
        entry_offset = PE64_RELOC_BLOCK_HEADER_BYTES;
        while (entry_offset < block_size)
        {
            u16 entry = (u16)(((u16)block[entry_offset])
                | ((u16)block[entry_offset + 1u] << 8));
            u32 type = ((u32)entry >> 12) & 0x0Fu;
            u32 offset = (u32)entry & 0x0FFFu;
            u32 target_rva;

            ++out_result->entry_count;
            if (type == PE64_IMAGE_REL_BASED_ABSOLUTE)
            {
                ++out_result->skipped_count;
            }
            else if (type == PE64_IMAGE_REL_BASED_DIR64)
            {
                target_rva = page_rva + offset;
                if (target_rva < page_rva)
                {
                    pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_TARGET_RANGE);
                    return PE64_DENIED;
                }
                if (pe64_apply_one_dir64(
                        pid,
                        sections,
                        section_count,
                        actual_base,
                        delta,
                        target_rva,
                        out_result) == PE64_DENIED)
                {
                    return PE64_DENIED;
                }
            }
            else
            {
                pe64_set_reloc_error(out_result, PE64_ERROR_RELOC_TYPE);
                return PE64_DENIED;
            }

            entry_offset += 2u;
        }

        cursor += block_size;
    }

    pe64_set_reloc_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_resolve_imports(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    const pe64_shim_registry_t *shim_registry,
    pe64_import_result_t *out_result)
{
    u32 descriptor_offset = 0u;
    u32 saw_terminator = 0u;

    pe64_clear_import_result(out_result);

    if ((pid == PROCESS64_INVALID_PID)
        || (header == 0)
        || (sections == 0)
        || (shim_registry == 0)
        || (out_result == 0)
        || (section_count == 0u)
        || (section_count > PE64_MAX_SECTIONS)
        || (actual_base == 0ull))
    {
        pe64_set_import_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }
    if (header->error != PE64_ERROR_NONE)
    {
        pe64_set_import_error(out_result, header->error);
        return PE64_DENIED;
    }
    if ((header->import_directory_rva == 0u) || (header->import_directory_size == 0u))
    {
        pe64_set_import_error(out_result, PE64_ERROR_NONE);
        return PE64_OK;
    }
    if ((header->import_directory_size < PE64_IMPORT_DESCRIPTOR_BYTES)
        || (pe64_rva_in_sections(
            sections,
            section_count,
            header->import_directory_rva,
            header->import_directory_size) == 0u)
        || (vma64_find(pid, actual_base + (u64)header->import_directory_rva) == 0))
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_DIRECTORY);
        return PE64_DENIED;
    }

    out_result->import_directory_rva = header->import_directory_rva;
    out_result->import_directory_bytes = header->import_directory_size;

    while (descriptor_offset < header->import_directory_size)
    {
        u32 descriptor_rva = header->import_directory_rva + descriptor_offset;
        u64 descriptor_address = actual_base + (u64)descriptor_rva;
        volatile u8 *descriptor;
        u32 original_first_thunk;
        u32 name_rva;
        u32 first_thunk;
        u32 lookup_rva;
        u32 thunk_index;
        const pe64_shim_library_t *library;

        if (((header->import_directory_size - descriptor_offset) < PE64_IMPORT_DESCRIPTOR_BYTES)
            || (descriptor_rva < header->import_directory_rva)
            || (vma64_find(pid, descriptor_address) == 0))
        {
            pe64_set_import_error(out_result, PE64_ERROR_IMPORT_DESCRIPTOR_RANGE);
            return PE64_DENIED;
        }

        descriptor = (volatile u8 *)(u64)descriptor_address;
        original_first_thunk = ((u32)descriptor[0])
            | ((u32)descriptor[1] << 8)
            | ((u32)descriptor[2] << 16)
            | ((u32)descriptor[3] << 24);
        name_rva = ((u32)descriptor[12])
            | ((u32)descriptor[13] << 8)
            | ((u32)descriptor[14] << 16)
            | ((u32)descriptor[15] << 24);
        first_thunk = ((u32)descriptor[16])
            | ((u32)descriptor[17] << 8)
            | ((u32)descriptor[18] << 16)
            | ((u32)descriptor[19] << 24);

        if ((original_first_thunk == 0u) && (name_rva == 0u) && (first_thunk == 0u))
        {
            saw_terminator = 1u;
            break;
        }
        if (out_result->descriptor_count >= PE64_IMPORT_DESCRIPTOR_MAX_COUNT)
        {
            pe64_set_import_error(out_result, PE64_ERROR_IMPORT_DESCRIPTOR_RANGE);
            return PE64_DENIED;
        }
        if ((name_rva == 0u) || (first_thunk == 0u))
        {
            pe64_set_import_error(out_result, PE64_ERROR_IMPORT_DESCRIPTOR_RANGE);
            return PE64_DENIED;
        }

        library = pe64_find_shim_library(
            pid,
            sections,
            section_count,
            actual_base,
            name_rva,
            shim_registry,
            out_result);
        if (library == 0)
        {
            return PE64_DENIED;
        }

        out_result->dll_name_rva = name_rva;
        ++out_result->descriptor_count;
        lookup_rva = (original_first_thunk != 0u) ? original_first_thunk : first_thunk;

        for (thunk_index = 0u; thunk_index < PE64_IMPORT_THUNK_MAX_COUNT; ++thunk_index)
        {
            u32 current_lookup_rva = lookup_rva + (thunk_index * 8u);
            u32 current_iat_rva = first_thunk + (thunk_index * 8u);
            u64 lookup_address = actual_base + (u64)current_lookup_rva;
            volatile u64 *lookup_slot;
            u64 lookup_value;
            const pe64_shim_symbol_t *symbol;

            if ((current_lookup_rva < lookup_rva)
                || (current_iat_rva < first_thunk)
                || (pe64_rva_in_sections(sections, section_count, current_lookup_rva, 8u) == 0u)
                || (pe64_rva_in_sections(sections, section_count, current_iat_rva, 8u) == 0u)
                || (vma64_find(pid, lookup_address) == 0))
            {
                pe64_set_import_error(out_result, PE64_ERROR_IMPORT_THUNK_RANGE);
                return PE64_DENIED;
            }

            lookup_slot = (volatile u64 *)(u64)lookup_address;
            lookup_value = *lookup_slot;
            if (lookup_value == 0ull)
            {
                break;
            }

            ++out_result->thunk_count;
            if ((lookup_value & PE64_IMPORT_ORDINAL_FLAG64) != 0ull)
            {
                u16 ordinal = (u16)(lookup_value & PE64_IMPORT_ORDINAL_MASK);

                ++out_result->ordinal_import_count;
                symbol = pe64_find_shim_symbol_by_ordinal(ordinal, library, out_result);
            }
            else
            {
                if ((lookup_value >> 32) != 0ull)
                {
                    pe64_set_import_error(out_result, PE64_ERROR_IMPORT_SYMBOL_NAME);
                    return PE64_DENIED;
                }

                ++out_result->name_import_count;
                symbol = pe64_find_shim_symbol_by_name(
                    pid,
                    sections,
                    section_count,
                    actual_base,
                    (u32)lookup_value,
                    library,
                    out_result);
            }

            if (symbol == 0)
            {
                return PE64_DENIED;
            }
            if (pe64_write_iat_slot(
                    pid,
                    actual_base,
                    current_iat_rva,
                    symbol->address,
                    out_result) == PE64_DENIED)
            {
                return PE64_DENIED;
            }

            if (out_result->resolved_count == 0u)
            {
                out_result->first_thunk_rva = current_iat_rva;
                out_result->first_function = symbol->address;
            }
            out_result->last_thunk_rva = current_iat_rva;
            out_result->last_function = symbol->address;
            ++out_result->resolved_count;
        }

        descriptor_offset += PE64_IMPORT_DESCRIPTOR_BYTES;
    }

    if (saw_terminator == 0u)
    {
        pe64_set_import_error(out_result, PE64_ERROR_IMPORT_DESCRIPTOR_RANGE);
        return PE64_DENIED;
    }

    pe64_set_import_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_handle_tls(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u64 tls_block_base,
    u32 tls_index,
    pe64_tls_callback_dispatch_t callback_dispatch,
    void *callback_context,
    pe64_tls_result_t *out_result)
{
    u64 directory_va;
    u64 raw_start_va;
    u64 raw_end_va;
    u64 index_va;
    u64 callbacks_va;
    u32 raw_start_rva;
    u32 raw_end_rva;
    u32 index_rva;
    u32 template_bytes;
    u32 zero_fill_bytes;
    u32 total_tls_bytes;
    u64 map_bytes;
    u32 callbacks_rva = 0u;
    u32 callback_index;
    u32 saw_callback_terminator = 0u;
    persona_context_t *context;

    pe64_clear_tls_result(out_result);

    if ((pid == PROCESS64_INVALID_PID)
        || (header == 0)
        || (sections == 0)
        || (out_result == 0)
        || (section_count == 0u)
        || (section_count > PE64_MAX_SECTIONS)
        || (actual_base == 0ull))
    {
        pe64_set_tls_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }
    if (header->error != PE64_ERROR_NONE)
    {
        pe64_set_tls_error(out_result, header->error);
        return PE64_DENIED;
    }
    if ((header->tls_directory_rva == 0u) || (header->tls_directory_size == 0u))
    {
        pe64_set_tls_error(out_result, PE64_ERROR_NONE);
        return PE64_OK;
    }
    if ((header->tls_directory_size < PE64_TLS_DIRECTORY64_BYTES)
        || (pe64_mapped_rva_range(
            pid,
            sections,
            section_count,
            actual_base,
            header->tls_directory_rva,
            PE64_TLS_DIRECTORY64_BYTES,
            VMA64_PROT_READ) == 0u))
    {
        pe64_set_tls_error(out_result, PE64_ERROR_TLS_DIRECTORY);
        return PE64_DENIED;
    }

    directory_va = actual_base + (u64)header->tls_directory_rva;
    raw_start_va = pe64_read_user_le64(directory_va);
    raw_end_va = pe64_read_user_le64(directory_va + 8ull);
    index_va = pe64_read_user_le64(directory_va + 16ull);
    callbacks_va = pe64_read_user_le64(directory_va + 24ull);
    zero_fill_bytes = pe64_read_user_le32(directory_va + 32ull);

    out_result->tls_directory_rva = header->tls_directory_rva;
    out_result->tls_directory_bytes = header->tls_directory_size;
    out_result->raw_start_va = raw_start_va;
    out_result->raw_end_va = raw_end_va;
    out_result->index_va = index_va;
    out_result->callbacks_va = callbacks_va;
    out_result->zero_fill_bytes = zero_fill_bytes;
    out_result->index_value = tls_index;

    if ((raw_end_va < raw_start_va)
        || (pe64_va_to_rva(actual_base, raw_start_va, &raw_start_rva) == 0u)
        || (pe64_va_to_rva(actual_base, raw_end_va, &raw_end_rva) == 0u)
        || (pe64_va_to_rva(actual_base, index_va, &index_rva) == 0u))
    {
        pe64_set_tls_error(out_result, PE64_ERROR_TLS_RAW_RANGE);
        return PE64_DENIED;
    }

    template_bytes = (u32)(raw_end_va - raw_start_va);
    out_result->template_bytes = template_bytes;
    if ((template_bytes > PE64_TLS_MAX_BLOCK_BYTES)
        || (zero_fill_bytes > PE64_TLS_MAX_BLOCK_BYTES)
        || ((template_bytes + zero_fill_bytes) < template_bytes)
        || ((template_bytes + zero_fill_bytes) > PE64_TLS_MAX_BLOCK_BYTES)
        || (pe64_mapped_rva_range(
            pid,
            sections,
            section_count,
            actual_base,
            raw_start_rva,
            (template_bytes != 0u) ? template_bytes : 1u,
            VMA64_PROT_READ) == 0u)
        || (pe64_mapped_rva_range(
            pid,
            sections,
            section_count,
            actual_base,
            index_rva,
            4u,
            VMA64_PROT_READ) == 0u))
    {
        pe64_set_tls_error(out_result, PE64_ERROR_TLS_SIZE);
        return PE64_DENIED;
    }

    total_tls_bytes = template_bytes + zero_fill_bytes;
    if (total_tls_bytes != 0u)
    {
        if ((tls_block_base == 0ull)
            || ((tls_block_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
        {
            pe64_set_tls_error(out_result, PE64_ERROR_TLS_BLOCK_MAP);
            return PE64_DENIED;
        }

        map_bytes = pe64_align_up((u64)total_tls_bytes, VMA64_PAGE_BYTES);
        if ((map_bytes == 0ull) || (map_bytes > (u64)PE64_TLS_MAX_BLOCK_BYTES))
        {
            pe64_set_tls_error(out_result, PE64_ERROR_TLS_BLOCK_MAP);
            return PE64_DENIED;
        }
        if (vma64_map_anon(
                pid,
                tls_block_base,
                map_bytes,
                VMA64_PROT_READ | VMA64_PROT_WRITE,
                VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
            != tls_block_base)
        {
            pe64_set_tls_error(out_result, PE64_ERROR_TLS_BLOCK_MAP);
            return PE64_DENIED;
        }

        out_result->tls_block_base = tls_block_base;
        out_result->tls_block_bytes = map_bytes;
        (void)pe64_copy_tls_template(
            raw_start_va,
            tls_block_base,
            template_bytes,
            zero_fill_bytes,
            out_result);
    }

    if (pe64_write_u32_to_va(pid, actual_base, index_va, tls_index, out_result) == PE64_DENIED)
    {
        if (out_result->tls_block_base != 0ull)
        {
            (void)vma64_unmap(pid, out_result->tls_block_base, out_result->tls_block_bytes);
        }
        return PE64_DENIED;
    }
    out_result->index_written = 1u;

    context = persona64_context_for_process(pid);
    if ((context != 0) && (context->persona_type == PERSONA64_TYPE_WINDOWS_PE))
    {
        context->windows_tls_pointer = out_result->tls_block_base;
    }

    if (callbacks_va != 0ull)
    {
        if (pe64_va_to_rva(actual_base, callbacks_va, &callbacks_rva) == 0u)
        {
            pe64_set_tls_error(out_result, PE64_ERROR_TLS_CALLBACK_TABLE);
            if (out_result->tls_block_base != 0ull)
            {
                (void)vma64_unmap(pid, out_result->tls_block_base, out_result->tls_block_bytes);
            }
            return PE64_DENIED;
        }

        for (callback_index = 0u; callback_index < PE64_TLS_CALLBACK_MAX_COUNT; ++callback_index)
        {
            u32 callback_slot_rva = callbacks_rva + (callback_index * 8u);
            u64 callback_slot_va;
            u64 callback_va;
            u32 callback_rva;

            if ((callback_slot_rva < callbacks_rva)
                || (pe64_mapped_rva_range(
                    pid,
                    sections,
                    section_count,
                    actual_base,
                    callback_slot_rva,
                    8u,
                    VMA64_PROT_READ) == 0u))
            {
                pe64_set_tls_error(out_result, PE64_ERROR_TLS_CALLBACK_TABLE);
                if (out_result->tls_block_base != 0ull)
                {
                    (void)vma64_unmap(pid, out_result->tls_block_base, out_result->tls_block_bytes);
                }
                return PE64_DENIED;
            }

            callback_slot_va = actual_base + (u64)callback_slot_rva;
            callback_va = pe64_read_user_le64(callback_slot_va);
            if (callback_va == 0ull)
            {
                saw_callback_terminator = 1u;
                break;
            }
            if ((pe64_va_to_rva(actual_base, callback_va, &callback_rva) == 0u)
                || (pe64_mapped_rva_range(
                    pid,
                    sections,
                    section_count,
                    actual_base,
                    callback_rva,
                    1u,
                    VMA64_PROT_EXECUTE) == 0u))
            {
                pe64_set_tls_error(out_result, PE64_ERROR_TLS_CALLBACK_TARGET);
                if (out_result->tls_block_base != 0ull)
                {
                    (void)vma64_unmap(pid, out_result->tls_block_base, out_result->tls_block_bytes);
                }
                return PE64_DENIED;
            }
            if (callback_dispatch == 0)
            {
                pe64_set_tls_error(out_result, PE64_ERROR_TLS_CALLBACK_DISPATCH);
                if (out_result->tls_block_base != 0ull)
                {
                    (void)vma64_unmap(pid, out_result->tls_block_base, out_result->tls_block_bytes);
                }
                return PE64_DENIED;
            }

            if (out_result->callback_count == 0u)
            {
                out_result->first_callback = callback_va;
            }
            out_result->last_callback = callback_va;
            out_result->callback_checksum =
                pe64_checksum_u64(out_result->callback_checksum, callback_va);
            ++out_result->callback_count;

            if (callback_dispatch(
                    pid,
                    callback_va,
                    actual_base,
                    PE64_TLS_REASON_DLL_PROCESS_ATTACH,
                    0ull,
                    callback_context) != PE64_OK)
            {
                pe64_set_tls_error(out_result, PE64_ERROR_TLS_CALLBACK_DISPATCH);
                if (out_result->tls_block_base != 0ull)
                {
                    (void)vma64_unmap(pid, out_result->tls_block_base, out_result->tls_block_bytes);
                }
                return PE64_DENIED;
            }

            ++out_result->invoked_count;
        }

        if (saw_callback_terminator == 0u)
        {
            pe64_set_tls_error(out_result, PE64_ERROR_TLS_CALLBACK_TABLE);
            if (out_result->tls_block_base != 0ull)
            {
                (void)vma64_unmap(pid, out_result->tls_block_base, out_result->tls_block_bytes);
            }
            return PE64_DENIED;
        }
    }

    pe64_set_tls_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_register_exception_directory(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    pe64_exception_result_t *out_result)
{
    persona_context_t *context;
    u32 function_count;
    u32 index;

    pe64_clear_exception_result(out_result);

    if ((pid == PROCESS64_INVALID_PID)
        || (header == 0)
        || (sections == 0)
        || (out_result == 0)
        || (section_count == 0u)
        || (section_count > PE64_MAX_SECTIONS)
        || (actual_base == 0ull))
    {
        pe64_set_exception_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }
    if (header->error != PE64_ERROR_NONE)
    {
        pe64_set_exception_error(out_result, header->error);
        return PE64_DENIED;
    }
    if ((header->exception_directory_rva == 0u) || (header->exception_directory_size == 0u))
    {
        pe64_set_exception_error(out_result, PE64_ERROR_NONE);
        return PE64_OK;
    }

    out_result->exception_directory_rva = header->exception_directory_rva;
    out_result->exception_directory_bytes = header->exception_directory_size;
    out_result->table_base = actual_base + (u64)header->exception_directory_rva;
    out_result->table_bytes = (u64)header->exception_directory_size;

    if ((out_result->table_base < actual_base)
        || (header->exception_directory_size < PE64_RUNTIME_FUNCTION_BYTES)
        || ((header->exception_directory_size % PE64_RUNTIME_FUNCTION_BYTES) != 0u))
    {
        pe64_set_exception_error(out_result, PE64_ERROR_EXCEPTION_SIZE);
        return PE64_DENIED;
    }

    function_count = header->exception_directory_size / PE64_RUNTIME_FUNCTION_BYTES;
    out_result->function_count = function_count;
    if ((function_count == 0u) || (function_count > PE64_EXCEPTION_MAX_FUNCTIONS))
    {
        pe64_set_exception_error(out_result, PE64_ERROR_EXCEPTION_SIZE);
        return PE64_DENIED;
    }
    if (pe64_mapped_rva_range(
            pid,
            sections,
            section_count,
            actual_base,
            header->exception_directory_rva,
            header->exception_directory_size,
            VMA64_PROT_READ) == 0u)
    {
        pe64_set_exception_error(out_result, PE64_ERROR_EXCEPTION_DIRECTORY);
        return PE64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        pe64_set_exception_error(out_result, PE64_ERROR_EXCEPTION_PERSONA);
        return PE64_DENIED;
    }

    for (index = 0u; index < function_count; ++index)
    {
        u32 entry_rva = header->exception_directory_rva
            + (index * PE64_RUNTIME_FUNCTION_BYTES);
        u64 entry_va = actual_base + (u64)entry_rva;
        u32 begin_rva;
        u32 end_rva;
        u32 unwind_rva;

        if ((entry_rva < header->exception_directory_rva)
            || (entry_va < actual_base)
            || (pe64_mapped_rva_range(
                pid,
                sections,
                section_count,
                actual_base,
                entry_rva,
                PE64_RUNTIME_FUNCTION_BYTES,
                VMA64_PROT_READ) == 0u))
        {
            pe64_set_exception_error(out_result, PE64_ERROR_EXCEPTION_DIRECTORY);
            return PE64_DENIED;
        }

        begin_rva = pe64_read_user_le32(entry_va);
        end_rva = pe64_read_user_le32(entry_va + 4ull);
        unwind_rva = pe64_read_user_le32(entry_va + 8ull);

        if ((begin_rva == 0u)
            || (end_rva <= begin_rva)
            || (begin_rva >= header->size_of_image)
            || (end_rva > header->size_of_image)
            || (pe64_mapped_rva_range(
                pid,
                sections,
                section_count,
                actual_base,
                begin_rva,
                1u,
                VMA64_PROT_EXECUTE) == 0u)
            || (pe64_mapped_rva_range(
                pid,
                sections,
                section_count,
                actual_base,
                end_rva - 1u,
                1u,
                VMA64_PROT_EXECUTE) == 0u))
        {
            pe64_set_exception_error(out_result, PE64_ERROR_EXCEPTION_ENTRY_RANGE);
            return PE64_DENIED;
        }
        if ((unwind_rva == 0u)
            || (unwind_rva >= header->size_of_image)
            || (pe64_mapped_rva_range(
                pid,
                sections,
                section_count,
                actual_base,
                unwind_rva,
                1u,
                VMA64_PROT_READ) == 0u))
        {
            pe64_set_exception_error(out_result, PE64_ERROR_EXCEPTION_UNWIND_RANGE);
            return PE64_DENIED;
        }

        if (index == 0u)
        {
            out_result->first_begin_rva = begin_rva;
            out_result->first_unwind_rva = unwind_rva;
        }
        out_result->last_end_rva = end_rva;
        out_result->last_unwind_rva = unwind_rva;
        out_result->table_checksum =
            pe64_checksum_step(out_result->table_checksum, begin_rva);
        out_result->table_checksum =
            pe64_checksum_step(out_result->table_checksum, end_rva);
        out_result->table_checksum =
            pe64_checksum_step(out_result->table_checksum, unwind_rva);
        ++out_result->registered_count;
    }

    context->windows_exception_table_base = out_result->table_base;
    context->windows_exception_table_bytes = out_result->table_bytes;
    context->windows_exception_function_count = out_result->registered_count;
    context->windows_exception_table_checksum = out_result->table_checksum;
    out_result->persona_stored = 1u;

    pe64_set_exception_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_setup_teb(
    u32 pid,
    u64 teb_base,
    u64 stack_base,
    u64 stack_limit,
    u64 tls_pointer,
    pe64_teb_result_t *out_result)
{
    persona_context_t *context;
    u64 effective_teb;
    u64 peb_base;
    u64 effective_tls;
    vma_region_t *region;

    pe64_clear_teb_result(out_result);

    if ((pid == PROCESS64_INVALID_PID) || (out_result == 0))
    {
        pe64_set_teb_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        pe64_set_teb_error(out_result, PE64_ERROR_TEB_PERSONA);
        return PE64_DENIED;
    }

    effective_teb = (teb_base != 0ull) ? teb_base : PE64_TEB_DEFAULT_BASE;
    if (((effective_teb & ((u64)PE64_TEB_PAGE_BYTES - 1ull)) != 0ull)
        || (effective_teb >= 0x0000800000000000ull)
        || ((effective_teb + (u64)PE64_TEB_PAGE_BYTES) <= effective_teb))
    {
        pe64_set_teb_error(out_result, PE64_ERROR_TEB_ADDRESS);
        return PE64_DENIED;
    }

    if ((stack_base == 0ull)
        || (stack_limit == 0ull)
        || (stack_base <= stack_limit)
        || (stack_base > 0x0000800000000000ull)
        || (stack_limit >= 0x0000800000000000ull))
    {
        pe64_set_teb_error(out_result, PE64_ERROR_TEB_STACK);
        return PE64_DENIED;
    }

    effective_tls = (tls_pointer != 0ull)
        ? tls_pointer
        : (effective_teb + (u64)PE64_TEB_TLS_POINTER_OFFSET);
    peb_base = effective_teb + (u64)PE64_TEB_PEB_OFFSET;
    if ((effective_tls < effective_teb)
        || (effective_tls >= (effective_teb + (u64)PE64_TEB_PAGE_BYTES))
        || (peb_base < effective_teb)
        || (peb_base >= (effective_teb + (u64)PE64_TEB_PAGE_BYTES)))
    {
        pe64_set_teb_error(out_result, PE64_ERROR_TEB_ADDRESS);
        return PE64_DENIED;
    }

    if (vma64_map_anon(
            pid,
            effective_teb,
            PE64_TEB_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS)
        != effective_teb)
    {
        pe64_set_teb_error(out_result, PE64_ERROR_TEB_MAP);
        return PE64_DENIED;
    }

    region = vma64_find(pid, effective_teb);
    if ((region == 0)
        || (region->virt_base != effective_teb)
        || (region->virt_end != (effective_teb + (u64)PE64_TEB_PAGE_BYTES))
        || ((region->prot_flags & (VMA64_PROT_READ | VMA64_PROT_WRITE))
            != (VMA64_PROT_READ | VMA64_PROT_WRITE)))
    {
        (void)vma64_unmap(pid, effective_teb, PE64_TEB_PAGE_BYTES);
        pe64_set_teb_error(out_result, PE64_ERROR_TEB_MAP);
        return PE64_DENIED;
    }

    (void)pe64_zero_user(effective_teb, PE64_TEB_PAGE_BYTES);
    pe64_write_user_le64(
        effective_teb + (u64)PE64_TEB_OFFSET_EXCEPTION_LIST,
        PE64_TEB_CHAIN_END);
    pe64_write_user_le64(
        effective_teb + (u64)PE64_TEB_OFFSET_STACK_BASE,
        stack_base);
    pe64_write_user_le64(
        effective_teb + (u64)PE64_TEB_OFFSET_STACK_LIMIT,
        stack_limit);
    pe64_write_user_le64(
        effective_teb + (u64)PE64_TEB_OFFSET_SELF,
        effective_teb);
    pe64_write_user_le64(
        effective_teb + (u64)PE64_TEB_OFFSET_TLS_POINTER,
        effective_tls);
    pe64_write_user_le64(
        effective_teb + (u64)PE64_TEB_OFFSET_PEB,
        peb_base);

    out_result->gs_base_before = read_gs_base64();
    write_gs_base64(effective_teb);
    out_result->gs_base_after = read_gs_base64();

    if (out_result->gs_base_after != effective_teb)
    {
        (void)vma64_unmap(pid, effective_teb, PE64_TEB_PAGE_BYTES);
        pe64_set_teb_error(out_result, PE64_ERROR_TEB_MAP);
        return PE64_DENIED;
    }

    context->tls_base = effective_teb;
    context->tls_size = (u64)PE64_TEB_PAGE_BYTES;
    context->windows_teb_base = effective_teb;
    context->windows_peb_base = peb_base;
    context->windows_stack_base = stack_base;
    context->windows_stack_limit = stack_limit;
    context->windows_tls_pointer = effective_tls;

    out_result->teb_base = effective_teb;
    out_result->peb_base = peb_base;
    out_result->stack_base = stack_base;
    out_result->stack_limit = stack_limit;
    out_result->tls_pointer = effective_tls;
    out_result->exception_list_value =
        pe64_read_user_le64(effective_teb + (u64)PE64_TEB_OFFSET_EXCEPTION_LIST);
    out_result->stack_base_value =
        pe64_read_user_le64(effective_teb + (u64)PE64_TEB_OFFSET_STACK_BASE);
    out_result->stack_limit_value =
        pe64_read_user_le64(effective_teb + (u64)PE64_TEB_OFFSET_STACK_LIMIT);
    out_result->self_value =
        pe64_read_user_le64(effective_teb + (u64)PE64_TEB_OFFSET_SELF);
    out_result->tls_pointer_value =
        pe64_read_user_le64(effective_teb + (u64)PE64_TEB_OFFSET_TLS_POINTER);
    out_result->peb_value =
        pe64_read_user_le64(effective_teb + (u64)PE64_TEB_OFFSET_PEB);
    out_result->page_bytes = (u64)PE64_TEB_PAGE_BYTES;
    out_result->page_protection = region->prot_flags;
    out_result->page_checksum = pe64_checksum_user_bytes(effective_teb, PE64_TEB_PAGE_BYTES);
    out_result->context_stored = 1u;
    pe64_set_teb_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_setup_peb(
    u32 pid,
    u64 image_base,
    const char *image_path,
    const char *command_line,
    const char *environment,
    pe64_peb_result_t *out_result)
{
    persona_context_t *context;
    vma_region_t *region;
    u64 teb_base;
    u64 teb_end;
    u64 peb_base;
    u64 process_parameters;
    u64 image_path_buffer;
    u64 command_line_buffer;
    u64 environment_buffer;
    u64 image_path_descriptor;
    u64 command_line_descriptor;
    u64 environment_descriptor;

    pe64_clear_peb_result(out_result);

    if ((pid == PROCESS64_INVALID_PID) || (out_result == 0))
    {
        pe64_set_peb_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        pe64_set_peb_error(out_result, PE64_ERROR_PEB_PERSONA);
        return PE64_DENIED;
    }

    teb_base = context->windows_teb_base;
    peb_base = context->windows_peb_base;
    teb_end = teb_base + (u64)PE64_TEB_PAGE_BYTES;
    if ((teb_base == 0ull)
        || (peb_base == 0ull)
        || (teb_end <= teb_base)
        || (peb_base < teb_base)
        || ((peb_base + (u64)PE64_PEB_OFFSET_OS_BUILD + 4ull) > teb_end))
    {
        pe64_set_peb_error(out_result, PE64_ERROR_PEB_TEB);
        return PE64_DENIED;
    }

    process_parameters = teb_base + (u64)PE64_TEB_PROCESS_PARAMETERS_OFFSET;
    image_path_buffer = teb_base + (u64)PE64_TEB_IMAGE_PATH_BUFFER_OFFSET;
    command_line_buffer = teb_base + (u64)PE64_TEB_COMMAND_LINE_BUFFER_OFFSET;
    environment_buffer = teb_base + (u64)PE64_TEB_ENVIRONMENT_BUFFER_OFFSET;
    image_path_descriptor =
        process_parameters + (u64)PE64_PROCESS_PARAMETERS_OFFSET_IMAGE_PATH_NAME;
    command_line_descriptor =
        process_parameters + (u64)PE64_PROCESS_PARAMETERS_OFFSET_COMMAND_LINE;
    environment_descriptor =
        process_parameters + (u64)PE64_PROCESS_PARAMETERS_OFFSET_ENVIRONMENT;

    if ((process_parameters < teb_base)
        || ((process_parameters + 0x100ull) > teb_end)
        || (image_path_buffer < teb_base)
        || ((image_path_buffer + (u64)PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES) > teb_end)
        || (command_line_buffer < teb_base)
        || ((command_line_buffer + (u64)PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES) > teb_end)
        || (environment_buffer < teb_base)
        || ((environment_buffer + (u64)PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES) > teb_end)
        || ((image_path_descriptor + (u64)PE64_PROCESS_PARAMETERS_UNICODE_STRING_BYTES)
            > teb_end)
        || ((command_line_descriptor + (u64)PE64_PROCESS_PARAMETERS_UNICODE_STRING_BYTES)
            > teb_end)
        || ((environment_descriptor + (u64)PE64_PROCESS_PARAMETERS_UNICODE_STRING_BYTES)
            > teb_end))
    {
        pe64_set_peb_error(out_result, PE64_ERROR_PEB_TEB);
        return PE64_DENIED;
    }

    if ((image_base == 0ull)
        || ((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (image_base >= 0x0000800000000000ull))
    {
        pe64_set_peb_error(out_result, PE64_ERROR_PEB_IMAGE_BASE);
        return PE64_DENIED;
    }

    region = vma64_find(pid, teb_base);
    if ((region == 0)
        || (paging64_user_page_present(teb_base) == 0u)
        || (region->virt_base != teb_base)
        || (region->virt_end != teb_end)
        || ((region->prot_flags & (VMA64_PROT_READ | VMA64_PROT_WRITE))
            != (VMA64_PROT_READ | VMA64_PROT_WRITE)))
    {
        pe64_set_peb_error(out_result, PE64_ERROR_PEB_MAP);
        return PE64_DENIED;
    }

    if ((pe64_utf16le_string_fits(
            image_path,
            PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES) == 0u)
        || (pe64_utf16le_string_fits(
            command_line,
            PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES) == 0u)
        || (pe64_utf16le_string_fits(
            environment,
            PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES) == 0u))
    {
        pe64_set_peb_error(out_result, PE64_ERROR_PEB_STRING);
        return PE64_DENIED;
    }

    (void)pe64_zero_user(peb_base, teb_end - peb_base);

    pe64_write_user_le64(
        peb_base + (u64)PE64_PEB_OFFSET_IMAGE_BASE_ADDRESS,
        image_base);
    pe64_write_user_le64(
        peb_base + (u64)PE64_PEB_OFFSET_PROCESS_PARAMETERS,
        process_parameters);
    pe64_write_user_le32(
        peb_base + (u64)PE64_PEB_OFFSET_NT_GLOBAL_FLAG,
        0u);
    pe64_write_user_le32(
        peb_base + (u64)PE64_PEB_OFFSET_OS_MAJOR,
        PE64_PEB_OS_MAJOR);
    pe64_write_user_le32(
        peb_base + (u64)PE64_PEB_OFFSET_OS_MINOR,
        PE64_PEB_OS_MINOR);
    pe64_write_user_le32(
        peb_base + (u64)PE64_PEB_OFFSET_OS_BUILD,
        PE64_PEB_OS_BUILD);

    pe64_write_utf16le_string(
        image_path_buffer,
        image_path,
        PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES,
        &out_result->image_path_bytes,
        &out_result->image_path_checksum);
    pe64_write_utf16le_string(
        command_line_buffer,
        command_line,
        PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES,
        &out_result->command_line_bytes,
        &out_result->command_line_checksum);
    pe64_write_utf16le_string(
        environment_buffer,
        environment,
        PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES,
        &out_result->environment_bytes,
        &out_result->environment_checksum);
    pe64_write_unicode_descriptor(
        image_path_descriptor,
        image_path_buffer,
        out_result->image_path_bytes);
    pe64_write_unicode_descriptor(
        command_line_descriptor,
        command_line_buffer,
        out_result->command_line_bytes);
    pe64_write_unicode_descriptor(
        environment_descriptor,
        environment_buffer,
        out_result->environment_bytes);

    context->windows_image_base = image_base;
    context->windows_process_parameters = process_parameters;
    context->windows_os_major = PE64_PEB_OS_MAJOR;
    context->windows_os_minor = PE64_PEB_OS_MINOR;
    context->windows_os_build = PE64_PEB_OS_BUILD;
    context->windows_nt_global_flag = 0u;

    out_result->peb_base = peb_base;
    out_result->process_parameters = process_parameters;
    out_result->image_base = image_base;
    out_result->image_base_value =
        pe64_read_user_le64(peb_base + (u64)PE64_PEB_OFFSET_IMAGE_BASE_ADDRESS);
    out_result->process_parameters_value =
        pe64_read_user_le64(peb_base + (u64)PE64_PEB_OFFSET_PROCESS_PARAMETERS);
    out_result->image_path_buffer = image_path_buffer;
    out_result->command_line_buffer = command_line_buffer;
    out_result->environment_buffer = environment_buffer;
    out_result->os_major = PE64_PEB_OS_MAJOR;
    out_result->os_minor = PE64_PEB_OS_MINOR;
    out_result->os_build = PE64_PEB_OS_BUILD;
    out_result->nt_global_flag = 0u;
    out_result->os_major_value =
        pe64_read_user_le32(peb_base + (u64)PE64_PEB_OFFSET_OS_MAJOR);
    out_result->os_minor_value =
        pe64_read_user_le32(peb_base + (u64)PE64_PEB_OFFSET_OS_MINOR);
    out_result->os_build_value =
        pe64_read_user_le32(peb_base + (u64)PE64_PEB_OFFSET_OS_BUILD);
    out_result->nt_global_flag_value =
        pe64_read_user_le32(peb_base + (u64)PE64_PEB_OFFSET_NT_GLOBAL_FLAG);
    out_result->context_stored =
        ((context->windows_image_base == image_base)
            && (context->windows_process_parameters == process_parameters)
            && (context->windows_os_major == PE64_PEB_OS_MAJOR)
            && (context->windows_os_minor == PE64_PEB_OS_MINOR)
            && (context->windows_os_build == PE64_PEB_OS_BUILD)
            && (context->windows_nt_global_flag == 0u))
            ? 1u
            : 0u;

    pe64_set_peb_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_setup_kuser_shared_data(u32 pid, pe64_kuser_result_t *out_result)
{
    persona_context_t *context;
    vma_region_t *region;
    u64 mapped;
    u64 system_time;
    u64 tick_count;

    pe64_clear_kuser_result(out_result);

    if ((pid == PROCESS64_INVALID_PID) || (out_result == 0))
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_PERSONA);
        return PE64_DENIED;
    }

    mapped = vma64_map_anon(
        pid,
        PE64_KUSER_SHARED_DATA_BASE,
        PE64_KUSER_SHARED_DATA_BYTES,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS);
    if (mapped != PE64_KUSER_SHARED_DATA_BASE)
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_MAP);
        return PE64_DENIED;
    }

    region = vma64_find(pid, PE64_KUSER_SHARED_DATA_BASE);
    if ((region == 0)
        || (region->virt_base != PE64_KUSER_SHARED_DATA_BASE)
        || (region->virt_end != (PE64_KUSER_SHARED_DATA_BASE
            + (u64)PE64_KUSER_SHARED_DATA_BYTES))
        || (paging64_user_page_present(PE64_KUSER_SHARED_DATA_BASE) == 0u))
    {
        (void)vma64_unmap(pid, PE64_KUSER_SHARED_DATA_BASE, PE64_KUSER_SHARED_DATA_BYTES);
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_MAP);
        return PE64_DENIED;
    }

    (void)pe64_zero_user(PE64_KUSER_SHARED_DATA_BASE, PE64_KUSER_SHARED_DATA_BYTES);
    if (pe64_write_utf16le_string(
            PE64_KUSER_SHARED_DATA_BASE + (u64)PE64_KUSER_OFFSET_NT_SYSTEM_ROOT,
            "\\SystemRoot",
            PE64_KUSER_SYSTEM_ROOT_MAX_BYTES,
            &out_result->system_root_bytes,
            0) == 0u)
    {
        (void)vma64_unmap(pid, PE64_KUSER_SHARED_DATA_BASE, PE64_KUSER_SHARED_DATA_BYTES);
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_STRING);
        return PE64_DENIED;
    }

    pe64_write_user_le32(
        PE64_KUSER_SHARED_DATA_BASE + (u64)PE64_KUSER_OFFSET_NT_PRODUCT_TYPE,
        PE64_KUSER_NT_PRODUCT_WORKSTATION);
    system_time = pe64_kuser_system_time_100ns();
    tick_count = (u64)pit_get_ticks();
    pe64_kuser_write_time_fields(
        PE64_KUSER_SHARED_DATA_BASE,
        system_time,
        tick_count);

    if (vma64_protect(
            pid,
            PE64_KUSER_SHARED_DATA_BASE,
            PE64_KUSER_SHARED_DATA_BYTES,
            VMA64_PROT_READ) == 0u)
    {
        (void)vma64_unmap(pid, PE64_KUSER_SHARED_DATA_BASE, PE64_KUSER_SHARED_DATA_BYTES);
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_PROTECT);
        return PE64_DENIED;
    }

    context->windows_kuser_shared_data_base = PE64_KUSER_SHARED_DATA_BASE;
    context->windows_kuser_shared_data_updates = 1u;
    context->windows_kuser_shared_data_checksum = pe64_checksum_user_bytes(
        PE64_KUSER_SHARED_DATA_BASE,
        PE64_KUSER_SHARED_DATA_BYTES);

    if (pe64_kuser_register_pid(pid) == 0u)
    {
        context->windows_kuser_shared_data_base = 0ull;
        context->windows_kuser_shared_data_updates = 0u;
        context->windows_kuser_shared_data_checksum = 0u;
        (void)vma64_unmap(pid, PE64_KUSER_SHARED_DATA_BASE, PE64_KUSER_SHARED_DATA_BYTES);
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_MAP);
        return PE64_DENIED;
    }

    pe64_kuser_fill_result(out_result, PE64_KUSER_SHARED_DATA_BASE, context);
    return PE64_OK;
}

u32 pe64_refresh_kuser_shared_data(u32 pid, pe64_kuser_result_t *out_result)
{
    persona_context_t *context;
    u64 system_time;
    u64 tick_count;

    pe64_clear_kuser_result(out_result);

    if ((pid == PROCESS64_INVALID_PID) || (out_result == 0))
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0)
        || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
        || (context->windows_kuser_shared_data_base != PE64_KUSER_SHARED_DATA_BASE))
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_PERSONA);
        return PE64_DENIED;
    }

    if ((vma64_find(pid, PE64_KUSER_SHARED_DATA_BASE) == 0)
        || (paging64_user_page_present(PE64_KUSER_SHARED_DATA_BASE) == 0u)
        || (paging64_user_page_protection(PE64_KUSER_SHARED_DATA_BASE)
            != VMA64_PROT_READ))
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_MAP);
        return PE64_DENIED;
    }

    if (vma64_protect(
            pid,
            PE64_KUSER_SHARED_DATA_BASE,
            PE64_KUSER_SHARED_DATA_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE) == 0u)
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_PROTECT);
        return PE64_DENIED;
    }
    system_time = pe64_kuser_system_time_100ns();
    tick_count = (u64)pit_get_ticks();
    pe64_kuser_write_time_fields(
        PE64_KUSER_SHARED_DATA_BASE,
        system_time,
        tick_count);
    if (vma64_protect(
            pid,
            PE64_KUSER_SHARED_DATA_BASE,
            PE64_KUSER_SHARED_DATA_BYTES,
            VMA64_PROT_READ) == 0u)
    {
        pe64_set_kuser_error(out_result, PE64_ERROR_KUSER_PROTECT);
        return PE64_DENIED;
    }
    ++g_pe64_kuser_update_sequence;
    context->windows_kuser_shared_data_updates = g_pe64_kuser_update_sequence + 1u;
    context->windows_kuser_shared_data_checksum = pe64_checksum_user_bytes(
        PE64_KUSER_SHARED_DATA_BASE,
        PE64_KUSER_SHARED_DATA_BYTES);

    pe64_kuser_fill_result(out_result, PE64_KUSER_SHARED_DATA_BASE, context);
    return PE64_OK;
}

u32 pe64_initialize_security_cookie(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    pe64_security_cookie_result_t *out_result)
{
    persona_context_t *context;
    u64 load_config_base;
    u64 security_cookie_field;
    u64 security_cookie_address;
    u64 cookie_value;
    u32 cookie_rva;

    pe64_clear_security_cookie_result(out_result);

    if ((pid == PROCESS64_INVALID_PID)
        || (header == 0)
        || (sections == 0)
        || (section_count == 0u)
        || (actual_base == 0ull)
        || (out_result == 0))
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_PERSONA);
        return PE64_DENIED;
    }

    if (header->error != PE64_ERROR_NONE)
    {
        pe64_set_security_cookie_error(out_result, header->error);
        return PE64_DENIED;
    }

    out_result->load_config_directory_rva = header->load_config_directory_rva;
    out_result->load_config_directory_bytes = header->load_config_directory_size;

    if ((header->load_config_directory_rva == 0u)
        || (header->load_config_directory_size == 0u))
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_NONE);
        return PE64_OK;
    }

    if (header->load_config_directory_size < PE64_LOAD_CONFIG_SECURITY_COOKIE_MIN_BYTES)
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_LOAD_CONFIG_DIRECTORY);
        return PE64_DENIED;
    }

    if (pe64_mapped_rva_range(
            pid,
            sections,
            section_count,
            actual_base,
            header->load_config_directory_rva,
            header->load_config_directory_size,
            VMA64_PROT_READ) == 0u)
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_LOAD_CONFIG_DIRECTORY);
        return PE64_DENIED;
    }

    load_config_base = actual_base + (u64)header->load_config_directory_rva;
    security_cookie_field =
        load_config_base + (u64)PE64_LOAD_CONFIG_SECURITY_COOKIE_OFFSET;
    if ((load_config_base < actual_base)
        || (security_cookie_field < load_config_base)
        || ((security_cookie_field + 8ull) < security_cookie_field))
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_FIELD);
        return PE64_DENIED;
    }

    out_result->load_config_base = load_config_base;
    out_result->security_cookie_field = security_cookie_field;
    security_cookie_address = pe64_read_user_le64(security_cookie_field);
    out_result->security_cookie_address = security_cookie_address;

    if ((security_cookie_address == 0ull)
        || (pe64_va_to_rva(actual_base, security_cookie_address, &cookie_rva) == 0u)
        || (pe64_mapped_rva_range(
            pid,
            sections,
            section_count,
            actual_base,
            cookie_rva,
            8u,
            VMA64_PROT_READ) == 0u))
    {
        pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_FIELD);
        return PE64_DENIED;
    }

    out_result->cookie_before = pe64_read_user_le64(security_cookie_address);
    cookie_value = pe64_security_cookie_next(pid, actual_base, security_cookie_address);
    out_result->cookie_value = cookie_value;
    if (pe64_write_cookie_u64_to_va(
            pid,
            security_cookie_address,
            cookie_value,
            out_result) == PE64_DENIED)
    {
        if (out_result->error == PE64_ERROR_NONE)
        {
            pe64_set_security_cookie_error(out_result, PE64_ERROR_SECURITY_COOKIE_WRITE);
        }
        return PE64_DENIED;
    }

    out_result->cookie_after = pe64_read_user_le64(security_cookie_address);
    out_result->cookie_checksum =
        pe64_checksum_u64(2166136261u, out_result->cookie_after);
    out_result->page_protection = paging64_user_page_protection(security_cookie_address);

    context->windows_security_cookie_address = security_cookie_address;
    context->windows_security_cookie_value = out_result->cookie_after;
    context->windows_security_cookie_checksum = out_result->cookie_checksum;
    out_result->context_stored =
        ((context->windows_security_cookie_address == security_cookie_address)
            && (context->windows_security_cookie_value == out_result->cookie_after)
            && (context->windows_security_cookie_checksum == out_result->cookie_checksum))
            ? 1u
            : 0u;

    pe64_set_security_cookie_error(out_result, PE64_ERROR_NONE);
    return PE64_OK;
}

u32 pe64_launch_entry(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u64 stack_base,
    u64 stack_bytes,
    u64 ldr_initialize_thunk,
    u32 run_transfer_probe,
    pe64_entry_result_t *out_result)
{
    persona_context_t *context;
    u64 entry_rip;
    u64 stack_top;
    u64 initial_rsp;
    u64 mapped_stack;
    u32 entry_rva;
    u32 dll_entry;

    pe64_clear_entry_result(out_result);

    if ((pid == PROCESS64_INVALID_PID)
        || (header == 0)
        || (sections == 0)
        || (section_count == 0u)
        || (actual_base == 0ull)
        || (stack_base == 0ull)
        || (stack_bytes == 0ull)
        || (out_result == 0))
    {
        pe64_set_entry_error(out_result, PE64_ERROR_NULL);
        return PE64_DENIED;
    }

    context = persona64_context_for_process(pid);
    if ((context == 0) || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE))
    {
        pe64_set_entry_error(out_result, PE64_ERROR_ENTRY_PERSONA);
        return PE64_DENIED;
    }

    if (header->error != PE64_ERROR_NONE)
    {
        pe64_set_entry_error(out_result, header->error);
        return PE64_DENIED;
    }

    dll_entry = ((header->characteristics & PE64_IMAGE_FILE_DLL) != 0u) ? 1u : 0u;
    if ((dll_entry == 0u) && (ldr_initialize_thunk == 0ull))
    {
        pe64_set_entry_error(out_result, PE64_ERROR_ENTRY_NTDLL_UNAVAILABLE);
        return PE64_DENIED;
    }

    entry_rip = actual_base + (u64)header->address_of_entry_point;
    if ((entry_rip < actual_base)
        || (pe64_va_to_rva(actual_base, entry_rip, &entry_rva) == 0u)
        || (pe64_mapped_rva_range(
            pid,
            sections,
            section_count,
            actual_base,
            entry_rva,
            1u,
            VMA64_PROT_EXECUTE) == 0u))
    {
        pe64_set_entry_error(out_result, PE64_ERROR_ENTRY_ADDRESS);
        return PE64_DENIED;
    }

    if (((stack_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_bytes & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_base + stack_bytes) <= stack_base)
        || (stack_bytes <= PE64_ENTRY_SHADOW_SPACE_BYTES))
    {
        pe64_set_entry_error(out_result, PE64_ERROR_ENTRY_STACK);
        return PE64_DENIED;
    }

    mapped_stack = vma64_map_anon(
        pid,
        stack_base,
        stack_bytes,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS);
    if (mapped_stack != stack_base)
    {
        pe64_set_entry_error(out_result, PE64_ERROR_ENTRY_STACK);
        return PE64_DENIED;
    }

    stack_top = stack_base + stack_bytes;
    initial_rsp = (stack_top - (u64)PE64_ENTRY_SHADOW_SPACE_BYTES) & ~0xFull;

    out_result->entry_rip = entry_rip;
    out_result->transfer_rip = (dll_entry != 0u) ? entry_rip : ldr_initialize_thunk;
    out_result->stack_base = stack_base;
    out_result->stack_top = stack_top;
    out_result->initial_rsp = initial_rsp;
    out_result->arg_rcx = (u64)PE64_TLS_REASON_DLL_PROCESS_ATTACH;
    out_result->arg_rdx = actual_base;
    out_result->arg_r8 = 0ull;
    out_result->ldr_initialize_thunk = ldr_initialize_thunk;
    out_result->transfer_selectors =
        ((u32)DESCRIPTORS64_USER_DATA_SELECTOR << 16)
            | (u32)DESCRIPTORS64_USER_CODE_SELECTOR;
    out_result->transfer_rflags = LAUNCH64_USER_RFLAGS;
    out_result->dll_entry = dll_entry;
    out_result->transfer_ready = pe64_entry_transfer_ready(
        pid,
        out_result->transfer_rip,
        stack_base,
        initial_rsp,
        out_result);
    if (out_result->transfer_ready == 0u)
    {
        (void)vma64_unmap(pid, stack_base, stack_bytes);
        pe64_set_entry_error(out_result, PE64_ERROR_ENTRY_TRANSFER);
        return PE64_DENIED;
    }

    context->windows_entry_rip = out_result->transfer_rip;
    context->windows_entry_rsp = initial_rsp;
    context->windows_entry_arg_rcx = out_result->arg_rcx;
    context->windows_entry_arg_rdx = out_result->arg_rdx;
    context->windows_entry_arg_r8 = out_result->arg_r8;
    context->windows_entry_transfer_ready = out_result->transfer_ready;
    out_result->context_stored =
        ((context->windows_entry_rip == out_result->transfer_rip)
            && (context->windows_entry_rsp == initial_rsp)
            && (context->windows_entry_arg_rcx == out_result->arg_rcx)
            && (context->windows_entry_arg_rdx == out_result->arg_rdx)
            && (context->windows_entry_arg_r8 == out_result->arg_r8)
            && (context->windows_entry_transfer_ready != 0u))
            ? 1u
            : 0u;

    if (run_transfer_probe != 0u)
    {
        out_result->transfer_result = interrupts64_trigger_user_entry_probe_args(
            out_result->transfer_rip,
            initial_rsp,
            (u64)out_result->transfer_selectors,
            (u64)out_result->transfer_rflags,
            out_result->arg_rcx,
            out_result->arg_rdx,
            out_result->arg_r8,
            &out_result->transfer_aux);
        out_result->transfer_executed = 1u;
        if (out_result->transfer_result == 0u)
        {
            (void)vma64_unmap(pid, stack_base, stack_bytes);
            pe64_set_entry_error(out_result, PE64_ERROR_ENTRY_TRANSFER);
            return PE64_DENIED;
        }
    }

    out_result->error = PE64_ERROR_NONE;
    return PE64_OK;
}

void pe64_kuser_tick_update_all(void)
{
    u32 index;

    for (index = 0u; index < PE64_KUSER_REGISTERED_MAX; ++index)
    {
        u32 pid = g_pe64_kuser_registered_pids[index];
        persona_context_t *context;
        u64 system_time;
        u64 tick_count;

        if (pid == 0u)
        {
            continue;
        }

        context = persona64_context_for_process(pid);
        if ((context == 0)
            || (context->persona_type != PERSONA64_TYPE_WINDOWS_PE)
            || (context->windows_kuser_shared_data_base != PE64_KUSER_SHARED_DATA_BASE)
            || (vma64_find(pid, PE64_KUSER_SHARED_DATA_BASE) == 0)
            || (paging64_user_page_present(PE64_KUSER_SHARED_DATA_BASE) == 0u)
            || (paging64_user_page_protection(PE64_KUSER_SHARED_DATA_BASE)
                != VMA64_PROT_READ))
        {
            g_pe64_kuser_registered_pids[index] = 0u;
            continue;
        }

        if (vma64_protect(
                pid,
                PE64_KUSER_SHARED_DATA_BASE,
                PE64_KUSER_SHARED_DATA_BYTES,
                VMA64_PROT_READ | VMA64_PROT_WRITE) == 0u)
        {
            g_pe64_kuser_registered_pids[index] = 0u;
            continue;
        }
        system_time = pe64_kuser_system_time_100ns();
        tick_count = (u64)pit_get_ticks();
        pe64_kuser_write_time_fields(
            PE64_KUSER_SHARED_DATA_BASE,
            system_time,
            tick_count);
        if (vma64_protect(
                pid,
                PE64_KUSER_SHARED_DATA_BASE,
                PE64_KUSER_SHARED_DATA_BYTES,
                VMA64_PROT_READ) == 0u)
        {
            g_pe64_kuser_registered_pids[index] = 0u;
            continue;
        }
        ++g_pe64_kuser_update_sequence;
        context->windows_kuser_shared_data_updates = g_pe64_kuser_update_sequence + 1u;
        context->windows_kuser_shared_data_checksum = pe64_checksum_user_bytes(
            PE64_KUSER_SHARED_DATA_BASE,
            PE64_KUSER_SHARED_DATA_BYTES);
    }
}
