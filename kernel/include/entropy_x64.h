#ifndef LIMITLESS_ENTROPY_X64_H
#define LIMITLESS_ENTROPY_X64_H

#include "types.h"

/*
 * Kernel entropy pool (UEFI kernel). It mixes RDRAND when the CPU has it with
 * the TSC and PIT tick count, and callers stir in unpredictable events such
 * as keystroke timing. Output is suitable for salts, IDs, and getrandom; it is
 * only as strong as RDRAND on CPUs without a hardware generator.
 */
void entropy64_stir(u64 sample);
u64 entropy64_next_u64(void);
void entropy64_fill(u8 *output, u32 byte_count);
u32 entropy64_hardware_available(void);

#endif
