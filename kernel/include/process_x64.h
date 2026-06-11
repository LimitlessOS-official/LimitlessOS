#ifndef LIMITLESS_PROCESS_X64_H
#define LIMITLESS_PROCESS_X64_H

#include "types.h"

#define PROCESS64_INVALID_PID 0xFFFFFFFFu
#define PROCESS64_CLONE_PID_BASE 0x00002000u
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#define PROCESS64_CLONE_RECORD_LIMIT 16u
#else
#define PROCESS64_CLONE_RECORD_LIMIT 0u
#endif

#define PROCESS64_STATE_BOOTSTRAPPED 0x00000001u
#define PROCESS64_STATE_SERVICE 0x00000002u
#define PROCESS64_STATE_READY 0x00000004u
#define PROCESS64_STATE_SEALED 0x00000008u
#define PROCESS64_STATE_MANIFEST_VERIFIED 0x00000010u
#define PROCESS64_STATE_LAUNCH_STARTED 0x00000020u
#define PROCESS64_STATE_RESTARTED 0x00000040u
#define PROCESS64_STATE_IMAGE_PLANNED 0x00000080u
#define PROCESS64_STATE_IMAGE_MAPPED 0x00000100u
#define PROCESS64_STATE_ENTRY_READY 0x00000200u
#define PROCESS64_STATE_IMAGE_MAP_INSTALLED 0x00000400u
#define PROCESS64_STATE_IMAGE_PROTECTED 0x00000800u
#define PROCESS64_STATE_USER_ENTRY_PLANNED 0x00001000u
#define PROCESS64_STATE_USER_ENTRY_BLOCKED 0x00002000u
#define PROCESS64_STATE_USER_ENTRY_READY 0x00004000u

#define PROCESS64_CLASS_SYSTEM 0x00000001u
#define PROCESS64_CLASS_POLICY 0x00000002u
#define PROCESS64_CLASS_INTERACTIVE 0x00000003u
#define PROCESS64_CLASS_IO 0x00000004u
#define PROCESS64_CLASS_BACKGROUND 0x00000005u

#define PROCESS64_CHILD_KIND_NONE 0x00000000u
#define PROCESS64_CHILD_KIND_CLONE 0x00000001u
#define PROCESS64_CHILD_KIND_FORK 0x00000002u

void process64_init(void);
u32 process64_count(void);
u32 process64_pid_by_index(u32 index);
u32 process64_pid_for_principal(u32 principal_id);
u32 process64_principal(u32 pid);
u32 process64_endpoint(u32 pid);
u32 process64_endpoint_class(u32 pid);
u32 process64_state(u32 pid);
u32 process64_scheduler_class(u32 pid);
u32 process64_capability_limit(u32 pid);
u32 process64_manifest_index(u32 pid);
u32 process64_manifest_package_id(u32 pid);
u32 process64_manifest_executable_id(u32 pid);
u32 process64_manifest_signer_id(u32 pid);
u32 process64_manifest_token(u32 pid);
u32 process64_runtime_generation(u32 pid);
u32 process64_runtime_token(u32 pid);
u32 process64_runtime_image_generation(u32 pid);
u32 process64_runtime_image_token(u32 pid);
u32 process64_runtime_image_base(u32 pid);
u32 process64_runtime_image_entry(u32 pid);
u32 process64_runtime_image_mapped_bytes(u32 pid);
u32 process64_runtime_image_rights(u32 pid);
u32 process64_runtime_image_plan_token(u32 pid);
u32 process64_runtime_image_map_token(u32 pid);
u32 process64_runtime_image_page_count(u32 pid);
u32 process64_runtime_image_pml4_index(u32 pid);
u32 process64_runtime_image_pdpt_index(u32 pid);
u32 process64_runtime_image_pd_index(u32 pid);
u32 process64_runtime_entry_transfer_token(u32 pid);
u32 process64_runtime_image_install_token(u32 pid);
u32 process64_runtime_image_source_checksum(u32 pid);
u32 process64_runtime_image_entry_probe(u32 pid);
u32 process64_runtime_image_map_installed(u32 pid);
u32 process64_runtime_image_protection_flags(u32 pid);
u32 process64_runtime_image_protection_token(u32 pid);
u32 process64_runtime_user_entry_state(u32 pid);
u32 process64_runtime_user_entry_token(u32 pid);
u32 process64_runtime_user_entry_rip(u32 pid);
u32 process64_runtime_user_entry_rsp(u32 pid);
u32 process64_runtime_user_entry_selectors(u32 pid);
u32 process64_runtime_user_entry_rflags(u32 pid);
u32 process64_runtime_user_entry_denial(u32 pid);
u32 process64_runtime_user_entry_ready(u32 pid);
u32 process64_runtime_payload_offset(u32 pid);
u32 process64_runtime_payload_size(u32 pid);
u32 process64_runtime_payload_checksum(u32 pid);
u32 process64_manifest_verified_count(void);
const char *process64_name(u32 pid);
u32 process64_attach_vma(u32 pid, void *vma_root);
void *process64_detach_vma(u32 pid);
void *process64_vma_root(u32 pid);
u32 process64_attach_fd(u32 pid, void *fd_table);
void *process64_detach_fd(u32 pid);
void *process64_fd_table(u32 pid);
u32 process64_attach_persona(u32 pid, void *persona_ctx);
void *process64_detach_persona(u32 pid);
void *process64_persona_ctx(u32 pid);
u32 process64_attach_audit(u32 pid, void *audit_ctx);
void *process64_detach_audit(u32 pid);
void *process64_audit_ctx(u32 pid);
u32 process64_attach_page_root(
    u32 pid,
    u64 root_physical,
    u32 root_index,
    u32 root_token,
    u32 authority_token);
u32 process64_clear_page_root(u32 pid, u32 root_token);
u64 process64_page_root_physical(u32 pid);
u32 process64_page_root_index(u32 pid);
u32 process64_page_root_token(u32 pid);
u32 process64_page_root_attach_count(void);
u32 process64_page_root_clear_count(void);
u32 process64_page_root_denial_count(void);
u32 process64_spawn_clone(u32 parent_pid);
u32 process64_spawn_fork(u32 parent_pid);
u32 process64_parent_pid(u32 pid);
u32 process64_child_kind(u32 pid);
u32 process64_child_count(u32 pid);
u32 process64_mark_child_exited(u32 pid, u32 exit_code);
u32 process64_child_exited(u32 pid);
u32 process64_child_exit_code(u32 pid);
u32 process64_release_clone(u32 pid);
u32 process64_is_clone(u32 pid);
u32 process64_is_fork_child(u32 pid);
u32 process64_clone_count(void);

#endif
