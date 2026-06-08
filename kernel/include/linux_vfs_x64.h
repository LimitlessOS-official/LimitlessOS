#ifndef LIMITLESS_LINUX_VFS_X64_H
#define LIMITLESS_LINUX_VFS_X64_H

#include "capability_x64.h"
#include "fd_x64.h"
#include "types.h"

#define LINUX_VFS64_MAX_MOUNTS 10u
#define LINUX_VFS64_MAX_PATH_BYTES 128u
#define LINUX_VFS64_INVALID_RESULT 0xFFFFFFFFu
#define LINUX_VFS64_DEVICE_HANDLE_TAG 0x76000000u
#define LINUX_VFS64_DEVICE_HANDLE_MASK 0xFFFF0000u
#define LINUX_VFS64_DEVICE_HANDLE_KIND_MASK 0x000000FFu

#define LINUX_VFS64_PROVIDER_NONE 0u
#define LINUX_VFS64_PROVIDER_RAMFS 1u
#define LINUX_VFS64_PROVIDER_PROC 2u
#define LINUX_VFS64_PROVIDER_DEV 3u
#define LINUX_VFS64_PROVIDER_NVME 4u
#define LINUX_VFS64_PROVIDER_BIN 5u

#define LINUX_VFS64_NODE_UNKNOWN 0u
#define LINUX_VFS64_NODE_RAMFS_PATH 1u
#define LINUX_VFS64_NODE_PROC_DIR 2u
#define LINUX_VFS64_NODE_DEV_DIR 3u
#define LINUX_VFS64_NODE_DEV_CHAR 4u
#define LINUX_VFS64_NODE_PROC_FILE 5u
#define LINUX_VFS64_NODE_PROC_SYMLINK 6u
#define LINUX_VFS64_NODE_TMP_SYMLINK 7u
#define LINUX_VFS64_NODE_NVME_DIR 8u
#define LINUX_VFS64_NODE_NVME_FILE 9u
#define LINUX_VFS64_NODE_BIN_DIR 10u
#define LINUX_VFS64_NODE_BIN_APPLET 11u

#define LINUX_VFS64_DEVICE_UNKNOWN 0u
#define LINUX_VFS64_DEVICE_NULL 1u
#define LINUX_VFS64_DEVICE_ZERO 2u
#define LINUX_VFS64_DEVICE_URANDOM 3u
#define LINUX_VFS64_DEVICE_STDIN 4u
#define LINUX_VFS64_DEVICE_STDOUT 5u
#define LINUX_VFS64_DEVICE_STDERR 6u
#define LINUX_VFS64_DEVICE_DIRECTORY 7u
#define LINUX_VFS64_DEVICE_PROC_MAPS 8u
#define LINUX_VFS64_DEVICE_PROC_EXE 9u
#define LINUX_VFS64_DEVICE_PROC_STATUS 10u
#define LINUX_VFS64_DEVICE_PROC_CMDLINE 11u
#define LINUX_VFS64_DEVICE_PROC_ENVIRON 12u
#define LINUX_VFS64_DEVICE_PROC_FD_DIR 13u
#define LINUX_VFS64_DEVICE_PROC_FD_LINK 14u
#define LINUX_VFS64_DEVICE_PROC_MEMINFO 15u
#define LINUX_VFS64_DEVICE_NVME_FILE 16u
#define LINUX_VFS64_DEVICE_LAST LINUX_VFS64_DEVICE_NVME_FILE

#define LINUX_VFS64_PROC_IDENTITY_BYTES 128u
#define LINUX_VFS64_PROC_PAYLOAD_BYTES 128u

#define LINUX_VFS64_DIRENT_NAME_MAX 32u
#define LINUX_VFS64_DIRENT_TYPE_UNKNOWN 0u
#define LINUX_VFS64_DIRENT_TYPE_CHR 2u
#define LINUX_VFS64_DIRENT_TYPE_DIR 4u
#define LINUX_VFS64_DIRENT_TYPE_REG 8u
#define LINUX_VFS64_DIRENT_TYPE_LNK 10u
#define LINUX_VFS64_READDIR_OK 1u
#define LINUX_VFS64_READDIR_EOF 2u
#define LINUX_VFS64_READDIR_NOT_DIRECTORY 3u

#define LINUX_VFS64_ERROR_NONE 0u
#define LINUX_VFS64_ERROR_ARGUMENT 1u
#define LINUX_VFS64_ERROR_NO_PROCESS 2u
#define LINUX_VFS64_ERROR_PATH 3u
#define LINUX_VFS64_ERROR_NO_MOUNT 4u
#define LINUX_VFS64_ERROR_NOT_FOUND 5u
#define LINUX_VFS64_ERROR_UNSUPPORTED 6u
#define LINUX_VFS64_ERROR_FD 7u

#define LINUX_VFS64_OPEN_READ 0x00000001u
#define LINUX_VFS64_OPEN_WRITE 0x00000002u
#define LINUX_VFS64_OPEN_CREATE 0x00010000u
#define LINUX_VFS64_OPEN_NOFOLLOW 0x00020000u
#define LINUX_VFS64_OPEN_FD_FLAG_MASK (FD64_FLAG_O_CLOEXEC | FD64_FLAG_O_NONBLOCK)
#define LINUX_VFS64_OPEN_SUPPORTED_FLAGS \
    (LINUX_VFS64_OPEN_FD_FLAG_MASK | LINUX_VFS64_OPEN_CREATE | LINUX_VFS64_OPEN_NOFOLLOW)

typedef struct linux_vfs64_mount
{
    const u8 *prefix;
    u32 prefix_byte_count;
    u32 provider;
    u32 flags;
} linux_vfs64_mount_t;

typedef struct linux_vfs64_result
{
    u32 provider;
    u32 node_type;
    u32 device_type;
    u32 capability_handle;
    u32 mount_index;
    u32 path_token;
    u32 error;
    u32 denied;
} linux_vfs64_result_t;

typedef struct linux_vfs64_dirent
{
    u64 inode;
    u32 next_offset;
    u32 name_byte_count;
    u8 entry_type;
    u8 name[LINUX_VFS64_DIRENT_NAME_MAX];
} linux_vfs64_dirent_t;

void linux_vfs64_init(void);
u32 linux_vfs64_mount_count(void);
const linux_vfs64_mount_t *linux_vfs64_mount_at(u32 index);
u32 linux_vfs64_resolve(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u32 flags,
    linux_vfs64_result_t *result);
u32 linux_vfs64_open(u32 pid, const u8 *path, u32 path_byte_count, u32 flags, u32 mode);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 linux_vfs64_read_file_all(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u8 *output,
    u32 output_capacity,
    u32 *bytes_out);
#endif
u32 linux_vfs64_read_fd(u32 pid, u32 fd_number, u8 *output, u32 byte_count);
u32 linux_vfs64_write_fd(u32 pid, u32 fd_number, const u8 *input, u32 byte_count);
u32 linux_vfs64_delete(u32 pid, const u8 *path, u32 path_byte_count);
u32 linux_vfs64_symlink(
    u32 pid,
    const u8 *target_path,
    u32 target_path_byte_count,
    const u8 *link_path,
    u32 link_path_byte_count);
u32 linux_vfs64_readlink(
    u32 pid,
    const u8 *path,
    u32 path_byte_count,
    u8 *output,
    u32 byte_capacity);
u32 linux_vfs64_stat(u32 pid, const u8 *path, u32 path_byte_count, fd64_stat_t *stat_out);
u32 linux_vfs64_lstat(u32 pid, const u8 *path, u32 path_byte_count, fd64_stat_t *stat_out);
u32 linux_vfs64_fstat(u32 pid, u32 fd_number, fd64_stat_t *stat_out);
u32 linux_vfs64_path_is_directory(u32 pid, const u8 *path, u32 path_byte_count);
u32 linux_vfs64_fd_path(u32 pid, u32 fd_number, u8 *path_out, u32 max_path_bytes, u32 *path_byte_count);
u32 linux_vfs64_forget_fd_path(u32 pid, u32 fd_number);
u32 linux_vfs64_fd_dir_cursor(u32 pid, u32 fd_number, u32 *cursor_out);
u32 linux_vfs64_set_fd_dir_cursor(u32 pid, u32 fd_number, u32 cursor);
u32 linux_vfs64_read_dirent(u32 pid, u32 fd_number, u32 cursor, linux_vfs64_dirent_t *entry_out);
u32 linux_vfs64_device_handle(u32 device_type);
u32 linux_vfs64_device_type_from_handle(u32 handle);
u32 linux_vfs64_bind_nvme_read(u32 pid, u32 owner_id, u32 nvme_capability);
u32 linux_vfs64_fork_process(u32 parent_pid, u32 child_pid);
u32 linux_vfs64_release_nvme_read(u32 pid);
u32 linux_vfs64_release_process(u32 pid);
u32 linux_vfs64_proc_set_identity(
    u32 pid,
    const u8 *exe_path,
    u32 exe_path_bytes,
    const u8 *cmdline,
    u32 cmdline_bytes,
    const u8 *environ,
    u32 environ_bytes);
u32 linux_vfs64_proc_clear_identity(u32 pid);
u32 linux_vfs64_denial_count(void);
u32 linux_vfs64_open_count(void);
u32 linux_vfs64_read_count(void);
u32 linux_vfs64_write_count(void);
u32 linux_vfs64_proc_read_count(void);
u32 linux_vfs64_proc_denial_count(void);
u32 linux_vfs64_proc_last_maps_regions(void);
u64 linux_vfs64_proc_last_maps_bytes(void);
u32 linux_vfs64_proc_last_exe_bytes(void);
u32 linux_vfs64_proc_last_status_bytes(void);
u32 linux_vfs64_proc_last_cmdline_bytes(void);
u32 linux_vfs64_proc_last_environ_bytes(void);
u32 linux_vfs64_proc_last_fd_entries(void);
u32 linux_vfs64_proc_last_fd_target(void);
u32 linux_vfs64_proc_last_meminfo_bytes(void);
u32 linux_vfs64_proc_last_mem_total_kib(void);
u32 linux_vfs64_proc_last_mem_free_kib(void);
u32 linux_vfs64_proc_last_mem_available_kib(void);
u32 linux_vfs64_proc_last_mem_claimed_kib(void);
u32 linux_vfs64_tmp_create_count(void);
u32 linux_vfs64_tmp_delete_count(void);
u32 linux_vfs64_tmp_denial_count(void);
u32 linux_vfs64_tmp_last_dir_entries(void);
u32 linux_vfs64_tmp_last_backend_path_bytes(void);
u32 linux_vfs64_tmp_last_namespace_pid(void);
u32 linux_vfs64_symlink_create_count(void);
u32 linux_vfs64_symlink_follow_count(void);
u32 linux_vfs64_symlink_readlink_count(void);
u32 linux_vfs64_symlink_lstat_count(void);
u32 linux_vfs64_symlink_nofollow_denial_count(void);
u32 linux_vfs64_symlink_last_target_bytes(void);
u32 linux_vfs64_nvme_bind_count(void);
u32 linux_vfs64_nvme_release_count(void);
u32 linux_vfs64_nvme_read_count(void);
u32 linux_vfs64_nvme_readdir_count(void);
u32 linux_vfs64_nvme_dirent_count(void);
u32 linux_vfs64_nvme_denial_count(void);
u32 linux_vfs64_nvme_last_bytes(void);
u32 linux_vfs64_bin_alias_count(void);
u32 linux_vfs64_bin_open_count(void);
u32 linux_vfs64_bin_read_count(void);
u32 linux_vfs64_bin_denial_count(void);
u32 linux_vfs64_fork_copy_count(void);
u32 linux_vfs64_fork_copy_denial_count(void);
u32 linux_vfs64_fork_copy_last_parent_pid(void);
u32 linux_vfs64_fork_copy_last_child_pid(void);
u32 linux_vfs64_fork_copy_last_fd_paths(void);
u32 linux_vfs64_fork_copy_last_nvme(void);

#endif
