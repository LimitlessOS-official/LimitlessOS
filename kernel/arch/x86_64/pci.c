#include "pci_x64.h"

#include "boot_info.h"
#include "capability_x64.h"
#include "e1000e_x64.h"
#include "mmio_x64.h"
#include "paging_x64.h"
#include "principal_x64.h"
#include "serial.h"
#include "services.h"
#include "services_x64.h"
#include "virtio_net_x64.h"
#include "x64.h"
#include "xhci_x64.h"

enum
{
    PCI_CONFIG_ADDRESS_PORT = 0xCF8u,
    PCI_CONFIG_DATA_PORT = 0xCFCu,
    PCI_CONFIG_ENABLE = 0x80000000u,
    PCI_VENDOR_INVALID = 0xFFFFu,
    PCI_HEADER_MULTIFUNCTION = 0x80u,
    PCI_MAX_BUS = 256u,
    PCI_MAX_DEVICE = 32u,
    PCI_MAX_FUNCTION = 8u,
    PCI_ECAM_BUS_BYTES = 0x00100000u,
    PCI_ECAM_BUS_PAGES = PCI_ECAM_BUS_BYTES / 0x1000u,

    PCI_CLASS_STORAGE = 0x01u,
    PCI_CLASS_NETWORK = 0x02u,
    PCI_CLASS_DISPLAY = 0x03u,
    PCI_CLASS_SERIAL_BUS = 0x0Cu,
    PCI_SUBCLASS_IDE = 0x01u,
    PCI_SUBCLASS_AHCI = 0x06u,
    PCI_SUBCLASS_NVME = 0x08u,
    PCI_SUBCLASS_USB = 0x03u,
    PCI_PROGIF_UHCI = 0x00u,
    PCI_PROGIF_OHCI = 0x10u,
    PCI_PROGIF_EHCI = 0x20u,
    PCI_PROGIF_XHCI = 0x30u,
    PCI_BAR_IO_SPACE = 0x00000001u,
    PCI_BAR_MEMORY_TYPE_MASK = 0x00000006u,
    PCI_BAR_MEMORY_TYPE_32BIT = 0x00000000u,
    PCI_BAR_MEMORY_TYPE_64BIT = 0x00000004u,
    PCI_BAR_MEMORY_BASE_MASK = 0xFFFFFFF0u,
    PCI_STATUS_COMMAND = 0x04u,
    PCI_STATUS_CAP_LIST = 0x00100000u,
    PCI_COMMAND_MEMORY = 0x00000002u,
    PCI_COMMAND_BUS_MASTER = 0x00000004u,
    PCI_CAPABILITY_POINTER = 0x34u,
    PCI_CAPABILITY_VENDOR = 0x09u,
    PCI_VIRTIO_VENDOR = 0x1AF4u,
    PCI_VIRTIO_NET_DEVICE_LEGACY = 0x1000u,
    PCI_VIRTIO_NET_DEVICE_MODERN = 0x1041u,
    PCI_INTEL_VENDOR = 0x8086u,
    PCI_INTEL_E1000_QEMU_82540EM = 0x100Eu,
    PCI_INTEL_E1000E_82574L = 0x10D3u,
    PCI_INTEL_E1000E_82577LM = 0x10EAu,
    PCI_INTEL_E1000E_82578DM = 0x10F0u,
    PCI_INTEL_E1000E_I217LM = 0x153Au,
    PCI_INTEL_E1000E_I217V = 0x153Bu,
    PCI_INTEL_E1000E_I218LM = 0x15A0u,
    PCI_INTEL_E1000E_I218V = 0x15A1u,
    PCI_INTEL_E1000E_I218LM2 = 0x15A2u,
    PCI_INTEL_E1000E_I218V2 = 0x15A3u,
    PCI_INTEL_E1000E_82579LM = 0x1502u,
    PCI_INTEL_E1000E_82579V = 0x1503u,
    PCI_INTEL_LPSS_I2C_TGL_FIRST = 0xA0E8u,
    PCI_INTEL_LPSS_I2C_TGL_LAST = 0xA0EFu,
    PCI_INTEL_LPSS_I2C_ADL_RPL_FIRST = 0x51E8u,
    PCI_INTEL_LPSS_I2C_ADL_RPL_LAST = 0x51EBu,
    PCI_INTEL_LPSS_I2C_ADL_RPL_EXTRA_FIRST = 0x51D8u,
    PCI_INTEL_LPSS_I2C_ADL_RPL_EXTRA_LAST = 0x51D9u,
    PCI_INTEL_LPSS_I2C_RPL_PCH_P_FIRST = 0x51C5u,
    PCI_INTEL_LPSS_I2C_RPL_PCH_P_LAST = 0x51C6u,
    PCI_INTEL_LPSS_I2C_ICL_FIRST = 0x34E8u,
    PCI_INTEL_LPSS_I2C_ICL_LAST = 0x34EFu,
    PCI_INTEL_LPSS_I2C_ADL_N_FIRST = 0x54E8u,
    PCI_INTEL_LPSS_I2C_ADL_N_LAST = 0x54EFu,
    PCI_VIRTIO_CAP_COMMON_CFG = 1u,
    PCI_VIRTIO_CAP_NOTIFY_CFG = 2u,
    PCI_VIRTIO_CAP_DEVICE_CFG = 4u,
    PCI_AHCI_MMIO_SPAN_HINT = 0x00002000u,
    PCI_NVME_MMIO_SPAN_HINT = 0x00004000u,
    PCI_XHCI_MMIO_SPAN_HINT = 0x00010000u,
    PCI_LPSS_I2C_MMIO_SPAN_HINT = 0x00001000u,
    PCI_VIRTIO_NET_MMIO_SPAN_HINT = 0x00004000u,
    PCI_E1000E_MMIO_SPAN_HINT = 0x00020000u,
    PCI_ECAM_INVALID_BUS = 0xFFFFFFFFu,
    PCI_UEFI_BOOT_DRIVE_MARKER = 0x000000EFu,
    PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT = 8u
};

#define PCI64_ECAM_MAP_VIRTUAL_BASE 0xFFFFFFFF90000000ull

static u32 g_device_count = 0u;
static u32 g_multifunction_count = 0u;
static u32 g_storage_count = 0u;
static u32 g_ide_count = 0u;
static u32 g_ahci_count = 0u;
static u32 g_nvme_count = 0u;
static u32 g_usb_count = 0u;
static u32 g_usb_uhci_count = 0u;
static u32 g_usb_ohci_count = 0u;
static u32 g_usb_ehci_count = 0u;
static u32 g_usb_xhci_count = 0u;
static u32 g_display_count = 0u;
static u32 g_first_ahci_address = 0xFFFFFFFFu;
static u32 g_first_ahci_vendor_device = 0u;
static u32 g_first_ahci_class = 0u;
static u32 g_first_ahci_bar5 = 0u;
static u32 g_first_ahci_mmio_base = 0u;
static u32 g_first_ahci_mmio_span_hint = 0u;
static u32 g_first_ahci_mmio_flags = 0u;
static u32 g_first_ahci_mmio_token = 0u;
static u32 g_first_nvme_address = 0xFFFFFFFFu;
static u32 g_first_nvme_vendor_device = 0u;
static u32 g_first_nvme_class = 0u;
static u32 g_first_nvme_bar0 = 0u;
static u32 g_first_nvme_bar1 = 0u;
static u32 g_first_nvme_mmio_base_low = 0u;
static u32 g_first_nvme_mmio_base_high = 0u;
static u32 g_first_nvme_mmio_span_hint = 0u;
static u32 g_first_nvme_mmio_flags = 0u;
static u32 g_first_nvme_mmio_token = 0u;
static u32 g_first_xhci_address = 0xFFFFFFFFu;
static u32 g_first_xhci_vendor_device = 0u;
static u32 g_first_xhci_class = 0u;
static u32 g_first_xhci_bar0 = 0u;
static u32 g_first_xhci_bar1 = 0u;
static u32 g_first_xhci_mmio_base_low = 0u;
static u32 g_first_xhci_mmio_base_high = 0u;
static u32 g_first_xhci_mmio_span_hint = 0u;
static u32 g_first_xhci_mmio_flags = 0u;
static u32 g_first_xhci_mmio_token = 0u;
static u32 g_first_virtio_net_address = 0xFFFFFFFFu;
static u32 g_first_virtio_net_vendor_device = 0u;
static u32 g_first_virtio_net_class = 0u;
static u32 g_first_virtio_net_bar = 0u;
static u32 g_first_virtio_net_base_low = 0u;
static u32 g_first_virtio_net_base_high = 0u;
static u32 g_first_virtio_net_common_offset = 0u;
static u32 g_first_virtio_net_notify_offset = 0u;
static u32 g_first_virtio_net_device_offset = 0u;
static u32 g_first_virtio_net_notify_multiplier = 0u;
static u32 g_first_virtio_net_common_present = 0u;
static u32 g_first_virtio_net_notify_present = 0u;
static u32 g_first_virtio_net_device_present = 0u;
static u32 g_first_virtio_net_mmio_flags = 0u;
static u32 g_first_virtio_net_mmio_token = 0u;
static u32 g_first_e1000e_address = 0xFFFFFFFFu;
static u32 g_first_e1000e_vendor_device = 0u;
static u32 g_first_e1000e_class = 0u;
static u32 g_first_e1000e_bar0 = 0u;
static u32 g_first_e1000e_bar1 = 0u;
static u32 g_first_e1000e_base_low = 0u;
static u32 g_first_e1000e_base_high = 0u;
static u32 g_first_e1000e_mmio_flags = 0u;
static u32 g_first_e1000e_mmio_token = 0u;
static u32 g_lpss_i2c_count = 0u;
static u32 g_first_lpss_i2c_address = 0xFFFFFFFFu;
static u32 g_first_lpss_i2c_vendor_device = 0u;
static u32 g_first_lpss_i2c_class = 0u;
static u32 g_first_lpss_i2c_bar0 = 0u;
static u32 g_first_lpss_i2c_bar1 = 0u;
static u32 g_first_lpss_i2c_base_low = 0u;
static u32 g_first_lpss_i2c_base_high = 0u;
static u32 g_first_lpss_i2c_span_hint = 0u;
static u32 g_first_lpss_i2c_mmio_flags = 0u;
static u32 g_first_lpss_i2c_mmio_token = 0u;
static u32 g_second_lpss_i2c_address = 0xFFFFFFFFu;
static u32 g_second_lpss_i2c_vendor_device = 0u;
static u32 g_second_lpss_i2c_class = 0u;
static u32 g_second_lpss_i2c_bar0 = 0u;
static u32 g_second_lpss_i2c_bar1 = 0u;
static u32 g_second_lpss_i2c_base_low = 0u;
static u32 g_second_lpss_i2c_base_high = 0u;
static u32 g_second_lpss_i2c_span_hint = 0u;
static u32 g_second_lpss_i2c_mmio_flags = 0u;
static u32 g_second_lpss_i2c_mmio_token = 0u;
static u32 g_lpss_i2c_pointer_candidate_count = 0u;
static u32 g_lpss_i2c_pointer_candidate_address[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_vendor_device[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_class[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_bar0[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_bar1[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_base_low[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_base_high[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_span_hint[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_mmio_flags[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_lpss_i2c_pointer_candidate_mmio_token[PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT];
static u32 g_inventory_token = 2166136261u;
static u32 g_query_count = 0u;
static u32 g_denial_count = 0u;
static u32 g_config_use_ecam = 0u;
static u32 g_ecam_rsdp_found = 0u;
static u32 g_ecam_mcfg_found = 0u;
static u64 g_ecam_base = 0ull;
static u32 g_ecam_segment = 0u;
static u32 g_ecam_bus_start = 0u;
static u32 g_ecam_bus_end = 0u;
static u32 g_ecam_active = 0u;
static u32 g_ecam_fallback_io = 0u;
static u32 g_ecam_ahci_found = 0u;
static u32 g_ecam_mapped_bus = PCI_ECAM_INVALID_BUS;
static u32 g_ecam_map_success_count = 0u;
static u32 g_ecam_map_failed = 0u;

static void pci64_token_u32(u32 value)
{
    g_inventory_token ^= value;
    g_inventory_token *= 16777619u;
}

static u32 pci64_make_address(u32 bus, u32 device, u32 function, u32 offset)
{
    return PCI_CONFIG_ENABLE
        | ((bus & 0xFFu) << 16)
        | ((device & 0x1Fu) << 11)
        | ((function & 0x07u) << 8)
        | (offset & 0xFCu);
}

static u32 pci64_read_config_io(u32 bus, u32 device, u32 function, u32 offset)
{
    outl((u16)PCI_CONFIG_ADDRESS_PORT, pci64_make_address(bus, device, function, offset));
    return inl((u16)PCI_CONFIG_DATA_PORT);
}

static u32 pci64_map_ecam_bus(u32 bus)
{
    u64 physical_base;

    if (g_ecam_map_failed != 0u)
    {
        return 0u;
    }

    if (g_ecam_mapped_bus == bus)
    {
        return 1u;
    }

    if (bus < g_ecam_bus_start || bus > g_ecam_bus_end || g_ecam_base == 0ull)
    {
        return 0u;
    }

    physical_base = g_ecam_base + ((u64)bus << 20);
    if (physical_base < g_ecam_base ||
        physical_base > 0xFFFFFFFFull ||
        ((physical_base & 0xFFFull) != 0ull))
    {
        g_ecam_map_failed = 1u;
        return 0u;
    }

    if (paging64_install_kernel_mmio_mapping(
            PCI64_ECAM_MAP_VIRTUAL_BASE,
            (u32)physical_base,
            PCI_ECAM_BUS_PAGES) == 0u)
    {
        g_ecam_map_failed = 1u;
        g_ecam_mapped_bus = PCI_ECAM_INVALID_BUS;
        return 0u;
    }

    g_ecam_mapped_bus = bus;
    ++g_ecam_map_success_count;
    return 1u;
}

static u32 pci64_read_config_ecam(u32 bus, u32 device, u32 function, u32 offset)
{
    u64 ecam_offset;
    volatile u32 *config;

    if (pci64_map_ecam_bus(bus) == 0u)
    {
        return 0xFFFFFFFFu;
    }

    ecam_offset =
        (((u64)(device & 0x1Fu)) << 15) |
        (((u64)(function & 0x07u)) << 12) |
        (u64)(offset & 0xFCu);
    config = (volatile u32 *)(u64)(PCI64_ECAM_MAP_VIRTUAL_BASE + ecam_offset);
    return *config;
}

static u32 pci64_read_config(u32 bus, u32 device, u32 function, u32 offset)
{
    if (g_config_use_ecam != 0u)
    {
        return pci64_read_config_ecam(bus, device, function, offset);
    }

    return pci64_read_config_io(bus, device, function, offset);
}

static u32 pci64_read_config8(u32 bus, u32 device, u32 function, u32 offset)
{
    u32 value = pci64_read_config(bus, device, function, offset);
    return (value >> ((offset & 3u) * 8u)) & 0xFFu;
}

static void pci64_write_config_ecam(u32 bus, u32 device, u32 function, u32 offset, u32 value)
{
    u64 ecam_offset;
    volatile u32 *config;
    u32 page;

    if (g_config_use_ecam == 0u || pci64_map_ecam_bus(bus) == 0u)
    {
        return;
    }

    ecam_offset =
        (((u64)(device & 0x1Fu)) << 15) |
        (((u64)(function & 0x07u)) << 12) |
        (u64)(offset & 0xFCu);
    page = (u32)(ecam_offset >> 12);
    if (paging64_kernel_mmio_write_window_open(page) == 0u)
    {
        return;
    }

    config = (volatile u32 *)(u64)(PCI64_ECAM_MAP_VIRTUAL_BASE + ecam_offset);
    *config = value;
    (void)*config;
    (void)paging64_kernel_mmio_write_window_close(page);
}

static void pci64_enable_memory_busmaster(u32 bus, u32 device, u32 function)
{
    u32 status_command = pci64_read_config(bus, device, function, PCI_STATUS_COMMAND);
    u32 command = status_command & 0xFFFFu;

    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER;
    pci64_write_config_ecam(
        bus,
        device,
        function,
        PCI_STATUS_COMMAND,
        (status_command & 0xFFFF0000u) | command);
}

static void pci64_enable_memory_decode(u32 bus, u32 device, u32 function)
{
    u32 status_command = pci64_read_config(bus, device, function, PCI_STATUS_COMMAND);
    u32 command = status_command & 0xFFFFu;

    if ((command & PCI_COMMAND_MEMORY) != 0u)
    {
        return;
    }

    command |= PCI_COMMAND_MEMORY;
    pci64_write_config_ecam(
        bus,
        device,
        function,
        PCI_STATUS_COMMAND,
        (status_command & 0xFFFF0000u) | command);
}

static u32 pci64_bar_raw(u32 bus, u32 device, u32 function, u32 bar_index)
{
    if (bar_index >= 6u)
    {
        return 0u;
    }

    return pci64_read_config(bus, device, function, 0x10u + (bar_index * 4u));
}

static void pci64_record_virtio_cap(
    u32 bus,
    u32 device,
    u32 function,
    u32 cap_offset)
{
    u32 cap_len = pci64_read_config8(bus, device, function, cap_offset + 2u);
    u32 cfg_type = pci64_read_config8(bus, device, function, cap_offset + 3u);
    u32 bar = pci64_read_config8(bus, device, function, cap_offset + 4u);
    u32 offset = pci64_read_config(bus, device, function, cap_offset + 8u);
    u32 length = pci64_read_config(bus, device, function, cap_offset + 12u);

    if (bar >= 6u || length == 0u || cap_len < 16u)
    {
        return;
    }

    if (cfg_type == PCI_VIRTIO_CAP_COMMON_CFG)
    {
        g_first_virtio_net_bar = bar;
        g_first_virtio_net_common_offset = offset;
        g_first_virtio_net_common_present = 1u;
    }
    else if (cfg_type == PCI_VIRTIO_CAP_NOTIFY_CFG)
    {
        g_first_virtio_net_bar = bar;
        g_first_virtio_net_notify_offset = offset;
        g_first_virtio_net_notify_present = 1u;
        g_first_virtio_net_notify_multiplier =
            (cap_len >= 20u)
                ? pci64_read_config(bus, device, function, cap_offset + 16u)
                : 0u;
    }
    else if (cfg_type == PCI_VIRTIO_CAP_DEVICE_CFG)
    {
        g_first_virtio_net_bar = bar;
        g_first_virtio_net_device_offset = offset;
        g_first_virtio_net_device_present = 1u;
    }
}

static void pci64_scan_virtio_caps(u32 bus, u32 device, u32 function)
{
    u32 status_command = pci64_read_config(bus, device, function, PCI_STATUS_COMMAND);
    u32 cap = pci64_read_config8(bus, device, function, PCI_CAPABILITY_POINTER) & 0xFCu;
    u32 guard;

    if ((status_command & PCI_STATUS_CAP_LIST) == 0u)
    {
        return;
    }

    for (guard = 0u; guard < 48u && cap >= 0x40u && cap < 0x100u; ++guard)
    {
        u32 cap_id = pci64_read_config8(bus, device, function, cap);
        u32 next = pci64_read_config8(bus, device, function, cap + 1u) & 0xFCu;

        if (cap_id == PCI_CAPABILITY_VENDOR)
        {
            pci64_record_virtio_cap(bus, device, function, cap);
        }

        if (next == cap)
        {
            break;
        }
        cap = next;
    }
}

static void pci64_note_virtio_net(
    u32 bus,
    u32 device,
    u32 function,
    u32 vendor_device,
    u32 class_register)
{
    u32 bar0;
    u32 bar1;
    u32 bar;

    if (g_first_virtio_net_address != 0xFFFFFFFFu)
    {
        return;
    }

    g_first_virtio_net_address = (bus << 16) | (device << 8) | function;
    g_first_virtio_net_vendor_device = vendor_device;
    g_first_virtio_net_class = class_register;
    g_first_virtio_net_bar = 0u;
    g_first_virtio_net_notify_multiplier = 1u;
    pci64_scan_virtio_caps(bus, device, function);
    pci64_enable_memory_busmaster(bus, device, function);

    bar = g_first_virtio_net_bar;
    bar0 = pci64_bar_raw(bus, device, function, bar);
    bar1 = ((bar + 1u) < 6u) ? pci64_bar_raw(bus, device, function, bar + 1u) : 0u;
    g_first_virtio_net_base_low = bar0 & PCI_BAR_MEMORY_BASE_MASK;
    g_first_virtio_net_base_high =
        ((bar0 & PCI_BAR_MEMORY_TYPE_MASK) == PCI_BAR_MEMORY_TYPE_64BIT)
            ? bar1
            : 0u;
}

static u32 pci64_is_e1000e_device(u32 device_id)
{
    switch (device_id)
    {
        case PCI_INTEL_E1000_QEMU_82540EM:
        case PCI_INTEL_E1000E_82574L:
        case PCI_INTEL_E1000E_82577LM:
        case PCI_INTEL_E1000E_82578DM:
        case PCI_INTEL_E1000E_82579LM:
        case PCI_INTEL_E1000E_82579V:
        case PCI_INTEL_E1000E_I217LM:
        case PCI_INTEL_E1000E_I217V:
        case PCI_INTEL_E1000E_I218LM:
        case PCI_INTEL_E1000E_I218V:
        case PCI_INTEL_E1000E_I218LM2:
        case PCI_INTEL_E1000E_I218V2:
            return 1u;
        default:
            return 0u;
    }
}

static u32 pci64_is_lpss_i2c_device(u32 device_id)
{
    return ((device_id >= PCI_INTEL_LPSS_I2C_TGL_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_TGL_LAST))
        || ((device_id >= PCI_INTEL_LPSS_I2C_ADL_RPL_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_ADL_RPL_LAST))
        || ((device_id >= PCI_INTEL_LPSS_I2C_ADL_RPL_EXTRA_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_ADL_RPL_EXTRA_LAST))
        || ((device_id >= PCI_INTEL_LPSS_I2C_RPL_PCH_P_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_RPL_PCH_P_LAST))
        || ((device_id >= PCI_INTEL_LPSS_I2C_ICL_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_ICL_LAST))
        || ((device_id >= PCI_INTEL_LPSS_I2C_ADL_N_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_ADL_N_LAST));
}

static u32 pci64_is_lpss_i2c_primary_keyboard_candidate(u32 device_id)
{
    return (device_id == PCI_INTEL_LPSS_I2C_ADL_RPL_FIRST) ? 1u : 0u;
}

static u32 pci64_is_lpss_i2c_pointer_candidate(u32 device_id)
{
    return ((device_id >= (PCI_INTEL_LPSS_I2C_ADL_RPL_FIRST + 1u))
            && (device_id <= (PCI_INTEL_LPSS_I2C_ADL_RPL_FIRST + 3u)))
        || ((device_id >= PCI_INTEL_LPSS_I2C_ADL_RPL_EXTRA_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_ADL_RPL_EXTRA_LAST))
        || ((device_id >= PCI_INTEL_LPSS_I2C_RPL_PCH_P_FIRST)
            && (device_id <= PCI_INTEL_LPSS_I2C_RPL_PCH_P_LAST));
}

static void pci64_note_lpss_i2c_pointer_candidate(
    u32 address,
    u32 vendor_device,
    u32 class_register,
    u32 bar0,
    u32 bar1)
{
    u32 index;

    for (index = 0u; index < g_lpss_i2c_pointer_candidate_count; ++index)
    {
        if (g_lpss_i2c_pointer_candidate_address[index] == address)
        {
            return;
        }
    }

    if (g_lpss_i2c_pointer_candidate_count >= PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT)
    {
        return;
    }

    index = g_lpss_i2c_pointer_candidate_count;
    g_lpss_i2c_pointer_candidate_address[index] = address;
    g_lpss_i2c_pointer_candidate_vendor_device[index] = vendor_device;
    g_lpss_i2c_pointer_candidate_class[index] = class_register;
    g_lpss_i2c_pointer_candidate_bar0[index] = bar0;
    g_lpss_i2c_pointer_candidate_bar1[index] = bar1;
    ++g_lpss_i2c_pointer_candidate_count;
}

static void pci64_note_e1000e(
    u32 bus,
    u32 device,
    u32 function,
    u32 vendor_device,
    u32 class_register)
{
    if (g_first_e1000e_address != 0xFFFFFFFFu)
    {
        return;
    }

    g_first_e1000e_address = (bus << 16) | (device << 8) | function;
    g_first_e1000e_vendor_device = vendor_device;
    g_first_e1000e_class = class_register;
    g_first_e1000e_bar0 = pci64_bar_raw(bus, device, function, 0u);
    g_first_e1000e_bar1 = pci64_bar_raw(bus, device, function, 1u);
    pci64_enable_memory_busmaster(bus, device, function);
}

static void pci64_note_function(u32 bus, u32 device, u32 function)
{
    u32 vendor_device = pci64_read_config(bus, device, function, 0x00u);
    u32 class_register;
    u32 bar5;
    u32 bar0;
    u32 bar1;
    u32 vendor = vendor_device & 0xFFFFu;
    u32 device_id = (vendor_device >> 16) & 0xFFFFu;
    u32 class_code;
    u32 subclass;
    u32 prog_if;

    if ((vendor == PCI_VENDOR_INVALID) || (vendor == 0u))
    {
        return;
    }

    class_register = pci64_read_config(bus, device, function, 0x08u);
    class_code = (class_register >> 24) & 0xFFu;
    subclass = (class_register >> 16) & 0xFFu;
    prog_if = (class_register >> 8) & 0xFFu;

    ++g_device_count;
    pci64_token_u32((bus << 24) | (device << 16) | (function << 8));
    pci64_token_u32(vendor_device);
    pci64_token_u32(class_register);

    if (class_code == PCI_CLASS_STORAGE)
    {
        ++g_storage_count;
        if (subclass == PCI_SUBCLASS_IDE)
        {
            ++g_ide_count;
        }
        else if (subclass == PCI_SUBCLASS_AHCI)
        {
            ++g_ahci_count;
            if (g_first_ahci_address == 0xFFFFFFFFu)
            {
                bar5 = pci64_read_config(bus, device, function, 0x24u);
                g_first_ahci_address = (bus << 16) | (device << 8) | function;
                g_first_ahci_vendor_device = vendor_device;
                g_first_ahci_class = class_register;
                g_first_ahci_bar5 = bar5;
            }
        }
        else if (subclass == PCI_SUBCLASS_NVME)
        {
            ++g_nvme_count;
            if (g_first_nvme_address == 0xFFFFFFFFu)
            {
                bar0 = pci64_read_config(bus, device, function, 0x10u);
                bar1 = pci64_read_config(bus, device, function, 0x14u);
                g_first_nvme_address = (bus << 16) | (device << 8) | function;
                g_first_nvme_vendor_device = vendor_device;
                g_first_nvme_class = class_register;
                g_first_nvme_bar0 = bar0;
                g_first_nvme_bar1 = bar1;
            }
        }
    }
    else if ((class_code == PCI_CLASS_SERIAL_BUS) && (subclass == PCI_SUBCLASS_USB))
    {
        ++g_usb_count;
        if (prog_if == PCI_PROGIF_UHCI)
        {
            ++g_usb_uhci_count;
        }
        else if (prog_if == PCI_PROGIF_OHCI)
        {
            ++g_usb_ohci_count;
        }
        else if (prog_if == PCI_PROGIF_EHCI)
        {
            ++g_usb_ehci_count;
        }
        else if (prog_if == PCI_PROGIF_XHCI)
        {
            ++g_usb_xhci_count;
        }

        if ((prog_if == PCI_PROGIF_XHCI) && (g_first_xhci_address == 0xFFFFFFFFu))
        {
            bar0 = pci64_read_config(bus, device, function, 0x10u);
            bar1 = pci64_read_config(bus, device, function, 0x14u);
            g_first_xhci_address = (bus << 16) | (device << 8) | function;
            g_first_xhci_vendor_device = vendor_device;
            g_first_xhci_class = class_register;
            g_first_xhci_bar0 = bar0;
            g_first_xhci_bar1 = bar1;
        }
    }
    else if (class_code == PCI_CLASS_DISPLAY)
    {
        ++g_display_count;
    }

    if ((vendor == PCI_VIRTIO_VENDOR)
        && ((device_id == PCI_VIRTIO_NET_DEVICE_LEGACY)
            || (device_id == PCI_VIRTIO_NET_DEVICE_MODERN))
        && (class_code == PCI_CLASS_NETWORK))
    {
        pci64_note_virtio_net(bus, device, function, vendor_device, class_register);
    }
    else if ((g_config_use_ecam != 0u)
        && (vendor == PCI_INTEL_VENDOR)
        && (class_code == PCI_CLASS_NETWORK)
        && (pci64_is_e1000e_device(device_id) != 0u))
    {
        pci64_note_e1000e(bus, device, function, vendor_device, class_register);
    }

    if ((g_config_use_ecam != 0u)
        && (vendor == PCI_INTEL_VENDOR)
        && (pci64_is_lpss_i2c_device(device_id) != 0u))
    {
        u32 lpss_address;
        u32 lpss_bar0;
        u32 lpss_bar1;

        pci64_enable_memory_decode(bus, device, function);
        lpss_address = (bus << 16) | (device << 8) | function;
        lpss_bar0 = pci64_bar_raw(bus, device, function, 0u);
        lpss_bar1 = pci64_bar_raw(bus, device, function, 1u);

        ++g_lpss_i2c_count;
        pci64_note_lpss_i2c_pointer_candidate(
            lpss_address,
            vendor_device,
            class_register,
            lpss_bar0,
            lpss_bar1);

        if ((pci64_is_lpss_i2c_pointer_candidate(device_id) != 0u)
            && (g_second_lpss_i2c_address == 0xFFFFFFFFu))
        {
            g_second_lpss_i2c_address = lpss_address;
            g_second_lpss_i2c_vendor_device = vendor_device;
            g_second_lpss_i2c_class = class_register;
            g_second_lpss_i2c_bar0 = lpss_bar0;
            g_second_lpss_i2c_bar1 = lpss_bar1;
        }
        if ((pci64_is_lpss_i2c_primary_keyboard_candidate(device_id) != 0u)
            || (g_first_lpss_i2c_address == 0xFFFFFFFFu))
        {
            g_first_lpss_i2c_address = lpss_address;
            g_first_lpss_i2c_vendor_device = vendor_device;
            g_first_lpss_i2c_class = class_register;
            g_first_lpss_i2c_bar0 = lpss_bar0;
            g_first_lpss_i2c_bar1 = lpss_bar1;
        }
        else if (g_second_lpss_i2c_address == 0xFFFFFFFFu)
        {
            g_second_lpss_i2c_address = lpss_address;
            g_second_lpss_i2c_vendor_device = vendor_device;
            g_second_lpss_i2c_class = class_register;
            g_second_lpss_i2c_bar0 = lpss_bar0;
            g_second_lpss_i2c_bar1 = lpss_bar1;
        }
    }
}

static int pci64_authorize_query(u32 hardware_capability_handle, u32 owner_id)
{
    u32 endpoint;

    if (principal64_is_active(owner_id) == 0u)
    {
        ++g_denial_count;
        return 0;
    }

    endpoint = capability64_route(
        hardware_capability_handle,
        CAPABILITY64_RIGHT_QUERY,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_HARDWARE))
    {
        ++g_denial_count;
        return 0;
    }

    ++g_query_count;
    return 1;
}

static u32 pci64_authorized_value(
    u32 hardware_capability_handle,
    u32 owner_id,
    u32 value)
{
    if (!pci64_authorize_query(hardware_capability_handle, owner_id))
    {
        return PCI64_INVALID_RESULT;
    }

    return value;
}

static u32 pci64_mix_token(u32 token, u32 value)
{
    token ^= value;
    token *= 16777619u;
    return token;
}

static void pci64_update_ahci_mmio_plan(void)
{
    u32 flags = 0u;
    u32 base = 0u;
    u32 token = 2166136261u;

    if (g_ahci_count != 0u)
    {
        flags |= PCI64_AHCI_MMIO_FLAG_PRESENT;

        if ((g_first_ahci_bar5 & PCI_BAR_IO_SPACE) == 0u)
        {
            flags |= PCI64_AHCI_MMIO_FLAG_MEMORY_BAR;

            if ((g_first_ahci_bar5 & PCI_BAR_MEMORY_TYPE_MASK) == PCI_BAR_MEMORY_TYPE_32BIT)
            {
                flags |= PCI64_AHCI_MMIO_FLAG_32BIT_BAR;
            }

            base = g_first_ahci_bar5 & PCI_BAR_MEMORY_BASE_MASK;
            if (base != 0u)
            {
                flags |= PCI64_AHCI_MMIO_FLAG_BASE_NONZERO;
            }

            if ((base & 0xFFFu) == 0u)
            {
                flags |= PCI64_AHCI_MMIO_FLAG_PAGE_ALIGNED;
            }

            flags |= PCI64_AHCI_MMIO_FLAG_MAPPING_REQUIRED;
        }
    }

    flags |= PCI64_AHCI_MMIO_FLAG_SAFE_NO_TOUCH;
    token = pci64_mix_token(token, g_first_ahci_address);
    token = pci64_mix_token(token, g_first_ahci_vendor_device);
    token = pci64_mix_token(token, g_first_ahci_class);
    token = pci64_mix_token(token, g_first_ahci_bar5);
    token = pci64_mix_token(token, base);
    token = pci64_mix_token(token, PCI_AHCI_MMIO_SPAN_HINT);
    token = pci64_mix_token(token, flags);

    g_first_ahci_mmio_base = base;
    g_first_ahci_mmio_span_hint = ((flags & PCI64_AHCI_MMIO_FLAG_PRESENT) != 0u)
        ? PCI_AHCI_MMIO_SPAN_HINT
        : 0u;
    g_first_ahci_mmio_flags = flags;
    g_first_ahci_mmio_token = token;

    mmio64_register_ahci_candidate(
        g_first_ahci_mmio_base,
        g_first_ahci_mmio_span_hint,
        g_first_ahci_mmio_flags,
        g_first_ahci_mmio_token);
}

static void pci64_update_nvme_mmio_plan(void)
{
    u32 flags = PCI64_NVME_MMIO_FLAG_ADMIN_ONLY
        | PCI64_NVME_MMIO_FLAG_SAFE_NO_IO_QUEUE;
    u32 base_low = 0u;
    u32 base_high = 0u;
    u32 token = 2166136261u;

    if (g_nvme_count != 0u)
    {
        flags |= PCI64_NVME_MMIO_FLAG_PRESENT;

        if ((g_first_nvme_bar0 & PCI_BAR_IO_SPACE) == 0u)
        {
            flags |= PCI64_NVME_MMIO_FLAG_MEMORY_BAR;

            if ((g_first_nvme_bar0 & PCI_BAR_MEMORY_TYPE_MASK)
                == PCI_BAR_MEMORY_TYPE_64BIT)
            {
                flags |= PCI64_NVME_MMIO_FLAG_64BIT_BAR;
                base_high = g_first_nvme_bar1;
            }

            base_low = g_first_nvme_bar0 & PCI_BAR_MEMORY_BASE_MASK;
            if ((base_low != 0u) || (base_high != 0u))
            {
                flags |= PCI64_NVME_MMIO_FLAG_BASE_NONZERO;
            }

            if ((base_low & 0xFFFu) == 0u)
            {
                flags |= PCI64_NVME_MMIO_FLAG_PAGE_ALIGNED;
            }

            if (base_high == 0u)
            {
                flags |= PCI64_NVME_MMIO_FLAG_BELOW_4G;
            }

            flags |= PCI64_NVME_MMIO_FLAG_MAPPING_REQUIRED;
        }
    }

    token = pci64_mix_token(token, g_first_nvme_address);
    token = pci64_mix_token(token, g_first_nvme_vendor_device);
    token = pci64_mix_token(token, g_first_nvme_class);
    token = pci64_mix_token(token, g_first_nvme_bar0);
    token = pci64_mix_token(token, g_first_nvme_bar1);
    token = pci64_mix_token(token, base_low);
    token = pci64_mix_token(token, base_high);
    token = pci64_mix_token(token, PCI_NVME_MMIO_SPAN_HINT);
    token = pci64_mix_token(token, flags);

    g_first_nvme_mmio_base_low = base_low;
    g_first_nvme_mmio_base_high = base_high;
    g_first_nvme_mmio_span_hint =
        ((flags & PCI64_NVME_MMIO_FLAG_PRESENT) != 0u)
            ? PCI_NVME_MMIO_SPAN_HINT
            : 0u;
    g_first_nvme_mmio_flags = flags;
    g_first_nvme_mmio_token = token;

    mmio64_register_nvme_candidate(
        g_first_nvme_mmio_base_low,
        g_first_nvme_mmio_base_high,
        g_first_nvme_mmio_span_hint,
        g_first_nvme_mmio_flags,
        g_first_nvme_mmio_token);
}

static void pci64_update_xhci_mmio_plan(void)
{
    u32 flags = XHCI64_MMIO_FLAG_BROKER_PRIVATE;
    u32 base_low = 0u;
    u32 base_high = 0u;
    u32 token = 2166136261u;

    if (g_first_xhci_address != 0xFFFFFFFFu)
    {
        flags |= XHCI64_MMIO_FLAG_PRESENT;

        if ((g_first_xhci_bar0 & PCI_BAR_IO_SPACE) == 0u)
        {
            flags |= XHCI64_MMIO_FLAG_MEMORY_BAR;

            if ((g_first_xhci_bar0 & PCI_BAR_MEMORY_TYPE_MASK)
                == PCI_BAR_MEMORY_TYPE_64BIT)
            {
                flags |= XHCI64_MMIO_FLAG_64BIT_BAR;
                base_high = g_first_xhci_bar1;
            }

            base_low = g_first_xhci_bar0 & PCI_BAR_MEMORY_BASE_MASK;
            if ((base_low != 0u) || (base_high != 0u))
            {
                flags |= XHCI64_MMIO_FLAG_BASE_NONZERO;
            }

            if ((base_low & 0xFFFu) == 0u)
            {
                flags |= XHCI64_MMIO_FLAG_PAGE_ALIGNED;
            }

            flags |= XHCI64_MMIO_FLAG_MAPPING_REQUIRED;
        }
    }

    token = pci64_mix_token(token, g_first_xhci_address);
    token = pci64_mix_token(token, g_first_xhci_vendor_device);
    token = pci64_mix_token(token, g_first_xhci_class);
    token = pci64_mix_token(token, g_first_xhci_bar0);
    token = pci64_mix_token(token, g_first_xhci_bar1);
    token = pci64_mix_token(token, base_low);
    token = pci64_mix_token(token, base_high);
    token = pci64_mix_token(token, PCI_XHCI_MMIO_SPAN_HINT);
    token = pci64_mix_token(token, flags);

    g_first_xhci_mmio_base_low = base_low;
    g_first_xhci_mmio_base_high = base_high;
    g_first_xhci_mmio_span_hint =
        ((flags & XHCI64_MMIO_FLAG_PRESENT) != 0u)
            ? PCI_XHCI_MMIO_SPAN_HINT
            : 0u;
    g_first_xhci_mmio_flags = flags;
    g_first_xhci_mmio_token = token;

    xhci64_register_candidate(
        g_first_xhci_address,
        g_first_xhci_vendor_device,
        g_first_xhci_class,
        g_first_xhci_bar0,
        g_first_xhci_bar1,
        g_first_xhci_mmio_base_low,
        g_first_xhci_mmio_base_high,
        g_first_xhci_mmio_span_hint,
        g_first_xhci_mmio_flags,
        g_first_xhci_mmio_token);
}

static void pci64_build_lpss_i2c_mmio_plan(
    u32 address,
    u32 vendor_device,
    u32 class_register,
    u32 bar0,
    u32 bar1,
    u32 *base_low_out,
    u32 *base_high_out,
    u32 *span_hint_out,
    u32 *flags_out,
    u32 *token_out)
{
    u32 flags = PCI64_LPSS_I2C_MMIO_FLAG_CONFIG_ONLY_DETECT;
    u32 base_low = 0u;
    u32 base_high = 0u;
    u32 token = 2166136261u;

    if (address != 0xFFFFFFFFu)
    {
        flags |= PCI64_LPSS_I2C_MMIO_FLAG_PRESENT;

        if ((bar0 & PCI_BAR_IO_SPACE) == 0u)
        {
            flags |= PCI64_LPSS_I2C_MMIO_FLAG_MEMORY_BAR;

            if ((bar0 & PCI_BAR_MEMORY_TYPE_MASK)
                == PCI_BAR_MEMORY_TYPE_64BIT)
            {
                flags |= PCI64_LPSS_I2C_MMIO_FLAG_64BIT_BAR;
                base_high = bar1;
            }

            base_low = bar0 & PCI_BAR_MEMORY_BASE_MASK;
            if ((base_low != 0u) || (base_high != 0u))
            {
                flags |= PCI64_LPSS_I2C_MMIO_FLAG_BASE_NONZERO;
            }

            if ((base_low & 0xFFFu) == 0u)
            {
                flags |= PCI64_LPSS_I2C_MMIO_FLAG_PAGE_ALIGNED;
            }
        }
    }

    token = pci64_mix_token(token, address);
    token = pci64_mix_token(token, vendor_device);
    token = pci64_mix_token(token, class_register);
    token = pci64_mix_token(token, bar0);
    token = pci64_mix_token(token, bar1);
    token = pci64_mix_token(token, base_low);
    token = pci64_mix_token(token, base_high);
    token = pci64_mix_token(token, PCI_LPSS_I2C_MMIO_SPAN_HINT);
    token = pci64_mix_token(token, flags);
    token = (token != 0u) ? token : 1u;

    *base_low_out = base_low;
    *base_high_out = base_high;
    *span_hint_out =
        ((flags & PCI64_LPSS_I2C_MMIO_FLAG_PRESENT) != 0u)
            ? PCI_LPSS_I2C_MMIO_SPAN_HINT
            : 0u;
    *flags_out = flags;
    *token_out = token;
}

static void pci64_update_lpss_i2c_mmio_plan(void)
{
    u32 index;

    pci64_build_lpss_i2c_mmio_plan(
        g_first_lpss_i2c_address,
        g_first_lpss_i2c_vendor_device,
        g_first_lpss_i2c_class,
        g_first_lpss_i2c_bar0,
        g_first_lpss_i2c_bar1,
        &g_first_lpss_i2c_base_low,
        &g_first_lpss_i2c_base_high,
        &g_first_lpss_i2c_span_hint,
        &g_first_lpss_i2c_mmio_flags,
        &g_first_lpss_i2c_mmio_token);

    pci64_build_lpss_i2c_mmio_plan(
        g_second_lpss_i2c_address,
        g_second_lpss_i2c_vendor_device,
        g_second_lpss_i2c_class,
        g_second_lpss_i2c_bar0,
        g_second_lpss_i2c_bar1,
        &g_second_lpss_i2c_base_low,
        &g_second_lpss_i2c_base_high,
        &g_second_lpss_i2c_span_hint,
        &g_second_lpss_i2c_mmio_flags,
        &g_second_lpss_i2c_mmio_token);

    for (index = 0u; index < g_lpss_i2c_pointer_candidate_count; ++index)
    {
        pci64_build_lpss_i2c_mmio_plan(
            g_lpss_i2c_pointer_candidate_address[index],
            g_lpss_i2c_pointer_candidate_vendor_device[index],
            g_lpss_i2c_pointer_candidate_class[index],
            g_lpss_i2c_pointer_candidate_bar0[index],
            g_lpss_i2c_pointer_candidate_bar1[index],
            &g_lpss_i2c_pointer_candidate_base_low[index],
            &g_lpss_i2c_pointer_candidate_base_high[index],
            &g_lpss_i2c_pointer_candidate_span_hint[index],
            &g_lpss_i2c_pointer_candidate_mmio_flags[index],
            &g_lpss_i2c_pointer_candidate_mmio_token[index]);
    }
}

static void pci64_update_virtio_net_mmio_plan(void)
{
    u32 flags = VIRTIO_NET64_MMIO_FLAG_BROKER_PRIVATE;
    u32 token = 2166136261u;

    if (g_first_virtio_net_address != 0xFFFFFFFFu)
    {
        flags |= VIRTIO_NET64_MMIO_FLAG_PRESENT;

        if ((g_first_virtio_net_base_low != 0u) || (g_first_virtio_net_base_high != 0u))
        {
            flags |= VIRTIO_NET64_MMIO_FLAG_BASE_NONZERO;
        }

        if ((g_first_virtio_net_base_low & 0xFFFu) == 0u)
        {
            flags |= VIRTIO_NET64_MMIO_FLAG_PAGE_ALIGNED;
        }

        flags |= VIRTIO_NET64_MMIO_FLAG_MEMORY_BAR;
        if (g_first_virtio_net_base_high != 0u)
        {
            flags |= VIRTIO_NET64_MMIO_FLAG_64BIT_BAR;
        }

        if (g_first_virtio_net_common_present != 0u)
        {
            flags |= VIRTIO_NET64_MMIO_FLAG_COMMON_CAP;
        }
        if (g_first_virtio_net_notify_present != 0u)
        {
            flags |= VIRTIO_NET64_MMIO_FLAG_NOTIFY_CAP;
        }
        if (g_first_virtio_net_device_present != 0u)
        {
            flags |= VIRTIO_NET64_MMIO_FLAG_DEVICE_CAP;
        }
    }

    token = pci64_mix_token(token, g_first_virtio_net_address);
    token = pci64_mix_token(token, g_first_virtio_net_vendor_device);
    token = pci64_mix_token(token, g_first_virtio_net_class);
    token = pci64_mix_token(token, g_first_virtio_net_bar);
    token = pci64_mix_token(token, g_first_virtio_net_base_low);
    token = pci64_mix_token(token, g_first_virtio_net_base_high);
    token = pci64_mix_token(token, g_first_virtio_net_common_offset);
    token = pci64_mix_token(token, g_first_virtio_net_notify_offset);
    token = pci64_mix_token(token, g_first_virtio_net_device_offset);
    token = pci64_mix_token(token, g_first_virtio_net_notify_multiplier);
    token = pci64_mix_token(token, flags);
    token = (token != 0u) ? token : 1u;

    g_first_virtio_net_mmio_flags = flags;
    g_first_virtio_net_mmio_token = token;
    virtio_net64_register_candidate(
        g_first_virtio_net_address,
        g_first_virtio_net_vendor_device,
        g_first_virtio_net_class,
        g_first_virtio_net_bar,
        g_first_virtio_net_base_low,
        g_first_virtio_net_base_high,
        ((flags & VIRTIO_NET64_MMIO_FLAG_PRESENT) != 0u)
            ? PCI_VIRTIO_NET_MMIO_SPAN_HINT
            : 0u,
        g_first_virtio_net_common_offset,
        g_first_virtio_net_notify_offset,
        g_first_virtio_net_device_offset,
        g_first_virtio_net_notify_multiplier,
        g_first_virtio_net_mmio_flags,
        g_first_virtio_net_mmio_token);
}

static void pci64_update_e1000e_mmio_plan(void)
{
    u32 flags = E1000E64_MMIO_FLAG_BROKER_PRIVATE;
    u32 base_low = 0u;
    u32 base_high = 0u;
    u32 token = 2166136261u;

    if (g_first_e1000e_address != 0xFFFFFFFFu)
    {
        flags |= E1000E64_MMIO_FLAG_PRESENT;

        if ((g_first_e1000e_bar0 & PCI_BAR_IO_SPACE) == 0u)
        {
            flags |= E1000E64_MMIO_FLAG_MEMORY_BAR;

            if ((g_first_e1000e_bar0 & PCI_BAR_MEMORY_TYPE_MASK)
                == PCI_BAR_MEMORY_TYPE_64BIT)
            {
                flags |= E1000E64_MMIO_FLAG_64BIT_BAR;
                base_high = g_first_e1000e_bar1;
            }

            base_low = g_first_e1000e_bar0 & PCI_BAR_MEMORY_BASE_MASK;
            if ((base_low != 0u) || (base_high != 0u))
            {
                flags |= E1000E64_MMIO_FLAG_BASE_NONZERO;
            }

            if ((base_low & 0xFFFu) == 0u)
            {
                flags |= E1000E64_MMIO_FLAG_PAGE_ALIGNED;
            }
        }
    }

    token = pci64_mix_token(token, g_first_e1000e_address);
    token = pci64_mix_token(token, g_first_e1000e_vendor_device);
    token = pci64_mix_token(token, g_first_e1000e_class);
    token = pci64_mix_token(token, g_first_e1000e_bar0);
    token = pci64_mix_token(token, g_first_e1000e_bar1);
    token = pci64_mix_token(token, base_low);
    token = pci64_mix_token(token, base_high);
    token = pci64_mix_token(token, PCI_E1000E_MMIO_SPAN_HINT);
    token = pci64_mix_token(token, flags);
    token = (token != 0u) ? token : 1u;

    g_first_e1000e_base_low = base_low;
    g_first_e1000e_base_high = base_high;
    g_first_e1000e_mmio_flags = flags;
    g_first_e1000e_mmio_token = token;
    e1000e64_register_candidate(
        g_first_e1000e_address,
        g_first_e1000e_vendor_device,
        g_first_e1000e_class,
        g_first_e1000e_bar0,
        g_first_e1000e_bar1,
        g_first_e1000e_base_low,
        g_first_e1000e_base_high,
        ((flags & E1000E64_MMIO_FLAG_PRESENT) != 0u)
            ? PCI_E1000E_MMIO_SPAN_HINT
            : 0u,
        g_first_e1000e_mmio_flags,
        g_first_e1000e_mmio_token);
}

static void pci64_configure_ecam(const struct boot_info *boot_info)
{
    if (boot_info == 0 || boot_info->boot_drive != PCI_UEFI_BOOT_DRIVE_MARKER)
    {
        return;
    }

    g_ecam_rsdp_found =
        ((boot_info->pci_ecam_flags & LIMITLESS_BOOT_ACPI_FLAG_RSDP) != 0u &&
         boot_info->acpi_rsdp != 0ull) ? 1u : 0u;
    g_ecam_mcfg_found =
        ((boot_info->pci_ecam_flags & LIMITLESS_BOOT_ACPI_FLAG_MCFG) != 0u &&
         boot_info->pci_ecam_base != 0ull &&
         boot_info->pci_ecam_segment == 0u &&
         boot_info->pci_ecam_bus_end >= boot_info->pci_ecam_bus_start) ? 1u : 0u;

    if (g_ecam_mcfg_found != 0u)
    {
        g_ecam_base = boot_info->pci_ecam_base;
        g_ecam_segment = boot_info->pci_ecam_segment;
        g_ecam_bus_start = boot_info->pci_ecam_bus_start;
        g_ecam_bus_end = boot_info->pci_ecam_bus_end;
        if (g_ecam_bus_end >= PCI_MAX_BUS)
        {
            g_ecam_bus_end = PCI_MAX_BUS - 1u;
        }
    }
}

static void pci64_scan_bus_range(u32 start_bus, u32 end_bus)
{
    u32 bus;
    u32 device;
    u32 function;

    if (end_bus >= PCI_MAX_BUS)
    {
        end_bus = PCI_MAX_BUS - 1u;
    }

    for (bus = start_bus; bus <= end_bus; ++bus)
    {
        for (device = 0u; device < PCI_MAX_DEVICE; ++device)
        {
            u32 header = pci64_read_config(bus, device, 0u, 0x0Cu);
            u32 function_count = ((header >> 16) & PCI_HEADER_MULTIFUNCTION) != 0u
                ? PCI_MAX_FUNCTION
                : 1u;

            if ((pci64_read_config(bus, device, 0u, 0x00u) & 0xFFFFu) == PCI_VENDOR_INVALID)
            {
                continue;
            }

            if (function_count > 1u)
            {
                ++g_multifunction_count;
            }

            for (function = 0u; function < function_count; ++function)
            {
                pci64_note_function(bus, device, function);
            }
        }

        if (bus == end_bus)
        {
            break;
        }
    }
}

void pci64_init(const struct boot_info *boot_info)
{
    u32 candidate_index;

    g_device_count = 0u;
    g_multifunction_count = 0u;
    g_storage_count = 0u;
    g_ide_count = 0u;
    g_ahci_count = 0u;
    g_nvme_count = 0u;
    g_usb_count = 0u;
    g_usb_uhci_count = 0u;
    g_usb_ohci_count = 0u;
    g_usb_ehci_count = 0u;
    g_usb_xhci_count = 0u;
    g_display_count = 0u;
    g_first_ahci_address = 0xFFFFFFFFu;
    g_first_ahci_vendor_device = 0u;
    g_first_ahci_class = 0u;
    g_first_ahci_bar5 = 0u;
    g_first_ahci_mmio_base = 0u;
    g_first_ahci_mmio_span_hint = 0u;
    g_first_ahci_mmio_flags = 0u;
    g_first_ahci_mmio_token = 0u;
    g_first_nvme_address = 0xFFFFFFFFu;
    g_first_nvme_vendor_device = 0u;
    g_first_nvme_class = 0u;
    g_first_nvme_bar0 = 0u;
    g_first_nvme_bar1 = 0u;
    g_first_nvme_mmio_base_low = 0u;
    g_first_nvme_mmio_base_high = 0u;
    g_first_nvme_mmio_span_hint = 0u;
    g_first_nvme_mmio_flags = 0u;
    g_first_nvme_mmio_token = 0u;
    g_first_xhci_address = 0xFFFFFFFFu;
    g_first_xhci_vendor_device = 0u;
    g_first_xhci_class = 0u;
    g_first_xhci_bar0 = 0u;
    g_first_xhci_bar1 = 0u;
    g_first_xhci_mmio_base_low = 0u;
    g_first_xhci_mmio_base_high = 0u;
    g_first_xhci_mmio_span_hint = 0u;
    g_first_xhci_mmio_flags = 0u;
    g_first_xhci_mmio_token = 0u;
    g_first_virtio_net_address = 0xFFFFFFFFu;
    g_first_virtio_net_vendor_device = 0u;
    g_first_virtio_net_class = 0u;
    g_first_virtio_net_bar = 0u;
    g_first_virtio_net_base_low = 0u;
    g_first_virtio_net_base_high = 0u;
    g_first_virtio_net_common_offset = 0u;
    g_first_virtio_net_notify_offset = 0u;
    g_first_virtio_net_device_offset = 0u;
    g_first_virtio_net_notify_multiplier = 0u;
    g_first_virtio_net_common_present = 0u;
    g_first_virtio_net_notify_present = 0u;
    g_first_virtio_net_device_present = 0u;
    g_first_virtio_net_mmio_flags = 0u;
    g_first_virtio_net_mmio_token = 0u;
    g_first_e1000e_address = 0xFFFFFFFFu;
    g_first_e1000e_vendor_device = 0u;
    g_first_e1000e_class = 0u;
    g_first_e1000e_bar0 = 0u;
    g_first_e1000e_bar1 = 0u;
    g_first_e1000e_base_low = 0u;
    g_first_e1000e_base_high = 0u;
    g_first_e1000e_mmio_flags = 0u;
    g_first_e1000e_mmio_token = 0u;
    g_lpss_i2c_count = 0u;
    g_first_lpss_i2c_address = 0xFFFFFFFFu;
    g_first_lpss_i2c_vendor_device = 0u;
    g_first_lpss_i2c_class = 0u;
    g_first_lpss_i2c_bar0 = 0u;
    g_first_lpss_i2c_bar1 = 0u;
    g_first_lpss_i2c_base_low = 0u;
    g_first_lpss_i2c_base_high = 0u;
    g_first_lpss_i2c_span_hint = 0u;
    g_first_lpss_i2c_mmio_flags = 0u;
    g_first_lpss_i2c_mmio_token = 0u;
    g_second_lpss_i2c_address = 0xFFFFFFFFu;
    g_second_lpss_i2c_vendor_device = 0u;
    g_second_lpss_i2c_class = 0u;
    g_second_lpss_i2c_bar0 = 0u;
    g_second_lpss_i2c_bar1 = 0u;
    g_second_lpss_i2c_base_low = 0u;
    g_second_lpss_i2c_base_high = 0u;
    g_second_lpss_i2c_span_hint = 0u;
    g_second_lpss_i2c_mmio_flags = 0u;
    g_second_lpss_i2c_mmio_token = 0u;
    g_lpss_i2c_pointer_candidate_count = 0u;
    for (candidate_index = 0u;
         candidate_index < PCI64_LPSS_I2C_POINTER_CANDIDATE_LIMIT;
         ++candidate_index)
    {
        g_lpss_i2c_pointer_candidate_address[candidate_index] = 0xFFFFFFFFu;
        g_lpss_i2c_pointer_candidate_vendor_device[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_class[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_bar0[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_bar1[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_base_low[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_base_high[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_span_hint[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_mmio_flags[candidate_index] = 0u;
        g_lpss_i2c_pointer_candidate_mmio_token[candidate_index] = 0u;
    }
    g_inventory_token = 2166136261u;
    g_query_count = 0u;
    g_denial_count = 0u;
    g_config_use_ecam = 0u;
    g_ecam_rsdp_found = 0u;
    g_ecam_mcfg_found = 0u;
    g_ecam_base = 0ull;
    g_ecam_segment = 0u;
    g_ecam_bus_start = 0u;
    g_ecam_bus_end = 0u;
    g_ecam_active = 0u;
    g_ecam_fallback_io = 0u;
    g_ecam_ahci_found = 0u;
    g_ecam_mapped_bus = PCI_ECAM_INVALID_BUS;
    g_ecam_map_success_count = 0u;
    g_ecam_map_failed = 0u;

    pci64_configure_ecam(boot_info);
    if (g_ecam_mcfg_found != 0u)
    {
        g_config_use_ecam = 1u;
        pci64_scan_bus_range(g_ecam_bus_start, g_ecam_bus_end);
        g_ecam_active = (g_ecam_map_success_count != 0u) ? 1u : 0u;
        g_ecam_ahci_found = (g_ecam_active != 0u && g_ahci_count != 0u) ? 1u : 0u;
    }

    if (g_ecam_active == 0u)
    {
        g_config_use_ecam = 0u;
        g_ecam_fallback_io = 1u;
        pci64_scan_bus_range(0u, PCI_MAX_BUS - 1u);
    }

    pci64_update_ahci_mmio_plan();
    pci64_update_nvme_mmio_plan();
    pci64_update_xhci_mmio_plan();
    pci64_update_lpss_i2c_mmio_plan();
    pci64_update_virtio_net_mmio_plan();
    pci64_update_e1000e_mmio_plan();
    serial_write_string("[x64] I2C detect config-only complete\n");
}

u32 pci64_device_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_device_count);
}

u32 pci64_multifunction_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_multifunction_count);
}

u32 pci64_storage_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_storage_count);
}

u32 pci64_ide_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_ide_count);
}

u32 pci64_ahci_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_ahci_count);
}

u32 pci64_nvme_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_nvme_count);
}

u32 pci64_usb_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_usb_count);
}

u32 pci64_display_count(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_display_count);
}

u32 pci64_first_ahci_address(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_address);
}

u32 pci64_first_ahci_vendor_device(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_vendor_device);
}

u32 pci64_first_ahci_class(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_class);
}

u32 pci64_first_ahci_bar5(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_bar5);
}

u32 pci64_inventory_token(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_inventory_token);
}

u32 pci64_first_ahci_mmio_base(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_mmio_base);
}

u32 pci64_first_ahci_mmio_span_hint(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_mmio_span_hint);
}

u32 pci64_first_ahci_mmio_flags(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_mmio_flags);
}

u32 pci64_first_ahci_mmio_token(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_ahci_mmio_token);
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 pci64_first_nvme_address(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_address);
}

u32 pci64_first_nvme_vendor_device(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_vendor_device);
}

u32 pci64_first_nvme_class(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_class);
}

u32 pci64_first_nvme_bar0(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_bar0);
}

u32 pci64_first_nvme_bar1(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_bar1);
}

u32 pci64_first_nvme_mmio_base_low(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_mmio_base_low);
}

u32 pci64_first_nvme_mmio_base_high(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_mmio_base_high);
}

u32 pci64_first_nvme_mmio_span_hint(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_mmio_span_hint);
}

u32 pci64_first_nvme_mmio_flags(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_mmio_flags);
}

u32 pci64_first_nvme_mmio_token(u32 hardware_capability_handle, u32 owner_id)
{
    return pci64_authorized_value(hardware_capability_handle, owner_id, g_first_nvme_mmio_token);
}
#endif

u32 pci64_query_count(void)
{
    return g_query_count;
}

u32 pci64_denial_count(void)
{
    return g_denial_count;
}

u32 pci64_ecam_rsdp_found(void)
{
    return g_ecam_rsdp_found;
}

u32 pci64_ecam_mcfg_found(void)
{
    return g_ecam_mcfg_found;
}

u64 pci64_ecam_base(void)
{
    return g_ecam_base;
}

u32 pci64_ecam_segment(void)
{
    return g_ecam_segment;
}

u32 pci64_ecam_bus_start(void)
{
    return g_ecam_bus_start;
}

u32 pci64_ecam_bus_end(void)
{
    return g_ecam_bus_end;
}

u32 pci64_ecam_active(void)
{
    return g_ecam_active;
}

u32 pci64_ecam_fallback_io(void)
{
    return g_ecam_fallback_io;
}

u32 pci64_ecam_ahci_found(void)
{
    return g_ecam_ahci_found;
}

u32 pci64_lpss_i2c_hid_found(void)
{
    return (g_lpss_i2c_count != 0u) ? 1u : 0u;
}

u32 pci64_lpss_i2c_count(void)
{
    return g_lpss_i2c_count;
}

u32 pci64_lpss_i2c_address(void)
{
    return g_first_lpss_i2c_address;
}

u32 pci64_lpss_i2c_vendor_device(void)
{
    return g_first_lpss_i2c_vendor_device;
}

u32 pci64_lpss_i2c_class(void)
{
    return g_first_lpss_i2c_class;
}

u32 pci64_lpss_i2c_bar0(void)
{
    return g_first_lpss_i2c_bar0;
}

u32 pci64_lpss_i2c_bar1(void)
{
    return g_first_lpss_i2c_bar1;
}

u32 pci64_lpss_i2c_base_low(void)
{
    return g_first_lpss_i2c_base_low;
}

u32 pci64_lpss_i2c_base_high(void)
{
    return g_first_lpss_i2c_base_high;
}

u32 pci64_lpss_i2c_span_hint(void)
{
    return g_first_lpss_i2c_span_hint;
}

u32 pci64_lpss_i2c_mmio_flags(void)
{
    return g_first_lpss_i2c_mmio_flags;
}

u32 pci64_lpss_i2c_mmio_token(void)
{
    return g_first_lpss_i2c_mmio_token;
}

u32 pci64_lpss_i2c_second_address(void)
{
    return g_second_lpss_i2c_address;
}

u32 pci64_lpss_i2c_second_vendor_device(void)
{
    return g_second_lpss_i2c_vendor_device;
}

u32 pci64_lpss_i2c_second_class(void)
{
    return g_second_lpss_i2c_class;
}

u32 pci64_lpss_i2c_second_bar0(void)
{
    return g_second_lpss_i2c_bar0;
}

u32 pci64_lpss_i2c_second_bar1(void)
{
    return g_second_lpss_i2c_bar1;
}

u32 pci64_lpss_i2c_second_base_low(void)
{
    return g_second_lpss_i2c_base_low;
}

u32 pci64_lpss_i2c_second_base_high(void)
{
    return g_second_lpss_i2c_base_high;
}

u32 pci64_lpss_i2c_second_span_hint(void)
{
    return g_second_lpss_i2c_span_hint;
}

u32 pci64_lpss_i2c_second_mmio_flags(void)
{
    return g_second_lpss_i2c_mmio_flags;
}

u32 pci64_lpss_i2c_second_mmio_token(void)
{
    return g_second_lpss_i2c_mmio_token;
}

u32 pci64_lpss_i2c_pointer_candidate_count(void)
{
    return g_lpss_i2c_pointer_candidate_count;
}

u32 pci64_lpss_i2c_pointer_candidate_address(u32 index)
{
    return (index < g_lpss_i2c_pointer_candidate_count)
        ? g_lpss_i2c_pointer_candidate_address[index]
        : 0xFFFFFFFFu;
}

u32 pci64_lpss_i2c_pointer_candidate_base_low(u32 index)
{
    return (index < g_lpss_i2c_pointer_candidate_count)
        ? g_lpss_i2c_pointer_candidate_base_low[index]
        : 0u;
}

u32 pci64_lpss_i2c_pointer_candidate_base_high(u32 index)
{
    return (index < g_lpss_i2c_pointer_candidate_count)
        ? g_lpss_i2c_pointer_candidate_base_high[index]
        : 0u;
}

u32 pci64_lpss_i2c_pointer_candidate_mmio_flags(u32 index)
{
    return (index < g_lpss_i2c_pointer_candidate_count)
        ? g_lpss_i2c_pointer_candidate_mmio_flags[index]
        : 0u;
}

u32 pci64_usb_uhci_count(void)
{
    return g_usb_uhci_count;
}

u32 pci64_usb_ohci_count(void)
{
    return g_usb_ohci_count;
}

u32 pci64_usb_ehci_count(void)
{
    return g_usb_ehci_count;
}

u32 pci64_usb_xhci_count(void)
{
    return g_usb_xhci_count;
}
