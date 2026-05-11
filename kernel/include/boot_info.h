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

#define LIMITLESS_BOOT_ACPI_FLAG_RSDP 0x00000001u
#define LIMITLESS_BOOT_ACPI_FLAG_XSDT 0x00000002u
#define LIMITLESS_BOOT_ACPI_FLAG_MCFG 0x00000004u
#define LIMITLESS_BOOT_ACPI_FLAG_MADT 0x00000008u
#define LIMITLESS_BOOT_ACPI_FLAG_LAPIC 0x00000010u
#define LIMITLESS_BOOT_ACPI_FLAG_IOAPIC 0x00000020u
#define LIMITLESS_BOOT_ACPI_FLAG_APIC_OVERRIDES 0x00000040u

#define LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_RGB 0u
#define LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_BGR 1u

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
};

#endif
