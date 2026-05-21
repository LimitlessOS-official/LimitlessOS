#ifndef LIMITLESS_WINDOWS_VFS_X64_H
#define LIMITLESS_WINDOWS_VFS_X64_H

#include "types.h"

#define WINDOWS_VFS64_AUDIT_CREATE_FILE 0x0055u
#define WINDOWS_VFS64_SHIM_ID_NTDLL 1u
#define WINDOWS_VFS64_MAX_PATH_BYTES 192u

typedef struct windows_vfs64_open_result
{
    u64 handle;
    u32 capability_handle;
    u32 status;
    u32 shim_id;
    u32 path_hash;
    u32 path_bytes;
    u32 backend_endpoint;
} windows_vfs64_open_result_t;

void windows_vfs64_init(void);
u32 windows_vfs64_open_path(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    u32 desired_access,
    u32 create_disposition,
    u32 create_options,
    windows_vfs64_open_result_t *out_result);
u32 windows_vfs64_open_count(void);
u32 windows_vfs64_denial_count(void);
u32 windows_vfs64_last_result(void);
u32 windows_vfs64_last_path_hash(void);
u32 windows_vfs64_last_path_bytes(void);
u32 windows_vfs64_last_shim_id(void);
u64 windows_vfs64_last_handle(void);
u32 windows_vfs64_last_capability(void);
u32 windows_vfs64_last_backend_endpoint(void);

#endif
