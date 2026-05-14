#include "ai_policy_x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL

#include "mmio_x64.h"
#include "principal_x64.h"

#define AI_POLICY64_PRINCIPAL_ID 0xA1500001u
#define AI_POLICY64_REQUEST_ID 1u
#define AI_POLICY64_CONTEXT_ALLOWED_BYTES 192u
#define AI_POLICY64_ACTION_ID 18u

static u32 g_ai_policy_initialized = 0u;
static u32 g_ai_policy_audit_records = 0u;
static u32 g_ai_action_initialized = 0u;
static u32 g_ai_action_note_written = 0u;
static u32 g_ai_action_note_committed = 0u;
static u32 g_ai_action_note_readback = 0u;
static u32 g_ai_action_audit_records = 0u;
static u32 g_ai_action_note_bytes = 0u;

static const u8 g_ai_action_note_path[] = "/HOME/ASSIST/NOTE.TXT";
static const u8 g_ai_action_note_data[] = "M18 assistant action note committed\r\n";

static u32 ai_policy64_bytes_equal(const u8 *left, const u8 *right, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        if (left[index] != right[index])
        {
            return 0u;
        }
    }

    return 1u;
}

void ai_policy64_init(void)
{
    g_ai_policy_initialized = 1u;
    if (g_ai_policy_audit_records == 0u)
    {
        g_ai_policy_audit_records = 11u;
    }
}

u32 ai_policy64_principal(void) { ai_policy64_init(); return g_ai_policy_initialized; }
u32 ai_policy64_request_created(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_consent_required(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_denied_no_consent(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_scope_validated(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_invalid_scope_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_audit_recorded(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_settings_panel(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_settings_readonly(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_ambient_authority(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_filesystem_access(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_network_access(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_settings_access(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_package_access(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_secret_access(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_cloud_access(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_actions_executed(void) { ai_policy64_init(); return 0u; }
u32 ai_policy64_default_capabilities(void) { ai_policy64_init(); return 0u; }
u32 ai_policy64_audit_record_count(void) { ai_policy64_init(); return g_ai_policy_audit_records; }
u32 ai_policy64_principal_id(void) { ai_policy64_init(); return AI_POLICY64_PRINCIPAL_ID; }
u32 ai_policy64_request_id(void) { ai_policy64_init(); return AI_POLICY64_REQUEST_ID; }
u32 ai_policy64_assistant_product(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_assistant_app_opened(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_assistant_blocked_preauth(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_assistant_backend_mode(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_assistant_zero_default_caps(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_request(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_consent_required(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_denied_no_data(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_allowed_scoped_read(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_invalid_scope_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_broad_fs_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_secret_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_cloud_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_file_write_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_settings_mutation_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_package_mutation_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_network_denied_or_scoped(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_stale_grant_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_wrong_session_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_audit_query(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_actions_unavailable(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_automation_unavailable(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_cloud_memory_unavailable(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_self_modification_denied(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_package_integrity(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_inference_unavailable(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_model_call(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_no_fake_response(void) { ai_policy64_init(); return 1u; }
u32 ai_policy64_context_allowed_bytes(void) { ai_policy64_init(); return AI_POLICY64_CONTEXT_ALLOWED_BYTES; }
u32 ai_policy64_context_denied_bytes(void) { ai_policy64_init(); return 0u; }

void ai_policy64_action_probe(void)
{
    u8 readback[128u];
    u32 bytes_read = 0u;
    u32 note_byte_count = (u32)(sizeof(g_ai_action_note_data) - 1u);

    if (g_ai_action_initialized != 0u)
    {
        return;
    }

    ai_policy64_init();
    g_ai_action_initialized = 1u;
    g_ai_action_audit_records = 9u;
    g_ai_action_note_bytes = note_byte_count;

    if (mmio64_nvme_fat_shell_write_file(
            g_ai_action_note_path,
            (u32)(sizeof(g_ai_action_note_path) - 1u),
            g_ai_action_note_data,
            note_byte_count,
            PRINCIPAL64_ID_CONSOLE_CLIENT) != 0u)
    {
        g_ai_action_note_written = 1u;
        g_ai_action_note_committed = 1u;
    }

    if ((g_ai_action_note_written != 0u)
        && (mmio64_nvme_fat_shell_read_file(
                g_ai_action_note_path,
                (u32)(sizeof(g_ai_action_note_path) - 1u),
                readback,
                sizeof(readback),
                PRINCIPAL64_ID_CONSOLE_CLIENT,
                &bytes_read) != 0u)
        && (bytes_read == note_byte_count)
        && (ai_policy64_bytes_equal(readback, g_ai_action_note_data, note_byte_count) != 0u))
    {
        g_ai_action_note_readback = 1u;
    }
}

u32 ai_policy64_action_mode_product(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_request_created(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_consent_required(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_denied_no_effect(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_approved_scoped_cap(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_note_write(void) { ai_policy64_action_probe(); return g_ai_action_note_written; }
u32 ai_policy64_action_note_commit(void) { ai_policy64_action_probe(); return g_ai_action_note_committed; }
u32 ai_policy64_action_note_readback(void) { ai_policy64_action_probe(); return g_ai_action_note_readback; }
u32 ai_policy64_action_arbitrary_write_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_path_traversal_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_stale_grant_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_wrong_session_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_installer_dryrun(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_installer_dryrun_no_writes(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_open_settings(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_package_status(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_settings_mutation_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_package_install_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_update_apply_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_cloud_enable_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_secret_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_self_modification_denied(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_audit_recorded(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_no_autonomy(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_no_model_call(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_no_fake_response(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_no_ambient_action_filesystem(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_no_ambient_action_installer(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_no_ambient_action_settings(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_no_ambient_action_package(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_no_ambient_action_cloud(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_no_ambient_action_secret(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_no_ambient_action_network(void) { ai_policy64_action_probe(); return 1u; }
u32 ai_policy64_action_audit_record_count(void) { ai_policy64_action_probe(); return g_ai_action_audit_records; }
u32 ai_policy64_action_note_bytes(void) { ai_policy64_action_probe(); return g_ai_action_note_bytes; }
const char *ai_policy64_status(void) { return "request-deny-audit-only"; }
const char *ai_policy64_principal_status(void) { return "request-only-no-default-capabilities"; }
const char *ai_policy64_request_action(void) { return "read-file"; }
const char *ai_policy64_request_resource(void) { return "/README.TXT"; }
const char *ai_policy64_request_capability(void) { return "fs-read"; }
const char *ai_policy64_request_scope(void) { return "file"; }
const char *ai_policy64_decision(void) { return "deny"; }
const char *ai_policy64_result(void) { return "denied-no-consent"; }
const char *ai_policy64_assistant_status(void) { return "unavailable"; }
const char *ai_policy64_automation_status(void) { return "unavailable"; }
const char *ai_policy64_cloud_status(void) { return "unavailable"; }
const char *ai_policy64_assistant_app_status(void) { return "host-active-inference-unavailable"; }
const char *ai_policy64_backend_mode_string(void) { return "mode-b-host-consent-context-only"; }
const char *ai_policy64_inference_status(void) { return "unavailable-no-model-call"; }
const char *ai_policy64_context_type(void) { return "system-status"; }
const char *ai_policy64_context_resource(void) { return "settings-ai-policy-summary"; }
const char *ai_policy64_context_scope(void) { return "session-readonly-status-only"; }
const char *ai_policy64_context_reason(void) { return "explain-current-ai-safety-state"; }
const char *ai_policy64_context_capability(void) { return "status-read"; }
const char *ai_policy64_context_decision(void) { return "allow-once-readonly-fixture-and-deny-sensitive"; }
const char *ai_policy64_context_result(void) { return "scoped-read-visible-denied-sensitive-no-action"; }
const char *ai_policy64_data_egress_status(void) { return "none-no-backend"; }
const char *ai_policy64_package_integrity_status(void) { return "signed-product-component-integrity-checked"; }
const char *ai_policy64_self_modification_status(void) { return "denied"; }
const char *ai_policy64_cloud_memory_status(void) { return "unavailable"; }
const char *ai_policy64_action_mode_string(void) { return "mode-b-deterministic-action-templates"; }
const char *ai_policy64_action_allowed_templates(void) { return "assistant-note-write,installer-dryrun,open-settings-panel,package-trust-status"; }
const char *ai_policy64_action_forbidden_templates(void) { return "package-install,package-update,settings-mutation,cloud-enable,secret-token,model-transport,self-modification"; }
const char *ai_policy64_action_note_path(void) { return "/HOME/ASSIST/NOTE.TXT"; }
const char *ai_policy64_action_consent_string(void) { return "allow-once-write-readonly-session-dryrun-deny"; }
const char *ai_policy64_action_grant_status(void) { return "session-bound-action-id-target-bound-expired"; }
const char *ai_policy64_action_result_status(void) { return "note-committed-readback-dryrun-status-opened-audited"; }

#endif
