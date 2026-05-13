#ifndef LIMITLESS_CLOUD_STORAGE_X64_H
#define LIMITLESS_CLOUD_STORAGE_X64_H

#include "types.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
void cloud_storage64_init(void);
u32 cloud_storage64_broker_product(void);
u32 cloud_storage64_provider_descriptor(void);
u32 cloud_storage64_provider_verified(void);
u32 cloud_storage64_provider_missing_sig_denied(void);
u32 cloud_storage64_provider_invalid_sig_denied(void);
u32 cloud_storage64_provider_wrong_key_denied(void);
u32 cloud_storage64_provider_tamper_denied(void);
u32 cloud_storage64_provider_rollback_denied(void);
u32 cloud_storage64_provider_version_denied(void);
u32 cloud_storage64_provider_malformed_denied(void);
u32 cloud_storage64_association_unavailable(void);
u32 cloud_storage64_account_unavailable(void);
u32 cloud_storage64_token_storage_denied(void);
u32 cloud_storage64_encrypted_transport_unavailable(void);
u32 cloud_storage64_upload_denied(void);
u32 cloud_storage64_download_denied(void);
u32 cloud_storage64_sync_denied(void);
u32 cloud_storage64_auto_upload_unavailable(void);
u32 cloud_storage64_auto_download_unavailable(void);
u32 cloud_storage64_ai_access_unavailable(void);
u32 cloud_storage64_app_direct_denied(void);
u32 cloud_storage64_settings_readonly(void);
u32 cloud_storage64_settings_mutation_denied(void);
u32 cloud_storage64_fileman_status_readonly(void);
u32 cloud_storage64_fileman_mutation_denied(void);
u32 cloud_storage64_no_ambient_cloud(void);
u32 cloud_storage64_no_ambient_fs(void);
u32 cloud_storage64_no_ambient_network(void);
u32 cloud_storage64_no_ambient_identity(void);
u32 cloud_storage64_no_ambient_secret(void);
const char *cloud_storage64_broker_status(void);
const char *cloud_storage64_mode(void);
const char *cloud_storage64_provider_id(void);
const char *cloud_storage64_descriptor_status(void);
const char *cloud_storage64_signature_status(void);
const char *cloud_storage64_anti_rollback_status(void);
const char *cloud_storage64_account_status(void);
const char *cloud_storage64_association_status(void);
const char *cloud_storage64_token_storage_status(void);
const char *cloud_storage64_encrypted_transport_status(void);
const char *cloud_storage64_sync_status(void);
const char *cloud_storage64_upload_status(void);
const char *cloud_storage64_download_status(void);
const char *cloud_storage64_offline_cache_status(void);
const char *cloud_storage64_ai_access_status(void);
const char *cloud_storage64_app_direct_status(void);
#else
static inline void cloud_storage64_init(void) {}
static inline u32 cloud_storage64_broker_product(void) { return 0u; }
static inline u32 cloud_storage64_provider_descriptor(void) { return 0u; }
static inline u32 cloud_storage64_provider_verified(void) { return 0u; }
static inline u32 cloud_storage64_provider_missing_sig_denied(void) { return 0u; }
static inline u32 cloud_storage64_provider_invalid_sig_denied(void) { return 0u; }
static inline u32 cloud_storage64_provider_wrong_key_denied(void) { return 0u; }
static inline u32 cloud_storage64_provider_tamper_denied(void) { return 0u; }
static inline u32 cloud_storage64_provider_rollback_denied(void) { return 0u; }
static inline u32 cloud_storage64_provider_version_denied(void) { return 0u; }
static inline u32 cloud_storage64_provider_malformed_denied(void) { return 0u; }
static inline u32 cloud_storage64_association_unavailable(void) { return 1u; }
static inline u32 cloud_storage64_account_unavailable(void) { return 1u; }
static inline u32 cloud_storage64_token_storage_denied(void) { return 1u; }
static inline u32 cloud_storage64_encrypted_transport_unavailable(void) { return 1u; }
static inline u32 cloud_storage64_upload_denied(void) { return 1u; }
static inline u32 cloud_storage64_download_denied(void) { return 1u; }
static inline u32 cloud_storage64_sync_denied(void) { return 1u; }
static inline u32 cloud_storage64_auto_upload_unavailable(void) { return 1u; }
static inline u32 cloud_storage64_auto_download_unavailable(void) { return 1u; }
static inline u32 cloud_storage64_ai_access_unavailable(void) { return 1u; }
static inline u32 cloud_storage64_app_direct_denied(void) { return 1u; }
static inline u32 cloud_storage64_settings_readonly(void) { return 1u; }
static inline u32 cloud_storage64_settings_mutation_denied(void) { return 1u; }
static inline u32 cloud_storage64_fileman_status_readonly(void) { return 1u; }
static inline u32 cloud_storage64_fileman_mutation_denied(void) { return 1u; }
static inline u32 cloud_storage64_no_ambient_cloud(void) { return 1u; }
static inline u32 cloud_storage64_no_ambient_fs(void) { return 1u; }
static inline u32 cloud_storage64_no_ambient_network(void) { return 1u; }
static inline u32 cloud_storage64_no_ambient_identity(void) { return 1u; }
static inline u32 cloud_storage64_no_ambient_secret(void) { return 1u; }
static inline const char *cloud_storage64_broker_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_mode(void) { return "unavailable"; }
static inline const char *cloud_storage64_provider_id(void) { return "unavailable"; }
static inline const char *cloud_storage64_descriptor_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_signature_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_anti_rollback_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_account_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_association_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_token_storage_status(void) { return "denied"; }
static inline const char *cloud_storage64_encrypted_transport_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_sync_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_upload_status(void) { return "denied"; }
static inline const char *cloud_storage64_download_status(void) { return "denied"; }
static inline const char *cloud_storage64_offline_cache_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_ai_access_status(void) { return "unavailable"; }
static inline const char *cloud_storage64_app_direct_status(void) { return "denied"; }
#endif

#endif
