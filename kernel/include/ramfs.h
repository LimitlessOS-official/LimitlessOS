#ifndef LIMITLESS_RAMFS_H
#define LIMITLESS_RAMFS_H

#include "types.h"

enum ramfs_node_type
{
    RAMFS_NODE_NONE = 0,
    RAMFS_NODE_DIRECTORY = 1,
    RAMFS_NODE_FILE = 2
};

struct ramfs_stat
{
    u32 node_type;
    u32 byte_length;
    u32 child_count;
};

void ramfs_init(void);
u32 ramfs_root_node(void);
int ramfs_node_exists(u32 node_id);
int ramfs_node_is_directory(u32 node_id);
const char *ramfs_node_name(u32 node_id);
int ramfs_open(u32 base_node_id, const u8 *path_bytes, u32 path_length, u32 *node_id_out);
int ramfs_create(
    u32 base_node_id,
    const u8 *path_bytes,
    u32 path_length,
    u32 node_type,
    u32 *node_id_out);
u32 ramfs_list(u32 node_id, u8 *destination_bytes, u32 byte_capacity);
u32 ramfs_read(u32 node_id, u32 file_offset, u8 *destination_bytes, u32 byte_capacity);
u32 ramfs_write(u32 node_id, u32 file_offset, const u8 *source_bytes, u32 byte_count);
int ramfs_stat(u32 node_id, struct ramfs_stat *stat_out);
u32 ramfs_format_stat(const struct ramfs_stat *stat, u8 *destination_bytes, u32 byte_capacity);
int ramfs_rename(
    u32 base_node_id,
    const u8 *source_path_bytes,
    u32 source_path_length,
    const u8 *destination_path_bytes,
    u32 destination_path_length);
int ramfs_move(
    u32 source_base_node_id,
    const u8 *source_path_bytes,
    u32 source_path_length,
    u32 destination_base_node_id,
    const u8 *destination_path_bytes,
    u32 destination_path_length);
int ramfs_delete(
    u32 base_node_id,
    const u8 *path_bytes,
    u32 path_length);
const u8 *ramfs_startup_script_bytes(u32 *byte_length_out);

#endif
