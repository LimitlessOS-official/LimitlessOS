#ifndef LIMITLESS_FS_X64_H
#define LIMITLESS_FS_X64_H

#include "types.h"

#define FS64_INVALID_HANDLE 0xFFFFFFFFu
#define FS64_RIGHT_LIST 0x00000100u
#define FS64_RIGHT_READ 0x00000200u
#define FS64_RIGHT_CREATE 0x00000400u
#define FS64_RIGHT_WRITE 0x00000800u
#define FS64_RIGHT_STAT 0x00001000u
#define FS64_RIGHT_RENAME 0x00002000u
#define FS64_RIGHT_DELETE 0x00004000u
#define FS64_RIGHT_DELEGATE 0x00008000u

void fs64_init(void);
u32 fs64_open(u32 base_capability_handle, u64 path_address, u32 path_byte_count, u32 owner_id);
u32 fs64_open_kernel(
    u32 base_capability_handle,
    const u8 *path,
    u32 path_byte_count,
    u32 owner_id);
u32 fs64_create(
    u32 base_capability_handle,
    u64 path_address,
    u32 path_byte_count,
    u32 node_type,
    u32 owner_id);
u32 fs64_create_kernel(
    u32 base_capability_handle,
    const u8 *path,
    u32 path_byte_count,
    u32 node_type,
    u32 owner_id);
u32 fs64_list(u32 node_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id);
u32 fs64_list_kernel(
    u32 node_capability_handle,
    u8 *output,
    u32 byte_capacity,
    u32 owner_id);
u32 fs64_read(
    u32 node_capability_handle,
    u64 output_address,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id);
u32 fs64_read_kernel(
    u32 node_capability_handle,
    u8 *output,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id);
u32 fs64_write(
    u32 node_capability_handle,
    u64 input_address,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id);
u32 fs64_write_kernel(
    u32 node_capability_handle,
    const u8 *input,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id);
u32 fs64_stat(u32 node_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id);
u32 fs64_stat_kernel(
    u32 node_capability_handle,
    u8 *output,
    u32 byte_capacity,
    u32 owner_id);
u32 fs64_revoke(u32 node_capability_handle, u32 owner_id);
u32 fs64_rename(
    u32 base_capability_handle,
    u64 path_address,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id);
u32 fs64_rename_kernel(
    u32 base_capability_handle,
    const u8 *path_pair,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id);
u32 fs64_move(
    u32 source_base_capability_handle,
    u32 destination_base_capability_handle,
    u64 path_address,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id);
u32 fs64_move_kernel(
    u32 source_base_capability_handle,
    u32 destination_base_capability_handle,
    const u8 *path_pair,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id);
u32 fs64_delete(
    u32 base_capability_handle,
    u64 path_address,
    u32 path_byte_count,
    u32 owner_id);
u32 fs64_delete_kernel(
    u32 base_capability_handle,
    const u8 *path,
    u32 path_byte_count,
    u32 owner_id);
u32 fs64_node_rights(u32 node_capability_handle, u32 owner_id);
u32 fs64_node_owner(u32 node_capability_handle, u32 owner_id);
u32 fs64_live_count(void);
u32 fs64_open_count(void);
u32 fs64_create_count(void);
u32 fs64_list_count(void);
u32 fs64_read_count(void);
u32 fs64_write_count(void);
u32 fs64_stat_count(void);
u32 fs64_revoke_count(void);
u32 fs64_denial_count(void);
u32 fs64_stale_denial_count(void);

#endif
