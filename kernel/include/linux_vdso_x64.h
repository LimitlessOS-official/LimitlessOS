#ifndef LIMITLESS_LINUX_VDSO_X64_H
#define LIMITLESS_LINUX_VDSO_X64_H

#include "linux_abi_x64.h"
#include "types.h"

#define LINUX_VDSO64_BASE 0x00000000441C0000ull
#define LINUX_VDSO64_PAGE_BYTES 0x00001000u
#define LINUX_VDSO64_ELF_EHDR_BYTES 64u
#define LINUX_VDSO64_ELF_PHDR_BYTES 56u
#define LINUX_VDSO64_PHDR_COUNT 2u

#define LINUX_VDSO64_FUNC_CLOCK_GETTIME 0x00000180u
#define LINUX_VDSO64_FUNC_GETTIMEOFDAY 0x000001A0u
#define LINUX_VDSO64_FUNC_TIME 0x000001C0u
#define LINUX_VDSO64_NAME_TABLE_OFFSET 0x00000200u

#define LINUX_VDSO64_MAP_OK 1u
#define LINUX_VDSO64_MAP_DENIED 0u

typedef struct linux_vdso64_info
{
    u64 base;
    u32 size;
    u32 mapped;
    u32 page_present;
    u32 page_prot;
    u32 elf_magic;
    u32 elf_class;
    u32 elf_data;
    u32 elf_type;
    u32 elf_machine;
    u32 phdr_count;
    u32 has_load;
    u32 has_dynamic;
    u32 has_clock_gettime;
    u32 has_gettimeofday;
    u32 has_time;
    u32 image_checksum;
    u32 duplicate_denied;
    u32 invalid_pid_denied;
} linux_vdso64_info_t;

void linux_vdso64_init(void);
u32 linux_vdso64_map(u32 pid, linux_vdso64_info_t *out_info);
u32 linux_vdso64_validate(u32 pid, linux_vdso64_info_t *out_info);
u32 linux_vdso64_unmap(u32 pid);
u64 linux_vdso64_clock_gettime_fast(u32 pid, u64 clock_id, u64 user_timespec);
u32 linux_vdso64_map_count(void);
u32 linux_vdso64_fast_clock_count(void);
u32 linux_vdso64_fast_clock_fault_count(void);
u32 linux_vdso64_fast_clock_denial_count(void);

#endif
