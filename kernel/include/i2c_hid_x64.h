#ifndef LIMITLESS_I2C_HID_X64_H
#define LIMITLESS_I2C_HID_X64_H

#include "types.h"

#define I2C_HID64_ACPI_TELEMETRY_FOUND 0u
#define I2C_HID64_ACPI_TELEMETRY_BIND_SOURCE 1u
#define I2C_HID64_ACPI_TELEMETRY_ADDRESS 2u
#define I2C_HID64_ACPI_TELEMETRY_ADDRESS_PLAUSIBLE 3u
#define I2C_HID64_ACPI_TELEMETRY_SPEED_HZ 4u
#define I2C_HID64_ACPI_TELEMETRY_GPIO_FOUND 5u
#define I2C_HID64_ACPI_TELEMETRY_GPIO_PIN 6u

void i2c_hid64_init(void);
void i2c_hid64_poll_keyboard(void);
void i2c_hid64_poll_pointer(void);
u32 i2c_hid64_controller_present(void);
u32 i2c_hid64_device_found(void);
u32 i2c_hid64_report_count(void);
u32 i2c_hid64_error(void);
u32 i2c_hid64_pointer_found(void);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 i2c_hid64_primary_probe_address_count(void);
u32 i2c_hid64_pointer_probe_address_count(void);
u32 i2c_hid64_pointer_kind(void);
u32 i2c_hid64_pointer_address(void);
u32 i2c_hid64_acpi_telemetry(u32 field);
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
