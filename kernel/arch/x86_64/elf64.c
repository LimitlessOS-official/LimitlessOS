#include "elf64_x64.h"
#include "descriptors_x64.h"
#include "interrupts_x64.h"
#include "launch_x64.h"
#include "linux_vdso_x64.h"
#include "paging_x64.h"
#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * E.1-E.7 add the first LimitlessOS-owned ELF64 metadata parser, PT_LOAD
 * mapper, GNU_RELRO enforcement, Linux auxiliary-vector builder, initial stack
 * builder, and static launch planner. H.2 extends that Linux auxv path by
 * mapping the LimitlessOS VDSO page through linux_vdso_x64.h and publishing its
 * ELF header address with AT_SYSINFO_EHDR for Linux persona processes only. It
 * integrates with elf64_x64.h, process_x64.h, pit.h, paging_x64.h,
 * interrupts_x64.h, launch_x64.h, persona_x64.h, linux_vdso_x64.h, and the VMA
 * subsystem in vma_x64.h. The scaffold checkpoint proves valid x86-64 ELF
 * headers and program headers are accepted, load segments are mapped with final
 * R/W/X permissions, BSS tails are zero, RELRO pages lose write permission,
 * auxv contains the System V startup records, 16 runtime-derived random bytes,
 * and for Linux persona launches the VDSO AT_SYSINFO_EHDR pointer, the initial
 * stack lays out argc/argv/envp/auxv with rewritten AT_RANDOM/AT_PLATFORM
 * pointers, E.7 prepares a truthful ring-3 RIP/RSP transfer frame for a static
 * ELF, and malformed inputs fail with specific denial codes.
 */

static void elf64_clear_header(elf64_header_t *header)
{
    if (header == 0)
    {
        return;
    }

    header->type = 0u;
    header->machine = 0u;
    header->version = 0u;
    header->entry = 0ull;
    header->phoff = 0ull;
    header->shoff = 0ull;
    header->flags = 0u;
    header->ehsize = 0u;
    header->phentsize = 0u;
    header->phnum = 0u;
    header->shentsize = 0u;
    header->shnum = 0u;
    header->shstrndx = 0u;
    header->osabi = 0u;
    header->error = ELF64_ERROR_NONE;
}

static void elf64_clear_program_header(elf64_program_header_t *phdr)
{
    if (phdr == 0)
    {
        return;
    }

    phdr->type = 0u;
    phdr->flags = 0u;
    phdr->offset = 0ull;
    phdr->vaddr = 0ull;
    phdr->paddr = 0ull;
    phdr->filesz = 0ull;
    phdr->memsz = 0ull;
    phdr->align = 0ull;
}

static void elf64_clear_summary(elf64_phdr_summary_t *summary)
{
    if (summary == 0)
    {
        return;
    }

    summary->load_count = 0u;
    summary->interp_count = 0u;
    summary->gnu_stack_count = 0u;
    summary->gnu_relro_count = 0u;
    summary->tls_count = 0u;
    summary->dynamic_count = 0u;
    summary->executable_loads = 0u;
    summary->writable_loads = 0u;
    summary->load_mem_bytes = 0ull;
    summary->first_load_vaddr = 0ull;
    summary->max_load_end = 0ull;
    summary->error = ELF64_ERROR_NONE;
}

static void elf64_clear_load_result(elf64_load_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->mapped_count = 0u;
    result->reserved = 0u;
    result->total_map_bytes = 0ull;
    result->total_file_bytes = 0ull;
    result->total_bss_bytes = 0ull;
    result->first_mapped_vaddr = 0ull;
    result->max_mapped_end = 0ull;
    result->source_checksum = 0u;
    result->mapped_checksum = 0u;
    result->bss_nonzero_count = 0u;
    result->error = ELF64_ERROR_NONE;
}

static void elf64_clear_relro_result(elf64_relro_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->relro_count = 0u;
    result->protected_count = 0u;
    result->total_protected_bytes = 0ull;
    result->first_protected_vaddr = 0ull;
    result->max_protected_end = 0ull;
    result->error = ELF64_ERROR_NONE;
}

static void elf64_clear_auxv(elf64_auxv_t *auxv)
{
    u32 index;

    if (auxv == 0)
    {
        return;
    }

    for (index = 0u; index < ELF64_AUXV_MAX_ENTRIES; ++index)
    {
        auxv->entries[index].type = ELF64_AT_NULL;
        auxv->entries[index].value = 0ull;
    }
    for (index = 0u; index < ELF64_AUX_RANDOM_BYTES; ++index)
    {
        auxv->random[index] = 0u;
    }
    for (index = 0u; index < ELF64_AUX_PLATFORM_BYTES; ++index)
    {
        auxv->platform[index] = 0u;
    }

    auxv->entry_count = 0u;
    auxv->random_byte_count = 0u;
    auxv->random_staging_address = 0ull;
    auxv->platform_staging_address = 0ull;
    auxv->random_checksum = 0u;
    auxv->platform_checksum = 0u;
    auxv->error = ELF64_ERROR_NONE;
}

static void elf64_clear_stack_result(elf64_stack_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->stack_base = 0ull;
    result->stack_top = 0ull;
    result->initial_rsp = 0ull;
    result->argc_address = 0ull;
    result->argv_address = 0ull;
    result->envp_address = 0ull;
    result->auxv_address = 0ull;
    result->strings_base = 0ull;
    result->random_address = 0ull;
    result->platform_address = 0ull;
    result->argc = 0u;
    result->envc = 0u;
    result->auxv_entry_count = 0u;
    result->pointer_slot_count = 0u;
    result->string_bytes = 0u;
    result->layout_bytes = 0u;
    result->random_checksum = 0u;
    result->platform_checksum = 0u;
    result->alignment_ok = 0u;
    result->argv_null_ok = 0u;
    result->envp_null_ok = 0u;
    result->auxv_null_ok = 0u;
    result->error = ELF64_ERROR_NONE;
}

static void elf64_clear_launch_result(elf64_launch_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    elf64_clear_header(&result->header);
    elf64_clear_summary(&result->phdr_summary);
    elf64_clear_load_result(&result->load_result);
    elf64_clear_relro_result(&result->relro_result);
    elf64_clear_auxv(&result->auxv);
    elf64_clear_stack_result(&result->stack_result);
    result->load_bias = 0ull;
    result->entry_rip = 0ull;
    result->phdr_vaddr = 0ull;
    result->stack_base = 0ull;
    result->stack_top = 0ull;
    result->initial_rsp = 0ull;
    result->transfer_rip = 0ull;
    result->transfer_rsp = 0ull;
    result->transfer_selectors = 0u;
    result->transfer_ready = 0u;
    result->transfer_executed = 0u;
    result->transfer_result = 0u;
    result->entry_page_present = 0u;
    result->entry_page_prot = 0u;
    result->stack_page_present = 0u;
    result->stack_page_prot = 0u;
    result->error = ELF64_ERROR_NONE;
}

static void elf64_set_header_error(elf64_header_t *header, u32 error)
{
    if (header != 0)
    {
        header->error = error;
    }
}

static void elf64_set_summary_error(elf64_phdr_summary_t *summary, u32 error)
{
    if (summary != 0)
    {
        summary->error = error;
    }
}

static void elf64_set_load_error(elf64_load_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void elf64_set_relro_error(elf64_relro_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void elf64_set_auxv_error(elf64_auxv_t *auxv, u32 error)
{
    if (auxv != 0)
    {
        auxv->error = error;
    }
}

static void elf64_set_stack_error(elf64_stack_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static void elf64_set_launch_error(elf64_launch_result_t *result, u32 error)
{
    if (result != 0)
    {
        result->error = error;
    }
}

static u16 elf64_read_le16(const u8 *data)
{
    return (u16)(((u16)data[0]) | ((u16)data[1] << 8));
}

static u32 elf64_read_le32(const u8 *data)
{
    return ((u32)data[0])
        | ((u32)data[1] << 8)
        | ((u32)data[2] << 16)
        | ((u32)data[3] << 24);
}

static u64 elf64_read_le64(const u8 *data)
{
    return ((u64)elf64_read_le32(data))
        | ((u64)elf64_read_le32(data + 4u) << 32);
}

static u32 elf64_range_available(u32 size, u64 offset, u64 bytes)
{
    if (offset > (u64)size)
    {
        return 0u;
    }

    return (bytes <= ((u64)size - offset)) ? 1u : 0u;
}

static u32 elf64_is_power_of_two(u64 value)
{
    return ((value != 0ull) && ((value & (value - 1ull)) == 0ull)) ? 1u : 0u;
}

static u64 elf64_align_down(u64 value, u64 alignment)
{
    return value & ~(alignment - 1ull);
}

static u64 elf64_align_up(u64 value, u64 alignment)
{
    return (value + alignment - 1ull) & ~(alignment - 1ull);
}

static u32 elf64_mix_checksum(u32 checksum, u8 value)
{
    return ((checksum ^ (u32)value) * 16777619u) + 0x9E3779B9u;
}

static u32 elf64_checksum_bytes(const u8 *data, u32 byte_count)
{
    u32 index;
    u32 checksum = 0u;

    if (data == 0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = elf64_mix_checksum(checksum, data[index]);
    }

    return checksum;
}

static u32 elf64_strlen_bounded(const char *text, u32 max_bytes)
{
    u32 count;

    if (text == 0)
    {
        return 0u;
    }

    for (count = 0u; count < max_bytes; ++count)
    {
        if (text[count] == 0)
        {
            return count + 1u;
        }
    }

    return 0u;
}

static void elf64_stack_write_bytes(u64 address, const u8 *data, u32 byte_count)
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

static void elf64_stack_write_u64(u64 address, u64 value)
{
    volatile u64 *target = (volatile u64 *)(u64)address;

    if (address != 0ull)
    {
        *target = value;
    }
}

static u64 elf64_stack_read_u64(u64 address)
{
    volatile const u64 *source = (volatile const u64 *)(u64)address;

    return (address != 0ull) ? *source : 0ull;
}

static u64 elf64_mix_entropy64(u64 state, u64 value)
{
    state ^= value + 0x9E3779B97F4A7C15ull + (state << 6) + (state >> 2);
    state ^= state >> 30;
    state *= 0xBF58476D1CE4E5B9ull;
    state ^= state >> 27;
    state *= 0x94D049BB133111EBull;
    state ^= state >> 31;
    return state;
}

static u64 elf64_entropy_next(u64 *state)
{
    u64 value;

    if ((state == 0) || (*state == 0ull))
    {
        return 0xA5A5A5A55A5A5A5Aull;
    }

    value = *state;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    *state = value;
    return value * 0x2545F4914F6CDD1Dull;
}

static u64 elf64_auxv_entropy_seed(
    u32 pid,
    u64 entry,
    u64 phdr_vaddr,
    u32 phnum,
    u64 interp_base)
{
    u64 seed = 0x454C463641555856ull;

    seed = elf64_mix_entropy64(seed, (u64)pid);
    seed = elf64_mix_entropy64(seed, (u64)process64_principal(pid));
    seed = elf64_mix_entropy64(seed, (u64)process64_manifest_token(pid));
    seed = elf64_mix_entropy64(seed, (u64)process64_runtime_token(pid));
    seed = elf64_mix_entropy64(seed, (u64)process64_runtime_image_token(pid));
    seed = elf64_mix_entropy64(seed, entry);
    seed = elf64_mix_entropy64(seed, phdr_vaddr);
    seed = elf64_mix_entropy64(seed, (u64)phnum);
    seed = elf64_mix_entropy64(seed, interp_base);
    seed = elf64_mix_entropy64(seed, (u64)pit_get_ticks());
    seed = elf64_mix_entropy64(seed, (u64)pit_get_frequency_hz());
    return (seed != 0ull) ? seed : 0x9E3779B97F4A7C15ull;
}

static u32 elf64_auxv_add(elf64_auxv_t *auxv, u64 type, u64 value)
{
    if ((auxv == 0) || (auxv->entry_count >= ELF64_AUXV_MAX_ENTRIES))
    {
        return 0u;
    }

    auxv->entries[auxv->entry_count].type = type;
    auxv->entries[auxv->entry_count].value = value;
    ++auxv->entry_count;
    return 1u;
}

static u32 elf64_auxv_has_null_terminator(const elf64_auxv_t *auxv)
{
    if ((auxv == 0)
        || (auxv->entry_count == 0u)
        || (auxv->entry_count > ELF64_AUXV_MAX_ENTRIES))
    {
        return 0u;
    }

    return ((auxv->entries[auxv->entry_count - 1u].type == ELF64_AT_NULL)
        && (auxv->entries[auxv->entry_count - 1u].value == 0ull))
        ? 1u
        : 0u;
}

static u32 elf64_stack_range_writable(u64 stack_base, u64 stack_top)
{
    u64 page;

    if ((stack_base >= stack_top)
        || ((stack_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_top & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        return 0u;
    }

    for (page = stack_base; page < stack_top; page += VMA64_PAGE_BYTES)
    {
        if ((paging64_user_page_present(page) == 0u)
            || ((paging64_user_page_protection(page) & PAGING64_USER_PROT_WRITE) == 0u))
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 elf64_validate_table_range(u32 size, u64 offset, u16 count, u16 entry_size)
{
    u64 total;

    if (count == 0u)
    {
        return 1u;
    }
    if ((offset == 0ull) || (entry_size == 0u))
    {
        return 0u;
    }

    total = (u64)count * (u64)entry_size;
    return elf64_range_available(size, offset, total);
}

static u32 elf64_vma_prot_from_flags(u32 flags)
{
    u32 prot = 0u;

    if ((flags & ELF64_PF_R) != 0u)
    {
        prot |= VMA64_PROT_READ;
    }
    if ((flags & ELF64_PF_W) != 0u)
    {
        prot |= VMA64_PROT_WRITE;
    }
    if ((flags & ELF64_PF_X) != 0u)
    {
        prot |= VMA64_PROT_EXECUTE;
    }

    return prot;
}

static void elf64_unmap_recorded_segments(
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

static void elf64_unmap_phdr_segments(
    u32 pid,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 base_offset)
{
    u32 index;

    if ((phdrs == 0) || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        return;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 load_vaddr;
        u64 load_end;
        u64 map_base;
        u64 map_offset;
        u64 map_bytes;

        if ((phdr->type != ELF64_PT_LOAD) || (phdr->memsz == 0ull))
        {
            continue;
        }

        load_vaddr = phdr->vaddr + base_offset;
        if (load_vaddr < phdr->vaddr)
        {
            continue;
        }

        load_end = load_vaddr + phdr->memsz;
        if (load_end < load_vaddr)
        {
            continue;
        }

        map_base = elf64_align_down(load_vaddr, VMA64_PAGE_BYTES);
        map_offset = load_vaddr - map_base;
        map_bytes = elf64_align_up(map_offset + phdr->memsz, VMA64_PAGE_BYTES);
        if ((map_bytes != 0ull) && ((map_base + map_bytes) >= map_base))
        {
            (void)vma64_unmap(pid, map_base, map_bytes);
        }
    }
}

static u64 elf64_program_header_vaddr(
    const elf64_header_t *header,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 base_offset)
{
    u32 index;
    u64 fallback = 0ull;
    u64 phdr_bytes;

    if ((header == 0) || (phdrs == 0) || (phdr_count == 0u))
    {
        return 0ull;
    }

    phdr_bytes = (u64)header->phnum * (u64)header->phentsize;
    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 file_end;

        if (phdr->type != ELF64_PT_LOAD)
        {
            continue;
        }
        if (fallback == 0ull)
        {
            fallback = base_offset + phdr->vaddr + header->phoff;
        }
        file_end = phdr->offset + phdr->filesz;
        if ((file_end >= phdr->offset)
            && (header->phoff >= phdr->offset)
            && ((header->phoff + phdr_bytes) >= header->phoff)
            && ((header->phoff + phdr_bytes) <= file_end))
        {
            return base_offset + phdr->vaddr + (header->phoff - phdr->offset);
        }
    }

    return fallback;
}

static u32 elf64_transfer_frame_ready(
    u32 pid,
    u64 entry_rip,
    u64 stack_base,
    u64 stack_top,
    elf64_launch_result_t *result)
{
    u64 entry_page;
    u32 entry_present;
    u32 entry_prot;
    u32 stack_present;
    u32 stack_prot;

    if ((pid == PROCESS64_INVALID_PID)
        || (entry_rip == 0ull)
        || (stack_base >= stack_top)
        || (vma64_find(pid, entry_rip) == 0)
        || (vma64_find(pid, stack_base) == 0))
    {
        return 0u;
    }

    entry_page = elf64_align_down(entry_rip, VMA64_PAGE_BYTES);
    entry_present = paging64_user_page_present(entry_page);
    entry_prot = paging64_user_page_protection(entry_page);
    stack_present = paging64_user_page_present(stack_base);
    stack_prot = paging64_user_page_protection(stack_base);

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

static void elf64_copy_to_user(
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
        source_sum = elf64_mix_checksum(source_sum, value);
        mapped_sum = elf64_mix_checksum(mapped_sum, target[index]);
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

static u32 elf64_zero_bss(u64 destination, u64 byte_count)
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

u32 elf64_parse_header(const u8 *data, u32 size, elf64_header_t *out_header)
{
    elf64_clear_header(out_header);

    if ((data == 0) || (out_header == 0))
    {
        elf64_set_header_error(out_header, ELF64_ERROR_NULL);
        return ELF64_DENIED;
    }
    if (size < ELF64_EHDR_BYTES)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_SHORT_HEADER);
        return ELF64_DENIED;
    }
    if ((data[0] != 0x7Fu)
        || (data[1] != (u8)'E')
        || (data[2] != (u8)'L')
        || (data[3] != (u8)'F'))
    {
        elf64_set_header_error(out_header, ELF64_ERROR_MAGIC);
        return ELF64_DENIED;
    }
    if (data[ELF64_EI_CLASS] != (u8)ELF64_CLASS_64)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_CLASS);
        return ELF64_DENIED;
    }
    if (data[ELF64_EI_DATA] != (u8)ELF64_DATA_LSB)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_DATA);
        return ELF64_DENIED;
    }
    if (data[ELF64_EI_VERSION] != (u8)ELF64_VERSION_CURRENT)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_VERSION);
        return ELF64_DENIED;
    }
    if ((data[ELF64_EI_OSABI] != (u8)ELF64_OSABI_NONE)
        && (data[ELF64_EI_OSABI] != (u8)ELF64_OSABI_LINUX))
    {
        elf64_set_header_error(out_header, ELF64_ERROR_OSABI);
        return ELF64_DENIED;
    }

    out_header->osabi = (u32)data[ELF64_EI_OSABI];
    out_header->type = elf64_read_le16(data + 16u);
    out_header->machine = elf64_read_le16(data + 18u);
    out_header->version = elf64_read_le32(data + 20u);
    out_header->entry = elf64_read_le64(data + 24u);
    out_header->phoff = elf64_read_le64(data + 32u);
    out_header->shoff = elf64_read_le64(data + 40u);
    out_header->flags = elf64_read_le32(data + 48u);
    out_header->ehsize = elf64_read_le16(data + 52u);
    out_header->phentsize = elf64_read_le16(data + 54u);
    out_header->phnum = elf64_read_le16(data + 56u);
    out_header->shentsize = elf64_read_le16(data + 58u);
    out_header->shnum = elf64_read_le16(data + 60u);
    out_header->shstrndx = elf64_read_le16(data + 62u);

    if ((out_header->type != ELF64_TYPE_EXEC) && (out_header->type != ELF64_TYPE_DYN))
    {
        elf64_set_header_error(out_header, ELF64_ERROR_TYPE);
        return ELF64_DENIED;
    }
    if (out_header->machine != ELF64_MACHINE_X86_64)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_MACHINE);
        return ELF64_DENIED;
    }
    if (out_header->version != ELF64_VERSION_CURRENT)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_VERSION);
        return ELF64_DENIED;
    }
    if (out_header->ehsize != ELF64_EHDR_BYTES)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_HEADER_SIZE);
        return ELF64_DENIED;
    }
    if ((out_header->phnum == 0u) || (out_header->phentsize != ELF64_PHDR_BYTES))
    {
        elf64_set_header_error(out_header, ELF64_ERROR_PHDR_SIZE);
        return ELF64_DENIED;
    }
    if (out_header->phnum > ELF64_MAX_PROGRAM_HEADERS)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_PHDR_COUNT);
        return ELF64_DENIED;
    }
    if (elf64_validate_table_range(
            size,
            out_header->phoff,
            out_header->phnum,
            out_header->phentsize) == 0u)
    {
        elf64_set_header_error(out_header, ELF64_ERROR_PHDR_RANGE);
        return ELF64_DENIED;
    }
    if ((out_header->shnum != 0u)
        && (elf64_validate_table_range(
            size,
            out_header->shoff,
            out_header->shnum,
            out_header->shentsize) == 0u))
    {
        elf64_set_header_error(out_header, ELF64_ERROR_SECTION_RANGE);
        return ELF64_DENIED;
    }

    out_header->error = ELF64_ERROR_NONE;
    return ELF64_OK;
}

u32 elf64_parse_phdrs(
    const u8 *data,
    u32 size,
    const elf64_header_t *header,
    elf64_program_header_t *out_phdrs,
    u32 max_phdrs,
    elf64_phdr_summary_t *out_summary)
{
    u32 index;

    elf64_clear_summary(out_summary);

    if ((data == 0) || (header == 0) || (out_phdrs == 0))
    {
        elf64_set_summary_error(out_summary, ELF64_ERROR_NULL);
        return ELF64_DENIED;
    }
    if (header->error != ELF64_ERROR_NONE)
    {
        elf64_set_summary_error(out_summary, header->error);
        return ELF64_DENIED;
    }
    if (header->phnum > max_phdrs)
    {
        elf64_set_summary_error(out_summary, ELF64_ERROR_OUTPUT_CAPACITY);
        return ELF64_DENIED;
    }
    if ((header->phnum == 0u)
        || (header->phentsize != ELF64_PHDR_BYTES)
        || (elf64_validate_table_range(
            size,
            header->phoff,
            header->phnum,
            header->phentsize) == 0u))
    {
        elf64_set_summary_error(out_summary, ELF64_ERROR_PHDR_RANGE);
        return ELF64_DENIED;
    }

    for (index = 0u; index < max_phdrs; ++index)
    {
        elf64_clear_program_header(&out_phdrs[index]);
    }

    for (index = 0u; index < header->phnum; ++index)
    {
        const u8 *record = data + header->phoff + ((u64)index * header->phentsize);
        elf64_program_header_t *phdr = &out_phdrs[index];
        u64 load_end;

        phdr->type = elf64_read_le32(record);
        phdr->flags = elf64_read_le32(record + 4u);
        phdr->offset = elf64_read_le64(record + 8u);
        phdr->vaddr = elf64_read_le64(record + 16u);
        phdr->paddr = elf64_read_le64(record + 24u);
        phdr->filesz = elf64_read_le64(record + 32u);
        phdr->memsz = elf64_read_le64(record + 40u);
        phdr->align = elf64_read_le64(record + 48u);

        if (phdr->type == ELF64_PT_LOAD)
        {
            if (phdr->memsz < phdr->filesz)
            {
                elf64_set_summary_error(out_summary, ELF64_ERROR_LOAD_SIZE);
                return ELF64_DENIED;
            }
            if (elf64_range_available(size, phdr->offset, phdr->filesz) == 0u)
            {
                elf64_set_summary_error(out_summary, ELF64_ERROR_LOAD_RANGE);
                return ELF64_DENIED;
            }
            if ((phdr->align > 1ull)
                && ((elf64_is_power_of_two(phdr->align) == 0u)
                    || ((phdr->vaddr & (phdr->align - 1ull))
                        != (phdr->offset & (phdr->align - 1ull)))))
            {
                elf64_set_summary_error(out_summary, ELF64_ERROR_LOAD_ALIGN);
                return ELF64_DENIED;
            }

            load_end = phdr->vaddr + phdr->memsz;
            if (load_end < phdr->vaddr)
            {
                elf64_set_summary_error(out_summary, ELF64_ERROR_LOAD_SIZE);
                return ELF64_DENIED;
            }

            if (out_summary != 0)
            {
                if (out_summary->load_count == 0u)
                {
                    out_summary->first_load_vaddr = phdr->vaddr;
                }
                ++out_summary->load_count;
                out_summary->load_mem_bytes += phdr->memsz;
                if (load_end > out_summary->max_load_end)
                {
                    out_summary->max_load_end = load_end;
                }
                if ((phdr->flags & ELF64_PF_X) != 0u)
                {
                    ++out_summary->executable_loads;
                }
                if ((phdr->flags & ELF64_PF_W) != 0u)
                {
                    ++out_summary->writable_loads;
                }
            }
        }
        else if (phdr->type == ELF64_PT_INTERP)
        {
            if ((phdr->filesz == 0ull)
                || (elf64_range_available(size, phdr->offset, phdr->filesz) == 0u))
            {
                elf64_set_summary_error(out_summary, ELF64_ERROR_INTERP_RANGE);
                return ELF64_DENIED;
            }
            if (out_summary != 0)
            {
                ++out_summary->interp_count;
            }
        }
        else if (phdr->type == ELF64_PT_GNU_STACK)
        {
            if (out_summary != 0)
            {
                ++out_summary->gnu_stack_count;
            }
        }
        else if (phdr->type == ELF64_PT_GNU_RELRO)
        {
            if (out_summary != 0)
            {
                ++out_summary->gnu_relro_count;
            }
        }
        else if (phdr->type == ELF64_PT_TLS)
        {
            if (out_summary != 0)
            {
                ++out_summary->tls_count;
            }
        }
        else if (phdr->type == ELF64_PT_DYNAMIC)
        {
            if (out_summary != 0)
            {
                ++out_summary->dynamic_count;
            }
        }
    }

    elf64_set_summary_error(out_summary, ELF64_ERROR_NONE);
    return ELF64_OK;
}

u32 elf64_map_load_segments(
    u32 pid,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    const u8 *binary_data,
    u32 binary_size,
    u64 base_offset,
    elf64_load_result_t *out_result)
{
    u64 mapped_bases[ELF64_MAX_PROGRAM_HEADERS];
    u64 mapped_lengths[ELF64_MAX_PROGRAM_HEADERS];
    u32 index;
    u32 mapped_count = 0u;

    elf64_clear_load_result(out_result);

    if ((phdrs == 0)
        || (binary_data == 0)
        || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        elf64_set_load_error(out_result, ELF64_ERROR_NULL);
        return ELF64_DENIED;
    }

    for (index = 0u; index < ELF64_MAX_PROGRAM_HEADERS; ++index)
    {
        mapped_bases[index] = 0ull;
        mapped_lengths[index] = 0ull;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 load_vaddr;
        u64 load_end;
        u64 map_base;
        u64 map_offset;
        u64 map_bytes;
        u32 final_prot;
        u32 map_result;

        if (phdr->type != ELF64_PT_LOAD)
        {
            continue;
        }

        if (phdr->memsz == 0ull)
        {
            continue;
        }
        if ((phdr->filesz > phdr->memsz)
            || (elf64_range_available(binary_size, phdr->offset, phdr->filesz) == 0u))
        {
            elf64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            elf64_set_load_error(out_result, ELF64_ERROR_LOAD_RANGE);
            return ELF64_DENIED;
        }

        load_vaddr = phdr->vaddr + base_offset;
        if (load_vaddr < phdr->vaddr)
        {
            elf64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            elf64_set_load_error(out_result, ELF64_ERROR_LOAD_ADDRESS);
            return ELF64_DENIED;
        }

        load_end = load_vaddr + phdr->memsz;
        if (load_end < load_vaddr)
        {
            elf64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            elf64_set_load_error(out_result, ELF64_ERROR_LOAD_ADDRESS);
            return ELF64_DENIED;
        }

        map_base = elf64_align_down(load_vaddr, VMA64_PAGE_BYTES);
        map_offset = load_vaddr - map_base;
        map_bytes = elf64_align_up(map_offset + phdr->memsz, VMA64_PAGE_BYTES);
        if ((map_bytes == 0ull) || ((map_base + map_bytes) < map_base))
        {
            elf64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            elf64_set_load_error(out_result, ELF64_ERROR_LOAD_ADDRESS);
            return ELF64_DENIED;
        }

        final_prot = elf64_vma_prot_from_flags(phdr->flags);
        if (final_prot == 0u)
        {
            elf64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            elf64_set_load_error(out_result, ELF64_ERROR_LOAD_PROTECT);
            return ELF64_DENIED;
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
            elf64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            elf64_set_load_error(out_result, ELF64_ERROR_LOAD_MAP);
            return ELF64_DENIED;
        }

        mapped_bases[mapped_count] = map_base;
        mapped_lengths[mapped_count] = map_bytes;
        ++mapped_count;

        elf64_copy_to_user(
            load_vaddr,
            binary_data + phdr->offset,
            phdr->filesz,
            (out_result != 0) ? &out_result->source_checksum : 0,
            (out_result != 0) ? &out_result->mapped_checksum : 0);
        if (out_result != 0)
        {
            out_result->bss_nonzero_count += elf64_zero_bss(
                load_vaddr + phdr->filesz,
                phdr->memsz - phdr->filesz);
        }
        else
        {
            (void)elf64_zero_bss(load_vaddr + phdr->filesz, phdr->memsz - phdr->filesz);
        }

        if (vma64_protect(pid, map_base, map_bytes, final_prot) == 0u)
        {
            elf64_unmap_recorded_segments(pid, mapped_bases, mapped_lengths, mapped_count);
            elf64_set_load_error(out_result, ELF64_ERROR_LOAD_PROTECT);
            return ELF64_DENIED;
        }

        if (out_result != 0)
        {
            if (out_result->mapped_count == 0u)
            {
                out_result->first_mapped_vaddr = map_base;
            }
            ++out_result->mapped_count;
            out_result->total_map_bytes += map_bytes;
            out_result->total_file_bytes += phdr->filesz;
            out_result->total_bss_bytes += phdr->memsz - phdr->filesz;
            if ((map_base + map_bytes) > out_result->max_mapped_end)
            {
                out_result->max_mapped_end = map_base + map_bytes;
            }
        }
    }

    elf64_set_load_error(out_result, ELF64_ERROR_NONE);
    return ELF64_OK;
}

u32 elf64_apply_gnu_relro(
    u32 pid,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 base_offset,
    elf64_relro_result_t *out_result)
{
    u32 index;

    elf64_clear_relro_result(out_result);

    if ((phdrs == 0) || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        elf64_set_relro_error(out_result, ELF64_ERROR_NULL);
        return ELF64_DENIED;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 relro_start;
        u64 relro_end;
        u64 protect_base;
        u64 protect_end;
        u64 protect_bytes;

        if (phdr->type != ELF64_PT_GNU_RELRO)
        {
            continue;
        }

        if (phdr->memsz == 0ull)
        {
            continue;
        }

        relro_start = phdr->vaddr + base_offset;
        if (relro_start < phdr->vaddr)
        {
            elf64_set_relro_error(out_result, ELF64_ERROR_RELRO_RANGE);
            return ELF64_DENIED;
        }

        relro_end = relro_start + phdr->memsz;
        if (relro_end < relro_start)
        {
            elf64_set_relro_error(out_result, ELF64_ERROR_RELRO_RANGE);
            return ELF64_DENIED;
        }

        protect_base = elf64_align_down(relro_start, VMA64_PAGE_BYTES);
        protect_end = elf64_align_up(relro_end, VMA64_PAGE_BYTES);
        if ((protect_end <= protect_base) || (protect_end < relro_end))
        {
            elf64_set_relro_error(out_result, ELF64_ERROR_RELRO_RANGE);
            return ELF64_DENIED;
        }

        protect_bytes = protect_end - protect_base;
        if (vma64_protect(pid, protect_base, protect_bytes, VMA64_PROT_READ) == 0u)
        {
            elf64_set_relro_error(out_result, ELF64_ERROR_RELRO_PROTECT);
            return ELF64_DENIED;
        }

        if (out_result != 0)
        {
            if (out_result->relro_count == 0u)
            {
                out_result->first_protected_vaddr = protect_base;
            }
            ++out_result->relro_count;
            ++out_result->protected_count;
            out_result->total_protected_bytes += protect_bytes;
            if (protect_end > out_result->max_protected_end)
            {
                out_result->max_protected_end = protect_end;
            }
        }
    }

    elf64_set_relro_error(out_result, ELF64_ERROR_NONE);
    return ELF64_OK;
}

u32 elf64_build_auxv(
    u32 pid,
    u64 entry,
    u64 phdr_vaddr,
    u32 phnum,
    u64 interp_base,
    elf64_auxv_t *out_auxv)
{
    linux_vdso64_info_t vdso_info;
    u64 entropy;
    u64 random_word = 0ull;
    u32 index;
    u32 ok = 1u;
    u32 required_entries = ELF64_AUXV_BASE_ENTRIES;
    u32 vdso_mapped = 0u;

    if (out_auxv == 0)
    {
        return ELF64_DENIED;
    }

    elf64_clear_auxv(out_auxv);

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (entry == 0ull)
        || (phdr_vaddr == 0ull)
        || (phnum == 0u)
        || (phnum > ELF64_MAX_PROGRAM_HEADERS))
    {
        elf64_set_auxv_error(out_auxv, ELF64_ERROR_AUX_ARGUMENT);
        return ELF64_DENIED;
    }

    out_auxv->platform[0] = (u8)'x';
    out_auxv->platform[1] = (u8)'8';
    out_auxv->platform[2] = (u8)'6';
    out_auxv->platform[3] = (u8)'_';
    out_auxv->platform[4] = (u8)'6';
    out_auxv->platform[5] = (u8)'4';
    out_auxv->platform[6] = 0u;
    out_auxv->platform[7] = 0u;
    out_auxv->platform_staging_address = (u64)(void *)&out_auxv->platform[0];

    entropy = elf64_auxv_entropy_seed(pid, entry, phdr_vaddr, phnum, interp_base);
    for (index = 0u; index < ELF64_AUX_RANDOM_BYTES; ++index)
    {
        if ((index & 7u) == 0u)
        {
            random_word = elf64_entropy_next(&entropy);
        }
        out_auxv->random[index] = (u8)(random_word >> ((index & 7u) * 8u));
    }
    out_auxv->random_byte_count = ELF64_AUX_RANDOM_BYTES;
    out_auxv->random_staging_address = (u64)(void *)&out_auxv->random[0];
    out_auxv->random_checksum = elf64_checksum_bytes(
        out_auxv->random,
        ELF64_AUX_RANDOM_BYTES);
    out_auxv->platform_checksum = elf64_checksum_bytes(out_auxv->platform, 7u);

    if (persona64_type(pid) == PERSONA64_TYPE_LINUX_ELF)
    {
        if ((linux_vdso64_map(pid, &vdso_info) != LINUX_VDSO64_MAP_OK)
            || (vdso_info.base != LINUX_VDSO64_BASE)
            || (vdso_info.page_present == 0u)
            || (vdso_info.elf_magic == 0u)
            || (vdso_info.phdr_count != LINUX_VDSO64_PHDR_COUNT))
        {
            elf64_set_auxv_error(out_auxv, ELF64_ERROR_AUX_VDSO_MAP);
            return ELF64_DENIED;
        }
        vdso_mapped = 1u;
        required_entries = ELF64_AUXV_LINUX_VDSO_ENTRIES;
    }

    /*
     * E.6 will move the random/platform staging blobs onto the ring-3 stack
     * and rewrite these two pointer-valued entries to their user addresses.
     */
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_PHDR, phdr_vaddr);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_PHENT, (u64)ELF64_PHDR_BYTES);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_PHNUM, (u64)phnum);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_PAGESZ, (u64)VMA64_PAGE_BYTES);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_BASE, interp_base);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_FLAGS, 0ull);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_ENTRY, entry);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_UID, ELF64_AUX_DEFAULT_UID);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_EUID, ELF64_AUX_DEFAULT_UID);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_GID, ELF64_AUX_DEFAULT_GID);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_EGID, ELF64_AUX_DEFAULT_GID);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_HWCAP, ELF64_AUX_HWCAP_BASELINE);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_CLKTCK, (u64)pit_get_frequency_hz());
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_SECURE, 0ull);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_RANDOM, out_auxv->random_staging_address);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_HWCAP2, ELF64_AUX_HWCAP2_BASELINE);
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_PLATFORM, out_auxv->platform_staging_address);
    if (vdso_mapped != 0u)
    {
        ok &= elf64_auxv_add(out_auxv, ELF64_AT_SYSINFO_EHDR, LINUX_VDSO64_BASE);
    }
    ok &= elf64_auxv_add(out_auxv, ELF64_AT_NULL, 0ull);

    if ((ok == 0u) || (out_auxv->entry_count != required_entries))
    {
        if (vdso_mapped != 0u)
        {
            (void)linux_vdso64_unmap(pid);
        }
        elf64_set_auxv_error(out_auxv, ELF64_ERROR_AUX_CAPACITY);
        return ELF64_DENIED;
    }

    elf64_set_auxv_error(out_auxv, ELF64_ERROR_NONE);
    return ELF64_OK;
}

u32 elf64_auxv_has_type(const elf64_auxv_t *auxv, u64 type)
{
    u32 index;

    if (auxv == 0)
    {
        return 0u;
    }

    for (index = 0u; index < auxv->entry_count; ++index)
    {
        if (auxv->entries[index].type == type)
        {
            return 1u;
        }
    }

    return 0u;
}

u64 elf64_auxv_value(const elf64_auxv_t *auxv, u64 type)
{
    u32 index;

    if (auxv == 0)
    {
        return 0ull;
    }

    for (index = 0u; index < auxv->entry_count; ++index)
    {
        if (auxv->entries[index].type == type)
        {
            return auxv->entries[index].value;
        }
    }

    return 0ull;
}

static void elf64_unmap_linux_vdso_if_present(u32 pid, const elf64_auxv_t *auxv)
{
    if (elf64_auxv_has_type(auxv, ELF64_AT_SYSINFO_EHDR) != 0u)
    {
        (void)linux_vdso64_unmap(pid);
    }
}

u32 elf64_build_initial_stack(
    u32 pid,
    u64 stack_base,
    u64 stack_top,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    const elf64_auxv_t *auxv,
    elf64_stack_result_t *out_result)
{
    u64 argv_ptrs[ELF64_STACK_MAX_ARGC];
    u64 envp_ptrs[ELF64_STACK_MAX_ENVC];
    u64 cursor;
    u64 pointer_bytes;
    u64 initial_rsp;
    u64 write_cursor;
    u32 index;
    u32 string_length;
    u32 stack_bytes;
    u32 zero_index;

    if (out_result == 0)
    {
        return ELF64_DENIED;
    }

    elf64_clear_stack_result(out_result);
    out_result->stack_base = stack_base;
    out_result->stack_top = stack_top;
    out_result->argc = argc;
    out_result->envc = envc;

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (argc > ELF64_STACK_MAX_ARGC)
        || (envc > ELF64_STACK_MAX_ENVC)
        || ((argc != 0u) && (argv == 0))
        || ((envc != 0u) && (envp == 0))
        || (auxv == 0)
        || (elf64_auxv_has_null_terminator(auxv) == 0u))
    {
        elf64_set_stack_error(out_result, ELF64_ERROR_STACK_ARGUMENT);
        return ELF64_DENIED;
    }

    if ((stack_base >= stack_top)
        || ((stack_top - stack_base) > 0xFFFFFFFFull)
        || (elf64_stack_range_writable(stack_base, stack_top) == 0u))
    {
        elf64_set_stack_error(out_result, ELF64_ERROR_STACK_RANGE);
        return ELF64_DENIED;
    }

    stack_bytes = (u32)(stack_top - stack_base);
    for (zero_index = 0u; zero_index < stack_bytes; ++zero_index)
    {
        ((volatile u8 *)(u64)stack_base)[zero_index] = 0u;
    }

    cursor = stack_top;
    if ((cursor - stack_base) < ELF64_AUX_PLATFORM_BYTES)
    {
        elf64_set_stack_error(out_result, ELF64_ERROR_STACK_OVERFLOW);
        return ELF64_DENIED;
    }
    cursor -= ELF64_AUX_PLATFORM_BYTES;
    out_result->platform_address = cursor;
    elf64_stack_write_bytes(cursor, auxv->platform, ELF64_AUX_PLATFORM_BYTES);

    if ((cursor - stack_base) < ELF64_AUX_RANDOM_BYTES)
    {
        elf64_set_stack_error(out_result, ELF64_ERROR_STACK_OVERFLOW);
        return ELF64_DENIED;
    }
    cursor -= ELF64_AUX_RANDOM_BYTES;
    out_result->random_address = cursor;
    elf64_stack_write_bytes(cursor, auxv->random, ELF64_AUX_RANDOM_BYTES);

    index = envc;
    while (index > 0u)
    {
        --index;
        string_length = elf64_strlen_bounded(envp[index], ELF64_STACK_MAX_STRING_BYTES);
        if ((string_length == 0u) || ((cursor - stack_base) < (u64)string_length))
        {
            elf64_set_stack_error(out_result, ELF64_ERROR_STACK_OVERFLOW);
            return ELF64_DENIED;
        }
        cursor -= (u64)string_length;
        envp_ptrs[index] = cursor;
        out_result->string_bytes += string_length;
        elf64_stack_write_bytes(cursor, (const u8 *)envp[index], string_length);
    }

    index = argc;
    while (index > 0u)
    {
        --index;
        string_length = elf64_strlen_bounded(argv[index], ELF64_STACK_MAX_STRING_BYTES);
        if ((string_length == 0u) || ((cursor - stack_base) < (u64)string_length))
        {
            elf64_set_stack_error(out_result, ELF64_ERROR_STACK_OVERFLOW);
            return ELF64_DENIED;
        }
        cursor -= (u64)string_length;
        argv_ptrs[index] = cursor;
        out_result->string_bytes += string_length;
        elf64_stack_write_bytes(cursor, (const u8 *)argv[index], string_length);
    }

    out_result->strings_base = cursor;
    out_result->random_checksum = elf64_checksum_bytes(
        (const u8 *)(u64)out_result->random_address,
        ELF64_AUX_RANDOM_BYTES);
    out_result->platform_checksum = elf64_checksum_bytes(
        (const u8 *)(u64)out_result->platform_address,
        ELF64_AUX_PLATFORM_BYTES);

    out_result->pointer_slot_count =
        1u + argc + 1u + envc + 1u + (auxv->entry_count * 2u);
    pointer_bytes = (u64)out_result->pointer_slot_count * sizeof(u64);
    if ((cursor < stack_base)
        || ((cursor - stack_base) < pointer_bytes))
    {
        elf64_set_stack_error(out_result, ELF64_ERROR_STACK_OVERFLOW);
        return ELF64_DENIED;
    }

    initial_rsp = (cursor - pointer_bytes) & ~0xFull;
    if ((initial_rsp < stack_base) || ((initial_rsp + pointer_bytes) > cursor))
    {
        elf64_set_stack_error(out_result, ELF64_ERROR_STACK_OVERFLOW);
        return ELF64_DENIED;
    }

    out_result->initial_rsp = initial_rsp;
    out_result->argc_address = initial_rsp;
    out_result->argv_address = initial_rsp + sizeof(u64);
    out_result->envp_address = out_result->argv_address + ((u64)(argc + 1u) * sizeof(u64));
    out_result->auxv_address = out_result->envp_address + ((u64)(envc + 1u) * sizeof(u64));
    out_result->auxv_entry_count = auxv->entry_count;
    out_result->layout_bytes = (u32)(stack_top - initial_rsp);
    out_result->alignment_ok = ((initial_rsp & 0xFull) == 0ull) ? 1u : 0u;

    write_cursor = initial_rsp;
    elf64_stack_write_u64(write_cursor, (u64)argc);
    write_cursor += sizeof(u64);
    for (index = 0u; index < argc; ++index)
    {
        elf64_stack_write_u64(write_cursor, argv_ptrs[index]);
        write_cursor += sizeof(u64);
    }
    elf64_stack_write_u64(write_cursor, 0ull);
    write_cursor += sizeof(u64);
    for (index = 0u; index < envc; ++index)
    {
        elf64_stack_write_u64(write_cursor, envp_ptrs[index]);
        write_cursor += sizeof(u64);
    }
    elf64_stack_write_u64(write_cursor, 0ull);
    write_cursor += sizeof(u64);
    for (index = 0u; index < auxv->entry_count; ++index)
    {
        u64 value = auxv->entries[index].value;

        if (auxv->entries[index].type == ELF64_AT_RANDOM)
        {
            value = out_result->random_address;
        }
        else if (auxv->entries[index].type == ELF64_AT_PLATFORM)
        {
            value = out_result->platform_address;
        }

        elf64_stack_write_u64(write_cursor, auxv->entries[index].type);
        write_cursor += sizeof(u64);
        elf64_stack_write_u64(write_cursor, value);
        write_cursor += sizeof(u64);
    }

    if (write_cursor != (initial_rsp + pointer_bytes))
    {
        elf64_set_stack_error(out_result, ELF64_ERROR_STACK_WRITE);
        return ELF64_DENIED;
    }

    out_result->argv_null_ok =
        (elf64_stack_read_u64(out_result->argv_address + ((u64)argc * sizeof(u64))) == 0ull)
        ? 1u
        : 0u;
    out_result->envp_null_ok =
        (elf64_stack_read_u64(out_result->envp_address + ((u64)envc * sizeof(u64))) == 0ull)
        ? 1u
        : 0u;
    out_result->auxv_null_ok =
        ((elf64_stack_read_u64(
                out_result->auxv_address + ((u64)(auxv->entry_count - 1u) * 16ull)) == ELF64_AT_NULL)
            && (elf64_stack_read_u64(
                out_result->auxv_address + ((u64)(auxv->entry_count - 1u) * 16ull) + 8ull) == 0ull))
        ? 1u
        : 0u;

    elf64_set_stack_error(out_result, ELF64_ERROR_NONE);
    return ELF64_OK;
}

u32 elf64_launch_static(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    u64 load_bias,
    u64 stack_base,
    u64 stack_bytes,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    u32 run_transfer_probe,
    elf64_launch_result_t *out_result)
{
    elf64_program_header_t phdrs[ELF64_MAX_PROGRAM_HEADERS];
    u64 stack_top;
    u64 entry_rip;
    u32 index;
    u32 segments_mapped = 0u;
    u32 stack_mapped = 0u;

    if (out_result == 0)
    {
        return ELF64_DENIED;
    }

    elf64_clear_launch_result(out_result);
    out_result->load_bias = load_bias;
    out_result->stack_base = stack_base;
    out_result->stack_top = stack_base + stack_bytes;

    for (index = 0u; index < ELF64_MAX_PROGRAM_HEADERS; ++index)
    {
        elf64_clear_program_header(&phdrs[index]);
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (binary_data == 0)
        || (binary_size == 0u)
        || (stack_bytes == 0ull)
        || ((stack_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_bytes & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_base + stack_bytes) <= stack_base))
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_ARGUMENT);
        return ELF64_DENIED;
    }

    stack_top = stack_base + stack_bytes;
    out_result->stack_top = stack_top;

    if (elf64_parse_header(binary_data, binary_size, &out_result->header) != ELF64_OK)
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_PARSE);
        return ELF64_DENIED;
    }

    if (out_result->header.type != ELF64_TYPE_EXEC)
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_DYNAMIC);
        return ELF64_DENIED;
    }

    if (elf64_parse_phdrs(
            binary_data,
            binary_size,
            &out_result->header,
            phdrs,
            ELF64_MAX_PROGRAM_HEADERS,
            &out_result->phdr_summary) != ELF64_OK)
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_PHDR);
        return ELF64_DENIED;
    }

    if ((out_result->phdr_summary.load_count == 0u)
        || (out_result->phdr_summary.interp_count != 0u)
        || (out_result->phdr_summary.dynamic_count != 0u))
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_DYNAMIC);
        return ELF64_DENIED;
    }

    entry_rip = out_result->header.entry + load_bias;
    if ((entry_rip < out_result->header.entry) || (entry_rip == 0ull))
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_ARGUMENT);
        return ELF64_DENIED;
    }

    out_result->entry_rip = entry_rip;
    out_result->phdr_vaddr = elf64_program_header_vaddr(
        &out_result->header,
        phdrs,
        out_result->header.phnum,
        load_bias);

    if (out_result->phdr_vaddr == 0ull)
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_PHDR);
        return ELF64_DENIED;
    }

    if (elf64_map_load_segments(
            pid,
            phdrs,
            out_result->header.phnum,
            binary_data,
            binary_size,
            load_bias,
            &out_result->load_result) != ELF64_OK)
    {
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_LOAD);
        return ELF64_DENIED;
    }
    segments_mapped = 1u;

    if (elf64_apply_gnu_relro(
            pid,
            phdrs,
            out_result->header.phnum,
            load_bias,
            &out_result->relro_result) != ELF64_OK)
    {
        elf64_unmap_phdr_segments(pid, phdrs, out_result->header.phnum, load_bias);
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_RELRO);
        return ELF64_DENIED;
    }

    if (vma64_map_anon(
            pid,
            stack_base,
            stack_bytes,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS) != stack_base)
    {
        elf64_unmap_phdr_segments(pid, phdrs, out_result->header.phnum, load_bias);
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_STACK_MAP);
        return ELF64_DENIED;
    }
    stack_mapped = 1u;

    if (elf64_build_auxv(
            pid,
            out_result->entry_rip,
            out_result->phdr_vaddr,
            (u32)out_result->header.phnum,
            0ull,
            &out_result->auxv) != ELF64_OK)
    {
        (void)vma64_unmap(pid, stack_base, stack_bytes);
        elf64_unmap_phdr_segments(pid, phdrs, out_result->header.phnum, load_bias);
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_AUXV);
        return ELF64_DENIED;
    }

    if (elf64_build_initial_stack(
            pid,
            stack_base,
            stack_top,
            argc,
            argv,
            envc,
            envp,
            &out_result->auxv,
            &out_result->stack_result) != ELF64_OK)
    {
        elf64_unmap_linux_vdso_if_present(pid, &out_result->auxv);
        (void)vma64_unmap(pid, stack_base, stack_bytes);
        elf64_unmap_phdr_segments(pid, phdrs, out_result->header.phnum, load_bias);
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_STACK);
        return ELF64_DENIED;
    }

    out_result->initial_rsp = out_result->stack_result.initial_rsp;
    out_result->transfer_rip = out_result->entry_rip;
    out_result->transfer_rsp = out_result->initial_rsp;
    out_result->transfer_selectors =
        ((u32)DESCRIPTORS64_USER_DATA_SELECTOR << 16)
        | (u32)DESCRIPTORS64_USER_CODE_SELECTOR;
    out_result->transfer_ready = elf64_transfer_frame_ready(
        pid,
        out_result->entry_rip,
        stack_base,
        stack_top,
        out_result);

    if (out_result->transfer_ready == 0u)
    {
        elf64_unmap_linux_vdso_if_present(pid, &out_result->auxv);
        (void)vma64_unmap(pid, stack_base, stack_bytes);
        elf64_unmap_phdr_segments(pid, phdrs, out_result->header.phnum, load_bias);
        elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_TRANSFER);
        return ELF64_DENIED;
    }

    if (run_transfer_probe != 0u)
    {
        out_result->transfer_result = interrupts64_trigger_user_entry_probe(
            out_result->transfer_rip,
            out_result->transfer_rsp,
            (u64)out_result->transfer_selectors,
            (u64)LAUNCH64_USER_RFLAGS);
        out_result->transfer_executed = 1u;
        if (out_result->transfer_result == 0u)
        {
            if (stack_mapped != 0u)
            {
                elf64_unmap_linux_vdso_if_present(pid, &out_result->auxv);
                (void)vma64_unmap(pid, stack_base, stack_bytes);
            }
            if (segments_mapped != 0u)
            {
                elf64_unmap_phdr_segments(pid, phdrs, out_result->header.phnum, load_bias);
            }
            elf64_set_launch_error(out_result, ELF64_ERROR_LAUNCH_TRANSFER);
            return ELF64_DENIED;
        }
    }

    elf64_set_launch_error(out_result, ELF64_ERROR_NONE);
    return ELF64_OK;
}
