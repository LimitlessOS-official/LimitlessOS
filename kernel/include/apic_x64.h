#ifndef LIMITLESS_APIC_X64_H
#define LIMITLESS_APIC_X64_H

#include "boot_info.h"
#include "types.h"

void apic64_init(const struct boot_info *boot_info);
u32 apic64_enabled(void);
void apic64_send_eoi(u8 irq);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
/* Replaces a silent PIT: masks IRQ0 at the IO-APIC and runs the LAPIC timer at hz on vector. */
u32 apic64_start_periodic_timer(u8 vector, u32 hz, u64 tsc_hz);
u32 apic64_timer_initial_count(void);
#endif
u32 apic64_madt_found(void);
u64 apic64_lapic_base(void);
u64 apic64_ioapic_base(void);
u32 apic64_pic_disabled(void);
u32 apic64_lapic_id(void);
u32 apic64_irq0_routed(void);
u32 apic64_irq1_routed(void);
u32 apic64_irq11_routed(void);
u32 apic64_irq12_routed(void);
u32 apic64_ioapic_gsi_base(void);
u32 apic64_ioapic_max_redirection(void);
u32 apic64_override_scanned(void);
u32 apic64_override_count(void);
u32 apic64_timer_gsi(void);
u32 apic64_keyboard_gsi(void);
u32 apic64_ahci_gsi(void);
u32 apic64_mouse_gsi(void);
u32 apic64_timer_polarity(void);
u32 apic64_timer_trigger(void);
u32 apic64_keyboard_polarity(void);
u32 apic64_keyboard_trigger(void);
u32 apic64_ahci_polarity(void);
u32 apic64_ahci_trigger(void);
u32 apic64_mouse_polarity(void);
u32 apic64_mouse_trigger(void);

#endif
