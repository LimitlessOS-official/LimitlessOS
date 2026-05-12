#include "input_x64.h"

#include "arch_build.h"
#include "capability_x64.h"
#include "display_x64.h"
#include "launch_x64.h"
#include "services.h"
#include "services_x64.h"
#include "x64.h"

#define INPUT64_MAX_READ_BYTES 128u
#define INPUT64_KEYBOARD_QUEUE_CAPACITY 256u
#define INPUT64_MOUSE_QUEUE_CAPACITY 64u
#define INPUT64_MOUSE_RECORD_BYTES 20u
#define INPUT64_KERNEL_HIGH_BASE_HIGH32 0xFFFFFFFFu
#define INPUT64_KERNEL_HIGH_BASE_LOW32 0x80000000u
#define INPUT64_PS2_STATUS_PORT 0x64u
#define INPUT64_PS2_DATA_PORT 0x60u
#define INPUT64_PS2_STATUS_OUTPUT_READY 0x01u
#define INPUT64_PS2_STATUS_INPUT_FULL 0x02u
#define INPUT64_PS2_STATUS_AUX_DATA 0x20u
#define INPUT64_PS2_DRAIN_LIMIT 32u
#define INPUT64_PS2_WAIT_LIMIT 100000u
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
#define INPUT64_PS2_CONFIG_IRQ1 0x01u
#define INPUT64_PS2_CONFIG_IRQ12 0x02u
#define INPUT64_PS2_CONFIG_FIRST_PORT_CLOCK_DISABLE 0x10u
#define INPUT64_PS2_CONFIG_SECOND_PORT_CLOCK_DISABLE 0x20u
#define INPUT64_PS2_CONFIG_TRANSLATION 0x40u

struct input64_mouse_event
{
    s32 dx;
    s32 dy;
    u32 buttons;
    u32 x;
    u32 y;
};

static const u8 g_seeded_command[INPUT64_SEEDED_COMMAND_BYTES] = {
    'c', 'a', 't', ' ', 'R', 'E', 'A', 'D', 'M', 'E', '.', 'T', 'X', 'T',
    'h', 'e', 'l', 'p', 'X', '\b', '\n',
    'h', 'e', 'l', 'p', ' ', 'l', 's', '\n',
    'h', 'e', 'l', 'p', ' ', 'c', 'a', 't', '\n',
    'h', 'e', 'l', 'p', ' ', 's', 't', 'a', 't', '\n',
    'h', 'e', 'l', 'p', ' ', 'm', 'k', 'd', 'i', 'r', '\n',
    'h', 'e', 'l', 'p', ' ', 'w', 'r', 'i', 't', 'e', '\n',
    'w', 'r', 'i', 't', 'e', ' ', 'S', 'H', 'E', 'L', 'L', '.', 'T', 'X', 'T', '\n',
    'c', 'a', 't', ' ', 'S', 'H', 'E', 'L', 'L', '.', 'T', 'X', 'T', '\n',
    'a', 'p', 'p', 's', '\n',
    'p', 'w', 'd', '\n',
    'l', 's', ' ', '/', '\n',
    'l', 's', ' ', 'A', 'P', 'P', 'S', '\n',
    'i', 'n', 'f', 'o', ' ', 'l', 's', '\n',
    'i', 'n', 'f', 'o', ' ', 'c', 'a', 't', '\n',
    'i', 'n', 'f', 'o', ' ', 's', 't', 'a', 't', '\n',
    'i', 'n', 'f', 'o', ' ', 'm', 'k', 'd', 'i', 'r', '\n',
    'i', 'n', 'f', 'o', ' ', 'w', 'r', 'i', 't', 'e', '\n',
    'c', 'a', 't', ' ', 'R', 'E', 'A', 'D', 'M', 'E', '.', 'T', 'X', 'T', '\n',
    's', 't', 'a', 't', ' ', 'R', 'E', 'A', 'D', 'M', 'E', '.', 'T', 'X', 'T', '\n',
    'h', 'e', 'l', 'p', 'X', '\n',
    'n', 'o', 'o', 'p', '\n'
};

static u32 g_read_count = 0u;
static u32 g_byte_count = 0u;
static u32 g_denial_count = 0u;
static u32 g_eof_count = 0u;
static u32 g_line_count = 0u;
static u32 g_edit_count = 0u;
static u32 g_cursor = 0u;
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
static u32 g_ps2_mouse_ack = 0u;
static u8 g_ps2_mouse_packet[3];
static u32 g_ps2_mouse_phase = 0u;

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

static s32 input64_sign_extend_byte(u8 value)
{
    return ((value & 0x80u) != 0u) ? (s32)((u32)value | 0xFFFFFF00u) : (s32)value;
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
    g_ps2_mouse_ack = 0u;
    g_ps2_mouse_packet[0] = 0u;
    g_ps2_mouse_packet[1] = 0u;
    g_ps2_mouse_packet[2] = 0u;
    g_ps2_mouse_phase = 0u;
}

static void input64_mouse_publish_diagnostics(void)
{
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
        g_mouse_buttons);
}

static void input64_mouse_enqueue_delta(s32 dx, s32 dy, u32 buttons)
{
    struct input64_mouse_event event;

    g_mouse_found = 1u;
    g_mouse_buttons = buttons & 0x7u;
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
        return;
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
}

static void input64_mouse_accept_ps2_byte(u8 value)
{
    if (g_ps2_mouse_phase == 0u)
    {
        if ((value & 0x08u) == 0u)
        {
            return;
        }
        g_ps2_mouse_packet[0] = value;
        g_ps2_mouse_phase = 1u;
        return;
    }

    g_ps2_mouse_packet[g_ps2_mouse_phase] = value;
    ++g_ps2_mouse_phase;
    if (g_ps2_mouse_phase >= 3u)
    {
        s32 dx = input64_sign_extend_byte(g_ps2_mouse_packet[1]);
        s32 dy = -input64_sign_extend_byte(g_ps2_mouse_packet[2]);
        input64_mouse_enqueue_delta(dx, dy, (u32)(g_ps2_mouse_packet[0] & 0x07u));
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

static u8 input64_usb_hid_translate_key(u8 keycode, u8 shifted)
{
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
        return shifted != 0u ? (u8)(base - 32u) : base;
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

    translated = input64_keyboard_translate_set2_scancode(scancode);
    if (translated != 0u)
    {
        input64_keyboard_enqueue_byte(translated);
    }
}

static void input64_keyboard_accept_set1_scancode(u8 scancode)
{
    u8 translated;

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

    if ((scancode & 0x80u) != 0u)
    {
        return;
    }

    translated = input64_keyboard_translate_scancode(scancode);
    if (translated != 0u)
    {
        input64_keyboard_enqueue_byte(translated);
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
            input64_mouse_accept_ps2_byte(inb(INPUT64_PS2_DATA_PORT));
        }
        else
        {
            input64_keyboard_accept_scancode(inb(INPUT64_PS2_DATA_PORT));
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

    if (input64_ps2_read_data(&response) == 0u)
    {
        return 0u;
    }

    *ack = response;
    return response == INPUT64_PS2_ACK ? 1u : 0u;
}

static u32 input64_ps2_send_aux_command(u8 command, u8 *ack)
{
    u8 response = 0u;

    if (input64_ps2_write_command(INPUT64_PS2_COMMAND_WRITE_SECOND_PORT) == 0u)
    {
        return 0u;
    }

    if (input64_ps2_write_data(command) == 0u)
    {
        return 0u;
    }

    if (input64_ps2_read_data(&response) == 0u)
    {
        return 0u;
    }

    *ack = response;
    return response == INPUT64_PS2_ACK ? 1u : 0u;
}

static void input64_ps2_enable_controller(void)
{
    u8 status = inb(INPUT64_PS2_STATUS_PORT);
    u8 config = 0u;
    u8 ack = 0u;
    u8 mouse_ack = 0u;

    g_ps2_status_snapshot = (u32)status;
    g_ps2_present = (status != 0xFFu) ? 1u : 0u;
    if (g_ps2_present == 0u)
    {
        return;
    }

    g_ps2_enable_attempted = 1u;

    (void)input64_ps2_write_command(INPUT64_PS2_COMMAND_DISABLE_FIRST_PORT);
    (void)input64_ps2_write_command(INPUT64_PS2_COMMAND_DISABLE_SECOND_PORT);
    input64_keyboard_drain_raw();

    if (input64_ps2_write_command(INPUT64_PS2_COMMAND_ENABLE_SECOND_PORT) != 0u)
    {
        g_ps2_mouse_aux_enabled = 1u;
    }

    if (input64_ps2_read_config(&config) == 0u)
    {
        g_ps2_enable_status = g_ps2_status_snapshot;
        input64_mouse_publish_diagnostics();
        return;
    }
    g_ps2_mouse_config_read = 1u;

    config |= INPUT64_PS2_CONFIG_IRQ1 | INPUT64_PS2_CONFIG_IRQ12 | INPUT64_PS2_CONFIG_TRANSLATION;
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

    if (input64_ps2_write_command(INPUT64_PS2_COMMAND_ENABLE_FIRST_PORT) == 0u)
    {
        g_ps2_enable_status = g_ps2_status_snapshot;
        input64_mouse_publish_diagnostics();
        return;
    }

    if (g_ps2_mouse_aux_enabled != 0u)
    {
        if (input64_ps2_send_aux_command(INPUT64_PS2_DEVICE_ENABLE_SCANNING, &mouse_ack) != 0u)
        {
            g_mouse_enabled = 1u;
            g_mouse_found = 1u;
        }
        g_ps2_mouse_enable_command = (mouse_ack == INPUT64_PS2_ACK) ? 1u : 0u;
        g_ps2_mouse_ack = (u32)mouse_ack;
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
    g_cursor = 0u;
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
    input64_poll_mouse();
}

void input64_poll_mouse(void)
{
    ++g_mouse_poll_count;
    input64_keyboard_drain_controller();
    input64_mouse_publish_diagnostics();
}

void input64_handle_keyboard_interrupt(void)
{
    ++g_keyboard_irq_count;
    input64_keyboard_drain_controller();
}

void input64_handle_mouse_interrupt(void)
{
    ++g_mouse_irq_count;
    input64_keyboard_drain_controller();
    input64_mouse_publish_diagnostics();
}

void input64_accept_usb_hid_boot_report(const u8 *report, u32 byte_count)
{
    u8 shifted;
    u32 index;

    if ((report == 0) || (byte_count < 8u))
    {
        return;
    }

    shifted = ((report[0] & 0x22u) != 0u) ? 1u : 0u;
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
        translated = input64_usb_hid_translate_key(keycode, shifted);
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

void input64_accept_usb_hid_mouse_report(const u8 *report, u32 byte_count)
{
    if ((report == 0) || (byte_count < 3u))
    {
        return;
    }

    input64_mouse_enqueue_delta(
        input64_sign_extend_byte(report[1]),
        input64_sign_extend_byte(report[2]),
        (u32)(report[0] & 0x07u));
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
    u32 endpoint;
    u32 remaining;
    u32 actual_count;

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

    if (g_cursor >= (u32)sizeof(g_seeded_command))
    {
        ++g_eof_count;
        return 0u;
    }

    remaining = ((u32)sizeof(g_seeded_command)) - g_cursor;
    actual_count = (byte_capacity < remaining) ? byte_capacity : remaining;
    input64_copy((void *)output_address, &g_seeded_command[g_cursor], actual_count);
    g_cursor += actual_count;
    ++g_read_count;
    g_byte_count += actual_count;
    return actual_count;
}

u32 input64_read_line(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    u32 endpoint;
    u32 scan;
    u32 actual_count;
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

    if (g_cursor >= (u32)sizeof(g_seeded_command))
    {
        ++g_eof_count;
        return 0u;
    }

    scan = g_cursor;
    actual_count = 0u;
    output = (u8 *)output_address;
    while (scan < (u32)sizeof(g_seeded_command))
    {
        u8 input_byte = g_seeded_command[scan];

        if (input_byte == (u8)'\n')
        {
            ++scan;
            break;
        }

        if ((input_byte == (u8)'\b') || (input_byte == 0x7Fu))
        {
            if (actual_count > 0u)
            {
                --actual_count;
            }
            ++g_edit_count;
            ++scan;
            continue;
        }

        if (actual_count >= byte_capacity)
        {
            return input64_deny();
        }

        output[actual_count] = input_byte;
        ++actual_count;
        ++scan;
    }

    g_cursor = scan;
    ++g_read_count;
    ++g_line_count;
    g_byte_count += actual_count;
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
