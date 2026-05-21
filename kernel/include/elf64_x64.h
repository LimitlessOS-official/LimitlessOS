#ifndef LIMITLESS_ELF64_X64_H
#define LIMITLESS_ELF64_X64_H

#include "types.h"

#define ELF64_IDENT_BYTES 16u
#define ELF64_EHDR_BYTES 64u
#define ELF64_PHDR_BYTES 56u
#define ELF64_MAX_PROGRAM_HEADERS 32u
#define ELF64_AUXV_BASE_ENTRIES 18u
#define ELF64_AUXV_LINUX_VDSO_ENTRIES 19u
#define ELF64_AUXV_REQUIRED_ENTRIES ELF64_AUXV_BASE_ENTRIES
#define ELF64_AUXV_MAX_ENTRIES ELF64_AUXV_LINUX_VDSO_ENTRIES
#define ELF64_AUX_RANDOM_BYTES 16u
#define ELF64_AUX_PLATFORM_BYTES 8u
#define ELF64_STACK_MAX_ARGC 8u
#define ELF64_STACK_MAX_ENVC 8u
#define ELF64_STACK_MAX_STRING_BYTES 256u

#define ELF64_EI_CLASS 4u
#define ELF64_EI_DATA 5u
#define ELF64_EI_VERSION 6u
#define ELF64_EI_OSABI 7u

#define ELF64_CLASS_64 2u
#define ELF64_DATA_LSB 1u
#define ELF64_VERSION_CURRENT 1u
#define ELF64_OSABI_NONE 0u
#define ELF64_OSABI_LINUX 3u

#define ELF64_TYPE_EXEC 2u
#define ELF64_TYPE_DYN 3u
#define ELF64_MACHINE_X86_64 62u

#define ELF64_PT_LOAD 1u
#define ELF64_PT_DYNAMIC 2u
#define ELF64_PT_INTERP 3u
#define ELF64_PT_TLS 7u
#define ELF64_PT_GNU_STACK 0x6474E551u
#define ELF64_PT_GNU_RELRO 0x6474E552u

#define ELF64_PF_X 0x00000001u
#define ELF64_PF_W 0x00000002u
#define ELF64_PF_R 0x00000004u

#define ELF64_OK 1u
#define ELF64_DENIED 0u

#define ELF64_ERROR_NONE 0u
#define ELF64_ERROR_NULL 1u
#define ELF64_ERROR_SHORT_HEADER 2u
#define ELF64_ERROR_MAGIC 3u
#define ELF64_ERROR_CLASS 4u
#define ELF64_ERROR_DATA 5u
#define ELF64_ERROR_VERSION 6u
#define ELF64_ERROR_OSABI 7u
#define ELF64_ERROR_TYPE 8u
#define ELF64_ERROR_MACHINE 9u
#define ELF64_ERROR_HEADER_SIZE 10u
#define ELF64_ERROR_PHDR_SIZE 11u
#define ELF64_ERROR_PHDR_RANGE 12u
#define ELF64_ERROR_SECTION_RANGE 13u
#define ELF64_ERROR_PHDR_COUNT 14u
#define ELF64_ERROR_LOAD_RANGE 15u
#define ELF64_ERROR_LOAD_SIZE 16u
#define ELF64_ERROR_LOAD_ALIGN 17u
#define ELF64_ERROR_INTERP_RANGE 18u
#define ELF64_ERROR_OUTPUT_CAPACITY 19u
#define ELF64_ERROR_LOAD_ADDRESS 20u
#define ELF64_ERROR_LOAD_MAP 21u
#define ELF64_ERROR_LOAD_PROTECT 22u
#define ELF64_ERROR_RELRO_RANGE 23u
#define ELF64_ERROR_RELRO_PROTECT 24u
#define ELF64_ERROR_AUX_ARGUMENT 25u
#define ELF64_ERROR_AUX_CAPACITY 26u
#define ELF64_ERROR_STACK_ARGUMENT 27u
#define ELF64_ERROR_STACK_RANGE 28u
#define ELF64_ERROR_STACK_OVERFLOW 29u
#define ELF64_ERROR_STACK_WRITE 30u
#define ELF64_ERROR_LAUNCH_ARGUMENT 31u
#define ELF64_ERROR_LAUNCH_PARSE 32u
#define ELF64_ERROR_LAUNCH_PHDR 33u
#define ELF64_ERROR_LAUNCH_DYNAMIC 34u
#define ELF64_ERROR_LAUNCH_LOAD 35u
#define ELF64_ERROR_LAUNCH_RELRO 36u
#define ELF64_ERROR_LAUNCH_STACK_MAP 37u
#define ELF64_ERROR_LAUNCH_AUXV 38u
#define ELF64_ERROR_LAUNCH_STACK 39u
#define ELF64_ERROR_LAUNCH_TRANSFER 40u
#define ELF64_ERROR_AUX_VDSO_MAP 41u

#define ELF64_STATIC_STACK_BYTES 0x00001000u

#define ELF64_AT_NULL 0ull
#define ELF64_AT_PHDR 3ull
#define ELF64_AT_PHENT 4ull
#define ELF64_AT_PHNUM 5ull
#define ELF64_AT_PAGESZ 6ull
#define ELF64_AT_BASE 7ull
#define ELF64_AT_FLAGS 8ull
#define ELF64_AT_ENTRY 9ull
#define ELF64_AT_UID 11ull
#define ELF64_AT_EUID 12ull
#define ELF64_AT_GID 13ull
#define ELF64_AT_EGID 14ull
#define ELF64_AT_PLATFORM 15ull
#define ELF64_AT_HWCAP 16ull
#define ELF64_AT_CLKTCK 17ull
#define ELF64_AT_SECURE 23ull
#define ELF64_AT_RANDOM 25ull
#define ELF64_AT_HWCAP2 26ull
#define ELF64_AT_SYSINFO_EHDR 33ull

#define ELF64_AUX_HWCAP_BASELINE 0ull
#define ELF64_AUX_HWCAP2_BASELINE 0ull
#define ELF64_AUX_DEFAULT_UID 1000ull
#define ELF64_AUX_DEFAULT_GID 1000ull

typedef struct elf64_header
{
    u16 type;
    u16 machine;
    u32 version;
    u64 entry;
    u64 phoff;
    u64 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
    u32 osabi;
    u32 error;
} elf64_header_t;

typedef struct elf64_program_header
{
    u32 type;
    u32 flags;
    u64 offset;
    u64 vaddr;
    u64 paddr;
    u64 filesz;
    u64 memsz;
    u64 align;
} elf64_program_header_t;

typedef struct elf64_phdr_summary
{
    u32 load_count;
    u32 interp_count;
    u32 gnu_stack_count;
    u32 gnu_relro_count;
    u32 tls_count;
    u32 dynamic_count;
    u32 executable_loads;
    u32 writable_loads;
    u64 load_mem_bytes;
    u64 first_load_vaddr;
    u64 max_load_end;
    u32 error;
} elf64_phdr_summary_t;

typedef struct elf64_load_result
{
    u32 mapped_count;
    u32 reserved;
    u64 total_map_bytes;
    u64 total_file_bytes;
    u64 total_bss_bytes;
    u64 first_mapped_vaddr;
    u64 max_mapped_end;
    u32 source_checksum;
    u32 mapped_checksum;
    u32 bss_nonzero_count;
    u32 error;
} elf64_load_result_t;

typedef struct elf64_relro_result
{
    u32 relro_count;
    u32 protected_count;
    u64 total_protected_bytes;
    u64 first_protected_vaddr;
    u64 max_protected_end;
    u32 error;
} elf64_relro_result_t;

typedef struct elf64_auxv_entry
{
    u64 type;
    u64 value;
} elf64_auxv_entry_t;

typedef struct elf64_auxv
{
    elf64_auxv_entry_t entries[ELF64_AUXV_MAX_ENTRIES];
    u32 entry_count;
    u32 random_byte_count;
    u64 random_staging_address;
    u64 platform_staging_address;
    u8 random[ELF64_AUX_RANDOM_BYTES];
    u8 platform[ELF64_AUX_PLATFORM_BYTES];
    u32 random_checksum;
    u32 platform_checksum;
    u32 error;
} elf64_auxv_t;

typedef struct elf64_stack_result
{
    u64 stack_base;
    u64 stack_top;
    u64 initial_rsp;
    u64 argc_address;
    u64 argv_address;
    u64 envp_address;
    u64 auxv_address;
    u64 strings_base;
    u64 random_address;
    u64 platform_address;
    u32 argc;
    u32 envc;
    u32 auxv_entry_count;
    u32 pointer_slot_count;
    u32 string_bytes;
    u32 layout_bytes;
    u32 random_checksum;
    u32 platform_checksum;
    u32 alignment_ok;
    u32 argv_null_ok;
    u32 envp_null_ok;
    u32 auxv_null_ok;
    u32 error;
} elf64_stack_result_t;

typedef struct elf64_launch_result
{
    elf64_header_t header;
    elf64_phdr_summary_t phdr_summary;
    elf64_load_result_t load_result;
    elf64_relro_result_t relro_result;
    elf64_auxv_t auxv;
    elf64_stack_result_t stack_result;
    u64 load_bias;
    u64 entry_rip;
    u64 phdr_vaddr;
    u64 stack_base;
    u64 stack_top;
    u64 initial_rsp;
    u64 transfer_rip;
    u64 transfer_rsp;
    u32 transfer_selectors;
    u32 transfer_ready;
    u32 transfer_executed;
    u32 transfer_result;
    u32 entry_page_present;
    u32 entry_page_prot;
    u32 stack_page_present;
    u32 stack_page_prot;
    u32 error;
} elf64_launch_result_t;

u32 elf64_parse_header(const u8 *data, u32 size, elf64_header_t *out_header);
u32 elf64_parse_phdrs(
    const u8 *data,
    u32 size,
    const elf64_header_t *header,
    elf64_program_header_t *out_phdrs,
    u32 max_phdrs,
    elf64_phdr_summary_t *out_summary);
u32 elf64_map_load_segments(
    u32 pid,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    const u8 *binary_data,
    u32 binary_size,
    u64 base_offset,
    elf64_load_result_t *out_result);
u32 elf64_apply_gnu_relro(
    u32 pid,
    const elf64_program_header_t *phdrs,
    u32 phdr_count,
    u64 base_offset,
    elf64_relro_result_t *out_result);
u32 elf64_build_auxv(
    u32 pid,
    u64 entry,
    u64 phdr_vaddr,
    u32 phnum,
    u64 interp_base,
    elf64_auxv_t *out_auxv);
u32 elf64_auxv_has_type(const elf64_auxv_t *auxv, u64 type);
u64 elf64_auxv_value(const elf64_auxv_t *auxv, u64 type);
u32 elf64_build_initial_stack(
    u32 pid,
    u64 stack_base,
    u64 stack_top,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    const elf64_auxv_t *auxv,
    elf64_stack_result_t *out_result);
u32 elf64_launch_static(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    u64 load_bias,
    u64 stack_base,
    u64 stack_bytes,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    u32 run_transfer_probe,
    elf64_launch_result_t *out_result);

#endif
