#include "account_association_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

enum account_association64_type
{
    ACCOUNT_ASSOCIATION64_LOCAL = 1,
    ACCOUNT_ASSOCIATION64_PERSONAL = 2,
    ACCOUNT_ASSOCIATION64_ENTERPRISE = 3
};

struct account_association64_record
{
    const char *association_id;
    const char *local_user_id;
    enum account_association64_type account_type;
    const char *provider_id;
    const char *provider_descriptor_id;
    const char *association_status;
    const char *online_status;
    const char *credential_transport_status;
    const char *token_storage_status;
    const char *security_key_status;
    const char *cloud_storage_status;
    const char *enterprise_policy_status;
    const char *last_verification_result;
    u32 created_generation;
    u32 revoked;
    u32 unlinked;
};

static u32 g_account_association_initialized = 0u;

static struct account_association64_record g_account_associations[3] = {
    {
        "assoc-local-limitless",
        "local:limitless",
        ACCOUNT_ASSOCIATION64_LOCAL,
        "local",
        "local-descriptor",
        "active",
        "offline-capable",
        "local-bcrypt",
        "none",
        "unavailable",
        "unavailable",
        "unavailable",
        "verified-local",
        13u,
        0u,
        0u
    },
    {
        "assoc-personal-planned",
        "local:limitless",
        ACCOUNT_ASSOCIATION64_PERSONAL,
        "personal.fixture.limitless",
        "m12-provider-descriptor",
        "planned-unavailable",
        "offline-fixture",
        "denied",
        "denied",
        "unavailable",
        "unavailable",
        "unavailable",
        "blocked-mode-b",
        13u,
        0u,
        0u
    },
    {
        "assoc-enterprise-planned",
        "local:limitless",
        ACCOUNT_ASSOCIATION64_ENTERPRISE,
        "enterprise.fixture.limitless",
        "m12-provider-descriptor",
        "planned-unavailable",
        "offline-fixture",
        "denied",
        "denied",
        "unavailable",
        "unavailable",
        "unavailable",
        "blocked-mode-b",
        13u,
        0u,
        0u
    }
};

void account_association64_init(void)
{
    g_account_association_initialized = 1u;
}

u32 account_association64_product(void) { account_association64_init(); return g_account_association_initialized; }
u32 account_association64_local_active(void) { account_association64_init(); return 1u; }
u32 account_association64_personal_unavailable(void) { account_association64_init(); return 1u; }
u32 account_association64_enterprise_unavailable(void) { account_association64_init(); return 1u; }
u32 account_association64_cloud_unavailable(void) { account_association64_init(); return 1u; }
u32 account_association64_security_key_unavailable(void) { account_association64_init(); return 1u; }
u32 account_association64_status_readonly(void) { account_association64_init(); return 1u; }
u32 account_association64_mutation_denied(void) { account_association64_init(); return 1u; }
u32 account_association64_unlink_denied(void) { account_association64_init(); return 1u; }
u32 account_association64_token_storage_denied(void) { account_association64_init(); return 1u; }
u32 account_association64_credential_transport_denied(void) { account_association64_init(); return 1u; }
u32 account_association64_enterprise_policy_unavailable(void) { account_association64_init(); return 1u; }
u32 account_association64_remote_no_ambient_authority(void) { account_association64_init(); return 1u; }
u32 account_association64_no_ambient_identity(void) { account_association64_init(); return 1u; }
u32 account_association64_no_ambient_network(void) { account_association64_init(); return 1u; }
u32 account_association64_no_ambient_secret(void) { account_association64_init(); return 1u; }

const char *account_association64_mode(void) { return "mode-b-status-only"; }
const char *account_association64_local_status(void) { return g_account_associations[0].association_status; }
const char *account_association64_personal_status(void) { return g_account_associations[1].association_status; }
const char *account_association64_enterprise_status(void) { return g_account_associations[2].association_status; }
const char *account_association64_cloud_status(void) { return "planned-unavailable"; }
const char *account_association64_security_key_status(void) { return "planned-unavailable"; }
const char *account_association64_enterprise_policy_status(void) { return g_account_associations[2].enterprise_policy_status; }
const char *account_association64_encrypted_transport_status(void) { return "unavailable"; }
const char *account_association64_token_storage_status(void) { return "denied"; }
const char *account_association64_trusted_time_status(void) { return "unavailable"; }
const char *account_association64_remote_login_status(void) { return "unavailable"; }
const char *account_association64_local_user_id(void) { return g_account_associations[0].local_user_id; }
const char *account_association64_provider_id(void) { return g_account_associations[1].provider_id; }

#endif
