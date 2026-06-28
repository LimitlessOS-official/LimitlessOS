#include "console_x64.h"

#include "capability_x64.h"
#include "display_x64.h"
#include "launch_x64.h"
#include "runtime_image_x64.h"
#include "services.h"
#include "services_x64.h"
#include "x64.h"

#define CONSOLE64_MAX_WRITE_BYTES 512u
#define CONSOLE64_KERNEL_HIGH_BASE_HIGH32 0xFFFFFFFFu
#define CONSOLE64_KERNEL_HIGH_BASE_LOW32 0x80000000u

enum
{
    CONSOLE64_VGA_WIDTH = 80u,
    CONSOLE64_VGA_HEIGHT = 25u
};

static u32 g_write_count = 0u;
static u32 g_byte_count = 0u;
static u32 g_denial_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_capture_active = 0u;
static u32 g_capture_owner = 0u;
static u32 g_capture_capability = CAPABILITY64_INVALID_HANDLE;
static u8 *g_capture_buffer = (u8 *)0;
static u32 g_capture_capacity = 0u;
static u32 g_capture_offset = 0u;
static u32 g_capture_truncated = 0u;
static u32 g_capture_writes = 0u;
#endif
#ifndef LIMITLESS_X64_UEFI_KERNEL
static volatile u16 *const g_console64_vga = (volatile u16 *)(u64)0x00000000000B8000ull;
static u32 g_vga_row = 0u;
static u32 g_vga_column = 0u;
static u8 g_vga_color = 0x1Fu;
#endif

static void console64_debug_write_char(char character)
{
    outb(0x00E9u, (u8)character);
}

#ifndef LIMITLESS_X64_UEFI_KERNEL
static void console64_scroll_if_needed(void)
{
    u32 row;
    u32 column;

    if (g_vga_row < CONSOLE64_VGA_HEIGHT)
    {
        return;
    }

    for (row = 1u; row < CONSOLE64_VGA_HEIGHT; ++row)
    {
        for (column = 0u; column < CONSOLE64_VGA_WIDTH; ++column)
        {
            g_console64_vga[(row - 1u) * CONSOLE64_VGA_WIDTH + column] =
                g_console64_vga[row * CONSOLE64_VGA_WIDTH + column];
        }
    }

    for (column = 0u; column < CONSOLE64_VGA_WIDTH; ++column)
    {
        g_console64_vga[(CONSOLE64_VGA_HEIGHT - 1u) * CONSOLE64_VGA_WIDTH + column] =
            (u16)' ' | ((u16)g_vga_color << 8);
    }

    g_vga_row = CONSOLE64_VGA_HEIGHT - 1u;
}
#endif

static void console64_vga_write_char(char character)
{
#ifdef LIMITLESS_X64_UEFI_KERNEL
    (void)character;
    return;
#else
    if (character == '\n')
    {
        g_vga_column = 0u;
        ++g_vga_row;
        console64_scroll_if_needed();
        return;
    }

    if ((character == '\b') || ((u8)character == 0x7Fu))
    {
        if (g_vga_column > 0u)
        {
            --g_vga_column;
        }
        else if (g_vga_row > 0u)
        {
            --g_vga_row;
            g_vga_column = CONSOLE64_VGA_WIDTH - 1u;
        }
        g_console64_vga[g_vga_row * CONSOLE64_VGA_WIDTH + g_vga_column] =
            (u16)' ' | ((u16)g_vga_color << 8);
        return;
    }

    g_console64_vga[g_vga_row * CONSOLE64_VGA_WIDTH + g_vga_column] =
        (u16)character | ((u16)g_vga_color << 8);
    ++g_vga_column;

    if (g_vga_column >= CONSOLE64_VGA_WIDTH)
    {
        g_vga_column = 0u;
        ++g_vga_row;
        console64_scroll_if_needed();
    }
#endif
}

static int console64_range_overflows(u64 address, u32 byte_count)
{
    u64 end;

    if (byte_count == 0u)
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return end < address;
}

static int console64_address_is_kernel_high(u64 address, u32 byte_count)
{
    u64 end;

    if (console64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return ((u32)(address >> 32) >= CONSOLE64_KERNEL_HIGH_BASE_HIGH32)
        && ((u32)address >= CONSOLE64_KERNEL_HIGH_BASE_LOW32)
        && (end >= address);
}

static int console64_address_is_user_stack(u64 address, u32 byte_count)
{
    u64 stack_base = (u64)(LAUNCH64_USER_STACK_TOP - LAUNCH64_USER_STACK_BYTES);
    u64 stack_top = (u64)LAUNCH64_USER_STACK_TOP;
    u64 end;

    if (console64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= stack_base) && (end <= stack_top);
}

static int console64_address_is_user_image(u64 address, u32 byte_count)
{
    u64 image_base = (u64)LAUNCH64_USER_IMAGE_BASE;
    u64 image_end = image_base + (u64)runtime64_transfer_image_size();
    u64 end;

    if (console64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= image_base) && (end <= image_end);
}

static int console64_address_readable(u64 address, u32 byte_count)
{
    if (byte_count == 0u)
    {
        return 1;
    }

    if (address == 0ull)
    {
        return 0;
    }

    return console64_address_is_kernel_high(address, byte_count)
        || console64_address_is_user_stack(address, byte_count)
        || console64_address_is_user_image(address, byte_count);
}

void console64_init(void)
{
    g_write_count = 0u;
    g_byte_count = 0u;
    g_denial_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_capture_active = 0u;
    g_capture_owner = 0u;
    g_capture_capability = CAPABILITY64_INVALID_HANDLE;
    g_capture_buffer = (u8 *)0;
    g_capture_capacity = 0u;
    g_capture_offset = 0u;
    g_capture_truncated = 0u;
    g_capture_writes = 0u;
#endif
#ifndef LIMITLESS_X64_UEFI_KERNEL
    g_vga_row = 0u;
    g_vga_column = 0u;
    g_vga_color = 0x1Fu;
#endif
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 console64_capture_append(const u8 *bytes, u32 byte_count)
{
    u32 index;
    u32 writable = byte_count;

    if ((g_capture_active == 0u) || (bytes == (const u8 *)0))
    {
        return 0u;
    }
    if ((g_capture_offset + writable) > g_capture_capacity)
    {
        writable = g_capture_capacity - g_capture_offset;
        g_capture_truncated = 1u;
    }
    for (index = 0u; index < writable; ++index)
    {
        g_capture_buffer[g_capture_offset + index] = bytes[index];
    }
    g_capture_offset += writable;
    ++g_capture_writes;
    return writable;
}

u32 console64_capture_begin(
    u32 console_capability_handle,
    u32 owner_id,
    u8 *buffer,
    u32 byte_capacity)
{
    u32 endpoint;

    if ((g_capture_active != 0u)
        || (buffer == (u8 *)0)
        || (byte_capacity == 0u))
    {
        ++g_denial_count;
        return 0u;
    }

    endpoint = capability64_route(
        console_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE))
    {
        ++g_denial_count;
        return 0u;
    }

    g_capture_active = 1u;
    g_capture_owner = owner_id;
    g_capture_capability = console_capability_handle;
    g_capture_buffer = buffer;
    g_capture_capacity = byte_capacity;
    g_capture_offset = 0u;
    g_capture_truncated = 0u;
    g_capture_writes = 0u;
    return 1u;
}

u32 console64_capture_end(
    u32 console_capability_handle,
    u32 owner_id,
    u32 *bytes_captured,
    u32 *truncated)
{
    if ((g_capture_active == 0u)
        || (console_capability_handle != g_capture_capability)
        || (owner_id != g_capture_owner)
        || (bytes_captured == (u32 *)0)
        || (truncated == (u32 *)0))
    {
        ++g_denial_count;
        return 0u;
    }

    *bytes_captured = g_capture_offset;
    *truncated = g_capture_truncated;
    g_capture_active = 0u;
    g_capture_owner = 0u;
    g_capture_capability = CAPABILITY64_INVALID_HANDLE;
    g_capture_buffer = (u8 *)0;
    g_capture_capacity = 0u;
    g_capture_offset = 0u;
    g_capture_truncated = 0u;
    g_capture_writes = 0u;
    return 1u;
}
#endif

u32 console64_write(u32 console_capability_handle, u64 input_address, u32 byte_count, u32 owner_id)
{
    const u8 *bytes;
    u32 endpoint;
    u32 index;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if ((byte_count > CONSOLE64_MAX_WRITE_BYTES)
        || !console64_address_readable(input_address, byte_count))
    {
        ++g_denial_count;
        return CONSOLE64_INVALID_RESULT;
    }

    endpoint = capability64_route(
        console_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE))
    {
        ++g_denial_count;
        return CONSOLE64_INVALID_RESULT;
    }

    bytes = (const u8 *)input_address;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_capture_active != 0u)
    {
        (void)console64_capture_append(bytes, byte_count);
        ++g_write_count;
        g_byte_count += byte_count;
        return byte_count;
    }
#endif
    for (index = 0u; index < byte_count; ++index)
    {
        console64_debug_write_char((char)bytes[index]);
        console64_vga_write_char((char)bytes[index]);
    }
    (void)display64_write_console_stream(input_address, byte_count);

    ++g_write_count;
    g_byte_count += byte_count;
    return byte_count;
}

u32 console64_write_kernel(
    u32 console_capability_handle,
    const u8 *input,
    u32 byte_count,
    u32 owner_id)
{
    u32 endpoint;
    u32 index;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if ((input == 0) || (byte_count > CONSOLE64_MAX_WRITE_BYTES))
    {
        ++g_denial_count;
        return CONSOLE64_INVALID_RESULT;
    }

    endpoint = capability64_route(
        console_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE))
    {
        ++g_denial_count;
        return CONSOLE64_INVALID_RESULT;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_capture_active != 0u)
    {
        (void)console64_capture_append(input, byte_count);
        ++g_write_count;
        g_byte_count += byte_count;
        return byte_count;
    }
#endif
    for (index = 0u; index < byte_count; ++index)
    {
        console64_debug_write_char((char)input[index]);
        console64_vga_write_char((char)input[index]);
    }
    (void)display64_write_console_stream_kernel(input, byte_count);

    ++g_write_count;
    g_byte_count += byte_count;
    return byte_count;
}

u32 console64_write_count(void)
{
    return g_write_count;
}

u32 console64_byte_count(void)
{
    return g_byte_count;
}

u32 console64_denial_count(void)
{
    return g_denial_count;
}
