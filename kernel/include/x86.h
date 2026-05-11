#ifndef LIMITLESS_X86_H
#define LIMITLESS_X86_H

#include "types.h"

static inline void outb(u16 port, u8 value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port)
{
    u8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline u32 read_cr0(void)
{
    u32 value;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(value));
    return value;
}

static inline void write_cr0(u32 value)
{
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(value) : "memory");
}

static inline u32 read_cr2(void)
{
    u32 value;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(value));
    return value;
}

static inline void write_cr3(u32 value)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(value) : "memory");
}

static inline void invlpg(const void *address)
{
    __asm__ __volatile__("invlpg (%0)" : : "r"(address) : "memory");
}

static inline void cpu_cli(void)
{
    __asm__ __volatile__("cli");
}

static inline void cpu_sti(void)
{
    __asm__ __volatile__("sti");
}

static inline void cpu_halt(void)
{
    __asm__ __volatile__("hlt");
}

static inline void cpu_halt_forever(void)
{
    for (;;)
    {
        cpu_halt();
    }
}

static inline void lidt(const void *base, u16 limit)
{
    struct
    {
        u16 limit;
        u32 base;
    } __attribute__((packed)) idtr;

    idtr.limit = limit;
    idtr.base = (u32)base;

    __asm__ __volatile__("lidt %0" : : "m"(idtr));
}

static inline void lgdt(const void *base, u16 limit)
{
    struct
    {
        u16 limit;
        u32 base;
    } __attribute__((packed)) gdtr;

    gdtr.limit = limit;
    gdtr.base = (u32)base;

    __asm__ __volatile__("lgdt %0" : : "m"(gdtr));
}

static inline void ltr(u16 selector)
{
    __asm__ __volatile__("ltr %0" : : "r"(selector));
}

#endif
