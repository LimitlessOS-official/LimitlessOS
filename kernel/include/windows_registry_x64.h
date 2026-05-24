#ifndef LIMITLESS_WINDOWS_REGISTRY_X64_H
#define LIMITLESS_WINDOWS_REGISTRY_X64_H

#include "types.h"

#define WINDOWS_REGISTRY64_MAX_PATH_BYTES 192u
#define WINDOWS_REGISTRY64_MAX_VALUE_NAME_BYTES 64u
#define WINDOWS_REGISTRY64_MAX_VALUE_DATA_BYTES 128u
#define WINDOWS_REGISTRY64_DYNAMIC_KEY_LIMIT 4u

#define WINDOWS_REGISTRY64_KEY_NONE 0u
#define WINDOWS_REGISTRY64_KEY_CODEPAGE 1u
#define WINDOWS_REGISTRY64_KEY_CURRENT_VERSION 2u
#define WINDOWS_REGISTRY64_KEY_ENVIRONMENT 3u
#define WINDOWS_REGISTRY64_KEY_DYNAMIC_BASE 16u

#define WINDOWS_REGISTRY64_VALUE_INFORMATION_PARTIAL 2u
#define WINDOWS_REGISTRY64_REG_SZ 1u
#define WINDOWS_REGISTRY64_DISPOSITION_OPENED 1u
#define WINDOWS_REGISTRY64_DISPOSITION_CREATED 2u

typedef struct windows_registry64_open_result
{
    u64 handle;
    u32 status;
    u32 key_id;
    u32 path_hash;
    u32 path_bytes;
    u32 created;
    u32 disposition;
} windows_registry64_open_result_t;

typedef struct windows_registry64_value_result
{
    u32 status;
    u32 key_id;
    u32 value_type;
    u32 data_bytes;
    u32 required_bytes;
    u32 value_hash;
    u32 data_hash;
    const u8 *data_ascii;
} windows_registry64_value_result_t;

void windows_registry64_init(void);
u32 windows_registry64_open_key(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    u32 desired_access,
    windows_registry64_open_result_t *out_result);
u32 windows_registry64_create_key(
    u32 pid,
    const u8 *path,
    u32 path_bytes,
    u32 desired_access,
    windows_registry64_open_result_t *out_result);
u32 windows_registry64_query_value(
    u32 pid,
    u64 handle,
    const u8 *value_name,
    u32 value_name_bytes,
    u32 information_class,
    windows_registry64_value_result_t *out_result);
u32 windows_registry64_open_count(void);
u32 windows_registry64_create_count(void);
u32 windows_registry64_query_count(void);
u32 windows_registry64_denial_count(void);
u32 windows_registry64_last_status(void);
u32 windows_registry64_last_key_id(void);
u32 windows_registry64_last_path_hash(void);
u32 windows_registry64_last_value_hash(void);
u32 windows_registry64_last_data_hash(void);
u32 windows_registry64_last_required_bytes(void);
u32 windows_registry64_live_dynamic_count(void);

#endif
