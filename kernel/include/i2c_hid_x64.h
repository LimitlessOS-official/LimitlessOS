#ifndef LIMITLESS_I2C_HID_X64_H
#define LIMITLESS_I2C_HID_X64_H

#include "types.h"

void i2c_hid64_init(void);
void i2c_hid64_poll_keyboard(void);
void i2c_hid64_poll_pointer(void);
u32 i2c_hid64_controller_present(void);
u32 i2c_hid64_device_found(void);
u32 i2c_hid64_report_count(void);
u32 i2c_hid64_error(void);
u32 i2c_hid64_pointer_found(void);
u32 i2c_hid64_pointer_report_count(void);
u32 i2c_hid64_pointer_error(void);

#endif
