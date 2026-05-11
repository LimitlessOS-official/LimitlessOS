#include "klog.h"

#include "console.h"
#include "debug.h"
#include "serial.h"

static void klog_write_buffer(const char *buffer, u32 length)
{
    u32 index;

    for (index = 0; index < length; ++index)
    {
        console_write_char(buffer[index]);
        serial_write_char(buffer[index]);
        debug_write_char(buffer[index]);
    }
}

void klog_write_bytes(const u8 *bytes, u32 length)
{
    klog_write_buffer((const char *)bytes, length);
}

void klog_write_string(const char *text)
{
    while (*text != '\0')
    {
        console_write_char(*text);
        serial_write_char(*text);
        debug_write_char(*text);
        ++text;
    }
}

void klog_write_line(const char *text)
{
    klog_write_string(text);
    klog_newline();
}

void klog_write_dec_u32(u32 value)
{
    char digits[10];
    u32 count = 0;
    u32 index;

    if (value == 0)
    {
        klog_write_string("0");
        return;
    }

    while (value != 0)
    {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (index = 0; index < count / 2; ++index)
    {
        char swap = digits[index];
        digits[index] = digits[count - index - 1];
        digits[count - index - 1] = swap;
    }

    klog_write_buffer(digits, count);
}

void klog_write_hex_u32(u32 value)
{
    static const char HEX[] = "0123456789ABCDEF";
    char digits[10];
    u32 index;

    digits[0] = '0';
    digits[1] = 'x';

    for (index = 0; index < 8; ++index)
    {
        u32 shift = (7 - index) * 4;
        digits[index + 2] = HEX[(value >> shift) & 0x0F];
    }

    klog_write_buffer(digits, 10);
}

void klog_newline(void)
{
    console_write_char('\n');
    serial_write_string("\r\n");
    debug_write_char('\n');
}
