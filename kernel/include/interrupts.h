#ifndef LIMITLESS_INTERRUPTS_H
#define LIMITLESS_INTERRUPTS_H

#include "types.h"

struct interrupt_frame
{
    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 vector;
    u32 error_code;
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 user_esp;
    u32 user_ss;
};

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame);

#endif
