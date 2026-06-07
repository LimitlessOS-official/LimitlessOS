#include "persona_audit_x64.h"

#include "linux_abi_x64.h"
#include "macos_abi_x64.h"
#include "persona_x64.h"
#include "pit.h"
#include "process_x64.h"
#include "vma_x64.h"
#include "windows_abi_x64.h"

/*
 * D.5 adds the first lightweight persona audit ring. O.2 extends each
 * syscall record with a deterministic syscall-name token and a translated
 * LimitlessOS operation class. It integrates with process_x64.h only through
 * the audit attach/detach/accessor APIs, with persona_x64.h to stamp each
 * record with the active persona type, with the persona ABI headers for
 * documented syscall numbers, and with pit.h for a monotonic checkpoint token.
 * The scaffold checkpoints prove that valid records are appended in
 * chronological order, denial/unimplemented events carry their real ABI-facing
 * result codes, translated syscall records carry nonzero name tokens plus
 * operation classes, and release clears the PCB audit slot without granting
 * ambient authority. O.6 adds a compact structured crash report beside the
 * audit ring. It integrates with vma_x64.h for the VMA dump and interrupts.c
 * for unhandled ring-3 persona fault handoff; the scaffold proves a Linux
 * persona NULL page fault records SIGSEGV, RIP/RSP/error, and real mapped VMA
 * state without inventing a successful recovery.
 */

static persona_audit64_context_t g_persona_audit64_contexts[PERSONA_AUDIT64_MAX_CONTEXTS];
static u32 g_persona_audit64_context_used[PERSONA_AUDIT64_MAX_CONTEXTS];
static u32 g_persona_audit64_initialized = 0u;
static u32 g_persona_audit64_sequence = 1u;

static void persona_audit64_clear_record(persona_audit64_record_t *record)
{
    if (record == 0)
    {
        return;
    }

    record->timestamp = 0ull;
    record->pid = PROCESS64_INVALID_PID;
    record->persona_type = (u8)PERSONA64_TYPE_COUNT;
    record->event_type = 0u;
    record->event_code = 0u;
    record->result = 0u;
    record->syscall_name_token = 0u;
    record->translated_operation = PERSONA_AUDIT64_OP_NONE;
    record->rip = 0ull;
}

static void persona_audit64_clear_crash_report(persona_audit64_crash_report_t *report)
{
    if (report == 0)
    {
        return;
    }

    report->timestamp = 0ull;
    report->pid = PROCESS64_INVALID_PID;
    report->persona_type = (u8)PERSONA64_TYPE_COUNT;
    report->reserved0 = 0u;
    report->vector = 0u;
    report->abi_code = 0u;
    report->result = 0u;
    report->rip = 0ull;
    report->rsp = 0ull;
    report->error_code = 0ull;
    report->fault_address = 0ull;
    report->vma_region_count = 0u;
    report->vma_dump_count = 0u;
    report->vma_mapped_bytes = 0ull;
    report->vma_first_base = 0ull;
    report->vma_first_end = 0ull;
    report->vma_first_prot = 0u;
    report->fault_region_present = 0u;
    report->fault_region_base = 0ull;
    report->fault_region_end = 0ull;
    report->fault_region_prot = 0u;
    report->reserved1 = 0u;
}

static void persona_audit64_clear_context(persona_audit64_context_t *context)
{
    u32 index;

    if (context == 0)
    {
        return;
    }

    context->pid = PROCESS64_INVALID_PID;
    context->write_index = 0u;
    context->count = 0u;
    context->dropped_count = 0u;
    context->crash_count = 0u;
    context->reserved = 0u;
    persona_audit64_clear_crash_report(&context->last_crash);

    for (index = 0u; index < PERSONA_AUDIT64_RING_CAPACITY; ++index)
    {
        persona_audit64_clear_record(&context->records[index]);
    }
}

static persona_audit64_context_t *persona_audit64_acquire_context(u32 pid)
{
    u32 index;

    for (index = 0u; index < PERSONA_AUDIT64_MAX_CONTEXTS; ++index)
    {
        if (g_persona_audit64_context_used[index] == 0u)
        {
            g_persona_audit64_context_used[index] = 1u;
            persona_audit64_clear_context(&g_persona_audit64_contexts[index]);
            g_persona_audit64_contexts[index].pid = pid;
            return &g_persona_audit64_contexts[index];
        }
    }

    return 0;
}

static void persona_audit64_release_context(persona_audit64_context_t *context)
{
    u32 index;

    if (context == 0)
    {
        return;
    }

    for (index = 0u; index < PERSONA_AUDIT64_MAX_CONTEXTS; ++index)
    {
        if (&g_persona_audit64_contexts[index] == context)
        {
            persona_audit64_clear_context(context);
            g_persona_audit64_context_used[index] = 0u;
            return;
        }
    }
}

static u64 persona_audit64_timestamp(void)
{
    u32 ticks = pit_get_ticks();
    u32 sequence = g_persona_audit64_sequence++;

    if (g_persona_audit64_sequence == 0u)
    {
        g_persona_audit64_sequence = 1u;
    }

    return ((u64)ticks << 32) | (u64)sequence;
}

static u32 persona_audit64_hash_text(const char *text)
{
    u32 hash = 2166136261u;

    if (text == 0)
    {
        return 0u;
    }

    while (*text != '\0')
    {
        hash ^= (u32)(u8)(*text);
        hash *= 16777619u;
        ++text;
    }

    return (hash != 0u) ? hash : 1u;
}

static const char *persona_audit64_linux_syscall_name(u32 syscall_number)
{
    switch (syscall_number)
    {
    case LINUX_ABI64_SYSCALL_READ: return "linux.read";
    case LINUX_ABI64_SYSCALL_WRITE: return "linux.write";
    case LINUX_ABI64_SYSCALL_OPEN: return "linux.open";
    case LINUX_ABI64_SYSCALL_CLOSE: return "linux.close";
    case LINUX_ABI64_SYSCALL_STAT: return "linux.stat";
    case LINUX_ABI64_SYSCALL_FSTAT: return "linux.fstat";
    case LINUX_ABI64_SYSCALL_POLL: return "linux.poll";
    case LINUX_ABI64_SYSCALL_LSEEK: return "linux.lseek";
    case LINUX_ABI64_SYSCALL_MMAP: return "linux.mmap";
    case LINUX_ABI64_SYSCALL_MPROTECT: return "linux.mprotect";
    case LINUX_ABI64_SYSCALL_MUNMAP: return "linux.munmap";
    case LINUX_ABI64_SYSCALL_BRK: return "linux.brk";
    case LINUX_ABI64_SYSCALL_RT_SIGACTION: return "linux.rt_sigaction";
    case LINUX_ABI64_SYSCALL_RT_SIGPROCMASK: return "linux.rt_sigprocmask";
    case LINUX_ABI64_SYSCALL_RT_SIGRETURN: return "linux.rt_sigreturn";
    case LINUX_ABI64_SYSCALL_PREAD64: return "linux.pread64";
    case LINUX_ABI64_SYSCALL_PWRITE64: return "linux.pwrite64";
    case LINUX_ABI64_SYSCALL_READV: return "linux.readv";
    case LINUX_ABI64_SYSCALL_WRITEV: return "linux.writev";
    case LINUX_ABI64_SYSCALL_DUP: return "linux.dup";
    case LINUX_ABI64_SYSCALL_DUP2: return "linux.dup2";
    case LINUX_ABI64_SYSCALL_NANOSLEEP: return "linux.nanosleep";
    case LINUX_ABI64_SYSCALL_GETPID: return "linux.getpid";
    case LINUX_ABI64_SYSCALL_CLONE: return "linux.clone";
    case LINUX_ABI64_SYSCALL_FORK: return "linux.fork";
    case LINUX_ABI64_SYSCALL_EXECVE: return "linux.execve";
    case LINUX_ABI64_SYSCALL_EXIT: return "linux.exit";
    case LINUX_ABI64_SYSCALL_WAIT4: return "linux.wait4";
    case LINUX_ABI64_SYSCALL_KILL: return "linux.kill";
    case LINUX_ABI64_SYSCALL_FCNTL: return "linux.fcntl";
    case LINUX_ABI64_SYSCALL_GETCWD: return "linux.getcwd";
    case LINUX_ABI64_SYSCALL_CHDIR: return "linux.chdir";
    case LINUX_ABI64_SYSCALL_FCHDIR: return "linux.fchdir";
    case LINUX_ABI64_SYSCALL_GETRLIMIT: return "linux.getrlimit";
    case LINUX_ABI64_SYSCALL_PRCTL: return "linux.prctl";
    case LINUX_ABI64_SYSCALL_ARCH_PRCTL: return "linux.arch_prctl";
    case LINUX_ABI64_SYSCALL_SETRLIMIT: return "linux.setrlimit";
    case LINUX_ABI64_SYSCALL_GETTID: return "linux.gettid";
    case LINUX_ABI64_SYSCALL_TKILL: return "linux.tkill";
    case LINUX_ABI64_SYSCALL_FUTEX: return "linux.futex";
    case LINUX_ABI64_SYSCALL_GETDENTS64: return "linux.getdents64";
    case LINUX_ABI64_SYSCALL_SET_TID_ADDRESS: return "linux.set_tid_address";
    case LINUX_ABI64_SYSCALL_CLOCK_GETTIME: return "linux.clock_gettime";
    case LINUX_ABI64_SYSCALL_EXIT_GROUP: return "linux.exit_group";
    case LINUX_ABI64_SYSCALL_OPENAT: return "linux.openat";
    case LINUX_ABI64_SYSCALL_NEWFSTATAT: return "linux.newfstatat";
    case LINUX_ABI64_SYSCALL_PPOLL: return "linux.ppoll";
    case LINUX_ABI64_SYSCALL_DUP3: return "linux.dup3";
    case LINUX_ABI64_SYSCALL_PIPE2: return "linux.pipe2";
    case LINUX_ABI64_SYSCALL_GETRANDOM: return "linux.getrandom";
    case LINUX_ABI64_SYSCALL_EXECVEAT: return "linux.execveat";
    default: return "linux.unknown";
    }
}

static const char *persona_audit64_windows_syscall_name(u32 syscall_number)
{
    switch (syscall_number)
    {
    case WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT: return "windows.NtWaitForSingleObject";
    case WINDOWS_ABI64_SYSCALL_NTREADFILE: return "windows.NtReadFile";
    case WINDOWS_ABI64_SYSCALL_NTWRITEFILE: return "windows.NtWriteFile";
    case WINDOWS_ABI64_SYSCALL_NTOPENKEY: return "windows.NtOpenKey";
    case WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY: return "windows.NtQueryValueKey";
    case WINDOWS_ABI64_SYSCALL_NTALLOCATEVIRTUALMEMORY: return "windows.NtAllocateVirtualMemory";
    case WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONPROCESS: return "windows.NtQueryInformationProcess";
    case WINDOWS_ABI64_SYSCALL_NTCREATEKEY: return "windows.NtCreateKey";
    case WINDOWS_ABI64_SYSCALL_NTFREEVIRTUALMEMORY: return "windows.NtFreeVirtualMemory";
    case WINDOWS_ABI64_SYSCALL_NTQUERYSYSTEMINFORMATION: return "windows.NtQuerySystemInformation";
    case WINDOWS_ABI64_SYSCALL_NTPROTECTVIRTUALMEMORY: return "windows.NtProtectVirtualMemory";
    case WINDOWS_ABI64_SYSCALL_NTCREATEFILE: return "windows.NtCreateFile";
    case WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT: return "windows.NtCreateMutant";
    case WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT: return "windows.NtReleaseMutant";
    case WINDOWS_ABI64_SYSCALL_NTCREATEEVENT: return "windows.NtCreateEvent";
    case WINDOWS_ABI64_SYSCALL_NTSETEVENT: return "windows.NtSetEvent";
    default: return "windows.unknown";
    }
}

static const char *persona_audit64_macos_syscall_name(u32 syscall_number)
{
    switch (syscall_number)
    {
    case MACOS_ABI64_SYSCALL_EXIT: return "macos.exit";
    case MACOS_ABI64_SYSCALL_READ: return "macos.read";
    case MACOS_ABI64_SYSCALL_WRITE: return "macos.write";
    case MACOS_ABI64_SYSCALL_OPEN: return "macos.open";
    case MACOS_ABI64_SYSCALL_CLOSE: return "macos.close";
    case MACOS_ABI64_SYSCALL_GETPID: return "macos.getpid";
    case MACOS_ABI64_SYSCALL_MUNMAP: return "macos.munmap";
    case MACOS_ABI64_SYSCALL_MPROTECT: return "macos.mprotect";
    case MACOS_ABI64_SYSCALL_CLOCK_GETTIME: return "macos.clock_gettime";
    case MACOS_ABI64_SYSCALL_STAT: return "macos.stat";
    case MACOS_ABI64_SYSCALL_FSTAT: return "macos.fstat";
    case MACOS_ABI64_SYSCALL_MMAP: return "macos.mmap";
    case MACOS_ABI64_SYSCALL_SYSCTL: return "macos.sysctl";
    default: return "macos.unknown";
    }
}

const char *persona_audit64_syscall_name(u32 persona_type, u32 syscall_number)
{
    switch (persona_type)
    {
    case PERSONA64_TYPE_LINUX_ELF:
        return persona_audit64_linux_syscall_name(syscall_number);
    case PERSONA64_TYPE_WINDOWS_PE:
        return persona_audit64_windows_syscall_name(syscall_number);
    case PERSONA64_TYPE_MACOS_MACHO:
        return persona_audit64_macos_syscall_name(syscall_number);
    default:
        return "native.unknown";
    }
}

u32 persona_audit64_syscall_name_token(u32 persona_type, u32 syscall_number)
{
    return persona_audit64_hash_text(
        persona_audit64_syscall_name(persona_type, syscall_number));
}

static u32 persona_audit64_linux_operation(u32 syscall_number)
{
    switch (syscall_number)
    {
    case LINUX_ABI64_SYSCALL_READ:
    case LINUX_ABI64_SYSCALL_WRITE:
    case LINUX_ABI64_SYSCALL_OPEN:
    case LINUX_ABI64_SYSCALL_STAT:
    case LINUX_ABI64_SYSCALL_FSTAT:
    case LINUX_ABI64_SYSCALL_LSEEK:
    case LINUX_ABI64_SYSCALL_PREAD64:
    case LINUX_ABI64_SYSCALL_PWRITE64:
    case LINUX_ABI64_SYSCALL_READV:
    case LINUX_ABI64_SYSCALL_WRITEV:
    case LINUX_ABI64_SYSCALL_OPENAT:
    case LINUX_ABI64_SYSCALL_NEWFSTATAT:
    case LINUX_ABI64_SYSCALL_GETDENTS64:
        return PERSONA_AUDIT64_OP_FILE_IO;
    case LINUX_ABI64_SYSCALL_MMAP:
    case LINUX_ABI64_SYSCALL_MPROTECT:
    case LINUX_ABI64_SYSCALL_MUNMAP:
    case LINUX_ABI64_SYSCALL_BRK:
        return PERSONA_AUDIT64_OP_MEMORY;
    case LINUX_ABI64_SYSCALL_GETPID:
    case LINUX_ABI64_SYSCALL_GETTID:
    case LINUX_ABI64_SYSCALL_CLONE:
    case LINUX_ABI64_SYSCALL_FORK:
    case LINUX_ABI64_SYSCALL_EXECVE:
    case LINUX_ABI64_SYSCALL_EXIT:
    case LINUX_ABI64_SYSCALL_WAIT4:
    case LINUX_ABI64_SYSCALL_PRCTL:
    case LINUX_ABI64_SYSCALL_ARCH_PRCTL:
    case LINUX_ABI64_SYSCALL_SET_TID_ADDRESS:
    case LINUX_ABI64_SYSCALL_EXIT_GROUP:
    case LINUX_ABI64_SYSCALL_EXECVEAT:
    case LINUX_ABI64_SYSCALL_GETRLIMIT:
    case LINUX_ABI64_SYSCALL_SETRLIMIT:
        return PERSONA_AUDIT64_OP_PROCESS;
    case LINUX_ABI64_SYSCALL_NANOSLEEP:
    case LINUX_ABI64_SYSCALL_CLOCK_GETTIME:
        return PERSONA_AUDIT64_OP_TIME;
    case LINUX_ABI64_SYSCALL_CLOSE:
    case LINUX_ABI64_SYSCALL_DUP:
    case LINUX_ABI64_SYSCALL_DUP2:
    case LINUX_ABI64_SYSCALL_FCNTL:
    case LINUX_ABI64_SYSCALL_DUP3:
        return PERSONA_AUDIT64_OP_FD;
    case LINUX_ABI64_SYSCALL_POLL:
    case LINUX_ABI64_SYSCALL_FUTEX:
    case LINUX_ABI64_SYSCALL_PIPE2:
    case LINUX_ABI64_SYSCALL_PPOLL:
        return PERSONA_AUDIT64_OP_IPC;
    case LINUX_ABI64_SYSCALL_RT_SIGACTION:
    case LINUX_ABI64_SYSCALL_RT_SIGPROCMASK:
    case LINUX_ABI64_SYSCALL_RT_SIGRETURN:
    case LINUX_ABI64_SYSCALL_KILL:
    case LINUX_ABI64_SYSCALL_TKILL:
        return PERSONA_AUDIT64_OP_SIGNAL;
    case LINUX_ABI64_SYSCALL_GETRANDOM:
        return PERSONA_AUDIT64_OP_RANDOM;
    case LINUX_ABI64_SYSCALL_GETCWD:
    case LINUX_ABI64_SYSCALL_CHDIR:
    case LINUX_ABI64_SYSCALL_FCHDIR:
        return PERSONA_AUDIT64_OP_VFS;
    default:
        return PERSONA_AUDIT64_OP_UNAVAILABLE;
    }
}

static u32 persona_audit64_windows_operation(u32 syscall_number)
{
    switch (syscall_number)
    {
    case WINDOWS_ABI64_SYSCALL_NTREADFILE:
    case WINDOWS_ABI64_SYSCALL_NTWRITEFILE:
    case WINDOWS_ABI64_SYSCALL_NTCREATEFILE:
        return PERSONA_AUDIT64_OP_FILE_IO;
    case WINDOWS_ABI64_SYSCALL_NTALLOCATEVIRTUALMEMORY:
    case WINDOWS_ABI64_SYSCALL_NTFREEVIRTUALMEMORY:
    case WINDOWS_ABI64_SYSCALL_NTPROTECTVIRTUALMEMORY:
        return PERSONA_AUDIT64_OP_MEMORY;
    case WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONPROCESS:
    case WINDOWS_ABI64_SYSCALL_NTQUERYSYSTEMINFORMATION:
        return PERSONA_AUDIT64_OP_PROCESS;
    case WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT:
    case WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT:
    case WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT:
    case WINDOWS_ABI64_SYSCALL_NTCREATEEVENT:
    case WINDOWS_ABI64_SYSCALL_NTSETEVENT:
        return PERSONA_AUDIT64_OP_NT_OBJECT;
    case WINDOWS_ABI64_SYSCALL_NTOPENKEY:
    case WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY:
    case WINDOWS_ABI64_SYSCALL_NTCREATEKEY:
        return PERSONA_AUDIT64_OP_REGISTRY;
    default:
        return PERSONA_AUDIT64_OP_UNAVAILABLE;
    }
}

static u32 persona_audit64_macos_operation(u32 syscall_number)
{
    switch (syscall_number)
    {
    case MACOS_ABI64_SYSCALL_READ:
    case MACOS_ABI64_SYSCALL_WRITE:
    case MACOS_ABI64_SYSCALL_OPEN:
    case MACOS_ABI64_SYSCALL_CLOSE:
    case MACOS_ABI64_SYSCALL_STAT:
    case MACOS_ABI64_SYSCALL_FSTAT:
        return PERSONA_AUDIT64_OP_FILE_IO;
    case MACOS_ABI64_SYSCALL_MMAP:
    case MACOS_ABI64_SYSCALL_MUNMAP:
    case MACOS_ABI64_SYSCALL_MPROTECT:
        return PERSONA_AUDIT64_OP_MEMORY;
    case MACOS_ABI64_SYSCALL_EXIT:
    case MACOS_ABI64_SYSCALL_GETPID:
    case MACOS_ABI64_SYSCALL_SYSCTL:
        return PERSONA_AUDIT64_OP_PROCESS;
    case MACOS_ABI64_SYSCALL_CLOCK_GETTIME:
        return PERSONA_AUDIT64_OP_TIME;
    default:
        return PERSONA_AUDIT64_OP_UNAVAILABLE;
    }
}

u32 persona_audit64_translated_operation(u32 persona_type, u32 syscall_number)
{
    switch (persona_type)
    {
    case PERSONA64_TYPE_LINUX_ELF:
        return persona_audit64_linux_operation(syscall_number);
    case PERSONA64_TYPE_WINDOWS_PE:
        return persona_audit64_windows_operation(syscall_number);
    case PERSONA64_TYPE_MACOS_MACHO:
        return persona_audit64_macos_operation(syscall_number);
    default:
        return PERSONA_AUDIT64_OP_UNAVAILABLE;
    }
}

const char *persona_audit64_operation_name(u32 operation)
{
    switch (operation)
    {
    case PERSONA_AUDIT64_OP_NONE: return "none";
    case PERSONA_AUDIT64_OP_FILE_IO: return "file-io";
    case PERSONA_AUDIT64_OP_MEMORY: return "memory";
    case PERSONA_AUDIT64_OP_PROCESS: return "process";
    case PERSONA_AUDIT64_OP_TIME: return "time";
    case PERSONA_AUDIT64_OP_FD: return "fd";
    case PERSONA_AUDIT64_OP_IPC: return "ipc";
    case PERSONA_AUDIT64_OP_SIGNAL: return "signal";
    case PERSONA_AUDIT64_OP_RANDOM: return "random";
    case PERSONA_AUDIT64_OP_SCHEDULER: return "scheduler";
    case PERSONA_AUDIT64_OP_VFS: return "vfs";
    case PERSONA_AUDIT64_OP_REGISTRY: return "registry";
    case PERSONA_AUDIT64_OP_NT_OBJECT: return "nt-object";
    case PERSONA_AUDIT64_OP_MACH_PORT: return "mach-port";
    case PERSONA_AUDIT64_OP_UNAVAILABLE: return "unavailable";
    default: return "unknown";
    }
}

void persona_audit64_init(void)
{
    u32 index;

    if (g_persona_audit64_initialized != 0u)
    {
        return;
    }

    for (index = 0u; index < PERSONA_AUDIT64_MAX_CONTEXTS; ++index)
    {
        g_persona_audit64_context_used[index] = 0u;
        persona_audit64_clear_context(&g_persona_audit64_contexts[index]);
    }

    g_persona_audit64_sequence = 1u;
    g_persona_audit64_initialized = 1u;
}

persona_audit64_context_t *persona_audit64_context_for_process(u32 pid)
{
    persona_audit64_init();
    return (persona_audit64_context_t *)process64_audit_ctx(pid);
}

u32 persona_audit64_attach(u32 pid)
{
    persona_audit64_context_t *context;

    persona_audit64_init();

    if ((pid == PROCESS64_INVALID_PID)
        || (process64_principal(pid) == 0u)
        || (process64_audit_ctx(pid) != 0))
    {
        return 0u;
    }

    context = persona_audit64_acquire_context(pid);
    if (context == 0)
    {
        return 0u;
    }

    if (process64_attach_audit(pid, context) == 0u)
    {
        persona_audit64_release_context(context);
        return 0u;
    }

    return 1u;
}

u32 persona_audit64_release(u32 pid)
{
    persona_audit64_context_t *context;
    void *detached;

    persona_audit64_init();

    if ((pid == PROCESS64_INVALID_PID) || (process64_principal(pid) == 0u))
    {
        return 0u;
    }

    context = persona_audit64_context_for_process(pid);
    if (context == 0)
    {
        return 0u;
    }

    detached = process64_detach_audit(pid);
    if (detached != context)
    {
        return 0u;
    }

    persona_audit64_release_context(context);
    return 1u;
}

u32 persona_audit64_record(u32 pid, u8 event_type, u16 event_code, u32 result, u64 rip)
{
    persona_audit64_context_t *context;
    persona_audit64_record_t *record;
    u32 persona_type;

    persona_audit64_init();

    context = persona_audit64_context_for_process(pid);
    if (context == 0)
    {
        return 0u;
    }

    record = &context->records[context->write_index];
    persona_type = persona64_type(pid);
    if (persona_type >= PERSONA64_TYPE_COUNT)
    {
        persona_type = PERSONA64_TYPE_LIMITLESS_NATIVE;
    }

    record->timestamp = persona_audit64_timestamp();
    record->pid = pid;
    record->persona_type = (u8)persona_type;
    record->event_type = event_type;
    record->event_code = event_code;
    record->result = result;
    if ((event_type == PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED)
        || (event_type == PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED))
    {
        record->syscall_name_token =
            persona_audit64_syscall_name_token(persona_type, (u32)event_code);
        record->translated_operation =
            (event_type == PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED)
                ? PERSONA_AUDIT64_OP_UNAVAILABLE
                : persona_audit64_translated_operation(persona_type, (u32)event_code);
    }
    else
    {
        record->syscall_name_token = 0u;
        record->translated_operation = PERSONA_AUDIT64_OP_NONE;
    }
    record->rip = rip;

    context->write_index = (context->write_index + 1u) % PERSONA_AUDIT64_RING_CAPACITY;
    if (context->count < PERSONA_AUDIT64_RING_CAPACITY)
    {
        ++context->count;
    }
    else
    {
        ++context->dropped_count;
    }

    return 1u;
}

u32 persona_audit64_count(u32 pid)
{
    persona_audit64_context_t *context = persona_audit64_context_for_process(pid);

    return (context != 0) ? context->count : 0u;
}

u32 persona_audit64_dropped_count(u32 pid)
{
    persona_audit64_context_t *context = persona_audit64_context_for_process(pid);

    return (context != 0) ? context->dropped_count : 0u;
}

u32 persona_audit64_read(u32 pid, u32 index, persona_audit64_record_t *out_record)
{
    persona_audit64_context_t *context;
    u32 start;
    u32 physical_index;

    persona_audit64_init();

    context = persona_audit64_context_for_process(pid);
    if ((context == 0) || (out_record == 0) || (index >= context->count))
    {
        return 0u;
    }

    start = (context->write_index + PERSONA_AUDIT64_RING_CAPACITY - context->count)
        % PERSONA_AUDIT64_RING_CAPACITY;
    physical_index = (start + index) % PERSONA_AUDIT64_RING_CAPACITY;
    *out_record = context->records[physical_index];

    return 1u;
}

static u32 persona_audit64_crash_result_for_persona(u32 persona_type, u32 vector)
{
    if ((persona_type == PERSONA64_TYPE_LINUX_ELF)
        || (persona_type == PERSONA64_TYPE_MACOS_MACHO))
    {
        if (vector == PERSONA_AUDIT64_CRASH_VECTOR_INVALID_OPCODE)
        {
            return PERSONA_AUDIT64_CRASH_SIGILL;
        }
        if (vector == PERSONA_AUDIT64_CRASH_VECTOR_DIVIDE)
        {
            return PERSONA_AUDIT64_CRASH_SIGFPE;
        }

        return PERSONA_AUDIT64_CRASH_SIGSEGV;
    }

    if (persona_type == PERSONA64_TYPE_WINDOWS_PE)
    {
        if (vector == PERSONA_AUDIT64_CRASH_VECTOR_INVALID_OPCODE)
        {
            return PERSONA_AUDIT64_CRASH_STATUS_ILLEGAL_INSTRUCTION;
        }
        if (vector == PERSONA_AUDIT64_CRASH_VECTOR_DIVIDE)
        {
            return PERSONA_AUDIT64_CRASH_STATUS_INTEGER_DIVIDE_BY_ZERO;
        }

        return PERSONA_AUDIT64_CRASH_STATUS_ACCESS_VIOLATION;
    }

    return PERSONA_AUDIT64_RESULT_DENY;
}

static u16 persona_audit64_crash_event_code(u32 persona_type, u32 vector, u32 abi_code)
{
    if ((persona_type == PERSONA64_TYPE_LINUX_ELF)
        || (persona_type == PERSONA64_TYPE_MACOS_MACHO))
    {
        return (u16)abi_code;
    }

    return (u16)vector;
}

u32 persona_audit64_record_crash(
    u32 pid,
    u32 vector,
    u64 error_code,
    u64 rip,
    u64 rsp,
    u64 fault_address)
{
    persona_audit64_context_t *context;
    persona_audit64_record_t record;
    persona_audit64_crash_report_t report;
    const vma_region_t *first_region;
    vma_region_t *fault_region;
    u32 persona_type;
    u32 abi_code;
    u32 recorded;

    persona_audit64_init();

    context = persona_audit64_context_for_process(pid);
    if ((context == 0) || (pid == PROCESS64_INVALID_PID))
    {
        return 0u;
    }

    persona_type = persona64_type(pid);
    if ((persona_type != PERSONA64_TYPE_LINUX_ELF)
        && (persona_type != PERSONA64_TYPE_WINDOWS_PE)
        && (persona_type != PERSONA64_TYPE_MACOS_MACHO))
    {
        return 0u;
    }

    abi_code = persona_audit64_crash_result_for_persona(persona_type, vector);
    recorded = persona_audit64_record(
        pid,
        PERSONA_AUDIT64_EVENT_CRASH,
        persona_audit64_crash_event_code(persona_type, vector, abi_code),
        abi_code,
        rip);
    if (recorded == 0u)
    {
        return 0u;
    }

    persona_audit64_clear_crash_report(&report);
    report.pid = pid;
    report.persona_type = (u8)persona_type;
    report.vector = (u16)vector;
    report.abi_code = abi_code;
    report.result = abi_code;
    report.rip = rip;
    report.rsp = rsp;
    report.error_code = error_code;
    report.fault_address = fault_address;
    report.vma_region_count = vma64_region_count(pid);
    report.vma_mapped_bytes = vma64_mapped_bytes(pid);

    first_region = vma64_first_region(pid);
    if (first_region != 0)
    {
        report.vma_dump_count = 1u;
        report.vma_first_base = first_region->virt_base;
        report.vma_first_end = first_region->virt_end;
        report.vma_first_prot = first_region->prot_flags;
    }

    fault_region = vma64_find(pid, fault_address);
    if (fault_region != 0)
    {
        report.fault_region_present = 1u;
        report.fault_region_base = fault_region->virt_base;
        report.fault_region_end = fault_region->virt_end;
        report.fault_region_prot = fault_region->prot_flags;
    }

    if ((context->count != 0u)
        && (persona_audit64_read(pid, context->count - 1u, &record) != 0u))
    {
        report.timestamp = record.timestamp;
    }

    context->last_crash = report;
    ++context->crash_count;
    return 1u;
}

u32 persona_audit64_read_last_crash(u32 pid, persona_audit64_crash_report_t *out_report)
{
    persona_audit64_context_t *context = persona_audit64_context_for_process(pid);

    if ((context == 0) || (out_report == 0) || (context->crash_count == 0u))
    {
        return 0u;
    }

    *out_report = context->last_crash;
    return 1u;
}

u32 persona_audit64_crash_count(u32 pid)
{
    persona_audit64_context_t *context = persona_audit64_context_for_process(pid);

    return (context != 0) ? context->crash_count : 0u;
}

u32 persona_audit64_last_event_type(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? (u32)record.event_type
        : 0u;
}

u32 persona_audit64_last_event_code(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? (u32)record.event_code
        : 0u;
}

u32 persona_audit64_last_result(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? record.result
        : 0u;
}

u32 persona_audit64_last_persona_type(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? (u32)record.persona_type
        : PERSONA64_TYPE_COUNT;
}

u32 persona_audit64_last_syscall_name_token(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? record.syscall_name_token
        : 0u;
}

u32 persona_audit64_last_translated_operation(u32 pid)
{
    persona_audit64_record_t record;
    u32 count = persona_audit64_count(pid);

    return ((count != 0u) && (persona_audit64_read(pid, count - 1u, &record) != 0u))
        ? record.translated_operation
        : PERSONA_AUDIT64_OP_NONE;
}
