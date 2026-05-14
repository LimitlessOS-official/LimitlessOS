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

void paging64_configure_kernel_physical_base(u32 kernel_load_address);
u64 paging64_kernel_physical_alias(const void *address);
u32 paging64_install_runtime_mapping(u32 virtual_base, const void *source, u32 mapped_bytes);
u32 paging64_install_user_runtime_mapping(u32 virtual_base, const void *source, u32 mapped_bytes);
u32 paging64_install_user_stack_mapping(u32 stack_top, u32 stack_bytes);
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
