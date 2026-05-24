#ifndef LIMITLESS_PERSONA_AUDIT_X64_H
#define LIMITLESS_PERSONA_AUDIT_X64_H

#include "types.h"

#define PERSONA_AUDIT64_RING_CAPACITY 256u
#define PERSONA_AUDIT64_MAX_CONTEXTS 16u

#define PERSONA_AUDIT64_RESULT_OK 0u
#define PERSONA_AUDIT64_RESULT_DENY 1u
#define PERSONA_AUDIT64_RESULT_ENOSYS 38u

#define PERSONA_AUDIT64_EVENT_FORMAT_REJECTED 1u
#define PERSONA_AUDIT64_EVENT_SYSCALL_UNIMPLEMENTED 2u
#define PERSONA_AUDIT64_EVENT_SYSCALL_TRANSLATED 3u
#define PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED 4u
#define PERSONA_AUDIT64_EVENT_CRASH 5u

#define PERSONA_AUDIT64_CRASH_VECTOR_DIVIDE 0u
#define PERSONA_AUDIT64_CRASH_VECTOR_INVALID_OPCODE 6u
#define PERSONA_AUDIT64_CRASH_VECTOR_PAGE_FAULT 14u
#define PERSONA_AUDIT64_CRASH_SIGILL 4u
#define PERSONA_AUDIT64_CRASH_SIGFPE 8u
#define PERSONA_AUDIT64_CRASH_SIGSEGV 11u
#define PERSONA_AUDIT64_CRASH_STATUS_ACCESS_VIOLATION 0xC0000005u
#define PERSONA_AUDIT64_CRASH_STATUS_ILLEGAL_INSTRUCTION 0xC000001Du
#define PERSONA_AUDIT64_CRASH_STATUS_INTEGER_DIVIDE_BY_ZERO 0xC0000094u

#define PERSONA_AUDIT64_OP_NONE 0u
#define PERSONA_AUDIT64_OP_FILE_IO 1u
#define PERSONA_AUDIT64_OP_MEMORY 2u
#define PERSONA_AUDIT64_OP_PROCESS 3u
#define PERSONA_AUDIT64_OP_TIME 4u
#define PERSONA_AUDIT64_OP_FD 5u
#define PERSONA_AUDIT64_OP_IPC 6u
#define PERSONA_AUDIT64_OP_SIGNAL 7u
#define PERSONA_AUDIT64_OP_RANDOM 8u
#define PERSONA_AUDIT64_OP_SCHEDULER 9u
#define PERSONA_AUDIT64_OP_VFS 10u
#define PERSONA_AUDIT64_OP_REGISTRY 11u
#define PERSONA_AUDIT64_OP_NT_OBJECT 12u
#define PERSONA_AUDIT64_OP_MACH_PORT 13u
#define PERSONA_AUDIT64_OP_UNAVAILABLE 255u

typedef struct persona_audit64_record
{
    u64 timestamp;
    u32 pid;
    u8 persona_type;
    u8 event_type;
    u16 event_code;
    u32 result;
    u32 syscall_name_token;
    u32 translated_operation;
    u64 rip;
} persona_audit64_record_t;

typedef struct persona_audit64_crash_report
{
    u64 timestamp;
    u32 pid;
    u8 persona_type;
    u8 reserved0;
    u16 vector;
    u32 abi_code;
    u32 result;
    u64 rip;
    u64 rsp;
    u64 error_code;
    u64 fault_address;
    u32 vma_region_count;
    u32 vma_dump_count;
    u64 vma_mapped_bytes;
    u64 vma_first_base;
    u64 vma_first_end;
    u32 vma_first_prot;
    u32 fault_region_present;
    u64 fault_region_base;
    u64 fault_region_end;
    u32 fault_region_prot;
    u32 reserved1;
} persona_audit64_crash_report_t;

typedef struct persona_audit64_context
{
    u32 pid;
    u32 write_index;
    u32 count;
    u32 dropped_count;
    u32 crash_count;
    u32 reserved;
    persona_audit64_crash_report_t last_crash;
    persona_audit64_record_t records[PERSONA_AUDIT64_RING_CAPACITY];
} persona_audit64_context_t;

void persona_audit64_init(void);
persona_audit64_context_t *persona_audit64_context_for_process(u32 pid);
u32 persona_audit64_attach(u32 pid);
u32 persona_audit64_release(u32 pid);
u32 persona_audit64_record(u32 pid, u8 event_type, u16 event_code, u32 result, u64 rip);
u32 persona_audit64_count(u32 pid);
u32 persona_audit64_dropped_count(u32 pid);
u32 persona_audit64_read(u32 pid, u32 index, persona_audit64_record_t *out_record);
u32 persona_audit64_record_crash(
    u32 pid,
    u32 vector,
    u64 error_code,
    u64 rip,
    u64 rsp,
    u64 fault_address);
u32 persona_audit64_read_last_crash(u32 pid, persona_audit64_crash_report_t *out_report);
u32 persona_audit64_crash_count(u32 pid);
u32 persona_audit64_last_event_type(u32 pid);
u32 persona_audit64_last_event_code(u32 pid);
u32 persona_audit64_last_result(u32 pid);
u32 persona_audit64_last_persona_type(u32 pid);
const char *persona_audit64_syscall_name(u32 persona_type, u32 syscall_number);
u32 persona_audit64_syscall_name_token(u32 persona_type, u32 syscall_number);
u32 persona_audit64_translated_operation(u32 persona_type, u32 syscall_number);
const char *persona_audit64_operation_name(u32 operation);
u32 persona_audit64_last_syscall_name_token(u32 pid);
u32 persona_audit64_last_translated_operation(u32 pid);

#endif
