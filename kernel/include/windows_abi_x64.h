#ifndef LIMITLESS_WINDOWS_ABI_X64_H
#define LIMITLESS_WINDOWS_ABI_X64_H

#include "types.h"

#define WINDOWS_ABI64_SYSCALL_LIMIT 512u

#define WINDOWS_ABI64_STATUS_SUCCESS 0x00000000u
#define WINDOWS_ABI64_STATUS_ACCESS_VIOLATION 0xC0000005u
#define WINDOWS_ABI64_STATUS_INVALID_HANDLE 0xC0000008u
#define WINDOWS_ABI64_STATUS_INVALID_PARAMETER 0xC000000Du
#define WINDOWS_ABI64_STATUS_NO_MEMORY 0xC0000017u
#define WINDOWS_ABI64_STATUS_NOT_IMPLEMENTED 0xC0000002u
#define WINDOWS_ABI64_STATUS_INVALID_INFO_CLASS 0xC0000003u
#define WINDOWS_ABI64_STATUS_INFO_LENGTH_MISMATCH 0xC0000004u
#define WINDOWS_ABI64_STATUS_INVALID_SYSTEM_SERVICE 0xC000001Cu
#define WINDOWS_ABI64_STATUS_BUFFER_TOO_SMALL 0xC0000023u
#define WINDOWS_ABI64_STATUS_ACCESS_DENIED 0xC0000022u
#define WINDOWS_ABI64_STATUS_OBJECT_NAME_NOT_FOUND 0xC0000034u
#define WINDOWS_ABI64_STATUS_MUTANT_NOT_OWNED 0xC0000046u
#define WINDOWS_ABI64_STATUS_TIMEOUT 0x00000102u

#define WINDOWS_ABI64_SYSCALL_NTWAITFORSINGLEOBJECT 0x00000004u
#define WINDOWS_ABI64_SYSCALL_NTREADFILE 0x00000006u
#define WINDOWS_ABI64_SYSCALL_UNIMPLEMENTED_PROBE 0x00000007u
#define WINDOWS_ABI64_SYSCALL_NTWRITEFILE 0x00000008u
#define WINDOWS_ABI64_SYSCALL_NTOPENKEY 0x00000012u
#define WINDOWS_ABI64_SYSCALL_NTQUERYVALUEKEY 0x00000017u
#define WINDOWS_ABI64_SYSCALL_NTALLOCATEVIRTUALMEMORY 0x00000018u
#define WINDOWS_ABI64_SYSCALL_NTQUERYINFORMATIONPROCESS 0x00000019u
#define WINDOWS_ABI64_SYSCALL_NTCREATEKEY 0x0000001Du
#define WINDOWS_ABI64_SYSCALL_NTFREEVIRTUALMEMORY 0x0000001Eu
#define WINDOWS_ABI64_SYSCALL_NTQUERYSYSTEMINFORMATION 0x00000036u
#define WINDOWS_ABI64_SYSCALL_NTPROTECTVIRTUALMEMORY 0x00000050u
#define WINDOWS_ABI64_SYSCALL_NTCREATEFILE 0x00000055u
#define WINDOWS_ABI64_SYSCALL_NTCREATEMUTANT 0x00000057u
#define WINDOWS_ABI64_SYSCALL_NTRELEASEMUTANT 0x000000ACu
#define WINDOWS_ABI64_SYSCALL_NTCREATEEVENT 0x00000048u
#define WINDOWS_ABI64_SYSCALL_NTSETEVENT 0x000000E2u

#define WINDOWS_ABI64_CURRENT_PROCESS_HANDLE 0xFFFFFFFFFFFFFFFFull
#define WINDOWS_ABI64_STDOUT_HANDLE 4ull
#define WINDOWS_ABI64_STDERR_HANDLE 8ull
#define WINDOWS_ABI64_STDIN_HANDLE 12ull
#define WINDOWS_ABI64_IO_STATUS_BLOCK_BYTES 16u
#define WINDOWS_ABI64_READ_CHUNK_BYTES 128u
#define WINDOWS_ABI64_WRITE_CHUNK_BYTES 512u
#define WINDOWS_ABI64_OBJECT_ATTRIBUTES_BYTES 48u
#define WINDOWS_ABI64_UNICODE_STRING_BYTES 16u
#define WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_CLASS 0u
#define WINDOWS_ABI64_PROCESS_DEBUG_PORT_CLASS 7u
#define WINDOWS_ABI64_PROCESS_IMAGE_FILE_NAME_CLASS 27u
#define WINDOWS_ABI64_PROCESS_BASIC_INFORMATION_BYTES 48u
#define WINDOWS_ABI64_PROCESS_DEBUG_PORT_BYTES 8u
#define WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_CLASS 0u
#define WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_CLASS 1u
#define WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_CLASS 2u
#define WINDOWS_ABI64_SYSTEM_BASIC_INFORMATION_BYTES 64u
#define WINDOWS_ABI64_SYSTEM_PROCESSOR_INFORMATION_BYTES 12u
#define WINDOWS_ABI64_SYSTEM_PERFORMANCE_INFORMATION_BYTES 0x00000138u
#define WINDOWS_ABI64_ALLOCATION_GRANULARITY_BYTES 0x00010000u
#define WINDOWS_ABI64_PROCESSOR_ARCHITECTURE_AMD64 9u
#define WINDOWS_ABI64_CREATE_PATH_ASCII_BYTES 192u
#define WINDOWS_ABI64_CREATE_PATH_UTF16_BYTES \
    (WINDOWS_ABI64_CREATE_PATH_ASCII_BYTES * 2u)
#define WINDOWS_ABI64_FILE_SUPERSEDE 0u
#define WINDOWS_ABI64_FILE_OPEN 1u
#define WINDOWS_ABI64_FILE_CREATE 2u
#define WINDOWS_ABI64_FILE_OPEN_IF 3u
#define WINDOWS_ABI64_FILE_OVERWRITE 4u
#define WINDOWS_ABI64_FILE_OVERWRITE_IF 5u
#define WINDOWS_ABI64_FILE_OPENED 1ull
#define WINDOWS_ABI64_MEM_COMMIT 0x00001000u
#define WINDOWS_ABI64_MEM_RESERVE 0x00002000u
#define WINDOWS_ABI64_MEM_DECOMMIT 0x00004000u
#define WINDOWS_ABI64_MEM_RELEASE 0x00008000u
#define WINDOWS_ABI64_MEM_TOP_DOWN 0x00100000u
#define WINDOWS_ABI64_PAGE_NOACCESS 0x00000001u
#define WINDOWS_ABI64_PAGE_READONLY 0x00000002u
#define WINDOWS_ABI64_PAGE_READWRITE 0x00000004u
#define WINDOWS_ABI64_PAGE_EXECUTE 0x00000010u
#define WINDOWS_ABI64_PAGE_EXECUTE_READ 0x00000020u
#define WINDOWS_ABI64_PAGE_EXECUTE_READWRITE 0x00000040u
#define WINDOWS_ABI64_NOTIFICATION_EVENT 0u
#define WINDOWS_ABI64_SYNCHRONIZATION_EVENT 1u

typedef u32 (*windows_abi64_handler_t)(
    u32 pid,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip);

void windows_abi64_init(void);
void windows_abi64_configure_system_information(
    u64 physical_memory_bytes,
    u32 processor_count,
    u32 processor_id);
windows_abi64_handler_t *windows_abi64_dispatch_table(void);
u32 windows_abi64_dispatch(
    u32 pid,
    u32 syscall_number,
    u64 rcx,
    u64 rdx,
    u64 r8,
    u64 r9,
    const u64 *stack_args,
    u32 stack_arg_count,
    u64 rip);
u32 windows_abi64_table_size(void);
u32 windows_abi64_table_ready(void);
u32 windows_abi64_unimplemented_entry_count(void);
u32 windows_abi64_entry_is_unimplemented(u32 syscall_number);
u32 windows_abi64_dispatch_count(void);
u32 windows_abi64_unimplemented_count(void);
u32 windows_abi64_invalid_service_count(void);
u32 windows_abi64_last_pid(void);
u32 windows_abi64_last_syscall(void);
u32 windows_abi64_last_result(void);
u64 windows_abi64_last_rip(void);
u32 windows_abi64_read_entry_installed(void);
u32 windows_abi64_read_count(void);
u32 windows_abi64_read_byte_count(void);
u32 windows_abi64_read_denial_count(void);
u32 windows_abi64_read_fault_count(void);
u32 windows_abi64_read_last_handle_low(void);
u32 windows_abi64_read_last_byte_count(void);
u32 windows_abi64_read_last_capability(void);
u32 windows_abi64_read_last_result(void);
u32 windows_abi64_write_entry_installed(void);
u32 windows_abi64_write_count(void);
u32 windows_abi64_write_byte_count(void);
u32 windows_abi64_write_denial_count(void);
u32 windows_abi64_write_fault_count(void);
u32 windows_abi64_write_last_handle_low(void);
u32 windows_abi64_write_last_byte_count(void);
u32 windows_abi64_write_last_capability(void);
u32 windows_abi64_write_last_result(void);
u32 windows_abi64_allocate_entry_installed(void);
u32 windows_abi64_allocate_count(void);
u32 windows_abi64_allocate_denial_count(void);
u32 windows_abi64_allocate_fault_count(void);
u32 windows_abi64_allocate_byte_count(void);
u64 windows_abi64_allocate_last_base(void);
u64 windows_abi64_allocate_last_size(void);
u32 windows_abi64_allocate_last_protect(void);
u32 windows_abi64_allocate_last_type(void);
u32 windows_abi64_allocate_last_result(void);
u32 windows_abi64_free_entry_installed(void);
u32 windows_abi64_free_count(void);
u32 windows_abi64_free_denial_count(void);
u32 windows_abi64_free_fault_count(void);
u32 windows_abi64_free_byte_count(void);
u64 windows_abi64_free_last_base(void);
u64 windows_abi64_free_last_size(void);
u32 windows_abi64_free_last_type(void);
u32 windows_abi64_free_last_result(void);
u32 windows_abi64_protect_entry_installed(void);
u32 windows_abi64_protect_count(void);
u32 windows_abi64_protect_denial_count(void);
u32 windows_abi64_protect_fault_count(void);
u32 windows_abi64_protect_byte_count(void);
u64 windows_abi64_protect_last_base(void);
u64 windows_abi64_protect_last_size(void);
u32 windows_abi64_protect_last_new_protect(void);
u32 windows_abi64_protect_last_old_protect(void);
u32 windows_abi64_protect_last_result(void);
u32 windows_abi64_wait_entry_installed(void);
u32 windows_abi64_wait_timed_count(void);
u32 windows_abi64_wait_timeout_count(void);
u32 windows_abi64_wait_timeout_denial_count(void);
u32 windows_abi64_wait_last_timeout_task(void);
u32 windows_abi64_wait_last_timeout_ticks(void);
u32 windows_abi64_wait_last_timeout_result(void);
u32 windows_abi64_wait_last_timeout_handle_low(void);
u32 windows_abi64_create_event_entry_installed(void);
u32 windows_abi64_set_event_entry_installed(void);
u32 windows_abi64_event_create_count(void);
u32 windows_abi64_event_set_count(void);
u32 windows_abi64_event_wait_count(void);
u32 windows_abi64_event_denial_count(void);
u32 windows_abi64_event_fault_count(void);
u32 windows_abi64_event_last_handle_low(void);
u32 windows_abi64_event_last_previous_state(void);
u32 windows_abi64_event_last_state(void);
u32 windows_abi64_event_last_result(void);
u32 windows_abi64_create_mutant_entry_installed(void);
u32 windows_abi64_release_mutant_entry_installed(void);
u32 windows_abi64_mutant_create_count(void);
u32 windows_abi64_mutant_wait_count(void);
u32 windows_abi64_mutant_release_count(void);
u32 windows_abi64_mutant_denial_count(void);
u32 windows_abi64_mutant_fault_count(void);
u32 windows_abi64_mutant_last_handle_low(void);
u32 windows_abi64_mutant_last_previous_count(void);
u32 windows_abi64_mutant_last_owner(void);
u32 windows_abi64_mutant_last_count(void);
u32 windows_abi64_mutant_last_result(void);
u32 windows_abi64_query_process_entry_installed(void);
u32 windows_abi64_query_process_count(void);
u32 windows_abi64_query_process_denial_count(void);
u32 windows_abi64_query_process_fault_count(void);
u32 windows_abi64_query_process_last_class(void);
u32 windows_abi64_query_process_last_result(void);
u64 windows_abi64_query_process_last_peb(void);
u32 windows_abi64_query_process_last_return_length(void);
u32 windows_abi64_query_system_entry_installed(void);
u32 windows_abi64_query_system_count(void);
u32 windows_abi64_query_system_denial_count(void);
u32 windows_abi64_query_system_fault_count(void);
u32 windows_abi64_query_system_last_class(void);
u32 windows_abi64_query_system_last_result(void);
u32 windows_abi64_query_system_last_return_length(void);
u32 windows_abi64_query_system_last_page_size(void);
u32 windows_abi64_query_system_last_processor_count(void);
u32 windows_abi64_query_system_last_physical_pages(void);
u32 windows_abi64_query_system_last_free_pages(void);
u32 windows_abi64_open_key_entry_installed(void);
u32 windows_abi64_create_key_entry_installed(void);
u32 windows_abi64_query_value_key_entry_installed(void);
u32 windows_abi64_registry_open_count(void);
u32 windows_abi64_registry_create_count(void);
u32 windows_abi64_registry_query_count(void);
u32 windows_abi64_registry_denial_count(void);
u32 windows_abi64_registry_fault_count(void);
u32 windows_abi64_registry_last_syscall(void);
u32 windows_abi64_registry_last_result(void);
u32 windows_abi64_registry_last_key_id(void);
u32 windows_abi64_registry_last_required_bytes(void);
u32 windows_abi64_registry_last_value_type(void);
u32 windows_abi64_create_entry_installed(void);
u32 windows_abi64_create_count(void);
u32 windows_abi64_create_denial_count(void);
u32 windows_abi64_create_fault_count(void);
u32 windows_abi64_create_last_handle_low(void);
u32 windows_abi64_create_last_capability(void);
u32 windows_abi64_create_last_result(void);
u32 windows_abi64_create_last_path_hash(void);
u32 windows_abi64_create_last_path_bytes(void);
u32 windows_abi64_create_last_shim_id(void);

#endif
