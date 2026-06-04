/* Split scaffold fragment. Real code is unity-included by scaffold.c; direct compilation emits only the anchor below. */

#if defined(LIMITLESS_SCAFFOLD_SCHED_LAUNCH_AND_TASKS)
static void log_launch_namespace(void)
{
    write_string("[x64] launch archive valid ");
    write_dec_u32(launch64_archive_valid());
    write_string(" checksum ");
    write_hex_u32(launch64_archive_checksum());
    write_string(" total ");
    write_dec_u32(launch64_manifest_total_count());
    write_string(" service-manifests ");
    write_dec_u32(launch64_manifest_count());
    write_string(" ignored ");
    write_dec_u32(launch64_manifest_ignored_count());
    write_string(" denied ");
    write_dec_u32(launch64_manifest_denial_count());
    write_string(" ready ");
    write_dec_u32(launch64_service_ready_count());
    write_string(" started ");
    write_dec_u32(launch64_service_started_count());
    write_string(" drained ");
    write_dec_u32(launch64_service_drained_count());
    write_string(" quiesce-ready ");
    write_dec_u32(launch64_service_quiesce_ready_count());
    write_string(" start-denials ");
    write_dec_u32(launch64_service_start_denial_count());
    write_string(" requests ");
    write_dec_u32(launch64_service_start_request_count());
    write_string(" approvals ");
    write_dec_u32(launch64_service_start_approval_count());
    write_string(" pending ");
    write_dec_u32(launch64_service_start_pending_count());
    write_string(" request-denied ");
    write_dec_u32(launch64_service_start_denied_count());
    write_string(" completed ");
    write_dec_u32(launch64_service_start_completed_count());
    write_string(" quiesce-requests ");
    write_dec_u32(launch64_service_quiesce_request_count());
    write_string(" quiesce-approvals ");
    write_dec_u32(launch64_service_quiesce_approval_count());
    write_string(" quiesce-pending ");
    write_dec_u32(launch64_service_quiesce_pending_count());
    write_string(" quiesce-denied ");
    write_dec_u32(launch64_service_quiesce_denied_count());
    write_string(" quiesce-completed ");
    write_dec_u32(launch64_service_quiesce_completed_count());
    write_string(" drain-requests ");
    write_dec_u32(launch64_service_drain_request_count());
    write_string(" drain-approvals ");
    write_dec_u32(launch64_service_drain_approval_count());
    write_string(" drain-pending ");
    write_dec_u32(launch64_service_drain_pending_count());
    write_string(" drain-denied ");
    write_dec_u32(launch64_service_drain_denied_count());
    write_string(" drain-completed ");
    write_dec_u32(launch64_service_drain_completed_count());
    write_string(" restart-requests ");
    write_dec_u32(launch64_service_restart_request_count());
    write_string(" restart-approvals ");
    write_dec_u32(launch64_service_restart_approval_count());
    write_string(" restart-pending ");
    write_dec_u32(launch64_service_restart_pending_count());
    write_string(" restart-denied ");
    write_dec_u32(launch64_service_restart_denied_count());
    write_string(" restart-completed ");
    write_dec_u32(launch64_service_restart_completed_count());
    write_string(" stop-requests ");
    write_dec_u32(launch64_service_stop_request_count());
    write_string(" stop-approvals ");
    write_dec_u32(launch64_service_stop_approval_count());
    write_string(" stop-pending ");
    write_dec_u32(launch64_service_stop_pending_count());
    write_string(" stop-denied ");
    write_dec_u32(launch64_service_stop_denied_count());
    write_string(" stop-completed ");
    write_dec_u32(launch64_service_stop_completed_count());
    write_string(" log ");
    write_dec_u32(launch64_request_log_count());
    write_string(" init-auth ");
    write_dec_u32(launch64_requester_can_start(PRINCIPAL64_ID_INIT_SUPERVISOR));
    write_string(" policy-auth ");
    write_dec_u32(launch64_requester_can_start(PRINCIPAL64_ID_POLICY_WORKER));
    write_string(" quiesce-init-auth ");
    write_dec_u32(launch64_requester_can_quiesce(PRINCIPAL64_ID_INIT_SUPERVISOR));
    write_string(" quiesce-policy-auth ");
    write_dec_u32(launch64_requester_can_quiesce(PRINCIPAL64_ID_POLICY_WORKER));
    write_string(" drain-init-auth ");
    write_dec_u32(launch64_requester_can_drain(PRINCIPAL64_ID_INIT_SUPERVISOR));
    write_string(" drain-policy-auth ");
    write_dec_u32(launch64_requester_can_drain(PRINCIPAL64_ID_POLICY_WORKER));
    write_string(" restart-init-auth ");
    write_dec_u32(launch64_requester_can_restart(PRINCIPAL64_ID_INIT_SUPERVISOR));
    write_string(" restart-policy-auth ");
    write_dec_u32(launch64_requester_can_restart(PRINCIPAL64_ID_POLICY_WORKER));
    write_string(" stop-init-auth ");
    write_dec_u32(launch64_requester_can_stop(PRINCIPAL64_ID_INIT_SUPERVISOR));
    write_string(" stop-policy-auth ");
    write_dec_u32(launch64_requester_can_stop(PRINCIPAL64_ID_POLICY_WORKER));
    write_line("");
}

static void log_identity_map(const struct boot_info *boot_info)
{
    write_string("[x64] identity map ");
    write_dec_u32(boot_info->identity_map_bytes / (1024u * 1024u));
    write_line(" MiB");
}

static int framebuffer_format_is_supported(u32 format)
{
    return (format == LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_RGB ||
            format == LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_BGR) ? 1 : 0;
}

static int boot_info_has_framebuffer(const struct boot_info *boot_info)
{
    return (boot_info != 0 &&
            (boot_info->bootstrap_flags & LIMITLESS_BOOT_FLAG_FRAMEBUFFER) != 0u &&
            boot_info->framebuffer_base != 0ull &&
            boot_info->framebuffer_bytes >= 4ull &&
            boot_info->framebuffer_width != 0u &&
            boot_info->framebuffer_height != 0u &&
            boot_info->framebuffer_pixels_per_scanline >= boot_info->framebuffer_width &&
            framebuffer_format_is_supported(boot_info->framebuffer_format)) ? 1 : 0;
}

static u32 framebuffer_make_pixel(u32 format, u8 red, u8 green, u8 blue)
{
    if (format == LIMITLESS_BOOT_FRAMEBUFFER_FORMAT_RGB)
    {
        return ((u32)blue << 16) | ((u32)green << 8) | (u32)red;
    }

    return ((u32)red << 16) | ((u32)green << 8) | (u32)blue;
}

static u32 draw_framebuffer_kernel_marker(const struct boot_info *boot_info, u32 *drawn_pixels)
{
    volatile u32 *framebuffer;
    u32 width;
    u32 height;
    u32 start_y;
    u32 x;
    u32 y;
    u32 token = 2166136261u;
    u32 drawn = 0u;

    if (drawn_pixels != 0)
    {
        *drawn_pixels = 0u;
    }

    if (!boot_info_has_framebuffer(boot_info))
    {
        return 0u;
    }

    width = boot_info->framebuffer_width;
    height = boot_info->framebuffer_height;
    if (width > 128u)
    {
        width = 128u;
    }
    if (height > 8u)
    {
        height = 8u;
    }
    start_y = (boot_info->framebuffer_height > 48u) ? 40u : 0u;
    if ((start_y + height) > boot_info->framebuffer_height)
    {
        start_y = 0u;
    }

    framebuffer = (volatile u32 *)(u64)boot_info->framebuffer_base;
    for (y = 0u; y < height; ++y)
    {
        for (x = 0u; x < width; ++x)
        {
            u64 offset = ((u64)(start_y + y) * (u64)boot_info->framebuffer_pixels_per_scanline) + (u64)x;
            u32 pixel;

            if (((offset + 1ull) * 4ull) > boot_info->framebuffer_bytes)
            {
                continue;
            }

            pixel = framebuffer_make_pixel(
                boot_info->framebuffer_format,
                (u8)(0xE0u - ((x * 2u) & 0x3Fu)),
                (u8)(0xE8u - ((y * 6u) & 0x3Fu)),
                (u8)(0x40u + ((x + y) & 0x3Fu)));
            framebuffer[offset] = pixel;
            token ^= framebuffer[offset];
            token *= 16777619u;
            ++drawn;
        }
    }

    if (drawn_pixels != 0)
    {
        *drawn_pixels = drawn;
    }

    return (drawn != 0u && token != 0u) ? token : 0u;
}

static void log_framebuffer_handoff(const struct boot_info *boot_info)
{
    u32 drawn_pixels = 0u;
    u32 token = 0u;

    if (boot_info_has_framebuffer(boot_info))
    {
        token = draw_framebuffer_kernel_marker(boot_info, &drawn_pixels);
    }

    write_string("[x64] framebuffer handoff base ");
    write_hex_u64((boot_info != 0) ? boot_info->framebuffer_base : 0ull);
    write_string(" bytes ");
    write_hex_u64((boot_info != 0) ? boot_info->framebuffer_bytes : 0ull);
    write_string(" ");
    write_dec_u32((boot_info != 0) ? boot_info->framebuffer_width : 0u);
    write_string("x");
    write_dec_u32((boot_info != 0) ? boot_info->framebuffer_height : 0u);
    write_string(" ppsl ");
    write_dec_u32((boot_info != 0) ? boot_info->framebuffer_pixels_per_scanline : 0u);
    write_string(" format ");
    write_dec_u32((boot_info != 0) ? boot_info->framebuffer_format : 0u);
    write_string(" firmware-token ");
    write_hex_u32((boot_info != 0) ? boot_info->framebuffer_firmware_token : 0u);
    write_labeled_dec_u32(" kernel-draw-pixels ", drawn_pixels);
    write_labeled_hex_u32(" kernel-token ", token);
    write_string(" status ");
    write_dec_u32((drawn_pixels != 0u && token != 0u) ? 1u : 0u);
    write_line("");
}

static void log_runtime_mapping(void)
{
    write_string("[x64] runtime image map installed ");
    write_dec_u32(paging64_runtime_mapping_installed());
    write_string(" source ");
    write_hex_u64(paging64_runtime_mapping_source_physical());
    write_string(" pages ");
    write_dec_u32(paging64_runtime_mapping_page_count());
    write_string(" checksum ");
    write_hex_u32(paging64_runtime_mapping_source_checksum());
    write_string(" install ");
    write_hex_u32(paging64_runtime_mapping_install_token());
    write_string(" entry-probe ");
    write_hex_u32(paging64_runtime_mapping_entry_probe());
    write_string(" protection ");
    write_hex_u32(paging64_runtime_mapping_protection_flags());
    write_string(" protection-token ");
    write_hex_u32(paging64_runtime_mapping_protection_token());
    write_protection_summary(paging64_runtime_mapping_protection_flags());
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_entry_result());
    write_line("");

    write_string("[x64] runtime user image map installed ");
    write_dec_u32(paging64_user_runtime_mapping_installed());
    write_string(" source ");
    write_hex_u64(paging64_user_runtime_mapping_source_physical());
    write_string(" pages ");
    write_dec_u32(paging64_user_runtime_mapping_page_count());
    write_string(" checksum ");
    write_hex_u32(paging64_user_runtime_mapping_source_checksum());
    write_string(" install ");
    write_hex_u32(paging64_user_runtime_mapping_install_token());
    write_string(" entry-probe ");
    write_hex_u32(paging64_user_runtime_mapping_entry_probe());
    write_string(" protection ");
    write_hex_u32(paging64_user_runtime_mapping_protection_flags());
    write_string(" protection-token ");
    write_hex_u32(paging64_user_runtime_mapping_protection_token());
    write_protection_summary(paging64_user_runtime_mapping_protection_flags());
    write_line("");

    write_string("[x64] runtime user stack map installed ");
    write_dec_u32(paging64_user_stack_mapping_installed());
    write_string(" protection ");
    write_hex_u32(paging64_user_stack_mapping_protection_flags());
    write_string(" protection-token ");
    write_hex_u32(paging64_user_stack_mapping_protection_token());
    write_protection_summary(paging64_user_stack_mapping_protection_flags());
    write_line("");
}

static u64 lower_half_alias_address(const void *mapped_address)
{
    return paging64_kernel_physical_alias(mapped_address);
}

static u64 higher_half_alias_address(const void *mapped_address)
{
    u64 address = (u64)(const void *)mapped_address;

    if (address >= LIMITLESS_X64_KERNEL_VIRTUAL_BASE)
    {
        return address;
    }

    return LIMITLESS_X64_KERNEL_VIRTUAL_BASE + address;
}

static void log_higher_half_alias(void)
{
    const char *name_runtime = g_x64_scaffold_name;
    u64 name_low = lower_half_alias_address((const void *)name_runtime);
    u64 name_alias = higher_half_alias_address((const void *)name_runtime);
    const volatile struct x64_scaffold_report *report_alias =
        (const volatile struct x64_scaffold_report *)(u64)higher_half_alias_address(
            (const void *)&g_x64_scaffold_report);

    write_string("[x64] higher-half base ");
    write_hex_u64(LIMITLESS_X64_KERNEL_VIRTUAL_BASE);
    write_string(" runtime ");
    write_hex_u64((u64)(const void *)name_runtime);
    write_string(" low ");
    write_hex_u64(name_low);
    write_string(" alias ");
    write_hex_u64(name_alias);
    write_line("");

    write_string("[x64] higher-half name ");
    write_line((const char *)(u64)name_alias);

    write_string("[x64] higher-half report magic ");
    write_hex_u64(report_alias->magic);
    write_string(" active ");
    write_hex_u64(report_alias->active_virtual_base);
    write_string(" planned ");
    write_hex_u64(report_alias->planned_virtual_base);
    write_line("");
}

static void log_kernel_load(const struct boot_info *boot_info)
{
    write_string("[x64] kernel load ");
    write_hex_u32(boot_info->kernel_load_address);
    write_string(" sectors ");
    write_dec_u32(boot_info->kernel_sector_count);
    write_line("");
}

static void log_interrupt_probes(void)
{
    write_string("[x64] exception hits ");
    write_dec_u32(interrupts64_exception_count());
    write_string(" breakpoint ");
    write_dec_u32(interrupts64_breakpoint_count());
    write_string(" invalid-op ");
    write_dec_u32(interrupts64_invalid_opcode_count());
    write_string(" page-fault ");
    write_dec_u32(interrupts64_page_fault_count());
    write_line("");
    write_string("[x64] probe hits ");
    write_dec_u32(interrupts64_probe_count());
    write_line("");
    write_string("[x64] irq hits ");
    write_dec_u32(interrupts64_irq_count());
    write_line("");
    write_string("[x64] syscall hits ");
    write_dec_u32(interrupts64_syscall_count());
    write_string(" code ");
    write_hex_u64(interrupts64_last_syscall_code());
    write_line("");
}

static void run_user_entry_transfer_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u32 result = 0u;

    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        result = interrupts64_trigger_user_entry_probe(
            launch64_manifest_runtime_user_entry_rip(policy_manifest),
            launch64_manifest_runtime_user_entry_rsp(policy_manifest),
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            launch64_manifest_runtime_user_entry_rflags(policy_manifest));
    }

    write_string("[x64] user transfer probe attempts ");
    write_dec_u32(interrupts64_user_entry_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_entry_probe_exits());
    write_labeled_hex_u32(" result ", result);
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_syscall_result());
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

static void run_user_entry_preempt_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u32 interruptible_rflags = 0u;
    u32 result = 0u;

    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        interruptible_rflags =
            launch64_manifest_runtime_user_entry_rflags(policy_manifest) | 0x00000200u;
        interrupts64_enable();
        result = interrupts64_trigger_user_preempt_probe(
            LAUNCH64_USER_IMAGE_BASE + runtime64_transfer_user_preempt_entry_offset(),
            launch64_manifest_runtime_user_entry_rsp(policy_manifest),
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            interruptible_rflags);
        interrupts64_disable();
    }

    write_string("[x64] user preempt probe attempts ");
    write_dec_u32(interrupts64_user_preempt_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_preempt_probe_exits());
    write_string(" irqs ");
    write_dec_u32(interrupts64_user_preempt_probe_irqs());
    write_labeled_hex_u32(" result ", result);
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_preempt_result());
    write_string(" rip ");
    write_hex_u64(interrupts64_user_preempt_probe_rip());
    write_string(" rsp ");
    write_hex_u64(interrupts64_user_preempt_probe_rsp());
    write_string(" cs ");
    write_hex_u64(interrupts64_user_preempt_probe_cs());
    write_string(" ss ");
    write_hex_u64(interrupts64_user_preempt_probe_ss());
    write_labeled_hex_u32(" rflags ", interruptible_rflags);
    write_line("");
}

static void run_user_entry_switch_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u32 interruptible_rflags = 0u;
    u64 source_rip = 0u;
    u64 source_rsp = 0u;
    u64 target_rip = 0u;
    u64 target_rsp = 0u;
    u32 result = 0u;
    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        source_rip = LAUNCH64_USER_IMAGE_BASE + runtime64_transfer_user_switch_source_offset();
        source_rsp = launch64_manifest_runtime_user_entry_rsp(policy_manifest);
        target_rip = LAUNCH64_USER_IMAGE_BASE + runtime64_transfer_user_switch_target_offset();
        target_rsp = source_rsp - runtime64_transfer_user_switch_target_stack_delta();
        interruptible_rflags =
            launch64_manifest_runtime_user_entry_rflags(policy_manifest) | 0x00000200u;
        interrupts64_enable();
        result = interrupts64_trigger_user_switch_probe(
            source_rip,
            source_rsp,
            target_rip,
            target_rsp,
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            interruptible_rflags);
        interrupts64_disable();
    }

    write_string("[x64] user switch probe attempts ");
    write_dec_u32(interrupts64_user_switch_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_switch_probe_exits());
    write_string(" irqs ");
    write_dec_u32(interrupts64_user_switch_probe_irqs());
    write_string(" switches ");
    write_dec_u32(interrupts64_user_switch_probe_switches());
    write_labeled_hex_u32(" result ", result);
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_switch_result());
    write_string(" source-rip ");
    write_hex_u64(interrupts64_user_switch_source_rip());
    write_string(" source-rsp ");
    write_hex_u64(interrupts64_user_switch_source_rsp());
    write_string(" target-rip ");
    write_hex_u64(interrupts64_user_switch_target_rip());
    write_string(" target-rsp ");
    write_hex_u64(interrupts64_user_switch_target_rsp());
    write_string(" cs ");
    write_hex_u64(interrupts64_user_switch_probe_cs());
    write_string(" ss ");
    write_hex_u64(interrupts64_user_switch_probe_ss());
    write_labeled_hex_u32(" rflags ", interruptible_rflags);
    write_line("");
}

static void run_user_entry_runqueue_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 console_pid = process64_pid_for_principal(PRINCIPAL64_ID_CONSOLE_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 console_manifest = process64_manifest_index(console_pid);
    u32 policy_user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u32 console_user_entry_state = launch64_manifest_runtime_user_entry_state(console_manifest);
    u32 interruptible_rflags = 0u;
    u64 source_rip = 0u;
    u64 source_rsp = 0u;
    u64 target_rip = 0u;
    u64 target_rsp = 0u;
    u32 result = 0u;
#ifdef LIMITLESS_X64_UEFI_KERNEL
    u32 block_source_task = SCHEDULER64_INVALID_TASK;
    u32 block_target_task = SCHEDULER64_INVALID_TASK;
    u32 block_start = 0u;
    u32 block_result = 0u;
    u32 block_state = 0u;
    u32 block_count = 0u;
    u32 blocked_after_block = 0u;
    u32 block_switch = 0u;
    u32 block_target_state = 0u;
    u32 block_wake = 0u;
    u32 block_wake_state = 0u;
    u32 block_wake_count = 0u;
    u32 block_switch_back = 0u;
    u32 block_final_state = 0u;
    u32 block_denials_before = 0u;
    u32 block_denials_after = 0u;
    u32 block_invalid_deny = 0u;
    u32 block_running_wake_deny = 0u;
    u32 block_positive = 0u;
    struct interrupt_frame64 block_frame = {0};
    u32 sleep_source_task = SCHEDULER64_INVALID_TASK;
    u32 sleep_peer_task = SCHEDULER64_INVALID_TASK;
    u32 sleep_start = 0u;
    u32 sleep_result = 0u;
    u32 sleep_requested = 3u;
    u32 sleep_wake_tick = 0u;
    u32 sleep_elapsed = 0u;
    u32 sleep_last_task = SCHEDULER64_INVALID_TASK;
    u32 sleep_current_before = SCHEDULER64_INVALID_TASK;
    u32 sleep_state_after_block = 0u;
    u32 sleep_pending_before = 0u;
    u32 sleep_pending_after_block = 0u;
    u32 sleep_pending_after_wake = 0u;
    u32 sleep_count_before = 0u;
    u32 sleep_count_after = 0u;
    u32 sleep_wake_before = 0u;
    u32 sleep_wake_after = 0u;
    u32 sleep_sched_block_before = 0u;
    u32 sleep_sched_block_after = 0u;
    u32 sleep_sched_wake_before = 0u;
    u32 sleep_sched_wake_after = 0u;
    u32 sleep_switch_to_peer = 0u;
    u32 sleep_peer_state = 0u;
    u32 sleep_switch_to_source = 0u;
    u32 sleep_current_after_wake = SCHEDULER64_INVALID_TASK;
    u32 sleep_final_state = 0u;
    u32 sleep_deny_task = SCHEDULER64_INVALID_TASK;
    u32 sleep_deny_start = 0u;
    u32 sleep_deny_result = 0u;
    u32 sleep_deny_state = 0u;
    u32 sleep_denial_before = 0u;
    u32 sleep_denial_after = 0u;
    u32 sleep_positive = 0u;
    struct interrupt_frame64 sleep_source_frame = {0};
    struct interrupt_frame64 sleep_peer_frame = {0};
#endif

    if ((policy_pid != PROCESS64_INVALID_PID)
        && (console_pid != PROCESS64_INVALID_PID)
        && (policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && (console_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((policy_user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u)
        && ((console_user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        source_rip = LAUNCH64_USER_IMAGE_BASE + runtime64_transfer_user_runqueue_source_offset();
        source_rsp = launch64_manifest_runtime_user_entry_rsp(policy_manifest);
        target_rip = LAUNCH64_USER_IMAGE_BASE + runtime64_transfer_user_runqueue_target_offset();
        target_rsp = launch64_manifest_runtime_user_entry_rsp(console_manifest)
            - runtime64_transfer_user_runqueue_target_stack_delta();
        interruptible_rflags =
            launch64_manifest_runtime_user_entry_rflags(policy_manifest) | 0x00000200u;
        interrupts64_enable();
        result = interrupts64_trigger_user_runqueue_probe(
            policy_pid,
            source_rip,
            source_rsp,
            console_pid,
            target_rip,
            target_rsp,
            interruptible_rflags);
        interrupts64_disable();
    }

    write_string("[x64] user runqueue probe attempts ");
    write_dec_u32(interrupts64_user_runqueue_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_runqueue_probe_exits());
    write_string(" irqs ");
    write_dec_u32(interrupts64_user_runqueue_probe_irqs());
    write_string(" switches ");
    write_dec_u32(interrupts64_user_runqueue_probe_switches());
    write_labeled_hex_u32(" result ", result);
    write_string(" source-result ");
    write_hex_u32(interrupts64_user_runqueue_source_result());
    write_string(" target-result ");
    write_hex_u32(interrupts64_user_runqueue_target_result());
    write_string(" expected-source ");
    write_hex_u32(runtime64_transfer_user_runqueue_source_result());
    write_string(" expected-target ");
    write_hex_u32(runtime64_transfer_user_runqueue_target_result());
    write_string(" source-pid ");
    write_dec_u32(interrupts64_user_runqueue_source_pid());
    write_string(" target-pid ");
    write_dec_u32(interrupts64_user_runqueue_target_pid());
    write_string(" source-runtime ");
    write_hex_u32(interrupts64_user_runqueue_source_runtime_token());
    write_string(" target-runtime ");
    write_hex_u32(interrupts64_user_runqueue_target_runtime_token());
    write_string(" source-entry-token ");
    write_hex_u32(interrupts64_user_runqueue_source_entry_token());
    write_string(" target-entry-token ");
    write_hex_u32(interrupts64_user_runqueue_target_entry_token());
    write_string(" source-rip ");
    write_hex_u64(interrupts64_user_runqueue_source_rip());
    write_string(" source-rsp ");
    write_hex_u64(interrupts64_user_runqueue_source_rsp());
    write_string(" target-rip ");
    write_hex_u64(interrupts64_user_runqueue_target_rip());
    write_string(" target-rsp ");
    write_hex_u64(interrupts64_user_runqueue_target_rsp());
    write_string(" cs ");
    write_hex_u64(interrupts64_user_runqueue_probe_cs());
    write_string(" ss ");
    write_hex_u64(interrupts64_user_runqueue_probe_ss());
    write_labeled_hex_u32(" rflags ", interruptible_rflags);
    write_line("");

#ifdef LIMITLESS_X64_UEFI_KERNEL
    scheduler64_runqueue_reset();
    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && (console_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((policy_user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u)
        && ((console_user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        block_source_task = scheduler64_runqueue_register_user_task(
            source_rip,
            source_rsp,
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            interruptible_rflags);
        block_target_task = scheduler64_runqueue_register_user_task(
            target_rip,
            target_rsp,
            launch64_manifest_runtime_user_entry_selectors(console_manifest),
            interruptible_rflags);
    }
    block_start = (block_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_start(block_source_task)
        : 0u;
    block_result = (block_start != 0u)
        ? scheduler64_runqueue_block_task(block_source_task)
        : 0u;
    block_state = (block_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(block_source_task)
        : 0u;
    block_count = scheduler64_runqueue_block_count();
    blocked_after_block = scheduler64_runqueue_blocked_count();
    if (block_result != 0u)
    {
        block_frame.rip = source_rip;
        block_frame.rsp = source_rsp;
        block_frame.rflags = interruptible_rflags;
        block_frame.cs =
            launch64_manifest_runtime_user_entry_selectors(policy_manifest) & 0xFFFFull;
        block_frame.ss =
            (launch64_manifest_runtime_user_entry_selectors(policy_manifest) >> 16) & 0xFFFFull;
        block_switch = scheduler64_runqueue_on_timer(&block_frame);
    }
    block_target_state = (block_target_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(block_target_task)
        : 0u;
    block_wake = (block_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_wake_task(block_source_task)
        : 0u;
    block_wake_state = (block_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(block_source_task)
        : 0u;
    block_wake_count = scheduler64_runqueue_wake_count();
    block_switch_back = (block_wake != 0u)
        ? scheduler64_runqueue_on_timer(&block_frame)
        : 0u;
    block_final_state = (block_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(block_source_task)
        : 0u;
    block_denials_before = scheduler64_runqueue_block_denial_count();
    block_invalid_deny =
        (scheduler64_runqueue_block_task(SCHEDULER64_INVALID_TASK) == 0u) ? 1u : 0u;
    block_running_wake_deny =
        (scheduler64_runqueue_wake_task(block_source_task) == 0u) ? 1u : 0u;
    block_denials_after = scheduler64_runqueue_block_denial_count();
    block_positive =
        ((block_start != 0u)
            && (block_result != 0u)
            && (block_state == SCHEDULER64_TASK_BLOCKED)
            && (block_count == 1u)
            && (blocked_after_block == 1u)
            && (block_switch != 0u)
            && (block_target_state == SCHEDULER64_TASK_RUNNING)
            && (block_wake != 0u)
            && (block_wake_state == SCHEDULER64_TASK_READY)
            && (block_wake_count == 1u)
            && (block_switch_back != 0u)
            && (block_final_state == SCHEDULER64_TASK_RUNNING)
            && (block_invalid_deny != 0u)
            && (block_running_wake_deny != 0u)
            && ((block_denials_after - block_denials_before) == 2u))
            ? 1u
            : 0u;
    scheduler64_runqueue_stop();

    write_string("[x64] sched-block start ");
    write_dec_u32(block_start);
    write_string(" block ");
    write_dec_u32(block_result);
    write_string("/");
    write_dec_u32(block_state);
    write_string("/");
    write_dec_u32(blocked_after_block);
    write_string(" switch ");
    write_dec_u32(block_switch);
    write_string("/");
    write_dec_u32(block_target_state);
    write_string(" wake ");
    write_dec_u32(block_wake);
    write_string("/");
    write_dec_u32(block_wake_state);
    write_string(" back ");
    write_dec_u32(block_switch_back);
    write_string("/");
    write_dec_u32(block_final_state);
    write_string(" counts ");
    write_dec_u32(block_count);
    write_string("/");
    write_dec_u32(block_wake_count);
    write_string(" deny ");
    write_dec_u32(block_invalid_deny);
    write_string("/");
    write_dec_u32(block_running_wake_deny);
    write_string("/");
    write_dec_u32(block_denials_after - block_denials_before);
    write_string(" positive ");
    write_dec_u32(block_positive);
    write_line("");

    scheduler64_runqueue_reset();
    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && (console_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((policy_user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u)
        && ((console_user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        sleep_source_task = scheduler64_runqueue_register_user_task(
            source_rip,
            source_rsp,
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            interruptible_rflags);
        sleep_peer_task = scheduler64_runqueue_register_user_task(
            target_rip,
            target_rsp,
            launch64_manifest_runtime_user_entry_selectors(console_manifest),
            interruptible_rflags);
    }
    sleep_start = (sleep_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_start(sleep_source_task)
        : 0u;
    sleep_current_before = scheduler64_runqueue_current_task_id();
    sleep_pending_before = scheduler64_sleep_pending_count();
    sleep_count_before = scheduler64_sleep_count();
    sleep_wake_before = scheduler64_sleep_wake_count();
    sleep_sched_block_before = scheduler64_runqueue_block_count();
    sleep_sched_wake_before = scheduler64_runqueue_wake_count();
    sleep_result = (sleep_start != 0u)
        ? scheduler64_sleep_for_ticks(sleep_requested)
        : 0u;
    sleep_wake_tick = scheduler64_sleep_last_wake_tick();
    sleep_state_after_block = (sleep_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(sleep_source_task)
        : 0u;
    sleep_pending_after_block = scheduler64_sleep_pending_count();
    sleep_count_after = scheduler64_sleep_count();
    sleep_sched_block_after = scheduler64_runqueue_block_count();
    sleep_source_frame.rip = source_rip;
    sleep_source_frame.rsp = source_rsp;
    sleep_source_frame.rflags = interruptible_rflags;
    sleep_source_frame.cs =
        launch64_manifest_runtime_user_entry_selectors(policy_manifest) & 0xFFFFull;
    sleep_source_frame.ss =
        (launch64_manifest_runtime_user_entry_selectors(policy_manifest) >> 16) & 0xFFFFull;
    sleep_switch_to_peer =
        (sleep_state_after_block == SCHEDULER64_TASK_BLOCKED)
            ? scheduler64_runqueue_on_timer(&sleep_source_frame)
            : 0u;
    sleep_peer_state = (sleep_peer_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(sleep_peer_task)
        : 0u;
    if (sleep_wake_tick > pit_get_ticks())
    {
        interrupts64_enable();
        wait_for_timer_ticks(sleep_wake_tick);
        interrupts64_disable();
    }
    sleep_peer_frame.rip = target_rip;
    sleep_peer_frame.rsp = target_rsp;
    sleep_peer_frame.rflags = interruptible_rflags;
    sleep_peer_frame.cs =
        launch64_manifest_runtime_user_entry_selectors(console_manifest) & 0xFFFFull;
    sleep_peer_frame.ss =
        (launch64_manifest_runtime_user_entry_selectors(console_manifest) >> 16) & 0xFFFFull;
    sleep_switch_to_source =
        (sleep_peer_state == SCHEDULER64_TASK_RUNNING)
            ? scheduler64_runqueue_on_timer(&sleep_peer_frame)
            : 0u;
    sleep_current_after_wake = scheduler64_runqueue_current_task_id();
    sleep_final_state = (sleep_source_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(sleep_source_task)
        : 0u;
    sleep_pending_after_wake = scheduler64_sleep_pending_count();
    sleep_wake_after = scheduler64_sleep_wake_count();
    sleep_sched_wake_after = scheduler64_runqueue_wake_count();
    sleep_elapsed = scheduler64_sleep_last_elapsed_ticks();
    sleep_last_task = scheduler64_sleep_last_task_id();
    scheduler64_runqueue_stop();
    scheduler64_runqueue_reset();

    sleep_denial_before = scheduler64_sleep_denial_count();
    sleep_deny_task = scheduler64_runqueue_register_user_task(
        source_rip,
        source_rsp,
        launch64_manifest_runtime_user_entry_selectors(policy_manifest),
        interruptible_rflags);
    sleep_deny_start = (sleep_deny_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_start(sleep_deny_task)
        : 0u;
    sleep_deny_result = (sleep_deny_start != 0u)
        ? scheduler64_sleep_for_ticks(1u)
        : 1u;
    sleep_deny_state = (sleep_deny_task != SCHEDULER64_INVALID_TASK)
        ? scheduler64_runqueue_task_state(sleep_deny_task)
        : 0u;
    sleep_denial_after = scheduler64_sleep_denial_count();
    scheduler64_runqueue_stop();

    sleep_positive =
        ((sleep_start != 0u)
            && (sleep_result != 0u)
            && (sleep_current_before == sleep_source_task)
            && (sleep_state_after_block == SCHEDULER64_TASK_BLOCKED)
            && (sleep_pending_before == 0u)
            && (sleep_pending_after_block == 1u)
            && ((sleep_count_after - sleep_count_before) == 1u)
            && ((sleep_sched_block_after - sleep_sched_block_before) == 1u)
            && (sleep_switch_to_peer != 0u)
            && (sleep_peer_state == SCHEDULER64_TASK_RUNNING)
            && (sleep_wake_tick != 0u)
            && (sleep_switch_to_source != 0u)
            && (sleep_current_after_wake == sleep_source_task)
            && (sleep_final_state == SCHEDULER64_TASK_RUNNING)
            && (sleep_pending_after_wake == 0u)
            && ((sleep_wake_after - sleep_wake_before) == 1u)
            && ((sleep_sched_wake_after - sleep_sched_wake_before) == 1u)
            && (sleep_elapsed >= sleep_requested)
            && (sleep_last_task == sleep_source_task)
            && (sleep_deny_start != 0u)
            && (sleep_deny_result == 0u)
            && (sleep_deny_state == SCHEDULER64_TASK_RUNNING)
            && ((sleep_denial_after - sleep_denial_before) == 1u))
            ? 1u
            : 0u;

    write_string("[x64] sched-sleep start ");
    write_dec_u32(sleep_start);
    write_string(" sleep ");
    write_dec_u32(sleep_result);
    write_string("/");
    write_dec_u32(sleep_state_after_block);
    write_string("/");
    write_dec_u32(sleep_pending_after_block);
    write_string(" switch ");
    write_dec_u32(sleep_switch_to_peer);
    write_string("/");
    write_dec_u32(sleep_peer_state);
    write_string(" wake ");
    write_dec_u32(sleep_switch_to_source);
    write_string("/");
    write_dec_u32(sleep_current_after_wake);
    write_string("/");
    write_dec_u32(sleep_final_state);
    write_string("/");
    write_dec_u32(sleep_pending_after_wake);
    write_string(" ticks ");
    write_dec_u32(sleep_requested);
    write_string("/");
    write_dec_u32(sleep_elapsed);
    write_string("/");
    write_dec_u32(sleep_wake_tick);
    write_string(" counts ");
    write_dec_u32(sleep_count_after - sleep_count_before);
    write_string("/");
    write_dec_u32(sleep_wake_after - sleep_wake_before);
    write_string("/");
    write_dec_u32(sleep_sched_block_after - sleep_sched_block_before);
    write_string("/");
    write_dec_u32(sleep_sched_wake_after - sleep_sched_wake_before);
    write_string(" deny ");
    write_dec_u32(sleep_deny_result);
    write_string("/");
    write_dec_u32(sleep_deny_state);
    write_string("/");
    write_dec_u32(sleep_denial_after - sleep_denial_before);
    write_string(" last ");
    write_dec_u32(sleep_last_task);
    write_string(" positive ");
    write_dec_u32(sleep_positive);
    write_line("");
    scheduler64_runqueue_reset();
#endif
}

#endif /* LIMITLESS_SCAFFOLD_SCHED_LAUNCH_AND_TASKS */

#if !defined(LIMITLESS_SCAFFOLD_UNITY)
void limitless_scaffold_sched_anchor(void) {}
#endif
