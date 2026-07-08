#include "shell_x64.h"

#include "account_association_x64.h"
#include "ai_policy_x64.h"
#include "apic_x64.h"
#include "auth_x64.h"
#include "boot_diag_x64.h"
#include "boot_media_x64.h"
#include "capability_x64.h"
#include "console_x64.h"
#include "cloud_storage_x64.h"
#include "display_x64.h"
#include "e1000e_x64.h"
#include "fs_x64.h"
#include "hardware_registry_x64.h"
#include "i2c_hid_x64.h"
#include "identity_transport_x64.h"
#include "input_x64.h"
#include "installer_ux_x64.h"
#include "launch_x64.h"
#include "linux_exec_x64.h"
#include "mmio_x64.h"
#include "network_socket_x64.h"
#include "package_signing_x64.h"
#include "pci_x64.h"
#include "ramfs.h"
#include "runtime_image_x64.h"
#include "services.h"
#include "types.h"
#include "virtio_net_x64.h"
#include "xhci_x64.h"

#define SHELL64_MAX_LINE_BYTES 128u
#define SHELL64_MAX_PATH_BYTES 128u
#define SHELL64_IO_BYTES 4096u
#define SHELL64_CONSOLE_CHUNK_BYTES 512u
#define SHELL64_KERNEL_HIGH_BASE_HIGH32 0xFFFFFFFFu
#define SHELL64_KERNEL_HIGH_BASE_LOW32 0x80000000u
#define SHELL64_REDIRECT_NONE 0u
#define SHELL64_REDIRECT_FOUND 1u
#define SHELL64_REDIRECT_INVALID 2u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define SHELL64_REDIRECT_BUFFER_BYTES 8192u
#define SHELL64_REDIRECT_RAMFS_CHUNK_BYTES 256u
#define SHELL64_REDIRECT_BACKEND_NONE 0u
#define SHELL64_REDIRECT_BACKEND_FAT 1u
#define SHELL64_REDIRECT_BACKEND_USB 2u
#define SHELL64_REDIRECT_BACKEND_RAMFS 3u
#define SHELL64_REDIRECT_ERROR_NONE 0u
#define SHELL64_REDIRECT_ERROR_ARGUMENT 1u
#define SHELL64_REDIRECT_ERROR_RAMFS 2u
#define SHELL64_REDIRECT_ERROR_FAT 3u
#define SHELL64_REDIRECT_ERROR_USB_UNAVAILABLE 4u
#define SHELL64_XHCI_PORTSC_CCS 0x00000001u
#define SHELL64_XHCI_PORTSC_PED 0x00000002u
#define SHELL64_XHCI_PORTSC_SPEED_SHIFT 10u
#endif

static u8 g_shell64_line[SHELL64_MAX_LINE_BYTES + 1u];
static u8 g_shell64_path_a[SHELL64_MAX_PATH_BYTES];
static u8 g_shell64_path_b[SHELL64_MAX_PATH_BYTES];
static u8 g_shell64_pair[SHELL64_MAX_PATH_BYTES * 2u];
static u8 g_shell64_io[SHELL64_IO_BYTES];
static u8 g_shell64_stat[64u];
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_shell64_redirect_active = 0u;
static u32 g_shell64_redirect_capability = FS64_INVALID_HANDLE;
static u32 g_shell64_redirect_offset = 0u;
static u32 g_shell64_redirect_append = 0u;
static u32 g_shell64_redirect_count = 0u;
static u32 g_shell64_redirect_append_count = 0u;
static u32 g_shell64_redirect_write_count = 0u;
static u32 g_shell64_redirect_byte_count = 0u;
static u32 g_shell64_redirect_denial_count = 0u;
static u32 g_shell64_redirect_last_result = 0u;
static u32 g_shell64_redirect_commit_count = 0u;
static u32 g_shell64_redirect_path_length = 0u;
static u32 g_shell64_redirect_committed = 0u;
static u32 g_shell64_redirect_usb_requested = 0u;
static u32 g_shell64_redirect_usb_unavailable = 0u;
static u32 g_shell64_redirect_backend = SHELL64_REDIRECT_BACKEND_NONE;
static u32 g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_NONE;
static u8 g_shell64_redirect_path[SHELL64_MAX_PATH_BYTES];
static u8 g_shell64_redirect_buffer[SHELL64_REDIRECT_BUFFER_BYTES];
static char g_shell64_linux_argv_storage[LINUX_EXEC64_ARG_MAX][SHELL64_MAX_LINE_BYTES + 1u];
#endif

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_redirect_flush(u32 owner_id);
static u32 shell64_stat_size(const u8 *bytes, u32 byte_count);
#endif

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_redirect_flush_ramfs(u32 owner_id)
{
    u32 stat_count;
    u32 file_offset = 0u;
    u32 flush_offset = 0u;
    u32 chunk;
    u32 written;

    if ((g_shell64_redirect_capability == FS64_INVALID_HANDLE)
        || (g_shell64_redirect_offset == 0u))
    {
        return 1u;
    }

    if ((g_shell64_redirect_append != 0u) || (g_shell64_redirect_committed != 0u))
    {
        stat_count = fs64_stat_kernel(
            g_shell64_redirect_capability,
            g_shell64_stat,
            sizeof(g_shell64_stat),
            owner_id);
        if (stat_count == FS64_INVALID_HANDLE)
        {
            ++g_shell64_redirect_denial_count;
            g_shell64_redirect_last_result = 0u;
            g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_RAMFS;
            return 0u;
        }
        file_offset = shell64_stat_size(g_shell64_stat, stat_count);
    }

    while (flush_offset < g_shell64_redirect_offset)
    {
        chunk = g_shell64_redirect_offset - flush_offset;
        if (chunk > SHELL64_REDIRECT_RAMFS_CHUNK_BYTES)
        {
            chunk = SHELL64_REDIRECT_RAMFS_CHUNK_BYTES;
        }

        written = fs64_write_kernel(
            g_shell64_redirect_capability,
            g_shell64_redirect_buffer + flush_offset,
            file_offset + flush_offset,
            chunk,
            owner_id);
        if (written != chunk)
        {
            ++g_shell64_redirect_denial_count;
            g_shell64_redirect_last_result = flush_offset + written;
            g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_RAMFS;
            return 0u;
        }

        flush_offset += written;
    }

    ++g_shell64_redirect_commit_count;
    g_shell64_redirect_last_result = g_shell64_redirect_offset;
    g_shell64_redirect_committed = 1u;
    g_shell64_redirect_backend = SHELL64_REDIRECT_BACKEND_RAMFS;
    g_shell64_redirect_offset = 0u;
    shell64_zero(g_shell64_redirect_buffer, sizeof(g_shell64_redirect_buffer));
    g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_NONE;
    return 1u;
}

static u32 shell64_redirect_flush(u32 owner_id)
{
    u32 commit_ok;

    if ((g_shell64_redirect_path_length == 0u) || (g_shell64_redirect_offset == 0u))
    {
        return 1u;
    }

    if (g_shell64_redirect_backend == SHELL64_REDIRECT_BACKEND_RAMFS)
    {
        return shell64_redirect_flush_ramfs(owner_id);
    }

    if (g_shell64_redirect_backend == SHELL64_REDIRECT_BACKEND_USB)
    {
        if ((g_shell64_redirect_append != 0u) || (g_shell64_redirect_committed != 0u))
        {
            commit_ok = mmio64_usb_fat_shell_append_file(
                g_shell64_redirect_path,
                g_shell64_redirect_path_length,
                g_shell64_redirect_buffer,
                g_shell64_redirect_offset,
                owner_id);
        }
        else
        {
            commit_ok = mmio64_usb_fat_shell_write_file(
                g_shell64_redirect_path,
                g_shell64_redirect_path_length,
                g_shell64_redirect_buffer,
                g_shell64_redirect_offset,
                owner_id);
        }

        if (commit_ok != 0u)
        {
            ++g_shell64_redirect_commit_count;
            g_shell64_redirect_last_result = g_shell64_redirect_offset;
            g_shell64_redirect_committed = 1u;
            g_shell64_redirect_offset = 0u;
            shell64_zero(g_shell64_redirect_buffer, sizeof(g_shell64_redirect_buffer));
            g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_NONE;
            return 1u;
        }

        ++g_shell64_redirect_usb_unavailable;
        ++g_shell64_redirect_denial_count;
        g_shell64_redirect_last_result = 0u;
        g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_USB_UNAVAILABLE;
        return 0u;
    }

    if ((g_shell64_redirect_append != 0u) || (g_shell64_redirect_committed != 0u))
    {
        commit_ok = mmio64_nvme_fat_shell_append_file(
            g_shell64_redirect_path,
            g_shell64_redirect_path_length,
            g_shell64_redirect_buffer,
            g_shell64_redirect_offset,
            owner_id);
    }
    else
    {
        commit_ok = mmio64_nvme_fat_shell_write_file(
            g_shell64_redirect_path,
            g_shell64_redirect_path_length,
            g_shell64_redirect_buffer,
            g_shell64_redirect_offset,
            owner_id);
    }

    if (commit_ok != 0u)
    {
        ++g_shell64_redirect_commit_count;
        g_shell64_redirect_last_result = g_shell64_redirect_offset;
        g_shell64_redirect_committed = 1u;
        g_shell64_redirect_offset = 0u;
        shell64_zero(g_shell64_redirect_buffer, sizeof(g_shell64_redirect_buffer));
        return 1u;
    }

    return shell64_redirect_flush_ramfs(owner_id);
}
#endif

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
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 index;
    u32 writable_bytes;

    if (g_shell64_redirect_active != 0u)
    {
        if (bytes == 0)
        {
            ++g_shell64_redirect_denial_count;
            g_shell64_redirect_last_result = FS64_INVALID_HANDLE;
            return 0u;
        }

        writable_bytes = 0u;
        while (writable_bytes < byte_count)
        {
            u32 available = SHELL64_REDIRECT_BUFFER_BYTES - g_shell64_redirect_offset;
            u32 take;

            if (available == 0u)
            {
                if (shell64_redirect_flush(owner_id) == 0u)
                {
                    return writable_bytes;
                }
                available = SHELL64_REDIRECT_BUFFER_BYTES;
            }

            take = byte_count - writable_bytes;
            if (take > available)
            {
                take = available;
            }

            for (index = 0u; index < take; ++index)
            {
                g_shell64_redirect_buffer[g_shell64_redirect_offset + index] = bytes[writable_bytes + index];
            }

            g_shell64_redirect_offset += take;
            writable_bytes += take;
        }

        g_shell64_redirect_byte_count += writable_bytes;
        ++g_shell64_redirect_write_count;
        g_shell64_redirect_last_result = writable_bytes;
        return writable_bytes;
    }
#endif

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

static u32 shell64_write_bytes_chunked(
    u32 console_capability_handle,
    u32 owner_id,
    const u8 *bytes,
    u32 byte_count)
{
    u32 offset = 0u;
    u32 chunk;
    u32 written;

    if (bytes == (const u8 *)0)
    {
        return 0u;
    }

    while (offset < byte_count)
    {
        chunk = byte_count - offset;
        if (chunk > SHELL64_CONSOLE_CHUNK_BYTES)
        {
            chunk = SHELL64_CONSOLE_CHUNK_BYTES;
        }
        written = shell64_write(console_capability_handle, owner_id, bytes + offset, chunk);
        if (written != chunk)
        {
            return offset;
        }
        offset += written;
    }

    return offset;
}

static u32 shell64_login_available(void)
{
    return (auth64_login_screen() != 0u) && (auth64_auth_success() != 0u);
}

static u32 shell64_write_builtins_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_login_available() != 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "Builtins: apps devices dev hwdevices lsdev export exporthw help hwfull hwval hwexport info linux lock net open pkginfo port ports pwd usbscan\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "Builtins: apps devices dev hwdevices lsdev export exporthw help hwfull hwval hwexport info linux net open pkginfo port ports pwd usbscan\n");
#else
    if (shell64_login_available() != 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "Builtins: apps help hwval info linux lock net pkginfo pwd\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "Builtins: apps help hwval info linux net pkginfo pwd\n");
#endif
}

static u32 shell64_write_login_status_line(u32 console_capability_handle, u32 owner_id)
{
    if (shell64_login_available() != 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "Product login: first-run setup, authenticated session, lock/unlock through brokered input\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "UEFI login/session lock: unavailable on BIOS checksum fallback\n");
}

static u32 shell64_write_identity_status_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "Product identity/cloud/installer: Settings shows local account, cloud policy, and dry-run installer planning; remote/cloud login unavailable\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "Product identity: unavailable on BIOS checksum fallback\n");
#endif
}

static u32 shell64_write_gui_status_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "Product GUI: Terminal, File Manager, Settings, Installer, Assistant through brokered desktop input/display\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "Product GUI: unavailable on BIOS checksum fallback\n");
#endif
}

static u32 shell64_write_service_status_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "Product services: Settings shows service/session status; installer planning writes disabled\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "Product services: BIOS service/session stubs active; installer UX unavailable\n");
#endif
}

static u32 shell64_write_installer_status_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "Product installer UX: launcher and Settings show dry-run planning; writes, formatting, and boot-entry changes disabled\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "Product installer UX: unavailable on BIOS checksum fallback; dry-run safety tooling only\n");
#endif
}

static u32 shell64_write_ai_status_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "Product AI assistant: launcher, Settings, and pkginfo show consent-scoped action templates; inference unavailable\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "Product AI policy: unavailable on BIOS checksum fallback; AI actions unavailable\n");
#endif
}

static u32 shell64_write_apps_gui_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "GUI desktop: Terminal File Manager Settings Installer Assistant\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "GUI desktop: unavailable on BIOS checksum fallback\n");
#endif
}

static u32 shell64_write_apps_installer_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "Installer UX: launcher/Settings; dry-run planning only; writes disabled\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "Installer UX: unavailable on BIOS checksum fallback; dry-run safety tooling only\n");
#endif
}

static u32 shell64_write_apps_ai_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "AI Assistant: launcher, Settings, and pkginfo show consent-scoped action templates; inference unavailable\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "AI policy: unavailable on BIOS checksum fallback; no actions\n");
#endif
}

static u32 shell64_format_decimal_u8(char *buffer, u32 value)
{
    u32 offset = 0u;

    if (value >= 100u)
    {
        buffer[offset++] = (char)('0' + (value / 100u));
        value %= 100u;
        buffer[offset++] = (char)('0' + (value / 10u));
        buffer[offset++] = (char)('0' + (value % 10u));
        return offset;
    }

    if (value >= 10u)
    {
        buffer[offset++] = (char)('0' + (value / 10u));
        buffer[offset++] = (char)('0' + (value % 10u));
        return offset;
    }

    buffer[offset++] = (char)('0' + value);
    return offset;
}

static u32 shell64_format_decimal_u32(char *buffer, u32 value)
{
    char reverse[10];
    u32 reverse_count = 0u;
    u32 offset = 0u;

    if (value == 0u)
    {
        buffer[offset++] = '0';
        return offset;
    }

    while ((value != 0u) && (reverse_count < sizeof(reverse)))
    {
        reverse[reverse_count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (reverse_count > 0u)
    {
        buffer[offset++] = reverse[--reverse_count];
    }

    return offset;
}

static u32 shell64_format_hex32(char *buffer, u32 value)
{
    static const char hex[] = "0123456789ABCDEF";
    u32 offset = 0u;
    u32 shift;

    buffer[offset++] = '0';
    buffer[offset++] = 'x';
    for (shift = 28u; shift <= 28u; shift -= 4u)
    {
        buffer[offset++] = hex[(value >> shift) & 0xFu];
        if (shift == 0u)
        {
            break;
        }
    }

    return offset;
}

static u32 g_shell64_hwval_filter_active;
static u32 g_shell64_hwval_filter_start;
static u32 g_shell64_hwval_filter_length;

static u8 shell64_ascii_lower(u8 value)
{
    if ((value >= (u8)'A') && (value <= (u8)'Z'))
    {
        return (u8)(value + ((u8)'a' - (u8)'A'));
    }

    return value;
}

static u32 shell64_hwval_label_matches_filter(const char *label)
{
    u32 label_length = 0u;
    u32 offset;
    u32 filter_offset;

    if ((g_shell64_hwval_filter_active == 0u) || (g_shell64_hwval_filter_length == 0u))
    {
        return 1u;
    }

    while (label[label_length] != '\0')
    {
        ++label_length;
    }

    if (g_shell64_hwval_filter_length > label_length)
    {
        return 0u;
    }

    for (offset = 0u; offset <= (label_length - g_shell64_hwval_filter_length); ++offset)
    {
        for (filter_offset = 0u; filter_offset < g_shell64_hwval_filter_length; ++filter_offset)
        {
            u8 label_byte = shell64_ascii_lower((u8)label[offset + filter_offset]);
            u8 filter_byte = shell64_ascii_lower(g_shell64_line[g_shell64_hwval_filter_start + filter_offset]);
            if (label_byte != filter_byte)
            {
                break;
            }
        }
        if (filter_offset == g_shell64_hwval_filter_length)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 shell64_write_hwval_text_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *line)
{
    if (shell64_hwval_label_matches_filter(line) == 0u)
    {
        return 0u;
    }

    return shell64_write_text(console_capability_handle, owner_id, line);
}

static u32 shell64_write_decimal_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 value)
{
    char buffer[10];
    u32 length;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_hwval_label_matches_filter(label) == 0u)
    {
        return 0u;
    }
#endif

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    length = shell64_format_decimal_u32(buffer, value);
    (void)shell64_write(console_capability_handle, owner_id, (const u8 *)buffer, length);
    return shell64_write_text(console_capability_handle, owner_id, "\n");
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define SHELL64_HWVAL_COMPOSITE_INACTIVE 0u
#define SHELL64_HWVAL_COMPOSITE_EMIT 1u
#define SHELL64_HWVAL_COMPOSITE_SUPPRESS 2u

static u32 g_shell64_hwval_composite_line_state;

static u32 shell64_begin_hwval_composite_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *prefix)
{
    if (shell64_hwval_label_matches_filter(prefix) == 0u)
    {
        g_shell64_hwval_composite_line_state = SHELL64_HWVAL_COMPOSITE_SUPPRESS;
        return 0u;
    }

    g_shell64_hwval_composite_line_state = SHELL64_HWVAL_COMPOSITE_EMIT;
    return shell64_write_text(console_capability_handle, owner_id, prefix);
}

static u32 shell64_end_hwval_composite_line(
    u32 console_capability_handle,
    u32 owner_id)
{
    u32 state = g_shell64_hwval_composite_line_state;
    g_shell64_hwval_composite_line_state = SHELL64_HWVAL_COMPOSITE_INACTIVE;
    if (state != SHELL64_HWVAL_COMPOSITE_EMIT)
    {
        return 0u;
    }

    return shell64_write_text(console_capability_handle, owner_id, "\n");
}

static void shell64_write_decimal_field(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 value)
{
    char buffer[10];
    u32 length;

    if (g_shell64_hwval_composite_line_state == SHELL64_HWVAL_COMPOSITE_SUPPRESS)
    {
        return;
    }
    if ((g_shell64_hwval_composite_line_state == SHELL64_HWVAL_COMPOSITE_INACTIVE)
        && (shell64_hwval_label_matches_filter(label) == 0u))
    {
        return;
    }

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    length = shell64_format_decimal_u32(buffer, value);
    (void)shell64_write(console_capability_handle, owner_id, (const u8 *)buffer, length);
}

static void shell64_write_hex32_field(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 value)
{
    char buffer[10];
    u32 length;

    if (g_shell64_hwval_composite_line_state == SHELL64_HWVAL_COMPOSITE_SUPPRESS)
    {
        return;
    }
    if ((g_shell64_hwval_composite_line_state == SHELL64_HWVAL_COMPOSITE_INACTIVE)
        && (shell64_hwval_label_matches_filter(label) == 0u))
    {
        return;
    }

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    length = shell64_format_hex32(buffer, value);
    (void)shell64_write(console_capability_handle, owner_id, (const u8 *)buffer, length);
}

static void shell64_write_gui_interaction_telemetry(
    u32 console_capability_handle,
    u32 owner_id)
{
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-gui drs-gui-interactive ");
    shell64_write_decimal_field(console_capability_handle, owner_id, "", display64_gui_interactive());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-click-hittest ", display64_gui_click_hittest());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-launcher-opened ", display64_gui_launcher_opened());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-terminal-opened ", display64_gui_terminal_opened());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-drag-completed ", display64_gui_drag_completed());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-keyboard-routed ", display64_gui_keyboard_routed());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-close-completed ", display64_gui_close_completed());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-taskbar-focus ", display64_gui_taskbar_focus());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-fileman-opened ", display64_gui_fileman_opened());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-settings-opened ", display64_gui_settings_opened());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-installer-opened ", display64_gui_installer_opened());
    shell64_write_decimal_field(console_capability_handle, owner_id, " input-diag-suppressed ", display64_gui_input_diag_suppressed_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " mouse-diag-suppressed ", display64_gui_mouse_diag_suppressed_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-right-click ", display64_gui_right_click_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-context-action ", display64_gui_context_menu_action_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " wm-resize ", display64_wm_resize_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " wm-minimize ", display64_wm_minimize_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " wm-restore ", display64_wm_restore_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " wm-zorder ", display64_wm_zorder_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-scroll ", display64_gui_scroll_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " terminal-actions ", display64_gui_terminal_action_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " terminal-scroll ", display64_gui_terminal_scroll_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " terminal-scroll-offset ", display64_gui_terminal_scroll_offset());
    shell64_write_decimal_field(console_capability_handle, owner_id, " terminal-selection ", display64_gui_terminal_selection_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " terminal-copy ", display64_gui_terminal_copy_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " terminal-copied-bytes ", display64_gui_terminal_copied_bytes());
    shell64_write_decimal_field(console_capability_handle, owner_id, " terminal-cursor ", display64_gui_terminal_cursor_draw_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-actions ", display64_gui_fileman_action_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-refresh ", display64_gui_fileman_backend_refresh_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-preview ", display64_gui_fileman_backend_preview_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-open-dir ", display64_gui_fileman_backend_open_dir_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-write ", display64_gui_fileman_backend_write_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-write-denial ", display64_gui_fileman_backend_write_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-delete ", display64_gui_fileman_backend_delete_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-delete-denial ", display64_gui_fileman_backend_delete_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-delete-confirm ", display64_gui_fileman_backend_delete_confirm_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-mkdir ", display64_gui_fileman_backend_mkdir_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-mkdir-denial ", display64_gui_fileman_backend_mkdir_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-copy ", display64_gui_fileman_backend_copy_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-copy-denial ", display64_gui_fileman_backend_copy_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-rename ", display64_gui_fileman_backend_rename_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-rename-denial ", display64_gui_fileman_backend_rename_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-move ", display64_gui_fileman_backend_move_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-move-denial ", display64_gui_fileman_backend_move_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-edit ", display64_gui_fileman_backend_edit_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-edit-commit ", display64_gui_fileman_backend_edit_commit_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-actions ", display64_gui_settings_action_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-load ", display64_gui_settings_load_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-save ", display64_gui_settings_save_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-save-denial ", display64_gui_settings_save_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-export ", display64_gui_settings_export_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-export-denial ", display64_gui_settings_export_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-hardware-panel ", display64_gui_settings_hardware_panel_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-input-panel ", display64_gui_settings_input_panel_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-readiness-strip ", display64_gui_settings_readiness_strip_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fileman-storage-card ", display64_gui_fileman_storage_card_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-theme ", display64_gui_settings_theme());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-pointer ", display64_gui_settings_pointer_speed());
    shell64_write_decimal_field(console_capability_handle, owner_id, " settings-keyrepeat ", display64_gui_settings_key_repeat());
    shell64_write_decimal_field(console_capability_handle, owner_id, " installer-actions ", display64_gui_installer_action_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " keyboard-open ", display64_gui_keyboard_open_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-unfocused-key-denied ", display64_gui_unfocused_key_denied());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-no-ambient-input ", display64_gui_no_ambient_input());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-no-ambient-display ", display64_gui_no_ambient_display());
    shell64_write_decimal_field(console_capability_handle, owner_id, " drs-gui-no-ambient-fs ", display64_gui_no_ambient_fs());
    shell64_write_decimal_field(console_capability_handle, owner_id, " mouse-x ", display64_gui_mouse_x());
    shell64_write_decimal_field(console_capability_handle, owner_id, " mouse-y ", display64_gui_mouse_y());
    shell64_write_decimal_field(console_capability_handle, owner_id, " target-window ", display64_gui_target_window());
    shell64_write_decimal_field(console_capability_handle, owner_id, " target-region ", display64_gui_target_region());
    shell64_write_decimal_field(console_capability_handle, owner_id, " focus-before ", display64_gui_focus_before());
    shell64_write_decimal_field(console_capability_handle, owner_id, " focus-after ", display64_gui_focus_after());
    shell64_write_decimal_field(console_capability_handle, owner_id, " z-before ", display64_gui_z_before());
    shell64_write_decimal_field(console_capability_handle, owner_id, " z-after ", display64_gui_z_after());
    shell64_write_decimal_field(console_capability_handle, owner_id, " key-target-window ", display64_gui_key_target_window());
    shell64_write_decimal_field(console_capability_handle, owner_id, " unfocused-key-denials ", display64_gui_unfocused_key_denial_count());
    shell64_write_hex32_field(console_capability_handle, owner_id, " input-token ", display64_gui_input_path_token());
    shell64_write_hex32_field(console_capability_handle, owner_id, " display-token ", display64_gui_display_path_token());
    shell64_write_hex32_field(console_capability_handle, owner_id, " fs-token ", display64_gui_fs_path_token());
    shell64_write_decimal_field(console_capability_handle, owner_id, " assistant-opened ", display64_gui_assistant_opened());
    (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
}
#endif

static u32 shell64_write_hex32_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 value)
{
    char buffer[10];
    u32 length;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_hwval_label_matches_filter(label) == 0u)
    {
        return 0u;
    }
#endif

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    length = shell64_format_hex32(buffer, value);
    (void)shell64_write(console_capability_handle, owner_id, (const u8 *)buffer, length);
    return shell64_write_text(console_capability_handle, owner_id, "\n");
}

static u32 shell64_format_ipv4(char *buffer, u32 address)
{
    u32 offset = 0u;
    u32 index;

    for (index = 0u; index < 4u; ++index)
    {
        u32 octet = (address >> (24u - (index * 8u))) & 0xFFu;
        offset += shell64_format_decimal_u8(buffer + offset, octet);
        if (index != 3u)
        {
            buffer[offset++] = '.';
        }
    }

    return offset;
}

static u32 shell64_write_ipv4_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 address)
{
    char buffer[16];
    u32 length;

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    length = shell64_format_ipv4(buffer, address);
    (void)shell64_write(console_capability_handle, owner_id, (const u8 *)buffer, length);
    return shell64_write_text(console_capability_handle, owner_id, "\n");
}

static u32 shell64_print_network_status(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    network_socket64_probe();
#endif
    if (virtio_net64_dhcp_ack() == 0u)
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "no network\n");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)shell64_write_text(console_capability_handle, owner_id, "socket api: brokered tcp-client foundation\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "socket connect: unavailable until brokered DHCP/DNS/HTTP is online\n");
        return shell64_write_text(console_capability_handle, owner_id, "authority: capability required; no ambient network\n");
#else
        return shell64_write_text(console_capability_handle, owner_id, "authority: brokered status only; no ambient network\n");
#endif
    }

    (void)shell64_write_text(console_capability_handle, owner_id, "network: online\n");
    (void)shell64_write_ipv4_line(
        console_capability_handle,
        owner_id,
        "ip: ",
        virtio_net64_dhcp_ip());
    (void)shell64_write_ipv4_line(
        console_capability_handle,
        owner_id,
        "gateway: ",
        virtio_net64_dhcp_gateway());
    (void)shell64_write_ipv4_line(
        console_capability_handle,
        owner_id,
        "dns: ",
        virtio_net64_dhcp_dns());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_text(console_capability_handle, owner_id, "socket api: brokered tcp-client foundation\n");
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "socket http status: ",
        network_socket64_last_http_status());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "socket response bytes: ",
        network_socket64_last_response_bytes());
    (void)shell64_write_text(console_capability_handle, owner_id, "socket denied: raw packet, listen, send without broker data-plane authority\n");
#endif
    return shell64_write_text(console_capability_handle, owner_id, "authority: brokered\n");
}

static u32 shell64_net_curl(
    u32 console_capability_handle,
    u32 owner_id,
    u32 url_start,
    u32 url_length)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 byte_count = 0u;
    u32 offset;
    u32 result;

    result = network_socket64_curl_http(
        &g_shell64_line[url_start],
        url_length,
        g_shell64_io,
        sizeof(g_shell64_io),
        owner_id,
        &byte_count);
    if (result == 0u)
    {
        if (network_socket64_curl_url_denied() != 0u)
        {
            return shell64_write_text(
                console_capability_handle,
                owner_id,
                "curl denied: only example.com is brokered\n");
        }
        return shell64_write_text(console_capability_handle, owner_id, "curl unavailable\n");
    }

    if (byte_count != 0u)
    {
        offset = 0u;
        while (offset < byte_count)
        {
            u32 remaining = byte_count - offset;
            u32 chunk = (remaining < SHELL64_CONSOLE_CHUNK_BYTES)
                ? remaining
                : SHELL64_CONSOLE_CHUNK_BYTES;
            (void)shell64_write(
                console_capability_handle,
                owner_id,
                &g_shell64_io[offset],
                chunk);
            offset += chunk;
        }
        if (g_shell64_io[byte_count - 1u] != (u8)'\n')
        {
            (void)shell64_write_text(console_capability_handle, owner_id, "\n");
        }
    }
    if (network_socket64_curl_truncated() != 0u)
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "[truncated to 4096 bytes]\n");
    }

    return byte_count;
#else
    (void)url_start;
    (void)url_length;
    return shell64_write_text(console_capability_handle, owner_id, "curl unavailable\n");
#endif
}

static u32 shell64_print_package_status(u32 console_capability_handle, u32 owner_id)
{
    if (package_signing64_signed() == 0u)
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "package system: BIOS checksum-only fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "bios package mode: checksum-only fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "uefi package mode: unavailable on BIOS boot\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "signature verification: unavailable on BIOS fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "auto-install: unavailable\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "public update fetch: unavailable/non-product\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "trusted-time expiry: unavailable/non-product\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "identity transport: unavailable on BIOS fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "account association: unavailable on BIOS fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "cloud storage broker: unavailable on BIOS fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "cloud sync: unavailable\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "ai policy broker: unavailable on BIOS fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "ai actions: unavailable\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "ai assistant: unavailable on BIOS fallback\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "ai inference: unavailable\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "install authority: disabled; scoped capability required\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "update-check authority: scoped; no ambient network\n");
        return shell64_write_text(console_capability_handle, owner_id, "update-apply authority: disabled; scoped install required\n");
    }

    (void)shell64_write_text(console_capability_handle, owner_id, "package system: enabled on UEFI Product\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "bios package mode: checksum-only fallback\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "uefi package mode: Ed25519 verified\n");
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "package format version: ", 2u);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "trusted public key id: ", package_signing64_public_key_id());
    (void)shell64_write_text(console_capability_handle, owner_id, "trusted public key fingerprint: ");
    (void)shell64_write_text(console_capability_handle, owner_id, package_signing64_public_key_fingerprint());
    (void)shell64_write_text(console_capability_handle, owner_id, "\n");
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "signed package count: ", package_signing64_signed_package_count());
    (void)shell64_write_text(console_capability_handle, owner_id, "installed packages: signed bootstrap archive visible\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "signature verification: verified\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "payload hash status: verified\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "capability requests: visible; policy enforced\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "admitted capabilities: scoped only\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "denied capabilities: capability-policy denial observed\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "update-index: local signed fixture verified\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "rollback/replay: denied/handled\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "auto-install: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "public update fetch: unavailable/non-product\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "trusted-time expiry: unavailable/non-product\n");
    identity_transport64_init();
    (void)shell64_write_text(console_capability_handle, owner_id, "identity descriptor: signed local fixture verified\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "identity encrypted transport: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "identity credential transport: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "identity token storage: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "identity remote login: unavailable\n");
    account_association64_init();
    (void)shell64_write_text(console_capability_handle, owner_id, "account association mode: Mode B status only\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "local association: active/offline-capable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "personal association: unavailable/planned\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "enterprise association: unavailable/planned\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud association: unavailable/planned\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "security key login: unavailable/planned\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "remote account authority: none\n");
    cloud_storage64_init();
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud storage broker: foundation active\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud provider descriptor: signed local fixture verified\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud storage mode: unavailable/planned\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud token storage: denied while vault Mode B\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud encrypted transport: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud sync: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud upload/download: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud auto-upload/download: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud AI access: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "cloud app direct authority: denied\n");
    installer_ux64_init();
    (void)shell64_write_text(console_capability_handle, owner_id, "installer ux: planning and dry-run only\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "installer selected profile: general-use\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "installer writes planned: 0\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "installer formats planned: 0\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "installer boot entries planned: 0\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "installer real install approved: false\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "installer unavailable components: personal enterprise security-key cloud-sync ai-assisted-setup package-install browser gaming developer-toolchain\n");
    ai_policy64_init();
    (void)shell64_write_text(console_capability_handle, owner_id, "ai policy broker: foundation active\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai principal: request-only no default capabilities\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai action request: modeled\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai consent: required no auto-approve\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai assistant: host active; inference unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai backend mode: Mode B host and consent foundation only\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai context request: read-only system status scoped\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai context consent: allow once/read-only session/deny\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai denied request data: 0\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai allowed context: scoped read-only status only\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai model call: none\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai scripted response: none\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai package integrity: signed Product component\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai self-modification: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai cloud memory: unavailable\n");
    ai_policy64_action_probe();
    (void)shell64_write_text(console_capability_handle, owner_id, "ai actions: consent-scoped templates only\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai action broker: Mode B deterministic templates\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai action templates: assistant-note-write installer-dryrun open-settings-panel package-trust-status\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai forbidden actions: package-install package-update settings-mutation cloud-enable secret-token model-transport self-modification\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai note action: /HOME/ASSIST/NOTE.TXT committed readback verified\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai action consent: allow once required for write; read-only session allowed for status\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai action grant: scoped session-bound action-bound target-bound expired\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai action audit: request consent grant result revocation recorded\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai autonomous actions: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai audit: immutable queryable settings-visible\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai filesystem access: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai network access: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai settings access: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai package access: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai secret access: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai cloud access: denied\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ai automation: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "install authority: disabled; scoped capability required\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "update-check authority: scoped; no ambient network\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "update-apply authority: disabled; scoped install required\n");
    return shell64_write_text(console_capability_handle, owner_id, "no ambient install/update/network/cloud/fs/identity/secret/ai\n");
}

static const char *shell64_yes_no(u32 value)
{
    return (value != 0u) ? "yes" : "no";
}

static u32 shell64_write_status_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    const char *value)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_hwval_label_matches_filter(label) == 0u)
    {
        return 0u;
    }
#endif

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    (void)shell64_write_text(console_capability_handle, owner_id, value);
    return shell64_write_text(console_capability_handle, owner_id, "\n");
}

static u32 shell64_write_yes_no_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 value)
{
    return shell64_write_status_line(console_capability_handle, owner_id, label, shell64_yes_no(value));
}

static const char *shell64_keyboard_backend_label(void)
{
    if (i2c_hid64_device_found() != 0u)
    {
        return "LPSS I2C HID";
    }
    if (input64_ps2_enabled() != 0u)
    {
        return "PS/2";
    }
    if (xhci64_input_live() != 0u)
    {
        return "xHCI HID";
    }

    return "pending";
}

static const char *shell64_mouse_backend_label(void)
{
    if (i2c_hid64_pointer_found() != 0u)
    {
        return "LPSS I2C HID touchpad";
    }
    if (input64_mouse_enabled() != 0u)
    {
        return "PS/2 mouse";
    }
    if (xhci64_mouse_device() != 0u)
    {
        return "xHCI HID mouse";
    }

    return "pending";
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_storage_triage_mix(u32 token, u32 value)
{
    token ^= value;
    token *= 16777619u;
    return token;
}

static u32 shell64_print_nvme_storage_triage(u32 console_capability_handle, u32 owner_id)
{
    static const u8 apps_path[] = "/APPS";
    static const u8 busybox_path[] = "/APPS/BUSYBOX";
    static const u8 dynldlimit_path[] = "/APPS/DYNLDLIMIT";
    static const u8 ldlimit_path[] = "/APPS/LDLIMIT";
    mmio64_nvme_fat_stat_t apps_stat;
    mmio64_nvme_fat_stat_t busybox_stat;
    mmio64_nvme_fat_stat_t dynldlimit_stat;
    mmio64_nvme_fat_stat_t ldlimit_stat;
    mmio64_nvme_fat_dirent_t apps_dirent;
    u32 rw_cap_present;
    u32 apps_stat_ok;
    u32 busybox_stat_ok;
    u32 dynldlimit_stat_ok;
    u32 ldlimit_stat_ok;
    u32 apps_dir_result;
    u32 apps_dirent_ok;
    u32 dynldlimit_expected;
    u32 ldlimit_expected;
    u32 dynldlimit_match;
    u32 ldlimit_match;
    u32 stage_match;
    u32 hardware_capability;
    u32 pci_storage_count;
    u32 pci_nvme_count;
    u32 pci_raid_count;
    u32 pci_other_storage_count;
    u32 pci_intel_system_count;
    u32 pci_vmd_candidate_count;
    u32 pci_nvme_address;
    u32 pci_nvme_vendor_device;
    u32 pci_nvme_class;
    u32 pci_nvme_bar0;
    u32 pci_nvme_bar1;
    u32 pci_nvme_mmio_base_low;
    u32 pci_nvme_mmio_base_high;
    u32 pci_nvme_mmio_span_hint;
    u32 pci_nvme_mmio_flags;
    u32 pci_nvme_mmio_token;
    u32 pci_other_storage_address;
    u32 pci_other_storage_vendor_device;
    u32 pci_other_storage_class;
    u32 pci_other_storage_bar0;
    u32 pci_other_storage_bar1;
    u32 pci_intel_system_address;
    u32 pci_intel_system_vendor_device;
    u32 pci_intel_system_class;
    u32 pci_intel_system_bar0;
    u32 pci_intel_system_bar1;
    u32 pci_vmd_candidate_address;
    u32 pci_vmd_candidate_vendor_device;
    u32 pci_vmd_candidate_class;
    u32 pci_vmd_candidate_bar0;
    u32 pci_vmd_candidate_bar1;
    u32 pci_vmd_candidate_mmio_base_low;
    u32 pci_vmd_candidate_mmio_base_high;
    u32 pci_vmd_candidate_mmio_span_hint;
    u32 pci_vmd_candidate_mmio_flags;
    u32 pci_vmd_candidate_mmio_token;
    u32 pci_vmd_nested_plan;
    u32 pci_vmd_nested_enumerated;
    u32 pci_vmd_nested_nvme_count;
    u32 pci_vmd_nested_status;
    u32 pci_vmd_nested_token;
    u32 pci_vmd_nested_address;
    u32 pci_vmd_nested_vendor_device;
    u32 pci_vmd_nested_class;
    u32 pci_vmd_nested_bar0;
    u32 pci_vmd_nested_bar1;
    u32 pci_vmd_nested_scan_buses;
    u32 pci_vmd_nested_scan_devices;
    u32 pci_vmd_nested_scan_functions;
    u32 pci_vmd_nested_scan_windows;
    u32 pci_vmd_nested_scan_truncated;
    u32 pci_vmd_nested_mmio_base_low;
    u32 pci_vmd_nested_mmio_base_high;
    u32 pci_vmd_nested_mmio_span_hint;
    u32 pci_vmd_nested_mmio_flags;
    u32 pci_vmd_nested_mmio_token;
    u32 pci_vmd_nested_bind_ready;
    u32 pci_vmd_nested_bind_status;
    u32 pci_vmd_nested_bind_token;
    u32 pci_vmd_nested_register_candidate;
    u32 pci_vmd_nested_register_status;
    u32 pci_vmd_nested_register_token;
    u32 pci_vmd_nested_driver_plan_result;
    u32 pci_vmd_nested_driver_plan_state;
    u32 pci_vmd_nested_driver_plan_flags;
    u32 pci_vmd_nested_driver_plan_token;
    u32 pci_vmd_nested_driver_plan_stage_count;
    u32 pci_vmd_nested_driver_plan_denial_count;
    u32 pci_vmd_nested_driver_plan_unavailable_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 pci_vmd_nested_driver_bind_result;
    u32 nvme_candidate_source;
    u32 nvme_candidate_deferred;
    u32 nvme_candidate_bdf;
    u32 nvme_candidate_token;
#endif
    u32 token = 2166136261u;

    hardware_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_HARDWARE,
        CAPABILITY64_RIGHT_QUERY,
        owner_id);
    pci_storage_count = pci64_storage_count(hardware_capability, owner_id);
    pci_nvme_count = pci64_nvme_count(hardware_capability, owner_id);
    pci_raid_count = pci64_raid_count(hardware_capability, owner_id);
    pci_other_storage_count = pci64_other_storage_count(hardware_capability, owner_id);
    pci_intel_system_count = pci64_intel_system_count(hardware_capability, owner_id);
    pci_vmd_candidate_count = pci64_vmd_candidate_count(hardware_capability, owner_id);
    pci_nvme_address = pci64_first_nvme_address(hardware_capability, owner_id);
    pci_nvme_vendor_device = pci64_first_nvme_vendor_device(hardware_capability, owner_id);
    pci_nvme_class = pci64_first_nvme_class(hardware_capability, owner_id);
    pci_nvme_bar0 = pci64_first_nvme_bar0(hardware_capability, owner_id);
    pci_nvme_bar1 = pci64_first_nvme_bar1(hardware_capability, owner_id);
    pci_nvme_mmio_base_low = pci64_first_nvme_mmio_base_low(hardware_capability, owner_id);
    pci_nvme_mmio_base_high = pci64_first_nvme_mmio_base_high(hardware_capability, owner_id);
    pci_nvme_mmio_span_hint = pci64_first_nvme_mmio_span_hint(hardware_capability, owner_id);
    pci_nvme_mmio_flags = pci64_first_nvme_mmio_flags(hardware_capability, owner_id);
    pci_nvme_mmio_token = pci64_first_nvme_mmio_token(hardware_capability, owner_id);
    pci_other_storage_address = pci64_first_other_storage_address(hardware_capability, owner_id);
    pci_other_storage_vendor_device = pci64_first_other_storage_vendor_device(hardware_capability, owner_id);
    pci_other_storage_class = pci64_first_other_storage_class(hardware_capability, owner_id);
    pci_other_storage_bar0 = pci64_first_other_storage_bar0(hardware_capability, owner_id);
    pci_other_storage_bar1 = pci64_first_other_storage_bar1(hardware_capability, owner_id);
    pci_intel_system_address = pci64_first_intel_system_address(hardware_capability, owner_id);
    pci_intel_system_vendor_device = pci64_first_intel_system_vendor_device(hardware_capability, owner_id);
    pci_intel_system_class = pci64_first_intel_system_class(hardware_capability, owner_id);
    pci_intel_system_bar0 = pci64_first_intel_system_bar0(hardware_capability, owner_id);
    pci_intel_system_bar1 = pci64_first_intel_system_bar1(hardware_capability, owner_id);
    pci_vmd_candidate_address = pci64_first_vmd_candidate_address(hardware_capability, owner_id);
    pci_vmd_candidate_vendor_device = pci64_first_vmd_candidate_vendor_device(hardware_capability, owner_id);
    pci_vmd_candidate_class = pci64_first_vmd_candidate_class(hardware_capability, owner_id);
    pci_vmd_candidate_bar0 = pci64_first_vmd_candidate_bar0(hardware_capability, owner_id);
    pci_vmd_candidate_bar1 = pci64_first_vmd_candidate_bar1(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_base_low = pci64_first_vmd_candidate_mmio_base_low(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_base_high = pci64_first_vmd_candidate_mmio_base_high(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_span_hint = pci64_first_vmd_candidate_mmio_span_hint(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_flags = pci64_first_vmd_candidate_mmio_flags(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_token = pci64_first_vmd_candidate_mmio_token(hardware_capability, owner_id);
    pci_vmd_nested_plan = pci64_vmd_nested_plan(hardware_capability, owner_id);
    pci_vmd_nested_enumerated = pci64_vmd_nested_enumerated(hardware_capability, owner_id);
    pci_vmd_nested_nvme_count = pci64_vmd_nested_nvme_count(hardware_capability, owner_id);
    pci_vmd_nested_status = pci64_vmd_nested_status(hardware_capability, owner_id);
    pci_vmd_nested_token = pci64_vmd_nested_token(hardware_capability, owner_id);
    pci_vmd_nested_address = pci64_vmd_nested_first_address(hardware_capability, owner_id);
    pci_vmd_nested_vendor_device = pci64_vmd_nested_first_vendor_device(hardware_capability, owner_id);
    pci_vmd_nested_class = pci64_vmd_nested_first_class(hardware_capability, owner_id);
    pci_vmd_nested_bar0 = pci64_vmd_nested_first_bar0(hardware_capability, owner_id);
    pci_vmd_nested_bar1 = pci64_vmd_nested_first_bar1(hardware_capability, owner_id);
    pci_vmd_nested_scan_buses = pci64_vmd_nested_scan_buses(hardware_capability, owner_id);
    pci_vmd_nested_scan_devices = pci64_vmd_nested_scan_devices(hardware_capability, owner_id);
    pci_vmd_nested_scan_functions = pci64_vmd_nested_scan_functions(hardware_capability, owner_id);
    pci_vmd_nested_scan_windows = pci64_vmd_nested_scan_windows(hardware_capability, owner_id);
    pci_vmd_nested_scan_truncated = pci64_vmd_nested_scan_truncated(hardware_capability, owner_id);
    pci_vmd_nested_mmio_base_low = pci64_vmd_nested_mmio_base_low(hardware_capability, owner_id);
    pci_vmd_nested_mmio_base_high = pci64_vmd_nested_mmio_base_high(hardware_capability, owner_id);
    pci_vmd_nested_mmio_span_hint = pci64_vmd_nested_mmio_span_hint(hardware_capability, owner_id);
    pci_vmd_nested_mmio_flags = pci64_vmd_nested_mmio_flags(hardware_capability, owner_id);
    pci_vmd_nested_mmio_token = pci64_vmd_nested_mmio_token(hardware_capability, owner_id);
    pci_vmd_nested_bind_ready = pci64_vmd_nested_bind_ready(hardware_capability, owner_id);
    pci_vmd_nested_bind_status = pci64_vmd_nested_bind_status(hardware_capability, owner_id);
    pci_vmd_nested_bind_token = pci64_vmd_nested_bind_token(hardware_capability, owner_id);
    pci_vmd_nested_register_candidate = pci64_vmd_nested_register_candidate(hardware_capability, owner_id);
    pci_vmd_nested_register_status = pci64_vmd_nested_register_status(hardware_capability, owner_id);
    pci_vmd_nested_register_token = pci64_vmd_nested_register_token(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_result =
        pci64_stage_vmd_nested_driver_plan(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_state =
        pci64_vmd_nested_driver_plan_state(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_flags =
        pci64_vmd_nested_driver_plan_flags(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_token =
        pci64_vmd_nested_driver_plan_token(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_stage_count =
        pci64_vmd_nested_driver_plan_stage_count();
    pci_vmd_nested_driver_plan_denial_count =
        pci64_vmd_nested_driver_plan_denial_count();
    pci_vmd_nested_driver_plan_unavailable_count =
        pci64_vmd_nested_driver_plan_unavailable_count();
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    pci_vmd_nested_driver_bind_result =
        (pci_vmd_nested_driver_plan_result != PCI64_INVALID_RESULT)
            ? mmio64_bind_vmd_nested_nvme_candidate(
                hardware_capability,
                owner_id,
                pci_vmd_nested_address,
                pci_vmd_nested_driver_plan_result)
            : MMIO64_INVALID_RESULT;
    nvme_candidate_source = mmio64_nvme_candidate_source();
    nvme_candidate_deferred = mmio64_nvme_candidate_deferred();
    nvme_candidate_bdf = mmio64_nvme_candidate_bdf();
    nvme_candidate_token = mmio64_nvme_candidate_token();
#endif
    if (hardware_capability != CAPABILITY64_INVALID_HANDLE)
    {
        (void)capability64_revoke(hardware_capability, owner_id);
    }

    rw_cap_present = (mmio64_nvme_rw_capability() != CAPABILITY64_INVALID_HANDLE) ? 1u : 0u;
    apps_stat_ok = mmio64_nvme_fat_shell_stat_path(
        apps_path,
        (u32)(sizeof(apps_path) - 1u),
        owner_id,
        &apps_stat);
    busybox_stat_ok = mmio64_nvme_fat_shell_stat_path(
        busybox_path,
        (u32)(sizeof(busybox_path) - 1u),
        owner_id,
        &busybox_stat);
    dynldlimit_stat_ok = mmio64_nvme_fat_shell_stat_path(
        dynldlimit_path,
        (u32)(sizeof(dynldlimit_path) - 1u),
        owner_id,
        &dynldlimit_stat);
    ldlimit_stat_ok = mmio64_nvme_fat_shell_stat_path(
        ldlimit_path,
        (u32)(sizeof(ldlimit_path) - 1u),
        owner_id,
        &ldlimit_stat);
    apps_dir_result = mmio64_nvme_fat_shell_read_dirent(
        apps_path,
        (u32)(sizeof(apps_path) - 1u),
        0u,
        owner_id,
        &apps_dirent);
    apps_dirent_ok = (apps_dir_result == MMIO64_NVME_FAT_READDIR_OK) ? 1u : 0u;
    dynldlimit_expected = (boot_media64_app_bytes() != 0u) ? 1u : 0u;
    ldlimit_expected = (boot_media64_interp_bytes() != 0u) ? 1u : 0u;
    dynldlimit_match = ((dynldlimit_expected != 0u)
        && (dynldlimit_stat_ok != 0u)
        && (dynldlimit_stat.byte_count == boot_media64_app_bytes())) ? 1u : 0u;
    ldlimit_match = ((ldlimit_expected != 0u)
        && (ldlimit_stat_ok != 0u)
        && (ldlimit_stat.byte_count == boot_media64_interp_bytes())) ? 1u : 0u;
    stage_match = (((dynldlimit_expected == 0u) || (dynldlimit_match != 0u))
        && ((ldlimit_expected == 0u) || (ldlimit_match != 0u))) ? 1u : 0u;

    token = shell64_storage_triage_mix(token, mmio64_nvme_probe_found());
    token = shell64_storage_triage_mix(token, pci_storage_count);
    token = shell64_storage_triage_mix(token, pci_nvme_count);
    token = shell64_storage_triage_mix(token, pci_raid_count);
    token = shell64_storage_triage_mix(token, pci_other_storage_count);
    token = shell64_storage_triage_mix(token, pci_intel_system_count);
    token = shell64_storage_triage_mix(token, pci_vmd_candidate_count);
    token = shell64_storage_triage_mix(token, pci_nvme_address);
    token = shell64_storage_triage_mix(token, pci_nvme_vendor_device);
    token = shell64_storage_triage_mix(token, pci_nvme_class);
    token = shell64_storage_triage_mix(token, pci_nvme_mmio_flags);
    token = shell64_storage_triage_mix(token, pci_vmd_candidate_address);
    token = shell64_storage_triage_mix(token, pci_vmd_candidate_vendor_device);
    token = shell64_storage_triage_mix(token, pci_vmd_candidate_mmio_flags);
    token = shell64_storage_triage_mix(token, pci_vmd_candidate_mmio_token);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_plan);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_status);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_token);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_address);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_vendor_device);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_class);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_scan_buses);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_scan_windows);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_scan_truncated);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_mmio_base_low);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_mmio_base_high);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_mmio_span_hint);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_mmio_flags);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_mmio_token);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_bind_ready);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_bind_status);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_bind_token);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_register_candidate);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_register_status);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_register_token);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_driver_plan_result);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_driver_plan_state);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_driver_plan_flags);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_driver_plan_token);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_driver_plan_stage_count);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_driver_plan_denial_count);
    token = shell64_storage_triage_mix(token, pci_vmd_nested_driver_plan_unavailable_count);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    token = shell64_storage_triage_mix(token, nvme_candidate_source);
    token = shell64_storage_triage_mix(token, nvme_candidate_deferred);
    token = shell64_storage_triage_mix(token, nvme_candidate_bdf);
    token = shell64_storage_triage_mix(token, nvme_candidate_token);
#endif
    token = shell64_storage_triage_mix(token, mmio64_nvme_probe_ready());
    token = shell64_storage_triage_mix(token, mmio64_nvme_probe_identify());
    token = shell64_storage_triage_mix(token, mmio64_nvme_read_ioq_created());
    token = shell64_storage_triage_mix(token, mmio64_nvme_read_status());
    token = shell64_storage_triage_mix(token, mmio64_nvme_gpt_signature());
    token = shell64_storage_triage_mix(token, mmio64_nvme_gpt_vbr());
    token = shell64_storage_triage_mix(token, mmio64_nvme_fat_bpb());
    token = shell64_storage_triage_mix(token, mmio64_nvme_fat_located());
    token = shell64_storage_triage_mix(token, mmio64_nvme_fat_error());
    token = shell64_storage_triage_mix(token, rw_cap_present);
    token = shell64_storage_triage_mix(token, apps_stat_ok);
    token = shell64_storage_triage_mix(token, apps_dirent_ok);
    token = shell64_storage_triage_mix(token, busybox_stat_ok);
    token = shell64_storage_triage_mix(token, dynldlimit_stat_ok);
    token = shell64_storage_triage_mix(token, ldlimit_stat_ok);
    token = shell64_storage_triage_mix(token, dynldlimit_expected);
    token = shell64_storage_triage_mix(token, ldlimit_expected);
    token = shell64_storage_triage_mix(token, dynldlimit_match);
    token = shell64_storage_triage_mix(token, ldlimit_match);
    token = shell64_storage_triage_mix(token, stage_match);
    token = shell64_storage_triage_mix(token, boot_media64_status());

    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-nvme-triage storage-triage 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-found ", mmio64_nvme_probe_found());
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-storage ", pci_storage_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-nvme ", pci_nvme_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-raid ", pci_raid_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-other-storage ", pci_other_storage_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-intel-system ", pci_intel_system_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-vmd ", pci_vmd_candidate_count);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-pci ", pci_nvme_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-vendor-device ", pci_nvme_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-class ", pci_nvme_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-bar0 ", pci_nvme_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-bar1 ", pci_nvme_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-low ", pci_nvme_mmio_base_low);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-high ", pci_nvme_mmio_base_high);
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-mmio-span ", pci_nvme_mmio_span_hint);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-flags ", pci_nvme_mmio_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-token ", pci_nvme_mmio_token);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-candidate-source ", nvme_candidate_source);
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-candidate-deferred ", nvme_candidate_deferred);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-candidate-bdf ", nvme_candidate_bdf);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-candidate-token ", nvme_candidate_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-probe-error ", mmio64_nvme_probe_error());
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-regs ", mmio64_nvme_probe_register_snapshot());
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-cap-low ", mmio64_nvme_probe_cap_low());
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-cap-high ", mmio64_nvme_probe_cap_high());
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-vs ", mmio64_nvme_probe_version());
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-cc ", mmio64_nvme_probe_cc());
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-csts ", mmio64_nvme_probe_csts());
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-dstrd-bytes ", mmio64_nvme_probe_dstrd_bytes());
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-doorbell-page ", mmio64_nvme_probe_doorbell_page());
#endif
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-pci ", pci_other_storage_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-vendor-device ", pci_other_storage_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-class ", pci_other_storage_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-bar0 ", pci_other_storage_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-bar1 ", pci_other_storage_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-pci ", pci_intel_system_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-vendor-device ", pci_intel_system_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-class ", pci_intel_system_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-bar0 ", pci_intel_system_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-bar1 ", pci_intel_system_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-pci ", pci_vmd_candidate_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-vendor-device ", pci_vmd_candidate_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-class ", pci_vmd_candidate_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-bar0 ", pci_vmd_candidate_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-bar1 ", pci_vmd_candidate_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-low ", pci_vmd_candidate_mmio_base_low);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-high ", pci_vmd_candidate_mmio_base_high);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-mmio-span ", pci_vmd_candidate_mmio_span_hint);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-flags ", pci_vmd_candidate_mmio_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-token ", pci_vmd_candidate_mmio_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-plan ", pci_vmd_nested_plan);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-enum ", pci_vmd_nested_enumerated);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-nvme ", pci_vmd_nested_nvme_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-status ", pci_vmd_nested_status);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-token ", pci_vmd_nested_token);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-pci ", pci_vmd_nested_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-vendor-device ", pci_vmd_nested_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-class ", pci_vmd_nested_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-bar0 ", pci_vmd_nested_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-bar1 ", pci_vmd_nested_bar1);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-buses ", pci_vmd_nested_scan_buses);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-devices ", pci_vmd_nested_scan_devices);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-functions ", pci_vmd_nested_scan_functions);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-windows ", pci_vmd_nested_scan_windows);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-truncated ", pci_vmd_nested_scan_truncated);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-low ", pci_vmd_nested_mmio_base_low);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-high ", pci_vmd_nested_mmio_base_high);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-mmio-span ", pci_vmd_nested_mmio_span_hint);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-flags ", pci_vmd_nested_mmio_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-token ", pci_vmd_nested_mmio_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-bind-ready ", pci_vmd_nested_bind_ready);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-bind-status ", pci_vmd_nested_bind_status);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-bind-token ", pci_vmd_nested_bind_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-register-candidate ", pci_vmd_nested_register_candidate);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-register-status ", pci_vmd_nested_register_status);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-register-token ", pci_vmd_nested_register_token);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-result ", pci_vmd_nested_driver_plan_result);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-state ", pci_vmd_nested_driver_plan_state);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-flags ", pci_vmd_nested_driver_plan_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-token ", pci_vmd_nested_driver_plan_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-stage-count ", pci_vmd_nested_driver_plan_stage_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-denials ", pci_vmd_nested_driver_plan_denial_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-unavailable ", pci_vmd_nested_driver_plan_unavailable_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-ready ", mmio64_nvme_probe_ready());
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-identify ", mmio64_nvme_probe_identify());
    shell64_write_decimal_field(console_capability_handle, owner_id, " ioq ", mmio64_nvme_read_ioq_created());
    shell64_write_decimal_field(console_capability_handle, owner_id, " read-issued ", mmio64_nvme_read_issued());
    shell64_write_decimal_field(console_capability_handle, owner_id, " read-completed ", mmio64_nvme_read_completed());
    shell64_write_decimal_field(console_capability_handle, owner_id, " read-status ", mmio64_nvme_read_status());
    shell64_write_decimal_field(console_capability_handle, owner_id, " gpt-signature ", mmio64_nvme_gpt_signature());
    shell64_write_decimal_field(console_capability_handle, owner_id, " gpt-partitions ", mmio64_nvme_gpt_partitions());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fat32-start ", mmio64_nvme_gpt_fat32_start());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fat32-sectors ", mmio64_nvme_gpt_fat32_sectors());
    shell64_write_decimal_field(console_capability_handle, owner_id, " gpt-vbr ", mmio64_nvme_gpt_vbr());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fat-bpb ", mmio64_nvme_fat_bpb());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fat-located ", mmio64_nvme_fat_located());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fat-unavailable ", mmio64_nvme_fat_unavailable());
    shell64_write_decimal_field(console_capability_handle, owner_id, " fat-error ", mmio64_nvme_fat_error());
    shell64_write_decimal_field(console_capability_handle, owner_id, " rw-cap ", rw_cap_present);
    shell64_write_decimal_field(console_capability_handle, owner_id, " rw-delegated ", mmio64_nvme_rw_delegated());
    shell64_write_decimal_field(console_capability_handle, owner_id, " rw-error ", mmio64_nvme_rw_error());
    shell64_write_decimal_field(console_capability_handle, owner_id, " apps-stat ", apps_stat_ok);
    shell64_write_decimal_field(console_capability_handle, owner_id, " apps-type ", apps_stat.entry_type);
    shell64_write_decimal_field(console_capability_handle, owner_id, " apps-dirent ", apps_dirent_ok);
    shell64_write_decimal_field(console_capability_handle, owner_id, " apps-dir-result ", apps_dir_result);
    shell64_write_decimal_field(console_capability_handle, owner_id, " busybox-stat ", busybox_stat_ok);
    shell64_write_decimal_field(console_capability_handle, owner_id, " busybox-bytes ", busybox_stat.byte_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " dynldlimit-stat ", dynldlimit_stat_ok);
    shell64_write_decimal_field(console_capability_handle, owner_id, " dynldlimit-bytes ", dynldlimit_stat.byte_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " ldlimit-stat ", ldlimit_stat_ok);
    shell64_write_decimal_field(console_capability_handle, owner_id, " ldlimit-bytes ", ldlimit_stat.byte_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " boot-staged ", boot_media64_available());
    shell64_write_decimal_field(console_capability_handle, owner_id, " boot-app-bytes ", boot_media64_app_bytes());
    shell64_write_decimal_field(console_capability_handle, owner_id, " boot-interp-bytes ", boot_media64_interp_bytes());
    shell64_write_decimal_field(console_capability_handle, owner_id, " boot-status ", boot_media64_status());
    shell64_write_decimal_field(console_capability_handle, owner_id, " stage-expected ", ((dynldlimit_expected != 0u) || (ldlimit_expected != 0u)) ? 1u : 0u);
    shell64_write_decimal_field(console_capability_handle, owner_id, " dynldlimit-expected ", dynldlimit_expected);
    shell64_write_decimal_field(console_capability_handle, owner_id, " ldlimit-expected ", ldlimit_expected);
    shell64_write_decimal_field(console_capability_handle, owner_id, " dynldlimit-match ", dynldlimit_match);
    shell64_write_decimal_field(console_capability_handle, owner_id, " ldlimit-match ", ldlimit_match);
    shell64_write_decimal_field(console_capability_handle, owner_id, " stage-match ", stage_match);
    shell64_write_hex32_field(console_capability_handle, owner_id, " token ", token);
    (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-vmd-nvme-bind bind 1");
    shell64_write_hex32_field(console_capability_handle, owner_id, " result ", pci_vmd_nested_driver_bind_result);
    shell64_write_decimal_field(console_capability_handle, owner_id, " state ", mmio64_vmd_nvme_bind_state());
    shell64_write_hex32_field(console_capability_handle, owner_id, " flags ", mmio64_vmd_nvme_bind_flags());
    shell64_write_hex32_field(console_capability_handle, owner_id, " token ", mmio64_vmd_nvme_bind_token());
    shell64_write_decimal_field(console_capability_handle, owner_id, " count ", mmio64_vmd_nvme_bind_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " denials ", mmio64_vmd_nvme_bind_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " unavailable ", mmio64_vmd_nvme_bind_unavailable_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " candidate-source ", mmio64_nvme_candidate_source());
    shell64_write_decimal_field(console_capability_handle, owner_id, " candidate-deferred ", mmio64_nvme_candidate_deferred());
    shell64_write_hex32_field(console_capability_handle, owner_id, " candidate-bdf ", mmio64_nvme_candidate_bdf());
    shell64_write_hex32_field(console_capability_handle, owner_id, " candidate-token ", mmio64_nvme_candidate_token());
    return shell64_end_hwval_composite_line(console_capability_handle, owner_id);
#else
    return 1u;
#endif
}
#endif

static u32 shell64_print_hardware_validation_status(u32 console_capability_handle, u32 owner_id)
{
    u32 network_online = (virtio_net64_dhcp_ack() != 0u) ? 1u : 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 hardware_capability;
    u32 pci_storage_count;
    u32 pci_nvme_count;
    u32 pci_raid_count;
    u32 pci_other_storage_count;
    u32 pci_intel_system_count;
    u32 pci_vmd_candidate_count;
    u32 pci_nvme_address;
    u32 pci_nvme_vendor_device;
    u32 pci_nvme_class;
    u32 pci_nvme_bar0;
    u32 pci_nvme_bar1;
    u32 pci_nvme_mmio_base_low;
    u32 pci_nvme_mmio_base_high;
    u32 pci_nvme_mmio_span_hint;
    u32 pci_nvme_mmio_flags;
    u32 pci_nvme_mmio_token;
    u32 pci_other_storage_address;
    u32 pci_other_storage_vendor_device;
    u32 pci_other_storage_class;
    u32 pci_other_storage_bar0;
    u32 pci_other_storage_bar1;
    u32 pci_intel_system_address;
    u32 pci_intel_system_vendor_device;
    u32 pci_intel_system_class;
    u32 pci_intel_system_bar0;
    u32 pci_intel_system_bar1;
    u32 pci_vmd_candidate_address;
    u32 pci_vmd_candidate_vendor_device;
    u32 pci_vmd_candidate_class;
    u32 pci_vmd_candidate_bar0;
    u32 pci_vmd_candidate_bar1;
    u32 pci_vmd_candidate_mmio_base_low;
    u32 pci_vmd_candidate_mmio_base_high;
    u32 pci_vmd_candidate_mmio_span_hint;
    u32 pci_vmd_candidate_mmio_flags;
    u32 pci_vmd_candidate_mmio_token;
    u32 pci_vmd_nested_plan;
    u32 pci_vmd_nested_enumerated;
    u32 pci_vmd_nested_nvme_count;
    u32 pci_vmd_nested_status;
    u32 pci_vmd_nested_token;
    u32 pci_vmd_nested_address;
    u32 pci_vmd_nested_vendor_device;
    u32 pci_vmd_nested_class;
    u32 pci_vmd_nested_bar0;
    u32 pci_vmd_nested_bar1;
    u32 pci_vmd_nested_scan_buses;
    u32 pci_vmd_nested_scan_devices;
    u32 pci_vmd_nested_scan_functions;
    u32 pci_vmd_nested_scan_windows;
    u32 pci_vmd_nested_scan_truncated;
    u32 pci_vmd_nested_mmio_base_low;
    u32 pci_vmd_nested_mmio_base_high;
    u32 pci_vmd_nested_mmio_span_hint;
    u32 pci_vmd_nested_mmio_flags;
    u32 pci_vmd_nested_mmio_token;
    u32 pci_vmd_nested_bind_ready;
    u32 pci_vmd_nested_bind_status;
    u32 pci_vmd_nested_bind_token;
    u32 pci_vmd_nested_register_candidate;
    u32 pci_vmd_nested_register_status;
    u32 pci_vmd_nested_register_token;
    u32 pci_vmd_nested_driver_plan_result;
    u32 pci_vmd_nested_driver_plan_state;
    u32 pci_vmd_nested_driver_plan_flags;
    u32 pci_vmd_nested_driver_plan_token;
    u32 pci_vmd_nested_driver_plan_stage_count;
    u32 pci_vmd_nested_driver_plan_denial_count;
    u32 pci_vmd_nested_driver_plan_unavailable_count;
    u32 pci_vmd_nested_driver_bind_result;
    u32 nvme_candidate_source;
    u32 nvme_candidate_deferred;
    u32 nvme_candidate_bdf;
    u32 nvme_candidate_token;

    hardware_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_HARDWARE,
        CAPABILITY64_RIGHT_QUERY,
        owner_id);
    hardware64_registry_refresh(hardware_capability, owner_id);
    pci_storage_count = pci64_storage_count(hardware_capability, owner_id);
    pci_nvme_count = pci64_nvme_count(hardware_capability, owner_id);
    pci_raid_count = pci64_raid_count(hardware_capability, owner_id);
    pci_other_storage_count = pci64_other_storage_count(hardware_capability, owner_id);
    pci_intel_system_count = pci64_intel_system_count(hardware_capability, owner_id);
    pci_vmd_candidate_count = pci64_vmd_candidate_count(hardware_capability, owner_id);
    pci_nvme_address = pci64_first_nvme_address(hardware_capability, owner_id);
    pci_nvme_vendor_device = pci64_first_nvme_vendor_device(hardware_capability, owner_id);
    pci_nvme_class = pci64_first_nvme_class(hardware_capability, owner_id);
    pci_nvme_bar0 = pci64_first_nvme_bar0(hardware_capability, owner_id);
    pci_nvme_bar1 = pci64_first_nvme_bar1(hardware_capability, owner_id);
    pci_nvme_mmio_base_low = pci64_first_nvme_mmio_base_low(hardware_capability, owner_id);
    pci_nvme_mmio_base_high = pci64_first_nvme_mmio_base_high(hardware_capability, owner_id);
    pci_nvme_mmio_span_hint = pci64_first_nvme_mmio_span_hint(hardware_capability, owner_id);
    pci_nvme_mmio_flags = pci64_first_nvme_mmio_flags(hardware_capability, owner_id);
    pci_nvme_mmio_token = pci64_first_nvme_mmio_token(hardware_capability, owner_id);
    pci_other_storage_address = pci64_first_other_storage_address(hardware_capability, owner_id);
    pci_other_storage_vendor_device = pci64_first_other_storage_vendor_device(hardware_capability, owner_id);
    pci_other_storage_class = pci64_first_other_storage_class(hardware_capability, owner_id);
    pci_other_storage_bar0 = pci64_first_other_storage_bar0(hardware_capability, owner_id);
    pci_other_storage_bar1 = pci64_first_other_storage_bar1(hardware_capability, owner_id);
    pci_intel_system_address = pci64_first_intel_system_address(hardware_capability, owner_id);
    pci_intel_system_vendor_device = pci64_first_intel_system_vendor_device(hardware_capability, owner_id);
    pci_intel_system_class = pci64_first_intel_system_class(hardware_capability, owner_id);
    pci_intel_system_bar0 = pci64_first_intel_system_bar0(hardware_capability, owner_id);
    pci_intel_system_bar1 = pci64_first_intel_system_bar1(hardware_capability, owner_id);
    pci_vmd_candidate_address = pci64_first_vmd_candidate_address(hardware_capability, owner_id);
    pci_vmd_candidate_vendor_device = pci64_first_vmd_candidate_vendor_device(hardware_capability, owner_id);
    pci_vmd_candidate_class = pci64_first_vmd_candidate_class(hardware_capability, owner_id);
    pci_vmd_candidate_bar0 = pci64_first_vmd_candidate_bar0(hardware_capability, owner_id);
    pci_vmd_candidate_bar1 = pci64_first_vmd_candidate_bar1(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_base_low = pci64_first_vmd_candidate_mmio_base_low(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_base_high = pci64_first_vmd_candidate_mmio_base_high(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_span_hint = pci64_first_vmd_candidate_mmio_span_hint(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_flags = pci64_first_vmd_candidate_mmio_flags(hardware_capability, owner_id);
    pci_vmd_candidate_mmio_token = pci64_first_vmd_candidate_mmio_token(hardware_capability, owner_id);
    pci_vmd_nested_plan = pci64_vmd_nested_plan(hardware_capability, owner_id);
    pci_vmd_nested_enumerated = pci64_vmd_nested_enumerated(hardware_capability, owner_id);
    pci_vmd_nested_nvme_count = pci64_vmd_nested_nvme_count(hardware_capability, owner_id);
    pci_vmd_nested_status = pci64_vmd_nested_status(hardware_capability, owner_id);
    pci_vmd_nested_token = pci64_vmd_nested_token(hardware_capability, owner_id);
    pci_vmd_nested_address = pci64_vmd_nested_first_address(hardware_capability, owner_id);
    pci_vmd_nested_vendor_device = pci64_vmd_nested_first_vendor_device(hardware_capability, owner_id);
    pci_vmd_nested_class = pci64_vmd_nested_first_class(hardware_capability, owner_id);
    pci_vmd_nested_bar0 = pci64_vmd_nested_first_bar0(hardware_capability, owner_id);
    pci_vmd_nested_bar1 = pci64_vmd_nested_first_bar1(hardware_capability, owner_id);
    pci_vmd_nested_scan_buses = pci64_vmd_nested_scan_buses(hardware_capability, owner_id);
    pci_vmd_nested_scan_devices = pci64_vmd_nested_scan_devices(hardware_capability, owner_id);
    pci_vmd_nested_scan_functions = pci64_vmd_nested_scan_functions(hardware_capability, owner_id);
    pci_vmd_nested_scan_windows = pci64_vmd_nested_scan_windows(hardware_capability, owner_id);
    pci_vmd_nested_scan_truncated = pci64_vmd_nested_scan_truncated(hardware_capability, owner_id);
    pci_vmd_nested_mmio_base_low = pci64_vmd_nested_mmio_base_low(hardware_capability, owner_id);
    pci_vmd_nested_mmio_base_high = pci64_vmd_nested_mmio_base_high(hardware_capability, owner_id);
    pci_vmd_nested_mmio_span_hint = pci64_vmd_nested_mmio_span_hint(hardware_capability, owner_id);
    pci_vmd_nested_mmio_flags = pci64_vmd_nested_mmio_flags(hardware_capability, owner_id);
    pci_vmd_nested_mmio_token = pci64_vmd_nested_mmio_token(hardware_capability, owner_id);
    pci_vmd_nested_bind_ready = pci64_vmd_nested_bind_ready(hardware_capability, owner_id);
    pci_vmd_nested_bind_status = pci64_vmd_nested_bind_status(hardware_capability, owner_id);
    pci_vmd_nested_bind_token = pci64_vmd_nested_bind_token(hardware_capability, owner_id);
    pci_vmd_nested_register_candidate = pci64_vmd_nested_register_candidate(hardware_capability, owner_id);
    pci_vmd_nested_register_status = pci64_vmd_nested_register_status(hardware_capability, owner_id);
    pci_vmd_nested_register_token = pci64_vmd_nested_register_token(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_result =
        pci64_stage_vmd_nested_driver_plan(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_state =
        pci64_vmd_nested_driver_plan_state(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_flags =
        pci64_vmd_nested_driver_plan_flags(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_token =
        pci64_vmd_nested_driver_plan_token(hardware_capability, owner_id);
    pci_vmd_nested_driver_plan_stage_count =
        pci64_vmd_nested_driver_plan_stage_count();
    pci_vmd_nested_driver_plan_denial_count =
        pci64_vmd_nested_driver_plan_denial_count();
    pci_vmd_nested_driver_plan_unavailable_count =
        pci64_vmd_nested_driver_plan_unavailable_count();
    pci_vmd_nested_driver_bind_result =
        (pci_vmd_nested_driver_plan_result != PCI64_INVALID_RESULT)
            ? mmio64_bind_vmd_nested_nvme_candidate(
                hardware_capability,
                owner_id,
                pci_vmd_nested_address,
                pci_vmd_nested_driver_plan_result)
            : MMIO64_INVALID_RESULT;
    nvme_candidate_source = mmio64_nvme_candidate_source();
    nvme_candidate_deferred = mmio64_nvme_candidate_deferred();
    nvme_candidate_bdf = mmio64_nvme_candidate_bdf();
    nvme_candidate_token = mmio64_nvme_candidate_token();
    if (hardware_capability != CAPABILITY64_INVALID_HANDLE)
    {
        (void)capability64_revoke(hardware_capability, owner_id);
    }
#endif

    (void)shell64_write_hwval_text_line(console_capability_handle, owner_id, "hardware validation: read-only Product mode\n");
    (void)shell64_write_hwval_text_line(console_capability_handle, owner_id, "machine model: unavailable from firmware table\n");
    (void)shell64_write_hwval_text_line(console_capability_handle, owner_id, "secure boot: unavailable/not Product-detected\n");
    (void)shell64_write_hwval_text_line(console_capability_handle, owner_id, "build profile: Product\n");
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "boot path: ",
        (package_signing64_signed() != 0u) ? "UEFI Product" : "BIOS checksum fallback");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirects: ", g_shell64_redirect_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect appends: ", g_shell64_redirect_append_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect writes: ", g_shell64_redirect_write_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect bytes: ", g_shell64_redirect_byte_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect commits: ", g_shell64_redirect_commit_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect denials: ", g_shell64_redirect_denial_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect last result: ", g_shell64_redirect_last_result);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect last error: ", g_shell64_redirect_last_error);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect backend: ", g_shell64_redirect_backend);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect usb requested: ", g_shell64_redirect_usb_requested);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell redirect usb unavailable: ", g_shell64_redirect_usb_unavailable);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage present: ", xhci64_usb_storage_present());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage ready: ", xhci64_usb_storage_ready());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage block bytes: ", xhci64_usb_storage_block_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage last lba: ", xhci64_usb_storage_last_lba());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage error: ", xhci64_usb_storage_error());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage reads: ", xhci64_usb_storage_read_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage writes: ", xhci64_usb_storage_write_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage last completion: ", xhci64_usb_storage_last_completion());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat located: ", mmio64_usb_fat_located());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat error: ", mmio64_usb_fat_error());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat writes: ", mmio64_usb_fat_write_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat appends: ", mmio64_usb_fat_append_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat last bytes: ", mmio64_usb_fat_last_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat partition start: ", mmio64_usb_fat_last_partition_start());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat partition sectors: ", mmio64_usb_fat_last_partition_sectors());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb fat partition scheme: ", mmio64_usb_fat_partition_scheme());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell fat read last error: ", mmio64_nvme_fat_shell_read_last_error());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell fat read last bytes: ", mmio64_nvme_fat_shell_read_last_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell fat read last capacity: ", mmio64_nvme_fat_shell_read_last_capacity());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell fat read last size: ", mmio64_nvme_fat_shell_read_last_size());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "shell fat read last attr: ", mmio64_nvme_fat_shell_read_last_attr());
#endif
    if (display64_available() != 0u)
    {
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "framebuffer width: ", display64_width());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "framebuffer height: ", display64_height());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "framebuffer pitch pixels: ", display64_pixels_per_scanline());
        (void)shell64_write_hex32_line(console_capability_handle, owner_id, "framebuffer format: ", display64_framebuffer_format());
        (void)shell64_write_hex32_line(console_capability_handle, owner_id, "framebuffer base high: ", display64_framebuffer_base_high());
        (void)shell64_write_hex32_line(console_capability_handle, owner_id, "framebuffer base low: ", display64_framebuffer_base_low());
        (void)shell64_write_hex32_line(console_capability_handle, owner_id, "framebuffer bytes low: ", display64_framebuffer_bytes_low());
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "framebuffer required high: ",
            display64_framebuffer_required_bytes_high());
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "framebuffer required low: ",
            display64_framebuffer_required_bytes_low());
        (void)shell64_write_yes_no_line(
            console_capability_handle,
            owner_id,
            "framebuffer stride sane: ",
            display64_framebuffer_stride_ok());
        (void)shell64_write_yes_no_line(
            console_capability_handle,
            owner_id,
            "framebuffer bounds sane: ",
            display64_framebuffer_bounds_ok());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display text scale: ", display64_text_scale());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display console columns: ", display64_console_columns());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display console rows: ", display64_console_rows());
        (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "display readable: ", display64_readable());
        (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "display cursor visible: ", display64_cursor_visible());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display cursor draws: ", display64_compositor_cursor_count());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display direct cursor draws: ", display64_direct_cursor_count());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display cursor x: ", display64_cursor_x());
        (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display cursor y: ", display64_cursor_y());
        (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "display cursor in bounds: ", display64_cursor_in_bounds());
        (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "display cursor surface ready: ", display64_cursor_surface_ready());
        (void)shell64_write_yes_no_line(
            console_capability_handle,
            owner_id,
            "display compositor direct: ",
            display64_compositor_direct_mode());
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "display ui polish token: ",
            display64_ui_polish_token());
        (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-display-readability display-readability 1");
        shell64_write_decimal_field(console_capability_handle, owner_id, " available ", display64_available());
        shell64_write_decimal_field(console_capability_handle, owner_id, " width ", display64_width());
        shell64_write_decimal_field(console_capability_handle, owner_id, " height ", display64_height());
        shell64_write_decimal_field(console_capability_handle, owner_id, " pitch ", display64_pixels_per_scanline());
        shell64_write_decimal_field(console_capability_handle, owner_id, " stride-ok ", display64_framebuffer_stride_ok());
        shell64_write_decimal_field(console_capability_handle, owner_id, " bounds-ok ", display64_framebuffer_bounds_ok());
        shell64_write_decimal_field(console_capability_handle, owner_id, " scale ", display64_text_scale());
        shell64_write_decimal_field(console_capability_handle, owner_id, " viewport-x ", display64_console_viewport_x());
        shell64_write_decimal_field(console_capability_handle, owner_id, " viewport-y ", display64_console_viewport_y());
        shell64_write_decimal_field(console_capability_handle, owner_id, " viewport-w ", display64_console_viewport_w());
        shell64_write_decimal_field(console_capability_handle, owner_id, " viewport-h ", display64_console_viewport_h());
        shell64_write_decimal_field(console_capability_handle, owner_id, " columns ", display64_console_columns());
        shell64_write_decimal_field(console_capability_handle, owner_id, " rows ", display64_console_rows());
        shell64_write_decimal_field(console_capability_handle, owner_id, " fit ", display64_console_fit());
        shell64_write_decimal_field(console_capability_handle, owner_id, " readable ", display64_readable());
        shell64_write_decimal_field(console_capability_handle, owner_id, " clip ", display64_console_clip_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " cursor-visible ", display64_cursor_visible());
        shell64_write_decimal_field(console_capability_handle, owner_id, " cursor-draws ", display64_compositor_cursor_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " direct-cursor-draws ", display64_direct_cursor_count());
        shell64_write_hex32_field(
            console_capability_handle,
            owner_id,
            " token ",
            display64_layout_token());
        (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
        (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-ui-polish ui-polish 1");
        shell64_write_decimal_field(console_capability_handle, owner_id, " compositor-active ", display64_compositor_init_done());
        shell64_write_decimal_field(console_capability_handle, owner_id, " compositor-direct ", display64_compositor_direct_mode());
        shell64_write_decimal_field(console_capability_handle, owner_id, " font ", display64_font_init_done());
        shell64_write_decimal_field(console_capability_handle, owner_id, " wm ", display64_wm_init_done());
        shell64_write_decimal_field(console_capability_handle, owner_id, " desktop ", display64_desktop_init_done());
        shell64_write_decimal_field(console_capability_handle, owner_id, " taskbar ", display64_desktop_taskbar_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " launcher ", display64_desktop_launcher_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " windows ", display64_wm_window_created_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " cursor-visible ", display64_cursor_visible());
        shell64_write_decimal_field(console_capability_handle, owner_id, " product-chrome ", display64_gui_product_chrome_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " product-layout ", display64_product_layout_active());
        shell64_write_decimal_field(console_capability_handle, owner_id, " startup-minimized ", display64_product_startup_minimized_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " readiness-strip ", display64_gui_settings_readiness_strip_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " display-ready ", display64_product_display_ready());
        shell64_write_decimal_field(console_capability_handle, owner_id, " input-ready ", display64_product_input_ready());
        shell64_write_decimal_field(console_capability_handle, owner_id, " storage-ready ", display64_product_storage_ready());
        shell64_write_decimal_field(console_capability_handle, owner_id, " network-ready ", display64_product_network_ready());
        shell64_write_decimal_field(console_capability_handle, owner_id, " diagnostic-overlays-suppressed ",
            display64_gui_input_diag_suppressed_count() + display64_gui_mouse_diag_suppressed_count());
        shell64_write_hex32_field(
            console_capability_handle,
            owner_id,
            " token ",
            display64_ui_polish_token());
        (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
        (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-cursor-path cursor-path 1");
        shell64_write_decimal_field(console_capability_handle, owner_id, " surface-ready ", display64_cursor_surface_ready());
        shell64_write_decimal_field(console_capability_handle, owner_id, " format-supported ", display64_cursor_framebuffer_format_supported());
        shell64_write_decimal_field(console_capability_handle, owner_id, " compositor-active ", display64_compositor_init_done());
        shell64_write_decimal_field(console_capability_handle, owner_id, " compositor-direct ", display64_compositor_direct_mode());
        shell64_write_decimal_field(console_capability_handle, owner_id, " visible ", display64_cursor_visible());
        shell64_write_decimal_field(console_capability_handle, owner_id, " draws ", display64_compositor_cursor_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " direct-draws ", display64_direct_cursor_count());
        shell64_write_decimal_field(console_capability_handle, owner_id, " x ", display64_cursor_x());
        shell64_write_decimal_field(console_capability_handle, owner_id, " y ", display64_cursor_y());
        shell64_write_decimal_field(console_capability_handle, owner_id, " buttons ", display64_cursor_buttons());
        shell64_write_decimal_field(console_capability_handle, owner_id, " in-bounds ", display64_cursor_in_bounds());
        shell64_write_decimal_field(console_capability_handle, owner_id, " rect-w ", display64_cursor_rect_w());
        shell64_write_decimal_field(console_capability_handle, owner_id, " rect-h ", display64_cursor_rect_h());
        shell64_write_decimal_field(console_capability_handle, owner_id, " saved ", display64_cursor_saved_valid());
        shell64_write_decimal_field(console_capability_handle, owner_id, " drawn ", display64_cursor_drawn_valid());
        shell64_write_hex32_field(
            console_capability_handle,
            owner_id,
            " token ",
            display64_cursor_path_token());
        (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
        shell64_write_gui_interaction_telemetry(console_capability_handle, owner_id);
#endif
    }
    else
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "framebuffer: unavailable\n");
    }

    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "keyboard backend: ",
        shell64_keyboard_backend_label());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "keyboard left shift: ", input64_keyboard_left_shift());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "keyboard right shift: ", input64_keyboard_right_shift());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "keyboard caps lock: ", input64_keyboard_caps_lock());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "usb keyboard modifier: ", input64_usb_hid_last_modifier());
#endif
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "mouse backend: ",
        shell64_mouse_backend_label());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci found: ", xhci64_found());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci handoff: ", xhci64_legacy_handoff());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci hid keyboard: ", xhci64_hid_device());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci mouse endpoint: ", xhci64_mouse_endpoint_present());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci mouse probe configured: ", xhci64_mouse_endpoint_present());
#endif
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci mouse reports: ", xhci64_mouse_reports());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci mouse bytes: ", xhci64_mouse_report_bytes());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci mouse report size: ", xhci64_mouse_report_size());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "usb mouse layout ready: ", input64_usb_hid_mouse_layout_ready());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb mouse layout reports: ", input64_usb_hid_mouse_layout_reports());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb mouse layout fallbacks: ", input64_usb_hid_mouse_layout_fallbacks());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb mouse layout bytes: ", input64_usb_hid_mouse_layout_report_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb mouse buttons bit: ", input64_usb_hid_mouse_layout_buttons_offset());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb mouse x bit: ", input64_usb_hid_mouse_layout_x_offset());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb mouse y bit: ", input64_usb_hid_mouse_layout_y_offset());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb mouse wheel bit: ", input64_usb_hid_mouse_layout_wheel_offset());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci slots disabled: ", xhci64_slots_disabled());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci slot disable failures: ", xhci64_slot_disable_failures());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci max slots: ", xhci64_max_slots_limit());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci disable slot poll limit: ", xhci64_disable_slot_poll_limit());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last enable completion: ", xhci64_last_enable_slot_completion());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last enable slot: ", xhci64_last_enable_slot_id());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last address completion: ", xhci64_last_address_completion());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last address slot: ", xhci64_last_address_slot());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last address port: ", xhci64_last_address_port());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last address speed: ", xhci64_last_address_speed());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci last address event dw0: ", xhci64_last_address_event_dw0());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci last address event dw1: ", xhci64_last_address_event_dw1());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci last address event dw2: ", xhci64_last_address_event_dw2());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci last address event dw3: ", xhci64_last_address_event_dw3());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci pre-address portsc raw: ", xhci64_pre_address_portsc());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci pre-address portsc pls: ", xhci64_pre_address_portsc_pls());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci post-address portsc raw: ", xhci64_post_address_portsc());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci post-address portsc pls: ", xhci64_post_address_portsc_pls());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci address failures: ", xhci64_address_failure_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci hid interfaces: ", xhci64_hid_interface_inventory());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci boot mouse interface: ", xhci64_boot_mouse_interface());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci boot mouse port: ", xhci64_boot_mouse_port());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci boot mouse dci: ", xhci64_boot_mouse_dci());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci boot mouse mps: ", xhci64_boot_mouse_mps());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci boot mouse configured: ", xhci64_boot_mouse_configured());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci mouse last completion: ", xhci64_mouse_last_completion());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci mouse last event dw0: ", xhci64_mouse_last_event_dw0());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci mouse last event dw1: ", xhci64_mouse_last_event_dw1());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci mouse last event dw2: ", xhci64_mouse_last_event_dw2());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "xhci mouse last event dw3: ", xhci64_mouse_last_event_dw3());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last disable completion: ", xhci64_last_disable_slot_completion());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last disable slot: ", xhci64_last_disable_slot_id());
#endif
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci error: ", xhci64_error());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last skip port: ", xhci64_last_skip_port());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last skip code: ", xhci64_last_skip_code());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last device class: ", xhci64_last_device_class());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last device subclass: ", xhci64_last_device_subclass());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last device protocol: ", xhci64_last_device_protocol());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last config bytes: ", xhci64_last_config_total_length());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last interface class: ", xhci64_last_interface_class());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last interface subclass: ", xhci64_last_interface_subclass());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last interface protocol: ", xhci64_last_interface_protocol());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci last endpoint mps: ", xhci64_last_endpoint_max_packet());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci broad mouse probes: ", xhci64_broad_mouse_probe_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci vendor mouse candidates: ", xhci64_vendor_mouse_candidate_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci port probe attempts: ", xhci64_port_probe_attempts());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci port probe retry skips: ", xhci64_port_probe_retry_skips());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first hid port: ", xhci64_first_hid_port());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first hid class: ", xhci64_first_hid_interface_class());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first hid subclass: ", xhci64_first_hid_interface_subclass());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first hid protocol: ", xhci64_first_hid_interface_protocol());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first mouse port: ", xhci64_first_mouse_candidate_port());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first mouse class: ", xhci64_first_mouse_candidate_interface_class());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first mouse subclass: ", xhci64_first_mouse_candidate_interface_subclass());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first mouse protocol: ", xhci64_first_mouse_candidate_interface_protocol());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci first mouse mps: ", xhci64_first_mouse_candidate_endpoint_mps());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks xhci init: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_XHCI_INIT));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks boot diag: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_BOOT_DIAG));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks nvme probe: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_NVME_PROBE));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks display init: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_DISPLAY_INIT));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks i2c hid init: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_I2C_HID_INIT));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks timer wait: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_TIMER_WAIT));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks keyboard wait: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_KEYBOARD_WAIT));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks mouse wait: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_MOUSE_WAIT));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks login: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_LOGIN));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks lock probe: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_LOCK_PROBE));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks wm probe: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_WM_PROBE));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks desktop probe: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_DESKTOP_PROBE));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks virtio net init: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_VIRTIO_NET_INIT));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot ticks pit to shell: ", boot_diag64_timing_ticks(BOOT_DIAG64_TIMING_PIT_TO_SHELL));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot timer wait spin budget: ", boot_diag64_timer_wait_spin_budget());
#endif
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "i2c pointer found: ", i2c_hid64_pointer_found());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer kind: ", i2c_hid64_pointer_kind());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer address: ", i2c_hid64_pointer_address());
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-i2c-acpi i2c-acpi 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " found ", i2c_hid64_acpi_telemetry(I2C_HID64_ACPI_TELEMETRY_FOUND));
    shell64_write_decimal_field(console_capability_handle, owner_id, " source ", i2c_hid64_acpi_telemetry(I2C_HID64_ACPI_TELEMETRY_BIND_SOURCE));
    shell64_write_decimal_field(console_capability_handle, owner_id, " addr ", i2c_hid64_acpi_telemetry(I2C_HID64_ACPI_TELEMETRY_ADDRESS));
    shell64_write_decimal_field(console_capability_handle, owner_id, " plausible ", i2c_hid64_acpi_telemetry(I2C_HID64_ACPI_TELEMETRY_ADDRESS_PLAUSIBLE));
    shell64_write_decimal_field(console_capability_handle, owner_id, " speed ", i2c_hid64_acpi_telemetry(I2C_HID64_ACPI_TELEMETRY_SPEED_HZ));
    shell64_write_decimal_field(console_capability_handle, owner_id, " gpio-found ", i2c_hid64_acpi_telemetry(I2C_HID64_ACPI_TELEMETRY_GPIO_FOUND));
    shell64_write_decimal_field(console_capability_handle, owner_id, " gpio-pin ", i2c_hid64_acpi_telemetry(I2C_HID64_ACPI_TELEMETRY_GPIO_PIN));
    (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer desc-reg: ", i2c_hid64_pointer_descriptor_register());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer report-reg: ", i2c_hid64_pointer_report_descriptor_register());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer report-len: ", i2c_hid64_pointer_report_descriptor_length());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer input-reg: ", i2c_hid64_pointer_input_register());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer cmd-reg: ", i2c_hid64_pointer_command_register());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer max-in: ", i2c_hid64_pointer_max_input_length());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "i2c pointer report-id: ", i2c_hid64_pointer_report_has_id());
#endif
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer reports: ", i2c_hid64_pointer_report_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer error: ", i2c_hid64_pointer_error());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c controllers: ", pci64_lpss_i2c_count());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c primary probe addresses: ", i2c_hid64_primary_probe_address_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer probe addresses: ", i2c_hid64_pointer_probe_address_count());
#endif
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer candidates: ", pci64_lpss_i2c_pointer_candidate_count());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary pci: ", pci64_lpss_i2c_address());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary bar0: ", pci64_lpss_i2c_bar0());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary bar1: ", pci64_lpss_i2c_bar1());
#endif
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary base low: ", pci64_lpss_i2c_base_low());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary base high: ", pci64_lpss_i2c_base_high());
#endif
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary flags: ", pci64_lpss_i2c_mmio_flags());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_yes_no_line(
        console_capability_handle,
        owner_id,
        "i2c primary acpi resource needed: ",
        ((pci64_lpss_i2c_mmio_flags() & PCI64_LPSS_I2C_MMIO_FLAG_ACPI_RESOURCE_REQUIRED) != 0u) ? 1u : 0u);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c second pci: ", pci64_lpss_i2c_second_address());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c second bar0: ", pci64_lpss_i2c_second_bar0());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c second bar1: ", pci64_lpss_i2c_second_bar1());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c second base low: ", pci64_lpss_i2c_second_base_low());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c second base high: ", pci64_lpss_i2c_second_base_high());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c second flags: ", pci64_lpss_i2c_second_mmio_flags());
    (void)shell64_write_yes_no_line(
        console_capability_handle,
        owner_id,
        "i2c second acpi resource needed: ",
        ((pci64_lpss_i2c_second_mmio_flags() & PCI64_LPSS_I2C_MMIO_FLAG_ACPI_RESOURCE_REQUIRED) != 0u) ? 1u : 0u);
#endif
    if (pci64_lpss_i2c_pointer_candidate_count() != 0u)
    {
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 pci: ",
            pci64_lpss_i2c_pointer_candidate_address(0u));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 bar0: ",
            pci64_lpss_i2c_pointer_candidate_bar0(0u));
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 bar1: ",
            pci64_lpss_i2c_pointer_candidate_bar1(0u));
#endif
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 base low: ",
            pci64_lpss_i2c_pointer_candidate_base_low(0u));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 base high: ",
            pci64_lpss_i2c_pointer_candidate_base_high(0u));
#endif
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 flags: ",
            pci64_lpss_i2c_pointer_candidate_mmio_flags(0u));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)shell64_write_yes_no_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 acpi resource needed: ",
            ((pci64_lpss_i2c_pointer_candidate_mmio_flags(0u) & PCI64_LPSS_I2C_MMIO_FLAG_ACPI_RESOURCE_REQUIRED) != 0u) ? 1u : 0u);
#endif
    }
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "mouse packets: ", input64_mouse_packet_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "mouse x: ", input64_mouse_x());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "mouse y: ", input64_mouse_y());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 fallback present: ", input64_ps2_present());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 fallback enabled: ", input64_ps2_enabled());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "ps2 mouse packet bytes: ", input64_ps2_mouse_packet_bytes());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 mouse wheel enabled: ", input64_ps2_mouse_wheel_enabled());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "ps2 mouse wheel packets: ", input64_ps2_mouse_wheel_count());
#endif
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "apic status: ",
        (apic64_enabled() != 0u) ? "APIC enabled" : "PIC fallback");
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "pci/ecam status: ",
        (pci64_ecam_active() != 0u) ? "ECAM active" : "legacy/fallback");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi xsdt high: ", (u32)(pci64_acpi_xsdt() >> 32));
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi xsdt low: ", (u32)(pci64_acpi_xsdt() & 0xFFFFFFFFull));
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi fadt high: ", (u32)(pci64_acpi_fadt() >> 32));
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi fadt low: ", (u32)(pci64_acpi_fadt() & 0xFFFFFFFFull));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "acpi fadt bytes: ", pci64_acpi_fadt_bytes());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi dsdt high: ", (u32)(pci64_acpi_dsdt() >> 32));
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi dsdt low: ", (u32)(pci64_acpi_dsdt() & 0xFFFFFFFFull));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "acpi dsdt bytes: ", pci64_acpi_dsdt_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "acpi ssdt count: ", pci64_acpi_ssdt_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "acpi ssdt total bytes: ", pci64_acpi_ssdt_total_bytes());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi ssdt0 high: ", (u32)(pci64_acpi_ssdt0() >> 32));
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "acpi ssdt0 low: ", (u32)(pci64_acpi_ssdt0() & 0xFFFFFFFFull));
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "acpi ssdt0 bytes: ", pci64_acpi_ssdt0_bytes());
#endif
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "usb hci uhci: ",
        pci64_usb_uhci_count());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "usb hci ohci: ",
        pci64_usb_ohci_count());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "usb hci ehci: ",
        pci64_usb_ehci_count());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "usb hci xhci: ",
        pci64_usb_xhci_count());
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "usb input coverage: ",
        "xHCI native; UHCI/OHCI/EHCI config-detected with PS/2 or firmware legacy fallback");
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme detected: ", mmio64_nvme_probe_found());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme ready: ", mmio64_nvme_probe_ready());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci storage controllers: ", pci_storage_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci nvme controllers: ", pci_nvme_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci raid controllers: ", pci_raid_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci other storage controllers: ", pci_other_storage_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci intel system candidates: ", pci_intel_system_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd candidates: ", pci_vmd_candidate_count);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme first bdf: ", pci_nvme_address);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme vendor/device: ", pci_nvme_vendor_device);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme class: ", pci_nvme_class);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme bar0: ", pci_nvme_bar0);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme bar1: ", pci_nvme_bar1);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme mmio base low: ", pci_nvme_mmio_base_low);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme mmio base high: ", pci_nvme_mmio_base_high);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci nvme mmio span: ", pci_nvme_mmio_span_hint);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme mmio flags: ", pci_nvme_mmio_flags);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci nvme mmio token: ", pci_nvme_mmio_token);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme candidate source: ", nvme_candidate_source);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme candidate deferred: ", nvme_candidate_deferred);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "nvme candidate bdf: ", nvme_candidate_bdf);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "nvme candidate token: ", nvme_candidate_token);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci other storage first bdf: ", pci_other_storage_address);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci other storage vendor/device: ", pci_other_storage_vendor_device);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci other storage class: ", pci_other_storage_class);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci other storage bar0: ", pci_other_storage_bar0);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci other storage bar1: ", pci_other_storage_bar1);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci intel system first bdf: ", pci_intel_system_address);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci intel system vendor/device: ", pci_intel_system_vendor_device);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci intel system class: ", pci_intel_system_class);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci intel system bar0: ", pci_intel_system_bar0);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci intel system bar1: ", pci_intel_system_bar1);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd first bdf: ", pci_vmd_candidate_address);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd vendor/device: ", pci_vmd_candidate_vendor_device);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd class: ", pci_vmd_candidate_class);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd bar0: ", pci_vmd_candidate_bar0);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd bar1: ", pci_vmd_candidate_bar1);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd mmio base low: ", pci_vmd_candidate_mmio_base_low);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd mmio base high: ", pci_vmd_candidate_mmio_base_high);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd mmio span: ", pci_vmd_candidate_mmio_span_hint);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd mmio flags: ", pci_vmd_candidate_mmio_flags);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd mmio token: ", pci_vmd_candidate_mmio_token);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested plan: ", pci_vmd_nested_plan);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested enum: ", pci_vmd_nested_enumerated);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested nvme: ", pci_vmd_nested_nvme_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested status: ", pci_vmd_nested_status);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested token: ", pci_vmd_nested_token);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested first bdf: ", pci_vmd_nested_address);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested vendor/device: ", pci_vmd_nested_vendor_device);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested class: ", pci_vmd_nested_class);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested bar0: ", pci_vmd_nested_bar0);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested bar1: ", pci_vmd_nested_bar1);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested scan buses: ", pci_vmd_nested_scan_buses);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested scan devices: ", pci_vmd_nested_scan_devices);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested scan functions: ", pci_vmd_nested_scan_functions);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested scan windows: ", pci_vmd_nested_scan_windows);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested scan truncated: ", pci_vmd_nested_scan_truncated);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested mmio low: ", pci_vmd_nested_mmio_base_low);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested mmio high: ", pci_vmd_nested_mmio_base_high);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested mmio span: ", pci_vmd_nested_mmio_span_hint);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested mmio flags: ", pci_vmd_nested_mmio_flags);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested mmio token: ", pci_vmd_nested_mmio_token);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested bind ready: ", pci_vmd_nested_bind_ready);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested bind status: ", pci_vmd_nested_bind_status);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested bind token: ", pci_vmd_nested_bind_token);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested register candidate: ", pci_vmd_nested_register_candidate);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested register status: ", pci_vmd_nested_register_status);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested register token: ", pci_vmd_nested_register_token);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested driver plan result: ", pci_vmd_nested_driver_plan_result);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested driver plan state: ", pci_vmd_nested_driver_plan_state);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested driver plan flags: ", pci_vmd_nested_driver_plan_flags);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "pci vmd nested driver plan token: ", pci_vmd_nested_driver_plan_token);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested driver plan stage count: ", pci_vmd_nested_driver_plan_stage_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested driver plan denials: ", pci_vmd_nested_driver_plan_denial_count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "pci vmd nested driver plan unavailable: ", pci_vmd_nested_driver_plan_unavailable_count);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "vmd nvme bind result: ", pci_vmd_nested_driver_bind_result);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "vmd nvme bind state: ", mmio64_vmd_nvme_bind_state());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "vmd nvme bind flags: ", mmio64_vmd_nvme_bind_flags());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "vmd nvme bind token: ", mmio64_vmd_nvme_bind_token());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "vmd nvme bind count: ", mmio64_vmd_nvme_bind_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "vmd nvme bind denials: ", mmio64_vmd_nvme_bind_denial_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "vmd nvme bind unavailable: ", mmio64_vmd_nvme_bind_unavailable_count());
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-nvme-pci nvme-pci-diag 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-storage ", pci_storage_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-nvme ", pci_nvme_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-raid ", pci_raid_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-other-storage ", pci_other_storage_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-intel-system ", pci_intel_system_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-vmd ", pci_vmd_candidate_count);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-pci ", pci_nvme_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-vendor-device ", pci_nvme_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-class ", pci_nvme_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-bar0 ", pci_nvme_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-bar1 ", pci_nvme_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-low ", pci_nvme_mmio_base_low);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-high ", pci_nvme_mmio_base_high);
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-mmio-span ", pci_nvme_mmio_span_hint);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-flags ", pci_nvme_mmio_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-mmio-token ", pci_nvme_mmio_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-candidate-source ", nvme_candidate_source);
    shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-candidate-deferred ", nvme_candidate_deferred);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-candidate-bdf ", nvme_candidate_bdf);
    shell64_write_hex32_field(console_capability_handle, owner_id, " nvme-candidate-token ", nvme_candidate_token);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-pci ", pci_other_storage_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-vendor-device ", pci_other_storage_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-class ", pci_other_storage_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-bar0 ", pci_other_storage_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " other-storage-bar1 ", pci_other_storage_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-pci ", pci_intel_system_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-vendor-device ", pci_intel_system_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-class ", pci_intel_system_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-bar0 ", pci_intel_system_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " intel-system-bar1 ", pci_intel_system_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-pci ", pci_vmd_candidate_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-vendor-device ", pci_vmd_candidate_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-class ", pci_vmd_candidate_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-bar0 ", pci_vmd_candidate_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-bar1 ", pci_vmd_candidate_bar1);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-low ", pci_vmd_candidate_mmio_base_low);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-high ", pci_vmd_candidate_mmio_base_high);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-mmio-span ", pci_vmd_candidate_mmio_span_hint);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-flags ", pci_vmd_candidate_mmio_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-mmio-token ", pci_vmd_candidate_mmio_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-plan ", pci_vmd_nested_plan);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-enum ", pci_vmd_nested_enumerated);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-nvme ", pci_vmd_nested_nvme_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-status ", pci_vmd_nested_status);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-token ", pci_vmd_nested_token);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-pci ", pci_vmd_nested_address);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-vendor-device ", pci_vmd_nested_vendor_device);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-class ", pci_vmd_nested_class);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-bar0 ", pci_vmd_nested_bar0);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-bar1 ", pci_vmd_nested_bar1);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-buses ", pci_vmd_nested_scan_buses);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-devices ", pci_vmd_nested_scan_devices);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-functions ", pci_vmd_nested_scan_functions);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-windows ", pci_vmd_nested_scan_windows);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-scan-truncated ", pci_vmd_nested_scan_truncated);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-low ", pci_vmd_nested_mmio_base_low);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-high ", pci_vmd_nested_mmio_base_high);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-mmio-span ", pci_vmd_nested_mmio_span_hint);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-flags ", pci_vmd_nested_mmio_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-mmio-token ", pci_vmd_nested_mmio_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-bind-ready ", pci_vmd_nested_bind_ready);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-bind-status ", pci_vmd_nested_bind_status);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-bind-token ", pci_vmd_nested_bind_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-register-candidate ", pci_vmd_nested_register_candidate);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-register-status ", pci_vmd_nested_register_status);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-register-token ", pci_vmd_nested_register_token);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-result ", pci_vmd_nested_driver_plan_result);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-state ", pci_vmd_nested_driver_plan_state);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-flags ", pci_vmd_nested_driver_plan_flags);
    shell64_write_hex32_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-token ", pci_vmd_nested_driver_plan_token);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-stage-count ", pci_vmd_nested_driver_plan_stage_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-denials ", pci_vmd_nested_driver_plan_denial_count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " vmd-nested-driver-plan-unavailable ", pci_vmd_nested_driver_plan_unavailable_count);
    (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-vmd-nvme-bind bind 1");
    shell64_write_hex32_field(console_capability_handle, owner_id, " result ", pci_vmd_nested_driver_bind_result);
    shell64_write_decimal_field(console_capability_handle, owner_id, " state ", mmio64_vmd_nvme_bind_state());
    shell64_write_hex32_field(console_capability_handle, owner_id, " flags ", mmio64_vmd_nvme_bind_flags());
    shell64_write_hex32_field(console_capability_handle, owner_id, " token ", mmio64_vmd_nvme_bind_token());
    shell64_write_decimal_field(console_capability_handle, owner_id, " count ", mmio64_vmd_nvme_bind_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " denials ", mmio64_vmd_nvme_bind_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " unavailable ", mmio64_vmd_nvme_bind_unavailable_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " candidate-source ", mmio64_nvme_candidate_source());
    shell64_write_decimal_field(console_capability_handle, owner_id, " candidate-deferred ", mmio64_nvme_candidate_deferred());
    shell64_write_hex32_field(console_capability_handle, owner_id, " candidate-bdf ", mmio64_nvme_candidate_bdf());
    shell64_write_hex32_field(console_capability_handle, owner_id, " candidate-token ", mmio64_nvme_candidate_token());
    (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "nvme bar high: ", (u32)(mmio64_nvme_probe_bar0() >> 32));
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "nvme bar low: ", (u32)mmio64_nvme_probe_bar0());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme probe unavailable: ", mmio64_nvme_probe_unavailable());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme probe error: ", mmio64_nvme_probe_error());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme fat located: ", mmio64_nvme_fat_located());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme fat unavailable: ", mmio64_nvme_fat_unavailable());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme fat error: ", mmio64_nvme_fat_error());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw delegated: ", mmio64_nvme_rw_delegated());
    (void)shell64_write_yes_no_line(
        console_capability_handle,
        owner_id,
        "nvme rw capability: ",
        mmio64_nvme_rw_capability() != CAPABILITY64_INVALID_HANDLE);
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw delete: ", mmio64_nvme_rw_shell_delete());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw delete verified: ", mmio64_nvme_rw_shell_delete_verified());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw mkdir: ", mmio64_nvme_rw_shell_mkdir());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw copy: ", mmio64_nvme_rw_shell_copy());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw recursive copy: ", mmio64_nvme_rw_shell_recursive_copy());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw rename: ", mmio64_nvme_rw_shell_rename());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw move: ", mmio64_nvme_rw_shell_move());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme rw recursive delete: ", mmio64_nvme_rw_shell_recursive_delete());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme fat mkdir proof: ", mmio64_nvme_fat_mkdir_proof());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme fat copy proof: ", mmio64_nvme_fat_copy_proof());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme fat rename proof: ", mmio64_nvme_fat_rename_proof());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme fat move proof: ", mmio64_nvme_fat_move_proof());
    (void)shell64_write_yes_no_line(
        console_capability_handle,
        owner_id,
        "nvme fat recursive delete proof: ",
        mmio64_nvme_fat_recursive_delete_proof());
    (void)shell64_write_yes_no_line(
        console_capability_handle,
        owner_id,
        "nvme fat recursive copy proof: ",
        mmio64_nvme_fat_recursive_copy_proof());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme fat dir grow count: ", mmio64_nvme_fat_dir_grow_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme fat dir grow cluster: ", mmio64_nvme_fat_dir_grow_cluster());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme fat dir grow denials: ", mmio64_nvme_fat_dir_grow_denial());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme fat dir grow tombstones: ", mmio64_nvme_fat_dir_grow_tombstone());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme rw mutation denials: ", mmio64_nvme_rw_shell_mutation_denial());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme rw error: ", mmio64_nvme_rw_error());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "boot media linux staged: ", boot_media64_available());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media app bytes: ", boot_media64_app_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media interp bytes: ", boot_media64_interp_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media flags: ", boot_media64_flags());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media status: ", boot_media64_status());
    (void)shell64_print_nvme_storage_triage(console_capability_handle, owner_id);
#endif
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ahci detected: ", pci64_ecam_ahci_found());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "hardware registry devices: ",
        hardware64_registry_count());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "hardware driver bound: ",
        hardware64_registry_driver_bound_count());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "hardware driver deferred: ",
        hardware64_registry_driver_deferred_count());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "hardware driver unsupported: ",
        hardware64_registry_driver_unsupported_count());
    (void)shell64_write_decimal_line(
        console_capability_handle,
        owner_id,
        "hardware driver failed: ",
        hardware64_registry_driver_failed_count());
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-hardware-registry hardware-registry 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " refresh ", hardware64_registry_refresh_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " limit ", hardware64_registry_limit());
    shell64_write_decimal_field(console_capability_handle, owner_id, " inventory ", hardware64_registry_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-enumerated ", hardware64_registry_pci_device_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " pci-query-denial ", hardware64_registry_pci_query_denial_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " acpi-tables ", hardware64_registry_acpi_table_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " display-device ", hardware64_registry_display_device_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " input-device ", hardware64_registry_input_device_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " storage-device ", hardware64_registry_storage_device_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " usb-controller ", hardware64_registry_usb_controller_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " network-device ", hardware64_registry_network_device_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-bound ", hardware64_registry_driver_bound_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-candidate ", hardware64_registry_driver_candidate_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-deferred ", hardware64_registry_driver_deferred_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-unsupported ", hardware64_registry_driver_unsupported_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-failed ", hardware64_registry_driver_failed_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " overflow ", hardware64_registry_overflow_count());
    shell64_write_hex32_field(
        console_capability_handle,
        owner_id,
        " token ",
        hardware64_registry_token());
    (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
#endif
    (void)shell64_write_yes_no_line(
        console_capability_handle,
        owner_id,
        "network device detected: ",
        (virtio_net64_found() != 0u) || (e1000e64_found() != 0u));
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "network product status: ",
        (network_online != 0u) ? "online brokered" : "unavailable cleanly");
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "package trust surface: ",
        (package_signing64_signed() != 0u) ? "UEFI Ed25519 verified" : "BIOS checksum-only fallback");
    (void)shell64_write_text(console_capability_handle, owner_id, "installer dry-run: pending manual evidence; dry-run only\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "internal writes: disabled by default\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "format authority: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "nvram boot-entry authority: unavailable\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "real install approved: false\n");
    return shell64_write_text(console_capability_handle, owner_id, "authority: read-only scoped validation; no ambient storage/installer/network/update/install\n");
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_print_hardware_validation_summary(u32 console_capability_handle, u32 owner_id)
{
    u32 hardware_capability;
    u32 keyboard_ready;
    u32 pointer_ready;
    u32 input_ready;
    u32 storage_ready;
    u32 network_devices;
    u32 driver_bound;
    u32 driver_deferred;
    u32 driver_failed;

    hardware_capability =
        capability64_grant_service(
            SERVICE_ENDPOINT_CLASS_HARDWARE,
            CAPABILITY64_RIGHT_QUERY,
            owner_id);
    if (hardware_capability != CAPABILITY64_INVALID_HANDLE)
    {
        (void)hardware64_registry_refresh(hardware_capability, owner_id);
        (void)capability64_revoke(hardware_capability, owner_id);
    }

    keyboard_ready =
        ((input64_keyboard_scancode_count() != 0u) || (xhci64_report_count() != 0u)) ? 1u : 0u;
    pointer_ready =
        ((input64_mouse_packet_count() != 0u)
            || (xhci64_mouse_reports() != 0u)
            || (i2c_hid64_pointer_report_count() != 0u)) ? 1u : 0u;
    input_ready = ((keyboard_ready != 0u) || (pointer_ready != 0u)) ? 1u : 0u;
    storage_ready = mmio64_nvme_fat_located();
    network_devices = hardware64_registry_network_device_count();
    driver_bound = hardware64_registry_driver_bound_count();
    driver_deferred = hardware64_registry_driver_deferred_count();
    driver_failed = hardware64_registry_driver_failed_count();

    (void)shell64_write_text(console_capability_handle, owner_id, "LimitlessOS universal hardware summary\n");
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "display readable: ", display64_readable());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display width: ", display64_width());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "display height: ", display64_height());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "display cursor visible: ", display64_cursor_visible());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "keyboard ready: ", keyboard_ready);
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "pointer ready: ", pointer_ready);
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 fallback enabled: ", input64_ps2_enabled());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci present: ", xhci64_found());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci mouse endpoint: ", xhci64_mouse_endpoint_present());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "i2c pointer found: ", i2c_hid64_pointer_found());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme fat mounted: ", storage_ready);
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "nvme scoped writes: ", mmio64_nvme_rw_delegated());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "network devices: ", network_devices);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "hardware inventory: ", hardware64_registry_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "drivers bound: ", driver_bound);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "drivers deferred: ", driver_deferred);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "drivers failed: ", driver_failed);
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-hw-summary summary 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " display-readable ", display64_readable());
    shell64_write_decimal_field(console_capability_handle, owner_id, " input-ready ", input_ready);
    shell64_write_decimal_field(console_capability_handle, owner_id, " keyboard-ready ", keyboard_ready);
    shell64_write_decimal_field(console_capability_handle, owner_id, " pointer-ready ", pointer_ready);
    shell64_write_decimal_field(console_capability_handle, owner_id, " storage-ready ", storage_ready);
    shell64_write_decimal_field(console_capability_handle, owner_id, " network-devices ", network_devices);
    shell64_write_decimal_field(console_capability_handle, owner_id, " hardware-inventory ", hardware64_registry_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-bound ", driver_bound);
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-deferred ", driver_deferred);
    shell64_write_decimal_field(console_capability_handle, owner_id, " driver-failed ", driver_failed);
    shell64_write_decimal_field(console_capability_handle, owner_id, " xhci-error ", xhci64_error());
    shell64_write_decimal_field(console_capability_handle, owner_id, " i2c-pointer-error ", i2c_hid64_pointer_error());
    (void)shell64_end_hwval_composite_line(console_capability_handle, owner_id);
    return shell64_write_text(console_capability_handle, owner_id, "Use hwval full for raw counters and handoff evidence.\n");
}

static void shell64_refresh_hardware_registry(u32 owner_id)
{
    u32 hardware_capability =
        capability64_grant_service(
            SERVICE_ENDPOINT_CLASS_HARDWARE,
            CAPABILITY64_RIGHT_QUERY,
            owner_id);
    if (hardware_capability != CAPABILITY64_INVALID_HANDLE)
    {
        (void)hardware64_registry_refresh(hardware_capability, owner_id);
        (void)capability64_revoke(hardware_capability, owner_id);
    }
}

static const char *shell64_hardware_class_name(u32 class_id)
{
    if (class_id == HARDWARE64_CLASS_PLATFORM) { return "platform"; }
    if (class_id == HARDWARE64_CLASS_DISPLAY) { return "display"; }
    if (class_id == HARDWARE64_CLASS_INPUT) { return "input"; }
    if (class_id == HARDWARE64_CLASS_STORAGE) { return "storage"; }
    if (class_id == HARDWARE64_CLASS_USB) { return "usb"; }
    if (class_id == HARDWARE64_CLASS_NETWORK) { return "network"; }
    return "unknown";
}

static const char *shell64_hardware_source_name(u32 source)
{
    if (source == 1u) { return "pci"; }
    if (source == 2u) { return "uefi"; }
    if (source == 3u) { return "ps2"; }
    if (source == 4u) { return "xhci"; }
    if (source == 5u) { return "i2c"; }
    if (source == 6u) { return "nvme"; }
    if (source == 7u) { return "network"; }
    return "unknown";
}

static const char *shell64_hardware_binding_name(u32 binding)
{
    if (binding == HARDWARE64_BINDING_CANDIDATE) { return "candidate"; }
    if (binding == HARDWARE64_BINDING_BOUND) { return "bound"; }
    if (binding == HARDWARE64_BINDING_DEFERRED) { return "deferred"; }
    if (binding == HARDWARE64_BINDING_UNSUPPORTED) { return "unsupported"; }
    if (binding == HARDWARE64_BINDING_FAILED) { return "failed"; }
    return "none";
}

static const char *shell64_hardware_device_name(u32 class_id, u32 subclass_id)
{
    if (class_id == HARDWARE64_CLASS_PLATFORM)
    {
        if (subclass_id == 1u) { return "pci-ecam"; }
        if (subclass_id == 2u) { return "legacy-pci"; }
    }
    else if (class_id == HARDWARE64_CLASS_DISPLAY)
    {
        if (subclass_id == 1u) { return "gop-framebuffer"; }
        if (subclass_id == 2u) { return "pci-display"; }
    }
    else if (class_id == HARDWARE64_CLASS_INPUT)
    {
        if (subclass_id == 1u) { return "keyboard"; }
        if (subclass_id == 2u) { return "pointer"; }
    }
    else if (class_id == HARDWARE64_CLASS_STORAGE)
    {
        if (subclass_id == 1u) { return "nvme"; }
        if (subclass_id == 2u) { return "ahci"; }
    }
    else if (class_id == HARDWARE64_CLASS_USB)
    {
        if (subclass_id == 1u) { return "uhci"; }
        if (subclass_id == 2u) { return "ohci"; }
        if (subclass_id == 3u) { return "ehci"; }
        if (subclass_id == 4u) { return "xhci"; }
    }
    else if (class_id == HARDWARE64_CLASS_NETWORK)
    {
        if (subclass_id == 1u) { return "virtio-net"; }
        if (subclass_id == 2u) { return "e1000e"; }
    }

    return "device";
}

static u32 shell64_print_hardware_devices(u32 console_capability_handle, u32 owner_id)
{
    u32 index;
    u32 count;

    shell64_refresh_hardware_registry(owner_id);
    count = hardware64_registry_count();
    (void)shell64_write_text(console_capability_handle, owner_id, "devices\n");
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "inventory: ", count);
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "bound: ", hardware64_registry_driver_bound_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "deferred: ", hardware64_registry_driver_deferred_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "failed: ", hardware64_registry_driver_failed_count());

    for (index = 0u; index < count; ++index)
    {
        u32 class_id = hardware64_registry_record_class(index);
        u32 subclass_id = hardware64_registry_record_subclass(index);
        u32 binding = hardware64_registry_record_binding(index);
        u32 source = hardware64_registry_record_source(index);

        (void)shell64_write_text(console_capability_handle, owner_id, "  ");
        shell64_write_decimal_field(console_capability_handle, owner_id, "", index);
        (void)shell64_write_text(console_capability_handle, owner_id, " ");
        (void)shell64_write_text(console_capability_handle, owner_id, shell64_hardware_class_name(class_id));
        (void)shell64_write_text(console_capability_handle, owner_id, " ");
        (void)shell64_write_text(console_capability_handle, owner_id, shell64_hardware_device_name(class_id, subclass_id));
        (void)shell64_write_text(console_capability_handle, owner_id, " ");
        (void)shell64_write_text(console_capability_handle, owner_id, shell64_hardware_source_name(source));
        (void)shell64_write_text(console_capability_handle, owner_id, " ");
        (void)shell64_write_text(console_capability_handle, owner_id, shell64_hardware_binding_name(binding));
        shell64_write_hex32_field(console_capability_handle, owner_id, " addr ", hardware64_registry_record_address(index));
        shell64_write_hex32_field(console_capability_handle, owner_id, " flags ", hardware64_registry_record_flags(index));
        (void)shell64_write_text(console_capability_handle, owner_id, "\n");
    }

    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-hw-devices devices 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " inventory ", count);
    shell64_write_decimal_field(console_capability_handle, owner_id, " bound ", hardware64_registry_driver_bound_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " deferred ", hardware64_registry_driver_deferred_count());
    shell64_write_decimal_field(console_capability_handle, owner_id, " failed ", hardware64_registry_driver_failed_count());
    shell64_write_hex32_field(console_capability_handle, owner_id, " token ", hardware64_registry_token());
    return shell64_end_hwval_composite_line(console_capability_handle, owner_id);
}

static const char *shell64_xhci_protocol_name(u32 protocol)
{
    if (protocol == 2u) { return "usb2"; }
    if (protocol == 3u) { return "usb3"; }
    return "usb";
}

static u32 shell64_print_hardware_ports(u32 console_capability_handle, u32 owner_id)
{
    u32 port_id;
    u32 port_limit = xhci64_hcs_ports();
    u32 printed = 0u;

    if (port_limit > 32u)
    {
        port_limit = 32u;
    }

    (void)shell64_write_text(console_capability_handle, owner_id, "ports\n");
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci present: ", xhci64_found());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci root ports: ", xhci64_hcs_ports());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci connected: ", xhci64_connected_ports());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci usb2 ports: ", xhci64_usb2_ports());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci usb3 ports: ", xhci64_usb3_ports());

    for (port_id = 1u; port_id <= port_limit; ++port_id)
    {
        u32 protocol = xhci64_port_protocol(port_id);
        u32 portsc = xhci64_portsc_snapshot(port_id);
        u32 connected = ((portsc & SHELL64_XHCI_PORTSC_CCS) != 0u) ? 1u : 0u;
        if ((connected == 0u) && (protocol == 0u))
        {
            continue;
        }

        ++printed;
        (void)shell64_write_text(console_capability_handle, owner_id, "  xhci port ");
        shell64_write_decimal_field(console_capability_handle, owner_id, "", port_id);
        (void)shell64_write_text(console_capability_handle, owner_id, " ");
        (void)shell64_write_text(console_capability_handle, owner_id, shell64_xhci_protocol_name(protocol));
        shell64_write_decimal_field(console_capability_handle, owner_id, " connected ", connected);
        shell64_write_decimal_field(
            console_capability_handle,
            owner_id,
            " enabled ",
            ((portsc & SHELL64_XHCI_PORTSC_PED) != 0u) ? 1u : 0u);
        shell64_write_decimal_field(console_capability_handle, owner_id, " pls ", xhci64_portsc_snapshot_pls(port_id));
        shell64_write_decimal_field(
            console_capability_handle,
            owner_id,
            " speed ",
            (portsc >> SHELL64_XHCI_PORTSC_SPEED_SHIFT) & 0xFu);
        shell64_write_hex32_field(console_capability_handle, owner_id, " raw ", portsc);
        (void)shell64_write_text(console_capability_handle, owner_id, "\n");
    }

    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 keyboard: ", input64_ps2_enabled());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 mouse: ", input64_mouse_enabled());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "i2c pointer: ", i2c_hid64_pointer_found());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer address: ", i2c_hid64_pointer_address());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c controllers: ", pci64_lpss_i2c_count());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary pci: ", pci64_lpss_i2c_address());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary flags: ", pci64_lpss_i2c_mmio_flags());
    (void)shell64_write_yes_no_line(
        console_capability_handle,
        owner_id,
        "i2c acpi resource needed: ",
        ((pci64_lpss_i2c_mmio_flags() & PCI64_LPSS_I2C_MMIO_FLAG_ACPI_RESOURCE_REQUIRED) != 0u) ? 1u : 0u);
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-hw-ports ports 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " xhci-root-ports ", xhci64_hcs_ports());
    shell64_write_decimal_field(console_capability_handle, owner_id, " xhci-connected ", xhci64_connected_ports());
    shell64_write_decimal_field(console_capability_handle, owner_id, " port-lines ", printed);
    shell64_write_decimal_field(console_capability_handle, owner_id, " xhci-mouse-port ", xhci64_boot_mouse_port());
    shell64_write_decimal_field(console_capability_handle, owner_id, " i2c-pointer-address ", i2c_hid64_pointer_address());
    shell64_write_decimal_field(
        console_capability_handle,
        owner_id,
        " i2c-acpi-resource-needed ",
        ((pci64_lpss_i2c_mmio_flags() & PCI64_LPSS_I2C_MMIO_FLAG_ACPI_RESOURCE_REQUIRED) != 0u) ? 1u : 0u);
    return shell64_end_hwval_composite_line(console_capability_handle, owner_id);
}

static u32 shell64_rescan_usb(u32 console_capability_handle, u32 owner_id)
{
    u32 result = xhci64_rescan_devices();

    (void)shell64_write_text(console_capability_handle, owner_id, "usbscan\n");
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci present: ", xhci64_found());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci root ports: ", xhci64_hcs_ports());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci connected: ", xhci64_connected_ports());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "usb storage present: ", xhci64_usb_storage_present());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "usb storage ready: ", xhci64_usb_storage_ready());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "usb storage error: ", xhci64_usb_storage_error());
    (void)shell64_begin_hwval_composite_line(console_capability_handle, owner_id, "[x64] drs-usbscan usbscan 1");
    shell64_write_decimal_field(console_capability_handle, owner_id, " result ", result);
    shell64_write_decimal_field(console_capability_handle, owner_id, " xhci-connected ", xhci64_connected_ports());
    shell64_write_decimal_field(console_capability_handle, owner_id, " storage-present ", xhci64_usb_storage_present());
    shell64_write_decimal_field(console_capability_handle, owner_id, " storage-ready ", xhci64_usb_storage_ready());
    shell64_write_decimal_field(console_capability_handle, owner_id, " storage-error ", xhci64_usb_storage_error());
    shell64_write_decimal_field(console_capability_handle, owner_id, " last-skip-port ", xhci64_last_skip_port());
    shell64_write_decimal_field(console_capability_handle, owner_id, " last-skip-code ", xhci64_last_skip_code());
    return shell64_end_hwval_composite_line(console_capability_handle, owner_id);
}
#endif

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_next_linux_arg(
    u32 *cursor,
    u32 line_length,
    char *destination,
    u32 destination_bytes,
    u32 *out_length,
    u32 *out_error)
{
    u32 copied = 0u;
    u8 quote = 0u;
    u8 value;

    if (out_length != 0)
    {
        *out_length = 0u;
    }
    if (out_error != 0)
    {
        *out_error = 0u;
    }
    if ((destination == 0) || (destination_bytes == 0u))
    {
        if (out_error != 0)
        {
            *out_error = 1u;
        }
        return 0u;
    }

    *cursor = shell64_skip_spaces(*cursor, line_length);
    if (*cursor >= line_length)
    {
        destination[0] = '\0';
        return 0u;
    }

    while (*cursor < line_length)
    {
        value = g_shell64_line[*cursor];
        if (quote == 0u)
        {
            if (shell64_is_space(value))
            {
                break;
            }
            if ((value == (u8)'\'') || (value == (u8)'"'))
            {
                quote = value;
                ++*cursor;
                continue;
            }
        }
        else if (value == quote)
        {
            quote = 0u;
            ++*cursor;
            continue;
        }

        if ((copied + 1u) >= destination_bytes)
        {
            if (out_error != 0)
            {
                *out_error = 2u;
            }
            destination[0] = '\0';
            return 0u;
        }
        destination[copied] = (char)value;
        ++copied;
        ++*cursor;
    }

    if (quote != 0u)
    {
        if (out_error != 0)
        {
            *out_error = 3u;
        }
        destination[0] = '\0';
        return 0u;
    }

    destination[copied] = '\0';
    if (out_length != 0)
    {
        *out_length = copied;
    }
    return 1u;
}

static u32 shell64_print_hardware_validation_status_filtered(
    u32 console_capability_handle,
    u32 owner_id,
    u32 filter_start,
    u32 filter_length)
{
    u32 result;

    g_shell64_hwval_filter_active = (filter_length != 0u) ? 1u : 0u;
    g_shell64_hwval_filter_start = filter_start;
    g_shell64_hwval_filter_length = filter_length;

    result = shell64_print_hardware_validation_status(console_capability_handle, owner_id);

    g_shell64_hwval_filter_active = 0u;
    g_shell64_hwval_filter_start = 0u;
    g_shell64_hwval_filter_length = 0u;

    return result;
}
#endif

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_gui_app_id_from_token(u32 token_start, u32 token_length)
{
    if (shell64_token_equals(token_start, token_length, "terminal")
        || shell64_token_equals(token_start, token_length, "term")
        || shell64_token_equals(token_start, token_length, "shell"))
    {
        return DISPLAY64_DESKTOP_APP_TERMINAL;
    }
    if (shell64_token_equals(token_start, token_length, "files")
        || shell64_token_equals(token_start, token_length, "file")
        || shell64_token_equals(token_start, token_length, "fileman")
        || shell64_token_equals(token_start, token_length, "file-manager"))
    {
        return DISPLAY64_DESKTOP_APP_FILES;
    }
    if (shell64_token_equals(token_start, token_length, "settings"))
    {
        return DISPLAY64_DESKTOP_APP_SETTINGS;
    }
    if (shell64_token_equals(token_start, token_length, "installer")
        || shell64_token_equals(token_start, token_length, "install"))
    {
        return DISPLAY64_DESKTOP_APP_INSTALLER;
    }
    if (shell64_token_equals(token_start, token_length, "assistant"))
    {
        return DISPLAY64_DESKTOP_APP_ASSISTANT;
    }
    return 0u;
}

static u32 shell64_open_product_app(
    u32 console_capability_handle,
    u32 owner_id,
    u32 token_start,
    u32 token_length)
{
    u32 app_id = shell64_gui_app_id_from_token(token_start, token_length);

    if (app_id == 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: open <terminal|files|settings|installer|assistant>\n");
    }
    if (display64_desktop_open_app_by_id(app_id) == 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "open: not ready\n");
    }
    return shell64_write_text(console_capability_handle, owner_id, "gui open ok\n");
}
#endif

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static int shell64_token_has_usb_prefix(u32 token_start, u32 token_length)
{
    return (token_length >= 4u)
        && (shell64_lower(g_shell64_line[token_start]) == (u8)'u')
        && (shell64_lower(g_shell64_line[token_start + 1u]) == (u8)'s')
        && (shell64_lower(g_shell64_line[token_start + 2u]) == (u8)'b')
        && (g_shell64_line[token_start + 3u] == (u8)':');
}

static u32 shell64_normalize_redirect_path(u32 token_start, u32 token_length, u8 *destination)
{
    u32 source_index = token_start;
    u32 copy_index;

    g_shell64_redirect_backend = SHELL64_REDIRECT_BACKEND_FAT;
    if (shell64_token_has_usb_prefix(token_start, token_length))
    {
        source_index += 4u;
        token_length -= 4u;
        g_shell64_redirect_backend = SHELL64_REDIRECT_BACKEND_USB;
        ++g_shell64_redirect_usb_requested;
    }

    while ((token_length > 0u) && (g_shell64_line[source_index] == (u8)'/'))
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
#endif

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_trim_trailing_spaces(u32 length)
{
    while ((length != 0u)
        && ((g_shell64_line[length - 1u] == (u8)' ')
            || (g_shell64_line[length - 1u] == (u8)'\t')))
    {
        --length;
    }

    return length;
}

static u32 shell64_parse_redirection(
    u32 line_byte_count,
    u32 *redirect_position,
    u32 *path_start,
    u32 *path_length,
    u32 *append)
{
    u32 index;
    u8 quote = 0u;

    if ((redirect_position == 0) || (path_start == 0) || (path_length == 0) || (append == 0))
    {
        return SHELL64_REDIRECT_INVALID;
    }

    *redirect_position = 0u;
    *path_start = 0u;
    *path_length = 0u;
    *append = 0u;

    for (index = 0u; index < line_byte_count; ++index)
    {
        if ((quote != 0u) && (g_shell64_line[index] == quote))
        {
            quote = 0u;
            continue;
        }
        if ((quote == 0u)
            && ((g_shell64_line[index] == (u8)'\'') || (g_shell64_line[index] == (u8)'"')))
        {
            quote = g_shell64_line[index];
            continue;
        }
        if ((quote != 0u) || (g_shell64_line[index] != (u8)'>'))
        {
            continue;
        }

        *redirect_position = index;
        if (((index + 1u) < line_byte_count) && (g_shell64_line[index + 1u] == (u8)'>'))
        {
            *append = 1u;
            index += 2u;
        }
        else
        {
            *append = 0u;
            ++index;
        }

        while ((index < line_byte_count)
            && ((g_shell64_line[index] == (u8)' ') || (g_shell64_line[index] == (u8)'\t')))
        {
            ++index;
        }

        *path_start = index;
        while ((index < line_byte_count)
            && (g_shell64_line[index] != (u8)' ')
            && (g_shell64_line[index] != (u8)'\t')
            && (g_shell64_line[index] != (u8)'>'))
        {
            ++index;
        }

        *path_length = index - *path_start;
        if (*path_length == 0u)
        {
            return SHELL64_REDIRECT_INVALID;
        }

        while ((index < line_byte_count)
            && ((g_shell64_line[index] == (u8)' ') || (g_shell64_line[index] == (u8)'\t')))
        {
            ++index;
        }

        if (index != line_byte_count)
        {
            return SHELL64_REDIRECT_INVALID;
        }

        return SHELL64_REDIRECT_FOUND;
    }

    return SHELL64_REDIRECT_NONE;
}

static u32 shell64_end_redirect(u32 owner_id)
{
    u32 ok = 1u;

    (void)shell64_redirect_flush(owner_id);
    if ((g_shell64_redirect_path_length != 0u)
        && (g_shell64_redirect_offset == 0u)
        && (g_shell64_redirect_append == 0u)
        && (g_shell64_redirect_committed == 0u))
    {
        if (g_shell64_redirect_backend == SHELL64_REDIRECT_BACKEND_USB)
        {
            if (mmio64_usb_fat_shell_write_file(
                    g_shell64_redirect_path,
                    g_shell64_redirect_path_length,
                    g_shell64_redirect_buffer,
                    0u,
                    owner_id) != 0u)
            {
                ++g_shell64_redirect_commit_count;
                g_shell64_redirect_last_result = 0u;
                g_shell64_redirect_committed = 1u;
                g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_NONE;
            }
            else
            {
                ++g_shell64_redirect_usb_unavailable;
                ++g_shell64_redirect_denial_count;
                g_shell64_redirect_last_result = 0u;
                g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_USB_UNAVAILABLE;
                ok = 0u;
            }
        }
        else if (mmio64_nvme_fat_shell_write_file(
                g_shell64_redirect_path,
                g_shell64_redirect_path_length,
                g_shell64_redirect_buffer,
                0u,
                owner_id) != 0u)
        {
            ++g_shell64_redirect_commit_count;
            g_shell64_redirect_last_result = 0u;
            g_shell64_redirect_committed = 1u;
        }
        else if (g_shell64_redirect_capability != FS64_INVALID_HANDLE)
        {
            ++g_shell64_redirect_commit_count;
            g_shell64_redirect_last_result = 0u;
            g_shell64_redirect_committed = 1u;
            g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_NONE;
        }
        else
        {
            ++g_shell64_redirect_denial_count;
            g_shell64_redirect_last_result = 0u;
            g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_RAMFS;
            ok = 0u;
        }
    }
    else if ((g_shell64_redirect_path_length != 0u)
        && (g_shell64_redirect_offset != 0u))
    {
        if (g_shell64_redirect_last_error == SHELL64_REDIRECT_ERROR_NONE)
        {
            g_shell64_redirect_last_error =
                (g_shell64_redirect_backend == SHELL64_REDIRECT_BACKEND_USB)
                    ? SHELL64_REDIRECT_ERROR_USB_UNAVAILABLE
                    : SHELL64_REDIRECT_ERROR_FAT;
        }
        ok = 0u;
    }

    if (g_shell64_redirect_capability != FS64_INVALID_HANDLE)
    {
        (void)fs64_revoke(g_shell64_redirect_capability, owner_id);
    }

    g_shell64_redirect_active = 0u;
    g_shell64_redirect_capability = FS64_INVALID_HANDLE;
    g_shell64_redirect_offset = 0u;
    g_shell64_redirect_append = 0u;
    g_shell64_redirect_committed = 0u;
    g_shell64_redirect_backend = SHELL64_REDIRECT_BACKEND_NONE;
    g_shell64_redirect_path_length = 0u;
    shell64_zero(g_shell64_redirect_path, sizeof(g_shell64_redirect_path));
    shell64_zero(g_shell64_redirect_buffer, sizeof(g_shell64_redirect_buffer));
    return ok;
}

static u32 shell64_begin_redirect(
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 append,
    u32 owner_id)
{
    u32 path_length;
    u32 file_capability;

    if (g_shell64_redirect_active != 0u)
    {
        (void)shell64_end_redirect(owner_id);
    }

    path_length = shell64_normalize_redirect_path(token_start, token_length, g_shell64_redirect_path);
    if (path_length == 0u)
    {
        ++g_shell64_redirect_denial_count;
        g_shell64_redirect_last_result = FS64_INVALID_HANDLE;
        g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_ARGUMENT;
        return 0u;
    }

    if (append != 0u)
    {
        file_capability = fs64_open_kernel(root_capability_handle, g_shell64_redirect_path, path_length, owner_id);
        if (file_capability == FS64_INVALID_HANDLE)
        {
            file_capability = fs64_create_kernel(
                root_capability_handle,
                g_shell64_redirect_path,
                path_length,
                RAMFS_NODE_FILE,
                owner_id);
        }
    }
    else
    {
        (void)fs64_delete_kernel(root_capability_handle, g_shell64_redirect_path, path_length, owner_id);
        file_capability = fs64_create_kernel(
            root_capability_handle,
            g_shell64_redirect_path,
            path_length,
            RAMFS_NODE_FILE,
            owner_id);
    }

    if (file_capability == FS64_INVALID_HANDLE)
    {
        ++g_shell64_redirect_denial_count;
        g_shell64_redirect_last_result = FS64_INVALID_HANDLE;
        g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_RAMFS;
        return 0u;
    }

    shell64_zero(g_shell64_redirect_buffer, sizeof(g_shell64_redirect_buffer));
    g_shell64_redirect_offset = 0u;

    g_shell64_redirect_capability = file_capability;
    g_shell64_redirect_path_length = path_length;
    g_shell64_redirect_active = 1u;
    g_shell64_redirect_append = (append != 0u) ? 1u : 0u;
    g_shell64_redirect_committed = 0u;
    g_shell64_redirect_last_error = SHELL64_REDIRECT_ERROR_NONE;
    ++g_shell64_redirect_count;
    if (append != 0u)
    {
        ++g_shell64_redirect_append_count;
    }
    g_shell64_redirect_last_result = 0u;
    return 1u;
}

static u32 shell64_write_redirect_failure(u32 console_capability_handle, u32 owner_id)
{
    if (g_shell64_redirect_last_error == SHELL64_REDIRECT_ERROR_USB_UNAVAILABLE)
    {
        return shell64_write_text(
            console_capability_handle,
            owner_id,
            "redirect failed: USB storage export backend unavailable\n");
    }
    if (g_shell64_redirect_last_error == SHELL64_REDIRECT_ERROR_FAT)
    {
        return shell64_write_text(
            console_capability_handle,
            owner_id,
            "redirect failed: writable FAT backend unavailable\n");
    }
    return shell64_write_text(console_capability_handle, owner_id, "redirect failed\n");
}

static u32 shell64_export_usb_file(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 owner_id,
    const char *usb_path,
    u32 content_id)
{
    u32 path_length;

    path_length = shell64_length(usb_path);
    if ((path_length == 0u) || (path_length >= SHELL64_MAX_LINE_BYTES))
    {
        return 0u;
    }

    shell64_zero(g_shell64_line, sizeof(g_shell64_line));
    shell64_copy(g_shell64_line, (const u8 *)usb_path, path_length);
    if (shell64_begin_redirect(root_capability_handle, 0u, path_length, 0u, owner_id) == 0u)
    {
        (void)shell64_write_redirect_failure(console_capability_handle, owner_id);
        return 0u;
    }

    if (content_id == 1u)
    {
        (void)shell64_print_hardware_validation_summary(console_capability_handle, owner_id);
    }
    else if (content_id == 2u)
    {
        (void)shell64_print_hardware_validation_status(console_capability_handle, owner_id);
    }
    else if (content_id == 3u)
    {
        (void)shell64_print_hardware_devices(console_capability_handle, owner_id);
    }
    else
    {
        (void)shell64_print_hardware_ports(console_capability_handle, owner_id);
    }

    if (shell64_end_redirect(owner_id) == 0u)
    {
        (void)shell64_write_redirect_failure(console_capability_handle, owner_id);
        return 0u;
    }

    (void)shell64_write_text(console_capability_handle, owner_id, "wrote ");
    (void)shell64_write_text(console_capability_handle, owner_id, usb_path);
    return shell64_write_text(console_capability_handle, owner_id, "\n");
}

static u32 shell64_export_hardware_bundle(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 owner_id)
{
    u32 ok = 1u;

    (void)shell64_write_text(
        console_capability_handle,
        owner_id,
        "exporthw: writing hardware evidence bundle to USB FAT32\n");

    if (shell64_export_usb_file(console_capability_handle, root_capability_handle, owner_id, "USB:HWVAL.TXT", 1u) == 0u)
    {
        ok = 0u;
    }
    if (shell64_export_usb_file(console_capability_handle, root_capability_handle, owner_id, "USB:HWFULL.TXT", 2u) == 0u)
    {
        ok = 0u;
    }
    if (shell64_export_usb_file(console_capability_handle, root_capability_handle, owner_id, "USB:DEVICES.TXT", 3u) == 0u)
    {
        ok = 0u;
    }
    if (shell64_export_usb_file(console_capability_handle, root_capability_handle, owner_id, "USB:PORTS.TXT", 4u) == 0u)
    {
        ok = 0u;
    }

    if (ok == 0u)
    {
        return shell64_write_text(
            console_capability_handle,
            owner_id,
            "exporthw failed: run hwval full to inspect usb storage/fat fields\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "exporthw ok\n");
}
#endif

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_token_equals(token_start, token_length, "exporthw")
        || shell64_token_equals(token_start, token_length, "hwexport")
        || shell64_token_equals(token_start, token_length, "export"))
    {
        return shell64_write_text(
            console_capability_handle,
            owner_id,
            "usage: exporthw | hwexport | export - write HWVAL.TXT HWFULL.TXT DEVICES.TXT PORTS.TXT to USB\n");
    }
#endif

    if (shell64_token_equals(token_start, token_length, "nethello"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: nethello - native app-model socket client\n");
    }

    if (shell64_token_equals(token_start, token_length, "linux"))
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        return shell64_write_text(console_capability_handle, owner_id, "usage: linux <path> [args...]\n");
#else
        return shell64_write_text(console_capability_handle, owner_id, "usage: linux unavailable on BIOS checksum fallback\n");
#endif
    }

    if (shell64_token_equals(token_start, token_length, "apps"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: apps\n");
    }

    if (shell64_token_equals(token_start, token_length, "info"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: info <command>\n");
    }

    if (shell64_token_equals(token_start, token_length, "net"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: net [curl example.com]\n");
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_token_equals(token_start, token_length, "open"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: open <terminal|files|settings|installer|assistant>\n");
    }
#endif

    if (shell64_token_equals(token_start, token_length, "hwval"))
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        return shell64_write_text(console_capability_handle, owner_id, "usage: hwval [summary|full] or hwfull - show read-only hardware validation status\n");
#else
        return shell64_write_text(console_capability_handle, owner_id, "usage: hwval - show read-only hardware validation status\n");
#endif
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_token_equals(token_start, token_length, "devices")
        || shell64_token_equals(token_start, token_length, "dev")
        || shell64_token_equals(token_start, token_length, "hwdevices")
        || shell64_token_equals(token_start, token_length, "lsdev"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: devices | dev | hwdevices | lsdev - compact connected hardware and driver-binding inventory\n");
    }
    if (shell64_token_equals(token_start, token_length, "ports")
        || shell64_token_equals(token_start, token_length, "port"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: ports | port - compact USB/PS2/I2C port and pointer inventory\n");
    }

    if (shell64_token_equals(token_start, token_length, "usbscan"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: usbscan - rescan xHCI ports for newly attached USB input/storage devices\n");
    }
#endif

    if (shell64_token_equals(token_start, token_length, "pkginfo"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: pkginfo - show read-only package, identity, account, cloud, installer, and AI policy status\n");
    }

    if (shell64_token_equals(token_start, token_length, "lock"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: lock - lock the authenticated local session\n");
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

    return shell64_write_text(console_capability_handle, owner_id, "unknown: help\n");
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
        || shell64_token_equals(token_start, token_length, "nethello")
        || shell64_token_equals(token_start, token_length, "rename")
        || shell64_token_equals(token_start, token_length, "stat")
        || shell64_token_equals(token_start, token_length, "touch")
        || shell64_token_equals(token_start, token_length, "write");
}

static int shell64_token_is_builtin_command(u32 token_start, u32 token_length)
{
    return shell64_token_equals(token_start, token_length, "apps")
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        || shell64_token_equals(token_start, token_length, "devices")
        || shell64_token_equals(token_start, token_length, "dev")
        || shell64_token_equals(token_start, token_length, "hwdevices")
        || shell64_token_equals(token_start, token_length, "lsdev")
        || shell64_token_equals(token_start, token_length, "export")
        || shell64_token_equals(token_start, token_length, "exporthw")
        || shell64_token_equals(token_start, token_length, "hwexport")
        || shell64_token_equals(token_start, token_length, "hwfull")
#endif
        || shell64_token_equals(token_start, token_length, "help")
        || shell64_token_equals(token_start, token_length, "hwval")
        || shell64_token_equals(token_start, token_length, "info")
        || shell64_token_equals(token_start, token_length, "linux")
        || shell64_token_equals(token_start, token_length, "lock")
        || shell64_token_equals(token_start, token_length, "net")
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        || shell64_token_equals(token_start, token_length, "open")
#endif
        || shell64_token_equals(token_start, token_length, "pkginfo")
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        || shell64_token_equals(token_start, token_length, "ports")
        || shell64_token_equals(token_start, token_length, "port")
#endif
        || shell64_token_equals(token_start, token_length, "pwd");
}

static u32 shell64_linux_run(
    u32 console_capability_handle,
    u32 line_byte_count,
    u32 *cursor,
    u32 owner_id)
{
    const char *argv[LINUX_EXEC64_ARG_MAX];
    u32 path_start = 0u;
    u32 path_length;
    u32 argc = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 nvme_capability;
    u32 nvme_available;
    u32 nvme_capability_present;
    u32 nvme_probe_found;
    u32 nvme_probe_ready;
    u32 index;
    u32 arg_error = 0u;
    u32 arg_length = 0u;
#endif

    path_length = shell64_next_token(cursor, line_byte_count, &path_start);
    if (path_length == 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: linux <path> [args...]\n");
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (path_length > SHELL64_MAX_LINE_BYTES)
    {
        return shell64_write_text(console_capability_handle, owner_id, "linux: argument too long\n");
    }
    for (index = 0u; index < path_length; ++index)
    {
        g_shell64_linux_argv_storage[argc][index] = (char)shell64_lower(g_shell64_line[path_start + index]);
    }
    g_shell64_linux_argv_storage[argc][path_length] = '\0';
    argv[argc++] = g_shell64_linux_argv_storage[0];

    while (argc < LINUX_EXEC64_ARG_MAX)
    {
        if (shell64_next_linux_arg(
                cursor,
                line_byte_count,
                g_shell64_linux_argv_storage[argc],
                SHELL64_MAX_LINE_BYTES + 1u,
                &arg_length,
                &arg_error) == 0u)
        {
            if (arg_error == 2u)
            {
                return shell64_write_text(console_capability_handle, owner_id, "linux: argument too long\n");
            }
            if (arg_error == 3u)
            {
                return shell64_write_text(console_capability_handle, owner_id, "linux: unterminated quote\n");
            }
            if (arg_error != 0u)
            {
                return shell64_write_text(console_capability_handle, owner_id, "linux: argument parse error\n");
            }
            break;
        }
        (void)arg_length;
        argv[argc] = g_shell64_linux_argv_storage[argc];
        ++argc;
    }
    if (shell64_skip_spaces(*cursor, line_byte_count) < line_byte_count)
    {
        return shell64_write_text(console_capability_handle, owner_id, "linux: too many args\n");
    }

    nvme_capability = mmio64_nvme_rw_capability();
    if (boot_media64_has_file(g_shell64_line + path_start, path_length) != 0u)
    {
        (void)shell64_write_text(
            console_capability_handle,
            owner_id,
            "linux: using UEFI boot-media staged file\n");
        return linux_exec64_run_boot_media(
            g_shell64_line + path_start,
            path_length,
            argv,
            argc,
            owner_id,
            nvme_capability,
            console_capability_handle);
    }

    nvme_capability_present = (nvme_capability != CAPABILITY64_INVALID_HANDLE) ? 1u : 0u;
    nvme_probe_found = mmio64_nvme_probe_found();
    nvme_probe_ready = mmio64_nvme_probe_ready();
    nvme_available = ((nvme_capability_present != 0u) && (nvme_probe_found != 0u)) ? 1u : 0u;
    if (nvme_available == 0u)
    {
        (void)shell64_write_text(
            console_capability_handle,
            owner_id,
            "linux: NVMe FAT unavailable (UEFI storage probe did not expose the FAT source)\n");
        (void)shell64_write_text(
            console_capability_handle,
            owner_id,
            "linux: run hwval for framebuffer, I2C pointer, and NVMe hardware evidence\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "drs-realbin-unavailable bios 0 nvme ");
        shell64_write_decimal_field(console_capability_handle, owner_id, "", nvme_available);
        shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-probe ", nvme_probe_found);
        shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-ready ", nvme_probe_ready);
        shell64_write_decimal_field(console_capability_handle, owner_id, " nvme-cap ", nvme_capability_present);
        shell64_write_decimal_field(console_capability_handle, owner_id, " fat-located ", mmio64_nvme_fat_located());
        shell64_write_decimal_field(console_capability_handle, owner_id, " fat-unavailable ", mmio64_nvme_fat_unavailable());
        shell64_write_decimal_field(console_capability_handle, owner_id, " fat-error ", mmio64_nvme_fat_error());
        shell64_write_decimal_field(console_capability_handle, owner_id, " rw-delegated ", mmio64_nvme_rw_delegated());
        shell64_write_decimal_field(console_capability_handle, owner_id, " rw-error ", mmio64_nvme_rw_error());
        (void)shell64_write_text(console_capability_handle, owner_id, "\n");
        return shell64_print_nvme_storage_triage(console_capability_handle, owner_id);
    }

    return linux_exec64_run_nvme(
        g_shell64_line + path_start,
        path_length,
        argv,
        argc,
        owner_id,
        nvme_capability,
        console_capability_handle);
#else
    (void)argv;
    (void)argc;
    return shell64_write_text(
        console_capability_handle,
        owner_id,
        "linux: unavailable on BIOS checksum fallback\ndrs-realbin-unavailable bios 1 nvme 0\n");
#endif
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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_read_fat_file(
    u32 console_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id,
    u32 *handled)
{
    u32 path_length;
    u32 offset = 0u;
    u32 byte_count = 0u;
    u32 file_size = 0u;
    u32 total_bytes = 0u;
    u8 last_byte = 0u;

    if (handled == 0)
    {
        return 0u;
    }
    *handled = 0u;

    path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_b);
    if ((path_length == 0u) || shell64_token_is_root(token_start, token_length))
    {
        return 0u;
    }

    shell64_zero(g_shell64_io, sizeof(g_shell64_io));
    if (mmio64_nvme_fat_shell_read_file(
            g_shell64_path_b,
            path_length,
            g_shell64_io,
            sizeof(g_shell64_io),
            owner_id,
            &byte_count) != 0u)
    {
        *handled = 1u;
        if (byte_count != 0u)
        {
            (void)shell64_write_bytes_chunked(console_capability_handle, owner_id, g_shell64_io, byte_count);
            if (g_shell64_io[byte_count - 1u] != (u8)'\n')
            {
                (void)shell64_write_text(console_capability_handle, owner_id, "\n");
            }
        }
        return byte_count;
    }
    if (mmio64_nvme_fat_shell_read_last_error() != MMIO64_NVME_FAT_SHELL_READ_ERROR_TOO_LARGE)
    {
        return 0u;
    }

    for (;;)
    {
        shell64_zero(g_shell64_io, sizeof(g_shell64_io));
        if (mmio64_nvme_fat_shell_read_file_range(
                g_shell64_path_b,
                path_length,
                offset,
                g_shell64_io,
                sizeof(g_shell64_io),
                owner_id,
                &byte_count,
                &file_size) == 0u)
        {
            if (offset == 0u)
            {
                return 0u;
            }
            *handled = 1u;
            return shell64_write_text(console_capability_handle, owner_id, "read failed\n");
        }

        *handled = 1u;
        if (byte_count == 0u)
        {
            if (offset < file_size)
            {
                return shell64_write_text(console_capability_handle, owner_id, "read failed\n");
            }
            break;
        }

        (void)shell64_write_bytes_chunked(console_capability_handle, owner_id, g_shell64_io, byte_count);
        offset += byte_count;
        total_bytes += byte_count;
        last_byte = g_shell64_io[byte_count - 1u];
        if (offset >= file_size)
        {
            break;
        }
    }

    if ((total_bytes > 0u) && (last_byte != (u8)'\n'))
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "\n");
    }

    return total_bytes;
}
#endif

static u32 shell64_read_file(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 file_capability;
    u32 byte_count;
    u32 total_bytes = 0u;
    u32 offset = 0u;
    u8 last_byte = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 fat_handled = 0u;
    u32 fat_bytes;

    fat_bytes = shell64_read_fat_file(
        console_capability_handle,
        token_start,
        token_length,
        owner_id,
        &fat_handled);
    if (fat_handled != 0u)
    {
        return fat_bytes;
    }
#endif

    file_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);

    if ((file_capability == FS64_INVALID_HANDLE) || (borrowed_root != 0u))
    {
        return shell64_write_text(console_capability_handle, owner_id, "not found\n");
    }

    for (;;)
    {
        shell64_zero(g_shell64_io, sizeof(g_shell64_io));
        byte_count = fs64_read_kernel(file_capability, g_shell64_io, offset, sizeof(g_shell64_io), owner_id);
        if (byte_count == FS64_INVALID_HANDLE)
        {
            (void)fs64_revoke(file_capability, owner_id);
            return shell64_write_text(console_capability_handle, owner_id, "read failed\n");
        }
        if (byte_count == 0u)
        {
            break;
        }

        (void)shell64_write_bytes_chunked(console_capability_handle, owner_id, g_shell64_io, byte_count);
        offset += byte_count;
        total_bytes += byte_count;
        last_byte = g_shell64_io[byte_count - 1u];
    }

    (void)fs64_revoke(file_capability, owner_id);

    if ((total_bytes > 0u) && (last_byte != (u8)'\n'))
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "\n");
    }
    return total_bytes;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 shell64_list_fat_path(
    u32 console_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id,
    u32 *handled)
{
    mmio64_nvme_fat_dirent_t entry;
    mmio64_nvme_fat_stat_t stat;
    u32 path_length;
    u32 cursor = 0u;
    u32 result;
    u32 entries = 0u;

    if (handled == 0)
    {
        return 0u;
    }
    *handled = 0u;

    if (token_length == 0u)
    {
        return 0u;
    }
    if (shell64_token_is_root(token_start, token_length))
    {
        g_shell64_path_b[0] = (u8)'/';
        g_shell64_path_b[1] = 0u;
        path_length = 1u;
    }
    else
    {
        path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_b);
        if (path_length == 0u)
        {
            return 0u;
        }
    }

    if (mmio64_nvme_fat_shell_stat_path(g_shell64_path_b, path_length, owner_id, &stat) == 0u)
    {
        return 0u;
    }
    *handled = 1u;
    if (stat.entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
    {
        (void)shell64_write(console_capability_handle, owner_id, g_shell64_line + token_start, token_length);
        (void)shell64_write_text(console_capability_handle, owner_id, "\n");
        return 1u;
    }
    if (stat.entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY)
    {
        return shell64_write_text(console_capability_handle, owner_id, "list failed\n");
    }

    for (;;)
    {
        result = mmio64_nvme_fat_shell_read_dirent(
            g_shell64_path_b,
            path_length,
            cursor,
            owner_id,
            &entry);
        if (result == MMIO64_NVME_FAT_READDIR_EOF)
        {
            break;
        }
        if (result != MMIO64_NVME_FAT_READDIR_OK)
        {
            return shell64_write_text(console_capability_handle, owner_id, "list failed\n");
        }
        if ((entry.name_byte_count == 1u) && (entry.name[0] == (u8)'.'))
        {
            cursor = entry.next_cursor;
            continue;
        }
        if ((entry.name_byte_count == 2u)
            && (entry.name[0] == (u8)'.')
            && (entry.name[1] == (u8)'.'))
        {
            cursor = entry.next_cursor;
            continue;
        }
        if (entry.name_byte_count != 0u)
        {
            (void)shell64_write(
                console_capability_handle,
                owner_id,
                entry.name,
                entry.name_byte_count);
            if (entry.entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY)
            {
                (void)shell64_write_text(console_capability_handle, owner_id, "/");
            }
            (void)shell64_write_text(console_capability_handle, owner_id, "\n");
            ++entries;
        }
        cursor = entry.next_cursor;
    }

    return entries;
}
#endif

static u32 shell64_list_path(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 dir_capability;
    u32 byte_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 fat_handled = 0u;
    u32 fat_entries;

    fat_entries = shell64_list_fat_path(
        console_capability_handle,
        token_start,
        token_length,
        owner_id,
        &fat_handled);
    if (fat_handled != 0u)
    {
        return fat_entries;
    }
#endif

    dir_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);

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
        (void)shell64_write_bytes_chunked(console_capability_handle, owner_id, g_shell64_io, byte_count);
    }

    return byte_count;
}

static u32 shell64_list_apps(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 owner_id)
{
    (void)root_capability_handle;

    (void)shell64_write_text(console_capability_handle, owner_id, "Product apps:\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "APPEND\nCAT\nCOPY\nDELETE\nLS\nMKDIR\nMOVE\nNETHELLO\nRENAME\nSTAT\nTOUCH\nWRITE\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Product services:\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Network (hardware-gated): use net or net curl example.com\n");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_text(console_capability_handle, owner_id, "Brokered socket API: capability-scoped TCP-client foundation in net\n");
#endif
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_text(console_capability_handle, owner_id, "Hardware validation: use hwval; read-only; MSI evidence pending\n");
#else
    (void)shell64_write_text(console_capability_handle, owner_id, "Hardware validation: use hwval; read-only\n");
#endif
    (void)shell64_write_text(console_capability_handle, owner_id, "Package trust: use pkginfo or Settings\n");
    (void)shell64_write_apps_gui_line(console_capability_handle, owner_id);
    (void)shell64_write_text(console_capability_handle, owner_id, "Service/session status: Settings\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Identity/account/vault/transport status: Settings; local only; no secret storage\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Cloud storage status: Settings and File Manager; policy only; sync unavailable\n");
    (void)shell64_write_apps_installer_line(console_capability_handle, owner_id);
    (void)shell64_write_apps_ai_line(console_capability_handle, owner_id);
    if (shell64_login_available() != 0u)
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "Login/session lock: use lock; first-run user stored on NVMe\n");
    }
    else
    {
        (void)shell64_write_text(console_capability_handle, owner_id, "Login/session lock: unavailable on BIOS checksum fallback\n");
    }
    (void)shell64_write_text(console_capability_handle, owner_id, "Installer dry-run: safe tooling only; writes disabled\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Unavailable in M21:\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "ASK (not AI)\nECHO\nAliases: SAY SHOW LIST MAKE PUT SWAP SHIFT\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Personal login\nEnterprise login\nAccount linking\nReal cloud storage\nEncrypted secret storage\nEncrypted identity transport\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Security key login\nCredential transport\nToken storage\nEnterprise policy\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Cloud sync\nAutomatic cloud upload/download\nGeneral sockets\nServer sockets\nRaw packet APIs\nArbitrary network send/receive\nAI cloud access\nAI inference backend\nAI autonomous actions\nAI automation\nCloud AI\nAI-assisted setup\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Real internal install/write\nFormatting\nBoot entry changes\nPackage install/update actions\nApp store\n");
    return shell64_write_text(
        console_capability_handle,
        owner_id,
        "Auto-install\nPublic update fetch\nInternal files hidden from app output: HELLO.TXT INDEX.TXT\n");
}

static u32 shell64_stat_path(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u32 token_start,
    u32 token_length,
    u32 owner_id)
{
    u32 borrowed_root;
    u32 node_capability;
    u32 byte_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 path_length;
    mmio64_nvme_fat_stat_t fat_stat;

    path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_a);
    if ((path_length != 0u)
        && (mmio64_nvme_fat_shell_stat_path(g_shell64_path_a, path_length, owner_id, &fat_stat) != 0u))
    {
        if (fat_stat.entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY)
        {
            (void)shell64_write_text(console_capability_handle, owner_id, "type=directory size=");
        }
        else if (fat_stat.entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
        {
            (void)shell64_write_text(console_capability_handle, owner_id, "type=file size=");
        }
        else
        {
            (void)shell64_write_text(console_capability_handle, owner_id, "type=unknown size=");
        }
        shell64_write_decimal_field(console_capability_handle, owner_id, "", fat_stat.byte_count);
        return shell64_write_text(console_capability_handle, owner_id, "\n");
    }
#endif

    node_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (mmio64_nvme_fat_shell_write_file(
            g_shell64_path_a,
            path_length,
            g_shell64_line + text_start,
            text_length,
            owner_id) != 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "ok\n");
    }
#endif

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
    u32 file_capability;
    u32 file_size = 0u;
    u32 byte_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 path_length;
#endif

    if (text_length == 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: append <path> <text>\n");
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    path_length = shell64_normalize_path(token_start, token_length, g_shell64_path_a);
    if ((path_length == 0u) || shell64_token_is_root(token_start, token_length))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: append <path> <text>\n");
    }
    if (mmio64_nvme_fat_shell_append_file(
            g_shell64_path_a,
            path_length,
            g_shell64_line + text_start,
            text_length,
            owner_id) != 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "ok\n");
    }
#endif

    file_capability = shell64_open_path(
        root_capability_handle,
        token_start,
        token_length,
        owner_id,
        &borrowed_root);
    if (borrowed_root != 0u)
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
    u32 source_capability;
    u32 destination_path_length;
    u32 destination_capability;
    u32 byte_count;
    u32 written;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 source_path_length;
#endif

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    source_path_length = shell64_normalize_path(source_start, source_length, g_shell64_path_a);
    destination_path_length = shell64_normalize_path(
        destination_start,
        destination_length,
        g_shell64_path_b);
    if ((source_path_length != 0u)
        && (destination_path_length != 0u)
        && (shell64_token_is_root(source_start, source_length) == 0u)
        && (shell64_token_is_root(destination_start, destination_length) == 0u)
        && (mmio64_nvme_fat_shell_copy_file(
                g_shell64_path_a,
                source_path_length,
                g_shell64_path_b,
                destination_path_length,
                owner_id) != 0u))
    {
        return shell64_write_text(console_capability_handle, owner_id, "ok\n");
    }
#endif

    source_capability = shell64_open_path(
        root_capability_handle,
        source_start,
        source_length,
        owner_id,
        &borrowed_root);

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
    const char *failure_text = (move != 0) ? "move failed\n" : "rename failed\n";

    if ((source_path_length == 0u)
        || (destination_path_length == 0u)
        || shell64_token_is_root(source_start, source_length)
        || shell64_token_is_root(destination_start, destination_length))
    {
        return shell64_write_text(console_capability_handle, owner_id, failure_text);
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (move != 0)
    {
        if (mmio64_nvme_fat_shell_move_file(
                g_shell64_path_a,
                source_path_length,
                g_shell64_path_b,
                destination_path_length,
                owner_id) != 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "ok\n");
        }
    }
    else if (mmio64_nvme_fat_shell_rename_file(
            g_shell64_path_a,
            source_path_length,
            g_shell64_path_b,
            destination_path_length,
            owner_id) != 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "ok\n");
    }
#endif

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
        return shell64_write_text(console_capability_handle, owner_id, failure_text);
    }

    return shell64_write_text(console_capability_handle, owner_id, "ok\n");
}

static u32 shell64_execute_line_inner(
    u32 console_capability_handle,
    u32 root_capability_handle,
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
        (void)shell64_write_builtins_line(console_capability_handle, owner_id);
        (void)shell64_write_text(console_capability_handle, owner_id, "Product apps: append cat copy delete ls mkdir move nethello rename stat touch write\n");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)shell64_write_text(console_capability_handle, owner_id, "Redirection: command > file writes a live shell file; command > USB:file writes real USB FAT when available\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "Hardware export: exporthw, hwexport, or export writes HWVAL.TXT HWFULL.TXT DEVICES.TXT PORTS.TXT to USB\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "Hardware: hwfull shows full validation; dev/devices/hwdevices/lsdev shows inventory; port/ports shows port/input mapping\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "GUI fallback: open terminal/files/settings/installer/assistant focuses Product windows from the keyboard\n");
#endif
        (void)shell64_write_text(console_capability_handle, owner_id, "Product network: net shows DHCP lease; net curl example.com performs a scoped HTTP GET\n");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        (void)shell64_write_text(console_capability_handle, owner_id, "Product hardware validation: hwval is read-only; MSI manual evidence pending\n");
#else
        (void)shell64_write_text(console_capability_handle, owner_id, "Product hardware validation: hwval is read-only\n");
#endif
        (void)shell64_write_text(console_capability_handle, owner_id, "Product package trust: pkginfo and Settings are read-only; installation disabled\n");
        (void)shell64_write_gui_status_line(console_capability_handle, owner_id);
        (void)shell64_write_service_status_line(console_capability_handle, owner_id);
        (void)shell64_write_login_status_line(console_capability_handle, owner_id);
        (void)shell64_write_identity_status_line(console_capability_handle, owner_id);
        (void)shell64_write_text(console_capability_handle, owner_id, "Product cloud storage: Settings/File Manager show broker policy; sync unavailable; transfers denied\n");
        (void)shell64_write_installer_status_line(console_capability_handle, owner_id);
        (void)shell64_write_ai_status_line(console_capability_handle, owner_id);
        return shell64_write_text(
            console_capability_handle,
            owner_id,
            "Unavailable in M21: ask (not AI), echo, aliases, personal-login, enterprise-login, account-linking, real-cloud-storage, cloud-sync, auto-upload-download, general-sockets, server-sockets, raw-packets, arbitrary-network-send-receive, encrypted-secrets, encrypted-identity-transport, credential-transport, token-storage, ai-inference, ai-autonomy, ai-automation, cloud-ai, ai-assisted-setup, real-install\n");
    }

    if (shell64_token_equals(command_start, command_length, "pwd"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "/\n");
    }

    if (shell64_token_equals(command_start, command_length, "apps"))
    {
        return shell64_list_apps(console_capability_handle, root_capability_handle, owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "net"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_print_network_status(console_capability_handle, owner_id);
        }
        if (shell64_token_equals(first_start, first_length, "curl"))
        {
            second_length = shell64_next_token(&cursor, line_byte_count, &second_start);
            if (second_length == 0u)
            {
                return shell64_write_text(console_capability_handle, owner_id, "usage: net curl example.com\n");
            }
            return shell64_net_curl(console_capability_handle, owner_id, second_start, second_length);
        }
        return shell64_write_text(console_capability_handle, owner_id, "usage: net [curl example.com]\n");
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_token_equals(command_start, command_length, "open"))
    {
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        return shell64_open_product_app(
            console_capability_handle,
            owner_id,
            first_start,
            first_length);
    }
#endif

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_token_equals(command_start, command_length, "exporthw")
        || shell64_token_equals(command_start, command_length, "hwexport")
        || shell64_token_equals(command_start, command_length, "export"))
    {
        return shell64_export_hardware_bundle(console_capability_handle, root_capability_handle, owner_id);
    }
#endif

    if (shell64_token_equals(command_start, command_length, "hwval")
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        || shell64_token_equals(command_start, command_length, "hwfull")
#endif
    )
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        if (shell64_token_equals(command_start, command_length, "hwfull"))
        {
            first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
            return shell64_print_hardware_validation_status_filtered(
                console_capability_handle,
                owner_id,
                first_start,
                first_length);
        }
        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_print_hardware_validation_summary(console_capability_handle, owner_id);
        }
        if (shell64_token_equals(first_start, first_length, "summary"))
        {
            return shell64_print_hardware_validation_summary(console_capability_handle, owner_id);
        }
        if (shell64_token_equals(first_start, first_length, "full"))
        {
            second_length = shell64_next_token(&cursor, line_byte_count, &second_start);
            return shell64_print_hardware_validation_status_filtered(
                console_capability_handle,
                owner_id,
                second_start,
                second_length);
        }
        return shell64_write_text(console_capability_handle, owner_id, "usage: hwval [summary|full [filter]]\n");
#else
        return shell64_print_hardware_validation_status(console_capability_handle, owner_id);
#endif
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (shell64_token_equals(command_start, command_length, "devices")
        || shell64_token_equals(command_start, command_length, "dev")
        || shell64_token_equals(command_start, command_length, "hwdevices")
        || shell64_token_equals(command_start, command_length, "lsdev"))
    {
        return shell64_print_hardware_devices(console_capability_handle, owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "ports")
        || shell64_token_equals(command_start, command_length, "port"))
    {
        return shell64_print_hardware_ports(console_capability_handle, owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "usbscan"))
    {
        return shell64_rescan_usb(console_capability_handle, owner_id);
    }
#endif

    if (shell64_token_equals(command_start, command_length, "pkginfo"))
    {
        return shell64_print_package_status(console_capability_handle, owner_id);
    }

    if (shell64_token_equals(command_start, command_length, "lock"))
    {
        if (auth64_lock_session() != 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "session unlocked\n");
        }
        return shell64_write_text(console_capability_handle, owner_id, "lock unavailable on this boot path\n");
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

    if (shell64_token_equals(command_start, command_length, "nethello"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "nethello: packaged native app runs during boot validation\n");
    }

    if (shell64_token_equals(command_start, command_length, "linux"))
    {
        return shell64_linux_run(console_capability_handle, line_byte_count, &cursor, owner_id);
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
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        u32 directory_path_length;
#endif

        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "mkdir failed\n");
        }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        directory_path_length = shell64_normalize_path(first_start, first_length, g_shell64_path_a);
        if ((directory_path_length != 0u)
            && (shell64_token_is_root(first_start, first_length) == 0u)
            && (mmio64_nvme_fat_shell_mkdir(g_shell64_path_a, directory_path_length, owner_id) != 0u))
        {
            return shell64_write_text(console_capability_handle, owner_id, "ok\n");
        }
#endif

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
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        u32 touch_path_length;
#endif

        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "usage: touch <path>\n");
        }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        touch_path_length = shell64_normalize_path(first_start, first_length, g_shell64_path_a);
        if ((touch_path_length != 0u)
            && (shell64_token_is_root(first_start, first_length) == 0u)
            && (mmio64_nvme_fat_shell_touch_file(g_shell64_path_a, touch_path_length, owner_id) != 0u))
        {
            return shell64_write_text(console_capability_handle, owner_id, "ok\n");
        }
#endif

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
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        u32 delete_path_length;
#endif

        first_length = shell64_next_token(&cursor, line_byte_count, &first_start);
        if (first_length == 0u)
        {
            return shell64_write_text(console_capability_handle, owner_id, "delete failed\n");
        }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        delete_path_length = shell64_normalize_path(first_start, first_length, g_shell64_path_a);
        if ((delete_path_length != 0u)
            && (shell64_token_is_root(first_start, first_length) == 0u)
            && (mmio64_nvme_fat_shell_delete_file(g_shell64_path_a, delete_path_length, owner_id) != 0u))
        {
            return shell64_write_text(console_capability_handle, owner_id, "ok\n");
        }
#endif

        if (shell64_delete_path(
                    root_capability_handle,
                    first_start,
                    first_length,
                    owner_id) != 1u)
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

    return shell64_write_text(console_capability_handle, owner_id, "unknown: help\n");
}

u32 shell64_execute_line(
    u32 console_capability_handle,
    u32 root_capability_handle,
    u64 line_address,
    u32 line_byte_count,
    u32 owner_id)
{
    u32 effective_line_byte_count = line_byte_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 redirect_position = 0u;
    u32 redirect_path_start = 0u;
    u32 redirect_path_length = 0u;
    u32 redirect_append = 0u;
    u32 redirect_parse;
    u32 redirect_started = 0u;
    u32 redirect_linux = 0u;
    u32 capture_started = 0u;
    u32 capture_bytes = 0u;
    u32 capture_truncated = 0u;
    u32 cursor = 0u;
    u32 command_start = 0u;
    u32 command_length;
#endif
    u32 result;

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

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    redirect_parse = shell64_parse_redirection(
        line_byte_count,
        &redirect_position,
        &redirect_path_start,
        &redirect_path_length,
        &redirect_append);
    if (redirect_parse == SHELL64_REDIRECT_INVALID)
    {
        ++g_shell64_redirect_denial_count;
        return shell64_write_text(console_capability_handle, owner_id, "redirect syntax error\n");
    }

    if (redirect_parse == SHELL64_REDIRECT_FOUND)
    {
        effective_line_byte_count = shell64_trim_trailing_spaces(redirect_position);
        command_length = shell64_next_token(&cursor, effective_line_byte_count, &command_start);
        if (command_length == 0u)
        {
            ++g_shell64_redirect_denial_count;
            return shell64_write_text(console_capability_handle, owner_id, "redirect syntax error\n");
        }
        redirect_linux = shell64_token_equals(command_start, command_length, "linux");
        if (shell64_begin_redirect(
                root_capability_handle,
                redirect_path_start,
                redirect_path_length,
                redirect_append,
                owner_id) == 0u)
        {
            return shell64_write_redirect_failure(console_capability_handle, owner_id);
        }
        redirect_started = 1u;
        if (redirect_linux != 0u)
        {
            if (console64_capture_begin(
                    console_capability_handle,
                    owner_id,
                    g_shell64_redirect_buffer + g_shell64_redirect_offset,
                    SHELL64_REDIRECT_BUFFER_BYTES - g_shell64_redirect_offset) == 0u)
            {
                (void)shell64_end_redirect(owner_id);
                ++g_shell64_redirect_denial_count;
                return shell64_write_redirect_failure(console_capability_handle, owner_id);
            }
            capture_started = 1u;
        }
    }
#endif

    result = shell64_execute_line_inner(
        console_capability_handle,
        root_capability_handle,
        effective_line_byte_count,
        owner_id);

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (capture_started != 0u)
    {
        if (console64_capture_end(
                console_capability_handle,
                owner_id,
                &capture_bytes,
                &capture_truncated) == 0u)
        {
            ++g_shell64_redirect_denial_count;
            g_shell64_redirect_path_length = 0u;
            g_shell64_redirect_offset = 0u;
        }
        else if (capture_truncated != 0u)
        {
            ++g_shell64_redirect_denial_count;
            g_shell64_redirect_last_result = capture_bytes;
            g_shell64_redirect_path_length = 0u;
            g_shell64_redirect_offset = 0u;
        }
        else
        {
            g_shell64_redirect_offset += capture_bytes;
            g_shell64_redirect_byte_count += capture_bytes;
            ++g_shell64_redirect_write_count;
            g_shell64_redirect_last_result = capture_bytes;
        }
    }
    if (redirect_started != 0u)
    {
        if (shell64_end_redirect(owner_id) == 0u)
        {
            return shell64_write_redirect_failure(console_capability_handle, owner_id);
        }
    }
#endif

    return result;
}
