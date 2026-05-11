#include "block_x64.h"

#include "capability_x64.h"
#include "launch_x64.h"
#include "principal_x64.h"
#include "services.h"
#include "services_x64.h"
#include "x64.h"

enum
{
    ATA_PRIMARY_DATA = 0x1F0u,
    ATA_PRIMARY_SECTOR_COUNT = 0x1F2u,
    ATA_PRIMARY_LBA_LOW = 0x1F3u,
    ATA_PRIMARY_LBA_MID = 0x1F4u,
    ATA_PRIMARY_LBA_HIGH = 0x1F5u,
    ATA_PRIMARY_DRIVE = 0x1F6u,
    ATA_PRIMARY_STATUS_COMMAND = 0x1F7u,
    ATA_PRIMARY_ALT_STATUS = 0x3F6u,

    ATA_STATUS_ERR = 0x01u,
    ATA_STATUS_DRQ = 0x08u,
    ATA_STATUS_DRDY = 0x40u,
    ATA_STATUS_BSY = 0x80u,
    ATA_STATUS_FLOATING_BUS = 0xFFu,
    ATA_COMMAND_READ_SECTORS = 0x20u,
    ATA_DRIVE_MASTER_LBA = 0xE0u,
    ATA_LBA28_LIMIT = 0x10000000u,
    ATA_POLL_LIMIT = 100000u,

    BLOCK64_KERNEL_HIGH_BASE_LOW32 = 0x80000000u,
    BLOCK64_KERNEL_HIGH_BASE_HIGH32 = 0xFFFFFFFFu
};

static u8 g_block64_sector[BLOCK64_SECTOR_BYTES];
static u32 g_available = 0u;
static u32 g_last_status = 0u;
static u32 g_read_count = 0u;
static u32 g_byte_count = 0u;
static u32 g_denial_count = 0u;
static u32 g_unavailable_count = 0u;
static u32 g_last_lba = 0u;
static u32 g_last_token = 0u;

static void block64_copy(void *destination, const void *source, u32 byte_count)
{
    u8 *dest = (u8 *)destination;
    const u8 *src = (const u8 *)source;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        dest[index] = src[index];
    }
}

static void block64_zero(void *address, u32 byte_count)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static u32 block64_token_bytes(const u8 *bytes, u32 byte_count)
{
    u32 token = 2166136261u;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        token ^= (u32)bytes[index];
        token *= 16777619u;
    }

    return token;
}

static int block64_range_overflows(u64 address, u32 byte_count)
{
    u64 end;

    if (byte_count == 0u)
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return end < address;
}

static int block64_address_is_kernel_high(u64 address, u32 byte_count)
{
    u64 end;

    if (block64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return ((u32)(address >> 32) >= BLOCK64_KERNEL_HIGH_BASE_HIGH32)
        && ((u32)address >= BLOCK64_KERNEL_HIGH_BASE_LOW32)
        && (end >= address);
}

static int block64_address_is_user_stack(u64 address, u32 byte_count)
{
    u64 stack_base = (u64)(LAUNCH64_USER_STACK_TOP - LAUNCH64_USER_STACK_BYTES);
    u64 stack_top = (u64)LAUNCH64_USER_STACK_TOP;
    u64 end;

    if (block64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= stack_base) && (end <= stack_top);
}

static int block64_address_writable(u64 address, u32 byte_count)
{
    if ((address == 0ull) || (byte_count == 0u))
    {
        return 0;
    }

    return block64_address_is_kernel_high(address, byte_count)
        || block64_address_is_user_stack(address, byte_count);
}

static void block64_io_delay(void)
{
    (void)inb((u16)ATA_PRIMARY_ALT_STATUS);
    (void)inb((u16)ATA_PRIMARY_ALT_STATUS);
    (void)inb((u16)ATA_PRIMARY_ALT_STATUS);
    (void)inb((u16)ATA_PRIMARY_ALT_STATUS);
}

static u32 block64_wait_ready(void)
{
    u32 attempt;
    u32 status = 0u;

    for (attempt = 0u; attempt < ATA_POLL_LIMIT; ++attempt)
    {
        status = (u32)inb((u16)ATA_PRIMARY_STATUS_COMMAND);
        g_last_status = status;

        if (status == ATA_STATUS_FLOATING_BUS)
        {
            return status;
        }

        if ((status & ATA_STATUS_BSY) == 0u)
        {
            return status;
        }
    }

    return status;
}

static int block64_wait_drq(void)
{
    u32 attempt;
    u32 status = 0u;

    for (attempt = 0u; attempt < ATA_POLL_LIMIT; ++attempt)
    {
        status = block64_wait_ready();
        if ((status == ATA_STATUS_FLOATING_BUS) || ((status & ATA_STATUS_ERR) != 0u))
        {
            return 0;
        }

        if ((status & ATA_STATUS_DRQ) != 0u)
        {
            return 1;
        }
    }

    return 0;
}

static int block64_pio_read_lba28(u32 lba, u8 *destination)
{
    u32 index;

    if ((destination == 0) || (lba >= ATA_LBA28_LIMIT))
    {
        return 0;
    }

    if (block64_wait_ready() == ATA_STATUS_FLOATING_BUS)
    {
        return 0;
    }

    outb((u16)ATA_PRIMARY_DRIVE, (u8)(ATA_DRIVE_MASTER_LBA | ((lba >> 24) & 0x0Fu)));
    block64_io_delay();
    outb((u16)ATA_PRIMARY_SECTOR_COUNT, 1u);
    outb((u16)ATA_PRIMARY_LBA_LOW, (u8)(lba & 0xFFu));
    outb((u16)ATA_PRIMARY_LBA_MID, (u8)((lba >> 8) & 0xFFu));
    outb((u16)ATA_PRIMARY_LBA_HIGH, (u8)((lba >> 16) & 0xFFu));
    outb((u16)ATA_PRIMARY_STATUS_COMMAND, ATA_COMMAND_READ_SECTORS);

    if (!block64_wait_drq())
    {
        return 0;
    }

    for (index = 0u; index < (BLOCK64_SECTOR_BYTES / 2u); ++index)
    {
        u16 word = inw((u16)ATA_PRIMARY_DATA);
        destination[index * 2u] = (u8)(word & 0xFFu);
        destination[(index * 2u) + 1u] = (u8)((word >> 8) & 0xFFu);
    }

    g_last_status = (u32)inb((u16)ATA_PRIMARY_STATUS_COMMAND);
    g_last_lba = lba;
    g_last_token = block64_token_bytes(destination, BLOCK64_SECTOR_BYTES);
    return 1;
}

void block64_init(void)
{
    block64_zero(g_block64_sector, BLOCK64_SECTOR_BYTES);
    g_available = 0u;
    g_last_status = 0u;
    g_read_count = 0u;
    g_byte_count = 0u;
    g_denial_count = 0u;
    g_unavailable_count = 0u;
    g_last_lba = 0u;
    g_last_token = 0u;

    if (block64_pio_read_lba28(0u, g_block64_sector))
    {
        g_available = 1u;
    }
}

u32 block64_available(void)
{
    return g_available;
}

u32 block64_last_status(void)
{
    return g_last_status;
}

u32 block64_read_sector(u32 block_capability_handle, u32 lba, u64 output_address, u32 owner_id)
{
    u32 endpoint;

    if ((principal64_is_active(owner_id) == 0u)
        || !block64_address_writable(output_address, BLOCK64_SECTOR_BYTES))
    {
        ++g_denial_count;
        return BLOCK64_INVALID_RESULT;
    }

    endpoint = capability64_route(
        block_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_BLOCK))
    {
        ++g_denial_count;
        return BLOCK64_INVALID_RESULT;
    }

    if (g_available == 0u)
    {
        ++g_unavailable_count;
        return BLOCK64_INVALID_RESULT;
    }

    if (!block64_pio_read_lba28(lba, g_block64_sector))
    {
        g_available = 0u;
        ++g_unavailable_count;
        return BLOCK64_INVALID_RESULT;
    }

    block64_copy((void *)output_address, g_block64_sector, BLOCK64_SECTOR_BYTES);
    ++g_read_count;
    g_byte_count += BLOCK64_SECTOR_BYTES;
    return BLOCK64_SECTOR_BYTES;
}

u32 block64_read_count(void)
{
    return g_read_count;
}

u32 block64_byte_count(void)
{
    return g_byte_count;
}

u32 block64_denial_count(void)
{
    return g_denial_count;
}

u32 block64_unavailable_count(void)
{
    return g_unavailable_count;
}

u32 block64_last_lba(void)
{
    return g_last_lba;
}

u32 block64_last_token(void)
{
    return g_last_token;
}
