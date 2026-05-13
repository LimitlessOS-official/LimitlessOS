#ifndef LIMITLESS_DISPLAY_X64_H
#define LIMITLESS_DISPLAY_X64_H

#include "boot_info.h"
#include "types.h"

#define DISPLAY64_INVALID_RESULT 0xFFFFFFFFu

void display64_init(const struct boot_info *boot_info);
u32 display64_draw_marker(u32 display_capability_handle, u32 x, u32 y, u32 rgb, u32 owner_id);
u32 display64_clear_text_panel(u32 display_capability_handle, u32 owner_id);
u32 display64_write_text(u32 display_capability_handle, u64 input_address, u32 byte_count, u32 owner_id);
u32 display64_write_console_stream(u64 input_address, u32 byte_count);
u32 display64_write_console_stream_kernel(const u8 *input, u32 byte_count);
u32 display64_write_boot_diagnostics(
    u32 xhci_found,
    u32 xhci_handoff,
    u32 xhci_usb2_ports,
    u32 xhci_hid_device,
    u32 xhci_error,
    u32 ps2_present,
    u32 ps2_enabled,
    u32 ps2_scanning,
    u32 ps2_status,
    u32 ps2_config,
    u32 ps2_ack,
    u32 keyboard_scancodes,
    u32 keyboard_pending,
    u32 keyboard_last_scancode,
    u32 i2c_hid_present);
u32 display64_write_mouse_diagnostics(
    u32 init_done,
    u32 aux_enabled,
    u32 config_read,
    u32 config_write,
    u32 irq12_configured,
    u32 enable_command,
    u32 ack,
    u32 irq_count,
    u32 packet_count,
    u32 pending_count,
    u32 x,
    u32 y,
    u32 buttons);
void display64_compositor_probe(u32 cursor_x, u32 cursor_y, u32 buttons);
u32 display64_compositor_update_cursor(u32 cursor_x, u32 cursor_y, u32 buttons);
u32 display64_compositor_init_done(void);
u32 display64_compositor_present_count(void);
u32 display64_compositor_cursor_count(void);
void display64_font_probe(void);
u32 display64_font_init_done(void);
u32 display64_font_glyph_count(void);
u32 display64_font_render_count(void);
void display64_login_setup_screen(void);
void display64_login_screen_draw(const char *title, const char *message, u32 failures, u32 lockout_seconds);
void display64_wm_probe(void);
u32 display64_wm_process_mouse_event(u32 x, u32 y, u32 buttons, s32 dx, s32 dy);
u32 display64_wm_process_keyboard_event(u8 value);
u32 display64_wm_init_done(void);
u32 display64_wm_window_created_count(void);
u32 display64_wm_focus_count(void);
u32 display64_wm_present_count(void);
void display64_desktop_probe(void);
u32 display64_desktop_init_done(void);
u32 display64_desktop_taskbar_count(void);
u32 display64_desktop_launcher_count(void);
u32 display64_desktop_terminal_count(void);
u32 display64_desktop_fileman_count(void);
u32 display64_desktop_settings_count(void);
u32 display64_pkg_settings_panel_count(void);
u32 display64_identity_settings_panel_count(void);
u32 display64_identity_transport_settings_panel_count(void);
u32 display64_account_settings_panel_count(void);
u32 display64_gui_interactive(void);
u32 display64_gui_click_hittest(void);
u32 display64_gui_launcher_opened(void);
u32 display64_gui_terminal_opened(void);
u32 display64_gui_drag_completed(void);
u32 display64_gui_keyboard_routed(void);
u32 display64_gui_close_completed(void);
u32 display64_gui_taskbar_focus(void);
u32 display64_gui_fileman_opened(void);
u32 display64_gui_settings_opened(void);
u32 display64_gui_unfocused_key_denied(void);
u32 display64_gui_no_ambient_input(void);
u32 display64_gui_no_ambient_display(void);
u32 display64_gui_no_ambient_fs(void);
u32 display64_gui_mouse_x(void);
u32 display64_gui_mouse_y(void);
u32 display64_gui_target_window(void);
u32 display64_gui_target_region(void);
u32 display64_gui_focus_before(void);
u32 display64_gui_focus_after(void);
u32 display64_gui_z_before(void);
u32 display64_gui_z_after(void);
u32 display64_gui_key_target_window(void);
u32 display64_gui_unfocused_key_denial_count(void);
u32 display64_gui_input_path_token(void);
u32 display64_gui_display_path_token(void);
u32 display64_gui_fs_path_token(void);
u32 display64_available(void);
u32 display64_width(void);
u32 display64_height(void);
u32 display64_draw_count(void);
u32 display64_pixel_count(void);
u32 display64_denial_count(void);
u32 display64_unavailable_count(void);
u32 display64_last_token(void);
u32 display64_text_write_count(void);
u32 display64_text_byte_count(void);
u32 display64_clear_count(void);
u32 display64_console_write_count(void);
u32 display64_console_byte_count(void);
u32 display64_console_line_clear_count(void);
u32 display64_console_wrap_count(void);
u32 display64_console_scroll_count(void);

#endif
