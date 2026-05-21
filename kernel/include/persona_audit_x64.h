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

typedef struct persona_audit64_record
{
    u64 timestamp;
    u32 pid;
    u8 persona_type;
    u8 event_type;
    u16 event_code;
    u32 result;
    u64 rip;
} persona_audit64_record_t;

typedef struct persona_audit64_context
{
    u32 pid;
    u32 write_index;
    u32 count;
    u32 dropped_count;
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
u32 persona_audit64_last_event_type(u32 pid);
u32 persona_audit64_last_event_code(u32 pid);
u32 persona_audit64_last_result(u32 pid);
u32 persona_audit64_last_persona_type(u32 pid);

#endif
