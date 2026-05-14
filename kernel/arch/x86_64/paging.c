#include "paging_x64.h"

#include "x64.h"

#define PAGING64_KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ull
#define PAGING64_KERNEL_LINKED_OFFSET 0x00010000ull
#define PAGING64_BOOT_TABLE_BASE_FALLBACK 0x00001000ull
#ifdef LIMITLESS_X64_UEFI_KERNEL
#define PAGING64_PML4_OFFSET 0x00000000ull
#define PAGING64_PDPT_OFFSET 0x00001000ull
#define PAGING64_KERNEL_PD_OFFSET 0x0000D000ull
#define PAGING64_HIGH_PDPT_OFFSET 0x00003000ull
#define PAGING64_RUNTIME_PD_OFFSET 0x00004000ull
#define PAGING64_RUNTIME_PT_OFFSET 0x00005000ull
#define PAGING64_USER_RUNTIME_PT_OFFSET 0x00006000ull
#define PAGING64_USER_STACK_OFFSET 0x00007000ull
#define PAGING64_KERNEL_MMIO_PT_OFFSET 0x0000B000ull
#define PAGING64_APIC_MMIO_PT_OFFSET 0x0000C000ull
#else
#define PAGING64_PML4_PHYSICAL 0x00001000ull
#define PAGING64_PDPT_PHYSICAL 0x00002000ull
#define PAGING64_KERNEL_PD_PHYSICAL 0x00003000ull
#define PAGING64_HIGH_PDPT_PHYSICAL 0x00004000ull
#define PAGING64_RUNTIME_PD_PHYSICAL 0x00005000ull
#define PAGING64_RUNTIME_PT_PHYSICAL 0x00006000ull
#define PAGING64_USER_RUNTIME_PT_PHYSICAL 0x00007000ull
#define PAGING64_USER_STACK_PHYSICAL 0x00008000ull
#define PAGING64_KERNEL_MMIO_PT_PHYSICAL 0x0000C000ull
#define PAGING64_APIC_MMIO_PT_PHYSICAL 0x0000D000ull
#endif
#define PAGING64_ENTRY_COUNT 512u
#define PAGING64_PAGE_BYTES 0x00001000u
#define PAGING64_PHYSICAL_ADDRESS_MASK 0x000FFFFFFFFFF000ull
#define PAGING64_PAGE_MASK 0xFFFFFFFFFFFFF000ull
#define PAGING64_PAGE_PRESENT 0x0000000000000001ull
#define PAGING64_PAGE_WRITABLE 0x0000000000000002ull
#define PAGING64_PAGE_USER 0x0000000000000004ull
#define PAGING64_PAGE_WRITE_THROUGH 0x0000000000000008ull
#define PAGING64_PAGE_CACHE_DISABLED 0x0000000000000010ull
#define PAGING64_PAGE_LARGE 0x0000000000000080ull
#define PAGING64_PAGE_NO_EXECUTE 0x8000000000000000ull
#define PAGING64_PAGE_FLAGS_MASK 0x0000000000000FFFull
#define PAGING64_HIGH_HALF_PML4_INDEX 511u
#define PAGING64_HIGH_HALF_PDPT_INDEX 510u
#define PAGING64_EFER_MSR 0xC0000080u
#define PAGING64_EFER_NXE 0x0000000000000800ull
#define PAGING64_CPUID_EXTENDED_MAX 0x80000000u
#define PAGING64_CPUID_EXTENDED_FEATURES 0x80000001u
#define PAGING64_CPUID_EDX_NX (1u << 20)

static u32 g_runtime_mapping_installed = 0u;
static u32 g_runtime_mapping_page_count = 0u;
static u32 g_runtime_mapping_source_checksum = 0u;
static u32 g_runtime_mapping_install_token = 0u;
static u32 g_runtime_mapping_entry_probe = 0u;
static u32 g_runtime_mapping_protection_flags = 0u;
static u32 g_runtime_mapping_protection_token = 0u;
static u64 g_runtime_mapping_source_physical = 0ull;
static u32 g_user_runtime_mapping_installed = 0u;
static u32 g_user_runtime_mapping_page_count = 0u;
static u32 g_user_runtime_mapping_source_checksum = 0u;
static u32 g_user_runtime_mapping_install_token = 0u;
static u32 g_user_runtime_mapping_entry_probe = 0u;
static u32 g_user_runtime_mapping_protection_flags = 0u;
static u32 g_user_runtime_mapping_protection_token = 0u;
static u64 g_user_runtime_mapping_source_physical = 0ull;
static u32 g_user_stack_mapping_installed = 0u;
static u32 g_user_stack_mapping_protection_flags = 0u;
static u32 g_user_stack_mapping_protection_token = 0u;
static u32 g_kernel_mmio_mapping_installed = 0u;
static u32 g_kernel_mmio_mapping_install_token = 0u;
static u32 g_kernel_mmio_mapping_pml4_index = 0u;
static u32 g_kernel_mmio_mapping_pdpt_index = 0u;
static u32 g_kernel_mmio_mapping_pd_index = 0u;
static u32 g_kernel_mmio_mapping_pt_index = 0u;
static u64 g_kernel_mmio_mapping_entry_flags = 0ull;
static u32 g_kernel_mmio_mapping_nx_enabled = 0u;
static u32 g_kernel_mmio_pt_initialized = 0u;
static u64 g_paging64_kernel_physical_base = 0ull;

#ifdef LIMITLESS_X64_UEFI_KERNEL
static u64 paging64_active_root_physical(void)
{
    u64 root = read_cr3_64() & PAGING64_PAGE_MASK;

    return (root != 0ull) ? root : PAGING64_BOOT_TABLE_BASE_FALLBACK;
}

#define PAGING64_PML4_PHYSICAL (paging64_active_root_physical() + PAGING64_PML4_OFFSET)
#define PAGING64_PDPT_PHYSICAL (paging64_active_root_physical() + PAGING64_PDPT_OFFSET)
#define PAGING64_KERNEL_PD_PHYSICAL (paging64_active_root_physical() + PAGING64_KERNEL_PD_OFFSET)
#define PAGING64_HIGH_PDPT_PHYSICAL (paging64_active_root_physical() + PAGING64_HIGH_PDPT_OFFSET)
#define PAGING64_RUNTIME_PD_PHYSICAL (paging64_active_root_physical() + PAGING64_RUNTIME_PD_OFFSET)
#define PAGING64_RUNTIME_PT_PHYSICAL (paging64_active_root_physical() + PAGING64_RUNTIME_PT_OFFSET)
#define PAGING64_USER_RUNTIME_PT_PHYSICAL (paging64_active_root_physical() + PAGING64_USER_RUNTIME_PT_OFFSET)
#define PAGING64_USER_STACK_PHYSICAL (paging64_active_root_physical() + PAGING64_USER_STACK_OFFSET)
#define PAGING64_KERNEL_MMIO_PT_PHYSICAL (paging64_active_root_physical() + PAGING64_KERNEL_MMIO_PT_OFFSET)
#define PAGING64_APIC_MMIO_PT_PHYSICAL (paging64_active_root_physical() + PAGING64_APIC_MMIO_PT_OFFSET)
#endif

static u64 paging64_lower_half_alias(const void *address)
{
    return paging64_kernel_physical_alias(address);
}

void paging64_configure_kernel_physical_base(u32 kernel_load_address)
{
    if (kernel_load_address >= PAGING64_KERNEL_LINKED_OFFSET)
    {
        g_paging64_kernel_physical_base =
            ((u64)kernel_load_address) - PAGING64_KERNEL_LINKED_OFFSET;
        return;
    }

    g_paging64_kernel_physical_base = 0ull;
}

u64 paging64_kernel_physical_alias(const void *address)
{
    u64 value = (u64)address;

    if (value >= PAGING64_KERNEL_VIRTUAL_BASE)
    {
        return g_paging64_kernel_physical_base + (value - PAGING64_KERNEL_VIRTUAL_BASE);
    }

    return value;
}

static u32 paging64_page_count(u32 mapped_bytes)
{
    if ((mapped_bytes == 0u) || ((mapped_bytes & (PAGING64_PAGE_BYTES - 1u)) != 0u))
    {
        return 0u;
    }

    return mapped_bytes / PAGING64_PAGE_BYTES;
}

static u32 paging64_index(u32 address, u32 shift)
{
    return (u32)(((u64)address >> shift) & 0x1FFu);
}

static u32 paging64_index64(u64 address, u32 shift)
{
    return (u32)((address >> shift) & 0x1FFull);
}

static u32 paging64_mix_token(u32 digest, u32 value)
{
    digest ^= value;
    digest *= 16777619u;
    return digest;
}

static void paging64_cpuid(
    u32 leaf,
    u32 subleaf,
    u32 *eax_out,
    u32 *ebx_out,
    u32 *ecx_out,
    u32 *edx_out)
{
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;

    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(subleaf));

    if (eax_out != 0)
    {
        *eax_out = eax;
    }
    if (ebx_out != 0)
    {
        *ebx_out = ebx;
    }
    if (ecx_out != 0)
    {
        *ecx_out = ecx;
    }
    if (edx_out != 0)
    {
        *edx_out = edx;
    }
}

static u32 paging64_cpu_supports_nx(void)
{
    u32 max_extended_leaf;
    u32 edx;

    paging64_cpuid(PAGING64_CPUID_EXTENDED_MAX, 0u, &max_extended_leaf, 0, 0, 0);
    if (max_extended_leaf < PAGING64_CPUID_EXTENDED_FEATURES)
    {
        return 0u;
    }

    paging64_cpuid(PAGING64_CPUID_EXTENDED_FEATURES, 0u, 0, 0, 0, &edx);
    return ((edx & PAGING64_CPUID_EDX_NX) != 0u) ? 1u : 0u;
}

static u32 paging64_checksum(const u8 *bytes, u32 byte_count)
{
    u32 digest = 2166136261u;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        digest ^= bytes[index];
        digest *= 16777619u;
    }

    return (digest != 0u) ? digest : 1u;
}

static void paging64_zero_table(volatile u64 *table)
{
    u32 index;

    for (index = 0u; index < PAGING64_ENTRY_COUNT; ++index)
    {
        table[index] = 0ull;
    }
}

static void paging64_clear_user_runtime_mapping(void)
{
    g_user_runtime_mapping_installed = 0u;
    g_user_runtime_mapping_page_count = 0u;
    g_user_runtime_mapping_source_checksum = 0u;
    g_user_runtime_mapping_install_token = 0u;
    g_user_runtime_mapping_entry_probe = 0u;
    g_user_runtime_mapping_protection_flags = 0u;
    g_user_runtime_mapping_protection_token = 0u;
    g_user_runtime_mapping_source_physical = 0ull;
}

static void paging64_clear_user_stack_mapping(void)
{
    g_user_stack_mapping_installed = 0u;
    g_user_stack_mapping_protection_flags = 0u;
    g_user_stack_mapping_protection_token = 0u;
}

static void paging64_reload_active_root(void)
{
    write_cr3_64(read_cr3_64());
}

static u32 paging64_compute_protection_flags(u64 pml4e, u64 pdpte, u64 pde, u64 pte)
{
    u32 flags = 0u;
    u64 all_entries = pml4e & pdpte & pde & pte;

    if ((all_entries & PAGING64_PAGE_PRESENT) != 0ull)
    {
        flags |= PAGING64_RUNTIME_PROTECTION_READ | PAGING64_RUNTIME_PROTECTION_EXECUTE;
    }

    if ((pte & PAGING64_PAGE_WRITABLE) != 0ull)
    {
        flags |= PAGING64_RUNTIME_PROTECTION_WRITABLE;
    }
    else
    {
        flags |= PAGING64_RUNTIME_PROTECTION_VIEW_SEALED;
    }

    if ((all_entries & PAGING64_PAGE_USER) != 0ull)
    {
        flags |= PAGING64_RUNTIME_PROTECTION_USER_ACCESSIBLE;
    }
    else
    {
        flags |= PAGING64_RUNTIME_PROTECTION_SUPERVISOR_ONLY
            | PAGING64_RUNTIME_PROTECTION_VALIDATION_ONLY;
    }

    return flags;
}

static u32 paging64_compute_protection_token(
    u32 install_token,
    u32 protection_flags,
    u64 pml4e,
    u64 pdpte,
    u64 pde,
    u64 pte)
{
    u32 token = 2166136261u;

    token = paging64_mix_token(token, install_token);
    token = paging64_mix_token(token, protection_flags);
    token = paging64_mix_token(token, (u32)(pml4e & PAGING64_PAGE_FLAGS_MASK));
    token = paging64_mix_token(token, (u32)(pdpte & PAGING64_PAGE_FLAGS_MASK));
    token = paging64_mix_token(token, (u32)(pde & PAGING64_PAGE_FLAGS_MASK));
    token = paging64_mix_token(token, (u32)(pte & PAGING64_PAGE_FLAGS_MASK));
    token = paging64_mix_token(token, g_runtime_mapping_page_count);
    return (token != 0u) ? token : 1u;
}

u32 paging64_install_runtime_mapping(u32 virtual_base, const void *source, u32 mapped_bytes)
{
    volatile u64 *pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
    volatile u64 *pdpt = (volatile u64 *)(u64)PAGING64_PDPT_PHYSICAL;
    volatile u64 *runtime_pd = (volatile u64 *)(u64)PAGING64_RUNTIME_PD_PHYSICAL;
    volatile u64 *runtime_pt = (volatile u64 *)(u64)PAGING64_RUNTIME_PT_PHYSICAL;
    u32 page_count;
    u32 page_index;
    u32 pml4_index;
    u32 pdpt_index;
    u32 pd_index;
    u32 pt_index;
    u64 source_physical;
    u32 source_checksum;
    u32 install_token;
    typedef u32 (*runtime_entry_probe_fn)(void);
    runtime_entry_probe_fn entry_probe;

    g_runtime_mapping_installed = 0u;
    g_runtime_mapping_page_count = 0u;
    g_runtime_mapping_source_checksum = 0u;
    g_runtime_mapping_install_token = 0u;
    g_runtime_mapping_entry_probe = 0u;
    g_runtime_mapping_protection_flags = 0u;
    g_runtime_mapping_protection_token = 0u;
    g_runtime_mapping_source_physical = 0ull;
    paging64_clear_user_runtime_mapping();
    paging64_clear_user_stack_mapping();

    if (source == 0)
    {
        return 0u;
    }

    page_count = paging64_page_count(mapped_bytes);
    source_physical = paging64_lower_half_alias(source);
    if ((page_count == 0u)
        || (page_count > PAGING64_ENTRY_COUNT)
        || ((virtual_base & (PAGING64_PAGE_BYTES - 1u)) != 0u)
        || ((source_physical & (PAGING64_PAGE_BYTES - 1u)) != 0u))
    {
        return 0u;
    }

    pml4_index = paging64_index(virtual_base, 39u);
    pdpt_index = paging64_index(virtual_base, 30u);
    pd_index = paging64_index(virtual_base, 21u);
    pt_index = paging64_index(virtual_base, 12u);
    if ((pml4_index != 0u) || ((pt_index + page_count) > PAGING64_ENTRY_COUNT))
    {
        return 0u;
    }

    paging64_zero_table(runtime_pd);
    paging64_zero_table(runtime_pt);
    pml4[pml4_index] = (PAGING64_PDPT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    pdpt[pdpt_index] = (PAGING64_RUNTIME_PD_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    runtime_pd[pd_index] = (PAGING64_RUNTIME_PT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;

    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        runtime_pt[pt_index + page_index] =
            ((source_physical + ((u64)page_index * PAGING64_PAGE_BYTES)) & PAGING64_PAGE_MASK)
            | PAGING64_PAGE_PRESENT;
    }

    paging64_reload_active_root();

    source_checksum = paging64_checksum((const u8 *)source, mapped_bytes);
    install_token = 2166136261u;
    install_token = paging64_mix_token(install_token, virtual_base);
    install_token = paging64_mix_token(install_token, mapped_bytes);
    install_token = paging64_mix_token(install_token, page_count);
    install_token = paging64_mix_token(install_token, source_checksum);
    install_token = paging64_mix_token(install_token, pml4_index);
    install_token = paging64_mix_token(install_token, pdpt_index);
    install_token = paging64_mix_token(install_token, pd_index);
    install_token = (install_token != 0u) ? install_token : 1u;

    entry_probe = (runtime_entry_probe_fn)(u64)virtual_base;
    g_runtime_mapping_entry_probe = entry_probe();
    g_runtime_mapping_installed = 1u;
    g_runtime_mapping_page_count = page_count;
    g_runtime_mapping_source_checksum = source_checksum;
    g_runtime_mapping_install_token = install_token;
    g_runtime_mapping_protection_flags = paging64_compute_protection_flags(
        pml4[pml4_index],
        pdpt[pdpt_index],
        runtime_pd[pd_index],
        runtime_pt[pt_index]);
    g_runtime_mapping_protection_token = paging64_compute_protection_token(
        install_token,
        g_runtime_mapping_protection_flags,
        pml4[pml4_index],
        pdpt[pdpt_index],
        runtime_pd[pd_index],
        runtime_pt[pt_index]);
    g_runtime_mapping_source_physical = source_physical;
    return 1u;
}

u32 paging64_install_user_runtime_mapping(u32 virtual_base, const void *source, u32 mapped_bytes)
{
    volatile u64 *pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
    volatile u64 *pdpt = (volatile u64 *)(u64)PAGING64_PDPT_PHYSICAL;
    volatile u64 *runtime_pd = (volatile u64 *)(u64)PAGING64_RUNTIME_PD_PHYSICAL;
    volatile u64 *user_runtime_pt = (volatile u64 *)(u64)PAGING64_USER_RUNTIME_PT_PHYSICAL;
    u32 page_count;
    u32 page_index;
    u32 pml4_index;
    u32 pdpt_index;
    u32 pd_index;
    u32 pt_index;
    u64 source_physical;
    u32 source_checksum;
    u32 install_token;
    typedef u32 (*runtime_entry_probe_fn)(void);
    runtime_entry_probe_fn entry_probe;

    paging64_clear_user_runtime_mapping();

    if ((source == 0) || (g_runtime_mapping_installed == 0u))
    {
        return 0u;
    }

    page_count = paging64_page_count(mapped_bytes);
    source_physical = paging64_lower_half_alias(source);
    if ((page_count == 0u)
        || (page_count > PAGING64_ENTRY_COUNT)
        || ((virtual_base & (PAGING64_PAGE_BYTES - 1u)) != 0u)
        || ((source_physical & (PAGING64_PAGE_BYTES - 1u)) != 0u))
    {
        return 0u;
    }

    pml4_index = paging64_index(virtual_base, 39u);
    pdpt_index = paging64_index(virtual_base, 30u);
    pd_index = paging64_index(virtual_base, 21u);
    pt_index = paging64_index(virtual_base, 12u);
    if ((pml4_index != 0u)
        || (pd_index == 0u)
        || ((pt_index + page_count) > PAGING64_ENTRY_COUNT))
    {
        return 0u;
    }

    paging64_zero_table(user_runtime_pt);
    pml4[pml4_index] = (PAGING64_PDPT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    pdpt[pdpt_index] = (PAGING64_RUNTIME_PD_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    runtime_pd[pd_index] = (PAGING64_USER_RUNTIME_PT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;

    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        user_runtime_pt[pt_index + page_index] =
            ((source_physical + ((u64)page_index * PAGING64_PAGE_BYTES)) & PAGING64_PAGE_MASK)
            | PAGING64_PAGE_PRESENT
            | PAGING64_PAGE_USER;
    }

    paging64_reload_active_root();

    source_checksum = paging64_checksum((const u8 *)source, mapped_bytes);
    install_token = 2166136261u;
    install_token = paging64_mix_token(install_token, virtual_base);
    install_token = paging64_mix_token(install_token, mapped_bytes);
    install_token = paging64_mix_token(install_token, page_count);
    install_token = paging64_mix_token(install_token, source_checksum);
    install_token = paging64_mix_token(install_token, pml4_index);
    install_token = paging64_mix_token(install_token, pdpt_index);
    install_token = paging64_mix_token(install_token, pd_index);
    install_token = paging64_mix_token(install_token, 1u);
    install_token = (install_token != 0u) ? install_token : 1u;

    entry_probe = (runtime_entry_probe_fn)(u64)virtual_base;
    g_user_runtime_mapping_entry_probe = entry_probe();
    g_user_runtime_mapping_installed = 1u;
    g_user_runtime_mapping_page_count = page_count;
    g_user_runtime_mapping_source_checksum = source_checksum;
    g_user_runtime_mapping_install_token = install_token;
    g_user_runtime_mapping_protection_flags = paging64_compute_protection_flags(
        pml4[pml4_index],
        pdpt[pdpt_index],
        runtime_pd[pd_index],
        user_runtime_pt[pt_index]);
    g_user_runtime_mapping_protection_token = paging64_compute_protection_token(
        install_token,
        g_user_runtime_mapping_protection_flags,
        pml4[pml4_index],
        pdpt[pdpt_index],
        runtime_pd[pd_index],
        user_runtime_pt[pt_index]);
    g_user_runtime_mapping_source_physical = source_physical;
    return 1u;
}

u32 paging64_install_user_stack_mapping(u32 stack_top, u32 stack_bytes)
{
    volatile u64 *pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
    volatile u64 *pdpt = (volatile u64 *)(u64)PAGING64_PDPT_PHYSICAL;
    volatile u64 *runtime_pd = (volatile u64 *)(u64)PAGING64_RUNTIME_PD_PHYSICAL;
    volatile u64 *runtime_pt = (volatile u64 *)(u64)PAGING64_RUNTIME_PT_PHYSICAL;
    volatile u64 *stack_page = (volatile u64 *)(u64)PAGING64_USER_STACK_PHYSICAL;
    u32 pml4_index;
    u32 pdpt_index;
    u32 pd_index;
    u32 pt_index;
    u32 stack_base;
    u32 index;
    u32 token;

    paging64_clear_user_stack_mapping();

    if ((g_runtime_mapping_installed == 0u)
        || (stack_bytes != PAGING64_PAGE_BYTES)
        || ((stack_top & (PAGING64_PAGE_BYTES - 1u)) != 0u)
        || (stack_top < stack_bytes))
    {
        return 0u;
    }

    stack_base = stack_top - stack_bytes;
    pml4_index = paging64_index(stack_base, 39u);
    pdpt_index = paging64_index(stack_base, 30u);
    pd_index = paging64_index(stack_base, 21u);
    pt_index = paging64_index(stack_base, 12u);
    if ((pml4_index != 0u)
        || (pdpt_index != paging64_index(0x40000000u, 30u))
        || (pd_index != paging64_index(0x40000000u, 21u))
        || (pt_index < g_runtime_mapping_page_count))
    {
        return 0u;
    }

    for (index = 0u; index < (PAGING64_PAGE_BYTES / sizeof(u64)); ++index)
    {
        stack_page[index] = 0ull;
    }

    pml4[pml4_index] = (PAGING64_PDPT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    pdpt[pdpt_index] = (PAGING64_RUNTIME_PD_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    runtime_pd[pd_index] = (PAGING64_RUNTIME_PT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    runtime_pt[pt_index] = (PAGING64_USER_STACK_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;

    paging64_reload_active_root();

    token = 2166136261u;
    token = paging64_mix_token(token, stack_base);
    token = paging64_mix_token(token, stack_top);
    token = paging64_mix_token(token, stack_bytes);
    token = paging64_mix_token(token, pt_index);
    token = paging64_mix_token(token, (u32)PAGING64_USER_STACK_PHYSICAL);
    token = (token != 0u) ? token : 1u;

    g_user_stack_mapping_installed = 1u;
    g_user_stack_mapping_protection_flags = paging64_compute_protection_flags(
        pml4[pml4_index],
        pdpt[pdpt_index],
        runtime_pd[pd_index],
        runtime_pt[pt_index]);
    g_user_stack_mapping_protection_token = paging64_compute_protection_token(
        token,
        g_user_stack_mapping_protection_flags,
        pml4[pml4_index],
        pdpt[pdpt_index],
        runtime_pd[pd_index],
        runtime_pt[pt_index]);
    return 1u;
}

u32 paging64_install_kernel_mmio_mapping(u64 virtual_base, u64 physical_base, u32 page_count)
{
    volatile u64 *pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
    volatile u64 *high_pdpt = (volatile u64 *)(u64)PAGING64_HIGH_PDPT_PHYSICAL;
    volatile u64 *kernel_pd = (volatile u64 *)(u64)PAGING64_KERNEL_PD_PHYSICAL;
    volatile u64 *mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    u32 pml4_index;
    u32 pdpt_index;
    u32 pd_index;
    u32 pt_index;
    u32 page_index;
    u32 token;
    u64 physical_end;
    u64 pde_flags;
    u64 pte_flags;
    u64 expected_pml4e;
    u64 expected_pdpte;

    if ((page_count == 0u)
        || (page_count > PAGING64_ENTRY_COUNT)
        || ((virtual_base & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || ((physical_base & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || (paging64_cpu_supports_nx() == 0u))
    {
        return 0u;
    }

    physical_end = physical_base + ((u64)page_count * (u64)PAGING64_PAGE_BYTES);
    if (physical_end <= physical_base)
    {
        return 0u;
    }

    pml4_index = paging64_index64(virtual_base, 39u);
    pdpt_index = paging64_index64(virtual_base, 30u);
    pd_index = paging64_index64(virtual_base, 21u);
    pt_index = paging64_index64(virtual_base, 12u);
    if ((pml4_index != PAGING64_HIGH_HALF_PML4_INDEX)
        || (pdpt_index != PAGING64_HIGH_HALF_PDPT_INDEX)
        || (pd_index == 0u)
        || ((pt_index + page_count) > PAGING64_ENTRY_COUNT))
    {
        return 0u;
    }

    expected_pml4e = (PAGING64_HIGH_PDPT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE;
    expected_pdpte = (PAGING64_KERNEL_PD_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE;
    if (((pml4[pml4_index] & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT)) !=
            (expected_pml4e & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT)))
        || ((high_pdpt[pdpt_index] & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT)) !=
            (expected_pdpte & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT))))
    {
        return 0u;
    }

    if (((kernel_pd[pd_index] & PAGING64_PAGE_PRESENT) != 0ull)
        && ((kernel_pd[pd_index] & PAGING64_PHYSICAL_ADDRESS_MASK) !=
            (PAGING64_KERNEL_MMIO_PT_PHYSICAL & PAGING64_PHYSICAL_ADDRESS_MASK)))
    {
        return 0u;
    }
    wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);

    pde_flags = PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_NO_EXECUTE;
    pte_flags = PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITE_THROUGH
        | PAGING64_PAGE_CACHE_DISABLED
        | PAGING64_PAGE_NO_EXECUTE;

    if (g_kernel_mmio_pt_initialized == 0u)
    {
        paging64_zero_table(mmio_pt);
        g_kernel_mmio_pt_initialized = 1u;
    }

    kernel_pd[pd_index] = (PAGING64_KERNEL_MMIO_PT_PHYSICAL & PAGING64_PAGE_MASK)
        | pde_flags;

    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        mmio_pt[pt_index + page_index] =
            (((u64)physical_base + ((u64)page_index * (u64)PAGING64_PAGE_BYTES))
                & PAGING64_PAGE_MASK)
            | pte_flags;
    }

    paging64_reload_active_root();

    token = 2166136261u;
    token = paging64_mix_token(token, (u32)(virtual_base & 0xFFFFFFFFull));
    token = paging64_mix_token(token, (u32)((virtual_base >> 32) & 0xFFFFFFFFull));
    token = paging64_mix_token(token, (u32)(physical_base & 0xFFFFFFFFull));
    token = paging64_mix_token(token, (u32)((physical_base >> 32) & 0xFFFFFFFFull));
    token = paging64_mix_token(token, page_count);
    token = paging64_mix_token(token, pml4_index);
    token = paging64_mix_token(token, pdpt_index);
    token = paging64_mix_token(token, pd_index);
    token = paging64_mix_token(token, pt_index);
    token = paging64_mix_token(token, (u32)(pte_flags & 0xFFFFFFFFull));
    token = paging64_mix_token(token, (u32)((pte_flags >> 32) & 0xFFFFFFFFull));
    token = (token != 0u) ? token : 1u;

    g_kernel_mmio_mapping_installed = 1u;
    g_kernel_mmio_mapping_install_token = token;
    g_kernel_mmio_mapping_pml4_index = pml4_index;
    g_kernel_mmio_mapping_pdpt_index = pdpt_index;
    g_kernel_mmio_mapping_pd_index = pd_index;
    g_kernel_mmio_mapping_pt_index = pt_index;
    g_kernel_mmio_mapping_entry_flags = pte_flags;
    g_kernel_mmio_mapping_nx_enabled =
        ((rdmsr64(PAGING64_EFER_MSR) & PAGING64_EFER_NXE) != 0ull) ? 1u : 0u;
    return 1u;
}

u32 paging64_install_apic_mmio_mapping(u64 lapic_virtual, u32 lapic_physical, u64 ioapic_virtual, u32 ioapic_physical)
{
    volatile u64 *pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
    volatile u64 *high_pdpt = (volatile u64 *)(u64)PAGING64_HIGH_PDPT_PHYSICAL;
    volatile u64 *kernel_pd = (volatile u64 *)(u64)PAGING64_KERNEL_PD_PHYSICAL;
    volatile u64 *apic_pt = (volatile u64 *)(u64)PAGING64_APIC_MMIO_PT_PHYSICAL;
    u32 lapic_pml4_index;
    u32 lapic_pdpt_index;
    u32 lapic_pd_index;
    u32 lapic_pt_index;
    u32 ioapic_pml4_index;
    u32 ioapic_pdpt_index;
    u32 ioapic_pd_index;
    u32 ioapic_pt_index;
    u32 index;
    u64 pde_flags;
    u64 pte_flags;
    u64 expected_pml4e;
    u64 expected_pdpte;

    if (((lapic_virtual & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || ((ioapic_virtual & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || ((lapic_physical & (PAGING64_PAGE_BYTES - 1u)) != 0u)
        || ((ioapic_physical & (PAGING64_PAGE_BYTES - 1u)) != 0u)
        || (lapic_physical == 0u)
        || (ioapic_physical == 0u)
        || (paging64_cpu_supports_nx() == 0u))
    {
        return 0u;
    }

    lapic_pml4_index = paging64_index64(lapic_virtual, 39u);
    lapic_pdpt_index = paging64_index64(lapic_virtual, 30u);
    lapic_pd_index = paging64_index64(lapic_virtual, 21u);
    lapic_pt_index = paging64_index64(lapic_virtual, 12u);
    ioapic_pml4_index = paging64_index64(ioapic_virtual, 39u);
    ioapic_pdpt_index = paging64_index64(ioapic_virtual, 30u);
    ioapic_pd_index = paging64_index64(ioapic_virtual, 21u);
    ioapic_pt_index = paging64_index64(ioapic_virtual, 12u);

    if ((lapic_pml4_index != PAGING64_HIGH_HALF_PML4_INDEX)
        || (lapic_pdpt_index != PAGING64_HIGH_HALF_PDPT_INDEX)
        || (ioapic_pml4_index != PAGING64_HIGH_HALF_PML4_INDEX)
        || (ioapic_pdpt_index != PAGING64_HIGH_HALF_PDPT_INDEX)
        || (lapic_pd_index == 0u)
        || (lapic_pd_index != ioapic_pd_index)
        || (lapic_pt_index == ioapic_pt_index))
    {
        return 0u;
    }

    expected_pml4e = (PAGING64_HIGH_PDPT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE;
    expected_pdpte = (PAGING64_KERNEL_PD_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE;
    if (((pml4[lapic_pml4_index] & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT)) !=
            (expected_pml4e & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT)))
        || ((high_pdpt[lapic_pdpt_index] & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT)) !=
            (expected_pdpte & (PAGING64_PAGE_MASK | PAGING64_PAGE_PRESENT))))
    {
        return 0u;
    }

    if (((kernel_pd[lapic_pd_index] & PAGING64_PAGE_PRESENT) != 0ull)
        && ((kernel_pd[lapic_pd_index] & PAGING64_PHYSICAL_ADDRESS_MASK) !=
            (PAGING64_APIC_MMIO_PT_PHYSICAL & PAGING64_PHYSICAL_ADDRESS_MASK)))
    {
        return 0u;
    }

    wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);
    for (index = 0u; index < PAGING64_ENTRY_COUNT; ++index)
    {
        apic_pt[index] = 0ull;
    }

    pde_flags = PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_NO_EXECUTE;
    pte_flags = PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_WRITE_THROUGH
        | PAGING64_PAGE_CACHE_DISABLED
        | PAGING64_PAGE_NO_EXECUTE;

    kernel_pd[lapic_pd_index] = (PAGING64_APIC_MMIO_PT_PHYSICAL & PAGING64_PAGE_MASK)
        | pde_flags;
    apic_pt[lapic_pt_index] = ((u64)lapic_physical & PAGING64_PAGE_MASK) | pte_flags;
    apic_pt[ioapic_pt_index] = ((u64)ioapic_physical & PAGING64_PAGE_MASK) | pte_flags;
    paging64_reload_active_root();
    return 1u;
}

u32 paging64_runtime_mapping_installed(void)
{
    return g_runtime_mapping_installed;
}

u32 paging64_runtime_mapping_page_count(void)
{
    return g_runtime_mapping_page_count;
}

u32 paging64_runtime_mapping_source_checksum(void)
{
    return g_runtime_mapping_source_checksum;
}

u32 paging64_runtime_mapping_install_token(void)
{
    return g_runtime_mapping_install_token;
}

u32 paging64_runtime_mapping_entry_probe(void)
{
    return g_runtime_mapping_entry_probe;
}

u32 paging64_runtime_mapping_protection_flags(void)
{
    return g_runtime_mapping_protection_flags;
}

u32 paging64_runtime_mapping_protection_token(void)
{
    return g_runtime_mapping_protection_token;
}

u64 paging64_runtime_mapping_source_physical(void)
{
    return g_runtime_mapping_source_physical;
}

u32 paging64_user_runtime_mapping_installed(void)
{
    return g_user_runtime_mapping_installed;
}

u32 paging64_user_runtime_mapping_page_count(void)
{
    return g_user_runtime_mapping_page_count;
}

u32 paging64_user_runtime_mapping_source_checksum(void)
{
    return g_user_runtime_mapping_source_checksum;
}

u32 paging64_user_runtime_mapping_install_token(void)
{
    return g_user_runtime_mapping_install_token;
}

u32 paging64_user_runtime_mapping_entry_probe(void)
{
    return g_user_runtime_mapping_entry_probe;
}

u32 paging64_user_runtime_mapping_protection_flags(void)
{
    return g_user_runtime_mapping_protection_flags;
}

u32 paging64_user_runtime_mapping_protection_token(void)
{
    return g_user_runtime_mapping_protection_token;
}

u64 paging64_user_runtime_mapping_source_physical(void)
{
    return g_user_runtime_mapping_source_physical;
}

u32 paging64_user_stack_mapping_installed(void)
{
    return g_user_stack_mapping_installed;
}

u32 paging64_user_stack_mapping_protection_flags(void)
{
    return g_user_stack_mapping_protection_flags;
}

u32 paging64_user_stack_mapping_protection_token(void)
{
    return g_user_stack_mapping_protection_token;
}

u32 paging64_kernel_mmio_mapping_installed(void)
{
    return g_kernel_mmio_mapping_installed;
}

u32 paging64_kernel_mmio_mapping_install_token(void)
{
    return g_kernel_mmio_mapping_install_token;
}

u32 paging64_kernel_mmio_mapping_pml4_index(void)
{
    return g_kernel_mmio_mapping_pml4_index;
}

u32 paging64_kernel_mmio_mapping_pdpt_index(void)
{
    return g_kernel_mmio_mapping_pdpt_index;
}

u32 paging64_kernel_mmio_mapping_pd_index(void)
{
    return g_kernel_mmio_mapping_pd_index;
}

u32 paging64_kernel_mmio_mapping_pt_index(void)
{
    return g_kernel_mmio_mapping_pt_index;
}

u64 paging64_kernel_mmio_mapping_entry_flags(void)
{
    return g_kernel_mmio_mapping_entry_flags;
}

u32 paging64_kernel_mmio_mapping_nx_enabled(void)
{
    return g_kernel_mmio_mapping_nx_enabled;
}

u32 paging64_kernel_mmio_write_window_open(u32 page_index)
{
    volatile u64 *mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    u32 pt_index = g_kernel_mmio_mapping_pt_index + page_index;

    if ((g_kernel_mmio_mapping_installed == 0u)
        || (pt_index >= PAGING64_ENTRY_COUNT)
        || ((mmio_pt[pt_index] & PAGING64_PAGE_PRESENT) == 0ull))
    {
        return 0u;
    }

    mmio_pt[pt_index] |= PAGING64_PAGE_WRITABLE;
    paging64_reload_active_root();
    return 1u;
}

u32 paging64_kernel_mmio_write_window_close(u32 page_index)
{
    volatile u64 *mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    u32 pt_index = g_kernel_mmio_mapping_pt_index + page_index;

    if ((g_kernel_mmio_mapping_installed == 0u)
        || (pt_index >= PAGING64_ENTRY_COUNT)
        || ((mmio_pt[pt_index] & PAGING64_PAGE_PRESENT) == 0ull))
    {
        return 0u;
    }

    mmio_pt[pt_index] &= ~PAGING64_PAGE_WRITABLE;
    paging64_reload_active_root();
    return 1u;
}

static u32 paging64_kernel_mmio_write_window_set_virtual(u64 virtual_address, u32 writable)
{
    volatile u64 *pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
    volatile u64 *high_pdpt = (volatile u64 *)(u64)PAGING64_HIGH_PDPT_PHYSICAL;
    volatile u64 *kernel_pd = (volatile u64 *)(u64)PAGING64_KERNEL_PD_PHYSICAL;
    volatile u64 *mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    u32 pml4_index = paging64_index64(virtual_address, 39u);
    u32 pdpt_index = paging64_index64(virtual_address, 30u);
    u32 pd_index = paging64_index64(virtual_address, 21u);
    u32 pt_index = paging64_index64(virtual_address, 12u);

    if ((pml4_index != PAGING64_HIGH_HALF_PML4_INDEX)
        || (pdpt_index != PAGING64_HIGH_HALF_PDPT_INDEX)
        || ((pml4[pml4_index] & PAGING64_PAGE_PRESENT) == 0ull)
        || ((high_pdpt[pdpt_index] & PAGING64_PAGE_PRESENT) == 0ull)
        || ((kernel_pd[pd_index] & PAGING64_PAGE_PRESENT) == 0ull)
        || ((kernel_pd[pd_index] & PAGING64_PHYSICAL_ADDRESS_MASK) !=
            (PAGING64_KERNEL_MMIO_PT_PHYSICAL & PAGING64_PHYSICAL_ADDRESS_MASK))
        || ((mmio_pt[pt_index] & PAGING64_PAGE_PRESENT) == 0ull))
    {
        return 0u;
    }

    if (writable != 0u)
    {
        mmio_pt[pt_index] |= PAGING64_PAGE_WRITABLE;
    }
    else
    {
        mmio_pt[pt_index] &= ~PAGING64_PAGE_WRITABLE;
    }
    paging64_reload_active_root();
    return 1u;
}

u32 paging64_kernel_mmio_write_window_open_virtual(u64 virtual_address)
{
    return paging64_kernel_mmio_write_window_set_virtual(virtual_address, 1u);
}

u32 paging64_kernel_mmio_write_window_close_virtual(u64 virtual_address)
{
    return paging64_kernel_mmio_write_window_set_virtual(virtual_address, 0u);
}
