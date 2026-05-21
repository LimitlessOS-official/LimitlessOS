#ifndef LIMITLESS_PE64_X64_H
#define LIMITLESS_PE64_X64_H

#include "types.h"

#define PE64_DOS_HEADER_MIN_BYTES 64u
#define PE64_DOS_LFANEW_OFFSET 0x3Cu
#define PE64_NT_SIGNATURE_BYTES 4u
#define PE64_COFF_HEADER_BYTES 20u
#define PE64_OPTIONAL_HEADER_PE32_PLUS_BYTES 240u
#define PE64_SECTION_NAME_BYTES 8u
#define PE64_SECTION_HEADER_BYTES 40u
#define PE64_MAX_SECTIONS 16u
#define PE64_RELOC_BLOCK_HEADER_BYTES 8u
#define PE64_DATA_DIRECTORY_BYTES 8u
#define PE64_DIRECTORY_IMPORT_INDEX 1u
#define PE64_DIRECTORY_EXCEPTION_INDEX 3u
#define PE64_DIRECTORY_TLS_INDEX 9u
#define PE64_DIRECTORY_LOAD_CONFIG_INDEX 10u
#define PE64_IMPORT_DESCRIPTOR_BYTES 20u
#define PE64_IMPORT_NAME_MAX_BYTES 128u
#define PE64_IMPORT_DESCRIPTOR_MAX_COUNT 16u
#define PE64_IMPORT_THUNK_MAX_COUNT 64u
#define PE64_RUNTIME_FUNCTION_BYTES 12u
#define PE64_EXCEPTION_MAX_FUNCTIONS 64u
#define PE64_TLS_DIRECTORY64_BYTES 40u
#define PE64_TLS_CALLBACK_MAX_COUNT 16u
#define PE64_TLS_MAX_BLOCK_BYTES 4096u
#define PE64_TLS_REASON_DLL_PROCESS_ATTACH 1u
#define PE64_LOAD_CONFIG_SECURITY_COOKIE_OFFSET 0x58u
#define PE64_LOAD_CONFIG_SECURITY_COOKIE_MIN_BYTES 0x60u
#define PE64_ENTRY_STACK_BYTES 0x00001000u
#define PE64_ENTRY_SHADOW_SPACE_BYTES 0x20u
#define PE64_ENTRY_PROBE_RESULT 0x50453132u
#define PE64_ENTRY_PROBE_AUX_MATCH 0x00000007u
#define PE64_TEB_DEFAULT_BASE 0x0000000044240000ull
#define PE64_TEB_PAGE_BYTES 4096u
#define PE64_TEB_OFFSET_EXCEPTION_LIST 0x000u
#define PE64_TEB_OFFSET_STACK_BASE 0x008u
#define PE64_TEB_OFFSET_STACK_LIMIT 0x010u
#define PE64_TEB_OFFSET_SELF 0x030u
#define PE64_TEB_OFFSET_TLS_POINTER 0x058u
#define PE64_TEB_OFFSET_PEB 0x060u
#define PE64_TEB_TLS_POINTER_OFFSET 0x700u
#define PE64_TEB_PEB_OFFSET 0x800u
#define PE64_TEB_PROCESS_PARAMETERS_OFFSET 0xA00u
#define PE64_TEB_IMAGE_PATH_BUFFER_OFFSET 0xB00u
#define PE64_TEB_COMMAND_LINE_BUFFER_OFFSET 0xC00u
#define PE64_TEB_ENVIRONMENT_BUFFER_OFFSET 0xD00u
#define PE64_TEB_CHAIN_END 0xFFFFFFFFFFFFFFFFull
#define PE64_PEB_OFFSET_IMAGE_BASE_ADDRESS 0x010u
#define PE64_PEB_OFFSET_PROCESS_PARAMETERS 0x020u
#define PE64_PEB_OFFSET_NT_GLOBAL_FLAG 0x068u
#define PE64_PEB_OFFSET_OS_MAJOR 0x118u
#define PE64_PEB_OFFSET_OS_MINOR 0x11Cu
#define PE64_PEB_OFFSET_OS_BUILD 0x120u
#define PE64_PEB_OS_MAJOR 10u
#define PE64_PEB_OS_MINOR 0u
#define PE64_PEB_OS_BUILD 22621u
#define PE64_PROCESS_PARAMETERS_UNICODE_STRING_BYTES 16u
#define PE64_PROCESS_PARAMETERS_OFFSET_IMAGE_PATH_NAME 0x040u
#define PE64_PROCESS_PARAMETERS_OFFSET_COMMAND_LINE 0x050u
#define PE64_PROCESS_PARAMETERS_OFFSET_ENVIRONMENT 0x060u
#define PE64_PROCESS_PARAMETERS_STRING_MAX_BYTES 0x100u
#define PE64_KUSER_SHARED_DATA_BASE 0x000000007FFE0000ull
#define PE64_KUSER_SHARED_DATA_BYTES 4096u
#define PE64_KUSER_OFFSET_SYSTEM_TIME_HIGH1 0x010u
#define PE64_KUSER_OFFSET_SYSTEM_TIME_LOW 0x014u
#define PE64_KUSER_OFFSET_SYSTEM_TIME_HIGH2 0x018u
#define PE64_KUSER_OFFSET_NT_SYSTEM_ROOT 0x030u
#define PE64_KUSER_OFFSET_NT_PRODUCT_TYPE 0x264u
#define PE64_KUSER_OFFSET_PROCESSOR_FEATURES 0x274u
#define PE64_KUSER_PROCESSOR_FEATURE_BYTES 64u
#define PE64_KUSER_OFFSET_TICK_COUNT_LOW 0x320u
#define PE64_KUSER_OFFSET_TICK_COUNT_HIGH 0x324u
#define PE64_KUSER_SYSTEM_ROOT_MAX_BYTES 0x208u
#define PE64_KUSER_NT_PRODUCT_WORKSTATION 1u

#define PE64_DOS_MAGIC_MZ 0x5A4Du
#define PE64_NT_SIGNATURE 0x00004550u
#define PE64_MACHINE_AMD64 0x8664u
#define PE64_OPTIONAL_MAGIC_PE32_PLUS 0x020Bu
#define PE64_IMAGE_FILE_DLL 0x2000u
#define PE64_IMAGE_REL_BASED_ABSOLUTE 0u
#define PE64_IMAGE_REL_BASED_DIR64 10u
#define PE64_IMPORT_ORDINAL_FLAG64 0x8000000000000000ull
#define PE64_IMPORT_ORDINAL_MASK 0x000000000000FFFFull

#define PE64_SCN_CNT_CODE 0x00000020u
#define PE64_SCN_CNT_INITIALIZED_DATA 0x00000040u
#define PE64_SCN_CNT_UNINITIALIZED_DATA 0x00000080u
#define PE64_SCN_MEM_EXECUTE 0x20000000u
#define PE64_SCN_MEM_READ 0x40000000u
#define PE64_SCN_MEM_WRITE 0x80000000u

#define PE64_SECTION_PROT_READ 0x00000001u
#define PE64_SECTION_PROT_WRITE 0x00000002u
#define PE64_SECTION_PROT_EXECUTE 0x00000004u

#define PE64_OK 1u
#define PE64_DENIED 0u

#define PE64_ERROR_NONE 0u
#define PE64_ERROR_NULL 1u
#define PE64_ERROR_SHORT_DOS 2u
#define PE64_ERROR_DOS_MAGIC 3u
#define PE64_ERROR_LFANEW_RANGE 4u
#define PE64_ERROR_SIGNATURE 5u
#define PE64_ERROR_MACHINE 6u
#define PE64_ERROR_OPTIONAL_SIZE 7u
#define PE64_ERROR_OPTIONAL_MAGIC 8u
#define PE64_ERROR_SECTION_COUNT 9u
#define PE64_ERROR_SECTION_TABLE_RANGE 10u
#define PE64_ERROR_ALIGNMENT 11u
#define PE64_ERROR_IMAGE_SIZE 12u
#define PE64_ERROR_HEADER_SIZE 13u
#define PE64_ERROR_ENTRY_RANGE 14u
#define PE64_ERROR_OUTPUT_CAPACITY 15u
#define PE64_ERROR_SECTION_SIZE 16u
#define PE64_ERROR_SECTION_RAW_RANGE 17u
#define PE64_ERROR_SECTION_ADDRESS 18u
#define PE64_ERROR_SECTION_PROTECT 19u
#define PE64_ERROR_SECTION_MAP 20u
#define PE64_ERROR_RELOC_SECTION 21u
#define PE64_ERROR_RELOC_BLOCK_RANGE 22u
#define PE64_ERROR_RELOC_BLOCK_SIZE 23u
#define PE64_ERROR_RELOC_TYPE 24u
#define PE64_ERROR_RELOC_TARGET_RANGE 25u
#define PE64_ERROR_RELOC_TARGET_MAP 26u
#define PE64_ERROR_IMPORT_DIRECTORY 27u
#define PE64_ERROR_IMPORT_DESCRIPTOR_RANGE 28u
#define PE64_ERROR_IMPORT_DLL_NAME 29u
#define PE64_ERROR_IMPORT_THUNK_RANGE 30u
#define PE64_ERROR_IMPORT_SYMBOL_NAME 31u
#define PE64_ERROR_IMPORT_LIBRARY 32u
#define PE64_ERROR_IMPORT_SYMBOL 33u
#define PE64_ERROR_IMPORT_IAT_MAP 34u
#define PE64_ERROR_IMPORT_WRITE 35u
#define PE64_ERROR_TLS_DIRECTORY 36u
#define PE64_ERROR_TLS_RAW_RANGE 37u
#define PE64_ERROR_TLS_BLOCK_MAP 38u
#define PE64_ERROR_TLS_INDEX_MAP 39u
#define PE64_ERROR_TLS_CALLBACK_TABLE 40u
#define PE64_ERROR_TLS_CALLBACK_TARGET 41u
#define PE64_ERROR_TLS_CALLBACK_DISPATCH 42u
#define PE64_ERROR_TLS_SIZE 43u
#define PE64_ERROR_EXCEPTION_DIRECTORY 44u
#define PE64_ERROR_EXCEPTION_ENTRY_RANGE 45u
#define PE64_ERROR_EXCEPTION_UNWIND_RANGE 46u
#define PE64_ERROR_EXCEPTION_PERSONA 47u
#define PE64_ERROR_EXCEPTION_SIZE 48u
#define PE64_ERROR_TEB_PERSONA 49u
#define PE64_ERROR_TEB_ADDRESS 50u
#define PE64_ERROR_TEB_STACK 51u
#define PE64_ERROR_TEB_MAP 52u
#define PE64_ERROR_PEB_PERSONA 53u
#define PE64_ERROR_PEB_TEB 54u
#define PE64_ERROR_PEB_IMAGE_BASE 55u
#define PE64_ERROR_PEB_STRING 56u
#define PE64_ERROR_PEB_MAP 57u
#define PE64_ERROR_KUSER_PERSONA 58u
#define PE64_ERROR_KUSER_MAP 59u
#define PE64_ERROR_KUSER_PROTECT 60u
#define PE64_ERROR_KUSER_TIME 61u
#define PE64_ERROR_KUSER_STRING 62u
#define PE64_ERROR_SECURITY_COOKIE_PERSONA 63u
#define PE64_ERROR_LOAD_CONFIG_DIRECTORY 64u
#define PE64_ERROR_SECURITY_COOKIE_FIELD 65u
#define PE64_ERROR_SECURITY_COOKIE_MAP 66u
#define PE64_ERROR_SECURITY_COOKIE_WRITE 67u
#define PE64_ERROR_ENTRY_PERSONA 68u
#define PE64_ERROR_ENTRY_ADDRESS 69u
#define PE64_ERROR_ENTRY_STACK 70u
#define PE64_ERROR_ENTRY_TRANSFER 71u
#define PE64_ERROR_ENTRY_NTDLL_UNAVAILABLE 72u

typedef struct pe64_header
{
    u32 pe_offset;
    u16 machine;
    u16 number_of_sections;
    u32 time_date_stamp;
    u32 pointer_to_symbol_table;
    u32 number_of_symbols;
    u16 size_of_optional_header;
    u16 characteristics;
    u16 optional_magic;
    u32 address_of_entry_point;
    u64 image_base;
    u32 section_alignment;
    u32 file_alignment;
    u32 size_of_image;
    u32 size_of_headers;
    u16 subsystem;
    u16 dll_characteristics;
    u64 size_of_stack_reserve;
    u64 size_of_stack_commit;
    u64 size_of_heap_reserve;
    u64 size_of_heap_commit;
    u32 number_of_rva_and_sizes;
    u32 import_directory_rva;
    u32 import_directory_size;
    u32 exception_directory_rva;
    u32 exception_directory_size;
    u32 tls_directory_rva;
    u32 tls_directory_size;
    u32 load_config_directory_rva;
    u32 load_config_directory_size;
    u32 error;
} pe64_header_t;

typedef struct pe64_section
{
    u8 name[PE64_SECTION_NAME_BYTES];
    u32 virtual_size;
    u32 virtual_address;
    u32 size_of_raw_data;
    u32 pointer_to_raw_data;
    u32 pointer_to_relocations;
    u32 pointer_to_linenumbers;
    u16 number_of_relocations;
    u16 number_of_linenumbers;
    u32 characteristics;
    u32 prot_flags;
} pe64_section_t;

typedef struct pe64_section_summary
{
    u32 section_count;
    u32 readable_count;
    u32 writable_count;
    u32 executable_count;
    u32 code_count;
    u32 initialized_data_count;
    u32 uninitialized_data_count;
    u64 total_virtual_bytes;
    u64 total_raw_bytes;
    u32 first_virtual_address;
    u32 max_virtual_end;
    u32 name_checksum;
    u32 error;
} pe64_section_summary_t;

typedef struct pe64_map_result
{
    u32 mapped_count;
    u32 section_count;
    u64 actual_base;
    u64 total_map_bytes;
    u64 total_file_bytes;
    u64 total_bss_bytes;
    u64 first_mapped_vaddr;
    u64 max_mapped_end;
    u32 source_checksum;
    u32 mapped_checksum;
    u32 bss_nonzero_count;
    u32 error;
} pe64_map_result_t;

typedef struct pe64_reloc_result
{
    u32 block_count;
    u32 entry_count;
    u32 applied_count;
    u32 skipped_count;
    u64 preferred_base;
    u64 actual_base;
    u64 delta;
    u32 reloc_section_rva;
    u32 reloc_section_bytes;
    u32 first_fixup_rva;
    u32 last_fixup_rva;
    u32 before_checksum;
    u32 after_checksum;
    u32 error;
} pe64_reloc_result_t;

typedef struct pe64_shim_symbol
{
    const char *name;
    u16 ordinal;
    u64 address;
} pe64_shim_symbol_t;

typedef struct pe64_shim_library
{
    const char *dll_name;
    const pe64_shim_symbol_t *symbols;
    u32 symbol_count;
} pe64_shim_library_t;

typedef struct pe64_shim_registry
{
    const pe64_shim_library_t *libraries;
    u32 library_count;
} pe64_shim_registry_t;

typedef struct pe64_import_result
{
    u32 descriptor_count;
    u32 thunk_count;
    u32 name_import_count;
    u32 ordinal_import_count;
    u32 resolved_count;
    u32 import_directory_rva;
    u32 import_directory_bytes;
    u32 dll_name_rva;
    u32 first_thunk_rva;
    u32 last_thunk_rva;
    u64 first_function;
    u64 last_function;
    u32 dll_checksum;
    u32 symbol_checksum;
    u32 iat_checksum;
    u32 error;
} pe64_import_result_t;

typedef u32 (*pe64_tls_callback_dispatch_t)(
    u32 pid,
    u64 callback_va,
    u64 image_base,
    u32 reason,
    u64 reserved,
    void *context);

typedef struct pe64_tls_result
{
    u32 tls_directory_rva;
    u32 tls_directory_bytes;
    u64 raw_start_va;
    u64 raw_end_va;
    u64 index_va;
    u64 callbacks_va;
    u64 tls_block_base;
    u64 tls_block_bytes;
    u32 template_bytes;
    u32 zero_fill_bytes;
    u32 index_value;
    u32 index_written;
    u32 callback_count;
    u32 invoked_count;
    u64 first_callback;
    u64 last_callback;
    u32 template_checksum;
    u32 block_checksum;
    u32 callback_checksum;
    u32 zero_nonzero_count;
    u32 error;
} pe64_tls_result_t;

typedef struct pe64_exception_result
{
    u32 exception_directory_rva;
    u32 exception_directory_bytes;
    u32 function_count;
    u32 registered_count;
    u64 table_base;
    u64 table_bytes;
    u32 first_begin_rva;
    u32 last_end_rva;
    u32 first_unwind_rva;
    u32 last_unwind_rva;
    u32 table_checksum;
    u32 persona_stored;
    u32 error;
} pe64_exception_result_t;

typedef struct pe64_teb_result
{
    u64 teb_base;
    u64 peb_base;
    u64 stack_base;
    u64 stack_limit;
    u64 tls_pointer;
    u64 gs_base_before;
    u64 gs_base_after;
    u64 exception_list_value;
    u64 stack_base_value;
    u64 stack_limit_value;
    u64 self_value;
    u64 tls_pointer_value;
    u64 peb_value;
    u64 page_bytes;
    u32 page_protection;
    u32 page_checksum;
    u32 context_stored;
    u32 error;
} pe64_teb_result_t;

typedef struct pe64_peb_result
{
    u64 peb_base;
    u64 process_parameters;
    u64 image_base;
    u64 image_base_value;
    u64 process_parameters_value;
    u64 image_path_buffer;
    u64 command_line_buffer;
    u64 environment_buffer;
    u32 os_major;
    u32 os_minor;
    u32 os_build;
    u32 nt_global_flag;
    u32 os_major_value;
    u32 os_minor_value;
    u32 os_build_value;
    u32 nt_global_flag_value;
    u32 image_path_bytes;
    u32 command_line_bytes;
    u32 environment_bytes;
    u32 image_path_checksum;
    u32 command_line_checksum;
    u32 environment_checksum;
    u32 context_stored;
    u32 error;
} pe64_peb_result_t;

typedef struct pe64_kuser_result
{
    u64 base;
    u64 page_bytes;
    u64 system_time_100ns;
    u64 tick_count;
    u32 system_time_low_value;
    u32 tick_count_low_value;
    u32 system_root_bytes;
    u32 nt_product_type_value;
    u32 processor_feature_checksum;
    u32 page_protection;
    u32 page_checksum;
    u32 update_count;
    u32 context_stored;
    u32 error;
} pe64_kuser_result_t;

typedef struct pe64_security_cookie_result
{
    u32 load_config_directory_rva;
    u32 load_config_directory_bytes;
    u64 load_config_base;
    u64 security_cookie_field;
    u64 security_cookie_address;
    u64 cookie_before;
    u64 cookie_value;
    u64 cookie_after;
    u32 cookie_checksum;
    u32 page_protection;
    u32 context_stored;
    u32 error;
} pe64_security_cookie_result_t;

typedef struct pe64_entry_result
{
    u64 entry_rip;
    u64 transfer_rip;
    u64 stack_base;
    u64 stack_top;
    u64 initial_rsp;
    u64 arg_rcx;
    u64 arg_rdx;
    u64 arg_r8;
    u64 ldr_initialize_thunk;
    u32 transfer_selectors;
    u32 transfer_rflags;
    u32 entry_page_present;
    u32 entry_page_prot;
    u32 stack_page_present;
    u32 stack_page_prot;
    u32 dll_entry;
    u32 transfer_ready;
    u32 transfer_executed;
    u32 transfer_result;
    u32 transfer_aux;
    u32 context_stored;
    u32 error;
} pe64_entry_result_t;

u32 pe64_parse_header(const u8 *data, u32 size, pe64_header_t *out_header);
u32 pe64_parse_sections(
    const u8 *data,
    u32 size,
    const pe64_header_t *header,
    pe64_section_t *out_sections,
    u32 max_sections,
    pe64_section_summary_t *out_summary);
u32 pe64_map_sections(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    const u8 *binary_data,
    u32 binary_size,
    u64 actual_base,
    pe64_map_result_t *out_result);
u32 pe64_apply_relocations(
    u32 pid,
    const pe64_section_t *sections,
    u32 section_count,
    u64 preferred_base,
    u64 actual_base,
    pe64_reloc_result_t *out_result);
u32 pe64_resolve_imports(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    const pe64_shim_registry_t *shim_registry,
    pe64_import_result_t *out_result);
u32 pe64_handle_tls(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u64 tls_block_base,
    u32 tls_index,
    pe64_tls_callback_dispatch_t callback_dispatch,
    void *callback_context,
    pe64_tls_result_t *out_result);
u32 pe64_register_exception_directory(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    pe64_exception_result_t *out_result);
u32 pe64_setup_teb(
    u32 pid,
    u64 teb_base,
    u64 stack_base,
    u64 stack_limit,
    u64 tls_pointer,
    pe64_teb_result_t *out_result);
u32 pe64_setup_peb(
    u32 pid,
    u64 image_base,
    const char *image_path,
    const char *command_line,
    const char *environment,
    pe64_peb_result_t *out_result);
u32 pe64_setup_kuser_shared_data(u32 pid, pe64_kuser_result_t *out_result);
u32 pe64_refresh_kuser_shared_data(u32 pid, pe64_kuser_result_t *out_result);
void pe64_kuser_tick_update_all(void);
u32 pe64_initialize_security_cookie(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    pe64_security_cookie_result_t *out_result);
u32 pe64_launch_entry(
    u32 pid,
    const pe64_header_t *header,
    const pe64_section_t *sections,
    u32 section_count,
    u64 actual_base,
    u64 stack_base,
    u64 stack_bytes,
    u64 ldr_initialize_thunk,
    u32 run_transfer_probe,
    pe64_entry_result_t *out_result);

#endif
