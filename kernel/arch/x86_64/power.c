#include "power_x64.h"

#include "paging_x64.h"
#include "pci_x64.h"
#include "x64.h"

/* Shares the ACPI table window with i2c_hid.c; power-off and reset never return, so reuse is safe. */
#define POWER64_ACPI_MAP_VIRTUAL_BASE 0xFFFFFFFF90220000ull
#define POWER64_ACPI_MAP_PAGES 256u
#define POWER64_PAGE_BYTES 4096u
#define POWER64_FADT_PM1A_CNT 64u
#define POWER64_FADT_PM1B_CNT 68u
#define POWER64_FADT_FLAGS 112u
#define POWER64_FADT_RESET_REG 116u
#define POWER64_FADT_RESET_VALUE 128u
#define POWER64_FADT_FLAG_RESET_REG_SUP (1u << 10)
#define POWER64_GAS_SYSTEM_IO 1u
#define POWER64_SLP_EN (1u << 13)

static inline void power64_outw(u16 port, u16 value)
{
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static const volatile u8 *power64_map(u64 physical, u32 byte_count)
{
    u64 page_base = physical & ~((u64)POWER64_PAGE_BYTES - 1ull);
    u32 offset = (u32)(physical - page_base);
    u32 pages = (offset + byte_count + POWER64_PAGE_BYTES - 1u) / POWER64_PAGE_BYTES;

    if ((physical == 0ull) || (byte_count == 0u) || (pages > POWER64_ACPI_MAP_PAGES))
    {
        return (const volatile u8 *)0;
    }
    if (paging64_install_kernel_mmio_mapping(POWER64_ACPI_MAP_VIRTUAL_BASE, page_base, pages) == 0u)
    {
        return (const volatile u8 *)0;
    }
    return (const volatile u8 *)(POWER64_ACPI_MAP_VIRTUAL_BASE + offset);
}

static u32 power64_load_u32(const volatile u8 *bytes, u32 offset)
{
    return (u32)bytes[offset] | ((u32)bytes[offset + 1u] << 8)
        | ((u32)bytes[offset + 2u] << 16) | ((u32)bytes[offset + 3u] << 24);
}

static u64 power64_load_u64(const volatile u8 *bytes, u32 offset)
{
    return (u64)power64_load_u32(bytes, offset) | ((u64)power64_load_u32(bytes, offset + 4u) << 32);
}

/* Reads an AML integer at *cursor (Zero, One, Ones, or a Byte/Word prefix). */
static u32 power64_aml_integer(const volatile u8 *aml, u32 limit, u32 *cursor, u32 *value)
{
    u8 op;

    if (*cursor >= limit)
    {
        return 0u;
    }
    op = aml[*cursor];
    *cursor += 1u;
    if ((op == 0x00u) || (op == 0x01u))
    {
        *value = op;
        return 1u;
    }
    if (op == 0xFFu)
    {
        *value = 0xFFu;
        return 1u;
    }
    if ((op == 0x0Au) && (*cursor < limit))
    {
        *value = aml[*cursor];
        *cursor += 1u;
        return 1u;
    }
    if ((op == 0x0Bu) && ((*cursor + 1u) < limit))
    {
        *value = (u32)aml[*cursor] | ((u32)aml[*cursor + 1u] << 8);
        *cursor += 2u;
        return 1u;
    }
    return 0u;
}

/* Finds Name(_S5_, Package(){ SLP_TYPa, SLP_TYPb, ... }) in the DSDT. */
static u32 power64_find_s5(u32 *slp_typ_a, u32 *slp_typ_b)
{
    u64 dsdt = pci64_acpi_dsdt();
    u32 bytes = pci64_acpi_dsdt_bytes();
    const volatile u8 *aml = power64_map(dsdt, bytes);
    u32 index;

    if (aml == 0)
    {
        return 0u;
    }
    for (index = 36u; (index + 8u) < bytes; ++index)
    {
        u32 cursor;
        u32 extra;

        if ((aml[index] != (u8)'_') || (aml[index + 1u] != (u8)'S') || (aml[index + 2u] != (u8)'5')
            || (aml[index + 3u] != (u8)'_') || (aml[index + 4u] != 0x12u))
        {
            continue;
        }
        cursor = index + 5u;
        extra = (u32)(aml[cursor] >> 6);
        cursor += 1u + extra; /* PkgLength */
        cursor += 1u;         /* NumElements */
        if ((power64_aml_integer(aml, bytes, &cursor, slp_typ_a) != 0u)
            && (power64_aml_integer(aml, bytes, &cursor, slp_typ_b) != 0u))
        {
            return 1u;
        }
    }
    return 0u;
}

void power64_shutdown(void)
{
    const volatile u8 *fadt;
    u32 fadt_bytes = pci64_acpi_fadt_bytes();
    u32 pm1a;
    u32 pm1b;
    u32 slp_typ_a = 0u;
    u32 slp_typ_b = 0u;

    cpu_cli();
    fadt = power64_map(pci64_acpi_fadt(), fadt_bytes);
    if ((fadt == 0) || (fadt_bytes < (POWER64_FADT_PM1B_CNT + 4u)))
    {
        return;
    }
    pm1a = power64_load_u32(fadt, POWER64_FADT_PM1A_CNT);
    pm1b = power64_load_u32(fadt, POWER64_FADT_PM1B_CNT);
    if ((pm1a == 0u) || (pm1a > 0xFFFFu) || (power64_find_s5(&slp_typ_a, &slp_typ_b) == 0u))
    {
        return;
    }
    power64_outw((u16)pm1a, (u16)(((slp_typ_a & 7u) << 10) | POWER64_SLP_EN));
    if ((pm1b != 0u) && (pm1b <= 0xFFFFu))
    {
        power64_outw((u16)pm1b, (u16)(((slp_typ_b & 7u) << 10) | POWER64_SLP_EN));
    }
}

void power64_reboot(void)
{
    const volatile u8 *fadt;
    u32 fadt_bytes = pci64_acpi_fadt_bytes();
    u32 spin;

    cpu_cli();
    fadt = power64_map(pci64_acpi_fadt(), fadt_bytes);
    if ((fadt != 0) && (fadt_bytes > POWER64_FADT_RESET_VALUE)
        && ((power64_load_u32(fadt, POWER64_FADT_FLAGS) & POWER64_FADT_FLAG_RESET_REG_SUP) != 0u)
        && (fadt[POWER64_FADT_RESET_REG] == POWER64_GAS_SYSTEM_IO))
    {
        u64 port = power64_load_u64(fadt, POWER64_FADT_RESET_REG + 4u);
        if ((port != 0ull) && (port <= 0xFFFFull))
        {
            outb((u16)port, fadt[POWER64_FADT_RESET_VALUE]);
        }
    }
    /* PCI reset control: full reset + reset CPU. */
    outb(0xCF9u, 0x02u);
    outb(0xCF9u, 0x06u);
    /* Keyboard controller pulse reset line. */
    for (spin = 0u; (spin < 100000u) && ((inb(0x64u) & 0x02u) != 0u); ++spin)
    {
    }
    outb(0x64u, 0xFEu);
}
