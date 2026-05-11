#include "console.h"

enum
{
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25
};

static volatile u16 *const VGA_BUFFER = (u16 *)0xB8000;
static u8 current_color = (CONSOLE_COLOR_LIGHT_GREY | (CONSOLE_COLOR_BLACK << 4));
static u32 cursor_row = 0;
static u32 cursor_column = 0;

static u16 vga_entry(char character, u8 color)
{
    return (u16)character | ((u16)color << 8);
}

static void scroll_if_needed(void)
{
    u32 row;
    u32 column;

    if (cursor_row < VGA_HEIGHT)
    {
        return;
    }

    for (row = 1; row < VGA_HEIGHT; ++row)
    {
        for (column = 0; column < VGA_WIDTH; ++column)
        {
            VGA_BUFFER[(row - 1) * VGA_WIDTH + column] = VGA_BUFFER[row * VGA_WIDTH + column];
        }
    }

    for (column = 0; column < VGA_WIDTH; ++column)
    {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + column] = vga_entry(' ', current_color);
    }

    cursor_row = VGA_HEIGHT - 1;
}

void console_set_color(u8 foreground, u8 background)
{
    current_color = (foreground & 0x0F) | ((background & 0x0F) << 4);
}

void console_clear(void)
{
    u32 row;
    u32 column;

    for (row = 0; row < VGA_HEIGHT; ++row)
    {
        for (column = 0; column < VGA_WIDTH; ++column)
        {
            VGA_BUFFER[row * VGA_WIDTH + column] = vga_entry(' ', current_color);
        }
    }

    cursor_row = 0;
    cursor_column = 0;
}

void console_write_char(char character)
{
    if (character == '\n')
    {
        cursor_column = 0;
        ++cursor_row;
        scroll_if_needed();
        return;
    }

    VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_column] = vga_entry(character, current_color);
    ++cursor_column;

    if (cursor_column >= VGA_WIDTH)
    {
        cursor_column = 0;
        ++cursor_row;
        scroll_if_needed();
    }
}

void console_write_string(const char *text)
{
    while (*text != '\0')
    {
        console_write_char(*text);
        ++text;
    }
}

void console_write_line(const char *text)
{
    console_write_string(text);
    console_write_char('\n');
}

