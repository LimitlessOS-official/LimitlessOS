#include "serial.h"

#include "types.h"
#include "x86.h"

enum
{
    COM1_PORT = 0x3F8
};

static int serial_transmit_ready(void)
{
    return (inb(COM1_PORT + 5) & 0x20) != 0;
}

void serial_init(void)
{
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
}

void serial_write_char(char character)
{
    while (!serial_transmit_ready())
    {
    }

    outb(COM1_PORT, (u8)character);
}

void serial_write_string(const char *text)
{
    while (*text != '\0')
    {
        serial_write_char(*text);
        ++text;
    }
}

