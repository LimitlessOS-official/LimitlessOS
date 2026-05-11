#include "shell_x64.h"

#include "console_x64.h"
#include "fs_x64.h"
#include "launch_x64.h"
#include "ramfs.h"
#include "runtime_image_x64.h"
#include "types.h"

#define SHELL64_MAX_LINE_BYTES 128u
#define SHELL64_MAX_PATH_BYTES 128u
#define SHELL64_IO_BYTES 512u
#define SHELL64_KERNEL_HIGH_BASE_HIGH32 0xFFFFFFFFu
#define SHELL64_KERNEL_HIGH_BASE_LOW32 0x80000000u

static u8 g_shell64_line[SHELL64_MAX_LINE_BYTES + 1u];
static u8 g_shell64_path_a[SHELL64_MAX_PATH_BYTES];
static u8 g_shell64_path_b[SHELL64_MAX_PATH_BYTES];
static u8 g_shell64_pair[SHELL64_MAX_PATH_BYTES * 2u];
static u8 g_shell64_io[SHELL64_IO_BYTES];
static u8 g_shell64_stat[64u];

static u32 shell64_length(const char *text)
{
    u32 length = 0u;

    if (text == 0)
    {
        return 0u;
    }

    while (text[length] != '\0')
    {
        ++length;
    }

    return length;
}

static void shell64_zero(u8 *bytes, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static void shell64_copy(u8 *destination, const u8 *source, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        destination[index] = source[index];
    }
}

static int shell64_range_overflows(u64 address, u32 byte_count)
{
    u64 end;

    if (byte_count == 0u)
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return end < address;
}

static int shell64_address_is_kernel_high(u64 address, u32 byte_count)
{
    u64 end;

    if (shell64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return ((u32)(address >> 32) >= SHELL64_KERNEL_HIGH_BASE_HIGH32)
        && ((u32)address >= SHELL64_KERNEL_HIGH_BASE_LOW32)
        && (end >= address);
}

static int shell64_address_is_user_stack(u64 address, u32 byte_count)
{
    u64 stack_base = (u64)(LAUNCH64_USER_STACK_TOP - LAUNCH64_USER_STACK_BYTES);
    u64 stack_top = (u64)LAUNCH64_USER_STACK_TOP;
    u64 end;

    if (shell64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= stack_base) && (end <= stack_top);
}

static int shell64_address_is_user_image(u64 address, u32 byte_count)
{
    u64 image_base = (u64)LAUNCH64_USER_IMAGE_BASE;
    u64 image_end = image_base + (u64)runtime64_transfer_image_size();
    u64 end;

    if (shell64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= image_base) && (end <= image_end);
}

static int shell64_address_readable(u64 address, u32 byte_count)
{
    if (byte_count == 0u)
    {
        return 1;
    }

    if (address == 0ull)
    {
        return 0;
    }

    return shell64_address_is_kernel_high(address, byte_count)
        || shell64_address_is_user_stack(address, byte_count)
        || shell64_address_is_user_image(address, byte_count);
}

static u32 shell64_write(u32 console_capability_handle, u32 owner_id, const u8 *bytes, u32 byte_count)
{
    return console64_write_kernel(console_capability_handle, bytes, byte_count, owner_id);
}

static u32 shell64_write_text(u32 console_capability_handle, u32 owner_id, const char *text)
{
    return shell64_write(
        console_capability_handle,
        owner_id,
        (const u8 *)text,
        shell64_length(text));
}

static u8 shell64_lower(u8 value)
{
    if ((value >= (u8)'A') && (value <= (u8)'Z'))
    {
        return (u8)(value + ((u8)'a' - (u8)'A'));
    }

    return value;
}

static u8 shell64_upper(u8 value)
{
    if ((value >= (u8)'a') && (value <= (u8)'z'))
    {
        return (u8)(value - ((u8)'a' - (u8)'A'));
    }

    return value;
}

static int shell64_is_space(u8 value)
{
    return (value == (u8)' ') || (value == (u8)'\t');
}

static u32 shell64_skip_spaces(u32 cursor, u32 line_length)
{
    while ((cursor < line_length) && shell64_is_space(g_shell64_line[cursor]))
    {
        ++cursor;
    }

    return cursor;
}

static u32 shell64_next_token(u32 *cursor, u32 line_length, u32 *token_start)
{
    u32 start;

    *cursor = shell64_skip_spaces(*cursor, line_length);
    start = *cursor;
    while ((*cursor < line_length) && !shell64_is_space(g_shell64_line[*cursor]))
    {
        ++*cursor;
    }

    *token_start = start;
    return *cursor - start;
}

static int shell64_token_equals(u32 token_start, u32 token_length, const char *text)
{
    u32 expected_length = shell64_length(text);
    u32 index;

    if (token_length != expected_length)
    {
        return 0;
    }

    for (index = 0u; index < token_length; ++index)
    {
        if (shell64_lower(g_shell64_line[token_start + index]) != (u8)text[index])
        {
            return 0;
        }
    }

    return 1;
}

static u32 shell64_normalize_path(u32 token_start, u32 token_length, u8 *destination)
{
    u32 source_index = token_start;
    u32 copy_index;

    while ((token_length > 1u) && (g_shell64_line[source_index] == (u8)'/'))
    {
        ++source_index;
        --token_length;
    }

    if ((token_length == 0u) || (token_length >= SHELL64_MAX_PATH_BYTES))
    {
        return 0u;
    }

    for (copy_index = 0u; copy_index < token_length; ++copy_index)
    {
        destination[copy_index] = shell64_upper(g_shell64_line[source_index + copy_index]);
    }

    return token_length;
}

static int shell64_token_is_root(u32 token_start, u32 token_length)
{
    return (token_length == 1u) && (g_shell64_line[token_start] == (u8)'/');
}

static int shell64_token_is_apps_path(u32 token_start, u32 token_length)
{
    if (shell64_token_equals(token_start, token_length, "apps"))
    {
        return 1;
    }

    return (token_length == 5u)
        && (g_shell64_line[token_start] == (u8)'/')
        && (shell64_lower(g_shell64_line[token_start + 1u]) == (u8)'a')
        && (shell64_lower(g_shell64_line[token_start + 2u]) == (u8)'p')
        && (shell64_lower(g_shell64_line[token_start + 3u]) == (u8)'p')
        && (shell64_lower(g_shell64_line[token_start + 4u]) == (u8)'s');
}

static u32 shell64_open_path(
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id,
    u32 *borrowed_root)
{
    u32 path_length;

    *borrowed_root = 0u;
    if ((token_length == 0u) || shell64_token_is_root(token_start, token_length))
    {
        *borrowed_root = 1u;
        return root_capability_handle;
    }

    path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_a);
    if (path_length == 0u)
    {
        return FS64_INVALID_HANDLE;
    }

    return fs64_open_kernel(root_capability_handle, g_shell64_path_a, path_length, owner_id);
}

static u32 shell64_create_path(
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 node_type,
    u32 owner_id)
{
    u32 path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_a);

    if ((path_length == 0u) || shell64_token_is_root(token_start, token_length))
    {
        return FS64_INVALID_HANDLE;
    }

    return fs64_create_kernel(
        root_capability_handle,
        g_shell64_path_a,
        path_length,
        node_type,
        owner_id);
}

static u32 shell64_delete_path(
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id)
{
    u32 path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_a);

    if ((path_length == 0u) || shell64_token_is_root(token_start, token_length))
    {
        return 0u;
    }

    return fs64_delete_kernel(root_capability_handle, g_shell64_path_a, path_length, owner_id);
}

static u32 shell64_stat_size(const u8 *bytes, u32 byte_count)
{
    u32 index;

    for (index = 0u; (index + 5u) < byte_count; ++index)
    {
        u32 value = 0u;

        if ((bytes[index] != (u8)'s')
            || (bytes[index + 1u] != (u8)'i')
            || (bytes[index + 2u] != (u8)'z')
            || (bytes[index + 3u] != (u8)'e')
            || (bytes[index + 4u] != (u8)'='))
        {
            continue;
        }

        index += 5u;
        while ((index < byte_count)
            && (bytes[index] >= (u8)'0')
            && (bytes[index] <= (u8)'9'))
        {
            value = (value * 10u) + (u32)(bytes[index] - (u8)'0');
            ++index;
        }

        return value;
    }

    return 0u;
}

static void shell64_write_newline_if_needed(u32 console_capability_handle, u32 owner_id, u32 byte_count)
{
    if ((byte_count == 0u) || (g_shell64_io[byte_count - 1u] != (u8)'\n'))
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "\n");
    }
}

static u32 shell64_print_usage(u32 console_capability_handle, u32 owner_id, u32 token_start, u32 token_length)
{
    if (shell64_token_equals(token_start, token_length, "ls"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: ls [path]\n");
    }

    if (shell64_token_equals(token_start, token_length, "cat"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: cat <path>\n");
    }

    if (shell64_token_equals(token_start, token_length, "write"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: write <path> <text>\n");
    }

    if (shell64_token_equals(token_start, token_length, "mkdir"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: mkdir <path>\n");
    }

    if (shell64_token_equals(token_start, token_length, "stat"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: stat <path>\n");
    }

    if (shell64_token_equals(token_start, token_length, "copy"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: copy <source> <dest>\n");
    }

    if (shell64_token_equals(token_start, token_length, "delete"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: delete <path>\n");
    }

    if (shell64_token_equals(token_start, token_length, "rename"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: rename <from> <to>\n");
    }

    if (shell64_token_equals(token_start, token_length, "move"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: move <source> <dest>\n");
    }

    if (shell64_token_equals(token_start, token_length, "touch"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: touch <path>\n");
    }

    if (shell64_token_equals(token_start, token_length, "append"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: append <path> <text>\n");
    }

    if (shell64_token_equals(token_start, token_length, "apps"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: apps\n");
    }

    if (shell64_token_equals(token_start, token_length, "info"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: info <command>\n");
    }

    if (shell64_token_equals(token_start, token_length, "pwd"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: pwd\n");
    }

    if (shell64_token_equals(token_start, token_length, "help"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: help [command]\n");
    }

    if (shell64_token_equals(token_start, token_length, "ask"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "unavailable in M1: ASK is not an AI feature\n");
    }

    if (shell64_token_equals(token_start, token_length, "echo")
        || shell64_token_equals(token_start, token_length, "say")
        || shell64_token_equals(token_start, token_length, "show")
        || shell64_token_equals(token_start, token_length, "list")
        || shell64_token_equals(token_start, token_length, "make")
        || shell64_token_equals(token_start, token_length, "put")
        || shell64_token_equals(token_start, token_length, "swap")
        || shell64_token_equals(token_start, token_length, "shift"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "unavailable in M1: alias/experimental command\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "unknown command\n");
}

static int shell64_token_is_product_command(u32 token_start, u32 token_length)
{
    return shell64_token_equals(token_start, token_length, "append")
        || shell64_token_equals(token_start, token_length, "cat")
        || shell64_token_equals(token_start, token_length, "copy")
        || shell64_token_equals(token_start, token_length, "delete")
        || shell64_token_equals(token_start, token_length, "ls")
        || shell64_token_equals(token_start, token_length, "mkdir")
        || shell64_token_equals(token_start, token_length, "move")
        || shell64_token_equals(token_start, token_length, "rename")
        || shell64_token_equals(token_start, token_length, "stat")
        || shell64_token_equals(token_start, token_length, "touch")
        || shell64_token_equals(token_start, token_length, "write");
}

static int shell64_token_is_builtin_command(u32 token_start, u32 token_length)
{
    return shell64_token_equals(token_start, token_length, "apps")
        || shell64_token_equals(token_start, token_length, "help")
        || shell64_token_equals(token_start, token_length, "info")
        || shell64_token_equals(token_start, token_length, "pwd");
}

static u32 shell64_info(u32 console_capability_handle, u32 owner_id, u32 token_start, u32 token_length)
{
    if (shell64_token_is_builtin_command(token_start, token_length))
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "cmd=");
        (void)shell64_write(console_capability_handle, owner_id, g_shell64_line + token_start, token_length);
        (void)shell64_write_text(console_capability_handle, owner_id, " type=shell-builtin source=kernel\n");
        return shell64_print_usage(console_capability_handle, owner_id, token_start, token_length);
    }

    if (!shell64_token_is_product_command(token_start, token_length))
    {
        return shell64_print_usage(console_capability_handle, owner_id, token_start, token_length);
    }

    (void)shell64_write_text(console_capability_handle, owner_id, "cmd=");
    (void)shell64_write(console_capability_handle, owner_id, g_shell64_line + token_start, token_length);
    (void)shell64_write_text(console_capability_handle, owner_id, " auth=buffer base policy=capability-path source=disk-preferred\n");
    return shell64_print_usage(console_capability_handle, owner_id, token_start, token_length);
}

static u32 shell64_read_file(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 file_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);
    u32 byte_count;

    if ((file_capability == FS64_INVALID_HANDLE) || (borrowed_root != 0u))
    {
        return shell64_write_text(console_capability_handle, owner_id, "not found\n");
    }

    shell64_zero(g_shell64_io, sizeof(g_shell64_io));
    byte_count = fs64_read_kernel(file_capability, g_shell64_io, 0u, 256u, owner_id);
    (void)fs64_revoke(file_capability, owner_id);
    if (byte_count == FS64_INVALID_HANDLE)
    {
        return shell64_write_text(console_capability_handle, owner_id, "read failed\n");
    }

    if (byte_count > 0u)
    {
        (void)shell64_write(console_capability_handle, owner_id, g_shell64_io, byte_count);
        shell64_write_newline_if_needed(console_capability_handle, owner_id, byte_count);
    }
    return byte_count;
}

static u32 shell64_list_path(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 dir_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);
    u32 byte_count;

    if (dir_capability == FS64_INVALID_HANDLE)
    {
        return shell64_write_text(console_capability_handle, owner_id, "not found\n");
    }

    shell64_zero(g_shell64_io, sizeof(g_shell64_io));
    byte_count = fs64_list_kernel(dir_capability, g_shell64_io, sizeof(g_shell64_io), owner_id);
    if (borrowed_root == 0u)
    {
        (void)fs64_revoke(dir_capability, owner_id);
    }

    if (byte_count == FS64_INVALID_HANDLE)
    {
        return shell64_write_text(console_capability_handle, owner_id, "list failed\n");
    }

    if (byte_count > 0u)
    {
        (void)shell64_write(console_capability_handle, owner_id, g_shell64_io, byte_count);
    }

    return byte_count;
}

static u32 shell64_list_apps(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 owner_id)
{
    (void)root_capability_handle;

    return shell64_write_text(
        console_capability_handle,
        owner_id,
        "Product apps:\n"
        "APPEND\n"
        "CAT\n"
        "COPY\n"
        "DELETE\n"
        "LS\n"
        "MKDIR\n"
        "MOVE\n"
        "RENAME\n"
        "STAT\n"
        "TOUCH\n"
        "WRITE\n"
        "Unavailable in M1:\n"
        "ASK (not AI)\n"
        "ECHO\n"
        "Aliases: SAY SHOW LIST MAKE PUT SWAP SHIFT\n"
        "GUI\n"
        "Network\n"
        "Installer\n"
        "Package manager\n"
        "AI assistant\n"
        "Internal files hidden from app output: HELLO.TXT INDEX.TXT\n");
}

static u32 shell64_stat_path(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 node_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);
    u32 byte_count;

    if (node_capability == FS64_INVALID_HANDLE)
    {
        return shell64_write_text(console_capability_handle, owner_id, "not found\n");
    }

    shell64_zero(g_shell64_stat, sizeof(g_shell64_stat));
    byte_count = fs64_stat_kernel(node_capability, g_shell64_stat, sizeof(g_shell64_stat), owner_id);
    if (borrowed_root == 0u)
    {
        (void)fs64_revoke(node_capability, owner_id);
    }

    if ((byte_count == 0u) || (byte_count == FS64_INVALID_HANDLE))
    {
        return shell64_write_text(console_capability_handle, owner_id, "stat failed\n");
    }

    (void)shell64_write(console_capability_handle, owner_id, g_shell64_stat, byte_count);
    (void)shell64_write_text(console_capability_handle, owner_id, "\n");
    return byte_count;
}

static u32 shell64_write_file(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 text_start,
    u32 text_length,
    u32 owner_id)
{
    u32 file_capability;
    u32 path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_a);
    u32 byte_count = 0u;

    if ((path_length == 0u) || (text_length == 0u) || shell64_token_is_root(token_start, token_length))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: write <path> <text>\n");
    }

    (void)fs64_delete_kernel(root_capability_handle, g_shell64_path_a, path_length, owner_id);
    file_capability = fs64_create_kernel(
        root_capability_handle,
        g_shell64_path_a,
        path_length,
        RAMFS_NODE_FILE,
        owner_id);
    if (file_capability == FS64_INVALID_HANDLE)
    {
        return shell64_write_text(console_capability_handle, owner_id, "write failed\n");
    }

    if (text_length > 0u)
    {
        byte_count = fs64_write_kernel(
            file_capability,
            g_shell64_line + text_start,
            0u,
            text_length,
            owner_id);
    }
    (void)fs64_revoke(file_capability, owner_id);

    if (byte_count != text_length)
    {
        return shell64_write_text(console_capability_handle, owner_id, "write failed\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "ok\n");
}

static u32 shell64_append_file(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 text_start,
    u32 text_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 file_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);
    u32 file_size = 0u;
    u32 byte_count;

    if ((borrowed_root != 0u) || (text_length == 0u))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: append <path> <text>\n");
    }

    if (file_capability == FS64_INVALID_HANDLE)
    {
        file_capability = shell64_create_path(
            root_capability_handle,
            token_start,
            token_length,
            RAMFS_NODE_FILE,
            owner_id);
    }
    else
    {
        shell64_zero(g_shell64_stat, sizeof(g_shell64_stat));
        byte_count = fs64_stat_kernel(file_capability, g_shell64_stat, sizeof(g_shell64_stat), owner_id);
        if (byte_count != FS64_INVALID_HANDLE)
        {
            file_size = shell64_stat_size(g_shell64_stat, byte_count);
        }
    }

    if (file_capability == FS64_INVALID_HANDLE)
    {
        return shell64_write_text(console_capability_handle, owner_id, "append failed\n");
    }

    byte_count = fs64_write_kernel(
        file_capability,
        g_shell64_line + text_start,
        file_size,
        text_length,
        owner_id);
    (void)fs64_revoke(file_capability, owner_id);
    if (byte_count != text_length)
    {
        return shell64_write_text(console_capability_handle, owner_id, "append failed\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "ok\n");
}

static u32 shell64_copy_file(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 source_start,
    u32 source_length,
    u32 destination_start,
    u32 destination_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 source_capability = shell64_open_path(
        root_capability_handle,
        source_start,
        source_length,
        owner_id,
        &borrowed_root);
    u32 destination_capability;
    u32 destination_path_length;
    u32 byte_count;
    u32 written;

    if ((source_capability == FS64_INVALID_HANDLE) || (borrowed_root != 0u))
    {
        return shell64_write_text(console_capability_handle, owner_id, "copy failed\n");
    }

    shell64_zero(g_shell64_io, sizeof(g_shell64_io));
    byte_count = fs64_read_kernel(source_capability, g_shell64_io, 0u, 256u, owner_id);
    (void)fs64_revoke(source_capability, owner_id);
    if ((byte_count == 0u) || (byte_count == FS64_INVALID_HANDLE))
    {
        return shell64_write_text(console_capability_handle, owner_id, "copy failed\n");
    }

    destination_path_length = shell64_normalize_path(
        destination_start,
        destination_length,
        g_shell64_path_b);
    if ((destination_path_length == 0u) || shell64_token_is_root(destination_start, destination_length))
    {
        return shell64_write_text(console_capability_handle, owner_id, "copy failed\n");
    }

    (void)fs64_delete_kernel(
        root_capability_handle,
        g_shell64_path_b,
        destination_path_length,
        owner_id);
    destination_capability = fs64_create_kernel(
        root_capability_handle,
        g_shell64_path_b,
        destination_path_length,
        RAMFS_NODE_FILE,
        owner_id);
    if (destination_capability == FS64_INVALID_HANDLE)
    {
        return shell64_write_text(console_capability_handle, owner_id, "copy failed\n");
    }

    written = fs64_write_kernel(destination_capability, g_shell64_io, 0u, byte_count, owner_id);
    (void)fs64_revoke(destination_capability, owner_id);
    if (written != byte_count)
    {
        return shell64_write_text(console_capability_handle, owner_id, "copy failed\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "ok\n");
}

static u32 shell64_rename_or_move(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 source_start,
    u32 source_length,
    u32 destination_start,
    u32 destination_length,
    u32 owner_id,
    int move)
{
    u32 source_path_length = shell64_normalize_path(source_start, source_length, g_shell64_path_a);
    u32 destination_path_length = shell64_normalize_path(
        destination_start,
        destination_length,
        g_shell64_path_b);
    u32 index;
    u32 result;

    if ((source_path_length == 0u)
        || (destination_path_length == 0u)
        || shell64_token_is_root(source_start, source_length)
        || shell64_token_is_root(destination_start, destination_length))
    {
        return shell64_write_text(console_capability_handle, owner_id, "rename failed\n");
    }

    shell64_zero(g_shell64_pair, sizeof(g_shell64_pair));
    for (index = 0u; index < source_path_length; ++index)
    {
        g_shell64_pair[index] = g_shell64_path_a[index];
    }
    for (index = 0u; index < destination_path_length; ++index)
    {
        g_shell64_pair[source_path_length + 1u + index] = g_shell64_path_b[index];
    }

    if (move != 0)
    {
        result = fs64_move_kernel(
            root_capability_handle,
            root_capability_handle,
            g_shell64_pair,
            source_path_length,
            destination_path_length,
            owner_id);
    }
    else
    {
        result = fs64_rename_kernel(
            root_capability_handle,
            g_shell64_pair,
            source_path_length,
            destination_path_length,
            owner_id);
    }

    if (result != 1u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "rename failed\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "ok\n");
}

u32 shell64_execute_line(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u64 line_address,
    u32 line_byte_count,
    u32 owner_id)
{
    u32 cursor = 0u;
    u32 command_start = 0u;
    u32 command_length;
    u32 first_start = 0u;
    u32 first_length;
    u32 second_start = 0u;
    u32 second_length;

    if (line_byte_count > SHELL64_MAX_LINE_BYTES)
    {
        return SHELL64_INVALID_RESULT;
    }

    if (!shell64_address_readable(line_address, line_byte_count))
    {
        return SHELL64_INVALID_RESULT;
    }

    shell64_zero(g_shell64_line, sizeof(g_shell64_line));
    if (line_byte_count > 0u)
    {
        shell64_copy(g_shell64_line, (const u8 *)line_address, line_byte_count);
    }

    command_length = shell64_next_token(&cursor, line_byte_count, &command_start);
    if (command_length == 0u)
    {
        return 0u;
    }

    if (shell64_token_equals(command_start, command_length, "help"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length != 0u)
        {
            return shell64_print_usage(console_capability_handle, owner_id, first_start, first_length);
        }
        return shell64_write_text(
            console_capability_handle,
            owner_id,
            "Builtins: apps help info pwd\n"
            "Product apps: append cat copy delete ls mkdir move rename stat touch write\n"
            "Unavailable in M1: ask (not AI), echo, aliases, gui, network, installer, package-manager, ai\n");
    }

    if (shell64_token_equals(command_start, command_length, "pwd"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "/\n");
    }

    if (shell64_token_equals(command_start, command_length, "apps"))
    {
        return shell64_list_apps(console_capability_handle, root_capability_handle, owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "info"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: info <command>\n");
        }
        return shell64_info(console_capability_handle, owner_id, first_start, first_length);
    }

    if (shell64_token_equals(command_start, command_length, "ls"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if ((first_length != 0u) && shell64_token_is_apps_path(first_start, first_length))
        {
            return shell64_list_apps(console_capability_handle, root_capability_handle, owner_id);
        }
        return shell64_list_path(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "cat"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: cat <path>\n");
        }
        return shell64_read_file(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "stat"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: stat <path>\n");
        }
        return shell64_stat_path(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "mkdir"))
    {
        u32 directory_capability;

        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "mkdir failed\n");
        }

        directory_capability = shell64_create_path(
            root_capability_handle,
            first_start,
            first_length,
            RAMFS_NODE_DIRECTORY,
            owner_id);
        if (directory_capability == FS64_INVALID_HANDLE)
        {
            return shell64_write_text(console_capability_handle, owner_id, "mkdir failed\n");
        }

        (void)fs64_revoke(directory_capability, owner_id);
        return shell64_write_text(console_capability_handle, owner_id, "ok\n");
    }

    if (shell64_token_equals(command_start, command_length, "write"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        cursor = shell64_skip_spaces(cursor, line_byte_count);
        if ((first_length == 0u) || (cursor >= line_byte_count))
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: write <path> <text>\n");
        }
        return shell64_write_file(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            cursor,
            line_byte_count - cursor,
            owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "touch"))
    {
        u32 borrowed_root;
        u32 existing_capability;

        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: touch <path>\n");
        }

        existing_capability = shell64_open_path(
            root_capability_handle,
            first_start,
            first_length,
            owner_id,
            &borrowed_root);
        if ((existing_capability != FS64_INVALID_HANDLE) && (borrowed_root == 0u))
        {
            (void)fs64_revoke(existing_capability, owner_id);
            return shell64_write_text(console_capability_handle, owner_id, "ok\n");
        }

        existing_capability = shell64_create_path(
            root_capability_handle,
            first_start,
            first_length,
            RAMFS_NODE_FILE,
            owner_id);
        if (existing_capability == FS64_INVALID_HANDLE)
        {
            return shell64_write_text(console_capability_handle, owner_id, "touch failed\n");
        }

        (void)fs64_revoke(existing_capability, owner_id);
        return shell64_write_text(console_capability_handle, owner_id, "ok\n");
    }

    if (shell64_token_equals(command_start, command_length, "append"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        cursor = shell64_skip_spaces(cursor, line_byte_count);
        if ((first_length == 0u) || (cursor >= line_byte_count))
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: append <path> <text>\n");
        }
        return shell64_append_file(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            cursor,
            line_byte_count - cursor,
            owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "copy"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        second_length = shell64_next_token(&cursor, line_byte_count, &second_start);
        if ((first_length == 0u) || (second_length == 0u))
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: copy <source> <dest>\n");
        }
        return shell64_copy_file(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            second_start,
            second_length,
            owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "delete"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if ((first_length == 0u)
            || (shell64_delete_path(
                    root_capability_handle,
                    first_start,
                    first_length,
                    owner_id) != 1u))
        {
            return shell64_write_text(console_capability_handle, owner_id, "delete failed\n");
        }
        return shell64_write_text(console_capability_handle, owner_id, "ok\n");
    }

    if (shell64_token_equals(command_start, command_length, "rename"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        second_length = shell64_next_token(&cursor, line_byte_count, &second_start);
        if ((first_length == 0u) || (second_length == 0u))
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: rename <from> <to>\n");
        }
        return shell64_rename_or_move(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            second_start,
            second_length,
            owner_id,
            0);
    }

    if (shell64_token_equals(command_start, command_length, "move"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        second_length = shell64_next_token(&cursor, line_byte_count, &second_start);
        if ((first_length == 0u) || (second_length == 0u))
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: move <source> <dest>\n");
        }
        return shell64_rename_or_move(
            console_capability_handle,
            root_capability_handle,
            first_start,
            first_length,
            second_start,
            second_length,
            owner_id,
            1);
    }

    return shell64_write_text(console_capability_handle, owner_id, "unknown command\n");
}
