#ifndef LIMITLESS_PAGING_X64_H
#define LIMITLESS_PAGING_X64_H

#include "types.h"

#define PAGING64_RUNTIME_PROTECTION_READ 0x00000001u
#define PAGING64_RUNTIME_PROTECTION_EXECUTE 0x00000002u
#define PAGING64_RUNTIME_PROTECTION_VIEW_SEALED 0x00000004u
#define PAGING64_RUNTIME_PROTECTION_SUPERVISOR_ONLY 0x00000008u
#define PAGING64_RUNTIME_PROTECTION_VALIDATION_ONLY 0x00000010u
#define PAGING64_RUNTIME_PROTECTION_USER_ACCESSIBLE 0x00000020u
#define PAGING64_RUNTIME_PROTECTION_WRITABLE 0x00000040u

#define PAGING64_USER_PROT_READ 0x00000001u
#define PAGING64_USER_PROT_WRITE 0x00000002u
#define PAGING64_USER_PROT_EXECUTE 0x00000004u

void paging64_configure_kernel_physical_base(u32 kernel_load_address);
u64 paging64_kernel_physical_alias(const void *address);
u32 paging64_install_runtime_mapping(u32 virtual_base, const void *source, u32 mapped_bytes);
u32 paging64_install_user_runtime_mapping(u32 virtual_base, const void *source, u32 mapped_bytes);
u32 paging64_install_user_stack_mapping(u32 stack_top, u32 stack_bytes);
u32 paging64_install_user_page_mapping(u64 virtual_address, u64 physical_address, u32 protection_flags);
u32 paging64_clear_user_page_mapping(u64 virtual_address);
u32 paging64_remap_user_page(u64 virtual_address, u64 physical_address, u32 protection_flags);
u32 paging64_update_user_page_protection(u64 virtual_address, u32 protection_flags);
u32 paging64_user_page_present(u64 virtual_address);
u64 paging64_user_page_physical(u64 virtual_address);
u32 paging64_user_page_protection(u64 virtual_address);
u32 paging64_install_user_page_mapping_for_process(
    u32 pid,
    u64 virtual_address,
    u64 physical_address,
    u32 protection_flags);
u32 paging64_clear_user_page_mapping_for_process(u32 pid, u64 virtual_address);
u32 paging64_remap_user_page_for_process(
    u32 pid,
    u64 virtual_address,
    u64 physical_address,
    u32 protection_flags);
u32 paging64_update_user_page_protection_for_process(
    u32 pid,
    u64 virtual_address,
    u32 protection_flags);
u32 paging64_user_page_present_for_process(u32 pid, u64 virtual_address);
u64 paging64_user_page_physical_for_process(u32 pid, u64 virtual_address);
u32 paging64_user_page_protection_for_process(u32 pid, u64 virtual_address);
u32 paging64_process_root_alloc(u32 pid, u32 owner_id, u32 authority_token);
u32 paging64_process_root_fork_alloc(u32 parent_pid, u32 child_pid, u32 owner_id, u32 authority_token);
u32 paging64_process_root_release(u32 pid, u32 authority_token);
u64 paging64_process_root_physical(u32 pid);
u32 paging64_process_root_slot(u32 pid);
u32 paging64_process_root_token(u32 pid);
u32 paging64_switch_to_process_root(u32 pid, u32 reason);
u32 paging64_switch_to_kernel_root(u32 reason);
u32 paging64_reload_current_root(void);
u64 paging64_current_root_physical(void);
u64 paging64_kernel_root_physical(void);
u32 paging64_process_root_pool_limit(void);
u32 paging64_process_root_pool_used(void);
u32 paging64_process_root_alloc_count(void);
u32 paging64_process_root_release_count(void);
u32 paging64_process_root_alloc_denial_count(void);
u32 paging64_process_root_switch_count(void);
u32 paging64_process_root_switch_denial_count(void);
u32 paging64_process_root_kernel_switch_count(void);
u32 paging64_process_root_last_switch_reason(void);
u32 paging64_process_root_low_compat_count(void);
u32 paging64_process_root_last_low_compat(void);
u32 paging64_process_root_last_low_pdpt_present(void);
u32 paging64_process_root_high_copy_count(void);
u32 paging64_process_root_last_high_copy(void);
u32 paging64_process_root_mmio_shared_count(void);
u32 paging64_process_root_last_mmio_shared(void);
u32 paging64_process_root_last_pool_mapped(void);
u32 paging64_process_root_last_user_pdpt_private(void);
u32 paging64_process_root_last_vma_pt_private(void);
u32 paging64_process_root_last_slot(void);
u32 paging64_process_root_last_pid(void);
u64 paging64_process_root_last_physical(void);
u32 paging64_process_root_fork_count(void);
u32 paging64_process_root_fork_denial_count(void);
u32 paging64_process_root_fork_last_parent_pid(void);
u32 paging64_process_root_fork_last_child_pid(void);
u32 paging64_process_root_fork_last_parent_slot(void);
u32 paging64_process_root_fork_last_child_slot(void);
u32 paging64_process_root_fork_last_child_root_distinct(void);
u32 paging64_install_kernel_mmio_mapping(u64 virtual_base, u64 physical_base, u32 page_count);
u32 paging64_install_apic_mmio_mapping(u64 lapic_virtual, u32 lapic_physical, u64 ioapic_virtual, u32 ioapic_physical);
u32 paging64_runtime_mapping_installed(void);
u32 paging64_runtime_mapping_page_count(void);
u32 paging64_runtime_mapping_source_checksum(void);
u32 paging64_runtime_mapping_install_token(void);
u32 paging64_runtime_mapping_entry_probe(void);
u32 paging64_runtime_mapping_protection_flags(void);
u32 paging64_runtime_mapping_protection_token(void);
u64 paging64_runtime_mapping_source_physical(void);
u32 paging64_user_runtime_mapping_installed(void);
u32 paging64_user_runtime_mapping_page_count(void);
u32 paging64_user_runtime_mapping_source_checksum(void);
u32 paging64_user_runtime_mapping_install_token(void);
u32 paging64_user_runtime_mapping_entry_probe(void);
u32 paging64_user_runtime_mapping_protection_flags(void);
u32 paging64_user_runtime_mapping_protection_token(void);
u64 paging64_user_runtime_mapping_source_physical(void);
u32 paging64_user_stack_mapping_installed(void);
u32 paging64_user_stack_mapping_protection_flags(void);
u32 paging64_user_stack_mapping_protection_token(void);
u32 paging64_kernel_mmio_mapping_installed(void);
u32 paging64_kernel_mmio_mapping_install_token(void);
u32 paging64_kernel_mmio_mapping_pml4_index(void);
u32 paging64_kernel_mmio_mapping_pdpt_index(void);
u32 paging64_kernel_mmio_mapping_pd_index(void);
u32 paging64_kernel_mmio_mapping_pt_index(void);
u64 paging64_kernel_mmio_mapping_entry_flags(void);
u32 paging64_kernel_mmio_mapping_nx_enabled(void);
u32 paging64_kernel_mmio_write_window_open(u32 page_index);
u32 paging64_kernel_mmio_write_window_close(u32 page_index);
u32 paging64_kernel_mmio_write_window_open_virtual(u64 virtual_address);
u32 paging64_kernel_mmio_write_window_close_virtual(u64 virtual_address);

#endif
