#ifndef LIMITLESS_IDENTITY_TRANSPORT_X64_H
#define LIMITLESS_IDENTITY_TRANSPORT_X64_H

#include "types.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
void identity_transport64_init(void);
u32 identity_transport64_product(void);
u32 identity_transport64_provider_descriptor(void);
u32 identity_transport64_descriptor_verified(void);
u32 identity_transport64_descriptor_missing_sig_denied(void);
u32 identity_transport64_descriptor_invalid_sig_denied(void);
u32 identity_transport64_descriptor_wrong_key_denied(void);
u32 identity_transport64_descriptor_tamper_denied(void);
u32 identity_transport64_descriptor_rollback_denied(void);
u32 identity_transport64_descriptor_version_denied(void);
u32 identity_transport64_network_scoped(void);
u32 identity_transport64_no_network_cap_denied(void);
u32 identity_transport64_plaintext_credential_denied(void);
u32 identity_transport64_unverified_endpoint_denied(void);
u32 identity_transport64_token_storage_denied(void);
u32 identity_transport64_personal_unavailable(void);
u32 identity_transport64_enterprise_unavailable(void);
u32 identity_transport64_cloud_association_unavailable(void);
u32 identity_transport64_status_readonly(void);
u32 identity_transport64_trusted_time_status(void);
u32 identity_transport64_no_ambient_network(void);
u32 identity_transport64_no_ambient_identity(void);
u32 identity_transport64_no_ambient_secret(void);
u32 identity_transport64_encrypted_channel_unavailable(void);
u32 identity_transport64_credential_transport_unavailable(void);
const char *identity_transport64_mode(void);
const char *identity_transport64_provider_id(void);
const char *identity_transport64_provider_type(void);
const char *identity_transport64_endpoint_status(void);
const char *identity_transport64_online_status(void);
const char *identity_transport64_encrypted_transport_status(void);
const char *identity_transport64_credential_transport_status(void);
const char *identity_transport64_token_storage_status(void);
const char *identity_transport64_trusted_time_string(void);
#else
static inline void identity_transport64_init(void) {}
static inline u32 identity_transport64_product(void) { return 0u; }
static inline u32 identity_transport64_provider_descriptor(void) { return 0u; }
static inline u32 identity_transport64_descriptor_verified(void) { return 0u; }
static inline u32 identity_transport64_descriptor_missing_sig_denied(void) { return 0u; }
static inline u32 identity_transport64_descriptor_invalid_sig_denied(void) { return 0u; }
static inline u32 identity_transport64_descriptor_wrong_key_denied(void) { return 0u; }
static inline u32 identity_transport64_descriptor_tamper_denied(void) { return 0u; }
static inline u32 identity_transport64_descriptor_rollback_denied(void) { return 0u; }
static inline u32 identity_transport64_descriptor_version_denied(void) { return 0u; }
static inline u32 identity_transport64_network_scoped(void) { return 0u; }
static inline u32 identity_transport64_no_network_cap_denied(void) { return 1u; }
static inline u32 identity_transport64_plaintext_credential_denied(void) { return 1u; }
static inline u32 identity_transport64_unverified_endpoint_denied(void) { return 1u; }
static inline u32 identity_transport64_token_storage_denied(void) { return 1u; }
static inline u32 identity_transport64_personal_unavailable(void) { return 1u; }
static inline u32 identity_transport64_enterprise_unavailable(void) { return 1u; }
static inline u32 identity_transport64_cloud_association_unavailable(void) { return 1u; }
static inline u32 identity_transport64_status_readonly(void) { return 1u; }
static inline u32 identity_transport64_trusted_time_status(void) { return 1u; }
static inline u32 identity_transport64_no_ambient_network(void) { return 1u; }
static inline u32 identity_transport64_no_ambient_identity(void) { return 1u; }
static inline u32 identity_transport64_no_ambient_secret(void) { return 1u; }
static inline u32 identity_transport64_encrypted_channel_unavailable(void) { return 1u; }
static inline u32 identity_transport64_credential_transport_unavailable(void) { return 1u; }
static inline const char *identity_transport64_mode(void) { return "unavailable"; }
static inline const char *identity_transport64_provider_id(void) { return "unavailable"; }
static inline const char *identity_transport64_provider_type(void) { return "unavailable"; }
static inline const char *identity_transport64_endpoint_status(void) { return "unavailable"; }
static inline const char *identity_transport64_online_status(void) { return "unavailable"; }
static inline const char *identity_transport64_encrypted_transport_status(void) { return "unavailable"; }
static inline const char *identity_transport64_credential_transport_status(void) { return "denied"; }
static inline const char *identity_transport64_token_storage_status(void) { return "denied"; }
static inline const char *identity_transport64_trusted_time_string(void) { return "unavailable"; }
#endif

#endif
