#include "installer_ux_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

#include "capability_x64.h"
#include "mmio_x64.h"
#include "principal_x64.h"

static u32 g_installer_ux_initialized = 0u;
static u32 g_installer_commit_attempted = 0u;
static u32 g_installer_commit_runtime_fat_target = 0u;
static u32 g_installer_commit_confirmation_token = 0u;
static u32 g_installer_commit_scoped_write_cap = 0u;
static u32 g_installer_commit_bad_token_denied = 0u;
static u32 g_installer_commit_wrong_owner_denied = 0u;
static u32 g_installer_commit_write = 0u;
static u32 g_installer_commit_readback = 0u;
static u32 g_installer_commit_bytes = 0u;
static u32 g_installer_commit_checksum = 0u;
static u32 g_installer_commit_audit_count = 0u;
static u32 g_installer_commit_no_ambient_authority = 0u;
static u32 g_installer_commit_unavailable = 1u;
static u32 g_installer_commit_error = 0u;
static u32 g_installer_target_attempted = 0u;
static u32 g_installer_target_confirmation_token = 0u;
static u32 g_installer_target_classified = 0u;
static u32 g_installer_target_boot_partition = 0u;
static u32 g_installer_target_root_partition = 0u;
static u32 g_installer_target_boot_start = 0u;
static u32 g_installer_target_root_start = 0u;
static u32 g_installer_target_forbidden_denied = 0u;
static u32 g_installer_target_bad_token_denied = 0u;
static u32 g_installer_target_wrong_target_denied = 0u;
static u32 g_installer_target_wrong_owner_denied = 0u;
static u32 g_installer_target_m5_write_cap = 0u;
static u32 g_installer_target_write = 0u;
static u32 g_installer_target_readback = 0u;
static u32 g_installer_target_bytes = 0u;
static u32 g_installer_target_checksum = 0u;
static u32 g_installer_target_write_denied = 0u;
static u32 g_installer_target_format_denied = 0u;
static u32 g_installer_target_boot_entry_denied = 0u;
static u32 g_installer_target_no_ambient_authority = 0u;
static u32 g_installer_target_unavailable = 1u;
static u32 g_installer_target_error = 0u;

static const char g_installer_commit_token[] =
    "INSTALL-LIMITLESSOS-INTERNAL:NVME-FAT-MARKER";
static const char g_installer_target_token[] =
    "INSTALL-LIMITLESSOS-M5:5/6";

static u32 installer_ux64_checksum_bytes(const u8 *bytes, u32 byte_count)
{
    u32 digest = 2166136261u;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        digest ^= bytes[index];
        digest *= 16777619u;
    }

    return (digest != 0u) ? digest : 1u;
}

static u32 installer_ux64_token_matches(const char *token)
{
    u32 index = 0u;

    if (token == (const char *)0)
    {
        return 0u;
    }

    while ((g_installer_commit_token[index] != '\0') || (token[index] != '\0'))
    {
        if (g_installer_commit_token[index] != token[index])
        {
            return 0u;
        }
        ++index;
        if (index > 96u)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 installer_ux64_target_token_matches(const char *token)
{
    u32 index = 0u;

    if (token == (const char *)0)
    {
        return 0u;
    }

    while ((g_installer_target_token[index] != '\0') || (token[index] != '\0'))
    {
        if (g_installer_target_token[index] != token[index])
        {
            return 0u;
        }
        ++index;
        if (index > 64u)
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 installer_ux64_runtime_fat_target_ready(void)
{
    return ((mmio64_nvme_gpt_signature() != 0u)
        && (mmio64_nvme_gpt_vbr() != 0u)
        && (mmio64_nvme_fat_bpb() != 0u)
        && (mmio64_nvme_fat_located() != 0u)
        && (mmio64_nvme_fat_write_gate() != 0u)
        && (mmio64_nvme_fat_unavailable() == 0u)) ? 1u : 0u;
}

static u32 installer_ux64_scoped_write_cap_ready(void)
{
    return ((mmio64_nvme_rw_delegated() != 0u)
        && (mmio64_nvme_rw_capability() != CAPABILITY64_INVALID_HANDLE)
        && (mmio64_nvme_rw_write_authority() != 0u)
        && (mmio64_nvme_rw_commit_authority() != 0u)
        && (mmio64_nvme_rw_unavailable() == 0u)) ? 1u : 0u;
}

static u32 installer_ux64_m5_target_classified(void)
{
    return ((mmio64_nvme_gpt_signature() != 0u)
        && (mmio64_nvme_gpt_unavailable() == 0u)
        && (mmio64_nvme_gpt_m5_safe_targets() == 2u)
        && (mmio64_nvme_gpt_m5_forbidden_targets() != 0u)
        && (mmio64_nvme_gpt_m5_unknown_targets() == 0u)
        && (mmio64_nvme_gpt_m5_boot_partition() != 0u)
        && (mmio64_nvme_gpt_m5_root_partition() != 0u)
        && (mmio64_nvme_gpt_m5_boot_start() != 0u)
        && (mmio64_nvme_gpt_m5_root_start() != 0u)) ? 1u : 0u;
}

static u32 installer_ux64_m5_write_cap_ready(void)
{
    return ((installer_ux64_scoped_write_cap_ready() != 0u)
        && (installer_ux64_m5_target_classified() != 0u)) ? 1u : 0u;
}

static u32 installer_ux64_target_attempt(
    const char *confirmation_token,
    u32 boot_partition,
    u32 root_partition,
    u32 owner_id,
    u32 record_success)
{
    static const u8 payload[] = "LimitlessOS M5 boot target marker\n";
    u32 bytes_written = 0u;
    u32 checksum = 0u;

    if (installer_ux64_target_token_matches(confirmation_token) == 0u)
    {
        return 0u;
    }

    if (record_success != 0u)
    {
        g_installer_target_confirmation_token = 1u;
    }

    if (owner_id != PRINCIPAL64_ID_CONSOLE_CLIENT)
    {
        return 0u;
    }

    if (installer_ux64_m5_target_classified() == 0u)
    {
        if (record_success != 0u)
        {
            g_installer_target_unavailable = 1u;
            g_installer_target_error = 1u;
        }
        return 0u;
    }

    if ((boot_partition != mmio64_nvme_gpt_m5_boot_partition())
        || (root_partition != mmio64_nvme_gpt_m5_root_partition()))
    {
        return 0u;
    }

    if (record_success != 0u)
    {
        g_installer_target_classified = 1u;
        g_installer_target_boot_partition = boot_partition;
        g_installer_target_root_partition = root_partition;
        g_installer_target_boot_start = mmio64_nvme_gpt_m5_boot_start();
        g_installer_target_root_start = mmio64_nvme_gpt_m5_root_start();
        g_installer_target_forbidden_denied =
            mmio64_nvme_gpt_m5_forbidden_denied();
        g_installer_target_m5_write_cap =
            installer_ux64_m5_write_cap_ready();
        g_installer_target_format_denied = 1u;
        g_installer_target_boot_entry_denied = 1u;
    }

    if (installer_ux64_m5_write_cap_ready() == 0u)
    {
        if (record_success != 0u)
        {
            g_installer_target_write = 0u;
            g_installer_target_readback = 0u;
            g_installer_target_bytes = 0u;
            g_installer_target_checksum = 0u;
            g_installer_target_write_denied = 1u;
            g_installer_target_unavailable = 0u;
            g_installer_target_error = 0u;
        }
        return 0u;
    }

    if (record_success == 0u)
    {
        return 1u;
    }

    if (mmio64_nvme_m5_write_boot_marker(
            mmio64_nvme_rw_capability(),
            owner_id,
            payload,
            sizeof(payload) - 1u,
            &bytes_written,
            &checksum) == 0u)
    {
        g_installer_target_write = 0u;
        g_installer_target_readback = 0u;
        g_installer_target_bytes = 0u;
        g_installer_target_checksum = 0u;
        g_installer_target_write_denied = 1u;
        g_installer_target_unavailable = 0u;
        g_installer_target_error = 2u;
        return 0u;
    }

    g_installer_target_write = 1u;
    g_installer_target_readback = 1u;
    g_installer_target_bytes = bytes_written;
    g_installer_target_checksum = checksum;
    g_installer_target_write_denied =
        ((g_installer_target_bad_token_denied != 0u)
            && (g_installer_target_wrong_target_denied != 0u)
            && (g_installer_target_wrong_owner_denied != 0u)) ? 1u : 0u;
    g_installer_target_unavailable = 0u;
    g_installer_target_error = 0u;
    return 1u;
}

static u32 installer_ux64_commit_attempt(
    const char *confirmation_token,
    u32 owner_id,
    u32 record_success)
{
    static const u8 path[] = "/INSTALL.TXT";
    static const u8 payload[] = "LimitlessOS installer scoped commit proof\n";
    u8 verify_buffer[64];
    u32 bytes_read = 0u;
    u32 index;

    if (installer_ux64_token_matches(confirmation_token) == 0u)
    {
        return 0u;
    }

    if (record_success != 0u)
    {
        g_installer_commit_confirmation_token = 1u;
    }

    if (installer_ux64_runtime_fat_target_ready() == 0u)
    {
        if (record_success != 0u)
        {
            g_installer_commit_unavailable = 1u;
            g_installer_commit_error = 1u;
        }
        return 0u;
    }
    if (record_success != 0u)
    {
        g_installer_commit_runtime_fat_target = 1u;
    }

    if (installer_ux64_scoped_write_cap_ready() == 0u)
    {
        if (record_success != 0u)
        {
            g_installer_commit_unavailable = 1u;
            g_installer_commit_error = 2u;
        }
        return 0u;
    }
    if (record_success != 0u)
    {
        g_installer_commit_scoped_write_cap = 1u;
    }

    if (mmio64_nvme_fat_shell_write_file(
            path,
            sizeof(path) - 1u,
            payload,
            sizeof(payload) - 1u,
            owner_id) == 0u)
    {
        if ((record_success == 0u) && (owner_id != PRINCIPAL64_ID_CONSOLE_CLIENT))
        {
            return 0u;
        }
        if (record_success != 0u)
        {
            g_installer_commit_error = 3u;
        }
        return 0u;
    }

    if (record_success == 0u)
    {
        return 1u;
    }

    for (index = 0u; index < sizeof(verify_buffer); ++index)
    {
        verify_buffer[index] = 0u;
    }

    if (mmio64_nvme_fat_shell_read_file(
            path,
            sizeof(path) - 1u,
            verify_buffer,
            sizeof(verify_buffer),
            owner_id,
            &bytes_read) == 0u)
    {
        g_installer_commit_error = 4u;
        return 0u;
    }

    if (bytes_read != (sizeof(payload) - 1u))
    {
        g_installer_commit_error = 5u;
        return 0u;
    }

    for (index = 0u; index < bytes_read; ++index)
    {
        if (verify_buffer[index] != payload[index])
        {
            g_installer_commit_error = 6u;
            return 0u;
        }
    }

    g_installer_commit_write = 1u;
    g_installer_commit_readback = 1u;
    g_installer_commit_bytes = bytes_read;
    g_installer_commit_checksum =
        installer_ux64_checksum_bytes(verify_buffer, bytes_read);
    ++g_installer_commit_audit_count;
    g_installer_commit_unavailable = 0u;
    g_installer_commit_error = 0u;
    return 1u;
}

void installer_ux64_init(void)
{
    g_installer_ux_initialized = 1u;
}

u32 installer_ux64_product(void) { installer_ux64_init(); return g_installer_ux_initialized; }
u32 installer_ux64_welcome(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_beginner_mode(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_advanced_mode(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_hardware_summary(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_recommendation(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_component_selection(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_unavailable_components_labeled(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_account_page(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_personal_unavailable(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_enterprise_unavailable(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_cloud_page(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_cloud_sync_unavailable(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_ai_page(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_ai_setup_unavailable(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_plan_generated(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_dryrun_no_writes(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_forbidden_target_denied(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_write_action_denied(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_format_action_denied(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_boot_entry_denied(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_package_install_denied(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_cloud_enable_denied(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_ai_enable_denied(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_no_ambient_installer(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_no_ambient_storage(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_no_ambient_firmware(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_no_ambient_package(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_no_ambient_identity_cloud_secret(void) { installer_ux64_init(); return 1u; }
u32 installer_ux64_writes_planned(void) { installer_ux64_init(); return 0u; }
u32 installer_ux64_formats_planned(void) { installer_ux64_init(); return 0u; }
u32 installer_ux64_boot_entries_planned(void) { installer_ux64_init(); return 0u; }
u32 installer_ux64_package_ops_planned(void) { installer_ux64_init(); return 0u; }
u32 installer_ux64_real_install_approved(void) { installer_ux64_init(); return 0u; }
const char *installer_ux64_mode(void) { return "planning-dry-run-only"; }
const char *installer_ux64_selected_profile(void) { return "general-use"; }
const char *installer_ux64_recommendation_text(void) { return "general-use-safe-profile"; }
const char *installer_ux64_component_status(void) { return "product-components-selected-unavailable-labeled"; }
const char *installer_ux64_account_status(void) { return "local-only-personal-enterprise-unavailable"; }
const char *installer_ux64_cloud_status(void) { return "cloud-sync-unavailable"; }
const char *installer_ux64_ai_status(void) { return "ai-assisted-setup-unavailable"; }
const char *installer_ux64_plan_status(void) { return "generated-zero-write-plan"; }
const char *installer_ux64_dryrun_status(void) { return "validated-no-writes"; }

u32 installer_ux64_commit_probe(void)
{
    if (g_installer_commit_attempted != 0u)
    {
        return g_installer_commit_write;
    }

    installer_ux64_init();
    g_installer_commit_attempted = 1u;
    g_installer_commit_runtime_fat_target =
        installer_ux64_runtime_fat_target_ready();
    g_installer_commit_scoped_write_cap =
        installer_ux64_scoped_write_cap_ready();

    if (installer_ux64_commit_attempt(
            "INSTALL-LIMITLESSOS-INTERNAL:DENIED",
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            0u) == 0u)
    {
        g_installer_commit_bad_token_denied = 1u;
    }

    if (installer_ux64_commit_attempt(
            g_installer_commit_token,
            PRINCIPAL64_ID_POLICY_CLIENT,
            0u) == 0u)
    {
        g_installer_commit_wrong_owner_denied = 1u;
    }

    if (installer_ux64_commit_attempt(
            g_installer_commit_token,
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            1u) != 0u)
    {
        g_installer_commit_no_ambient_authority =
            ((g_installer_commit_bad_token_denied != 0u)
                && (g_installer_commit_wrong_owner_denied != 0u)
                && (g_installer_commit_scoped_write_cap != 0u)) ? 1u : 0u;
    }

    return g_installer_commit_write;
}

u32 installer_ux64_commit_attempted(void) { return g_installer_commit_attempted; }
u32 installer_ux64_commit_runtime_fat_target(void) { return g_installer_commit_runtime_fat_target; }
u32 installer_ux64_commit_confirmation_token(void) { return g_installer_commit_confirmation_token; }
u32 installer_ux64_commit_scoped_write_cap(void) { return g_installer_commit_scoped_write_cap; }
u32 installer_ux64_commit_bad_token_denied(void) { return g_installer_commit_bad_token_denied; }
u32 installer_ux64_commit_wrong_owner_denied(void) { return g_installer_commit_wrong_owner_denied; }
u32 installer_ux64_commit_write(void) { return g_installer_commit_write; }
u32 installer_ux64_commit_readback(void) { return g_installer_commit_readback; }
u32 installer_ux64_commit_bytes(void) { return g_installer_commit_bytes; }
u32 installer_ux64_commit_checksum(void) { return g_installer_commit_checksum; }
u32 installer_ux64_commit_audit_count(void) { return g_installer_commit_audit_count; }
u32 installer_ux64_commit_no_ambient_authority(void) { return g_installer_commit_no_ambient_authority; }
u32 installer_ux64_commit_unavailable(void) { return g_installer_commit_unavailable; }
u32 installer_ux64_commit_error(void) { return g_installer_commit_error; }
const char *installer_ux64_commit_mode(void) { return "nvme-fat-marker-only"; }

u32 installer_ux64_target_probe(void)
{
    u32 boot_partition;
    u32 root_partition;

    if (g_installer_target_attempted != 0u)
    {
        return g_installer_target_write;
    }

    installer_ux64_init();
    g_installer_target_attempted = 1u;
    boot_partition = mmio64_nvme_gpt_m5_boot_partition();
    root_partition = mmio64_nvme_gpt_m5_root_partition();

    if (installer_ux64_target_attempt(
            "INSTALL-LIMITLESSOS-M5:DENIED",
            boot_partition,
            root_partition,
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            0u) == 0u)
    {
        g_installer_target_bad_token_denied = 1u;
    }

    if (installer_ux64_target_attempt(
            g_installer_target_token,
            boot_partition + 1u,
            root_partition,
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            0u) == 0u)
    {
        g_installer_target_wrong_target_denied = 1u;
    }

    if (installer_ux64_target_attempt(
            g_installer_target_token,
            boot_partition,
            root_partition,
            PRINCIPAL64_ID_POLICY_CLIENT,
            0u) == 0u)
    {
        g_installer_target_wrong_owner_denied = 1u;
    }

    (void)installer_ux64_target_attempt(
        g_installer_target_token,
        boot_partition,
        root_partition,
        PRINCIPAL64_ID_CONSOLE_CLIENT,
        1u);

    g_installer_target_no_ambient_authority =
        ((g_installer_target_bad_token_denied != 0u)
            && (g_installer_target_wrong_target_denied != 0u)
            && (g_installer_target_wrong_owner_denied != 0u)
            && (g_installer_target_write_denied != 0u)
            && (g_installer_target_m5_write_cap != 0u)
            && (g_installer_target_write != 0u)
            && (g_installer_target_readback != 0u)
            && (g_installer_target_format_denied != 0u)
            && (g_installer_target_boot_entry_denied != 0u)) ? 1u : 0u;

    return g_installer_target_write;
}

u32 installer_ux64_target_attempted(void) { return g_installer_target_attempted; }
u32 installer_ux64_target_confirmation_token(void) { return g_installer_target_confirmation_token; }
u32 installer_ux64_target_classified(void) { return g_installer_target_classified; }
u32 installer_ux64_target_boot_partition(void) { return g_installer_target_boot_partition; }
u32 installer_ux64_target_root_partition(void) { return g_installer_target_root_partition; }
u32 installer_ux64_target_boot_start(void) { return g_installer_target_boot_start; }
u32 installer_ux64_target_root_start(void) { return g_installer_target_root_start; }
u32 installer_ux64_target_forbidden_denied(void) { return g_installer_target_forbidden_denied; }
u32 installer_ux64_target_bad_token_denied(void) { return g_installer_target_bad_token_denied; }
u32 installer_ux64_target_wrong_target_denied(void) { return g_installer_target_wrong_target_denied; }
u32 installer_ux64_target_wrong_owner_denied(void) { return g_installer_target_wrong_owner_denied; }
u32 installer_ux64_target_m5_write_cap(void) { return g_installer_target_m5_write_cap; }
u32 installer_ux64_target_write(void) { return g_installer_target_write; }
u32 installer_ux64_target_readback(void) { return g_installer_target_readback; }
u32 installer_ux64_target_bytes(void) { return g_installer_target_bytes; }
u32 installer_ux64_target_checksum(void) { return g_installer_target_checksum; }
u32 installer_ux64_target_write_denied(void) { return g_installer_target_write_denied; }
u32 installer_ux64_target_format_denied(void) { return g_installer_target_format_denied; }
u32 installer_ux64_target_boot_entry_denied(void) { return g_installer_target_boot_entry_denied; }
u32 installer_ux64_target_no_ambient_authority(void) { return g_installer_target_no_ambient_authority; }
u32 installer_ux64_target_unavailable(void) { return g_installer_target_unavailable; }
u32 installer_ux64_target_error(void) { return g_installer_target_error; }
const char *installer_ux64_target_mode(void) { return "m5-boot-marker-write-only"; }

#endif
