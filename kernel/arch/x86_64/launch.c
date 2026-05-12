#include "launch_x64.h"

#include "bootstrap_catalog.h"
#include "capability_x64.h"
#include "descriptors_x64.h"
#include "package_signing_x64.h"
#include "paging_x64.h"
#include "principal_x64.h"
#include "runtime_image_x64.h"

enum
{
    LAUNCH64_HEADER_FIELD_COUNT = 7u,
    LAUNCH64_HEADER_SIZE = LAUNCH64_HEADER_FIELD_COUNT * 4u,
    LAUNCH64_SIGNER_FIELD_COUNT = 3u,
    LAUNCH64_SIGNER_SIZE = LAUNCH64_SIGNER_FIELD_COUNT * 4u,
    LAUNCH64_MANIFEST_FIELD_COUNT = 23u,
    LAUNCH64_MANIFEST_SIZE = LAUNCH64_MANIFEST_FIELD_COUNT * 4u,
    LAUNCH64_PAYLOAD_FIELD_COUNT = 5u,
    LAUNCH64_PAYLOAD_SIZE = LAUNCH64_PAYLOAD_FIELD_COUNT * 4u,
    LAUNCH64_ARCHIVE_CHECKSUM_OFFSET = 24u,
    LAUNCH64_MANIFEST_LIMIT = 8u,
    LAUNCH64_REQUEST_LIMIT = 16u,
    LAUNCH64_TRUST_MEASURED = 0x00000001u,
    LAUNCH64_TRUST_POLICY_APPROVED = 0x00000002u,
    LAUNCH64_PAYLOAD_KIND_BOOTSTRAP_SERVICE = 1u,
    LAUNCH64_PAYLOAD_KIND_FLAT_BINARY = 2u
};

enum launch64_header_field
{
    LAUNCH64_HEADER_MAGIC = 0u,
    LAUNCH64_HEADER_VERSION = 1u,
    LAUNCH64_HEADER_SIGNER_COUNT = 2u,
    LAUNCH64_HEADER_MANIFEST_COUNT = 3u,
    LAUNCH64_HEADER_PAYLOAD_COUNT = 4u,
    LAUNCH64_HEADER_STRING_BYTES = 5u,
    LAUNCH64_HEADER_ARCHIVE_CHECKSUM = 6u
};

enum launch64_signer_field
{
    LAUNCH64_SIGNER_ID = 0u,
    LAUNCH64_SIGNER_NAME_OFFSET = 1u,
    LAUNCH64_SIGNER_VERIFICATION_TOKEN = 2u
};

enum launch64_manifest_field
{
    LAUNCH64_MANIFEST_SOURCE_SLOT = 0u,
    LAUNCH64_MANIFEST_PACKAGE_ID = 1u,
    LAUNCH64_MANIFEST_PACKAGE_NAME_OFFSET = 2u,
    LAUNCH64_MANIFEST_PACKAGE_VERSION = 3u,
    LAUNCH64_MANIFEST_SIGNER_ID = 4u,
    LAUNCH64_MANIFEST_TRUST_FLAGS = 5u,
    LAUNCH64_MANIFEST_LAUNCH_AUTHORITY_MASK = 6u,
    LAUNCH64_MANIFEST_MAX_INSTANCES = 7u,
    LAUNCH64_MANIFEST_EXECUTABLE_ID = 8u,
    LAUNCH64_MANIFEST_EXECUTABLE_NAME_OFFSET = 9u,
    LAUNCH64_MANIFEST_PROCESS_NAME_OFFSET = 10u,
    LAUNCH64_MANIFEST_PROFILE_NAME_OFFSET = 11u,
    LAUNCH64_MANIFEST_PEER_ENDPOINT_NAME_OFFSET = 12u,
    LAUNCH64_MANIFEST_POLICY_ENDPOINT_NAME_OFFSET = 13u,
    LAUNCH64_MANIFEST_ALLOWED_ENDPOINT_ROLE_MASK = 14u,
    LAUNCH64_MANIFEST_ALLOWED_SERVICE_CLASS_MASK = 15u,
    LAUNCH64_MANIFEST_SCHEDULER_CLASS = 16u,
    LAUNCH64_MANIFEST_SCHEDULER_WEIGHT = 17u,
    LAUNCH64_MANIFEST_SCHEDULER_LATENCY_TARGET_TICKS = 18u,
    LAUNCH64_MANIFEST_SCHEDULER_IO_WAKEUP_DEADLINE_TICKS = 19u,
    LAUNCH64_MANIFEST_CAPABILITY_ADMISSION_LIMIT = 20u,
    LAUNCH64_MANIFEST_LAUNCH_ROLE = 21u,
    LAUNCH64_MANIFEST_PAYLOAD_SLOT = 22u
};

enum launch64_payload_field
{
    LAUNCH64_PAYLOAD_SLOT = 0u,
    LAUNCH64_PAYLOAD_KIND = 1u,
    LAUNCH64_PAYLOAD_IMAGE_OFFSET = 2u,
    LAUNCH64_PAYLOAD_IMAGE_SIZE = 3u,
    LAUNCH64_PAYLOAD_IMAGE_CHECKSUM = 4u
};

struct launch64_payload_metadata
{
    u32 slot;
    u32 kind;
    u32 image_offset;
    u32 image_size;
    u32 image_checksum;
};

struct launch64_archive_view
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

struct launch64_manifest_record
{
    u32 source_slot;
    u32 package_id;
    u32 package_version;
    u32 signer_id;
    u32 trust_flags;
    u32 launch_authority_mask;
    u32 max_instances;
    u32 executable_id;
    u32 scheduler_class;
    u32 scheduler_weight;
    u32 scheduler_latency_target_ticks;
    u32 scheduler_io_wakeup_deadline_ticks;
    u32 capability_admission_limit;
    u32 launch_role;
    u32 payload_slot;
    u32 token;
    u32 launch_state;
    u32 last_denial;
    u32 last_requester;
    u32 last_request_id;
    u32 last_request_status;
    u32 restart_count;
    u32 runtime_generation;
    u32 runtime_token;
    u32 runtime_image_generation;
    u32 runtime_image_token;
    u32 runtime_image_base;
    u32 runtime_image_entry;
    u32 runtime_image_mapped_bytes;
    u32 runtime_image_rights;
    u32 runtime_image_plan_token;
    u32 runtime_image_map_token;
    u32 runtime_image_page_count;
    u32 runtime_image_pml4_index;
    u32 runtime_image_pdpt_index;
    u32 runtime_image_pd_index;
    u32 runtime_entry_transfer_token;
    u32 runtime_image_install_token;
    u32 runtime_image_source_checksum;
    u32 runtime_image_entry_probe;
    u32 runtime_image_map_installed;
    u32 runtime_image_protection_flags;
    u32 runtime_image_protection_token;
    u32 runtime_user_entry_state;
    u32 runtime_user_entry_token;
    u32 runtime_user_entry_rip;
    u32 runtime_user_entry_rsp;
    u32 runtime_user_entry_selectors;
    u32 runtime_user_entry_rflags;
    u32 runtime_user_entry_denial;
    u32 runtime_payload_slot;
    u32 runtime_payload_kind;
    u32 runtime_payload_offset;
    u32 runtime_payload_size;
    u32 runtime_payload_checksum;
    u32 launched_pid;
    u32 launched_principal;
    u32 launched_endpoint_class;
    const char *package_name;
    const char *executable_name;
    const char *process_name;
    const char *profile_name;
};

struct launch64_request_record
{
    u32 request_id;
    u32 operation;
    u32 manifest_index;
    u32 requester_principal;
    u32 requested_pid;
    u32 requested_principal;
    u32 requested_endpoint_class;
    u32 expected_scheduler_class;
    u32 expected_capability_limit;
    u32 observed_capability_count;
    u32 revoked_capability_count;
    u32 runtime_generation;
    u32 runtime_token;
    u32 runtime_image_generation;
    u32 runtime_image_token;
    u32 runtime_image_base;
    u32 runtime_image_entry;
    u32 runtime_image_mapped_bytes;
    u32 runtime_image_rights;
    u32 runtime_image_plan_token;
    u32 runtime_image_map_token;
    u32 runtime_image_page_count;
    u32 runtime_image_pml4_index;
    u32 runtime_image_pdpt_index;
    u32 runtime_image_pd_index;
    u32 runtime_entry_transfer_token;
    u32 runtime_image_install_token;
    u32 runtime_image_source_checksum;
    u32 runtime_image_entry_probe;
    u32 runtime_image_map_installed;
    u32 runtime_image_protection_flags;
    u32 runtime_image_protection_token;
    u32 runtime_user_entry_state;
    u32 runtime_user_entry_token;
    u32 runtime_user_entry_rip;
    u32 runtime_user_entry_rsp;
    u32 runtime_user_entry_selectors;
    u32 runtime_user_entry_rflags;
    u32 runtime_user_entry_denial;
    u32 runtime_payload_slot;
    u32 runtime_payload_kind;
    u32 runtime_payload_offset;
    u32 runtime_payload_size;
    u32 runtime_payload_checksum;
    u32 status;
    u32 denial_reason;
};

static struct launch64_archive_view g_view;
static struct launch64_manifest_record g_manifests[LAUNCH64_MANIFEST_LIMIT];
static struct launch64_request_record g_requests[LAUNCH64_REQUEST_LIMIT];
static u32 g_initialized = 0u;
static u32 g_archive_valid = 0u;
static u32 g_manifest_count = 0u;
static u32 g_ignored_count = 0u;
static u32 g_denial_count = 0u;
static u32 g_service_ready_count = 0u;
static u32 g_service_started_count = 0u;
static u32 g_service_start_denial_count = 0u;
static u32 g_service_start_request_count = 0u;
static u32 g_service_start_approval_count = 0u;
static u32 g_service_start_pending_count = 0u;
static u32 g_service_start_denied_count = 0u;
static u32 g_service_start_completed_count = 0u;
static u32 g_service_stop_request_count = 0u;
static u32 g_service_stop_approval_count = 0u;
static u32 g_service_stop_pending_count = 0u;
static u32 g_service_stop_denied_count = 0u;
static u32 g_service_stop_completed_count = 0u;
static u32 g_service_quiesce_request_count = 0u;
static u32 g_service_quiesce_approval_count = 0u;
static u32 g_service_quiesce_pending_count = 0u;
static u32 g_service_quiesce_denied_count = 0u;
static u32 g_service_quiesce_completed_count = 0u;
static u32 g_service_drain_request_count = 0u;
static u32 g_service_drain_approval_count = 0u;
static u32 g_service_drain_pending_count = 0u;
static u32 g_service_drain_denied_count = 0u;
static u32 g_service_drain_completed_count = 0u;
static u32 g_service_restart_request_count = 0u;
static u32 g_service_restart_approval_count = 0u;
static u32 g_service_restart_pending_count = 0u;
static u32 g_service_restart_denied_count = 0u;
static u32 g_service_restart_completed_count = 0u;
static u32 g_request_log_count = 0u;
static u32 g_next_request_id = 1u;

static u32 launch64_read_u32(const u8 *address)
{
    return (u32)address[0]
        | ((u32)address[1] << 8u)
        | ((u32)address[2] << 16u)
        | ((u32)address[3] << 24u);
}

static int launch64_add_u32(u32 left, u32 right, u32 *result_out)
{
    u64 total;

    if (result_out == 0)
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

static int launch64_mul_u32(u32 left, u32 right, u32 *result_out)
{
    u64 total;

    if (result_out == 0)
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

static int launch64_align_up_4k(u32 value, u32 *result_out)
{
    u32 with_padding;

    if (result_out == 0)
    {
        return 0;
    }

    if (!launch64_add_u32(value, 0x00000FFFu, &with_padding))
    {
        return 0;
    }

    *result_out = with_padding & 0xFFFFF000u;
    return 1;
}

static u32 launch64_page_count_4k(u32 mapped_bytes)
{
    if ((mapped_bytes == 0u) || ((mapped_bytes & (LAUNCH64_IMAGE_MAP_PAGE_BYTES - 1u)) != 0u))
    {
        return 0u;
    }

    return mapped_bytes / LAUNCH64_IMAGE_MAP_PAGE_BYTES;
}

static u32 launch64_pml4_index(u32 address)
{
    return (u32)(((u64)address >> 39u) & 0x1FFu);
}

static u32 launch64_pdpt_index(u32 address)
{
    return (u32)(((u64)address >> 30u) & 0x1FFu);
}

static u32 launch64_pd_index(u32 address)
{
    return (u32)(((u64)address >> 21u) & 0x1FFu);
}

static u32 launch64_hash_archive(void)
{
    u32 digest = 2166136261u;
    u32 index;

    for (index = 0u; index < PACKAGE_STORE_GENERATED_ARCHIVE_SIZE; ++index)
    {
        u8 value = package_store_generated_archive[index];

        if ((index >= LAUNCH64_ARCHIVE_CHECKSUM_OFFSET)
            && (index < (LAUNCH64_ARCHIVE_CHECKSUM_OFFSET + 4u)))
        {
            value = 0u;
        }

        digest ^= value;
        digest *= 16777619u;
    }

    return digest;
}

static u32 launch64_hash_bytes(const u8 *bytes, u32 byte_count)
{
    u32 digest = 2166136261u;
    u32 index;

    if (bytes == 0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        digest ^= bytes[index];
        digest *= 16777619u;
    }

    return (digest != 0u) ? digest : 1u;
}

static u32 launch64_hash_string(const char *text)
{
    u32 digest = 2166136261u;

    if (text == 0)
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

static int launch64_string_equal(const char *left, const char *right)
{
    if ((left == 0) || (right == 0))
    {
        return 0;
    }

    while ((*left != '\0') && (*right != '\0'))
    {
        if (*left != *right)
        {
            return 0;
        }

        ++left;
        ++right;
    }

    return (*left == '\0') && (*right == '\0');
}

static void launch64_reset(void)
{
    u32 index;

    g_view.signer_count = 0u;
    g_view.manifest_count = 0u;
    g_view.payload_count = 0u;
    g_view.string_bytes = 0u;
    g_view.archive_checksum = 0u;
    g_view.signers_offset = 0u;
    g_view.manifests_offset = 0u;
    g_view.payloads_offset = 0u;
    g_view.strings_offset = 0u;
    g_archive_valid = 0u;
    g_manifest_count = 0u;
    g_ignored_count = 0u;
    g_denial_count = 0u;

    for (index = 0u; index < LAUNCH64_MANIFEST_LIMIT; ++index)
    {
        g_manifests[index].source_slot = 0u;
        g_manifests[index].package_id = 0u;
        g_manifests[index].package_version = 0u;
        g_manifests[index].signer_id = 0u;
        g_manifests[index].trust_flags = 0u;
        g_manifests[index].launch_authority_mask = 0u;
        g_manifests[index].max_instances = 0u;
        g_manifests[index].executable_id = 0u;
        g_manifests[index].scheduler_class = 0u;
        g_manifests[index].scheduler_weight = 0u;
        g_manifests[index].scheduler_latency_target_ticks = 0u;
        g_manifests[index].scheduler_io_wakeup_deadline_ticks = 0u;
        g_manifests[index].capability_admission_limit = 0u;
        g_manifests[index].launch_role = 0u;
        g_manifests[index].payload_slot = 0u;
        g_manifests[index].token = 0u;
        g_manifests[index].launch_state = 0u;
        g_manifests[index].last_denial = LAUNCH64_DENY_NONE;
        g_manifests[index].last_requester = 0u;
        g_manifests[index].last_request_id = LAUNCH64_INVALID_REQUEST;
        g_manifests[index].last_request_status = LAUNCH64_REQUEST_EMPTY;
        g_manifests[index].restart_count = 0u;
        g_manifests[index].runtime_generation = 0u;
        g_manifests[index].runtime_token = 0u;
        g_manifests[index].runtime_image_generation = 0u;
        g_manifests[index].runtime_image_token = 0u;
        g_manifests[index].runtime_image_base = 0u;
        g_manifests[index].runtime_image_entry = 0u;
        g_manifests[index].runtime_image_mapped_bytes = 0u;
        g_manifests[index].runtime_image_rights = 0u;
        g_manifests[index].runtime_image_plan_token = 0u;
        g_manifests[index].runtime_image_map_token = 0u;
        g_manifests[index].runtime_image_page_count = 0u;
        g_manifests[index].runtime_image_pml4_index = 0u;
        g_manifests[index].runtime_image_pdpt_index = 0u;
        g_manifests[index].runtime_image_pd_index = 0u;
        g_manifests[index].runtime_entry_transfer_token = 0u;
        g_manifests[index].runtime_image_install_token = 0u;
        g_manifests[index].runtime_image_source_checksum = 0u;
        g_manifests[index].runtime_image_entry_probe = 0u;
        g_manifests[index].runtime_image_map_installed = 0u;
        g_manifests[index].runtime_image_protection_flags = 0u;
        g_manifests[index].runtime_image_protection_token = 0u;
        g_manifests[index].runtime_user_entry_state = 0u;
        g_manifests[index].runtime_user_entry_token = 0u;
        g_manifests[index].runtime_user_entry_rip = 0u;
        g_manifests[index].runtime_user_entry_rsp = 0u;
        g_manifests[index].runtime_user_entry_selectors = 0u;
        g_manifests[index].runtime_user_entry_rflags = 0u;
        g_manifests[index].runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_NONE;
        g_manifests[index].runtime_payload_slot = 0u;
        g_manifests[index].runtime_payload_kind = 0u;
        g_manifests[index].runtime_payload_offset = 0u;
        g_manifests[index].runtime_payload_size = 0u;
        g_manifests[index].runtime_payload_checksum = 0u;
        g_manifests[index].launched_pid = 0u;
        g_manifests[index].launched_principal = 0u;
        g_manifests[index].launched_endpoint_class = 0u;
        g_manifests[index].package_name = 0;
        g_manifests[index].executable_name = 0;
        g_manifests[index].process_name = 0;
        g_manifests[index].profile_name = 0;
    }

    g_service_ready_count = 0u;
    g_service_started_count = 0u;
    g_service_start_denial_count = 0u;
    g_service_start_request_count = 0u;
    g_service_start_approval_count = 0u;
    g_service_start_pending_count = 0u;
    g_service_start_denied_count = 0u;
    g_service_start_completed_count = 0u;
    g_service_stop_request_count = 0u;
    g_service_stop_approval_count = 0u;
    g_service_stop_pending_count = 0u;
    g_service_stop_denied_count = 0u;
    g_service_stop_completed_count = 0u;
    g_service_quiesce_request_count = 0u;
    g_service_quiesce_approval_count = 0u;
    g_service_quiesce_pending_count = 0u;
    g_service_quiesce_denied_count = 0u;
    g_service_quiesce_completed_count = 0u;
    g_service_drain_request_count = 0u;
    g_service_drain_approval_count = 0u;
    g_service_drain_pending_count = 0u;
    g_service_drain_denied_count = 0u;
    g_service_drain_completed_count = 0u;
    g_service_restart_request_count = 0u;
    g_service_restart_approval_count = 0u;
    g_service_restart_pending_count = 0u;
    g_service_restart_denied_count = 0u;
    g_service_restart_completed_count = 0u;
    g_request_log_count = 0u;
    g_next_request_id = 1u;

    for (index = 0u; index < LAUNCH64_REQUEST_LIMIT; ++index)
    {
        g_requests[index].request_id = LAUNCH64_INVALID_REQUEST;
        g_requests[index].operation = LAUNCH64_OPERATION_NONE;
        g_requests[index].manifest_index = LAUNCH64_INVALID_MANIFEST;
        g_requests[index].requester_principal = 0u;
        g_requests[index].requested_pid = 0u;
        g_requests[index].requested_principal = 0u;
        g_requests[index].requested_endpoint_class = 0u;
        g_requests[index].expected_scheduler_class = 0u;
        g_requests[index].expected_capability_limit = 0u;
        g_requests[index].observed_capability_count = 0u;
        g_requests[index].revoked_capability_count = 0u;
        g_requests[index].runtime_generation = 0u;
        g_requests[index].runtime_token = 0u;
        g_requests[index].runtime_image_generation = 0u;
        g_requests[index].runtime_image_token = 0u;
        g_requests[index].runtime_image_base = 0u;
        g_requests[index].runtime_image_entry = 0u;
        g_requests[index].runtime_image_mapped_bytes = 0u;
        g_requests[index].runtime_image_rights = 0u;
        g_requests[index].runtime_image_plan_token = 0u;
        g_requests[index].runtime_image_map_token = 0u;
        g_requests[index].runtime_image_page_count = 0u;
        g_requests[index].runtime_image_pml4_index = 0u;
        g_requests[index].runtime_image_pdpt_index = 0u;
        g_requests[index].runtime_image_pd_index = 0u;
        g_requests[index].runtime_entry_transfer_token = 0u;
        g_requests[index].runtime_image_install_token = 0u;
        g_requests[index].runtime_image_source_checksum = 0u;
        g_requests[index].runtime_image_entry_probe = 0u;
        g_requests[index].runtime_image_map_installed = 0u;
        g_requests[index].runtime_image_protection_flags = 0u;
        g_requests[index].runtime_image_protection_token = 0u;
        g_requests[index].runtime_user_entry_state = 0u;
        g_requests[index].runtime_user_entry_token = 0u;
        g_requests[index].runtime_user_entry_rip = 0u;
        g_requests[index].runtime_user_entry_rsp = 0u;
        g_requests[index].runtime_user_entry_selectors = 0u;
        g_requests[index].runtime_user_entry_rflags = 0u;
        g_requests[index].runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_NONE;
        g_requests[index].runtime_payload_slot = 0u;
        g_requests[index].runtime_payload_kind = 0u;
        g_requests[index].runtime_payload_offset = 0u;
        g_requests[index].runtime_payload_size = 0u;
        g_requests[index].runtime_payload_checksum = 0u;
        g_requests[index].status = LAUNCH64_REQUEST_EMPTY;
        g_requests[index].denial_reason = LAUNCH64_DENY_NONE;
    }
}

static const u8 *launch64_signer_bytes(u32 index)
{
    u32 offset;

    if (index >= g_view.signer_count)
    {
        return 0;
    }

    if (!launch64_mul_u32(index, LAUNCH64_SIGNER_SIZE, &offset)
        || !launch64_add_u32(g_view.signers_offset, offset, &offset)
        || ((offset + LAUNCH64_SIGNER_SIZE) > PACKAGE_STORE_GENERATED_ARCHIVE_SIZE))
    {
        return 0;
    }

    return &package_store_generated_archive[offset];
}

static const u8 *launch64_manifest_bytes(u32 index)
{
    u32 offset;

    if (index >= g_view.manifest_count)
    {
        return 0;
    }

    if (!launch64_mul_u32(index, LAUNCH64_MANIFEST_SIZE, &offset)
        || !launch64_add_u32(g_view.manifests_offset, offset, &offset)
        || ((offset + LAUNCH64_MANIFEST_SIZE) > PACKAGE_STORE_GENERATED_ARCHIVE_SIZE))
    {
        return 0;
    }

    return &package_store_generated_archive[offset];
}

static const u8 *launch64_payload_bytes(u32 index)
{
    u32 offset;

    if (index >= g_view.payload_count)
    {
        return 0;
    }

    if (!launch64_mul_u32(index, LAUNCH64_PAYLOAD_SIZE, &offset)
        || !launch64_add_u32(g_view.payloads_offset, offset, &offset)
        || ((offset + LAUNCH64_PAYLOAD_SIZE) > PACKAGE_STORE_GENERATED_ARCHIVE_SIZE))
    {
        return 0;
    }

    return &package_store_generated_archive[offset];
}

static const char *launch64_string(u32 offset)
{
    u32 index;
    u32 base;

    if (offset >= g_view.string_bytes)
    {
        return 0;
    }

    base = g_view.strings_offset + offset;
    if (base >= PACKAGE_STORE_GENERATED_ARCHIVE_SIZE)
    {
        return 0;
    }

    for (index = offset; index < g_view.string_bytes; ++index)
    {
        if (package_store_generated_archive[g_view.strings_offset + index] == 0u)
        {
            return (const char *)&package_store_generated_archive[base];
        }
    }

    return 0;
}

static int launch64_find_signer_token(u32 signer_id, u32 *token_out)
{
    u32 index;

    for (index = 0u; index < g_view.signer_count; ++index)
    {
        const u8 *record = launch64_signer_bytes(index);
        u32 record_id;

        if (record == 0)
        {
            return 0;
        }

        record_id = launch64_read_u32(record + (LAUNCH64_SIGNER_ID * 4u));
        if (record_id == signer_id)
        {
            if (token_out != 0)
            {
                *token_out = launch64_read_u32(record + (LAUNCH64_SIGNER_VERIFICATION_TOKEN * 4u));
            }

            return 1;
        }
    }

    return 0;
}

static int launch64_payload_metadata_by_slot(
    u32 payload_slot,
    struct launch64_payload_metadata *metadata_out)
{
    u32 index;

    for (index = 0u; index < g_view.payload_count; ++index)
    {
        const u8 *record = launch64_payload_bytes(index);
        u32 record_slot;

        if (record == 0)
        {
            return 0;
        }

        record_slot = launch64_read_u32(record + (LAUNCH64_PAYLOAD_SLOT * 4u));
        if (record_slot == payload_slot)
        {
            if (metadata_out != 0)
            {
                metadata_out->slot = record_slot;
                metadata_out->kind = launch64_read_u32(record + (LAUNCH64_PAYLOAD_KIND * 4u));
                metadata_out->image_offset =
                    launch64_read_u32(record + (LAUNCH64_PAYLOAD_IMAGE_OFFSET * 4u));
                metadata_out->image_size =
                    launch64_read_u32(record + (LAUNCH64_PAYLOAD_IMAGE_SIZE * 4u));
                metadata_out->image_checksum =
                    launch64_read_u32(record + (LAUNCH64_PAYLOAD_IMAGE_CHECKSUM * 4u));
            }

            return 1;
        }
    }

    return 0;
}

static int launch64_find_payload_slot(u32 payload_slot)
{
    struct launch64_payload_metadata metadata;

    if (!launch64_payload_metadata_by_slot(payload_slot, &metadata))
    {
        return 0;
    }

    return ((metadata.kind == LAUNCH64_PAYLOAD_KIND_BOOTSTRAP_SERVICE)
            || (metadata.kind == LAUNCH64_PAYLOAD_KIND_FLAT_BINARY))
        && (metadata.image_size != 0u)
        && (metadata.image_checksum != 0u);
}

static u32 launch64_compute_manifest_token(
    const struct launch64_manifest_record *manifest,
    u32 signer_token)
{
    u32 digest;

    if (manifest == 0)
    {
        return 0u;
    }

    digest = launch64_hash_string(manifest->package_name);
    digest ^= launch64_hash_string(manifest->executable_name);
    digest ^= launch64_hash_string(manifest->process_name);
    digest ^= manifest->package_id;
    digest ^= manifest->package_version << 4;
    digest ^= g_view.archive_checksum << 8;
    digest ^= manifest->trust_flags << 12;
    digest ^= manifest->launch_authority_mask << 16;
    digest ^= manifest->max_instances << 20;
    digest ^= manifest->launch_role << 24;
    digest ^= signer_token;
    digest *= 16777619u;
    return digest;
}

static u32 launch64_mix_runtime_token(u32 digest, u32 value)
{
    digest ^= value;
    digest *= 16777619u;
    return digest;
}

static u32 launch64_compute_runtime_token(const struct launch64_manifest_record *manifest)
{
    u32 digest;

    if (manifest == 0)
    {
        return 0u;
    }

    digest = 2166136261u;
    digest = launch64_mix_runtime_token(digest, manifest->token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_generation);
    digest = launch64_mix_runtime_token(digest, manifest->launched_pid);
    digest = launch64_mix_runtime_token(digest, manifest->launched_principal);
    digest = launch64_mix_runtime_token(digest, manifest->launched_endpoint_class);
    digest = launch64_mix_runtime_token(digest, manifest->package_id);
    digest = launch64_mix_runtime_token(digest, manifest->executable_id);

    return (digest != 0u) ? digest : 1u;
}

static u32 launch64_compute_runtime_image_token(const struct launch64_manifest_record *manifest)
{
    u32 digest;

    if (manifest == 0)
    {
        return 0u;
    }

    digest = 2166136261u;
    digest = launch64_mix_runtime_token(digest, g_view.archive_checksum);
    digest = launch64_mix_runtime_token(digest, manifest->token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_generation);
    digest = launch64_mix_runtime_token(digest, manifest->package_id);
    digest = launch64_mix_runtime_token(digest, manifest->executable_id);
    digest = launch64_mix_runtime_token(digest, manifest->signer_id);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_slot);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_kind);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_offset);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_size);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_checksum);

    return (digest != 0u) ? digest : 1u;
}

static u32 launch64_compute_runtime_image_plan_token(const struct launch64_manifest_record *manifest)
{
    u32 digest;

    if (manifest == 0)
    {
        return 0u;
    }

    digest = 2166136261u;
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_generation);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_base);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_entry);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_mapped_bytes);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_rights);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_offset);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_size);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_checksum);

    return (digest != 0u) ? digest : 1u;
}

static u32 launch64_compute_runtime_image_map_token(const struct launch64_manifest_record *manifest)
{
    u32 digest;

    if (manifest == 0)
    {
        return 0u;
    }

    digest = 2166136261u;
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_plan_token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_base);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_entry);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_mapped_bytes);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_page_count);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_rights);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_pml4_index);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_pdpt_index);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_pd_index);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_install_token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_source_checksum);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_entry_probe);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_map_installed);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_protection_flags);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_protection_token);

    return (digest != 0u) ? digest : 1u;
}

static u32 launch64_compute_runtime_entry_transfer_token(const struct launch64_manifest_record *manifest)
{
    u32 digest;

    if (manifest == 0)
    {
        return 0u;
    }

    digest = 2166136261u;
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_map_token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_install_token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_protection_token);
    digest = launch64_mix_runtime_token(digest, paging64_user_runtime_mapping_install_token());
    digest = launch64_mix_runtime_token(digest, paging64_user_runtime_mapping_protection_token());
    digest = launch64_mix_runtime_token(digest, paging64_user_stack_mapping_protection_token());
    digest = launch64_mix_runtime_token(digest, manifest->runtime_token);
    digest = launch64_mix_runtime_token(digest, manifest->launched_pid);
    digest = launch64_mix_runtime_token(digest, manifest->launched_principal);
    digest = launch64_mix_runtime_token(digest, manifest->launched_endpoint_class);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_entry);
    digest = launch64_mix_runtime_token(digest, LAUNCH64_USER_IMAGE_BASE);
    digest = launch64_mix_runtime_token(digest, LAUNCH64_USER_STACK_TOP);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_payload_checksum);

    return (digest != 0u) ? digest : 1u;
}

static u32 launch64_compute_runtime_user_entry_token(const struct launch64_manifest_record *manifest)
{
    u32 digest;

    if (manifest == 0)
    {
        return 0u;
    }

    digest = 2166136261u;
    digest = launch64_mix_runtime_token(digest, manifest->runtime_entry_transfer_token);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_image_protection_token);
    digest = launch64_mix_runtime_token(digest, descriptors64_gdt_token());
    digest = launch64_mix_runtime_token(digest, descriptors64_tss_token());
    digest = launch64_mix_runtime_token(digest, manifest->runtime_user_entry_state);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_user_entry_rip);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_user_entry_rsp);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_user_entry_selectors);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_user_entry_rflags);
    digest = launch64_mix_runtime_token(digest, manifest->runtime_user_entry_denial);

    return (digest != 0u) ? digest : 1u;
}

static void launch64_clear_runtime_user_entry(struct launch64_manifest_record *manifest)
{
    if (manifest == 0)
    {
        return;
    }

    manifest->runtime_user_entry_state = 0u;
    manifest->runtime_user_entry_token = 0u;
    manifest->runtime_user_entry_rip = 0u;
    manifest->runtime_user_entry_rsp = 0u;
    manifest->runtime_user_entry_selectors = 0u;
    manifest->runtime_user_entry_rflags = 0u;
    manifest->runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_NONE;
    manifest->launch_state &= ~(
        LAUNCH64_STATE_USER_ENTRY_PLANNED
        | LAUNCH64_STATE_USER_ENTRY_BLOCKED
        | LAUNCH64_STATE_USER_ENTRY_READY);
}

static void launch64_clear_runtime_image_plan(struct launch64_manifest_record *manifest)
{
    if (manifest == 0)
    {
        return;
    }

    manifest->runtime_image_token = 0u;
    manifest->runtime_image_base = 0u;
    manifest->runtime_image_entry = 0u;
    manifest->runtime_image_mapped_bytes = 0u;
    manifest->runtime_image_rights = 0u;
    manifest->runtime_image_plan_token = 0u;
    manifest->runtime_image_map_token = 0u;
    manifest->runtime_image_page_count = 0u;
    manifest->runtime_image_pml4_index = 0u;
    manifest->runtime_image_pdpt_index = 0u;
    manifest->runtime_image_pd_index = 0u;
    manifest->runtime_entry_transfer_token = 0u;
    manifest->runtime_image_install_token = 0u;
    manifest->runtime_image_source_checksum = 0u;
    manifest->runtime_image_entry_probe = 0u;
    manifest->runtime_image_map_installed = 0u;
    manifest->runtime_image_protection_flags = 0u;
    manifest->runtime_image_protection_token = 0u;
    launch64_clear_runtime_user_entry(manifest);
    manifest->runtime_payload_slot = 0u;
    manifest->runtime_payload_kind = 0u;
    manifest->runtime_payload_offset = 0u;
    manifest->runtime_payload_size = 0u;
    manifest->runtime_payload_checksum = 0u;
    manifest->launch_state &= ~(
        LAUNCH64_STATE_IMAGE_PLAN_READY
        | LAUNCH64_STATE_IMAGE_MAP_READY
        | LAUNCH64_STATE_IMAGE_MAP_INSTALLED
        | LAUNCH64_STATE_IMAGE_PROTECTED);
}

static u32 launch64_runtime_image_protection_is_valid(u32 protection_flags)
{
    const u32 required =
        PAGING64_RUNTIME_PROTECTION_READ
        | PAGING64_RUNTIME_PROTECTION_EXECUTE
        | PAGING64_RUNTIME_PROTECTION_VIEW_SEALED
        | PAGING64_RUNTIME_PROTECTION_SUPERVISOR_ONLY
        | PAGING64_RUNTIME_PROTECTION_VALIDATION_ONLY;
    const u32 forbidden =
        PAGING64_RUNTIME_PROTECTION_USER_ACCESSIBLE
        | PAGING64_RUNTIME_PROTECTION_WRITABLE;

    return (((protection_flags & required) == required)
        && ((protection_flags & forbidden) == 0u)) ? 1u : 0u;
}

static u32 launch64_user_entry_view_is_valid(u32 protection_flags)
{
    const u32 required =
        PAGING64_RUNTIME_PROTECTION_READ
        | PAGING64_RUNTIME_PROTECTION_EXECUTE
        | PAGING64_RUNTIME_PROTECTION_USER_ACCESSIBLE;
    const u32 forbidden =
        PAGING64_RUNTIME_PROTECTION_WRITABLE
        | PAGING64_RUNTIME_PROTECTION_SUPERVISOR_ONLY
        | PAGING64_RUNTIME_PROTECTION_VALIDATION_ONLY;

    return (((protection_flags & required) == required)
        && ((protection_flags & forbidden) == 0u)) ? 1u : 0u;
}

static u32 launch64_user_stack_view_is_valid(u32 protection_flags)
{
    const u32 required =
        PAGING64_RUNTIME_PROTECTION_READ
        | PAGING64_RUNTIME_PROTECTION_USER_ACCESSIBLE
        | PAGING64_RUNTIME_PROTECTION_WRITABLE;
    const u32 forbidden =
        PAGING64_RUNTIME_PROTECTION_SUPERVISOR_ONLY
        | PAGING64_RUNTIME_PROTECTION_VALIDATION_ONLY;

    return (((protection_flags & required) == required)
        && ((protection_flags & forbidden) == 0u)) ? 1u : 0u;
}

static void launch64_plan_runtime_user_entry(struct launch64_manifest_record *manifest)
{
    u32 user_entry;

    if (manifest == 0)
    {
        return;
    }

    if (!launch64_add_u32(
            manifest->runtime_payload_offset,
            runtime64_transfer_user_entry_offset(),
            &user_entry)
        || !launch64_add_u32(LAUNCH64_USER_IMAGE_BASE, user_entry, &user_entry))
    {
        user_entry = 0u;
    }

    launch64_clear_runtime_user_entry(manifest);
    manifest->runtime_user_entry_state = LAUNCH64_USER_ENTRY_FRAME_PLANNED;
    manifest->runtime_user_entry_rip = user_entry;
    manifest->runtime_user_entry_rsp = LAUNCH64_USER_STACK_TOP;
    manifest->runtime_user_entry_selectors =
        (u32)descriptors64_user_code_selector()
        | ((u32)descriptors64_user_data_selector() << 16);
    manifest->runtime_user_entry_rflags = LAUNCH64_USER_RFLAGS;
    manifest->runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_NONE;

    if ((descriptors64_installed() == 0u)
        || (descriptors64_tss_loaded() == 0u)
        || (descriptors64_user_selectors_ready() == 0u))
    {
        manifest->runtime_user_entry_state |= LAUNCH64_USER_ENTRY_BLOCKED;
        manifest->runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_DESCRIPTOR_STATE;
    }
    else
    {
        manifest->runtime_user_entry_state |= LAUNCH64_USER_ENTRY_DESCRIPTORS_READY;
        if ((paging64_user_runtime_mapping_installed() != 0u)
            && (paging64_user_runtime_mapping_entry_probe() == runtime64_transfer_entry_result())
            && (paging64_user_runtime_mapping_source_checksum()
                == manifest->runtime_image_source_checksum)
            && (launch64_user_entry_view_is_valid(
                paging64_user_runtime_mapping_protection_flags()) != 0u))
        {
            manifest->runtime_user_entry_state |= LAUNCH64_USER_ENTRY_USER_VIEW_READY;
            if ((paging64_user_stack_mapping_installed() != 0u)
                && (launch64_user_stack_view_is_valid(
                    paging64_user_stack_mapping_protection_flags()) != 0u))
            {
                manifest->runtime_user_entry_state |=
                    LAUNCH64_USER_ENTRY_STACK_READY
                    | LAUNCH64_USER_ENTRY_TRANSFER_READY;
            }
            else
            {
                manifest->runtime_user_entry_state |= LAUNCH64_USER_ENTRY_BLOCKED;
                manifest->runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_STACK_VIEW;
            }
        }
        else
        {
            manifest->runtime_user_entry_state |= LAUNCH64_USER_ENTRY_BLOCKED;
            manifest->runtime_user_entry_denial =
                LAUNCH64_USER_ENTRY_DENY_SUPERVISOR_VALIDATION_VIEW;
        }
    }

    manifest->runtime_user_entry_token = launch64_compute_runtime_user_entry_token(manifest);
    manifest->launch_state |= LAUNCH64_STATE_USER_ENTRY_PLANNED;
    if ((manifest->runtime_user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u)
    {
        manifest->launch_state |= LAUNCH64_STATE_USER_ENTRY_READY;
    }
    else
    {
        manifest->launch_state |= LAUNCH64_STATE_USER_ENTRY_BLOCKED;
    }
}

static u32 launch64_refresh_runtime_image(struct launch64_manifest_record *manifest)
{
    struct launch64_payload_metadata metadata;
    u32 payload_end;
    u32 mapped_bytes;
    u32 image_entry;
    u32 image_limit;
    u32 page_count;
    const void *transfer_image;

    if (manifest == 0)
    {
        return 0u;
    }

    manifest->runtime_image_generation = manifest->runtime_generation;
    launch64_clear_runtime_image_plan(manifest);
    if (!launch64_payload_metadata_by_slot(manifest->payload_slot, &metadata)
        || (metadata.kind != LAUNCH64_PAYLOAD_KIND_BOOTSTRAP_SERVICE)
        || (metadata.image_size == 0u)
        || (metadata.image_checksum == 0u)
        || !launch64_add_u32(metadata.image_offset, metadata.image_size, &payload_end)
        || !launch64_add_u32(LAUNCH64_IMAGE_PLAN_BASE, metadata.image_offset, &image_entry))
    {
        manifest->last_denial = LAUNCH64_DENY_IMAGE_PLAN;
        return 0u;
    }

    transfer_image = runtime64_transfer_image_base();
    if (!launch64_align_up_4k(runtime64_transfer_image_size(), &mapped_bytes)
        || !launch64_add_u32(LAUNCH64_IMAGE_PLAN_BASE, mapped_bytes, &image_limit))
    {
        manifest->last_denial = LAUNCH64_DENY_IMAGE_MAP;
        return 0u;
    }

    page_count = launch64_page_count_4k(mapped_bytes);
    if ((page_count == 0u) || (runtime64_transfer_image_size() > mapped_bytes))
    {
        manifest->last_denial = LAUNCH64_DENY_IMAGE_MAP;
        return 0u;
    }

    if (paging64_install_runtime_mapping(LAUNCH64_IMAGE_PLAN_BASE, transfer_image, mapped_bytes) == 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_IMAGE_MAP;
        return 0u;
    }

    manifest->runtime_payload_slot = metadata.slot;
    manifest->runtime_payload_kind = metadata.kind;
    manifest->runtime_payload_offset = metadata.image_offset;
    manifest->runtime_payload_size = metadata.image_size;
    manifest->runtime_payload_checksum = metadata.image_checksum;
    manifest->runtime_image_token = launch64_compute_runtime_image_token(manifest);
    manifest->runtime_image_base = LAUNCH64_IMAGE_PLAN_BASE;
    manifest->runtime_image_entry = image_entry;
    manifest->runtime_image_mapped_bytes = mapped_bytes;
    manifest->runtime_image_rights = LAUNCH64_IMAGE_PLAN_RIGHTS;
    manifest->runtime_image_plan_token = launch64_compute_runtime_image_plan_token(manifest);
    manifest->runtime_image_page_count = page_count;
    manifest->runtime_image_pml4_index = launch64_pml4_index(manifest->runtime_image_base);
    manifest->runtime_image_pdpt_index = launch64_pdpt_index(manifest->runtime_image_base);
    manifest->runtime_image_pd_index = launch64_pd_index(manifest->runtime_image_base);
    manifest->runtime_image_install_token = paging64_runtime_mapping_install_token();
    manifest->runtime_image_source_checksum = paging64_runtime_mapping_source_checksum();
    manifest->runtime_image_entry_probe = paging64_runtime_mapping_entry_probe();
    manifest->runtime_image_map_installed = paging64_runtime_mapping_installed();
    manifest->runtime_image_protection_flags = paging64_runtime_mapping_protection_flags();
    manifest->runtime_image_protection_token = paging64_runtime_mapping_protection_token();
    manifest->runtime_image_map_token = launch64_compute_runtime_image_map_token(manifest);

    if ((manifest->runtime_image_token == 0u)
        || (manifest->runtime_image_plan_token == 0u)
        || (manifest->runtime_image_map_token == 0u)
        || (manifest->runtime_image_install_token == 0u)
        || (manifest->runtime_image_source_checksum == 0u)
        || (manifest->runtime_image_entry_probe != runtime64_transfer_entry_result())
        || (manifest->runtime_image_map_installed == 0u)
        || (manifest->runtime_image_protection_token == 0u)
        || (launch64_runtime_image_protection_is_valid(manifest->runtime_image_protection_flags) == 0u))
    {
        manifest->last_denial = LAUNCH64_DENY_IMAGE_MAP;
        launch64_clear_runtime_image_plan(manifest);
        return 0u;
    }

    if ((paging64_install_user_runtime_mapping(LAUNCH64_USER_IMAGE_BASE, transfer_image, mapped_bytes) == 0u)
        || (paging64_install_user_stack_mapping(LAUNCH64_USER_STACK_TOP, LAUNCH64_USER_STACK_BYTES) == 0u)
        || (paging64_user_runtime_mapping_install_token() == 0u)
        || (paging64_user_runtime_mapping_protection_token() == 0u)
        || (paging64_user_stack_mapping_protection_token() == 0u))
    {
        manifest->last_denial = LAUNCH64_DENY_IMAGE_MAP;
        launch64_clear_runtime_image_plan(manifest);
        return 0u;
    }

    manifest->runtime_entry_transfer_token = launch64_compute_runtime_entry_transfer_token(manifest);
    if (manifest->runtime_entry_transfer_token == 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_IMAGE_MAP;
        launch64_clear_runtime_image_plan(manifest);
        return 0u;
    }

    launch64_plan_runtime_user_entry(manifest);

    manifest->launch_state |=
        LAUNCH64_STATE_IMAGE_PLAN_READY
        | LAUNCH64_STATE_IMAGE_MAP_READY
        | LAUNCH64_STATE_IMAGE_MAP_INSTALLED
        | LAUNCH64_STATE_IMAGE_PROTECTED;
    manifest->last_denial = LAUNCH64_DENY_NONE;
    (void)image_limit;
    return 1u;
}

static int launch64_validate_archive(void)
{
    u32 signer_bytes;
    u32 manifest_bytes;
    u32 payload_bytes;
    u32 strings_end;
    u32 checksum;
    u32 index;

    if (PACKAGE_STORE_GENERATED_ARCHIVE_SIZE < LAUNCH64_HEADER_SIZE)
    {
        return 0;
    }

    if ((launch64_read_u32(&package_store_generated_archive[LAUNCH64_HEADER_MAGIC * 4u])
            != LIMITLESS_BOOTSTRAP_CATALOG_MAGIC)
        || (launch64_read_u32(&package_store_generated_archive[LAUNCH64_HEADER_VERSION * 4u])
            != LIMITLESS_BOOTSTRAP_CATALOG_VERSION))
    {
        return 0;
    }

    g_view.signer_count =
        launch64_read_u32(&package_store_generated_archive[LAUNCH64_HEADER_SIGNER_COUNT * 4u]);
    g_view.manifest_count =
        launch64_read_u32(&package_store_generated_archive[LAUNCH64_HEADER_MANIFEST_COUNT * 4u]);
    g_view.payload_count =
        launch64_read_u32(&package_store_generated_archive[LAUNCH64_HEADER_PAYLOAD_COUNT * 4u]);
    g_view.string_bytes =
        launch64_read_u32(&package_store_generated_archive[LAUNCH64_HEADER_STRING_BYTES * 4u]);
    g_view.archive_checksum =
        launch64_read_u32(&package_store_generated_archive[LAUNCH64_HEADER_ARCHIVE_CHECKSUM * 4u]);

    g_view.signers_offset = LAUNCH64_HEADER_SIZE;
    if (!launch64_mul_u32(g_view.signer_count, LAUNCH64_SIGNER_SIZE, &signer_bytes)
        || !launch64_add_u32(g_view.signers_offset, signer_bytes, &g_view.manifests_offset)
        || !launch64_mul_u32(g_view.manifest_count, LAUNCH64_MANIFEST_SIZE, &manifest_bytes)
        || !launch64_add_u32(g_view.manifests_offset, manifest_bytes, &g_view.payloads_offset)
        || !launch64_mul_u32(g_view.payload_count, LAUNCH64_PAYLOAD_SIZE, &payload_bytes)
        || !launch64_add_u32(g_view.payloads_offset, payload_bytes, &g_view.strings_offset)
        || !launch64_add_u32(g_view.strings_offset, g_view.string_bytes, &strings_end))
    {
        return 0;
    }

    if (strings_end != PACKAGE_STORE_GENERATED_ARCHIVE_SIZE)
    {
        return 0;
    }

    checksum = launch64_hash_archive();
    if ((checksum != g_view.archive_checksum)
        || (checksum != PACKAGE_STORE_GENERATED_ARCHIVE_CHECKSUM))
    {
        return 0;
    }

    if (package_signing64_verify_archive() == 0u)
    {
        return 0;
    }

    for (index = 0u; index < g_view.signer_count; ++index)
    {
        const u8 *record = launch64_signer_bytes(index);

        if ((record == 0)
            || (launch64_string(launch64_read_u32(record + (LAUNCH64_SIGNER_NAME_OFFSET * 4u))) == 0))
        {
            return 0;
        }
    }

    for (index = 0u; index < g_view.payload_count; ++index)
    {
        const u8 *record = launch64_payload_bytes(index);
        u32 payload_slot;
        u32 payload_kind;
        u32 payload_size;
        u32 payload_checksum;

        if (record == 0)
        {
            return 0;
        }

        payload_slot = launch64_read_u32(record + (LAUNCH64_PAYLOAD_SLOT * 4u));
        payload_kind = launch64_read_u32(record + (LAUNCH64_PAYLOAD_KIND * 4u));
        payload_size = launch64_read_u32(record + (LAUNCH64_PAYLOAD_IMAGE_SIZE * 4u));
        payload_checksum = launch64_read_u32(record + (LAUNCH64_PAYLOAD_IMAGE_CHECKSUM * 4u));
        if ((payload_slot == 0u)
            || ((payload_kind != LAUNCH64_PAYLOAD_KIND_BOOTSTRAP_SERVICE)
                && (payload_kind != LAUNCH64_PAYLOAD_KIND_FLAT_BINARY))
            || (payload_size == 0u)
            || (payload_checksum == 0u))
        {
            return 0;
        }
    }

    return 1;
}

static int launch64_read_manifest(u32 index, struct launch64_manifest_record *out_manifest)
{
    const u8 *record;

    if (out_manifest == 0)
    {
        return 0;
    }

    record = launch64_manifest_bytes(index);
    if (record == 0)
    {
        return 0;
    }

    out_manifest->source_slot = launch64_read_u32(record + (LAUNCH64_MANIFEST_SOURCE_SLOT * 4u));
    out_manifest->package_id = launch64_read_u32(record + (LAUNCH64_MANIFEST_PACKAGE_ID * 4u));
    out_manifest->package_name =
        launch64_string(launch64_read_u32(record + (LAUNCH64_MANIFEST_PACKAGE_NAME_OFFSET * 4u)));
    out_manifest->package_version = launch64_read_u32(record + (LAUNCH64_MANIFEST_PACKAGE_VERSION * 4u));
    out_manifest->signer_id = launch64_read_u32(record + (LAUNCH64_MANIFEST_SIGNER_ID * 4u));
    out_manifest->trust_flags = launch64_read_u32(record + (LAUNCH64_MANIFEST_TRUST_FLAGS * 4u));
    out_manifest->launch_authority_mask =
        launch64_read_u32(record + (LAUNCH64_MANIFEST_LAUNCH_AUTHORITY_MASK * 4u));
    out_manifest->max_instances = launch64_read_u32(record + (LAUNCH64_MANIFEST_MAX_INSTANCES * 4u));
    out_manifest->executable_id = launch64_read_u32(record + (LAUNCH64_MANIFEST_EXECUTABLE_ID * 4u));
    out_manifest->executable_name =
        launch64_string(launch64_read_u32(record + (LAUNCH64_MANIFEST_EXECUTABLE_NAME_OFFSET * 4u)));
    out_manifest->process_name =
        launch64_string(launch64_read_u32(record + (LAUNCH64_MANIFEST_PROCESS_NAME_OFFSET * 4u)));
    out_manifest->profile_name =
        launch64_string(launch64_read_u32(record + (LAUNCH64_MANIFEST_PROFILE_NAME_OFFSET * 4u)));
    out_manifest->scheduler_class =
        launch64_read_u32(record + (LAUNCH64_MANIFEST_SCHEDULER_CLASS * 4u));
    out_manifest->scheduler_weight =
        launch64_read_u32(record + (LAUNCH64_MANIFEST_SCHEDULER_WEIGHT * 4u));
    out_manifest->scheduler_latency_target_ticks =
        launch64_read_u32(record + (LAUNCH64_MANIFEST_SCHEDULER_LATENCY_TARGET_TICKS * 4u));
    out_manifest->scheduler_io_wakeup_deadline_ticks =
        launch64_read_u32(record + (LAUNCH64_MANIFEST_SCHEDULER_IO_WAKEUP_DEADLINE_TICKS * 4u));
    out_manifest->capability_admission_limit =
        launch64_read_u32(record + (LAUNCH64_MANIFEST_CAPABILITY_ADMISSION_LIMIT * 4u));
    out_manifest->launch_role = launch64_read_u32(record + (LAUNCH64_MANIFEST_LAUNCH_ROLE * 4u));
    out_manifest->payload_slot = launch64_read_u32(record + (LAUNCH64_MANIFEST_PAYLOAD_SLOT * 4u));
    out_manifest->token = 0u;
    out_manifest->launch_state = 0u;
    out_manifest->last_denial = LAUNCH64_DENY_NONE;
    out_manifest->last_requester = 0u;
    out_manifest->last_request_id = LAUNCH64_INVALID_REQUEST;
    out_manifest->last_request_status = LAUNCH64_REQUEST_EMPTY;
    out_manifest->restart_count = 0u;
    out_manifest->runtime_generation = 0u;
    out_manifest->runtime_token = 0u;
    out_manifest->runtime_image_generation = 0u;
    out_manifest->runtime_image_token = 0u;
    out_manifest->runtime_image_base = 0u;
    out_manifest->runtime_image_entry = 0u;
    out_manifest->runtime_image_mapped_bytes = 0u;
    out_manifest->runtime_image_rights = 0u;
    out_manifest->runtime_image_plan_token = 0u;
    out_manifest->runtime_image_map_token = 0u;
    out_manifest->runtime_image_page_count = 0u;
    out_manifest->runtime_image_pml4_index = 0u;
    out_manifest->runtime_image_pdpt_index = 0u;
    out_manifest->runtime_image_pd_index = 0u;
    out_manifest->runtime_entry_transfer_token = 0u;
    out_manifest->runtime_image_install_token = 0u;
    out_manifest->runtime_image_source_checksum = 0u;
    out_manifest->runtime_image_entry_probe = 0u;
    out_manifest->runtime_image_map_installed = 0u;
    out_manifest->runtime_image_protection_flags = 0u;
    out_manifest->runtime_image_protection_token = 0u;
    out_manifest->runtime_user_entry_state = 0u;
    out_manifest->runtime_user_entry_token = 0u;
    out_manifest->runtime_user_entry_rip = 0u;
    out_manifest->runtime_user_entry_rsp = 0u;
    out_manifest->runtime_user_entry_selectors = 0u;
    out_manifest->runtime_user_entry_rflags = 0u;
    out_manifest->runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_NONE;
    out_manifest->runtime_payload_slot = 0u;
    out_manifest->runtime_payload_kind = 0u;
    out_manifest->runtime_payload_offset = 0u;
    out_manifest->runtime_payload_size = 0u;
    out_manifest->runtime_payload_checksum = 0u;
    out_manifest->launched_pid = 0u;
    out_manifest->launched_principal = 0u;
    out_manifest->launched_endpoint_class = 0u;

    return (out_manifest->package_name != 0)
        && (out_manifest->executable_name != 0)
        && (out_manifest->process_name != 0)
        && (out_manifest->profile_name != 0);
}

static void launch64_load_manifests(void)
{
    u32 index;

    for (index = 0u; index < g_view.manifest_count; ++index)
    {
        struct launch64_manifest_record manifest;
        u32 signer_token = 0u;

        if (!launch64_read_manifest(index, &manifest))
        {
            ++g_denial_count;
            continue;
        }

        if ((manifest.launch_authority_mask & LAUNCH64_AUTHORITY_KERNEL_SERVICE) == 0u)
        {
            ++g_ignored_count;
            continue;
        }

        if (((manifest.trust_flags & (LAUNCH64_TRUST_MEASURED | LAUNCH64_TRUST_POLICY_APPROVED))
                != (LAUNCH64_TRUST_MEASURED | LAUNCH64_TRUST_POLICY_APPROVED))
            || (manifest.max_instances != 1u)
            || !launch64_find_payload_slot(manifest.payload_slot)
            || !launch64_find_signer_token(manifest.signer_id, &signer_token))
        {
            ++g_denial_count;
            continue;
        }

        if (g_manifest_count >= LAUNCH64_MANIFEST_LIMIT)
        {
            ++g_denial_count;
            continue;
        }

        manifest.token = launch64_compute_manifest_token(&manifest, signer_token);
        manifest.launch_state = LAUNCH64_STATE_VERIFIED;
        g_manifests[g_manifest_count] = manifest;
        ++g_manifest_count;
    }
}

void launch64_init(void)
{
    if (g_initialized != 0u)
    {
        return;
    }

    launch64_reset();
    if (launch64_validate_archive())
    {
        g_archive_valid = 1u;
        launch64_load_manifests();
    }
    else
    {
        ++g_denial_count;
    }

    g_initialized = 1u;
}

u32 launch64_archive_valid(void)
{
    launch64_init();
    return g_archive_valid;
}

u32 launch64_archive_checksum(void)
{
    launch64_init();
    return g_view.archive_checksum;
}

u32 launch64_manifest_total_count(void)
{
    launch64_init();
    return g_view.manifest_count;
}

u32 launch64_manifest_count(void)
{
    launch64_init();
    return g_manifest_count;
}

u32 launch64_manifest_ignored_count(void)
{
    launch64_init();
    return g_ignored_count;
}

u32 launch64_manifest_denial_count(void)
{
    launch64_init();
    return g_denial_count;
}

u32 launch64_service_ready_count(void)
{
    launch64_init();
    return g_service_ready_count;
}

u32 launch64_service_started_count(void)
{
    launch64_init();
    return g_service_started_count;
}

static u32 launch64_phase_from_state(u32 launch_state)
{
    if ((launch_state & LAUNCH64_STATE_QUIESCE_READY) != 0u)
    {
        return LAUNCH64_PHASE_QUIESCE_READY;
    }

    if ((launch_state & LAUNCH64_STATE_CAPS_DRAINED) != 0u)
    {
        return LAUNCH64_PHASE_DRAINED;
    }

    if ((launch_state & LAUNCH64_STATE_STARTED) != 0u)
    {
        return LAUNCH64_PHASE_STARTED;
    }

    if ((launch_state & LAUNCH64_STATE_READY) != 0u)
    {
        return LAUNCH64_PHASE_READY;
    }

    if ((launch_state & LAUNCH64_STATE_VERIFIED) != 0u)
    {
        return LAUNCH64_PHASE_VERIFIED;
    }

    return LAUNCH64_PHASE_NONE;
}

u32 launch64_service_drained_count(void)
{
    u32 index;
    u32 count = 0u;

    launch64_init();
    for (index = 0u; index < g_manifest_count; ++index)
    {
        if ((g_manifests[index].launch_state & LAUNCH64_STATE_CAPS_DRAINED) != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 launch64_service_quiesce_ready_count(void)
{
    u32 index;
    u32 count = 0u;

    launch64_init();
    for (index = 0u; index < g_manifest_count; ++index)
    {
        if ((g_manifests[index].launch_state & LAUNCH64_STATE_QUIESCE_READY) != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 launch64_service_start_denial_count(void)
{
    launch64_init();
    return g_service_start_denial_count;
}

u32 launch64_service_start_request_count(void)
{
    launch64_init();
    return g_service_start_request_count;
}

u32 launch64_service_start_approval_count(void)
{
    launch64_init();
    return g_service_start_approval_count;
}

u32 launch64_service_start_pending_count(void)
{
    launch64_init();
    return g_service_start_pending_count;
}

u32 launch64_service_start_denied_count(void)
{
    launch64_init();
    return g_service_start_denied_count;
}

u32 launch64_service_start_completed_count(void)
{
    launch64_init();
    return g_service_start_completed_count;
}

u32 launch64_service_stop_request_count(void)
{
    launch64_init();
    return g_service_stop_request_count;
}

u32 launch64_service_stop_approval_count(void)
{
    launch64_init();
    return g_service_stop_approval_count;
}

u32 launch64_service_stop_pending_count(void)
{
    launch64_init();
    return g_service_stop_pending_count;
}

u32 launch64_service_stop_denied_count(void)
{
    launch64_init();
    return g_service_stop_denied_count;
}

u32 launch64_service_stop_completed_count(void)
{
    launch64_init();
    return g_service_stop_completed_count;
}

u32 launch64_service_quiesce_request_count(void)
{
    launch64_init();
    return g_service_quiesce_request_count;
}

u32 launch64_service_quiesce_approval_count(void)
{
    launch64_init();
    return g_service_quiesce_approval_count;
}

u32 launch64_service_quiesce_pending_count(void)
{
    launch64_init();
    return g_service_quiesce_pending_count;
}

u32 launch64_service_quiesce_denied_count(void)
{
    launch64_init();
    return g_service_quiesce_denied_count;
}

u32 launch64_service_quiesce_completed_count(void)
{
    launch64_init();
    return g_service_quiesce_completed_count;
}

u32 launch64_service_drain_request_count(void)
{
    launch64_init();
    return g_service_drain_request_count;
}

u32 launch64_service_drain_approval_count(void)
{
    launch64_init();
    return g_service_drain_approval_count;
}

u32 launch64_service_drain_pending_count(void)
{
    launch64_init();
    return g_service_drain_pending_count;
}

u32 launch64_service_drain_denied_count(void)
{
    launch64_init();
    return g_service_drain_denied_count;
}

u32 launch64_service_drain_completed_count(void)
{
    launch64_init();
    return g_service_drain_completed_count;
}

u32 launch64_service_restart_request_count(void)
{
    launch64_init();
    return g_service_restart_request_count;
}

u32 launch64_service_restart_approval_count(void)
{
    launch64_init();
    return g_service_restart_approval_count;
}

u32 launch64_service_restart_pending_count(void)
{
    launch64_init();
    return g_service_restart_pending_count;
}

u32 launch64_service_restart_denied_count(void)
{
    launch64_init();
    return g_service_restart_denied_count;
}

u32 launch64_service_restart_completed_count(void)
{
    launch64_init();
    return g_service_restart_completed_count;
}

u32 launch64_request_log_count(void)
{
    launch64_init();
    return g_request_log_count;
}

u32 launch64_requester_can_start(u32 requester_principal)
{
    return (requester_principal == PRINCIPAL64_ID_SYSTEM)
        || (requester_principal == PRINCIPAL64_ID_INIT_SUPERVISOR);
}

u32 launch64_requester_can_stop(u32 requester_principal)
{
    return (requester_principal == PRINCIPAL64_ID_SYSTEM)
        || (requester_principal == PRINCIPAL64_ID_INIT_SUPERVISOR);
}

u32 launch64_requester_can_quiesce(u32 requester_principal)
{
    return (requester_principal == PRINCIPAL64_ID_SYSTEM)
        || (requester_principal == PRINCIPAL64_ID_INIT_SUPERVISOR);
}

u32 launch64_requester_can_drain(u32 requester_principal)
{
    return (requester_principal == PRINCIPAL64_ID_SYSTEM)
        || (requester_principal == PRINCIPAL64_ID_INIT_SUPERVISOR);
}

u32 launch64_requester_can_restart(u32 requester_principal)
{
    return (requester_principal == PRINCIPAL64_ID_SYSTEM)
        || (requester_principal == PRINCIPAL64_ID_INIT_SUPERVISOR);
}

u32 launch64_manifest_by_process(const char *process_name)
{
    u32 index;

    launch64_init();
    for (index = 0u; index < g_manifest_count; ++index)
    {
        if (launch64_string_equal(g_manifests[index].process_name, process_name))
        {
            return index;
        }
    }

    return LAUNCH64_INVALID_MANIFEST;
}

u32 launch64_manifest_by_endpoint_class(u32 endpoint_class)
{
    u32 index;

    launch64_init();
    for (index = 0u; index < g_manifest_count; ++index)
    {
        if (g_manifests[index].launched_endpoint_class == endpoint_class)
        {
            return index;
        }
    }

    return LAUNCH64_INVALID_MANIFEST;
}

static struct launch64_manifest_record *launch64_manifest_mutable(u32 manifest_index)
{
    launch64_init();
    if (manifest_index >= g_manifest_count)
    {
        return 0;
    }

    return &g_manifests[manifest_index];
}

static const struct launch64_manifest_record *launch64_manifest(u32 manifest_index)
{
    launch64_init();
    if (manifest_index >= g_manifest_count)
    {
        return 0;
    }

    return &g_manifests[manifest_index];
}

static u32 launch64_deny_start(struct launch64_manifest_record *manifest, u32 reason)
{
    if (manifest != 0)
    {
        manifest->last_denial = reason;
    }

    ++g_service_start_denial_count;
    return 0u;
}

static u32 launch64_deny_stop(struct launch64_manifest_record *manifest, u32 reason)
{
    if (manifest != 0)
    {
        manifest->last_denial = reason;
        manifest->launch_state |= LAUNCH64_STATE_STOP_DENIED;
    }

    return 0u;
}

static void launch64_count_request_pending(u32 operation)
{
    if (operation == LAUNCH64_OPERATION_START)
    {
        ++g_service_start_pending_count;
    }
    else if (operation == LAUNCH64_OPERATION_STOP)
    {
        ++g_service_stop_pending_count;
    }
    else if (operation == LAUNCH64_OPERATION_QUIESCE)
    {
        ++g_service_quiesce_pending_count;
    }
    else if (operation == LAUNCH64_OPERATION_DRAIN_CAPS)
    {
        ++g_service_drain_pending_count;
    }
    else if (operation == LAUNCH64_OPERATION_RESTART)
    {
        ++g_service_restart_pending_count;
    }
}

static void launch64_uncount_request_pending(u32 operation)
{
    if ((operation == LAUNCH64_OPERATION_START) && (g_service_start_pending_count > 0u))
    {
        --g_service_start_pending_count;
    }
    else if ((operation == LAUNCH64_OPERATION_STOP) && (g_service_stop_pending_count > 0u))
    {
        --g_service_stop_pending_count;
    }
    else if ((operation == LAUNCH64_OPERATION_QUIESCE) && (g_service_quiesce_pending_count > 0u))
    {
        --g_service_quiesce_pending_count;
    }
    else if ((operation == LAUNCH64_OPERATION_DRAIN_CAPS) && (g_service_drain_pending_count > 0u))
    {
        --g_service_drain_pending_count;
    }
    else if ((operation == LAUNCH64_OPERATION_RESTART) && (g_service_restart_pending_count > 0u))
    {
        --g_service_restart_pending_count;
    }
}

static void launch64_count_request_approval(u32 operation)
{
    if (operation == LAUNCH64_OPERATION_START)
    {
        ++g_service_start_approval_count;
    }
    else if (operation == LAUNCH64_OPERATION_STOP)
    {
        ++g_service_stop_approval_count;
    }
    else if (operation == LAUNCH64_OPERATION_QUIESCE)
    {
        ++g_service_quiesce_approval_count;
    }
    else if (operation == LAUNCH64_OPERATION_DRAIN_CAPS)
    {
        ++g_service_drain_approval_count;
    }
    else if (operation == LAUNCH64_OPERATION_RESTART)
    {
        ++g_service_restart_approval_count;
    }
}

static void launch64_count_request_denied(u32 operation)
{
    if (operation == LAUNCH64_OPERATION_START)
    {
        ++g_service_start_denied_count;
    }
    else if (operation == LAUNCH64_OPERATION_STOP)
    {
        ++g_service_stop_denied_count;
    }
    else if (operation == LAUNCH64_OPERATION_QUIESCE)
    {
        ++g_service_quiesce_denied_count;
    }
    else if (operation == LAUNCH64_OPERATION_DRAIN_CAPS)
    {
        ++g_service_drain_denied_count;
    }
    else if (operation == LAUNCH64_OPERATION_RESTART)
    {
        ++g_service_restart_denied_count;
    }
}

static void launch64_count_request_completed(u32 operation)
{
    if (operation == LAUNCH64_OPERATION_START)
    {
        ++g_service_start_completed_count;
    }
    else if (operation == LAUNCH64_OPERATION_STOP)
    {
        ++g_service_stop_completed_count;
    }
    else if (operation == LAUNCH64_OPERATION_QUIESCE)
    {
        ++g_service_quiesce_completed_count;
    }
    else if (operation == LAUNCH64_OPERATION_DRAIN_CAPS)
    {
        ++g_service_drain_completed_count;
    }
    else if (operation == LAUNCH64_OPERATION_RESTART)
    {
        ++g_service_restart_completed_count;
    }
}

static struct launch64_request_record *launch64_find_request(u32 request_id)
{
    u32 index;

    for (index = 0u; index < g_request_log_count; ++index)
    {
        if (g_requests[index].request_id == request_id)
        {
            return &g_requests[index];
        }
    }

    return 0;
}

static struct launch64_request_record *launch64_allocate_request(
    u32 operation,
    u32 requester_principal,
    u32 manifest_index,
    u32 pid,
    u32 principal_id,
    u32 endpoint_class,
    u32 expected_scheduler_class,
    u32 expected_capability_limit)
{
    struct launch64_request_record *request;

    if (g_request_log_count >= LAUNCH64_REQUEST_LIMIT)
    {
        return 0;
    }

    request = &g_requests[g_request_log_count];
    request->request_id = g_next_request_id++;
    request->operation = operation;
    request->manifest_index = manifest_index;
    request->requester_principal = requester_principal;
    request->requested_pid = pid;
    request->requested_principal = principal_id;
    request->requested_endpoint_class = endpoint_class;
    request->expected_scheduler_class = expected_scheduler_class;
    request->expected_capability_limit = expected_capability_limit;
    request->observed_capability_count = 0u;
    request->revoked_capability_count = 0u;
    request->runtime_generation = 0u;
    request->runtime_token = 0u;
    request->runtime_image_generation = 0u;
    request->runtime_image_token = 0u;
    request->runtime_image_base = 0u;
    request->runtime_image_entry = 0u;
    request->runtime_image_mapped_bytes = 0u;
    request->runtime_image_rights = 0u;
    request->runtime_image_plan_token = 0u;
    request->runtime_image_map_token = 0u;
    request->runtime_image_page_count = 0u;
    request->runtime_image_pml4_index = 0u;
    request->runtime_image_pdpt_index = 0u;
    request->runtime_image_pd_index = 0u;
    request->runtime_entry_transfer_token = 0u;
    request->runtime_image_install_token = 0u;
    request->runtime_image_source_checksum = 0u;
    request->runtime_image_entry_probe = 0u;
    request->runtime_image_map_installed = 0u;
    request->runtime_image_protection_flags = 0u;
    request->runtime_image_protection_token = 0u;
    request->runtime_user_entry_state = 0u;
    request->runtime_user_entry_token = 0u;
    request->runtime_user_entry_rip = 0u;
    request->runtime_user_entry_rsp = 0u;
    request->runtime_user_entry_selectors = 0u;
    request->runtime_user_entry_rflags = 0u;
    request->runtime_user_entry_denial = LAUNCH64_USER_ENTRY_DENY_NONE;
    request->runtime_payload_slot = 0u;
    request->runtime_payload_kind = 0u;
    request->runtime_payload_offset = 0u;
    request->runtime_payload_size = 0u;
    request->runtime_payload_checksum = 0u;
    request->status = LAUNCH64_REQUEST_PENDING;
    request->denial_reason = LAUNCH64_DENY_NONE;
    ++g_request_log_count;
    launch64_count_request_pending(operation);
    return request;
}

static void launch64_note_manifest_request(
    struct launch64_manifest_record *manifest,
    const struct launch64_request_record *request)
{
    if ((manifest == 0) || (request == 0))
    {
        return;
    }

    manifest->last_requester = request->requester_principal;
    manifest->last_request_id = request->request_id;
    manifest->last_request_status = request->status;
}

static void launch64_note_request_runtime(
    struct launch64_request_record *request,
    const struct launch64_manifest_record *manifest)
{
    if ((request == 0) || (manifest == 0))
    {
        return;
    }

    request->runtime_generation = manifest->runtime_generation;
    request->runtime_token = manifest->runtime_token;
    request->runtime_image_generation = manifest->runtime_image_generation;
    request->runtime_image_token = manifest->runtime_image_token;
    request->runtime_image_base = manifest->runtime_image_base;
    request->runtime_image_entry = manifest->runtime_image_entry;
    request->runtime_image_mapped_bytes = manifest->runtime_image_mapped_bytes;
    request->runtime_image_rights = manifest->runtime_image_rights;
    request->runtime_image_plan_token = manifest->runtime_image_plan_token;
    request->runtime_image_map_token = manifest->runtime_image_map_token;
    request->runtime_image_page_count = manifest->runtime_image_page_count;
    request->runtime_image_pml4_index = manifest->runtime_image_pml4_index;
    request->runtime_image_pdpt_index = manifest->runtime_image_pdpt_index;
    request->runtime_image_pd_index = manifest->runtime_image_pd_index;
    request->runtime_entry_transfer_token = manifest->runtime_entry_transfer_token;
    request->runtime_image_install_token = manifest->runtime_image_install_token;
    request->runtime_image_source_checksum = manifest->runtime_image_source_checksum;
    request->runtime_image_entry_probe = manifest->runtime_image_entry_probe;
    request->runtime_image_map_installed = manifest->runtime_image_map_installed;
    request->runtime_image_protection_flags = manifest->runtime_image_protection_flags;
    request->runtime_image_protection_token = manifest->runtime_image_protection_token;
    request->runtime_user_entry_state = manifest->runtime_user_entry_state;
    request->runtime_user_entry_token = manifest->runtime_user_entry_token;
    request->runtime_user_entry_rip = manifest->runtime_user_entry_rip;
    request->runtime_user_entry_rsp = manifest->runtime_user_entry_rsp;
    request->runtime_user_entry_selectors = manifest->runtime_user_entry_selectors;
    request->runtime_user_entry_rflags = manifest->runtime_user_entry_rflags;
    request->runtime_user_entry_denial = manifest->runtime_user_entry_denial;
    request->runtime_payload_slot = manifest->runtime_payload_slot;
    request->runtime_payload_kind = manifest->runtime_payload_kind;
    request->runtime_payload_offset = manifest->runtime_payload_offset;
    request->runtime_payload_size = manifest->runtime_payload_size;
    request->runtime_payload_checksum = manifest->runtime_payload_checksum;
}

static void launch64_complete_pending_request(struct launch64_request_record *request)
{
    if ((request != 0) && ((request->status & LAUNCH64_REQUEST_PENDING) != 0u))
    {
        request->status &= ~LAUNCH64_REQUEST_PENDING;
        launch64_uncount_request_pending(request->operation);
    }
}

static u32 launch64_deny_request(
    struct launch64_request_record *request,
    struct launch64_manifest_record *manifest,
    u32 reason,
    u32 count_start_denial)
{
    if (reason == LAUNCH64_DENY_NONE)
    {
        reason = LAUNCH64_DENY_CONTRACT_MISMATCH;
    }

    if (count_start_denial != 0u)
    {
        launch64_deny_start(manifest, reason);
    }
    else if (manifest != 0)
    {
        manifest->last_denial = reason;
    }

    launch64_complete_pending_request(request);
    if (request != 0)
    {
        request->status = LAUNCH64_REQUEST_DENIED | LAUNCH64_REQUEST_COMPLETED;
        request->denial_reason = reason;
        launch64_count_request_denied(request->operation);
        launch64_count_request_completed(request->operation);
    }

    if (manifest != 0)
    {
        manifest->last_request_status = (request != 0)
            ? request->status
            : (LAUNCH64_REQUEST_DENIED | LAUNCH64_REQUEST_COMPLETED);
    }

    return 0u;
}

static void launch64_approve_request(
    struct launch64_request_record *request,
    struct launch64_manifest_record *manifest)
{
    if (request != 0)
    {
        request->status |= LAUNCH64_REQUEST_APPROVED;
        request->denial_reason = LAUNCH64_DENY_NONE;
        launch64_count_request_approval(request->operation);
    }

    if (manifest != 0)
    {
        manifest->last_request_status = (request != 0) ? request->status : LAUNCH64_REQUEST_APPROVED;
    }
}

static void launch64_finish_request(
    struct launch64_request_record *request,
    struct launch64_manifest_record *manifest)
{
    launch64_complete_pending_request(request);
    if (request != 0)
    {
        request->status |= LAUNCH64_REQUEST_COMPLETED;
        launch64_count_request_completed(request->operation);
    }

    if (manifest != 0)
    {
        manifest->last_request_status = (request != 0)
            ? request->status
            : (LAUNCH64_REQUEST_APPROVED | LAUNCH64_REQUEST_COMPLETED);
    }
}

static u32 launch64_prepare_service(u32 manifest_index, u32 expected_scheduler_class, u32 expected_capability_limit)
{
    struct launch64_manifest_record *manifest = launch64_manifest_mutable(manifest_index);

    if (manifest == 0)
    {
        return launch64_deny_start(0, LAUNCH64_DENY_INVALID_MANIFEST);
    }

    if (((manifest->launch_state & LAUNCH64_STATE_VERIFIED) == 0u)
        || (manifest->scheduler_class != expected_scheduler_class)
        || (manifest->capability_admission_limit != expected_capability_limit)
        || (manifest->token == 0u))
    {
        return launch64_deny_start(manifest, LAUNCH64_DENY_CONTRACT_MISMATCH);
    }

    if ((manifest->launch_state & LAUNCH64_STATE_READY) == 0u)
    {
        manifest->launch_state |= LAUNCH64_STATE_READY;
        ++g_service_ready_count;
    }

    return 1u;
}

static u32 launch64_start_service(
    u32 manifest_index,
    u32 pid,
    u32 principal_id,
    u32 endpoint_class,
    u32 expected_scheduler_class,
    u32 expected_capability_limit)
{
    struct launch64_manifest_record *manifest = launch64_manifest_mutable(manifest_index);

    if (manifest == 0)
    {
        return launch64_deny_start(0, LAUNCH64_DENY_INVALID_MANIFEST);
    }

    if ((manifest->launch_state & LAUNCH64_STATE_STARTED) != 0u)
    {
        return launch64_deny_start(manifest, LAUNCH64_DENY_DUPLICATE_START);
    }

    if ((pid == 0u) || (principal_id == 0u))
    {
        return launch64_deny_start(manifest, LAUNCH64_DENY_INVALID_BINDING);
    }

    if (launch64_prepare_service(
            manifest_index,
            expected_scheduler_class,
            expected_capability_limit) == 0u)
    {
        return 0u;
    }

    manifest->launched_pid = pid;
    manifest->launched_principal = principal_id;
    manifest->launched_endpoint_class = endpoint_class;
    manifest->runtime_generation = 1u;
    manifest->runtime_token = launch64_compute_runtime_token(manifest);
    if (launch64_refresh_runtime_image(manifest) == 0u)
    {
        return launch64_deny_start(manifest, manifest->last_denial);
    }

    manifest->launch_state |= LAUNCH64_STATE_STARTED;
    ++g_service_started_count;
    return 1u;
}

u32 launch64_request_service_start(
    u32 requester_principal,
    u32 manifest_index,
    u32 pid,
    u32 principal_id,
    u32 endpoint_class,
    u32 expected_scheduler_class,
    u32 expected_capability_limit)
{
    struct launch64_manifest_record *manifest;
    struct launch64_request_record *request;

    launch64_init();
    ++g_service_start_request_count;
    request = launch64_allocate_request(
        LAUNCH64_OPERATION_START,
        requester_principal,
        manifest_index,
        pid,
        principal_id,
        endpoint_class,
        expected_scheduler_class,
        expected_capability_limit);
    if (request == 0)
    {
        ++g_service_start_denied_count;
        return launch64_deny_start(0, LAUNCH64_DENY_REQUEST_LOG_FULL);
    }

    manifest = launch64_manifest_mutable(manifest_index);
    launch64_note_manifest_request(manifest, request);
    if (manifest == 0)
    {
        return launch64_deny_request(request, 0, LAUNCH64_DENY_INVALID_MANIFEST, 1u);
    }

    if (launch64_requester_can_start(requester_principal) == 0u)
    {
        return launch64_deny_request(request, manifest, LAUNCH64_DENY_UNAUTHORIZED_REQUESTER, 1u);
    }

    if (launch64_start_service(
            manifest_index,
            pid,
            principal_id,
            endpoint_class,
            expected_scheduler_class,
            expected_capability_limit) == 0u)
    {
        return launch64_deny_request(request, manifest, manifest->last_denial, 0u);
    }

    launch64_note_request_runtime(request, manifest);
    launch64_approve_request(request, manifest);
    launch64_finish_request(request, manifest);
    return 1u;
}

static u32 launch64_stop_service(u32 manifest_index)
{
    struct launch64_manifest_record *manifest = launch64_manifest_mutable(manifest_index);

    if (manifest == 0)
    {
        return launch64_deny_stop(0, LAUNCH64_DENY_INVALID_MANIFEST);
    }

    if ((manifest->launch_state & LAUNCH64_STATE_STARTED) == 0u)
    {
        return launch64_deny_stop(manifest, LAUNCH64_DENY_NOT_STARTED);
    }

    return launch64_deny_stop(manifest, LAUNCH64_DENY_PROTECTED_SERVICE);
}

static u32 launch64_quiesce_preflight(
    u32 manifest_index,
    struct launch64_request_record *request)
{
    struct launch64_manifest_record *manifest = launch64_manifest_mutable(manifest_index);
    u32 live_capabilities;

    if (manifest == 0)
    {
        return 0u;
    }

    if ((manifest->launch_state & LAUNCH64_STATE_STARTED) == 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_NOT_STARTED;
        return 0u;
    }

    live_capabilities = capability64_live_for_endpoint_class(manifest->launched_endpoint_class);
    if (request != 0)
    {
        request->observed_capability_count = live_capabilities;
    }

    if (live_capabilities != 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_ACTIVE_CAPABILITIES;
        manifest->launch_state &= ~LAUNCH64_STATE_QUIESCE_READY;
        return 0u;
    }

    manifest->launch_state |= LAUNCH64_STATE_QUIESCE_READY;
    manifest->last_denial = LAUNCH64_DENY_NONE;
    return 1u;
}

u32 launch64_request_service_quiesce(u32 requester_principal, u32 manifest_index)
{
    struct launch64_manifest_record *manifest;
    struct launch64_request_record *request;

    launch64_init();
    ++g_service_quiesce_request_count;
    request = launch64_allocate_request(
        LAUNCH64_OPERATION_QUIESCE,
        requester_principal,
        manifest_index,
        0u,
        0u,
        0u,
        0u,
        0u);
    if (request == 0)
    {
        ++g_service_quiesce_denied_count;
        return 0u;
    }

    manifest = launch64_manifest_mutable(manifest_index);
    launch64_note_manifest_request(manifest, request);
    if (manifest == 0)
    {
        return launch64_deny_request(request, 0, LAUNCH64_DENY_INVALID_MANIFEST, 0u);
    }

    if (launch64_requester_can_quiesce(requester_principal) == 0u)
    {
        return launch64_deny_request(request, manifest, LAUNCH64_DENY_UNAUTHORIZED_REQUESTER, 0u);
    }

    if (launch64_quiesce_preflight(manifest_index, request) == 0u)
    {
        return launch64_deny_request(request, manifest, manifest->last_denial, 0u);
    }

    launch64_approve_request(request, manifest);
    launch64_finish_request(request, manifest);
    return 1u;
}

static u32 launch64_drain_capabilities(
    u32 manifest_index,
    struct launch64_request_record *request)
{
    struct launch64_manifest_record *manifest = launch64_manifest_mutable(manifest_index);
    u32 live_capabilities;
    u32 revoked_capabilities;

    if (manifest == 0)
    {
        return 0u;
    }

    if ((manifest->launch_state & LAUNCH64_STATE_STARTED) == 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_NOT_STARTED;
        return 0u;
    }

    live_capabilities = capability64_live_for_endpoint_class(manifest->launched_endpoint_class);
    revoked_capabilities = capability64_revoke_endpoint_class(manifest->launched_endpoint_class);
    if (request != 0)
    {
        request->observed_capability_count = live_capabilities;
        request->revoked_capability_count = revoked_capabilities;
    }

    if (capability64_live_for_endpoint_class(manifest->launched_endpoint_class) != 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_ACTIVE_CAPABILITIES;
        manifest->launch_state &= ~LAUNCH64_STATE_CAPS_DRAINED;
        return 0u;
    }

    manifest->launch_state |= LAUNCH64_STATE_CAPS_DRAINED;
    manifest->last_denial = LAUNCH64_DENY_NONE;
    return 1u;
}

u32 launch64_request_service_drain(u32 requester_principal, u32 manifest_index)
{
    struct launch64_manifest_record *manifest;
    struct launch64_request_record *request;

    launch64_init();
    ++g_service_drain_request_count;
    request = launch64_allocate_request(
        LAUNCH64_OPERATION_DRAIN_CAPS,
        requester_principal,
        manifest_index,
        0u,
        0u,
        0u,
        0u,
        0u);
    if (request == 0)
    {
        ++g_service_drain_denied_count;
        return 0u;
    }

    manifest = launch64_manifest_mutable(manifest_index);
    launch64_note_manifest_request(manifest, request);
    if (manifest == 0)
    {
        return launch64_deny_request(request, 0, LAUNCH64_DENY_INVALID_MANIFEST, 0u);
    }

    if (launch64_requester_can_drain(requester_principal) == 0u)
    {
        return launch64_deny_request(request, manifest, LAUNCH64_DENY_UNAUTHORIZED_REQUESTER, 0u);
    }

    if (launch64_drain_capabilities(manifest_index, request) == 0u)
    {
        return launch64_deny_request(request, manifest, manifest->last_denial, 0u);
    }

    launch64_approve_request(request, manifest);
    launch64_finish_request(request, manifest);
    return 1u;
}

static u32 launch64_restart_service(
    u32 manifest_index,
    struct launch64_request_record *request)
{
    struct launch64_manifest_record *manifest = launch64_manifest_mutable(manifest_index);
    u32 live_capabilities;

    if (manifest == 0)
    {
        return 0u;
    }

    if ((manifest->launch_state & LAUNCH64_STATE_STARTED) == 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_NOT_STARTED;
        return 0u;
    }

    live_capabilities = capability64_live_for_endpoint_class(manifest->launched_endpoint_class);
    if (request != 0)
    {
        request->observed_capability_count = live_capabilities;
    }

    if (live_capabilities != 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_ACTIVE_CAPABILITIES;
        manifest->launch_state &= ~(LAUNCH64_STATE_CAPS_DRAINED | LAUNCH64_STATE_QUIESCE_READY);
        return 0u;
    }

    if ((manifest->launch_state & LAUNCH64_STATE_QUIESCE_READY) == 0u)
    {
        manifest->last_denial = LAUNCH64_DENY_NOT_QUIESCE_READY;
        return 0u;
    }

    ++manifest->restart_count;
    ++manifest->runtime_generation;
    if (manifest->runtime_generation == 0u)
    {
        manifest->runtime_generation = 1u;
    }

    manifest->runtime_token = launch64_compute_runtime_token(manifest);
    if (launch64_refresh_runtime_image(manifest) == 0u)
    {
        if (manifest->last_denial == LAUNCH64_DENY_NONE)
        {
            manifest->last_denial = LAUNCH64_DENY_IMAGE_PLAN;
        }
        return 0u;
    }

    launch64_note_request_runtime(request, manifest);
    manifest->launch_state &= ~(LAUNCH64_STATE_CAPS_DRAINED | LAUNCH64_STATE_QUIESCE_READY);
    manifest->launch_state |= LAUNCH64_STATE_STARTED;
    manifest->last_denial = LAUNCH64_DENY_NONE;
    return 1u;
}

u32 launch64_request_service_restart(u32 requester_principal, u32 manifest_index)
{
    struct launch64_manifest_record *manifest;
    struct launch64_request_record *request;

    launch64_init();
    ++g_service_restart_request_count;
    request = launch64_allocate_request(
        LAUNCH64_OPERATION_RESTART,
        requester_principal,
        manifest_index,
        0u,
        0u,
        0u,
        0u,
        0u);
    if (request == 0)
    {
        ++g_service_restart_denied_count;
        return 0u;
    }

    manifest = launch64_manifest_mutable(manifest_index);
    launch64_note_manifest_request(manifest, request);
    if (manifest == 0)
    {
        return launch64_deny_request(request, 0, LAUNCH64_DENY_INVALID_MANIFEST, 0u);
    }

    if (launch64_requester_can_restart(requester_principal) == 0u)
    {
        return launch64_deny_request(request, manifest, LAUNCH64_DENY_UNAUTHORIZED_REQUESTER, 0u);
    }

    if (launch64_restart_service(manifest_index, request) == 0u)
    {
        return launch64_deny_request(request, manifest, manifest->last_denial, 0u);
    }

    launch64_approve_request(request, manifest);
    launch64_finish_request(request, manifest);
    return 1u;
}

u32 launch64_request_service_stop(u32 requester_principal, u32 manifest_index)
{
    struct launch64_manifest_record *manifest;
    struct launch64_request_record *request;

    launch64_init();
    ++g_service_stop_request_count;
    request = launch64_allocate_request(
        LAUNCH64_OPERATION_STOP,
        requester_principal,
        manifest_index,
        0u,
        0u,
        0u,
        0u,
        0u);
    if (request == 0)
    {
        ++g_service_stop_denied_count;
        return 0u;
    }

    manifest = launch64_manifest_mutable(manifest_index);
    launch64_note_manifest_request(manifest, request);
    if (manifest == 0)
    {
        return launch64_deny_request(request, 0, LAUNCH64_DENY_INVALID_MANIFEST, 0u);
    }

    if (launch64_requester_can_stop(requester_principal) == 0u)
    {
        return launch64_deny_request(request, manifest, LAUNCH64_DENY_UNAUTHORIZED_REQUESTER, 0u);
    }

    if (launch64_stop_service(manifest_index) == 0u)
    {
        return launch64_deny_request(request, manifest, manifest->last_denial, 0u);
    }

    launch64_approve_request(request, manifest);
    launch64_finish_request(request, manifest);
    return 1u;
}

u32 launch64_manifest_source_slot(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->source_slot : 0u;
}

u32 launch64_manifest_package_id(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->package_id : 0u;
}

u32 launch64_manifest_executable_id(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->executable_id : 0u;
}

u32 launch64_manifest_signer_id(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->signer_id : 0u;
}

u32 launch64_manifest_scheduler_class(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->scheduler_class : 0u;
}

u32 launch64_manifest_capability_limit(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->capability_admission_limit : 0u;
}

u32 launch64_manifest_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->token : 0u;
}

u32 launch64_manifest_launch_state(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->launch_state : 0u;
}

u32 launch64_manifest_lifecycle_phase(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? launch64_phase_from_state(manifest->launch_state) : LAUNCH64_PHASE_NONE;
}

u32 launch64_manifest_restart_count(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->restart_count : 0u;
}

u32 launch64_manifest_runtime_generation(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_generation : 0u;
}

u32 launch64_manifest_runtime_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_token : 0u;
}

u32 launch64_manifest_accepts_runtime_token(u32 manifest_index, u32 runtime_token)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    if ((manifest == 0) || (runtime_token == 0u))
    {
        return 0u;
    }

    return (manifest->runtime_token == runtime_token) ? 1u : 0u;
}

u32 launch64_manifest_runtime_image_generation(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_generation : 0u;
}

u32 launch64_manifest_runtime_image_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_token : 0u;
}

u32 launch64_manifest_runtime_image_base(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_base : 0u;
}

u32 launch64_manifest_runtime_image_entry(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_entry : 0u;
}

u32 launch64_manifest_runtime_image_mapped_bytes(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_mapped_bytes : 0u;
}

u32 launch64_manifest_runtime_image_rights(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_rights : 0u;
}

u32 launch64_manifest_runtime_image_plan_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_plan_token : 0u;
}

u32 launch64_manifest_runtime_image_map_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_map_token : 0u;
}

u32 launch64_manifest_runtime_image_page_count(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_page_count : 0u;
}

u32 launch64_manifest_runtime_image_pml4_index(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_pml4_index : 0u;
}

u32 launch64_manifest_runtime_image_pdpt_index(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_pdpt_index : 0u;
}

u32 launch64_manifest_runtime_image_pd_index(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_pd_index : 0u;
}

u32 launch64_manifest_runtime_entry_transfer_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_entry_transfer_token : 0u;
}

u32 launch64_manifest_runtime_image_install_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_install_token : 0u;
}

u32 launch64_manifest_runtime_image_source_checksum(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_source_checksum : 0u;
}

u32 launch64_manifest_runtime_image_entry_probe(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_entry_probe : 0u;
}

u32 launch64_manifest_runtime_image_map_installed(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_map_installed : 0u;
}

u32 launch64_manifest_runtime_image_protection_flags(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_protection_flags : 0u;
}

u32 launch64_manifest_runtime_image_protection_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_image_protection_token : 0u;
}

u32 launch64_manifest_runtime_user_entry_state(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_user_entry_state : 0u;
}

u32 launch64_manifest_runtime_user_entry_token(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_user_entry_token : 0u;
}

u32 launch64_manifest_runtime_user_entry_rip(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_user_entry_rip : 0u;
}

u32 launch64_manifest_runtime_user_entry_rsp(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_user_entry_rsp : 0u;
}

u32 launch64_manifest_runtime_user_entry_selectors(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_user_entry_selectors : 0u;
}

u32 launch64_manifest_runtime_user_entry_rflags(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_user_entry_rflags : 0u;
}

u32 launch64_manifest_runtime_user_entry_denial(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_user_entry_denial : LAUNCH64_USER_ENTRY_DENY_DESCRIPTOR_STATE;
}

u32 launch64_manifest_runtime_payload_slot(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_payload_slot : 0u;
}

u32 launch64_manifest_runtime_payload_kind(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_payload_kind : 0u;
}

u32 launch64_manifest_runtime_payload_offset(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_payload_offset : 0u;
}

u32 launch64_manifest_runtime_payload_size(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_payload_size : 0u;
}

u32 launch64_manifest_runtime_payload_checksum(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->runtime_payload_checksum : 0u;
}

u32 launch64_payload_size_by_slot(u32 payload_slot)
{
    struct launch64_payload_metadata metadata;

    launch64_init();
    if (!launch64_payload_metadata_by_slot(payload_slot, &metadata))
    {
        return 0u;
    }

    return metadata.image_size;
}

u32 launch64_payload_checksum_by_slot(u32 payload_slot)
{
    struct launch64_payload_metadata metadata;

    launch64_init();
    if (!launch64_payload_metadata_by_slot(payload_slot, &metadata))
    {
        return 0u;
    }

    return metadata.image_checksum;
}

u32 launch64_stage_disk_flat_binary(
    u32 payload_slot,
    const void *source,
    u32 source_bytes,
    u32 mapped_bytes,
    u32 entry_probe_result,
    u32 *entry_rip_out,
    u32 *entry_rsp_out,
    u32 *entry_selectors_out,
    u32 *entry_rflags_out,
    u32 *map_token_out)
{
    struct launch64_payload_metadata metadata;
    u32 source_checksum;
    u32 page_count;
    u32 entry_rip;
    u32 selectors;
    u32 token;

    launch64_init();
    if (entry_rip_out != 0)
    {
        *entry_rip_out = 0u;
    }
    if (entry_rsp_out != 0)
    {
        *entry_rsp_out = 0u;
    }
    if (entry_selectors_out != 0)
    {
        *entry_selectors_out = 0u;
    }
    if (entry_rflags_out != 0)
    {
        *entry_rflags_out = 0u;
    }
    if (map_token_out != 0)
    {
        *map_token_out = 0u;
    }

    if ((source == 0)
        || (source_bytes == 0u)
        || (mapped_bytes == 0u)
        || (source_bytes > mapped_bytes)
        || ((mapped_bytes & (LAUNCH64_IMAGE_MAP_PAGE_BYTES - 1u)) != 0u)
        || (entry_probe_result == 0u)
        || !launch64_payload_metadata_by_slot(payload_slot, &metadata)
        || (metadata.kind != LAUNCH64_PAYLOAD_KIND_FLAT_BINARY)
        || (metadata.image_offset != 0u)
        || (metadata.image_size != source_bytes)
        || (metadata.image_checksum == 0u))
    {
        return 0u;
    }

    source_checksum = launch64_hash_bytes((const u8 *)source, source_bytes);
    if (source_checksum != metadata.image_checksum)
    {
        return 0u;
    }
    if (package_signing64_verify_payload(
            payload_slot,
            source,
            source_bytes,
            source_checksum) == 0u)
    {
        return 0u;
    }

    page_count = launch64_page_count_4k(mapped_bytes);
    if ((page_count == 0u)
        || (paging64_install_runtime_mapping(
                LAUNCH64_DISK_IMAGE_PLAN_BASE,
                source,
                mapped_bytes) == 0u)
        || (paging64_runtime_mapping_entry_probe() != entry_probe_result)
        || (paging64_runtime_mapping_install_token() == 0u)
        || (paging64_runtime_mapping_source_checksum() == 0u)
        || (launch64_runtime_image_protection_is_valid(
                paging64_runtime_mapping_protection_flags()) == 0u)
        || (paging64_install_user_runtime_mapping(
                LAUNCH64_DISK_USER_IMAGE_BASE,
                source,
                mapped_bytes) == 0u)
        || (paging64_install_user_stack_mapping(
                LAUNCH64_USER_STACK_TOP,
                LAUNCH64_USER_STACK_BYTES) == 0u)
        || (paging64_user_runtime_mapping_entry_probe() != entry_probe_result)
        || (paging64_user_runtime_mapping_install_token() == 0u)
        || (paging64_user_runtime_mapping_protection_token() == 0u)
        || (paging64_user_stack_mapping_protection_token() == 0u)
        || (launch64_user_entry_view_is_valid(
                paging64_user_runtime_mapping_protection_flags()) == 0u)
        || (launch64_user_stack_view_is_valid(
                paging64_user_stack_mapping_protection_flags()) == 0u))
    {
        return 0u;
    }

    entry_rip = LAUNCH64_DISK_USER_IMAGE_BASE + LAUNCH64_DISK_USER_ENTRY_OFFSET;
    selectors =
        (descriptors64_user_code_selector() & 0xFFFFu)
        | ((descriptors64_user_data_selector() & 0xFFFFu) << 16);
    token = 2166136261u;
    token = launch64_mix_runtime_token(token, payload_slot);
    token = launch64_mix_runtime_token(token, source_bytes);
    token = launch64_mix_runtime_token(token, source_checksum);
    token = launch64_mix_runtime_token(token, mapped_bytes);
    token = launch64_mix_runtime_token(token, page_count);
    token = launch64_mix_runtime_token(token, paging64_runtime_mapping_install_token());
    token = launch64_mix_runtime_token(token, paging64_user_runtime_mapping_install_token());
    token = launch64_mix_runtime_token(token, paging64_user_stack_mapping_protection_token());
    token = launch64_mix_runtime_token(token, entry_rip);
    token = (token != 0u) ? token : 1u;

    if (entry_rip_out != 0)
    {
        *entry_rip_out = entry_rip;
    }
    if (entry_rsp_out != 0)
    {
        *entry_rsp_out = LAUNCH64_USER_STACK_TOP;
    }
    if (entry_selectors_out != 0)
    {
        *entry_selectors_out = selectors;
    }
    if (entry_rflags_out != 0)
    {
        *entry_rflags_out = LAUNCH64_USER_RFLAGS;
    }
    if (map_token_out != 0)
    {
        *map_token_out = token;
    }

    return token;
}

u32 launch64_manifest_launched_pid(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->launched_pid : 0u;
}

u32 launch64_manifest_launched_principal(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->launched_principal : 0u;
}

u32 launch64_manifest_launched_endpoint_class(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->launched_endpoint_class : 0u;
}

u32 launch64_manifest_last_requester(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->last_requester : 0u;
}

u32 launch64_manifest_last_denial(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->last_denial : LAUNCH64_DENY_INVALID_MANIFEST;
}

u32 launch64_manifest_last_request_id(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->last_request_id : LAUNCH64_INVALID_REQUEST;
}

u32 launch64_manifest_last_request_status(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->last_request_status : LAUNCH64_REQUEST_EMPTY;
}

u32 launch64_request_id_by_index(u32 index)
{
    launch64_init();
    if (index >= g_request_log_count)
    {
        return LAUNCH64_INVALID_REQUEST;
    }

    return g_requests[index].request_id;
}

u32 launch64_request_operation(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->operation : LAUNCH64_OPERATION_NONE;
}

u32 launch64_request_status(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->status : LAUNCH64_REQUEST_EMPTY;
}

u32 launch64_request_manifest(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->manifest_index : LAUNCH64_INVALID_MANIFEST;
}

u32 launch64_request_requester(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->requester_principal : 0u;
}

u32 launch64_request_denial(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->denial_reason : LAUNCH64_DENY_INVALID_MANIFEST;
}

u32 launch64_request_observed_capabilities(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->observed_capability_count : 0u;
}

u32 launch64_request_revoked_capabilities(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->revoked_capability_count : 0u;
}

u32 launch64_request_runtime_generation(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_generation : 0u;
}

u32 launch64_request_runtime_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_token : 0u;
}

u32 launch64_request_runtime_image_generation(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_generation : 0u;
}

u32 launch64_request_runtime_image_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_token : 0u;
}

u32 launch64_request_runtime_image_base(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_base : 0u;
}

u32 launch64_request_runtime_image_entry(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_entry : 0u;
}

u32 launch64_request_runtime_image_mapped_bytes(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_mapped_bytes : 0u;
}

u32 launch64_request_runtime_image_rights(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_rights : 0u;
}

u32 launch64_request_runtime_image_plan_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_plan_token : 0u;
}

u32 launch64_request_runtime_image_map_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_map_token : 0u;
}

u32 launch64_request_runtime_image_page_count(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_page_count : 0u;
}

u32 launch64_request_runtime_image_pml4_index(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_pml4_index : 0u;
}

u32 launch64_request_runtime_image_pdpt_index(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_pdpt_index : 0u;
}

u32 launch64_request_runtime_image_pd_index(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_pd_index : 0u;
}

u32 launch64_request_runtime_entry_transfer_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_entry_transfer_token : 0u;
}

u32 launch64_request_runtime_image_install_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_install_token : 0u;
}

u32 launch64_request_runtime_image_source_checksum(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_source_checksum : 0u;
}

u32 launch64_request_runtime_image_entry_probe(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_entry_probe : 0u;
}

u32 launch64_request_runtime_image_map_installed(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_map_installed : 0u;
}

u32 launch64_request_runtime_image_protection_flags(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_protection_flags : 0u;
}

u32 launch64_request_runtime_image_protection_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_image_protection_token : 0u;
}

u32 launch64_request_runtime_user_entry_state(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_user_entry_state : 0u;
}

u32 launch64_request_runtime_user_entry_token(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_user_entry_token : 0u;
}

u32 launch64_request_runtime_user_entry_rip(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_user_entry_rip : 0u;
}

u32 launch64_request_runtime_user_entry_rsp(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_user_entry_rsp : 0u;
}

u32 launch64_request_runtime_user_entry_selectors(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_user_entry_selectors : 0u;
}

u32 launch64_request_runtime_user_entry_rflags(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_user_entry_rflags : 0u;
}

u32 launch64_request_runtime_user_entry_denial(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_user_entry_denial : LAUNCH64_USER_ENTRY_DENY_DESCRIPTOR_STATE;
}

u32 launch64_request_runtime_payload_slot(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_payload_slot : 0u;
}

u32 launch64_request_runtime_payload_kind(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_payload_kind : 0u;
}

u32 launch64_request_runtime_payload_offset(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_payload_offset : 0u;
}

u32 launch64_request_runtime_payload_size(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_payload_size : 0u;
}

u32 launch64_request_runtime_payload_checksum(u32 request_id)
{
    const struct launch64_request_record *request;

    launch64_init();
    request = launch64_find_request(request_id);
    return (request != 0) ? request->runtime_payload_checksum : 0u;
}

const char *launch64_manifest_package_name(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->package_name : "unknown";
}

const char *launch64_manifest_process_name(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->process_name : "unknown";
}

const char *launch64_manifest_profile_name(u32 manifest_index)
{
    const struct launch64_manifest_record *manifest = launch64_manifest(manifest_index);

    return (manifest != 0) ? manifest->profile_name : "unknown";
}
