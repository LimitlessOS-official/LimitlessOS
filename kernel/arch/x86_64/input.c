#include "input_x64.h"

#include "arch_build.h"
#include "capability_x64.h"
#include "display_x64.h"
#include "i2c_hid_x64.h"
#include "launch_x64.h"
#include "pci_x64.h"
#include "serial.h"
#include "services.h"
#include "services_x64.h"
#include "x64.h"
#include "xhci_x64.h"

#define INPUT64_MAX_READ_BYTES 128u
#define INPUT64_KEYBOARD_QUEUE_CAPACITY 256u
#define INPUT64_MOUSE_QUEUE_CAPACITY 64u
#define INPUT64_MOUSE_RECORD_BYTES 20u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define INPUT64_HID_USAGE_PAGE_GENERIC_DESKTOP 0x01u
#define INPUT64_HID_USAGE_PAGE_BUTTON 0x09u
#define INPUT64_HID_USAGE_X 0x30u
#define INPUT64_HID_USAGE_Y 0x31u
#define INPUT64_HID_USAGE_WHEEL 0x38u
#define INPUT64_HID_MOUSE_USAGE_SLOTS 8u
#define INPUT64_HID_MOUSE_INVALID_BIT 0xFFFFFFFFu
#endif
#define INPUT64_KERNEL_HIGH_BASE_HIGH32 0xFFFFFFFFu
#define INPUT64_KERNEL_HIGH_BASE_LOW32 0x80000000u
#define INPUT64_PS2_STATUS_PORT 0x64u
#define INPUT64_PS2_DATA_PORT 0x60u
#define INPUT64_PS2_STATUS_OUTPUT_READY 0x01u
#define INPUT64_PS2_STATUS_INPUT_FULL 0x02u
#define INPUT64_PS2_STATUS_AUX_DATA 0x20u
#define INPUT64_PS2_DRAIN_LIMIT 32u
#define INPUT64_PS2_WAIT_LIMIT 100000u
#define INPUT64_PS2_MOUSE_ENABLE_RETRIES 5u
#define INPUT64_PS2_ACK 0xFAu
#define INPUT64_PS2_SELF_TEST_OK 0x55u
#define INPUT64_PS2_FIRST_PORT_OK 0x00u
#define INPUT64_PS2_DEVICE_SELF_TEST_OK 0xAAu
#define INPUT64_PS2_COMMAND_READ_CONFIG 0x20u
#define INPUT64_PS2_COMMAND_WRITE_CONFIG 0x60u
#define INPUT64_PS2_COMMAND_CONTROLLER_SELF_TEST 0xAAu
#define INPUT64_PS2_COMMAND_TEST_FIRST_PORT 0xABu
#define INPUT64_PS2_COMMAND_DISABLE_SECOND_PORT 0xA7u
#define INPUT64_PS2_COMMAND_DISABLE_FIRST_PORT 0xADu
#define INPUT64_PS2_COMMAND_ENABLE_SECOND_PORT 0xA8u
#define INPUT64_PS2_COMMAND_ENABLE_FIRST_PORT 0xAEu
#define INPUT64_PS2_COMMAND_WRITE_SECOND_PORT 0xD4u
#define INPUT64_PS2_DEVICE_RESET 0xFFu
#define INPUT64_PS2_DEVICE_ENABLE_SCANNING 0xF4u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define INPUT64_PS2_DEVICE_GET_ID 0xF2u
#define INPUT64_PS2_DEVICE_SET_SAMPLE_RATE 0xF3u
#endif
#define INPUT64_PS2_CONFIG_IRQ1 0x01u
#define INPUT64_PS2_CONFIG_IRQ12 0x02u
#define INPUT64_PS2_CONFIG_FIRST_PORT_CLOCK_DISABLE 0x10u
#define INPUT64_PS2_CONFIG_SECOND_PORT_CLOCK_DISABLE 0x20u
#define INPUT64_PS2_CONFIG_TRANSLATION 0x40u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define INPUT64_PS2_MOUSE_PACKET_BYTES 4u
#else
#define INPUT64_PS2_MOUSE_PACKET_BYTES 3u
#endif

struct input64_mouse_event
{
    s32 dx;
    s32 dy;
    u32 buttons;
    u32 x;
    u32 y;
};

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
struct input64_hid_mouse_layout
{
    u32 ready;
    u32 report_id;
    u32 report_bytes;
    u32 buttons_bit_offset;
    u32 buttons_bit_count;
    u32 x_bit_offset;
    u32 x_bit_count;
    u32 y_bit_offset;
    u32 y_bit_count;
    u32 wheel_bit_offset;
    u32 wheel_bit_count;
};
#endif

static u32 g_read_count = 0u;
static u32 g_byte_count = 0u;
static u32 g_denial_count = 0u;
static u32 g_eof_count = 0u;
static u32 g_line_count = 0u;
static u32 g_edit_count = 0u;
static u8 g_keyboard_queue[INPUT64_KEYBOARD_QUEUE_CAPACITY];
static u32 g_keyboard_head = 0u;
static u32 g_keyboard_tail = 0u;
static u32 g_keyboard_pending = 0u;
static u32 g_keyboard_irq_count = 0u;
static u32 g_keyboard_poll_count = 0u;
static u32 g_keyboard_scancode_count = 0u;
static u32 g_keyboard_byte_count = 0u;
static u32 g_keyboard_drop_count = 0u;
static u32 g_keyboard_last_scancode = 0u;
static u32 g_keyboard_last_byte = 0u;
static u32 g_keyboard_read_count = 0u;
static u32 g_keyboard_read_byte_count = 0u;
static u32 g_keyboard_line_count = 0u;
static u32 g_keyboard_line_byte_count = 0u;
static u32 g_keyboard_line_edit_count = 0u;
static u32 g_ps2_status_snapshot = 0u;
static u32 g_ps2_present = 0u;
static u32 g_ps2_enable_attempted = 0u;
static u32 g_ps2_enabled = 0u;
static u32 g_ps2_enable_status = 0u;
static u32 g_ps2_config_byte = 0u;
static u32 g_ps2_device_ack = 0u;
static u32 g_ps2_scanning_enabled = 0u;
static u32 g_ps2_recommended_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
static u32 g_ps2_self_test = 0u;
static u32 g_ps2_first_port_test = 0u;
static u32 g_ps2_reset_ack = 0u;
static u32 g_ps2_reset_self_test = 0u;
static u8 g_keyboard_extended_prefix = 0u;
static u8 g_keyboard_break_prefix = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u8 g_keyboard_left_shift = 0u;
static u8 g_keyboard_right_shift = 0u;
static u8 g_keyboard_caps_lock = 0u;
static u8 g_usb_hid_last_modifier = 0u;
#endif
static u32 g_keyboard_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
static u8 g_usb_hid_last_keys[6];
static struct input64_mouse_event g_mouse_queue[INPUT64_MOUSE_QUEUE_CAPACITY];
static u32 g_mouse_head = 0u;
static u32 g_mouse_tail = 0u;
static u32 g_mouse_pending = 0u;
static u32 g_mouse_found = 0u;
static u32 g_mouse_enabled = 0u;
static u32 g_mouse_irq_count = 0u;
static u32 g_mouse_poll_count = 0u;
static u32 g_mouse_packet_count = 0u;
static u32 g_mouse_drop_count = 0u;
static u32 g_mouse_delta_seen = 0u;
static u32 g_mouse_button_seen = 0u;
static u32 g_mouse_buttons = 0u;
static u32 g_mouse_x = 0u;
static u32 g_mouse_y = 0u;
static u32 g_mouse_width = 1024u;
static u32 g_mouse_height = 768u;
static s32 g_mouse_last_dx = 0;
static s32 g_mouse_last_dy = 0;
static u32 g_mouse_read_count = 0u;
static u32 g_mouse_read_packet_count = 0u;
static u32 g_ps2_mouse_aux_enabled = 0u;
static u32 g_ps2_mouse_config_read = 0u;
static u32 g_ps2_mouse_config_write = 0u;
static u32 g_ps2_mouse_irq12_configured = 0u;
static u32 g_ps2_mouse_enable_command = 0u;
static u32 g_ps2_mouse_present = 0u;
static u32 g_ps2_mouse_ack = 0u;
static u8 g_ps2_mouse_packet[INPUT64_PS2_MOUSE_PACKET_BYTES];
static u32 g_ps2_mouse_phase = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_ps2_mouse_packet_bytes = 3u;
static u32 g_ps2_mouse_wheel_enabled = 0u;
static u32 g_ps2_mouse_wheel_count = 0u;
#endif
static u32 g_ps2_mouse_last_raw_byte = 0u;
static u32 g_ps2_mouse_bad_start_count = 0u;
static u32 g_ps2_mouse_raw_log_count = 0u;
static u32 g_mouse_diag_valid = 0u;
static u32 g_mouse_diag_init_done = 0u;
static u32 g_mouse_diag_aux_enabled = 0u;
static u32 g_mouse_diag_config_read = 0u;
static u32 g_mouse_diag_config_write = 0u;
static u32 g_mouse_diag_irq12_configured = 0u;
static u32 g_mouse_diag_enable_command = 0u;
static u32 g_mouse_diag_ack = 0u;
static u32 g_mouse_diag_irq_count = 0u;
static u32 g_mouse_diag_packet_count = 0u;
static u32 g_mouse_diag_pending_count = 0u;
static u32 g_mouse_diag_x = 0u;
static u32 g_mouse_diag_y = 0u;
static u32 g_mouse_diag_buttons = 0u;
static u32 g_mouse_diag_raw_byte = 0u;
static u32 g_mouse_diag_bad_starts = 0u;
static u32 g_mouse_diag_xhci_keyboard_endpoint = 0u;
static u32 g_mouse_diag_xhci_keyboard_pending = 0u;
static u32 g_mouse_diag_xhci_keyboard_reports = 0u;
static u32 g_mouse_diag_xhci_mouse_endpoint = 0u;
static u32 g_mouse_diag_xhci_mouse_pending = 0u;
static u32 g_mouse_diag_xhci_mouse_reports = 0u;
static u32 g_mouse_diag_xhci_live_enabled = 0u;
static u32 g_mouse_diag_i2c_keyboard_found = 0u;
static u32 g_mouse_diag_i2c_keyboard_reports = 0u;
static u32 g_mouse_diag_i2c_keyboard_error = 0u;
static u32 g_mouse_diag_i2c_pointer_found = 0u;
static u32 g_mouse_diag_i2c_pointer_reports = 0u;
static u32 g_mouse_diag_i2c_pointer_error = 0u;
static u32 g_i2c_touchpad_contact_active = 0u;
static u32 g_i2c_touchpad_last_x = 0u;
static u32 g_i2c_touchpad_last_y = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static struct input64_hid_mouse_layout g_usb_hid_mouse_layout;
static u32 g_usb_hid_mouse_layout_reports = 0u;
static u32 g_usb_hid_mouse_layout_fallbacks = 0u;
#endif

static void input64_copy(void *destination, const void *source, u32 byte_count)
{
    u8 *dest = (u8 *)destination;
    const u8 *src = (const u8 *)source;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        dest[index] = src[index];
    }
}

static int input64_range_overflows(u64 address, u32 byte_count)
{
    u64 end;

    if (byte_count == 0u)
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return end < address;
}

static int input64_address_is_kernel_high(u64 address, u32 byte_count)
{
    u64 end;

    if (input64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return ((u32)(address >> 32) >= INPUT64_KERNEL_HIGH_BASE_HIGH32)
        && ((u32)address >= INPUT64_KERNEL_HIGH_BASE_LOW32)
        && (end >= address);
}

static int input64_address_is_user_stack(u64 address, u32 byte_count)
{
    u64 stack_base = (u64)(LAUNCH64_USER_STACK_TOP - LAUNCH64_USER_STACK_BYTES);
    u64 stack_top = (u64)LAUNCH64_USER_STACK_TOP;
    u64 end;

    if (input64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= stack_base) && (end <= stack_top);
}

static int input64_address_writable(u64 address, u32 byte_count)
{
    if (byte_count == 0u)
    {
        return 1;
    }

    if (address == 0ull)
    {
        return 0;
    }

    return input64_address_is_kernel_high(address, byte_count)
        || input64_address_is_user_stack(address, byte_count);
}

static u32 input64_deny(void)
{
    ++g_denial_count;
    return INPUT64_INVALID_RESULT;
}

static void input64_keyboard_reset(void)
{
    u32 index;

    g_keyboard_head = 0u;
    g_keyboard_tail = 0u;
    g_keyboard_pending = 0u;
    g_keyboard_irq_count = 0u;
    g_keyboard_poll_count = 0u;
    g_keyboard_scancode_count = 0u;
    g_keyboard_byte_count = 0u;
    g_keyboard_drop_count = 0u;
    g_keyboard_last_scancode = 0u;
    g_keyboard_last_byte = 0u;
    g_keyboard_read_count = 0u;
    g_keyboard_read_byte_count = 0u;
    g_keyboard_line_count = 0u;
    g_keyboard_line_byte_count = 0u;
    g_keyboard_line_edit_count = 0u;
    g_ps2_status_snapshot = 0u;
    g_keyboard_extended_prefix = 0u;
    g_keyboard_break_prefix = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_keyboard_left_shift = 0u;
    g_keyboard_right_shift = 0u;
    g_keyboard_caps_lock = 0u;
#endif
    for (index = 0u; index < 6u; ++index)
    {
        g_usb_hid_last_keys[index] = 0u;
    }
}

static void input64_keyboard_clear_pending(void)
{
    g_keyboard_head = 0u;
    g_keyboard_tail = 0u;
    g_keyboard_pending = 0u;
    g_keyboard_extended_prefix = 0u;
    g_keyboard_break_prefix = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_keyboard_left_shift = 0u;
    g_keyboard_right_shift = 0u;
#endif
}

void input64_clear_keyboard_pending(void)
{
    input64_keyboard_clear_pending();
}

static void input64_keyboard_enqueue_byte(u8 value)
{
#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
    if (display64_wm_process_keyboard_event(value) == 0u)
    {
        return;
    }
#endif

    if (g_keyboard_pending >= INPUT64_KEYBOARD_QUEUE_CAPACITY)
    {
        ++g_keyboard_drop_count;
        return;
    }

    g_keyboard_queue[g_keyboard_tail] = value;
    g_keyboard_tail = (g_keyboard_tail + 1u) % INPUT64_KEYBOARD_QUEUE_CAPACITY;
    ++g_keyboard_pending;
    ++g_keyboard_byte_count;
    g_keyboard_last_byte = value;
}

static void input64_keyboard_enqueue_sequence(const u8 *bytes, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        input64_keyboard_enqueue_byte(bytes[index]);
    }
}

static void input64_keyboard_enqueue_csi_final(u8 final_byte)
{
    const u8 sequence[3] = { 27u, (u8)'[', final_byte };
    input64_keyboard_enqueue_sequence(sequence, 3u);
}

static void input64_keyboard_enqueue_delete_sequence(void)
{
    const u8 sequence[4] = { 27u, (u8)'[', (u8)'3', (u8)'~' };
    input64_keyboard_enqueue_sequence(sequence, 4u);
}

static u32 input64_native_pointer_present(void)
{
    return ((i2c_hid64_pointer_found() != 0u)
        || (xhci64_mouse_endpoint_present() != 0u)
        || (xhci64_mouse_reports() != 0u)) ? 1u : 0u;
}

static s32 input64_sign_extend_byte(u8 value)
{
    return ((value & 0x80u) != 0u) ? (s32)((u32)value | 0xFFFFFF00u) : (s32)value;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void input64_usb_hid_mouse_layout_reset(void)
{
    g_usb_hid_mouse_layout.ready = 0u;
    g_usb_hid_mouse_layout.report_id = 0u;
    g_usb_hid_mouse_layout.report_bytes = 0u;
    g_usb_hid_mouse_layout.buttons_bit_offset = INPUT64_HID_MOUSE_INVALID_BIT;
    g_usb_hid_mouse_layout.buttons_bit_count = 0u;
    g_usb_hid_mouse_layout.x_bit_offset = INPUT64_HID_MOUSE_INVALID_BIT;
    g_usb_hid_mouse_layout.x_bit_count = 0u;
    g_usb_hid_mouse_layout.y_bit_offset = INPUT64_HID_MOUSE_INVALID_BIT;
    g_usb_hid_mouse_layout.y_bit_count = 0u;
    g_usb_hid_mouse_layout.wheel_bit_offset = INPUT64_HID_MOUSE_INVALID_BIT;
    g_usb_hid_mouse_layout.wheel_bit_count = 0u;
}

static u32 input64_hid_short_item_value(const u8 *bytes, u32 byte_count)
{
    u32 value = 0u;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        value |= ((u32)bytes[index]) << (index * 8u);
    }

    return value;
}

static u32 input64_hid_field_value(
    const u8 *report,
    u32 report_bytes,
    u32 bit_offset,
    u32 bit_count)
{
    u32 value = 0u;
    u32 bit_index;

    if ((report == 0) || (bit_count == 0u) || (bit_count > 31u))
    {
        return 0u;
    }

    if ((bit_offset + bit_count) > (report_bytes * 8u))
    {
        return 0u;
    }

    for (bit_index = 0u; bit_index < bit_count; ++bit_index)
    {
        u32 absolute_bit = bit_offset + bit_index;
        u32 byte_index = absolute_bit / 8u;
        u32 bit_in_byte = absolute_bit % 8u;
        if ((report[byte_index] & (u8)(1u << bit_in_byte)) != 0u)
        {
            value |= 1u << bit_index;
        }
    }

    return value;
}

static s32 input64_hid_signed_field(
    const u8 *report,
    u32 report_bytes,
    u32 bit_offset,
    u32 bit_count)
{
    u32 value = input64_hid_field_value(report, report_bytes, bit_offset, bit_count);
    u32 sign_bit;
    u32 extend_mask;

    if ((bit_count == 0u) || (bit_count >= 32u))
    {
        return (s32)value;
    }

    sign_bit = 1u << (bit_count - 1u);
    if ((value & sign_bit) == 0u)
    {
        return (s32)value;
    }

    extend_mask = 0xFFFFFFFFu << bit_count;
    return (s32)(value | extend_mask);
}

static u32 input64_hid_local_usage(
    const u32 *usages,
    u32 usage_count,
    u32 usage_min,
    u32 usage_max,
    u32 index)
{
    if (index < usage_count)
    {
        return usages[index];
    }

    if ((usage_max >= usage_min) && ((usage_min + index) <= usage_max))
    {
        return usage_min + index;
    }

    return 0u;
}
#endif

static s32 input64_ps2_mouse_delta(u8 flags, u8 value, u8 sign_bit)
{
    return ((flags & sign_bit) != 0u) ? (s32)((u32)value | 0xFFFFFF00u) : (s32)value;
}

static void input64_serial_hex_nibble(u8 value)
{
    value &= 0x0Fu;
    if (value < 10u)
    {
        serial_write_char((char)((u8)'0' + value));
        return;
    }

    serial_write_char((char)((u8)'A' + (value - 10u)));
}

static void input64_serial_hex_byte(u8 value)
{
    input64_serial_hex_nibble((u8)(value >> 4));
    input64_serial_hex_nibble(value);
}

static u32 input64_mouse_clamp_axis(s32 value, u32 limit)
{
    if (limit == 0u)
    {
        return 0u;
    }

    if (value < 0)
    {
        return 0u;
    }

    if ((u32)value >= limit)
    {
        return limit - 1u;
    }

    return (u32)value;
}

static void input64_mouse_reset(void)
{
    g_mouse_head = 0u;
    g_mouse_tail = 0u;
    g_mouse_pending = 0u;
    g_mouse_found = 0u;
    g_mouse_enabled = 0u;
    g_mouse_irq_count = 0u;
    g_mouse_poll_count = 0u;
    g_mouse_packet_count = 0u;
    g_mouse_drop_count = 0u;
    g_mouse_delta_seen = 0u;
    g_mouse_button_seen = 0u;
    g_mouse_buttons = 0u;
    g_mouse_x = g_mouse_width / 2u;
    g_mouse_y = g_mouse_height / 2u;
    g_mouse_last_dx = 0;
    g_mouse_last_dy = 0;
    g_mouse_read_count = 0u;
    g_mouse_read_packet_count = 0u;
    g_ps2_mouse_aux_enabled = 0u;
    g_ps2_mouse_config_read = 0u;
    g_ps2_mouse_config_write = 0u;
    g_ps2_mouse_irq12_configured = 0u;
    g_ps2_mouse_enable_command = 0u;
    g_ps2_mouse_present = 0u;
    g_ps2_mouse_ack = 0u;
    g_ps2_mouse_packet[0] = 0u;
    g_ps2_mouse_packet[1] = 0u;
    g_ps2_mouse_packet[2] = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_ps2_mouse_packet[3] = 0u;
    g_ps2_mouse_packet_bytes = 3u;
    g_ps2_mouse_wheel_enabled = 0u;
    g_ps2_mouse_wheel_count = 0u;
#endif
    g_ps2_mouse_phase = 0u;
    g_ps2_mouse_last_raw_byte = 0u;
    g_ps2_mouse_bad_start_count = 0u;
    g_ps2_mouse_raw_log_count = 0u;
    g_i2c_touchpad_contact_active = 0u;
    g_i2c_touchpad_last_x = 0u;
    g_i2c_touchpad_last_y = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    input64_usb_hid_mouse_layout_reset();
    g_usb_hid_mouse_layout_reports = 0u;
    g_usb_hid_mouse_layout_fallbacks = 0u;
#endif
    g_mouse_diag_valid = 0u;
}

static void input64_mouse_publish_diagnostics(void)
{
    u32 xhci_keyboard_endpoint = xhci64_keyboard_endpoint_present();
    u32 xhci_keyboard_pending = xhci64_keyboard_transfer_pending();
    u32 xhci_keyboard_reports = xhci64_report_count();
    u32 xhci_mouse_endpoint = xhci64_mouse_endpoint_present();
    u32 xhci_mouse_pending = xhci64_mouse_transfer_pending();
    u32 xhci_mouse_reports = xhci64_mouse_reports();
    u32 xhci_live_enabled = xhci64_live_polling_enabled();
    u32 i2c_keyboard_found = i2c_hid64_device_found();
    u32 i2c_keyboard_reports = i2c_hid64_report_count();
    u32 i2c_keyboard_error = i2c_hid64_error();
    u32 i2c_pointer_found = i2c_hid64_pointer_found();
    u32 i2c_pointer_reports = i2c_hid64_pointer_report_count();
    u32 i2c_pointer_error = i2c_hid64_pointer_error();

    if ((g_mouse_diag_valid != 0u)
        && (g_mouse_diag_init_done == g_mouse_enabled)
        && (g_mouse_diag_aux_enabled == g_ps2_mouse_aux_enabled)
        && (g_mouse_diag_config_read == g_ps2_mouse_config_read)
        && (g_mouse_diag_config_write == g_ps2_mouse_config_write)
        && (g_mouse_diag_irq12_configured == g_ps2_mouse_irq12_configured)
        && (g_mouse_diag_enable_command == g_ps2_mouse_enable_command)
        && (g_mouse_diag_ack == g_ps2_mouse_ack)
        && (g_mouse_diag_irq_count == g_mouse_irq_count)
        && (g_mouse_diag_packet_count == g_mouse_packet_count)
        && (g_mouse_diag_pending_count == g_mouse_pending)
        && (g_mouse_diag_x == g_mouse_x)
        && (g_mouse_diag_y == g_mouse_y)
        && (g_mouse_diag_buttons == g_mouse_buttons)
        && (g_mouse_diag_raw_byte == g_ps2_mouse_last_raw_byte)
        && (g_mouse_diag_bad_starts == g_ps2_mouse_bad_start_count)
        && (g_mouse_diag_xhci_keyboard_endpoint == xhci_keyboard_endpoint)
        && (g_mouse_diag_xhci_keyboard_pending == xhci_keyboard_pending)
        && (g_mouse_diag_xhci_keyboard_reports == xhci_keyboard_reports)
        && (g_mouse_diag_xhci_mouse_endpoint == xhci_mouse_endpoint)
        && (g_mouse_diag_xhci_mouse_pending == xhci_mouse_pending)
        && (g_mouse_diag_xhci_mouse_reports == xhci_mouse_reports)
        && (g_mouse_diag_xhci_live_enabled == xhci_live_enabled)
        && (g_mouse_diag_i2c_keyboard_found == i2c_keyboard_found)
        && (g_mouse_diag_i2c_keyboard_reports == i2c_keyboard_reports)
        && (g_mouse_diag_i2c_keyboard_error == i2c_keyboard_error)
        && (g_mouse_diag_i2c_pointer_found == i2c_pointer_found)
        && (g_mouse_diag_i2c_pointer_reports == i2c_pointer_reports)
        && (g_mouse_diag_i2c_pointer_error == i2c_pointer_error))
    {
        return;
    }

    g_mouse_diag_valid = 1u;
    g_mouse_diag_init_done = g_mouse_enabled;
    g_mouse_diag_aux_enabled = g_ps2_mouse_aux_enabled;
    g_mouse_diag_config_read = g_ps2_mouse_config_read;
    g_mouse_diag_config_write = g_ps2_mouse_config_write;
    g_mouse_diag_irq12_configured = g_ps2_mouse_irq12_configured;
    g_mouse_diag_enable_command = g_ps2_mouse_enable_command;
    g_mouse_diag_ack = g_ps2_mouse_ack;
    g_mouse_diag_irq_count = g_mouse_irq_count;
    g_mouse_diag_packet_count = g_mouse_packet_count;
    g_mouse_diag_pending_count = g_mouse_pending;
    g_mouse_diag_x = g_mouse_x;
    g_mouse_diag_y = g_mouse_y;
    g_mouse_diag_buttons = g_mouse_buttons;
    g_mouse_diag_raw_byte = g_ps2_mouse_last_raw_byte;
    g_mouse_diag_bad_starts = g_ps2_mouse_bad_start_count;
    g_mouse_diag_xhci_keyboard_endpoint = xhci_keyboard_endpoint;
    g_mouse_diag_xhci_keyboard_pending = xhci_keyboard_pending;
    g_mouse_diag_xhci_keyboard_reports = xhci_keyboard_reports;
    g_mouse_diag_xhci_mouse_endpoint = xhci_mouse_endpoint;
    g_mouse_diag_xhci_mouse_pending = xhci_mouse_pending;
    g_mouse_diag_xhci_mouse_reports = xhci_mouse_reports;
    g_mouse_diag_xhci_live_enabled = xhci_live_enabled;
    g_mouse_diag_i2c_keyboard_found = i2c_keyboard_found;
    g_mouse_diag_i2c_keyboard_reports = i2c_keyboard_reports;
    g_mouse_diag_i2c_keyboard_error = i2c_keyboard_error;
    g_mouse_diag_i2c_pointer_found = i2c_pointer_found;
    g_mouse_diag_i2c_pointer_reports = i2c_pointer_reports;
    g_mouse_diag_i2c_pointer_error = i2c_pointer_error;

    (void)display64_write_mouse_diagnostics(
        g_mouse_enabled,
        g_ps2_mouse_aux_enabled,
        g_ps2_mouse_config_read,
        g_ps2_mouse_config_write,
        g_ps2_mouse_irq12_configured,
        g_ps2_mouse_enable_command,
        g_ps2_mouse_ack,
        g_mouse_irq_count,
        g_mouse_packet_count,
        g_mouse_pending,
        g_mouse_x,
        g_mouse_y,
        g_mouse_buttons,
        g_ps2_mouse_last_raw_byte,
        g_ps2_mouse_bad_start_count,
        xhci_keyboard_endpoint,
        xhci_keyboard_pending,
        xhci_keyboard_reports,
        xhci_mouse_endpoint,
        xhci_mouse_pending,
        xhci_mouse_reports,
        xhci_live_enabled,
        i2c_keyboard_found,
        i2c_keyboard_reports,
        i2c_keyboard_error,
        i2c_pointer_found,
        i2c_pointer_reports,
        i2c_pointer_error);
}

static u32 input64_mouse_enqueue_delta(s32 dx, s32 dy, u32 buttons)
{
    struct input64_mouse_event event;
    u32 next_buttons = buttons & 0x7u;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (display64_gui_settings_pointer_speed() >= 3u)
    {
        dx *= 2;
        dy *= 2;
    }
    else if (display64_gui_settings_pointer_speed() <= 1u)
    {
        dx /= 2;
        dy /= 2;
    }
#endif

    if ((dx == 0) && (dy == 0) && (next_buttons == g_mouse_buttons))
    {
        return 0u;
    }

    g_mouse_found = 1u;
    g_mouse_buttons = next_buttons;
    g_mouse_last_dx = dx;
    g_mouse_last_dy = dy;
    if ((dx != 0) || (dy != 0))
    {
        g_mouse_delta_seen = 1u;
    }
    if (g_mouse_buttons != 0u)
    {
        g_mouse_button_seen = 1u;
    }

    g_mouse_x = input64_mouse_clamp_axis((s32)g_mouse_x + dx, g_mouse_width);
    g_mouse_y = input64_mouse_clamp_axis((s32)g_mouse_y + dy, g_mouse_height);
#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
    (void)display64_compositor_update_cursor(g_mouse_x, g_mouse_y, g_mouse_buttons);
    (void)display64_wm_process_mouse_event(g_mouse_x, g_mouse_y, g_mouse_buttons, dx, dy);
#endif
    ++g_mouse_packet_count;

    if (g_mouse_pending >= INPUT64_MOUSE_QUEUE_CAPACITY)
    {
        ++g_mouse_drop_count;
        input64_mouse_publish_diagnostics();
        return 1u;
    }

    event.dx = dx;
    event.dy = dy;
    event.buttons = g_mouse_buttons;
    event.x = g_mouse_x;
    event.y = g_mouse_y;
    g_mouse_queue[g_mouse_tail] = event;
    g_mouse_tail = (g_mouse_tail + 1u) % INPUT64_MOUSE_QUEUE_CAPACITY;
    ++g_mouse_pending;
    input64_mouse_publish_diagnostics();
    return 1u;
}

static void input64_mouse_accept_ps2_byte(u8 value)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 packet_bytes = g_ps2_mouse_packet_bytes;
#endif

    g_ps2_mouse_last_raw_byte = (u32)value;
    if (g_ps2_mouse_phase == 0u)
    {
        if (((value & 0x08u) == 0u)
            || ((value & 0xC0u) != 0u)
            || (value == INPUT64_PS2_ACK)
            || (value == INPUT64_PS2_DEVICE_SELF_TEST_OK))
        {
            ++g_ps2_mouse_bad_start_count;
            if (g_ps2_mouse_raw_log_count < 16u)
            {
                serial_write_string("[x64] ps2 mouse raw discard 0x");
                input64_serial_hex_byte(value);
                serial_write_string("\n");
                ++g_ps2_mouse_raw_log_count;
            }
            return;
        }
        g_ps2_mouse_packet[0] = value;
        g_ps2_mouse_phase = 1u;
        return;
    }

    g_ps2_mouse_packet[g_ps2_mouse_phase] = value;
    ++g_ps2_mouse_phase;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_ps2_mouse_phase >= packet_bytes)
#else
    if (g_ps2_mouse_phase >= 3u)
#endif
    {
        s32 dx = input64_ps2_mouse_delta(g_ps2_mouse_packet[0], g_ps2_mouse_packet[1], 0x10u);
        s32 dy = -input64_ps2_mouse_delta(g_ps2_mouse_packet[0], g_ps2_mouse_packet[2], 0x20u);
        if ((g_ps2_mouse_packet[0] & 0xC0u) != 0u)
        {
            ++g_ps2_mouse_bad_start_count;
            g_ps2_mouse_phase = 0u;
            return;
        }
        input64_mouse_enqueue_delta(dx, dy, (u32)(g_ps2_mouse_packet[0] & 0x07u));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        if (packet_bytes >= 4u)
        {
            s32 wheel = (s32)(g_ps2_mouse_packet[3] & 0x0Fu);
            if ((wheel & 0x08) != 0)
            {
                wheel -= 16;
            }
            if (wheel != 0)
            {
                ++g_ps2_mouse_wheel_count;
                (void)display64_wm_process_mouse_wheel(wheel);
            }
        }
#endif
        g_ps2_mouse_phase = 0u;
    }
}

static u8 input64_usb_hid_key_was_down(const u8 *keys, u8 keycode)
{
    u32 index;

    for (index = 0u; index < 6u; ++index)
    {
        if (keys[index] == keycode)
        {
            return 1u;
        }
    }

    return 0u;
}

static u8 input64_usb_hid_translate_key(u8 keycode)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u8 shifted = ((g_keyboard_left_shift != 0u) || (g_keyboard_right_shift != 0u)) ? 1u : 0u;
#else
    u8 shifted = 0u;
#endif
    static const u8 digit_unshifted[10] = {
        (u8)'1', (u8)'2', (u8)'3', (u8)'4', (u8)'5',
        (u8)'6', (u8)'7', (u8)'8', (u8)'9', (u8)'0'
    };
    static const u8 digit_shifted[10] = {
        (u8)'!', (u8)'@', (u8)'#', (u8)'$', (u8)'%',
        (u8)'^', (u8)'&', (u8)'*', (u8)'(', (u8)')'
    };

    if ((keycode >= 0x04u) && (keycode <= 0x1Du))
    {
        u8 base = (u8)('a' + (keycode - 0x04u));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        return ((shifted ^ g_keyboard_caps_lock) != 0u) ? (u8)(base - 32u) : base;
#else
        return shifted != 0u ? (u8)(base - 32u) : base;
#endif
    }

    if ((keycode >= 0x1Eu) && (keycode <= 0x27u))
    {
        u32 index = keycode - 0x1Eu;
        return shifted != 0u ? digit_shifted[index] : digit_unshifted[index];
    }

    switch (keycode)
    {
        case 0x28u: return (u8)'\n';
        case 0x2Au: return (u8)'\b';
        case 0x2Bu: return (u8)'\t';
        case 0x2Cu: return (u8)' ';
        case 0x2Du: return shifted != 0u ? (u8)'_' : (u8)'-';
        case 0x2Eu: return shifted != 0u ? (u8)'+' : (u8)'=';
        case 0x2Fu: return shifted != 0u ? (u8)'{' : (u8)'[';
        case 0x30u: return shifted != 0u ? (u8)'}' : (u8)']';
        case 0x31u: return shifted != 0u ? (u8)'|' : (u8)'\\';
        case 0x33u: return shifted != 0u ? (u8)':' : (u8)';';
        case 0x34u: return shifted != 0u ? (u8)'"' : (u8)'\'';
        case 0x35u: return shifted != 0u ? (u8)'~' : (u8)'`';
        case 0x36u: return shifted != 0u ? (u8)'<' : (u8)',';
        case 0x37u: return shifted != 0u ? (u8)'>' : (u8)'.';
        case 0x38u: return shifted != 0u ? (u8)'?' : (u8)'/';
        default: return 0u;
    }
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u8 input64_keyboard_shift_active(void)
{
    return ((g_keyboard_left_shift != 0u) || (g_keyboard_right_shift != 0u)) ? 1u : 0u;
}
#endif

static u8 input64_keyboard_apply_modifiers(u8 value)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u8 shifted = input64_keyboard_shift_active();

    if ((value >= (u8)'a') && (value <= (u8)'z'))
    {
        return ((shifted ^ g_keyboard_caps_lock) != 0u) ? (u8)(value - 32u) : value;
    }

    if (shifted == 0u)
    {
        return value;
    }

    switch (value)
    {
        case (u8)'1': return (u8)'!';
        case (u8)'2': return (u8)'@';
        case (u8)'3': return (u8)'#';
        case (u8)'4': return (u8)'$';
        case (u8)'5': return (u8)'%';
        case (u8)'6': return (u8)'^';
        case (u8)'7': return (u8)'&';
        case (u8)'8': return (u8)'*';
        case (u8)'9': return (u8)'(';
        case (u8)'0': return (u8)')';
        case (u8)'-': return (u8)'_';
        case (u8)'=': return (u8)'+';
        case (u8)'[': return (u8)'{';
        case (u8)']': return (u8)'}';
        case (u8)'\\': return (u8)'|';
        case (u8)';': return (u8)':';
        case (u8)'\'': return (u8)'"';
        case (u8)'`': return (u8)'~';
        case (u8)',': return (u8)'<';
        case (u8)'.': return (u8)'>';
        case (u8)'/': return (u8)'?';
        default: return value;
    }
#else
    return value;
#endif
}

static u8 input64_keyboard_translate_scancode(u8 scancode)
{
    switch (scancode)
    {
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return '-';
        case 0x0D: return '=';
        case 0x0E: return '\b';
        case 0x0F: return '\t';
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1A: return '[';
        case 0x1B: return ']';
        case 0x1C: return '\n';
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x27: return ';';
        case 0x28: return '\'';
        case 0x29: return '`';
        case 0x2B: return '\\';
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';
        case 0x39: return ' ';
        default: return 0u;
    }
}

static u8 input64_keyboard_translate_set2_scancode(u8 scancode)
{
    switch (scancode)
    {
        case 0x0D: return '\t';
        case 0x15: return 'q';
        case 0x16: return '1';
        case 0x1A: return 'z';
        case 0x1B: return 's';
        case 0x1C: return 'a';
        case 0x1D: return 'w';
        case 0x1E: return '2';
        case 0x21: return 'c';
        case 0x22: return 'x';
        case 0x23: return 'd';
        case 0x24: return 'e';
        case 0x25: return '4';
        case 0x26: return '3';
        case 0x29: return ' ';
        case 0x2A: return 'v';
        case 0x2B: return 'f';
        case 0x2C: return 't';
        case 0x2D: return 'r';
        case 0x2E: return '5';
        case 0x31: return 'n';
        case 0x32: return 'b';
        case 0x33: return 'h';
        case 0x34: return 'g';
        case 0x35: return 'y';
        case 0x36: return '6';
        case 0x3A: return 'm';
        case 0x3B: return 'j';
        case 0x3C: return 'u';
        case 0x3D: return '7';
        case 0x3E: return '8';
        case 0x41: return ',';
        case 0x42: return 'k';
        case 0x43: return 'i';
        case 0x44: return 'o';
        case 0x45: return '0';
        case 0x46: return '9';
        case 0x49: return '.';
        case 0x4A: return '/';
        case 0x4B: return 'l';
        case 0x4C: return ';';
        case 0x4D: return 'p';
        case 0x4E: return '-';
        case 0x52: return '\'';
        case 0x54: return '[';
        case 0x55: return '=';
        case 0x5A: return '\n';
        case 0x5B: return ']';
        case 0x5D: return '\\';
        case 0x66: return '\b';
        default: return 0u;
    }
}

static void input64_keyboard_accept_set2_scancode(u8 scancode)
{
    u8 translated;

    ++g_keyboard_scancode_count;
    g_keyboard_last_scancode = scancode;

    if (scancode == 0xE0u)
    {
        g_keyboard_extended_prefix = 1u;
        return;
    }

    if (scancode == 0xF0u)
    {
        g_keyboard_break_prefix = 1u;
        return;
    }

    if (g_keyboard_break_prefix != 0u)
    {
        g_keyboard_break_prefix = 0u;
        g_keyboard_extended_prefix = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        if (scancode == 0x12u)
        {
            g_keyboard_left_shift = 0u;
        }
        else if (scancode == 0x59u)
        {
            g_keyboard_right_shift = 0u;
        }
#endif
        return;
    }

    if (g_keyboard_extended_prefix != 0u)
    {
        g_keyboard_extended_prefix = 0u;
        switch (scancode)
        {
            case 0x69:
                input64_keyboard_enqueue_csi_final((u8)'F');
                return;

            case 0x6B:
                input64_keyboard_enqueue_csi_final((u8)'D');
                return;

            case 0x6C:
                input64_keyboard_enqueue_csi_final((u8)'H');
                return;

            case 0x71:
                input64_keyboard_enqueue_delete_sequence();
                return;

            case 0x72:
                input64_keyboard_enqueue_csi_final((u8)'B');
                return;

            case 0x74:
                input64_keyboard_enqueue_csi_final((u8)'C');
                return;

            case 0x75:
                input64_keyboard_enqueue_csi_final((u8)'A');
                return;

            default:
                return;
        }
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (scancode == 0x12u)
    {
        g_keyboard_left_shift = 1u;
        return;
    }
    if (scancode == 0x59u)
    {
        g_keyboard_right_shift = 1u;
        return;
    }
    if (scancode == 0x58u)
    {
        g_keyboard_caps_lock = (g_keyboard_caps_lock == 0u) ? 1u : 0u;
        return;
    }
#endif

    translated = input64_keyboard_translate_set2_scancode(scancode);
    if (translated != 0u)
    {
        input64_keyboard_enqueue_byte(input64_keyboard_apply_modifiers(translated));
    }
}

static void input64_keyboard_accept_set1_scancode(u8 scancode)
{
    u8 translated;
    u8 released;

    ++g_keyboard_scancode_count;
    g_keyboard_last_scancode = scancode;

    if (scancode == 0xE0u)
    {
        g_keyboard_extended_prefix = 1u;
        return;
    }

    if (g_keyboard_extended_prefix != 0u)
    {
        g_keyboard_extended_prefix = 0u;
        if ((scancode & 0x80u) != 0u)
        {
            return;
        }

        switch (scancode)
        {
            case 0x47:
                input64_keyboard_enqueue_csi_final((u8)'H');
                return;

            case 0x48:
                input64_keyboard_enqueue_csi_final((u8)'A');
                return;

            case 0x4B:
                input64_keyboard_enqueue_csi_final((u8)'D');
                return;

            case 0x4D:
                input64_keyboard_enqueue_csi_final((u8)'C');
                return;

            case 0x4F:
                input64_keyboard_enqueue_csi_final((u8)'F');
                return;

            case 0x50:
                input64_keyboard_enqueue_csi_final((u8)'B');
                return;

            case 0x53:
                input64_keyboard_enqueue_delete_sequence();
                return;

            default:
                return;
        }
    }

    released = (scancode & 0x80u) != 0u ? 1u : 0u;
    scancode &= 0x7Fu;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (scancode == 0x2Au)
    {
        g_keyboard_left_shift = (released == 0u) ? 1u : 0u;
        return;
    }
    if (scancode == 0x36u)
    {
        g_keyboard_right_shift = (released == 0u) ? 1u : 0u;
        return;
    }
    if ((scancode == 0x3Au) && (released == 0u))
    {
        g_keyboard_caps_lock = (g_keyboard_caps_lock == 0u) ? 1u : 0u;
        return;
    }
#endif

    if (released != 0u)
    {
        return;
    }

    translated = input64_keyboard_translate_scancode(scancode);
    if (translated != 0u)
    {
        input64_keyboard_enqueue_byte(input64_keyboard_apply_modifiers(translated));
    }
}

static void input64_keyboard_accept_scancode(u8 scancode)
{
    if ((g_keyboard_scancode_set == INPUT64_KEYBOARD_SCANCODE_SET2)
        && ((scancode & 0x80u) != 0u))
    {
        g_keyboard_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
        input64_keyboard_clear_pending();
        input64_keyboard_accept_set1_scancode(scancode);
        return;
    }

    if ((g_keyboard_scancode_set == INPUT64_KEYBOARD_SCANCODE_SET1)
        && (scancode == 0xF0u))
    {
        g_keyboard_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET2;
        input64_keyboard_clear_pending();
        input64_keyboard_accept_set2_scancode(scancode);
        return;
    }

    if (g_keyboard_scancode_set == INPUT64_KEYBOARD_SCANCODE_SET2)
    {
        input64_keyboard_accept_set2_scancode(scancode);
        return;
    }

    input64_keyboard_accept_set1_scancode(scancode);
}

static void input64_keyboard_drain_controller(void)
{
    u32 drained = 0u;

    while (drained < INPUT64_PS2_DRAIN_LIMIT)
    {
        u8 status = inb(INPUT64_PS2_STATUS_PORT);

        g_ps2_status_snapshot = (u32)status;
        if ((status & INPUT64_PS2_STATUS_OUTPUT_READY) == 0u)
        {
            return;
        }

        if ((status & INPUT64_PS2_STATUS_AUX_DATA) != 0u)
        {
            u8 value = inb(INPUT64_PS2_DATA_PORT);
            if ((g_ps2_mouse_present != 0u) && (input64_native_pointer_present() == 0u))
            {
                input64_mouse_accept_ps2_byte(value);
            }
        }
        else
        {
            input64_keyboard_accept_scancode(inb(INPUT64_PS2_DATA_PORT));
        }
        ++drained;
    }
}

static void input64_mouse_drain_aux_controller(void)
{
    u32 drained = 0u;
    u32 accept_mouse_bytes;

    if (g_ps2_mouse_present == 0u)
    {
        return;
    }

    accept_mouse_bytes = (input64_native_pointer_present() == 0u) ? 1u : 0u;

    while (drained < INPUT64_PS2_DRAIN_LIMIT)
    {
        u8 status = inb(INPUT64_PS2_STATUS_PORT);
        u8 value;

        g_ps2_status_snapshot = (u32)status;
        if ((status & INPUT64_PS2_STATUS_OUTPUT_READY) == 0u)
        {
            return;
        }

        value = inb(INPUT64_PS2_DATA_PORT);
        /*
         * This path is entered only from IRQ12. Some real 8042-compatible
         * controllers deliver the interrupt while leaving the AUX status bit
         * clear, so IRQ provenance is stronger than the status tag here.
         */
        if (accept_mouse_bytes != 0u)
        {
            input64_mouse_accept_ps2_byte(value);
        }
        ++drained;
    }
}

static void input64_keyboard_drain_raw(void)
{
    u32 drained = 0u;

    while (drained < INPUT64_PS2_DRAIN_LIMIT)
    {
        u8 status = inb(INPUT64_PS2_STATUS_PORT);

        g_ps2_status_snapshot = (u32)status;
        if ((status & INPUT64_PS2_STATUS_OUTPUT_READY) == 0u)
        {
            return;
        }

        (void)inb(INPUT64_PS2_DATA_PORT);
        ++drained;
    }
}

static u32 input64_ps2_wait_output_ready(void)
{
    u32 poll;

    for (poll = 0u; poll < INPUT64_PS2_WAIT_LIMIT; ++poll)
    {
        u8 status = inb(INPUT64_PS2_STATUS_PORT);

        g_ps2_status_snapshot = (u32)status;
        if (status == 0xFFu)
        {
            return 0u;
        }

        if ((status & INPUT64_PS2_STATUS_OUTPUT_READY) != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 input64_ps2_wait_input_clear(void)
{
    u32 poll;

    for (poll = 0u; poll < INPUT64_PS2_WAIT_LIMIT; ++poll)
    {
        u8 status = inb(INPUT64_PS2_STATUS_PORT);

        g_ps2_status_snapshot = (u32)status;
        if (status == 0xFFu)
        {
            return 0u;
        }

        if ((status & INPUT64_PS2_STATUS_INPUT_FULL) == 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 input64_ps2_write_command(u8 command)
{
    if (input64_ps2_wait_input_clear() == 0u)
    {
        return 0u;
    }

    outb(INPUT64_PS2_STATUS_PORT, command);
    return 1u;
}

static u32 input64_ps2_write_data(u8 value)
{
    if (input64_ps2_wait_input_clear() == 0u)
    {
        return 0u;
    }

    outb(INPUT64_PS2_DATA_PORT, value);
    return 1u;
}

static u32 input64_ps2_read_data(u8 *value)
{
    if (input64_ps2_wait_output_ready() == 0u)
    {
        return 0u;
    }

    *value = inb(INPUT64_PS2_DATA_PORT);
    return 1u;
}

static u32 input64_ps2_read_device_data(u8 *value, u32 require_aux)
{
    u32 poll;

    if (value == 0)
    {
        return 0u;
    }

    for (poll = 0u; poll < INPUT64_PS2_WAIT_LIMIT; ++poll)
    {
        u8 status = inb(INPUT64_PS2_STATUS_PORT);

        g_ps2_status_snapshot = (u32)status;
        if (status == 0xFFu)
        {
            return 0u;
        }

        if ((status & INPUT64_PS2_STATUS_OUTPUT_READY) == 0u)
        {
            continue;
        }

        *value = inb(INPUT64_PS2_DATA_PORT);
        if (require_aux != 0u)
        {
            if ((status & INPUT64_PS2_STATUS_AUX_DATA) != 0u)
            {
                return 1u;
            }
            input64_keyboard_accept_scancode(*value);
        }
        else
        {
            if ((status & INPUT64_PS2_STATUS_AUX_DATA) == 0u)
            {
                return 1u;
            }
            input64_mouse_accept_ps2_byte(*value);
        }
    }

    return 0u;
}

static u32 input64_ps2_read_config(u8 *config)
{
    if (input64_ps2_write_command(INPUT64_PS2_COMMAND_READ_CONFIG) == 0u)
    {
        return 0u;
    }

    return input64_ps2_read_data(config);
}

static u32 input64_ps2_write_config(u8 config)
{
    if (input64_ps2_write_command(INPUT64_PS2_COMMAND_WRITE_CONFIG) == 0u)
    {
        return 0u;
    }

    return input64_ps2_write_data(config);
}

static u32 input64_ps2_send_device_command(u8 command, u8 *ack)
{
    u8 response = 0u;

    if (input64_ps2_write_data(command) == 0u)
    {
        return 0u;
    }

    if (input64_ps2_read_device_data(&response, 0u) == 0u)
    {
        return 0u;
    }

    *ack = response;
    return response == INPUT64_PS2_ACK ? 1u : 0u;
}

static u32 input64_ps2_read_mouse_ack_lenient(u8 *ack)
{
    u32 poll;

    if (ack == 0)
    {
        return 0u;
    }

    *ack = 0u;
    for (poll = 0u; poll < INPUT64_PS2_WAIT_LIMIT; ++poll)
    {
        u8 status = inb(INPUT64_PS2_STATUS_PORT);
        u8 value;

        g_ps2_status_snapshot = (u32)status;
        if (status == 0xFFu)
        {
            return 0u;
        }

        if ((status & INPUT64_PS2_STATUS_OUTPUT_READY) == 0u)
        {
            continue;
        }

        value = inb(INPUT64_PS2_DATA_PORT);
        if ((value == INPUT64_PS2_ACK) || (value == 0xFEu))
        {
            *ack = value;
            return value == INPUT64_PS2_ACK ? 1u : 0u;
        }

        if ((status & INPUT64_PS2_STATUS_AUX_DATA) != 0u)
        {
            input64_mouse_accept_ps2_byte(value);
        }
        else
        {
            input64_keyboard_accept_scancode(value);
        }
    }

    return 0u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 input64_ps2_write_aux_data(u8 value)
{
    if (input64_ps2_wait_input_clear() == 0u)
    {
        return 0u;
    }
    outb(INPUT64_PS2_STATUS_PORT, INPUT64_PS2_COMMAND_WRITE_SECOND_PORT);
    if (input64_ps2_wait_input_clear() == 0u)
    {
        return 0u;
    }
    outb(INPUT64_PS2_DATA_PORT, value);
    return 1u;
}

static u32 input64_ps2_send_aux_command_ack(u8 command)
{
    u8 ack = 0u;

    if (input64_ps2_write_aux_data(command) == 0u)
    {
        return 0u;
    }
    return input64_ps2_read_mouse_ack_lenient(&ack);
}

static u32 input64_ps2_send_aux_sample_rate(u8 sample_rate)
{
    return (input64_ps2_send_aux_command_ack(INPUT64_PS2_DEVICE_SET_SAMPLE_RATE) != 0u)
        && (input64_ps2_send_aux_command_ack(sample_rate) != 0u)
        ? 1u
        : 0u;
}

static u32 input64_ps2_try_enable_wheel(void)
{
    u8 device_id = 0u;

    if ((input64_ps2_send_aux_sample_rate(200u) == 0u)
        || (input64_ps2_send_aux_sample_rate(100u) == 0u)
        || (input64_ps2_send_aux_sample_rate(80u) == 0u)
        || (input64_ps2_send_aux_command_ack(INPUT64_PS2_DEVICE_GET_ID) == 0u)
        || (input64_ps2_read_device_data(&device_id, 1u) == 0u))
    {
        g_ps2_mouse_packet_bytes = 3u;
        g_ps2_mouse_wheel_enabled = 0u;
        return 0u;
    }

    if ((device_id == 3u) || (device_id == 4u))
    {
        g_ps2_mouse_packet_bytes = 4u;
        g_ps2_mouse_wheel_enabled = 1u;
        return 1u;
    }

    g_ps2_mouse_packet_bytes = 3u;
    g_ps2_mouse_wheel_enabled = 0u;
    return 0u;
}
#endif

static u32 input64_ps2_send_aux_enable_streaming_strict(u8 *ack)
{
    u32 attempt;

    if (ack != 0)
    {
        *ack = 0u;
    }

    for (attempt = 0u; attempt < INPUT64_PS2_MOUSE_ENABLE_RETRIES; ++attempt)
    {
        u8 response = 0u;

        if (input64_ps2_wait_input_clear() == 0u)
        {
            return 0u;
        }
        outb(INPUT64_PS2_STATUS_PORT, INPUT64_PS2_COMMAND_WRITE_SECOND_PORT);

        if (input64_ps2_wait_input_clear() == 0u)
        {
            return 0u;
        }
        outb(INPUT64_PS2_DATA_PORT, INPUT64_PS2_DEVICE_ENABLE_SCANNING);

        if (input64_ps2_read_mouse_ack_lenient(&response) != 0u)
        {
            if (ack != 0)
            {
                *ack = response;
            }
            return 1u;
        }
        if (ack != 0)
        {
            *ack = response;
        }
    }

    return 0u;
}

static u32 input64_lpss_i2c_pointer_controller_present(void)
{
    u32 index;

    for (index = 0u; index < pci64_lpss_i2c_pointer_candidate_count(); ++index)
    {
        u32 flags = pci64_lpss_i2c_pointer_candidate_mmio_flags(index);
        if ((pci64_lpss_i2c_pointer_candidate_address(index) != PCI64_INVALID_RESULT)
            && ((flags & PCI64_LPSS_I2C_MMIO_FLAG_PRESENT) != 0u)
            && ((flags & PCI64_LPSS_I2C_MMIO_FLAG_MEMORY_BAR) != 0u)
            && ((flags & PCI64_LPSS_I2C_MMIO_FLAG_BASE_NONZERO) != 0u))
        {
            return 1u;
        }
    }

    return 0u;
}

static void input64_ps2_enable_controller(void)
{
    u8 status = inb(INPUT64_PS2_STATUS_PORT);
    u8 config = 0u;
    u8 ack = 0u;
    u8 mouse_ack = 0u;
    u32 i2c_pointer_controller = 0u;

    g_ps2_status_snapshot = (u32)status;
    g_ps2_present = (status != 0xFFu) ? 1u : 0u;
    if (g_ps2_present == 0u)
    {
        return;
    }

    g_ps2_enable_attempted = 1u;

    input64_keyboard_drain_raw();
    i2c_pointer_controller = input64_lpss_i2c_pointer_controller_present();
    if (i2c_pointer_controller != 0u)
    {
        if (input64_ps2_read_config(&config) == 0u)
        {
            g_ps2_enable_status = g_ps2_status_snapshot;
            input64_mouse_publish_diagnostics();
            return;
        }
        g_ps2_mouse_config_read = 1u;

        config &= (u8)~INPUT64_PS2_CONFIG_IRQ12;
        config |= INPUT64_PS2_CONFIG_IRQ1 | INPUT64_PS2_CONFIG_TRANSLATION;
        config &= (u8)~INPUT64_PS2_CONFIG_FIRST_PORT_CLOCK_DISABLE;
        config |= INPUT64_PS2_CONFIG_SECOND_PORT_CLOCK_DISABLE;
        g_ps2_config_byte = (u32)config;
        g_ps2_recommended_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
        if (input64_ps2_write_config(config) == 0u)
        {
            g_ps2_enable_status = g_ps2_status_snapshot;
            input64_mouse_publish_diagnostics();
            return;
        }
        g_ps2_mouse_config_write = 1u;
        g_ps2_mouse_irq12_configured = 0u;
        g_ps2_mouse_aux_enabled = 0u;
        g_ps2_mouse_enable_command = 0u;
        g_ps2_mouse_present = 0u;
        g_ps2_mouse_ack = 0u;
        serial_write_string("[x64:input] ps2 aux mouse skipped; lpss i2c pointer controller present\n");
        goto enable_first_port;
    }

    if (input64_ps2_write_command(INPUT64_PS2_COMMAND_ENABLE_SECOND_PORT) == 0u)
    {
        g_ps2_enable_status = g_ps2_status_snapshot;
        input64_mouse_publish_diagnostics();
        return;
    }
    g_ps2_mouse_aux_enabled = 1u;

    if (input64_ps2_read_config(&config) == 0u)
    {
        g_ps2_enable_status = g_ps2_status_snapshot;
        input64_mouse_publish_diagnostics();
        return;
    }
    g_ps2_mouse_config_read = 1u;

    config |= INPUT64_PS2_CONFIG_IRQ12;
    config |= INPUT64_PS2_CONFIG_IRQ1 | INPUT64_PS2_CONFIG_TRANSLATION;
    config &= (u8)~INPUT64_PS2_CONFIG_FIRST_PORT_CLOCK_DISABLE;
    config &= (u8)~INPUT64_PS2_CONFIG_SECOND_PORT_CLOCK_DISABLE;
    g_ps2_config_byte = (u32)config;
    g_ps2_recommended_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
    if (input64_ps2_write_config(config) == 0u)
    {
        g_ps2_enable_status = g_ps2_status_snapshot;
        input64_mouse_publish_diagnostics();
        return;
    }
    g_ps2_mouse_config_write = 1u;
    g_ps2_mouse_irq12_configured =
        (((config & INPUT64_PS2_CONFIG_IRQ12) != 0u)
            && ((config & INPUT64_PS2_CONFIG_SECOND_PORT_CLOCK_DISABLE) == 0u)) ? 1u : 0u;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)input64_ps2_try_enable_wheel();
#endif
    g_ps2_mouse_enable_command = 1u;
    if (input64_ps2_send_aux_enable_streaming_strict(&mouse_ack) != 0u)
    {
        g_ps2_mouse_present = 1u;
        g_mouse_enabled = 1u;
        g_mouse_found = 1u;
    }
    else
    {
        config &= (u8)~INPUT64_PS2_CONFIG_IRQ12;
        config |= INPUT64_PS2_CONFIG_SECOND_PORT_CLOCK_DISABLE;
        g_ps2_config_byte = (u32)config;
        g_ps2_mouse_irq12_configured = 0u;
        (void)input64_ps2_write_config(config);
    }
    g_ps2_mouse_ack = (u32)mouse_ack;

enable_first_port:
    if (input64_ps2_write_command(INPUT64_PS2_COMMAND_ENABLE_FIRST_PORT) == 0u)
    {
        g_ps2_enable_status = g_ps2_status_snapshot;
        input64_mouse_publish_diagnostics();
        return;
    }

    g_ps2_enable_status = (u32)inb(INPUT64_PS2_STATUS_PORT);
    g_ps2_status_snapshot = g_ps2_enable_status;
    g_ps2_enabled = 1u;

    if (input64_ps2_send_device_command(INPUT64_PS2_DEVICE_ENABLE_SCANNING, &ack) != 0u)
    {
        g_ps2_scanning_enabled = 1u;
    }
    g_ps2_device_ack = (u32)ack;
    if ((g_ps2_device_ack == 0u) && (mouse_ack != 0u))
    {
        g_ps2_device_ack = (u32)mouse_ack;
    }
    input64_mouse_publish_diagnostics();
    input64_keyboard_clear_pending();
}

void input64_init(void)
{
    g_read_count = 0u;
    g_byte_count = 0u;
    g_denial_count = 0u;
    g_eof_count = 0u;
    g_line_count = 0u;
    g_edit_count = 0u;
    g_keyboard_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
    input64_keyboard_reset();
    input64_mouse_reset();
    g_ps2_present = 0u;
    g_ps2_enable_attempted = 0u;
    g_ps2_enabled = 0u;
    g_ps2_enable_status = 0u;
    g_ps2_config_byte = 0u;
    g_ps2_device_ack = 0u;
    g_ps2_scanning_enabled = 0u;
    g_ps2_recommended_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
    g_ps2_self_test = 0u;
    g_ps2_first_port_test = 0u;
    g_ps2_reset_ack = 0u;
    g_ps2_reset_self_test = 0u;
    g_ps2_status_snapshot = (u32)inb(INPUT64_PS2_STATUS_PORT);
    input64_ps2_enable_controller();
}

void input64_set_keyboard_scancode_set(u32 scancode_set)
{
    if (scancode_set == INPUT64_KEYBOARD_SCANCODE_SET2)
    {
        g_keyboard_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET2;
        return;
    }

    g_keyboard_scancode_set = INPUT64_KEYBOARD_SCANCODE_SET1;
}

u32 input64_keyboard_scancode_set(void)
{
    return g_keyboard_scancode_set;
}

void input64_poll_keyboard(void)
{
    ++g_keyboard_poll_count;
    input64_keyboard_drain_controller();
    i2c_hid64_poll_keyboard();
    xhci64_poll_keyboard();
}

void input64_poll_mouse(void)
{
    ++g_mouse_poll_count;
    input64_keyboard_drain_controller();
    i2c_hid64_poll_pointer();
    xhci64_poll_mouse();
    input64_mouse_publish_diagnostics();
}

void input64_handle_keyboard_interrupt(void)
{
    ++g_keyboard_irq_count;
    input64_keyboard_drain_controller();
}

void input64_handle_mouse_interrupt(void)
{
    if (g_ps2_mouse_present == 0u)
    {
        return;
    }

    ++g_mouse_irq_count;
    input64_mouse_drain_aux_controller();
    input64_mouse_publish_diagnostics();
}

void input64_accept_usb_hid_boot_report(const u8 *report, u32 byte_count)
{
    u32 index;

    if ((report == 0) || (byte_count < 8u))
    {
        return;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_usb_hid_last_modifier = report[0];
    g_keyboard_left_shift = ((report[0] & 0x02u) != 0u) ? 1u : 0u;
    g_keyboard_right_shift = ((report[0] & 0x20u) != 0u) ? 1u : 0u;
#endif

    for (index = 0u; index < 6u; ++index)
    {
        u8 keycode = report[2u + index];
        u8 translated;

        if ((keycode == 0u) ||
            (input64_usb_hid_key_was_down(g_usb_hid_last_keys, keycode) != 0u))
        {
            continue;
        }

        ++g_keyboard_scancode_count;
        g_keyboard_last_scancode = 0x700u | (u32)keycode;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        if (keycode == 0x39u)
        {
            g_keyboard_caps_lock = (g_keyboard_caps_lock == 0u) ? 1u : 0u;
            continue;
        }
#endif
        translated = input64_usb_hid_translate_key(keycode);
        if (translated != 0u)
        {
            input64_keyboard_enqueue_byte(translated);
        }
    }

    for (index = 0u; index < 6u; ++index)
    {
        g_usb_hid_last_keys[index] = report[2u + index];
    }
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 input64_configure_usb_hid_mouse_report_layout(const u8 *descriptor, u32 byte_count)
{
    u32 index = 0u;
    u32 usage_page = 0u;
    u32 report_size = 0u;
    u32 report_count = 0u;
    u32 report_id = 0u;
    u32 report_bit_offset = 0u;
    u32 local_usages[INPUT64_HID_MOUSE_USAGE_SLOTS];
    u32 local_usage_count = 0u;
    u32 usage_min = 0u;
    u32 usage_max = 0u;

    input64_usb_hid_mouse_layout_reset();
    if ((descriptor == 0) || (byte_count == 0u))
    {
        return 0u;
    }

    while (index < byte_count)
    {
        u8 prefix = descriptor[index++];
        u32 item_size_code;
        u32 item_size;
        u32 item_type;
        u32 item_tag;
        u32 value;

        if (prefix == 0xFEu)
        {
            if ((index + 2u) > byte_count)
            {
                break;
            }
            item_size = descriptor[index];
            index += 2u;
            if ((index + item_size) > byte_count)
            {
                break;
            }
            index += item_size;
            continue;
        }

        item_size_code = (u32)(prefix & 0x03u);
        item_size = (item_size_code == 3u) ? 4u : item_size_code;
        item_type = ((u32)prefix >> 2) & 0x03u;
        item_tag = ((u32)prefix >> 4) & 0x0Fu;
        if ((index + item_size) > byte_count)
        {
            break;
        }
        value = input64_hid_short_item_value(&descriptor[index], item_size);
        index += item_size;

        if (item_type == 1u)
        {
            if (item_tag == 0u)
            {
                usage_page = value;
            }
            else if (item_tag == 7u)
            {
                report_size = value;
            }
            else if (item_tag == 8u)
            {
                report_id = value;
                report_bit_offset = 0u;
            }
            else if (item_tag == 9u)
            {
                report_count = value;
            }
            continue;
        }

        if (item_type == 2u)
        {
            if (item_tag == 0u)
            {
                if (local_usage_count < INPUT64_HID_MOUSE_USAGE_SLOTS)
                {
                    local_usages[local_usage_count] = value;
                    ++local_usage_count;
                }
            }
            else if (item_tag == 1u)
            {
                usage_min = value;
            }
            else if (item_tag == 2u)
            {
                usage_max = value;
            }
            continue;
        }

        if ((item_type == 0u) && (item_tag == 8u))
        {
            u32 field_index;
            u32 is_constant = value & 0x01u;

            if ((is_constant == 0u) && (report_size != 0u) && (report_count != 0u))
            {
                if ((usage_page == INPUT64_HID_USAGE_PAGE_BUTTON)
                    && (g_usb_hid_mouse_layout.buttons_bit_offset == INPUT64_HID_MOUSE_INVALID_BIT))
                {
                    g_usb_hid_mouse_layout.buttons_bit_offset = report_bit_offset;
                    g_usb_hid_mouse_layout.buttons_bit_count =
                        (report_count > 8u) ? 8u : report_count;
                }

                for (field_index = 0u; field_index < report_count; ++field_index)
                {
                    u32 usage = input64_hid_local_usage(
                        local_usages,
                        local_usage_count,
                        usage_min,
                        usage_max,
                        field_index);
                    u32 bit_offset = report_bit_offset + (field_index * report_size);

                    if (usage_page != INPUT64_HID_USAGE_PAGE_GENERIC_DESKTOP)
                    {
                        continue;
                    }

                    if ((usage == INPUT64_HID_USAGE_X)
                        && (g_usb_hid_mouse_layout.x_bit_offset == INPUT64_HID_MOUSE_INVALID_BIT))
                    {
                        g_usb_hid_mouse_layout.x_bit_offset = bit_offset;
                        g_usb_hid_mouse_layout.x_bit_count = report_size;
                    }
                    else if ((usage == INPUT64_HID_USAGE_Y)
                        && (g_usb_hid_mouse_layout.y_bit_offset == INPUT64_HID_MOUSE_INVALID_BIT))
                    {
                        g_usb_hid_mouse_layout.y_bit_offset = bit_offset;
                        g_usb_hid_mouse_layout.y_bit_count = report_size;
                    }
                    else if ((usage == INPUT64_HID_USAGE_WHEEL)
                        && (g_usb_hid_mouse_layout.wheel_bit_offset == INPUT64_HID_MOUSE_INVALID_BIT))
                    {
                        g_usb_hid_mouse_layout.wheel_bit_offset = bit_offset;
                        g_usb_hid_mouse_layout.wheel_bit_count = report_size;
                    }
                }
            }

            report_bit_offset += report_size * report_count;
            local_usage_count = 0u;
            usage_min = 0u;
            usage_max = 0u;
        }
    }

    if ((g_usb_hid_mouse_layout.x_bit_offset == INPUT64_HID_MOUSE_INVALID_BIT)
        || (g_usb_hid_mouse_layout.y_bit_offset == INPUT64_HID_MOUSE_INVALID_BIT)
        || (g_usb_hid_mouse_layout.x_bit_count == 0u)
        || (g_usb_hid_mouse_layout.y_bit_count == 0u)
        || (g_usb_hid_mouse_layout.x_bit_count > 31u)
        || (g_usb_hid_mouse_layout.y_bit_count > 31u))
    {
        input64_usb_hid_mouse_layout_reset();
        return 0u;
    }

    if (g_usb_hid_mouse_layout.buttons_bit_offset == INPUT64_HID_MOUSE_INVALID_BIT)
    {
        g_usb_hid_mouse_layout.buttons_bit_offset = 0u;
        g_usb_hid_mouse_layout.buttons_bit_count = 0u;
    }
    g_usb_hid_mouse_layout.report_id = report_id;
    g_usb_hid_mouse_layout.report_bytes = (report_bit_offset + 7u) >> 3;
    g_usb_hid_mouse_layout.ready = 1u;
    return 1u;
}

static u32 input64_accept_usb_hid_mouse_report_by_layout(const u8 *report, u32 byte_count)
{
    u32 buttons = 0u;
    s32 dx;
    s32 dy;

    if ((g_usb_hid_mouse_layout.ready == 0u) || (report == 0))
    {
        return 0u;
    }

    if (((g_usb_hid_mouse_layout.x_bit_offset + g_usb_hid_mouse_layout.x_bit_count) > (byte_count * 8u))
        || ((g_usb_hid_mouse_layout.y_bit_offset + g_usb_hid_mouse_layout.y_bit_count) > (byte_count * 8u)))
    {
        return 0u;
    }

    if ((g_usb_hid_mouse_layout.buttons_bit_count != 0u)
        && (g_usb_hid_mouse_layout.buttons_bit_offset != INPUT64_HID_MOUSE_INVALID_BIT))
    {
        if ((g_usb_hid_mouse_layout.buttons_bit_offset + g_usb_hid_mouse_layout.buttons_bit_count)
            > (byte_count * 8u))
        {
            return 0u;
        }
        buttons = input64_hid_field_value(
            report,
            byte_count,
            g_usb_hid_mouse_layout.buttons_bit_offset,
            g_usb_hid_mouse_layout.buttons_bit_count) & 0x7u;
    }

    dx = input64_hid_signed_field(
        report,
        byte_count,
        g_usb_hid_mouse_layout.x_bit_offset,
        g_usb_hid_mouse_layout.x_bit_count);
    dy = input64_hid_signed_field(
        report,
        byte_count,
        g_usb_hid_mouse_layout.y_bit_offset,
        g_usb_hid_mouse_layout.y_bit_count);

    if (input64_mouse_enqueue_delta(dx, dy, buttons) != 0u)
    {
        ++g_usb_hid_mouse_layout_reports;
    }

    if ((g_usb_hid_mouse_layout.wheel_bit_offset != INPUT64_HID_MOUSE_INVALID_BIT)
        && (g_usb_hid_mouse_layout.wheel_bit_count != 0u)
        && (g_usb_hid_mouse_layout.wheel_bit_count <= 31u))
    {
        s32 wheel = input64_hid_signed_field(
            report,
            byte_count,
            g_usb_hid_mouse_layout.wheel_bit_offset,
            g_usb_hid_mouse_layout.wheel_bit_count);
        if (wheel != 0)
        {
            (void)display64_wm_process_mouse_wheel(wheel);
        }
    }

    return 1u;
}
#endif

void input64_accept_usb_hid_mouse_report(const u8 *report, u32 byte_count)
{
    u32 offset;
    u32 accepted;

    if ((report == 0) || (byte_count < 3u))
    {
        return;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (input64_accept_usb_hid_mouse_report_by_layout(report, byte_count) != 0u)
    {
        return;
    }
    ++g_usb_hid_mouse_layout_fallbacks;
#endif

    accepted = input64_mouse_enqueue_delta(
        input64_sign_extend_byte(report[1]),
        input64_sign_extend_byte(report[2]),
        (u32)(report[0] & 0x07u));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (byte_count >= 4u)
    {
        (void)display64_wm_process_mouse_wheel(input64_sign_extend_byte(report[3]));
    }
#endif
    if (accepted != 0u)
    {
        return;
    }

    for (offset = 1u; (offset + 2u) < byte_count && offset < 4u; ++offset)
    {
        u32 buttons = (u32)(report[offset] & 0x07u);
        s32 dx = input64_sign_extend_byte(report[offset + 1u]);
        s32 dy = input64_sign_extend_byte(report[offset + 2u]);

        if (((report[offset] & 0xF8u) == 0u)
            && ((dx != 0) || (dy != 0) || (buttons != g_mouse_buttons)))
        {
            if (input64_mouse_enqueue_delta(dx, dy, buttons) != 0u)
            {
                return;
            }
        }
    }
}

void input64_accept_i2c_hid_touchpad_sample(u32 x, u32 y, u32 contact_active, u32 buttons)
{
    s32 dx;
    s32 dy;

    if (contact_active == 0u)
    {
        g_i2c_touchpad_contact_active = 0u;
        if ((buttons & 0x7u) != g_mouse_buttons)
        {
            input64_mouse_enqueue_delta(0, 0, buttons);
        }
        return;
    }

    if (g_i2c_touchpad_contact_active == 0u)
    {
        g_i2c_touchpad_contact_active = 1u;
        g_i2c_touchpad_last_x = x;
        g_i2c_touchpad_last_y = y;
        if ((buttons & 0x7u) != g_mouse_buttons)
        {
            input64_mouse_enqueue_delta(0, 0, buttons);
        }
        return;
    }

    dx = (s32)x - (s32)g_i2c_touchpad_last_x;
    dy = (s32)y - (s32)g_i2c_touchpad_last_y;
    g_i2c_touchpad_last_x = x;
    g_i2c_touchpad_last_y = y;
    input64_mouse_enqueue_delta(dx, dy, buttons);
}

void input64_set_mouse_bounds(u32 width, u32 height)
{
    if ((width == 0u) || (height == 0u))
    {
        return;
    }

    g_mouse_width = width;
    g_mouse_height = height;
    g_mouse_x = input64_mouse_clamp_axis((s32)g_mouse_x, g_mouse_width);
    g_mouse_y = input64_mouse_clamp_axis((s32)g_mouse_y, g_mouse_height);
}

u32 input64_read(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    u32 actual_count = input64_read_keyboard(
        input_capability_handle,
        output_address,
        byte_capacity,
        owner_id);

    if ((actual_count != INPUT64_INVALID_RESULT) && (actual_count > 0u))
    {
        ++g_read_count;
        g_byte_count += actual_count;
    }

    return actual_count;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 input64_read_kernel(
    u32 input_capability_handle,
    u8 *output,
    u32 byte_capacity,
    u32 owner_id)
{
    u32 endpoint;
    u32 actual_count = 0u;

    if ((output == 0) || (byte_capacity == 0u) || (byte_capacity > INPUT64_MAX_READ_BYTES))
    {
        return input64_deny();
    }

    endpoint = capability64_route(
        input_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT))
    {
        return input64_deny();
    }

    input64_poll_keyboard();

    while ((actual_count < byte_capacity) && (g_keyboard_pending > 0u))
    {
        output[actual_count] = g_keyboard_queue[g_keyboard_head];
        g_keyboard_head = (g_keyboard_head + 1u) % INPUT64_KEYBOARD_QUEUE_CAPACITY;
        --g_keyboard_pending;
        ++actual_count;
    }

    if (actual_count > 0u)
    {
        ++g_read_count;
        g_byte_count += actual_count;
        ++g_keyboard_read_count;
        g_keyboard_read_byte_count += actual_count;
    }

    return actual_count;
}
#endif

u32 input64_read_line(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    u32 actual_count = input64_read_keyboard_line(
        input_capability_handle,
        output_address,
        byte_capacity,
        owner_id);

    if ((actual_count != INPUT64_INVALID_RESULT) && (actual_count > 0u))
    {
        ++g_read_count;
        ++g_line_count;
        g_byte_count += actual_count;
    }

    return actual_count;
}

u32 input64_read_keyboard(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    u32 endpoint;
    u32 actual_count = 0u;
    u8 *output;

    if ((byte_capacity == 0u)
        || (byte_capacity > INPUT64_MAX_READ_BYTES)
        || !input64_address_writable(output_address, byte_capacity))
    {
        return input64_deny();
    }

    endpoint = capability64_route(
        input_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT))
    {
        return input64_deny();
    }

    input64_poll_keyboard();

    output = (u8 *)output_address;
    while ((actual_count < byte_capacity) && (g_keyboard_pending > 0u))
    {
        output[actual_count] = g_keyboard_queue[g_keyboard_head];
        g_keyboard_head = (g_keyboard_head + 1u) % INPUT64_KEYBOARD_QUEUE_CAPACITY;
        --g_keyboard_pending;
        ++actual_count;
    }

    if (actual_count > 0u)
    {
        ++g_keyboard_read_count;
        g_keyboard_read_byte_count += actual_count;
    }

    return actual_count;
}

u32 input64_read_mouse(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    u32 endpoint;
    u32 packet_capacity;
    u32 actual_count = 0u;
    struct input64_mouse_event *output;

    if ((byte_capacity < INPUT64_MOUSE_RECORD_BYTES)
        || ((byte_capacity % INPUT64_MOUSE_RECORD_BYTES) != 0u)
        || (byte_capacity > INPUT64_MAX_READ_BYTES)
        || !input64_address_writable(output_address, byte_capacity))
    {
        return input64_deny();
    }

    endpoint = capability64_route(
        input_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT))
    {
        return input64_deny();
    }

    input64_poll_mouse();

    packet_capacity = byte_capacity / INPUT64_MOUSE_RECORD_BYTES;
    output = (struct input64_mouse_event *)output_address;
    while ((actual_count < packet_capacity) && (g_mouse_pending > 0u))
    {
        output[actual_count] = g_mouse_queue[g_mouse_head];
        g_mouse_head = (g_mouse_head + 1u) % INPUT64_MOUSE_QUEUE_CAPACITY;
        --g_mouse_pending;
        ++actual_count;
    }

    if (actual_count > 0u)
    {
        ++g_mouse_read_count;
        g_mouse_read_packet_count += actual_count;
    }

    return actual_count;
}

u32 input64_read_keyboard_line(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    u32 endpoint;
    u32 scan_head;
    u32 scan_pending;
    u32 actual_count = 0u;
    u32 consumed_count = 0u;
    u32 edit_count = 0u;
    u32 overlong_fragment = 0u;
    u32 line_complete = 0u;
    u8 line[INPUT64_MAX_READ_BYTES];

    if ((byte_capacity == 0u)
        || (byte_capacity > INPUT64_MAX_READ_BYTES)
        || !input64_address_writable(output_address, byte_capacity))
    {
        return input64_deny();
    }

    endpoint = capability64_route(
        input_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT))
    {
        return input64_deny();
    }

    input64_poll_keyboard();

    scan_head = g_keyboard_head;
    scan_pending = g_keyboard_pending;
    while (scan_pending > 0u)
    {
        u8 input_byte = g_keyboard_queue[scan_head];

        scan_head = (scan_head + 1u) % INPUT64_KEYBOARD_QUEUE_CAPACITY;
        --scan_pending;
        ++consumed_count;

        if (input_byte == (u8)'\n')
        {
            line_complete = 1u;
            break;
        }

        if ((input_byte == (u8)'\b') || (input_byte == 0x7Fu))
        {
            if (actual_count > 0u)
            {
                --actual_count;
            }
            ++edit_count;
            continue;
        }

        if (actual_count >= byte_capacity)
        {
            overlong_fragment = 1u;
            break;
        }

        line[actual_count] = input_byte;
        ++actual_count;
    }

    if (line_complete == 0u)
    {
        if ((overlong_fragment != 0u) || (g_keyboard_pending >= byte_capacity))
        {
            g_keyboard_head = scan_head;
            g_keyboard_pending -= consumed_count;
        }
        return 0u;
    }

    g_keyboard_head = scan_head;
    g_keyboard_pending -= consumed_count;
    input64_copy((void *)output_address, line, actual_count);
    ++g_keyboard_read_count;
    g_keyboard_read_byte_count += actual_count;
    ++g_keyboard_line_count;
    g_keyboard_line_byte_count += actual_count;
    g_keyboard_line_edit_count += edit_count;
    g_edit_count += edit_count;
    return actual_count;
}

u32 input64_read_count(void)
{
    return g_read_count;
}

u32 input64_byte_count(void)
{
    return g_byte_count;
}

u32 input64_denial_count(void)
{
    return g_denial_count;
}

u32 input64_eof_count(void)
{
    return g_eof_count;
}

u32 input64_line_count(void)
{
    return g_line_count;
}

u32 input64_edit_count(void)
{
    return g_edit_count;
}

u32 input64_keyboard_irq_count(void)
{
    return g_keyboard_irq_count;
}

u32 input64_keyboard_poll_count(void)
{
    return g_keyboard_poll_count;
}

u32 input64_keyboard_scancode_count(void)
{
    return g_keyboard_scancode_count;
}

u32 input64_keyboard_byte_count(void)
{
    return g_keyboard_byte_count;
}

u32 input64_keyboard_pending_count(void)
{
    return g_keyboard_pending;
}

u32 input64_keyboard_drop_count(void)
{
    return g_keyboard_drop_count;
}

u32 input64_keyboard_last_scancode(void)
{
    return g_keyboard_last_scancode;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 input64_keyboard_left_shift(void)
{
    return g_keyboard_left_shift;
}

u32 input64_keyboard_right_shift(void)
{
    return g_keyboard_right_shift;
}

u32 input64_keyboard_caps_lock(void)
{
    return g_keyboard_caps_lock;
}

u32 input64_usb_hid_last_modifier(void)
{
    return g_usb_hid_last_modifier;
}
#endif

u32 input64_keyboard_last_byte(void)
{
    return g_keyboard_last_byte;
}

u32 input64_keyboard_read_count(void)
{
    return g_keyboard_read_count;
}

u32 input64_keyboard_read_byte_count(void)
{
    return g_keyboard_read_byte_count;
}

u32 input64_keyboard_line_count(void)
{
    return g_keyboard_line_count;
}

u32 input64_keyboard_line_byte_count(void)
{
    return g_keyboard_line_byte_count;
}

u32 input64_keyboard_line_edit_count(void)
{
    return g_keyboard_line_edit_count;
}

u32 input64_ps2_status_snapshot(void)
{
    return g_ps2_status_snapshot;
}

u32 input64_ps2_present(void)
{
    return g_ps2_present;
}

u32 input64_ps2_enable_attempted(void)
{
    return g_ps2_enable_attempted;
}

u32 input64_ps2_enabled(void)
{
    return g_ps2_enabled;
}

u32 input64_ps2_enable_status(void)
{
    return g_ps2_enable_status;
}

u32 input64_ps2_config_byte(void)
{
    return g_ps2_config_byte;
}

u32 input64_ps2_device_ack(void)
{
    return g_ps2_device_ack;
}

u32 input64_ps2_scanning_enabled(void)
{
    return g_ps2_scanning_enabled;
}

u32 input64_ps2_recommended_scancode_set(void)
{
    return g_ps2_recommended_scancode_set;
}

u32 input64_ps2_self_test(void)
{
    return g_ps2_self_test;
}

u32 input64_ps2_first_port_test(void)
{
    return g_ps2_first_port_test;
}

u32 input64_ps2_reset_ack(void)
{
    return g_ps2_reset_ack;
}

u32 input64_ps2_reset_self_test(void)
{
    return g_ps2_reset_self_test;
}

u32 input64_mouse_found(void)
{
    return g_mouse_found;
}

u32 input64_mouse_enabled(void)
{
    return g_mouse_enabled;
}

u32 input64_mouse_irq_count(void)
{
    return g_mouse_irq_count;
}

u32 input64_mouse_poll_count(void)
{
    return g_mouse_poll_count;
}

u32 input64_mouse_packet_count(void)
{
    return g_mouse_packet_count;
}

u32 input64_mouse_pending_count(void)
{
    return g_mouse_pending;
}

u32 input64_mouse_drop_count(void)
{
    return g_mouse_drop_count;
}

u32 input64_mouse_delta_seen(void)
{
    return g_mouse_delta_seen;
}

u32 input64_mouse_button_seen(void)
{
    return g_mouse_button_seen;
}

u32 input64_mouse_buttons(void)
{
    return g_mouse_buttons;
}

u32 input64_mouse_x(void)
{
    return g_mouse_x;
}

u32 input64_mouse_y(void)
{
    return g_mouse_y;
}

u32 input64_mouse_last_dx(void)
{
    return (u32)g_mouse_last_dx;
}

u32 input64_mouse_last_dy(void)
{
    return (u32)g_mouse_last_dy;
}

u32 input64_mouse_read_count(void)
{
    return g_mouse_read_count;
}

u32 input64_mouse_read_packet_count(void)
{
    return g_mouse_read_packet_count;
}

u32 input64_ps2_mouse_init_done(void)
{
    return g_mouse_enabled;
}

u32 input64_ps2_mouse_aux_enabled(void)
{
    return g_ps2_mouse_aux_enabled;
}

u32 input64_ps2_mouse_config_read(void)
{
    return g_ps2_mouse_config_read;
}

u32 input64_ps2_mouse_config_write(void)
{
    return g_ps2_mouse_config_write;
}

u32 input64_ps2_mouse_irq12_configured(void)
{
    return g_ps2_mouse_irq12_configured;
}

u32 input64_ps2_mouse_enable_command(void)
{
    return g_ps2_mouse_enable_command;
}

u32 input64_ps2_mouse_ack(void)
{
    return g_ps2_mouse_ack;
}

u32 input64_ps2_mouse_raw_byte(void)
{
    return g_ps2_mouse_last_raw_byte;
}

u32 input64_ps2_mouse_bad_start_count(void)
{
    return g_ps2_mouse_bad_start_count;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 input64_ps2_mouse_packet_bytes(void)
{
    return g_ps2_mouse_packet_bytes;
}

u32 input64_ps2_mouse_wheel_enabled(void)
{
    return g_ps2_mouse_wheel_enabled;
}

u32 input64_ps2_mouse_wheel_count(void)
{
    return g_ps2_mouse_wheel_count;
}

u32 input64_usb_hid_mouse_layout_ready(void)
{
    return g_usb_hid_mouse_layout.ready;
}

u32 input64_usb_hid_mouse_layout_reports(void)
{
    return g_usb_hid_mouse_layout_reports;
}

u32 input64_usb_hid_mouse_layout_fallbacks(void)
{
    return g_usb_hid_mouse_layout_fallbacks;
}

u32 input64_usb_hid_mouse_layout_buttons_offset(void)
{
    return g_usb_hid_mouse_layout.buttons_bit_offset;
}

u32 input64_usb_hid_mouse_layout_report_bytes(void)
{
    return g_usb_hid_mouse_layout.report_bytes;
}

u32 input64_usb_hid_mouse_layout_x_offset(void)
{
    return g_usb_hid_mouse_layout.x_bit_offset;
}

u32 input64_usb_hid_mouse_layout_y_offset(void)
{
    return g_usb_hid_mouse_layout.y_bit_offset;
}

u32 input64_usb_hid_mouse_layout_wheel_offset(void)
{
    return g_usb_hid_mouse_layout.wheel_bit_offset;
}
#endif
