#include "identity_x64.h"

#include "auth_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

static u32 g_identity64_foundation = 0u;
static u32 g_identity64_local_active = 0u;
static u32 g_identity64_status_readonly = 0u;
static u32 g_identity64_mutation_denied = 0u;
static u32 g_identity64_vault_foundation = 0u;
static u32 g_identity64_vault_secret_read_denied = 0u;
static u32 g_identity64_vault_secret_write_denied = 0u;
static u32 g_identity64_vault_no_plaintext_token = 0u;
static u32 g_identity64_no_ambient_identity = 0u;
static u32 g_identity64_no_ambient_secret = 0u;

void identity64_init(void)
{
    g_identity64_foundation = 1u;
    g_identity64_local_active = (auth64_auth_success() != 0u) ? 1u : 0u;
    g_identity64_status_readonly = 1u;
    g_identity64_mutation_denied = 1u;
    g_identity64_vault_foundation = 1u;
    g_identity64_vault_secret_read_denied = 1u;
    g_identity64_vault_secret_write_denied = 1u;
    g_identity64_vault_no_plaintext_token = 1u;
    g_identity64_no_ambient_identity = 1u;
    g_identity64_no_ambient_secret = 1u;
}

u32 identity64_foundation(void) { return g_identity64_foundation; }
u32 identity64_local_active(void) { return g_identity64_local_active; }
u32 identity64_personal_unavailable(void) { return 1u; }
u32 identity64_enterprise_unavailable(void) { return 1u; }
u32 identity64_status_readonly(void) { return g_identity64_status_readonly; }
u32 identity64_mutation_denied(void) { return g_identity64_mutation_denied; }
u32 identity64_cloud_association_unavailable(void) { return 1u; }
u32 identity64_no_ambient_identity(void) { return g_identity64_no_ambient_identity; }
u32 identity64_vault_foundation(void) { return g_identity64_vault_foundation; }
u32 identity64_vault_secret_read_denied(void) { return g_identity64_vault_secret_read_denied; }
u32 identity64_vault_secret_write_denied(void) { return g_identity64_vault_secret_write_denied; }
u32 identity64_vault_no_plaintext_token(void) { return g_identity64_vault_no_plaintext_token; }
u32 identity64_no_ambient_secret(void) { return g_identity64_no_ambient_secret; }
u32 identity64_encrypted_vault_available(void) { return 0u; }
u32 identity64_real_secret_storage_enabled(void) { return 0u; }

const char *identity64_active_account_type(void) { return "local"; }
const char *identity64_active_account_id(void) { return "local:limitless"; }
const char *identity64_display_name(void) { return auth64_active_user(); }
const char *identity64_association_status(void) { return "local-active"; }
const char *identity64_offline_online_status(void) { return "offline-capable"; }
const char *identity64_credential_record_type(void) { return "bcrypt-local"; }
const char *identity64_vault_binding_status(void) { return "metadata-only"; }

#else

void identity64_init(void) {}
u32 identity64_foundation(void) { return 0u; }
u32 identity64_local_active(void) { return 0u; }
u32 identity64_personal_unavailable(void) { return 1u; }
u32 identity64_enterprise_unavailable(void) { return 1u; }
u32 identity64_status_readonly(void) { return 1u; }
u32 identity64_mutation_denied(void) { return 1u; }
u32 identity64_cloud_association_unavailable(void) { return 1u; }
u32 identity64_no_ambient_identity(void) { return 1u; }
u32 identity64_vault_foundation(void) { return 0u; }
u32 identity64_vault_secret_read_denied(void) { return 1u; }
u32 identity64_vault_secret_write_denied(void) { return 1u; }
u32 identity64_vault_no_plaintext_token(void) { return 1u; }
u32 identity64_no_ambient_secret(void) { return 1u; }
u32 identity64_encrypted_vault_available(void) { return 0u; }
u32 identity64_real_secret_storage_enabled(void) { return 0u; }
const char *identity64_active_account_type(void) { return "unavailable"; }
const char *identity64_active_account_id(void) { return "bios-fallback"; }
const char *identity64_display_name(void) { return ""; }
const char *identity64_association_status(void) { return "unavailable"; }
const char *identity64_offline_online_status(void) { return "unavailable"; }
const char *identity64_credential_record_type(void) { return "unavailable"; }
const char *identity64_vault_binding_status(void) { return "unavailable"; }

#endif
