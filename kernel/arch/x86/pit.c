#include "pit.h"

#include "scheduler.h"
#include "x86.h"

enum
{
    PIT_COMMAND_PORT = 0x43,
    PIT_CHANNEL0_PORT = 0x40,
    PIT_BASE_FREQUENCY = 1193182
};

static volatile u32 pit_ticks = 0;
static u32 pit_frequency_hz = 100;

void pit_initialize(u32 frequency_hz)
{
    u32 divisor;

    if (frequency_hz == 0)
    {
        frequency_hz = 100;
    }

    pit_frequency_hz = frequency_hz;
    divisor = PIT_BASE_FREQUENCY / frequency_hz;

    if (divisor == 0)
    {
        divisor = 1;
    }

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (u8)((divisor >> 8) & 0xFF));
}

void pit_handle_interrupt(void)
{
    ++pit_ticks;
    scheduler_on_tick();
    scheduler_run_ready(2);
}

u32 pit_get_ticks(void)
{
    return pit_ticks;
}

u32 pit_get_frequency_hz(void)
{
    return pit_frequency_hz;
}

u32 pit_get_uptime_seconds(void)
{
    if (pit_frequency_hz == 0)
    {
        return 0;
    }

    return pit_ticks / pit_frequency_hz;
}
