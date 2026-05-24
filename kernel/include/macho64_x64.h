#ifndef LIMITLESS_MACHO64_X64_H
#define LIMITLESS_MACHO64_X64_H

#include "types.h"

#define MACHO64_HEADER_BYTES 32u
#define MACHO64_FAT_HEADER_BYTES 8u
#define MACHO64_FAT_ARCH_BYTES 20u
#define MACHO64_LOAD_COMMAND_MIN_BYTES 8u
#define MACHO64_LC_SEGMENT_64_BYTES 72u
#define MACHO64_LC_MAIN_BYTES 24u
#define MACHO64_DYLIB_COMMAND_BYTES 24u
#define MACHO64_DYLD_INFO_COMMAND_BYTES 48u
#define MACHO64_LINKEDIT_DATA_COMMAND_BYTES 16u
#define MACHO64_SECTION_64_BYTES 80u
#define MACHO64_SEGMENT_NAME_BYTES 16u
#define MACHO64_MAX_LOAD_COMMANDS 64u
#define MACHO64_MAX_FAT_ARCHES 16u
#define MACHO64_MAX_SEGMENTS 8u
#define MACHO64_MAX_DYLIBS 8u
#define MACHO64_MAX_DYLIB_NAME_BYTES 96u
#define MACHO64_MAX_BIND_SYMBOL_BYTES 64u
#define MACHO64_TLS_SELF_POINTER_BYTES 8u
#define MACHO64_TLS_MAX_BLOCK_BYTES 4096u
#define MACHO64_STACK_MAX_ARGC 8u
#define MACHO64_STACK_MAX_ENVC 8u
#define MACHO64_STACK_APPLE_COUNT 2u
#define MACHO64_STACK_MAX_AUX_ENTRIES 8u
#define MACHO64_STACK_MAX_STRING_BYTES 128u
#define MACHO64_STACK_MAX_BUILD_BYTES 4096u
#define MACHO64_STACK_AUX_NULL 0ull
#define MACHO64_STACK_AUX_ENTRY 1ull
#define MACHO64_STACK_AUX_PAGESZ 2ull
#define MACHO64_DEFAULT_STACK_BYTES 0x0000000000800000ull
#define MACHO64_STACK_COMMIT_BYTES 0x0000000000001000ull
#define MACHO64_SHIM_LIBSYSTEM_WRITE_ADDR 0x0000000047301000ull

#define MACHO64_MAGIC_LE64 0xFEEDFACFu
#define MACHO64_FAT_MAGIC 0xCAFEBABEu
#define MACHO64_CPU_TYPE_X86_64 0x01000007u
#define MACHO64_CPU_SUBTYPE_X86_64_ALL 3u
#define MACHO64_FILETYPE_EXECUTE 2u
#define MACHO64_FILETYPE_DYLIB 6u

#define MACHO64_LC_SEGMENT_64 0x00000019u
#define MACHO64_LC_MAIN 0x80000028u
#define MACHO64_LC_LOAD_DYLIB 0x0000000Cu
#define MACHO64_LC_LOAD_WEAK_DYLIB 0x80000018u
#define MACHO64_LC_DYLD_INFO_ONLY 0x80000022u
#define MACHO64_LC_DYLD_EXPORTS_TRIE 0x80000033u

#define MACHO64_PROT_READ 0x1u
#define MACHO64_PROT_WRITE 0x2u
#define MACHO64_PROT_EXECUTE 0x4u

#define MACHO64_SECTION_TYPE_MASK 0x000000FFu
#define MACHO64_SECTION_THREAD_LOCAL_REGULAR 0x00000011u
#define MACHO64_SECTION_THREAD_LOCAL_ZEROFILL 0x00000012u
#define MACHO64_SECTION_THREAD_LOCAL_VARIABLES 0x00000013u

#define MACHO64_REBASE_TYPE_POINTER 1u
#define MACHO64_REBASE_OPCODE_MASK 0xF0u
#define MACHO64_REBASE_IMMEDIATE_MASK 0x0Fu
#define MACHO64_REBASE_OPCODE_DONE 0x00u
#define MACHO64_REBASE_OPCODE_SET_TYPE_IMM 0x10u
#define MACHO64_REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x20u
#define MACHO64_REBASE_OPCODE_ADD_ADDR_ULEB 0x30u
#define MACHO64_REBASE_OPCODE_DO_REBASE_IMM_TIMES 0x50u

#define MACHO64_BIND_TYPE_POINTER 1u
#define MACHO64_BIND_OPCODE_MASK 0xF0u
#define MACHO64_BIND_IMMEDIATE_MASK 0x0Fu
#define MACHO64_BIND_OPCODE_DONE 0x00u
#define MACHO64_BIND_OPCODE_SET_DYLIB_ORDINAL_IMM 0x10u
#define MACHO64_BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM 0x40u
#define MACHO64_BIND_OPCODE_SET_TYPE_IMM 0x50u
#define MACHO64_BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x70u
#define MACHO64_BIND_OPCODE_DO_BIND 0x90u

#define MACHO64_OK 1u
#define MACHO64_DENIED 0u

#define MACHO64_ERROR_NONE 0u
#define MACHO64_ERROR_NULL 1u
#define MACHO64_ERROR_SHORT_HEADER 2u
#define MACHO64_ERROR_MAGIC 3u
#define MACHO64_ERROR_CPU_TYPE 4u
#define MACHO64_ERROR_CPU_SUBTYPE 5u
#define MACHO64_ERROR_FILETYPE 6u
#define MACHO64_ERROR_LOAD_COMMAND_COUNT 7u
#define MACHO64_ERROR_LOAD_COMMAND_SIZE 8u
#define MACHO64_ERROR_LOAD_COMMAND_RANGE 9u
#define MACHO64_ERROR_LOAD_COMMAND_ALIGN 10u
#define MACHO64_ERROR_FAT_SHORT_HEADER 11u
#define MACHO64_ERROR_FAT_MAGIC 12u
#define MACHO64_ERROR_FAT_ARCH_COUNT 13u
#define MACHO64_ERROR_FAT_ARCH_TABLE 14u
#define MACHO64_ERROR_FAT_SLICE_RANGE 15u
#define MACHO64_ERROR_FAT_NO_X86_64 16u
#define MACHO64_ERROR_COMMAND_RANGE 17u
#define MACHO64_ERROR_COMMAND_SIZE 18u
#define MACHO64_ERROR_SEGMENT_COUNT 19u
#define MACHO64_ERROR_SEGMENT_SIZE 20u
#define MACHO64_ERROR_SEGMENT_RANGE 21u
#define MACHO64_ERROR_SEGMENT_ADDRESS 22u
#define MACHO64_ERROR_SEGMENT_PROTECT 23u
#define MACHO64_ERROR_SEGMENT_MAP 24u
#define MACHO64_ERROR_MAIN_COUNT 25u
#define MACHO64_ERROR_MAIN_SIZE 26u
#define MACHO64_ERROR_MAIN_MISSING 27u
#define MACHO64_ERROR_MAIN_RANGE 28u
#define MACHO64_ERROR_MAIN_STACK 29u
#define MACHO64_ERROR_MAIN_TEXT 30u
#define MACHO64_ERROR_DYLIB_COUNT 31u
#define MACHO64_ERROR_DYLIB_SIZE 32u
#define MACHO64_ERROR_DYLIB_NAME 33u
#define MACHO64_ERROR_DYLIB_REQUIRED 34u
#define MACHO64_ERROR_DYLD_INFO_MISSING 35u
#define MACHO64_ERROR_DYLD_INFO_SIZE 36u
#define MACHO64_ERROR_DYLD_INFO_RANGE 37u
#define MACHO64_ERROR_DYLD_REBASE_OPCODE 38u
#define MACHO64_ERROR_DYLD_BIND_OPCODE 39u
#define MACHO64_ERROR_DYLD_SEGMENT 40u
#define MACHO64_ERROR_DYLD_POINTER 41u
#define MACHO64_ERROR_DYLD_SYMBOL 42u
#define MACHO64_ERROR_DYLD_DYLIB 43u
#define MACHO64_ERROR_TLS_PERSONA 44u
#define MACHO64_ERROR_TLS_SECTION 45u
#define MACHO64_ERROR_TLS_ADDRESS 46u
#define MACHO64_ERROR_TLS_MAP 47u
#define MACHO64_ERROR_TLS_SIZE 48u
#define MACHO64_ERROR_TLS_SOURCE 49u
#define MACHO64_ERROR_STACK_PERSONA 50u
#define MACHO64_ERROR_STACK_ARGUMENT 51u
#define MACHO64_ERROR_STACK_RANGE 52u
#define MACHO64_ERROR_STACK_OVERFLOW 53u
#define MACHO64_ERROR_STACK_WRITE 54u

#define MACHO64_SHIM_NONE 0u
#define MACHO64_SHIM_LIBSYSTEM 1u
#define MACHO64_SHIM_LIBDYLD 2u
#define MACHO64_SHIM_COREFOUNDATION 3u

typedef struct macho64_header
{
    u32 magic;
    u32 cpu_type;
    u32 cpu_subtype;
    u32 filetype;
    u32 ncmds;
    u32 sizeofcmds;
    u32 flags;
    u32 reserved;
    u32 load_command_offset;
    u32 load_command_end;
    u32 error;
} macho64_header_t;

typedef struct macho64_fat_slice
{
    u32 magic;
    u32 arch_count;
    u32 selected_index;
    u32 cpu_type;
    u32 cpu_subtype;
    u32 offset;
    u32 size;
    u32 align;
    u32 error;
} macho64_fat_slice_t;

typedef struct macho64_segment_map_result
{
    u32 segment_count;
    u32 mapped_count;
    u32 text_mapped;
    u32 data_mapped;
    u32 linkedit_mapped;
    u32 text_prot;
    u32 data_prot;
    u32 linkedit_prot;
    u64 total_map_bytes;
    u64 total_file_bytes;
    u64 total_bss_bytes;
    u64 first_mapped_vaddr;
    u64 max_mapped_end;
    u32 source_checksum;
    u32 mapped_checksum;
    u32 bss_nonzero_count;
    u32 error;
} macho64_segment_map_result_t;

typedef struct macho64_main_result
{
    u32 main_count;
    u32 text_found;
    u32 stack_defaulted;
    u32 stack_mapped;
    u32 entry_within_text;
    u32 entry_page_present;
    u32 entry_page_prot;
    u32 stack_page_present;
    u32 stack_page_prot;
    u32 error;
    u64 entryoff;
    u64 stack_size;
    u64 stack_mapped_bytes;
    u64 text_vmaddr;
    u64 text_vmsize;
    u64 entry_rip;
    u64 stack_base;
    u64 stack_top;
    u64 initial_rsp;
} macho64_main_result_t;

typedef struct macho64_dylib_dependency
{
    u32 present;
    u32 weak;
    u32 shim_found;
    u32 shim_id;
    u32 name_offset;
    u32 path_length;
    u32 path_checksum;
    u32 timestamp;
    u32 current_version;
    u32 compatibility_version;
    char path[MACHO64_MAX_DYLIB_NAME_BYTES];
} macho64_dylib_dependency_t;

typedef struct macho64_dylib_result
{
    u32 load_command_count;
    u32 weak_command_count;
    u32 recorded_count;
    u32 shim_found_count;
    u32 weak_absent_count;
    u32 required_missing_count;
    u32 first_shim_id;
    u32 first_path_checksum;
    u32 error;
    macho64_dylib_dependency_t dependencies[MACHO64_MAX_DYLIBS];
} macho64_dylib_result_t;

typedef struct macho64_dyld_info_result
{
    u32 dyld_info_found;
    u32 exports_trie_found;
    u32 rebase_count;
    u32 bind_count;
    u32 rebase_type;
    u32 bind_type;
    u32 bind_ordinal;
    u32 bind_shim_id;
    u32 bind_symbol_length;
    u32 bind_symbol_checksum;
    u32 error;
    u64 rebase_target;
    u64 rebase_before;
    u64 rebase_after;
    u64 bind_target;
    u64 bind_value;
    u32 rebase_off;
    u32 rebase_size;
    u32 bind_off;
    u32 bind_size;
    u32 exports_trie_off;
    u32 exports_trie_size;
} macho64_dyld_info_result_t;

typedef struct macho64_tls_result
{
    u32 section_count;
    u32 variables_count;
    u32 regular_count;
    u32 zerofill_count;
    u32 error;
    u64 variables_addr;
    u64 variables_bytes;
    u64 regular_addr;
    u64 regular_bytes;
    u64 zerofill_addr;
    u64 zerofill_bytes;
    u64 tls_block_base;
    u64 tls_block_bytes;
    u64 tls_template_base;
    u64 tls_template_bytes;
    u64 gs_base_before;
    u64 gs_base_after;
    u64 gs_zero_value;
    u32 template_checksum;
    u32 block_checksum;
    u32 zero_nonzero_count;
    u32 first_template_word;
    u32 page_present;
    u32 page_protection;
    u32 context_stored;
} macho64_tls_result_t;

typedef struct macho64_stack_aux_entry
{
    u64 type;
    u64 value;
} macho64_stack_aux_entry_t;

typedef struct macho64_stack_result
{
    u32 error;
    u32 argc;
    u32 envc;
    u32 apple_count;
    u32 aux_entry_count;
    u32 pointer_slot_count;
    u32 string_bytes;
    u32 layout_bytes;
    u32 alignment_ok;
    u32 argv_null_ok;
    u32 envp_null_ok;
    u32 apple_null_ok;
    u32 aux_null_ok;
    u32 stack_page_present;
    u32 stack_page_protection;
    u32 exec_path_checksum;
    u32 persona_string_checksum;
    u32 stack_checksum;
    u64 stack_base;
    u64 stack_top;
    u64 initial_rsp;
    u64 argc_address;
    u64 argv_address;
    u64 envp_address;
    u64 apple_address;
    u64 auxv_address;
    u64 strings_base;
    u64 argv0_address;
    u64 env0_address;
    u64 apple_exec_path_address;
    u64 apple_persona_address;
    u64 first_aux_type;
    u64 first_aux_value;
} macho64_stack_result_t;

u32 macho64_parse_header(const u8 *data, u32 size, macho64_header_t *out_header);
u32 macho64_slice_fat_x86_64(const u8 *data, u32 size, macho64_fat_slice_t *out_slice);
u32 macho64_map_segments(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    u64 base_offset,
    macho64_segment_map_result_t *out_result);
u32 macho64_prepare_main_entry(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    u64 base_offset,
    u64 stack_top,
    macho64_main_result_t *out_result);
u32 macho64_walk_dylib_dependencies(
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    macho64_dylib_result_t *out_result);
u32 macho64_apply_dyld_fixups(
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    const macho64_dylib_result_t *dylibs,
    u64 slide,
    macho64_dyld_info_result_t *out_result);
u32 macho64_setup_tls(
    u32 pid,
    const u8 *binary_data,
    u32 binary_size,
    const macho64_header_t *header,
    u64 tls_block_base,
    macho64_tls_result_t *out_result);
u32 macho64_build_initial_stack(
    u32 pid,
    u64 stack_base,
    u64 stack_top,
    u32 argc,
    const char *const *argv,
    u32 envc,
    const char *const *envp,
    const char *exec_path,
    const macho64_stack_aux_entry_t *auxv,
    macho64_stack_result_t *out_result);

#endif
