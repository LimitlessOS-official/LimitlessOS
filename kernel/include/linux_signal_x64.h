#ifndef LIMITLESS_LINUX_SIGNAL_X64_H
#define LIMITLESS_LINUX_SIGNAL_X64_H

#include "types.h"

#define LINUX_SIGNAL64_MAX_SIGNALS 64u
#define LINUX_SIGNAL64_SIGILL 4u
#define LINUX_SIGNAL64_SIGFPE 8u
#define LINUX_SIGNAL64_SIGUSR1 10u
#define LINUX_SIGNAL64_SIGSEGV 11u
#define LINUX_SIGNAL64_SIGKILL 9u
#define LINUX_SIGNAL64_SIGSTOP 19u
#define LINUX_SIGNAL64_SIG_BLOCK 0u
#define LINUX_SIGNAL64_SIG_UNBLOCK 1u
#define LINUX_SIGNAL64_SIG_SETMASK 2u
#define LINUX_SIGNAL64_SIGUSR1_MASK (1ull << (LINUX_SIGNAL64_SIGUSR1 - 1u))
#define LINUX_SIGNAL64_SIGPIPE 13u
#define LINUX_SIGNAL64_SIGCHLD 17u
#define LINUX_SIGNAL64_SIGTERM 15u
#define LINUX_SIGNAL64_SIGINT 2u
#define LINUX_SIGNAL64_SIGPIPE_MASK (1ull << (LINUX_SIGNAL64_SIGPIPE - 1u))
#define LINUX_SIGNAL64_SIGCHLD_MASK (1ull << (LINUX_SIGNAL64_SIGCHLD - 1u))
#define LINUX_SIGNAL64_SIGKILL_MASK (1ull << (LINUX_SIGNAL64_SIGKILL - 1u))
#define LINUX_SIGNAL64_SIGSTOP_MASK (1ull << (LINUX_SIGNAL64_SIGSTOP - 1u))
#define LINUX_SIGNAL64_UNBLOCKABLE_MASK \
    (LINUX_SIGNAL64_SIGKILL_MASK | LINUX_SIGNAL64_SIGSTOP_MASK)
#define LINUX_SIGNAL64_SIGACTION_BYTES 32u
#define LINUX_SIGNAL64_SIGINFO_BYTES 128u
#define LINUX_SIGNAL64_UCONTEXT_BYTES 256u
#define LINUX_SIGNAL64_DELIVERY_ALIGN 16u
#define LINUX_SIGNAL64_REDZONE_BYTES 128u
#define LINUX_SIGNAL64_RFLAGS_FIXED_BIT 0x0000000000000002ull
#define LINUX_SIGNAL64_RFLAGS_FORBIDDEN_MASK 0x0000000000300000ull
#define LINUX_SIGNAL64_PENDING_NONE 0ull
#define LINUX_SIGNAL64_MASK_EMPTY 0ull
#define LINUX_SIGNAL64_DEFAULT_HANDLER 0ull
#define LINUX_SIGNAL64_FLAGS_NONE 0ull

typedef struct linux_signal64_sigaction
{
    u64 handler;
    u64 sa_flags;
    u64 restorer;
    u64 sa_mask;
} linux_signal64_sigaction_t;

typedef struct linux_signal64_siginfo
{
    u32 signo;
    u32 errno_value;
    u32 code;
    u32 sender_pid;
    u32 sender_uid;
    u32 reserved0;
    u64 reserved[13];
} linux_signal64_siginfo_t;

typedef struct linux_signal64_stack
{
    u64 sp;
    u32 flags;
    u32 reserved0;
    u64 size;
} linux_signal64_stack_t;

typedef struct linux_signal64_mcontext
{
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
    u64 rdi;
    u64 rsi;
    u64 rbp;
    u64 rbx;
    u64 rdx;
    u64 rax;
    u64 rcx;
    u64 rsp;
    u64 rip;
    u64 eflags;
    u64 cs;
    u64 gs;
    u64 fs;
    u64 ss;
    u64 err;
    u64 trapno;
    u64 oldmask;
    u64 cr2;
    u64 fpstate;
    u64 reserved1[8];
} linux_signal64_mcontext_t;

typedef struct linux_signal64_ucontext
{
    u64 flags;
    u64 link;
    linux_signal64_stack_t stack;
    linux_signal64_mcontext_t mcontext;
    u64 sigmask;
} linux_signal64_ucontext_t;

typedef struct linux_signal64_delivery_frame
{
    u64 pretcode;
    linux_signal64_ucontext_t ucontext;
    linux_signal64_siginfo_t siginfo;
} linux_signal64_delivery_frame_t;

#endif
