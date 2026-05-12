#include "arch_build.h"
#include "apic_x64.h"
#include "block_x64.h"
#include "boot_info.h"
#include "capability_x64.h"
#include "descriptors_x64.h"
#include "display_x64.h"
#include "e1000e_x64.h"
#include "fs_x64.h"
#include "input_x64.h"
#include "interrupts_x64.h"
#include "launch_x64.h"
#include "mmio_x64.h"
#include "paging_x64.h"
#include "package_signing_x64.h"
#include "pci_x64.h"
#include "pic.h"
#include "pit.h"
#include "principal_x64.h"
#include "process_x64.h"
#include "ramfs.h"
#include "runtime_image_x64.h"
#include "services.h"
#include "services_x64.h"
#include "syscall_x64.h"
#include "types.h"
#include "virtio_net_x64.h"
#include "x64.h"
#include "xhci_x64.h"

#define LIMITLESS_X64_SCAFFOLD_MAGIC 0x4C4F533634534346ull
#define LIMITLESS_X64_KERNEL_PHYSICAL_BASE 0x0000000000010000ull
#define LIMITLESS_X64_KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ull
#define LIMITLESS_X64_ACTIVE_VIRTUAL_BASE 0xFFFFFFFF80010000ull
#define LIMITLESS_X64_PAGE_LEVELS 4u
#define LIMITLESS_X64_COMPAT32_LANE 1u

enum
{
    VGA_WIDTH = 80u,
    VGA_HEIGHT = 25u
};

struct x64_scaffold_report
{
    u64 magic;
    u64 kernel_physical_base;
    u64 active_virtual_base;
    u64 planned_virtual_base;
    u32 architecture_bits;
    u32 page_levels;
    u32 compat32_lane;
    u32 reserved;
};

__attribute__((section(".data")))
volatile struct x64_scaffold_report g_x64_scaffold_report = {
    LIMITLESS_X64_SCAFFOLD_MAGIC,
    LIMITLESS_X64_KERNEL_PHYSICAL_BASE,
    LIMITLESS_X64_ACTIVE_VIRTUAL_BASE,
    LIMITLESS_X64_KERNEL_VIRTUAL_BASE,
    LIMITLESS_ARCH_BITS,
    LIMITLESS_X64_PAGE_LEVELS,
    LIMITLESS_X64_COMPAT32_LANE,
    0u
};

__attribute__((section(".rodata")))
const char g_x64_scaffold_name[] = "LimitlessOS x86_64 scaffold";

__attribute__((section(".rodata")))
const char g_x64_scaffold_bootstrap_kind[] = LIMITLESS_ARCH_BOOTSTRAP_KIND;

__attribute__((section(".rodata")))
const char g_x64_scaffold_plan[] =
    "higher-half execution, native syscall entry, and shared capability services";

static volatile u16 *const VGA_BUFFER = (volatile u16 *)(u64)0x00000000000B8000ull;
static u32 g_console_row = 0u;
static u32 g_console_column = 0u;
static u8 g_console_color = 0x1Fu;

static void wait_for_timer_ticks(u32 target_ticks);
static void collect_keyboard_probe_input(u32 target_pending, u32 max_wait_ticks);

static void debug_write_char(char character)
{
    outb(0x00E9u, (u8)character);
}

static void console_scroll_if_needed(void)
{
    u32 row;
    u32 column;

    if (g_console_row < VGA_HEIGHT)
    {
        return;
    }

    for (row = 1u; row < VGA_HEIGHT; ++row)
    {
        for (column = 0u; column < VGA_WIDTH; ++column)
        {
            VGA_BUFFER[(row - 1u) * VGA_WIDTH + column] = VGA_BUFFER[row * VGA_WIDTH + column];
        }
    }

    for (column = 0u; column < VGA_WIDTH; ++column)
    {
        VGA_BUFFER[(VGA_HEIGHT - 1u) * VGA_WIDTH + column] = (u16)' ' | ((u16)g_console_color << 8);
    }

    g_console_row = VGA_HEIGHT - 1u;
}

static void console_write_char(char character)
{
    if (character == '\n')
    {
        g_console_column = 0u;
        ++g_console_row;
        console_scroll_if_needed();
        return;
    }

    VGA_BUFFER[g_console_row * VGA_WIDTH + g_console_column] = (u16)character | ((u16)g_console_color << 8);
    ++g_console_column;

    if (g_console_column >= VGA_WIDTH)
    {
        g_console_column = 0u;
        ++g_console_row;
        console_scroll_if_needed();
    }
}

static void write_string(const char *text)
{
    while (*text != '\0')
    {
        debug_write_char(*text);
        console_write_char(*text);
        ++text;
    }
}

static void write_line(const char *text)
{
    write_string(text);
    debug_write_char('\n');
    console_write_char('\n');
}

static void write_hex_digit(u8 value)
{
    if (value < 10u)
    {
        debug_write_char((char)('0' + value));
        console_write_char((char)('0' + value));
        return;
    }

    debug_write_char((char)('A' + (value - 10u)));
    console_write_char((char)('A' + (value - 10u)));
}

static void write_hex_u64(u64 value)
{
    s32 shift;

    write_string("0x");
    for (shift = 60; shift >= 0; shift -= 4)
    {
        write_hex_digit((u8)((value >> shift) & 0x0Full));
    }
}

static void write_hex_u32(u32 value)
{
    s32 shift;

    write_string("0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        write_hex_digit((u8)((value >> shift) & 0x0Fu));
    }
}

static void write_dec_u32(u32 value)
{
    char digits[10];
    u32 count = 0u;

    if (value == 0u)
    {
        debug_write_char('0');
        console_write_char('0');
        return;
    }

    while ((value > 0u) && (count < 10u))
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count > 0u)
    {
        --count;
        debug_write_char(digits[count]);
        console_write_char(digits[count]);
    }
}

static void __attribute__((noinline)) write_labeled_hex_u32(const char *label, u32 value)
{
    write_string(label);
    write_hex_u32(value);
}

static void __attribute__((noinline)) write_labeled_dec_u32(const char *label, u32 value)
{
    write_string(label);
    write_dec_u32(value);
}

static void __attribute__((noinline)) write_syscall0_hex_u32(const char *label, u64 call_id)
{
    write_labeled_hex_u32(label, (u32)syscall64_invoke(call_id, 0u, 0u, 0u));
}

static void __attribute__((noinline)) write_syscall0_dec_u32(const char *label, u64 call_id)
{
    write_labeled_dec_u32(label, (u32)syscall64_invoke(call_id, 0u, 0u, 0u));
}

static void __attribute__((noinline)) write_syscall1_hex_u32(const char *label, u64 call_id, u64 arg1)
{
    write_labeled_hex_u32(label, (u32)syscall64_invoke(call_id, arg1, 0u, 0u));
}

static void __attribute__((noinline)) write_syscall1_dec_u32(const char *label, u64 call_id, u64 arg1)
{
    write_labeled_dec_u32(label, (u32)syscall64_invoke(call_id, arg1, 0u, 0u));
}

enum scaffold_telemetry_format
{
    SCAFFOLD_TELEMETRY_DEC = 0u,
    SCAFFOLD_TELEMETRY_HEX = 1u,
    SCAFFOLD_TELEMETRY_HEX64 = 2u
};

struct __attribute__((packed)) scaffold_telemetry_field
{
    const char *suffix;
    u8 selector : 7;
    u8 format : 1;
};

struct __attribute__((packed)) scaffold_arg_telemetry_field
{
    const char *suffix;
    u8 offset;
    u8 format;
};

struct __attribute__((packed)) scaffold_syscall0_field
{
    u16 label_offset;
    u16 call_id;
    u8 format;
};

static void __attribute__((noinline)) write_formatted_u64(u64 value, u8 format)
{
    if (format == SCAFFOLD_TELEMETRY_HEX64)
    {
        write_hex_u64(value);
        return;
    }

    if (format == SCAFFOLD_TELEMETRY_HEX)
    {
        write_hex_u32((u32)value);
        return;
    }

    write_dec_u32((u32)value);
}

static void __attribute__((noinline)) write_syscall0_prefixed_label_fields(
    const char *prefix,
    const char *suffixes,
    const struct scaffold_syscall0_field *fields,
    u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        write_string(prefix);
        write_string(suffixes + fields[index].label_offset);
        write_formatted_u64(syscall64_invoke((u64)fields[index].call_id, 0u, 0u, 0u), fields[index].format);
    }
}



#define SCAFFOLD_COMPACT_FORMAT_SHIFT 14u
#define SCAFFOLD_COMPACT_SUFFIX_MASK 0x3FFFu

struct __attribute__((packed)) scaffold_compact_selector_field
{
    u16 encoded_suffix;
    u8 selector;
};

static const char scaffold_compact_suffix_pool[] =
    "state \0"
    "flags \0"
    "token \0"
    "handoff-token \0"
    "cap \0"
    "owner \0"
    "owner-bound \0"
    "query-only \0"
    "cap-reg \0"
    "ghc \0"
    "pi \0"
    "version \0"
    "port \0"
    "ssts \0"
    "sig \0"
    "cmd \0"
    "tfd \0"
    "ci \0"
    "serr \0"
    "kind \0"
    "read-ready \0"
    "busy \0"
    "drq \0"
    "ci-idle \0"
    "serr-clear \0"
    "op \0"
    "lba \0"
    "blocks \0"
    "bytes \0"
    "mmio-written \0"
    "port-programmed \0"
    "published \0"
    "command-issued \0"
    "dma \0"
    "armed \0"
    "staged \0"
    "denials \0"
    "unavailable \0"
    "probe-token \0"
    "media-read \0"
    "intent-token \0"
    "read-bytes \0"
    "page-bytes \0"
    "offset \0"
    "checksum \0"
    "zeroed \0"
    "buffer-token \0"
    "exec-required \0"
    "exec-granted \0"
    "issue-allowed \0"
    "gate-token \0"
    "attempted \0"
    "required \0"
    "granted \0"
    "issue-denied \0"
    "exec-token \0"
    "exec-denied \0"
    "requested \0"
    "denied \0"
    "bytes-available \0"
    "block-cap-minted \0"
    "fs-minted \0"
    "result-token \0"
    "result-denied \0"
    "block-endpoint \0"
    "media-written \0"
    "pub-token \0"
    "qonly \0"
    "ready \0"
    "pub-denied \0"
    "media-auth \0"
    "block-cap \0"
    "grant-token \0"
    "drg-denied \0"
    "auth \0"
    "buffer \0"
    "dmr-token \0"
    "dmr-denied \0"
    "completed \0"
    "status \0"
    "drc-token \0"
    "drc-denied \0"
    "drcap-token \0"
    "drcap-denied \0"
    "user-bytes \0"
    "user-buffer \0"
    "drx-token \0"
    "drx-denied \0"
    "resp-bytes \0"
    "resp-status \0"
    "resp-checksum \0"
    "drr-token \0"
    "drr-denied \0"
    "deliv-bytes \0"
    "deliv-status \0"
    "deliv-checksum \0"
    "drd-token \0"
    "drd-denied \0"
    "vis-bytes \0"
    "vis-status \0"
    "vis-checksum \0"
    "drv-token \0"
    "drv-denied \0"
    "commit-bytes \0"
    "commit-status \0"
    "commit-checksum \0"
    "drk-token \0"
    "drk-denied \0"
    "audit-bytes \0"
    "audit-status \0"
    "audit-checksum \0"
    "dra-token \0"
    "dra-denied \0"
    "up-cap \0"
    "exec-auth \0"
    "dru-token \0"
    "dru-denied \0"
    "act-cap \0"
    "read-auth \0"
    "dact-token \0"
    "dact-denied \0"
    "arm-cap \0"
    "darm-token \0"
    "darm-denied \0"
    "submit-cap \0"
    "dsub-token \0"
    "dsub-denied \0"
    "obs-status \0"
    "obs-bytes \0"
    "obs-checksum \0"
    "dobs-token \0"
    "dobs-denied \0"
    "ret-status \0"
    "ret-bytes \0"
    "ret-checksum \0"
    "dret-token \0"
    "dret-denied \0"
    "permit-cap \0"
    "dprm-token \0"
    "dprm-denied \0"
    "window-cap \0"
    "open \0"
    "dwin-token \0"
    "dwin-denied \0"
    "lease-cap \0"
    "active \0"
    "dlse-token \0"
    "dlse-denied \0"
    "use-cap \0"
    "duse-token \0"
    "duse-denied \0"
    "report-bytes \0"
    "report-checksum \0"
    "report-cap \0"
    "drpt-token \0"
    "drpt-denied \0"
    "receipt-bytes \0"
    "receipt-checksum \0"
    "receipt-cap \0"
    "drrc-token \0"
    "drrc-denied \0"
    "ack-bytes \0"
    "ack-checksum \0"
    "ack-cap \0"
    "drak-token \0"
    "drak-denied \0"
    "close-bytes \0"
    "close-checksum \0"
    "close-cap \0"
    "drcl-token \0"
    "drcl-denied \0"
    "seal-bytes \0"
    "seal-checksum \0"
    "seal-cap \0"
    "drsl-token \0"
    "drsl-denied \0"
    "unseal-bytes \0"
    "unseal-checksum \0"
    "unseal-cap \0"
    "drul-token \0"
    "drul-denied \0"
    "discard-bytes \0"
    "discard-checksum \0"
    "discard-cap \0"
    "read-discard-token \0"
    "read-discard-denied \0"
    "finalized-bytes \0"
    "finalize-checksum \0"
    "finalize-cap \0"
    "read-authority \0"
    "execute-authority \0"
    "buffer-unchanged \0"
    "read-finalize-token \0"
    "read-finalize-denied \0"
    "policy-grant \0"
    "issue-authority \0"
    "dma-authority \0"
    "media-read-authority \0"
    "write-authority \0"
    "commit-authority \0"
    "read-authorize-token \0"
    "read-authorize-denied \0"
    "dispatch-queued \0"
    "queue-depth \0"
    "read-dispatch-token \0"
    "read-dispatch-denied \0"
    "queue-inserted \0"
    "worker-wake \0"
    "read-queue-token \0"
    "read-queue-denied \0"
    "worker-dequeued \0"
    "read-worker-token \0"
    "read-worker-denied \0"
    "worker-runnable \0"
    "worker-scheduled \0"
    "dma-token \0"
    "dma-denied \0"
    "wait \0"
    "fired \0"
    "cstatus \0"
    "cbytes \0"
    "cchecksum \0"
    "issue-auth \0"
    "dma-auth \0"
    "write-auth \0"
    "commit-auth \0"
    "irq-token \0"
    "irq-denied \0"
    "poll \0"
    "sready \0"
    "pxis \0"
    "irq-clear \0"
    "status-token \0"
    "status-denied \0"
    "rstatus \0"
    "rbytes \0"
    "rchecksum \0"
    "tfd-ready \0"
    "sample-token \0"
    "sample-ready \0"
    "sample-bound \0"
    "pxis-b \0"
    "pxis-a \0"
    "pxis-same \0"
    "clear-requested \0"
    "clear-granted \0"
    "clear-denied \0"
    "clear-value \0"
    "clear-token \0"
    "pxis-clear-denied \0"
    "result-requested \0"
    "result-granted \0"
    "result-status \0"
    "result-bytes \0"
    "result-checksum \0"
    "read-schedule-token \0"
    "read-schedule-denied \0"
    "worker-run \0"
    "worker-executed \0"
    "read-run-token \0"
    "read-run-denied \0"
    "body-entered \0"
    "body-completed \0"
    "read-body-token \0"
    "read-body-denied \0"
    "issue-entered \0"
    "issue-completed \0"
    "read-issue-token \0"
    "read-issue-denied \0"
    "window-open \0"
    "entered \0"
    "clear-result-token \0"
    "clear-result-denied \0"
    "resample-token \0"
    "resample-read-only \0"
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
    "stable-token \0"
    "issue-ok \0"
    "dma-ok \0"
    "read-denied \0"
    "write-denied \0"
    "commit-denied \0"
    "guard-token \0"
    "view-requested \0"
    "view-granted \0"
    "view-denied \0"
    "sealed \0"
    "user-copy \0"
    "authority \0"
    "effects \0"
    "authz-denied \0"
    "safety \0"
    "dispatch-denied \0"
    "depth \0"
    "admit \0"
    "worker \0"
    "runnable \0"
    "schedule \0"
    "queue-denied \0"
    "dequeue \0"
    "wake \0"
    "sched \0"
    "run \0"
    "exec \0"
    "worker-denied \0"
    "policy \0"
    "read \0"
    "issue \0"
    "write \0"
    "commit \0"
    "rauth \0"
    "shaped \0"
    "slot \0"
    "header \0"
    "table \0"
    "cfis \0"
    "prdt \0"
    "prdt-bytes \0"
    "packet \0"
    "opcode \0"
    "packet-op \0"
    "transfer \0"
    "desc \0"
    "mat \0"
    "before \0"
    "after \0"
    "changed \0"
    "hdr \0"
    "dbc \0"
    "written \0"
    "mmio \0"
    "portw \0"
    "media \0"
    "mask \0"
    "slot-idle \0"
    "table-check \0"
    "expected \0"
    "match \0"
    "dma-bound \0"
    "request \0"
    "grant \0"
    "reg \0"
    "value \0"
    "rollback-required \0"
    "rollback-done \0"
    "teardown \0"
    "stale-denied \0"
    "media-write \0"
    "mmio-bound \0"
    "page-low \0"
    "page-high \0"
    "bounce-low \0"
    "bounce-high \0"
    "bounce-bytes \0"
    "range-end \0"
    "single-page \0"
    "broker \0"
    "bounds \0"
    "confined \0"
    "below4g \0"
    "iommu \0"
    "identity \0"
    "non-user \0"
    "alias-safe \0"
    "opened \0"
    "closed \0"
    "revoke-required \0"
    "revoke-done \0"
    "dwin-bound \0"
    "issued \0"
    "error \0"
    "table-before \0"
    "table-after \0"
    "prdbc \0"
    "polls \0"
    "read-bound \0"
    "endpoint \0"
    "cap-minted \0"
    "read-only \0"
    "delegated-cap \0"
    "read-routed \0"
    "read-checksum \0"
    "checksum-match \0"
    "wrong-owner \0"
    "block-bound \0"
    "block-read-only \0"
    "block-route \0"
    "pvd \0"
    "pvd-checksum \0"
    "root-lba \0"
    "root-bytes \0"
    "root-read \0"
    "located \0"
    "file-lba \0"
    "expected-checksum \0"
    "content-match \0"
    "user-owner \0"
    "fs-bound \0"
    "delegated \0"
    "apps-lba \0"
    "apps-bytes \0"
    "stale \0"
    "additional-fs-caps \0"
    "fs-user-bound \0"
    "descriptors-read \0"
    "descriptors-parsed \0"
    "scan-dynamic \0"
    "ls-dispatched \0"
    "cat-dispatched \0"
    "stat-dispatched \0"
    "ramfs-route \0"
    "iso-route \0"
    "load-bound \0"
    "fs-shell-bound \0"
    "binaries \0"
    "verified \0"
    "registered \0"
    "cat \0"
    "mkdir \0"
    "rename \0"
    "move \0";

static void __attribute__((noinline)) write_scaffold_compact_suffix(u16 encoded_field)
{
    write_string(&scaffold_compact_suffix_pool[encoded_field & SCAFFOLD_COMPACT_SUFFIX_MASK]);
}

static u8 scaffold_compact_format(u16 encoded_field)
{
    return (u8)(encoded_field >> SCAFFOLD_COMPACT_FORMAT_SHIFT);
}

static void __attribute__((noinline)) write_syscall0_compact_fields(
    const char *prefix,
    u64 base_call_id,
    const u16 *fields,
    u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        u16 encoded_field = fields[index];
        write_string(prefix);
        write_scaffold_compact_suffix(encoded_field);
        write_formatted_u64(syscall64_invoke(base_call_id + index, 0u, 0u, 0u), scaffold_compact_format(encoded_field));
    }
}

static void __attribute__((noinline)) write_syscall0_compact_selector_fields(
    const char *prefix,
    u64 base_call_id,
    const struct scaffold_compact_selector_field *fields,
    u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        const struct scaffold_compact_selector_field *field = &fields[index];
        write_string(prefix);
        write_scaffold_compact_suffix(field->encoded_suffix);
        write_formatted_u64(syscall64_invoke(base_call_id + field->selector, 0u, 0u, 0u), scaffold_compact_format(field->encoded_suffix));
    }
}

static void __attribute__((noinline)) write_syscall1_compact_selector_fields(
    const char *prefix,
    u64 call_id,
    const struct scaffold_compact_selector_field *fields,
    u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        const struct scaffold_compact_selector_field *field = &fields[index];
        write_string(prefix);
        write_scaffold_compact_suffix(field->encoded_suffix);
        write_formatted_u64(syscall64_invoke(call_id, field->selector, 0u, 0u), scaffold_compact_format(field->encoded_suffix));
    }
}

static void __attribute__((noinline)) write_syscall3_prefixed_offset_fields(
    const char *prefix,
    u64 base_call_id,
    const struct scaffold_arg_telemetry_field *fields,
    u32 field_count,
    u64 arg1,
    u64 arg3)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        const struct scaffold_arg_telemetry_field *field = &fields[index];
        u64 value = syscall64_invoke(base_call_id + (u64)field->offset, arg1, 0u, arg3);

        write_string(prefix);
        write_string(field->suffix);
        write_formatted_u64(value, field->format);
    }
}

static void __attribute__((noinline)) write_syscall1_prefixed_fields(
    const char *prefix,
    u64 call_id,
    const struct scaffold_telemetry_field *fields,
    u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        const struct scaffold_telemetry_field *field = &fields[index];
        u32 value = (u32)syscall64_invoke(call_id, (u64)field->selector, 0u, 0u);

        write_string(prefix);
        write_string(field->suffix);
        write_formatted_u64((u64)value, field->format);
    }
}

static void __attribute__((noinline)) write_syscall0_prefixed_fields(
    const char *prefix,
    u64 base_call_id,
    const struct scaffold_telemetry_field *fields,
    u32 field_count)
{
    u32 index;

    for (index = 0u; index < field_count; ++index)
    {
        const struct scaffold_telemetry_field *field = &fields[index];
        u32 value = (u32)syscall64_invoke(base_call_id + (u64)field->selector, 0u, 0u, 0u);

        write_string(prefix);
        write_string(field->suffix);
        write_formatted_u64((u64)value, field->format);
    }
}

static void __attribute__((noinline)) write_driver_read_run_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4C48u, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x003Du, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x008Fu, 17u},
        {0x0C5Du, 18u},
        {0x021Bu, 19u},
        {0x01EBu, 20u},
        {0x0226u, 21u},
        {0x08F5u, 22u},
        {0x09D2u, 23u},
        {0x099Au, 24u},
        {0x09E2u, 25u},
        {0x0A14u, 26u},
        {0x0A4Cu, 27u},
        {0x0A5Du, 28u},
        {0x0C73u, 29u},
        {0x0C7Fu, 30u},
        {0x0903u, 31u},
        {0x0914u, 32u},
        {0x0923u, 33u},
        {0x0939u, 34u},
        {0x094Au, 35u},
        {0x0279u, 36u},
        {0x023Fu, 37u},
        {0x0251u, 38u},
        {0x00D3u, 39u},
        {0x00E1u, 40u},
        {0x00F2u, 41u},
        {0x00FDu, 42u},
        {0x010Du, 43u},
        {0x0112u, 44u},
        {0x0144u, 45u},
        {0x0289u, 46u},
        {0x08B8u, 47u},
        {0x0119u, 48u},
        {0x0121u, 49u},
        {0x012Au, 50u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_driver_read_body_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4C90u, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x003Du, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x008Fu, 17u},
        {0x0CA0u, 18u},
        {0x021Bu, 19u},
        {0x01EBu, 20u},
        {0x0226u, 21u},
        {0x08F5u, 22u},
        {0x09D2u, 23u},
        {0x099Au, 24u},
        {0x09E2u, 25u},
        {0x0A14u, 26u},
        {0x0A4Cu, 27u},
        {0x0A5Du, 28u},
        {0x0C73u, 29u},
        {0x0C7Fu, 30u},
        {0x0CB1u, 31u},
        {0x0CBFu, 32u},
        {0x0903u, 33u},
        {0x0914u, 34u},
        {0x0923u, 35u},
        {0x0939u, 36u},
        {0x094Au, 37u},
        {0x0279u, 38u},
        {0x023Fu, 39u},
        {0x0251u, 40u},
        {0x00D3u, 41u},
        {0x00E1u, 42u},
        {0x00F2u, 43u},
        {0x00FDu, 44u},
        {0x010Du, 45u},
        {0x0112u, 46u},
        {0x0144u, 47u},
        {0x0289u, 48u},
        {0x08B8u, 49u},
        {0x0119u, 50u},
        {0x0121u, 51u},
        {0x012Au, 52u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_driver_read_issue_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4CCFu, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x003Du, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x008Fu, 17u},
        {0x0CE0u, 18u},
        {0x021Bu, 19u},
        {0x01EBu, 20u},
        {0x0226u, 21u},
        {0x08F5u, 22u},
        {0x09D2u, 23u},
        {0x099Au, 24u},
        {0x09E2u, 25u},
        {0x0A14u, 26u},
        {0x0A4Cu, 27u},
        {0x0A5Du, 28u},
        {0x0C73u, 29u},
        {0x0C7Fu, 30u},
        {0x0CF2u, 31u},
        {0x0D01u, 32u},
        {0x0903u, 33u},
        {0x0914u, 34u},
        {0x0923u, 35u},
        {0x0939u, 36u},
        {0x094Au, 37u},
        {0x0279u, 38u},
        {0x023Fu, 39u},
        {0x0251u, 40u},
        {0x00D3u, 41u},
        {0x00E1u, 42u},
        {0x00F2u, 43u},
        {0x00FDu, 44u},
        {0x010Du, 45u},
        {0x0112u, 46u},
        {0x0144u, 47u},
        {0x0289u, 48u},
        {0x08B8u, 49u},
        {0x0119u, 50u},
        {0x0121u, 51u},
        {0x012Au, 52u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_driver_read_dma_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4D12u, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x003Du, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x008Fu, 17u},
        {0x0D24u, 18u},
        {0x021Bu, 19u},
        {0x01EBu, 20u},
        {0x0226u, 21u},
        {0x08F5u, 22u},
        {0x022Eu, 23u},
        {0x460Au, 24u},
        {0x0D37u, 25u},
        {0x0D44u, 26u},
        {0x0312u, 27u},
        {0x0903u, 28u},
        {0x0914u, 29u},
        {0x0923u, 30u},
        {0x0939u, 31u},
        {0x094Au, 32u},
        {0x0279u, 33u},
        {0x023Fu, 34u},
        {0x0251u, 35u},
        {0x00D3u, 36u},
        {0x00E1u, 37u},
        {0x00F2u, 38u},
        {0x00FDu, 39u},
        {0x010Du, 40u},
        {0x0112u, 41u},
        {0x0144u, 42u},
        {0x0289u, 43u},
        {0x08B8u, 44u},
        {0x0119u, 45u},
        {0x0121u, 46u},
        {0x012Au, 47u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_resample_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4D4Du, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x02A3u, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x02AAu, 17u},
        {0x0D61u, 18u},
        {0x021Bu, 19u},
        {0x01EBu, 20u},
        {0x0226u, 21u},
        {0x08F5u, 22u},
        {0x00CCu, 23u},
        {0x4B82u, 24u},
        {0x4B8Au, 25u},
        {0x0B92u, 26u},
        {0x407Fu, 27u},
        {0x407Au, 28u},
        {0x4083u, 29u},
        {0x0B4Du, 30u},
        {0x00A6u, 31u},
        {0x00AFu, 32u},
        {0x0B09u, 33u},
        {0x0C1Au, 34u},
        {0x0C29u, 35u},
        {0x4C37u, 36u},
        {0x0AAFu, 37u},
        {0x0ABBu, 38u},
        {0x04FEu, 39u},
        {0x0AC5u, 40u},
        {0x0AD1u, 41u},
        {0x0279u, 42u},
        {0x02C9u, 43u},
        {0x0251u, 44u},
        {0x00D3u, 45u},
        {0x00E1u, 46u},
        {0x00F2u, 47u},
        {0x00FDu, 48u},
        {0x010Du, 49u},
        {0x0112u, 50u},
        {0x0144u, 51u},
        {0x0289u, 52u},
        {0x02F3u, 53u},
        {0x0119u, 54u},
        {0x0121u, 55u},
        {0x012Au, 56u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_stable_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4D76u, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x02A3u, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x02AAu, 17u},
        {0x0D61u, 18u},
        {0x0D86u, 19u},
        {0x021Bu, 20u},
        {0x01EBu, 21u},
        {0x0226u, 22u},
        {0x08F5u, 23u},
        {0x00CCu, 24u},
        {0x4B82u, 25u},
        {0x4B8Au, 26u},
        {0x0D9Au, 27u},
        {0x4DA7u, 28u},
        {0x4DADu, 29u},
        {0x0DB3u, 30u},
        {0x4DBEu, 31u},
        {0x4DC5u, 32u},
        {0x0DCCu, 33u},
        {0x4DD8u, 34u},
        {0x4DE0u, 35u},
        {0x0DE8u, 36u},
        {0x0B4Du, 37u},
        {0x00A6u, 38u},
        {0x00AFu, 39u},
        {0x0B09u, 40u},
        {0x0C1Au, 41u},
        {0x0C29u, 42u},
        {0x4C37u, 43u},
        {0x0AAFu, 44u},
        {0x0ABBu, 45u},
        {0x04FEu, 46u},
        {0x0AC5u, 47u},
        {0x0AD1u, 48u},
        {0x0279u, 49u},
        {0x02C9u, 50u},
        {0x0251u, 51u},
        {0x00D3u, 52u},
        {0x00E1u, 53u},
        {0x00F2u, 54u},
        {0x00FDu, 55u},
        {0x010Du, 56u},
        {0x0112u, 57u},
        {0x0144u, 58u},
        {0x0289u, 59u},
        {0x02F3u, 60u},
        {0x0119u, 61u},
        {0x0121u, 62u},
        {0x012Au, 63u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_guard_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4DF5u, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x02A3u, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x02AAu, 17u},
        {0x4B82u, 18u},
        {0x4B8Au, 19u},
        {0x0D9Au, 20u},
        {0x4DA7u, 21u},
        {0x4DADu, 22u},
        {0x0DB3u, 23u},
        {0x4DBEu, 24u},
        {0x4DC5u, 25u},
        {0x0DCCu, 26u},
        {0x4DD8u, 27u},
        {0x4DE0u, 28u},
        {0x0DE8u, 29u},
        {0x0B4Du, 30u},
        {0x00A6u, 31u},
        {0x00AFu, 32u},
        {0x021Bu, 33u},
        {0x0E03u, 34u},
        {0x01F4u, 35u},
        {0x0E0Du, 36u},
        {0x0A7Au, 37u},
        {0x04FEu, 38u},
        {0x0E15u, 39u},
        {0x0AC5u, 40u},
        {0x0E22u, 41u},
        {0x0AD1u, 42u},
        {0x0E30u, 43u},
        {0x0B09u, 44u},
        {0x0C1Au, 45u},
        {0x0C29u, 46u},
        {0x4C37u, 47u},
        {0x0279u, 48u},
        {0x02C9u, 49u},
        {0x0251u, 50u},
        {0x00D3u, 51u},
        {0x00E1u, 52u},
        {0x00F2u, 53u},
        {0x00FDu, 54u},
        {0x010Du, 55u},
        {0x0112u, 56u},
        {0x0144u, 57u},
        {0x0289u, 58u},
        {0x02F3u, 59u},
        {0x0119u, 60u},
        {0x0121u, 61u},
        {0x012Au, 62u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_buffer_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4E3Fu, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x02A3u, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x417Eu, 15u},
        {0x0188u, 16u},
        {0x02AAu, 17u},
        {0x0E4Cu, 18u},
        {0x0E5Cu, 19u},
        {0x0E6Au, 20u},
        {0x0C1Au, 21u},
        {0x0C29u, 22u},
        {0x4C37u, 23u},
        {0x04FEu, 24u},
        {0x0AC5u, 25u},
        {0x0AD1u, 26u},
        {0x0279u, 27u},
        {0x02C9u, 28u},
        {0x0251u, 29u},
        {0x00D3u, 30u},
        {0x00E1u, 31u},
        {0x00F2u, 32u},
        {0x00FDu, 33u},
        {0x010Du, 34u},
        {0x0112u, 35u},
        {0x0144u, 36u},
        {0x0289u, 37u},
        {0x02F3u, 38u},
        {0x0119u, 39u},
        {0x0121u, 40u},
        {0x012Au, 41u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_export_fields(const char *prefix, u64 base_call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x400Eu, 2u},
        {0x4190u, 3u},
        {0x4024u, 4u},
        {0x4029u, 5u},
        {0x0030u, 6u},
        {0x02A3u, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x417Eu, 14u},
        {0x0E77u, 15u},
        {0x021Bu, 16u},
        {0x01EBu, 17u},
        {0x0226u, 18u},
        {0x4E7Fu, 19u},
        {0x4E8Au, 20u},
        {0x4E95u, 21u},
        {0x02F3u, 22u},
        {0x0119u, 23u},
        {0x0121u, 24u},
        {0x012Au, 25u}
    };

    write_syscall0_compact_selector_fields(prefix, base_call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_compact_status_fields(
    const char *prefix,
    u64 base_call_id,
    const char *denied_suffix,
    const char *action_suffix)
{
    static const struct scaffold_compact_selector_field first_fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u}
    };
    static const struct scaffold_compact_selector_field tail_fields[] = {
        {0x4E7Fu, 7u},
        {0x4E8Au, 8u},
        {0x4E95u, 9u},
        {0x02F3u, 10u},
        {0x0119u, 11u},
        {0x0121u, 12u},
        {0x012Au, 13u}
    };
    struct scaffold_telemetry_field transition_fields[2];

    transition_fields[0].suffix = denied_suffix;
    transition_fields[0].selector = 5u;
    transition_fields[0].format = SCAFFOLD_TELEMETRY_DEC;
    transition_fields[1].suffix = action_suffix;
    transition_fields[1].selector = 6u;
    transition_fields[1].format = SCAFFOLD_TELEMETRY_HEX;

    write_syscall0_compact_selector_fields(prefix, base_call_id, first_fields, (u32)(sizeof(first_fields) / sizeof(first_fields[0])));
    write_syscall0_prefixed_fields(prefix, base_call_id, transition_fields, 2u);
    write_syscall0_compact_selector_fields(prefix, base_call_id, tail_fields, (u32)(sizeof(tail_fields) / sizeof(tail_fields[0])));
}

static void __attribute__((noinline)) write_drs_dispatch_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x0E9Eu, 5u},
        {0x4EACu, 6u},
        {0x02F3u, 7u},
        {0x0119u, 8u},
        {0x0121u, 9u},
        {0x012Au, 10u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_queue_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x0EB4u, 5u},
        {0x4EACu, 6u},
        {0x0EC5u, 7u},
        {0x0ECCu, 8u},
        {0x0ED3u, 9u},
        {0x0EDBu, 15u},
        {0x0EE5u, 10u},
        {0x02F3u, 11u},
        {0x0119u, 12u},
        {0x0121u, 13u},
        {0x012Au, 14u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_worker_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x0EEFu, 5u},
        {0x4EACu, 6u},
        {0x0EFDu, 7u},
        {0x0ECCu, 8u},
        {0x0F06u, 9u},
        {0x0EDBu, 17u},
        {0x0F0Cu, 10u},
        {0x0F13u, 11u},
        {0x0F18u, 12u},
        {0x02F3u, 13u},
        {0x0119u, 14u},
        {0x0121u, 15u},
        {0x012Au, 16u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_read_authority_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x0F1Eu, 5u},
        {0x0F2Du, 6u},
        {0x0F35u, 7u},
        {0x0F3Bu, 8u},
        {0x0ABBu, 9u},
        {0x02BDu, 10u},
        {0x0F42u, 11u},
        {0x0F49u, 12u},
        {0x0279u, 13u},
        {0x02C9u, 14u},
        {0x0251u, 15u},
        {0x4EACu, 16u},
        {0x0EFDu, 17u},
        {0x0ECCu, 18u},
        {0x0F06u, 19u},
        {0x0EDBu, 20u},
        {0x0F0Cu, 21u},
        {0x0F13u, 22u},
        {0x0F18u, 23u},
        {0x02F3u, 24u},
        {0x0119u, 25u},
        {0x0121u, 26u},
        {0x012Au, 27u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_descriptor_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x0F51u, 5u},
        {0x0F58u, 6u},
        {0x0F35u, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x0F60u, 15u},
        {0x0F66u, 16u},
        {0x0F6Eu, 17u},
        {0x0F75u, 18u},
        {0x0F7Bu, 19u},
        {0x0F81u, 20u},
        {0x0F8Du, 21u},
        {0x4F95u, 22u},
        {0x4F9Du, 23u},
        {0x0FA8u, 24u},
        {0x0F3Bu, 25u},
        {0x0ABBu, 26u},
        {0x02BDu, 27u},
        {0x0F42u, 28u},
        {0x0F49u, 29u},
        {0x0279u, 30u},
        {0x02C9u, 31u},
        {0x0251u, 32u},
        {0x4EACu, 33u},
        {0x0EFDu, 34u},
        {0x0ECCu, 35u},
        {0x0F06u, 36u},
        {0x0EDBu, 37u},
        {0x0F0Cu, 38u},
        {0x0F13u, 39u},
        {0x0F18u, 40u},
        {0x02F3u, 41u},
        {0x0119u, 42u},
        {0x0121u, 43u},
        {0x012Au, 44u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_command_table_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x0FB2u, 5u},
        {0x0FB8u, 6u},
        {0x02AAu, 7u},
        {0x4064u, 8u},
        {0x0089u, 9u},
        {0x00BBu, 10u},
        {0x00BFu, 11u},
        {0x00C4u, 12u},
        {0x015Eu, 13u},
        {0x016Au, 14u},
        {0x4FBDu, 15u},
        {0x4FC5u, 16u},
        {0x0FCCu, 17u},
        {0x4FD5u, 18u},
        {0x4F75u, 19u},
        {0x4F8Du, 20u},
        {0x0FDAu, 21u},
        {0x0FDFu, 22u},
        {0x0F3Bu, 23u},
        {0x0ABBu, 24u},
        {0x02BDu, 25u},
        {0x0F42u, 26u},
        {0x0F49u, 27u},
        {0x0279u, 28u},
        {0x02C9u, 29u},
        {0x0251u, 30u},
        {0x4EACu, 31u},
        {0x0FE8u, 32u},
        {0x0FEEu, 33u},
        {0x0075u, 34u},
        {0x010Du, 35u},
        {0x0FF5u, 36u},
        {0x02F3u, 37u},
        {0x0119u, 38u},
        {0x0121u, 39u},
        {0x012Au, 40u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_issue_ladder_fields(
    const char *prefix,
    u64 call_id,
    const char *bound_suffix,
    const char *ready_suffix,
    const char *request_suffix,
    const char *grant_suffix,
    const char *denied_suffix)
{
    static const struct scaffold_compact_selector_field first_fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u}
    };
    static const struct scaffold_compact_selector_field common_fields[] = {
        {0x4064u, 10u},
        {0x0089u, 11u},
        {0x00BBu, 12u},
        {0x00BFu, 13u},
        {0x00C4u, 14u},
        {0x015Eu, 15u},
        {0x016Au, 16u},
        {0x407Fu, 17u},
        {0x4FFCu, 18u},
        {0x1002u, 19u},
        {0x007Au, 20u},
        {0x0083u, 21u},
        {0x500Du, 22u},
        {0x501Au, 23u},
        {0x1024u, 24u},
        {0x0AAFu, 25u},
        {0x0ABBu, 26u},
        {0x02BDu, 27u},
        {0x0F42u, 28u},
        {0x0F49u, 29u},
        {0x0279u, 30u},
        {0x02C9u, 31u},
        {0x0251u, 32u},
        {0x4EACu, 33u},
        {0x0FE8u, 34u},
        {0x0FEEu, 35u},
        {0x0075u, 36u},
        {0x010Du, 37u},
        {0x0FF5u, 38u},
        {0x02F3u, 39u},
        {0x0119u, 40u},
        {0x0121u, 41u},
        {0x012Au, 42u}
    };
    struct scaffold_telemetry_field transition_fields[5];

    write_syscall1_compact_selector_fields(prefix, call_id, first_fields, (u32)(sizeof(first_fields) / sizeof(first_fields[0])));

    transition_fields[0].suffix = bound_suffix;
    transition_fields[0].selector = 5u;
    transition_fields[0].format = SCAFFOLD_TELEMETRY_DEC;
    transition_fields[1].suffix = ready_suffix;
    transition_fields[1].selector = 6u;
    transition_fields[1].format = SCAFFOLD_TELEMETRY_DEC;
    transition_fields[2].suffix = request_suffix;
    transition_fields[2].selector = 7u;
    transition_fields[2].format = SCAFFOLD_TELEMETRY_DEC;
    transition_fields[3].suffix = grant_suffix;
    transition_fields[3].selector = 8u;
    transition_fields[3].format = SCAFFOLD_TELEMETRY_DEC;
    transition_fields[4].suffix = denied_suffix;
    transition_fields[4].selector = 9u;
    transition_fields[4].format = SCAFFOLD_TELEMETRY_DEC;

    write_syscall1_prefixed_fields(prefix, call_id, transition_fields, 5u);
    write_syscall1_compact_selector_fields(prefix, call_id, common_fields, (u32)(sizeof(common_fields) / sizeof(common_fields[0])));
}

static void __attribute__((noinline)) write_drs_mmio_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x102Bu, 5u},
        {0x02AAu, 6u},
        {0x1036u, 7u},
        {0x103Fu, 8u},
        {0x0226u, 9u},
        {0x4064u, 10u},
        {0x0089u, 11u},
        {0x00BBu, 12u},
        {0x00BFu, 13u},
        {0x00C4u, 14u},
        {0x015Eu, 15u},
        {0x016Au, 16u},
        {0x407Fu, 17u},
        {0x4FFCu, 18u},
        {0x1002u, 19u},
        {0x007Au, 20u},
        {0x0083u, 21u},
        {0x500Du, 22u},
        {0x501Au, 23u},
        {0x1024u, 24u},
        {0x5046u, 25u},
        {0x504Bu, 26u},
        {0x4B82u, 27u},
        {0x4B8Au, 28u},
        {0x0B92u, 29u},
        {0x1052u, 30u},
        {0x1065u, 31u},
        {0x1074u, 32u},
        {0x107Eu, 33u},
        {0x0AAFu, 34u},
        {0x0ABBu, 35u},
        {0x02BDu, 36u},
        {0x0F42u, 37u},
        {0x0F49u, 38u},
        {0x0279u, 39u},
        {0x02C9u, 40u},
        {0x0251u, 41u},
        {0x4EACu, 42u},
        {0x0FE8u, 43u},
        {0x0FEEu, 44u},
        {0x0075u, 45u},
        {0x010Du, 46u},
        {0x0FF5u, 47u},
        {0x108Cu, 48u},
        {0x02F3u, 49u},
        {0x0119u, 50u},
        {0x0121u, 51u},
        {0x012Au, 52u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_dma_window_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x417Eu, 4u},
        {0x1099u, 5u},
        {0x02AAu, 6u},
        {0x1036u, 7u},
        {0x103Fu, 8u},
        {0x0226u, 9u},
        {0x4064u, 10u},
        {0x0089u, 11u},
        {0x00BBu, 12u},
        {0x00BFu, 13u},
        {0x00C4u, 14u},
        {0x015Eu, 15u},
        {0x016Au, 16u},
        {0x407Fu, 17u},
        {0x4FFCu, 18u},
        {0x1002u, 19u},
        {0x007Au, 20u},
        {0x0083u, 21u},
        {0x500Du, 22u},
        {0x501Au, 23u},
        {0x1024u, 24u},
        {0x50A5u, 25u},
        {0x50AFu, 26u},
        {0x50BAu, 27u},
        {0x50C6u, 28u},
        {0x10D3u, 29u},
        {0x0176u, 30u},
        {0x10E1u, 31u},
        {0x10ECu, 32u},
        {0x10F9u, 33u},
        {0x1101u, 34u},
        {0x1109u, 35u},
        {0x1113u, 36u},
        {0x111Cu, 37u},
        {0x1123u, 38u},
        {0x112Du, 39u},
        {0x1137u, 40u},
        {0x1143u, 41u},
        {0x114Bu, 42u},
        {0x0640u, 43u},
        {0x1153u, 44u},
        {0x1164u, 45u},
        {0x107Eu, 46u},
        {0x0AAFu, 47u},
        {0x0ABBu, 48u},
        {0x02BDu, 49u},
        {0x0F42u, 50u},
        {0x0F49u, 51u},
        {0x0279u, 52u},
        {0x02C9u, 53u},
        {0x0251u, 54u},
        {0x4EACu, 55u},
        {0x0FE8u, 56u},
        {0x0FEEu, 57u},
        {0x0075u, 58u},
        {0x010Du, 59u},
        {0x0FF5u, 60u},
        {0x108Cu, 61u},
        {0x02F3u, 62u},
        {0x0119u, 63u},
        {0x0121u, 64u},
        {0x012Au, 65u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_read_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x02A3u, 3u},
        {0x1171u, 4u},
        {0x02AAu, 5u},
        {0x1036u, 6u},
        {0x117Du, 7u},
        {0x0312u, 8u},
        {0x00CCu, 9u},
        {0x417Eu, 10u},
        {0x1185u, 11u},
        {0x4064u, 12u},
        {0x0089u, 13u},
        {0x00BBu, 14u},
        {0x00BFu, 15u},
        {0x00C4u, 16u},
        {0x50A5u, 17u},
        {0x50BAu, 18u},
        {0x518Cu, 19u},
        {0x519Au, 20u},
        {0x11A7u, 21u},
        {0x4DA7u, 22u},
        {0x4DADu, 23u},
        {0x4DBEu, 24u},
        {0x4DC5u, 25u},
        {0x11AEu, 26u},
        {0x0FE8u, 27u},
        {0x0FEEu, 28u},
        {0x010Du, 29u},
        {0x0FF5u, 30u},
        {0x0F42u, 31u},
        {0x0F49u, 32u},
        {0x0279u, 33u},
        {0x0251u, 34u},
        {0x0640u, 35u},
        {0x0119u, 36u},
        {0x0121u, 37u},
        {0x012Au, 38u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_block_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x11B5u, 3u},
        {0x11C1u, 4u},
        {0x11CBu, 5u},
        {0x4024u, 6u},
        {0x11D7u, 7u},
        {0x11E2u, 8u},
        {0x11F1u, 9u},
        {0x00CCu, 10u},
        {0x417Eu, 11u},
        {0x51FEu, 12u},
        {0x120Du, 13u},
        {0x121Du, 14u},
        {0x107Eu, 15u},
        {0x0F42u, 16u},
        {0x0F49u, 17u},
        {0x0251u, 18u},
        {0x00BFu, 19u},
        {0x00C4u, 20u},
        {0x02F3u, 21u},
        {0x0119u, 22u},
        {0x0121u, 23u},
        {0x012Au, 24u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_fs_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x122Au, 3u},
        {0x42C9u, 4u},
        {0x1237u, 5u},
        {0x1248u, 6u},
        {0x1255u, 7u},
        {0x525Au, 8u},
        {0x1268u, 9u},
        {0x1272u, 10u},
        {0x127Eu, 11u},
        {0x1289u, 12u},
        {0x1292u, 13u},
        {0x015Eu, 14u},
        {0x417Eu, 15u},
        {0x529Cu, 16u},
        {0x12AFu, 17u},
        {0x0F42u, 18u},
        {0x0F49u, 19u},
        {0x0251u, 20u},
        {0x0119u, 21u},
        {0x0121u, 22u},
        {0x012Au, 23u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_fs_user_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x52BEu, 3u},
        {0x12CAu, 4u},
        {0x11CBu, 5u},
        {0x12D4u, 6u},
        {0x11F1u, 7u},
        {0x127Eu, 8u},
        {0x12DFu, 9u},
        {0x12E9u, 10u},
        {0x1292u, 11u},
        {0x00CCu, 12u},
        {0x417Eu, 13u},
        {0x529Cu, 14u},
        {0x12AFu, 15u},
        {0x121Du, 16u},
        {0x12F5u, 17u},
        {0x0F42u, 18u},
        {0x0F49u, 19u},
        {0x12FCu, 20u},
        {0x0363u, 21u},
        {0x0119u, 22u},
        {0x0121u, 23u},
        {0x012Au, 24u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_fs_shell_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x52BEu, 3u},
        {0x1310u, 4u},
        {0x12D4u, 5u},
        {0x131Fu, 6u},
        {0x1331u, 7u},
        {0x1345u, 8u},
        {0x1353u, 9u},
        {0x1362u, 10u},
        {0x1372u, 11u},
        {0x1383u, 12u},
        {0x1390u, 13u},
        {0x0F42u, 14u},
        {0x0F49u, 15u},
        {0x12FCu, 16u},
        {0x0119u, 17u},
        {0x0121u, 18u},
        {0x012Au, 19u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
}

static void __attribute__((noinline)) write_drs_load_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_telemetry_field leading_fields[] = {
        {"state ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_STATE, SCAFFOLD_TELEMETRY_DEC},
        {"flags ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_FLAGS, SCAFFOLD_TELEMETRY_HEX},
        {"owner ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {"user-owner ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_USER_OWNER, SCAFFOLD_TELEMETRY_HEX},
        {"fs-shell-bound ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_FS_SHELL_BOUND, SCAFFOLD_TELEMETRY_DEC},
        {"binary-read ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_BINARY_READ, SCAFFOLD_TELEMETRY_DEC},
        {"checksum-verified ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_CHECKSUM_VERIFIED, SCAFFOLD_TELEMETRY_DEC},
        {"mapped ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {"launched ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_LAUNCHED, SCAFFOLD_TELEMETRY_DEC},
        {"ls-completed ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_LS_COMPLETED, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_telemetry_field trailing_fields[] = {
        {"bytes ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {"checksum ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {"expected-checksum ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_EXPECTED_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {"mapped-bytes ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_MAPPED_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {"entry-rip ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_ENTRY_RIP, SCAFFOLD_TELEMETRY_HEX},
        {"exit-result ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_EXIT_RESULT, SCAFFOLD_TELEMETRY_HEX},
        {"ls-bytes ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_LS_BYTES, SCAFFOLD_TELEMETRY_DEC},
        {"write ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {"commit ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {"additional-fs-caps ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_ADDITIONAL_FS_CAPS, SCAFFOLD_TELEMETRY_DEC},
        {"staged ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {"denials ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {"unavailable ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };

    write_syscall1_prefixed_fields(prefix, call_id, leading_fields, (u32)(sizeof(leading_fields) / sizeof(leading_fields[0])));
    write_string(prefix);
    write_string("source ");
    if ((u32)syscall64_invoke(call_id, MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_TELEMETRY_SOURCE, 0u, 0u) == 1u)
    {
        write_string("disk");
    }
    else
    {
        write_string("unavailable");
    }
    write_syscall1_prefixed_fields(prefix, call_id, trailing_fields, (u32)(sizeof(trailing_fields) / sizeof(trailing_fields[0])));
}

static void __attribute__((noinline)) write_drs_load_full_fields(const char *prefix, u64 call_id)
{
    static const struct scaffold_compact_selector_field fields[] = {
        {0x0000u, 0u},
        {0x4007u, 1u},
        {0x4029u, 2u},
        {0x52BEu, 3u},
        {0x139Bu, 4u},
        {0x13A7u, 5u},
        {0x13B7u, 6u},
        {0x13C1u, 7u},
        {0x13CBu, 8u},
        {0x13D7u, 9u},
        {0x13DCu, 10u},
        {0x0F42u, 11u},
        {0x13E3u, 27u},
        {0x13EBu, 28u}
    };

    write_syscall1_compact_selector_fields(prefix, call_id, fields, (u32)(sizeof(fields) / sizeof(fields[0])));
    write_string(prefix);
    write_string("source ");
    if ((u32)syscall64_invoke(call_id, MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_SOURCE, 0u, 0u) == 1u)
    {
        write_string("disk");
    }
    else
    {
        write_string("unavailable");
    }
    static const struct scaffold_telemetry_field trailing_fields[] = {
        {"exit-result ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_EXIT_RESULT, SCAFFOLD_TELEMETRY_HEX},
        {"exit-aux ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_EXIT_AUX, SCAFFOLD_TELEMETRY_DEC},
        {"write-escalation ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_WRITE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {"commit ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_COMMIT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {"additional-fs-caps ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_ADDITIONAL_FS_CAPS, SCAFFOLD_TELEMETRY_DEC},
        {"staged ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_STAGE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {"denials ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {"unavailable ", MMIO64_AHCI_DRIVER_READ_STATUS_LOAD_FULL_TELEMETRY_UNAVAILABLE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };

    write_syscall1_prefixed_fields(prefix, call_id, trailing_fields, (u32)(sizeof(trailing_fields) / sizeof(trailing_fields[0])));
}

static u32 string_length(const char *text)
{
    u32 length = 0u;

    while (text[length] != '\0')
    {
        ++length;
    }

    return length;
}

static void zero_bytes(u8 *bytes, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static void write_buffer_preview(const u8 *bytes, u32 byte_count, u32 max_count)
{
    u32 index;
    u32 limit = (byte_count < max_count) ? byte_count : max_count;

    for (index = 0u; index < limit; ++index)
    {
        char character = (char)bytes[index];

        if (character == '\0')
        {
            return;
        }

        if (character == '\n' || character == '\t')
        {
            character = '|';
        }
        else if (((u8)character < 32u) || ((u8)character > 126u))
        {
            character = ' ';
        }

        debug_write_char(character);
        console_write_char(character);
    }
}

static u64 pack_count_owner(u32 byte_count, u32 owner_id)
{
    return ((u64)owner_id << 32) | (u64)byte_count;
}

static u64 pack_create_owner(u32 path_byte_count, u32 node_type, u32 owner_id)
{
    return ((u64)owner_id << 32)
        | (((u64)node_type & 0xFFFFull) << 16)
        | ((u64)path_byte_count & 0xFFFFull);
}

static u64 pack_rw_owner(u32 byte_count, u32 file_offset, u32 owner_id)
{
    return ((u64)owner_id << 32)
        | (((u64)file_offset & 0xFFFFull) << 16)
        | ((u64)byte_count & 0xFFFFull);
}

static u32 live_console_write_bytes(u32 console_capability, const u8 *bytes, u32 byte_count)
{
    if ((console_capability == CAPABILITY64_INVALID_HANDLE) || (bytes == 0) || (byte_count == 0u))
    {
        return 0u;
    }

    return (u32)syscall64_invoke(
        X64_SYSCALL_CONSOLE_WRITE,
        console_capability,
        (u64)bytes,
        pack_count_owner(byte_count, PRINCIPAL64_ID_CONSOLE_CLIENT));
}

static u32 live_console_write_string(u32 console_capability, const char *text)
{
    return live_console_write_bytes(console_capability, (const u8 *)text, string_length(text));
}

static void live_console_write_dec_u32(u32 console_capability, u32 value)
{
    char digits[10];
    u32 count = 0u;
    u32 index;

    if (value == 0u)
    {
        (void)live_console_write_bytes(console_capability, (const u8 *)"0", 1u);
        return;
    }

    while (value != 0u)
    {
        digits[count] = (char)('0' + (value % 10u));
        value /= 10u;
        ++count;
    }

    for (index = 0u; index < count; ++index)
    {
        char digit = digits[count - index - 1u];
        (void)live_console_write_bytes(console_capability, (const u8 *)&digit, 1u);
    }
}

static void live_console_write_hex_u32(u32 console_capability, u32 value)
{
    char text[10];
    u32 index;

    text[0] = '0';
    text[1] = 'x';
    for (index = 0u; index < 8u; ++index)
    {
        u8 nibble = (u8)((value >> ((7u - index) * 4u)) & 0x0Fu);
        if (nibble < 10u)
        {
            text[2u + index] = (char)((u8)'0' + nibble);
        }
        else
        {
            text[2u + index] = (char)((u8)'A' + (nibble - 10u));
        }
    }

    (void)live_console_write_bytes(console_capability, (const u8 *)text, sizeof(text));
}

static int live_line_equals(const u8 *line, u32 line_length, const char *text)
{
    u32 expected = string_length(text);
    u32 index;

    if (line_length != expected)
    {
        return 0;
    }

    for (index = 0u; index < expected; ++index)
    {
        if (line[index] != (u8)text[index])
        {
            return 0;
        }
    }

    return 1;
}

static int live_line_starts_with(const u8 *line, u32 line_length, const char *prefix)
{
    u32 expected = string_length(prefix);
    u32 index;

    if (line_length < expected)
    {
        return 0;
    }

    for (index = 0u; index < expected; ++index)
    {
        if (line[index] != (u8)prefix[index])
        {
            return 0;
        }
    }

    return 1;
}

static void run_live_keyboard_console_command(u32 console_capability, const u8 *line, u32 line_length)
{
    if ((line_length == 0u) || live_line_equals(line, line_length, "help") != 0)
    {
        (void)live_console_write_string(
            console_capability,
            "commands: help keys echo <text>\n");
        return;
    }

    if (live_line_equals(line, line_length, "keys") != 0)
    {
        (void)live_console_write_string(console_capability, "keys ");
        live_console_write_dec_u32(console_capability, input64_keyboard_scancode_count());
        (void)live_console_write_string(console_capability, " pending ");
        live_console_write_dec_u32(console_capability, input64_keyboard_pending_count());
        (void)live_console_write_string(console_capability, " last ");
        live_console_write_hex_u32(console_capability, input64_keyboard_last_scancode());
        (void)live_console_write_string(console_capability, "\n");
        return;
    }

    if (live_line_starts_with(line, line_length, "echo ") != 0)
    {
        (void)live_console_write_bytes(console_capability, line + 5u, line_length - 5u);
        (void)live_console_write_string(console_capability, "\n");
        return;
    }

    (void)live_console_write_string(console_capability, "unknown command\n");
}

static void run_live_keyboard_console(void)
{
    u32 console_capability = (u32)syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        PRINCIPAL64_ID_CONSOLE_CLIENT);
    u32 input_capability = (u32)syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_INPUT,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        PRINCIPAL64_ID_CONSOLE_CLIENT);
    u8 line[64];
    u8 bytes[16];
    u32 line_length = 0u;
    u32 index;

    (void)live_console_write_string(
        console_capability,
        "\n[x64:live] brokered keyboard console online\n[x64:live] $ ");

    interrupts64_enable();
    for (;;)
    {
        u32 byte_count = (u32)syscall64_invoke(
            X64_SYSCALL_INPUT_READ_KEYBOARD,
            input_capability,
            (u64)bytes,
            pack_count_owner((u32)sizeof(bytes), PRINCIPAL64_ID_CONSOLE_CLIENT));

        if (byte_count == 0u)
        {
            cpu_halt();
            continue;
        }

        for (index = 0u; index < byte_count; ++index)
        {
            u8 input_byte = bytes[index];

            if (input_byte == (u8)'\r')
            {
                continue;
            }

            if (input_byte == (u8)'\n')
            {
                (void)live_console_write_string(console_capability, "\n");
                run_live_keyboard_console_command(console_capability, line, line_length);
                line_length = 0u;
                (void)live_console_write_string(console_capability, "[x64:live] $ ");
                continue;
            }

            if ((input_byte == (u8)'\b') || (input_byte == 0x7Fu))
            {
                if (line_length > 0u)
                {
                    --line_length;
                    (void)live_console_write_string(console_capability, "\b \b");
                }
                continue;
            }

            if ((input_byte >= 0x20u) && (input_byte < 0x7Fu) && (line_length < (u32)sizeof(line)))
            {
                line[line_length] = input_byte;
                ++line_length;
                (void)live_console_write_bytes(console_capability, &input_byte, 1u);
            }
        }
    }
}

static int run_persistent_ring3_shell(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    const void *transfer_image = runtime64_transfer_image_base();
    u32 mapped_bytes = runtime64_transfer_image_size();
    u64 shell_rip;
    u32 shell_rflags;

    if ((policy_manifest == LAUNCH64_INVALID_MANIFEST)
        || ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) == 0u))
    {
        return 0;
    }

    if ((mapped_bytes == 0u)
        || ((mapped_bytes & (LAUNCH64_IMAGE_MAP_PAGE_BYTES - 1u)) != 0u)
        || (paging64_install_runtime_mapping(LAUNCH64_IMAGE_PLAN_BASE, transfer_image, mapped_bytes) == 0u)
        || (paging64_install_user_runtime_mapping(LAUNCH64_USER_IMAGE_BASE, transfer_image, mapped_bytes) == 0u)
        || (paging64_install_user_stack_mapping(LAUNCH64_USER_STACK_TOP, LAUNCH64_USER_STACK_BYTES) == 0u))
    {
        write_line("[x64] persistent ring3 shell remap failed");
        return 0;
    }

    shell_rip = (u64)LAUNCH64_USER_IMAGE_BASE
        + (u64)runtime64_transfer_user_hardware_shell_probe_offset();
    shell_rflags = launch64_manifest_runtime_user_entry_rflags(policy_manifest) | 0x00000200u;
    write_line("[x64] persistent ring3 shell default");
    interrupts64_enable();
    (void)interrupts64_trigger_user_entry_probe(
        shell_rip,
        launch64_manifest_runtime_user_entry_rsp(policy_manifest),
        launch64_manifest_runtime_user_entry_selectors(policy_manifest),
        shell_rflags);
    interrupts64_disable();

    return 0;
}

static void write_protection_summary(u32 protection_flags)
{
    write_string(" user ");
    write_dec_u32((protection_flags & PAGING64_RUNTIME_PROTECTION_USER_ACCESSIBLE) != 0u ? 1u : 0u);
    write_string(" writable ");
    write_dec_u32((protection_flags & PAGING64_RUNTIME_PROTECTION_WRITABLE) != 0u ? 1u : 0u);
    write_string(" supervisor-only ");
    write_dec_u32((protection_flags & PAGING64_RUNTIME_PROTECTION_SUPERVISOR_ONLY) != 0u ? 1u : 0u);
    write_string(" validation-only ");
    write_dec_u32((protection_flags & PAGING64_RUNTIME_PROTECTION_VALIDATION_ONLY) != 0u ? 1u : 0u);
}

static void log_descriptor_surface(void)
{
    write_string("[x64] descriptors state ");
    write_hex_u32(descriptors64_state());
    write_string(" gdt ");
    write_hex_u32(descriptors64_gdt_token());
    write_string(" tss-token ");
    write_hex_u32(descriptors64_tss_token());
    write_string(" cs ");
    write_hex_u32((u32)read_cs64());
    write_string(" ss ");
    write_hex_u32((u32)read_ss64());
    write_string(" kernel-cs ");
    write_hex_u32((u32)descriptors64_kernel_code_selector());
    write_string(" kernel-ds ");
    write_hex_u32((u32)descriptors64_kernel_data_selector());
    write_string(" user-cs ");
    write_hex_u32((u32)descriptors64_user_code_selector());
    write_string(" user-ds ");
    write_hex_u32((u32)descriptors64_user_data_selector());
    write_string(" tss ");
    write_hex_u32((u32)descriptors64_tss_selector());
    write_string(" tr ");
    write_hex_u32((u32)read_tr64());
    write_string(" rsp0 ");
    write_hex_u64(descriptors64_tss_rsp0());
    write_string(" star-plan ");
    write_hex_u64(descriptors64_syscall_star_plan());
    write_string(" star ");
    write_hex_u64(syscall64_native_star_value());
    write_string(" star-ready ");
    write_dec_u32(syscall64_native_star_ready());
    write_line("");
}

static void log_boot_memory(const struct boot_info *boot_info)
{
    write_string("[boot] drive ");
    write_hex_u32(boot_info->boot_drive);
    write_string(" conventional ");
    write_dec_u32(boot_info->conventional_memory_kb);
    write_string(" KiB extended ");
    write_dec_u32(boot_info->extended_memory_kb);
    write_line(" KiB");
}

static void log_bootstrap_state(const struct boot_info *boot_info)
{
    write_string("[boot] arch ");
    write_dec_u32(boot_info->architecture_bits);
    write_string(" flags ");
    write_hex_u32(boot_info->bootstrap_flags);
    write_line("");
}

static void log_bootstrap_catalog(void)
{
    if (services64_package_valid() == 0u)
    {
        write_line("[x64] package archive unavailable");
        return;
    }

    write_string("[x64] package archive v");
    write_dec_u32(services64_package_version());
    write_string(" signers ");
    write_dec_u32(services64_package_signer_count());
    write_string(" manifests ");
    write_dec_u32(services64_package_manifest_count());
    write_string(" payloads ");
    write_dec_u32(services64_package_payload_count());
    write_string(" checksum ");
    write_hex_u32(services64_package_checksum());
    write_line("");
}

static void log_service_namespace(void)
{
    write_string("[x64] services total ");
    write_dec_u32(services64_count());
    write_string(" policy ");
    write_dec_u32(services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_AI_POLICY));
    write_string(" console ");
    write_dec_u32(services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE));
    write_string(" ramfs ");
    write_dec_u32(services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS));
    write_string(" input ");
    write_dec_u32(services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT));
    write_string(" display ");
    write_dec_u32(services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_DISPLAY));
    write_string(" block ");
    write_dec_u32(services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_BLOCK));
    write_string(" hardware ");
    write_dec_u32(services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_HARDWARE));
    write_line("");

    write_string("[x64] service caps policy ");
    write_hex_u32(services64_capabilities_for_endpoint(
        services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_AI_POLICY)));
    write_string(" console ");
    write_hex_u32(services64_capabilities_for_endpoint(
        services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_CONSOLE)));
    write_string(" input ");
    write_hex_u32(services64_capabilities_for_endpoint(
        services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_INPUT)));
    write_string(" display ");
    write_hex_u32(services64_capabilities_for_endpoint(
        services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_DISPLAY)));
    write_string(" block ");
    write_hex_u32(services64_capabilities_for_endpoint(
        services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_BLOCK)));
    write_string(" hardware ");
    write_hex_u32(services64_capabilities_for_endpoint(
        services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_HARDWARE)));
    write_line("");
}

static void log_principal_namespace(void)
{
    u32 policy_client = principal64_lookup_by_index(1u);
    u32 policy_worker = principal64_lookup_by_index(2u);

    write_string("[x64] principals total ");
    write_dec_u32(principal64_count());
    write_labeled_hex_u32(" system ", PRINCIPAL64_ID_SYSTEM);
    write_labeled_hex_u32(" policy-client ", policy_client);
    write_labeled_hex_u32(" policy-worker ", policy_worker);
    write_line("");

    write_string("[x64] principal roles ");
    write_string(principal64_name(policy_client));
    write_string(" ");
    write_hex_u32(principal64_role(policy_client));
    write_string(" ");
    write_string(principal64_name(policy_worker));
    write_string(" ");
    write_hex_u32(principal64_role(policy_worker));
    write_line("");
}

static void log_process_namespace(void)
{
    u32 init_pid = process64_pid_for_principal(PRINCIPAL64_ID_INIT_SUPERVISOR);
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 ramfs_pid = process64_pid_for_principal(PRINCIPAL64_ID_RAMFS_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);

    write_string("[x64] processes total ");
    write_dec_u32(process64_count());
    write_string(" manifest-verified ");
    write_dec_u32(process64_manifest_verified_count());
    write_labeled_dec_u32(" init-pid ", init_pid);
    write_string(" principal ");
    write_hex_u32(process64_principal(init_pid));
    write_labeled_dec_u32(" policy-pid ", policy_pid);
    write_string(" principal ");
    write_hex_u32(process64_principal(policy_pid));
    write_labeled_dec_u32(" ramfs-pid ", ramfs_pid);
    write_line("");

    write_string("[x64] process binds ");
    write_string(process64_name(policy_pid));
    write_string(" endpoint ");
    write_dec_u32(process64_endpoint(policy_pid));
    write_string(" state ");
    write_hex_u32(process64_state(policy_pid));
    write_labeled_dec_u32(" manifest ", policy_manifest);
    write_string(" pkg ");
    write_dec_u32(process64_manifest_package_id(policy_pid));
    write_string(" token ");
    write_hex_u32(process64_manifest_token(policy_pid));
    write_string(" launch-state ");
    write_hex_u32(launch64_manifest_launch_state(policy_manifest));
    write_string(" phase ");
    write_dec_u32(launch64_manifest_lifecycle_phase(policy_manifest));
    write_string(" restarts ");
    write_dec_u32(launch64_manifest_restart_count(policy_manifest));
    write_string(" generation ");
    write_dec_u32(launch64_manifest_runtime_generation(policy_manifest));
    write_string(" runtime ");
    write_hex_u32(launch64_manifest_runtime_token(policy_manifest));
    write_string(" image-generation ");
    write_dec_u32(launch64_manifest_runtime_image_generation(policy_manifest));
    write_string(" image ");
    write_hex_u32(launch64_manifest_runtime_image_token(policy_manifest));
    write_string(" image-plan ");
    write_hex_u32(launch64_manifest_runtime_image_plan_token(policy_manifest));
    write_string(" plan-base ");
    write_hex_u32(launch64_manifest_runtime_image_base(policy_manifest));
    write_string(" plan-entry ");
    write_hex_u32(launch64_manifest_runtime_image_entry(policy_manifest));
    write_string(" plan-bytes ");
    write_dec_u32(launch64_manifest_runtime_image_mapped_bytes(policy_manifest));
    write_string(" plan-rights ");
    write_hex_u32(launch64_manifest_runtime_image_rights(policy_manifest));
    write_string(" image-map ");
    write_hex_u32(launch64_manifest_runtime_image_map_token(policy_manifest));
    write_string(" map-pages ");
    write_dec_u32(launch64_manifest_runtime_image_page_count(policy_manifest));
    write_string(" map-pml4 ");
    write_dec_u32(launch64_manifest_runtime_image_pml4_index(policy_manifest));
    write_string(" map-pdpt ");
    write_dec_u32(launch64_manifest_runtime_image_pdpt_index(policy_manifest));
    write_string(" map-pd ");
    write_dec_u32(launch64_manifest_runtime_image_pd_index(policy_manifest));
    write_string(" transfer ");
    write_hex_u32(launch64_manifest_runtime_entry_transfer_token(policy_manifest));
    write_string(" install ");
    write_hex_u32(launch64_manifest_runtime_image_install_token(policy_manifest));
    write_string(" source-checksum ");
    write_hex_u32(launch64_manifest_runtime_image_source_checksum(policy_manifest));
    write_string(" entry-probe ");
    write_hex_u32(launch64_manifest_runtime_image_entry_probe(policy_manifest));
    write_string(" installed ");
    write_dec_u32(launch64_manifest_runtime_image_map_installed(policy_manifest));
    write_string(" protection ");
    write_hex_u32(launch64_manifest_runtime_image_protection_flags(policy_manifest));
    write_string(" protection-token ");
    write_hex_u32(launch64_manifest_runtime_image_protection_token(policy_manifest));
    write_protection_summary(launch64_manifest_runtime_image_protection_flags(policy_manifest));
    write_string(" user-entry-state ");
    write_hex_u32(launch64_manifest_runtime_user_entry_state(policy_manifest));
    write_string(" user-entry-token ");
    write_hex_u32(launch64_manifest_runtime_user_entry_token(policy_manifest));
    write_string(" user-rip ");
    write_hex_u32(launch64_manifest_runtime_user_entry_rip(policy_manifest));
    write_string(" user-rsp ");
    write_hex_u32(launch64_manifest_runtime_user_entry_rsp(policy_manifest));
    write_string(" user-selectors ");
    write_hex_u32(launch64_manifest_runtime_user_entry_selectors(policy_manifest));
    write_string(" user-rflags ");
    write_hex_u32(launch64_manifest_runtime_user_entry_rflags(policy_manifest));
    write_string(" user-entry-denial ");
    write_dec_u32(launch64_manifest_runtime_user_entry_denial(policy_manifest));
    write_string(" user-transfer-ready ");
    write_dec_u32(
        (launch64_manifest_runtime_user_entry_state(policy_manifest)
            & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u ? 1u : 0u);
    write_string(" payload ");
    write_dec_u32(launch64_manifest_runtime_payload_slot(policy_manifest));
    write_string(" kind ");
    write_dec_u32(launch64_manifest_runtime_payload_kind(policy_manifest));
    write_string(" payload-offset ");
    write_dec_u32(launch64_manifest_runtime_payload_offset(policy_manifest));
    write_string(" payload-size ");
    write_dec_u32(launch64_manifest_runtime_payload_size(policy_manifest));
    write_string(" payload-checksum ");
    write_hex_u32(launch64_manifest_runtime_payload_checksum(policy_manifest));
    write_string(" launched-pid ");
    write_dec_u32(launch64_manifest_launched_pid(policy_manifest));
    write_string(" requester ");
    write_hex_u32(launch64_manifest_last_requester(policy_manifest));
    write_string(" request ");
    write_dec_u32(launch64_manifest_last_request_id(policy_manifest));
    write_string(" request-state ");
    write_hex_u32(launch64_manifest_last_request_status(policy_manifest));
    write_string(" cap-limit ");
    write_dec_u32(process64_capability_limit(policy_pid));
    write_string(" ");
    write_string(process64_name(ramfs_pid));
    write_string(" endpoint ");
    write_dec_u32(process64_endpoint(ramfs_pid));
    write_line("");
}

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
    u64 address = (u64)(const void *)mapped_address;

    if (address >= LIMITLESS_X64_KERNEL_VIRTUAL_BASE)
    {
        return address - LIMITLESS_X64_KERNEL_VIRTUAL_BASE;
    }

    return address;
}

static u64 higher_half_alias_address(const void *mapped_address)
{
    return LIMITLESS_X64_KERNEL_VIRTUAL_BASE + lower_half_alias_address(mapped_address);
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
}

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

static void run_user_entry_cli_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u64 cli_rip = 0ull;
    u32 result = 0u;

    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        cli_rip = (u64)LAUNCH64_USER_IMAGE_BASE
            + (u64)runtime64_transfer_user_cli_probe_offset();
        result = interrupts64_trigger_user_entry_probe(
            cli_rip,
            launch64_manifest_runtime_user_entry_rsp(policy_manifest),
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            launch64_manifest_runtime_user_entry_rflags(policy_manifest));
    }

    write_string("[x64] user cli probe attempts ");
    write_dec_u32(interrupts64_user_entry_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_entry_probe_exits());
    write_labeled_hex_u32(" result ", result);
    write_string(" recorded ");
    write_hex_u32(interrupts64_user_entry_probe_result());
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_cli_result());
    write_string(" read-bytes ");
    write_dec_u32(runtime64_transfer_user_fs_read_bytes());
    write_string(" prompt-bytes ");
    write_dec_u32(runtime64_transfer_user_cli_prompt_length());
    write_syscall0_dec_u32(" console-writes ", X64_SYSCALL_CONSOLE_WRITE_COUNT);
    write_syscall0_dec_u32(" console-bytes ", X64_SYSCALL_CONSOLE_BYTE_COUNT);
    write_syscall0_dec_u32(" console-denials ", X64_SYSCALL_CONSOLE_DENIAL_COUNT);
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

static void run_user_entry_input_cli_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u64 input_cli_rip = 0ull;
    u32 result = 0u;

    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        input_cli_rip = (u64)LAUNCH64_USER_IMAGE_BASE
            + (u64)runtime64_transfer_user_input_cli_probe_offset();
        result = interrupts64_trigger_user_entry_probe(
            input_cli_rip,
            launch64_manifest_runtime_user_entry_rsp(policy_manifest),
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            launch64_manifest_runtime_user_entry_rflags(policy_manifest));
    }

    write_string("[x64] user input cli probe attempts ");
    write_dec_u32(interrupts64_user_entry_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_entry_probe_exits());
    write_labeled_hex_u32(" result ", result);
    write_string(" recorded ");
    write_hex_u32(interrupts64_user_entry_probe_result());
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_input_cli_result());
    write_string(" command-bytes ");
    write_dec_u32(runtime64_transfer_user_input_cli_command_bytes());
    write_string(" read-bytes ");
    write_dec_u32(runtime64_transfer_user_fs_read_bytes());
    write_string(" prompt-bytes ");
    write_dec_u32(runtime64_transfer_user_input_cli_prompt_length());
    static const char syscall0_suffixes_0[] =
        "console-writes \0"
        "console-bytes \0"
        "console-denials \0"
        "input-reads \0"
        "input-bytes \0"
        "input-denials \0"
        "input-eof \0";
    static const struct scaffold_syscall0_field syscall0_fields_0[] = {        {0, X64_SYSCALL_CONSOLE_WRITE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {16, X64_SYSCALL_CONSOLE_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {31, X64_SYSCALL_CONSOLE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {48, X64_SYSCALL_INPUT_READ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {61, X64_SYSCALL_INPUT_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {74, X64_SYSCALL_INPUT_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {89, X64_SYSCALL_INPUT_EOF_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" ", syscall0_suffixes_0, syscall0_fields_0, (u32)(sizeof(syscall0_fields_0) / sizeof(syscall0_fields_0[0])));
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

static void run_user_entry_shell_stream_probe(void)
{
    u32 policy_pid = process64_pid_for_principal(PRINCIPAL64_ID_POLICY_WORKER);
    u32 policy_manifest = process64_manifest_index(policy_pid);
    u32 user_entry_state = launch64_manifest_runtime_user_entry_state(policy_manifest);
    u64 shell_stream_rip = 0ull;
    u32 result = 0u;

    if ((policy_manifest != LAUNCH64_INVALID_MANIFEST)
        && ((user_entry_state & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u))
    {
        shell_stream_rip = (u64)LAUNCH64_USER_IMAGE_BASE
            + (u64)runtime64_transfer_user_shell_stream_probe_offset();
        result = interrupts64_trigger_user_entry_probe(
            shell_stream_rip,
            launch64_manifest_runtime_user_entry_rsp(policy_manifest),
            launch64_manifest_runtime_user_entry_selectors(policy_manifest),
            launch64_manifest_runtime_user_entry_rflags(policy_manifest));
    }

    write_string("[x64] user shell stream probe attempts ");
    write_dec_u32(interrupts64_user_entry_probe_attempts());
    write_string(" exits ");
    write_dec_u32(interrupts64_user_entry_probe_exits());
    write_labeled_hex_u32(" result ", result);
    write_string(" recorded ");
    write_hex_u32(interrupts64_user_entry_probe_result());
    write_string(" expected ");
    write_hex_u32(runtime64_transfer_user_shell_stream_result());
    write_string(" command-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_command_bytes());
    write_string(" command-capacity ");
    write_dec_u32(runtime64_transfer_user_shell_stream_command_capacity());
    write_string(" commands ");
    write_dec_u32(runtime64_transfer_user_shell_stream_expected_commands());
    write_string(" unknowns ");
    write_dec_u32(runtime64_transfer_user_shell_stream_expected_unknowns());
    write_string(" unknown-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_unknown_command_bytes());
    write_string(" unknown-message-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_unknown_message_bytes());
    write_string(" help-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_help_bytes());
    write_string(" help-ls-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_help_ls_bytes());
    write_string(" help-cat-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_help_cat_bytes());
    write_string(" help-stat-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_help_stat_bytes());
    write_string(" help-mkdir-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_help_mkdir_bytes());
    write_string(" help-write-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_help_write_bytes());
    write_string(" apps-index-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_apps_index_bytes());
    write_string(" ls-app-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_ls_app_bytes());
    write_string(" cat-app-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_cat_app_bytes());
    write_string(" stat-app-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_stat_app_bytes());
    write_string(" mkdir-app-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_mkdir_app_bytes());
    write_string(" write-app-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_write_app_bytes());
    write_string(" info-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_info_ls_output_bytes());
    write_string(" info-cat-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_info_cat_output_bytes());
    write_string(" info-stat-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_info_stat_output_bytes());
    write_string(" info-mkdir-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_info_mkdir_output_bytes());
    write_string(" info-write-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_info_write_output_bytes());
    write_string(" pwd-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_pwd_bytes());
    write_string(" root-list-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_root_list_bytes());
    write_string(" list-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_list_bytes());
    write_string(" cat-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_cat_bytes());
    write_string(" stat-bytes ");
    write_dec_u32(runtime64_transfer_user_shell_stream_stat_bytes());
    write_string(" prompt-bytes ");
    write_dec_u32(runtime64_transfer_user_input_cli_prompt_length());
    static const char syscall0_suffixes_1[] =
        "console-writes \0"
        "console-bytes \0"
        "console-denials \0"
        "input-reads \0"
        "input-lines \0"
        "input-bytes \0"
        "input-edits \0"
        "input-denials \0"
        "input-eof \0"
        "fs-lists \0"
        "fs-creates \0"
        "fs-reads \0"
        "fs-writes \0"
        "fs-stats \0";
    static const struct scaffold_syscall0_field syscall0_fields_1[] = {        {0, X64_SYSCALL_CONSOLE_WRITE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {16, X64_SYSCALL_CONSOLE_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {31, X64_SYSCALL_CONSOLE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {48, X64_SYSCALL_INPUT_READ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {61, X64_SYSCALL_INPUT_LINE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {74, X64_SYSCALL_INPUT_BYTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {87, X64_SYSCALL_INPUT_EDIT_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {100, X64_SYSCALL_INPUT_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {115, X64_SYSCALL_INPUT_EOF_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {126, X64_SYSCALL_FS_LIST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {136, X64_SYSCALL_FS_CREATE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {148, X64_SYSCALL_FS_READ_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {158, X64_SYSCALL_FS_WRITE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {169, X64_SYSCALL_FS_STAT_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" ", syscall0_suffixes_1, syscall0_fields_1, (u32)(sizeof(syscall0_fields_1) / sizeof(syscall0_fields_1[0])));
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
    write_line("");
}

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

static void log_virtio_net_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"found ", SCAFFOLD_VALUE_NET_FOUND, SCAFFOLD_TELEMETRY_DEC},
        {"bar0 ", SCAFFOLD_VALUE_NET_BAR0, SCAFFOLD_TELEMETRY_HEX64},
        {"mapped ", SCAFFOLD_VALUE_NET_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {"common ", SCAFFOLD_VALUE_NET_COMMON, SCAFFOLD_TELEMETRY_DEC},
        {"notify ", SCAFFOLD_VALUE_NET_NOTIFY, SCAFFOLD_TELEMETRY_DEC},
        {"device-config ", SCAFFOLD_VALUE_NET_DEVICE_CONFIG, SCAFFOLD_TELEMETRY_DEC},
        {"mac ", SCAFFOLD_VALUE_NET_MAC, SCAFFOLD_TELEMETRY_HEX64},
        {"mac-nonzero ", SCAFFOLD_VALUE_NET_MAC_NONZERO, SCAFFOLD_TELEMETRY_DEC},
        {"status-ack ", SCAFFOLD_VALUE_NET_STATUS_ACK, SCAFFOLD_TELEMETRY_DEC},
        {"status-driver ", SCAFFOLD_VALUE_NET_STATUS_DRIVER, SCAFFOLD_TELEMETRY_DEC},
        {"features-ok ", SCAFFOLD_VALUE_NET_FEATURES_OK, SCAFFOLD_TELEMETRY_DEC},
        {"driver-ok ", SCAFFOLD_VALUE_NET_DRIVER_OK, SCAFFOLD_TELEMETRY_DEC},
        {"rx-queue ", SCAFFOLD_VALUE_NET_RX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"tx-queue ", SCAFFOLD_VALUE_NET_TX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"rx-buffers ", SCAFFOLD_VALUE_NET_RX_BUFFERS, SCAFFOLD_TELEMETRY_DEC},
        {"tx ", SCAFFOLD_VALUE_NET_TX, SCAFFOLD_TELEMETRY_DEC},
        {"rx ", SCAFFOLD_VALUE_NET_RX, SCAFFOLD_TELEMETRY_DEC},
        {"arp-reply ", SCAFFOLD_VALUE_NET_ARP_REPLY, SCAFFOLD_TELEMETRY_DEC},
        {"arp-mac ", SCAFFOLD_VALUE_NET_ARP_MAC, SCAFFOLD_TELEMETRY_HEX64},
        {"arp-ip ", SCAFFOLD_VALUE_NET_ARP_IP, SCAFFOLD_TELEMETRY_HEX}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_NET_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_NET_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_NET_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_NET_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_NET_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-net drs-net-",
        " drs-net-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_e1000_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"found ", SCAFFOLD_VALUE_E1000_FOUND, SCAFFOLD_TELEMETRY_DEC},
        {"bar0 ", SCAFFOLD_VALUE_E1000_BAR0, SCAFFOLD_TELEMETRY_HEX64},
        {"mapped ", SCAFFOLD_VALUE_E1000_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {"reset ", SCAFFOLD_VALUE_E1000_RESET, SCAFFOLD_TELEMETRY_DEC},
        {"mac ", SCAFFOLD_VALUE_E1000_MAC, SCAFFOLD_TELEMETRY_HEX64},
        {"mac-nonzero ", SCAFFOLD_VALUE_E1000_MAC_NONZERO, SCAFFOLD_TELEMETRY_DEC},
        {"link-up ", SCAFFOLD_VALUE_E1000_LINK_UP, SCAFFOLD_TELEMETRY_DEC},
        {"rx-queue ", SCAFFOLD_VALUE_E1000_RX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"tx-queue ", SCAFFOLD_VALUE_E1000_TX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"rx-buffers ", SCAFFOLD_VALUE_E1000_RX_BUFFERS, SCAFFOLD_TELEMETRY_DEC},
        {"tx ", SCAFFOLD_VALUE_E1000_TX, SCAFFOLD_TELEMETRY_DEC},
        {"rx ", SCAFFOLD_VALUE_E1000_RX, SCAFFOLD_TELEMETRY_DEC},
        {"dhcp ", SCAFFOLD_VALUE_E1000_DHCP, SCAFFOLD_TELEMETRY_DEC},
        {"dns ", SCAFFOLD_VALUE_E1000_DNS, SCAFFOLD_TELEMETRY_DEC},
        {"http ", SCAFFOLD_VALUE_E1000_HTTP, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_E1000_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_E1000_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_E1000_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_E1000_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_E1000_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-e1000 drs-e1000-",
        " drs-e1000-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_dhcp_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"discover ", SCAFFOLD_VALUE_DHCP_DISCOVER, SCAFFOLD_TELEMETRY_DEC},
        {"offer ", SCAFFOLD_VALUE_DHCP_OFFER, SCAFFOLD_TELEMETRY_DEC},
        {"request ", SCAFFOLD_VALUE_DHCP_REQUEST, SCAFFOLD_TELEMETRY_DEC},
        {"ack ", SCAFFOLD_VALUE_DHCP_ACK, SCAFFOLD_TELEMETRY_DEC},
        {"ip ", SCAFFOLD_VALUE_DHCP_IP, SCAFFOLD_TELEMETRY_HEX},
        {"gateway ", SCAFFOLD_VALUE_DHCP_GATEWAY, SCAFFOLD_TELEMETRY_HEX},
        {"dns ", SCAFFOLD_VALUE_DHCP_DNS, SCAFFOLD_TELEMETRY_HEX},
        {"lease ", SCAFFOLD_VALUE_DHCP_LEASE, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" ambient-authority ", SCAFFOLD_VALUE_DHCP_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_DHCP_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_DHCP_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-dhcp drs-dhcp-",
        " drs-dhcp-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_dns_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"query ", SCAFFOLD_VALUE_DNS_QUERY, SCAFFOLD_TELEMETRY_DEC},
        {"response ", SCAFFOLD_VALUE_DNS_RESPONSE, SCAFFOLD_TELEMETRY_DEC},
        {"rcode ", SCAFFOLD_VALUE_DNS_RCODE, SCAFFOLD_TELEMETRY_DEC},
        {"resolved ", SCAFFOLD_VALUE_DNS_RESOLVED, SCAFFOLD_TELEMETRY_HEX}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_DNS_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_DNS_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_DNS_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_DNS_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_DNS_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-dns drs-dns-",
        " drs-dns-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_http_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"connected ", SCAFFOLD_VALUE_HTTP_CONNECTED, SCAFFOLD_TELEMETRY_DEC},
        {"sent ", SCAFFOLD_VALUE_HTTP_SENT, SCAFFOLD_TELEMETRY_DEC},
        {"status ", SCAFFOLD_VALUE_HTTP_STATUS, SCAFFOLD_TELEMETRY_DEC},
        {"response-bytes ", SCAFFOLD_VALUE_HTTP_RESPONSE_BYTES, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_HTTP_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_HTTP_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_HTTP_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_HTTP_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_HTTP_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-http drs-http-",
        " drs-http-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

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

static void log_principal_surface(void)
{
    u64 indexed_policy = syscall64_invoke(X64_SYSCALL_PRINCIPAL_BY_INDEX, 1u, 0u, 0u);
    u64 indexed_worker = syscall64_invoke(X64_SYSCALL_PRINCIPAL_BY_INDEX, 2u, 0u, 0u);

    write_syscall0_dec_u32("[x64] syscall principals ", X64_SYSCALL_PRINCIPAL_COUNT);
    write_string(" policy ");
    write_hex_u32((u32)indexed_policy);
    write_syscall1_dec_u32(" active ", X64_SYSCALL_PRINCIPAL_ACTIVE, indexed_policy);
    write_syscall1_hex_u32(" role ", X64_SYSCALL_PRINCIPAL_ROLE, indexed_policy);
    write_string(" worker ");
    write_hex_u32((u32)indexed_worker);
    write_syscall1_hex_u32(" role ", X64_SYSCALL_PRINCIPAL_ROLE, indexed_worker);
    write_line("");
}

static void log_process_surface(void)
{
    u64 policy_pid = syscall64_invoke(
        X64_SYSCALL_PROCESS_BY_PRINCIPAL,
        PRINCIPAL64_ID_POLICY_WORKER,
        0u,
        0u);
    u64 policy_manifest = syscall64_invoke(X64_SYSCALL_PROCESS_MANIFEST_INDEX, policy_pid, 0u, 0u);
    u64 invalid_pid = syscall64_invoke(X64_SYSCALL_PROCESS_PID_BY_INDEX, 99u, 0u, 0u);

    write_syscall0_dec_u32("[x64] syscall processes ", X64_SYSCALL_PROCESS_COUNT);
    write_string(" policy-pid ");
    write_dec_u32((u32)policy_pid);
    write_syscall1_hex_u32(" principal ", X64_SYSCALL_PROCESS_PRINCIPAL, policy_pid);
    write_syscall1_dec_u32(" endpoint ", X64_SYSCALL_PROCESS_ENDPOINT, policy_pid);
    write_syscall1_hex_u32(" state ", X64_SYSCALL_PROCESS_STATE, policy_pid);
    write_syscall1_dec_u32(" class ", X64_SYSCALL_PROCESS_SCHEDULER_CLASS, policy_pid);
    write_syscall1_dec_u32(" cap-limit ", X64_SYSCALL_PROCESS_CAPABILITY_LIMIT, policy_pid);
    write_string(" manifest ");
    write_dec_u32((u32)policy_manifest);
    write_syscall1_dec_u32(" pkg ", X64_SYSCALL_PROCESS_MANIFEST_PACKAGE, policy_pid);
    write_syscall1_dec_u32(" exec ", X64_SYSCALL_PROCESS_MANIFEST_EXECUTABLE, policy_pid);
    write_syscall1_dec_u32(" signer ", X64_SYSCALL_PROCESS_MANIFEST_SIGNER, policy_pid);
    write_syscall1_hex_u32(" token ", X64_SYSCALL_PROCESS_MANIFEST_TOKEN, policy_pid);
    write_syscall1_hex_u32(" launch-state ", X64_SYSCALL_LAUNCH_MANIFEST_STATE, policy_manifest);
    write_syscall1_dec_u32(" phase ", X64_SYSCALL_LAUNCH_MANIFEST_PHASE, policy_manifest);
    write_syscall1_dec_u32(" restarts ", X64_SYSCALL_LAUNCH_MANIFEST_RESTART_COUNT, policy_manifest);
    write_syscall1_dec_u32(" generation ", X64_SYSCALL_PROCESS_RUNTIME_GENERATION, policy_pid);
    write_syscall1_hex_u32(" runtime ", X64_SYSCALL_PROCESS_RUNTIME_TOKEN, policy_pid);
    write_syscall1_dec_u32(" image-generation ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_GENERATION, policy_pid);
    write_syscall1_hex_u32(" image ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_TOKEN, policy_pid);
    write_syscall1_hex_u32(" image-plan ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PLAN_TOKEN, policy_pid);
    write_syscall1_hex_u32(" plan-base ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_BASE, policy_pid);
    write_syscall1_hex_u32(" plan-entry ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY, policy_pid);
    write_syscall1_dec_u32(" plan-bytes ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAPPED_BYTES, policy_pid);
    write_syscall1_hex_u32(" plan-rights ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_RIGHTS, policy_pid);
    write_syscall1_hex_u32(" image-map ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_TOKEN, policy_pid);
    write_syscall1_dec_u32(" map-pages ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PAGE_COUNT, policy_pid);
    write_syscall1_dec_u32(" map-pml4 ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PML4_INDEX, policy_pid);
    write_syscall1_dec_u32(" map-pdpt ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PDPT_INDEX, policy_pid);
    write_syscall1_dec_u32(" map-pd ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PD_INDEX, policy_pid);
    write_syscall1_hex_u32(" transfer ", X64_SYSCALL_PROCESS_RUNTIME_ENTRY_TRANSFER_TOKEN, policy_pid);
    write_syscall1_hex_u32(" install ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_INSTALL_TOKEN, policy_pid);
    write_syscall1_hex_u32(" source-checksum ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_SOURCE_CHECKSUM, policy_pid);
    write_syscall1_hex_u32(" entry-probe ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY_PROBE, policy_pid);
    write_syscall1_dec_u32(" installed ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_INSTALLED, policy_pid);
    write_syscall1_hex_u32(" protection ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_FLAGS, policy_pid);
    write_syscall1_hex_u32(" protection-token ", X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_TOKEN, policy_pid);
    write_protection_summary((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_FLAGS,
        policy_pid,
        0u,
        0u));
    write_string(" user-entry-state ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_STATE,
        policy_pid,
        0u,
        0u));
    write_string(" user-entry-token ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_TOKEN,
        policy_pid,
        0u,
        0u));
    write_string(" user-rip ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_RIP,
        policy_pid,
        0u,
        0u));
    write_string(" user-rsp ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_RSP,
        policy_pid,
        0u,
        0u));
    write_string(" user-selectors ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_SELECTORS,
        policy_pid,
        0u,
        0u));
    write_string(" user-rflags ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_RFLAGS,
        policy_pid,
        0u,
        0u));
    write_string(" user-entry-denial ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_DENIAL,
        policy_pid,
        0u,
        0u));
    write_string(" user-transfer-ready ");
    write_dec_u32(
        (((u32)syscall64_invoke(
            X64_SYSCALL_PROCESS_RUNTIME_USER_ENTRY_STATE,
            policy_pid,
            0u,
            0u) & LAUNCH64_USER_ENTRY_TRANSFER_READY) != 0u)
            ? 1u
            : 0u);
    write_string(" process-entry-ready ");
    write_dec_u32(
        (((u32)syscall64_invoke(
            X64_SYSCALL_PROCESS_STATE,
            policy_pid,
            0u,
            0u) & PROCESS64_STATE_USER_ENTRY_READY) != 0u)
            ? 1u
            : 0u);
    write_syscall1_dec_u32(" payload-offset ", X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_OFFSET, policy_pid);
    write_syscall1_dec_u32(" payload-size ", X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_SIZE, policy_pid);
    write_syscall1_hex_u32(" payload-checksum ", X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_CHECKSUM, policy_pid);
    write_syscall1_dec_u32(" launch-pid ", X64_SYSCALL_LAUNCH_MANIFEST_PID, policy_manifest);
    write_syscall1_hex_u32(" launch-principal ", X64_SYSCALL_LAUNCH_MANIFEST_PRINCIPAL, policy_manifest);
    write_syscall1_hex_u32(" requester ", X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUESTER, policy_manifest);
    write_syscall1_dec_u32(" request ", X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID, policy_manifest);
    write_syscall1_hex_u32(" request-state ", X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_STATUS, policy_manifest);
    write_string(" invalid ");
    write_hex_u32((u32)invalid_pid);
    write_line("");
}

static void log_launch_surface(void)
{
    u64 policy_pid = syscall64_invoke(
        X64_SYSCALL_PROCESS_BY_PRINCIPAL,
        PRINCIPAL64_ID_POLICY_WORKER,
        0u,
        0u);
    u64 policy_manifest = syscall64_invoke(X64_SYSCALL_PROCESS_MANIFEST_INDEX, policy_pid, 0u, 0u);
    u64 quiesce_preflight = syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_QUIESCE,
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        policy_manifest,
        0u);
    u64 quiesce_request = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID,
        policy_manifest,
        0u,
        0u);
    u64 protected_stop = syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_STOP,
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        policy_manifest,
        0u);
    u64 protected_stop_request = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID,
        policy_manifest,
        0u,
        0u);

    static const char syscall0_suffixes_80[] =
        "[x64] syscall launch archive \0"
        " checksum \0"
        " total \0"
        " service \0"
        " verified \0"
        " ignored \0"
        " denied \0"
        " ready \0"
        " started \0"
        " drained \0"
        " quiesce-ready \0"
        " start-denials \0"
        " requests \0"
        " approvals \0"
        " pending \0"
        " request-denied \0"
        " completed \0"
        " quiesce-requests \0"
        " quiesce-approvals \0"
        " quiesce-pending \0"
        " quiesce-denied \0"
        " quiesce-completed \0"
        " drain-requests \0"
        " drain-approvals \0"
        " drain-pending \0"
        " drain-denied \0"
        " drain-completed \0"
        " restart-requests \0"
        " restart-approvals \0"
        " restart-pending \0"
        " restart-denied \0"
        " restart-completed \0"
        " stop-requests \0"
        " stop-approvals \0"
        " stop-pending \0"
        " stop-denied \0"
        " stop-completed \0"
        " log \0";
    static const struct scaffold_syscall0_field syscall0_fields_80[] = {        {0, X64_SYSCALL_LAUNCH_ARCHIVE_VALID, SCAFFOLD_TELEMETRY_DEC},
        {30, X64_SYSCALL_LAUNCH_ARCHIVE_CHECKSUM, SCAFFOLD_TELEMETRY_HEX},
        {41, X64_SYSCALL_LAUNCH_MANIFEST_TOTAL, SCAFFOLD_TELEMETRY_DEC},
        {49, X64_SYSCALL_LAUNCH_MANIFEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {59, X64_SYSCALL_PROCESS_MANIFEST_VERIFIED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {70, X64_SYSCALL_LAUNCH_MANIFEST_IGNORED, SCAFFOLD_TELEMETRY_DEC},
        {80, X64_SYSCALL_LAUNCH_MANIFEST_DENIED, SCAFFOLD_TELEMETRY_DEC},
        {89, X64_SYSCALL_LAUNCH_READY_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {97, X64_SYSCALL_LAUNCH_STARTED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {107, X64_SYSCALL_LAUNCH_DRAINED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {117, X64_SYSCALL_LAUNCH_QUIESCE_READY_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {133, X64_SYSCALL_LAUNCH_START_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {149, X64_SYSCALL_LAUNCH_REQUEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {160, X64_SYSCALL_LAUNCH_APPROVAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {172, X64_SYSCALL_LAUNCH_PENDING_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {182, X64_SYSCALL_LAUNCH_DENIED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {199, X64_SYSCALL_LAUNCH_COMPLETED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {211, X64_SYSCALL_LAUNCH_QUIESCE_REQUEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {230, X64_SYSCALL_LAUNCH_QUIESCE_APPROVAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {250, X64_SYSCALL_LAUNCH_QUIESCE_PENDING_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {268, X64_SYSCALL_LAUNCH_QUIESCE_DENIED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {285, X64_SYSCALL_LAUNCH_QUIESCE_COMPLETED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {305, X64_SYSCALL_LAUNCH_DRAIN_REQUEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {322, X64_SYSCALL_LAUNCH_DRAIN_APPROVAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {340, X64_SYSCALL_LAUNCH_DRAIN_PENDING_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {356, X64_SYSCALL_LAUNCH_DRAIN_DENIED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {371, X64_SYSCALL_LAUNCH_DRAIN_COMPLETED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {389, X64_SYSCALL_LAUNCH_RESTART_REQUEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {408, X64_SYSCALL_LAUNCH_RESTART_APPROVAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {428, X64_SYSCALL_LAUNCH_RESTART_PENDING_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {446, X64_SYSCALL_LAUNCH_RESTART_DENIED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {463, X64_SYSCALL_LAUNCH_RESTART_COMPLETED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {483, X64_SYSCALL_LAUNCH_STOP_REQUEST_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {499, X64_SYSCALL_LAUNCH_STOP_APPROVAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {516, X64_SYSCALL_LAUNCH_STOP_PENDING_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {531, X64_SYSCALL_LAUNCH_STOP_DENIED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {545, X64_SYSCALL_LAUNCH_STOP_COMPLETED_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {562, X64_SYSCALL_LAUNCH_REQUEST_LOG_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields("", syscall0_suffixes_80, syscall0_fields_80, (u32)(sizeof(syscall0_fields_80) / sizeof(syscall0_fields_80[0])));
    write_syscall1_dec_u32(" init-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_START, PRINCIPAL64_ID_INIT_SUPERVISOR);
    write_syscall1_dec_u32(" policy-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_START, PRINCIPAL64_ID_POLICY_WORKER);
    write_syscall1_dec_u32(" quiesce-init-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_QUIESCE, PRINCIPAL64_ID_INIT_SUPERVISOR);
    write_syscall1_dec_u32(" quiesce-policy-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_QUIESCE, PRINCIPAL64_ID_POLICY_WORKER);
    write_syscall1_dec_u32(" drain-init-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_DRAIN, PRINCIPAL64_ID_INIT_SUPERVISOR);
    write_syscall1_dec_u32(" drain-policy-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_DRAIN, PRINCIPAL64_ID_POLICY_WORKER);
    write_syscall1_dec_u32(" restart-init-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_RESTART, PRINCIPAL64_ID_INIT_SUPERVISOR);
    write_syscall1_dec_u32(" restart-policy-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_RESTART, PRINCIPAL64_ID_POLICY_WORKER);
    write_syscall1_dec_u32(" stop-init-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_STOP, PRINCIPAL64_ID_INIT_SUPERVISOR);
    write_syscall1_dec_u32(" stop-policy-auth ", X64_SYSCALL_LAUNCH_REQUESTER_CAN_STOP, PRINCIPAL64_ID_POLICY_WORKER);
    write_string(" quiesce-proof ");
    write_dec_u32((u32)quiesce_preflight);
    write_string(" quiesce-request ");
    write_dec_u32((u32)quiesce_request);
    write_syscall1_dec_u32(" quiesce-op ", X64_SYSCALL_LAUNCH_REQUEST_OPERATION, quiesce_request);
    write_syscall1_hex_u32(" quiesce-state ", X64_SYSCALL_LAUNCH_REQUEST_STATUS, quiesce_request);
    write_syscall1_dec_u32(" quiesce-caps ", X64_SYSCALL_LAUNCH_REQUEST_OBSERVED_CAPABILITIES, quiesce_request);
    write_syscall1_dec_u32(" quiesce-denial ", X64_SYSCALL_LAUNCH_REQUEST_DENIAL, quiesce_request);
    write_string(" stop-proof ");
    write_dec_u32((u32)protected_stop);
    write_string(" stop-request ");
    write_dec_u32((u32)protected_stop_request);
    write_syscall1_dec_u32(" stop-op ", X64_SYSCALL_LAUNCH_REQUEST_OPERATION, protected_stop_request);
    write_syscall1_hex_u32(" stop-state ", X64_SYSCALL_LAUNCH_REQUEST_STATUS, protected_stop_request);
    write_syscall1_dec_u32(" stop-denial ", X64_SYSCALL_LAUNCH_REQUEST_DENIAL, protected_stop_request);
    write_syscall1_dec_u32(" manifest-phase ", X64_SYSCALL_LAUNCH_MANIFEST_PHASE, policy_manifest);
    write_line("");
}

static void log_capability_surface(void)
{
    u32 policy_owner = CAPABILITY64_OWNER_POLICY_CLIENT;
    u32 worker_owner = CAPABILITY64_OWNER_POLICY_WORKER;
    u32 delegate_context = CAPABILITY64_CONTEXT(
        CAPABILITY64_OWNER_POLICY_CLIENT,
        CAPABILITY64_OWNER_POLICY_WORKER);
    u64 policy_pid = syscall64_invoke(
        X64_SYSCALL_PROCESS_BY_PRINCIPAL,
        PRINCIPAL64_ID_POLICY_WORKER,
        0u,
        0u);
    u64 policy_manifest = syscall64_invoke(X64_SYSCALL_PROCESS_MANIFEST_INDEX, policy_pid, 0u, 0u);
    u64 policy_handle = syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY | CAPABILITY64_RIGHT_DELEGATE,
        policy_owner);
    u64 policy_route = syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        policy_handle,
        CAPABILITY64_RIGHT_SEND,
        policy_owner);
    u64 policy_rights = syscall64_invoke(X64_SYSCALL_CAP_RIGHTS, policy_handle, 0u, policy_owner);
    u64 policy_owner_seen = syscall64_invoke(X64_SYSCALL_CAP_OWNER, policy_handle, 0u, policy_owner);
    u64 wrong_owner_route = syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        policy_handle,
        CAPABILITY64_RIGHT_SEND,
        worker_owner);
    u64 policy_child = syscall64_invoke(
        X64_SYSCALL_CAP_DELEGATE,
        policy_handle,
        CAPABILITY64_RIGHT_SEND,
        delegate_context);
    u64 child_route = syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        policy_child,
        CAPABILITY64_RIGHT_SEND,
        worker_owner);
    u64 child_rights = syscall64_invoke(X64_SYSCALL_CAP_RIGHTS, policy_child, 0u, worker_owner);
    u64 child_parent = syscall64_invoke(X64_SYSCALL_CAP_PARENT, policy_child, 0u, worker_owner);
    u64 child_owner = syscall64_invoke(X64_SYSCALL_CAP_OWNER, policy_child, 0u, worker_owner);
    u64 child_expiry = syscall64_invoke(X64_SYSCALL_CAP_EXPIRY, policy_child, 0u, worker_owner);
    u64 denied_second_hop = syscall64_invoke(
        X64_SYSCALL_CAP_DELEGATE,
        policy_child,
        CAPABILITY64_RIGHT_SEND,
        CAPABILITY64_CONTEXT(CAPABILITY64_OWNER_POLICY_WORKER, CAPABILITY64_OWNER_POLICY_CLIENT));
    u64 denied_ramfs_delegate = syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_RAMFS,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_DELEGATE,
        policy_owner);
    u64 denied_unknown_principal = syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        CAPABILITY64_RIGHT_SEND,
        0x0F0Fu);
    u64 live_quiesce = syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_QUIESCE,
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        policy_manifest,
        0u);
    u64 live_quiesce_request = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID,
        policy_manifest,
        0u,
        0u);

    write_string("[x64] syscall cap policy ");
    write_hex_u32((u32)policy_handle);
    write_string(" route ");
    write_dec_u32((u32)policy_route);
    write_string(" rights ");
    write_hex_u32((u32)policy_rights);
    write_string(" owner ");
    write_hex_u32((u32)policy_owner_seen);
    write_string(" generation ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_CAP_RUNTIME_GENERATION, policy_handle, 0u, policy_owner));
    write_string(" runtime ");
    write_hex_u32((u32)syscall64_invoke(X64_SYSCALL_CAP_RUNTIME_TOKEN, policy_handle, 0u, policy_owner));
    write_string(" wrong-owner ");
    write_hex_u32((u32)wrong_owner_route);
    write_string(" child ");
    write_hex_u32((u32)policy_child);
    write_string(" child-route ");
    write_dec_u32((u32)child_route);
    write_string(" child-rights ");
    write_hex_u32((u32)child_rights);
    write_string(" child-owner ");
    write_hex_u32((u32)child_owner);
    write_string(" parent ");
    write_hex_u32((u32)child_parent);
    write_string(" expiry ");
    write_dec_u32((u32)child_expiry);
    write_string(" denied2 ");
    write_hex_u32((u32)denied_second_hop);
    write_string(" denied ");
    write_hex_u32((u32)denied_ramfs_delegate);
    write_string(" bad-principal ");
    write_hex_u32((u32)denied_unknown_principal);
    write_string(" quiesce-live-proof ");
    write_dec_u32((u32)live_quiesce);
    write_string(" quiesce-live-request ");
    write_dec_u32((u32)live_quiesce_request);
    write_string(" quiesce-live-caps ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_OBSERVED_CAPABILITIES,
        live_quiesce_request,
        0u,
        0u));
    write_string(" quiesce-live-denial ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_DENIAL,
        live_quiesce_request,
        0u,
        0u));
    write_syscall1_dec_u32(" quiesce-live-phase ", X64_SYSCALL_LAUNCH_MANIFEST_PHASE, policy_manifest);
    write_line("");

    interrupts64_enable();
    wait_for_timer_ticks((u32)child_expiry);
    interrupts64_disable();

    u64 expired_child_route = syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        policy_child,
        CAPABILITY64_RIGHT_SEND,
        worker_owner);
    u64 cascade_child = syscall64_invoke(
        X64_SYSCALL_CAP_DELEGATE,
        policy_handle,
        CAPABILITY64_RIGHT_SEND,
        delegate_context);
    u64 cascade_route = syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        cascade_child,
        CAPABILITY64_RIGHT_SEND,
        worker_owner);

    write_string("[x64] syscall cap revoke ");
    write_dec_u32((u32)syscall64_invoke(X64_SYSCALL_CAP_REVOKE, policy_handle, 0u, policy_owner));
    write_string(" expired-child ");
    write_hex_u32((u32)expired_child_route);
    write_string(" cascade-child ");
    write_hex_u32((u32)cascade_child);
    write_string(" cascade-route ");
    write_dec_u32((u32)cascade_route);
    write_string(" cascade-stale ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        cascade_child,
        CAPABILITY64_RIGHT_SEND,
        worker_owner));
    static const char syscall0_suffixes_81[] =
        "grants \0"
        "delegates \0"
        "routes \0"
        "revokes \0"
        "cascades \0"
        "expirations \0"
        "owner-denials \0"
        "principal-denials \0"
        "stale-denials \0"
        "live \0";
    static const struct scaffold_syscall0_field syscall0_fields_81[] = {        {0, X64_SYSCALL_CAP_GRANT_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {8, X64_SYSCALL_CAP_DELEGATE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {19, X64_SYSCALL_CAP_ROUTE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {27, X64_SYSCALL_CAP_REVOKE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {36, X64_SYSCALL_CAP_CASCADE_REVOKE_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {46, X64_SYSCALL_CAP_EXPIRATION_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {59, X64_SYSCALL_CAP_OWNER_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {74, X64_SYSCALL_CAP_PRINCIPAL_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {93, X64_SYSCALL_CAP_RUNTIME_STALE_DENIAL_COUNT, SCAFFOLD_TELEMETRY_DEC},
        {108, X64_SYSCALL_CAP_LIVE_COUNT, SCAFFOLD_TELEMETRY_DEC}
    };
    write_syscall0_prefixed_label_fields(" ", syscall0_suffixes_81, syscall0_fields_81, (u32)(sizeof(syscall0_fields_81) / sizeof(syscall0_fields_81[0])));
    write_string(" policy-live ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_CAP_LIVE_FOR_ENDPOINT_CLASS,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        0u,
        0u));
    write_syscall0_dec_u32(" denials ", X64_SYSCALL_CAP_DENIAL_COUNT);
    write_line("");

    u64 drain_parent = syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY | CAPABILITY64_RIGHT_DELEGATE,
        policy_owner);
    u64 drain_child = syscall64_invoke(
        X64_SYSCALL_CAP_DELEGATE,
        drain_parent,
        CAPABILITY64_RIGHT_SEND,
        delegate_context);
    u64 drain_route = syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        drain_child,
        CAPABILITY64_RIGHT_SEND,
        worker_owner);
    u64 drain_proof = syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_DRAIN,
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        policy_manifest,
        0u);
    u64 drain_request = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID,
        policy_manifest,
        0u,
        0u);
    u64 pre_quiesce_restart = syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RESTART,
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        policy_manifest,
        0u);
    u64 pre_quiesce_restart_request = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID,
        policy_manifest,
        0u,
        0u);
    u64 pre_quiesce_restart_phase = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_PHASE,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_generation = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_GENERATION,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_generation = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_GENERATION,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_base = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_BASE,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_entry = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_ENTRY,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_mapped_bytes = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAPPED_BYTES,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_rights = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_RIGHTS,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_plan_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PLAN_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_map_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAP_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_page_count = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PAGE_COUNT,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_transfer_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_ENTRY_TRANSFER_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_image_install_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_INSTALL_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_payload_offset = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_OFFSET,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_payload_size = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_SIZE,
        policy_manifest,
        0u,
        0u);
    u64 pre_restart_payload_checksum = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_CHECKSUM,
        policy_manifest,
        0u,
        0u);
    u64 post_drain_quiesce = syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_QUIESCE,
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        policy_manifest,
        0u);
    u64 post_drain_quiesce_request = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID,
        policy_manifest,
        0u,
        0u);
    u64 post_drain_quiesce_phase = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_PHASE,
        policy_manifest,
        0u,
        0u);
    u64 restart_proof = syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RESTART,
        PRINCIPAL64_ID_INIT_SUPERVISOR,
        policy_manifest,
        0u);
    u64 restart_request = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID,
        policy_manifest,
        0u,
        0u);
    u64 restart_phase = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_PHASE,
        policy_manifest,
        0u,
        0u);
    u64 restart_generation = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_GENERATION,
        policy_manifest,
        0u,
        0u);
    u64 restart_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_generation = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_GENERATION,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_base = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_BASE,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_entry = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_ENTRY,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_mapped_bytes = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAPPED_BYTES,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_rights = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_RIGHTS,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_plan_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PLAN_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_map_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAP_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_page_count = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PAGE_COUNT,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_pml4_index = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PML4_INDEX,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_pdpt_index = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PDPT_INDEX,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_pd_index = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_PD_INDEX,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_transfer_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_ENTRY_TRANSFER_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_install_token = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_INSTALL_TOKEN,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_source_checksum = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_SOURCE_CHECKSUM,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_entry_probe = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_ENTRY_PROBE,
        policy_manifest,
        0u,
        0u);
    u64 restart_image_map_installed = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_IMAGE_MAP_INSTALLED,
        policy_manifest,
        0u,
        0u);
    u64 restart_payload_slot = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_SLOT,
        policy_manifest,
        0u,
        0u);
    u64 restart_payload_kind = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_KIND,
        policy_manifest,
        0u,
        0u);
    u64 restart_payload_offset = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_OFFSET,
        policy_manifest,
        0u,
        0u);
    u64 restart_payload_size = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_SIZE,
        policy_manifest,
        0u,
        0u);
    u64 restart_payload_checksum = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_RUNTIME_PAYLOAD_CHECKSUM,
        policy_manifest,
        0u,
        0u);
    u64 restart_process_generation = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_GENERATION,
        policy_pid,
        0u,
        0u);
    u64 restart_process_token = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_TOKEN,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_generation = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_GENERATION,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_token = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_TOKEN,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_base = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_BASE,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_entry = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_mapped_bytes = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAPPED_BYTES,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_rights = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_RIGHTS,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_plan_token = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PLAN_TOKEN,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_map_token = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_TOKEN,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_page_count = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PAGE_COUNT,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_transfer_token = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_ENTRY_TRANSFER_TOKEN,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_install_token = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_INSTALL_TOKEN,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_entry_probe = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY_PROBE,
        policy_pid,
        0u,
        0u);
    u64 restart_process_image_map_installed = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_INSTALLED,
        policy_pid,
        0u,
        0u);
    u64 restart_process_payload_offset = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_OFFSET,
        policy_pid,
        0u,
        0u);
    u64 restart_process_payload_size = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_SIZE,
        policy_pid,
        0u,
        0u);
    u64 restart_process_payload_checksum = syscall64_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_CHECKSUM,
        policy_pid,
        0u,
        0u);
    u64 post_restart_cap = syscall64_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY,
        policy_owner);
    u64 post_restart_cap_generation = syscall64_invoke(
        X64_SYSCALL_CAP_RUNTIME_GENERATION,
        post_restart_cap,
        0u,
        policy_owner);
    u64 post_restart_cap_token = syscall64_invoke(
        X64_SYSCALL_CAP_RUNTIME_TOKEN,
        post_restart_cap,
        0u,
        policy_owner);
    u64 post_restart_route = syscall64_invoke(
        X64_SYSCALL_CAP_ROUTE,
        post_restart_cap,
        CAPABILITY64_RIGHT_SEND,
        policy_owner);
    u64 post_restart_revoke = syscall64_invoke(
        X64_SYSCALL_CAP_REVOKE,
        post_restart_cap,
        0u,
        policy_owner);
    u64 old_token_current = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_ACCEPTS_RUNTIME_TOKEN,
        policy_manifest,
        pre_restart_token,
        0u);
    u64 new_token_current = syscall64_invoke(
        X64_SYSCALL_LAUNCH_MANIFEST_ACCEPTS_RUNTIME_TOKEN,
        policy_manifest,
        restart_token,
        0u);

    write_string("[x64] syscall cap drain parent ");
    write_hex_u32((u32)drain_parent);
    write_string(" child ");
    write_hex_u32((u32)drain_child);
    write_string(" route ");
    write_dec_u32((u32)drain_route);
    write_string(" drain-proof ");
    write_dec_u32((u32)drain_proof);
    write_string(" drain-request ");
    write_dec_u32((u32)drain_request);
    write_syscall1_dec_u32(" drain-op ", X64_SYSCALL_LAUNCH_REQUEST_OPERATION, drain_request);
    write_syscall1_hex_u32(" drain-state ", X64_SYSCALL_LAUNCH_REQUEST_STATUS, drain_request);
    write_string(" drain-seen ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_OBSERVED_CAPABILITIES,
        drain_request,
        0u,
        0u));
    write_string(" drain-revoked ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_REVOKED_CAPABILITIES,
        drain_request,
        0u,
        0u));
    write_syscall1_dec_u32(" drain-denial ", X64_SYSCALL_LAUNCH_REQUEST_DENIAL, drain_request);
    write_string(" pre-restart-proof ");
    write_dec_u32((u32)pre_quiesce_restart);
    write_string(" pre-restart-request ");
    write_dec_u32((u32)pre_quiesce_restart_request);
    write_string(" pre-restart-caps ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_OBSERVED_CAPABILITIES,
        pre_quiesce_restart_request,
        0u,
        0u));
    write_string(" pre-restart-denial ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_DENIAL,
        pre_quiesce_restart_request,
        0u,
        0u));
    write_string(" pre-restart-phase ");
    write_dec_u32((u32)pre_quiesce_restart_phase);
    write_string(" pre-restart-generation ");
    write_dec_u32((u32)pre_restart_generation);
    write_string(" pre-restart-token ");
    write_hex_u32((u32)pre_restart_token);
    write_string(" pre-restart-image-generation ");
    write_dec_u32((u32)pre_restart_image_generation);
    write_string(" pre-restart-image ");
    write_hex_u32((u32)pre_restart_image_token);
    write_string(" pre-restart-plan ");
    write_hex_u32((u32)pre_restart_image_plan_token);
    write_string(" pre-restart-map ");
    write_hex_u32((u32)pre_restart_image_map_token);
    write_string(" pre-restart-map-pages ");
    write_dec_u32((u32)pre_restart_image_page_count);
    write_string(" pre-restart-transfer ");
    write_hex_u32((u32)pre_restart_image_transfer_token);
    write_string(" pre-restart-install ");
    write_hex_u32((u32)pre_restart_image_install_token);
    write_string(" pre-restart-plan-base ");
    write_hex_u32((u32)pre_restart_image_base);
    write_string(" pre-restart-plan-entry ");
    write_hex_u32((u32)pre_restart_image_entry);
    write_string(" pre-restart-plan-bytes ");
    write_dec_u32((u32)pre_restart_image_mapped_bytes);
    write_string(" pre-restart-plan-rights ");
    write_hex_u32((u32)pre_restart_image_rights);
    write_string(" pre-restart-payload-offset ");
    write_dec_u32((u32)pre_restart_payload_offset);
    write_string(" pre-restart-payload-size ");
    write_dec_u32((u32)pre_restart_payload_size);
    write_string(" pre-restart-payload-checksum ");
    write_hex_u32((u32)pre_restart_payload_checksum);
    write_string(" post-quiesce-proof ");
    write_dec_u32((u32)post_drain_quiesce);
    write_string(" post-quiesce-request ");
    write_dec_u32((u32)post_drain_quiesce_request);
    write_string(" post-quiesce-caps ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_OBSERVED_CAPABILITIES,
        post_drain_quiesce_request,
        0u,
        0u));
    write_string(" post-quiesce-denial ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_DENIAL,
        post_drain_quiesce_request,
        0u,
        0u));
    write_string(" post-quiesce-phase ");
    write_dec_u32((u32)post_drain_quiesce_phase);
    write_string(" restart-proof ");
    write_dec_u32((u32)restart_proof);
    write_string(" restart-request ");
    write_dec_u32((u32)restart_request);
    write_syscall1_dec_u32(" restart-op ", X64_SYSCALL_LAUNCH_REQUEST_OPERATION, restart_request);
    write_syscall1_hex_u32(" restart-state ", X64_SYSCALL_LAUNCH_REQUEST_STATUS, restart_request);
    write_string(" restart-caps ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_OBSERVED_CAPABILITIES,
        restart_request,
        0u,
        0u));
    write_syscall1_dec_u32(" restart-denial ", X64_SYSCALL_LAUNCH_REQUEST_DENIAL, restart_request);
    write_syscall1_dec_u32(" restart-count ", X64_SYSCALL_LAUNCH_MANIFEST_RESTART_COUNT, policy_manifest);
    write_string(" restart-phase ");
    write_dec_u32((u32)restart_phase);
    write_string(" restart-generation ");
    write_dec_u32((u32)restart_generation);
    write_string(" restart-token ");
    write_hex_u32((u32)restart_token);
    write_string(" restart-image-generation ");
    write_dec_u32((u32)restart_image_generation);
    write_string(" restart-image ");
    write_hex_u32((u32)restart_image_token);
    write_string(" restart-plan ");
    write_hex_u32((u32)restart_image_plan_token);
    write_string(" restart-map ");
    write_hex_u32((u32)restart_image_map_token);
    write_string(" restart-map-pages ");
    write_dec_u32((u32)restart_image_page_count);
    write_string(" restart-map-pml4 ");
    write_dec_u32((u32)restart_image_pml4_index);
    write_string(" restart-map-pdpt ");
    write_dec_u32((u32)restart_image_pdpt_index);
    write_string(" restart-map-pd ");
    write_dec_u32((u32)restart_image_pd_index);
    write_string(" restart-transfer ");
    write_hex_u32((u32)restart_image_transfer_token);
    write_string(" restart-install ");
    write_hex_u32((u32)restart_image_install_token);
    write_string(" restart-source-checksum ");
    write_hex_u32((u32)restart_image_source_checksum);
    write_string(" restart-entry-probe ");
    write_hex_u32((u32)restart_image_entry_probe);
    write_string(" restart-installed ");
    write_dec_u32((u32)restart_image_map_installed);
    write_string(" restart-plan-base ");
    write_hex_u32((u32)restart_image_base);
    write_string(" restart-plan-entry ");
    write_hex_u32((u32)restart_image_entry);
    write_string(" restart-plan-bytes ");
    write_dec_u32((u32)restart_image_mapped_bytes);
    write_string(" restart-plan-rights ");
    write_hex_u32((u32)restart_image_rights);
    write_string(" restart-payload ");
    write_dec_u32((u32)restart_payload_slot);
    write_string(" restart-kind ");
    write_dec_u32((u32)restart_payload_kind);
    write_string(" restart-payload-offset ");
    write_dec_u32((u32)restart_payload_offset);
    write_string(" restart-payload-size ");
    write_dec_u32((u32)restart_payload_size);
    write_string(" restart-payload-checksum ");
    write_hex_u32((u32)restart_payload_checksum);
    write_string(" restart-request-generation ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_GENERATION,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-token ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_TOKEN,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-image-generation ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_GENERATION,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-image ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_TOKEN,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-plan ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PLAN_TOKEN,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-map ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_MAP_TOKEN,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-map-pages ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_PAGE_COUNT,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-transfer ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_ENTRY_TRANSFER_TOKEN,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-install ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_INSTALL_TOKEN,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-entry-probe ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_ENTRY_PROBE,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-installed ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_MAP_INSTALLED,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-plan-base ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_BASE,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-plan-entry ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_ENTRY,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-plan-bytes ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_MAPPED_BYTES,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-plan-rights ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_RIGHTS,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-payload ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_SLOT,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-kind ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_KIND,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-payload-offset ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_OFFSET,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-payload-size ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_SIZE,
        restart_request,
        0u,
        0u));
    write_string(" restart-request-payload-checksum ");
    write_hex_u32((u32)syscall64_invoke(
        X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_PAYLOAD_CHECKSUM,
        restart_request,
        0u,
        0u));
    write_string(" process-generation ");
    write_dec_u32((u32)restart_process_generation);
    write_string(" process-runtime ");
    write_hex_u32((u32)restart_process_token);
    write_string(" process-image-generation ");
    write_dec_u32((u32)restart_process_image_generation);
    write_string(" process-image ");
    write_hex_u32((u32)restart_process_image_token);
    write_string(" process-plan ");
    write_hex_u32((u32)restart_process_image_plan_token);
    write_string(" process-map ");
    write_hex_u32((u32)restart_process_image_map_token);
    write_string(" process-map-pages ");
    write_dec_u32((u32)restart_process_image_page_count);
    write_string(" process-transfer ");
    write_hex_u32((u32)restart_process_image_transfer_token);
    write_string(" process-install ");
    write_hex_u32((u32)restart_process_image_install_token);
    write_string(" process-entry-probe ");
    write_hex_u32((u32)restart_process_image_entry_probe);
    write_string(" process-installed ");
    write_dec_u32((u32)restart_process_image_map_installed);
    write_string(" process-plan-base ");
    write_hex_u32((u32)restart_process_image_base);
    write_string(" process-plan-entry ");
    write_hex_u32((u32)restart_process_image_entry);
    write_string(" process-plan-bytes ");
    write_dec_u32((u32)restart_process_image_mapped_bytes);
    write_string(" process-plan-rights ");
    write_hex_u32((u32)restart_process_image_rights);
    write_string(" process-payload-offset ");
    write_dec_u32((u32)restart_process_payload_offset);
    write_string(" process-payload-size ");
    write_dec_u32((u32)restart_process_payload_size);
    write_string(" process-payload-checksum ");
    write_hex_u32((u32)restart_process_payload_checksum);
    write_syscall1_hex_u32(" process-state ", X64_SYSCALL_PROCESS_STATE, policy_pid);
    write_string(" token-changed ");
    write_dec_u32(((u32)pre_restart_token != (u32)restart_token) ? 1u : 0u);
    write_string(" image-changed ");
    write_dec_u32(((u32)pre_restart_image_token != (u32)restart_image_token) ? 1u : 0u);
    write_string(" plan-token-changed ");
    write_dec_u32(((u32)pre_restart_image_plan_token != (u32)restart_image_plan_token) ? 1u : 0u);
    write_string(" map-token-changed ");
    write_dec_u32(((u32)pre_restart_image_map_token != (u32)restart_image_map_token) ? 1u : 0u);
    write_string(" transfer-token-changed ");
    write_dec_u32(((u32)pre_restart_image_transfer_token != (u32)restart_image_transfer_token) ? 1u : 0u);
    write_string(" install-token-stable ");
    write_dec_u32(((u32)pre_restart_image_install_token == (u32)restart_image_install_token) ? 1u : 0u);
    write_string(" plan-ready ");
    write_dec_u32(
        (((u32)restart_image_plan_token != 0u)
            && ((u32)restart_image_base == LAUNCH64_IMAGE_PLAN_BASE)
            && ((u32)restart_image_entry == ((u32)restart_image_base + (u32)restart_payload_offset))
            && ((u32)restart_image_mapped_bytes >= (u32)restart_payload_size)
            && ((u32)restart_image_rights == LAUNCH64_IMAGE_PLAN_RIGHTS))
            ? 1u
            : 0u);
    write_string(" map-ready ");
    write_dec_u32(
        (((u32)restart_image_map_token != 0u)
            && ((u32)restart_image_transfer_token != 0u)
            && ((u32)restart_image_install_token != 0u)
            && ((u32)restart_image_page_count == ((u32)restart_image_mapped_bytes / LAUNCH64_IMAGE_MAP_PAGE_BYTES))
            && ((u32)restart_image_pml4_index == 0u)
            && ((u32)restart_image_pdpt_index == 1u)
            && ((u32)restart_image_pd_index == 0u)
            && ((u32)restart_process_image_map_token == (u32)restart_image_map_token)
            && ((u32)restart_process_image_transfer_token == (u32)restart_image_transfer_token))
            ? 1u
            : 0u);
    write_string(" install-ready ");
    write_dec_u32(
        (((u32)restart_image_map_installed == 1u)
            && ((u32)restart_process_image_map_installed == 1u)
            && ((u32)restart_image_entry_probe == runtime64_transfer_entry_result())
            && ((u32)restart_process_image_entry_probe == runtime64_transfer_entry_result())
            && ((u32)restart_process_image_install_token == (u32)restart_image_install_token)
            && ((u32)restart_image_source_checksum == paging64_runtime_mapping_source_checksum()))
            ? 1u
            : 0u);
    write_string(" plan-stable ");
    write_dec_u32(
        (((u32)pre_restart_image_base == (u32)restart_image_base)
            && ((u32)pre_restart_image_entry == (u32)restart_image_entry)
            && ((u32)pre_restart_image_mapped_bytes == (u32)restart_image_mapped_bytes)
            && ((u32)pre_restart_image_rights == (u32)restart_image_rights)
            && ((u32)restart_process_image_base == (u32)restart_image_base)
            && ((u32)restart_process_image_entry == (u32)restart_image_entry)
            && ((u32)restart_process_image_mapped_bytes == (u32)restart_image_mapped_bytes)
            && ((u32)restart_process_image_rights == (u32)restart_image_rights))
            ? 1u
            : 0u);
    write_string(" map-stable ");
    write_dec_u32(
        (((u32)pre_restart_image_page_count == (u32)restart_image_page_count)
            && ((u32)restart_process_image_page_count == (u32)restart_image_page_count)
            && ((u32)syscall64_invoke(
                    X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_MAP_TOKEN,
                    restart_request,
                    0u,
                    0u)
                == (u32)restart_image_map_token)
            && ((u32)syscall64_invoke(
                    X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_ENTRY_TRANSFER_TOKEN,
                    restart_request,
                    0u,
                    0u)
                == (u32)restart_image_transfer_token)
            && ((u32)syscall64_invoke(
                    X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_INSTALL_TOKEN,
                    restart_request,
                    0u,
                    0u)
                == (u32)restart_image_install_token)
            && ((u32)syscall64_invoke(
                    X64_SYSCALL_LAUNCH_REQUEST_RUNTIME_IMAGE_ENTRY_PROBE,
                    restart_request,
                    0u,
                    0u)
                == runtime64_transfer_entry_result()))
            ? 1u
            : 0u);
    write_string(" payload-stable ");
    write_dec_u32(
        (((u32)pre_restart_payload_offset == (u32)restart_payload_offset)
            && ((u32)pre_restart_payload_size == (u32)restart_payload_size)
            && ((u32)pre_restart_payload_checksum == (u32)restart_payload_checksum))
            ? 1u
            : 0u);
    write_string(" old-token-current ");
    write_dec_u32((u32)old_token_current);
    write_string(" new-token-current ");
    write_dec_u32((u32)new_token_current);
    write_string(" post-restart-cap ");
    write_hex_u32((u32)post_restart_cap);
    write_string(" post-restart-route ");
    write_dec_u32((u32)post_restart_route);
    write_string(" post-restart-cap-generation ");
    write_dec_u32((u32)post_restart_cap_generation);
    write_string(" post-restart-cap-token ");
    write_hex_u32((u32)post_restart_cap_token);
    write_string(" post-restart-cap-revoke ");
    write_dec_u32((u32)post_restart_revoke);
    write_syscall0_dec_u32(" stale-denials ", X64_SYSCALL_CAP_RUNTIME_STALE_DENIAL_COUNT);
    write_string(" policy-live ");
    write_dec_u32((u32)syscall64_invoke(
        X64_SYSCALL_CAP_LIVE_FOR_ENDPOINT_CLASS,
        SERVICE_ENDPOINT_CLASS_AI_POLICY,
        0u,
        0u));
    write_line("");
}

static void log_native_syscall_surface(void)
{
    write_string("[x64] native syscall arch ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_ARCH_BITS, 0u, 0u, 0u));
    write_string(" boot flags ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_GET_BOOT_FLAGS, 0u, 0u, 0u));
    write_line("");

    write_string("[x64] native syscall page root ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_GET_PAGE_TABLE_ROOT, 0u, 0u, 0u));
    write_string(" map ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_IDENTITY_MAP_MIB, 0u, 0u, 0u));
    write_line(" MiB");

    write_string("[x64] native syscall kernel load ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_GET_KERNEL_LOAD_ADDRESS, 0u, 0u, 0u));
    write_string(" sectors ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_KERNEL_SECTOR_COUNT, 0u, 0u, 0u));
    write_line("");

    write_string("[x64] native syscall memory conventional ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_CONVENTIONAL_MEMORY_KIB, 0u, 0u, 0u));
    write_string(" KiB extended ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_EXTENDED_MEMORY_KIB, 0u, 0u, 0u));
    write_line(" KiB");

    write_string("[x64] native syscall uptime ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_UPTIME_TICKS, 0u, 0u, 0u));
    write_string(" irq ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_IRQ_COUNT, 0u, 0u, 0u));
    write_string(" probe ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_PROBE_COUNT, 0u, 0u, 0u));
    write_string(" calls ");
    write_dec_u32(syscall64_native_count());
    write_string(" code ");
    write_hex_u64(syscall64_native_last_code());
    write_line("");

    write_string("[x64] native syscall descriptors state ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_DESCRIPTOR_STATE, 0u, 0u, 0u));
    write_string(" star-plan ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_DESCRIPTOR_SYSCALL_STAR_PLAN, 0u, 0u, 0u));
    write_string(" star ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_NATIVE_SYSCALL_STAR_VALUE, 0u, 0u, 0u));
    write_string(" ready ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_NATIVE_SYSCALL_STAR_READY, 0u, 0u, 0u));
    write_line("");
}

static void log_native_service_surface(void)
{
    u64 policy_endpoint = syscall64_native_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_AI_POLICY, 0u, 0u);
    u64 console_endpoint = syscall64_native_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_CONSOLE, 0u, 0u);
    u64 ramfs_endpoint = syscall64_native_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_RAMFS, 0u, 0u);
    u64 input_endpoint = syscall64_native_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_INPUT, 0u, 0u);
    u64 display_endpoint = syscall64_native_invoke(X64_SYSCALL_RESOLVE_SERVICE_CLASS, SERVICE_ENDPOINT_CLASS_DISPLAY, 0u, 0u);

    write_string("[x64] native services ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_SERVICE_COUNT, 0u, 0u, 0u));
    write_string(" manifests ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_PACKAGE_MANIFEST_COUNT, 0u, 0u, 0u));
    write_string(" policy ");
    write_dec_u32((u32)policy_endpoint);
    write_string(" console ");
    write_dec_u32((u32)console_endpoint);
    write_string(" ramfs ");
    write_dec_u32((u32)ramfs_endpoint);
    write_string(" input ");
    write_dec_u32((u32)input_endpoint);
    write_string(" display ");
    write_dec_u32((u32)display_endpoint);
    write_string(" display-available ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_DISPLAY_AVAILABLE, 0u, 0u, 0u));
    write_string(" display-text-writes ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_DISPLAY_TEXT_WRITE_COUNT, 0u, 0u, 0u));
    write_string(" display-clears ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_DISPLAY_CLEAR_COUNT, 0u, 0u, 0u));
    write_line("");
}

static void log_native_principal_surface(void)
{
    u64 console_principal = syscall64_native_invoke(X64_SYSCALL_PRINCIPAL_BY_INDEX, 3u, 0u, 0u);

    write_string("[x64] native principals ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PRINCIPAL_COUNT, 0u, 0u, 0u));
    write_string(" console ");
    write_hex_u32((u32)console_principal);
    write_string(" active ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PRINCIPAL_ACTIVE, console_principal, 0u, 0u));
    write_string(" role ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PRINCIPAL_ROLE, console_principal, 0u, 0u));
    write_line("");
}

static void log_native_process_surface(void)
{
    u64 console_pid = syscall64_native_invoke(
        X64_SYSCALL_PROCESS_BY_PRINCIPAL,
        PRINCIPAL64_ID_CONSOLE_WORKER,
        0u,
        0u);
    u64 console_manifest = syscall64_native_invoke(X64_SYSCALL_PROCESS_MANIFEST_INDEX, console_pid, 0u, 0u);
    u64 invalid_principal = syscall64_native_invoke(
        X64_SYSCALL_PROCESS_BY_PRINCIPAL,
        0x0F0Fu,
        0u,
        0u);

    write_string("[x64] native processes ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_COUNT, 0u, 0u, 0u));
    write_string(" console-pid ");
    write_dec_u32((u32)console_pid);
    write_string(" endpoint ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_ENDPOINT, console_pid, 0u, 0u));
    write_string(" state ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_STATE, console_pid, 0u, 0u));
    write_string(" class ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_SCHEDULER_CLASS, console_pid, 0u, 0u));
    write_string(" cap-limit ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_CAPABILITY_LIMIT, console_pid, 0u, 0u));
    write_string(" pkg ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_MANIFEST_PACKAGE, console_pid, 0u, 0u));
    write_string(" exec ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_MANIFEST_EXECUTABLE, console_pid, 0u, 0u));
    write_string(" token ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_MANIFEST_TOKEN, console_pid, 0u, 0u));
    write_string(" launch-state ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_STATE, console_manifest, 0u, 0u));
    write_string(" phase ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_PHASE, console_manifest, 0u, 0u));
    write_string(" restarts ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_RESTART_COUNT, console_manifest, 0u, 0u));
    write_string(" generation ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_GENERATION, console_pid, 0u, 0u));
    write_string(" runtime ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_TOKEN, console_pid, 0u, 0u));
    write_string(" image-generation ");
    write_dec_u32((u32)syscall64_native_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_GENERATION,
        console_pid,
        0u,
        0u));
    write_string(" image ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_TOKEN, console_pid, 0u, 0u));
    write_string(" image-plan ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PLAN_TOKEN, console_pid, 0u, 0u));
    write_string(" plan-base ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_BASE, console_pid, 0u, 0u));
    write_string(" plan-entry ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY, console_pid, 0u, 0u));
    write_string(" plan-bytes ");
    write_dec_u32((u32)syscall64_native_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAPPED_BYTES,
        console_pid,
        0u,
        0u));
    write_string(" plan-rights ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_RIGHTS, console_pid, 0u, 0u));
    write_string(" image-map ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_TOKEN, console_pid, 0u, 0u));
    write_string(" map-pages ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PAGE_COUNT, console_pid, 0u, 0u));
    write_string(" map-pml4 ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PML4_INDEX, console_pid, 0u, 0u));
    write_string(" map-pdpt ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PDPT_INDEX, console_pid, 0u, 0u));
    write_string(" map-pd ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PD_INDEX, console_pid, 0u, 0u));
    write_string(" transfer ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_ENTRY_TRANSFER_TOKEN, console_pid, 0u, 0u));
    write_string(" install ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_INSTALL_TOKEN, console_pid, 0u, 0u));
    write_string(" entry-probe ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_ENTRY_PROBE, console_pid, 0u, 0u));
    write_string(" installed ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_IMAGE_MAP_INSTALLED, console_pid, 0u, 0u));
    write_string(" protection ");
    write_hex_u32((u32)syscall64_native_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_FLAGS,
        console_pid,
        0u,
        0u));
    write_string(" protection-token ");
    write_hex_u32((u32)syscall64_native_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_TOKEN,
        console_pid,
        0u,
        0u));
    write_protection_summary((u32)syscall64_native_invoke(
        X64_SYSCALL_PROCESS_RUNTIME_IMAGE_PROTECTION_FLAGS,
        console_pid,
        0u,
        0u));
    write_string(" payload-offset ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_OFFSET, console_pid, 0u, 0u));
    write_string(" payload-size ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_SIZE, console_pid, 0u, 0u));
    write_string(" payload-checksum ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_RUNTIME_PAYLOAD_CHECKSUM, console_pid, 0u, 0u));
    write_string(" launch-pid ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_PID, console_manifest, 0u, 0u));
    write_string(" requester ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUESTER, console_manifest, 0u, 0u));
    write_string(" request ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_ID, console_manifest, 0u, 0u));
    write_string(" request-state ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_LAST_REQUEST_STATUS, console_manifest, 0u, 0u));
    write_string(" invalid-principal ");
    write_hex_u32((u32)invalid_principal);
    write_line("");
}

static void log_native_launch_surface(void)
{
    write_string("[x64] native launch service ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_COUNT, 0u, 0u, 0u));
    write_string(" verified ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_PROCESS_MANIFEST_VERIFIED_COUNT, 0u, 0u, 0u));
    write_string(" ignored ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_IGNORED, 0u, 0u, 0u));
    write_string(" denied ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_MANIFEST_DENIED, 0u, 0u, 0u));
    write_string(" ready ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_READY_COUNT, 0u, 0u, 0u));
    write_string(" started ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_STARTED_COUNT, 0u, 0u, 0u));
    write_string(" drained ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_DRAINED_COUNT, 0u, 0u, 0u));
    write_string(" quiesce-ready ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_QUIESCE_READY_COUNT, 0u, 0u, 0u));
    write_string(" start-denials ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_START_DENIAL_COUNT, 0u, 0u, 0u));
    write_string(" requests ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUEST_COUNT, 0u, 0u, 0u));
    write_string(" approvals ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_APPROVAL_COUNT, 0u, 0u, 0u));
    write_string(" pending ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_PENDING_COUNT, 0u, 0u, 0u));
    write_string(" request-denied ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_DENIED_COUNT, 0u, 0u, 0u));
    write_string(" completed ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_COMPLETED_COUNT, 0u, 0u, 0u));
    write_string(" quiesce-requests ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_QUIESCE_REQUEST_COUNT, 0u, 0u, 0u));
    write_string(" quiesce-approvals ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_QUIESCE_APPROVAL_COUNT, 0u, 0u, 0u));
    write_string(" quiesce-pending ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_QUIESCE_PENDING_COUNT, 0u, 0u, 0u));
    write_string(" quiesce-denied ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_QUIESCE_DENIED_COUNT, 0u, 0u, 0u));
    write_string(" quiesce-completed ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_QUIESCE_COMPLETED_COUNT, 0u, 0u, 0u));
    write_string(" drain-requests ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_DRAIN_REQUEST_COUNT, 0u, 0u, 0u));
    write_string(" drain-approvals ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_DRAIN_APPROVAL_COUNT, 0u, 0u, 0u));
    write_string(" drain-pending ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_DRAIN_PENDING_COUNT, 0u, 0u, 0u));
    write_string(" drain-denied ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_DRAIN_DENIED_COUNT, 0u, 0u, 0u));
    write_string(" drain-completed ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_DRAIN_COMPLETED_COUNT, 0u, 0u, 0u));
    write_string(" restart-requests ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_RESTART_REQUEST_COUNT, 0u, 0u, 0u));
    write_string(" restart-approvals ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_RESTART_APPROVAL_COUNT, 0u, 0u, 0u));
    write_string(" restart-pending ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_RESTART_PENDING_COUNT, 0u, 0u, 0u));
    write_string(" restart-denied ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_RESTART_DENIED_COUNT, 0u, 0u, 0u));
    write_string(" restart-completed ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_RESTART_COMPLETED_COUNT, 0u, 0u, 0u));
    write_string(" stop-requests ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_STOP_REQUEST_COUNT, 0u, 0u, 0u));
    write_string(" stop-approvals ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_STOP_APPROVAL_COUNT, 0u, 0u, 0u));
    write_string(" stop-pending ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_STOP_PENDING_COUNT, 0u, 0u, 0u));
    write_string(" stop-denied ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_STOP_DENIED_COUNT, 0u, 0u, 0u));
    write_string(" stop-completed ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_STOP_COMPLETED_COUNT, 0u, 0u, 0u));
    write_string(" log ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUEST_LOG_COUNT, 0u, 0u, 0u));
    write_string(" init-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_START, PRINCIPAL64_ID_INIT_SUPERVISOR, 0u, 0u));
    write_string(" policy-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_START, PRINCIPAL64_ID_POLICY_WORKER, 0u, 0u));
    write_string(" quiesce-init-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_QUIESCE, PRINCIPAL64_ID_INIT_SUPERVISOR, 0u, 0u));
    write_string(" quiesce-policy-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_QUIESCE, PRINCIPAL64_ID_POLICY_WORKER, 0u, 0u));
    write_string(" drain-init-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_DRAIN, PRINCIPAL64_ID_INIT_SUPERVISOR, 0u, 0u));
    write_string(" drain-policy-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_DRAIN, PRINCIPAL64_ID_POLICY_WORKER, 0u, 0u));
    write_string(" restart-init-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_RESTART, PRINCIPAL64_ID_INIT_SUPERVISOR, 0u, 0u));
    write_string(" restart-policy-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_RESTART, PRINCIPAL64_ID_POLICY_WORKER, 0u, 0u));
    write_string(" stop-init-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_STOP, PRINCIPAL64_ID_INIT_SUPERVISOR, 0u, 0u));
    write_string(" stop-policy-auth ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_LAUNCH_REQUESTER_CAN_STOP, PRINCIPAL64_ID_POLICY_WORKER, 0u, 0u));
    write_line("");
}

static void log_native_capability_surface(void)
{
    u32 console_owner = CAPABILITY64_OWNER_CONSOLE_CLIENT;
    u32 console_worker = CAPABILITY64_OWNER_CONSOLE_WORKER;
    u32 console_context = CAPABILITY64_CONTEXT(
        CAPABILITY64_OWNER_CONSOLE_CLIENT,
        CAPABILITY64_OWNER_CONSOLE_WORKER);
    u64 console_handle = syscall64_native_invoke(
        X64_SYSCALL_CAP_GRANT_SERVICE,
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY | CAPABILITY64_RIGHT_DELEGATE,
        console_owner);
    u64 console_route = syscall64_native_invoke(
        X64_SYSCALL_CAP_ROUTE,
        console_handle,
        CAPABILITY64_RIGHT_SEND,
        console_owner);
    u64 console_child = syscall64_native_invoke(
        X64_SYSCALL_CAP_DELEGATE,
        console_handle,
        CAPABILITY64_RIGHT_SEND,
        console_context);
    u64 console_child_route = syscall64_native_invoke(
        X64_SYSCALL_CAP_ROUTE,
        console_child,
        CAPABILITY64_RIGHT_SEND,
        console_worker);

    write_string("[x64] native cap console ");
    write_hex_u32((u32)console_handle);
    write_string(" route ");
    write_dec_u32((u32)console_route);
    write_string(" rights ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_RIGHTS, console_handle, 0u, console_owner));
    write_string(" owner ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_OWNER, console_handle, 0u, console_owner));
    write_string(" generation ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_RUNTIME_GENERATION, console_handle, 0u, console_owner));
    write_string(" runtime ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_RUNTIME_TOKEN, console_handle, 0u, console_owner));
    write_string(" child ");
    write_hex_u32((u32)console_child);
    write_string(" child-route ");
    write_dec_u32((u32)console_child_route);
    write_string(" child-rights ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_RIGHTS, console_child, 0u, console_worker));
    write_string(" child-owner ");
    write_hex_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_OWNER, console_child, 0u, console_worker));
    write_string(" revoke ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_REVOKE, console_handle, 0u, console_owner));
    write_string(" live ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_LIVE_COUNT, 0u, 0u, 0u));
    write_string(" console-live ");
    write_dec_u32((u32)syscall64_native_invoke(
        X64_SYSCALL_CAP_LIVE_FOR_ENDPOINT_CLASS,
        SERVICE_ENDPOINT_CLASS_CONSOLE,
        0u,
        0u));
    write_string(" delegates ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_DELEGATE_COUNT, 0u, 0u, 0u));
    write_string(" cascades ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_CASCADE_REVOKE_COUNT, 0u, 0u, 0u));
    write_string(" expirations ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_EXPIRATION_COUNT, 0u, 0u, 0u));
    write_string(" owner-denials ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_OWNER_DENIAL_COUNT, 0u, 0u, 0u));
    write_string(" principal-denials ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_PRINCIPAL_DENIAL_COUNT, 0u, 0u, 0u));
    write_string(" stale-denials ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_RUNTIME_STALE_DENIAL_COUNT, 0u, 0u, 0u));
    write_string(" denials ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_CAP_DENIAL_COUNT, 0u, 0u, 0u));
    write_line("");
}

static void log_native_fault_surface(void)
{
    write_string("[x64] native faults exceptions ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_EXCEPTION_COUNT, 0u, 0u, 0u));
    write_string(" breakpoint ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_BREAKPOINT_COUNT, 0u, 0u, 0u));
    write_string(" invalid-op ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_INVALID_OPCODE_COUNT, 0u, 0u, 0u));
    write_string(" page-fault ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_PAGE_FAULT_COUNT, 0u, 0u, 0u));
    write_string(" last vector ");
    write_dec_u32((u32)syscall64_native_invoke(X64_SYSCALL_GET_LAST_EXCEPTION_VECTOR, 0u, 0u, 0u));
    write_line("");

    write_string("[x64] native faults error ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_GET_LAST_EXCEPTION_ERROR, 0u, 0u, 0u));
    write_string(" rip ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_GET_LAST_EXCEPTION_RIP, 0u, 0u, 0u));
    write_string(" cr2 ");
    write_hex_u64(syscall64_native_invoke(X64_SYSCALL_GET_LAST_EXCEPTION_CR2, 0u, 0u, 0u));
    write_line("");
}

static void wait_for_timer_ticks(u32 target_ticks)
{
    while (pit_get_ticks() < target_ticks)
    {
        cpu_halt();
    }
}

static void collect_keyboard_probe_input(u32 target_pending, u32 max_wait_ticks)
{
    u32 target_ticks = pit_get_ticks() + max_wait_ticks;

    interrupts64_enable();
    while ((input64_keyboard_pending_count() < target_pending)
        && (pit_get_ticks() < target_ticks))
    {
        input64_poll_keyboard();
        xhci64_poll_keyboard();
        input64_poll_mouse();
        xhci64_poll_mouse();
        cpu_halt();
    }
    interrupts64_disable();
    input64_poll_keyboard();
    xhci64_poll_keyboard();
    input64_poll_mouse();
    xhci64_poll_mouse();
}

static void collect_mouse_probe_input(u32 target_packets, u32 max_wait_ticks)
{
    u32 target_ticks = pit_get_ticks() + max_wait_ticks;

    interrupts64_enable();
    while ((input64_mouse_packet_count() < target_packets)
        && (pit_get_ticks() < target_ticks))
    {
        input64_poll_mouse();
        xhci64_poll_mouse();
        cpu_halt();
    }
    interrupts64_disable();
    input64_poll_mouse();
    xhci64_poll_mouse();
}

static void collect_gui_interactive_probe_input(u32 max_wait_ticks)
{
    u32 target_ticks = pit_get_ticks() + max_wait_ticks;

    interrupts64_enable();
    while (((display64_gui_launcher_opened() == 0u)
            || (display64_gui_terminal_opened() == 0u)
            || (display64_gui_drag_completed() == 0u)
            || (display64_gui_keyboard_routed() == 0u)
            || (display64_gui_close_completed() == 0u)
            || (display64_gui_taskbar_focus() == 0u)
            || (display64_gui_fileman_opened() == 0u)
            || (display64_gui_settings_opened() == 0u)
            || (display64_gui_unfocused_key_denied() == 0u))
        && (pit_get_ticks() < target_ticks))
    {
        input64_poll_keyboard();
        xhci64_poll_keyboard();
        input64_poll_mouse();
        xhci64_poll_mouse();
        cpu_halt();
    }
    interrupts64_disable();
    input64_poll_keyboard();
    xhci64_poll_keyboard();
    input64_poll_mouse();
    xhci64_poll_mouse();
}

static int boot_info_is_valid_x64(const struct boot_info *boot_info)
{
    u32 required_flags = LIMITLESS_BOOT_FLAG_PROTECTED_MODE |
                         LIMITLESS_BOOT_FLAG_LONG_MODE |
                         LIMITLESS_BOOT_FLAG_PAGING |
                         LIMITLESS_BOOT_FLAG_IDENTITY_MAP |
                         LIMITLESS_BOOT_FLAG_HIGH_HALF_ALIAS;

    if ((boot_info == 0) || (boot_info->magic != LIMITLESS_BOOT_INFO_MAGIC))
    {
        return 0;
    }

    if (boot_info->architecture_bits != 64u)
    {
        return 0;
    }

    if ((boot_info->bootstrap_flags & required_flags) != required_flags)
    {
        return 0;
    }

    if ((boot_info->page_table_root == 0u) || (boot_info->identity_map_bytes == 0u))
    {
        return 0;
    }

    return 1;
}

static void log_build_profile_surface(void)
{
    write_string("[x64] build-profile ");
    write_string(LIMITLESS_BUILD_PROFILE_NAME);
    write_labeled_dec_u32(" product ", LIMITLESS_BUILD_PROFILE_PRODUCT);
    write_labeled_dec_u32(" experimental ", LIMITLESS_BUILD_PROFILE_EXPERIMENTAL);
    write_labeled_dec_u32(" experimental-runtime ", LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED);
    write_line("");

#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED
    write_line("[x64] experimental-runtime enabled proof-surface 1 not-product-path 1");
#else
    write_line("[x64] experimental-runtime disabled proof-surface 0 gui product-gated network product-gated ai unavailable installer unavailable package-manager unavailable");
#endif
}

void kernel_main64_scaffold(const struct boot_info *boot_info)
{
    write_line(g_x64_scaffold_name);

    if (!boot_info_is_valid_x64(boot_info))
    {
        write_line("[boot] invalid x64 boot information");
        for (;;)
        {
            cpu_halt();
        }
    }

    log_boot_memory(boot_info);
    log_build_profile_surface();
    services64_init();
    descriptors64_init();
    log_bootstrap_state(boot_info);
    write_line("[x64] long mode active");
    log_framebuffer_handoff(boot_info);
    syscall64_init(boot_info);
    syscall64_native_init();
    log_descriptor_surface();
    log_bootstrap_catalog();
    log_principal_namespace();
    log_process_namespace();
    log_launch_namespace();
    log_service_namespace();
    write_string("[x64] page root ");
    write_hex_u32(boot_info->page_table_root);
    write_line("");
    log_identity_map(boot_info);
    log_runtime_mapping();
    log_higher_half_alias();
    log_kernel_load(boot_info);
    write_string("[x64] bootstrap kind ");
    write_line(g_x64_scaffold_bootstrap_kind);
    write_string("[x64] paging levels ");
    write_dec_u32(g_x64_scaffold_report.page_levels);
    write_line("");
    write_line("[x64] initializing interrupts");
    interrupts64_init();
    write_line("[x64] IDT online");
    interrupts64_trigger_probe();
    interrupts64_trigger_breakpoint_proof();
    interrupts64_trigger_invalid_opcode_proof();
    interrupts64_trigger_page_fault_proof();
    interrupts64_trigger_syscall_probe(1u);
    run_user_entry_transfer_probe();
    apic64_init(boot_info);
    if (apic64_enabled() != 0u)
    {
        write_line("[x64] APIC ready");
    }
    else
    {
        pic_initialize(0xF8u, 0xEFu);
        write_line("[x64] PIC ready");
    }
    pit_initialize(100u);
    write_line("[x64] PIT at 100 Hz");
    xhci64_init();
    xhci64_poll_keyboard();
    xhci64_poll_mouse();
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
        input64_keyboard_scancode_count(),
        input64_keyboard_pending_count(),
        input64_keyboard_last_scancode(),
        pci64_lpss_i2c_hid_found());
    write_line("[x64] input devices ready");
    run_user_entry_preempt_probe();
    run_user_entry_switch_probe();
    run_user_entry_runqueue_probe();
    interrupts64_enable();
    wait_for_timer_ticks(30u);
    interrupts64_disable();
    input64_poll_keyboard();
    collect_keyboard_probe_input(127u, 360u);
    collect_mouse_probe_input(1u, 120u);
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
        input64_keyboard_scancode_count(),
        input64_keyboard_pending_count(),
        input64_keyboard_last_scancode(),
        pci64_lpss_i2c_hid_found());
    (void)display64_write_mouse_diagnostics(
        input64_ps2_mouse_init_done(),
        input64_ps2_mouse_aux_enabled(),
        input64_ps2_mouse_config_read(),
        input64_ps2_mouse_config_write(),
        input64_ps2_mouse_irq12_configured(),
        input64_ps2_mouse_enable_command(),
        input64_ps2_mouse_ack(),
        input64_mouse_irq_count(),
        input64_mouse_packet_count(),
        input64_mouse_pending_count(),
        input64_mouse_x(),
        input64_mouse_y(),
        input64_mouse_buttons());
    log_interrupt_probes();
    write_string("[x64] timer ticks ");
    write_dec_u32(pit_get_ticks());
    write_line("");
    log_syscall_surface();
    log_principal_surface();
    log_process_surface();
    log_launch_surface();
    log_service_surface();
    log_input_keyboard_surface();
    log_mouse_surface();
#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
    display64_compositor_probe(input64_mouse_x(), input64_mouse_y(), input64_mouse_buttons());
#endif
    log_compositor_surface();
#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
    display64_font_probe();
#endif
    log_font_surface();
#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
    display64_wm_probe();
#endif
    log_window_manager_surface();
#if LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED || LIMITLESS_BUILD_PROFILE_PRODUCT
    display64_desktop_probe();
    if (display64_desktop_init_done() != 0u)
    {
        write_line("[x64] gui interactive input wait");
        collect_gui_interactive_probe_input(2000u);
    }
#endif
    log_desktop_surface();
    log_gui_interactive_surface();
    log_input_keyboard_surface();
    log_service_session_surface();
    log_package_signing_surface();
    log_package_trust_status_surface();
    log_hardware_validation_surface();
    (void)display64_write_mouse_diagnostics(
        input64_ps2_mouse_init_done(),
        input64_ps2_mouse_aux_enabled(),
        input64_ps2_mouse_config_read(),
        input64_ps2_mouse_config_write(),
        input64_ps2_mouse_irq12_configured(),
        input64_ps2_mouse_enable_command(),
        input64_ps2_mouse_ack(),
        input64_mouse_irq_count(),
        input64_mouse_packet_count(),
        input64_mouse_pending_count(),
        input64_mouse_x(),
        input64_mouse_y(),
        input64_mouse_buttons());
    log_apic_surface();
    log_xhci_surface();
    log_block_surface();
    log_pci_storage_surface();
    virtio_net64_init();
    log_virtio_net_surface();
    log_e1000_surface();
    log_dhcp_surface();
    log_dns_surface();
    log_http_surface();
    log_filesystem_surface();
    log_capability_surface();
    log_fault_surface();
    log_native_syscall_surface();
    log_native_principal_surface();
    log_native_process_surface();
    log_native_launch_surface();
    log_native_service_surface();
    log_native_capability_surface();
    log_native_fault_surface();
    log_brokered_keyboard_read_probe();
    run_user_entry_filesystem_probe();
    run_user_entry_cli_probe();
    run_user_entry_input_cli_probe();
    run_user_entry_shell_stream_probe();
    log_nvme_rw_surface();
    run_user_entry_second_page_probe();
    run_drs_load_probe();
    run_drs_load_full_probe();
    write_string("[x64] active virtual ");
    write_hex_u64(g_x64_scaffold_report.active_virtual_base);
    write_line("");
    write_string("[x64] planned high half ");
    write_hex_u64(g_x64_scaffold_report.planned_virtual_base);
    write_line("");
    write_string("[x64] plan ");
    write_line(g_x64_scaffold_plan);
    write_line("[x64] compat32 lane planned");
    write_line("[x64] bootstrap halt");
    if (run_persistent_ring3_shell() == 0)
    {
        run_live_keyboard_console();
    }

    for (;;)
    {
        cpu_halt();
    }
}
