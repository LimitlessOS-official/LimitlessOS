#include "debug.h"

#include "x86.h"

enum
{
    DEBUG_PORT = 0x00E9
};

void debug_write_char(char character)
{
    outb(DEBUG_PORT, (u8)character);
}

void debug_write_string(const char *text)
{
    while (*text != '\0')
    {
        debug_write_char(*text);
        ++text;
    }
}

