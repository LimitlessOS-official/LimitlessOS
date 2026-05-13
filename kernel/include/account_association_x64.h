#ifndef LIMITLESS_ACCOUNT_ASSOCIATION_X64_H
#define LIMITLESS_ACCOUNT_ASSOCIATION_X64_H

#include "types.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
void account_association64_init(void);
u32 account_association64_product(void);
u32 account_association64_local_active(void);
u32 account_association64_personal_unavailable(void);
u32 account_association64_enterprise_unavailable(void);
u32 account_association64_cloud_unavailable(void);
u32 account_association64_security_key_unavailable(void);
u32 account_association64_status_readonly(void);
u32 account_association64_mutation_denied(void);
u32 account_association64_unlink_denied(void);
u32 account_association64_token_storage_denied(void);
u32 account_association64_credential_transport_denied(void);
u32 account_association64_enterprise_policy_unavailable(void);
u32 account_association64_remote_no_ambient_authority(void);
u32 account_association64_no_ambient_identity(void);
u32 account_association64_no_ambient_network(void);
u32 account_association64_no_ambient_secret(void);
const char *account_association64_mode(void);
const char *account_association64_local_status(void);
const char *account_association64_personal_status(void);
const char *account_association64_enterprise_status(void);
const char *account_association64_cloud_status(void);
const char *account_association64_security_key_status(void);
const char *account_association64_enterprise_policy_status(void);
const char *account_association64_encrypted_transport_status(void);
const char *account_association64_token_storage_status(void);
const char *account_association64_trusted_time_status(void);
const char *account_association64_remote_login_status(void);
const char *account_association64_local_user_id(void);
const char *account_association64_provider_id(void);
#else
static inline void account_association64_init(void) {}
static inline u32 account_association64_product(void) { return 0u; }
static inline u32 account_association64_local_active(void) { return 0u; }
static inline u32 account_association64_personal_unavailable(void) { return 1u; }
static inline u32 account_association64_enterprise_unavailable(void) { return 1u; }
static inline u32 account_association64_cloud_unavailable(void) { return 1u; }
static inline u32 account_association64_security_key_unavailable(void) { return 1u; }
static inline u32 account_association64_status_readonly(void) { return 1u; }
static inline u32 account_association64_mutation_denied(void) { return 1u; }
static inline u32 account_association64_unlink_denied(void) { return 1u; }
static inline u32 account_association64_token_storage_denied(void) { return 1u; }
static inline u32 account_association64_credential_transport_denied(void) { return 1u; }
static inline u32 account_association64_enterprise_policy_unavailable(void) { return 1u; }
static inline u32 account_association64_remote_no_ambient_authority(void) { return 1u; }
static inline u32 account_association64_no_ambient_identity(void) { return 1u; }
static inline u32 account_association64_no_ambient_network(void) { return 1u; }
static inline u32 account_association64_no_ambient_secret(void) { return 1u; }
static inline const char *account_association64_mode(void) { return "unavailable"; }
static inline const char *account_association64_local_status(void) { return "unavailable"; }
static inline const char *account_association64_personal_status(void) { return "unavailable"; }
static inline const char *account_association64_enterprise_status(void) { return "unavailable"; }
static inline const char *account_association64_cloud_status(void) { return "unavailable"; }
static inline const char *account_association64_security_key_status(void) { return "unavailable"; }
static inline const char *account_association64_enterprise_policy_status(void) { return "unavailable"; }
static inline const char *account_association64_encrypted_transport_status(void) { return "unavailable"; }
static inline const char *account_association64_token_storage_status(void) { return "denied"; }
static inline const char *account_association64_trusted_time_status(void) { return "unavailable"; }
static inline const char *account_association64_remote_login_status(void) { return "unavailable"; }
static inline const char *account_association64_local_user_id(void) { return "unavailable"; }
static inline const char *account_association64_provider_id(void) { return "unavailable"; }
#endif

#endif
