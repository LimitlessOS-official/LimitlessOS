#ifndef LIMITLESS_PIT_H
#define LIMITLESS_PIT_H

#include "types.h"

void pit_initialize(u32 frequency_hz);
void pit_handle_interrupt(void);
u32 pit_get_ticks(void);
u32 pit_get_frequency_hz(void);
u32 pit_get_uptime_seconds(void);

#endif
