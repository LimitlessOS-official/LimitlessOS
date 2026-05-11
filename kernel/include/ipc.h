#ifndef LIMITLESS_IPC_H
#define LIMITLESS_IPC_H

#include "types.h"

#define IPC_ENDPOINT_INIT 1u
#define IPC_ENDPOINT_DRIVER_HOST 2u
#define IPC_ENDPOINT_AI_POLICY 3u

#define IPC_STATUS_OK 0
#define IPC_STATUS_UNKNOWN_ENDPOINT -1
#define IPC_STATUS_QUEUE_FULL -2
#define IPC_STATUS_ACCESS_DENIED -3
#define IPC_STATUS_NOT_OWNER -4
#define IPC_STATUS_EMPTY -5

#define IPC_ENDPOINT_FLAG_KERNEL_OWNED 0x00000001u
#define IPC_ENDPOINT_FLAG_POLICY_GUARDED 0x00000002u

#define IPC_MESSAGE_FLAG_INTERACTIVE_WAIT 0x00000001u
#define IPC_MESSAGE_FLAG_DEPENDENCY_PROBE 0x00000002u

#define IPC_DEPENDENCY_DEPTH_LIMIT 2u

#define IPC_MESSAGE_POLICY_HANDSHAKE 0x00000100u
#define IPC_MESSAGE_POLICY_REJECTED 0x00000101u
#define IPC_MESSAGE_POLICY_APPROVED 0x00000102u

struct ipc_message
{
    u32 source_endpoint;
    u32 type;
    u32 arg0;
    u32 arg1;
    u32 flags;
    u32 dependency_depth;
};

void ipc_init(void);
u32 ipc_endpoint_count(void);
const char *ipc_endpoint_name(u32 endpoint_id);
s32 ipc_send(u32 endpoint_id, const struct ipc_message *message);
s32 ipc_receive(u32 endpoint_id, struct ipc_message *message);

#endif
