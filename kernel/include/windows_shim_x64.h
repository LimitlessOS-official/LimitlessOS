#ifndef LIMITLESS_WINDOWS_SHIM_X64_H
#define LIMITLESS_WINDOWS_SHIM_X64_H

#include "pe64_x64.h"
#include "types.h"

#define WINDOWS_SHIM64_NTDLL_DEFAULT_BASE 0x0000000044C00000ull
#define WINDOWS_SHIM64_NTDLL_IMAGE_BYTES 0x00003000u
#define WINDOWS_SHIM64_NTDLL_FILE_BYTES 0x00000600u
#define WINDOWS_SHIM64_NTDLL_TEXT_RVA 0x00001000u
#define WINDOWS_SHIM64_NTDLL_RDATA_RVA 0x00002000u
#define WINDOWS_SHIM64_NTDLL_PAGE_BYTES 0x00001000u
#define WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT 29u
#define WINDOWS_SHIM64_NTDLL_LIBRARY_COUNT 1u
#define WINDOWS_SHIM64_NTDLL_RVA_LDR_INITIALIZE_THUNK 0x00001000u
#define WINDOWS_SHIM64_NTDLL_RVA_LDR_LOAD_DLL 0x00001040u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_QUERY_INFORMATION_FILE 0x00001048u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_SET_INFORMATION_FILE 0x00001054u
#define WINDOWS_SHIM64_NTDLL_RVA_RTL_ALLOCATE_HEAP 0x00001060u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_TERMINATE_PROCESS 0x00001068u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_CLOSE 0x00001074u
#define WINDOWS_SHIM64_NTDLL_RVA_RTL_FREE_HEAP 0x00001080u
#define WINDOWS_SHIM64_NTDLL_RVA_RTL_REALLOCATE_HEAP 0x000010A0u
#define WINDOWS_SHIM64_NTDLL_RVA_RTL_CREATE_HEAP 0x000010C0u
#define WINDOWS_SHIM64_NTDLL_RVA_RTL_USER_THREAD_START 0x000010E0u
#define WINDOWS_SHIM64_NTDLL_RVA_KI_USER_EXCEPTION_DISPATCHER 0x00001100u
#define WINDOWS_SHIM64_NTDLL_RVA_NTDLL_DEF_WINDOW_PROC_W 0x00001120u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_WAIT_FOR_SINGLE_OBJECT 0x00001140u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_READ_FILE 0x0000114Cu
#define WINDOWS_SHIM64_NTDLL_RVA_NT_WRITE_FILE 0x00001158u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_OPEN_KEY 0x00001164u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_QUERY_VALUE_KEY 0x00001170u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_ALLOCATE_VIRTUAL_MEMORY 0x0000117Cu
#define WINDOWS_SHIM64_NTDLL_RVA_NT_QUERY_INFORMATION_PROCESS 0x00001188u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_KEY 0x00001194u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_FREE_VIRTUAL_MEMORY 0x000011A0u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_QUERY_SYSTEM_INFORMATION 0x000011ACu
#define WINDOWS_SHIM64_NTDLL_RVA_NT_PROTECT_VIRTUAL_MEMORY 0x000011B8u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_FILE 0x000011C4u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_EVENT 0x000011D0u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_CREATE_MUTANT 0x000011DCu
#define WINDOWS_SHIM64_NTDLL_RVA_NT_RELEASE_MUTANT 0x000011E8u
#define WINDOWS_SHIM64_NTDLL_RVA_NT_SET_EVENT 0x000011F4u

#define WINDOWS_SHIM64_KERNEL32_DEFAULT_BASE 0x0000000044F00000ull
#define WINDOWS_SHIM64_KERNEL32_IMAGE_BYTES 0x00003000u
#define WINDOWS_SHIM64_KERNEL32_FILE_BYTES 0x00000E00u
#define WINDOWS_SHIM64_KERNEL32_TEXT_RVA 0x00001000u
#define WINDOWS_SHIM64_KERNEL32_TEXT_BYTES 0x00000A00u
#define WINDOWS_SHIM64_KERNEL32_RDATA_RVA 0x00002000u
#define WINDOWS_SHIM64_KERNEL32_RDATA_RAW_OFFSET 0x00000C00u
#define WINDOWS_SHIM64_KERNEL32_PAGE_BYTES 0x00001000u
#define WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT 27u
#define WINDOWS_SHIM64_KERNEL32_LIVE_SYMBOL_COUNT 16u
#define WINDOWS_SHIM64_KERNEL32_UNAVAILABLE_SYMBOL_COUNT \
    (WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT - WINDOWS_SHIM64_KERNEL32_LIVE_SYMBOL_COUNT)
#define WINDOWS_SHIM64_KERNEL32_LIBRARY_COUNT 1u
#define WINDOWS_SHIM64_COMBINED_LIBRARY_COUNT 2u
#define WINDOWS_SHIM64_KERNEL32_PROCESS_HEAP_HANDLE 0x0000000048EE0001ull
#define WINDOWS_SHIM64_KERNEL32_RVA_EXIT_PROCESS 0x00001000u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_STD_HANDLE 0x00001020u
#define WINDOWS_SHIM64_KERNEL32_RVA_WRITE_CONSOLE_A 0x00001040u
#define WINDOWS_SHIM64_KERNEL32_RVA_WRITE_CONSOLE_W 0x00001060u
#define WINDOWS_SHIM64_KERNEL32_RVA_READ_FILE 0x00001080u
#define WINDOWS_SHIM64_KERNEL32_RVA_WRITE_FILE 0x000010A0u
#define WINDOWS_SHIM64_KERNEL32_RVA_CREATE_FILE_A 0x000010C0u
#define WINDOWS_SHIM64_KERNEL32_RVA_CREATE_FILE_W 0x000010E0u
#define WINDOWS_SHIM64_KERNEL32_RVA_CLOSE_HANDLE 0x00001100u
#define WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_ALLOC 0x00001120u
#define WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_FREE 0x00001140u
#define WINDOWS_SHIM64_KERNEL32_RVA_VIRTUAL_PROTECT 0x00001160u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_LAST_ERROR 0x00001180u
#define WINDOWS_SHIM64_KERNEL32_RVA_SET_LAST_ERROR 0x000011A0u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_PROCESS 0x000011C0u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_CURRENT_THREAD 0x000011E0u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_SYSTEM_INFO 0x00001200u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_TICK_COUNT64 0x00001220u
#define WINDOWS_SHIM64_KERNEL32_RVA_QUERY_PERFORMANCE_COUNTER 0x00001240u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_PROCESS_HEAP 0x00001260u
#define WINDOWS_SHIM64_KERNEL32_RVA_HEAP_ALLOC 0x00001280u
#define WINDOWS_SHIM64_KERNEL32_RVA_HEAP_FREE 0x000012A0u
#define WINDOWS_SHIM64_KERNEL32_RVA_HEAP_REALLOC 0x000012C0u
#define WINDOWS_SHIM64_KERNEL32_RVA_LOAD_LIBRARY_A 0x000012E0u
#define WINDOWS_SHIM64_KERNEL32_RVA_LOAD_LIBRARY_W 0x00001300u
#define WINDOWS_SHIM64_KERNEL32_RVA_GET_PROC_ADDRESS 0x00001320u
#define WINDOWS_SHIM64_KERNEL32_RVA_SET_FILE_POINTER_EX 0x00001330u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_GET_STD_HANDLE 0x00001340u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_WRITE_FILE 0x00001380u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_VM_HEAP_BASE 0x00001400u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_VIRTUAL_ALLOC 0x00001400u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_VIRTUAL_FREE 0x0000144Bu
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_VIRTUAL_PROTECT 0x00001484u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_GET_PROCESS_HEAP 0x000014C2u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_HEAP_ALLOC 0x000014C8u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_HEAP_FREE 0x0000156Bu
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_HEAP_REALLOC 0x000015DAu
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_EXIT_PROCESS 0x00001740u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_CLOSE_HANDLE 0x00001770u
#define WINDOWS_SHIM64_KERNEL32_RVA_HELPER_SET_FILE_POINTER_EX 0x00001800u

#define WINDOWS_SHIM64_CRT_DEFAULT_BASE 0x0000000045400000ull
#define WINDOWS_SHIM64_CRT_IMAGE_BYTES 0x00003000u
#define WINDOWS_SHIM64_CRT_FILE_BYTES 0x00000A00u
#define WINDOWS_SHIM64_CRT_TEXT_RVA 0x00001000u
#define WINDOWS_SHIM64_CRT_RDATA_RVA 0x00002000u
#define WINDOWS_SHIM64_CRT_PAGE_BYTES 0x00001000u
#define WINDOWS_SHIM64_CRT_SYMBOL_COUNT 21u
#define WINDOWS_SHIM64_CRT_LIBRARY_COUNT 2u
#define WINDOWS_SHIM64_FULL_LIBRARY_COUNT 4u
#define WINDOWS_SHIM64_CRT_RVA_PRINTF 0x00001000u
#define WINDOWS_SHIM64_CRT_RVA_MALLOC 0x00001020u
#define WINDOWS_SHIM64_CRT_RVA_FREE 0x00001040u
#define WINDOWS_SHIM64_CRT_RVA_REALLOC 0x00001060u
#define WINDOWS_SHIM64_CRT_RVA_MEMCPY 0x00001080u
#define WINDOWS_SHIM64_CRT_RVA_MEMSET 0x000010A0u
#define WINDOWS_SHIM64_CRT_RVA_MEMMOVE 0x000010C0u
#define WINDOWS_SHIM64_CRT_RVA_MEMCMP 0x000010E0u
#define WINDOWS_SHIM64_CRT_RVA_STRLEN 0x00001100u
#define WINDOWS_SHIM64_CRT_RVA_STRCPY 0x00001120u
#define WINDOWS_SHIM64_CRT_RVA_STRCMP 0x00001140u
#define WINDOWS_SHIM64_CRT_RVA_STRCAT 0x00001160u
#define WINDOWS_SHIM64_CRT_RVA_FOPEN 0x00001180u
#define WINDOWS_SHIM64_CRT_RVA_FCLOSE 0x000011A0u
#define WINDOWS_SHIM64_CRT_RVA_FREAD 0x000011C0u
#define WINDOWS_SHIM64_CRT_RVA_FWRITE 0x000011E0u
#define WINDOWS_SHIM64_CRT_RVA_FSEEK 0x00001200u
#define WINDOWS_SHIM64_CRT_RVA_FTELL 0x00001220u
#define WINDOWS_SHIM64_CRT_RVA_EXIT 0x00001240u
#define WINDOWS_SHIM64_CRT_RVA_P_ARGC 0x00001260u
#define WINDOWS_SHIM64_CRT_RVA_P_ARGV 0x00001280u

#define WINDOWS_SHIM64_OK 1u
#define WINDOWS_SHIM64_DENIED 0u

#define WINDOWS_SHIM64_ERROR_NONE 0u
#define WINDOWS_SHIM64_ERROR_NULL 1u
#define WINDOWS_SHIM64_ERROR_PERSONA 2u
#define WINDOWS_SHIM64_ERROR_BASE 3u
#define WINDOWS_SHIM64_ERROR_HEADER 4u
#define WINDOWS_SHIM64_ERROR_SECTION 5u
#define WINDOWS_SHIM64_ERROR_MAP 6u
#define WINDOWS_SHIM64_ERROR_ALREADY_MAPPED 7u
#define WINDOWS_SHIM64_ERROR_CONTEXT 8u
#define WINDOWS_SHIM64_ERROR_EXPORT 9u
#define WINDOWS_SHIM64_ERROR_DEPENDENCY 10u

typedef struct windows_shim64_ntdll_result
{
    u64 image_base;
    u64 image_end;
    u64 ldr_initialize_thunk;
    u64 ldr_load_dll;
    u64 rtl_allocate_heap;
    u64 rtl_free_heap;
    u64 rtl_reallocate_heap;
    u64 rtl_create_heap;
    u64 rtl_user_thread_start;
    u64 ki_user_exception_dispatcher;
    u64 ntdll_def_window_proc_w;
    u32 file_bytes;
    u32 image_bytes;
    u32 section_count;
    u32 mapped_count;
    u32 symbol_count;
    u32 image_checksum;
    u32 text_checksum;
    u32 rdata_checksum;
    u32 name_checksum;
    u32 text_protection;
    u32 rdata_protection;
    u32 context_stored;
    u32 error;
} windows_shim64_ntdll_result_t;

typedef struct windows_shim64_kernel32_result
{
    u64 image_base;
    u64 image_end;
    u64 exit_process;
    u64 get_std_handle;
    u64 write_console_a;
    u64 write_console_w;
    u64 read_file;
    u64 write_file;
    u64 create_file_a;
    u64 create_file_w;
    u64 close_handle;
    u64 virtual_alloc;
    u64 virtual_free;
    u64 virtual_protect;
    u64 get_last_error;
    u64 set_last_error;
    u64 get_current_process;
    u64 get_current_thread;
    u64 get_system_info;
    u64 get_tick_count64;
    u64 query_performance_counter;
    u64 get_process_heap;
    u64 heap_alloc;
    u64 heap_free;
    u64 heap_realloc;
    u64 load_library_a;
    u64 load_library_w;
    u64 get_proc_address;
    u64 set_file_pointer_ex;
    u32 file_bytes;
    u32 image_bytes;
    u32 section_count;
    u32 mapped_count;
    u32 symbol_count;
    u32 image_checksum;
    u32 text_checksum;
    u32 rdata_checksum;
    u32 name_checksum;
    u32 text_protection;
    u32 rdata_protection;
    u32 ntdll_ready;
    u32 nt_call_bridge_mask;
    u32 live_stub_count;
    u32 unavailable_stub_count;
    u32 context_stored;
    u32 error;
} windows_shim64_kernel32_result_t;

typedef struct windows_shim64_crt_result
{
    u64 image_base;
    u64 image_end;
    u64 printf_fn;
    u64 malloc_fn;
    u64 free_fn;
    u64 realloc_fn;
    u64 memcpy_fn;
    u64 memset_fn;
    u64 memmove_fn;
    u64 memcmp_fn;
    u64 strlen_fn;
    u64 strcpy_fn;
    u64 strcmp_fn;
    u64 strcat_fn;
    u64 fopen_fn;
    u64 fclose_fn;
    u64 fread_fn;
    u64 fwrite_fn;
    u64 fseek_fn;
    u64 ftell_fn;
    u64 exit_fn;
    u64 p_argc_fn;
    u64 p_argv_fn;
    u32 file_bytes;
    u32 image_bytes;
    u32 section_count;
    u32 mapped_count;
    u32 symbol_count;
    u32 image_checksum;
    u32 text_checksum;
    u32 rdata_checksum;
    u32 name_checksum;
    u32 text_protection;
    u32 rdata_protection;
    u32 kernel32_ready;
    u32 kernel32_export_mask;
    u32 live_stub_count;
    u32 unavailable_stub_count;
    u32 context_stored;
    u32 error;
} windows_shim64_crt_result_t;

typedef struct windows_shim64_registry
{
    pe64_shim_symbol_t ntdll_symbols[WINDOWS_SHIM64_NTDLL_SYMBOL_COUNT];
    pe64_shim_symbol_t kernel32_symbols[WINDOWS_SHIM64_KERNEL32_SYMBOL_COUNT];
    pe64_shim_symbol_t crt_symbols[WINDOWS_SHIM64_CRT_SYMBOL_COUNT];
    pe64_shim_library_t libraries[WINDOWS_SHIM64_FULL_LIBRARY_COUNT];
    pe64_shim_registry_t registry;
} windows_shim64_registry_t;

void windows_shim64_init(void);
u32 windows_shim64_load_ntdll(
    u32 pid,
    u64 image_base,
    windows_shim64_ntdll_result_t *out_result);
u32 windows_shim64_load_kernel32(
    u32 pid,
    u64 image_base,
    windows_shim64_kernel32_result_t *out_result);
u32 windows_shim64_load_crt(
    u32 pid,
    u64 image_base,
    windows_shim64_crt_result_t *out_result);
u64 windows_shim64_ntdll_export(u32 pid, const char *name);
u64 windows_shim64_kernel32_export(u32 pid, const char *name);
u64 windows_shim64_crt_export(u32 pid, const char *name);
u32 windows_shim64_ntdll_registry(u32 pid, windows_shim64_registry_t *out_registry);
u32 windows_shim64_kernel32_registry(u32 pid, windows_shim64_registry_t *out_registry);
u32 windows_shim64_combined_registry(u32 pid, windows_shim64_registry_t *out_registry);
u32 windows_shim64_crt_registry(u32 pid, windows_shim64_registry_t *out_registry);
u32 windows_shim64_full_registry(u32 pid, windows_shim64_registry_t *out_registry);
const u8 *windows_shim64_ntdll_image(void);
const u8 *windows_shim64_kernel32_image(void);
const u8 *windows_shim64_crt_image(void);
u32 windows_shim64_ntdll_image_size(void);
u32 windows_shim64_kernel32_image_size(void);
u32 windows_shim64_crt_image_size(void);
u32 windows_shim64_ntdll_load_count(void);
u32 windows_shim64_ntdll_denial_count(void);
u32 windows_shim64_ntdll_last_error(void);
u64 windows_shim64_ntdll_last_base(void);
u32 windows_shim64_kernel32_load_count(void);
u32 windows_shim64_kernel32_denial_count(void);
u32 windows_shim64_kernel32_last_error(void);
u64 windows_shim64_kernel32_last_base(void);
u32 windows_shim64_crt_load_count(void);
u32 windows_shim64_crt_denial_count(void);
u32 windows_shim64_crt_last_error(void);
u64 windows_shim64_crt_last_base(void);

#endif
