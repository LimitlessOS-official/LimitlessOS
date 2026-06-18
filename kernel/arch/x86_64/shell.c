#include "shell_x64.h"

#include "account_association_x64.h"
#include "ai_policy_x64.h"
#include "apic_x64.h"
#include "auth_x64.h"
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

static u8 g_shell64_line[SHELL64_MAX_LINE_BYTES + 1u];
static u8 g_shell64_path_a[SHELL64_MAX_PATH_BYTES];
static u8 g_shell64_path_b[SHELL64_MAX_PATH_BYTES];
static u8 g_shell64_pair[SHELL64_MAX_PATH_BYTES * 2u];
static u8 g_shell64_io[SHELL64_IO_BYTES];
static u8 g_shell64_stat[64u];
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static char g_shell64_linux_argv_storage[LINUX_EXEC64_ARG_MAX][SHELL64_MAX_LINE_BYTES + 1u];
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

static u32 shell64_login_available(void)
{
    return (auth64_login_screen() != 0u) && (auth64_auth_success() != 0u);
}

static u32 shell64_write_builtins_line(u32 console_capability_handle, u32 owner_id)
{
    if (shell64_login_available() != 0u)
    {
        return shell64_write_text(console_capability_handle, owner_id, "Builtins: apps help hwval info linux lock net pkginfo pwd\n");
    }

    return shell64_write_text(console_capability_handle, owner_id, "Builtins: apps help hwval info linux net pkginfo pwd\n");
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
    return shell64_write_text(console_capability_handle, owner_id, "Product installer UX: launcher/Settings show dry-run planning; writes/format/boot-entry disabled\n");
#else
    return shell64_write_text(console_capability_handle, owner_id, "Product installer UX: unavailable on BIOS checksum fallback; dry-run safety tooling only\n");
#endif
}

static u32 shell64_write_ai_status_line(u32 console_capability_handle, u32 owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return shell64_write_text(console_capability_handle, owner_id, "Product AI assistant: launcher/Settings/pkginfo show consent-scoped action templates; inference unavailable\n");
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
    return shell64_write_text(console_capability_handle, owner_id, "AI Assistant: launcher/Settings/pkginfo; consent-scoped action templates; inference unavailable\n");
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

static u32 shell64_write_decimal_line(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 value)
{
    char buffer[10];
    u32 length;

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    length = shell64_format_decimal_u32(buffer, value);
    (void)shell64_write(console_capability_handle, owner_id, (const u8 *)buffer, length);
    return shell64_write_text(console_capability_handle, owner_id, "\n");
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void shell64_write_decimal_field(
    u32 console_capability_handle,
    u32 owner_id,
    const char *label,
    u32 value)
{
    char buffer[10];
    u32 length;

    (void)shell64_write_text(console_capability_handle, owner_id, label);
    length = shell64_format_decimal_u32(buffer, value);
    (void)shell64_write(console_capability_handle, owner_id, (const u8 *)buffer, length);
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

static u32 shell64_print_hardware_validation_status(u32 console_capability_handle, u32 owner_id)
{
    u32 network_online = (virtio_net64_dhcp_ack() != 0u) ? 1u : 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 hardware_capability;

    hardware_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_HARDWARE,
        CAPABILITY64_RIGHT_QUERY,
        owner_id);
    hardware64_registry_refresh(hardware_capability, owner_id);
    if (hardware_capability != CAPABILITY64_INVALID_HANDLE)
    {
        (void)capability64_revoke(hardware_capability, owner_id);
    }
#endif

    (void)shell64_write_text(console_capability_handle, owner_id, "hardware validation: read-only Product mode\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "machine model: unavailable from firmware table\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "secure boot: unavailable/not Product-detected\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "build profile: Product\n");
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "boot path: ",
        (package_signing64_signed() != 0u) ? "UEFI Product" : "BIOS checksum fallback");
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
    (void)shell64_write_status_line(
        console_capability_handle,
        owner_id,
        "mouse backend: ",
        shell64_mouse_backend_label());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci found: ", xhci64_found());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci handoff: ", xhci64_legacy_handoff());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci hid keyboard: ", xhci64_hid_device());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "xhci mouse endpoint: ", xhci64_mouse_endpoint_present());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci mouse reports: ", xhci64_mouse_reports());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci mouse bytes: ", xhci64_mouse_report_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "xhci error: ", xhci64_error());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "i2c pointer found: ", i2c_hid64_pointer_found());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer reports: ", i2c_hid64_pointer_report_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer error: ", i2c_hid64_pointer_error());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c controllers: ", pci64_lpss_i2c_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "i2c pointer candidates: ", pci64_lpss_i2c_pointer_candidate_count());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary pci: ", pci64_lpss_i2c_address());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary bar0: ", pci64_lpss_i2c_bar0());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary base low: ", pci64_lpss_i2c_base_low());
    (void)shell64_write_hex32_line(console_capability_handle, owner_id, "i2c primary flags: ", pci64_lpss_i2c_mmio_flags());
    if (pci64_lpss_i2c_pointer_candidate_count() != 0u)
    {
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 pci: ",
            pci64_lpss_i2c_pointer_candidate_address(0u));
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 base low: ",
            pci64_lpss_i2c_pointer_candidate_base_low(0u));
        (void)shell64_write_hex32_line(
            console_capability_handle,
            owner_id,
            "i2c pointer0 flags: ",
            pci64_lpss_i2c_pointer_candidate_mmio_flags(0u));
    }
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "mouse packets: ", input64_mouse_packet_count());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "mouse x: ", input64_mouse_x());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "mouse y: ", input64_mouse_y());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 fallback present: ", input64_ps2_present());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "ps2 fallback enabled: ", input64_ps2_enabled());
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
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "nvme rw error: ", mmio64_nvme_rw_error());
    (void)shell64_write_yes_no_line(console_capability_handle, owner_id, "boot media linux staged: ", boot_media64_available());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media app bytes: ", boot_media64_app_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media interp bytes: ", boot_media64_interp_bytes());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media flags: ", boot_media64_flags());
    (void)shell64_write_decimal_line(console_capability_handle, owner_id, "boot media status: ", boot_media64_status());
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
    (void)shell64_write_text(console_capability_handle, owner_id, "[x64] drs-hardware-registry hardware-registry 1");
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
    (void)shell64_write_hex32_line(
        console_capability_handle,
        owner_id,
        " token ",
        hardware64_registry_token());
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

    if (shell64_token_equals(token_start, token_length, "hwval"))
    {
        return shell64_write_text(console_capability_handle, owner_id, "usage: hwval - show read-only hardware validation status\n");
    }

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
        || shell64_token_equals(token_start, token_length, "nethello")
        || shell64_token_equals(token_start, token_length, "rename")
        || shell64_token_equals(token_start, token_length, "stat")
        || shell64_token_equals(token_start, token_length, "touch")
        || shell64_token_equals(token_start, token_length, "write");
}

static int shell64_token_is_builtin_command(u32 token_start, u32 token_length)
{
    return shell64_token_equals(token_start, token_length, "apps")
        || shell64_token_equals(token_start, token_length, "help")
        || shell64_token_equals(token_start, token_length, "hwval")
        || shell64_token_equals(token_start, token_length, "info")
        || shell64_token_equals(token_start, token_length, "linux")
        || shell64_token_equals(token_start, token_length, "lock")
        || shell64_token_equals(token_start, token_length, "net")
        || shell64_token_equals(token_start, token_length, "pkginfo")
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
        return shell64_write_text(console_capability_handle, owner_id, "\n");
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

    (void)shell64_write_text(console_capability_handle, owner_id, "Product apps:\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "APPEND\nCAT\nCOPY\nDELETE\nLS\nMKDIR\nMOVE\nNETHELLO\nRENAME\nSTAT\nTOUCH\nWRITE\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Product services:\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Network (hardware-gated): use net or net curl example.com\n");
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)shell64_write_text(console_capability_handle, owner_id, "Brokered socket API: capability-scoped TCP-client foundation in net\n");
#endif
    (void)shell64_write_text(console_capability_handle, owner_id, "Hardware validation: use hwval; read-only; MSI evidence pending\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Package trust: use pkginfo or Settings\n");
    (void)shell64_write_apps_gui_line(console_capability_handle, owner_id);
    (void)shell64_write_text(console_capability_handle, owner_id, "Service/session status: Settings\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Identity/account/vault/transport status: Settings; local only; no secret storage\n");
    (void)shell64_write_text(console_capability_handle, owner_id, "Cloud storage status: Settings/File Manager; unavailable/planned; no sync\n");
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
        (void)shell64_write_builtins_line(console_capability_handle, owner_id);
        (void)shell64_write_text(console_capability_handle, owner_id, "Product apps: append cat copy delete ls mkdir move nethello rename stat touch write\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "Product network: net shows DHCP lease; net curl example.com performs a scoped HTTP GET\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "Product hardware validation: hwval is read-only; MSI manual evidence pending\n");
        (void)shell64_write_text(console_capability_handle, owner_id, "Product package trust: pkginfo and Settings are read-only; install/apply disabled\n");
        (void)shell64_write_gui_status_line(console_capability_handle, owner_id);
        (void)shell64_write_service_status_line(console_capability_handle, owner_id);
        (void)shell64_write_login_status_line(console_capability_handle, owner_id);
        (void)shell64_write_identity_status_line(console_capability_handle, owner_id);
        (void)shell64_write_text(console_capability_handle, owner_id, "Product cloud storage: Settings/File Manager show broker policy; sync/upload/download unavailable\n");
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

    if (shell64_token_equals(command_start, command_length, "hwval"))
    {
        return shell64_print_hardware_validation_status(console_capability_handle, owner_id);
    }

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
