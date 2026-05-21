#ifndef LIMITLESS_WINDOWS_SEH_X64_H
#define LIMITLESS_WINDOWS_SEH_X64_H

#include "interrupts_x64.h"
#include "types.h"

#define WINDOWS_SEH64_CHAIN_END 0xFFFFFFFFFFFFFFFFull
#define WINDOWS_SEH64_MAX_CHAIN_DEPTH 8u
#define WINDOWS_SEH64_USER_TOP 0x0000800000000000ull
#define WINDOWS_SEH64_FRAME_ALIGN 16ull
#define WINDOWS_SEH64_USER_FRAME_BYTES 144u

#define WINDOWS_SEH64_STATUS_SUCCESS 0x00000000u
#define WINDOWS_SEH64_STATUS_INVALID_PARAMETER 0xC000000Du
#define WINDOWS_SEH64_STATUS_ACCESS_VIOLATION 0xC0000005u
#define WINDOWS_SEH64_STATUS_ILLEGAL_INSTRUCTION 0xC000001Du
#define WINDOWS_SEH64_STATUS_INTEGER_DIVIDE_BY_ZERO 0xC0000094u
#define WINDOWS_SEH64_STATUS_NOT_IMPLEMENTED 0xC0000002u
#define WINDOWS_SEH64_STATUS_UNHANDLED_EXCEPTION 0xC0000144u

#define WINDOWS_SEH64_EVENT_CODE_DISPATCH 0x0006u

typedef struct windows_seh64_registration
{
    u64 next;
    u64 handler;
} windows_seh64_registration_t;

typedef struct windows_seh64_exception_record
{
    u32 code;
    u32 flags;
    u64 address;
    u64 information0;
    u64 information1;
} windows_seh64_exception_record_t;

typedef struct windows_seh64_context_record
{
    u64 rip;
    u64 rsp;
    u64 rflags;
    u64 rax;
    u64 rcx;
    u64 rdx;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
} windows_seh64_context_record_t;

typedef struct windows_seh64_dispatcher_context
{
    u64 registration;
    u64 handler;
    u32 exception_vector;
    u32 chain_depth;
} windows_seh64_dispatcher_context_t;

typedef struct windows_seh64_user_frame
{
    windows_seh64_exception_record_t exception_record;
    windows_seh64_context_record_t context_record;
    windows_seh64_dispatcher_context_t dispatcher_context;
    u64 return_address;
} windows_seh64_user_frame_t;

void windows_seh64_init(void);
u32 windows_seh64_dispatch_exception(u32 pid, struct interrupt_frame64 *frame);
u32 windows_seh64_dispatch_count(void);
u32 windows_seh64_denial_count(void);
u32 windows_seh64_unhandled_count(void);
u32 windows_seh64_last_result(void);
u32 windows_seh64_last_vector(void);
u32 windows_seh64_last_chain_depth(void);
u64 windows_seh64_last_teb(void);
u64 windows_seh64_last_registration(void);
u64 windows_seh64_last_handler(void);
u64 windows_seh64_last_frame(void);
u64 windows_seh64_last_exception_record(void);
u64 windows_seh64_last_context_record(void);
u64 windows_seh64_last_dispatcher_context(void);

#endif
