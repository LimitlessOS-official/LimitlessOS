#include "linux_vdso_x64.h"

#include "arch_build.h"
#include "paging_x64.h"
#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"
#include "vma_x64.h"

/*
 * H.1 adds the first Linux persona VDSO foundation. It integrates with
 * vma_x64.h and paging_x64.h to map one read/execute user page, with
 * persona_x64.h to restrict the map to Linux ELF persona processes, and with
 * pit.h for the syscall-free clock fast path. The scaffold checkpoint proves
 * the page is a real ELF image with PT_LOAD/PT_DYNAMIC metadata, is not
 * writable after sealing, rejects duplicate/invalid maps, and can produce a
 * clock value without touching the Linux syscall audit/counter path.
 */

#ifdef LIMITLESS_X64_UEFI_KERNEL

#define LINUX_VDSO64_ELF_TYPE_DYN 3u
#define LINUX_VDSO64_ELF_MACHINE_X86_64 62u
#define LINUX_VDSO64_ELF_CLASS_64 2u
#define LINUX_VDSO64_ELF_DATA_LSB 1u
#define LINUX_VDSO64_ELF_VERSION_CURRENT 1u
#define LINUX_VDSO64_PT_LOAD 1u
#define LINUX_VDSO64_PT_DYNAMIC 2u
#define LINUX_VDSO64_PF_X 0x00000001u
#define LINUX_VDSO64_PF_R 0x00000004u
#define LINUX_VDSO64_DYNAMIC_OFFSET 0x00000100u
#define LINUX_VDSO64_DYNAMIC_BYTES 0x00000010u

static u32 g_linux_vdso64_initialized = 0u;
static u32 g_linux_vdso64_map_count = 0u;
static u32 g_linux_vdso64_fast_clock_count = 0u;
static u32 g_linux_vdso64_fast_clock_fault_count = 0u;
static u32 g_linux_vdso64_fast_clock_denial_count = 0u;

static void linux_vdso64_store_u16(volatile u8 *page, u32 offset, u16 value)
{
    page[offset] = (u8)(value & 0xFFu);
    page[offset + 1u] = (u8)((value >> 8) & 0xFFu);
}

static void linux_vdso64_store_u32(volatile u8 *page, u32 offset, u32 value)
{
    page[offset] = (u8)(value & 0xFFu);
    page[offset + 1u] = (u8)((value >> 8) & 0xFFu);
    page[offset + 2u] = (u8)((value >> 16) & 0xFFu);
    page[offset + 3u] = (u8)((value >> 24) & 0xFFu);
}

static void linux_vdso64_store_u64(volatile u8 *page, u32 offset, u64 value)
{
    u32 index;

    for (index = 0u; index < 8u; ++index)
    {
        page[offset + index] = (u8)(value >> (index * 8u));
    }
}

static u16 linux_vdso64_load_u16(const volatile u8 *page, u32 offset)
{
    return (u16)((u16)page[offset] | ((u16)page[offset + 1u] << 8));
}

static u32 linux_vdso64_load_u32(const volatile u8 *page, u32 offset)
{
    return (u32)page[offset]
        | ((u32)page[offset + 1u] << 8)
        | ((u32)page[offset + 2u] << 16)
        | ((u32)page[offset + 3u] << 24);
}

static void linux_vdso64_write_text(volatile u8 *page, u32 offset)
{
    page[offset] = 0xB8u;
    page[offset + 1u] = (u8)(0u - LINUX_ABI64_ENOSYS);
    page[offset + 2u] = 0xFFu;
    page[offset + 3u] = 0xFFu;
    page[offset + 4u] = 0xFFu;
    page[offset + 5u] = 0xC3u;
}

static void linux_vdso64_write_name(volatile u8 *page, u32 *offset, const char *name)
{
    u32 index = 0u;

    if ((page == 0) || (offset == 0) || (name == 0))
    {
        return;
    }

    while ((name[index] != '\0') && (*offset < LINUX_VDSO64_PAGE_BYTES))
    {
        page[*offset] = (u8)name[index];
        ++(*offset);
        ++index;
    }
    if (*offset < LINUX_VDSO64_PAGE_BYTES)
    {
        page[*offset] = 0u;
        ++(*offset);
    }
}

static void linux_vdso64_write_phdr(
    volatile u8 *page,
    u32 offset,
    u32 type,
    u32 flags,
    u64 file_offset,
    u64 vaddr,
    u64 filesz,
    u64 memsz,
    u64 align)
{
    linux_vdso64_store_u32(page, offset, type);
    linux_vdso64_store_u32(page, offset + 4u, flags);
    linux_vdso64_store_u64(page, offset + 8u, file_offset);
    linux_vdso64_store_u64(page, offset + 16u, vaddr);
    linux_vdso64_store_u64(page, offset + 24u, 0ull);
    linux_vdso64_store_u64(page, offset + 32u, filesz);
    linux_vdso64_store_u64(page, offset + 40u, memsz);
    linux_vdso64_store_u64(page, offset + 48u, align);
}

static void linux_vdso64_write_image(volatile u8 *page)
{
    u32 index;
    u32 name_offset = LINUX_VDSO64_NAME_TABLE_OFFSET;

    if (page == 0)
    {
        return;
    }

    for (index = 0u; index < LINUX_VDSO64_PAGE_BYTES; ++index)
    {
        page[index] = 0u;
    }

    page[0] = 0x7Fu;
    page[1] = (u8)'E';
    page[2] = (u8)'L';
    page[3] = (u8)'F';
    page[4] = LINUX_VDSO64_ELF_CLASS_64;
    page[5] = LINUX_VDSO64_ELF_DATA_LSB;
    page[6] = LINUX_VDSO64_ELF_VERSION_CURRENT;
    page[7] = 3u;

    linux_vdso64_store_u16(page, 16u, LINUX_VDSO64_ELF_TYPE_DYN);
    linux_vdso64_store_u16(page, 18u, LINUX_VDSO64_ELF_MACHINE_X86_64);
    linux_vdso64_store_u32(page, 20u, LINUX_VDSO64_ELF_VERSION_CURRENT);
    linux_vdso64_store_u64(
        page,
        24u,
        LINUX_VDSO64_BASE + (u64)LINUX_VDSO64_FUNC_CLOCK_GETTIME);
    linux_vdso64_store_u64(page, 32u, LINUX_VDSO64_ELF_EHDR_BYTES);
    linux_vdso64_store_u64(page, 40u, 0ull);
    linux_vdso64_store_u32(page, 48u, 0u);
    linux_vdso64_store_u16(page, 52u, LINUX_VDSO64_ELF_EHDR_BYTES);
    linux_vdso64_store_u16(page, 54u, LINUX_VDSO64_ELF_PHDR_BYTES);
    linux_vdso64_store_u16(page, 56u, LINUX_VDSO64_PHDR_COUNT);

    linux_vdso64_write_phdr(
        page,
        LINUX_VDSO64_ELF_EHDR_BYTES,
        LINUX_VDSO64_PT_LOAD,
        LINUX_VDSO64_PF_R | LINUX_VDSO64_PF_X,
        0ull,
        0ull,
        (u64)LINUX_VDSO64_PAGE_BYTES,
        (u64)LINUX_VDSO64_PAGE_BYTES,
        (u64)LINUX_VDSO64_PAGE_BYTES);
    linux_vdso64_write_phdr(
        page,
        LINUX_VDSO64_ELF_EHDR_BYTES + LINUX_VDSO64_ELF_PHDR_BYTES,
        LINUX_VDSO64_PT_DYNAMIC,
        LINUX_VDSO64_PF_R,
        (u64)LINUX_VDSO64_DYNAMIC_OFFSET,
        (u64)LINUX_VDSO64_DYNAMIC_OFFSET,
        (u64)LINUX_VDSO64_DYNAMIC_BYTES,
        (u64)LINUX_VDSO64_DYNAMIC_BYTES,
        8ull);

    linux_vdso64_write_text(page, LINUX_VDSO64_FUNC_CLOCK_GETTIME);
    linux_vdso64_write_text(page, LINUX_VDSO64_FUNC_GETTIMEOFDAY);
    linux_vdso64_write_text(page, LINUX_VDSO64_FUNC_TIME);
    linux_vdso64_write_name(page, &name_offset, "__vdso_clock_gettime");
    linux_vdso64_write_name(page, &name_offset, "__vdso_gettimeofday");
    linux_vdso64_write_name(page, &name_offset, "__vdso_time");
}

static u32 linux_vdso64_checksum(const volatile u8 *page)
{
    u32 index;
    u32 digest = 2166136261u;

    if (page == 0)
    {
        return 0u;
    }

    for (index = 0u; index < LINUX_VDSO64_PAGE_BYTES; ++index)
    {
        digest ^= (u32)page[index];
        digest *= 16777619u;
    }

    return (digest != 0u) ? digest : 1u;
}

static u32 linux_vdso64_name_present(const volatile u8 *page, const char *name)
{
    u32 cursor;
    u32 index;

    if ((page == 0) || (name == 0))
    {
        return 0u;
    }

    for (cursor = 0u; cursor < LINUX_VDSO64_PAGE_BYTES; ++cursor)
    {
        index = 0u;
        while (((cursor + index) < LINUX_VDSO64_PAGE_BYTES)
            && (name[index] != '\0')
            && (page[cursor + index] == (u8)name[index]))
        {
            ++index;
        }
        if (name[index] == '\0')
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 linux_vdso64_user_buffer_writable(u32 pid, u64 address, u32 byte_count)
{
    u64 cursor;
    u64 end;

    if (byte_count == 0u)
    {
        return 1u;
    }
    if ((address == 0ull) || ((address + (u64)byte_count) < address))
    {
        return 0u;
    }

    cursor = address;
    end = address + (u64)byte_count;
    while (cursor < end)
    {
        vma_region_t *region = vma64_find(pid, cursor);
        u64 page = cursor & ~((u64)VMA64_PAGE_BYTES - 1ull);
        u64 next_page = page + (u64)VMA64_PAGE_BYTES;
        u64 next = (next_page < end) ? next_page : end;

        if ((region == 0)
            || (cursor < region->virt_base)
            || (cursor >= region->virt_end)
            || ((region->prot_flags & VMA64_PROT_WRITE) == 0u)
            || (paging64_user_page_present(page) == 0u)
            || ((paging64_user_page_protection(page) & PAGING64_USER_PROT_WRITE) == 0u))
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

static u32 linux_vdso64_clock_supported(u64 clock_id)
{
    return ((clock_id == (u64)LINUX_ABI64_CLOCK_REALTIME)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_MONOTONIC)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_MONOTONIC_RAW)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_REALTIME_COARSE)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_MONOTONIC_COARSE)
        || (clock_id == (u64)LINUX_ABI64_CLOCK_BOOTTIME))
        ? 1u
        : 0u;
}

static void linux_vdso64_copy_to_user(u64 user_buffer, const u8 *source, u32 byte_count)
{
    volatile u8 *target = (volatile u8 *)(u64)user_buffer;
    u32 index;

    if ((target == 0) || (source == 0))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        target[index] = source[index];
    }
}

void linux_vdso64_init(void)
{
    if (g_linux_vdso64_initialized != 0u)
    {
        return;
    }

    g_linux_vdso64_map_count = 0u;
    g_linux_vdso64_fast_clock_count = 0u;
    g_linux_vdso64_fast_clock_fault_count = 0u;
    g_linux_vdso64_fast_clock_denial_count = 0u;
    g_linux_vdso64_initialized = 1u;
}

u32 linux_vdso64_validate(u32 pid, linux_vdso64_info_t *out_info)
{
    const volatile u8 *page = (const volatile u8 *)(u64)LINUX_VDSO64_BASE;
    u32 first_phdr;
    u32 second_phdr;

    linux_vdso64_init();

    if (out_info != 0)
    {
        out_info->base = LINUX_VDSO64_BASE;
        out_info->size = LINUX_VDSO64_PAGE_BYTES;
        out_info->mapped = 0u;
        out_info->page_present = 0u;
        out_info->page_prot = 0u;
        out_info->elf_magic = 0u;
        out_info->elf_class = 0u;
        out_info->elf_data = 0u;
        out_info->elf_type = 0u;
        out_info->elf_machine = 0u;
        out_info->phdr_count = 0u;
        out_info->has_load = 0u;
        out_info->has_dynamic = 0u;
        out_info->has_clock_gettime = 0u;
        out_info->has_gettimeofday = 0u;
        out_info->has_time = 0u;
        out_info->image_checksum = 0u;
        out_info->duplicate_denied = 0u;
        out_info->invalid_pid_denied = 0u;
    }

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF)
        || (vma64_find(pid, LINUX_VDSO64_BASE) == 0)
        || (paging64_user_page_present(LINUX_VDSO64_BASE) == 0u))
    {
        return LINUX_VDSO64_MAP_DENIED;
    }

    if (out_info == 0)
    {
        return LINUX_VDSO64_MAP_OK;
    }

    first_phdr = LINUX_VDSO64_ELF_EHDR_BYTES;
    second_phdr = LINUX_VDSO64_ELF_EHDR_BYTES + LINUX_VDSO64_ELF_PHDR_BYTES;
    out_info->mapped = 1u;
    out_info->page_present = paging64_user_page_present(LINUX_VDSO64_BASE);
    out_info->page_prot = paging64_user_page_protection(LINUX_VDSO64_BASE);
    out_info->elf_magic =
        ((page[0] == 0x7Fu)
            && (page[1] == (u8)'E')
            && (page[2] == (u8)'L')
            && (page[3] == (u8)'F'))
            ? 1u
            : 0u;
    out_info->elf_class = page[4];
    out_info->elf_data = page[5];
    out_info->elf_type = (u32)linux_vdso64_load_u16(page, 16u);
    out_info->elf_machine = (u32)linux_vdso64_load_u16(page, 18u);
    out_info->phdr_count = (u32)linux_vdso64_load_u16(page, 56u);
    out_info->has_load =
        ((linux_vdso64_load_u32(page, first_phdr) == LINUX_VDSO64_PT_LOAD)
            && (linux_vdso64_load_u32(page, first_phdr + 4u)
                == (LINUX_VDSO64_PF_R | LINUX_VDSO64_PF_X)))
            ? 1u
            : 0u;
    out_info->has_dynamic =
        (linux_vdso64_load_u32(page, second_phdr) == LINUX_VDSO64_PT_DYNAMIC)
            ? 1u
            : 0u;
    out_info->has_clock_gettime = linux_vdso64_name_present(page, "__vdso_clock_gettime");
    out_info->has_gettimeofday = linux_vdso64_name_present(page, "__vdso_gettimeofday");
    out_info->has_time = linux_vdso64_name_present(page, "__vdso_time");
    out_info->image_checksum = linux_vdso64_checksum(page);
    return LINUX_VDSO64_MAP_OK;
}

u32 linux_vdso64_map(u32 pid, linux_vdso64_info_t *out_info)
{
    volatile u8 *page = (volatile u8 *)(u64)LINUX_VDSO64_BASE;
    u64 mapped;

    linux_vdso64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF)
        || (vma64_init_process(pid) == 0u)
        || (vma64_find(pid, LINUX_VDSO64_BASE) != 0)
        || (paging64_user_page_present(LINUX_VDSO64_BASE) != 0u))
    {
        if (out_info != 0)
        {
            (void)linux_vdso64_validate(pid, out_info);
        }
        return LINUX_VDSO64_MAP_DENIED;
    }

    mapped = vma64_map_anon(
        pid,
        LINUX_VDSO64_BASE,
        LINUX_VDSO64_PAGE_BYTES,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS);
    if (mapped != LINUX_VDSO64_BASE)
    {
        return LINUX_VDSO64_MAP_DENIED;
    }

    linux_vdso64_write_image(page);
    if (vma64_protect(
            pid,
            LINUX_VDSO64_BASE,
            LINUX_VDSO64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_EXECUTE) == 0u)
    {
        (void)vma64_unmap(pid, LINUX_VDSO64_BASE, LINUX_VDSO64_PAGE_BYTES);
        return LINUX_VDSO64_MAP_DENIED;
    }

    ++g_linux_vdso64_map_count;
    return linux_vdso64_validate(pid, out_info);
}

u32 linux_vdso64_unmap(u32 pid)
{
    if ((pid == PROCESS64_INVALID_PID)
        || (vma64_find(pid, LINUX_VDSO64_BASE) == 0)
        || (paging64_user_page_present(LINUX_VDSO64_BASE) == 0u))
    {
        return 0u;
    }

    return vma64_unmap(pid, LINUX_VDSO64_BASE, LINUX_VDSO64_PAGE_BYTES);
}

u64 linux_vdso64_clock_gettime_fast(u32 pid, u64 clock_id, u64 user_timespec)
{
    linux_abi64_timespec_t timespec;
    u32 ticks;
    u32 frequency;
    u32 remainder;

    linux_vdso64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (persona64_type(pid) != PERSONA64_TYPE_LINUX_ELF)
        || (linux_vdso64_validate(pid, 0) != LINUX_VDSO64_MAP_OK))
    {
        ++g_linux_vdso64_fast_clock_denial_count;
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ESRCH);
    }
    if (linux_vdso64_clock_supported(clock_id) == 0u)
    {
        ++g_linux_vdso64_fast_clock_denial_count;
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EINVAL);
    }
    if (linux_vdso64_user_buffer_writable(
            pid,
            user_timespec,
            LINUX_ABI64_TIMESPEC_BYTES) == 0u)
    {
        ++g_linux_vdso64_fast_clock_fault_count;
        return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_EFAULT);
    }

    frequency = pit_get_frequency_hz();
    if (frequency == 0u)
    {
        frequency = 100u;
    }
    ticks = pit_get_ticks();
    remainder = ticks % frequency;
    timespec.tv_sec = (u64)(ticks / frequency);
    timespec.tv_nsec = ((u64)remainder * 1000000000ull) / (u64)frequency;
    linux_vdso64_copy_to_user(
        user_timespec,
        (const u8 *)&timespec,
        LINUX_ABI64_TIMESPEC_BYTES);

    ++g_linux_vdso64_fast_clock_count;
    return 0ull;
}

u32 linux_vdso64_map_count(void)
{
    linux_vdso64_init();
    return g_linux_vdso64_map_count;
}

u32 linux_vdso64_fast_clock_count(void)
{
    linux_vdso64_init();
    return g_linux_vdso64_fast_clock_count;
}

u32 linux_vdso64_fast_clock_fault_count(void)
{
    linux_vdso64_init();
    return g_linux_vdso64_fast_clock_fault_count;
}

u32 linux_vdso64_fast_clock_denial_count(void)
{
    linux_vdso64_init();
    return g_linux_vdso64_fast_clock_denial_count;
}

#else

void linux_vdso64_init(void) {}
u32 linux_vdso64_map(u32 pid, linux_vdso64_info_t *out_info)
{
    (void)pid;
    (void)out_info;
    return LINUX_VDSO64_MAP_DENIED;
}
u32 linux_vdso64_validate(u32 pid, linux_vdso64_info_t *out_info)
{
    (void)pid;
    (void)out_info;
    return LINUX_VDSO64_MAP_DENIED;
}
u32 linux_vdso64_unmap(u32 pid)
{
    (void)pid;
    return 0u;
}
u64 linux_vdso64_clock_gettime_fast(u32 pid, u64 clock_id, u64 user_timespec)
{
    (void)pid;
    (void)clock_id;
    (void)user_timespec;
    return LINUX_ABI64_ERROR_RETURN(LINUX_ABI64_ENOSYS);
}
u32 linux_vdso64_map_count(void) { return 0u; }
u32 linux_vdso64_fast_clock_count(void) { return 0u; }
u32 linux_vdso64_fast_clock_fault_count(void) { return 0u; }
u32 linux_vdso64_fast_clock_denial_count(void) { return 0u; }

#endif
