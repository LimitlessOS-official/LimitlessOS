#ifndef LIMITLESS_PCI_X64_H
#define LIMITLESS_PCI_X64_H

#include "types.h"

#define PCI64_INVALID_RESULT 0xFFFFFFFFu
#define PCI64_AHCI_MMIO_FLAG_PRESENT 0x00000001u
#define PCI64_AHCI_MMIO_FLAG_MEMORY_BAR 0x00000002u
#define PCI64_AHCI_MMIO_FLAG_32BIT_BAR 0x00000004u
#define PCI64_AHCI_MMIO_FLAG_BASE_NONZERO 0x00000008u
#define PCI64_AHCI_MMIO_FLAG_PAGE_ALIGNED 0x00000010u
#define PCI64_AHCI_MMIO_FLAG_MAPPING_REQUIRED 0x00000020u
#define PCI64_AHCI_MMIO_FLAG_SAFE_NO_TOUCH 0x00000040u

#define PCI64_NVME_MMIO_FLAG_PRESENT 0x00000001u
#define PCI64_NVME_MMIO_FLAG_MEMORY_BAR 0x00000002u
#define PCI64_NVME_MMIO_FLAG_64BIT_BAR 0x00000004u
#define PCI64_NVME_MMIO_FLAG_BASE_NONZERO 0x00000008u
#define PCI64_NVME_MMIO_FLAG_PAGE_ALIGNED 0x00000010u
#define PCI64_NVME_MMIO_FLAG_MAPPING_REQUIRED 0x00000020u
#define PCI64_NVME_MMIO_FLAG_BELOW_4G 0x00000040u
#define PCI64_NVME_MMIO_FLAG_ADMIN_ONLY 0x00000080u
#define PCI64_NVME_MMIO_FLAG_SAFE_NO_IO_QUEUE 0x00000100u

#define PCI64_VMD_MMIO_FLAG_PRESENT 0x00000001u
#define PCI64_VMD_MMIO_FLAG_MEMORY_BAR 0x00000002u
#define PCI64_VMD_MMIO_FLAG_64BIT_BAR 0x00000004u
#define PCI64_VMD_MMIO_FLAG_BASE_NONZERO 0x00000008u
#define PCI64_VMD_MMIO_FLAG_PAGE_ALIGNED 0x00000010u
#define PCI64_VMD_MMIO_FLAG_MAPPING_REQUIRED 0x00000020u
#define PCI64_VMD_MMIO_FLAG_BELOW_4G 0x00000040u
#define PCI64_VMD_MMIO_FLAG_SAFE_NO_TOUCH 0x00000080u
#define PCI64_VMD_MMIO_FLAG_NESTED_ENUM_REQUIRED 0x00000100u
#define PCI64_VMD_MMIO_FLAG_NO_DRIVER_BOUND 0x00000200u

#define PCI64_VMD_NESTED_STATUS_NOT_APPLICABLE 0x00000000u
#define PCI64_VMD_NESTED_STATUS_ENUM_UNAVAILABLE 0x00000001u
#define PCI64_VMD_NESTED_STATUS_ENUM_NO_NVME 0x00000002u
#define PCI64_VMD_NESTED_STATUS_NVME_UNBOUND 0x00000003u
#define PCI64_VMD_NESTED_STATUS_MAP_FAILED 0x00000004u
#define PCI64_VMD_NESTED_STATUS_BUS_RESTRICT_UNKNOWN 0x00000005u

#define PCI64_VMD_NESTED_BIND_STATUS_NOT_APPLICABLE 0x00000000u
#define PCI64_VMD_NESTED_BIND_STATUS_ENUM_PENDING 0x00000001u
#define PCI64_VMD_NESTED_BIND_STATUS_NO_CHILD_NVME 0x00000002u
#define PCI64_VMD_NESTED_BIND_STATUS_CHILD_INVALID 0x00000003u
#define PCI64_VMD_NESTED_BIND_STATUS_MMIO_INVALID 0x00000004u
#define PCI64_VMD_NESTED_BIND_STATUS_READY_UNBOUND 0x00000005u

#define PCI64_VMD_NESTED_REGISTER_STATUS_NOT_APPLICABLE 0x00000000u
#define PCI64_VMD_NESTED_REGISTER_STATUS_WAITING_BIND_READY 0x00000001u
#define PCI64_VMD_NESTED_REGISTER_STATUS_DEFERRED_NO_DRIVER 0x00000002u

#define PCI64_LPSS_I2C_MMIO_FLAG_PRESENT 0x00000001u
#define PCI64_LPSS_I2C_MMIO_FLAG_MEMORY_BAR 0x00000002u
#define PCI64_LPSS_I2C_MMIO_FLAG_64BIT_BAR 0x00000004u
#define PCI64_LPSS_I2C_MMIO_FLAG_BASE_NONZERO 0x00000008u
#define PCI64_LPSS_I2C_MMIO_FLAG_PAGE_ALIGNED 0x00000010u
#define PCI64_LPSS_I2C_MMIO_FLAG_CONFIG_ONLY_DETECT 0x00000020u

struct boot_info;

void pci64_init(const struct boot_info *boot_info);
u32 pci64_device_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_multifunction_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_storage_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_ide_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_ahci_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_nvme_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_raid_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_other_storage_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_intel_system_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_candidate_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_usb_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_display_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_address(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_vendor_device(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_class(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_bar5(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_inventory_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_mmio_base(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_mmio_span_hint(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_mmio_flags(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_ahci_mmio_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_address(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_vendor_device(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_class(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_bar0(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_bar1(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_mmio_base_low(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_mmio_base_high(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_mmio_span_hint(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_mmio_flags(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_nvme_mmio_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_other_storage_address(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_other_storage_vendor_device(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_other_storage_class(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_other_storage_bar0(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_other_storage_bar1(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_intel_system_address(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_intel_system_vendor_device(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_intel_system_class(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_intel_system_bar0(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_intel_system_bar1(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_address(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_vendor_device(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_class(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_bar0(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_bar1(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_mmio_base_low(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_mmio_base_high(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_mmio_span_hint(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_mmio_flags(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_first_vmd_candidate_mmio_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_plan(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_enumerated(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_nvme_count(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_status(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_first_address(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_first_vendor_device(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_first_class(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_first_bar0(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_first_bar1(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_scan_buses(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_scan_devices(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_scan_functions(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_scan_windows(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_scan_truncated(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_mmio_base_low(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_mmio_base_high(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_mmio_span_hint(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_mmio_flags(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_mmio_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_bind_ready(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_bind_status(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_bind_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_register_candidate(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_register_status(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_vmd_nested_register_token(u32 hardware_capability_handle, u32 owner_id);
u32 pci64_query_count(void);
u32 pci64_denial_count(void);
u32 pci64_ecam_rsdp_found(void);
u32 pci64_ecam_mcfg_found(void);
u64 pci64_ecam_base(void);
u32 pci64_ecam_segment(void);
u32 pci64_ecam_bus_start(void);
u32 pci64_ecam_bus_end(void);
u32 pci64_ecam_active(void);
u32 pci64_ecam_fallback_io(void);
u32 pci64_ecam_ahci_found(void);
u32 pci64_lpss_i2c_hid_found(void);
u32 pci64_lpss_i2c_count(void);
u32 pci64_lpss_i2c_address(void);
u32 pci64_lpss_i2c_vendor_device(void);
u32 pci64_lpss_i2c_class(void);
u32 pci64_lpss_i2c_bar0(void);
u32 pci64_lpss_i2c_bar1(void);
u32 pci64_lpss_i2c_base_low(void);
u32 pci64_lpss_i2c_base_high(void);
u32 pci64_lpss_i2c_span_hint(void);
u32 pci64_lpss_i2c_mmio_flags(void);
u32 pci64_lpss_i2c_mmio_token(void);
u32 pci64_lpss_i2c_second_address(void);
u32 pci64_lpss_i2c_second_vendor_device(void);
u32 pci64_lpss_i2c_second_class(void);
u32 pci64_lpss_i2c_second_bar0(void);
u32 pci64_lpss_i2c_second_bar1(void);
u32 pci64_lpss_i2c_second_base_low(void);
u32 pci64_lpss_i2c_second_base_high(void);
u32 pci64_lpss_i2c_second_span_hint(void);
u32 pci64_lpss_i2c_second_mmio_flags(void);
u32 pci64_lpss_i2c_second_mmio_token(void);
u32 pci64_lpss_i2c_pointer_candidate_count(void);
u32 pci64_lpss_i2c_pointer_candidate_address(u32 index);
u32 pci64_lpss_i2c_pointer_candidate_base_low(u32 index);
u32 pci64_lpss_i2c_pointer_candidate_base_high(u32 index);
u32 pci64_lpss_i2c_pointer_candidate_mmio_flags(u32 index);
u32 pci64_usb_uhci_count(void);
u32 pci64_usb_ohci_count(void);
u32 pci64_usb_ehci_count(void);
u32 pci64_usb_xhci_count(void);

#endif
