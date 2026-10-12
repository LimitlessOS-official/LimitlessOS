#ifndef LIMITLESS_TIMESOURCE_X64_H
#define LIMITLESS_TIMESOURCE_X64_H

#include "types.h"

/*
 * Timer health for the UEFI kernel. Some laptops clock-gate the legacy 8254
 * PIT, so IRQ0 never fires and every tick-based clock and timeout stalls.
 * The PIT driver asks this module to check once ticks should have arrived;
 * if the PIT is dead, the LAPIC timer takes over IRQ0's vector, or, without
 * a usable LAPIC, ticks are derived from the TSC.
 */
enum
{
    TIMESOURCE64_MODE_PIT = 0u,
    TIMESOURCE64_MODE_LAPIC = 1u,
    TIMESOURCE64_MODE_TSC = 2u
};

void timesource64_init(void);
u64 timesource64_rdtsc(void);
/* TSC frequency in Hz, or 0 when it is not known (no CPUID leaf 0x15/0x16 and no calibration yet). */
u64 timesource64_tsc_hz(void);
/* Called by the PIT driver while it has seen no interrupt; may seed *ticks when the LAPIC takes over. */
void timesource64_check_pit(volatile u32 *ticks, u32 frequency_hz);
u32 timesource64_mode(void);
u32 timesource64_decided(void);
/* Ticks at frequency_hz since timesource64_init, from the TSC; 0 when the TSC rate is unknown. */
u32 timesource64_tsc_ticks(u32 frequency_hz);
/* Busy-wait for at least the given microseconds when the TSC rate is known; returns 0 otherwise. */
u32 timesource64_delay_us(u32 microseconds);
/* True once at least the given milliseconds of TSC time have passed since start_tsc; 1 when the rate is unknown. */
u32 timesource64_elapsed_ms(u64 start_tsc, u32 milliseconds);
const char *timesource64_mode_name(void);

#endif
