#include "display_x64.h"

#include "arch_build.h"
#include "capability_x64.h"
#include "launch_x64.h"
#include "pit.h"
#include "runtime_image_x64.h"
#include "services.h"
#include "services_x64.h"

#define DISPLAY64_MARKER_WIDTH 16u
#define DISPLAY64_MARKER_HEIGHT 8u
#define DISPLAY64_MAX_TEXT_BYTES 128u
#define DISPLAY64_MAX_CONSOLE_BYTES 512u
#define DISPLAY64_KERNEL_HIGH_BASE_HIGH32 0xFFFFFFFFu
#define DISPLAY64_KERNEL_HIGH_BASE_LOW32 0x80000000u
#define DISPLAY64_TEXT_START_X 24u
#define DISPLAY64_TEXT_START_Y 96u
#define DISPLAY64_FONT_WIDTH 5u
#define DISPLAY64_FONT_HEIGHT 7u
#define DISPLAY64_FONT_SCALE 2u
#define DISPLAY64_FONT_ADVANCE ((DISPLAY64_FONT_WIDTH + 1u) * DISPLAY64_FONT_SCALE)
#define DISPLAY64_LINE_ADVANCE ((DISPLAY64_FONT_HEIGHT + 2u) * DISPLAY64_FONT_SCALE)
#define DISPLAY64_TEXT_RGB 0x00F8FBFFu
#define DISPLAY64_PANEL_X DISPLAY64_TEXT_START_X
#define DISPLAY64_PANEL_Y DISPLAY64_TEXT_START_Y
#define DISPLAY64_PANEL_WIDTH 360u
#define DISPLAY64_PANEL_HEIGHT (DISPLAY64_LINE_ADVANCE + 4u)
#define DISPLAY64_PANEL_RGB 0x00101822u
#define DISPLAY64_CONSOLE_VIEWPORT_WIDTH 960u
#define DISPLAY64_CONSOLE_VIEWPORT_HEIGHT 648u
#define DISPLAY64_DIAG_MARGIN 16u
#define DISPLAY64_DIAG_PANEL_WIDTH 424u
#define DISPLAY64_DIAG_PANEL_HEIGHT 144u
#define DISPLAY64_DIAG_RGB 0x00182214u
#define DISPLAY64_DIAG_TEXT_RGB 0x00EAF7D7u
#define DISPLAY64_MOUSE_DIAG_PANEL_WIDTH 336u
#define DISPLAY64_MOUSE_DIAG_PANEL_HEIGHT 76u
#define DISPLAY64_MOUSE_DIAG_RGB 0x0014212Cu
#define DISPLAY64_MOUSE_DIAG_TEXT_RGB 0x00D9F7FFu
#define DISPLAY64_COMPOSITOR_MAX_SCANLINE 2048u
#define DISPLAY64_COMPOSITOR_MAX_HEIGHT 1200u
#define DISPLAY64_COMPOSITOR_CURSOR_WIDTH 12u
#define DISPLAY64_COMPOSITOR_CURSOR_HEIGHT 20u
#define DISPLAY64_COMPOSITOR_CURSOR_RGB 0x00F8FBFFu
#define DISPLAY64_COMPOSITOR_CURSOR_SHADOW_RGB 0x00040A10u
#define DISPLAY64_FONT_TRANSPARENT 0xFFFFFFFFu
#define DISPLAY64_FONT_SMALL 0u
#define DISPLAY64_FONT_NORMAL 1u
#define DISPLAY64_FONT_LARGE 2u
#define DISPLAY64_FONT_GLYPHS 256u
#define DISPLAY64_STATUS_BAR_HEIGHT 24u
#define DISPLAY64_DESKTOP_TASKBAR_HEIGHT 32u
#define DISPLAY64_DESKTOP_LAUNCHER_WIDTH 228u
#define DISPLAY64_DESKTOP_LAUNCHER_HEIGHT 132u
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
#define DISPLAY64_GUI_INPUT_PATH_TOKEN 0x494E5054u
#define DISPLAY64_GUI_DISPLAY_PATH_TOKEN 0x44495350u
#define DISPLAY64_GUI_FS_PATH_TOKEN 0x46535041u

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
static u32 g_display_text_x = DISPLAY64_TEXT_START_X;
static u32 g_display_text_y = DISPLAY64_TEXT_START_Y;
static u32 g_display_console_x = DISPLAY64_TEXT_START_X;
static u32 g_display_console_y = DISPLAY64_TEXT_START_Y;
static u32 g_display_console_w = DISPLAY64_CONSOLE_VIEWPORT_WIDTH;
static u32 g_display_console_h = DISPLAY64_CONSOLE_VIEWPORT_HEIGHT;
static u32 g_display_compositor_active = 0u;
static u32 g_display_compositor_present_count = 0u;
static u32 g_display_compositor_cursor_count = 0u;
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
static u32 g_display_wm_last_buttons = 0u;
static u32 g_display_desktop_active = 0u;
static u32 g_display_desktop_taskbar_count = 0u;
static u32 g_display_desktop_launcher_count = 0u;
static u32 g_display_desktop_terminal_count = 0u;
static u32 g_display_desktop_fileman_count = 0u;
static u32 g_display_desktop_settings_count = 0u;
static u32 g_display_desktop_fileman_handle = 0u;
static u32 g_display_desktop_settings_handle = 0u;
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
static u32 g_display_back_buffer[
    DISPLAY64_COMPOSITOR_MAX_SCANLINE * DISPLAY64_COMPOSITOR_MAX_HEIGHT];

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
    if (g_display_compositor_active != 0u)
    {
        return (volatile u32 *)g_display_back_buffer;
    }

    return display64_physical_framebuffer();
}

#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
static u32 display64_compositor_capacity_ok(void)
{
    u64 pixels;

    if (!display64_has_framebuffer()
        || (g_display_boot_info->framebuffer_pixels_per_scanline > DISPLAY64_COMPOSITOR_MAX_SCANLINE)
        || (g_display_boot_info->framebuffer_height > DISPLAY64_COMPOSITOR_MAX_HEIGHT))
    {
        return 0u;
    }

    pixels = (u64)g_display_boot_info->framebuffer_pixels_per_scanline
        * (u64)g_display_boot_info->framebuffer_height;
    return (pixels <= (u64)(DISPLAY64_COMPOSITOR_MAX_SCANLINE * DISPLAY64_COMPOSITOR_MAX_HEIGHT)) ? 1u : 0u;
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
        || (g_display_compositor_active == 0u)
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
}

static void display64_compositor_save_cursor_underlay(void)
{
    u32 width;
    u32 height;
    u32 row;
    u32 column;

    g_display_compositor_cursor_saved_valid = 0u;
    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
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
                g_display_back_buffer[base_pixel + column];
        }
    }

    g_display_compositor_cursor_saved_x = g_display_compositor_cursor_x;
    g_display_compositor_cursor_saved_y = g_display_compositor_cursor_y;
    g_display_compositor_cursor_saved_w = width;
    g_display_compositor_cursor_saved_h = height;
    g_display_compositor_cursor_saved_valid = 1u;
}

static void display64_compositor_draw_cursor(void)
{
    volatile u32 *framebuffer;
    u32 cursor_pixel;
    u32 shadow_pixel;
    u32 row;
    u32 column;

    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        return;
    }

    framebuffer = display64_physical_framebuffer();
    cursor_pixel = display64_make_pixel(
        (g_display_compositor_cursor_buttons != 0u) ? 0x00FFD66Bu : DISPLAY64_COMPOSITOR_CURSOR_RGB);
    shadow_pixel = display64_make_pixel(DISPLAY64_COMPOSITOR_CURSOR_SHADOW_RGB);

    for (row = 0u; row < DISPLAY64_COMPOSITOR_CURSOR_HEIGHT; ++row)
    {
        for (column = 0u; column < DISPLAY64_COMPOSITOR_CURSOR_WIDTH; ++column)
        {
            u64 pixel_index = 0ull;
            u32 draw_shadow;
            u32 draw_main;
            u32 x = g_display_compositor_cursor_x + column;
            u32 y = g_display_compositor_cursor_y + row;

            draw_main = (column == 0u)
                || ((row < 13u) && (column <= (row >> 1)))
                || ((row >= 12u) && (row <= 18u) && (column >= 4u) && (column <= 6u));
            draw_shadow = (draw_main == 0u)
                && (column > 0u)
                && (row > 0u)
                && (((column - 1u) == 0u)
                    || ((row < 14u) && ((column - 1u) <= ((row - 1u) >> 1u))));

            if ((draw_main == 0u) && (draw_shadow == 0u))
            {
                continue;
            }

            if (display64_pixel_index(x, y, &pixel_index) == 0)
            {
                continue;
            }

            framebuffer[pixel_index] = (draw_main != 0u) ? cursor_pixel : shadow_pixel;
        }
    }

    ++g_display_compositor_cursor_count;
}

#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
static void display64_compositor_init_back_buffer(void)
{
    volatile u32 *framebuffer;
    u64 pixels;
    u64 index;

    g_display_compositor_active = 0u;
    if (display64_compositor_capacity_ok() == 0u)
    {
        return;
    }

    framebuffer = display64_physical_framebuffer();
    pixels = (u64)g_display_boot_info->framebuffer_pixels_per_scanline
        * (u64)g_display_boot_info->framebuffer_height;
    for (index = 0ull; index < pixels; ++index)
    {
        g_display_back_buffer[index] = framebuffer[index];
    }

    g_display_compositor_active = 1u;
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

    if ((character >= (u8)'a') && (character <= (u8)'z'))
    {
        character = (u8)(character - ((u8)'a' - (u8)'A'));
    }

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

            for (scale_y = 0u; scale_y < DISPLAY64_FONT_SCALE; ++scale_y)
            {
                for (scale_x = 0u; scale_x < DISPLAY64_FONT_SCALE; ++scale_x)
                {
                    u64 index = 0ull;
                    u32 px = x + (column * DISPLAY64_FONT_SCALE) + scale_x;
                    u32 py = y + (row * DISPLAY64_FONT_SCALE) + scale_y;

                    if (display64_pixel_index(px, py, &index) == 0)
                    {
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

    if ((viewport_width == 0u) || (viewport_height <= DISPLAY64_LINE_ADVANCE))
    {
        return 0u;
    }

    framebuffer = display64_draw_buffer();
    scroll_height = viewport_height - DISPLAY64_LINE_ADVANCE;

    for (row = 0u; row < scroll_height; ++row)
    {
        u64 source_index = 0ull;
        u64 target_index = 0ull;
        u32 source_y = g_display_console_y + DISPLAY64_LINE_ADVANCE + row;
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
        DISPLAY64_LINE_ADVANCE,
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
    g_display_text_y += DISPLAY64_LINE_ADVANCE;

    if (display64_has_framebuffer()
        && ((g_display_text_y + (DISPLAY64_FONT_HEIGHT * DISPLAY64_FONT_SCALE))
            >= display64_console_viewport_bottom()))
    {
        if (track_console_wrap != 0u)
        {
            drawn += display64_scroll_console_viewport(token);
            if (display64_console_viewport_height() > DISPLAY64_LINE_ADVANCE)
            {
                g_display_text_y = display64_console_viewport_bottom() - DISPLAY64_LINE_ADVANCE;
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
        DISPLAY64_LINE_ADVANCE,
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

        if ((g_display_text_x + DISPLAY64_FONT_ADVANCE) >= display64_text_limit_x(clear_console_lines))
        {
            if (clear_console_lines != 0u)
            {
                ++g_display_console_wrap_count;
            }
            drawn += display64_text_newline(clear_console_lines, token);
        }

        drawn += (clear_console_lines != 0u) ? display64_clear_console_line(token) : 0u;
        drawn += display64_draw_glyph(character, g_display_text_x, g_display_text_y, pixel, token);
        g_display_text_x += DISPLAY64_FONT_ADVANCE;
    }

    return drawn;
}

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
            cursor_y += DISPLAY64_LINE_ADVANCE;
            continue;
        }

        if ((cursor_x + DISPLAY64_FONT_ADVANCE) >= g_display_boot_info->framebuffer_width)
        {
            cursor_x = base_x;
            cursor_y += DISPLAY64_LINE_ADVANCE;
        }

        if ((cursor_y + DISPLAY64_LINE_ADVANCE) >= g_display_boot_info->framebuffer_height)
        {
            break;
        }

        drawn += display64_draw_glyph(character, cursor_x, cursor_y, pixel, token);
        cursor_x += DISPLAY64_FONT_ADVANCE;
    }

    return drawn;
}

u32 display64_compositor_present(void)
{
    u32 x;
    u32 y;
    u32 width;
    u32 height;

    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        return 0u;
    }

    display64_compositor_restore_cursor_saved();
    if (g_display_compositor_dirty == 0u)
    {
        display64_compositor_save_cursor_underlay();
        display64_compositor_draw_cursor();
        ++g_display_compositor_present_count;
        return 1u;
    }

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
        return 16u;
    }

    if (font_size == DISPLAY64_FONT_NORMAL)
    {
        return 8u;
    }

    return 5u;
}

static u32 display64_font_height(u32 font_size)
{
    if (font_size == DISPLAY64_FONT_LARGE)
    {
        return 32u;
    }

    if (font_size == DISPLAY64_FONT_NORMAL)
    {
        return 16u;
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

static void display64_font_draw_status_bar(void)
{
    u32 y;
    u32 time_x;

    if (!display64_has_framebuffer())
    {
        return;
    }

    y = (g_display_boot_info->framebuffer_height > DISPLAY64_STATUS_BAR_HEIGHT)
        ? (g_display_boot_info->framebuffer_height - DISPLAY64_STATUS_BAR_HEIGHT)
        : 0u;
    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, DISPLAY64_STATUS_BAR_HEIGHT, 0x000D1520u);
    (void)display64_draw_font_text(8u, y + 4u, "LimitlessOS", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    time_x = (g_display_boot_info->framebuffer_width > 112u)
        ? (g_display_boot_info->framebuffer_width - 112u)
        : 0u;
    (void)display64_draw_font_text(time_x, y + 4u, "time --:--", DISPLAY64_FONT_NORMAL, 0x00B8C7D8u, DISPLAY64_FONT_TRANSPARENT);
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
    display64_compositor_fill_rect(12u, probe_y, 128u, 28u, 0x0014263Cu);
    display64_compositor_draw_rect(12u, probe_y, 128u, 28u, 0x0046D9A6u);
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

    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        return 0u;
    }

    old_x = g_display_compositor_cursor_x;
    old_y = g_display_compositor_cursor_y;
    display64_compositor_cursor_rect(old_x, old_y, &old_w, &old_h);
    display64_compositor_union_rect(old_x, old_y, old_w, old_h, &union_x, &union_y, &union_w, &union_h);

    display64_compositor_restore_cursor_saved();
    display64_compositor_clamp_cursor(&cursor_x, &cursor_y);
    g_display_compositor_cursor_x = cursor_x;
    g_display_compositor_cursor_y = cursor_y;
    g_display_compositor_cursor_buttons = buttons & 0x7u;
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

    display64_compositor_save_cursor_underlay();
    display64_compositor_draw_cursor();

    if ((union_w != 0u) && (union_h != 0u))
    {
        display64_compositor_present_back_buffer_rect(union_x, union_y, union_w, union_h);
        display64_compositor_draw_cursor();
    }

    ++g_display_compositor_present_count;
    return 1u;
}

void display64_font_probe(void)
{
    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        g_display_font_active = 0u;
        return;
    }

    g_display_font_active = 1u;
    display64_font_draw_status_bar();
    (void)display64_draw_font_text(24u, 40u, "DISPLAY ONLINE", DISPLAY64_FONT_LARGE, 0x0046D9A6u, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_compositor_present();
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
        if ((g_display_windows[index].visible != 0u) && (g_display_windows[index].z >= best_z))
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
    window->focused = 1u;
    window->z = g_display_wm_next_z++;
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
    if (handle == g_display_desktop_fileman_handle)
    {
        g_display_desktop_fileman_handle = 0u;
    }
    if (handle == g_display_desktop_settings_handle)
    {
        g_display_desktop_settings_handle = 0u;
    }
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

static void display64_wm_present_window(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 title_rgb;
    u32 close_x;
    u32 close_y;
    u32 content_y;
    u32 content_h;

    if ((window == 0) || (g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        return;
    }

    title_rgb = (window->focused != 0u) ? 0x003C8FCEu : 0x00303A45u;
    display64_compositor_fill_rect(window->x, window->y, window->width, window->height, 0x000B111Au);
    display64_compositor_draw_rect(window->x, window->y, window->width, window->height, 0x0061748Au);
    display64_compositor_fill_rect(
        window->x + DISPLAY64_WM_BORDER,
        window->y + DISPLAY64_WM_BORDER,
        window->width - (DISPLAY64_WM_BORDER * 2u),
        DISPLAY64_WM_TITLE_HEIGHT,
        title_rgb);
    content_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 2u;
    content_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + 4u))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - 4u)
        : 0u;
    display64_compositor_fill_rect(window->x + 2u, content_y, window->width - 4u, content_h, DISPLAY64_PANEL_RGB);
    (void)display64_draw_font_text(
        window->x + 8u,
        window->y + 7u,
        window->title,
        DISPLAY64_FONT_NORMAL,
        0x00F8FBFFu,
        DISPLAY64_FONT_TRANSPARENT);
    close_x = (window->width > 28u) ? (window->x + window->width - 22u) : window->x;
    close_y = window->y + 7u;
    display64_compositor_draw_rect(close_x, close_y, 14u, 14u, 0x00F8FBFFu);
    (void)display64_draw_font_text(close_x + 4u, close_y - 1u, "X", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    ++g_display_wm_present_count;
}

void display64_wm_probe(void)
{
    u32 width;
    u32 height;
    struct display64_window *window;

    if ((g_display_compositor_active == 0u) || !display64_has_framebuffer())
    {
        g_display_wm_active = 0u;
        return;
    }

    display64_compositor_fill_rect(0u, 0u, g_display_boot_info->framebuffer_width, g_display_boot_info->framebuffer_height, 0x00081218u);
    display64_font_draw_status_bar();
    width = display64_min_u32(920u, (g_display_boot_info->framebuffer_width > 64u) ? (g_display_boot_info->framebuffer_width - 64u) : g_display_boot_info->framebuffer_width);
    height = display64_min_u32(560u, (g_display_boot_info->framebuffer_height > 112u) ? (g_display_boot_info->framebuffer_height - 112u) : g_display_boot_info->framebuffer_height);
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

    display64_compositor_fill_rect(x, y, 28u, 24u, 0x00213A4Eu);
    display64_compositor_draw_rect(x, y, 28u, 24u, 0x007FD1FFu);
    for (row = 0u; row < 2u; ++row)
    {
        for (column = 0u; column < 3u; ++column)
        {
            display64_compositor_fill_rect(
                x + 6u + (column * 7u),
                y + 5u + (row * 8u),
                4u,
                4u,
                0x00F8FBFFu);
        }
    }
}

static void display64_desktop_draw_window_button(u32 x, u32 y, const char *title)
{
    display64_compositor_fill_rect(x, y, 116u, 24u, 0x00192633u);
    display64_compositor_draw_rect(x, y, 116u, 24u, 0x00426174u);
    (void)display64_draw_font_text(x + 8u, y + 4u, title, DISPLAY64_FONT_NORMAL, 0x00D9EAF6u, DISPLAY64_FONT_TRANSPARENT);
}

static void display64_desktop_draw_taskbar(void)
{
    u32 y = display64_desktop_taskbar_y();
    u32 clock_x;
    u32 button_x = DISPLAY64_DESKTOP_WINDOW_BUTTON_X;
    u32 index;

    if (!display64_has_framebuffer())
    {
        return;
    }

    display64_compositor_fill_rect(0u, y, g_display_boot_info->framebuffer_width, DISPLAY64_DESKTOP_TASKBAR_HEIGHT, 0x00091118u);
    display64_desktop_draw_launcher_button(DISPLAY64_DESKTOP_LAUNCHER_BUTTON_X, y + 4u);
    clock_x = (g_display_boot_info->framebuffer_width > 96u)
        ? (g_display_boot_info->framebuffer_width - 96u)
        : 0u;
    for (index = 0u; index < DISPLAY64_WM_MAX_WINDOWS; ++index)
    {
        if ((g_display_windows[index].visible != 0u)
            && ((button_x + DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH + 8u) < clock_x))
        {
            display64_desktop_draw_window_button(button_x, y + 4u, g_display_windows[index].title);
            button_x += DISPLAY64_DESKTOP_WINDOW_BUTTON_WIDTH + DISPLAY64_DESKTOP_WINDOW_BUTTON_GAP;
        }
    }
    display64_draw_label_value(clock_x, y + 8u, "T+", pit_get_uptime_seconds(), 0x00B8C7D8u);
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

static void display64_desktop_draw_launcher_panel(void)
{
    u32 panel_y = display64_desktop_launcher_panel_y();

    display64_compositor_fill_rect(12u, panel_y, DISPLAY64_DESKTOP_LAUNCHER_WIDTH, DISPLAY64_DESKTOP_LAUNCHER_HEIGHT, 0x00101A24u);
    display64_compositor_draw_rect(12u, panel_y, DISPLAY64_DESKTOP_LAUNCHER_WIDTH, DISPLAY64_DESKTOP_LAUNCHER_HEIGHT, 0x0046D9A6u);
    (void)display64_draw_font_text(24u, panel_y + 12u, "Apps", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    display64_compositor_fill_rect(28u, panel_y + 40u, 28u, 28u, 0x003C8FCEu);
    display64_compositor_fill_rect(28u, panel_y + 76u, 28u, 28u, 0x002DAA75u);
    display64_compositor_fill_rect(124u, panel_y + 40u, 28u, 28u, 0x00A9703Eu);
    (void)display64_draw_font_text(64u, panel_y + 46u, "Terminal", DISPLAY64_FONT_NORMAL, 0x00EAF7D7u, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(64u, panel_y + 82u, "Files", DISPLAY64_FONT_NORMAL, 0x00EAF7D7u, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(160u, panel_y + 46u, "Settings", DISPLAY64_FONT_NORMAL, 0x00EAF7D7u, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_desktop_launcher_count == 0u)
    {
        ++g_display_desktop_launcher_count;
    }
}

static void display64_desktop_draw_file_manager(u32 handle)
{
    struct display64_window *window = display64_wm_find_window(handle);
    u32 body_x;
    u32 body_y;
    u32 body_h;

    if (window == 0)
    {
        return;
    }

    display64_wm_present_window(handle);
    body_x = window->x + 10u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 12u;
    body_h = (window->height > (DISPLAY64_WM_TITLE_HEIGHT + 20u))
        ? (window->height - DISPLAY64_WM_TITLE_HEIGHT - 20u)
        : 0u;
    display64_compositor_draw_rect(body_x + 100u, body_y - 4u, 1u, body_h, 0x00426174u);
    (void)display64_draw_font_text(body_x, body_y, "RAMFS /", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 20u, "NVME FAT32", DISPLAY64_FONT_NORMAL, 0x00B8C7D8u, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 116u, body_y, "README.TXT", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 116u, body_y + 20u, "APPS/", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x + 116u, body_y + 40u, "NVME.TXT", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    if (g_display_desktop_fileman_count == 0u)
    {
        ++g_display_desktop_fileman_count;
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
    body_x = window->x + 10u;
    body_y = window->y + DISPLAY64_WM_TITLE_HEIGHT + 12u;
    (void)display64_draw_font_text(body_x, body_y, "Display", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    display64_draw_label_value(body_x, body_y + 18u, "W ", g_display_boot_info->framebuffer_width, 0x00B8C7D8u);
    display64_draw_label_value(body_x + 88u, body_y + 18u, "H ", g_display_boot_info->framebuffer_height, 0x00B8C7D8u);
    (void)display64_draw_font_text(body_x, body_y + 40u, "FB BGR", DISPLAY64_FONT_NORMAL, 0x00B8C7D8u, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 64u, "Storage RAMFS NVME", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 88u, "Network DHCP DNS HTTP", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    (void)display64_draw_font_text(body_x, body_y + 112u, "About LimitlessOS", DISPLAY64_FONT_NORMAL, 0x00F8FBFFu, DISPLAY64_FONT_TRANSPARENT);
    display64_draw_label_value(body_x, body_y + 130u, "Sectors ", g_display_boot_info->kernel_sector_count, 0x00B8C7D8u);
    if (g_display_desktop_settings_count == 0u)
    {
        ++g_display_desktop_settings_count;
    }
}

static void display64_desktop_draw_background(void)
{
    display64_compositor_fill_rect(
        0u,
        0u,
        g_display_boot_info->framebuffer_width,
        g_display_boot_info->framebuffer_height,
        0x00070E16u);
    display64_font_draw_status_bar();
}

static void display64_desktop_present_window_content(u32 handle)
{
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

    display64_wm_present_window(handle);
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

static void display64_desktop_redraw(void)
{
    if ((g_display_desktop_active == 0u)
        || (g_display_compositor_active == 0u)
        || !display64_has_framebuffer())
    {
        return;
    }

    display64_desktop_draw_background();
    display64_desktop_draw_windows_by_z();
    if (g_display_desktop_launcher_open != 0u)
    {
        display64_desktop_draw_launcher_panel();
    }
    display64_desktop_draw_taskbar();
    (void)display64_compositor_present();
}

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

static void display64_desktop_open_file_manager(void)
{
    if (display64_wm_find_window(g_display_desktop_fileman_handle) == 0)
    {
        u32 side_x = (g_display_boot_info->framebuffer_width > 368u)
            ? (g_display_boot_info->framebuffer_width - 344u)
            : 24u;
        u32 side_w = (g_display_boot_info->framebuffer_width > (side_x + 24u))
            ? display64_min_u32(320u, g_display_boot_info->framebuffer_width - side_x - 24u)
            : 160u;
        g_display_desktop_fileman_handle = display64_wm_create_window("File Manager", side_x, 64u, side_w, 210u);
    }
    display64_wm_focus_and_route_console(g_display_desktop_fileman_handle);
}

static void display64_desktop_open_settings(void)
{
    if (display64_wm_find_window(g_display_desktop_settings_handle) == 0)
    {
        u32 side_x = (g_display_boot_info->framebuffer_width > 368u)
            ? (g_display_boot_info->framebuffer_width - 344u)
            : 24u;
        u32 side_w = (g_display_boot_info->framebuffer_width > (side_x + 24u))
            ? display64_min_u32(320u, g_display_boot_info->framebuffer_width - side_x - 24u)
            : 160u;
        u32 settings_y = (g_display_boot_info->framebuffer_height < 620u) ? 250u : 310u;
        g_display_desktop_settings_handle = display64_wm_create_window("Settings", side_x, settings_y, side_w, 230u);
    }
    display64_wm_focus_and_route_console(g_display_desktop_settings_handle);
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
    g_display_desktop_launcher_open = 0u;
    display64_desktop_redraw();
}

void display64_desktop_probe(void)
{
    u32 side_x;
    u32 side_w;
    u32 file_y = 64u;
    u32 settings_y = 310u;
    struct display64_window *terminal;

    if ((g_display_compositor_active == 0u)
        || (g_display_wm_active == 0u)
        || !display64_has_framebuffer())
    {
        g_display_desktop_active = 0u;
        return;
    }

    display64_desktop_draw_background();

    side_x = (g_display_boot_info->framebuffer_width > 368u)
        ? (g_display_boot_info->framebuffer_width - 344u)
        : 24u;
    side_w = (g_display_boot_info->framebuffer_width > (side_x + 24u))
        ? display64_min_u32(320u, g_display_boot_info->framebuffer_width - side_x - 24u)
        : 160u;
    if (side_w < 160u)
    {
        side_w = 160u;
    }
    if (g_display_boot_info->framebuffer_height < 620u)
    {
        settings_y = 250u;
    }

    g_display_desktop_terminal_count = (g_display_wm_shell_handle != 0u) ? 1u : 0u;
    g_display_desktop_fileman_handle = display64_wm_create_window("File Manager", side_x, file_y, side_w, 210u);
    g_display_desktop_settings_handle = display64_wm_create_window("Settings", side_x, settings_y, side_w, 230u);
    g_display_desktop_launcher_open = 1u;

    display64_wm_present_window(g_display_wm_shell_handle);
    display64_desktop_draw_file_manager(g_display_desktop_fileman_handle);
    display64_desktop_draw_settings(g_display_desktop_settings_handle);
    display64_desktop_draw_launcher_panel();
    display64_desktop_draw_taskbar();

    display64_wm_focus_window(g_display_wm_shell_handle);
    display64_wm_present_window(g_display_wm_shell_handle);
    terminal = display64_wm_find_window(g_display_wm_shell_handle);
    display64_wm_configure_console(terminal);
    display64_desktop_draw_taskbar();
    g_display_desktop_active = (g_display_desktop_terminal_count != 0u)
        && (g_display_desktop_fileman_count != 0u)
        && (g_display_desktop_settings_count != 0u)
        && (g_display_desktop_taskbar_count != 0u)
        && (g_display_desktop_launcher_count != 0u)
        ? 1u
        : 0u;
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

    if (g_display_wm_dragging != 0u)
    {
        if (left != 0u)
        {
            u32 new_x = (x > g_display_wm_drag_offset_x) ? (x - g_display_wm_drag_offset_x) : 0u;
            u32 new_y = (y > g_display_wm_drag_offset_y) ? (y - g_display_wm_drag_offset_y) : 0u;
            z_before = display64_wm_window_z(g_display_wm_drag_handle);
            display64_wm_move_window(g_display_wm_drag_handle, new_x, new_y);
            display64_desktop_redraw();
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

    if (pressed != 0u)
    {
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
                u32 region = DISPLAY64_GUI_REGION_BODY;
                g_display_desktop_launcher_open = 0u;
                z_before = window->z;

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

    display64_gui_record_unfocused_keyboard_denial(focused_handle);
    return 0u;
}

void display64_init(const struct boot_info *boot_info)
{
    u32 window_index;

    g_display_boot_info = boot_info;
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
    g_display_text_x = DISPLAY64_TEXT_START_X;
    g_display_text_y = DISPLAY64_TEXT_START_Y;
    g_display_console_x = DISPLAY64_TEXT_START_X;
    g_display_console_y = DISPLAY64_TEXT_START_Y;
    g_display_console_w = DISPLAY64_CONSOLE_VIEWPORT_WIDTH;
    g_display_console_h = DISPLAY64_CONSOLE_VIEWPORT_HEIGHT;
    g_display_compositor_active = 0u;
    g_display_compositor_present_count = 0u;
    g_display_compositor_cursor_count = 0u;
    g_display_compositor_cursor_x = 32u;
    g_display_compositor_cursor_y = 32u;
    g_display_compositor_cursor_buttons = 0u;
    g_display_compositor_dirty = 0u;
    g_display_compositor_cursor_saved_valid = 0u;
    g_display_compositor_cursor_saved_x = 0u;
    g_display_compositor_cursor_saved_y = 0u;
    g_display_compositor_cursor_saved_w = 0u;
    g_display_compositor_cursor_saved_h = 0u;
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
    g_display_wm_last_buttons = 0u;
    g_display_desktop_active = 0u;
    g_display_desktop_taskbar_count = 0u;
    g_display_desktop_launcher_count = 0u;
    g_display_desktop_terminal_count = 0u;
    g_display_desktop_fileman_count = 0u;
    g_display_desktop_settings_count = 0u;
    g_display_desktop_fileman_handle = 0u;
    g_display_desktop_settings_handle = 0u;
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
    for (window_index = 0u; window_index < DISPLAY64_WM_MAX_WINDOWS; ++window_index)
    {
        g_display_windows[window_index].handle = 0u;
        g_display_windows[window_index].visible = 0u;
        g_display_windows[window_index].focused = 0u;
        g_display_windows[window_index].z = 0u;
    }
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
    u32 buttons)
{
    char text[192];
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
    cursor = display64_diag_append_text(text, cursor, sizeof(text), " BTN ");
    cursor = display64_diag_append_u32(text, cursor, sizeof(text), buttons);
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

u32 display64_compositor_init_done(void)
{
    return g_display_compositor_active;
}

u32 display64_compositor_present_count(void)
{
    return g_display_compositor_present_count;
}

u32 display64_compositor_cursor_count(void)
{
    return g_display_compositor_cursor_count;
}

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
