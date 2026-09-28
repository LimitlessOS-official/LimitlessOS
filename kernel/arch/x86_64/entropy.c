#include "entropy_x64.h"

#include "pit.h"

#define ENTROPY64_HARDWARE_UNKNOWN 0xFFFFFFFFu

static u64 g_entropy64_pool[2] = { 0x6A09E667F3BCC909ull, 0xBB67AE8584CAA73Bull };
static u64 g_entropy64_counter = 0ull;
static u32 g_entropy64_hardware = ENTROPY64_HARDWARE_UNKNOWN;

static u64 entropy64_rdtsc(void)
{
    u32 low;
    u32 high;

    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((u64)high << 32) | (u64)low;
}

static u64 entropy64_mix64(u64 value)
{
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

u32 entropy64_hardware_available(void)
{
    u32 eax = 1u;
    u32 ebx;
    u32 ecx = 0u;
    u32 edx;

    if (g_entropy64_hardware == ENTROPY64_HARDWARE_UNKNOWN)
    {
        __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
        g_entropy64_hardware = ((ecx & (1u << 30)) != 0u) ? 1u : 0u;
    }
    return g_entropy64_hardware;
}

static u64 entropy64_rdrand(void)
{
    u64 value = 0ull;
    u32 attempt;
    u8 ok;

    if (entropy64_hardware_available() == 0u)
    {
        return 0ull;
    }
    for (attempt = 0u; attempt < 10u; ++attempt)
    {
        __asm__ volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(ok) : : "cc");
        if (ok != 0u)
        {
            return value;
        }
    }
    return 0ull;
}

void entropy64_stir(u64 sample)
{
    g_entropy64_pool[0] = entropy64_mix64(g_entropy64_pool[0] ^ sample ^ entropy64_rdtsc());
    g_entropy64_pool[1] = entropy64_mix64(g_entropy64_pool[1] + g_entropy64_pool[0] + (u64)pit_get_ticks());
}

u64 entropy64_next_u64(void)
{
    entropy64_stir(entropy64_rdrand());
    ++g_entropy64_counter;
    return entropy64_mix64(g_entropy64_pool[0] ^ entropy64_mix64(g_entropy64_pool[1] + g_entropy64_counter));
}

void entropy64_fill(u8 *output, u32 byte_count)
{
    u64 word = 0ull;
    u32 index;

    if (output == (u8 *)0)
    {
        return;
    }
    for (index = 0u; index < byte_count; ++index)
    {
        if ((index & 7u) == 0u)
        {
            word = entropy64_next_u64();
        }
        output[index] = (u8)(word >> ((index & 7u) * 8u));
    }
}
