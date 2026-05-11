#include "syscall_x64.h"

#include "block_x64.h"
#include "capability_x64.h"
#include "console_x64.h"
#include "descriptors_x64.h"
#include "display_x64.h"
#include "fs_x64.h"
#include "input_x64.h"
#include "interrupts_x64.h"
#include "launch_x64.h"
#include "mmio_x64.h"
#include "pci_x64.h"
#include "pit.h"
#include "principal_x64.h"
#include "process_x64.h"
#include "services_x64.h"
#include "shell_x64.h"
#include "x64.h"
#include "xhci_x64.h"

enum
{
    EFER_MSR = 0xC0000080u,
    STAR_MSR = 0xC0000081u,
    LSTAR_MSR = 0xC0000082u,
    FMASK_MSR = 0xC0000084u,
    EFER_SCE = 0x00000001u,
    RFLAGS_INTERRUPT_FLAG = 0x00000200u
};

extern void syscall64_native_entry(void);

static const struct boot_info *g_boot_info = 0;
static volatile u32 g_native_syscall_count = 0u;
static volatile u64 g_native_last_syscall_code = 0u;
static volatile u64 g_native_star_value = 0u;
static volatile u32 g_native_star_ready = 0u;
static u32 g_input_diag_scancodes = 0xFFFFFFFFu;
static u32 g_input_diag_pending = 0xFFFFFFFFu;
static u32 g_input_diag_last_scancode = 0xFFFFFFFFu;

static void syscall64_refresh_input_diagnostics_if_changed(void)
{
    u32 scancodes = input64_keyboard_scancode_count();
    u32 pending = input64_keyboard_pending_count();
    u32 last_scancode = input64_keyboard_last_scancode();

    if ((scancodes == g_input_diag_scancodes)
        && (pending == g_input_diag_pending)
        && (last_scancode == g_input_diag_last_scancode))
    {
        return;
    }

    g_input_diag_scancodes = scancodes;
    g_input_diag_pending = pending;
    g_input_diag_last_scancode = last_scancode;
    (void)display64_write_boot_diagnostics(
        xhci64_found(),
        xhci64_legacy_handoff(),
        xhci64_usb2_ports(),
        xhci64_hid_device(),
        xhci64_error(),
        input64_ps2_present(),
        input64_ps2_enabled(),
        input64_ps2_scanning_enabled(),
        input64_ps2_status_snapshot(),
        input64_ps2_config_byte(),
        input64_ps2_device_ack(),
        scancodes,
        pending,
        last_scancode,
        pci64_lpss_i2c_hid_found());
}

void syscall64_init(const struct boot_info *boot_info)
{
    g_boot_info = boot_info;
    g_native_syscall_count = 0u;
    g_native_last_syscall_code = 0u;
    g_native_star_value = 0u;
    g_native_star_ready = 0u;
    principal64_init();
    services64_init();
    launch64_init();
    process64_init();
    capability64_init();
    mmio64_init();
    pci64_init(boot_info);
    block64_init();
    console64_init();
    input64_init();
    input64_set_keyboard_scancode_set(input64_ps2_recommended_scancode_set());
    fs64_init();
    display64_init(boot_info);
    input64_set_mouse_bounds(display64_width(), display64_height());
}

static u32 syscall64_pack_low32(u64 packed)
{
    return (u32)(packed & 0xFFFFFFFFull);
}

static u32 syscall64_pack_high32(u64 packed)
{
    return (u32)((packed >> 32) & 0xFFFFFFFFull);
}

static u32 syscall64_pack_low16(u64 packed)
{
    return (u32)(packed & 0xFFFFull);
}

static u32 syscall64_pack_mid16(u64 packed)
{
    return (u32)((packed >> 16) & 0xFFFFull);
}

void syscall64_native_init(void)
{
    u64 efer = rdmsr64(EFER_MSR);
    u64 star = descriptors64_syscall_star_plan();

    efer |= EFER_SCE;
    wrmsr64(EFER_MSR, efer);
    wrmsr64(STAR_MSR, star);
    wrmsr64(LSTAR_MSR, (u64)syscall64_native_entry);
    wrmsr64(FMASK_MSR, (u64)RFLAGS_INTERRUPT_FLAG);
    g_native_star_value = rdmsr64(STAR_MSR);
    g_native_star_ready = (g_native_star_value == star) ? 1u : 0u;
}

u64 syscall64_dispatch(u64 number, u64 arg0, u64 arg1, u64 arg2)
{
    switch (number)
    {
        case X64_SYSCALL_GET_UPTIME_TICKS:
            return (u64)pit_get_ticks();

        case X64_SYSCALL_GET_BOOT_FLAGS:
            return (g_boot_info != 0) ? (u64)g_boot_info->bootstrap_flags : 0ull;

        case X64_SYSCALL_GET_PAGE_TABLE_ROOT:
            return (g_boot_info != 0) ? (u64)g_boot_info->page_table_root : 0ull;

        case X64_SYSCALL_GET_IDENTITY_MAP_MIB:
            if (g_boot_info == 0)
            {
                return 0ull;
            }
            return (u64)(g_boot_info->identity_map_bytes / (1024u * 1024u));

        case X64_SYSCALL_GET_IRQ_COUNT:
            return (u64)interrupts64_irq_count();

        case X64_SYSCALL_GET_PROBE_COUNT:
            return (u64)interrupts64_probe_count();

        case X64_SYSCALL_GET_SYSCALL_COUNT:
            return (u64)interrupts64_syscall_count();

        case X64_SYSCALL_GET_KERNEL_LOAD_ADDRESS:
            return (g_boot_info != 0) ? (u64)g_boot_info->kernel_load_address : 0ull;

        case X64_SYSCALL_GET_KERNEL_SECTOR_COUNT:
            return (g_boot_info != 0) ? (u64)g_boot_info->kernel_sector_count : 0ull;

        case X64_SYSCALL_GET_ARCH_BITS:
            return (g_boot_info != 0) ? (u64)g_boot_info->architecture_bits : 0ull;

        case X64_SYSCALL_GET_CONVENTIONAL_MEMORY_KIB:
            return (g_boot_info != 0) ? (u64)g_boot_info->conventional_memory_kb : 0ull;

        case X64_SYSCALL_GET_EXTENDED_MEMORY_KIB:
            return (g_boot_info != 0) ? (u64)g_boot_info->extended_memory_kb : 0ull;

        case X64_SYSCALL_GET_NATIVE_SYSCALL_COUNT:
            return (u64)g_native_syscall_count;

        case X64_SYSCALL_GET_NATIVE_LAST_CODE:
            return g_native_last_syscall_code;

        case X64_SYSCALL_GET_EXCEPTION_COUNT:
            return (u64)interrupts64_exception_count();

        case X64_SYSCALL_GET_BREAKPOINT_COUNT:
            return (u64)interrupts64_breakpoint_count();

        case X64_SYSCALL_GET_LAST_EXCEPTION_VECTOR:
            return interrupts64_last_exception_vector();

        case X64_SYSCALL_GET_LAST_EXCEPTION_ERROR:
            return interrupts64_last_exception_error();

        case X64_SYSCALL_GET_LAST_EXCEPTION_RIP:
            return interrupts64_last_exception_rip();

        case X64_SYSCALL_GET_LAST_EXCEPTION_CR2:
            return interrupts64_last_exception_cr2();

        case X64_SYSCALL_GET_INVALID_OPCODE_COUNT:
            return (u64)interrupts64_invalid_opcode_count();

        case X64_SYSCALL_GET_PAGE_FAULT_COUNT:
            return (u64)interrupts64_page_fault_count();

        case X64_SYSCALL_GET_SERVICE_COUNT:
            return (u64)services64_count();

        case X64_SYSCALL_RESOLVE_SERVICE_CLASS:
            return (u64)services64_resolve_endpoint_class((u32)arg0);

        case X64_SYSCALL_GET_SERVICE_CAPABILITIES:
            return (u64)services64_capabilities_for_endpoint((u32)arg0);

        case X64_SYSCALL_GET_SERVICE_DELEGABLE:
            return (u64)services64_endpoint_is_delegable((u32)arg0);

        case X64_SYSCALL_GET_PACKAGE_SIGNER_COUNT:
            return (u64)services64_package_signer_count();

        case X64_SYSCALL_GET_PACKAGE_MANIFEST_COUNT:
            return (u64)services64_package_manifest_count();

        case X64_SYSCALL_GET_PACKAGE_PAYLOAD_COUNT:
            return (u64)services64_package_payload_count();

        case X64_SYSCALL_GET_PACKAGE_CHECKSUM:
            return (u64)services64_package_checksum();

        case X64_SYSCALL_GET_PACKAGE_VALID:
            return (u64)services64_package_valid();

        case X64_SYSCALL_CAP_GRANT_SERVICE:
            return (u64)capability64_grant_service((u32)arg0, (u32)arg1, (u32)arg2);

        case X64_SYSCALL_CAP_ROUTE:
            return (u64)capability64_route((u32)arg0, (u32)arg1, (u32)arg2);

        case X64_SYSCALL_CAP_REVOKE:
            return (u64)capability64_revoke((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_TARGET:
            return (u64)capability64_target_endpoint((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_RIGHTS:
            return (u64)capability64_rights((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_LIVE_COUNT:
            return (u64)capability64_live_count();

        case X64_SYSCALL_CAP_GRANT_COUNT:
            return (u64)capability64_grant_count();

        case X64_SYSCALL_CAP_ROUTE_COUNT:
            return (u64)capability64_route_count();

        case X64_SYSCALL_CAP_REVOKE_COUNT:
            return (u64)capability64_revoke_count();

        case X64_SYSCALL_CAP_DENIAL_COUNT:
            return (u64)capability64_denial_count();

        case X64_SYSCALL_CAP_DELEGATE:
            return (u64)capability64_delegate((u32)arg0, (u32)arg1, (u32)arg2);

        case X64_SYSCALL_CAP_PARENT:
            return (u64)capability64_parent((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_DELEGATE_COUNT:
            return (u64)capability64_delegate_count();

        case X64_SYSCALL_CAP_CASCADE_REVOKE_COUNT:
            return (u64)capability64_cascade_revoke_count();

        case X64_SYSCALL_CAP_OWNER:
            return (u64)capability64_owner((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_EXPIRY:
            return (u64)capability64_expiry_tick((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_EXPIRATION_COUNT:
            return (u64)capability64_expiration_count();

        case X64_SYSCALL_CAP_OWNER_DENIAL_COUNT:
            return (u64)capability64_owner_denial_count();

        case X64_SYSCALL_PRINCIPAL_COUNT:
            return (u64)principal64_count();

        case X64_SYSCALL_PRINCIPAL_ACTIVE:
            return (u64)principal64_is_active((u32)arg0);

        case X64_SYSCALL_PRINCIPAL_ROLE:
            return (u64)principal64_role((u32)arg0);

        case X64_SYSCALL_PRINCIPAL_BY_INDEX:
            return (u64)principal64_lookup_by_index((u32)arg0);

        case X64_SYSCALL_CAP_PRINCIPAL_DENIAL_COUNT:
            return (u64)capability64_principal_denial_count();

        case X64_SYSCALL_CAP_RUNTIME_GENERATION:
            return (u64)capability64_runtime_generation((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_RUNTIME_TOKEN:
            return (u64)capability64_runtime_token((u32)arg0, (u32)arg2);

        case X64_SYSCALL_CAP_RUNTIME_STALE_DENIAL_COUNT:
            return (u64)capability64_runtime_stale_denial_count();

        case X64_SYSCALL_PROCESS_COUNT:
            return (u64)process64_count();

        case X64_SYSCALL_PROCESS_PID_BY_INDEX:
            return (u64)process64_pid_by_index((u32)arg0);

        case X64_SYSCALL_PROCESS_PRINCIPAL:
            return (u64)process64_principal((u32)arg0);

        case X64_SYSCALL_PROCESS_ENDPOINT:
            return (u64)process64_endpoint((u32)arg0);

        case X64_SYSCALL_PROCESS_ENDPOINT_CLASS:
            return (u64)process64_endpoint_class((u32)arg0);

        case X64_SYSCALL_PROCESS_STATE:
            return (u64)process64_state((u32)arg0);

        case X64_SYSCALL_PROCESS_SCHEDULER_CLASS:
            return (u64)process64_scheduler_class((u32)arg0);

        case X64_SYSCALL_PROCESS_CAPABILITY_LIMIT:
            return (u64)process64_capability_limit((u32)arg0);

        case X64_SYSCALL_PROCESS_BY_PRINCIPAL:
            return (u64)process64_pid_for_principal((u32)arg0);

        case X64_SYSCALL_PROCESS_MANIFEST_INDEX:
            return (u64)process64_manifest_index((u32)arg0);

        case X64_SYSCALL_PROCESS_MANIFEST_PACKAGE:
            return (u64)process64_manifest_package_id((u32)arg0);

        case X64_SYSCALL_PROCESS_MANIFEST_EXECUTABLE:
            return (u64)process64_manifest_executable_id((u32)arg0);

        case X64_SYSCALL_PROCESS_MANIFEST_SIGNER:
            return (u64)process64_manifest_signer_id((u32)arg0);

        case X64_SYSCALL_PROCESS_MANIFEST_TOKEN:
            return (u64)process64_manifest_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_GENERATION:
            return (u64)process64_runtime_generation((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_TOKEN:
            return (u64)process64_runtime_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_GENERATION:
            return (u64)process64_runtime_image_generation((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_TOKEN:
            return (u64)process64_runtime_image_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_BASE:
            return (u64)process64_runtime_image_base((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY:
            return (u64)process64_runtime_image_entry((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAPPED_BYTES:
            return (u64)process64_runtime_image_mapped_bytes((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_RIGHTS:
            return (u64)process64_runtime_image_rights((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PLAN_TOKEN:
            return (u64)process64_runtime_image_plan_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_TOKEN:
            return (u64)process64_runtime_image_map_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PAGE_COUNT:
            return (u64)process64_runtime_image_page_count((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PML4_INDEX:
            return (u64)process64_runtime_image_pml4_index((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PDPT_INDEX:
            return (u64)process64_runtime_image_pdpt_index((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PD_INDEX:
            return (u64)process64_runtime_image_pd_index((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_ENTRY_TRANSFER_TOKEN:
            return (u64)process64_runtime_entry_transfer_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_INSTALL_TOKEN:
            return (u64)process64_runtime_image_install_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_SOURCE_CHECKSUM:
            return (u64)process64_runtime_image_source_checksum((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY_PROBE:
            return (u64)process64_runtime_image_entry_probe((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_INSTALLED:
            return (u64)process64_runtime_image_map_installed((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_FLAGS:
            return (u64)process64_runtime_image_protection_flags((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_TOKEN:
            return (u64)process64_runtime_image_protection_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_STATE:
            return (u64)process64_runtime_user_entry_state((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_TOKEN:
            return (u64)process64_runtime_user_entry_token((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_RIP:
            return (u64)process64_runtime_user_entry_rip((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_RSP:
            return (u64)process64_runtime_user_entry_rsp((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_SELECTORS:
            return (u64)process64_runtime_user_entry_selectors((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_RFLAGS:
            return (u64)process64_runtime_user_entry_rflags((u32)arg0);

        case X64_SYSCALL_FS_OPEN:
            return (u64)fs64_open(
                (u32)arg0,
                arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_CREATE:
            return (u64)fs64_create(
                (u32)arg0,
                arg1,
                syscall64_pack_low16(arg2),
                syscall64_pack_mid16(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_LIST:
            return (u64)fs64_list(
                (u32)arg0,
                arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_READ:
            return (u64)fs64_read(
                (u32)arg0,
                arg1,
                syscall64_pack_mid16(arg2),
                syscall64_pack_low16(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_WRITE:
            return (u64)fs64_write(
                (u32)arg0,
                arg1,
                syscall64_pack_mid16(arg2),
                syscall64_pack_low16(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_STAT:
            return (u64)fs64_stat(
                (u32)arg0,
                arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_REVOKE:
            return (u64)fs64_revoke((u32)arg0, (u32)arg2);

        case X64_SYSCALL_FS_RENAME:
            return (u64)fs64_rename(
                (u32)arg0,
                arg1,
                syscall64_pack_low16(arg2),
                syscall64_pack_mid16(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_MOVE:
            return (u64)fs64_move(
                (u32)arg0,
                syscall64_pack_high32(arg1),
                (u64)syscall64_pack_low32(arg1),
                syscall64_pack_low16(arg2),
                syscall64_pack_mid16(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_SHELL_EXECUTE_LINE:
            return (u64)shell64_execute_line(
                syscall64_pack_low32(arg0),
                syscall64_pack_high32(arg0),
                arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_FS_NODE_RIGHTS:
            return (u64)fs64_node_rights((u32)arg0, (u32)arg2);

        case X64_SYSCALL_FS_NODE_OWNER:
            return (u64)fs64_node_owner((u32)arg0, (u32)arg2);

        case X64_SYSCALL_FS_LIVE_COUNT:
            return (u64)fs64_live_count();

        case X64_SYSCALL_FS_OPEN_COUNT:
            return (u64)fs64_open_count();

        case X64_SYSCALL_FS_CREATE_COUNT:
            return (u64)fs64_create_count();

        case X64_SYSCALL_FS_LIST_COUNT:
            return (u64)fs64_list_count();

        case X64_SYSCALL_FS_READ_COUNT:
            return (u64)fs64_read_count();

        case X64_SYSCALL_FS_WRITE_COUNT:
            return (u64)fs64_write_count();

        case X64_SYSCALL_FS_STAT_COUNT:
            return (u64)fs64_stat_count();

        case X64_SYSCALL_FS_REVOKE_COUNT:
            return (u64)fs64_revoke_count();

        case X64_SYSCALL_FS_DENIAL_COUNT:
            return (u64)fs64_denial_count();

        case X64_SYSCALL_FS_STALE_DENIAL_COUNT:
            return (u64)fs64_stale_denial_count();

        case X64_SYSCALL_CONSOLE_WRITE:
            return (u64)console64_write(
                (u32)arg0,
                arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_CONSOLE_WRITE_COUNT:
            return (u64)console64_write_count();

        case X64_SYSCALL_CONSOLE_BYTE_COUNT:
            return (u64)console64_byte_count();

        case X64_SYSCALL_CONSOLE_DENIAL_COUNT:
            return (u64)console64_denial_count();

        case X64_SYSCALL_INPUT_READ:
            return (u64)input64_read(
                (u32)arg0,
                arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_INPUT_READ_COUNT:
            return (u64)input64_read_count();

        case X64_SYSCALL_INPUT_BYTE_COUNT:
            return (u64)input64_byte_count();

        case X64_SYSCALL_INPUT_DENIAL_COUNT:
            return (u64)input64_denial_count();

        case X64_SYSCALL_INPUT_EOF_COUNT:
            return (u64)input64_eof_count();

        case X64_SYSCALL_INPUT_READ_LINE:
            return (u64)input64_read_line(
                (u32)arg0,
                arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_INPUT_LINE_COUNT:
            return (u64)input64_line_count();

        case X64_SYSCALL_INPUT_EDIT_COUNT:
            return (u64)input64_edit_count();

        case X64_SYSCALL_INPUT_KEYBOARD_IRQ_COUNT:
            return (u64)input64_keyboard_irq_count();

        case X64_SYSCALL_INPUT_KEYBOARD_POLL_COUNT:
            return (u64)input64_keyboard_poll_count();

        case X64_SYSCALL_INPUT_KEYBOARD_SCANCODE_COUNT:
            return (u64)input64_keyboard_scancode_count();

        case X64_SYSCALL_INPUT_KEYBOARD_BYTE_COUNT:
            return (u64)input64_keyboard_byte_count();

        case X64_SYSCALL_INPUT_KEYBOARD_PENDING_COUNT:
            return (u64)input64_keyboard_pending_count();

        case X64_SYSCALL_INPUT_KEYBOARD_DROP_COUNT:
            return (u64)input64_keyboard_drop_count();

        case X64_SYSCALL_INPUT_KEYBOARD_LAST_SCANCODE:
            return (u64)input64_keyboard_last_scancode();

        case X64_SYSCALL_INPUT_KEYBOARD_LAST_BYTE:
            return (u64)input64_keyboard_last_byte();

        case X64_SYSCALL_INPUT_PS2_STATUS_SNAPSHOT:
            return (u64)input64_ps2_status_snapshot();

        case X64_SYSCALL_INPUT_READ_KEYBOARD:
            xhci64_poll_keyboard();
            {
                u64 result = (u64)input64_read_keyboard(
                    (u32)arg0,
                    arg1,
                    syscall64_pack_low32(arg2),
                    syscall64_pack_high32(arg2));
                syscall64_refresh_input_diagnostics_if_changed();
                return result;
            }

        case X64_SYSCALL_INPUT_KEYBOARD_READ_COUNT:
            return (u64)input64_keyboard_read_count();

        case X64_SYSCALL_INPUT_KEYBOARD_READ_BYTE_COUNT:
            return (u64)input64_keyboard_read_byte_count();

        case X64_SYSCALL_INPUT_READ_KEYBOARD_LINE:
            xhci64_poll_keyboard();
            {
                u64 result = (u64)input64_read_keyboard_line(
                    (u32)arg0,
                    arg1,
                    syscall64_pack_low32(arg2),
                    syscall64_pack_high32(arg2));
                syscall64_refresh_input_diagnostics_if_changed();
                return result;
            }

        case X64_SYSCALL_INPUT_KEYBOARD_LINE_COUNT:
            return (u64)input64_keyboard_line_count();

        case X64_SYSCALL_INPUT_KEYBOARD_LINE_BYTE_COUNT:
            return (u64)input64_keyboard_line_byte_count();

        case X64_SYSCALL_INPUT_KEYBOARD_LINE_EDIT_COUNT:
            return (u64)input64_keyboard_line_edit_count();

        case X64_SYSCALL_BLOCK_AVAILABLE:
            return (u64)block64_available();

        case X64_SYSCALL_BLOCK_LAST_STATUS:
            return (u64)block64_last_status();

        case X64_SYSCALL_BLOCK_READ_SECTOR:
            return (u64)block64_read_sector(
                (u32)arg0,
                syscall64_pack_low32(arg2),
                arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_BLOCK_READ_COUNT:
            return (u64)block64_read_count();

        case X64_SYSCALL_BLOCK_BYTE_COUNT:
            return (u64)block64_byte_count();

        case X64_SYSCALL_BLOCK_DENIAL_COUNT:
            return (u64)block64_denial_count();

        case X64_SYSCALL_BLOCK_UNAVAILABLE_COUNT:
            return (u64)block64_unavailable_count();

        case X64_SYSCALL_BLOCK_LAST_LBA:
            return (u64)block64_last_lba();

        case X64_SYSCALL_BLOCK_LAST_TOKEN:
            return (u64)block64_last_token();

        case X64_SYSCALL_PCI_DEVICE_COUNT:
            return (u64)pci64_device_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_MULTIFUNCTION_COUNT:
            return (u64)pci64_multifunction_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_STORAGE_COUNT:
            return (u64)pci64_storage_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_IDE_COUNT:
            return (u64)pci64_ide_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_AHCI_COUNT:
            return (u64)pci64_ahci_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_NVME_COUNT:
            return (u64)pci64_nvme_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_USB_COUNT:
            return (u64)pci64_usb_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_DISPLAY_COUNT:
            return (u64)pci64_display_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_FIRST_AHCI_ADDRESS:
            return (u64)pci64_first_ahci_address((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_FIRST_AHCI_VENDOR_DEVICE:
            return (u64)pci64_first_ahci_vendor_device((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_FIRST_AHCI_CLASS:
            return (u64)pci64_first_ahci_class((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_FIRST_AHCI_BAR5:
            return (u64)pci64_first_ahci_bar5((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_INVENTORY_TOKEN:
            return (u64)pci64_inventory_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_QUERY_COUNT:
            return (u64)pci64_query_count();

        case X64_SYSCALL_PCI_DENIAL_COUNT:
            return (u64)pci64_denial_count();

        case X64_SYSCALL_PCI_FIRST_AHCI_MMIO_BASE:
            return (u64)pci64_first_ahci_mmio_base((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_FIRST_AHCI_MMIO_SPAN_HINT:
            return (u64)pci64_first_ahci_mmio_span_hint((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_FIRST_AHCI_MMIO_FLAGS:
            return (u64)pci64_first_ahci_mmio_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_PCI_FIRST_AHCI_MMIO_TOKEN:
            return (u64)pci64_first_ahci_mmio_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_PLAN_COUNT:
            return (u64)mmio64_plan_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BASE:
            return (u64)mmio64_ahci_base((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SPAN:
            return (u64)mmio64_ahci_span((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_FLAGS:
            return (u64)mmio64_ahci_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_STATE:
            return (u64)mmio64_ahci_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TOKEN:
            return (u64)mmio64_ahci_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_QUERY_COUNT:
            return (u64)mmio64_query_count();

        case X64_SYSCALL_MMIO_DENIAL_COUNT:
            return (u64)mmio64_denial_count();

        case X64_SYSCALL_MMIO_REQUEST_AHCI_MAPPING:
            return (u64)mmio64_request_ahci_mapping((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_VIRTUAL_BASE:
            return mmio64_ahci_map_virtual_base((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_PAGE_COUNT:
            return (u64)mmio64_ahci_map_page_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_FLAGS:
            return (u64)mmio64_ahci_map_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_STATE:
            return (u64)mmio64_ahci_map_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_TOKEN:
            return (u64)mmio64_ahci_map_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_INSTALLED:
            return (u64)mmio64_ahci_map_installed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_MAP_REQUEST_COUNT:
            return (u64)mmio64_map_request_count();

        case X64_SYSCALL_MMIO_MAP_DENIAL_COUNT:
            return (u64)mmio64_map_denial_count();

        case X64_SYSCALL_MMIO_MAP_UNAVAILABLE_COUNT:
            return (u64)mmio64_map_unavailable_count();

        case X64_SYSCALL_MMIO_AHCI_MAP_INSTALL_TOKEN:
            return (u64)mmio64_ahci_map_install_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_PML4_INDEX:
            return (u64)mmio64_ahci_map_pml4_index((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_PDPT_INDEX:
            return (u64)mmio64_ahci_map_pdpt_index((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_PD_INDEX:
            return (u64)mmio64_ahci_map_pd_index((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_PT_INDEX:
            return (u64)mmio64_ahci_map_pt_index((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_ENTRY_FLAGS:
            return mmio64_ahci_map_entry_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MAP_NX_ENABLED:
            return (u64)mmio64_ahci_map_nx_enabled((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_SNAPSHOT_AHCI_REGISTERS:
            return (u64)mmio64_snapshot_ahci_registers((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_STATE:
            return (u64)mmio64_ahci_snapshot_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_FLAGS:
            return (u64)mmio64_ahci_snapshot_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_TOKEN:
            return (u64)mmio64_ahci_snapshot_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_CAP:
            return (u64)mmio64_ahci_snapshot_cap((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_GHC:
            return (u64)mmio64_ahci_snapshot_ghc((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_PI:
            return (u64)mmio64_ahci_snapshot_pi((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_VERSION:
            return (u64)mmio64_ahci_snapshot_version((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_READ_COUNT:
            return (u64)mmio64_ahci_snapshot_read_count();

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_DENIAL_COUNT:
            return (u64)mmio64_ahci_snapshot_denial_count();

        case X64_SYSCALL_MMIO_AHCI_SNAPSHOT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_snapshot_unavailable_count();

        case X64_SYSCALL_MMIO_SNAPSHOT_AHCI_PORTS:
            return (u64)mmio64_snapshot_ahci_ports((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_STATE:
            return (u64)mmio64_ahci_port_snapshot_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_FLAGS:
            return (u64)mmio64_ahci_port_snapshot_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_TOKEN:
            return (u64)mmio64_ahci_port_snapshot_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_IMPLEMENTED_COUNT:
            return (u64)mmio64_ahci_port_implemented_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_ACTIVE_COUNT:
            return (u64)mmio64_ahci_port_active_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_FIRST_IMPLEMENTED:
            return (u64)mmio64_ahci_port_first_implemented((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_FIRST_ACTIVE:
            return (u64)mmio64_ahci_port_first_active((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SELECTED:
            return (u64)mmio64_ahci_port_selected((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_SSTS:
            return (u64)mmio64_ahci_port_selected_ssts((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_SIGNATURE:
            return (u64)mmio64_ahci_port_selected_signature((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_COMMAND:
            return (u64)mmio64_ahci_port_selected_command((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_TASK_FILE:
            return (u64)mmio64_ahci_port_selected_task_file((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_CI:
            return (u64)mmio64_ahci_port_selected_ci((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_SERR:
            return (u64)mmio64_ahci_port_selected_serr((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_READ_COUNT:
            return (u64)mmio64_ahci_port_snapshot_read_count();

        case X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_DENIAL_COUNT:
            return (u64)mmio64_ahci_port_snapshot_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_port_snapshot_unavailable_count();

        case X64_SYSCALL_MMIO_CLASSIFY_AHCI_PORT:
            return (u64)mmio64_classify_ahci_port((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_STATE:
            return (u64)mmio64_ahci_port_policy_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_FLAGS:
            return (u64)mmio64_ahci_port_policy_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_TOKEN:
            return (u64)mmio64_ahci_port_policy_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DEVICE_KIND:
            return (u64)mmio64_ahci_port_policy_device_kind((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DET:
            return (u64)mmio64_ahci_port_policy_det((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_SPD:
            return (u64)mmio64_ahci_port_policy_spd((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_IPM:
            return (u64)mmio64_ahci_port_policy_ipm((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_READY:
            return (u64)mmio64_ahci_port_policy_ready((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_BUSY:
            return (u64)mmio64_ahci_port_policy_busy((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DRQ:
            return (u64)mmio64_ahci_port_policy_drq((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_CI_IDLE:
            return (u64)mmio64_ahci_port_policy_ci_idle((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_SERR_CLEAR:
            return (u64)mmio64_ahci_port_policy_serr_clear((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_READ_COUNT:
            return (u64)mmio64_ahci_port_policy_read_count();

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DENIAL_COUNT:
            return (u64)mmio64_ahci_port_policy_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PORT_POLICY_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_port_policy_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_READ_PLAN:
            return (u64)mmio64_stage_ahci_read_plan(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_STATE:
            return (u64)mmio64_ahci_read_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_FLAGS:
            return (u64)mmio64_ahci_read_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_TOKEN:
            return (u64)mmio64_ahci_read_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_OPERATION:
            return (u64)mmio64_ahci_read_plan_operation((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_PORT:
            return (u64)mmio64_ahci_read_plan_port((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_POLICY_TOKEN:
            return (u64)mmio64_ahci_read_plan_policy_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_LBA_LOW:
            return (u64)mmio64_ahci_read_plan_lba_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_BLOCK_COUNT:
            return (u64)mmio64_ahci_read_plan_block_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_BYTE_COUNT_HINT:
            return (u64)mmio64_ahci_read_plan_byte_count_hint((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_COMMAND_SLOT:
            return (u64)mmio64_ahci_read_plan_command_slot((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_ARMED:
            return (u64)mmio64_ahci_read_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_ISSUED:
            return (u64)mmio64_ahci_read_plan_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_read_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_read_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_read_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_READ_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_read_plan_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_COMMAND_PLAN:
            return (u64)mmio64_stage_ahci_command_plan((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_STATE:
            return (u64)mmio64_ahci_command_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_FLAGS:
            return (u64)mmio64_ahci_command_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_TOKEN:
            return (u64)mmio64_ahci_command_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_READ_TOKEN:
            return (u64)mmio64_ahci_command_plan_read_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_OPERATION:
            return (u64)mmio64_ahci_command_plan_operation((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_SLOT:
            return (u64)mmio64_ahci_command_plan_slot((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_HEADER_BYTES:
            return (u64)mmio64_ahci_command_plan_header_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_TABLE_BYTES:
            return (u64)mmio64_ahci_command_plan_table_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_CFIS_BYTES:
            return (u64)mmio64_ahci_command_plan_cfis_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_CFIS_DWORDS:
            return (u64)mmio64_ahci_command_plan_cfis_dwords((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_PRDT_ENTRIES:
            return (u64)mmio64_ahci_command_plan_prdt_entries((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_PRDT_BYTES:
            return (u64)mmio64_ahci_command_plan_prdt_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_ATAPI_PACKET_BYTES:
            return (u64)mmio64_ahci_command_plan_atapi_packet_bytes(
                (u32)arg0,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_COMMAND_OPCODE:
            return (u64)mmio64_ahci_command_plan_command_opcode((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_PACKET_OPCODE:
            return (u64)mmio64_ahci_command_plan_packet_opcode((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_TRANSFER_BYTES:
            return (u64)mmio64_ahci_command_plan_transfer_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_ARMED:
            return (u64)mmio64_ahci_command_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_ISSUED:
            return (u64)mmio64_ahci_command_plan_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_command_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_command_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_command_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_command_plan_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_MEMORY_PLAN:
            return (u64)mmio64_stage_ahci_memory_plan((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_STATE:
            return (u64)mmio64_ahci_memory_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_FLAGS:
            return (u64)mmio64_ahci_memory_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_TOKEN:
            return (u64)mmio64_ahci_memory_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_COMMAND_TOKEN:
            return (u64)mmio64_ahci_memory_plan_command_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_SLOT:
            return (u64)mmio64_ahci_memory_plan_slot((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PAGE_COUNT:
            return (u64)mmio64_ahci_memory_plan_page_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PAGE_BYTES:
            return (u64)mmio64_ahci_memory_plan_page_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_COMMAND_LIST_OFFSET:
            return (u64)mmio64_ahci_memory_plan_command_list_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_COMMAND_LIST_BYTES:
            return (u64)mmio64_ahci_memory_plan_command_list_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_COMMAND_HEADER_OFFSET:
            return (u64)mmio64_ahci_memory_plan_command_header_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_COMMAND_HEADER_BYTES:
            return (u64)mmio64_ahci_memory_plan_command_header_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_COMMAND_TABLE_OFFSET:
            return (u64)mmio64_ahci_memory_plan_command_table_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_COMMAND_TABLE_BYTES:
            return (u64)mmio64_ahci_memory_plan_command_table_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PRDT_OFFSET:
            return (u64)mmio64_ahci_memory_plan_prdt_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PRDT_BYTES:
            return (u64)mmio64_ahci_memory_plan_prdt_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_BOUNCE_OFFSET:
            return (u64)mmio64_ahci_memory_plan_bounce_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_BOUNCE_BYTES:
            return (u64)mmio64_ahci_memory_plan_bounce_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PRDT_DATA_BYTE_COUNT:
            return (u64)mmio64_ahci_memory_plan_prdt_data_byte_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_DMA_ADDRESS_LOW:
            return (u64)mmio64_ahci_memory_plan_dma_address_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_DMA_ADDRESS_HIGH:
            return (u64)mmio64_ahci_memory_plan_dma_address_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_memory_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_TABLE_WRITTEN:
            return (u64)mmio64_ahci_memory_plan_table_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_memory_plan_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_ARMED:
            return (u64)mmio64_ahci_memory_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_memory_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_memory_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_memory_plan_unavailable_count();

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PAGE_VIRTUAL:
            return mmio64_ahci_memory_plan_page_virtual((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PAGE_PHYSICAL:
            return mmio64_ahci_memory_plan_page_physical((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PAGE_CHECKSUM:
            return (u64)mmio64_ahci_memory_plan_page_checksum((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PAGE_ZEROED:
            return (u64)mmio64_ahci_memory_plan_page_zeroed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_PAGE_MATERIALIZED:
            return (u64)mmio64_ahci_memory_plan_page_materialized((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_PREPARE_AHCI_COMMAND_TABLE:
            return (u64)mmio64_prepare_ahci_command_table((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_STATE:
            return (u64)mmio64_ahci_table_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_FLAGS:
            return (u64)mmio64_ahci_table_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_TOKEN:
            return (u64)mmio64_ahci_table_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_MEMORY_TOKEN:
            return (u64)mmio64_ahci_table_plan_memory_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CHECKSUM_BEFORE:
            return (u64)mmio64_ahci_table_plan_checksum_before((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CHECKSUM_AFTER:
            return (u64)mmio64_ahci_table_plan_checksum_after((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CHECKSUM_CHANGED:
            return (u64)mmio64_ahci_table_plan_checksum_changed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_HEADER_FLAGS:
            return (u64)mmio64_ahci_table_plan_header_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_HEADER_PRDT_LENGTH:
            return (u64)mmio64_ahci_table_plan_header_prdt_length((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_HEADER_PRDBC:
            return (u64)mmio64_ahci_table_plan_header_prdbc((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_HEADER_CTBA_LOW:
            return (u64)mmio64_ahci_table_plan_header_ctba_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_HEADER_CTBA_HIGH:
            return (u64)mmio64_ahci_table_plan_header_ctba_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CFIS_TYPE:
            return (u64)mmio64_ahci_table_plan_cfis_type((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CFIS_FLAGS:
            return (u64)mmio64_ahci_table_plan_cfis_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CFIS_COMMAND:
            return (u64)mmio64_ahci_table_plan_cfis_command((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CFIS_DEVICE:
            return (u64)mmio64_ahci_table_plan_cfis_device((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_CFIS_COUNT:
            return (u64)mmio64_ahci_table_plan_cfis_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_PACKET_OPCODE:
            return (u64)mmio64_ahci_table_plan_packet_opcode((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_PACKET_BLOCKS:
            return (u64)mmio64_ahci_table_plan_packet_blocks((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_PRDT_DBA_LOW:
            return (u64)mmio64_ahci_table_plan_prdt_dba_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_PRDT_DBA_HIGH:
            return (u64)mmio64_ahci_table_plan_prdt_dba_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_PRDT_BYTE_COUNT:
            return (u64)mmio64_ahci_table_plan_prdt_byte_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_TABLE_WRITTEN:
            return (u64)mmio64_ahci_table_plan_table_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_table_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_table_plan_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_ARMED:
            return (u64)mmio64_ahci_table_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_ISSUED:
            return (u64)mmio64_ahci_table_plan_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_table_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_table_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_table_plan_unavailable_count();

        case X64_SYSCALL_MMIO_PREFLIGHT_AHCI_COMMAND_ISSUE:
            return (u64)mmio64_preflight_ahci_command_issue((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_STATE:
            return (u64)mmio64_ahci_issue_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_FLAGS:
            return (u64)mmio64_ahci_issue_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_TOKEN:
            return (u64)mmio64_ahci_issue_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_TABLE_TOKEN:
            return (u64)mmio64_ahci_issue_plan_table_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_MEMORY_TOKEN:
            return (u64)mmio64_ahci_issue_plan_memory_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_COMMAND_TOKEN:
            return (u64)mmio64_ahci_issue_plan_command_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_READ_TOKEN:
            return (u64)mmio64_ahci_issue_plan_read_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_PORT:
            return (u64)mmio64_ahci_issue_plan_port((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_SLOT:
            return (u64)mmio64_ahci_issue_plan_slot((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_CI_SNAPSHOT:
            return (u64)mmio64_ahci_issue_plan_ci_snapshot((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_SLOT_MASK:
            return (u64)mmio64_ahci_issue_plan_slot_mask((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_SLOT_IDLE:
            return (u64)mmio64_ahci_issue_plan_slot_idle((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_TFD_READY:
            return (u64)mmio64_ahci_issue_plan_tfd_ready((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_SERR_CLEAR:
            return (u64)mmio64_ahci_issue_plan_serr_clear((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_POLICY_READY:
            return (u64)mmio64_ahci_issue_plan_policy_ready((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_ENGINE_ST:
            return (u64)mmio64_ahci_issue_plan_engine_st((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_ENGINE_FRE:
            return (u64)mmio64_ahci_issue_plan_engine_fre((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_ENGINE_FR:
            return (u64)mmio64_ahci_issue_plan_engine_fr((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_ENGINE_CR:
            return (u64)mmio64_ahci_issue_plan_engine_cr((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_STOP_REQUIRED:
            return (u64)mmio64_ahci_issue_plan_stop_required((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_START_REQUIRED:
            return (u64)mmio64_ahci_issue_plan_start_required((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_TIMEOUT_TICKS:
            return (u64)mmio64_ahci_issue_plan_timeout_ticks((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_POLL_BUDGET:
            return (u64)mmio64_ahci_issue_plan_poll_budget((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_TABLE_CHECKSUM:
            return (u64)mmio64_ahci_issue_plan_table_checksum((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_EXPECTED_TABLE_CHECKSUM:
            return (u64)mmio64_ahci_issue_plan_expected_table_checksum(
                (u32)arg0,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_CHECKSUM_MATCH:
            return (u64)mmio64_ahci_issue_plan_checksum_match((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_issue_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_issue_plan_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_COMMAND_ISSUED:
            return (u64)mmio64_ahci_issue_plan_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_ARMED:
            return (u64)mmio64_ahci_issue_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_issue_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_issue_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_issue_plan_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_ADDRESS_BIND_PLAN:
            return (u64)mmio64_stage_ahci_address_bind_plan((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_STATE:
            return (u64)mmio64_ahci_bind_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_FLAGS:
            return (u64)mmio64_ahci_bind_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_TOKEN:
            return (u64)mmio64_ahci_bind_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_ISSUE_TOKEN:
            return (u64)mmio64_ahci_bind_plan_issue_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_TABLE_TOKEN:
            return (u64)mmio64_ahci_bind_plan_table_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_MEMORY_TOKEN:
            return (u64)mmio64_ahci_bind_plan_memory_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_COMMAND_TOKEN:
            return (u64)mmio64_ahci_bind_plan_command_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_READ_TOKEN:
            return (u64)mmio64_ahci_bind_plan_read_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PAGE_PHYSICAL_LOW:
            return (u64)mmio64_ahci_bind_plan_page_physical_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PAGE_PHYSICAL_HIGH:
            return (u64)mmio64_ahci_bind_plan_page_physical_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_COMMAND_LIST_LOW:
            return (u64)mmio64_ahci_bind_plan_command_list_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_COMMAND_LIST_HIGH:
            return (u64)mmio64_ahci_bind_plan_command_list_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_COMMAND_TABLE_LOW:
            return (u64)mmio64_ahci_bind_plan_command_table_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_COMMAND_TABLE_HIGH:
            return (u64)mmio64_ahci_bind_plan_command_table_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_BOUNCE_LOW:
            return (u64)mmio64_ahci_bind_plan_bounce_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_BOUNCE_HIGH:
            return (u64)mmio64_ahci_bind_plan_bounce_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_HEADER_CTBA_LOW:
            return (u64)mmio64_ahci_bind_plan_header_ctba_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_HEADER_CTBA_HIGH:
            return (u64)mmio64_ahci_bind_plan_header_ctba_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PRDT_DBA_LOW:
            return (u64)mmio64_ahci_bind_plan_prdt_dba_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PRDT_DBA_HIGH:
            return (u64)mmio64_ahci_bind_plan_prdt_dba_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PRDT_BYTE_COUNT:
            return (u64)mmio64_ahci_bind_plan_prdt_byte_count((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_HEADER_PATCH_OFFSET:
            return (u64)mmio64_ahci_bind_plan_header_patch_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PRDT_PATCH_OFFSET:
            return (u64)mmio64_ahci_bind_plan_prdt_patch_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_CHECKSUM_BEFORE:
            return (u64)mmio64_ahci_bind_plan_checksum_before((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_CHECKSUM_PREDICTED:
            return (u64)mmio64_ahci_bind_plan_checksum_predicted((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_CHECKSUM_CHANGED:
            return (u64)mmio64_ahci_bind_plan_checksum_changed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_ADDRESS_ALIGNED:
            return (u64)mmio64_ahci_bind_plan_address_aligned((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_ADDRESS_RANGE_READY:
            return (u64)mmio64_ahci_bind_plan_address_range_ready((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_ADDRESS_BELOW_4G:
            return (u64)mmio64_ahci_bind_plan_address_below_4g((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_MEMORY_WRITTEN:
            return (u64)mmio64_ahci_bind_plan_memory_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_bind_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_bind_plan_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_PUBLISHED:
            return (u64)mmio64_ahci_bind_plan_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_COMMAND_ISSUED:
            return (u64)mmio64_ahci_bind_plan_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_ARMED:
            return (u64)mmio64_ahci_bind_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_bind_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_bind_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_BIND_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_bind_plan_unavailable_count();

        case X64_SYSCALL_MMIO_APPLY_AHCI_PRIVATE_ADDRESS_PATCH:
            return (u64)mmio64_apply_ahci_private_address_patch((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_STATE:
            return (u64)mmio64_ahci_patch_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_FLAGS:
            return (u64)mmio64_ahci_patch_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_TOKEN:
            return (u64)mmio64_ahci_patch_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_BIND_TOKEN:
            return (u64)mmio64_ahci_patch_plan_bind_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_ISSUE_TOKEN:
            return (u64)mmio64_ahci_patch_plan_issue_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_TABLE_TOKEN:
            return (u64)mmio64_ahci_patch_plan_table_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_MEMORY_TOKEN:
            return (u64)mmio64_ahci_patch_plan_memory_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_COMMAND_TOKEN:
            return (u64)mmio64_ahci_patch_plan_command_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_READ_TOKEN:
            return (u64)mmio64_ahci_patch_plan_read_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_HEADER_PATCH_OFFSET:
            return (u64)mmio64_ahci_patch_plan_header_patch_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_PRDT_PATCH_OFFSET:
            return (u64)mmio64_ahci_patch_plan_prdt_patch_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_HEADER_CTBA_LOW:
            return (u64)mmio64_ahci_patch_plan_header_ctba_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_HEADER_CTBA_HIGH:
            return (u64)mmio64_ahci_patch_plan_header_ctba_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_PRDT_DBA_LOW:
            return (u64)mmio64_ahci_patch_plan_prdt_dba_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_PRDT_DBA_HIGH:
            return (u64)mmio64_ahci_patch_plan_prdt_dba_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_CHECKSUM_BEFORE:
            return (u64)mmio64_ahci_patch_plan_checksum_before((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_CHECKSUM_EXPECTED:
            return (u64)mmio64_ahci_patch_plan_checksum_expected((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_CHECKSUM_AFTER:
            return (u64)mmio64_ahci_patch_plan_checksum_after((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_CHECKSUM_MATCH:
            return (u64)mmio64_ahci_patch_plan_checksum_match((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_CHECKSUM_CHANGED:
            return (u64)mmio64_ahci_patch_plan_checksum_changed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_MEMORY_WRITTEN:
            return (u64)mmio64_ahci_patch_plan_memory_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_patch_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_patch_plan_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_PUBLISHED:
            return (u64)mmio64_ahci_patch_plan_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_COMMAND_ISSUED:
            return (u64)mmio64_ahci_patch_plan_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_ARMED:
            return (u64)mmio64_ahci_patch_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_patch_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_patch_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_patch_plan_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_CONTROLLER_PUBLISH_PLAN:
            return (u64)mmio64_stage_ahci_controller_publish_plan((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_STATE:
            return (u64)mmio64_ahci_publish_plan_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_FLAGS:
            return (u64)mmio64_ahci_publish_plan_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_TOKEN:
            return (u64)mmio64_ahci_publish_plan_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_PATCH_TOKEN:
            return (u64)mmio64_ahci_publish_plan_patch_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_BIND_TOKEN:
            return (u64)mmio64_ahci_publish_plan_bind_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_ISSUE_TOKEN:
            return (u64)mmio64_ahci_publish_plan_issue_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_TABLE_TOKEN:
            return (u64)mmio64_ahci_publish_plan_table_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_MEMORY_TOKEN:
            return (u64)mmio64_ahci_publish_plan_memory_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_COMMAND_TOKEN:
            return (u64)mmio64_ahci_publish_plan_command_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_READ_TOKEN:
            return (u64)mmio64_ahci_publish_plan_read_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_PORT:
            return (u64)mmio64_ahci_publish_plan_port((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_PORT_BASE_OFFSET:
            return (u64)mmio64_ahci_publish_plan_port_base_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_COMMAND_LIST_LOW:
            return (u64)mmio64_ahci_publish_plan_command_list_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_COMMAND_LIST_HIGH:
            return (u64)mmio64_ahci_publish_plan_command_list_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_RECEIVE_FIS_LOW:
            return (u64)mmio64_ahci_publish_plan_receive_fis_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_RECEIVE_FIS_HIGH:
            return (u64)mmio64_ahci_publish_plan_receive_fis_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_CLB_OFFSET:
            return (u64)mmio64_ahci_publish_plan_clb_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_CLBU_OFFSET:
            return (u64)mmio64_ahci_publish_plan_clbu_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_FB_OFFSET:
            return (u64)mmio64_ahci_publish_plan_fb_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_FBU_OFFSET:
            return (u64)mmio64_ahci_publish_plan_fbu_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_CMD_OFFSET:
            return (u64)mmio64_ahci_publish_plan_cmd_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_CI_OFFSET:
            return (u64)mmio64_ahci_publish_plan_ci_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_CLB_VALUE_LOW:
            return (u64)mmio64_ahci_publish_plan_clb_value_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_CLB_VALUE_HIGH:
            return (u64)mmio64_ahci_publish_plan_clb_value_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_FB_VALUE_LOW:
            return (u64)mmio64_ahci_publish_plan_fb_value_low((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_FB_VALUE_HIGH:
            return (u64)mmio64_ahci_publish_plan_fb_value_high((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_RECEIVE_FIS_OFFSET:
            return (u64)mmio64_ahci_publish_plan_receive_fis_offset((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_RECEIVE_FIS_BYTES:
            return (u64)mmio64_ahci_publish_plan_receive_fis_bytes((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_PAGE_CHECKSUM:
            return (u64)mmio64_ahci_publish_plan_page_checksum((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_PAGE_CHECKSUM_MATCH:
            return (u64)mmio64_ahci_publish_plan_page_checksum_match((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_CLB_ALIGNED:
            return (u64)mmio64_ahci_publish_plan_clb_aligned((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_FIS_ALIGNED:
            return (u64)mmio64_ahci_publish_plan_fis_aligned((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_RANGE_READY:
            return (u64)mmio64_ahci_publish_plan_range_ready((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_BELOW_4G:
            return (u64)mmio64_ahci_publish_plan_below_4g((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_MEMORY_WRITTEN:
            return (u64)mmio64_ahci_publish_plan_memory_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_DMA_MAPPED:
            return (u64)mmio64_ahci_publish_plan_dma_mapped((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_plan_mmio_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_plan_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_PUBLISHED:
            return (u64)mmio64_ahci_publish_plan_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_plan_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_ARMED:
            return (u64)mmio64_ahci_publish_plan_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_plan_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_plan_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_plan_unavailable_count();

        case X64_SYSCALL_MMIO_GATE_AHCI_CONTROLLER_PUBLICATION:
            return (u64)mmio64_gate_ahci_controller_publication((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_STATE:
            return (u64)mmio64_ahci_publish_gate_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_FLAGS:
            return (u64)mmio64_ahci_publish_gate_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_TOKEN:
            return (u64)mmio64_ahci_publish_gate_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_PUBLISH_TOKEN:
            return (u64)mmio64_ahci_publish_gate_publish_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_LIVE_HARDWARE_HANDLES:
            return (u64)mmio64_ahci_publish_gate_live_hardware_handles((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_EXCLUSIVE_HARDWARE_HANDLE:
            return (u64)mmio64_ahci_publish_gate_exclusive_hardware_handle((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_REVOCATION_REQUIRED:
            return (u64)mmio64_ahci_publish_gate_revocation_required((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_REVOCATION_SATISFIED:
            return (u64)mmio64_ahci_publish_gate_revocation_satisfied((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_WRITE_WINDOW_ENABLED:
            return (u64)mmio64_ahci_publish_gate_write_window_enabled((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_COMMIT_ALLOWED:
            return (u64)mmio64_ahci_publish_gate_commit_allowed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_gate_mmio_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_gate_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_PUBLISHED:
            return (u64)mmio64_ahci_publish_gate_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_gate_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_ARMED:
            return (u64)mmio64_ahci_publish_gate_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_gate_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_gate_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_gate_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_WRITE_WINDOW_POLICY:
            return (u64)mmio64_stage_ahci_publish_write_window_policy((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_STATE:
            return (u64)mmio64_ahci_publish_window_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_FLAGS:
            return (u64)mmio64_ahci_publish_window_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_TOKEN:
            return (u64)mmio64_ahci_publish_window_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_GATE_TOKEN:
            return (u64)mmio64_ahci_publish_window_gate_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_PUBLISH_TOKEN:
            return (u64)mmio64_ahci_publish_window_publish_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_LIVE_HARDWARE_HANDLES:
            return (u64)mmio64_ahci_publish_window_live_hardware_handles((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_EXCLUSIVE_HARDWARE_HANDLE:
            return (u64)mmio64_ahci_publish_window_exclusive_hardware_handle((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_REVOCATION_REQUIRED:
            return (u64)mmio64_ahci_publish_window_revocation_required((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_REVOCATION_SATISFIED:
            return (u64)mmio64_ahci_publish_window_revocation_satisfied((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_REVOCATION_EXECUTED:
            return (u64)mmio64_ahci_publish_window_revocation_executed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_WRITE_WINDOW_ENABLED:
            return (u64)mmio64_ahci_publish_window_write_window_enabled((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_COMMIT_ALLOWED:
            return (u64)mmio64_ahci_publish_window_commit_allowed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_window_mmio_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_window_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_PUBLISHED:
            return (u64)mmio64_ahci_publish_window_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_window_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_ARMED:
            return (u64)mmio64_ahci_publish_window_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_window_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_window_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_window_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_REVOCATION_PLAN:
            return (u64)mmio64_stage_ahci_publish_revocation_plan((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_STATE:
            return (u64)mmio64_ahci_publish_revoke_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_FLAGS:
            return (u64)mmio64_ahci_publish_revoke_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_TOKEN:
            return (u64)mmio64_ahci_publish_revoke_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_WINDOW_TOKEN:
            return (u64)mmio64_ahci_publish_revoke_window_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_GATE_TOKEN:
            return (u64)mmio64_ahci_publish_revoke_gate_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_LIVE_BEFORE:
            return (u64)mmio64_ahci_publish_revoke_live_before((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_LIVE_AFTER:
            return (u64)mmio64_ahci_publish_revoke_live_after((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_EXCLUSIVE_HARDWARE_HANDLE:
            return (u64)mmio64_ahci_publish_revoke_exclusive_hardware_handle((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_REVOCATION_REQUIRED:
            return (u64)mmio64_ahci_publish_revoke_revocation_required((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_REVOCATION_PLANNED:
            return (u64)mmio64_ahci_publish_revoke_revocation_planned((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_REVOCATION_EXECUTED:
            return (u64)mmio64_ahci_publish_revoke_revocation_executed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_WOULD_REVOKE:
            return (u64)mmio64_ahci_publish_revoke_would_revoke((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_WRITE_WINDOW_ENABLED:
            return (u64)mmio64_ahci_publish_revoke_write_window_enabled((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_COMMIT_ALLOWED:
            return (u64)mmio64_ahci_publish_revoke_commit_allowed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_revoke_mmio_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_revoke_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_PUBLISHED:
            return (u64)mmio64_ahci_publish_revoke_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_revoke_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_ARMED:
            return (u64)mmio64_ahci_publish_revoke_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_revoke_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_revoke_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_revoke_unavailable_count();

        case X64_SYSCALL_MMIO_TRY_OPEN_AHCI_PUBLISH_WRITE_WINDOW:
            return (u64)mmio64_try_open_ahci_publish_write_window((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_STATE:
            return (u64)mmio64_ahci_publish_open_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_FLAGS:
            return (u64)mmio64_ahci_publish_open_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_TOKEN:
            return (u64)mmio64_ahci_publish_open_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOKE_TOKEN:
            return (u64)mmio64_ahci_publish_open_revoke_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_WINDOW_TOKEN:
            return (u64)mmio64_ahci_publish_open_window_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_LIVE_HARDWARE_HANDLES:
            return (u64)mmio64_ahci_publish_open_live_hardware_handles((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOCATION_REQUIRED:
            return (u64)mmio64_ahci_publish_open_revocation_required((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOCATION_PLANNED:
            return (u64)mmio64_ahci_publish_open_revocation_planned((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOCATION_EXECUTED:
            return (u64)mmio64_ahci_publish_open_revocation_executed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_WRITE_WINDOW_ENABLED:
            return (u64)mmio64_ahci_publish_open_write_window_enabled((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_ALLOWED:
            return (u64)mmio64_ahci_publish_open_allowed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_COMMIT_ALLOWED:
            return (u64)mmio64_ahci_publish_open_commit_allowed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_open_mmio_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_open_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_PUBLISHED:
            return (u64)mmio64_ahci_publish_open_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_open_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_ARMED:
            return (u64)mmio64_ahci_publish_open_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_open_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_open_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_open_unavailable_count();

        case X64_SYSCALL_MMIO_REQUEST_AHCI_PUBLISH_EXCLUSIVE_SESSION:
            return (u64)mmio64_request_ahci_publish_exclusive_session((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_STATE:
            return (u64)mmio64_ahci_publish_session_state((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_FLAGS:
            return (u64)mmio64_ahci_publish_session_flags((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_TOKEN:
            return (u64)mmio64_ahci_publish_session_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_OPEN_TOKEN:
            return (u64)mmio64_ahci_publish_session_open_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOKE_TOKEN:
            return (u64)mmio64_ahci_publish_session_revoke_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_WINDOW_TOKEN:
            return (u64)mmio64_ahci_publish_session_window_token((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_LIVE_HARDWARE_HANDLES:
            return (u64)mmio64_ahci_publish_session_live_hardware_handles((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOCATION_REQUIRED:
            return (u64)mmio64_ahci_publish_session_revocation_required((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOCATION_PLANNED:
            return (u64)mmio64_ahci_publish_session_revocation_planned((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOCATION_EXECUTED:
            return (u64)mmio64_ahci_publish_session_revocation_executed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_ALLOWED:
            return (u64)mmio64_ahci_publish_session_allowed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_DRIVER_OWNED:
            return (u64)mmio64_ahci_publish_session_driver_owned((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_WRITE_WINDOW_ENABLED:
            return (u64)mmio64_ahci_publish_session_write_window_enabled((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_COMMIT_ALLOWED:
            return (u64)mmio64_ahci_publish_session_commit_allowed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_session_mmio_written((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_session_port_programmed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_PUBLISHED:
            return (u64)mmio64_ahci_publish_session_published((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_session_command_issued((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_ARMED:
            return (u64)mmio64_ahci_publish_session_armed((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_session_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_session_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_session_unavailable_count();

        case X64_SYSCALL_MMIO_EXECUTE_AHCI_PUBLISH_CAPABILITY_DRAIN:
            return (u64)mmio64_execute_ahci_publish_capability_drain((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_STATE:
            return (u64)mmio64_ahci_publish_drain_state();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_FLAGS:
            return (u64)mmio64_ahci_publish_drain_flags();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_TOKEN:
            return (u64)mmio64_ahci_publish_drain_token();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_SESSION_TOKEN:
            return (u64)mmio64_ahci_publish_drain_session_token();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_OPEN_TOKEN:
            return (u64)mmio64_ahci_publish_drain_open_token();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOKE_TOKEN:
            return (u64)mmio64_ahci_publish_drain_revoke_token();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_WINDOW_TOKEN:
            return (u64)mmio64_ahci_publish_drain_window_token();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_LIVE_BEFORE:
            return (u64)mmio64_ahci_publish_drain_live_before();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOKED_HANDLES:
            return (u64)mmio64_ahci_publish_drain_revoked_handles();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_LIVE_AFTER:
            return (u64)mmio64_ahci_publish_drain_live_after();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOCATION_REQUIRED:
            return (u64)mmio64_ahci_publish_drain_revocation_required();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOCATION_PLANNED:
            return (u64)mmio64_ahci_publish_drain_revocation_planned();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOCATION_EXECUTED:
            return (u64)mmio64_ahci_publish_drain_revocation_executed();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_WRITE_WINDOW_ENABLED:
            return (u64)mmio64_ahci_publish_drain_write_window_enabled();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_COMMIT_ALLOWED:
            return (u64)mmio64_ahci_publish_drain_commit_allowed();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_drain_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_drain_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_PUBLISHED:
            return (u64)mmio64_ahci_publish_drain_published();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_drain_command_issued();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_ARMED:
            return (u64)mmio64_ahci_publish_drain_armed();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_drain_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_drain_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_drain_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_DRIVER_HANDOFF:
            return (u64)mmio64_stage_ahci_publish_driver_handoff(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2),
                syscall64_pack_low32(arg2));

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_STATE:
            return (u64)mmio64_ahci_publish_handoff_state();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_FLAGS:
            return (u64)mmio64_ahci_publish_handoff_flags();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_TOKEN:
            return (u64)mmio64_ahci_publish_handoff_token();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRAIN_TOKEN:
            return (u64)mmio64_ahci_publish_handoff_drain_token();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_OLD_HANDLE:
            return (u64)mmio64_ahci_publish_handoff_old_handle();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_OWNER:
            return (u64)mmio64_ahci_publish_handoff_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_publish_handoff_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_LIVE_BEFORE:
            return (u64)mmio64_ahci_publish_handoff_live_before();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_STALE_OLD_DENIED:
            return (u64)mmio64_ahci_publish_handoff_stale_old_denied();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_PRINCIPAL_VALID:
            return (u64)mmio64_ahci_publish_handoff_driver_principal_valid();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_ROLE:
            return (u64)mmio64_ahci_publish_handoff_driver_role();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_OWNER_BOUND:
            return (u64)mmio64_ahci_publish_handoff_driver_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_QUERY_ONLY:
            return (u64)mmio64_ahci_publish_handoff_driver_query_only();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_LIVE_AFTER:
            return (u64)mmio64_ahci_publish_handoff_live_after();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_WRITE_WINDOW_ENABLED:
            return (u64)mmio64_ahci_publish_handoff_write_window_enabled();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_COMMIT_ALLOWED:
            return (u64)mmio64_ahci_publish_handoff_commit_allowed();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_MMIO_WRITTEN:
            return (u64)mmio64_ahci_publish_handoff_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_publish_handoff_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_PUBLISHED:
            return (u64)mmio64_ahci_publish_handoff_published();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_COMMAND_ISSUED:
            return (u64)mmio64_ahci_publish_handoff_command_issued();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_ARMED:
            return (u64)mmio64_ahci_publish_handoff_armed();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_STAGE_COUNT:
            return (u64)mmio64_ahci_publish_handoff_stage_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DENIAL_COUNT:
            return (u64)mmio64_ahci_publish_handoff_denial_count();

        case X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_publish_handoff_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_PROBE:
            return (u64)mmio64_stage_ahci_driver_read_probe(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_STATE:
            return (u64)mmio64_ahci_driver_probe_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_FLAGS:
            return (u64)mmio64_ahci_driver_probe_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_TOKEN:
            return (u64)mmio64_ahci_driver_probe_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_HANDOFF_TOKEN:
            return (u64)mmio64_ahci_driver_probe_handoff_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_probe_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_probe_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_probe_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_probe_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_CONTROLLER_CAP:
            return (u64)mmio64_ahci_driver_probe_controller_cap();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_CONTROLLER_GHC:
            return (u64)mmio64_ahci_driver_probe_controller_ghc();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_CONTROLLER_PI:
            return (u64)mmio64_ahci_driver_probe_controller_pi();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_CONTROLLER_VERSION:
            return (u64)mmio64_ahci_driver_probe_controller_version();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_SELECTED:
            return (u64)mmio64_ahci_driver_probe_port_selected();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_SSTS:
            return (u64)mmio64_ahci_driver_probe_port_ssts();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_SIGNATURE:
            return (u64)mmio64_ahci_driver_probe_port_signature();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_COMMAND:
            return (u64)mmio64_ahci_driver_probe_port_command();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_TASK_FILE:
            return (u64)mmio64_ahci_driver_probe_port_task_file();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_CI:
            return (u64)mmio64_ahci_driver_probe_port_ci();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_SERR:
            return (u64)mmio64_ahci_driver_probe_port_serr();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_POLICY_KIND:
            return (u64)mmio64_ahci_driver_probe_policy_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_POLICY_READY:
            return (u64)mmio64_ahci_driver_probe_policy_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_POLICY_BUSY:
            return (u64)mmio64_ahci_driver_probe_policy_busy();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_POLICY_DRQ:
            return (u64)mmio64_ahci_driver_probe_policy_drq();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_POLICY_CI_IDLE:
            return (u64)mmio64_ahci_driver_probe_policy_ci_idle();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_POLICY_SERR_CLEAR:
            return (u64)mmio64_ahci_driver_probe_policy_serr_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_probe_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_READ_LBA:
            return (u64)mmio64_ahci_driver_probe_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_probe_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_READ_BYTES:
            return (u64)mmio64_ahci_driver_probe_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_probe_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_probe_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_PUBLISHED:
            return (u64)mmio64_ahci_driver_probe_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_probe_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_probe_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_ARMED:
            return (u64)mmio64_ahci_driver_probe_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_probe_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_probe_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_probe_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_INTENT:
            return (u64)mmio64_stage_ahci_driver_read_intent(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_STATE:
            return (u64)mmio64_ahci_driver_intent_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_FLAGS:
            return (u64)mmio64_ahci_driver_intent_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_TOKEN:
            return (u64)mmio64_ahci_driver_intent_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_PROBE_TOKEN:
            return (u64)mmio64_ahci_driver_intent_probe_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_intent_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_intent_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_intent_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_intent_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_PORT:
            return (u64)mmio64_ahci_driver_intent_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_KIND:
            return (u64)mmio64_ahci_driver_intent_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_intent_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_READ_LBA:
            return (u64)mmio64_ahci_driver_intent_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_intent_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_READ_BYTES:
            return (u64)mmio64_ahci_driver_intent_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_READ_READY:
            return (u64)mmio64_ahci_driver_intent_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_intent_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_intent_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_PUBLISHED:
            return (u64)mmio64_ahci_driver_intent_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_intent_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_intent_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_ARMED:
            return (u64)mmio64_ahci_driver_intent_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_intent_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_intent_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_intent_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_intent_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_BUFFER:
            return (u64)mmio64_stage_ahci_driver_read_buffer(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_STATE:
            return (u64)mmio64_ahci_driver_buffer_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_FLAGS:
            return (u64)mmio64_ahci_driver_buffer_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_TOKEN:
            return (u64)mmio64_ahci_driver_buffer_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_INTENT_TOKEN:
            return (u64)mmio64_ahci_driver_buffer_intent_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_buffer_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_buffer_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_buffer_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_buffer_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_PORT:
            return (u64)mmio64_ahci_driver_buffer_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_KIND:
            return (u64)mmio64_ahci_driver_buffer_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_READ_OPERATION:
            return (u64)mmio64_ahci_driver_buffer_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_READ_LBA:
            return (u64)mmio64_ahci_driver_buffer_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_buffer_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_READ_BYTES:
            return (u64)mmio64_ahci_driver_buffer_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_buffer_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_OFFSET:
            return (u64)mmio64_ahci_driver_buffer_offset();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_READ_READY:
            return (u64)mmio64_ahci_driver_buffer_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_buffer_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_buffer_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_PUBLISHED:
            return (u64)mmio64_ahci_driver_buffer_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_buffer_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_buffer_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_ARMED:
            return (u64)mmio64_ahci_driver_buffer_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_MEDIA_READ:
            return (u64)mmio64_ahci_driver_buffer_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_buffer_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_buffer_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_buffer_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_GATE:
            return (u64)mmio64_stage_ahci_driver_read_gate(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_STATE:
            return (u64)mmio64_ahci_driver_gate_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_FLAGS:
            return (u64)mmio64_ahci_driver_gate_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_TOKEN:
            return (u64)mmio64_ahci_driver_gate_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_BUFFER_TOKEN:
            return (u64)mmio64_ahci_driver_gate_buffer_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_gate_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_gate_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_gate_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_gate_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_PORT:
            return (u64)mmio64_ahci_driver_gate_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_KIND:
            return (u64)mmio64_ahci_driver_gate_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_gate_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_READ_LBA:
            return (u64)mmio64_ahci_driver_gate_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_gate_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_READ_BYTES:
            return (u64)mmio64_ahci_driver_gate_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_gate_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_gate_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_gate_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_READ_READY:
            return (u64)mmio64_ahci_driver_gate_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_EXECUTION_REQUIRED:
            return (u64)mmio64_ahci_driver_gate_execution_required();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_EXECUTION_GRANTED:
            return (u64)mmio64_ahci_driver_gate_execution_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_ISSUE_ALLOWED:
            return (u64)mmio64_ahci_driver_gate_issue_allowed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_gate_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_gate_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_PUBLISHED:
            return (u64)mmio64_ahci_driver_gate_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_gate_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_gate_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_ARMED:
            return (u64)mmio64_ahci_driver_gate_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_gate_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_gate_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_gate_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_gate_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_EXECUTE:
            return (u64)mmio64_stage_ahci_driver_read_execute(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_STATE:
            return (u64)mmio64_ahci_driver_exec_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_FLAGS:
            return (u64)mmio64_ahci_driver_exec_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_TOKEN:
            return (u64)mmio64_ahci_driver_exec_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_GATE_TOKEN:
            return (u64)mmio64_ahci_driver_exec_gate_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_exec_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_exec_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_exec_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_exec_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_PORT:
            return (u64)mmio64_ahci_driver_exec_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_KIND:
            return (u64)mmio64_ahci_driver_exec_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_READ_OPERATION:
            return (u64)mmio64_ahci_driver_exec_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_READ_LBA:
            return (u64)mmio64_ahci_driver_exec_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_exec_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_READ_BYTES:
            return (u64)mmio64_ahci_driver_exec_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_exec_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_exec_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_exec_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_READ_READY:
            return (u64)mmio64_ahci_driver_exec_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_EXECUTION_ATTEMPTED:
            return (u64)mmio64_ahci_driver_exec_execution_attempted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_EXECUTION_REQUIRED:
            return (u64)mmio64_ahci_driver_exec_execution_required();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_EXECUTION_GRANTED:
            return (u64)mmio64_ahci_driver_exec_execution_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_ISSUE_ALLOWED:
            return (u64)mmio64_ahci_driver_exec_issue_allowed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_ISSUE_DENIED:
            return (u64)mmio64_ahci_driver_exec_issue_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_exec_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_exec_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_PUBLISHED:
            return (u64)mmio64_ahci_driver_exec_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_exec_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_exec_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_ARMED:
            return (u64)mmio64_ahci_driver_exec_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_MEDIA_READ:
            return (u64)mmio64_ahci_driver_exec_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_exec_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_exec_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_exec_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RESULT:
            return (u64)mmio64_stage_ahci_driver_read_result(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_STATE:
            return (u64)mmio64_ahci_driver_result_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_FLAGS:
            return (u64)mmio64_ahci_driver_result_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_TOKEN:
            return (u64)mmio64_ahci_driver_result_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_EXEC_TOKEN:
            return (u64)mmio64_ahci_driver_result_exec_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_result_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_result_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_result_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_result_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_PORT:
            return (u64)mmio64_ahci_driver_result_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_KIND:
            return (u64)mmio64_ahci_driver_result_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_result_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_READ_LBA:
            return (u64)mmio64_ahci_driver_result_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_result_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_READ_BYTES:
            return (u64)mmio64_ahci_driver_result_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_result_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_result_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_result_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_READ_READY:
            return (u64)mmio64_ahci_driver_result_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_EXECUTION_DENIED:
            return (u64)mmio64_ahci_driver_result_execution_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_RESULT_REQUESTED:
            return (u64)mmio64_ahci_driver_result_result_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_RESULT_GRANTED:
            return (u64)mmio64_ahci_driver_result_result_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_result_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_result_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_result_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_FS_MINTED:
            return (u64)mmio64_ahci_driver_result_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_result_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_result_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_PUBLISHED:
            return (u64)mmio64_ahci_driver_result_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_result_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_result_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_ARMED:
            return (u64)mmio64_ahci_driver_result_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_result_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_result_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_result_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_result_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_BLOCK_PUBLISH:
            return (u64)mmio64_stage_ahci_driver_block_publish(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_STATE:
            return (u64)mmio64_ahci_driver_publish_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_FLAGS:
            return (u64)mmio64_ahci_driver_publish_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_TOKEN:
            return (u64)mmio64_ahci_driver_publish_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_RESULT_TOKEN:
            return (u64)mmio64_ahci_driver_publish_result_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_publish_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_publish_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_publish_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_publish_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_PORT:
            return (u64)mmio64_ahci_driver_publish_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_KIND:
            return (u64)mmio64_ahci_driver_publish_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_READ_OPERATION:
            return (u64)mmio64_ahci_driver_publish_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_READ_LBA:
            return (u64)mmio64_ahci_driver_publish_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_publish_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_READ_BYTES:
            return (u64)mmio64_ahci_driver_publish_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_publish_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_publish_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_publish_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_READ_READY:
            return (u64)mmio64_ahci_driver_publish_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_EXECUTION_DENIED:
            return (u64)mmio64_ahci_driver_publish_execution_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_publish_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_publish_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_PUBLISH_REQUESTED:
            return (u64)mmio64_ahci_driver_publish_publish_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_PUBLISH_GRANTED:
            return (u64)mmio64_ahci_driver_publish_publish_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_PUBLISH_DENIED:
            return (u64)mmio64_ahci_driver_publish_publish_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_publish_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_publish_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_FS_MINTED:
            return (u64)mmio64_ahci_driver_publish_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_publish_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_publish_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_PUBLISHED:
            return (u64)mmio64_ahci_driver_publish_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_publish_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_publish_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_ARMED:
            return (u64)mmio64_ahci_driver_publish_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_MEDIA_READ:
            return (u64)mmio64_ahci_driver_publish_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_publish_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_publish_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_publish_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_publish_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_GRANT:
            return (u64)mmio64_stage_ahci_driver_read_grant(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_STATE:
            return (u64)mmio64_ahci_driver_read_grant_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_FLAGS:
            return (u64)mmio64_ahci_driver_read_grant_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_TOKEN:
            return (u64)mmio64_ahci_driver_read_grant_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_PUBLISH_TOKEN:
            return (u64)mmio64_ahci_driver_read_grant_publish_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_RESULT_TOKEN:
            return (u64)mmio64_ahci_driver_read_grant_result_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_grant_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_grant_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_grant_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_grant_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_PORT:
            return (u64)mmio64_ahci_driver_read_grant_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_KIND:
            return (u64)mmio64_ahci_driver_read_grant_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_grant_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_grant_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_grant_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_grant_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_grant_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_grant_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_grant_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_READY:
            return (u64)mmio64_ahci_driver_read_grant_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_EXECUTION_DENIED:
            return (u64)mmio64_ahci_driver_read_grant_execution_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_read_grant_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_PUBLISH_DENIED:
            return (u64)mmio64_ahci_driver_read_grant_publish_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_grant_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_GRANT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_grant_read_grant_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_GRANT_GRANTED:
            return (u64)mmio64_ahci_driver_read_grant_read_grant_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_READ_GRANT_DENIED:
            return (u64)mmio64_ahci_driver_read_grant_read_grant_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_grant_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_grant_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_grant_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_grant_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_grant_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_grant_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_grant_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_grant_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_grant_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_ARMED:
            return (u64)mmio64_ahci_driver_read_grant_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_grant_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_grant_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_grant_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_grant_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_grant_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_MEDIA_READ:
            return (u64)mmio64_stage_ahci_driver_media_read(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_STATE:
            return (u64)mmio64_ahci_driver_media_read_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_FLAGS:
            return (u64)mmio64_ahci_driver_media_read_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_TOKEN:
            return (u64)mmio64_ahci_driver_media_read_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_GRANT_TOKEN:
            return (u64)mmio64_ahci_driver_media_read_read_grant_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_media_read_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_media_read_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_media_read_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_media_read_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_PORT:
            return (u64)mmio64_ahci_driver_media_read_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_KIND:
            return (u64)mmio64_ahci_driver_media_read_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_OPERATION:
            return (u64)mmio64_ahci_driver_media_read_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_LBA:
            return (u64)mmio64_ahci_driver_media_read_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_media_read_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_BYTES:
            return (u64)mmio64_ahci_driver_media_read_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_media_read_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_media_read_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_media_read_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_READY:
            return (u64)mmio64_ahci_driver_media_read_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_GRANT_DENIED:
            return (u64)mmio64_ahci_driver_media_read_read_grant_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_media_read_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_ATTEMPTED:
            return (u64)mmio64_ahci_driver_media_read_read_attempted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_READ_DENIED:
            return (u64)mmio64_ahci_driver_media_read_read_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_media_read_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_media_read_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_media_read_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_FS_MINTED:
            return (u64)mmio64_ahci_driver_media_read_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_media_read_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_media_read_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_PUBLISHED:
            return (u64)mmio64_ahci_driver_media_read_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_media_read_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_media_read_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_ARMED:
            return (u64)mmio64_ahci_driver_media_read_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_MEDIA_READ:
            return (u64)mmio64_ahci_driver_media_read_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_media_read_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_media_read_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_media_read_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_media_read_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_media_read_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_COMPLETE:
            return (u64)mmio64_stage_ahci_driver_complete(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_STATE:
            return (u64)mmio64_ahci_driver_complete_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_FLAGS:
            return (u64)mmio64_ahci_driver_complete_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_TOKEN:
            return (u64)mmio64_ahci_driver_complete_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_MEDIA_READ_TOKEN:
            return (u64)mmio64_ahci_driver_complete_media_read_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_complete_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_complete_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_complete_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_complete_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_PORT:
            return (u64)mmio64_ahci_driver_complete_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_KIND:
            return (u64)mmio64_ahci_driver_complete_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_complete_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_READ_LBA:
            return (u64)mmio64_ahci_driver_complete_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_complete_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_READ_BYTES:
            return (u64)mmio64_ahci_driver_complete_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_complete_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_complete_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_complete_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_READ_READY:
            return (u64)mmio64_ahci_driver_complete_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_MEDIA_READ_DENIED:
            return (u64)mmio64_ahci_driver_complete_media_read_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_COMPLETION_REQUESTED:
            return (u64)mmio64_ahci_driver_complete_completion_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_COMPLETION_GRANTED:
            return (u64)mmio64_ahci_driver_complete_completion_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_COMPLETION_DENIED:
            return (u64)mmio64_ahci_driver_complete_completion_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_COMPLETED:
            return (u64)mmio64_ahci_driver_complete_completed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_STATUS:
            return (u64)mmio64_ahci_driver_complete_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_complete_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_complete_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_complete_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_FS_MINTED:
            return (u64)mmio64_ahci_driver_complete_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_complete_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_complete_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_PUBLISHED:
            return (u64)mmio64_ahci_driver_complete_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_complete_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_complete_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_ARMED:
            return (u64)mmio64_ahci_driver_complete_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_complete_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_complete_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_complete_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_complete_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_complete_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_complete_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_CAP:
            return (u64)mmio64_stage_ahci_driver_read_cap(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_STATE:
            return (u64)mmio64_ahci_driver_read_cap_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_FLAGS:
            return (u64)mmio64_ahci_driver_read_cap_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_TOKEN:
            return (u64)mmio64_ahci_driver_read_cap_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_COMPLETE_TOKEN:
            return (u64)mmio64_ahci_driver_read_cap_complete_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_cap_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_cap_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_cap_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_cap_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_PORT:
            return (u64)mmio64_ahci_driver_read_cap_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_KIND:
            return (u64)mmio64_ahci_driver_read_cap_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_cap_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_READ_LBA:
            return (u64)mmio64_ahci_driver_read_cap_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_cap_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_cap_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_cap_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_cap_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_cap_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_READ_READY:
            return (u64)mmio64_ahci_driver_read_cap_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_COMPLETE_DENIED:
            return (u64)mmio64_ahci_driver_read_cap_complete_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_REQUESTED:
            return (u64)mmio64_ahci_driver_read_cap_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_GRANTED:
            return (u64)mmio64_ahci_driver_read_cap_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_DENIED:
            return (u64)mmio64_ahci_driver_read_cap_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_cap_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_cap_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_cap_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_cap_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_cap_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_cap_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_cap_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_cap_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_cap_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_ARMED:
            return (u64)mmio64_ahci_driver_read_cap_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_cap_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_cap_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_cap_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_cap_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_cap_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_cap_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_EXPORT:
            return (u64)mmio64_stage_ahci_driver_read_export(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_STATE:
            return (u64)mmio64_ahci_driver_read_export_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_FLAGS:
            return (u64)mmio64_ahci_driver_read_export_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_TOKEN:
            return (u64)mmio64_ahci_driver_read_export_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_READ_CAP_TOKEN:
            return (u64)mmio64_ahci_driver_read_export_read_cap_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_export_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_export_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_export_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_export_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_PORT:
            return (u64)mmio64_ahci_driver_read_export_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_KIND:
            return (u64)mmio64_ahci_driver_read_export_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_export_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_export_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_export_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_export_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_export_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_export_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_export_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_READ_READY:
            return (u64)mmio64_ahci_driver_read_export_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_READ_CAP_DENIED:
            return (u64)mmio64_ahci_driver_read_export_read_cap_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_export_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_GRANTED:
            return (u64)mmio64_ahci_driver_read_export_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_DENIED:
            return (u64)mmio64_ahci_driver_read_export_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_export_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_USER_BYTES_COPIED:
            return (u64)mmio64_ahci_driver_read_export_user_bytes_copied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_USER_BUFFER_WRITTEN:
            return (u64)mmio64_ahci_driver_read_export_user_buffer_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_export_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_export_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_export_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_export_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_export_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_export_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_export_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_export_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_ARMED:
            return (u64)mmio64_ahci_driver_read_export_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_export_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_export_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_export_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_export_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_export_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_export_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RESPONSE:
            return (u64)mmio64_stage_ahci_driver_read_response(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_STATE:
            return (u64)mmio64_ahci_driver_read_response_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_FLAGS:
            return (u64)mmio64_ahci_driver_read_response_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_TOKEN:
            return (u64)mmio64_ahci_driver_read_response_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_READ_EXPORT_TOKEN:
            return (u64)mmio64_ahci_driver_read_response_read_export_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_response_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_response_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_response_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_response_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_PORT:
            return (u64)mmio64_ahci_driver_read_response_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_KIND:
            return (u64)mmio64_ahci_driver_read_response_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_response_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_response_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_response_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_response_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_response_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_response_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_response_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_READ_READY:
            return (u64)mmio64_ahci_driver_read_response_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_READ_EXPORT_DENIED:
            return (u64)mmio64_ahci_driver_read_response_read_export_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_response_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_GRANTED:
            return (u64)mmio64_ahci_driver_read_response_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_DENIED:
            return (u64)mmio64_ahci_driver_read_response_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_response_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_RESPONSE_BYTES:
            return (u64)mmio64_ahci_driver_read_response_response_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_RESPONSE_STATUS:
            return (u64)mmio64_ahci_driver_read_response_response_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_RESPONSE_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_response_response_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_response_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_response_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_response_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_response_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_response_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_response_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_response_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_response_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_ARMED:
            return (u64)mmio64_ahci_driver_read_response_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_response_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_response_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_response_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_response_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_response_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_response_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DELIVERY:
            return (u64)mmio64_stage_ahci_driver_read_delivery(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_STATE:
            return (u64)mmio64_ahci_driver_read_delivery_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_FLAGS:
            return (u64)mmio64_ahci_driver_read_delivery_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_TOKEN:
            return (u64)mmio64_ahci_driver_read_delivery_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_READ_RESPONSE_TOKEN:
            return (u64)mmio64_ahci_driver_read_delivery_read_response_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_delivery_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_delivery_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_delivery_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_delivery_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_PORT:
            return (u64)mmio64_ahci_driver_read_delivery_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_KIND:
            return (u64)mmio64_ahci_driver_read_delivery_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_delivery_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_READ_LBA:
            return (u64)mmio64_ahci_driver_read_delivery_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_delivery_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_delivery_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_delivery_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_delivery_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_delivery_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_READ_READY:
            return (u64)mmio64_ahci_driver_read_delivery_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_READ_RESPONSE_DENIED:
            return (u64)mmio64_ahci_driver_read_delivery_read_response_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_REQUESTED:
            return (u64)mmio64_ahci_driver_read_delivery_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_GRANTED:
            return (u64)mmio64_ahci_driver_read_delivery_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DENIED:
            return (u64)mmio64_ahci_driver_read_delivery_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_delivery_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DELIVERED_BYTES:
            return (u64)mmio64_ahci_driver_read_delivery_delivered_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DELIVERY_STATUS:
            return (u64)mmio64_ahci_driver_read_delivery_delivery_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DELIVERY_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_delivery_delivery_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_delivery_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_delivery_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_delivery_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_delivery_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_delivery_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_delivery_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_delivery_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_delivery_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_ARMED:
            return (u64)mmio64_ahci_driver_read_delivery_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_delivery_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_delivery_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_delivery_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_delivery_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_delivery_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_delivery_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_VISIBLE:
            return (u64)mmio64_stage_ahci_driver_read_visible(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_STATE:
            return (u64)mmio64_ahci_driver_read_visible_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_FLAGS:
            return (u64)mmio64_ahci_driver_read_visible_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_visible_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_READ_DELIVERY_TOKEN:
            return (u64)mmio64_ahci_driver_read_visible_read_delivery_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_visible_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_visible_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_visible_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_visible_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_PORT:
            return (u64)mmio64_ahci_driver_read_visible_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_KIND:
            return (u64)mmio64_ahci_driver_read_visible_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_visible_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_visible_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_visible_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_visible_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_visible_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_visible_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_visible_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_READ_READY:
            return (u64)mmio64_ahci_driver_read_visible_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_READ_DELIVERY_DENIED:
            return (u64)mmio64_ahci_driver_read_visible_read_delivery_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_visible_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_GRANTED:
            return (u64)mmio64_ahci_driver_read_visible_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_DENIED:
            return (u64)mmio64_ahci_driver_read_visible_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_visible_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_VISIBLE_BYTES:
            return (u64)mmio64_ahci_driver_read_visible_visible_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_VISIBILITY_STATUS:
            return (u64)mmio64_ahci_driver_read_visible_visibility_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_VISIBILITY_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_visible_visibility_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_visible_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_visible_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_visible_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_visible_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_visible_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_visible_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_visible_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_visible_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_ARMED:
            return (u64)mmio64_ahci_driver_read_visible_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_visible_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_visible_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_visible_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_visible_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_visible_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_visible_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_COMMIT:
            return (u64)mmio64_stage_ahci_driver_read_commit(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_STATE:
            return (u64)mmio64_ahci_driver_read_commit_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_FLAGS:
            return (u64)mmio64_ahci_driver_read_commit_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_commit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_READ_VISIBLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_commit_read_visible_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_commit_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_commit_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_commit_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_commit_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_PORT:
            return (u64)mmio64_ahci_driver_read_commit_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_KIND:
            return (u64)mmio64_ahci_driver_read_commit_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_commit_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_commit_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_commit_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_commit_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_commit_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_commit_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_commit_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_READ_READY:
            return (u64)mmio64_ahci_driver_read_commit_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_READ_VISIBLE_DENIED:
            return (u64)mmio64_ahci_driver_read_commit_read_visible_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_commit_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_GRANTED:
            return (u64)mmio64_ahci_driver_read_commit_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_DENIED:
            return (u64)mmio64_ahci_driver_read_commit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_commit_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_COMMITTED_BYTES:
            return (u64)mmio64_ahci_driver_read_commit_committed_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_COMMIT_STATUS:
            return (u64)mmio64_ahci_driver_read_commit_commit_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_COMMIT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_commit_commit_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_commit_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_commit_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_commit_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_commit_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_commit_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_commit_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_commit_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_commit_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_ARMED:
            return (u64)mmio64_ahci_driver_read_commit_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_commit_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_commit_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_commit_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_commit_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_commit_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_commit_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_AUDIT:
            return (u64)mmio64_stage_ahci_driver_read_audit(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_STATE:
            return (u64)mmio64_ahci_driver_read_audit_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_FLAGS:
            return (u64)mmio64_ahci_driver_read_audit_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_audit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_READ_COMMIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_audit_read_commit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_audit_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_audit_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_audit_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_audit_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_PORT:
            return (u64)mmio64_ahci_driver_read_audit_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_KIND:
            return (u64)mmio64_ahci_driver_read_audit_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_audit_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_audit_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_audit_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_audit_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_audit_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_audit_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_audit_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_READ_READY:
            return (u64)mmio64_ahci_driver_read_audit_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_READ_COMMIT_DENIED:
            return (u64)mmio64_ahci_driver_read_audit_read_commit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_audit_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_GRANTED:
            return (u64)mmio64_ahci_driver_read_audit_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_DENIED:
            return (u64)mmio64_ahci_driver_read_audit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_audit_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_AUDITED_BYTES:
            return (u64)mmio64_ahci_driver_read_audit_audited_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_AUDIT_STATUS:
            return (u64)mmio64_ahci_driver_read_audit_audit_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_AUDIT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_audit_audit_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_audit_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_audit_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_audit_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_audit_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_audit_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_audit_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_audit_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_audit_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_ARMED:
            return (u64)mmio64_ahci_driver_read_audit_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_audit_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_audit_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_audit_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_audit_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_audit_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_audit_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_UPGRADE:
            return (u64)mmio64_stage_ahci_driver_read_upgrade(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_STATE:
            return (u64)mmio64_ahci_driver_read_upgrade_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_FLAGS:
            return (u64)mmio64_ahci_driver_read_upgrade_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_TOKEN:
            return (u64)mmio64_ahci_driver_read_upgrade_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_READ_AUDIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_upgrade_read_audit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_upgrade_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_upgrade_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_upgrade_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_upgrade_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_PORT:
            return (u64)mmio64_ahci_driver_read_upgrade_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_KIND:
            return (u64)mmio64_ahci_driver_read_upgrade_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_upgrade_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_upgrade_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_upgrade_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_upgrade_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_upgrade_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_upgrade_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_upgrade_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_READ_READY:
            return (u64)mmio64_ahci_driver_read_upgrade_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_READ_AUDIT_DENIED:
            return (u64)mmio64_ahci_driver_read_upgrade_read_audit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_upgrade_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_GRANTED:
            return (u64)mmio64_ahci_driver_read_upgrade_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_DENIED:
            return (u64)mmio64_ahci_driver_read_upgrade_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_upgrade_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_UPGRADED_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_upgrade_upgraded_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_MEDIA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_upgrade_media_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_upgrade_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_upgrade_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_upgrade_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_upgrade_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_upgrade_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_upgrade_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_upgrade_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_upgrade_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_upgrade_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_ARMED:
            return (u64)mmio64_ahci_driver_read_upgrade_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_upgrade_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_upgrade_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_upgrade_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_upgrade_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_upgrade_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_upgrade_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ACTIVATE:
            return (u64)mmio64_stage_ahci_driver_read_activate(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_STATE:
            return (u64)mmio64_ahci_driver_read_activate_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_FLAGS:
            return (u64)mmio64_ahci_driver_read_activate_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_TOKEN:
            return (u64)mmio64_ahci_driver_read_activate_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_UPGRADE_TOKEN:
            return (u64)mmio64_ahci_driver_read_activate_read_upgrade_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_activate_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_activate_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_activate_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_activate_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_PORT:
            return (u64)mmio64_ahci_driver_read_activate_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_KIND:
            return (u64)mmio64_ahci_driver_read_activate_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_activate_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_activate_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_activate_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_activate_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_activate_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_activate_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_activate_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_READY:
            return (u64)mmio64_ahci_driver_read_activate_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_UPGRADE_DENIED:
            return (u64)mmio64_ahci_driver_read_activate_read_upgrade_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_activate_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_GRANTED:
            return (u64)mmio64_ahci_driver_read_activate_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_DENIED:
            return (u64)mmio64_ahci_driver_read_activate_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_activate_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_ACTIVATED_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_activate_activated_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_activate_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_activate_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_activate_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_activate_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_activate_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_activate_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_activate_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_activate_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_activate_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_activate_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_ARMED:
            return (u64)mmio64_ahci_driver_read_activate_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_activate_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_activate_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_activate_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_activate_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_activate_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_activate_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ARM:
            return (u64)mmio64_stage_ahci_driver_read_arm(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_STATE:
            return (u64)mmio64_ahci_driver_read_arm_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_FLAGS:
            return (u64)mmio64_ahci_driver_read_arm_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_TOKEN:
            return (u64)mmio64_ahci_driver_read_arm_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_ACTIVATE_TOKEN:
            return (u64)mmio64_ahci_driver_read_arm_read_activate_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_arm_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_arm_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_arm_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_arm_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_PORT:
            return (u64)mmio64_ahci_driver_read_arm_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_KIND:
            return (u64)mmio64_ahci_driver_read_arm_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_arm_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_LBA:
            return (u64)mmio64_ahci_driver_read_arm_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_arm_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_arm_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_arm_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_arm_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_arm_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_READY:
            return (u64)mmio64_ahci_driver_read_arm_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_ACTIVATE_DENIED:
            return (u64)mmio64_ahci_driver_read_arm_read_activate_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_REQUESTED:
            return (u64)mmio64_ahci_driver_read_arm_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_GRANTED:
            return (u64)mmio64_ahci_driver_read_arm_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_DENIED:
            return (u64)mmio64_ahci_driver_read_arm_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_arm_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_ARMED_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_arm_armed_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_arm_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_arm_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_arm_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_arm_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_arm_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_arm_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_arm_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_arm_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_arm_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_arm_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_ARMED:
            return (u64)mmio64_ahci_driver_read_arm_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_arm_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_arm_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_arm_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_arm_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_arm_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_arm_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SUBMIT:
            return (u64)mmio64_stage_ahci_driver_read_submit(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_STATE:
            return (u64)mmio64_ahci_driver_read_submit_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_FLAGS:
            return (u64)mmio64_ahci_driver_read_submit_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_submit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_ARM_TOKEN:
            return (u64)mmio64_ahci_driver_read_submit_read_arm_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_submit_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_submit_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_submit_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_submit_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_PORT:
            return (u64)mmio64_ahci_driver_read_submit_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_KIND:
            return (u64)mmio64_ahci_driver_read_submit_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_submit_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_submit_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_submit_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_submit_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_submit_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_submit_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_submit_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_READY:
            return (u64)mmio64_ahci_driver_read_submit_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_ARM_DENIED:
            return (u64)mmio64_ahci_driver_read_submit_read_arm_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_submit_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_GRANTED:
            return (u64)mmio64_ahci_driver_read_submit_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_DENIED:
            return (u64)mmio64_ahci_driver_read_submit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_submit_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_SUBMITTED_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_submit_submitted_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_submit_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_submit_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_submit_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_submit_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_submit_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_submit_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_submit_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_submit_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_submit_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_submit_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_ARMED:
            return (u64)mmio64_ahci_driver_read_submit_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_submit_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_submit_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_submit_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_submit_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_submit_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_submit_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_OBSERVE:
            return (u64)mmio64_stage_ahci_driver_read_observe(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_STATE:
            return (u64)mmio64_ahci_driver_read_observe_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_FLAGS:
            return (u64)mmio64_ahci_driver_read_observe_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_TOKEN:
            return (u64)mmio64_ahci_driver_read_observe_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_SUBMIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_observe_read_submit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_observe_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_observe_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_observe_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_observe_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_PORT:
            return (u64)mmio64_ahci_driver_read_observe_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_KIND:
            return (u64)mmio64_ahci_driver_read_observe_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_observe_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_observe_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_observe_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_observe_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_observe_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_observe_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_observe_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_READY:
            return (u64)mmio64_ahci_driver_read_observe_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_SUBMIT_DENIED:
            return (u64)mmio64_ahci_driver_read_observe_read_submit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_observe_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_GRANTED:
            return (u64)mmio64_ahci_driver_read_observe_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_DENIED:
            return (u64)mmio64_ahci_driver_read_observe_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_observe_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_OBSERVED_STATUS:
            return (u64)mmio64_ahci_driver_read_observe_observed_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_OBSERVED_BYTES:
            return (u64)mmio64_ahci_driver_read_observe_observed_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_OBSERVED_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_observe_observed_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_observe_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_observe_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_observe_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_observe_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_observe_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_observe_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_observe_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_observe_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_observe_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_observe_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_ARMED:
            return (u64)mmio64_ahci_driver_read_observe_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_observe_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_observe_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_observe_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_observe_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_observe_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_observe_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RETIRE:
            return (u64)mmio64_stage_ahci_driver_read_retire(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_STATE:
            return (u64)mmio64_ahci_driver_read_retire_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_FLAGS:
            return (u64)mmio64_ahci_driver_read_retire_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_TOKEN:
            return (u64)mmio64_ahci_driver_read_retire_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_OBSERVE_TOKEN:
            return (u64)mmio64_ahci_driver_read_retire_read_observe_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_retire_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_retire_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_retire_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_retire_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_PORT:
            return (u64)mmio64_ahci_driver_read_retire_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_KIND:
            return (u64)mmio64_ahci_driver_read_retire_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_retire_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_retire_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_retire_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_retire_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_retire_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_retire_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_retire_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_READY:
            return (u64)mmio64_ahci_driver_read_retire_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_OBSERVE_DENIED:
            return (u64)mmio64_ahci_driver_read_retire_read_observe_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_retire_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_GRANTED:
            return (u64)mmio64_ahci_driver_read_retire_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_DENIED:
            return (u64)mmio64_ahci_driver_read_retire_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_retire_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_RETIRED_STATUS:
            return (u64)mmio64_ahci_driver_read_retire_retired_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_RETIRED_BYTES:
            return (u64)mmio64_ahci_driver_read_retire_retired_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_RETIRED_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_retire_retired_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_retire_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_retire_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_retire_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_retire_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_retire_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_retire_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_retire_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_retire_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_retire_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_retire_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_ARMED:
            return (u64)mmio64_ahci_driver_read_retire_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_retire_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_retire_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_retire_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_retire_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_retire_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_retire_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_PERMIT:
            return (u64)mmio64_stage_ahci_driver_read_permit(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_STATE:
            return (u64)mmio64_ahci_driver_read_permit_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_FLAGS:
            return (u64)mmio64_ahci_driver_read_permit_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_permit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_RETIRE_TOKEN:
            return (u64)mmio64_ahci_driver_read_permit_read_retire_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_permit_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_permit_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_permit_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_permit_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_PORT:
            return (u64)mmio64_ahci_driver_read_permit_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_KIND:
            return (u64)mmio64_ahci_driver_read_permit_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_permit_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_permit_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_permit_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_permit_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_permit_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_permit_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_permit_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_READY:
            return (u64)mmio64_ahci_driver_read_permit_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_RETIRE_DENIED:
            return (u64)mmio64_ahci_driver_read_permit_read_retire_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_permit_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_GRANTED:
            return (u64)mmio64_ahci_driver_read_permit_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_DENIED:
            return (u64)mmio64_ahci_driver_read_permit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_permit_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_PERMIT_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_permit_permit_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_permit_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_permit_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_permit_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_permit_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_permit_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_permit_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_permit_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_permit_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_permit_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_permit_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_ARMED:
            return (u64)mmio64_ahci_driver_read_permit_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_permit_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_permit_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_permit_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_permit_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_permit_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_permit_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_WINDOW:
            return (u64)mmio64_stage_ahci_driver_read_window(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_STATE:
            return (u64)mmio64_ahci_driver_read_window_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_FLAGS:
            return (u64)mmio64_ahci_driver_read_window_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_TOKEN:
            return (u64)mmio64_ahci_driver_read_window_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_PERMIT_TOKEN:
            return (u64)mmio64_ahci_driver_read_window_read_permit_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_window_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_window_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_window_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_window_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_PORT:
            return (u64)mmio64_ahci_driver_read_window_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_KIND:
            return (u64)mmio64_ahci_driver_read_window_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_window_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_LBA:
            return (u64)mmio64_ahci_driver_read_window_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_window_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_window_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_window_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_window_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_window_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_READY:
            return (u64)mmio64_ahci_driver_read_window_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_PERMIT_DENIED:
            return (u64)mmio64_ahci_driver_read_window_read_permit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_REQUESTED:
            return (u64)mmio64_ahci_driver_read_window_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_GRANTED:
            return (u64)mmio64_ahci_driver_read_window_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_DENIED:
            return (u64)mmio64_ahci_driver_read_window_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_window_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_WINDOW_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_window_window_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_OPEN:
            return (u64)mmio64_ahci_driver_read_window_open();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_window_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_window_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_window_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_window_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_window_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_window_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_window_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_window_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_window_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_window_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_ARMED:
            return (u64)mmio64_ahci_driver_read_window_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_window_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_window_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_window_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_window_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_window_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_window_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_LEASE:
            return (u64)mmio64_stage_ahci_driver_read_lease(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_STATE:
            return (u64)mmio64_ahci_driver_read_lease_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_FLAGS:
            return (u64)mmio64_ahci_driver_read_lease_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_TOKEN:
            return (u64)mmio64_ahci_driver_read_lease_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_WINDOW_TOKEN:
            return (u64)mmio64_ahci_driver_read_lease_read_window_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_lease_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_lease_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_lease_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_lease_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_PORT:
            return (u64)mmio64_ahci_driver_read_lease_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_KIND:
            return (u64)mmio64_ahci_driver_read_lease_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_lease_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_lease_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_lease_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_lease_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_lease_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_lease_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_lease_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_READY:
            return (u64)mmio64_ahci_driver_read_lease_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_WINDOW_DENIED:
            return (u64)mmio64_ahci_driver_read_lease_read_window_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_lease_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_GRANTED:
            return (u64)mmio64_ahci_driver_read_lease_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_DENIED:
            return (u64)mmio64_ahci_driver_read_lease_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_lease_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_LEASE_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_lease_lease_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_ACTIVE:
            return (u64)mmio64_ahci_driver_read_lease_active();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_lease_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_lease_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_lease_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_lease_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_lease_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_lease_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_lease_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_lease_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_lease_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_lease_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_ARMED:
            return (u64)mmio64_ahci_driver_read_lease_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_lease_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_lease_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_lease_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_lease_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_lease_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_lease_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_USE:
            return (u64)mmio64_stage_ahci_driver_read_use(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_STATE:
            return (u64)mmio64_ahci_driver_read_use_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_FLAGS:
            return (u64)mmio64_ahci_driver_read_use_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_TOKEN:
            return (u64)mmio64_ahci_driver_read_use_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_LEASE_TOKEN:
            return (u64)mmio64_ahci_driver_read_use_read_lease_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_use_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_use_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_use_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_use_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_PORT:
            return (u64)mmio64_ahci_driver_read_use_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_KIND:
            return (u64)mmio64_ahci_driver_read_use_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_use_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_use_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_use_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_use_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_use_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_use_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_use_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_READY:
            return (u64)mmio64_ahci_driver_read_use_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_LEASE_DENIED:
            return (u64)mmio64_ahci_driver_read_use_read_lease_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_use_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_GRANTED:
            return (u64)mmio64_ahci_driver_read_use_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_DENIED:
            return (u64)mmio64_ahci_driver_read_use_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_use_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_USE_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_use_use_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_ACTIVE:
            return (u64)mmio64_ahci_driver_read_use_active();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_use_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_use_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_use_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_use_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_use_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_use_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_use_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_use_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_use_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_use_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_ARMED:
            return (u64)mmio64_ahci_driver_read_use_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_use_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_use_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_use_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_use_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_use_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_use_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_REPORT:
            return (u64)mmio64_stage_ahci_driver_read_report(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_STATE:
            return (u64)mmio64_ahci_driver_read_report_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_FLAGS:
            return (u64)mmio64_ahci_driver_read_report_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_TOKEN:
            return (u64)mmio64_ahci_driver_read_report_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_USE_TOKEN:
            return (u64)mmio64_ahci_driver_read_report_read_use_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_report_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_report_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_report_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_report_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_PORT:
            return (u64)mmio64_ahci_driver_read_report_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_KIND:
            return (u64)mmio64_ahci_driver_read_report_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_report_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_report_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_report_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_report_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_report_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_report_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_report_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_READY:
            return (u64)mmio64_ahci_driver_read_report_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_USE_DENIED:
            return (u64)mmio64_ahci_driver_read_report_read_use_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_report_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_GRANTED:
            return (u64)mmio64_ahci_driver_read_report_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_DENIED:
            return (u64)mmio64_ahci_driver_read_report_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_report_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_STATUS:
            return (u64)mmio64_ahci_driver_read_report_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_REPORTED_BYTES:
            return (u64)mmio64_ahci_driver_read_report_reported_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_report_report_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_REPORT_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_report_report_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_report_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_report_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_report_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_report_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_report_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_report_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_report_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_report_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_report_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_report_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_ARMED:
            return (u64)mmio64_ahci_driver_read_report_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_report_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_report_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_report_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_report_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_report_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_report_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RECEIPT:
            return (u64)mmio64_stage_ahci_driver_read_receipt(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_STATE:
            return (u64)mmio64_ahci_driver_read_receipt_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_FLAGS:
            return (u64)mmio64_ahci_driver_read_receipt_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_TOKEN:
            return (u64)mmio64_ahci_driver_read_receipt_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_REPORT_TOKEN:
            return (u64)mmio64_ahci_driver_read_receipt_read_report_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_receipt_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_receipt_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_receipt_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_receipt_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_PORT:
            return (u64)mmio64_ahci_driver_read_receipt_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_KIND:
            return (u64)mmio64_ahci_driver_read_receipt_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_receipt_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_receipt_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_receipt_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_receipt_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_receipt_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_receipt_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_receipt_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_READY:
            return (u64)mmio64_ahci_driver_read_receipt_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_REPORT_DENIED:
            return (u64)mmio64_ahci_driver_read_receipt_read_report_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_receipt_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_GRANTED:
            return (u64)mmio64_ahci_driver_read_receipt_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_DENIED:
            return (u64)mmio64_ahci_driver_read_receipt_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_receipt_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_STATUS:
            return (u64)mmio64_ahci_driver_read_receipt_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_RECEIPTED_BYTES:
            return (u64)mmio64_ahci_driver_read_receipt_receipted_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_receipt_receipt_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_RECEIPT_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_receipt_receipt_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_receipt_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_receipt_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_receipt_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_receipt_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_receipt_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_receipt_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_receipt_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_receipt_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_receipt_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_receipt_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_ARMED:
            return (u64)mmio64_ahci_driver_read_receipt_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_receipt_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_receipt_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_receipt_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_receipt_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_receipt_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_receipt_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ACK:
            return (u64)mmio64_stage_ahci_driver_read_ack(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_STATE:
            return (u64)mmio64_ahci_driver_read_ack_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_FLAGS:
            return (u64)mmio64_ahci_driver_read_ack_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_TOKEN:
            return (u64)mmio64_ahci_driver_read_ack_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_RECEIPT_TOKEN:
            return (u64)mmio64_ahci_driver_read_ack_read_receipt_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_ack_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_ack_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_ack_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_ack_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_PORT:
            return (u64)mmio64_ahci_driver_read_ack_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_KIND:
            return (u64)mmio64_ahci_driver_read_ack_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_ack_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_LBA:
            return (u64)mmio64_ahci_driver_read_ack_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_ack_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_ack_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_ack_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_ack_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_ack_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_READY:
            return (u64)mmio64_ahci_driver_read_ack_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_RECEIPT_DENIED:
            return (u64)mmio64_ahci_driver_read_ack_read_receipt_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_REQUESTED:
            return (u64)mmio64_ahci_driver_read_ack_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_GRANTED:
            return (u64)mmio64_ahci_driver_read_ack_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_DENIED:
            return (u64)mmio64_ahci_driver_read_ack_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_ack_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_STATUS:
            return (u64)mmio64_ahci_driver_read_ack_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_ACKED_BYTES:
            return (u64)mmio64_ahci_driver_read_ack_acked_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_ack_ack_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_ACK_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_ack_ack_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_ack_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_ack_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_ack_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_ack_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_ack_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_ack_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_ack_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_ack_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_ack_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_ack_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_ARMED:
            return (u64)mmio64_ahci_driver_read_ack_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_ack_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_ack_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_ack_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_ack_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_ack_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_ack_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_CLOSE:
            return (u64)mmio64_stage_ahci_driver_read_close(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_STATE:
            return (u64)mmio64_ahci_driver_read_close_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_FLAGS:
            return (u64)mmio64_ahci_driver_read_close_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_TOKEN:
            return (u64)mmio64_ahci_driver_read_close_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_ACK_TOKEN:
            return (u64)mmio64_ahci_driver_read_close_read_ack_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_close_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_close_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_close_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_close_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_PORT:
            return (u64)mmio64_ahci_driver_read_close_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_KIND:
            return (u64)mmio64_ahci_driver_read_close_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_close_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_close_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_close_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_close_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_close_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_close_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_close_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_READY:
            return (u64)mmio64_ahci_driver_read_close_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_ACK_DENIED:
            return (u64)mmio64_ahci_driver_read_close_read_ack_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_close_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_GRANTED:
            return (u64)mmio64_ahci_driver_read_close_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_DENIED:
            return (u64)mmio64_ahci_driver_read_close_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_close_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_STATUS:
            return (u64)mmio64_ahci_driver_read_close_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_CLOSED_BYTES:
            return (u64)mmio64_ahci_driver_read_close_closed_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_close_close_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_CLOSE_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_close_close_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_close_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_close_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_close_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_close_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_close_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_close_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_close_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_close_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_close_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_close_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_ARMED:
            return (u64)mmio64_ahci_driver_read_close_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_close_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_close_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_close_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_close_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_close_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_close_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SEAL:
            return (u64)mmio64_stage_ahci_driver_read_seal(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_STATE:
            return (u64)mmio64_ahci_driver_read_seal_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_FLAGS:
            return (u64)mmio64_ahci_driver_read_seal_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_TOKEN:
            return (u64)mmio64_ahci_driver_read_seal_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_CLOSE_TOKEN:
            return (u64)mmio64_ahci_driver_read_seal_read_close_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_seal_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_seal_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_seal_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_seal_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_PORT:
            return (u64)mmio64_ahci_driver_read_seal_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_KIND:
            return (u64)mmio64_ahci_driver_read_seal_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_seal_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_LBA:
            return (u64)mmio64_ahci_driver_read_seal_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_seal_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_seal_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_seal_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_seal_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_seal_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_READY:
            return (u64)mmio64_ahci_driver_read_seal_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_CLOSE_DENIED:
            return (u64)mmio64_ahci_driver_read_seal_read_close_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_REQUESTED:
            return (u64)mmio64_ahci_driver_read_seal_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_GRANTED:
            return (u64)mmio64_ahci_driver_read_seal_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_DENIED:
            return (u64)mmio64_ahci_driver_read_seal_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_seal_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_STATUS:
            return (u64)mmio64_ahci_driver_read_seal_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_SEALED_BYTES:
            return (u64)mmio64_ahci_driver_read_seal_sealed_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_seal_seal_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_SEAL_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_seal_seal_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_seal_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_seal_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_seal_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_seal_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_seal_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_seal_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_seal_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_seal_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_seal_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_seal_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_ARMED:
            return (u64)mmio64_ahci_driver_read_seal_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_seal_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_seal_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_seal_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_seal_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_seal_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_seal_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_UNSEAL:
            return (u64)mmio64_stage_ahci_driver_read_unseal(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_STATE:
            return (u64)mmio64_ahci_driver_read_unseal_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_FLAGS:
            return (u64)mmio64_ahci_driver_read_unseal_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_TOKEN:
            return (u64)mmio64_ahci_driver_read_unseal_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_SEAL_TOKEN:
            return (u64)mmio64_ahci_driver_read_unseal_read_seal_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_unseal_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_unseal_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_unseal_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_unseal_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_PORT:
            return (u64)mmio64_ahci_driver_read_unseal_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_KIND:
            return (u64)mmio64_ahci_driver_read_unseal_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_unseal_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_LBA:
            return (u64)mmio64_ahci_driver_read_unseal_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_unseal_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_unseal_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_unseal_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_unseal_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_unseal_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_READY:
            return (u64)mmio64_ahci_driver_read_unseal_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_SEAL_DENIED:
            return (u64)mmio64_ahci_driver_read_unseal_read_seal_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_REQUESTED:
            return (u64)mmio64_ahci_driver_read_unseal_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_GRANTED:
            return (u64)mmio64_ahci_driver_read_unseal_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_DENIED:
            return (u64)mmio64_ahci_driver_read_unseal_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_unseal_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_STATUS:
            return (u64)mmio64_ahci_driver_read_unseal_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_UNSEALED_BYTES:
            return (u64)mmio64_ahci_driver_read_unseal_unsealed_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_unseal_unseal_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_UNSEAL_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_unseal_unseal_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_unseal_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_unseal_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_unseal_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_unseal_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_unseal_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_unseal_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_unseal_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_unseal_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_unseal_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_unseal_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_ARMED:
            return (u64)mmio64_ahci_driver_read_unseal_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_unseal_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_unseal_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_unseal_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_unseal_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_unseal_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_unseal_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DISCARD:
            return (u64)mmio64_stage_ahci_driver_read_discard(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_STATE:
            return (u64)mmio64_ahci_driver_read_discard_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_FLAGS:
            return (u64)mmio64_ahci_driver_read_discard_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_TOKEN:
            return (u64)mmio64_ahci_driver_read_discard_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_UNSEAL_TOKEN:
            return (u64)mmio64_ahci_driver_read_discard_read_unseal_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_discard_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_discard_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_discard_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_discard_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_PORT:
            return (u64)mmio64_ahci_driver_read_discard_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_KIND:
            return (u64)mmio64_ahci_driver_read_discard_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_discard_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_LBA:
            return (u64)mmio64_ahci_driver_read_discard_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_discard_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_discard_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_discard_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_discard_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_discard_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_READY:
            return (u64)mmio64_ahci_driver_read_discard_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_UNSEAL_DENIED:
            return (u64)mmio64_ahci_driver_read_discard_read_unseal_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_REQUESTED:
            return (u64)mmio64_ahci_driver_read_discard_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_GRANTED:
            return (u64)mmio64_ahci_driver_read_discard_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_DENIED:
            return (u64)mmio64_ahci_driver_read_discard_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_discard_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_STATUS:
            return (u64)mmio64_ahci_driver_read_discard_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_DISCARDED_BYTES:
            return (u64)mmio64_ahci_driver_read_discard_discarded_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_discard_discard_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_DISCARD_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_discard_discard_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_discard_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_discard_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_discard_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_discard_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_discard_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_discard_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_discard_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_discard_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_discard_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_discard_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_ARMED:
            return (u64)mmio64_ahci_driver_read_discard_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_discard_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_discard_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_discard_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_discard_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_discard_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_discard_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_FINALIZE:
            return (u64)mmio64_stage_ahci_driver_read_finalize(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_STATE:
            return (u64)mmio64_ahci_driver_read_finalize_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_FLAGS:
            return (u64)mmio64_ahci_driver_read_finalize_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_TOKEN:
            return (u64)mmio64_ahci_driver_read_finalize_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_DISCARD_TOKEN:
            return (u64)mmio64_ahci_driver_read_finalize_read_discard_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_finalize_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_finalize_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_finalize_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_finalize_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_PORT:
            return (u64)mmio64_ahci_driver_read_finalize_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_KIND:
            return (u64)mmio64_ahci_driver_read_finalize_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_finalize_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_finalize_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_finalize_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_finalize_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_finalize_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_finalize_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_finalize_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_READY:
            return (u64)mmio64_ahci_driver_read_finalize_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_DISCARD_DENIED:
            return (u64)mmio64_ahci_driver_read_finalize_read_discard_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_finalize_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_GRANTED:
            return (u64)mmio64_ahci_driver_read_finalize_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_DENIED:
            return (u64)mmio64_ahci_driver_read_finalize_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_finalize_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_STATUS:
            return (u64)mmio64_ahci_driver_read_finalize_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_FINALIZED_BYTES:
            return (u64)mmio64_ahci_driver_read_finalize_finalized_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_finalize_finalize_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_FINALIZE_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_finalize_finalize_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_finalize_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_EXECUTE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_finalize_execute_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_finalize_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_finalize_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_finalize_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_finalize_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_finalize_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_finalize_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_finalize_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_finalize_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_ARMED:
            return (u64)mmio64_ahci_driver_read_finalize_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_finalize_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_finalize_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_finalize_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_finalize_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_finalize_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_finalize_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_AUTHORIZE:
            return (u64)mmio64_stage_ahci_driver_read_authorize(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_STATE:
            return (u64)mmio64_ahci_driver_read_authorize_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_FLAGS:
            return (u64)mmio64_ahci_driver_read_authorize_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_TOKEN:
            return (u64)mmio64_ahci_driver_read_authorize_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_READ_FINALIZE_TOKEN:
            return (u64)mmio64_ahci_driver_read_authorize_read_finalize_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_authorize_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_authorize_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_authorize_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_authorize_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_PORT:
            return (u64)mmio64_ahci_driver_read_authorize_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_KIND:
            return (u64)mmio64_ahci_driver_read_authorize_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_authorize_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_authorize_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_authorize_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_authorize_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_authorize_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_authorize_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_authorize_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_READ_READY:
            return (u64)mmio64_ahci_driver_read_authorize_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_READ_FINALIZE_DENIED:
            return (u64)mmio64_ahci_driver_read_authorize_read_finalize_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_authorize_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_GRANTED:
            return (u64)mmio64_ahci_driver_read_authorize_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_DENIED:
            return (u64)mmio64_ahci_driver_read_authorize_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_authorize_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_authorize_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_authorize_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_authorize_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_authorize_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_authorize_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_authorize_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_authorize_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_authorize_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_authorize_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_authorize_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_authorize_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_authorize_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_authorize_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_ARMED:
            return (u64)mmio64_ahci_driver_read_authorize_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_authorize_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_authorize_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_authorize_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_authorize_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_authorize_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_authorize_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DISPATCH:
            return (u64)mmio64_stage_ahci_driver_read_dispatch(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_STATE:
            return (u64)mmio64_ahci_driver_read_dispatch_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_FLAGS:
            return (u64)mmio64_ahci_driver_read_dispatch_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_TOKEN:
            return (u64)mmio64_ahci_driver_read_dispatch_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_READ_AUTHORIZE_TOKEN:
            return (u64)mmio64_ahci_driver_read_dispatch_read_authorize_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_dispatch_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_dispatch_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_dispatch_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_dispatch_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_PORT:
            return (u64)mmio64_ahci_driver_read_dispatch_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_KIND:
            return (u64)mmio64_ahci_driver_read_dispatch_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_dispatch_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_READ_LBA:
            return (u64)mmio64_ahci_driver_read_dispatch_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_dispatch_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_dispatch_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_dispatch_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_dispatch_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_dispatch_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_READ_READY:
            return (u64)mmio64_ahci_driver_read_dispatch_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_READ_AUTHORIZE_DENIED:
            return (u64)mmio64_ahci_driver_read_dispatch_read_authorize_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_REQUESTED:
            return (u64)mmio64_ahci_driver_read_dispatch_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_GRANTED:
            return (u64)mmio64_ahci_driver_read_dispatch_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_DENIED:
            return (u64)mmio64_ahci_driver_read_dispatch_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_dispatch_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_DISPATCH_QUEUED:
            return (u64)mmio64_ahci_driver_read_dispatch_dispatch_queued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_QUEUE_DEPTH:
            return (u64)mmio64_ahci_driver_read_dispatch_queue_depth();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dispatch_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dispatch_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dispatch_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dispatch_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dispatch_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_dispatch_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_dispatch_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_dispatch_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_dispatch_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_dispatch_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_dispatch_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_dispatch_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_dispatch_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_ARMED:
            return (u64)mmio64_ahci_driver_read_dispatch_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_dispatch_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_dispatch_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_dispatch_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_dispatch_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_dispatch_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_dispatch_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_QUEUE:
            return (u64)mmio64_stage_ahci_driver_read_queue(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_STATE:
            return (u64)mmio64_ahci_driver_read_queue_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_FLAGS:
            return (u64)mmio64_ahci_driver_read_queue_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_TOKEN:
            return (u64)mmio64_ahci_driver_read_queue_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_READ_DISPATCH_TOKEN:
            return (u64)mmio64_ahci_driver_read_queue_read_dispatch_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_queue_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_queue_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_queue_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_queue_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_PORT:
            return (u64)mmio64_ahci_driver_read_queue_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_KIND:
            return (u64)mmio64_ahci_driver_read_queue_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_queue_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_queue_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_queue_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_queue_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_queue_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_queue_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_queue_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_READ_READY:
            return (u64)mmio64_ahci_driver_read_queue_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_READ_DISPATCH_DENIED:
            return (u64)mmio64_ahci_driver_read_queue_read_dispatch_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_queue_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_GRANTED:
            return (u64)mmio64_ahci_driver_read_queue_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_DENIED:
            return (u64)mmio64_ahci_driver_read_queue_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_queue_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_QUEUE_INSERTED:
            return (u64)mmio64_ahci_driver_read_queue_queue_inserted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_QUEUE_DEPTH:
            return (u64)mmio64_ahci_driver_read_queue_queue_depth();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_WORKER_WAKE:
            return (u64)mmio64_ahci_driver_read_queue_worker_wake();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_queue_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_queue_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_queue_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_queue_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_queue_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_queue_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_queue_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_queue_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_queue_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_queue_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_queue_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_queue_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_queue_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_ARMED:
            return (u64)mmio64_ahci_driver_read_queue_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_queue_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_queue_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_queue_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_queue_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_queue_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_queue_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_WORKER:
            return (u64)mmio64_stage_ahci_driver_read_worker(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_STATE:
            return (u64)mmio64_ahci_driver_read_worker_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_FLAGS:
            return (u64)mmio64_ahci_driver_read_worker_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_TOKEN:
            return (u64)mmio64_ahci_driver_read_worker_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_READ_QUEUE_TOKEN:
            return (u64)mmio64_ahci_driver_read_worker_read_queue_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_worker_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_worker_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_worker_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_worker_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_PORT:
            return (u64)mmio64_ahci_driver_read_worker_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_KIND:
            return (u64)mmio64_ahci_driver_read_worker_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_worker_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_READ_LBA:
            return (u64)mmio64_ahci_driver_read_worker_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_worker_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_worker_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_worker_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_worker_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_worker_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_READ_READY:
            return (u64)mmio64_ahci_driver_read_worker_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_READ_QUEUE_DENIED:
            return (u64)mmio64_ahci_driver_read_worker_read_queue_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_REQUESTED:
            return (u64)mmio64_ahci_driver_read_worker_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_GRANTED:
            return (u64)mmio64_ahci_driver_read_worker_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_DENIED:
            return (u64)mmio64_ahci_driver_read_worker_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_worker_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_QUEUE_INSERTED:
            return (u64)mmio64_ahci_driver_read_worker_queue_inserted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_QUEUE_DEPTH:
            return (u64)mmio64_ahci_driver_read_worker_queue_depth();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_WORKER_WAKE:
            return (u64)mmio64_ahci_driver_read_worker_worker_wake();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_WORKER_DEQUEUED:
            return (u64)mmio64_ahci_driver_read_worker_worker_dequeued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_worker_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_worker_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_worker_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_worker_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_worker_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_worker_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_worker_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_worker_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_worker_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_worker_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_worker_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_worker_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_worker_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_ARMED:
            return (u64)mmio64_ahci_driver_read_worker_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_worker_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_worker_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_worker_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_worker_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_worker_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_worker_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SCHEDULE:
            return (u64)mmio64_stage_ahci_driver_read_schedule(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_STATE:
            return (u64)mmio64_ahci_driver_read_schedule_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_FLAGS:
            return (u64)mmio64_ahci_driver_read_schedule_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_TOKEN:
            return (u64)mmio64_ahci_driver_read_schedule_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_READ_WORKER_TOKEN:
            return (u64)mmio64_ahci_driver_read_schedule_read_worker_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_schedule_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_schedule_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_schedule_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_schedule_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_PORT:
            return (u64)mmio64_ahci_driver_read_schedule_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_KIND:
            return (u64)mmio64_ahci_driver_read_schedule_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_schedule_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_schedule_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_schedule_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_schedule_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_schedule_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_schedule_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_schedule_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_READ_READY:
            return (u64)mmio64_ahci_driver_read_schedule_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_READ_WORKER_DENIED:
            return (u64)mmio64_ahci_driver_read_schedule_read_worker_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_schedule_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_GRANTED:
            return (u64)mmio64_ahci_driver_read_schedule_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_DENIED:
            return (u64)mmio64_ahci_driver_read_schedule_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_schedule_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_QUEUE_INSERTED:
            return (u64)mmio64_ahci_driver_read_schedule_queue_inserted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_QUEUE_DEPTH:
            return (u64)mmio64_ahci_driver_read_schedule_queue_depth();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_WORKER_WAKE:
            return (u64)mmio64_ahci_driver_read_schedule_worker_wake();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_WORKER_DEQUEUED:
            return (u64)mmio64_ahci_driver_read_schedule_worker_dequeued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_WORKER_RUNNABLE:
            return (u64)mmio64_ahci_driver_read_schedule_worker_runnable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_WORKER_SCHEDULED:
            return (u64)mmio64_ahci_driver_read_schedule_worker_scheduled();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_schedule_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_schedule_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_schedule_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_schedule_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_schedule_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_schedule_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_schedule_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_schedule_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_schedule_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_schedule_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_schedule_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_schedule_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_schedule_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_ARMED:
            return (u64)mmio64_ahci_driver_read_schedule_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_schedule_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_schedule_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_schedule_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_schedule_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_schedule_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_schedule_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RUN:
            return (u64)mmio64_stage_ahci_driver_read_run(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_STATE:
            return (u64)mmio64_ahci_driver_read_run_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_FLAGS:
            return (u64)mmio64_ahci_driver_read_run_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_TOKEN:
            return (u64)mmio64_ahci_driver_read_run_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_SCHEDULE_TOKEN:
            return (u64)mmio64_ahci_driver_read_run_read_schedule_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_run_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_run_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_run_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_run_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PORT:
            return (u64)mmio64_ahci_driver_read_run_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_KIND:
            return (u64)mmio64_ahci_driver_read_run_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_run_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_LBA:
            return (u64)mmio64_ahci_driver_read_run_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_run_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_run_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_run_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_run_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_run_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_READY:
            return (u64)mmio64_ahci_driver_read_run_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_SCHEDULE_DENIED:
            return (u64)mmio64_ahci_driver_read_run_read_schedule_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_REQUESTED:
            return (u64)mmio64_ahci_driver_read_run_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_GRANTED:
            return (u64)mmio64_ahci_driver_read_run_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DENIED:
            return (u64)mmio64_ahci_driver_read_run_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_run_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_QUEUE_INSERTED:
            return (u64)mmio64_ahci_driver_read_run_queue_inserted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_QUEUE_DEPTH:
            return (u64)mmio64_ahci_driver_read_run_queue_depth();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_WAKE:
            return (u64)mmio64_ahci_driver_read_run_worker_wake();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_DEQUEUED:
            return (u64)mmio64_ahci_driver_read_run_worker_dequeued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_RUNNABLE:
            return (u64)mmio64_ahci_driver_read_run_worker_runnable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_SCHEDULED:
            return (u64)mmio64_ahci_driver_read_run_worker_scheduled();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_RUN:
            return (u64)mmio64_ahci_driver_read_run_worker_run();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_EXECUTED:
            return (u64)mmio64_ahci_driver_read_run_worker_executed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_run_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_run_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_run_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_run_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_run_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_run_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_run_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_run_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_run_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_run_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_run_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_run_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_run_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_ARMED:
            return (u64)mmio64_ahci_driver_read_run_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_run_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_run_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_run_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_run_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_run_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_run_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_BODY:
            return (u64)mmio64_stage_ahci_driver_read_body(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_STATE:
            return (u64)mmio64_ahci_driver_read_body_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_FLAGS:
            return (u64)mmio64_ahci_driver_read_body_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_TOKEN:
            return (u64)mmio64_ahci_driver_read_body_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_RUN_TOKEN:
            return (u64)mmio64_ahci_driver_read_body_read_run_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_body_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_body_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_body_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_body_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PORT:
            return (u64)mmio64_ahci_driver_read_body_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_KIND:
            return (u64)mmio64_ahci_driver_read_body_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_body_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_LBA:
            return (u64)mmio64_ahci_driver_read_body_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_body_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_body_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_body_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_body_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_body_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_READY:
            return (u64)mmio64_ahci_driver_read_body_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_RUN_DENIED:
            return (u64)mmio64_ahci_driver_read_body_read_run_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_REQUESTED:
            return (u64)mmio64_ahci_driver_read_body_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_GRANTED:
            return (u64)mmio64_ahci_driver_read_body_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DENIED:
            return (u64)mmio64_ahci_driver_read_body_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_body_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_QUEUE_INSERTED:
            return (u64)mmio64_ahci_driver_read_body_queue_inserted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_QUEUE_DEPTH:
            return (u64)mmio64_ahci_driver_read_body_queue_depth();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_WAKE:
            return (u64)mmio64_ahci_driver_read_body_worker_wake();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_DEQUEUED:
            return (u64)mmio64_ahci_driver_read_body_worker_dequeued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_RUNNABLE:
            return (u64)mmio64_ahci_driver_read_body_worker_runnable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_SCHEDULED:
            return (u64)mmio64_ahci_driver_read_body_worker_scheduled();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_RUN:
            return (u64)mmio64_ahci_driver_read_body_worker_run();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_EXECUTED:
            return (u64)mmio64_ahci_driver_read_body_worker_executed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BODY_ENTERED:
            return (u64)mmio64_ahci_driver_read_body_body_entered();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BODY_COMPLETED:
            return (u64)mmio64_ahci_driver_read_body_body_completed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_body_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_body_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_body_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_body_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_body_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_body_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_body_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_body_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_body_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_body_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_body_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_body_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_body_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_ARMED:
            return (u64)mmio64_ahci_driver_read_body_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_body_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_body_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_body_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_body_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_body_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_body_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ISSUE:
            return (u64)mmio64_stage_ahci_driver_read_issue(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_STATE:
            return (u64)mmio64_ahci_driver_read_issue_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_FLAGS:
            return (u64)mmio64_ahci_driver_read_issue_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_TOKEN:
            return (u64)mmio64_ahci_driver_read_issue_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BODY_TOKEN:
            return (u64)mmio64_ahci_driver_read_issue_read_body_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_issue_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_issue_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_issue_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_issue_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PORT:
            return (u64)mmio64_ahci_driver_read_issue_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_KIND:
            return (u64)mmio64_ahci_driver_read_issue_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_issue_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_issue_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_issue_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_issue_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_issue_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_issue_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_issue_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_READY:
            return (u64)mmio64_ahci_driver_read_issue_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BODY_DENIED:
            return (u64)mmio64_ahci_driver_read_issue_read_body_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_issue_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_GRANTED:
            return (u64)mmio64_ahci_driver_read_issue_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DENIED:
            return (u64)mmio64_ahci_driver_read_issue_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_issue_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_QUEUE_INSERTED:
            return (u64)mmio64_ahci_driver_read_issue_queue_inserted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_QUEUE_DEPTH:
            return (u64)mmio64_ahci_driver_read_issue_queue_depth();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_WAKE:
            return (u64)mmio64_ahci_driver_read_issue_worker_wake();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_DEQUEUED:
            return (u64)mmio64_ahci_driver_read_issue_worker_dequeued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_RUNNABLE:
            return (u64)mmio64_ahci_driver_read_issue_worker_runnable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_SCHEDULED:
            return (u64)mmio64_ahci_driver_read_issue_worker_scheduled();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_RUN:
            return (u64)mmio64_ahci_driver_read_issue_worker_run();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_EXECUTED:
            return (u64)mmio64_ahci_driver_read_issue_worker_executed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ISSUE_ENTERED:
            return (u64)mmio64_ahci_driver_read_issue_issue_entered();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ISSUE_COMPLETED:
            return (u64)mmio64_ahci_driver_read_issue_issue_completed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_issue_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_issue_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_issue_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_issue_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_issue_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_issue_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_issue_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_issue_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_issue_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_issue_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_issue_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_issue_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_issue_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ARMED:
            return (u64)mmio64_ahci_driver_read_issue_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_issue_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_issue_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_issue_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_issue_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_issue_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_issue_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DMA:
            return (u64)mmio64_stage_ahci_driver_read_dma(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_STATE:
            return (u64)mmio64_ahci_driver_read_dma_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_FLAGS:
            return (u64)mmio64_ahci_driver_read_dma_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_TOKEN:
            return (u64)mmio64_ahci_driver_read_dma_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_ISSUE_TOKEN:
            return (u64)mmio64_ahci_driver_read_dma_read_issue_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_dma_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_dma_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_dma_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_dma_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PORT:
            return (u64)mmio64_ahci_driver_read_dma_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_KIND:
            return (u64)mmio64_ahci_driver_read_dma_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_dma_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_LBA:
            return (u64)mmio64_ahci_driver_read_dma_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_dma_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_dma_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_dma_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_dma_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_dma_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_READY:
            return (u64)mmio64_ahci_driver_read_dma_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_ISSUE_DENIED:
            return (u64)mmio64_ahci_driver_read_dma_read_issue_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_REQUESTED:
            return (u64)mmio64_ahci_driver_read_dma_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_GRANTED:
            return (u64)mmio64_ahci_driver_read_dma_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DENIED:
            return (u64)mmio64_ahci_driver_read_dma_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_dma_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_dma_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_WINDOW_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_dma_window_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_WINDOW_OPEN:
            return (u64)mmio64_ahci_driver_read_dma_window_open();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_ENTERED:
            return (u64)mmio64_ahci_driver_read_dma_entered();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_COMPLETED:
            return (u64)mmio64_ahci_driver_read_dma_completed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dma_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dma_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dma_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dma_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_dma_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_dma_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_dma_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_dma_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_dma_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_dma_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_dma_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_dma_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_dma_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_ARMED:
            return (u64)mmio64_ahci_driver_read_dma_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_dma_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_dma_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_dma_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_dma_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_dma_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_dma_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_IRQ:
            return (u64)mmio64_stage_ahci_driver_read_irq(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_STATE:
            return (u64)mmio64_ahci_driver_read_irq_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_FLAGS:
            return (u64)mmio64_ahci_driver_read_irq_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_TOKEN:
            return (u64)mmio64_ahci_driver_read_irq_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_READ_DMA_TOKEN:
            return (u64)mmio64_ahci_driver_read_irq_read_dma_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_irq_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_irq_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_irq_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_irq_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_PORT:
            return (u64)mmio64_ahci_driver_read_irq_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_KIND:
            return (u64)mmio64_ahci_driver_read_irq_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_irq_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_READ_LBA:
            return (u64)mmio64_ahci_driver_read_irq_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_irq_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_irq_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_irq_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_irq_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_irq_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_READ_READY:
            return (u64)mmio64_ahci_driver_read_irq_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_READ_DMA_DENIED:
            return (u64)mmio64_ahci_driver_read_irq_read_dma_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_REQUESTED:
            return (u64)mmio64_ahci_driver_read_irq_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_GRANTED:
            return (u64)mmio64_ahci_driver_read_irq_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_DENIED:
            return (u64)mmio64_ahci_driver_read_irq_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_irq_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_irq_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_IRQ_WAIT:
            return (u64)mmio64_ahci_driver_read_irq_irq_wait();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_IRQ_FIRED:
            return (u64)mmio64_ahci_driver_read_irq_irq_fired();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_COMPLETION_STATUS:
            return (u64)mmio64_ahci_driver_read_irq_completion_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_COMPLETION_BYTES:
            return (u64)mmio64_ahci_driver_read_irq_completion_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_COMPLETION_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_irq_completion_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_irq_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_irq_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_irq_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_irq_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_irq_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_irq_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_irq_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_irq_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_irq_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_irq_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_irq_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_irq_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_irq_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_ARMED:
            return (u64)mmio64_ahci_driver_read_irq_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_irq_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_irq_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_irq_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_irq_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_irq_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_irq_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS:
            return (u64)mmio64_stage_ahci_driver_read_status(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STATE:
            return (u64)mmio64_ahci_driver_read_status_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_IRQ_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_read_irq_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_PORT:
            return (u64)mmio64_ahci_driver_read_status_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_KIND:
            return (u64)mmio64_ahci_driver_read_status_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_IRQ_DENIED:
            return (u64)mmio64_ahci_driver_read_status_read_irq_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DENIED:
            return (u64)mmio64_ahci_driver_read_status_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_status_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_status_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STATUS_POLL:
            return (u64)mmio64_ahci_driver_read_status_status_poll();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STATUS_READY:
            return (u64)mmio64_ahci_driver_read_status_status_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_PORT_INTERRUPT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_port_interrupt_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE:
            return (u64)mmio64_ahci_driver_read_status_command_issue();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_TASK_FILE_STATUS:
            return (u64)mmio64_ahci_driver_read_status_task_file_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_PORT_ERROR:
            return (u64)mmio64_ahci_driver_read_status_port_error();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_IRQ_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_irq_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMPLETION_STATUS:
            return (u64)mmio64_ahci_driver_read_status_completion_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMPLETION_BYTES:
            return (u64)mmio64_ahci_driver_read_status_completion_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMPLETION_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_completion_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARMED:
            return (u64)mmio64_ahci_driver_read_status_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RESULT:
            return (u64)mmio64_stage_ahci_driver_read_status_result(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_STATE:
            return (u64)mmio64_ahci_driver_read_status_result_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_result_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_result_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_READ_STATUS_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_result_read_status_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_result_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_result_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_result_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_result_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_PORT:
            return (u64)mmio64_ahci_driver_read_status_result_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_KIND:
            return (u64)mmio64_ahci_driver_read_status_result_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_result_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_result_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_result_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_result_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_result_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_result_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_result_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_result_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_READ_STATUS_DENIED:
            return (u64)mmio64_ahci_driver_read_status_result_read_status_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_result_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_result_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_status_result_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_status_result_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_result_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_result_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_result_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_result_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_result_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_result_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_result_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_result_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_result_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_result_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_result_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_result_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_result_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_result_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_result_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_result_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_ARMED:
            return (u64)mmio64_ahci_driver_read_status_result_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_result_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_result_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_result_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_result_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_result_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_result_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_SAMPLE:
            return (u64)mmio64_stage_ahci_driver_read_status_sample(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_STATE:
            return (u64)mmio64_ahci_driver_read_status_sample_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_sample_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_sample_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_READ_STATUS_RESULT_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_sample_read_status_result_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_sample_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_sample_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_sample_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_sample_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_PORT:
            return (u64)mmio64_ahci_driver_read_status_sample_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_KIND:
            return (u64)mmio64_ahci_driver_read_status_sample_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_sample_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_sample_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_sample_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_sample_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_sample_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_sample_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_sample_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_sample_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_STATUS_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_sample_status_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_sample_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_sample_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_DENIED:
            return (u64)mmio64_ahci_driver_read_status_sample_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_status_sample_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_status_sample_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_PORT_INTERRUPT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_sample_port_interrupt_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_COMMAND_ISSUE:
            return (u64)mmio64_ahci_driver_read_status_sample_command_issue();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_TASK_FILE_STATUS:
            return (u64)mmio64_ahci_driver_read_status_sample_task_file_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_PORT_ERROR:
            return (u64)mmio64_ahci_driver_read_status_sample_port_error();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_TFD_READY:
            return (u64)mmio64_ahci_driver_read_status_sample_tfd_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_CI_IDLE:
            return (u64)mmio64_ahci_driver_read_status_sample_ci_idle();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_SERR_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_sample_serr_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_IRQ_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_sample_irq_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_sample_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_sample_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_sample_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_sample_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_sample_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_sample_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_sample_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_sample_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_sample_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_sample_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_sample_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_sample_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_sample_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_sample_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_sample_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_sample_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_ARMED:
            return (u64)mmio64_ahci_driver_read_status_sample_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_sample_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_sample_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_sample_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_sample_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_sample_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_sample_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLEAR:
            return (u64)mmio64_stage_ahci_driver_read_status_clear(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_STATE:
            return (u64)mmio64_ahci_driver_read_status_clear_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_clear_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_clear_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_READ_STATUS_SAMPLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_clear_read_status_sample_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_clear_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_clear_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_clear_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_clear_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PORT:
            return (u64)mmio64_ahci_driver_read_status_clear_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_KIND:
            return (u64)mmio64_ahci_driver_read_status_clear_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_clear_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_clear_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_clear_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_clear_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_clear_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_clear_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_clear_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_clear_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_STATUS_SAMPLE_READY:
            return (u64)mmio64_ahci_driver_read_status_clear_status_sample_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_STATUS_SAMPLE_TOKEN_BOUND:
            return (u64)mmio64_ahci_driver_read_status_clear_status_sample_token_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_clear_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_clear_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_DENIED:
            return (u64)mmio64_ahci_driver_read_status_clear_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_status_clear_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_status_clear_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PORT_INTERRUPT_STATUS_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_clear_port_interrupt_status_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PORT_INTERRUPT_STATUS_AFTER:
            return (u64)mmio64_ahci_driver_read_status_clear_port_interrupt_status_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PORT_INTERRUPT_STATUS_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_clear_port_interrupt_status_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_COMMAND_ISSUE:
            return (u64)mmio64_ahci_driver_read_status_clear_command_issue();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_TASK_FILE_STATUS:
            return (u64)mmio64_ahci_driver_read_status_clear_task_file_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PORT_ERROR:
            return (u64)mmio64_ahci_driver_read_status_clear_port_error();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_TFD_READY:
            return (u64)mmio64_ahci_driver_read_status_clear_tfd_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_CI_IDLE:
            return (u64)mmio64_ahci_driver_read_status_clear_ci_idle();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_SERR_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_clear_serr_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_CLEAR_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_clear_clear_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_CLEAR_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_clear_clear_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_CLEAR_DENIED:
            return (u64)mmio64_ahci_driver_read_status_clear_clear_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_CLEAR_VALUE:
            return (u64)mmio64_ahci_driver_read_status_clear_clear_value();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_IRQ_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_clear_irq_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_clear_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_clear_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_clear_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_clear_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_clear_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_clear_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_clear_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_clear_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_clear_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_clear_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_clear_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_ARMED:
            return (u64)mmio64_ahci_driver_read_status_clear_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_clear_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_clear_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_clear_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_clear_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_clear_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_clear_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT:
            return (u64)mmio64_stage_ahci_driver_read_status_clear_result(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_STATE:
            return (u64)mmio64_ahci_driver_read_status_clear_result_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_clear_result_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_clear_result_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_READ_STATUS_CLEAR_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_clear_result_read_status_clear_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_clear_result_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_clear_result_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PORT:
            return (u64)mmio64_ahci_driver_read_status_clear_result_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_KIND:
            return (u64)mmio64_ahci_driver_read_status_clear_result_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_clear_result_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_clear_result_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_clear_result_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_clear_result_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_clear_result_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_clear_result_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_STATUS_CLEAR_DENIED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_status_clear_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_status_clear_result_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_status_clear_result_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PORT_INTERRUPT_STATUS_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_clear_result_port_interrupt_status_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PORT_INTERRUPT_STATUS_AFTER:
            return (u64)mmio64_ahci_driver_read_status_clear_result_port_interrupt_status_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PORT_INTERRUPT_STATUS_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_port_interrupt_status_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_COMMAND_ISSUE:
            return (u64)mmio64_ahci_driver_read_status_clear_result_command_issue();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_TASK_FILE_STATUS:
            return (u64)mmio64_ahci_driver_read_status_clear_result_task_file_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PORT_ERROR:
            return (u64)mmio64_ahci_driver_read_status_clear_result_port_error();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_TFD_READY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_tfd_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CI_IDLE:
            return (u64)mmio64_ahci_driver_read_status_clear_result_ci_idle();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_SERR_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_clear_result_serr_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CLEAR_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_clear_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CLEAR_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_clear_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CLEAR_DENIED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_clear_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CLEAR_VALUE:
            return (u64)mmio64_ahci_driver_read_status_clear_result_clear_value();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_IRQ_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_clear_result_irq_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CLEAR_RESULT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_clear_result_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CLEAR_RESULT_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_clear_result_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_CLEAR_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_clear_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_clear_result_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_clear_result_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_clear_result_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_clear_result_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_clear_result_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_ARMED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_clear_result_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_clear_result_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_clear_result_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_clear_result_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_clear_result_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_clear_result_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RESAMPLE:
            return (u64)mmio64_stage_ahci_driver_read_status_resample(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_STATE:
            return (u64)mmio64_ahci_driver_read_status_resample_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_resample_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_resample_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_STATUS_CLEAR_RESULT_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_resample_read_status_clear_result_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_resample_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_resample_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_resample_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_resample_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT:
            return (u64)mmio64_ahci_driver_read_status_resample_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_KIND:
            return (u64)mmio64_ahci_driver_read_status_resample_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_resample_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_resample_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_resample_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_resample_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_resample_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_resample_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_resample_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_resample_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_CLEAR_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_resample_clear_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_resample_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_resample_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DENIED:
            return (u64)mmio64_ahci_driver_read_status_resample_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_status_resample_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_status_resample_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_INTERRUPT_STATUS_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_resample_port_interrupt_status_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_INTERRUPT_STATUS_AFTER:
            return (u64)mmio64_ahci_driver_read_status_resample_port_interrupt_status_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_INTERRUPT_STATUS_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_resample_port_interrupt_status_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_COMMAND_ISSUE:
            return (u64)mmio64_ahci_driver_read_status_resample_command_issue();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_TASK_FILE_STATUS:
            return (u64)mmio64_ahci_driver_read_status_resample_task_file_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_ERROR:
            return (u64)mmio64_ahci_driver_read_status_resample_port_error();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_TFD_READY:
            return (u64)mmio64_ahci_driver_read_status_resample_tfd_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_CI_IDLE:
            return (u64)mmio64_ahci_driver_read_status_resample_ci_idle();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_SERR_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_resample_serr_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_IRQ_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_resample_irq_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_resample_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_resample_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_resample_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_resample_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_resample_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_resample_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_resample_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_resample_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_resample_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_resample_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_resample_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_resample_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_resample_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_resample_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_resample_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_resample_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_ARMED:
            return (u64)mmio64_ahci_driver_read_status_resample_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_resample_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_resample_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_resample_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_resample_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_resample_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_resample_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_STABLE:
            return (u64)mmio64_stage_ahci_driver_read_status_stable(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_STATE:
            return (u64)mmio64_ahci_driver_read_status_stable_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_stable_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_stable_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_STATUS_RESAMPLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_stable_read_status_resample_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_stable_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_stable_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_stable_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_stable_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT:
            return (u64)mmio64_ahci_driver_read_status_stable_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_KIND:
            return (u64)mmio64_ahci_driver_read_status_stable_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_stable_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_stable_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_stable_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_stable_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_stable_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_stable_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_stable_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_stable_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_CLEAR_RESULT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_stable_clear_result_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESAMPLE_READ_ONLY:
            return (u64)mmio64_ahci_driver_read_status_stable_resample_read_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_stable_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_stable_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DENIED:
            return (u64)mmio64_ahci_driver_read_status_stable_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_POLICY_GRANT:
            return (u64)mmio64_ahci_driver_read_status_stable_policy_grant();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BYTES_AVAILABLE:
            return (u64)mmio64_ahci_driver_read_status_stable_bytes_available();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_INTERRUPT_STATUS_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_stable_port_interrupt_status_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_INTERRUPT_STATUS_AFTER:
            return (u64)mmio64_ahci_driver_read_status_stable_port_interrupt_status_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_INTERRUPT_STATUS_STABLE:
            return (u64)mmio64_ahci_driver_read_status_stable_port_interrupt_status_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUE_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_stable_command_issue_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUE_AFTER:
            return (u64)mmio64_ahci_driver_read_status_stable_command_issue_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUE_STABLE:
            return (u64)mmio64_ahci_driver_read_status_stable_command_issue_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TASK_FILE_STATUS_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_stable_task_file_status_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TASK_FILE_STATUS_AFTER:
            return (u64)mmio64_ahci_driver_read_status_stable_task_file_status_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TASK_FILE_STATUS_STABLE:
            return (u64)mmio64_ahci_driver_read_status_stable_task_file_status_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_ERROR_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_stable_port_error_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_ERROR_AFTER:
            return (u64)mmio64_ahci_driver_read_status_stable_port_error_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_ERROR_STABLE:
            return (u64)mmio64_ahci_driver_read_status_stable_port_error_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TFD_READY:
            return (u64)mmio64_ahci_driver_read_status_stable_tfd_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_CI_IDLE:
            return (u64)mmio64_ahci_driver_read_status_stable_ci_idle();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_SERR_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_stable_serr_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_IRQ_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_stable_irq_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_stable_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_stable_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_stable_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_ISSUE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_stable_issue_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DMA_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_stable_dma_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_stable_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_stable_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_stable_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_stable_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_stable_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_stable_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_stable_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_stable_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_stable_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_stable_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_stable_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_ARMED:
            return (u64)mmio64_ahci_driver_read_status_stable_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_stable_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_stable_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_stable_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_stable_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_stable_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_stable_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_GUARD:
            return (u64)mmio64_stage_ahci_driver_read_status_guard(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_STATE:
            return (u64)mmio64_ahci_driver_read_status_guard_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_guard_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_guard_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_STATUS_STABLE_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_guard_read_status_stable_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_guard_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_guard_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_guard_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_guard_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT:
            return (u64)mmio64_ahci_driver_read_status_guard_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_KIND:
            return (u64)mmio64_ahci_driver_read_status_guard_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_guard_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_guard_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_guard_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_guard_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_guard_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_guard_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_guard_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_guard_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_INTERRUPT_STATUS_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_guard_port_interrupt_status_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_INTERRUPT_STATUS_AFTER:
            return (u64)mmio64_ahci_driver_read_status_guard_port_interrupt_status_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_INTERRUPT_STATUS_STABLE:
            return (u64)mmio64_ahci_driver_read_status_guard_port_interrupt_status_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUE_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_guard_command_issue_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUE_AFTER:
            return (u64)mmio64_ahci_driver_read_status_guard_command_issue_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUE_STABLE:
            return (u64)mmio64_ahci_driver_read_status_guard_command_issue_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TASK_FILE_STATUS_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_guard_task_file_status_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TASK_FILE_STATUS_AFTER:
            return (u64)mmio64_ahci_driver_read_status_guard_task_file_status_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TASK_FILE_STATUS_STABLE:
            return (u64)mmio64_ahci_driver_read_status_guard_task_file_status_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_ERROR_BEFORE:
            return (u64)mmio64_ahci_driver_read_status_guard_port_error_before();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_ERROR_AFTER:
            return (u64)mmio64_ahci_driver_read_status_guard_port_error_after();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_ERROR_STABLE:
            return (u64)mmio64_ahci_driver_read_status_guard_port_error_stable();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TFD_READY:
            return (u64)mmio64_ahci_driver_read_status_guard_tfd_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_CI_IDLE:
            return (u64)mmio64_ahci_driver_read_status_guard_ci_idle();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_SERR_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_guard_serr_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_GUARD_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_guard_guard_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_ISSUE_ALLOWED:
            return (u64)mmio64_ahci_driver_read_status_guard_issue_allowed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_ISSUE_DENIED:
            return (u64)mmio64_ahci_driver_read_status_guard_issue_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DMA_ALLOWED:
            return (u64)mmio64_ahci_driver_read_status_guard_dma_allowed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DMA_DENIED:
            return (u64)mmio64_ahci_driver_read_status_guard_dma_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_guard_media_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_READ_DENIED:
            return (u64)mmio64_ahci_driver_read_status_guard_media_read_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_guard_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_WRITE_DENIED:
            return (u64)mmio64_ahci_driver_read_status_guard_write_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_guard_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMIT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_guard_commit_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_IRQ_CLEAR:
            return (u64)mmio64_ahci_driver_read_status_guard_irq_clear();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_guard_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_guard_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_guard_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_guard_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_guard_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_guard_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_guard_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_guard_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_guard_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_guard_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_guard_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_ARMED:
            return (u64)mmio64_ahci_driver_read_status_guard_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_guard_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_guard_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_guard_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_guard_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_guard_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_guard_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_BUFFER:
            return (u64)mmio64_stage_ahci_driver_read_status_buffer(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_STATE:
            return (u64)mmio64_ahci_driver_read_status_buffer_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_buffer_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_buffer_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_STATUS_GUARD_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_buffer_read_status_guard_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_buffer_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_buffer_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_buffer_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_buffer_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PORT:
            return (u64)mmio64_ahci_driver_read_status_buffer_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_KIND:
            return (u64)mmio64_ahci_driver_read_status_buffer_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_buffer_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_buffer_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_buffer_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_buffer_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PAGE_BYTES:
            return (u64)mmio64_ahci_driver_read_status_buffer_page_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_buffer_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BUFFER_ZEROED:
            return (u64)mmio64_ahci_driver_read_status_buffer_buffer_zeroed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_READY:
            return (u64)mmio64_ahci_driver_read_status_buffer_read_ready();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_VIEW_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_buffer_result_view_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_VIEW_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_buffer_result_view_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_VIEW_DENIED:
            return (u64)mmio64_ahci_driver_read_status_buffer_result_view_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_STATUS:
            return (u64)mmio64_ahci_driver_read_status_buffer_result_status();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_BYTES:
            return (u64)mmio64_ahci_driver_read_status_buffer_result_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_buffer_result_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_buffer_read_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_WRITE_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_buffer_write_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_COMMIT_AUTHORITY:
            return (u64)mmio64_ahci_driver_read_status_buffer_commit_authority();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BLOCK_ENDPOINT_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_buffer_block_endpoint_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BLOCK_CAP_MINTED:
            return (u64)mmio64_ahci_driver_read_status_buffer_block_cap_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_FS_MINTED:
            return (u64)mmio64_ahci_driver_read_status_buffer_fs_minted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_MMIO_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_buffer_mmio_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PORT_PROGRAMMED:
            return (u64)mmio64_ahci_driver_read_status_buffer_port_programmed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PUBLISHED:
            return (u64)mmio64_ahci_driver_read_status_buffer_published();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_COMMAND_ISSUED:
            return (u64)mmio64_ahci_driver_read_status_buffer_command_issued();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DMA_MAPPED:
            return (u64)mmio64_ahci_driver_read_status_buffer_dma_mapped();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_ARMED:
            return (u64)mmio64_ahci_driver_read_status_buffer_armed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_MEDIA_READ:
            return (u64)mmio64_ahci_driver_read_status_buffer_media_read();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_MEDIA_WRITTEN:
            return (u64)mmio64_ahci_driver_read_status_buffer_media_written();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_buffer_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_buffer_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_buffer_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_buffer_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_EXPORT:
            return (u64)mmio64_stage_ahci_driver_read_status_export(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_STATE:
            return (u64)mmio64_ahci_driver_read_status_export_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_export_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_export_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_STATUS_BUFFER_TOKEN:
            return (u64)mmio64_ahci_driver_read_status_export_read_status_buffer_token();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_DRIVER_CAPABILITY:
            return (u64)mmio64_ahci_driver_read_status_export_driver_capability();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_export_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_OWNER_BOUND:
            return (u64)mmio64_ahci_driver_read_status_export_owner_bound();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_export_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_PORT:
            return (u64)mmio64_ahci_driver_read_status_export_port();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_KIND:
            return (u64)mmio64_ahci_driver_read_status_export_kind();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_OPERATION:
            return (u64)mmio64_ahci_driver_read_status_export_read_operation();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_LBA:
            return (u64)mmio64_ahci_driver_read_status_export_read_lba();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_BLOCKS:
            return (u64)mmio64_ahci_driver_read_status_export_read_blocks();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_BYTES:
            return (u64)mmio64_ahci_driver_read_status_export_read_bytes();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_export_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_STATUS_BUFFER_SEALED:
            return (u64)mmio64_ahci_driver_read_status_export_status_buffer_sealed();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_EXPORT_REQUESTED:
            return (u64)mmio64_ahci_driver_read_status_export_export_requested();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_EXPORT_GRANTED:
            return (u64)mmio64_ahci_driver_read_status_export_export_granted();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_EXPORT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_export_export_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_export_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_export_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_export_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_export_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_export_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_export_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_export_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_REPORT:
            return (u64)mmio64_stage_ahci_driver_read_status_report(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_STATE:
            return (u64)mmio64_ahci_driver_read_status_report_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_report_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_report_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_report_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_report_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_STATUS_EXPORT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_report_status_export_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_REPORT_MASK:
            return (u64)mmio64_ahci_driver_read_status_report_report_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_report_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_report_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_report_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_report_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_report_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_report_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_report_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RECEIPT:
            return (u64)mmio64_stage_ahci_driver_read_status_receipt(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_STATE:
            return (u64)mmio64_ahci_driver_read_status_receipt_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_receipt_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_receipt_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_receipt_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_receipt_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_STATUS_REPORT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_receipt_status_report_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_RECEIPT_MASK:
            return (u64)mmio64_ahci_driver_read_status_receipt_receipt_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_receipt_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_receipt_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_receipt_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_receipt_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_receipt_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_receipt_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_receipt_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ACK:
            return (u64)mmio64_stage_ahci_driver_read_status_ack(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_STATE:
            return (u64)mmio64_ahci_driver_read_status_ack_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_ack_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_ack_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_ack_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_ack_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_STATUS_RECEIPT_DENIED:
            return (u64)mmio64_ahci_driver_read_status_ack_status_receipt_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_ACK_MASK:
            return (u64)mmio64_ahci_driver_read_status_ack_ack_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_ack_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_ack_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_ack_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_ack_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_ack_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_ack_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_ack_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLOSE:
            return (u64)mmio64_stage_ahci_driver_read_status_close(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_STATE:
            return (u64)mmio64_ahci_driver_read_status_close_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_close_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_close_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_close_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_close_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_STATUS_ACK_DENIED:
            return (u64)mmio64_ahci_driver_read_status_close_status_ack_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_CLOSE_MASK:
            return (u64)mmio64_ahci_driver_read_status_close_close_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_close_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_close_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_close_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_close_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_close_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_close_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_close_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_SEAL:
            return (u64)mmio64_stage_ahci_driver_read_status_seal(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_STATE:
            return (u64)mmio64_ahci_driver_read_status_seal_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_seal_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_seal_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_seal_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_seal_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_STATUS_CLOSE_DENIED:
            return (u64)mmio64_ahci_driver_read_status_seal_status_close_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_SEAL_MASK:
            return (u64)mmio64_ahci_driver_read_status_seal_seal_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_seal_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_seal_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_seal_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_seal_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_seal_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_seal_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_seal_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_UNSEAL:
            return (u64)mmio64_stage_ahci_driver_read_status_unseal(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_STATE:
            return (u64)mmio64_ahci_driver_read_status_unseal_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_unseal_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_unseal_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_unseal_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_unseal_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_STATUS_SEAL_DENIED:
            return (u64)mmio64_ahci_driver_read_status_unseal_status_seal_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_UNSEAL_MASK:
            return (u64)mmio64_ahci_driver_read_status_unseal_unseal_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_unseal_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_unseal_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_unseal_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_unseal_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_unseal_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_unseal_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_unseal_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DISCARD:
            return (u64)mmio64_stage_ahci_driver_read_status_discard(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_STATE:
            return (u64)mmio64_ahci_driver_read_status_discard_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_discard_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_discard_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_discard_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_discard_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_STATUS_UNSEAL_DENIED:
            return (u64)mmio64_ahci_driver_read_status_discard_status_unseal_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_DISCARD_MASK:
            return (u64)mmio64_ahci_driver_read_status_discard_discard_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_discard_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_discard_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_discard_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_discard_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_discard_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_discard_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_discard_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FINALIZE:
            return (u64)mmio64_stage_ahci_driver_read_status_finalize(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_STATE:
            return (u64)mmio64_ahci_driver_read_status_finalize_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_finalize_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_finalize_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_finalize_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_finalize_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_STATUS_DISCARD_DENIED:
            return (u64)mmio64_ahci_driver_read_status_finalize_status_discard_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_FINALIZE_MASK:
            return (u64)mmio64_ahci_driver_read_status_finalize_finalize_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_finalize_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_finalize_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_finalize_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_finalize_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_finalize_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_finalize_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_finalize_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_AUTHORIZE:
            return (u64)mmio64_stage_ahci_driver_read_status_authorize(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_STATE:
            return (u64)mmio64_ahci_driver_read_status_authorize_state();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_FLAGS:
            return (u64)mmio64_ahci_driver_read_status_authorize_flags();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_DRIVER_OWNER:
            return (u64)mmio64_ahci_driver_read_status_authorize_driver_owner();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_QUERY_ONLY:
            return (u64)mmio64_ahci_driver_read_status_authorize_query_only();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_BUFFER_CHECKSUM:
            return (u64)mmio64_ahci_driver_read_status_authorize_buffer_checksum();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_STATUS_FINALIZE_DENIED:
            return (u64)mmio64_ahci_driver_read_status_authorize_status_finalize_denied();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_AUTHORIZE_MASK:
            return (u64)mmio64_ahci_driver_read_status_authorize_authorize_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_USER_COPY_MASK:
            return (u64)mmio64_ahci_driver_read_status_authorize_user_copy_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_AUTHORITY_MASK:
            return (u64)mmio64_ahci_driver_read_status_authorize_authority_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_SIDE_EFFECT_MASK:
            return (u64)mmio64_ahci_driver_read_status_authorize_side_effect_mask();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_BUFFER_UNCHANGED:
            return (u64)mmio64_ahci_driver_read_status_authorize_buffer_unchanged();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_STAGE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_authorize_stage_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_DENIAL_COUNT:
            return (u64)mmio64_ahci_driver_read_status_authorize_denial_count();

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_UNAVAILABLE_COUNT:
            return (u64)mmio64_ahci_driver_read_status_authorize_unavailable_count();

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DISPATCH:
            return (u64)mmio64_stage_ahci_driver_read_status_dispatch(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_dispatch_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_QUEUE:
            return (u64)mmio64_stage_ahci_driver_read_status_queue(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_queue_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_WORKER:
            return (u64)mmio64_stage_ahci_driver_read_status_worker(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_worker_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY:
            return (u64)mmio64_stage_ahci_driver_read_status_read_authority(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_read_authority_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DESCRIPTOR:
            return (u64)mmio64_stage_ahci_driver_read_status_descriptor(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_descriptor_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE:
            return (u64)mmio64_stage_ahci_driver_read_status_command_table(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_command_table_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE:
            return (u64)mmio64_stage_ahci_driver_read_status_command_issue(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_command_issue_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT:
            return (u64)mmio64_stage_ahci_driver_read_status_issue_grant(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_issue_grant_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ARM:
            return (u64)mmio64_stage_ahci_driver_read_status_arm(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_arm_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_EXEC:
            return (u64)mmio64_stage_ahci_driver_read_status_exec(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_exec_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DMA:
            return (u64)mmio64_stage_ahci_driver_read_status_dma(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_dma_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_MMIO:
            return (u64)mmio64_stage_ahci_driver_read_status_mmio(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_mmio_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DMA_WINDOW:
            return (u64)mmio64_stage_ahci_driver_read_status_dma_window(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_dma_window_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_READ:
            return (u64)mmio64_stage_ahci_driver_read_status_read(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_read_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_BLOCK:
            return (u64)mmio64_stage_ahci_driver_read_status_block(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BLOCK_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_block_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FS:
            return (u64)mmio64_stage_ahci_driver_read_status_fs(
                (u32)arg0,
                (u32)arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_fs_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FS_USER:
            return (u64)mmio64_stage_ahci_driver_read_status_fs_user(
                (u32)arg0,
                arg1,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_USER_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_fs_user_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FS_SHELL:
            return (u64)mmio64_stage_ahci_driver_read_status_fs_shell(
                (u32)arg0,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_SHELL_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_fs_shell_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_LOAD:
            return (u64)mmio64_stage_ahci_driver_read_status_load(
                (u32)arg0,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_load_telemetry((u32)arg0);

        case X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_LOAD_FULL:
            return (u64)mmio64_stage_ahci_driver_read_status_load_full(
                (u32)arg0,
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY:
            return (u64)mmio64_ahci_driver_read_status_load_full_telemetry((u32)arg0);

        case X64_SYSCALL_DISPLAY_DRAW_MARKER:
            return (u64)display64_draw_marker(
                (u32)arg0,
                syscall64_pack_low16(arg1),
                syscall64_pack_mid16(arg1),
                syscall64_pack_low32(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_DISPLAY_WRITE_TEXT:
            return (u64)display64_write_text(
                (u32)arg0,
                arg1,
                syscall64_pack_low16(arg2),
                syscall64_pack_high32(arg2));

        case X64_SYSCALL_DISPLAY_CLEAR_TEXT_PANEL:
            return (u64)display64_clear_text_panel((u32)arg0, syscall64_pack_high32(arg2));

        case X64_SYSCALL_DISPLAY_AVAILABLE:
            return (u64)display64_available();

        case X64_SYSCALL_DISPLAY_WIDTH:
            return (u64)display64_width();

        case X64_SYSCALL_DISPLAY_HEIGHT:
            return (u64)display64_height();

        case X64_SYSCALL_DISPLAY_DRAW_COUNT:
            return (u64)display64_draw_count();

        case X64_SYSCALL_DISPLAY_PIXEL_COUNT:
            return (u64)display64_pixel_count();

        case X64_SYSCALL_DISPLAY_DENIAL_COUNT:
            return (u64)display64_denial_count();

        case X64_SYSCALL_DISPLAY_UNAVAILABLE_COUNT:
            return (u64)display64_unavailable_count();

        case X64_SYSCALL_DISPLAY_LAST_TOKEN:
            return (u64)display64_last_token();

        case X64_SYSCALL_DISPLAY_TEXT_WRITE_COUNT:
            return (u64)display64_text_write_count();

        case X64_SYSCALL_DISPLAY_TEXT_BYTE_COUNT:
            return (u64)display64_text_byte_count();

        case X64_SYSCALL_DISPLAY_CLEAR_COUNT:
            return (u64)display64_clear_count();

        case X64_SYSCALL_DISPLAY_CONSOLE_WRITE_COUNT:
            return (u64)display64_console_write_count();

        case X64_SYSCALL_DISPLAY_CONSOLE_BYTE_COUNT:
            return (u64)display64_console_byte_count();

        case X64_SYSCALL_DISPLAY_CONSOLE_LINE_CLEAR_COUNT:
            return (u64)display64_console_line_clear_count();

        case X64_SYSCALL_DISPLAY_CONSOLE_WRAP_COUNT:
            return (u64)display64_console_wrap_count();

        case X64_SYSCALL_DISPLAY_CONSOLE_SCROLL_COUNT:
            return (u64)display64_console_scroll_count();

        case X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_DENIAL:
            return (u64)process64_runtime_user_entry_denial((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_OFFSET:
            return (u64)process64_runtime_payload_offset((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_SIZE:
            return (u64)process64_runtime_payload_size((u32)arg0);

        case X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_CHECKSUM:
            return (u64)process64_runtime_payload_checksum((u32)arg0);

        case X64_SYSCALL_PROCESS_MANIFEST_VERIFIED_COUNT:
            return (u64)process64_manifest_verified_count();

        case X64_SYSCALL_LAUNCH_ARCHIVE_VALID:
            return (u64)launch64_archive_valid();

        case X64_SYSCALL_LAUNCH_ARCHIVE_CHECKSUM:
            return (u64)launch64_archive_checksum();

        case X64_SYSCALL_LAUNCH_MANIFEST_TOTAL:
            return (u64)launch64_manifest_total_count();

        case X64_SYSCALL_LAUNCH_MANIFEST_COUNT:
            return (u64)launch64_manifest_count();

        case X64_SYSCALL_LAUNCH_MANIFEST_IGNORED:
            return (u64)launch64_manifest_ignored_count();

        case X64_SYSCALL_LAUNCH_MANIFEST_DENIED:
            return (u64)launch64_manifest_denial_count();

        case X64_SYSCALL_LAUNCH_READY_COUNT:
            return (u64)launch64_service_ready_count();

        case X64_SYSCALL_LAUNCH_STARTED_COUNT:
            return (u64)launch64_service_started_count();

        case X64_SYSCALL_LAUNCH_START_DENIAL_COUNT:
            return (u64)launch64_service_start_denial_count();

        case X64_SYSCALL_LAUNCH_MANIFEST_STATE:
            return (u64)launch64_manifest_launch_state((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_PID:
            return (u64)launch64_manifest_launched_pid((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_PRINCIPAL:
            return (u64)launch64_manifest_launched_principal((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_ENDPOINT_CLASS:
            return (u64)launch64_manifest_launched_endpoint_class((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_LAST_DENIAL:
            return (u64)launch64_manifest_last_denial((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_COUNT:
            return (u64)launch64_service_start_request_count();

        case X64_SYSCALL_LAUNCH_APPROVAL_COUNT:
            return (u64)launch64_service_start_approval_count();

        case X64_SYSCALL_LAUNCH_REQUESTER_CAN_START:
            return (u64)launch64_requester_can_start((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUESTER:
            return (u64)launch64_manifest_last_requester((u32)arg0);

        case X64_SYSCALL_LAUNCH_PENDING_COUNT:
            return (u64)launch64_service_start_pending_count();

        case X64_SYSCALL_LAUNCH_DENIED_COUNT:
            return (u64)launch64_service_start_denied_count();

        case X64_SYSCALL_LAUNCH_COMPLETED_COUNT:
            return (u64)launch64_service_start_completed_count();

        case X64_SYSCALL_LAUNCH_REQUEST_LOG_COUNT:
            return (u64)launch64_request_log_count();

        case X64_SYSCALL_LAUNCH_REQUEST_ID_BY_INDEX:
            return (u64)launch64_request_id_by_index((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_STATUS:
            return (u64)launch64_request_status((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_MANIFEST:
            return (u64)launch64_request_manifest((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_REQUESTER:
            return (u64)launch64_request_requester((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_DENIAL:
            return (u64)launch64_request_denial((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID:
            return (u64)launch64_manifest_last_request_id((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_STATUS:
            return (u64)launch64_manifest_last_request_status((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_OPERATION:
            return (u64)launch64_request_operation((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUESTER_CAN_STOP:
            return (u64)launch64_requester_can_stop((u32)arg0);

        case X64_SYSCALL_LAUNCH_STOP_REQUEST_COUNT:
            return (u64)launch64_service_stop_request_count();

        case X64_SYSCALL_LAUNCH_STOP_APPROVAL_COUNT:
            return (u64)launch64_service_stop_approval_count();

        case X64_SYSCALL_LAUNCH_STOP_PENDING_COUNT:
            return (u64)launch64_service_stop_pending_count();

        case X64_SYSCALL_LAUNCH_STOP_DENIED_COUNT:
            return (u64)launch64_service_stop_denied_count();

        case X64_SYSCALL_LAUNCH_STOP_COMPLETED_COUNT:
            return (u64)launch64_service_stop_completed_count();

        case X64_SYSCALL_LAUNCH_REQUEST_STOP:
            return (u64)launch64_request_service_stop((u32)arg0, (u32)arg1);

        case X64_SYSCALL_CAP_LIVE_FOR_ENDPOINT_CLASS:
            return (u64)capability64_live_for_endpoint_class((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_QUIESCE:
            return (u64)launch64_request_service_quiesce((u32)arg0, (u32)arg1);

        case X64_SYSCALL_LAUNCH_REQUESTER_CAN_QUIESCE:
            return (u64)launch64_requester_can_quiesce((u32)arg0);

        case X64_SYSCALL_LAUNCH_QUIESCE_REQUEST_COUNT:
            return (u64)launch64_service_quiesce_request_count();

        case X64_SYSCALL_LAUNCH_QUIESCE_APPROVAL_COUNT:
            return (u64)launch64_service_quiesce_approval_count();

        case X64_SYSCALL_LAUNCH_QUIESCE_PENDING_COUNT:
            return (u64)launch64_service_quiesce_pending_count();

        case X64_SYSCALL_LAUNCH_QUIESCE_DENIED_COUNT:
            return (u64)launch64_service_quiesce_denied_count();

        case X64_SYSCALL_LAUNCH_QUIESCE_COMPLETED_COUNT:
            return (u64)launch64_service_quiesce_completed_count();

        case X64_SYSCALL_LAUNCH_REQUEST_OBSERVED_CAPABILITIES:
            return (u64)launch64_request_observed_capabilities((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_DRAIN:
            return (u64)launch64_request_service_drain((u32)arg0, (u32)arg1);

        case X64_SYSCALL_LAUNCH_REQUESTER_CAN_DRAIN:
            return (u64)launch64_requester_can_drain((u32)arg0);

        case X64_SYSCALL_LAUNCH_DRAIN_REQUEST_COUNT:
            return (u64)launch64_service_drain_request_count();

        case X64_SYSCALL_LAUNCH_DRAIN_APPROVAL_COUNT:
            return (u64)launch64_service_drain_approval_count();

        case X64_SYSCALL_LAUNCH_DRAIN_PENDING_COUNT:
            return (u64)launch64_service_drain_pending_count();

        case X64_SYSCALL_LAUNCH_DRAIN_DENIED_COUNT:
            return (u64)launch64_service_drain_denied_count();

        case X64_SYSCALL_LAUNCH_DRAIN_COMPLETED_COUNT:
            return (u64)launch64_service_drain_completed_count();

        case X64_SYSCALL_LAUNCH_REQUEST_REVOKED_CAPABILITIES:
            return (u64)launch64_request_revoked_capabilities((u32)arg0);

        case X64_SYSCALL_LAUNCH_DRAINED_COUNT:
            return (u64)launch64_service_drained_count();

        case X64_SYSCALL_LAUNCH_QUIESCE_READY_COUNT:
            return (u64)launch64_service_quiesce_ready_count();

        case X64_SYSCALL_LAUNCH_MANIFEST_PHASE:
            return (u64)launch64_manifest_lifecycle_phase((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RESTART:
            return (u64)launch64_request_service_restart((u32)arg0, (u32)arg1);

        case X64_SYSCALL_LAUNCH_REQUESTER_CAN_RESTART:
            return (u64)launch64_requester_can_restart((u32)arg0);

        case X64_SYSCALL_LAUNCH_RESTART_REQUEST_COUNT:
            return (u64)launch64_service_restart_request_count();

        case X64_SYSCALL_LAUNCH_RESTART_APPROVAL_COUNT:
            return (u64)launch64_service_restart_approval_count();

        case X64_SYSCALL_LAUNCH_RESTART_PENDING_COUNT:
            return (u64)launch64_service_restart_pending_count();

        case X64_SYSCALL_LAUNCH_RESTART_DENIED_COUNT:
            return (u64)launch64_service_restart_denied_count();

        case X64_SYSCALL_LAUNCH_RESTART_COMPLETED_COUNT:
            return (u64)launch64_service_restart_completed_count();

        case X64_SYSCALL_LAUNCH_MANIFEST_RESTART_COUNT:
            return (u64)launch64_manifest_restart_count((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_GENERATION:
            return (u64)launch64_manifest_runtime_generation((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_TOKEN:
            return (u64)launch64_manifest_runtime_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_GENERATION:
            return (u64)launch64_request_runtime_generation((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_TOKEN:
            return (u64)launch64_request_runtime_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_ACCEPTS_RUNTIME_TOKEN:
            return (u64)launch64_manifest_accepts_runtime_token((u32)arg0, (u32)arg1);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_GENERATION:
            return (u64)launch64_manifest_runtime_image_generation((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_TOKEN:
            return (u64)launch64_manifest_runtime_image_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_BASE:
            return (u64)launch64_manifest_runtime_image_base((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_ENTRY:
            return (u64)launch64_manifest_runtime_image_entry((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAPPED_BYTES:
            return (u64)launch64_manifest_runtime_image_mapped_bytes((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_RIGHTS:
            return (u64)launch64_manifest_runtime_image_rights((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PLAN_TOKEN:
            return (u64)launch64_manifest_runtime_image_plan_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAP_TOKEN:
            return (u64)launch64_manifest_runtime_image_map_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PAGE_COUNT:
            return (u64)launch64_manifest_runtime_image_page_count((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PML4_INDEX:
            return (u64)launch64_manifest_runtime_image_pml4_index((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PDPT_INDEX:
            return (u64)launch64_manifest_runtime_image_pdpt_index((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PD_INDEX:
            return (u64)launch64_manifest_runtime_image_pd_index((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_ENTRY_TRANSFER_TOKEN:
            return (u64)launch64_manifest_runtime_entry_transfer_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_INSTALL_TOKEN:
            return (u64)launch64_manifest_runtime_image_install_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_SOURCE_CHECKSUM:
            return (u64)launch64_manifest_runtime_image_source_checksum((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_ENTRY_PROBE:
            return (u64)launch64_manifest_runtime_image_entry_probe((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAP_INSTALLED:
            return (u64)launch64_manifest_runtime_image_map_installed((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PROTECTION_FLAGS:
            return (u64)launch64_manifest_runtime_image_protection_flags((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PROTECTION_TOKEN:
            return (u64)launch64_manifest_runtime_image_protection_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_USER_ENTRY_STATE:
            return (u64)launch64_manifest_runtime_user_entry_state((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_USER_ENTRY_TOKEN:
            return (u64)launch64_manifest_runtime_user_entry_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_USER_ENTRY_RIP:
            return (u64)launch64_manifest_runtime_user_entry_rip((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_USER_ENTRY_RSP:
            return (u64)launch64_manifest_runtime_user_entry_rsp((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_USER_ENTRY_SELECTORS:
            return (u64)launch64_manifest_runtime_user_entry_selectors((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_USER_ENTRY_RFLAGS:
            return (u64)launch64_manifest_runtime_user_entry_rflags((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_USER_ENTRY_DENIAL:
            return (u64)launch64_manifest_runtime_user_entry_denial((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_SLOT:
            return (u64)launch64_manifest_runtime_payload_slot((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_KIND:
            return (u64)launch64_manifest_runtime_payload_kind((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_OFFSET:
            return (u64)launch64_manifest_runtime_payload_offset((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_SIZE:
            return (u64)launch64_manifest_runtime_payload_size((u32)arg0);

        case X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_CHECKSUM:
            return (u64)launch64_manifest_runtime_payload_checksum((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_GENERATION:
            return (u64)launch64_request_runtime_image_generation((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_TOKEN:
            return (u64)launch64_request_runtime_image_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_BASE:
            return (u64)launch64_request_runtime_image_base((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_ENTRY:
            return (u64)launch64_request_runtime_image_entry((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_MAPPED_BYTES:
            return (u64)launch64_request_runtime_image_mapped_bytes((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_RIGHTS:
            return (u64)launch64_request_runtime_image_rights((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PLAN_TOKEN:
            return (u64)launch64_request_runtime_image_plan_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_MAP_TOKEN:
            return (u64)launch64_request_runtime_image_map_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PAGE_COUNT:
            return (u64)launch64_request_runtime_image_page_count((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PML4_INDEX:
            return (u64)launch64_request_runtime_image_pml4_index((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PDPT_INDEX:
            return (u64)launch64_request_runtime_image_pdpt_index((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PD_INDEX:
            return (u64)launch64_request_runtime_image_pd_index((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_ENTRY_TRANSFER_TOKEN:
            return (u64)launch64_request_runtime_entry_transfer_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_INSTALL_TOKEN:
            return (u64)launch64_request_runtime_image_install_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_SOURCE_CHECKSUM:
            return (u64)launch64_request_runtime_image_source_checksum((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_ENTRY_PROBE:
            return (u64)launch64_request_runtime_image_entry_probe((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_MAP_INSTALLED:
            return (u64)launch64_request_runtime_image_map_installed((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PROTECTION_FLAGS:
            return (u64)launch64_request_runtime_image_protection_flags((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PROTECTION_TOKEN:
            return (u64)launch64_request_runtime_image_protection_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_USER_ENTRY_STATE:
            return (u64)launch64_request_runtime_user_entry_state((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_USER_ENTRY_TOKEN:
            return (u64)launch64_request_runtime_user_entry_token((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_USER_ENTRY_RIP:
            return (u64)launch64_request_runtime_user_entry_rip((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_USER_ENTRY_RSP:
            return (u64)launch64_request_runtime_user_entry_rsp((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_USER_ENTRY_SELECTORS:
            return (u64)launch64_request_runtime_user_entry_selectors((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_USER_ENTRY_RFLAGS:
            return (u64)launch64_request_runtime_user_entry_rflags((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_USER_ENTRY_DENIAL:
            return (u64)launch64_request_runtime_user_entry_denial((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_SLOT:
            return (u64)launch64_request_runtime_payload_slot((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_KIND:
            return (u64)launch64_request_runtime_payload_kind((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_OFFSET:
            return (u64)launch64_request_runtime_payload_offset((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_SIZE:
            return (u64)launch64_request_runtime_payload_size((u32)arg0);

        case X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_CHECKSUM:
            return (u64)launch64_request_runtime_payload_checksum((u32)arg0);

        case X64_SYSCALL_DESCRIPTOR_STATE:
            return (u64)descriptors64_state();

        case X64_SYSCALL_DESCRIPTOR_GDT_TOKEN:
            return (u64)descriptors64_gdt_token();

        case X64_SYSCALL_DESCRIPTOR_TSS_TOKEN:
            return (u64)descriptors64_tss_token();

        case X64_SYSCALL_DESCRIPTOR_KERNEL_CODE:
            return (u64)descriptors64_kernel_code_selector();

        case X64_SYSCALL_DESCRIPTOR_KERNEL_DATA:
            return (u64)descriptors64_kernel_data_selector();

        case X64_SYSCALL_DESCRIPTOR_USER_CODE:
            return (u64)descriptors64_user_code_selector();

        case X64_SYSCALL_DESCRIPTOR_USER_DATA:
            return (u64)descriptors64_user_data_selector();

        case X64_SYSCALL_DESCRIPTOR_TSS_SELECTOR:
            return (u64)descriptors64_tss_selector();

        case X64_SYSCALL_DESCRIPTOR_TSS_RSP0:
            return descriptors64_tss_rsp0();

        case X64_SYSCALL_DESCRIPTOR_SYSCALL_STAR_PLAN:
            return descriptors64_syscall_star_plan();

        case X64_SYSCALL_NATIVE_SYSCALL_STAR_VALUE:
            return syscall64_native_star_value();

        case X64_SYSCALL_NATIVE_SYSCALL_STAR_READY:
            return (u64)syscall64_native_star_ready();

        default:
            return 0xFFFFFFFFFFFFFFFFull;
    }
}

u64 syscall64_native_dispatch(u64 number, u64 arg0, u64 arg1, u64 arg2)
{
    ++g_native_syscall_count;
    g_native_last_syscall_code = number;
    return syscall64_dispatch(number, arg0, arg1, arg2);
}

u32 syscall64_native_count(void)
{
    return g_native_syscall_count;
}

u64 syscall64_native_last_code(void)
{
    return g_native_last_syscall_code;
}

u64 syscall64_native_star_value(void)
{
    return g_native_star_value;
}

u32 syscall64_native_star_ready(void)
{
    return g_native_star_ready;
}
