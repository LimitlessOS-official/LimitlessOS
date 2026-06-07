#include "paging_x64.h"

#include "x64.h"

#define PAGING64_KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ull
#define PAGING64_KERNEL_LINKED_OFFSET 0x00010000ull
#define PAGING64_KERNEL_LINKED_LOW_LIMIT 0x01000000ull
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
#define PAGING64_INVALID_PID 0xFFFFFFFFu

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
#define PAGING64_VMA_USER_PT_COUNT 32u
#define PAGING64_PROCESS_ROOT_POOL_LIMIT 4u
#define PAGING64_PROCESS_ROOT_VMA_PT_COUNT 16u
static u64 g_paging64_vma_user_pts[PAGING64_VMA_USER_PT_COUNT][PAGING64_ENTRY_COUNT]
    __attribute__((aligned(PAGING64_PAGE_BYTES)));
static u32 g_paging64_vma_user_pt_initialized[PAGING64_VMA_USER_PT_COUNT];
static u32 g_paging64_vma_user_pd_index[PAGING64_VMA_USER_PT_COUNT];
static u64 g_paging64_process_pml4[PAGING64_PROCESS_ROOT_POOL_LIMIT][PAGING64_ENTRY_COUNT]
    __attribute__((aligned(PAGING64_PAGE_BYTES)));
static u64 g_paging64_process_pdpt[PAGING64_PROCESS_ROOT_POOL_LIMIT][PAGING64_ENTRY_COUNT]
    __attribute__((aligned(PAGING64_PAGE_BYTES)));
static u64 g_paging64_process_runtime_pd[PAGING64_PROCESS_ROOT_POOL_LIMIT][PAGING64_ENTRY_COUNT]
    __attribute__((aligned(PAGING64_PAGE_BYTES)));
static u64 g_paging64_process_vma_pts[PAGING64_PROCESS_ROOT_POOL_LIMIT][PAGING64_PROCESS_ROOT_VMA_PT_COUNT][PAGING64_ENTRY_COUNT]
    __attribute__((aligned(PAGING64_PAGE_BYTES)));
static u32 g_paging64_process_root_used[PAGING64_PROCESS_ROOT_POOL_LIMIT];
static u32 g_paging64_process_root_pid[PAGING64_PROCESS_ROOT_POOL_LIMIT];
static u32 g_paging64_process_root_owner[PAGING64_PROCESS_ROOT_POOL_LIMIT];
static u32 g_paging64_process_root_token[PAGING64_PROCESS_ROOT_POOL_LIMIT];
static u32 g_paging64_process_root_initialized[PAGING64_PROCESS_ROOT_POOL_LIMIT];
static u32 g_paging64_process_vma_pt_initialized[PAGING64_PROCESS_ROOT_POOL_LIMIT][PAGING64_PROCESS_ROOT_VMA_PT_COUNT];
static u32 g_paging64_process_vma_pt_pd_index[PAGING64_PROCESS_ROOT_POOL_LIMIT][PAGING64_PROCESS_ROOT_VMA_PT_COUNT];
static u64 g_paging64_kernel_root_physical = 0ull;
static u32 g_paging64_process_root_pool_used = 0u;
static u32 g_paging64_process_root_alloc_count = 0u;
static u32 g_paging64_process_root_release_count = 0u;
static u32 g_paging64_process_root_alloc_denial_count = 0u;
static u32 g_paging64_process_root_switch_count = 0u;
static u32 g_paging64_process_root_switch_denial_count = 0u;
static u32 g_paging64_process_root_kernel_switch_count = 0u;
static u32 g_paging64_process_root_last_switch_reason = 0u;
static u32 g_paging64_process_root_low_compat_count = 0u;
static u32 g_paging64_process_root_last_low_compat = 0u;
static u32 g_paging64_process_root_high_copy_count = 0u;
static u32 g_paging64_process_root_last_high_copy = 0u;
static u32 g_paging64_process_root_mmio_shared_count = 0u;
static u32 g_paging64_process_root_last_mmio_shared = 0u;
static u32 g_paging64_process_root_pool_mapped_count = 0u;
static u32 g_paging64_process_root_last_pool_mapped = 0u;
static u32 g_paging64_process_root_user_pdpt_private_count = 0u;
static u32 g_paging64_process_root_last_user_pdpt_private = 0u;
static u32 g_paging64_process_root_vma_pt_private_count = 0u;
static u32 g_paging64_process_root_last_vma_pt_private = 0u;
static u32 g_paging64_process_root_last_slot = 0xFFFFFFFFu;
static u32 g_paging64_process_root_last_pid = PAGING64_INVALID_PID;
static u64 g_paging64_process_root_last_physical = 0ull;
static u32 g_paging64_process_root_fork_count = 0u;
static u32 g_paging64_process_root_fork_denial_count = 0u;
static u32 g_paging64_process_root_fork_last_parent_pid = PAGING64_INVALID_PID;
static u32 g_paging64_process_root_fork_last_child_pid = PAGING64_INVALID_PID;
static u32 g_paging64_process_root_fork_last_parent_slot = 0xFFFFFFFFu;
static u32 g_paging64_process_root_fork_last_child_slot = 0xFFFFFFFFu;
static u32 g_paging64_process_root_fork_last_child_root_distinct = 0u;
#endif

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
    if ((g_paging64_kernel_physical_base != 0ull)
        && (value >= PAGING64_KERNEL_LINKED_OFFSET)
        && (value < PAGING64_KERNEL_LINKED_LOW_LIMIT))
    {
        return g_paging64_kernel_physical_base + value;
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

#ifdef LIMITLESS_X64_UEFI_KERNEL
static u64 paging64_ensure_kernel_root_physical(void)
{
    if (g_paging64_kernel_root_physical == 0ull)
    {
        g_paging64_kernel_root_physical = paging64_active_root_physical();
    }

    return g_paging64_kernel_root_physical;
}

u64 paging64_current_root_physical(void)
{
    return read_cr3_64() & PAGING64_PAGE_MASK;
}

static u32 paging64_process_root_find_slot(u32 pid)
{
    u32 slot;

    if ((pid == 0u) || (pid == PAGING64_INVALID_PID))
    {
        return 0xFFFFFFFFu;
    }

    for (slot = 0u; slot < PAGING64_PROCESS_ROOT_POOL_LIMIT; ++slot)
    {
        if ((g_paging64_process_root_used[slot] != 0u)
            && (g_paging64_process_root_pid[slot] == pid))
        {
            return slot;
        }
    }

    return 0xFFFFFFFFu;
}

static void paging64_process_root_clear_slot(u32 slot)
{
    u32 index;

    if (slot >= PAGING64_PROCESS_ROOT_POOL_LIMIT)
    {
        return;
    }

    paging64_zero_table((volatile u64 *)g_paging64_process_pml4[slot]);
    paging64_zero_table((volatile u64 *)g_paging64_process_pdpt[slot]);
    paging64_zero_table((volatile u64 *)g_paging64_process_runtime_pd[slot]);
    for (index = 0u; index < PAGING64_PROCESS_ROOT_VMA_PT_COUNT; ++index)
    {
        paging64_zero_table((volatile u64 *)g_paging64_process_vma_pts[slot][index]);
        g_paging64_process_vma_pt_initialized[slot][index] = 0u;
        g_paging64_process_vma_pt_pd_index[slot][index] = 0u;
    }
    g_paging64_process_root_used[slot] = 0u;
    g_paging64_process_root_pid[slot] = PAGING64_INVALID_PID;
    g_paging64_process_root_owner[slot] = 0u;
    g_paging64_process_root_token[slot] = 0u;
    g_paging64_process_root_initialized[slot] = 1u;
}

static u32 paging64_process_root_token_for(
    u32 pid,
    u32 owner_id,
    u32 authority_token,
    u32 slot,
    u64 root_physical)
{
    u32 token = 2166136261u;

    token = paging64_mix_token(token, pid);
    token = paging64_mix_token(token, owner_id);
    token = paging64_mix_token(token, authority_token);
    token = paging64_mix_token(token, slot);
    token = paging64_mix_token(token, (u32)(root_physical & 0xFFFFFFFFull));
    token = paging64_mix_token(token, (u32)((root_physical >> 32) & 0xFFFFFFFFull));
    return (token != 0u) ? token : 1u;
}

static u32 paging64_kernel_root_maps_virtual(u64 virtual_address)
{
    volatile u64 *pml4;
    volatile u64 *pdpt;
    volatile u64 *pd;
    volatile u64 *pt;
    u64 pml4e;
    u64 pdpte;
    u64 pde;
    u64 pte;

    pml4 = (volatile u64 *)(u64)paging64_ensure_kernel_root_physical();
    pml4e = pml4[paging64_index64(virtual_address, 39u)];
    if ((pml4e & PAGING64_PAGE_PRESENT) == 0ull)
    {
        return 0u;
    }

    pdpt = (volatile u64 *)(u64)(pml4e & PAGING64_PHYSICAL_ADDRESS_MASK);
    pdpte = pdpt[paging64_index64(virtual_address, 30u)];
    if ((pdpte & PAGING64_PAGE_PRESENT) == 0ull)
    {
        return 0u;
    }
    if ((pdpte & PAGING64_PAGE_LARGE) != 0ull)
    {
        return 1u;
    }

    pd = (volatile u64 *)(u64)(pdpte & PAGING64_PHYSICAL_ADDRESS_MASK);
    pde = pd[paging64_index64(virtual_address, 21u)];
    if ((pde & PAGING64_PAGE_PRESENT) == 0ull)
    {
        return 0u;
    }
    if ((pde & PAGING64_PAGE_LARGE) != 0ull)
    {
        return 1u;
    }

    pt = (volatile u64 *)(u64)(pde & PAGING64_PHYSICAL_ADDRESS_MASK);
    pte = pt[paging64_index64(virtual_address, 12u)];
    return ((pte & PAGING64_PAGE_PRESENT) != 0ull) ? 1u : 0u;
}

static u32 paging64_kernel_root_maps_range(const void *base, u32 byte_count)
{
    u64 first;
    u64 last;
    u64 page;

    if ((base == 0) || (byte_count == 0u))
    {
        return 0u;
    }

    first = ((u64)base) & PAGING64_PAGE_MASK;
    last = (((u64)base) + ((u64)byte_count - 1ull)) & PAGING64_PAGE_MASK;
    if (last < first)
    {
        return 0u;
    }

    for (page = first; page <= last; page += (u64)PAGING64_PAGE_BYTES)
    {
        if (paging64_kernel_root_maps_virtual(page) == 0u)
        {
            return 0u;
        }
        if ((last - page) < (u64)PAGING64_PAGE_BYTES)
        {
            break;
        }
    }

    return 1u;
}

static u32 paging64_process_root_pool_mapping_present(
    volatile u64 *pml4,
    volatile u64 *kernel_pml4)
{
    if ((pml4 == 0)
        || (kernel_pml4 == 0)
        || ((kernel_pml4[PAGING64_HIGH_HALF_PML4_INDEX] & PAGING64_PAGE_PRESENT) == 0ull)
        || (pml4[PAGING64_HIGH_HALF_PML4_INDEX]
            != kernel_pml4[PAGING64_HIGH_HALF_PML4_INDEX]))
    {
        return 0u;
    }

    if ((paging64_kernel_root_maps_range(
            g_paging64_process_pml4,
            (u32)sizeof(g_paging64_process_pml4)) == 0u)
        || (paging64_kernel_root_maps_range(
            g_paging64_process_pdpt,
            (u32)sizeof(g_paging64_process_pdpt)) == 0u)
        || (paging64_kernel_root_maps_range(
            g_paging64_process_runtime_pd,
            (u32)sizeof(g_paging64_process_runtime_pd)) == 0u)
        || (paging64_kernel_root_maps_range(
            g_paging64_process_vma_pts,
            (u32)sizeof(g_paging64_process_vma_pts)) == 0u))
    {
        return 0u;
    }

    return 1u;
}

u32 paging64_process_root_alloc(u32 pid, u32 owner_id, u32 authority_token)
{
    volatile u64 *kernel_pml4;
    volatile u64 *kernel_pdpt;
    volatile u64 *pml4;
    volatile u64 *pdpt;
    u64 kernel_root;
    u64 root_physical;
    u64 pdpt_physical;
    u64 runtime_pd_physical;
    u32 slot;
    u32 framebuffer_pdpt_index;
    u32 framebuffer_shared = 0u;

    if ((pid == 0u)
        || (pid == PAGING64_INVALID_PID)
        || (owner_id == 0u)
        || (authority_token == 0u))
    {
        ++g_paging64_process_root_alloc_denial_count;
        return 0u;
    }

    if (paging64_process_root_find_slot(pid) != 0xFFFFFFFFu)
    {
        return 1u;
    }

    for (slot = 0u; slot < PAGING64_PROCESS_ROOT_POOL_LIMIT; ++slot)
    {
        if (g_paging64_process_root_used[slot] == 0u)
        {
            break;
        }
    }
    if (slot >= PAGING64_PROCESS_ROOT_POOL_LIMIT)
    {
        ++g_paging64_process_root_alloc_denial_count;
        return 0u;
    }

    kernel_root = paging64_ensure_kernel_root_physical();
    if (paging64_current_root_physical() != (kernel_root & PAGING64_PAGE_MASK))
    {
        if (paging64_switch_to_kernel_root(0x504D4C34u) == 0u)
        {
            ++g_paging64_process_root_alloc_denial_count;
            return 0u;
        }
    }
    kernel_pml4 = (volatile u64 *)(u64)kernel_root;
    kernel_pdpt = (volatile u64 *)(u64)(kernel_pml4[0u] & PAGING64_PHYSICAL_ADDRESS_MASK);
    pml4 = (volatile u64 *)g_paging64_process_pml4[slot];
    pdpt = (volatile u64 *)g_paging64_process_pdpt[slot];
    root_physical = paging64_kernel_physical_alias(g_paging64_process_pml4[slot]);
    pdpt_physical = paging64_kernel_physical_alias(g_paging64_process_pdpt[slot]);
    runtime_pd_physical = paging64_kernel_physical_alias(g_paging64_process_runtime_pd[slot]);

    paging64_process_root_clear_slot(slot);
    pml4[0u] = (pdpt_physical & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    g_paging64_process_root_last_user_pdpt_private =
        (((pml4[0u] & PAGING64_PHYSICAL_ADDRESS_MASK)
            == (pdpt_physical & PAGING64_PAGE_MASK))
            && ((pml4[0u] & PAGING64_PAGE_PRESENT) != 0ull))
            ? 1u
            : 0u;
    if (g_paging64_process_root_last_user_pdpt_private != 0u)
    {
        ++g_paging64_process_root_user_pdpt_private_count;
    }
    pml4[PAGING64_HIGH_HALF_PML4_INDEX] = kernel_pml4[PAGING64_HIGH_HALF_PML4_INDEX];

    /*
     * M22 transitional low-identity compatibility mapping. This keeps the
     * loader's low 16 MiB identity window visible while user mappings move to
     * private per-process lower-half tables. Keep the low-compat telemetry in
     * every gate run until this compatibility entry is removed.
     */
    if ((kernel_pdpt != 0) && ((kernel_pdpt[0u] & PAGING64_PAGE_PRESENT) != 0ull))
    {
        pdpt[0u] = kernel_pdpt[0u];
        g_paging64_process_root_last_low_compat = 1u;
        ++g_paging64_process_root_low_compat_count;
    }
    else
    {
        g_paging64_process_root_last_low_compat = 0u;
    }

    framebuffer_pdpt_index = paging64_index64(0x80000000ull, 30u);
    if ((kernel_pdpt != 0)
        && (framebuffer_pdpt_index != paging64_index64(0x40000000ull, 30u))
        && ((kernel_pdpt[framebuffer_pdpt_index] & PAGING64_PAGE_PRESENT) != 0ull))
    {
        pdpt[framebuffer_pdpt_index] =
            kernel_pdpt[framebuffer_pdpt_index] & ~PAGING64_PAGE_USER;
        framebuffer_shared = 1u;
    }
    pdpt[paging64_index64(0x40000000ull, 30u)] =
        (runtime_pd_physical & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;

    g_paging64_process_root_last_high_copy =
        ((pml4[PAGING64_HIGH_HALF_PML4_INDEX] & PAGING64_PAGE_PRESENT) != 0ull)
            ? 1u
            : 0u;
    if (g_paging64_process_root_last_high_copy != 0u)
    {
        ++g_paging64_process_root_high_copy_count;
    }
    g_paging64_process_root_last_mmio_shared =
        ((g_paging64_process_root_last_high_copy != 0u)
            && ((g_kernel_mmio_mapping_installed != 0u)
                || (framebuffer_shared != 0u)))
            ? 1u
            : 0u;
    if (g_paging64_process_root_last_mmio_shared != 0u)
    {
        ++g_paging64_process_root_mmio_shared_count;
    }
    g_paging64_process_root_last_pool_mapped =
        paging64_process_root_pool_mapping_present(pml4, kernel_pml4);
    if (g_paging64_process_root_last_pool_mapped != 0u)
    {
        ++g_paging64_process_root_pool_mapped_count;
    }

    g_paging64_process_root_used[slot] = 1u;
    g_paging64_process_root_pid[slot] = pid;
    g_paging64_process_root_owner[slot] = owner_id;
    g_paging64_process_root_token[slot] =
        paging64_process_root_token_for(pid, owner_id, authority_token, slot, root_physical);
    ++g_paging64_process_root_pool_used;
    ++g_paging64_process_root_alloc_count;
    g_paging64_process_root_last_slot = slot;
    g_paging64_process_root_last_pid = pid;
    g_paging64_process_root_last_physical = root_physical;
    return 1u;
}

u32 paging64_process_root_fork_alloc(
    u32 parent_pid,
    u32 child_pid,
    u32 owner_id,
    u32 authority_token)
{
    u32 parent_slot;
    u32 child_slot;
    u64 parent_root;
    u64 child_root;

    g_paging64_process_root_fork_last_parent_pid = parent_pid;
    g_paging64_process_root_fork_last_child_pid = child_pid;
    g_paging64_process_root_fork_last_parent_slot = 0xFFFFFFFFu;
    g_paging64_process_root_fork_last_child_slot = 0xFFFFFFFFu;
    g_paging64_process_root_fork_last_child_root_distinct = 0u;

    if ((parent_pid == 0u)
        || (parent_pid == PAGING64_INVALID_PID)
        || (child_pid == 0u)
        || (child_pid == PAGING64_INVALID_PID)
        || (parent_pid == child_pid)
        || (owner_id == 0u)
        || (authority_token == 0u))
    {
        ++g_paging64_process_root_fork_denial_count;
        return 0u;
    }

    parent_slot = paging64_process_root_find_slot(parent_pid);
    if ((parent_slot == 0xFFFFFFFFu)
        || (paging64_process_root_find_slot(child_pid) != 0xFFFFFFFFu))
    {
        ++g_paging64_process_root_fork_denial_count;
        return 0u;
    }

    parent_root = paging64_kernel_physical_alias(g_paging64_process_pml4[parent_slot]);
    if (paging64_process_root_alloc(child_pid, owner_id, authority_token) == 0u)
    {
        ++g_paging64_process_root_fork_denial_count;
        return 0u;
    }

    child_slot = paging64_process_root_find_slot(child_pid);
    child_root = (child_slot != 0xFFFFFFFFu)
        ? paging64_kernel_physical_alias(g_paging64_process_pml4[child_slot])
        : 0ull;

    if ((child_slot == 0xFFFFFFFFu) || (child_root == 0ull))
    {
        ++g_paging64_process_root_fork_denial_count;
        return 0u;
    }

    g_paging64_process_root_fork_last_parent_slot = parent_slot;
    g_paging64_process_root_fork_last_child_slot = child_slot;
    g_paging64_process_root_fork_last_child_root_distinct =
        ((child_root != parent_root)
            && (child_root != (paging64_ensure_kernel_root_physical() & PAGING64_PAGE_MASK)))
            ? 1u
            : 0u;
    ++g_paging64_process_root_fork_count;
    return 1u;
}

u32 paging64_process_root_release(u32 pid, u32 authority_token)
{
    u32 slot = paging64_process_root_find_slot(pid);
    u64 root_physical;

    if ((slot == 0xFFFFFFFFu)
        || (authority_token == 0u)
        || (authority_token != g_paging64_process_root_token[slot]))
    {
        ++g_paging64_process_root_alloc_denial_count;
        return 0u;
    }

    root_physical = paging64_kernel_physical_alias(g_paging64_process_pml4[slot]);
    if ((read_cr3_64() & PAGING64_PAGE_MASK) == (root_physical & PAGING64_PAGE_MASK))
    {
        (void)paging64_switch_to_kernel_root(0x52454C00u);
    }

    paging64_process_root_clear_slot(slot);
    if (g_paging64_process_root_pool_used != 0u)
    {
        --g_paging64_process_root_pool_used;
    }
    ++g_paging64_process_root_release_count;
    g_paging64_process_root_last_slot = slot;
    g_paging64_process_root_last_pid = pid;
    g_paging64_process_root_last_physical = root_physical;
    return 1u;
}

u64 paging64_process_root_physical(u32 pid)
{
    u32 slot = paging64_process_root_find_slot(pid);

    return (slot != 0xFFFFFFFFu)
        ? paging64_kernel_physical_alias(g_paging64_process_pml4[slot])
        : 0ull;
}

u32 paging64_process_root_slot(u32 pid)
{
    return paging64_process_root_find_slot(pid);
}

u32 paging64_process_root_token(u32 pid)
{
    u32 slot = paging64_process_root_find_slot(pid);

    return (slot != 0xFFFFFFFFu) ? g_paging64_process_root_token[slot] : 0u;
}

u32 paging64_switch_to_process_root(u32 pid, u32 reason)
{
    u64 root = paging64_process_root_physical(pid);

    if (root == 0ull)
    {
        ++g_paging64_process_root_switch_denial_count;
        return 0u;
    }

    write_cr3_64(root & PAGING64_PAGE_MASK);
    ++g_paging64_process_root_switch_count;
    g_paging64_process_root_last_switch_reason = reason;
    g_paging64_process_root_last_pid = pid;
    g_paging64_process_root_last_physical = root & PAGING64_PAGE_MASK;
    return 1u;
}

u32 paging64_switch_to_kernel_root(u32 reason)
{
    u64 root = paging64_ensure_kernel_root_physical();

    if (root == 0ull)
    {
        ++g_paging64_process_root_switch_denial_count;
        return 0u;
    }

    write_cr3_64(root & PAGING64_PAGE_MASK);
    ++g_paging64_process_root_kernel_switch_count;
    g_paging64_process_root_last_switch_reason = reason;
    g_paging64_process_root_last_pid = 0u;
    g_paging64_process_root_last_physical = root & PAGING64_PAGE_MASK;
    return 1u;
}

u32 paging64_reload_current_root(void)
{
    write_cr3_64(read_cr3_64());
    return 1u;
}

u64 paging64_kernel_root_physical(void) { return paging64_ensure_kernel_root_physical(); }
u32 paging64_process_root_pool_limit(void) { return PAGING64_PROCESS_ROOT_POOL_LIMIT; }
u32 paging64_process_root_pool_used(void) { return g_paging64_process_root_pool_used; }
u32 paging64_process_root_alloc_count(void) { return g_paging64_process_root_alloc_count; }
u32 paging64_process_root_release_count(void) { return g_paging64_process_root_release_count; }
u32 paging64_process_root_alloc_denial_count(void) { return g_paging64_process_root_alloc_denial_count; }
u32 paging64_process_root_switch_count(void) { return g_paging64_process_root_switch_count; }
u32 paging64_process_root_switch_denial_count(void) { return g_paging64_process_root_switch_denial_count; }
u32 paging64_process_root_kernel_switch_count(void) { return g_paging64_process_root_kernel_switch_count; }
u32 paging64_process_root_last_switch_reason(void) { return g_paging64_process_root_last_switch_reason; }
u32 paging64_process_root_low_compat_count(void) { return g_paging64_process_root_low_compat_count; }
u32 paging64_process_root_last_low_compat(void) { return g_paging64_process_root_last_low_compat; }
u32 paging64_process_root_high_copy_count(void) { return g_paging64_process_root_high_copy_count; }
u32 paging64_process_root_last_high_copy(void) { return g_paging64_process_root_last_high_copy; }
u32 paging64_process_root_mmio_shared_count(void) { return g_paging64_process_root_mmio_shared_count; }
u32 paging64_process_root_last_mmio_shared(void) { return g_paging64_process_root_last_mmio_shared; }
u32 paging64_process_root_last_pool_mapped(void) { return g_paging64_process_root_last_pool_mapped; }
u32 paging64_process_root_last_user_pdpt_private(void)
{
    return g_paging64_process_root_last_user_pdpt_private;
}
u32 paging64_process_root_last_vma_pt_private(void)
{
    return g_paging64_process_root_last_vma_pt_private;
}
u32 paging64_process_root_last_slot(void) { return g_paging64_process_root_last_slot; }
u32 paging64_process_root_last_pid(void) { return g_paging64_process_root_last_pid; }
u64 paging64_process_root_last_physical(void) { return g_paging64_process_root_last_physical; }
u32 paging64_process_root_fork_count(void) { return g_paging64_process_root_fork_count; }
u32 paging64_process_root_fork_denial_count(void)
{
    return g_paging64_process_root_fork_denial_count;
}
u32 paging64_process_root_fork_last_parent_pid(void)
{
    return g_paging64_process_root_fork_last_parent_pid;
}
u32 paging64_process_root_fork_last_child_pid(void)
{
    return g_paging64_process_root_fork_last_child_pid;
}
u32 paging64_process_root_fork_last_parent_slot(void)
{
    return g_paging64_process_root_fork_last_parent_slot;
}
u32 paging64_process_root_fork_last_child_slot(void)
{
    return g_paging64_process_root_fork_last_child_slot;
}
u32 paging64_process_root_fork_last_child_root_distinct(void)
{
    return g_paging64_process_root_fork_last_child_root_distinct;
}
#else
u32 paging64_process_root_alloc(u32 pid, u32 owner_id, u32 authority_token)
{
    (void)pid;
    (void)owner_id;
    (void)authority_token;
    return 0u;
}

u32 paging64_process_root_fork_alloc(
    u32 parent_pid,
    u32 child_pid,
    u32 owner_id,
    u32 authority_token)
{
    (void)parent_pid;
    (void)child_pid;
    (void)owner_id;
    (void)authority_token;
    return 0u;
}

u32 paging64_process_root_release(u32 pid, u32 authority_token)
{
    (void)pid;
    (void)authority_token;
    return 0u;
}

u64 paging64_process_root_physical(u32 pid) { (void)pid; return 0ull; }
u32 paging64_process_root_slot(u32 pid) { (void)pid; return 0xFFFFFFFFu; }
u32 paging64_process_root_token(u32 pid) { (void)pid; return 0u; }
u32 paging64_switch_to_process_root(u32 pid, u32 reason)
{
    (void)pid;
    (void)reason;
    return 0u;
}
u32 paging64_switch_to_kernel_root(u32 reason) { (void)reason; return 0u; }
u32 paging64_reload_current_root(void) { write_cr3_64(read_cr3_64()); return 1u; }
u64 paging64_current_root_physical(void) { return read_cr3_64() & PAGING64_PAGE_MASK; }
u64 paging64_kernel_root_physical(void) { return read_cr3_64() & PAGING64_PAGE_MASK; }
u32 paging64_process_root_pool_limit(void) { return 0u; }
u32 paging64_process_root_pool_used(void) { return 0u; }
u32 paging64_process_root_alloc_count(void) { return 0u; }
u32 paging64_process_root_release_count(void) { return 0u; }
u32 paging64_process_root_alloc_denial_count(void) { return 0u; }
u32 paging64_process_root_switch_count(void) { return 0u; }
u32 paging64_process_root_switch_denial_count(void) { return 0u; }
u32 paging64_process_root_kernel_switch_count(void) { return 0u; }
u32 paging64_process_root_last_switch_reason(void) { return 0u; }
u32 paging64_process_root_low_compat_count(void) { return 0u; }
u32 paging64_process_root_last_low_compat(void) { return 0u; }
u32 paging64_process_root_high_copy_count(void) { return 0u; }
u32 paging64_process_root_last_high_copy(void) { return 0u; }
u32 paging64_process_root_mmio_shared_count(void) { return 0u; }
u32 paging64_process_root_last_mmio_shared(void) { return 0u; }
u32 paging64_process_root_last_pool_mapped(void) { return 0u; }
u32 paging64_process_root_last_user_pdpt_private(void) { return 0u; }
u32 paging64_process_root_last_vma_pt_private(void) { return 0u; }
u32 paging64_process_root_last_slot(void) { return 0xFFFFFFFFu; }
u32 paging64_process_root_last_pid(void) { return PAGING64_INVALID_PID; }
u64 paging64_process_root_last_physical(void) { return 0ull; }
u32 paging64_process_root_fork_count(void) { return 0u; }
u32 paging64_process_root_fork_denial_count(void) { return 0u; }
u32 paging64_process_root_fork_last_parent_pid(void) { return PAGING64_INVALID_PID; }
u32 paging64_process_root_fork_last_child_pid(void) { return PAGING64_INVALID_PID; }
u32 paging64_process_root_fork_last_parent_slot(void) { return 0xFFFFFFFFu; }
u32 paging64_process_root_fork_last_child_slot(void) { return 0xFFFFFFFFu; }
u32 paging64_process_root_fork_last_child_root_distinct(void) { return 0u; }
#endif

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

#ifdef LIMITLESS_X64_UEFI_KERNEL
static u32 paging64_vma_user_pt_find_slot(u32 pd_index)
{
    u32 slot;

    for (slot = 0u; slot < PAGING64_VMA_USER_PT_COUNT; ++slot)
    {
        if ((g_paging64_vma_user_pt_initialized[slot] != 0u)
            && (g_paging64_vma_user_pd_index[slot] == pd_index))
        {
            return slot;
        }
    }

    return 0xFFFFFFFFu;
}

static u32 paging64_vma_user_pt_acquire_slot(u32 pd_index)
{
    u32 slot = paging64_vma_user_pt_find_slot(pd_index);

    if (slot != 0xFFFFFFFFu)
    {
        return slot;
    }

    for (slot = 0u; slot < PAGING64_VMA_USER_PT_COUNT; ++slot)
    {
        if (g_paging64_vma_user_pt_initialized[slot] == 0u)
        {
            paging64_zero_table((volatile u64 *)g_paging64_vma_user_pts[slot]);
            g_paging64_vma_user_pd_index[slot] = pd_index;
            g_paging64_vma_user_pt_initialized[slot] = 1u;
            return slot;
        }
    }

    return 0xFFFFFFFFu;
}

static volatile u64 *paging64_user_page_entry_slot(u64 virtual_address)
{
    u32 pml4_index = paging64_index64(virtual_address, 39u);
    u32 pdpt_index = paging64_index64(virtual_address, 30u);
    u32 pd_index = paging64_index64(virtual_address, 21u);
    u32 pt_index = paging64_index64(virtual_address, 12u);
    u32 slot = paging64_vma_user_pt_find_slot(pd_index);

    if ((pml4_index != 0u)
        || (pdpt_index != paging64_index64(0x40000000ull, 30u))
        || (slot == 0xFFFFFFFFu))
    {
        return 0;
    }

    return &g_paging64_vma_user_pts[slot][pt_index];
}

u32 paging64_install_user_page_mapping(u64 virtual_address, u64 physical_address, u32 protection_flags)
{
    volatile u64 *pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
    volatile u64 *pdpt = (volatile u64 *)(u64)PAGING64_PDPT_PHYSICAL;
    volatile u64 *runtime_pd = (volatile u64 *)(u64)PAGING64_RUNTIME_PD_PHYSICAL;
    volatile u64 *vma_pt;
    u32 pml4_index = paging64_index64(virtual_address, 39u);
    u32 pdpt_index = paging64_index64(virtual_address, 30u);
    u32 pd_index = paging64_index64(virtual_address, 21u);
    u32 pt_index = paging64_index64(virtual_address, 12u);
    u32 slot;
    u64 vma_pt_physical;
    u64 pte_flags;

    if (((virtual_address & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || ((physical_address & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || (physical_address == 0ull)
        || ((protection_flags & (PAGING64_USER_PROT_READ
                | PAGING64_USER_PROT_WRITE
                | PAGING64_USER_PROT_EXECUTE)) == 0u)
        || (pml4_index != 0u)
        || (pdpt_index != paging64_index64(0x40000000ull, 30u))
        || (pd_index == paging64_index64(0x40000000ull, 21u))
        || (pd_index == paging64_index64(0x41000000ull, 21u)))
    {
        return 0u;
    }

    slot = paging64_vma_user_pt_acquire_slot(pd_index);
    if (slot == 0xFFFFFFFFu)
    {
        return 0u;
    }
    vma_pt = (volatile u64 *)g_paging64_vma_user_pts[slot];
    vma_pt_physical = paging64_kernel_physical_alias(g_paging64_vma_user_pts[slot]);
    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);
    }

    pml4[pml4_index] = (PAGING64_PDPT_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    pdpt[pdpt_index] = (PAGING64_RUNTIME_PD_PHYSICAL & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    runtime_pd[pd_index] = (vma_pt_physical & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;

    pte_flags = PAGING64_PAGE_PRESENT | PAGING64_PAGE_USER;
    if ((protection_flags & PAGING64_USER_PROT_WRITE) != 0u)
    {
        pte_flags |= PAGING64_PAGE_WRITABLE;
    }
    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        pte_flags |= PAGING64_PAGE_NO_EXECUTE;
    }

    vma_pt[pt_index] = (physical_address & PAGING64_PAGE_MASK) | pte_flags;
    paging64_reload_active_root();
    return 1u;
}

u32 paging64_clear_user_page_mapping(u64 virtual_address)
{
    volatile u64 *slot = paging64_user_page_entry_slot(virtual_address);

    if ((slot == 0) || ((*slot & PAGING64_PAGE_PRESENT) == 0ull))
    {
        return 0u;
    }

    *slot = 0ull;
    paging64_reload_active_root();
    return 1u;
}

u32 paging64_remap_user_page(u64 virtual_address, u64 physical_address, u32 protection_flags)
{
    volatile u64 *slot = paging64_user_page_entry_slot(virtual_address);
    u64 pte_flags;

    if ((slot == 0)
        || ((*slot & PAGING64_PAGE_PRESENT) == 0ull)
        || ((physical_address & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || (physical_address == 0ull)
        || ((protection_flags & (PAGING64_USER_PROT_READ
                | PAGING64_USER_PROT_WRITE
                | PAGING64_USER_PROT_EXECUTE)) == 0u))
    {
        return 0u;
    }

    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);
    }

    pte_flags = PAGING64_PAGE_PRESENT | PAGING64_PAGE_USER;
    if ((protection_flags & PAGING64_USER_PROT_WRITE) != 0u)
    {
        pte_flags |= PAGING64_PAGE_WRITABLE;
    }
    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        pte_flags |= PAGING64_PAGE_NO_EXECUTE;
    }

    *slot = (physical_address & PAGING64_PAGE_MASK) | pte_flags;
    paging64_reload_active_root();
    return 1u;
}

u32 paging64_update_user_page_protection(u64 virtual_address, u32 protection_flags)
{
    volatile u64 *slot = paging64_user_page_entry_slot(virtual_address);
    u64 physical_address;
    u64 pte_flags;

    if ((slot == 0)
        || ((*slot & PAGING64_PAGE_PRESENT) == 0ull)
        || ((protection_flags & (PAGING64_USER_PROT_READ
                | PAGING64_USER_PROT_WRITE
                | PAGING64_USER_PROT_EXECUTE)) == 0u))
    {
        return 0u;
    }

    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);
    }

    physical_address = *slot & PAGING64_PHYSICAL_ADDRESS_MASK;
    pte_flags = PAGING64_PAGE_PRESENT | PAGING64_PAGE_USER;
    if ((protection_flags & PAGING64_USER_PROT_WRITE) != 0u)
    {
        pte_flags |= PAGING64_PAGE_WRITABLE;
    }
    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        pte_flags |= PAGING64_PAGE_NO_EXECUTE;
    }

    *slot = physical_address | pte_flags;
    paging64_reload_active_root();
    return 1u;
}

u32 paging64_user_page_present(u64 virtual_address)
{
    volatile u64 *slot = paging64_user_page_entry_slot(virtual_address);

    return ((slot != 0) && ((*slot & PAGING64_PAGE_PRESENT) != 0ull)) ? 1u : 0u;
}

u64 paging64_user_page_physical(u64 virtual_address)
{
    volatile u64 *slot = paging64_user_page_entry_slot(virtual_address);

    return ((slot != 0) && ((*slot & PAGING64_PAGE_PRESENT) != 0ull))
        ? (*slot & PAGING64_PHYSICAL_ADDRESS_MASK)
        : 0ull;
}

u32 paging64_user_page_protection(u64 virtual_address)
{
    volatile u64 *slot = paging64_user_page_entry_slot(virtual_address);
    u32 protection = 0u;

    if ((slot == 0) || ((*slot & PAGING64_PAGE_PRESENT) == 0ull))
    {
        return 0u;
    }

    protection |= PAGING64_USER_PROT_READ;
    if ((*slot & PAGING64_PAGE_WRITABLE) != 0ull)
    {
        protection |= PAGING64_USER_PROT_WRITE;
    }
    if ((*slot & PAGING64_PAGE_NO_EXECUTE) == 0ull)
    {
        protection |= PAGING64_USER_PROT_EXECUTE;
    }

    return protection;
}

static u32 paging64_process_vma_user_pt_find_slot(u32 root_slot, u32 pd_index)
{
    u32 slot;

    if (root_slot >= PAGING64_PROCESS_ROOT_POOL_LIMIT)
    {
        return 0xFFFFFFFFu;
    }

    for (slot = 0u; slot < PAGING64_PROCESS_ROOT_VMA_PT_COUNT; ++slot)
    {
        if ((g_paging64_process_vma_pt_initialized[root_slot][slot] != 0u)
            && (g_paging64_process_vma_pt_pd_index[root_slot][slot] == pd_index))
        {
            return slot;
        }
    }

    return 0xFFFFFFFFu;
}

static u32 paging64_process_vma_user_pt_acquire_slot(u32 root_slot, u32 pd_index)
{
    u32 slot = paging64_process_vma_user_pt_find_slot(root_slot, pd_index);

    if (root_slot >= PAGING64_PROCESS_ROOT_POOL_LIMIT)
    {
        return 0xFFFFFFFFu;
    }
    if (slot != 0xFFFFFFFFu)
    {
        return slot;
    }

    for (slot = 0u; slot < PAGING64_PROCESS_ROOT_VMA_PT_COUNT; ++slot)
    {
        if (g_paging64_process_vma_pt_initialized[root_slot][slot] == 0u)
        {
            paging64_zero_table((volatile u64 *)g_paging64_process_vma_pts[root_slot][slot]);
            g_paging64_process_vma_pt_pd_index[root_slot][slot] = pd_index;
            g_paging64_process_vma_pt_initialized[root_slot][slot] = 1u;
            return slot;
        }
    }

    return 0xFFFFFFFFu;
}

static volatile u64 *paging64_process_user_page_entry_slot(u32 pid, u64 virtual_address)
{
    u32 root_slot = paging64_process_root_find_slot(pid);
    u32 pml4_index = paging64_index64(virtual_address, 39u);
    u32 pdpt_index = paging64_index64(virtual_address, 30u);
    u32 pd_index = paging64_index64(virtual_address, 21u);
    u32 pt_index = paging64_index64(virtual_address, 12u);
    u32 pt_slot;

    if ((root_slot == 0xFFFFFFFFu)
        || (pml4_index != 0u)
        || (pdpt_index != paging64_index64(0x40000000ull, 30u)))
    {
        return 0;
    }

    pt_slot = paging64_process_vma_user_pt_find_slot(root_slot, pd_index);
    if (pt_slot == 0xFFFFFFFFu)
    {
        return 0;
    }

    return &g_paging64_process_vma_pts[root_slot][pt_slot][pt_index];
}

static void paging64_reload_root_if_active(u64 root_physical)
{
    if ((read_cr3_64() & PAGING64_PAGE_MASK) == (root_physical & PAGING64_PAGE_MASK))
    {
        paging64_reload_active_root();
    }
}

u32 paging64_install_user_page_mapping_for_process(
    u32 pid,
    u64 virtual_address,
    u64 physical_address,
    u32 protection_flags)
{
    u32 root_slot = paging64_process_root_find_slot(pid);
    volatile u64 *pml4;
    volatile u64 *pdpt;
    volatile u64 *runtime_pd;
    volatile u64 *vma_pt;
    u64 root_physical;
    u64 pdpt_physical;
    u64 runtime_pd_physical;
    u64 vma_pt_physical;
    u64 pte_flags;
    u32 pml4_index = paging64_index64(virtual_address, 39u);
    u32 pdpt_index = paging64_index64(virtual_address, 30u);
    u32 pd_index = paging64_index64(virtual_address, 21u);
    u32 pt_index = paging64_index64(virtual_address, 12u);
    u32 pt_slot;

    if ((root_slot == 0xFFFFFFFFu)
        || ((virtual_address & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || ((physical_address & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || (physical_address == 0ull)
        || ((protection_flags & (PAGING64_USER_PROT_READ
                | PAGING64_USER_PROT_WRITE
                | PAGING64_USER_PROT_EXECUTE)) == 0u)
        || (pml4_index != 0u)
        || (pdpt_index != paging64_index64(0x40000000ull, 30u))
        || (pd_index == paging64_index64(0x40000000ull, 21u))
        || (pd_index == paging64_index64(0x41000000ull, 21u)))
    {
        return 0u;
    }

    pt_slot = paging64_process_vma_user_pt_acquire_slot(root_slot, pd_index);
    if (pt_slot == 0xFFFFFFFFu)
    {
        g_paging64_process_root_last_vma_pt_private = 0u;
        return 0u;
    }

    pml4 = (volatile u64 *)g_paging64_process_pml4[root_slot];
    pdpt = (volatile u64 *)g_paging64_process_pdpt[root_slot];
    runtime_pd = (volatile u64 *)g_paging64_process_runtime_pd[root_slot];
    vma_pt = (volatile u64 *)g_paging64_process_vma_pts[root_slot][pt_slot];
    root_physical = paging64_kernel_physical_alias(g_paging64_process_pml4[root_slot]);
    pdpt_physical = paging64_kernel_physical_alias(g_paging64_process_pdpt[root_slot]);
    runtime_pd_physical = paging64_kernel_physical_alias(g_paging64_process_runtime_pd[root_slot]);
    vma_pt_physical =
        paging64_kernel_physical_alias(g_paging64_process_vma_pts[root_slot][pt_slot]);

    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);
    }

    pml4[pml4_index] = (pdpt_physical & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    pdpt[pdpt_index] = (runtime_pd_physical & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;
    runtime_pd[pd_index] = (vma_pt_physical & PAGING64_PAGE_MASK)
        | PAGING64_PAGE_PRESENT
        | PAGING64_PAGE_WRITABLE
        | PAGING64_PAGE_USER;

    pte_flags = PAGING64_PAGE_PRESENT | PAGING64_PAGE_USER;
    if ((protection_flags & PAGING64_USER_PROT_WRITE) != 0u)
    {
        pte_flags |= PAGING64_PAGE_WRITABLE;
    }
    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        pte_flags |= PAGING64_PAGE_NO_EXECUTE;
    }

    vma_pt[pt_index] = (physical_address & PAGING64_PAGE_MASK) | pte_flags;
    g_paging64_process_root_last_vma_pt_private = 1u;
    ++g_paging64_process_root_vma_pt_private_count;
    paging64_reload_root_if_active(root_physical);
    return 1u;
}

u32 paging64_clear_user_page_mapping_for_process(u32 pid, u64 virtual_address)
{
    volatile u64 *slot = paging64_process_user_page_entry_slot(pid, virtual_address);
    u64 root_physical = paging64_process_root_physical(pid);

    if ((slot == 0) || ((*slot & PAGING64_PAGE_PRESENT) == 0ull))
    {
        return 0u;
    }

    *slot = 0ull;
    paging64_reload_root_if_active(root_physical);
    return 1u;
}

u32 paging64_remap_user_page_for_process(
    u32 pid,
    u64 virtual_address,
    u64 physical_address,
    u32 protection_flags)
{
    volatile u64 *slot = paging64_process_user_page_entry_slot(pid, virtual_address);
    u64 root_physical = paging64_process_root_physical(pid);
    u64 pte_flags;

    if ((slot == 0)
        || ((*slot & PAGING64_PAGE_PRESENT) == 0ull)
        || ((physical_address & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || (physical_address == 0ull)
        || ((protection_flags & (PAGING64_USER_PROT_READ
                | PAGING64_USER_PROT_WRITE
                | PAGING64_USER_PROT_EXECUTE)) == 0u))
    {
        return 0u;
    }

    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);
    }

    pte_flags = PAGING64_PAGE_PRESENT | PAGING64_PAGE_USER;
    if ((protection_flags & PAGING64_USER_PROT_WRITE) != 0u)
    {
        pte_flags |= PAGING64_PAGE_WRITABLE;
    }
    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        pte_flags |= PAGING64_PAGE_NO_EXECUTE;
    }

    *slot = (physical_address & PAGING64_PAGE_MASK) | pte_flags;
    paging64_reload_root_if_active(root_physical);
    return 1u;
}

u32 paging64_update_user_page_protection_for_process(
    u32 pid,
    u64 virtual_address,
    u32 protection_flags)
{
    volatile u64 *slot = paging64_process_user_page_entry_slot(pid, virtual_address);
    u64 root_physical = paging64_process_root_physical(pid);
    u64 physical_address;
    u64 pte_flags;

    if ((slot == 0)
        || ((*slot & PAGING64_PAGE_PRESENT) == 0ull)
        || ((protection_flags & (PAGING64_USER_PROT_READ
                | PAGING64_USER_PROT_WRITE
                | PAGING64_USER_PROT_EXECUTE)) == 0u))
    {
        return 0u;
    }

    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        wrmsr64(PAGING64_EFER_MSR, rdmsr64(PAGING64_EFER_MSR) | PAGING64_EFER_NXE);
    }

    physical_address = *slot & PAGING64_PHYSICAL_ADDRESS_MASK;
    pte_flags = PAGING64_PAGE_PRESENT | PAGING64_PAGE_USER;
    if ((protection_flags & PAGING64_USER_PROT_WRITE) != 0u)
    {
        pte_flags |= PAGING64_PAGE_WRITABLE;
    }
    if ((protection_flags & PAGING64_USER_PROT_EXECUTE) == 0u)
    {
        pte_flags |= PAGING64_PAGE_NO_EXECUTE;
    }

    *slot = physical_address | pte_flags;
    paging64_reload_root_if_active(root_physical);
    return 1u;
}

u32 paging64_user_page_present_for_process(u32 pid, u64 virtual_address)
{
    volatile u64 *slot = paging64_process_user_page_entry_slot(pid, virtual_address);

    return ((slot != 0) && ((*slot & PAGING64_PAGE_PRESENT) != 0ull)) ? 1u : 0u;
}

u64 paging64_user_page_physical_for_process(u32 pid, u64 virtual_address)
{
    volatile u64 *slot = paging64_process_user_page_entry_slot(pid, virtual_address);

    return ((slot != 0) && ((*slot & PAGING64_PAGE_PRESENT) != 0ull))
        ? (*slot & PAGING64_PHYSICAL_ADDRESS_MASK)
        : 0ull;
}

u32 paging64_user_page_protection_for_process(u32 pid, u64 virtual_address)
{
    volatile u64 *slot = paging64_process_user_page_entry_slot(pid, virtual_address);
    u32 protection = 0u;

    if ((slot == 0) || ((*slot & PAGING64_PAGE_PRESENT) == 0ull))
    {
        return 0u;
    }

    protection |= PAGING64_USER_PROT_READ;
    if ((*slot & PAGING64_PAGE_WRITABLE) != 0ull)
    {
        protection |= PAGING64_USER_PROT_WRITE;
    }
    if ((*slot & PAGING64_PAGE_NO_EXECUTE) == 0ull)
    {
        protection |= PAGING64_USER_PROT_EXECUTE;
    }

    return protection;
}
#endif

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
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u64 kernel_root;
    u64 previous_root = 0ull;
    u32 restore_previous_root = 0u;
    u32 result = 0u;
#endif

    if ((page_count == 0u)
        || (page_count > PAGING64_ENTRY_COUNT)
        || ((virtual_base & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || ((physical_base & ((u64)PAGING64_PAGE_BYTES - 1ull)) != 0ull)
        || (paging64_cpu_supports_nx() == 0u))
    {
        return 0u;
    }

#ifdef LIMITLESS_X64_UEFI_KERNEL
    kernel_root = paging64_ensure_kernel_root_physical();
    previous_root = paging64_current_root_physical();
    if (previous_root != (kernel_root & PAGING64_PAGE_MASK))
    {
        if (paging64_switch_to_kernel_root(0x4D4D494Fu) == 0u)
        {
            return 0u;
        }
        restore_previous_root = 1u;
        pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
        high_pdpt = (volatile u64 *)(u64)PAGING64_HIGH_PDPT_PHYSICAL;
        kernel_pd = (volatile u64 *)(u64)PAGING64_KERNEL_PD_PHYSICAL;
        mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    }
#endif

    physical_end = physical_base + ((u64)page_count * (u64)PAGING64_PAGE_BYTES);
    if (physical_end <= physical_base)
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        goto paging64_install_kernel_mmio_mapping_done;
#else
        return 0u;
#endif
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
#ifdef LIMITLESS_X64_UEFI_KERNEL
        goto paging64_install_kernel_mmio_mapping_done;
#else
        return 0u;
#endif
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
#ifdef LIMITLESS_X64_UEFI_KERNEL
        goto paging64_install_kernel_mmio_mapping_done;
#else
        return 0u;
#endif
    }

    if (((kernel_pd[pd_index] & PAGING64_PAGE_PRESENT) != 0ull)
        && ((kernel_pd[pd_index] & PAGING64_PHYSICAL_ADDRESS_MASK) !=
            (PAGING64_KERNEL_MMIO_PT_PHYSICAL & PAGING64_PHYSICAL_ADDRESS_MASK)))
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        goto paging64_install_kernel_mmio_mapping_done;
#else
        return 0u;
#endif
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
#ifdef LIMITLESS_X64_UEFI_KERNEL
    result = 1u;
paging64_install_kernel_mmio_mapping_done:
    if (restore_previous_root != 0u)
    {
        write_cr3_64(previous_root & PAGING64_PAGE_MASK);
    }
    return result;
#else
    return 1u;
#endif
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
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u64 previous_root = paging64_current_root_physical();
    u64 kernel_root = paging64_ensure_kernel_root_physical() & PAGING64_PAGE_MASK;
    u32 restore_previous_root = 0u;

    if (previous_root != kernel_root)
    {
        if (paging64_switch_to_kernel_root(0x4D574F50u) == 0u)
        {
            return 0u;
        }
        restore_previous_root = 1u;
        mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    }
#endif

    if ((g_kernel_mmio_mapping_installed == 0u)
        || (pt_index >= PAGING64_ENTRY_COUNT)
        || ((mmio_pt[pt_index] & PAGING64_PAGE_PRESENT) == 0ull))
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        if (restore_previous_root != 0u)
        {
            write_cr3_64(previous_root & PAGING64_PAGE_MASK);
        }
#endif
        return 0u;
    }

    mmio_pt[pt_index] |= PAGING64_PAGE_WRITABLE;
    paging64_reload_active_root();
#ifdef LIMITLESS_X64_UEFI_KERNEL
    if (restore_previous_root != 0u)
    {
        write_cr3_64(previous_root & PAGING64_PAGE_MASK);
    }
#endif
    return 1u;
}

u32 paging64_kernel_mmio_write_window_close(u32 page_index)
{
    volatile u64 *mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    u32 pt_index = g_kernel_mmio_mapping_pt_index + page_index;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u64 previous_root = paging64_current_root_physical();
    u64 kernel_root = paging64_ensure_kernel_root_physical() & PAGING64_PAGE_MASK;
    u32 restore_previous_root = 0u;

    if (previous_root != kernel_root)
    {
        if (paging64_switch_to_kernel_root(0x4D574350u) == 0u)
        {
            return 0u;
        }
        restore_previous_root = 1u;
        mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    }
#endif

    if ((g_kernel_mmio_mapping_installed == 0u)
        || (pt_index >= PAGING64_ENTRY_COUNT)
        || ((mmio_pt[pt_index] & PAGING64_PAGE_PRESENT) == 0ull))
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        if (restore_previous_root != 0u)
        {
            write_cr3_64(previous_root & PAGING64_PAGE_MASK);
        }
#endif
        return 0u;
    }

    mmio_pt[pt_index] &= ~PAGING64_PAGE_WRITABLE;
    paging64_reload_active_root();
#ifdef LIMITLESS_X64_UEFI_KERNEL
    if (restore_previous_root != 0u)
    {
        write_cr3_64(previous_root & PAGING64_PAGE_MASK);
    }
#endif
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
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u64 previous_root = paging64_current_root_physical();
    u64 kernel_root = paging64_ensure_kernel_root_physical() & PAGING64_PAGE_MASK;
    u32 restore_previous_root = 0u;

    if (previous_root != kernel_root)
    {
        if (paging64_switch_to_kernel_root(0x4D575650u) == 0u)
        {
            return 0u;
        }
        restore_previous_root = 1u;
        pml4 = (volatile u64 *)(u64)PAGING64_PML4_PHYSICAL;
        high_pdpt = (volatile u64 *)(u64)PAGING64_HIGH_PDPT_PHYSICAL;
        kernel_pd = (volatile u64 *)(u64)PAGING64_KERNEL_PD_PHYSICAL;
        mmio_pt = (volatile u64 *)(u64)PAGING64_KERNEL_MMIO_PT_PHYSICAL;
    }
#endif

    if ((pml4_index != PAGING64_HIGH_HALF_PML4_INDEX)
        || (pdpt_index != PAGING64_HIGH_HALF_PDPT_INDEX)
        || ((pml4[pml4_index] & PAGING64_PAGE_PRESENT) == 0ull)
        || ((high_pdpt[pdpt_index] & PAGING64_PAGE_PRESENT) == 0ull)
        || ((kernel_pd[pd_index] & PAGING64_PAGE_PRESENT) == 0ull)
        || ((kernel_pd[pd_index] & PAGING64_PHYSICAL_ADDRESS_MASK) !=
            (PAGING64_KERNEL_MMIO_PT_PHYSICAL & PAGING64_PHYSICAL_ADDRESS_MASK))
        || ((mmio_pt[pt_index] & PAGING64_PAGE_PRESENT) == 0ull))
    {
#ifdef LIMITLESS_X64_UEFI_KERNEL
        if (restore_previous_root != 0u)
        {
            write_cr3_64(previous_root & PAGING64_PAGE_MASK);
        }
#endif
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
#ifdef LIMITLESS_X64_UEFI_KERNEL
    if (restore_previous_root != 0u)
    {
        write_cr3_64(previous_root & PAGING64_PAGE_MASK);
    }
#endif
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
