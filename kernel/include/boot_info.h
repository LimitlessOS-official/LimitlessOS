#ifndef LIMITLESS_BOOT_INFO_H
#define LIMITLESS_BOOT_INFO_H

#include "types.h"

#define LIMITLESS_BOOT_INFO_MAGIC 0x42534F4Cu
#define LIMITLESS_BOOT_FLAG_PROTECTED_MODE 0x00000001u
#define LIMITLESS_BOOT_FLAG_LONG_MODE 0x00000002u
#define LIMITLESS_BOOT_FLAG_PAGING 0x00000004u
#define LIMITLESS_BOOT_FLAG_IDENTITY_MAP 0x00000008u
#define LIMITLESS_BOOT_FLAG_HIGH_HALF_ALIAS 0x00000010u
#define LIMITLESS_BOOT_FLAG_FRAMEBUFFER 0x00000020u
#define LIMITLESS_BOOT_FLAG_BOOT_MEDIA_APPS 0x00000040u

#define LIMITLESS_BOOT_ACPI_FLAG_RSDP 0x00000001u
#define LIMITLESS_BOOT_ACPI_FLAG_XSDT 0x00000002u
#define LIMITLESS_BOOT_ACPI_FLAG_MCFG 0x00000004u
#define LIMITLESS_BOOT_ACPI_FLAG_MADT 0x00000008u
#define LIMITLESS_BOOT_ACPI_FLAG_LAPIC 0x00000010u
#define LIMITLESS_BOOT_ACPI_FLAG_IOAPIC 0x00000020u
#define LIMITLESS_BOOT_ACPI_FLAG_APIC_OVERRIDES 0x00000040u
#define LIMITLESS_BOOT_ACPI_FLAG_FADT 0x00000080u
#define LIMITLESS_BOOT_ACPI_FLAG_DSDT 0x00000100u
#define LIMITLESS_BOOT_ACPI_FLAG_SSDT 0x00000200u

#define LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_RGB 0u
#define LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_BGR 1u
#define LIMITLESS_BOOT_MEDIA_PATH_BYTES 64u
#define LIMITLESS_BOOT_ACPI_SSDT_SLOTS 4u

struct boot_info
{
    u32 magic;
    u32 boot_drive;
    u32 conventional_memory_kb;
    u32 extended_memory_kb;
    u32 kernel_load_address;
    u32 kernel_sector_count;
    u32 architecture_bits;
    u32 bootstrap_flags;
    u32 page_table_root;
    u32 identity_map_bytes;
    u64 framebuffer_base;
    u64 framebuffer_bytes;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u32 framebuffer_pixels_per_scanline;
    u32 framebuffer_format;
    u32 framebuffer_firmware_token;
    u64 memory_map_base;
    u64 memory_map_bytes;
    u32 memory_map_descriptor_size;
    u32 memory_map_descriptor_version;
    u32 memory_map_descriptor_count;
    u32 memory_map_firmware_token;
    u64 acpi_rsdp;
    u64 pci_ecam_base;
    u32 pci_ecam_flags;
    u32 pci_ecam_segment;
    u32 pci_ecam_bus_start;
    u32 pci_ecam_bus_end;
    u64 apic_lapic_base;
    u64 apic_ioapic_base;
    u32 apic_ioapic_id;
    u32 apic_ioapic_gsi_base;
    u32 apic_interrupt_override_scanned;
    u32 apic_interrupt_override_count;
    u32 apic_interrupt_override_valid_mask;
    u32 apic_interrupt_override_source[16];
    u32 apic_interrupt_override_gsi[16];
    u32 apic_interrupt_override_flags[16];
    u64 acpi_xsdt;
    u64 acpi_fadt;
    u32 acpi_fadt_bytes;
    u32 acpi_dsdt_bytes;
    u64 acpi_dsdt;
    u32 acpi_ssdt_count;
    u32 acpi_ssdt_total_bytes;
    u64 acpi_ssdt[LIMITLESS_BOOT_ACPI_SSDT_SLOTS];
    u32 acpi_ssdt_bytes[LIMITLESS_BOOT_ACPI_SSDT_SLOTS];
    u64 boot_media_app_base;
    u32 boot_media_app_bytes;
    u32 boot_media_app_token;
    u64 boot_media_interp_base;
    u32 boot_media_interp_bytes;
    u32 boot_media_interp_token;
    u32 boot_media_flags;
    u32 boot_media_status;
    u32 boot_media_app_path_bytes;
    u8 boot_media_app_path[LIMITLESS_BOOT_MEDIA_PATH_BYTES];
    u32 boot_media_interp_path_bytes;
    u8 boot_media_interp_path[LIMITLESS_BOOT_MEDIA_PATH_BYTES];
};

#endif
