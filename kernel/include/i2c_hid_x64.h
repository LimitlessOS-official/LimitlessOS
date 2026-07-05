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
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 i2c_hid64_pointer_kind(void);
u32 i2c_hid64_pointer_address(void);
u32 i2c_hid64_pointer_descriptor_register(void);
u32 i2c_hid64_pointer_report_descriptor_register(void);
u32 i2c_hid64_pointer_report_descriptor_length(void);
u32 i2c_hid64_pointer_input_register(void);
u32 i2c_hid64_pointer_command_register(void);
u32 i2c_hid64_pointer_max_input_length(void);
u32 i2c_hid64_pointer_report_has_id(void);
#endif
u32 i2c_hid64_pointer_report_count(void);
u32 i2c_hid64_pointer_error(void);

#endif
