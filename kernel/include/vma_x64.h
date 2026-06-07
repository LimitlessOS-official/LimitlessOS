#ifndef LIMITLESS_VMA_X64_H
#define LIMITLESS_VMA_X64_H

#include "types.h"

#define VMA64_PAGE_BYTES 0x00001000u
#define VMA64_INVALID_TOKEN 0x00000000u
#define VMA64_BACKING_HANDLE_NONE 0xFFFFFFFFu
#define VMA64_PHYS_ANON 0xFFFFFFFFFFFFFFFFull

#define VMA64_PROT_READ 0x00000001u
#define VMA64_PROT_WRITE 0x00000002u
#define VMA64_PROT_EXECUTE 0x00000004u

#define VMA64_MAP_PRIVATE 0x00000001u
#define VMA64_MAP_SHARED 0x00000002u
#define VMA64_MAP_FIXED 0x00000004u
#define VMA64_MAP_ANONYMOUS 0x00000008u
#define VMA64_MAP_COPY_ON_WRITE 0x00000010u

#define VMA64_FAULT_PRESENT 0x00000001u
#define VMA64_FAULT_WRITE 0x00000002u
#define VMA64_FAULT_USER 0x00000004u

#define VMA64_DIAG_REGION_COUNT 0x00000001u
#define VMA64_DIAG_MAPPED_BYTES 0x00000002u
#define VMA64_DIAG_COW_PAGE_COUNT 0x00000003u
#define VMA64_DIAG_BRK_CURRENT 0x00000004u
#define VMA64_DIAG_PEAK_REGION_COUNT 0x00000005u
#define VMA64_DIAG_DENIED 0xFFFFFFFFFFFFFFFFull

#define VMA64_BACKING_ANON 0x00000001u
#define VMA64_BACKING_RAMFS_NODE 0x00000002u
#define VMA64_BACKING_NVME_EXTENT 0x00000003u
#define VMA64_BACKING_DEVICE 0x00000004u

#define VMA64_RB_RED 0x00000000u
#define VMA64_RB_BLACK 0x00000001u

typedef struct vma_region
{
    u64 virt_base;
    u64 virt_end;
    u64 phys_base;
    u32 prot_flags;
    u32 map_flags;
    u32 backing_type;
    u32 backing_handle;
    u32 vma_token;
    u32 reserved;
    struct vma_region *prev;
    struct vma_region *next;
    struct vma_region *rb_parent;
    struct vma_region *rb_left;
    struct vma_region *rb_right;
    u32 rb_color;
} vma_region_t;

typedef struct vma_tree
{
    vma_region_t *head;
    vma_region_t *rb_root;
    u32 region_count;
    u32 peak_region_count;
    u64 mapped_bytes;
    u64 peak_mapped_bytes;
    u64 brk_base;
    u64 brk_current;
    u64 brk_peak;
} vma_tree_t;

void vma64_init(void);
u32 vma64_init_process(u32 pid);
vma_tree_t *vma64_tree_for_process(u32 pid);
const vma_region_t *vma64_first_region(u32 pid);
const vma_region_t *vma64_next_region(const vma_region_t *region);
vma_region_t *vma64_region_acquire(void);
void vma64_region_release(vma_region_t *region);
u32 vma64_region_prepare(
    vma_region_t *region,
    u64 virt_base,
    u64 virt_end,
    u64 phys_base,
    u32 prot_flags,
    u32 map_flags,
    u32 backing_type,
    u32 backing_handle,
    u32 vma_token);
u32 vma64_insert(u32 pid, vma_region_t *region);
vma_region_t *vma64_remove(u32 pid, u64 virt_base, u64 virt_end);
vma_region_t *vma64_find(u32 pid, u64 address);
u64 vma64_find_gap(u32 pid, u64 min_addr, u64 max_addr, u64 length, u64 alignment);
u64 vma64_map_anon(u32 pid, u64 hint_addr, u64 length, u32 prot_flags, u32 map_flags);
u32 vma64_clone_cow_page(u32 source_pid, u64 source_address, u32 target_pid, u64 target_address);
u32 vma64_fork_copy_process(u32 parent_pid, u32 child_pid);
u32 vma64_handle_cow_fault(u32 pid, u64 fault_address, u64 fault_error_code);
u64 vma64_brk_query(u32 pid);
u64 vma64_brk_extend(u32 pid, u64 new_brk);
u32 vma64_unmap(u32 pid, u64 address, u64 length);
u32 vma64_release_process(u32 pid);
u32 vma64_protect(u32 pid, u64 address, u64 length, u32 new_prot_flags);
u32 vma64_region_count(u32 pid);
u64 vma64_mapped_bytes(u32 pid);
u32 vma64_anon_total_pages(void);
u32 vma64_anon_claimed_pages(void);
u32 vma64_anon_free_pages(void);
u32 vma64_cow_region_count(u32 pid);
u32 vma64_cow_page_count(u32 pid);
u32 vma64_cow_fault_count(void);
u32 vma64_peak_region_count(u32 pid);
u32 vma64_physical_page_ref_count(u64 physical_address);
u32 vma64_physical_page_checksum(u64 physical_address);
u64 vma64_diag_query(u32 pid, u32 selector);
void vma64_reset_lookup_telemetry(void);
u32 vma64_last_lookup_steps(void);
u32 vma64_peak_lookup_steps(void);
u32 vma64_last_map_stage(void);
u32 vma64_last_unmap_stage(void);
u32 vma64_fork_copy_count(void);
u32 vma64_fork_copy_denial_count(void);
u32 vma64_fork_copy_last_parent_pid(void);
u32 vma64_fork_copy_last_child_pid(void);
u32 vma64_fork_copy_last_regions(void);
u32 vma64_fork_copy_last_pages(void);
u32 vma64_fork_copy_last_stage(void);
u32 vma64_fork_copy_last_unsupported_backing(void);

#endif
