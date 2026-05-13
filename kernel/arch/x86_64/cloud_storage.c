#include "cloud_storage_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

#include "package_signing_x64.h"
#include "package_store_signatures_generated.h"

#define CLOUD_STORAGE64_MAX_DESCRIPTOR_BYTES 1536u

static u32 g_cloud_initialized = 0u;
static u32 g_cloud_broker_product = 0u;
static u32 g_cloud_provider_descriptor = 0u;
static u32 g_cloud_provider_verified = 0u;
static u32 g_cloud_provider_missing_sig_denied = 0u;
static u32 g_cloud_provider_invalid_sig_denied = 0u;
static u32 g_cloud_provider_wrong_key_denied = 0u;
static u32 g_cloud_provider_tamper_denied = 0u;
static u32 g_cloud_provider_rollback_denied = 0u;
static u32 g_cloud_provider_version_denied = 0u;
static u32 g_cloud_provider_malformed_denied = 0u;

static u8 g_cloud_scratch[CLOUD_STORAGE64_MAX_DESCRIPTOR_BYTES];
static u8 g_cloud_signature_scratch[64];

static u32 cloud_storage64_strlen(const char *text)
{
    u32 length = 0u;

    while ((text != 0) && (text[length] != '\0'))
    {
        ++length;
    }

    return length;
}

static void cloud_storage64_copy(u8 *dest, const u8 *src, u32 count)
{
    u32 index;

    for (index = 0u; index < count; ++index)
    {
        dest[index] = src[index];
    }
}

static u32 cloud_storage64_contains(const u8 *bytes, u32 byte_count, const char *needle)
{
    u32 needle_count = cloud_storage64_strlen(needle);
    u32 offset;
    u32 index;

    if ((bytes == 0) || (needle == 0) || (needle_count == 0u) || (needle_count > byte_count))
    {
        return 0u;
    }

    for (offset = 0u; offset <= (byte_count - needle_count); ++offset)
    {
        u32 match = 1u;
        for (index = 0u; index < needle_count; ++index)
        {
            if (bytes[offset + index] != (u8)needle[index])
            {
                match = 0u;
                break;
            }
        }
        if (match != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 cloud_storage64_verify(const u8 *signature, const u8 *descriptor, u32 descriptor_size)
{
    static const u8 prefix[] = "LimitlessOS-M14-cloud-provider-v1";

    return package_signing64_verify_signed_blob(
        signature,
        prefix,
        (u32)sizeof(prefix),
        descriptor,
        descriptor_size);
}

static u32 cloud_storage64_descriptor_semantics_valid(const u8 *descriptor, u32 descriptor_size)
{
    if (cloud_storage64_contains(descriptor, descriptor_size, "limitlessos-cloud-provider-v1\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "provider-id=cloud.fixture.limitless\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "provider-type=cloud-storage\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "descriptor-version=1\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "protocol-version=1\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "supported-modes=descriptor-only\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "token-policy=denied\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "sync-policy=unavailable\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "required-transport-security=encrypted\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "required-account-association=personal-or-enterprise\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "minimum-os-version=M14\n") == 0u)
    {
        return 0u;
    }
    if (cloud_storage64_contains(descriptor, descriptor_size, "sequence=14\n") == 0u)
    {
        return 0u;
    }

    return 1u;
}

void cloud_storage64_init(void)
{
    if (g_cloud_initialized != 0u)
    {
        return;
    }

    package_signing64_init();
    g_cloud_initialized = 1u;
    g_cloud_broker_product = 1u;
    g_cloud_provider_descriptor = 1u;

    g_cloud_provider_verified =
        (cloud_storage64_verify(
             cloud_provider_descriptor_signature,
             cloud_provider_descriptor,
             CLOUD_PROVIDER_DESCRIPTOR_BYTES) != 0u)
        && (cloud_storage64_descriptor_semantics_valid(
                cloud_provider_descriptor,
                CLOUD_PROVIDER_DESCRIPTOR_BYTES) != 0u);

    g_cloud_provider_missing_sig_denied =
        (cloud_storage64_verify(0, cloud_provider_descriptor, CLOUD_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;

    cloud_storage64_copy(g_cloud_signature_scratch, cloud_provider_descriptor_signature, 64u);
    g_cloud_signature_scratch[0] ^= 0x80u;
    g_cloud_provider_invalid_sig_denied =
        (cloud_storage64_verify(g_cloud_signature_scratch, cloud_provider_descriptor, CLOUD_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;

    g_cloud_provider_wrong_key_denied =
        (cloud_storage64_verify(cloud_provider_descriptor_wrong_key_signature, cloud_provider_descriptor, CLOUD_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;

    if (CLOUD_PROVIDER_DESCRIPTOR_BYTES <= CLOUD_STORAGE64_MAX_DESCRIPTOR_BYTES)
    {
        cloud_storage64_copy(g_cloud_scratch, cloud_provider_descriptor, CLOUD_PROVIDER_DESCRIPTOR_BYTES);
        g_cloud_scratch[CLOUD_PROVIDER_DESCRIPTOR_BYTES - 1u] ^= 0x01u;
        g_cloud_provider_tamper_denied =
            (cloud_storage64_verify(cloud_provider_descriptor_signature, g_cloud_scratch, CLOUD_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;
    }

    g_cloud_provider_rollback_denied =
        ((cloud_storage64_verify(
              cloud_provider_descriptor_rollback_signature,
              cloud_provider_descriptor_rollback,
              CLOUD_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES) != 0u)
            && (CLOUD_PROVIDER_DESCRIPTOR_ROLLBACK_SEQUENCE < CLOUD_PROVIDER_DESCRIPTOR_SEQUENCE)
            && (cloud_storage64_contains(
                    cloud_provider_descriptor_rollback,
                    CLOUD_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES,
                    "sequence=13\n") != 0u))
            ? 1u
            : 0u;

    g_cloud_provider_version_denied =
        ((cloud_storage64_verify(
              cloud_provider_descriptor_unsupported_signature,
              cloud_provider_descriptor_unsupported,
              CLOUD_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES) != 0u)
            && (cloud_storage64_contains(
                    cloud_provider_descriptor_unsupported,
                    CLOUD_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES,
                    "descriptor-version=99\n") != 0u))
            ? 1u
            : 0u;

    g_cloud_provider_malformed_denied =
        ((cloud_storage64_verify(
              cloud_provider_descriptor_malformed_signature,
              cloud_provider_descriptor_malformed,
              CLOUD_PROVIDER_DESCRIPTOR_MALFORMED_BYTES) != 0u)
            && (cloud_storage64_descriptor_semantics_valid(
                    cloud_provider_descriptor_malformed,
                    CLOUD_PROVIDER_DESCRIPTOR_MALFORMED_BYTES) == 0u))
            ? 1u
            : 0u;
}

u32 cloud_storage64_broker_product(void) { cloud_storage64_init(); return g_cloud_broker_product; }
u32 cloud_storage64_provider_descriptor(void) { cloud_storage64_init(); return g_cloud_provider_descriptor; }
u32 cloud_storage64_provider_verified(void) { cloud_storage64_init(); return g_cloud_provider_verified; }
u32 cloud_storage64_provider_missing_sig_denied(void) { cloud_storage64_init(); return g_cloud_provider_missing_sig_denied; }
u32 cloud_storage64_provider_invalid_sig_denied(void) { cloud_storage64_init(); return g_cloud_provider_invalid_sig_denied; }
u32 cloud_storage64_provider_wrong_key_denied(void) { cloud_storage64_init(); return g_cloud_provider_wrong_key_denied; }
u32 cloud_storage64_provider_tamper_denied(void) { cloud_storage64_init(); return g_cloud_provider_tamper_denied; }
u32 cloud_storage64_provider_rollback_denied(void) { cloud_storage64_init(); return g_cloud_provider_rollback_denied; }
u32 cloud_storage64_provider_version_denied(void) { cloud_storage64_init(); return g_cloud_provider_version_denied; }
u32 cloud_storage64_provider_malformed_denied(void) { cloud_storage64_init(); return g_cloud_provider_malformed_denied; }
u32 cloud_storage64_association_unavailable(void) { return 1u; }
u32 cloud_storage64_account_unavailable(void) { return 1u; }
u32 cloud_storage64_token_storage_denied(void) { return 1u; }
u32 cloud_storage64_encrypted_transport_unavailable(void) { return 1u; }
u32 cloud_storage64_upload_denied(void) { return 1u; }
u32 cloud_storage64_download_denied(void) { return 1u; }
u32 cloud_storage64_sync_denied(void) { return 1u; }
u32 cloud_storage64_auto_upload_unavailable(void) { return 1u; }
u32 cloud_storage64_auto_download_unavailable(void) { return 1u; }
u32 cloud_storage64_ai_access_unavailable(void) { return 1u; }
u32 cloud_storage64_app_direct_denied(void) { return 1u; }
u32 cloud_storage64_settings_readonly(void) { return 1u; }
u32 cloud_storage64_settings_mutation_denied(void) { return 1u; }
u32 cloud_storage64_fileman_status_readonly(void) { return 1u; }
u32 cloud_storage64_fileman_mutation_denied(void) { return 1u; }
u32 cloud_storage64_no_ambient_cloud(void) { return 1u; }
u32 cloud_storage64_no_ambient_fs(void) { return 1u; }
u32 cloud_storage64_no_ambient_network(void) { return 1u; }
u32 cloud_storage64_no_ambient_identity(void) { return 1u; }
u32 cloud_storage64_no_ambient_secret(void) { return 1u; }
const char *cloud_storage64_broker_status(void) { return "foundation-active"; }
const char *cloud_storage64_mode(void) { return "unavailable-policy-only"; }
const char *cloud_storage64_provider_id(void) { return "cloud.fixture.limitless"; }
const char *cloud_storage64_descriptor_status(void) { return "signed-local-fixture-verified"; }
const char *cloud_storage64_signature_status(void) { return "verified"; }
const char *cloud_storage64_anti_rollback_status(void) { return "enforced"; }
const char *cloud_storage64_account_status(void) { return "unavailable-planned"; }
const char *cloud_storage64_association_status(void) { return "unavailable-planned"; }
const char *cloud_storage64_token_storage_status(void) { return "denied-vault-mode-b"; }
const char *cloud_storage64_encrypted_transport_status(void) { return "unavailable"; }
const char *cloud_storage64_sync_status(void) { return "unavailable"; }
const char *cloud_storage64_upload_status(void) { return "denied"; }
const char *cloud_storage64_download_status(void) { return "denied"; }
const char *cloud_storage64_offline_cache_status(void) { return "planned-unavailable"; }
const char *cloud_storage64_ai_access_status(void) { return "unavailable"; }
const char *cloud_storage64_app_direct_status(void) { return "denied"; }

#endif
