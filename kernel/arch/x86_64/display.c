#include "display_x64.h"

#include "arch_build.h"
#include "ai_policy_x64.h"
#include "auth_x64.h"
#include "account_association_x64.h"
#include "capability_x64.h"
#include "cloud_storage_x64.h"
#include "hardware_registry_x64.h"
#include "i2c_hid_x64.h"
#include "identity_x64.h"
#include "identity_transport_x64.h"
#include "input_x64.h"
#include "installer_ux_x64.h"
#include "launch_x64.h"
#include "mmio_x64.h"
#include "package_signing_x64.h"
#include "pit.h"
#include "principal_x64.h"
#include "runtime_image_x64.h"
#include "services.h"
#include "services_x64.h"
#include "xhci_x64.h"

#define DISPLAY64_MARKER_WIDTH 16u
#define DISPLAY64_MARKER_HEIGHT 8u
#define DISPLAY64_MAX_TEXT_BYTES 128u
#define DISPLAY64_MAX_CONSOLE_BYTES 512u
#define DISPLAY64_CONSOLE_REPLAY_BYTES 8192u
#define DISPLAY64_KERNEL_HIGH_BASE_HIGH32 0xFFFFFFFFu
#define DISPLAY64_KERNEL_HIGH_BASE_LOW32 0x80000000u
#define DISPLAY64_TEXT_START_X 24u
#define DISPLAY64_TEXT_START_Y 96u
#define DISPLAY64_FONT_WIDTH 5u
#define DISPLAY64_FONT_HEIGHT 7u
#define DISPLAY64_FONT_DEFAULT_SCALE 2u
#define DISPLAY64_FONT_MAX_SCALE 3u
#define DISPLAY64_FONT_ADVANCE_DEFAULT ((DISPLAY64_FONT_WIDTH + 1u) * DISPLAY64_FONT_DEFAULT_SCALE)
#define DISPLAY64_LINE_ADVANCE_DEFAULT ((DISPLAY64_FONT_HEIGHT + 2u) * DISPLAY64_FONT_DEFAULT_SCALE)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define DISPLAY64_UI_STYLE_GENERATION 1u
#define DISPLAY64_RGB_DESKTOP_BG 0x00101213u
#define DISPLAY64_RGB_BAR_BG 0x0015191Bu
#define DISPLAY64_RGB_SURFACE 0x001B2023u
#define DISPLAY64_RGB_SURFACE_HIGH 0x0022282Cu
#define DISPLAY64_RGB_SURFACE_BORDER 0x00343C42u
#define DISPLAY64_RGB_SURFACE_BORDER_STRONG 0x0055616Au
#define DISPLAY64_RGB_CONTENT 0x0015191Bu
#define DISPLAY64_RGB_TITLE_UNFOCUSED 0x001B2023u
#define DISPLAY64_RGB_TASK_BUTTON 0x0022282Cu
#define DISPLAY64_RGB_TASK_BUTTON_BORDER 0x00343C42u
#define DISPLAY64_RGB_POPOVER_HOVER 0x002A3136u
#define DISPLAY64_RGB_ACCENT 0x0035B88Eu
#define DISPLAY64_RGB_FOCUS_BLUE 0x004D8FEAu
#define DISPLAY64_RGB_TEXT_PRIMARY 0x00F2F4F3u
#define DISPLAY64_RGB_TEXT_SECONDARY 0x00AEB6B8u
#define DISPLAY64_RGB_TEXT_MUTED 0x007C8588u
#else
#define DISPLAY64_UI_STYLE_GENERATION 0u
#define DISPLAY64_RGB_DESKTOP_BG 0x00121416u
#define DISPLAY64_RGB_BAR_BG 0x001A1D20u
#define DISPLAY64_RGB_SURFACE 0x0024262Au
#define DISPLAY64_RGB_SURFACE_HIGH DISPLAY64_RGB_SURFACE
#define DISPLAY64_RGB_SURFACE_BORDER 0x00434A50u
#define DISPLAY64_RGB_SURFACE_BORDER_STRONG DISPLAY64_RGB_SURFACE_BORDER
#define DISPLAY64_RGB_CONTENT 0x001B2024u
#define DISPLAY64_RGB_TITLE_UNFOCUSED 0x002A3035u
#define DISPLAY64_RGB_TASK_BUTTON 0x00252B30u
#define DISPLAY64_RGB_TASK_BUTTON_BORDER 0x004B565Eu
#define DISPLAY64_RGB_POPOVER_HOVER 0x00303934u
#define DISPLAY64_RGB_ACCENT 0x0037B088u
#define DISPLAY64_RGB_FOCUS_BLUE 0x004A90E2u
#define DISPLAY64_RGB_TEXT_PRIMARY 0x00F0F0F2u
#define DISPLAY64_RGB_TEXT_SECONDARY 0x009A9AA8u
#define DISPLAY64_RGB_TEXT_MUTED DISPLAY64_RGB_TEXT_SECONDARY
#endif
#define DISPLAY64_RGB_TEXT_ON_ACCENT 0x00FFFFFFu
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define DISPLAY64_RGB_FIELD 0x000B0F10u
#define DISPLAY64_RGB_CLOSE 0x00D96161u
#define DISPLAY64_RGB_APP_TERMINAL DISPLAY64_RGB_ACCENT
#define DISPLAY64_RGB_APP_FILES DISPLAY64_RGB_FOCUS_BLUE
#define DISPLAY64_RGB_APP_SETTINGS 0x008E76D8u
#define DISPLAY64_RGB_APP_ASSISTANT 0x00D8A63Au
#define DISPLAY64_RGB_APP_INSTALLER 0x00168E6Eu
#define DISPLAY64_RGB_WARNING 0x00D8A63Au
#define DISPLAY64_RGB_DISABLED_TEXT 0x005E6669u
#define DISPLAY64_RGB_SHADOW 0x00060809u
#define DISPLAY64_RGB_HIGHLIGHT 0x00313A40u
#else
#define DISPLAY64_RGB_FIELD 0x00161618u
#define DISPLAY64_RGB_CLOSE 0x00D45D5Du
#define DISPLAY64_RGB_APP_TERMINAL 0x0037B088u
#define DISPLAY64_RGB_APP_FILES 0x004A90E2u
#define DISPLAY64_RGB_APP_SETTINGS 0x008B6BD6u
#define DISPLAY64_RGB_APP_ASSISTANT 0x00C97941u
#define DISPLAY64_RGB_APP_INSTALLER 0x00B79E42u
#define DISPLAY64_RGB_WARNING 0x00D9A441u
#define DISPLAY64_RGB_DISABLED_TEXT 0x006E6E78u
#define DISPLAY64_RGB_SHADOW 0x00000000u
#define DISPLAY64_RGB_HIGHLIGHT DISPLAY64_RGB_SURFACE_BORDER
#endif
#define DISPLAY64_TEXT_RGB DISPLAY64_RGB_TEXT_PRIMARY
#define DISPLAY64_PANEL_X DISPLAY64_TEXT_START_X
#define DISPLAY64_PANEL_Y DISPLAY64_TEXT_START_Y
#define DISPLAY64_PANEL_WIDTH 360u
#define DISPLAY64_PANEL_HEIGHT (DISPLAY64_LINE_ADVANCE_DEFAULT + 4u)
#define DISPLAY64_PANEL_RGB DISPLAY64_RGB_CONTENT
#define DISPLAY64_CONSOLE_VIEWPORT_WIDTH 960u
#define DISPLAY64_CONSOLE_VIEWPORT_HEIGHT 648u
#define DISPLAY64_DIAG_MARGIN 16u
#define DISPLAY64_DIAG_PANEL_WIDTH 424u
#define DISPLAY64_DIAG_PANEL_HEIGHT 144u
#define DISPLAY64_DIAG_RGB 0x00182214u
#define DISPLAY64_DIAG_TEXT_RGB DISPLAY64_RGB_TEXT_PRIMARY
#define DISPLAY64_MOUSE_DIAG_PANEL_WIDTH 336u
#define DISPLAY64_MOUSE_DIAG_PANEL_HEIGHT 144u
#define DISPLAY64_MOUSE_DIAG_RGB 0x0014212Cu
#define DISPLAY64_MOUSE_DIAG_TEXT_RGB 0x00D9F7FFu
#define DISPLAY64_COMPOSITOR_BYTES_PER_PIXEL 4ull
#define DISPLAY64_KERNEL_HIGH_BASE 0xFFFFFFFF80000000ull
#define DISPLAY64_KERNEL_FALLBACK_WINDOW_BYTES 0x01000000ull
#define DISPLAY64_PAGE_BYTES 4096ull
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define DISPLAY64_COMPOSITOR_CURSOR_WIDTH 22u
#define DISPLAY64_COMPOSITOR_CURSOR_HEIGHT 32u
#else
#define DISPLAY64_COMPOSITOR_CURSOR_WIDTH 12u
#define DISPLAY64_COMPOSITOR_CURSOR_HEIGHT 20u
#endif
#define DISPLAY64_COMPOSITOR_CURSOR_RGB DISPLAY64_RGB_TEXT_PRIMARY
#define DISPLAY64_COMPOSITOR_CURSOR_SHADOW_RGB 0x00000000u
#define DISPLAY64_FONT_TRANSPARENT 0xFFFFFFFFu
#define DISPLAY64_FONT_SMALL 0u
#define DISPLAY64_FONT_NORMAL 1u
#define DISPLAY64_FONT_LARGE 2u
#define DISPLAY64_FONT_GLYPHS 256u
#define DISPLAY64_STATUS_BAR_HEIGHT 24u
#define DISPLAY64_DESKTOP_TASKBAR_HEIGHT 32u
#define DISPLAY64_DESKTOP_LAUNCHER_WIDTH 228u
#define DISPLAY64_DESKTOP_LAUNCHER_HEIGHT 168u
#define DISPLAY64_DESKTOP_LAUNCHER_BUTTON_X 8u
#define DISPLAY64_DESKTOP_LAUNCHER_BUTTON_WIDTH 28u
#define DISPLAY64_DESKTOP_LAUNCHER_BUTTON_HEIGHT 24u
#define DISPLAY64_DESKTOP_WINDOW_BUTTON_X 48u
#define DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH 116u
#define DISPLAY64_DESKTOP_WINDOW_BUTTON_HEIGHT 24u
#define DISPLAY64_DESKTOP_WINDOW_BUTTON_GAP 8u
#define DISPLAY64_WM_MAX_WINDOWS 16u
#define DISPLAY64_WM_TITLE_HEIGHT 28u
#define DISPLAY64_WM_BORDER 1u
#define DISPLAY64_GUI_REGION_NONE 0u
#define DISPLAY64_GUI_REGION_DESKTOP 1u
#define DISPLAY64_GUI_REGION_TITLE 2u
#define DISPLAY64_GUI_REGION_CLOSE 3u
#define DISPLAY64_GUI_REGION_BODY 4u
#define DISPLAY64_GUI_REGION_TASKBAR_LAUNCHER 5u
#define DISPLAY64_GUI_REGION_TASKBAR_BUTTON 6u
#define DISPLAY64_GUI_REGION_LAUNCHER_PANEL 7u
#define DISPLAY64_GUI_REGION_LAUNCHER_TERMINAL 8u
#define DISPLAY64_GUI_REGION_LAUNCHER_FILEMAN 9u
#define DISPLAY64_GUI_REGION_LAUNCHER_SETTINGS 10u
#define DISPLAY64_GUI_REGION_LAUNCHER_ASSISTANT 11u
#define DISPLAY64_GUI_REGION_CONTEXT_MENU 12u
#define DISPLAY64_GUI_REGION_FILEMAN_ROW 13u
#define DISPLAY64_GUI_REGION_SETTINGS_ROW 14u
#define DISPLAY64_GUI_REGION_INSTALLER_ACTION 15u
#define DISPLAY64_GUI_REGION_TERMINAL_ACTION 16u
#define DISPLAY64_GUI_REGION_RESIZE 17u
#define DISPLAY64_GUI_REGION_MINIMIZE 18u
#define DISPLAY64_GUI_INPUT_PATH_TOKEN 0x494E5054u
#define DISPLAY64_GUI_DISPLAY_PATH_TOKEN 0x44495350u
#define DISPLAY64_GUI_FS_PATH_TOKEN 0x46535041u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define DISPLAY64_FILEMAN_MAX_ENTRIES 6u
#define DISPLAY64_FILEMAN_PATH_BYTES 64u
#define DISPLAY64_FILEMAN_EDIT_BYTES 64u
#define DISPLAY64_FILEMAN_PREVIEW_BYTES 112u
#define DISPLAY64_FILEMAN_PREVIEW_LINES 4u
#define DISPLAY64_FILEMAN_ROW_BASE_Y 48u
#define DISPLAY64_FILEMAN_ROW_STEP 34u
#define DISPLAY64_FILEMAN_ROW_HEIGHT 32u
#define DISPLAY64_FILEMAN_PREVIEW_GAP 8u
#define DISPLAY64_SETTINGS_VISIBLE_ROWS 5u
#define DISPLAY64_SETTINGS_ROW_STEP 40u
#define DISPLAY64_SETTINGS_DETAIL_Y 236u
#define DISPLAY64_SETTINGS_DETAIL_HEIGHT 58u
#define DISPLAY64_SETTINGS_READINESS_Y 306u
#define DISPLAY64_FILEMAN_NAV_UP 1u
#define DISPLAY64_FILEMAN_NAV_ROOT 2u
#define DISPLAY64_FILEMAN_NAV_APPS 3u
#define DISPLAY64_FILEMAN_NAV_DATA 4u
#define DISPLAY64_FILEMAN_NAV_NEW_NOTE 5u
#define DISPLAY64_FILEMAN_NAV_DELETE_NOTE 6u
#define DISPLAY64_FILEMAN_NAV_NEW_FOLDER 7u
#define DISPLAY64_FILEMAN_NAV_RENAME 8u
#define DISPLAY64_FILEMAN_NAV_MOVE 9u
#define DISPLAY64_FILEMAN_NAV_COPY 10u
#define DISPLAY64_SETTINGS_ROW_COUNT 14u
#define DISPLAY64_SETTINGS_CFG_BYTES 192u
#define DISPLAY64_SETTINGS_DIAG_BYTES 384u
#define DISPLAY64_SETTINGS_DETAIL_BYTES 64u
#define DISPLAY64_SETTINGS_THEME_DARK 0u
#define DISPLAY64_SETTINGS_THEME_LIGHT 1u
#define DISPLAY64_SETTINGS_POINTER_SLOW 1u
#define DISPLAY64_SETTINGS_POINTER_NORMAL 2u
#define DISPLAY64_SETTINGS_POINTER_FAST 3u
#define DISPLAY64_TERMINAL_SCROLL_STEP_BYTES 512u
#define DISPLAY64_TERMINAL_SELECTION_BYTES 128u
#define DISPLAY64_WM_MIN_WINDOW_WIDTH 180u
#define DISPLAY64_WM_MIN_WINDOW_HEIGHT 120u
#define DISPLAY64_WM_RESIZE_GRIP 18u
#define DISPLAY64_LOGIN_STATE_SETUP 1u
#define DISPLAY64_LOGIN_STATE_LOGIN 2u
#define DISPLAY64_LOGIN_STATE_ACCEPTED 3u
#define DISPLAY64_LOGIN_STATE_DENIED 4u
#define DISPLAY64_LOGIN_STATE_LOCKED 5u
#define DISPLAY64_LOGIN_STATE_UNLOCKED 6u
#define DISPLAY64_LOGIN_STATE_RECOVERY 7u
#define DISPLAY64_LOGIN_STATE_UNAVAILABLE 8u
#endif

static struct boot_info g_display_boot_info_storage;
static const struct boot_info *g_display_boot_info = 0;
static u32 g_display_draw_count = 0u;
static u32 g_display_pixel_count = 0u;
static u32 g_display_denial_count = 0u;
static u32 g_display_unavailable_count = 0u;
static u32 g_display_last_token = 0u;
static u32 g_display_text_write_count = 0u;
static u32 g_display_text_byte_count = 0u;
static u32 g_display_clear_count = 0u;
static u32 g_display_console_write_count = 0u;
static u32 g_display_console_byte_count = 0u;
static u32 g_display_console_line_clear_count = 0u;
static u32 g_display_console_wrap_count = 0u;
static u32 g_display_console_scroll_count = 0u;
static u32 g_display_console_line_dirty = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_display_console_clip_count = 0u;
#endif
static u8 g_display_console_replay[DISPLAY64_CONSOLE_REPLAY_BYTES];
static u32 g_display_console_replay_head = 0u;
static u32 g_display_console_replay_count = 0u;
static u32 g_display_console_replay_overflow = 0u;
static u32 g_display_text_x = DISPLAY64_TEXT_START_X;
static u32 g_display_text_y = DISPLAY64_TEXT_START_Y;
static u32 g_display_console_x = DISPLAY64_TEXT_START_X;
static u32 g_display_console_y = DISPLAY64_TEXT_START_Y;
static u32 g_display_console_w = DISPLAY64_CONSOLE_VIEWPORT_WIDTH;
static u32 g_display_console_h = DISPLAY64_CONSOLE_VIEWPORT_HEIGHT;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_display_text_scale = DISPLAY64_FONT_DEFAULT_SCALE;
static u32 g_display_stride_ok = 0u;
static u32 g_display_bounds_ok = 0u;
static u32 g_display_console_fit = 0u;
static u32 g_display_readable = 0u;
static u32 g_display_layout_token = 0u;
#endif
static u32 g_display_compositor_active = 0u;
static u32 g_display_compositor_present_count = 0u;
static u32 g_display_compositor_cursor_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_display_direct_cursor_count = 0u;
static u32 g_display_compositor_direct_mode = 0u;
#endif
static u32 g_display_compositor_cursor_x = 32u;
static u32 g_display_compositor_cursor_y = 32u;
static u32 g_display_compositor_cursor_buttons = 0u;
static u32 g_display_compositor_dirty = 0u;
static u32 g_display_compositor_dirty_x = 0u;
static u32 g_display_compositor_dirty_y = 0u;
static u32 g_display_compositor_dirty_w = 0u;
static u32 g_display_compositor_dirty_h = 0u;
static u32 g_display_compositor_cursor_saved[
    DISPLAY64_COMPOSITOR_CURSOR_WIDTH * DISPLAY64_COMPOSITOR_CURSOR_HEIGHT];
static u32 g_display_compositor_cursor_saved_x = 0u;
static u32 g_display_compositor_cursor_saved_y = 0u;
static u32 g_display_compositor_cursor_saved_w = 0u;
static u32 g_display_compositor_cursor_saved_h = 0u;
static u32 g_display_compositor_cursor_saved_valid = 0u;
static u32 g_display_compositor_cursor_drawn_valid = 0u;
static u32 g_display_compositor_cursor_drawn_x = 0u;
static u32 g_display_compositor_cursor_drawn_y = 0u;
static u32 g_display_compositor_cursor_drawn_buttons = 0u;
static u32 g_display_font_active = 0u;
static u32 g_display_font_render_count = 0u;
struct display64_window
{
    u32 handle;
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    const char *title;
    u32 visible;
    u32 focused;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 minimized;
#endif
    u32 z;
};
static struct display64_window g_display_windows[DISPLAY64_WM_MAX_WINDOWS];
static u32 g_display_wm_active = 0u;
static u32 g_display_wm_next_handle = 1u;
static u32 g_display_wm_next_z = 1u;
static u32 g_display_wm_window_count = 0u;
static u32 g_display_wm_focus_count = 0u;
static u32 g_display_wm_present_count = 0u;
static u32 g_display_wm_shell_handle = 0u;
static u32 g_display_wm_dragging = 0u;
static u32 g_display_wm_drag_handle = 0u;
static u32 g_display_wm_drag_offset_x = 0u;
static u32 g_display_wm_drag_offset_y = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_display_wm_resizing = 0u;
static u32 g_display_wm_resize_handle = 0u;
#endif
static u32 g_display_wm_last_buttons = 0u;
static u32 g_display_desktop_active = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_display_desktop_product_layout = 0u;
static u32 g_display_product_startup_minimized_count = 0u;
#endif
static u32 g_display_desktop_taskbar_count = 0u;
static u32 g_display_desktop_launcher_count = 0u;
static u32 g_display_desktop_terminal_count = 0u;
static u32 g_display_desktop_fileman_count = 0u;
static u32 g_display_desktop_settings_count = 0u;
static u32 g_display_desktop_assistant_count = 0u;
static u32 g_display_pkg_settings_panel_count = 0u;
static u32 g_display_identity_settings_panel_count = 0u;
static u32 g_display_identity_transport_settings_panel_count = 0u;
static u32 g_display_account_settings_panel_count = 0u;
static u32 g_display_cloud_settings_panel_count = 0u;
static u32 g_display_cloud_fileman_status_count = 0u;
static u32 g_display_ai_settings_panel_count = 0u;
static u32 g_display_installer_welcome_count = 0u;
static u32 g_display_installer_beginner_count = 0u;
static u32 g_display_installer_advanced_count = 0u;
static u32 g_display_installer_hardware_count = 0u;
static u32 g_display_installer_recommendation_count = 0u;
static u32 g_display_installer_component_count = 0u;
static u32 g_display_installer_account_count = 0u;
static u32 g_display_installer_cloud_count = 0u;
static u32 g_display_installer_ai_count = 0u;
static u32 g_display_installer_plan_count = 0u;
static u32 g_display_installer_dryrun_count = 0u;
static u32 g_display_desktop_fileman_handle = 0u;
static u32 g_display_desktop_settings_handle = 0u;
static u32 g_display_desktop_installer_handle = 0u;
static u32 g_display_desktop_assistant_handle = 0u;
static u32 g_display_desktop_launcher_open = 0u;
static u32 g_display_gui_interactive = 0u;
static u32 g_display_gui_click_hittest = 0u;
static u32 g_display_gui_launcher_opened = 0u;
static u32 g_display_gui_terminal_opened = 0u;
static u32 g_display_gui_drag_completed = 0u;
static u32 g_display_gui_keyboard_routed = 0u;
static u32 g_display_gui_close_completed = 0u;
static u32 g_display_gui_taskbar_focus = 0u;
static u32 g_display_gui_fileman_opened = 0u;
static u32 g_display_gui_settings_opened = 0u;
static u32 g_display_gui_installer_opened = 0u;
static u32 g_display_gui_assistant_opened = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_display_gui_keyboard_open_count = 0u;
static u32 g_display_input_diag_suppressed_count = 0u;
static u32 g_display_mouse_diag_suppressed_count = 0u;
#endif
static u32 g_display_gui_unfocused_key_denied = 0u;
static u32 g_display_gui_unfocused_key_denial_count = 0u;
static u32 g_display_gui_no_ambient_input = 0u;
static u32 g_display_gui_no_ambient_display = 0u;
static u32 g_display_gui_no_ambient_fs = 0u;
static u32 g_display_gui_mouse_x = 0u;
static u32 g_display_gui_mouse_y = 0u;
static u32 g_display_gui_target_window = 0u;
static u32 g_display_gui_target_region = DISPLAY64_GUI_REGION_NONE;
static u32 g_display_gui_focus_before = 0u;
static u32 g_display_gui_focus_after = 0u;
static u32 g_display_gui_z_before = 0u;
static u32 g_display_gui_z_after = 0u;
static u32 g_display_gui_key_target_window = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_display_gui_right_click_count = 0u;
static u32 g_display_gui_scroll_count = 0u;
static u32 g_display_settings_scroll_index = 0u;
static u32 g_display_context_menu_open = 0u;
static u32 g_display_context_menu_x = 0u;
static u32 g_display_context_menu_y = 0u;
static u32 g_display_context_menu_target = 0u;
static u32 g_display_context_menu_kind = 0u;
static u32 g_display_context_menu_action_count = 0u;
static u32 g_display_wm_resize_count = 0u;
static u32 g_display_wm_minimize_count = 0u;
static u32 g_display_wm_restore_count = 0u;
static u32 g_display_wm_zorder_count = 0u;
static u32 g_display_settings_loaded = 0u;
static u32 g_display_settings_theme = DISPLAY64_SETTINGS_THEME_DARK;
static u32 g_display_settings_pointer_speed = DISPLAY64_SETTINGS_POINTER_NORMAL;
static u32 g_display_settings_key_repeat = 1u;
static u32 g_display_settings_load_count = 0u;
static u32 g_display_settings_save_count = 0u;
static u32 g_display_settings_save_denial_count = 0u;
static u32 g_display_settings_export_count = 0u;
static u32 g_display_settings_export_denial_count = 0u;
static u32 g_display_settings_hardware_panel_count = 0u;
static u32 g_display_settings_input_panel_count = 0u;
static u32 g_display_settings_readiness_strip_count = 0u;
static u32 g_display_fileman_storage_card_count = 0u;
static u32 g_display_product_chrome_count = 0u;
static u8 g_display_settings_config[DISPLAY64_SETTINGS_CFG_BYTES];
static u8 g_display_settings_diag[DISPLAY64_SETTINGS_DIAG_BYTES];
static char g_display_settings_hardware_detail[DISPLAY64_SETTINGS_DETAIL_BYTES];
static char g_display_settings_input_detail[DISPLAY64_SETTINGS_DETAIL_BYTES];
static char g_display_settings_storage_detail[DISPLAY64_SETTINGS_DETAIL_BYTES];
static char g_display_settings_network_detail[DISPLAY64_SETTINGS_DETAIL_BYTES];
static char g_display_fileman_status_detail[DISPLAY64_SETTINGS_DETAIL_BYTES];
static u32 g_display_fileman_selected_index = 0u;
static u32 g_display_fileman_window_cursor = 0u;
static u32 g_display_settings_selected_index = 0u;
static u32 g_display_installer_step_index = 0u;
static u32 g_display_terminal_action_count = 0u;
static u32 g_display_terminal_scroll_offset = 0u;
static u32 g_display_terminal_scroll_count = 0u;
static u32 g_display_terminal_selection_active = 0u;
static u32 g_display_terminal_selection_anchor_x = 0u;
static u32 g_display_terminal_selection_anchor_y = 0u;
static u32 g_display_terminal_selection_x = 0u;
static u32 g_display_terminal_selection_y = 0u;
static u32 g_display_terminal_selection_count = 0u;
static u32 g_display_terminal_copy_count = 0u;
static u32 g_display_terminal_selection_bytes = 0u;
static u32 g_display_terminal_copied_bytes = 0u;
static u32 g_display_terminal_cursor_draw_count = 0u;
static u8 g_display_terminal_selection_buffer[DISPLAY64_TERMINAL_SELECTION_BYTES];
static u32 g_display_login_present_count = 0u;
static u32 g_display_login_setup_present_count = 0u;
static u32 g_display_login_lock_present_count = 0u;
static u32 g_display_login_unlock_present_count = 0u;
static u32 g_display_login_recovery_present_count = 0u;
static u32 g_display_login_wait_visible_count = 0u;
static u32 g_display_login_safe_path_count = 0u;
static u32 g_display_login_last_state = 0u;
static u32 g_display_fileman_action_count = 0u;
static u32 g_display_settings_action_count = 0u;
static u32 g_display_installer_action_count = 0u;
static u32 g_display_fileman_entry_count = 0u;
static u32 g_display_fileman_backend_refresh_count = 0u;
static u32 g_display_fileman_backend_preview_count = 0u;
static u32 g_display_fileman_backend_open_dir_count = 0u;
static u32 g_display_fileman_backend_write_count = 0u;
static u32 g_display_fileman_backend_write_denial_count = 0u;
static u32 g_display_fileman_backend_delete_count = 0u;
static u32 g_display_fileman_backend_delete_denial_count = 0u;
static u32 g_display_fileman_backend_delete_confirm_count = 0u;
static u32 g_display_fileman_backend_mkdir_count = 0u;
static u32 g_display_fileman_backend_mkdir_denial_count = 0u;
static u32 g_display_fileman_backend_copy_count = 0u;
static u32 g_display_fileman_backend_copy_denial_count = 0u;
static u32 g_display_fileman_backend_rename_count = 0u;
static u32 g_display_fileman_backend_rename_denial_count = 0u;
static u32 g_display_fileman_backend_move_count = 0u;
static u32 g_display_fileman_backend_move_denial_count = 0u;
static u32 g_display_fileman_backend_edit_count = 0u;
static u32 g_display_fileman_backend_edit_commit_count = 0u;
static u32 g_display_fileman_last_status = 0u;
static u32 g_display_fileman_last_write_status = 0u;
static u32 g_display_fileman_last_delete_status = 0u;
static u32 g_display_fileman_last_mutation_status = 0u;
static u32 g_display_fileman_delete_armed = 0u;
static u32 g_display_fileman_edit_mode = 0u;
static u32 g_display_fileman_edit_bytes = 0u;
static u8 g_display_fileman_current_path[DISPLAY64_FILEMAN_PATH_BYTES];
static u8 g_display_fileman_delete_path[DISPLAY64_FILEMAN_PATH_BYTES];
static u8 g_display_fileman_edit_buffer[DISPLAY64_FILEMAN_EDIT_BYTES];
static mmio64_nvme_fat_dirent_t g_display_fileman_entries[DISPLAY64_FILEMAN_MAX_ENTRIES];
static u8 g_display_fileman_preview[DISPLAY64_FILEMAN_PREVIEW_BYTES + 1u];
static u32 g_display_fileman_preview_bytes = 0u;
static u32 g_display_fileman_preview_size = 0u;
#endif
static u32 *g_display_back_buffer = 0;
static u64 g_display_back_buffer_pixels = 0ull;
static u64 g_display_back_buffer_bytes = 0ull;
static u64 g_display_back_buffer_next = 0ull;

extern u8 __bss_end[];

static void display64_set_boot_info(const struct boot_info *boot_info)
{
    if (boot_info == 0)
    {
        g_display_boot_info = 0;
        return;
    }

    g_display_boot_info_storage = *boot_info;
    g_display_boot_info = &g_display_boot_info_storage;
}

static const u8 g_display_alpha_font[26][DISPLAY64_FONT_HEIGHT] = {
    { 0x0Eu, 0x11u, 0x11u, 0x1Fu, 0x11u, 0x11u, 0x11u },
    { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x11u, 0x11u, 0x1Eu },
    { 0x0Eu, 0x11u, 0x10u, 0x10u, 0x10u, 0x11u, 0x0Eu },
    { 0x1Eu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x1Eu },
    { 0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x10u, 0x10u, 0x1Fu },
    { 0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x10u, 0x10u, 0x10u },
    { 0x0Eu, 0x11u, 0x10u, 0x17u, 0x11u, 0x11u, 0x0Fu },
    { 0x11u, 0x11u, 0x11u, 0x1Fu, 0x11u, 0x11u, 0x11u },
    { 0x1Fu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x1Fu },
    { 0x07u, 0x02u, 0x02u, 0x02u, 0x12u, 0x12u, 0x0Cu },
    { 0x11u, 0x12u, 0x14u, 0x18u, 0x14u, 0x12u, 0x11u },
    { 0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x1Fu },
    { 0x11u, 0x1Bu, 0x15u, 0x15u, 0x11u, 0x11u, 0x11u },
    { 0x11u, 0x19u, 0x15u, 0x13u, 0x11u, 0x11u, 0x11u },
    { 0x0Eu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Eu },
    { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x10u, 0x10u, 0x10u },
    { 0x0Eu, 0x11u, 0x11u, 0x11u, 0x15u, 0x12u, 0x0Du },
    { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x14u, 0x12u, 0x11u },
    { 0x0Fu, 0x10u, 0x10u, 0x0Eu, 0x01u, 0x01u, 0x1Eu },
    { 0x1Fu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u },
    { 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Eu },
    { 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Au, 0x04u },
    { 0x11u, 0x11u, 0x11u, 0x15u, 0x15u, 0x15u, 0x0Au },
    { 0x11u, 0x11u, 0x0Au, 0x04u, 0x0Au, 0x11u, 0x11u },
    { 0x11u, 0x11u, 0x0Au, 0x04u, 0x04u, 0x04u, 0x04u },
    { 0x1Fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x1Fu }
};

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static const u8 g_display_lower_font[26][DISPLAY64_FONT_HEIGHT] = {
    { 0x00u, 0x00u, 0x0Eu, 0x01u, 0x0Fu, 0x11u, 0x0Fu },
    { 0x10u, 0x10u, 0x16u, 0x19u, 0x11u, 0x19u, 0x16u },
    { 0x00u, 0x00u, 0x0Eu, 0x10u, 0x10u, 0x10u, 0x0Eu },
    { 0x01u, 0x01u, 0x0Du, 0x13u, 0x11u, 0x13u, 0x0Du },
    { 0x00u, 0x00u, 0x0Eu, 0x11u, 0x1Fu, 0x10u, 0x0Eu },
    { 0x06u, 0x08u, 0x1Eu, 0x08u, 0x08u, 0x08u, 0x08u },
    { 0x00u, 0x00u, 0x0Fu, 0x11u, 0x0Fu, 0x01u, 0x0Eu },
    { 0x10u, 0x10u, 0x16u, 0x19u, 0x11u, 0x11u, 0x11u },
    { 0x04u, 0x00u, 0x0Cu, 0x04u, 0x04u, 0x04u, 0x0Eu },
    { 0x02u, 0x00u, 0x06u, 0x02u, 0x02u, 0x12u, 0x0Cu },
    { 0x10u, 0x10u, 0x12u, 0x14u, 0x18u, 0x14u, 0x12u },
    { 0x0Cu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x0Eu },
    { 0x00u, 0x00u, 0x1Au, 0x15u, 0x15u, 0x15u, 0x15u },
    { 0x00u, 0x00u, 0x16u, 0x19u, 0x11u, 0x11u, 0x11u },
    { 0x00u, 0x00u, 0x0Eu, 0x11u, 0x11u, 0x11u, 0x0Eu },
    { 0x00u, 0x00u, 0x16u, 0x19u, 0x16u, 0x10u, 0x10u },
    { 0x00u, 0x00u, 0x0Du, 0x13u, 0x0Du, 0x01u, 0x01u },
    { 0x00u, 0x00u, 0x16u, 0x19u, 0x10u, 0x10u, 0x10u },
    { 0x00u, 0x00u, 0x0Fu, 0x10u, 0x0Eu, 0x01u, 0x1Eu },
    { 0x08u, 0x08u, 0x1Eu, 0x08u, 0x08u, 0x09u, 0x06u },
    { 0x00u, 0x00u, 0x11u, 0x11u, 0x11u, 0x13u, 0x0Du },
    { 0x00u, 0x00u, 0x11u, 0x11u, 0x0Au, 0x0Au, 0x04u },
    { 0x00u, 0x00u, 0x11u, 0x15u, 0x15u, 0x15u, 0x0Au },
    { 0x00u, 0x00u, 0x11u, 0x0Au, 0x04u, 0x0Au, 0x11u },
    { 0x00u, 0x00u, 0x11u, 0x11u, 0x0Fu, 0x01u, 0x0Eu },
    { 0x00u, 0x00u, 0x1Fu, 0x02u, 0x04u, 0x08u, 0x1Fu }
};
#endif

static const u8 g_display_digit_font[10][DISPLAY64_FONT_HEIGHT] = {
    { 0x0Eu, 0x11u, 0x13u, 0x15u, 0x19u, 0x11u, 0x0Eu },
    { 0x04u, 0x0Cu, 0x04u, 0x04u, 0x04u, 0x04u, 0x0Eu },
    { 0x0Eu, 0x11u, 0x01u, 0x02u, 0x04u, 0x08u, 0x1Fu },
    { 0x1Eu, 0x01u, 0x01u, 0x0Eu, 0x01u, 0x01u, 0x1Eu },
    { 0x02u, 0x06u, 0x0Au, 0x12u, 0x1Fu, 0x02u, 0x02u },
    { 0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x01u, 0x01u, 0x1Eu },
    { 0x0Eu, 0x10u, 0x10u, 0x1Eu, 0x11u, 0x11u, 0x0Eu },
    { 0x1Fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x08u, 0x08u },
    { 0x0Eu, 0x11u, 0x11u, 0x0Eu, 0x11u, 0x11u, 0x0Eu },
    { 0x0Eu, 0x11u, 0x11u, 0x0Fu, 0x01u, 0x01u, 0x0Eu }
};

static u32 display64_min_u32(u32 left, u32 right)
{
    return (left < right) ? left : right;
}

static int display64_has_framebuffer(void);
static u32 display64_console_viewport_width(void);
static u32 display64_console_viewport_height(void);

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 display64_font_scale_value(void)
{
    if (g_display_text_scale == 0u)
    {
        return DISPLAY64_FONT_DEFAULT_SCALE;
    }

    return g_display_text_scale;
}

static u32 display64_font_advance(void)
{
    return (DISPLAY64_FONT_WIDTH + 1u) * display64_font_scale_value();
}

static u32 display64_line_advance(void)
{
    return (DISPLAY64_FONT_HEIGHT + 2u) * display64_font_scale_value();
}

static u64 display64_required_framebuffer_bytes(void)
{
    u64 last_row_pixels;

    if ((g_display_boot_info == 0)
        || (g_display_boot_info->framebuffer_width == 0u)
        || (g_display_boot_info->framebuffer_height == 0u)
        || (g_display_boot_info->framebuffer_pixels_per_scanline == 0u))
    {
        return 0ull;
    }

    last_row_pixels = ((u64)(g_display_boot_info->framebuffer_height - 1u)
        * (u64)g_display_boot_info->framebuffer_pixels_per_scanline)
        + (u64)g_display_boot_info->framebuffer_width;
    return last_row_pixels * 4ull;
}

static u32 display64_choose_text_scale(void)
{
    if (!display64_has_framebuffer())
    {
        return DISPLAY64_FONT_DEFAULT_SCALE;
    }

    if ((g_display_boot_info->framebuffer_width >= 1800u)
        && (g_display_boot_info->framebuffer_height >= 1000u))
    {
        return DISPLAY64_FONT_MAX_SCALE;
    }
    if ((g_display_boot_info->framebuffer_width >= 1000u)
        && (g_display_boot_info->framebuffer_height >= 700u))
    {
        return DISPLAY64_FONT_DEFAULT_SCALE;
    }

    return 1u;
}

static void display64_refresh_layout_token(void)
{
    u32 token = 2166136261u;

    token ^= g_display_text_scale;
    token *= 16777619u;
    token ^= g_display_console_x;
    token *= 16777619u;
    token ^= g_display_console_y;
    token *= 16777619u;
    token ^= g_display_console_w;
    token *= 16777619u;
    token ^= g_display_console_h;
    token *= 16777619u;
    token ^= g_display_stride_ok;
    token *= 16777619u;
    token ^= g_display_bounds_ok;
    token *= 16777619u;
    token ^= g_display_console_fit;
    token *= 16777619u;
    token ^= g_display_readable;
    token *= 16777619u;
    g_display_layout_token = token;
}

static void display64_configure_console_layout(void)
{
    u64 required_bytes;
    u32 margin = 8u;
    u32 top_margin = 48u;
    u32 columns;
    u32 rows;

    g_display_text_scale = DISPLAY64_FONT_DEFAULT_SCALE;
    g_display_stride_ok = 0u;
    g_display_bounds_ok = 0u;
    g_display_console_fit = 0u;
    g_display_readable = 0u;
    g_display_console_x = DISPLAY64_TEXT_START_X;
    g_display_console_y = DISPLAY64_TEXT_START_Y;
    g_display_console_w = DISPLAY64_CONSOLE_VIEWPORT_WIDTH;
    g_display_console_h = DISPLAY64_CONSOLE_VIEWPORT_HEIGHT;

    if (!display64_has_framebuffer())
    {
        display64_refresh_layout_token();
        return;
    }

    g_display_text_scale = display64_choose_text_scale();
    if (g_display_boot_info->framebuffer_width >= 1200u)
    {
        margin = 24u;
    }
    if (g_display_boot_info->framebuffer_height >= 720u)
    {
        top_margin = 96u;
    }

    g_display_console_x = display64_min_u32(margin, g_display_boot_info->framebuffer_width - 1u);
    g_display_console_y = display64_min_u32(top_margin, g_display_boot_info->framebuffer_height - 1u);
    g_display_console_w = (g_display_boot_info->framebuffer_width > (g_display_console_x + margin))
        ? (g_display_boot_info->framebuffer_width - g_display_console_x - margin)
        : (g_display_boot_info->framebuffer_width - g_display_console_x);
    g_display_console_h = (g_display_boot_info->framebuffer_height > (g_display_console_y + margin))
        ? (g_display_boot_info->framebuffer_height - g_display_console_y - margin)
        : (g_display_boot_info->framebuffer_height - g_display_console_y);
    g_display_text_x = g_display_console_x;
    g_display_text_y = g_display_console_y;

    required_bytes = display64_required_framebuffer_bytes();
    g_display_stride_ok =
        (g_display_boot_info->framebuffer_pixels_per_scanline >= g_display_boot_info->framebuffer_width)
            ? 1u
            : 0u;
    g_display_bounds_ok =
        ((required_bytes != 0ull) && (required_bytes <= g_display_boot_info->framebuffer_bytes))
            ? 1u
            : 0u;
    columns = display64_console_viewport_width() / display64_font_advance();
    rows = display64_console_viewport_height() / display64_line_advance();
    g_display_console_fit = ((columns >= 40u) && (rows >= 16u)) ? 1u : 0u;
    g_display_readable =
        ((g_display_stride_ok != 0u) && (g_display_bounds_ok != 0u) && (g_display_console_fit != 0u))
            ? 1u
            : 0u;
    display64_refresh_layout_token();
}
#else
#define display64_font_scale_value() DISPLAY64_FONT_DEFAULT_SCALE
#define display64_font_advance() DISPLAY64_FONT_ADVANCE_DEFAULT
#define display64_line_advance() DISPLAY64_LINE_ADVANCE_DEFAULT
#endif

static int display64_format_supported(u32 format)
{
    return (format == LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_RGB)
        || (format == LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_BGR);
}

static int display64_has_framebuffer(void)
{
    if (g_display_boot_info == 0)
    {
        return 0;
    }

    if ((g_display_boot_info->bootstrap_flags & LIMITLESS_BOOT_FLAG_FRAMEBUFFER) == 0u)
    {
        return 0;
    }

    if ((g_display_boot_info->framebuffer_base == 0ull)
        || (g_display_boot_info->framebuffer_bytes == 0ull)
        || (g_display_boot_info->framebuffer_width == 0u)
        || (g_display_boot_info->framebuffer_height == 0u)
        || (g_display_boot_info->framebuffer_pixels_per_scanline < g_display_boot_info->framebuffer_width)
        || !display64_format_supported(g_display_boot_info->framebuffer_format))
    {
        return 0;
    }

    return 1;
}

static int display64_range_overflows(u64 address, u32 byte_count)
{
    u64 end;

    if (byte_count == 0u)
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return end < address;
}

static int display64_address_is_kernel_high(u64 address, u32 byte_count)
{
    u64 end;

    if (display64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return ((u32)(address >> 32) >= DISPLAY64_KERNEL_HIGH_BASE_HIGH32)
        && ((u32)address >= DISPLAY64_KERNEL_HIGH_BASE_LOW32)
        && (end >= address);
}

static int display64_address_is_user_stack(u64 address, u32 byte_count)
{
    u64 stack_base = (u64)(LAUNCH64_USER_STACK_TOP - LAUNCH64_USER_STACK_BYTES);
    u64 stack_top = (u64)LAUNCH64_USER_STACK_TOP;
    u64 end;

    if (display64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= stack_base) && (end <= stack_top);
}

static int display64_address_is_user_image(u64 address, u32 byte_count)
{
    u64 image_base = (u64)LAUNCH64_USER_IMAGE_BASE;
    u64 image_end = image_base + (u64)runtime64_transfer_image_size();
    u64 end;

    if (display64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= image_base) && (end <= image_end);
}

static int display64_address_readable(u64 address, u32 byte_count)
{
    if (byte_count == 0u)
    {
        return 1;
    }

    if (address == 0ull)
    {
        return 0;
    }

    return display64_address_is_kernel_high(address, byte_count)
        || display64_address_is_user_stack(address, byte_count)
        || display64_address_is_user_image(address, byte_count);
}

static int display64_pixel_index(u32 x, u32 y, u64 *pixel_index);

static u32 display64_make_pixel(u32 rgb)
{
    u32 red = (rgb >> 16) & 0xFFu;
    u32 green = (rgb >> 8) & 0xFFu;
    u32 blue = rgb & 0xFFu;

    if ((g_display_boot_info != 0)
        && (g_display_boot_info->framebuffer_format == LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_BGR))
    {
        return (red << 16) | (green << 8) | blue;
    }

    return (blue << 16) | (green << 8) | red;
}

static volatile u32 *display64_physical_framebuffer(void)
{
    return (volatile u32 *)(u64)g_display_boot_info->framebuffer_base;
}

static volatile u32 *display64_draw_buffer(void)
{
    if ((g_display_compositor_active != 0u) && (g_display_back_buffer != 0))
    {
        return (volatile u32 *)g_display_back_buffer;
    }

    return display64_physical_framebuffer();
}

#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
static u32 display64_compositor_required_back_buffer(u64 *pixels_out, u64 *bytes_out)
{
    u64 pixels;
    u64 bytes;

    if (pixels_out != 0)
    {
        *pixels_out = 0ull;
    }
    if (bytes_out != 0)
    {
        *bytes_out = 0ull;
    }

    if (!display64_has_framebuffer()
        || (g_display_boot_info->framebuffer_pixels_per_scanline == 0u)
        || (g_display_boot_info->framebuffer_height == 0u))
    {
        return 0u;
    }

    pixels = (u64)g_display_boot_info->framebuffer_pixels_per_scanline
        * (u64)g_display_boot_info->framebuffer_height;
    if ((pixels == 0ull)
        || ((pixels / (u64)g_display_boot_info->framebuffer_pixels_per_scanline)
            != (u64)g_display_boot_info->framebuffer_height))
    {
        return 0u;
    }

    bytes = pixels * DISPLAY64_COMPOSITOR_BYTES_PER_PIXEL;
    if ((bytes / DISPLAY64_COMPOSITOR_BYTES_PER_PIXEL) != pixels)
    {
        return 0u;
    }

    if (bytes > g_display_boot_info->framebuffer_bytes)
    {
        return 0u;
    }

    if (pixels_out != 0)
    {
        *pixels_out = pixels;
    }
    if (bytes_out != 0)
    {
        *bytes_out = bytes;
    }

    return 1u;
}

static u32 display64_compositor_allocate_back_buffer(u64 pixels, u64 bytes)
{
    u64 start;
    u64 end;
    u64 limit;

    g_display_back_buffer = 0;
    g_display_back_buffer_pixels = 0ull;
    g_display_back_buffer_bytes = 0ull;
    g_display_back_buffer_next = 0ull;

    if ((pixels == 0ull) || (bytes == 0ull) || (g_display_boot_info == 0))
    {
        return 0u;
    }

    if (g_display_back_buffer_next == 0ull)
    {
        g_display_back_buffer_next = ((u64)__bss_end + (DISPLAY64_PAGE_BYTES - 1ull))
            & ~(DISPLAY64_PAGE_BYTES - 1ull);
        if (g_display_back_buffer_next < DISPLAY64_KERNEL_HIGH_BASE)
        {
            g_display_back_buffer_next += DISPLAY64_KERNEL_HIGH_BASE;
        }
    }

    start = g_display_back_buffer_next;
    end = (start + bytes + (DISPLAY64_PAGE_BYTES - 1ull)) & ~(DISPLAY64_PAGE_BYTES - 1ull);
    if ((end <= start) || (end < bytes))
    {
        return 0u;
    }

    limit = DISPLAY64_KERNEL_HIGH_BASE + (u64)g_display_boot_info->identity_map_bytes;
    if ((limit <= DISPLAY64_KERNEL_HIGH_BASE)
        || (limit > (DISPLAY64_KERNEL_HIGH_BASE + DISPLAY64_KERNEL_FALLBACK_WINDOW_BYTES)))
    {
        limit = DISPLAY64_KERNEL_HIGH_BASE + DISPLAY64_KERNEL_FALLBACK_WINDOW_BYTES;
    }
    if (end > limit)
    {
        return 0u;
    }

    g_display_back_buffer = (u32 *)(u64)start;
    g_display_back_buffer_pixels = pixels;
    g_display_back_buffer_bytes = bytes;
    g_display_back_buffer_next = end;
    return 1u;
}
#endif

static void display64_compositor_mark_dirty(u32 x, u32 y, u32 width, u32 height)
{
    u32 right;
    u32 bottom;
    u32 old_right;
    u32 old_bottom;

    if ((g_display_compositor_active == 0u)
        || !display64_has_framebuffer()
        || (width == 0u)
        || (height == 0u)
        || (x >= g_display_boot_info->framebuffer_width)
        || (y >= g_display_boot_info->framebuffer_height))
    {
        return;
    }

    width = display64_min_u32(width, g_display_boot_info->framebuffer_width - x);
    height = display64_min_u32(height, g_display_boot_info->framebuffer_height - y);
    right = x + width;
    bottom = y + height;

    if (g_display_compositor_dirty == 0u)
    {
        g_display_compositor_dirty = 1u;
        g_display_compositor_dirty_x = x;
        g_display_compositor_dirty_y = y;
        g_display_compositor_dirty_w = width;
        g_display_compositor_dirty_h = height;
        return;
    }

    old_right = g_display_compositor_dirty_x + g_display_compositor_dirty_w;
    old_bottom = g_display_compositor_dirty_y + g_display_compositor_dirty_h;
    if (x < g_display_compositor_dirty_x)
    {
        g_display_compositor_dirty_x = x;
    }
    if (y < g_display_compositor_dirty_y)
    {
        g_display_compositor_dirty_y = y;
    }
    if (right < old_right)
    {
        right = old_right;
    }
    if (bottom < old_bottom)
    {
        bottom = old_bottom;
    }
    g_display_compositor_dirty_w = right - g_display_compositor_dirty_x;
    g_display_compositor_dirty_h = bottom - g_display_compositor_dirty_y;
}

static void display64_compositor_mark_cursor(u32 x, u32 y)
{
    display64_compositor_mark_dirty(
        x,
        y,
        DISPLAY64_COMPOSITOR_CURSOR_WIDTH,
        DISPLAY64_COMPOSITOR_CURSOR_HEIGHT);
}

static void display64_compositor_clamp_cursor(u32 *cursor_x, u32 *cursor_y)
{
    if ((cursor_x == 0) || (cursor_y == 0) || !display64_has_framebuffer())
    {
        return;
    }

    if (*cursor_x >= g_display_boot_info->framebuffer_width)
    {
        *cursor_x = g_display_boot_info->framebuffer_width - 1u;
    }
    if (*cursor_y >= g_display_boot_info->framebuffer_height)
    {
        *cursor_y = g_display_boot_info->framebuffer_height - 1u;
    }
}

static void display64_compositor_cursor_rect(u32 x, u32 y, u32 *width, u32 *height)
{
    if ((width == 0) || (height == 0) || !display64_has_framebuffer()
        || (x >= g_display_boot_info->framebuffer_width)
        || (y >= g_display_boot_info->framebuffer_height))
    {
        if (width != 0)
        {
            *width = 0u;
        }
        if (height != 0)
        {
            *height = 0u;
        }
        return;
    }

    *width = display64_min_u32(DISPLAY64_COMPOSITOR_CURSOR_WIDTH, g_display_boot_info->framebuffer_width - x);
    *height = display64_min_u32(DISPLAY64_COMPOSITOR_CURSOR_HEIGHT, g_display_boot_info->framebuffer_height - y);
}

static void display64_compositor_union_rect(
    u32 x,
    u32 y,
    u32 width,
    u32 height,
    u32 *union_x,
    u32 *union_y,
    u32 *union_w,
    u32 *union_h)
{
    u32 right;
    u32 bottom;
    u32 union_right;
    u32 union_bottom;

    if ((union_x == 0) || (union_y == 0) || (union_w == 0) || (union_h == 0)
        || (width == 0u) || (height == 0u))
    {
        return;
    }

    if (*union_w == 0u || *union_h == 0u)
    {
        *union_x = x;
        *union_y = y;
        *union_w = width;
        *union_h = height;
        return;
    }

    right = x + width;
    bottom = y + height;
    union_right = *union_x + *union_w;
    union_bottom = *union_y + *union_h;
    if (x < *union_x)
    {
        *union_x = x;
    }
    if (y < *union_y)
    {
        *union_y = y;
    }
    if (right > union_right)
    {
        union_right = right;
    }
    if (bottom > union_bottom)
    {
        union_bottom = bottom;
    }
    *union_w = union_right - *union_x;
    *union_h = union_bottom - *union_y;
}

static void display64_compositor_present_back_buffer_rect(u32 x, u32 y, u32 width, u32 height)
{
    volatile u32 *framebuffer;
    u32 draw_width;
    u32 draw_height;
    u32 row;
    u32 column;

    if ((g_display_compositor_active == 0u)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        || (g_display_back_buffer == 0)
#endif
        || !display64_has_framebuffer()
        || (width == 0u)
        || (height == 0u)
        || (x >= g_display_boot_info->framebuffer_width)
        || (y >= g_display_boot_info->framebuffer_height))
    {
        return;
    }

    draw_width = display64_min_u32(width, g_display_boot_info->framebuffer_width - x);
    draw_height = display64_min_u32(height, g_display_boot_info->framebuffer_height - y);
    framebuffer = display64_physical_framebuffer();

    for (row = 0u; row < draw_height; ++row)
    {
        u64 base_pixel = 0ull;
        if (display64_pixel_index(x, y + row, &base_pixel) == 0)
        {
            continue;
        }

        for (column = 0u; column < draw_width; ++column)
        {
            framebuffer[base_pixel + column] = g_display_back_buffer[base_pixel + column];
        }
    }
}

static void display64_compositor_restore_cursor_saved(void)
{
    volatile u32 *framebuffer;
    u32 row;
    u32 column;

    if ((g_display_compositor_cursor_saved_valid == 0u)
#if !defined(LIMITLESS_X64_UEFI_KERNEL) || !LIMITLESS_X64_UEFI_KERNEL
        || (g_display_compositor_active == 0u)
#endif
        || !display64_has_framebuffer())
    {
        return;
    }

    framebuffer = display64_physical_framebuffer();
    for (row = 0u; row < g_display_compositor_cursor_saved_h; ++row)
    {
        u64 base_pixel = 0ull;
        if (display64_pixel_index(
                g_display_compositor_cursor_saved_x,
                g_display_compositor_cursor_saved_y + row,
                &base_pixel) == 0)
        {
            continue;
        }

        for (column = 0u; column < g_display_compositor_cursor_saved_w; ++column)
        {
            framebuffer[base_pixel + column] =
                g_display_compositor_cursor_saved[(row * DISPLAY64_COMPOSITOR_CURSOR_WIDTH) + column];
        }
    }

    g_display_compositor_cursor_saved_valid = 0u;
    g_display_compositor_cursor_drawn_valid = 0u;
}

static void display64_compositor_save_cursor_underlay(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    volatile u32 *framebuffer;
#endif
    u32 width;
    u32 height;
    u32 row;
    u32 column;

    g_display_compositor_cursor_saved_valid = 0u;
    if (
#if !defined(LIMITLESS_X64_UEFI_KERNEL) || !LIMITLESS_X64_UEFI_KERNEL
        (g_display_compositor_active == 0u) ||
#endif
        !display64_has_framebuffer())
    {
        return;
    }

    display64_compositor_cursor_rect(
        g_display_compositor_cursor_x,
        g_display_compositor_cursor_y,
        &width,
        &height);
    if ((width == 0u) || (height == 0u))
    {
        return;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    framebuffer = display64_physical_framebuffer();
#endif
    for (row = 0u; row < height; ++row)
    {
        u64 base_pixel = 0ull;
        if (display64_pixel_index(g_display_compositor_cursor_x, g_display_compositor_cursor_y + row, &base_pixel) == 0)
        {
            continue;
        }

        for (column = 0u; column < width; ++column)
        {
            g_display_compositor_cursor_saved[(row * DISPLAY64_COMPOSITOR_CURSOR_WIDTH) + column] =
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                (g_display_back_buffer != 0)
                    ? g_display_back_buffer[base_pixel + column]
                    : framebuffer[base_pixel + column];
#else
                g_display_back_buffer[base_pixel + column];
#endif
        }
    }

    g_display_compositor_cursor_saved_x = g_display_compositor_cursor_x;
    g_display_compositor_cursor_saved_y = g_display_compositor_cursor_y;
    g_display_compositor_cursor_saved_w = width;
    g_display_compositor_cursor_saved_h = height;
    g_display_compositor_cursor_saved_valid = 1u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 display64_compositor_cursor_shape(u32 row, u32 column)
{
    if ((column <= 2u) && (row <= 28u))
    {
        return 1u;
    }
    if ((row < 23u) && (column <= ((row >> 1u) + 3u)))
    {
        return 1u;
    }
    if ((row >= 20u) && (row <= 30u) && (column >= 8u) && (column <= 11u))
    {
        return 1u;
    }
    if ((row >= 23u) && (row <= 28u) && (column >= 12u) && (column <= 15u))
    {
        return 1u;
    }
    return 0u;
}

static u32 display64_compositor_cursor_outline(u32 row, u32 column)
{
    s32 row_delta;
    s32 column_delta;

    if (display64_compositor_cursor_shape(row, column) != 0u)
    {
        return 0u;
    }

    for (row_delta = -1; row_delta <= 1; ++row_delta)
    {
        for (column_delta = -1; column_delta <= 1; ++column_delta)
        {
            s32 shape_row = (s32)row + row_delta;
            s32 shape_column = (s32)column + column_delta;
            if ((shape_row < 0)
                || (shape_column < 0)
                || (shape_row >= (s32)DISPLAY64_COMPOSITOR_CURSOR_HEIGHT)
                || (shape_column >= (s32)DISPLAY64_COMPOSITOR_CURSOR_WIDTH))
            {
                continue;
            }
            if (display64_compositor_cursor_shape((u32)shape_row, (u32)shape_column) != 0u)
            {
                return 1u;
            }
        }
    }

    return 0u;
}
#endif

static void display64_compositor_draw_cursor(void)
{
    volatile u32 *framebuffer;
    u32 cursor_pixel;
    u32 outline_pixel;
    u32 row;
    u32 column;

    if (
#if !defined(LIMITLESS_X64_UEFI_KERNEL) || !LIMITLESS_X64_UEFI_KERNEL
        (g_display_compositor_active == 0u) ||
#endif
        !display64_has_framebuffer())
    {
        return;
    }

    framebuffer = display64_physical_framebuffer();
    cursor_pixel = display64_make_pixel(
        (g_display_compositor_cursor_buttons != 0u) ? 0x00FFD66Bu : DISPLAY64_COMPOSITOR_CURSOR_RGB);
    outline_pixel = display64_make_pixel(DISPLAY64_COMPOSITOR_CURSOR_SHADOW_RGB);

    for (row = 0u; row < DISPLAY64_COMPOSITOR_CURSOR_HEIGHT; ++row)
    {
        for (column = 0u; column < DISPLAY64_COMPOSITOR_CURSOR_WIDTH; ++column)
        {
            u64 pixel_index = 0ull;
            u32 draw_main;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            u32 draw_outline;
#else
            u32 draw_outline;
#endif
            u32 x = g_display_compositor_cursor_x + column;
            u32 y = g_display_compositor_cursor_y + row;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            draw_main = display64_compositor_cursor_shape(row, column);
            draw_outline = display64_compositor_cursor_outline(row, column);
#else
            draw_main = (column == 0u)
                || ((row < 13u) && (column <= (row >> 1u)))
                || ((row >= 12u) && (row <= 18u) && (column >= 4u) && (column <= 6u));
            draw_outline = (draw_main == 0u)
                && (column > 0u)
                && (row > 0u)
                && (((column - 1u) == 0u)
                    || ((row < 14u) && ((column - 1u) <= ((row - 1u) >> 1u))));
#endif

            if ((draw_main == 0u) && (draw_outline == 0u))
            {
                continue;
            }

            if (display64_pixel_index(x, y, &pixel_index) == 0)
            {
                continue;
            }

            framebuffer[pixel_index] = (draw_main != 0u) ? cursor_pixel : outline_pixel;
        }
    }

    ++g_display_compositor_cursor_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_back_buffer == 0)
    {
        ++g_display_direct_cursor_count;
    }
#endif
    g_display_compositor_cursor_drawn_valid = 1u;
    g_display_compositor_cursor_drawn_x = g_display_compositor_cursor_x;
    g_display_compositor_cursor_drawn_y = g_display_compositor_cursor_y;
    g_display_compositor_cursor_drawn_buttons = g_display_compositor_cursor_buttons;
}

#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
static void display64_compositor_init_back_buffer(void)
{
    volatile u32 *framebuffer;
    u64 pixels;
    u64 bytes;
    u64 index;

    g_display_compositor_active = 0u;
    g_display_back_buffer = 0;
    g_display_back_buffer_pixels = 0ull;
    g_display_back_buffer_bytes = 0ull;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_compositor_direct_mode = 0u;
#endif
    if (display64_compositor_required_back_buffer(&pixels, &bytes) == 0u)
    {
        return;
    }
    if (display64_compositor_allocate_back_buffer(pixels, bytes) == 0u)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_display_compositor_active = 1u;
        g_display_compositor_direct_mode = 1u;
        g_display_compositor_cursor_x = g_display_boot_info->framebuffer_width >> 1u;
        g_display_compositor_cursor_y = g_display_boot_info->framebuffer_height >> 1u;
#endif
        return;
    }

    framebuffer = display64_physical_framebuffer();
    for (index = 0ull; index < pixels; ++index)
    {
        g_display_back_buffer[index] = framebuffer[index];
    }

    g_display_compositor_active = 1u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_compositor_direct_mode = 0u;
#endif
    g_display_compositor_cursor_x = g_display_boot_info->framebuffer_width >> 1u;
    g_display_compositor_cursor_y = g_display_boot_info->framebuffer_height >> 1u;
}
#endif

static u32 display64_mix_token(u32 token, u32 value)
{
    token ^= value;
    token *= 16777619u;
    return token;
}

static u32 display64_deny(void)
{
    ++g_display_denial_count;
    return DISPLAY64_INVALID_RESULT;
}

static u8 display64_glyph_row(u8 character, u32 row)
{
    if (row >= DISPLAY64_FONT_HEIGHT)
    {
        return 0u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if ((character >= (u8)'a') && (character <= (u8)'z'))
    {
        return g_display_lower_font[character - (u8)'a'][row];
    }
#else
    if ((character >= (u8)'a') && (character <= (u8)'z'))
    {
        character = (u8)(character - ((u8)'a' - (u8)'A'));
    }
#endif

    if ((character >= (u8)'A') && (character <= (u8)'Z'))
    {
        return g_display_alpha_font[character - (u8)'A'][row];
    }

    if ((character >= (u8)'0') && (character <= (u8)'9'))
    {
        return g_display_digit_font[character - (u8)'0'][row];
    }

    if (character == (u8)'-')
    {
        return (row == 3u) ? 0x1Eu : 0u;
    }

    if (character == (u8)'.')
    {
        return (row == 6u) ? 0x04u : 0u;
    }

    if (character == (u8)',')
    {
        return (row == 5u) ? 0x04u : ((row == 6u) ? 0x08u : 0u);
    }

    if (character == (u8)':')
    {
        return ((row == 2u) || (row == 5u)) ? 0x04u : 0u;
    }

    if (character == (u8)'/')
    {
        static const u8 slash[DISPLAY64_FONT_HEIGHT] = {
            0x01u, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x10u
        };
        return slash[row];
    }

    if (character == (u8)'\\')
    {
        static const u8 backslash[DISPLAY64_FONT_HEIGHT] = {
            0x10u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u, 0x01u
        };
        return backslash[row];
    }

    if (character == (u8)'[')
    {
        return ((row == 0u) || (row == 6u)) ? 0x0Eu : 0x08u;
    }

    if (character == (u8)']')
    {
        return ((row == 0u) || (row == 6u)) ? 0x0Eu : 0x02u;
    }

    if (character == (u8)'$')
    {
        static const u8 dollar[DISPLAY64_FONT_HEIGHT] = {
            0x04u, 0x0Fu, 0x14u, 0x0Eu, 0x05u, 0x1Eu, 0x04u
        };
        return dollar[row];
    }

    if (character == (u8)'_')
    {
        return (row == 6u) ? 0x1Fu : 0u;
    }

    if (character == (u8)'=')
    {
        return ((row == 2u) || (row == 4u)) ? 0x1Fu : 0u;
    }

    if (character == (u8)'<')
    {
        static const u8 left_angle[DISPLAY64_FONT_HEIGHT] = {
            0x02u, 0x04u, 0x08u, 0x10u, 0x08u, 0x04u, 0x02u
        };
        return left_angle[row];
    }

    if (character == (u8)'>')
    {
        static const u8 right_angle[DISPLAY64_FONT_HEIGHT] = {
            0x08u, 0x04u, 0x02u, 0x01u, 0x02u, 0x04u, 0x08u
        };
        return right_angle[row];
    }

    if (character == (u8)'*')
    {
        return (row == 1u) ? 0x15u : ((row == 2u) ? 0x0Eu : ((row == 3u) ? 0x15u : 0u));
    }

    return 0u;
}

static int display64_pixel_index(u32 x, u32 y, u64 *pixel_index)
{
    u64 base_pixel;
    u64 byte_offset;

    if (!display64_has_framebuffer()
        || (pixel_index == 0)
        || (x >= g_display_boot_info->framebuffer_width)
        || (y >= g_display_boot_info->framebuffer_height))
    {
        return 0;
    }

    base_pixel = ((u64)y * (u64)g_display_boot_info->framebuffer_pixels_per_scanline) + (u64)x;
    byte_offset = base_pixel * 4ull;
    if ((byte_offset + 4ull) > g_display_boot_info->framebuffer_bytes)
    {
        return 0;
    }

    *pixel_index = base_pixel;
    return 1;
}

static u32 display64_draw_glyph(u8 character, u32 x, u32 y, u32 pixel, u32 *token)
{
    volatile u32 *framebuffer = display64_draw_buffer();
    u32 row;
    u32 column;
    u32 scale_x;
    u32 scale_y;
    u32 drawn = 0u;

    for (row = 0u; row < DISPLAY64_FONT_HEIGHT; ++row)
    {
        u8 bits = display64_glyph_row(character, row);
        for (column = 0u; column < DISPLAY64_FONT_WIDTH; ++column)
        {
            if ((bits & (u8)(1u << (DISPLAY64_FONT_WIDTH - 1u - column))) == 0u)
            {
                continue;
            }

            for (scale_y = 0u; scale_y < display64_font_scale_value(); ++scale_y)
            {
                for (scale_x = 0u; scale_x < display64_font_scale_value(); ++scale_x)
                {
                    u64 index = 0ull;
                    u32 px = x + (column * display64_font_scale_value()) + scale_x;
                    u32 py = y + (row * display64_font_scale_value()) + scale_y;

                    if (display64_pixel_index(px, py, &index) == 0)
                    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                        ++g_display_console_clip_count;
#endif
                        continue;
                    }

                    framebuffer[index] = pixel;
                    display64_compositor_mark_dirty(px, py, 1u, 1u);
                    if (token != 0)
                    {
                        *token = display64_mix_token(*token, pixel ^ (u32)index ^ (u32)character);
                    }
                    ++drawn;
                }
            }
        }
    }

    return drawn;
}

u32 display64_write_early_kernel_line(const struct boot_info *boot_info, const char *text)
{
    const struct boot_info *previous_boot_info = g_display_boot_info;
    struct boot_info previous_boot_info_storage = g_display_boot_info_storage;
    u32 previous_compositor_active = g_display_compositor_active;
    volatile u32 *framebuffer;
    u32 text_pixel;
    u32 panel_pixel;
    u32 panel_x = 8u;
    u32 panel_y = 8u;
    u32 panel_w;
    u32 panel_h = 28u;
    u32 row;
    u32 column;
    u32 cursor_x;
    u32 drawn = 0u;
    u32 token = 2166136261u;

    display64_set_boot_info(boot_info);
    g_display_compositor_active = 0u;
    if (!display64_has_framebuffer())
    {
        g_display_boot_info_storage = previous_boot_info_storage;
        g_display_boot_info = previous_boot_info;
        g_display_compositor_active = previous_compositor_active;
        return 0u;
    }

    if (text == 0)
    {
        text = "";
    }

    panel_w = display64_min_u32(680u, g_display_boot_info->framebuffer_width);
    if (panel_w > 16u)
    {
        panel_w -= 8u;
    }
    if ((panel_y + panel_h) > g_display_boot_info->framebuffer_height)
    {
        panel_y = 0u;
        panel_h = display64_min_u32(panel_h, g_display_boot_info->framebuffer_height);
    }

    framebuffer = display64_physical_framebuffer();
    panel_pixel = display64_make_pixel(DISPLAY64_RGB_SURFACE);
    text_pixel = display64_make_pixel(DISPLAY64_RGB_ACCENT);
    for (row = 0u; row < panel_h; ++row)
    {
        for (column = 0u; column < panel_w; ++column)
        {
            u64 index = 0ull;
            if (display64_pixel_index(panel_x + column, panel_y + row, &index) != 0)
            {
                framebuffer[index] = panel_pixel;
            }
        }
    }

    cursor_x = panel_x + 8u;
    while ((*text != '\0')
        && ((cursor_x + (DISPLAY64_FONT_WIDTH * display64_font_scale_value())) < (panel_x + panel_w)))
    {
        drawn += display64_draw_glyph((u8)*text, cursor_x, panel_y + 7u, text_pixel, &token);
        cursor_x += display64_font_advance();
        ++text;
    }

    g_display_boot_info_storage = previous_boot_info_storage;
    g_display_boot_info = previous_boot_info;
    g_display_compositor_active = previous_compositor_active;
    return (drawn != 0u && token != 0u) ? drawn : 0u;
}

static u32 display64_console_viewport_width(void)
{
    if (!display64_has_framebuffer()
        || (g_display_console_x >= g_display_boot_info->framebuffer_width))
    {
        return 0u;
    }

    return display64_min_u32(
        g_display_console_w,
        g_display_boot_info->framebuffer_width - g_display_console_x);
}

static u32 display64_console_viewport_bottom(void)
{
    u32 bottom;

    if (!display64_has_framebuffer())
    {
        return g_display_console_y;
    }

    bottom = g_display_console_y + g_display_console_h;
    if (bottom > g_display_boot_info->framebuffer_height)
    {
        bottom = g_display_boot_info->framebuffer_height;
    }

    return bottom;
}

static u32 display64_console_viewport_height(void)
{
    u32 bottom = display64_console_viewport_bottom();

    if (bottom <= g_display_console_y)
    {
        return 0u;
    }

    return bottom - g_display_console_y;
}

static u32 display64_clear_rect(u32 x, u32 y, u32 width, u32 height, u32 rgb, u32 *token);

static u32 display64_scroll_console_viewport(u32 *token)
{
    volatile u32 *framebuffer;
    u32 viewport_width = display64_console_viewport_width();
    u32 viewport_height = display64_console_viewport_height();
    u32 scroll_height;
    u32 row;
    u32 column;
    u32 drawn = 0u;
    u32 cleared;

    if ((viewport_width == 0u) || (viewport_height <= display64_line_advance()))
    {
        return 0u;
    }

    framebuffer = display64_draw_buffer();
    scroll_height = viewport_height - display64_line_advance();

    for (row = 0u; row < scroll_height; ++row)
    {
        u64 source_index = 0ull;
        u64 target_index = 0ull;
        u32 source_y = g_display_console_y + display64_line_advance() + row;
        u32 target_y = g_display_console_y + row;

        if ((display64_pixel_index(g_display_console_x, source_y, &source_index) == 0)
            || (display64_pixel_index(g_display_console_x, target_y, &target_index) == 0))
        {
            continue;
        }

        for (column = 0u; column < viewport_width; ++column)
        {
            u32 pixel = framebuffer[source_index + column];
            framebuffer[target_index + column] = pixel;
            display64_compositor_mark_dirty(g_display_console_x + column, target_y, 1u, 1u);
            if (token != 0)
            {
                *token = display64_mix_token(*token, pixel ^ (u32)(target_index + column));
            }
            ++drawn;
        }
    }

    cleared = display64_clear_rect(
        g_display_console_x,
        g_display_console_y + scroll_height,
        viewport_width,
        display64_line_advance(),
        DISPLAY64_PANEL_RGB,
        token);
    drawn += cleared;

    if (drawn != 0u)
    {
        ++g_display_console_scroll_count;
        if (cleared != 0u)
        {
            ++g_display_console_line_clear_count;
            g_display_console_line_dirty = 1u;
        }
    }

    return drawn;
}

static u32 display64_text_newline(u32 track_console_wrap, u32 *token)
{
    u32 drawn = 0u;
    u32 scrolled = 0u;

    g_display_text_x = g_display_console_x;
    g_display_text_y += display64_line_advance();

    if (display64_has_framebuffer()
        && ((g_display_text_y + (DISPLAY64_FONT_HEIGHT * display64_font_scale_value()))
            >= display64_console_viewport_bottom()))
    {
        if (track_console_wrap != 0u)
        {
            drawn += display64_scroll_console_viewport(token);
            if (display64_console_viewport_height() > display64_line_advance())
            {
                g_display_text_y = display64_console_viewport_bottom() - display64_line_advance();
            }
            else
            {
                g_display_text_y = g_display_console_y;
            }
            scrolled = 1u;
        }
        else
        {
            g_display_text_y = g_display_console_y;
        }
    }

    if (track_console_wrap != 0u)
    {
        g_display_console_line_dirty = (scrolled != 0u) ? 1u : 0u;
    }

    return drawn;
}

static u32 display64_clear_rect(u32 x, u32 y, u32 width, u32 height, u32 rgb, u32 *token)
{
    volatile u32 *framebuffer;
    u32 draw_width;
    u32 draw_height;
    u32 row;
    u32 column;
    u32 pixel;
    u32 drawn = 0u;

    if (!display64_has_framebuffer()
        || (x >= g_display_boot_info->framebuffer_width)
        || (y >= g_display_boot_info->framebuffer_height))
    {
        return 0u;
    }

    draw_width = display64_min_u32(width, g_display_boot_info->framebuffer_width - x);
    draw_height = display64_min_u32(height, g_display_boot_info->framebuffer_height - y);
    if ((draw_width == 0u) || (draw_height == 0u))
    {
        return 0u;
    }

    pixel = display64_make_pixel(rgb);
    framebuffer = display64_draw_buffer();
    display64_compositor_mark_dirty(x, y, draw_width, draw_height);
    for (row = 0u; row < draw_height; ++row)
    {
        u64 base_pixel = 0ull;
        u64 byte_offset;

        if (display64_pixel_index(x, y + row, &base_pixel) == 0)
        {
            continue;
        }

        byte_offset = base_pixel * 4ull;
        if ((byte_offset + ((u64)draw_width * 4ull)) > g_display_boot_info->framebuffer_bytes)
        {
            break;
        }

        for (column = 0u; column < draw_width; ++column)
        {
            framebuffer[base_pixel + column] = pixel;
            if (token != 0)
            {
                *token = display64_mix_token(*token, pixel ^ (u32)(base_pixel + column));
            }
            ++drawn;
        }
    }

    return drawn;
}

static u32 display64_clear_console_line(u32 *token)
{
    u32 drawn;
    u32 viewport_width = display64_console_viewport_width();

    if ((viewport_width == 0u) || (g_display_console_line_dirty != 0u))
    {
        return 0u;
    }

    drawn = display64_clear_rect(
        g_display_console_x,
        g_display_text_y,
        viewport_width,
        display64_line_advance(),
        DISPLAY64_PANEL_RGB,
        token);

    if (drawn != 0u)
    {
        ++g_display_console_line_clear_count;
        g_display_console_line_dirty = 1u;
    }

    return drawn;
}

static u32 display64_text_limit_x(u32 clear_console_lines)
{
    u32 viewport_width;

    if ((clear_console_lines != 0u)
        && ((viewport_width = display64_console_viewport_width()) != 0u))
    {
        return g_display_console_x + viewport_width;
    }

    return display64_has_framebuffer() ? g_display_boot_info->framebuffer_width : g_display_console_x;
}

static u32 display64_text_backspace(u32 clear_console_lines, u32 *token)
{
    u32 drawn = 0u;
    u32 cell_width = display64_font_advance();
    u32 cell_height = display64_line_advance();

    if (!display64_has_framebuffer())
    {
        return 0u;
    }

    if (g_display_text_x > g_display_console_x)
    {
        g_display_text_x -= display64_font_advance();
    }
    else if ((clear_console_lines != 0u) && (g_display_text_y > g_display_console_y))
    {
        u32 viewport_width = display64_console_viewport_width();
        g_display_text_y -= display64_line_advance();
        if (viewport_width > display64_font_advance())
        {
            g_display_text_x = g_display_console_x
                + ((viewport_width - display64_font_advance()) / display64_font_advance())
                    * display64_font_advance();
        }
    }

    if (g_display_text_x >= display64_text_limit_x(clear_console_lines))
    {
        cell_width = 0u;
    }
    else if ((g_display_text_x + cell_width) > display64_text_limit_x(clear_console_lines))
    {
        cell_width = display64_text_limit_x(clear_console_lines) - g_display_text_x;
    }
    if (g_display_text_y >= display64_console_viewport_bottom())
    {
        cell_height = 0u;
    }
    else if ((g_display_text_y + cell_height) > display64_console_viewport_bottom())
    {
        cell_height = display64_console_viewport_bottom() - g_display_text_y;
    }
    if ((cell_width != 0u) && (cell_height != 0u))
    {
        drawn = display64_clear_rect(
            g_display_text_x,
            g_display_text_y,
            cell_width,
            cell_height,
            DISPLAY64_PANEL_RGB,
            token);
        if (drawn != 0u)
        {
            g_display_console_line_dirty = 1u;
        }
    }

    return drawn;
}

static u32 display64_render_text_bytes(
    const u8 *bytes,
    u32 byte_count,
    u32 *token,
    u32 clear_console_lines)
{
    u32 index;
    u32 pixel;
    u32 drawn = 0u;

    if ((bytes == 0) || !display64_has_framebuffer())
    {
        return 0u;
    }

    pixel = display64_make_pixel(DISPLAY64_TEXT_RGB);
    for (index = 0u; index < byte_count; ++index)
    {
        u8 character = bytes[index];

        if (character == (u8)'\n')
        {
            drawn += display64_text_newline(clear_console_lines, token);
            continue;
        }

        if (character == (u8)'\r')
        {
            g_display_text_x = g_display_console_x;
            if (clear_console_lines != 0u)
            {
                g_display_console_line_dirty = 0u;
            }
            continue;
        }

        if ((character == (u8)'\b') || (character == 0x7Fu))
        {
            drawn += display64_text_backspace(clear_console_lines, token);
            continue;
        }

        if ((g_display_text_x + display64_font_advance()) >= display64_text_limit_x(clear_console_lines))
        {
            if (clear_console_lines != 0u)
            {
                ++g_display_console_wrap_count;
            }
            drawn += display64_text_newline(clear_console_lines, token);
        }

        drawn += (clear_console_lines != 0u) ? display64_clear_console_line(token) : 0u;
        drawn += display64_draw_glyph(character, g_display_text_x, g_display_text_y, pixel, token);
        g_display_text_x += display64_font_advance();
    }

    return drawn;
}

static void display64_console_replay_append(const u8 *bytes, u32 byte_count)
{
    u32 index;

    if (bytes == 0)
    {
        return;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        if (g_display_console_replay_count < DISPLAY64_CONSOLE_REPLAY_BYTES)
        {
            u32 write_index =
                (g_display_console_replay_head + g_display_console_replay_count)
                    % DISPLAY64_CONSOLE_REPLAY_BYTES;
            g_display_console_replay[write_index] = bytes[index];
            ++g_display_console_replay_count;
        }
        else
        {
            g_display_console_replay[g_display_console_replay_head] = bytes[index];
            g_display_console_replay_head =
                (g_display_console_replay_head + 1u) % DISPLAY64_CONSOLE_REPLAY_BYTES;
            ++g_display_console_replay_overflow;
        }
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if ((byte_count != 0u) && (g_display_terminal_scroll_offset != 0u))
    {
        g_display_terminal_scroll_offset = 0u;
    }
#endif
}

static u32 display64_console_replay_render(u32 *token)
{
    u32 drawn = 0u;
    u32 render_count;

    if ((g_display_console_replay_count == 0u) || !display64_has_framebuffer())
    {
        return 0u;
    }

    render_count = g_display_console_replay_count;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_terminal_scroll_offset >= render_count)
    {
        render_count = 1u;
    }
    else
    {
        render_count -= g_display_terminal_scroll_offset;
    }
#endif
    g_display_text_x = g_display_console_x;
    g_display_text_y = g_display_console_y;
    g_display_console_line_dirty = 0u;

    if ((g_display_console_replay_head + render_count)
        <= DISPLAY64_CONSOLE_REPLAY_BYTES)
    {
        drawn += display64_render_text_bytes(
            &g_display_console_replay[g_display_console_replay_head],
            render_count,
            token,
            1u);
    }
    else
    {
        u32 first_count = DISPLAY64_CONSOLE_REPLAY_BYTES - g_display_console_replay_head;
        u32 second_count = render_count - first_count;
        drawn += display64_render_text_bytes(
            &g_display_console_replay[g_display_console_replay_head],
            first_count,
            token,
            1u);
        drawn += display64_render_text_bytes(
            g_display_console_replay,
            second_count,
            token,
            1u);
    }

    return drawn;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static int display64_point_in_rect(u32 x, u32 y, u32 rect_x, u32 rect_y, u32 rect_w, u32 rect_h);
static void display64_compositor_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 rgb);
static void display64_compositor_fill_round_rect_4(u32 x, u32 y, u32 width, u32 height, u32 rgb);
static u32 display64_draw_font_text(
    u32 x,
    u32 y,
    const char *text,
    u32 size,
    u32 rgb,
    u32 background_rgb);

static u8 display64_console_replay_byte_at(u32 offset)
{
    u32 index;

    if (offset >= g_display_console_replay_count)
    {
        return 0u;
    }

    index = (g_display_console_replay_head + offset) % DISPLAY64_CONSOLE_REPLAY_BYTES;
    return g_display_console_replay[index];
}

static u32 display64_terminal_selection_span_bytes(void)
{
    u32 min_x = display64_min_u32(g_display_terminal_selection_anchor_x, g_display_terminal_selection_x);
    u32 max_x = (g_display_terminal_selection_anchor_x > g_display_terminal_selection_x)
        ? g_display_terminal_selection_anchor_x
        : g_display_terminal_selection_x;
    u32 min_y = display64_min_u32(g_display_terminal_selection_anchor_y, g_display_terminal_selection_y);
    u32 max_y = (g_display_terminal_selection_anchor_y > g_display_terminal_selection_y)
        ? g_display_terminal_selection_anchor_y
        : g_display_terminal_selection_y;
    u32 columns = ((max_x - min_x) / display64_font_advance()) + 1u;
    u32 rows = ((max_y - min_y) / display64_line_advance()) + 1u;
    u32 bytes = columns * rows;

    if (bytes == 0u)
    {
        bytes = 1u;
    }
    if (bytes > DISPLAY64_TERMINAL_SELECTION_BYTES)
    {
        bytes = DISPLAY64_TERMINAL_SELECTION_BYTES;
    }

    return bytes;
}

static void display64_terminal_copy_selection(void)
{
    u32 bytes = display64_terminal_selection_span_bytes();
    u32 start = (g_display_console_replay_count > bytes)
        ? (g_display_console_replay_count - bytes)
        : 0u;
    u32 index;

    for (index = 0u; index < bytes; ++index)
    {
        g_display_terminal_selection_buffer[index] =
            display64_console_replay_byte_at(start + index);
    }
    g_display_terminal_selection_bytes = bytes;
    g_display_terminal_copied_bytes = bytes;
    ++g_display_terminal_copy_count;
}

static u32 display64_terminal_point_in_content(const struct display64_window *window, u32 x, u32 y)
{
    u32 content_x;
    u32 content_y;
    u32 content_w;
    u32 content_h;

    if (window == 0)
    {
        return 0u;
    }

    content_x = window->x + 8u;
    content_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 8u;
    content_w = (window->width > 16u) ? (window->width - 16u) : 0u;
    content_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + 16u))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - 16u)
        : 0u;
    return display64_point_in_rect(x, y, content_x, content_y, content_w, content_h);
}

static void display64_terminal_draw_overlay(const struct display64_window *window)
{
    u32 badge_x;
    u32 badge_y;
    u32 selection_x;
    u32 selection_y;
    u32 selection_w;
    u32 selection_h;

    if ((window == 0) || !display64_has_framebuffer())
    {
        return;
    }

    badge_x = window->x + 12u;
    badge_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 10u;
    display64_compositor_fill_round_rect_4(badge_x, badge_y, 108u, 18u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(
        badge_x + 8u,
        badge_y + 5u,
        "Scrollback",
        DISPLAY64_FONT_SMALL,
        DISPLAY64_RGB_TEXT_SECONDARY,
        DISPLAY64_FONT_TRANSPARENT);

    if ((g_display_terminal_selection_active != 0u) || (g_display_terminal_copied_bytes != 0u))
    {
        selection_x = display64_min_u32(g_display_terminal_selection_anchor_x, g_display_terminal_selection_x);
        selection_y = display64_min_u32(g_display_terminal_selection_anchor_y, g_display_terminal_selection_y);
        selection_w = ((g_display_terminal_selection_anchor_x > g_display_terminal_selection_x)
                ? (g_display_terminal_selection_anchor_x - g_display_terminal_selection_x)
                : (g_display_terminal_selection_x - g_display_terminal_selection_anchor_x))
            + display64_font_advance();
        selection_h = ((g_display_terminal_selection_anchor_y > g_display_terminal_selection_y)
                ? (g_display_terminal_selection_anchor_y - g_display_terminal_selection_y)
                : (g_display_terminal_selection_y - g_display_terminal_selection_anchor_y))
            + display64_line_advance();
        display64_compositor_fill_rect(selection_x, selection_y, selection_w, 1u, DISPLAY64_RGB_HIGHLIGHT);
        display64_compositor_fill_rect(selection_x, selection_y + selection_h - 1u, selection_w, 1u, DISPLAY64_RGB_HIGHLIGHT);
        display64_compositor_fill_rect(selection_x, selection_y, 1u, selection_h, DISPLAY64_RGB_HIGHLIGHT);
        display64_compositor_fill_rect(selection_x + selection_w - 1u, selection_y, 1u, selection_h, DISPLAY64_RGB_HIGHLIGHT);
        (void)display64_draw_font_text(
            badge_x + 116u,
            badge_y + 5u,
            "Copied",
            DISPLAY64_FONT_SMALL,
            DISPLAY64_RGB_APP_TERMINAL,
            DISPLAY64_FONT_TRANSPARENT);
    }

    display64_compositor_fill_rect(
        g_display_text_x,
        g_display_text_y,
        2u,
        display64_line_advance(),
        DISPLAY64_RGB_APP_TERMINAL);
    ++g_display_terminal_cursor_draw_count;
}
#endif

static u32 display64_diag_append_char(char *buffer, u32 cursor, u32 capacity, char value)
{
    if (cursor + 1u >= capacity)
    {
        return cursor;
    }

    buffer[cursor] = value;
    return cursor + 1u;
}

static u32 display64_diag_append_text(char *buffer, u32 cursor, u32 capacity, const char *text)
{
    while (*text != '\0')
    {
        cursor = display64_diag_append_char(buffer, cursor, capacity, *text);
        ++text;
    }

    return cursor;
}

static u32 display64_diag_append_bool(char *buffer, u32 cursor, u32 capacity, u32 value)
{
    return display64_diag_append_char(buffer, cursor, capacity, value != 0u ? 'Y' : 'N');
}

static u32 display64_diag_append_u32(char *buffer, u32 cursor, u32 capacity, u32 value)
{
    char digits[10];
    u32 count = 0u;

    if (value == 0u)
    {
        return display64_diag_append_char(buffer, cursor, capacity, '0');
    }

    while ((value > 0u) && (count < 10u))
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count > 0u)
    {
        --count;
        cursor = display64_diag_append_char(buffer, cursor, capacity, digits[count]);
    }

    return cursor;
}

static u32 display64_diag_append_hex_nibble(char *buffer, u32 cursor, u32 capacity, u8 value)
{
    char digit = (value < 10u) ? (char)('0' + value) : (char)('A' + (value - 10u));
    return display64_diag_append_char(buffer, cursor, capacity, digit);
}

static u32 display64_diag_append_hex_u32(char *buffer, u32 cursor, u32 capacity, u32 value)
{
    s32 shift;

    cursor = display64_diag_append_text(buffer, cursor, capacity, "0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        cursor = display64_diag_append_hex_nibble(
            buffer,
            cursor,
            capacity,
            (u8)((value >> shift) & 0x0Fu));
    }

    return cursor;
}

static u32 display64_string_length(const char *text);

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 display64_settings_line_value(const u8 *buffer, u32 byte_count, const char *key, u32 fallback)
{
    u32 key_length = display64_string_length(key);
    u32 index = 0u;

    while ((buffer != 0) && ((index + key_length) < byte_count))
    {
        u32 key_index;
        u32 value = 0u;
        u32 matched = 1u;

        for (key_index = 0u; key_index < key_length; ++key_index)
        {
            if (buffer[index + key_index] != (u8)key[key_index])
            {
                matched = 0u;
                break;
            }
        }

        if ((matched != 0u) && (buffer[index + key_length] == (u8)' '))
        {
            u32 cursor = index + key_length + 1u;
            u32 digits = 0u;
            while ((cursor < byte_count) && (buffer[cursor] >= (u8)'0') && (buffer[cursor] <= (u8)'9'))
            {
                value = (value * 10u) + (u32)(buffer[cursor] - (u8)'0');
                ++cursor;
                ++digits;
            }
            return (digits != 0u) ? value : fallback;
        }

        while ((index < byte_count) && (buffer[index] != (u8)'\n'))
        {
            ++index;
        }
        if (index < byte_count)
        {
            ++index;
        }
    }

    return fallback;
}

static u32 display64_settings_clamp_scale(u32 value)
{
    if (value < 1u)
    {
        return 1u;
    }
    if (value > DISPLAY64_FONT_MAX_SCALE)
    {
        return DISPLAY64_FONT_MAX_SCALE;
    }
    return value;
}

static u32 display64_settings_clamp_pointer(u32 value)
{
    if (value < DISPLAY64_SETTINGS_POINTER_SLOW)
    {
        return DISPLAY64_SETTINGS_POINTER_SLOW;
    }
    if (value > DISPLAY64_SETTINGS_POINTER_FAST)
    {
        return DISPLAY64_SETTINGS_POINTER_FAST;
    }
    return value;
}

static u32 display64_settings_append_u32_line(char *buffer, u32 cursor, u32 capacity, const char *label, u32 value)
{
    cursor = display64_diag_append_text(buffer, cursor, capacity, label);
    cursor = display64_diag_append_u32(buffer, cursor, capacity, value);
    return display64_diag_append_char(buffer, cursor, capacity, '\n');
}

static u32 display64_settings_build_config(void)
{
    u32 cursor = 0u;
    char *buffer = (char *)g_display_settings_config;

    cursor = display64_diag_append_text(buffer, cursor, sizeof(g_display_settings_config), "LIMITLESS-SETTINGS 1\n");
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_config), "THEME ", g_display_settings_theme);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_config), "SCALE ", g_display_text_scale);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_config), "POINTER ", g_display_settings_pointer_speed);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_config), "KEYREPEAT ", g_display_settings_key_repeat);
    return cursor;
}

static void display64_settings_load_once(void)
{
    u32 bytes_read = 0u;

    if (g_display_settings_loaded != 0u)
    {
        return;
    }
    g_display_settings_loaded = 1u;
    if (mmio64_nvme_fat_shell_read_file(
            (const u8 *)"/SETTINGS.CFG",
            13u,
            g_display_settings_config,
            sizeof(g_display_settings_config),
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            &bytes_read) == 0u)
    {
        return;
    }

    g_display_settings_theme = (display64_settings_line_value(
        g_display_settings_config,
        bytes_read,
        "THEME",
        DISPLAY64_SETTINGS_THEME_DARK) != 0u) ? DISPLAY64_SETTINGS_THEME_LIGHT : DISPLAY64_SETTINGS_THEME_DARK;
    g_display_text_scale = display64_settings_clamp_scale(
        display64_settings_line_value(
            g_display_settings_config,
            bytes_read,
            "SCALE",
            DISPLAY64_FONT_DEFAULT_SCALE));
    g_display_settings_pointer_speed = display64_settings_clamp_pointer(
        display64_settings_line_value(
            g_display_settings_config,
            bytes_read,
            "POINTER",
            DISPLAY64_SETTINGS_POINTER_NORMAL));
    g_display_settings_key_repeat = (display64_settings_line_value(
        g_display_settings_config,
        bytes_read,
        "KEYREPEAT",
        1u) != 0u) ? 1u : 0u;
    ++g_display_settings_load_count;
}

static u32 display64_settings_save(void)
{
    u32 byte_count = display64_settings_build_config();

    if ((byte_count == 0u)
        || (mmio64_nvme_fat_shell_write_file(
            (const u8 *)"/SETTINGS.CFG",
            13u,
            g_display_settings_config,
            byte_count,
            PRINCIPAL64_ID_CONSOLE_CLIENT) == 0u))
    {
        ++g_display_settings_save_denial_count;
        return 0u;
    }

    ++g_display_settings_save_count;
    return 1u;
}

static u32 display64_settings_export_diagnostics(void)
{
    char *buffer = (char *)g_display_settings_diag;
    u32 cursor = 0u;
    u32 wrote_usb = 0u;
    u32 wrote_nvme = 0u;

    cursor = display64_diag_append_text(buffer, cursor, sizeof(g_display_settings_diag), "LimitlessOS Product diagnostics\n");
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "theme ", g_display_settings_theme);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "scale ", g_display_text_scale);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "pointer ", g_display_settings_pointer_speed);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "keyrepeat ", g_display_settings_key_repeat);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fb-width ", display64_width());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fb-height ", display64_height());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "nvme-fat ", mmio64_nvme_fat_located());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "nvme-rw-error ", mmio64_nvme_rw_error());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "usb-fat ", mmio64_usb_fat_located());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "usb-fat-error ", mmio64_usb_fat_error());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "usb-fat-scheme ", mmio64_usb_fat_partition_scheme());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "usb-storage-present ", xhci64_usb_storage_present());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "usb-storage-ready ", xhci64_usb_storage_ready());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "usb-storage-error ", xhci64_usb_storage_error());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-found ", xhci64_found());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-error ", xhci64_error());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-connected-ports ", xhci64_connected_ports());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-hid-inventory ", xhci64_hid_interface_inventory());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-mouse-endpoint ", xhci64_mouse_endpoint_present());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-mouse-reports ", xhci64_mouse_reports());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-hid-port ", xhci64_first_hid_port());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-hid-interface-class ", xhci64_first_hid_interface_class());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-hid-interface-subclass ", xhci64_first_hid_interface_subclass());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-hid-interface-protocol ", xhci64_first_hid_interface_protocol());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-mouse-candidate-port ", xhci64_first_mouse_candidate_port());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-mouse-candidate-class ", xhci64_first_mouse_candidate_interface_class());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-mouse-candidate-subclass ", xhci64_first_mouse_candidate_interface_subclass());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-mouse-candidate-protocol ", xhci64_first_mouse_candidate_interface_protocol());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-first-mouse-candidate-mps ", xhci64_first_mouse_candidate_endpoint_mps());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-last-address-completion ", xhci64_last_address_completion());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-last-address-port ", xhci64_last_address_port());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-pre-address-portsc-pls ", xhci64_pre_address_portsc_pls());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "xhci-post-address-portsc-pls ", xhci64_post_address_portsc_pls());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "i2c-controller ", i2c_hid64_controller_present());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "i2c-pointer-found ", i2c_hid64_pointer_found());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "i2c-pointer-reports ", i2c_hid64_pointer_report_count());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "i2c-pointer-error ", i2c_hid64_pointer_error());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "ps2-present ", input64_ps2_present());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "ps2-mouse-packets ", input64_mouse_packet_count());
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-refresh ", g_display_fileman_backend_refresh_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-preview ", g_display_fileman_backend_preview_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-open-dir ", g_display_fileman_backend_open_dir_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-write ", g_display_fileman_backend_write_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-write-denial ", g_display_fileman_backend_write_denial_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-delete ", g_display_fileman_backend_delete_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-delete-denial ", g_display_fileman_backend_delete_denial_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-delete-confirm ", g_display_fileman_backend_delete_confirm_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-mkdir ", g_display_fileman_backend_mkdir_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-mkdir-denial ", g_display_fileman_backend_mkdir_denial_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-copy ", g_display_fileman_backend_copy_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-copy-denial ", g_display_fileman_backend_copy_denial_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-rename ", g_display_fileman_backend_rename_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-rename-denial ", g_display_fileman_backend_rename_denial_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-move ", g_display_fileman_backend_move_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-move-denial ", g_display_fileman_backend_move_denial_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-edit ", g_display_fileman_backend_edit_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "fileman-edit-commit ", g_display_fileman_backend_edit_commit_count);
    cursor = display64_settings_append_u32_line(buffer, cursor, sizeof(g_display_settings_diag), "session ", services64_session_id());

    if (cursor == 0u)
    {
        ++g_display_settings_export_denial_count;
        return 0u;
    }

    if (mmio64_usb_fat_shell_write_file(
            (const u8 *)"USB:GUI-DIAG.TXT",
            16u,
            g_display_settings_diag,
            cursor,
            PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u)
    {
        wrote_usb = 1u;
    }

    if (mmio64_nvme_fat_shell_write_file(
            (const u8 *)"/DIAG.TXT",
            9u,
            g_display_settings_diag,
            cursor,
            PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u)
    {
        wrote_nvme = 1u;
    }

    if ((wrote_usb == 0u) && (wrote_nvme == 0u))
    {
        ++g_display_settings_export_denial_count;
        return 0u;
    }

    ++g_display_settings_export_count;
    return 1u;
}
#endif

static u32 display64_render_text_at(
    const u8 *bytes,
    u32 byte_count,
    u32 x,
    u32 y,
    u32 rgb,
    u32 *token)
{
    u32 index;
    u32 pixel;
    u32 base_x = x;
    u32 cursor_x = x;
    u32 cursor_y = y;
    u32 drawn = 0u;

    if ((bytes == 0) || !display64_has_framebuffer())
    {
        return 0u;
    }

    pixel = display64_make_pixel(rgb);
    for (index = 0u; index < byte_count; ++index)
    {
        u8 character = bytes[index];

        if (character == (u8)'\n')
        {
            cursor_x = base_x;
            cursor_y += display64_line_advance();
            continue;
        }

        if ((cursor_x + display64_font_advance()) >= g_display_boot_info->framebuffer_width)
        {
            cursor_x = base_x;
            cursor_y += display64_line_advance();
        }

        if ((cursor_y + display64_line_advance()) >= g_display_boot_info->framebuffer_height)
        {
            break;
        }

        drawn += display64_draw_glyph(character, cursor_x, cursor_y, pixel, token);
        cursor_x += display64_font_advance();
    }

    return drawn;
}

u32 display64_compositor_present(void)
{
    u32 x;
    u32 y;
    u32 width;
    u32 height;

    if (!display64_has_framebuffer())
    {
        return 0u;
    }

    if (g_display_compositor_dirty == 0u)
    {
        if ((g_display_compositor_cursor_drawn_valid != 0u)
            && (g_display_compositor_cursor_drawn_x == g_display_compositor_cursor_x)
            && (g_display_compositor_cursor_drawn_y == g_display_compositor_cursor_y)
            && (g_display_compositor_cursor_drawn_buttons == g_display_compositor_cursor_buttons))
        {
            return 0u;
        }

        display64_compositor_restore_cursor_saved();
        display64_compositor_save_cursor_underlay();
        display64_compositor_draw_cursor();
        ++g_display_compositor_present_count;
        return 1u;
    }

    display64_compositor_restore_cursor_saved();
    x = g_display_compositor_dirty_x;
    y = g_display_compositor_dirty_y;
    width = display64_min_u32(g_display_compositor_dirty_w, g_display_boot_info->framebuffer_width - x);
    height = display64_min_u32(g_display_compositor_dirty_h, g_display_boot_info->framebuffer_height - y);
    display64_compositor_present_back_buffer_rect(x, y, width, height);

    g_display_compositor_dirty = 0u;
    display64_compositor_save_cursor_underlay();
    display64_compositor_draw_cursor();
    ++g_display_compositor_present_count;
    return 1u;
}

static void display64_compositor_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 rgb)
{
    u32 token = 2166136261u;
    (void)display64_clear_rect(x, y, width, height, rgb, &token);
}

static void display64_compositor_draw_rect(u32 x, u32 y, u32 width, u32 height, u32 rgb)
{
    if ((width == 0u) || (height == 0u))
    {
        return;
    }

    display64_compositor_fill_rect(x, y, width, 1u, rgb);
    display64_compositor_fill_rect(x, y + height - 1u, width, 1u, rgb);
    display64_compositor_fill_rect(x, y, 1u, height, rgb);
    display64_compositor_fill_rect(x + width - 1u, y, 1u, height, rgb);
}

static u32 display64_draw_font_text(
    u32 x,
    u32 y,
    const char *text,
    u32 font_size,
    u32 color,
    u32 bg_color);

#if !defined(LIMITLESS_X64_UEFI_KERNEL) || !LIMITLESS_X64_UEFI_KERNEL
static u32 display64_rgb(u32 red, u32 green, u32 blue)
{
    return ((red & 0xFFu) << 16u) | ((green & 0xFFu) << 8u) | (blue & 0xFFu);
}
#endif

static void display64_compositor_fill_round_rect_4(u32 x, u32 y, u32 width, u32 height, u32 rgb)
{
    if ((width == 0u) || (height == 0u))
    {
        return;
    }

    if ((width < 8u) || (height < 8u))
    {
        display64_compositor_fill_rect(x, y, width, height, rgb);
        return;
    }

    display64_compositor_fill_rect(x + 4u, y, width - 8u, height, rgb);
    display64_compositor_fill_rect(x + 2u, y + 1u, width - 4u, height - 2u, rgb);
    display64_compositor_fill_rect(x + 1u, y + 2u, width - 2u, height - 4u, rgb);
    display64_compositor_fill_rect(x, y + 4u, width, height - 8u, rgb);
}

static void display64_compositor_fill_circle_16(u32 x, u32 y, u32 rgb)
{
    display64_compositor_fill_rect(x + 5u, y, 6u, 1u, rgb);
    display64_compositor_fill_rect(x + 3u, y + 1u, 10u, 1u, rgb);
    display64_compositor_fill_rect(x + 2u, y + 2u, 12u, 1u, rgb);
    display64_compositor_fill_rect(x + 1u, y + 3u, 14u, 1u, rgb);
    display64_compositor_fill_rect(x, y + 4u, 16u, 8u, rgb);
    display64_compositor_fill_rect(x + 1u, y + 12u, 14u, 1u, rgb);
    display64_compositor_fill_rect(x + 2u, y + 13u, 12u, 1u, rgb);
    display64_compositor_fill_rect(x + 3u, y + 14u, 10u, 1u, rgb);
    display64_compositor_fill_rect(x + 5u, y + 15u, 6u, 1u, rgb);
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void display64_compositor_draw_soft_shadow(u32 x, u32 y, u32 width, u32 height, u32 depth)
{
    if ((width < 8u) || (height < 8u) || (depth == 0u))
    {
        return;
    }

    display64_compositor_fill_round_rect_4(x + 2u, y + 2u, width, height, DISPLAY64_RGB_SHADOW);
    if (depth > 1u)
    {
        display64_compositor_fill_round_rect_4(x + 4u, y + 4u, width, height, DISPLAY64_RGB_SHADOW);
    }
}

static void display64_compositor_draw_surface(u32 x, u32 y, u32 width, u32 height, u32 fill_rgb, u32 border_rgb, u32 elevation)
{
    if ((width == 0u) || (height == 0u))
    {
        return;
    }

    display64_compositor_draw_soft_shadow(x, y, width, height, elevation);
    if ((width < 4u) || (height < 4u))
    {
        display64_compositor_fill_rect(x, y, width, height, fill_rgb);
        return;
    }

    display64_compositor_fill_round_rect_4(x, y, width, height, border_rgb);
    display64_compositor_fill_round_rect_4(x + 1u, y + 1u, width - 2u, height - 2u, fill_rgb);
    display64_compositor_fill_rect(x + 4u, y + 1u, width - 8u, 1u, DISPLAY64_RGB_HIGHLIGHT);
}

static void display64_compositor_draw_badge(u32 x, u32 y, u32 width, const char *text, u32 rgb)
{
    display64_compositor_fill_round_rect_4(x, y, width, 22u, DISPLAY64_RGB_SURFACE_BORDER);
    display64_compositor_fill_round_rect_4(x + 1u, y + 1u, width - 2u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    display64_compositor_fill_rect(x + 5u, y + 6u, 4u, 10u, rgb);
    (void)display64_draw_font_text(x + 14u, y + 4u, text, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
}

static void display64_desktop_draw_icon_glyph(u32 x, u32 y, u32 kind, u32 rgb)
{
    display64_compositor_fill_round_rect_4(x, y, 24u, 24u, rgb);

    if (kind == 0u)
    {
        (void)display64_draw_font_text(x + 4u, y + 4u, ">_", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
        return;
    }
    if (kind == 1u)
    {
        display64_compositor_fill_rect(x + 5u, y + 7u, 6u, 3u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 4u, y + 10u, 16u, 9u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 5u, y + 11u, 14u, 7u, rgb);
        return;
    }
    if (kind == 2u)
    {
        display64_compositor_fill_rect(x + 6u, y + 7u, 12u, 2u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 6u, y + 12u, 12u, 2u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 6u, y + 17u, 12u, 2u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 9u, y + 5u, 3u, 6u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 14u, y + 10u, 3u, 6u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 8u, y + 15u, 3u, 6u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        return;
    }
    if (kind == 3u)
    {
        display64_compositor_fill_rect(x + 6u, y + 6u, 12u, 10u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 8u, y + 8u, 8u, 6u, rgb);
        display64_compositor_fill_rect(x + 8u, y + 18u, 8u, 2u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 13u, y + 14u, 2u, 5u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        display64_compositor_fill_rect(x + 15u, y + 16u, 4u, 2u, DISPLAY64_RGB_TEXT_ON_ACCENT);
        return;
    }

    display64_compositor_fill_rect(x + 5u, y + 7u, 14u, 10u, DISPLAY64_RGB_TEXT_ON_ACCENT);
    display64_compositor_fill_rect(x + 7u, y + 9u, 10u, 6u, rgb);
    display64_compositor_fill_rect(x + 9u, y + 17u, 5u, 3u, DISPLAY64_RGB_TEXT_ON_ACCENT);
}

static void display64_desktop_draw_info_row(
    u32 x,
    u32 y,
    u32 width,
    const char *title,
    const char *detail,
    u32 accent_rgb)
{
    if (width < 24u)
    {
        return;
    }

    display64_compositor_draw_surface(x, y, width, 38u, DISPLAY64_RGB_SURFACE_HIGH, DISPLAY64_RGB_SURFACE_BORDER, 0u);
    display64_compositor_fill_rect(x + 5u, y + 8u, 3u, 22u, accent_rgb);
    (void)display64_draw_font_text(x + 14u, y + 5u, title, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(x + 14u, y + 21u, detail, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void display64_desktop_draw_info_row_selected(
    u32 x,
    u32 y,
    u32 width,
    const char *title,
    const char *detail,
    u32 accent_rgb,
    u32 selected)
{
    display64_desktop_draw_info_row(x, y, width, title, detail, accent_rgb);
    if ((selected != 0u) && (width > 4u))
    {
        display64_compositor_draw_rect(x + 1u, y + 1u, width - 2u, 36u, DISPLAY64_RGB_FOCUS_BLUE);
    }
}

static void display64_desktop_draw_fileman_row(
    u32 x,
    u32 y,
    u32 width,
    const char *title,
    const char *detail,
    u32 accent_rgb,
    u32 selected)
{
    if (width < 24u)
    {
        return;
    }

    display64_compositor_draw_surface(x, y, width, DISPLAY64_FILEMAN_ROW_HEIGHT, DISPLAY64_RGB_SURFACE_HIGH, DISPLAY64_RGB_SURFACE_BORDER, 0u);
    display64_compositor_fill_rect(x + 5u, y + 7u, 3u, 18u, accent_rgb);
    (void)display64_draw_font_text(x + 14u, y + 3u, title, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(x + 14u, y + 18u, detail, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    if ((selected != 0u) && (width > 4u))
    {
        display64_compositor_draw_rect(x + 1u, y + 1u, width - 2u, DISPLAY64_FILEMAN_ROW_HEIGHT - 2u, DISPLAY64_RGB_FOCUS_BLUE);
    }
}

static const char *display64_settings_title(u32 index)
{
    switch (index)
    {
        case 0u: return "Display scale";
        case 1u: return "Theme";
        case 2u: return "Pointer";
        case 3u: return "Keyboard";
        case 4u: return "Hardware";
        case 5u: return "Input devices";
        case 6u: return "Export diagnostics";
        case 7u: return "Storage";
        case 8u: return "Network";
        case 9u: return "Session lock";
        case 10u: return "Installer";
        case 11u: return "Identity";
        case 12u: return "Account";
        default: return "Package Trust";
    }
}

static const char *display64_settings_hardware_detail(void)
{
    u32 cursor = 0u;

    cursor = display64_diag_append_u32(
        g_display_settings_hardware_detail,
        cursor,
        sizeof(g_display_settings_hardware_detail),
        hardware64_registry_count());
    cursor = display64_diag_append_text(
        g_display_settings_hardware_detail,
        cursor,
        sizeof(g_display_settings_hardware_detail),
        " devices, ");
    cursor = display64_diag_append_u32(
        g_display_settings_hardware_detail,
        cursor,
        sizeof(g_display_settings_hardware_detail),
        hardware64_registry_driver_bound_count());
    cursor = display64_diag_append_text(
        g_display_settings_hardware_detail,
        cursor,
        sizeof(g_display_settings_hardware_detail),
        " bound, ");
    cursor = display64_diag_append_u32(
        g_display_settings_hardware_detail,
        cursor,
        sizeof(g_display_settings_hardware_detail),
        hardware64_registry_driver_deferred_count());
    cursor = display64_diag_append_text(
        g_display_settings_hardware_detail,
        cursor,
        sizeof(g_display_settings_hardware_detail),
        " deferred");
    g_display_settings_hardware_detail[cursor] = '\0';
    return g_display_settings_hardware_detail;
}

static const char *display64_settings_input_detail(void)
{
    u32 cursor = 0u;
    u32 pointer_live = ((input64_mouse_packet_count() != 0u)
        || (xhci64_mouse_reports() != 0u)
        || (i2c_hid64_pointer_report_count() != 0u)) ? 1u : 0u;

    cursor = display64_diag_append_text(
        g_display_settings_input_detail,
        cursor,
        sizeof(g_display_settings_input_detail),
        "kbd ");
    cursor = display64_diag_append_bool(
        g_display_settings_input_detail,
        cursor,
        sizeof(g_display_settings_input_detail),
        (input64_keyboard_scancode_count() != 0u) || (xhci64_report_count() != 0u));
    cursor = display64_diag_append_text(
        g_display_settings_input_detail,
        cursor,
        sizeof(g_display_settings_input_detail),
        ", pointer ");
    cursor = display64_diag_append_bool(
        g_display_settings_input_detail,
        cursor,
        sizeof(g_display_settings_input_detail),
        pointer_live);
    cursor = display64_diag_append_text(
        g_display_settings_input_detail,
        cursor,
        sizeof(g_display_settings_input_detail),
        ", USB reports ");
    cursor = display64_diag_append_u32(
        g_display_settings_input_detail,
        cursor,
        sizeof(g_display_settings_input_detail),
        xhci64_mouse_reports());
    cursor = display64_diag_append_text(
        g_display_settings_input_detail,
        cursor,
        sizeof(g_display_settings_input_detail),
        ", I2C ");
    cursor = display64_diag_append_bool(g_display_settings_input_detail, cursor, sizeof(g_display_settings_input_detail), i2c_hid64_pointer_found());
    cursor = display64_diag_append_char(g_display_settings_input_detail, cursor, sizeof(g_display_settings_input_detail), '/');
    cursor = display64_diag_append_u32(g_display_settings_input_detail, cursor, sizeof(g_display_settings_input_detail), i2c_hid64_pointer_error());
    g_display_settings_input_detail[cursor] = '\0';
    return g_display_settings_input_detail;
}

static const char *display64_settings_storage_detail(void)
{
    u32 cursor = 0u;

    cursor = display64_diag_append_text(
        g_display_settings_storage_detail,
        cursor,
        sizeof(g_display_settings_storage_detail),
        (mmio64_nvme_fat_located() != 0u) ? "NVMe FAT mounted, " : "NVMe FAT unavailable, ");
    cursor = display64_diag_append_text(
        g_display_settings_storage_detail,
        cursor,
        sizeof(g_display_settings_storage_detail),
        (mmio64_usb_fat_located() != 0u) ? "USB FAT export ready, " : "USB FAT unavailable, ");
    cursor = display64_diag_append_text(
        g_display_settings_storage_detail,
        cursor,
        sizeof(g_display_settings_storage_detail),
        (mmio64_nvme_rw_delegated() != 0u) ? "NVMe writes scoped" : "NVMe read-only");
    g_display_settings_storage_detail[cursor] = '\0';
    return g_display_settings_storage_detail;
}

static const char *display64_settings_network_detail(void)
{
    u32 cursor = 0u;

    cursor = display64_diag_append_u32(
        g_display_settings_network_detail,
        cursor,
        sizeof(g_display_settings_network_detail),
        hardware64_registry_network_device_count());
    cursor = display64_diag_append_text(
        g_display_settings_network_detail,
        cursor,
        sizeof(g_display_settings_network_detail),
        " devices, broker ");
    cursor = display64_diag_append_text(
        g_display_settings_network_detail,
        cursor,
        sizeof(g_display_settings_network_detail),
        (hardware64_registry_network_device_count() != 0u) ? "detected" : "unavailable");
    g_display_settings_network_detail[cursor] = '\0';
    return g_display_settings_network_detail;
}

static const char *display64_settings_detail(u32 index)
{
    switch (index)
    {
        case 0u:
            if (g_display_text_scale <= 1u) { return "small, persisted to /SETTINGS.CFG"; }
            if (g_display_text_scale >= 3u) { return "large, persisted to /SETTINGS.CFG"; }
            return "normal, persisted to /SETTINGS.CFG";
        case 1u:
            return (g_display_settings_theme != 0u) ? "light accent, persisted" : "dark accent, persisted";
        case 2u:
            if (g_display_settings_pointer_speed <= DISPLAY64_SETTINGS_POINTER_SLOW) { return "slow cursor, persisted"; }
            if (g_display_settings_pointer_speed >= DISPLAY64_SETTINGS_POINTER_FAST) { return "fast cursor, persisted"; }
            return "normal cursor, persisted";
        case 3u:
            return (g_display_settings_key_repeat != 0u) ? "repeat policy on, persisted" : "repeat policy off, persisted";
        case 4u: return display64_settings_hardware_detail();
        case 5u: return display64_settings_input_detail();
        case 6u: return "writes USB:GUI-DIAG.TXT, mirrors /DIAG.TXT when available";
        case 7u: return display64_settings_storage_detail();
        case 8u: return display64_settings_network_detail();
        case 9u: return "click to lock current session";
        case 10u: return "dry-run only, writes disabled";
        case 11u: return "local identity metadata";
        case 12u: return "local-only account state";
        default: return "signed package inventory";
    }
}

static u32 display64_settings_accent(u32 index)
{
    switch (index)
    {
        case 0u: return DISPLAY64_RGB_FOCUS_BLUE;
        case 1u: return (g_display_settings_theme != 0u) ? DISPLAY64_RGB_WARNING : DISPLAY64_RGB_APP_SETTINGS;
        case 2u: return DISPLAY64_RGB_ACCENT;
        case 3u: return DISPLAY64_RGB_APP_ASSISTANT;
        case 4u: return DISPLAY64_RGB_FOCUS_BLUE;
        case 5u: return DISPLAY64_RGB_ACCENT;
        case 6u: return DISPLAY64_RGB_APP_FILES;
        case 7u: return DISPLAY64_RGB_APP_FILES;
        case 8u: return DISPLAY64_RGB_ACCENT;
        case 9u: return DISPLAY64_RGB_APP_SETTINGS;
        case 10u: return DISPLAY64_RGB_APP_INSTALLER;
        case 11u:
        case 12u: return DISPLAY64_RGB_APP_SETTINGS;
        case 13u: return DISPLAY64_RGB_WARNING;
        default: return DISPLAY64_RGB_TEXT_SECONDARY;
    }
}

static void display64_settings_activate_row(u32 index)
{
    if (index == 0u)
    {
        g_display_text_scale = (g_display_text_scale >= DISPLAY64_FONT_MAX_SCALE) ? 1u : (g_display_text_scale + 1u);
        (void)display64_settings_save();
        display64_configure_console_layout();
        return;
    }
    if (index == 1u)
    {
        g_display_settings_theme = (g_display_settings_theme == DISPLAY64_SETTINGS_THEME_DARK)
            ? DISPLAY64_SETTINGS_THEME_LIGHT
            : DISPLAY64_SETTINGS_THEME_DARK;
        (void)display64_settings_save();
        return;
    }
    if (index == 2u)
    {
        g_display_settings_pointer_speed = (g_display_settings_pointer_speed >= DISPLAY64_SETTINGS_POINTER_FAST)
            ? DISPLAY64_SETTINGS_POINTER_SLOW
            : (g_display_settings_pointer_speed + 1u);
        (void)display64_settings_save();
        return;
    }
    if (index == 3u)
    {
        g_display_settings_key_repeat = (g_display_settings_key_repeat == 0u) ? 1u : 0u;
        (void)display64_settings_save();
        return;
    }
    if (index == 6u)
    {
        (void)display64_settings_export_diagnostics();
        return;
    }
    if (index == 9u)
    {
        (void)auth64_lock_session();
    }
}

static void display64_desktop_draw_readiness_pill(
    u32 x,
    u32 y,
    u32 width,
    const char *label,
    u32 ready,
    u32 accent_rgb)
{
    u32 fill = (ready != 0u) ? DISPLAY64_RGB_SURFACE_HIGH : DISPLAY64_RGB_FIELD;
    u32 text = (ready != 0u) ? DISPLAY64_RGB_TEXT_PRIMARY : DISPLAY64_RGB_TEXT_MUTED;

    display64_compositor_draw_surface(x, y, width, 22u, fill, DISPLAY64_RGB_SURFACE_BORDER, 0u);
    display64_compositor_fill_rect(x + 6u, y + 6u, 4u, 10u, (ready != 0u) ? accent_rgb : DISPLAY64_RGB_DISABLED_TEXT);
    (void)display64_draw_font_text(x + 16u, y + 5u, label, DISPLAY64_FONT_SMALL, text, DISPLAY64_FONT_TRANSPARENT);
}

static void display64_desktop_draw_status_card(
    u32 x,
    u32 y,
    u32 width,
    const char *title,
    const char *detail,
    u32 ready,
    u32 accent_rgb)
{
    u32 state_rgb = (ready != 0u) ? accent_rgb : DISPLAY64_RGB_WARNING;

    if (width < 96u)
    {
        return;
    }

    display64_compositor_draw_surface(x, y, width, 50u, DISPLAY64_RGB_CONTENT, DISPLAY64_RGB_SURFACE_BORDER, 0u);
    display64_compositor_fill_rect(x + 8u, y + 9u, 4u, 32u, state_rgb);
    (void)display64_draw_font_text(x + 18u, y + 8u, title, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(x + 18u, y + 25u, detail, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(x + width - 50u, y + 14u, 34u, 20u, (ready != 0u) ? DISPLAY64_RGB_SURFACE_HIGH : DISPLAY64_RGB_FIELD);
    (void)display64_draw_font_text(x + width - 42u, y + 18u, (ready != 0u) ? "Ready" : "Down", DISPLAY64_FONT_SMALL, (ready != 0u) ? DISPLAY64_RGB_TEXT_PRIMARY : DISPLAY64_RGB_TEXT_MUTED, DISPLAY64_FONT_TRANSPARENT);
}

static u32 display64_desktop_draw_readiness_strip(u32 body_x, u32 body_y, u32 width)
{
    u32 pill_w;
    u32 input_ready = ((input64_mouse_packet_count() != 0u)
        || (xhci64_mouse_reports() != 0u)
        || (i2c_hid64_pointer_report_count() != 0u)
        || (input64_keyboard_scancode_count() != 0u)
        || (xhci64_report_count() != 0u)) ? 1u : 0u;

    if (width < 156u)
    {
        return 0u;
    }

    ++g_display_settings_readiness_strip_count;
    if (width < 304u)
    {
        pill_w = (width - 8u) / 2u;
        display64_desktop_draw_readiness_pill(
            body_x,
            body_y,
            pill_w,
            "Display",
            display64_readable(),
            DISPLAY64_RGB_FOCUS_BLUE);
        display64_desktop_draw_readiness_pill(
            body_x + pill_w + 8u,
            body_y,
            pill_w,
            "Input",
            input_ready,
            DISPLAY64_RGB_ACCENT);
        display64_desktop_draw_readiness_pill(
            body_x,
            body_y + 26u,
            pill_w,
            "Storage",
            mmio64_nvme_fat_located(),
            DISPLAY64_RGB_APP_FILES);
        display64_desktop_draw_readiness_pill(
            body_x + pill_w + 8u,
            body_y + 26u,
            pill_w,
            "Network",
            hardware64_registry_network_device_count(),
            DISPLAY64_RGB_APP_ASSISTANT);
        return 48u;
    }

    pill_w = (width - 44u) / 4u;
    display64_desktop_draw_readiness_pill(
        body_x,
        body_y,
        pill_w,
        "Display",
        display64_readable(),
        DISPLAY64_RGB_FOCUS_BLUE);
    display64_desktop_draw_readiness_pill(
        body_x + pill_w + 8u,
        body_y,
        pill_w,
        "Input",
        input_ready,
        DISPLAY64_RGB_ACCENT);
    display64_desktop_draw_readiness_pill(
        body_x + ((pill_w + 8u) * 2u),
        body_y,
        pill_w,
        "Storage",
        mmio64_nvme_fat_located(),
        DISPLAY64_RGB_APP_FILES);
    display64_desktop_draw_readiness_pill(
        body_x + ((pill_w + 8u) * 3u),
        body_y,
        pill_w,
        "Network",
        hardware64_registry_network_device_count(),
        DISPLAY64_RGB_APP_ASSISTANT);
    return 22u;
}

static void display64_desktop_draw_settings_detail_card(
    u32 body_x,
    u32 body_y,
    u32 width,
    u32 selected_index);
static void display64_draw_label_value(u32 x, u32 y, const char *label, u32 value, u32 rgb);

static void display64_desktop_draw_settings_summary(
    u32 body_x,
    u32 body_y,
    u32 width)
{
    u32 row_index;
    u32 visible_row = 0u;
    u32 start = g_display_settings_scroll_index;
    u32 row_count = DISPLAY64_SETTINGS_ROW_COUNT;
    u32 row_h = DISPLAY64_SETTINGS_ROW_STEP;
    u32 row_w = (width > 32u) ? (width - 32u) : width;
    u32 readiness_h;
    u32 action_y;

    if (start >= row_count)
    {
        start = 0u;
        g_display_settings_scroll_index = 0u;
    }

    (void)display64_draw_font_text(body_x, body_y, "Settings", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_draw_badge(body_x + 96u, body_y - 4u, 94u, "Product", DISPLAY64_RGB_ACCENT);
    for (row_index = start; (row_index < row_count) && (visible_row < DISPLAY64_SETTINGS_VISIBLE_ROWS); ++row_index)
    {
        u32 row_y = body_y + 28u + (visible_row * row_h);
        display64_desktop_draw_info_row_selected(
            body_x,
            row_y,
            row_w,
            display64_settings_title(row_index),
            display64_settings_detail(row_index),
            display64_settings_accent(row_index),
            (row_index == g_display_settings_selected_index) ? 1u : 0u);
        ++visible_row;
    }

    display64_desktop_draw_settings_detail_card(body_x, body_y, width, g_display_settings_selected_index);
    (void)display64_draw_font_text(body_x, body_y + DISPLAY64_SETTINGS_READINESS_Y, "System readiness", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    readiness_h = display64_desktop_draw_readiness_strip(body_x, body_y + DISPLAY64_SETTINGS_READINESS_Y + 16u, row_w);
    action_y = body_y + DISPLAY64_SETTINGS_READINESS_Y + 22u + readiness_h;
    display64_compositor_fill_round_rect_4(body_x, action_y, 72u, 24u, DISPLAY64_RGB_ACCENT);
    (void)display64_draw_font_text(body_x + 18u, action_y + 5u, "Lock", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_draw_badge(body_x + 84u, action_y + 1u, 116u, display64_settings_title(g_display_settings_selected_index), display64_settings_accent(g_display_settings_selected_index));
    if (g_display_settings_save_count != 0u)
    {
        (void)display64_draw_font_text(body_x + 210u, action_y + 6u, "Saved", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_APP_FILES, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_settings_export_count != 0u)
    {
        (void)display64_draw_font_text(body_x + 210u, action_y + 6u, "Exported", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_APP_FILES, DISPLAY64_FONT_TRANSPARENT);
    }
    else if ((g_display_settings_save_denial_count + g_display_settings_export_denial_count) != 0u)
    {
        (void)display64_draw_font_text(body_x + 210u, action_y + 6u, "Denied", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }
}

static u32 display64_desktop_settings_readiness_height(u32 width)
{
    return (width < 304u) ? 48u : 22u;
}

static void display64_desktop_draw_settings_detail_card(
    u32 body_x,
    u32 body_y,
    u32 width,
    u32 selected_index)
{
    u32 card_y = body_y + DISPLAY64_SETTINGS_DETAIL_Y;
    u32 card_w = (width > 32u) ? (width - 32u) : width;

    if (card_w < 140u)
    {
        return;
    }

    display64_compositor_draw_surface(
        body_x,
        card_y,
        card_w,
        DISPLAY64_SETTINGS_DETAIL_HEIGHT,
        DISPLAY64_RGB_CONTENT,
        DISPLAY64_RGB_SURFACE_BORDER,
        0u);
    display64_compositor_fill_rect(body_x + 8u, card_y + 8u, 3u, 16u, display64_settings_accent(selected_index));
    (void)display64_draw_font_text(body_x + 18u, card_y + 7u, display64_settings_title(selected_index), DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);

    if (selected_index == 0u)
    {
        display64_draw_label_value(body_x + 18u, card_y + 26u, "Scale ", g_display_text_scale, DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 92u, card_y + 26u, "W ", display64_width(), DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 166u, card_y + 26u, "H ", display64_height(), DISPLAY64_RGB_TEXT_SECONDARY);
    }
    else if (selected_index == 2u)
    {
        display64_draw_label_value(body_x + 18u, card_y + 26u, "Pointer speed ", g_display_settings_pointer_speed, DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 154u, card_y + 26u, "Packets ", input64_mouse_packet_count(), DISPLAY64_RGB_TEXT_SECONDARY);
    }
    else if (selected_index == 4u)
    {
        display64_draw_label_value(body_x + 18u, card_y + 26u, "Devices ", hardware64_registry_count(), DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 112u, card_y + 26u, "Bound ", hardware64_registry_driver_bound_count(), DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 192u, card_y + 26u, "Deferred ", hardware64_registry_driver_deferred_count(), DISPLAY64_RGB_TEXT_SECONDARY);
    }
    else if (selected_index == 5u)
    {
        display64_draw_label_value(body_x + 18u, card_y + 26u, "USB reports ", xhci64_mouse_reports(), DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 134u, card_y + 26u, "I2C err ", i2c_hid64_pointer_error(), DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 216u, card_y + 26u, "PS2 ", input64_mouse_packet_count(), DISPLAY64_RGB_TEXT_SECONDARY);
        if (card_w > 236u)
        {
            (void)display64_draw_font_text(body_x + 18u, card_y + 42u, (const char *)display64_settings_input_detail(), DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_MUTED, DISPLAY64_FONT_TRANSPARENT);
        }
    }
    else if (selected_index == 6u)
    {
        (void)display64_draw_font_text(body_x + 18u, card_y + 26u, "Exports GUI-DIAG.TXT to USB FAT when available", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
        display64_draw_label_value(body_x + 18u, card_y + 42u, "USB FAT ", mmio64_usb_fat_located(), DISPLAY64_RGB_TEXT_MUTED);
    }
    else if (selected_index == 7u)
    {
        display64_draw_label_value(body_x + 18u, card_y + 26u, "NVMe FAT ", mmio64_nvme_fat_located(), DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 120u, card_y + 26u, "USB FAT ", mmio64_usb_fat_located(), DISPLAY64_RGB_TEXT_SECONDARY);
        display64_draw_label_value(body_x + 222u, card_y + 26u, "RW ", mmio64_nvme_rw_delegated(), DISPLAY64_RGB_TEXT_SECONDARY);
        if (card_w > 236u)
        {
            (void)display64_draw_font_text(body_x + 18u, card_y + 42u, (const char *)display64_settings_storage_detail(), DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_MUTED, DISPLAY64_FONT_TRANSPARENT);
        }
    }
    else if (selected_index == 8u)
    {
        display64_draw_label_value(body_x + 18u, card_y + 26u, "Network devices ", hardware64_registry_network_device_count(), DISPLAY64_RGB_TEXT_SECONDARY);
        (void)display64_draw_font_text(body_x + 18u, card_y + 42u, "Brokered network remains gated until a real adapter binds", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_MUTED, DISPLAY64_FONT_TRANSPARENT);
    }
    else
    {
        (void)display64_draw_font_text(body_x + 18u, card_y + 26u, display64_settings_detail(selected_index), DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    }
}
#endif
#endif

static u32 display64_string_length(const char *text)
{
    u32 length = 0u;

    if (text == 0)
    {
        return 0u;
    }

    while ((text[length] != '\0') && (length < 512u))
    {
        ++length;
    }

    return length;
}

static void display64_u32_to_dec_text(u32 value, char *text, u32 capacity)
{
    char reverse[10];
    u32 length = 0u;
    u32 index;

    if ((text == 0) || (capacity == 0u))
    {
        return;
    }

    if (value == 0u)
    {
        text[0] = '0';
        if (capacity > 1u)
        {
            text[1] = '\0';
        }
        return;
    }

    while ((value != 0u) && (length < (u32)(sizeof(reverse) / sizeof(reverse[0]))))
    {
        reverse[length++] = (char)('0' + (char)(value % 10u));
        value /= 10u;
    }

    if (length >= capacity)
    {
        length = capacity - 1u;
    }

    for (index = 0u; index < length; ++index)
    {
        text[index] = reverse[length - 1u - index];
    }
    text[length] = '\0';
}

static u32 display64_font_width(u32 font_size)
{
    if (font_size == DISPLAY64_FONT_LARGE)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        return 14u;
#else
        return 16u;
#endif
    }

    if (font_size == DISPLAY64_FONT_NORMAL)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        return 7u;
#else
        return 8u;
#endif
    }

    return 5u;
}

static u32 display64_font_height(u32 font_size)
{
    if (font_size == DISPLAY64_FONT_LARGE)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        return 28u;
#else
        return 32u;
#endif
    }

    if (font_size == DISPLAY64_FONT_NORMAL)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        return 14u;
#else
        return 16u;
#endif
    }

    return 7u;
}

static u32 display64_font_bit(u8 character, u32 font_size, u32 column, u32 row)
{
    u32 source_column = column;
    u32 source_row = row;
    u8 bits;

    if (font_size == DISPLAY64_FONT_LARGE)
    {
        source_column = column >> 1u;
        source_row = row >> 1u;
        font_size = DISPLAY64_FONT_NORMAL;
    }

    if (font_size == DISPLAY64_FONT_NORMAL)
    {
        if ((column == 0u) || (column > 5u))
        {
            return 0u;
        }
        source_column = column - 1u;
        source_row = row >> 1u;
    }

    if ((source_column >= DISPLAY64_FONT_WIDTH) || (source_row >= DISPLAY64_FONT_HEIGHT))
    {
        return 0u;
    }

    bits = display64_glyph_row(character, source_row);
    return ((bits & (u8)(1u << (DISPLAY64_FONT_WIDTH - 1u - source_column))) != 0u) ? 1u : 0u;
}

static u32 display64_draw_font_text(
    u32 x,
    u32 y,
    const char *text,
    u32 font_size,
    u32 color,
    u32 bg_color)
{
    volatile u32 *buffer;
    u32 width = display64_font_width(font_size);
    u32 height = display64_font_height(font_size);
    u32 advance = width + 1u;
    u32 cursor_x = x;
    u32 cursor_y = y;
    u32 color_pixel;
    u32 bg_pixel = 0u;
    u32 index;
    u32 drawn = 0u;
    u32 text_length = display64_string_length(text);

    if ((text == 0) || (text_length == 0u) || !display64_has_framebuffer())
    {
        return 0u;
    }

    buffer = display64_draw_buffer();
    color_pixel = display64_make_pixel(color);
    if (bg_color != DISPLAY64_FONT_TRANSPARENT)
    {
        bg_pixel = display64_make_pixel(bg_color);
    }

    for (index = 0u; index < text_length; ++index)
    {
        u8 character = (u8)text[index];
        u32 row;
        u32 column;

        if (character == (u8)'\n')
        {
            cursor_x = x;
            cursor_y += height + 2u;
            continue;
        }

        if ((cursor_x + width) >= g_display_boot_info->framebuffer_width)
        {
            cursor_x = x;
            cursor_y += height + 2u;
        }
        if ((cursor_y + height) >= g_display_boot_info->framebuffer_height)
        {
            break;
        }

        for (row = 0u; row < height; ++row)
        {
            for (column = 0u; column < width; ++column)
            {
                u64 pixel_index = 0ull;
                u32 bit = display64_font_bit(character, font_size, column, row);
                if ((bit == 0u) && (bg_color == DISPLAY64_FONT_TRANSPARENT))
                {
                    continue;
                }
                if (display64_pixel_index(cursor_x + column, cursor_y + row, &pixel_index) == 0)
                {
                    continue;
                }

                buffer[pixel_index] = (bit != 0u) ? color_pixel : bg_pixel;
                display64_compositor_mark_dirty(cursor_x + column, cursor_y + row, 1u, 1u);
                ++drawn;
            }
        }

        cursor_x += advance;
    }

    if (drawn != 0u)
    {
        ++g_display_font_render_count;
    }

    return drawn;
}

static u32 display64_font_text_advance(const char *text, u32 font_size)
{
    return display64_string_length(text) * (display64_font_width(font_size) + 1u);
}

static void display64_draw_label_value(
    u32 x,
    u32 y,
    const char *label,
    u32 value,
    u32 color)
{
    char value_text[12];

    display64_u32_to_dec_text(value, value_text, (u32)sizeof(value_text));
    (void)display64_draw_font_text(x, y, label, DISPLAY64_FONT_NORMAL, color, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(
        x + display64_font_text_advance(label, DISPLAY64_FONT_NORMAL),
        y,
        value_text,
        DISPLAY64_FONT_NORMAL,
        color,
        DISPLAY64_FONT_TRANSPARENT);
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 display64_product_input_ready_internal(void)
{
    return ((input64_mouse_packet_count() != 0u)
        || (xhci64_mouse_reports() != 0u)
        || (i2c_hid64_pointer_report_count() != 0u)
        || (input64_keyboard_scancode_count() != 0u)
        || (xhci64_report_count() != 0u)) ? 1u : 0u;
}

static void display64_font_draw_system_chip(
    u32 x,
    u32 y,
    u32 width,
    const char *label,
    u32 ready,
    u32 accent_rgb)
{
    u32 fill_rgb = (ready != 0u) ? DISPLAY64_RGB_SURFACE_HIGH : DISPLAY64_RGB_FIELD;
    u32 text_rgb = (ready != 0u) ? DISPLAY64_RGB_TEXT_SECONDARY : DISPLAY64_RGB_TEXT_MUTED;

    display64_compositor_draw_surface(x, y, width, 22u, fill_rgb, DISPLAY64_RGB_SURFACE_BORDER, 0u);
    display64_compositor_fill_round_rect_4(x + 8u, y + 7u, 8u, 8u, (ready != 0u) ? accent_rgb : DISPLAY64_RGB_DISABLED_TEXT);
    (void)display64_draw_font_text(x + 22u, y + 5u, label, DISPLAY64_FONT_SMALL, text_rgb, DISPLAY64_FONT_TRANSPARENT);
}
#endif

static void display64_font_draw_status_bar(void)
{
    u32 y = 0u;
    u32 height;
    u32 brand_y;
    u32 time_y;
    u32 time_x;
    const char *time_text = "time --:--";

    if (!display64_has_framebuffer())
    {
        return;
    }

    height = display64_min_u32(40u, g_display_boot_info->framebuffer_height);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, height, DISPLAY64_RGB_DESKTOP_BG);
    if (height > 4u)
    {
        display64_compositor_fill_rect(0u, y + height - 4u, g_display_boot_info->framebuffer_width, 4u, DISPLAY64_RGB_BAR_BG);
    }
#else
    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, height, DISPLAY64_RGB_BAR_BG);
#endif
    if (height != 0u)
    {
        display64_compositor_fill_rect(0u, y + height - 1u, g_display_boot_info->framebuffer_width, 1u, DISPLAY64_RGB_SURFACE_BORDER);
    }
    brand_y = (height > display64_font_height(DISPLAY64_FONT_LARGE))
        ? (y + ((height - display64_font_height(DISPLAY64_FONT_LARGE)) / 2u))
        : y;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_compositor_fill_round_rect_4(12u, brand_y + 4u, 6u, 18u, DISPLAY64_RGB_ACCENT);
    (void)display64_draw_font_text(26u, brand_y, "LimitlessOS", DISPLAY64_FONT_LARGE, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_draw_badge(152u, brand_y + 5u, 74u, "Product", DISPLAY64_RGB_ACCENT);
    if (g_display_boot_info->framebuffer_width >= 920u)
    {
        display64_font_draw_system_chip(248u, 9u, 78u, "Display", display64_readable(), DISPLAY64_RGB_FOCUS_BLUE);
        display64_font_draw_system_chip(334u, 9u, 70u, "Input", display64_product_input_ready_internal(), DISPLAY64_RGB_ACCENT);
        display64_font_draw_system_chip(412u, 9u, 82u, "Storage", mmio64_nvme_fat_located(), DISPLAY64_RGB_APP_FILES);
        display64_font_draw_system_chip(502u, 9u, 76u, "Network", hardware64_registry_network_device_count(), DISPLAY64_RGB_APP_ASSISTANT);
    }
    if (g_display_product_chrome_count == 0u)
    {
        ++g_display_product_chrome_count;
    }
#else
    (void)display64_draw_font_text(10u, brand_y, "LimitlessOS", DISPLAY64_FONT_LARGE, DISPLAY64_RGB_ACCENT, DISPLAY64_FONT_TRANSPARENT);
#endif
    time_x = (g_display_boot_info->framebuffer_width
            > (display64_font_text_advance(time_text, DISPLAY64_FONT_NORMAL) + 12u))
        ? (g_display_boot_info->framebuffer_width
            - display64_font_text_advance(time_text, DISPLAY64_FONT_NORMAL)
            - 12u)
        : 0u;
    time_y = (height > display64_font_height(DISPLAY64_FONT_NORMAL))
        ? (y + ((height - display64_font_height(DISPLAY64_FONT_NORMAL)) / 2u))
        : y;
    (void)display64_draw_font_text(time_x, time_y, time_text, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
}

void display64_compositor_probe(u32 cursor_x, u32 cursor_y, u32 buttons)
{
    u32 probe_y;

    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        return;
    }

    display64_compositor_mark_cursor(g_display_compositor_cursor_x, g_display_compositor_cursor_y);
    if (cursor_x >= g_display_boot_info->framebuffer_width)
    {
        cursor_x = g_display_boot_info->framebuffer_width - 1u;
    }
    if (cursor_y >= g_display_boot_info->framebuffer_height)
    {
        cursor_y = g_display_boot_info->framebuffer_height - 1u;
    }
    g_display_compositor_cursor_x = cursor_x;
    g_display_compositor_cursor_y = cursor_y;
    g_display_compositor_cursor_buttons = buttons;
    display64_compositor_mark_cursor(g_display_compositor_cursor_x, g_display_compositor_cursor_y);

    probe_y = (g_display_boot_info->framebuffer_height > 56u)
        ? (g_display_boot_info->framebuffer_height - 56u)
        : 0u;
    display64_compositor_fill_rect(12u, probe_y, 128u, 28u, DISPLAY64_RGB_SURFACE);
    display64_compositor_draw_rect(12u, probe_y, 128u, 28u, DISPLAY64_RGB_SURFACE_BORDER);
    (void)display64_compositor_present();
}

u32 display64_compositor_update_cursor(u32 cursor_x, u32 cursor_y, u32 buttons)
{
    u32 old_x;
    u32 old_y;
    u32 old_w;
    u32 old_h;
    u32 new_w;
    u32 new_h;
    u32 union_x = 0u;
    u32 union_y = 0u;
    u32 union_w = 0u;
    u32 union_h = 0u;

    if (
#if !defined(LIMITLESS_X64_UEFI_KERNEL) || !LIMITLESS_X64_UEFI_KERNEL
        (g_display_compositor_active == 0u) ||
#endif
        !display64_has_framebuffer())
    {
        return 0u;
    }

    display64_compositor_clamp_cursor(&cursor_x, &cursor_y);
    buttons &= 0x7u;
    if ((g_display_compositor_cursor_drawn_valid != 0u)
        && (cursor_x == g_display_compositor_cursor_drawn_x)
        && (cursor_y == g_display_compositor_cursor_drawn_y)
        && (buttons == g_display_compositor_cursor_drawn_buttons))
    {
        return 0u;
    }

    old_x = g_display_compositor_cursor_x;
    old_y = g_display_compositor_cursor_y;
    display64_compositor_cursor_rect(old_x, old_y, &old_w, &old_h);
    display64_compositor_union_rect(old_x, old_y, old_w, old_h, &union_x, &union_y, &union_w, &union_h);

    display64_compositor_restore_cursor_saved();
    g_display_compositor_cursor_x = cursor_x;
    g_display_compositor_cursor_y = cursor_y;
    g_display_compositor_cursor_buttons = buttons;
    display64_compositor_cursor_rect(g_display_compositor_cursor_x, g_display_compositor_cursor_y, &new_w, &new_h);
    display64_compositor_union_rect(
        g_display_compositor_cursor_x,
        g_display_compositor_cursor_y,
        new_w,
        new_h,
        &union_x,
        &union_y,
        &union_w,
        &union_h);

    if ((g_display_compositor_active != 0u) && (union_w != 0u) && (union_h != 0u))
    {
        display64_compositor_present_back_buffer_rect(union_x, union_y, union_w, union_h);
    }

    display64_compositor_save_cursor_underlay();
    display64_compositor_draw_cursor();
    ++g_display_compositor_present_count;
    return 1u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 display64_login_text_equal(const char *left, const char *right)
{
    u32 index = 0u;

    if ((left == (const char *)0) || (right == (const char *)0))
    {
        return 0u;
    }

    while ((left[index] != '\0') && (right[index] != '\0'))
    {
        if (left[index] != right[index])
        {
            return 0u;
        }
        ++index;
    }

    return (left[index] == '\0') && (right[index] == '\0');
}

static u32 display64_login_classify_state(const char *title, const char *message)
{
    if (display64_login_text_equal(title, "First-run setup") != 0u)
    {
        return DISPLAY64_LOGIN_STATE_SETUP;
    }
    if (display64_login_text_equal(title, "Login accepted") != 0u)
    {
        if (display64_login_text_equal(message, "Hardware input recovery session") != 0u)
        {
            return DISPLAY64_LOGIN_STATE_RECOVERY;
        }
        return DISPLAY64_LOGIN_STATE_ACCEPTED;
    }
    if (display64_login_text_equal(title, "Login denied") != 0u)
    {
        return DISPLAY64_LOGIN_STATE_DENIED;
    }
    if (display64_login_text_equal(title, "Login locked") != 0u)
    {
        return DISPLAY64_LOGIN_STATE_LOCKED;
    }
    if (display64_login_text_equal(title, "Session locked") != 0u)
    {
        return DISPLAY64_LOGIN_STATE_LOCKED;
    }
    if (display64_login_text_equal(title, "Session unlocked") != 0u)
    {
        return DISPLAY64_LOGIN_STATE_UNLOCKED;
    }
    if (display64_login_text_equal(title, "Session lock unavailable") != 0u)
    {
        return DISPLAY64_LOGIN_STATE_UNAVAILABLE;
    }
    if (display64_login_text_equal(message, "Using default local console account") != 0u)
    {
        return DISPLAY64_LOGIN_STATE_RECOVERY;
    }

    return DISPLAY64_LOGIN_STATE_LOGIN;
}

static void display64_login_record_state(u32 state)
{
    ++g_display_login_present_count;
    g_display_login_last_state = state;
    if (state == DISPLAY64_LOGIN_STATE_SETUP)
    {
        ++g_display_login_setup_present_count;
    }
    if (state == DISPLAY64_LOGIN_STATE_LOCKED)
    {
        ++g_display_login_lock_present_count;
    }
    if (state == DISPLAY64_LOGIN_STATE_UNLOCKED)
    {
        ++g_display_login_unlock_present_count;
    }
    if (state == DISPLAY64_LOGIN_STATE_RECOVERY)
    {
        ++g_display_login_recovery_present_count;
    }
    if ((state == DISPLAY64_LOGIN_STATE_SETUP)
        || (state == DISPLAY64_LOGIN_STATE_LOGIN)
        || (state == DISPLAY64_LOGIN_STATE_LOCKED))
    {
        ++g_display_login_wait_visible_count;
    }
    ++g_display_login_safe_path_count;
}

static void display64_login_draw_state_row(u32 x, u32 y, const char *label, const char *value, u32 accent)
{
    display64_compositor_fill_round_rect_4(x, y, 456u, 26u, DISPLAY64_RGB_SURFACE_HIGH);
    display64_compositor_fill_rect(x + 8u, y + 7u, 3u, 12u, accent);
    (void)display64_draw_font_text(x + 20u, y + 7u, label, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(x + 132u, y + 7u, value, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
}
#endif

void display64_font_probe(void)
{
    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        g_display_font_active = 0u;
        return;
    }

    g_display_font_active = 1u;
    display64_font_draw_status_bar();
    (void)display64_draw_font_text(24u, 48u, "Display ready", DISPLAY64_FONT_LARGE, DISPLAY64_RGB_ACCENT, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_compositor_present();
}

void display64_login_screen_draw(const char *title, const char *message, u32 failures, u32 lockout_seconds)
{
    u32 panel_w;
    u32 panel_h;
    u32 panel_x;
    u32 panel_y;
    u32 field_w;
    u32 field_x;
    u32 username_y;
    u32 password_y;
    u32 button_y;
    u32 button_text_x;
    u32 button_text_y;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 state;
    const char *button_text = "Continue";
    u32 accent = DISPLAY64_RGB_ACCENT;
#endif
#if !defined(LIMITLESS_X64_UEFI_KERNEL) || !LIMITLESS_X64_UEFI_KERNEL
    u32 inset;
    u32 red;
    u32 green;
    u32 blue;
#endif

    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        return;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    state = display64_login_classify_state(title, message);
    display64_login_record_state(state);
    if ((state == DISPLAY64_LOGIN_STATE_LOCKED) || (state == DISPLAY64_LOGIN_STATE_DENIED))
    {
        accent = DISPLAY64_RGB_WARNING;
    }
    else if (state == DISPLAY64_LOGIN_STATE_UNLOCKED)
    {
        accent = DISPLAY64_RGB_APP_FILES;
        button_text = "Resume";
    }
    else if (state == DISPLAY64_LOGIN_STATE_UNAVAILABLE)
    {
        accent = DISPLAY64_RGB_DISABLED_TEXT;
        button_text = "Dismiss";
    }
    else if ((state == DISPLAY64_LOGIN_STATE_ACCEPTED) || (state == DISPLAY64_LOGIN_STATE_RECOVERY))
    {
        button_text = "Start session";
    }
#endif

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    panel_w = display64_min_u32(560u, (g_display_boot_info->framebuffer_width > 48u) ? (g_display_boot_info->framebuffer_width - 48u) : g_display_boot_info->framebuffer_width);
    panel_h = 430u;
#else
    panel_w = display64_min_u32(520u, (g_display_boot_info->framebuffer_width > 48u) ? (g_display_boot_info->framebuffer_width - 48u) : g_display_boot_info->framebuffer_width);
    panel_h = 344u;
#endif
    panel_x = (g_display_boot_info->framebuffer_width > panel_w) ? ((g_display_boot_info->framebuffer_width - panel_w) / 2u) : 0u;
    panel_y = (g_display_boot_info->framebuffer_height > panel_h) ? ((g_display_boot_info->framebuffer_height - panel_h) / 2u) : 0u;
    field_x = panel_x + 34u;
    field_w = (panel_w > 68u) ? (panel_w - 68u) : panel_w;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    username_y = panel_y + 238u;
    password_y = panel_y + 298u;
    button_y = panel_y + 350u;
#else
    username_y = panel_y + 186u;
    password_y = panel_y + 246u;
    button_y = panel_y + 286u;
#endif

    display64_compositor_fill_rect(0u, 0u, g_display_boot_info->framebuffer_width, g_display_boot_info->framebuffer_height, DISPLAY64_RGB_DESKTOP_BG);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_compositor_fill_rect(0u, 0u, g_display_boot_info->framebuffer_width, 4u, accent);
    display64_compositor_fill_rect(0u, 4u, 5u, g_display_boot_info->framebuffer_height - 4u, DISPLAY64_RGB_SURFACE_BORDER);
    display64_compositor_draw_surface(panel_x, panel_y, panel_w, panel_h, DISPLAY64_RGB_SURFACE, DISPLAY64_RGB_SURFACE_BORDER, 2u);
    display64_compositor_fill_rect(panel_x, panel_y, 5u, panel_h, accent);
    display64_compositor_draw_badge(panel_x + 34u, panel_y + 72u, 104u, "Local auth", accent);
    display64_compositor_draw_badge(panel_x + 148u, panel_y + 72u, 112u, "Scoped input", DISPLAY64_RGB_FOCUS_BLUE);
    display64_compositor_draw_badge(panel_x + 270u, panel_y + 72u, 122u, "No ambient fs", DISPLAY64_RGB_APP_SETTINGS);
    (void)display64_draw_font_text(panel_x + 32u, panel_y + 28u, "LimitlessOS", DISPLAY64_FONT_LARGE, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(panel_x + 34u, panel_y + 104u, title, DISPLAY64_FONT_NORMAL, accent, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(panel_x + 34u, panel_y + 128u, message, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_login_draw_state_row(panel_x + 34u, panel_y + 154u, "Status", "visible blocking state", accent);
    display64_login_draw_state_row(panel_x + 34u, panel_y + 184u, "Fallback", "hardware input recovery is bounded", DISPLAY64_RGB_FOCUS_BLUE);

    (void)display64_draw_font_text(field_x, panel_y + 218u, "Username", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_draw_surface(field_x, username_y, field_w, 30u, DISPLAY64_RGB_FIELD, DISPLAY64_RGB_SURFACE_BORDER, 0u);
    (void)display64_draw_font_text(field_x + 12u, username_y + 7u, "limitless", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);

    (void)display64_draw_font_text(field_x, panel_y + 278u, "Password", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_draw_surface(field_x, password_y, field_w, 30u, DISPLAY64_RGB_FIELD, accent, 0u);
    display64_compositor_fill_rect(field_x + 1u, password_y + 1u, field_w - 2u, 2u, accent);
    (void)display64_draw_font_text(field_x + 12u, password_y + 7u, "********", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);

    display64_compositor_fill_round_rect_4(field_x, button_y, field_w, 34u, accent);
    (void)display64_draw_font_text(field_x, panel_y + 398u, "Pre-auth desktop, filesystem, and network actions stay blocked.", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_MUTED, DISPLAY64_FONT_TRANSPARENT);
#else
    inset = 8u;
    red = 0x10u;
    green = 0x10u;
    blue = 0x12u;
    while ((inset < (g_display_boot_info->framebuffer_width / 2u))
        && (inset < (g_display_boot_info->framebuffer_height / 2u)))
    {
        display64_compositor_fill_rect(
            inset,
            inset,
            g_display_boot_info->framebuffer_width - (inset * 2u),
            g_display_boot_info->framebuffer_height - (inset * 2u),
            display64_rgb(red, green, blue));
        inset += 8u;
        red = (red < 0x1Eu) ? (red + 2u) : 0x1Eu;
        green = (green < 0x1Eu) ? (green + 2u) : 0x1Eu;
        blue = (blue < 0x22u) ? (blue + 2u) : 0x22u;
    }

    display64_compositor_fill_rect(panel_x, panel_y, panel_w, panel_h, DISPLAY64_RGB_SURFACE);
    display64_compositor_draw_rect(panel_x, panel_y, panel_w, panel_h, DISPLAY64_RGB_SURFACE_BORDER);
    (void)display64_draw_font_text(panel_x + 32u, panel_y + 28u, "LimitlessOS", DISPLAY64_FONT_LARGE, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(panel_x + 34u, panel_y + 74u, "M10 local console authentication", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(panel_x + 34u, panel_y + 108u, title, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_ACCENT, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(panel_x + 34u, panel_y + 132u, message, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);

    (void)display64_draw_font_text(field_x, panel_y + 166u, "Username", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_rect(field_x, username_y, field_w, 28u, DISPLAY64_RGB_FIELD);
    display64_compositor_draw_rect(field_x, username_y, field_w, 28u, DISPLAY64_RGB_SURFACE_BORDER);
    (void)display64_draw_font_text(field_x + 10u, username_y + 6u, "limitless", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);

    (void)display64_draw_font_text(field_x, panel_y + 226u, "Password", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_rect(field_x, password_y, field_w, 28u, DISPLAY64_RGB_FIELD);
    display64_compositor_draw_rect(field_x, password_y, field_w, 28u, DISPLAY64_RGB_FOCUS_BLUE);
    (void)display64_draw_font_text(field_x + 10u, password_y + 6u, "********", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);

    display64_compositor_fill_round_rect_4(field_x, button_y, field_w, 32u, DISPLAY64_RGB_ACCENT);
#endif
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    button_text_x = (field_w > display64_font_text_advance(button_text, DISPLAY64_FONT_NORMAL))
        ? (field_x + ((field_w - display64_font_text_advance(button_text, DISPLAY64_FONT_NORMAL)) / 2u))
        : field_x;
#else
    button_text_x = (field_w > display64_font_text_advance("Login", DISPLAY64_FONT_NORMAL))
        ? (field_x + ((field_w - display64_font_text_advance("Login", DISPLAY64_FONT_NORMAL)) / 2u))
        : field_x;
#endif
    button_text_y = button_y + 8u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)display64_draw_font_text(button_text_x, button_text_y, button_text, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
#else
    (void)display64_draw_font_text(button_text_x, button_text_y, "Login", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
#endif

    if (failures != 0u)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        display64_draw_label_value(field_x, panel_y + 388u, "Failed attempts ", failures, DISPLAY64_RGB_WARNING);
#else
        display64_draw_label_value(field_x, panel_y + 326u, "Failed attempts ", failures, DISPLAY64_RGB_WARNING);
#endif
    }
    if (lockout_seconds != 0u)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        display64_draw_label_value(panel_x + 260u, panel_y + 388u, "Lockout ", lockout_seconds, DISPLAY64_RGB_WARNING);
#else
        display64_draw_label_value(panel_x + 260u, panel_y + 326u, "Lockout ", lockout_seconds, DISPLAY64_RGB_WARNING);
#endif
    }

    (void)display64_compositor_present();
}

void display64_login_setup_screen(void)
{
    display64_login_screen_draw("First-run setup", "Create the initial local user", 0u, 0u);
}

static struct display64_window *display64_wm_find_window(u32 handle)
{
    u32 index;

    if (handle == 0u)
    {
        return 0;
    }

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if ((g_display_windows[index].visible != 0u)
            && (g_display_windows[index].handle == handle))
        {
            return &g_display_windows[index];
        }
    }

    return 0;
}

static int display64_point_in_rect(u32 x, u32 y, u32 rect_x, u32 rect_y, u32 rect_w, u32 rect_h)
{
    if ((rect_w == 0u) || (rect_h == 0u))
    {
        return 0;
    }

    return (x >= rect_x)
        && (y >= rect_y)
        && (x < (rect_x + rect_w))
        && (y < (rect_y + rect_h));
}

static int display64_string_equal(const char *left, const char *right)
{
    u32 index = 0u;

    if ((left == 0) || (right == 0))
    {
        return 0;
    }

    while ((left[index] != '\0') && (right[index] != '\0'))
    {
        if (left[index] != right[index])
        {
            return 0;
        }
        ++index;
    }

    return (left[index] == '\0') && (right[index] == '\0');
}

static int display64_wm_window_is_terminal(const struct display64_window *window)
{
    return (window != 0) && display64_string_equal(window->title, "Terminal");
}

static struct display64_window *display64_wm_hit_window(u32 x, u32 y)
{
    struct display64_window *best = 0;
    u32 best_z = 0u;
    u32 index;

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        struct display64_window *window = &g_display_windows[index];
        if ((window->visible != 0u)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            && (window->minimized == 0u)
#endif
            && display64_point_in_rect(x, y, window->x, window->y, window->width, window->height)
            && (window->z >= best_z))
        {
            best = window;
            best_z = window->z;
        }
    }

    return best;
}

static void display64_wm_clear_focus(void)
{
    u32 index;

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        g_display_windows[index].focused = 0u;
    }
}

static u32 display64_wm_top_visible_handle(void)
{
    u32 best_handle = 0u;
    u32 best_z = 0u;
    u32 index;

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if ((g_display_windows[index].visible != 0u)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            && (g_display_windows[index].minimized == 0u)
#endif
            && (g_display_windows[index].z >= best_z))
        {
            best_handle = g_display_windows[index].handle;
            best_z = g_display_windows[index].z;
        }
    }

    return best_handle;
}

static struct display64_window *display64_wm_focused_window(void)
{
    u32 index;

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if ((g_display_windows[index].visible != 0u)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            && (g_display_windows[index].minimized == 0u)
#endif
            && (g_display_windows[index].focused != 0u))
        {
            return &g_display_windows[index];
        }
    }

    return 0;
}

static u32 display64_wm_focused_handle(void)
{
    struct display64_window *window = display64_wm_focused_window();
    return (window != 0) ? window->handle : 0u;
}

static u32 display64_wm_window_z(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    return (window != 0) ? window->z : 0u;
}

static u32 display64_wm_has_unfocused_terminal(u32 focused_handle)
{
    u32 index;

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if ((g_display_windows[index].visible != 0u)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            && (g_display_windows[index].minimized == 0u)
#endif
            && (g_display_windows[index].handle != focused_handle)
            && display64_wm_window_is_terminal(&g_display_windows[index]))
        {
            return 1u;
        }
    }

    return 0u;
}

static void display64_gui_record_event(
    u32 x,
    u32 y,
    u32 region,
    u32 target_window,
    u32 focus_before,
    u32 focus_after,
    u32 z_before,
    u32 z_after)
{
    g_display_gui_interactive = 1u;
    g_display_gui_mouse_x = x;
    g_display_gui_mouse_y = y;
    g_display_gui_target_region = region;
    g_display_gui_target_window = target_window;
    g_display_gui_focus_before = focus_before;
    g_display_gui_focus_after = focus_after;
    g_display_gui_z_before = z_before;
    g_display_gui_z_after = z_after;
    if (region != DISPLAY64_GUI_REGION_NONE)
    {
        g_display_gui_click_hittest = 1u;
    }
}

static void display64_gui_record_unfocused_keyboard_denial(u32 focused_handle)
{
    if (display64_wm_has_unfocused_terminal(focused_handle) != 0u)
    {
        g_display_gui_unfocused_key_denied = 1u;
        ++g_display_gui_unfocused_key_denial_count;
    }
}

static u32 display64_wm_create_window(const char *title, u32 x, u32 y, u32 width, u32 height)
{
    u32 index;

    if ((g_display_compositor_active == 0u)
        || !display64_has_framebuffer()
        || (width < 96u)
        || (height < 72u))
    {
        return 0u;
    }

    if (x >= g_display_boot_info->framebuffer_width)
    {
        x = 0u;
    }
    if (y >= g_display_boot_info->framebuffer_height)
    {
        y = 0u;
    }
    width = display64_min_u32(width, g_display_boot_info->framebuffer_width - x);
    height = display64_min_u32(height, g_display_boot_info->framebuffer_height - y);

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if (g_display_windows[index].visible == 0u)
        {
            g_display_windows[index].handle = g_display_wm_next_handle++;
            g_display_windows[index].x = x;
            g_display_windows[index].y = y;
            g_display_windows[index].width = width;
            g_display_windows[index].height = height;
            g_display_windows[index].title = title;
            g_display_windows[index].visible = 1u;
            g_display_windows[index].focused = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            g_display_windows[index].minimized = 0u;
#endif
            g_display_windows[index].z = g_display_wm_next_z++;
            ++g_display_wm_window_count;
            return g_display_windows[index].handle;
        }
    }

    return 0u;
}

static void display64_wm_focus_window(u32 handle)
{
    struct display64_window *window;
    u32 index;

    window = display64_wm_find_window(handle);
    if (window == 0)
    {
        return;
    }

    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        g_display_windows[index].focused = 0u;
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (window->minimized != 0u)
    {
        window->minimized = 0u;
        ++g_display_wm_restore_count;
    }
#endif
    window->focused = 1u;
    window->z = g_display_wm_next_z++;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    ++g_display_wm_zorder_count;
#endif
    ++g_display_wm_focus_count;
}

static void display64_wm_move_window(u32 handle, u32 x, u32 y)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 max_x;
    u32 max_y;

    if ((window == 0) || !display64_has_framebuffer())
    {
        return;
    }

    display64_compositor_mark_dirty(window->x, window->y, window->width, window->height);
    max_x = (window->width < g_display_boot_info->framebuffer_width)
        ? (g_display_boot_info->framebuffer_width - window->width)
        : 0u;
    max_y = (window->height < g_display_boot_info->framebuffer_height)
        ? (g_display_boot_info->framebuffer_height - window->height)
        : 0u;
    window->x = display64_min_u32(x, max_x);
    window->y = display64_min_u32(y, max_y);
    display64_compositor_mark_dirty(window->x, window->y, window->width, window->height);
}

static void display64_wm_configure_console(struct display64_window *window)
{
    u32 content_x;
    u32 content_y;
    u32 content_w;
    u32 content_h;

    if (window == 0)
    {
        return;
    }

    content_x = window->x + 8u;
    content_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 8u;
    content_w = (window->width > 16u) ? (window->width - 16u) : 0u;
    content_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + 16u))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - 16u)
        : 0u;
    g_display_console_x = content_x;
    g_display_console_y = content_y;
    g_display_console_w = content_w;
    g_display_console_h = content_h;
    g_display_text_x = g_display_console_x;
    g_display_text_y = g_display_console_y;
    g_display_console_line_dirty = 0u;
}

static void display64_wm_focus_and_route_console(u32 handle)
{
    struct display64_window *window;

    display64_wm_focus_window(handle);
    window = display64_wm_find_window(handle);
    if (display64_wm_window_is_terminal(window))
    {
        g_display_wm_shell_handle = handle;
        display64_wm_configure_console(window);
    }
}

static void display64_wm_destroy_window(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 replacement;

    if (window == 0)
    {
        return;
    }

    display64_compositor_mark_dirty(window->x, window->y, window->width, window->height);
    window->visible = 0u;
    window->focused = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    window->minimized = 0u;
#endif
    if (handle == g_display_desktop_fileman_handle)
    {
        g_display_desktop_fileman_handle = 0u;
    }
    if (handle == g_display_desktop_settings_handle)
    {
        g_display_desktop_settings_handle = 0u;
    }
    if (handle == g_display_desktop_installer_handle)
    {
        g_display_desktop_installer_handle = 0u;
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (handle == g_display_desktop_assistant_handle)
    {
        g_display_desktop_assistant_handle = 0u;
    }
#endif
    if (handle == g_display_wm_shell_handle)
    {
        g_display_wm_shell_handle = 0u;
    }

    replacement = display64_wm_top_visible_handle();
    if (replacement != 0u)
    {
        display64_wm_focus_and_route_console(replacement);
    }
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void display64_wm_minimize_window(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 replacement;

    if (window == 0)
    {
        return;
    }

    display64_compositor_mark_dirty(window->x, window->y, window->width, window->height);
    window->minimized = 1u;
    window->focused = 0u;
    ++g_display_wm_minimize_count;
    if (handle == g_display_wm_shell_handle)
    {
        g_display_wm_shell_handle = 0u;
    }

    replacement = display64_wm_top_visible_handle();
    if (replacement != 0u)
    {
        display64_wm_focus_and_route_console(replacement);
    }
}

static void display64_wm_resize_window(u32 handle, u32 width, u32 height)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 max_w;
    u32 max_h;

    if ((window == 0) || !display64_has_framebuffer())
    {
        return;
    }

    display64_compositor_mark_dirty(window->x, window->y, window->width, window->height);
    max_w = (g_display_boot_info->framebuffer_width > window->x)
        ? (g_display_boot_info->framebuffer_width - window->x)
        : DISPLAY64_WM_MIN_WINDOW_WIDTH;
    max_h = (g_display_boot_info->framebuffer_height > window->y)
        ? (g_display_boot_info->framebuffer_height - window->y)
        : DISPLAY64_WM_MIN_WINDOW_HEIGHT;
    if (width < DISPLAY64_WM_MIN_WINDOW_WIDTH)
    {
        width = DISPLAY64_WM_MIN_WINDOW_WIDTH;
    }
    if (height < DISPLAY64_WM_MIN_WINDOW_HEIGHT)
    {
        height = DISPLAY64_WM_MIN_WINDOW_HEIGHT;
    }
    window->width = display64_min_u32(width, max_w);
    window->height = display64_min_u32(height, max_h);
    display64_compositor_mark_dirty(window->x, window->y, window->width, window->height);
    if (display64_wm_window_is_terminal(window))
    {
        g_display_wm_shell_handle = window->handle;
        display64_wm_configure_console(window);
    }
    ++g_display_wm_resize_count;
}

static int display64_wm_resize_hit(const struct display64_window *window, u32 x, u32 y)
{
    if ((window == 0) || (window->minimized != 0u))
    {
        return 0;
    }
    if ((window->width <= DISPLAY64_WM_RESIZE_GRIP)
        || (window->height <= DISPLAY64_WM_RESIZE_GRIP))
    {
        return 0;
    }

    return display64_point_in_rect(
        x,
        y,
        window->x + window->width - DISPLAY64_WM_RESIZE_GRIP,
        window->y + window->height - DISPLAY64_WM_RESIZE_GRIP,
        DISPLAY64_WM_RESIZE_GRIP,
        DISPLAY64_WM_RESIZE_GRIP);
}
#endif

static void display64_wm_present_window(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 title_rgb;
    u32 close_x;
    u32 close_y;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 minimize_x;
#endif
    u32 content_y;
    u32 content_h;

    if ((window == 0)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        || (window->minimized != 0u)
#endif
        || (g_display_compositor_active == 0u)
        || !display64_has_framebuffer())
    {
        return;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    title_rgb = (window->focused != 0u) ? DISPLAY64_RGB_SURFACE_HIGH : DISPLAY64_RGB_TITLE_UNFOCUSED;
    display64_compositor_draw_surface(window->x, window->y, window->width, window->height, DISPLAY64_RGB_SURFACE, DISPLAY64_RGB_SURFACE_BORDER, (window->focused != 0u) ? 2u : 1u);
    display64_compositor_fill_rect(
        window->x + DISPLAY64_WM_BORDER,
        window->y + DISPLAY64_WM_BORDER,
        window->width - (DISPLAY64_WM_BORDER * 2u),
        DISPLAY64_WM_TITLE_HEIGHT - DISPLAY64_WM_BORDER,
        title_rgb);
    if (window->focused != 0u)
    {
        display64_compositor_fill_rect(window->x + 8u, window->y + 1u, window->width - 16u, 2u, DISPLAY64_RGB_ACCENT);
    }
    display64_compositor_fill_rect(
        window->x + DISPLAY64_WM_BORDER,
        window->y + DISPLAY64_WM_TITLE_HEIGHT,
        window->width - (DISPLAY64_WM_BORDER * 2u),
        1u,
        DISPLAY64_RGB_SURFACE_BORDER);
    content_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + DISPLAY64_WM_BORDER;
    content_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + (DISPLAY64_WM_BORDER * 2u)))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - (DISPLAY64_WM_BORDER * 2u))
        : 0u;
    display64_compositor_fill_rect(
        window->x + DISPLAY64_WM_BORDER,
        content_y,
        window->width - (DISPLAY64_WM_BORDER * 2u),
        content_h,
        DISPLAY64_RGB_CONTENT);
    (void)display64_draw_font_text(
        window->x + 12u,
        window->y + ((DISPLAY64_WM_TITLE_HEIGHT - display64_font_height(DISPLAY64_FONT_NORMAL)) / 2u),
        window->title,
        DISPLAY64_FONT_NORMAL,
        DISPLAY64_RGB_TEXT_PRIMARY,
        DISPLAY64_FONT_TRANSPARENT);
    close_x = (window->width > 32u) ? (window->x + window->width - 24u) : window->x;
    close_y = window->y + ((DISPLAY64_WM_TITLE_HEIGHT - 16u) / 2u);
    minimize_x = (close_x > 24u) ? (close_x - 22u) : close_x;
    display64_compositor_fill_circle_16(minimize_x, close_y, DISPLAY64_RGB_SURFACE_BORDER);
    display64_compositor_fill_rect(minimize_x + 5u, close_y + 10u, 6u, 2u, DISPLAY64_RGB_TEXT_SECONDARY);
    display64_compositor_fill_circle_16(close_x, close_y, DISPLAY64_RGB_SURFACE_BORDER);
    (void)display64_draw_font_text(close_x + 5u, close_y + 4u, "X", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_CLOSE, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_rect(window->x + window->width - 12u, window->y + window->height - 4u, 8u, 1u, DISPLAY64_RGB_SURFACE_BORDER_STRONG);
    display64_compositor_fill_rect(window->x + window->width - 8u, window->y + window->height - 8u, 4u, 1u, DISPLAY64_RGB_SURFACE_BORDER_STRONG);
#else
    title_rgb = (window->focused != 0u) ? DISPLAY64_RGB_ACCENT : DISPLAY64_RGB_TITLE_UNFOCUSED;
    display64_compositor_fill_rect(window->x, window->y, window->width, window->height, DISPLAY64_RGB_SURFACE);
    display64_compositor_draw_rect(window->x, window->y, window->width, window->height, DISPLAY64_RGB_SURFACE_BORDER);
    display64_compositor_fill_rect(
        window->x + DISPLAY64_WM_BORDER,
        window->y + DISPLAY64_WM_BORDER,
        window->width - (DISPLAY64_WM_BORDER * 2u),
        DISPLAY64_WM_TITLE_HEIGHT - DISPLAY64_WM_BORDER,
        title_rgb);
    display64_compositor_fill_rect(
        window->x + DISPLAY64_WM_BORDER,
        window->y + DISPLAY64_WM_TITLE_HEIGHT,
        window->width - (DISPLAY64_WM_BORDER * 2u),
        1u,
        DISPLAY64_RGB_SURFACE_BORDER);
    content_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + DISPLAY64_WM_BORDER;
    content_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + (DISPLAY64_WM_BORDER * 2u)))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - (DISPLAY64_WM_BORDER * 2u))
        : 0u;
    display64_compositor_fill_rect(
        window->x + DISPLAY64_WM_BORDER,
        content_y,
        window->width - (DISPLAY64_WM_BORDER * 2u),
        content_h,
        DISPLAY64_RGB_CONTENT);
    (void)display64_draw_font_text(
        window->x + 10u,
        window->y + ((DISPLAY64_WM_TITLE_HEIGHT - display64_font_height(DISPLAY64_FONT_NORMAL)) / 2u),
        window->title,
        DISPLAY64_FONT_NORMAL,
        (window->focused != 0u) ? DISPLAY64_RGB_TEXT_ON_ACCENT : DISPLAY64_RGB_TEXT_PRIMARY,
        DISPLAY64_FONT_TRANSPARENT);
    close_x = (window->width > 32u) ? (window->x + window->width - 24u) : window->x;
    close_y = window->y + ((DISPLAY64_WM_TITLE_HEIGHT - 16u) / 2u);
    display64_compositor_fill_circle_16(close_x, close_y, DISPLAY64_RGB_CLOSE);
    (void)display64_draw_font_text(close_x + 5u, close_y + 4u, "X", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
#endif
    ++g_display_wm_present_count;
}

void display64_wm_probe(void)
{
    u32 width;
    u32 height;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 work_right;
    u32 work_bottom;
#endif
    struct display64_window *window;

    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        g_display_wm_active = 0u;
        return;
    }

    display64_compositor_fill_rect(0u, 0u, g_display_boot_info->framebuffer_width, g_display_boot_info->framebuffer_height, DISPLAY64_RGB_DESKTOP_BG);
    display64_font_draw_status_bar();
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_boot_info->framebuffer_width > 1200u)
    {
        work_right = g_display_boot_info->framebuffer_width - 416u;
        width = (work_right > 64u) ? (work_right - 32u) : work_right;
    }
    else
    {
        width = (g_display_boot_info->framebuffer_width > 64u)
            ? (g_display_boot_info->framebuffer_width - 64u)
            : g_display_boot_info->framebuffer_width;
    }
    if (width > 1480u)
    {
        width = 1480u;
    }
    work_bottom = (g_display_boot_info->framebuffer_height > (DISPLAY64_STATUS_BAR_HEIGHT + DISPLAY64_DESKTOP_TASKBAR_HEIGHT + 20u))
        ? (g_display_boot_info->framebuffer_height - DISPLAY64_STATUS_BAR_HEIGHT - DISPLAY64_DESKTOP_TASKBAR_HEIGHT - 20u)
        : g_display_boot_info->framebuffer_height;
    height = (work_bottom > 56u) ? (work_bottom - 56u) : work_bottom;
    if (height < 320u)
    {
        height = display64_min_u32(320u, g_display_boot_info->framebuffer_height);
    }
#else
    width = display64_min_u32(920u, (g_display_boot_info->framebuffer_width > 64u) ? (g_display_boot_info->framebuffer_width - 64u) : g_display_boot_info->framebuffer_width);
    height = display64_min_u32(560u, (g_display_boot_info->framebuffer_height > 112u) ? (g_display_boot_info->framebuffer_height - 112u) : g_display_boot_info->framebuffer_height);
#endif
    g_display_wm_shell_handle = display64_wm_create_window("Terminal", 32u, 56u, width, height);
    display64_wm_focus_window(g_display_wm_shell_handle);
    display64_wm_present_window(g_display_wm_shell_handle);
    window = display64_wm_find_window(g_display_wm_shell_handle);
    display64_wm_configure_console(window);
    display64_wm_move_window(g_display_wm_shell_handle, 32u, 56u);
    display64_wm_present_window(g_display_wm_shell_handle);
    g_display_wm_active = (g_display_wm_shell_handle != 0u) ? 1u : 0u;
    (void)display64_compositor_present();
}

static u32 display64_desktop_taskbar_y(void)
{
    if (!display64_has_framebuffer())
    {
        return 0u;
    }

    if (g_display_boot_info->framebuffer_height
        > (DISPLAY64_STATUS_BAR_HEIGHT + DISPLAY64_DESKTOP_TASKBAR_HEIGHT))
    {
        return g_display_boot_info->framebuffer_height
            - DISPLAY64_STATUS_BAR_HEIGHT
            - DISPLAY64_DESKTOP_TASKBAR_HEIGHT;
    }

    return 0u;
}

static void display64_desktop_draw_launcher_button(u32 x, u32 y)
{
    u32 row;
    u32 column;
    u32 active;
    u32 bg_rgb;
    u32 dot_rgb;

    active = ((g_display_desktop_launcher_open != 0u)
        || display64_point_in_rect(
            g_display_compositor_cursor_x,
            g_display_compositor_cursor_y,
            x,
            y,
            DISPLAY64_DESKTOP_LAUNCHER_BUTTON_WIDTH,
            DISPLAY64_DESKTOP_LAUNCHER_BUTTON_HEIGHT))
        ? 1u
        : 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    bg_rgb = (active != 0u) ? DISPLAY64_RGB_SURFACE_HIGH : DISPLAY64_RGB_TASK_BUTTON;
    dot_rgb = (active != 0u) ? DISPLAY64_RGB_ACCENT : DISPLAY64_RGB_TEXT_SECONDARY;
    display64_compositor_draw_surface(x, y, 28u, 24u, bg_rgb, DISPLAY64_RGB_TASK_BUTTON_BORDER, 0u);
#else
    bg_rgb = (active != 0u) ? DISPLAY64_RGB_ACCENT : DISPLAY64_RGB_TASK_BUTTON;
    dot_rgb = (active != 0u) ? DISPLAY64_RGB_TEXT_ON_ACCENT : DISPLAY64_RGB_TEXT_PRIMARY;
    display64_compositor_fill_round_rect_4(x, y, 28u, 24u, DISPLAY64_RGB_TASK_BUTTON_BORDER);
    display64_compositor_fill_round_rect_4(x + 1u, y + 1u, 26u, 22u, bg_rgb);
#endif
    for (row = 0u; row < 3u; ++row)
    {
        for (column = 0u; column < 3u; ++column)
        {
            display64_compositor_fill_rect(
                x + 7u + (column * 6u),
                y + 5u + (row * 6u),
                3u,
                3u,
                dot_rgb);
        }
    }
}

static void display64_desktop_draw_window_button(u32 x, u32 y, const char *title, u32 active)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 bg_rgb = (active != 0u) ? DISPLAY64_RGB_SURFACE_HIGH : DISPLAY64_RGB_TASK_BUTTON;
    u32 text_rgb = (active != 0u) ? DISPLAY64_RGB_TEXT_PRIMARY : DISPLAY64_RGB_TEXT_SECONDARY;

    display64_compositor_draw_surface(
        x,
        y,
        DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH,
        DISPLAY64_DESKTOP_WINDOW_BUTTON_HEIGHT,
        bg_rgb,
        DISPLAY64_RGB_TASK_BUTTON_BORDER,
        0u);
    if (active != 0u)
    {
        display64_compositor_fill_rect(x + 8u, y + DISPLAY64_DESKTOP_WINDOW_BUTTON_HEIGHT - 3u, DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH - 16u, 2u, DISPLAY64_RGB_ACCENT);
    }
#else
    u32 bg_rgb = (active != 0u) ? DISPLAY64_RGB_ACCENT : DISPLAY64_RGB_TASK_BUTTON;
    u32 text_rgb = (active != 0u) ? DISPLAY64_RGB_TEXT_ON_ACCENT : DISPLAY64_RGB_TEXT_PRIMARY;

    display64_compositor_fill_round_rect_4(
        x,
        y,
        DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH,
        DISPLAY64_DESKTOP_WINDOW_BUTTON_HEIGHT,
        DISPLAY64_RGB_TASK_BUTTON_BORDER);
    display64_compositor_fill_round_rect_4(
        x + 1u,
        y + 1u,
        DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH - 2u,
        DISPLAY64_DESKTOP_WINDOW_BUTTON_HEIGHT - 2u,
        bg_rgb);
#endif
    (void)display64_draw_font_text(
        x + 8u,
        y + ((DISPLAY64_DESKTOP_WINDOW_BUTTON_HEIGHT - display64_font_height(DISPLAY64_FONT_NORMAL)) / 2u),
        title,
        DISPLAY64_FONT_NORMAL,
        text_rgb,
        DISPLAY64_FONT_TRANSPARENT);
}

static void display64_desktop_draw_taskbar(void)
{
    u32 y = display64_desktop_taskbar_y();
    u32 clock_x;
    u32 button_x = DISPLAY64_DESKTOP_WINDOW_BUTTON_X;
    u32 index;
    u32 uptime;
    u32 clock_text_x;
    u32 clock_text_y;
    u32 clock_text_w;
    char uptime_text[12];

    if (!display64_has_framebuffer())
    {
        return;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, DISPLAY64_DESKTOP_TASKBAR_HEIGHT, DISPLAY64_RGB_BAR_BG);
    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, 1u, DISPLAY64_RGB_HIGHLIGHT);
#else
    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, DISPLAY64_DESKTOP_TASKBAR_HEIGHT, DISPLAY64_RGB_BAR_BG);
    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, 1u, DISPLAY64_RGB_SURFACE_BORDER);
#endif
    display64_desktop_draw_launcher_button(DISPLAY64_DESKTOP_LAUNCHER_BUTTON_X, y + 4u);
    clock_x = (g_display_boot_info->framebuffer_width > 96u)
        ? (g_display_boot_info->framebuffer_width - 96u)
        : 0u;
    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if ((g_display_windows[index].visible != 0u)
            && ((button_x + DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH + 8u) < clock_x))
        {
            display64_desktop_draw_window_button(
                button_x,
                y + 4u,
                g_display_windows[index].title,
                g_display_windows[index].focused);
            button_x += DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH + DISPLAY64_DESKTOP_WINDOW_BUTTON_GAP;
        }
    }
    uptime = pit_get_uptime_seconds();
    display64_u32_to_dec_text(uptime, uptime_text, (u32)sizeof(uptime_text));
    clock_text_w = display64_font_text_advance("T+", DISPLAY64_FONT_NORMAL)
        + display64_font_text_advance(uptime_text, DISPLAY64_FONT_NORMAL);
    clock_text_x = (g_display_boot_info->framebuffer_width > (clock_text_w + 12u))
        ? (g_display_boot_info->framebuffer_width - clock_text_w - 12u)
        : clock_x;
    clock_text_y = y + ((DISPLAY64_DESKTOP_TASKBAR_HEIGHT - display64_font_height(DISPLAY64_FONT_NORMAL)) / 2u);
    (void)display64_draw_font_text(clock_text_x, clock_text_y, "T+", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(
        clock_text_x + display64_font_text_advance("T+", DISPLAY64_FONT_NORMAL),
        clock_text_y,
        uptime_text,
        DISPLAY64_FONT_NORMAL,
        DISPLAY64_RGB_TEXT_SECONDARY,
        DISPLAY64_FONT_TRANSPARENT);
    if (g_display_desktop_taskbar_count == 0u)
    {
        ++g_display_desktop_taskbar_count;
    }
}

static u32 display64_desktop_launcher_panel_y(void)
{
    u32 y = display64_desktop_taskbar_y();

    return (y > (DISPLAY64_DESKTOP_LAUNCHER_HEIGHT + 8u))
        ? (y - DISPLAY64_DESKTOP_LAUNCHER_HEIGHT - 8u)
        : 40u;
}

static void display64_desktop_draw_launcher_entry(
    u32 row_x,
    u32 row_y,
    u32 row_w,
    u32 row_h,
    u32 icon_rgb,
    u32 icon_kind,
    const char *letter,
    const char *name)
{
    u32 icon_x = row_x + 4u;
    u32 icon_y = row_y + 4u;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)letter;
#else
    (void)icon_kind;
#endif

    if (display64_point_in_rect(
            g_display_compositor_cursor_x,
            g_display_compositor_cursor_y,
            row_x,
            row_y,
            row_w,
            row_h))
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        display64_compositor_fill_round_rect_4(row_x, row_y, row_w, row_h, DISPLAY64_RGB_POPOVER_HOVER);
#else
        display64_compositor_fill_rect(row_x, row_y, row_w, row_h, DISPLAY64_RGB_POPOVER_HOVER);
#endif
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_desktop_draw_icon_glyph(icon_x, icon_y, icon_kind, icon_rgb);
#else
    display64_compositor_fill_rect(icon_x, icon_y, 24u, 24u, icon_rgb);
    (void)display64_draw_font_text(icon_x + 8u, icon_y + 4u, letter, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
#endif
    (void)display64_draw_font_text(row_x + 36u, row_y + 8u, name, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
}

static void display64_desktop_draw_launcher_panel(void)
{
    u32 panel_y = display64_desktop_launcher_panel_y();

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_compositor_draw_surface(12u, panel_y, DISPLAY64_DESKTOP_LAUNCHER_WIDTH, DISPLAY64_DESKTOP_LAUNCHER_HEIGHT, DISPLAY64_RGB_SURFACE, DISPLAY64_RGB_SURFACE_BORDER, 2u);
#else
    display64_compositor_fill_rect(12u, panel_y, DISPLAY64_DESKTOP_LAUNCHER_WIDTH, DISPLAY64_DESKTOP_LAUNCHER_HEIGHT, DISPLAY64_RGB_SURFACE);
    display64_compositor_draw_rect(12u, panel_y, DISPLAY64_DESKTOP_LAUNCHER_WIDTH, DISPLAY64_DESKTOP_LAUNCHER_HEIGHT, DISPLAY64_RGB_SURFACE_BORDER);
#endif
    (void)display64_draw_font_text(20u, panel_y + 10u, "Apps", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_desktop_draw_launcher_entry(24u, panel_y + 36u, 92u, 32u, DISPLAY64_RGB_APP_TERMINAL, 0u, "T", "Terminal");
    display64_desktop_draw_launcher_entry(24u, panel_y + 72u, 92u, 32u, DISPLAY64_RGB_APP_FILES, 1u, "F", "Files");
    display64_desktop_draw_launcher_entry(120u, panel_y + 36u, 96u, 32u, DISPLAY64_RGB_APP_SETTINGS, 2u, "S", "Settings");
    display64_desktop_draw_launcher_entry(120u, panel_y + 72u, 96u, 32u, DISPLAY64_RGB_APP_INSTALLER, 3u, "I", "Installer");
    display64_desktop_draw_launcher_entry(24u, panel_y + 108u, 192u, 32u, DISPLAY64_RGB_APP_ASSISTANT, 4u, "A", "Assistant");
    if (g_display_desktop_launcher_count == 0u)
    {
        ++g_display_desktop_launcher_count;
    }
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void display64_fileman_zero(void *address, u32 byte_count)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static u32 display64_fileman_copy_cstr(u8 *destination, u32 capacity, const char *source)
{
    u32 index = 0u;

    if ((destination == (u8 *)0) || (capacity == 0u) || (source == (const char *)0))
    {
        return 0u;
    }

    while ((index + 1u) < capacity && source[index] != '\0')
    {
        destination[index] = (u8)source[index];
        ++index;
    }
    destination[index] = 0u;
    return index;
}

static u32 display64_fileman_cstr_bytes(const u8 *text, u32 capacity)
{
    u32 index = 0u;

    if (text == (const u8 *)0)
    {
        return 0u;
    }

    while ((index < capacity) && (text[index] != 0u))
    {
        ++index;
    }

    return index;
}

static void display64_fileman_clear_delete_confirm(void)
{
    g_display_fileman_delete_armed = 0u;
    display64_fileman_zero(g_display_fileman_delete_path, sizeof(g_display_fileman_delete_path));
}

static void display64_fileman_clear_edit(void)
{
    g_display_fileman_edit_mode = 0u;
    g_display_fileman_edit_bytes = 0u;
    display64_fileman_zero(g_display_fileman_edit_buffer, sizeof(g_display_fileman_edit_buffer));
}

static u32 display64_fileman_copy_bytes(u8 *destination, u32 capacity, const u8 *source, u32 source_bytes)
{
    u32 index;

    if ((destination == (u8 *)0) || (source == (const u8 *)0) || (capacity == 0u))
    {
        return 0u;
    }

    display64_fileman_zero(destination, capacity);
    for (index = 0u; (index < source_bytes) && (index < (capacity - 1u)); ++index)
    {
        destination[index] = source[index];
    }
    destination[index] = 0u;
    return index;
}

static u32 display64_fileman_path_matches(const u8 *left, const u8 *right, u32 capacity)
{
    u32 index;

    if ((left == (const u8 *)0) || (right == (const u8 *)0))
    {
        return 0u;
    }
    for (index = 0u; index < capacity; ++index)
    {
        if (left[index] != right[index])
        {
            return 0u;
        }
        if (left[index] == 0u)
        {
            return 1u;
        }
    }
    return 1u;
}

static void display64_desktop_redraw(void);

static void display64_fileman_begin_edit(u32 mode, const u8 *default_text, u32 default_bytes)
{
    g_display_fileman_edit_mode = mode;
    g_display_fileman_last_mutation_status = 7u;
    g_display_fileman_edit_bytes = display64_fileman_copy_bytes(
        g_display_fileman_edit_buffer,
        sizeof(g_display_fileman_edit_buffer),
        default_text,
        default_bytes);
    display64_fileman_clear_delete_confirm();
    ++g_display_fileman_backend_edit_count;
}

static void display64_fileman_set_path(const char *path)
{
    (void)display64_fileman_copy_cstr(g_display_fileman_current_path, sizeof(g_display_fileman_current_path), path);
    g_display_fileman_selected_index = 0u;
    g_display_fileman_window_cursor = 0u;
    g_display_fileman_preview_bytes = 0u;
    g_display_fileman_preview_size = 0u;
    g_display_fileman_preview[0] = 0u;
    display64_fileman_clear_delete_confirm();
    display64_fileman_clear_edit();
}

static void display64_fileman_ensure_path(void)
{
    if (g_display_fileman_current_path[0] == 0u)
    {
        display64_fileman_set_path("/APPS");
    }
}

static u32 display64_fileman_build_child_path(u8 *destination, u32 capacity, const mmio64_nvme_fat_dirent_t *entry)
{
    u32 path_bytes;
    u32 name_index;
    u32 cursor = 0u;

    if ((destination == (u8 *)0) || (capacity < 2u) || (entry == (const mmio64_nvme_fat_dirent_t *)0))
    {
        return 0u;
    }

    path_bytes = display64_fileman_cstr_bytes(g_display_fileman_current_path, sizeof(g_display_fileman_current_path));
    if (path_bytes == 0u)
    {
        return 0u;
    }

    display64_fileman_zero(destination, capacity);
    while ((cursor < path_bytes) && ((cursor + 1u) < capacity))
    {
        destination[cursor] = g_display_fileman_current_path[cursor];
        ++cursor;
    }

    if ((cursor > 1u) && (destination[cursor - 1u] != (u8)'/'))
    {
        if ((cursor + 1u) >= capacity)
        {
            return 0u;
        }
        destination[cursor] = (u8)'/';
        ++cursor;
    }

    for (name_index = 0u;
         (name_index < entry->name_byte_count)
            && (name_index < MMIO64_NVME_FAT_DIRENT_NAME_MAX)
            && ((cursor + 1u) < capacity);
         ++name_index)
    {
        destination[cursor] = entry->name[name_index];
        ++cursor;
    }

    destination[cursor] = 0u;
    return cursor;
}

static void display64_fileman_parent_path(void)
{
    u32 path_bytes;

    display64_fileman_ensure_path();
    path_bytes = display64_fileman_cstr_bytes(g_display_fileman_current_path, sizeof(g_display_fileman_current_path));
    if (path_bytes <= 1u)
    {
        display64_fileman_set_path("/");
        return;
    }

    while ((path_bytes > 1u) && (g_display_fileman_current_path[path_bytes - 1u] == (u8)'/'))
    {
        --path_bytes;
    }
    while ((path_bytes > 1u) && (g_display_fileman_current_path[path_bytes - 1u] != (u8)'/'))
    {
        --path_bytes;
    }
    if (path_bytes <= 1u)
    {
        display64_fileman_set_path("/");
        return;
    }

    g_display_fileman_current_path[path_bytes - 1u] = 0u;
    g_display_fileman_selected_index = 0u;
    g_display_fileman_window_cursor = 0u;
    g_display_fileman_preview_bytes = 0u;
    g_display_fileman_preview_size = 0u;
    g_display_fileman_preview[0] = 0u;
}

static const char *display64_fileman_type_text(u32 entry_type)
{
    if (entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY)
    {
        return "directory";
    }
    if (entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
    {
        return "file";
    }

    return "entry";
}

static u32 display64_fileman_find_previous_cursor(u32 target_cursor)
{
    mmio64_nvme_fat_dirent_t entry;
    u32 cursor = 0u;
    u32 previous = 0u;
    u32 result;
    u32 guard;

    if (target_cursor == 0u)
    {
        return 0u;
    }

    for (guard = 0u; guard < 128u; ++guard)
    {
        result = mmio64_nvme_fat_shell_read_dirent(
            g_display_fileman_current_path,
            display64_fileman_cstr_bytes(g_display_fileman_current_path, sizeof(g_display_fileman_current_path)),
            cursor,
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            &entry);
        if (result != MMIO64_NVME_FAT_READDIR_OK)
        {
            return 0u;
        }
        if (entry.next_cursor == target_cursor)
        {
            return cursor;
        }
        if ((entry.next_cursor == 0u) || (entry.next_cursor == cursor))
        {
            return previous;
        }
        previous = cursor;
        cursor = entry.next_cursor;
    }

    return 0u;
}

static void display64_fileman_refresh(void)
{
    u32 cursor;
    u32 index;
    u32 result;

    display64_fileman_ensure_path();
    display64_fileman_zero(g_display_fileman_entries, sizeof(g_display_fileman_entries));
    g_display_fileman_entry_count = 0u;
    g_display_fileman_last_status = 0u;

    if ((mmio64_nvme_fat_located() == 0u) || (mmio64_nvme_fat_unavailable() != 0u) || (mmio64_nvme_fat_error() != 0u))
    {
        g_display_fileman_last_status = 1u;
        return;
    }

    cursor = g_display_fileman_window_cursor;
    for (index = 0u; index < DISPLAY64_FILEMAN_MAX_ENTRIES; ++index)
    {
        result = mmio64_nvme_fat_shell_read_dirent(
            g_display_fileman_current_path,
            display64_fileman_cstr_bytes(g_display_fileman_current_path, sizeof(g_display_fileman_current_path)),
            cursor,
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            &g_display_fileman_entries[index]);
        if (result == MMIO64_NVME_FAT_READDIR_EOF)
        {
            if ((index == 0u) && (g_display_fileman_window_cursor != 0u))
            {
                g_display_fileman_window_cursor = 0u;
                cursor = 0u;
                --index;
                continue;
            }
            break;
        }
        if (result != MMIO64_NVME_FAT_READDIR_OK)
        {
            g_display_fileman_last_status = 2u;
            break;
        }
        ++g_display_fileman_entry_count;
        cursor = g_display_fileman_entries[index].next_cursor;
        if (cursor == 0u)
        {
            break;
        }
    }

    if (g_display_fileman_selected_index >= g_display_fileman_entry_count)
    {
        g_display_fileman_selected_index = 0u;
    }

    ++g_display_fileman_backend_refresh_count;
}

static u32 display64_fileman_loaded_next_cursor(void)
{
    if (g_display_fileman_entry_count == 0u)
    {
        return 0u;
    }

    return g_display_fileman_entries[g_display_fileman_entry_count - 1u].next_cursor;
}

static void display64_fileman_preview_selected(void)
{
    u8 path[DISPLAY64_FILEMAN_PATH_BYTES];
    mmio64_nvme_fat_dirent_t *entry;
    u32 path_bytes;
    u32 index;

    g_display_fileman_preview_bytes = 0u;
    g_display_fileman_preview_size = 0u;
    g_display_fileman_preview[0] = 0u;
    if (g_display_fileman_selected_index >= g_display_fileman_entry_count)
    {
        return;
    }

    entry = &g_display_fileman_entries[g_display_fileman_selected_index];
    if ((entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
        && (entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY))
    {
        return;
    }

    path_bytes = display64_fileman_build_child_path(path, sizeof(path), entry);
    if (path_bytes == 0u)
    {
        return;
    }

    if (mmio64_nvme_fat_shell_read_file_range(
            path,
            path_bytes,
            0u,
            g_display_fileman_preview,
            DISPLAY64_FILEMAN_PREVIEW_BYTES,
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            &g_display_fileman_preview_bytes,
            &g_display_fileman_preview_size) == 0u)
    {
        g_display_fileman_preview_bytes = 0u;
        g_display_fileman_preview_size = 0u;
        g_display_fileman_preview[0] = 0u;
        return;
    }

    for (index = 0u; index < g_display_fileman_preview_bytes; ++index)
    {
        if ((g_display_fileman_preview[index] < 0x20u) || (g_display_fileman_preview[index] > 0x7Eu))
        {
            g_display_fileman_preview[index] = (u8)' ';
        }
    }
    g_display_fileman_preview[g_display_fileman_preview_bytes] = 0u;
    ++g_display_fileman_backend_preview_count;
}

static void display64_fileman_open_selected_if_directory(void)
{
    u8 path[DISPLAY64_FILEMAN_PATH_BYTES];
    mmio64_nvme_fat_dirent_t *entry;
    u32 path_bytes;

    if (g_display_fileman_selected_index >= g_display_fileman_entry_count)
    {
        return;
    }

    entry = &g_display_fileman_entries[g_display_fileman_selected_index];
    if (entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY)
    {
        display64_fileman_preview_selected();
        return;
    }

    path_bytes = display64_fileman_build_child_path(path, sizeof(path), entry);
    if (path_bytes == 0u)
    {
        return;
    }

    display64_fileman_zero(g_display_fileman_current_path, sizeof(g_display_fileman_current_path));
    for (g_display_fileman_selected_index = 0u;
         (g_display_fileman_selected_index < path_bytes) && (g_display_fileman_selected_index < (sizeof(g_display_fileman_current_path) - 1u));
         ++g_display_fileman_selected_index)
    {
        g_display_fileman_current_path[g_display_fileman_selected_index] = path[g_display_fileman_selected_index];
    }
    g_display_fileman_current_path[g_display_fileman_selected_index] = 0u;
    g_display_fileman_selected_index = 0u;
    g_display_fileman_window_cursor = 0u;
    g_display_fileman_preview_bytes = 0u;
    g_display_fileman_preview_size = 0u;
    g_display_fileman_preview[0] = 0u;
    ++g_display_fileman_backend_open_dir_count;
    display64_fileman_refresh();
}

static u32 display64_fileman_build_current_named_path(
    u8 *destination,
    u32 capacity,
    const u8 *name,
    u32 name_bytes)
{
    u32 path_bytes;
    u32 cursor = 0u;
    u32 name_index;

    if ((destination == (u8 *)0)
        || (name == (const u8 *)0)
        || (name_bytes == 0u)
        || (capacity < (name_bytes + 2u)))
    {
        return 0u;
    }

    display64_fileman_ensure_path();
    path_bytes = display64_fileman_cstr_bytes(g_display_fileman_current_path, sizeof(g_display_fileman_current_path));
    if (path_bytes == 0u)
    {
        return 0u;
    }

    display64_fileman_zero(destination, capacity);
    while ((cursor < path_bytes) && ((cursor + 1u) < capacity))
    {
        destination[cursor] = g_display_fileman_current_path[cursor];
        ++cursor;
    }
    if ((cursor > 1u) && (destination[cursor - 1u] != (u8)'/'))
    {
        if ((cursor + 1u) >= capacity)
        {
            return 0u;
        }
        destination[cursor++] = (u8)'/';
    }
    for (name_index = 0u; name_index < name_bytes; ++name_index)
    {
        if ((cursor + 1u) >= capacity)
        {
            return 0u;
        }
        destination[cursor++] = name[name_index];
    }
    destination[cursor] = 0u;
    return cursor;
}

static u32 display64_fileman_build_edit_target_path(u8 *destination, u32 capacity)
{
    u32 index;

    if ((destination == (u8 *)0)
        || (capacity == 0u)
        || (g_display_fileman_edit_bytes == 0u))
    {
        return 0u;
    }
    if (g_display_fileman_edit_buffer[0] == (u8)'/')
    {
        display64_fileman_zero(destination, capacity);
        for (index = 0u;
             (index < g_display_fileman_edit_bytes) && ((index + 1u) < capacity);
             ++index)
        {
            destination[index] = g_display_fileman_edit_buffer[index];
        }
        destination[index] = 0u;
        return (index == g_display_fileman_edit_bytes) ? index : 0u;
    }

    return display64_fileman_build_current_named_path(
        destination,
        capacity,
        g_display_fileman_edit_buffer,
        g_display_fileman_edit_bytes);
}

static u32 display64_fileman_build_current_note_path(u8 *destination, u32 capacity)
{
    static const u8 note_name[] = "GUI-NOTE.TXT";

    return display64_fileman_build_current_named_path(
        destination,
        capacity,
        note_name,
        (u32)(sizeof(note_name) - 1u));
}

static void display64_fileman_write_note(void)
{
    static const u8 note_data[] =
        "LimitlessOS Product GUI note\n"
        "Created through the brokered NVMe FAT File Manager.\n";
    u8 note_path[DISPLAY64_FILEMAN_PATH_BYTES];
    u32 path_bytes;

    path_bytes = display64_fileman_build_current_note_path(note_path, sizeof(note_path));
    g_display_fileman_last_write_status = 0u;
    if ((path_bytes != 0u)
        && (mmio64_nvme_fat_shell_write_file(
            note_path,
            path_bytes,
            note_data,
            (u32)(sizeof(note_data) - 1u),
            PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u))
    {
        ++g_display_fileman_backend_write_count;
        g_display_fileman_last_write_status = 1u;
        display64_fileman_refresh();
        return;
    }

    ++g_display_fileman_backend_write_denial_count;
    g_display_fileman_last_write_status = 2u;
    display64_fileman_refresh();
}

static void display64_fileman_delete_selected(void)
{
    u8 path[DISPLAY64_FILEMAN_PATH_BYTES];
    mmio64_nvme_fat_dirent_t *entry;
    u32 path_bytes;

    g_display_fileman_last_delete_status = 0u;
    if (g_display_fileman_selected_index >= g_display_fileman_entry_count)
    {
        ++g_display_fileman_backend_delete_denial_count;
        g_display_fileman_last_delete_status = 2u;
        display64_fileman_clear_delete_confirm();
        return;
    }

    entry = &g_display_fileman_entries[g_display_fileman_selected_index];
    if ((entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
        && (entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY))
    {
        ++g_display_fileman_backend_delete_denial_count;
        g_display_fileman_last_delete_status = 2u;
        display64_fileman_clear_delete_confirm();
        return;
    }

    path_bytes = display64_fileman_build_child_path(path, sizeof(path), entry);
    if (path_bytes == 0u)
    {
        ++g_display_fileman_backend_delete_denial_count;
        g_display_fileman_last_delete_status = 2u;
        display64_fileman_clear_delete_confirm();
        return;
    }

    if ((g_display_fileman_delete_armed == 0u)
        || (display64_fileman_path_matches(path, g_display_fileman_delete_path, sizeof(path)) == 0u))
    {
        (void)display64_fileman_copy_bytes(g_display_fileman_delete_path, sizeof(g_display_fileman_delete_path), path, path_bytes);
        g_display_fileman_delete_armed = 1u;
        g_display_fileman_last_delete_status = 3u;
        ++g_display_fileman_backend_delete_confirm_count;
        return;
    }

    if (mmio64_nvme_fat_shell_delete_file(
            path,
            path_bytes,
            PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u)
    {
        ++g_display_fileman_backend_delete_count;
        g_display_fileman_last_delete_status = 1u;
        g_display_fileman_window_cursor = 0u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_refresh();
        return;
    }

    ++g_display_fileman_backend_delete_denial_count;
    g_display_fileman_last_delete_status = 2u;
    display64_fileman_clear_delete_confirm();
    display64_fileman_refresh();
}

static void display64_fileman_create_folder(void)
{
    static const u8 folder_name[] = "GUI-DIR";

    display64_fileman_begin_edit(1u, folder_name, (u32)(sizeof(folder_name) - 1u));
}

static void display64_fileman_commit_create_folder(void)
{
    u8 path[DISPLAY64_FILEMAN_PATH_BYTES];
    u32 path_bytes;

    g_display_fileman_last_mutation_status = 0u;
    path_bytes = display64_fileman_build_edit_target_path(path, sizeof(path));
    if ((path_bytes != 0u)
        && (mmio64_nvme_fat_shell_mkdir(
                path,
                path_bytes,
                PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u))
    {
        ++g_display_fileman_backend_mkdir_count;
        g_display_fileman_last_mutation_status = 1u;
        g_display_fileman_window_cursor = 0u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        display64_fileman_refresh();
        return;
    }

    ++g_display_fileman_backend_mkdir_denial_count;
    g_display_fileman_last_mutation_status = 2u;
    display64_fileman_clear_delete_confirm();
    display64_fileman_clear_edit();
    display64_fileman_refresh();
}

static void display64_fileman_rename_selected(void)
{
    static const u8 renamed_name[] = "RENAMED.TXT";

    display64_fileman_begin_edit(2u, renamed_name, (u32)(sizeof(renamed_name) - 1u));
}

static void display64_fileman_commit_rename_selected(void)
{
    u8 source[DISPLAY64_FILEMAN_PATH_BYTES];
    u8 destination[DISPLAY64_FILEMAN_PATH_BYTES];
    mmio64_nvme_fat_dirent_t *entry;
    u32 source_bytes;
    u32 destination_bytes;

    g_display_fileman_last_mutation_status = 0u;
    if (g_display_fileman_selected_index >= g_display_fileman_entry_count)
    {
        ++g_display_fileman_backend_rename_denial_count;
        g_display_fileman_last_mutation_status = 4u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        return;
    }

    entry = &g_display_fileman_entries[g_display_fileman_selected_index];
    if (entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
    {
        ++g_display_fileman_backend_rename_denial_count;
        g_display_fileman_last_mutation_status = 4u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        return;
    }

    source_bytes = display64_fileman_build_child_path(source, sizeof(source), entry);
    destination_bytes = display64_fileman_build_edit_target_path(destination, sizeof(destination));
    if ((source_bytes != 0u)
        && (destination_bytes != 0u)
        && (mmio64_nvme_fat_shell_rename_file(
                source,
                source_bytes,
                destination,
                destination_bytes,
                PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u))
    {
        ++g_display_fileman_backend_rename_count;
        g_display_fileman_last_mutation_status = 3u;
        g_display_fileman_window_cursor = 0u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        display64_fileman_refresh();
        return;
    }

    ++g_display_fileman_backend_rename_denial_count;
    g_display_fileman_last_mutation_status = 4u;
    display64_fileman_clear_delete_confirm();
    display64_fileman_clear_edit();
    display64_fileman_refresh();
}

static void display64_fileman_move_selected_to_data(void)
{
    static const u8 destination[] = "/APPS/DATA/MOVED.TXT";

    display64_fileman_begin_edit(3u, destination, (u32)(sizeof(destination) - 1u));
}

static void display64_fileman_copy_selected_to_data(void)
{
    static const u8 destination[] = "/APPS/DATA/COPY.TXT";

    display64_fileman_begin_edit(4u, destination, (u32)(sizeof(destination) - 1u));
}

static void display64_fileman_commit_move_selected(void)
{
    u8 source[DISPLAY64_FILEMAN_PATH_BYTES];
    u8 destination[DISPLAY64_FILEMAN_PATH_BYTES];
    mmio64_nvme_fat_dirent_t *entry;
    u32 source_bytes;
    u32 destination_bytes;

    g_display_fileman_last_mutation_status = 0u;
    if (g_display_fileman_selected_index >= g_display_fileman_entry_count)
    {
        ++g_display_fileman_backend_move_denial_count;
        g_display_fileman_last_mutation_status = 6u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        return;
    }

    entry = &g_display_fileman_entries[g_display_fileman_selected_index];
    if (entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
    {
        ++g_display_fileman_backend_move_denial_count;
        g_display_fileman_last_mutation_status = 6u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        return;
    }

    source_bytes = display64_fileman_build_child_path(source, sizeof(source), entry);
    destination_bytes = display64_fileman_build_edit_target_path(destination, sizeof(destination));
    if ((source_bytes != 0u)
        && (destination_bytes != 0u)
        && (mmio64_nvme_fat_shell_move_file(
                source,
                source_bytes,
                destination,
                destination_bytes,
                PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u))
    {
        ++g_display_fileman_backend_move_count;
        g_display_fileman_last_mutation_status = 5u;
        g_display_fileman_window_cursor = 0u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        display64_fileman_refresh();
        return;
    }

    ++g_display_fileman_backend_move_denial_count;
    g_display_fileman_last_mutation_status = 6u;
    display64_fileman_clear_delete_confirm();
    display64_fileman_clear_edit();
    display64_fileman_refresh();
}

static void display64_fileman_commit_copy_selected(void)
{
    u8 source[DISPLAY64_FILEMAN_PATH_BYTES];
    u8 destination[DISPLAY64_FILEMAN_PATH_BYTES];
    mmio64_nvme_fat_dirent_t *entry;
    u32 source_bytes;
    u32 destination_bytes;

    g_display_fileman_last_mutation_status = 0u;
    if (g_display_fileman_selected_index >= g_display_fileman_entry_count)
    {
        ++g_display_fileman_backend_copy_denial_count;
        g_display_fileman_last_mutation_status = 9u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        return;
    }

    entry = &g_display_fileman_entries[g_display_fileman_selected_index];
    if ((entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
        && (entry->entry_type != MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY))
    {
        ++g_display_fileman_backend_copy_denial_count;
        g_display_fileman_last_mutation_status = 9u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        return;
    }

    source_bytes = display64_fileman_build_child_path(source, sizeof(source), entry);
    destination_bytes = display64_fileman_build_edit_target_path(destination, sizeof(destination));
    if ((source_bytes != 0u)
        && (destination_bytes != 0u)
        && (mmio64_nvme_fat_shell_copy_file(
                source,
                source_bytes,
                destination,
                destination_bytes,
                PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u))
    {
        ++g_display_fileman_backend_copy_count;
        g_display_fileman_last_mutation_status = 8u;
        g_display_fileman_window_cursor = 0u;
        display64_fileman_clear_delete_confirm();
        display64_fileman_clear_edit();
        display64_fileman_refresh();
        return;
    }

    ++g_display_fileman_backend_copy_denial_count;
    g_display_fileman_last_mutation_status = 9u;
    display64_fileman_clear_delete_confirm();
    display64_fileman_clear_edit();
    display64_fileman_refresh();
}

static u32 display64_fileman_process_keyboard_edit(u8 value)
{
    if (g_display_fileman_edit_mode == 0u)
    {
        return 0u;
    }
    if (value == (u8)'\b')
    {
        if (g_display_fileman_edit_bytes > 0u)
        {
            --g_display_fileman_edit_bytes;
            g_display_fileman_edit_buffer[g_display_fileman_edit_bytes] = 0u;
        }
        display64_desktop_redraw();
        return 1u;
    }
    if ((value == (u8)'\n') || (value == (u8)'\r'))
    {
        ++g_display_fileman_backend_edit_commit_count;
        if (g_display_fileman_edit_mode == 1u)
        {
            display64_fileman_commit_create_folder();
        }
        else if (g_display_fileman_edit_mode == 2u)
        {
            display64_fileman_commit_rename_selected();
        }
        else if (g_display_fileman_edit_mode == 3u)
        {
            display64_fileman_commit_move_selected();
        }
        else if (g_display_fileman_edit_mode == 4u)
        {
            display64_fileman_commit_copy_selected();
        }
        else
        {
            display64_fileman_clear_edit();
        }
        display64_desktop_redraw();
        return 1u;
    }
    if ((value >= 32u) && (value <= 126u))
    {
        if ((g_display_fileman_edit_bytes + 1u) < sizeof(g_display_fileman_edit_buffer))
        {
            g_display_fileman_edit_buffer[g_display_fileman_edit_bytes++] = value;
            g_display_fileman_edit_buffer[g_display_fileman_edit_bytes] = 0u;
        }
        display64_desktop_redraw();
        return 1u;
    }

    return 1u;
}

static void display64_fileman_select_next(void)
{
    display64_fileman_refresh();
    if ((g_display_fileman_entry_count != 0u)
        && ((g_display_fileman_selected_index + 1u) < g_display_fileman_entry_count)
        && ((g_display_fileman_selected_index + 1u) < DISPLAY64_FILEMAN_MAX_ENTRIES))
    {
        ++g_display_fileman_selected_index;
    }
    else if ((g_display_fileman_entry_count == DISPLAY64_FILEMAN_MAX_ENTRIES)
        && (display64_fileman_loaded_next_cursor() != 0u))
    {
        g_display_fileman_window_cursor = display64_fileman_loaded_next_cursor();
        g_display_fileman_selected_index = 0u;
        display64_fileman_refresh();
    }
    display64_fileman_clear_delete_confirm();
    display64_fileman_preview_selected();
}

static void display64_fileman_select_previous(void)
{
    display64_fileman_refresh();
    if (g_display_fileman_selected_index > 0u)
    {
        --g_display_fileman_selected_index;
    }
    else if (g_display_fileman_window_cursor != 0u)
    {
        g_display_fileman_window_cursor = display64_fileman_find_previous_cursor(g_display_fileman_window_cursor);
        g_display_fileman_selected_index = 0u;
        display64_fileman_refresh();
    }
    display64_fileman_clear_delete_confirm();
    display64_fileman_preview_selected();
}

static u32 display64_fileman_process_keyboard_command(u8 value)
{
    u32 handled = 1u;

    if ((value == (u8)'\n') || (value == (u8)'\r'))
    {
        display64_fileman_open_selected_if_directory();
    }
    else if ((value == (u8)'j') || (value == (u8)'J'))
    {
        display64_fileman_select_next();
    }
    else if ((value == (u8)'k') || (value == (u8)'K'))
    {
        display64_fileman_select_previous();
    }
    else if ((value == (u8)'u') || (value == (u8)'U') || (value == (u8)'\b'))
    {
        display64_fileman_parent_path();
        display64_fileman_refresh();
    }
    else if ((value == (u8)'h') || (value == (u8)'H'))
    {
        display64_fileman_set_path("/");
        display64_fileman_refresh();
    }
    else if ((value == (u8)'a') || (value == (u8)'A'))
    {
        display64_fileman_set_path("/APPS");
        display64_fileman_refresh();
    }
    else if ((value == (u8)'t') || (value == (u8)'T'))
    {
        display64_fileman_set_path("/APPS/DATA");
        display64_fileman_refresh();
    }
    else if ((value == (u8)'n') || (value == (u8)'N'))
    {
        display64_fileman_write_note();
    }
    else if ((value == (u8)'f') || (value == (u8)'F'))
    {
        display64_fileman_create_folder();
    }
    else if ((value == (u8)'r') || (value == (u8)'R'))
    {
        display64_fileman_rename_selected();
    }
    else if ((value == (u8)'m') || (value == (u8)'M'))
    {
        display64_fileman_move_selected_to_data();
    }
    else if ((value == (u8)'c') || (value == (u8)'C'))
    {
        display64_fileman_copy_selected_to_data();
    }
    else if ((value == (u8)'d') || (value == (u8)'D'))
    {
        display64_fileman_delete_selected();
    }
    else
    {
        handled = 0u;
    }

    if (handled != 0u)
    {
        ++g_display_fileman_action_count;
        display64_desktop_redraw();
    }
    return handled;
}

static void display64_fileman_draw_preview_line(u32 x, u32 y, u32 line_index)
{
    char line[29];
    u32 source = line_index * 28u;
    u32 index;

    for (index = 0u; index < 28u; ++index)
    {
        line[index] = (source + index < g_display_fileman_preview_bytes)
            ? (char)g_display_fileman_preview[source + index]
            : '\0';
    }
    line[28] = '\0';
    if (line[0] != '\0')
    {
        (void)display64_draw_font_text(x, y, line, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    }
}

static const char *display64_fileman_status_detail(void)
{
    u32 cursor = 0u;

    cursor = display64_diag_append_u32(
        g_display_fileman_status_detail,
        cursor,
        sizeof(g_display_fileman_status_detail),
        g_display_fileman_entry_count);
    cursor = display64_diag_append_text(
        g_display_fileman_status_detail,
        cursor,
        sizeof(g_display_fileman_status_detail),
        " entries, sel ");
    cursor = display64_diag_append_u32(
        g_display_fileman_status_detail,
        cursor,
        sizeof(g_display_fileman_status_detail),
        (g_display_fileman_entry_count != 0u) ? (g_display_fileman_selected_index + 1u) : 0u);
    cursor = display64_diag_append_text(
        g_display_fileman_status_detail,
        cursor,
        sizeof(g_display_fileman_status_detail),
        ", ");
    cursor = display64_diag_append_text(
        g_display_fileman_status_detail,
        cursor,
        sizeof(g_display_fileman_status_detail),
        (mmio64_nvme_rw_delegated() != 0u) ? "scoped write authority" : "read-only authority");
    cursor = display64_diag_append_text(
        g_display_fileman_status_detail,
        cursor,
        sizeof(g_display_fileman_status_detail),
        (mmio64_usb_fat_located() != 0u) ? ", USB export ready" : ", USB export unavailable");
    g_display_fileman_status_detail[cursor] = '\0';
    return g_display_fileman_status_detail;
}
#endif

static void display64_desktop_draw_file_manager(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 body_x;
    u32 body_y;
    u32 body_h;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 content_w;
    u32 detail_path_bytes;
    u32 preview_bottom;
    u32 preview_h;
    u32 preview_line_count;
    u32 preview_y;
    u32 row;
    u32 row_y;
    mmio64_nvme_fat_dirent_t *selected_entry;
    u8 detail_path[DISPLAY64_FILEMAN_PATH_BYTES];
#endif

    if (window == 0)
    {
        return;
    }

    display64_wm_present_window(handle);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    cloud_storage64_init();
    display64_fileman_refresh();
    display64_fileman_preview_selected();
    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    body_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + 28u))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - 28u)
        : 0u;
    content_w = (window->width > 170u) ? (window->width - 158u) : 0u;
    if (content_w < 120u)
    {
        content_w = 120u;
    }
    selected_entry = (g_display_fileman_selected_index < g_display_fileman_entry_count)
        ? &g_display_fileman_entries[g_display_fileman_selected_index]
        : (mmio64_nvme_fat_dirent_t *)0;
    display64_compositor_draw_surface(body_x, body_y, 110u, body_h, DISPLAY64_RGB_SURFACE, DISPLAY64_RGB_SURFACE_BORDER, 0u);
    display64_compositor_fill_rect(body_x + 6u, body_y + 10u, 3u, 18u, DISPLAY64_RGB_APP_FILES);
    (void)display64_draw_font_text(body_x + 16u, body_y + 10u, "NVMe FAT32", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 36u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 31u, body_y + 40u, "Up", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 60u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 28u, body_y + 64u, "Root", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 84u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 29u, body_y + 88u, "APPS", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 108u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 29u, body_y + 112u, "DATA", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 132u, 78u, 20u, DISPLAY64_RGB_APP_FILES);
    (void)display64_draw_font_text(body_x + 23u, body_y + 136u, "New Note", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 156u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 25u, body_y + 160u, "Folder", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 180u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 23u, body_y + 184u, "Rename", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 204u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 30u, body_y + 208u, "Move", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 228u, 78u, 20u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 30u, body_y + 232u, "Copy", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 14u, body_y + 252u, 78u, 20u, DISPLAY64_RGB_CLOSE);
    (void)display64_draw_font_text(body_x + 25u, body_y + 256u, "Delete", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 16u, body_y + 276u, cloud_storage64_mode(), DISPLAY64_FONT_SMALL, DISPLAY64_RGB_DISABLED_TEXT, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_fileman_last_write_status == 1u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 288u, "Wrote note", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_APP_FILES, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_write_status == 2u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 288u, "Write denied", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }
    if (g_display_fileman_last_delete_status == 1u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 300u, "Deleted path", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_CLOSE, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_delete_status == 2u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 300u, "Delete denied", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_delete_status == 3u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 300u, "Confirm delete", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }
    if (g_display_fileman_last_mutation_status == 1u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Folder made", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_APP_FILES, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 2u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Folder denied", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 3u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Renamed file", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_APP_FILES, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 4u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Rename denied", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 5u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Moved file", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_APP_FILES, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 6u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Move denied", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 7u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Type, Enter", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_ACCENT, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 8u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Copied file", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_APP_FILES, DISPLAY64_FONT_TRANSPARENT);
    }
    else if (g_display_fileman_last_mutation_status == 9u)
    {
        (void)display64_draw_font_text(body_x + 16u, body_y + 312u, "Copy denied", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    }

    display64_compositor_draw_badge(body_x + 126u, body_y - 4u, content_w, (const char *)g_display_fileman_current_path, DISPLAY64_RGB_APP_FILES);
    ++g_display_fileman_storage_card_count;
    if (g_display_fileman_last_status != 0u)
    {
        display64_desktop_draw_status_card(
            body_x + 126u,
            body_y + 28u,
            content_w,
            "Storage unavailable",
            "run hwval full for raw evidence",
            0u,
            DISPLAY64_RGB_APP_FILES);
    }
    else if (g_display_fileman_entry_count == 0u)
    {
        display64_desktop_draw_status_card(
            body_x + 126u,
            body_y + 28u,
            content_w,
            "Empty folder",
            display64_fileman_status_detail(),
            1u,
            DISPLAY64_RGB_APP_FILES);
    }
    else
    {
        for (row = 0u; row < g_display_fileman_entry_count; ++row)
        {
            row_y = body_y + DISPLAY64_FILEMAN_ROW_BASE_Y + (row * DISPLAY64_FILEMAN_ROW_STEP);
            display64_desktop_draw_fileman_row(
                body_x + 126u,
                row_y,
                content_w,
                (const char *)g_display_fileman_entries[row].name,
                display64_fileman_type_text(g_display_fileman_entries[row].entry_type),
                (g_display_fileman_entries[row].entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY) ? DISPLAY64_RGB_APP_FILES : DISPLAY64_RGB_TEXT_SECONDARY,
                (g_display_fileman_selected_index == row) ? 1u : 0u);
            if (g_display_fileman_entries[row].entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_FILE)
            {
                if (content_w > 92u)
                {
                    display64_draw_label_value(
                        body_x + 126u + content_w - 68u,
                        row_y + 7u,
                        "",
                        g_display_fileman_entries[row].byte_count,
                        DISPLAY64_RGB_TEXT_MUTED);
                }
            }
        }
        if (content_w > 160u)
        {
            (void)display64_draw_font_text(body_x + 138u, body_y + 28u, display64_fileman_status_detail(), DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_MUTED, DISPLAY64_FONT_TRANSPARENT);
        }
    }
    if (content_w > 24u)
    {
        preview_y = body_y + DISPLAY64_FILEMAN_ROW_BASE_Y + (g_display_fileman_entry_count * DISPLAY64_FILEMAN_ROW_STEP) + DISPLAY64_FILEMAN_PREVIEW_GAP;
        preview_bottom = body_y + body_h;
        preview_h = (preview_bottom > preview_y) ? display64_min_u32(96u, preview_bottom - preview_y) : 0u;
        if (preview_h >= 44u)
        {
            display64_compositor_draw_surface(body_x + 126u, preview_y, content_w, preview_h, DISPLAY64_RGB_CONTENT, DISPLAY64_RGB_SURFACE_BORDER, 0u);
        if (g_display_fileman_edit_mode != 0u)
        {
            const char *edit_title = "Type path";
            if (g_display_fileman_edit_mode == 1u)
            {
                edit_title = "New folder";
            }
            else if (g_display_fileman_edit_mode == 2u)
            {
                edit_title = "Rename to";
            }
            else if (g_display_fileman_edit_mode == 3u)
            {
                edit_title = "Move to";
            }
            else if (g_display_fileman_edit_mode == 4u)
            {
                edit_title = "Copy to";
            }
            (void)display64_draw_font_text(body_x + 138u, preview_y + 8u, edit_title, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
            display64_compositor_fill_round_rect_4(body_x + 138u, preview_y + 30u, content_w - 24u, 24u, DISPLAY64_RGB_SURFACE_HIGH);
            (void)display64_draw_font_text(body_x + 148u, preview_y + 36u, (const char *)g_display_fileman_edit_buffer, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
            if (preview_h >= 70u)
            {
                (void)display64_draw_font_text(body_x + 138u, preview_y + 60u, "Enter commits", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
            }
        }
        else if ((selected_entry != (mmio64_nvme_fat_dirent_t *)0)
            && (selected_entry->entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_FILE))
        {
            (void)display64_draw_font_text(body_x + 138u, preview_y + 8u, (const char *)selected_entry->name, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
            display64_draw_label_value(body_x + 138u, preview_y + 24u, "Size ", selected_entry->byte_count, DISPLAY64_RGB_TEXT_SECONDARY);
            display64_draw_label_value(body_x + 224u, preview_y + 24u, "Cluster ", selected_entry->cluster, DISPLAY64_RGB_TEXT_SECONDARY);
            if (g_display_fileman_preview_bytes != 0u)
            {
                display64_draw_label_value(body_x + 138u, preview_y + 38u, "Preview ", g_display_fileman_preview_size, DISPLAY64_RGB_TEXT_PRIMARY);
                preview_line_count = (preview_h > 56u)
                    ? display64_min_u32(DISPLAY64_FILEMAN_PREVIEW_LINES, (preview_h - 50u) / 10u)
                    : 0u;
                for (row = 0u; row < preview_line_count; ++row)
                {
                    display64_fileman_draw_preview_line(body_x + 138u, preview_y + 52u + (row * 10u), row);
                }
            }
            else if (preview_h >= 58u)
            {
                (void)display64_draw_font_text(body_x + 138u, preview_y + 42u, "No readable preview bytes", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
            }
        }
        else if ((selected_entry != (mmio64_nvme_fat_dirent_t *)0)
            && (selected_entry->entry_type == MMIO64_NVME_FAT_DIRENT_TYPE_DIRECTORY))
        {
            (void)display64_draw_font_text(body_x + 138u, preview_y + 8u, (const char *)selected_entry->name, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
            display64_draw_label_value(body_x + 138u, preview_y + 24u, "Directory cluster ", selected_entry->cluster, DISPLAY64_RGB_TEXT_SECONDARY);
            detail_path_bytes = display64_fileman_build_child_path(detail_path, sizeof(detail_path), selected_entry);
            if ((detail_path_bytes != 0u) && (preview_h >= 58u))
            {
                (void)display64_draw_font_text(body_x + 138u, preview_y + 42u, (const char *)detail_path, DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
            }
            if (preview_h >= 72u)
            {
                (void)display64_draw_font_text(body_x + 138u, preview_y + 58u, "Open with click or context menu", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_MUTED, DISPLAY64_FONT_TRANSPARENT);
            }
        }
        else if (selected_entry != (mmio64_nvme_fat_dirent_t *)0)
        {
            (void)display64_draw_font_text(body_x + 138u, preview_y + 8u, (const char *)selected_entry->name, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
            display64_draw_label_value(body_x + 138u, preview_y + 24u, "Unknown entry cluster ", selected_entry->cluster, DISPLAY64_RGB_TEXT_SECONDARY);
        }
        else
        {
            (void)display64_draw_font_text(body_x + 138u, preview_y + 8u, "NVMe FAT actions", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
            (void)display64_draw_font_text(body_x + 138u, preview_y + 26u, "Select an item for properties", DISPLAY64_FONT_SMALL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
        }
        }
    }
#else
    body_x = window->x + 10u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 12u;
    body_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + 20u))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - 20u)
        : 0u;
    display64_compositor_draw_rect(body_x + 100u, body_y - 4u, 1u, body_h, DISPLAY64_RGB_SURFACE_BORDER);
    (void)display64_draw_font_text(body_x, body_y, "RAMFS /", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 20u, "NVMe FAT32", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 40u, "Cloud", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_DISABLED_TEXT, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 116u, body_y, "README.TXT", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 116u, body_y + 20u, "APPS/", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 116u, body_y + 40u, "Cloud unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_DISABLED_TEXT, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 116u, body_y + 60u, "No sync/upload", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_DISABLED_TEXT, DISPLAY64_FONT_TRANSPARENT);
#endif
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    cloud_storage64_init();
    (void)cloud_storage64_fileman_status_readonly();
#endif
    if (g_display_desktop_fileman_count == 0u)
    {
        ++g_display_desktop_fileman_count;
    }
    if (g_display_cloud_fileman_status_count == 0u)
    {
        ++g_display_cloud_fileman_status_count;
    }
}

static void display64_desktop_draw_settings(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 body_x;
    u32 body_y;

    if (window == 0)
    {
        return;
    }

    display64_wm_present_window(handle);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    display64_desktop_draw_settings_summary(body_x, body_y, window->width);
    services64_product_status_query();
    identity64_status_readonly();
    identity_transport64_init();
    (void)identity_transport64_status_readonly();
    account_association64_init();
    (void)account_association64_status_readonly();
    cloud_storage64_init();
    (void)cloud_storage64_settings_readonly();
    ai_policy64_init();
    ai_policy64_action_probe();
    (void)ai_policy64_settings_readonly();
    if (g_display_desktop_settings_count == 0u)
    {
        ++g_display_desktop_settings_count;
    }
    if (g_display_pkg_settings_panel_count == 0u)
    {
        ++g_display_pkg_settings_panel_count;
    }
    if (g_display_settings_hardware_panel_count == 0u)
    {
        ++g_display_settings_hardware_panel_count;
    }
    if (g_display_settings_input_panel_count == 0u)
    {
        ++g_display_settings_input_panel_count;
    }
    if (g_display_identity_settings_panel_count == 0u)
    {
        ++g_display_identity_settings_panel_count;
    }
    if (g_display_identity_transport_settings_panel_count == 0u)
    {
        ++g_display_identity_transport_settings_panel_count;
    }
    if (g_display_account_settings_panel_count == 0u)
    {
        ++g_display_account_settings_panel_count;
    }
    if (g_display_cloud_settings_panel_count == 0u)
    {
        ++g_display_cloud_settings_panel_count;
    }
    if (g_display_ai_settings_panel_count == 0u)
    {
        ++g_display_ai_settings_panel_count;
    }
    return;
#endif
    body_x = window->x + 10u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 12u;
    (void)display64_draw_font_text(body_x, body_y, "Display", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_draw_label_value(body_x, body_y + 18u, "W ", g_display_boot_info->framebuffer_width, DISPLAY64_RGB_TEXT_SECONDARY);
    display64_draw_label_value(body_x + 88u, body_y + 18u, "H ", g_display_boot_info->framebuffer_height, DISPLAY64_RGB_TEXT_SECONDARY);
    (void)display64_draw_font_text(body_x, body_y + 40u, "FB BGR", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 64u, "Storage RAMFS NVMe", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 88u, "Network DHCP DNS HTTP", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 112u, "About LimitlessOS", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_draw_label_value(body_x, body_y + 130u, "Sectors ", g_display_boot_info->kernel_sector_count, DISPLAY64_RGB_TEXT_SECONDARY);
    services64_product_status_query();
    display64_draw_label_value(body_x, body_y + 148u, "Services ", services64_product_service_running(), DISPLAY64_RGB_TEXT_SECONDARY);
    display64_draw_label_value(body_x + 128u, body_y + 148u, "Session ", services64_session_id(), DISPLAY64_RGB_TEXT_SECONDARY);
    (void)display64_draw_font_text(body_x, body_y + 166u, "Installer UX dry-run; writes disabled", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x, body_y + 184u, 72u, 24u, DISPLAY64_RGB_ACCENT);
    (void)display64_draw_font_text(body_x + 18u, body_y + 188u, "Lock", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)display64_draw_font_text(body_x, body_y + 216u, "Identity", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 234u, "Local active; vault metadata only", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 252u, "No ambient identity/secret", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_identity_settings_panel_count == 0u)
    {
        ++g_display_identity_settings_panel_count;
    }
    (void)identity64_status_readonly();
    identity_transport64_init();
    (void)display64_draw_font_text(body_x, body_y + 270u, "Identity transport", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 288u, "Descriptor verified; encrypted transport unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_identity_transport_settings_panel_count == 0u)
    {
        ++g_display_identity_transport_settings_panel_count;
    }
    (void)identity_transport64_status_readonly();
    account_association64_init();
    (void)display64_draw_font_text(body_x, body_y + 306u, "Account association", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 324u, "Local active; personal/enterprise unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 342u, "Cloud/key unavailable; tokens denied", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_account_settings_panel_count == 0u)
    {
        ++g_display_account_settings_panel_count;
    }
    (void)account_association64_status_readonly();
    cloud_storage64_init();
    (void)display64_draw_font_text(body_x, body_y + 360u, "Cloud storage", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 378u, "Broker foundation; descriptor verified", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 396u, "No sync/upload/download; AI denied", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_cloud_settings_panel_count == 0u)
    {
        ++g_display_cloud_settings_panel_count;
    }
    (void)cloud_storage64_settings_readonly();
    ai_policy64_init();
    ai_policy64_action_probe();
    (void)display64_draw_font_text(body_x, body_y + 420u, "AI policy", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 438u, "Assistant Mode B; inference unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 456u, "Action templates require consent", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 474u, "No ambient fs/net/pkg/secret/cloud", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_ai_settings_panel_count == 0u)
    {
        ++g_display_ai_settings_panel_count;
    }
    (void)ai_policy64_settings_readonly();
    (void)display64_draw_font_text(body_x, body_y + 492u, "Package Trust", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    if (package_signing64_signed() != 0u)
    {
        (void)display64_draw_font_text(body_x, body_y + 510u, "UEFI Ed25519 verified", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    }
    else
    {
        (void)display64_draw_font_text(body_x, body_y + 510u, "BIOS checksum fallback", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    }
#else
    (void)display64_draw_font_text(body_x, body_y + 216u, "Package Trust", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    if (package_signing64_signed() != 0u)
    {
        (void)display64_draw_font_text(body_x, body_y + 234u, "UEFI Ed25519 verified", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
        display64_draw_label_value(body_x, body_y + 252u, "Signed ", package_signing64_signed_package_count(), DISPLAY64_RGB_TEXT_SECONDARY);
    }
    else
    {
        (void)display64_draw_font_text(body_x, body_y + 234u, "BIOS checksum fallback", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
        (void)display64_draw_font_text(body_x, body_y + 252u, "UEFI signing unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    }
    (void)display64_draw_font_text(body_x, body_y + 270u, "Index verified local fixture", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 288u, "No auto-install/public fetch", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 306u, "Install/apply disabled", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
#endif
    if (g_display_desktop_settings_count == 0u)
    {
        ++g_display_desktop_settings_count;
    }
    if (g_display_pkg_settings_panel_count == 0u)
    {
        ++g_display_pkg_settings_panel_count;
    }
}

static void display64_desktop_draw_installer(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 body_x;
    u32 body_y;

    if (window == 0)
    {
        return;
    }

    display64_wm_present_window(handle);
    body_x = window->x + 10u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 12u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    installer_ux64_init();
    (void)display64_draw_font_text(body_x, body_y, "LimitlessOS Installer", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_draw_badge(body_x + 164u, body_y - 4u, 92u, installer_ux64_dryrun_status(), DISPLAY64_RGB_APP_INSTALLER);
    if (g_display_installer_welcome_count == 0u)
    {
        ++g_display_installer_welcome_count;
    }
    (void)installer_ux64_welcome();
    if (g_display_installer_beginner_count == 0u)
    {
        ++g_display_installer_beginner_count;
    }
    if (g_display_installer_advanced_count == 0u)
    {
        ++g_display_installer_advanced_count;
    }
    (void)installer_ux64_beginner_mode();
    (void)installer_ux64_advanced_mode();
    if (g_display_installer_hardware_count == 0u)
    {
        ++g_display_installer_hardware_count;
    }
    if (g_display_installer_recommendation_count == 0u)
    {
        ++g_display_installer_recommendation_count;
    }
    (void)installer_ux64_hardware_summary();
    (void)installer_ux64_recommendation();
    if (g_display_installer_component_count == 0u)
    {
        ++g_display_installer_component_count;
    }
    (void)installer_ux64_component_selection();
    (void)installer_ux64_unavailable_components_labeled();
    if (g_display_installer_account_count == 0u)
    {
        ++g_display_installer_account_count;
    }
    (void)installer_ux64_account_page();
    if (g_display_installer_cloud_count == 0u)
    {
        ++g_display_installer_cloud_count;
    }
    (void)installer_ux64_cloud_page();
    if (g_display_installer_ai_count == 0u)
    {
        ++g_display_installer_ai_count;
    }
    (void)installer_ux64_ai_page();
    if (g_display_installer_plan_count == 0u)
    {
        ++g_display_installer_plan_count;
    }
    if (g_display_installer_dryrun_count == 0u)
    {
        ++g_display_installer_dryrun_count;
    }
    (void)installer_ux64_plan_generated();
    (void)installer_ux64_dryrun_no_writes();

    display64_desktop_draw_info_row_selected(body_x, body_y + 34u, window->width - 32u, "Profile", installer_ux64_selected_profile(), DISPLAY64_RGB_APP_INSTALLER, (g_display_installer_step_index == 0u) ? 1u : 0u);
    display64_desktop_draw_info_row_selected(body_x, body_y + 80u, window->width - 32u, "Hardware", installer_ux64_recommendation_text(), DISPLAY64_RGB_FOCUS_BLUE, (g_display_installer_step_index == 1u) ? 1u : 0u);
    display64_desktop_draw_info_row_selected(body_x, body_y + 126u, window->width - 32u, "Components", installer_ux64_component_status(), DISPLAY64_RGB_TEXT_SECONDARY, (g_display_installer_step_index == 2u) ? 1u : 0u);
    display64_desktop_draw_info_row_selected(body_x, body_y + 172u, window->width - 32u, "Account", installer_ux64_account_status(), DISPLAY64_RGB_APP_SETTINGS, (g_display_installer_step_index == 3u) ? 1u : 0u);
    display64_desktop_draw_info_row_selected(body_x, body_y + 218u, window->width - 32u, "Cloud and AI", installer_ux64_cloud_status(), DISPLAY64_RGB_DISABLED_TEXT, (g_display_installer_step_index == 4u) ? 1u : 0u);
    display64_desktop_draw_info_row_selected(body_x, body_y + 264u, window->width - 32u, "Plan", installer_ux64_plan_status(), DISPLAY64_RGB_WARNING, (g_display_installer_step_index == 5u) ? 1u : 0u);
    display64_compositor_fill_round_rect_4(body_x, body_y + 312u, 78u, 26u, DISPLAY64_RGB_SURFACE_HIGH);
    (void)display64_draw_font_text(body_x + 20u, body_y + 318u, "Back", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 88u, body_y + 312u, 78u, 26u, DISPLAY64_RGB_ACCENT);
    (void)display64_draw_font_text(body_x + 112u, body_y + 318u, "Next", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_round_rect_4(body_x + 176u, body_y + 312u, 92u, 26u, DISPLAY64_RGB_WARNING);
    (void)display64_draw_font_text(body_x + 190u, body_y + 318u, "Dry run", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_ON_ACCENT, DISPLAY64_FONT_TRANSPARENT);
#else
    (void)display64_draw_font_text(body_x, body_y, "Installer UX unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 18u, "BIOS fallback keeps dry-run/write disabled", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
#endif
}

static void display64_desktop_draw_assistant(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 body_x;
    u32 body_y;

    if (window == 0)
    {
        return;
    }

    display64_wm_present_window(handle);
    body_x = window->x + 10u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 12u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    ai_policy64_init();
    ai_policy64_action_probe();
    (void)display64_draw_font_text(body_x, body_y, "Limitless Assistant", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 20u, "Mode B: predefined action templates", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 44u, "Inference backend unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 68u, "Actions require explicit consent", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 92u, "Templates: note, dry-run, settings, pkg", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 116u, "Denied: package, settings, cloud, secret", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_WARNING, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 140u, "No model call or scripted response", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 164u, "Audit: request, consent, grant, result", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
    (void)ai_policy64_audit_query();
#else
    (void)display64_draw_font_text(body_x, body_y, "Assistant unavailable", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 18u, "BIOS checksum fallback", DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_SECONDARY, DISPLAY64_FONT_TRANSPARENT);
#endif
    if (g_display_desktop_assistant_count == 0u)
    {
        ++g_display_desktop_assistant_count;
    }
}

static int display64_desktop_settings_lock_hit(const struct display64_window *window, u32 x, u32 y)
{
    u32 body_x;
    u32 body_y;

    if (window == 0)
    {
        return 0;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    return display64_point_in_rect(
        x,
        y,
        body_x,
        body_y + DISPLAY64_SETTINGS_READINESS_Y + 22u + display64_desktop_settings_readiness_height((window->width > 32u) ? (window->width - 32u) : window->width),
        72u,
        24u);
#else
    body_x = window->x + 10u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 12u;
    return display64_point_in_rect(x, y, body_x, body_y + 184u, 72u, 24u);
#endif
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 display64_desktop_settings_row_hit(const struct display64_window *window, u32 x, u32 y)
{
    u32 body_x;
    u32 body_y;
    u32 row_w;
    u32 row_index;
    u32 visible_row;

    if (window == 0)
    {
        return 0xFFFFFFFFu;
    }

    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    row_w = (window->width > 32u) ? (window->width - 32u) : window->width;
    for (visible_row = 0u; visible_row < DISPLAY64_SETTINGS_VISIBLE_ROWS; ++visible_row)
    {
        row_index = g_display_settings_scroll_index + visible_row;
        if (row_index >= DISPLAY64_SETTINGS_ROW_COUNT)
        {
            return 0xFFFFFFFFu;
        }
        if (display64_point_in_rect(x, y, body_x, body_y + 28u + (visible_row * DISPLAY64_SETTINGS_ROW_STEP), row_w, 38u))
        {
            return row_index;
        }
    }

    return 0xFFFFFFFFu;
}

static u32 display64_desktop_file_manager_row_hit(const struct display64_window *window, u32 x, u32 y)
{
    u32 body_x;
    u32 body_y;
    u32 content_w;
    u32 index;

    if (window == 0)
    {
        return 0xFFFFFFFFu;
    }

    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    content_w = (window->width > 170u) ? (window->width - 158u) : 0u;
    for (index = 0u; (index < g_display_fileman_entry_count) && (index < DISPLAY64_FILEMAN_MAX_ENTRIES); ++index)
    {
        if (display64_point_in_rect(x, y, body_x + 126u, body_y + DISPLAY64_FILEMAN_ROW_BASE_Y + (index * DISPLAY64_FILEMAN_ROW_STEP), content_w, DISPLAY64_FILEMAN_ROW_HEIGHT))
        {
            return index;
        }
    }

    return 0xFFFFFFFFu;
}

static u32 display64_desktop_file_manager_nav_hit(const struct display64_window *window, u32 x, u32 y)
{
    u32 body_x;
    u32 body_y;

    if (window == 0)
    {
        return 0u;
    }

    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 36u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_UP;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 60u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_ROOT;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 84u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_APPS;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 108u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_DATA;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 132u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_NEW_NOTE;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 156u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_NEW_FOLDER;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 180u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_RENAME;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 204u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_MOVE;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 228u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_COPY;
    }
    if (display64_point_in_rect(x, y, body_x + 14u, body_y + 252u, 78u, 20u))
    {
        return DISPLAY64_FILEMAN_NAV_DELETE_NOTE;
    }

    return 0u;
}

static int display64_desktop_file_manager_apps_hit(const struct display64_window *window, u32 x, u32 y)
{
    return (display64_desktop_file_manager_nav_hit(window, x, y) == DISPLAY64_FILEMAN_NAV_APPS) ? 1 : 0;
}

static u32 display64_desktop_installer_action_hit(const struct display64_window *window, u32 x, u32 y)
{
    u32 body_x;
    u32 body_y;

    if (window == 0)
    {
        return 0u;
    }

    body_x = window->x + 16u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 14u;
    if (display64_point_in_rect(x, y, body_x, body_y + 312u, 78u, 26u))
    {
        return 1u;
    }
    if (display64_point_in_rect(x, y, body_x + 88u, body_y + 312u, 78u, 26u))
    {
        return 2u;
    }
    if (display64_point_in_rect(x, y, body_x + 176u, body_y + 312u, 92u, 26u))
    {
        return 3u;
    }

    return 0u;
}
#endif

static void display64_desktop_draw_background(void)
{
    display64_compositor_fill_rect(
        0u,
        0u,
        g_display_boot_info->framebuffer_width,
        g_display_boot_info->framebuffer_height,
        DISPLAY64_RGB_DESKTOP_BG);
    display64_font_draw_status_bar();
}

static void display64_desktop_present_window_content(u32 handle)
{
    struct display64_window *window;
    u32 token = 2166136261u;

    if (handle == g_display_desktop_fileman_handle)
    {
        display64_desktop_draw_file_manager(handle);
        return;
    }
    if (handle == g_display_desktop_settings_handle)
    {
        display64_desktop_draw_settings(handle);
        return;
    }
    if (handle == g_display_desktop_installer_handle)
    {
        display64_desktop_draw_installer(handle);
        return;
    }
    if (handle == g_display_desktop_assistant_handle)
    {
        display64_desktop_draw_assistant(handle);
        return;
    }

    display64_wm_present_window(handle);
    window = display64_wm_find_window(handle);
    if ((handle == g_display_wm_shell_handle) && display64_wm_window_is_terminal(window))
    {
        display64_wm_configure_console(window);
        (void)display64_console_replay_render(&token);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        display64_terminal_draw_overlay(window);
#endif
    }
}

static void display64_desktop_draw_windows_by_z(void)
{
    u32 last_z = 0u;
    u32 drawn = 0u;

    while (drawn < DISPLAY64_WM_MAX_WINDOWS)
    {
        struct display64_window *best = 0;
        u32 best_z = 0xFFFFFFFFu;
        u32 index;

        for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
        {
            if ((g_display_windows[index].visible != 0u)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                && (g_display_windows[index].minimized == 0u)
#endif
                && (g_display_windows[index].z > last_z)
                && (g_display_windows[index].z < best_z))
            {
                best = &g_display_windows[index];
                best_z = g_display_windows[index].z;
            }
        }

        if (best == 0)
        {
            break;
        }

        display64_desktop_present_window_content(best->handle);
        last_z = best_z;
        ++drawn;
    }
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void display64_desktop_draw_context_row(u32 x, u32 y, const char *text, u32 accent)
{
    display64_compositor_fill_round_rect_4(x + 4u, y, 140u, 24u, DISPLAY64_RGB_SURFACE_HIGH);
    display64_compositor_fill_rect(x + 10u, y + 7u, 3u, 10u, accent);
    (void)display64_draw_font_text(x + 20u, y + 5u, text, DISPLAY64_FONT_NORMAL, DISPLAY64_RGB_TEXT_PRIMARY, DISPLAY64_FONT_TRANSPARENT);
}

static void display64_desktop_draw_context_menu(void)
{
    u32 x;
    u32 y;
    u32 menu_h;
    const char *third = "Settings";
    u32 third_rgb = DISPLAY64_RGB_APP_SETTINGS;

    if (g_display_context_menu_open == 0u)
    {
        return;
    }

    x = g_display_context_menu_x;
    y = g_display_context_menu_y;
    menu_h = (g_display_context_menu_kind == 3u) ? 120u : 94u;
    if ((x + 152u) >= g_display_boot_info->framebuffer_width)
    {
        x = (g_display_boot_info->framebuffer_width > 160u) ? (g_display_boot_info->framebuffer_width - 160u) : 0u;
    }
    if ((y + menu_h + 2u) >= display64_desktop_taskbar_y())
    {
        y = (display64_desktop_taskbar_y() > (menu_h + 10u)) ? (display64_desktop_taskbar_y() - menu_h - 10u) : 0u;
    }

    display64_compositor_draw_surface(x, y, 152u, menu_h, DISPLAY64_RGB_SURFACE, DISPLAY64_RGB_SURFACE_BORDER, 2u);
    if (g_display_context_menu_kind == 3u)
    {
        display64_desktop_draw_context_row(x, y + 8u, "Open", DISPLAY64_RGB_APP_FILES);
        display64_desktop_draw_context_row(x, y + 34u, "Rename", DISPLAY64_RGB_FOCUS_BLUE);
        display64_desktop_draw_context_row(x, y + 60u, "Copy", DISPLAY64_RGB_APP_SETTINGS);
        display64_desktop_draw_context_row(x, y + 86u, "Delete", DISPLAY64_RGB_CLOSE);
        return;
    }
    if (g_display_context_menu_kind == 2u)
    {
        struct display64_window *window = display64_wm_find_window(g_display_context_menu_target);
        if ((window != 0) && (window->handle == g_display_desktop_installer_handle))
        {
            third = "Dry run";
            third_rgb = DISPLAY64_RGB_WARNING;
        }
        else if ((window != 0) && (window->handle == g_display_desktop_settings_handle))
        {
            third = "Lock";
            third_rgb = DISPLAY64_RGB_ACCENT;
        }
        else if ((window != 0) && display64_wm_window_is_terminal(window))
        {
            third = "New Terminal";
            third_rgb = DISPLAY64_RGB_APP_TERMINAL;
        }
        else if ((window != 0) && (window->handle == g_display_desktop_fileman_handle))
        {
            third = "Delete";
            third_rgb = DISPLAY64_RGB_CLOSE;
        }
        else
        {
            third = "Refresh";
            third_rgb = DISPLAY64_RGB_APP_FILES;
        }
        display64_desktop_draw_context_row(x, y + 8u, "Focus", DISPLAY64_RGB_FOCUS_BLUE);
        display64_desktop_draw_context_row(x, y + 34u, "Close", DISPLAY64_RGB_CLOSE);
        display64_desktop_draw_context_row(x, y + 60u, third, third_rgb);
        return;
    }

    display64_desktop_draw_context_row(x, y + 8u, "Terminal", DISPLAY64_RGB_APP_TERMINAL);
    display64_desktop_draw_context_row(x, y + 34u, "Files", DISPLAY64_RGB_APP_FILES);
    display64_desktop_draw_context_row(x, y + 60u, "Settings", DISPLAY64_RGB_APP_SETTINGS);
}

static u32 display64_desktop_context_menu_hit(u32 x, u32 y)
{
    u32 menu_x = g_display_context_menu_x;
    u32 menu_y = g_display_context_menu_y;
    u32 menu_h;
    u32 rows;
    u32 row;

    if (g_display_context_menu_open == 0u)
    {
        return 0u;
    }
    rows = (g_display_context_menu_kind == 3u) ? 4u : 3u;
    menu_h = (g_display_context_menu_kind == 3u) ? 120u : 94u;
    if ((menu_x + 152u) >= g_display_boot_info->framebuffer_width)
    {
        menu_x = (g_display_boot_info->framebuffer_width > 160u) ? (g_display_boot_info->framebuffer_width - 160u) : 0u;
    }
    if ((menu_y + menu_h + 2u) >= display64_desktop_taskbar_y())
    {
        menu_y = (display64_desktop_taskbar_y() > (menu_h + 10u)) ? (display64_desktop_taskbar_y() - menu_h - 10u) : 0u;
    }
    for (row = 0u; row < rows; ++row)
    {
        if (display64_point_in_rect(x, y, menu_x + 4u, menu_y + 8u + (row * 26u), 140u, 24u))
        {
            return row + 1u;
        }
    }

    return 0u;
}
#endif

static void display64_desktop_redraw(void)
{
    if ((g_display_desktop_active == 0u)
        || (g_display_compositor_active == 0u)
        || !display64_has_framebuffer())
    {
        return;
    }

    display64_compositor_restore_cursor_saved();
    display64_desktop_draw_background();
    display64_desktop_draw_windows_by_z();
    if (g_display_desktop_launcher_open != 0u)
    {
        display64_desktop_draw_launcher_panel();
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_desktop_draw_context_menu();
#endif
    display64_desktop_draw_taskbar();
    (void)display64_compositor_present();
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void display64_desktop_redraw_existing_dirty(void)
{
    u32 dirty;
    u32 dirty_x;
    u32 dirty_y;
    u32 dirty_w;
    u32 dirty_h;

    if ((g_display_desktop_active == 0u)
        || (g_display_compositor_active == 0u)
        || !display64_has_framebuffer())
    {
        return;
    }

    dirty = g_display_compositor_dirty;
    dirty_x = g_display_compositor_dirty_x;
    dirty_y = g_display_compositor_dirty_y;
    dirty_w = g_display_compositor_dirty_w;
    dirty_h = g_display_compositor_dirty_h;
    if ((dirty == 0u) || (dirty_w == 0u) || (dirty_h == 0u))
    {
        display64_desktop_redraw();
        return;
    }

    display64_compositor_restore_cursor_saved();
    g_display_compositor_dirty = 0u;
    display64_desktop_draw_background();
    display64_desktop_draw_windows_by_z();
    if (g_display_desktop_launcher_open != 0u)
    {
        display64_desktop_draw_launcher_panel();
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_desktop_draw_context_menu();
#endif
    display64_desktop_draw_taskbar();
    g_display_compositor_dirty = dirty;
    g_display_compositor_dirty_x = dirty_x;
    g_display_compositor_dirty_y = dirty_y;
    g_display_compositor_dirty_w = dirty_w;
    g_display_compositor_dirty_h = dirty_h;
    (void)display64_compositor_present();
}

static void display64_desktop_redraw_window_dirty(const struct display64_window *window)
{
    if (window == 0)
    {
        display64_desktop_redraw();
        return;
    }

    display64_compositor_mark_dirty(window->x, window->y, window->width, window->height);
    display64_desktop_redraw_existing_dirty();
}
#endif

static u32 display64_desktop_hit_taskbar_button(u32 x, u32 y)
{
    u32 taskbar_y = display64_desktop_taskbar_y();
    u32 clock_x;
    u32 button_x = DISPLAY64_DESKTOP_WINDOW_BUTTON_X;
    u32 index;

    if (!display64_has_framebuffer()
        || !display64_point_in_rect(x, y, 0u, taskbar_y, g_display_boot_info->framebuffer_width, DISPLAY64_DESKTOP_TASKBAR_HEIGHT))
    {
        return 0u;
    }

    if (display64_point_in_rect(
            x,
            y,
            DISPLAY64_DESKTOP_LAUNCHER_BUTTON_X,
            taskbar_y + 4u,
            DISPLAY64_DESKTOP_LAUNCHER_BUTTON_WIDTH,
            DISPLAY64_DESKTOP_LAUNCHER_BUTTON_HEIGHT))
    {
        return 0xFFFFFFFFu;
    }

    clock_x = (g_display_boot_info->framebuffer_width > 96u)
        ? (g_display_boot_info->framebuffer_width - 96u)
        : 0u;
    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if ((g_display_windows[index].visible != 0u)
            && ((button_x + DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH + 8u) < clock_x))
        {
            if (display64_point_in_rect(
                    x,
                    y,
                    button_x,
                    taskbar_y + 4u,
                    DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH,
                    DISPLAY64_DESKTOP_WINDOW_BUTTON_HEIGHT))
            {
                return g_display_windows[index].handle;
            }
            button_x += DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH + DISPLAY64_DESKTOP_WINDOW_BUTTON_GAP;
        }
    }

    return 0u;
}

static int display64_desktop_point_in_launcher_panel(u32 x, u32 y)
{
    return display64_point_in_rect(
        x,
        y,
        12u,
        display64_desktop_launcher_panel_y(),
        DISPLAY64_DESKTOP_LAUNCHER_WIDTH,
        DISPLAY64_DESKTOP_LAUNCHER_HEIGHT);
}

static u32 display64_desktop_hit_launcher_icon(u32 x, u32 y)
{
    u32 panel_y = display64_desktop_launcher_panel_y();

    if (display64_point_in_rect(x, y, 24u, panel_y + 36u, 92u, 32u))
    {
        return 1u;
    }
    if (display64_point_in_rect(x, y, 24u, panel_y + 72u, 92u, 32u))
    {
        return 2u;
    }
    if (display64_point_in_rect(x, y, 120u, panel_y + 36u, 96u, 32u))
    {
        return 3u;
    }
    if (display64_point_in_rect(x, y, 120u, panel_y + 72u, 96u, 32u))
    {
        return 4u;
    }
    if (display64_point_in_rect(x, y, 24u, panel_y + 108u, 192u, 32u))
    {
        return 5u;
    }

    return 0u;
}

static void display64_desktop_open_terminal(void)
{
    u32 offset = (g_display_desktop_terminal_count * 18u) % 96u;
    u32 width;
    u32 height;
    u32 handle;

    width = display64_min_u32(
        760u,
        (g_display_boot_info->framebuffer_width > 96u)
            ? (g_display_boot_info->framebuffer_width - 96u)
            : g_display_boot_info->framebuffer_width);
    height = display64_min_u32(
        420u,
        (g_display_boot_info->framebuffer_height > 160u)
            ? (g_display_boot_info->framebuffer_height - 160u)
            : g_display_boot_info->framebuffer_height);
    handle = display64_wm_create_window("Terminal", 48u + offset, 80u + offset, width, height);
    if (handle != 0u)
    {
        ++g_display_desktop_terminal_count;
        g_display_wm_shell_handle = handle;
        display64_wm_focus_and_route_console(handle);
    }
}

static u32 display64_desktop_side_reserved_width(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_desktop_product_layout != 0u)
    {
        return 0u;
    }
    if (g_display_boot_info->framebuffer_width > 960u)
    {
        u32 panel_width = (DISPLAY64_DIAG_PANEL_WIDTH > DISPLAY64_MOUSE_DIAG_PANEL_WIDTH)
            ? DISPLAY64_DIAG_PANEL_WIDTH
            : DISPLAY64_MOUSE_DIAG_PANEL_WIDTH;
        return panel_width + (DISPLAY64_DIAG_MARGIN * 2u);
    }
#endif
    return 0u;
}

static u32 display64_desktop_side_right_edge(void)
{
    u32 reserved = display64_desktop_side_reserved_width();

    if ((g_display_boot_info->framebuffer_width > reserved)
        && ((g_display_boot_info->framebuffer_width - reserved) > 384u))
    {
        return g_display_boot_info->framebuffer_width - reserved;
    }

    return g_display_boot_info->framebuffer_width;
}

static u32 display64_desktop_side_x(void)
{
    u32 right_edge = display64_desktop_side_right_edge();

    if (right_edge > 368u)
    {
        return right_edge - 344u;
    }

    return 24u;
}

static u32 display64_desktop_side_w(u32 side_x)
{
    u32 right_edge = display64_desktop_side_right_edge();
    u32 side_w = (right_edge > (side_x + 24u))
        ? display64_min_u32(320u, right_edge - side_x - 24u)
        : 160u;

    return (side_w < 160u) ? 160u : side_w;
}

static void display64_desktop_open_file_manager(void)
{
    if (display64_wm_find_window(g_display_desktop_fileman_handle) == 0)
    {
        u32 side_x = display64_desktop_side_x();
        u32 side_w = display64_desktop_side_w(side_x);
        g_display_desktop_fileman_handle = display64_wm_create_window(
            "File Manager",
            side_x,
            64u,
            side_w,
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            380u
#else
            210u
#endif
        );
    }
    display64_wm_focus_and_route_console(g_display_desktop_fileman_handle);
}

static void display64_desktop_open_settings(void)
{
    if (display64_wm_find_window(g_display_desktop_settings_handle) == 0)
    {
        u32 side_x = display64_desktop_side_x();
        u32 side_w = display64_desktop_side_w(side_x);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        u32 settings_y = (g_display_boot_info->framebuffer_height < 740u) ? 300u : 456u;
        u32 settings_h = display64_min_u32(
            390u,
            (g_display_boot_info->framebuffer_height > (settings_y + 60u))
                ? (g_display_boot_info->framebuffer_height - settings_y - 60u)
                : 260u);
        g_display_desktop_settings_handle = display64_wm_create_window("Settings", side_x, settings_y, side_w, settings_h);
#else
        u32 settings_y = (g_display_boot_info->framebuffer_height < 620u) ? 220u : 250u;
        g_display_desktop_settings_handle = display64_wm_create_window("Settings", side_x, settings_y, side_w, 500u);
#endif
    }
    display64_wm_focus_and_route_console(g_display_desktop_settings_handle);
}

static void display64_desktop_open_installer(void)
{
    if (display64_wm_find_window(g_display_desktop_installer_handle) == 0)
    {
        u32 width = display64_min_u32(
            520u,
            (g_display_boot_info->framebuffer_width > 80u)
                ? (g_display_boot_info->framebuffer_width - 80u)
                : g_display_boot_info->framebuffer_width);
        u32 height = display64_min_u32(
            430u,
            (g_display_boot_info->framebuffer_height > 150u)
                ? (g_display_boot_info->framebuffer_height - 150u)
                : g_display_boot_info->framebuffer_height);
        g_display_desktop_installer_handle = display64_wm_create_window("Installer", 70u, 92u, width, height);
    }
    display64_wm_focus_and_route_console(g_display_desktop_installer_handle);
}

static void display64_desktop_open_assistant(void)
{
    if (display64_wm_find_window(g_display_desktop_assistant_handle) == 0)
    {
        u32 width = display64_min_u32(
            500u,
            (g_display_boot_info->framebuffer_width > 96u)
                ? (g_display_boot_info->framebuffer_width - 96u)
                : g_display_boot_info->framebuffer_width);
        u32 height = display64_min_u32(
            300u,
            (g_display_boot_info->framebuffer_height > 150u)
                ? (g_display_boot_info->framebuffer_height - 150u)
                : g_display_boot_info->framebuffer_height);
        g_display_desktop_assistant_handle = display64_wm_create_window("Assistant", 96u, 118u, width, height);
    }
    display64_wm_focus_and_route_console(g_display_desktop_assistant_handle);
}

static void display64_desktop_open_launcher_icon(u32 icon)
{
    if (icon == 1u)
    {
        display64_desktop_open_terminal();
        g_display_gui_terminal_opened = 1u;
    }
    else if (icon == 2u)
    {
        display64_desktop_open_file_manager();
        g_display_gui_fileman_opened = 1u;
    }
    else if (icon == 3u)
    {
        display64_desktop_open_settings();
        g_display_gui_settings_opened = 1u;
    }
    else if (icon == 4u)
    {
        display64_desktop_open_installer();
        g_display_gui_installer_opened = 1u;
    }
    else if (icon == 5u)
    {
        display64_desktop_open_assistant();
        g_display_gui_assistant_opened = 1u;
    }
    g_display_desktop_launcher_open = 0u;
    display64_desktop_redraw();
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_desktop_open_app_by_id(u32 app_id)
{
    if ((g_display_desktop_active == 0u)
        || (g_display_wm_active == 0u)
        || !display64_has_framebuffer())
    {
        return 0u;
    }

    if (app_id == DISPLAY64_DESKTOP_APP_TERMINAL)
    {
        display64_desktop_open_terminal();
        g_display_gui_terminal_opened = 1u;
    }
    else if (app_id == DISPLAY64_DESKTOP_APP_FILES)
    {
        display64_desktop_open_file_manager();
        g_display_gui_fileman_opened = 1u;
    }
    else if (app_id == DISPLAY64_DESKTOP_APP_SETTINGS)
    {
        display64_desktop_open_settings();
        g_display_gui_settings_opened = 1u;
    }
    else if (app_id == DISPLAY64_DESKTOP_APP_INSTALLER)
    {
        display64_desktop_open_installer();
        g_display_gui_installer_opened = 1u;
    }
    else if (app_id == DISPLAY64_DESKTOP_APP_ASSISTANT)
    {
        display64_desktop_open_assistant();
        g_display_gui_assistant_opened = 1u;
    }
    else
    {
        return 0u;
    }

    ++g_display_gui_keyboard_open_count;
    g_display_gui_keyboard_routed = 1u;
    display64_desktop_redraw();
    return 1u;
}
#endif

void display64_desktop_probe(void)
{
    u32 side_x;
    u32 side_w;
    u32 file_y = 64u;
    u32 settings_y;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 settings_h;
#endif
    struct display64_window *terminal;

    if ((g_display_compositor_active == 0u)
        || (g_display_wm_active == 0u)
        || !display64_has_framebuffer())
    {
        g_display_desktop_active = 0u;
        return;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    /*
     * Product desktop composition owns the framebuffer from this point.
     * Mark it active before drawing windows so periodic boot/input
     * diagnostics do not repaint stale engineering overlays during the
     * desktop construction window. The raw evidence remains available
     * through hwval/hwfull/export commands.
     */
    g_display_desktop_active = 1u;
    g_display_desktop_product_layout = 1u;
#endif
    display64_desktop_draw_background();
    side_x = display64_desktop_side_x();
    side_w = display64_desktop_side_w(side_x);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    settings_y = 456u;
    if (g_display_boot_info->framebuffer_height < 740u)
    {
        settings_y = 300u;
    }
    settings_h = display64_min_u32(
        390u,
        (g_display_boot_info->framebuffer_height > (settings_y + 60u))
            ? (g_display_boot_info->framebuffer_height - settings_y - 60u)
            : 260u);
#else
    settings_y = 250u;
    if (g_display_boot_info->framebuffer_height < 620u)
    {
        settings_y = 220u;
    }
#endif

    g_display_desktop_terminal_count = (g_display_wm_shell_handle != 0u) ? 1u : 0u;
    g_display_desktop_fileman_handle = display64_wm_create_window(
        "File Manager",
        side_x,
        file_y,
        side_w,
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        380u
#else
        210u
#endif
    );
    g_display_desktop_settings_handle = display64_wm_create_window(
        "Settings",
        side_x,
        settings_y,
        side_w,
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        settings_h
#else
        500u
#endif
    );
    g_display_desktop_launcher_open = 0u;

    display64_wm_present_window(g_display_wm_shell_handle);
    display64_desktop_draw_file_manager(g_display_desktop_fileman_handle);
    display64_desktop_draw_settings(g_display_desktop_settings_handle);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_desktop_fileman_handle != 0u)
    {
        display64_wm_minimize_window(g_display_desktop_fileman_handle);
        ++g_display_product_startup_minimized_count;
    }
    if (g_display_desktop_settings_handle != 0u)
    {
        display64_wm_minimize_window(g_display_desktop_settings_handle);
        ++g_display_product_startup_minimized_count;
    }
#endif
    if (g_display_desktop_launcher_count == 0u)
    {
        ++g_display_desktop_launcher_count;
    }
    display64_desktop_draw_taskbar();

    display64_wm_focus_window(g_display_wm_shell_handle);
    display64_wm_present_window(g_display_wm_shell_handle);
    terminal = display64_wm_find_window(g_display_wm_shell_handle);
    display64_wm_configure_console(terminal);
    display64_desktop_draw_taskbar();
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    /*
     * Product mode keeps desktop ownership once the shell window and
     * framebuffer compositor are live. Optional side panels may be minimized,
     * resized, or delayed on real hardware, but boot diagnostics must not draw
     * over the active desktop after handoff.
     */
    g_display_desktop_active = (g_display_desktop_terminal_count != 0u) ? 1u : 0u;
#else
    g_display_desktop_active = (g_display_desktop_terminal_count != 0u)
        && (g_display_desktop_fileman_count != 0u)
        && (g_display_desktop_settings_count != 0u)
        && (g_display_desktop_taskbar_count != 0u)
        && (g_display_desktop_launcher_count != 0u)
        ? 1u
        : 0u;
#endif
    if (g_display_desktop_active != 0u)
    {
        g_display_gui_no_ambient_input = 1u;
        g_display_gui_no_ambient_display = 1u;
        g_display_gui_no_ambient_fs = 1u;
    }
    (void)display64_compositor_present();
}

u32 display64_wm_process_mouse_event(u32 x, u32 y, u32 buttons, s32 dx, s32 dy)
{
    u32 left = buttons & 0x1u;
    u32 previous_left = g_display_wm_last_buttons & 0x1u;
    u32 pressed = ((left != 0u) && (previous_left == 0u)) ? 1u : 0u;
    u32 released = ((left == 0u) && (previous_left != 0u)) ? 1u : 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 right = buttons & 0x2u;
    u32 previous_right = g_display_wm_last_buttons & 0x2u;
    u32 right_pressed = ((right != 0u) && (previous_right == 0u)) ? 1u : 0u;
#endif
    u32 focus_before = display64_wm_focused_handle();
    u32 z_before = 0u;

    (void)dx;
    (void)dy;

    if ((g_display_desktop_active == 0u)
        || (g_display_wm_active == 0u)
        || !display64_has_framebuffer())
    {
        g_display_wm_last_buttons = buttons;
        return 0u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (right_pressed != 0u)
    {
        struct display64_window *right_window = display64_wm_hit_window(x, y);
        u32 file_row = 0xFFFFFFFFu;
        ++g_display_gui_right_click_count;
        g_display_context_menu_open = 1u;
        g_display_context_menu_x = x;
        g_display_context_menu_y = y;
        if (right_window != 0)
        {
            z_before = right_window->z;
            g_display_desktop_launcher_open = 0u;
            g_display_context_menu_target = right_window->handle;
            if (right_window->handle == g_display_desktop_fileman_handle)
            {
                display64_fileman_refresh();
                file_row = display64_desktop_file_manager_row_hit(right_window, x, y);
                if (file_row != 0xFFFFFFFFu)
                {
                    g_display_fileman_selected_index = file_row;
                    display64_fileman_clear_delete_confirm();
                    display64_fileman_clear_edit();
                    display64_fileman_preview_selected();
                    g_display_context_menu_kind = 3u;
                }
                else
                {
                    g_display_context_menu_kind = 2u;
                }
            }
            else
            {
                g_display_context_menu_kind = 2u;
            }
            display64_wm_focus_and_route_console(right_window->handle);
            display64_desktop_redraw();
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_BODY,
                right_window->handle,
                focus_before,
                display64_wm_focused_handle(),
                z_before,
                display64_wm_window_z(right_window->handle));
        }
        else
        {
            g_display_context_menu_kind = 1u;
            g_display_context_menu_target = 0u;
            g_display_desktop_launcher_open = 0u;
            display64_desktop_redraw();
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_DESKTOP,
                0u,
                focus_before,
                display64_wm_focused_handle(),
                0u,
                0u);
        }
        g_display_wm_last_buttons = buttons;
        return 1u;
    }
#endif

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_terminal_selection_active != 0u)
    {
        struct display64_window *selected_window = display64_wm_find_window(g_display_wm_shell_handle);
        if (left != 0u)
        {
            g_display_terminal_selection_x = x;
            g_display_terminal_selection_y = y;
            g_display_terminal_selection_bytes = display64_terminal_selection_span_bytes();
            display64_desktop_redraw();
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_TERMINAL_ACTION,
                (selected_window != 0) ? selected_window->handle : 0u,
                focus_before,
                display64_wm_focused_handle(),
                z_before,
                (selected_window != 0) ? display64_wm_window_z(selected_window->handle) : 0u);
            g_display_wm_last_buttons = buttons;
            return 1u;
        }
        if (released != 0u)
        {
            display64_terminal_copy_selection();
            g_display_terminal_selection_active = 0u;
            ++g_display_terminal_action_count;
            display64_desktop_redraw();
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_TERMINAL_ACTION,
                (selected_window != 0) ? selected_window->handle : 0u,
                focus_before,
                display64_wm_focused_handle(),
                z_before,
                (selected_window != 0) ? display64_wm_window_z(selected_window->handle) : 0u);
            g_display_wm_last_buttons = buttons;
            return 1u;
        }
    }
#endif

    if (g_display_wm_dragging != 0u)
    {
        if (left != 0u)
        {
            u32 new_x = (x > g_display_wm_drag_offset_x) ? (x - g_display_wm_drag_offset_x) : 0u;
            u32 new_y = (y > g_display_wm_drag_offset_y) ? (y - g_display_wm_drag_offset_y) : 0u;
            z_before = display64_wm_window_z(g_display_wm_drag_handle);
            display64_wm_move_window(g_display_wm_drag_handle, new_x, new_y);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
            display64_desktop_redraw_existing_dirty();
#else
            display64_desktop_redraw();
#endif
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_TITLE,
                g_display_wm_drag_handle,
                focus_before,
                display64_wm_focused_handle(),
                z_before,
                display64_wm_window_z(g_display_wm_drag_handle));
            g_display_wm_last_buttons = buttons;
            return 1u;
        }

        if (released != 0u)
        {
            u32 drag_handle = g_display_wm_drag_handle;
            z_before = display64_wm_window_z(drag_handle);
            g_display_wm_dragging = 0u;
            g_display_wm_drag_handle = 0u;
            g_display_gui_drag_completed = 1u;
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_TITLE,
                drag_handle,
                focus_before,
                display64_wm_focused_handle(),
                z_before,
                display64_wm_window_z(drag_handle));
            g_display_wm_last_buttons = buttons;
            return 1u;
        }
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_wm_resizing != 0u)
    {
        if (left != 0u)
        {
            struct display64_window *resized_window = display64_wm_find_window(g_display_wm_resize_handle);
            if (resized_window != 0)
            {
                u32 new_w = (x > resized_window->x) ? (x - resized_window->x + 4u) : DISPLAY64_WM_MIN_WINDOW_WIDTH;
                u32 new_h = (y > resized_window->y) ? (y - resized_window->y + 4u) : DISPLAY64_WM_MIN_WINDOW_HEIGHT;
                z_before = display64_wm_window_z(g_display_wm_resize_handle);
                display64_wm_resize_window(g_display_wm_resize_handle, new_w, new_h);
                display64_desktop_redraw_existing_dirty();
                display64_gui_record_event(
                    x,
                    y,
                    DISPLAY64_GUI_REGION_RESIZE,
                    g_display_wm_resize_handle,
                    focus_before,
                    display64_wm_focused_handle(),
                    z_before,
                    display64_wm_window_z(g_display_wm_resize_handle));
            }
            g_display_wm_last_buttons = buttons;
            return 1u;
        }

        if (released != 0u)
        {
            u32 resize_handle = g_display_wm_resize_handle;
            z_before = display64_wm_window_z(resize_handle);
            g_display_wm_resizing = 0u;
            g_display_wm_resize_handle = 0u;
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_RESIZE,
                resize_handle,
                focus_before,
                display64_wm_focused_handle(),
                z_before,
                display64_wm_window_z(resize_handle));
            g_display_wm_last_buttons = buttons;
            return 1u;
        }
    }
#endif

    if (pressed != 0u)
    {
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        if (g_display_context_menu_open != 0u)
        {
            u32 menu_action = display64_desktop_context_menu_hit(x, y);
            u32 target = g_display_context_menu_target;
            u32 kind = g_display_context_menu_kind;
            g_display_context_menu_open = 0u;
            if (menu_action != 0u)
            {
                ++g_display_context_menu_action_count;
                if (kind == 1u)
                {
                    if (menu_action == 1u)
                    {
                        display64_desktop_open_terminal();
                        g_display_gui_terminal_opened = 1u;
                        ++g_display_terminal_action_count;
                    }
                    else if (menu_action == 2u)
                    {
                        display64_desktop_open_file_manager();
                        g_display_gui_fileman_opened = 1u;
                        ++g_display_fileman_action_count;
                    }
                    else
                    {
                        display64_desktop_open_settings();
                        g_display_gui_settings_opened = 1u;
                        ++g_display_settings_action_count;
                    }
                }
                else if (target != 0u)
                {
                    struct display64_window *target_window = display64_wm_find_window(target);
                    if ((kind == 3u) && (target_window != 0) && (target == g_display_desktop_fileman_handle))
                    {
                        if (menu_action == 1u)
                        {
                            display64_fileman_open_selected_if_directory();
                        }
                        else if (menu_action == 2u)
                        {
                            display64_fileman_rename_selected();
                        }
                        else if (menu_action == 3u)
                        {
                            display64_fileman_copy_selected_to_data();
                        }
                        else
                        {
                            display64_fileman_delete_selected();
                        }
                        display64_wm_focus_and_route_console(target);
                        ++g_display_fileman_action_count;
                    }
                    else if (menu_action == 1u)
                    {
                        display64_wm_focus_and_route_console(target);
                    }
                    else if (menu_action == 2u)
                    {
                        display64_wm_destroy_window(target);
                        g_display_gui_close_completed = 1u;
                    }
                    else if ((target_window != 0) && (target == g_display_desktop_settings_handle))
                    {
                        (void)auth64_lock_session();
                        ++g_display_settings_action_count;
                    }
                    else if ((target_window != 0) && (target == g_display_desktop_installer_handle))
                    {
                        (void)installer_ux64_commit_probe();
                        (void)installer_ux64_commit_unavailable();
                        ++g_display_installer_action_count;
                    }
                    else if ((target_window != 0) && display64_wm_window_is_terminal(target_window))
                    {
                        display64_desktop_open_terminal();
                        g_display_gui_terminal_opened = 1u;
                        ++g_display_terminal_action_count;
                    }
                    else if ((target_window != 0) && (target == g_display_desktop_fileman_handle))
                    {
                        display64_fileman_delete_selected();
                        ++g_display_fileman_action_count;
                    }
                    else
                    {
                        ++g_display_fileman_action_count;
                    }
                }
                display64_desktop_redraw();
                display64_gui_record_event(
                    x,
                    y,
                    DISPLAY64_GUI_REGION_CONTEXT_MENU,
                    target,
                    focus_before,
                    display64_wm_focused_handle(),
                    0u,
                    display64_wm_window_z(display64_wm_focused_handle()));
                g_display_wm_last_buttons = buttons;
                return 1u;
            }
            display64_desktop_redraw();
        }
#endif
        u32 taskbar_hit = display64_desktop_hit_taskbar_button(x, y);
        if (taskbar_hit == 0xFFFFFFFFu)
        {
            g_display_desktop_launcher_open = 1u;
            g_display_gui_launcher_opened = 1u;
            display64_desktop_redraw();
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_TASKBAR_LAUNCHER,
                0u,
                focus_before,
                display64_wm_focused_handle(),
                0u,
                0u);
            g_display_wm_last_buttons = buttons;
            return 1u;
        }
        if (taskbar_hit != 0u)
        {
            z_before = display64_wm_window_z(taskbar_hit);
            g_display_desktop_launcher_open = 0u;
            display64_wm_focus_and_route_console(taskbar_hit);
            g_display_gui_taskbar_focus = 1u;
            display64_desktop_redraw();
            display64_gui_record_event(
                x,
                y,
                DISPLAY64_GUI_REGION_TASKBAR_BUTTON,
                taskbar_hit,
                focus_before,
                display64_wm_focused_handle(),
                z_before,
                display64_wm_window_z(taskbar_hit));
            g_display_wm_last_buttons = buttons;
            return 1u;
        }

        if ((g_display_desktop_launcher_open != 0u)
            && display64_desktop_point_in_launcher_panel(x, y))
        {
            u32 icon = display64_desktop_hit_launcher_icon(x, y);
            if (icon != 0u)
            {
                u32 region = DISPLAY64_GUI_REGION_LAUNCHER_TERMINAL;
                if (icon == 2u)
                {
                    region = DISPLAY64_GUI_REGION_LAUNCHER_FILEMAN;
                }
                else if (icon == 3u)
                {
                    region = DISPLAY64_GUI_REGION_LAUNCHER_SETTINGS;
                }
                else if (icon == 4u)
                {
                    region = DISPLAY64_GUI_REGION_LAUNCHER_PANEL;
                }
                else if (icon == 5u)
                {
                    region = DISPLAY64_GUI_REGION_LAUNCHER_ASSISTANT;
                }
                display64_desktop_open_launcher_icon(icon);
                display64_gui_record_event(
                    x,
                    y,
                    region,
                    display64_wm_focused_handle(),
                    focus_before,
                    display64_wm_focused_handle(),
                    0u,
                    display64_wm_window_z(display64_wm_focused_handle()));
            }
            else
            {
                display64_gui_record_event(
                    x,
                    y,
                    DISPLAY64_GUI_REGION_LAUNCHER_PANEL,
                    0u,
                    focus_before,
                    display64_wm_focused_handle(),
                    0u,
                    0u);
            }
            g_display_wm_last_buttons = buttons;
            return 1u;
        }
        else
        {
            struct display64_window *window = display64_wm_hit_window(x, y);
            if (window != 0)
            {
                u32 close_x = (window->width > 28u) ? (window->x + window->width - 22u) : window->x;
                u32 close_y = window->y + 7u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                u32 minimize_x = (close_x > 22u) ? (close_x - 22u) : close_x;
#endif
                u32 region = DISPLAY64_GUI_REGION_BODY;
                g_display_desktop_launcher_open = 0u;
                z_before = window->z;

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                if (display64_point_in_rect(x, y, minimize_x, close_y, 14u, 14u))
                {
                    u32 target_handle = window->handle;
                    display64_wm_minimize_window(target_handle);
                    display64_desktop_redraw();
                    display64_gui_record_event(
                        x,
                        y,
                        DISPLAY64_GUI_REGION_MINIMIZE,
                        target_handle,
                        focus_before,
                        display64_wm_focused_handle(),
                        z_before,
                        display64_wm_window_z(target_handle));
                    g_display_wm_last_buttons = buttons;
                    return 1u;
                }

                if (display64_wm_resize_hit(window, x, y) != 0)
                {
                    display64_wm_focus_and_route_console(window->handle);
                    g_display_wm_resizing = 1u;
                    g_display_wm_resize_handle = window->handle;
                    display64_desktop_redraw();
                    display64_gui_record_event(
                        x,
                        y,
                        DISPLAY64_GUI_REGION_RESIZE,
                        window->handle,
                        focus_before,
                        display64_wm_focused_handle(),
                        z_before,
                        display64_wm_window_z(window->handle));
                    g_display_wm_last_buttons = buttons;
                    return 1u;
                }
#endif

                if ((window->handle == g_display_desktop_settings_handle)
                    && (display64_desktop_settings_lock_hit(window, x, y) != 0))
                {
                    display64_wm_focus_and_route_console(window->handle);
                    display64_desktop_redraw();
                    (void)auth64_lock_session();
                    display64_desktop_redraw();
                    display64_gui_record_event(
                        x,
                        y,
                        DISPLAY64_GUI_REGION_BODY,
                        window->handle,
                        focus_before,
                        display64_wm_focused_handle(),
                        z_before,
                        display64_wm_window_z(window->handle));
                    g_display_wm_last_buttons = buttons;
                    return 1u;
                }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
                if (window->handle == g_display_desktop_settings_handle)
                {
                    u32 settings_hit = display64_desktop_settings_row_hit(window, x, y);
                    if (settings_hit != 0xFFFFFFFFu)
                    {
                        g_display_settings_selected_index = settings_hit;
                        ++g_display_settings_action_count;
                        display64_settings_activate_row(settings_hit);
                        display64_wm_focus_and_route_console(window->handle);
                        display64_desktop_redraw();
                        display64_gui_record_event(
                            x,
                            y,
                            DISPLAY64_GUI_REGION_SETTINGS_ROW,
                            window->handle,
                            focus_before,
                            display64_wm_focused_handle(),
                            z_before,
                            display64_wm_window_z(window->handle));
                        g_display_wm_last_buttons = buttons;
                        return 1u;
                    }
                }

                if (window->handle == g_display_desktop_installer_handle)
                {
                    u32 installer_action = display64_desktop_installer_action_hit(window, x, y);
                    if (installer_action != 0u)
                    {
                        if (installer_action == 1u)
                        {
                            if (g_display_installer_step_index > 0u)
                            {
                                --g_display_installer_step_index;
                            }
                        }
                        else if (installer_action == 2u)
                        {
                            if (g_display_installer_step_index < 5u)
                            {
                                ++g_display_installer_step_index;
                            }
                        }
                        else
                        {
                            (void)installer_ux64_commit_probe();
                            (void)installer_ux64_commit_unavailable();
                        }
                        ++g_display_installer_action_count;
                        display64_wm_focus_and_route_console(window->handle);
                        display64_desktop_redraw();
                        display64_gui_record_event(
                            x,
                            y,
                            DISPLAY64_GUI_REGION_INSTALLER_ACTION,
                            window->handle,
                            focus_before,
                            display64_wm_focused_handle(),
                            z_before,
                            display64_wm_window_z(window->handle));
                        g_display_wm_last_buttons = buttons;
                        return 1u;
                    }
                }

                if (window->handle == g_display_desktop_fileman_handle)
                {
                    u32 nav_hit = display64_desktop_file_manager_nav_hit(window, x, y);
                    if (nav_hit != 0u)
                    {
                        if (nav_hit == DISPLAY64_FILEMAN_NAV_UP)
                        {
                            display64_fileman_parent_path();
                        }
                        else if (nav_hit == DISPLAY64_FILEMAN_NAV_ROOT)
                        {
                            display64_fileman_set_path("/");
                        }
                        else if (nav_hit == DISPLAY64_FILEMAN_NAV_APPS)
                        {
                            display64_fileman_set_path("/APPS");
                        }
                        else if (nav_hit == DISPLAY64_FILEMAN_NAV_DATA)
                        {
                            display64_fileman_set_path("/APPS/DATA");
                        }
                        else
                        {
                            if (nav_hit == DISPLAY64_FILEMAN_NAV_NEW_NOTE)
                            {
                                display64_fileman_write_note();
                            }
                            else if (nav_hit == DISPLAY64_FILEMAN_NAV_NEW_FOLDER)
                            {
                                display64_fileman_create_folder();
                            }
                            else if (nav_hit == DISPLAY64_FILEMAN_NAV_RENAME)
                            {
                                display64_fileman_rename_selected();
                            }
                            else if (nav_hit == DISPLAY64_FILEMAN_NAV_MOVE)
                            {
                                display64_fileman_move_selected_to_data();
                            }
                            else if (nav_hit == DISPLAY64_FILEMAN_NAV_COPY)
                            {
                                display64_fileman_copy_selected_to_data();
                            }
                            else
                            {
                                display64_fileman_delete_selected();
                            }
                        }
                        if ((nav_hit != DISPLAY64_FILEMAN_NAV_NEW_NOTE)
                            && (nav_hit != DISPLAY64_FILEMAN_NAV_NEW_FOLDER)
                            && (nav_hit != DISPLAY64_FILEMAN_NAV_RENAME)
                            && (nav_hit != DISPLAY64_FILEMAN_NAV_MOVE)
                            && (nav_hit != DISPLAY64_FILEMAN_NAV_COPY)
                            && (nav_hit != DISPLAY64_FILEMAN_NAV_DELETE_NOTE))
                        {
                            display64_fileman_refresh();
                        }
                        ++g_display_fileman_action_count;
                        display64_wm_focus_and_route_console(window->handle);
                        display64_desktop_redraw();
                        display64_gui_record_event(
                            x,
                            y,
                            DISPLAY64_GUI_REGION_FILEMAN_ROW,
                            window->handle,
                            focus_before,
                            display64_wm_focused_handle(),
                            z_before,
                            display64_wm_window_z(window->handle));
                        g_display_wm_last_buttons = buttons;
                        return 1u;
                    }

                    u32 file_hit = display64_desktop_file_manager_row_hit(window, x, y);
                    if (file_hit != 0xFFFFFFFFu)
                    {
                        g_display_fileman_selected_index = file_hit;
                        display64_fileman_clear_delete_confirm();
                        display64_fileman_clear_edit();
                        ++g_display_fileman_action_count;
                        display64_fileman_open_selected_if_directory();
                        display64_wm_focus_and_route_console(window->handle);
                        display64_desktop_redraw();
                        display64_gui_record_event(
                            x,
                            y,
                            DISPLAY64_GUI_REGION_FILEMAN_ROW,
                            window->handle,
                            focus_before,
                            display64_wm_focused_handle(),
                            z_before,
                            display64_wm_window_z(window->handle));
                        g_display_wm_last_buttons = buttons;
                        return 1u;
                    }
                }

                if ((window->handle == g_display_desktop_fileman_handle)
                    && (display64_desktop_file_manager_apps_hit(window, x, y) != 0))
                {
                    display64_wm_focus_and_route_console(window->handle);
                    g_display_desktop_launcher_open = 1u;
                    g_display_gui_launcher_opened = 1u;
                    display64_desktop_redraw();
                    display64_gui_record_event(
                        x,
                        y,
                        DISPLAY64_GUI_REGION_BODY,
                        window->handle,
                        focus_before,
                        display64_wm_focused_handle(),
                        z_before,
                        display64_wm_window_z(window->handle));
                    g_display_wm_last_buttons = buttons;
                    return 1u;
                }

                if (display64_wm_window_is_terminal(window)
                    && (display64_terminal_point_in_content(window, x, y) != 0u))
                {
                    display64_wm_focus_and_route_console(window->handle);
                    g_display_terminal_selection_active = 1u;
                    g_display_terminal_selection_anchor_x = x;
                    g_display_terminal_selection_anchor_y = y;
                    g_display_terminal_selection_x = x;
                    g_display_terminal_selection_y = y;
                    g_display_terminal_selection_bytes = 1u;
                    ++g_display_terminal_selection_count;
                    ++g_display_terminal_action_count;
                    display64_desktop_redraw();
                    display64_gui_record_event(
                        x,
                        y,
                        DISPLAY64_GUI_REGION_TERMINAL_ACTION,
                        window->handle,
                        focus_before,
                        display64_wm_focused_handle(),
                        z_before,
                        display64_wm_window_z(window->handle));
                    g_display_wm_last_buttons = buttons;
                    return 1u;
                }
#endif

                if (display64_point_in_rect(x, y, close_x, close_y, 14u, 14u))
                {
                    u32 target_handle = window->handle;
                    display64_wm_destroy_window(window->handle);
                    g_display_gui_close_completed = 1u;
                    display64_desktop_redraw();
                    display64_gui_record_event(
                        x,
                        y,
                        DISPLAY64_GUI_REGION_CLOSE,
                        target_handle,
                        focus_before,
                        display64_wm_focused_handle(),
                        z_before,
                        0u);
                    g_display_wm_last_buttons = buttons;
                    return 1u;
                }

                display64_wm_focus_and_route_console(window->handle);
                if (y < (window->y + DISPLAY64_WM_TITLE_HEIGHT))
                {
                    region = DISPLAY64_GUI_REGION_TITLE;
                    g_display_wm_dragging = 1u;
                    g_display_wm_drag_handle = window->handle;
                    g_display_wm_drag_offset_x = x - window->x;
                    g_display_wm_drag_offset_y = y - window->y;
                }
                display64_desktop_redraw();
                display64_gui_record_event(
                    x,
                    y,
                    region,
                    window->handle,
                    focus_before,
                    display64_wm_focused_handle(),
                    z_before,
                    display64_wm_window_z(window->handle));
                g_display_wm_last_buttons = buttons;
                return 1u;
            }
        }

        g_display_desktop_launcher_open = 0u;
        display64_wm_clear_focus();
        display64_desktop_redraw();
        display64_gui_record_event(
            x,
            y,
            DISPLAY64_GUI_REGION_DESKTOP,
            0u,
            focus_before,
            display64_wm_focused_handle(),
            0u,
            0u);
        g_display_wm_last_buttons = buttons;
        return 1u;
    }

    g_display_wm_last_buttons = buttons;
    return 0u;
}

u32 display64_wm_process_keyboard_event(u8 value)
{
    struct display64_window *focused;
    u32 focused_handle;

    (void)value;

    if ((g_display_desktop_active == 0u)
        || (g_display_wm_active == 0u)
        || !display64_has_framebuffer())
    {
        return 1u;
    }

    focused = display64_wm_focused_window();
    focused_handle = (focused != 0) ? focused->handle : 0u;
    g_display_gui_interactive = 1u;
    g_display_gui_key_target_window = focused_handle;

    if (focused == 0)
    {
        display64_gui_record_unfocused_keyboard_denial(0u);
        return 0u;
    }

    g_display_gui_keyboard_routed = 1u;
    if (display64_wm_window_is_terminal(focused))
    {
        display64_gui_record_unfocused_keyboard_denial(focused_handle);
        return 1u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (focused->handle == g_display_desktop_fileman_handle)
    {
        if (display64_fileman_process_keyboard_edit(value) != 0u)
        {
            return 1u;
        }
        if (display64_fileman_process_keyboard_command(value) != 0u)
        {
            display64_wm_focus_and_route_console(focused->handle);
            return 1u;
        }
    }
#endif

    display64_gui_record_unfocused_keyboard_denial(focused_handle);
    return 0u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_wm_process_mouse_wheel(s32 wheel_delta)
{
    struct display64_window *window;

    if ((wheel_delta == 0)
        || (g_display_desktop_active == 0u)
        || (g_display_wm_active == 0u)
        || !display64_has_framebuffer())
    {
        return 0u;
    }

    window = display64_wm_hit_window(g_display_compositor_cursor_x, g_display_compositor_cursor_y);
    if (window == 0)
    {
        window = display64_wm_focused_window();
    }
    if ((window != 0) && (window->handle == g_display_desktop_settings_handle))
    {
        if (wheel_delta < 0)
        {
            if (g_display_settings_scroll_index < (DISPLAY64_SETTINGS_ROW_COUNT - DISPLAY64_SETTINGS_VISIBLE_ROWS))
            {
                ++g_display_settings_scroll_index;
            }
        }
        else if (g_display_settings_scroll_index > 0u)
        {
            --g_display_settings_scroll_index;
        }
        ++g_display_gui_scroll_count;
        display64_wm_focus_and_route_console(window->handle);
        display64_desktop_redraw_window_dirty(window);
        return 1u;
    }
    if ((window != 0) && display64_wm_window_is_terminal(window))
    {
        if (wheel_delta > 0)
        {
            u32 max_offset = (g_display_console_replay_count > 1u)
                ? (g_display_console_replay_count - 1u)
                : 0u;
            if (g_display_terminal_scroll_offset < max_offset)
            {
                g_display_terminal_scroll_offset += DISPLAY64_TERMINAL_SCROLL_STEP_BYTES;
                if (g_display_terminal_scroll_offset > max_offset)
                {
                    g_display_terminal_scroll_offset = max_offset;
                }
            }
        }
        else if (g_display_terminal_scroll_offset > DISPLAY64_TERMINAL_SCROLL_STEP_BYTES)
        {
            g_display_terminal_scroll_offset -= DISPLAY64_TERMINAL_SCROLL_STEP_BYTES;
        }
        else
        {
            g_display_terminal_scroll_offset = 0u;
        }
        ++g_display_terminal_scroll_count;
        ++g_display_terminal_action_count;
        ++g_display_gui_scroll_count;
        display64_wm_focus_and_route_console(window->handle);
        display64_desktop_redraw_window_dirty(window);
        return 1u;
    }
    if ((window != 0) && (window->handle == g_display_desktop_fileman_handle))
    {
        display64_fileman_refresh();
        if (wheel_delta < 0)
        {
            if ((g_display_fileman_entry_count != 0u)
                && ((g_display_fileman_selected_index + 1u) < g_display_fileman_entry_count)
                && ((g_display_fileman_selected_index + 1u) < DISPLAY64_FILEMAN_MAX_ENTRIES))
            {
                ++g_display_fileman_selected_index;
            }
            else if ((g_display_fileman_entry_count == DISPLAY64_FILEMAN_MAX_ENTRIES)
                && (display64_fileman_loaded_next_cursor() != 0u))
            {
                g_display_fileman_window_cursor = display64_fileman_loaded_next_cursor();
                display64_fileman_refresh();
            }
        }
        else if (g_display_fileman_selected_index > 0u)
        {
            --g_display_fileman_selected_index;
        }
        else if (g_display_fileman_window_cursor != 0u)
        {
            g_display_fileman_window_cursor = display64_fileman_find_previous_cursor(g_display_fileman_window_cursor);
            display64_fileman_refresh();
        }
        display64_fileman_preview_selected();
        ++g_display_gui_scroll_count;
        display64_wm_focus_and_route_console(window->handle);
        display64_desktop_redraw_window_dirty(window);
        return 1u;
    }
    if ((window != 0) && (window->handle == g_display_desktop_installer_handle))
    {
        if (wheel_delta < 0)
        {
            if (g_display_installer_step_index < 5u)
            {
                ++g_display_installer_step_index;
            }
        }
        else if (g_display_installer_step_index > 0u)
        {
            --g_display_installer_step_index;
        }
        ++g_display_gui_scroll_count;
        display64_wm_focus_and_route_console(window->handle);
        display64_desktop_redraw_window_dirty(window);
        return 1u;
    }

    return 0u;
}
#endif

void display64_init(const struct boot_info *boot_info)
{
    u32 window_index;

    display64_set_boot_info(boot_info);
    g_display_draw_count = 0u;
    g_display_pixel_count = 0u;
    g_display_denial_count = 0u;
    g_display_unavailable_count = 0u;
    g_display_last_token = 0u;
    g_display_text_write_count = 0u;
    g_display_text_byte_count = 0u;
    g_display_clear_count = 0u;
    g_display_console_write_count = 0u;
    g_display_console_byte_count = 0u;
    g_display_console_line_clear_count = 0u;
    g_display_console_wrap_count = 0u;
    g_display_console_scroll_count = 0u;
    g_display_console_line_dirty = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_console_clip_count = 0u;
#endif
    g_display_console_replay_head = 0u;
    g_display_console_replay_count = 0u;
    g_display_console_replay_overflow = 0u;
    g_display_text_x = DISPLAY64_TEXT_START_X;
    g_display_text_y = DISPLAY64_TEXT_START_Y;
    g_display_console_x = DISPLAY64_TEXT_START_X;
    g_display_console_y = DISPLAY64_TEXT_START_Y;
    g_display_console_w = DISPLAY64_CONSOLE_VIEWPORT_WIDTH;
    g_display_console_h = DISPLAY64_CONSOLE_VIEWPORT_HEIGHT;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_text_scale = DISPLAY64_FONT_DEFAULT_SCALE;
    g_display_stride_ok = 0u;
    g_display_bounds_ok = 0u;
    g_display_console_fit = 0u;
    g_display_readable = 0u;
    g_display_layout_token = 0u;
#endif
    g_display_compositor_active = 0u;
    g_display_back_buffer = 0;
    g_display_back_buffer_pixels = 0ull;
    g_display_back_buffer_bytes = 0ull;
    g_display_compositor_present_count = 0u;
    g_display_compositor_cursor_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_direct_cursor_count = 0u;
    g_display_compositor_direct_mode = 0u;
#endif
    g_display_compositor_cursor_x = 32u;
    g_display_compositor_cursor_y = 32u;
    g_display_compositor_cursor_buttons = 0u;
    g_display_compositor_dirty = 0u;
    g_display_compositor_cursor_saved_valid = 0u;
    g_display_compositor_cursor_saved_x = 0u;
    g_display_compositor_cursor_saved_y = 0u;
    g_display_compositor_cursor_saved_w = 0u;
    g_display_compositor_cursor_saved_h = 0u;
    g_display_compositor_cursor_drawn_valid = 0u;
    g_display_compositor_cursor_drawn_x = 0u;
    g_display_compositor_cursor_drawn_y = 0u;
    g_display_compositor_cursor_drawn_buttons = 0u;
    g_display_font_active = 0u;
    g_display_font_render_count = 0u;
    g_display_wm_active = 0u;
    g_display_wm_next_handle = 1u;
    g_display_wm_next_z = 1u;
    g_display_wm_window_count = 0u;
    g_display_wm_focus_count = 0u;
    g_display_wm_present_count = 0u;
    g_display_wm_shell_handle = 0u;
    g_display_wm_dragging = 0u;
    g_display_wm_drag_handle = 0u;
    g_display_wm_drag_offset_x = 0u;
    g_display_wm_drag_offset_y = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_wm_resizing = 0u;
    g_display_wm_resize_handle = 0u;
#endif
    g_display_wm_last_buttons = 0u;
    g_display_desktop_active = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_desktop_product_layout = 0u;
    g_display_product_startup_minimized_count = 0u;
#endif
    g_display_desktop_taskbar_count = 0u;
    g_display_desktop_launcher_count = 0u;
    g_display_desktop_terminal_count = 0u;
    g_display_desktop_fileman_count = 0u;
    g_display_desktop_settings_count = 0u;
    g_display_desktop_assistant_count = 0u;
    g_display_pkg_settings_panel_count = 0u;
    g_display_identity_settings_panel_count = 0u;
    g_display_identity_transport_settings_panel_count = 0u;
    g_display_account_settings_panel_count = 0u;
    g_display_cloud_settings_panel_count = 0u;
    g_display_cloud_fileman_status_count = 0u;
    g_display_ai_settings_panel_count = 0u;
    g_display_installer_welcome_count = 0u;
    g_display_installer_beginner_count = 0u;
    g_display_installer_advanced_count = 0u;
    g_display_installer_hardware_count = 0u;
    g_display_installer_recommendation_count = 0u;
    g_display_installer_component_count = 0u;
    g_display_installer_account_count = 0u;
    g_display_installer_cloud_count = 0u;
    g_display_installer_ai_count = 0u;
    g_display_installer_plan_count = 0u;
    g_display_installer_dryrun_count = 0u;
    g_display_desktop_fileman_handle = 0u;
    g_display_desktop_settings_handle = 0u;
    g_display_desktop_installer_handle = 0u;
    g_display_desktop_assistant_handle = 0u;
    g_display_desktop_launcher_open = 0u;
    g_display_gui_interactive = 0u;
    g_display_gui_click_hittest = 0u;
    g_display_gui_launcher_opened = 0u;
    g_display_gui_terminal_opened = 0u;
    g_display_gui_drag_completed = 0u;
    g_display_gui_keyboard_routed = 0u;
    g_display_gui_close_completed = 0u;
    g_display_gui_taskbar_focus = 0u;
    g_display_gui_fileman_opened = 0u;
    g_display_gui_settings_opened = 0u;
    g_display_gui_installer_opened = 0u;
    g_display_gui_assistant_opened = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_gui_keyboard_open_count = 0u;
    g_display_input_diag_suppressed_count = 0u;
    g_display_mouse_diag_suppressed_count = 0u;
#endif
    g_display_gui_unfocused_key_denied = 0u;
    g_display_gui_unfocused_key_denial_count = 0u;
    g_display_gui_no_ambient_input = 0u;
    g_display_gui_no_ambient_display = 0u;
    g_display_gui_no_ambient_fs = 0u;
    g_display_gui_mouse_x = 0u;
    g_display_gui_mouse_y = 0u;
    g_display_gui_target_window = 0u;
    g_display_gui_target_region = DISPLAY64_GUI_REGION_NONE;
    g_display_gui_focus_before = 0u;
    g_display_gui_focus_after = 0u;
    g_display_gui_z_before = 0u;
    g_display_gui_z_after = 0u;
    g_display_gui_key_target_window = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_display_gui_right_click_count = 0u;
    g_display_gui_scroll_count = 0u;
    g_display_settings_scroll_index = 0u;
    g_display_context_menu_open = 0u;
    g_display_context_menu_x = 0u;
    g_display_context_menu_y = 0u;
    g_display_context_menu_target = 0u;
    g_display_context_menu_kind = 0u;
    g_display_context_menu_action_count = 0u;
    g_display_wm_resize_count = 0u;
    g_display_wm_minimize_count = 0u;
    g_display_wm_restore_count = 0u;
    g_display_wm_zorder_count = 0u;
    g_display_settings_loaded = 0u;
    g_display_settings_theme = DISPLAY64_SETTINGS_THEME_DARK;
    g_display_settings_pointer_speed = DISPLAY64_SETTINGS_POINTER_NORMAL;
    g_display_settings_key_repeat = 1u;
    g_display_settings_load_count = 0u;
    g_display_settings_save_count = 0u;
    g_display_settings_save_denial_count = 0u;
    g_display_settings_export_count = 0u;
    g_display_settings_export_denial_count = 0u;
    g_display_product_chrome_count = 0u;
    g_display_fileman_selected_index = 0u;
    g_display_fileman_window_cursor = 0u;
    g_display_settings_selected_index = 0u;
    g_display_installer_step_index = 0u;
    g_display_terminal_action_count = 0u;
    g_display_terminal_scroll_offset = 0u;
    g_display_terminal_scroll_count = 0u;
    g_display_terminal_selection_active = 0u;
    g_display_terminal_selection_anchor_x = 0u;
    g_display_terminal_selection_anchor_y = 0u;
    g_display_terminal_selection_x = 0u;
    g_display_terminal_selection_y = 0u;
    g_display_terminal_selection_count = 0u;
    g_display_terminal_copy_count = 0u;
    g_display_terminal_selection_bytes = 0u;
    g_display_terminal_copied_bytes = 0u;
    g_display_terminal_cursor_draw_count = 0u;
    g_display_login_present_count = 0u;
    g_display_login_setup_present_count = 0u;
    g_display_login_lock_present_count = 0u;
    g_display_login_unlock_present_count = 0u;
    g_display_login_recovery_present_count = 0u;
    g_display_login_wait_visible_count = 0u;
    g_display_login_safe_path_count = 0u;
    g_display_login_last_state = 0u;
    g_display_fileman_action_count = 0u;
    g_display_settings_action_count = 0u;
    g_display_installer_action_count = 0u;
    g_display_fileman_entry_count = 0u;
    g_display_fileman_backend_refresh_count = 0u;
    g_display_fileman_backend_preview_count = 0u;
    g_display_fileman_backend_open_dir_count = 0u;
    g_display_fileman_backend_write_count = 0u;
    g_display_fileman_backend_write_denial_count = 0u;
    g_display_fileman_backend_delete_count = 0u;
    g_display_fileman_backend_delete_denial_count = 0u;
    g_display_fileman_backend_delete_confirm_count = 0u;
    g_display_fileman_backend_mkdir_count = 0u;
    g_display_fileman_backend_mkdir_denial_count = 0u;
    g_display_fileman_backend_copy_count = 0u;
    g_display_fileman_backend_copy_denial_count = 0u;
    g_display_fileman_backend_rename_count = 0u;
    g_display_fileman_backend_rename_denial_count = 0u;
    g_display_fileman_backend_move_count = 0u;
    g_display_fileman_backend_move_denial_count = 0u;
    g_display_fileman_backend_edit_count = 0u;
    g_display_fileman_backend_edit_commit_count = 0u;
    g_display_fileman_last_status = 0u;
    g_display_fileman_last_write_status = 0u;
    g_display_fileman_last_delete_status = 0u;
    g_display_fileman_last_mutation_status = 0u;
    g_display_fileman_delete_armed = 0u;
    g_display_fileman_edit_mode = 0u;
    g_display_fileman_edit_bytes = 0u;
    display64_fileman_set_path("/APPS");
    display64_fileman_zero(g_display_fileman_delete_path, sizeof(g_display_fileman_delete_path));
    display64_fileman_zero(g_display_fileman_edit_buffer, sizeof(g_display_fileman_edit_buffer));
    display64_fileman_zero(g_display_fileman_entries, sizeof(g_display_fileman_entries));
    display64_fileman_zero(g_display_settings_config, sizeof(g_display_settings_config));
    display64_fileman_zero(g_display_settings_diag, sizeof(g_display_settings_diag));
    display64_settings_load_once();
#endif
    for (window_index = 0u; window_index < DISPLAY64_WM_MAX_WINDOWS; ++window_index)
    {
        g_display_windows[window_index].handle = 0u;
        g_display_windows[window_index].visible = 0u;
        g_display_windows[window_index].focused = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_display_windows[window_index].minimized = 0u;
#endif
        g_display_windows[window_index].z = 0u;
    }
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    display64_configure_console_layout();
#endif
#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
    display64_compositor_init_back_buffer();
#endif
}

u32 display64_draw_marker(u32 display_capability_handle, u32 x, u32 y, u32 rgb, u32 owner_id)
{
    volatile u32 *framebuffer;
    u32 endpoint;
    u32 draw_width;
    u32 draw_height;
    u32 row;
    u32 column;
    u32 pixel;
    u32 token = 2166136261u;
    u32 drawn = 0u;

    endpoint = capability64_route(display_capability_handle, CAPABILITY64_RIGHT_SEND, owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_DISPLAY))
    {
        return display64_deny();
    }

    if (!display64_has_framebuffer())
    {
        ++g_display_unavailable_count;
        g_display_last_token = 0u;
        return 0u;
    }

    if ((x >= g_display_boot_info->framebuffer_width)
        || (y >= g_display_boot_info->framebuffer_height))
    {
        return display64_deny();
    }

    draw_width = display64_min_u32(DISPLAY64_MARKER_WIDTH, g_display_boot_info->framebuffer_width - x);
    draw_height = display64_min_u32(DISPLAY64_MARKER_HEIGHT, g_display_boot_info->framebuffer_height - y);
    if ((draw_width == 0u) || (draw_height == 0u))
    {
        return display64_deny();
    }

    pixel = display64_make_pixel(rgb);
    framebuffer = display64_draw_buffer();
    display64_compositor_mark_dirty(x, y, draw_width, draw_height);
    for (row = 0u; row < draw_height; ++row)
    {
        u64 base_pixel = ((u64)(y + row) * (u64)g_display_boot_info->framebuffer_pixels_per_scanline) + (u64)x;
        u64 byte_offset = base_pixel * 4ull;

        if ((byte_offset + ((u64)draw_width * 4ull)) > g_display_boot_info->framebuffer_bytes)
        {
            return display64_deny();
        }

        for (column = 0u; column < draw_width; ++column)
        {
            u32 marker_pixel = pixel;
            if (((row + column) & 1u) != 0u)
            {
                marker_pixel ^= 0x00202020u;
            }

            framebuffer[base_pixel + column] = marker_pixel;
            token = display64_mix_token(token, marker_pixel ^ (u32)(base_pixel + column));
            ++drawn;
        }
    }

    ++g_display_draw_count;
    g_display_pixel_count += drawn;
    g_display_last_token = token;
    (void)display64_compositor_present();
    return drawn;
}

u32 display64_clear_text_panel(u32 display_capability_handle, u32 owner_id)
{
    u32 endpoint;
    u32 token = 2166136261u;
    u32 drawn;

    endpoint = capability64_route(display_capability_handle, CAPABILITY64_RIGHT_SEND, owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_DISPLAY))
    {
        return display64_deny();
    }

    if (!display64_has_framebuffer())
    {
        ++g_display_unavailable_count;
        g_display_last_token = 0u;
        return 0u;
    }

    drawn = display64_clear_rect(
        DISPLAY64_PANEL_X,
        DISPLAY64_PANEL_Y,
        DISPLAY64_PANEL_WIDTH,
        DISPLAY64_PANEL_HEIGHT,
        DISPLAY64_PANEL_RGB,
        &token);

    if (drawn == 0u)
    {
        g_display_last_token = 0u;
        return 0u;
    }

    ++g_display_draw_count;
    ++g_display_clear_count;
    g_display_pixel_count += drawn;
    g_display_last_token = token;
    g_display_text_x = DISPLAY64_TEXT_START_X;
    g_display_text_y = DISPLAY64_TEXT_START_Y;
    g_display_console_line_dirty = 0u;
    (void)display64_compositor_present();
    return drawn;
}

u32 display64_write_text(u32 display_capability_handle, u64 input_address, u32 byte_count, u32 owner_id)
{
    const u8 *bytes;
    u32 endpoint;
    u32 token = 2166136261u;
    u32 drawn = 0u;

    endpoint = capability64_route(display_capability_handle, CAPABILITY64_RIGHT_SEND, owner_id);
    if (endpoint != services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_DISPLAY))
    {
        return display64_deny();
    }

    if ((byte_count == 0u) || (byte_count > DISPLAY64_MAX_TEXT_BYTES)
        || !display64_address_readable(input_address, byte_count))
    {
        return display64_deny();
    }

    if (!display64_has_framebuffer())
    {
        ++g_display_unavailable_count;
        g_display_last_token = 0u;
        return 0u;
    }

    bytes = (const u8 *)input_address;
    drawn = display64_render_text_bytes(bytes, byte_count, &token, 0u);

    ++g_display_draw_count;
    ++g_display_text_write_count;
    g_display_text_byte_count += byte_count;
    g_display_pixel_count += drawn;
    g_display_last_token = token;
    (void)display64_compositor_present();
    return byte_count;
}

u32 display64_write_boot_diagnostics(
    u32 xhci_found,
    u32 xhci_handoff,
    u32 xhci_usb2_ports,
    u32 xhci_hid_device,
    u32 xhci_error,
    u32 usb_uhci_count,
    u32 usb_ohci_count,
    u32 usb_ehci_count,
    u32 usb_xhci_count,
    u32 ps2_present,
    u32 ps2_enabled,
    u32 ps2_scanning,
    u32 ps2_status,
    u32 ps2_config,
    u32 ps2_ack,
    u32 keyboard_scancodes,
    u32 keyboard_pending,
    u32 keyboard_last_scancode,
    u32 i2c_hid_present)
{
    char text[256];
    u32 cursor = 0u;
    u32 token = 2166136261u;
    u32 panel_width;
    u32 x;
    u32 y;
    u32 drawn;

    if (!display64_has_framebuffer())
    {
        ++g_display_unavailable_count;
        return 0u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_desktop_active != 0u)
    {
        ++g_display_input_diag_suppressed_count;
        return 1u;
    }
#endif

    panel_width = display64_min_u32(DISPLAY64_DIAG_PANEL_WIDTH, g_display_boot_info->framebuffer_width);
    x = (g_display_boot_info->framebuffer_width > (panel_width + DISPLAY64_DIAG_MARGIN))
        ? (g_display_boot_info->framebuffer_width - panel_width - DISPLAY64_DIAG_MARGIN)
        : 0u;
    y = (g_display_boot_info->framebuffer_height > (DISPLAY64_DIAG_PANEL_HEIGHT + DISPLAY64_DIAG_MARGIN))
        ? DISPLAY64_DIAG_MARGIN
        : 0u;

    cursor = display64_diag_append_text(text, cursor, sizeof(text), "INPUT DIAG\n");
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "XHCI ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), xhci_found);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " HANDOFF ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), xhci_handoff);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " HID ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), xhci_hid_device);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "USB2 ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), xhci_usb2_ports);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " ERR ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), xhci_error);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "HCI U/O/E/X ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), usb_uhci_count);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '/');
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), usb_ohci_count);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '/');
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), usb_ehci_count);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '/');
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), usb_xhci_count);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "PS2 ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), ps2_present);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " EN ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), ps2_enabled);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " SCAN ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), ps2_scanning);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "ST ");
    cursor = display64_diag_append_hex_u32(text, cursor, sizeof(text), ps2_status);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " CFG ");
    cursor = display64_diag_append_hex_u32(text, cursor, sizeof(text), ps2_config);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "ACK ");
    cursor = display64_diag_append_hex_u32(text, cursor, sizeof(text), ps2_ack);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " I2C ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), i2c_hid_present);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "KEYS ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), keyboard_scancodes);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " PEND ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), keyboard_pending);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " LAST ");
    cursor = display64_diag_append_hex_u32(text, cursor, sizeof(text), keyboard_last_scancode);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');

    drawn = display64_clear_rect(
        x,
        y,
        panel_width,
        display64_min_u32(DISPLAY64_DIAG_PANEL_HEIGHT, g_display_boot_info->framebuffer_height - y),
        DISPLAY64_DIAG_RGB,
        &token);
    drawn += display64_render_text_at(
        (const u8 *)text,
        cursor,
        x + 8u,
        y + 8u,
        DISPLAY64_DIAG_TEXT_RGB,
        &token);

    ++g_display_draw_count;
    ++g_display_text_write_count;
    g_display_text_byte_count += cursor;
    g_display_pixel_count += drawn;
    g_display_last_token = token;
    (void)display64_compositor_present();
    return cursor;
}

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
    u32 x_position,
    u32 y_position,
    u32 buttons,
    u32 ps2_raw_byte,
    u32 ps2_bad_starts,
    u32 xhci_keyboard_endpoint,
    u32 xhci_keyboard_pending,
    u32 xhci_keyboard_reports,
    u32 xhci_mouse_endpoint,
    u32 xhci_mouse_pending,
    u32 xhci_mouse_reports,
    u32 xhci_live_enabled,
    u32 i2c_keyboard_found,
    u32 i2c_keyboard_reports,
    u32 i2c_keyboard_error,
    u32 i2c_pointer_found,
    u32 i2c_pointer_reports,
    u32 i2c_pointer_error)
{
    char text[384];
    u32 cursor = 0u;
    u32 token = 2166136261u;
    u32 panel_width;
    u32 panel_height;
    u32 x;
    u32 y;
    u32 drawn;

    if (!display64_has_framebuffer())
    {
        ++g_display_unavailable_count;
        return 0u;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (g_display_desktop_active != 0u)
    {
        ++g_display_mouse_diag_suppressed_count;
        return 1u;
    }
#endif

    panel_width = display64_min_u32(DISPLAY64_MOUSE_DIAG_PANEL_WIDTH, g_display_boot_info->framebuffer_width);
    panel_height = display64_min_u32(DISPLAY64_MOUSE_DIAG_PANEL_HEIGHT, g_display_boot_info->framebuffer_height);
    x = (g_display_boot_info->framebuffer_width > (panel_width + DISPLAY64_DIAG_MARGIN))
        ? (g_display_boot_info->framebuffer_width - panel_width - DISPLAY64_DIAG_MARGIN)
        : 0u;
    y = (g_display_boot_info->framebuffer_height > (DISPLAY64_DIAG_PANEL_HEIGHT + DISPLAY64_DIAG_PANEL_HEIGHT + DISPLAY64_DIAG_MARGIN + 8u))
        ? (DISPLAY64_DIAG_MARGIN + DISPLAY64_DIAG_PANEL_HEIGHT + 8u)
        : 0u;

    cursor = display64_diag_append_text(text, cursor, sizeof(text), "PS2-MOUSE INIT ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), init_done);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " ACK ");
    cursor = display64_diag_append_hex_u32(text, cursor, sizeof(text), ack);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "A8 ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), aux_enabled);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " CFG ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), config_read);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '/');
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), config_write);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " IRQ12 ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), irq12_configured);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " F4 ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), enable_command);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "IRQ ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), irq_count);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " PACKETS ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), packet_count);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " PEND ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), pending_count);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "XY ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), x_position);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), ',');
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), y_position);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " BND ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), g_display_boot_info->framebuffer_width);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), ',');
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), g_display_boot_info->framebuffer_height);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " BTN ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), buttons);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "RAW ");
    cursor = display64_diag_append_hex_u32(text, cursor, sizeof(text), ps2_raw_byte);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " BAD ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), ps2_bad_starts);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "USB KEP ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), xhci_keyboard_endpoint);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " KP ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), xhci_keyboard_pending);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " KR ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), xhci_keyboard_reports);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "USB MEP ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), xhci_mouse_endpoint);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " MP ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), xhci_mouse_pending);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " MR ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), xhci_mouse_reports);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " LIVE ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), xhci_live_enabled);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "I2C KBD ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), i2c_keyboard_found);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " IR ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), i2c_keyboard_reports);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " ERR ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), i2c_keyboard_error);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');
    cursor = display64_diag_append_text(text, cursor, sizeof(text), "I2C TOUCH ");
    cursor = display64_diag_append_bool(text, cursor, sizeof(text), i2c_pointer_found);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " PR ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), i2c_pointer_reports);
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " ERR ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), i2c_pointer_error);
    cursor = display64_diag_append_char(text, cursor, sizeof(text), '\n');

    drawn = display64_clear_rect(
        x,
        y,
        panel_width,
        display64_min_u32(panel_height, g_display_boot_info->framebuffer_height - y),
        DISPLAY64_MOUSE_DIAG_RGB,
        &token);
    drawn += display64_render_text_at(
        (const u8 *)text,
        cursor,
        x + 8u,
        y + 8u,
        DISPLAY64_MOUSE_DIAG_TEXT_RGB,
        &token);

    ++g_display_draw_count;
    ++g_display_text_write_count;
    g_display_text_byte_count += cursor;
    g_display_pixel_count += drawn;
    g_display_last_token = token;
    (void)display64_compositor_present();
    return cursor;
}

u32 display64_write_console_stream(u64 input_address, u32 byte_count)
{
    const u8 *bytes;
    u32 token = 2166136261u;
    u32 drawn;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if ((byte_count > DISPLAY64_MAX_CONSOLE_BYTES)
        || !display64_address_readable(input_address, byte_count))
    {
        return DISPLAY64_INVALID_RESULT;
    }

    if (!display64_has_framebuffer())
    {
        ++g_display_unavailable_count;
        g_display_last_token = 0u;
        return 0u;
    }

    bytes = (const u8 *)input_address;
    display64_console_replay_append(bytes, byte_count);
    drawn = display64_render_text_bytes(bytes, byte_count, &token, 1u);

    ++g_display_draw_count;
    ++g_display_text_write_count;
    ++g_display_console_write_count;
    g_display_text_byte_count += byte_count;
    g_display_console_byte_count += byte_count;
    g_display_pixel_count += drawn;
    g_display_last_token = token;
    (void)display64_compositor_present();
    return byte_count;
}

u32 display64_write_console_stream_kernel(const u8 *input, u32 byte_count)
{
    u32 token = 2166136261u;
    u32 drawn;

    if (byte_count == 0u)
    {
        return 0u;
    }

    if ((input == 0) || (byte_count > DISPLAY64_MAX_CONSOLE_BYTES))
    {
        return DISPLAY64_INVALID_RESULT;
    }

    if (!display64_has_framebuffer())
    {
        ++g_display_unavailable_count;
        g_display_last_token = 0u;
        return 0u;
    }

    display64_console_replay_append(input, byte_count);
    drawn = display64_render_text_bytes(input, byte_count, &token, 1u);

    ++g_display_draw_count;
    ++g_display_text_write_count;
    ++g_display_console_write_count;
    g_display_text_byte_count += byte_count;
    g_display_console_byte_count += byte_count;
    g_display_pixel_count += drawn;
    g_display_last_token = token;
    (void)display64_compositor_present();
    return byte_count;
}

u32 display64_available(void)
{
    return display64_has_framebuffer() ? 1u : 0u;
}

u32 display64_width(void)
{
    return display64_has_framebuffer() ? g_display_boot_info->framebuffer_width : 0u;
}

u32 display64_height(void)
{
    return display64_has_framebuffer() ? g_display_boot_info->framebuffer_height : 0u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_pixels_per_scanline(void)
{
    return display64_has_framebuffer() ? g_display_boot_info->framebuffer_pixels_per_scanline : 0u;
}

u32 display64_framebuffer_format(void)
{
    return display64_has_framebuffer() ? g_display_boot_info->framebuffer_format : DISPLAY64_INVALID_RESULT;
}

u32 display64_framebuffer_base_low(void)
{
    return display64_has_framebuffer() ? (u32)g_display_boot_info->framebuffer_base : 0u;
}

u32 display64_framebuffer_base_high(void)
{
    return display64_has_framebuffer() ? (u32)(g_display_boot_info->framebuffer_base >> 32) : 0u;
}

u32 display64_framebuffer_bytes_low(void)
{
    return display64_has_framebuffer() ? (u32)g_display_boot_info->framebuffer_bytes : 0u;
}

u32 display64_framebuffer_required_bytes_low(void)
{
    return display64_has_framebuffer() ? (u32)display64_required_framebuffer_bytes() : 0u;
}

u32 display64_framebuffer_required_bytes_high(void)
{
    return display64_has_framebuffer() ? (u32)(display64_required_framebuffer_bytes() >> 32) : 0u;
}

u32 display64_framebuffer_stride_ok(void)
{
    return g_display_stride_ok;
}

u32 display64_framebuffer_bounds_ok(void)
{
    return g_display_bounds_ok;
}

u32 display64_text_scale(void)
{
    return g_display_text_scale;
}

u32 display64_console_viewport_x(void)
{
    return g_display_console_x;
}

u32 display64_console_viewport_y(void)
{
    return g_display_console_y;
}

u32 display64_console_viewport_w(void)
{
    return display64_console_viewport_width();
}

u32 display64_console_viewport_h(void)
{
    return display64_console_viewport_height();
}

u32 display64_console_columns(void)
{
    u32 advance = display64_font_advance();

    return (advance != 0u) ? (display64_console_viewport_width() / advance) : 0u;
}

u32 display64_console_rows(void)
{
    u32 advance = display64_line_advance();

    return (advance != 0u) ? (display64_console_viewport_height() / advance) : 0u;
}

u32 display64_console_fit(void)
{
    return g_display_console_fit;
}

u32 display64_readable(void)
{
    return g_display_readable;
}

u32 display64_console_clip_count(void)
{
    return g_display_console_clip_count;
}

u32 display64_layout_token(void)
{
    return g_display_layout_token;
}
#endif

u32 display64_compositor_init_done(void)
{
    return g_display_compositor_active;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_compositor_direct_mode(void)
{
    return g_display_compositor_direct_mode;
}
#endif

u32 display64_compositor_present_count(void)
{
    return g_display_compositor_present_count;
}

u32 display64_compositor_cursor_count(void)
{
    return g_display_compositor_cursor_count;
}

u32 display64_direct_cursor_count(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_display_direct_cursor_count;
#else
    return 0u;
#endif
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_ui_polish_token(void)
{
    u32 token = 2166136261u;

    token = display64_mix_token(token, DISPLAY64_UI_STYLE_GENERATION);
    token = display64_mix_token(token, DISPLAY64_RGB_DESKTOP_BG);
    token = display64_mix_token(token, DISPLAY64_RGB_BAR_BG);
    token = display64_mix_token(token, DISPLAY64_RGB_SURFACE);
    token = display64_mix_token(token, DISPLAY64_RGB_SURFACE_HIGH);
    token = display64_mix_token(token, DISPLAY64_RGB_ACCENT);
    token = display64_mix_token(token, g_display_compositor_active);
    token = display64_mix_token(token, display64_compositor_direct_mode());
    token = display64_mix_token(token, g_display_font_active);
    token = display64_mix_token(token, g_display_wm_active);
    token = display64_mix_token(token, g_display_desktop_active);
    token = display64_mix_token(token, g_display_desktop_taskbar_count);
    token = display64_mix_token(token, g_display_desktop_launcher_count);
    token = display64_mix_token(token, g_display_wm_window_count);
    token = display64_mix_token(token, g_display_desktop_product_layout);
    token = display64_mix_token(token, g_display_product_startup_minimized_count);
    token = display64_mix_token(token, g_display_gui_keyboard_open_count);
    token = display64_mix_token(token, g_display_settings_readiness_strip_count);
    token = display64_mix_token(token, g_display_input_diag_suppressed_count);
    token = display64_mix_token(token, g_display_mouse_diag_suppressed_count);
    token = display64_mix_token(token, display64_product_input_ready_internal());
    token = display64_mix_token(token, mmio64_nvme_fat_located());
    token = display64_mix_token(token, hardware64_registry_network_device_count());
    return token;
}

u32 display64_product_layout_active(void)
{
    return g_display_desktop_product_layout;
}

u32 display64_product_display_ready(void)
{
    return display64_readable();
}

u32 display64_product_input_ready(void)
{
    return display64_product_input_ready_internal();
}

u32 display64_product_storage_ready(void)
{
    return mmio64_nvme_fat_located();
}

u32 display64_product_network_ready(void)
{
    return (hardware64_registry_network_device_count() != 0u) ? 1u : 0u;
}

u32 display64_product_startup_minimized_count(void)
{
    return g_display_product_startup_minimized_count;
}
#endif

u32 display64_cursor_visible(void)
{
    return (g_display_compositor_cursor_drawn_valid != 0u) ? 1u : 0u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_cursor_x(void)
{
    return g_display_compositor_cursor_x;
}

u32 display64_cursor_y(void)
{
    return g_display_compositor_cursor_y;
}

u32 display64_cursor_buttons(void)
{
    return g_display_compositor_cursor_buttons;
}

u32 display64_cursor_saved_valid(void)
{
    return g_display_compositor_cursor_saved_valid;
}

u32 display64_cursor_drawn_valid(void)
{
    return g_display_compositor_cursor_drawn_valid;
}

u32 display64_cursor_in_bounds(void)
{
    u32 width = 0u;
    u32 height = 0u;

    display64_compositor_cursor_rect(g_display_compositor_cursor_x, g_display_compositor_cursor_y, &width, &height);
    return ((width != 0u) && (height != 0u)) ? 1u : 0u;
}

u32 display64_cursor_rect_w(void)
{
    u32 width = 0u;
    u32 height = 0u;

    display64_compositor_cursor_rect(g_display_compositor_cursor_x, g_display_compositor_cursor_y, &width, &height);
    return width;
}

u32 display64_cursor_rect_h(void)
{
    u32 width = 0u;
    u32 height = 0u;

    display64_compositor_cursor_rect(g_display_compositor_cursor_x, g_display_compositor_cursor_y, &width, &height);
    return height;
}

u32 display64_cursor_surface_ready(void)
{
    return (display64_has_framebuffer()
            && (g_display_stride_ok != 0u)
            && (g_display_bounds_ok != 0u))
        ? 1u
        : 0u;
}

u32 display64_cursor_framebuffer_format_supported(void)
{
    if ((g_display_boot_info == 0)
        || ((g_display_boot_info->bootstrap_flags & LIMITLESS_BOOT_FLAG_FRAMEBUFFER) == 0u))
    {
        return 0u;
    }

    return display64_format_supported(g_display_boot_info->framebuffer_format) ? 1u : 0u;
}

u32 display64_cursor_path_token(void)
{
    u32 token = 2166136261u;

    token = display64_mix_token(token, display64_cursor_surface_ready());
    token = display64_mix_token(token, display64_cursor_framebuffer_format_supported());
    token = display64_mix_token(token, g_display_compositor_active);
    token = display64_mix_token(token, display64_compositor_direct_mode());
    token = display64_mix_token(token, display64_cursor_visible());
    token = display64_mix_token(token, g_display_compositor_cursor_count);
    token = display64_mix_token(token, g_display_direct_cursor_count);
    token = display64_mix_token(token, g_display_compositor_cursor_x);
    token = display64_mix_token(token, g_display_compositor_cursor_y);
    token = display64_mix_token(token, g_display_compositor_cursor_buttons);
    token = display64_mix_token(token, display64_cursor_in_bounds());
    token = display64_mix_token(token, display64_cursor_rect_w());
    token = display64_mix_token(token, display64_cursor_rect_h());
    token = display64_mix_token(token, g_display_compositor_cursor_saved_valid);
    token = display64_mix_token(token, g_display_compositor_cursor_drawn_valid);
    return token;
}
#endif

u32 display64_font_init_done(void)
{
    return g_display_font_active;
}

u32 display64_font_glyph_count(void)
{
    return DISPLAY64_FONT_GLYPHS;
}

u32 display64_font_render_count(void)
{
    return g_display_font_render_count;
}

u32 display64_wm_init_done(void)
{
    return g_display_wm_active;
}

u32 display64_wm_window_created_count(void)
{
    return g_display_wm_window_count;
}

u32 display64_wm_focus_count(void)
{
    return g_display_wm_focus_count;
}

u32 display64_wm_present_count(void)
{
    return g_display_wm_present_count;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_wm_resize_count(void)
{
    return g_display_wm_resize_count;
}

u32 display64_wm_minimize_count(void)
{
    return g_display_wm_minimize_count;
}

u32 display64_wm_restore_count(void)
{
    return g_display_wm_restore_count;
}

u32 display64_wm_zorder_count(void)
{
    return g_display_wm_zorder_count;
}
#endif

u32 display64_desktop_init_done(void)
{
    return g_display_desktop_active;
}

u32 display64_desktop_taskbar_count(void)
{
    return g_display_desktop_taskbar_count;
}

u32 display64_desktop_launcher_count(void)
{
    return g_display_desktop_launcher_count;
}

u32 display64_desktop_terminal_count(void)
{
    return g_display_desktop_terminal_count;
}

u32 display64_desktop_fileman_count(void)
{
    return g_display_desktop_fileman_count;
}

u32 display64_desktop_settings_count(void)
{
    return g_display_desktop_settings_count;
}

u32 display64_desktop_assistant_count(void)
{
    return g_display_desktop_assistant_count;
}

u32 display64_pkg_settings_panel_count(void)
{
    return g_display_pkg_settings_panel_count;
}

u32 display64_identity_settings_panel_count(void)
{
    return g_display_identity_settings_panel_count;
}

u32 display64_identity_transport_settings_panel_count(void)
{
    return g_display_identity_transport_settings_panel_count;
}

u32 display64_account_settings_panel_count(void)
{
    return g_display_account_settings_panel_count;
}

u32 display64_cloud_settings_panel_count(void)
{
    return g_display_cloud_settings_panel_count;
}

u32 display64_cloud_fileman_status_count(void)
{
    return g_display_cloud_fileman_status_count;
}

u32 display64_ai_settings_panel_count(void)
{
    return g_display_ai_settings_panel_count;
}

u32 display64_installer_welcome_count(void)
{
    return g_display_installer_welcome_count;
}

u32 display64_installer_beginner_count(void)
{
    return g_display_installer_beginner_count;
}

u32 display64_installer_advanced_count(void)
{
    return g_display_installer_advanced_count;
}

u32 display64_installer_hardware_count(void)
{
    return g_display_installer_hardware_count;
}

u32 display64_installer_recommendation_count(void)
{
    return g_display_installer_recommendation_count;
}

u32 display64_installer_component_count(void)
{
    return g_display_installer_component_count;
}

u32 display64_installer_account_count(void)
{
    return g_display_installer_account_count;
}

u32 display64_installer_cloud_count(void)
{
    return g_display_installer_cloud_count;
}

u32 display64_installer_ai_count(void)
{
    return g_display_installer_ai_count;
}

u32 display64_installer_plan_count(void)
{
    return g_display_installer_plan_count;
}

u32 display64_installer_dryrun_count(void)
{
    return g_display_installer_dryrun_count;
}

u32 display64_gui_interactive(void)
{
    return g_display_gui_interactive;
}

u32 display64_gui_click_hittest(void)
{
    return g_display_gui_click_hittest;
}

u32 display64_gui_launcher_opened(void)
{
    return g_display_gui_launcher_opened;
}

u32 display64_gui_terminal_opened(void)
{
    return g_display_gui_terminal_opened;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_gui_input_diag_suppressed_count(void)
{
    return g_display_input_diag_suppressed_count;
}

u32 display64_gui_mouse_diag_suppressed_count(void)
{
    return g_display_mouse_diag_suppressed_count;
}
#endif

u32 display64_gui_drag_completed(void)
{
    return g_display_gui_drag_completed;
}

u32 display64_gui_keyboard_routed(void)
{
    return g_display_gui_keyboard_routed;
}

u32 display64_gui_close_completed(void)
{
    return g_display_gui_close_completed;
}

u32 display64_gui_taskbar_focus(void)
{
    return g_display_gui_taskbar_focus;
}

u32 display64_gui_fileman_opened(void)
{
    return g_display_gui_fileman_opened;
}

u32 display64_gui_settings_opened(void)
{
    return g_display_gui_settings_opened;
}

u32 display64_gui_installer_opened(void)
{
    return g_display_gui_installer_opened;
}

u32 display64_gui_assistant_opened(void)
{
    return g_display_gui_assistant_opened;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 display64_gui_keyboard_open_count(void)
{
    return g_display_gui_keyboard_open_count;
}

u32 display64_gui_right_click_count(void)
{
    return g_display_gui_right_click_count;
}

u32 display64_gui_context_menu_action_count(void)
{
    return g_display_context_menu_action_count;
}

u32 display64_gui_scroll_count(void)
{
    return g_display_gui_scroll_count;
}

u32 display64_gui_terminal_action_count(void)
{
    return g_display_terminal_action_count;
}

u32 display64_gui_terminal_scroll_count(void)
{
    return g_display_terminal_scroll_count;
}

u32 display64_gui_terminal_scroll_offset(void)
{
    return g_display_terminal_scroll_offset;
}

u32 display64_gui_terminal_selection_count(void)
{
    return g_display_terminal_selection_count;
}

u32 display64_gui_terminal_copy_count(void)
{
    return g_display_terminal_copy_count;
}

u32 display64_gui_terminal_copied_bytes(void)
{
    return g_display_terminal_copied_bytes;
}

u32 display64_gui_terminal_cursor_draw_count(void)
{
    return g_display_terminal_cursor_draw_count;
}

u32 display64_gui_fileman_action_count(void)
{
    return g_display_fileman_action_count;
}

u32 display64_gui_fileman_backend_refresh_count(void)
{
    return g_display_fileman_backend_refresh_count;
}

u32 display64_gui_fileman_backend_preview_count(void)
{
    return g_display_fileman_backend_preview_count;
}

u32 display64_gui_fileman_backend_open_dir_count(void)
{
    return g_display_fileman_backend_open_dir_count;
}

u32 display64_gui_fileman_backend_write_count(void)
{
    return g_display_fileman_backend_write_count;
}

u32 display64_gui_fileman_backend_write_denial_count(void)
{
    return g_display_fileman_backend_write_denial_count;
}

u32 display64_gui_fileman_backend_delete_count(void)
{
    return g_display_fileman_backend_delete_count;
}

u32 display64_gui_fileman_backend_delete_denial_count(void)
{
    return g_display_fileman_backend_delete_denial_count;
}

u32 display64_gui_fileman_backend_delete_confirm_count(void)
{
    return g_display_fileman_backend_delete_confirm_count;
}

u32 display64_gui_fileman_backend_mkdir_count(void)
{
    return g_display_fileman_backend_mkdir_count;
}

u32 display64_gui_fileman_backend_mkdir_denial_count(void)
{
    return g_display_fileman_backend_mkdir_denial_count;
}

u32 display64_gui_fileman_backend_copy_count(void)
{
    return g_display_fileman_backend_copy_count;
}

u32 display64_gui_fileman_backend_copy_denial_count(void)
{
    return g_display_fileman_backend_copy_denial_count;
}

u32 display64_gui_fileman_backend_rename_count(void)
{
    return g_display_fileman_backend_rename_count;
}

u32 display64_gui_fileman_backend_rename_denial_count(void)
{
    return g_display_fileman_backend_rename_denial_count;
}

u32 display64_gui_fileman_backend_move_count(void)
{
    return g_display_fileman_backend_move_count;
}

u32 display64_gui_fileman_backend_move_denial_count(void)
{
    return g_display_fileman_backend_move_denial_count;
}

u32 display64_gui_fileman_backend_edit_count(void)
{
    return g_display_fileman_backend_edit_count;
}

u32 display64_gui_fileman_backend_edit_commit_count(void)
{
    return g_display_fileman_backend_edit_commit_count;
}

u32 display64_gui_settings_action_count(void)
{
    return g_display_settings_action_count;
}

u32 display64_gui_settings_load_count(void)
{
    return g_display_settings_load_count;
}

u32 display64_gui_settings_save_count(void)
{
    return g_display_settings_save_count;
}

u32 display64_gui_settings_save_denial_count(void)
{
    return g_display_settings_save_denial_count;
}

u32 display64_gui_settings_export_count(void)
{
    return g_display_settings_export_count;
}

u32 display64_gui_settings_export_denial_count(void)
{
    return g_display_settings_export_denial_count;
}

u32 display64_gui_settings_hardware_panel_count(void)
{
    return g_display_settings_hardware_panel_count;
}

u32 display64_gui_settings_input_panel_count(void)
{
    return g_display_settings_input_panel_count;
}

u32 display64_gui_settings_readiness_strip_count(void)
{
    return g_display_settings_readiness_strip_count;
}

u32 display64_gui_fileman_storage_card_count(void)
{
    return g_display_fileman_storage_card_count;
}

u32 display64_gui_product_chrome_count(void)
{
    return g_display_product_chrome_count;
}

u32 display64_gui_settings_theme(void)
{
    return g_display_settings_theme;
}

u32 display64_gui_settings_pointer_speed(void)
{
    return g_display_settings_pointer_speed;
}

u32 display64_gui_settings_key_repeat(void)
{
    return g_display_settings_key_repeat;
}

u32 display64_gui_installer_action_count(void)
{
    return g_display_installer_action_count;
}

u32 display64_login_present_count(void)
{
    return g_display_login_present_count;
}

u32 display64_login_setup_present_count(void)
{
    return g_display_login_setup_present_count;
}

u32 display64_login_lock_present_count(void)
{
    return g_display_login_lock_present_count;
}

u32 display64_login_unlock_present_count(void)
{
    return g_display_login_unlock_present_count;
}

u32 display64_login_recovery_present_count(void)
{
    return g_display_login_recovery_present_count;
}

u32 display64_login_wait_visible_count(void)
{
    return g_display_login_wait_visible_count;
}

u32 display64_login_safe_path_count(void)
{
    return g_display_login_safe_path_count;
}

u32 display64_login_last_state(void)
{
    return g_display_login_last_state;
}
#endif

u32 display64_gui_unfocused_key_denied(void)
{
    return g_display_gui_unfocused_key_denied;
}

u32 display64_gui_no_ambient_input(void)
{
    return g_display_gui_no_ambient_input;
}

u32 display64_gui_no_ambient_display(void)
{
    return g_display_gui_no_ambient_display;
}

u32 display64_gui_no_ambient_fs(void)
{
    return g_display_gui_no_ambient_fs;
}

u32 display64_gui_mouse_x(void)
{
    return g_display_gui_mouse_x;
}

u32 display64_gui_mouse_y(void)
{
    return g_display_gui_mouse_y;
}

u32 display64_gui_target_window(void)
{
    return g_display_gui_target_window;
}

u32 display64_gui_target_region(void)
{
    return g_display_gui_target_region;
}

u32 display64_gui_focus_before(void)
{
    return g_display_gui_focus_before;
}

u32 display64_gui_focus_after(void)
{
    return g_display_gui_focus_after;
}

u32 display64_gui_z_before(void)
{
    return g_display_gui_z_before;
}

u32 display64_gui_z_after(void)
{
    return g_display_gui_z_after;
}

u32 display64_gui_key_target_window(void)
{
    return g_display_gui_key_target_window;
}

u32 display64_gui_unfocused_key_denial_count(void)
{
    return g_display_gui_unfocused_key_denial_count;
}

u32 display64_gui_input_path_token(void)
{
    return DISPLAY64_GUI_INPUT_PATH_TOKEN;
}

u32 display64_gui_display_path_token(void)
{
    return DISPLAY64_GUI_DISPLAY_PATH_TOKEN;
}

u32 display64_gui_fs_path_token(void)
{
    return DISPLAY64_GUI_FS_PATH_TOKEN;
}

u32 display64_draw_count(void)
{
    return g_display_draw_count;
}

u32 display64_pixel_count(void)
{
    return g_display_pixel_count;
}

u32 display64_denial_count(void)
{
    return g_display_denial_count;
}

u32 display64_unavailable_count(void)
{
    return g_display_unavailable_count;
}

u32 display64_last_token(void)
{
    return g_display_last_token;
}

u32 display64_text_write_count(void)
{
    return g_display_text_write_count;
}

u32 display64_text_byte_count(void)
{
    return g_display_text_byte_count;
}

u32 display64_clear_count(void)
{
    return g_display_clear_count;
}

u32 display64_console_write_count(void)
{
    return g_display_console_write_count;
}

u32 display64_console_byte_count(void)
{
    return g_display_console_byte_count;
}

u32 display64_console_line_clear_count(void)
{
    return g_display_console_line_clear_count;
}

u32 display64_console_wrap_count(void)
{
    return g_display_console_wrap_count;
}

u32 display64_console_scroll_count(void)
{
    return g_display_console_scroll_count;
}
