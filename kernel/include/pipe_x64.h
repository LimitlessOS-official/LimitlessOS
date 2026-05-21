#ifndef LIMITLESS_PIPE_X64_H
#define LIMITLESS_PIPE_X64_H

#include "types.h"

#define PIPE64_INVALID_HANDLE 0xFFFFFFFFu
#define PIPE64_IO_ERROR 0xFFFFFFFFu
#define PIPE64_BUFFER_BYTES 4096u
#define PIPE64_MAX_BUFFER_BYTES 65536u
#define PIPE64_MAX_OBJECTS 16u
#define PIPE64_SPIN_WAIT_LIMIT 1024u
#define PIPE64_HANDLE_BASE 0x0F000000u
#define PIPE64_HANDLE_BASE_MASK 0xFF000000u
#define PIPE64_HANDLE_GENERATION_SHIFT 8u
#define PIPE64_HANDLE_GENERATION_MASK 0x000FFF00u
#define PIPE64_HANDLE_INDEX_SHIFT 1u
#define PIPE64_HANDLE_INDEX_MASK 0x000000FEu
#define PIPE64_HANDLE_KIND_MASK 0x00000001u
#define PIPE64_HANDLE_KIND_READ 0u
#define PIPE64_HANDLE_KIND_WRITE 1u

typedef struct pipe64_buffer
{
    u32 live;
    u32 owner_pid;
    u32 owner_id;
    u32 read_owner_id;
    u32 write_owner_id;
    u32 read_grantee_owner_id;
    u32 write_grantee_owner_id;
    u32 read_ref_count;
    u32 write_ref_count;
    u32 generation;
    u32 read_handle;
    u32 write_handle;
    u32 read_fd;
    u32 write_fd;
    u32 capacity;
    u32 max_capacity;
    u32 read_index;
    u32 write_index;
    u32 byte_count;
    u32 byte_count_semaphore;
    u32 writer_closed;
    u32 reader_closed;
    u8 bytes[PIPE64_BUFFER_BYTES];
} pipe64_buffer_t;

void pipe64_init(void);
u32 pipe64_create(u32 pid, u32 *read_fd_out, u32 *write_fd_out);
u32 pipe64_create_flags(u32 pid, u32 flags, u32 *read_fd_out, u32 *write_fd_out);
u32 pipe64_grant_endpoint(u32 source_pid, u32 source_fd, u32 target_pid, u32 *target_fd_out);
u32 pipe64_write(u32 pipe_handle, const u8 *input, u32 byte_count, u32 owner_id);
u32 pipe64_read(u32 pipe_handle, u8 *output, u32 byte_count, u32 owner_id);
u32 pipe64_revoke_handle(u32 pipe_handle, u32 owner_id);
u32 pipe64_live_count(void);
u32 pipe64_bytes_available(u32 pipe_handle, u32 owner_id);
u32 pipe64_capacity(u32 pipe_handle, u32 owner_id);
u32 pipe64_reader_closed(u32 pipe_handle, u32 owner_id);
u32 pipe64_writer_closed(u32 pipe_handle, u32 owner_id);
u32 pipe64_handle_kind(u32 pipe_handle);
u32 pipe64_denial_count(void);

#endif
