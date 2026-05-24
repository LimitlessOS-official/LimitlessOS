#ifndef LIMITLESS_MACOS_MACH_X64_H
#define LIMITLESS_MACOS_MACH_X64_H

#include "capability_x64.h"
#include "types.h"

#define MACOS_MACH64_TRAP_TABLE_SIZE 64u
#define MACOS_MACH64_TRAP_MACH_MSG (-31)
#define MACOS_MACH64_TRAP_TASK_SELF (-28)
#define MACOS_MACH64_TRAP_THREAD_SELF (-27)
#define MACOS_MACH64_TRAP_MACH_REPLY_PORT (-26)
#define MACOS_MACH64_TRAP_HOST_SELF (-20)

#define MACOS_MACH64_PORT_NAME_BASE 0x0000A200u
#define MACOS_MACH64_MAX_PORTS 32u
#define MACOS_MACH64_MAX_MESSAGE_BYTES 64u
#define MACOS_MACH64_INLINE_BODY_BYTES 32u
#define MACOS_MACH64_MSG_HEADER_BYTES 24u

#define MACOS_MACH64_PORT_KIND_TASK 1u
#define MACOS_MACH64_PORT_KIND_THREAD 2u
#define MACOS_MACH64_PORT_KIND_HOST 3u
#define MACOS_MACH64_PORT_KIND_REPLY 4u

#define MACOS_MACH64_PORT_RIGHT_SEND CAPABILITY64_RIGHT_SEND
#define MACOS_MACH64_PORT_RIGHT_RECEIVE 0x00000008u
#define MACOS_MACH64_PORT_RIGHT_QUERY CAPABILITY64_RIGHT_QUERY

#define MACOS_MACH64_KERN_SUCCESS 0x00000000u
#define MACOS_MACH64_KERN_INVALID_ADDRESS 0x00000001u
#define MACOS_MACH64_KERN_INVALID_ARGUMENT 0x00000004u
#define MACOS_MACH64_KERN_NO_SPACE 0x00000003u

#define MACOS_MACH64_MSG_OPTION_SEND 0x00000001u
#define MACOS_MACH64_MSG_OPTION_RCV 0x00000002u
#define MACOS_MACH64_MSG_OPTION_ALLOWED \
    (MACOS_MACH64_MSG_OPTION_SEND | MACOS_MACH64_MSG_OPTION_RCV)

#define MACOS_MACH64_MACH_MSG_SUCCESS MACOS_MACH64_KERN_SUCCESS
#define MACOS_MACH64_MACH_SEND_INVALID_DEST 0x10000003u
#define MACOS_MACH64_MACH_SEND_INVALID_DATA 0x10000002u
#define MACOS_MACH64_MACH_SEND_TIMED_OUT 0x10000004u
#define MACOS_MACH64_MACH_RCV_INVALID_NAME 0x10004002u
#define MACOS_MACH64_MACH_RCV_TIMED_OUT 0x10004003u
#define MACOS_MACH64_MACH_RCV_TOO_LARGE 0x10004004u

typedef struct macos_mach64_msg_header
{
    u32 msgh_bits;
    u32 msgh_size;
    u32 msgh_remote_port;
    u32 msgh_local_port;
    u32 msgh_voucher_port;
    s32 msgh_id;
} macos_mach64_msg_header_t;

typedef u64 (*macos_mach64_trap_handler_t)(
    u32 pid,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip);

void macos_mach64_init(void);
macos_mach64_trap_handler_t *macos_mach64_trap_table(void);
u64 macos_mach64_dispatch(
    u32 pid,
    s32 trap_number,
    u64 rdi,
    u64 rsi,
    u64 rdx,
    u64 r10,
    u64 r8,
    u64 r9,
    u64 rip);
u32 macos_mach64_release_process(u32 pid);
u32 macos_mach64_table_size(void);
u32 macos_mach64_unimplemented_entry_count(void);
u32 macos_mach64_entry_installed(s32 trap_number);
u32 macos_mach64_mach_msg_entry_installed(void);
u32 macos_mach64_task_self_entry_installed(void);
u32 macos_mach64_thread_self_entry_installed(void);
u32 macos_mach64_host_self_entry_installed(void);
u32 macos_mach64_reply_port_entry_installed(void);
u32 macos_mach64_dispatch_count(void);
u32 macos_mach64_unimplemented_count(void);
u32 macos_mach64_mach_msg_count(void);
u32 macos_mach64_port_create_count(void);
u32 macos_mach64_send_count(void);
u32 macos_mach64_receive_count(void);
u32 macos_mach64_denial_count(void);
u32 macos_mach64_fault_count(void);
u32 macos_mach64_live_port_count(u32 pid);
u32 macos_mach64_total_live_port_count(void);
u32 macos_mach64_port_backing_endpoint(u32 pid, u32 port_name);
u32 macos_mach64_port_backing_capability(u32 pid, u32 port_name);
u32 macos_mach64_port_rights(u32 pid, u32 port_name);
u32 macos_mach64_port_kind(u32 pid, u32 port_name);
u32 macos_mach64_port_pending_count(u32 pid, u32 port_name);
u32 macos_mach64_last_trap(void);
u32 macos_mach64_last_result(void);
u32 macos_mach64_last_port(void);
u32 macos_mach64_last_remote_port(void);
u32 macos_mach64_last_local_port(void);
u32 macos_mach64_last_message_id(void);
u32 macos_mach64_last_message_checksum(void);
u32 macos_mach64_last_send_size(void);
u32 macos_mach64_last_receive_size(void);
u32 macos_mach64_last_backing_endpoint(void);
u32 macos_mach64_last_backing_capability(void);

#endif
