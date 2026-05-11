#include "apic_x64.h"

#include "paging_x64.h"
#include "pic.h"
#include "x64.h"

enum
{
    APIC64_UEFI_BOOT_DRIVE_MARKER = 0x000000EFu,
    APIC64_IRQ0_VECTOR = 0x20u,
    APIC64_IRQ1_VECTOR = 0x21u,
    APIC64_IRQ11_VECTOR = 0x2Bu,
    APIC64_IRQ12_VECTOR = 0x2Cu,
    APIC64_ISA_IRQ_LIMIT = 16u,
    APIC64_LAPIC_VIRTUAL_BASE = 0xFFFFFFFF90200000ull,
    APIC64_IOAPIC_VIRTUAL_BASE = 0xFFFFFFFF90201000ull,
    APIC64_IA32_APIC_BASE_MSR = 0x0000001Bu,
    APIC64_IA32_APIC_BASE_ENABLE = 0x0000000000000800ull,
    APIC64_IA32_APIC_BASE_PHYSICAL_MASK = 0x00000000FFFFF000ull,
    APIC64_LAPIC_ID = 0x020u,
    APIC64_LAPIC_EOI = 0x0B0u,
    APIC64_LAPIC_SPURIOUS_VECTOR = 0x0F0u,
    APIC64_LAPIC_LVT_LINT0 = 0x350u,
    APIC64_LAPIC_LVT_LINT1 = 0x360u,
    APIC64_LAPIC_SOFTWARE_ENABLE = 0x00000100u,
    APIC64_LAPIC_LVT_MASKED = 0x00010000u,
    APIC64_IOAPIC_REGISTER_SELECT = 0x00u,
    APIC64_IOAPIC_WINDOW = 0x10u,
    APIC64_IOAPIC_VERSION = 0x01u,
    APIC64_IOAPIC_REDIRECTION_BASE = 0x10u,
    APIC64_IOAPIC_ACTIVE_LOW = 0x00002000u,
    APIC64_IOAPIC_LEVEL_TRIGGER = 0x00008000u,
    APIC64_IOAPIC_REDIRECTION_MASKED = 0x00010000u,
    APIC64_IOAPIC_INVALID_REDIRECTION = 0xFFFFFFFFu,
    APIC64_ACPI_POLARITY_MASK = 0x0003u,
    APIC64_ACPI_POLARITY_ACTIVE_HIGH = 0x0001u,
    APIC64_ACPI_POLARITY_ACTIVE_LOW = 0x0003u,
    APIC64_ACPI_TRIGGER_MASK = 0x000Cu,
    APIC64_ACPI_TRIGGER_EDGE = 0x0004u,
    APIC64_ACPI_TRIGGER_LEVEL = 0x000Cu
};

static u32 g_enabled = 0u;
static u32 g_madt_found = 0u;
static u32 g_lapic_found = 0u;
static u32 g_ioapic_found = 0u;
static u32 g_pic_disabled = 0u;
static u32 g_lapic_id = 0u;
static u32 g_irq0_routed = 0u;
static u32 g_irq1_routed = 0u;
static u32 g_irq11_routed = 0u;
static u32 g_irq12_routed = 0u;
static u32 g_ioapic_id = 0u;
static u32 g_ioapic_gsi_base = 0u;
static u32 g_ioapic_max_redirection = 0u;
static u32 g_override_scanned = 0u;
static u32 g_override_count = 0u;
static u32 g_override_valid_mask = 0u;
static u32 g_override_gsi[APIC64_ISA_IRQ_LIMIT];
static u32 g_override_flags[APIC64_ISA_IRQ_LIMIT];
static u32 g_timer_gsi = 0u;
static u32 g_keyboard_gsi = 0u;
static u32 g_ahci_gsi = 0u;
static u32 g_mouse_gsi = 0u;
static u32 g_timer_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
static u32 g_timer_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
static u32 g_keyboard_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
static u32 g_keyboard_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
static u32 g_ahci_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
static u32 g_ahci_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
static u32 g_mouse_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
static u32 g_mouse_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
static u64 g_lapic_base = 0ull;
static u64 g_ioapic_base = 0ull;

static volatile u32 *apic64_lapic_register(u32 offset)
{
    return (volatile u32 *)(u64)(APIC64_LAPIC_VIRTUAL_BASE + (u64)offset);
}

static volatile u32 *apic64_ioapic_register(u32 offset)
{
    return (volatile u32 *)(u64)(APIC64_IOAPIC_VIRTUAL_BASE + (u64)offset);
}

static u32 apic64_lapic_read(u32 offset)
{
    return *apic64_lapic_register(offset);
}

static void apic64_lapic_write(u32 offset, u32 value)
{
    *apic64_lapic_register(offset) = value;
}

static u32 apic64_ioapic_read(u8 register_index)
{
    *apic64_ioapic_register(APIC64_IOAPIC_REGISTER_SELECT) = (u32)register_index;
    return *apic64_ioapic_register(APIC64_IOAPIC_WINDOW);
}

static void apic64_ioapic_write(u8 register_index, u32 value)
{
    *apic64_ioapic_register(APIC64_IOAPIC_REGISTER_SELECT) = (u32)register_index;
    *apic64_ioapic_register(APIC64_IOAPIC_WINDOW) = value;
}

static u32 apic64_ioapic_redirection_index(u32 irq)
{
    u32 index;

    if (irq < g_ioapic_gsi_base)
    {
        return APIC64_IOAPIC_INVALID_REDIRECTION;
    }

    index = irq - g_ioapic_gsi_base;
    if (index > g_ioapic_max_redirection)
    {
        return APIC64_IOAPIC_INVALID_REDIRECTION;
    }

    return index;
}

static u32 apic64_irq_override_present(u32 irq)
{
    if (irq >= APIC64_ISA_IRQ_LIMIT)
    {
        return 0u;
    }

    return ((g_override_valid_mask & (1u << irq)) != 0u) ? 1u : 0u;
}

static u32 apic64_normalized_polarity(u32 acpi_flags)
{
    u32 polarity = acpi_flags & APIC64_ACPI_POLARITY_MASK;

    if (polarity == APIC64_ACPI_POLARITY_ACTIVE_LOW)
    {
        return APIC64_ACPI_POLARITY_ACTIVE_LOW;
    }

    return APIC64_ACPI_POLARITY_ACTIVE_HIGH;
}

static u32 apic64_normalized_trigger(u32 acpi_flags)
{
    u32 trigger = acpi_flags & APIC64_ACPI_TRIGGER_MASK;

    if (trigger == APIC64_ACPI_TRIGGER_LEVEL)
    {
        return APIC64_ACPI_TRIGGER_LEVEL >> 2;
    }

    return APIC64_ACPI_TRIGGER_EDGE >> 2;
}

static u32 apic64_ioapic_flags_from_acpi(u32 acpi_flags)
{
    u32 value = 0u;

    if (apic64_normalized_polarity(acpi_flags) == APIC64_ACPI_POLARITY_ACTIVE_LOW)
    {
        value |= APIC64_IOAPIC_ACTIVE_LOW;
    }

    if (apic64_normalized_trigger(acpi_flags) == (APIC64_ACPI_TRIGGER_LEVEL >> 2))
    {
        value |= APIC64_IOAPIC_LEVEL_TRIGGER;
    }

    return value;
}

static u32 apic64_route_gsi(u32 gsi, u8 vector, u32 acpi_flags)
{
    u32 index = apic64_ioapic_redirection_index(gsi);
    u8 low_register;
    u8 high_register;
    u32 low_value = (u32)vector | apic64_ioapic_flags_from_acpi(acpi_flags);
    u32 high_value = g_lapic_id << 24;

    if (index == APIC64_IOAPIC_INVALID_REDIRECTION)
    {
        return 0u;
    }

    low_register = (u8)(APIC64_IOAPIC_REDIRECTION_BASE + (index * 2u));
    high_register = (u8)(low_register + 1u);
    apic64_ioapic_write(low_register, low_value | APIC64_IOAPIC_REDIRECTION_MASKED);
    apic64_ioapic_write(high_register, high_value);
    apic64_ioapic_write(low_register, low_value);
    return 1u;
}

static u32 apic64_route_isa_irq(
    u32 irq,
    u8 vector,
    u32 *routed_gsi,
    u32 *routed_polarity,
    u32 *routed_trigger)
{
    u32 gsi = irq;
    u32 acpi_flags = 0u;

    if (apic64_irq_override_present(irq) != 0u)
    {
        gsi = g_override_gsi[irq];
        acpi_flags = g_override_flags[irq];
    }

    if (routed_gsi != 0)
    {
        *routed_gsi = gsi;
    }
    if (routed_polarity != 0)
    {
        *routed_polarity = apic64_normalized_polarity(acpi_flags);
    }
    if (routed_trigger != 0)
    {
        *routed_trigger = apic64_normalized_trigger(acpi_flags);
    }

    return apic64_route_gsi(gsi, vector, acpi_flags);
}

static void apic64_reset_state(void)
{
    u32 index;

    g_enabled = 0u;
    g_madt_found = 0u;
    g_lapic_found = 0u;
    g_ioapic_found = 0u;
    g_pic_disabled = 0u;
    g_lapic_id = 0u;
    g_irq0_routed = 0u;
    g_irq1_routed = 0u;
    g_irq11_routed = 0u;
    g_irq12_routed = 0u;
    g_ioapic_id = 0u;
    g_ioapic_gsi_base = 0u;
    g_ioapic_max_redirection = 0u;
    g_override_scanned = 0u;
    g_override_count = 0u;
    g_override_valid_mask = 0u;
    g_timer_gsi = 0u;
    g_keyboard_gsi = 0u;
    g_ahci_gsi = 0u;
    g_mouse_gsi = 0u;
    g_timer_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
    g_timer_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
    g_keyboard_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
    g_keyboard_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
    g_ahci_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
    g_ahci_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
    g_mouse_polarity = APIC64_ACPI_POLARITY_ACTIVE_HIGH;
    g_mouse_trigger = APIC64_ACPI_TRIGGER_EDGE >> 2;
    for (index = 0u; index < APIC64_ISA_IRQ_LIMIT; ++index)
    {
        g_override_gsi[index] = 0u;
        g_override_flags[index] = 0u;
    }
    g_lapic_base = 0ull;
    g_ioapic_base = 0ull;
}

void apic64_init(const struct boot_info *boot_info)
{
    u64 apic_base_msr;
    u32 ioapic_version;
    u32 index;

    apic64_reset_state();
    if (boot_info == 0 || boot_info->boot_drive != APIC64_UEFI_BOOT_DRIVE_MARKER)
    {
        return;
    }

    g_madt_found = ((boot_info->pci_ecam_flags & LIMITLESS_BOOT_ACPI_FLAG_MADT) != 0u) ? 1u : 0u;
    g_lapic_found = ((boot_info->pci_ecam_flags & LIMITLESS_BOOT_ACPI_FLAG_LAPIC) != 0u) ? 1u : 0u;
    g_ioapic_found = ((boot_info->pci_ecam_flags & LIMITLESS_BOOT_ACPI_FLAG_IOAPIC) != 0u) ? 1u : 0u;
    g_lapic_base = boot_info->apic_lapic_base;
    g_ioapic_base = boot_info->apic_ioapic_base;
    g_ioapic_id = boot_info->apic_ioapic_id;
    g_ioapic_gsi_base = boot_info->apic_ioapic_gsi_base;
    g_override_scanned = (boot_info->apic_interrupt_override_scanned != 0u) ? 1u : 0u;
    g_override_valid_mask = boot_info->apic_interrupt_override_valid_mask & 0x0000FFFFu;
    for (index = 0u; index < APIC64_ISA_IRQ_LIMIT; ++index)
    {
        if ((g_override_valid_mask & (1u << index)) != 0u)
        {
            g_override_gsi[index] = boot_info->apic_interrupt_override_gsi[index];
            g_override_flags[index] = boot_info->apic_interrupt_override_flags[index];
            ++g_override_count;
        }
    }

    if (g_madt_found == 0u ||
        g_lapic_found == 0u ||
        g_ioapic_found == 0u ||
        g_lapic_base == 0ull ||
        g_ioapic_base == 0ull ||
        g_lapic_base > 0xFFFFFFFFull ||
        g_ioapic_base > 0xFFFFFFFFull)
    {
        return;
    }

    if (paging64_install_apic_mmio_mapping(
            APIC64_LAPIC_VIRTUAL_BASE,
            (u32)g_lapic_base,
            APIC64_IOAPIC_VIRTUAL_BASE,
            (u32)g_ioapic_base) == 0u)
    {
        return;
    }

    apic_base_msr = rdmsr64(APIC64_IA32_APIC_BASE_MSR);
    apic_base_msr &= ~APIC64_IA32_APIC_BASE_PHYSICAL_MASK;
    apic_base_msr |= (g_lapic_base & APIC64_IA32_APIC_BASE_PHYSICAL_MASK);
    apic_base_msr |= APIC64_IA32_APIC_BASE_ENABLE;
    wrmsr64(APIC64_IA32_APIC_BASE_MSR, apic_base_msr);

    g_lapic_id = (apic64_lapic_read(APIC64_LAPIC_ID) >> 24) & 0xFFu;
    apic64_lapic_write(
        APIC64_LAPIC_SPURIOUS_VECTOR,
        (apic64_lapic_read(APIC64_LAPIC_SPURIOUS_VECTOR) & 0xFFFFFF00u) |
            APIC64_LAPIC_SOFTWARE_ENABLE |
            0xFFu);

    ioapic_version = apic64_ioapic_read(APIC64_IOAPIC_VERSION);
    g_ioapic_max_redirection = (ioapic_version >> 16) & 0xFFu;
    g_irq0_routed = apic64_route_isa_irq(
        0u,
        (u8)APIC64_IRQ0_VECTOR,
        &g_timer_gsi,
        &g_timer_polarity,
        &g_timer_trigger);
    g_irq1_routed = apic64_route_isa_irq(
        1u,
        (u8)APIC64_IRQ1_VECTOR,
        &g_keyboard_gsi,
        &g_keyboard_polarity,
        &g_keyboard_trigger);
    g_irq11_routed = apic64_route_isa_irq(
        11u,
        (u8)APIC64_IRQ11_VECTOR,
        &g_ahci_gsi,
        &g_ahci_polarity,
        &g_ahci_trigger);
    g_irq12_routed = apic64_route_isa_irq(
        12u,
        (u8)APIC64_IRQ12_VECTOR,
        &g_mouse_gsi,
        &g_mouse_polarity,
        &g_mouse_trigger);
    if (g_irq0_routed == 0u || g_irq1_routed == 0u)
    {
        return;
    }

    pic_initialize(0xFFu, 0xFFu);
    pic_disable();
    g_pic_disabled = 1u;
    apic64_lapic_write(APIC64_LAPIC_LVT_LINT0, apic64_lapic_read(APIC64_LAPIC_LVT_LINT0) | APIC64_LAPIC_LVT_MASKED);
    apic64_lapic_write(APIC64_LAPIC_LVT_LINT1, apic64_lapic_read(APIC64_LAPIC_LVT_LINT1) | APIC64_LAPIC_LVT_MASKED);
    apic64_lapic_write(APIC64_LAPIC_EOI, 0u);
    g_enabled = 1u;
}

u32 apic64_enabled(void)
{
    return g_enabled;
}

void apic64_send_eoi(u8 irq)
{
    (void)irq;
    if (g_enabled != 0u)
    {
        apic64_lapic_write(APIC64_LAPIC_EOI, 0u);
    }
}

u32 apic64_madt_found(void)
{
    return g_madt_found;
}

u64 apic64_lapic_base(void)
{
    return g_lapic_base;
}

u64 apic64_ioapic_base(void)
{
    return g_ioapic_base;
}

u32 apic64_pic_disabled(void)
{
    return g_pic_disabled;
}

u32 apic64_lapic_id(void)
{
    return g_lapic_id;
}

u32 apic64_irq0_routed(void)
{
    return g_irq0_routed;
}

u32 apic64_irq1_routed(void)
{
    return g_irq1_routed;
}

u32 apic64_irq11_routed(void)
{
    return g_irq11_routed;
}

u32 apic64_irq12_routed(void)
{
    return g_irq12_routed;
}

u32 apic64_ioapic_gsi_base(void)
{
    return g_ioapic_gsi_base;
}

u32 apic64_ioapic_max_redirection(void)
{
    return g_ioapic_max_redirection;
}

u32 apic64_override_scanned(void)
{
    return g_override_scanned;
}

u32 apic64_override_count(void)
{
    return g_override_count;
}

u32 apic64_timer_gsi(void)
{
    return g_timer_gsi;
}

u32 apic64_keyboard_gsi(void)
{
    return g_keyboard_gsi;
}

u32 apic64_ahci_gsi(void)
{
    return g_ahci_gsi;
}

u32 apic64_mouse_gsi(void)
{
    return g_mouse_gsi;
}

u32 apic64_timer_polarity(void)
{
    return g_timer_polarity;
}

u32 apic64_timer_trigger(void)
{
    return g_timer_trigger;
}

u32 apic64_keyboard_polarity(void)
{
    return g_keyboard_polarity;
}

u32 apic64_keyboard_trigger(void)
{
    return g_keyboard_trigger;
}

u32 apic64_ahci_polarity(void)
{
    return g_ahci_polarity;
}

u32 apic64_ahci_trigger(void)
{
    return g_ahci_trigger;
}

u32 apic64_mouse_polarity(void)
{
    return g_mouse_polarity;
}

u32 apic64_mouse_trigger(void)
{
    return g_mouse_trigger;
}
