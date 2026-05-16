#ifndef LIMITLESS_INPUT_X64_H
#define LIMITLESS_INPUT_X64_H

#include "types.h"

#define INPUT64_INVALID_RESULT 0xFFFFFFFFu
#define INPUT64_SEEDED_COMMAND_BYTES 213u
#define INPUT64_KEYBOARD_SCANCODE_SET1 1u
#define INPUT64_KEYBOARD_SCANCODE_SET2 2u

void input64_init(void);
void input64_clear_keyboard_pending(void);
void input64_set_keyboard_scancode_set(u32 scancode_set);
u32 input64_keyboard_scancode_set(void);
void input64_poll_keyboard(void);
void input64_poll_mouse(void);
void input64_handle_keyboard_interrupt(void);
void input64_handle_mouse_interrupt(void);
void input64_accept_usb_hid_boot_report(const u8 *report, u32 byte_count);
void input64_accept_usb_hid_mouse_report(const u8 *report, u32 byte_count);
void input64_set_mouse_bounds(u32 width, u32 height);
u32 input64_read(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id);
u32 input64_read_line(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id);
u32 input64_read_keyboard(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id);
u32 input64_read_keyboard_line(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id);
u32 input64_read_mouse(u32 input_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id);
u32 input64_read_count(void);
u32 input64_byte_count(void);
u32 input64_denial_count(void);
u32 input64_eof_count(void);
u32 input64_line_count(void);
u32 input64_edit_count(void);
u32 input64_keyboard_irq_count(void);
u32 input64_keyboard_poll_count(void);
u32 input64_keyboard_scancode_count(void);
u32 input64_keyboard_byte_count(void);
u32 input64_keyboard_pending_count(void);
u32 input64_keyboard_drop_count(void);
u32 input64_keyboard_last_scancode(void);
u32 input64_keyboard_last_byte(void);
u32 input64_keyboard_read_count(void);
u32 input64_keyboard_read_byte_count(void);
u32 input64_keyboard_line_count(void);
u32 input64_keyboard_line_byte_count(void);
u32 input64_keyboard_line_edit_count(void);
u32 input64_ps2_status_snapshot(void);
u32 input64_ps2_present(void);
u32 input64_ps2_enable_attempted(void);
u32 input64_ps2_enabled(void);
u32 input64_ps2_enable_status(void);
u32 input64_ps2_config_byte(void);
u32 input64_ps2_device_ack(void);
u32 input64_ps2_scanning_enabled(void);
u32 input64_ps2_recommended_scancode_set(void);
u32 input64_ps2_self_test(void);
u32 input64_ps2_first_port_test(void);
u32 input64_ps2_reset_ack(void);
u32 input64_ps2_reset_self_test(void);
u32 input64_mouse_found(void);
u32 input64_mouse_enabled(void);
u32 input64_mouse_irq_count(void);
u32 input64_mouse_poll_count(void);
u32 input64_mouse_packet_count(void);
u32 input64_mouse_pending_count(void);
u32 input64_mouse_drop_count(void);
u32 input64_mouse_delta_seen(void);
u32 input64_mouse_button_seen(void);
u32 input64_mouse_buttons(void);
u32 input64_mouse_x(void);
u32 input64_mouse_y(void);
u32 input64_mouse_last_dx(void);
u32 input64_mouse_last_dy(void);
u32 input64_mouse_read_count(void);
u32 input64_mouse_read_packet_count(void);
u32 input64_ps2_mouse_init_done(void);
u32 input64_ps2_mouse_aux_enabled(void);
u32 input64_ps2_mouse_config_read(void);
u32 input64_ps2_mouse_config_write(void);
u32 input64_ps2_mouse_irq12_configured(void);
u32 input64_ps2_mouse_enable_command(void);
u32 input64_ps2_mouse_ack(void);
u32 input64_ps2_mouse_raw_byte(void);
u32 input64_ps2_mouse_bad_start_count(void);

#endif
