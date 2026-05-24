#ifndef LIMITLESS_WINDOWS_VFS_X64_H
#define LIMITLESS_WINDOWS_VFS_X64_H

#include "types.h"

#define WINDOWS_VFS64_AUDIT_CREATE_FILE 0x0055u
#define WINDOWS_VFS64_SHIM_ID_NTDLL 1u
#define WINDOWS_VFS64_SHIM_ID_KERNEL32 2u
#define WINDOWS_VFS64_SHIM_ID_MSVCRT 3u
#define WINDOWS_VFS64_SHIM_ID_UCRTBASE 4u
#define WINDOWS_VFS64_MAX_PATH_BYTES 192u
#define WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES 192u

#define WINDOWS_VFS64_ROUTE_UNKNOWN 0u
#define WINDOWS_VFS64_ROUTE_DOS_C 1u
#define WINDOWS_VFS64_ROUTE_UNC 2u
#define WINDOWS_VFS64_ROUTE_CONDRV 3u
#define WINDOWS_VFS64_ROUTE_NULL 4u
#define WINDOWS_VFS64_ROUTE_USERS 5u
#define WINDOWS_VFS64_ROUTE_TEMP 6u
#define WINDOWS_VFS64_PROFILE_DIR_MASK_COMPLETE 0x000000FFu
#define WINDOWS_VFS64_TEMP_DIR_MASK_COMPLETE 0x00000003u

#define WINDOWS_VFS64_DEVICE_NONE 0u
#define WINDOWS_VFS64_DEVICE_CONSOLE 1u
#define WINDOWS_VFS64_DEVICE_NULL 2u

typedef struct windows_vfs64_route_result
{
    u32 status;
    u32 route_type;
    u32 device_id;
    u32 unavailable;
    u32 path_hash;
    u32 path_bytes;
    u32 target_hash;
    u32 target_bytes;
    u32 backend_endpoint;
    u8 target_path[WINDOWS_VFS64_ROUTE_TARGET_MAX_BYTES];
} windows_vfs64_route_result_t;

typedef struct windows_vfs64_open_result
{
    u64 handle;
    u32 capability_handle;
    u32 status;
    u32 shim_id;
    u32 node_id;
    u32 path_hash;
    u32 path_bytes;
    u32 backend_endpoint;
} windows_vfs64_open_result_t;

void windows_vfs64_init(void);
u32 windows_vfs64_route_path(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    windows_vfs64_route_result_t *out_result);
u32 windows_vfs64_open_path(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    u32 desired_access,
    u32 create_disposition,
    u32 create_options,
    windows_vfs64_open_result_t *out_result);
u32 windows_vfs64_write_file(
    u32 pid,
    u64 handle,
    const u8 *input,
    u32 byte_count,
    u32 file_offset,
    u32 *bytes_written_out);
u32 windows_vfs64_read_file(
    u32 pid,
    u64 handle,
    u8 *output,
    u32 byte_count,
    u32 file_offset,
    u32 *bytes_read_out);
u32 windows_vfs64_delete_path(
    u32 pid,
    const u8 *path,
    u32 path_bytes);
u32 windows_vfs64_route_count(void);
u32 windows_vfs64_route_denial_count(void);
u32 windows_vfs64_last_route_type(void);
u32 windows_vfs64_last_target_hash(void);
u32 windows_vfs64_last_target_bytes(void);
u32 windows_vfs64_last_device_id(void);
u32 windows_vfs64_last_route_unavailable(void);
u32 windows_vfs64_open_count(void);
u32 windows_vfs64_denial_count(void);
u32 windows_vfs64_last_result(void);
u32 windows_vfs64_last_path_hash(void);
u32 windows_vfs64_last_path_bytes(void);
u32 windows_vfs64_last_shim_id(void);
u32 windows_vfs64_profile_create_count(void);
u32 windows_vfs64_profile_denial_count(void);
u32 windows_vfs64_profile_last_node(void);
u32 windows_vfs64_profile_last_dir_mask(void);
u32 windows_vfs64_temp_create_count(void);
u32 windows_vfs64_temp_write_count(void);
u32 windows_vfs64_temp_read_count(void);
u32 windows_vfs64_temp_delete_count(void);
u32 windows_vfs64_temp_denial_count(void);
u32 windows_vfs64_temp_last_node(void);
u32 windows_vfs64_temp_last_dir_mask(void);
u64 windows_vfs64_last_handle(void);
u32 windows_vfs64_last_capability(void);
u32 windows_vfs64_last_backend_endpoint(void);

#endif
