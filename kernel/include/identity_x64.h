#ifndef LIMITLESS_IDENTITY_X64_H
#define LIMITLESS_IDENTITY_X64_H

#include "types.h"

void identity64_init(void);

u32 identity64_foundation(void);
u32 identity64_local_active(void);
u32 identity64_personal_unavailable(void);
u32 identity64_enterprise_unavailable(void);
u32 identity64_status_readonly(void);
u32 identity64_mutation_denied(void);
u32 identity64_cloud_association_unavailable(void);
u32 identity64_no_ambient_identity(void);

u32 identity64_vault_foundation(void);
u32 identity64_vault_secret_read_denied(void);
u32 identity64_vault_secret_write_denied(void);
u32 identity64_vault_no_plaintext_token(void);
u32 identity64_no_ambient_secret(void);
u32 identity64_encrypted_vault_available(void);
u32 identity64_real_secret_storage_enabled(void);

const char *identity64_active_account_type(void);
const char *identity64_active_account_id(void);
const char *identity64_display_name(void);
const char *identity64_association_status(void);
const char *identity64_offline_online_status(void);
const char *identity64_credential_record_type(void);
const char *identity64_vault_binding_status(void);

#endif
