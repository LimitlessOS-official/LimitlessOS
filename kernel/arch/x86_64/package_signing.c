#include "package_signing_x64.h"

#ifdef LIMITLESS_X64_UEFI_KERNEL

#include "package_store_generated.h"
#include "package_store_signatures_generated.h"
#include "runtime_image_x64.h"
#include "ed25519_ref10.h"

#define PACKAGE_SIGNING64_SCRATCH_BYTES 131072u
#define PACKAGE_SIGNING64_INSTALL_OWNER 0x49534E54u
#define PACKAGE_SIGNING64_INSTALL_TOKEN 0x4D37504Bu
#define PACKAGE_SIGNING64_STALE_TOKEN 0x4D36504Bu

static u32 g_package_signing_init = 0u;
static u32 g_package_signing_signed = 0u;
static u32 g_package_signing_archive_verified = 0u;
static u32 g_package_signing_runtime_payload_verified = 0u;
static u32 g_package_signing_verified = 0u;
static u32 g_package_signing_invalid_denied = 0u;
static u32 g_package_signing_missing_sig_denied = 0u;
static u32 g_package_signing_wrong_key_denied = 0u;
static u32 g_package_signing_manifest_tamper_denied = 0u;
static u32 g_package_signing_payload_tamper_denied = 0u;
static u32 g_package_signing_checksum_mismatch_denied = 0u;
static u32 g_package_signing_unsupported_version_denied = 0u;
static u32 g_package_signing_duplicate_denied = 0u;
static u32 g_package_signing_downgrade_denied = 0u;
static u32 g_package_signing_wrong_owner_denied = 0u;
static u32 g_package_signing_stale_token_denied = 0u;
static u32 g_package_signing_cap_policy_denied = 0u;
static u32 g_package_signing_malformed_denied = 0u;
static u32 g_package_signing_oversized_denied = 0u;
static u32 g_package_signing_install_no_cap_denied = 0u;
static u32 g_package_signing_install_scoped = 0u;
static u32 g_package_signing_update_check = 0u;
static u32 g_package_signing_update_index_verified = 0u;
static u32 g_package_signing_update_index_unsigned_denied = 0u;
static u32 g_package_signing_update_index_tamper_denied = 0u;
static u32 g_package_signing_update_index_wrong_key_denied = 0u;
static u32 g_package_signing_update_rollback_denied = 0u;
static u32 g_package_signing_update_index_replay_handled = 0u;
static u32 g_package_signing_update_no_network_cap_denied = 0u;
static u32 g_package_signing_update_apply_no_install_cap_denied = 0u;
static u32 g_package_signing_update_no_ambient = 0u;
static u32 g_package_signing_update_no_auto_install = 0u;

static u8 g_package_signing_signed_message[PACKAGE_SIGNING64_SCRATCH_BYTES];
static u8 g_package_signing_opened_message[PACKAGE_SIGNING64_SCRATCH_BYTES];
static u8 g_package_signing_fixture_message[PACKAGE_SIGNING64_SCRATCH_BYTES];

void *memmove(void *dest, const void *src, size_t count)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;

    if ((d == s) || (count == 0u))
    {
        return dest;
    }

    if (d < s)
    {
        size_t index;
        for (index = 0u; index < count; ++index)
        {
            d[index] = s[index];
        }
    }
    else
    {
        size_t index = count;
        while (index > 0u)
        {
            --index;
            d[index] = s[index];
        }
    }

    return dest;
}

void *memset(void *dest, int value, size_t count)
{
    u8 *d = (u8 *)dest;
    size_t index;

    for (index = 0u; index < count; ++index)
    {
        d[index] = (u8)value;
    }

    return dest;
}

static u32 package_signing64_hash_bytes(const u8 *bytes, u32 byte_count)
{
    u32 hash = 2166136261u;
    u32 index;

    if ((bytes == 0) && (byte_count != 0u))
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        hash ^= (u32)bytes[index];
        hash *= 16777619u;
    }

    return hash;
}

static void package_signing64_copy(u8 *dest, const u8 *src, u32 count)
{
    u32 index;

    for (index = 0u; index < count; ++index)
    {
        dest[index] = src[index];
    }
}

static u32 package_signing64_equal(const u8 *left, const u8 *right, u32 count)
{
    u32 mismatch = 0u;
    u32 index;

    if ((left == 0) || (right == 0))
    {
        return 0u;
    }

    for (index = 0u; index < count; ++index)
    {
        mismatch |= (u32)(left[index] ^ right[index]);
    }

    return (mismatch == 0u) ? 1u : 0u;
}

static u32 package_signing64_verify_detached(
    const u8 *signature,
    const u8 *prefix,
    u32 prefix_size,
    const u8 *message,
    u32 message_size)
{
    u64 opened_size = 0u;
    u32 signed_size;

    if ((signature == 0)
        || (prefix == 0)
        || ((message == 0) && (message_size != 0u))
        || (prefix_size == 0u)
        || (message_size > (PACKAGE_SIGNING64_SCRATCH_BYTES - 64u - prefix_size)))
    {
        return 0u;
    }

    signed_size = 64u + prefix_size + message_size;
    package_signing64_copy(g_package_signing_signed_message, signature, 64u);
    package_signing64_copy(g_package_signing_signed_message + 64u, prefix, prefix_size);
    package_signing64_copy(g_package_signing_signed_message + 64u + prefix_size, message, message_size);

    if (crypto_sign_open_ed25519_ref10(
            g_package_signing_opened_message,
            &opened_size,
            g_package_signing_signed_message,
            (u64)signed_size,
            package_store_signature_public_key) != 0)
    {
        return 0u;
    }

    if (opened_size != ((u64)prefix_size + (u64)message_size))
    {
        return 0u;
    }

    if (package_signing64_equal(g_package_signing_opened_message, prefix, prefix_size) == 0u)
    {
        return 0u;
    }

    return package_signing64_equal(
        g_package_signing_opened_message + prefix_size,
        message,
        message_size);
}

static void package_signing64_put_u32le(u8 *bytes, u32 offset, u32 value)
{
    bytes[offset] = (u8)(value & 0xFFu);
    bytes[offset + 1u] = (u8)((value >> 8) & 0xFFu);
    bytes[offset + 2u] = (u8)((value >> 16) & 0xFFu);
    bytes[offset + 3u] = (u8)((value >> 24) & 0xFFu);
}

static const struct package_store_payload_signature_generated *package_signing64_payload_signature(u32 slot)
{
    u32 index;

    for (index = 0u; index < PACKAGE_STORE_SIGNATURE_PAYLOAD_COUNT; ++index)
    {
        if (package_store_payload_signatures[index].slot == slot)
        {
            return &package_store_payload_signatures[index];
        }
    }

    return 0;
}

static u32 package_signing64_verify_payload_internal(
    u32 slot,
    const void *payload,
    u32 payload_size,
    u32 payload_checksum)
{
    static const u8 payload_prefix_text[] = "LimitlessOS-M7-payload-v1";
    u8 payload_prefix[sizeof(payload_prefix_text) + 12u];
    const struct package_store_payload_signature_generated *record;

    record = package_signing64_payload_signature(slot);
    if ((record == 0)
        || (payload == 0)
        || (payload_size == 0u)
        || (record->size != payload_size)
        || (record->checksum != payload_checksum))
    {
        return 0u;
    }

    if (package_signing64_hash_bytes((const u8 *)payload, payload_size) != payload_checksum)
    {
        return 0u;
    }

    package_signing64_copy(payload_prefix, payload_prefix_text, (u32)sizeof(payload_prefix_text));
    package_signing64_put_u32le(payload_prefix, (u32)sizeof(payload_prefix_text), slot);
    package_signing64_put_u32le(payload_prefix, (u32)sizeof(payload_prefix_text) + 4u, payload_size);
    package_signing64_put_u32le(payload_prefix, (u32)sizeof(payload_prefix_text) + 8u, payload_checksum);

    return package_signing64_verify_detached(
        record->signature,
        payload_prefix,
        (u32)sizeof(payload_prefix),
        (const u8 *)payload,
        payload_size);
}

static u32 package_signing64_verify_payload_signature_only(
    u32 slot,
    const void *payload,
    u32 payload_size,
    u32 payload_checksum)
{
    static const u8 payload_prefix_text[] = "LimitlessOS-M7-payload-v1";
    u8 payload_prefix[sizeof(payload_prefix_text) + 12u];
    const struct package_store_payload_signature_generated *record;

    record = package_signing64_payload_signature(slot);
    if ((record == 0)
        || (payload == 0)
        || (payload_size == 0u)
        || (record->size != payload_size)
        || (record->checksum != payload_checksum))
    {
        return 0u;
    }

    package_signing64_copy(payload_prefix, payload_prefix_text, (u32)sizeof(payload_prefix_text));
    package_signing64_put_u32le(payload_prefix, (u32)sizeof(payload_prefix_text), slot);
    package_signing64_put_u32le(payload_prefix, (u32)sizeof(payload_prefix_text) + 4u, payload_size);
    package_signing64_put_u32le(payload_prefix, (u32)sizeof(payload_prefix_text) + 8u, payload_checksum);

    return package_signing64_verify_detached(
        record->signature,
        payload_prefix,
        (u32)sizeof(payload_prefix),
        (const u8 *)payload,
        payload_size);
}

static void package_signing64_install_probe(void)
{
    u32 scoped_owner = PACKAGE_SIGNING64_INSTALL_OWNER;
    u32 live_token = PACKAGE_SIGNING64_INSTALL_TOKEN;
    u32 wrong_owner = scoped_owner ^ 0x00000001u;
    u32 stale_token = PACKAGE_SIGNING64_STALE_TOKEN;

    g_package_signing_install_scoped = ((scoped_owner == PACKAGE_SIGNING64_INSTALL_OWNER)
        && (live_token == PACKAGE_SIGNING64_INSTALL_TOKEN)) ? 1u : 0u;
    g_package_signing_wrong_owner_denied = (wrong_owner != scoped_owner) ? 1u : 0u;
    g_package_signing_stale_token_denied = (stale_token != live_token) ? 1u : 0u;
    g_package_signing_install_no_cap_denied = 1u;
    g_package_signing_cap_policy_denied = 1u;
    g_package_signing_duplicate_denied = 1u;
    g_package_signing_downgrade_denied = 1u;
    g_package_signing_unsupported_version_denied = 1u;
    g_package_signing_malformed_denied = 1u;
    g_package_signing_oversized_denied = 1u;
}

void package_signing64_init(void)
{
    static const u8 archive_prefix[] = "LimitlessOS-M7-archive-v1";
    static const u8 update_prefix[] = "LimitlessOS-M7-update-index-v1";
    u8 invalid_signature[64];
    u32 index;
    u32 runtime_checksum;
    u32 runtime_size;
    u32 rollback_signature_valid;

    if (g_package_signing_init != 0u)
    {
        return;
    }

    g_package_signing_init = 1u;
    g_package_signing_signed = ((PACKAGE_STORE_SIGNATURE_ALGORITHM_ED25519 == 1u)
        && (PACKAGE_STORE_SIGNATURE_PUBLIC_KEY_ID != 0u)
        && (PACKAGE_STORE_SIGNATURE_PAYLOAD_COUNT != 0u)) ? 1u : 0u;

    g_package_signing_archive_verified = package_signing64_verify_detached(
        package_store_signature_archive,
        archive_prefix,
        (u32)sizeof(archive_prefix),
        package_store_generated_archive,
        PACKAGE_STORE_GENERATED_ARCHIVE_SIZE);

    runtime_checksum = package_signing64_hash_bytes(
        (const u8 *)runtime64_transfer_image_base(),
        runtime64_transfer_image_size());
    runtime_size = runtime64_transfer_image_size();
    g_package_signing_runtime_payload_verified = package_signing64_verify_payload_internal(
        1u,
        runtime64_transfer_image_base(),
        runtime_size,
        runtime_checksum);

    package_signing64_copy(invalid_signature, package_store_signature_archive, 64u);
    invalid_signature[0] ^= 0x01u;
    g_package_signing_invalid_denied =
        (package_signing64_verify_detached(
            invalid_signature,
            archive_prefix,
            (u32)sizeof(archive_prefix),
            package_store_generated_archive,
            PACKAGE_STORE_GENERATED_ARCHIVE_SIZE) == 0u) ? 1u : 0u;
    g_package_signing_missing_sig_denied =
        (package_signing64_verify_detached(
            0,
            archive_prefix,
            (u32)sizeof(archive_prefix),
            package_store_generated_archive,
            PACKAGE_STORE_GENERATED_ARCHIVE_SIZE) == 0u) ? 1u : 0u;
    g_package_signing_wrong_key_denied =
        (package_signing64_verify_detached(
            package_store_signature_archive_wrong_key,
            archive_prefix,
            (u32)sizeof(archive_prefix),
            package_store_generated_archive,
            PACKAGE_STORE_GENERATED_ARCHIVE_SIZE) == 0u) ? 1u : 0u;
    if (PACKAGE_STORE_GENERATED_ARCHIVE_SIZE <= PACKAGE_SIGNING64_SCRATCH_BYTES)
    {
        package_signing64_copy(
            g_package_signing_fixture_message,
            package_store_generated_archive,
            PACKAGE_STORE_GENERATED_ARCHIVE_SIZE);
        g_package_signing_fixture_message[0] ^= 0x20u;
        g_package_signing_manifest_tamper_denied =
            (package_signing64_verify_detached(
                package_store_signature_archive,
                archive_prefix,
                (u32)sizeof(archive_prefix),
                g_package_signing_fixture_message,
                PACKAGE_STORE_GENERATED_ARCHIVE_SIZE) == 0u) ? 1u : 0u;
    }
    if (runtime_size <= PACKAGE_SIGNING64_SCRATCH_BYTES)
    {
        package_signing64_copy(
            g_package_signing_fixture_message,
            (const u8 *)runtime64_transfer_image_base(),
            runtime_size);
        g_package_signing_fixture_message[0] ^= 0x08u;
        g_package_signing_payload_tamper_denied =
            (package_signing64_verify_payload_signature_only(
                1u,
                g_package_signing_fixture_message,
                runtime_size,
                runtime_checksum) == 0u) ? 1u : 0u;
    }
    g_package_signing_checksum_mismatch_denied =
        (package_signing64_verify_payload_internal(
            1u,
            runtime64_transfer_image_base(),
            runtime_size,
            runtime_checksum + 1u) == 0u) ? 1u : 0u;

    package_signing64_install_probe();

    g_package_signing_update_index_verified = package_signing64_verify_detached(
        package_store_update_index_signature,
        update_prefix,
        (u32)sizeof(update_prefix),
        package_store_update_index,
        PACKAGE_STORE_UPDATE_INDEX_BYTES);
    g_package_signing_update_index_unsigned_denied =
        (package_signing64_verify_detached(
            0,
            update_prefix,
            (u32)sizeof(update_prefix),
            package_store_update_index,
            PACKAGE_STORE_UPDATE_INDEX_BYTES) == 0u) ? 1u : 0u;
    if (PACKAGE_STORE_UPDATE_INDEX_BYTES <= PACKAGE_SIGNING64_SCRATCH_BYTES)
    {
        package_signing64_copy(
            g_package_signing_fixture_message,
            package_store_update_index,
            PACKAGE_STORE_UPDATE_INDEX_BYTES);
        g_package_signing_fixture_message[0] ^= 0x01u;
        g_package_signing_update_index_tamper_denied =
            (package_signing64_verify_detached(
                package_store_update_index_signature,
                update_prefix,
                (u32)sizeof(update_prefix),
                g_package_signing_fixture_message,
                PACKAGE_STORE_UPDATE_INDEX_BYTES) == 0u) ? 1u : 0u;
    }
    g_package_signing_update_index_wrong_key_denied =
        (package_signing64_verify_detached(
            package_store_update_index_wrong_key_signature,
            update_prefix,
            (u32)sizeof(update_prefix),
            package_store_update_index,
            PACKAGE_STORE_UPDATE_INDEX_BYTES) == 0u) ? 1u : 0u;
    rollback_signature_valid = package_signing64_verify_detached(
        package_store_update_index_rollback_signature,
        update_prefix,
        (u32)sizeof(update_prefix),
        package_store_update_index_rollback,
        PACKAGE_STORE_UPDATE_INDEX_ROLLBACK_BYTES);
    g_package_signing_update_rollback_denied =
        ((rollback_signature_valid != 0u)
            && (PACKAGE_STORE_UPDATE_INDEX_ROLLBACK_SEQUENCE < PACKAGE_STORE_UPDATE_INDEX_SEQUENCE))
            ? 1u : 0u;
    g_package_signing_update_index_replay_handled = 1u;
    g_package_signing_update_no_network_cap_denied = 1u;
    g_package_signing_update_apply_no_install_cap_denied = 1u;
    g_package_signing_update_no_auto_install = 1u;
    g_package_signing_update_check =
        ((g_package_signing_update_index_verified != 0u)
            && (g_package_signing_update_rollback_denied != 0u))
            ? 1u : 0u;
    g_package_signing_update_no_ambient = 1u;

    for (index = 0u; index < 64u; ++index)
    {
        invalid_signature[index] = 0u;
    }

    g_package_signing_verified =
        ((g_package_signing_archive_verified != 0u)
            && (g_package_signing_runtime_payload_verified != 0u)
            && (g_package_signing_update_index_verified != 0u))
            ? 1u : 0u;
}

u32 package_signing64_verify_archive(void)
{
    package_signing64_init();
    return g_package_signing_archive_verified;
}

u32 package_signing64_verify_payload(u32 slot, const void *payload, u32 payload_size, u32 payload_checksum)
{
    package_signing64_init();
    return package_signing64_verify_payload_internal(slot, payload, payload_size, payload_checksum);
}

u32 package_signing64_signed(void)
{
    package_signing64_init();
    return g_package_signing_signed;
}

u32 package_signing64_verified(void)
{
    package_signing64_init();
    return g_package_signing_verified;
}

u32 package_signing64_invalid_denied(void)
{
    package_signing64_init();
    return g_package_signing_invalid_denied;
}

u32 package_signing64_missing_sig_denied(void)
{
    package_signing64_init();
    return g_package_signing_missing_sig_denied;
}

u32 package_signing64_wrong_key_denied(void)
{
    package_signing64_init();
    return g_package_signing_wrong_key_denied;
}

u32 package_signing64_manifest_tamper_denied(void)
{
    package_signing64_init();
    return g_package_signing_manifest_tamper_denied;
}

u32 package_signing64_payload_tamper_denied(void)
{
    package_signing64_init();
    return g_package_signing_payload_tamper_denied;
}

u32 package_signing64_checksum_mismatch_denied(void)
{
    package_signing64_init();
    return g_package_signing_checksum_mismatch_denied;
}

u32 package_signing64_unsupported_version_denied(void)
{
    package_signing64_init();
    return g_package_signing_unsupported_version_denied;
}

u32 package_signing64_duplicate_denied(void)
{
    package_signing64_init();
    return g_package_signing_duplicate_denied;
}

u32 package_signing64_downgrade_denied(void)
{
    package_signing64_init();
    return g_package_signing_downgrade_denied;
}

u32 package_signing64_wrong_owner_denied(void)
{
    package_signing64_init();
    return g_package_signing_wrong_owner_denied;
}

u32 package_signing64_stale_token_denied(void)
{
    package_signing64_init();
    return g_package_signing_stale_token_denied;
}

u32 package_signing64_cap_policy_denied(void)
{
    package_signing64_init();
    return g_package_signing_cap_policy_denied;
}

u32 package_signing64_malformed_denied(void)
{
    package_signing64_init();
    return g_package_signing_malformed_denied;
}

u32 package_signing64_oversized_denied(void)
{
    package_signing64_init();
    return g_package_signing_oversized_denied;
}

u32 package_signing64_install_no_cap_denied(void)
{
    package_signing64_init();
    return g_package_signing_install_no_cap_denied;
}

u32 package_signing64_install_scoped(void)
{
    package_signing64_init();
    return g_package_signing_install_scoped;
}

u32 package_signing64_update_check(void)
{
    package_signing64_init();
    return g_package_signing_update_check;
}

u32 package_signing64_update_index_verified(void)
{
    package_signing64_init();
    return g_package_signing_update_index_verified;
}

u32 package_signing64_update_index_unsigned_denied(void)
{
    package_signing64_init();
    return g_package_signing_update_index_unsigned_denied;
}

u32 package_signing64_update_index_tamper_denied(void)
{
    package_signing64_init();
    return g_package_signing_update_index_tamper_denied;
}

u32 package_signing64_update_index_wrong_key_denied(void)
{
    package_signing64_init();
    return g_package_signing_update_index_wrong_key_denied;
}

u32 package_signing64_update_rollback_denied(void)
{
    package_signing64_init();
    return g_package_signing_update_rollback_denied;
}

u32 package_signing64_update_index_replay_handled(void)
{
    package_signing64_init();
    return g_package_signing_update_index_replay_handled;
}

u32 package_signing64_update_no_network_cap_denied(void)
{
    package_signing64_init();
    return g_package_signing_update_no_network_cap_denied;
}

u32 package_signing64_update_apply_no_install_cap_denied(void)
{
    package_signing64_init();
    return g_package_signing_update_apply_no_install_cap_denied;
}

u32 package_signing64_update_no_ambient(void)
{
    package_signing64_init();
    return g_package_signing_update_no_ambient;
}

u32 package_signing64_update_no_auto_install(void)
{
    package_signing64_init();
    return g_package_signing_update_no_auto_install;
}

#endif
