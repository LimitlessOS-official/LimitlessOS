#include "boot_info.h"
#include "console.h"
#include "interrupts.h"
#include "ipc.h"
#include "klog.h"
#include "memory.h"
#include "paging.h"
#include "pit.h"
#include "scheduler.h"
#include "serial.h"
#include "services.h"
#include "syscall.h"
#include "userspace.h"
#include "x86.h"

static void log_boot_memory(const struct boot_info *boot_info)
{
    klog_write_string("[boot] drive ");
    klog_write_hex_u32(boot_info->boot_drive);
    klog_write_string(" conventional ");
    klog_write_dec_u32(boot_info->conventional_memory_kb);
    klog_write_string(" KiB extended ");
    klog_write_dec_u32(boot_info->extended_memory_kb);
    klog_write_line(" KiB");
}

static void log_memory_state(void)
{
    klog_write_string("[memory] free ");
    klog_write_dec_u32(memory_get_free_bytes() / 1024u);
    klog_write_string(" KiB frames ");
    klog_write_dec_u32(memory_get_free_frame_count());
    klog_write_line("");
}

static void log_ipc_state(void)
{
    klog_write_string("[ipc] bootstrap endpoints ");
    klog_write_dec_u32(ipc_endpoint_count());
    klog_write_string(" including ");
    klog_write_line(ipc_endpoint_name(IPC_ENDPOINT_AI_POLICY));
}

static void log_paging_state(void)
{
    klog_write_string("[paging] identity mapped ");
    klog_write_dec_u32(paging_get_mapped_bytes() / (1024u * 1024u));
    klog_write_string(" MiB cr3 ");
    klog_write_hex_u32(paging_get_page_directory_address());
    klog_newline();
}

void kernel_main(const struct boot_info *boot_info)
{
    u32 assistant_policy_flags;
    u32 ipc_count_from_syscall;
    u32 scheduler_task_count_value;
    u32 mapped_memory_mib;
    u32 service_count_value;
    u32 denied_ipc_value;
    u32 user_syscall_count_value;
    u32 user_process_count_value;
    u32 user_endpoint_count_value;
    u32 user_sleep_count_value;
    u32 user_ipc_send_count_value;
    u32 user_ipc_denied_count_value;
    u32 user_ipc_wait_count_value;
    u32 user_ipc_message_count_value;
    u32 user_yield_count_value;
    u32 user_preempt_count_value;

    console_set_color(CONSOLE_COLOR_LIGHT_GREY, CONSOLE_COLOR_BLUE);
    console_clear();
    serial_init();

    klog_write_line("LimitlessOS kernel core milestone");

    if ((boot_info == NULL) || (boot_info->magic != LIMITLESS_BOOT_INFO_MAGIC))
    {
        klog_write_line("[boot] invalid boot information");
        cpu_cli();
        cpu_halt_forever();
    }

    log_boot_memory(boot_info);

    memory_init(boot_info);
    log_memory_state();
    paging_init(0x00100000u + memory_get_extended_bytes());
    log_paging_state();

    ipc_init();
    log_ipc_state();
    scheduler_init();
    services_init();
    klog_write_string("[scheduler] registered tasks ");
    klog_write_dec_u32(services_core_count());
    klog_newline();
    userspace_init();

    klog_write_line("[kernel] initializing interrupts");
    interrupts_init();
    klog_write_line("[kernel] IDT and PIC online");
    pit_initialize(100);
    klog_write_line("[interrupts] PIT at 100 Hz");

    interrupts_enable();

    ipc_count_from_syscall = syscall_invoke(SYSCALL_GET_IPC_ENDPOINT_COUNT, 0, 0, 0);
    klog_write_string("[syscall] endpoint count ");
    klog_write_dec_u32(ipc_count_from_syscall);
    klog_newline();

    assistant_policy_flags = syscall_invoke(SYSCALL_GET_ASSISTANT_POLICY_FLAGS, 0, 0, 0);
    klog_write_string("[syscall] assistant policy flags ");
    klog_write_hex_u32(assistant_policy_flags);
    klog_newline();

    scheduler_task_count_value = syscall_invoke(SYSCALL_GET_SCHEDULER_TASK_COUNT, 0, 0, 0);
    klog_write_string("[syscall] scheduler task count ");
    klog_write_dec_u32(scheduler_task_count_value);
    klog_newline();

    mapped_memory_mib = syscall_invoke(SYSCALL_GET_MAPPED_MEMORY_MIB, 0, 0, 0);
    klog_write_string("[syscall] mapped memory ");
    klog_write_dec_u32(mapped_memory_mib);
    klog_write_line(" MiB");

    service_count_value = syscall_invoke(SYSCALL_GET_SERVICE_COUNT, 0, 0, 0);
    klog_write_string("[syscall] service count ");
    klog_write_dec_u32(service_count_value);
    klog_newline();

    denied_ipc_value = syscall_invoke(SYSCALL_GET_DENIED_IPC_COUNT, 0, 0, 0);
    klog_write_string("[syscall] denied ipc ");
    klog_write_dec_u32(denied_ipc_value);
    klog_newline();

    user_syscall_count_value = syscall_invoke(SYSCALL_GET_USER_SYSCALL_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user syscalls ");
    klog_write_dec_u32(user_syscall_count_value);
    klog_newline();

    user_process_count_value = syscall_invoke(SYSCALL_GET_USER_PROCESS_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user processes ");
    klog_write_dec_u32(user_process_count_value);
    klog_newline();

    user_endpoint_count_value = syscall_invoke(SYSCALL_GET_USER_ENDPOINT_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user endpoints ");
    klog_write_dec_u32(user_endpoint_count_value);
    klog_newline();

    user_sleep_count_value = syscall_invoke(SYSCALL_GET_USER_SLEEP_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user sleeps ");
    klog_write_dec_u32(user_sleep_count_value);
    klog_newline();

    user_ipc_send_count_value = syscall_invoke(SYSCALL_GET_USER_IPC_SEND_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user ipc sends ");
    klog_write_dec_u32(user_ipc_send_count_value);
    klog_newline();

    user_ipc_denied_count_value = syscall_invoke(SYSCALL_GET_USER_IPC_DENIED_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user ipc denied ");
    klog_write_dec_u32(user_ipc_denied_count_value);
    klog_newline();

    user_ipc_wait_count_value = syscall_invoke(SYSCALL_GET_USER_IPC_WAIT_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user ipc waits ");
    klog_write_dec_u32(user_ipc_wait_count_value);
    klog_newline();

    user_ipc_message_count_value = syscall_invoke(SYSCALL_GET_USER_IPC_MESSAGE_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user ipc messages ");
    klog_write_dec_u32(user_ipc_message_count_value);
    klog_newline();

    user_yield_count_value = syscall_invoke(SYSCALL_GET_USER_YIELD_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user yields ");
    klog_write_dec_u32(user_yield_count_value);
    klog_newline();

    user_preempt_count_value = syscall_invoke(SYSCALL_GET_USER_PREEMPT_COUNT, 0, 0, 0);
    klog_write_string("[syscall] user preempts ");
    klog_write_dec_u32(user_preempt_count_value);
    klog_newline();

    log_memory_state();
    klog_write_line("[kernel] entering user mode");
    userspace_enter_session();
    klog_write_line("[kernel] entering idle loop");

    for (;;)
    {
        cpu_halt();
    }
}
