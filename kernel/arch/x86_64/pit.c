#include "pit.h"

#include "x64.h"
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#include "timesource_x64.h"
#endif

enum
{
    PIT_COMMAND_PORT = 0x43,
    PIT_CHANNEL0_PORT = 0x40,
    PIT_BASE_FREQUENCY = 1193182u
};

static volatile u32 g_pit_ticks = 0u;
static u32 g_pit_frequency_hz = 100u;

void pit_initialize(u32 frequency_hz)
{
    u32 divisor;

    if (frequency_hz == 0u)
    {
        frequency_hz = 100u;
    }

    g_pit_frequency_hz = frequency_hz;
    divisor = PIT_BASE_FREQUENCY / frequency_hz;

    if (divisor == 0u)
    {
        divisor = 1u;
    }

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (u8)(divisor & 0xFFu));
    outb(PIT_CHANNEL0_PORT, (u8)((divisor >> 8) & 0xFFu));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    timesource64_init();
#endif
}

void pit_handle_interrupt(void)
{
    ++g_pit_ticks;
}

u32 pit_get_ticks(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    /* Some firmware clock-gates the 8254; the first callers after boot find out and switch sources. */
    if ((g_pit_ticks == 0u) && (timesource64_decided() == 0u))
    {
        timesource64_check_pit(&g_pit_ticks, g_pit_frequency_hz);
    }
    if (timesource64_mode() == TIMESOURCE64_MODE_TSC)
    {
        return timesource64_tsc_ticks(g_pit_frequency_hz);
    }
#endif
    return g_pit_ticks;
}

u32 pit_get_frequency_hz(void)
{
    return g_pit_frequency_hz;
}

u32 pit_get_uptime_seconds(void)
{
    if (g_pit_frequency_hz == 0u)
    {
        return 0u;
    }

    return pit_get_ticks() / g_pit_frequency_hz;
}
