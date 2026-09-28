#ifndef LIMITLESS_RTC_X64_H
#define LIMITLESS_RTC_X64_H

#include "types.h"

/* Calendar time from the CMOS real-time clock (UEFI kernel). Times are UTC as the firmware keeps them. */
typedef struct rtc64_datetime
{
    u32 year;
    u32 month;
    u32 day;
    u32 hour;
    u32 minute;
    u32 second;
} rtc64_datetime_t;

u32 rtc64_available(void);
/* Seconds since 1970-01-01 UTC, or 0 when the clock is unavailable. */
u64 rtc64_now_epoch_seconds(void);
/* Nanoseconds into the current second, interpolated from the PIT. */
u32 rtc64_now_subsecond_nanoseconds(void);
void rtc64_epoch_to_datetime(u64 epoch_seconds, rtc64_datetime_t *out);
/* Writes "YYYY-MM-DD HH:MM:SS UTC" (24 bytes including NUL); returns the length or 0. */
u32 rtc64_format_now(char *output, u32 capacity);
/* Writes "HH:MM" (6 bytes including NUL); returns the length or 0. */
u32 rtc64_format_now_hhmm(char *output, u32 capacity);

#endif
