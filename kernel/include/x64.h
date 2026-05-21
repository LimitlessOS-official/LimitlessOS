#ifndef LIMITLESS_X64_H
#define LIMITLESS_X64_H

#include "types.h"

#define X64_MSR_IA32_FS_BASE 0xC0000100u
#define X64_MSR_IA32_GS_BASE 0xC0000101u

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

static inline u16 inw(u16 port)
{
    u16 value;
    __asm__ __volatile__("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(u16 port, u32 value)
{
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline u32 inl(u16 port)
{
    u32 value;
    __asm__ __volatile__("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
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

static inline u64 read_cr2_64(void)
{
    u64 value;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(value));
    return value;
}

static inline u64 read_cr3_64(void)
{
    u64 value;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(value));
    return value;
}

static inline void write_cr3_64(u64 value)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(value) : "memory");
}

static inline u64 read_rsp64(void)
{
    u64 value;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(value));
    return value;
}

static inline u16 read_cs64(void)
{
    u16 value;
    __asm__ __volatile__("mov %%cs, %0" : "=r"(value));
    return value;
}

static inline u16 read_ss64(void)
{
    u16 value;
    __asm__ __volatile__("mov %%ss, %0" : "=r"(value));
    return value;
}

static inline u64 rdmsr64(u32 msr)
{
    u32 low;
    u32 high;

    __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((u64)high << 32) | (u64)low;
}

static inline void wrmsr64(u32 msr, u64 value)
{
    u32 low = (u32)(value & 0xFFFFFFFFu);
    u32 high = (u32)(value >> 32);

    __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline u64 read_fs_base64(void)
{
    return rdmsr64(X64_MSR_IA32_FS_BASE);
}

static inline void write_fs_base64(u64 value)
{
    wrmsr64(X64_MSR_IA32_FS_BASE, value);
}

static inline u64 read_gs_base64(void)
{
    return rdmsr64(X64_MSR_IA32_GS_BASE);
}

static inline void write_gs_base64(u64 value)
{
    wrmsr64(X64_MSR_IA32_GS_BASE, value);
}

static inline void lgdt64(const void *base, u16 limit)
{
    struct
    {
        u16 limit;
        u64 base;
    } __attribute__((packed)) gdtr;

    gdtr.limit = limit;
    gdtr.base = (u64)base;

    __asm__ __volatile__("lgdt %0" : : "m"(gdtr) : "memory");
}

static inline void lidt64(const void *base, u16 limit)
{
    struct
    {
        u16 limit;
        u64 base;
    } __attribute__((packed)) idtr;

    idtr.limit = limit;
    idtr.base = (u64)base;

    __asm__ __volatile__("lidt %0" : : "m"(idtr));
}

static inline void load_data_segments64(u16 selector)
{
    __asm__ __volatile__(
        "mov %0, %%ds\n\t"
        "mov %0, %%es\n\t"
        "mov %0, %%ss"
        :
        : "r"(selector)
        : "memory");
}

static inline void load_code_segment64(u16 selector)
{
    __asm__ __volatile__(
        "pushq %[selector]\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:"
        :
        : [selector] "r"((u64)selector)
        : "rax", "memory");
}

static inline void ltr64(u16 selector)
{
    __asm__ __volatile__("ltr %0" : : "r"(selector) : "memory");
}

static inline u16 read_tr64(void)
{
    u16 value;
    __asm__ __volatile__("str %0" : "=r"(value));
    return value;
}

#endif
