#ifndef LIMITLESS_UEFI_H
#define LIMITLESS_UEFI_H

#include "types.h"

typedef void *efi_handle_t;
typedef u64 efi_status_t;
typedef u16 efi_char16_t;
typedef u64 efi_uintn_t;
typedef u64 efi_physical_address_t;

#define EFI_SUCCESS 0ull

struct efi_guid
{
    u32 data1;
    u16 data2;
    u16 data3;
    u8 data4[8];
};

struct efi_configuration_table
{
    struct efi_guid vendor_guid;
    void *vendor_table;
};

struct efi_table_header
{
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc32;
    u32 reserved;
};

struct efi_simple_text_output_protocol;

typedef efi_status_t (*efi_text_reset_fn)(
    struct efi_simple_text_output_protocol *self,
    u8 extended_verification);

typedef efi_status_t (*efi_text_output_string_fn)(
    struct efi_simple_text_output_protocol *self,
    efi_char16_t *string);

struct efi_simple_text_output_protocol
{
    efi_text_reset_fn reset;
    efi_text_output_string_fn output_string;
    void *test_string;
    void *query_mode;
    void *set_mode;
    void *set_attribute;
    void *clear_screen;
    void *set_cursor_position;
    void *enable_cursor;
    void *mode;
};

typedef efi_status_t (*efi_locate_protocol_fn)(
    struct efi_guid *protocol,
    void *registration,
    void **interface);

typedef efi_status_t (*efi_handle_protocol_fn)(
    efi_handle_t handle,
    struct efi_guid *protocol,
    void **interface);

struct efi_memory_descriptor
{
    u32 type;
    u32 padding;
    u64 physical_start;
    u64 virtual_start;
    u64 number_of_pages;
    u64 attribute;
};

typedef efi_status_t (*efi_get_memory_map_fn)(
    efi_uintn_t *memory_map_size,
    struct efi_memory_descriptor *memory_map,
    efi_uintn_t *map_key,
    efi_uintn_t *descriptor_size,
    u32 *descriptor_version);

typedef efi_status_t (*efi_allocate_pages_fn)(
    u32 type,
    u32 memory_type,
    efi_uintn_t pages,
    efi_physical_address_t *memory);

typedef efi_status_t (*efi_exit_boot_services_fn)(
    efi_handle_t image_handle,
    efi_uintn_t map_key);

struct efi_boot_services
{
    struct efi_table_header header;
    void *raise_tpl;
    void *restore_tpl;
    efi_allocate_pages_fn allocate_pages;
    void *free_pages;
    efi_get_memory_map_fn get_memory_map;
    void *allocate_pool;
    void *free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    efi_handle_protocol_fn handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    efi_exit_boot_services_fn exit_boot_services;
    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;
    efi_locate_protocol_fn locate_protocol;
    void *install_multiple_protocol_interfaces;
    void *uninstall_multiple_protocol_interfaces;
    void *calculate_crc32;
    void *copy_mem;
    void *set_mem;
    void *create_event_ex;
};

struct efi_pixel_bitmask
{
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
    u32 reserved_mask;
};

struct efi_graphics_output_mode_information
{
    u32 version;
    u32 horizontal_resolution;
    u32 vertical_resolution;
    u32 pixel_format;
    struct efi_pixel_bitmask pixel_information;
    u32 pixels_per_scan_line;
};

struct efi_graphics_output_protocol_mode
{
    u32 max_mode;
    u32 mode;
    struct efi_graphics_output_mode_information *info;
    u64 size_of_info;
    u64 frame_buffer_base;
    u64 frame_buffer_size;
};

struct efi_graphics_output_protocol
{
    void *query_mode;
    void *set_mode;
    void *blt;
    struct efi_graphics_output_protocol_mode *mode;
};

struct efi_loaded_image_protocol
{
    u32 revision;
    efi_handle_t parent_handle;
    struct efi_system_table *system_table;
    efi_handle_t device_handle;
    void *file_path;
    void *reserved;
    u32 load_options_size;
    void *load_options;
    void *image_base;
    u64 image_size;
    u32 image_code_type;
    u32 image_data_type;
    void *unload;
};

struct efi_file_protocol;
struct efi_simple_file_system_protocol;

typedef efi_status_t (*efi_simple_file_system_open_volume_fn)(
    struct efi_simple_file_system_protocol *self,
    struct efi_file_protocol **root);

typedef efi_status_t (*efi_file_open_fn)(
    struct efi_file_protocol *self,
    struct efi_file_protocol **new_handle,
    efi_char16_t *file_name,
    u64 open_mode,
    u64 attributes);

typedef efi_status_t (*efi_file_close_fn)(
    struct efi_file_protocol *self);

typedef efi_status_t (*efi_file_read_fn)(
    struct efi_file_protocol *self,
    u64 *buffer_size,
    void *buffer);

struct efi_simple_file_system_protocol
{
    u64 revision;
    efi_simple_file_system_open_volume_fn open_volume;
};

struct efi_file_protocol
{
    u64 revision;
    efi_file_open_fn open;
    efi_file_close_fn close;
    void *delete_file;
    efi_file_read_fn read;
};

struct efi_system_table
{
    struct efi_table_header header;
    efi_char16_t *firmware_vendor;
    u32 firmware_revision;
    efi_handle_t console_in_handle;
    void *con_in;
    efi_handle_t console_out_handle;
    struct efi_simple_text_output_protocol *con_out;
    efi_handle_t standard_error_handle;
    struct efi_simple_text_output_protocol *std_err;
    void *runtime_services;
    struct efi_boot_services *boot_services;
    u64 number_of_table_entries;
    struct efi_configuration_table *configuration_table;
};

#endif
