#include "hardware_registry_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

#include "display_x64.h"
#include "e1000e_x64.h"
#include "i2c_hid_x64.h"
#include "input_x64.h"
#include "mmio_x64.h"
#include "pci_x64.h"
#include "virtio_net_x64.h"
#include "xhci_x64.h"

struct hardware64_device_record
{
    u32 active;
    u32 class_id;
    u32 subclass_id;
    u32 binding;
    u32 source;
    u32 address;
    u32 flags;
    u32 token;
};

enum
{
    HARDWARE64_SUBCLASS_PLATFORM_ECAM = 1u,
    HARDWARE64_SUBCLASS_PLATFORM_LEGACY_PCI = 2u,
    HARDWARE64_SUBCLASS_DISPLAY_GOP = 1u,
    HARDWARE64_SUBCLASS_DISPLAY_PCI = 2u,
    HARDWARE64_SUBCLASS_INPUT_KEYBOARD = 1u,
    HARDWARE64_SUBCLASS_INPUT_POINTER = 2u,
    HARDWARE64_SUBCLASS_STORAGE_NVME = 1u,
    HARDWARE64_SUBCLASS_STORAGE_AHCI = 2u,
    HARDWARE64_SUBCLASS_USB_UHCI = 1u,
    HARDWARE64_SUBCLASS_USB_OHCI = 2u,
    HARDWARE64_SUBCLASS_USB_EHCI = 3u,
    HARDWARE64_SUBCLASS_USB_XHCI = 4u,
    HARDWARE64_SUBCLASS_NETWORK_VIRTIO = 1u,
    HARDWARE64_SUBCLASS_NETWORK_E1000E = 2u,

    HARDWARE64_SOURCE_PCI = 1u,
    HARDWARE64_SOURCE_UEFI = 2u,
    HARDWARE64_SOURCE_PS2 = 3u,
    HARDWARE64_SOURCE_XHCI = 4u,
    HARDWARE64_SOURCE_I2C = 5u,
    HARDWARE64_SOURCE_NVME = 6u,
    HARDWARE64_SOURCE_NETWORK = 7u
};

static struct hardware64_device_record g_hardware64_records[HARDWARE64_REGISTRY_MAX_DEVICES];
static u32 g_hardware64_refresh_count = 0u;
static u32 g_hardware64_count = 0u;
static u32 g_hardware64_overflow_count = 0u;
static u32 g_hardware64_token = 2166136261u;
static u32 g_hardware64_pci_device_count = 0u;
static u32 g_hardware64_pci_query_denial_count = 0u;
static u32 g_hardware64_acpi_table_count = 0u;
static u32 g_hardware64_display_count = 0u;
static u32 g_hardware64_input_count = 0u;
static u32 g_hardware64_storage_count = 0u;
static u32 g_hardware64_usb_count = 0u;
static u32 g_hardware64_network_count = 0u;
static u32 g_hardware64_bound_count = 0u;
static u32 g_hardware64_candidate_count = 0u;
static u32 g_hardware64_deferred_count = 0u;
static u32 g_hardware64_unsupported_count = 0u;
static u32 g_hardware64_failed_count = 0u;

static void hardware64_token_u32(u32 value)
{
    g_hardware64_token ^= value;
    g_hardware64_token *= 16777619u;
}

static u32 hardware64_authorized_count(u32 value)
{
    if (value == PCI64_INVALID_RESULT)
    {
        ++g_hardware64_pci_query_denial_count;
        return 0u;
    }

    return value;
}

static void hardware64_note_class(u32 class_id)
{
    if (class_id == HARDWARE64_CLASS_DISPLAY)
    {
        ++g_hardware64_display_count;
    }
    else if (class_id == HARDWARE64_CLASS_INPUT)
    {
        ++g_hardware64_input_count;
    }
    else if (class_id == HARDWARE64_CLASS_STORAGE)
    {
        ++g_hardware64_storage_count;
    }
    else if (class_id == HARDWARE64_CLASS_USB)
    {
        ++g_hardware64_usb_count;
    }
    else if (class_id == HARDWARE64_CLASS_NETWORK)
    {
        ++g_hardware64_network_count;
    }
}

static void hardware64_note_binding(u32 binding)
{
    if (binding == HARDWARE64_BINDING_BOUND)
    {
        ++g_hardware64_bound_count;
    }
    else if (binding == HARDWARE64_BINDING_CANDIDATE)
    {
        ++g_hardware64_candidate_count;
    }
    else if (binding == HARDWARE64_BINDING_DEFERRED)
    {
        ++g_hardware64_deferred_count;
    }
    else if (binding == HARDWARE64_BINDING_UNSUPPORTED)
    {
        ++g_hardware64_unsupported_count;
    }
    else if (binding == HARDWARE64_BINDING_FAILED)
    {
        ++g_hardware64_failed_count;
    }
}

static u32 hardware64_append(
    u32 class_id,
    u32 subclass_id,
    u32 binding,
    u32 source,
    u32 address,
    u32 flags)
{
    struct hardware64_device_record *record;
    u32 token = 2166136261u;

    if (g_hardware64_count >= HARDWARE64_REGISTRY_MAX_DEVICES)
    {
        ++g_hardware64_overflow_count;
        ++g_hardware64_failed_count;
        hardware64_token_u32(class_id);
        hardware64_token_u32(HARDWARE64_BINDING_FAILED);
        return 0u;
    }

    token ^= class_id;
    token *= 16777619u;
    token ^= subclass_id;
    token *= 16777619u;
    token ^= binding;
    token *= 16777619u;
    token ^= source;
    token *= 16777619u;
    token ^= address;
    token *= 16777619u;
    token ^= flags;
    token *= 16777619u;

    record = &g_hardware64_records[g_hardware64_count];
    record->active = 1u;
    record->class_id = class_id;
    record->subclass_id = subclass_id;
    record->binding = binding;
    record->source = source;
    record->address = address;
    record->flags = flags;
    record->token = token;
    ++g_hardware64_count;

    hardware64_note_class(class_id);
    hardware64_note_binding(binding);
    hardware64_token_u32(token);
    return 1u;
}

static void hardware64_reset(void)
{
    u32 index;

    for (index = 0u; index < HARDWARE64_REGISTRY_MAX_DEVICES; ++index)
    {
        g_hardware64_records[index].active = 0u;
        g_hardware64_records[index].class_id = 0u;
        g_hardware64_records[index].subclass_id = 0u;
        g_hardware64_records[index].binding = HARDWARE64_BINDING_NONE;
        g_hardware64_records[index].source = 0u;
        g_hardware64_records[index].address = 0u;
        g_hardware64_records[index].flags = 0u;
        g_hardware64_records[index].token = 0u;
    }

    g_hardware64_count = 0u;
    g_hardware64_pci_query_denial_count = 0u;
    g_hardware64_acpi_table_count = 0u;
    g_hardware64_display_count = 0u;
    g_hardware64_input_count = 0u;
    g_hardware64_storage_count = 0u;
    g_hardware64_usb_count = 0u;
    g_hardware64_network_count = 0u;
    g_hardware64_bound_count = 0u;
    g_hardware64_candidate_count = 0u;
    g_hardware64_deferred_count = 0u;
    g_hardware64_unsupported_count = 0u;
    g_hardware64_failed_count = 0u;
    g_hardware64_token = 2166136261u;
}

static void hardware64_refresh_platform(void)
{
    g_hardware64_acpi_table_count = pci64_ecam_rsdp_found() + pci64_ecam_mcfg_found();
    if (pci64_ecam_active() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_PLATFORM,
            HARDWARE64_SUBCLASS_PLATFORM_ECAM,
            HARDWARE64_BINDING_BOUND,
            HARDWARE64_SOURCE_PCI,
            (u32)pci64_ecam_base(),
            pci64_ecam_segment());
    }
    else if (pci64_ecam_fallback_io() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_PLATFORM,
            HARDWARE64_SUBCLASS_PLATFORM_LEGACY_PCI,
            HARDWARE64_BINDING_BOUND,
            HARDWARE64_SOURCE_PCI,
            0u,
            0u);
    }
}

static void hardware64_refresh_display(u32 pci_display_count)
{
    if (display64_available() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_DISPLAY,
            HARDWARE64_SUBCLASS_DISPLAY_GOP,
            HARDWARE64_BINDING_BOUND,
            HARDWARE64_SOURCE_UEFI,
            display64_width(),
            display64_height());
    }

    if (pci_display_count != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_DISPLAY,
            HARDWARE64_SUBCLASS_DISPLAY_PCI,
            HARDWARE64_BINDING_DEFERRED,
            HARDWARE64_SOURCE_PCI,
            pci_display_count,
            0u);
    }
}

static void hardware64_refresh_input(void)
{
    if ((input64_ps2_present() != 0u) || (input64_keyboard_scancode_count() != 0u))
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_INPUT,
            HARDWARE64_SUBCLASS_INPUT_KEYBOARD,
            (input64_ps2_enabled() != 0u) ? HARDWARE64_BINDING_BOUND : HARDWARE64_BINDING_CANDIDATE,
            HARDWARE64_SOURCE_PS2,
            input64_keyboard_scancode_count(),
            input64_ps2_status_snapshot());
    }

    if (xhci64_hid_device() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_INPUT,
            HARDWARE64_SUBCLASS_INPUT_KEYBOARD,
            HARDWARE64_BINDING_BOUND,
            HARDWARE64_SOURCE_XHCI,
            xhci64_hid_report_read(),
            xhci64_connected_ports());
    }

    if (i2c_hid64_device_found() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_INPUT,
            HARDWARE64_SUBCLASS_INPUT_KEYBOARD,
            HARDWARE64_BINDING_BOUND,
            HARDWARE64_SOURCE_I2C,
            i2c_hid64_report_count(),
            i2c_hid64_error());
    }

    if ((input64_mouse_found() != 0u) || (input64_ps2_mouse_init_done() != 0u))
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_INPUT,
            HARDWARE64_SUBCLASS_INPUT_POINTER,
            (input64_mouse_enabled() != 0u) ? HARDWARE64_BINDING_BOUND : HARDWARE64_BINDING_CANDIDATE,
            HARDWARE64_SOURCE_PS2,
            input64_mouse_packet_count(),
            input64_ps2_mouse_ack());
    }

    if (xhci64_mouse_device() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_INPUT,
            HARDWARE64_SUBCLASS_INPUT_POINTER,
            HARDWARE64_BINDING_BOUND,
            HARDWARE64_SOURCE_XHCI,
            xhci64_mouse_reports(),
            xhci64_mouse_report_bytes());
    }

    if (i2c_hid64_pointer_found() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_INPUT,
            HARDWARE64_SUBCLASS_INPUT_POINTER,
            HARDWARE64_BINDING_BOUND,
            HARDWARE64_SOURCE_I2C,
            i2c_hid64_pointer_report_count(),
            i2c_hid64_pointer_error());
    }
}

static void hardware64_refresh_storage(u32 pci_ahci_count, u32 pci_nvme_count)
{
    if ((pci_nvme_count != 0u) || (mmio64_nvme_probe_found() != 0u))
    {
        u32 binding = HARDWARE64_BINDING_CANDIDATE;
        if (mmio64_nvme_probe_ready() != 0u)
        {
            binding = HARDWARE64_BINDING_BOUND;
        }
        else if (mmio64_nvme_probe_error() != 0u)
        {
            binding = HARDWARE64_BINDING_FAILED;
        }

        (void)hardware64_append(
            HARDWARE64_CLASS_STORAGE,
            HARDWARE64_SUBCLASS_STORAGE_NVME,
            binding,
            HARDWARE64_SOURCE_NVME,
            (u32)mmio64_nvme_probe_bar0(),
            mmio64_nvme_fat_located());
    }

    if ((pci_ahci_count != 0u) || (pci64_ecam_ahci_found() != 0u))
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_STORAGE,
            HARDWARE64_SUBCLASS_STORAGE_AHCI,
            HARDWARE64_BINDING_DEFERRED,
            HARDWARE64_SOURCE_PCI,
            pci_ahci_count,
            pci64_ecam_ahci_found());
    }
}

static void hardware64_refresh_usb(void)
{
    if (pci64_usb_uhci_count() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_USB,
            HARDWARE64_SUBCLASS_USB_UHCI,
            HARDWARE64_BINDING_UNSUPPORTED,
            HARDWARE64_SOURCE_PCI,
            pci64_usb_uhci_count(),
            0u);
    }
    if (pci64_usb_ohci_count() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_USB,
            HARDWARE64_SUBCLASS_USB_OHCI,
            HARDWARE64_BINDING_UNSUPPORTED,
            HARDWARE64_SOURCE_PCI,
            pci64_usb_ohci_count(),
            0u);
    }
    if (pci64_usb_ehci_count() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_USB,
            HARDWARE64_SUBCLASS_USB_EHCI,
            HARDWARE64_BINDING_DEFERRED,
            HARDWARE64_SOURCE_PCI,
            pci64_usb_ehci_count(),
            0u);
    }
    if (pci64_usb_xhci_count() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_USB,
            HARDWARE64_SUBCLASS_USB_XHCI,
            (xhci64_mapped() != 0u) ? HARDWARE64_BINDING_BOUND : HARDWARE64_BINDING_CANDIDATE,
            HARDWARE64_SOURCE_XHCI,
            pci64_usb_xhci_count(),
            xhci64_hcs_ports());
    }
}

static void hardware64_refresh_network(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (virtio_net64_found() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_NETWORK,
            HARDWARE64_SUBCLASS_NETWORK_VIRTIO,
            (virtio_net64_driver_ok() != 0u) ? HARDWARE64_BINDING_BOUND : HARDWARE64_BINDING_CANDIDATE,
            HARDWARE64_SOURCE_NETWORK,
            (u32)virtio_net64_bar_base(),
            virtio_net64_error());
    }

    if (e1000e64_found() != 0u)
    {
        (void)hardware64_append(
            HARDWARE64_CLASS_NETWORK,
            HARDWARE64_SUBCLASS_NETWORK_E1000E,
            (e1000e64_mapped() != 0u) ? HARDWARE64_BINDING_BOUND : HARDWARE64_BINDING_CANDIDATE,
            HARDWARE64_SOURCE_NETWORK,
            (u32)e1000e64_bar_base(),
            e1000e64_error());
    }
#endif
}

void hardware64_registry_refresh(u32 hardware_capability_handle, u32 owner_id)
{
    u32 pci_ahci_count;
    u32 pci_nvme_count;
    u32 pci_display_count;

    hardware64_reset();
    ++g_hardware64_refresh_count;

    g_hardware64_pci_device_count =
        hardware64_authorized_count(pci64_device_count(hardware_capability_handle, owner_id));
    pci_ahci_count =
        hardware64_authorized_count(pci64_ahci_count(hardware_capability_handle, owner_id));
    pci_nvme_count =
        hardware64_authorized_count(pci64_nvme_count(hardware_capability_handle, owner_id));
    pci_display_count =
        hardware64_authorized_count(pci64_display_count(hardware_capability_handle, owner_id));

    hardware64_token_u32(g_hardware64_pci_device_count);
    hardware64_refresh_platform();
    hardware64_refresh_display(pci_display_count);
    hardware64_refresh_input();
    hardware64_refresh_storage(pci_ahci_count, pci_nvme_count);
    hardware64_refresh_usb();
    hardware64_refresh_network();
}

u32 hardware64_registry_refresh_count(void)
{
    return g_hardware64_refresh_count;
}

u32 hardware64_registry_limit(void)
{
    return HARDWARE64_REGISTRY_MAX_DEVICES;
}

u32 hardware64_registry_count(void)
{
    return g_hardware64_count;
}

u32 hardware64_registry_overflow_count(void)
{
    return g_hardware64_overflow_count;
}

u32 hardware64_registry_token(void)
{
    return g_hardware64_token;
}

u32 hardware64_registry_pci_device_count(void)
{
    return g_hardware64_pci_device_count;
}

u32 hardware64_registry_pci_query_denial_count(void)
{
    return g_hardware64_pci_query_denial_count;
}

u32 hardware64_registry_acpi_table_count(void)
{
    return g_hardware64_acpi_table_count;
}

u32 hardware64_registry_display_device_count(void)
{
    return g_hardware64_display_count;
}

u32 hardware64_registry_input_device_count(void)
{
    return g_hardware64_input_count;
}

u32 hardware64_registry_storage_device_count(void)
{
    return g_hardware64_storage_count;
}

u32 hardware64_registry_usb_controller_count(void)
{
    return g_hardware64_usb_count;
}

u32 hardware64_registry_network_device_count(void)
{
    return g_hardware64_network_count;
}

u32 hardware64_registry_driver_bound_count(void)
{
    return g_hardware64_bound_count;
}

u32 hardware64_registry_driver_candidate_count(void)
{
    return g_hardware64_candidate_count;
}

u32 hardware64_registry_driver_deferred_count(void)
{
    return g_hardware64_deferred_count;
}

u32 hardware64_registry_driver_unsupported_count(void)
{
    return g_hardware64_unsupported_count;
}

u32 hardware64_registry_driver_failed_count(void)
{
    return g_hardware64_failed_count;
}

static const struct hardware64_device_record *hardware64_record_at(u32 index)
{
    if (index >= g_hardware64_count)
    {
        return 0;
    }

    return &g_hardware64_records[index];
}

u32 hardware64_registry_record_active(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->active : 0u;
}

u32 hardware64_registry_record_class(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->class_id : 0u;
}

u32 hardware64_registry_record_subclass(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->subclass_id : 0u;
}

u32 hardware64_registry_record_binding(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->binding : HARDWARE64_BINDING_NONE;
}

u32 hardware64_registry_record_source(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->source : 0u;
}

u32 hardware64_registry_record_address(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->address : 0u;
}

u32 hardware64_registry_record_flags(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->flags : 0u;
}

u32 hardware64_registry_record_token(u32 index)
{
    const struct hardware64_device_record *record = hardware64_record_at(index);
    return (record != 0) ? record->token : 0u;
}

#else

void hardware64_registry_refresh(u32 hardware_capability_handle, u32 owner_id)
{
    (void)hardware_capability_handle;
    (void)owner_id;
}

u32 hardware64_registry_refresh_count(void)
{
    return 0u;
}

u32 hardware64_registry_limit(void)
{
    return HARDWARE64_REGISTRY_MAX_DEVICES;
}

u32 hardware64_registry_count(void)
{
    return 0u;
}

u32 hardware64_registry_overflow_count(void)
{
    return 0u;
}

u32 hardware64_registry_token(void)
{
    return 0u;
}

u32 hardware64_registry_pci_device_count(void)
{
    return 0u;
}

u32 hardware64_registry_pci_query_denial_count(void)
{
    return 0u;
}

u32 hardware64_registry_acpi_table_count(void)
{
    return 0u;
}

u32 hardware64_registry_display_device_count(void)
{
    return 0u;
}

u32 hardware64_registry_input_device_count(void)
{
    return 0u;
}

u32 hardware64_registry_storage_device_count(void)
{
    return 0u;
}

u32 hardware64_registry_usb_controller_count(void)
{
    return 0u;
}

u32 hardware64_registry_network_device_count(void)
{
    return 0u;
}

u32 hardware64_registry_driver_bound_count(void)
{
    return 0u;
}

u32 hardware64_registry_driver_candidate_count(void)
{
    return 0u;
}

u32 hardware64_registry_driver_deferred_count(void)
{
    return 0u;
}

u32 hardware64_registry_driver_unsupported_count(void)
{
    return 0u;
}

u32 hardware64_registry_driver_failed_count(void)
{
    return 0u;
}

#endif
