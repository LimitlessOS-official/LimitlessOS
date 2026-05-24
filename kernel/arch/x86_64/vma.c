#include "vma_x64.h"

#include "paging_x64.h"
#include "persona_x64.h"
#include "process_x64.h"

/*
 * A.2-A.8a add the first per-process VMA substrate plus bounded anonymous map
 * and unmap primitives. The code integrates with process_x64.h through
 * PID-based accessors and with paging_x64.h for user PTE installation/removal
 * and permission updates; scaffold checkpoints prove tree insertion/denial,
 * map, teardown, protect-time permission splitting, RB-tree lookup bounds,
 * copy-on-write fault handling, kernel-internal brk growth/shrink, and the
 * read-only A.9 diagnostic surface.
 */

#define VMA64_MAX_PROCESS_TREES 16u
#define VMA64_MAX_STATIC_REGIONS 1152u
#define VMA64_MAX_ANON_PAGES 16u
#define VMA64_ANON_HINT_BASE 0x0000000044000000ull
#define VMA64_ANON_HINT_LIMIT 0x0000000044200000ull
#define VMA64_BRK_BASE_DEFAULT 0x0000000044100000ull
#define VMA64_BRK_LIMIT_DEFAULT 0x0000000044180000ull

static vma_tree_t g_vma64_trees[VMA64_MAX_PROCESS_TREES];
static u32 g_vma64_tree_pids[VMA64_MAX_PROCESS_TREES];
static vma_region_t g_vma64_regions[VMA64_MAX_STATIC_REGIONS];
static u32 g_vma64_region_used[VMA64_MAX_STATIC_REGIONS];
static u8 g_vma64_anon_pages[VMA64_MAX_ANON_PAGES][VMA64_PAGE_BYTES] __attribute__((aligned(VMA64_PAGE_BYTES)));
static u32 g_vma64_anon_page_used[VMA64_MAX_ANON_PAGES];
static u32 g_vma64_anon_claimed_pages = 0u;
static u32 g_vma64_cow_fault_count = 0u;
static u32 g_vma64_last_lookup_steps = 0u;
static u32 g_vma64_peak_lookup_steps = 0u;
static u32 g_vma64_last_map_stage = 0u;
static u32 g_vma64_last_unmap_stage = 0u;
static u32 g_vma64_initialized = 0u;

static u64 vma64_align_up(u64 value, u64 alignment)
{
    u64 mask;

    if (alignment == 0ull)
    {
        return value;
    }

    mask = alignment - 1ull;
    if ((alignment & mask) != 0ull)
    {
        return 0ull;
    }

    return (value + mask) & ~mask;
}

static u64 vma64_align_down(u64 value, u64 alignment)
{
    if (alignment == 0ull)
    {
        return value;
    }

    return value & ~(alignment - 1ull);
}

static u32 vma64_page_count_from_length(u64 length)
{
    if ((length == 0ull)
        || ((length & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((length / VMA64_PAGE_BYTES) > 0xFFFFFFFFull))
    {
        return 0u;
    }

    return (u32)(length / VMA64_PAGE_BYTES);
}

static u32 vma64_paging_prot(u32 prot_flags)
{
    u32 paging_prot = 0u;

    if ((prot_flags & VMA64_PROT_READ) != 0u)
    {
        paging_prot |= PAGING64_USER_PROT_READ;
    }
    if ((prot_flags & VMA64_PROT_WRITE) != 0u)
    {
        paging_prot |= PAGING64_USER_PROT_WRITE;
    }
    if ((prot_flags & VMA64_PROT_EXECUTE) != 0u)
    {
        paging_prot |= PAGING64_USER_PROT_EXECUTE;
    }

    return paging_prot;
}

static u32 vma64_split_token(u32 token, u32 salt)
{
    u32 split = token ^ salt;

    return (split != VMA64_INVALID_TOKEN) ? split : salt;
}

static void vma64_clear_tree(vma_tree_t *tree)
{
    if (tree == 0)
    {
        return;
    }

    tree->head = 0;
    tree->rb_root = 0;
    tree->region_count = 0u;
    tree->peak_region_count = 0u;
    tree->mapped_bytes = 0ull;
    tree->peak_mapped_bytes = 0ull;
    tree->brk_base = 0ull;
    tree->brk_current = 0ull;
    tree->brk_peak = 0ull;
}

static void vma64_clear_region(vma_region_t *region)
{
    if (region == 0)
    {
        return;
    }

    region->virt_base = 0ull;
    region->virt_end = 0ull;
    region->phys_base = 0ull;
    region->prot_flags = 0u;
    region->map_flags = 0u;
    region->backing_type = 0u;
    region->backing_handle = VMA64_BACKING_HANDLE_NONE;
    region->vma_token = VMA64_INVALID_TOKEN;
    region->reserved = 0u;
    region->prev = 0;
    region->next = 0;
    region->rb_parent = 0;
    region->rb_left = 0;
    region->rb_right = 0;
    region->rb_color = VMA64_RB_BLACK;
}

static void vma64_zero_page(void *page)
{
    u32 index;
    u8 *bytes = (u8 *)page;

    if (page == 0)
    {
        return;
    }

    for (index = 0u; index < VMA64_PAGE_BYTES; ++index)
    {
        bytes[index] = 0u;
    }
}

static void vma64_copy_page(void *target, const void *source)
{
    u32 index;
    u8 *target_bytes = (u8 *)target;
    const u8 *source_bytes = (const u8 *)source;

    if ((target == 0) || (source == 0))
    {
        return;
    }

    for (index = 0u; index < VMA64_PAGE_BYTES; ++index)
    {
        target_bytes[index] = source_bytes[index];
    }
}

static u32 vma64_page_checksum(const void *page)
{
    const u8 *bytes = (const u8 *)page;
    u32 index;
    u32 digest = 2166136261u;

    if (page == 0)
    {
        return 0u;
    }

    for (index = 0u; index < VMA64_PAGE_BYTES; ++index)
    {
        digest ^= bytes[index];
        digest *= 16777619u;
    }

    return (digest != 0u) ? digest : 1u;
}

static u32 vma64_range_valid(u64 virt_base, u64 virt_end)
{
    if ((virt_base >= virt_end)
        || ((virt_base & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((virt_end & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        return 0u;
    }

    return 1u;
}

static u32 vma64_ranges_overlap(u64 left_base, u64 left_end, u64 right_base, u64 right_end)
{
    return ((left_base < right_end) && (right_base < left_end)) ? 1u : 0u;
}

static u32 vma64_region_is_red(const vma_region_t *region)
{
    return ((region != 0) && (region->rb_color == VMA64_RB_RED)) ? 1u : 0u;
}

static void vma64_rb_rotate_left(vma_tree_t *tree, vma_region_t *node)
{
    vma_region_t *right;

    if ((tree == 0) || (node == 0) || (node->rb_right == 0))
    {
        return;
    }

    right = node->rb_right;
    node->rb_right = right->rb_left;
    if (right->rb_left != 0)
    {
        right->rb_left->rb_parent = node;
    }

    right->rb_parent = node->rb_parent;
    if (node->rb_parent == 0)
    {
        tree->rb_root = right;
    }
    else if (node == node->rb_parent->rb_left)
    {
        node->rb_parent->rb_left = right;
    }
    else
    {
        node->rb_parent->rb_right = right;
    }

    right->rb_left = node;
    node->rb_parent = right;
}

static void vma64_rb_rotate_right(vma_tree_t *tree, vma_region_t *node)
{
    vma_region_t *left;

    if ((tree == 0) || (node == 0) || (node->rb_left == 0))
    {
        return;
    }

    left = node->rb_left;
    node->rb_left = left->rb_right;
    if (left->rb_right != 0)
    {
        left->rb_right->rb_parent = node;
    }

    left->rb_parent = node->rb_parent;
    if (node->rb_parent == 0)
    {
        tree->rb_root = left;
    }
    else if (node == node->rb_parent->rb_right)
    {
        node->rb_parent->rb_right = left;
    }
    else
    {
        node->rb_parent->rb_left = left;
    }

    left->rb_right = node;
    node->rb_parent = left;
}

static void vma64_rb_insert_fixup(vma_tree_t *tree, vma_region_t *node)
{
    vma_region_t *parent;
    vma_region_t *grandparent;
    vma_region_t *uncle;

    while ((node != 0)
        && (node->rb_parent != 0)
        && (vma64_region_is_red(node->rb_parent) != 0u))
    {
        parent = node->rb_parent;
        grandparent = parent->rb_parent;
        if (grandparent == 0)
        {
            break;
        }

        if (parent == grandparent->rb_left)
        {
            uncle = grandparent->rb_right;
            if (vma64_region_is_red(uncle) != 0u)
            {
                parent->rb_color = VMA64_RB_BLACK;
                uncle->rb_color = VMA64_RB_BLACK;
                grandparent->rb_color = VMA64_RB_RED;
                node = grandparent;
            }
            else
            {
                if (node == parent->rb_right)
                {
                    node = parent;
                    vma64_rb_rotate_left(tree, node);
                    parent = node->rb_parent;
                    grandparent = (parent != 0) ? parent->rb_parent : 0;
                }
                if ((parent != 0) && (grandparent != 0))
                {
                    parent->rb_color = VMA64_RB_BLACK;
                    grandparent->rb_color = VMA64_RB_RED;
                    vma64_rb_rotate_right(tree, grandparent);
                }
            }
        }
        else
        {
            uncle = grandparent->rb_left;
            if (vma64_region_is_red(uncle) != 0u)
            {
                parent->rb_color = VMA64_RB_BLACK;
                uncle->rb_color = VMA64_RB_BLACK;
                grandparent->rb_color = VMA64_RB_RED;
                node = grandparent;
            }
            else
            {
                if (node == parent->rb_left)
                {
                    node = parent;
                    vma64_rb_rotate_right(tree, node);
                    parent = node->rb_parent;
                    grandparent = (parent != 0) ? parent->rb_parent : 0;
                }
                if ((parent != 0) && (grandparent != 0))
                {
                    parent->rb_color = VMA64_RB_BLACK;
                    grandparent->rb_color = VMA64_RB_RED;
                    vma64_rb_rotate_left(tree, grandparent);
                }
            }
        }
    }

    if (tree->rb_root != 0)
    {
        tree->rb_root->rb_color = VMA64_RB_BLACK;
    }
}

static u32 vma64_rb_insert(vma_tree_t *tree, vma_region_t *region)
{
    vma_region_t *parent = 0;
    vma_region_t *cursor;

    if ((tree == 0) || (region == 0))
    {
        return 0u;
    }

    region->rb_parent = 0;
    region->rb_left = 0;
    region->rb_right = 0;
    region->rb_color = VMA64_RB_RED;

    cursor = tree->rb_root;
    while (cursor != 0)
    {
        parent = cursor;
        if (region->virt_base < cursor->virt_base)
        {
            cursor = cursor->rb_left;
        }
        else if (region->virt_base > cursor->virt_base)
        {
            cursor = cursor->rb_right;
        }
        else
        {
            return 0u;
        }
    }

    region->rb_parent = parent;
    if (parent == 0)
    {
        tree->rb_root = region;
    }
    else if (region->virt_base < parent->virt_base)
    {
        parent->rb_left = region;
    }
    else
    {
        parent->rb_right = region;
    }

    vma64_rb_insert_fixup(tree, region);
    return 1u;
}

static void vma64_rb_rebuild(vma_tree_t *tree)
{
    vma_region_t *cursor;

    if (tree == 0)
    {
        return;
    }

    tree->rb_root = 0;
    cursor = tree->head;
    while (cursor != 0)
    {
        cursor->rb_parent = 0;
        cursor->rb_left = 0;
        cursor->rb_right = 0;
        cursor->rb_color = VMA64_RB_BLACK;
        (void)vma64_rb_insert(tree, cursor);
        cursor = cursor->next;
    }
}

static vma_region_t *vma64_detach_region(vma_tree_t *tree, vma_region_t *region)
{
    vma_region_t *cursor;
    u64 bytes;

    if ((tree == 0) || (region == 0))
    {
        return 0;
    }

    cursor = tree->head;
    while ((cursor != 0) && (cursor != region))
    {
        cursor = cursor->next;
    }
    if (cursor == region)
    {
        if (region->prev != 0)
        {
            region->prev->next = region->next;
        }
        else
        {
            tree->head = region->next;
        }
        if (region->next != 0)
        {
            region->next->prev = region->prev;
        }
    }

    bytes = region->virt_end - region->virt_base;
    region->prev = 0;
    region->next = 0;
    if (tree->region_count != 0u)
    {
        --tree->region_count;
    }
    if (tree->mapped_bytes >= bytes)
    {
        tree->mapped_bytes -= bytes;
    }
    else
    {
        tree->mapped_bytes = 0ull;
    }
    region->rb_parent = 0;
    region->rb_left = 0;
    region->rb_right = 0;
    region->rb_color = VMA64_RB_BLACK;
    vma64_rb_rebuild(tree);
    return region;
}

void vma64_init(void)
{
    u32 index;

    if (g_vma64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < VMA64_MAX_PROCESS_TREES; ++index)
    {
        g_vma64_tree_pids[index] = PROCESS64_INVALID_PID;
        vma64_clear_tree(&g_vma64_trees[index]);
    }

    for (index = 0u; index < VMA64_MAX_STATIC_REGIONS; ++index)
    {
        g_vma64_region_used[index] = 0u;
        vma64_clear_region(&g_vma64_regions[index]);
    }

    for (index = 0u; index < VMA64_MAX_ANON_PAGES; ++index)
    {
        g_vma64_anon_page_used[index] = 0u;
        vma64_zero_page(g_vma64_anon_pages[index]);
    }
    g_vma64_anon_claimed_pages = 0u;
    g_vma64_cow_fault_count = 0u;

    g_vma64_initialized = 1u;
}

static u64 vma64_claim_anon_pages(u32 page_count)
{
    u32 index;
    u32 run;

    vma64_init();

    if ((page_count == 0u) || (page_count > VMA64_MAX_ANON_PAGES))
    {
        return 0ull;
    }

    for (index = 0u; (index + page_count) <= VMA64_MAX_ANON_PAGES; ++index)
    {
        for (run = 0u; run < page_count; ++run)
        {
            if (g_vma64_anon_page_used[index + run] != 0u)
            {
                break;
            }
        }

        if (run == page_count)
        {
            for (run = 0u; run < page_count; ++run)
            {
                g_vma64_anon_page_used[index + run] = 1u;
                vma64_zero_page(g_vma64_anon_pages[index + run]);
            }
            g_vma64_anon_claimed_pages += page_count;
            return paging64_kernel_physical_alias(g_vma64_anon_pages[index]);
        }
    }

    return 0ull;
}

static u32 vma64_anon_page_index(u64 physical_address)
{
    u32 index;

    if (physical_address == 0ull)
    {
        return VMA64_MAX_ANON_PAGES;
    }

    for (index = 0u; index < VMA64_MAX_ANON_PAGES; ++index)
    {
        if (paging64_kernel_physical_alias(g_vma64_anon_pages[index]) == physical_address)
        {
            return index;
        }
    }

    return VMA64_MAX_ANON_PAGES;
}

static void *vma64_anon_page_ptr(u64 physical_address)
{
    u32 index = vma64_anon_page_index(physical_address);

    return (index < VMA64_MAX_ANON_PAGES) ? g_vma64_anon_pages[index] : 0;
}

static u32 vma64_retain_anon_page(u64 physical_address)
{
    u32 index = vma64_anon_page_index(physical_address);

    if ((index >= VMA64_MAX_ANON_PAGES)
        || (g_vma64_anon_page_used[index] == 0u))
    {
        return 0u;
    }

    ++g_vma64_anon_page_used[index];
    ++g_vma64_anon_claimed_pages;
    return 1u;
}

static void vma64_release_anon_pages(u64 physical_address, u32 page_count)
{
    u32 index;
    u32 run;

    if ((physical_address == 0ull)
        || (page_count == 0u)
        || (page_count > VMA64_MAX_ANON_PAGES))
    {
        return;
    }

    for (index = 0u; (index + page_count) <= VMA64_MAX_ANON_PAGES; ++index)
    {
        if (paging64_kernel_physical_alias(g_vma64_anon_pages[index]) == physical_address)
        {
            for (run = 0u; run < page_count; ++run)
            {
                if (g_vma64_anon_page_used[index + run] != 0u)
                {
                    --g_vma64_anon_page_used[index + run];
                    if (g_vma64_anon_claimed_pages != 0u)
                    {
                        --g_vma64_anon_claimed_pages;
                    }
                }
                if (g_vma64_anon_page_used[index + run] == 0u)
                {
                    vma64_zero_page(g_vma64_anon_pages[index + run]);
                }
            }
            return;
        }
    }
}

u32 vma64_init_process(u32 pid)
{
    u32 index;

    vma64_init();

    if (pid == PROCESS64_INVALID_PID)
    {
        return 0u;
    }

    if (process64_vma_root(pid) != 0)
    {
        return 1u;
    }

    for (index = 0u; index < VMA64_MAX_PROCESS_TREES; ++index)
    {
        if (g_vma64_tree_pids[index] == PROCESS64_INVALID_PID)
        {
            vma64_clear_tree(&g_vma64_trees[index]);
            if (process64_attach_vma(pid, &g_vma64_trees[index]) == 0u)
            {
                return 0u;
            }
            g_vma64_tree_pids[index] = pid;
            return 1u;
        }
    }

    return 0u;
}

vma_tree_t *vma64_tree_for_process(u32 pid)
{
    vma64_init();
    return (vma_tree_t *)process64_vma_root(pid);
}

const vma_region_t *vma64_first_region(u32 pid)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    return (tree != 0) ? tree->head : 0;
}

const vma_region_t *vma64_next_region(const vma_region_t *region)
{
    return (region != 0) ? region->next : 0;
}

vma_region_t *vma64_region_acquire(void)
{
    u32 index;

    vma64_init();

    for (index = 0u; index < VMA64_MAX_STATIC_REGIONS; ++index)
    {
        if (g_vma64_region_used[index] == 0u)
        {
            g_vma64_region_used[index] = 1u;
            vma64_clear_region(&g_vma64_regions[index]);
            return &g_vma64_regions[index];
        }
    }

    return 0;
}

void vma64_region_release(vma_region_t *region)
{
    u32 index;

    if (region == 0)
    {
        return;
    }

    vma64_init();

    for (index = 0u; index < VMA64_MAX_STATIC_REGIONS; ++index)
    {
        if (&g_vma64_regions[index] == region)
        {
            vma64_clear_region(region);
            g_vma64_region_used[index] = 0u;
            return;
        }
    }
}

u32 vma64_region_prepare(
    vma_region_t *region,
    u64 virt_base,
    u64 virt_end,
    u64 phys_base,
    u32 prot_flags,
    u32 map_flags,
    u32 backing_type,
    u32 backing_handle,
    u32 vma_token)
{
    if ((region == 0)
        || (vma64_range_valid(virt_base, virt_end) == 0u)
        || (vma_token == VMA64_INVALID_TOKEN))
    {
        return 0u;
    }

    region->virt_base = virt_base;
    region->virt_end = virt_end;
    region->phys_base = phys_base;
    region->prot_flags = prot_flags;
    region->map_flags = map_flags;
    region->backing_type = backing_type;
    region->backing_handle = backing_handle;
    region->vma_token = vma_token;
    region->reserved = 0u;
    region->prev = 0;
    region->next = 0;
    region->rb_parent = 0;
    region->rb_left = 0;
    region->rb_right = 0;
    region->rb_color = VMA64_RB_BLACK;
    return 1u;
}

u32 vma64_insert(u32 pid, vma_region_t *region)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    vma_region_t *cursor;
    vma_region_t *previous = 0;
    u64 bytes;

    if ((tree == 0)
        || (region == 0)
        || (vma64_range_valid(region->virt_base, region->virt_end) == 0u)
        || (region->prev != 0)
        || (region->next != 0))
    {
        return 0u;
    }

    cursor = tree->head;
    while (cursor != 0)
    {
        if (vma64_ranges_overlap(
                region->virt_base,
                region->virt_end,
                cursor->virt_base,
                cursor->virt_end) != 0u)
        {
            return 0u;
        }
        if (region->virt_base < cursor->virt_base)
        {
            break;
        }
        previous = cursor;
        cursor = cursor->next;
    }

    bytes = region->virt_end - region->virt_base;
    if (persona64_budget_check_vma_pages(
            pid,
            (u32)((tree->mapped_bytes + ((u64)VMA64_PAGE_BYTES - 1ull)) / VMA64_PAGE_BYTES),
            (u32)((bytes + ((u64)VMA64_PAGE_BYTES - 1ull)) / VMA64_PAGE_BYTES)) == 0u)
    {
        return 0u;
    }

    region->prev = previous;
    region->next = cursor;
    if (previous != 0)
    {
        previous->next = region;
    }
    else
    {
        tree->head = region;
    }
    if (cursor != 0)
    {
        cursor->prev = region;
    }

    if (vma64_rb_insert(tree, region) == 0u)
    {
        if (region->prev != 0)
        {
            region->prev->next = region->next;
        }
        else
        {
            tree->head = region->next;
        }
        if (region->next != 0)
        {
            region->next->prev = region->prev;
        }
        region->prev = 0;
        region->next = 0;
        region->rb_parent = 0;
        region->rb_left = 0;
        region->rb_right = 0;
        region->rb_color = VMA64_RB_BLACK;
        vma64_rb_rebuild(tree);
        return 0u;
    }

    ++tree->region_count;
    tree->mapped_bytes += bytes;
    if (tree->region_count > tree->peak_region_count)
    {
        tree->peak_region_count = tree->region_count;
    }
    if (tree->mapped_bytes > tree->peak_mapped_bytes)
    {
        tree->peak_mapped_bytes = tree->mapped_bytes;
    }

    return 1u;
}

vma_region_t *vma64_remove(u32 pid, u64 virt_base, u64 virt_end)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    vma_region_t *cursor;

    if ((tree == 0) || (vma64_range_valid(virt_base, virt_end) == 0u))
    {
        return 0;
    }

    cursor = tree->head;
    while (cursor != 0)
    {
        if ((cursor->virt_base == virt_base) && (cursor->virt_end == virt_end))
        {
            return vma64_detach_region(tree, cursor);
        }

        cursor = cursor->next;
    }

    return 0;
}

vma_region_t *vma64_find(u32 pid, u64 address)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    vma_region_t *cursor;
    u32 steps = 0u;

    if (tree == 0)
    {
        g_vma64_last_lookup_steps = 0u;
        return 0;
    }

    cursor = tree->rb_root;
    while ((cursor != 0) && (steps <= VMA64_MAX_STATIC_REGIONS))
    {
        ++steps;
        if ((address >= cursor->virt_base) && (address < cursor->virt_end))
        {
            g_vma64_last_lookup_steps = steps;
            if (steps > g_vma64_peak_lookup_steps)
            {
                g_vma64_peak_lookup_steps = steps;
            }
            return cursor;
        }
        if (address < cursor->virt_base)
        {
            cursor = cursor->rb_left;
        }
        else
        {
            cursor = cursor->rb_right;
        }
    }

    g_vma64_last_lookup_steps = steps;
    if (steps > g_vma64_peak_lookup_steps)
    {
        g_vma64_peak_lookup_steps = steps;
    }
    return 0;
}

u64 vma64_find_gap(u32 pid, u64 min_addr, u64 max_addr, u64 length, u64 alignment)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    vma_region_t *cursor;
    u64 candidate;

    if ((tree == 0) || (length == 0ull) || (max_addr <= min_addr))
    {
        return 0ull;
    }

    candidate = vma64_align_up(min_addr, alignment);
    if (candidate == 0ull)
    {
        return 0ull;
    }

    cursor = tree->head;
    while (cursor != 0)
    {
        if ((candidate < cursor->virt_end) && ((candidate + length) > cursor->virt_base))
        {
            candidate = vma64_align_up(cursor->virt_end, alignment);
            if (candidate == 0ull)
            {
                return 0ull;
            }
        }
        if ((candidate + length) <= cursor->virt_base)
        {
            break;
        }
        cursor = cursor->next;
    }

    if ((candidate < min_addr)
        || ((candidate + length) < candidate)
        || ((candidate + length) > max_addr))
    {
        return 0ull;
    }

    return candidate;
}

static u32 vma64_user_pages_clear(u64 virtual_address, u32 page_count)
{
    u32 page_index;

    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        if (paging64_user_page_present(
                virtual_address + ((u64)page_index * VMA64_PAGE_BYTES)) != 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

static u64 vma64_region_physical_for_address(const vma_region_t *region, u64 address)
{
    if ((region == 0)
        || (region->phys_base == 0ull)
        || (region->phys_base == VMA64_PHYS_ANON)
        || (address < region->virt_base)
        || (address >= region->virt_end))
    {
        return 0ull;
    }

    return region->phys_base + (address - region->virt_base);
}

static u32 vma64_user_pages_match_region(
    const vma_region_t *region,
    u64 virtual_address,
    u32 page_count)
{
    u32 page_index;
    u64 page_virtual;
    u64 expected_physical;

    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        page_virtual = virtual_address + ((u64)page_index * VMA64_PAGE_BYTES);
        expected_physical = vma64_region_physical_for_address(region, page_virtual);
        if ((expected_physical == 0ull)
            || (paging64_user_page_present(page_virtual) == 0u)
            || (paging64_user_page_physical(page_virtual) != expected_physical))
        {
            return 0u;
        }
    }

    return 1u;
}

u64 vma64_map_anon(u32 pid, u64 hint_addr, u64 length, u32 prot_flags, u32 map_flags)
{
    vma_region_t *region;
    u64 virtual_address;
    u64 physical_address;
    u32 page_count = vma64_page_count_from_length(length);
    u32 page_index;
    u32 paging_prot = vma64_paging_prot(prot_flags);

    g_vma64_last_map_stage = 1u;
    if ((page_count == 0u)
        || (page_count > VMA64_MAX_ANON_PAGES)
        || ((map_flags & VMA64_MAP_ANONYMOUS) == 0u)
        || (paging_prot == 0u)
        || (vma64_init_process(pid) == 0u))
    {
        return 0ull;
    }

    g_vma64_last_map_stage = 2u;
    if ((map_flags & VMA64_MAP_FIXED) != 0u)
    {
        virtual_address = hint_addr;
        if (((virtual_address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
            || (vma64_find_gap(
                    pid,
                    virtual_address,
                    virtual_address + length,
                    length,
                    VMA64_PAGE_BYTES) != virtual_address)
            || (vma64_user_pages_clear(virtual_address, page_count) == 0u))
        {
            return 0ull;
        }
    }
    else
    {
        g_vma64_last_map_stage = 3u;
        virtual_address = (hint_addr != 0ull) ? hint_addr : VMA64_ANON_HINT_BASE;
        virtual_address = vma64_find_gap(
            pid,
            virtual_address,
            VMA64_ANON_HINT_LIMIT,
            length,
            VMA64_PAGE_BYTES);
        if ((virtual_address == 0ull)
            || (vma64_user_pages_clear(virtual_address, page_count) == 0u))
        {
            return 0ull;
        }
    }

    g_vma64_last_map_stage = 4u;
    region = vma64_region_acquire();
    if (region == 0)
    {
        return 0ull;
    }

    g_vma64_last_map_stage = 5u;
    physical_address = vma64_claim_anon_pages(page_count);
    if (physical_address == 0ull)
    {
        vma64_region_release(region);
        return 0ull;
    }

    g_vma64_last_map_stage = 6u;
    if ((vma64_region_prepare(
            region,
            virtual_address,
            virtual_address + length,
            physical_address,
            prot_flags,
            map_flags,
            VMA64_BACKING_ANON,
            VMA64_BACKING_HANDLE_NONE,
            0xA3000001u) == 0u)
        || (vma64_insert(pid, region) == 0u))
    {
        vma64_release_anon_pages(physical_address, page_count);
        vma64_region_release(region);
        return 0ull;
    }

    g_vma64_last_map_stage = 7u;
    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        if (paging64_install_user_page_mapping(
                virtual_address + ((u64)page_index * VMA64_PAGE_BYTES),
                physical_address + ((u64)page_index * VMA64_PAGE_BYTES),
                paging_prot) == 0u)
        {
            while (page_index > 0u)
            {
                --page_index;
                (void)paging64_clear_user_page_mapping(
                    virtual_address + ((u64)page_index * VMA64_PAGE_BYTES));
            }
            (void)vma64_remove(pid, virtual_address, virtual_address + length);
            vma64_release_anon_pages(physical_address, page_count);
            vma64_region_release(region);
            return 0ull;
        }
    }

    g_vma64_last_map_stage = 8u;
    return virtual_address;
}

u32 vma64_clone_cow_page(u32 source_pid, u64 source_address, u32 target_pid, u64 target_address)
{
    vma_region_t *source_region;
    vma_region_t *target_region;
    u64 source_page = vma64_align_down(source_address, VMA64_PAGE_BYTES);
    u64 target_page = vma64_align_down(target_address, VMA64_PAGE_BYTES);
    u64 physical_address;
    u32 source_readonly_prot;
    u32 target_readonly_prot;

    if ((source_page != source_address)
        || (target_page != target_address)
        || (source_pid == PROCESS64_INVALID_PID)
        || (target_pid == PROCESS64_INVALID_PID)
        || (vma64_init_process(source_pid) == 0u)
        || (vma64_init_process(target_pid) == 0u)
        || (vma64_find(target_pid, target_page) != 0)
        || (paging64_user_page_present(target_page) != 0u))
    {
        return 0u;
    }

    source_region = vma64_find(source_pid, source_page);
    if ((source_region == 0)
        || (source_region->backing_type != VMA64_BACKING_ANON)
        || (source_region->virt_base != source_page)
        || (source_region->virt_end != (source_page + VMA64_PAGE_BYTES))
        || ((source_region->prot_flags & VMA64_PROT_WRITE) == 0u)
        || (vma64_user_pages_match_region(source_region, source_page, 1u) == 0u))
    {
        return 0u;
    }

    physical_address = paging64_user_page_physical(source_page);
    if ((physical_address == 0ull) || (vma64_retain_anon_page(physical_address) == 0u))
    {
        return 0u;
    }

    target_region = vma64_region_acquire();
    if (target_region == 0)
    {
        vma64_release_anon_pages(physical_address, 1u);
        return 0u;
    }

    if ((vma64_region_prepare(
            target_region,
            target_page,
            target_page + VMA64_PAGE_BYTES,
            physical_address,
            source_region->prot_flags,
            source_region->map_flags | VMA64_MAP_COPY_ON_WRITE,
            VMA64_BACKING_ANON,
            VMA64_BACKING_HANDLE_NONE,
            vma64_split_token(source_region->vma_token, 0xA7001000u)) == 0u)
        || (vma64_insert(target_pid, target_region) == 0u))
    {
        vma64_release_anon_pages(physical_address, 1u);
        vma64_region_release(target_region);
        return 0u;
    }

    source_region->map_flags |= VMA64_MAP_COPY_ON_WRITE;
    source_readonly_prot = vma64_paging_prot(source_region->prot_flags & ~VMA64_PROT_WRITE);
    target_readonly_prot = vma64_paging_prot(target_region->prot_flags & ~VMA64_PROT_WRITE);
    if ((source_readonly_prot == 0u)
        || (target_readonly_prot == 0u)
        || (paging64_update_user_page_protection(source_page, source_readonly_prot) == 0u)
        || (paging64_install_user_page_mapping(
                target_page,
                physical_address,
                target_readonly_prot) == 0u))
    {
        (void)vma64_remove(target_pid, target_page, target_page + VMA64_PAGE_BYTES);
        vma64_region_release(target_region);
        vma64_release_anon_pages(physical_address, 1u);
        return 0u;
    }

    return 1u;
}

u32 vma64_handle_cow_fault(u32 pid, u64 fault_address, u64 fault_error_code)
{
    vma_region_t *region;
    u64 page_address = vma64_align_down(fault_address, VMA64_PAGE_BYTES);
    u64 old_physical;
    u64 new_physical;
    void *old_page;
    void *new_page;
    u32 writable_prot;

    if (((fault_error_code & (VMA64_FAULT_PRESENT | VMA64_FAULT_WRITE))
            != (VMA64_FAULT_PRESENT | VMA64_FAULT_WRITE))
        || ((fault_address - page_address) >= VMA64_PAGE_BYTES))
    {
        return 0u;
    }

    region = vma64_find(pid, page_address);
    if ((region == 0)
        || ((region->map_flags & VMA64_MAP_COPY_ON_WRITE) == 0u)
        || ((region->prot_flags & VMA64_PROT_WRITE) == 0u)
        || (region->backing_type != VMA64_BACKING_ANON)
        || (region->virt_base != page_address)
        || (region->virt_end != (page_address + VMA64_PAGE_BYTES))
        || (paging64_user_page_present(page_address) == 0u)
        || ((paging64_user_page_protection(page_address) & PAGING64_USER_PROT_WRITE) != 0u))
    {
        return 0u;
    }

    old_physical = paging64_user_page_physical(page_address);
    old_page = vma64_anon_page_ptr(old_physical);
    new_physical = vma64_claim_anon_pages(1u);
    new_page = vma64_anon_page_ptr(new_physical);
    writable_prot = vma64_paging_prot(region->prot_flags);
    if ((old_page == 0)
        || (new_page == 0)
        || (new_physical == old_physical)
        || (writable_prot == 0u))
    {
        if (new_physical != 0ull)
        {
            vma64_release_anon_pages(new_physical, 1u);
        }
        return 0u;
    }

    vma64_copy_page(new_page, old_page);
    if (paging64_remap_user_page(page_address, new_physical, writable_prot) == 0u)
    {
        vma64_release_anon_pages(new_physical, 1u);
        return 0u;
    }

    region->phys_base = new_physical;
    region->map_flags &= ~VMA64_MAP_COPY_ON_WRITE;
    vma64_release_anon_pages(old_physical, 1u);
    ++g_vma64_cow_fault_count;
    return 1u;
}

static void vma64_brk_init_tree(vma_tree_t *tree)
{
    if ((tree != 0) && (tree->brk_base == 0ull))
    {
        tree->brk_base = VMA64_BRK_BASE_DEFAULT;
        tree->brk_current = VMA64_BRK_BASE_DEFAULT;
        tree->brk_peak = VMA64_BRK_BASE_DEFAULT;
    }
}

u64 vma64_brk_query(u32 pid)
{
    vma_tree_t *tree;

    if (vma64_init_process(pid) == 0u)
    {
        return 0ull;
    }

    tree = vma64_tree_for_process(pid);
    vma64_brk_init_tree(tree);
    return (tree != 0) ? tree->brk_current : 0ull;
}

u64 vma64_brk_extend(u32 pid, u64 new_brk)
{
    vma_tree_t *tree;
    u64 current;
    u64 length;

    if ((vma64_init_process(pid) == 0u)
        || ((new_brk & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull))
    {
        return 0ull;
    }

    tree = vma64_tree_for_process(pid);
    vma64_brk_init_tree(tree);
    if ((tree == 0)
        || (new_brk < tree->brk_base)
        || (new_brk > VMA64_BRK_LIMIT_DEFAULT))
    {
        return 0ull;
    }

    current = tree->brk_current;
    if (new_brk == current)
    {
        return current;
    }

    if (new_brk > current)
    {
        length = new_brk - current;
        if (vma64_map_anon(
                pid,
                current,
                length,
                VMA64_PROT_READ | VMA64_PROT_WRITE,
                VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS) != current)
        {
            return 0ull;
        }
    }
    else
    {
        length = current - new_brk;
        if (vma64_unmap(pid, new_brk, length) == 0u)
        {
            return 0ull;
        }
    }

    tree->brk_current = new_brk;
    if (tree->brk_current > tree->brk_peak)
    {
        tree->brk_peak = tree->brk_current;
    }
    return tree->brk_current;
}

u32 vma64_unmap(u32 pid, u64 address, u64 length)
{
    vma_tree_t *tree;
    vma_region_t *region;
    vma_region_t *removed;
    u64 physical_address;
    u64 end_address;
    u32 page_count = vma64_page_count_from_length(length);
    u32 page_index;

    g_vma64_last_unmap_stage = 1u;
    if ((page_count == 0u)
        || ((address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((address + length) < address))
    {
        return 0u;
    }

    g_vma64_last_unmap_stage = 2u;
    end_address = address + length;
    region = vma64_find(pid, address);
    if (region == 0)
    {
        return 0u;
    }
    g_vma64_last_unmap_stage = 3u;
    if ((region->virt_base != address) || (region->virt_end != end_address))
    {
        return 0u;
    }
    g_vma64_last_unmap_stage = 4u;
    if (region->backing_type != VMA64_BACKING_ANON)
    {
        return 0u;
    }
    g_vma64_last_unmap_stage = 5u;
    if (region->phys_base == 0ull)
    {
        return 0u;
    }
    g_vma64_last_unmap_stage = 6u;
    (void)vma64_user_pages_match_region(region, address, page_count);

    physical_address = region->phys_base;
    g_vma64_last_unmap_stage = 7u;
    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        (void)paging64_clear_user_page_mapping(
            address + ((u64)page_index * VMA64_PAGE_BYTES));
    }

    tree = vma64_tree_for_process(pid);
    removed = vma64_remove(pid, address, end_address);
    if (removed == 0)
    {
        removed = vma64_detach_region(tree, region);
    }
    if (removed == 0)
    {
        g_vma64_last_unmap_stage = 8u;
        return 0u;
    }

    vma64_release_anon_pages(physical_address, page_count);
    vma64_region_release(removed);
    g_vma64_last_unmap_stage = 9u;
    return 1u;
}

u32 vma64_release_process(u32 pid)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    vma_region_t *cursor;
    u32 released = 0u;
    u32 index;

    if (tree == 0)
    {
        return 0u;
    }

    cursor = tree->head;
    while (cursor != 0)
    {
        vma_region_t *next = cursor->next;
        u64 length = cursor->virt_end - cursor->virt_base;
        u32 page_count = vma64_page_count_from_length(length);
        u32 page_index;

        for (page_index = 0u; page_index < page_count; ++page_index)
        {
            (void)paging64_clear_user_page_mapping(
                cursor->virt_base + ((u64)page_index * VMA64_PAGE_BYTES));
        }

        if ((cursor->backing_type == VMA64_BACKING_ANON)
            && (cursor->phys_base != 0ull)
            && (cursor->phys_base != VMA64_PHYS_ANON)
            && (page_count != 0u))
        {
            vma64_release_anon_pages(cursor->phys_base, page_count);
        }

        vma64_region_release(cursor);
        ++released;
        cursor = next;
    }

    (void)process64_detach_vma(pid);
    for (index = 0u; index < VMA64_MAX_PROCESS_TREES; ++index)
    {
        if (&g_vma64_trees[index] == tree)
        {
            g_vma64_tree_pids[index] = PROCESS64_INVALID_PID;
            break;
        }
    }
    vma64_clear_tree(tree);
    return released;
}

u32 vma64_protect(u32 pid, u64 address, u64 length, u32 new_prot_flags)
{
    vma_region_t *region;
    vma_region_t *removed;
    vma_region_t *prefix = 0;
    vma_region_t *suffix = 0;
    u64 old_base;
    u64 old_end;
    u64 old_phys;
    u32 old_prot;
    u32 old_map;
    u32 old_backing_type;
    u32 old_backing_handle;
    u32 old_token;
    u64 end_address;
    u32 page_count = vma64_page_count_from_length(length);
    u32 page_index;
    u32 paging_prot = vma64_paging_prot(new_prot_flags);
    u32 need_prefix;
    u32 need_suffix;

    if ((page_count == 0u)
        || ((address & ((u64)VMA64_PAGE_BYTES - 1ull)) != 0ull)
        || ((address + length) < address)
        || (paging_prot == 0u))
    {
        return 0u;
    }

    end_address = address + length;
    region = vma64_find(pid, address);
    if ((region == 0)
        || (end_address > region->virt_end)
        || (region->backing_type != VMA64_BACKING_ANON)
        || (vma64_user_pages_match_region(region, address, page_count) == 0u))
    {
        return 0u;
    }

    old_base = region->virt_base;
    old_end = region->virt_end;
    old_phys = region->phys_base;
    old_prot = region->prot_flags;
    old_map = region->map_flags;
    old_backing_type = region->backing_type;
    old_backing_handle = region->backing_handle;
    old_token = region->vma_token;
    need_prefix = (address > old_base) ? 1u : 0u;
    need_suffix = (end_address < old_end) ? 1u : 0u;

    if (need_prefix != 0u)
    {
        prefix = vma64_region_acquire();
        if (prefix == 0)
        {
            return 0u;
        }
    }
    if (need_suffix != 0u)
    {
        suffix = vma64_region_acquire();
        if (suffix == 0)
        {
            if (prefix != 0)
            {
                vma64_region_release(prefix);
            }
            return 0u;
        }
    }

    removed = vma64_remove(pid, old_base, old_end);
    if (removed != region)
    {
        if (prefix != 0)
        {
            vma64_region_release(prefix);
        }
        if (suffix != 0)
        {
            vma64_region_release(suffix);
        }
        return 0u;
    }

    if ((need_prefix != 0u)
        && ((vma64_region_prepare(
                prefix,
                old_base,
                address,
                old_phys,
                old_prot,
                old_map,
                old_backing_type,
                old_backing_handle,
                vma64_split_token(old_token, 0xA5000100u)) == 0u)
            || (vma64_insert(pid, prefix) == 0u)))
    {
        vma64_region_release(prefix);
        return 0u;
    }

    if ((vma64_region_prepare(
            region,
            address,
            end_address,
            vma64_region_physical_for_address(removed, address),
            new_prot_flags,
            old_map,
            old_backing_type,
            old_backing_handle,
            old_token) == 0u)
        || (vma64_insert(pid, region) == 0u))
    {
        return 0u;
    }

    if ((need_suffix != 0u)
        && ((vma64_region_prepare(
                suffix,
                end_address,
                old_end,
                old_phys + (end_address - old_base),
                old_prot,
                old_map,
                old_backing_type,
                old_backing_handle,
                vma64_split_token(old_token, 0xA5000200u)) == 0u)
            || (vma64_insert(pid, suffix) == 0u)))
    {
        vma64_region_release(suffix);
        return 0u;
    }

    for (page_index = 0u; page_index < page_count; ++page_index)
    {
        if (paging64_update_user_page_protection(
                address + ((u64)page_index * VMA64_PAGE_BYTES),
                paging_prot) == 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

u32 vma64_region_count(u32 pid)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);

    return (tree != 0) ? tree->region_count : 0u;
}

u64 vma64_mapped_bytes(u32 pid)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);

    return (tree != 0) ? tree->mapped_bytes : 0ull;
}

u32 vma64_anon_claimed_pages(void)
{
    return g_vma64_anon_claimed_pages;
}

u32 vma64_anon_total_pages(void)
{
    return VMA64_MAX_ANON_PAGES;
}

u32 vma64_anon_free_pages(void)
{
    return (g_vma64_anon_claimed_pages < VMA64_MAX_ANON_PAGES)
        ? (VMA64_MAX_ANON_PAGES - g_vma64_anon_claimed_pages)
        : 0u;
}

u32 vma64_cow_region_count(u32 pid)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    vma_region_t *cursor;
    u32 count = 0u;

    if (tree == 0)
    {
        return 0u;
    }

    cursor = tree->head;
    while (cursor != 0)
    {
        if ((cursor->map_flags & VMA64_MAP_COPY_ON_WRITE) != 0u)
        {
            ++count;
        }
        cursor = cursor->next;
    }

    return count;
}

u32 vma64_cow_page_count(u32 pid)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    vma_region_t *cursor;
    u32 count = 0u;

    if (tree == 0)
    {
        return 0u;
    }

    cursor = tree->head;
    while (cursor != 0)
    {
        if ((cursor->map_flags & VMA64_MAP_COPY_ON_WRITE) != 0u)
        {
            count += vma64_page_count_from_length(cursor->virt_end - cursor->virt_base);
        }
        cursor = cursor->next;
    }

    return count;
}

u32 vma64_cow_fault_count(void)
{
    return g_vma64_cow_fault_count;
}

u32 vma64_peak_region_count(u32 pid)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);

    return (tree != 0) ? tree->peak_region_count : 0u;
}

u32 vma64_physical_page_ref_count(u64 physical_address)
{
    u32 index = vma64_anon_page_index(physical_address);

    return (index < VMA64_MAX_ANON_PAGES) ? g_vma64_anon_page_used[index] : 0u;
}

u32 vma64_physical_page_checksum(u64 physical_address)
{
    return vma64_page_checksum(vma64_anon_page_ptr(physical_address));
}

u64 vma64_diag_query(u32 pid, u32 selector)
{
    vma_tree_t *tree = vma64_tree_for_process(pid);
    volatile u32 selected = selector;

    if (tree == 0)
    {
        return VMA64_DIAG_DENIED;
    }

    if (selected == VMA64_DIAG_REGION_COUNT)
    {
        return (u64)tree->region_count;
    }
    if (selected == VMA64_DIAG_MAPPED_BYTES)
    {
        return tree->mapped_bytes;
    }
    if (selected == VMA64_DIAG_COW_PAGE_COUNT)
    {
        return (u64)vma64_cow_page_count(pid);
    }
    if (selected == VMA64_DIAG_BRK_CURRENT)
    {
        return tree->brk_current;
    }
    if (selected == VMA64_DIAG_PEAK_REGION_COUNT)
    {
        return (u64)tree->peak_region_count;
    }

    return VMA64_DIAG_DENIED;
}

void vma64_reset_lookup_telemetry(void)
{
    g_vma64_last_lookup_steps = 0u;
    g_vma64_peak_lookup_steps = 0u;
}

u32 vma64_last_lookup_steps(void)
{
    return g_vma64_last_lookup_steps;
}

u32 vma64_peak_lookup_steps(void)
{
    return g_vma64_peak_lookup_steps;
}

u32 vma64_last_map_stage(void)
{
    return g_vma64_last_map_stage;
}

u32 vma64_last_unmap_stage(void)
{
    return g_vma64_last_unmap_stage;
}
