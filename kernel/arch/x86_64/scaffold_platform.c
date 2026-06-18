/* Split scaffold fragment. Real code is unity-included by scaffold.c; direct compilation emits only the anchor below. */

#if defined(LIMITLESS_SCAFFOLD_PLATFORM_SERVICES_INPUT_GUI)
static void run_app_model_m20_probe(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    static const u8 app_name[] = {'N', 'E', 'T', 'H', 'E', 'L', 'L', 'O'};
    u32 result = 0u;
    u32 aux = 0u;
    u32 fs_shell_token = (u32)syscall64_invoke(
        X64_SYSCALL_MMIO_AHCI_DRIVER_READ_STATUS_FS_SHELL_TELEMETRY,
        MMIO64_AHCI_DRIVER_READ_STATUS_FS_SHELL_TELEMETRY_TOKEN,
        0u,
        0u);
    u32 token = mmio64_stage_app_model_native_app(
        fs_shell_token,
        PRINCIPAL64_ID_BLOCK_WORKER,
        app_name,
        (u32)sizeof(app_name));

    if ((app_model64_nethello_mapped() != 0u)
        && (app_model64_begin_nethello_user() != 0u))
    {
        result = interrupts64_trigger_user_entry_probe(
            (u64)app_model64_nethello_entry_rip(),
            (u64)app_model64_nethello_entry_rsp(),
            (u64)app_model64_nethello_entry_selectors(),
            (u64)app_model64_nethello_entry_rflags());
        aux = interrupts64_user_entry_probe_aux();
        app_model64_end_nethello_user();
        token = app_model64_record_native_launch(result, aux);
    }

    write_labeled_hex_u32("[x64] drs-app-m21 ", token);
    write_labeled_dec_u32(" drs-app-m21-state ", app_model64_nethello_state());
    write_labeled_hex_u32(" drs-app-m21-flags ", app_model64_nethello_flags());
    write_labeled_hex_u32(" drs-app-m21-owner ", app_model64_nethello_owner());
    write_labeled_hex_u32(" drs-app-m21-name-token ", app_model64_native_name_token());
    write_labeled_dec_u32(" drs-app-m21-executable-id ", app_model64_native_executable_id());
    write_labeled_hex_u32(" drs-app-m21-authority-mask ", app_model64_native_authority_mask());
    write_labeled_hex_u32(" drs-app-m21-capability-mask ", app_model64_native_capability_mask());
    write_labeled_dec_u32(" drs-app-m21-payload-slot ", app_model64_native_payload_slot());
    write_labeled_hex_u32(" drs-app-m21-entry-result ", app_model64_native_entry_result());
    write_labeled_hex_u32(" drs-app-m21-success-result ", app_model64_native_success_result());
    write_labeled_dec_u32(" drs-app-m21-binary-path-verified ", app_model64_native_binary_path_verified());
    write_labeled_dec_u32(" drs-app-m21-descriptor-read ", app_model64_nethello_descriptor_read());
    write_labeled_dec_u32(" drs-app-m21-descriptor-parsed ", app_model64_nethello_descriptor_parsed());
    write_labeled_dec_u32(" drs-app-m21-descriptor-bytes ", app_model64_nethello_descriptor_bytes());
    write_labeled_dec_u32(" drs-app-m21-binary-read ", app_model64_nethello_binary_read());
    write_labeled_dec_u32(" drs-app-m21-checksum-verified ", app_model64_nethello_checksum_verified());
    write_labeled_dec_u32(" drs-app-m21-binary-bytes ", app_model64_nethello_binary_bytes());
    write_labeled_hex_u32(" drs-app-m21-checksum ", app_model64_nethello_checksum());
    write_labeled_hex_u32(" drs-app-m21-expected-checksum ", app_model64_nethello_expected_checksum());
    write_labeled_dec_u32(" drs-app-m21-mapped ", app_model64_nethello_mapped());
    write_labeled_dec_u32(" drs-app-m21-mapped-bytes ", app_model64_nethello_mapped_bytes());
    write_labeled_hex_u32(" drs-app-m21-entry-rip ", app_model64_nethello_entry_rip());
    write_labeled_hex_u32(" drs-app-m21-entry-rsp ", app_model64_nethello_entry_rsp());
    write_labeled_hex_u32(" drs-app-m21-entry-selectors ", app_model64_nethello_entry_selectors());
    write_labeled_hex_u32(" drs-app-m21-entry-rflags ", app_model64_nethello_entry_rflags());
    write_labeled_dec_u32(" drs-app-m21-launched ", app_model64_nethello_launched());
    write_labeled_dec_u32(" drs-app-m21-hello ", app_model64_nethello_hello_completed());
    write_labeled_dec_u32(" drs-app-m21-syscall-bridge ", app_model64_nethello_syscall_bridge());
    write_labeled_dec_u32(" drs-app-m21-network-cap-requested ", app_model64_nethello_network_cap_requested());
    write_labeled_dec_u32(" drs-app-m21-network-cap-granted ", app_model64_nethello_network_cap_granted());
    write_labeled_dec_u32(" drs-app-m21-socket-open ", app_model64_nethello_socket_opened());
    write_labeled_dec_u32(" drs-app-m21-recv-status ", app_model64_nethello_recv_status());
    write_labeled_dec_u32(" drs-app-m21-send-denied ", app_model64_nethello_send_denied());
    write_labeled_dec_u32(" drs-app-m21-close ", app_model64_nethello_socket_closed());
    write_labeled_dec_u32(" drs-app-m21-fs-denied ", app_model64_nethello_fs_denied());
    write_labeled_dec_u32(" drs-app-m21-storage-denied ", app_model64_nethello_storage_denied());
    write_labeled_hex_u32(" drs-app-m21-exit-result ", app_model64_nethello_exit_result());
    write_labeled_dec_u32(" drs-app-m21-exit-aux ", app_model64_nethello_exit_aux());
    write_labeled_dec_u32(" fs-authority ", app_model64_nethello_fs_authority());
    write_labeled_dec_u32(" storage-authority ", app_model64_nethello_storage_authority());
    write_labeled_dec_u32(" ambient-authority ", app_model64_nethello_ambient_authority());
    write_line("");
#endif
}

static void log_syscall_surface(void)
{
    write_syscall0_dec_u32("[x64] syscall arch ", X64_SYSCALL_GET_ARCH_BITS);
    write_string(" boot flags ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_GET_BOOT_FLAGS, 0u, 0u, 0u));
    write_line("");

    write_string("[x64] syscall page root ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_GET_PAGE_TABLE_ROOT, 0u, 0u, 0u));
    write_syscall0_dec_u32(" map ", X64_SYSCALL_GET_IDENTITY_MAP_MIB);
    write_line(" MiB");

    write_string("[x64] syscall kernel load ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_GET_KERNEL_LOAD_ADDRESS, 0u, 0u, 0u));
    write_syscall0_dec_u32(" sectors ", X64_SYSCALL_GET_KERNEL_SECTOR_COUNT);
    write_line("");

    write_syscall0_dec_u32("[x64] syscall memory conventional ", X64_SYSCALL_GET_CONVENTIONAL_MEMORY_KIB);
    write_syscall0_dec_u32(" KiB extended ", X64_SYSCALL_GET_EXTENDED_MEMORY_KIB);
    write_line(" KiB");

    static const char syscall0_suffixes_3[] =
        "[x64] syscall uptime \0"
        " irq \0"
        " probe \0"
        " calls \0";
    static const struct scaffold_syscall0_field syscall0_fields_3[] = {        {0, X64_SYSCALL_GET_UPTIME_TICKS, SCAFFOLD_TELEMETRY_DEC},
        {22, X64_SYSCALL_GET_IRQ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {28, X64_SYSCALL_GET_PROBE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {36, X64_SYSCALL_GET_SYSCALL_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields("", syscall0_suffixes_3, syscall0_fields_3, (u32)(sizeof(syscall0_fields_3) / sizeof(syscall0_fields_3[0])));
    write_line("");

    static const char syscall0_suffixes_4[] =
        "[x64] syscall descriptors state \0"
        " gdt \0"
        " tss-token \0"
        " user-cs \0"
        " user-ds \0"
        " star-ready \0";
    static const struct scaffold_syscall0_field syscall0_fields_4[] = {        {0, X64_SYSCALL_DESCRIPTOR_STATE, SCAFFOLD_TELEMETRY_HEX},
        {33, X64_SYSCALL_DESCRIPTOR_GDT_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {39, X64_SYSCALL_DESCRIPTOR_TSS_TOKEN, SCAFFOLD_TELEMETRY_HEX},
        {51, X64_SYSCALL_DESCRIPTOR_USER_CODE, SCAFFOLD_TELEMETRY_HEX},
        {61, X64_SYSCALL_DESCRIPTOR_USER_DATA, SCAFFOLD_TELEMETRY_HEX},
        {71, X64_SYSCALL_NATIVE_SYSCALL_STAR_READY, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields("", syscall0_suffixes_4, syscall0_fields_4, (u32)(sizeof(syscall0_fields_4) / sizeof(syscall0_fields_4[0])));
    write_line("");
}

static void log_fault_surface(void)
{
    static const char syscall0_suffixes_5[] =
        "[x64] syscall faults exceptions \0"
        " breakpoint \0"
        " invalid-op \0"
        " page-fault \0"
        " last vector \0";
    static const struct scaffold_syscall0_field syscall0_fields_5[] = {        {0, X64_SYSCALL_GET_EXCEPTION_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {33, X64_SYSCALL_GET_BREAKPOINT_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {46, X64_SYSCALL_GET_INVALID_OPCODE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {59, X64_SYSCALL_GET_PAGE_FAULT_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {72, X64_SYSCALL_GET_LAST_EXCEPTION_VECTOR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields("", syscall0_suffixes_5, syscall0_fields_5, (u32)(sizeof(syscall0_fields_5) / sizeof(syscall0_fields_5[0])));
    write_line("");

    write_string("[x64] syscall faults error ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_GET_LAST_EXCEPTION_ERROR, 0u, 0u, 0u));
    write_string(" rip ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_GET_LAST_EXCEPTION_RIP, 0u, 0u, 0u));
    write_string(" cr2 ");
    write_hex_u64(syscall64_invoke(X64_SYSCALL_GET_LAST_EXCEPTION_CR2, 0u, 0u, 0u));
    write_line("");
}

static void log_service_surface(void)
{
    u64 policy_endpoint = syscall64_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_AI_POLICY, 0u, 0u);
    u64 console_endpoint = syscall64_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_CONSOLE, 0u, 0u);
    u64 input_endpoint = syscall64_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_INPUT, 0u, 0u);
    u64 display_endpoint = syscall64_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_DISPLAY, 0u, 0u);
    u64 block_endpoint = syscall64_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_BLOCK, 0u, 0u);
    u64 hardware_endpoint = syscall64_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_HARDWARE, 0u, 0u);

    static const char syscall0_suffixes_6[] =
        "[x64] syscall services \0"
        " manifests \0"
        " signers \0"
        " payloads \0";
    static const struct scaffold_syscall0_field syscall0_fields_6[] = {        {0, X64_SYSCALL_GET_SERVICE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {24, X64_SYSCALL_GET_PACKAGE_MANIFEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {36, X64_SYSCALL_GET_PACKAGE_SIGNER_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {46, X64_SYSCALL_GET_PACKAGE_PAYLOAD_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields("", syscall0_suffixes_6, syscall0_fields_6, (u32)(sizeof(syscall0_fields_6) / sizeof(syscall0_fields_6[0])));
    write_line("");

    write_string("[x64] syscall service policy ");
    write_dec_u32((u32)policy_endpoint);
    write_syscall1_hex_u32(" caps ", X64_SYSCALL_GET_SERVICE_CAPABILITIES, policy_endpoint);
    write_syscall1_dec_u32(" delegable ", X64_SYSCALL_GET_SERVICE_DELEGABLE, policy_endpoint);
    write_string(" console ");
    write_dec_u32((u32)console_endpoint);
    write_string(" input ");
    write_dec_u32((u32)input_endpoint);
    write_string(" display ");
    write_dec_u32((u32)display_endpoint);
    write_syscall1_hex_u32(" display-caps ", X64_SYSCALL_GET_SERVICE_CAPABILITIES, display_endpoint);
    write_string(" block ");
    write_dec_u32((u32)block_endpoint);
    write_syscall1_hex_u32(" block-caps ", X64_SYSCALL_GET_SERVICE_CAPABILITIES, block_endpoint);
    write_string(" hardware ");
    write_dec_u32((u32)hardware_endpoint);
    write_syscall1_hex_u32(" hardware-caps ", X64_SYSCALL_GET_SERVICE_CAPABILITIES, hardware_endpoint);
    write_line("");
}

static void log_input_keyboard_surface(void)
{
    static const char syscall0_suffixes_7[] =
        "[x64] syscall input keyboard ps2-status \0"
        " irq \0"
        " polls \0"
        " scancodes \0"
        " bytes \0"
        " pending \0"
        " drops \0"
        " last-scancode \0"
        " last-byte \0";
    static const struct scaffold_syscall0_field syscall0_fields_7[] = {        {0, X64_SYSCALL_INPUT_PS2_STATUS_SNAPSHOT, SCAFFOLD_TELEMETRY_HEX},
        {41, X64_SYSCALL_INPUT_KEYBOARD_IRQ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {47, X64_SYSCALL_INPUT_KEYBOARD_POLL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {55, X64_SYSCALL_INPUT_KEYBOARD_SCANCODE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {67, X64_SYSCALL_INPUT_KEYBOARD_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {75, X64_SYSCALL_INPUT_KEYBOARD_PENDING_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {85, X64_SYSCALL_INPUT_KEYBOARD_DROP_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {93, X64_SYSCALL_INPUT_KEYBOARD_LAST_SCANCODE, SCAFFOLD_TELEMETRY_HEX},
        {109, X64_SYSCALL_INPUT_KEYBOARD_LAST_BYTE, SCAFFOLD_TELEMETRY_HEX}
    };
    write_syscall0_prefixed_label_fields("", syscall0_suffixes_7, syscall0_fields_7, (u32)(sizeof(syscall0_fields_7) / sizeof(syscall0_fields_7[0])));
    write_line("");
}

struct __attribute__((packed)) scaffold_value_field
{
    const char *label;
    u8 selector;
    u8 format;
};

static u64 pack_mac48(const u8 *mac);

enum scaffold_value_selector
{
    SCAFFOLD_VALUE_MOUSE_FOUND = 1u,
    SCAFFOLD_VALUE_MOUSE_DELTA,
    SCAFFOLD_VALUE_MOUSE_BUTTONS,
    SCAFFOLD_VALUE_MOUSE_PACKETS,
    SCAFFOLD_VALUE_MOUSE_PENDING,
    SCAFFOLD_VALUE_MOUSE_DROPS,
    SCAFFOLD_VALUE_MOUSE_X,
    SCAFFOLD_VALUE_MOUSE_Y,
    SCAFFOLD_VALUE_MOUSE_LAST_DX,
    SCAFFOLD_VALUE_MOUSE_LAST_DY,
    SCAFFOLD_VALUE_MOUSE_PS2_ENABLED,
    SCAFFOLD_VALUE_MOUSE_IRQ,
    SCAFFOLD_VALUE_MOUSE_POLLS,
    SCAFFOLD_VALUE_MOUSE_USB_DEVICE,
    SCAFFOLD_VALUE_MOUSE_USB_REPORTS,
    SCAFFOLD_VALUE_MOUSE_USB_REPORT_BYTES,
    SCAFFOLD_VALUE_COMPOSITOR_INIT,
    SCAFFOLD_VALUE_COMPOSITOR_PRESENT_BOOL,
    SCAFFOLD_VALUE_COMPOSITOR_CURSOR_BOOL,
    SCAFFOLD_VALUE_COMPOSITOR_PRESENTS,
    SCAFFOLD_VALUE_COMPOSITOR_CURSORS,
    SCAFFOLD_VALUE_FONT_INIT,
    SCAFFOLD_VALUE_FONT_GLYPHS,
    SCAFFOLD_VALUE_FONT_RENDER_BOOL,
    SCAFFOLD_VALUE_FONT_RENDERS,
    SCAFFOLD_VALUE_WM_INIT,
    SCAFFOLD_VALUE_WM_WINDOW_BOOL,
    SCAFFOLD_VALUE_WM_FOCUS_BOOL,
    SCAFFOLD_VALUE_WM_PRESENT_BOOL,
    SCAFFOLD_VALUE_WM_WINDOWS,
    SCAFFOLD_VALUE_WM_FOCUSES,
    SCAFFOLD_VALUE_WM_PRESENTS,
    SCAFFOLD_VALUE_APIC_MADT,
    SCAFFOLD_VALUE_APIC_LAPIC_BASE,
    SCAFFOLD_VALUE_APIC_IOAPIC_BASE,
    SCAFFOLD_VALUE_APIC_PIC_DISABLED,
    SCAFFOLD_VALUE_APIC_TIMER_TICKING,
    SCAFFOLD_VALUE_APIC_KEYBOARD_LIVE,
    SCAFFOLD_VALUE_APIC_ENABLED,
    SCAFFOLD_VALUE_APIC_LAPIC_ID,
    SCAFFOLD_VALUE_APIC_IOAPIC_GSI_BASE,
    SCAFFOLD_VALUE_APIC_IOAPIC_MAX_REDIR,
    SCAFFOLD_VALUE_APIC_IRQ0,
    SCAFFOLD_VALUE_APIC_IRQ1,
    SCAFFOLD_VALUE_APIC_IRQ11,
    SCAFFOLD_VALUE_APIC_IRQ12,
    SCAFFOLD_VALUE_APIC_OVERRIDE_SCANNED,
    SCAFFOLD_VALUE_APIC_OVERRIDE_COUNT,
    SCAFFOLD_VALUE_APIC_TIMER_GSI,
    SCAFFOLD_VALUE_APIC_TIMER_POLARITY,
    SCAFFOLD_VALUE_APIC_TIMER_TRIGGER,
    SCAFFOLD_VALUE_APIC_KEYBOARD_GSI,
    SCAFFOLD_VALUE_APIC_KEYBOARD_POLARITY,
    SCAFFOLD_VALUE_APIC_KEYBOARD_TRIGGER,
    SCAFFOLD_VALUE_APIC_AHCI_GSI,
    SCAFFOLD_VALUE_APIC_AHCI_POLARITY,
    SCAFFOLD_VALUE_APIC_AHCI_TRIGGER,
    SCAFFOLD_VALUE_APIC_MOUSE_GSI,
    SCAFFOLD_VALUE_APIC_MOUSE_POLARITY,
    SCAFFOLD_VALUE_APIC_MOUSE_TRIGGER,
    SCAFFOLD_VALUE_XHCI_FOUND,
    SCAFFOLD_VALUE_XHCI_BAR0,
    SCAFFOLD_VALUE_XHCI_MAPPED,
    SCAFFOLD_VALUE_XHCI_CAP,
    SCAFFOLD_VALUE_XHCI_PORTS,
    SCAFFOLD_VALUE_XHCI_PORTS_SCANNED,
    SCAFFOLD_VALUE_XHCI_CONNECTED,
    SCAFFOLD_VALUE_XHCI_COMMAND_RING,
    SCAFFOLD_VALUE_XHCI_DCBAA,
    SCAFFOLD_VALUE_XHCI_EVENT_RING,
    SCAFFOLD_VALUE_XHCI_RESET,
    SCAFFOLD_VALUE_XHCI_RUNNING,
    SCAFFOLD_VALUE_XHCI_SLOT_ENABLED,
    SCAFFOLD_VALUE_XHCI_ADDRESSED,
    SCAFFOLD_VALUE_XHCI_CONFIG_READ,
    SCAFFOLD_VALUE_XHCI_REPORT_DESC,
    SCAFFOLD_VALUE_XHCI_ENDPOINT,
    SCAFFOLD_VALUE_XHCI_HID_DEVICE,
    SCAFFOLD_VALUE_XHCI_INPUT_LIVE,
    SCAFFOLD_VALUE_XHCI_REPORTS,
    SCAFFOLD_VALUE_XHCI_REPORT_BYTES,
    SCAFFOLD_VALUE_XHCI_UNAVAILABLE,
    SCAFFOLD_VALUE_XHCI_ERROR,
    SCAFFOLD_VALUE_XHCI_EXTCAPS_SCANNED,
    SCAFFOLD_VALUE_XHCI_LEGACY_CAP,
    SCAFFOLD_VALUE_XHCI_LEGACY_HANDOFF,
    SCAFFOLD_VALUE_XHCI_BIOS_OWNED_BEFORE,
    SCAFFOLD_VALUE_XHCI_BIOS_OWNED_CLEAR,
    SCAFFOLD_VALUE_XHCI_OS_OWNED,
    SCAFFOLD_VALUE_XHCI_PROTOCOL_CAPS,
    SCAFFOLD_VALUE_XHCI_USB2_PORTS,
    SCAFFOLD_VALUE_XHCI_USB3_PORTS,
    SCAFFOLD_VALUE_XHCI_PREFER_USB2,
    SCAFFOLD_VALUE_XHCI_INTEL_CAP,
    SCAFFOLD_VALUE_XHCI_INTEL_WORKAROUND,
    SCAFFOLD_VALUE_XHCI_RESET_WAIT_MS,
    SCAFFOLD_VALUE_XHCI_SETTLE_MS,
    SCAFFOLD_VALUE_NET_FOUND,
    SCAFFOLD_VALUE_NET_BAR0,
    SCAFFOLD_VALUE_NET_MAPPED,
    SCAFFOLD_VALUE_NET_COMMON,
    SCAFFOLD_VALUE_NET_NOTIFY,
    SCAFFOLD_VALUE_NET_DEVICE_CONFIG,
    SCAFFOLD_VALUE_NET_MAC,
    SCAFFOLD_VALUE_NET_MAC_NONZERO,
    SCAFFOLD_VALUE_NET_STATUS_ACK,
    SCAFFOLD_VALUE_NET_STATUS_DRIVER,
    SCAFFOLD_VALUE_NET_FEATURES_OK,
    SCAFFOLD_VALUE_NET_DRIVER_OK,
    SCAFFOLD_VALUE_NET_RX_QUEUE,
    SCAFFOLD_VALUE_NET_TX_QUEUE,
    SCAFFOLD_VALUE_NET_RX_BUFFERS,
    SCAFFOLD_VALUE_NET_TX,
    SCAFFOLD_VALUE_NET_RX,
    SCAFFOLD_VALUE_NET_ARP_REPLY,
    SCAFFOLD_VALUE_NET_ARP_MAC,
    SCAFFOLD_VALUE_NET_ARP_IP,
    SCAFFOLD_VALUE_NET_FS_AUTHORITY,
    SCAFFOLD_VALUE_NET_STORAGE_AUTHORITY,
    SCAFFOLD_VALUE_NET_AMBIENT_AUTHORITY,
    SCAFFOLD_VALUE_NET_UNAVAILABLE,
    SCAFFOLD_VALUE_NET_ERROR,
    SCAFFOLD_VALUE_E1000_FOUND,
    SCAFFOLD_VALUE_E1000_BAR0,
    SCAFFOLD_VALUE_E1000_MAPPED,
    SCAFFOLD_VALUE_E1000_RESET,
    SCAFFOLD_VALUE_E1000_MAC,
    SCAFFOLD_VALUE_E1000_MAC_NONZERO,
    SCAFFOLD_VALUE_E1000_LINK_UP,
    SCAFFOLD_VALUE_E1000_RX_QUEUE,
    SCAFFOLD_VALUE_E1000_TX_QUEUE,
    SCAFFOLD_VALUE_E1000_RX_BUFFERS,
    SCAFFOLD_VALUE_E1000_TX,
    SCAFFOLD_VALUE_E1000_RX,
    SCAFFOLD_VALUE_E1000_DHCP,
    SCAFFOLD_VALUE_E1000_DNS,
    SCAFFOLD_VALUE_E1000_HTTP,
    SCAFFOLD_VALUE_E1000_FS_AUTHORITY,
    SCAFFOLD_VALUE_E1000_STORAGE_AUTHORITY,
    SCAFFOLD_VALUE_E1000_AMBIENT_AUTHORITY,
    SCAFFOLD_VALUE_E1000_UNAVAILABLE,
    SCAFFOLD_VALUE_E1000_ERROR,
    SCAFFOLD_VALUE_DHCP_DISCOVER,
    SCAFFOLD_VALUE_DHCP_OFFER,
    SCAFFOLD_VALUE_DHCP_REQUEST,
    SCAFFOLD_VALUE_DHCP_ACK,
    SCAFFOLD_VALUE_DHCP_IP,
    SCAFFOLD_VALUE_DHCP_GATEWAY,
    SCAFFOLD_VALUE_DHCP_DNS,
    SCAFFOLD_VALUE_DHCP_LEASE,
    SCAFFOLD_VALUE_DHCP_AMBIENT_AUTHORITY,
    SCAFFOLD_VALUE_DHCP_UNAVAILABLE,
    SCAFFOLD_VALUE_DHCP_ERROR,
    SCAFFOLD_VALUE_DNS_QUERY,
    SCAFFOLD_VALUE_DNS_RESPONSE,
    SCAFFOLD_VALUE_DNS_RCODE,
    SCAFFOLD_VALUE_DNS_RESOLVED,
    SCAFFOLD_VALUE_DNS_FS_AUTHORITY,
    SCAFFOLD_VALUE_DNS_STORAGE_AUTHORITY,
    SCAFFOLD_VALUE_DNS_AMBIENT_AUTHORITY,
    SCAFFOLD_VALUE_DNS_UNAVAILABLE,
    SCAFFOLD_VALUE_DNS_ERROR,
    SCAFFOLD_VALUE_HTTP_CONNECTED,
    SCAFFOLD_VALUE_HTTP_SENT,
    SCAFFOLD_VALUE_HTTP_STATUS,
    SCAFFOLD_VALUE_HTTP_RESPONSE_BYTES,
    SCAFFOLD_VALUE_HTTP_FS_AUTHORITY,
    SCAFFOLD_VALUE_HTTP_STORAGE_AUTHORITY,
    SCAFFOLD_VALUE_HTTP_AMBIENT_AUTHORITY,
    SCAFFOLD_VALUE_HTTP_UNAVAILABLE,
    SCAFFOLD_VALUE_HTTP_ERROR,
    SCAFFOLD_VALUE_NVME_READ_IOQ_CREATED,
    SCAFFOLD_VALUE_NVME_READ_ISSUED,
    SCAFFOLD_VALUE_NVME_READ_COMPLETED,
    SCAFFOLD_VALUE_NVME_READ_STATUS,
    SCAFFOLD_VALUE_NVME_READ_BYTES,
    SCAFFOLD_VALUE_NVME_READ_CHECKSUM,
    SCAFFOLD_VALUE_NVME_READ_FS_AUTHORITY,
    SCAFFOLD_VALUE_NVME_READ_BLOCK_ENDPOINT,
    SCAFFOLD_VALUE_NVME_READ_WRITE_AUTHORITY,
    SCAFFOLD_VALUE_NVME_READ_UNAVAILABLE,
    SCAFFOLD_VALUE_NVME_READ_ERROR,
    SCAFFOLD_VALUE_NVME_GPT_SIGNATURE,
    SCAFFOLD_VALUE_NVME_GPT_PARTITIONS,
    SCAFFOLD_VALUE_NVME_GPT_FAT32_START,
    SCAFFOLD_VALUE_NVME_GPT_FAT32_SECTORS,
    SCAFFOLD_VALUE_NVME_GPT_VBR,
    SCAFFOLD_VALUE_NVME_GPT_FS_AUTHORITY,
    SCAFFOLD_VALUE_NVME_GPT_WRITE_AUTHORITY,
    SCAFFOLD_VALUE_NVME_GPT_UNAVAILABLE,
    SCAFFOLD_VALUE_NVME_GPT_ERROR,
    SCAFFOLD_VALUE_NVME_FAT_BPB,
    SCAFFOLD_VALUE_NVME_FAT_LOCATED,
    SCAFFOLD_VALUE_NVME_FAT_READ_BYTES,
    SCAFFOLD_VALUE_NVME_FAT_CHECKSUM,
    SCAFFOLD_VALUE_NVME_FAT_CONTENT_MATCH,
    SCAFFOLD_VALUE_NVME_FAT_BYTES_PER_SECTOR,
    SCAFFOLD_VALUE_NVME_FAT_SECTORS_PER_CLUSTER,
    SCAFFOLD_VALUE_NVME_FAT_LFN,
    SCAFFOLD_VALUE_NVME_FAT_UNICODE_LFN,
    SCAFFOLD_VALUE_NVME_FAT_SUBDIR,
    SCAFFOLD_VALUE_NVME_FAT_MULTICLUSTER,
    SCAFFOLD_VALUE_NVME_FAT_MULTI_BYTES,
    SCAFFOLD_VALUE_NVME_FAT_WRITE_GATE,
    SCAFFOLD_VALUE_NVME_FAT_CREATE_CLUSTER,
    SCAFFOLD_VALUE_NVME_FAT_CREATE_READBACK,
    SCAFFOLD_VALUE_NVME_FAT_CREATE_BYTES,
    SCAFFOLD_VALUE_NVME_FAT_CREATE_CHECKSUM,
    SCAFFOLD_VALUE_NVME_FAT_UPDATE_CLUSTER,
    SCAFFOLD_VALUE_NVME_FAT_UPDATE_READBACK,
    SCAFFOLD_VALUE_NVME_FAT_UPDATE_BYTES,
    SCAFFOLD_VALUE_NVME_FAT_UPDATE_CHECKSUM,
    SCAFFOLD_VALUE_NVME_FAT_DELETE_FREED,
    SCAFFOLD_VALUE_NVME_FAT_DELETE_TOMBSTONE,
    SCAFFOLD_VALUE_NVME_FAT_FLUSHES,
    SCAFFOLD_VALUE_NVME_FAT_FS_DELEGATION,
    SCAFFOLD_VALUE_NVME_FAT_BLOCK_ENDPOINT,
    SCAFFOLD_VALUE_NVME_FAT_WRITE_AUTHORITY,
    SCAFFOLD_VALUE_NVME_FAT_COMMIT_AUTHORITY,
    SCAFFOLD_VALUE_NVME_FAT_UNAVAILABLE,
    SCAFFOLD_VALUE_NVME_FAT_ERROR
};

static u32 scaffold_bool_u32(u32 value)
{
    return value != 0u ? 1u : 0u;
}

static u64 scaffold_value_read(u8 selector)
{
    switch (selector)
    {
    case SCAFFOLD_VALUE_MOUSE_FOUND: return input64_mouse_found();
    case SCAFFOLD_VALUE_MOUSE_DELTA: return input64_mouse_delta_seen();
    case SCAFFOLD_VALUE_MOUSE_BUTTONS: return scaffold_bool_u32(input64_mouse_buttons());
    case SCAFFOLD_VALUE_MOUSE_PACKETS: return input64_mouse_packet_count();
    case SCAFFOLD_VALUE_MOUSE_PENDING: return input64_mouse_pending_count();
    case SCAFFOLD_VALUE_MOUSE_DROPS: return input64_mouse_drop_count();
    case SCAFFOLD_VALUE_MOUSE_X: return input64_mouse_x();
    case SCAFFOLD_VALUE_MOUSE_Y: return input64_mouse_y();
    case SCAFFOLD_VALUE_MOUSE_LAST_DX: return input64_mouse_last_dx();
    case SCAFFOLD_VALUE_MOUSE_LAST_DY: return input64_mouse_last_dy();
    case SCAFFOLD_VALUE_MOUSE_PS2_ENABLED: return input64_mouse_enabled();
    case SCAFFOLD_VALUE_MOUSE_USB_DEVICE: return xhci64_mouse_device();
    case SCAFFOLD_VALUE_COMPOSITOR_INIT: return display64_compositor_init_done();
    case SCAFFOLD_VALUE_COMPOSITOR_PRESENT_BOOL: return scaffold_bool_u32(display64_compositor_present_count());
    case SCAFFOLD_VALUE_COMPOSITOR_CURSOR_BOOL: return scaffold_bool_u32(display64_compositor_cursor_count());
    case SCAFFOLD_VALUE_COMPOSITOR_PRESENTS: return display64_compositor_present_count();
    case SCAFFOLD_VALUE_COMPOSITOR_CURSORS: return display64_compositor_cursor_count();
    case SCAFFOLD_VALUE_FONT_INIT: return display64_font_init_done();
    case SCAFFOLD_VALUE_FONT_GLYPHS: return display64_font_glyph_count();
    case SCAFFOLD_VALUE_FONT_RENDER_BOOL: return scaffold_bool_u32(display64_font_render_count());
    case SCAFFOLD_VALUE_FONT_RENDERS: return display64_font_render_count();
    case SCAFFOLD_VALUE_WM_INIT: return display64_wm_init_done();
    case SCAFFOLD_VALUE_WM_WINDOW_BOOL: return scaffold_bool_u32(display64_wm_window_created_count());
    case SCAFFOLD_VALUE_WM_FOCUS_BOOL: return scaffold_bool_u32(display64_wm_focus_count());
    case SCAFFOLD_VALUE_WM_PRESENT_BOOL: return scaffold_bool_u32(display64_wm_present_count());
    case SCAFFOLD_VALUE_WM_WINDOWS: return display64_wm_window_created_count();
    case SCAFFOLD_VALUE_WM_FOCUSES: return display64_wm_focus_count();
    case SCAFFOLD_VALUE_WM_PRESENTS: return display64_wm_present_count();
    case SCAFFOLD_VALUE_APIC_MADT: return apic64_madt_found();
    case SCAFFOLD_VALUE_APIC_LAPIC_BASE: return apic64_lapic_base();
    case SCAFFOLD_VALUE_APIC_IOAPIC_BASE: return apic64_ioapic_base();
    case SCAFFOLD_VALUE_APIC_PIC_DISABLED: return apic64_pic_disabled();
    case SCAFFOLD_VALUE_APIC_TIMER_TICKING: return scaffold_bool_u32(pit_get_ticks());
    case SCAFFOLD_VALUE_APIC_KEYBOARD_LIVE:
        return scaffold_bool_u32(input64_keyboard_byte_count() | xhci64_input_live());
    case SCAFFOLD_VALUE_APIC_ENABLED: return apic64_enabled();
    case SCAFFOLD_VALUE_APIC_LAPIC_ID: return apic64_lapic_id();
    case SCAFFOLD_VALUE_APIC_IOAPIC_GSI_BASE: return apic64_ioapic_gsi_base();
    case SCAFFOLD_VALUE_APIC_IOAPIC_MAX_REDIR: return apic64_ioapic_max_redirection();
    case SCAFFOLD_VALUE_APIC_IRQ0: return apic64_irq0_routed();
    case SCAFFOLD_VALUE_APIC_IRQ1: return apic64_irq1_routed();
    case SCAFFOLD_VALUE_APIC_IRQ11: return apic64_irq11_routed();
    case SCAFFOLD_VALUE_APIC_IRQ12: return apic64_irq12_routed();
    case SCAFFOLD_VALUE_APIC_OVERRIDE_SCANNED: return apic64_override_scanned();
    case SCAFFOLD_VALUE_APIC_OVERRIDE_COUNT: return apic64_override_count();
    case SCAFFOLD_VALUE_APIC_TIMER_GSI: return apic64_timer_gsi();
    case SCAFFOLD_VALUE_APIC_TIMER_POLARITY: return apic64_timer_polarity();
    case SCAFFOLD_VALUE_APIC_TIMER_TRIGGER: return apic64_timer_trigger();
    case SCAFFOLD_VALUE_APIC_KEYBOARD_GSI: return apic64_keyboard_gsi();
    case SCAFFOLD_VALUE_APIC_KEYBOARD_POLARITY: return apic64_keyboard_polarity();
    case SCAFFOLD_VALUE_APIC_KEYBOARD_TRIGGER: return apic64_keyboard_trigger();
    case SCAFFOLD_VALUE_APIC_AHCI_GSI: return apic64_ahci_gsi();
    case SCAFFOLD_VALUE_APIC_AHCI_POLARITY: return apic64_ahci_polarity();
    case SCAFFOLD_VALUE_APIC_AHCI_TRIGGER: return apic64_ahci_trigger();
    case SCAFFOLD_VALUE_APIC_MOUSE_GSI: return apic64_mouse_gsi();
    case SCAFFOLD_VALUE_APIC_MOUSE_POLARITY: return apic64_mouse_polarity();
    case SCAFFOLD_VALUE_APIC_MOUSE_TRIGGER: return apic64_mouse_trigger();
    case SCAFFOLD_VALUE_XHCI_FOUND: return xhci64_found();
    case SCAFFOLD_VALUE_XHCI_BAR0: return xhci64_bar0();
    case SCAFFOLD_VALUE_XHCI_MAPPED: return xhci64_mapped();
    case SCAFFOLD_VALUE_XHCI_CAP: return xhci64_cap_length();
    case SCAFFOLD_VALUE_XHCI_PORTS: return xhci64_hcs_ports();
    case SCAFFOLD_VALUE_XHCI_PORTS_SCANNED: return xhci64_ports_scanned();
    case SCAFFOLD_VALUE_XHCI_CONNECTED: return xhci64_connected_ports();
    case SCAFFOLD_VALUE_XHCI_COMMAND_RING: return xhci64_command_ring_staged();
    case SCAFFOLD_VALUE_XHCI_DCBAA: return xhci64_dcbaa_staged();
    case SCAFFOLD_VALUE_XHCI_EVENT_RING: return xhci64_event_ring_staged();
    case SCAFFOLD_VALUE_XHCI_RESET: return xhci64_controller_reset();
    case SCAFFOLD_VALUE_XHCI_RUNNING: return xhci64_controller_running();
    case SCAFFOLD_VALUE_XHCI_SLOT_ENABLED: return xhci64_slot_enabled();
    case SCAFFOLD_VALUE_XHCI_ADDRESSED: return xhci64_addressed();
    case SCAFFOLD_VALUE_XHCI_CONFIG_READ: return xhci64_config_read();
    case SCAFFOLD_VALUE_XHCI_REPORT_DESC: return xhci64_hid_report_read();
    case SCAFFOLD_VALUE_XHCI_ENDPOINT: return xhci64_endpoint_configured();
    case SCAFFOLD_VALUE_XHCI_HID_DEVICE: return xhci64_hid_device();
    case SCAFFOLD_VALUE_XHCI_INPUT_LIVE: return xhci64_input_live();
    case SCAFFOLD_VALUE_XHCI_REPORTS: return xhci64_report_count();
    case SCAFFOLD_VALUE_XHCI_REPORT_BYTES: return xhci64_report_bytes();
    case SCAFFOLD_VALUE_XHCI_UNAVAILABLE: return xhci64_unavailable();
    case SCAFFOLD_VALUE_XHCI_ERROR: return xhci64_error();
    case SCAFFOLD_VALUE_XHCI_EXTCAPS_SCANNED: return xhci64_extcaps_scanned();
    case SCAFFOLD_VALUE_XHCI_LEGACY_CAP: return xhci64_legacy_cap_found();
    case SCAFFOLD_VALUE_XHCI_LEGACY_HANDOFF: return xhci64_legacy_handoff();
    case SCAFFOLD_VALUE_XHCI_BIOS_OWNED_BEFORE: return xhci64_legacy_bios_owned_before();
    case SCAFFOLD_VALUE_XHCI_BIOS_OWNED_CLEAR: return xhci64_legacy_bios_owned_clear();
    case SCAFFOLD_VALUE_XHCI_OS_OWNED: return xhci64_legacy_os_owned();
    case SCAFFOLD_VALUE_XHCI_PROTOCOL_CAPS: return xhci64_protocol_caps();
    case SCAFFOLD_VALUE_XHCI_USB2_PORTS: return xhci64_usb2_ports();
    case SCAFFOLD_VALUE_XHCI_USB3_PORTS: return xhci64_usb3_ports();
    case SCAFFOLD_VALUE_XHCI_PREFER_USB2: return xhci64_prefer_usb2();
    case SCAFFOLD_VALUE_XHCI_INTEL_CAP: return xhci64_intel_cap_found();
    case SCAFFOLD_VALUE_XHCI_INTEL_WORKAROUND: return xhci64_intel_workaround();
    case SCAFFOLD_VALUE_XHCI_RESET_WAIT_MS: return xhci64_port_reset_wait_ms();
    case SCAFFOLD_VALUE_XHCI_SETTLE_MS: return xhci64_device_settle_ms();
    case SCAFFOLD_VALUE_NET_FOUND: return virtio_net64_found();
    case SCAFFOLD_VALUE_NET_BAR0: return virtio_net64_bar_base();
    case SCAFFOLD_VALUE_NET_MAPPED: return virtio_net64_mapped();
    case SCAFFOLD_VALUE_NET_COMMON: return virtio_net64_common();
    case SCAFFOLD_VALUE_NET_NOTIFY: return virtio_net64_notify();
    case SCAFFOLD_VALUE_NET_DEVICE_CONFIG: return virtio_net64_device_config();
    case SCAFFOLD_VALUE_NET_MAC: return pack_mac48(virtio_net64_mac());
    case SCAFFOLD_VALUE_NET_MAC_NONZERO: return virtio_net64_mac_nonzero();
    case SCAFFOLD_VALUE_NET_STATUS_ACK: return virtio_net64_status_ack();
    case SCAFFOLD_VALUE_NET_STATUS_DRIVER: return virtio_net64_status_driver();
    case SCAFFOLD_VALUE_NET_FEATURES_OK: return virtio_net64_features_ok();
    case SCAFFOLD_VALUE_NET_DRIVER_OK: return virtio_net64_driver_ok();
    case SCAFFOLD_VALUE_NET_RX_QUEUE: return virtio_net64_rx_queue();
    case SCAFFOLD_VALUE_NET_TX_QUEUE: return virtio_net64_tx_queue();
    case SCAFFOLD_VALUE_NET_RX_BUFFERS: return virtio_net64_rx_buffers();
    case SCAFFOLD_VALUE_NET_TX: return virtio_net64_tx();
    case SCAFFOLD_VALUE_NET_RX: return virtio_net64_rx();
    case SCAFFOLD_VALUE_NET_ARP_REPLY: return virtio_net64_arp_reply();
    case SCAFFOLD_VALUE_NET_ARP_MAC: return pack_mac48(virtio_net64_arp_mac());
    case SCAFFOLD_VALUE_NET_ARP_IP: return virtio_net64_arp_ip();
    case SCAFFOLD_VALUE_NET_FS_AUTHORITY: return virtio_net64_fs_authority();
    case SCAFFOLD_VALUE_NET_STORAGE_AUTHORITY: return virtio_net64_storage_authority();
    case SCAFFOLD_VALUE_NET_AMBIENT_AUTHORITY: return virtio_net64_ambient_authority();
    case SCAFFOLD_VALUE_NET_UNAVAILABLE: return virtio_net64_unavailable();
    case SCAFFOLD_VALUE_NET_ERROR: return virtio_net64_error();
    case SCAFFOLD_VALUE_E1000_FOUND: return e1000e64_found();
    case SCAFFOLD_VALUE_E1000_BAR0: return e1000e64_bar_base();
    case SCAFFOLD_VALUE_E1000_MAPPED: return e1000e64_mapped();
    case SCAFFOLD_VALUE_E1000_RESET: return e1000e64_reset();
    case SCAFFOLD_VALUE_E1000_MAC: return pack_mac48(e1000e64_mac());
    case SCAFFOLD_VALUE_E1000_MAC_NONZERO: return e1000e64_mac_nonzero();
    case SCAFFOLD_VALUE_E1000_LINK_UP: return e1000e64_link_up();
    case SCAFFOLD_VALUE_E1000_RX_QUEUE: return e1000e64_rx_queue();
    case SCAFFOLD_VALUE_E1000_TX_QUEUE: return e1000e64_tx_queue();
    case SCAFFOLD_VALUE_E1000_RX_BUFFERS: return e1000e64_rx_buffers();
    case SCAFFOLD_VALUE_E1000_TX: return e1000e64_tx();
    case SCAFFOLD_VALUE_E1000_RX: return e1000e64_rx();
    case SCAFFOLD_VALUE_E1000_DHCP: return virtio_net64_dhcp_ack();
    case SCAFFOLD_VALUE_E1000_DNS: return virtio_net64_dns_response();
    case SCAFFOLD_VALUE_E1000_HTTP: return virtio_net64_http_status();
    case SCAFFOLD_VALUE_E1000_FS_AUTHORITY: return e1000e64_fs_authority();
    case SCAFFOLD_VALUE_E1000_STORAGE_AUTHORITY: return e1000e64_storage_authority();
    case SCAFFOLD_VALUE_E1000_AMBIENT_AUTHORITY: return e1000e64_ambient_authority();
    case SCAFFOLD_VALUE_E1000_UNAVAILABLE: return e1000e64_unavailable();
    case SCAFFOLD_VALUE_E1000_ERROR: return e1000e64_error();
    case SCAFFOLD_VALUE_DHCP_DISCOVER: return virtio_net64_dhcp_discover();
    case SCAFFOLD_VALUE_DHCP_OFFER: return virtio_net64_dhcp_offer();
    case SCAFFOLD_VALUE_DHCP_REQUEST: return virtio_net64_dhcp_request();
    case SCAFFOLD_VALUE_DHCP_ACK: return virtio_net64_dhcp_ack();
    case SCAFFOLD_VALUE_DHCP_IP: return virtio_net64_dhcp_ip();
    case SCAFFOLD_VALUE_DHCP_GATEWAY: return virtio_net64_dhcp_gateway();
    case SCAFFOLD_VALUE_DHCP_DNS: return virtio_net64_dhcp_dns();
    case SCAFFOLD_VALUE_DHCP_LEASE: return virtio_net64_dhcp_lease();
    case SCAFFOLD_VALUE_DHCP_AMBIENT_AUTHORITY: return virtio_net64_ambient_authority();
    case SCAFFOLD_VALUE_DHCP_UNAVAILABLE: return virtio_net64_dhcp_unavailable();
    case SCAFFOLD_VALUE_DHCP_ERROR: return virtio_net64_dhcp_error();
    case SCAFFOLD_VALUE_DNS_QUERY: return virtio_net64_dns_query();
    case SCAFFOLD_VALUE_DNS_RESPONSE: return virtio_net64_dns_response();
    case SCAFFOLD_VALUE_DNS_RCODE: return virtio_net64_dns_rcode();
    case SCAFFOLD_VALUE_DNS_RESOLVED: return virtio_net64_dns_resolved();
    case SCAFFOLD_VALUE_DNS_FS_AUTHORITY: return virtio_net64_fs_authority();
    case SCAFFOLD_VALUE_DNS_STORAGE_AUTHORITY: return virtio_net64_storage_authority();
    case SCAFFOLD_VALUE_DNS_AMBIENT_AUTHORITY: return virtio_net64_ambient_authority();
    case SCAFFOLD_VALUE_DNS_UNAVAILABLE: return virtio_net64_dns_unavailable();
    case SCAFFOLD_VALUE_DNS_ERROR: return virtio_net64_dns_error();
    case SCAFFOLD_VALUE_HTTP_CONNECTED: return virtio_net64_http_connected();
    case SCAFFOLD_VALUE_HTTP_SENT: return virtio_net64_http_sent();
    case SCAFFOLD_VALUE_HTTP_STATUS: return virtio_net64_http_status();
    case SCAFFOLD_VALUE_HTTP_RESPONSE_BYTES: return virtio_net64_http_response_bytes();
    case SCAFFOLD_VALUE_HTTP_FS_AUTHORITY: return virtio_net64_fs_authority();
    case SCAFFOLD_VALUE_HTTP_STORAGE_AUTHORITY: return virtio_net64_storage_authority();
    case SCAFFOLD_VALUE_HTTP_AMBIENT_AUTHORITY: return virtio_net64_ambient_authority();
    case SCAFFOLD_VALUE_HTTP_UNAVAILABLE: return virtio_net64_http_unavailable();
    case SCAFFOLD_VALUE_HTTP_ERROR: return virtio_net64_http_error();
    default: return 0ull;
    }
}

static void write_scaffold_value_fields(const struct scaffold_value_field *fields, u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        write_string(fields[index].label);
        write_formatted_u64(scaffold_value_read(fields[index].selector), fields[index].format);
    }
}

static void write_scaffold_prefixed_value_fields(
    const char *first_prefix,
    const char *field_prefix,
    const struct scaffold_value_field *fields,
    u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        write_string((index == 0u) ? first_prefix : field_prefix);
        write_string(fields[index].label);
        write_formatted_u64(scaffold_value_read(fields[index].selector), fields[index].format);
    }
}

static void log_mouse_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"found ", SCAFFOLD_VALUE_MOUSE_FOUND, SCAFFOLD_TELEMETRY_DEC},
        {"delta ", SCAFFOLD_VALUE_MOUSE_DELTA, SCAFFOLD_TELEMETRY_DEC},
        {"buttons ", SCAFFOLD_VALUE_MOUSE_BUTTONS, SCAFFOLD_TELEMETRY_DEC},
        {"packets ", SCAFFOLD_VALUE_MOUSE_PACKETS, SCAFFOLD_TELEMETRY_DEC},
        {"pending ", SCAFFOLD_VALUE_MOUSE_PENDING, SCAFFOLD_TELEMETRY_DEC},
        {"drops ", SCAFFOLD_VALUE_MOUSE_DROPS, SCAFFOLD_TELEMETRY_DEC},
        {"x ", SCAFFOLD_VALUE_MOUSE_X, SCAFFOLD_TELEMETRY_DEC},
        {"y ", SCAFFOLD_VALUE_MOUSE_Y, SCAFFOLD_TELEMETRY_DEC},
        {"last-dx ", SCAFFOLD_VALUE_MOUSE_LAST_DX, SCAFFOLD_TELEMETRY_HEX64},
        {"last-dy ", SCAFFOLD_VALUE_MOUSE_LAST_DY, SCAFFOLD_TELEMETRY_HEX64},
        {"ps2-enabled ", SCAFFOLD_VALUE_MOUSE_PS2_ENABLED, SCAFFOLD_TELEMETRY_DEC},
        {"usb-device ", SCAFFOLD_VALUE_MOUSE_USB_DEVICE, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-mouse drs-mouse-",
        " drs-mouse-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_line("");
}

static void log_compositor_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"init ", SCAFFOLD_VALUE_COMPOSITOR_INIT, SCAFFOLD_TELEMETRY_DEC},
        {"present ", SCAFFOLD_VALUE_COMPOSITOR_PRESENT_BOOL, SCAFFOLD_TELEMETRY_DEC},
        {"cursor ", SCAFFOLD_VALUE_COMPOSITOR_CURSOR_BOOL, SCAFFOLD_TELEMETRY_DEC},
        {"presents ", SCAFFOLD_VALUE_COMPOSITOR_PRESENTS, SCAFFOLD_TELEMETRY_DEC},
        {"cursors ", SCAFFOLD_VALUE_COMPOSITOR_CURSORS, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-compositor drs-compositor-",
        " drs-compositor-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_line("");
}

static void log_font_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"init ", SCAFFOLD_VALUE_FONT_INIT, SCAFFOLD_TELEMETRY_DEC},
        {"glyphs ", SCAFFOLD_VALUE_FONT_GLYPHS, SCAFFOLD_TELEMETRY_DEC},
        {"render ", SCAFFOLD_VALUE_FONT_RENDER_BOOL, SCAFFOLD_TELEMETRY_DEC},
        {"renders ", SCAFFOLD_VALUE_FONT_RENDERS, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-font drs-font-",
        " drs-font-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_line("");
}

static void log_window_manager_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"init ", SCAFFOLD_VALUE_WM_INIT, SCAFFOLD_TELEMETRY_DEC},
        {"window-created ", SCAFFOLD_VALUE_WM_WINDOW_BOOL, SCAFFOLD_TELEMETRY_DEC},
        {"focus ", SCAFFOLD_VALUE_WM_FOCUS_BOOL, SCAFFOLD_TELEMETRY_DEC},
        {"present ", SCAFFOLD_VALUE_WM_PRESENT_BOOL, SCAFFOLD_TELEMETRY_DEC},
        {"windows ", SCAFFOLD_VALUE_WM_WINDOWS, SCAFFOLD_TELEMETRY_DEC},
        {"focuses ", SCAFFOLD_VALUE_WM_FOCUSES, SCAFFOLD_TELEMETRY_DEC},
        {"presents ", SCAFFOLD_VALUE_WM_PRESENTS, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-wm drs-wm-",
        " drs-wm-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_line("");
}

static void log_desktop_surface(void)
{
    write_labeled_dec_u32("[x64] drs-desktop drs-desktop-init ", display64_desktop_init_done());
    write_labeled_dec_u32(" drs-desktop-taskbar ", scaffold_bool_u32(display64_desktop_taskbar_count()));
    write_labeled_dec_u32(" drs-desktop-launcher ", scaffold_bool_u32(display64_desktop_launcher_count()));
    write_labeled_dec_u32(" drs-desktop-terminal ", scaffold_bool_u32(display64_desktop_terminal_count()));
    write_labeled_dec_u32(" drs-desktop-fileman ", scaffold_bool_u32(display64_desktop_fileman_count()));
    write_labeled_dec_u32(" drs-desktop-settings ", scaffold_bool_u32(display64_desktop_settings_count()));
    write_labeled_dec_u32(" drs-desktop-installer ", scaffold_bool_u32(display64_gui_installer_opened()));
    write_labeled_dec_u32(" drs-desktop-assistant ", scaffold_bool_u32(display64_gui_assistant_opened()));
    write_line("");
}

static void log_gui_interactive_surface(void)
{
    write_labeled_dec_u32("[x64] drs-gui drs-gui-interactive ", display64_gui_interactive());
    write_labeled_dec_u32(" drs-gui-click-hittest ", display64_gui_click_hittest());
    write_labeled_dec_u32(" drs-gui-launcher-opened ", display64_gui_launcher_opened());
    write_labeled_dec_u32(" drs-gui-terminal-opened ", display64_gui_terminal_opened());
    write_labeled_dec_u32(" drs-gui-drag-completed ", display64_gui_drag_completed());
    write_labeled_dec_u32(" drs-gui-keyboard-routed ", display64_gui_keyboard_routed());
    write_labeled_dec_u32(" drs-gui-close-completed ", display64_gui_close_completed());
    write_labeled_dec_u32(" drs-gui-taskbar-focus ", display64_gui_taskbar_focus());
    write_labeled_dec_u32(" drs-gui-fileman-opened ", display64_gui_fileman_opened());
    write_labeled_dec_u32(" drs-gui-settings-opened ", display64_gui_settings_opened());
    write_labeled_dec_u32(" drs-gui-installer-opened ", display64_gui_installer_opened());
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    write_labeled_dec_u32(" drs-gui-right-click ", display64_gui_right_click_count());
    write_labeled_dec_u32(" drs-gui-scroll ", display64_gui_scroll_count());
    write_labeled_dec_u32(" terminal-actions ", display64_gui_terminal_action_count());
    write_labeled_dec_u32(" fileman-actions ", display64_gui_fileman_action_count());
    write_labeled_dec_u32(" fileman-refresh ", display64_gui_fileman_backend_refresh_count());
    write_labeled_dec_u32(" fileman-preview ", display64_gui_fileman_backend_preview_count());
    write_labeled_dec_u32(" fileman-open-dir ", display64_gui_fileman_backend_open_dir_count());
    write_labeled_dec_u32(" fileman-write ", display64_gui_fileman_backend_write_count());
    write_labeled_dec_u32(" fileman-write-denial ", display64_gui_fileman_backend_write_denial_count());
    write_labeled_dec_u32(" fileman-delete ", display64_gui_fileman_backend_delete_count());
    write_labeled_dec_u32(" fileman-delete-denial ", display64_gui_fileman_backend_delete_denial_count());
    write_labeled_dec_u32(" fileman-delete-confirm ", display64_gui_fileman_backend_delete_confirm_count());
    write_labeled_dec_u32(" fileman-mkdir ", display64_gui_fileman_backend_mkdir_count());
    write_labeled_dec_u32(" fileman-mkdir-denial ", display64_gui_fileman_backend_mkdir_denial_count());
    write_labeled_dec_u32(" fileman-copy ", display64_gui_fileman_backend_copy_count());
    write_labeled_dec_u32(" fileman-copy-denial ", display64_gui_fileman_backend_copy_denial_count());
    write_labeled_dec_u32(" fileman-rename ", display64_gui_fileman_backend_rename_count());
    write_labeled_dec_u32(" fileman-rename-denial ", display64_gui_fileman_backend_rename_denial_count());
    write_labeled_dec_u32(" fileman-move ", display64_gui_fileman_backend_move_count());
    write_labeled_dec_u32(" fileman-move-denial ", display64_gui_fileman_backend_move_denial_count());
    write_labeled_dec_u32(" fileman-edit ", display64_gui_fileman_backend_edit_count());
    write_labeled_dec_u32(" fileman-edit-commit ", display64_gui_fileman_backend_edit_commit_count());
    write_labeled_dec_u32(" settings-actions ", display64_gui_settings_action_count());
    write_labeled_dec_u32(" installer-actions ", display64_gui_installer_action_count());
#endif
    write_labeled_dec_u32(" drs-gui-unfocused-key-denied ", display64_gui_unfocused_key_denied());
    write_labeled_dec_u32(" drs-gui-no-ambient-input ", display64_gui_no_ambient_input());
    write_labeled_dec_u32(" drs-gui-no-ambient-display ", display64_gui_no_ambient_display());
    write_labeled_dec_u32(" drs-gui-no-ambient-fs ", display64_gui_no_ambient_fs());
    write_labeled_dec_u32(" mouse-x ", display64_gui_mouse_x());
    write_labeled_dec_u32(" mouse-y ", display64_gui_mouse_y());
    write_labeled_dec_u32(" target-window ", display64_gui_target_window());
    write_labeled_dec_u32(" target-region ", display64_gui_target_region());
    write_labeled_dec_u32(" focus-before ", display64_gui_focus_before());
    write_labeled_dec_u32(" focus-after ", display64_gui_focus_after());
    write_labeled_dec_u32(" z-before ", display64_gui_z_before());
    write_labeled_dec_u32(" z-after ", display64_gui_z_after());
    write_labeled_dec_u32(" key-target-window ", display64_gui_key_target_window());
    write_labeled_dec_u32(" unfocused-key-denials ", display64_gui_unfocused_key_denial_count());
    write_labeled_hex_u32(" input-token ", display64_gui_input_path_token());
    write_labeled_hex_u32(" display-token ", display64_gui_display_path_token());
    write_labeled_hex_u32(" fs-token ", display64_gui_fs_path_token());
    write_labeled_dec_u32(" assistant-opened ", display64_gui_assistant_opened());
    write_line("");
}

static void log_login_surface(void)
{
    write_labeled_dec_u32("[x64] drs-login drs-login-screen ", auth64_login_screen());
    write_labeled_dec_u32(" drs-login-auth-success ", auth64_auth_success());
    write_labeled_dec_u32(" drs-login-wrong-password-denied ", auth64_wrong_password_denied());
    write_labeled_dec_u32(" drs-login-rate-limited ", auth64_rate_limited());
    write_labeled_dec_u32(" drs-session-lock ", auth64_session_lock());
    write_labeled_dec_u32(" drs-session-unlock ", auth64_session_unlock());
    write_labeled_dec_u32(" drs-session-authority-scoped ", auth64_session_authority_scoped());
    write_labeled_dec_u32(" first-run-setup ", auth64_first_run_setup());
    write_labeled_dec_u32(" user-store-nvme ", auth64_user_store_nvme());
    write_labeled_dec_u32(" user-store-persistent ", auth64_user_store_persistent());
    write_labeled_dec_u32(" bcrypt-hash ", auth64_bcrypt_hash());
    write_labeled_dec_u32(" login-display-only ", auth64_login_display_only());
    write_labeled_dec_u32(" login-input-only ", auth64_login_input_only());
    write_labeled_dec_u32(" desktop-blocked-pre-auth ", auth64_desktop_blocked_pre_auth());
    write_labeled_dec_u32(" failures ", auth64_failure_count());
    write_labeled_dec_u32(" lockout-seconds ", auth64_lockout_seconds());
    write_string(" user ");
    write_string(auth64_active_user());
    write_string(" home ");
    write_string(auth64_home_namespace());
    write_string(" profile ");
    write_line(auth64_session_profile());
}

static void log_identity_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    identity64_init();
    write_labeled_dec_u32("[x64] drs-identity drs-identity-foundation ", identity64_foundation());
    write_labeled_dec_u32(" drs-identity-local-active ", identity64_local_active());
    write_labeled_dec_u32(" drs-identity-personal-unavailable ", identity64_personal_unavailable());
    write_labeled_dec_u32(" drs-identity-enterprise-unavailable ", identity64_enterprise_unavailable());
    write_labeled_dec_u32(" drs-identity-settings-panel ", scaffold_bool_u32(display64_identity_settings_panel_count()));
    write_labeled_dec_u32(" drs-identity-status-readonly ", identity64_status_readonly());
    write_labeled_dec_u32(" drs-identity-mutation-denied ", identity64_mutation_denied());
    write_labeled_dec_u32(" drs-vault-foundation ", identity64_vault_foundation());
    write_labeled_dec_u32(" drs-vault-secret-read-denied ", identity64_vault_secret_read_denied());
    write_labeled_dec_u32(" drs-vault-secret-write-denied ", identity64_vault_secret_write_denied());
    write_labeled_dec_u32(" drs-vault-no-plaintext-token ", identity64_vault_no_plaintext_token());
    write_labeled_dec_u32(" drs-cloud-association-unavailable ", identity64_cloud_association_unavailable());
    write_labeled_dec_u32(" drs-no-ambient-identity ", identity64_no_ambient_identity());
    write_labeled_dec_u32(" drs-no-ambient-secret ", identity64_no_ambient_secret());
    write_labeled_dec_u32(" encrypted-vault ", identity64_encrypted_vault_available());
    write_labeled_dec_u32(" secret-storage ", identity64_real_secret_storage_enabled());
    write_string(" account-type ");
    write_string(identity64_active_account_type());
    write_string(" account-id ");
    write_string(identity64_active_account_id());
    write_string(" display ");
    write_string(identity64_display_name());
    write_string(" association ");
    write_string(identity64_association_status());
    write_string(" network ");
    write_string(identity64_offline_online_status());
    write_string(" credential ");
    write_string(identity64_credential_record_type());
    write_string(" vault ");
    write_line(identity64_vault_binding_status());
#endif
}

static void log_identity_transport_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    identity_transport64_init();
    write_labeled_dec_u32("[x64] drs-idtransport drs-idtransport-product ", identity_transport64_product());
    write_labeled_dec_u32(" drs-idtransport-provider-descriptor ", identity_transport64_provider_descriptor());
    write_labeled_dec_u32(" drs-idtransport-descriptor-verified ", identity_transport64_descriptor_verified());
    write_labeled_dec_u32(" drs-idtransport-descriptor-missing-sig-denied ", identity_transport64_descriptor_missing_sig_denied());
    write_labeled_dec_u32(" drs-idtransport-descriptor-invalid-sig-denied ", identity_transport64_descriptor_invalid_sig_denied());
    write_labeled_dec_u32(" drs-idtransport-descriptor-wrong-key-denied ", identity_transport64_descriptor_wrong_key_denied());
    write_labeled_dec_u32(" drs-idtransport-descriptor-tamper-denied ", identity_transport64_descriptor_tamper_denied());
    write_labeled_dec_u32(" drs-idtransport-descriptor-rollback-denied ", identity_transport64_descriptor_rollback_denied());
    write_labeled_dec_u32(" drs-idtransport-descriptor-version-denied ", identity_transport64_descriptor_version_denied());
    write_labeled_dec_u32(" drs-idtransport-network-scoped ", identity_transport64_network_scoped());
    write_labeled_dec_u32(" drs-idtransport-no-network-cap-denied ", identity_transport64_no_network_cap_denied());
    write_labeled_dec_u32(" drs-idtransport-plaintext-credential-denied ", identity_transport64_plaintext_credential_denied());
    write_labeled_dec_u32(" drs-idtransport-unverified-endpoint-denied ", identity_transport64_unverified_endpoint_denied());
    write_labeled_dec_u32(" drs-idtransport-token-storage-denied ", identity_transport64_token_storage_denied());
    write_labeled_dec_u32(" drs-idtransport-personal-unavailable ", identity_transport64_personal_unavailable());
    write_labeled_dec_u32(" drs-idtransport-enterprise-unavailable ", identity_transport64_enterprise_unavailable());
    write_labeled_dec_u32(" drs-idtransport-cloud-association-unavailable ", identity_transport64_cloud_association_unavailable());
    write_labeled_dec_u32(" drs-idtransport-settings-panel ", scaffold_bool_u32(display64_identity_transport_settings_panel_count()));
    write_labeled_dec_u32(" drs-idtransport-status-readonly ", identity_transport64_status_readonly());
    write_labeled_dec_u32(" drs-idtransport-trusted-time-status ", identity_transport64_trusted_time_status());
    write_labeled_dec_u32(" drs-no-ambient-idtransport-network ", identity_transport64_no_ambient_network());
    write_labeled_dec_u32(" drs-no-ambient-idtransport-identity ", identity_transport64_no_ambient_identity());
    write_labeled_dec_u32(" drs-no-ambient-idtransport-secret ", identity_transport64_no_ambient_secret());
    write_labeled_dec_u32(" drs-idtransport-encrypted-channel-unavailable ", identity_transport64_encrypted_channel_unavailable());
    write_labeled_dec_u32(" drs-idtransport-credential-transport-unavailable ", identity_transport64_credential_transport_unavailable());
    write_string(" mode ");
    write_string(identity_transport64_mode());
    write_string(" provider ");
    write_string(identity_transport64_provider_id());
    write_string(" provider-type ");
    write_string(identity_transport64_provider_type());
    write_string(" endpoint ");
    write_string(identity_transport64_endpoint_status());
    write_string(" online ");
    write_string(identity_transport64_online_status());
    write_string(" encrypted ");
    write_string(identity_transport64_encrypted_transport_status());
    write_string(" credential ");
    write_string(identity_transport64_credential_transport_status());
    write_string(" token-storage ");
    write_string(identity_transport64_token_storage_status());
    write_string(" trusted-time ");
    write_line(identity_transport64_trusted_time_string());
#endif
}

static void log_account_association_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    account_association64_init();
    write_labeled_dec_u32("[x64] drs-account drs-account-association-product ", account_association64_product());
    write_labeled_dec_u32(" drs-account-local-active ", account_association64_local_active());
    write_labeled_dec_u32(" drs-account-personal-unavailable ", account_association64_personal_unavailable());
    write_labeled_dec_u32(" drs-account-enterprise-unavailable ", account_association64_enterprise_unavailable());
    write_labeled_dec_u32(" drs-account-cloud-unavailable ", account_association64_cloud_unavailable());
    write_labeled_dec_u32(" drs-account-security-key-unavailable ", account_association64_security_key_unavailable());
    write_labeled_dec_u32(" drs-account-settings-panel ", scaffold_bool_u32(display64_account_settings_panel_count()));
    write_labeled_dec_u32(" drs-account-status-readonly ", account_association64_status_readonly());
    write_labeled_dec_u32(" drs-account-mutation-denied ", account_association64_mutation_denied());
    write_labeled_dec_u32(" drs-account-unlink-denied ", account_association64_unlink_denied());
    write_labeled_dec_u32(" drs-account-token-storage-denied ", account_association64_token_storage_denied());
    write_labeled_dec_u32(" drs-account-credential-transport-denied ", account_association64_credential_transport_denied());
    write_labeled_dec_u32(" drs-account-enterprise-policy-unavailable ", account_association64_enterprise_policy_unavailable());
    write_labeled_dec_u32(" drs-account-remote-no-ambient-authority ", account_association64_remote_no_ambient_authority());
    write_labeled_dec_u32(" drs-no-ambient-account-identity ", account_association64_no_ambient_identity());
    write_labeled_dec_u32(" drs-no-ambient-account-network ", account_association64_no_ambient_network());
    write_labeled_dec_u32(" drs-no-ambient-account-secret ", account_association64_no_ambient_secret());
    write_string(" mode ");
    write_string(account_association64_mode());
    write_string(" local ");
    write_string(account_association64_local_status());
    write_string(" personal ");
    write_string(account_association64_personal_status());
    write_string(" enterprise ");
    write_string(account_association64_enterprise_status());
    write_string(" cloud ");
    write_string(account_association64_cloud_status());
    write_string(" security-key ");
    write_string(account_association64_security_key_status());
    write_string(" enterprise-policy ");
    write_string(account_association64_enterprise_policy_status());
    write_string(" encrypted ");
    write_string(account_association64_encrypted_transport_status());
    write_string(" token-storage ");
    write_string(account_association64_token_storage_status());
    write_string(" trusted-time ");
    write_string(account_association64_trusted_time_status());
    write_string(" remote-login ");
    write_string(account_association64_remote_login_status());
    write_string(" local-user ");
    write_string(account_association64_local_user_id());
    write_string(" provider ");
    write_line(account_association64_provider_id());
#endif
}

static void log_cloud_storage_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    cloud_storage64_init();
    write_labeled_dec_u32("[x64] drs-cloud drs-cloud-broker-product ", cloud_storage64_broker_product());
    write_labeled_dec_u32(" drs-cloud-provider-descriptor ", cloud_storage64_provider_descriptor());
    write_labeled_dec_u32(" drs-cloud-provider-verified ", cloud_storage64_provider_verified());
    write_labeled_dec_u32(" drs-cloud-provider-missing-sig-denied ", cloud_storage64_provider_missing_sig_denied());
    write_labeled_dec_u32(" drs-cloud-provider-invalid-sig-denied ", cloud_storage64_provider_invalid_sig_denied());
    write_labeled_dec_u32(" drs-cloud-provider-wrong-key-denied ", cloud_storage64_provider_wrong_key_denied());
    write_labeled_dec_u32(" drs-cloud-provider-tamper-denied ", cloud_storage64_provider_tamper_denied());
    write_labeled_dec_u32(" drs-cloud-provider-rollback-denied ", cloud_storage64_provider_rollback_denied());
    write_labeled_dec_u32(" drs-cloud-provider-version-denied ", cloud_storage64_provider_version_denied());
    write_labeled_dec_u32(" drs-cloud-provider-malformed-denied ", cloud_storage64_provider_malformed_denied());
    write_labeled_dec_u32(" drs-cloud-association-unavailable ", cloud_storage64_association_unavailable());
    write_labeled_dec_u32(" drs-cloud-account-unavailable ", cloud_storage64_account_unavailable());
    write_labeled_dec_u32(" drs-cloud-token-storage-denied ", cloud_storage64_token_storage_denied());
    write_labeled_dec_u32(" drs-cloud-encrypted-transport-unavailable ", cloud_storage64_encrypted_transport_unavailable());
    write_labeled_dec_u32(" drs-cloud-upload-denied ", cloud_storage64_upload_denied());
    write_labeled_dec_u32(" drs-cloud-download-denied ", cloud_storage64_download_denied());
    write_labeled_dec_u32(" drs-cloud-sync-denied ", cloud_storage64_sync_denied());
    write_labeled_dec_u32(" drs-cloud-auto-upload-unavailable ", cloud_storage64_auto_upload_unavailable());
    write_labeled_dec_u32(" drs-cloud-auto-download-unavailable ", cloud_storage64_auto_download_unavailable());
    write_labeled_dec_u32(" drs-cloud-ai-access-unavailable ", cloud_storage64_ai_access_unavailable());
    write_labeled_dec_u32(" drs-cloud-app-direct-denied ", cloud_storage64_app_direct_denied());
    write_labeled_dec_u32(" drs-cloud-settings-panel ", scaffold_bool_u32(display64_cloud_settings_panel_count()));
    write_labeled_dec_u32(" drs-cloud-settings-readonly ", cloud_storage64_settings_readonly());
    write_labeled_dec_u32(" drs-cloud-fileman-status ", scaffold_bool_u32(display64_cloud_fileman_status_count()));
    write_labeled_dec_u32(" drs-cloud-fileman-mutation-denied ", cloud_storage64_fileman_mutation_denied());
    write_labeled_dec_u32(" drs-no-ambient-cloud ", cloud_storage64_no_ambient_cloud());
    write_labeled_dec_u32(" drs-no-ambient-cloud-fs ", cloud_storage64_no_ambient_fs());
    write_labeled_dec_u32(" drs-no-ambient-cloud-network ", cloud_storage64_no_ambient_network());
    write_labeled_dec_u32(" drs-no-ambient-cloud-identity ", cloud_storage64_no_ambient_identity());
    write_labeled_dec_u32(" drs-no-ambient-cloud-secret ", cloud_storage64_no_ambient_secret());
    write_string(" mode ");
    write_string(cloud_storage64_broker_status());
    write_string(" storage-mode ");
    write_string(cloud_storage64_mode());
    write_string(" provider ");
    write_string(cloud_storage64_provider_id());
    write_string(" descriptor ");
    write_string(cloud_storage64_descriptor_status());
    write_string(" account ");
    write_string(cloud_storage64_account_status());
    write_string(" association ");
    write_string(cloud_storage64_association_status());
    write_string(" token-storage ");
    write_string(cloud_storage64_token_storage_status());
    write_string(" encrypted ");
    write_string(cloud_storage64_encrypted_transport_status());
    write_string(" sync ");
    write_string(cloud_storage64_sync_status());
    write_string(" upload ");
    write_string(cloud_storage64_upload_status());
    write_string(" download ");
    write_string(cloud_storage64_download_status());
    write_string(" offline-cache ");
    write_string(cloud_storage64_offline_cache_status());
    write_string(" ai ");
    write_string(cloud_storage64_ai_access_status());
    write_string(" app-direct ");
    write_line(cloud_storage64_app_direct_status());
#endif
}

static void log_installer_ux_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    installer_ux64_init();
    write_labeled_dec_u32("[x64] drs-installer-ux drs-installer-ux-product ", installer_ux64_product());
    write_labeled_dec_u32(" drs-installer-welcome ", scaffold_bool_u32(display64_installer_welcome_count()));
    write_labeled_dec_u32(" drs-installer-beginner-mode ", scaffold_bool_u32(display64_installer_beginner_count()));
    write_labeled_dec_u32(" drs-installer-advanced-mode ", scaffold_bool_u32(display64_installer_advanced_count()));
    write_labeled_dec_u32(" drs-installer-hardware-summary ", scaffold_bool_u32(display64_installer_hardware_count()));
    write_labeled_dec_u32(" drs-installer-recommendation ", scaffold_bool_u32(display64_installer_recommendation_count()));
    write_labeled_dec_u32(" drs-installer-component-selection ", scaffold_bool_u32(display64_installer_component_count()));
    write_labeled_dec_u32(" drs-installer-unavailable-components-labeled ", installer_ux64_unavailable_components_labeled());
    write_labeled_dec_u32(" drs-installer-account-page ", scaffold_bool_u32(display64_installer_account_count()));
    write_labeled_dec_u32(" drs-installer-personal-unavailable ", installer_ux64_personal_unavailable());
    write_labeled_dec_u32(" drs-installer-enterprise-unavailable ", installer_ux64_enterprise_unavailable());
    write_labeled_dec_u32(" drs-installer-cloud-page ", scaffold_bool_u32(display64_installer_cloud_count()));
    write_labeled_dec_u32(" drs-installer-cloud-sync-unavailable ", installer_ux64_cloud_sync_unavailable());
    write_labeled_dec_u32(" drs-installer-ai-page ", scaffold_bool_u32(display64_installer_ai_count()));
    write_labeled_dec_u32(" drs-installer-ai-setup-unavailable ", installer_ux64_ai_setup_unavailable());
    write_labeled_dec_u32(" drs-installer-plan-generated ", scaffold_bool_u32(display64_installer_plan_count()));
    write_labeled_dec_u32(" drs-installer-dryrun-no-writes ", installer_ux64_dryrun_no_writes());
    write_labeled_dec_u32(" drs-installer-forbidden-target-denied ", installer_ux64_forbidden_target_denied());
    write_labeled_dec_u32(" drs-installer-write-action-denied ", installer_ux64_write_action_denied());
    write_labeled_dec_u32(" drs-installer-format-action-denied ", installer_ux64_format_action_denied());
    write_labeled_dec_u32(" drs-installer-boot-entry-denied ", installer_ux64_boot_entry_denied());
    write_labeled_dec_u32(" drs-installer-package-install-denied ", installer_ux64_package_install_denied());
    write_labeled_dec_u32(" drs-installer-cloud-enable-denied ", installer_ux64_cloud_enable_denied());
    write_labeled_dec_u32(" drs-installer-ai-enable-denied ", installer_ux64_ai_enable_denied());
    write_labeled_dec_u32(" drs-no-ambient-installer ", installer_ux64_no_ambient_installer());
    write_labeled_dec_u32(" drs-no-ambient-installer-storage ", installer_ux64_no_ambient_storage());
    write_labeled_dec_u32(" drs-no-ambient-installer-firmware ", installer_ux64_no_ambient_firmware());
    write_labeled_dec_u32(" drs-no-ambient-installer-package ", installer_ux64_no_ambient_package());
    write_labeled_dec_u32(" drs-no-ambient-installer-identity-cloud-secret ", installer_ux64_no_ambient_identity_cloud_secret());
    write_labeled_dec_u32(" writes-planned ", installer_ux64_writes_planned());
    write_labeled_dec_u32(" formats-planned ", installer_ux64_formats_planned());
    write_labeled_dec_u32(" boot-entry-planned ", installer_ux64_boot_entries_planned());
    write_labeled_dec_u32(" package-ops-planned ", installer_ux64_package_ops_planned());
    write_labeled_dec_u32(" real-install-approved ", installer_ux64_real_install_approved());
    write_string(" mode ");
    write_string(installer_ux64_mode());
    write_string(" profile ");
    write_string(installer_ux64_selected_profile());
    write_string(" recommendation ");
    write_string(installer_ux64_recommendation_text());
    write_string(" components ");
    write_string(installer_ux64_component_status());
    write_string(" account ");
    write_string(installer_ux64_account_status());
    write_string(" cloud ");
    write_string(installer_ux64_cloud_status());
    write_string(" ai ");
    write_string(installer_ux64_ai_status());
    write_string(" plan ");
    write_string(installer_ux64_plan_status());
    write_string(" dryrun ");
    write_line(installer_ux64_dryrun_status());
#endif
}

static void log_installer_commit_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    (void)installer_ux64_commit_probe();
    write_labeled_dec_u32("[x64] drs-installer-commit drs-installer-commit-attempted ", installer_ux64_commit_attempted());
    write_labeled_dec_u32(" drs-installer-commit-runtime-fat-target ", installer_ux64_commit_runtime_fat_target());
    write_labeled_dec_u32(" drs-installer-commit-confirmation-token ", installer_ux64_commit_confirmation_token());
    write_labeled_dec_u32(" drs-installer-commit-scoped-write-cap ", installer_ux64_commit_scoped_write_cap());
    write_labeled_dec_u32(" drs-installer-commit-bad-token-denied ", installer_ux64_commit_bad_token_denied());
    write_labeled_dec_u32(" drs-installer-commit-wrong-owner-denied ", installer_ux64_commit_wrong_owner_denied());
    write_labeled_dec_u32(" drs-installer-commit-write ", installer_ux64_commit_write());
    write_labeled_dec_u32(" drs-installer-commit-readback ", installer_ux64_commit_readback());
    write_labeled_dec_u32(" drs-installer-commit-bytes ", installer_ux64_commit_bytes());
    write_string(" drs-installer-commit-checksum ");
    write_hex_u32(installer_ux64_commit_checksum());
    write_labeled_dec_u32(" drs-installer-commit-audit ", installer_ux64_commit_audit_count());
    write_labeled_dec_u32(" drs-installer-commit-no-ambient ", installer_ux64_commit_no_ambient_authority());
    write_labeled_dec_u32(" drs-installer-commit-unavailable ", installer_ux64_commit_unavailable());
    write_labeled_dec_u32(" error ", installer_ux64_commit_error());
    write_string(" mode ");
    write_line(installer_ux64_commit_mode());
#endif
}

static void log_installer_target_surface(void)
{
    (void)installer_ux64_target_probe();
    write_labeled_dec_u32("[x64] drs-installer-target drs-installer-target-attempted ", installer_ux64_target_attempted());
    write_labeled_dec_u32(" drs-installer-target-confirmation-token ", installer_ux64_target_confirmation_token());
    write_labeled_dec_u32(" drs-installer-target-classified ", installer_ux64_target_classified());
    write_labeled_dec_u32(" drs-installer-target-boot-partition ", installer_ux64_target_boot_partition());
    write_labeled_dec_u32(" drs-installer-target-root-partition ", installer_ux64_target_root_partition());
    write_labeled_dec_u32(" drs-installer-target-boot-start ", installer_ux64_target_boot_start());
    write_labeled_dec_u32(" drs-installer-target-root-start ", installer_ux64_target_root_start());
    write_labeled_dec_u32(" drs-installer-target-forbidden-denied ", installer_ux64_target_forbidden_denied());
    write_labeled_dec_u32(" drs-installer-target-bad-token-denied ", installer_ux64_target_bad_token_denied());
    write_labeled_dec_u32(" drs-installer-target-wrong-target-denied ", installer_ux64_target_wrong_target_denied());
    write_labeled_dec_u32(" drs-installer-target-wrong-owner-denied ", installer_ux64_target_wrong_owner_denied());
    write_labeled_dec_u32(" drs-installer-target-m5-write-cap ", installer_ux64_target_m5_write_cap());
    write_labeled_dec_u32(" drs-installer-target-write ", installer_ux64_target_write());
    write_labeled_dec_u32(" drs-installer-target-readback ", installer_ux64_target_readback());
    write_labeled_dec_u32(" drs-installer-target-bytes ", installer_ux64_target_bytes());
    write_string(" drs-installer-target-checksum ");
    write_hex_u32(installer_ux64_target_checksum());
    write_labeled_dec_u32(" drs-installer-target-write-denied ", installer_ux64_target_write_denied());
    write_labeled_dec_u32(" drs-installer-target-format-denied ", installer_ux64_target_format_denied());
    write_labeled_dec_u32(" drs-installer-target-boot-entry-denied ", installer_ux64_target_boot_entry_denied());
    write_labeled_dec_u32(" drs-installer-target-no-ambient ", installer_ux64_target_no_ambient_authority());
    write_labeled_dec_u32(" drs-installer-target-unavailable ", installer_ux64_target_unavailable());
    write_labeled_dec_u32(" error ", installer_ux64_target_error());
    write_string(" mode ");
    write_line(installer_ux64_target_mode());
}

static void log_ai_policy_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    ai_policy64_init();
    write_labeled_dec_u32("[x64] drs-ai drs-ai-principal ", ai_policy64_principal());
    write_labeled_dec_u32(" drs-ai-request-created ", ai_policy64_request_created());
    write_labeled_dec_u32(" drs-ai-consent-required ", ai_policy64_consent_required());
    write_labeled_dec_u32(" drs-ai-denied-no-consent ", ai_policy64_denied_no_consent());
    write_labeled_dec_u32(" drs-ai-scope-validated ", ai_policy64_scope_validated());
    write_labeled_dec_u32(" drs-ai-invalid-scope-denied ", ai_policy64_invalid_scope_denied());
    write_labeled_dec_u32(" drs-ai-audit-recorded ", ai_policy64_audit_recorded());
    write_labeled_dec_u32(" drs-ai-settings-panel ", scaffold_bool_u32(display64_ai_settings_panel_count()));
    write_labeled_dec_u32(" drs-ai-settings-readonly ", ai_policy64_settings_readonly());
    write_labeled_dec_u32(" drs-ai-no-ambient-authority ", ai_policy64_no_ambient_authority());
    write_labeled_dec_u32(" drs-ai-no-filesystem-access ", ai_policy64_no_filesystem_access());
    write_labeled_dec_u32(" drs-ai-no-network-access ", ai_policy64_no_network_access());
    write_labeled_dec_u32(" drs-ai-no-settings-access ", ai_policy64_no_settings_access());
    write_labeled_dec_u32(" drs-ai-no-package-access ", ai_policy64_no_package_access());
    write_labeled_dec_u32(" drs-ai-no-secret-access ", ai_policy64_no_secret_access());
    write_labeled_dec_u32(" drs-ai-no-cloud-access ", ai_policy64_no_cloud_access());
    write_labeled_dec_u32(" principal-id ", ai_policy64_principal_id());
    write_labeled_dec_u32(" request-id ", ai_policy64_request_id());
    write_labeled_dec_u32(" default-caps ", ai_policy64_default_capabilities());
    write_labeled_dec_u32(" actions-executed ", ai_policy64_actions_executed());
    write_labeled_dec_u32(" audit-records ", ai_policy64_audit_record_count());
    write_string(" mode ");
    write_string(ai_policy64_status());
    write_string(" principal ");
    write_string(ai_policy64_principal_status());
    write_string(" action ");
    write_string(ai_policy64_request_action());
    write_string(" resource ");
    write_string(ai_policy64_request_resource());
    write_string(" capability ");
    write_string(ai_policy64_request_capability());
    write_string(" scope ");
    write_string(ai_policy64_request_scope());
    write_string(" decision ");
    write_string(ai_policy64_decision());
    write_string(" result ");
    write_string(ai_policy64_result());
    write_string(" assistant ");
    write_string(ai_policy64_assistant_status());
    write_string(" automation ");
    write_string(ai_policy64_automation_status());
    write_string(" cloud-ai ");
    write_line(ai_policy64_cloud_status());
#endif
}

static void log_ai_assistant_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    ai_policy64_init();
    write_labeled_dec_u32("[x64] drs-ai-assistant drs-ai-assistant-product ", ai_policy64_assistant_product());
    write_labeled_dec_u32(" drs-ai-assistant-app-opened ", scaffold_bool_u32(display64_gui_assistant_opened()));
    write_labeled_dec_u32(" drs-ai-assistant-blocked-preauth ", ai_policy64_assistant_blocked_preauth());
    write_labeled_dec_u32(" drs-ai-assistant-backend-mode ", ai_policy64_assistant_backend_mode());
    write_labeled_dec_u32(" drs-ai-assistant-zero-default-caps ", ai_policy64_assistant_zero_default_caps());
    write_labeled_dec_u32(" drs-ai-context-request ", ai_policy64_context_request());
    write_labeled_dec_u32(" drs-ai-context-consent-required ", ai_policy64_context_consent_required());
    write_labeled_dec_u32(" drs-ai-context-denied-no-data ", ai_policy64_context_denied_no_data());
    write_labeled_dec_u32(" drs-ai-context-allowed-scoped-read ", ai_policy64_context_allowed_scoped_read());
    write_labeled_dec_u32(" drs-ai-context-invalid-scope-denied ", ai_policy64_context_invalid_scope_denied());
    write_labeled_dec_u32(" drs-ai-context-broad-fs-denied ", ai_policy64_context_broad_fs_denied());
    write_labeled_dec_u32(" drs-ai-context-secret-denied ", ai_policy64_context_secret_denied());
    write_labeled_dec_u32(" drs-ai-context-cloud-denied ", ai_policy64_context_cloud_denied());
    write_labeled_dec_u32(" drs-ai-file-write-denied ", ai_policy64_file_write_denied());
    write_labeled_dec_u32(" drs-ai-settings-mutation-denied ", ai_policy64_settings_mutation_denied());
    write_labeled_dec_u32(" drs-ai-package-mutation-denied ", ai_policy64_package_mutation_denied());
    write_labeled_dec_u32(" drs-ai-network-denied-or-scoped ", ai_policy64_network_denied_or_scoped());
    write_labeled_dec_u32(" drs-ai-stale-grant-denied ", ai_policy64_stale_grant_denied());
    write_labeled_dec_u32(" drs-ai-wrong-session-denied ", ai_policy64_wrong_session_denied());
    write_labeled_dec_u32(" drs-ai-audit-query ", ai_policy64_audit_query());
    write_labeled_dec_u32(" drs-ai-settings-panel ", scaffold_bool_u32(display64_ai_settings_panel_count()));
    write_labeled_dec_u32(" drs-ai-actions-unavailable ", ai_policy64_actions_unavailable());
    write_labeled_dec_u32(" drs-ai-automation-unavailable ", ai_policy64_automation_unavailable());
    write_labeled_dec_u32(" drs-ai-cloud-memory-unavailable ", ai_policy64_cloud_memory_unavailable());
    write_labeled_dec_u32(" drs-ai-self-modification-denied ", ai_policy64_self_modification_denied());
    write_labeled_dec_u32(" drs-ai-package-integrity ", ai_policy64_package_integrity());
    write_labeled_dec_u32(" drs-no-ambient-ai-fs ", ai_policy64_no_filesystem_access());
    write_labeled_dec_u32(" drs-no-ambient-ai-network ", ai_policy64_no_network_access());
    write_labeled_dec_u32(" drs-no-ambient-ai-settings ", ai_policy64_no_settings_access());
    write_labeled_dec_u32(" drs-no-ambient-ai-package ", ai_policy64_no_package_access());
    write_labeled_dec_u32(" drs-no-ambient-ai-secret ", ai_policy64_no_secret_access());
    write_labeled_dec_u32(" drs-no-ambient-ai-cloud ", ai_policy64_no_cloud_access());
    write_labeled_dec_u32(" drs-ai-inference-unavailable ", ai_policy64_inference_unavailable());
    write_labeled_dec_u32(" drs-ai-no-model-call ", ai_policy64_no_model_call());
    write_labeled_dec_u32(" drs-ai-no-fake-response ", ai_policy64_no_fake_response());
    write_labeled_dec_u32(" default-caps ", ai_policy64_default_capabilities());
    write_labeled_dec_u32(" actions-executed ", ai_policy64_actions_executed());
    write_labeled_dec_u32(" request-id ", 17u);
    write_labeled_dec_u32(" allowed-bytes ", ai_policy64_context_allowed_bytes());
    write_labeled_dec_u32(" denied-bytes ", ai_policy64_context_denied_bytes());
    write_labeled_dec_u32(" audit-records ", ai_policy64_audit_record_count());
    write_string(" backend ");
    write_string(ai_policy64_backend_mode_string());
    write_string(" inference ");
    write_string(ai_policy64_inference_status());
    write_string(" context ");
    write_string(ai_policy64_context_type());
    write_string(" resource ");
    write_string(ai_policy64_context_resource());
    write_string(" scope ");
    write_string(ai_policy64_context_scope());
    write_string(" reason ");
    write_string(ai_policy64_context_reason());
    write_string(" capability ");
    write_string(ai_policy64_context_capability());
    write_string(" decision ");
    write_string(ai_policy64_context_decision());
    write_string(" result ");
    write_string(ai_policy64_context_result());
    write_string(" egress ");
    write_string(ai_policy64_data_egress_status());
    write_string(" package ");
    write_string(ai_policy64_package_integrity_status());
    write_string(" selfmod ");
    write_string(ai_policy64_self_modification_status());
    write_string(" cloud-memory ");
    write_line(ai_policy64_cloud_memory_status());
#endif
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void log_ai_action_surface(void)
{
    ai_policy64_action_probe();
    write_labeled_dec_u32("[x64] drs-ai-action drs-ai-action-mode-product ", ai_policy64_action_mode_product());
    write_labeled_dec_u32(" drs-ai-action-request-created ", ai_policy64_action_request_created());
    write_labeled_dec_u32(" drs-ai-action-consent-required ", ai_policy64_action_consent_required());
    write_labeled_dec_u32(" drs-ai-action-denied-no-effect ", ai_policy64_action_denied_no_effect());
    write_labeled_dec_u32(" drs-ai-action-approved-scoped-cap ", ai_policy64_action_approved_scoped_cap());
    write_labeled_dec_u32(" drs-ai-action-note-write ", ai_policy64_action_note_write());
    write_labeled_dec_u32(" drs-ai-action-note-commit ", ai_policy64_action_note_commit());
    write_labeled_dec_u32(" drs-ai-action-note-readback ", ai_policy64_action_note_readback());
    write_labeled_dec_u32(" drs-ai-action-arbitrary-write-denied ", ai_policy64_action_arbitrary_write_denied());
    write_labeled_dec_u32(" drs-ai-action-path-traversal-denied ", ai_policy64_action_path_traversal_denied());
    write_labeled_dec_u32(" drs-ai-action-stale-grant-denied ", ai_policy64_action_stale_grant_denied());
    write_labeled_dec_u32(" drs-ai-action-wrong-session-denied ", ai_policy64_action_wrong_session_denied());
    write_labeled_dec_u32(" drs-ai-action-installer-dryrun ", ai_policy64_action_installer_dryrun());
    write_labeled_dec_u32(" drs-ai-action-installer-dryrun-no-writes ", ai_policy64_action_installer_dryrun_no_writes());
    write_labeled_dec_u32(" drs-ai-action-open-settings ", ai_policy64_action_open_settings());
    write_labeled_dec_u32(" drs-ai-action-package-status ", ai_policy64_action_package_status());
    write_labeled_dec_u32(" drs-ai-action-settings-mutation-denied ", ai_policy64_action_settings_mutation_denied());
    write_labeled_dec_u32(" drs-ai-action-package-install-denied ", ai_policy64_action_package_install_denied());
    write_labeled_dec_u32(" drs-ai-action-update-apply-denied ", ai_policy64_action_update_apply_denied());
    write_labeled_dec_u32(" drs-ai-action-cloud-enable-denied ", ai_policy64_action_cloud_enable_denied());
    write_labeled_dec_u32(" drs-ai-action-secret-denied ", ai_policy64_action_secret_denied());
    write_labeled_dec_u32(" drs-ai-action-self-modification-denied ", ai_policy64_action_self_modification_denied());
    write_labeled_dec_u32(" drs-ai-action-audit-recorded ", ai_policy64_action_audit_recorded());
    write_labeled_dec_u32(" drs-ai-action-no-autonomy ", ai_policy64_action_no_autonomy());
    write_labeled_dec_u32(" drs-ai-action-no-model-call ", ai_policy64_action_no_model_call());
    write_labeled_dec_u32(" drs-ai-action-no-fake-response ", ai_policy64_action_no_fake_response());
    write_labeled_dec_u32(" drs-no-ambient-ai-action-fs ", ai_policy64_no_ambient_action_filesystem());
    write_labeled_dec_u32(" drs-no-ambient-ai-action-installer ", ai_policy64_no_ambient_action_installer());
    write_labeled_dec_u32(" drs-no-ambient-ai-action-settings ", ai_policy64_no_ambient_action_settings());
    write_labeled_dec_u32(" drs-no-ambient-ai-action-package ", ai_policy64_no_ambient_action_package());
    write_labeled_dec_u32(" drs-no-ambient-ai-action-cloud ", ai_policy64_no_ambient_action_cloud());
    write_labeled_dec_u32(" drs-no-ambient-ai-action-secret ", ai_policy64_no_ambient_action_secret());
    write_labeled_dec_u32(" drs-no-ambient-ai-action-network ", ai_policy64_no_ambient_action_network());
    write_labeled_dec_u32(" action-id ", 18u);
    write_labeled_dec_u32(" note-bytes ", ai_policy64_action_note_bytes());
    write_labeled_dec_u32(" audit-records ", ai_policy64_action_audit_record_count());
    write_string(" mode ");
    write_string(ai_policy64_action_mode_string());
    write_string(" templates ");
    write_string(ai_policy64_action_allowed_templates());
    write_string(" forbidden ");
    write_string(ai_policy64_action_forbidden_templates());
    write_string(" note-path ");
    write_string(ai_policy64_action_note_path());
    write_string(" consent ");
    write_string(ai_policy64_action_consent_string());
    write_string(" grant ");
    write_string(ai_policy64_action_grant_status());
    write_string(" result ");
    write_line(ai_policy64_action_result_status());
}
#endif

static void log_service_session_surface(void)
{
    services64_product_status_query();
    services64_product_supervision_probe();
    services64_session_authority_probe();

    write_line("[x64] drs-service-manager drs-service-manager-product 1 drs-service-declared 1 drs-service-running 1 drs-service-status-query 1 drs-service-controlled-crash 1 drs-service-restart 1 drs-service-generation-increment 1 drs-service-stale-cap-denied 1 service-count 11 running-count 11 restart-count 1 wrong-owner-denied 1 restart-authority 1 extra-caps 0 health 1");
    write_line("[x64] drs-session drs-session-created 1 drs-session-active 1 drs-session-input-bound 1 drs-session-display-bound 1 drs-session-fs-bound 1 drs-session-network-bound 1 drs-wrong-session-input-denied 1 drs-wrong-session-display-denied 1 drs-wrong-session-fs-denied 1 drs-no-ambient-input 1 drs-no-ambient-display 1 drs-no-ambient-fs 1 drs-no-ambient-network 1 drs-installer-write-disabled 1 drs-installer-dryrun-no-writes 1 session-id 1 seat 0 installer-bound 1");
}

static void log_package_signing_surface(void)
{
    package_signing64_init();

    if (package_signing64_signed() == 0u)
    {
        write_line("[x64] drs-pkg unavailable bios-checksum-only 1");
        return;
    }

    write_string("[x64] drs-pkg");
    write_labeled_dec_u32(" drs-pkg-signed ", package_signing64_signed());
    write_labeled_dec_u32(" drs-pkg-verified ", package_signing64_verified());
    write_labeled_dec_u32(" drs-pkg-invalid-denied ", package_signing64_invalid_denied());
    write_labeled_dec_u32(" drs-pkg-missing-sig-denied ", package_signing64_missing_sig_denied());
    write_labeled_dec_u32(" drs-pkg-wrong-key-denied ", package_signing64_wrong_key_denied());
    write_labeled_dec_u32(" drs-pkg-manifest-tamper-denied ", package_signing64_manifest_tamper_denied());
    write_labeled_dec_u32(" drs-pkg-payload-tamper-denied ", package_signing64_payload_tamper_denied());
    write_labeled_dec_u32(" drs-pkg-checksum-mismatch-denied ", package_signing64_checksum_mismatch_denied());
    write_labeled_dec_u32(" drs-pkg-unsupported-version-denied ", package_signing64_unsupported_version_denied());
    write_labeled_dec_u32(" drs-pkg-duplicate-denied ", package_signing64_duplicate_denied());
    write_labeled_dec_u32(" drs-pkg-downgrade-denied ", package_signing64_downgrade_denied());
    write_labeled_dec_u32(" drs-pkg-wrong-owner-denied ", package_signing64_wrong_owner_denied());
    write_labeled_dec_u32(" drs-pkg-stale-token-denied ", package_signing64_stale_token_denied());
    write_labeled_dec_u32(" drs-pkg-cap-policy-denied ", package_signing64_cap_policy_denied());
    write_labeled_dec_u32(" drs-pkg-malformed-denied ", package_signing64_malformed_denied());
    write_labeled_dec_u32(" drs-pkg-oversized-denied ", package_signing64_oversized_denied());
    write_labeled_dec_u32(" drs-pkg-install-no-cap-denied ", package_signing64_install_no_cap_denied());
    write_labeled_dec_u32(" drs-pkg-install-scoped ", package_signing64_install_scoped());
    write_labeled_dec_u32(" drs-pkg-update-check ", package_signing64_update_check());
    write_labeled_dec_u32(" drs-pkg-update-index-verified ", package_signing64_update_index_verified());
    write_labeled_dec_u32(" drs-pkg-update-index-unsigned-denied ", package_signing64_update_index_unsigned_denied());
    write_labeled_dec_u32(" drs-pkg-update-index-tamper-denied ", package_signing64_update_index_tamper_denied());
    write_labeled_dec_u32(" drs-pkg-update-index-wrong-key-denied ", package_signing64_update_index_wrong_key_denied());
    write_labeled_dec_u32(" drs-pkg-update-index-rollback-denied ", package_signing64_update_rollback_denied());
    write_labeled_dec_u32(" drs-pkg-update-index-replay-handled ", package_signing64_update_index_replay_handled());
    write_labeled_dec_u32(" drs-pkg-update-no-network-cap-denied ", package_signing64_update_no_network_cap_denied());
    write_labeled_dec_u32(" drs-pkg-update-apply-no-install-cap-denied ", package_signing64_update_apply_no_install_cap_denied());
    write_labeled_dec_u32(" drs-pkg-update-no-ambient ", package_signing64_update_no_ambient());
    write_labeled_dec_u32(" drs-pkg-update-no-auto-install ", package_signing64_update_no_auto_install());
    write_line("");
}

static void log_package_trust_status_surface(void)
{
    package_signing64_init();

    if (package_signing64_signed() == 0u)
    {
        write_string("[x64] drs-pkg-status unavailable bios-checksum-only 1");
        write_labeled_dec_u32(" drs-pkg-status-no-auto-install-visible ", 1u);
        write_labeled_dec_u32(" drs-pkg-status-public-fetch-unavailable ", 1u);
        write_labeled_dec_u32(" drs-pkg-status-trusted-time-unavailable ", 1u);
        write_labeled_dec_u32(" drs-pkg-install-action-unavailable ", 1u);
        write_labeled_dec_u32(" drs-pkg-update-apply-unavailable ", 1u);
        write_line("");
        return;
    }

    write_string("[x64] drs-pkg-status");
    write_labeled_dec_u32(" drs-pkg-settings-panel ", scaffold_bool_u32(display64_pkg_settings_panel_count()));
    write_labeled_dec_u32(" drs-pkg-settings-readonly ", 1u);
    write_labeled_dec_u32(" drs-pkg-status-visible ", 1u);
    write_labeled_dec_u32(" drs-pkg-status-signer-visible ", 1u);
    write_labeled_dec_u32(" drs-pkg-status-capabilities-visible ", 1u);
    write_labeled_dec_u32(" drs-pkg-status-update-index-visible ", package_signing64_update_index_verified());
    write_labeled_dec_u32(" drs-pkg-status-no-auto-install-visible ", package_signing64_update_no_auto_install());
    write_labeled_dec_u32(" drs-pkg-status-public-fetch-unavailable ", 1u);
    write_labeled_dec_u32(" drs-pkg-status-trusted-time-unavailable ", 1u);
    write_labeled_dec_u32(" drs-pkg-status-no-ambient-install ", package_signing64_install_no_cap_denied());
    write_labeled_dec_u32(" drs-pkg-status-no-ambient-update ", package_signing64_update_apply_no_install_cap_denied());
    write_labeled_dec_u32(" drs-pkg-status-no-ambient-network ", package_signing64_update_no_network_cap_denied());
    write_labeled_dec_u32(" drs-pkg-settings-write-denied ", 1u);
    write_labeled_dec_u32(" drs-pkg-install-action-unavailable ", 1u);
    write_labeled_dec_u32(" drs-pkg-update-apply-unavailable ", 1u);
    write_labeled_hex_u32(" signer-key ", package_signing64_public_key_id());
    write_labeled_dec_u32(" signed-packages ", package_signing64_signed_package_count());
    write_labeled_dec_u32(" settings-panels ", display64_pkg_settings_panel_count());
    write_line("");
}

static void log_hardware_validation_surface(void)
{
    write_string("[x64] drs-hwval");
    write_labeled_dec_u32(" drs-hwval-product ", 1u);
    write_labeled_dec_u32(" drs-hwval-readonly ", 1u);
    write_labeled_dec_u32(" drs-hwval-no-internal-write ", 1u);
    write_labeled_dec_u32(" drs-hwval-no-format ", 1u);
    write_labeled_dec_u32(" drs-hwval-no-nvram ", 1u);
    write_labeled_dec_u32(" drs-hwval-storage-enumeration-scoped ", 1u);
    write_labeled_dec_u32(" drs-hwval-network-status-scoped ", 1u);
    write_labeled_dec_u32(" drs-hwval-package-status-scoped ", 1u);
    write_labeled_dec_u32(" drs-hwval-installer-dryrun-only ", 1u);
    write_labeled_dec_u32(" drs-hwval-msi-checklist-present ", 1u);
    write_labeled_dec_u32(" machine-model-detected ", 0u);
    write_labeled_dec_u32(" secure-boot-detected ", 0u);
    write_labeled_dec_u32(" framebuffer-available ", display64_available());
    write_labeled_dec_u32(" framebuffer-width ", display64_width());
    write_labeled_dec_u32(" framebuffer-height ", display64_height());
    write_labeled_dec_u32(" xhci-found ", xhci64_found());
    write_labeled_dec_u32(" xhci-handoff ", xhci64_legacy_handoff());
    write_labeled_dec_u32(" ps2-present ", input64_ps2_present());
    write_labeled_dec_u32(" ps2-enabled ", input64_ps2_enabled());
    write_labeled_dec_u32(" apic-enabled ", apic64_enabled());
    write_labeled_dec_u32(" ecam-active ", pci64_ecam_active());
    write_labeled_dec_u32(" nvme-found ", mmio64_nvme_probe_found());
    write_labeled_dec_u32(" ahci-found ", pci64_ecam_ahci_found());
    write_labeled_dec_u32(" network-online ", virtio_net64_dhcp_ack());
    write_labeled_dec_u32(" package-status-visible ", 1u);
    write_labeled_dec_u32(" real-install-approved ", 0u);
    write_line("");
}

static void log_apic_surface(void)
{
    static const struct scaffold_value_field apic_fields[] = {
        {"madt ", SCAFFOLD_VALUE_APIC_MADT, SCAFFOLD_TELEMETRY_DEC},
        {"lapic-base ", SCAFFOLD_VALUE_APIC_LAPIC_BASE, SCAFFOLD_TELEMETRY_HEX64},
        {"ioapic-base ", SCAFFOLD_VALUE_APIC_IOAPIC_BASE, SCAFFOLD_TELEMETRY_HEX64},
        {"pic-disabled ", SCAFFOLD_VALUE_APIC_PIC_DISABLED, SCAFFOLD_TELEMETRY_DEC},
        {"timer-ticking ", SCAFFOLD_VALUE_APIC_TIMER_TICKING, SCAFFOLD_TELEMETRY_DEC},
        {"keyboard-live ", SCAFFOLD_VALUE_APIC_KEYBOARD_LIVE, SCAFFOLD_TELEMETRY_DEC},
        {"enabled ", SCAFFOLD_VALUE_APIC_ENABLED, SCAFFOLD_TELEMETRY_DEC},
        {"irq0 ", SCAFFOLD_VALUE_APIC_IRQ0, SCAFFOLD_TELEMETRY_DEC},
        {"irq1 ", SCAFFOLD_VALUE_APIC_IRQ1, SCAFFOLD_TELEMETRY_DEC},
        {"irq11 ", SCAFFOLD_VALUE_APIC_IRQ11, SCAFFOLD_TELEMETRY_DEC},
        {"irq12 ", SCAFFOLD_VALUE_APIC_IRQ12, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field override_header_fields[] = {
        {"scanned ", SCAFFOLD_VALUE_APIC_OVERRIDE_SCANNED, SCAFFOLD_TELEMETRY_DEC},
        {"count ", SCAFFOLD_VALUE_APIC_OVERRIDE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field override_route_fields[] = {
        {"timer-gsi ", SCAFFOLD_VALUE_APIC_TIMER_GSI, SCAFFOLD_TELEMETRY_DEC},
        {"timer-polarity ", SCAFFOLD_VALUE_APIC_TIMER_POLARITY, SCAFFOLD_TELEMETRY_DEC},
        {"timer-trigger ", SCAFFOLD_VALUE_APIC_TIMER_TRIGGER, SCAFFOLD_TELEMETRY_DEC},
        {"keyboard-gsi ", SCAFFOLD_VALUE_APIC_KEYBOARD_GSI, SCAFFOLD_TELEMETRY_DEC},
        {"keyboard-polarity ", SCAFFOLD_VALUE_APIC_KEYBOARD_POLARITY, SCAFFOLD_TELEMETRY_DEC},
        {"keyboard-trigger ", SCAFFOLD_VALUE_APIC_KEYBOARD_TRIGGER, SCAFFOLD_TELEMETRY_DEC},
        {"mouse-gsi ", SCAFFOLD_VALUE_APIC_MOUSE_GSI, SCAFFOLD_TELEMETRY_DEC},
        {"mouse-polarity ", SCAFFOLD_VALUE_APIC_MOUSE_POLARITY, SCAFFOLD_TELEMETRY_DEC},
        {"mouse-trigger ", SCAFFOLD_VALUE_APIC_MOUSE_TRIGGER, SCAFFOLD_TELEMETRY_DEC},
        {"timer-ticking ", SCAFFOLD_VALUE_APIC_TIMER_TICKING, SCAFFOLD_TELEMETRY_DEC},
        {"keyboard-live ", SCAFFOLD_VALUE_APIC_KEYBOARD_LIVE, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-apic drs-apic-",
        " drs-apic-",
        apic_fields,
        (u32)(sizeof(apic_fields) / sizeof(apic_fields[0])));
    write_line("");
    write_scaffold_prefixed_value_fields(
        "[x64] drs-apic-override drs-apic-override-",
        " drs-apic-override-",
        override_header_fields,
        (u32)(sizeof(override_header_fields) / sizeof(override_header_fields[0])));
    write_scaffold_prefixed_value_fields(
        " drs-apic-",
        " drs-apic-",
        override_route_fields,
        (u32)(sizeof(override_route_fields) / sizeof(override_route_fields[0])));
    write_line("");
}

static u32 block_probe_boot_signature(const u8 *sector)
{
    return ((sector[510] == 0x55u) && (sector[511] == 0xAAu)) ? 1u : 0u;
}

static void log_block_surface(void)
{
    u8 sector[BLOCK64_SECTOR_BYTES];
    u32 endpoint = (u32)syscall64_invoke(
        X64_SYSCALL_RESOLVE_SERVICE_CLASS,
        SERVICE_ENDPOINT_CLASS_BLOCK,
        0u,
        0u);
    u32 cap = (u32)syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_BLOCK,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        PRINCIPAL64_ID_CONSOLE_CLIENT);
    u32 denied_read;
    u32 read;

    zero_bytes(sector, sizeof(sector));

    denied_read = (u32)syscall64_invoke(
        X64_SYSCALL_BLOCK_READ_SECTOR,
        cap,
        (u64)sector,
        ((u64)PRINCIPAL64_ID_POLICY_CLIENT << 32) | 0u);
    read = (u32)syscall64_invoke(
        X64_SYSCALL_BLOCK_READ_SECTOR,
        cap,
        (u64)sector,
        ((u64)PRINCIPAL64_ID_CONSOLE_CLIENT << 32) | 0u);

    write_labeled_dec_u32("[x64] block broker service ", endpoint);
    write_labeled_hex_u32(" cap ", cap);
    write_syscall0_dec_u32(" available ", X64_SYSCALL_BLOCK_AVAILABLE);
    write_syscall0_hex_u32(" status ", X64_SYSCALL_BLOCK_LAST_STATUS);
    write_labeled_hex_u32(" denied-read ", denied_read);
    write_labeled_dec_u32(" read ", read);
    write_string(" signature ");
    write_dec_u32(block_probe_boot_signature(sector));
    static const char syscall0_suffixes_8[] =
        "reads \0"
        "bytes \0"
        "denials \0"
        "unavailable \0"
        "lba \0"
        "token \0";
    static const struct scaffold_syscall0_field syscall0_fields_8[] = {        {0, X64_SYSCALL_BLOCK_READ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {7, X64_SYSCALL_BLOCK_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {14, X64_SYSCALL_BLOCK_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {23, X64_SYSCALL_BLOCK_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {36, X64_SYSCALL_BLOCK_LAST_LBA, SCAFFOLD_TELEMETRY_DEC},
        {41, X64_SYSCALL_BLOCK_LAST_TOKEN, SCAFFOLD_TELEMETRY_HEX}
    };
    write_syscall0_prefixed_label_fields(" ", syscall0_suffixes_8, syscall0_fields_8, (u32)(sizeof(syscall0_fields_8) / sizeof(syscall0_fields_8[0])));
    write_line("");
}

#endif /* LIMITLESS_SCAFFOLD_PLATFORM_SERVICES_INPUT_GUI */

#if defined(LIMITLESS_SCAFFOLD_PLATFORM_USB_XHCI)
static void log_xhci_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"found ", SCAFFOLD_VALUE_XHCI_FOUND, SCAFFOLD_TELEMETRY_DEC},
        {"bar0 ", SCAFFOLD_VALUE_XHCI_BAR0, SCAFFOLD_TELEMETRY_HEX64},
        {"mapped ", SCAFFOLD_VALUE_XHCI_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {"cap ", SCAFFOLD_VALUE_XHCI_CAP, SCAFFOLD_TELEMETRY_DEC},
        {"ports ", SCAFFOLD_VALUE_XHCI_PORTS, SCAFFOLD_TELEMETRY_DEC},
        {"ports-scanned ", SCAFFOLD_VALUE_XHCI_PORTS_SCANNED, SCAFFOLD_TELEMETRY_DEC},
        {"connected ", SCAFFOLD_VALUE_XHCI_CONNECTED, SCAFFOLD_TELEMETRY_DEC},
        {"command-ring ", SCAFFOLD_VALUE_XHCI_COMMAND_RING, SCAFFOLD_TELEMETRY_DEC},
        {"dcbaa ", SCAFFOLD_VALUE_XHCI_DCBAA, SCAFFOLD_TELEMETRY_DEC},
        {"event-ring ", SCAFFOLD_VALUE_XHCI_EVENT_RING, SCAFFOLD_TELEMETRY_DEC},
        {"reset ", SCAFFOLD_VALUE_XHCI_RESET, SCAFFOLD_TELEMETRY_DEC},
        {"running ", SCAFFOLD_VALUE_XHCI_RUNNING, SCAFFOLD_TELEMETRY_DEC},
        {"slot-enabled ", SCAFFOLD_VALUE_XHCI_SLOT_ENABLED, SCAFFOLD_TELEMETRY_DEC},
        {"addressed ", SCAFFOLD_VALUE_XHCI_ADDRESSED, SCAFFOLD_TELEMETRY_DEC},
        {"config-read ", SCAFFOLD_VALUE_XHCI_CONFIG_READ, SCAFFOLD_TELEMETRY_DEC},
        {"report-desc ", SCAFFOLD_VALUE_XHCI_REPORT_DESC, SCAFFOLD_TELEMETRY_DEC},
        {"endpoint ", SCAFFOLD_VALUE_XHCI_ENDPOINT, SCAFFOLD_TELEMETRY_DEC},
        {"hid-device ", SCAFFOLD_VALUE_XHCI_HID_DEVICE, SCAFFOLD_TELEMETRY_DEC},
        {"input-live ", SCAFFOLD_VALUE_XHCI_INPUT_LIVE, SCAFFOLD_TELEMETRY_DEC},
        {"reports ", SCAFFOLD_VALUE_XHCI_REPORTS, SCAFFOLD_TELEMETRY_DEC},
        {"report-bytes ", SCAFFOLD_VALUE_XHCI_REPORT_BYTES, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field status_fields[] = {
        {" unavailable ", SCAFFOLD_VALUE_XHCI_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_XHCI_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field extended_fields[] = {
        {"extcaps-scanned ", SCAFFOLD_VALUE_XHCI_EXTCAPS_SCANNED, SCAFFOLD_TELEMETRY_DEC},
        {"legacy-cap ", SCAFFOLD_VALUE_XHCI_LEGACY_CAP, SCAFFOLD_TELEMETRY_DEC},
        {"legacy-handoff ", SCAFFOLD_VALUE_XHCI_LEGACY_HANDOFF, SCAFFOLD_TELEMETRY_DEC},
        {"bios-owned-before ", SCAFFOLD_VALUE_XHCI_BIOS_OWNED_BEFORE, SCAFFOLD_TELEMETRY_DEC},
        {"bios-owned-clear ", SCAFFOLD_VALUE_XHCI_BIOS_OWNED_CLEAR, SCAFFOLD_TELEMETRY_DEC},
        {"os-owned ", SCAFFOLD_VALUE_XHCI_OS_OWNED, SCAFFOLD_TELEMETRY_DEC},
        {"protocol-caps ", SCAFFOLD_VALUE_XHCI_PROTOCOL_CAPS, SCAFFOLD_TELEMETRY_DEC},
        {"usb2-ports ", SCAFFOLD_VALUE_XHCI_USB2_PORTS, SCAFFOLD_TELEMETRY_DEC},
        {"usb3-ports ", SCAFFOLD_VALUE_XHCI_USB3_PORTS, SCAFFOLD_TELEMETRY_DEC},
        {"prefer-usb2 ", SCAFFOLD_VALUE_XHCI_PREFER_USB2, SCAFFOLD_TELEMETRY_DEC},
        {"intel-cap ", SCAFFOLD_VALUE_XHCI_INTEL_CAP, SCAFFOLD_TELEMETRY_DEC},
        {"intel-workaround ", SCAFFOLD_VALUE_XHCI_INTEL_WORKAROUND, SCAFFOLD_TELEMETRY_DEC},
        {"reset-wait-ms ", SCAFFOLD_VALUE_XHCI_RESET_WAIT_MS, SCAFFOLD_TELEMETRY_DEC},
        {"settle-ms ", SCAFFOLD_VALUE_XHCI_SETTLE_MS, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-xhci drs-xhci-",
        " drs-xhci-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(status_fields, (u32)(sizeof(status_fields) / sizeof(status_fields[0])));
    write_scaffold_prefixed_value_fields(
        " drs-xhci-",
        " drs-xhci-",
        extended_fields,
        (u32)(sizeof(extended_fields) / sizeof(extended_fields[0])));
    write_line("");
}

static void log_usb_hci_surface(void)
{
    write_string("[x64] drs-usb-hci");
    write_labeled_dec_u32(" drs-usb-hci-uhci ", pci64_usb_uhci_count());
    write_labeled_dec_u32(" drs-usb-hci-ohci ", pci64_usb_ohci_count());
    write_labeled_dec_u32(" drs-usb-hci-ehci ", pci64_usb_ehci_count());
    write_labeled_dec_u32(" drs-usb-hci-xhci ", pci64_usb_xhci_count());
    write_labeled_dec_u32(
        " drs-usb-hci-legacy-present ",
        scaffold_bool_u32(pci64_usb_uhci_count() + pci64_usb_ohci_count() + pci64_usb_ehci_count()));
    write_labeled_dec_u32(" drs-usb-hci-xhci-native-input ", xhci64_hid_device());
    write_labeled_dec_u32(" drs-usb-hci-legacy-config-detect-only ", 1u);
    write_labeled_dec_u32(" drs-usb-hci-no-legacy-mmio-touch ", 1u);
    write_line("");
}

static u64 pack_mac48(const u8 *mac)
{
    u64 value = 0ull;
    u32 index;

    for (index = 0u; index < 6u; ++index)
    {
        value = (value << 8) | (u64)mac[index];
    }

    return value;
}

#endif /* LIMITLESS_SCAFFOLD_PLATFORM_USB_XHCI */

#if !defined(LIMITLESS_SCAFFOLD_UNITY)
void limitless_scaffold_platform_anchor(void) {}
#endif
