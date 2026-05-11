#include "userspace.h"

#include "input.h"
#include "ipc.h"
#include "interrupts.h"
#include "klog.h"
#include "memory.h"
#include "package_store.h"
#include "paging.h"
#include "pit.h"
#include "ramfs.h"
#include "services.h"
#include "x86.h"

enum
{
    GDT_ENTRY_COUNT = 6,
    GDT_SELECTOR_KERNEL_DATA = 0x10,
    GDT_SELECTOR_USER_DATA = 0x23,
    GDT_SELECTOR_TSS = 0x28,
    USER_PAGE_SIZE = 4096u,
    USER_CODE_PAGE_LIMIT = 8u,
    USER_PROCESS_LIMIT = 6,
    USER_EXECUTABLE_LIMIT = 15,
    USER_PACKAGE_STORE_CANDIDATE_LIMIT = 16,
    USER_TRUSTED_SIGNER_LIMIT = 1,
    USER_BASE_TIME_SLICE_TICKS = 4u,
    USER_RUNTIME_SCALE = 12u,
    USER_MAILBOX_CAPACITY = 8u,
    USER_IPC_MAX_PAYLOAD_WORDS = 4u,
    USER_MESSAGE_CAP_GRANTED = 0x00000210u,
    USER_CAPABILITY_LIMIT = 16u,
    USER_CAPABILITY_BASE = 0x00002000u,
    USER_CAPABILITY_LEASE_NONE = 0xFFFFFFFFu,
    USER_DELEGATED_CAPABILITY_LEASE_TICKS = 12u,
    USER_ENDPOINTS_PER_PROCESS = 4u,
    USER_ENDPOINT_BASE = 0x00000100u,
    USER_SHARED_BUFFER_LIMIT = 4u,
    USER_SHARED_BUFFER_BASE = 0x00004000u,
    USER_BUFFER_COPY_SCRATCH_BYTES = 256u,
    USER_ENDPOINT_ROLE_PEER = 1u,
    USER_ENDPOINT_ROLE_POLICY = 2u,
    USER_SCHEDULER_CLASS_COUNT = 3u,
    USER_SCHEDULER_BUDGET_WINDOW_TICKS = 32u
};

enum user_manifest_trust_flag
{
    USER_MANIFEST_TRUST_MEASURED = 0x00000001u,
    USER_MANIFEST_TRUST_POLICY_APPROVED = 0x00000002u
};

enum user_manifest_launch_authority
{
    USER_MANIFEST_LAUNCH_AUTHORITY_INIT = 0x00000001u,
    USER_MANIFEST_LAUNCH_AUTHORITY_SESSION = 0x00000002u
};

enum user_launch_role
{
    USER_LAUNCH_ROLE_SESSION_SHELL = 1u,
    USER_LAUNCH_ROLE_AUTOMATION_WORKER = 2u,
    USER_LAUNCH_ROLE_LS_UTILITY = 3u,
    USER_LAUNCH_ROLE_CAT_UTILITY = 4u,
    USER_LAUNCH_ROLE_MKDIR_UTILITY = 5u,
    USER_LAUNCH_ROLE_WRITE_UTILITY = 6u,
    USER_LAUNCH_ROLE_STAT_UTILITY = 7u,
    USER_LAUNCH_ROLE_RENAME_UTILITY = 8u,
    USER_LAUNCH_ROLE_APPEND_UTILITY = 9u,
    USER_LAUNCH_ROLE_DELETE_UTILITY = 10u,
    USER_LAUNCH_ROLE_MOVE_UTILITY = 11u,
    USER_LAUNCH_ROLE_ECHO_UTILITY = 12u,
    USER_LAUNCH_ROLE_ASK_UTILITY = 13u,
    USER_LAUNCH_ROLE_TOUCH_UTILITY = 14u,
    USER_LAUNCH_ROLE_COPY_UTILITY = 15u
};

enum user_capability_right
{
    USER_CAPABILITY_RIGHT_SEND = 0x00000001u,
    USER_CAPABILITY_RIGHT_DELEGATE = 0x00000002u,
    USER_CAPABILITY_RIGHT_BUFFER_READ = 0x00000010u,
    USER_CAPABILITY_RIGHT_BUFFER_WRITE = 0x00000020u,
    USER_CAPABILITY_RIGHT_NODE_LIST = 0x00000100u,
    USER_CAPABILITY_RIGHT_NODE_READ = 0x00000200u,
    USER_CAPABILITY_RIGHT_NODE_CREATE = 0x00000400u,
    USER_CAPABILITY_RIGHT_NODE_WRITE = 0x00000800u,
    USER_CAPABILITY_RIGHT_NODE_STAT = 0x00001000u,
    USER_CAPABILITY_RIGHT_NODE_RENAME = 0x00002000u,
    USER_CAPABILITY_RIGHT_NODE_DELETE = 0x00004000u
};

enum user_capability_type
{
    USER_CAPABILITY_TYPE_NONE = 0,
    USER_CAPABILITY_TYPE_ENDPOINT = 1,
    USER_CAPABILITY_TYPE_SHARED_BUFFER = 2,
    USER_CAPABILITY_TYPE_RAMFS_NODE = 3
};

enum user_process_state
{
    USER_PROCESS_UNUSED = 0,
    USER_PROCESS_RUNNABLE = 1,
    USER_PROCESS_RUNNING = 2,
    USER_PROCESS_SLEEPING = 3,
    USER_PROCESS_WAITING_IPC = 4,
    USER_PROCESS_WAITING_INPUT = 5,
    USER_PROCESS_WAITING_PROCESS = 6
};

enum user_scheduler_class
{
    USER_SCHEDULER_CLASS_INTERACTIVE = 0,
    USER_SCHEDULER_CLASS_STANDARD = 1,
    USER_SCHEDULER_CLASS_SANDBOXED = 2
};

enum user_wake_source
{
    USER_WAKE_SOURCE_NONE = 0,
    USER_WAKE_SOURCE_TIMER = 1,
    USER_WAKE_SOURCE_IPC = 2,
    USER_WAKE_SOURCE_INPUT = 3
};

struct user_process_policy
{
    const char *name;
    u32 allowed_endpoint_role_mask;
    u32 allowed_service_class_mask;
    u32 scheduler_class;
    u32 scheduler_weight;
    u32 scheduler_latency_target_ticks;
    u32 scheduler_io_wakeup_deadline_ticks;
    u32 capability_admission_limit;
};

struct user_ipc_message
{
    u32 source_endpoint;
    u32 destination_endpoint;
    u32 type;
    u32 payload_word_count;
    u32 payload[USER_IPC_MAX_PAYLOAD_WORDS];
};

struct user_endpoint
{
    u32 id;
    const char *name;
    u32 role;
    u32 endpoint_class;
    u32 delegable;
    u32 registered;
    u32 allowed_sender_mask;
};

struct user_shared_buffer
{
    u32 id;
    u32 virtual_address;
    u32 byte_length;
    u32 delegable;
    u32 registered;
};

struct user_capability
{
    u32 handle;
    u32 object_type;
    u32 object_id;
    u32 rights;
    u32 object_owner_process_id;
    u32 object_owner_generation;
    u32 issued_by_process_id;
    u32 issued_by_process_generation;
    u32 parent_handle;
    u32 lease_expiry_tick;
    u32 active;
    u32 expired;
    u32 stale;
};

struct user_builtin_executable
{
    u32 source_slot;
    u32 package_id;
    const char *package_name;
    u32 package_version;
    u32 signer_id;
    u32 signature_token;
    u32 trust_flags;
    u32 launch_authority_mask;
    u32 max_instances;
    u32 expected_image_size;
    u32 expected_image_checksum;
    u32 id;
    const char *name;
    const char *process_name;
    const char *profile_name;
    const char *peer_endpoint_name;
    const char *policy_endpoint_name;
    const u8 *image_start;
    const u8 *image_end;
    u32 allowed_endpoint_role_mask;
    u32 allowed_service_class_mask;
    u32 scheduler_class;
    u32 scheduler_weight;
    u32 scheduler_latency_target_ticks;
    u32 scheduler_io_wakeup_deadline_ticks;
    u32 capability_admission_limit;
    u32 launch_role;
    u32 loaded;
};

struct user_trusted_signer
{
    u32 id;
    const char *name;
    u32 verification_token;
};

struct gdt_entry
{
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granularity;
    u8 base_high;
} __attribute__((packed));

struct tss_entry
{
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt_selector;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed));

extern void x86_resume_user_frame(struct interrupt_frame *frame);
extern void x86_kernel_idle_loop(void);

struct user_process
{
    u32 id;
    u32 generation;
    u32 executable_id;
    u32 launch_role;
    const char *name;
    struct user_process_policy policy;
    u32 address_space;
    u32 code_pages[USER_CODE_PAGE_LIMIT];
    u32 code_page_count;
    u32 stack_page;
    u32 kernel_stack_page;
    u32 kernel_stack_top;
    u32 state;
    u32 wake_tick;
    u32 ready_since_tick;
    u32 wakeup_deadline_tick;
    u32 wake_source;
    u32 sleep_count;
    u32 yield_count;
    u32 preempt_count;
    u32 dispatch_count;
    u32 runtime_ticks;
    u32 virtual_runtime;
    u32 latency_pick_count;
    u32 deadline_pick_count;
    u32 io_wake_count;
    u32 policy_wait_pending;
    u32 syscall_count;
    struct user_endpoint endpoints[USER_ENDPOINTS_PER_PROCESS];
    struct user_shared_buffer shared_buffers[USER_SHARED_BUFFER_LIMIT];
    u32 ipc_send_count;
    u32 ipc_denied_count;
    u32 ipc_wait_count;
    u32 ipc_message_count;
    u32 wait_buffer_address;
    u32 wait_buffer_word_capacity;
    u32 input_wait_buffer_capability_handle;
    u32 input_wait_byte_capacity;
    u32 wait_process_id;
    struct user_capability capabilities[USER_CAPABILITY_LIMIT];
    struct user_ipc_message mailbox[USER_MAILBOX_CAPACITY];
    u32 mailbox_head;
    u32 mailbox_tail;
    u32 mailbox_count;
    struct interrupt_frame frame;
};

static struct gdt_entry gdt[GDT_ENTRY_COUNT];
static struct tss_entry tss;
static struct user_process processes[USER_PROCESS_LIMIT];
static struct user_builtin_executable builtin_executables[USER_EXECUTABLE_LIMIT];
static struct user_builtin_executable package_store_candidates[USER_PACKAGE_STORE_CANDIDATE_LIMIT];
static struct user_trusted_signer trusted_signers[USER_TRUSTED_SIGNER_LIMIT];
static struct user_process *current_process = NULL;
static struct interrupt_frame *kernel_idle_frame = NULL;
static u32 process_generation_counters[USER_PROCESS_LIMIT];
static u32 kernel_idle_stack_top = 0;
static u8 buffer_copy_scratch[USER_BUFFER_COPY_SCRATCH_BYTES];
static u32 user_syscalls = 0;
static u32 total_sleeps = 0;
static u32 total_yields = 0;
static u32 total_preemptions = 0;
static u32 total_ipc_sends = 0;
static u32 total_ipc_denied = 0;
static u32 total_ipc_waits = 0;
static u32 total_ipc_messages = 0;
static u32 total_registered_endpoints = 0;
static u32 total_policy_requests = 0;
static u32 total_service_resolutions = 0;
static u32 total_endpoint_class_resolutions = 0;
static u32 total_capability_grants = 0;
static u32 total_capability_revocations = 0;
static u32 total_capability_delegations = 0;
static u32 total_capability_expirations = 0;
static u32 total_policy_denials = 0;
static u32 total_user_dispatches = 0;
static u32 total_latency_picks = 0;
static u32 total_deadline_picks = 0;
static u32 total_io_wakes = 0;
static u32 total_budget_throttles = 0;
static u32 total_capability_admission_denials = 0;
static u32 total_capability_reuses = 0;
static u32 total_capability_compactions = 0;
static u32 total_shared_buffer_registrations = 0;
static u32 total_shared_buffer_copies = 0;
static u32 total_process_exits = 0;
static u32 total_console_writes = 0;
static u32 total_input_reads = 0;
static u32 total_fs_opens = 0;
static u32 total_fs_creates = 0;
static u32 total_fs_lists = 0;
static u32 total_fs_reads = 0;
static u32 total_fs_stats = 0;
static u32 total_fs_renames = 0;
static u32 total_fs_moves = 0;
static u32 total_fs_deletes = 0;
static u32 total_fs_writes = 0;
static u32 total_package_manifest_loads = 0;
static u32 total_package_manifest_rejections = 0;
static u32 total_signer_verifications = 0;
static u32 total_signer_denials = 0;
static u32 total_manifest_verifications = 0;
static u32 total_manifest_denials = 0;
static u32 interactive_policy_waiters = 0;
static u32 next_capability_handle = USER_CAPABILITY_BASE + 1u;
static u32 scheduler_budget_window_start_tick = 0u;
static u32 scheduler_class_runtime_used[USER_SCHEDULER_CLASS_COUNT];
static u32 current_slice_ticks = 0;
static u32 bootstrap_policy_approved = 0u;
static int userspace_last_pick_used_deadline = 0;
static int userspace_last_pick_used_latency = 0;
static int userspace_ready = 0;
static int userspace_active = 0;

static int userspace_copy_bytes_from_process(
    struct user_process *process,
    u32 source_address,
    u8 *destination_bytes,
    u32 byte_count);
static int userspace_copy_bytes_to_process(
    struct user_process *process,
    u32 destination_address,
    const u8 *source_bytes,
    u32 byte_count);

static void memory_zero(void *address, u32 size)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0; index < size; ++index)
    {
        bytes[index] = 0;
    }
}

static void memory_copy(void *destination, const void *source, u32 size)
{
    u8 *dest = (u8 *)destination;
    const u8 *src = (const u8 *)source;
    u32 index;

    for (index = 0; index < size; ++index)
    {
        dest[index] = src[index];
    }
}

static int userspace_range_within_page(u32 address, u32 byte_count, u32 page_base)
{
    u32 end;
    u32 page_end = page_base + USER_PAGE_SIZE;

    if (address < page_base)
    {
        return 0;
    }

    end = address + byte_count;
    if (end < address)
    {
        return 0;
    }

    return end <= page_end;
}

static u32 userspace_measure_image_digest(const u8 *image_start, u32 image_size)
{
    u32 digest = 2166136261u;
    u32 index;

    if (image_start == NULL)
    {
        return 0u;
    }

    for (index = 0; index < image_size; ++index)
    {
        digest ^= image_start[index];
        digest *= 16777619u;
    }

    return digest;
}

static u32 userspace_measure_string_digest(const char *text)
{
    u32 digest = 2166136261u;

    if (text == NULL)
    {
        return 0u;
    }

    while (*text != '\0')
    {
        digest ^= (u8)(*text);
        digest *= 16777619u;
        ++text;
    }

    return digest;
}

static int userspace_region_is_valid(u32 buffer_address, u32 byte_count)
{
    u32 stack_page_base = PAGING_USER_STACK_TOP - USER_PAGE_SIZE;

    if (byte_count == 0u)
    {
        return 1;
    }

    if (buffer_address == 0u)
    {
        return 0;
    }

    return userspace_range_within_page(buffer_address, byte_count, PAGING_USER_CODE_VIRTUAL)
        || userspace_range_within_page(buffer_address, byte_count, stack_page_base);
}

static int userspace_word_buffer_is_valid(u32 buffer_address, u32 payload_word_count)
{
    if (payload_word_count > USER_IPC_MAX_PAYLOAD_WORDS)
    {
        return 0;
    }

    if ((buffer_address & 0x3u) != 0u)
    {
        return 0;
    }

    return userspace_region_is_valid(buffer_address, payload_word_count * sizeof(u32));
}

static u32 userspace_restore_address_space(void)
{
    if ((current_process != NULL) && userspace_active)
    {
        return current_process->address_space;
    }

    return paging_get_page_directory_address();
}

static int userspace_copy_words_from_current(
    u32 buffer_address,
    u32 payload_word_count,
    u32 *destination_words)
{
    if (payload_word_count == 0u)
    {
        return 1;
    }

    if ((destination_words == NULL)
        || !userspace_word_buffer_is_valid(buffer_address, payload_word_count))
    {
        return 0;
    }

    memory_copy(destination_words, (const void *)buffer_address, payload_word_count * sizeof(u32));
    return 1;
}

static int userspace_copy_bytes_from_current(
    u32 buffer_address,
    u32 byte_count,
    u8 *destination_bytes)
{
    if (byte_count == 0u)
    {
        return 1;
    }

    if ((destination_bytes == NULL)
        || !userspace_region_is_valid(buffer_address, byte_count))
    {
        return 0;
    }

    memory_copy(destination_bytes, (const void *)buffer_address, byte_count);
    return 1;
}

static int userspace_copy_words_to_process(
    struct user_process *process,
    u32 buffer_address,
    u32 payload_word_capacity,
    const u32 *payload_words,
    u32 payload_word_count)
{
    u32 restore_address_space;

    if (payload_word_count > payload_word_capacity)
    {
        return 0;
    }

    if (payload_word_count == 0u)
    {
        return 1;
    }

    if ((process == NULL)
        || (payload_words == NULL)
        || !userspace_word_buffer_is_valid(buffer_address, payload_word_count))
    {
        return 0;
    }

    restore_address_space = userspace_restore_address_space();
    paging_switch_address_space(process->address_space);
    memory_copy((void *)buffer_address, payload_words, payload_word_count * sizeof(u32));
    paging_switch_address_space(restore_address_space);
    return 1;
}

static void userspace_prepare_wait_result(
    struct interrupt_frame *frame,
    const struct user_ipc_message *message)
{
    frame->eax = message->type;
    frame->ebx = message->source_endpoint;
    frame->ecx = message->destination_endpoint;
    frame->edx = message->payload_word_count;
    frame->esi = 0u;
}

static void userspace_prepare_wait_error(struct interrupt_frame *frame, u32 required_words)
{
    frame->eax = 0xFFFFFFFFu;
    frame->ebx = 0xFFFFFFFFu;
    frame->ecx = 0u;
    frame->edx = required_words;
    frame->esi = 0u;
}

static void gdt_set_entry(u32 index, u32 base, u32 limit, u8 access, u8 flags)
{
    gdt[index].limit_low = (u16)(limit & 0xFFFFu);
    gdt[index].base_low = (u16)(base & 0xFFFFu);
    gdt[index].base_middle = (u8)((base >> 16) & 0xFFu);
    gdt[index].access = access;
    gdt[index].granularity = (u8)(((limit >> 16) & 0x0Fu) | (flags & 0xF0u));
    gdt[index].base_high = (u8)((base >> 24) & 0xFFu);
}

static void userspace_load_gdt(void)
{
    lgdt(gdt, sizeof(gdt) - 1u);
    __asm__ __volatile__(
        "movw %0, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        :
        : "i"(GDT_SELECTOR_KERNEL_DATA)
        : "ax", "memory");
    ltr(GDT_SELECTOR_TSS);
}

static void userspace_set_kernel_stack(u32 stack_top)
{
    tss.esp0 = stack_top;
}

static u32 userspace_endpoint_id(u32 process_id, u32 slot_index)
{
    return USER_ENDPOINT_BASE
        + ((process_id - 1u) * USER_ENDPOINTS_PER_PROCESS)
        + slot_index
        + 1u;
}

static u32 userspace_shared_buffer_id(u32 process_id, u32 slot_index)
{
    return USER_SHARED_BUFFER_BASE
        + ((process_id - 1u) * USER_SHARED_BUFFER_LIMIT)
        + slot_index
        + 1u;
}

static u32 userspace_allocate_capability_handle(void)
{
    return next_capability_handle++;
}

static u32 userspace_role_policy_bit(u32 role)
{
    if ((role == 0u) || (role > 32u))
    {
        return 0u;
    }

    return 1u << (role - 1u);
}

static u32 userspace_service_class_policy_bit(u32 endpoint_class)
{
    if ((endpoint_class == 0u) || (endpoint_class > 32u))
    {
        return 0u;
    }

    return 1u << (endpoint_class - 1u);
}

static int userspace_tick_reached(u32 now, u32 target)
{
    return (s32)(now - target) >= 0;
}

static u32 userspace_runtime_cost(
    const struct user_process *process,
    u32 elapsed_ticks)
{
    u32 weight;

    if ((process == NULL) || (elapsed_ticks == 0u))
    {
        return 0u;
    }

    weight = (process->policy.scheduler_weight == 0u)
        ? 1u
        : process->policy.scheduler_weight;
    return (elapsed_ticks * USER_RUNTIME_SCALE) / weight;
}

static u32 userspace_latency_target_ticks(const struct user_process *process)
{
    if ((process == NULL) || (process->policy.scheduler_latency_target_ticks == 0u))
    {
        return 1u;
    }

    return process->policy.scheduler_latency_target_ticks;
}

static u32 userspace_io_wakeup_deadline_ticks(const struct user_process *process)
{
    if ((process == NULL) || (process->policy.scheduler_io_wakeup_deadline_ticks == 0u))
    {
        return 0u;
    }

    return process->policy.scheduler_io_wakeup_deadline_ticks;
}

static u32 userspace_capability_admission_limit(const struct user_process *process)
{
    if ((process == NULL) || (process->policy.capability_admission_limit == 0u))
    {
        return USER_CAPABILITY_LIMIT;
    }

    return process->policy.capability_admission_limit;
}

static u32 userspace_active_capability_count(const struct user_process *process)
{
    u32 slot;
    u32 count = 0u;

    if (process == NULL)
    {
        return 0u;
    }

    for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
    {
        if (process->capabilities[slot].active)
        {
            ++count;
        }
    }

    return count;
}

static u32 userspace_scheduler_class_index(const struct user_process *process)
{
    if ((process == NULL) || (process->policy.scheduler_class >= USER_SCHEDULER_CLASS_COUNT))
    {
        return USER_SCHEDULER_CLASS_SANDBOXED;
    }

    return process->policy.scheduler_class;
}

static u32 userspace_scheduler_class_budget_ticks(u32 scheduler_class)
{
    switch (scheduler_class)
    {
        case USER_SCHEDULER_CLASS_INTERACTIVE:
            return 12u;

        case USER_SCHEDULER_CLASS_STANDARD:
            return 16u;

        case USER_SCHEDULER_CLASS_SANDBOXED:
        default:
            return 12u;
    }
}

static void userspace_refresh_budget_window(u32 now)
{
    if ((scheduler_budget_window_start_tick == 0u)
        || ((now - scheduler_budget_window_start_tick) >= USER_SCHEDULER_BUDGET_WINDOW_TICKS))
    {
        scheduler_budget_window_start_tick = now;
        memory_zero(scheduler_class_runtime_used, sizeof(scheduler_class_runtime_used));
    }
}

static u32 userspace_scheduler_class_budget_used(u32 scheduler_class)
{
    if (scheduler_class >= USER_SCHEDULER_CLASS_COUNT)
    {
        return 0u;
    }

    return scheduler_class_runtime_used[scheduler_class];
}

static int userspace_scheduler_class_within_budget(u32 scheduler_class)
{
    u32 budget_ticks;

    if (scheduler_class >= USER_SCHEDULER_CLASS_COUNT)
    {
        return 1;
    }

    budget_ticks = userspace_scheduler_class_budget_ticks(scheduler_class);
    if (budget_ticks == 0u)
    {
        return 1;
    }

    return userspace_scheduler_class_budget_used(scheduler_class) < budget_ticks;
}

static void userspace_account_runtime(const struct user_process *process, u32 elapsed_ticks)
{
    u32 scheduler_class;

    if ((process == NULL) || (elapsed_ticks == 0u))
    {
        return;
    }

    userspace_refresh_budget_window(pit_get_ticks());
    scheduler_class = userspace_scheduler_class_index(process);
    scheduler_class_runtime_used[scheduler_class] += elapsed_ticks;
}

static u32 userspace_process_mask_by_id(u32 process_id)
{
    if ((process_id == 0u) || (process_id > USER_PROCESS_LIMIT))
    {
        return 0u;
    }

    return 1u << (process_id - 1u);
}

static const char *userspace_capability_type_name(u32 object_type)
{
    switch (object_type)
    {
        case USER_CAPABILITY_TYPE_ENDPOINT:
            return "endpoint";

        case USER_CAPABILITY_TYPE_SHARED_BUFFER:
            return "buffer";

        case USER_CAPABILITY_TYPE_RAMFS_NODE:
            return "ramfs-node";

        default:
            return "unknown-object";
    }
}

static int userspace_tick_before(u32 left, u32 right)
{
    return (s32)(left - right) < 0;
}

static struct user_process *userspace_process_for_id(u32 process_id)
{
    if ((process_id == 0u) || (process_id > USER_PROCESS_LIMIT))
    {
        return NULL;
    }

    if (processes[process_id - 1u].state == USER_PROCESS_UNUSED)
    {
        return NULL;
    }

    return &processes[process_id - 1u];
}

static const char *userspace_process_name_for_id(u32 process_id)
{
    struct user_process *process = userspace_process_for_id(process_id);

    if (process == NULL)
    {
        return "kernel";
    }

    return process->name;
}

static const char *userspace_role_name(u32 role)
{
    switch (role)
    {
        case USER_ENDPOINT_ROLE_PEER:
            return "peer";

        case USER_ENDPOINT_ROLE_POLICY:
            return "policy";

        default:
            return "unknown-role";
    }
}

static const char *userspace_service_class_name(u32 endpoint_class)
{
    switch (endpoint_class)
    {
        case SERVICE_ENDPOINT_CLASS_AI_POLICY:
            return "ai-policy";

        case SERVICE_ENDPOINT_CLASS_INIT:
            return "init";

        case SERVICE_ENDPOINT_CLASS_DRIVER_HOST:
            return "driver-host";

        case SERVICE_ENDPOINT_CLASS_CONSOLE:
            return "console";

        case SERVICE_ENDPOINT_CLASS_RAMFS:
            return "ramfs";

        case SERVICE_ENDPOINT_CLASS_INPUT:
            return "input";

        default:
            return "unknown-service";
    }
}

static const struct user_builtin_executable *userspace_find_executable(u32 executable_id)
{
    u32 index;

    for (index = 0; index < USER_EXECUTABLE_LIMIT; ++index)
    {
        if (builtin_executables[index].loaded
            && (builtin_executables[index].id == executable_id))
        {
            return &builtin_executables[index];
        }
    }

    return NULL;
}

static u32 userspace_loaded_executable_count(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0; index < USER_EXECUTABLE_LIMIT; ++index)
    {
        if (builtin_executables[index].loaded)
        {
            ++count;
        }
    }

    return count;
}

static const struct user_trusted_signer *userspace_find_signer(u32 signer_id)
{
    u32 index;

    for (index = 0; index < USER_TRUSTED_SIGNER_LIMIT; ++index)
    {
        if (trusted_signers[index].id == signer_id)
        {
            return &trusted_signers[index];
        }
    }

    return NULL;
}

static u32 userspace_measure_package_signature(
    const struct user_builtin_executable *executable,
    u32 verification_token)
{
    u32 digest;

    if (executable == NULL)
    {
        return 0u;
    }

    digest = userspace_measure_string_digest(executable->package_name);
    digest ^= userspace_measure_string_digest(executable->name);
    digest ^= userspace_measure_string_digest(executable->process_name);
    digest ^= executable->package_id;
    digest ^= executable->package_version << 4;
    digest ^= executable->expected_image_size << 8;
    digest ^= executable->expected_image_checksum;
    digest ^= executable->trust_flags << 12;
    digest ^= executable->launch_authority_mask << 16;
    digest ^= executable->max_instances << 20;
    digest ^= executable->launch_role << 24;
    digest ^= verification_token;
    digest *= 16777619u;
    return digest;
}

static u32 userspace_active_package_instances(u32 package_id)
{
    u32 index;
    u32 count = 0u;

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        if ((processes[index].state != USER_PROCESS_UNUSED)
            && (processes[index].executable_id != 0u)
            && (userspace_find_executable(processes[index].executable_id) != NULL)
            && (userspace_find_executable(processes[index].executable_id)->package_id == package_id))
        {
            ++count;
        }
    }

    return count;
}

static void userspace_note_manifest_denied(
    const struct user_builtin_executable *executable,
    const char *reason)
{
    ++total_manifest_denials;

    if ((executable != NULL) && (total_manifest_denials <= 6u))
    {
        klog_write_string("[userspace] deny manifest ");
        klog_write_string(executable->package_name);
        klog_write_string(" reason ");
        klog_write_string(reason);
        klog_newline();
    }
}

static int userspace_verify_launch_manifest(const struct user_builtin_executable *executable)
{
    u32 image_size;
    u32 measured_digest;
    int authority_allowed = 0;

    if (executable == NULL)
    {
        return 0;
    }

    image_size = (u32)(executable->image_end - executable->image_start);
    measured_digest = userspace_measure_image_digest(executable->image_start, image_size);

    if ((executable->trust_flags & USER_MANIFEST_TRUST_MEASURED) != 0u)
    {
        if ((image_size != executable->expected_image_size)
            || (measured_digest != executable->expected_image_checksum))
        {
            userspace_note_manifest_denied(executable, "measurement");
            return 0;
        }
    }

    if (((executable->trust_flags & USER_MANIFEST_TRUST_POLICY_APPROVED) != 0u)
        && (bootstrap_policy_approved == 0u))
    {
        userspace_note_manifest_denied(executable, "policy");
        return 0;
    }

    if (executable->launch_authority_mask == 0u)
    {
        authority_allowed = 1;
    }

    if (((executable->launch_authority_mask & USER_MANIFEST_LAUNCH_AUTHORITY_INIT) != 0u)
        && (services_current_service_id() == SERVICE_ID_INIT))
    {
        authority_allowed = 1;
    }

    if (((executable->launch_authority_mask & USER_MANIFEST_LAUNCH_AUTHORITY_SESSION) != 0u)
        && (current_process != NULL)
        && (current_process->launch_role == USER_LAUNCH_ROLE_SESSION_SHELL))
    {
        authority_allowed = 1;
    }

    if (!authority_allowed)
    {
        userspace_note_manifest_denied(executable, "authority");
        return 0;
    }

    if ((executable->max_instances != 0u)
        && (userspace_active_package_instances(executable->package_id) >= executable->max_instances))
    {
        userspace_note_manifest_denied(executable, "instances");
        return 0;
    }

    ++total_manifest_verifications;
    if (total_manifest_verifications <= 4u)
    {
        klog_write_string("[userspace] verify manifest ");
        klog_write_string(executable->package_name);
        klog_write_string(" signer ");
        klog_write_dec_u32(executable->signer_id);
        klog_write_string(" size ");
        klog_write_dec_u32(image_size);
        klog_write_string(" digest ");
        klog_write_hex_u32(measured_digest);
        klog_newline();
    }

    return 1;
}

static u32 userspace_active_process_count(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        if (processes[index].state != USER_PROCESS_UNUSED)
        {
            ++count;
        }
    }

    return count;
}

static struct user_endpoint *userspace_endpoint_for_role(
    struct user_process *process,
    u32 role)
{
    u32 slot;

    if (process == NULL)
    {
        return NULL;
    }

    for (slot = 0; slot < USER_ENDPOINTS_PER_PROCESS; ++slot)
    {
        if (process->endpoints[slot].role == role)
        {
            return &process->endpoints[slot];
        }
    }

    return NULL;
}

static struct user_endpoint *userspace_find_endpoint(
    u32 endpoint_id,
    struct user_process **owner_out)
{
    u32 index;
    u32 slot;

    if (owner_out != NULL)
    {
        *owner_out = NULL;
    }

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        if (processes[index].state == USER_PROCESS_UNUSED)
        {
            continue;
        }

        for (slot = 0; slot < USER_ENDPOINTS_PER_PROCESS; ++slot)
        {
            if (processes[index].endpoints[slot].id == endpoint_id)
            {
                if (owner_out != NULL)
                {
                    *owner_out = &processes[index];
                }

                return &processes[index].endpoints[slot];
            }
        }
    }

    return NULL;
}

static struct user_endpoint *userspace_find_endpoint_by_class(
    u32 endpoint_class,
    struct user_process **owner_out)
{
    u32 index;
    u32 slot;

    if (owner_out != NULL)
    {
        *owner_out = NULL;
    }

    if (endpoint_class == 0u)
    {
        return NULL;
    }

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        if (processes[index].state == USER_PROCESS_UNUSED)
        {
            continue;
        }

        for (slot = 0; slot < USER_ENDPOINTS_PER_PROCESS; ++slot)
        {
            if (processes[index].endpoints[slot].registered
                && (processes[index].endpoints[slot].endpoint_class == endpoint_class))
            {
                if (owner_out != NULL)
                {
                    *owner_out = &processes[index];
                }

                return &processes[index].endpoints[slot];
            }
        }
    }

    return NULL;
}

static struct user_process *userspace_process_for_endpoint(u32 endpoint_id)
{
    struct user_process *owner = NULL;
    struct user_endpoint *endpoint = userspace_find_endpoint(endpoint_id, &owner);

    if ((endpoint == NULL) || !endpoint->registered)
    {
        return NULL;
    }

    return owner;
}

static struct user_shared_buffer *userspace_shared_buffer_for_id(
    struct user_process *process,
    u32 buffer_id)
{
    u32 slot;

    if (process == NULL)
    {
        return NULL;
    }

    for (slot = 0; slot < USER_SHARED_BUFFER_LIMIT; ++slot)
    {
        if (process->shared_buffers[slot].registered
            && (process->shared_buffers[slot].id == buffer_id))
        {
            return &process->shared_buffers[slot];
        }
    }

    return NULL;
}

static struct user_shared_buffer *userspace_find_shared_buffer(
    u32 buffer_id,
    struct user_process **owner_out)
{
    u32 index;

    if (owner_out != NULL)
    {
        *owner_out = NULL;
    }

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        struct user_shared_buffer *buffer;

        if (processes[index].state == USER_PROCESS_UNUSED)
        {
            continue;
        }

        buffer = userspace_shared_buffer_for_id(&processes[index], buffer_id);
        if (buffer != NULL)
        {
            if (owner_out != NULL)
            {
                *owner_out = &processes[index];
            }

            return buffer;
        }
    }

    return NULL;
}

static const char *userspace_scheduler_class_name(u32 scheduler_class)
{
    switch (scheduler_class)
    {
        case USER_SCHEDULER_CLASS_INTERACTIVE:
            return "interactive";

        case USER_SCHEDULER_CLASS_STANDARD:
            return "standard";

        case USER_SCHEDULER_CLASS_SANDBOXED:
            return "sandboxed";

        default:
            return "unknown-class";
    }
}

static u32 userspace_scheduler_class_rank(const struct user_process *process)
{
    if (process == NULL)
    {
        return 0u;
    }

    switch (process->policy.scheduler_class)
    {
        case USER_SCHEDULER_CLASS_INTERACTIVE:
            return 2u;

        case USER_SCHEDULER_CLASS_STANDARD:
            return 1u;

        case USER_SCHEDULER_CLASS_SANDBOXED:
        default:
            return 0u;
    }
}

static const char *userspace_wake_source_name(u32 wake_source)
{
    switch (wake_source)
    {
        case USER_WAKE_SOURCE_TIMER:
            return "timer";

        case USER_WAKE_SOURCE_IPC:
            return "ipc";

        case USER_WAKE_SOURCE_INPUT:
            return "input";

        case USER_WAKE_SOURCE_NONE:
        default:
            return "none";
    }
}

static const char *userspace_endpoint_name_from_id(u32 endpoint_id)
{
    struct user_endpoint *endpoint = userspace_find_endpoint(endpoint_id, NULL);

    if ((endpoint == NULL) || (endpoint->name == NULL))
    {
        return "unknown-user-endpoint";
    }

    return endpoint->name;
}

static const char *userspace_target_name_from_id(u32 endpoint_id)
{
    struct user_endpoint *endpoint = userspace_find_endpoint(endpoint_id, NULL);

    if ((endpoint != NULL) && endpoint->registered && (endpoint->name != NULL))
    {
        return endpoint->name;
    }

    if (services_endpoint_exists(endpoint_id))
    {
        return services_endpoint_name(endpoint_id);
    }

    return ipc_endpoint_name(endpoint_id);
}

static void userspace_write_object_target(u32 object_type, u32 object_id)
{
    if (object_type == USER_CAPABILITY_TYPE_ENDPOINT)
    {
        klog_write_string(userspace_target_name_from_id(object_id));
        return;
    }

    if (object_type == USER_CAPABILITY_TYPE_SHARED_BUFFER)
    {
        struct user_process *owner = NULL;
        struct user_shared_buffer *buffer = userspace_find_shared_buffer(object_id, &owner);

        klog_write_string("buffer ");
        klog_write_dec_u32(object_id);
        if ((buffer != NULL) && (owner != NULL))
        {
            klog_write_string(" owner ");
            klog_write_string(owner->name);
        }

        return;
    }

    if (object_type == USER_CAPABILITY_TYPE_RAMFS_NODE)
    {
        klog_write_string(ramfs_node_name(object_id));
        return;
    }

    klog_write_string("unknown-object");
}

static int userspace_endpoint_is_delegable(u32 endpoint_id)
{
    struct user_endpoint *endpoint = userspace_find_endpoint(endpoint_id, NULL);

    if (endpoint != NULL)
    {
        return endpoint->delegable != 0u;
    }

    return services_endpoint_is_delegable(endpoint_id);
}

static int userspace_policy_allows_role(
    const struct user_process *process,
    u32 role)
{
    u32 role_bit;

    if (process == NULL)
    {
        return 0;
    }

    role_bit = userspace_role_policy_bit(role);
    return (role_bit != 0u)
        && ((process->policy.allowed_endpoint_role_mask & role_bit) != 0u);
}

static int userspace_policy_allows_service_class(
    const struct user_process *process,
    u32 endpoint_class)
{
    u32 class_bit;

    if (process == NULL)
    {
        return 0;
    }

    class_bit = userspace_service_class_policy_bit(endpoint_class);
    return (class_bit != 0u)
        && ((process->policy.allowed_service_class_mask & class_bit) != 0u);
}

static int userspace_endpoint_allows_sender(
    const struct user_endpoint *destination_endpoint,
    u32 sender_process_id)
{
    return (destination_endpoint->allowed_sender_mask
        & userspace_process_mask_by_id(sender_process_id)) != 0u;
}

static void userspace_mark_runnable_with_source(
    struct user_process *process,
    u32 ready_tick,
    u32 wake_source)
{
    if (process == NULL)
    {
        return;
    }

    process->state = USER_PROCESS_RUNNABLE;
    process->ready_since_tick = ready_tick;
    process->wake_source = wake_source;
    process->wakeup_deadline_tick = 0u;

    if (((wake_source == USER_WAKE_SOURCE_IPC)
            || (wake_source == USER_WAKE_SOURCE_INPUT))
        && (userspace_io_wakeup_deadline_ticks(process) != 0u))
    {
        process->wakeup_deadline_tick = ready_tick + userspace_io_wakeup_deadline_ticks(process);
        ++process->io_wake_count;
        ++total_io_wakes;

        if (total_io_wakes <= 4u)
        {
            klog_write_string("[userspace] wake deadline ");
            klog_write_string(process->name);
            klog_write_string(" class ");
            klog_write_string(userspace_scheduler_class_name(process->policy.scheduler_class));
            klog_write_string(" source ");
            klog_write_string(userspace_wake_source_name(wake_source));
            klog_write_string(" due ");
            klog_write_dec_u32(process->wakeup_deadline_tick);
            klog_newline();
        }
    }
}

static void userspace_mark_runnable(
    struct user_process *process,
    u32 ready_tick)
{
    userspace_mark_runnable_with_source(process, ready_tick, USER_WAKE_SOURCE_NONE);
}

static void userspace_note_policy_wait_started(struct user_process *process)
{
    if ((process == NULL)
        || (process->policy.scheduler_class != USER_SCHEDULER_CLASS_INTERACTIVE)
        || (process->policy_wait_pending != 0u))
    {
        return;
    }

    process->policy_wait_pending = 1u;
    ++interactive_policy_waiters;
}

static void userspace_note_policy_wait_resolved(
    struct user_process *process,
    u32 source_endpoint,
    u32 type)
{
    if ((process == NULL)
        || (process->policy_wait_pending == 0u)
        || (source_endpoint != IPC_ENDPOINT_AI_POLICY)
        || (type != IPC_MESSAGE_POLICY_APPROVED))
    {
        return;
    }

    process->policy_wait_pending = 0u;
    if (interactive_policy_waiters != 0u)
    {
        --interactive_policy_waiters;
    }
}

static void userspace_note_policy_wait_cancel(struct user_process *process)
{
    if ((process == NULL) || (process->policy_wait_pending == 0u))
    {
        return;
    }

    process->policy_wait_pending = 0u;
    if (interactive_policy_waiters != 0u)
    {
        --interactive_policy_waiters;
    }
}

static struct user_capability *userspace_find_capability(
    struct user_process *process,
    u32 handle)
{
    u32 slot;

    if (process == NULL)
    {
        return NULL;
    }

    for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
    {
        if (process->capabilities[slot].active
            && (process->capabilities[slot].handle == handle))
        {
            return &process->capabilities[slot];
        }
    }

    return NULL;
}

static int userspace_capability_was_expired(
    const struct user_process *process,
    u32 handle)
{
    u32 slot;

    if (process == NULL)
    {
        return 0;
    }

    for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
    {
        if (!process->capabilities[slot].active
            && process->capabilities[slot].expired
            && (process->capabilities[slot].handle == handle))
        {
            return 1;
        }
    }

    return 0;
}

static int userspace_capability_was_stale(
    const struct user_process *process,
    u32 handle)
{
    u32 slot;

    if (process == NULL)
    {
        return 0;
    }

    for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
    {
        if (!process->capabilities[slot].active
            && process->capabilities[slot].stale
            && (process->capabilities[slot].handle == handle))
        {
            return 1;
        }
    }

    return 0;
}

static void userspace_reset_capability(
    struct user_capability *capability,
    u32 mark_expired,
    u32 mark_stale)
{
    if (capability == NULL)
    {
        return;
    }

    capability->handle = 0u;
    capability->object_type = USER_CAPABILITY_TYPE_NONE;
    capability->object_id = 0u;
    capability->rights = 0u;
    capability->object_owner_process_id = 0u;
    capability->object_owner_generation = 0u;
    capability->issued_by_process_id = 0u;
    capability->issued_by_process_generation = 0u;
    capability->parent_handle = 0u;
    capability->lease_expiry_tick = 0u;
    capability->active = 0u;
    capability->expired = mark_expired;
    capability->stale = mark_stale;
}

static int userspace_capability_matches_live_object(
    const struct user_capability *capability)
{
    struct user_process *owner_process;

    if (capability == NULL)
    {
        return 0;
    }

    switch (capability->object_type)
    {
        case USER_CAPABILITY_TYPE_ENDPOINT:
            if (capability->object_owner_process_id == 0u)
            {
                return (capability->object_id != 0u)
                    && services_endpoint_exists(capability->object_id);
            }

            owner_process = userspace_process_for_id(capability->object_owner_process_id);
            if ((owner_process == NULL)
                || (owner_process->generation != capability->object_owner_generation))
            {
                return 0;
            }

            return userspace_find_endpoint(capability->object_id, NULL) != NULL;

        case USER_CAPABILITY_TYPE_SHARED_BUFFER:
            owner_process = userspace_process_for_id(capability->object_owner_process_id);
            if ((owner_process == NULL)
                || (owner_process->generation != capability->object_owner_generation))
            {
                return 0;
            }

            return userspace_shared_buffer_for_id(owner_process, capability->object_id) != NULL;

        case USER_CAPABILITY_TYPE_RAMFS_NODE:
            return ramfs_node_exists(capability->object_id);

        default:
            return 0;
    }
}

static void userspace_compact_capabilities(struct user_process *process, const char *reason)
{
    u32 read_slot;
    u32 write_slot = 0u;
    u32 moves = 0u;

    if (process == NULL)
    {
        return;
    }

    for (read_slot = 0; read_slot < USER_CAPABILITY_LIMIT; ++read_slot)
    {
        struct user_capability temporary;

        if (!process->capabilities[read_slot].active)
        {
            continue;
        }

        while ((write_slot < read_slot) && process->capabilities[write_slot].active)
        {
            ++write_slot;
        }

        if (write_slot >= read_slot)
        {
            ++write_slot;
            continue;
        }

        temporary = process->capabilities[write_slot];
        process->capabilities[write_slot] = process->capabilities[read_slot];
        process->capabilities[read_slot] = temporary;
        ++moves;
        ++write_slot;
    }

    if (moves != 0u)
    {
        ++total_capability_compactions;

        if (total_capability_compactions <= 4u)
        {
            klog_write_string("[userspace] compact caps ");
            klog_write_string(process->name);
            klog_write_string(" reason ");
            klog_write_string(reason);
            klog_write_string(" moves ");
            klog_write_dec_u32(moves);
            klog_newline();
        }
    }
}

static void userspace_note_policy_denied(
    struct user_process *process,
    const char *kind,
    const char *target)
{
    if (process == NULL)
    {
        return;
    }

    ++process->ipc_denied_count;
    ++total_ipc_denied;
    ++total_policy_denials;

    if (total_policy_denials <= 6u)
    {
        klog_write_string("[userspace] denied profile ");
        klog_write_string(process->name);
        klog_write_string(" ");
        klog_write_string(kind);
        klog_write_string(" ");
        klog_write_string(target);
        klog_newline();
    }
}

static void userspace_note_denied_capability(
    struct user_process *process,
    u32 handle,
    const char *action)
{
    if (process == NULL)
    {
        return;
    }

    ++process->ipc_denied_count;
    ++total_ipc_denied;

    if (total_ipc_denied <= 8u)
    {
        klog_write_string("[userspace] denied ");
        klog_write_string(action);
        klog_write_string(" ");
        klog_write_string(process->name);
        klog_write_string(" handle ");
        klog_write_dec_u32(handle);
        klog_newline();
    }
}

static u32 userspace_grant_capability(
    struct user_process *process,
    u32 object_type,
    u32 object_id,
    u32 rights,
    u32 object_owner_process_id,
    u32 object_owner_generation,
    u32 issued_by_process_id,
    u32 issued_by_process_generation,
    u32 parent_handle,
    u32 lease_expiry_tick)
{
    u32 slot;

    if (process == NULL)
    {
        return 0xFFFFFFFFu;
    }

    for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
    {
        if (process->capabilities[slot].active
            && (process->capabilities[slot].object_type == object_type)
            && (process->capabilities[slot].object_id == object_id)
            && (process->capabilities[slot].rights == rights)
            && (process->capabilities[slot].object_owner_process_id == object_owner_process_id)
            && (process->capabilities[slot].object_owner_generation == object_owner_generation)
            && (process->capabilities[slot].issued_by_process_id == issued_by_process_id)
            && (process->capabilities[slot].issued_by_process_generation == issued_by_process_generation)
            && (process->capabilities[slot].parent_handle == parent_handle))
        {
            if ((process->capabilities[slot].lease_expiry_tick == USER_CAPABILITY_LEASE_NONE)
                || (lease_expiry_tick == USER_CAPABILITY_LEASE_NONE))
            {
                process->capabilities[slot].lease_expiry_tick = USER_CAPABILITY_LEASE_NONE;
            }
            else if (lease_expiry_tick > process->capabilities[slot].lease_expiry_tick)
            {
                process->capabilities[slot].lease_expiry_tick = lease_expiry_tick;
            }

            process->capabilities[slot].expired = 0u;
            ++total_capability_reuses;

            if (total_capability_reuses <= 6u)
            {
                klog_write_string("[userspace] reuse cap ");
                klog_write_string(process->name);
                klog_write_string(" -> ");
                userspace_write_object_target(object_type, object_id);
                klog_write_string(" handle ");
                klog_write_dec_u32(process->capabilities[slot].handle);
                klog_newline();
            }

            return process->capabilities[slot].handle;
        }
    }

    if (userspace_active_capability_count(process) >= userspace_capability_admission_limit(process))
    {
        ++process->ipc_denied_count;
        ++total_ipc_denied;
        ++total_capability_admission_denials;

        if (total_capability_admission_denials <= 4u)
        {
            klog_write_string("[userspace] admission denied ");
            klog_write_string(process->name);
            klog_write_string(" caps ");
            klog_write_dec_u32(userspace_active_capability_count(process));
            klog_write_string(" limit ");
            klog_write_dec_u32(userspace_capability_admission_limit(process));
            klog_write_string(" target ");
            userspace_write_object_target(object_type, object_id);
            klog_newline();
        }

        return 0xFFFFFFFFu;
    }

    for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
    {
        if (!process->capabilities[slot].active)
        {
            process->capabilities[slot].active = 1u;
            process->capabilities[slot].handle = userspace_allocate_capability_handle();
            process->capabilities[slot].object_type = object_type;
            process->capabilities[slot].object_id = object_id;
            process->capabilities[slot].rights = rights;
            process->capabilities[slot].object_owner_process_id = object_owner_process_id;
            process->capabilities[slot].object_owner_generation = object_owner_generation;
            process->capabilities[slot].issued_by_process_id = issued_by_process_id;
            process->capabilities[slot].issued_by_process_generation = issued_by_process_generation;
            process->capabilities[slot].parent_handle = parent_handle;
            process->capabilities[slot].lease_expiry_tick = lease_expiry_tick;
            process->capabilities[slot].expired = 0u;
            process->capabilities[slot].stale = 0u;
            ++total_capability_grants;

            if (total_capability_grants <= 8u)
            {
                klog_write_string("[userspace] grant cap ");
                klog_write_string(process->name);
                klog_write_string(" -> ");
                userspace_write_object_target(object_type, object_id);
                klog_write_string(" handle ");
                klog_write_dec_u32(process->capabilities[slot].handle);
                klog_write_string(" rights ");
                klog_write_hex_u32(process->capabilities[slot].rights);
                if (issued_by_process_id != 0u)
                {
                    klog_write_string(" issuer ");
                    klog_write_string(userspace_process_name_for_id(issued_by_process_id));
                }
                if (lease_expiry_tick != USER_CAPABILITY_LEASE_NONE)
                {
                    klog_write_string(" expires ");
                    klog_write_dec_u32(lease_expiry_tick);
                }
                klog_newline();
            }

            return process->capabilities[slot].handle;
        }
    }

    return 0xFFFFFFFFu;
}

static struct user_capability *userspace_resolve_capability(
    struct user_process *process,
    u32 handle,
    u32 required_type,
    u32 required_rights,
    const char *action)
{
    struct user_capability *capability = userspace_find_capability(process, handle);

    if (capability == NULL)
    {
        if (userspace_capability_was_expired(process, handle))
        {
            userspace_note_denied_capability(process, handle, "expired cap");
        }
        else if (userspace_capability_was_stale(process, handle))
        {
            userspace_note_denied_capability(process, handle, "stale cap");
        }
        else
        {
            userspace_note_denied_capability(process, handle, action);
        }

        return NULL;
    }

    if ((required_type != USER_CAPABILITY_TYPE_NONE)
        && (capability->object_type != required_type))
    {
        userspace_note_denied_capability(process, handle, action);
        return NULL;
    }

    if ((capability->rights & required_rights) != required_rights)
    {
        userspace_note_denied_capability(process, handle, action);
        return NULL;
    }

    if (!userspace_capability_matches_live_object(capability))
    {
        capability->active = 0u;
        capability->expired = 0u;
        capability->stale = 1u;
        userspace_note_denied_capability(process, handle, "stale cap");
        return NULL;
    }

    return capability;
}

static u32 userspace_resolve_capability_endpoint(
    struct user_process *process,
    u32 handle,
    u32 required_rights,
    const char *action)
{
    struct user_capability *capability = userspace_resolve_capability(
        process,
        handle,
        USER_CAPABILITY_TYPE_ENDPOINT,
        required_rights,
        action);

    if (capability == NULL)
    {
        return 0xFFFFFFFFu;
    }

    return capability->object_id;
}

static struct user_shared_buffer *userspace_resolve_capability_shared_buffer(
    struct user_process *process,
    u32 handle,
    u32 required_rights,
    const char *action,
    struct user_process **owner_out)
{
    struct user_capability *capability = userspace_resolve_capability(
        process,
        handle,
        USER_CAPABILITY_TYPE_SHARED_BUFFER,
        required_rights,
        action);
    struct user_process *owner_process;

    if (owner_out != NULL)
    {
        *owner_out = NULL;
    }

    if (capability == NULL)
    {
        return NULL;
    }

    owner_process = userspace_process_for_id(capability->object_owner_process_id);
    if ((owner_process == NULL)
        || (owner_process->generation != capability->object_owner_generation))
    {
        capability->active = 0u;
        capability->expired = 0u;
        capability->stale = 1u;
        userspace_note_denied_capability(process, handle, "stale cap");
        return NULL;
    }

    if (owner_out != NULL)
    {
        *owner_out = owner_process;
    }

    return userspace_shared_buffer_for_id(owner_process, capability->object_id);
}

static u32 userspace_resolve_capability_ramfs_node(
    struct user_process *process,
    u32 handle,
    u32 required_rights,
    const char *action)
{
    struct user_capability *capability = userspace_resolve_capability(
        process,
        handle,
        USER_CAPABILITY_TYPE_RAMFS_NODE,
        required_rights,
        action);

    if (capability == NULL)
    {
        return 0xFFFFFFFFu;
    }

    return capability->object_id;
}

static u32 userspace_resolve_service_capability(
    struct user_process *process,
    u32 handle,
    u32 endpoint_class,
    const char *action)
{
    u32 endpoint_id = userspace_resolve_capability_endpoint(
        process,
        handle,
        USER_CAPABILITY_RIGHT_SEND,
        action);

    if (endpoint_id == 0xFFFFFFFFu)
    {
        return 0xFFFFFFFFu;
    }

    if (endpoint_id != services_resolve_endpoint_class(endpoint_class))
    {
        userspace_note_denied_capability(process, handle, action);
        return 0xFFFFFFFFu;
    }

    return endpoint_id;
}

static u32 userspace_copy_bytes_from_shared_buffer_capability(
    struct user_process *process,
    u32 capability_handle,
    u32 buffer_offset,
    u8 *destination_bytes,
    u32 byte_count)
{
    struct user_process *owner_process = NULL;
    struct user_shared_buffer *shared_buffer;

    if ((process == NULL) || (destination_bytes == NULL))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    shared_buffer = userspace_resolve_capability_shared_buffer(
        process,
        capability_handle,
        USER_CAPABILITY_RIGHT_BUFFER_READ,
        "stale cap",
        &owner_process);
    if ((shared_buffer == NULL) || (owner_process == NULL))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((buffer_offset > shared_buffer->byte_length)
        || ((shared_buffer->byte_length - buffer_offset) < byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!userspace_copy_bytes_from_process(
            owner_process,
            shared_buffer->virtual_address + buffer_offset,
            destination_bytes,
            byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    return IPC_STATUS_OK;
}

static u32 userspace_copy_bytes_to_shared_buffer_capability(
    struct user_process *process,
    u32 capability_handle,
    u32 buffer_offset,
    const u8 *source_bytes,
    u32 byte_count)
{
    struct user_process *owner_process = NULL;
    struct user_shared_buffer *shared_buffer;

    if ((process == NULL) || (source_bytes == NULL))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    shared_buffer = userspace_resolve_capability_shared_buffer(
        process,
        capability_handle,
        USER_CAPABILITY_RIGHT_BUFFER_WRITE,
        "stale cap",
        &owner_process);
    if ((shared_buffer == NULL) || (owner_process == NULL))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((buffer_offset > shared_buffer->byte_length)
        || ((shared_buffer->byte_length - buffer_offset) < byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!userspace_copy_bytes_to_process(
            owner_process,
            shared_buffer->virtual_address + buffer_offset,
            source_bytes,
            byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    return IPC_STATUS_OK;
}

static int userspace_copy_bytes_from_process(
    struct user_process *process,
    u32 source_address,
    u8 *destination_bytes,
    u32 byte_count)
{
    u32 restore_address_space;

    if ((process == NULL)
        || (destination_bytes == NULL)
        || !userspace_region_is_valid(source_address, byte_count))
    {
        return 0;
    }

    restore_address_space = userspace_restore_address_space();
    paging_switch_address_space(process->address_space);
    memory_copy(destination_bytes, (const void *)source_address, byte_count);
    paging_switch_address_space(restore_address_space);
    return 1;
}

static int userspace_copy_bytes_to_process(
    struct user_process *process,
    u32 destination_address,
    const u8 *source_bytes,
    u32 byte_count)
{
    u32 restore_address_space;

    if ((process == NULL)
        || (source_bytes == NULL)
        || !userspace_region_is_valid(destination_address, byte_count))
    {
        return 0;
    }

    restore_address_space = userspace_restore_address_space();
    paging_switch_address_space(process->address_space);
    memory_copy((void *)destination_address, source_bytes, byte_count);
    paging_switch_address_space(restore_address_space);
    return 1;
}

static int userspace_mailbox_peek(
    const struct user_process *process,
    struct user_ipc_message *message)
{
    if ((process == NULL) || (message == NULL) || (process->mailbox_count == 0u))
    {
        return 0;
    }

    *message = process->mailbox[process->mailbox_head];
    return 1;
}

static void userspace_mailbox_consume(struct user_process *process)
{
    if ((process == NULL) || (process->mailbox_count == 0u))
    {
        return;
    }

    process->mailbox_head = (process->mailbox_head + 1u) % USER_MAILBOX_CAPACITY;
    --process->mailbox_count;
}

static s32 userspace_mailbox_push(
    struct user_process *process,
    const struct user_ipc_message *message)
{
    if ((process == NULL) || (message == NULL))
    {
        return IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    if (process->mailbox_count == USER_MAILBOX_CAPACITY)
    {
        return IPC_STATUS_QUEUE_FULL;
    }

    process->mailbox[process->mailbox_tail] = *message;
    process->mailbox_tail = (process->mailbox_tail + 1u) % USER_MAILBOX_CAPACITY;
    ++process->mailbox_count;
    return IPC_STATUS_OK;
}

static void userspace_wake_sleepers(void)
{
    u32 index;
    u32 now = pit_get_ticks();

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        if ((processes[index].state == USER_PROCESS_SLEEPING)
            && userspace_tick_reached(now, processes[index].wake_tick))
        {
            userspace_mark_runnable_with_source(&processes[index], now, USER_WAKE_SOURCE_TIMER);
        }

        {
            u32 slot;
            int expired_capability = 0;

            for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
            {
                struct user_capability *capability = &processes[index].capabilities[slot];

                if (capability->active
                    && (capability->lease_expiry_tick != USER_CAPABILITY_LEASE_NONE)
                    && userspace_tick_reached(now, capability->lease_expiry_tick))
                {
                    capability->active = 0u;
                    capability->expired = 1u;
                    capability->stale = 0u;
                    expired_capability = 1;
                    ++total_capability_expirations;

                    if (total_capability_expirations <= 4u)
                    {
                        klog_write_string("[userspace] expire cap ");
                        klog_write_string(processes[index].name);
                        klog_write_string(" handle ");
                        klog_write_dec_u32(capability->handle);
                        klog_write_string(" target ");
                        userspace_write_object_target(capability->object_type, capability->object_id);
                        klog_newline();
                    }
                }
            }

            if (expired_capability)
            {
                userspace_compact_capabilities(&processes[index], "expiry");
            }
        }
    }
}

static void userspace_switch_to_process(struct user_process *process)
{
    u32 now = pit_get_ticks();
    u32 wait_ticks = now - process->ready_since_tick;
    int used_deadline = userspace_last_pick_used_deadline && (process->wakeup_deadline_tick != 0u);

    current_process = process;
    current_slice_ticks = 0;
    process->state = USER_PROCESS_RUNNING;
    process->dispatch_count += 1u;
    ++total_user_dispatches;

    if (used_deadline)
    {
        ++process->deadline_pick_count;
        ++total_deadline_picks;

        if (total_deadline_picks <= 4u)
        {
            klog_write_string("[userspace] deadline pick ");
            klog_write_string(process->name);
            klog_write_string(" class ");
            klog_write_string(userspace_scheduler_class_name(process->policy.scheduler_class));
            klog_write_string(" source ");
            klog_write_string(userspace_wake_source_name(process->wake_source));
            if (userspace_tick_reached(now, process->wakeup_deadline_tick))
            {
                klog_write_string(" overdue ");
                klog_write_dec_u32(now - process->wakeup_deadline_tick);
            }
            else
            {
                klog_write_string(" remaining ");
                klog_write_dec_u32(process->wakeup_deadline_tick - now);
            }
            klog_newline();
        }
    }

    if (userspace_last_pick_used_latency)
    {
        ++process->latency_pick_count;
        ++total_latency_picks;

        if (total_latency_picks <= 4u)
        {
            klog_write_string("[userspace] latency pick ");
            klog_write_string(process->name);
            klog_write_string(" waited ");
            klog_write_dec_u32(wait_ticks);
            klog_write_string(" target ");
            klog_write_dec_u32(userspace_latency_target_ticks(process));
            klog_newline();
        }
    }

    process->wake_source = USER_WAKE_SOURCE_NONE;
    process->wakeup_deadline_tick = 0u;

    if (total_user_dispatches <= 6u)
    {
        klog_write_string("[userspace] dispatch ");
        klog_write_string(process->name);
        klog_write_string(" class ");
        klog_write_string(userspace_scheduler_class_name(process->policy.scheduler_class));
        klog_write_string(" weight ");
        klog_write_dec_u32(process->policy.scheduler_weight);
        klog_write_string(" latency ");
        klog_write_dec_u32(process->policy.scheduler_latency_target_ticks);
        klog_write_string(" vr ");
        klog_write_dec_u32(process->virtual_runtime);
        if (used_deadline)
        {
            klog_write_string(" deadline");
        }
        if (userspace_last_pick_used_latency)
        {
            klog_write_string(" overdue ");
            klog_write_dec_u32(wait_ticks);
        }
        klog_newline();
    }

    userspace_set_kernel_stack(process->kernel_stack_top);
    paging_switch_address_space(process->address_space);
}

static int userspace_has_budget_eligible_candidate(u32 start_index)
{
    u32 count;

    for (count = 0; count < USER_PROCESS_LIMIT; ++count)
    {
        u32 index = (start_index + count) % USER_PROCESS_LIMIT;
        struct user_process *candidate = &processes[index];

        if ((candidate->state != USER_PROCESS_RUNNABLE)
            || (candidate->wakeup_deadline_tick != 0u))
        {
            continue;
        }

        if (userspace_scheduler_class_within_budget(
                userspace_scheduler_class_index(candidate)))
        {
            return 1;
        }
    }

    return 0;
}

static struct user_process *userspace_select_runnable(
    u32 start_index,
    u32 now,
    int enforce_budget,
    int *found_overdue_out,
    struct user_process **budget_blocked_out)
{
    u32 count;
    struct user_process *best_process = NULL;
    u32 best_class_rank = 0u;
    u32 best_virtual_runtime = 0u;
    u32 best_wait_ticks = 0u;
    u32 best_latency_target = 0u;
    u32 best_overdue_ticks = 0u;
    int found_overdue = 0;

    if (found_overdue_out != NULL)
    {
        *found_overdue_out = 0;
    }

    if (budget_blocked_out != NULL)
    {
        *budget_blocked_out = NULL;
    }

    for (count = 0; count < USER_PROCESS_LIMIT; ++count)
    {
        u32 index = (start_index + count) % USER_PROCESS_LIMIT;
        struct user_process *candidate = &processes[index];
        u32 age_ticks;
        u32 class_rank;
        u32 latency_target;

        if ((candidate->state != USER_PROCESS_RUNNABLE)
            || (candidate->wakeup_deadline_tick != 0u))
        {
            continue;
        }

        if (enforce_budget
            && !userspace_scheduler_class_within_budget(
                userspace_scheduler_class_index(candidate)))
        {
            if ((budget_blocked_out != NULL) && (*budget_blocked_out == NULL))
            {
                *budget_blocked_out = candidate;
            }

            continue;
        }

        age_ticks = now - candidate->ready_since_tick;
        class_rank = userspace_scheduler_class_rank(candidate);
        latency_target = userspace_latency_target_ticks(candidate);

        if (age_ticks >= latency_target)
        {
            u32 overdue_ticks = age_ticks - latency_target;

            if (!found_overdue
                || (overdue_ticks > best_overdue_ticks)
                || ((overdue_ticks == best_overdue_ticks)
                    && (class_rank > best_class_rank))
                || ((overdue_ticks == best_overdue_ticks)
                    && (class_rank == best_class_rank)
                    && (latency_target < best_latency_target))
                || ((overdue_ticks == best_overdue_ticks)
                    && (class_rank == best_class_rank)
                    && (latency_target == best_latency_target)
                    && (candidate->virtual_runtime < best_virtual_runtime)))
            {
                best_process = candidate;
                best_class_rank = class_rank;
                best_virtual_runtime = candidate->virtual_runtime;
                best_wait_ticks = age_ticks;
                best_latency_target = latency_target;
                best_overdue_ticks = overdue_ticks;
                found_overdue = 1;
            }

            continue;
        }

        if (found_overdue)
        {
            continue;
        }

        if ((best_process == NULL)
            || (candidate->virtual_runtime < best_virtual_runtime))
        {
            best_process = candidate;
            best_class_rank = class_rank;
            best_virtual_runtime = candidate->virtual_runtime;
            best_wait_ticks = age_ticks;
            best_latency_target = latency_target;
            continue;
        }

        if ((candidate->virtual_runtime == best_virtual_runtime)
            && (class_rank > best_class_rank))
        {
            best_process = candidate;
            best_class_rank = class_rank;
            best_wait_ticks = age_ticks;
            best_latency_target = latency_target;
            continue;
        }

        if ((candidate->virtual_runtime == best_virtual_runtime)
            && (class_rank == best_class_rank)
            && ((age_ticks > best_wait_ticks)
                || ((age_ticks == best_wait_ticks)
                    && (latency_target < best_latency_target))))
        {
            best_process = candidate;
            best_class_rank = class_rank;
            best_wait_ticks = age_ticks;
            best_latency_target = latency_target;
        }
    }

    if (found_overdue_out != NULL)
    {
        *found_overdue_out = found_overdue;
    }

    return best_process;
}

static struct user_process *userspace_next_process(void)
{
    u32 start_index = 0;
    u32 now = pit_get_ticks();
    struct user_process *best_process = NULL;
    u32 best_class_rank = 0u;
    u32 best_virtual_runtime = 0u;
    u32 best_deadline_tick = 0u;
    u32 count;
    int budget_enforced;
    int found_overdue = 0;
    int found_deadline = 0;
    struct user_process *budget_blocked_process = NULL;

    userspace_last_pick_used_deadline = 0;
    userspace_last_pick_used_latency = 0;
    userspace_refresh_budget_window(now);

    if (current_process != NULL)
    {
        start_index = current_process->id % USER_PROCESS_LIMIT;
    }

    for (count = 0; count < USER_PROCESS_LIMIT; ++count)
    {
        u32 index = (start_index + count) % USER_PROCESS_LIMIT;
        struct user_process *candidate = &processes[index];
        u32 class_rank;

        if (candidate->state != USER_PROCESS_RUNNABLE)
        {
            continue;
        }

        class_rank = userspace_scheduler_class_rank(candidate);

        if (candidate->wakeup_deadline_tick != 0u)
        {
            if (!found_deadline
                || userspace_tick_before(candidate->wakeup_deadline_tick, best_deadline_tick)
                || ((candidate->wakeup_deadline_tick == best_deadline_tick)
                    && (class_rank > best_class_rank))
                || ((candidate->wakeup_deadline_tick == best_deadline_tick)
                    && (class_rank == best_class_rank)
                    && (candidate->virtual_runtime < best_virtual_runtime)))
            {
                best_process = candidate;
                best_class_rank = class_rank;
                best_virtual_runtime = candidate->virtual_runtime;
                best_deadline_tick = candidate->wakeup_deadline_tick;
                found_deadline = 1;
            }
        }
    }

    if (found_deadline)
    {
        userspace_last_pick_used_deadline = 1;
        return best_process;
    }

    budget_enforced = userspace_has_budget_eligible_candidate(start_index);
    best_process = userspace_select_runnable(
        start_index,
        now,
        budget_enforced,
        &found_overdue,
        &budget_blocked_process);

    if ((best_process == NULL) && budget_enforced)
    {
        budget_blocked_process = NULL;
        best_process = userspace_select_runnable(
            start_index,
            now,
            0,
            &found_overdue,
            &budget_blocked_process);
    }

    if ((best_process != NULL) && budget_enforced && (budget_blocked_process != NULL))
    {
        u32 blocked_class = userspace_scheduler_class_index(budget_blocked_process);

        ++total_budget_throttles;
        if (total_budget_throttles <= 4u)
        {
            klog_write_string("[userspace] budget throttle ");
            klog_write_string(budget_blocked_process->name);
            klog_write_string(" class ");
            klog_write_string(userspace_scheduler_class_name(budget_blocked_process->policy.scheduler_class));
            klog_write_string(" used ");
            klog_write_dec_u32(userspace_scheduler_class_budget_used(blocked_class));
            klog_write_string(" budget ");
            klog_write_dec_u32(userspace_scheduler_class_budget_ticks(blocked_class));
            klog_newline();
        }
    }

    if (found_overdue)
    {
        userspace_last_pick_used_latency = 1;
    }

    return best_process;
}

static struct interrupt_frame *userspace_enter_idle(void)
{
    current_process = NULL;
    current_slice_ticks = 0;
    userspace_set_kernel_stack(kernel_idle_stack_top);
    paging_switch_address_space(paging_get_page_directory_address());
    return kernel_idle_frame;
}

static struct interrupt_frame *userspace_switch_frame(
    struct interrupt_frame *frame,
    int is_preemptive)
{
    int did_yield = 0;
    struct user_process *previous_process;
    struct user_process *next_process;

    if ((current_process == NULL) || !userspace_active)
    {
        return frame;
    }

    previous_process = current_process;
    previous_process->frame = *frame;
    previous_process->runtime_ticks += current_slice_ticks;
    userspace_account_runtime(previous_process, current_slice_ticks);
    previous_process->virtual_runtime += userspace_runtime_cost(
        previous_process,
        current_slice_ticks);
    if (is_preemptive)
    {
        userspace_mark_runnable(previous_process, pit_get_ticks());
        ++previous_process->preempt_count;
        ++total_preemptions;
    }
    else
    {
        if (previous_process->state == USER_PROCESS_RUNNING)
        {
            userspace_mark_runnable(previous_process, pit_get_ticks());
            ++previous_process->yield_count;
            ++total_yields;
            did_yield = 1;
        }
    }

    next_process = userspace_next_process();
    if (next_process == NULL)
    {
        return userspace_enter_idle();
    }

    userspace_switch_to_process(next_process);

    if ((is_preemptive && total_preemptions <= 4u)
        || (did_yield && total_yields <= 4u))
    {
        klog_write_string("[userspace] switch ");
        klog_write_string(previous_process->name);
        klog_write_string(" -> ");
        klog_write_string(current_process->name);
        klog_write_string(is_preemptive ? " preempt" : " yield");
        if (userspace_last_pick_used_deadline)
        {
            klog_write_string(" deadline");
        }
        if (userspace_last_pick_used_latency)
        {
            klog_write_string(" latency");
        }
        klog_newline();
    }

    return &current_process->frame;
}

static void userspace_log_process_profile(const struct user_process *process)
{
    if (process == NULL)
    {
        return;
    }

    klog_write_string("[userspace] profile ");
    klog_write_string(process->name);
    klog_write_string(" ");
    klog_write_string(process->policy.name);
    klog_write_string(" roles ");
    klog_write_hex_u32(process->policy.allowed_endpoint_role_mask);
    klog_write_string(" services ");
    klog_write_hex_u32(process->policy.allowed_service_class_mask);
    klog_write_string(" class ");
    klog_write_string(userspace_scheduler_class_name(process->policy.scheduler_class));
    klog_write_string(" weight ");
    klog_write_dec_u32(process->policy.scheduler_weight);
    klog_write_string(" latency ");
    klog_write_dec_u32(process->policy.scheduler_latency_target_ticks);
    klog_write_string(" io-deadline ");
    klog_write_dec_u32(process->policy.scheduler_io_wakeup_deadline_ticks);
    klog_write_string(" budget ");
    klog_write_dec_u32(userspace_scheduler_class_budget_ticks(process->policy.scheduler_class));
    klog_write_string(" cap-limit ");
    klog_write_dec_u32(process->policy.capability_admission_limit);
    klog_newline();
}

static void userspace_log_trusted_signers(void)
{
    u32 index;

    for (index = 0; index < USER_TRUSTED_SIGNER_LIMIT; ++index)
    {
        if (trusted_signers[index].id == 0u)
        {
            continue;
        }

        klog_write_string("[userspace] signer ");
        klog_write_string(trusted_signers[index].name);
        klog_write_string(" id ");
        klog_write_dec_u32(trusted_signers[index].id);
        klog_newline();
    }
}

static void userspace_configure_trusted_signers(void)
{
    struct package_store_signer_record record;
    u32 index;
    u32 count = package_store_signer_count();

    memory_zero(trusted_signers, sizeof(trusted_signers));

    for (index = 0; (index < USER_TRUSTED_SIGNER_LIMIT) && (index < count); ++index)
    {
        if (!package_store_read_signer(index, &record))
        {
            continue;
        }

        trusted_signers[index].id = record.id;
        trusted_signers[index].name = record.name;
        trusted_signers[index].verification_token = record.verification_token;
    }
}

static void userspace_note_package_rejection(
    const struct user_builtin_executable *candidate,
    const char *reason)
{
    ++total_package_manifest_rejections;

    if ((candidate != NULL) && (total_package_manifest_rejections <= 4u))
    {
        klog_write_string("[userspace] reject package ");
        klog_write_string(candidate->package_name);
        klog_write_string(" source ");
        klog_write_dec_u32(candidate->source_slot);
        klog_write_string(" reason ");
        klog_write_string(reason);
        klog_newline();
    }
}

static int userspace_verify_store_candidate(const struct user_builtin_executable *candidate)
{
    const struct user_trusted_signer *signer;
    u32 measured_signature;

    if (candidate == NULL)
    {
        return 0;
    }

    signer = userspace_find_signer(candidate->signer_id);
    if (signer == NULL)
    {
        ++total_signer_denials;
        userspace_note_package_rejection(candidate, "signer");
        return 0;
    }

    measured_signature = userspace_measure_package_signature(candidate, signer->verification_token);
    if (measured_signature != candidate->signature_token)
    {
        ++total_signer_denials;
        userspace_note_package_rejection(candidate, "signature");
        return 0;
    }

    ++total_signer_verifications;
    return 1;
}

static int userspace_manifest_supported_on_x86(u32 launch_authority_mask)
{
    return (launch_authority_mask
        & (USER_MANIFEST_LAUNCH_AUTHORITY_INIT | USER_MANIFEST_LAUNCH_AUTHORITY_SESSION)) != 0u;
}

static void userspace_load_package_store(void)
{
    struct package_store_manifest_record manifest;
    const u8 *payload_start;
    const u8 *payload_end;
    u32 candidate_index;
    u32 loaded_index = 0u;
    u32 manifest_count = package_store_manifest_count();

    memory_zero(package_store_candidates, sizeof(package_store_candidates));
    memory_zero(builtin_executables, sizeof(builtin_executables));

    for (candidate_index = 0; candidate_index < manifest_count; ++candidate_index)
    {
        struct user_builtin_executable *candidate;

        if (!package_store_read_manifest(candidate_index, &manifest))
        {
            ++total_package_manifest_rejections;
            continue;
        }

        if (!userspace_manifest_supported_on_x86(manifest.launch_authority_mask))
        {
            continue;
        }

        if (candidate_index >= USER_PACKAGE_STORE_CANDIDATE_LIMIT)
        {
            ++total_package_manifest_rejections;
            continue;
        }

        if (!package_store_read_payload(manifest.payload_slot, &payload_start, &payload_end))
        {
            package_store_candidates[candidate_index].package_name = manifest.package_name;
            package_store_candidates[candidate_index].source_slot = manifest.source_slot;
            userspace_note_package_rejection(&package_store_candidates[candidate_index], "payload");
            continue;
        }

        candidate = &package_store_candidates[candidate_index];
        memory_zero(candidate, sizeof(*candidate));
        candidate->source_slot = manifest.source_slot;
        candidate->package_id = manifest.package_id;
        candidate->package_name = manifest.package_name;
        candidate->package_version = manifest.package_version;
        candidate->signer_id = manifest.signer_id;
        candidate->signature_token = manifest.signature_token;
        candidate->trust_flags = manifest.trust_flags;
        candidate->launch_authority_mask = manifest.launch_authority_mask;
        candidate->max_instances = manifest.max_instances;
        candidate->expected_image_size = manifest.expected_image_size;
        candidate->expected_image_checksum = manifest.expected_image_checksum;
        candidate->id = manifest.executable_id;
        candidate->name = manifest.name;
        candidate->process_name = manifest.process_name;
        candidate->profile_name = manifest.profile_name;
        candidate->peer_endpoint_name = manifest.peer_endpoint_name;
        candidate->policy_endpoint_name = manifest.policy_endpoint_name;
        candidate->image_start = payload_start;
        candidate->image_end = payload_end;
        candidate->allowed_endpoint_role_mask = manifest.allowed_endpoint_role_mask;
        candidate->allowed_service_class_mask = manifest.allowed_service_class_mask;
        candidate->scheduler_class = manifest.scheduler_class;
        candidate->scheduler_weight = manifest.scheduler_weight;
        candidate->scheduler_latency_target_ticks = manifest.scheduler_latency_target_ticks;
        candidate->scheduler_io_wakeup_deadline_ticks = manifest.scheduler_io_wakeup_deadline_ticks;
        candidate->capability_admission_limit = manifest.capability_admission_limit;
        candidate->launch_role = manifest.launch_role;

        if (!userspace_verify_store_candidate(candidate))
        {
            continue;
        }

        if (loaded_index >= USER_EXECUTABLE_LIMIT)
        {
            userspace_note_package_rejection(candidate, "catalog");
            continue;
        }

        builtin_executables[loaded_index] = *candidate;
        builtin_executables[loaded_index].loaded = 1u;
        ++total_package_manifest_loads;

        if (total_package_manifest_loads <= USER_EXECUTABLE_LIMIT)
        {
            klog_write_string("[userspace] load manifest ");
            klog_write_string(builtin_executables[loaded_index].package_name);
            klog_write_string(" source ");
            klog_write_dec_u32(builtin_executables[loaded_index].source_slot);
            klog_newline();
        }

        ++loaded_index;
    }
}

static struct user_process *userspace_allocate_process_slot(u32 *process_id_out)
{
    u32 index;

    if (process_id_out != NULL)
    {
        *process_id_out = 0u;
    }

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        if (processes[index].state == USER_PROCESS_UNUSED)
        {
            if (process_id_out != NULL)
            {
                *process_id_out = index + 1u;
            }

            return &processes[index];
        }
    }

    return NULL;
}

u32 userspace_executable_count(void)
{
    return userspace_loaded_executable_count();
}

u32 userspace_package_manifest_count(void)
{
    return total_package_manifest_loads;
}

void userspace_note_bootstrap_policy_approved(void)
{
    bootstrap_policy_approved = 1u;
}

u32 userspace_spawn_builtin(u32 executable_id)
{
    const struct user_builtin_executable *executable = userspace_find_executable(executable_id);
    struct user_process *process;
    u32 process_id;
    u32 image_size;
    u32 code_page_count;
    u32 page_index;

    if (!userspace_ready || (executable == NULL))
    {
        return 0xFFFFFFFFu;
    }

    if (!userspace_verify_launch_manifest(executable))
    {
        return 0xFFFFFFFFu;
    }

    process = userspace_allocate_process_slot(&process_id);
    if (process == NULL)
    {
        klog_write_string("[userspace] no free slot for ");
        klog_write_string((executable == NULL) ? "unknown" : executable->process_name);
        klog_newline();
        return 0xFFFFFFFFu;
    }

    image_size = (u32)(executable->image_end - executable->image_start);
    code_page_count = (image_size + USER_PAGE_SIZE - 1u) / USER_PAGE_SIZE;
    if ((image_size == 0u)
        || (code_page_count == 0u)
        || (code_page_count > USER_CODE_PAGE_LIMIT))
    {
        klog_write_line("[userspace] invalid executable image");
        cpu_cli();
        cpu_halt_forever();
    }

    memory_zero(process, sizeof(*process));
    process->id = process_id;
    process_generation_counters[process_id - 1u] += 1u;
    process->generation = process_generation_counters[process_id - 1u];
    process->executable_id = executable->id;
    process->launch_role = executable->launch_role;
    process->code_page_count = code_page_count;
    process->name = executable->process_name;
    process->policy.name = executable->profile_name;
    process->policy.allowed_endpoint_role_mask = executable->allowed_endpoint_role_mask;
    process->policy.allowed_service_class_mask = executable->allowed_service_class_mask;
    process->policy.scheduler_class = executable->scheduler_class;
    process->policy.scheduler_weight = executable->scheduler_weight;
    process->policy.scheduler_latency_target_ticks = executable->scheduler_latency_target_ticks;
    process->policy.scheduler_io_wakeup_deadline_ticks = executable->scheduler_io_wakeup_deadline_ticks;
    process->policy.capability_admission_limit = executable->capability_admission_limit;
    process->endpoints[0].id = userspace_endpoint_id(process_id, 0u);
    process->endpoints[0].name = executable->peer_endpoint_name;
    process->endpoints[0].role = USER_ENDPOINT_ROLE_PEER;
    process->endpoints[1].id = userspace_endpoint_id(process_id, 1u);
    process->endpoints[1].name = executable->policy_endpoint_name;
    process->endpoints[1].role = USER_ENDPOINT_ROLE_POLICY;
    process->endpoints[2].id = userspace_endpoint_id(process_id, 2u);
    process->endpoints[3].id = userspace_endpoint_id(process_id, 3u);
    process->state = USER_PROCESS_RUNNABLE;
    process->ready_since_tick = pit_get_ticks();
    process->wake_source = USER_WAKE_SOURCE_NONE;
    for (page_index = 0u; page_index < USER_CODE_PAGE_LIMIT; ++page_index)
    {
        process->code_pages[page_index] = 0xFFFFFFFFu;
    }

    for (page_index = 0u; page_index < code_page_count; ++page_index)
    {
        process->code_pages[page_index] = memory_claim_frame();
    }

    process->stack_page = memory_claim_frame();
    process->kernel_stack_page = memory_claim_frame();

    for (page_index = 0u; page_index < code_page_count; ++page_index)
    {
        if (process->code_pages[page_index] == 0xFFFFFFFFu)
        {
            klog_write_line("[userspace] failed to claim process frames");
            cpu_cli();
            cpu_halt_forever();
        }
    }

    if ((process->stack_page == 0xFFFFFFFFu)
        || (process->kernel_stack_page == 0xFFFFFFFFu))
    {
        klog_write_line("[userspace] failed to claim process frames");
        cpu_cli();
        cpu_halt_forever();
    }

    for (page_index = 0u; page_index < code_page_count; ++page_index)
    {
        memory_zero((void *)process->code_pages[page_index], USER_PAGE_SIZE);
    }

    memory_zero((void *)process->stack_page, USER_PAGE_SIZE);
    memory_zero((void *)process->kernel_stack_page, USER_PAGE_SIZE);
    process->kernel_stack_top = process->kernel_stack_page + USER_PAGE_SIZE;
    for (page_index = 0u; page_index < code_page_count; ++page_index)
    {
        u32 copy_offset = page_index * USER_PAGE_SIZE;
        u32 copy_size = USER_PAGE_SIZE;

        if ((copy_offset + copy_size) > image_size)
        {
            copy_size = image_size - copy_offset;
        }

        memory_copy(
            (void *)process->code_pages[page_index],
            executable->image_start + copy_offset,
            copy_size);
    }

    process->address_space = paging_create_user_space(
        process->code_pages,
        code_page_count,
        process->stack_page);
    if (process->address_space == 0u)
    {
        klog_write_line("[userspace] failed to create process address space");
        cpu_cli();
        cpu_halt_forever();
    }

    memory_zero(&process->frame, sizeof(process->frame));
    process->frame.gs = GDT_SELECTOR_USER_DATA;
    process->frame.fs = GDT_SELECTOR_USER_DATA;
    process->frame.es = GDT_SELECTOR_USER_DATA;
    process->frame.ds = GDT_SELECTOR_USER_DATA;
    process->frame.ebx = process_id;
    process->frame.ecx = executable->launch_role;
    process->frame.edx = executable->id;
    process->frame.eip = PAGING_USER_CODE_VIRTUAL;
    process->frame.cs = 0x1Bu;
    process->frame.eflags = 0x202u;
    process->frame.user_esp = PAGING_USER_STACK_TOP - 16u;
    process->frame.user_ss = GDT_SELECTOR_USER_DATA;

    klog_write_string("[userspace] spawn ");
    klog_write_string(process->name);
    klog_write_string(" exec ");
    klog_write_string(executable->name);
    klog_write_string(" pkg ");
    klog_write_string(executable->package_name);
    klog_write_string(" pid ");
    klog_write_dec_u32(process->id);
    klog_newline();
    userspace_log_process_profile(process);

    return process->id;
}

u32 userspace_launch_executable(u32 executable_id)
{
    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    return userspace_spawn_builtin(executable_id);
}

void userspace_init(void)
{
    u32 index;

    memory_zero(gdt, sizeof(gdt));
    memory_zero(&tss, sizeof(tss));
    memory_zero(processes, sizeof(processes));
    memory_zero(process_generation_counters, sizeof(process_generation_counters));
    ramfs_init();
    input_init();
    package_store_init();
    if (!package_store_ready())
    {
        klog_write_line("[userspace] package store unavailable");
        cpu_cli();
        cpu_halt_forever();
    }
    userspace_configure_trusted_signers();
    userspace_load_package_store();

    if (userspace_loaded_executable_count() == 0u)
    {
        klog_write_line("[userspace] package store produced no launchable services");
        cpu_cli();
        cpu_halt_forever();
    }

    for (index = 0; index < userspace_loaded_executable_count(); ++index)
    {
        u32 image_size = (u32)(builtin_executables[index].image_end - builtin_executables[index].image_start);
        u32 code_page_count = (image_size + USER_PAGE_SIZE - 1u) / USER_PAGE_SIZE;

        if ((image_size == 0u) || (code_page_count == 0u) || (code_page_count > USER_CODE_PAGE_LIMIT))
        {
            klog_write_line("[userspace] executable image exceeds loader limit");
            cpu_cli();
            cpu_halt_forever();
        }
    }

    gdt_set_entry(1, 0u, 0x000FFFFFu, 0x9Au, 0xC0u);
    gdt_set_entry(2, 0u, 0x000FFFFFu, 0x92u, 0xC0u);
    gdt_set_entry(3, 0u, 0x000FFFFFu, 0xFAu, 0xC0u);
    gdt_set_entry(4, 0u, 0x000FFFFFu, 0xF2u, 0xC0u);

    tss.ss0 = GDT_SELECTOR_KERNEL_DATA;
    tss.iomap_base = sizeof(tss);
    gdt_set_entry(5, (u32)&tss, sizeof(tss) - 1u, 0x89u, 0x00u);

    userspace_load_gdt();

    {
        u32 idle_stack_page = memory_claim_frame();

        if (idle_stack_page == 0xFFFFFFFFu)
        {
            klog_write_line("[userspace] failed to claim idle stack");
            cpu_cli();
            cpu_halt_forever();
        }

        memory_zero((void *)idle_stack_page, USER_PAGE_SIZE);
        kernel_idle_stack_top = idle_stack_page + USER_PAGE_SIZE;
        kernel_idle_frame = (struct interrupt_frame *)(kernel_idle_stack_top - sizeof(struct interrupt_frame));
        memory_zero(kernel_idle_frame, sizeof(struct interrupt_frame));
        kernel_idle_frame->gs = GDT_SELECTOR_KERNEL_DATA;
        kernel_idle_frame->fs = GDT_SELECTOR_KERNEL_DATA;
        kernel_idle_frame->es = GDT_SELECTOR_KERNEL_DATA;
        kernel_idle_frame->ds = GDT_SELECTOR_KERNEL_DATA;
        kernel_idle_frame->eip = (u32)&x86_kernel_idle_loop;
        kernel_idle_frame->cs = 0x08u;
        kernel_idle_frame->eflags = 0x202u;
    }

    userspace_ready = 1;

    klog_write_string("[userspace] executables ");
    klog_write_dec_u32(userspace_executable_count());
    klog_write_string(" code ");
    klog_write_hex_u32(PAGING_USER_CODE_VIRTUAL);
    klog_write_string(" stack ");
    klog_write_hex_u32(PAGING_USER_STACK_TOP);
    klog_newline();

    userspace_log_trusted_signers();

    for (index = 0; index < userspace_loaded_executable_count(); ++index)
    {
        klog_write_string("[userspace] catalog ");
        klog_write_string(builtin_executables[index].name);
        klog_write_string(" pkg ");
        klog_write_string(builtin_executables[index].package_name);
        klog_write_string(" signer ");
        klog_write_dec_u32(builtin_executables[index].signer_id);
        klog_write_string(" -> ");
        klog_write_string(builtin_executables[index].process_name);
        klog_newline();
    }
}

void userspace_enter_session(void)
{
    struct user_process *entry_process;

    if (!userspace_ready)
    {
        klog_write_line("[userspace] runtime not ready");
        cpu_cli();
        cpu_halt_forever();
    }

    if (userspace_active_process_count() == 0u)
    {
        klog_write_line("[userspace] no spawned user services");
        cpu_cli();
        cpu_halt_forever();
    }

    userspace_active = 1;
    entry_process = userspace_next_process();
    if (entry_process == NULL)
    {
        klog_write_line("[userspace] no runnable user service");
        cpu_cli();
        cpu_halt_forever();
    }

    userspace_switch_to_process(entry_process);
    klog_write_string("[userspace] entering ring3 ");
    klog_write_line(entry_process->name);
    ((void (*)(struct interrupt_frame *))&x86_resume_user_frame)(&entry_process->frame);
    klog_write_line("[userspace] unexpected return from ring3");
    cpu_cli();
    cpu_halt_forever();
}

void userspace_note_syscall(void)
{
    ++user_syscalls;
    if (current_process != NULL)
    {
        ++current_process->syscall_count;
    }
}

u32 userspace_register_endpoint(u32 role, u32 allowed_sender_mask, u32 endpoint_class, u32 delegable)
{
    struct user_endpoint *endpoint;
    struct user_process *owner = NULL;
    struct user_endpoint *existing_endpoint;

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    endpoint = userspace_endpoint_for_role(current_process, role);
    if (endpoint == NULL)
    {
        return 0xFFFFFFFFu;
    }

    existing_endpoint = userspace_find_endpoint_by_class(endpoint_class, &owner);
    if ((existing_endpoint != NULL) && (existing_endpoint->id != endpoint->id))
    {
        return 0xFFFFFFFFu;
    }

    endpoint->allowed_sender_mask = allowed_sender_mask & ((1u << USER_PROCESS_LIMIT) - 1u);
    endpoint->endpoint_class = endpoint_class;
    endpoint->delegable = (delegable != 0u) ? 1u : 0u;

    if (!endpoint->registered)
    {
        endpoint->registered = 1u;
        ++total_registered_endpoints;
    }

    if (total_registered_endpoints <= (USER_PROCESS_LIMIT * USER_ENDPOINTS_PER_PROCESS))
    {
        klog_write_string("[userspace] register ");
        klog_write_string(endpoint->name);
        klog_write_string(" senders ");
        klog_write_hex_u32(endpoint->allowed_sender_mask);
        klog_write_string(" class ");
        klog_write_hex_u32(endpoint->endpoint_class);
        klog_write_string(" delegable ");
        klog_write_dec_u32(endpoint->delegable);
        klog_newline();
    }

    return endpoint->id;
}

u32 userspace_lookup_endpoint(u32 owner_process_id, u32 role)
{
    struct user_process *process = userspace_process_for_id(owner_process_id);
    struct user_endpoint *endpoint = userspace_endpoint_for_role(process, role);

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    if (!userspace_policy_allows_role(current_process, role))
    {
        userspace_note_policy_denied(current_process, "role", userspace_role_name(role));
        return 0xFFFFFFFFu;
    }

    if ((endpoint == NULL) || !endpoint->registered)
    {
        return 0xFFFFFFFFu;
    }

    if (!userspace_endpoint_allows_sender(endpoint, current_process->id))
    {
        ++current_process->ipc_denied_count;
        ++total_ipc_denied;

        if (total_ipc_denied <= 6u)
        {
            klog_write_string("[userspace] denied lookup ");
            klog_write_string(current_process->name);
            klog_write_string(" -> ");
            klog_write_string(endpoint->name);
            klog_newline();
        }

        return 0xFFFFFFFFu;
    }

    return userspace_grant_capability(
        current_process,
        USER_CAPABILITY_TYPE_ENDPOINT,
        endpoint->id,
        USER_CAPABILITY_RIGHT_SEND | USER_CAPABILITY_RIGHT_DELEGATE,
        process->id,
        process->generation,
        0u,
        0u,
        0u,
        USER_CAPABILITY_LEASE_NONE);
}

u32 userspace_lookup_endpoint_class(u32 endpoint_class)
{
    struct user_endpoint *endpoint;
    struct user_process *owner = NULL;

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    endpoint = userspace_find_endpoint_by_class(endpoint_class, &owner);
    if (endpoint == NULL)
    {
        return 0xFFFFFFFFu;
    }

    if (!userspace_policy_allows_role(current_process, endpoint->role))
    {
        userspace_note_policy_denied(current_process, "class", endpoint->name);
        return 0xFFFFFFFFu;
    }

    if (!userspace_endpoint_allows_sender(endpoint, current_process->id))
    {
        ++current_process->ipc_denied_count;
        ++total_ipc_denied;

        if (total_ipc_denied <= 8u)
        {
            klog_write_string("[userspace] denied class lookup ");
            klog_write_string(current_process->name);
            klog_write_string(" class ");
            klog_write_hex_u32(endpoint_class);
            klog_newline();
        }

        return 0xFFFFFFFFu;
    }

    if (total_endpoint_class_resolutions < 4u)
    {
        ++total_endpoint_class_resolutions;
        klog_write_string("[userspace] resolve endpoint class ");
        klog_write_hex_u32(endpoint_class);
        klog_write_string(" -> ");
        klog_write_string(endpoint->name);
        klog_newline();
    }

    return userspace_grant_capability(
        current_process,
        USER_CAPABILITY_TYPE_ENDPOINT,
        endpoint->id,
        USER_CAPABILITY_RIGHT_SEND | USER_CAPABILITY_RIGHT_DELEGATE,
        owner->id,
        owner->generation,
        0u,
        0u,
        0u,
        USER_CAPABILITY_LEASE_NONE);
}

u32 userspace_lookup_service_endpoint(u32 endpoint_class)
{
    u32 endpoint_id = services_resolve_endpoint_class(endpoint_class);

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    if (!userspace_policy_allows_service_class(current_process, endpoint_class))
    {
        userspace_note_policy_denied(current_process, "service", userspace_service_class_name(endpoint_class));
        return 0xFFFFFFFFu;
    }

    if (endpoint_id == 0xFFFFFFFFu)
    {
        return 0xFFFFFFFFu;
    }

    if (total_service_resolutions < 4u)
    {
        ++total_service_resolutions;
        klog_write_string("[userspace] resolve service class ");
        klog_write_dec_u32(endpoint_class);
        klog_write_string(" -> ");
        klog_write_string(services_endpoint_name(endpoint_id));
        klog_newline();
    }

    return userspace_grant_capability(
        current_process,
        USER_CAPABILITY_TYPE_ENDPOINT,
        endpoint_id,
        USER_CAPABILITY_RIGHT_SEND | USER_CAPABILITY_RIGHT_DELEGATE,
        0u,
        0u,
        0u,
        0u,
        0u,
        USER_CAPABILITY_LEASE_NONE);
}

u32 userspace_revoke_capability(u32 capability_handle)
{
    struct user_capability *capability;

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    capability = userspace_find_capability(current_process, capability_handle);
    if (capability == NULL)
    {
        userspace_note_denied_capability(current_process, capability_handle, "revoke");
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    userspace_reset_capability(capability, 0u, 0u);
    userspace_compact_capabilities(current_process, "revoke");
    ++total_capability_revocations;

    if (total_capability_revocations <= 4u)
    {
        klog_write_string("[userspace] revoke cap ");
        klog_write_string(current_process->name);
        klog_write_string(" handle ");
        klog_write_dec_u32(capability_handle);
        klog_newline();
    }

    return IPC_STATUS_OK;
}

u32 userspace_delegate_capability(
    u32 delegated_capability_handle,
    u32 recipient_endpoint_capability_handle)
{
    struct user_process *recipient_process = NULL;
    struct user_process *shared_buffer_owner = NULL;
    struct user_capability *source_capability;
    struct user_endpoint *recipient_endpoint;
    struct user_endpoint *delegated_user_endpoint = NULL;
    struct user_endpoint *source_peer_endpoint;
    struct user_shared_buffer *shared_buffer = NULL;
    u32 recipient_endpoint_id;
    u32 delegated_handle;
    u32 delegated_object_type;
    u32 delegated_object_id;
    u32 delegated_rights;
    u32 delegated_owner_process_id;
    u32 delegated_owner_generation;
    u32 lease_expiry_tick;
    u32 payload_words[4];
    s32 status;

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    source_capability = userspace_resolve_capability(
        current_process,
        delegated_capability_handle,
        USER_CAPABILITY_TYPE_NONE,
        USER_CAPABILITY_RIGHT_DELEGATE,
        "delegate source");
    if (source_capability == NULL)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    recipient_endpoint_id = userspace_resolve_capability_endpoint(
        current_process,
        recipient_endpoint_capability_handle,
        USER_CAPABILITY_RIGHT_SEND,
        "delegate target");
    if (recipient_endpoint_id == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    recipient_endpoint = userspace_find_endpoint(recipient_endpoint_id, &recipient_process);
    if ((recipient_endpoint == NULL) || !recipient_endpoint->registered)
    {
        return (u32)IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    delegated_object_type = source_capability->object_type;
    delegated_object_id = source_capability->object_id;
    delegated_owner_process_id = source_capability->object_owner_process_id;
    delegated_owner_generation = source_capability->object_owner_generation;
    delegated_rights = 0u;

    if (delegated_object_type == USER_CAPABILITY_TYPE_ENDPOINT)
    {
        if (!userspace_endpoint_is_delegable(delegated_object_id))
        {
            ++current_process->ipc_denied_count;
            ++total_ipc_denied;

            if (total_ipc_denied <= 10u)
            {
                klog_write_string("[userspace] denied policy delegate ");
                klog_write_string(current_process->name);
                klog_write_string(" source ");
                klog_write_string(userspace_target_name_from_id(delegated_object_id));
                klog_newline();
            }

            return (u32)IPC_STATUS_ACCESS_DENIED;
        }

        delegated_user_endpoint = userspace_find_endpoint(delegated_object_id, NULL);
        if ((delegated_user_endpoint != NULL)
            && !userspace_endpoint_allows_sender(delegated_user_endpoint, recipient_process->id))
        {
            ++current_process->ipc_denied_count;
            ++total_ipc_denied;

            if (total_ipc_denied <= 8u)
            {
                klog_write_string("[userspace] denied delegate ");
                klog_write_string(current_process->name);
                klog_write_string(" -> ");
                klog_write_string(recipient_endpoint->name);
                klog_newline();
            }

            return (u32)IPC_STATUS_ACCESS_DENIED;
        }

        delegated_rights = USER_CAPABILITY_RIGHT_SEND;
    }
    else if (delegated_object_type == USER_CAPABILITY_TYPE_SHARED_BUFFER)
    {
        shared_buffer = userspace_resolve_capability_shared_buffer(
            current_process,
            delegated_capability_handle,
            USER_CAPABILITY_RIGHT_DELEGATE,
            "delegate source",
            &shared_buffer_owner);
        if (shared_buffer == NULL)
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }

        if (shared_buffer->delegable == 0u)
        {
            ++current_process->ipc_denied_count;
            ++total_ipc_denied;

            if (total_ipc_denied <= 10u)
            {
                klog_write_string("[userspace] denied policy delegate ");
                klog_write_string(current_process->name);
                klog_write_string(" source buffer ");
                klog_write_dec_u32(shared_buffer->id);
                klog_newline();
            }

            return (u32)IPC_STATUS_ACCESS_DENIED;
        }

        delegated_rights = source_capability->rights
            & (USER_CAPABILITY_RIGHT_BUFFER_READ | USER_CAPABILITY_RIGHT_BUFFER_WRITE);
    }
    else if (delegated_object_type == USER_CAPABILITY_TYPE_RAMFS_NODE)
    {
        if (!ramfs_node_exists(delegated_object_id))
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }

        delegated_rights = source_capability->rights
            & (USER_CAPABILITY_RIGHT_NODE_LIST
                | USER_CAPABILITY_RIGHT_NODE_CREATE
                | USER_CAPABILITY_RIGHT_NODE_READ
                | USER_CAPABILITY_RIGHT_NODE_WRITE
                | USER_CAPABILITY_RIGHT_NODE_STAT
                | USER_CAPABILITY_RIGHT_NODE_RENAME
                | USER_CAPABILITY_RIGHT_NODE_DELETE);
    }
    else
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (delegated_rights == 0u)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    source_peer_endpoint = userspace_endpoint_for_role(current_process, USER_ENDPOINT_ROLE_PEER);
    if ((source_peer_endpoint == NULL) || !source_peer_endpoint->registered)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    lease_expiry_tick = pit_get_ticks() + USER_DELEGATED_CAPABILITY_LEASE_TICKS;
    delegated_handle = userspace_grant_capability(
        recipient_process,
        delegated_object_type,
        delegated_object_id,
        delegated_rights,
        delegated_owner_process_id,
        delegated_owner_generation,
        current_process->id,
        current_process->generation,
        delegated_capability_handle,
        lease_expiry_tick);
    if (delegated_handle == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_QUEUE_FULL;
    }

    payload_words[0] = delegated_handle;
    payload_words[1] = delegated_object_type;
    payload_words[2] = delegated_object_id;
    payload_words[3] = delegated_rights;
    status = userspace_deliver_message(
        recipient_endpoint_id,
        source_peer_endpoint->id,
        USER_MESSAGE_CAP_GRANTED,
        payload_words,
        4u);
    if (status == IPC_STATUS_OK)
    {
        ++total_capability_delegations;

        if (total_capability_delegations <= 4u)
        {
            klog_write_string("[userspace] delegate cap ");
            klog_write_string(current_process->name);
            klog_write_string(" -> ");
            klog_write_string(recipient_endpoint->name);
            klog_write_string(" for ");
            userspace_write_object_target(delegated_object_type, delegated_object_id);
            klog_write_string(" type ");
            klog_write_string(userspace_capability_type_name(delegated_object_type));
            klog_write_string(" handle ");
            klog_write_dec_u32(delegated_handle);
            klog_newline();
        }
    }
    else
    {
        struct user_capability *recipient_capability = userspace_find_capability(recipient_process, delegated_handle);

        userspace_reset_capability(recipient_capability, 0u, 0u);
        userspace_compact_capabilities(recipient_process, "delegate");
    }

    return (u32)status;
}

u32 userspace_request_policy(u32 policy_capability_handle, u32 request_code)
{
    struct user_endpoint *policy_endpoint;
    struct ipc_message message;
    u32 resolved_policy_endpoint;
    u32 policy_endpoint_id;
    u32 wait_pending_before;
    s32 status;

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    policy_endpoint = userspace_endpoint_for_role(current_process, USER_ENDPOINT_ROLE_POLICY);
    if ((policy_endpoint == NULL) || !policy_endpoint->registered)
    {
        return 0xFFFFFFFFu;
    }

    policy_endpoint_id = userspace_resolve_capability_endpoint(
        current_process,
        policy_capability_handle,
        USER_CAPABILITY_RIGHT_SEND,
        "stale cap");
    if (policy_endpoint_id == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    resolved_policy_endpoint = services_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_AI_POLICY);
    if ((resolved_policy_endpoint == 0xFFFFFFFFu)
        || (resolved_policy_endpoint != policy_endpoint_id))
    {
        return (u32)IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    message.source_endpoint = policy_endpoint->id;
    message.type = IPC_MESSAGE_POLICY_HANDSHAKE;
    message.arg0 = request_code;
    message.arg1 = current_process->id;
    message.flags = (current_process->policy.scheduler_class == USER_SCHEDULER_CLASS_INTERACTIVE)
        ? IPC_MESSAGE_FLAG_INTERACTIVE_WAIT
        : 0u;
    message.dependency_depth = 1u;

    wait_pending_before = current_process->policy_wait_pending;
    userspace_note_policy_wait_started(current_process);
    status = ipc_send(policy_endpoint_id, &message);
    if (status == IPC_STATUS_OK)
    {
        ++total_policy_requests;

        if (total_policy_requests <= 4u)
        {
            klog_write_string("[userspace] policy request ");
            klog_write_string(current_process->name);
            klog_write_string(" code ");
            klog_write_dec_u32(request_code);
            klog_newline();
        }
    }
    else
    {
        if (wait_pending_before == 0u)
        {
            userspace_note_policy_wait_cancel(current_process);
        }

        if (status == IPC_STATUS_QUEUE_FULL)
        {
            ++current_process->ipc_denied_count;
            ++total_ipc_denied;

            if (total_ipc_denied <= 10u)
            {
                klog_write_string("[userspace] broker busy ");
                klog_write_string(current_process->name);
                klog_write_string(" endpoint ");
                klog_write_dec_u32(policy_endpoint_id);
                klog_newline();
            }
        }
    }

    return (u32)status;
}

u32 userspace_register_shared_buffer(u32 buffer_address, u32 byte_length, u32 delegable)
{
    u32 slot;
    struct user_shared_buffer *shared_buffer = NULL;
    u32 rights;

    if ((current_process == NULL) || !userspace_active)
    {
        return 0xFFFFFFFFu;
    }

    if ((byte_length == 0u)
        || (byte_length > USER_PAGE_SIZE)
        || !userspace_region_is_valid(buffer_address, byte_length))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    for (slot = 0; slot < USER_SHARED_BUFFER_LIMIT; ++slot)
    {
        if (current_process->shared_buffers[slot].registered
            && (current_process->shared_buffers[slot].virtual_address == buffer_address)
            && (current_process->shared_buffers[slot].byte_length == byte_length))
        {
            shared_buffer = &current_process->shared_buffers[slot];
            break;
        }

        if ((shared_buffer == NULL) && !current_process->shared_buffers[slot].registered)
        {
            shared_buffer = &current_process->shared_buffers[slot];
        }
    }

    if (shared_buffer == NULL)
    {
        return (u32)IPC_STATUS_QUEUE_FULL;
    }

    if (!shared_buffer->registered)
    {
        shared_buffer->id = userspace_shared_buffer_id(current_process->id, slot);
        shared_buffer->virtual_address = buffer_address;
        shared_buffer->byte_length = byte_length;
        shared_buffer->delegable = (delegable != 0u) ? 1u : 0u;
        shared_buffer->registered = 1u;
        ++total_shared_buffer_registrations;

        if (total_shared_buffer_registrations <= 4u)
        {
            klog_write_string("[userspace] register buffer ");
            klog_write_string(current_process->name);
            klog_write_string(" id ");
            klog_write_dec_u32(shared_buffer->id);
            klog_write_string(" bytes ");
            klog_write_dec_u32(byte_length);
            klog_write_string(" delegable ");
            klog_write_dec_u32(shared_buffer->delegable);
            klog_newline();
        }
    }
    else
    {
        shared_buffer->delegable = (delegable != 0u) ? 1u : 0u;
    }

    rights = USER_CAPABILITY_RIGHT_BUFFER_READ | USER_CAPABILITY_RIGHT_BUFFER_WRITE;
    if (shared_buffer->delegable)
    {
        rights |= USER_CAPABILITY_RIGHT_DELEGATE;
    }

    return userspace_grant_capability(
        current_process,
        USER_CAPABILITY_TYPE_SHARED_BUFFER,
        shared_buffer->id,
        rights,
        current_process->id,
        current_process->generation,
        0u,
        0u,
        0u,
        USER_CAPABILITY_LEASE_NONE);
}

static u32 userspace_copy_shared_buffer(
    u32 capability_handle,
    u32 buffer_offset,
    u32 local_buffer_address,
    u32 byte_count,
    u32 required_rights)
{
    struct user_process *owner_process = NULL;
    struct user_shared_buffer *shared_buffer;
    u32 processed = 0u;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((byte_count == 0u)
        || !userspace_region_is_valid(local_buffer_address, byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    shared_buffer = userspace_resolve_capability_shared_buffer(
        current_process,
        capability_handle,
        required_rights,
        "stale cap",
        &owner_process);
    if ((shared_buffer == NULL) || (owner_process == NULL))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((buffer_offset > shared_buffer->byte_length)
        || ((shared_buffer->byte_length - buffer_offset) < byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    while (processed < byte_count)
    {
        u32 chunk_bytes = byte_count - processed;

        if (chunk_bytes > USER_BUFFER_COPY_SCRATCH_BYTES)
        {
            chunk_bytes = USER_BUFFER_COPY_SCRATCH_BYTES;
        }

        if (required_rights == USER_CAPABILITY_RIGHT_BUFFER_WRITE)
        {
            if (!userspace_copy_bytes_from_current(
                    local_buffer_address + processed,
                    chunk_bytes,
                    buffer_copy_scratch))
            {
                return (u32)IPC_STATUS_ACCESS_DENIED;
            }

            if (!userspace_copy_bytes_to_process(
                    owner_process,
                    shared_buffer->virtual_address + buffer_offset + processed,
                    buffer_copy_scratch,
                    chunk_bytes))
            {
                return (u32)IPC_STATUS_ACCESS_DENIED;
            }
        }
        else
        {
            if (!userspace_copy_bytes_from_process(
                    owner_process,
                    shared_buffer->virtual_address + buffer_offset + processed,
                    buffer_copy_scratch,
                    chunk_bytes))
            {
                return (u32)IPC_STATUS_ACCESS_DENIED;
            }

            memory_copy(
                (void *)(local_buffer_address + processed),
                buffer_copy_scratch,
                chunk_bytes);
        }

        processed += chunk_bytes;
    }

    ++total_shared_buffer_copies;
    if (total_shared_buffer_copies <= 4u)
    {
        klog_write_string("[userspace] buffer ");
        klog_write_string((required_rights == USER_CAPABILITY_RIGHT_BUFFER_WRITE) ? "write " : "read ");
        klog_write_string(current_process->name);
        klog_write_string(" handle ");
        klog_write_dec_u32(capability_handle);
        klog_write_string(" bytes ");
        klog_write_dec_u32(byte_count);
        klog_newline();
    }

    return IPC_STATUS_OK;
}

u32 userspace_read_shared_buffer(
    u32 capability_handle,
    u32 buffer_offset,
    u32 local_buffer_address,
    u32 byte_count)
{
    return userspace_copy_shared_buffer(
        capability_handle,
        buffer_offset,
        local_buffer_address,
        byte_count,
        USER_CAPABILITY_RIGHT_BUFFER_READ);
}

u32 userspace_write_shared_buffer(
    u32 capability_handle,
    u32 buffer_offset,
    u32 local_buffer_address,
    u32 byte_count)
{
    return userspace_copy_shared_buffer(
        capability_handle,
        buffer_offset,
        local_buffer_address,
        byte_count,
        USER_CAPABILITY_RIGHT_BUFFER_WRITE);
}

u32 userspace_console_write(
    u32 console_capability_handle,
    u32 buffer_capability_handle,
    u32 buffer_offset,
    u32 byte_count)
{
    u32 processed = 0u;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_resolve_service_capability(
            current_process,
            console_capability_handle,
            SERVICE_ENDPOINT_CLASS_CONSOLE,
            "console") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    while (processed < byte_count)
    {
        u32 chunk_bytes = byte_count - processed;

        if (chunk_bytes > USER_BUFFER_COPY_SCRATCH_BYTES)
        {
            chunk_bytes = USER_BUFFER_COPY_SCRATCH_BYTES;
        }

        if (userspace_copy_bytes_from_shared_buffer_capability(
                current_process,
                buffer_capability_handle,
                buffer_offset + processed,
                buffer_copy_scratch,
                chunk_bytes) != IPC_STATUS_OK)
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }

        klog_write_bytes(buffer_copy_scratch, chunk_bytes);
        processed += chunk_bytes;
    }

    ++total_console_writes;
    return byte_count;
}

u32 userspace_input_read(
    u32 input_capability_handle,
    u32 output_buffer_capability_handle,
    u32 byte_capacity)
{
    u32 actual_count;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((byte_capacity == 0u) || (byte_capacity > USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_resolve_service_capability(
            current_process,
            input_capability_handle,
            SERVICE_ENDPOINT_CLASS_INPUT,
            "input") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    actual_count = input_read(buffer_copy_scratch, byte_capacity);
    if (actual_count == 0u)
    {
        return 0u;
    }

    if (userspace_copy_bytes_to_shared_buffer_capability(
            current_process,
            output_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            actual_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_input_reads;

    if (total_input_reads <= 4u)
    {
        klog_write_string("[userspace] input read ");
        klog_write_string(current_process->name);
        klog_write_string(" bytes ");
        klog_write_dec_u32(actual_count);
        klog_newline();
    }

    return actual_count;
}

static u32 userspace_complete_input_read(
    struct user_process *process,
    u32 output_buffer_capability_handle,
    u32 byte_capacity)
{
    u32 actual_count;

    if ((process == NULL)
        || (byte_capacity == 0u)
        || (byte_capacity > USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        return 0xFFFFFFFFu;
    }

    actual_count = input_read(buffer_copy_scratch, byte_capacity);
    if (actual_count == 0u)
    {
        return 0u;
    }

    if (userspace_copy_bytes_to_shared_buffer_capability(
            process,
            output_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            actual_count) != IPC_STATUS_OK)
    {
        return 0xFFFFFFFFu;
    }

    ++total_input_reads;

    if (total_input_reads <= 4u)
    {
        klog_write_string("[userspace] input read ");
        klog_write_string(process->name);
        klog_write_string(" bytes ");
        klog_write_dec_u32(actual_count);
        klog_newline();
    }

    return actual_count;
}

u32 userspace_fs_open(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 path_byte_count)
{
    struct user_capability *base_capability;
    u32 base_node_id = ramfs_root_node();
    u32 node_id = 0u;
    u32 rights;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((path_byte_count == 0u) || (path_byte_count >= USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    base_capability = userspace_find_capability(current_process, base_capability_handle);
    if ((base_capability != NULL)
        && (base_capability->object_type == USER_CAPABILITY_TYPE_RAMFS_NODE))
    {
        base_node_id = userspace_resolve_capability_ramfs_node(
            current_process,
            base_capability_handle,
            USER_CAPABILITY_RIGHT_NODE_LIST,
            "ramfs");
        if ((base_node_id == 0xFFFFFFFFu) || !ramfs_node_is_directory(base_node_id))
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }
    }
    else if (userspace_resolve_service_capability(
                 current_process,
                 base_capability_handle,
                 SERVICE_ENDPOINT_CLASS_RAMFS,
                 "ramfs") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            path_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            path_byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!ramfs_open(base_node_id, buffer_copy_scratch, path_byte_count, &node_id))
    {
        return (u32)IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    rights = ramfs_node_is_directory(node_id)
        ? (USER_CAPABILITY_RIGHT_NODE_LIST
            | USER_CAPABILITY_RIGHT_NODE_CREATE
            | USER_CAPABILITY_RIGHT_NODE_STAT
            | USER_CAPABILITY_RIGHT_NODE_RENAME
            | USER_CAPABILITY_RIGHT_NODE_DELETE
            | USER_CAPABILITY_RIGHT_DELEGATE)
        : (USER_CAPABILITY_RIGHT_NODE_READ
            | USER_CAPABILITY_RIGHT_NODE_WRITE
            | USER_CAPABILITY_RIGHT_NODE_STAT
            | USER_CAPABILITY_RIGHT_DELEGATE);

    ++total_fs_opens;
    return userspace_grant_capability(
        current_process,
        USER_CAPABILITY_TYPE_RAMFS_NODE,
        node_id,
        rights,
        0u,
        0u,
        0u,
        0u,
        0u,
        USER_CAPABILITY_LEASE_NONE);
}

u32 userspace_fs_create(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 path_byte_count,
    u32 node_type)
{
    struct user_capability *base_capability;
    u32 base_node_id = ramfs_root_node();
    u32 node_id = 0u;
    u32 rights;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((path_byte_count == 0u) || (path_byte_count >= USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    base_capability = userspace_find_capability(current_process, base_capability_handle);
    if ((base_capability != NULL)
        && (base_capability->object_type == USER_CAPABILITY_TYPE_RAMFS_NODE))
    {
        base_node_id = userspace_resolve_capability_ramfs_node(
            current_process,
            base_capability_handle,
            USER_CAPABILITY_RIGHT_NODE_CREATE,
            "ramfs");
        if ((base_node_id == 0xFFFFFFFFu) || !ramfs_node_is_directory(base_node_id))
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }
    }
    else if (userspace_resolve_service_capability(
                 current_process,
                 base_capability_handle,
                 SERVICE_ENDPOINT_CLASS_RAMFS,
                 "ramfs") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            path_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            path_byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!ramfs_create(base_node_id, buffer_copy_scratch, path_byte_count, node_type, &node_id))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    rights = ramfs_node_is_directory(node_id)
        ? (USER_CAPABILITY_RIGHT_NODE_LIST
            | USER_CAPABILITY_RIGHT_NODE_CREATE
            | USER_CAPABILITY_RIGHT_NODE_STAT
            | USER_CAPABILITY_RIGHT_NODE_RENAME
            | USER_CAPABILITY_RIGHT_NODE_DELETE
            | USER_CAPABILITY_RIGHT_DELEGATE)
        : (USER_CAPABILITY_RIGHT_NODE_READ
            | USER_CAPABILITY_RIGHT_NODE_WRITE
            | USER_CAPABILITY_RIGHT_NODE_STAT
            | USER_CAPABILITY_RIGHT_DELEGATE);

    ++total_fs_creates;
    return userspace_grant_capability(
        current_process,
        USER_CAPABILITY_TYPE_RAMFS_NODE,
        node_id,
        rights,
        0u,
        0u,
        0u,
        0u,
        0u,
        USER_CAPABILITY_LEASE_NONE);
}

u32 userspace_fs_list(
    u32 node_capability_handle,
    u32 output_buffer_capability_handle,
    u32 byte_capacity)
{
    u32 node_id;
    u32 byte_count;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((byte_capacity == 0u) || (byte_capacity > USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        byte_capacity = USER_BUFFER_COPY_SCRATCH_BYTES;
    }

    node_id = userspace_resolve_capability_ramfs_node(
        current_process,
        node_capability_handle,
        USER_CAPABILITY_RIGHT_NODE_LIST,
        "ramfs");
    if (node_id == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    byte_count = ramfs_list(node_id, buffer_copy_scratch, byte_capacity);
    if (byte_count == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_to_shared_buffer_capability(
            current_process,
            output_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_fs_lists;
    return byte_count;
}

u32 userspace_fs_read(
    u32 node_capability_handle,
    u32 output_buffer_capability_handle,
    u32 file_offset,
    u32 byte_count)
{
    u32 node_id;
    u32 actual_count;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((byte_count == 0u) || (byte_count > USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        byte_count = USER_BUFFER_COPY_SCRATCH_BYTES;
    }

    node_id = userspace_resolve_capability_ramfs_node(
        current_process,
        node_capability_handle,
        USER_CAPABILITY_RIGHT_NODE_READ,
        "ramfs");
    if (node_id == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    actual_count = ramfs_read(node_id, file_offset, buffer_copy_scratch, byte_count);
    if (actual_count == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_to_shared_buffer_capability(
            current_process,
            output_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            actual_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_fs_reads;
    return actual_count;
}

u32 userspace_fs_stat(
    u32 node_capability_handle,
    u32 output_buffer_capability_handle,
    u32 byte_capacity)
{
    struct ramfs_stat stat;
    u32 node_id;
    u32 actual_count;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((byte_capacity == 0u) || (byte_capacity > USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        byte_capacity = USER_BUFFER_COPY_SCRATCH_BYTES;
    }

    node_id = userspace_resolve_capability_ramfs_node(
        current_process,
        node_capability_handle,
        USER_CAPABILITY_RIGHT_NODE_STAT,
        "ramfs");
    if (node_id == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!ramfs_stat(node_id, &stat))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    actual_count = ramfs_format_stat(&stat, buffer_copy_scratch, byte_capacity);
    if (actual_count == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_to_shared_buffer_capability(
            current_process,
            output_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            actual_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_fs_stats;
    return actual_count;
}

u32 userspace_fs_rename(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 source_path_byte_count,
    u32 destination_path_byte_count)
{
    struct user_capability *base_capability;
    u32 base_node_id = ramfs_root_node();
    u32 destination_offset;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((source_path_byte_count == 0u)
        || (destination_path_byte_count == 0u)
        || (source_path_byte_count >= USER_BUFFER_COPY_SCRATCH_BYTES)
        || (destination_path_byte_count >= USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    destination_offset = source_path_byte_count + 1u;
    if ((destination_offset + destination_path_byte_count) > USER_BUFFER_COPY_SCRATCH_BYTES)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    base_capability = userspace_find_capability(current_process, base_capability_handle);
    if ((base_capability != NULL)
        && (base_capability->object_type == USER_CAPABILITY_TYPE_RAMFS_NODE))
    {
        base_node_id = userspace_resolve_capability_ramfs_node(
            current_process,
            base_capability_handle,
            USER_CAPABILITY_RIGHT_NODE_RENAME,
            "ramfs");
        if ((base_node_id == 0xFFFFFFFFu) || !ramfs_node_is_directory(base_node_id))
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }
    }
    else if (userspace_resolve_service_capability(
                 current_process,
                 base_capability_handle,
                 SERVICE_ENDPOINT_CLASS_RAMFS,
                 "ramfs") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            path_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            source_path_byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            path_buffer_capability_handle,
            destination_offset,
            buffer_copy_scratch + destination_offset,
            destination_path_byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!ramfs_rename(
            base_node_id,
            buffer_copy_scratch,
            source_path_byte_count,
            buffer_copy_scratch + destination_offset,
            destination_path_byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_fs_renames;
    return (u32)IPC_STATUS_OK;
}

u32 userspace_fs_move(
    u32 source_base_capability_handle,
    u32 destination_base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 packed_path_lengths)
{
    struct user_capability *source_base_capability;
    struct user_capability *destination_base_capability;
    u32 source_base_node_id = ramfs_root_node();
    u32 destination_base_node_id = ramfs_root_node();
    u32 source_path_byte_count = packed_path_lengths & 0xFFFFu;
    u32 destination_path_byte_count = (packed_path_lengths >> 16) & 0xFFFFu;
    u32 destination_offset;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((source_path_byte_count == 0u)
        || (destination_path_byte_count == 0u)
        || (source_path_byte_count >= USER_BUFFER_COPY_SCRATCH_BYTES)
        || (destination_path_byte_count >= USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    destination_offset = source_path_byte_count + 1u;
    if ((destination_offset + destination_path_byte_count) > USER_BUFFER_COPY_SCRATCH_BYTES)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    source_base_capability = userspace_find_capability(current_process, source_base_capability_handle);
    if ((source_base_capability != NULL)
        && (source_base_capability->object_type == USER_CAPABILITY_TYPE_RAMFS_NODE))
    {
        source_base_node_id = userspace_resolve_capability_ramfs_node(
            current_process,
            source_base_capability_handle,
            USER_CAPABILITY_RIGHT_NODE_RENAME,
            "ramfs");
        if ((source_base_node_id == 0xFFFFFFFFu) || !ramfs_node_is_directory(source_base_node_id))
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }
    }
    else if (userspace_resolve_service_capability(
                 current_process,
                 source_base_capability_handle,
                 SERVICE_ENDPOINT_CLASS_RAMFS,
                 "ramfs") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    destination_base_capability = userspace_find_capability(current_process, destination_base_capability_handle);
    if ((destination_base_capability != NULL)
        && (destination_base_capability->object_type == USER_CAPABILITY_TYPE_RAMFS_NODE))
    {
        destination_base_node_id = userspace_resolve_capability_ramfs_node(
            current_process,
            destination_base_capability_handle,
            USER_CAPABILITY_RIGHT_NODE_RENAME,
            "ramfs");
        if ((destination_base_node_id == 0xFFFFFFFFu) || !ramfs_node_is_directory(destination_base_node_id))
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }
    }
    else if (userspace_resolve_service_capability(
                 current_process,
                 destination_base_capability_handle,
                 SERVICE_ENDPOINT_CLASS_RAMFS,
                 "ramfs") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            path_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            source_path_byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            path_buffer_capability_handle,
            destination_offset,
            buffer_copy_scratch + destination_offset,
            destination_path_byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!ramfs_move(
            source_base_node_id,
            buffer_copy_scratch,
            source_path_byte_count,
            destination_base_node_id,
            buffer_copy_scratch + destination_offset,
            destination_path_byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_fs_moves;
    return (u32)IPC_STATUS_OK;
}

u32 userspace_fs_delete(
    u32 base_capability_handle,
    u32 path_buffer_capability_handle,
    u32 path_byte_count)
{
    struct user_capability *base_capability;
    u32 base_node_id = ramfs_root_node();

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((path_byte_count == 0u) || (path_byte_count >= USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    base_capability = userspace_find_capability(current_process, base_capability_handle);
    if ((base_capability != NULL)
        && (base_capability->object_type == USER_CAPABILITY_TYPE_RAMFS_NODE))
    {
        base_node_id = userspace_resolve_capability_ramfs_node(
            current_process,
            base_capability_handle,
            USER_CAPABILITY_RIGHT_NODE_DELETE,
            "ramfs");
        if ((base_node_id == 0xFFFFFFFFu) || !ramfs_node_is_directory(base_node_id))
        {
            return (u32)IPC_STATUS_ACCESS_DENIED;
        }
    }
    else if (userspace_resolve_service_capability(
                 current_process,
                 base_capability_handle,
                 SERVICE_ENDPOINT_CLASS_RAMFS,
                 "ramfs") == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            path_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            path_byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (!ramfs_delete(base_node_id, buffer_copy_scratch, path_byte_count))
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_fs_deletes;
    return (u32)IPC_STATUS_OK;
}

u32 userspace_fs_write(
    u32 node_capability_handle,
    u32 input_buffer_capability_handle,
    u32 file_offset,
    u32 byte_count)
{
    u32 node_id;
    u32 actual_count;

    if ((current_process == NULL) || !userspace_active)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if ((byte_count == 0u) || (byte_count > USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        byte_count = USER_BUFFER_COPY_SCRATCH_BYTES;
    }

    node_id = userspace_resolve_capability_ramfs_node(
        current_process,
        node_capability_handle,
        USER_CAPABILITY_RIGHT_NODE_WRITE,
        "ramfs");
    if (node_id == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    if (userspace_copy_bytes_from_shared_buffer_capability(
            current_process,
            input_buffer_capability_handle,
            0u,
            buffer_copy_scratch,
            byte_count) != IPC_STATUS_OK)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    actual_count = ramfs_write(node_id, file_offset, buffer_copy_scratch, byte_count);
    if (actual_count == 0xFFFFFFFFu)
    {
        return (u32)IPC_STATUS_ACCESS_DENIED;
    }

    ++total_fs_writes;
    return actual_count;
}

s32 userspace_send_message(
    u32 endpoint_capability_handle,
    u32 type,
    u32 payload_buffer_address,
    u32 payload_word_count)
{
    struct user_process *destination;
    struct user_endpoint *destination_endpoint;
    struct user_endpoint *source_peer_endpoint;
    u32 endpoint_id;
    u32 source_endpoint;
    u32 payload_words[USER_IPC_MAX_PAYLOAD_WORDS];
    s32 status;

    if ((current_process == NULL) || !userspace_active)
    {
        return IPC_STATUS_ACCESS_DENIED;
    }

    endpoint_id = userspace_resolve_capability_endpoint(
        current_process,
        endpoint_capability_handle,
        USER_CAPABILITY_RIGHT_SEND,
        "stale cap");
    if (endpoint_id == 0xFFFFFFFFu)
    {
        return IPC_STATUS_ACCESS_DENIED;
    }

    destination_endpoint = userspace_find_endpoint(endpoint_id, &destination);
    if ((destination_endpoint == NULL) || !destination_endpoint->registered)
    {
        return IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    if (!userspace_endpoint_allows_sender(destination_endpoint, current_process->id))
    {
        ++current_process->ipc_denied_count;
        ++total_ipc_denied;

        if (total_ipc_denied <= 4u)
        {
            klog_write_string("[userspace] denied direct ");
            klog_write_string(current_process->name);
            klog_write_string(" -> ");
            klog_write_string(destination_endpoint->name);
            klog_newline();
        }

        return IPC_STATUS_ACCESS_DENIED;
    }

    source_peer_endpoint = userspace_endpoint_for_role(current_process, USER_ENDPOINT_ROLE_PEER);
    if ((source_peer_endpoint == NULL) || !source_peer_endpoint->registered)
    {
        return IPC_STATUS_ACCESS_DENIED;
    }

    source_endpoint = source_peer_endpoint->id;

    if (!userspace_copy_words_from_current(
            payload_buffer_address,
            payload_word_count,
            payload_words))
    {
        return IPC_STATUS_ACCESS_DENIED;
    }

    status = userspace_deliver_message(
        endpoint_id,
        source_endpoint,
        type,
        payload_words,
        payload_word_count);

    if (status == IPC_STATUS_OK)
    {
        ++current_process->ipc_send_count;
        ++total_ipc_sends;

        if (total_ipc_sends <= 4u)
        {
            klog_write_string("[userspace] direct ");
            klog_write_string(current_process->name);
            klog_write_string(" -> ");
            klog_write_string(userspace_endpoint_name_from_id(endpoint_id));
            klog_newline();
        }
    }

    return status;
}

static int userspace_capability_depends_on_process(
    const struct user_capability *capability,
    u32 process_id,
    u32 process_generation)
{
    if ((capability == NULL) || !capability->active)
    {
        return 0;
    }

    if ((capability->object_owner_process_id == process_id)
        && (capability->object_owner_generation == process_generation))
    {
        return 1;
    }

    if ((capability->issued_by_process_id == process_id)
        && (capability->issued_by_process_generation == process_generation))
    {
        return 1;
    }

    return 0;
}

static void userspace_invalidate_process_capabilities(
    u32 exiting_process_id,
    u32 exiting_generation,
    const char *exiting_name)
{
    u32 index;

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        u32 slot;
        struct user_process *process = &processes[index];

        if (process->state == USER_PROCESS_UNUSED)
        {
            continue;
        }

        for (slot = 0; slot < USER_CAPABILITY_LIMIT; ++slot)
        {
            struct user_capability *capability = &process->capabilities[slot];

            if (!userspace_capability_depends_on_process(
                    capability,
                    exiting_process_id,
                    exiting_generation))
            {
                continue;
            }

            capability->active = 0u;
            capability->expired = 0u;
            capability->stale = 1u;

            if (total_process_exits <= 4u)
            {
                klog_write_string("[userspace] retire cap ");
                klog_write_string(process->name);
                klog_write_string(" source-exit ");
                klog_write_string(exiting_name);
                klog_write_string(" handle ");
                klog_write_dec_u32(capability->handle);
                klog_newline();
            }
        }
    }
}

static void userspace_wake_process_exit_waiters(u32 exiting_process_id)
{
    u32 index;
    u32 now = pit_get_ticks();

    for (index = 0; index < USER_PROCESS_LIMIT; ++index)
    {
        struct user_process *process = &processes[index];

        if ((process->state != USER_PROCESS_WAITING_PROCESS)
            || (process->wait_process_id != exiting_process_id))
        {
            continue;
        }

        process->wait_process_id = 0u;
        process->frame.eax = 0u;
        userspace_mark_runnable_with_source(process, now, USER_WAKE_SOURCE_NONE);
    }
}

static void userspace_release_process(struct user_process *process, u32 exit_code)
{
    u32 slot;
    u32 exiting_process_id;
    u32 exiting_generation;
    const char *exiting_name;
    u32 code_pages[USER_CODE_PAGE_LIMIT];
    u32 code_page_count;
    u32 stack_page;
    u32 kernel_stack_page;
    u32 address_space;

    if (process == NULL)
    {
        return;
    }

    exiting_process_id = process->id;
    exiting_generation = process->generation;
    exiting_name = process->name;
    code_page_count = process->code_page_count;
    for (slot = 0u; slot < USER_CODE_PAGE_LIMIT; ++slot)
    {
        code_pages[slot] = process->code_pages[slot];
    }
    stack_page = process->stack_page;
    kernel_stack_page = process->kernel_stack_page;
    address_space = process->address_space;

    ++total_process_exits;
    userspace_note_policy_wait_cancel(process);

    for (slot = 0; slot < USER_ENDPOINTS_PER_PROCESS; ++slot)
    {
        if (process->endpoints[slot].registered && (total_registered_endpoints != 0u))
        {
            --total_registered_endpoints;
        }
    }

    userspace_invalidate_process_capabilities(
        exiting_process_id,
        exiting_generation,
        exiting_name);
    userspace_wake_process_exit_waiters(exiting_process_id);

    klog_write_string("[userspace] exit ");
    klog_write_string(exiting_name);
    klog_write_string(" code ");
    klog_write_dec_u32(exit_code);
    klog_newline();

    memory_zero(process, sizeof(*process));

    if (address_space != 0u)
    {
        paging_destroy_user_space(address_space);
    }

    for (slot = 0u; slot < code_page_count; ++slot)
    {
        if (code_pages[slot] != 0xFFFFFFFFu)
        {
            memory_release_frame(code_pages[slot]);
        }
    }

    if (stack_page != 0xFFFFFFFFu)
    {
        memory_release_frame(stack_page);
    }

    if (kernel_stack_page != 0xFFFFFFFFu)
    {
        memory_release_frame(kernel_stack_page);
    }
}

struct interrupt_frame *userspace_handle_exit(struct interrupt_frame *frame, u32 exit_code)
{
    struct user_process *exiting_process;
    struct user_process *next_process;

    if ((current_process == NULL) || !userspace_active)
    {
        return frame;
    }

    exiting_process = current_process;
    exiting_process->frame = *frame;
    exiting_process->runtime_ticks += current_slice_ticks;
    userspace_account_runtime(exiting_process, current_slice_ticks);
    exiting_process->virtual_runtime += userspace_runtime_cost(
        exiting_process,
        current_slice_ticks);

    current_process = NULL;
    current_slice_ticks = 0u;
    userspace_set_kernel_stack(kernel_idle_stack_top);
    paging_switch_address_space(paging_get_page_directory_address());

    userspace_release_process(exiting_process, exit_code);

    next_process = userspace_next_process();
    if (next_process == NULL)
    {
        return userspace_enter_idle();
    }

    userspace_switch_to_process(next_process);
    return &current_process->frame;
}

struct interrupt_frame *userspace_handle_yield(struct interrupt_frame *frame)
{
    return userspace_switch_frame(frame, 0);
}

struct interrupt_frame *userspace_handle_sleep(struct interrupt_frame *frame, u32 ticks)
{
    if ((current_process == NULL) || !userspace_active)
    {
        return frame;
    }

    if (ticks == 0u)
    {
        ticks = 1u;
    }

    current_process->frame = *frame;
    current_process->state = USER_PROCESS_SLEEPING;
    current_process->wake_tick = pit_get_ticks() + ticks;
    ++current_process->sleep_count;
    ++total_sleeps;

    if (total_sleeps <= 4u)
    {
        klog_write_string("[userspace] sleep ");
        klog_write_string(current_process->name);
        klog_write_string(" ticks ");
        klog_write_dec_u32(ticks);
        klog_newline();
    }

    return userspace_switch_frame(frame, 0);
}

struct interrupt_frame *userspace_handle_wait_message(struct interrupt_frame *frame)
{
    struct user_ipc_message message;
    u32 buffer_address;
    u32 payload_word_capacity;

    if ((current_process == NULL) || !userspace_active)
    {
        userspace_prepare_wait_error(frame, 0u);
        return frame;
    }

    buffer_address = frame->ebx;
    payload_word_capacity = frame->ecx;

    if (!userspace_word_buffer_is_valid(buffer_address, payload_word_capacity))
    {
        userspace_prepare_wait_error(frame, 0u);
        return frame;
    }

    if (userspace_mailbox_peek(current_process, &message))
    {
        if (message.payload_word_count > payload_word_capacity)
        {
            userspace_prepare_wait_error(frame, message.payload_word_count);
            return frame;
        }

        if (!userspace_copy_words_to_process(
                current_process,
                buffer_address,
                payload_word_capacity,
                message.payload,
                message.payload_word_count))
        {
            userspace_prepare_wait_error(frame, message.payload_word_count);
            return frame;
        }

        userspace_note_policy_wait_resolved(
            current_process,
            message.source_endpoint,
            message.type);
        userspace_mailbox_consume(current_process);
        userspace_prepare_wait_result(frame, &message);
        ++current_process->ipc_message_count;
        ++total_ipc_messages;
        return frame;
    }

    current_process->frame = *frame;
    current_process->state = USER_PROCESS_WAITING_IPC;
    current_process->wait_buffer_address = buffer_address;
    current_process->wait_buffer_word_capacity = payload_word_capacity;
    ++current_process->ipc_wait_count;
    ++total_ipc_waits;

    if (total_ipc_waits <= 4u)
    {
        klog_write_string("[userspace] wait ");
        klog_write_string(current_process->name);
        klog_write_line(" mailbox");
    }

    return userspace_switch_frame(frame, 0);
}

struct interrupt_frame *userspace_handle_input_read(struct interrupt_frame *frame)
{
    u32 byte_count;

    if ((current_process == NULL) || !userspace_active)
    {
        frame->eax = (u32)IPC_STATUS_ACCESS_DENIED;
        return frame;
    }

    if ((frame->edx == 0u) || (frame->edx > USER_BUFFER_COPY_SCRATCH_BYTES))
    {
        frame->eax = (u32)IPC_STATUS_ACCESS_DENIED;
        return frame;
    }

    if (userspace_resolve_service_capability(
            current_process,
            frame->ebx,
            SERVICE_ENDPOINT_CLASS_INPUT,
            "input") == 0xFFFFFFFFu)
    {
        frame->eax = (u32)IPC_STATUS_ACCESS_DENIED;
        return frame;
    }

    byte_count = userspace_complete_input_read(current_process, frame->ecx, frame->edx);
    if (byte_count == 0xFFFFFFFFu)
    {
        frame->eax = (u32)IPC_STATUS_ACCESS_DENIED;
        return frame;
    }

    if (byte_count != 0u)
    {
        frame->eax = byte_count;
        return frame;
    }

    current_process->frame = *frame;
    current_process->state = USER_PROCESS_WAITING_INPUT;
    current_process->input_wait_buffer_capability_handle = frame->ecx;
    current_process->input_wait_byte_capacity = frame->edx;
    return userspace_switch_frame(frame, 0);
}

struct interrupt_frame *userspace_handle_wait_process(struct interrupt_frame *frame, u32 process_id)
{
    if ((current_process == NULL) || !userspace_active)
    {
        frame->eax = (u32)IPC_STATUS_ACCESS_DENIED;
        return frame;
    }

    if ((process_id == 0u) || (process_id == current_process->id))
    {
        frame->eax = (u32)IPC_STATUS_ACCESS_DENIED;
        return frame;
    }

    if (userspace_process_for_id(process_id) == NULL)
    {
        frame->eax = 0u;
        return frame;
    }

    current_process->frame = *frame;
    current_process->state = USER_PROCESS_WAITING_PROCESS;
    current_process->wait_process_id = process_id;
    return userspace_switch_frame(frame, 0);
}

void userspace_note_input_ready(void)
{
    u32 index;
    u32 now;

    if (!userspace_active || (input_pending_byte_count() == 0u))
    {
        return;
    }

    now = pit_get_ticks();

    for (index = 0u; index < USER_PROCESS_LIMIT; ++index)
    {
        struct user_process *process = &processes[index];
        u32 byte_count;

        if (process->state != USER_PROCESS_WAITING_INPUT)
        {
            continue;
        }

        byte_count = userspace_complete_input_read(
            process,
            process->input_wait_buffer_capability_handle,
            process->input_wait_byte_capacity);

        if (byte_count == 0u)
        {
            return;
        }

        if (byte_count == 0xFFFFFFFFu)
        {
            process->frame.eax = (u32)IPC_STATUS_ACCESS_DENIED;
        }
        else
        {
            process->frame.eax = byte_count;
        }

        process->input_wait_buffer_capability_handle = 0u;
        process->input_wait_byte_capacity = 0u;
        userspace_mark_runnable_with_source(process, now, USER_WAKE_SOURCE_INPUT);

        if (byte_count != 0xFFFFFFFFu)
        {
            klog_write_string("[userspace] wake ");
            klog_write_string(process->name);
            klog_write_line(" input");
        }

        if (input_pending_byte_count() == 0u)
        {
            return;
        }
    }
}

struct interrupt_frame *userspace_handle_timer_tick(struct interrupt_frame *frame)
{
    userspace_wake_sleepers();

    if (!userspace_active)
    {
        return frame;
    }

    if (current_process == NULL)
    {
        struct user_process *next_process = userspace_next_process();

        if (next_process != NULL)
        {
            userspace_switch_to_process(next_process);
            return &current_process->frame;
        }

        return frame;
    }

    if ((frame->cs & 0x3u) != 0x3u)
    {
        return frame;
    }

    ++current_slice_ticks;
    if (current_slice_ticks < USER_BASE_TIME_SLICE_TICKS)
    {
        return frame;
    }

    return userspace_switch_frame(frame, 1);
}

u32 userspace_syscall_count(void)
{
    return user_syscalls;
}

u32 userspace_process_count(void)
{
    return userspace_active_process_count();
}

u32 userspace_total_sleep_count(void)
{
    return total_sleeps;
}

u32 userspace_total_yield_count(void)
{
    return total_yields;
}

u32 userspace_total_preemption_count(void)
{
    return total_preemptions;
}

u32 userspace_registered_endpoint_count(void)
{
    return total_registered_endpoints;
}

u32 userspace_total_ipc_send_count(void)
{
    return total_ipc_sends;
}

u32 userspace_total_ipc_denied_count(void)
{
    return total_ipc_denied;
}

u32 userspace_total_ipc_wait_count(void)
{
    return total_ipc_waits;
}

u32 userspace_total_ipc_message_count(void)
{
    return total_ipc_messages;
}

u32 userspace_total_capability_grant_count(void)
{
    return total_capability_grants;
}

u32 userspace_total_capability_revoke_count(void)
{
    return total_capability_revocations;
}

u32 userspace_total_capability_delegation_count(void)
{
    return total_capability_delegations;
}

u32 userspace_total_capability_expiration_count(void)
{
    return total_capability_expirations;
}

u32 userspace_total_policy_denial_count(void)
{
    return total_policy_denials;
}

u32 userspace_total_dispatch_count(void)
{
    return total_user_dispatches;
}

u32 userspace_total_latency_pick_count(void)
{
    return total_latency_picks;
}

u32 userspace_total_deadline_pick_count(void)
{
    return total_deadline_picks;
}

u32 userspace_total_io_wake_count(void)
{
    return total_io_wakes;
}

u32 userspace_total_budget_throttle_count(void)
{
    return total_budget_throttles;
}

u32 userspace_total_capability_admission_denial_count(void)
{
    return total_capability_admission_denials;
}

u32 userspace_total_capability_reuse_count(void)
{
    return total_capability_reuses;
}

u32 userspace_total_capability_compaction_count(void)
{
    return total_capability_compactions;
}

u32 userspace_total_buffer_registration_count(void)
{
    return total_shared_buffer_registrations;
}

u32 userspace_total_buffer_copy_count(void)
{
    return total_shared_buffer_copies;
}

u32 userspace_total_process_exit_count(void)
{
    return total_process_exits;
}

u32 userspace_total_console_write_count(void)
{
    return total_console_writes;
}

u32 userspace_total_input_read_count(void)
{
    return total_input_reads;
}

u32 userspace_total_fs_open_count(void)
{
    return total_fs_opens;
}

u32 userspace_total_fs_create_count(void)
{
    return total_fs_creates;
}

u32 userspace_total_fs_list_count(void)
{
    return total_fs_lists;
}

u32 userspace_total_fs_read_count(void)
{
    return total_fs_reads;
}

u32 userspace_total_fs_stat_count(void)
{
    return total_fs_stats;
}

u32 userspace_total_fs_rename_count(void)
{
    return total_fs_renames;
}

u32 userspace_total_fs_move_count(void)
{
    return total_fs_moves;
}

u32 userspace_total_fs_delete_count(void)
{
    return total_fs_deletes;
}

u32 userspace_total_fs_write_count(void)
{
    return total_fs_writes;
}

u32 userspace_total_package_load_count(void)
{
    return total_package_manifest_loads;
}

u32 userspace_total_package_rejection_count(void)
{
    return total_package_manifest_rejections;
}

u32 userspace_total_signer_verification_count(void)
{
    return total_signer_verifications;
}

u32 userspace_total_signer_denial_count(void)
{
    return total_signer_denials;
}

u32 userspace_total_manifest_verification_count(void)
{
    return total_manifest_verifications;
}

u32 userspace_total_manifest_denial_count(void)
{
    return total_manifest_denials;
}

u32 userspace_interactive_policy_waiter_count(void)
{
    return interactive_policy_waiters;
}

int userspace_is_active(void)
{
    return userspace_active;
}

int userspace_is_endpoint(u32 endpoint_id)
{
    return userspace_process_for_endpoint(endpoint_id) != NULL;
}

s32 userspace_deliver_message(
    u32 endpoint_id,
    u32 source_endpoint,
    u32 type,
    const u32 *payload_words,
    u32 payload_word_count)
{
    struct user_process *process = userspace_process_for_endpoint(endpoint_id);
    struct user_ipc_message message;

    if (process == NULL)
    {
        return IPC_STATUS_UNKNOWN_ENDPOINT;
    }

    if (payload_word_count > USER_IPC_MAX_PAYLOAD_WORDS)
    {
        return IPC_STATUS_QUEUE_FULL;
    }

    memory_zero(&message, sizeof(message));
    message.source_endpoint = source_endpoint;
    message.destination_endpoint = endpoint_id;
    message.type = type;
    message.payload_word_count = payload_word_count;

    if ((payload_word_count > 0u) && (payload_words == NULL))
    {
        return IPC_STATUS_ACCESS_DENIED;
    }

    if (payload_word_count > 0u)
    {
        memory_copy(message.payload, payload_words, payload_word_count * sizeof(u32));
    }

    if (process->state == USER_PROCESS_WAITING_IPC)
    {
        if (!userspace_copy_words_to_process(
                process,
                process->wait_buffer_address,
                process->wait_buffer_word_capacity,
                message.payload,
                message.payload_word_count))
        {
            s32 queue_status = userspace_mailbox_push(process, &message);

            userspace_prepare_wait_error(&process->frame, message.payload_word_count);
            if (queue_status == IPC_STATUS_OK)
            {
                userspace_note_policy_wait_resolved(process, source_endpoint, type);
            }
            userspace_mark_runnable_with_source(process, pit_get_ticks(), USER_WAKE_SOURCE_IPC);
            process->wait_buffer_address = 0u;
            process->wait_buffer_word_capacity = 0u;
            return queue_status;
        }

        userspace_note_policy_wait_resolved(process, source_endpoint, type);
        userspace_prepare_wait_result(&process->frame, &message);
        userspace_mark_runnable_with_source(process, pit_get_ticks(), USER_WAKE_SOURCE_IPC);
        process->wait_buffer_address = 0u;
        process->wait_buffer_word_capacity = 0u;
        ++process->ipc_message_count;
        ++total_ipc_messages;

        if (total_ipc_messages <= 4u)
        {
            klog_write_string("[userspace] wake ");
            klog_write_string(process->name);
            klog_write_string(" from endpoint ");
            klog_write_dec_u32(source_endpoint);
            klog_newline();
        }

        return IPC_STATUS_OK;
    }

    {
        s32 queue_status = userspace_mailbox_push(process, &message);

        if (queue_status == IPC_STATUS_OK)
        {
            userspace_note_policy_wait_resolved(process, source_endpoint, type);
        }

        return queue_status;
    }
}
