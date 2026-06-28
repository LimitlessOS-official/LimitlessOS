/* Split scaffold fragment. Real code is unity-included by scaffold.c; direct compilation emits only the anchor below. */

#if defined(LIMITLESS_SCAFFOLD_STORAGE_USER_FS_LOAD)
static void run_user_entry_filesystem_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u64 fs_rip = 0ull;
    u32 result = 0u;

    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        fs_rip = (u64)LAUNCH64_USER_IMAGE_BASE
            + (u64)runtime64_transfer_user_fs_probe_offset();
        result = interrupts64_trigger_user_entry_probe(
            fs_rip,
            launch64_manifest_runtime_user_entry_rsp(policy_manifest),
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            launch64_manifest_runtime_user_entry_rflags(policy_manifest));
    }

    write_string("[x64] user fs probe attempts ");
    write_dec_u32(interrupts64_user_entry_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_entry_probe_exits());
    write_labeled_hex_u32(" result ", result);
    write_string(" recorded ");
    write_hex_u32(interrupts64_user_entry_probe_result());
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_fs_result());
    write_string(" read-bytes ");
    write_dec_u32(runtime64_transfer_user_fs_read_bytes());
    write_string(" rip ");
    write_hex_u64(interrupts64_user_entry_probe_rip());
    write_string(" rsp ");
    write_hex_u64(interrupts64_user_entry_probe_rsp());
    write_string(" cs ");
    write_hex_u64(interrupts64_user_entry_probe_cs());
    write_string(" ss ");
    write_hex_u64(interrupts64_user_entry_probe_ss());
    write_line("");
}

static void log_brokered_keyboard_read_probe(void)
{
    u32 pending_before = (u32)syscall64_invoke(X64_SYSCALL_INPUT_KEYBOARD_PENDING_COUNT, 0u, 0u, 0u);
    u32 input_endpoint = services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT);
    u32 input_capability = (u32)syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        input_endpoint,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        PRINCIPAL64_ID_CONSOLE_CLIENT);
    u8 keyboard_byte = 0u;
    u32 result = 0u;

    if (pending_before == 0u)
    {
        collect_keyboard_probe_input(1u, 120u);
        pending_before = (u32)syscall64_invoke(X64_SYSCALL_INPUT_KEYBOARD_PENDING_COUNT, 0u, 0u, 0u);
    }

    if ((pending_before > 0u) && (input_capability != CAPABILITY64_INVALID_HANDLE))
    {
        result = (u32)syscall64_invoke(
            X64_SYSCALL_INPUT_READ_KEYBOARD,
            input_capability,
            (u64)&keyboard_byte,
            ((u64)PRINCIPAL64_ID_CONSOLE_CLIENT << 32) | 1ull);
    }

    write_labeled_hex_u32("[x64] brokered keyboard read cap ", input_capability);
    write_labeled_dec_u32(" result ", result);
    write_string(" first-byte ");
    write_hex_u32((u32)keyboard_byte);
    write_labeled_dec_u32(" pending-before ", pending_before);
    write_syscall0_dec_u32(" pending-after ", X64_SYSCALL_INPUT_KEYBOARD_PENDING_COUNT);
    write_syscall0_dec_u32(" keyboard-reads ", X64_SYSCALL_INPUT_KEYBOARD_READ_COUNT);
    write_syscall0_dec_u32(" keyboard-read-bytes ", X64_SYSCALL_INPUT_KEYBOARD_READ_BYTE_COUNT);
    write_line("");
}

static void run_user_entry_second_page_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u64 second_page_rip = 0ull;
    u32 result = 0u;

    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        second_page_rip = (u64)LAUNCH64_USER_IMAGE_BASE
            + (u64)runtime64_transfer_user_second_page_offset();
        result = interrupts64_trigger_user_entry_probe(
            second_page_rip,
            launch64_manifest_runtime_user_entry_rsp(policy_manifest),
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            launch64_manifest_runtime_user_entry_rflags(policy_manifest));
    }

    write_string("[x64] user second-page probe attempts ");
    write_dec_u32(interrupts64_user_entry_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_entry_probe_exits());
    write_labeled_hex_u32(" result ", result);
    write_string(" recorded ");
    write_hex_u32(interrupts64_user_entry_probe_result());
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_second_page_result());
    write_string(" mapped-bytes ");
    write_dec_u32(process64_runtime_image_mapped_bytes(policy_pid));
    write_string(" page-count ");
    write_dec_u32(process64_runtime_image_page_count(policy_pid));
    write_string(" offset ");
    write_hex_u32(runtime64_transfer_user_second_page_offset());
    write_string(" note-path-bytes ");
    write_dec_u32(runtime64_transfer_user_second_page_note_path_bytes());
    write_string(" note-bytes ");
    write_dec_u32(runtime64_transfer_user_second_page_note_bytes());
    static const char syscall0_suffixes_2[] =
        "fs-creates \0"
        "fs-writes \0"
        "fs-reads \0"
        "display-pixels \0"
        "display-draws \0"
        "display-denials \0"
        "display-unavailable \0"
        "display-token \0"
        "display-available \0"
        "display-text-writes \0"
        "display-text-bytes \0"
        "display-clears \0"
        "display-console-writes \0"
        "display-console-bytes \0"
        "display-console-line-clears \0"
        "display-console-wraps \0"
        "display-console-scrolls \0";
    static const struct scaffold_syscall0_field syscall0_fields_2[] = {        {0, X64_SYSCALL_FS_CREATE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {12, X64_SYSCALL_FS_WRITE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {23, X64_SYSCALL_FS_READ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {33, X64_SYSCALL_DISPLAY_PIXEL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {49, X64_SYSCALL_DISPLAY_DRAW_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {64, X64_SYSCALL_DISPLAY_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {81, X64_SYSCALL_DISPLAY_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {102, X64_SYSCALL_DISPLAY_LAST_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {117, X64_SYSCALL_DISPLAY_AVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {136, X64_SYSCALL_DISPLAY_TEXT_WRITE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {157, X64_SYSCALL_DISPLAY_TEXT_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {177, X64_SYSCALL_DISPLAY_CLEAR_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {193, X64_SYSCALL_DISPLAY_CONSOLE_WRITE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {217, X64_SYSCALL_DISPLAY_CONSOLE_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {240, X64_SYSCALL_DISPLAY_CONSOLE_LINE_CLEAR_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {269, X64_SYSCALL_DISPLAY_CONSOLE_WRAP_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {292, X64_SYSCALL_DISPLAY_CONSOLE_SCROLL_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" ", syscall0_suffixes_2, syscall0_fields_2, (u32)(sizeof(syscall0_fields_2) / sizeof(syscall0_fields_2[0])));
    write_string(" rip ");
    write_hex_u64(interrupts64_user_entry_probe_rip());
    write_string(" rsp ");
    write_hex_u64(interrupts64_user_entry_probe_rsp());
    write_string(" cs ");
    write_hex_u64(interrupts64_user_entry_probe_cs());
    write_string(" ss ");
    write_hex_u64(interrupts64_user_entry_probe_ss());
    write_line("");
}

static void run_drs_load_probe(void)
{
    u64 driver_owner_arg = ((u64)PRINCIPAL64_ID_BLOCK_WORKER << 32);
    u32 fs_shell_token = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_SHELL_TELEMETRY,
        MMIO64_AHCI_DRIVER_READ_STATUS_FS_SHELL_TELEMETRY_TOKEN,
        0u,
        0u);
    u32 drs_load = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_LOAD,
        fs_shell_token,
        0u,
        driver_owner_arg);
    u32 mapped = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY,
        MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_MAPPED,
        0u,
        0u);

    if (mapped != 0u)
    {
        u32 result = interrupts64_trigger_user_entry_probe(
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_ENTRY_RIP,
                0u,
                0u),
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_ENTRY_RSP,
                0u,
                0u),
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_ENTRY_SELECTORS,
                0u,
                0u),
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_ENTRY_RFLAGS,
                0u,
                0u));
        drs_load = mmio64_ahci_driver_read_status_load_record_launch(
            result,
            interrupts64_user_entry_probe_aux());
    }

    write_string("[x64]");
    write_labeled_hex_u32(" drs-load ", drs_load);
    write_drs_load_fields(
        " drs-load-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY);
    write_line("");
}

static void run_drs_load_full_command(u32 command_id)
{
    if (mmio64_ahci_driver_read_status_load_full_prepare_launch(command_id) == 0u)
    {
        return;
    }

    mmio64_ahci_driver_read_status_load_full_record_launch(
        interrupts64_trigger_user_entry_probe(
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_ENTRY_RIP,
                0u,
                0u),
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_ENTRY_RSP,
                0u,
                0u),
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_ENTRY_SELECTORS,
                0u,
                0u),
            (u64)(u32)syscall64_invoke(
                X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY,
                MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_ENTRY_RFLAGS,
                0u,
                0u)),
        interrupts64_user_entry_probe_aux());
}

static void run_drs_load_full_probe(void)
{
    u64 driver_owner_arg = ((u64)PRINCIPAL64_ID_BLOCK_WORKER << 32);
    u32 load_token = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY,
        MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_TOKEN,
        0u,
        0u);
    u32 drs_load_full = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_LOAD_FULL,
        load_token,
        0u,
        driver_owner_arg);
    u32 ready = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY,
        MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_REGISTERED,
        0u,
        0u);

    if (ready == 10u)
    {
        run_drs_load_full_command(MMIO64_DRS_LOAD_FULL_COMMAND_CAT);
        run_drs_load_full_command(MMIO64_DRS_LOAD_FULL_COMMAND_MKDIR);
        run_drs_load_full_command(MMIO64_DRS_LOAD_FULL_COMMAND_WRITE);
        run_drs_load_full_command(MMIO64_DRS_LOAD_FULL_COMMAND_RENAME);
        run_drs_load_full_command(MMIO64_DRS_LOAD_FULL_COMMAND_MOVE);
        drs_load_full = (u32)syscall64_invoke(
            X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY,
            MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_TOKEN,
            0u,
            0u);
    }

    write_string("[x64]");
    write_labeled_hex_u32(" drs-load-full ", drs_load_full);
    write_drs_load_full_fields(
        " drs-load-full-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY);
    write_line("");
}

#endif /* LIMITLESS_SCAFFOLD_STORAGE_USER_FS_LOAD */

#if defined(LIMITLESS_SCAFFOLD_STORAGE_PCI_AHCI_DRS)
static void log_pci_storage_surface(void)
{
    static const char drs_fs_user_path[] = "/APPS/LS.APP";
    u32 owner = PRINCIPAL64_ID_DRIVER_HOST;
    u32 wrong_owner = PRINCIPAL64_ID_POLICY_CLIENT;
    u64 owner_arg = ((u64)owner << 32);
    u64 wrong_owner_arg = ((u64)wrong_owner << 32);
    u64 read_plan_arg = ((u64)owner << 32) | 1ull;
    u64 wrong_read_plan_arg = ((u64)wrong_owner << 32) | 1ull;
    u64 driver_probe_owner_arg = ((u64)PRINCIPAL64_ID_BLOCK_WORKER << 32);
    u64 denied_handoff_arg = ((u64)wrong_owner << 32) | (u64)PRINCIPAL64_ID_BLOCK_WORKER;
    u64 handoff_arg = ((u64)PRINCIPAL64_ID_INIT_SUPERVISOR << 32) | (u64)PRINCIPAL64_ID_BLOCK_WORKER;
    u32 endpoint = (u32)syscall64_invoke(
        X64_SYSCALL_RESOLVE_SERVICE_CLASS,
        SERVICE_ENDPOINT_CLASS_HARDWARE,
        0u,
        0u);
    u32 cap = (u32)syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_HARDWARE,
        CAPABILITY64_RIGHT_QUERY,
        owner);
    u32 denied_devices = (u32)syscall64_invoke(
        X64_SYSCALL_PCI_DEVICE_COUNT,
        cap,
        0u,
        wrong_owner_arg);
    u32 denied_mmio_base = (u32)syscall64_invoke(
        X64_SYSCALL_PCI_FIRST_AHCI_MMIO_BASE,
        cap,
        0u,
        wrong_owner_arg);
    u32 denied_nvme_probe = mmio64_probe_nvme_controller(cap, wrong_owner);
    u32 nvme_probe = mmio64_probe_nvme_controller(cap, owner);
    u32 denied_nvme_read = mmio64_read_nvme_lba0(nvme_probe, cap, wrong_owner);
    u32 nvme_read = mmio64_read_nvme_lba0(nvme_probe, cap, owner);
    u32 denied_nvme_gpt = mmio64_scan_nvme_gpt(nvme_read, cap, wrong_owner);
    u32 nvme_gpt = mmio64_scan_nvme_gpt(nvme_read, cap, owner);
    u32 denied_nvme_fat = mmio64_scan_nvme_fat(nvme_gpt, cap, wrong_owner);
    u32 nvme_fat = mmio64_scan_nvme_fat(nvme_gpt, cap, owner);
    u32 denied_plan_base = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_BASE,
        cap,
        0u,
        wrong_owner_arg);
    u32 denied_map_request = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_REQUEST_AHCI_MAPPING,
        cap,
        0u,
        wrong_owner_arg);
    u32 map_request = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_REQUEST_AHCI_MAPPING,
        cap,
        0u,
        owner_arg);
    u32 denied_snapshot_request = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_SNAPSHOT_AHCI_REGISTERS,
        cap,
        0u,
        wrong_owner_arg);
    u32 snapshot_request = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_SNAPSHOT_AHCI_REGISTERS,
        cap,
        0u,
        owner_arg);
    u32 denied_port_snapshot = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_SNAPSHOT_AHCI_PORTS,
        cap,
        0u,
        wrong_owner_arg);
    u32 port_snapshot = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_SNAPSHOT_AHCI_PORTS,
        cap,
        0u,
        owner_arg);
    u32 denied_port_policy = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_CLASSIFY_AHCI_PORT,
        cap,
        0u,
        wrong_owner_arg);
    u32 port_policy = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_CLASSIFY_AHCI_PORT,
        cap,
        0u,
        owner_arg);
    u32 denied_read_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_READ_PLAN,
        cap,
        0u,
        wrong_read_plan_arg);
    u32 read_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_READ_PLAN,
        cap,
        0u,
        read_plan_arg);
    u32 denied_command_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_COMMAND_PLAN,
        cap,
        0u,
        wrong_owner_arg);
    u32 command_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_COMMAND_PLAN,
        cap,
        0u,
        owner_arg);
    u32 denied_memory_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_MEMORY_PLAN,
        cap,
        0u,
        wrong_owner_arg);
    u32 memory_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_MEMORY_PLAN,
        cap,
        0u,
        owner_arg);
    u32 denied_table_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_PREPARE_AHCI_COMMAND_TABLE,
        cap,
        0u,
        wrong_owner_arg);
    u32 table_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_PREPARE_AHCI_COMMAND_TABLE,
        cap,
        0u,
        owner_arg);
    u32 denied_issue_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_PREFLIGHT_AHCI_COMMAND_ISSUE,
        cap,
        0u,
        wrong_owner_arg);
    u32 issue_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_PREFLIGHT_AHCI_COMMAND_ISSUE,
        cap,
        0u,
        owner_arg);
    u32 denied_bind_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_ADDRESS_BIND_PLAN,
        cap,
        0u,
        wrong_owner_arg);
    u32 bind_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_ADDRESS_BIND_PLAN,
        cap,
        0u,
        owner_arg);
    u32 denied_patch_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_APPLY_AHCI_PRIVATE_ADDRESS_PATCH,
        cap,
        0u,
        wrong_owner_arg);
    u32 patch_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_APPLY_AHCI_PRIVATE_ADDRESS_PATCH,
        cap,
        0u,
        owner_arg);
    u32 denied_publish_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_CONTROLLER_PUBLISH_PLAN,
        cap,
        0u,
        wrong_owner_arg);
    u32 publish_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_CONTROLLER_PUBLISH_PLAN,
        cap,
        0u,
        owner_arg);
    u32 denied_publish_gate = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_GATE_AHCI_CONTROLLER_PUBLICATION,
        cap,
        0u,
        wrong_owner_arg);
    u32 publish_gate = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_GATE_AHCI_CONTROLLER_PUBLICATION,
        cap,
        0u,
        owner_arg);
    u32 denied_window_policy = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_WRITE_WINDOW_POLICY,
        cap,
        0u,
        wrong_owner_arg);
    u32 window_policy = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_WRITE_WINDOW_POLICY,
        cap,
        0u,
        owner_arg);
    u32 denied_revoke_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_REVOCATION_PLAN,
        cap,
        0u,
        wrong_owner_arg);
    u32 revoke_plan = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_REVOCATION_PLAN,
        cap,
        0u,
        owner_arg);
    u32 denied_open_window = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_TRY_OPEN_AHCI_PUBLISH_WRITE_WINDOW,
        cap,
        0u,
        wrong_owner_arg);
    u32 open_window = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_TRY_OPEN_AHCI_PUBLISH_WRITE_WINDOW,
        cap,
        0u,
        owner_arg);
    u32 denied_session = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_REQUEST_AHCI_PUBLISH_EXCLUSIVE_SESSION,
        cap,
        0u,
        wrong_owner_arg);
    u32 session = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_REQUEST_AHCI_PUBLISH_EXCLUSIVE_SESSION,
        cap,
        0u,
        owner_arg);
    u32 denied_drain = 0xFFFFFFFFu;
    u32 drain = 0xFFFFFFFFu;
    u32 denied_handoff = 0xFFFFFFFFu;
    u32 handoff = 0xFFFFFFFFu;
    u32 driver_probe_cap = 0xFFFFFFFFu;
    u32 denied_driver_probe = 0xFFFFFFFFu;
    u32 driver_probe = 0xFFFFFFFFu;
    u32 denied_driver_intent = 0xFFFFFFFFu;
    u32 driver_intent = 0xFFFFFFFFu;
    u32 denied_driver_buffer = 0xFFFFFFFFu;
    u32 driver_buffer = 0xFFFFFFFFu;
    u32 denied_driver_gate = 0xFFFFFFFFu;
    u32 driver_gate = 0xFFFFFFFFu;
    u32 denied_driver_exec = 0xFFFFFFFFu;
    u32 driver_exec = 0xFFFFFFFFu;
    u32 denied_driver_result = 0xFFFFFFFFu;
    u32 driver_result = 0xFFFFFFFFu;
    u32 denied_driver_publish = 0xFFFFFFFFu;
    u32 driver_publish = 0xFFFFFFFFu;
    u32 denied_driver_read_grant = 0xFFFFFFFFu;
    u32 driver_read_grant = 0xFFFFFFFFu;
    u32 denied_driver_media_read = 0xFFFFFFFFu;
    u32 driver_media_read = 0xFFFFFFFFu;
    u32 denied_driver_complete = 0xFFFFFFFFu;
    u32 driver_complete = 0xFFFFFFFFu;
    u32 denied_driver_read_cap = 0xFFFFFFFFu;
    u32 driver_read_cap = 0xFFFFFFFFu;
    u32 denied_driver_read_export = 0xFFFFFFFFu;
    u32 driver_read_export = 0xFFFFFFFFu;
    u32 denied_driver_read_response = 0xFFFFFFFFu;
    u32 driver_read_response = 0xFFFFFFFFu;
    u32 denied_driver_read_delivery = 0xFFFFFFFFu;
    u32 driver_read_delivery = 0xFFFFFFFFu;
    u32 denied_driver_read_visible = 0xFFFFFFFFu;
    u32 driver_read_visible = 0xFFFFFFFFu;
    u32 denied_driver_read_commit = 0xFFFFFFFFu;
    u32 driver_read_commit = 0xFFFFFFFFu;
    u32 denied_driver_read_audit = 0xFFFFFFFFu;
    u32 driver_read_audit = 0xFFFFFFFFu;
    u32 denied_driver_read_upgrade = 0xFFFFFFFFu;
    u32 driver_read_upgrade = 0xFFFFFFFFu;
    u32 denied_driver_read_activate = 0xFFFFFFFFu;
    u32 driver_read_activate = 0xFFFFFFFFu;
    u32 denied_driver_read_arm = 0xFFFFFFFFu;
    u32 driver_read_arm = 0xFFFFFFFFu;
    u32 denied_driver_read_submit = 0xFFFFFFFFu;
    u32 driver_read_submit = 0xFFFFFFFFu;
    u32 denied_driver_read_observe = 0xFFFFFFFFu;
    u32 driver_read_observe = 0xFFFFFFFFu;
    u32 denied_driver_read_retire = 0xFFFFFFFFu;
    u32 driver_read_retire = 0xFFFFFFFFu;
    u32 denied_driver_read_permit = 0xFFFFFFFFu;
    u32 driver_read_permit = 0xFFFFFFFFu;
    u32 denied_driver_read_window = 0xFFFFFFFFu;
    u32 driver_read_window = 0xFFFFFFFFu;
    u32 denied_driver_read_lease = 0xFFFFFFFFu;
    u32 driver_read_lease = 0xFFFFFFFFu;
    u32 denied_driver_read_use = 0xFFFFFFFFu;
    u32 driver_read_use = 0xFFFFFFFFu;
    u32 denied_driver_read_report = 0xFFFFFFFFu;
    u32 driver_read_report = 0xFFFFFFFFu;
    u32 denied_driver_read_receipt = 0xFFFFFFFFu;
    u32 driver_read_receipt = 0xFFFFFFFFu;
    u32 denied_driver_read_ack = 0xFFFFFFFFu;
    u32 driver_read_ack = 0xFFFFFFFFu;
    u32 denied_driver_read_close = 0xFFFFFFFFu;
    u32 driver_read_close = 0xFFFFFFFFu;
    u32 denied_driver_read_seal = 0xFFFFFFFFu;
    u32 driver_read_seal = 0xFFFFFFFFu;
    u32 denied_driver_read_unseal = 0xFFFFFFFFu;
    u32 driver_read_unseal = 0xFFFFFFFFu;
    u32 denied_driver_read_discard = 0xFFFFFFFFu;
    u32 driver_read_discard = 0xFFFFFFFFu;
    u32 denied_driver_read_finalize = 0xFFFFFFFFu;
    u32 driver_read_finalize = 0xFFFFFFFFu;
    u32 denied_driver_read_authorize = 0xFFFFFFFFu;
    u32 driver_read_authorize = 0xFFFFFFFFu;
    u32 denied_driver_read_dispatch = 0xFFFFFFFFu;
    u32 driver_read_dispatch = 0xFFFFFFFFu;
    u32 denied_driver_read_queue = 0xFFFFFFFFu;
    u32 driver_read_queue = 0xFFFFFFFFu;
    u32 denied_driver_read_worker = 0xFFFFFFFFu;
    u32 driver_read_worker = 0xFFFFFFFFu;
    u32 denied_driver_read_schedule = 0xFFFFFFFFu;
    u32 driver_read_schedule = 0xFFFFFFFFu;
    u32 denied_driver_read_run = 0xFFFFFFFFu;
    u32 driver_read_run = 0xFFFFFFFFu;
    u32 denied_driver_read_body = 0xFFFFFFFFu;
    u32 driver_read_body = 0xFFFFFFFFu;
    u32 denied_driver_read_issue = 0xFFFFFFFFu;
    u32 driver_read_issue = 0xFFFFFFFFu;
    u32 denied_driver_read_dma = 0xFFFFFFFFu;
    u32 driver_read_dma = 0xFFFFFFFFu;
    u32 denied_driver_read_irq = 0xFFFFFFFFu;
    u32 driver_read_irq = 0xFFFFFFFFu;
    u32 denied_driver_read_status = 0xFFFFFFFFu;
    u32 driver_read_status = 0xFFFFFFFFu;
    u32 denied_driver_read_status_result = 0xFFFFFFFFu;
    u32 driver_read_status_result = 0xFFFFFFFFu;
    u32 denied_driver_read_status_sample = 0xFFFFFFFFu;
    u32 driver_read_status_sample = 0xFFFFFFFFu;
    u32 denied_driver_read_status_clear = 0xFFFFFFFFu;
    u32 driver_read_status_clear = 0xFFFFFFFFu;
    u32 denied_driver_read_status_clear_result = 0xFFFFFFFFu;
    u32 driver_read_status_clear_result = 0xFFFFFFFFu;
    u32 denied_driver_read_status_resample = 0xFFFFFFFFu;
    u32 driver_read_status_resample = 0xFFFFFFFFu;
    u32 denied_driver_read_status_stable = 0xFFFFFFFFu;
    u32 driver_read_status_stable = 0xFFFFFFFFu;
    u32 denied_driver_read_status_guard = 0xFFFFFFFFu;
    u32 driver_read_status_guard = 0xFFFFFFFFu;
    u32 denied_driver_read_status_buffer = 0xFFFFFFFFu;
    u32 driver_read_status_buffer = 0xFFFFFFFFu;
    u32 denied_driver_read_status_export = 0xFFFFFFFFu;
    u32 driver_read_status_export = 0xFFFFFFFFu;
    u32 denied_driver_read_status_report = 0xFFFFFFFFu;
    u32 driver_read_status_report = 0xFFFFFFFFu;
    u32 denied_driver_read_status_receipt = 0xFFFFFFFFu;
    u32 driver_read_status_receipt = 0xFFFFFFFFu;
    u32 denied_driver_read_status_ack = 0xFFFFFFFFu;
    u32 driver_read_status_ack = 0xFFFFFFFFu;
    u32 denied_driver_read_status_close = 0xFFFFFFFFu;
    u32 driver_read_status_close = 0xFFFFFFFFu;
    u32 denied_driver_read_status_seal = 0xFFFFFFFFu;
    u32 driver_read_status_seal = 0xFFFFFFFFu;
    u32 denied_driver_read_status_unseal = 0xFFFFFFFFu;
    u32 driver_read_status_unseal = 0xFFFFFFFFu;
    u32 denied_driver_read_status_discard = 0xFFFFFFFFu;
    u32 driver_read_status_discard = 0xFFFFFFFFu;
    u32 denied_driver_read_status_finalize = 0xFFFFFFFFu;
    u32 driver_read_status_finalize = 0xFFFFFFFFu;
    u32 denied_driver_read_status_authorize = 0xFFFFFFFFu;
    u32 driver_read_status_authorize = 0xFFFFFFFFu;
    u32 denied_driver_read_status_dispatch = 0xFFFFFFFFu;
    u32 driver_read_status_dispatch = 0xFFFFFFFFu;
    u32 denied_driver_read_status_queue = 0xFFFFFFFFu;
    u32 driver_read_status_queue = 0xFFFFFFFFu;
    u32 denied_driver_read_status_worker = 0xFFFFFFFFu;
    u32 driver_read_status_worker = 0xFFFFFFFFu;
    u32 denied_driver_read_status_read_authority = 0xFFFFFFFFu;
    u32 driver_read_status_read_authority = 0xFFFFFFFFu;
    u32 denied_driver_read_status_descriptor = 0xFFFFFFFFu;
    u32 driver_read_status_descriptor = 0xFFFFFFFFu;
    u32 denied_driver_read_status_command_table = 0xFFFFFFFFu;
    u32 driver_read_status_command_table = 0xFFFFFFFFu;
    u32 denied_driver_read_status_command_issue = 0xFFFFFFFFu;
    u32 driver_read_status_command_issue = 0xFFFFFFFFu;
    u32 denied_driver_read_status_issue_grant = 0xFFFFFFFFu;
    u32 driver_read_status_issue_grant = 0xFFFFFFFFu;
    u32 denied_driver_read_status_arm = 0xFFFFFFFFu;
    u32 driver_read_status_arm = 0xFFFFFFFFu;
    u32 denied_driver_read_status_exec = 0xFFFFFFFFu;
    u32 driver_read_status_exec = 0xFFFFFFFFu;
    u32 denied_driver_read_status_dma = 0xFFFFFFFFu;
    u32 driver_read_status_dma = 0xFFFFFFFFu;
    u32 denied_driver_read_status_mmio = 0xFFFFFFFFu;
    u32 stale_driver_read_status_mmio = 0xFFFFFFFFu;
    u32 driver_read_status_mmio = 0xFFFFFFFFu;
    u32 denied_driver_read_status_dma_window = 0xFFFFFFFFu;
    u32 stale_driver_read_status_dma_window = 0xFFFFFFFFu;
    u32 driver_read_status_dma_window = 0xFFFFFFFFu;
    u32 driver_read_status_read = 0xFFFFFFFFu;
    u32 denied_driver_read_status_block = 0xFFFFFFFFu;
    u32 stale_driver_read_status_block = 0xFFFFFFFFu;
    u32 driver_read_status_block = 0xFFFFFFFFu;
    u32 driver_read_status_block_cap = 0xFFFFFFFFu;
    u32 driver_read_status_fs = 0xFFFFFFFFu;
    u32 driver_read_status_fs_user = 0xFFFFFFFFu;
    u32 driver_read_status_fs_shell = 0xFFFFFFFFu;

    write_labeled_dec_u32("[x64] pci broker service ", endpoint);
    write_labeled_hex_u32(" cap ", cap);
    write_labeled_hex_u32(" denied-devices ", denied_devices);
    write_labeled_hex_u32(" denied-mmio-base ", denied_mmio_base);
    write_string(" devices ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_DEVICE_COUNT, cap, 0u, owner_arg));
    write_string(" multi ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_MULTIFUNCTION_COUNT, cap, 0u, owner_arg));
    write_string(" storage ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_STORAGE_COUNT, cap, 0u, owner_arg));
    write_string(" ide ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_IDE_COUNT, cap, 0u, owner_arg));
    write_string(" ahci ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_AHCI_COUNT, cap, 0u, owner_arg));
    write_string(" nvme ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_NVME_COUNT, cap, 0u, owner_arg));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    write_string(" raid ");
    write_dec_u32(pci64_raid_count(cap, owner));
    write_string(" other-storage ");
    write_dec_u32(pci64_other_storage_count(cap, owner));
    write_string(" intel-system ");
    write_dec_u32(pci64_intel_system_count(cap, owner));
#endif
    write_string(" usb ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_USB_COUNT, cap, 0u, owner_arg));
    write_string(" display ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_DISPLAY_COUNT, cap, 0u, owner_arg));
    write_string(" ahci-addr ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_ADDRESS, cap, 0u, owner_arg));
    write_string(" ahci-vendor-device ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_VENDOR_DEVICE, cap, 0u, owner_arg));
    write_string(" ahci-class ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_CLASS, cap, 0u, owner_arg));
    write_string(" ahci-bar5 ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_BAR5, cap, 0u, owner_arg));
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    write_string(" nvme-addr ");
    write_hex_u32(pci64_first_nvme_address(cap, owner));
    write_string(" nvme-vendor-device ");
    write_hex_u32(pci64_first_nvme_vendor_device(cap, owner));
    write_string(" nvme-class ");
    write_hex_u32(pci64_first_nvme_class(cap, owner));
    write_string(" nvme-bar0 ");
    write_hex_u32(pci64_first_nvme_bar0(cap, owner));
    write_string(" nvme-bar1 ");
    write_hex_u32(pci64_first_nvme_bar1(cap, owner));
    write_string(" nvme-mmio-low ");
    write_hex_u32(pci64_first_nvme_mmio_base_low(cap, owner));
    write_string(" nvme-mmio-high ");
    write_hex_u32(pci64_first_nvme_mmio_base_high(cap, owner));
    write_string(" nvme-mmio-span ");
    write_dec_u32(pci64_first_nvme_mmio_span_hint(cap, owner));
    write_string(" nvme-mmio-flags ");
    write_hex_u32(pci64_first_nvme_mmio_flags(cap, owner));
    write_string(" nvme-mmio-token ");
    write_hex_u32(pci64_first_nvme_mmio_token(cap, owner));
    write_string(" other-storage-addr ");
    write_hex_u32(pci64_first_other_storage_address(cap, owner));
    write_string(" other-storage-vendor-device ");
    write_hex_u32(pci64_first_other_storage_vendor_device(cap, owner));
    write_string(" other-storage-class ");
    write_hex_u32(pci64_first_other_storage_class(cap, owner));
    write_string(" other-storage-bar0 ");
    write_hex_u32(pci64_first_other_storage_bar0(cap, owner));
    write_string(" other-storage-bar1 ");
    write_hex_u32(pci64_first_other_storage_bar1(cap, owner));
    write_string(" intel-system-addr ");
    write_hex_u32(pci64_first_intel_system_address(cap, owner));
    write_string(" intel-system-vendor-device ");
    write_hex_u32(pci64_first_intel_system_vendor_device(cap, owner));
    write_string(" intel-system-class ");
    write_hex_u32(pci64_first_intel_system_class(cap, owner));
    write_string(" intel-system-bar0 ");
    write_hex_u32(pci64_first_intel_system_bar0(cap, owner));
    write_string(" intel-system-bar1 ");
    write_hex_u32(pci64_first_intel_system_bar1(cap, owner));
#endif
    write_string(" token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_INVENTORY_TOKEN, cap, 0u, owner_arg));
    write_string(" mmio-base ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_MMIO_BASE, cap, 0u, owner_arg));
    write_string(" mmio-span ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_MMIO_SPAN_HINT, cap, 0u, owner_arg));
    write_string(" mmio-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_MMIO_FLAGS, cap, 0u, owner_arg));
    write_string(" mmio-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_PCI_FIRST_AHCI_MMIO_TOKEN, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" queries ", X64_SYSCALL_PCI_QUERY_COUNT);
    write_syscall0_dec_u32(" denials ", X64_SYSCALL_PCI_DENIAL_COUNT);
    write_line("");

    write_string("[x64] drs-pci-ecam");
    write_labeled_dec_u32(" drs-pci-ecam-rsdp ", pci64_ecam_rsdp_found());
    write_labeled_dec_u32(" drs-pci-ecam-mcfg ", pci64_ecam_mcfg_found());
    write_string(" drs-pci-ecam-base ");
    write_hex_u64(pci64_ecam_base());
    write_labeled_dec_u32(" drs-pci-ecam-segment ", pci64_ecam_segment());
    write_labeled_dec_u32(" drs-pci-ecam-bus-start ", pci64_ecam_bus_start());
    write_labeled_dec_u32(" drs-pci-ecam-bus-end ", pci64_ecam_bus_end());
    write_labeled_dec_u32(" drs-pci-ecam-active ", pci64_ecam_active());
    write_labeled_dec_u32(" drs-pci-ecam-fallback-io ", pci64_ecam_fallback_io());
    write_labeled_dec_u32(" drs-pci-ecam-ahci-found ", pci64_ecam_ahci_found());
    write_line("");

    write_labeled_hex_u32("[x64] drs-nvme-probe denied-drs-nvme-probe ", denied_nvme_probe);
    write_labeled_hex_u32(" drs-nvme-probe ", nvme_probe);
    write_labeled_dec_u32(" drs-nvme-probe-found ", mmio64_nvme_probe_found());
    write_string(" drs-nvme-probe-bar0 ");
    write_hex_u64(mmio64_nvme_probe_bar0());
    write_labeled_dec_u32(" drs-nvme-probe-ready ", mmio64_nvme_probe_ready());
    write_labeled_dec_u32(" drs-nvme-probe-identify ", mmio64_nvme_probe_identify());
    write_string(" drs-nvme-probe-model ");
    write_string(mmio64_nvme_probe_model());
    write_string(" drs-nvme-probe-firmware ");
    write_string(mmio64_nvme_probe_firmware());
    write_labeled_dec_u32(" drs-nvme-probe-io-queue ", mmio64_nvme_probe_io_queue());
    write_labeled_dec_u32(" drs-nvme-probe-read-authority ", mmio64_nvme_probe_read_authority());
    write_labeled_dec_u32(" drs-nvme-probe-fs-authority ", mmio64_nvme_probe_fs_authority());
    write_labeled_dec_u32(" drs-nvme-probe-unavailable ", mmio64_nvme_probe_unavailable());
    write_labeled_dec_u32(" drs-nvme-probe-error ", mmio64_nvme_probe_error());
    write_line("");

    write_labeled_hex_u32("[x64] drs-nvme-read denied-drs-nvme-read ", denied_nvme_read);
    write_labeled_hex_u32(" drs-nvme-read ", nvme_read);
    write_labeled_dec_u32(" drs-nvme-read-ioq-created ", mmio64_nvme_read_ioq_created());
    write_labeled_dec_u32(" drs-nvme-read-issued ", mmio64_nvme_read_issued());
    write_labeled_dec_u32(" drs-nvme-read-completed ", mmio64_nvme_read_completed());
    write_labeled_dec_u32(" drs-nvme-read-status ", mmio64_nvme_read_status());
    write_labeled_dec_u32(" drs-nvme-read-bytes ", mmio64_nvme_read_bytes());
    write_string(" drs-nvme-read-checksum ");
    write_hex_u32(mmio64_nvme_read_checksum());
    write_labeled_dec_u32(" fs-authority ", mmio64_nvme_read_fs_authority());
    write_labeled_dec_u32(" block-endpoint ", mmio64_nvme_read_block_endpoint());
    write_labeled_dec_u32(" write-authority ", mmio64_nvme_read_write_authority());
    write_labeled_dec_u32(" unavailable ", mmio64_nvme_read_unavailable());
    write_labeled_dec_u32(" error ", mmio64_nvme_read_error());
    write_line("");

    write_labeled_hex_u32("[x64] drs-nvme-gpt denied-drs-nvme-gpt ", denied_nvme_gpt);
    write_labeled_hex_u32(" drs-nvme-gpt ", nvme_gpt);
    write_labeled_dec_u32(" drs-nvme-gpt-signature ", mmio64_nvme_gpt_signature());
    write_labeled_dec_u32(" drs-nvme-gpt-partitions ", mmio64_nvme_gpt_partitions());
    write_labeled_dec_u32(" drs-nvme-gpt-fat32-start ", mmio64_nvme_gpt_fat32_start());
    write_labeled_dec_u32(" drs-nvme-gpt-fat32-sectors ", mmio64_nvme_gpt_fat32_sectors());
    write_labeled_dec_u32(" drs-nvme-gpt-vbr ", mmio64_nvme_gpt_vbr());
    write_labeled_dec_u32(" fs-authority ", mmio64_nvme_gpt_fs_authority());
    write_labeled_dec_u32(" write-authority ", mmio64_nvme_gpt_write_authority());
    write_labeled_dec_u32(" m5-safe-targets ", mmio64_nvme_gpt_m5_safe_targets());
    write_labeled_dec_u32(" m5-forbidden-targets ", mmio64_nvme_gpt_m5_forbidden_targets());
    write_labeled_dec_u32(" m5-unknown-targets ", mmio64_nvme_gpt_m5_unknown_targets());
    write_labeled_dec_u32(" m5-boot-partition ", mmio64_nvme_gpt_m5_boot_partition());
    write_labeled_dec_u32(" m5-root-partition ", mmio64_nvme_gpt_m5_root_partition());
    write_labeled_dec_u32(" m5-boot-start ", mmio64_nvme_gpt_m5_boot_start());
    write_labeled_dec_u32(" m5-root-start ", mmio64_nvme_gpt_m5_root_start());
    write_labeled_dec_u32(" m5-forbidden-denied ", mmio64_nvme_gpt_m5_forbidden_denied());
    write_labeled_dec_u32(" m5-no-write-authority ", mmio64_nvme_gpt_m5_no_write_authority());
    write_labeled_dec_u32(" unavailable ", mmio64_nvme_gpt_unavailable());
    write_labeled_dec_u32(" error ", mmio64_nvme_gpt_error());
    write_line("");

    write_labeled_hex_u32("[x64] drs-nvme-fat denied-drs-nvme-fat ", denied_nvme_fat);
    write_labeled_hex_u32(" drs-nvme-fat ", nvme_fat);
    write_labeled_dec_u32(" drs-nvme-fat-bpb ", mmio64_nvme_fat_bpb());
    write_labeled_dec_u32(" drs-nvme-fat-located ", mmio64_nvme_fat_located());
    write_labeled_dec_u32(" drs-nvme-fat-read-bytes ", mmio64_nvme_fat_read_bytes());
    write_string(" drs-nvme-fat-checksum ");
    write_hex_u32(mmio64_nvme_fat_checksum());
    write_labeled_dec_u32(" drs-nvme-fat-content-match ", mmio64_nvme_fat_content_match());
    write_labeled_dec_u32(" drs-nvme-fat-bytes-per-sector ", mmio64_nvme_fat_bytes_per_sector());
    write_labeled_dec_u32(" drs-nvme-fat-sectors-per-cluster ", mmio64_nvme_fat_sectors_per_cluster());
    write_labeled_dec_u32(" drs-nvme-fat-lfn ", mmio64_nvme_fat_lfn());
    write_labeled_dec_u32(" drs-nvme-fat-unicode-lfn ", mmio64_nvme_fat_unicode_lfn());
    write_labeled_dec_u32(" drs-nvme-fat-subdir ", mmio64_nvme_fat_subdir());
    write_labeled_dec_u32(" drs-nvme-fat-multicluster ", mmio64_nvme_fat_multicluster());
    write_labeled_dec_u32(" drs-nvme-fat-multi-bytes ", mmio64_nvme_fat_multi_read_bytes());
    write_labeled_dec_u32(" drs-nvme-fat-write-gate ", mmio64_nvme_fat_write_gate());
    write_labeled_dec_u32(" drs-nvme-fat-create-cluster ", mmio64_nvme_fat_create_cluster());
    write_labeled_dec_u32(" drs-nvme-fat-create-readback ", mmio64_nvme_fat_create_readback());
    write_labeled_dec_u32(" drs-nvme-fat-create-bytes ", mmio64_nvme_fat_create_bytes());
    write_string(" drs-nvme-fat-create-checksum ");
    write_hex_u32(mmio64_nvme_fat_create_checksum());
    write_labeled_dec_u32(" drs-nvme-fat-update-cluster ", mmio64_nvme_fat_update_cluster());
    write_labeled_dec_u32(" drs-nvme-fat-update-readback ", mmio64_nvme_fat_update_readback());
    write_labeled_dec_u32(" drs-nvme-fat-update-bytes ", mmio64_nvme_fat_update_bytes());
    write_string(" drs-nvme-fat-update-checksum ");
    write_hex_u32(mmio64_nvme_fat_update_checksum());
    write_labeled_dec_u32(" drs-nvme-fat-delete-freed ", mmio64_nvme_fat_delete_freed());
    write_labeled_dec_u32(" drs-nvme-fat-delete-tombstone ", mmio64_nvme_fat_delete_tombstone());
    write_labeled_dec_u32(" drs-nvme-fat-flushes ", mmio64_nvme_fat_flushes());
    write_labeled_dec_u32(" fs-delegation ", mmio64_nvme_fat_fs_delegation());
    write_labeled_dec_u32(" block-endpoint ", mmio64_nvme_fat_block_endpoint());
    write_labeled_dec_u32(" write-authority ", mmio64_nvme_fat_write_authority());
    write_labeled_dec_u32(" commit-authority ", mmio64_nvme_fat_commit_authority());
    write_labeled_dec_u32(" unavailable ", mmio64_nvme_fat_unavailable());
    write_labeled_dec_u32(" error ", mmio64_nvme_fat_error());
    write_line("");

    write_labeled_dec_u32("[x64] mmio planner service ", endpoint);
    write_labeled_hex_u32(" cap ", cap);
    write_labeled_hex_u32(" denied-ahci-base ", denied_plan_base);
    write_labeled_hex_u32(" denied-map ", denied_map_request);
    write_labeled_hex_u32(" map-request ", map_request);
    write_string(" plans ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_PLAN_COUNT, cap, 0u, owner_arg));
    write_string(" ahci-base ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_BASE, cap, 0u, owner_arg));
    write_string(" ahci-span ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SPAN, cap, 0u, owner_arg));
    write_string(" ahci-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_FLAGS, cap, 0u, owner_arg));
    write_string(" ahci-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_STATE, cap, 0u, owner_arg));
    write_string(" ahci-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_TOKEN, cap, 0u, owner_arg));
    write_string(" map-virt ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_VIRTUAL_BASE, cap, 0u, owner_arg));
    write_string(" map-pages ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_PAGE_COUNT, cap, 0u, owner_arg));
    write_string(" map-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_FLAGS, cap, 0u, owner_arg));
    write_string(" map-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_STATE, cap, 0u, owner_arg));
    write_string(" map-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_TOKEN, cap, 0u, owner_arg));
    write_string(" map-installed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_INSTALLED, cap, 0u, owner_arg));
    write_string(" map-install ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_INSTALL_TOKEN, cap, 0u, owner_arg));
    write_string(" map-pml4 ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_PML4_INDEX, cap, 0u, owner_arg));
    write_string(" map-pdpt ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_PDPT_INDEX, cap, 0u, owner_arg));
    write_string(" map-pd ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_PD_INDEX, cap, 0u, owner_arg));
    write_string(" map-pt ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_PT_INDEX, cap, 0u, owner_arg));
    write_string(" map-entry ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_ENTRY_FLAGS, cap, 0u, owner_arg));
    write_string(" map-nx ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_MAP_NX_ENABLED, cap, 0u, owner_arg));
    write_labeled_hex_u32(" denied-snapshot ", denied_snapshot_request);
    write_labeled_hex_u32(" snapshot ", snapshot_request);
    write_string(" snap-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SNAPSHOT_STATE, cap, 0u, owner_arg));
    write_string(" snap-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SNAPSHOT_FLAGS, cap, 0u, owner_arg));
    write_string(" snap-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SNAPSHOT_TOKEN, cap, 0u, owner_arg));
    write_string(" snap-cap ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SNAPSHOT_CAP, cap, 0u, owner_arg));
    write_string(" snap-ghc ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SNAPSHOT_GHC, cap, 0u, owner_arg));
    write_string(" snap-pi ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SNAPSHOT_PI, cap, 0u, owner_arg));
    write_string(" snap-version ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_SNAPSHOT_VERSION, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" snap-reads ", X64_SYSCALL_MMIO_AHCI_SNAPSHOT_READ_COUNT);
    write_syscall0_dec_u32(" snap-denials ", X64_SYSCALL_MMIO_AHCI_SNAPSHOT_DENIAL_COUNT);
    write_syscall0_dec_u32(" snap-unavailable ", X64_SYSCALL_MMIO_AHCI_SNAPSHOT_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-port-snapshot ", denied_port_snapshot);
    write_labeled_hex_u32(" port-snapshot ", port_snapshot);
    write_string(" port-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_STATE, cap, 0u, owner_arg));
    write_string(" port-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_FLAGS, cap, 0u, owner_arg));
    write_string(" port-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_TOKEN, cap, 0u, owner_arg));
    write_string(" ports-implemented ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_IMPLEMENTED_COUNT, cap, 0u, owner_arg));
    write_string(" ports-active ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_ACTIVE_COUNT, cap, 0u, owner_arg));
    write_string(" port-first ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_FIRST_IMPLEMENTED, cap, 0u, owner_arg));
    write_string(" port-first-active ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_FIRST_ACTIVE, cap, 0u, owner_arg));
    write_string(" port-selected ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SELECTED, cap, 0u, owner_arg));
    write_string(" port-ssts ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_SSTS, cap, 0u, owner_arg));
    write_string(" port-sig ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_SIGNATURE, cap, 0u, owner_arg));
    write_string(" port-cmd ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_COMMAND, cap, 0u, owner_arg));
    write_string(" port-tfd ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_TASK_FILE, cap, 0u, owner_arg));
    write_string(" port-ci ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_CI, cap, 0u, owner_arg));
    write_string(" port-serr ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_SELECTED_SERR, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" port-reads ", X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_READ_COUNT);
    write_syscall0_dec_u32(" port-denials ", X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_DENIAL_COUNT);
    write_syscall0_dec_u32(" port-unavailable ", X64_SYSCALL_MMIO_AHCI_PORT_SNAPSHOT_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-port-policy ", denied_port_policy);
    write_labeled_hex_u32(" port-policy ", port_policy);
    write_string(" policy-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_STATE, cap, 0u, owner_arg));
    write_string(" policy-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_FLAGS, cap, 0u, owner_arg));
    write_string(" policy-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_TOKEN, cap, 0u, owner_arg));
    write_string(" policy-kind ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DEVICE_KIND, cap, 0u, owner_arg));
    write_string(" policy-det ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DET, cap, 0u, owner_arg));
    write_string(" policy-spd ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_SPD, cap, 0u, owner_arg));
    write_string(" policy-ipm ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_IPM, cap, 0u, owner_arg));
    write_string(" policy-ready ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_READY, cap, 0u, owner_arg));
    write_string(" policy-busy ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_BUSY, cap, 0u, owner_arg));
    write_string(" policy-drq ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DRQ, cap, 0u, owner_arg));
    write_string(" policy-ci-idle ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_CI_IDLE, cap, 0u, owner_arg));
    write_string(" policy-serr-clear ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PORT_POLICY_SERR_CLEAR, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" policy-reads ", X64_SYSCALL_MMIO_AHCI_PORT_POLICY_READ_COUNT);
    write_syscall0_dec_u32(" policy-denials ", X64_SYSCALL_MMIO_AHCI_PORT_POLICY_DENIAL_COUNT);
    write_syscall0_dec_u32(" policy-unavailable ", X64_SYSCALL_MMIO_AHCI_PORT_POLICY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-read-plan ", denied_read_plan);
    write_labeled_hex_u32(" read-plan ", read_plan);
    write_string(" read-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_STATE, cap, 0u, owner_arg));
    write_string(" read-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_FLAGS, cap, 0u, owner_arg));
    write_string(" read-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_TOKEN, cap, 0u, owner_arg));
    write_string(" read-op ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_OPERATION, cap, 0u, owner_arg));
    write_string(" read-port ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_PORT, cap, 0u, owner_arg));
    write_string(" read-policy-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_POLICY_TOKEN, cap, 0u, owner_arg));
    write_string(" read-lba ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_LBA_LOW, cap, 0u, owner_arg));
    write_string(" read-blocks ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_BLOCK_COUNT, cap, 0u, owner_arg));
    write_string(" read-bytes ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_BYTE_COUNT_HINT, cap, 0u, owner_arg));
    write_string(" read-slot ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_COMMAND_SLOT, cap, 0u, owner_arg));
    write_string(" read-armed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_ARMED, cap, 0u, owner_arg));
    write_string(" read-issued ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_ISSUED, cap, 0u, owner_arg));
    write_string(" read-dma ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_READ_PLAN_DMA_MAPPED, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" read-staged ", X64_SYSCALL_MMIO_AHCI_READ_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" read-denials ", X64_SYSCALL_MMIO_AHCI_READ_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" read-unavailable ", X64_SYSCALL_MMIO_AHCI_READ_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-cmd-plan ", denied_command_plan);
    write_labeled_hex_u32(" cmd-plan ", command_plan);
    write_string(" cmd-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_STATE, cap, 0u, owner_arg));
    write_string(" cmd-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_FLAGS, cap, 0u, owner_arg));
    write_string(" cmd-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_TOKEN, cap, 0u, owner_arg));
    write_string(" cmd-read-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_READ_TOKEN, cap, 0u, owner_arg));
    write_string(" cmd-op ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_OPERATION, cap, 0u, owner_arg));
    write_string(" cmd-slot ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_SLOT, cap, 0u, owner_arg));
    write_string(" cmd-header ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_HEADER_BYTES, cap, 0u, owner_arg));
    write_string(" cmd-table ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_TABLE_BYTES, cap, 0u, owner_arg));
    write_string(" cmd-cfis ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_CFIS_BYTES, cap, 0u, owner_arg));
    write_string(" cmd-cfis-dwords ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_CFIS_DWORDS, cap, 0u, owner_arg));
    write_string(" cmd-prdt ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_PRDT_ENTRIES, cap, 0u, owner_arg));
    write_string(" cmd-prdt-bytes ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_PRDT_BYTES, cap, 0u, owner_arg));
    write_string(" cmd-atapi-packet ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_ATAPI_PACKET_BYTES, cap, 0u, owner_arg));
    write_string(" cmd-opcode ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_COMMAND_OPCODE, cap, 0u, owner_arg));
    write_string(" cmd-packet-opcode ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_PACKET_OPCODE, cap, 0u, owner_arg));
    write_string(" cmd-transfer ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_TRANSFER_BYTES, cap, 0u, owner_arg));
    write_string(" cmd-armed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_ARMED, cap, 0u, owner_arg));
    write_string(" cmd-issued ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_ISSUED, cap, 0u, owner_arg));
    write_string(" cmd-dma ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_DMA_MAPPED, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" cmd-staged ", X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" cmd-denials ", X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" cmd-unavailable ", X64_SYSCALL_MMIO_AHCI_COMMAND_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-mem-plan ", denied_memory_plan);
    write_labeled_hex_u32(" mem-plan ", memory_plan);
    {
        static const struct scaffold_arg_telemetry_field fields[] = {
            {"state ", 0u, SCAFFOLD_TELEMETRY_DEC},
            {"flags ", 1u, SCAFFOLD_TELEMETRY_HEX},
            {"token ", 2u, SCAFFOLD_TELEMETRY_HEX},
            {"cmd-token ", 3u, SCAFFOLD_TELEMETRY_HEX},
            {"slot ", 4u, SCAFFOLD_TELEMETRY_DEC},
            {"pages ", 5u, SCAFFOLD_TELEMETRY_DEC},
            {"page-bytes ", 6u, SCAFFOLD_TELEMETRY_DEC},
            {"page-virt ", 27u, SCAFFOLD_TELEMETRY_HEX64},
            {"page-phys ", 28u, SCAFFOLD_TELEMETRY_HEX64},
            {"page-checksum ", 29u, SCAFFOLD_TELEMETRY_HEX},
            {"zeroed ", 30u, SCAFFOLD_TELEMETRY_DEC},
            {"materialized ", 31u, SCAFFOLD_TELEMETRY_DEC},
            {"list-off ", 7u, SCAFFOLD_TELEMETRY_DEC},
            {"list-bytes ", 8u, SCAFFOLD_TELEMETRY_DEC},
            {"header-off ", 9u, SCAFFOLD_TELEMETRY_DEC},
            {"header-bytes ", 10u, SCAFFOLD_TELEMETRY_DEC},
            {"table-off ", 11u, SCAFFOLD_TELEMETRY_DEC},
            {"table-bytes ", 12u, SCAFFOLD_TELEMETRY_DEC},
            {"prdt-off ", 13u, SCAFFOLD_TELEMETRY_DEC},
            {"prdt-bytes ", 14u, SCAFFOLD_TELEMETRY_DEC},
            {"bounce-off ", 15u, SCAFFOLD_TELEMETRY_DEC},
            {"bounce-bytes ", 16u, SCAFFOLD_TELEMETRY_DEC},
            {"prdt-dbc ", 17u, SCAFFOLD_TELEMETRY_DEC},
            {"dma-low ", 18u, SCAFFOLD_TELEMETRY_HEX},
            {"dma-high ", 19u, SCAFFOLD_TELEMETRY_HEX},
            {"dma ", 20u, SCAFFOLD_TELEMETRY_DEC},
            {"table-written ", 21u, SCAFFOLD_TELEMETRY_DEC},
            {"port-programmed ", 22u, SCAFFOLD_TELEMETRY_DEC},
            {"armed ", 23u, SCAFFOLD_TELEMETRY_DEC}
        };

        write_syscall3_prefixed_offset_fields(
            " mem-",
            X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_STATE,
            fields,
            (u32)(sizeof(fields) / sizeof(fields[0])),
            cap,
            owner_arg);
    }
    write_syscall0_dec_u32(" mem-staged ", X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" mem-denials ", X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" mem-unavailable ", X64_SYSCALL_MMIO_AHCI_MEMORY_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-table-plan ", denied_table_plan);
    write_labeled_hex_u32(" table-plan ", table_plan);
    {
        static const struct scaffold_arg_telemetry_field fields[] = {
            {"state ", 0u, SCAFFOLD_TELEMETRY_DEC},
            {"flags ", 1u, SCAFFOLD_TELEMETRY_HEX},
            {"token ", 2u, SCAFFOLD_TELEMETRY_HEX},
            {"mem-token ", 3u, SCAFFOLD_TELEMETRY_HEX},
            {"check-before ", 4u, SCAFFOLD_TELEMETRY_HEX},
            {"check-after ", 5u, SCAFFOLD_TELEMETRY_HEX},
            {"check-changed ", 6u, SCAFFOLD_TELEMETRY_DEC},
            {"header-flags ", 7u, SCAFFOLD_TELEMETRY_HEX},
            {"prdtl ", 8u, SCAFFOLD_TELEMETRY_DEC},
            {"prdbc ", 9u, SCAFFOLD_TELEMETRY_DEC},
            {"ctba-low ", 10u, SCAFFOLD_TELEMETRY_HEX},
            {"ctba-high ", 11u, SCAFFOLD_TELEMETRY_HEX},
            {"cfis-type ", 12u, SCAFFOLD_TELEMETRY_HEX},
            {"cfis-flags ", 13u, SCAFFOLD_TELEMETRY_HEX},
            {"cfis-command ", 14u, SCAFFOLD_TELEMETRY_HEX},
            {"cfis-device ", 15u, SCAFFOLD_TELEMETRY_HEX},
            {"cfis-count ", 16u, SCAFFOLD_TELEMETRY_DEC},
            {"packet-opcode ", 17u, SCAFFOLD_TELEMETRY_HEX},
            {"packet-blocks ", 18u, SCAFFOLD_TELEMETRY_DEC},
            {"prdt-dba-low ", 19u, SCAFFOLD_TELEMETRY_HEX},
            {"prdt-dba-high ", 20u, SCAFFOLD_TELEMETRY_HEX},
            {"prdt-dbc ", 21u, SCAFFOLD_TELEMETRY_DEC},
            {"written ", 22u, SCAFFOLD_TELEMETRY_DEC},
            {"dma ", 23u, SCAFFOLD_TELEMETRY_DEC},
            {"port-programmed ", 24u, SCAFFOLD_TELEMETRY_DEC},
            {"armed ", 25u, SCAFFOLD_TELEMETRY_DEC},
            {"issued ", 26u, SCAFFOLD_TELEMETRY_DEC}
        };

        write_syscall3_prefixed_offset_fields(
            " table-",
            X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_STATE,
            fields,
            (u32)(sizeof(fields) / sizeof(fields[0])),
            cap,
            owner_arg);
    }
    write_syscall0_dec_u32(" table-staged ", X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" table-denials ", X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" table-unavailable ", X64_SYSCALL_MMIO_AHCI_TABLE_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-issue-plan ", denied_issue_plan);
    write_labeled_hex_u32(" issue-plan ", issue_plan);
    {
        static const struct scaffold_arg_telemetry_field fields[] = {
            {"state ", 0u, SCAFFOLD_TELEMETRY_DEC},
            {"flags ", 1u, SCAFFOLD_TELEMETRY_HEX},
            {"token ", 2u, SCAFFOLD_TELEMETRY_HEX},
            {"table-token ", 3u, SCAFFOLD_TELEMETRY_HEX},
            {"mem-token ", 4u, SCAFFOLD_TELEMETRY_HEX},
            {"cmd-token ", 5u, SCAFFOLD_TELEMETRY_HEX},
            {"read-token ", 6u, SCAFFOLD_TELEMETRY_HEX},
            {"port ", 7u, SCAFFOLD_TELEMETRY_HEX},
            {"slot ", 8u, SCAFFOLD_TELEMETRY_DEC},
            {"ci ", 9u, SCAFFOLD_TELEMETRY_HEX},
            {"slot-mask ", 10u, SCAFFOLD_TELEMETRY_HEX},
            {"slot-idle ", 11u, SCAFFOLD_TELEMETRY_DEC},
            {"tfd-ready ", 12u, SCAFFOLD_TELEMETRY_DEC},
            {"serr-clear ", 13u, SCAFFOLD_TELEMETRY_DEC},
            {"policy-ready ", 14u, SCAFFOLD_TELEMETRY_DEC},
            {"engine-st ", 15u, SCAFFOLD_TELEMETRY_DEC},
            {"engine-fre ", 16u, SCAFFOLD_TELEMETRY_DEC},
            {"engine-fr ", 17u, SCAFFOLD_TELEMETRY_DEC},
            {"engine-cr ", 18u, SCAFFOLD_TELEMETRY_DEC},
            {"stop-required ", 19u, SCAFFOLD_TELEMETRY_DEC},
            {"start-required ", 20u, SCAFFOLD_TELEMETRY_DEC},
            {"timeout ", 21u, SCAFFOLD_TELEMETRY_DEC},
            {"poll-budget ", 22u, SCAFFOLD_TELEMETRY_DEC},
            {"table-check ", 23u, SCAFFOLD_TELEMETRY_HEX},
            {"expected-check ", 24u, SCAFFOLD_TELEMETRY_HEX},
            {"check-match ", 25u, SCAFFOLD_TELEMETRY_DEC},
            {"dma ", 26u, SCAFFOLD_TELEMETRY_DEC},
            {"port-programmed ", 27u, SCAFFOLD_TELEMETRY_DEC},
            {"command-issued ", 28u, SCAFFOLD_TELEMETRY_DEC},
            {"armed ", 29u, SCAFFOLD_TELEMETRY_DEC}
        };

        write_syscall3_prefixed_offset_fields(
            " issue-",
            X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_STATE,
            fields,
            (u32)(sizeof(fields) / sizeof(fields[0])),
            cap,
            owner_arg);
    }
    write_syscall0_dec_u32(" issue-staged ", X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" issue-denials ", X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" issue-unavailable ", X64_SYSCALL_MMIO_AHCI_ISSUE_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-bind-plan ", denied_bind_plan);
    write_labeled_hex_u32(" bind-plan ", bind_plan);
    {
        static const struct scaffold_arg_telemetry_field fields[] = {
            {"state ", 0u, SCAFFOLD_TELEMETRY_DEC},
            {"flags ", 1u, SCAFFOLD_TELEMETRY_HEX},
            {"token ", 2u, SCAFFOLD_TELEMETRY_HEX},
            {"issue-token ", 3u, SCAFFOLD_TELEMETRY_HEX},
            {"table-token ", 4u, SCAFFOLD_TELEMETRY_HEX},
            {"mem-token ", 5u, SCAFFOLD_TELEMETRY_HEX},
            {"cmd-token ", 6u, SCAFFOLD_TELEMETRY_HEX},
            {"read-token ", 7u, SCAFFOLD_TELEMETRY_HEX},
            {"page-low ", 8u, SCAFFOLD_TELEMETRY_HEX},
            {"page-high ", 9u, SCAFFOLD_TELEMETRY_HEX},
            {"list-low ", 10u, SCAFFOLD_TELEMETRY_HEX},
            {"list-high ", 11u, SCAFFOLD_TELEMETRY_HEX},
            {"table-low ", 12u, SCAFFOLD_TELEMETRY_HEX},
            {"table-high ", 13u, SCAFFOLD_TELEMETRY_HEX},
            {"bounce-low ", 14u, SCAFFOLD_TELEMETRY_HEX},
            {"bounce-high ", 15u, SCAFFOLD_TELEMETRY_HEX},
            {"header-ctba-low ", 16u, SCAFFOLD_TELEMETRY_HEX},
            {"header-ctba-high ", 17u, SCAFFOLD_TELEMETRY_HEX},
            {"prdt-dba-low ", 18u, SCAFFOLD_TELEMETRY_HEX},
            {"prdt-dba-high ", 19u, SCAFFOLD_TELEMETRY_HEX},
            {"prdt-dbc ", 20u, SCAFFOLD_TELEMETRY_DEC},
            {"header-patch ", 21u, SCAFFOLD_TELEMETRY_DEC},
            {"prdt-patch ", 22u, SCAFFOLD_TELEMETRY_DEC},
            {"check-before ", 23u, SCAFFOLD_TELEMETRY_HEX},
            {"check-predicted ", 24u, SCAFFOLD_TELEMETRY_HEX},
            {"check-changed ", 25u, SCAFFOLD_TELEMETRY_DEC},
            {"aligned ", 26u, SCAFFOLD_TELEMETRY_DEC},
            {"range-ready ", 27u, SCAFFOLD_TELEMETRY_DEC},
            {"below-4g ", 28u, SCAFFOLD_TELEMETRY_DEC},
            {"memory-written ", 29u, SCAFFOLD_TELEMETRY_DEC},
            {"dma ", 30u, SCAFFOLD_TELEMETRY_DEC},
            {"port-programmed ", 31u, SCAFFOLD_TELEMETRY_DEC},
            {"published ", 32u, SCAFFOLD_TELEMETRY_DEC},
            {"command-issued ", 33u, SCAFFOLD_TELEMETRY_DEC},
            {"armed ", 34u, SCAFFOLD_TELEMETRY_DEC}
        };

        write_syscall3_prefixed_offset_fields(
            " bind-",
            X64_SYSCALL_MMIO_AHCI_BIND_PLAN_STATE,
            fields,
            (u32)(sizeof(fields) / sizeof(fields[0])),
            cap,
            owner_arg);
    }
    write_syscall0_dec_u32(" bind-staged ", X64_SYSCALL_MMIO_AHCI_BIND_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" bind-denials ", X64_SYSCALL_MMIO_AHCI_BIND_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" bind-unavailable ", X64_SYSCALL_MMIO_AHCI_BIND_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-patch-plan ", denied_patch_plan);
    write_labeled_hex_u32(" patch-plan ", patch_plan);
    {
        static const struct scaffold_arg_telemetry_field fields[] = {
            {"state ", 0u, SCAFFOLD_TELEMETRY_DEC},
            {"flags ", 1u, SCAFFOLD_TELEMETRY_HEX},
            {"token ", 2u, SCAFFOLD_TELEMETRY_HEX},
            {"bind-token ", 3u, SCAFFOLD_TELEMETRY_HEX},
            {"issue-token ", 4u, SCAFFOLD_TELEMETRY_HEX},
            {"table-token ", 5u, SCAFFOLD_TELEMETRY_HEX},
            {"mem-token ", 6u, SCAFFOLD_TELEMETRY_HEX},
            {"cmd-token ", 7u, SCAFFOLD_TELEMETRY_HEX},
            {"read-token ", 8u, SCAFFOLD_TELEMETRY_HEX},
            {"header-patch ", 9u, SCAFFOLD_TELEMETRY_DEC},
            {"prdt-patch ", 10u, SCAFFOLD_TELEMETRY_DEC},
            {"header-ctba-low ", 11u, SCAFFOLD_TELEMETRY_HEX},
            {"header-ctba-high ", 12u, SCAFFOLD_TELEMETRY_HEX},
            {"prdt-dba-low ", 13u, SCAFFOLD_TELEMETRY_HEX},
            {"prdt-dba-high ", 14u, SCAFFOLD_TELEMETRY_HEX},
            {"check-before ", 15u, SCAFFOLD_TELEMETRY_HEX},
            {"check-expected ", 16u, SCAFFOLD_TELEMETRY_HEX},
            {"check-after ", 17u, SCAFFOLD_TELEMETRY_HEX},
            {"check-match ", 18u, SCAFFOLD_TELEMETRY_DEC},
            {"check-changed ", 19u, SCAFFOLD_TELEMETRY_DEC},
            {"memory-written ", 20u, SCAFFOLD_TELEMETRY_DEC},
            {"dma ", 21u, SCAFFOLD_TELEMETRY_DEC},
            {"port-programmed ", 22u, SCAFFOLD_TELEMETRY_DEC},
            {"published ", 23u, SCAFFOLD_TELEMETRY_DEC},
            {"command-issued ", 24u, SCAFFOLD_TELEMETRY_DEC},
            {"armed ", 25u, SCAFFOLD_TELEMETRY_DEC}
        };

        write_syscall3_prefixed_offset_fields(
            " patch-",
            X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_STATE,
            fields,
            (u32)(sizeof(fields) / sizeof(fields[0])),
            cap,
            owner_arg);
    }
    write_syscall0_dec_u32(" patch-staged ", X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" patch-denials ", X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" patch-unavailable ", X64_SYSCALL_MMIO_AHCI_PATCH_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-publish-plan ", denied_publish_plan);
    write_labeled_hex_u32(" publish-plan ", publish_plan);
    {
        static const struct scaffold_arg_telemetry_field fields[] = {
            {"state ", 0u, SCAFFOLD_TELEMETRY_DEC},
            {"flags ", 1u, SCAFFOLD_TELEMETRY_HEX},
            {"token ", 2u, SCAFFOLD_TELEMETRY_HEX},
            {"patch-token ", 3u, SCAFFOLD_TELEMETRY_HEX},
            {"bind-token ", 4u, SCAFFOLD_TELEMETRY_HEX},
            {"issue-token ", 5u, SCAFFOLD_TELEMETRY_HEX},
            {"table-token ", 6u, SCAFFOLD_TELEMETRY_HEX},
            {"mem-token ", 7u, SCAFFOLD_TELEMETRY_HEX},
            {"cmd-token ", 8u, SCAFFOLD_TELEMETRY_HEX},
            {"read-token ", 9u, SCAFFOLD_TELEMETRY_HEX},
            {"port ", 10u, SCAFFOLD_TELEMETRY_HEX},
            {"port-base ", 11u, SCAFFOLD_TELEMETRY_DEC},
            {"list-low ", 12u, SCAFFOLD_TELEMETRY_HEX},
            {"list-high ", 13u, SCAFFOLD_TELEMETRY_HEX},
            {"fis-low ", 14u, SCAFFOLD_TELEMETRY_HEX},
            {"fis-high ", 15u, SCAFFOLD_TELEMETRY_HEX},
            {"clb-off ", 16u, SCAFFOLD_TELEMETRY_DEC},
            {"clbu-off ", 17u, SCAFFOLD_TELEMETRY_DEC},
            {"fb-off ", 18u, SCAFFOLD_TELEMETRY_DEC},
            {"fbu-off ", 19u, SCAFFOLD_TELEMETRY_DEC},
            {"cmd-off ", 20u, SCAFFOLD_TELEMETRY_DEC},
            {"ci-off ", 21u, SCAFFOLD_TELEMETRY_DEC},
            {"clb-low ", 22u, SCAFFOLD_TELEMETRY_HEX},
            {"clb-high ", 23u, SCAFFOLD_TELEMETRY_HEX},
            {"fb-low ", 24u, SCAFFOLD_TELEMETRY_HEX},
            {"fb-high ", 25u, SCAFFOLD_TELEMETRY_HEX},
            {"fis-off ", 26u, SCAFFOLD_TELEMETRY_DEC},
            {"fis-bytes ", 27u, SCAFFOLD_TELEMETRY_DEC},
            {"page-check ", 28u, SCAFFOLD_TELEMETRY_HEX},
            {"page-match ", 29u, SCAFFOLD_TELEMETRY_DEC},
            {"clb-aligned ", 30u, SCAFFOLD_TELEMETRY_DEC},
            {"fis-aligned ", 31u, SCAFFOLD_TELEMETRY_DEC},
            {"range-ready ", 32u, SCAFFOLD_TELEMETRY_DEC},
            {"below-4g ", 33u, SCAFFOLD_TELEMETRY_DEC},
            {"memory-written ", 34u, SCAFFOLD_TELEMETRY_DEC},
            {"dma ", 35u, SCAFFOLD_TELEMETRY_DEC},
            {"mmio-written ", 36u, SCAFFOLD_TELEMETRY_DEC},
            {"port-programmed ", 37u, SCAFFOLD_TELEMETRY_DEC},
            {"published ", 38u, SCAFFOLD_TELEMETRY_DEC},
            {"command-issued ", 39u, SCAFFOLD_TELEMETRY_DEC},
            {"armed ", 40u, SCAFFOLD_TELEMETRY_DEC}
        };

        write_syscall3_prefixed_offset_fields(
            " publish-",
            X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_STATE,
            fields,
            (u32)(sizeof(fields) / sizeof(fields[0])),
            cap,
            owner_arg);
    }
    write_syscall0_dec_u32(" publish-staged ", X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_STAGE_COUNT);
    write_syscall0_dec_u32(" publish-denials ", X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_DENIAL_COUNT);
    write_syscall0_dec_u32(" publish-unavailable ", X64_SYSCALL_MMIO_AHCI_PUBLISH_PLAN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-publish-gate ", denied_publish_gate);
    write_labeled_hex_u32(" publish-gate ", publish_gate);
    write_string(" gate-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_STATE, cap, 0u, owner_arg));
    write_string(" gate-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_FLAGS, cap, 0u, owner_arg));
    write_string(" gate-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_TOKEN, cap, 0u, owner_arg));
    write_string(" gate-publish-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_PUBLISH_TOKEN, cap, 0u, owner_arg));
    write_string(" gate-live-hardware ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_LIVE_HARDWARE_HANDLES,
        cap,
        0u,
        owner_arg));
    write_string(" gate-exclusive ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_EXCLUSIVE_HARDWARE_HANDLE,
        cap,
        0u,
        owner_arg));
    write_string(" gate-revocation-required ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_REVOCATION_REQUIRED,
        cap,
        0u,
        owner_arg));
    write_string(" gate-revocation-satisfied ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_REVOCATION_SATISFIED,
        cap,
        0u,
        owner_arg));
    write_string(" gate-write-window ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_WRITE_WINDOW_ENABLED,
        cap,
        0u,
        owner_arg));
    write_string(" gate-commit-allowed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_COMMIT_ALLOWED,
        cap,
        0u,
        owner_arg));
    write_string(" gate-mmio-written ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_MMIO_WRITTEN, cap, 0u, owner_arg));
    write_string(" gate-port-programmed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_PORT_PROGRAMMED, cap, 0u, owner_arg));
    write_string(" gate-published ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_PUBLISHED, cap, 0u, owner_arg));
    write_string(" gate-command-issued ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_COMMAND_ISSUED, cap, 0u, owner_arg));
    write_string(" gate-armed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_ARMED, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" gate-staged ", X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_STAGE_COUNT);
    write_syscall0_dec_u32(" gate-denials ", X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_DENIAL_COUNT);
    write_syscall0_dec_u32(" gate-unavailable ", X64_SYSCALL_MMIO_AHCI_PUBLISH_GATE_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-window-policy ", denied_window_policy);
    write_labeled_hex_u32(" window-policy ", window_policy);
    write_string(" window-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_STATE, cap, 0u, owner_arg));
    write_string(" window-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_FLAGS, cap, 0u, owner_arg));
    write_string(" window-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_TOKEN, cap, 0u, owner_arg));
    write_string(" window-gate-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_GATE_TOKEN, cap, 0u, owner_arg));
    write_string(" window-publish-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_PUBLISH_TOKEN, cap, 0u, owner_arg));
    write_string(" window-live-hardware ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_LIVE_HARDWARE_HANDLES,
        cap,
        0u,
        owner_arg));
    write_string(" window-exclusive ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_EXCLUSIVE_HARDWARE_HANDLE,
        cap,
        0u,
        owner_arg));
    write_string(" window-revocation-required ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_REVOCATION_REQUIRED,
        cap,
        0u,
        owner_arg));
    write_string(" window-revocation-satisfied ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_REVOCATION_SATISFIED,
        cap,
        0u,
        owner_arg));
    write_string(" window-revocation-executed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_REVOCATION_EXECUTED,
        cap,
        0u,
        owner_arg));
    write_string(" window-write-window ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_WRITE_WINDOW_ENABLED,
        cap,
        0u,
        owner_arg));
    write_string(" window-commit-allowed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_COMMIT_ALLOWED,
        cap,
        0u,
        owner_arg));
    write_string(" window-mmio-written ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_MMIO_WRITTEN, cap, 0u, owner_arg));
    write_string(" window-port-programmed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_PORT_PROGRAMMED,
        cap,
        0u,
        owner_arg));
    write_string(" window-published ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_PUBLISHED, cap, 0u, owner_arg));
    write_string(" window-command-issued ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_COMMAND_ISSUED, cap, 0u, owner_arg));
    write_string(" window-armed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_ARMED, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" window-staged ", X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_STAGE_COUNT);
    write_syscall0_dec_u32(" window-denials ", X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_DENIAL_COUNT);
    write_syscall0_dec_u32(" window-unavailable ", X64_SYSCALL_MMIO_AHCI_PUBLISH_WINDOW_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-revoke-plan ", denied_revoke_plan);
    write_labeled_hex_u32(" revoke-plan ", revoke_plan);
    write_string(" revoke-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_STATE, cap, 0u, owner_arg));
    write_string(" revoke-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_FLAGS, cap, 0u, owner_arg));
    write_string(" revoke-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_TOKEN, cap, 0u, owner_arg));
    write_string(" revoke-window-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_WINDOW_TOKEN, cap, 0u, owner_arg));
    write_string(" revoke-gate-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_GATE_TOKEN, cap, 0u, owner_arg));
    write_string(" revoke-live-before ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_LIVE_BEFORE, cap, 0u, owner_arg));
    write_string(" revoke-live-after ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_LIVE_AFTER, cap, 0u, owner_arg));
    write_string(" revoke-exclusive ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_EXCLUSIVE_HARDWARE_HANDLE,
        cap,
        0u,
        owner_arg));
    write_string(" revoke-required ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_REVOCATION_REQUIRED,
        cap,
        0u,
        owner_arg));
    write_string(" revoke-planned ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_REVOCATION_PLANNED,
        cap,
        0u,
        owner_arg));
    write_string(" revoke-executed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_REVOCATION_EXECUTED,
        cap,
        0u,
        owner_arg));
    write_string(" revoke-would-revoke ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_WOULD_REVOKE, cap, 0u, owner_arg));
    write_string(" revoke-write-window ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_WRITE_WINDOW_ENABLED,
        cap,
        0u,
        owner_arg));
    write_string(" revoke-commit-allowed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_COMMIT_ALLOWED, cap, 0u, owner_arg));
    write_string(" revoke-mmio-written ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_MMIO_WRITTEN, cap, 0u, owner_arg));
    write_string(" revoke-port-programmed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_PORT_PROGRAMMED, cap, 0u, owner_arg));
    write_string(" revoke-published ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_PUBLISHED, cap, 0u, owner_arg));
    write_string(" revoke-command-issued ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_COMMAND_ISSUED, cap, 0u, owner_arg));
    write_string(" revoke-armed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_ARMED, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" revoke-staged ", X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_STAGE_COUNT);
    write_syscall0_dec_u32(" revoke-denials ", X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_DENIAL_COUNT);
    write_syscall0_dec_u32(" revoke-unavailable ", X64_SYSCALL_MMIO_AHCI_PUBLISH_REVOKE_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-open-window ", denied_open_window);
    write_labeled_hex_u32(" open-window ", open_window);
    write_string(" open-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_STATE, cap, 0u, owner_arg));
    write_string(" open-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_FLAGS, cap, 0u, owner_arg));
    write_string(" open-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_TOKEN, cap, 0u, owner_arg));
    write_string(" open-revoke-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOKE_TOKEN, cap, 0u, owner_arg));
    write_string(" open-window-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_WINDOW_TOKEN, cap, 0u, owner_arg));
    write_string(" open-live-hardware ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_LIVE_HARDWARE_HANDLES,
        cap,
        0u,
        owner_arg));
    write_string(" open-revocation-required ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOCATION_REQUIRED,
        cap,
        0u,
        owner_arg));
    write_string(" open-revocation-planned ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOCATION_PLANNED,
        cap,
        0u,
        owner_arg));
    write_string(" open-revocation-executed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_REVOCATION_EXECUTED,
        cap,
        0u,
        owner_arg));
    write_string(" open-write-window ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_WRITE_WINDOW_ENABLED,
        cap,
        0u,
        owner_arg));
    write_string(" open-allowed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_ALLOWED, cap, 0u, owner_arg));
    write_string(" open-commit-allowed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_COMMIT_ALLOWED,
        cap,
        0u,
        owner_arg));
    write_string(" open-mmio-written ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_MMIO_WRITTEN, cap, 0u, owner_arg));
    write_string(" open-port-programmed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_PORT_PROGRAMMED, cap, 0u, owner_arg));
    write_string(" open-published ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_PUBLISHED, cap, 0u, owner_arg));
    write_string(" open-command-issued ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_COMMAND_ISSUED, cap, 0u, owner_arg));
    write_string(" open-armed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_ARMED, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" open-staged ", X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_STAGE_COUNT);
    write_syscall0_dec_u32(" open-denials ", X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_DENIAL_COUNT);
    write_syscall0_dec_u32(" open-unavailable ", X64_SYSCALL_MMIO_AHCI_PUBLISH_OPEN_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-session ", denied_session);
    write_labeled_hex_u32(" session ", session);
    write_string(" session-state ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_STATE, cap, 0u, owner_arg));
    write_string(" session-flags ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_FLAGS, cap, 0u, owner_arg));
    write_string(" session-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_TOKEN, cap, 0u, owner_arg));
    write_string(" session-open-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_OPEN_TOKEN, cap, 0u, owner_arg));
    write_string(" session-revoke-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOKE_TOKEN, cap, 0u, owner_arg));
    write_string(" session-window-token ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_WINDOW_TOKEN, cap, 0u, owner_arg));
    write_string(" session-live-hardware ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_LIVE_HARDWARE_HANDLES,
        cap,
        0u,
        owner_arg));
    write_string(" session-revocation-required ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOCATION_REQUIRED,
        cap,
        0u,
        owner_arg));
    write_string(" session-revocation-planned ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOCATION_PLANNED,
        cap,
        0u,
        owner_arg));
    write_string(" session-revocation-executed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_REVOCATION_EXECUTED,
        cap,
        0u,
        owner_arg));
    write_string(" session-allowed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_ALLOWED, cap, 0u, owner_arg));
    write_string(" session-driver-owned ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_DRIVER_OWNED, cap, 0u, owner_arg));
    write_string(" session-write-window ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_WRITE_WINDOW_ENABLED,
        cap,
        0u,
        owner_arg));
    write_string(" session-commit-allowed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_COMMIT_ALLOWED,
        cap,
        0u,
        owner_arg));
    write_string(" session-mmio-written ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_MMIO_WRITTEN, cap, 0u, owner_arg));
    write_string(" session-port-programmed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_PORT_PROGRAMMED, cap, 0u, owner_arg));
    write_string(" session-published ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_PUBLISHED, cap, 0u, owner_arg));
    write_string(" session-command-issued ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_COMMAND_ISSUED, cap, 0u, owner_arg));
    write_string(" session-armed ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_ARMED, cap, 0u, owner_arg));
    write_syscall0_dec_u32(" session-staged ", X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_STAGE_COUNT);
    write_syscall0_dec_u32(" session-denials ", X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_DENIAL_COUNT);
    write_syscall0_dec_u32(" session-unavailable ", X64_SYSCALL_MMIO_AHCI_PUBLISH_SESSION_UNAVAILABLE_COUNT);
    denied_drain = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_EXECUTE_AHCI_PUBLISH_CAPABILITY_DRAIN,
        cap,
        0u,
        wrong_owner_arg);
    drain = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_EXECUTE_AHCI_PUBLISH_CAPABILITY_DRAIN,
        cap,
        0u,
        owner_arg);
    write_labeled_hex_u32(" denied-drain ", denied_drain);
    write_labeled_hex_u32(" drain ", drain);
    static const char syscall0_suffixes_9[] =
        "state \0"
        "flags \0"
        "token \0"
        "session-token \0"
        "open-token \0"
        "revoke-token \0"
        "window-token \0"
        "live-before \0"
        "revoked \0"
        "live-after \0";
    static const struct scaffold_syscall0_field syscall0_fields_9[] = {        {0, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_SESSION_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {36, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_OPEN_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {48, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOKE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {62, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_WINDOW_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {76, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_LIVE_BEFORE, SCAFFOLD_TELEMETRY_DEC},
        {89, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOKED_HANDLES, SCAFFOLD_TELEMETRY_DEC},
        {98, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_LIVE_AFTER, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drain-", syscall0_suffixes_9, syscall0_fields_9, (u32)(sizeof(syscall0_fields_9) / sizeof(syscall0_fields_9[0])));
    write_string(" drain-revocation-required ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOCATION_REQUIRED,
        0u,
        0u,
        0u));
    write_string(" drain-revocation-planned ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOCATION_PLANNED,
        0u,
        0u,
        0u));
    write_string(" drain-revocation-executed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_REVOCATION_EXECUTED,
        0u,
        0u,
        0u));
    write_string(" drain-write-window ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_WRITE_WINDOW_ENABLED,
        0u,
        0u,
        0u));
    static const char syscall0_suffixes_10[] =
        "commit-allowed \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "armed \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_10[] = {        {0, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_COMMIT_ALLOWED, SCAFFOLD_TELEMETRY_DEC},
        {16, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {30, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {47, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {58, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {74, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {81, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {89, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {98, X64_SYSCALL_MMIO_AHCI_PUBLISH_DRAIN_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drain-", syscall0_suffixes_10, syscall0_fields_10, (u32)(sizeof(syscall0_fields_10) / sizeof(syscall0_fields_10[0])));
    denied_handoff = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_DRIVER_HANDOFF,
        cap,
        drain,
        denied_handoff_arg);
    handoff = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_PUBLISH_DRIVER_HANDOFF,
        cap,
        drain,
        handoff_arg);
    write_labeled_hex_u32(" denied-handoff ", denied_handoff);
    write_labeled_hex_u32(" handoff ", handoff);
    static const char syscall0_suffixes_11[] =
        "state \0"
        "flags \0"
        "token \0"
        "drain-token \0"
        "old-handle \0"
        "driver-owner \0"
        "driver-cap \0"
        "live-before \0"
        "stale-old-denied \0";
    static const struct scaffold_syscall0_field syscall0_fields_11[] = {        {0, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRAIN_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {34, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_OLD_HANDLE, SCAFFOLD_TELEMETRY_HEX},
        {46, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {60, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {72, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_LIVE_BEFORE, SCAFFOLD_TELEMETRY_DEC},
        {85, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_STALE_OLD_DENIED, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" handoff-", syscall0_suffixes_11, syscall0_fields_11, (u32)(sizeof(syscall0_fields_11) / sizeof(syscall0_fields_11[0])));
    write_string(" handoff-driver-valid ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_PRINCIPAL_VALID,
        0u,
        0u,
        0u));
    static const char syscall0_suffixes_12[] =
        "driver-role \0"
        "owner-bound \0"
        "query-only \0"
        "live-after \0";
    static const struct scaffold_syscall0_field syscall0_fields_12[] = {        {0, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_ROLE, SCAFFOLD_TELEMETRY_HEX},
        {13, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {26, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {38, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_LIVE_AFTER, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" handoff-", syscall0_suffixes_12, syscall0_fields_12, (u32)(sizeof(syscall0_fields_12) / sizeof(syscall0_fields_12[0])));
    write_string(" handoff-write-window ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_WRITE_WINDOW_ENABLED,
        0u,
        0u,
        0u));
    static const char syscall0_suffixes_13[] =
        "commit-allowed \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "armed \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_13[] = {        {0, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_COMMIT_ALLOWED, SCAFFOLD_TELEMETRY_DEC},
        {16, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {30, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {47, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {58, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {74, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {81, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {89, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {98, X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" handoff-", syscall0_suffixes_13, syscall0_fields_13, (u32)(sizeof(syscall0_fields_13) / sizeof(syscall0_fields_13[0])));
    driver_probe_cap = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_PUBLISH_HANDOFF_DRIVER_CAPABILITY,
        0u,
        0u,
        0u);
    denied_driver_probe = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_PROBE,
        driver_probe_cap,
        handoff,
        wrong_owner_arg);
    driver_probe = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_PROBE,
        driver_probe_cap,
        handoff,
        driver_probe_owner_arg);
    denied_driver_intent = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_INTENT,
        driver_probe_cap,
        driver_probe,
        wrong_owner_arg);
    driver_intent = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_INTENT,
        driver_probe_cap,
        driver_probe,
        driver_probe_owner_arg);
    denied_driver_buffer = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_BUFFER,
        driver_probe_cap,
        driver_intent,
        wrong_owner_arg);
    driver_buffer = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_BUFFER,
        driver_probe_cap,
        driver_intent,
        driver_probe_owner_arg);
    denied_driver_gate = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_GATE,
        driver_probe_cap,
        driver_buffer,
        wrong_owner_arg);
    driver_gate = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_GATE,
        driver_probe_cap,
        driver_buffer,
        driver_probe_owner_arg);
    denied_driver_exec = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_EXECUTE,
        driver_probe_cap,
        driver_gate,
        wrong_owner_arg);
    driver_exec = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_EXECUTE,
        driver_probe_cap,
        driver_gate,
        driver_probe_owner_arg);
    denied_driver_result = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RESULT,
        driver_probe_cap,
        driver_exec,
        wrong_owner_arg);
    driver_result = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RESULT,
        driver_probe_cap,
        driver_exec,
        driver_probe_owner_arg);
    denied_driver_publish = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_BLOCK_PUBLISH,
        driver_probe_cap,
        driver_result,
        wrong_owner_arg);
    driver_publish = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_BLOCK_PUBLISH,
        driver_probe_cap,
        driver_result,
        driver_probe_owner_arg);
    denied_driver_read_grant = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_GRANT,
        driver_probe_cap,
        driver_publish,
        wrong_owner_arg);
    driver_read_grant = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_GRANT,
        driver_probe_cap,
        driver_publish,
        driver_probe_owner_arg);
    denied_driver_media_read = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_MEDIA_READ,
        driver_probe_cap,
        driver_read_grant,
        wrong_owner_arg);
    driver_media_read = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_MEDIA_READ,
        driver_probe_cap,
        driver_read_grant,
        driver_probe_owner_arg);
    denied_driver_complete = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_COMPLETE,
        driver_probe_cap,
        driver_media_read,
        wrong_owner_arg);
    driver_complete = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_COMPLETE,
        driver_probe_cap,
        driver_media_read,
        driver_probe_owner_arg);
    denied_driver_read_cap = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_CAP,
        driver_probe_cap,
        driver_complete,
        wrong_owner_arg);
    driver_read_cap = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_CAP,
        driver_probe_cap,
        driver_complete,
        driver_probe_owner_arg);
    denied_driver_read_export = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_EXPORT,
        driver_probe_cap,
        driver_read_cap,
        wrong_owner_arg);
    driver_read_export = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_EXPORT,
        driver_probe_cap,
        driver_read_cap,
        driver_probe_owner_arg);
    denied_driver_read_response = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RESPONSE,
        driver_probe_cap,
        driver_read_export,
        wrong_owner_arg);
    driver_read_response = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RESPONSE,
        driver_probe_cap,
        driver_read_export,
        driver_probe_owner_arg);
    denied_driver_read_delivery = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DELIVERY,
        driver_probe_cap,
        driver_read_response,
        wrong_owner_arg);
    driver_read_delivery = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DELIVERY,
        driver_probe_cap,
        driver_read_response,
        driver_probe_owner_arg);
    denied_driver_read_visible = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_VISIBLE,
        driver_probe_cap,
        driver_read_delivery,
        wrong_owner_arg);
    driver_read_visible = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_VISIBLE,
        driver_probe_cap,
        driver_read_delivery,
        driver_probe_owner_arg);
    denied_driver_read_commit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_COMMIT,
        driver_probe_cap,
        driver_read_visible,
        wrong_owner_arg);
    driver_read_commit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_COMMIT,
        driver_probe_cap,
        driver_read_visible,
        driver_probe_owner_arg);
    denied_driver_read_audit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_AUDIT,
        driver_probe_cap,
        driver_read_commit,
        wrong_owner_arg);
    driver_read_audit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_AUDIT,
        driver_probe_cap,
        driver_read_commit,
        driver_probe_owner_arg);
    denied_driver_read_upgrade = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_UPGRADE,
        driver_probe_cap,
        driver_read_audit,
        wrong_owner_arg);
    driver_read_upgrade = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_UPGRADE,
        driver_probe_cap,
        driver_read_audit,
        driver_probe_owner_arg);
    denied_driver_read_activate = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ACTIVATE,
        driver_probe_cap,
        driver_read_upgrade,
        wrong_owner_arg);
    driver_read_activate = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ACTIVATE,
        driver_probe_cap,
        driver_read_upgrade,
        driver_probe_owner_arg);
    denied_driver_read_arm = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ARM,
        driver_probe_cap,
        driver_read_activate,
        wrong_owner_arg);
    driver_read_arm = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ARM,
        driver_probe_cap,
        driver_read_activate,
        driver_probe_owner_arg);
    denied_driver_read_submit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SUBMIT,
        driver_probe_cap,
        driver_read_arm,
        wrong_owner_arg);
    driver_read_submit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SUBMIT,
        driver_probe_cap,
        driver_read_arm,
        driver_probe_owner_arg);
    denied_driver_read_observe = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_OBSERVE,
        driver_probe_cap,
        driver_read_submit,
        wrong_owner_arg);
    driver_read_observe = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_OBSERVE,
        driver_probe_cap,
        driver_read_submit,
        driver_probe_owner_arg);
    denied_driver_read_retire = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RETIRE,
        driver_probe_cap,
        driver_read_observe,
        wrong_owner_arg);
    driver_read_retire = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RETIRE,
        driver_probe_cap,
        driver_read_observe,
        driver_probe_owner_arg);
    denied_driver_read_permit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_PERMIT,
        driver_probe_cap,
        driver_read_retire,
        wrong_owner_arg);
    driver_read_permit = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_PERMIT,
        driver_probe_cap,
        driver_read_retire,
        driver_probe_owner_arg);
    denied_driver_read_window = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_WINDOW,
        driver_probe_cap,
        driver_read_permit,
        wrong_owner_arg);
    driver_read_window = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_WINDOW,
        driver_probe_cap,
        driver_read_permit,
        driver_probe_owner_arg);
    denied_driver_read_lease = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_LEASE,
        driver_probe_cap,
        driver_read_window,
        wrong_owner_arg);
    driver_read_lease = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_LEASE,
        driver_probe_cap,
        driver_read_window,
        driver_probe_owner_arg);
    denied_driver_read_use = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_USE,
        driver_probe_cap,
        driver_read_lease,
        wrong_owner_arg);
    driver_read_use = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_USE,
        driver_probe_cap,
        driver_read_lease,
        driver_probe_owner_arg);
    denied_driver_read_report = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_REPORT,
        driver_probe_cap,
        driver_read_use,
        wrong_owner_arg);
    driver_read_report = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_REPORT,
        driver_probe_cap,
        driver_read_use,
        driver_probe_owner_arg);
    denied_driver_read_receipt = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RECEIPT,
        driver_probe_cap,
        driver_read_report,
        wrong_owner_arg);
    driver_read_receipt = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RECEIPT,
        driver_probe_cap,
        driver_read_report,
        driver_probe_owner_arg);
    denied_driver_read_ack = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ACK,
        driver_probe_cap,
        driver_read_receipt,
        wrong_owner_arg);
    driver_read_ack = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ACK,
        driver_probe_cap,
        driver_read_receipt,
        driver_probe_owner_arg);
    denied_driver_read_close = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_CLOSE,
        driver_probe_cap,
        driver_read_ack,
        wrong_owner_arg);
    driver_read_close = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_CLOSE,
        driver_probe_cap,
        driver_read_ack,
        driver_probe_owner_arg);
    denied_driver_read_seal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SEAL,
        driver_probe_cap,
        driver_read_close,
        wrong_owner_arg);
    driver_read_seal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SEAL,
        driver_probe_cap,
        driver_read_close,
        driver_probe_owner_arg);
    denied_driver_read_unseal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_UNSEAL,
        driver_probe_cap,
        driver_read_seal,
        wrong_owner_arg);
    driver_read_unseal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_UNSEAL,
        driver_probe_cap,
        driver_read_seal,
        driver_probe_owner_arg);
    denied_driver_read_discard = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DISCARD,
        driver_probe_cap,
        driver_read_unseal,
        wrong_owner_arg);
    driver_read_discard = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DISCARD,
        driver_probe_cap,
        driver_read_unseal,
        driver_probe_owner_arg);
    denied_driver_read_finalize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_FINALIZE,
        driver_probe_cap,
        driver_read_discard,
        wrong_owner_arg);
    driver_read_finalize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_FINALIZE,
        driver_probe_cap,
        driver_read_discard,
        driver_probe_owner_arg);
    denied_driver_read_authorize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_AUTHORIZE,
        driver_probe_cap,
        driver_read_finalize,
        wrong_owner_arg);
    driver_read_authorize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_AUTHORIZE,
        driver_probe_cap,
        driver_read_finalize,
        driver_probe_owner_arg);
    denied_driver_read_dispatch = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DISPATCH,
        driver_probe_cap,
        driver_read_authorize,
        wrong_owner_arg);
    driver_read_dispatch = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DISPATCH,
        driver_probe_cap,
        driver_read_authorize,
        driver_probe_owner_arg);
    denied_driver_read_queue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_QUEUE,
        driver_probe_cap,
        driver_read_dispatch,
        wrong_owner_arg);
    driver_read_queue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_QUEUE,
        driver_probe_cap,
        driver_read_dispatch,
        driver_probe_owner_arg);
    denied_driver_read_worker = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_WORKER,
        driver_probe_cap,
        driver_read_queue,
        wrong_owner_arg);
    driver_read_worker = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_WORKER,
        driver_probe_cap,
        driver_read_queue,
        driver_probe_owner_arg);
    denied_driver_read_schedule = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SCHEDULE,
        driver_probe_cap,
        driver_read_worker,
        wrong_owner_arg);
    driver_read_schedule = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_SCHEDULE,
        driver_probe_cap,
        driver_read_worker,
        driver_probe_owner_arg);
    denied_driver_read_run = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RUN,
        driver_probe_cap,
        driver_read_schedule,
        wrong_owner_arg);
    driver_read_run = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_RUN,
        driver_probe_cap,
        driver_read_schedule,
        driver_probe_owner_arg);
    denied_driver_read_body = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_BODY,
        driver_probe_cap,
        driver_read_run,
        wrong_owner_arg);
    driver_read_body = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_BODY,
        driver_probe_cap,
        driver_read_run,
        driver_probe_owner_arg);
    denied_driver_read_issue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ISSUE,
        driver_probe_cap,
        driver_read_body,
        wrong_owner_arg);
    driver_read_issue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_ISSUE,
        driver_probe_cap,
        driver_read_body,
        driver_probe_owner_arg);
    denied_driver_read_dma = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DMA,
        driver_probe_cap,
        driver_read_issue,
        wrong_owner_arg);
    driver_read_dma = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_DMA,
        driver_probe_cap,
        driver_read_issue,
        driver_probe_owner_arg);
    denied_driver_read_irq = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_IRQ,
        driver_probe_cap,
        driver_read_dma,
        wrong_owner_arg);
    driver_read_irq = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_IRQ,
        driver_probe_cap,
        driver_read_dma,
        driver_probe_owner_arg);
    denied_driver_read_status = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS,
        driver_probe_cap,
        driver_read_irq,
        wrong_owner_arg);
    driver_read_status = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS,
        driver_probe_cap,
        driver_read_irq,
        driver_probe_owner_arg);
    denied_driver_read_status_result = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RESULT,
        driver_probe_cap,
        driver_read_status,
        wrong_owner_arg);
    driver_read_status_result = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RESULT,
        driver_probe_cap,
        driver_read_status,
        driver_probe_owner_arg);
    denied_driver_read_status_sample = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_SAMPLE,
        driver_probe_cap,
        driver_read_status_result,
        wrong_owner_arg);
    driver_read_status_sample = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_SAMPLE,
        driver_probe_cap,
        driver_read_status_result,
        driver_probe_owner_arg);
    denied_driver_read_status_clear = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLEAR,
        driver_probe_cap,
        driver_read_status_sample,
        wrong_owner_arg);
    driver_read_status_clear = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLEAR,
        driver_probe_cap,
        driver_read_status_sample,
        driver_probe_owner_arg);
    denied_driver_read_status_clear_result = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT,
        driver_probe_cap,
        driver_read_status_clear,
        wrong_owner_arg);
    driver_read_status_clear_result = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT,
        driver_probe_cap,
        driver_read_status_clear,
        driver_probe_owner_arg);
    denied_driver_read_status_resample = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RESAMPLE,
        driver_probe_cap,
        driver_read_status_clear_result,
        wrong_owner_arg);
    driver_read_status_resample = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RESAMPLE,
        driver_probe_cap,
        driver_read_status_clear_result,
        driver_probe_owner_arg);
    denied_driver_read_status_stable = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_STABLE,
        driver_probe_cap,
        driver_read_status_resample,
        wrong_owner_arg);
    driver_read_status_stable = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_STABLE,
        driver_probe_cap,
        driver_read_status_resample,
        driver_probe_owner_arg);
    denied_driver_read_status_guard = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_GUARD,
        driver_probe_cap,
        driver_read_status_stable,
        wrong_owner_arg);
    driver_read_status_guard = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_GUARD,
        driver_probe_cap,
        driver_read_status_stable,
        driver_probe_owner_arg);
    denied_driver_read_status_buffer = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_BUFFER,
        driver_probe_cap,
        driver_read_status_guard,
        wrong_owner_arg);
    driver_read_status_buffer = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_BUFFER,
        driver_probe_cap,
        driver_read_status_guard,
        driver_probe_owner_arg);
    denied_driver_read_status_export = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_EXPORT,
        driver_probe_cap,
        driver_read_status_buffer,
        wrong_owner_arg);
    driver_read_status_export = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_EXPORT,
        driver_probe_cap,
        driver_read_status_buffer,
        driver_probe_owner_arg);
    denied_driver_read_status_report = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_REPORT,
        driver_probe_cap,
        driver_read_status_export,
        wrong_owner_arg);
    driver_read_status_report = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_REPORT,
        driver_probe_cap,
        driver_read_status_export,
        driver_probe_owner_arg);
    denied_driver_read_status_receipt = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RECEIPT,
        driver_probe_cap,
        driver_read_status_report,
        wrong_owner_arg);
    driver_read_status_receipt = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_RECEIPT,
        driver_probe_cap,
        driver_read_status_report,
        driver_probe_owner_arg);
    denied_driver_read_status_ack = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ACK,
        driver_probe_cap,
        driver_read_status_receipt,
        wrong_owner_arg);
    driver_read_status_ack = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ACK,
        driver_probe_cap,
        driver_read_status_receipt,
        driver_probe_owner_arg);
    denied_driver_read_status_close = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLOSE,
        driver_probe_cap,
        driver_read_status_ack,
        wrong_owner_arg);
    driver_read_status_close = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_CLOSE,
        driver_probe_cap,
        driver_read_status_ack,
        driver_probe_owner_arg);
    denied_driver_read_status_seal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_SEAL,
        driver_probe_cap,
        driver_read_status_close,
        wrong_owner_arg);
    driver_read_status_seal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_SEAL,
        driver_probe_cap,
        driver_read_status_close,
        driver_probe_owner_arg);
    denied_driver_read_status_unseal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_UNSEAL,
        driver_probe_cap,
        driver_read_status_seal,
        wrong_owner_arg);
    driver_read_status_unseal = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_UNSEAL,
        driver_probe_cap,
        driver_read_status_seal,
        driver_probe_owner_arg);
    denied_driver_read_status_discard = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DISCARD,
        driver_probe_cap,
        driver_read_status_unseal,
        wrong_owner_arg);
    driver_read_status_discard = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DISCARD,
        driver_probe_cap,
        driver_read_status_unseal,
        driver_probe_owner_arg);
    denied_driver_read_status_finalize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FINALIZE,
        driver_probe_cap,
        driver_read_status_discard,
        wrong_owner_arg);
    driver_read_status_finalize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FINALIZE,
        driver_probe_cap,
        driver_read_status_discard,
        driver_probe_owner_arg);
    denied_driver_read_status_authorize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_AUTHORIZE,
        driver_probe_cap,
        driver_read_status_finalize,
        wrong_owner_arg);
    driver_read_status_authorize = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_AUTHORIZE,
        driver_probe_cap,
        driver_read_status_finalize,
        driver_probe_owner_arg);
    denied_driver_read_status_dispatch = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DISPATCH,
        driver_probe_cap,
        driver_read_status_authorize,
        wrong_owner_arg);
    driver_read_status_dispatch = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DISPATCH,
        driver_probe_cap,
        driver_read_status_authorize,
        driver_probe_owner_arg);
    denied_driver_read_status_queue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_QUEUE,
        driver_probe_cap,
        driver_read_status_dispatch,
        wrong_owner_arg);
    driver_read_status_queue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_QUEUE,
        driver_probe_cap,
        driver_read_status_dispatch,
        driver_probe_owner_arg);
    denied_driver_read_status_worker = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_WORKER,
        driver_probe_cap,
        driver_read_status_queue,
        wrong_owner_arg);
    driver_read_status_worker = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_WORKER,
        driver_probe_cap,
        driver_read_status_queue,
        driver_probe_owner_arg);
    denied_driver_read_status_read_authority = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY,
        driver_probe_cap,
        driver_read_status_worker,
        wrong_owner_arg);
    driver_read_status_read_authority = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY,
        driver_probe_cap,
        driver_read_status_worker,
        driver_probe_owner_arg);
    denied_driver_read_status_descriptor = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DESCRIPTOR,
        driver_probe_cap,
        driver_read_status_read_authority,
        wrong_owner_arg);
    driver_read_status_descriptor = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DESCRIPTOR,
        driver_probe_cap,
        driver_read_status_read_authority,
        driver_probe_owner_arg);
    denied_driver_read_status_command_table = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE,
        driver_probe_cap,
        driver_read_status_descriptor,
        wrong_owner_arg);
    driver_read_status_command_table = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE,
        driver_probe_cap,
        driver_read_status_descriptor,
        driver_probe_owner_arg);
    denied_driver_read_status_command_issue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE,
        driver_probe_cap,
        driver_read_status_command_table,
        wrong_owner_arg);
    driver_read_status_command_issue = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE,
        driver_probe_cap,
        driver_read_status_command_table,
        driver_probe_owner_arg);
    denied_driver_read_status_issue_grant = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT,
        driver_probe_cap,
        driver_read_status_command_issue,
        wrong_owner_arg);
    driver_read_status_issue_grant = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT,
        driver_probe_cap,
        driver_read_status_command_issue,
        driver_probe_owner_arg);
    denied_driver_read_status_arm = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ARM,
        driver_probe_cap,
        driver_read_status_issue_grant,
        wrong_owner_arg);
    driver_read_status_arm = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_ARM,
        driver_probe_cap,
        driver_read_status_issue_grant,
        driver_probe_owner_arg);
    denied_driver_read_status_exec = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_EXEC,
        driver_probe_cap,
        driver_read_status_arm,
        wrong_owner_arg);
    driver_read_status_exec = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_EXEC,
        driver_probe_cap,
        driver_read_status_arm,
        driver_probe_owner_arg);
    denied_driver_read_status_dma = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DMA,
        driver_probe_cap,
        driver_read_status_exec,
        wrong_owner_arg);
    driver_read_status_dma = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DMA,
        driver_probe_cap,
        driver_read_status_exec,
        driver_probe_owner_arg);
    denied_driver_read_status_mmio = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_MMIO,
        driver_probe_cap,
        driver_read_status_dma,
        wrong_owner_arg);
    if ((driver_read_status_dma != 0u)
        && (driver_read_status_dma != 0xFFFFFFFFu))
    {
        stale_driver_read_status_mmio = (u32)syscall64_invoke(
            X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_MMIO,
            driver_probe_cap,
            driver_read_status_dma ^ 0x5A5A5A5Au,
            driver_probe_owner_arg);
    }
    driver_read_status_mmio = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_MMIO,
        driver_probe_cap,
        driver_read_status_dma,
        driver_probe_owner_arg);
    denied_driver_read_status_dma_window = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DMA_WINDOW,
        driver_probe_cap,
        driver_read_status_mmio,
        wrong_owner_arg);
    if ((driver_read_status_mmio != 0u)
        && (driver_read_status_mmio != 0xFFFFFFFFu))
    {
        stale_driver_read_status_dma_window = (u32)syscall64_invoke(
            X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DMA_WINDOW,
            driver_probe_cap,
            driver_read_status_mmio ^ 0xA5A5A5A5u,
            driver_probe_owner_arg);
    }
    driver_read_status_dma_window = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_DMA_WINDOW,
        driver_probe_cap,
        driver_read_status_mmio,
        driver_probe_owner_arg);
    driver_read_status_read = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_READ,
        driver_probe_cap,
        driver_read_status_dma_window,
        driver_probe_owner_arg);
    denied_driver_read_status_block = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_BLOCK,
        driver_probe_cap,
        driver_read_status_read,
        wrong_owner_arg);
    if ((driver_read_status_read != 0u)
        && (driver_read_status_read != 0xFFFFFFFFu))
    {
        stale_driver_read_status_block = (u32)syscall64_invoke(
            X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_BLOCK,
            driver_probe_cap,
            driver_read_status_read ^ 0xA5A5A5A5u,
            driver_probe_owner_arg);
    }
    driver_read_status_block = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_BLOCK,
        driver_probe_cap,
        driver_read_status_read,
        driver_probe_owner_arg);
    driver_read_status_block_cap = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BLOCK_TELEMETRY,
        MMIO64_AHCI_DRIVER_READ_STATUS_BLOCK_TELEMETRY_CAPABILITY,
        0u,
        0u);
    driver_read_status_fs = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FS,
        driver_read_status_block_cap,
        driver_read_status_block,
        driver_probe_owner_arg);
    driver_read_status_fs_user = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FS_USER,
        driver_read_status_fs,
        (u64)drs_fs_user_path,
        driver_probe_owner_arg);
    driver_read_status_fs_shell = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_STAGE_AHCI_DRIVER_READ_STATUS_FS_SHELL,
        driver_read_status_fs_user,
        0u,
        driver_probe_owner_arg);
    write_labeled_hex_u32(" denied-driver-probe ", denied_driver_probe);
    write_labeled_hex_u32(" driver-probe ", driver_probe);
    static const u16 syscall0_compact_fields_14[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4015u, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4049u, 0x4052u, 0x4057u, 0x405Bu,
        0x4064u, 0x406Au, 0x4070u, 0x4075u, 0x407Au, 0x407Fu, 0x4083u, 0x0089u, 0x008Fu, 0x009Bu, 0x00A1u, 0x00A6u,
        0x00AFu, 0x00BBu, 0x00BFu, 0x00C4u, 0x00CCu, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0119u,
        0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-probe-", X64_SYSCALL_MMIO_AHCI_DRIVER_PROBE_STATE, syscall0_compact_fields_14, (u32)(sizeof(syscall0_compact_fields_14) / sizeof(syscall0_compact_fields_14[0])));
    write_labeled_hex_u32(" denied-driver-intent ", denied_driver_intent);
    write_labeled_hex_u32(" driver-intent ", driver_intent);
    static const u16 syscall0_compact_fields_15[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4137u, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x00CCu, 0x008Fu, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0119u, 0x0121u,
        0x012Au
    };
    write_syscall0_compact_fields(" driver-intent-", X64_SYSCALL_MMIO_AHCI_DRIVER_INTENT_STATE, syscall0_compact_fields_15, (u32)(sizeof(syscall0_compact_fields_15) / sizeof(syscall0_compact_fields_15[0])));
    write_labeled_hex_u32(" denied-driver-buffer ", denied_driver_buffer);
    write_labeled_hex_u32(" driver-buffer ", driver_buffer);
    static const u16 syscall0_compact_fields_16[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4150u, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x0176u, 0x417Eu, 0x0188u, 0x008Fu, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du,
        0x0112u, 0x0144u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-buffer-", X64_SYSCALL_MMIO_AHCI_DRIVER_BUFFER_STATE, syscall0_compact_fields_16, (u32)(sizeof(syscall0_compact_fields_16) / sizeof(syscall0_compact_fields_16[0])));
    write_labeled_hex_u32(" denied-driver-gate ", denied_driver_gate);
    write_labeled_hex_u32(" driver-gate ", driver_gate);
    static const u16 syscall0_compact_fields_17[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4190u, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x019Eu, 0x01ADu, 0x01BBu, 0x00D3u, 0x00E1u, 0x00F2u,
        0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-gate-", X64_SYSCALL_MMIO_AHCI_DRIVER_GATE_STATE, syscall0_compact_fields_17, (u32)(sizeof(syscall0_compact_fields_17) / sizeof(syscall0_compact_fields_17[0])));
    write_labeled_hex_u32(" denied-driver-exec ", denied_driver_exec);
    write_labeled_hex_u32(" driver-exec ", driver_exec);
    static const u16 syscall0_compact_fields_18[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x41CAu, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x01D6u, 0x01E1u, 0x01EBu, 0x01BBu, 0x01F4u, 0x00D3u,
        0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-exec-", X64_SYSCALL_MMIO_AHCI_DRIVER_EXEC_STATE, syscall0_compact_fields_18, (u32)(sizeof(syscall0_compact_fields_18) / sizeof(syscall0_compact_fields_18[0])));
    write_labeled_hex_u32(" denied-driver-result ", denied_driver_result);
    write_labeled_hex_u32(" driver-result ", driver_result);
    static const u16 syscall0_compact_fields_19[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4202u, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x020Eu, 0x021Bu, 0x01EBu, 0x0226u, 0x022Eu, 0x023Fu,
        0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-result-", X64_SYSCALL_MMIO_AHCI_DRIVER_RESULT_STATE, syscall0_compact_fields_19, (u32)(sizeof(syscall0_compact_fields_19) / sizeof(syscall0_compact_fields_19[0])));
    write_labeled_hex_u32(" denied-driver-publish ", denied_driver_publish);
    write_labeled_hex_u32(" driver-publish ", driver_publish);
    static const u16 syscall0_compact_fields_20[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x425Cu, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x020Eu, 0x026Au, 0x022Eu, 0x021Bu, 0x01EBu, 0x0226u,
        0x0279u, 0x023Fu, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x0119u,
        0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-publish-", X64_SYSCALL_MMIO_AHCI_DRIVER_PUBLISH_STATE, syscall0_compact_fields_20, (u32)(sizeof(syscall0_compact_fields_20) / sizeof(syscall0_compact_fields_20[0])));
    write_labeled_hex_u32(" denied-drg ", denied_driver_read_grant);
    write_labeled_hex_u32(" drg ", driver_read_grant);
    static const u16 syscall0_compact_fields_21[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4298u, 0x425Cu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu,
        0x00BFu, 0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x020Eu, 0x026Au, 0x02B1u, 0x00CCu, 0x021Bu,
        0x01EBu, 0x0226u, 0x02BDu, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u,
        0x0144u, 0x0289u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drg-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_GRANT_STATE, syscall0_compact_fields_21, (u32)(sizeof(syscall0_compact_fields_21) / sizeof(syscall0_compact_fields_21[0])));
    write_labeled_hex_u32(" denied-dmr ", denied_driver_media_read);
    write_labeled_hex_u32(" dmr ", driver_media_read);
    static const u16 syscall0_compact_fields_22[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x42D4u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x02E1u, 0x02EDu, 0x01D6u, 0x0226u, 0x00CCu, 0x0279u,
        0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u,
        0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dmr-", X64_SYSCALL_MMIO_AHCI_DRIVER_MEDIA_READ_STATE, syscall0_compact_fields_22, (u32)(sizeof(syscall0_compact_fields_22) / sizeof(syscall0_compact_fields_22[0])));
    write_labeled_hex_u32(" denied-drc ", denied_driver_complete);
    write_labeled_hex_u32(" drc ", driver_complete);
    static const u16 syscall0_compact_fields_23[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x42FBu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0306u, 0x021Bu, 0x01EBu, 0x0226u, 0x0312u, 0x031Du,
        0x00CCu, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u,
        0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drc-", X64_SYSCALL_MMIO_AHCI_DRIVER_COMPLETE_STATE, syscall0_compact_fields_23, (u32)(sizeof(syscall0_compact_fields_23) / sizeof(syscall0_compact_fields_23[0])));
    write_labeled_hex_u32(" denied-drcap ", denied_driver_read_cap);
    write_labeled_hex_u32(" drcap ", driver_read_cap);
    static const u16 syscall0_compact_fields_24[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4325u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0330u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x0279u,
        0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u,
        0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drcap-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CAP_STATE, syscall0_compact_fields_24, (u32)(sizeof(syscall0_compact_fields_24) / sizeof(syscall0_compact_fields_24[0])));
    write_labeled_hex_u32(" denied-drx ", denied_driver_read_export);
    write_labeled_hex_u32(" drx ", driver_read_export);
    static const u16 syscall0_compact_fields_25[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x433Cu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0349u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x0357u,
        0x0363u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u,
        0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drx-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_EXPORT_STATE, syscall0_compact_fields_25, (u32)(sizeof(syscall0_compact_fields_25) / sizeof(syscall0_compact_fields_25[0])));
    write_labeled_hex_u32(" denied-drr ", denied_driver_read_response);
    write_labeled_hex_u32(" drr ", driver_read_response);
    static const u16 syscall0_compact_fields_26[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4370u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x037Bu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x0387u,
        0x0393u, 0x43A0u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drr-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RESPONSE_STATE, syscall0_compact_fields_26, (u32)(sizeof(syscall0_compact_fields_26) / sizeof(syscall0_compact_fields_26[0])));
    write_labeled_hex_u32(" denied-drd ", denied_driver_read_delivery);
    write_labeled_hex_u32(" drd ", driver_read_delivery);
    static const u16 syscall0_compact_fields_27[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x43AFu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x03BAu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x03C6u,
        0x03D3u, 0x43E1u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drd-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DELIVERY_STATE, syscall0_compact_fields_27, (u32)(sizeof(syscall0_compact_fields_27) / sizeof(syscall0_compact_fields_27[0])));
    write_labeled_hex_u32(" denied-drv ", denied_driver_read_visible);
    write_labeled_hex_u32(" drv ", driver_read_visible);
    static const u16 syscall0_compact_fields_28[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x43F1u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x03FCu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x0408u,
        0x0413u, 0x441Fu, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drv-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_VISIBLE_STATE, syscall0_compact_fields_28, (u32)(sizeof(syscall0_compact_fields_28) / sizeof(syscall0_compact_fields_28[0])));
    write_labeled_hex_u32(" denied-drk ", denied_driver_read_commit);
    write_labeled_hex_u32(" drk ", driver_read_commit);
    static const u16 syscall0_compact_fields_29[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x442Du, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0438u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x0444u,
        0x0452u, 0x4461u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drk-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_COMMIT_STATE, syscall0_compact_fields_29, (u32)(sizeof(syscall0_compact_fields_29) / sizeof(syscall0_compact_fields_29[0])));
    write_labeled_hex_u32(" denied-dra ", denied_driver_read_audit);
    write_labeled_hex_u32(" dra ", driver_read_audit);
    static const u16 syscall0_compact_fields_30[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4472u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x047Du, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x0489u,
        0x0496u, 0x44A4u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dra-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUDIT_STATE, syscall0_compact_fields_30, (u32)(sizeof(syscall0_compact_fields_30) / sizeof(syscall0_compact_fields_30[0])));
    write_labeled_hex_u32(" denied-dru ", denied_driver_read_upgrade);
    write_labeled_hex_u32(" dru ", driver_read_upgrade);
    static const u16 syscall0_compact_fields_31[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x44B4u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x04BFu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x44CBu,
        0x02BDu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dru-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UPGRADE_STATE, syscall0_compact_fields_31, (u32)(sizeof(syscall0_compact_fields_31) / sizeof(syscall0_compact_fields_31[0])));
    write_labeled_hex_u32(" denied-dact ", denied_driver_read_activate);
    write_labeled_hex_u32(" dact ", driver_read_activate);
    static const u16 syscall0_compact_fields_32[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x44DEu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x04E9u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x44F5u,
        0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dact-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACTIVATE_STATE, syscall0_compact_fields_32, (u32)(sizeof(syscall0_compact_fields_32) / sizeof(syscall0_compact_fields_32[0])));
    write_labeled_hex_u32(" denied-darm ", denied_driver_read_arm);
    write_labeled_hex_u32(" darm ", driver_read_arm);
    static const u16 syscall0_compact_fields_33[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4509u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0515u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x4522u,
        0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" darm-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ARM_STATE, syscall0_compact_fields_33, (u32)(sizeof(syscall0_compact_fields_33) / sizeof(syscall0_compact_fields_33[0])));
    write_labeled_hex_u32(" denied-dsub ", denied_driver_read_submit);
    write_labeled_hex_u32(" dsub ", driver_read_submit);
    static const u16 syscall0_compact_fields_34[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x452Bu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0537u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x4544u,
        0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dsub-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SUBMIT_STATE, syscall0_compact_fields_34, (u32)(sizeof(syscall0_compact_fields_34) / sizeof(syscall0_compact_fields_34[0])));
    write_labeled_hex_u32(" denied-dobs ", denied_driver_read_observe);
    write_labeled_hex_u32(" dobs ", driver_read_observe);
    static const u16 syscall0_compact_fields_35[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4550u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x055Cu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x0569u,
        0x0575u, 0x4580u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du,
        0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dobs-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_OBSERVE_STATE, syscall0_compact_fields_35, (u32)(sizeof(syscall0_compact_fields_35) / sizeof(syscall0_compact_fields_35[0])));
    write_labeled_hex_u32(" denied-dret ", denied_driver_read_retire);
    write_labeled_hex_u32(" dret ", driver_read_retire);
    static const u16 syscall0_compact_fields_36[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x458Eu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x059Au, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x05A7u,
        0x05B3u, 0x45BEu, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du,
        0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dret-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RETIRE_STATE, syscall0_compact_fields_36, (u32)(sizeof(syscall0_compact_fields_36) / sizeof(syscall0_compact_fields_36[0])));
    write_labeled_hex_u32(" denied-dprm ", denied_driver_read_permit);
    write_labeled_hex_u32(" dprm ", driver_read_permit);
    static const u16 syscall0_compact_fields_37[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x45CCu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x05D8u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x45E5u,
        0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u,
        0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dprm-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_PERMIT_STATE, syscall0_compact_fields_37, (u32)(sizeof(syscall0_compact_fields_37) / sizeof(syscall0_compact_fields_37[0])));
    write_labeled_hex_u32(" denied-dwin ", denied_driver_read_window);
    write_labeled_hex_u32(" dwin ", driver_read_window);
    static const u16 syscall0_compact_fields_38[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x45F1u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x05FDu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x460Au,
        0x0616u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u,
        0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dwin-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WINDOW_STATE, syscall0_compact_fields_38, (u32)(sizeof(syscall0_compact_fields_38) / sizeof(syscall0_compact_fields_38[0])));
    write_labeled_hex_u32(" denied-dlse ", denied_driver_read_lease);
    write_labeled_hex_u32(" dlse ", driver_read_lease);
    static const u16 syscall0_compact_fields_39[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x461Cu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0628u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x4635u,
        0x0640u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u,
        0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" dlse-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_LEASE_STATE, syscall0_compact_fields_39, (u32)(sizeof(syscall0_compact_fields_39) / sizeof(syscall0_compact_fields_39[0])));
    write_labeled_hex_u32(" denied-duse ", denied_driver_read_use);
    write_labeled_hex_u32(" duse ", driver_read_use);
    static const u16 syscall0_compact_fields_40[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4648u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0654u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x4661u,
        0x0640u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u,
        0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" duse-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_USE_STATE, syscall0_compact_fields_40, (u32)(sizeof(syscall0_compact_fields_40) / sizeof(syscall0_compact_fields_40[0])));
    write_labeled_hex_u32(" denied-drpt ", denied_driver_read_report);
    write_labeled_hex_u32(" drpt ", driver_read_report);
    static const u16 syscall0_compact_fields_41[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x466Au, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0676u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x031Du,
        0x0683u, 0x4691u, 0x46A2u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drpt-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_REPORT_STATE, syscall0_compact_fields_41, (u32)(sizeof(syscall0_compact_fields_41) / sizeof(syscall0_compact_fields_41[0])));
    write_labeled_hex_u32(" denied-drrc ", denied_driver_read_receipt);
    write_labeled_hex_u32(" drrc ", driver_read_receipt);
    static const u16 syscall0_compact_fields_42[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x46AEu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x06BAu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x031Du,
        0x06C7u, 0x46D6u, 0x46E8u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drrc-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RECEIPT_STATE, syscall0_compact_fields_42, (u32)(sizeof(syscall0_compact_fields_42) / sizeof(syscall0_compact_fields_42[0])));
    write_labeled_hex_u32(" denied-drak ", denied_driver_read_ack);
    write_labeled_hex_u32(" drak ", driver_read_ack);
    static const u16 syscall0_compact_fields_43[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x46F5u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0701u, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x031Du,
        0x070Eu, 0x4719u, 0x4727u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drak-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ACK_STATE, syscall0_compact_fields_43, (u32)(sizeof(syscall0_compact_fields_43) / sizeof(syscall0_compact_fields_43[0])));
    write_labeled_hex_u32(" denied-drcl ", denied_driver_read_close);
    write_labeled_hex_u32(" drcl ", driver_read_close);
    static const u16 syscall0_compact_fields_44[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4730u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x073Cu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x031Du,
        0x0749u, 0x4756u, 0x4766u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drcl-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_CLOSE_STATE, syscall0_compact_fields_44, (u32)(sizeof(syscall0_compact_fields_44) / sizeof(syscall0_compact_fields_44[0])));
    write_labeled_hex_u32(" denied-drsl ", denied_driver_read_seal);
    write_labeled_hex_u32(" drsl ", driver_read_seal);
    static const u16 syscall0_compact_fields_45[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4771u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x077Du, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x031Du,
        0x078Au, 0x4796u, 0x47A5u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drsl-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SEAL_STATE, syscall0_compact_fields_45, (u32)(sizeof(syscall0_compact_fields_45) / sizeof(syscall0_compact_fields_45[0])));
    write_labeled_hex_u32(" denied-drul ", denied_driver_read_unseal);
    write_labeled_hex_u32(" drul ", driver_read_unseal);
    static const u16 syscall0_compact_fields_46[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x47AFu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x07BBu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x031Du,
        0x07C8u, 0x47D6u, 0x47E7u, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drul-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_UNSEAL_STATE, syscall0_compact_fields_46, (u32)(sizeof(syscall0_compact_fields_46) / sizeof(syscall0_compact_fields_46[0])));
    write_labeled_hex_u32(" denied-drdc ", denied_driver_read_discard);
    write_labeled_hex_u32(" drdc ", driver_read_discard);
    static const u16 syscall0_compact_fields_47[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x47F3u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x07FFu, 0x021Bu, 0x01EBu, 0x0226u, 0x00CCu, 0x031Du,
        0x080Cu, 0x481Bu, 0x482Du, 0x04FEu, 0x04D3u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drdc-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISCARD_STATE, syscall0_compact_fields_47, (u32)(sizeof(syscall0_compact_fields_47) / sizeof(syscall0_compact_fields_47[0])));
    write_labeled_hex_u32(" denied-driver-read-finalize ", denied_driver_read_finalize);
    write_labeled_hex_u32(" driver-read-finalize ", driver_read_finalize);
    static const u16 syscall0_compact_fields_48[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x483Au, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x084Eu, 0x021Bu, 0x01EBu, 0x0226u, 0x022Eu, 0x031Du,
        0x0863u, 0x4874u, 0x4887u, 0x0895u, 0x08A5u, 0x0279u, 0x023Fu, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu,
        0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x08B8u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-read-finalize-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_FINALIZE_STATE, syscall0_compact_fields_48, (u32)(sizeof(syscall0_compact_fields_48) / sizeof(syscall0_compact_fields_48[0])));
    write_labeled_hex_u32(" denied-driver-read-authorize ", denied_driver_read_authorize);
    write_labeled_hex_u32(" driver-read-authorize ", driver_read_authorize);
    static const u16 syscall0_compact_fields_49[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x48CAu, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x08DFu, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x0903u,
        0x0914u, 0x0923u, 0x0939u, 0x094Au, 0x0279u, 0x023Fu, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du,
        0x0112u, 0x0144u, 0x0289u, 0x08B8u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-read-authorize-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_AUTHORIZE_STATE, syscall0_compact_fields_49, (u32)(sizeof(syscall0_compact_fields_49) / sizeof(syscall0_compact_fields_49[0])));
    write_labeled_hex_u32(" denied-driver-read-dispatch ", denied_driver_read_dispatch);
    write_labeled_hex_u32(" driver-read-dispatch ", driver_read_dispatch);
    static const u16 syscall0_compact_fields_50[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x495Cu, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x0972u, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x0989u,
        0x099Au, 0x0903u, 0x0914u, 0x0923u, 0x0939u, 0x094Au, 0x0279u, 0x023Fu, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u,
        0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x08B8u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-read-dispatch-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DISPATCH_STATE, syscall0_compact_fields_50, (u32)(sizeof(syscall0_compact_fields_50) / sizeof(syscall0_compact_fields_50[0])));
    write_labeled_hex_u32(" denied-driver-read-queue ", denied_driver_read_queue);
    write_labeled_hex_u32(" driver-read-queue ", driver_read_queue);
    static const u16 syscall0_compact_fields_51[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x49A7u, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x09BCu, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x09D2u,
        0x099Au, 0x09E2u, 0x0903u, 0x0914u, 0x0923u, 0x0939u, 0x094Au, 0x0279u, 0x023Fu, 0x0251u, 0x00D3u, 0x00E1u,
        0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x08B8u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-read-queue-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_QUEUE_STATE, syscall0_compact_fields_51, (u32)(sizeof(syscall0_compact_fields_51) / sizeof(syscall0_compact_fields_51[0])));
    write_labeled_hex_u32(" denied-driver-read-worker ", denied_driver_read_worker);
    write_labeled_hex_u32(" driver-read-worker ", driver_read_worker);
    static const u16 syscall0_compact_fields_52[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x49EFu, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x0A01u, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x09D2u,
        0x099Au, 0x09E2u, 0x0A14u, 0x0903u, 0x0914u, 0x0923u, 0x0939u, 0x094Au, 0x0279u, 0x023Fu, 0x0251u, 0x00D3u,
        0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x08B8u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" driver-read-worker-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_WORKER_STATE, syscall0_compact_fields_52, (u32)(sizeof(syscall0_compact_fields_52) / sizeof(syscall0_compact_fields_52[0])));
    write_labeled_hex_u32(" denied-driver-read-schedule ", denied_driver_read_schedule);
    write_labeled_hex_u32(" driver-read-schedule ", driver_read_schedule);
    static const u16 syscall0_compact_fields_53[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4A25u, 0x4024u, 0x4029u, 0x0030u, 0x003Du, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x008Fu, 0x0A38u, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x09D2u,
        0x099Au, 0x09E2u, 0x0A14u, 0x0A4Cu, 0x0A5Du, 0x0903u, 0x0914u, 0x0923u, 0x0939u, 0x094Au, 0x0279u, 0x023Fu,
        0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x08B8u, 0x0119u, 0x0121u,
        0x012Au
    };
    write_syscall0_compact_fields(" driver-read-schedule-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_SCHEDULE_STATE, syscall0_compact_fields_53, (u32)(sizeof(syscall0_compact_fields_53) / sizeof(syscall0_compact_fields_53[0])));
    write_labeled_hex_u32(" denied-driver-read-run ", denied_driver_read_run);
    write_labeled_hex_u32(" driver-read-run ", driver_read_run);
    write_driver_read_run_fields(" driver-read-run-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_STATE);
    write_labeled_hex_u32(" denied-driver-read-body ", denied_driver_read_body);
    write_labeled_hex_u32(" driver-read-body ", driver_read_body);
    write_driver_read_body_fields(" driver-read-body-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_STATE);
#if 0
    write_labeled_hex_u32(" denied-driver-read-run ", denied_driver_read_run);
    write_labeled_hex_u32(" driver-read-run ", driver_read_run);
    static const char syscall0_suffixes_54[] =
        "state \0"
        "flags \0"
        "token \0"
        "read-schedule-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "query-only \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "read-ready \0"
        "read-schedule-denied \0"
        "requested \0"
        "granted \0"
        "denied \0"
        "policy-grant \0"
        "queue-inserted \0"
        "queue-depth \0"
        "worker-wake \0"
        "worker-dequeued \0"
        "worker-runnable \0"
        "worker-scheduled \0"
        "worker-run \0"
        "worker-executed \0"
        "issue-authority \0"
        "dma-authority \0"
        "media-read-authority \0"
        "write-authority \0"
        "commit-authority \0"
        "block-endpoint \0"
        "block-cap-minted \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer-unchanged \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_54[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_SCHEDULE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {42, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {47, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {54, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {67, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {79, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PORT, SCAFFOLD_TELEMETRY_HEX},
        {85, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_KIND, SCAFFOLD_TELEMETRY_DEC},
        {91, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {95, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {100, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {108, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {120, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {132, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {142, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {150, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {162, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_READ_SCHEDULE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {184, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {195, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {204, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {212, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_POLICY_GRANT, SCAFFOLD_TELEMETRY_DEC},
        {226, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_QUEUE_INSERTED, SCAFFOLD_TELEMETRY_DEC},
        {242, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_QUEUE_DEPTH, SCAFFOLD_TELEMETRY_DEC},
        {255, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_WAKE, SCAFFOLD_TELEMETRY_DEC},
        {268, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_DEQUEUED, SCAFFOLD_TELEMETRY_DEC},
        {285, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_RUNNABLE, SCAFFOLD_TELEMETRY_DEC},
        {302, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_SCHEDULED, SCAFFOLD_TELEMETRY_DEC},
        {320, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_RUN, SCAFFOLD_TELEMETRY_DEC},
        {332, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WORKER_EXECUTED, SCAFFOLD_TELEMETRY_DEC},
        {349, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_ISSUE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {366, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DMA_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {381, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MEDIA_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {403, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {420, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {438, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {454, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {472, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {483, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {497, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {514, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {525, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {541, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {546, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {553, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {565, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {580, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {598, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {606, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {615, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_RUN_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" driver-read-run-", syscall0_suffixes_54, syscall0_fields_54, (u32)(sizeof(syscall0_fields_54) / sizeof(syscall0_fields_54[0])));
    write_labeled_hex_u32(" denied-driver-read-body ", denied_driver_read_body);
    write_labeled_hex_u32(" driver-read-body ", driver_read_body);
    static const char syscall0_suffixes_55[] =
        "state \0"
        "flags \0"
        "token \0"
        "read-run-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "query-only \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "read-ready \0"
        "read-run-denied \0"
        "requested \0"
        "granted \0"
        "denied \0"
        "policy-grant \0"
        "queue-inserted \0"
        "queue-depth \0"
        "worker-wake \0"
        "worker-dequeued \0"
        "worker-runnable \0"
        "worker-scheduled \0"
        "worker-run \0"
        "worker-executed \0"
        "body-entered \0"
        "body-completed \0"
        "issue-authority \0"
        "dma-authority \0"
        "media-read-authority \0"
        "write-authority \0"
        "commit-authority \0"
        "block-endpoint \0"
        "block-cap-minted \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer-unchanged \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_55[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_RUN_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {37, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {42, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {49, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {62, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {74, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PORT, SCAFFOLD_TELEMETRY_HEX},
        {80, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_KIND, SCAFFOLD_TELEMETRY_DEC},
        {86, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {90, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {95, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {103, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {115, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {127, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {137, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {145, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {157, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_READ_RUN_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {174, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {185, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {194, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {202, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_POLICY_GRANT, SCAFFOLD_TELEMETRY_DEC},
        {216, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_QUEUE_INSERTED, SCAFFOLD_TELEMETRY_DEC},
        {232, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_QUEUE_DEPTH, SCAFFOLD_TELEMETRY_DEC},
        {245, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_WAKE, SCAFFOLD_TELEMETRY_DEC},
        {258, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_DEQUEUED, SCAFFOLD_TELEMETRY_DEC},
        {275, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_RUNNABLE, SCAFFOLD_TELEMETRY_DEC},
        {292, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_SCHEDULED, SCAFFOLD_TELEMETRY_DEC},
        {310, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_RUN, SCAFFOLD_TELEMETRY_DEC},
        {322, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WORKER_EXECUTED, SCAFFOLD_TELEMETRY_DEC},
        {339, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BODY_ENTERED, SCAFFOLD_TELEMETRY_DEC},
        {353, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BODY_COMPLETED, SCAFFOLD_TELEMETRY_DEC},
        {369, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_ISSUE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {386, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DMA_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {401, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MEDIA_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {423, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {440, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {458, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {474, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {492, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {503, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {517, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {534, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {545, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {561, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {566, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {573, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {585, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {600, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {618, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {626, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {635, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_BODY_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" driver-read-body-", syscall0_suffixes_55, syscall0_fields_55, (u32)(sizeof(syscall0_fields_55) / sizeof(syscall0_fields_55[0])));
#endif
    write_labeled_hex_u32(" denied-driver-read-issue ", denied_driver_read_issue);
    write_labeled_hex_u32(" driver-read-issue ", driver_read_issue);
    write_driver_read_issue_fields(" driver-read-issue-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_STATE);
    write_labeled_hex_u32(" denied-driver-read-dma ", denied_driver_read_dma);
    write_labeled_hex_u32(" driver-read-dma ", driver_read_dma);
    write_driver_read_dma_fields(" driver-read-dma-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_STATE);
#if 0
    write_labeled_hex_u32(" denied-driver-read-issue ", denied_driver_read_issue);
    write_labeled_hex_u32(" driver-read-issue ", driver_read_issue);
    static const char syscall0_suffixes_56[] =
        "state \0"
        "flags \0"
        "token \0"
        "read-body-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "query-only \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "read-ready \0"
        "read-body-denied \0"
        "requested \0"
        "granted \0"
        "denied \0"
        "policy-grant \0"
        "queue-inserted \0"
        "queue-depth \0"
        "worker-wake \0"
        "worker-dequeued \0"
        "worker-runnable \0"
        "worker-scheduled \0"
        "worker-run \0"
        "worker-executed \0"
        "issue-entered \0"
        "issue-completed \0"
        "issue-authority \0"
        "dma-authority \0"
        "media-read-authority \0"
        "write-authority \0"
        "commit-authority \0"
        "block-endpoint \0"
        "block-cap-minted \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer-unchanged \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_56[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BODY_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {43, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {50, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {63, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {75, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PORT, SCAFFOLD_TELEMETRY_HEX},
        {81, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_KIND, SCAFFOLD_TELEMETRY_DEC},
        {87, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {91, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {96, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {104, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {116, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {128, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {138, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {146, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {158, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_READ_BODY_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {176, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {187, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {196, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {204, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_POLICY_GRANT, SCAFFOLD_TELEMETRY_DEC},
        {218, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_QUEUE_INSERTED, SCAFFOLD_TELEMETRY_DEC},
        {234, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_QUEUE_DEPTH, SCAFFOLD_TELEMETRY_DEC},
        {247, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_WAKE, SCAFFOLD_TELEMETRY_DEC},
        {260, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_DEQUEUED, SCAFFOLD_TELEMETRY_DEC},
        {277, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_RUNNABLE, SCAFFOLD_TELEMETRY_DEC},
        {294, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_SCHEDULED, SCAFFOLD_TELEMETRY_DEC},
        {312, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_RUN, SCAFFOLD_TELEMETRY_DEC},
        {324, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WORKER_EXECUTED, SCAFFOLD_TELEMETRY_DEC},
        {341, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ISSUE_ENTERED, SCAFFOLD_TELEMETRY_DEC},
        {356, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ISSUE_COMPLETED, SCAFFOLD_TELEMETRY_DEC},
        {373, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ISSUE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {390, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DMA_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {405, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MEDIA_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {427, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {444, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {462, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {478, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {496, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {507, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {521, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {538, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {549, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {565, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {570, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {577, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {589, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {604, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {622, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {630, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {639, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_ISSUE_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" driver-read-issue-", syscall0_suffixes_56, syscall0_fields_56, (u32)(sizeof(syscall0_fields_56) / sizeof(syscall0_fields_56[0])));
    write_labeled_hex_u32(" denied-driver-read-dma ", denied_driver_read_dma);
    write_labeled_hex_u32(" driver-read-dma ", driver_read_dma);
    static const char syscall0_suffixes_57[] =
        "state \0"
        "flags \0"
        "token \0"
        "read-issue-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "query-only \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "read-ready \0"
        "read-issue-denied \0"
        "requested \0"
        "granted \0"
        "denied \0"
        "policy-grant \0"
        "bytes-available \0"
        "window-cap \0"
        "window-open \0"
        "entered \0"
        "completed \0"
        "issue-authority \0"
        "dma-authority \0"
        "media-read-authority \0"
        "write-authority \0"
        "commit-authority \0"
        "block-endpoint \0"
        "block-cap-minted \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer-unchanged \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_57[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_ISSUE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {39, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {44, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {51, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {64, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {76, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PORT, SCAFFOLD_TELEMETRY_HEX},
        {82, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_KIND, SCAFFOLD_TELEMETRY_DEC},
        {88, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {92, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {97, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {105, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {117, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {129, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {139, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {147, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {159, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_READ_ISSUE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {178, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {189, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {198, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {206, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_POLICY_GRANT, SCAFFOLD_TELEMETRY_DEC},
        {220, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BYTES_AVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {237, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_WINDOW_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {249, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_WINDOW_OPEN, SCAFFOLD_TELEMETRY_DEC},
        {262, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_ENTERED, SCAFFOLD_TELEMETRY_DEC},
        {271, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_COMPLETED, SCAFFOLD_TELEMETRY_DEC},
        {282, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_ISSUE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {299, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DMA_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {314, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MEDIA_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {336, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {353, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {371, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {387, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {405, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {416, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {430, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {447, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {458, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {474, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {479, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {486, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {498, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {513, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {531, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {539, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {548, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_DMA_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" driver-read-dma-", syscall0_suffixes_57, syscall0_fields_57, (u32)(sizeof(syscall0_fields_57) / sizeof(syscall0_fields_57[0])));
#endif
    write_labeled_hex_u32(" denied-drs-irq ", denied_driver_read_irq);
    write_labeled_hex_u32(" drs-irq ", driver_read_irq);
    static const u16 syscall0_compact_fields_58[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4A6Fu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0A7Au, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x00CCu,
        0x0A86u, 0x0A8Cu, 0x0A93u, 0x0A9Cu, 0x4AA4u, 0x0AAFu, 0x0ABBu, 0x04FEu, 0x0AC5u, 0x0AD1u, 0x0279u, 0x02C9u,
        0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u,
        0x012Au
    };
    write_syscall0_compact_fields(" drs-irq-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_IRQ_STATE, syscall0_compact_fields_58, (u32)(sizeof(syscall0_compact_fields_58) / sizeof(syscall0_compact_fields_58[0])));
    write_labeled_hex_u32(" denied-drs-status ", denied_driver_read_status);
    write_labeled_hex_u32(" drs-status ", driver_read_status);
    static const u16 syscall0_compact_fields_59[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4ADEu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0AE9u, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x00CCu,
        0x0AF5u, 0x0AFBu, 0x4B03u, 0x407Fu, 0x407Au, 0x4083u, 0x0B09u, 0x0A93u, 0x0A9Cu, 0x4AA4u, 0x0AAFu, 0x0ABBu,
        0x04FEu, 0x0AC5u, 0x0AD1u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u,
        0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drs-status-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STATE, syscall0_compact_fields_59, (u32)(sizeof(syscall0_compact_fields_59) / sizeof(syscall0_compact_fields_59[0])));
    write_labeled_hex_u32(" denied-drs-result ", denied_driver_read_status_result);
    write_labeled_hex_u32(" drs-result ", driver_read_status_result);
    static const u16 syscall0_compact_fields_60[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4B14u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0B22u, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x00CCu,
        0x0B31u, 0x0B3Au, 0x4B42u, 0x0AAFu, 0x0ABBu, 0x04FEu, 0x0AC5u, 0x0AD1u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u,
        0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drs-result-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESULT_STATE, syscall0_compact_fields_60, (u32)(sizeof(syscall0_compact_fields_60) / sizeof(syscall0_compact_fields_60[0])));
    write_labeled_hex_u32(" denied-drs-sample ", denied_driver_read_status_sample);
    write_labeled_hex_u32(" drs-sample ", driver_read_status_sample);
    static const u16 syscall0_compact_fields_61[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x425Cu, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x026Au, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x00CCu,
        0x4B03u, 0x407Fu, 0x407Au, 0x4083u, 0x0B4Du, 0x00A6u, 0x00AFu, 0x0B09u, 0x0B31u, 0x0B3Au, 0x4B42u, 0x0AAFu,
        0x0ABBu, 0x04FEu, 0x0AC5u, 0x0AD1u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du,
        0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drs-sample-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SAMPLE_STATE, syscall0_compact_fields_61, (u32)(sizeof(syscall0_compact_fields_61) / sizeof(syscall0_compact_fields_61[0])));
    write_labeled_hex_u32(" denied-drs-clear ", denied_driver_read_status_clear);
    write_labeled_hex_u32(" drs-clear ", driver_read_status_clear);
    static const u16 syscall0_compact_fields_62[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4B58u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0B66u, 0x0B74u, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u,
        0x00CCu, 0x4B82u, 0x4B8Au, 0x0B92u, 0x407Fu, 0x407Au, 0x4083u, 0x0B4Du, 0x00A6u, 0x00AFu, 0x0B9Du, 0x0BAEu,
        0x0BBDu, 0x4BCBu, 0x0B09u, 0x0B31u, 0x0B3Au, 0x4B42u, 0x0AAFu, 0x0ABBu, 0x04FEu, 0x0AC5u, 0x0AD1u, 0x0279u,
        0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u, 0x02F3u, 0x0119u,
        0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drs-clear-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_STATE, syscall0_compact_fields_62, (u32)(sizeof(syscall0_compact_fields_62) / sizeof(syscall0_compact_fields_62[0])));
    write_labeled_hex_u32(" denied-drs-clear-result ", denied_driver_read_status_clear_result);
    write_labeled_hex_u32(" drs-clear-result ", driver_read_status_clear_result);
    static const u16 syscall0_compact_fields_63[] = {
        0x0000u, 0x4007u, 0x400Eu, 0x4BD8u, 0x4024u, 0x4029u, 0x0030u, 0x02A3u, 0x4064u, 0x0089u, 0x00BBu, 0x00BFu,
        0x00C4u, 0x015Eu, 0x016Au, 0x417Eu, 0x0188u, 0x02AAu, 0x0BBDu, 0x021Bu, 0x01EBu, 0x0226u, 0x08F5u, 0x00CCu,
        0x4B82u, 0x4B8Au, 0x0B92u, 0x407Fu, 0x407Au, 0x4083u, 0x0B4Du, 0x00A6u, 0x00AFu, 0x0B9Du, 0x0BAEu, 0x0BE5u,
        0x4BCBu, 0x0B09u, 0x0BF8u, 0x0C0Au, 0x026Au, 0x0C1Au, 0x0C29u, 0x4C37u, 0x0AAFu, 0x0ABBu, 0x04FEu, 0x0AC5u,
        0x0AD1u, 0x0279u, 0x02C9u, 0x0251u, 0x00D3u, 0x00E1u, 0x00F2u, 0x00FDu, 0x010Du, 0x0112u, 0x0144u, 0x0289u,
        0x02F3u, 0x0119u, 0x0121u, 0x012Au
    };
    write_syscall0_compact_fields(" drs-clear-result-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLEAR_RESULT_STATE, syscall0_compact_fields_63, (u32)(sizeof(syscall0_compact_fields_63) / sizeof(syscall0_compact_fields_63[0])));
    write_labeled_hex_u32(" denied-drs-resample ", denied_driver_read_status_resample);
    write_labeled_hex_u32(" drs-resample ", driver_read_status_resample);
    write_drs_resample_fields(" drs-resample-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_STATE);
    write_labeled_hex_u32(" denied-drs-stable ", denied_driver_read_status_stable);
    write_labeled_hex_u32(" drs-stable ", driver_read_status_stable);
    write_drs_stable_fields(" drs-stable-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_STATE);
    write_labeled_hex_u32(" denied-drs-guard ", denied_driver_read_status_guard);
    write_labeled_hex_u32(" drs-guard ", driver_read_status_guard);
    write_drs_guard_fields(" drs-guard-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_STATE);
    write_labeled_hex_u32(" denied-drs-buffer ", denied_driver_read_status_buffer);
    write_labeled_hex_u32(" drs-buffer ", driver_read_status_buffer);
    write_drs_buffer_fields(" drs-buffer-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_STATE);
#if 0
    write_labeled_hex_u32(" denied-drs-resample ", denied_driver_read_status_resample);
    write_labeled_hex_u32(" drs-resample ", driver_read_status_resample);
    static const char syscall0_suffixes_64[] =
        "state \0"
        "flags \0"
        "token \0"
        "clear-result-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "qonly \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "ready \0"
        "clear-result-denied \0"
        "requested \0"
        "granted \0"
        "denied \0"
        "policy-grant \0"
        "bytes \0"
        "pxis-b \0"
        "pxis-a \0"
        "pxis-same \0"
        "ci \0"
        "tfd \0"
        "serr \0"
        "tfd-ready \0"
        "ci-idle \0"
        "serr-clear \0"
        "irq-clear \0"
        "result-status \0"
        "result-bytes \0"
        "result-checksum \0"
        "issue-auth \0"
        "dma-auth \0"
        "read-auth \0"
        "write-auth \0"
        "commit-auth \0"
        "block-endpoint \0"
        "block-cap \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_64[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_STATUS_CLEAR_RESULT_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {41, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {46, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {53, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {66, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {73, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT, SCAFFOLD_TELEMETRY_HEX},
        {79, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_KIND, SCAFFOLD_TELEMETRY_DEC},
        {85, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {89, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {94, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {102, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {114, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {126, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {136, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {144, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {151, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_CLEAR_RESULT_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {172, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {183, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {192, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {200, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_POLICY_GRANT, SCAFFOLD_TELEMETRY_DEC},
        {214, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BYTES_AVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {221, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_INTERRUPT_STATUS_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {229, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_INTERRUPT_STATUS_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {237, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_INTERRUPT_STATUS_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {248, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_COMMAND_ISSUE, SCAFFOLD_TELEMETRY_HEX},
        {252, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_TASK_FILE_STATUS, SCAFFOLD_TELEMETRY_HEX},
        {257, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_ERROR, SCAFFOLD_TELEMETRY_HEX},
        {263, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_TFD_READY, SCAFFOLD_TELEMETRY_DEC},
        {274, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_CI_IDLE, SCAFFOLD_TELEMETRY_DEC},
        {283, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_SERR_CLEAR, SCAFFOLD_TELEMETRY_DEC},
        {295, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_IRQ_CLEAR, SCAFFOLD_TELEMETRY_DEC},
        {306, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_RESULT_STATUS, SCAFFOLD_TELEMETRY_DEC},
        {321, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_RESULT_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {335, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_RESULT_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {352, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_ISSUE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {364, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DMA_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {374, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MEDIA_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {385, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {397, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {410, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {426, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {437, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {448, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {462, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {479, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {490, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {506, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {511, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {518, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {530, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {545, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {553, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {561, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {570, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RESAMPLE_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-resample-", syscall0_suffixes_64, syscall0_fields_64, (u32)(sizeof(syscall0_fields_64) / sizeof(syscall0_fields_64[0])));
    write_labeled_hex_u32(" denied-drs-stable ", denied_driver_read_status_stable);
    write_labeled_hex_u32(" drs-stable ", driver_read_status_stable);
    static const char syscall0_suffixes_65[] =
        "state \0"
        "flags \0"
        "token \0"
        "resample-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "qonly \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "ready \0"
        "clear-result-denied \0"
        "resample-read-only \0"
        "requested \0"
        "granted \0"
        "denied \0"
        "policy-grant \0"
        "bytes \0"
        "pxis-b \0"
        "pxis-a \0"
        "pxis-stable \0"
        "ci-b \0"
        "ci-a \0"
        "ci-stable \0"
        "tfd-b \0"
        "tfd-a \0"
        "tfd-stable \0"
        "serr-b \0"
        "serr-a \0"
        "serr-stable \0"
        "tfd-ready \0"
        "ci-idle \0"
        "serr-clear \0"
        "irq-clear \0"
        "result-status \0"
        "result-bytes \0"
        "result-checksum \0"
        "issue-auth \0"
        "dma-auth \0"
        "read-auth \0"
        "write-auth \0"
        "commit-auth \0"
        "block-endpoint \0"
        "block-cap \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_65[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_STATUS_RESAMPLE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {37, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {42, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {49, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {62, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {69, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT, SCAFFOLD_TELEMETRY_HEX},
        {75, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_KIND, SCAFFOLD_TELEMETRY_DEC},
        {81, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {85, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {90, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {98, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {110, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {122, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {132, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {140, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {147, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_CLEAR_RESULT_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {168, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESAMPLE_READ_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {188, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {199, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {208, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {216, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_POLICY_GRANT, SCAFFOLD_TELEMETRY_DEC},
        {230, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BYTES_AVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {237, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_INTERRUPT_STATUS_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {245, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_INTERRUPT_STATUS_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {253, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_INTERRUPT_STATUS_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {266, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUE_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {272, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUE_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {278, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUE_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {289, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TASK_FILE_STATUS_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {296, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TASK_FILE_STATUS_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {303, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TASK_FILE_STATUS_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {315, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_ERROR_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {323, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_ERROR_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {331, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_ERROR_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {344, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_TFD_READY, SCAFFOLD_TELEMETRY_DEC},
        {355, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_CI_IDLE, SCAFFOLD_TELEMETRY_DEC},
        {364, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_SERR_CLEAR, SCAFFOLD_TELEMETRY_DEC},
        {376, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_IRQ_CLEAR, SCAFFOLD_TELEMETRY_DEC},
        {387, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESULT_STATUS, SCAFFOLD_TELEMETRY_DEC},
        {402, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESULT_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {416, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_RESULT_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {433, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_ISSUE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {445, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DMA_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {455, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MEDIA_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {466, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {478, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {491, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {507, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {518, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {529, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {543, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {560, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {571, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {587, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {592, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {599, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {611, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {626, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {634, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {642, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {651, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_STABLE_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-stable-", syscall0_suffixes_65, syscall0_fields_65, (u32)(sizeof(syscall0_fields_65) / sizeof(syscall0_fields_65[0])));
    write_labeled_hex_u32(" denied-drs-guard ", denied_driver_read_status_guard);
    write_labeled_hex_u32(" drs-guard ", driver_read_status_guard);
    static const char syscall0_suffixes_66[] =
        "state \0"
        "flags \0"
        "token \0"
        "stable-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "qonly \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "ready \0"
        "pxis-b \0"
        "pxis-a \0"
        "pxis-stable \0"
        "ci-b \0"
        "ci-a \0"
        "ci-stable \0"
        "tfd-b \0"
        "tfd-a \0"
        "tfd-stable \0"
        "serr-b \0"
        "serr-a \0"
        "serr-stable \0"
        "tfd-ready \0"
        "ci-idle \0"
        "serr-clear \0"
        "requested \0"
        "issue-ok \0"
        "issue-denied \0"
        "dma-ok \0"
        "dma-denied \0"
        "read-auth \0"
        "read-denied \0"
        "write-auth \0"
        "write-denied \0"
        "commit-auth \0"
        "commit-denied \0"
        "irq-clear \0"
        "result-status \0"
        "result-bytes \0"
        "result-checksum \0"
        "block-endpoint \0"
        "block-cap \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_66[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_STATUS_STABLE_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {35, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {40, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {47, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {60, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {67, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT, SCAFFOLD_TELEMETRY_HEX},
        {73, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_KIND, SCAFFOLD_TELEMETRY_DEC},
        {79, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {83, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {88, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {96, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {108, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {120, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {130, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {138, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {145, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_INTERRUPT_STATUS_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {153, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_INTERRUPT_STATUS_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {161, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_INTERRUPT_STATUS_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {174, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUE_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {180, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUE_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {186, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUE_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {197, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TASK_FILE_STATUS_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {204, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TASK_FILE_STATUS_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {211, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TASK_FILE_STATUS_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {223, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_ERROR_BEFORE, SCAFFOLD_TELEMETRY_HEX},
        {231, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_ERROR_AFTER, SCAFFOLD_TELEMETRY_HEX},
        {239, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_ERROR_STABLE, SCAFFOLD_TELEMETRY_DEC},
        {252, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_TFD_READY, SCAFFOLD_TELEMETRY_DEC},
        {263, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_CI_IDLE, SCAFFOLD_TELEMETRY_DEC},
        {272, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_SERR_CLEAR, SCAFFOLD_TELEMETRY_DEC},
        {284, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_GUARD_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {295, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_ISSUE_ALLOWED, SCAFFOLD_TELEMETRY_DEC},
        {305, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_ISSUE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {319, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DMA_ALLOWED, SCAFFOLD_TELEMETRY_DEC},
        {327, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DMA_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {339, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {350, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_READ_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {363, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {375, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_WRITE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {389, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {402, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMIT_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {417, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_IRQ_CLEAR, SCAFFOLD_TELEMETRY_DEC},
        {428, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_RESULT_STATUS, SCAFFOLD_TELEMETRY_DEC},
        {443, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_RESULT_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {457, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_RESULT_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {474, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {490, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {501, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {512, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {526, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {543, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {554, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {570, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {575, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {582, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {594, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {609, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {617, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {625, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {634, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_GUARD_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-guard-", syscall0_suffixes_66, syscall0_fields_66, (u32)(sizeof(syscall0_fields_66) / sizeof(syscall0_fields_66[0])));
    write_labeled_hex_u32(" denied-drs-buffer ", denied_driver_read_status_buffer);
    write_labeled_hex_u32(" drs-buffer ", driver_read_status_buffer);
    static const char syscall0_suffixes_67[] =
        "state \0"
        "flags \0"
        "token \0"
        "guard-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "qonly \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "page-bytes \0"
        "checksum \0"
        "zeroed \0"
        "ready \0"
        "view-requested \0"
        "view-granted \0"
        "view-denied \0"
        "result-status \0"
        "result-bytes \0"
        "result-checksum \0"
        "read-auth \0"
        "write-auth \0"
        "commit-auth \0"
        "block-endpoint \0"
        "block-cap \0"
        "fs-minted \0"
        "mmio-written \0"
        "port-programmed \0"
        "published \0"
        "command-issued \0"
        "dma \0"
        "armed \0"
        "media-read \0"
        "media-written \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_67[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_STATUS_GUARD_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {34, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {39, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {46, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {59, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {66, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PORT, SCAFFOLD_TELEMETRY_HEX},
        {72, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_KIND, SCAFFOLD_TELEMETRY_DEC},
        {78, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {82, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {87, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {95, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {107, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PAGE_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {119, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {129, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BUFFER_ZEROED, SCAFFOLD_TELEMETRY_DEC},
        {137, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_READY, SCAFFOLD_TELEMETRY_DEC},
        {144, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_VIEW_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {160, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_VIEW_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {174, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_VIEW_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {187, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_STATUS, SCAFFOLD_TELEMETRY_DEC},
        {202, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {216, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_RESULT_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {233, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_READ_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {244, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {256, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {269, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BLOCK_ENDPOINT_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {285, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BLOCK_CAP_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {296, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_FS_MINTED, SCAFFOLD_TELEMETRY_DEC},
        {307, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_MMIO_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {321, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PORT_PROGRAMMED, SCAFFOLD_TELEMETRY_DEC},
        {338, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_PUBLISHED, SCAFFOLD_TELEMETRY_DEC},
        {349, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_COMMAND_ISSUED, SCAFFOLD_TELEMETRY_DEC},
        {365, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DMA_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {370, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_ARMED, SCAFFOLD_TELEMETRY_DEC},
        {377, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_MEDIA_READ, SCAFFOLD_TELEMETRY_DEC},
        {389, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_MEDIA_WRITTEN, SCAFFOLD_TELEMETRY_DEC},
        {404, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {412, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {420, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {429, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BUFFER_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-buffer-", syscall0_suffixes_67, syscall0_fields_67, (u32)(sizeof(syscall0_fields_67) / sizeof(syscall0_fields_67[0])));
#endif
    write_labeled_hex_u32(" denied-drs-export ", denied_driver_read_status_export);
    write_labeled_hex_u32(" drs-export ", driver_read_status_export);
    write_drs_export_fields(" drs-export-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_STATE);
    write_labeled_hex_u32(" denied-drs-report ", denied_driver_read_status_report);
    write_labeled_hex_u32(" drs-report ", driver_read_status_report);
    write_drs_compact_status_fields(
        " drs-report-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_STATE,
        "export-denied ",
        "report ");
    write_labeled_hex_u32(" denied-drs-receipt ", denied_driver_read_status_receipt);
    write_labeled_hex_u32(" drs-receipt ", driver_read_status_receipt);
    write_drs_compact_status_fields(
        " drs-receipt-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_STATE,
        "report-denied ",
        "receipt ");
    write_labeled_hex_u32(" denied-drs-ack ", denied_driver_read_status_ack);
    write_labeled_hex_u32(" drs-ack ", driver_read_status_ack);
    write_drs_compact_status_fields(
        " drs-ack-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_STATE,
        "receipt-denied ",
        "ack ");
    write_labeled_hex_u32(" denied-drs-close ", denied_driver_read_status_close);
    write_labeled_hex_u32(" drs-close ", driver_read_status_close);
    write_drs_compact_status_fields(
        " drs-close-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_STATE,
        "ack-denied ",
        "close ");
    write_labeled_hex_u32(" denied-drs-seal ", denied_driver_read_status_seal);
    write_labeled_hex_u32(" drs-seal ", driver_read_status_seal);
    write_drs_compact_status_fields(
        " drs-seal-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_STATE,
        "close-denied ",
        "seal ");
    write_labeled_hex_u32(" denied-drs-unseal ", denied_driver_read_status_unseal);
    write_labeled_hex_u32(" drs-unseal ", driver_read_status_unseal);
    write_drs_compact_status_fields(
        " drs-unseal-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_STATE,
        "seal-denied ",
        "unseal ");
    write_labeled_hex_u32(" denied-drs-discard ", denied_driver_read_status_discard);
    write_labeled_hex_u32(" drs-discard ", driver_read_status_discard);
    write_drs_compact_status_fields(
        " drs-discard-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_STATE,
        "unseal-denied ",
        "discard ");
    write_labeled_hex_u32(" denied-drs-final ", denied_driver_read_status_finalize);
    write_labeled_hex_u32(" drs-final ", driver_read_status_finalize);
    write_drs_compact_status_fields(
        " drs-final-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_STATE,
        "discard-denied ",
        "finish ");
    write_labeled_hex_u32(" denied-drs-authz ", denied_driver_read_status_authorize);
    write_labeled_hex_u32(" drs-authz ", driver_read_status_authorize);
    write_drs_compact_status_fields(
        " drs-authz-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_STATE,
        "final-denied ",
        "grant ");
#if 0
    write_labeled_hex_u32(" denied-drs-export ", denied_driver_read_status_export);
    write_labeled_hex_u32(" drs-export ", driver_read_status_export);
    static const char syscall0_suffixes_68[] =
        "state \0"
        "flags \0"
        "token \0"
        "buffer-token \0"
        "cap \0"
        "owner \0"
        "owner-bound \0"
        "qonly \0"
        "port \0"
        "kind \0"
        "op \0"
        "lba \0"
        "blocks \0"
        "read-bytes \0"
        "checksum \0"
        "sealed \0"
        "requested \0"
        "granted \0"
        "denied \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_68[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_STATUS_BUFFER_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {35, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_DRIVER_CAPABILITY, SCAFFOLD_TELEMETRY_HEX},
        {40, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {47, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_OWNER_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {60, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {67, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_PORT, SCAFFOLD_TELEMETRY_HEX},
        {73, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_KIND, SCAFFOLD_TELEMETRY_DEC},
        {79, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_OPERATION, SCAFFOLD_TELEMETRY_DEC},
        {83, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_LBA, SCAFFOLD_TELEMETRY_DEC},
        {88, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_BLOCKS, SCAFFOLD_TELEMETRY_DEC},
        {96, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_READ_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {108, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {118, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_STATUS_BUFFER_SEALED, SCAFFOLD_TELEMETRY_DEC},
        {126, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_EXPORT_REQUESTED, SCAFFOLD_TELEMETRY_DEC},
        {137, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_EXPORT_GRANTED, SCAFFOLD_TELEMETRY_DEC},
        {146, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_EXPORT_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {154, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {165, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {176, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {185, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {193, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {201, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {210, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXPORT_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-export-", syscall0_suffixes_68, syscall0_fields_68, (u32)(sizeof(syscall0_fields_68) / sizeof(syscall0_fields_68[0])));
    write_labeled_hex_u32(" denied-drs-report ", denied_driver_read_status_report);
    write_labeled_hex_u32(" drs-report ", driver_read_status_report);
    static const char syscall0_suffixes_69[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "export-denied \0"
        "report \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_69[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_STATUS_EXPORT_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {53, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_REPORT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {61, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {72, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {83, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {92, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {100, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {108, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {117, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_REPORT_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-report-", syscall0_suffixes_69, syscall0_fields_69, (u32)(sizeof(syscall0_fields_69) / sizeof(syscall0_fields_69[0])));
    write_labeled_hex_u32(" denied-drs-receipt ", denied_driver_read_status_receipt);
    write_labeled_hex_u32(" drs-receipt ", driver_read_status_receipt);
    static const char syscall0_suffixes_70[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "report-denied \0"
        "receipt \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_70[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_STATUS_REPORT_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {53, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_RECEIPT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {62, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {73, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {84, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {93, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {101, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {109, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {118, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_RECEIPT_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-receipt-", syscall0_suffixes_70, syscall0_fields_70, (u32)(sizeof(syscall0_fields_70) / sizeof(syscall0_fields_70[0])));
    write_labeled_hex_u32(" denied-drs-ack ", denied_driver_read_status_ack);
    write_labeled_hex_u32(" drs-ack ", driver_read_status_ack);
    static const char syscall0_suffixes_71[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "receipt-denied \0"
        "ack \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_71[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_STATUS_RECEIPT_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {54, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_ACK_MASK, SCAFFOLD_TELEMETRY_HEX},
        {59, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {70, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {81, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {90, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {98, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {106, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {115, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ACK_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-ack-", syscall0_suffixes_71, syscall0_fields_71, (u32)(sizeof(syscall0_fields_71) / sizeof(syscall0_fields_71[0])));
    write_labeled_hex_u32(" denied-drs-close ", denied_driver_read_status_close);
    write_labeled_hex_u32(" drs-close ", driver_read_status_close);
    static const char syscall0_suffixes_72[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "ack-denied \0"
        "close \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_72[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_STATUS_ACK_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {50, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_CLOSE_MASK, SCAFFOLD_TELEMETRY_HEX},
        {57, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {68, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {79, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {88, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {96, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {104, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {113, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_CLOSE_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-close-", syscall0_suffixes_72, syscall0_fields_72, (u32)(sizeof(syscall0_fields_72) / sizeof(syscall0_fields_72[0])));
    write_labeled_hex_u32(" denied-drs-seal ", denied_driver_read_status_seal);
    write_labeled_hex_u32(" drs-seal ", driver_read_status_seal);
    static const char syscall0_suffixes_73[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "close-denied \0"
        "seal \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_73[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_STATUS_CLOSE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {52, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_SEAL_MASK, SCAFFOLD_TELEMETRY_HEX},
        {58, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {69, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {80, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {89, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {97, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {105, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {114, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_SEAL_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-seal-", syscall0_suffixes_73, syscall0_fields_73, (u32)(sizeof(syscall0_fields_73) / sizeof(syscall0_fields_73[0])));
    write_labeled_hex_u32(" denied-drs-unseal ", denied_driver_read_status_unseal);
    write_labeled_hex_u32(" drs-unseal ", driver_read_status_unseal);
    static const char syscall0_suffixes_74[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "seal-denied \0"
        "unseal \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_74[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_STATUS_SEAL_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {51, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_UNSEAL_MASK, SCAFFOLD_TELEMETRY_HEX},
        {59, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {70, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {81, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {90, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {98, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {106, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {115, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_UNSEAL_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-unseal-", syscall0_suffixes_74, syscall0_fields_74, (u32)(sizeof(syscall0_fields_74) / sizeof(syscall0_fields_74[0])));
    write_labeled_hex_u32(" denied-drs-discard ", denied_driver_read_status_discard);
    write_labeled_hex_u32(" drs-discard ", driver_read_status_discard);
    static const char syscall0_suffixes_75[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "unseal-denied \0"
        "discard \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_75[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_STATUS_UNSEAL_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {53, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_DISCARD_MASK, SCAFFOLD_TELEMETRY_HEX},
        {62, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {73, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {84, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {93, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {101, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {109, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {118, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISCARD_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-discard-", syscall0_suffixes_75, syscall0_fields_75, (u32)(sizeof(syscall0_fields_75) / sizeof(syscall0_fields_75[0])));
    write_labeled_hex_u32(" denied-drs-final ", denied_driver_read_status_finalize);
    write_labeled_hex_u32(" drs-final ", driver_read_status_finalize);
    static const char syscall0_suffixes_76[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "discard-denied \0"
        "finish \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_76[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_STATUS_DISCARD_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {54, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_FINALIZE_MASK, SCAFFOLD_TELEMETRY_HEX},
        {62, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {73, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {84, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {93, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {101, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {109, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {118, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FINALIZE_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-final-", syscall0_suffixes_76, syscall0_fields_76, (u32)(sizeof(syscall0_fields_76) / sizeof(syscall0_fields_76[0])));
    write_labeled_hex_u32(" denied-drs-authz ", denied_driver_read_status_authorize);
    write_labeled_hex_u32(" drs-authz ", driver_read_status_authorize);
    static const char syscall0_suffixes_77[] =
        "state \0"
        "flags \0"
        "owner \0"
        "qonly \0"
        "checksum \0"
        "final-denied \0"
        "grant \0"
        "user-copy \0"
        "authority \0"
        "effects \0"
        "buffer \0"
        "staged \0"
        "denials \0"
        "unavailable \0";
    static const struct scaffold_syscall0_field syscall0_fields_77[] = {        {0, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_STATE, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {14, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_DRIVER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {21, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_QUERY_ONLY, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_BUFFER_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {38, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_STATUS_FINALIZE_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {52, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_AUTHORIZE_MASK, SCAFFOLD_TELEMETRY_HEX},
        {59, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_USER_COPY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {70, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_AUTHORITY_MASK, SCAFFOLD_TELEMETRY_HEX},
        {81, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_SIDE_EFFECT_MASK, SCAFFOLD_TELEMETRY_HEX},
        {90, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_BUFFER_UNCHANGED, SCAFFOLD_TELEMETRY_DEC},
        {98, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {106, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {115, X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_AUTHORIZE_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" drs-authz-", syscall0_suffixes_77, syscall0_fields_77, (u32)(sizeof(syscall0_fields_77) / sizeof(syscall0_fields_77[0])));
#endif
    write_labeled_hex_u32(" denied-drs-dispatch ", denied_driver_read_status_dispatch);
    write_labeled_hex_u32(" drs-dispatch ", driver_read_status_dispatch);
    write_drs_dispatch_fields(" drs-dispatch-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY);
    write_labeled_hex_u32(" denied-drs-queue ", denied_driver_read_status_queue);
    write_labeled_hex_u32(" drs-queue ", driver_read_status_queue);
    write_drs_queue_fields(" drs-queue-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY);
    write_labeled_hex_u32(" denied-drs-w ", denied_driver_read_status_worker);
    write_labeled_hex_u32(" drs-w ", driver_read_status_worker);
    write_drs_worker_fields(" drs-w-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY);
    write_labeled_hex_u32(" denied-drs-rauth ", denied_driver_read_status_read_authority);
    write_labeled_hex_u32(" drs-rauth ", driver_read_status_read_authority);
    write_drs_read_authority_fields(" drs-rauth-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY);
    write_labeled_hex_u32(" denied-drs-desc ", denied_driver_read_status_descriptor);
    write_labeled_hex_u32(" drs-desc ", driver_read_status_descriptor);
    write_drs_descriptor_fields(" drs-desc-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY);
    write_labeled_hex_u32(" denied-drs-ctab ", denied_driver_read_status_command_table);
    write_labeled_hex_u32(" drs-ctab ", driver_read_status_command_table);
    write_drs_command_table_fields(" drs-ctab-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY);
#if 0
    write_labeled_hex_u32(" denied-drs-dispatch ", denied_driver_read_status_dispatch);
    write_labeled_hex_u32(" drs-dispatch ", driver_read_status_dispatch);
    write_syscall1_dec_u32(" drs-dispatch-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-dispatch-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-dispatch-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-dispatch-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-dispatch-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-dispatch-authz-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_STATUS_AUTHORIZE_DENIED);
    write_syscall1_hex_u32(" drs-dispatch-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-dispatch-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-dispatch-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-dispatch-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-dispatch-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DISPATCH_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-queue ", denied_driver_read_status_queue);
    write_labeled_hex_u32(" drs-queue ", driver_read_status_queue);
    write_syscall1_dec_u32(" drs-queue-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-queue-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-queue-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-queue-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-queue-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-queue-dispatch-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_STATUS_DISPATCH_DENIED);
    write_syscall1_hex_u32(" drs-queue-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-queue-depth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_QUEUE_DEPTH);
    write_syscall1_dec_u32(" drs-queue-admit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_QUEUE_ADMIT);
    write_syscall1_dec_u32(" drs-queue-worker ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_WORKER_ADMIT);
    write_syscall1_dec_u32(" drs-queue-runnable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_WORKER_RUNNABLE);
    write_syscall1_dec_u32(" drs-queue-schedule ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_WORKER_SCHEDULE);
    write_syscall1_dec_u32(" drs-queue-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-queue-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-queue-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-queue-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_QUEUE_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-w ", denied_driver_read_status_worker);
    write_labeled_hex_u32(" drs-w ", driver_read_status_worker);
    write_syscall1_dec_u32(" drs-w-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-w-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-w-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-w-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-w-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-w-queue-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_STATUS_QUEUE_DENIED);
    write_syscall1_hex_u32(" drs-w-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-w-dequeue ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_QUEUE_DEQUEUE);
    write_syscall1_dec_u32(" drs-w-admit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_WORKER_ADMIT);
    write_syscall1_dec_u32(" drs-w-wake ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_WORKER_WAKE);
    write_syscall1_dec_u32(" drs-w-runnable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_WORKER_RUNNABLE);
    write_syscall1_dec_u32(" drs-w-sched ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_WORKER_SCHEDULE);
    write_syscall1_dec_u32(" drs-w-run ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_WORKER_RUN);
    write_syscall1_dec_u32(" drs-w-exec ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_WORKER_EXECUTE);
    write_syscall1_dec_u32(" drs-w-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-w-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-w-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-w-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_WORKER_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-rauth ", denied_driver_read_status_read_authority);
    write_labeled_hex_u32(" drs-rauth ", driver_read_status_read_authority);
    write_syscall1_dec_u32(" drs-rauth-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-rauth-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-rauth-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-rauth-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-rauth-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-rauth-worker-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_STATUS_WORKER_DENIED);
    write_syscall1_dec_u32(" drs-rauth-policy ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_POLICY_GRANTED);
    write_syscall1_dec_u32(" drs-rauth-read ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-rauth-issue ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-rauth-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-rauth-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-rauth-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-rauth-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-rauth-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-rauth-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-rauth-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-rauth-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-rauth-dequeue ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_QUEUE_DEQUEUE);
    write_syscall1_dec_u32(" drs-rauth-admit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_WORKER_ADMIT);
    write_syscall1_dec_u32(" drs-rauth-wake ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_WORKER_WAKE);
    write_syscall1_dec_u32(" drs-rauth-runnable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_WORKER_RUNNABLE);
    write_syscall1_dec_u32(" drs-rauth-sched ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_WORKER_SCHEDULE);
    write_syscall1_dec_u32(" drs-rauth-run ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_WORKER_RUN);
    write_syscall1_dec_u32(" drs-rauth-exec ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_WORKER_EXECUTE);
    write_syscall1_dec_u32(" drs-rauth-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-rauth-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-rauth-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-rauth-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_READ_AUTHORITY_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-desc ", denied_driver_read_status_descriptor);
    write_labeled_hex_u32(" drs-desc ", driver_read_status_descriptor);
    write_syscall1_dec_u32(" drs-desc-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-desc-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-desc-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-desc-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-desc-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-desc-rauth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_READ_AUTHORITY_BOUND);
    write_syscall1_dec_u32(" drs-desc-shaped ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_DESCRIPTOR_SHAPED);
    write_syscall1_dec_u32(" drs-desc-read ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_READ_DESCRIPTOR);
    write_syscall1_hex_u32(" drs-desc-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-desc-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-desc-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-desc-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-desc-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-desc-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-desc-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_PAGE_BYTES);
    write_syscall1_dec_u32(" drs-desc-slot ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_SLOT);
    write_syscall1_dec_u32(" drs-desc-header ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_COMMAND_HEADER_BYTES);
    write_syscall1_dec_u32(" drs-desc-table ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_COMMAND_TABLE_BYTES);
    write_syscall1_dec_u32(" drs-desc-cfis ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_CFIS_BYTES);
    write_syscall1_dec_u32(" drs-desc-prdt ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_PRDT_ENTRIES);
    write_syscall1_dec_u32(" drs-desc-prdt-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_PRDT_BYTES);
    write_syscall1_dec_u32(" drs-desc-packet ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_ATAPI_PACKET_BYTES);
    write_syscall1_hex_u32(" drs-desc-opcode ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_COMMAND_OPCODE);
    write_syscall1_hex_u32(" drs-desc-packet-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_PACKET_OPCODE);
    write_syscall1_dec_u32(" drs-desc-transfer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_TRANSFER_BYTES);
    write_syscall1_dec_u32(" drs-desc-issue ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-desc-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-desc-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-desc-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-desc-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-desc-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-desc-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-desc-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-desc-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-desc-dequeue ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_QUEUE_DEQUEUE);
    write_syscall1_dec_u32(" drs-desc-admit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_WORKER_ADMIT);
    write_syscall1_dec_u32(" drs-desc-wake ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_WORKER_WAKE);
    write_syscall1_dec_u32(" drs-desc-runnable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_WORKER_RUNNABLE);
    write_syscall1_dec_u32(" drs-desc-sched ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_WORKER_SCHEDULE);
    write_syscall1_dec_u32(" drs-desc-run ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_WORKER_RUN);
    write_syscall1_dec_u32(" drs-desc-exec ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_WORKER_EXECUTE);
    write_syscall1_dec_u32(" drs-desc-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-desc-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-desc-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-desc-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DESCRIPTOR_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-ctab ", denied_driver_read_status_command_table);
    write_labeled_hex_u32(" drs-ctab ", driver_read_status_command_table);
    write_syscall1_dec_u32(" drs-ctab-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-ctab-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-ctab-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-ctab-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-ctab-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-ctab-desc ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_DESCRIPTOR_BOUND);
    write_syscall1_dec_u32(" drs-ctab-mat ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_TABLE_MATERIALIZED);
    write_syscall1_dec_u32(" drs-ctab-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_TABLE_READY);
    write_syscall1_hex_u32(" drs-ctab-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-ctab-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-ctab-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-ctab-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-ctab-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-ctab-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-ctab-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-ctab-before ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_CHECKSUM_BEFORE);
    write_syscall1_hex_u32(" drs-ctab-after ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_CHECKSUM_AFTER);
    write_syscall1_dec_u32(" drs-ctab-changed ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_CHECKSUM_CHANGED);
    write_syscall1_hex_u32(" drs-ctab-hdr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_HEADER_FLAGS);
    write_syscall1_hex_u32(" drs-ctab-cfis ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_CFIS_COMMAND);
    write_syscall1_hex_u32(" drs-ctab-packet ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_PACKET_OPCODE);
    write_syscall1_dec_u32(" drs-ctab-dbc ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_PRDT_DBC);
    write_syscall1_dec_u32(" drs-ctab-written ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_TABLE_WRITTEN);
    write_syscall1_dec_u32(" drs-ctab-issue ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-ctab-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-ctab-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-ctab-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-ctab-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-ctab-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-ctab-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-ctab-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-ctab-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-ctab-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-ctab-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-ctab-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-ctab-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-ctab-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-ctab-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-ctab-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-ctab-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-ctab-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_TABLE_TELEMETRY_UNAVAILABLE_COUNT);
#endif
    write_labeled_hex_u32(" denied-drs-issue ", denied_driver_read_status_command_issue);
    write_labeled_hex_u32(" drs-issue ", driver_read_status_command_issue);
    write_drs_issue_ladder_fields(
        " drs-issue-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY,
        "ctab ",
        "ready ",
        "request ",
        "grant ",
        "denied ");
    write_labeled_hex_u32(" denied-drs-grant ", denied_driver_read_status_issue_grant);
    write_labeled_hex_u32(" drs-grant ", driver_read_status_issue_grant);
    write_drs_issue_ladder_fields(
        " drs-grant-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY,
        "issue ",
        "ready ",
        "request ",
        "grant ",
        "denied ");
    write_labeled_hex_u32(" denied-drs-arm ", denied_driver_read_status_arm);
    write_labeled_hex_u32(" drs-arm ", driver_read_status_arm);
    write_drs_issue_ladder_fields(
        " drs-arm-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY,
        "grant ",
        "ready ",
        "request ",
        "arm ",
        "denied ");
    write_labeled_hex_u32(" denied-drs-exec ", denied_driver_read_status_exec);
    write_labeled_hex_u32(" drs-exec ", driver_read_status_exec);
    write_drs_issue_ladder_fields(
        " drs-exec-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY,
        "arm ",
        "ready ",
        "request ",
        "exec ",
        "denied ");
    write_labeled_hex_u32(" denied-drs-dma ", denied_driver_read_status_dma);
    write_labeled_hex_u32(" drs-dma ", driver_read_status_dma);
    write_drs_issue_ladder_fields(
        " drs-dma-",
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY,
        "exec ",
        "ready ",
        "request ",
        "grant ",
        "denied ");
    write_labeled_hex_u32(" denied-drs-mmio ", denied_driver_read_status_mmio);
    write_labeled_hex_u32(" stale-drs-mmio ", stale_driver_read_status_mmio);
    write_labeled_hex_u32(" drs-mmio ", driver_read_status_mmio);
    write_drs_mmio_fields(" drs-mmio-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY);
    write_labeled_hex_u32(" denied-drs-dwin ", denied_driver_read_status_dma_window);
    write_labeled_hex_u32(" stale-drs-dwin ", stale_driver_read_status_dma_window);
    write_labeled_hex_u32(" drs-dwin ", driver_read_status_dma_window);
    write_drs_dma_window_fields(" drs-dwin-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY);
    write_labeled_hex_u32(" drs-read ", driver_read_status_read);
    write_drs_read_fields(" drs-read-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_READ_TELEMETRY);
    write_labeled_hex_u32(" denied-drs-block ", denied_driver_read_status_block);
    write_labeled_hex_u32(" stale-drs-block ", stale_driver_read_status_block);
    write_labeled_hex_u32(" drs-block ", driver_read_status_block);
    write_drs_block_fields(" drs-block-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_BLOCK_TELEMETRY);
    write_labeled_hex_u32(" drs-fs ", driver_read_status_fs);
    write_drs_fs_fields(" drs-fs-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_TELEMETRY);
    write_labeled_hex_u32(" drs-fs-user ", driver_read_status_fs_user);
    write_string(" drs-fs-user-path ");
    write_string(drs_fs_user_path);
    write_drs_fs_user_fields(" drs-fs-user-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_USER_TELEMETRY);
    write_labeled_hex_u32(" drs-fs-shell ", driver_read_status_fs_shell);
    write_drs_fs_shell_fields(" drs-fs-shell-", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_SHELL_TELEMETRY);
#if 0
    write_labeled_hex_u32(" denied-drs-issue ", denied_driver_read_status_command_issue);
    write_labeled_hex_u32(" drs-issue ", driver_read_status_command_issue);
    write_syscall1_dec_u32(" drs-issue-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-issue-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-issue-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-issue-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-issue-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-issue-ctab ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_COMMAND_TABLE_BOUND);
    write_syscall1_dec_u32(" drs-issue-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_ISSUE_READY);
    write_syscall1_dec_u32(" drs-issue-request ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_ISSUE_REQUESTED);
    write_syscall1_dec_u32(" drs-issue-grant ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_ISSUE_GRANTED);
    write_syscall1_dec_u32(" drs-issue-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_ISSUE_DENIED);
    write_syscall1_hex_u32(" drs-issue-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-issue-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-issue-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-issue-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-issue-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-issue-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-issue-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-issue-ci ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_CI);
    write_syscall1_hex_u32(" drs-issue-mask ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_SLOT_MASK);
    write_syscall1_dec_u32(" drs-issue-slot-idle ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_SLOT_IDLE);
    write_syscall1_dec_u32(" drs-issue-tfd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_TFD_READY);
    write_syscall1_dec_u32(" drs-issue-serr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_SERR_CLEAR);
    write_syscall1_hex_u32(" drs-issue-table-check ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_TABLE_CHECKSUM);
    write_syscall1_hex_u32(" drs-issue-expected ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_EXPECTED_CHECKSUM);
    write_syscall1_dec_u32(" drs-issue-match ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_CHECKSUM_MATCH);
    write_syscall1_dec_u32(" drs-issue-issue-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-issue-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-issue-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-issue-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-issue-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-issue-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-issue-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-issue-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-issue-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-issue-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-issue-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-issue-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-issue-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-issue-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-issue-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-issue-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-issue-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-issue-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_COMMAND_ISSUE_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-grant ", denied_driver_read_status_issue_grant);
    write_labeled_hex_u32(" drs-grant ", driver_read_status_issue_grant);
    write_syscall1_dec_u32(" drs-grant-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-grant-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-grant-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-grant-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-grant-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-grant-issue ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_COMMAND_ISSUE_BOUND);
    write_syscall1_dec_u32(" drs-grant-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_GRANT_READY);
    write_syscall1_dec_u32(" drs-grant-request ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_GRANT_REQUESTED);
    write_syscall1_dec_u32(" drs-grant-grant ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_GRANT_GRANTED);
    write_syscall1_dec_u32(" drs-grant-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_GRANT_DENIED);
    write_syscall1_hex_u32(" drs-grant-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-grant-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-grant-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-grant-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-grant-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-grant-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-grant-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-grant-ci ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_CI);
    write_syscall1_hex_u32(" drs-grant-mask ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_SLOT_MASK);
    write_syscall1_dec_u32(" drs-grant-slot-idle ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_SLOT_IDLE);
    write_syscall1_dec_u32(" drs-grant-tfd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_TFD_READY);
    write_syscall1_dec_u32(" drs-grant-serr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_SERR_CLEAR);
    write_syscall1_hex_u32(" drs-grant-table-check ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_TABLE_CHECKSUM);
    write_syscall1_hex_u32(" drs-grant-expected ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_EXPECTED_CHECKSUM);
    write_syscall1_dec_u32(" drs-grant-match ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_CHECKSUM_MATCH);
    write_syscall1_dec_u32(" drs-grant-issue-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-grant-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-grant-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-grant-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-grant-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-grant-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-grant-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-grant-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-grant-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-grant-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-grant-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-grant-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-grant-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-grant-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-grant-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-grant-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-grant-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-grant-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ISSUE_GRANT_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-arm ", denied_driver_read_status_arm);
    write_labeled_hex_u32(" drs-arm ", driver_read_status_arm);
    write_syscall1_dec_u32(" drs-arm-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-arm-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-arm-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-arm-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-arm-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-arm-grant ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_ISSUE_GRANT_BOUND);
    write_syscall1_dec_u32(" drs-arm-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_ARM_READY);
    write_syscall1_dec_u32(" drs-arm-request ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_ARM_REQUESTED);
    write_syscall1_dec_u32(" drs-arm-arm ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_ARM_GRANTED);
    write_syscall1_dec_u32(" drs-arm-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_ARM_DENIED);
    write_syscall1_hex_u32(" drs-arm-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-arm-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-arm-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-arm-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-arm-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-arm-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-arm-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-arm-ci ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_CI);
    write_syscall1_hex_u32(" drs-arm-mask ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_SLOT_MASK);
    write_syscall1_dec_u32(" drs-arm-slot-idle ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_SLOT_IDLE);
    write_syscall1_dec_u32(" drs-arm-tfd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_TFD_READY);
    write_syscall1_dec_u32(" drs-arm-serr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_SERR_CLEAR);
    write_syscall1_hex_u32(" drs-arm-table-check ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_TABLE_CHECKSUM);
    write_syscall1_hex_u32(" drs-arm-expected ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_EXPECTED_CHECKSUM);
    write_syscall1_dec_u32(" drs-arm-match ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_CHECKSUM_MATCH);
    write_syscall1_dec_u32(" drs-arm-issue-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-arm-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-arm-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-arm-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-arm-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-arm-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-arm-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-arm-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-arm-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-arm-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-arm-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-arm-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-arm-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-arm-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-arm-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-arm-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-arm-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-arm-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_ARM_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-exec ", denied_driver_read_status_exec);
    write_labeled_hex_u32(" drs-exec ", driver_read_status_exec);
    write_syscall1_dec_u32(" drs-exec-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-exec-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-exec-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-exec-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-exec-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-exec-arm ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_ARM_BOUND);
    write_syscall1_dec_u32(" drs-exec-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_EXEC_READY);
    write_syscall1_dec_u32(" drs-exec-request ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_EXEC_REQUESTED);
    write_syscall1_dec_u32(" drs-exec-exec ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_EXEC_GRANTED);
    write_syscall1_dec_u32(" drs-exec-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_EXEC_DENIED);
    write_syscall1_hex_u32(" drs-exec-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-exec-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-exec-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-exec-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-exec-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-exec-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-exec-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-exec-ci ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_CI);
    write_syscall1_hex_u32(" drs-exec-mask ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_SLOT_MASK);
    write_syscall1_dec_u32(" drs-exec-slot-idle ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_SLOT_IDLE);
    write_syscall1_dec_u32(" drs-exec-tfd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_TFD_READY);
    write_syscall1_dec_u32(" drs-exec-serr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_SERR_CLEAR);
    write_syscall1_hex_u32(" drs-exec-table-check ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_TABLE_CHECKSUM);
    write_syscall1_hex_u32(" drs-exec-expected ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_EXPECTED_CHECKSUM);
    write_syscall1_dec_u32(" drs-exec-match ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_CHECKSUM_MATCH);
    write_syscall1_dec_u32(" drs-exec-issue-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-exec-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-exec-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-exec-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-exec-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-exec-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-exec-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-exec-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-exec-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-exec-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-exec-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-exec-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-exec-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-exec-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-exec-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-exec-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-exec-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-exec-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_EXEC_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-dma ", denied_driver_read_status_dma);
    write_labeled_hex_u32(" drs-dma ", driver_read_status_dma);
    write_syscall1_dec_u32(" drs-dma-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-dma-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-dma-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-dma-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-dma-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-dma-exec ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_EXEC_BOUND);
    write_syscall1_dec_u32(" drs-dma-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DMA_READY);
    write_syscall1_dec_u32(" drs-dma-request ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DMA_REQUESTED);
    write_syscall1_dec_u32(" drs-dma-grant ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DMA_GRANTED);
    write_syscall1_dec_u32(" drs-dma-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DMA_DENIED);
    write_syscall1_hex_u32(" drs-dma-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-dma-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-dma-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-dma-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-dma-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-dma-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-dma-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-dma-ci ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_CI);
    write_syscall1_hex_u32(" drs-dma-mask ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_SLOT_MASK);
    write_syscall1_dec_u32(" drs-dma-slot-idle ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_SLOT_IDLE);
    write_syscall1_dec_u32(" drs-dma-tfd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_TFD_READY);
    write_syscall1_dec_u32(" drs-dma-serr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_SERR_CLEAR);
    write_syscall1_hex_u32(" drs-dma-table-check ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_TABLE_CHECKSUM);
    write_syscall1_hex_u32(" drs-dma-expected ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_EXPECTED_CHECKSUM);
    write_syscall1_dec_u32(" drs-dma-match ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_CHECKSUM_MATCH);
    write_syscall1_dec_u32(" drs-dma-issue-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-dma-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-dma-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-dma-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-dma-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-dma-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-dma-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-dma-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-dma-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-dma-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-dma-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-dma-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-dma-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-dma-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-dma-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-dma-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-dma-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-dma-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-mmio ", denied_driver_read_status_mmio);
    write_labeled_hex_u32(" stale-drs-mmio ", stale_driver_read_status_mmio);
    write_labeled_hex_u32(" drs-mmio ", driver_read_status_mmio);
    write_syscall1_dec_u32(" drs-mmio-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-mmio-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-mmio-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-mmio-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-mmio-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-mmio-dma-bound ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_DMA_BOUND);
    write_syscall1_dec_u32(" drs-mmio-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_MMIO_READY);
    write_syscall1_dec_u32(" drs-mmio-request ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_WRITE_REQUESTED);
    write_syscall1_dec_u32(" drs-mmio-grant ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_WRITE_GRANTED);
    write_syscall1_dec_u32(" drs-mmio-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_WRITE_DENIED);
    write_syscall1_hex_u32(" drs-mmio-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-mmio-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-mmio-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-mmio-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-mmio-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-mmio-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-mmio-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-mmio-ci ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_CI);
    write_syscall1_hex_u32(" drs-mmio-mask ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_SLOT_MASK);
    write_syscall1_dec_u32(" drs-mmio-slot-idle ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_SLOT_IDLE);
    write_syscall1_dec_u32(" drs-mmio-tfd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_TFD_READY);
    write_syscall1_dec_u32(" drs-mmio-serr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_SERR_CLEAR);
    write_syscall1_hex_u32(" drs-mmio-table-check ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_TABLE_CHECKSUM);
    write_syscall1_hex_u32(" drs-mmio-expected ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_EXPECTED_CHECKSUM);
    write_syscall1_dec_u32(" drs-mmio-match ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_CHECKSUM_MATCH);
    write_syscall1_hex_u32(" drs-mmio-reg ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_REGISTER_OFFSET);
    write_syscall1_hex_u32(" drs-mmio-value ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_WRITE_VALUE);
    write_syscall1_hex_u32(" drs-mmio-pxis-b ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_PXIS_BEFORE);
    write_syscall1_hex_u32(" drs-mmio-pxis-a ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_PXIS_AFTER);
    write_syscall1_dec_u32(" drs-mmio-pxis-same ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_PXIS_UNCHANGED);
    write_syscall1_dec_u32(" drs-mmio-rollback-required ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_ROLLBACK_REQUIRED);
    write_syscall1_dec_u32(" drs-mmio-rollback-done ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_ROLLBACK_DONE);
    write_syscall1_dec_u32(" drs-mmio-teardown ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_TEARDOWN);
    write_syscall1_dec_u32(" drs-mmio-stale-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_STALE_DENIED);
    write_syscall1_dec_u32(" drs-mmio-issue-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-mmio-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-mmio-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-mmio-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-mmio-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-mmio-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-mmio-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-mmio-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-mmio-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-mmio-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-mmio-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-mmio-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-mmio-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-mmio-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-mmio-media-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_MEDIA_WRITE);
    write_syscall1_dec_u32(" drs-mmio-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-mmio-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-mmio-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-mmio-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_MMIO_TELEMETRY_UNAVAILABLE_COUNT);
    write_labeled_hex_u32(" denied-drs-dwin ", denied_driver_read_status_dma_window);
    write_labeled_hex_u32(" stale-drs-dwin ", stale_driver_read_status_dma_window);
    write_labeled_hex_u32(" drs-dwin ", driver_read_status_dma_window);
    write_syscall1_dec_u32(" drs-dwin-state ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_STATE);
    write_syscall1_hex_u32(" drs-dwin-flags ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_FLAGS);
    write_syscall1_hex_u32(" drs-dwin-owner ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_DRIVER_OWNER);
    write_syscall1_dec_u32(" drs-dwin-qonly ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_QUERY_ONLY);
    write_syscall1_hex_u32(" drs-dwin-checksum ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BUFFER_CHECKSUM);
    write_syscall1_dec_u32(" drs-dwin-mmio-bound ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_MMIO_BOUND);
    write_syscall1_dec_u32(" drs-dwin-ready ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_WINDOW_READY);
    write_syscall1_dec_u32(" drs-dwin-request ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_WINDOW_REQUESTED);
    write_syscall1_dec_u32(" drs-dwin-grant ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_WINDOW_GRANTED);
    write_syscall1_dec_u32(" drs-dwin-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_WINDOW_DENIED);
    write_syscall1_hex_u32(" drs-dwin-port ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_PORT);
    write_syscall1_dec_u32(" drs-dwin-kind ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_KIND);
    write_syscall1_dec_u32(" drs-dwin-op ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_READ_OPERATION);
    write_syscall1_dec_u32(" drs-dwin-lba ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_READ_LBA);
    write_syscall1_dec_u32(" drs-dwin-blocks ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_READ_BLOCKS);
    write_syscall1_dec_u32(" drs-dwin-read-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_READ_BYTES);
    write_syscall1_dec_u32(" drs-dwin-page-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_PAGE_BYTES);
    write_syscall1_hex_u32(" drs-dwin-ci ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_CI);
    write_syscall1_hex_u32(" drs-dwin-mask ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_SLOT_MASK);
    write_syscall1_dec_u32(" drs-dwin-slot-idle ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_SLOT_IDLE);
    write_syscall1_dec_u32(" drs-dwin-tfd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_TFD_READY);
    write_syscall1_dec_u32(" drs-dwin-serr ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_SERR_CLEAR);
    write_syscall1_hex_u32(" drs-dwin-table-check ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_TABLE_CHECKSUM);
    write_syscall1_hex_u32(" drs-dwin-expected ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_EXPECTED_CHECKSUM);
    write_syscall1_dec_u32(" drs-dwin-match ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_CHECKSUM_MATCH);
    write_syscall1_hex_u32(" drs-dwin-page-low ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_PAGE_LOW);
    write_syscall1_hex_u32(" drs-dwin-page-high ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_PAGE_HIGH);
    write_syscall1_hex_u32(" drs-dwin-bounce-low ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BOUNCE_LOW);
    write_syscall1_hex_u32(" drs-dwin-bounce-high ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BOUNCE_HIGH);
    write_syscall1_dec_u32(" drs-dwin-bounce-bytes ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BOUNCE_BYTES);
    write_syscall1_dec_u32(" drs-dwin-offset ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BOUNCE_OFFSET);
    write_syscall1_dec_u32(" drs-dwin-range-end ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_RANGE_END);
    write_syscall1_dec_u32(" drs-dwin-single-page ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_SINGLE_PAGE);
    write_syscall1_dec_u32(" drs-dwin-broker ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BROKER_OWNED);
    write_syscall1_dec_u32(" drs-dwin-bounds ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BOUNDS_ENFORCED);
    write_syscall1_dec_u32(" drs-dwin-confined ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_PHYSICAL_CONFINED);
    write_syscall1_dec_u32(" drs-dwin-below4g ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BELOW_4G);
    write_syscall1_dec_u32(" drs-dwin-iommu ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_IOMMU_CONFIGURED);
    write_syscall1_dec_u32(" drs-dwin-identity ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_IDENTITY_DMA_ASSUMED);
    write_syscall1_dec_u32(" drs-dwin-non-user ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_NON_USER_MAPPED);
    write_syscall1_dec_u32(" drs-dwin-alias-safe ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_ALIAS_SAFE);
    write_syscall1_dec_u32(" drs-dwin-opened ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_LIFETIME_OPENED);
    write_syscall1_dec_u32(" drs-dwin-closed ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_LIFETIME_CLOSED);
    write_syscall1_dec_u32(" drs-dwin-active ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_ACTIVE);
    write_syscall1_dec_u32(" drs-dwin-revoke-required ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_REVOKE_REQUIRED);
    write_syscall1_dec_u32(" drs-dwin-revoke-done ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_REVOKE_DONE);
    write_syscall1_dec_u32(" drs-dwin-stale-denied ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_STALE_DENIED);
    write_syscall1_dec_u32(" drs-dwin-issue-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_ISSUE_AUTHORITY);
    write_syscall1_dec_u32(" drs-dwin-dma-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_DMA_AUTHORITY);
    write_syscall1_dec_u32(" drs-dwin-media-auth ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_MEDIA_READ_AUTHORITY);
    write_syscall1_dec_u32(" drs-dwin-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_WRITE_AUTHORITY);
    write_syscall1_dec_u32(" drs-dwin-commit ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_COMMIT_AUTHORITY);
    write_syscall1_dec_u32(" drs-dwin-block-endpoint ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BLOCK_ENDPOINT);
    write_syscall1_dec_u32(" drs-dwin-block-cap ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BLOCK_CAP_MINT);
    write_syscall1_dec_u32(" drs-dwin-fs-minted ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_FS_MINT);
    write_syscall1_hex_u32(" drs-dwin-safety ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_SAFETY_MASK);
    write_syscall1_dec_u32(" drs-dwin-mmio ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_MMIO_WRITE);
    write_syscall1_dec_u32(" drs-dwin-portw ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_PORT_PROGRAM);
    write_syscall1_dec_u32(" drs-dwin-cmd ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_COMMAND_ISSUE);
    write_syscall1_dec_u32(" drs-dwin-dma ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_DMA);
    write_syscall1_dec_u32(" drs-dwin-media ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_MEDIA_READ);
    write_syscall1_dec_u32(" drs-dwin-media-write ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_MEDIA_WRITE);
    write_syscall1_dec_u32(" drs-dwin-buffer ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_BUFFER_UNCHANGED);
    write_syscall1_dec_u32(" drs-dwin-staged ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_STAGE_COUNT);
    write_syscall1_dec_u32(" drs-dwin-denials ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_DENIAL_COUNT);
    write_syscall1_dec_u32(" drs-dwin-unavailable ", X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY, MMIO64_AHCI_DRIVER_READ_STATUS_DMA_WINDOW_TELEMETRY_UNAVAILABLE_COUNT);
#endif
    static const char syscall0_suffixes_78[] =
        "map-requests \0"
        "map-denials \0"
        "map-unavailable \0"
        "queries \0"
        "denials \0";
    static const struct scaffold_syscall0_field syscall0_fields_78[] = {        {0, X64_SYSCALL_MMIO_MAP_REQUEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {14, X64_SYSCALL_MMIO_MAP_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {27, X64_SYSCALL_MMIO_MAP_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {44, X64_SYSCALL_MMIO_QUERY_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {53, X64_SYSCALL_MMIO_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" ", syscall0_suffixes_78, syscall0_fields_78, (u32)(sizeof(syscall0_fields_78) / sizeof(syscall0_fields_78[0])));
    write_line("");
}

#endif /* LIMITLESS_SCAFFOLD_STORAGE_PCI_AHCI_DRS */

#if defined(LIMITLESS_SCAFFOLD_STORAGE_NVME_FS)
static void log_nvme_rw_surface(void)
{
    write_labeled_dec_u32("[x64] drs-nvme-rw delegated ", mmio64_nvme_rw_delegated());
    write_labeled_hex_u32(" cap ", mmio64_nvme_rw_capability());
    write_labeled_dec_u32(" wrong-owner ", mmio64_nvme_rw_wrong_owner());
    write_labeled_dec_u32(" stale ", mmio64_nvme_rw_stale_denied());
    write_labeled_dec_u32(" revoked ", mmio64_nvme_rw_revoked());
    write_labeled_dec_u32(" shell-write ", mmio64_nvme_rw_shell_write());
    write_labeled_dec_u32(" shell-readback ", mmio64_nvme_rw_shell_readback());
    write_labeled_dec_u32(" write-bytes ", mmio64_nvme_rw_write_bytes());
    write_string(" write-checksum ");
    write_hex_u32(mmio64_nvme_rw_write_checksum());
    write_labeled_dec_u32(" persisted ", mmio64_nvme_rw_persisted());
    write_labeled_dec_u32(" audit ", mmio64_nvme_rw_audit_count());
    write_labeled_dec_u32(" commits ", mmio64_nvme_rw_commit_count());
    write_labeled_dec_u32(" write-authority ", mmio64_nvme_rw_write_authority());
    write_labeled_dec_u32(" commit-authority ", mmio64_nvme_rw_commit_authority());
    write_labeled_dec_u32(" unavailable ", mmio64_nvme_rw_unavailable());
    write_labeled_dec_u32(" error ", mmio64_nvme_rw_error());
    write_line("");
}

static void log_filesystem_surface(void)
{
    static const char root_path[] = "/";
    static const char apps_path[] = "APPS";
    static const char readme_path[] = "README.TXT";
    static const char note_path[] = "NOTES.TXT";
    static const char note_text[] = "x64 capability filesystem online";
    u8 readme_buffer[96];
    u8 stat_buffer[96];
    u8 apps_buffer[160];
    u8 note_buffer[64];
    u32 owner = CAPABILITY64_OWNER_CONSOLE_CLIENT;
    u32 wrong_owner = CAPABILITY64_OWNER_POLICY_CLIENT;
    u64 ramfs_service = syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_RAMFS,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        owner);
    u64 denied_open = syscall64_invoke(
        X64_SYSCALL_FS_OPEN,
        0xDEADu,
        (u64)(const void *)readme_path,
        pack_count_owner((u32)(sizeof(readme_path) - 1u), owner));
    u64 root = syscall64_invoke(
        X64_SYSCALL_FS_OPEN,
        ramfs_service,
        (u64)(const void *)root_path,
        pack_count_owner((u32)(sizeof(root_path) - 1u), owner));
    u64 readme = syscall64_invoke(
        X64_SYSCALL_FS_OPEN,
        ramfs_service,
        (u64)(const void *)readme_path,
        pack_count_owner((u32)(sizeof(readme_path) - 1u), owner));
    u64 apps = syscall64_invoke(
        X64_SYSCALL_FS_OPEN,
        root,
        (u64)(const void *)apps_path,
        pack_count_owner((u32)(sizeof(apps_path) - 1u), owner));
    u64 readme_rights = syscall64_invoke(X64_SYSCALL_FS_NODE_RIGHTS, readme, 0u, owner);
    u64 readme_owner = syscall64_invoke(X64_SYSCALL_FS_NODE_OWNER, readme, 0u, owner);
    u64 wrong_owner_read;
    u64 readme_bytes;
    u64 stat_bytes;
    u64 apps_bytes;
    u64 note;
    u64 written;
    u64 note_bytes;
    u64 revoke_result;
    u64 revoked_read;

    zero_bytes(readme_buffer, sizeof(readme_buffer));
    zero_bytes(stat_buffer, sizeof(stat_buffer));
    zero_bytes(apps_buffer, sizeof(apps_buffer));
    zero_bytes(note_buffer, sizeof(note_buffer));

    wrong_owner_read = syscall64_invoke(
        X64_SYSCALL_FS_READ,
        readme,
        (u64)(void *)readme_buffer,
        pack_rw_owner((u32)(sizeof(readme_buffer) - 1u), 0u, wrong_owner));
    readme_bytes = syscall64_invoke(
        X64_SYSCALL_FS_READ,
        readme,
        (u64)(void *)readme_buffer,
        pack_rw_owner((u32)(sizeof(readme_buffer) - 1u), 0u, owner));
    stat_bytes = syscall64_invoke(
        X64_SYSCALL_FS_STAT,
        readme,
        (u64)(void *)stat_buffer,
        pack_count_owner((u32)(sizeof(stat_buffer) - 1u), owner));
    apps_bytes = syscall64_invoke(
        X64_SYSCALL_FS_LIST,
        apps,
        (u64)(void *)apps_buffer,
        pack_count_owner((u32)(sizeof(apps_buffer) - 1u), owner));
    note = syscall64_invoke(
        X64_SYSCALL_FS_CREATE,
        root,
        (u64)(const void *)note_path,
        pack_create_owner((u32)(sizeof(note_path) - 1u), RAMFS_NODE_FILE, owner));
    written = syscall64_invoke(
        X64_SYSCALL_FS_WRITE,
        note,
        (u64)(const void *)note_text,
        pack_rw_owner(string_length(note_text), 0u, owner));
    note_bytes = syscall64_invoke(
        X64_SYSCALL_FS_READ,
        note,
        (u64)(void *)note_buffer,
        pack_rw_owner((u32)(sizeof(note_buffer) - 1u), 0u, owner));
    revoke_result = syscall64_invoke(X64_SYSCALL_FS_REVOKE, note, 0u, owner);
    revoked_read = syscall64_invoke(
        X64_SYSCALL_FS_READ,
        note,
        (u64)(void *)note_buffer,
        pack_rw_owner((u32)(sizeof(note_buffer) - 1u), 0u, owner));

    write_string("[x64] fs broker service ");
    write_hex_u32((u32)ramfs_service);
    write_string(" denied-open ");
    write_hex_u32((u32)denied_open);
    write_string(" root ");
    write_hex_u32((u32)root);
    write_string(" readme ");
    write_hex_u32((u32)readme);
    write_string(" apps ");
    write_hex_u32((u32)apps);
    write_string(" rights ");
    write_hex_u32((u32)readme_rights);
    write_string(" owner ");
    write_hex_u32((u32)readme_owner);
    write_string(" wrong-owner ");
    write_hex_u32((u32)wrong_owner_read);
    write_string(" note ");
    write_hex_u32((u32)note);
    write_string(" written ");
    write_dec_u32((u32)written);
    write_string(" revoke ");
    write_dec_u32((u32)revoke_result);
    write_string(" revoked-read ");
    write_hex_u32((u32)revoked_read);
    write_line("");

    write_string("[x64] fs readme bytes ");
    write_dec_u32((u32)readme_bytes);
    write_string(" stat-bytes ");
    write_dec_u32((u32)stat_bytes);
    write_string(" text ");
    write_buffer_preview(readme_buffer, (u32)readme_bytes, 80u);
    write_string(" stat ");
    write_buffer_preview(stat_buffer, (u32)stat_bytes, 80u);
    write_line("");

    write_string("[x64] fs internal apps bytes ");
    write_dec_u32((u32)apps_bytes);
    write_string(" internal-listing ");
    write_buffer_preview(apps_buffer, (u32)apps_bytes, 150u);
    write_line("");

    write_string("[x64] fs note bytes ");
    write_dec_u32((u32)note_bytes);
    write_string(" text ");
    write_buffer_preview(note_buffer, (u32)note_bytes, 60u);
    static const char syscall0_suffixes_79[] =
        "live \0"
        "opens \0"
        "creates \0"
        "lists \0"
        "reads \0"
        "writes \0"
        "stats \0"
        "revokes \0"
        "denials \0"
        "stale-denials \0";
    static const struct scaffold_syscall0_field syscall0_fields_79[] = {        {0, X64_SYSCALL_FS_LIVE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {6, X64_SYSCALL_FS_OPEN_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {13, X64_SYSCALL_FS_CREATE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {22, X64_SYSCALL_FS_LIST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {29, X64_SYSCALL_FS_READ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {36, X64_SYSCALL_FS_WRITE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {44, X64_SYSCALL_FS_STAT_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {51, X64_SYSCALL_FS_REVOKE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {60, X64_SYSCALL_FS_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {69, X64_SYSCALL_FS_STALE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" ", syscall0_suffixes_79, syscall0_fields_79, (u32)(sizeof(syscall0_fields_79) / sizeof(syscall0_fields_79[0])));
    write_line("");
}

#endif /* LIMITLESS_SCAFFOLD_STORAGE_NVME_FS */

#if !defined(LIMITLESS_SCAFFOLD_UNITY)
void limitless_scaffold_storage_anchor(void) {}
#endif
