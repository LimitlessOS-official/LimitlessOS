#ifndef LIMITLESS_POWER_X64_H
#define LIMITLESS_POWER_X64_H

#include "types.h"

/*
 * Machine power control (UEFI kernel). power64_reboot tries the ACPI reset
 * register, the PCI reset port 0xCF9, and the keyboard controller in turn.
 * power64_shutdown enters ACPI S5 through PM1a/PM1b_CNT with the _S5_ sleep
 * type from the DSDT. Both return only when every method failed.
 */
void power64_reboot(void);
void power64_shutdown(void);

#endif
