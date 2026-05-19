#ifndef LIMITLESS_APP_MODEL_X64_H
#define LIMITLESS_APP_MODEL_X64_H

#include "types.h"

#define APP64_NETHELLO_ENTRY_RESULT 0x4E484530u
#define APP64_NETHELLO_RESULT 0x4E484531u
#define APP64_NETHELLO_ERROR_RESULT 0x4E484545u

#define APP64_NATIVE_CAPABILITY_CONSOLE 0x00000001u
#define APP64_NATIVE_CAPABILITY_NETWORK 0x00000002u
#define APP64_NATIVE_CAPABILITY_FILESYSTEM 0x00000004u
#define APP64_NATIVE_CAPABILITY_STORAGE 0x00000008u

#define APP64_NETHELLO_STATE_UNREQUESTED 1u
#define APP64_NETHELLO_STATE_UNAVAILABLE 2u
#define APP64_NETHELLO_STATE_READY 3u
#define APP64_NETHELLO_STATE_COMPLETED 4u

#define APP64_NETHELLO_FLAG_REQUESTED 0x00000001u
#define APP64_NETHELLO_FLAG_DESCRIPTOR_READ 0x00000002u
#define APP64_NETHELLO_FLAG_DESCRIPTOR_PARSED 0x00000004u
#define APP64_NETHELLO_FLAG_CONSOLE_DECLARED 0x00000008u
#define APP64_NETHELLO_FLAG_NETWORK_DECLARED 0x00000010u
#define APP64_NETHELLO_FLAG_BINARY_READ 0x00000020u
#define APP64_NETHELLO_FLAG_CHECKSUM_VERIFIED 0x00000040u
#define APP64_NETHELLO_FLAG_MAPPED 0x00000080u
#define APP64_NETHELLO_FLAG_LAUNCHED 0x00000100u
#define APP64_NETHELLO_FLAG_HELLO_COMPLETED 0x00000200u
#define APP64_NETHELLO_FLAG_NETWORK_CAP_REQUESTED 0x00000400u
#define APP64_NETHELLO_FLAG_NETWORK_CAP_GRANTED 0x00000800u
#define APP64_NETHELLO_FLAG_SOCKET_OPEN 0x00001000u
#define APP64_NETHELLO_FLAG_RECV_STATUS 0x00002000u
#define APP64_NETHELLO_FLAG_SEND_DENIED 0x00004000u
#define APP64_NETHELLO_FLAG_SOCKET_CLOSED 0x00008000u
#define APP64_NETHELLO_FLAG_FS_DENIED 0x00010000u
#define APP64_NETHELLO_FLAG_STORAGE_DENIED 0x00020000u
#define APP64_NETHELLO_FLAG_AMBIENT_GATED 0x00040000u
#define APP64_NETHELLO_FLAG_SYSCALL_BRIDGE 0x00080000u
#define APP64_NETHELLO_FLAG_UNAVAILABLE 0x80000000u

void app_model64_init(void);
void app_model64_mark_nethello_unavailable(void);
u32 app_model64_stage_native_app(
    const u8 *app_name,
    u32 app_name_bytes,
    const u8 *descriptor,
    u32 descriptor_bytes,
    const void *binary,
    u32 binary_bytes,
    u32 binary_checksum,
    u32 owner_id);
u32 app_model64_stage_nethello(
    const u8 *descriptor,
    u32 descriptor_bytes,
    const void *binary,
    u32 binary_bytes,
    u32 binary_checksum,
    u32 owner_id);
u32 app_model64_begin_nethello_user(void);
void app_model64_end_nethello_user(void);
u32 app_model64_effective_owner(u32 requested_owner);
u32 app_model64_capability_request_allowed(
    u32 endpoint_class,
    u32 requested_rights,
    u32 owner_id);
void app_model64_record_network_capability(u32 handle);
void app_model64_record_socket_open(u32 socket_handle);
void app_model64_record_recv_status(u32 byte_count);
void app_model64_record_send(u32 byte_count);
void app_model64_record_close(u32 closed);
u32 app_model64_record_native_launch(u32 result, u32 aux);
u32 app_model64_record_nethello_launch(u32 result, u32 aux);

u32 app_model64_native_name_token(void);
u32 app_model64_native_executable_id(void);
u32 app_model64_native_authority_mask(void);
u32 app_model64_native_capability_mask(void);
u32 app_model64_native_payload_slot(void);
u32 app_model64_native_entry_result(void);
u32 app_model64_native_success_result(void);
u32 app_model64_native_binary_path_verified(void);

u32 app_model64_nethello_token(void);
u32 app_model64_nethello_state(void);
u32 app_model64_nethello_flags(void);
u32 app_model64_nethello_owner(void);
u32 app_model64_nethello_descriptor_bytes(void);
u32 app_model64_nethello_binary_bytes(void);
u32 app_model64_nethello_checksum(void);
u32 app_model64_nethello_expected_checksum(void);
u32 app_model64_nethello_mapped_bytes(void);
u32 app_model64_nethello_entry_rip(void);
u32 app_model64_nethello_entry_rsp(void);
u32 app_model64_nethello_entry_selectors(void);
u32 app_model64_nethello_entry_rflags(void);
u32 app_model64_nethello_exit_result(void);
u32 app_model64_nethello_exit_aux(void);
u32 app_model64_nethello_descriptor_read(void);
u32 app_model64_nethello_descriptor_parsed(void);
u32 app_model64_nethello_binary_read(void);
u32 app_model64_nethello_checksum_verified(void);
u32 app_model64_nethello_mapped(void);
u32 app_model64_nethello_launched(void);
u32 app_model64_nethello_hello_completed(void);
u32 app_model64_nethello_network_cap_requested(void);
u32 app_model64_nethello_network_cap_granted(void);
u32 app_model64_nethello_socket_opened(void);
u32 app_model64_nethello_recv_status(void);
u32 app_model64_nethello_send_denied(void);
u32 app_model64_nethello_socket_closed(void);
u32 app_model64_nethello_fs_denied(void);
u32 app_model64_nethello_storage_denied(void);
u32 app_model64_nethello_syscall_bridge(void);
u32 app_model64_nethello_fs_authority(void);
u32 app_model64_nethello_storage_authority(void);
u32 app_model64_nethello_ambient_authority(void);

#endif
