#include "linux_dynamic_x64.h"

#include "linux_abi_x64.h"
#include "linux_libc_x64.h"
#include "linux_vdso_x64.h"
#include "paging_x64.h"
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * P.1 adds the first LimitlessOS-owned Linux dynamic-linker substrate. It
 * builds a compact in-tree ET_DYN ld-limitless.so image, maps it through the
 * VMA/ELF loader path, exposes a bounded rtld symbol registry, and prepares
 * dynamic ELF launch frames only when PT_INTERP and PT_DYNAMIC metadata are
 * valid and all DT_NEEDED entries are backed by known LimitlessOS shims. The
 * checkpoint proves interpreter mapping, AT_BASE stack publication, transfer
 * routing to _dl_start, explicit denial for unsupported dependencies, and
 * audit-backed failure rather than claiming libc or relocation support before
 * Phase P.2 provides it. P.3 adds libpthread DT_NEEDED alias accounting backed
 * by the existing pthread-capable libc shim, proving the dependency path
 * without adding a second runtime image or fabricated pthread coverage.
 */

typedef struct linux_dynamic64_export
{
    const char *name;
    u32 length;
    u32 symbol_id;
    u32 rva;
} linux_dynamic64_export_t;

static const linux_dynamic64_export_t g_linux_dynamic64_exports[
    LINUX_DYNAMIC64_SYMBOL_COUNT] = {
    { "_dl_start", 9u, 1u, LINUX_DYNAMIC64_RVA_DL_START },
    { "_dl_map_object", 14u, 2u, LINUX_DYNAMIC64_RVA_DL_MAP_OBJECT },
    { "_dl_bind_now", 12u, 3u, LINUX_DYNAMIC64_RVA_DL_BIND_NOW },
    { "_dl_runtime_resolve", 19u, 4u, LINUX_DYNAMIC64_RVA_DL_RUNTIME_RESOLVE },
    { "_dl_rtld_lock", 13u, 5u, LINUX_DYNAMIC64_RVA_DL_RTLD_LOCK },
    { "_dl_rtld_unlock", 15u, 6u, LINUX_DYNAMIC64_RVA_DL_RTLD_UNLOCK },
    { "dlopen", 6u, 7u, LINUX_DYNAMIC64_RVA_DLOPEN },
    { "dlclose", 7u, 8u, LINUX_DYNAMIC64_RVA_DLCLOSE },
    { "dlsym", 5u, 9u, LINUX_DYNAMIC64_RVA_DLSYM },
    { "dlerror", 7u, 10u, LINUX_DYNAMIC64_RVA_DLERROR }
};

static u8 g_linux_dynamic64_image[LINUX_DYNAMIC64_FILE_BYTES];
static u32 g_linux_dynamic64_image_ready = 0u;
static u32 g_linux_dynamic64_load_count = 0u;
static u32 g_linux_dynamic64_prepare_count = 0u;
static u32 g_linux_dynamic64_denial_count = 0u;
static u32 g_linux_dynamic64_dependency_denial_count = 0u;
static u32 g_linux_dynamic64_last_error = LINUX_DYNAMIC64_ERROR_NONE;
static u32 g_linux_dynamic64_last_needed_count = 0u;
static u32 g_linux_dynamic64_last_missing_count = 0u;

static void linux_dynamic64_write_le16(u8 *data, u32 offset, u16 value)
{
    data[offset] = (u8)(value & 0xFFu);
    data[offset + 1u] = (u8)((value >> 8) & 0xFFu);
}

static void linux_dynamic64_write_le32(u8 *data, u32 offset, u32 value)
{
    data[offset] = (u8)(value & 0xFFu);
    data[offset + 1u] = (u8)((value >> 8) & 0xFFu);
    data[offset + 2u] = (u8)((value >> 16) & 0xFFu);
    data[offset + 3u] = (u8)((value >> 24) & 0xFFu);
}

static void linux_dynamic64_write_le64(u8 *data, u32 offset, u64 value)
{
    linux_dynamic64_write_le32(data, offset, (u32)(value & 0xFFFFFFFFull));
    linux_dynamic64_write_le32(data, offset + 4u, (u32)(value >> 32));
}

static u64 linux_dynamic64_read_le64(const u8 *data)
{
    return (u64)data[0]
        | ((u64)data[1] << 8)
        | ((u64)data[2] << 16)
        | ((u64)data[3] << 24)
        | ((u64)data[4] << 32)
        | ((u64)data[5] << 40)
        | ((u64)data[6] << 48)
        | ((u64)data[7] << 56);
}

static u32 linux_dynamic64_mix_checksum(u32 checksum, u8 value)
{
    checksum ^= (u32)value;
    checksum *= 16777619u;
    return checksum;
}

static u32 linux_dynamic64_checksum_bytes(const u8 *data, u32 byte_count)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (data == 0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        checksum = linux_dynamic64_mix_checksum(checksum, data[index]);
    }

    return checksum;
}

static u32 linux_dynamic64_range_available(u32 size, u64 offset, u64 bytes)
{
    if (bytes == 0ull)
    {
        return 1u;
    }
    if ((offset > (u64)size) || (bytes > ((u64)size - offset)))
    {
        return 0u;
    }
    return 1u;
}

static u64 linux_dynamic64_align_down(u64 value, u64 alignment)
{
    return value & ~(alignment - 1ull);
}

static u64 linux_dynamic64_align_up(u64 value, u64 alignment)
{
    return (value + alignment - 1ull) & ~(alignment - 1ull);
}

static u32 linux_dynamic64_name_matches(
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

static u32 linux_dynamic64_cstring_length(const char *text, u32 max_bytes)
{
    u32 index;

    if (text == 0)
    {
        return 0u;
    }

    for (index = 0u; index < max_bytes; ++index)
    {
        if (text[index] == (char)0)
        {
            return index;
        }
    }

    return max_bytes;
}

static void linux_dynamic64_copy_bytes(u8 *target, const char *source, u32 length)
{
    u32 index;

    if ((target == 0) || (source == 0))
    {
        return;
    }

    for (index = 0u; index < length; ++index)
    {
        target[index] = (u8)source[index];
    }
}

static void linux_dynamic64_write_phdr(
    u32 offset,
    u32 type,
    u32 flags,
    u64 file_offset,
    u64 vaddr,
    u64 filesz,
    u64 memsz,
    u64 align)
{
    linux_dynamic64_write_le32(g_linux_dynamic64_image, offset, type);
    linux_dynamic64_write_le32(g_linux_dynamic64_image, offset + 4u, flags);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, offset + 8u, file_offset);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, offset + 16u, vaddr);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, offset + 24u, vaddr);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, offset + 32u, filesz);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, offset + 40u, memsz);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, offset + 48u, align);
}

static void linux_dynamic64_write_unavailable_stub(u32 rva)
{
    u32 offset = LINUX_DYNAMIC64_TEXT_FILE_OFFSET + (rva - LINUX_DYNAMIC64_TEXT_RVA);

    if ((rva < LINUX_DYNAMIC64_TEXT_RVA)
        || ((offset + 8u) > (LINUX_DYNAMIC64_TEXT_FILE_OFFSET + LINUX_DYNAMIC64_TEXT_FILE_BYTES)))
    {
        return;
    }

    g_linux_dynamic64_image[offset] = 0x48u;
    g_linux_dynamic64_image[offset + 1u] = 0xC7u;
    g_linux_dynamic64_image[offset + 2u] = 0xC0u;
    g_linux_dynamic64_image[offset + 3u] = 0xDAu;
    g_linux_dynamic64_image[offset + 4u] = 0xFFu;
    g_linux_dynamic64_image[offset + 5u] = 0xFFu;
    g_linux_dynamic64_image[offset + 6u] = 0xFFu;
    g_linux_dynamic64_image[offset + 7u] = 0xC3u;
}

static void linux_dynamic64_build_image(void)
{
    static const char image_name[] = "ld-limitless.so";
    static const char unavailable[] = "ld-limitless: runtime relocation unavailable";
    u32 index;

    if (g_linux_dynamic64_image_ready != 0u)
    {
        return;
    }

    for (index = 0u; index < LINUX_DYNAMIC64_FILE_BYTES; ++index)
    {
        g_linux_dynamic64_image[index] = 0u;
    }

    g_linux_dynamic64_image[0] = 0x7Fu;
    g_linux_dynamic64_image[1] = (u8)'E';
    g_linux_dynamic64_image[2] = (u8)'L';
    g_linux_dynamic64_image[3] = (u8)'F';
    g_linux_dynamic64_image[ELF64_EI_CLASS] = ELF64_CLASS_64;
    g_linux_dynamic64_image[ELF64_EI_DATA] = ELF64_DATA_LSB;
    g_linux_dynamic64_image[ELF64_EI_VERSION] = ELF64_VERSION_CURRENT;
    g_linux_dynamic64_image[ELF64_EI_OSABI] = ELF64_OSABI_LINUX;
    linux_dynamic64_write_le16(g_linux_dynamic64_image, 16u, (u16)ELF64_TYPE_DYN);
    linux_dynamic64_write_le16(g_linux_dynamic64_image, 18u, (u16)ELF64_MACHINE_X86_64);
    linux_dynamic64_write_le32(g_linux_dynamic64_image, 20u, ELF64_VERSION_CURRENT);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, 24u, LINUX_DYNAMIC64_RVA_DL_START);
    linux_dynamic64_write_le64(g_linux_dynamic64_image, 32u, 0x40ull);
    linux_dynamic64_write_le16(g_linux_dynamic64_image, 52u, (u16)ELF64_EHDR_BYTES);
    linux_dynamic64_write_le16(g_linux_dynamic64_image, 54u, (u16)ELF64_PHDR_BYTES);
    linux_dynamic64_write_le16(g_linux_dynamic64_image, 56u, 2u);

    linux_dynamic64_write_phdr(
        0x40u,
        ELF64_PT_LOAD,
        ELF64_PF_R | ELF64_PF_X,
        LINUX_DYNAMIC64_TEXT_FILE_OFFSET,
        LINUX_DYNAMIC64_TEXT_RVA,
        LINUX_DYNAMIC64_TEXT_FILE_BYTES + LINUX_DYNAMIC64_RODATA_FILE_BYTES,
        VMA64_PAGE_BYTES,
        0x100ull);
    linux_dynamic64_write_phdr(
        0x40u + ELF64_PHDR_BYTES,
        ELF64_PT_DYNAMIC,
        ELF64_PF_R,
        LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET,
        LINUX_DYNAMIC64_DYNAMIC_RVA,
        LINUX_DYNAMIC64_DYNAMIC_BYTES,
        LINUX_DYNAMIC64_DYNAMIC_BYTES,
        8ull);

    for (index = 0u; index < LINUX_DYNAMIC64_SYMBOL_COUNT; ++index)
    {
        linux_dynamic64_write_unavailable_stub(g_linux_dynamic64_exports[index].rva);
    }

    linux_dynamic64_copy_bytes(
        &g_linux_dynamic64_image[LINUX_DYNAMIC64_RODATA_FILE_OFFSET],
        image_name,
        (u32)sizeof(image_name));
    linux_dynamic64_copy_bytes(
        &g_linux_dynamic64_image[LINUX_DYNAMIC64_RODATA_FILE_OFFSET + 0x40u],
        unavailable,
        (u32)sizeof(unavailable));

    linux_dynamic64_write_le64(
        g_linux_dynamic64_image,
        LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET,
        LINUX_DYNAMIC64_DT_STRTAB);
    linux_dynamic64_write_le64(
        g_linux_dynamic64_image,
        LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET + 8u,
        LINUX_DYNAMIC64_RODATA_RVA);
    linux_dynamic64_write_le64(
        g_linux_dynamic64_image,
        LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET + 16u,
        LINUX_DYNAMIC64_DT_STRSZ);
    linux_dynamic64_write_le64(
        g_linux_dynamic64_image,
        LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET + 24u,
        0x80ull);
    linux_dynamic64_write_le64(
        g_linux_dynamic64_image,
        LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET + 32u,
        LINUX_DYNAMIC64_DT_NULL);
    linux_dynamic64_write_le64(
        g_linux_dynamic64_image,
        LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET + 40u,
        0ull);

    g_linux_dynamic64_image_ready = 1u;
}

static void linux_dynamic64_clear_needed(linux_dynamic64_needed_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->dynamic_found = 0u;
    result->strtab_found = 0u;
    result->needed_count = 0u;
    result->supported_count = 0u;
    result->missing_count = 0u;
    result->self_needed_count = 0u;
    result->libc_needed_count = 0u;
    result->pthread_needed_count = 0u;
    result->first_needed_checksum = 0u;
    result->last_needed_checksum = 0u;
    result->strtab_file_offset = 0u;
    result->strtab_size = 0u;
    result->error = LINUX_DYNAMIC64_ERROR_NONE;
}

static void linux_dynamic64_clear_load(linux_dynamic64_load_result_t *result)
{
    if (result == 0)
    {
        return;
    }

    result->image_base = 0ull;
    result->image_end = 0ull;
    result->dl_start = 0ull;
    result->dl_map_object = 0ull;
    result->dl_bind_now = 0ull;
    result->dl_runtime_resolve = 0ull;
    result->dl_rtld_lock = 0ull;
    result->dl_rtld_unlock = 0ull;
    result->dlopen_fn = 0ull;
    result->dlclose_fn = 0ull;
    result->dlsym_fn = 0ull;
    result->dlerror_fn = 0ull;
    result->file_bytes = 0u;
    result->image_bytes = 0u;
    result->symbol_count = 0u;
    result->image_checksum = 0u;
    result->text_checksum = 0u;
    result->rodata_checksum = 0u;
    result->name_checksum = 0u;
    result->text_protection = 0u;
    result->rodata_protection = 0u;
    result->context_stored = 0u;
    result->error = LINUX_DYNAMIC64_ERROR_NONE;
}

static void linux_dynamic64_clear_launch(linux_dynamic64_launch_result_t *result)
{
    u32 index;

    if (result == 0)
    {
        return;
    }

    linux_dynamic64_clear_needed(&result->needed_result);
    linux_dynamic64_clear_load(&result->interpreter_result);
    result->libc_result.image_base = 0ull;
    result->libc_result.image_end = 0ull;
    result->libc_result.read_fn = 0ull;
    result->libc_result.write_fn = 0ull;
    result->libc_result.open_fn = 0ull;
    result->libc_result.close_fn = 0ull;
    result->libc_result.exit_fn = 0ull;
    result->libc_result.memcpy_fn = 0ull;
    result->libc_result.memset_fn = 0ull;
    result->libc_result.strlen_fn = 0ull;
    result->libc_result.puts_fn = 0ull;
    result->libc_result.printf_fn = 0ull;
    result->libc_result.malloc_fn = 0ull;
    result->libc_result.abort_fn = 0ull;
    result->libc_result.getenv_fn = 0ull;
    result->libc_result.setenv_fn = 0ull;
    result->libc_result.errno_location_fn = 0ull;
    result->libc_result.errno_cell = 0ull;
    result->libc_result.pthread_create_fn = 0ull;
    result->libc_result.pthread_join_fn = 0ull;
    result->libc_result.pthread_exit_fn = 0ull;
    result->libc_result.pthread_mutex_lock_fn = 0ull;
    result->libc_result.pthread_mutex_unlock_fn = 0ull;
    result->libc_result.pthread_cond_init_fn = 0ull;
    result->libc_result.pthread_cond_destroy_fn = 0ull;
    result->libc_result.pthread_cond_wait_fn = 0ull;
    result->libc_result.pthread_cond_signal_fn = 0ull;
    result->libc_result.pthread_cond_broadcast_fn = 0ull;
    result->libc_result.pthread_key_create_fn = 0ull;
    result->libc_result.pthread_setspecific_fn = 0ull;
    result->libc_result.pthread_getspecific_fn = 0ull;
    result->libc_result.file_bytes = 0u;
    result->libc_result.image_bytes = 0u;
    result->libc_result.symbol_count = 0u;
    result->libc_result.syscall_symbol_count = 0u;
    result->libc_result.memory_symbol_count = 0u;
    result->libc_result.string_symbol_count = 0u;
    result->libc_result.stdio_symbol_count = 0u;
    result->libc_result.heap_symbol_count = 0u;
    result->libc_result.abort_symbol_count = 0u;
    result->libc_result.env_symbol_count = 0u;
    result->libc_result.unavailable_symbol_count = 0u;
    result->libc_result.errno_symbol_count = 0u;
    result->libc_result.pthread_create_symbol_count = 0u;
    result->libc_result.pthread_join_symbol_count = 0u;
    result->libc_result.pthread_exit_symbol_count = 0u;
    result->libc_result.pthread_mutex_symbol_count = 0u;
    result->libc_result.pthread_cond_symbol_count = 0u;
    result->libc_result.pthread_tls_symbol_count = 0u;
    result->libc_result.errno_page_mapped = 0u;
    result->libc_result.image_checksum = 0u;
    result->libc_result.text_checksum = 0u;
    result->libc_result.rodata_checksum = 0u;
    result->libc_result.name_checksum = 0u;
    result->libc_result.text_protection = 0u;
    result->libc_result.context_stored = 0u;
    result->libc_result.error = LINUX_LIBC64_ERROR_NONE;
    result->app_load_bias = 0ull;
    result->app_entry = 0ull;
    result->app_phdr_vaddr = 0ull;
    result->interpreter_base = 0ull;
    result->stack_base = 0ull;
    result->stack_top = 0ull;
    result->initial_rsp = 0ull;
    result->transfer_rip = 0ull;
    result->transfer_rsp = 0ull;
    for (index = 0u; index < ELF64_MAX_PROGRAM_HEADERS; ++index)
    {
        result->app_mapped_bases[index] = 0ull;
        result->app_mapped_lengths[index] = 0ull;
    }
    result->app_mapped_count = 0u;
    result->libc_required = 0u;
    result->libc_mapped = 0u;
    result->pthread_required = 0u;
    result->pthread_mapped = 0u;
    result->interp_path_bytes = 0u;
    result->interp_path_checksum = 0u;
    result->transfer_ready = 0u;
    result->error = LINUX_DYNAMIC64_ERROR_NONE;
}

static u32 linux_dynamic64_valid_persona(u32 pid)
{
    return ((pid != PROCESS64_INVALID_PID)
        && (process64_principal(pid) != 0u)
        && (persona64_type(pid) == PERSONA64_TYPE_LINUX_ELF))
        ? 1u
        : 0u;
}

static u32 linux_dynamic64_record_denial(u32 pid, u32 error, u64 rip)
{
    ++g_linux_dynamic64_denial_count;
    if (error == LINUX_DYNAMIC64_ERROR_DEPENDENCY)
    {
        ++g_linux_dynamic64_dependency_denial_count;
    }
    g_linux_dynamic64_last_error = error;
    if (pid != PROCESS64_INVALID_PID)
    {
        (void)persona_audit64_record(
            pid,
            PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
            (u16)error,
            LINUX_ABI64_ENOSYS,
            rip);
    }
    return LINUX_DYNAMIC64_DENIED;
}

static u32 linux_dynamic64_interpreter_path_supported(
    const u8 *path,
    u32 path_bytes)
{
    static const char preferred_path[] = "/lib/ld-limitless.so";
    static const char lib64_path[] = "/lib64/ld-limitless.so";

    if (path == 0)
    {
        return 0u;
    }

    if (linux_dynamic64_name_matches(
            (const char *)path,
            path_bytes,
            preferred_path,
            (u32)sizeof(preferred_path) - 1u) != 0u)
    {
        return 1u;
    }

    return linux_dynamic64_name_matches(
        (const char *)path,
        path_bytes,
        lib64_path,
        (u32)sizeof(lib64_path) - 1u);
}

static u32 linux_dynamic64_string_checksum(
    const u8 *data,
    u32 max_bytes,
    u32 *out_length,
    u32 *out_checksum)
{
    u32 checksum = 2166136261u;
    u32 index;

    if (out_length != 0)
    {
        *out_length = 0u;
    }
    if (out_checksum != 0)
    {
        *out_checksum = checksum;
    }
    if ((data == 0) || (max_bytes == 0u))
    {
        return 0u;
    }

    for (index = 0u; index < max_bytes; ++index)
    {
        u8 value = data[index];
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
        checksum = linux_dynamic64_mix_checksum(checksum, value);
    }

    return 0u;
}

static u32 linux_dynamic64_vaddr_to_file_offset(
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 vaddr,
    u64 byte_count,
    u64 *out_offset)
{
    u32 index;

    if (out_offset != 0)
    {
        *out_offset = 0ull;
    }
    if ((phdrs == 0) || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        return 0u;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 load_end;

        if (phdr->type != ELF64_PT_LOAD)
        {
            continue;
        }
        load_end = phdr->vaddr + phdr->filesz;
        if ((load_end < phdr->vaddr)
            || (vaddr < phdr->vaddr)
            || ((vaddr + byte_count) < vaddr)
            || ((vaddr + byte_count) > load_end))
        {
            continue;
        }
        if (out_offset != 0)
        {
            *out_offset = phdr->offset + (vaddr - phdr->vaddr);
        }
        return 1u;
    }

    return 0u;
}

static u64 linux_dynamic64_phdr_vaddr(
    const elf64_header_t *header,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 load_bias)
{
    u32 index;

    if ((header == 0) || (phdrs == 0))
    {
        return 0ull;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        if (phdr->type != ELF64_PT_LOAD)
        {
            continue;
        }
        if ((header->phoff >= phdr->offset)
            && (header->phoff < (phdr->offset + phdr->filesz)))
        {
            return load_bias + phdr->vaddr + (header->phoff - phdr->offset);
        }
    }

    return 0ull;
}

static u32 linux_dynamic64_auxv_add(elf64_auxv_t *auxv, u64 type, u64 value)
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

static u32 linux_dynamic64_build_compact_auxv(
    u32 pid,
    u64 entry,
    u64 phdr_vaddr,
    u32 phnum,
    u64 interp_base,
    elf64_auxv_t *out_auxv)
{
    u32 index;
    u32 ok = 1u;
    u64 state;

    if (out_auxv == 0)
    {
        return ELF64_DENIED;
    }

    for (index = 0u; index < ELF64_AUXV_MAX_ENTRIES; ++index)
    {
        out_auxv->entries[index].type = 0ull;
        out_auxv->entries[index].value = 0ull;
    }
    out_auxv->entry_count = 0u;
    out_auxv->random_byte_count = ELF64_AUX_RANDOM_BYTES;
    out_auxv->random_staging_address = (u64)(void *)&out_auxv->random[0];
    out_auxv->platform_staging_address = (u64)(void *)&out_auxv->platform[0];
    out_auxv->platform[0] = (u8)'x';
    out_auxv->platform[1] = (u8)'8';
    out_auxv->platform[2] = (u8)'6';
    out_auxv->platform[3] = (u8)'_';
    out_auxv->platform[4] = (u8)'6';
    out_auxv->platform[5] = (u8)'4';
    out_auxv->platform[6] = 0u;
    out_auxv->platform[7] = 0u;
    state = 0x4C445F4155585631ull
        ^ (u64)pid
        ^ entry
        ^ phdr_vaddr
        ^ interp_base
        ^ (u64)pit_get_ticks();
    for (index = 0u; index < ELF64_AUX_RANDOM_BYTES; ++index)
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        out_auxv->random[index] = (u8)(state >> ((index & 7u) * 8u));
    }
    out_auxv->random_checksum = linux_dynamic64_checksum_bytes(
        out_auxv->random,
        ELF64_AUX_RANDOM_BYTES);
    out_auxv->platform_checksum = linux_dynamic64_checksum_bytes(out_auxv->platform, 7u);

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (entry == 0ull)
        || (phdr_vaddr == 0ull)
        || (phnum == 0u)
        || (phnum > ELF64_MAX_PROGRAM_HEADERS))
    {
        out_auxv->error = ELF64_ERROR_AUX_ARGUMENT;
        return ELF64_DENIED;
    }

    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_PHDR, phdr_vaddr);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_PHENT, (u64)ELF64_PHDR_BYTES);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_PHNUM, (u64)phnum);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_PAGESZ, (u64)VMA64_PAGE_BYTES);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_BASE, interp_base);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_FLAGS, 0ull);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_ENTRY, entry);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_UID, ELF64_AUX_DEFAULT_UID);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_EUID, ELF64_AUX_DEFAULT_UID);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_GID, ELF64_AUX_DEFAULT_GID);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_EGID, ELF64_AUX_DEFAULT_GID);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_HWCAP, ELF64_AUX_HWCAP_BASELINE);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_CLKTCK, (u64)pit_get_frequency_hz());
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_SECURE, 0ull);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_RANDOM, out_auxv->random_staging_address);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_HWCAP2, ELF64_AUX_HWCAP2_BASELINE);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_PLATFORM, out_auxv->platform_staging_address);
    ok &= linux_dynamic64_auxv_add(out_auxv, ELF64_AT_NULL, 0ull);

    out_auxv->error = (ok != 0u) ? ELF64_ERROR_NONE : ELF64_ERROR_AUX_CAPACITY;
    return (ok != 0u) ? ELF64_OK : ELF64_DENIED;
}

static u32 linux_dynamic64_record_app_mappings(
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 load_bias,
    linux_dynamic64_launch_result_t *result)
{
    u32 index;
    u32 count = 0u;

    if ((phdrs == 0) || (result == 0) || (phdr_count > ELF64_MAX_PROGRAM_HEADERS))
    {
        return 0u;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 load_vaddr;
        u64 map_base;
        u64 map_offset;
        u64 map_bytes;

        if ((phdr->type != ELF64_PT_LOAD) || (phdr->memsz == 0ull))
        {
            continue;
        }
        load_vaddr = load_bias + phdr->vaddr;
        if (load_vaddr < phdr->vaddr)
        {
            return 0u;
        }
        map_base = linux_dynamic64_align_down(load_vaddr, VMA64_PAGE_BYTES);
        map_offset = load_vaddr - map_base;
        map_bytes = linux_dynamic64_align_up(map_offset + phdr->memsz, VMA64_PAGE_BYTES);
        if ((map_bytes == 0ull) || ((map_base + map_bytes) < map_base))
        {
            return 0u;
        }
        result->app_mapped_bases[count] = map_base;
        result->app_mapped_lengths[count] = map_bytes;
        ++count;
    }

    result->app_mapped_count = count;
    return 1u;
}

static u32 linux_dynamic64_extract_interp(
    const u8 *binary_data,
    u32 binary_size,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u32 *path_bytes_out,
    u32 *path_checksum_out)
{
    u32 index;

    if (path_bytes_out != 0)
    {
        *path_bytes_out = 0u;
    }
    if (path_checksum_out != 0)
    {
        *path_checksum_out = 0u;
    }
    if ((binary_data == 0) || (phdrs == 0))
    {
        return 0u;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u32 length;
        u32 checksum;

        if (phdr->type != ELF64_PT_INTERP)
        {
            continue;
        }
        if ((phdr->filesz == 0ull)
            || (phdr->filesz > LINUX_DYNAMIC64_INTERP_PATH_MAX)
            || (linux_dynamic64_range_available(binary_size, phdr->offset, phdr->filesz) == 0u))
        {
            return 0u;
        }
        if (linux_dynamic64_string_checksum(
                binary_data + phdr->offset,
                (u32)phdr->filesz,
                &length,
                &checksum) == 0u)
        {
            return 0u;
        }
        if (linux_dynamic64_interpreter_path_supported(binary_data + phdr->offset, length) == 0u)
        {
            return 0u;
        }
        if (path_bytes_out != 0)
        {
            *path_bytes_out = length;
        }
        if (path_checksum_out != 0)
        {
            *path_checksum_out = checksum;
        }
        return 1u;
    }

    return 0u;
}

void linux_dynamic64_init(void)
{
    linux_dynamic64_build_image();
}

u32 linux_dynamic64_load_interpreter(
    u32 pid,
    u64 image_base,
    linux_dynamic64_load_result_t *out_result)
{
    elf64_program_header_t phdrs[ELF64_MAX_PROGRAM_HEADERS];
    persona_context_t *context;
    u32 index;

    linux_dynamic64_clear_load(out_result);
    linux_dynamic64_init();

    if (out_result == 0)
    {
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_NULL, 0ull);
    }
    out_result->file_bytes = LINUX_DYNAMIC64_FILE_BYTES;
    out_result->image_bytes = LINUX_DYNAMIC64_IMAGE_BYTES;

    if (linux_dynamic64_valid_persona(pid) == 0u)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_PERSONA;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_PERSONA, 0ull);
    }
    if ((image_base == 0ull) || ((image_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_BASE;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_BASE, 0ull);
    }
    if ((vma64_find(pid, image_base + LINUX_DYNAMIC64_TEXT_RVA) != 0)
        || (vma64_find(pid, image_base + LINUX_DYNAMIC64_RODATA_RVA) != 0))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_ALREADY_MAPPED;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_ALREADY_MAPPED, 0ull);
    }

    if (elf64_parse_header(
            g_linux_dynamic64_image,
            LINUX_DYNAMIC64_FILE_BYTES,
            &out_result->header) != ELF64_OK)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_HEADER;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_HEADER, 0ull);
    }
    if ((out_result->header.type != ELF64_TYPE_DYN)
        || (elf64_parse_phdrs(
                g_linux_dynamic64_image,
                LINUX_DYNAMIC64_FILE_BYTES,
                &out_result->header,
                phdrs,
                ELF64_MAX_PROGRAM_HEADERS,
                &out_result->phdr_summary) != ELF64_OK)
        || (out_result->phdr_summary.load_count != 1u)
        || (out_result->phdr_summary.dynamic_count != 1u))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_PHDR;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_PHDR, 0ull);
    }

    if (elf64_map_load_segments(
            pid,
            phdrs,
            out_result->header.phnum,
            g_linux_dynamic64_image,
            LINUX_DYNAMIC64_FILE_BYTES,
            image_base,
            &out_result->load_result) != ELF64_OK)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_MAP;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_MAP, 0ull);
    }

    out_result->image_base = image_base;
    out_result->image_end = image_base + LINUX_DYNAMIC64_IMAGE_BYTES;
    out_result->symbol_count = LINUX_DYNAMIC64_SYMBOL_COUNT;
    out_result->dl_start = image_base + LINUX_DYNAMIC64_RVA_DL_START;
    out_result->dl_map_object = image_base + LINUX_DYNAMIC64_RVA_DL_MAP_OBJECT;
    out_result->dl_bind_now = image_base + LINUX_DYNAMIC64_RVA_DL_BIND_NOW;
    out_result->dl_runtime_resolve = image_base + LINUX_DYNAMIC64_RVA_DL_RUNTIME_RESOLVE;
    out_result->dl_rtld_lock = image_base + LINUX_DYNAMIC64_RVA_DL_RTLD_LOCK;
    out_result->dl_rtld_unlock = image_base + LINUX_DYNAMIC64_RVA_DL_RTLD_UNLOCK;
    out_result->dlopen_fn = image_base + LINUX_DYNAMIC64_RVA_DLOPEN;
    out_result->dlclose_fn = image_base + LINUX_DYNAMIC64_RVA_DLCLOSE;
    out_result->dlsym_fn = image_base + LINUX_DYNAMIC64_RVA_DLSYM;
    out_result->dlerror_fn = image_base + LINUX_DYNAMIC64_RVA_DLERROR;
    out_result->image_checksum = linux_dynamic64_checksum_bytes(
        g_linux_dynamic64_image,
        LINUX_DYNAMIC64_FILE_BYTES);
    out_result->text_checksum = linux_dynamic64_checksum_bytes(
        g_linux_dynamic64_image + LINUX_DYNAMIC64_TEXT_FILE_OFFSET,
        LINUX_DYNAMIC64_TEXT_FILE_BYTES);
    out_result->rodata_checksum = linux_dynamic64_checksum_bytes(
        g_linux_dynamic64_image + LINUX_DYNAMIC64_RODATA_FILE_OFFSET,
        LINUX_DYNAMIC64_RODATA_FILE_BYTES);
    out_result->name_checksum = linux_dynamic64_checksum_bytes(
        g_linux_dynamic64_image + LINUX_DYNAMIC64_RODATA_FILE_OFFSET,
        (u32)sizeof("ld-limitless.so") - 1u);
    out_result->text_protection = paging64_user_page_protection(image_base + LINUX_DYNAMIC64_TEXT_RVA);
    out_result->rodata_protection = paging64_user_page_protection(image_base + LINUX_DYNAMIC64_RODATA_RVA);

    context = persona64_context_for_process(pid);
    if (context != 0)
    {
        context->linux_dynamic_base = image_base;
        context->linux_dynamic_dl_start = out_result->dl_start;
        context->linux_dynamic_dlsym = out_result->dlsym_fn;
        context->linux_dynamic_dlerror = out_result->dlerror_fn;
        context->linux_dynamic_symbol_count = LINUX_DYNAMIC64_SYMBOL_COUNT;
        context->linux_dynamic_checksum = out_result->image_checksum;
        out_result->context_stored = 1u;
    }

    for (index = 0u; index < LINUX_DYNAMIC64_SYMBOL_COUNT; ++index)
    {
        if (linux_dynamic64_export(pid, g_linux_dynamic64_exports[index].name) == 0ull)
        {
            out_result->error = LINUX_DYNAMIC64_ERROR_SYMBOL;
            return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_SYMBOL, 0ull);
        }
    }

    ++g_linux_dynamic64_load_count;
    g_linux_dynamic64_last_error = LINUX_DYNAMIC64_ERROR_NONE;
    out_result->error = LINUX_DYNAMIC64_ERROR_NONE;
    return LINUX_DYNAMIC64_OK;
}

u32 linux_dynamic64_analyze_needed(
    const u8 *binary_data,
    u32 binary_size,
    const elf64_header_t *header,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    linux_dynamic64_needed_result_t *out_result)
{
    u64 needed_offsets[LINUX_DYNAMIC64_MAX_NEEDED];
    u64 strtab_vaddr = 0ull;
    u64 strtab_size = 0ull;
    u64 strtab_file_offset = 0ull;
    u32 index;

    linux_dynamic64_clear_needed(out_result);
    if ((binary_data == 0) || (header == 0) || (phdrs == 0) || (out_result == 0))
    {
        if (out_result != 0)
        {
            out_result->error = LINUX_DYNAMIC64_ERROR_NULL;
        }
        return LINUX_DYNAMIC64_DENIED;
    }

    for (index = 0u; index < LINUX_DYNAMIC64_MAX_NEEDED; ++index)
    {
        needed_offsets[index] = 0ull;
    }

    for (index = 0u; index < phdr_count; ++index)
    {
        const elf64_program_header_t *phdr = &phdrs[index];
        u64 cursor;
        u64 end;

        if (phdr->type != ELF64_PT_DYNAMIC)
        {
            continue;
        }
        if ((phdr->filesz == 0ull)
            || ((phdr->filesz & 0x0Full) != 0ull)
            || (linux_dynamic64_range_available(binary_size, phdr->offset, phdr->filesz) == 0u))
        {
            out_result->error = LINUX_DYNAMIC64_ERROR_PHDR;
            return LINUX_DYNAMIC64_DENIED;
        }
        out_result->dynamic_found = 1u;
        cursor = phdr->offset;
        end = phdr->offset + phdr->filesz;
        while (cursor < end)
        {
            u64 tag = linux_dynamic64_read_le64(binary_data + cursor);
            u64 value = linux_dynamic64_read_le64(binary_data + cursor + 8u);

            if (tag == LINUX_DYNAMIC64_DT_NULL)
            {
                break;
            }
            if (tag == LINUX_DYNAMIC64_DT_STRTAB)
            {
                strtab_vaddr = value;
                out_result->strtab_found = 1u;
            }
            else if (tag == LINUX_DYNAMIC64_DT_STRSZ)
            {
                strtab_size = value;
            }
            else if (tag == LINUX_DYNAMIC64_DT_NEEDED)
            {
                if (out_result->needed_count < LINUX_DYNAMIC64_MAX_NEEDED)
                {
                    needed_offsets[out_result->needed_count] = value;
                }
                ++out_result->needed_count;
            }
            cursor += 16ull;
        }
    }

    if (out_result->dynamic_found == 0u)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_NOT_DYNAMIC;
        return LINUX_DYNAMIC64_DENIED;
    }

    if (out_result->needed_count == 0u)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_NONE;
        return LINUX_DYNAMIC64_OK;
    }
    if ((out_result->strtab_found == 0u)
        || (strtab_size == 0ull)
        || (strtab_size > 4096ull)
        || (linux_dynamic64_vaddr_to_file_offset(
                phdrs,
                phdr_count,
                strtab_vaddr,
                strtab_size,
                &strtab_file_offset) == 0u)
        || (linux_dynamic64_range_available(binary_size, strtab_file_offset, strtab_size) == 0u))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_DEPENDENCY;
        return LINUX_DYNAMIC64_DENIED;
    }

    out_result->strtab_file_offset = (u32)strtab_file_offset;
    out_result->strtab_size = (u32)strtab_size;

    for (index = 0u; (index < out_result->needed_count) && (index < LINUX_DYNAMIC64_MAX_NEEDED); ++index)
    {
        u64 needed_offset = needed_offsets[index];
        u32 length = 0u;
        u32 checksum = 0u;

        if ((needed_offset >= strtab_size)
            || (linux_dynamic64_string_checksum(
                    binary_data + strtab_file_offset + needed_offset,
                    (u32)(strtab_size - needed_offset),
                    &length,
                    &checksum) == 0u))
        {
            ++out_result->missing_count;
            continue;
        }
        if (out_result->first_needed_checksum == 0u)
        {
            out_result->first_needed_checksum = checksum;
        }
        out_result->last_needed_checksum = checksum;
        if (linux_dynamic64_name_matches(
                (const char *)(binary_data + strtab_file_offset + needed_offset),
                length,
                "ld-limitless.so",
                (u32)sizeof("ld-limitless.so") - 1u) != 0u)
        {
            ++out_result->self_needed_count;
            ++out_result->supported_count;
        }
        else if (linux_libc64_pthread_dependency_supported(
                (const char *)(binary_data + strtab_file_offset + needed_offset),
                length) != 0u)
        {
            ++out_result->pthread_needed_count;
            ++out_result->supported_count;
        }
        else if (linux_libc64_dependency_supported(
                (const char *)(binary_data + strtab_file_offset + needed_offset),
                length) != 0u)
        {
            ++out_result->libc_needed_count;
            ++out_result->supported_count;
        }
        else
        {
            ++out_result->missing_count;
        }
    }

    out_result->error = LINUX_DYNAMIC64_ERROR_NONE;
    return LINUX_DYNAMIC64_OK;
}

u32 linux_dynamic64_prepare(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    u64 app_load_bias,
    u64 interpreter_base,
    u64 stack_base,
    u32 stack_bytes,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    linux_dynamic64_launch_result_t *out_result)
{
    elf64_program_header_t phdrs[ELF64_MAX_PROGRAM_HEADERS];
    persona_context_t *context;

    linux_dynamic64_clear_launch(out_result);
    if (out_result == 0)
    {
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_NULL, 0ull);
    }
    out_result->app_load_bias = app_load_bias;
    out_result->interpreter_base = interpreter_base;
    out_result->stack_base = stack_base;
    out_result->stack_top = stack_base + (u64)stack_bytes;

    if ((binary_data == 0) || (binary_size == 0u) || (stack_bytes == 0u))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_NULL;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_NULL, 0ull);
    }
    if (linux_dynamic64_valid_persona(pid) == 0u)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_PERSONA;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_PERSONA, 0ull);
    }
    if ((interpreter_base == 0ull)
        || ((interpreter_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || (stack_base == 0ull)
        || ((stack_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((stack_base + (u64)stack_bytes) <= stack_base))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_BASE;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_BASE, 0ull);
    }

    if (elf64_parse_header(binary_data, binary_size, &out_result->app_header) != ELF64_OK)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_HEADER;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_HEADER, 0ull);
    }
    if (elf64_parse_phdrs(
            binary_data,
            binary_size,
            &out_result->app_header,
            phdrs,
            ELF64_MAX_PROGRAM_HEADERS,
            &out_result->app_phdr_summary) != ELF64_OK)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_PHDR;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_PHDR, 0ull);
    }
    if ((out_result->app_phdr_summary.load_count == 0u)
        || (out_result->app_phdr_summary.interp_count != 1u)
        || (out_result->app_phdr_summary.dynamic_count != 1u))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_NOT_DYNAMIC;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_NOT_DYNAMIC, 0ull);
    }
    if (linux_dynamic64_extract_interp(
            binary_data,
            binary_size,
            phdrs,
            out_result->app_header.phnum,
            &out_result->interp_path_bytes,
            &out_result->interp_path_checksum) == 0u)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_INTERP;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_INTERP, 0ull);
    }
    if (linux_dynamic64_analyze_needed(
            binary_data,
            binary_size,
            &out_result->app_header,
            phdrs,
            out_result->app_header.phnum,
            &out_result->needed_result) != LINUX_DYNAMIC64_OK)
    {
        out_result->error = out_result->needed_result.error;
        return linux_dynamic64_record_denial(pid, out_result->error, 0ull);
    }
    if (out_result->needed_result.missing_count != 0u)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_DEPENDENCY;
        g_linux_dynamic64_last_needed_count = out_result->needed_result.needed_count;
        g_linux_dynamic64_last_missing_count = out_result->needed_result.missing_count;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_DEPENDENCY, 0ull);
    }

    if ((linux_dynamic64_record_app_mappings(
            phdrs,
            out_result->app_header.phnum,
            app_load_bias,
            out_result) == 0u)
        || (elf64_map_load_segments(
                pid,
                phdrs,
                out_result->app_header.phnum,
                binary_data,
                binary_size,
                app_load_bias,
                &out_result->app_load_result) != ELF64_OK))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_MAP;
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_MAP, 0ull);
    }
    if (elf64_apply_gnu_relro(
            pid,
            phdrs,
            out_result->app_header.phnum,
            app_load_bias,
            &out_result->app_relro_result) != ELF64_OK)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_RELRO;
        (void)linux_dynamic64_release_launch(pid, out_result);
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_RELRO, 0ull);
    }
    if (linux_dynamic64_load_interpreter(
            pid,
            interpreter_base,
            &out_result->interpreter_result) != LINUX_DYNAMIC64_OK)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_MAP;
        (void)linux_dynamic64_release_launch(pid, out_result);
        return LINUX_DYNAMIC64_DENIED;
    }
    if ((out_result->needed_result.libc_needed_count != 0u)
        || (out_result->needed_result.pthread_needed_count != 0u))
    {
        out_result->libc_required = 1u;
        out_result->pthread_required =
            (out_result->needed_result.pthread_needed_count != 0u) ? 1u : 0u;
        if (linux_libc64_load(
                pid,
                LINUX_LIBC64_DEFAULT_BASE,
                &out_result->libc_result) != LINUX_LIBC64_OK)
        {
            out_result->error = LINUX_DYNAMIC64_ERROR_DEPENDENCY;
            (void)linux_dynamic64_release_launch(pid, out_result);
            return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_DEPENDENCY, 0ull);
        }
        out_result->libc_mapped = 1u;
        out_result->pthread_mapped = out_result->pthread_required;
    }
    if (vma64_map_anon(
            pid,
            stack_base,
            stack_bytes,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS) != stack_base)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_STACK;
        (void)linux_dynamic64_release_launch(pid, out_result);
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_STACK, 0ull);
    }

    out_result->app_entry = app_load_bias + out_result->app_header.entry;
    out_result->app_phdr_vaddr = linux_dynamic64_phdr_vaddr(
        &out_result->app_header,
        phdrs,
        out_result->app_header.phnum,
        app_load_bias);
    if ((out_result->app_entry < out_result->app_header.entry)
        || (out_result->app_phdr_vaddr == 0ull)
        || (linux_dynamic64_build_compact_auxv(
                pid,
                out_result->app_entry,
                out_result->app_phdr_vaddr,
                (u32)out_result->app_header.phnum,
                interpreter_base,
                &out_result->auxv) != ELF64_OK))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_AUXV;
        (void)linux_dynamic64_release_launch(pid, out_result);
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_AUXV, 0ull);
    }
    if (elf64_build_initial_stack(
            pid,
            stack_base,
            stack_base + (u64)stack_bytes,
            argc,
            argv,
            envc,
            envp,
            &out_result->auxv,
            &out_result->stack_result) != ELF64_OK)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_STACK;
        (void)linux_dynamic64_release_launch(pid, out_result);
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_STACK, 0ull);
    }
    if ((out_result->libc_mapped != 0u)
        && (envc != 0u)
        && (linux_libc64_bind_environment(
                pid,
                out_result->stack_result.envp_address,
                envc,
                envp) != LINUX_LIBC64_OK))
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_DEPENDENCY;
        (void)linux_dynamic64_release_launch(pid, out_result);
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_DEPENDENCY, 0ull);
    }

    out_result->initial_rsp = out_result->stack_result.initial_rsp;
    out_result->transfer_rip = out_result->interpreter_result.dl_start;
    out_result->transfer_rsp = out_result->initial_rsp;
    out_result->transfer_ready =
        ((paging64_user_page_present(out_result->transfer_rip & ~((u64)VMA64_PAGE_BYTES - 1ull)) != 0u)
            && ((paging64_user_page_protection(out_result->transfer_rip & ~((u64)VMA64_PAGE_BYTES - 1ull))
                    & PAGING64_USER_PROT_EXECUTE) != 0u)
            && (paging64_user_page_present(stack_base) != 0u)
            && ((paging64_user_page_protection(stack_base) & PAGING64_USER_PROT_WRITE) != 0u))
            ? 1u
            : 0u;
    if (out_result->transfer_ready == 0u)
    {
        out_result->error = LINUX_DYNAMIC64_ERROR_TRANSFER;
        (void)linux_dynamic64_release_launch(pid, out_result);
        return linux_dynamic64_record_denial(pid, LINUX_DYNAMIC64_ERROR_TRANSFER, 0ull);
    }

    context = persona64_context_for_process(pid);
    if (context != 0)
    {
        context->linux_dynamic_needed_count = out_result->needed_result.needed_count;
        context->linux_dynamic_missing_count = out_result->needed_result.missing_count;
    }

    ++g_linux_dynamic64_prepare_count;
    g_linux_dynamic64_last_error = LINUX_DYNAMIC64_ERROR_NONE;
    g_linux_dynamic64_last_needed_count = out_result->needed_result.needed_count;
    g_linux_dynamic64_last_missing_count = out_result->needed_result.missing_count;
    out_result->error = LINUX_DYNAMIC64_ERROR_NONE;
    return LINUX_DYNAMIC64_OK;
}

u64 linux_dynamic64_export(u32 pid, const char *name)
{
    persona_context_t *context;
    u32 length;
    u32 index;
    u64 base;

    if (name == 0)
    {
        return 0ull;
    }

    context = persona64_context_for_process(pid);
    base = (context != 0) ? context->linux_dynamic_base : 0ull;
    if (base == 0ull)
    {
        return 0ull;
    }

    length = linux_dynamic64_cstring_length(name, LINUX_DYNAMIC64_STRING_LIMIT);
    for (index = 0u; index < LINUX_DYNAMIC64_SYMBOL_COUNT; ++index)
    {
        if (linux_dynamic64_name_matches(
                name,
                length,
                g_linux_dynamic64_exports[index].name,
                g_linux_dynamic64_exports[index].length) != 0u)
        {
            return base + (u64)g_linux_dynamic64_exports[index].rva;
        }
    }

    return 0ull;
}

u32 linux_dynamic64_symbol_supported(const char *name, u32 length)
{
    u32 index;

    if ((name == 0) || (length == 0u) || (length > LINUX_DYNAMIC64_STRING_LIMIT))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_DYNAMIC64_SYMBOL_COUNT; ++index)
    {
        if (linux_dynamic64_name_matches(
                name,
                length,
                g_linux_dynamic64_exports[index].name,
                g_linux_dynamic64_exports[index].length) != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

u32 linux_dynamic64_symbol_default_address(
    const char *name,
    u32 length,
    u64 *out_address)
{
    u32 index;

    if (out_address != 0)
    {
        *out_address = 0ull;
    }
    if ((name == 0)
        || (length == 0u)
        || (length > LINUX_DYNAMIC64_STRING_LIMIT)
        || (out_address == 0))
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_DYNAMIC64_SYMBOL_COUNT; ++index)
    {
        if (linux_dynamic64_name_matches(
                name,
                length,
                g_linux_dynamic64_exports[index].name,
                g_linux_dynamic64_exports[index].length) != 0u)
        {
            *out_address =
                LINUX_DYNAMIC64_DEFAULT_BASE + (u64)g_linux_dynamic64_exports[index].rva;
            return 1u;
        }
    }

    return 0u;
}

u32 linux_dynamic64_release_process(u32 pid)
{
    persona_context_t *context;
    u64 base;
    u32 released = 0u;

    context = persona64_context_for_process(pid);
    if (context == 0)
    {
        return 0u;
    }

    base = context->linux_dynamic_base;
    if (base != 0ull)
    {
        if (vma64_find(pid, base + LINUX_DYNAMIC64_TEXT_RVA) != 0)
        {
            released += vma64_unmap(pid, base + LINUX_DYNAMIC64_TEXT_RVA, VMA64_PAGE_BYTES);
        }
        if (((LINUX_DYNAMIC64_RODATA_RVA & ~((u64)VMA64_PAGE_BYTES - 1ull))
                != (LINUX_DYNAMIC64_TEXT_RVA & ~((u64)VMA64_PAGE_BYTES - 1ull)))
            && (vma64_find(pid, base + LINUX_DYNAMIC64_RODATA_RVA) != 0))
        {
            released += vma64_unmap(pid, base + LINUX_DYNAMIC64_RODATA_RVA, VMA64_PAGE_BYTES);
        }
    }

    context->linux_dynamic_base = 0ull;
    context->linux_dynamic_dl_start = 0ull;
    context->linux_dynamic_dlsym = 0ull;
    context->linux_dynamic_dlerror = 0ull;
    context->linux_dynamic_symbol_count = 0u;
    context->linux_dynamic_checksum = 0u;
    context->linux_dynamic_needed_count = 0u;
    context->linux_dynamic_missing_count = 0u;
    return released;
}

u32 linux_dynamic64_release_launch(u32 pid, const linux_dynamic64_launch_result_t *result)
{
    u32 index;
    u32 released = 0u;

    if ((pid == PROCESS64_INVALID_PID) || (result == 0))
    {
        return 0u;
    }

    if (elf64_auxv_has_type(&result->auxv, ELF64_AT_SYSINFO_EHDR) != 0u)
    {
        if (vma64_find(pid, LINUX_VDSO64_BASE) != 0)
        {
            released += vma64_unmap(pid, LINUX_VDSO64_BASE, VMA64_PAGE_BYTES);
        }
    }
    if ((result->stack_base != 0ull) && (vma64_find(pid, result->stack_base) != 0))
    {
        released += vma64_unmap(
            pid,
            result->stack_base,
            result->stack_top - result->stack_base);
    }
    for (index = 0u; index < result->app_mapped_count; ++index)
    {
        if ((result->app_mapped_bases[index] != 0ull)
            && (vma64_find(pid, result->app_mapped_bases[index]) != 0))
        {
            released += vma64_unmap(
                pid,
                result->app_mapped_bases[index],
                result->app_mapped_lengths[index]);
        }
    }
    released += linux_libc64_release_process(pid);
    released += linux_dynamic64_release_process(pid);
    return released;
}

const u8 *linux_dynamic64_interpreter_image(void)
{
    linux_dynamic64_init();
    return g_linux_dynamic64_image;
}

u32 linux_dynamic64_interpreter_file_bytes(void)
{
    return LINUX_DYNAMIC64_FILE_BYTES;
}

u32 linux_dynamic64_symbol_count(void)
{
    return LINUX_DYNAMIC64_SYMBOL_COUNT;
}

u32 linux_dynamic64_load_count(void)
{
    return g_linux_dynamic64_load_count;
}

u32 linux_dynamic64_prepare_count(void)
{
    return g_linux_dynamic64_prepare_count;
}

u32 linux_dynamic64_denial_count(void)
{
    return g_linux_dynamic64_denial_count;
}

u32 linux_dynamic64_dependency_denial_count(void)
{
    return g_linux_dynamic64_dependency_denial_count;
}

u32 linux_dynamic64_last_error(void)
{
    return g_linux_dynamic64_last_error;
}

u32 linux_dynamic64_last_needed_count(void)
{
    return g_linux_dynamic64_last_needed_count;
}

u32 linux_dynamic64_last_missing_count(void)
{
    return g_linux_dynamic64_last_missing_count;
}
