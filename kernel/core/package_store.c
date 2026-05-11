#include "package_store.h"

#include "klog.h"
#include "package_store_generated.h"

enum
{
    PACKAGE_STORE_MAGIC = 0x504B4753u,
    PACKAGE_STORE_VERSION = 2u,
    PACKAGE_STORE_ARCHIVE_CHECKSUM_OFFSET = 24u,
    PACKAGE_STORE_HEADER_FIELD_COUNT = 7u,
    PACKAGE_STORE_HEADER_SIZE = PACKAGE_STORE_HEADER_FIELD_COUNT * 4u,
    PACKAGE_STORE_SIGNER_FIELD_COUNT = 3u,
    PACKAGE_STORE_SIGNER_SIZE = PACKAGE_STORE_SIGNER_FIELD_COUNT * 4u,
    PACKAGE_STORE_MANIFEST_FIELD_COUNT = 23u,
    PACKAGE_STORE_MANIFEST_SIZE = PACKAGE_STORE_MANIFEST_FIELD_COUNT * 4u,
    PACKAGE_STORE_PAYLOAD_FIELD_COUNT = 5u,
    PACKAGE_STORE_PAYLOAD_SIZE = PACKAGE_STORE_PAYLOAD_FIELD_COUNT * 4u,
    PACKAGE_STORE_PAYLOAD_BOOTSTRAP = 1u,
    PACKAGE_STORE_PAYLOAD_KIND_BOOTSTRAP_SERVICE = 1u,
    PACKAGE_STORE_PAYLOAD_KIND_FLAT_BINARY = 2u
};

enum package_store_header_field
{
    PACKAGE_STORE_HEADER_MAGIC = 0u,
    PACKAGE_STORE_HEADER_VERSION = 1u,
    PACKAGE_STORE_HEADER_SIGNER_COUNT = 2u,
    PACKAGE_STORE_HEADER_MANIFEST_COUNT = 3u,
    PACKAGE_STORE_HEADER_PAYLOAD_COUNT = 4u,
    PACKAGE_STORE_HEADER_STRING_BYTES = 5u,
    PACKAGE_STORE_HEADER_ARCHIVE_CHECKSUM = 6u
};

enum package_store_signer_field
{
    PACKAGE_STORE_SIGNER_ID = 0u,
    PACKAGE_STORE_SIGNER_NAME_OFFSET = 1u,
    PACKAGE_STORE_SIGNER_VERIFICATION_TOKEN = 2u
};

enum package_store_manifest_field
{
    PACKAGE_STORE_MANIFEST_SOURCE_SLOT = 0u,
    PACKAGE_STORE_MANIFEST_PACKAGE_ID = 1u,
    PACKAGE_STORE_MANIFEST_PACKAGE_NAME_OFFSET = 2u,
    PACKAGE_STORE_MANIFEST_PACKAGE_VERSION = 3u,
    PACKAGE_STORE_MANIFEST_SIGNER_ID = 4u,
    PACKAGE_STORE_MANIFEST_TRUST_FLAGS = 5u,
    PACKAGE_STORE_MANIFEST_LAUNCH_AUTHORITY_MASK = 6u,
    PACKAGE_STORE_MANIFEST_MAX_INSTANCES = 7u,
    PACKAGE_STORE_MANIFEST_EXECUTABLE_ID = 8u,
    PACKAGE_STORE_MANIFEST_EXECUTABLE_NAME_OFFSET = 9u,
    PACKAGE_STORE_MANIFEST_PROCESS_NAME_OFFSET = 10u,
    PACKAGE_STORE_MANIFEST_PROFILE_NAME_OFFSET = 11u,
    PACKAGE_STORE_MANIFEST_PEER_ENDPOINT_NAME_OFFSET = 12u,
    PACKAGE_STORE_MANIFEST_POLICY_ENDPOINT_NAME_OFFSET = 13u,
    PACKAGE_STORE_MANIFEST_ALLOWED_ENDPOINT_ROLE_MASK = 14u,
    PACKAGE_STORE_MANIFEST_ALLOWED_SERVICE_CLASS_MASK = 15u,
    PACKAGE_STORE_MANIFEST_SCHEDULER_CLASS = 16u,
    PACKAGE_STORE_MANIFEST_SCHEDULER_WEIGHT = 17u,
    PACKAGE_STORE_MANIFEST_SCHEDULER_LATENCY_TARGET_TICKS = 18u,
    PACKAGE_STORE_MANIFEST_SCHEDULER_IO_WAKEUP_DEADLINE_TICKS = 19u,
    PACKAGE_STORE_MANIFEST_CAPABILITY_ADMISSION_LIMIT = 20u,
    PACKAGE_STORE_MANIFEST_LAUNCH_ROLE = 21u,
    PACKAGE_STORE_MANIFEST_PAYLOAD_SLOT = 22u
};

enum package_store_payload_field
{
    PACKAGE_STORE_PAYLOAD_SLOT = 0u,
    PACKAGE_STORE_PAYLOAD_KIND = 1u,
    PACKAGE_STORE_PAYLOAD_IMAGE_OFFSET = 2u,
    PACKAGE_STORE_PAYLOAD_IMAGE_SIZE = 3u,
    PACKAGE_STORE_PAYLOAD_IMAGE_CHECKSUM = 4u
};

struct package_store_view
{
    u32 signer_count;
    u32 manifest_count;
    u32 payload_count;
    u32 string_bytes;
    u32 archive_checksum;
    u32 signers_offset;
    u32 manifests_offset;
    u32 payloads_offset;
    u32 strings_offset;
};

extern u8 user_bootstrap_service_start;
extern u8 user_bootstrap_service_end;

static struct package_store_view store_view;
static u32 bootstrap_image_size = 0u;
static u32 bootstrap_image_digest = 0u;
static int store_ready = 0;

static void package_store_reset_view(void)
{
    store_view.signer_count = 0u;
    store_view.manifest_count = 0u;
    store_view.payload_count = 0u;
    store_view.string_bytes = 0u;
    store_view.archive_checksum = 0u;
    store_view.signers_offset = 0u;
    store_view.manifests_offset = 0u;
    store_view.payloads_offset = 0u;
    store_view.strings_offset = 0u;
}

static u32 package_store_read_u32(const u8 *address)
{
    return (u32)address[0]
        | ((u32)address[1] << 8u)
        | ((u32)address[2] << 16u)
        | ((u32)address[3] << 24u);
}

static int package_store_add_u32(u32 left, u32 right, u32 *result_out)
{
    u64 total;

    if (result_out == NULL)
    {
        return 0;
    }

    total = (u64)left + (u64)right;
    if (total > 0xFFFFFFFFu)
    {
        return 0;
    }

    *result_out = (u32)total;
    return 1;
}

static int package_store_mul_u32(u32 left, u32 right, u32 *result_out)
{
    u64 total;

    if (result_out == NULL)
    {
        return 0;
    }

    total = (u64)left * (u64)right;
    if (total > 0xFFFFFFFFu)
    {
        return 0;
    }

    *result_out = (u32)total;
    return 1;
}

static u32 package_store_hash_string(const char *text)
{
    u32 digest = 2166136261u;

    if (text == NULL)
    {
        return 0u;
    }

    while (*text != '\0')
    {
        digest ^= (u8)(*text);
        digest *= 16777619u;
        ++text;
    }

    return digest;
}

static u32 package_store_hash_image(const u8 *image_start, u32 image_size)
{
    u32 digest = 2166136261u;
    u32 index;

    if (image_start == NULL)
    {
        return 0u;
    }

    for (index = 0; index < image_size; ++index)
    {
        digest ^= image_start[index];
        digest *= 16777619u;
    }

    return digest;
}

static u32 package_store_hash_archive(void)
{
    u32 digest = 2166136261u;
    u32 index;

    for (index = 0u; index < PACKAGE_STORE_GENERATED_ARCHIVE_SIZE; ++index)
    {
        u8 value = package_store_generated_archive[index];

        if ((index >= PACKAGE_STORE_ARCHIVE_CHECKSUM_OFFSET)
            && (index < (PACKAGE_STORE_ARCHIVE_CHECKSUM_OFFSET + 4u)))
        {
            value = 0u;
        }

        digest ^= value;
        digest *= 16777619u;
    }

    return digest;
}

static u32 package_store_signature_token(
    const struct package_store_manifest_record *record,
    u32 verification_token)
{
    u32 digest;

    if (record == NULL)
    {
        return 0u;
    }

    digest = package_store_hash_string(record->package_name);
    digest ^= package_store_hash_string(record->name);
    digest ^= package_store_hash_string(record->process_name);
    digest ^= record->package_id;
    digest ^= record->package_version << 4;
    digest ^= record->expected_image_size << 8;
    digest ^= record->expected_image_checksum;
    digest ^= record->trust_flags << 12;
    digest ^= record->launch_authority_mask << 16;
    digest ^= record->max_instances << 20;
    digest ^= record->launch_role << 24;
    digest ^= verification_token;
    digest *= 16777619u;
    return digest;
}

static const u8 *package_store_signer_bytes(u32 index)
{
    u32 offset;

    if (index >= store_view.signer_count)
    {
        return NULL;
    }

    if (!package_store_mul_u32(index, PACKAGE_STORE_SIGNER_SIZE, &offset))
    {
        return NULL;
    }

    if (!package_store_add_u32(store_view.signers_offset, offset, &offset))
    {
        return NULL;
    }

    if ((offset + PACKAGE_STORE_SIGNER_SIZE) > PACKAGE_STORE_GENERATED_ARCHIVE_SIZE)
    {
        return NULL;
    }

    return &package_store_generated_archive[offset];
}

static const u8 *package_store_manifest_bytes(u32 index)
{
    u32 offset;

    if (index >= store_view.manifest_count)
    {
        return NULL;
    }

    if (!package_store_mul_u32(index, PACKAGE_STORE_MANIFEST_SIZE, &offset))
    {
        return NULL;
    }

    if (!package_store_add_u32(store_view.manifests_offset, offset, &offset))
    {
        return NULL;
    }

    if ((offset + PACKAGE_STORE_MANIFEST_SIZE) > PACKAGE_STORE_GENERATED_ARCHIVE_SIZE)
    {
        return NULL;
    }

    return &package_store_generated_archive[offset];
}

static const u8 *package_store_payload_bytes(u32 index)
{
    u32 offset;

    if (index >= store_view.payload_count)
    {
        return NULL;
    }

    if (!package_store_mul_u32(index, PACKAGE_STORE_PAYLOAD_SIZE, &offset))
    {
        return NULL;
    }

    if (!package_store_add_u32(store_view.payloads_offset, offset, &offset))
    {
        return NULL;
    }

    if ((offset + PACKAGE_STORE_PAYLOAD_SIZE) > PACKAGE_STORE_GENERATED_ARCHIVE_SIZE)
    {
        return NULL;
    }

    return &package_store_generated_archive[offset];
}

static const char *package_store_string(u32 offset)
{
    u32 index;
    u32 base;

    if (offset >= store_view.string_bytes)
    {
        return NULL;
    }

    base = store_view.strings_offset + offset;
    if (base >= PACKAGE_STORE_GENERATED_ARCHIVE_SIZE)
    {
        return NULL;
    }

    for (index = offset; index < store_view.string_bytes; ++index)
    {
        if (package_store_generated_archive[store_view.strings_offset + index] == 0u)
        {
            return (const char *)&package_store_generated_archive[base];
        }
    }

    return NULL;
}

static int package_store_find_payload_slot(
    u32 slot,
    u32 *kind_out,
    u32 *image_offset_out,
    u32 *image_size_out,
    u32 *image_checksum_out)
{
    u32 index;

    for (index = 0u; index < store_view.payload_count; ++index)
    {
        const u8 *record = package_store_payload_bytes(index);
        u32 payload_slot;
        u32 payload_kind;
        u32 image_offset;
        u32 image_size;
        u32 image_checksum;

        if (record == NULL)
        {
            return 0;
        }

        payload_slot = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_SLOT * 4u));
        payload_kind = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_KIND * 4u));
        image_offset = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_IMAGE_OFFSET * 4u));
        image_size = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_IMAGE_SIZE * 4u));
        image_checksum = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_IMAGE_CHECKSUM * 4u));

        if (payload_slot == slot)
        {
            if (kind_out != NULL)
            {
                *kind_out = payload_kind;
            }

            if (image_offset_out != NULL)
            {
                *image_offset_out = image_offset;
            }

            if (image_size_out != NULL)
            {
                *image_size_out = image_size;
            }

            if (image_checksum_out != NULL)
            {
                *image_checksum_out = image_checksum;
            }

            return 1;
        }
    }

    return 0;
}

static int package_store_find_signer_token(u32 signer_id, u32 *token_out)
{
    u32 index;

    for (index = 0u; index < store_view.signer_count; ++index)
    {
        const u8 *record = package_store_signer_bytes(index);
        u32 record_signer_id;
        u32 record_token;

        if (record == NULL)
        {
            return 0;
        }

        record_signer_id = package_store_read_u32(record + (PACKAGE_STORE_SIGNER_ID * 4u));
        record_token = package_store_read_u32(record + (PACKAGE_STORE_SIGNER_VERIFICATION_TOKEN * 4u));

        if (record_signer_id == signer_id)
        {
            if (token_out != NULL)
            {
                *token_out = record_token;
            }

            return 1;
        }
    }

    return 0;
}

static void package_store_fail(const char *reason)
{
    store_ready = 0;
    package_store_reset_view();
    klog_write_string("[package-store] invalid archive ");
    klog_write_line(reason);
}

static int package_store_validate_archive(void)
{
    u32 magic;
    u32 version;
    u32 signer_bytes;
    u32 manifest_bytes;
    u32 payload_bytes;
    u32 strings_end;
    u32 checksum;
    u32 index;

    if (PACKAGE_STORE_GENERATED_ARCHIVE_SIZE < PACKAGE_STORE_HEADER_SIZE)
    {
        package_store_fail("size");
        return 0;
    }

    magic = package_store_read_u32(&package_store_generated_archive[PACKAGE_STORE_HEADER_MAGIC * 4u]);
    version = package_store_read_u32(&package_store_generated_archive[PACKAGE_STORE_HEADER_VERSION * 4u]);
    store_view.signer_count = package_store_read_u32(&package_store_generated_archive[PACKAGE_STORE_HEADER_SIGNER_COUNT * 4u]);
    store_view.manifest_count = package_store_read_u32(&package_store_generated_archive[PACKAGE_STORE_HEADER_MANIFEST_COUNT * 4u]);
    store_view.payload_count = package_store_read_u32(&package_store_generated_archive[PACKAGE_STORE_HEADER_PAYLOAD_COUNT * 4u]);
    store_view.string_bytes = package_store_read_u32(&package_store_generated_archive[PACKAGE_STORE_HEADER_STRING_BYTES * 4u]);
    store_view.archive_checksum = package_store_read_u32(&package_store_generated_archive[PACKAGE_STORE_HEADER_ARCHIVE_CHECKSUM * 4u]);

    if ((magic != PACKAGE_STORE_MAGIC) || (version != PACKAGE_STORE_VERSION))
    {
        package_store_fail("header");
        return 0;
    }

    store_view.signers_offset = PACKAGE_STORE_HEADER_SIZE;
    if (!package_store_mul_u32(store_view.signer_count, PACKAGE_STORE_SIGNER_SIZE, &signer_bytes)
        || !package_store_add_u32(store_view.signers_offset, signer_bytes, &store_view.manifests_offset)
        || !package_store_mul_u32(store_view.manifest_count, PACKAGE_STORE_MANIFEST_SIZE, &manifest_bytes)
        || !package_store_add_u32(store_view.manifests_offset, manifest_bytes, &store_view.payloads_offset)
        || !package_store_mul_u32(store_view.payload_count, PACKAGE_STORE_PAYLOAD_SIZE, &payload_bytes)
        || !package_store_add_u32(store_view.payloads_offset, payload_bytes, &store_view.strings_offset)
        || !package_store_add_u32(store_view.strings_offset, store_view.string_bytes, &strings_end))
    {
        package_store_fail("layout");
        return 0;
    }

    if (strings_end != PACKAGE_STORE_GENERATED_ARCHIVE_SIZE)
    {
        package_store_fail("extent");
        return 0;
    }

    checksum = package_store_hash_archive();
    if ((checksum != store_view.archive_checksum)
        || (checksum != PACKAGE_STORE_GENERATED_ARCHIVE_CHECKSUM))
    {
        package_store_fail("checksum");
        return 0;
    }

    for (index = 0u; index < store_view.signer_count; ++index)
    {
        const u8 *record = package_store_signer_bytes(index);

        if ((record == NULL)
            || (package_store_string(package_store_read_u32(record + (PACKAGE_STORE_SIGNER_NAME_OFFSET * 4u))) == NULL))
        {
            package_store_fail("signer");
            return 0;
        }
    }

    for (index = 0u; index < store_view.payload_count; ++index)
    {
        const u8 *record = package_store_payload_bytes(index);
        u32 payload_kind;
        u32 payload_slot;
        u32 payload_image_size;
        u32 payload_image_checksum;

        if (record == NULL)
        {
            package_store_fail("payload");
            return 0;
        }

        payload_slot = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_SLOT * 4u));
        payload_kind = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_KIND * 4u));
        payload_image_size = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_IMAGE_SIZE * 4u));
        payload_image_checksum = package_store_read_u32(record + (PACKAGE_STORE_PAYLOAD_IMAGE_CHECKSUM * 4u));
        if ((payload_slot == 0u)
            || ((payload_kind != PACKAGE_STORE_PAYLOAD_KIND_BOOTSTRAP_SERVICE)
                && (payload_kind != PACKAGE_STORE_PAYLOAD_KIND_FLAT_BINARY))
            || (payload_image_size == 0u)
            || (payload_image_checksum == 0u))
        {
            package_store_fail("payload");
            return 0;
        }
    }

    for (index = 0u; index < store_view.manifest_count; ++index)
    {
        const u8 *record = package_store_manifest_bytes(index);
        u32 payload_slot;

        if (record == NULL)
        {
            package_store_fail("manifest");
            return 0;
        }

        if ((package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PACKAGE_NAME_OFFSET * 4u))) == NULL)
            || (package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_EXECUTABLE_NAME_OFFSET * 4u))) == NULL)
            || (package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PROCESS_NAME_OFFSET * 4u))) == NULL)
            || (package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PROFILE_NAME_OFFSET * 4u))) == NULL)
            || (package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PEER_ENDPOINT_NAME_OFFSET * 4u))) == NULL)
            || (package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_POLICY_ENDPOINT_NAME_OFFSET * 4u))) == NULL))
        {
            package_store_fail("string");
            return 0;
        }

        payload_slot = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PAYLOAD_SLOT * 4u));
        if (!package_store_find_payload_slot(payload_slot, NULL, NULL, NULL, NULL))
        {
            package_store_fail("payload-ref");
            return 0;
        }
    }

    return 1;
}

void package_store_init(void)
{
    u32 expected_image_offset = 0u;
    u32 expected_image_size = 0u;
    u32 expected_image_checksum = 0u;
    u32 payload_kind = 0u;

    package_store_reset_view();
    store_ready = 0;

    if (!package_store_validate_archive())
    {
        return;
    }

    bootstrap_image_size = (u32)(&user_bootstrap_service_end - &user_bootstrap_service_start);
    bootstrap_image_digest = package_store_hash_image(&user_bootstrap_service_start, bootstrap_image_size);
    if (!package_store_find_payload_slot(
            PACKAGE_STORE_PAYLOAD_BOOTSTRAP,
            &payload_kind,
            &expected_image_offset,
            &expected_image_size,
            &expected_image_checksum)
        || (payload_kind != PACKAGE_STORE_PAYLOAD_KIND_BOOTSTRAP_SERVICE)
        || (expected_image_offset != 0u)
        || (expected_image_size != bootstrap_image_size)
        || (expected_image_checksum != bootstrap_image_digest))
    {
        package_store_fail("payload-image");
        return;
    }

    store_ready = 1;

    klog_write_string("[package-store] archive v");
    klog_write_dec_u32(PACKAGE_STORE_VERSION);
    klog_write_string(" signers ");
    klog_write_dec_u32(store_view.signer_count);
    klog_write_string(" manifests ");
    klog_write_dec_u32(store_view.manifest_count);
    klog_write_string(" payloads ");
    klog_write_dec_u32(store_view.payload_count);
    klog_write_string(" checksum ");
    klog_write_hex_u32(store_view.archive_checksum);
    klog_newline();
}

int package_store_ready(void)
{
    return store_ready;
}

u32 package_store_signer_count(void)
{
    return store_ready ? store_view.signer_count : 0u;
}

u32 package_store_manifest_count(void)
{
    return store_ready ? store_view.manifest_count : 0u;
}

int package_store_read_signer(u32 index, struct package_store_signer_record *out_record)
{
    const u8 *record;

    if (!store_ready || (out_record == NULL))
    {
        return 0;
    }

    record = package_store_signer_bytes(index);
    if (record == NULL)
    {
        return 0;
    }

    out_record->id = package_store_read_u32(record + (PACKAGE_STORE_SIGNER_ID * 4u));
    out_record->name = package_store_string(package_store_read_u32(record + (PACKAGE_STORE_SIGNER_NAME_OFFSET * 4u)));
    out_record->verification_token = package_store_read_u32(record + (PACKAGE_STORE_SIGNER_VERIFICATION_TOKEN * 4u));
    return out_record->name != NULL;
}

int package_store_read_manifest(u32 index, struct package_store_manifest_record *out_record)
{
    const u8 *record;
    u32 verification_token = 0u;
    u32 expected_image_size = 0u;
    u32 expected_image_checksum = 0u;

    if (!store_ready || (out_record == NULL))
    {
        return 0;
    }

    record = package_store_manifest_bytes(index);
    if (record == NULL)
    {
        return 0;
    }

    out_record->source_slot = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_SOURCE_SLOT * 4u));
    out_record->package_id = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PACKAGE_ID * 4u));
    out_record->package_name = package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PACKAGE_NAME_OFFSET * 4u)));
    out_record->package_version = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PACKAGE_VERSION * 4u));
    out_record->signer_id = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_SIGNER_ID * 4u));
    out_record->trust_flags = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_TRUST_FLAGS * 4u));
    out_record->launch_authority_mask = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_LAUNCH_AUTHORITY_MASK * 4u));
    out_record->max_instances = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_MAX_INSTANCES * 4u));
    out_record->executable_id = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_EXECUTABLE_ID * 4u));
    out_record->name = package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_EXECUTABLE_NAME_OFFSET * 4u)));
    out_record->process_name = package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PROCESS_NAME_OFFSET * 4u)));
    out_record->profile_name = package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PROFILE_NAME_OFFSET * 4u)));
    out_record->peer_endpoint_name = package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PEER_ENDPOINT_NAME_OFFSET * 4u)));
    out_record->policy_endpoint_name = package_store_string(package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_POLICY_ENDPOINT_NAME_OFFSET * 4u)));
    out_record->allowed_endpoint_role_mask = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_ALLOWED_ENDPOINT_ROLE_MASK * 4u));
    out_record->allowed_service_class_mask = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_ALLOWED_SERVICE_CLASS_MASK * 4u));
    out_record->scheduler_class = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_SCHEDULER_CLASS * 4u));
    out_record->scheduler_weight = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_SCHEDULER_WEIGHT * 4u));
    out_record->scheduler_latency_target_ticks = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_SCHEDULER_LATENCY_TARGET_TICKS * 4u));
    out_record->scheduler_io_wakeup_deadline_ticks = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_SCHEDULER_IO_WAKEUP_DEADLINE_TICKS * 4u));
    out_record->capability_admission_limit = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_CAPABILITY_ADMISSION_LIMIT * 4u));
    out_record->launch_role = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_LAUNCH_ROLE * 4u));
    out_record->payload_slot = package_store_read_u32(record + (PACKAGE_STORE_MANIFEST_PAYLOAD_SLOT * 4u));
    if (!package_store_find_payload_slot(
            out_record->payload_slot,
            NULL,
            NULL,
            &expected_image_size,
            &expected_image_checksum))
    {
        return 0;
    }

    out_record->expected_image_size = expected_image_size;
    out_record->expected_image_checksum = expected_image_checksum;
    out_record->signature_token = 0u;

    if (package_store_find_signer_token(out_record->signer_id, &verification_token))
    {
        out_record->signature_token = package_store_signature_token(out_record, verification_token);
    }

    return (out_record->package_name != NULL)
        && (out_record->name != NULL)
        && (out_record->process_name != NULL)
        && (out_record->profile_name != NULL)
        && (out_record->peer_endpoint_name != NULL)
        && (out_record->policy_endpoint_name != NULL);
}

int package_store_read_payload(u32 payload_slot, const u8 **start_out, const u8 **end_out)
{
    u32 payload_kind = 0u;
    u32 image_offset = 0u;
    u32 image_size = 0u;
    u32 image_end = 0u;

    if ((start_out == NULL) || (end_out == NULL))
    {
        return 0;
    }

    *start_out = NULL;
    *end_out = NULL;

    if (!store_ready
        || !package_store_find_payload_slot(
            payload_slot,
            &payload_kind,
            &image_offset,
            &image_size,
            NULL))
    {
        return 0;
    }

    if ((payload_slot == PACKAGE_STORE_PAYLOAD_BOOTSTRAP)
        && (payload_kind == PACKAGE_STORE_PAYLOAD_KIND_BOOTSTRAP_SERVICE))
    {
        if (!package_store_add_u32(image_offset, image_size, &image_end)
            || (image_end > bootstrap_image_size))
        {
            return 0;
        }

        *start_out = &user_bootstrap_service_start + image_offset;
        *end_out = &user_bootstrap_service_start + image_end;
        return 1;
    }

    return 0;
}
