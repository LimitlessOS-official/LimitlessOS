#include "syscall.h"

#include "ipc.h"
#include "memory.h"
#include "paging.h"
#include "pit.h"
#include "scheduler.h"
#include "services.h"
#include "userspace.h"

u32 syscall_dispatch(u32 number, u32 arg0, u32 arg1, u32 arg2)
{
    (void)arg0;
    (void)arg1;
    (void)arg2;

    switch (number)
    {
        case SYSCALL_GET_UPTIME_TICKS:
            return pit_get_ticks();

        case SYSCALL_GET_IPC_ENDPOINT_COUNT:
            return ipc_endpoint_count();

        case SYSCALL_GET_FREE_MEMORY_KIB:
            return memory_get_free_bytes() / 1024u;

        case SYSCALL_GET_ASSISTANT_POLICY_FLAGS:
            return ASSISTANT_POLICY_REQUIRE_CONSENT
                | ASSISTANT_POLICY_AUDIT_ACTIONS
                | ASSISTANT_POLICY_SEALED_CORE;

        case SYSCALL_GET_SCHEDULER_TASK_COUNT:
            return scheduler_task_count();

        case SYSCALL_GET_FREE_FRAME_COUNT:
            return memory_get_free_frame_count();

        case SYSCALL_GET_MAPPED_MEMORY_MIB:
            return paging_get_mapped_bytes() / (1024u * 1024u);

        case SYSCALL_GET_SERVICE_COUNT:
            return services_service_count();

        case SYSCALL_GET_DENIED_IPC_COUNT:
            return services_total_denied_ipc_count();

        case SYSCALL_GET_USER_SYSCALL_COUNT:
            return userspace_syscall_count();

        case SYSCALL_GET_USER_PROCESS_COUNT:
            return userspace_process_count();

        case SYSCALL_GET_USER_YIELD_COUNT:
            return userspace_total_yield_count();

        case SYSCALL_GET_USER_PREEMPT_COUNT:
            return userspace_total_preemption_count();

        case SYSCALL_USER_SLEEP_TICKS:
            return 0u;

        case SYSCALL_USER_YIELD:
            return 0u;

        case SYSCALL_GET_USER_SLEEP_COUNT:
            return userspace_total_sleep_count();

        case SYSCALL_GET_USER_IPC_WAIT_COUNT:
            return userspace_total_ipc_wait_count();

        case SYSCALL_GET_USER_IPC_MESSAGE_COUNT:
            return userspace_total_ipc_message_count();

        case SYSCALL_USER_POLICY_REQUEST:
            return 0u;

        case SYSCALL_USER_WAIT_IPC:
            return 0u;

        case SYSCALL_GET_USER_IPC_SEND_COUNT:
            return userspace_total_ipc_send_count();

        case SYSCALL_GET_USER_IPC_DENIED_COUNT:
            return userspace_total_ipc_denied_count();

        case SYSCALL_USER_SEND_IPC:
            return 0u;

        case SYSCALL_USER_REGISTER_ENDPOINT:
            return 0u;

        case SYSCALL_GET_USER_ENDPOINT_COUNT:
            return userspace_registered_endpoint_count();

        case SYSCALL_USER_LOOKUP_ENDPOINT:
            return 0u;

        case SYSCALL_USER_LOOKUP_SERVICE_ENDPOINT:
            return 0u;

        case SYSCALL_USER_REVOKE_CAPABILITY:
            return 0u;

        case SYSCALL_USER_LOOKUP_ENDPOINT_CLASS:
            return 0u;

        case SYSCALL_USER_DELEGATE_CAPABILITY:
            return 0u;

        case SYSCALL_USER_REGISTER_SHARED_BUFFER:
            return 0u;

        case SYSCALL_USER_READ_SHARED_BUFFER:
            return 0u;

        case SYSCALL_USER_WRITE_SHARED_BUFFER:
            return 0u;

        case SYSCALL_USER_EXIT:
            return 0u;

        case SYSCALL_USER_CONSOLE_WRITE:
            return 0u;

        case SYSCALL_USER_FS_OPEN:
            return 0u;

        case SYSCALL_USER_FS_LIST:
            return 0u;

        case SYSCALL_USER_FS_READ:
            return 0u;

        case SYSCALL_USER_FS_CREATE:
            return 0u;

        case SYSCALL_USER_FS_WRITE:
            return 0u;

        case SYSCALL_USER_FS_STAT:
            return 0u;

        case SYSCALL_USER_FS_RENAME:
            return 0u;

        case SYSCALL_USER_FS_DELETE:
            return 0u;

        case SYSCALL_USER_WAIT_PROCESS:
            return 0u;

        case SYSCALL_USER_FS_MOVE:
            return 0u;

        case SYSCALL_USER_LAUNCH_EXECUTABLE:
            return 0u;

        case SYSCALL_USER_INPUT_READ:
            return 0u;

        default:
            return 0xFFFFFFFFu;
    }
}
