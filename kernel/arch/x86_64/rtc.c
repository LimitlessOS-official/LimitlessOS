#include "rtc_x64.h"

#include "pit.h"
#include "x64.h"

#define RTC64_INDEX_PORT 0x70u
#define RTC64_DATA_PORT 0x71u
#define RTC64_REG_SECONDS 0x00u
#define RTC64_REG_MINUTES 0x02u
#define RTC64_REG_HOURS 0x04u
#define RTC64_REG_DAY 0x07u
#define RTC64_REG_MONTH 0x08u
#define RTC64_REG_YEAR 0x09u
#define RTC64_REG_STATUS_A 0x0Au
#define RTC64_REG_STATUS_B 0x0Bu
#define RTC64_REG_CENTURY 0x32u
#define RTC64_STATUS_A_UPDATING 0x80u
#define RTC64_STATUS_B_24_HOUR 0x02u
#define RTC64_STATUS_B_BINARY 0x04u
#define RTC64_UPDATE_SPIN_LIMIT 100000u

static u32 g_rtc64_valid = 0u;
static u32 g_rtc64_read_attempted = 0u;
static u64 g_rtc64_epoch = 0ull;
static u32 g_rtc64_epoch_ticks = 0u;
static u32 g_rtc64_second_start_ticks = 0u;

static u8 rtc64_read_register(u8 reg)
{
    /* Keep NMI enabled (bit 7 clear), as firmware leaves it. */
    outb(RTC64_INDEX_PORT, (u8)(reg & 0x7Fu));
    return inb(RTC64_DATA_PORT);
}

static u32 rtc64_bcd_to_binary(u8 value)
{
    return ((u32)(value >> 4) * 10u) + (u32)(value & 0x0Fu);
}

static u32 rtc64_wait_not_updating(void)
{
    u32 spin;

    for (spin = 0u; spin < RTC64_UPDATE_SPIN_LIMIT; ++spin)
    {
        if ((rtc64_read_register(RTC64_REG_STATUS_A) & RTC64_STATUS_A_UPDATING) == 0u)
        {
            return 1u;
        }
    }
    return 0u;
}

/* Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's days_from_civil). */
static u64 rtc64_days_from_civil(u32 year, u32 month, u32 day)
{
    u32 y = (month <= 2u) ? (year - 1u) : year;
    u32 era = y / 400u;
    u32 yoe = y - (era * 400u);
    u32 mp = (month > 2u) ? (month - 3u) : (month + 9u);
    u32 doy = ((153u * mp) + 2u) / 5u + day - 1u;
    u32 doe = (yoe * 365u) + (yoe / 4u) - (yoe / 100u) + doy;

    return ((u64)era * 146097ull) + (u64)doe - 719468ull;
}

static u32 rtc64_read_datetime(rtc64_datetime_t *out)
{
    u8 raw[7];
    u8 again[7];
    u8 status_b;
    u32 attempt;
    u32 index;
    u32 hour_pm;
    u32 century;

    for (attempt = 0u; attempt < 4u; ++attempt)
    {
        if (rtc64_wait_not_updating() == 0u)
        {
            return 0u;
        }
        raw[0] = rtc64_read_register(RTC64_REG_SECONDS);
        raw[1] = rtc64_read_register(RTC64_REG_MINUTES);
        raw[2] = rtc64_read_register(RTC64_REG_HOURS);
        raw[3] = rtc64_read_register(RTC64_REG_DAY);
        raw[4] = rtc64_read_register(RTC64_REG_MONTH);
        raw[5] = rtc64_read_register(RTC64_REG_YEAR);
        raw[6] = rtc64_read_register(RTC64_REG_CENTURY);
        if (rtc64_wait_not_updating() == 0u)
        {
            return 0u;
        }
        again[0] = rtc64_read_register(RTC64_REG_SECONDS);
        again[1] = rtc64_read_register(RTC64_REG_MINUTES);
        again[2] = rtc64_read_register(RTC64_REG_HOURS);
        again[3] = rtc64_read_register(RTC64_REG_DAY);
        again[4] = rtc64_read_register(RTC64_REG_MONTH);
        again[5] = rtc64_read_register(RTC64_REG_YEAR);
        again[6] = rtc64_read_register(RTC64_REG_CENTURY);
        for (index = 0u; index < 7u; ++index)
        {
            if (raw[index] != again[index])
            {
                break;
            }
        }
        if (index == 7u)
        {
            break;
        }
    }
    if (attempt == 4u)
    {
        return 0u;
    }

    status_b = rtc64_read_register(RTC64_REG_STATUS_B);
    hour_pm = ((status_b & RTC64_STATUS_B_24_HOUR) == 0u) && ((raw[2] & 0x80u) != 0u);
    raw[2] = (u8)(raw[2] & 0x7Fu);
    if ((status_b & RTC64_STATUS_B_BINARY) == 0u)
    {
        for (index = 0u; index < 7u; ++index)
        {
            raw[index] = (u8)rtc64_bcd_to_binary(raw[index]);
        }
    }
    if (hour_pm != 0u)
    {
        raw[2] = (u8)((raw[2] % 12u) + 12u);
    }
    else if (((status_b & RTC64_STATUS_B_24_HOUR) == 0u) && (raw[2] == 12u))
    {
        raw[2] = 0u;
    }

    /* The century register is not standard everywhere; accept it only when plausible. */
    century = ((raw[6] >= 19u) && (raw[6] <= 30u)) ? raw[6] : 20u;
    out->second = raw[0];
    out->minute = raw[1];
    out->hour = raw[2];
    out->day = raw[3];
    out->month = raw[4];
    out->year = (century * 100u) + raw[5];

    return ((out->second < 60u) && (out->minute < 60u) && (out->hour < 24u)
        && (out->day >= 1u) && (out->day <= 31u) && (out->month >= 1u) && (out->month <= 12u)
        && (out->year >= 2000u))
        ? 1u
        : 0u;
}

static void rtc64_refresh(void)
{
    rtc64_datetime_t now;
    u32 ticks = pit_get_ticks();
    u32 frequency = pit_get_frequency_hz();
    u64 epoch;

    if (frequency == 0u)
    {
        frequency = 100u;
    }
    /* Re-read at most every quarter second of PIT time; in between, the cached value stands. */
    if ((g_rtc64_read_attempted != 0u) && ((ticks - g_rtc64_epoch_ticks) < (frequency / 4u)))
    {
        return;
    }
    g_rtc64_read_attempted = 1u;
    g_rtc64_epoch_ticks = ticks;
    if (rtc64_read_datetime(&now) == 0u)
    {
        return;
    }
    epoch = (rtc64_days_from_civil(now.year, now.month, now.day) * 86400ull)
        + ((u64)now.hour * 3600ull) + ((u64)now.minute * 60ull) + (u64)now.second;
    if ((g_rtc64_valid == 0u) || (epoch != g_rtc64_epoch))
    {
        g_rtc64_second_start_ticks = ticks;
    }
    g_rtc64_epoch = epoch;
    g_rtc64_valid = 1u;
}

u32 rtc64_available(void)
{
    rtc64_refresh();
    return g_rtc64_valid;
}

u64 rtc64_now_epoch_seconds(void)
{
    rtc64_refresh();
    return (g_rtc64_valid != 0u) ? g_rtc64_epoch : 0ull;
}

u32 rtc64_now_subsecond_nanoseconds(void)
{
    u32 frequency = pit_get_frequency_hz();
    u32 elapsed;

    rtc64_refresh();
    if ((g_rtc64_valid == 0u) || (frequency == 0u))
    {
        return 0u;
    }
    elapsed = pit_get_ticks() - g_rtc64_second_start_ticks;
    if (elapsed >= frequency)
    {
        elapsed = frequency - 1u;
    }
    return (u32)(((u64)elapsed * 1000000000ull) / (u64)frequency);
}

void rtc64_epoch_to_datetime(u64 epoch_seconds, rtc64_datetime_t *out)
{
    u64 days = epoch_seconds / 86400ull;
    u32 rem = (u32)(epoch_seconds % 86400ull);
    /* civil_from_days */
    u64 z = days + 719468ull;
    u64 era = z / 146097ull;
    u32 doe = (u32)(z - (era * 146097ull));
    u32 yoe = (doe - (doe / 1460u) + (doe / 36524u) - (doe / 146096u)) / 365u;
    u32 doy = doe - ((365u * yoe) + (yoe / 4u) - (yoe / 100u));
    u32 mp = ((5u * doy) + 2u) / 153u;
    u32 day = doy - (((153u * mp) + 2u) / 5u) + 1u;
    u32 month = (mp < 10u) ? (mp + 3u) : (mp - 9u);
    u64 year = (u64)yoe + (era * 400ull) + ((month <= 2u) ? 1ull : 0ull);

    if (out == 0)
    {
        return;
    }
    out->year = (u32)year;
    out->month = month;
    out->day = day;
    out->hour = rem / 3600u;
    out->minute = (rem / 60u) % 60u;
    out->second = rem % 60u;
}

static u32 rtc64_put_digits(char *output, u32 cursor, u32 value, u32 digits)
{
    u32 index;

    for (index = digits; index > 0u; --index)
    {
        output[cursor + index - 1u] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    return cursor + digits;
}

u32 rtc64_format_now(char *output, u32 capacity)
{
    static const char suffix[] = " UTC";
    rtc64_datetime_t now;
    u32 cursor = 0u;
    u32 index;

    if ((output == 0) || (capacity < 24u) || (rtc64_available() == 0u))
    {
        return 0u;
    }
    rtc64_epoch_to_datetime(g_rtc64_epoch, &now);
    cursor = rtc64_put_digits(output, cursor, now.year, 4u);
    output[cursor++] = '-';
    cursor = rtc64_put_digits(output, cursor, now.month, 2u);
    output[cursor++] = '-';
    cursor = rtc64_put_digits(output, cursor, now.day, 2u);
    output[cursor++] = ' ';
    cursor = rtc64_put_digits(output, cursor, now.hour, 2u);
    output[cursor++] = ':';
    cursor = rtc64_put_digits(output, cursor, now.minute, 2u);
    output[cursor++] = ':';
    cursor = rtc64_put_digits(output, cursor, now.second, 2u);
    for (index = 0u; suffix[index] != '\0'; ++index)
    {
        output[cursor++] = suffix[index];
    }
    output[cursor] = '\0';
    return cursor;
}

u32 rtc64_format_now_hhmm(char *output, u32 capacity)
{
    rtc64_datetime_t now;
    u32 cursor = 0u;

    if ((output == 0) || (capacity < 6u) || (rtc64_available() == 0u))
    {
        return 0u;
    }
    rtc64_epoch_to_datetime(g_rtc64_epoch, &now);
    cursor = rtc64_put_digits(output, cursor, now.hour, 2u);
    output[cursor++] = ':';
    cursor = rtc64_put_digits(output, cursor, now.minute, 2u);
    output[cursor] = '\0';
    return cursor;
}
