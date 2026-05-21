#ifndef LIMITLESS_FD_X64_H
#define LIMITLESS_FD_X64_H

#include "capability_x64.h"
#include "types.h"

#define FD64_TABLE_LIMIT 1024u
#define FD64_MAX_PROCESS_TABLES 16u
#define FD64_INVALID_FD 0xFFFFFFFFu
#define FD64_IO_ERROR 0xFFFFFFFFu
#define FD64_SEEK_ERROR 0xFFFFFFFFFFFFFFFFull

#define FD64_STDIN 0u
#define FD64_STDOUT 1u
#define FD64_STDERR 2u
#define FD64_FIRST_DYNAMIC 3u

#define FD64_TYPE_EMPTY 0x00000000u
#define FD64_TYPE_RAMFS_NODE 0x00000001u
#define FD64_TYPE_PIPE_READ 0x00000002u
#define FD64_TYPE_PIPE_WRITE 0x00000003u
#define FD64_TYPE_SOCKET 0x00000004u
#define FD64_TYPE_DEVICE 0x00000005u
#define FD64_TYPE_EVENTFD 0x00000006u

#define FD64_FLAG_O_CLOEXEC 0x00000001u
#define FD64_FLAG_O_NONBLOCK 0x00000002u

#define FD64_SEEK_SET 0u
#define FD64_SEEK_CUR 1u
#define FD64_SEEK_END 2u

#define FD64_STAT_NODE_UNKNOWN 0u
#define FD64_STAT_NODE_DIRECTORY 1u
#define FD64_STAT_NODE_FILE 2u
#define FD64_STAT_NODE_SYMLINK 3u
#define FD64_STAT_NODE_CHAR 4u

#define FD64_STAT_MODE_FIFO 0010000u
#define FD64_STAT_MODE_CHAR 0020000u
#define FD64_STAT_MODE_DIR 0040000u
#define FD64_STAT_MODE_FILE 0100000u
#define FD64_STAT_MODE_SYMLINK 0120000u
#define FD64_STAT_MODE_READ 0444u
#define FD64_STAT_MODE_WRITE 0222u
#define FD64_STAT_MODE_EXEC 0111u
#define FD64_STAT_MTIME_UNAVAILABLE 0ull

typedef struct fd_entry
{
    u32 fd_number;
    u32 capability_handle;
    u32 fd_type;
    u32 flags;
    u64 file_offset;
    u32 ref_count;
    u32 reserved;
} fd_entry_t;

typedef struct fd_table
{
    u32 pid;
    u32 owner_id;
    u32 live_count;
    u32 high_water_fd;
    u32 denial_count;
    u32 reserved;
    fd_entry_t entries[FD64_TABLE_LIMIT];
} fd_table_t;

typedef struct fd64_stat
{
    u64 size;
    u64 mtime;
    u64 blocks;
    u64 device_id;
    u64 inode;
    u32 mode;
    u32 fd_type;
    u32 node_type;
    u32 rights;
    u32 owner_id;
    u32 link_count;
    u32 block_size;
    u32 fd_number;
    u32 capability_handle;
} fd64_stat_t;

void fd64_init(void);
u32 fd64_init_process(
    u32 pid,
    u32 owner_id,
    u32 stdin_capability,
    u32 stdout_capability,
    u32 stderr_capability);
u32 fd64_release_process(u32 pid);
fd_table_t *fd64_table_for_process(u32 pid);
u32 fd64_alloc(u32 pid, u32 capability_handle, u32 fd_type, u32 flags);
u32 fd64_open_ramfs(u32 pid, const u8 *path, u32 path_byte_count, u32 flags, u32 mode);
u32 fd64_read(u32 pid, u32 fd_number, u8 *output, u32 byte_count);
u32 fd64_write(u32 pid, u32 fd_number, const u8 *input, u32 byte_count);
u32 fd64_read_at(u32 pid, u32 fd_number, u64 file_offset, u8 *output, u32 byte_count);
u32 fd64_write_at(u32 pid, u32 fd_number, u64 file_offset, const u8 *input, u32 byte_count);
u32 fd64_close(u32 pid, u32 fd_number);
u32 fd64_dup(u32 pid, u32 old_fd_number);
u32 fd64_dup_min(u32 pid, u32 old_fd_number, u32 min_fd_number);
u32 fd64_dup2(u32 pid, u32 old_fd_number, u32 new_fd_number);
u32 fd64_dup3(u32 pid, u32 old_fd_number, u32 new_fd_number, u32 flags);
u32 fd64_set_entry_flags(u32 pid, u32 fd_number, u32 flags);
u64 fd64_seek(u32 pid, u32 fd_number, s32 offset, u32 whence);
u32 fd64_stat(u32 pid, u32 fd_number, fd64_stat_t *stat_buf);
u32 fd64_fstat(u32 pid, u32 fd_number, fd64_stat_t *stat_buf);
u32 fd64_close_on_exec(u32 pid);
u32 fd64_free(u32 pid, u32 fd_number);
fd_entry_t *fd64_get(u32 pid, u32 fd_number);
u32 fd64_put(u32 pid, fd_entry_t *entry);
u32 fd64_live_count(u32 pid);
u32 fd64_high_water_fd(u32 pid);
u32 fd64_denial_count(u32 pid);
u32 fd64_entry_capability(u32 pid, u32 fd_number);
u32 fd64_entry_type(u32 pid, u32 fd_number);
u32 fd64_entry_flags(u32 pid, u32 fd_number);
u64 fd64_entry_offset(u32 pid, u32 fd_number);
u32 fd64_entry_ref_count(u32 pid, u32 fd_number);

#endif
