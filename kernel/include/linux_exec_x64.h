#ifndef LIMITLESS_LINUX_EXEC_X64_H
#define LIMITLESS_LINUX_EXEC_X64_H

#include "capability_x64.h"
#include "types.h"

#define LINUX_EXEC64_REAL_BINARY_MAX_BYTES 0x00200000u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define LINUX_EXEC64_NVME_MAX_FILE_BYTES 0x00400000u
#define LINUX_EXEC64_STAGING_BUFFER_BYTES LINUX_EXEC64_NVME_MAX_FILE_BYTES
#define LINUX_EXEC64_LOW_KERNEL_VMA_LIMIT 0x0000000001000000ull
#endif
#define LINUX_EXEC64_REAL_STACK_BASE 0x0000000053000000ull
#define LINUX_EXEC64_REAL_STACK_BYTES 0x00010000u
#define LINUX_EXEC64_ARG_MAX 8u
#define LINUX_EXEC64_EXIT_PROBE_RESULT 0x52424E31u

#define LINUX_EXEC64_RESULT_OK 1u
#define LINUX_EXEC64_RESULT_FAILED 0u

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
u32 linux_exec64_run_nvme(
    const u8 *path,
    u32 path_bytes,
    const char *const *argv,
    u32 argc,
    u32 owner_id,
    u32 nvme_fs_capability,
    u32 console_capability);
#else
static inline u32 linux_exec64_run_nvme(
    const u8 *path,
    u32 path_bytes,
    const char *const *argv,
    u32 argc,
    u32 owner_id,
    u32 nvme_fs_capability,
    u32 console_capability)
{
    (void)path;
    (void)path_bytes;
    (void)argv;
    (void)argc;
    (void)owner_id;
    (void)nvme_fs_capability;
    (void)console_capability;
    return LINUX_EXEC64_RESULT_FAILED;
}
#endif

#endif
