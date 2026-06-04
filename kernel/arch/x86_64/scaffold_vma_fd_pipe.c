/* Split scaffold fragment. Real code is unity-included by scaffold.c; direct compilation emits only the anchor below. */

#if defined(LIMITLESS_SCAFFOLD_VMA_FD_PIPE_BOOTSTRAP)
#ifdef LIMITLESS_X64_UEFI_KERNEL
    vma_attach = vma64_init_process(init_pid);
    vma_probe = vma64_region_acquire();
    vma_overlap = vma64_region_acquire();
    vma_inserted = (vma64_region_prepare(
            vma_probe,
            0x0000000042000000ull,
            0x0000000042002000ull,
            VMA64_PHYS_ANON,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_ANONYMOUS,
            VMA64_BACKING_ANON,
            VMA64_BACKING_HANDLE_NONE,
            0xA2000001u) != 0u)
        ? vma64_insert(init_pid, vma_probe)
        : 0u;
    vma_denied = (vma64_region_prepare(
            vma_overlap,
            0x0000000042001000ull,
            0x0000000042003000ull,
            VMA64_PHYS_ANON,
            VMA64_PROT_READ,
            VMA64_MAP_PRIVATE | VMA64_MAP_ANONYMOUS,
            VMA64_BACKING_ANON,
            VMA64_BACKING_HANDLE_NONE,
            0xA2000002u) != 0u)
        ? ((vma64_insert(init_pid, vma_overlap) == 0u) ? 1u : 0u)
        : 0u;
    vma_found = vma64_find(init_pid, 0x0000000042001000ull);
    vma_gap = vma64_find_gap(
        init_pid,
        0x0000000042000000ull,
        0x0000000042010000ull,
        0x0000000000001000ull,
        VMA64_PAGE_BYTES);
    vma_removed = vma64_remove(
        init_pid,
        0x0000000042000000ull,
        0x0000000042002000ull);
    if (vma_removed != 0)
    {
        vma64_region_release(vma_removed);
    }
    if (vma_overlap != 0)
    {
        vma64_region_release(vma_overlap);
    }
    write_string("[x64] vma-A2 attach ");
    write_dec_u32(vma_attach);
    write_string(" insert ");
    write_dec_u32(vma_inserted);
    write_string(" find ");
    write_dec_u32((vma_found != 0) ? 1u : 0u);
    write_string(" gap ");
    write_hex_u64(vma_gap);
    write_string(" deny-overlap ");
    write_dec_u32(vma_denied);
    write_string(" count ");
    write_dec_u32(vma64_region_count(init_pid));
    write_string(" bytes ");
    write_hex_u64(vma64_mapped_bytes(init_pid));
    write_line("");

    vma_anon = vma64_map_anon(
        init_pid,
        0x0000000044000000ull,
        VMA64_PAGE_BYTES,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_ANONYMOUS);
    vma_anon_found = vma64_find(init_pid, vma_anon);
    vma_anon_denied = (vma64_map_anon(
            init_pid,
            vma_anon,
            VMA64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_WRITE,
            VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS) == 0ull)
        ? 1u
        : 0u;
    write_string("[x64] vma-A3 map ");
    write_hex_u64(vma_anon);
    write_string(" pte ");
    write_dec_u32(paging64_user_page_present(vma_anon));
    write_string(" phys ");
    write_hex_u64(paging64_user_page_physical(vma_anon));
    write_string(" prot ");
    write_hex_u32(paging64_user_page_protection(vma_anon));
    write_string(" vma ");
    write_dec_u32((vma_anon_found != 0) ? 1u : 0u);
    write_string(" pages ");
    write_dec_u32(vma64_anon_claimed_pages());
    write_string(" count ");
    write_dec_u32(vma64_region_count(init_pid));
    write_string(" bytes ");
    write_hex_u64(vma64_mapped_bytes(init_pid));
    write_string(" deny-fixed-overlap ");
    write_dec_u32(vma_anon_denied);
    write_line("");

    vma_unmapped = vma64_unmap(init_pid, vma_anon, VMA64_PAGE_BYTES);
    vma_unmap_denied = (vma64_unmap(init_pid, vma_anon, VMA64_PAGE_BYTES) == 0u) ? 1u : 0u;
    write_string("[x64] vma-A4 unmap ");
    write_dec_u32(vma_unmapped);
    write_string(" pte ");
    write_dec_u32(paging64_user_page_present(vma_anon));
    write_string(" phys ");
    write_hex_u64(paging64_user_page_physical(vma_anon));
    write_string(" vma ");
    write_dec_u32((vma64_find(init_pid, vma_anon) != 0) ? 1u : 0u);
    write_string(" pages ");
    write_dec_u32(vma64_anon_claimed_pages());
    write_string(" count ");
    write_dec_u32(vma64_region_count(init_pid));
    write_string(" bytes ");
    write_hex_u64(vma64_mapped_bytes(init_pid));
    write_string(" stage ");
    write_dec_u32(vma64_last_unmap_stage());
    write_string(" deny-missing ");
    write_dec_u32(vma_unmap_denied);
    write_line("");

    vma_protect_base = vma64_map_anon(
        init_pid,
        0x0000000044003000ull,
        (u64)VMA64_PAGE_BYTES * 3ull,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_ANONYMOUS);
    vma_protect_mid = (vma_protect_base != 0ull)
        ? (vma_protect_base + VMA64_PAGE_BYTES)
        : 0ull;
    vma_protect_before = (vma_protect_mid != 0ull)
        ? paging64_user_page_protection(vma_protect_mid)
        : 0u;
    vma_protected = (vma_protect_mid != 0ull)
        ? vma64_protect(
            init_pid,
            vma_protect_mid,
            VMA64_PAGE_BYTES,
            VMA64_PROT_READ | VMA64_PROT_EXECUTE)
        : 0u;
    vma_protect_after = (vma_protect_mid != 0ull)
        ? paging64_user_page_protection(vma_protect_mid)
        : 0u;
    vma_protect_left_region = (vma_protect_base != 0ull)
        ? vma64_find(init_pid, vma_protect_base)
        : 0;
    vma_protect_mid_region = (vma_protect_mid != 0ull)
        ? vma64_find(init_pid, vma_protect_mid)
        : 0;
    vma_protect_right_region = (vma_protect_base != 0ull)
        ? vma64_find(init_pid, vma_protect_base + ((u64)VMA64_PAGE_BYTES * 2ull))
        : 0;
    vma_protect_split_count = vma64_region_count(init_pid);
    vma_protect_denied = (vma_protect_base != 0ull)
        ? ((vma64_protect(
                init_pid,
                vma_protect_base + ((u64)VMA64_PAGE_BYTES * 3ull),
                VMA64_PAGE_BYTES,
                VMA64_PROT_READ) == 0u)
            ? 1u
            : 0u)
        : 0u;
    write_string("[x64] vma-A5 protect ");
    write_dec_u32(vma_protected);
    write_string(" before ");
    write_hex_u32(vma_protect_before);
    write_string(" after ");
    write_hex_u32(vma_protect_after);
    write_string(" split-count ");
    write_dec_u32(vma_protect_split_count);
    write_string(" left ");
    write_hex_u32((vma_protect_left_region != 0) ? vma_protect_left_region->prot_flags : 0u);
    write_string(" mid ");
    write_hex_u32((vma_protect_mid_region != 0) ? vma_protect_mid_region->prot_flags : 0u);
    write_string(" right ");
    write_hex_u32((vma_protect_right_region != 0) ? vma_protect_right_region->prot_flags : 0u);
    write_string(" map-stage ");
    write_dec_u32(vma64_last_map_stage());
    write_string(" deny-missing ");
    write_dec_u32(vma_protect_denied);
    write_line("");
    if (vma_protect_base != 0ull)
    {
        (void)vma64_unmap(init_pid, vma_protect_base, VMA64_PAGE_BYTES);
        (void)vma64_unmap(init_pid, vma_protect_mid, VMA64_PAGE_BYTES);
        (void)vma64_unmap(
            init_pid,
            vma_protect_base + ((u64)VMA64_PAGE_BYTES * 2ull),
            VMA64_PAGE_BYTES);
    }

    vma_rb_inserted = 0u;
    vma_rb_lookup = 0u;
    vma_rb_missed = 0u;
    vma_rb_cleanup = 0u;
    for (vma_rb_index = 0u; vma_rb_index < 1000u; ++vma_rb_index)
    {
        vma_rb_slot = (vma_rb_index * 37u) % 1000u;
        vma_rb_addr = 0x0000000048000000ull + ((u64)vma_rb_slot * 0x0000000000002000ull);
        vma_rb_region = vma64_region_acquire();
        if ((vma_rb_region != 0)
            && (vma64_region_prepare(
                    vma_rb_region,
                    vma_rb_addr,
                    vma_rb_addr + VMA64_PAGE_BYTES,
                    VMA64_PHYS_ANON,
                    VMA64_PROT_READ,
                    VMA64_MAP_PRIVATE | VMA64_MAP_ANONYMOUS,
                    VMA64_BACKING_ANON,
                    VMA64_BACKING_HANDLE_NONE,
                    0xA6000000u | (vma_rb_index + 1u)) != 0u)
            && (vma64_insert(init_pid, vma_rb_region) != 0u))
        {
            ++vma_rb_inserted;
        }
        else if (vma_rb_region != 0)
        {
            vma64_region_release(vma_rb_region);
        }
    }

    vma64_reset_lookup_telemetry();
    for (vma_rb_index = 0u; vma_rb_index < 1000u; ++vma_rb_index)
    {
        vma_rb_slot = (vma_rb_index * 53u) % 1000u;
        vma_rb_addr = 0x0000000048000000ull + ((u64)vma_rb_slot * 0x0000000000002000ull);
        vma_rb_region = vma64_find(init_pid, vma_rb_addr + 0x80ull);
        if ((vma_rb_region != 0) && (vma_rb_region->virt_base == vma_rb_addr))
        {
            ++vma_rb_lookup;
        }
        else
        {
            ++vma_rb_missed;
        }
    }
    vma_rb_max_steps = vma64_peak_lookup_steps();

    for (vma_rb_index = 0u; vma_rb_index < 1000u; ++vma_rb_index)
    {
        vma_rb_addr = 0x0000000048000000ull + ((u64)vma_rb_index * 0x0000000000002000ull);
        vma_rb_region = vma64_remove(init_pid, vma_rb_addr, vma_rb_addr + VMA64_PAGE_BYTES);
        if (vma_rb_region != 0)
        {
            vma64_region_release(vma_rb_region);
            ++vma_rb_cleanup;
        }
    }
    write_string("[x64] vma-A6 rb-insert ");
    write_dec_u32(vma_rb_inserted);
    write_string(" rb-lookup ");
    write_dec_u32(vma_rb_lookup);
    write_string(" miss ");
    write_dec_u32(vma_rb_missed);
    write_string(" max-steps ");
    write_dec_u32(vma_rb_max_steps);
    write_string(" bound ");
    write_dec_u32(32u);
    write_string(" bound-ok ");
    write_dec_u32(((vma_rb_max_steps <= 32u)
            && (vma_rb_inserted == 1000u)
            && (vma_rb_lookup == 1000u)
            && (vma_rb_missed == 0u))
        ? 1u
        : 0u);
    write_string(" cleanup ");
    write_dec_u32(vma_rb_cleanup);
    write_string(" count ");
    write_dec_u32(vma64_region_count(init_pid));
    write_line("");

    vma_cow_source = vma64_map_anon(
        init_pid,
        0x0000000044006000ull,
        VMA64_PAGE_BYTES,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_ANONYMOUS);
    vma_cow_target = 0x0000000044007000ull;
    vma_cow_old_phys = (vma_cow_source != 0ull)
        ? paging64_user_page_physical(vma_cow_source)
        : 0ull;
    if (vma_cow_source != 0ull)
    {
        vma_cow_source_words = (volatile u32 *)(u64)vma_cow_source;
        vma_cow_source_words[0] = 0xA7C0FFEEu;
        vma_cow_source_words[1] = 0x13579BDFu;
    }
    vma_cow_old_checksum_before = vma64_physical_page_checksum(vma_cow_old_phys);
    vma_cow_cloned = (vma_cow_source != 0ull)
        ? vma64_clone_cow_page(init_pid, vma_cow_source, policy_pid, vma_cow_target)
        : 0u;
    vma_cow_ref_after_clone = vma64_physical_page_ref_count(vma_cow_old_phys);
    vma_cow_faulted = (vma_cow_cloned != 0u)
        ? vma64_handle_cow_fault(
            policy_pid,
            vma_cow_target,
            VMA64_FAULT_PRESENT | VMA64_FAULT_WRITE | VMA64_FAULT_USER)
        : 0u;
    vma_cow_new_phys = (vma_cow_faulted != 0u)
        ? paging64_user_page_physical(vma_cow_target)
        : 0ull;
    vma_cow_copy_checksum = vma64_physical_page_checksum(vma_cow_new_phys);
    vma_cow_ref_after_fault = vma64_physical_page_ref_count(vma_cow_old_phys);
    if (vma_cow_faulted != 0u)
    {
        vma_cow_target_words = (volatile u32 *)(u64)vma_cow_target;
        vma_cow_target_words[0] = 0xA7D0C0DEu;
        vma_cow_target_words[1] = 0x2468ACE0u;
    }
    vma_cow_old_checksum_after = vma64_physical_page_checksum(vma_cow_old_phys);
    vma_cow_new_checksum_after = vma64_physical_page_checksum(vma_cow_new_phys);
    vma_cow_source_unchanged =
        (vma_cow_old_checksum_after == vma_cow_old_checksum_before) ? 1u : 0u;
    vma_cow_copy_match =
        ((vma_cow_copy_checksum == vma_cow_old_checksum_before)
            && (vma_cow_copy_checksum != 0u))
            ? 1u
            : 0u;
    vma_cow_private_copy =
        ((vma_cow_new_phys != 0ull)
            && (vma_cow_new_phys != vma_cow_old_phys)
            && (vma_cow_new_checksum_after != vma_cow_old_checksum_after))
            ? 1u
            : 0u;
    vma_cow_denied = (vma_cow_target != 0ull)
        ? ((vma64_handle_cow_fault(
                policy_pid,
                vma_cow_target,
                VMA64_FAULT_PRESENT | VMA64_FAULT_USER) == 0u)
            ? 1u
            : 0u)
        : 0u;
    vma_cow_source_regions = vma64_cow_region_count(init_pid);
    vma_cow_target_regions = vma64_cow_region_count(policy_pid);
    write_string("[x64] vma-A7 cow-clone ");
    write_dec_u32(vma_cow_cloned);
    write_string(" cow-fault ");
    write_dec_u32(vma_cow_faulted);
    write_string(" old-phys ");
    write_hex_u64(vma_cow_old_phys);
    write_string(" new-phys ");
    write_hex_u64(vma_cow_new_phys);
    write_string(" ref-clone ");
    write_dec_u32(vma_cow_ref_after_clone);
    write_string(" ref-fault ");
    write_dec_u32(vma_cow_ref_after_fault);
    write_string(" copy-match ");
    write_dec_u32(vma_cow_copy_match);
    write_string(" source-unchanged ");
    write_dec_u32(vma_cow_source_unchanged);
    write_string(" private-copy ");
    write_dec_u32(vma_cow_private_copy);
    write_string(" source-cow ");
    write_dec_u32(vma_cow_source_regions);
    write_string(" target-cow ");
    write_dec_u32(vma_cow_target_regions);
    write_string(" faults ");
    write_dec_u32(vma64_cow_fault_count());
    write_string(" map-stage ");
    write_dec_u32(vma64_last_map_stage());
    write_string(" deny-read ");
    write_dec_u32(vma_cow_denied);
    write_line("");
    if (vma_cow_cloned != 0u)
    {
        (void)vma64_unmap(policy_pid, vma_cow_target, VMA64_PAGE_BYTES);
    }
    if (vma_cow_source != 0ull)
    {
        (void)vma64_unmap(init_pid, vma_cow_source, VMA64_PAGE_BYTES);
    }

    vma_brk_base = vma64_brk_query(init_pid);
    vma_brk_grown = (vma_brk_base != 0ull)
        ? vma64_brk_extend(init_pid, vma_brk_base + VMA64_PAGE_BYTES)
        : 0ull;
    vma_brk_present = (vma_brk_base != 0ull)
        ? paging64_user_page_present(vma_brk_base)
        : 0u;
    vma_brk_region = (vma_brk_base != 0ull)
        ? ((vma64_find(init_pid, vma_brk_base) != 0) ? 1u : 0u)
        : 0u;
    vma_brk_shrunk = (vma_brk_base != 0ull)
        ? vma64_brk_extend(init_pid, vma_brk_base)
        : 0ull;
    vma_brk_deny_underflow = (vma_brk_base != 0ull)
        ? ((vma64_brk_extend(init_pid, vma_brk_base - VMA64_PAGE_BYTES) == 0ull) ? 1u : 0u)
        : 0u;
    write_string("[x64] vma-A8a brk-base ");
    write_hex_u64(vma_brk_base);
    write_string(" grow ");
    write_hex_u64(vma_brk_grown);
    write_string(" pte ");
    write_dec_u32(vma_brk_present);
    write_string(" vma ");
    write_dec_u32(vma_brk_region);
    write_string(" shrink ");
    write_hex_u64(vma_brk_shrunk);
    write_string(" pte-after ");
    write_dec_u32(paging64_user_page_present(vma_brk_base));
    write_string(" count ");
    write_dec_u32(vma64_region_count(init_pid));
    write_string(" pages ");
    write_dec_u32(vma64_anon_claimed_pages());
    write_string(" deny-underflow ");
    write_dec_u32(vma_brk_deny_underflow);
    write_line("");

    vma_diag_source = vma64_map_anon(
        init_pid,
        0x0000000044008000ull,
        VMA64_PAGE_BYTES,
        VMA64_PROT_READ | VMA64_PROT_WRITE,
        VMA64_MAP_PRIVATE | VMA64_MAP_FIXED | VMA64_MAP_ANONYMOUS);
    vma_diag_target = 0x0000000044009000ull;
    vma_diag_cloned = (vma_diag_source != 0ull)
        ? vma64_clone_cow_page(init_pid, vma_diag_source, policy_pid, vma_diag_target)
        : 0u;
    vma_diag_count = syscall64_invoke(
        X64_SYSCALL_VMA_DIAG_QUERY,
        init_pid,
        VMA64_DIAG_REGION_COUNT,
        0u);
    vma_diag_bytes = syscall64_invoke(
        X64_SYSCALL_VMA_DIAG_QUERY,
        init_pid,
        VMA64_DIAG_MAPPED_BYTES,
        0u);
    vma_diag_cow_pages = syscall64_invoke(
        X64_SYSCALL_VMA_DIAG_QUERY,
        init_pid,
        VMA64_DIAG_COW_PAGE_COUNT,
        0u);
    vma_diag_brk = syscall64_invoke(
        X64_SYSCALL_VMA_DIAG_QUERY,
        init_pid,
        VMA64_DIAG_BRK_CURRENT,
        0u);
    vma_diag_peak = syscall64_invoke(
        X64_SYSCALL_VMA_DIAG_QUERY,
        init_pid,
        VMA64_DIAG_PEAK_REGION_COUNT,
        0u);
    vma_diag_deny_selector_raw = syscall64_invoke(
        X64_SYSCALL_VMA_DIAG_QUERY,
        init_pid,
        0xFFFFFFFFu,
        0u);
    vma_diag_deny_pid_raw = syscall64_invoke(
        X64_SYSCALL_VMA_DIAG_QUERY,
        PROCESS64_INVALID_PID,
        VMA64_DIAG_REGION_COUNT,
        0u);
    vma_diag_positive =
        ((vma_diag_cloned != 0u)
            && (vma_diag_count == 1ull)
            && (vma_diag_bytes == (u64)VMA64_PAGE_BYTES)
            && (vma_diag_cow_pages == 1ull)
            && (vma_diag_brk == vma_brk_base)
            && (vma_diag_peak != 0ull))
            ? 1u
            : 0u;
    vma_diag_cleanup = 0u;
    if (vma_diag_cloned != 0u)
    {
        vma_diag_cleanup += vma64_unmap(policy_pid, vma_diag_target, VMA64_PAGE_BYTES);
    }
    if (vma_diag_source != 0ull)
    {
        vma_diag_cleanup += vma64_unmap(init_pid, vma_diag_source, VMA64_PAGE_BYTES);
    }
    write_string("[x64] vma-A9 diag count ");
    write_dec_u32((u32)vma_diag_count);
    write_string(" bytes ");
    write_hex_u64(vma_diag_bytes);
    write_string(" cow-pages ");
    write_dec_u32((u32)vma_diag_cow_pages);
    write_string(" brk ");
    write_hex_u64(vma_diag_brk);
    write_string(" peak ");
    write_dec_u32((u32)vma_diag_peak);
    write_string(" positive ");
    write_dec_u32(vma_diag_positive);
    write_string(" deny-selector ");
    write_dec_u32((vma_diag_deny_selector_raw == VMA64_DIAG_DENIED) ? 1u : 0u);
    write_string(" deny-pid ");
    write_dec_u32((vma_diag_deny_pid_raw == VMA64_DIAG_DENIED) ? 1u : 0u);
    write_string(" cleanup ");
    write_dec_u32(vma_diag_cleanup);
    write_line("");

    fd_owner = process64_principal(init_pid);
    fd_stdin_cap = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INPUT,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        fd_owner);
    fd_stdout_cap = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        fd_owner);
    fd_stderr_cap = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        fd_owner);
    fd_extra_cap = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        fd_owner);
    fd_init_result = fd64_init_process(
        init_pid,
        fd_owner,
        fd_stdin_cap,
        fd_stdout_cap,
        fd_stderr_cap);
    fd_attached = (process64_fd_table(init_pid) != 0) ? 1u : 0u;
    fd0_live = ((fd64_entry_type(init_pid, FD64_STDIN) == FD64_TYPE_DEVICE)
        && (fd64_entry_capability(init_pid, FD64_STDIN) == fd_stdin_cap))
        ? 1u
        : 0u;
    fd1_live = ((fd64_entry_type(init_pid, FD64_STDOUT) == FD64_TYPE_DEVICE)
        && (fd64_entry_capability(init_pid, FD64_STDOUT) == fd_stdout_cap))
        ? 1u
        : 0u;
    fd2_live = ((fd64_entry_type(init_pid, FD64_STDERR) == FD64_TYPE_DEVICE)
        && (fd64_entry_capability(init_pid, FD64_STDERR) == fd_stderr_cap))
        ? 1u
        : 0u;
    fd_route_in = capability64_route(fd64_entry_capability(init_pid, FD64_STDIN), CAPABILITY64_RIGHT_SEND, fd_owner);
    fd_route_out = capability64_route(fd64_entry_capability(init_pid, FD64_STDOUT), CAPABILITY64_RIGHT_SEND, fd_owner);
    fd_route_err = capability64_route(fd64_entry_capability(init_pid, FD64_STDERR), CAPABILITY64_RIGHT_SEND, fd_owner);
    fd_extra = fd64_alloc(init_pid, fd_extra_cap, FD64_TYPE_DEVICE, FD64_FLAG_O_CLOEXEC);
    fd_ref_before = fd64_entry_ref_count(init_pid, FD64_STDOUT);
    fd_stdout_entry = fd64_get(init_pid, FD64_STDOUT);
    fd_ref_get = fd64_entry_ref_count(init_pid, FD64_STDOUT);
    (void)fd64_put(init_pid, fd_stdout_entry);
    fd_ref_after_put = fd64_entry_ref_count(init_pid, FD64_STDOUT);
    fd_free_extra = (fd_extra != FD64_INVALID_FD) ? fd64_free(init_pid, fd_extra) : 0u;
    fd_deny_badfd = (fd64_get(init_pid, FD64_TABLE_LIMIT) == 0) ? 1u : 0u;
    write_string("[x64] fd-B1 entry-bytes ");
    write_dec_u32((u32)sizeof(fd_entry_t));
    write_string(" table-limit ");
    write_dec_u32(FD64_TABLE_LIMIT);
    write_string(" types ");
    write_dec_u32(6u);
    write_string(" flags ");
    write_hex_u32(FD64_FLAG_O_CLOEXEC | FD64_FLAG_O_NONBLOCK);
    write_line("");
#endif
#endif /* LIMITLESS_SCAFFOLD_VMA_FD_PIPE_BOOTSTRAP */

#if defined(LIMITLESS_SCAFFOLD_VMA_FD_PIPE_FD_OUTPUT)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    write_string("[x64] fd-B3 open-ramfs fd ");
    write_dec_u32(fd_ramfs_fd);
    write_string(" cap ");
    write_hex_u32(fd_ramfs_cap);
    write_string(" type ");
    write_dec_u32(fd_ramfs_type);
    write_string(" rights ");
    write_hex_u32(fd_ramfs_rights);
    write_string(" owner ");
    write_hex_u32(fd_ramfs_owner);
    write_string(" positive ");
    write_dec_u32(fd_ramfs_positive);
    write_string(" missing-deny ");
    write_dec_u32(fd_ramfs_missing);
    write_string(" free ");
    write_dec_u32(fd_ramfs_free);
    write_string(" live ");
    write_dec_u32(fd_ramfs_live_after_free);
    write_line("");
    write_string("[x64] fd-B4 read bytes ");
    write_dec_u32(fd_ramfs_read_bytes);
    write_string(" offset ");
    write_hex_u64(fd_ramfs_read_offset);
    write_string(" checksum ");
    write_hex_u32(fd_ramfs_read_checksum);
    write_string(" match ");
    write_dec_u32(fd_ramfs_read_match);
    write_string(" positive ");
    write_dec_u32(fd_ramfs_read_positive);
    write_string(" deny-badfd ");
    write_dec_u32(fd_ramfs_read_denied);
    write_line("");
    write_string("[x64] fd-B5 write stdout-bytes ");
    write_dec_u32(fd_stdout_write_bytes);
    write_string(" console-writes ");
    write_dec_u32(fd_stdout_console_count);
    write_string(" console-bytes ");
    write_dec_u32(fd_stdout_console_bytes);
    write_string(" stdin-deny ");
    write_dec_u32(fd_stdin_write_denied);
    write_string(" positive ");
    write_dec_u32(fd_write_positive);
    write_line("");
    write_string("[x64] fd-B6 close fd ");
    write_dec_u32(fd_close_fd);
    write_string(" cap ");
    write_hex_u32(fd_close_cap);
    write_string(" result ");
    write_dec_u32(fd_close_result);
    write_string(" slot-free ");
    write_dec_u32(fd_close_slot_free);
    write_string(" revoked ");
    write_dec_u32(fd_close_revoked);
    write_string(" positive ");
    write_dec_u32(fd_close_positive);
    write_line("");
    write_string("[x64] fd-B7 dup fd ");
    write_dec_u32(fd_dup_fd);
    write_string(" ref ");
    write_dec_u32(fd_dup_ref_stdout);
    write_string("/");
    write_dec_u32(fd_dup_ref_new);
    write_string(" write-bytes ");
    write_dec_u32(fd_dup_write_bytes);
    write_string(" console ");
    write_dec_u32(fd_dup_console_count);
    write_string("/");
    write_dec_u32(fd_dup_console_bytes);
    write_string(" close ");
    write_dec_u32(fd_dup_close);
    write_string(" source-live ");
    write_dec_u32(fd_dup_close_source_live);
    write_string(" dup2 ");
    write_dec_u32(fd_dup2_fd);
    write_string(" same-cap ");
    write_dec_u32(fd_dup2_same_cap);
    write_string(" ref2 ");
    write_dec_u32(fd_dup2_ref_stdout);
    write_string("/");
    write_dec_u32(fd_dup2_ref_target);
    write_string(" write2 ");
    write_dec_u32(fd_dup2_write_bytes);
    write_string(" console2 ");
    write_dec_u32(fd_dup2_console_count);
    write_string("/");
    write_dec_u32(fd_dup2_console_bytes);
    write_string(" bad-old ");
    write_dec_u32(fd_dup_bad_old);
    write_string(" positive ");
    write_dec_u32(fd_dup_positive);
    write_line("");
    write_string("[x64] fd-B8 seek set ");
    write_hex_u64(fd_seek_set);
    write_string(" read ");
    write_dec_u32(fd_seek_read_bytes);
    write_string(" offset ");
    write_hex_u64(fd_seek_offset_after);
    write_string(" checksum ");
    write_hex_u32(fd_seek_checksum);
    write_string(" match ");
    write_dec_u32(fd_seek_match);
    write_string(" end-minus ");
    write_hex_u64(fd_seek_end_minus);
    write_string(" cur-back ");
    write_hex_u64(fd_seek_cur_back);
    write_string(" deny-device ");
    write_dec_u32(fd_seek_device_denied);
    write_string(" deny-whence ");
    write_dec_u32(fd_seek_bad_whence);
    write_string(" positive ");
    write_dec_u32(fd_seek_positive);
    write_line("");
    write_string("[x64] fd-B9 stat result ");
    write_dec_u32(fd_stat_result);
    write_string(" fstat ");
    write_dec_u32(fd_fstat_result);
    write_string(" size ");
    write_hex_u64(fd_stat_size);
    write_string(" mode ");
    write_hex_u32(fd_stat_mode);
    write_string(" owner ");
    write_hex_u32(fd_stat_owner);
    write_string(" mtime ");
    write_hex_u64(fd_stat_mtime);
    write_string(" blocks ");
    write_hex_u64(fd_stat_blocks);
    write_string(" preserved ");
    write_dec_u32(fd_stat_offset_preserved);
    write_string(" size-match ");
    write_dec_u32(fd_stat_size_match);
    write_string(" mode-file ");
    write_dec_u32(fd_stat_mode_file);
    write_string(" deny-device ");
    write_dec_u32(fd_stat_device_denied);
    write_string(" deny-null ");
    write_dec_u32(fd_stat_null_denied);
    write_string(" deny-badfd ");
    write_dec_u32(fd_stat_badfd_denied);
    write_string(" positive ");
    write_dec_u32(fd_stat_positive);
    write_line("");
    write_string("[x64] fd-B10 cloexec fd ");
    write_dec_u32(fd_cloexec_fd);
    write_string(" keep ");
    write_dec_u32(fd_cloexec_keep_fd);
    write_string(" closed ");
    write_dec_u32(fd_cloexec_closed);
    write_string(" slot-free ");
    write_dec_u32(fd_cloexec_slot_free);
    write_string(" revoked ");
    write_dec_u32(fd_cloexec_revoked);
    write_string(" keep-live ");
    write_dec_u32(fd_cloexec_keep_live);
    write_string(" keep-free ");
    write_dec_u32(fd_cloexec_keep_free);
    write_string(" invalid-deny ");
    write_dec_u32(fd_cloexec_invalid_denied);
    write_string(" positive ");
    write_dec_u32(fd_cloexec_positive);
    write_line("");
#endif
#endif /* LIMITLESS_SCAFFOLD_VMA_FD_PIPE_FD_OUTPUT */

#if defined(LIMITLESS_SCAFFOLD_VMA_FD_PIPE_PIPE_OUTPUT)
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    write_string("[x64] pipe-C1 object-bytes ");
    write_dec_u32((u32)sizeof(pipe64_buffer_t));
    write_string(" buffer ");
    write_dec_u32(PIPE64_BUFFER_BYTES);
    write_string(" max ");
    write_dec_u32(PIPE64_MAX_BUFFER_BYTES);
    write_string(" pool ");
    write_dec_u32(PIPE64_MAX_OBJECTS);
    write_line("");
    write_string("[x64] pipe-C2 create ");
    write_dec_u32(pipe_create_result);
    write_string(" read-fd ");
    write_dec_u32(pipe_read_fd);
    write_string(" write-fd ");
    write_dec_u32(pipe_write_fd);
    write_string(" read-type ");
    write_dec_u32(pipe_read_type);
    write_string(" write-type ");
    write_dec_u32(pipe_write_type);
    write_string(" empty ");
    write_dec_u32(pipe_empty);
    write_string(" capacity ");
    write_dec_u32(pipe_capacity_value);
    write_string(" live-before ");
    write_dec_u32(pipe_live_before);
    write_string(" live-after ");
    write_dec_u32(pipe_live_after_create);
    write_string(" reader-closed ");
    write_dec_u32(pipe_reader_closed);
    write_string(" close ");
    write_dec_u32(pipe_close_read);
    write_string("/");
    write_dec_u32(pipe_close_write);
    write_string(" live-final ");
    write_dec_u32(pipe_live_after_close);
    write_string(" bad-pid ");
    write_dec_u32(pipe_bad_pid_denied);
    write_string(" positive ");
    write_dec_u32(pipe_positive);
    write_line("");
    write_string("[x64] pipe-C3 write bytes ");
    write_dec_u32(pipe_c3_bytes_written);
    write_string(" available ");
    write_dec_u32(pipe_c3_available);
    write_string(" wrong-end ");
    write_dec_u32(pipe_c3_wrong_end);
    write_string(" close ");
    write_dec_u32(pipe_c3_close_read);
    write_string("/");
    write_dec_u32(pipe_c3_close_write);
    write_string(" positive ");
    write_dec_u32(pipe_c3_positive);
    write_line("");
    write_string("[x64] pipe-C4 read bytes ");
    write_dec_u32(pipe_c4_read_bytes);
    write_string(" checksum ");
    write_hex_u32(pipe_c4_read_checksum);
    write_string(" match ");
    write_dec_u32(pipe_c4_match);
    write_string(" after ");
    write_dec_u32(pipe_c4_after_read);
    write_string(" eof ");
    write_dec_u32(pipe_c4_eof_bytes);
    write_string(" wrong-end ");
    write_dec_u32(pipe_c4_wrong_end);
    write_string(" close ");
    write_dec_u32(pipe_c4_close_write);
    write_string("/");
    write_dec_u32(pipe_c4_close_read);
    write_string(" live-final ");
    write_dec_u32(pipe_c4_live_final);
    write_string(" positive ");
    write_dec_u32(pipe_c4_positive);
    write_line("");
    write_string("[x64] pipe-C5 pair src ");
    write_dec_u32(init_pid);
    write_string(" dst ");
    write_dec_u32(policy_pid);
    write_string(" target-init ");
    write_dec_u32(pipe_c5_target_init);
    write_string(" create ");
    write_dec_u32(pipe_c5_create);
    write_string(" grant ");
    write_dec_u32(pipe_c5_grant);
    write_string(" read-a ");
    write_dec_u32(pipe_c5_read_a_fd);
    write_string(" read-b ");
    write_dec_u32(pipe_c5_read_b_fd);
    write_string(" write-a ");
    write_dec_u32(pipe_c5_write_a_fd);
    write_string(" write-bytes ");
    write_dec_u32(pipe_c5_write_bytes);
    write_string(" available ");
    write_dec_u32(pipe_c5_available_after_write);
    write_string(" close-writer ");
    write_dec_u32(pipe_c5_close_writer);
    write_string(" read-bytes ");
    write_dec_u32(pipe_c5_read_bytes);
    write_string(" checksum ");
    write_hex_u32(pipe_c5_checksum);
    write_string(" match ");
    write_dec_u32(pipe_c5_match);
    write_string(" eof ");
    write_dec_u32(pipe_c5_eof);
    write_string(" close-read ");
    write_dec_u32(pipe_c5_close_b_read);
    write_string("/");
    write_dec_u32(pipe_c5_close_a_read);
    write_string(" cleanup ");
    write_dec_u32(pipe_c5_target_cleanup);
    write_string(" invalid-grant ");
    write_dec_u32(pipe_c5_invalid_grant_denied);
    write_string(" sched-boundary ");
    write_dec_u32(pipe_c5_scheduler_boundary);
    write_string(" live-final ");
    write_dec_u32(pipe_c5_live_final);
    write_string(" positive ");
    write_dec_u32(pipe_c5_positive);
    write_line("");
    write_string("[x64] pipe-C6 block direct ");
    write_dec_u32(pipe_c6_direct_denied);
    write_string(" block ");
    write_hex_u32(pipe_c6_read_block_result);
    write_string(" tasks ");
    write_dec_u32(pipe_c6_current_before);
    write_string("/");
    write_dec_u32(pipe_c6_blocked_slot);
    write_string("/");
    write_dec_u32(pipe_c6_reader_state_after_block);
    write_string("/");
    write_dec_u32(pipe_c6_switch_to_writer);
    write_string("/");
    write_dec_u32(pipe_c6_writer_state_after_switch);
    write_string(" write ");
    write_dec_u32(pipe_c6_write_bytes);
    write_string(" wake ");
    write_dec_u32(pipe_c6_pipe_wake_after - pipe_c6_pipe_wake_before);
    write_string("/");
    write_dec_u32(pipe_c6_sched_wake_after - pipe_c6_sched_wake_before);
    write_string("/");
    write_dec_u32(pipe_c6_reader_state_after_wake);
    write_string("/");
    write_dec_u32(pipe_c6_slot_after_wake);
    write_string(" back ");
    write_dec_u32(pipe_c6_switch_to_reader);
    write_string("/");
    write_dec_u32(pipe_c6_current_after_switch);
    write_string("/");
    write_dec_u32(pipe_c6_reader_state_after_switch);
    write_string(" read ");
    write_dec_u32(pipe_c6_read_after_wake);
    write_string(" checksum ");
    write_hex_u32(pipe_c6_checksum);
    write_string(" match ");
    write_dec_u32(pipe_c6_match);
    write_string(" avail ");
    write_dec_u32(pipe_c6_avail_after_write);
    write_string("/");
    write_dec_u32(pipe_c6_avail_after_read);
    write_string(" counts ");
    write_dec_u32(pipe_c6_pipe_block_after - pipe_c6_pipe_block_before);
    write_string("/");
    write_dec_u32(pipe_c6_pipe_wake_after - pipe_c6_pipe_wake_before);
    write_string("/");
    write_dec_u32(pipe_c6_sched_block_after - pipe_c6_sched_block_before);
    write_string("/");
    write_dec_u32(pipe_c6_sched_wake_after - pipe_c6_sched_wake_before);
    write_string("/");
    write_dec_u32(pipe_c6_sched_denial_after - pipe_c6_sched_denial_before);
    write_string(" cleanup ");
    write_dec_u32(pipe_c6_close_writer);
    write_string("/");
    write_dec_u32(pipe_c6_close_b_read);
    write_string("/");
    write_dec_u32(pipe_c6_close_a_read);
    write_string("/");
    write_dec_u32(pipe_c6_target_cleanup);
    write_string("/");
    write_dec_u32(pipe_c6_live_final);
    write_string(" positive ");
    write_dec_u32(pipe_c6_positive);
    write_line("");
    write_string("[x64] persona-D1 bytes ");
    write_dec_u32((u32)sizeof(persona_context_t));
    write_string(" native ");
    write_dec_u32(PERSONA64_TYPE_LIMITLESS_NATIVE);
    write_string(" linux ");
    write_dec_u32(PERSONA64_TYPE_LINUX_ELF);
    write_string(" windows ");
    write_dec_u32(PERSONA64_TYPE_WINDOWS_PE);
    write_string(" mac ");
    write_dec_u32(PERSONA64_TYPE_MACOS_MACHO);
    write_string(" heap-none ");
    write_hex_u32(PERSONA64_HEAP_CAP_NONE);
    write_string(" tls-unset ");
    write_dec_u32((PERSONA64_TLS_UNSET == 0ull) ? 1u : 0u);
    write_line("");
    write_string("[x64] persona-D2 native-init ");
    write_dec_u32(persona_init_result);
    write_string(" attached ");
    write_dec_u32(persona_attached);
    write_string(" type ");
    write_dec_u32(persona_type_value);
    write_string(" syscall ");
    write_dec_u32(persona_syscall_bound);
    write_string(" fd-bound ");
    write_dec_u32(persona_fd_bound);
    write_string(" vma-bound ");
    write_dec_u32(persona_vma_bound);
    write_string(" audit-bound ");
    write_dec_u32(persona_audit_bound);
    write_string(" tls-unset ");
    write_dec_u32(persona_tls_unset);
    write_string(" heap-none ");
    write_dec_u32(persona_heap_none);
    write_string(" invalid-deny ");
    write_dec_u32(persona_invalid_denied);
    write_string(" duplicate-deny ");
    write_dec_u32(persona_duplicate_denied);
    write_string(" denials ");
    write_dec_u32(persona_denials);
    write_string(" native-live ");
    write_dec_u32(persona_native_live);
    write_string(" release ");
    write_dec_u32(persona_release_result);
    write_string(" after-release ");
    write_dec_u32(persona_after_release);
    write_string(" positive ");
    write_dec_u32(persona_positive);
    write_line("");
    persona_d3_elf = persona64_detect_format(
        persona_d3_elf_bytes,
        (u32)sizeof(persona_d3_elf_bytes));
    persona_d3_pe = persona64_detect_format(
        persona_d3_pe_bytes,
        (u32)sizeof(persona_d3_pe_bytes));
    persona_d3_macho64 = persona64_detect_format(
        persona_d3_macho64_bytes,
        (u32)sizeof(persona_d3_macho64_bytes));
    persona_d3_fat = persona64_detect_format(
        persona_d3_fat_bytes,
        (u32)sizeof(persona_d3_fat_bytes));
    persona_d3_shebang = persona64_detect_format(
        persona_d3_shebang_bytes,
        (u32)sizeof(persona_d3_shebang_bytes));
    persona_d3_native = persona64_detect_format(
        persona_d3_native_bytes,
        (u32)sizeof(persona_d3_native_bytes) - 1u);
    persona_d3_unknown = persona64_detect_format(
        persona_d3_unknown_bytes,
        (u32)sizeof(persona_d3_unknown_bytes));
    persona_d3_bad_pe = persona64_detect_format(
        persona_d3_bad_pe_bytes,
        (u32)sizeof(persona_d3_bad_pe_bytes));
    persona_d3_null = persona64_detect_format(0, 0u);
    persona_d3_positive =
        ((persona_d3_elf == PERSONA64_FORMAT_ELF)
            && (persona_d3_pe == PERSONA64_FORMAT_PE)
            && (persona_d3_macho64 == PERSONA64_FORMAT_MACHO_LE64)
            && (persona_d3_fat == PERSONA64_FORMAT_MACHO_FAT)
            && (persona_d3_shebang == PERSONA64_FORMAT_SHEBANG)
            && (persona_d3_native == PERSONA64_FORMAT_NATIVE_APP)
            && (persona_d3_unknown == PERSONA64_FORMAT_UNKNOWN)
            && (persona_d3_bad_pe == PERSONA64_FORMAT_UNKNOWN)
            && (persona_d3_null == PERSONA64_FORMAT_UNKNOWN))
            ? 1u
            : 0u;
    write_string("[x64] persona-D3 detect elf ");
    write_dec_u32(persona_d3_elf);
    write_string(" pe ");
    write_dec_u32(persona_d3_pe);
    write_string(" macho64 ");
    write_dec_u32(persona_d3_macho64);
    write_string(" fat ");
    write_dec_u32(persona_d3_fat);
    write_string(" shebang ");
    write_dec_u32(persona_d3_shebang);
    write_string(" native ");
    write_dec_u32(persona_d3_native);
    write_string(" unknown ");
    write_dec_u32(persona_d3_unknown);
    write_string(" bad-pe ");
    write_dec_u32(persona_d3_bad_pe);
    write_string(" null ");
    write_dec_u32(persona_d3_null);
    write_string(" positive ");
    write_dec_u32(persona_d3_positive);
    write_line("");
    write_string("[x64] persona-D4 launch-format pid ");
    write_dec_u32(persona_d4_pid);
    write_string(" attach ");
    write_dec_u32(persona_d4_attach_result);
    write_string(" before ");
    write_dec_u32(persona_d4_count_before);
    write_string(" rejected-token ");
    write_dec_u32(persona_d4_reject_token);
    write_string(" count ");
    write_dec_u32(persona_d4_count_after);
    write_string(" read ");
    write_dec_u32(persona_d4_read);
    write_string(" event ");
    write_dec_u32((u32)persona_d4_audit_record.event_type);
    write_string(" code ");
    write_dec_u32((u32)persona_d4_audit_record.event_code);
    write_string(" result ");
    write_dec_u32(persona_d4_audit_record.result);
    write_string(" rip ");
    write_hex_u64(persona_d4_audit_record.rip);
    write_string(" native-descriptor ");
    write_dec_u32(persona_d4_native_format);
    write_string(" release ");
    write_dec_u32(persona_d4_release_result);
    write_string(" after-release ");
    write_dec_u32(persona_d4_after_release);
    write_string(" positive ");
    write_dec_u32(persona_d4_positive);
    write_line("");
    write_string("[x64] persona-audit-D5 attach ");
    write_dec_u32(persona_audit_attach_result);
    write_string(" before ");
    write_dec_u32(persona_audit_count_before);
    write_string(" count ");
    write_dec_u32(persona_audit_count_after);
    write_string(" format-rec ");
    write_dec_u32(persona_audit_format_recorded);
    write_string(" syscall-rec ");
    write_dec_u32(persona_audit_syscall_recorded);
    write_string(" read0 ");
    write_dec_u32(persona_audit_read0);
    write_string(" event0 ");
    write_dec_u32((u32)persona_audit_record0.event_type);
    write_string(" code0 ");
    write_dec_u32((u32)persona_audit_record0.event_code);
    write_string(" result0 ");
    write_dec_u32(persona_audit_record0.result);
    write_string(" read1 ");
    write_dec_u32(persona_audit_read1);
    write_string(" event1 ");
    write_dec_u32((u32)persona_audit_record1.event_type);
    write_string(" code1 ");
    write_dec_u32((u32)persona_audit_record1.event_code);
    write_string(" result1 ");
    write_dec_u32(persona_audit_record1.result);
    write_string(" type0 ");
    write_dec_u32((u32)persona_audit_record0.persona_type);
    write_string(" type1 ");
    write_dec_u32((u32)persona_audit_record1.persona_type);
    write_string(" rip0 ");
    write_hex_u64(persona_audit_record0.rip);
    write_string(" rip1 ");
    write_hex_u64(persona_audit_record1.rip);
    write_string(" ordered ");
    write_dec_u32(persona_audit_timestamp_order);
    write_string(" dropped ");
    write_dec_u32(persona_audit_drop_count);
    write_string(" release ");
    write_dec_u32(persona_audit_release_result);
    write_string(" after-release ");
    write_dec_u32(persona_audit_after_release);
    write_string(" positive ");
    write_dec_u32(persona_audit_positive);
    write_line("");
#endif
#endif /* LIMITLESS_SCAFFOLD_VMA_FD_PIPE_PIPE_OUTPUT */

#if !defined(LIMITLESS_SCAFFOLD_UNITY)
void limitless_scaffold_vma_fd_pipe_anchor(void) {}
#endif
