#ifndef LIMITLESS_AI_POLICY_X64_H
#define LIMITLESS_AI_POLICY_X64_H

#include "types.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
void ai_policy64_init(void);
u32 ai_policy64_principal(void);
u32 ai_policy64_request_created(void);
u32 ai_policy64_consent_required(void);
u32 ai_policy64_denied_no_consent(void);
u32 ai_policy64_scope_validated(void);
u32 ai_policy64_invalid_scope_denied(void);
u32 ai_policy64_audit_recorded(void);
u32 ai_policy64_settings_panel(void);
u32 ai_policy64_settings_readonly(void);
u32 ai_policy64_no_ambient_authority(void);
u32 ai_policy64_no_filesystem_access(void);
u32 ai_policy64_no_network_access(void);
u32 ai_policy64_no_settings_access(void);
u32 ai_policy64_no_package_access(void);
u32 ai_policy64_no_secret_access(void);
u32 ai_policy64_no_cloud_access(void);
u32 ai_policy64_actions_executed(void);
u32 ai_policy64_default_capabilities(void);
u32 ai_policy64_audit_record_count(void);
u32 ai_policy64_principal_id(void);
u32 ai_policy64_request_id(void);
const char *ai_policy64_status(void);
const char *ai_policy64_principal_status(void);
const char *ai_policy64_request_action(void);
const char *ai_policy64_request_resource(void);
const char *ai_policy64_request_capability(void);
const char *ai_policy64_request_scope(void);
const char *ai_policy64_decision(void);
const char *ai_policy64_result(void);
const char *ai_policy64_assistant_status(void);
const char *ai_policy64_automation_status(void);
const char *ai_policy64_cloud_status(void);
#else
static inline void ai_policy64_init(void) {}
static inline u32 ai_policy64_principal(void) { return 0u; }
static inline u32 ai_policy64_request_created(void) { return 0u; }
static inline u32 ai_policy64_consent_required(void) { return 1u; }
static inline u32 ai_policy64_denied_no_consent(void) { return 1u; }
static inline u32 ai_policy64_scope_validated(void) { return 0u; }
static inline u32 ai_policy64_invalid_scope_denied(void) { return 1u; }
static inline u32 ai_policy64_audit_recorded(void) { return 0u; }
static inline u32 ai_policy64_settings_panel(void) { return 0u; }
static inline u32 ai_policy64_settings_readonly(void) { return 1u; }
static inline u32 ai_policy64_no_ambient_authority(void) { return 1u; }
static inline u32 ai_policy64_no_filesystem_access(void) { return 1u; }
static inline u32 ai_policy64_no_network_access(void) { return 1u; }
static inline u32 ai_policy64_no_settings_access(void) { return 1u; }
static inline u32 ai_policy64_no_package_access(void) { return 1u; }
static inline u32 ai_policy64_no_secret_access(void) { return 1u; }
static inline u32 ai_policy64_no_cloud_access(void) { return 1u; }
static inline u32 ai_policy64_actions_executed(void) { return 0u; }
static inline u32 ai_policy64_default_capabilities(void) { return 0u; }
static inline u32 ai_policy64_audit_record_count(void) { return 0u; }
static inline u32 ai_policy64_principal_id(void) { return 0u; }
static inline u32 ai_policy64_request_id(void) { return 0u; }
static inline const char *ai_policy64_status(void) { return "unavailable-bios-fallback"; }
static inline const char *ai_policy64_principal_status(void) { return "unavailable"; }
static inline const char *ai_policy64_request_action(void) { return "none"; }
static inline const char *ai_policy64_request_resource(void) { return "none"; }
static inline const char *ai_policy64_request_capability(void) { return "none"; }
static inline const char *ai_policy64_request_scope(void) { return "none"; }
static inline const char *ai_policy64_decision(void) { return "deny"; }
static inline const char *ai_policy64_result(void) { return "unavailable"; }
static inline const char *ai_policy64_assistant_status(void) { return "unavailable"; }
static inline const char *ai_policy64_automation_status(void) { return "unavailable"; }
static inline const char *ai_policy64_cloud_status(void) { return "unavailable"; }
#endif

#endif
