#ifndef LIMITLESS_LINUX_DYNAMIC_X64_H
#define LIMITLESS_LINUX_DYNAMIC_X64_H

#include "elf64_x64.h"
#include "linux_libc_x64.h"
#include "types.h"

#define LINUX_DYNAMIC64_DEFAULT_BASE 0x0000000047800000ull
#define LINUX_DYNAMIC64_IMAGE_BYTES 0x00002000u
#define LINUX_DYNAMIC64_FILE_BYTES 0x00000600u
#define LINUX_DYNAMIC64_TEXT_RVA 0x00001000u
#define LINUX_DYNAMIC64_RODATA_RVA 0x000010A0u
#define LINUX_DYNAMIC64_DYNAMIC_RVA 0x00001120u
#define LINUX_DYNAMIC64_TEXT_FILE_OFFSET 0x00000200u
#define LINUX_DYNAMIC64_RODATA_FILE_OFFSET 0x000002A0u
#define LINUX_DYNAMIC64_DYNAMIC_FILE_OFFSET 0x00000320u
#define LINUX_DYNAMIC64_TEXT_FILE_BYTES 0x000000A0u
#define LINUX_DYNAMIC64_RODATA_FILE_BYTES 0x00000180u
#define LINUX_DYNAMIC64_DYNAMIC_BYTES 0x00000040u
#define LINUX_DYNAMIC64_STACK_BYTES 0x00001000u
#define LINUX_DYNAMIC64_SYMBOL_COUNT 10u
#define LINUX_DYNAMIC64_MAX_NEEDED 8u
#define LINUX_DYNAMIC64_STRING_LIMIT 128u
#define LINUX_DYNAMIC64_INTERP_PATH_MAX 64u

#define LINUX_DYNAMIC64_RVA_DL_START 0x00001000u
#define LINUX_DYNAMIC64_RVA_DL_MAP_OBJECT 0x00001010u
#define LINUX_DYNAMIC64_RVA_DL_BIND_NOW 0x00001020u
#define LINUX_DYNAMIC64_RVA_DL_RUNTIME_RESOLVE 0x00001030u
#define LINUX_DYNAMIC64_RVA_DL_RTLD_LOCK 0x00001040u
#define LINUX_DYNAMIC64_RVA_DL_RTLD_UNLOCK 0x00001050u
#define LINUX_DYNAMIC64_RVA_DLOPEN 0x00001060u
#define LINUX_DYNAMIC64_RVA_DLCLOSE 0x00001070u
#define LINUX_DYNAMIC64_RVA_DLSYM 0x00001080u
#define LINUX_DYNAMIC64_RVA_DLERROR 0x00001090u

#define LINUX_DYNAMIC64_OK 1u
#define LINUX_DYNAMIC64_DENIED 0u

#define LINUX_DYNAMIC64_ERROR_NONE 0u
#define LINUX_DYNAMIC64_ERROR_NULL 1u
#define LINUX_DYNAMIC64_ERROR_PERSONA 2u
#define LINUX_DYNAMIC64_ERROR_BASE 3u
#define LINUX_DYNAMIC64_ERROR_HEADER 4u
#define LINUX_DYNAMIC64_ERROR_PHDR 5u
#define LINUX_DYNAMIC64_ERROR_NOT_DYNAMIC 6u
#define LINUX_DYNAMIC64_ERROR_INTERP 7u
#define LINUX_DYNAMIC64_ERROR_DEPENDENCY 8u
#define LINUX_DYNAMIC64_ERROR_MAP 9u
#define LINUX_DYNAMIC64_ERROR_RELRO 10u
#define LINUX_DYNAMIC64_ERROR_STACK 11u
#define LINUX_DYNAMIC64_ERROR_AUXV 12u
#define LINUX_DYNAMIC64_ERROR_TRANSFER 13u
#define LINUX_DYNAMIC64_ERROR_ALREADY_MAPPED 14u
#define LINUX_DYNAMIC64_ERROR_SYMBOL 15u
#define LINUX_DYNAMIC64_ERROR_UNAVAILABLE 16u

#define LINUX_DYNAMIC64_DT_NULL 0ull
#define LINUX_DYNAMIC64_DT_NEEDED 1ull
#define LINUX_DYNAMIC64_DT_STRTAB 5ull
#define LINUX_DYNAMIC64_DT_STRSZ 10ull

typedef struct linux_dynamic64_load_result
{
    elf64_header_t header;
    elf64_phdr_summary_t phdr_summary;
    elf64_load_result_t load_result;
    u64 image_base;
    u64 image_end;
    u64 dl_start;
    u64 dl_map_object;
    u64 dl_bind_now;
    u64 dl_runtime_resolve;
    u64 dl_rtld_lock;
    u64 dl_rtld_unlock;
    u64 dlopen_fn;
    u64 dlclose_fn;
    u64 dlsym_fn;
    u64 dlerror_fn;
    u32 file_bytes;
    u32 image_bytes;
    u32 symbol_count;
    u32 image_checksum;
    u32 text_checksum;
    u32 rodata_checksum;
    u32 name_checksum;
    u32 text_protection;
    u32 rodata_protection;
    u32 context_stored;
    u32 error;
} linux_dynamic64_load_result_t;

typedef struct linux_dynamic64_needed_result
{
    u32 dynamic_found;
    u32 strtab_found;
    u32 needed_count;
    u32 supported_count;
    u32 missing_count;
    u32 self_needed_count;
    u32 libc_needed_count;
    u32 pthread_needed_count;
    u32 first_needed_checksum;
    u32 last_needed_checksum;
    u32 strtab_file_offset;
    u32 strtab_size;
    u32 error;
} linux_dynamic64_needed_result_t;

typedef struct linux_dynamic64_launch_result
{
    elf64_header_t app_header;
    elf64_phdr_summary_t app_phdr_summary;
    elf64_load_result_t app_load_result;
    elf64_relro_result_t app_relro_result;
    elf64_auxv_t auxv;
    elf64_stack_result_t stack_result;
    linux_dynamic64_needed_result_t needed_result;
    linux_dynamic64_load_result_t interpreter_result;
    linux_libc64_load_result_t libc_result;
    u64 app_load_bias;
    u64 app_entry;
    u64 app_phdr_vaddr;
    u64 interpreter_base;
    u64 stack_base;
    u64 stack_top;
    u64 initial_rsp;
    u64 transfer_rip;
    u64 transfer_rsp;
    u64 app_mapped_bases[ELF64_MAX_PROGRAM_HEADERS];
    u64 app_mapped_lengths[ELF64_MAX_PROGRAM_HEADERS];
    u32 app_mapped_count;
    u32 libc_required;
    u32 libc_mapped;
    u32 pthread_required;
    u32 pthread_mapped;
    u32 interp_path_bytes;
    u32 interp_path_checksum;
    u32 transfer_ready;
    u32 error;
} linux_dynamic64_launch_result_t;

void linux_dynamic64_init(void);
u32 linux_dynamic64_load_interpreter(
    u32 pid,
    u64 image_base,
    linux_dynamic64_load_result_t *out_result);
u32 linux_dynamic64_prepare(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    u64 app_load_bias,
    u64 interpreter_base,
    u64 stack_base,
    u32 stack_bytes,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    linux_dynamic64_launch_result_t *out_result);
u64 linux_dynamic64_export(u32 pid, const char *name);
u32 linux_dynamic64_symbol_supported(const char *name, u32 length);
u32 linux_dynamic64_analyze_needed(
    const u8 *binary_data,
    u32 binary_size,
    const elf64_header_t *header,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    linux_dynamic64_needed_result_t *out_result);
u32 linux_dynamic64_release_launch(u32 pid, const linux_dynamic64_launch_result_t *result);
u32 linux_dynamic64_release_process(u32 pid);
const u8 *linux_dynamic64_interpreter_image(void);
u32 linux_dynamic64_interpreter_file_bytes(void);
u32 linux_dynamic64_symbol_count(void);
u32 linux_dynamic64_load_count(void);
u32 linux_dynamic64_prepare_count(void);
u32 linux_dynamic64_denial_count(void);
u32 linux_dynamic64_dependency_denial_count(void);
u32 linux_dynamic64_last_error(void);
u32 linux_dynamic64_last_needed_count(void);
u32 linux_dynamic64_last_missing_count(void);

#endif
