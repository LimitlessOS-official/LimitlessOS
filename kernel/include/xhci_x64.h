#ifndef LIMITLESS_XHCI_X64_H
#define LIMITLESS_XHCI_X64_H

#include "types.h"

#define XHCI64_MMIO_FLAG_PRESENT 0x00000001u
#define XHCI64_MMIO_FLAG_MEMORY_BAR 0x00000002u
#define XHCI64_MMIO_FLAG_64BIT_BAR 0x00000004u
#define XHCI64_MMIO_FLAG_BASE_NONZERO 0x00000008u
#define XHCI64_MMIO_FLAG_PAGE_ALIGNED 0x00000010u
#define XHCI64_MMIO_FLAG_MAPPING_REQUIRED 0x00000020u
#define XHCI64_MMIO_FLAG_BROKER_PRIVATE 0x00000040u

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
    u32 token);
void xhci64_init(void);
void xhci64_set_live_polling_enabled(u32 enabled);
u32 xhci64_live_polling_supported(void);
void xhci64_poll_keyboard(void);
void xhci64_poll_mouse(void);

u32 xhci64_found(void);
u64 xhci64_bar0(void);
u32 xhci64_mapped(void);
u32 xhci64_cap_length(void);
u32 xhci64_hcs_ports(void);
u32 xhci64_ports_scanned(void);
u32 xhci64_connected_ports(void);
u32 xhci64_command_ring_staged(void);
u32 xhci64_dcbaa_staged(void);
u32 xhci64_event_ring_staged(void);
u32 xhci64_controller_reset(void);
u32 xhci64_controller_running(void);
u32 xhci64_slot_enabled(void);
u32 xhci64_addressed(void);
u32 xhci64_config_read(void);
u32 xhci64_hid_report_read(void);
u32 xhci64_endpoint_configured(void);
u32 xhci64_keyboard_endpoint_present(void);
u32 xhci64_keyboard_transfer_pending(void);
u32 xhci64_hid_device(void);
u32 xhci64_input_live(void);
u32 xhci64_report_count(void);
u32 xhci64_report_bytes(void);
u32 xhci64_mouse_device(void);
u32 xhci64_mouse_endpoint_present(void);
u32 xhci64_mouse_transfer_pending(void);
u32 xhci64_mouse_reports(void);
u32 xhci64_mouse_report_bytes(void);
u32 xhci64_live_polling_enabled(void);
u32 xhci64_extcaps_scanned(void);
u32 xhci64_legacy_cap_found(void);
u32 xhci64_legacy_handoff(void);
u32 xhci64_legacy_bios_owned_before(void);
u32 xhci64_legacy_bios_owned_clear(void);
u32 xhci64_legacy_os_owned(void);
u32 xhci64_protocol_caps(void);
u32 xhci64_usb2_ports(void);
u32 xhci64_usb3_ports(void);
u32 xhci64_prefer_usb2(void);
u32 xhci64_intel_cap_found(void);
u32 xhci64_intel_workaround(void);
u32 xhci64_port_reset_wait_ms(void);
u32 xhci64_device_settle_ms(void);
u32 xhci64_unavailable(void);
u32 xhci64_error(void);
u32 xhci64_last_skip_port(void);
u32 xhci64_last_skip_code(void);
u32 xhci64_last_device_class(void);
u32 xhci64_last_device_subclass(void);
u32 xhci64_last_device_protocol(void);
u32 xhci64_last_config_total_length(void);
u32 xhci64_last_interface_class(void);
u32 xhci64_last_interface_subclass(void);
u32 xhci64_last_interface_protocol(void);
u32 xhci64_last_endpoint_max_packet(void);
u32 xhci64_broad_mouse_probe_count(void);

#endif
