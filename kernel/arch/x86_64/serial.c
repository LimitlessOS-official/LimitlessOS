#include "serial.h"

#include "types.h"
#include "x64.h"

#define SERIAL64_COM1 0x03F8u
#define SERIAL64_THR_EMPTY 0x20u
#define SERIAL64_POLL_LIMIT 100000u

static u32 g_serial64_initialized = 0u;

static void serial64_write_raw(char character)
{
    u32 poll;

    if (g_serial64_initialized == 0u)
    {
        return;
    }

    for (poll = 0u; poll < SERIAL64_POLL_LIMIT; ++poll)
    {
        if ((inb((u16)(SERIAL64_COM1 + 5u)) & SERIAL64_THR_EMPTY) != 0u)
        {
            outb((u16)SERIAL64_COM1, (u8)character);
            return;
        }
    }
}

void serial_init(void)
{
    outb((u16)(SERIAL64_COM1 + 1u), 0x00u);
    outb((u16)(SERIAL64_COM1 + 3u), 0x80u);
    outb((u16)(SERIAL64_COM1 + 0u), 0x01u);
    outb((u16)(SERIAL64_COM1 + 1u), 0x00u);
    outb((u16)(SERIAL64_COM1 + 3u), 0x03u);
    outb((u16)(SERIAL64_COM1 + 2u), 0xC7u);
    outb((u16)(SERIAL64_COM1 + 4u), 0x0Bu);
    g_serial64_initialized = 1u;
}

void serial_write_char(char character)
{
    if (character == '\n')
    {
        serial64_write_raw('\r');
    }
    serial64_write_raw(character);
}

void serial_write_string(const char *text)
{
    if (text == 0)
    {
        return;
    }

    while (*text != '\0')
    {
        serial_write_char(*text);
        ++text;
    }
}
