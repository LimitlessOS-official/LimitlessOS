#ifndef LIMITLESS_LAUNCH_X64_H
#define LIMITLESS_LAUNCH_X64_H

#include "types.h"

#define LAUNCH64_INVALID_MANIFEST 0xFFFFFFFFu
#define LAUNCH64_INVALID_REQUEST 0xFFFFFFFFu
#define LAUNCH64_AUTHORITY_KERNEL_SERVICE 0x00000004u

#define LAUNCH64_STATE_VERIFIED 0x00000001u
#define LAUNCH64_STATE_READY 0x00000002u
#define LAUNCH64_STATE_STARTED 0x00000004u
#define LAUNCH64_STATE_STOP_DENIED 0x00000008u
#define LAUNCH64_STATE_QUIESCE_READY 0x00000010u
#define LAUNCH64_STATE_CAPS_DRAINED 0x00000020u
#define LAUNCH64_STATE_IMAGE_PLAN_READY 0x00000040u
#define LAUNCH64_STATE_IMAGE_MAP_READY 0x00000080u
#define LAUNCH64_STATE_IMAGE_MAP_INSTALLED 0x00000100u
#define LAUNCH64_STATE_IMAGE_PROTECTED 0x00000200u
#define LAUNCH64_STATE_USER_ENTRY_PLANNED 0x00000400u
#define LAUNCH64_STATE_USER_ENTRY_BLOCKED 0x00000800u
#define LAUNCH64_STATE_USER_ENTRY_READY 0x00001000u

#define LAUNCH64_IMAGE_PLAN_BASE 0x40000000u
#define LAUNCH64_USER_IMAGE_BASE 0x41000000u
#define LAUNCH64_DISK_IMAGE_PLAN_BASE 0x42000000u
#define LAUNCH64_DISK_USER_IMAGE_BASE 0x43000000u
#define LAUNCH64_DISK_USER_ENTRY_OFFSET 0x00000010u
#define LAUNCH64_DISK_LS_PAYLOAD_SLOT 2u
#define LAUNCH64_DISK_CAT_PAYLOAD_SLOT 3u
#define LAUNCH64_DISK_STAT_PAYLOAD_SLOT 4u
#define LAUNCH64_DISK_MKDIR_PAYLOAD_SLOT 5u
#define LAUNCH64_DISK_WRITE_PAYLOAD_SLOT 6u
#define LAUNCH64_DISK_TOUCH_PAYLOAD_SLOT 7u
#define LAUNCH64_DISK_APPEND_PAYLOAD_SLOT 8u
#define LAUNCH64_DISK_COPY_PAYLOAD_SLOT 9u
#define LAUNCH64_DISK_DELETE_PAYLOAD_SLOT 10u
#define LAUNCH64_DISK_RENAME_PAYLOAD_SLOT 11u
#define LAUNCH64_DISK_MOVE_PAYLOAD_SLOT 12u
#define LAUNCH64_DISK_NETHELLO_PAYLOAD_SLOT 13u
#define LAUNCH64_IMAGE_MAP_PAGE_BYTES 0x00001000u
#define LAUNCH64_USER_STACK_TOP 0x40020000u
#define LAUNCH64_USER_STACK_BYTES 0x00001000u
#define LAUNCH64_USER_RFLAGS 0x00000002u
#define LAUNCH64_IMAGE_PLAN_RIGHT_READ 0x00000001u
#define LAUNCH64_IMAGE_PLAN_RIGHT_EXECUTE 0x00000002u
#define LAUNCH64_IMAGE_PLAN_RIGHT_SEALED 0x00000004u
#define LAUNCH64_IMAGE_PLAN_RIGHT_SUPERVISOR_VALIDATE 0x00000008u
#define LAUNCH64_IMAGE_PLAN_RIGHT_VALIDATION_ONLY 0x00000010u
#define LAUNCH64_IMAGE_PLAN_RIGHTS \
    (LAUNCH64_IMAGE_PLAN_RIGHT_READ \
        | LAUNCH64_IMAGE_PLAN_RIGHT_EXECUTE \
        | LAUNCH64_IMAGE_PLAN_RIGHT_SEALED \
        | LAUNCH64_IMAGE_PLAN_RIGHT_SUPERVISOR_VALIDATE \
        | LAUNCH64_IMAGE_PLAN_RIGHT_VALIDATION_ONLY)
#define LAUNCH64_IMAGE_MAP_RIGHTS LAUNCH64_IMAGE_PLAN_RIGHTS

#define LAUNCH64_PHASE_NONE 0x00000000u
#define LAUNCH64_PHASE_VERIFIED 0x00000001u
#define LAUNCH64_PHASE_READY 0x00000002u
#define LAUNCH64_PHASE_STARTED 0x00000003u
#define LAUNCH64_PHASE_DRAINED 0x00000004u
#define LAUNCH64_PHASE_QUIESCE_READY 0x00000005u

#define LAUNCH64_OPERATION_NONE 0x00000000u
#define LAUNCH64_OPERATION_START 0x00000001u
#define LAUNCH64_OPERATION_STOP 0x00000002u
#define LAUNCH64_OPERATION_QUIESCE 0x00000003u
#define LAUNCH64_OPERATION_DRAIN_CAPS 0x00000004u
#define LAUNCH64_OPERATION_RESTART 0x00000005u

#define LAUNCH64_REQUEST_EMPTY 0x00000000u
#define LAUNCH64_REQUEST_PENDING 0x00000001u
#define LAUNCH64_REQUEST_APPROVED 0x00000002u
#define LAUNCH64_REQUEST_DENIED 0x00000004u
#define LAUNCH64_REQUEST_COMPLETED 0x00000008u

#define LAUNCH64_DENY_NONE 0x00000000u
#define LAUNCH64_DENY_INVALID_MANIFEST 0x00000001u
#define LAUNCH64_DENY_CONTRACT_MISMATCH 0x00000002u
#define LAUNCH64_DENY_DUPLICATE_START 0x00000003u
#define LAUNCH64_DENY_INVALID_BINDING 0x00000004u
#define LAUNCH64_DENY_UNAUTHORIZED_REQUESTER 0x00000005u
#define LAUNCH64_DENY_REQUEST_LOG_FULL 0x00000006u
#define LAUNCH64_DENY_NOT_STARTED 0x00000007u
#define LAUNCH64_DENY_PROTECTED_SERVICE 0x00000008u
#define LAUNCH64_DENY_ACTIVE_CAPABILITIES 0x00000009u
#define LAUNCH64_DENY_NOT_QUIESCE_READY 0x0000000Au
#define LAUNCH64_DENY_IMAGE_PLAN 0x0000000Bu
#define LAUNCH64_DENY_IMAGE_MAP 0x0000000Cu

#define LAUNCH64_USER_ENTRY_FRAME_PLANNED 0x00000001u
#define LAUNCH64_USER_ENTRY_DESCRIPTORS_READY 0x00000002u
#define LAUNCH64_USER_ENTRY_USER_VIEW_READY 0x00000004u
#define LAUNCH64_USER_ENTRY_TRANSFER_READY 0x00000008u
#define LAUNCH64_USER_ENTRY_BLOCKED 0x00000010u
#define LAUNCH64_USER_ENTRY_STACK_READY 0x00000020u

#define LAUNCH64_USER_ENTRY_DENY_NONE 0x00000000u
#define LAUNCH64_USER_ENTRY_DENY_DESCRIPTOR_STATE 0x00000001u
#define LAUNCH64_USER_ENTRY_DENY_SUPERVISOR_VALIDATION_VIEW 0x00000002u
#define LAUNCH64_USER_ENTRY_DENY_STACK_VIEW 0x00000003u

void launch64_init(void);
u32 launch64_archive_valid(void);
u32 launch64_archive_checksum(void);
u32 launch64_manifest_total_count(void);
u32 launch64_manifest_count(void);
u32 launch64_manifest_ignored_count(void);
u32 launch64_manifest_denial_count(void);
u32 launch64_service_ready_count(void);
u32 launch64_service_started_count(void);
u32 launch64_service_drained_count(void);
u32 launch64_service_quiesce_ready_count(void);
u32 launch64_service_start_denial_count(void);
u32 launch64_service_start_request_count(void);
u32 launch64_service_start_approval_count(void);
u32 launch64_service_start_pending_count(void);
u32 launch64_service_start_denied_count(void);
u32 launch64_service_start_completed_count(void);
u32 launch64_service_stop_request_count(void);
u32 launch64_service_stop_approval_count(void);
u32 launch64_service_stop_pending_count(void);
u32 launch64_service_stop_denied_count(void);
u32 launch64_service_stop_completed_count(void);
u32 launch64_service_quiesce_request_count(void);
u32 launch64_service_quiesce_approval_count(void);
u32 launch64_service_quiesce_pending_count(void);
u32 launch64_service_quiesce_denied_count(void);
u32 launch64_service_quiesce_completed_count(void);
u32 launch64_service_drain_request_count(void);
u32 launch64_service_drain_approval_count(void);
u32 launch64_service_drain_pending_count(void);
u32 launch64_service_drain_denied_count(void);
u32 launch64_service_drain_completed_count(void);
u32 launch64_service_restart_request_count(void);
u32 launch64_service_restart_approval_count(void);
u32 launch64_service_restart_pending_count(void);
u32 launch64_service_restart_denied_count(void);
u32 launch64_service_restart_completed_count(void);
u32 launch64_request_log_count(void);
u32 launch64_request_id_by_index(u32 index);
u32 launch64_request_operation(u32 request_id);
u32 launch64_request_status(u32 request_id);
u32 launch64_request_manifest(u32 request_id);
u32 launch64_request_requester(u32 request_id);
u32 launch64_request_denial(u32 request_id);
u32 launch64_request_observed_capabilities(u32 request_id);
u32 launch64_request_revoked_capabilities(u32 request_id);
u32 launch64_request_runtime_generation(u32 request_id);
u32 launch64_request_runtime_token(u32 request_id);
u32 launch64_request_runtime_image_generation(u32 request_id);
u32 launch64_request_runtime_image_token(u32 request_id);
u32 launch64_request_runtime_image_base(u32 request_id);
u32 launch64_request_runtime_image_entry(u32 request_id);
u32 launch64_request_runtime_image_mapped_bytes(u32 request_id);
u32 launch64_request_runtime_image_rights(u32 request_id);
u32 launch64_request_runtime_image_plan_token(u32 request_id);
u32 launch64_request_runtime_image_map_token(u32 request_id);
u32 launch64_request_runtime_image_page_count(u32 request_id);
u32 launch64_request_runtime_image_pml4_index(u32 request_id);
u32 launch64_request_runtime_image_pdpt_index(u32 request_id);
u32 launch64_request_runtime_image_pd_index(u32 request_id);
u32 launch64_request_runtime_entry_transfer_token(u32 request_id);
u32 launch64_request_runtime_image_install_token(u32 request_id);
u32 launch64_request_runtime_image_source_checksum(u32 request_id);
u32 launch64_request_runtime_image_entry_probe(u32 request_id);
u32 launch64_request_runtime_image_map_installed(u32 request_id);
u32 launch64_request_runtime_image_protection_flags(u32 request_id);
u32 launch64_request_runtime_image_protection_token(u32 request_id);
u32 launch64_request_runtime_user_entry_state(u32 request_id);
u32 launch64_request_runtime_user_entry_token(u32 request_id);
u32 launch64_request_runtime_user_entry_rip(u32 request_id);
u32 launch64_request_runtime_user_entry_rsp(u32 request_id);
u32 launch64_request_runtime_user_entry_selectors(u32 request_id);
u32 launch64_request_runtime_user_entry_rflags(u32 request_id);
u32 launch64_request_runtime_user_entry_denial(u32 request_id);
u32 launch64_request_runtime_payload_slot(u32 request_id);
u32 launch64_request_runtime_payload_kind(u32 request_id);
u32 launch64_request_runtime_payload_offset(u32 request_id);
u32 launch64_request_runtime_payload_size(u32 request_id);
u32 launch64_request_runtime_payload_checksum(u32 request_id);
u32 launch64_requester_can_start(u32 requester_principal);
u32 launch64_requester_can_stop(u32 requester_principal);
u32 launch64_requester_can_quiesce(u32 requester_principal);
u32 launch64_requester_can_drain(u32 requester_principal);
u32 launch64_requester_can_restart(u32 requester_principal);
u32 launch64_manifest_by_process(const char *process_name);
u32 launch64_manifest_by_endpoint_class(u32 endpoint_class);
u32 launch64_request_service_start(
    u32 requester_principal,
    u32 manifest_index,
    u32 pid,
    u32 principal_id,
    u32 endpoint_class,
    u32 expected_scheduler_class,
    u32 expected_capability_limit);
u32 launch64_request_service_quiesce(u32 requester_principal, u32 manifest_index);
u32 launch64_request_service_drain(u32 requester_principal, u32 manifest_index);
u32 launch64_request_service_restart(u32 requester_principal, u32 manifest_index);
u32 launch64_request_service_stop(u32 requester_principal, u32 manifest_index);
u32 launch64_manifest_source_slot(u32 manifest_index);
u32 launch64_manifest_package_id(u32 manifest_index);
u32 launch64_manifest_executable_id(u32 manifest_index);
u32 launch64_manifest_signer_id(u32 manifest_index);
u32 launch64_manifest_scheduler_class(u32 manifest_index);
u32 launch64_manifest_capability_limit(u32 manifest_index);
u32 launch64_manifest_token(u32 manifest_index);
u32 launch64_manifest_launch_state(u32 manifest_index);
u32 launch64_manifest_lifecycle_phase(u32 manifest_index);
u32 launch64_manifest_restart_count(u32 manifest_index);
u32 launch64_manifest_runtime_generation(u32 manifest_index);
u32 launch64_manifest_runtime_token(u32 manifest_index);
u32 launch64_manifest_accepts_runtime_token(u32 manifest_index, u32 runtime_token);
u32 launch64_manifest_runtime_image_generation(u32 manifest_index);
u32 launch64_manifest_runtime_image_token(u32 manifest_index);
u32 launch64_manifest_runtime_image_base(u32 manifest_index);
u32 launch64_manifest_runtime_image_entry(u32 manifest_index);
u32 launch64_manifest_runtime_image_mapped_bytes(u32 manifest_index);
u32 launch64_manifest_runtime_image_rights(u32 manifest_index);
u32 launch64_manifest_runtime_image_plan_token(u32 manifest_index);
u32 launch64_manifest_runtime_image_map_token(u32 manifest_index);
u32 launch64_manifest_runtime_image_page_count(u32 manifest_index);
u32 launch64_manifest_runtime_image_pml4_index(u32 manifest_index);
u32 launch64_manifest_runtime_image_pdpt_index(u32 manifest_index);
u32 launch64_manifest_runtime_image_pd_index(u32 manifest_index);
u32 launch64_manifest_runtime_entry_transfer_token(u32 manifest_index);
u32 launch64_manifest_runtime_image_install_token(u32 manifest_index);
u32 launch64_manifest_runtime_image_source_checksum(u32 manifest_index);
u32 launch64_manifest_runtime_image_entry_probe(u32 manifest_index);
u32 launch64_manifest_runtime_image_map_installed(u32 manifest_index);
u32 launch64_manifest_runtime_image_protection_flags(u32 manifest_index);
u32 launch64_manifest_runtime_image_protection_token(u32 manifest_index);
u32 launch64_manifest_runtime_user_entry_state(u32 manifest_index);
u32 launch64_manifest_runtime_user_entry_token(u32 manifest_index);
u32 launch64_manifest_runtime_user_entry_rip(u32 manifest_index);
u32 launch64_manifest_runtime_user_entry_rsp(u32 manifest_index);
u32 launch64_manifest_runtime_user_entry_selectors(u32 manifest_index);
u32 launch64_manifest_runtime_user_entry_rflags(u32 manifest_index);
u32 launch64_manifest_runtime_user_entry_denial(u32 manifest_index);
u32 launch64_manifest_runtime_payload_slot(u32 manifest_index);
u32 launch64_manifest_runtime_payload_kind(u32 manifest_index);
u32 launch64_manifest_runtime_payload_offset(u32 manifest_index);
u32 launch64_manifest_runtime_payload_size(u32 manifest_index);
u32 launch64_manifest_runtime_payload_checksum(u32 manifest_index);
u32 launch64_payload_size_by_slot(u32 payload_slot);
u32 launch64_payload_checksum_by_slot(u32 payload_slot);
u32 launch64_stage_disk_flat_binary(
    u32 payload_slot,
    const void *source,
    u32 source_bytes,
    u32 mapped_bytes,
    u32 entry_probe_result,
    u32 *entry_rip_out,
    u32 *entry_rsp_out,
    u32 *entry_selectors_out,
    u32 *entry_rflags_out,
    u32 *map_token_out);
u32 launch64_manifest_launched_pid(u32 manifest_index);
u32 launch64_manifest_launched_principal(u32 manifest_index);
u32 launch64_manifest_launched_endpoint_class(u32 manifest_index);
u32 launch64_manifest_last_requester(u32 manifest_index);
u32 launch64_manifest_last_request_id(u32 manifest_index);
u32 launch64_manifest_last_request_status(u32 manifest_index);
u32 launch64_manifest_last_denial(u32 manifest_index);
const char *launch64_manifest_package_name(u32 manifest_index);
const char *launch64_manifest_process_name(u32 manifest_index);
const char *launch64_manifest_profile_name(u32 manifest_index);

#endif
