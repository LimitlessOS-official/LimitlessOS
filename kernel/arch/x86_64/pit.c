#include "pit.h"

#include "x64.h"

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
}

void pit_handle_interrupt(void)
{
    ++g_pit_ticks;
}

u32 pit_get_ticks(void)
{
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

    return g_pit_ticks / g_pit_frequency_hz;
}
