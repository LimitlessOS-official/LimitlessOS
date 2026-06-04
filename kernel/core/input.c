#include "input.h"

#include "klog.h"
#include "userspace.h"
#include "x86.h"

enum
{
    INPUT_QUEUE_CAPACITY = 2048u,
    PS2_STATUS_PORT = 0x64u,
    PS2_DATA_PORT = 0x60u,
    PS2_STATUS_OUTPUT_READY = 0x01u
};

static u8 input_queue[INPUT_QUEUE_CAPACITY];
static u32 input_queue_head = 0u;
static u32 input_queue_tail = 0u;
static u32 input_queue_count = 0u;
static u32 input_drop_count = 0u;
static u8 input_extended_prefix = 0u;

static void input_reset_queue(void)
{
    input_queue_head = 0u;
    input_queue_tail = 0u;
    input_queue_count = 0u;
    input_drop_count = 0u;
    input_extended_prefix = 0u;
}

static void input_enqueue_byte(u8 value)
{
    if (input_queue_count >= INPUT_QUEUE_CAPACITY)
    {
        ++input_drop_count;
        return;
    }

    input_queue[input_queue_tail] = value;
    input_queue_tail = (input_queue_tail + 1u) % INPUT_QUEUE_CAPACITY;
    ++input_queue_count;
}

static void input_enqueue_sequence(const u8 *bytes, u32 byte_count)
{
    u32 index;

    if ((bytes == NULL) || (byte_count == 0u))
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        input_enqueue_byte(bytes[index]);
    }
}

static void input_enqueue_csi_final(u8 final_byte)
{
    const u8 sequence[3] = { 27u, (u8)'[', final_byte };
    input_enqueue_sequence(sequence, 3u);
}

static void input_enqueue_delete_sequence(void)
{
    const u8 sequence[4] = { 27u, (u8)'[', (u8)'3', (u8)'~' };
    input_enqueue_sequence(sequence, 4u);
}

static u8 input_translate_scancode(u8 scancode)
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

void input_init(void)
{
    input_reset_queue();

    klog_write_string("[input] bootstrap queue ");
    klog_write_dec_u32(input_queue_count);
    klog_write_line(" bytes");
}

void input_handle_keyboard_interrupt(void)
{
    u8 scancode;
    u8 translated;

    if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_READY) == 0u)
    {
        return;
    }

    scancode = inb(PS2_DATA_PORT);
    if (scancode == 0xE0u)
    {
        input_extended_prefix = 1u;
        return;
    }

    if (input_extended_prefix != 0u)
    {
        input_extended_prefix = 0u;
        if ((scancode & 0x80u) != 0u)
        {
            return;
        }

        switch (scancode)
        {
            case 0x47:
                input_enqueue_csi_final((u8)'H');
                userspace_note_input_ready();
                return;

            case 0x48:
                input_enqueue_csi_final((u8)'A');
                userspace_note_input_ready();
                return;

            case 0x4B:
                input_enqueue_csi_final((u8)'D');
                userspace_note_input_ready();
                return;

            case 0x4D:
                input_enqueue_csi_final((u8)'C');
                userspace_note_input_ready();
                return;

            case 0x4F:
                input_enqueue_csi_final((u8)'F');
                userspace_note_input_ready();
                return;

            case 0x50:
                input_enqueue_csi_final((u8)'B');
                userspace_note_input_ready();
                return;

            case 0x53:
                input_enqueue_delete_sequence();
                userspace_note_input_ready();
                return;

            default:
                return;
        }
    }

    if ((scancode & 0x80u) != 0u)
    {
        return;
    }

    translated = input_translate_scancode(scancode);
    if (translated != 0u)
    {
        input_enqueue_byte(translated);
        userspace_note_input_ready();
    }
}

u32 input_read(u8 *destination_bytes, u32 byte_capacity)
{
    u32 copied = 0u;

    if ((destination_bytes == NULL) || (byte_capacity == 0u))
    {
        return 0u;
    }

    while ((copied < byte_capacity) && (input_queue_count > 0u))
    {
        destination_bytes[copied] = input_queue[input_queue_head];
        input_queue_head = (input_queue_head + 1u) % INPUT_QUEUE_CAPACITY;
        --input_queue_count;
        ++copied;

        if (destination_bytes[copied - 1u] == '\n')
        {
            break;
        }
    }

    return copied;
}

u32 input_pending_byte_count(void)
{
    return input_queue_count;
}
