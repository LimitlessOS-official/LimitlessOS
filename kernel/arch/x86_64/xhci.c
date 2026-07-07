#include "xhci_x64.h"

#include "input_x64.h"
#include "paging_x64.h"
#include "serial.h"

#define XHCI64_MAP_VIRTUAL_BASE 0xFFFFFFFF90180000ull
#define XHCI64_MAP_PAGES 16u
#define XHCI64_PAGE_BYTES 4096u
#define XHCI64_RING_TRBS 64u
#define XHCI64_EVENT_TRBS 64u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define XHCI64_MAX_SLOTS 16u
#else
#define XHCI64_MAX_SLOTS 8u
#endif
#define XHCI64_MAX_SCRATCHPADS 256u
#define XHCI64_TRB_DWORDS 4u
#define XHCI64_PORT_REGISTER_BYTES 0x10u
#define XHCI64_VENDOR_QEMU 0x1B36u
#define XHCI64_VENDOR_NEC 0x1033u
#define XHCI64_LIVE_EVENT_POLL_LIMIT 128u
#define XHCI64_HID_PROTOCOL_ANY 0xFFu
#define XHCI64_PORT_PROBE_LIMIT 2u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define XHCI64_MOUSE_REPORT_BUFFER_BYTES 64u
#else
#define XHCI64_MOUSE_REPORT_BUFFER_BYTES 8u
#endif
#define XHCI64_MOUSE_MIN_PROBE_PACKET_BYTES 3u
#define XHCI64_MOUSE_MAX_PROBE_PACKET_BYTES XHCI64_MOUSE_REPORT_BUFFER_BYTES

#define XHCI64_CAP_HCSPARAMS1 0x04u
#define XHCI64_CAP_HCSPARAMS2 0x08u
#define XHCI64_CAP_HCCPARAMS1 0x10u
#define XHCI64_CAP_DBOFF 0x14u
#define XHCI64_CAP_RTSOFF 0x18u

#define XHCI64_EXTCAP_ID_MASK 0x000000FFu
#define XHCI64_EXTCAP_NEXT_SHIFT 8u
#define XHCI64_EXTCAP_NEXT_MASK 0x0000FF00u
#define XHCI64_EXTCAP_USB_LEGACY 0x01u
#define XHCI64_EXTCAP_SUPPORTED_PROTOCOL 0x02u
#define XHCI64_EXTCAP_INTEL_VENDOR 0xC0u
#define XHCI64_EXTCAP_USB_NAME 0x20425355u
#define XHCI64_EXTCAP_MAX_STEPS 64u
#define XHCI64_USBLEGSUP_BIOS_OWNED 0x00010000u
#define XHCI64_USBLEGSUP_OS_OWNED 0x01000000u

#define XHCI64_OP_USBCMD 0x00u
#define XHCI64_OP_USBSTS 0x04u
#define XHCI64_OP_CRCR 0x18u
#define XHCI64_OP_DCBAAP 0x30u
#define XHCI64_OP_CONFIG 0x38u
#define XHCI64_OP_PORTS 0x400u

#define XHCI64_USBCMD_RS 0x00000001u
#define XHCI64_USBCMD_HCRST 0x00000002u
#define XHCI64_USBCMD_INTE 0x00000004u
#define XHCI64_USBSTS_HCH 0x00000001u
#define XHCI64_USBSTS_CNR 0x00000800u

#define XHCI64_PORTSC_CCS 0x00000001u
#define XHCI64_PORTSC_PED 0x00000002u
#define XHCI64_PORTSC_PR 0x00000010u
#define XHCI64_PORTSC_PP 0x00000200u
#define XHCI64_PORTSC_PLS_SHIFT 5u
#define XHCI64_PORTSC_PLS_MASK 0x000001E0u
#define XHCI64_PORTSC_SPEED_SHIFT 10u
#define XHCI64_PORTSC_CHANGE_MASK 0x00FE0000u

#define XHCI64_INTR0_OFFSET 0x20u
#define XHCI64_INTR_IMAN 0x00u
#define XHCI64_INTR_ERSTSZ 0x08u
#define XHCI64_INTR_ERSTBA 0x10u
#define XHCI64_INTR_ERDP 0x18u

#define XHCI64_TRB_CYCLE 0x00000001u
#define XHCI64_TRB_TOGGLE_CYCLE 0x00000002u
#define XHCI64_TRB_ISP 0x00000004u
#define XHCI64_TRB_IOC 0x00000020u
#define XHCI64_TRB_IDT 0x00000040u
#define XHCI64_TRB_TYPE_SHIFT 10u
#define XHCI64_TRB_DIR 0x00010000u
#define XHCI64_TRB_TRT_IN 0x00030000u

#define XHCI64_TRB_TYPE_NORMAL 1u
#define XHCI64_TRB_TYPE_SETUP_STAGE 2u
#define XHCI64_TRB_TYPE_DATA_STAGE 3u
#define XHCI64_TRB_TYPE_STATUS_STAGE 4u
#define XHCI64_TRB_TYPE_LINK 6u
#define XHCI64_TRB_TYPE_ENABLE_SLOT 9u
#define XHCI64_TRB_TYPE_DISABLE_SLOT 10u
#define XHCI64_TRB_TYPE_ADDRESS_DEVICE 11u
#define XHCI64_TRB_TYPE_CONFIGURE_ENDPOINT 12u
#define XHCI64_TRB_TYPE_TRANSFER_EVENT 32u
#define XHCI64_TRB_TYPE_COMMAND_COMPLETION 33u

#define XHCI64_COMPLETION_SUCCESS 1u
#define XHCI64_COMPLETION_USB_TRANSACTION_ERROR 4u
#define XHCI64_COMPLETION_SHORT_PACKET 13u

#define XHCI64_EP_TYPE_BULK_OUT 2u
#define XHCI64_EP_TYPE_CONTROL 4u
#define XHCI64_EP_TYPE_BULK_IN 6u
#define XHCI64_EP_TYPE_INTERRUPT_IN 7u

#define XHCI64_USB_DESC_DEVICE 1u
#define XHCI64_USB_DESC_CONFIGURATION 2u
#define XHCI64_USB_DESC_INTERFACE 4u
#define XHCI64_USB_DESC_ENDPOINT 5u
#define XHCI64_USB_DESC_HID 0x21u
#define XHCI64_USB_DESC_REPORT 0x22u

#define XHCI64_USB_REQ_GET_DESCRIPTOR 6u
#define XHCI64_USB_REQ_SET_CONFIGURATION 9u
#define XHCI64_USB_REQ_SET_PROTOCOL 11u

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define XHCI64_USB_CLASS_MASS_STORAGE 0x08u
#define XHCI64_USB_MSC_SUBCLASS_SCSI 0x06u
#define XHCI64_USB_MSC_PROTOCOL_BULK_ONLY 0x50u
#define XHCI64_BOT_CBW_SIGNATURE 0x43425355u
#define XHCI64_BOT_CSW_SIGNATURE 0x53425355u
#define XHCI64_BOT_CBW_BYTES 31u
#define XHCI64_BOT_CSW_BYTES 13u
#define XHCI64_SCSI_INQUIRY 0x12u
#define XHCI64_SCSI_TEST_UNIT_READY 0x00u
#define XHCI64_SCSI_READ_CAPACITY_10 0x25u
#define XHCI64_SCSI_READ_10 0x28u
#define XHCI64_SCSI_WRITE_10 0x2Au
#define XHCI64_USB_STORAGE_SECTOR_BYTES 512u
#endif

#define XHCI64_DCI_EP0 1u
#define XHCI64_DEFAULT_MPS 8u
#define XHCI64_CONTROL_POLL_LIMIT 1000000u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define XHCI64_DISABLE_SLOT_POLL_LIMIT 10000u
#endif
#define XHCI64_PORT_RESET_POLL_LIMIT 250000u
#define XHCI64_LEGACY_HANDOFF_POLL_LIMIT 1000000u
#define XHCI64_DELAY_1MS_POLLS 25000u
#define XHCI64_CONNECTION_RETRIES 10u
#define XHCI64_ADDRESS_RECOVERY_RETRIES 2u
#define XHCI64_PORT_RESET_WAIT_MS 100u
#define XHCI64_DEVICE_SETTLE_MS 50u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define XHCI64_HID_INTERFACE_LIMIT 8u
#endif

struct xhci64_trb
{
    u32 dword[XHCI64_TRB_DWORDS];
};

struct xhci64_erst_entry
{
    u64 base;
    u32 size;
    u32 reserved;
};

struct xhci64_event
{
    u64 parameter;
    u32 status;
    u32 control;
};

struct xhci64_keyboard_endpoint
{
    u32 present;
    u32 slot_id;
    u32 dci;
    u32 max_packet;
    u32 interval;
    u32 interface_number;
    u32 interface_class;
    u32 interface_subclass;
    u32 interface_protocol;
    u32 report_length;
};

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
struct xhci64_hid_candidate
{
    struct xhci64_keyboard_endpoint endpoint;
};

struct xhci64_storage_endpoint
{
    u32 present;
    u32 slot_id;
    u32 port_id;
    u32 configuration_value;
    u32 interface_number;
    u32 bulk_in_dci;
    u32 bulk_out_dci;
    u32 bulk_in_mps;
    u32 bulk_out_mps;
    u32 bulk_in_interval;
    u32 bulk_out_interval;
};
#endif

static u32 xhci64_usb16(const u8 *bytes);

static u32 g_xhci_address = 0xFFFFFFFFu;
static u32 g_xhci_vendor_device = 0u;
static u32 g_xhci_class = 0u;
static u32 g_xhci_bar0 = 0u;
static u32 g_xhci_bar1 = 0u;
static u32 g_xhci_base_low = 0u;
static u32 g_xhci_base_high = 0u;
static u32 g_xhci_span_hint = 0u;
static u32 g_xhci_flags = 0u;
static u32 g_xhci_token = 0u;

static u32 g_xhci_mapped = 0u;
static u32 g_xhci_cap_length = 0u;
static u32 g_xhci_max_slots = 0u;
static u32 g_xhci_hcs_ports = 0u;
static u32 g_xhci_context_size = 32u;
static u32 g_xhci_doorbell_offset = 0u;
static u32 g_xhci_runtime_offset = 0u;
static u32 g_xhci_ports_scanned = 0u;
static u32 g_xhci_connected_ports = 0u;
static u32 g_xhci_command_ring_staged = 0u;
static u32 g_xhci_dcbaa_staged = 0u;
static u32 g_xhci_event_ring_staged = 0u;
static u32 g_xhci_controller_reset = 0u;
static u32 g_xhci_controller_running = 0u;
static u32 g_xhci_slot_enabled = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_xhci_slots_disabled = 0u;
static u32 g_xhci_slot_disable_failures = 0u;
#endif
static u32 g_xhci_addressed = 0u;
static u32 g_xhci_config_read = 0u;
static u32 g_xhci_hid_report_read = 0u;
static u32 g_xhci_endpoint_configured = 0u;
static u32 g_xhci_hid_device = 0u;
static u32 g_xhci_input_live = 0u;
static u32 g_xhci_report_count = 0u;
static u32 g_xhci_report_bytes = 0u;
static u32 g_xhci_mouse_device = 0u;
static u32 g_xhci_mouse_reports = 0u;
static u32 g_xhci_mouse_report_bytes = 0u;
static u32 g_xhci_mouse_report_offset = 0u;
static u32 g_xhci_mouse_report_size = 8u;
static u32 g_xhci_live_polling_enabled = 0u;
static u32 g_xhci_live_rescan_countdown = 0u;
static u32 g_xhci_unavailable = 1u;
static u32 g_xhci_error = 0u;
static u32 g_xhci_port_error_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_xhci_last_skip_port = 0u;
static u32 g_xhci_last_skip_code = 0u;
static u32 g_xhci_last_device_class = 0u;
static u32 g_xhci_last_device_subclass = 0u;
static u32 g_xhci_last_device_protocol = 0u;
static u32 g_xhci_last_config_total_length = 0u;
static u32 g_xhci_last_interface_class = 0u;
static u32 g_xhci_last_interface_subclass = 0u;
static u32 g_xhci_last_interface_protocol = 0u;
static u32 g_xhci_last_endpoint_max_packet = 0u;
static u32 g_xhci_broad_mouse_probe_count = 0u;
static u32 g_xhci_vendor_mouse_candidate_count = 0u;
static u32 g_xhci_port_probe_attempts = 0u;
static u32 g_xhci_port_probe_retry_skips = 0u;
static u32 g_xhci_first_hid_port = 0u;
static u32 g_xhci_first_hid_interface_class = 0u;
static u32 g_xhci_first_hid_interface_subclass = 0u;
static u32 g_xhci_first_hid_interface_protocol = 0u;
static u32 g_xhci_first_mouse_candidate_port = 0u;
static u32 g_xhci_first_mouse_candidate_interface_class = 0u;
static u32 g_xhci_first_mouse_candidate_interface_subclass = 0u;
static u32 g_xhci_first_mouse_candidate_interface_protocol = 0u;
static u32 g_xhci_first_mouse_candidate_endpoint_mps = 0u;
#endif
static u32 g_xhci_extcaps_scanned = 0u;
static u32 g_xhci_legacy_cap_found = 0u;
static u32 g_xhci_legacy_handoff = 0u;
static u32 g_xhci_legacy_bios_owned_before = 0u;
static u32 g_xhci_legacy_bios_owned_clear = 0u;
static u32 g_xhci_legacy_os_owned = 0u;
static u32 g_xhci_protocol_caps = 0u;
static u32 g_xhci_usb2_ports = 0u;
static u32 g_xhci_usb3_ports = 0u;
static u32 g_xhci_prefer_usb2 = 0u;
static u32 g_xhci_intel_cap_found = 0u;
static u32 g_xhci_intel_workaround = 0u;
static u32 g_xhci_command_enqueue = 0u;
static u32 g_xhci_command_cycle = 1u;
static u32 g_xhci_event_dequeue = 0u;
static u32 g_xhci_event_cycle = 1u;
static u32 g_xhci_ep0_enqueue = 0u;
static u32 g_xhci_ep0_cycle = 1u;
static u32 g_xhci_intr_enqueue = 0u;
static u32 g_xhci_intr_cycle = 1u;
static u32 g_xhci_intr_pending = 0u;
static u32 g_xhci_mouse_intr_enqueue = 0u;
static u32 g_xhci_mouse_intr_cycle = 1u;
static u32 g_xhci_mouse_intr_pending = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_xhci_bulk_in_enqueue = 0u;
static u32 g_xhci_bulk_in_cycle = 1u;
static u32 g_xhci_bulk_out_enqueue = 0u;
static u32 g_xhci_bulk_out_cycle = 1u;
static u32 g_xhci_last_enable_slot_completion = 0u;
static u32 g_xhci_last_enable_slot_id = 0u;
static u32 g_xhci_last_address_completion = 0u;
static u32 g_xhci_last_address_slot = 0u;
static u32 g_xhci_last_address_port = 0u;
static u32 g_xhci_last_address_speed = 0u;
static u32 g_xhci_last_address_event_dw0 = 0u;
static u32 g_xhci_last_address_event_dw1 = 0u;
static u32 g_xhci_last_address_event_dw2 = 0u;
static u32 g_xhci_last_address_event_dw3 = 0u;
static u32 g_xhci_pre_address_portsc = 0u;
static u32 g_xhci_pre_address_portsc_pls = 0u;
static u32 g_xhci_post_address_portsc = 0u;
static u32 g_xhci_post_address_portsc_pls = 0u;
static u32 g_xhci_address_failure_count = 0u;
static u32 g_xhci_hid_interface_inventory = 0u;
static u32 g_xhci_boot_mouse_interface = 0xFFFFFFFFu;
static u32 g_xhci_boot_mouse_port = 0u;
static u32 g_xhci_boot_mouse_dci = 0u;
static u32 g_xhci_boot_mouse_mps = 0u;
static u32 g_xhci_boot_mouse_configured = 0u;
static u32 g_xhci_mouse_last_completion = 0u;
static u32 g_xhci_mouse_last_event_dw0 = 0u;
static u32 g_xhci_mouse_last_event_dw1 = 0u;
static u32 g_xhci_mouse_last_event_dw2 = 0u;
static u32 g_xhci_mouse_last_event_dw3 = 0u;
static u32 g_xhci_last_disable_slot_completion = 0u;
static u32 g_xhci_last_disable_slot_id = 0u;
static u32 g_xhci_usb_storage_present = 0u;
static u32 g_xhci_usb_storage_ready = 0u;
static u32 g_xhci_usb_storage_error = 0u;
static u32 g_xhci_usb_storage_read_count = 0u;
static u32 g_xhci_usb_storage_write_count = 0u;
static u32 g_xhci_usb_storage_last_lba = 0u;
static u32 g_xhci_usb_storage_block_bytes = 0u;
static u32 g_xhci_usb_storage_last_completion = 0u;
static u32 g_xhci_usb_storage_last_tag = 0u;
#endif

static u64 g_xhci_dcbaa[256] __attribute__((aligned(4096)));
static u64 g_xhci_scratchpad_array[XHCI64_MAX_SCRATCHPADS] __attribute__((aligned(4096)));
static u8 g_xhci_scratchpads[XHCI64_MAX_SCRATCHPADS][XHCI64_PAGE_BYTES] __attribute__((aligned(4096)));
static u8 g_xhci_input_context[XHCI64_PAGE_BYTES] __attribute__((aligned(4096)));
static u8 g_xhci_device_contexts[XHCI64_MAX_SLOTS + 1u][XHCI64_PAGE_BYTES] __attribute__((aligned(4096)));
static struct xhci64_trb g_xhci_command_ring[XHCI64_RING_TRBS] __attribute__((aligned(4096)));
static struct xhci64_trb g_xhci_ep0_ring[XHCI64_RING_TRBS] __attribute__((aligned(4096)));
static struct xhci64_trb g_xhci_interrupt_ring[XHCI64_RING_TRBS] __attribute__((aligned(4096)));
static struct xhci64_trb g_xhci_mouse_interrupt_ring[XHCI64_RING_TRBS] __attribute__((aligned(4096)));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static struct xhci64_trb g_xhci_bulk_in_ring[XHCI64_RING_TRBS] __attribute__((aligned(4096)));
static struct xhci64_trb g_xhci_bulk_out_ring[XHCI64_RING_TRBS] __attribute__((aligned(4096)));
#endif
static struct xhci64_trb g_xhci_event_ring[XHCI64_EVENT_TRBS] __attribute__((aligned(4096)));
static struct xhci64_erst_entry g_xhci_erst[1] __attribute__((aligned(64)));
static u8 g_xhci_control_buffer[512] __attribute__((aligned(64)));
static u8 g_xhci_hid_descriptor[16] __attribute__((aligned(64)));
static u8 g_xhci_hid_report_descriptor[256] __attribute__((aligned(64)));
static u8 g_xhci_keyboard_report[8] __attribute__((aligned(64)));
static u8 g_xhci_mouse_report[XHCI64_MOUSE_REPORT_BUFFER_BYTES] __attribute__((aligned(64)));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u8 g_xhci_storage_cbw[64] __attribute__((aligned(64)));
static u8 g_xhci_storage_csw[64] __attribute__((aligned(64)));
static u8 g_xhci_storage_scratch[512] __attribute__((aligned(64)));
#endif
static u8 g_xhci_port_protocol[32];
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u8 g_xhci_port_probe_count[32];
static u32 g_xhci_descriptor_probe_port = 0u;
#endif
static struct xhci64_keyboard_endpoint g_xhci_keyboard_endpoint;
static struct xhci64_keyboard_endpoint g_xhci_mouse_endpoint;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static struct xhci64_storage_endpoint g_xhci_storage_endpoint;
#endif

static u64 xhci64_virtual_to_physical(const void *address)
{
    return paging64_kernel_physical_alias(address);
}

static volatile u32 *xhci64_reg(u32 offset)
{
    return (volatile u32 *)(u64)(XHCI64_MAP_VIRTUAL_BASE + (u64)offset);
}

static u32 xhci64_read32(u32 offset)
{
    return *xhci64_reg(offset);
}

static void xhci64_write32(u32 offset, u32 value)
{
    u32 page = offset / XHCI64_PAGE_BYTES;
    u64 virtual_address = XHCI64_MAP_VIRTUAL_BASE + (u64)offset;

    if (page >= XHCI64_MAP_PAGES)
    {
        g_xhci_error = (g_xhci_error == 0u) ? 100u : g_xhci_error;
        return;
    }

    if (paging64_kernel_mmio_write_window_open_virtual(virtual_address) == 0u)
    {
        g_xhci_error = (g_xhci_error == 0u) ? 101u : g_xhci_error;
        return;
    }

    *xhci64_reg(offset) = value;
    (void)xhci64_read32(offset);
    (void)paging64_kernel_mmio_write_window_close_virtual(virtual_address);
}

static void xhci64_write64(u32 offset, u64 value)
{
    xhci64_write32(offset, (u32)(value & 0xFFFFFFFFull));
    xhci64_write32(offset + 4u, (u32)(value >> 32));
}

static void xhci64_zero_memory(void *address, u32 byte_count)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static void xhci64_serial_write_dec(u32 value)
{
    char digits[10];
    u32 count = 0u;

    if (value == 0u)
    {
        serial_write_char('0');
        return;
    }

    while ((value != 0u) && (count < (u32)sizeof(digits)))
    {
        digits[count] = (char)('0' + (value % 10u));
        value /= 10u;
        ++count;
    }

    while (count != 0u)
    {
        --count;
        serial_write_char(digits[count]);
    }
}

static void xhci64_log_port_skip(u32 port_id, u32 code)
{
    ++g_xhci_port_error_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_last_skip_port = port_id;
    g_xhci_last_skip_code = code;
#endif
    if (g_xhci_error == 0u)
    {
        g_xhci_error = code;
    }

    serial_write_string("[x64] xHCI port ");
    xhci64_serial_write_dec(port_id);
    serial_write_string(" error ");
    xhci64_serial_write_dec(code);
    serial_write_string(" skipped\n");
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 xhci64_probe_port_index_valid(u32 port_id)
{
    return ((port_id >= 1u) && (port_id <= 32u)) ? 1u : 0u;
}

static u32 xhci64_probe_port_can_attempt(u32 port_id)
{
    u32 index;

    if (xhci64_probe_port_index_valid(port_id) == 0u)
    {
        return 0u;
    }

    index = port_id - 1u;
    if (g_xhci_port_probe_count[index] >= XHCI64_PORT_PROBE_LIMIT)
    {
        ++g_xhci_port_probe_retry_skips;
        return 0u;
    }

    ++g_xhci_port_probe_count[index];
    ++g_xhci_port_probe_attempts;
    return 1u;
}

static u32 xhci64_interface_can_collect_hid_candidate(u32 interface_class)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return ((interface_class == 0x03u) || (interface_class == 0xFFu)) ? 1u : 0u;
#else
    return (interface_class == 0x03u) ? 1u : 0u;
#endif
}

static void xhci64_note_hid_interface_probe(u32 interface_class, u32 interface_subclass, u32 interface_protocol)
{
    if ((interface_class == 0x03u) && (g_xhci_first_hid_port == 0u))
    {
        g_xhci_first_hid_port = g_xhci_descriptor_probe_port;
        g_xhci_first_hid_interface_class = interface_class;
        g_xhci_first_hid_interface_subclass = interface_subclass;
        g_xhci_first_hid_interface_protocol = interface_protocol;
    }

    if ((interface_class == 0x03u)
        && (interface_protocol == 0x02u)
        && (g_xhci_first_mouse_candidate_port == 0u))
    {
        g_xhci_first_mouse_candidate_port = g_xhci_descriptor_probe_port;
        g_xhci_first_mouse_candidate_interface_class = interface_class;
        g_xhci_first_mouse_candidate_interface_subclass = interface_subclass;
        g_xhci_first_mouse_candidate_interface_protocol = interface_protocol;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if ((interface_class == 0xFFu)
        && (g_xhci_first_mouse_candidate_port == 0u))
    {
        g_xhci_first_mouse_candidate_port = g_xhci_descriptor_probe_port;
        g_xhci_first_mouse_candidate_interface_class = interface_class;
        g_xhci_first_mouse_candidate_interface_subclass = interface_subclass;
        g_xhci_first_mouse_candidate_interface_protocol = interface_protocol;
    }
#endif
}

static void xhci64_note_mouse_endpoint_probe(u32 max_packet)
{
    if ((g_xhci_first_mouse_candidate_port != 0u)
        && (g_xhci_first_mouse_candidate_endpoint_mps == 0u))
    {
        g_xhci_first_mouse_candidate_endpoint_mps = max_packet;
    }
}

static void xhci64_clear_endpoint(struct xhci64_keyboard_endpoint *endpoint)
{
    if (endpoint == 0)
    {
        return;
    }

    endpoint->present = 0u;
    endpoint->slot_id = 0u;
    endpoint->dci = 0u;
    endpoint->max_packet = 0u;
    endpoint->interval = 0u;
    endpoint->interface_number = 0u;
    endpoint->interface_class = 0u;
    endpoint->interface_subclass = 0u;
    endpoint->interface_protocol = 0u;
    endpoint->report_length = 0u;
}

static u32 xhci64_collect_hid_candidates(
    const u8 *config,
    u32 length,
    struct xhci64_hid_candidate *candidates,
    u32 limit,
    u32 *configuration_value)
{
    u32 offset = 0u;
    u32 count = 0u;
    u32 current = 0xFFFFFFFFu;

    if ((config == 0) || (candidates == 0) || (configuration_value == 0) || (limit == 0u))
    {
        return 0u;
    }

    *configuration_value = config[5];
    while ((offset + 2u) <= length)
    {
        u32 descriptor_length = config[offset];
        u32 descriptor_type = config[offset + 1u];

        if ((descriptor_length == 0u) || ((offset + descriptor_length) > length))
        {
            break;
        }

        if ((descriptor_type == XHCI64_USB_DESC_INTERFACE) && (descriptor_length >= 9u))
        {
            u32 interface_class = config[offset + 5u];
            u32 interface_subclass = config[offset + 6u];
            u32 interface_protocol = config[offset + 7u];

            current = 0xFFFFFFFFu;
            g_xhci_last_interface_class = interface_class;
            g_xhci_last_interface_subclass = interface_subclass;
            g_xhci_last_interface_protocol = interface_protocol;
            xhci64_note_hid_interface_probe(interface_class, interface_subclass, interface_protocol);
            if ((xhci64_interface_can_collect_hid_candidate(interface_class) != 0u) && (count < limit))
            {
                current = count;
                xhci64_clear_endpoint(&candidates[current].endpoint);
                candidates[current].endpoint.interface_number = config[offset + 2u];
                candidates[current].endpoint.interface_class = interface_class;
                candidates[current].endpoint.interface_subclass = interface_subclass;
                candidates[current].endpoint.interface_protocol = interface_protocol;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                if (interface_class == 0xFFu)
                {
                    ++g_xhci_vendor_mouse_candidate_count;
                }
#endif
                ++count;
            }
        }
        else if ((descriptor_type == XHCI64_USB_DESC_HID)
            && (descriptor_length >= 9u)
            && (current != 0xFFFFFFFFu))
        {
            if (config[offset + 6u] == XHCI64_USB_DESC_REPORT)
            {
                candidates[current].endpoint.report_length = xhci64_usb16(&config[offset + 7u]);
                g_xhci_hid_report_read = 1u;
            }
        }
        else if ((descriptor_type == XHCI64_USB_DESC_ENDPOINT)
            && (descriptor_length >= 7u)
            && (current != 0xFFFFFFFFu))
        {
            u32 endpoint_address = config[offset + 2u];
            u32 attributes = config[offset + 3u];
            if (((endpoint_address & 0x80u) != 0u)
                && ((attributes & 0x3u) == 0x3u)
                && (candidates[current].endpoint.present == 0u))
            {
                u32 endpoint_number = endpoint_address & 0xFu;
                u32 max_packet = xhci64_usb16(&config[offset + 4u]) & 0x7FFu;

                g_xhci_last_endpoint_max_packet = max_packet;
                if (candidates[current].endpoint.interface_protocol == 0x02u)
                {
                    xhci64_note_mouse_endpoint_probe(max_packet);
                }
                candidates[current].endpoint.present = 1u;
                candidates[current].endpoint.dci = (endpoint_number * 2u) + 1u;
                candidates[current].endpoint.max_packet = (max_packet != 0u) ? max_packet : 8u;
                candidates[current].endpoint.interval = config[offset + 6u];
            }
        }

        offset += descriptor_length;
    }

    g_xhci_hid_interface_inventory += count;
    return count;
}

static u32 xhci64_select_hid_candidate(
    const struct xhci64_hid_candidate *candidates,
    u32 count,
    u32 interface_subclass,
    u32 interface_protocol,
    struct xhci64_keyboard_endpoint *endpoint)
{
    u32 index;

    if ((candidates == 0) || (endpoint == 0))
    {
        return 0u;
    }

    for (index = 0u; index < count; ++index)
    {
        const struct xhci64_keyboard_endpoint *candidate = &candidates[index].endpoint;
        if ((candidate->present != 0u)
            && (candidate->interface_class == 0x03u)
            && (candidate->interface_subclass == interface_subclass)
            && (candidate->interface_protocol == interface_protocol))
        {
            *endpoint = *candidate;
            return 1u;
        }
    }

    return 0u;
}

static u32 xhci64_select_generic_hid_candidate(
    const struct xhci64_hid_candidate *candidates,
    u32 count,
    struct xhci64_keyboard_endpoint *endpoint)
{
    u32 index;

    if ((candidates == 0) || (endpoint == 0))
    {
        return 0u;
    }

    for (index = 0u; index < count; ++index)
    {
        const struct xhci64_keyboard_endpoint *candidate = &candidates[index].endpoint;
        if ((candidate->present != 0u)
            && !((candidate->interface_subclass == 0x01u)
                && (candidate->interface_protocol == 0x01u)))
        {
            *endpoint = *candidate;
            return 1u;
        }
    }

    return 0u;
}
#endif

static void xhci64_log_hid_interface(
    const char *label,
    const struct xhci64_keyboard_endpoint *endpoint,
    u32 report_length)
{
    if (endpoint == 0)
    {
        return;
    }

    serial_write_string("[x64] xHCI HID ");
    serial_write_string(label);
    serial_write_string(" if ");
    xhci64_serial_write_dec(endpoint->interface_number);
    serial_write_string(" class ");
    xhci64_serial_write_dec(endpoint->interface_class);
    serial_write_string(" subclass ");
    xhci64_serial_write_dec(endpoint->interface_subclass);
    serial_write_string(" protocol ");
    xhci64_serial_write_dec(endpoint->interface_protocol);
    serial_write_string(" report-len ");
    xhci64_serial_write_dec(report_length);
    serial_write_string(" ep-dci ");
    xhci64_serial_write_dec(endpoint->dci);
    serial_write_string(" ep-mps ");
    xhci64_serial_write_dec(endpoint->max_packet);
    serial_write_string("\n");
}

static u32 xhci64_page_aligned(u64 value)
{
    return ((value & (u64)(XHCI64_PAGE_BYTES - 1u)) == 0ull) ? 1u : 0u;
}

static u32 xhci64_physical_base_valid(void)
{
    return ((g_xhci_flags & XHCI64_MMIO_FLAG_PRESENT) != 0u)
        && ((g_xhci_flags & XHCI64_MMIO_FLAG_MEMORY_BAR) != 0u)
        && ((g_xhci_flags & XHCI64_MMIO_FLAG_BASE_NONZERO) != 0u)
        && ((g_xhci_flags & XHCI64_MMIO_FLAG_PAGE_ALIGNED) != 0u);
}

static void xhci64_delay_ms(u32 milliseconds)
{
    u32 ms;

    for (ms = 0u; ms < milliseconds; ++ms)
    {
        u32 poll;
        for (poll = 0u; poll < XHCI64_DELAY_1MS_POLLS; ++poll)
        {
            __asm__ __volatile__("pause");
        }
    }
}

static u32 xhci64_trb_type(u32 control)
{
    return (control >> XHCI64_TRB_TYPE_SHIFT) & 0x3Fu;
}

static u32 xhci64_completion_code(const struct xhci64_event *event)
{
    return (event->status >> 24) & 0xFFu;
}

static void xhci64_set_trb(struct xhci64_trb *trb, u64 parameter, u32 status, u32 control)
{
    trb->dword[0] = (u32)(parameter & 0xFFFFFFFFull);
    trb->dword[1] = (u32)(parameter >> 32);
    trb->dword[2] = status;
    trb->dword[3] = control;
}

static void xhci64_reset_ring(struct xhci64_trb *ring, u32 count, u32 *enqueue, u32 *cycle)
{
    u32 index;

    for (index = 0u; index < count; ++index)
    {
        xhci64_set_trb(&ring[index], 0ull, 0u, 0u);
    }

    xhci64_set_trb(
        &ring[count - 1u],
        xhci64_virtual_to_physical(ring),
        0u,
        (XHCI64_TRB_TYPE_LINK << XHCI64_TRB_TYPE_SHIFT)
            | XHCI64_TRB_TOGGLE_CYCLE
            | XHCI64_TRB_CYCLE);
    *enqueue = 0u;
    *cycle = 1u;
}

static struct xhci64_trb *xhci64_ring_enqueue(
    struct xhci64_trb *ring,
    u32 count,
    u32 *enqueue,
    u32 *cycle,
    u64 parameter,
    u32 status,
    u32 control)
{
    struct xhci64_trb *trb;

    if (*enqueue >= (count - 1u))
    {
        ring[count - 1u].dword[3] =
            (XHCI64_TRB_TYPE_LINK << XHCI64_TRB_TYPE_SHIFT)
            | XHCI64_TRB_TOGGLE_CYCLE
            | (*cycle);
        *enqueue = 0u;
        *cycle ^= 1u;
    }

    trb = &ring[*enqueue];
    xhci64_set_trb(trb, parameter, status, control | (*cycle));
    ++(*enqueue);
    return trb;
}

static void xhci64_advance_event_ring(void)
{
    u64 erdp;

    ++g_xhci_event_dequeue;
    if (g_xhci_event_dequeue >= XHCI64_EVENT_TRBS)
    {
        g_xhci_event_dequeue = 0u;
        g_xhci_event_cycle ^= 1u;
    }

    erdp = xhci64_virtual_to_physical(&g_xhci_event_ring[g_xhci_event_dequeue]) | 0x8ull;
    xhci64_write64(g_xhci_runtime_offset + XHCI64_INTR0_OFFSET + XHCI64_INTR_ERDP, erdp);
}

static u32 xhci64_poll_event_bounded(
    u32 expected_type,
    u64 expected_parameter,
    u32 expected_slot,
    u32 expected_endpoint,
    u32 poll_limit,
    struct xhci64_event *out_event)
{
    u32 poll;

    for (poll = 0u; poll < poll_limit; ++poll)
    {
        struct xhci64_trb *trb = &g_xhci_event_ring[g_xhci_event_dequeue];
        u32 control = trb->dword[3];
        u32 type;
        u32 slot;
        u32 endpoint;
        struct xhci64_event event;

        if ((control & XHCI64_TRB_CYCLE) != g_xhci_event_cycle)
        {
            continue;
        }

        event.parameter = ((u64)trb->dword[1] << 32) | (u64)trb->dword[0];
        event.status = trb->dword[2];
        event.control = control;
        type = xhci64_trb_type(control);
        slot = (control >> 24) & 0xFFu;
        endpoint = (control >> 16) & 0x1Fu;
        if ((type == expected_type)
            && (expected_type == XHCI64_TRB_TYPE_TRANSFER_EVENT)
            && ((expected_parameter == 0ull) || (event.parameter == expected_parameter))
            && ((((expected_slot != 0u) && (slot != expected_slot)))
                || (((expected_endpoint != 0u) && (endpoint != expected_endpoint)))))
        {
            return 0u;
        }
        xhci64_advance_event_ring();

        if ((type == expected_type)
            && ((expected_parameter == 0ull) || (event.parameter == expected_parameter))
            && ((expected_slot == 0u) || (slot == expected_slot))
            && ((expected_endpoint == 0u) || (endpoint == expected_endpoint)))
        {
            if (out_event != 0)
            {
                *out_event = event;
            }
            return 1u;
        }
    }

    return 0u;
}

static u32 xhci64_poll_event(
    u32 expected_type,
    u64 expected_parameter,
    u32 expected_slot,
    u32 expected_endpoint,
    struct xhci64_event *out_event)
{
    return xhci64_poll_event_bounded(
        expected_type,
        expected_parameter,
        expected_slot,
        expected_endpoint,
        XHCI64_CONTROL_POLL_LIMIT,
        out_event);
}

static u32 xhci64_submit_command(u64 parameter, u32 control, struct xhci64_event *event_out)
{
    struct xhci64_trb *trb = xhci64_ring_enqueue(
        g_xhci_command_ring,
        XHCI64_RING_TRBS,
        &g_xhci_command_enqueue,
        &g_xhci_command_cycle,
        parameter,
        0u,
        control);
    u64 trb_physical = xhci64_virtual_to_physical(trb);

    xhci64_write32(g_xhci_doorbell_offset, 0u);
    return xhci64_poll_event(
        XHCI64_TRB_TYPE_COMMAND_COMPLETION,
        trb_physical,
        0u,
        0u,
        event_out);
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 xhci64_submit_command_bounded(
    u64 parameter,
    u32 control,
    u32 poll_limit,
    struct xhci64_event *event_out)
{
    struct xhci64_trb *trb = xhci64_ring_enqueue(
        g_xhci_command_ring,
        XHCI64_RING_TRBS,
        &g_xhci_command_enqueue,
        &g_xhci_command_cycle,
        parameter,
        0u,
        control);
    u64 trb_physical = xhci64_virtual_to_physical(trb);

    xhci64_write32(g_xhci_doorbell_offset, 0u);
    return xhci64_poll_event_bounded(
        XHCI64_TRB_TYPE_COMMAND_COMPLETION,
        trb_physical,
        0u,
        0u,
        poll_limit,
        event_out);
}
#endif

static u32 xhci64_context_dword(u8 *context, u32 index, u32 dword)
{
    u32 offset = (index * g_xhci_context_size) + (dword * 4u);
    (void)context;
    return offset;
}

static void xhci64_context_write(u8 *context, u32 index, u32 dword, u32 value)
{
    u32 offset = xhci64_context_dword(context, index, dword);
    context[offset] = (u8)(value & 0xFFu);
    context[offset + 1u] = (u8)((value >> 8) & 0xFFu);
    context[offset + 2u] = (u8)((value >> 16) & 0xFFu);
    context[offset + 3u] = (u8)((value >> 24) & 0xFFu);
}

static void xhci64_context_write64(u8 *context, u32 index, u32 dword, u64 value)
{
    xhci64_context_write(context, index, dword, (u32)(value & 0xFFFFFFFFull));
    xhci64_context_write(context, index, dword + 1u, (u32)(value >> 32));
}

static u32 xhci64_usb16(const u8 *bytes)
{
    return (u32)bytes[0] | ((u32)bytes[1] << 8);
}

static u32 xhci64_portsc_write_preserve(u32 portsc, u32 set_bits)
{
    return (portsc & XHCI64_PORTSC_PP) | set_bits;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 xhci64_portsc_pls(u32 portsc)
{
    return (portsc & XHCI64_PORTSC_PLS_MASK) >> XHCI64_PORTSC_PLS_SHIFT;
}

static u32 xhci64_read_portsc_snapshot(u32 port_id)
{
    u32 port_offset;

    if (port_id == 0u)
    {
        return 0u;
    }

    port_offset = g_xhci_cap_length + XHCI64_OP_PORTS + ((port_id - 1u) * XHCI64_PORT_REGISTER_BYTES);
    if ((port_offset + 4u) > g_xhci_span_hint)
    {
        return 0u;
    }

    return xhci64_read32(port_offset);
}
#endif

static u32 xhci64_initial_mps_for_speed(u32 speed)
{
    if (speed == 4u)
    {
        return 512u;
    }

    if (speed == 3u)
    {
        return 64u;
    }

    return XHCI64_DEFAULT_MPS;
}

static u32 xhci64_max_u32(u32 left, u32 right)
{
    return (left > right) ? left : right;
}

static void xhci64_prepare_slot_context(
    u8 *context,
    u32 context_entries,
    u32 root_port,
    u32 speed)
{
    xhci64_context_write(
        context,
        1u,
        0u,
        ((speed & 0xFu) << 20)
            | ((context_entries & 0x1Fu) << 27));
    xhci64_context_write(context, 1u, 1u, (root_port & 0xFFu) << 16);
    xhci64_context_write(context, 1u, 2u, 0u);
    xhci64_context_write(context, 1u, 3u, 0u);
}

static void xhci64_prepare_ep_context(
    u8 *context,
    u32 dci,
    u32 endpoint_type,
    u32 max_packet,
    u32 interval,
    u64 ring_physical,
    u32 average_length)
{
    u32 input_context_index = dci + 1u;

    xhci64_context_write(context, input_context_index, 0u, (interval & 0xFFu) << 16);
    xhci64_context_write(
        context,
        input_context_index,
        1u,
        (3u << 1)
            | ((endpoint_type & 0x7u) << 3)
            | ((max_packet & 0xFFFFu) << 16));
    xhci64_context_write64(context, input_context_index, 2u, ring_physical | 1ull);
    xhci64_context_write(
        context,
        input_context_index,
        4u,
        (average_length & 0xFFFFu) | ((max_packet & 0xFFFFu) << 16));
}

static void xhci64_stage_private_rings(void)
{
    u64 dcbaa_physical = xhci64_virtual_to_physical(g_xhci_dcbaa);
    u64 ring_physical = xhci64_virtual_to_physical(g_xhci_command_ring);
    u64 event_physical = xhci64_virtual_to_physical(g_xhci_event_ring);

    xhci64_zero_memory(g_xhci_dcbaa, sizeof(g_xhci_dcbaa));
    xhci64_zero_memory(g_xhci_scratchpad_array, sizeof(g_xhci_scratchpad_array));
    xhci64_zero_memory(g_xhci_scratchpads, sizeof(g_xhci_scratchpads));
    xhci64_zero_memory(g_xhci_input_context, sizeof(g_xhci_input_context));
    xhci64_zero_memory(g_xhci_device_contexts, sizeof(g_xhci_device_contexts));
    xhci64_zero_memory(g_xhci_event_ring, sizeof(g_xhci_event_ring));
    xhci64_zero_memory(g_xhci_erst, sizeof(g_xhci_erst));
    xhci64_zero_memory(g_xhci_control_buffer, sizeof(g_xhci_control_buffer));
    xhci64_zero_memory(g_xhci_hid_descriptor, sizeof(g_xhci_hid_descriptor));
    xhci64_zero_memory(g_xhci_hid_report_descriptor, sizeof(g_xhci_hid_report_descriptor));
    xhci64_zero_memory(g_xhci_keyboard_report, sizeof(g_xhci_keyboard_report));
    xhci64_zero_memory(g_xhci_mouse_report, sizeof(g_xhci_mouse_report));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    xhci64_zero_memory(g_xhci_storage_cbw, sizeof(g_xhci_storage_cbw));
    xhci64_zero_memory(g_xhci_storage_csw, sizeof(g_xhci_storage_csw));
    xhci64_zero_memory(g_xhci_storage_scratch, sizeof(g_xhci_storage_scratch));
#endif
    xhci64_reset_ring(g_xhci_command_ring, XHCI64_RING_TRBS, &g_xhci_command_enqueue, &g_xhci_command_cycle);
    xhci64_reset_ring(g_xhci_ep0_ring, XHCI64_RING_TRBS, &g_xhci_ep0_enqueue, &g_xhci_ep0_cycle);
    xhci64_reset_ring(g_xhci_interrupt_ring, XHCI64_RING_TRBS, &g_xhci_intr_enqueue, &g_xhci_intr_cycle);
    xhci64_reset_ring(g_xhci_mouse_interrupt_ring, XHCI64_RING_TRBS, &g_xhci_mouse_intr_enqueue, &g_xhci_mouse_intr_cycle);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    xhci64_reset_ring(g_xhci_bulk_in_ring, XHCI64_RING_TRBS, &g_xhci_bulk_in_enqueue, &g_xhci_bulk_in_cycle);
    xhci64_reset_ring(g_xhci_bulk_out_ring, XHCI64_RING_TRBS, &g_xhci_bulk_out_enqueue, &g_xhci_bulk_out_cycle);
#endif
    g_xhci_event_dequeue = 0u;
    g_xhci_event_cycle = 1u;
    g_xhci_intr_pending = 0u;
    g_xhci_mouse_intr_pending = 0u;
    g_xhci_mouse_report_size = 8u;
    g_xhci_keyboard_endpoint.present = 0u;
    g_xhci_mouse_endpoint.present = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    xhci64_zero_memory(&g_xhci_storage_endpoint, sizeof(g_xhci_storage_endpoint));
    g_xhci_usb_storage_present = 0u;
    g_xhci_usb_storage_ready = 0u;
    g_xhci_usb_storage_error = 0u;
    g_xhci_usb_storage_block_bytes = 0u;
    g_xhci_usb_storage_last_lba = 0u;
#endif

    if (xhci64_page_aligned(dcbaa_physical) != 0u)
    {
        g_xhci_dcbaa_staged = 1u;
    }

    if (xhci64_page_aligned(ring_physical) != 0u)
    {
        g_xhci_command_ring_staged = 1u;
    }

    if (xhci64_page_aligned(event_physical) != 0u)
    {
        g_xhci_event_ring_staged = 1u;
    }
}

static u32 xhci64_stage_scratchpads(u32 hcs_params2)
{
    u32 scratchpad_count =
        ((hcs_params2 >> 27) & 0x1Fu) | (((hcs_params2 >> 21) & 0x1Fu) << 5);
    u32 index;

    if (scratchpad_count == 0u)
    {
        return 1u;
    }

    if (scratchpad_count > XHCI64_MAX_SCRATCHPADS)
    {
        g_xhci_error = 4u;
        return 0u;
    }

    for (index = 0u; index < scratchpad_count; ++index)
    {
        g_xhci_scratchpad_array[index] = xhci64_virtual_to_physical(g_xhci_scratchpads[index]);
    }

    g_xhci_dcbaa[0] = xhci64_virtual_to_physical(g_xhci_scratchpad_array);
    return 1u;
}

static void xhci64_mark_protocol_ports(u32 start_port, u32 port_count, u8 protocol)
{
    u32 index;

    if ((start_port == 0u) || (port_count == 0u))
    {
        return;
    }

    for (index = 0u; index < port_count; ++index)
    {
        u32 port_id = start_port + index;
        if ((port_id >= 1u) && (port_id <= 32u))
        {
            g_xhci_port_protocol[port_id - 1u] = protocol;
        }
    }
}

static void xhci64_scan_extended_capabilities(u32 hcc_params1)
{
    u32 offset = ((hcc_params1 >> 16) & 0xFFFFu) << 2;
    u32 step;

    g_xhci_extcaps_scanned = 0u;
    g_xhci_legacy_cap_found = 0u;
    g_xhci_legacy_handoff = 0u;
    g_xhci_legacy_bios_owned_before = 0u;
    g_xhci_legacy_bios_owned_clear = 0u;
    g_xhci_legacy_os_owned = 0u;
    g_xhci_protocol_caps = 0u;
    g_xhci_usb2_ports = 0u;
    g_xhci_usb3_ports = 0u;
    g_xhci_prefer_usb2 = 0u;
    g_xhci_intel_cap_found = 0u;
    g_xhci_intel_workaround = 0u;
    xhci64_zero_memory(g_xhci_port_protocol, sizeof(g_xhci_port_protocol));

    if (offset == 0u)
    {
        g_xhci_legacy_handoff = 1u;
        return;
    }

    for (step = 0u; step < XHCI64_EXTCAP_MAX_STEPS; ++step)
    {
        u32 cap;
        u32 cap_id;
        u32 next;

        if ((offset + 4u) > g_xhci_span_hint)
        {
            break;
        }

        cap = xhci64_read32(offset);
        cap_id = cap & XHCI64_EXTCAP_ID_MASK;
        next = (cap & XHCI64_EXTCAP_NEXT_MASK) >> XHCI64_EXTCAP_NEXT_SHIFT;
        ++g_xhci_extcaps_scanned;

        if (cap_id == XHCI64_EXTCAP_USB_LEGACY)
        {
            u32 poll;
            u32 value = cap | XHCI64_USBLEGSUP_OS_OWNED;

            g_xhci_legacy_cap_found = 1u;
            g_xhci_legacy_bios_owned_before =
                ((cap & XHCI64_USBLEGSUP_BIOS_OWNED) != 0u) ? 1u : 0u;
            xhci64_write32(offset, value);

            for (poll = 0u; poll < XHCI64_LEGACY_HANDOFF_POLL_LIMIT; ++poll)
            {
                value = xhci64_read32(offset);
                if ((value & XHCI64_USBLEGSUP_BIOS_OWNED) == 0u)
                {
                    break;
                }
            }

            g_xhci_legacy_os_owned =
                ((xhci64_read32(offset) & XHCI64_USBLEGSUP_OS_OWNED) != 0u) ? 1u : 0u;
            g_xhci_legacy_bios_owned_clear =
                ((xhci64_read32(offset) & XHCI64_USBLEGSUP_BIOS_OWNED) == 0u) ? 1u : 0u;
            g_xhci_legacy_handoff =
                ((g_xhci_legacy_os_owned != 0u) && (g_xhci_legacy_bios_owned_clear != 0u))
                    ? 1u
                    : 0u;
        }
        else if ((cap_id == XHCI64_EXTCAP_SUPPORTED_PROTOCOL)
            && ((offset + 12u) <= g_xhci_span_hint))
        {
            u32 revision = cap;
            u32 name = xhci64_read32(offset + 4u);
            u32 port_info = xhci64_read32(offset + 8u);
            u32 major = (revision >> 24) & 0xFFu;
            u32 start_port = port_info & 0xFFu;
            u32 port_count = (port_info >> 8) & 0xFFu;

            if (name == XHCI64_EXTCAP_USB_NAME)
            {
                ++g_xhci_protocol_caps;
                if (major == 2u)
                {
                    g_xhci_usb2_ports += port_count;
                    xhci64_mark_protocol_ports(start_port, port_count, 2u);
                }
                else if (major == 3u)
                {
                    g_xhci_usb3_ports += port_count;
                    xhci64_mark_protocol_ports(start_port, port_count, 3u);
                }
            }
        }
        else if (cap_id == XHCI64_EXTCAP_INTEL_VENDOR)
        {
            g_xhci_intel_cap_found = 1u;
            if ((g_xhci_vendor_device & 0xFFFFu) == 0x8086u)
            {
                g_xhci_intel_workaround = 1u;
            }
        }

        if (next == 0u)
        {
            break;
        }

        offset += next << 2;
    }

    if (g_xhci_legacy_cap_found == 0u)
    {
        g_xhci_legacy_handoff = 1u;
        g_xhci_legacy_bios_owned_clear = 1u;
    }

    g_xhci_prefer_usb2 = (g_xhci_usb2_ports != 0u) ? 1u : 0u;
}

static void xhci64_scan_ports(void)
{
    u32 port_count;
    u32 index;
    u32 connected = 0u;

    port_count = g_xhci_hcs_ports;
    if (port_count > 32u)
    {
        port_count = 32u;
    }

    for (index = 0u; index < port_count; ++index)
    {
        u32 port_offset = g_xhci_cap_length + XHCI64_OP_PORTS + (index * XHCI64_PORT_REGISTER_BYTES);
        u32 portsc;

        if ((port_offset + 4u) > g_xhci_span_hint)
        {
            break;
        }

        portsc = xhci64_read32(port_offset);
        ++g_xhci_ports_scanned;
        if ((portsc & XHCI64_PORTSC_CCS) != 0u)
        {
            ++connected;
        }
    }

    g_xhci_connected_ports = connected;
}

static u32 xhci64_wait_status_clear(u32 mask)
{
    u32 poll;

    for (poll = 0u; poll < XHCI64_CONTROL_POLL_LIMIT; ++poll)
    {
        if ((xhci64_read32(g_xhci_cap_length + XHCI64_OP_USBSTS) & mask) == 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 xhci64_wait_status_set(u32 mask)
{
    u32 poll;

    for (poll = 0u; poll < XHCI64_CONTROL_POLL_LIMIT; ++poll)
    {
        if ((xhci64_read32(g_xhci_cap_length + XHCI64_OP_USBSTS) & mask) == mask)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 xhci64_reset_and_run_controller(u32 hcs_params2)
{
    u32 command;
    u32 max_slots_enabled = g_xhci_max_slots;
    u64 command_ring_physical = xhci64_virtual_to_physical(g_xhci_command_ring);
    u64 event_ring_physical = xhci64_virtual_to_physical(g_xhci_event_ring);
    u64 erst_physical = xhci64_virtual_to_physical(g_xhci_erst);

    if (max_slots_enabled > XHCI64_MAX_SLOTS)
    {
        max_slots_enabled = XHCI64_MAX_SLOTS;
    }

    if (max_slots_enabled == 0u)
    {
        g_xhci_error = 5u;
        return 0u;
    }

    command = xhci64_read32(g_xhci_cap_length + XHCI64_OP_USBCMD);
    if ((command & XHCI64_USBCMD_RS) != 0u)
    {
        xhci64_write32(g_xhci_cap_length + XHCI64_OP_USBCMD, command & ~XHCI64_USBCMD_RS);
        if (xhci64_wait_status_set(XHCI64_USBSTS_HCH) == 0u)
        {
            g_xhci_error = 6u;
            return 0u;
        }
    }

    xhci64_write32(g_xhci_cap_length + XHCI64_OP_USBCMD, XHCI64_USBCMD_HCRST);
    if (xhci64_wait_status_clear(XHCI64_USBSTS_CNR) == 0u)
    {
        g_xhci_error = 7u;
        return 0u;
    }

    command = xhci64_read32(g_xhci_cap_length + XHCI64_OP_USBCMD);
    if ((command & XHCI64_USBCMD_HCRST) != 0u)
    {
        u32 poll;
        for (poll = 0u; poll < XHCI64_CONTROL_POLL_LIMIT; ++poll)
        {
            if ((xhci64_read32(g_xhci_cap_length + XHCI64_OP_USBCMD) & XHCI64_USBCMD_HCRST) == 0u)
            {
                break;
            }
        }
        if ((xhci64_read32(g_xhci_cap_length + XHCI64_OP_USBCMD) & XHCI64_USBCMD_HCRST) != 0u)
        {
            g_xhci_error = 8u;
            return 0u;
        }
    }

    g_xhci_controller_reset = 1u;
    xhci64_stage_private_rings();
    if (xhci64_stage_scratchpads(hcs_params2) == 0u)
    {
        return 0u;
    }

    g_xhci_erst[0].base = event_ring_physical;
    g_xhci_erst[0].size = XHCI64_EVENT_TRBS;
    g_xhci_erst[0].reserved = 0u;

    xhci64_write32(g_xhci_cap_length + XHCI64_OP_CONFIG, max_slots_enabled);
    xhci64_write64(g_xhci_cap_length + XHCI64_OP_DCBAAP, xhci64_virtual_to_physical(g_xhci_dcbaa));
    xhci64_write64(g_xhci_cap_length + XHCI64_OP_CRCR, command_ring_physical | 1ull);
    xhci64_write32(g_xhci_runtime_offset + XHCI64_INTR0_OFFSET + XHCI64_INTR_ERSTSZ, 1u);
    xhci64_write64(g_xhci_runtime_offset + XHCI64_INTR0_OFFSET + XHCI64_INTR_ERSTBA, erst_physical);
    xhci64_write64(g_xhci_runtime_offset + XHCI64_INTR0_OFFSET + XHCI64_INTR_ERDP, event_ring_physical | 0x8ull);
    xhci64_write32(g_xhci_runtime_offset + XHCI64_INTR0_OFFSET + XHCI64_INTR_IMAN, 0x2u);
    xhci64_write32(g_xhci_cap_length + XHCI64_OP_USBSTS, 0xFFFFFFFFu);
    xhci64_write32(g_xhci_cap_length + XHCI64_OP_USBCMD, XHCI64_USBCMD_RS | XHCI64_USBCMD_INTE);

    if (xhci64_wait_status_clear(XHCI64_USBSTS_HCH) == 0u)
    {
        g_xhci_error = 9u;
        return 0u;
    }

    g_xhci_controller_running = 1u;
    return 1u;
}

static u32 xhci64_reset_port(u32 port_id, u32 *speed_out)
{
    u32 port_offset = g_xhci_cap_length + XHCI64_OP_PORTS + ((port_id - 1u) * XHCI64_PORT_REGISTER_BYTES);
    u32 portsc = xhci64_read32(port_offset);
    u32 poll;
    u32 retry;

    if ((port_id == 0u) || ((port_offset + 4u) > g_xhci_span_hint))
    {
        return 0u;
    }

    portsc = xhci64_read32(port_offset);
    for (retry = 0u; retry < XHCI64_CONNECTION_RETRIES; ++retry)
    {
        portsc = xhci64_read32(port_offset);
        if ((portsc & XHCI64_PORTSC_CCS) != 0u)
        {
            break;
        }
        xhci64_delay_ms(1u);
    }

    if ((portsc & XHCI64_PORTSC_CCS) == 0u)
    {
        return 0u;
    }

    if ((portsc & XHCI64_PORTSC_PED) == 0u)
    {
        xhci64_write32(
            port_offset,
            xhci64_portsc_write_preserve(portsc, XHCI64_PORTSC_PP | XHCI64_PORTSC_PR));
        for (poll = 0u; poll < XHCI64_PORT_RESET_POLL_LIMIT; ++poll)
        {
            portsc = xhci64_read32(port_offset);
            if ((portsc & XHCI64_PORTSC_PR) == 0u)
            {
                break;
            }
        }
        xhci64_delay_ms(XHCI64_PORT_RESET_WAIT_MS);
    }
    xhci64_delay_ms(XHCI64_DEVICE_SETTLE_MS);

    portsc = xhci64_read32(port_offset);
    xhci64_write32(
        port_offset,
        xhci64_portsc_write_preserve(portsc, portsc & XHCI64_PORTSC_CHANGE_MASK));
    for (retry = 0u; retry < XHCI64_CONNECTION_RETRIES; ++retry)
    {
        if ((portsc & XHCI64_PORTSC_CCS) != 0u)
        {
            break;
        }
        xhci64_delay_ms(1u);
        portsc = xhci64_read32(port_offset);
    }
    if ((portsc & XHCI64_PORTSC_CCS) == 0u)
    {
        return 0u;
    }

    if (speed_out != 0)
    {
        *speed_out = (portsc >> XHCI64_PORTSC_SPEED_SHIFT) & 0xFu;
    }
    return 1u;
}

static u32 xhci64_port_connected(u32 port_id)
{
    u32 port_offset;

    if (port_id == 0u)
    {
        return 0u;
    }

    port_offset = g_xhci_cap_length + XHCI64_OP_PORTS + ((port_id - 1u) * XHCI64_PORT_REGISTER_BYTES);
    if ((port_offset + 4u) > g_xhci_span_hint)
    {
        return 0u;
    }

    return ((xhci64_read32(port_offset) & XHCI64_PORTSC_CCS) != 0u) ? 1u : 0u;
}

static u32 xhci64_enable_slot(u32 *slot_out)
{
    struct xhci64_event event;
    u32 completion;
    u32 slot;

    if (xhci64_submit_command(
            0ull,
            XHCI64_TRB_TYPE_ENABLE_SLOT << XHCI64_TRB_TYPE_SHIFT,
            &event) == 0u)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_xhci_last_enable_slot_completion = 0u;
        g_xhci_last_enable_slot_id = 0u;
#endif
        return 0u;
    }

    completion = xhci64_completion_code(&event);
    slot = (event.control >> 24) & 0xFFu;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_last_enable_slot_completion = completion;
    g_xhci_last_enable_slot_id = slot;
#endif
    if ((completion != XHCI64_COMPLETION_SUCCESS)
        || (slot == 0u)
        || (slot > XHCI64_MAX_SLOTS))
    {
        return 0u;
    }

    *slot_out = slot;
    g_xhci_slot_enabled = 1u;
    return 1u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 xhci64_disable_slot(u32 slot_id)
{
    struct xhci64_event event;

    if ((slot_id == 0u) || (slot_id > XHCI64_MAX_SLOTS))
    {
        return 0u;
    }

    g_xhci_last_disable_slot_id = slot_id;
    g_xhci_last_disable_slot_completion = 0u;
    if (xhci64_submit_command_bounded(
            0ull,
            (XHCI64_TRB_TYPE_DISABLE_SLOT << XHCI64_TRB_TYPE_SHIFT) | (slot_id << 24),
            XHCI64_DISABLE_SLOT_POLL_LIMIT,
            &event) == 0u)
    {
        ++g_xhci_slot_disable_failures;
        return 0u;
    }

    g_xhci_last_disable_slot_completion = xhci64_completion_code(&event);
    if (g_xhci_last_disable_slot_completion != XHCI64_COMPLETION_SUCCESS)
    {
        ++g_xhci_slot_disable_failures;
        return 0u;
    }

    if (slot_id < (XHCI64_MAX_SLOTS + 1u))
    {
        g_xhci_dcbaa[slot_id] = 0ull;
    }

    ++g_xhci_slots_disabled;
    return 1u;
}

#define XHCI64_CLEANUP_FAILED_SLOT(slot_id) \
    do { \
        if ((slot_id) != 0u) \
        { \
            (void)xhci64_disable_slot((slot_id)); \
        } \
    } while (0)
#else
#define XHCI64_CLEANUP_FAILED_SLOT(slot_id)
#endif

static u32 xhci64_address_device(u32 *slot_id_inout, u32 port_id, u32 *speed_inout)
{
    struct xhci64_event event;
    u32 slot_id;
    u32 speed;
    u32 max_packet;
    u32 attempt;

    if ((slot_id_inout == 0) || (speed_inout == 0))
    {
        return 0u;
    }

    slot_id = *slot_id_inout;
    speed = *speed_inout;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_last_address_slot = slot_id;
    g_xhci_last_address_port = port_id;
    g_xhci_last_address_speed = speed;
    g_xhci_last_address_completion = 0u;
    g_xhci_last_address_event_dw0 = 0u;
    g_xhci_last_address_event_dw1 = 0u;
    g_xhci_last_address_event_dw2 = 0u;
    g_xhci_last_address_event_dw3 = 0u;
    g_xhci_pre_address_portsc = 0u;
    g_xhci_pre_address_portsc_pls = 0u;
    g_xhci_post_address_portsc = 0u;
    g_xhci_post_address_portsc_pls = 0u;
#endif

    for (attempt = 0u; attempt <= XHCI64_ADDRESS_RECOVERY_RETRIES; ++attempt)
    {
        max_packet = xhci64_initial_mps_for_speed(speed);

        xhci64_zero_memory(g_xhci_input_context, sizeof(g_xhci_input_context));
        xhci64_zero_memory(g_xhci_device_contexts[slot_id], XHCI64_PAGE_BYTES);
        xhci64_reset_ring(g_xhci_ep0_ring, XHCI64_RING_TRBS, &g_xhci_ep0_enqueue, &g_xhci_ep0_cycle);
        g_xhci_dcbaa[slot_id] = xhci64_virtual_to_physical(g_xhci_device_contexts[slot_id]);
        xhci64_context_write(g_xhci_input_context, 0u, 1u, 0x3u);
        xhci64_prepare_slot_context(g_xhci_input_context, XHCI64_DCI_EP0, port_id, speed);
        xhci64_prepare_ep_context(
            g_xhci_input_context,
            XHCI64_DCI_EP0,
            XHCI64_EP_TYPE_CONTROL,
            max_packet,
            0u,
            xhci64_virtual_to_physical(g_xhci_ep0_ring),
            8u);

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_xhci_last_address_slot = slot_id;
        g_xhci_last_address_speed = speed;
        g_xhci_pre_address_portsc = xhci64_read_portsc_snapshot(port_id);
        g_xhci_pre_address_portsc_pls = xhci64_portsc_pls(g_xhci_pre_address_portsc);
#endif

        if (xhci64_submit_command(
                xhci64_virtual_to_physical(g_xhci_input_context),
                (XHCI64_TRB_TYPE_ADDRESS_DEVICE << XHCI64_TRB_TYPE_SHIFT) | (slot_id << 24),
                &event) == 0u)
        {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            g_xhci_post_address_portsc = xhci64_read_portsc_snapshot(port_id);
            g_xhci_post_address_portsc_pls = xhci64_portsc_pls(g_xhci_post_address_portsc);
            ++g_xhci_address_failure_count;
#endif
            return 0u;
        }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_xhci_post_address_portsc = xhci64_read_portsc_snapshot(port_id);
        g_xhci_post_address_portsc_pls = xhci64_portsc_pls(g_xhci_post_address_portsc);
        g_xhci_last_address_completion = xhci64_completion_code(&event);
        if (g_xhci_last_address_completion == XHCI64_COMPLETION_SUCCESS)
#else
        if (xhci64_completion_code(&event) == XHCI64_COMPLETION_SUCCESS)
#endif
        {
            *slot_id_inout = slot_id;
            *speed_inout = speed;
            g_xhci_addressed = 1u;
            return 1u;
        }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_xhci_last_address_event_dw0 = (u32)(event.parameter & 0xFFFFFFFFull);
        g_xhci_last_address_event_dw1 = (u32)(event.parameter >> 32);
        g_xhci_last_address_event_dw2 = event.status;
        g_xhci_last_address_event_dw3 = event.control;
        ++g_xhci_address_failure_count;

        if ((g_xhci_last_address_completion == XHCI64_COMPLETION_USB_TRANSACTION_ERROR)
            && (attempt < XHCI64_ADDRESS_RECOVERY_RETRIES))
        {
            (void)xhci64_disable_slot(slot_id);
            if (xhci64_reset_port(port_id, &speed) == 0u)
            {
                return 0u;
            }
            if (xhci64_enable_slot(&slot_id) == 0u)
            {
                return 0u;
            }
            continue;
        }
#endif

        return 0u;
    }

    return 0u;
}

static u32 xhci64_control_transfer(
    u32 slot_id,
    u8 request_type,
    u8 request,
    u16 value,
    u16 index,
    u16 length,
    void *buffer,
    u32 direction_in)
{
    struct xhci64_trb *status_trb;
    struct xhci64_event event;
    u32 setup0 = (u32)request_type | ((u32)request << 8) | ((u32)value << 16);
    u32 setup1 = (u32)index | ((u32)length << 16);
    u32 setup_control = (XHCI64_TRB_TYPE_SETUP_STAGE << XHCI64_TRB_TYPE_SHIFT) | XHCI64_TRB_IDT;
    u32 completion;

    if ((length != 0u) && (direction_in != 0u))
    {
        setup_control |= XHCI64_TRB_TRT_IN;
    }

    xhci64_ring_enqueue(
        g_xhci_ep0_ring,
        XHCI64_RING_TRBS,
        &g_xhci_ep0_enqueue,
        &g_xhci_ep0_cycle,
        ((u64)setup1 << 32) | (u64)setup0,
        8u,
        setup_control);

    if (length != 0u)
    {
        u32 data_control = XHCI64_TRB_TYPE_DATA_STAGE << XHCI64_TRB_TYPE_SHIFT;
        if (direction_in != 0u)
        {
            data_control |= XHCI64_TRB_DIR;
        }

        xhci64_ring_enqueue(
            g_xhci_ep0_ring,
            XHCI64_RING_TRBS,
            &g_xhci_ep0_enqueue,
            &g_xhci_ep0_cycle,
            xhci64_virtual_to_physical(buffer),
            (u32)length,
            data_control);
    }

    status_trb = xhci64_ring_enqueue(
        g_xhci_ep0_ring,
        XHCI64_RING_TRBS,
        &g_xhci_ep0_enqueue,
        &g_xhci_ep0_cycle,
        0ull,
        0u,
        (XHCI64_TRB_TYPE_STATUS_STAGE << XHCI64_TRB_TYPE_SHIFT)
            | XHCI64_TRB_IOC
            | ((direction_in != 0u) ? 0u : XHCI64_TRB_DIR));

    xhci64_write32(g_xhci_doorbell_offset + (slot_id * 4u), XHCI64_DCI_EP0);
    if (xhci64_poll_event(
            XHCI64_TRB_TYPE_TRANSFER_EVENT,
            xhci64_virtual_to_physical(status_trb),
            slot_id,
            XHCI64_DCI_EP0,
            &event) == 0u)
    {
        return 0u;
    }

    completion = xhci64_completion_code(&event);
    return ((completion == XHCI64_COMPLETION_SUCCESS)
        || (completion == XHCI64_COMPLETION_SHORT_PACKET)) ? 1u : 0u;
}

static u32 xhci64_get_descriptor(
    u32 slot_id,
    u32 descriptor_value,
    u32 descriptor_index,
    void *buffer,
    u32 length)
{
    xhci64_zero_memory(buffer, length);
    return xhci64_control_transfer(
        slot_id,
        0x80u,
        XHCI64_USB_REQ_GET_DESCRIPTOR,
        (u16)descriptor_value,
        (u16)descriptor_index,
        (u16)length,
        buffer,
        1u);
}

static u32 xhci64_set_configuration(u32 slot_id, u32 configuration_value)
{
    return xhci64_control_transfer(
        slot_id,
        0x00u,
        XHCI64_USB_REQ_SET_CONFIGURATION,
        (u16)configuration_value,
        0u,
        0u,
        0,
        0u);
}

static u32 xhci64_set_boot_protocol(u32 slot_id, u32 interface_number)
{
    return xhci64_control_transfer(
        slot_id,
        0x21u,
        XHCI64_USB_REQ_SET_PROTOCOL,
        0u,
        (u16)interface_number,
        0u,
        0,
        0u);
}

static u32 xhci64_read_hid_descriptor(
    u32 slot_id,
    u32 interface_number,
    u32 *report_length_out)
{
    if (report_length_out != 0)
    {
        *report_length_out = 0u;
    }

    xhci64_zero_memory(g_xhci_hid_descriptor, sizeof(g_xhci_hid_descriptor));
    if (xhci64_control_transfer(
            slot_id,
            0x81u,
            XHCI64_USB_REQ_GET_DESCRIPTOR,
            (u16)(XHCI64_USB_DESC_HID << 8),
            (u16)interface_number,
            9u,
            g_xhci_hid_descriptor,
            1u) == 0u)
    {
        return 0u;
    }

    if ((g_xhci_hid_descriptor[1] != XHCI64_USB_DESC_HID)
        || (g_xhci_hid_descriptor[6] != XHCI64_USB_DESC_REPORT))
    {
        return 0u;
    }

    if (report_length_out != 0)
    {
        *report_length_out = xhci64_usb16(&g_xhci_hid_descriptor[7]);
    }

    g_xhci_hid_report_read = 1u;
    return 1u;
}

static u32 xhci64_read_hid_report_descriptor(
    u32 slot_id,
    u32 interface_number,
    u32 report_length)
{
    if (report_length > sizeof(g_xhci_hid_report_descriptor))
    {
        report_length = sizeof(g_xhci_hid_report_descriptor);
    }

    if (report_length == 0u)
    {
        return 0u;
    }

    xhci64_zero_memory(g_xhci_hid_report_descriptor, sizeof(g_xhci_hid_report_descriptor));
    if (xhci64_control_transfer(
            slot_id,
            0x81u,
            XHCI64_USB_REQ_GET_DESCRIPTOR,
            (u16)(XHCI64_USB_DESC_REPORT << 8),
            (u16)interface_number,
            (u16)report_length,
            g_xhci_hid_report_descriptor,
            1u) == 0u)
    {
        return 0u;
    }

    g_xhci_hid_report_read = 1u;
    return 1u;
}

static u32 xhci64_hid_report_has_usage(u32 report_length, u8 usage)
{
    u32 index;
    u32 usage_page = 0u;

    if (report_length > sizeof(g_xhci_hid_report_descriptor))
    {
        report_length = sizeof(g_xhci_hid_report_descriptor);
    }

    for (index = 0u; (index + 1u) < report_length; ++index)
    {
        if (g_xhci_hid_report_descriptor[index] == 0x05u)
        {
            usage_page = g_xhci_hid_report_descriptor[index + 1u];
            ++index;
            continue;
        }

        if ((usage_page == 0x01u)
            && (g_xhci_hid_report_descriptor[index] == 0x09u)
            && (g_xhci_hid_report_descriptor[index + 1u] == usage))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 xhci64_hid_report_has_report_id(u32 report_length)
{
    u32 index;

    if (report_length > sizeof(g_xhci_hid_report_descriptor))
    {
        report_length = sizeof(g_xhci_hid_report_descriptor);
    }

    for (index = 0u; (index + 1u) < report_length; ++index)
    {
        if (g_xhci_hid_report_descriptor[index] == 0x85u)
        {
            return 1u;
        }
    }

    return 0u;
}

static void xhci64_configure_input_mouse_layout(u32 report_length)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (report_length > sizeof(g_xhci_hid_report_descriptor))
    {
        report_length = sizeof(g_xhci_hid_report_descriptor);
    }

    if (report_length == 0u)
    {
        return;
    }

    (void)input64_configure_usb_hid_mouse_report_layout(
        g_xhci_hid_report_descriptor,
        report_length);
#else
    (void)report_length;
#endif
}

static u32 xhci64_mouse_transfer_size_from_endpoint(const struct xhci64_keyboard_endpoint *endpoint)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 size;
    u32 layout_size;

    if (endpoint == 0)
    {
        return 8u;
    }

    size = endpoint->max_packet;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    layout_size = input64_usb_hid_mouse_layout_report_bytes();
    if ((layout_size != 0u) && (layout_size > size))
    {
        size = layout_size;
    }
#endif

    if (size < XHCI64_MOUSE_MIN_PROBE_PACKET_BYTES)
    {
        size = XHCI64_MOUSE_MIN_PROBE_PACKET_BYTES;
    }

    if (size > XHCI64_MOUSE_MAX_PROBE_PACKET_BYTES)
    {
        size = XHCI64_MOUSE_MAX_PROBE_PACKET_BYTES;
    }

    return size;
#else
    u32 size;

    if (endpoint == 0)
    {
        return 8u;
    }

    size = endpoint->max_packet;
    if ((size == 3u) || (size == 4u) || (size == 5u) || (size == 8u))
    {
        return size;
    }

    if ((size > 3u) && (size < 8u))
    {
        return size;
    }

    return 8u;
#endif
}

static u32 xhci64_endpoint_can_probe_as_mouse(const struct xhci64_keyboard_endpoint *endpoint)
{
    u32 size;

    if ((endpoint == 0) || (endpoint->present == 0u))
    {
        return 0u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (((endpoint->interface_class != 0x03u) && (endpoint->interface_class != 0xFFu))
        || ((endpoint->interface_subclass == 0x01u) && (endpoint->interface_protocol == 0x01u)))
    {
        return 0u;
    }
#else
    if ((endpoint->interface_class != 0x03u)
        || ((endpoint->interface_subclass == 0x01u) && (endpoint->interface_protocol == 0x01u)))
    {
        return 0u;
    }
#endif

    size = endpoint->max_packet;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return ((size >= XHCI64_MOUSE_MIN_PROBE_PACKET_BYTES)
        && (size <= XHCI64_MOUSE_MAX_PROBE_PACKET_BYTES)) ? 1u : 0u;
#else
    return ((size == 3u) || (size == 4u) || (size == 5u) || (size == 8u)) ? 1u : 0u;
#endif
}

static void xhci64_prepare_hid_interface(
    u32 slot_id,
    struct xhci64_keyboard_endpoint *endpoint,
    const char *label)
{
    u32 descriptor_report_length = 0u;
    u32 report_length;

    if ((endpoint == 0) || (endpoint->present == 0u))
    {
        return;
    }

    if (xhci64_read_hid_descriptor(slot_id, endpoint->interface_number, &descriptor_report_length) != 0u
        && (descriptor_report_length != 0u))
    {
        endpoint->report_length = descriptor_report_length;
    }

    report_length = endpoint->report_length;
    if (report_length != 0u)
    {
        (void)xhci64_read_hid_report_descriptor(slot_id, endpoint->interface_number, report_length);
    }

    if (xhci64_set_boot_protocol(slot_id, endpoint->interface_number) == 0u)
    {
        serial_write_string("[x64] xHCI HID SET_PROTOCOL boot refused/ignored\n");
    }

    xhci64_log_hid_interface(label, endpoint, report_length);
}

static u32 xhci64_parse_hid_endpoint(
    const u8 *config,
    u32 length,
    u32 interface_protocol,
    struct xhci64_keyboard_endpoint *endpoint,
    u32 *configuration_value,
    u32 *report_length)
{
    u32 offset = 0u;
    u32 in_target_interface = 0u;
    u32 current_interface = 0u;

    endpoint->present = 0u;
    endpoint->slot_id = 0u;
    endpoint->dci = 0u;
    endpoint->max_packet = 0u;
    endpoint->interval = 0u;
    endpoint->interface_number = 0u;
    endpoint->interface_class = 0u;
    endpoint->interface_subclass = 0u;
    endpoint->interface_protocol = 0u;
    endpoint->report_length = 0u;
    *configuration_value = config[5];
    *report_length = 0u;

    while ((offset + 2u) <= length)
    {
        u32 descriptor_length = config[offset];
        u32 descriptor_type = config[offset + 1u];

        if ((descriptor_length == 0u) || ((offset + descriptor_length) > length))
        {
            break;
        }

        if ((descriptor_type == XHCI64_USB_DESC_INTERFACE) && (descriptor_length >= 9u))
        {
            current_interface = config[offset + 2u];
            endpoint->interface_class = config[offset + 5u];
            endpoint->interface_subclass = config[offset + 6u];
            endpoint->interface_protocol = config[offset + 7u];
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            xhci64_note_hid_interface_probe(
                endpoint->interface_class,
                endpoint->interface_subclass,
                endpoint->interface_protocol);
#endif
            in_target_interface =
                (config[offset + 5u] == 0x03u)
                && (((interface_protocol == XHCI64_HID_PROTOCOL_ANY)
                        && (config[offset + 4u] != 0u)
                        && !((config[offset + 6u] == 0x01u)
                            && (config[offset + 7u] == 0x01u)))
                    || ((config[offset + 6u] == 0x01u)
                        && (config[offset + 7u] == interface_protocol))
                    || ((interface_protocol != XHCI64_HID_PROTOCOL_ANY)
                        && (config[offset + 6u] == 0x00u)
                        && (config[offset + 7u] == interface_protocol)));
            if (in_target_interface != 0u)
            {
                endpoint->interface_number = current_interface;
            }
        }
        else if ((descriptor_type == XHCI64_USB_DESC_HID)
            && (descriptor_length >= 9u)
            && (in_target_interface != 0u))
        {
            if (config[offset + 6u] == XHCI64_USB_DESC_REPORT)
            {
                *report_length = xhci64_usb16(&config[offset + 7u]);
                endpoint->report_length = *report_length;
                g_xhci_hid_report_read = 1u;
            }
        }
        else if ((descriptor_type == XHCI64_USB_DESC_ENDPOINT)
            && (descriptor_length >= 7u)
            && (in_target_interface != 0u))
        {
            u32 endpoint_address = config[offset + 2u];
            u32 attributes = config[offset + 3u];
            if (((endpoint_address & 0x80u) != 0u) && ((attributes & 0x3u) == 0x3u))
            {
                u32 endpoint_number = endpoint_address & 0xFu;
                endpoint->present = 1u;
                endpoint->dci = (endpoint_number * 2u) + 1u;
                endpoint->max_packet = xhci64_usb16(&config[offset + 4u]) & 0x7FFu;
                endpoint->interval = config[offset + 6u];
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                if (endpoint->interface_protocol == 0x02u)
                {
                    xhci64_note_mouse_endpoint_probe(endpoint->max_packet);
                }
#endif
                if (endpoint->max_packet == 0u)
                {
                    endpoint->max_packet = 8u;
                }
                return 1u;
            }
        }

        offset += descriptor_length;
    }

    return 0u;
}

static u32 xhci64_parse_interrupt_mouse_probe_endpoint(
    const u8 *config,
    u32 length,
    struct xhci64_keyboard_endpoint *endpoint,
    u32 *configuration_value)
{
    u32 offset = 0u;
    u32 in_candidate_interface = 0u;
    u32 current_interface = 0u;

    endpoint->present = 0u;
    endpoint->slot_id = 0u;
    endpoint->dci = 0u;
    endpoint->max_packet = 0u;
    endpoint->interval = 0u;
    endpoint->interface_number = 0u;
    endpoint->interface_class = 0u;
    endpoint->interface_subclass = 0u;
    endpoint->interface_protocol = 0u;
    endpoint->report_length = 0u;
    *configuration_value = config[5];

    while ((offset + 2u) <= length)
    {
        u32 descriptor_length = config[offset];
        u32 descriptor_type = config[offset + 1u];

        if ((descriptor_length == 0u) || ((offset + descriptor_length) > length))
        {
            break;
        }

        if ((descriptor_type == XHCI64_USB_DESC_INTERFACE) && (descriptor_length >= 9u))
        {
            u32 interface_class = config[offset + 5u];
            u32 interface_subclass = config[offset + 6u];
            u32 interface_protocol = config[offset + 7u];

            current_interface = config[offset + 2u];
            endpoint->interface_class = interface_class;
            endpoint->interface_subclass = interface_subclass;
            endpoint->interface_protocol = interface_protocol;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            g_xhci_last_interface_class = interface_class;
            g_xhci_last_interface_subclass = interface_subclass;
            g_xhci_last_interface_protocol = interface_protocol;
            xhci64_note_hid_interface_probe(interface_class, interface_subclass, interface_protocol);
#endif
            in_candidate_interface =
                (config[offset + 4u] != 0u)
                && !((interface_class == 0x03u)
                    && (interface_subclass == 0x01u)
                    && (interface_protocol == 0x01u))
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                && (interface_class != 0x09u);
#else
                && ((interface_class == 0x03u) || (interface_class == 0xFFu));
#endif
            if (in_candidate_interface != 0u)
            {
                endpoint->interface_number = current_interface;
            }
        }
        else if ((descriptor_type == XHCI64_USB_DESC_ENDPOINT)
            && (descriptor_length >= 7u)
            && (in_candidate_interface != 0u))
        {
            u32 endpoint_address = config[offset + 2u];
            u32 attributes = config[offset + 3u];
            if (((endpoint_address & 0x80u) != 0u) && ((attributes & 0x3u) == 0x3u))
            {
                u32 endpoint_number = endpoint_address & 0xFu;
                u32 max_packet = xhci64_usb16(&config[offset + 4u]) & 0x7FFu;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                g_xhci_last_endpoint_max_packet = max_packet;
                if ((endpoint->interface_class == 0x03u) && (endpoint->interface_protocol == 0x02u))
                {
                    xhci64_note_mouse_endpoint_probe(max_packet);
                }
#endif

                if ((max_packet >= 3u) && (max_packet <= 16u))
                {
                    endpoint->present = 1u;
                    endpoint->dci = (endpoint_number * 2u) + 1u;
                    endpoint->max_packet = max_packet;
                    endpoint->interval = config[offset + 6u];
                    endpoint->report_length = max_packet;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                    if ((endpoint->interface_class != 0x03u) && (endpoint->interface_class != 0xFFu))
                    {
                        ++g_xhci_broad_mouse_probe_count;
                    }
#endif
                    return 1u;
                }
            }
        }

        offset += descriptor_length;
    }

    return 0u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 xhci64_parse_storage_bulk_endpoints(
    const u8 *config,
    u32 length,
    struct xhci64_storage_endpoint *storage,
    u32 *configuration_value)
{
    u32 offset = 0u;
    u32 in_storage_interface = 0u;
    u32 current_interface = 0u;

    xhci64_zero_memory(storage, sizeof(*storage));
    *configuration_value = config[5];

    while ((offset + 2u) <= length)
    {
        u32 descriptor_length = config[offset];
        u32 descriptor_type = config[offset + 1u];

        if ((descriptor_length == 0u) || ((offset + descriptor_length) > length))
        {
            break;
        }

        if ((descriptor_type == XHCI64_USB_DESC_INTERFACE) && (descriptor_length >= 9u))
        {
            u32 interface_class = config[offset + 5u];
            u32 interface_subclass = config[offset + 6u];
            u32 interface_protocol = config[offset + 7u];

            current_interface = config[offset + 2u];
            in_storage_interface =
                (interface_class == XHCI64_USB_CLASS_MASS_STORAGE)
                && (interface_subclass == XHCI64_USB_MSC_SUBCLASS_SCSI)
                && (interface_protocol == XHCI64_USB_MSC_PROTOCOL_BULK_ONLY);
            if (in_storage_interface != 0u)
            {
                storage->interface_number = current_interface;
                storage->configuration_value = *configuration_value;
                g_xhci_last_interface_class = interface_class;
                g_xhci_last_interface_subclass = interface_subclass;
                g_xhci_last_interface_protocol = interface_protocol;
            }
        }
        else if ((descriptor_type == XHCI64_USB_DESC_ENDPOINT)
            && (descriptor_length >= 7u)
            && (in_storage_interface != 0u))
        {
            u32 endpoint_address = config[offset + 2u];
            u32 attributes = config[offset + 3u];

            if ((attributes & 0x3u) == 0x2u)
            {
                u32 endpoint_number = endpoint_address & 0xFu;
                u32 max_packet = xhci64_usb16(&config[offset + 4u]) & 0x7FFu;
                u32 interval = config[offset + 6u];

                if (max_packet == 0u)
                {
                    max_packet = 64u;
                }

                if ((endpoint_address & 0x80u) != 0u)
                {
                    storage->bulk_in_dci = (endpoint_number * 2u) + 1u;
                    storage->bulk_in_mps = max_packet;
                    storage->bulk_in_interval = interval;
                }
                else
                {
                    storage->bulk_out_dci = endpoint_number * 2u;
                    storage->bulk_out_mps = max_packet;
                    storage->bulk_out_interval = interval;
                }

                g_xhci_last_endpoint_max_packet = max_packet;
            }
        }

        offset += descriptor_length;
    }

    storage->present = ((storage->bulk_in_dci != 0u) && (storage->bulk_out_dci != 0u)) ? 1u : 0u;
    return storage->present;
}

static u32 xhci64_configure_storage_bulk_endpoints(
    u32 slot_id,
    u32 port_id,
    u32 speed,
    const struct xhci64_storage_endpoint *storage)
{
    struct xhci64_event event;
    u32 context_entries = xhci64_max_u32(storage->bulk_in_dci, storage->bulk_out_dci);

    xhci64_zero_memory(g_xhci_input_context, sizeof(g_xhci_input_context));
    xhci64_reset_ring(g_xhci_bulk_in_ring, XHCI64_RING_TRBS, &g_xhci_bulk_in_enqueue, &g_xhci_bulk_in_cycle);
    xhci64_reset_ring(g_xhci_bulk_out_ring, XHCI64_RING_TRBS, &g_xhci_bulk_out_enqueue, &g_xhci_bulk_out_cycle);
    xhci64_context_write(
        g_xhci_input_context,
        0u,
        1u,
        (1u << 0) | (1u << storage->bulk_in_dci) | (1u << storage->bulk_out_dci));
    xhci64_prepare_slot_context(g_xhci_input_context, context_entries, port_id, speed);
    xhci64_prepare_ep_context(
        g_xhci_input_context,
        storage->bulk_in_dci,
        XHCI64_EP_TYPE_BULK_IN,
        storage->bulk_in_mps,
        storage->bulk_in_interval,
        xhci64_virtual_to_physical(g_xhci_bulk_in_ring),
        XHCI64_USB_STORAGE_SECTOR_BYTES);
    xhci64_prepare_ep_context(
        g_xhci_input_context,
        storage->bulk_out_dci,
        XHCI64_EP_TYPE_BULK_OUT,
        storage->bulk_out_mps,
        storage->bulk_out_interval,
        xhci64_virtual_to_physical(g_xhci_bulk_out_ring),
        XHCI64_USB_STORAGE_SECTOR_BYTES);

    if (xhci64_submit_command(
            xhci64_virtual_to_physical(g_xhci_input_context),
            (XHCI64_TRB_TYPE_CONFIGURE_ENDPOINT << XHCI64_TRB_TYPE_SHIFT) | (slot_id << 24),
            &event) == 0u)
    {
        g_xhci_usb_storage_error = 10u;
        return 0u;
    }

    g_xhci_usb_storage_last_completion = xhci64_completion_code(&event);
    if (g_xhci_usb_storage_last_completion != XHCI64_COMPLETION_SUCCESS)
    {
        g_xhci_usb_storage_error = 11u;
        return 0u;
    }

    g_xhci_endpoint_configured = 1u;
    return 1u;
}

static u32 xhci64_bulk_transfer(
    u32 slot_id,
    u32 dci,
    struct xhci64_trb *ring,
    u32 *enqueue,
    u32 *cycle,
    void *buffer,
    u32 byte_count)
{
    struct xhci64_trb *trb;
    struct xhci64_event event;
    u32 completion;

    if ((byte_count != 0u) && (buffer == 0))
    {
        return 0u;
    }

    trb = xhci64_ring_enqueue(
        ring,
        XHCI64_RING_TRBS,
        enqueue,
        cycle,
        xhci64_virtual_to_physical(buffer),
        byte_count,
        (XHCI64_TRB_TYPE_NORMAL << XHCI64_TRB_TYPE_SHIFT)
            | XHCI64_TRB_ISP
            | XHCI64_TRB_IOC);
    if (trb == 0)
    {
        g_xhci_usb_storage_error = 12u;
        return 0u;
    }

    xhci64_write32(g_xhci_doorbell_offset + (slot_id * 4u), dci);
    if (xhci64_poll_event(
            XHCI64_TRB_TYPE_TRANSFER_EVENT,
            xhci64_virtual_to_physical(trb),
            slot_id,
            dci,
            &event) == 0u)
    {
        g_xhci_usb_storage_error = 13u;
        return 0u;
    }

    completion = xhci64_completion_code(&event);
    g_xhci_usb_storage_last_completion = completion;
    if ((completion != XHCI64_COMPLETION_SUCCESS)
        && (completion != XHCI64_COMPLETION_SHORT_PACKET))
    {
        g_xhci_usb_storage_error = 14u;
        return 0u;
    }

    return 1u;
}

static void xhci64_write_le32(u8 *bytes, u32 offset, u32 value)
{
    bytes[offset] = (u8)(value & 0xFFu);
    bytes[offset + 1u] = (u8)((value >> 8) & 0xFFu);
    bytes[offset + 2u] = (u8)((value >> 16) & 0xFFu);
    bytes[offset + 3u] = (u8)((value >> 24) & 0xFFu);
}

static u32 xhci64_read_be32(const u8 *bytes, u32 offset)
{
    return ((u32)bytes[offset] << 24)
        | ((u32)bytes[offset + 1u] << 16)
        | ((u32)bytes[offset + 2u] << 8)
        | (u32)bytes[offset + 3u];
}

static u32 xhci64_storage_bot_command(
    const u8 *command,
    u32 command_bytes,
    void *data,
    u32 data_bytes,
    u32 direction_in)
{
    u32 tag;
    u32 csw_signature;
    u32 csw_tag;
    const struct xhci64_storage_endpoint *storage = &g_xhci_storage_endpoint;

    if ((storage->present == 0u)
        || (storage->slot_id == 0u)
        || (command == (const u8 *)0)
        || (command_bytes == 0u)
        || (command_bytes > 16u))
    {
        g_xhci_usb_storage_error = 20u;
        return 0u;
    }

    ++g_xhci_usb_storage_last_tag;
    if (g_xhci_usb_storage_last_tag == 0u)
    {
        g_xhci_usb_storage_last_tag = 1u;
    }
    tag = g_xhci_usb_storage_last_tag;

    xhci64_zero_memory(g_xhci_storage_cbw, sizeof(g_xhci_storage_cbw));
    xhci64_zero_memory(g_xhci_storage_csw, sizeof(g_xhci_storage_csw));
    xhci64_write_le32(g_xhci_storage_cbw, 0u, XHCI64_BOT_CBW_SIGNATURE);
    xhci64_write_le32(g_xhci_storage_cbw, 4u, tag);
    xhci64_write_le32(g_xhci_storage_cbw, 8u, data_bytes);
    g_xhci_storage_cbw[12u] = (direction_in != 0u) ? 0x80u : 0u;
    g_xhci_storage_cbw[13u] = 0u;
    g_xhci_storage_cbw[14u] = (u8)command_bytes;
    for (u32 index = 0u; index < command_bytes; ++index)
    {
        g_xhci_storage_cbw[15u + index] = command[index];
    }

    if (xhci64_bulk_transfer(
            storage->slot_id,
            storage->bulk_out_dci,
            g_xhci_bulk_out_ring,
            &g_xhci_bulk_out_enqueue,
            &g_xhci_bulk_out_cycle,
            g_xhci_storage_cbw,
            XHCI64_BOT_CBW_BYTES) == 0u)
    {
        return 0u;
    }

    if (data_bytes != 0u)
    {
        if (direction_in != 0u)
        {
            if (xhci64_bulk_transfer(
                    storage->slot_id,
                    storage->bulk_in_dci,
                    g_xhci_bulk_in_ring,
                    &g_xhci_bulk_in_enqueue,
                    &g_xhci_bulk_in_cycle,
                    data,
                    data_bytes) == 0u)
            {
                return 0u;
            }
        }
        else
        {
            if (xhci64_bulk_transfer(
                    storage->slot_id,
                    storage->bulk_out_dci,
                    g_xhci_bulk_out_ring,
                    &g_xhci_bulk_out_enqueue,
                    &g_xhci_bulk_out_cycle,
                    data,
                    data_bytes) == 0u)
            {
                return 0u;
            }
        }
    }

    if (xhci64_bulk_transfer(
            storage->slot_id,
            storage->bulk_in_dci,
            g_xhci_bulk_in_ring,
            &g_xhci_bulk_in_enqueue,
            &g_xhci_bulk_in_cycle,
            g_xhci_storage_csw,
            XHCI64_BOT_CSW_BYTES) == 0u)
    {
        return 0u;
    }

    csw_signature =
        (u32)g_xhci_storage_csw[0u]
        | ((u32)g_xhci_storage_csw[1u] << 8)
        | ((u32)g_xhci_storage_csw[2u] << 16)
        | ((u32)g_xhci_storage_csw[3u] << 24);
    csw_tag =
        (u32)g_xhci_storage_csw[4u]
        | ((u32)g_xhci_storage_csw[5u] << 8)
        | ((u32)g_xhci_storage_csw[6u] << 16)
        | ((u32)g_xhci_storage_csw[7u] << 24);
    if ((csw_signature != XHCI64_BOT_CSW_SIGNATURE)
        || (csw_tag != tag)
        || (g_xhci_storage_csw[12u] != 0u))
    {
        g_xhci_usb_storage_error = 21u;
        return 0u;
    }

    return 1u;
}

static u32 xhci64_usb_storage_probe_ready(void)
{
    u8 command[16];

    xhci64_zero_memory(command, sizeof(command));
    command[0u] = XHCI64_SCSI_INQUIRY;
    command[4u] = 36u;
    (void)xhci64_storage_bot_command(command, 6u, g_xhci_storage_scratch, 36u, 1u);

    xhci64_zero_memory(command, sizeof(command));
    command[0u] = XHCI64_SCSI_TEST_UNIT_READY;
    (void)xhci64_storage_bot_command(command, 6u, 0, 0u, 1u);

    xhci64_zero_memory(command, sizeof(command));
    xhci64_zero_memory(g_xhci_storage_scratch, sizeof(g_xhci_storage_scratch));
    command[0u] = XHCI64_SCSI_READ_CAPACITY_10;
    if (xhci64_storage_bot_command(command, 10u, g_xhci_storage_scratch, 8u, 1u) == 0u)
    {
        g_xhci_usb_storage_error = 30u;
        return 0u;
    }

    g_xhci_usb_storage_last_lba = xhci64_read_be32(g_xhci_storage_scratch, 0u);
    g_xhci_usb_storage_block_bytes = xhci64_read_be32(g_xhci_storage_scratch, 4u);
    if (g_xhci_usb_storage_block_bytes != XHCI64_USB_STORAGE_SECTOR_BYTES)
    {
        g_xhci_usb_storage_error = 31u;
        return 0u;
    }

    g_xhci_usb_storage_ready = 1u;
    g_xhci_usb_storage_error = 0u;
    return 1u;
}
#endif

static u32 xhci64_configure_interrupt_endpoint(
    u32 slot_id,
    u32 port_id,
    u32 speed,
    const struct xhci64_keyboard_endpoint *endpoint,
    struct xhci64_trb *ring,
    u32 *enqueue,
    u32 *cycle)
{
    struct xhci64_event event;

    xhci64_zero_memory(g_xhci_input_context, sizeof(g_xhci_input_context));
    xhci64_reset_ring(ring, XHCI64_RING_TRBS, enqueue, cycle);
    xhci64_context_write(g_xhci_input_context, 0u, 1u, (1u << 0) | (1u << endpoint->dci));
    xhci64_prepare_slot_context(g_xhci_input_context, endpoint->dci, port_id, speed);
    xhci64_prepare_ep_context(
        g_xhci_input_context,
        endpoint->dci,
        XHCI64_EP_TYPE_INTERRUPT_IN,
        endpoint->max_packet,
        endpoint->interval,
        xhci64_virtual_to_physical(ring),
        endpoint->max_packet);

    if (xhci64_submit_command(
            xhci64_virtual_to_physical(g_xhci_input_context),
            (XHCI64_TRB_TYPE_CONFIGURE_ENDPOINT << XHCI64_TRB_TYPE_SHIFT) | (slot_id << 24),
            &event) == 0u)
    {
        return 0u;
    }

    if (xhci64_completion_code(&event) != XHCI64_COMPLETION_SUCCESS)
    {
        return 0u;
    }

    g_xhci_endpoint_configured = 1u;
    return 1u;
}

static u32 xhci64_configure_dual_interrupt_endpoints(
    u32 slot_id,
    u32 port_id,
    u32 speed,
    const struct xhci64_keyboard_endpoint *keyboard_endpoint,
    const struct xhci64_keyboard_endpoint *mouse_endpoint)
{
    struct xhci64_event event;
    u32 context_entries;

    xhci64_zero_memory(g_xhci_input_context, sizeof(g_xhci_input_context));
    xhci64_reset_ring(g_xhci_interrupt_ring, XHCI64_RING_TRBS, &g_xhci_intr_enqueue, &g_xhci_intr_cycle);
    xhci64_reset_ring(
        g_xhci_mouse_interrupt_ring,
        XHCI64_RING_TRBS,
        &g_xhci_mouse_intr_enqueue,
        &g_xhci_mouse_intr_cycle);

    context_entries = xhci64_max_u32(keyboard_endpoint->dci, mouse_endpoint->dci);
    xhci64_context_write(
        g_xhci_input_context,
        0u,
        1u,
        (1u << 0)
            | (1u << keyboard_endpoint->dci)
            | (1u << mouse_endpoint->dci));
    xhci64_prepare_slot_context(g_xhci_input_context, context_entries, port_id, speed);
    xhci64_prepare_ep_context(
        g_xhci_input_context,
        keyboard_endpoint->dci,
        XHCI64_EP_TYPE_INTERRUPT_IN,
        keyboard_endpoint->max_packet,
        keyboard_endpoint->interval,
        xhci64_virtual_to_physical(g_xhci_interrupt_ring),
        keyboard_endpoint->max_packet);
    xhci64_prepare_ep_context(
        g_xhci_input_context,
        mouse_endpoint->dci,
        XHCI64_EP_TYPE_INTERRUPT_IN,
        mouse_endpoint->max_packet,
        mouse_endpoint->interval,
        xhci64_virtual_to_physical(g_xhci_mouse_interrupt_ring),
        mouse_endpoint->max_packet);

    if (xhci64_submit_command(
            xhci64_virtual_to_physical(g_xhci_input_context),
            (XHCI64_TRB_TYPE_CONFIGURE_ENDPOINT << XHCI64_TRB_TYPE_SHIFT) | (slot_id << 24),
            &event) == 0u)
    {
        return 0u;
    }

    if (xhci64_completion_code(&event) != XHCI64_COMPLETION_SUCCESS)
    {
        return 0u;
    }

    g_xhci_endpoint_configured = 1u;
    return 1u;
}

static u32 xhci64_try_enumerate_port(u32 port_id)
{
    u32 speed = 0u;
    u32 slot_id = 0u;
    u32 total_length;
    u32 configuration_value = 0u;
    u32 keyboard_report_length = 0u;
    u32 mouse_report_length = 0u;
    u32 generic_report_length = 0u;
    struct xhci64_keyboard_endpoint keyboard_endpoint;
    struct xhci64_keyboard_endpoint mouse_endpoint;
    struct xhci64_keyboard_endpoint generic_endpoint;
    struct xhci64_keyboard_endpoint probe_endpoint;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    struct xhci64_storage_endpoint storage_endpoint;
#endif
    u32 keyboard_match;
    u32 mouse_match;
    u32 generic_match = 0u;
    u32 probe_match = 0u;
    u32 storage_match = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    struct xhci64_hid_candidate hid_candidates[XHCI64_HID_INTERFACE_LIMIT];
    u32 hid_candidate_count;
    u32 hid_candidate_index;

    xhci64_clear_endpoint(&keyboard_endpoint);
    xhci64_clear_endpoint(&mouse_endpoint);
    xhci64_clear_endpoint(&generic_endpoint);
    xhci64_clear_endpoint(&probe_endpoint);
    xhci64_zero_memory(&storage_endpoint, sizeof(storage_endpoint));
    for (hid_candidate_index = 0u;
        hid_candidate_index < XHCI64_HID_INTERFACE_LIMIT;
        ++hid_candidate_index)
    {
        xhci64_clear_endpoint(&hid_candidates[hid_candidate_index].endpoint);
    }
#endif

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_descriptor_probe_port = port_id;
#endif

    if (xhci64_reset_port(port_id, &speed) == 0u)
    {
        xhci64_log_port_skip(port_id, 20u);
        return 0u;
    }

    if (xhci64_enable_slot(&slot_id) == 0u)
    {
        xhci64_log_port_skip(port_id, 21u);
        return 0u;
    }

    if (xhci64_address_device(&slot_id, port_id, &speed) == 0u)
    {
        xhci64_log_port_skip(port_id, 22u);
        XHCI64_CLEANUP_FAILED_SLOT(slot_id);
        return 0u;
    }

    if (xhci64_get_descriptor(
            slot_id,
            XHCI64_USB_DESC_DEVICE << 8,
            0u,
            g_xhci_control_buffer,
            18u) == 0u)
    {
        xhci64_log_port_skip(port_id, 23u);
        XHCI64_CLEANUP_FAILED_SLOT(slot_id);
        return 0u;
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_last_device_class = g_xhci_control_buffer[4u];
    g_xhci_last_device_subclass = g_xhci_control_buffer[5u];
    g_xhci_last_device_protocol = g_xhci_control_buffer[6u];
#endif

    if (xhci64_get_descriptor(
            slot_id,
            XHCI64_USB_DESC_CONFIGURATION << 8,
            0u,
            g_xhci_control_buffer,
            9u) == 0u)
    {
        xhci64_log_port_skip(port_id, 24u);
        XHCI64_CLEANUP_FAILED_SLOT(slot_id);
        return 0u;
    }

    if ((g_xhci_control_buffer[1] != XHCI64_USB_DESC_CONFIGURATION)
        || (xhci64_usb16(&g_xhci_control_buffer[2]) < 9u))
    {
        xhci64_log_port_skip(port_id, 25u);
        XHCI64_CLEANUP_FAILED_SLOT(slot_id);
        return 0u;
    }

    total_length = xhci64_usb16(&g_xhci_control_buffer[2]);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_last_config_total_length = total_length;
#endif
    if (total_length > sizeof(g_xhci_control_buffer))
    {
        total_length = sizeof(g_xhci_control_buffer);
    }

    if (xhci64_get_descriptor(
            slot_id,
            XHCI64_USB_DESC_CONFIGURATION << 8,
            0u,
            g_xhci_control_buffer,
            total_length) == 0u)
    {
        xhci64_log_port_skip(port_id, 26u);
        XHCI64_CLEANUP_FAILED_SLOT(slot_id);
        return 0u;
    }

    g_xhci_config_read = 1u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    hid_candidate_count = xhci64_collect_hid_candidates(
        g_xhci_control_buffer,
        total_length,
        hid_candidates,
        XHCI64_HID_INTERFACE_LIMIT,
        &configuration_value);
    keyboard_match = xhci64_select_hid_candidate(
        hid_candidates,
        hid_candidate_count,
        0x01u,
        0x01u,
        &keyboard_endpoint);
    mouse_match = xhci64_select_hid_candidate(
        hid_candidates,
        hid_candidate_count,
        0x01u,
        0x02u,
        &mouse_endpoint);
    if (mouse_match == 0u)
    {
        generic_match = xhci64_select_generic_hid_candidate(
            hid_candidates,
            hid_candidate_count,
            &generic_endpoint);
        generic_report_length = generic_endpoint.report_length;
    }
    if (hid_candidate_count == 0u)
    {
        keyboard_match = xhci64_parse_hid_endpoint(
            g_xhci_control_buffer,
            total_length,
            0x01u,
            &keyboard_endpoint,
            &configuration_value,
            &keyboard_report_length);
        mouse_match = xhci64_parse_hid_endpoint(
            g_xhci_control_buffer,
            total_length,
            0x02u,
            &mouse_endpoint,
            &configuration_value,
            &mouse_report_length);
        if (mouse_match == 0u)
        {
            generic_match = xhci64_parse_hid_endpoint(
                g_xhci_control_buffer,
                total_length,
                XHCI64_HID_PROTOCOL_ANY,
                &generic_endpoint,
                &configuration_value,
                &generic_report_length);
        }
    }
    keyboard_report_length = keyboard_endpoint.report_length;
    mouse_report_length = mouse_endpoint.report_length;
#else
    keyboard_match = xhci64_parse_hid_endpoint(
        g_xhci_control_buffer,
        total_length,
        0x01u,
        &keyboard_endpoint,
        &configuration_value,
        &keyboard_report_length);
    mouse_match = xhci64_parse_hid_endpoint(
        g_xhci_control_buffer,
        total_length,
        0x02u,
        &mouse_endpoint,
        &configuration_value,
        &mouse_report_length);
    if ((keyboard_match == 0u) && (mouse_match == 0u))
    {
        generic_match = xhci64_parse_hid_endpoint(
            g_xhci_control_buffer,
            total_length,
            XHCI64_HID_PROTOCOL_ANY,
            &generic_endpoint,
            &configuration_value,
            &generic_report_length);
    }
#endif
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    storage_match = xhci64_parse_storage_bulk_endpoints(
        g_xhci_control_buffer,
        total_length,
        &storage_endpoint,
        &configuration_value);
#endif
    if ((keyboard_match == 0u) && (mouse_match == 0u) && (generic_match == 0u))
    {
        probe_match = xhci64_parse_interrupt_mouse_probe_endpoint(
            g_xhci_control_buffer,
            total_length,
            &probe_endpoint,
            &configuration_value);
        if ((probe_match == 0u) && (storage_match == 0u))
        {
            xhci64_log_port_skip(port_id, 27u);
            XHCI64_CLEANUP_FAILED_SLOT(slot_id);
            return 0u;
        }
    }

    if (xhci64_set_configuration(slot_id, configuration_value) == 0u)
    {
        xhci64_log_port_skip(port_id, 28u);
        XHCI64_CLEANUP_FAILED_SLOT(slot_id);
        return 0u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if ((storage_match != 0u) && (g_xhci_storage_endpoint.present == 0u))
    {
        storage_endpoint.slot_id = slot_id;
        storage_endpoint.port_id = port_id;
        if (xhci64_configure_storage_bulk_endpoints(
                slot_id,
                port_id,
                speed,
                &storage_endpoint) == 0u)
        {
            xhci64_log_port_skip(port_id, 37u);
            XHCI64_CLEANUP_FAILED_SLOT(slot_id);
            return 0u;
        }

        g_xhci_storage_endpoint = storage_endpoint;
        g_xhci_usb_storage_present = 1u;
        if (xhci64_usb_storage_probe_ready() == 0u)
        {
            xhci64_log_port_skip(port_id, 38u);
            XHCI64_CLEANUP_FAILED_SLOT(slot_id);
            xhci64_zero_memory(&g_xhci_storage_endpoint, sizeof(g_xhci_storage_endpoint));
            g_xhci_usb_storage_present = 0u;
            return 0u;
        }

        return 1u;
    }
#endif

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if ((keyboard_match != 0u)
        && (mouse_match == 0u)
        && (generic_match != 0u)
        && (g_xhci_keyboard_endpoint.present == 0u)
        && (g_xhci_mouse_endpoint.present == 0u))
    {
        u32 generic_effective_report_length;
        u32 generic_has_mouse;
        u32 generic_has_keyboard;

        keyboard_endpoint.slot_id = slot_id;
        keyboard_endpoint.report_length = keyboard_report_length;
        generic_endpoint.slot_id = slot_id;
        generic_endpoint.report_length = generic_report_length;
        xhci64_prepare_hid_interface(slot_id, &keyboard_endpoint, "keyboard");
        xhci64_prepare_hid_interface(slot_id, &generic_endpoint, "generic-mouse");
        generic_effective_report_length = (generic_endpoint.report_length != 0u)
            ? generic_endpoint.report_length
            : generic_report_length;
        generic_has_mouse = xhci64_hid_report_has_usage(generic_effective_report_length, 0x02u);
        generic_has_keyboard = xhci64_hid_report_has_usage(generic_effective_report_length, 0x06u);

        if ((generic_has_mouse != 0u)
            || ((generic_has_keyboard == 0u)
                && (xhci64_endpoint_can_probe_as_mouse(&generic_endpoint) != 0u)))
        {
            xhci64_configure_input_mouse_layout(generic_effective_report_length);
            g_xhci_mouse_report_offset =
                (xhci64_hid_report_has_report_id(generic_effective_report_length) != 0u) ? 1u : 0u;
            g_xhci_mouse_report_size = xhci64_mouse_transfer_size_from_endpoint(&generic_endpoint);
            if (xhci64_configure_dual_interrupt_endpoints(
                    slot_id,
                    port_id,
                    speed,
                    &keyboard_endpoint,
                    &generic_endpoint) == 0u)
            {
                xhci64_log_port_skip(port_id, 39u);
                XHCI64_CLEANUP_FAILED_SLOT(slot_id);
                return 0u;
            }

            g_xhci_keyboard_endpoint = keyboard_endpoint;
            g_xhci_mouse_endpoint = generic_endpoint;
            g_xhci_mouse_device = 1u;
            g_xhci_hid_device = 1u;
            g_xhci_boot_mouse_interface = generic_endpoint.interface_number;
            g_xhci_boot_mouse_port = port_id;
            g_xhci_boot_mouse_dci = generic_endpoint.dci;
            g_xhci_boot_mouse_mps = generic_endpoint.max_packet;
            g_xhci_boot_mouse_configured = 1u;
            return 1u;
        }
    }
#endif

    if ((keyboard_match != 0u)
        && (mouse_match != 0u)
        && (g_xhci_keyboard_endpoint.present == 0u)
        && (g_xhci_mouse_endpoint.present == 0u))
    {
        keyboard_endpoint.slot_id = slot_id;
        mouse_endpoint.slot_id = slot_id;
        keyboard_endpoint.report_length = keyboard_report_length;
        mouse_endpoint.report_length = mouse_report_length;
        xhci64_prepare_hid_interface(slot_id, &keyboard_endpoint, "keyboard");
        xhci64_prepare_hid_interface(slot_id, &mouse_endpoint, "mouse");
        xhci64_configure_input_mouse_layout(mouse_endpoint.report_length);
        g_xhci_mouse_report_offset =
            (xhci64_hid_report_has_report_id(mouse_endpoint.report_length) != 0u) ? 1u : 0u;
        g_xhci_mouse_report_size = xhci64_mouse_transfer_size_from_endpoint(&mouse_endpoint);

        if (xhci64_configure_dual_interrupt_endpoints(
                slot_id,
                port_id,
                speed,
                &keyboard_endpoint,
                &mouse_endpoint) == 0u)
        {
            xhci64_log_port_skip(port_id, 34u);
            XHCI64_CLEANUP_FAILED_SLOT(slot_id);
            return 0u;
        }

        g_xhci_keyboard_endpoint = keyboard_endpoint;
        g_xhci_mouse_endpoint = mouse_endpoint;
        g_xhci_mouse_device = 1u;
        g_xhci_hid_device = 1u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_xhci_boot_mouse_interface = mouse_endpoint.interface_number;
        g_xhci_boot_mouse_port = port_id;
        g_xhci_boot_mouse_dci = mouse_endpoint.dci;
        g_xhci_boot_mouse_mps = mouse_endpoint.max_packet;
        g_xhci_boot_mouse_configured = 1u;
#endif
        return 1u;
    }

    if ((keyboard_match != 0u) && (g_xhci_keyboard_endpoint.present == 0u))
    {
        keyboard_endpoint.slot_id = slot_id;
        keyboard_endpoint.report_length = keyboard_report_length;
        xhci64_prepare_hid_interface(slot_id, &keyboard_endpoint, "keyboard");

        if (xhci64_configure_interrupt_endpoint(
                slot_id,
                port_id,
                speed,
                &keyboard_endpoint,
                g_xhci_interrupt_ring,
                &g_xhci_intr_enqueue,
                &g_xhci_intr_cycle) == 0u)
        {
            xhci64_log_port_skip(port_id, 29u);
            XHCI64_CLEANUP_FAILED_SLOT(slot_id);
            return 0u;
        }

        g_xhci_keyboard_endpoint = keyboard_endpoint;
        g_xhci_hid_device = 1u;
        return 1u;
    }

    if ((mouse_match != 0u) && (g_xhci_mouse_endpoint.present == 0u))
    {
        mouse_endpoint.slot_id = slot_id;
        mouse_endpoint.report_length = mouse_report_length;
        xhci64_prepare_hid_interface(slot_id, &mouse_endpoint, "mouse");
        xhci64_configure_input_mouse_layout(mouse_endpoint.report_length);
        g_xhci_mouse_report_offset =
            (xhci64_hid_report_has_report_id(mouse_endpoint.report_length) != 0u) ? 1u : 0u;
        g_xhci_mouse_report_size = xhci64_mouse_transfer_size_from_endpoint(&mouse_endpoint);

        if (xhci64_configure_interrupt_endpoint(
                slot_id,
                port_id,
                speed,
                &mouse_endpoint,
                g_xhci_mouse_interrupt_ring,
                &g_xhci_mouse_intr_enqueue,
                &g_xhci_mouse_intr_cycle) == 0u)
        {
            xhci64_log_port_skip(port_id, 30u);
            XHCI64_CLEANUP_FAILED_SLOT(slot_id);
            return 0u;
        }

        g_xhci_mouse_endpoint = mouse_endpoint;
        g_xhci_mouse_device = 1u;
        g_xhci_hid_device = 1u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_xhci_boot_mouse_interface = mouse_endpoint.interface_number;
        g_xhci_boot_mouse_port = port_id;
        g_xhci_boot_mouse_dci = mouse_endpoint.dci;
        g_xhci_boot_mouse_mps = mouse_endpoint.max_packet;
        g_xhci_boot_mouse_configured = 1u;
#endif
        return 1u;
    }

    if (generic_match != 0u)
    {
        u32 generic_effective_report_length;
        u32 generic_has_mouse;
        u32 generic_has_keyboard;
        generic_endpoint.slot_id = slot_id;
        generic_endpoint.report_length = generic_report_length;
        xhci64_prepare_hid_interface(slot_id, &generic_endpoint, "generic");
        generic_effective_report_length = (generic_endpoint.report_length != 0u)
            ? generic_endpoint.report_length
            : generic_report_length;
        generic_has_mouse = xhci64_hid_report_has_usage(generic_effective_report_length, 0x02u);
        generic_has_keyboard = xhci64_hid_report_has_usage(generic_effective_report_length, 0x06u);

        if ((g_xhci_mouse_endpoint.present == 0u) && (generic_has_mouse != 0u))
        {
            xhci64_configure_input_mouse_layout(generic_effective_report_length);
            g_xhci_mouse_report_offset =
                (xhci64_hid_report_has_report_id(generic_effective_report_length) != 0u) ? 1u : 0u;
            g_xhci_mouse_report_size = xhci64_mouse_transfer_size_from_endpoint(&generic_endpoint);
            if (xhci64_configure_interrupt_endpoint(
                    slot_id,
                    port_id,
                    speed,
                    &generic_endpoint,
                    g_xhci_mouse_interrupt_ring,
                    &g_xhci_mouse_intr_enqueue,
                    &g_xhci_mouse_intr_cycle) == 0u)
            {
                xhci64_log_port_skip(port_id, 32u);
                XHCI64_CLEANUP_FAILED_SLOT(slot_id);
                return 0u;
            }

            g_xhci_mouse_endpoint = generic_endpoint;
            g_xhci_mouse_device = 1u;
            g_xhci_hid_device = 1u;
            return 1u;
        }

        if ((g_xhci_mouse_endpoint.present == 0u)
            && (generic_has_keyboard == 0u)
            && (xhci64_endpoint_can_probe_as_mouse(&generic_endpoint) != 0u))
        {
            xhci64_configure_input_mouse_layout(generic_effective_report_length);
            g_xhci_mouse_report_offset = 0u;
            g_xhci_mouse_report_size = xhci64_mouse_transfer_size_from_endpoint(&generic_endpoint);
            if (xhci64_configure_interrupt_endpoint(
                    slot_id,
                    port_id,
                    speed,
                    &generic_endpoint,
                    g_xhci_mouse_interrupt_ring,
                    &g_xhci_mouse_intr_enqueue,
                    &g_xhci_mouse_intr_cycle) == 0u)
            {
                xhci64_log_port_skip(port_id, 35u);
                XHCI64_CLEANUP_FAILED_SLOT(slot_id);
                return 0u;
            }

            serial_write_string("[x64] xHCI generic HID interrupt endpoint configured as mouse probe\n");
            g_xhci_mouse_endpoint = generic_endpoint;
            g_xhci_mouse_device = 1u;
            g_xhci_hid_device = 1u;
            return 1u;
        }

        if ((g_xhci_keyboard_endpoint.present == 0u) && (generic_has_keyboard != 0u))
        {
            if (xhci64_configure_interrupt_endpoint(
                    slot_id,
                    port_id,
                    speed,
                    &generic_endpoint,
                    g_xhci_interrupt_ring,
                    &g_xhci_intr_enqueue,
                    &g_xhci_intr_cycle) == 0u)
            {
                xhci64_log_port_skip(port_id, 33u);
                XHCI64_CLEANUP_FAILED_SLOT(slot_id);
                return 0u;
            }

            g_xhci_keyboard_endpoint = generic_endpoint;
            g_xhci_hid_device = 1u;
            return 1u;
        }
    }

    if ((probe_match != 0u) && (g_xhci_mouse_endpoint.present == 0u))
    {
        probe_endpoint.slot_id = slot_id;
        g_xhci_mouse_report_offset = 0u;
        g_xhci_mouse_report_size = xhci64_mouse_transfer_size_from_endpoint(&probe_endpoint);
        if (xhci64_configure_interrupt_endpoint(
                slot_id,
                port_id,
                speed,
                &probe_endpoint,
                g_xhci_mouse_interrupt_ring,
                &g_xhci_mouse_intr_enqueue,
                &g_xhci_mouse_intr_cycle) == 0u)
        {
            xhci64_log_port_skip(port_id, 36u);
            XHCI64_CLEANUP_FAILED_SLOT(slot_id);
            return 0u;
        }

        serial_write_string("[x64] xHCI interrupt endpoint configured as mouse probe\n");
        g_xhci_mouse_endpoint = probe_endpoint;
        g_xhci_mouse_device = 1u;
        g_xhci_hid_device = 1u;
        g_xhci_error = 0u;
        return 1u;
    }

    return 0u;
}

static void xhci64_enumerate_hid(void)
{
    u32 port_count = g_xhci_hcs_ports;
    u32 port_id;

    if (port_count > 32u)
    {
        port_count = 32u;
    }

    if (g_xhci_prefer_usb2 != 0u)
    {
        for (port_id = 1u; port_id <= port_count; ++port_id)
        {
            if (g_xhci_port_protocol[port_id - 1u] != 2u)
            {
                continue;
            }
            if (xhci64_port_connected(port_id) == 0u)
            {
                continue;
            }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            if (xhci64_probe_port_can_attempt(port_id) == 0u)
            {
                continue;
            }
#endif
            if (xhci64_try_enumerate_port(port_id) != 0u)
            {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                if ((g_xhci_keyboard_endpoint.present != 0u)
                    && (g_xhci_mouse_endpoint.present != 0u)
                    && (g_xhci_storage_endpoint.present != 0u))
                {
                    return;
                }
#else
                if ((g_xhci_keyboard_endpoint.present != 0u) && (g_xhci_mouse_endpoint.present != 0u))
                {
                    return;
                }
#endif
            }
        }
    }

    for (port_id = 1u; port_id <= port_count; ++port_id)
    {
        if ((g_xhci_prefer_usb2 != 0u) && (g_xhci_port_protocol[port_id - 1u] == 2u))
        {
            continue;
        }
        if (xhci64_port_connected(port_id) == 0u)
        {
            continue;
        }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        if (xhci64_probe_port_can_attempt(port_id) == 0u)
        {
            continue;
        }
#endif
        if (xhci64_try_enumerate_port(port_id) != 0u)
        {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            if ((g_xhci_keyboard_endpoint.present != 0u)
                && (g_xhci_mouse_endpoint.present != 0u)
                && (g_xhci_storage_endpoint.present != 0u))
            {
                return;
            }
#else
            if ((g_xhci_keyboard_endpoint.present != 0u) && (g_xhci_mouse_endpoint.present != 0u))
            {
                return;
            }
#endif
        }
    }
}

static void xhci64_queue_keyboard_report(void)
{
    struct xhci64_trb *trb;

    if ((g_xhci_keyboard_endpoint.present == 0u) || (g_xhci_intr_pending != 0u))
    {
        return;
    }

    xhci64_zero_memory(g_xhci_keyboard_report, sizeof(g_xhci_keyboard_report));
    trb = xhci64_ring_enqueue(
        g_xhci_interrupt_ring,
        XHCI64_RING_TRBS,
        &g_xhci_intr_enqueue,
        &g_xhci_intr_cycle,
        xhci64_virtual_to_physical(g_xhci_keyboard_report),
        sizeof(g_xhci_keyboard_report),
        (XHCI64_TRB_TYPE_NORMAL << XHCI64_TRB_TYPE_SHIFT)
            | XHCI64_TRB_ISP
            | XHCI64_TRB_IOC);
    g_xhci_intr_pending = xhci64_virtual_to_physical(trb) != 0ull ? 1u : 0u;
    xhci64_write32(
        g_xhci_doorbell_offset + (g_xhci_keyboard_endpoint.slot_id * 4u),
        g_xhci_keyboard_endpoint.dci);
}

static void xhci64_queue_mouse_report(void)
{
    struct xhci64_trb *trb;

    if ((g_xhci_mouse_endpoint.present == 0u) || (g_xhci_mouse_intr_pending != 0u))
    {
        return;
    }

    xhci64_zero_memory(g_xhci_mouse_report, sizeof(g_xhci_mouse_report));
    trb = xhci64_ring_enqueue(
        g_xhci_mouse_interrupt_ring,
        XHCI64_RING_TRBS,
        &g_xhci_mouse_intr_enqueue,
        &g_xhci_mouse_intr_cycle,
        xhci64_virtual_to_physical(g_xhci_mouse_report),
        g_xhci_mouse_report_size,
        (XHCI64_TRB_TYPE_NORMAL << XHCI64_TRB_TYPE_SHIFT)
            | XHCI64_TRB_ISP
            | XHCI64_TRB_IOC);
    g_xhci_mouse_intr_pending = xhci64_virtual_to_physical(trb) != 0ull ? 1u : 0u;
    xhci64_write32(
        g_xhci_doorbell_offset + (g_xhci_mouse_endpoint.slot_id * 4u),
        g_xhci_mouse_endpoint.dci);
}

u32 xhci64_live_polling_supported(void)
{
    if (g_xhci_controller_running == 0u)
    {
        return 0u;
    }

    if (((g_xhci_flags & XHCI64_MMIO_FLAG_PRESENT) == 0u) || (g_xhci_mapped == 0u))
    {
        return 0u;
    }

    return ((g_xhci_keyboard_endpoint.present != 0u)
        || (g_xhci_mouse_endpoint.present != 0u)) ? 1u : 0u;
}

u32 xhci64_rescan_devices(void)
{
    if ((g_xhci_controller_running == 0u) || (g_xhci_mapped == 0u))
    {
        return 0u;
    }

    g_xhci_live_rescan_countdown = 0u;
    xhci64_scan_ports();
    xhci64_enumerate_hid();

    if (xhci64_live_polling_supported() != 0u)
    {
        g_xhci_live_polling_enabled = 1u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if ((g_xhci_hid_device != 0u) || (g_xhci_usb_storage_ready != 0u))
#else
    if (g_xhci_hid_device != 0u)
#endif
    {
        g_xhci_unavailable = 0u;
        return 1u;
    }

    return 0u;
}

void xhci64_register_candidate(
    u32 address,
    u32 vendor_device,
    u32 class_register,
    u32 bar0,
    u32 bar1,
    u32 base_low,
    u32 base_high,
    u32 span_hint,
    u32 flags,
    u32 token)
{
    g_xhci_address = address;
    g_xhci_vendor_device = vendor_device;
    g_xhci_class = class_register;
    g_xhci_bar0 = bar0;
    g_xhci_bar1 = bar1;
    g_xhci_base_low = base_low;
    g_xhci_base_high = base_high;
    g_xhci_span_hint = span_hint;
    g_xhci_flags = flags;
    g_xhci_token = token;
    g_xhci_mapped = 0u;
    g_xhci_cap_length = 0u;
    g_xhci_max_slots = 0u;
    g_xhci_hcs_ports = 0u;
    g_xhci_context_size = 32u;
    g_xhci_doorbell_offset = 0u;
    g_xhci_runtime_offset = 0u;
    g_xhci_ports_scanned = 0u;
    g_xhci_connected_ports = 0u;
    g_xhci_command_ring_staged = 0u;
    g_xhci_dcbaa_staged = 0u;
    g_xhci_event_ring_staged = 0u;
    g_xhci_controller_reset = 0u;
    g_xhci_controller_running = 0u;
    g_xhci_slot_enabled = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_slots_disabled = 0u;
    g_xhci_slot_disable_failures = 0u;
#endif
    g_xhci_addressed = 0u;
    g_xhci_config_read = 0u;
    g_xhci_hid_report_read = 0u;
    g_xhci_endpoint_configured = 0u;
    g_xhci_hid_device = 0u;
    g_xhci_input_live = 0u;
    g_xhci_report_count = 0u;
    g_xhci_report_bytes = 0u;
    g_xhci_mouse_device = 0u;
    g_xhci_mouse_reports = 0u;
    g_xhci_mouse_report_bytes = 0u;
    g_xhci_mouse_report_offset = 0u;
    g_xhci_mouse_report_size = 8u;
    g_xhci_live_polling_enabled = 0u;
    g_xhci_live_rescan_countdown = 0u;
    g_xhci_unavailable = ((flags & XHCI64_MMIO_FLAG_PRESENT) != 0u) ? 0u : 1u;
    g_xhci_error = 0u;
    g_xhci_port_error_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_last_skip_port = 0u;
    g_xhci_last_skip_code = 0u;
    g_xhci_last_device_class = 0u;
    g_xhci_last_device_subclass = 0u;
    g_xhci_last_device_protocol = 0u;
    g_xhci_last_config_total_length = 0u;
    g_xhci_last_interface_class = 0u;
    g_xhci_last_interface_subclass = 0u;
    g_xhci_last_interface_protocol = 0u;
    g_xhci_last_endpoint_max_packet = 0u;
    g_xhci_broad_mouse_probe_count = 0u;
    g_xhci_vendor_mouse_candidate_count = 0u;
    g_xhci_port_probe_attempts = 0u;
    g_xhci_port_probe_retry_skips = 0u;
    g_xhci_first_hid_port = 0u;
    g_xhci_first_hid_interface_class = 0u;
    g_xhci_first_hid_interface_subclass = 0u;
    g_xhci_first_hid_interface_protocol = 0u;
    g_xhci_first_mouse_candidate_port = 0u;
    g_xhci_first_mouse_candidate_interface_class = 0u;
    g_xhci_first_mouse_candidate_interface_subclass = 0u;
    g_xhci_first_mouse_candidate_interface_protocol = 0u;
    g_xhci_first_mouse_candidate_endpoint_mps = 0u;
    g_xhci_descriptor_probe_port = 0u;
    xhci64_zero_memory(g_xhci_port_probe_count, sizeof(g_xhci_port_probe_count));
#endif
    g_xhci_extcaps_scanned = 0u;
    g_xhci_legacy_cap_found = 0u;
    g_xhci_legacy_handoff = 0u;
    g_xhci_legacy_bios_owned_before = 0u;
    g_xhci_legacy_bios_owned_clear = 0u;
    g_xhci_legacy_os_owned = 0u;
    g_xhci_protocol_caps = 0u;
    g_xhci_usb2_ports = 0u;
    g_xhci_usb3_ports = 0u;
    g_xhci_prefer_usb2 = 0u;
    g_xhci_intel_cap_found = 0u;
    g_xhci_intel_workaround = 0u;
    g_xhci_intr_pending = 0u;
    g_xhci_mouse_intr_pending = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_last_enable_slot_completion = 0u;
    g_xhci_last_enable_slot_id = 0u;
    g_xhci_last_address_completion = 0u;
    g_xhci_last_address_slot = 0u;
    g_xhci_last_address_port = 0u;
    g_xhci_last_address_speed = 0u;
    g_xhci_last_address_event_dw0 = 0u;
    g_xhci_last_address_event_dw1 = 0u;
    g_xhci_last_address_event_dw2 = 0u;
    g_xhci_last_address_event_dw3 = 0u;
    g_xhci_pre_address_portsc = 0u;
    g_xhci_pre_address_portsc_pls = 0u;
    g_xhci_post_address_portsc = 0u;
    g_xhci_post_address_portsc_pls = 0u;
    g_xhci_address_failure_count = 0u;
    g_xhci_hid_interface_inventory = 0u;
    g_xhci_boot_mouse_interface = 0xFFFFFFFFu;
    g_xhci_boot_mouse_port = 0u;
    g_xhci_boot_mouse_dci = 0u;
    g_xhci_boot_mouse_mps = 0u;
    g_xhci_boot_mouse_configured = 0u;
    g_xhci_mouse_last_completion = 0u;
    g_xhci_mouse_last_event_dw0 = 0u;
    g_xhci_mouse_last_event_dw1 = 0u;
    g_xhci_mouse_last_event_dw2 = 0u;
    g_xhci_mouse_last_event_dw3 = 0u;
    g_xhci_last_disable_slot_completion = 0u;
    g_xhci_last_disable_slot_id = 0u;
#endif
    xhci64_zero_memory(g_xhci_port_protocol, sizeof(g_xhci_port_protocol));

    (void)g_xhci_address;
    (void)g_xhci_vendor_device;
    (void)g_xhci_class;
    (void)g_xhci_bar0;
    (void)g_xhci_bar1;
    (void)g_xhci_token;
}

void xhci64_init(void)
{
    u64 physical_base;
    u32 cap_register;
    u32 hcs_params1;
    u32 hcs_params2;
    u32 hcc_params1;

    if (xhci64_physical_base_valid() == 0u)
    {
        g_xhci_unavailable = 1u;
        return;
    }

    if (g_xhci_span_hint == 0u)
    {
        g_xhci_unavailable = 1u;
        g_xhci_error = 1u;
        return;
    }

    physical_base = ((u64)g_xhci_base_high << 32) | (u64)g_xhci_base_low;
    if (paging64_install_kernel_mmio_mapping(
            XHCI64_MAP_VIRTUAL_BASE,
            physical_base,
            XHCI64_MAP_PAGES) == 0u)
    {
        g_xhci_unavailable = 1u;
        g_xhci_error = 2u;
        return;
    }

    g_xhci_mapped = 1u;
    cap_register = xhci64_read32(0u);
    hcs_params1 = xhci64_read32(XHCI64_CAP_HCSPARAMS1);
    hcs_params2 = xhci64_read32(XHCI64_CAP_HCSPARAMS2);
    hcc_params1 = xhci64_read32(XHCI64_CAP_HCCPARAMS1);
    g_xhci_cap_length = cap_register & 0xFFu;
    g_xhci_max_slots = hcs_params1 & 0xFFu;
    g_xhci_hcs_ports = (hcs_params1 >> 24) & 0xFFu;
    g_xhci_context_size = ((hcc_params1 & 0x4u) != 0u) ? 64u : 32u;
    g_xhci_doorbell_offset = xhci64_read32(XHCI64_CAP_DBOFF) & ~0x3u;
    g_xhci_runtime_offset = xhci64_read32(XHCI64_CAP_RTSOFF) & ~0x1Fu;

    if ((g_xhci_cap_length == 0u)
        || (g_xhci_max_slots == 0u)
        || (g_xhci_hcs_ports == 0u)
        || (g_xhci_doorbell_offset == 0u)
        || (g_xhci_runtime_offset == 0u))
    {
        g_xhci_unavailable = 1u;
        g_xhci_error = 3u;
        return;
    }

    xhci64_scan_extended_capabilities(hcc_params1);
    if (g_xhci_legacy_handoff == 0u)
    {
        g_xhci_unavailable = 1u;
        g_xhci_error = 11u;
        return;
    }

    if (g_xhci_intel_workaround != 0u)
    {
        xhci64_delay_ms(XHCI64_DEVICE_SETTLE_MS);
    }

    if (xhci64_reset_and_run_controller(hcs_params2) == 0u)
    {
        g_xhci_unavailable = 1u;
        return;
    }

    xhci64_scan_ports();
    xhci64_enumerate_hid();
    g_xhci_unavailable = (g_xhci_hid_device != 0u) ? 0u : 1u;
    if ((g_xhci_hid_device == 0u) && (g_xhci_error == 0u))
    {
        g_xhci_error = 10u;
    }
}

void xhci64_set_live_polling_enabled(u32 enabled)
{
    g_xhci_live_polling_enabled =
        ((enabled != 0u) && (xhci64_live_polling_supported() != 0u)) ? 1u : 0u;
}

static void xhci64_rescan_hid_if_needed(void)
{
    if ((g_xhci_controller_running == 0u)
        || (g_xhci_mapped == 0u)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        || ((g_xhci_keyboard_endpoint.present != 0u)
            && (g_xhci_mouse_endpoint.present != 0u)
            && (g_xhci_storage_endpoint.present != 0u)))
#else
        || ((g_xhci_keyboard_endpoint.present != 0u) && (g_xhci_mouse_endpoint.present != 0u)))
#endif
    {
        return;
    }

    if (g_xhci_live_rescan_countdown != 0u)
    {
        --g_xhci_live_rescan_countdown;
        return;
    }

    g_xhci_live_rescan_countdown = 32u;
    (void)xhci64_rescan_devices();
    if (xhci64_live_polling_supported() != 0u)
    {
        g_xhci_live_polling_enabled = 1u;
        g_xhci_unavailable = 0u;
    }
}

void xhci64_poll_keyboard(void)
{
    struct xhci64_event event;
    u32 completion;

    xhci64_rescan_hid_if_needed();
    if (g_xhci_live_polling_enabled == 0u)
    {
        return;
    }

    if (g_xhci_keyboard_endpoint.present == 0u)
    {
        return;
    }

    xhci64_queue_keyboard_report();
    if (g_xhci_intr_pending == 0u)
    {
        return;
    }

    if (xhci64_poll_event_bounded(
            XHCI64_TRB_TYPE_TRANSFER_EVENT,
            0ull,
            g_xhci_keyboard_endpoint.slot_id,
            g_xhci_keyboard_endpoint.dci,
            XHCI64_LIVE_EVENT_POLL_LIMIT,
            &event) == 0u)
    {
        return;
    }

    completion = xhci64_completion_code(&event);
    g_xhci_intr_pending = 0u;
    if ((completion != XHCI64_COMPLETION_SUCCESS)
        && (completion != XHCI64_COMPLETION_SHORT_PACKET))
    {
        return;
    }

    input64_accept_usb_hid_boot_report(g_xhci_keyboard_report, sizeof(g_xhci_keyboard_report));
    ++g_xhci_report_count;
    g_xhci_report_bytes += sizeof(g_xhci_keyboard_report);
    if (g_xhci_report_count != 0u)
    {
        g_xhci_input_live = 1u;
    }
}

void xhci64_poll_mouse(void)
{
    struct xhci64_event event;
    u32 completion;

    xhci64_rescan_hid_if_needed();
    if (g_xhci_live_polling_enabled == 0u)
    {
        return;
    }

    if (g_xhci_mouse_endpoint.present == 0u)
    {
        return;
    }

    xhci64_queue_mouse_report();
    if (g_xhci_mouse_intr_pending == 0u)
    {
        return;
    }

    if (xhci64_poll_event_bounded(
            XHCI64_TRB_TYPE_TRANSFER_EVENT,
            0ull,
            g_xhci_mouse_endpoint.slot_id,
            g_xhci_mouse_endpoint.dci,
            XHCI64_LIVE_EVENT_POLL_LIMIT,
            &event) == 0u)
    {
        return;
    }

    completion = xhci64_completion_code(&event);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_xhci_mouse_last_completion = completion;
    g_xhci_mouse_last_event_dw0 = (u32)(event.parameter & 0xFFFFFFFFull);
    g_xhci_mouse_last_event_dw1 = (u32)(event.parameter >> 32);
    g_xhci_mouse_last_event_dw2 = event.status;
    g_xhci_mouse_last_event_dw3 = event.control;
#endif
    g_xhci_mouse_intr_pending = 0u;
    if ((completion != XHCI64_COMPLETION_SUCCESS)
        && (completion != XHCI64_COMPLETION_SHORT_PACKET))
    {
        return;
    }

    if (g_xhci_mouse_report_offset >= sizeof(g_xhci_mouse_report))
    {
        g_xhci_mouse_report_offset = 0u;
    }
    if (g_xhci_mouse_report_size <= g_xhci_mouse_report_offset)
    {
        g_xhci_mouse_report_size = 8u;
        g_xhci_mouse_report_offset = 0u;
    }
    input64_accept_usb_hid_mouse_report(
        &g_xhci_mouse_report[g_xhci_mouse_report_offset],
        g_xhci_mouse_report_size - g_xhci_mouse_report_offset);
    ++g_xhci_mouse_reports;
    g_xhci_mouse_report_bytes += g_xhci_mouse_report_size;
}

u32 xhci64_found(void)
{
    return ((g_xhci_flags & XHCI64_MMIO_FLAG_PRESENT) != 0u) ? 1u : 0u;
}

u64 xhci64_bar0(void)
{
    return ((u64)g_xhci_base_high << 32) | (u64)g_xhci_base_low;
}

u32 xhci64_mapped(void)
{
    return g_xhci_mapped;
}

u32 xhci64_cap_length(void)
{
    return g_xhci_cap_length;
}

u32 xhci64_hcs_ports(void)
{
    return g_xhci_hcs_ports;
}

u32 xhci64_ports_scanned(void)
{
    return g_xhci_ports_scanned;
}

u32 xhci64_connected_ports(void)
{
    return g_xhci_connected_ports;
}

u32 xhci64_command_ring_staged(void)
{
    return g_xhci_command_ring_staged;
}

u32 xhci64_dcbaa_staged(void)
{
    return g_xhci_dcbaa_staged;
}

u32 xhci64_event_ring_staged(void)
{
    return g_xhci_event_ring_staged;
}

u32 xhci64_controller_reset(void)
{
    return g_xhci_controller_reset;
}

u32 xhci64_controller_running(void)
{
    return g_xhci_controller_running;
}

u32 xhci64_slot_enabled(void)
{
    return g_xhci_slot_enabled;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 xhci64_slots_disabled(void)
{
    return g_xhci_slots_disabled;
}

u32 xhci64_slot_disable_failures(void)
{
    return g_xhci_slot_disable_failures;
}

u32 xhci64_last_disable_slot_completion(void)
{
    return g_xhci_last_disable_slot_completion;
}

u32 xhci64_last_disable_slot_id(void)
{
    return g_xhci_last_disable_slot_id;
}
#endif

u32 xhci64_addressed(void)
{
    return g_xhci_addressed;
}

u32 xhci64_config_read(void)
{
    return g_xhci_config_read;
}

u32 xhci64_hid_report_read(void)
{
    return g_xhci_hid_report_read;
}

u32 xhci64_endpoint_configured(void)
{
    return g_xhci_endpoint_configured;
}

u32 xhci64_keyboard_endpoint_present(void)
{
    return g_xhci_keyboard_endpoint.present;
}

u32 xhci64_keyboard_transfer_pending(void)
{
    return g_xhci_intr_pending;
}

u32 xhci64_hid_device(void)
{
    return g_xhci_hid_device;
}

u32 xhci64_input_live(void)
{
    return g_xhci_input_live;
}

u32 xhci64_report_count(void)
{
    return g_xhci_report_count;
}

u32 xhci64_report_bytes(void)
{
    return g_xhci_report_bytes;
}

u32 xhci64_mouse_device(void)
{
    return g_xhci_mouse_device;
}

u32 xhci64_mouse_endpoint_present(void)
{
    return g_xhci_mouse_endpoint.present;
}

u32 xhci64_mouse_transfer_pending(void)
{
    return g_xhci_mouse_intr_pending;
}

u32 xhci64_mouse_reports(void)
{
    return g_xhci_mouse_reports;
}

u32 xhci64_mouse_report_bytes(void)
{
    return g_xhci_mouse_report_bytes;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 xhci64_mouse_report_size(void)
{
    return g_xhci_mouse_report_size;
}
#endif

u32 xhci64_live_polling_enabled(void)
{
    return g_xhci_live_polling_enabled;
}

u32 xhci64_extcaps_scanned(void)
{
    return g_xhci_extcaps_scanned;
}

u32 xhci64_legacy_cap_found(void)
{
    return g_xhci_legacy_cap_found;
}

u32 xhci64_legacy_handoff(void)
{
    return g_xhci_legacy_handoff;
}

u32 xhci64_legacy_bios_owned_before(void)
{
    return g_xhci_legacy_bios_owned_before;
}

u32 xhci64_legacy_bios_owned_clear(void)
{
    return g_xhci_legacy_bios_owned_clear;
}

u32 xhci64_legacy_os_owned(void)
{
    return g_xhci_legacy_os_owned;
}

u32 xhci64_protocol_caps(void)
{
    return g_xhci_protocol_caps;
}

u32 xhci64_usb2_ports(void)
{
    return g_xhci_usb2_ports;
}

u32 xhci64_usb3_ports(void)
{
    return g_xhci_usb3_ports;
}

u32 xhci64_prefer_usb2(void)
{
    return g_xhci_prefer_usb2;
}

u32 xhci64_intel_cap_found(void)
{
    return g_xhci_intel_cap_found;
}

u32 xhci64_intel_workaround(void)
{
    return g_xhci_intel_workaround;
}

u32 xhci64_port_reset_wait_ms(void)
{
    return XHCI64_PORT_RESET_WAIT_MS;
}

u32 xhci64_device_settle_ms(void)
{
    return XHCI64_DEVICE_SETTLE_MS;
}

u32 xhci64_unavailable(void)
{
    return g_xhci_unavailable;
}

u32 xhci64_error(void)
{
    return g_xhci_error;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 xhci64_last_skip_port(void)
{
    return g_xhci_last_skip_port;
}

u32 xhci64_last_skip_code(void)
{
    return g_xhci_last_skip_code;
}

u32 xhci64_last_device_class(void)
{
    return g_xhci_last_device_class;
}

u32 xhci64_last_device_subclass(void)
{
    return g_xhci_last_device_subclass;
}

u32 xhci64_last_device_protocol(void)
{
    return g_xhci_last_device_protocol;
}

u32 xhci64_last_config_total_length(void)
{
    return g_xhci_last_config_total_length;
}

u32 xhci64_last_interface_class(void)
{
    return g_xhci_last_interface_class;
}

u32 xhci64_last_interface_subclass(void)
{
    return g_xhci_last_interface_subclass;
}

u32 xhci64_last_interface_protocol(void)
{
    return g_xhci_last_interface_protocol;
}

u32 xhci64_last_endpoint_max_packet(void)
{
    return g_xhci_last_endpoint_max_packet;
}

u32 xhci64_broad_mouse_probe_count(void)
{
    return g_xhci_broad_mouse_probe_count;
}

u32 xhci64_vendor_mouse_candidate_count(void)
{
    return g_xhci_vendor_mouse_candidate_count;
}

u32 xhci64_max_slots_limit(void)
{
    return XHCI64_MAX_SLOTS;
}

u32 xhci64_disable_slot_poll_limit(void)
{
    return XHCI64_DISABLE_SLOT_POLL_LIMIT;
}

u32 xhci64_last_enable_slot_completion(void)
{
    return g_xhci_last_enable_slot_completion;
}

u32 xhci64_last_enable_slot_id(void)
{
    return g_xhci_last_enable_slot_id;
}

u32 xhci64_last_address_completion(void)
{
    return g_xhci_last_address_completion;
}

u32 xhci64_last_address_slot(void)
{
    return g_xhci_last_address_slot;
}

u32 xhci64_last_address_port(void)
{
    return g_xhci_last_address_port;
}

u32 xhci64_last_address_speed(void)
{
    return g_xhci_last_address_speed;
}

u32 xhci64_last_address_event_dw0(void)
{
    return g_xhci_last_address_event_dw0;
}

u32 xhci64_last_address_event_dw1(void)
{
    return g_xhci_last_address_event_dw1;
}

u32 xhci64_last_address_event_dw2(void)
{
    return g_xhci_last_address_event_dw2;
}

u32 xhci64_last_address_event_dw3(void)
{
    return g_xhci_last_address_event_dw3;
}

u32 xhci64_pre_address_portsc(void)
{
    return g_xhci_pre_address_portsc;
}

u32 xhci64_pre_address_portsc_pls(void)
{
    return g_xhci_pre_address_portsc_pls;
}

u32 xhci64_post_address_portsc(void)
{
    return g_xhci_post_address_portsc;
}

u32 xhci64_post_address_portsc_pls(void)
{
    return g_xhci_post_address_portsc_pls;
}

u32 xhci64_address_failure_count(void)
{
    return g_xhci_address_failure_count;
}

u32 xhci64_hid_interface_inventory(void)
{
    return g_xhci_hid_interface_inventory;
}

u32 xhci64_boot_mouse_interface(void)
{
    return g_xhci_boot_mouse_interface;
}

u32 xhci64_boot_mouse_port(void)
{
    return g_xhci_boot_mouse_port;
}

u32 xhci64_boot_mouse_dci(void)
{
    return g_xhci_boot_mouse_dci;
}

u32 xhci64_boot_mouse_mps(void)
{
    return g_xhci_boot_mouse_mps;
}

u32 xhci64_boot_mouse_configured(void)
{
    return g_xhci_boot_mouse_configured;
}

u32 xhci64_mouse_last_completion(void)
{
    return g_xhci_mouse_last_completion;
}

u32 xhci64_mouse_last_event_dw0(void)
{
    return g_xhci_mouse_last_event_dw0;
}

u32 xhci64_mouse_last_event_dw1(void)
{
    return g_xhci_mouse_last_event_dw1;
}

u32 xhci64_mouse_last_event_dw2(void)
{
    return g_xhci_mouse_last_event_dw2;
}

u32 xhci64_mouse_last_event_dw3(void)
{
    return g_xhci_mouse_last_event_dw3;
}

u32 xhci64_port_probe_attempts(void)
{
    return g_xhci_port_probe_attempts;
}

u32 xhci64_port_probe_retry_skips(void)
{
    return g_xhci_port_probe_retry_skips;
}

u32 xhci64_first_hid_port(void)
{
    return g_xhci_first_hid_port;
}

u32 xhci64_first_hid_interface_class(void)
{
    return g_xhci_first_hid_interface_class;
}

u32 xhci64_first_hid_interface_subclass(void)
{
    return g_xhci_first_hid_interface_subclass;
}

u32 xhci64_first_hid_interface_protocol(void)
{
    return g_xhci_first_hid_interface_protocol;
}

u32 xhci64_first_mouse_candidate_port(void)
{
    return g_xhci_first_mouse_candidate_port;
}

u32 xhci64_first_mouse_candidate_interface_class(void)
{
    return g_xhci_first_mouse_candidate_interface_class;
}

u32 xhci64_first_mouse_candidate_interface_subclass(void)
{
    return g_xhci_first_mouse_candidate_interface_subclass;
}

u32 xhci64_first_mouse_candidate_interface_protocol(void)
{
    return g_xhci_first_mouse_candidate_interface_protocol;
}

u32 xhci64_first_mouse_candidate_endpoint_mps(void)
{
    return g_xhci_first_mouse_candidate_endpoint_mps;
}

u32 xhci64_port_protocol(u32 port_id)
{
    if ((port_id == 0u) || (port_id > 32u))
    {
        return 0u;
    }

    return g_xhci_port_protocol[port_id - 1u];
}

u32 xhci64_portsc_snapshot(u32 port_id)
{
    return xhci64_read_portsc_snapshot(port_id);
}

u32 xhci64_portsc_snapshot_pls(u32 port_id)
{
    return xhci64_portsc_pls(xhci64_read_portsc_snapshot(port_id));
}

u32 xhci64_usb_storage_present(void)
{
    return g_xhci_usb_storage_present;
}

u32 xhci64_usb_storage_ready(void)
{
    return g_xhci_usb_storage_ready;
}

u32 xhci64_usb_storage_block_bytes(void)
{
    return g_xhci_usb_storage_block_bytes;
}

u32 xhci64_usb_storage_last_lba(void)
{
    return g_xhci_usb_storage_last_lba;
}

u32 xhci64_usb_storage_error(void)
{
    return g_xhci_usb_storage_error;
}

u32 xhci64_usb_storage_read_count(void)
{
    return g_xhci_usb_storage_read_count;
}

u32 xhci64_usb_storage_write_count(void)
{
    return g_xhci_usb_storage_write_count;
}

u32 xhci64_usb_storage_last_completion(void)
{
    return g_xhci_usb_storage_last_completion;
}

u32 xhci64_usb_storage_read_sector(u32 lba, u8 *destination, u32 byte_count)
{
    u8 command[16];

    if ((g_xhci_usb_storage_ready == 0u)
        || (destination == (u8 *)0)
        || (byte_count != XHCI64_USB_STORAGE_SECTOR_BYTES))
    {
        g_xhci_usb_storage_error = 40u;
        return 0u;
    }

    xhci64_zero_memory(command, sizeof(command));
    command[0u] = XHCI64_SCSI_READ_10;
    command[2u] = (u8)((lba >> 24) & 0xFFu);
    command[3u] = (u8)((lba >> 16) & 0xFFu);
    command[4u] = (u8)((lba >> 8) & 0xFFu);
    command[5u] = (u8)(lba & 0xFFu);
    command[8u] = 1u;
    if (xhci64_storage_bot_command(command, 10u, destination, byte_count, 1u) == 0u)
    {
        return 0u;
    }

    ++g_xhci_usb_storage_read_count;
    return 1u;
}

u32 xhci64_usb_storage_write_sector(u32 lba, const u8 *source, u32 byte_count)
{
    u8 command[16];

    if ((g_xhci_usb_storage_ready == 0u)
        || (source == (const u8 *)0)
        || (byte_count != XHCI64_USB_STORAGE_SECTOR_BYTES))
    {
        g_xhci_usb_storage_error = 41u;
        return 0u;
    }

    xhci64_zero_memory(command, sizeof(command));
    command[0u] = XHCI64_SCSI_WRITE_10;
    command[2u] = (u8)((lba >> 24) & 0xFFu);
    command[3u] = (u8)((lba >> 16) & 0xFFu);
    command[4u] = (u8)((lba >> 8) & 0xFFu);
    command[5u] = (u8)(lba & 0xFFu);
    command[8u] = 1u;
    if (xhci64_storage_bot_command(command, 10u, (void *)source, byte_count, 0u) == 0u)
    {
        return 0u;
    }

    ++g_xhci_usb_storage_write_count;
    return 1u;
}
#endif
