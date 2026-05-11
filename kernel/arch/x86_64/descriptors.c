#include "descriptors_x64.h"

#include "x64.h"

enum
{
    GDT64_ENTRY_NULL = 0u,
    GDT64_ENTRY_CODE32 = 1u,
    GDT64_ENTRY_DATA32 = 2u,
    GDT64_ENTRY_KERNEL_CODE = 3u,
    GDT64_ENTRY_KERNEL_DATA = 4u,
    GDT64_ENTRY_USER_DATA = 5u,
    GDT64_ENTRY_USER_CODE = 6u,
    GDT64_ENTRY_TSS_LOW = 7u,
    GDT64_ENTRY_TSS_HIGH = 8u,
    GDT64_ENTRY_COUNT = 9u,
    TSS64_STACK_BYTES = 16384u
};

struct tss64
{
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 io_map_base;
} __attribute__((packed));

static u64 g_gdt[GDT64_ENTRY_COUNT];
static struct tss64 g_tss;
static u8 g_tss_stack[TSS64_STACK_BYTES] __attribute__((aligned(16)));
static volatile u32 g_descriptor_state = 0u;
static volatile u32 g_gdt_token = 0u;
static volatile u32 g_tss_token = 0u;

static u32 descriptors64_mix_token(u32 digest, u64 value)
{
    digest ^= (u32)(value & 0xFFFFFFFFull);
    digest = (digest << 5) | (digest >> 27);
    digest ^= (u32)(value >> 32);
    digest *= 16777619u;
    return digest;
}

static u64 descriptors64_tss_descriptor_low(u64 base, u32 limit)
{
    u64 descriptor = 0u;

    descriptor |= (u64)(limit & 0xFFFFu);
    descriptor |= (base & 0xFFFFFFull) << 16;
    descriptor |= 0x89ull << 40;
    descriptor |= (u64)((limit >> 16) & 0x0Fu) << 48;
    descriptor |= ((base >> 24) & 0xFFull) << 56;
    return descriptor;
}

static void descriptors64_install_tss_descriptor(void)
{
    u64 base = (u64)&g_tss;
    u32 limit = (u32)(sizeof(g_tss) - 1u);

    g_gdt[GDT64_ENTRY_TSS_LOW] = descriptors64_tss_descriptor_low(base, limit);
    g_gdt[GDT64_ENTRY_TSS_HIGH] = (base >> 32) & 0xFFFFFFFFull;
}

static u32 descriptors64_compute_gdt_token(void)
{
    u32 index;
    u32 digest = 0xD6475447u;

    for (index = 0u; index < GDT64_ENTRY_COUNT; ++index)
    {
        digest = descriptors64_mix_token(digest, g_gdt[index]);
    }

    digest = descriptors64_mix_token(digest, (u64)(u32)DESCRIPTORS64_KERNEL_CODE_SELECTOR);
    digest = descriptors64_mix_token(digest, (u64)(u32)DESCRIPTORS64_KERNEL_DATA_SELECTOR);
    digest = descriptors64_mix_token(digest, (u64)(u32)DESCRIPTORS64_USER_CODE_SELECTOR);
    digest = descriptors64_mix_token(digest, (u64)(u32)DESCRIPTORS64_USER_DATA_SELECTOR);
    digest = descriptors64_mix_token(digest, (u64)(u32)DESCRIPTORS64_TSS_SELECTOR);
    return digest;
}

static u32 descriptors64_compute_tss_token(void)
{
    u32 digest = 0x54535336u;

    digest = descriptors64_mix_token(digest, (u64)&g_tss);
    digest = descriptors64_mix_token(digest, g_tss.rsp0);
    digest = descriptors64_mix_token(digest, (u64)g_tss.io_map_base);
    digest = descriptors64_mix_token(digest, (u64)(u32)read_tr64());
    return digest;
}

static void descriptors64_seed_tables(void)
{
    u32 index;

    for (index = 0u; index < GDT64_ENTRY_COUNT; ++index)
    {
        g_gdt[index] = 0ull;
    }

    g_gdt[GDT64_ENTRY_CODE32] = 0x00CF9A000000FFFFull;
    g_gdt[GDT64_ENTRY_DATA32] = 0x00CF92000000FFFFull;
    g_gdt[GDT64_ENTRY_KERNEL_CODE] = 0x00AF9A000000FFFFull;
    g_gdt[GDT64_ENTRY_KERNEL_DATA] = 0x00CF92000000FFFFull;
    g_gdt[GDT64_ENTRY_USER_DATA] = 0x00CFF2000000FFFFull;
    g_gdt[GDT64_ENTRY_USER_CODE] = 0x00AFFA000000FFFFull;

    g_tss.reserved0 = 0u;
    g_tss.rsp0 = ((u64)&g_tss_stack[TSS64_STACK_BYTES]) & ~0xFull;
    g_tss.rsp1 = 0u;
    g_tss.rsp2 = 0u;
    g_tss.reserved1 = 0u;
    g_tss.ist1 = 0u;
    g_tss.ist2 = 0u;
    g_tss.ist3 = 0u;
    g_tss.ist4 = 0u;
    g_tss.ist5 = 0u;
    g_tss.ist6 = 0u;
    g_tss.ist7 = 0u;
    g_tss.reserved2 = 0u;
    g_tss.reserved3 = 0u;
    g_tss.io_map_base = (u16)sizeof(g_tss);

    descriptors64_install_tss_descriptor();
}

void descriptors64_init(void)
{
    u32 state = 0u;

    descriptors64_seed_tables();
    lgdt64(g_gdt, (u16)(sizeof(g_gdt) - 1u));
    load_code_segment64(DESCRIPTORS64_KERNEL_CODE_SELECTOR);
    load_data_segments64(DESCRIPTORS64_KERNEL_DATA_SELECTOR);
    ltr64(DESCRIPTORS64_TSS_SELECTOR);

    state |= DESCRIPTORS64_STATE_GDT_INSTALLED;
    state |= DESCRIPTORS64_STATE_TSS_PRESENT;

    if ((read_tr64() & 0xFFF8u) == DESCRIPTORS64_TSS_SELECTOR)
    {
        state |= DESCRIPTORS64_STATE_TSS_LOADED;
    }

    if ((read_cs64() & 0xFFF8u) == DESCRIPTORS64_KERNEL_CODE_SELECTOR)
    {
        state |= DESCRIPTORS64_STATE_KERNEL_CODE_ACTIVE;
    }

    if ((read_ss64() & 0xFFF8u) == DESCRIPTORS64_KERNEL_DATA_SELECTOR)
    {
        state |= DESCRIPTORS64_STATE_KERNEL_DATA_ACTIVE;
    }

    state |= DESCRIPTORS64_STATE_USER_SELECTORS_PRESENT;
    state |= DESCRIPTORS64_STATE_SYSCALL_PAIR_READY;

    g_descriptor_state = state;
    g_gdt_token = descriptors64_compute_gdt_token();
    g_tss_token = descriptors64_compute_tss_token();
}

u32 descriptors64_state(void)
{
    return g_descriptor_state;
}

u32 descriptors64_gdt_token(void)
{
    return g_gdt_token;
}

u32 descriptors64_tss_token(void)
{
    return g_tss_token;
}

u32 descriptors64_installed(void)
{
    return (g_descriptor_state & DESCRIPTORS64_STATE_GDT_INSTALLED) != 0u ? 1u : 0u;
}

u32 descriptors64_tss_loaded(void)
{
    return (g_descriptor_state & DESCRIPTORS64_STATE_TSS_LOADED) != 0u ? 1u : 0u;
}

u32 descriptors64_user_selectors_ready(void)
{
    return (g_descriptor_state & DESCRIPTORS64_STATE_USER_SELECTORS_PRESENT) != 0u ? 1u : 0u;
}

u16 descriptors64_kernel_code_selector(void)
{
    return DESCRIPTORS64_KERNEL_CODE_SELECTOR;
}

u16 descriptors64_kernel_data_selector(void)
{
    return DESCRIPTORS64_KERNEL_DATA_SELECTOR;
}

u16 descriptors64_user_code_selector(void)
{
    return DESCRIPTORS64_USER_CODE_SELECTOR;
}

u16 descriptors64_user_data_selector(void)
{
    return DESCRIPTORS64_USER_DATA_SELECTOR;
}

u16 descriptors64_tss_selector(void)
{
    return DESCRIPTORS64_TSS_SELECTOR;
}

u16 descriptors64_sysret_selector_base(void)
{
    return (u16)(DESCRIPTORS64_USER_CODE_SELECTOR - 0x10u);
}

u64 descriptors64_tss_rsp0(void)
{
    return g_tss.rsp0;
}

u64 descriptors64_syscall_star_plan(void)
{
    return ((u64)descriptors64_sysret_selector_base() << 48)
        | ((u64)DESCRIPTORS64_KERNEL_CODE_SELECTOR << 32);
}
