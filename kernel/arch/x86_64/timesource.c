#include "timesource_x64.h"

#include "apic_x64.h"
#include "serial.h"
#include "x64.h"

enum
{
    TIMESOURCE64_PIT_COMMAND_PORT = 0x43,
    TIMESOURCE64_PIT_CHANNEL0_PORT = 0x40,
    TIMESOURCE64_CMOS_INDEX_PORT = 0x70,
    TIMESOURCE64_CMOS_DATA_PORT = 0x71,
    TIMESOURCE64_IRQ0_VECTOR = 0x20u,
    /* Grace period after PIT programming before a silent PIT is suspected. */
    TIMESOURCE64_GRACE_MS = 500u,
    /* With interrupts enabled, a working 100 Hz PIT ticks several times in this window. */
    TIMESOURCE64_CONFIRM_MS = 60u,
    TIMESOURCE64_FROZEN_SAMPLES = 4u,
    TIMESOURCE64_FROZEN_SAMPLE_IO_DELAYS = 300u
};

static u64 g_timesource64_start_tsc = 0ull;
static u64 g_timesource64_tsc_hz = 0ull;
static u32 g_timesource64_mode = TIMESOURCE64_MODE_PIT;
static u32 g_timesource64_decided = 0u;
static u32 g_timesource64_initialized = 0u;
static u32 g_timesource64_rtc_calibrated = 0u;

u64 timesource64_rdtsc(void)
{
    u32 low;
    u32 high;

    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((u64)high << 32) | (u64)low;
}

static void timesource64_cpuid(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
    u32 a = leaf;
    u32 b;
    u32 c = 0u;
    u32 d;

    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    *eax = a;
    *ebx = b;
    *ecx = c;
    *edx = d;
}

static u64 timesource64_cpuid_tsc_hz(void)
{
    u32 max_leaf;
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;

    timesource64_cpuid(0u, &max_leaf, &ebx, &ecx, &edx);
    if (max_leaf >= 0x15u)
    {
        /* TSC = crystal * EBX / EAX. */
        timesource64_cpuid(0x15u, &eax, &ebx, &ecx, &edx);
        if ((eax != 0u) && (ebx != 0u) && (ecx != 0u))
        {
            return ((u64)ecx * (u64)ebx) / (u64)eax;
        }
    }
    if (max_leaf >= 0x16u)
    {
        /* Processor base frequency in MHz; equals the TSC rate on parts without leaf 0x15 crystal data. */
        timesource64_cpuid(0x16u, &eax, &ebx, &ecx, &edx);
        if ((eax & 0xFFFFu) != 0u)
        {
            return (u64)(eax & 0xFFFFu) * 1000000ull;
        }
    }
    return 0ull;
}

void timesource64_init(void)
{
    g_timesource64_start_tsc = timesource64_rdtsc();
    if (g_timesource64_initialized == 0u)
    {
        g_timesource64_tsc_hz = timesource64_cpuid_tsc_hz();
        g_timesource64_initialized = 1u;
    }
    g_timesource64_mode = TIMESOURCE64_MODE_PIT;
    g_timesource64_decided = 0u;
}

u64 timesource64_tsc_hz(void)
{
    return g_timesource64_tsc_hz;
}

u32 timesource64_mode(void)
{
    return g_timesource64_mode;
}

u32 timesource64_decided(void)
{
    return g_timesource64_decided;
}

const char *timesource64_mode_name(void)
{
    if (g_timesource64_mode == TIMESOURCE64_MODE_LAPIC)
    {
        return "lapic";
    }
    if (g_timesource64_mode == TIMESOURCE64_MODE_TSC)
    {
        return "tsc";
    }
    return "pit";
}

u32 timesource64_tsc_ticks(u32 frequency_hz)
{
    u64 per_tick;

    if ((g_timesource64_tsc_hz == 0ull) || (frequency_hz == 0u))
    {
        return 0u;
    }
    per_tick = g_timesource64_tsc_hz / (u64)frequency_hz;
    if (per_tick == 0ull)
    {
        return 0u;
    }
    return (u32)((timesource64_rdtsc() - g_timesource64_start_tsc) / per_tick);
}

u32 timesource64_elapsed_ms(u64 start_tsc, u32 milliseconds)
{
    if (g_timesource64_tsc_hz == 0ull)
    {
        return 1u;
    }
    return ((timesource64_rdtsc() - start_tsc) >= ((g_timesource64_tsc_hz / 1000ull) * (u64)milliseconds)) ? 1u : 0u;
}

u32 timesource64_delay_us(u32 microseconds)
{
    u64 start;
    u64 cycles;

    if (g_timesource64_tsc_hz == 0ull)
    {
        return 0u;
    }
    cycles = (g_timesource64_tsc_hz / 1000000ull) * (u64)microseconds;
    start = timesource64_rdtsc();
    while ((timesource64_rdtsc() - start) < cycles)
    {
        __asm__ volatile("pause");
    }
    return 1u;
}

static u32 timesource64_interrupts_enabled(void)
{
    u64 flags;

    __asm__ volatile("pushfq; popq %0" : "=r"(flags));
    return ((flags & 0x200ull) != 0ull) ? 1u : 0u;
}

static u32 timesource64_read_pit_count(void)
{
    u32 low;
    u32 high;

    outb(TIMESOURCE64_PIT_COMMAND_PORT, 0x00u);
    low = inb(TIMESOURCE64_PIT_CHANNEL0_PORT);
    high = inb(TIMESOURCE64_PIT_CHANNEL0_PORT);
    return (high << 8) | low;
}

/* A gated 8254 holds its count; a running one at 1.19 MHz changes within microseconds. */
static u32 timesource64_pit_counter_frozen(void)
{
    u32 first = timesource64_read_pit_count();
    u32 sample;

    for (sample = 0u; sample < TIMESOURCE64_FROZEN_SAMPLES; ++sample)
    {
        u32 delay;

        for (delay = 0u; delay < TIMESOURCE64_FROZEN_SAMPLE_IO_DELAYS; ++delay)
        {
            outb(0x80, 0u);
        }
        if (timesource64_read_pit_count() != first)
        {
            return 0u;
        }
    }
    return 1u;
}

static u8 timesource64_cmos_seconds(void)
{
    outb(TIMESOURCE64_CMOS_INDEX_PORT, 0x00u);
    return inb(TIMESOURCE64_CMOS_DATA_PORT);
}

/* Last resort for CPUs without CPUID frequency data: time one RTC second (takes up to two). */
static void timesource64_calibrate_with_rtc(void)
{
    u8 start;
    u8 second;
    u64 begin;
    u64 guard;

    if (g_timesource64_rtc_calibrated != 0u)
    {
        return;
    }
    g_timesource64_rtc_calibrated = 1u;
    start = timesource64_cmos_seconds();
    for (guard = 0ull; guard < 400000000ull; ++guard)
    {
        if (timesource64_cmos_seconds() != start)
        {
            break;
        }
    }
    second = timesource64_cmos_seconds();
    begin = timesource64_rdtsc();
    for (guard = 0ull; guard < 400000000ull; ++guard)
    {
        if (timesource64_cmos_seconds() != second)
        {
            g_timesource64_tsc_hz = timesource64_rdtsc() - begin;
            return;
        }
    }
}

static void timesource64_log_takeover(const char *reason)
{
    serial_write_string("[x64] timer: pit silent (");
    serial_write_string(reason);
    serial_write_string("), ticks from ");
    serial_write_string(timesource64_mode_name());
    serial_write_string("\n");
}

static void timesource64_take_over(volatile u32 *ticks, u32 frequency_hz, const char *reason)
{
    g_timesource64_decided = 1u;
    if (g_timesource64_tsc_hz == 0ull)
    {
        timesource64_calibrate_with_rtc();
    }
    if (g_timesource64_tsc_hz == 0ull)
    {
        serial_write_string("[x64] timer: pit silent, tsc rate unknown\n");
        return;
    }
    if (apic64_start_periodic_timer((u8)TIMESOURCE64_IRQ0_VECTOR, frequency_hz, g_timesource64_tsc_hz) != 0u)
    {
        /* Uptime keeps counting from boot: seed the tick counter from the TSC. */
        *ticks = timesource64_tsc_ticks(frequency_hz);
        g_timesource64_mode = TIMESOURCE64_MODE_LAPIC;
    }
    else
    {
        g_timesource64_mode = TIMESOURCE64_MODE_TSC;
    }
    timesource64_log_takeover(reason);
}

void timesource64_check_pit(volatile u32 *ticks, u32 frequency_hz)
{
    if (g_timesource64_decided != 0u)
    {
        return;
    }
    if (*ticks != 0u)
    {
        g_timesource64_decided = 1u;
        return;
    }
    if ((g_timesource64_tsc_hz != 0ull)
        && (timesource64_elapsed_ms(g_timesource64_start_tsc, TIMESOURCE64_GRACE_MS) == 0u))
    {
        return;
    }

    if (timesource64_pit_counter_frozen() != 0u)
    {
        timesource64_take_over(ticks, frequency_hz, "counter frozen");
        return;
    }
    if ((g_timesource64_tsc_hz == 0ull) || (timesource64_interrupts_enabled() == 0u))
    {
        /* The PIT counts; the interrupt can only be timed with a TSC rate and interrupts on, so ask again later. */
        return;
    }

    /* The PIT counts and interrupts are on: a routed IRQ0 arrives within a few ticks. */
    {
        u64 begin = timesource64_rdtsc();

        while (timesource64_elapsed_ms(begin, TIMESOURCE64_CONFIRM_MS) == 0u)
        {
            if (*ticks != 0u)
            {
                g_timesource64_decided = 1u;
                return;
            }
            __asm__ volatile("pause");
        }
    }
    timesource64_take_over(ticks, frequency_hz, "no irq0");
}
