#include "arch_build.h"
#include "boot_info.h"
#include "bootstrap_catalog.h"
#include "services.h"
#include "services_x64.h"
#include "types.h"
#include "uefi.h"

#define LIMITLESS_UEFI_BOOTSTRAP_KIND "uefi64-firmware-app"
#define EFI_PIXEL_RED_GREEN_BLUE_RESERVED8_BIT_PER_COLOR 0u
#define EFI_PIXEL_BLUE_GREEN_RED_RESERVED8_BIT_PER_COLOR 1u
#define EFI_FILE_MODE_READ 0x0000000000000001ull
#define LIMITLESS_EFI_LOCAL_ERROR 0xFFFFFFFFFFFFFFFEull
#define LIMITLESS_UEFI_LOADER_BUFFER_BYTES (768u * 1024u)
#define LIMITLESS_UEFI_MEMORY_MAP_BYTES (32u * 1024u)
#define LIMITLESS_UEFI_KERNEL_PLACEMENT_ALIGNMENT 0x0000000000200000ull
#define LIMITLESS_UEFI_KERNEL_LINKED_BASE 0x0000000000010000ull
#define LIMITLESS_UEFI_KERNEL_LINKED_ENTRY 0xFFFFFFFF80010000ull
#define LIMITLESS_UEFI_BOOT_INFO_ADDRESS 0x0000000000009000ull
#define LIMITLESS_UEFI_BOOT_PAGE_TABLE_ROOT 0x0000000000001000ull
#define LIMITLESS_UEFI_BOOT_IDENTITY_BYTES 0x0000000001000000ull
#define LIMITLESS_UEFI_BOOT_PDPT_ADDRESS 0x0000000000002000ull
#define LIMITLESS_UEFI_BOOT_PD_ADDRESS 0x0000000000003000ull
#define LIMITLESS_UEFI_BOOT_HIGH_PDPT_ADDRESS 0x0000000000004000ull
#define LIMITLESS_UEFI_BOOT_RUNTIME_PD_ADDRESS 0x0000000000005000ull
#define LIMITLESS_UEFI_BOOT_RUNTIME_PT_ADDRESS 0x0000000000006000ull
#define LIMITLESS_UEFI_BOOT_USER_RUNTIME_PT_ADDRESS 0x0000000000007000ull
#define LIMITLESS_UEFI_BOOT_USER_STACK_PAGE_ADDRESS 0x0000000000008000ull
#define LIMITLESS_UEFI_BOOT_TRAMPOLINE_ADDRESS 0x000000000000A000ull
#define LIMITLESS_UEFI_BOOT_FRAMEBUFFER_PD_ADDRESS 0x000000000000B000ull
#define LIMITLESS_UEFI_BOOT_APIC_PT_ADDRESS 0x000000000000D000ull
#define LIMITLESS_UEFI_BOOT_HANDOFF_BASE 0x0000000000001000ull
#define LIMITLESS_UEFI_BOOT_HANDOFF_PAGES 13u
#define LIMITLESS_UEFI_LARGE_PAGE_BYTES 0x0000000000200000ull
#define LIMITLESS_UEFI_BOOT_IDENTITY_ENTRIES (LIMITLESS_UEFI_BOOT_IDENTITY_BYTES / LIMITLESS_UEFI_LARGE_PAGE_BYTES)
#define LIMITLESS_UEFI_PAGE_PRESENT 0x0000000000000001ull
#define LIMITLESS_UEFI_PAGE_WRITABLE 0x0000000000000002ull
#define LIMITLESS_UEFI_PAGE_LARGE 0x0000000000000080ull
#define LIMITLESS_UEFI_HIGH_HALF_PML4_INDEX 511u
#define LIMITLESS_UEFI_HIGH_HALF_PDPT_INDEX 510u
#define LIMITLESS_UEFI_BOOT_DRIVE_MARKER 0x000000EFu
#define LIMITLESS_UEFI_APIC_OVERRIDE_SLOTS 16u
#define EFI_ALLOCATE_ADDRESS 2u
#define EFI_MEMORY_TYPE_LOADER_CODE 1u
#define EFI_MEMORY_TYPE_LOADER_DATA 2u
#define EFI_MEMORY_TYPE_BOOT_SERVICES_CODE 3u
#define EFI_MEMORY_TYPE_BOOT_SERVICES_DATA 4u
#define EFI_MEMORY_TYPE_RUNTIME_SERVICES_CODE 5u
#define EFI_MEMORY_TYPE_RUNTIME_SERVICES_DATA 6u
#define EFI_MEMORY_TYPE_CONVENTIONAL_MEMORY 7u

static struct efi_guid g_efi_graphics_output_protocol_guid = {
    0x9042A9DEu,
    0x23DCu,
    0x4A38u,
    { 0x96u, 0xFBu, 0x7Au, 0xDEu, 0xD0u, 0x80u, 0x51u, 0x6Au }
};

static struct efi_guid g_efi_loaded_image_protocol_guid = {
    0x5B1B31A1u,
    0x9562u,
    0x11D2u,
    { 0x8Eu, 0x3Fu, 0x00u, 0xA0u, 0xC9u, 0x69u, 0x72u, 0x3Bu }
};

static struct efi_guid g_efi_simple_file_system_protocol_guid = {
    0x0964E5B22u,
    0x6459u,
    0x11D2u,
    { 0x8Eu, 0x39u, 0x00u, 0xA0u, 0xC9u, 0x69u, 0x72u, 0x3Bu }
};

static struct efi_guid g_efi_acpi20_table_guid = {
    0x8868E871u,
    0xE4F1u,
    0x11D3u,
    { 0xBCu, 0x22u, 0x00u, 0x80u, 0xC7u, 0x3Cu, 0x88u, 0x81u }
};

static struct efi_guid g_efi_acpi10_table_guid = {
    0xEB9D2D30u,
    0x2D88u,
    0x11D3u,
    { 0x9Au, 0x16u, 0x00u, 0x90u, 0x27u, 0x3Fu, 0xC1u, 0x4Du }
};

static efi_char16_t g_boot_readme_path[] = {
    'R', 'E', 'A', 'D', 'M', 'E', '.', 'T', 'X', 'T', 0
};

static efi_char16_t g_boot_manifest_path[] = {
    'B', 'O', 'O', 'T', 'M', 'A', 'N', '.', 'T', 'X', 'T', 0
};

static efi_char16_t g_boot_kernel_path[] = {
    'K', 'E', 'R', 'N', 'E', 'L', '6', '4', '.', 'B', 'I', 'N', 0
};

struct uefi_boot_manifest
{
    u32 valid;
    u32 kernel_bytes;
    u32 kernel_checksum;
};

struct uefi_loader_payload
{
    u32 loaded;
    u32 match;
    u32 pages;
    u32 checksum;
    u32 bytes;
    efi_status_t status;
    u64 base;
    u64 capacity;
};

struct uefi_memory_map_summary
{
    u32 descriptors;
    u32 descriptor_size;
    u32 descriptor_version;
    u64 map_key;
    u64 total_pages;
    u64 conventional_pages;
    u64 loader_pages;
    u64 boot_services_pages;
    u64 runtime_services_pages;
    u64 largest_conventional_base;
    u64 largest_conventional_pages;
    efi_status_t status;
};

struct uefi_kernel_placement
{
    u32 planned;
    u32 allocated;
    u32 copied;
    u32 match;
    u32 pages;
    u32 bytes;
    u32 checksum;
    u64 requested_base;
    u64 physical_base;
    u64 source_base;
    u64 region_base;
    u64 region_pages;
    u64 alignment;
    efi_status_t status;
};

struct uefi_linked_kernel_placement
{
    u32 planned;
    u32 allocated;
    u32 copied;
    u32 match;
    u32 pages;
    u32 bytes;
    u32 checksum;
    u64 requested_base;
    u64 physical_base;
    u64 source_base;
    u64 linked_entry;
    u64 boot_info_base;
    u64 page_table_root;
    u64 identity_map_bytes;
    efi_status_t status;
};

struct uefi_boot_handoff
{
    u32 planned;
    u32 allocated;
    u32 built;
    u32 ready;
    u32 pages;
    u32 identity_entries;
    u32 kernel_sectors;
    u32 trampoline_bytes;
    u32 trampoline_ready;
    u32 token;
    u64 requested_base;
    u64 physical_base;
    u64 pml4;
    u64 pdpt;
    u64 pd;
    u64 high_pdpt;
    u64 runtime_pd;
    u64 runtime_pt;
    u64 user_runtime_pt;
    u64 user_stack_page;
    u64 trampoline;
    u64 framebuffer_pd;
    u64 boot_info_base;
    u64 identity_map_bytes;
    u64 linked_entry;
    efi_status_t status;
};

struct uefi_framebuffer_handoff
{
    u32 available;
    u32 mapped;
    u64 base;
    u64 bytes;
    u32 width;
    u32 height;
    u32 pixels_per_scanline;
    u32 pixel_format;
    u32 draw_pixels;
    u32 draw_token;
    u32 map_pdpt_index;
    u32 map_pd_start;
    u32 map_entries;
    u64 map_bytes;
};

struct uefi_acpi_handoff
{
    u32 rsdp_found;
    u32 xsdt_found;
    u32 mcfg_found;
    u32 madt_found;
    u32 lapic_found;
    u32 ioapic_found;
    u32 flags;
    u64 rsdp;
    u64 xsdt;
    u64 mcfg;
    u64 madt;
    u64 ecam_base;
    u64 lapic_base;
    u64 ioapic_base;
    u32 segment;
    u32 bus_start;
    u32 bus_end;
    u32 ioapic_id;
    u32 ioapic_gsi_base;
    u32 interrupt_override_scanned;
    u32 interrupt_override_count;
    u32 interrupt_override_valid_mask;
    u32 interrupt_override_source[LIMITLESS_UEFI_APIC_OVERRIDE_SLOTS];
    u32 interrupt_override_gsi[LIMITLESS_UEFI_APIC_OVERRIDE_SLOTS];
    u32 interrupt_override_flags[LIMITLESS_UEFI_APIC_OVERRIDE_SLOTS];
};

struct acpi_rsdp
{
    u8 signature[8];
    u8 checksum;
    u8 oem_id[6];
    u8 revision;
    u32 rsdt_address;
    u32 length;
    u64 xsdt_address;
    u8 extended_checksum;
    u8 reserved[3];
} __attribute__((packed));

struct acpi_sdt_header
{
    u8 signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    u8 oem_id[6];
    u8 oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
} __attribute__((packed));

struct acpi_mcfg_allocation
{
    u64 base_address;
    u16 segment_group;
    u8 start_bus;
    u8 end_bus;
    u32 reserved;
} __attribute__((packed));

struct acpi_madt
{
    struct acpi_sdt_header header;
    u32 local_apic_address;
    u32 flags;
} __attribute__((packed));

struct acpi_madt_entry_header
{
    u8 type;
    u8 length;
} __attribute__((packed));

struct acpi_madt_lapic
{
    struct acpi_madt_entry_header header;
    u8 acpi_processor_id;
    u8 apic_id;
    u32 flags;
} __attribute__((packed));

struct acpi_madt_ioapic
{
    struct acpi_madt_entry_header header;
    u8 ioapic_id;
    u8 reserved;
    u32 ioapic_address;
    u32 global_system_interrupt_base;
} __attribute__((packed));

struct acpi_madt_interrupt_source_override
{
    struct acpi_madt_entry_header header;
    u8 bus;
    u8 source;
    u32 global_system_interrupt;
    u16 flags;
} __attribute__((packed));

static u8 g_loader_kernel_buffer[LIMITLESS_UEFI_LOADER_BUFFER_BYTES] __attribute__((aligned(4096)));
static u8 g_memory_map_buffer[LIMITLESS_UEFI_MEMORY_MAP_BYTES] __attribute__((aligned(8)));
static const u8 g_kernel_entry_trampoline[] = {
    0xFAu, 0xFCu,
    0x0Fu, 0x20u, 0xC0u,
    0x48u, 0x83u, 0xE0u, 0xFBu,
    0x48u, 0x83u, 0xC8u, 0x02u,
    0x0Fu, 0x22u, 0xC0u,
    0x0Fu, 0x20u, 0xE0u,
    0x48u, 0x0Du, 0x20u, 0x06u, 0x00u, 0x00u,
    0x0Fu, 0x22u, 0xE0u,
    0xDBu, 0xE3u,
    0x48u, 0xB8u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x0Fu, 0x22u, 0xD8u,
    0x48u, 0xBFu, 0x00u, 0x90u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x48u, 0xB8u, 0x00u, 0x00u, 0x01u, 0x80u, 0xFFu, 0xFFu, 0xFFu, 0xFFu,
    0xFFu, 0xE0u,
    0xF4u, 0xEBu, 0xFDu
};

static void debug_write_char(char character)
{
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)character), "Nd"((u16)0x00E9u));
}

static void serial_init(void)
{
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)0x00u), "Nd"((u16)0x03F9u));
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)0x80u), "Nd"((u16)0x03FBu));
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)0x01u), "Nd"((u16)0x03F8u));
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)0x00u), "Nd"((u16)0x03F9u));
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)0x03u), "Nd"((u16)0x03FBu));
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)0xC7u), "Nd"((u16)0x03FAu));
    __asm__ __volatile__("outb %0, %1" : : "a"((u8)0x0Bu), "Nd"((u16)0x03FCu));
}

static void serial_write_char(char character)
{
    u8 ready = 0u;

    while ((ready & 0x20u) == 0u)
    {
        __asm__ __volatile__("inb %1, %0" : "=a"(ready) : "Nd"((u16)0x03FDu));
    }

    __asm__ __volatile__("outb %0, %1" : : "a"((u8)character), "Nd"((u16)0x03F8u));
}

static void debug_write_string(const char *text)
{
    while (*text != '\0')
    {
        debug_write_char(*text);
        serial_write_char(*text);
        ++text;
    }
}

static void console_write_ascii(struct efi_system_table *system_table, const char *text)
{
    efi_char16_t buffer[160];
    u32 index = 0u;

    if (system_table == NULL || system_table->con_out == NULL || system_table->con_out->output_string == NULL)
    {
        return;
    }

    while (*text != '\0')
    {
        char character = *text++;

        if (character == '\n')
        {
            buffer[index++] = (efi_char16_t)'\r';
            if (index >= (u32)(sizeof(buffer) / sizeof(buffer[0]) - 1u))
            {
                buffer[index] = 0;
                system_table->con_out->output_string(system_table->con_out, buffer);
                index = 0u;
            }

            buffer[index++] = (efi_char16_t)'\n';
        }
        else
        {
            buffer[index++] = (efi_char16_t)((u8)character);
        }

        if (index >= (u32)(sizeof(buffer) / sizeof(buffer[0]) - 1u))
        {
            buffer[index] = 0;
            system_table->con_out->output_string(system_table->con_out, buffer);
            index = 0u;
        }
    }

    if (index > 0u)
    {
        buffer[index] = 0;
        system_table->con_out->output_string(system_table->con_out, buffer);
    }
}

static void write_line(struct efi_system_table *system_table, const char *text)
{
    debug_write_string(text);
    console_write_ascii(system_table, text);
}

static void append_char(char *buffer, u32 buffer_size, u32 *length, char character)
{
    if ((*length + 1u) >= buffer_size)
    {
        return;
    }

    buffer[*length] = character;
    ++(*length);
    buffer[*length] = '\0';
}

static void append_string(char *buffer, u32 buffer_size, u32 *length, const char *text)
{
    while (*text != '\0')
    {
        append_char(buffer, buffer_size, length, *text);
        ++text;
    }
}

static void append_dec_u32(char *buffer, u32 buffer_size, u32 *length, u32 value)
{
    char digits[10];
    u32 count = 0u;

    if (value == 0u)
    {
        append_char(buffer, buffer_size, length, '0');
        return;
    }

    while ((value > 0u) && (count < 10u))
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count > 0u)
    {
        --count;
        append_char(buffer, buffer_size, length, digits[count]);
    }
}

static void append_dec_u64(char *buffer, u32 buffer_size, u32 *length, u64 value)
{
    char digits[20];
    u32 count = 0u;

    if (value == 0u)
    {
        append_char(buffer, buffer_size, length, '0');
        return;
    }

    while ((value > 0u) && (count < 20u))
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count > 0u)
    {
        --count;
        append_char(buffer, buffer_size, length, digits[count]);
    }
}

static void append_hex_digit(char *buffer, u32 buffer_size, u32 *length, u8 value)
{
    if (value < 10u)
    {
        append_char(buffer, buffer_size, length, (char)('0' + value));
        return;
    }

    append_char(buffer, buffer_size, length, (char)('A' + (value - 10u)));
}

static void append_hex_u32(char *buffer, u32 buffer_size, u32 *length, u32 value)
{
    s32 shift;

    append_string(buffer, buffer_size, length, "0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        append_hex_digit(buffer, buffer_size, length, (u8)((value >> shift) & 0x0Fu));
    }
}

static void append_hex_u64(char *buffer, u32 buffer_size, u32 *length, u64 value)
{
    s32 shift;

    append_string(buffer, buffer_size, length, "0x");
    for (shift = 60; shift >= 0; shift -= 4)
    {
        append_hex_digit(buffer, buffer_size, length, (u8)((value >> shift) & 0x0Fu));
    }
}

static void append_pixel_format(char *buffer, u32 buffer_size, u32 *length, u32 pixel_format)
{
    if (pixel_format == EFI_PIXEL_RED_GREEN_BLUE_RESERVED8_BIT_PER_COLOR)
    {
        append_string(buffer, buffer_size, length, "rgb");
        return;
    }

    if (pixel_format == EFI_PIXEL_BLUE_GREEN_RED_RESERVED8_BIT_PER_COLOR)
    {
        append_string(buffer, buffer_size, length, "bgr");
        return;
    }

    append_string(buffer, buffer_size, length, "other");
}

static u32 uefi_supported_framebuffer_format(u32 pixel_format)
{
    return (pixel_format == EFI_PIXEL_RED_GREEN_BLUE_RESERVED8_BIT_PER_COLOR ||
            pixel_format == EFI_PIXEL_BLUE_GREEN_RED_RESERVED8_BIT_PER_COLOR) ? 1u : 0u;
}

static void init_framebuffer_handoff(struct uefi_framebuffer_handoff *framebuffer)
{
    if (framebuffer == NULL)
    {
        return;
    }

    framebuffer->available = 0u;
    framebuffer->mapped = 0u;
    framebuffer->base = 0ull;
    framebuffer->bytes = 0ull;
    framebuffer->width = 0u;
    framebuffer->height = 0u;
    framebuffer->pixels_per_scanline = 0u;
    framebuffer->pixel_format = 0u;
    framebuffer->draw_pixels = 0u;
    framebuffer->draw_token = 0u;
    framebuffer->map_pdpt_index = 0u;
    framebuffer->map_pd_start = 0u;
    framebuffer->map_entries = 0u;
    framebuffer->map_bytes = 0ull;
}

static u32 gop_make_pixel(u32 pixel_format, u8 red, u8 green, u8 blue)
{
    if (pixel_format == EFI_PIXEL_RED_GREEN_BLUE_RESERVED8_BIT_PER_COLOR)
    {
        return ((u32)blue << 16) | ((u32)green << 8) | (u32)red;
    }

    return ((u32)red << 16) | ((u32)green << 8) | (u32)blue;
}

static u32 gop_draw_pattern(struct efi_graphics_output_protocol *gop, u32 *drawn_pixels)
{
    struct efi_graphics_output_protocol_mode *mode;
    struct efi_graphics_output_mode_information *info;
    volatile u32 *framebuffer;
    u32 width;
    u32 height;
    u32 y;
    u32 x;
    u32 token = 2166136261u;

    *drawn_pixels = 0u;

    if (gop == NULL || gop->mode == NULL || gop->mode->info == NULL)
    {
        return 0u;
    }

    mode = gop->mode;
    info = mode->info;
    if (mode->frame_buffer_base == 0u ||
        info->pixels_per_scan_line == 0u ||
        (info->pixel_format != EFI_PIXEL_RED_GREEN_BLUE_RESERVED8_BIT_PER_COLOR &&
         info->pixel_format != EFI_PIXEL_BLUE_GREEN_RED_RESERVED8_BIT_PER_COLOR))
    {
        return 0u;
    }

    width = info->horizontal_resolution;
    height = info->vertical_resolution;
    if (width > 96u)
    {
        width = 96u;
    }
    if (height > 32u)
    {
        height = 32u;
    }
    if (width == 0u || height == 0u)
    {
        return 0u;
    }

    framebuffer = (volatile u32 *)mode->frame_buffer_base;
    for (y = 0u; y < height; ++y)
    {
        for (x = 0u; x < width; ++x)
        {
            u8 red = (u8)(0x30u + ((x * 3u) & 0x7Fu));
            u8 green = (u8)(0x40u + ((y * 5u) & 0x7Fu));
            u8 blue = (u8)(0x90u + (((x + y) * 2u) & 0x5Fu));
            u32 pixel = gop_make_pixel(info->pixel_format, red, green, blue);
            u32 offset = (y * info->pixels_per_scan_line) + x;

            framebuffer[offset] = pixel;
            token ^= framebuffer[offset];
            token *= 16777619u;
            ++(*drawn_pixels);
        }
    }

    return token;
}

static void write_gop_framebuffer_line(
    struct efi_system_table *system_table,
    struct uefi_framebuffer_handoff *framebuffer)
{
    struct efi_graphics_output_protocol *gop = NULL;
    efi_status_t status;
    char line[192];
    u32 length = 0u;

    init_framebuffer_handoff(framebuffer);

    if (system_table == NULL || system_table->boot_services == NULL ||
        system_table->boot_services->locate_protocol == NULL)
    {
        write_line(system_table, "[uefi] gop framebuffer unavailable reason boot-services\n");
        return;
    }

    status = system_table->boot_services->locate_protocol(
        &g_efi_graphics_output_protocol_guid,
        NULL,
        (void **)&gop);
    if (status != EFI_SUCCESS || gop == NULL || gop->mode == NULL || gop->mode->info == NULL)
    {
        append_string(line, sizeof(line), &length, "[uefi] gop framebuffer unavailable status ");
        append_hex_u64(line, sizeof(line), &length, status);
        append_char(line, sizeof(line), &length, '\n');
        write_line(system_table, line);
        return;
    }

    if (framebuffer != NULL)
    {
        framebuffer->base = gop->mode->frame_buffer_base;
        framebuffer->bytes = gop->mode->frame_buffer_size;
        framebuffer->width = gop->mode->info->horizontal_resolution;
        framebuffer->height = gop->mode->info->vertical_resolution;
        framebuffer->pixels_per_scanline = gop->mode->info->pixels_per_scan_line;
        framebuffer->pixel_format = gop->mode->info->pixel_format;
        if (framebuffer->base != 0ull &&
            framebuffer->bytes >= 4ull &&
            framebuffer->width != 0u &&
            framebuffer->height != 0u &&
            framebuffer->pixels_per_scanline >= framebuffer->width &&
            uefi_supported_framebuffer_format(framebuffer->pixel_format) != 0u)
        {
            framebuffer->available = 1u;
        }
    }

    append_string(line, sizeof(line), &length, "[uefi] gop framebuffer mode ");
    append_dec_u32(line, sizeof(line), &length, gop->mode->mode);
    append_string(line, sizeof(line), &length, " max ");
    append_dec_u32(line, sizeof(line), &length, gop->mode->max_mode);
    append_char(line, sizeof(line), &length, ' ');
    append_dec_u32(line, sizeof(line), &length, gop->mode->info->horizontal_resolution);
    append_char(line, sizeof(line), &length, 'x');
    append_dec_u32(line, sizeof(line), &length, gop->mode->info->vertical_resolution);
    append_string(line, sizeof(line), &length, " ppsl ");
    append_dec_u32(line, sizeof(line), &length, gop->mode->info->pixels_per_scan_line);
    append_string(line, sizeof(line), &length, " format ");
    append_pixel_format(line, sizeof(line), &length, gop->mode->info->pixel_format);
    append_string(line, sizeof(line), &length, " base ");
    append_hex_u64(line, sizeof(line), &length, gop->mode->frame_buffer_base);
    append_string(line, sizeof(line), &length, " bytes ");
    append_hex_u64(line, sizeof(line), &length, gop->mode->frame_buffer_size);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);

    length = 0u;
    line[0] = '\0';
    {
        u32 drawn_pixels = 0u;
        u32 token = gop_draw_pattern(gop, &drawn_pixels);

        if (framebuffer != NULL)
        {
            framebuffer->draw_pixels = drawn_pixels;
            framebuffer->draw_token = token;
            if (drawn_pixels == 0u)
            {
                framebuffer->available = 0u;
            }
        }

        append_string(line, sizeof(line), &length, "[uefi] gop draw pixels ");
        append_dec_u32(line, sizeof(line), &length, drawn_pixels);
        append_string(line, sizeof(line), &length, " token ");
        append_hex_u32(line, sizeof(line), &length, token);
        append_string(line, sizeof(line), &length, " status ");
        append_dec_u32(line, sizeof(line), &length, drawn_pixels > 0u ? 1u : 0u);
        append_char(line, sizeof(line), &length, '\n');
        write_line(system_table, line);
    }
}

static void write_package_archive_line(struct efi_system_table *system_table)
{
    char line[160];
    u32 length = 0u;

    if (services64_package_valid() == 0u)
    {
        write_line(system_table, "[uefi] package archive unavailable\n");
        return;
    }

    append_string(line, sizeof(line), &length, "[uefi] package archive v");
    append_dec_u32(line, sizeof(line), &length, services64_package_version());
    append_string(line, sizeof(line), &length, " signers ");
    append_dec_u32(line, sizeof(line), &length, services64_package_signer_count());
    append_string(line, sizeof(line), &length, " manifests ");
    append_dec_u32(line, sizeof(line), &length, services64_package_manifest_count());
    append_string(line, sizeof(line), &length, " payloads ");
    append_dec_u32(line, sizeof(line), &length, services64_package_payload_count());
    append_string(line, sizeof(line), &length, " checksum ");
    append_hex_u32(line, sizeof(line), &length, services64_package_checksum());

    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void write_service_namespace_line(struct efi_system_table *system_table)
{
    char line[192];
    u32 length = 0u;

    append_string(line, sizeof(line), &length, "[uefi] services ");
    append_dec_u32(line, sizeof(line), &length, services64_count());
    append_string(line, sizeof(line), &length, " console ");
    append_dec_u32(line, sizeof(line), &length, services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE));
    append_string(line, sizeof(line), &length, " ramfs ");
    append_dec_u32(line, sizeof(line), &length, services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS));
    append_string(line, sizeof(line), &length, " input ");
    append_dec_u32(line, sizeof(line), &length, services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT));
    append_string(line, sizeof(line), &length, " display ");
    append_dec_u32(line, sizeof(line), &length, services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_DISPLAY));
    append_string(line, sizeof(line), &length, " block ");
    append_dec_u32(line, sizeof(line), &length, services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_BLOCK));
    append_string(line, sizeof(line), &length, " hardware ");
    append_dec_u32(line, sizeof(line), &length, services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_HARDWARE));
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static u32 string_length(const char *text)
{
    u32 length = 0u;

    while (text[length] != '\0')
    {
        ++length;
    }

    return length;
}

static u32 ascii_prefix_ok(const u8 *buffer, u64 length, const char *expected)
{
    u32 index = 0u;

    while (expected[index] != '\0')
    {
        if ((u64)index >= length || buffer[index] != (u8)expected[index])
        {
            return 0u;
        }
        ++index;
    }

    return 1u;
}

static u32 ascii_matches_at(const u8 *buffer, u64 length, u64 offset, const char *text)
{
    u32 index = 0u;

    while (text[index] != '\0')
    {
        if ((offset + index) >= length || buffer[offset + index] != (u8)text[index])
        {
            return 0u;
        }
        ++index;
    }

    return 1u;
}

static u32 buffer_contains_ascii(const u8 *buffer, u64 length, const char *text)
{
    u32 text_length = string_length(text);
    u64 offset;

    if (text_length == 0u)
    {
        return 1u;
    }

    if ((u64)text_length > length)
    {
        return 0u;
    }

    for (offset = 0u; offset <= (length - text_length); ++offset)
    {
        if (ascii_matches_at(buffer, length, offset, text) != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 parse_decimal_key(const u8 *buffer, u64 length, const char *key, u32 *value_out)
{
    u32 key_length = string_length(key);
    u64 offset;

    if (value_out == NULL || key_length == 0u || (u64)key_length >= length)
    {
        return 0u;
    }

    for (offset = 0u; offset <= (length - key_length); ++offset)
    {
        u64 cursor;
        u32 value = 0u;
        u32 digits = 0u;

        if (ascii_matches_at(buffer, length, offset, key) == 0u)
        {
            continue;
        }

        cursor = offset + key_length;
        while (cursor < length && buffer[cursor] >= (u8)'0' && buffer[cursor] <= (u8)'9')
        {
            value = (value * 10u) + (u32)(buffer[cursor] - (u8)'0');
            ++digits;
            ++cursor;
        }

        if (digits > 0u)
        {
            *value_out = value;
            return 1u;
        }
    }

    return 0u;
}

static u32 hex_digit_value(u8 character, u8 *value_out)
{
    if (value_out == NULL)
    {
        return 0u;
    }

    if (character >= (u8)'0' && character <= (u8)'9')
    {
        *value_out = (u8)(character - (u8)'0');
        return 1u;
    }

    if (character >= (u8)'A' && character <= (u8)'F')
    {
        *value_out = (u8)(10u + (character - (u8)'A'));
        return 1u;
    }

    if (character >= (u8)'a' && character <= (u8)'f')
    {
        *value_out = (u8)(10u + (character - (u8)'a'));
        return 1u;
    }

    return 0u;
}

static u32 parse_hex_key(const u8 *buffer, u64 length, const char *key, u32 *value_out)
{
    u32 key_length = string_length(key);
    u64 offset;

    if (value_out == NULL || key_length == 0u || (u64)key_length >= length)
    {
        return 0u;
    }

    for (offset = 0u; offset <= (length - key_length); ++offset)
    {
        u64 cursor;
        u32 value = 0u;
        u32 digits = 0u;

        if (ascii_matches_at(buffer, length, offset, key) == 0u)
        {
            continue;
        }

        cursor = offset + key_length;
        if ((cursor + 1u) < length && buffer[cursor] == (u8)'0' &&
            (buffer[cursor + 1u] == (u8)'x' || buffer[cursor + 1u] == (u8)'X'))
        {
            cursor += 2u;
        }

        while (cursor < length)
        {
            u8 digit = 0u;

            if (hex_digit_value(buffer[cursor], &digit) == 0u)
            {
                break;
            }

            value = (value << 4) | (u32)digit;
            ++digits;
            ++cursor;
        }

        if (digits > 0u)
        {
            *value_out = value;
            return 1u;
        }
    }

    return 0u;
}

static u32 checksum_update(u32 token, const u8 *buffer, u64 length)
{
    u64 index;

    for (index = 0u; index < length; ++index)
    {
        token ^= buffer[index];
        token *= 16777619u;
    }

    return token;
}

static u32 checksum_bytes(const u8 *buffer, u64 length)
{
    return checksum_update(2166136261u, buffer, length);
}

static u64 align_up_u64(u64 value, u64 alignment)
{
    if (alignment == 0u)
    {
        return value;
    }

    return (value + alignment - 1u) & ~(alignment - 1u);
}

static void copy_bytes(u8 *destination, const u8 *source, u64 length)
{
    u64 index;

    for (index = 0u; index < length; ++index)
    {
        destination[index] = source[index];
    }
}

static void zero_bytes(u8 *destination, u64 length)
{
    u64 index;

    for (index = 0u; index < length; ++index)
    {
        destination[index] = 0u;
    }
}

static u32 guid_equal(const struct efi_guid *left, const struct efi_guid *right)
{
    u32 index;

    if (left == NULL || right == NULL)
    {
        return 0u;
    }

    if (left->data1 != right->data1 ||
        left->data2 != right->data2 ||
        left->data3 != right->data3)
    {
        return 0u;
    }

    for (index = 0u; index < 8u; ++index)
    {
        if (left->data4[index] != right->data4[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 acpi_signature_equals(const u8 *signature, const char *expected, u32 length)
{
    u32 index;

    if (signature == NULL || expected == NULL)
    {
        return 0u;
    }

    for (index = 0u; index < length; ++index)
    {
        if (signature[index] != (u8)expected[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static void init_acpi_handoff(struct uefi_acpi_handoff *acpi)
{
    u32 index;

    if (acpi == NULL)
    {
        return;
    }

    acpi->rsdp_found = 0u;
    acpi->xsdt_found = 0u;
    acpi->mcfg_found = 0u;
    acpi->madt_found = 0u;
    acpi->lapic_found = 0u;
    acpi->ioapic_found = 0u;
    acpi->flags = 0u;
    acpi->rsdp = 0ull;
    acpi->xsdt = 0ull;
    acpi->mcfg = 0ull;
    acpi->madt = 0ull;
    acpi->ecam_base = 0ull;
    acpi->lapic_base = 0ull;
    acpi->ioapic_base = 0ull;
    acpi->segment = 0u;
    acpi->bus_start = 0u;
    acpi->bus_end = 0u;
    acpi->ioapic_id = 0u;
    acpi->ioapic_gsi_base = 0u;
    acpi->interrupt_override_scanned = 0u;
    acpi->interrupt_override_count = 0u;
    acpi->interrupt_override_valid_mask = 0u;
    for (index = 0u; index < LIMITLESS_UEFI_APIC_OVERRIDE_SLOTS; ++index)
    {
        acpi->interrupt_override_source[index] = 0u;
        acpi->interrupt_override_gsi[index] = 0u;
        acpi->interrupt_override_flags[index] = 0u;
    }
}

static const struct acpi_rsdp *find_acpi_rsdp(struct efi_system_table *system_table)
{
    struct efi_configuration_table *table;
    u64 index;
    const struct acpi_rsdp *fallback = NULL;

    if (system_table == NULL || system_table->configuration_table == NULL)
    {
        return NULL;
    }

    table = system_table->configuration_table;
    for (index = 0u; index < system_table->number_of_table_entries; ++index)
    {
        if (guid_equal(&table[index].vendor_guid, &g_efi_acpi20_table_guid) != 0u)
        {
            return (const struct acpi_rsdp *)table[index].vendor_table;
        }

        if (guid_equal(&table[index].vendor_guid, &g_efi_acpi10_table_guid) != 0u)
        {
            fallback = (const struct acpi_rsdp *)table[index].vendor_table;
        }
    }

    return fallback;
}

static void discover_acpi_madt(const struct acpi_sdt_header *table, struct uefi_acpi_handoff *acpi)
{
    const struct acpi_madt *madt;
    const u8 *cursor;
    const u8 *end;

    if (table == NULL ||
        acpi == NULL ||
        table->length < sizeof(struct acpi_madt) ||
        acpi_signature_equals(table->signature, "APIC", 4u) == 0u)
    {
        return;
    }

    madt = (const struct acpi_madt *)(const void *)table;
    acpi->madt_found = 1u;
    acpi->madt = (u64)(const void *)table;
    acpi->lapic_base = (u64)madt->local_apic_address;
    acpi->interrupt_override_scanned = 1u;
    acpi->flags |= LIMITLESS_BOOT_ACPI_FLAG_MADT;

    cursor = (const u8 *)table + sizeof(struct acpi_madt);
    end = (const u8 *)table + table->length;
    while ((cursor + sizeof(struct acpi_madt_entry_header)) <= end)
    {
        const struct acpi_madt_entry_header *entry =
            (const struct acpi_madt_entry_header *)(const void *)cursor;

        if (entry->length < sizeof(struct acpi_madt_entry_header) ||
            (cursor + entry->length) > end)
        {
            break;
        }

        if (entry->type == 0u && entry->length >= sizeof(struct acpi_madt_lapic))
        {
            const struct acpi_madt_lapic *lapic =
                (const struct acpi_madt_lapic *)(const void *)cursor;
            if ((lapic->flags & 0x00000001u) != 0u && acpi->lapic_base != 0ull)
            {
                acpi->lapic_found = 1u;
                acpi->flags |= LIMITLESS_BOOT_ACPI_FLAG_LAPIC;
            }
        }
        else if (entry->type == 1u && entry->length >= sizeof(struct acpi_madt_ioapic))
        {
            const struct acpi_madt_ioapic *ioapic =
                (const struct acpi_madt_ioapic *)(const void *)cursor;
            if (ioapic->ioapic_address != 0u && acpi->ioapic_found == 0u)
            {
                acpi->ioapic_found = 1u;
                acpi->ioapic_base = (u64)ioapic->ioapic_address;
                acpi->ioapic_id = ioapic->ioapic_id;
                acpi->ioapic_gsi_base = ioapic->global_system_interrupt_base;
                acpi->flags |= LIMITLESS_BOOT_ACPI_FLAG_IOAPIC;
            }
        }
        else if (entry->type == 2u && entry->length >= sizeof(struct acpi_madt_interrupt_source_override))
        {
            const struct acpi_madt_interrupt_source_override *override =
                (const struct acpi_madt_interrupt_source_override *)(const void *)cursor;
            u32 source = (u32)override->source;

            if (override->bus == 0u && source < LIMITLESS_UEFI_APIC_OVERRIDE_SLOTS)
            {
                u32 source_bit = 1u << source;

                if ((acpi->interrupt_override_valid_mask & source_bit) == 0u)
                {
                    ++acpi->interrupt_override_count;
                }

                acpi->interrupt_override_valid_mask |= source_bit;
                acpi->interrupt_override_source[source] = source;
                acpi->interrupt_override_gsi[source] = override->global_system_interrupt;
                acpi->interrupt_override_flags[source] = (u32)override->flags;
                acpi->flags |= LIMITLESS_BOOT_ACPI_FLAG_APIC_OVERRIDES;
            }
        }

        cursor += entry->length;
    }
}

static void discover_acpi_tables(struct efi_system_table *system_table, struct uefi_acpi_handoff *acpi)
{
    const struct acpi_rsdp *rsdp;
    const struct acpi_sdt_header *xsdt;
    const u64 *entries;
    u32 entry_count;
    u32 index;

    init_acpi_handoff(acpi);
    if (acpi == NULL)
    {
        return;
    }

    rsdp = find_acpi_rsdp(system_table);
    if (rsdp == NULL ||
        acpi_signature_equals(rsdp->signature, "RSD PTR ", 8u) == 0u)
    {
        return;
    }

    acpi->rsdp_found = 1u;
    acpi->rsdp = (u64)(const void *)rsdp;
    acpi->flags |= LIMITLESS_BOOT_ACPI_FLAG_RSDP;
    if (rsdp->revision < 2u || rsdp->xsdt_address == 0ull)
    {
        return;
    }

    xsdt = (const struct acpi_sdt_header *)(u64)rsdp->xsdt_address;
    if (xsdt == NULL ||
        xsdt->length < sizeof(struct acpi_sdt_header) ||
        acpi_signature_equals(xsdt->signature, "XSDT", 4u) == 0u)
    {
        return;
    }

    acpi->xsdt_found = 1u;
    acpi->xsdt = (u64)(const void *)xsdt;
    acpi->flags |= LIMITLESS_BOOT_ACPI_FLAG_XSDT;
    entry_count = (xsdt->length - (u32)sizeof(struct acpi_sdt_header)) / 8u;
    entries = (const u64 *)(const void *)((const u8 *)xsdt + sizeof(struct acpi_sdt_header));

    for (index = 0u; index < entry_count; ++index)
    {
        const struct acpi_sdt_header *table = (const struct acpi_sdt_header *)(u64)entries[index];

        if (table == NULL || table->length < sizeof(struct acpi_sdt_header))
        {
            continue;
        }

        if (acpi_signature_equals(table->signature, "APIC", 4u) != 0u)
        {
            discover_acpi_madt(table, acpi);
            continue;
        }

        if (table->length < (sizeof(struct acpi_sdt_header) + 8u + sizeof(struct acpi_mcfg_allocation)) ||
            acpi_signature_equals(table->signature, "MCFG", 4u) == 0u)
        {
            continue;
        }

        {
            const struct acpi_mcfg_allocation *allocation =
                (const struct acpi_mcfg_allocation *)(const void *)
                    ((const u8 *)table + sizeof(struct acpi_sdt_header) + 8u);
            u32 allocation_count =
                (table->length - (u32)sizeof(struct acpi_sdt_header) - 8u) /
                (u32)sizeof(struct acpi_mcfg_allocation);
            u32 allocation_index;

            for (allocation_index = 0u; allocation_index < allocation_count; ++allocation_index)
            {
                if (allocation[allocation_index].segment_group == 0u &&
                    allocation[allocation_index].base_address != 0ull &&
                    allocation[allocation_index].end_bus >= allocation[allocation_index].start_bus)
                {
                    acpi->mcfg_found = 1u;
                    acpi->mcfg = (u64)(const void *)table;
                    acpi->ecam_base = allocation[allocation_index].base_address;
                    acpi->segment = allocation[allocation_index].segment_group;
                    acpi->bus_start = allocation[allocation_index].start_bus;
                    acpi->bus_end = allocation[allocation_index].end_bus;
                    acpi->flags |= LIMITLESS_BOOT_ACPI_FLAG_MCFG;
                    break;
                }
            }
        }
    }
}

static u32 parse_boot_manifest(const u8 *buffer, u64 length, struct uefi_boot_manifest *manifest)
{
    u32 kernel_bytes = 0u;
    u32 kernel_checksum = 0u;

    if (manifest == NULL)
    {
        return 0u;
    }

    manifest->valid = 0u;
    manifest->kernel_bytes = 0u;
    manifest->kernel_checksum = 0u;

    if (ascii_prefix_ok(buffer, length, "LimitlessOS boot manifest v1") == 0u ||
        buffer_contains_ascii(buffer, length, "architecture=x86_64") == 0u ||
        buffer_contains_ascii(buffer, length, "kernel=KERNEL64.BIN") == 0u ||
        parse_decimal_key(buffer, length, "kernel-bytes=", &kernel_bytes) == 0u ||
        parse_hex_key(buffer, length, "kernel-checksum=", &kernel_checksum) == 0u ||
        kernel_bytes == 0u ||
        kernel_checksum == 0u)
    {
        return 0u;
    }

    manifest->valid = 1u;
    manifest->kernel_bytes = kernel_bytes;
    manifest->kernel_checksum = kernel_checksum;
    return 1u;
}

static void close_file_if_present(struct efi_file_protocol *file)
{
    if (file != NULL && file->close != NULL)
    {
        file->close(file);
    }
}

static efi_status_t open_boot_root(
    efi_handle_t image_handle,
    struct efi_system_table *system_table,
    struct efi_file_protocol **root_out,
    const char **stage_out)
{
    struct efi_loaded_image_protocol *loaded_image = NULL;
    struct efi_simple_file_system_protocol *file_system = NULL;
    efi_status_t status;

    if (root_out != NULL)
    {
        *root_out = NULL;
    }
    if (stage_out != NULL)
    {
        *stage_out = "boot-services";
    }

    if (system_table == NULL || system_table->boot_services == NULL ||
        system_table->boot_services->handle_protocol == NULL ||
        root_out == NULL)
    {
        return LIMITLESS_EFI_LOCAL_ERROR;
    }

    status = system_table->boot_services->handle_protocol(
        image_handle,
        &g_efi_loaded_image_protocol_guid,
        (void **)&loaded_image);
    if (status != EFI_SUCCESS || loaded_image == NULL || loaded_image->device_handle == NULL)
    {
        if (stage_out != NULL)
        {
            *stage_out = "loaded-image";
        }
        return status;
    }

    status = system_table->boot_services->handle_protocol(
        loaded_image->device_handle,
        &g_efi_simple_file_system_protocol_guid,
        (void **)&file_system);
    if (status != EFI_SUCCESS || file_system == NULL || file_system->open_volume == NULL)
    {
        if (stage_out != NULL)
        {
            *stage_out = "filesystem";
        }
        return status;
    }

    status = file_system->open_volume(file_system, root_out);
    if (status != EFI_SUCCESS || *root_out == NULL || (*root_out)->open == NULL)
    {
        if (stage_out != NULL)
        {
            *stage_out = "volume";
        }
        close_file_if_present(*root_out);
        *root_out = NULL;
        return status;
    }

    return EFI_SUCCESS;
}

static efi_status_t read_small_boot_file(
    struct efi_file_protocol *root,
    efi_char16_t *path,
    u8 *buffer,
    u64 *buffer_size)
{
    struct efi_file_protocol *file = NULL;
    efi_status_t status;

    if (root == NULL || root->open == NULL || path == NULL || buffer == NULL || buffer_size == NULL)
    {
        return LIMITLESS_EFI_LOCAL_ERROR;
    }

    status = root->open(root, &file, path, EFI_FILE_MODE_READ, 0u);
    if (status != EFI_SUCCESS || file == NULL || file->read == NULL)
    {
        close_file_if_present(file);
        return status;
    }

    status = file->read(file, buffer_size, buffer);
    close_file_if_present(file);
    return status;
}

static efi_status_t load_boot_file_into_buffer(
    struct efi_file_protocol *root,
    efi_char16_t *path,
    u8 *buffer,
    u64 capacity,
    u64 *bytes_read,
    u32 *token_out)
{
    struct efi_file_protocol *file = NULL;
    efi_status_t status;

    if (bytes_read != NULL)
    {
        *bytes_read = 0u;
    }
    if (token_out != NULL)
    {
        *token_out = 2166136261u;
    }

    if (root == NULL || root->open == NULL || path == NULL ||
        buffer == NULL || capacity == 0u || bytes_read == NULL || token_out == NULL)
    {
        return LIMITLESS_EFI_LOCAL_ERROR;
    }

    status = root->open(root, &file, path, EFI_FILE_MODE_READ, 0u);
    if (status != EFI_SUCCESS || file == NULL || file->read == NULL)
    {
        close_file_if_present(file);
        return status;
    }

    for (;;)
    {
        u64 chunk_size;

        if (*bytes_read >= capacity)
        {
            status = LIMITLESS_EFI_LOCAL_ERROR;
            break;
        }

        chunk_size = capacity - *bytes_read;
        if (chunk_size > 4096u)
        {
            chunk_size = 4096u;
        }

        status = file->read(file, &chunk_size, buffer + *bytes_read);
        if (status != EFI_SUCCESS)
        {
            break;
        }

        if (chunk_size == 0u)
        {
            break;
        }

        *token_out = checksum_update(*token_out, buffer + *bytes_read, chunk_size);
        *bytes_read += chunk_size;
    }

    close_file_if_present(file);
    return status;
}

static void write_boot_readme_line(struct efi_file_protocol *root, struct efi_system_table *system_table)
{
    u8 buffer[128] = { 0 };
    u64 buffer_size = sizeof(buffer);
    efi_status_t status = read_small_boot_file(root, g_boot_readme_path, buffer, &buffer_size);
    char line[192];
    u32 length = 0u;

    append_string(line, sizeof(line), &length, "[uefi] boot media read README.TXT bytes ");
    append_dec_u32(line, sizeof(line), &length, (u32)buffer_size);
    append_string(line, sizeof(line), &length, " token ");
    append_hex_u32(line, sizeof(line), &length, checksum_bytes(buffer, buffer_size));
    append_string(line, sizeof(line), &length, " prefix ");
    append_dec_u32(line, sizeof(line), &length,
        ascii_prefix_ok(buffer, buffer_size, "LimitlessOS x86_64 UEFI image"));
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, status);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void write_boot_manifest_line(
    struct efi_file_protocol *root,
    struct efi_system_table *system_table,
    struct uefi_boot_manifest *manifest)
{
    u8 buffer[512] = { 0 };
    u64 buffer_size = sizeof(buffer);
    efi_status_t status = read_small_boot_file(root, g_boot_manifest_path, buffer, &buffer_size);
    char line[224];
    u32 length = 0u;

    if (status == EFI_SUCCESS)
    {
        parse_boot_manifest(buffer, buffer_size, manifest);
    }

    append_string(line, sizeof(line), &length, "[uefi] boot manifest read BOOTMAN.TXT bytes ");
    append_dec_u32(line, sizeof(line), &length, (u32)buffer_size);
    append_string(line, sizeof(line), &length, " token ");
    append_hex_u32(line, sizeof(line), &length, checksum_bytes(buffer, buffer_size));
    append_string(line, sizeof(line), &length, " valid ");
    append_dec_u32(line, sizeof(line), &length, (manifest != NULL) ? manifest->valid : 0u);
    append_string(line, sizeof(line), &length, " kernel-bytes ");
    append_dec_u32(line, sizeof(line), &length, (manifest != NULL) ? manifest->kernel_bytes : 0u);
    append_string(line, sizeof(line), &length, " kernel-checksum ");
    append_hex_u32(line, sizeof(line), &length, (manifest != NULL) ? manifest->kernel_checksum : 0u);
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, status);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void write_loader_payload_line(
    struct efi_file_protocol *root,
    struct efi_system_table *system_table,
    const struct uefi_boot_manifest *manifest,
    struct uefi_loader_payload *payload)
{
    u64 bytes_read = 0u;
    u32 token = 0u;
    efi_status_t status = load_boot_file_into_buffer(
        root,
        g_boot_kernel_path,
        g_loader_kernel_buffer,
        sizeof(g_loader_kernel_buffer),
        &bytes_read,
        &token);
    u32 match = 0u;
    char line[192];
    u32 length = 0u;

    if (manifest != NULL && manifest->valid != 0u &&
        status == EFI_SUCCESS &&
        bytes_read == (u64)manifest->kernel_bytes &&
        token == manifest->kernel_checksum)
    {
        match = 1u;
    }

    if (payload != NULL)
    {
        payload->loaded = (status == EFI_SUCCESS && bytes_read > 0u) ? 1u : 0u;
        payload->match = match;
        payload->pages = (u32)((bytes_read + 4095u) / 4096u);
        payload->checksum = token;
        payload->bytes = (u32)bytes_read;
        payload->status = status;
        payload->base = (u64)(void *)g_loader_kernel_buffer;
        payload->capacity = sizeof(g_loader_kernel_buffer);
    }

    append_string(line, sizeof(line), &length, "[uefi] loader payload read KERNEL64.BIN bytes ");
    append_dec_u32(line, sizeof(line), &length, (u32)bytes_read);
    append_string(line, sizeof(line), &length, " token ");
    append_hex_u32(line, sizeof(line), &length, token);
    append_string(line, sizeof(line), &length, " expected-bytes ");
    append_dec_u32(line, sizeof(line), &length, (manifest != NULL) ? manifest->kernel_bytes : 0u);
    append_string(line, sizeof(line), &length, " expected-token ");
    append_hex_u32(line, sizeof(line), &length, (manifest != NULL) ? manifest->kernel_checksum : 0u);
    append_string(line, sizeof(line), &length, " match ");
    append_dec_u32(line, sizeof(line), &length, match);
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, status);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void write_loader_buffer_line(
    struct efi_system_table *system_table,
    const struct uefi_loader_payload *payload)
{
    char line[224];
    u32 length = 0u;

    append_string(line, sizeof(line), &length, "[uefi] loader buffer base ");
    append_hex_u64(line, sizeof(line), &length, (payload != NULL) ? payload->base : 0u);
    append_string(line, sizeof(line), &length, " capacity ");
    append_dec_u64(line, sizeof(line), &length, (payload != NULL) ? payload->capacity : 0u);
    append_string(line, sizeof(line), &length, " loaded ");
    append_dec_u32(line, sizeof(line), &length, (payload != NULL) ? payload->bytes : 0u);
    append_string(line, sizeof(line), &length, " pages ");
    append_dec_u32(line, sizeof(line), &length, (payload != NULL) ? payload->pages : 0u);
    append_string(line, sizeof(line), &length, " token ");
    append_hex_u32(line, sizeof(line), &length, (payload != NULL) ? payload->checksum : 0u);
    append_string(line, sizeof(line), &length, " match ");
    append_dec_u32(line, sizeof(line), &length, (payload != NULL) ? payload->match : 0u);
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, (payload != NULL) ? payload->status : LIMITLESS_EFI_LOCAL_ERROR);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void init_loader_payload(struct uefi_loader_payload *payload)
{
    if (payload == NULL)
    {
        return;
    }

    payload->loaded = 0u;
    payload->match = 0u;
    payload->pages = 0u;
    payload->checksum = 0u;
    payload->bytes = 0u;
    payload->status = LIMITLESS_EFI_LOCAL_ERROR;
    payload->base = 0u;
    payload->capacity = 0u;
}

static void write_boot_media_lines(
    efi_handle_t image_handle,
    struct efi_system_table *system_table,
    struct uefi_loader_payload *payload)
{
    struct efi_file_protocol *root = NULL;
    struct uefi_boot_manifest manifest = { 0u, 0u, 0u };
    const char *stage = "unknown";
    efi_status_t status;

    init_loader_payload(payload);

    status = open_boot_root(image_handle, system_table, &root, &stage);
    if (status != EFI_SUCCESS || root == NULL)
    {
        char line[160];
        u32 length = 0u;

        append_string(line, sizeof(line), &length, "[uefi] boot media read unavailable ");
        append_string(line, sizeof(line), &length, stage);
        append_char(line, sizeof(line), &length, ' ');
        append_hex_u64(line, sizeof(line), &length, status);
        append_char(line, sizeof(line), &length, '\n');
        write_line(system_table, line);
        close_file_if_present(root);
        return;
    }

    write_boot_readme_line(root, system_table);
    write_boot_manifest_line(root, system_table, &manifest);
    write_loader_payload_line(root, system_table, &manifest, payload);
    write_loader_buffer_line(system_table, payload);
    close_file_if_present(root);
}

static void summarize_memory_descriptor(
    const struct efi_memory_descriptor *descriptor,
    struct uefi_memory_map_summary *summary)
{
    if (descriptor == NULL || summary == NULL)
    {
        return;
    }

    summary->total_pages += descriptor->number_of_pages;

    if (descriptor->type == EFI_MEMORY_TYPE_CONVENTIONAL_MEMORY)
    {
        summary->conventional_pages += descriptor->number_of_pages;
        if (descriptor->number_of_pages > summary->largest_conventional_pages)
        {
            summary->largest_conventional_pages = descriptor->number_of_pages;
            summary->largest_conventional_base = descriptor->physical_start;
        }
        return;
    }

    if (descriptor->type == EFI_MEMORY_TYPE_LOADER_CODE ||
        descriptor->type == EFI_MEMORY_TYPE_LOADER_DATA)
    {
        summary->loader_pages += descriptor->number_of_pages;
        return;
    }

    if (descriptor->type == EFI_MEMORY_TYPE_BOOT_SERVICES_CODE ||
        descriptor->type == EFI_MEMORY_TYPE_BOOT_SERVICES_DATA)
    {
        summary->boot_services_pages += descriptor->number_of_pages;
        return;
    }

    if (descriptor->type == EFI_MEMORY_TYPE_RUNTIME_SERVICES_CODE ||
        descriptor->type == EFI_MEMORY_TYPE_RUNTIME_SERVICES_DATA)
    {
        summary->runtime_services_pages += descriptor->number_of_pages;
    }
}

static struct efi_memory_descriptor *memory_descriptor_at(
    u8 *memory_map,
    u64 index,
    u64 descriptor_size)
{
    return (struct efi_memory_descriptor *)(void *)(memory_map + (index * descriptor_size));
}

static void init_memory_map_summary(struct uefi_memory_map_summary *summary)
{
    if (summary == NULL)
    {
        return;
    }

    summary->descriptors = 0u;
    summary->descriptor_size = 0u;
    summary->descriptor_version = 0u;
    summary->map_key = 0u;
    summary->total_pages = 0u;
    summary->conventional_pages = 0u;
    summary->loader_pages = 0u;
    summary->boot_services_pages = 0u;
    summary->runtime_services_pages = 0u;
    summary->largest_conventional_base = 0u;
    summary->largest_conventional_pages = 0u;
    summary->status = LIMITLESS_EFI_LOCAL_ERROR;
}

static u32 capture_memory_map_summary(
    struct efi_system_table *system_table,
    struct uefi_memory_map_summary *summary)
{
    efi_uintn_t memory_map_size = sizeof(g_memory_map_buffer);
    efi_uintn_t map_key = 0u;
    efi_uintn_t descriptor_size = 0u;
    u32 descriptor_version = 0u;
    u64 index;

    init_memory_map_summary(summary);

    if (system_table == NULL || system_table->boot_services == NULL ||
        system_table->boot_services->get_memory_map == NULL || summary == NULL)
    {
        return 0u;
    }

    summary->status = system_table->boot_services->get_memory_map(
        &memory_map_size,
        (struct efi_memory_descriptor *)(void *)g_memory_map_buffer,
        &map_key,
        &descriptor_size,
        &descriptor_version);

    summary->descriptor_size = (u32)descriptor_size;
    summary->descriptor_version = descriptor_version;
    summary->map_key = map_key;

    if (summary->status == EFI_SUCCESS &&
        descriptor_size >= sizeof(struct efi_memory_descriptor) &&
        memory_map_size >= descriptor_size)
    {
        summary->descriptors = (u32)(memory_map_size / descriptor_size);
        for (index = 0u; index < summary->descriptors; ++index)
        {
            summarize_memory_descriptor(
                memory_descriptor_at(g_memory_map_buffer, index, descriptor_size),
                summary);
        }
    }

    return (summary->status == EFI_SUCCESS) ? 1u : 0u;
}

static void write_memory_map_summary_line(
    struct efi_system_table *system_table,
    const char *prefix,
    const struct uefi_memory_map_summary *summary)
{
    char line[288];
    u32 length = 0u;

    append_string(line, sizeof(line), &length, prefix);
    append_dec_u32(line, sizeof(line), &length, (summary != NULL) ? summary->descriptors : 0u);
    append_string(line, sizeof(line), &length, " desc-size ");
    append_dec_u32(line, sizeof(line), &length, (summary != NULL) ? summary->descriptor_size : 0u);
    append_string(line, sizeof(line), &length, " key ");
    append_hex_u64(line, sizeof(line), &length, (summary != NULL) ? summary->map_key : 0u);
    append_string(line, sizeof(line), &length, " version ");
    append_dec_u32(line, sizeof(line), &length, (summary != NULL) ? summary->descriptor_version : 0u);
    append_string(line, sizeof(line), &length, " total-pages ");
    append_dec_u64(line, sizeof(line), &length, (summary != NULL) ? summary->total_pages : 0u);
    append_string(line, sizeof(line), &length, " conventional-pages ");
    append_dec_u64(line, sizeof(line), &length, (summary != NULL) ? summary->conventional_pages : 0u);
    append_string(line, sizeof(line), &length, " loader-pages ");
    append_dec_u64(line, sizeof(line), &length, (summary != NULL) ? summary->loader_pages : 0u);
    append_string(line, sizeof(line), &length, " boot-pages ");
    append_dec_u64(line, sizeof(line), &length, (summary != NULL) ? summary->boot_services_pages : 0u);
    append_string(line, sizeof(line), &length, " runtime-pages ");
    append_dec_u64(line, sizeof(line), &length, (summary != NULL) ? summary->runtime_services_pages : 0u);
    append_string(line, sizeof(line), &length, " largest-conv ");
    append_hex_u64(line, sizeof(line), &length, (summary != NULL) ? summary->largest_conventional_base : 0u);
    append_char(line, sizeof(line), &length, '/');
    append_dec_u64(line, sizeof(line), &length, (summary != NULL) ? summary->largest_conventional_pages : 0u);
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, (summary != NULL) ? summary->status : LIMITLESS_EFI_LOCAL_ERROR);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void write_memory_map_line(
    struct efi_system_table *system_table,
    const char *prefix,
    struct uefi_memory_map_summary *summary)
{
    if (capture_memory_map_summary(system_table, summary) == 0u)
    {
        write_memory_map_summary_line(system_table, prefix, summary);
        return;
    }

    write_memory_map_summary_line(system_table, prefix, summary);
}

static void init_kernel_placement(struct uefi_kernel_placement *placement)
{
    if (placement == NULL)
    {
        return;
    }

    placement->planned = 0u;
    placement->allocated = 0u;
    placement->copied = 0u;
    placement->match = 0u;
    placement->pages = 0u;
    placement->bytes = 0u;
    placement->checksum = 0u;
    placement->requested_base = 0u;
    placement->physical_base = 0u;
    placement->source_base = 0u;
    placement->region_base = 0u;
    placement->region_pages = 0u;
    placement->alignment = LIMITLESS_UEFI_KERNEL_PLACEMENT_ALIGNMENT;
    placement->status = LIMITLESS_EFI_LOCAL_ERROR;
}

static void write_kernel_placement_line(
    struct efi_system_table *system_table,
    const struct uefi_loader_payload *payload,
    const struct uefi_memory_map_summary *summary,
    struct uefi_kernel_placement *placement)
{
    u64 required_bytes;
    u64 region_start;
    u64 region_end;
    efi_physical_address_t physical_base;
    char line[320];
    u32 length = 0u;

    init_kernel_placement(placement);

    if (placement != NULL && payload != NULL)
    {
        placement->pages = payload->pages;
        placement->bytes = payload->bytes;
        placement->checksum = payload->checksum;
        placement->source_base = payload->base;
    }

    if (system_table != NULL && system_table->boot_services != NULL &&
        system_table->boot_services->allocate_pages != NULL &&
        payload != NULL && payload->match != 0u && payload->pages > 0u &&
        summary != NULL && summary->status == EFI_SUCCESS &&
        summary->largest_conventional_pages >= payload->pages)
    {
        required_bytes = ((u64)payload->pages) * 4096u;
        region_start = summary->largest_conventional_base;
        region_end = region_start + (summary->largest_conventional_pages * 4096u);
        physical_base = align_up_u64(region_start, LIMITLESS_UEFI_KERNEL_PLACEMENT_ALIGNMENT);

        if ((physical_base + required_bytes) <= region_end)
        {
            placement->planned = 1u;
            placement->requested_base = physical_base;
            placement->physical_base = physical_base;
            placement->region_base = region_start;
            placement->region_pages = summary->largest_conventional_pages;
            placement->status = system_table->boot_services->allocate_pages(
                EFI_ALLOCATE_ADDRESS,
                EFI_MEMORY_TYPE_LOADER_DATA,
                payload->pages,
                &physical_base);
            placement->physical_base = physical_base;

            if (placement->status == EFI_SUCCESS && physical_base == placement->requested_base)
            {
                u8 *target = (u8 *)(void *)placement->physical_base;
                u64 page_bytes = ((u64)payload->pages) * 4096u;

                placement->allocated = 1u;
                copy_bytes(target, g_loader_kernel_buffer, payload->bytes);
                if (page_bytes > payload->bytes)
                {
                    zero_bytes(target + payload->bytes, page_bytes - payload->bytes);
                }

                placement->checksum = checksum_bytes(target, payload->bytes);
                placement->copied = 1u;
                if (placement->checksum == payload->checksum)
                {
                    placement->match = 1u;
                }
            }
        }
    }

    append_string(line, sizeof(line), &length, "[uefi] kernel placement planned ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->planned : 0u);
    append_string(line, sizeof(line), &length, " request ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->requested_base : 0u);
    append_string(line, sizeof(line), &length, " base ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->physical_base : 0u);
    append_string(line, sizeof(line), &length, " bytes ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->bytes : 0u);
    append_string(line, sizeof(line), &length, " pages ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->pages : 0u);
    append_string(line, sizeof(line), &length, " source ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->source_base : 0u);
    append_string(line, sizeof(line), &length, " region ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->region_base : 0u);
    append_char(line, sizeof(line), &length, '/');
    append_dec_u64(line, sizeof(line), &length, (placement != NULL) ? placement->region_pages : 0u);
    append_string(line, sizeof(line), &length, " align ");
    append_dec_u64(line, sizeof(line), &length, (placement != NULL) ? placement->alignment : 0u);
    append_string(line, sizeof(line), &length, " allocated ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->allocated : 0u);
    append_string(line, sizeof(line), &length, " copied ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->copied : 0u);
    append_string(line, sizeof(line), &length, " token ");
    append_hex_u32(line, sizeof(line), &length, (placement != NULL) ? placement->checksum : 0u);
    append_string(line, sizeof(line), &length, " match ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->match : 0u);
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->status : LIMITLESS_EFI_LOCAL_ERROR);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void init_linked_kernel_placement(struct uefi_linked_kernel_placement *placement)
{
    if (placement == NULL)
    {
        return;
    }

    placement->planned = 0u;
    placement->allocated = 0u;
    placement->copied = 0u;
    placement->match = 0u;
    placement->pages = 0u;
    placement->bytes = 0u;
    placement->checksum = 0u;
    placement->requested_base = LIMITLESS_UEFI_KERNEL_LINKED_BASE;
    placement->physical_base = 0u;
    placement->source_base = 0u;
    placement->linked_entry = LIMITLESS_UEFI_KERNEL_LINKED_ENTRY;
    placement->boot_info_base = LIMITLESS_UEFI_BOOT_INFO_ADDRESS;
    placement->page_table_root = LIMITLESS_UEFI_BOOT_PAGE_TABLE_ROOT;
    placement->identity_map_bytes = LIMITLESS_UEFI_BOOT_IDENTITY_BYTES;
    placement->status = LIMITLESS_EFI_LOCAL_ERROR;
}

static void write_linked_kernel_placement_line(
    struct efi_system_table *system_table,
    const struct uefi_loader_payload *payload,
    struct uefi_linked_kernel_placement *placement)
{
    efi_physical_address_t physical_base = LIMITLESS_UEFI_KERNEL_LINKED_BASE;
    char line[384];
    u32 length = 0u;

    init_linked_kernel_placement(placement);

    if (placement != NULL && payload != NULL)
    {
        placement->pages = payload->pages;
        placement->bytes = payload->bytes;
        placement->checksum = payload->checksum;
        placement->source_base = payload->base;
    }

    if (system_table != NULL && system_table->boot_services != NULL &&
        system_table->boot_services->allocate_pages != NULL &&
        payload != NULL && payload->match != 0u && payload->pages > 0u &&
        placement != NULL)
    {
        placement->planned = 1u;
        placement->physical_base = physical_base;
        placement->status = system_table->boot_services->allocate_pages(
            EFI_ALLOCATE_ADDRESS,
            EFI_MEMORY_TYPE_LOADER_DATA,
            payload->pages,
            &physical_base);
        placement->physical_base = physical_base;

        if (placement->status == EFI_SUCCESS && physical_base == LIMITLESS_UEFI_KERNEL_LINKED_BASE)
        {
            u8 *target = (u8 *)(void *)placement->physical_base;
            u64 page_bytes = ((u64)payload->pages) * 4096u;

            placement->allocated = 1u;
            copy_bytes(target, g_loader_kernel_buffer, payload->bytes);
            if (page_bytes > payload->bytes)
            {
                zero_bytes(target + payload->bytes, page_bytes - payload->bytes);
            }

            placement->checksum = checksum_bytes(target, payload->bytes);
            placement->copied = 1u;
            if (placement->checksum == payload->checksum)
            {
                placement->match = 1u;
            }
        }
    }

    append_string(line, sizeof(line), &length, "[uefi] linked kernel placement planned ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->planned : 0u);
    append_string(line, sizeof(line), &length, " request ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->requested_base : 0u);
    append_string(line, sizeof(line), &length, " base ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->physical_base : 0u);
    append_string(line, sizeof(line), &length, " bytes ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->bytes : 0u);
    append_string(line, sizeof(line), &length, " pages ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->pages : 0u);
    append_string(line, sizeof(line), &length, " entry ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->linked_entry : 0u);
    append_string(line, sizeof(line), &length, " boot-info ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->boot_info_base : 0u);
    append_string(line, sizeof(line), &length, " page-root ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->page_table_root : 0u);
    append_string(line, sizeof(line), &length, " identity ");
    append_dec_u64(line, sizeof(line), &length, (placement != NULL) ? placement->identity_map_bytes : 0u);
    append_string(line, sizeof(line), &length, " source ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->source_base : 0u);
    append_string(line, sizeof(line), &length, " allocated ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->allocated : 0u);
    append_string(line, sizeof(line), &length, " copied ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->copied : 0u);
    append_string(line, sizeof(line), &length, " token ");
    append_hex_u32(line, sizeof(line), &length, (placement != NULL) ? placement->checksum : 0u);
    append_string(line, sizeof(line), &length, " match ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->match : 0u);
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->status : LIMITLESS_EFI_LOCAL_ERROR);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static u32 uefi_pages_to_kib32(u64 pages)
{
    u64 kib = pages * 4u;

    if (kib > 0xFFFFFFFFull)
    {
        return 0xFFFFFFFFu;
    }

    return (u32)kib;
}

static u32 uefi_boot_flags(const struct uefi_framebuffer_handoff *framebuffer)
{
    u32 flags = LIMITLESS_BOOT_FLAG_PROTECTED_MODE |
                LIMITLESS_BOOT_FLAG_LONG_MODE |
                LIMITLESS_BOOT_FLAG_PAGING |
                LIMITLESS_BOOT_FLAG_IDENTITY_MAP |
                LIMITLESS_BOOT_FLAG_HIGH_HALF_ALIAS;

    if (framebuffer != NULL && framebuffer->available != 0u && framebuffer->mapped != 0u)
    {
        flags |= LIMITLESS_BOOT_FLAG_FRAMEBUFFER;
    }

    return flags;
}

static void map_framebuffer_handoff(
    volatile u64 *pdpt,
    volatile u64 *identity_pd,
    volatile u64 *framebuffer_pd,
    struct uefi_framebuffer_handoff *framebuffer)
{
    u64 map_start;
    u64 map_end;
    u64 page_base;
    u32 pdpt_index;
    u32 pd_start;
    u32 entries;
    u32 index;
    volatile u64 *target_pd;

    if (pdpt == NULL || identity_pd == NULL || framebuffer_pd == NULL ||
        framebuffer == NULL || framebuffer->available == 0u ||
        framebuffer->bytes == 0ull ||
        framebuffer->bytes > (0xFFFFFFFFFFFFFFFFull - framebuffer->base))
    {
        return;
    }

    map_start = framebuffer->base & ~(LIMITLESS_UEFI_LARGE_PAGE_BYTES - 1ull);
    map_end = (framebuffer->base + framebuffer->bytes + LIMITLESS_UEFI_LARGE_PAGE_BYTES - 1ull) &
              ~(LIMITLESS_UEFI_LARGE_PAGE_BYTES - 1ull);
    if (map_end <= map_start || (map_start >> 39) != 0ull)
    {
        return;
    }

    pdpt_index = (u32)((map_start >> 30) & 0x1FFull);
    pd_start = (u32)((map_start >> 21) & 0x1FFull);
    entries = (u32)((map_end - map_start) / LIMITLESS_UEFI_LARGE_PAGE_BYTES);
    if (entries == 0u || entries > (512u - pd_start))
    {
        entries = 512u - pd_start;
    }
    if (entries == 0u)
    {
        return;
    }

    target_pd = identity_pd;
    if (pdpt_index != 0u)
    {
        target_pd = framebuffer_pd;
        pdpt[pdpt_index] = LIMITLESS_UEFI_BOOT_FRAMEBUFFER_PD_ADDRESS |
                           LIMITLESS_UEFI_PAGE_PRESENT |
                           LIMITLESS_UEFI_PAGE_WRITABLE;
    }

    for (index = 0u; index < entries; ++index)
    {
        page_base = map_start + ((u64)index * LIMITLESS_UEFI_LARGE_PAGE_BYTES);
        target_pd[pd_start + index] = page_base |
                                      LIMITLESS_UEFI_PAGE_PRESENT |
                                      LIMITLESS_UEFI_PAGE_WRITABLE |
                                      LIMITLESS_UEFI_PAGE_LARGE;
    }

    framebuffer->mapped = 1u;
    framebuffer->map_pdpt_index = pdpt_index;
    framebuffer->map_pd_start = pd_start;
    framebuffer->map_entries = entries;
    framebuffer->map_bytes = (u64)entries * LIMITLESS_UEFI_LARGE_PAGE_BYTES;
}

static void init_boot_handoff(struct uefi_boot_handoff *handoff)
{
    if (handoff == NULL)
    {
        return;
    }

    handoff->planned = 0u;
    handoff->allocated = 0u;
    handoff->built = 0u;
    handoff->ready = 0u;
    handoff->pages = LIMITLESS_UEFI_BOOT_HANDOFF_PAGES;
    handoff->identity_entries = (u32)LIMITLESS_UEFI_BOOT_IDENTITY_ENTRIES;
    handoff->kernel_sectors = 0u;
    handoff->trampoline_bytes = 0u;
    handoff->trampoline_ready = 0u;
    handoff->token = 0u;
    handoff->requested_base = LIMITLESS_UEFI_BOOT_HANDOFF_BASE;
    handoff->physical_base = 0u;
    handoff->pml4 = LIMITLESS_UEFI_BOOT_PAGE_TABLE_ROOT;
    handoff->pdpt = LIMITLESS_UEFI_BOOT_PDPT_ADDRESS;
    handoff->pd = LIMITLESS_UEFI_BOOT_PD_ADDRESS;
    handoff->high_pdpt = LIMITLESS_UEFI_BOOT_HIGH_PDPT_ADDRESS;
    handoff->runtime_pd = LIMITLESS_UEFI_BOOT_RUNTIME_PD_ADDRESS;
    handoff->runtime_pt = LIMITLESS_UEFI_BOOT_RUNTIME_PT_ADDRESS;
    handoff->user_runtime_pt = LIMITLESS_UEFI_BOOT_USER_RUNTIME_PT_ADDRESS;
    handoff->user_stack_page = LIMITLESS_UEFI_BOOT_USER_STACK_PAGE_ADDRESS;
    handoff->trampoline = LIMITLESS_UEFI_BOOT_TRAMPOLINE_ADDRESS;
    handoff->framebuffer_pd = LIMITLESS_UEFI_BOOT_FRAMEBUFFER_PD_ADDRESS;
    handoff->boot_info_base = LIMITLESS_UEFI_BOOT_INFO_ADDRESS;
    handoff->identity_map_bytes = LIMITLESS_UEFI_BOOT_IDENTITY_BYTES;
    handoff->linked_entry = LIMITLESS_UEFI_KERNEL_LINKED_ENTRY;
    handoff->status = LIMITLESS_EFI_LOCAL_ERROR;
}

static void write_boot_handoff_line(
    struct efi_system_table *system_table,
    const struct uefi_loader_payload *payload,
    const struct uefi_memory_map_summary *summary,
    const struct uefi_linked_kernel_placement *linked_placement,
    const struct uefi_acpi_handoff *acpi,
    struct uefi_framebuffer_handoff *framebuffer,
    struct uefi_boot_handoff *handoff)
{
    efi_physical_address_t physical_base = LIMITLESS_UEFI_BOOT_HANDOFF_BASE;
    char line[896];
    u32 length = 0u;

    init_boot_handoff(handoff);

    if (handoff != NULL && payload != NULL)
    {
        handoff->kernel_sectors = (payload->bytes + 511u) / 512u;
    }

    if (system_table != NULL && system_table->boot_services != NULL &&
        system_table->boot_services->allocate_pages != NULL &&
        payload != NULL && payload->match != 0u &&
        linked_placement != NULL && linked_placement->match != 0u &&
        handoff != NULL)
    {
        handoff->planned = 1u;
        handoff->physical_base = physical_base;
        handoff->status = system_table->boot_services->allocate_pages(
            EFI_ALLOCATE_ADDRESS,
            EFI_MEMORY_TYPE_LOADER_DATA,
            LIMITLESS_UEFI_BOOT_HANDOFF_PAGES,
            &physical_base);
        handoff->physical_base = physical_base;

        if (handoff->status == EFI_SUCCESS && physical_base == LIMITLESS_UEFI_BOOT_HANDOFF_BASE)
        {
            volatile u64 *pml4 = (volatile u64 *)(u64)LIMITLESS_UEFI_BOOT_PAGE_TABLE_ROOT;
            volatile u64 *pdpt = (volatile u64 *)(u64)LIMITLESS_UEFI_BOOT_PDPT_ADDRESS;
            volatile u64 *pd = (volatile u64 *)(u64)LIMITLESS_UEFI_BOOT_PD_ADDRESS;
            volatile u64 *high_pdpt = (volatile u64 *)(u64)LIMITLESS_UEFI_BOOT_HIGH_PDPT_ADDRESS;
            volatile u64 *framebuffer_pd = (volatile u64 *)(u64)LIMITLESS_UEFI_BOOT_FRAMEBUFFER_PD_ADDRESS;
            struct boot_info *boot_info = (struct boot_info *)(void *)LIMITLESS_UEFI_BOOT_INFO_ADDRESS;
            u8 *trampoline = (u8 *)(void *)LIMITLESS_UEFI_BOOT_TRAMPOLINE_ADDRESS;
            u32 identity_index;
            u32 identity_ready = 1u;

            handoff->allocated = 1u;
            zero_bytes((u8 *)(void *)LIMITLESS_UEFI_BOOT_HANDOFF_BASE, LIMITLESS_UEFI_BOOT_HANDOFF_PAGES * 4096u);

            pml4[0] = LIMITLESS_UEFI_BOOT_PDPT_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE;
            pml4[LIMITLESS_UEFI_HIGH_HALF_PML4_INDEX] =
                LIMITLESS_UEFI_BOOT_HIGH_PDPT_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE;
            pdpt[0] = LIMITLESS_UEFI_BOOT_PD_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE;
            high_pdpt[LIMITLESS_UEFI_HIGH_HALF_PDPT_INDEX] =
                LIMITLESS_UEFI_BOOT_PD_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE;
            for (identity_index = 0u; identity_index < (u32)LIMITLESS_UEFI_BOOT_IDENTITY_ENTRIES; ++identity_index)
            {
                pd[identity_index] = ((u64)identity_index * LIMITLESS_UEFI_LARGE_PAGE_BYTES) |
                    LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE | LIMITLESS_UEFI_PAGE_LARGE;
            }
            map_framebuffer_handoff(pdpt, pd, framebuffer_pd, framebuffer);

            boot_info->magic = LIMITLESS_BOOT_INFO_MAGIC;
            boot_info->boot_drive = LIMITLESS_UEFI_BOOT_DRIVE_MARKER;
            boot_info->conventional_memory_kb = (summary != NULL) ? uefi_pages_to_kib32(summary->conventional_pages) : 0u;
            boot_info->extended_memory_kb = (summary != NULL) ? uefi_pages_to_kib32(summary->total_pages) : 0u;
            boot_info->kernel_load_address = (u32)LIMITLESS_UEFI_KERNEL_LINKED_BASE;
            boot_info->kernel_sector_count = handoff->kernel_sectors;
            boot_info->architecture_bits = 64u;
            boot_info->bootstrap_flags = uefi_boot_flags(framebuffer);
            boot_info->page_table_root = (u32)LIMITLESS_UEFI_BOOT_PAGE_TABLE_ROOT;
            boot_info->identity_map_bytes = (u32)LIMITLESS_UEFI_BOOT_IDENTITY_BYTES;
            boot_info->framebuffer_base = (framebuffer != NULL && framebuffer->mapped != 0u) ? framebuffer->base : 0ull;
            boot_info->framebuffer_bytes = (framebuffer != NULL && framebuffer->mapped != 0u) ? framebuffer->bytes : 0ull;
            boot_info->framebuffer_width = (framebuffer != NULL && framebuffer->mapped != 0u) ? framebuffer->width : 0u;
            boot_info->framebuffer_height = (framebuffer != NULL && framebuffer->mapped != 0u) ? framebuffer->height : 0u;
            boot_info->framebuffer_pixels_per_scanline =
                (framebuffer != NULL && framebuffer->mapped != 0u) ? framebuffer->pixels_per_scanline : 0u;
            boot_info->framebuffer_format =
                (framebuffer != NULL && framebuffer->mapped != 0u) ? framebuffer->pixel_format : 0u;
            boot_info->framebuffer_firmware_token =
                (framebuffer != NULL && framebuffer->mapped != 0u) ? framebuffer->draw_token : 0u;
            boot_info->acpi_rsdp = (acpi != NULL) ? acpi->rsdp : 0ull;
            boot_info->pci_ecam_base = (acpi != NULL && acpi->mcfg_found != 0u) ? acpi->ecam_base : 0ull;
            boot_info->pci_ecam_flags = (acpi != NULL) ? acpi->flags : 0u;
            boot_info->pci_ecam_segment = (acpi != NULL && acpi->mcfg_found != 0u) ? acpi->segment : 0u;
            boot_info->pci_ecam_bus_start = (acpi != NULL && acpi->mcfg_found != 0u) ? acpi->bus_start : 0u;
            boot_info->pci_ecam_bus_end = (acpi != NULL && acpi->mcfg_found != 0u) ? acpi->bus_end : 0u;
            boot_info->apic_lapic_base = (acpi != NULL && acpi->lapic_found != 0u) ? acpi->lapic_base : 0ull;
            boot_info->apic_ioapic_base = (acpi != NULL && acpi->ioapic_found != 0u) ? acpi->ioapic_base : 0ull;
            boot_info->apic_ioapic_id = (acpi != NULL && acpi->ioapic_found != 0u) ? acpi->ioapic_id : 0u;
            boot_info->apic_ioapic_gsi_base =
                (acpi != NULL && acpi->ioapic_found != 0u) ? acpi->ioapic_gsi_base : 0u;
            boot_info->apic_interrupt_override_scanned =
                (acpi != NULL && acpi->madt_found != 0u) ? acpi->interrupt_override_scanned : 0u;
            boot_info->apic_interrupt_override_count =
                (acpi != NULL && acpi->madt_found != 0u) ? acpi->interrupt_override_count : 0u;
            boot_info->apic_interrupt_override_valid_mask =
                (acpi != NULL && acpi->madt_found != 0u) ? acpi->interrupt_override_valid_mask : 0u;
            for (identity_index = 0u; identity_index < LIMITLESS_UEFI_APIC_OVERRIDE_SLOTS; ++identity_index)
            {
                boot_info->apic_interrupt_override_source[identity_index] =
                    (acpi != NULL && acpi->madt_found != 0u) ? acpi->interrupt_override_source[identity_index] : 0u;
                boot_info->apic_interrupt_override_gsi[identity_index] =
                    (acpi != NULL && acpi->madt_found != 0u) ? acpi->interrupt_override_gsi[identity_index] : 0u;
                boot_info->apic_interrupt_override_flags[identity_index] =
                    (acpi != NULL && acpi->madt_found != 0u) ? acpi->interrupt_override_flags[identity_index] : 0u;
            }

            copy_bytes(trampoline, g_kernel_entry_trampoline, sizeof(g_kernel_entry_trampoline));
            handoff->trampoline_bytes = (u32)sizeof(g_kernel_entry_trampoline);
            if (checksum_bytes(trampoline, sizeof(g_kernel_entry_trampoline)) ==
                checksum_bytes(g_kernel_entry_trampoline, sizeof(g_kernel_entry_trampoline)))
            {
                handoff->trampoline_ready = 1u;
            }

            for (identity_index = 0u; identity_index < (u32)LIMITLESS_UEFI_BOOT_IDENTITY_ENTRIES; ++identity_index)
            {
                u64 expected_identity = ((u64)identity_index * LIMITLESS_UEFI_LARGE_PAGE_BYTES) |
                    LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE | LIMITLESS_UEFI_PAGE_LARGE;
                if (pd[identity_index] != expected_identity)
                {
                    identity_ready = 0u;
                }
            }

            if (pml4[0] == (LIMITLESS_UEFI_BOOT_PDPT_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE) &&
                pml4[LIMITLESS_UEFI_HIGH_HALF_PML4_INDEX] ==
                    (LIMITLESS_UEFI_BOOT_HIGH_PDPT_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE) &&
                pdpt[0] == (LIMITLESS_UEFI_BOOT_PD_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE) &&
                high_pdpt[LIMITLESS_UEFI_HIGH_HALF_PDPT_INDEX] ==
                    (LIMITLESS_UEFI_BOOT_PD_ADDRESS | LIMITLESS_UEFI_PAGE_PRESENT | LIMITLESS_UEFI_PAGE_WRITABLE) &&
                identity_ready != 0u &&
                boot_info->magic == LIMITLESS_BOOT_INFO_MAGIC &&
                boot_info->architecture_bits == 64u &&
                boot_info->bootstrap_flags == uefi_boot_flags(framebuffer) &&
                boot_info->page_table_root == (u32)LIMITLESS_UEFI_BOOT_PAGE_TABLE_ROOT &&
                ((framebuffer == NULL) || (framebuffer->available == 0u) || (framebuffer->mapped != 0u)) &&
                handoff->trampoline_ready != 0u)
            {
                handoff->built = 1u;
                handoff->ready = 1u;
            }

            handoff->token = checksum_bytes(
                (const u8 *)(void *)LIMITLESS_UEFI_BOOT_HANDOFF_BASE,
                LIMITLESS_UEFI_BOOT_HANDOFF_PAGES * 4096u);
        }
    }

    append_string(line, sizeof(line), &length, "[uefi] boot handoff tables planned ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->planned : 0u);
    append_string(line, sizeof(line), &length, " request ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->requested_base : 0u);
    append_string(line, sizeof(line), &length, " base ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->physical_base : 0u);
    append_string(line, sizeof(line), &length, " pages ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->pages : 0u);
    append_string(line, sizeof(line), &length, " pml4 ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->pml4 : 0u);
    append_string(line, sizeof(line), &length, " pdpt ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->pdpt : 0u);
    append_string(line, sizeof(line), &length, " pd ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->pd : 0u);
    append_string(line, sizeof(line), &length, " high-pdpt ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->high_pdpt : 0u);
    append_string(line, sizeof(line), &length, " framebuffer-pd ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->framebuffer_pd : 0u);
    append_string(line, sizeof(line), &length, " boot-info ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->boot_info_base : 0u);
    append_string(line, sizeof(line), &length, " trampoline ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->trampoline : 0u);
    append_string(line, sizeof(line), &length, " tramp-bytes ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->trampoline_bytes : 0u);
    append_string(line, sizeof(line), &length, " tramp-ready ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->trampoline_ready : 0u);
    append_string(line, sizeof(line), &length, " identity ");
    append_dec_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->identity_map_bytes : 0u);
    append_string(line, sizeof(line), &length, " sectors ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->kernel_sectors : 0u);
    append_string(line, sizeof(line), &length, " entries ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->identity_entries : 0u);
    append_string(line, sizeof(line), &length, " flags ");
    append_hex_u32(line, sizeof(line), &length, uefi_boot_flags(framebuffer));
    append_string(line, sizeof(line), &length, " fb-base ");
    append_hex_u64(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->base : 0u);
    append_string(line, sizeof(line), &length, " fb-bytes ");
    append_hex_u64(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->bytes : 0u);
    append_string(line, sizeof(line), &length, " fb-geometry ");
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->width : 0u);
    append_char(line, sizeof(line), &length, 'x');
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->height : 0u);
    append_string(line, sizeof(line), &length, " fb-ppsl ");
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->pixels_per_scanline : 0u);
    append_string(line, sizeof(line), &length, " fb-format ");
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->pixel_format : 0u);
    append_string(line, sizeof(line), &length, " fb-map-pdpt ");
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->map_pdpt_index : 0u);
    append_string(line, sizeof(line), &length, " fb-map-start ");
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->map_pd_start : 0u);
    append_string(line, sizeof(line), &length, " fb-map-entries ");
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->map_entries : 0u);
    append_string(line, sizeof(line), &length, " fb-map-bytes ");
    append_dec_u64(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->map_bytes : 0u);
    append_string(line, sizeof(line), &length, " fb-mapped ");
    append_dec_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->mapped : 0u);
    append_string(line, sizeof(line), &length, " fb-token ");
    append_hex_u32(line, sizeof(line), &length, (framebuffer != NULL) ? framebuffer->draw_token : 0u);
    append_string(line, sizeof(line), &length, " token ");
    append_hex_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->token : 0u);
    append_string(line, sizeof(line), &length, " allocated ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->allocated : 0u);
    append_string(line, sizeof(line), &length, " built ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->built : 0u);
    append_string(line, sizeof(line), &length, " ready ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->ready : 0u);
    append_string(line, sizeof(line), &length, " status ");
    append_hex_u64(line, sizeof(line), &length, (handoff != NULL) ? handoff->status : LIMITLESS_EFI_LOCAL_ERROR);
    append_char(line, sizeof(line), &length, '\n');
    write_line(system_table, line);
}

static void halt_after_firmware_exit(void)
{
    for (;;)
    {
        __asm__ __volatile__("cli; hlt");
    }
}

static void debug_write_exit_boot_services_line(
    efi_status_t status,
    const struct uefi_memory_map_summary *summary,
    const struct uefi_kernel_placement *placement)
{
    char line[320];
    u32 length = 0u;

    append_string(line, sizeof(line), &length, "[uefi] exit boot services status ");
    append_hex_u64(line, sizeof(line), &length, status);
    append_string(line, sizeof(line), &length, " key ");
    append_hex_u64(line, sizeof(line), &length, (summary != NULL) ? summary->map_key : 0u);
    append_string(line, sizeof(line), &length, " descriptors ");
    append_dec_u32(line, sizeof(line), &length, (summary != NULL) ? summary->descriptors : 0u);
    append_string(line, sizeof(line), &length, " desc-size ");
    append_dec_u32(line, sizeof(line), &length, (summary != NULL) ? summary->descriptor_size : 0u);
    append_string(line, sizeof(line), &length, " kernel-base ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->physical_base : 0u);
    append_string(line, sizeof(line), &length, " kernel-bytes ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->bytes : 0u);
    append_string(line, sizeof(line), &length, " kernel-pages ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->pages : 0u);
    append_string(line, sizeof(line), &length, " placement-match ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->match : 0u);
    append_string(line, sizeof(line), &length, " firmware-offline ");
    append_dec_u32(line, sizeof(line), &length, (status == EFI_SUCCESS) ? 1u : 0u);
    append_string(line, sizeof(line), &length, " handoff-ready ");
    append_dec_u32(line, sizeof(line), &length,
        (status == EFI_SUCCESS && placement != NULL && placement->match != 0u) ? 1u : 0u);
    append_char(line, sizeof(line), &length, '\n');
    debug_write_string(line);
}

static void debug_write_kernel_entry_guard_line(
    const struct uefi_linked_kernel_placement *placement,
    const struct uefi_boot_handoff *handoff)
{
    char line[384];
    u32 length = 0u;
    u32 jump_ready = (placement != NULL && placement->match != 0u &&
        handoff != NULL && handoff->ready != 0u) ? 1u : 0u;

    append_string(line, sizeof(line), &length, "[uefi] kernel entry guard linked-base ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->physical_base : 0u);
    append_string(line, sizeof(line), &length, " linked-match ");
    append_dec_u32(line, sizeof(line), &length, (placement != NULL) ? placement->match : 0u);
    append_string(line, sizeof(line), &length, " entry ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->linked_entry : 0u);
    append_string(line, sizeof(line), &length, " boot-info ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->boot_info_base : 0u);
    append_string(line, sizeof(line), &length, " page-root ");
    append_hex_u64(line, sizeof(line), &length, (placement != NULL) ? placement->page_table_root : 0u);
    append_string(line, sizeof(line), &length, " identity ");
    append_dec_u64(line, sizeof(line), &length, (placement != NULL) ? placement->identity_map_bytes : 0u);
    append_string(line, sizeof(line), &length, " tables-ready ");
    append_dec_u32(line, sizeof(line), &length, (handoff != NULL) ? handoff->ready : 0u);
    append_string(line, sizeof(line), &length, " jump-ready ");
    append_dec_u32(line, sizeof(line), &length, jump_ready);
    append_string(line, sizeof(line), &length, " reason ");
    append_string(line, sizeof(line), &length,
        (jump_ready != 0u) ? "handoff-ready\n" : "tables-blocked\n");
    debug_write_string(line);
}

static void jump_to_linked_kernel_entry(const struct uefi_boot_handoff *handoff)
{
    typedef void (*uefi_kernel_entry_trampoline_fn)(void);
    uefi_kernel_entry_trampoline_fn trampoline;

    if (handoff == NULL || handoff->ready == 0u || handoff->trampoline == 0u)
    {
        debug_write_string("[uefi] firmware services offline; kernel handoff unavailable; halting\n");
        halt_after_firmware_exit();
    }

    debug_write_string("[uefi] firmware services offline; jumping to x64 kernel entry\n");
    trampoline = (uefi_kernel_entry_trampoline_fn)(u64)handoff->trampoline;
    trampoline();
    debug_write_string("[uefi] kernel entry returned unexpectedly\n");
    halt_after_firmware_exit();
}

static efi_status_t exit_boot_services_for_handoff(
    efi_handle_t image_handle,
    struct efi_system_table *system_table,
    const struct uefi_kernel_placement *placement,
    const struct uefi_linked_kernel_placement *linked_placement,
    const struct uefi_boot_handoff *handoff)
{
    struct uefi_memory_map_summary final_map;
    efi_status_t status = LIMITLESS_EFI_LOCAL_ERROR;
    u32 attempts;

    if (system_table == NULL || system_table->boot_services == NULL ||
        system_table->boot_services->exit_boot_services == NULL ||
        placement == NULL || placement->match == 0u)
    {
        debug_write_exit_boot_services_line(status, NULL, placement);
        return status;
    }

    for (attempts = 0u; attempts < 2u; ++attempts)
    {
        if (capture_memory_map_summary(system_table, &final_map) == 0u)
        {
            status = final_map.status;
            break;
        }

        status = system_table->boot_services->exit_boot_services(image_handle, final_map.map_key);
        if (status == EFI_SUCCESS)
        {
            debug_write_exit_boot_services_line(status, &final_map, placement);
            debug_write_kernel_entry_guard_line(linked_placement, handoff);
            jump_to_linked_kernel_entry(handoff);
        }
    }

    debug_write_exit_boot_services_line(status, &final_map, placement);
    return status;
}

efi_status_t efi_main(efi_handle_t image_handle, struct efi_system_table *system_table)
{
    struct uefi_loader_payload payload;
    struct uefi_memory_map_summary memory_map;
    struct uefi_memory_map_summary handoff_memory_map;
    struct uefi_kernel_placement placement;
    struct uefi_linked_kernel_placement linked_placement;
    struct uefi_boot_handoff boot_handoff;
    struct uefi_framebuffer_handoff framebuffer;
    struct uefi_acpi_handoff acpi;

    services64_init();
    serial_init();
    write_line(system_table, "LimitlessOS x86_64 UEFI scaffold\n");
    write_line(system_table, "[uefi] firmware boot active\n");
    write_line(system_table, "[uefi] arch x86_64\n");
    write_line(system_table, "[uefi] removable path EFI/BOOT/BOOTX64.EFI\n");
    write_line(system_table, "[uefi] target modern 64-bit systems\n");
    write_line(system_table, "[uefi] plan shared services, installer media, and kernel convergence\n");
    write_line(system_table, "[uefi] bootstrap kind " LIMITLESS_UEFI_BOOTSTRAP_KIND "\n");
    write_package_archive_line(system_table);
    write_service_namespace_line(system_table);
    write_gop_framebuffer_line(system_table, &framebuffer);
    write_boot_media_lines(image_handle, system_table, &payload);
    discover_acpi_tables(system_table, &acpi);
    write_memory_map_line(system_table, "[uefi] memory map descriptors ", &memory_map);
    write_kernel_placement_line(system_table, &payload, &memory_map, &placement);
    write_linked_kernel_placement_line(system_table, &payload, &linked_placement);
    write_boot_handoff_line(system_table, &payload, &memory_map, &linked_placement, &acpi, &framebuffer, &boot_handoff);
    write_memory_map_line(system_table, "[uefi] handoff memory map descriptors ", &handoff_memory_map);
    exit_boot_services_for_handoff(image_handle, system_table, &placement, &linked_placement, &boot_handoff);

    return EFI_SUCCESS;
}
