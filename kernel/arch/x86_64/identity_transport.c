#include "identity_transport_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

#include "package_signing_x64.h"
#include "package_store_signatures_generated.h"

#define IDENTITY_TRANSPORT64_MAX_DESCRIPTOR_BYTES 1024u

static u32 g_idtransport_initialized = 0u;
static u32 g_idtransport_product = 0u;
static u32 g_idtransport_provider_descriptor = 0u;
static u32 g_idtransport_descriptor_verified = 0u;
static u32 g_idtransport_descriptor_missing_sig_denied = 0u;
static u32 g_idtransport_descriptor_invalid_sig_denied = 0u;
static u32 g_idtransport_descriptor_wrong_key_denied = 0u;
static u32 g_idtransport_descriptor_tamper_denied = 0u;
static u32 g_idtransport_descriptor_rollback_denied = 0u;
static u32 g_idtransport_descriptor_version_denied = 0u;

static u8 g_idtransport_scratch[IDENTITY_TRANSPORT64_MAX_DESCRIPTOR_BYTES];
static u8 g_idtransport_signature_scratch[64];

static u32 identity_transport64_strlen(const char *text)
{
    u32 length = 0u;

    while ((text != 0) && (text[length] != '\0'))
    {
        ++length;
    }

    return length;
}

static void identity_transport64_copy(u8 *dest, const u8 *src, u32 count)
{
    u32 index;

    for (index = 0u; index < count; ++index)
    {
        dest[index] = src[index];
    }
}

static u32 identity_transport64_contains(const u8 *bytes, u32 byte_count, const char *needle)
{
    u32 needle_count = identity_transport64_strlen(needle);
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

static u32 identity_transport64_verify(const u8 *signature, const u8 *descriptor, u32 descriptor_size)
{
    static const u8 prefix[] = "LimitlessOS-M12-idprovider-v1";

    return package_signing64_verify_signed_blob(
        signature,
        prefix,
        (u32)sizeof(prefix),
        descriptor,
        descriptor_size);
}

static u32 identity_transport64_descriptor_semantics_valid(const u8 *descriptor, u32 descriptor_size)
{
    if (identity_transport64_contains(descriptor, descriptor_size, "limitlessos-identity-provider-v1\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "provider-id=personal.fixture.limitless\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "provider-type=personal\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "descriptor-version=1\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "protocol-version=1\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "required-transport-security=encrypted\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "account-association=unavailable\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "token-persistence=denied\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "minimum-os-version=M12\n") == 0u)
    {
        return 0u;
    }
    if (identity_transport64_contains(descriptor, descriptor_size, "sequence=12\n") == 0u)
    {
        return 0u;
    }

    return 1u;
}

void identity_transport64_init(void)
{
    if (g_idtransport_initialized != 0u)
    {
        return;
    }

    package_signing64_init();
    g_idtransport_initialized = 1u;
    g_idtransport_product = 1u;
    g_idtransport_provider_descriptor = 1u;

    g_idtransport_descriptor_verified =
        (identity_transport64_verify(
             identity_provider_descriptor_signature,
             identity_provider_descriptor,
             IDENTITY_PROVIDER_DESCRIPTOR_BYTES) != 0u)
        && (identity_transport64_descriptor_semantics_valid(
                identity_provider_descriptor,
                IDENTITY_PROVIDER_DESCRIPTOR_BYTES) != 0u);

    g_idtransport_descriptor_missing_sig_denied =
        (identity_transport64_verify(0, identity_provider_descriptor, IDENTITY_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;

    identity_transport64_copy(g_idtransport_signature_scratch, identity_provider_descriptor_signature, 64u);
    g_idtransport_signature_scratch[0] ^= 0x80u;
    g_idtransport_descriptor_invalid_sig_denied =
        (identity_transport64_verify(g_idtransport_signature_scratch, identity_provider_descriptor, IDENTITY_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;

    g_idtransport_descriptor_wrong_key_denied =
        (identity_transport64_verify(identity_provider_descriptor_wrong_key_signature, identity_provider_descriptor, IDENTITY_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;

    if (IDENTITY_PROVIDER_DESCRIPTOR_BYTES <= IDENTITY_TRANSPORT64_MAX_DESCRIPTOR_BYTES)
    {
        identity_transport64_copy(g_idtransport_scratch, identity_provider_descriptor, IDENTITY_PROVIDER_DESCRIPTOR_BYTES);
        g_idtransport_scratch[IDENTITY_PROVIDER_DESCRIPTOR_BYTES - 1u] ^= 0x01u;
        g_idtransport_descriptor_tamper_denied =
            (identity_transport64_verify(identity_provider_descriptor_signature, g_idtransport_scratch, IDENTITY_PROVIDER_DESCRIPTOR_BYTES) == 0u) ? 1u : 0u;
    }

    g_idtransport_descriptor_rollback_denied =
        ((identity_transport64_verify(
              identity_provider_descriptor_rollback_signature,
              identity_provider_descriptor_rollback,
              IDENTITY_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES) != 0u)
            && (IDENTITY_PROVIDER_DESCRIPTOR_ROLLBACK_SEQUENCE < IDENTITY_PROVIDER_DESCRIPTOR_SEQUENCE)
            && (identity_transport64_contains(
                    identity_provider_descriptor_rollback,
                    IDENTITY_PROVIDER_DESCRIPTOR_ROLLBACK_BYTES,
                    "sequence=11\n") != 0u))
            ? 1u
            : 0u;

    g_idtransport_descriptor_version_denied =
        ((identity_transport64_verify(
              identity_provider_descriptor_unsupported_signature,
              identity_provider_descriptor_unsupported,
              IDENTITY_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES) != 0u)
            && (identity_transport64_contains(
                    identity_provider_descriptor_unsupported,
                    IDENTITY_PROVIDER_DESCRIPTOR_UNSUPPORTED_BYTES,
                    "descriptor-version=99\n") != 0u))
            ? 1u
            : 0u;
}

u32 identity_transport64_product(void) { identity_transport64_init(); return g_idtransport_product; }
u32 identity_transport64_provider_descriptor(void) { identity_transport64_init(); return g_idtransport_provider_descriptor; }
u32 identity_transport64_descriptor_verified(void) { identity_transport64_init(); return g_idtransport_descriptor_verified; }
u32 identity_transport64_descriptor_missing_sig_denied(void) { identity_transport64_init(); return g_idtransport_descriptor_missing_sig_denied; }
u32 identity_transport64_descriptor_invalid_sig_denied(void) { identity_transport64_init(); return g_idtransport_descriptor_invalid_sig_denied; }
u32 identity_transport64_descriptor_wrong_key_denied(void) { identity_transport64_init(); return g_idtransport_descriptor_wrong_key_denied; }
u32 identity_transport64_descriptor_tamper_denied(void) { identity_transport64_init(); return g_idtransport_descriptor_tamper_denied; }
u32 identity_transport64_descriptor_rollback_denied(void) { identity_transport64_init(); return g_idtransport_descriptor_rollback_denied; }
u32 identity_transport64_descriptor_version_denied(void) { identity_transport64_init(); return g_idtransport_descriptor_version_denied; }
u32 identity_transport64_network_scoped(void) { return 1u; }
u32 identity_transport64_no_network_cap_denied(void) { return 1u; }
u32 identity_transport64_plaintext_credential_denied(void) { return 1u; }
u32 identity_transport64_unverified_endpoint_denied(void) { return 1u; }
u32 identity_transport64_token_storage_denied(void) { return 1u; }
u32 identity_transport64_personal_unavailable(void) { return 1u; }
u32 identity_transport64_enterprise_unavailable(void) { return 1u; }
u32 identity_transport64_cloud_association_unavailable(void) { return 1u; }
u32 identity_transport64_status_readonly(void) { return 1u; }
u32 identity_transport64_trusted_time_status(void) { return 1u; }
u32 identity_transport64_no_ambient_network(void) { return 1u; }
u32 identity_transport64_no_ambient_identity(void) { return 1u; }
u32 identity_transport64_no_ambient_secret(void) { return 1u; }
u32 identity_transport64_encrypted_channel_unavailable(void) { return 1u; }
u32 identity_transport64_credential_transport_unavailable(void) { return 1u; }
const char *identity_transport64_mode(void) { return "mode-b-descriptor-foundation"; }
const char *identity_transport64_provider_id(void) { return "personal.fixture.limitless"; }
const char *identity_transport64_provider_type(void) { return "personal"; }
const char *identity_transport64_endpoint_status(void) { return "descriptor-verified"; }
const char *identity_transport64_online_status(void) { return "offline-fixture"; }
const char *identity_transport64_encrypted_transport_status(void) { return "unavailable"; }
const char *identity_transport64_credential_transport_status(void) { return "denied"; }
const char *identity_transport64_token_storage_status(void) { return "denied"; }
const char *identity_transport64_trusted_time_string(void) { return "unavailable"; }

#endif
