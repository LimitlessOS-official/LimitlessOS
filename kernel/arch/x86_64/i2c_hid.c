#include "i2c_hid_x64.h"

#include "input_x64.h"
#include "paging_x64.h"
#include "pci_x64.h"
#include "pit.h"
#include "serial.h"

#if defined(LIMITLESS_X64_BIOS_KERNEL) && LIMITLESS_X64_BIOS_KERNEL

void i2c_hid64_init(void)
{
}

void i2c_hid64_poll_keyboard(void)
{
}

void i2c_hid64_poll_pointer(void)
{
}

u32 i2c_hid64_controller_present(void)
{
    return 0u;
}

u32 i2c_hid64_device_found(void)
{
    return 0u;
}

u32 i2c_hid64_report_count(void)
{
    return 0u;
}

u32 i2c_hid64_error(void)
{
    return 0u;
}

u32 i2c_hid64_pointer_found(void)
{
    return 0u;
}

u32 i2c_hid64_pointer_report_count(void)
{
    return 0u;
}

u32 i2c_hid64_pointer_error(void)
{
    return 0u;
}

#else

#define I2C_HID64_MAP_VIRTUAL_BASE 0xFFFFFFFF901E0000ull
#define I2C_HID64_MAP_PAGES 2u
#define I2C_HID64_PAGE_BYTES 4096u
#define I2C_HID64_SECOND_MAP_VIRTUAL_BASE \
    (I2C_HID64_MAP_VIRTUAL_BASE + (I2C_HID64_PAGE_BYTES * I2C_HID64_MAP_PAGES))
#define I2C_HID64_POLL_LIMIT 250000u
#define I2C_HID64_TIMEOUT_TICKS 1u
#define I2C_HID64_TRANSFER_TIMEOUT_BYTES_PER_TICK 64u

#define I2C_DW_IC_CON 0x00u
#define I2C_DW_IC_TAR 0x04u
#define I2C_DW_IC_DATA_CMD 0x10u
#define I2C_DW_IC_FS_SCL_HCNT 0x1Cu
#define I2C_DW_IC_FS_SCL_LCNT 0x20u
#define I2C_DW_IC_INTR_MASK 0x30u
#define I2C_DW_IC_RAW_INTR_STAT 0x34u
#define I2C_DW_IC_CLR_INTR 0x40u
#define I2C_DW_IC_CLR_TX_ABRT 0x54u
#define I2C_DW_IC_RX_TL 0x38u
#define I2C_DW_IC_TX_TL 0x3Cu
#define I2C_DW_IC_ENABLE 0x6Cu
#define I2C_DW_IC_STATUS 0x70u
#define I2C_DW_IC_SDA_HOLD 0x7Cu
#define I2C_DW_IC_TX_ABRT_SOURCE 0x80u
#define I2C_DW_IC_SDA_SETUP 0x94u
#define I2C_DW_IC_ENABLE_STATUS 0x9Cu
#define I2C_DW_IC_COMP_TYPE 0xFCu

#define I2C_DW_CON_MASTER 0x00000001u
#define I2C_DW_CON_SPEED_FAST 0x00000004u
#define I2C_DW_CON_RESTART_EN 0x00000020u
#define I2C_DW_CON_SLAVE_DISABLE 0x00000040u
#define I2C_DW_DATA_CMD_READ 0x00000100u
#define I2C_DW_DATA_CMD_STOP 0x00000200u
#define I2C_DW_DATA_CMD_RESTART 0x00000400u
#define I2C_DW_STATUS_ACTIVITY 0x00000001u
#define I2C_DW_STATUS_TFNF 0x00000002u
#define I2C_DW_STATUS_TFE 0x00000004u
#define I2C_DW_STATUS_RFNE 0x00000008u
#define I2C_DW_ENABLE_ACTIVE 0x00000001u
#define I2C_DW_INTR_TX_ABRT 0x00000040u
#define I2C_DW_COMP_TYPE_VALUE 0x44570140u

#define I2C_HID64_PRIMARY_ADDRESS_COUNT 8u
#define I2C_HID64_POINTER_ADDRESS_COUNT 16u
#define I2C_HID64_DESCRIPTOR_REGISTER_COUNT 2u
#define I2C_HID64_DESCRIPTOR_BYTES 30u
#define I2C_HID64_DESCRIPTOR_MIN_LENGTH 30u
#define I2C_HID64_REPORT_BYTES 128u
#define I2C_HID64_REPORT_DESCRIPTOR_BYTES 512u
#define I2C_HID64_TRANSFER_BYTES 512u
#define I2C_HID64_RESET_WAIT_TICKS 3u
#define I2C_HID64_RESET_WAIT_POLL_LIMIT 750000u
#define I2C_HID64_COMMAND_RESET 0x01u
#define I2C_HID64_COMMAND_SET_POWER 0x08u
#define I2C_HID64_POINTER_KIND_NONE 0u
#define I2C_HID64_POINTER_KIND_MOUSE 1u
#define I2C_HID64_POINTER_KIND_TOUCHPAD 2u

static const u8 g_i2c_hid64_primary_addresses[I2C_HID64_PRIMARY_ADDRESS_COUNT] = {
    0x15u, 0x2Cu, 0x10u, 0x11u, 0x2Du, 0x38u, 0x3Au, 0x40u
};
static const u8 g_i2c_hid64_pointer_addresses[I2C_HID64_POINTER_ADDRESS_COUNT] = {
    0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x18u, 0x1Au,
    0x2Cu, 0x2Du, 0x35u, 0x38u, 0x3Au, 0x40u, 0x41u, 0x5Du
};
static const u16 g_i2c_hid64_descriptor_registers[I2C_HID64_DESCRIPTOR_REGISTER_COUNT] = { 0x0001u, 0x0000u };

static u32 g_i2c_hid64_controller_present = 0u;
static u32 g_i2c_hid64_mapped = 0u;
static u32 g_i2c_hid64_initialized = 0u;
static u32 g_i2c_hid64_device_found = 0u;
static u32 g_i2c_hid64_disabled = 0u;
static u32 g_i2c_hid64_error = 0u;
static u32 g_i2c_hid64_address = 0u;
static u32 g_i2c_hid64_descriptor_register = 0u;
static u32 g_i2c_hid64_report_descriptor_register = 0u;
static u32 g_i2c_hid64_report_descriptor_length = 0u;
static u32 g_i2c_hid64_input_register = 0u;
static u32 g_i2c_hid64_command_register = 0u;
static u32 g_i2c_hid64_data_register = 0u;
static u32 g_i2c_hid64_max_input_length = 0u;
static u32 g_i2c_hid64_report_count = 0u;
static u32 g_i2c_hid64_pointer_found = 0u;
static u32 g_i2c_hid64_pointer_kind = I2C_HID64_POINTER_KIND_NONE;
static u32 g_i2c_hid64_pointer_address = 0u;
static u32 g_i2c_hid64_pointer_descriptor_register = 0u;
static u32 g_i2c_hid64_pointer_report_descriptor_register = 0u;
static u32 g_i2c_hid64_pointer_report_descriptor_length = 0u;
static u32 g_i2c_hid64_pointer_input_register = 0u;
static u32 g_i2c_hid64_pointer_command_register = 0u;
static u32 g_i2c_hid64_pointer_max_input_length = 0u;
static u32 g_i2c_hid64_pointer_report_has_id = 0u;
static u32 g_i2c_hid64_pointer_report_count = 0u;
static u32 g_i2c_hid64_pointer_error = 0u;
static u64 g_i2c_hid64_mapped_physical_base = 0ull;
static u64 g_i2c_hid64_keyboard_physical_base = 0ull;
static u64 g_i2c_hid64_pointer_physical_base = 0ull;
static u64 g_i2c_hid64_active_virtual_base = I2C_HID64_MAP_VIRTUAL_BASE;
static u64 g_i2c_hid64_keyboard_virtual_base = 0ull;
static u64 g_i2c_hid64_pointer_virtual_base = 0ull;
static u8 g_i2c_hid64_descriptor[I2C_HID64_DESCRIPTOR_BYTES];
static u8 g_i2c_hid64_report[I2C_HID64_REPORT_BYTES];
static u8 g_i2c_hid64_report_descriptor[I2C_HID64_REPORT_DESCRIPTOR_BYTES];
static u8 g_i2c_hid64_pointer_report[I2C_HID64_REPORT_BYTES];

static volatile u32 *i2c_hid64_reg(u32 offset)
{
    return (volatile u32 *)(u64)(g_i2c_hid64_active_virtual_base + (u64)offset);
}

static u32 i2c_hid64_program_controller(void);

static u32 i2c_hid64_mmio_flags_valid(u32 flags)
{
    return ((flags & (PCI64_LPSS_I2C_MMIO_FLAG_MEMORY_BAR
            | PCI64_LPSS_I2C_MMIO_FLAG_BASE_NONZERO))
        == (PCI64_LPSS_I2C_MMIO_FLAG_MEMORY_BAR
            | PCI64_LPSS_I2C_MMIO_FLAG_BASE_NONZERO)) ? 1u : 0u;
}

static u32 i2c_hid64_map_physical(u64 virtual_base, u64 physical_base)
{
    u64 physical_page_base = physical_base & ~0xFFFull;
    u64 page_offset = physical_base & 0xFFFull;
    u64 active_virtual_base = virtual_base + page_offset;

    if (physical_base == 0ull)
    {
        g_i2c_hid64_error = (g_i2c_hid64_error == 0u) ? 1u : g_i2c_hid64_error;
        return 0u;
    }

    if ((g_i2c_hid64_mapped != 0u)
        && (g_i2c_hid64_mapped_physical_base == physical_page_base)
        && (g_i2c_hid64_active_virtual_base == active_virtual_base))
    {
        return 1u;
    }

    if (paging64_install_kernel_mmio_mapping(
            virtual_base,
            physical_page_base,
            I2C_HID64_MAP_PAGES) == 0u)
    {
        g_i2c_hid64_error = (g_i2c_hid64_error == 0u) ? 2u : g_i2c_hid64_error;
        return 0u;
    }

    g_i2c_hid64_mapped = 1u;
    g_i2c_hid64_mapped_physical_base = physical_page_base;
    g_i2c_hid64_active_virtual_base = active_virtual_base;
    return 1u;
}

static u32 i2c_hid64_select_virtual(u64 virtual_base)
{
    if (virtual_base == 0ull)
    {
        return 0u;
    }

    g_i2c_hid64_active_virtual_base = virtual_base;
    return 1u;
}

static u32 i2c_hid64_prepare_controller(u64 virtual_base, u64 physical_base)
{
    g_i2c_hid64_disabled = 0u;
    if (i2c_hid64_map_physical(virtual_base, physical_base) == 0u)
    {
        return 0u;
    }

    if (i2c_hid64_program_controller() == 0u)
    {
        g_i2c_hid64_disabled = 0u;
        return 0u;
    }

    g_i2c_hid64_error = 0u;
    g_i2c_hid64_initialized = 1u;
    return 1u;
}

static u32 i2c_hid64_read32(u32 offset)
{
    if ((offset + 4u) > I2C_HID64_PAGE_BYTES)
    {
        g_i2c_hid64_error = (g_i2c_hid64_error == 0u) ? 100u : g_i2c_hid64_error;
        return 0u;
    }

    return *i2c_hid64_reg(offset);
}

static u32 i2c_hid64_write32(u32 offset, u32 value)
{
    u64 virtual_address = g_i2c_hid64_active_virtual_base + (u64)offset;

    if ((offset + 4u) > I2C_HID64_PAGE_BYTES)
    {
        g_i2c_hid64_error = (g_i2c_hid64_error == 0u) ? 101u : g_i2c_hid64_error;
        return 0u;
    }

    if (paging64_kernel_mmio_write_window_open_virtual(virtual_address) == 0u)
    {
        g_i2c_hid64_error = (g_i2c_hid64_error == 0u) ? 102u : g_i2c_hid64_error;
        return 0u;
    }

    *i2c_hid64_reg(offset) = value;
    (void)*i2c_hid64_reg(offset);
    (void)paging64_kernel_mmio_write_window_close_virtual(virtual_address);
    return 1u;
}

static u32 i2c_hid64_deadline_expired_after(u32 start_ticks, u32 poll, u32 timeout_ticks)
{
    if ((pit_get_ticks() - start_ticks) >= timeout_ticks)
    {
        return 1u;
    }

    return (poll >= I2C_HID64_POLL_LIMIT) ? 1u : 0u;
}

static u32 i2c_hid64_deadline_expired(u32 start_ticks, u32 poll)
{
    return i2c_hid64_deadline_expired_after(start_ticks, poll, I2C_HID64_TIMEOUT_TICKS);
}

static u32 i2c_hid64_transfer_timeout_ticks(u32 byte_count)
{
    return I2C_HID64_TIMEOUT_TICKS
        + ((byte_count + (I2C_HID64_TRANSFER_TIMEOUT_BYTES_PER_TICK - 1u))
            / I2C_HID64_TRANSFER_TIMEOUT_BYTES_PER_TICK);
}

static void i2c_hid64_fail(u32 code)
{
    if (g_i2c_hid64_error == 0u)
    {
        g_i2c_hid64_error = code;
    }
    g_i2c_hid64_disabled = 1u;
    g_i2c_hid64_initialized = 0u;
    g_i2c_hid64_device_found = 0u;
    serial_write_string("[x64] I2C HID disabled after bounded timeout/failure\n");
}

static void i2c_hid64_serial_write_dec(u32 value)
{
    char digits[10];
    u32 count = 0u;

    if (value == 0u)
    {
        serial_write_char('0');
        return;
    }

    while ((value != 0u) && (count < (u32)sizeof(digits)))
    {
        digits[count] = (char)('0' + (value % 10u));
        value /= 10u;
        ++count;
    }

    while (count != 0u)
    {
        --count;
        serial_write_char(digits[count]);
    }
}

static void i2c_hid64_log_status(const char *prefix)
{
    serial_write_string("[x64] I2C HID ");
    serial_write_string(prefix);
    serial_write_string(" addr ");
    i2c_hid64_serial_write_dec(g_i2c_hid64_address);
    serial_write_string(" desc-reg ");
    i2c_hid64_serial_write_dec(g_i2c_hid64_descriptor_register);
    serial_write_string(" in-reg ");
    i2c_hid64_serial_write_dec(g_i2c_hid64_input_register);
    serial_write_string(" cmd-reg ");
    i2c_hid64_serial_write_dec(g_i2c_hid64_command_register);
    serial_write_string(" max-in ");
    i2c_hid64_serial_write_dec(g_i2c_hid64_max_input_length);
    serial_write_string(" err ");
    i2c_hid64_serial_write_dec(g_i2c_hid64_error);
    serial_write_string("\n");
}

static u32 i2c_hid64_tx_aborted(void)
{
    if ((i2c_hid64_read32(I2C_DW_IC_RAW_INTR_STAT) & I2C_DW_INTR_TX_ABRT) == 0u)
    {
        return 0u;
    }

    (void)i2c_hid64_read32(I2C_DW_IC_CLR_TX_ABRT);
    if (g_i2c_hid64_error == 0u)
    {
        g_i2c_hid64_error = 4u;
    }
    return 1u;
}

static u32 i2c_hid64_wait_status(u32 mask, u32 want_set, u32 start_ticks, u32 code)
{
    u32 poll;

    for (poll = 0u; i2c_hid64_deadline_expired(start_ticks, poll) == 0u; ++poll)
    {
        u32 status = i2c_hid64_read32(I2C_DW_IC_STATUS);
        if (i2c_hid64_tx_aborted() != 0u)
        {
            return 0u;
        }
        if (want_set != 0u)
        {
            if ((status & mask) == mask)
            {
                return 1u;
            }
        }
        else if ((status & mask) == 0u)
        {
            return 1u;
        }
        __asm__ __volatile__("pause");
    }

    i2c_hid64_fail(code);
    return 0u;
}

static u32 i2c_hid64_wait_enable(u32 enabled, u32 start_ticks, u32 code)
{
    u32 poll;

    for (poll = 0u; i2c_hid64_deadline_expired(start_ticks, poll) == 0u; ++poll)
    {
        u32 status = i2c_hid64_read32(I2C_DW_IC_ENABLE_STATUS) & I2C_DW_ENABLE_ACTIVE;
        if (status == (enabled & I2C_DW_ENABLE_ACTIVE))
        {
            return 1u;
        }
        __asm__ __volatile__("pause");
    }

    i2c_hid64_fail(code);
    return 0u;
}

static u32 i2c_hid64_disable_controller(u32 start_ticks)
{
    if (i2c_hid64_write32(I2C_DW_IC_ENABLE, 0u) == 0u)
    {
        i2c_hid64_fail(110u);
        return 0u;
    }

    return i2c_hid64_wait_enable(0u, start_ticks, 111u);
}

static u32 i2c_hid64_enable_controller(u32 start_ticks)
{
    if (i2c_hid64_write32(I2C_DW_IC_ENABLE, 1u) == 0u)
    {
        i2c_hid64_fail(112u);
        return 0u;
    }

    return i2c_hid64_wait_enable(1u, start_ticks, 113u);
}

static u32 i2c_hid64_program_controller(void)
{
    u32 start_ticks = pit_get_ticks();
    u32 comp_type = i2c_hid64_read32(I2C_DW_IC_COMP_TYPE);

    if ((comp_type != I2C_DW_COMP_TYPE_VALUE) && (comp_type == 0xFFFFFFFFu || comp_type == 0u))
    {
        i2c_hid64_fail(120u);
        return 0u;
    }

    if (i2c_hid64_disable_controller(start_ticks) == 0u)
    {
        return 0u;
    }

    if (i2c_hid64_write32(
            I2C_DW_IC_CON,
            I2C_DW_CON_MASTER
                | I2C_DW_CON_SPEED_FAST
                | I2C_DW_CON_RESTART_EN
                | I2C_DW_CON_SLAVE_DISABLE) == 0u)
    {
        i2c_hid64_fail(121u);
        return 0u;
    }

    (void)i2c_hid64_write32(I2C_DW_IC_INTR_MASK, 0u);
    (void)i2c_hid64_read32(I2C_DW_IC_CLR_INTR);
    (void)i2c_hid64_write32(I2C_DW_IC_RX_TL, 0u);
    (void)i2c_hid64_write32(I2C_DW_IC_TX_TL, 0u);
    (void)i2c_hid64_write32(I2C_DW_IC_FS_SCL_HCNT, 0x0040u);
    (void)i2c_hid64_write32(I2C_DW_IC_FS_SCL_LCNT, 0x0080u);
    (void)i2c_hid64_write32(I2C_DW_IC_SDA_HOLD, 0x00300030u);
    (void)i2c_hid64_write32(I2C_DW_IC_SDA_SETUP, 0x00000064u);

    return i2c_hid64_enable_controller(start_ticks);
}

static u32 i2c_hid64_set_target(u32 address, u32 start_ticks)
{
    if (i2c_hid64_disable_controller(start_ticks) == 0u)
    {
        return 0u;
    }
    if (i2c_hid64_write32(I2C_DW_IC_TAR, address & 0x7Fu) == 0u)
    {
        i2c_hid64_fail(130u);
        return 0u;
    }
    return i2c_hid64_enable_controller(start_ticks);
}

static u32 i2c_hid64_send_data_cmd(u32 value, u32 start_ticks, u32 code)
{
    if (i2c_hid64_wait_status(I2C_DW_STATUS_TFNF, 1u, start_ticks, code) == 0u)
    {
        return 0u;
    }
    if (i2c_hid64_write32(I2C_DW_IC_DATA_CMD, value) == 0u)
    {
        i2c_hid64_fail(code + 1u);
        return 0u;
    }
    return 1u;
}

static u32 i2c_hid64_read_register(u32 address, u16 reg, u8 *buffer, u32 byte_count)
{
    u32 start_ticks;
    u32 issued;
    u32 received;
    u32 poll;
    u32 timeout_ticks;

    if ((buffer == 0) || (byte_count == 0u) || (byte_count > I2C_HID64_TRANSFER_BYTES))
    {
        return 0u;
    }

    start_ticks = pit_get_ticks();
    timeout_ticks = i2c_hid64_transfer_timeout_ticks(byte_count);
    if (i2c_hid64_set_target(address, start_ticks) == 0u)
    {
        return 0u;
    }

    if (i2c_hid64_send_data_cmd((u32)(reg & 0xFFu), start_ticks, 140u) == 0u
        || i2c_hid64_send_data_cmd((u32)(reg >> 8), start_ticks, 142u) == 0u)
    {
        return 0u;
    }

    issued = 0u;
    received = 0u;
    poll = 0u;
    while (received < byte_count)
    {
        u32 status;
        u32 progress = 0u;

        if (i2c_hid64_deadline_expired_after(start_ticks, poll, timeout_ticks) != 0u)
        {
            i2c_hid64_fail(146u);
            return 0u;
        }

        status = i2c_hid64_read32(I2C_DW_IC_STATUS);
        if (i2c_hid64_tx_aborted() != 0u)
        {
            return 0u;
        }

        if ((issued < byte_count) && ((status & I2C_DW_STATUS_TFNF) != 0u))
        {
            u32 command = I2C_DW_DATA_CMD_READ;
            if (issued == 0u)
            {
                command |= I2C_DW_DATA_CMD_RESTART;
            }
            if ((issued + 1u) == byte_count)
            {
                command |= I2C_DW_DATA_CMD_STOP;
            }
            if (i2c_hid64_write32(I2C_DW_IC_DATA_CMD, command) == 0u)
            {
                i2c_hid64_fail(145u);
                return 0u;
            }
            ++issued;
            progress = 1u;
        }

        if ((status & I2C_DW_STATUS_RFNE) != 0u)
        {
            buffer[received] = (u8)(i2c_hid64_read32(I2C_DW_IC_DATA_CMD) & 0xFFu);
            ++received;
            progress = 1u;
        }

        if (progress != 0u)
        {
            poll = 0u;
        }
        else
        {
            ++poll;
            __asm__ __volatile__("pause");
        }
    }

    start_ticks = pit_get_ticks();
    if (i2c_hid64_wait_status(I2C_DW_STATUS_TFE, 1u, start_ticks, 148u) == 0u
        || i2c_hid64_wait_status(I2C_DW_STATUS_ACTIVITY, 0u, start_ticks, 149u) == 0u)
    {
        return 0u;
    }

    return 1u;
}

static u32 i2c_hid64_write_register(u32 address, u16 reg, const u8 *buffer, u32 byte_count)
{
    u32 start_ticks;
    u32 index;

    if ((byte_count > I2C_HID64_TRANSFER_BYTES) || ((byte_count != 0u) && (buffer == 0)))
    {
        return 0u;
    }

    start_ticks = pit_get_ticks();
    if (i2c_hid64_set_target(address, start_ticks) == 0u)
    {
        return 0u;
    }

    if (i2c_hid64_send_data_cmd((u32)(reg & 0xFFu), start_ticks, 150u) == 0u)
    {
        return 0u;
    }
    if (byte_count == 0u)
    {
        if (i2c_hid64_send_data_cmd((u32)(reg >> 8) | I2C_DW_DATA_CMD_STOP, start_ticks, 152u) == 0u)
        {
            return 0u;
        }
    }
    else if (i2c_hid64_send_data_cmd((u32)(reg >> 8), start_ticks, 152u) == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        u32 command = (u32)buffer[index];
        if ((index + 1u) == byte_count)
        {
            command |= I2C_DW_DATA_CMD_STOP;
        }
        if (i2c_hid64_send_data_cmd(command, start_ticks, 154u) == 0u)
        {
            return 0u;
        }
    }

    if (i2c_hid64_wait_status(I2C_DW_STATUS_TFE, 1u, start_ticks, 156u) == 0u
        || i2c_hid64_wait_status(I2C_DW_STATUS_ACTIVITY, 0u, start_ticks, 157u) == 0u)
    {
        return 0u;
    }

    return 1u;
}

static u32 i2c_hid64_send_command_to(u32 address, u32 command_register, u32 command, u32 argument)
{
    u8 payload[2];

    if (command_register == 0u)
    {
        return 0u;
    }

    payload[0] = (u8)(command & 0xFFu);
    payload[1] = (u8)(argument & 0xFFu);
    return i2c_hid64_write_register(
        address,
        (u16)command_register,
        payload,
        (u32)sizeof(payload));
}

static void i2c_hid64_wait_reset_settle(void)
{
    u32 start_ticks = pit_get_ticks();
    u32 poll;

    for (poll = 0u; poll < I2C_HID64_RESET_WAIT_POLL_LIMIT; ++poll)
    {
        if ((pit_get_ticks() - start_ticks) >= I2C_HID64_RESET_WAIT_TICKS)
        {
            return;
        }
        __asm__ __volatile__("pause");
    }
}

static u32 i2c_hid64_u16(const u8 *bytes)
{
    return (u32)bytes[0] | ((u32)bytes[1] << 8);
}

static u32 i2c_hid64_hid_version_valid(u32 hid_version)
{
    return ((hid_version == 0x0100u)
        || (hid_version == 0x0101u)) ? 1u : 0u;
}

static void i2c_hid64_zero(u8 *buffer, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        buffer[index] = 0u;
    }
}

static u32 i2c_hid64_report_has_usage(const u8 *report, u32 report_length, u32 usage_page_match, u32 usage_match)
{
    u32 index;
    u32 usage_page = 0u;

    if (report == 0)
    {
        return 0u;
    }

    if (report_length > I2C_HID64_REPORT_DESCRIPTOR_BYTES)
    {
        report_length = I2C_HID64_REPORT_DESCRIPTOR_BYTES;
    }

    for (index = 0u; (index + 1u) < report_length; ++index)
    {
        if (report[index] == 0x05u)
        {
            usage_page = report[index + 1u];
            ++index;
            continue;
        }

        if ((usage_page == usage_page_match)
            && (report[index] == 0x09u)
            && (report[index + 1u] == usage_match))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 i2c_hid64_report_has_report_id(const u8 *report, u32 report_length)
{
    u32 index;

    if (report == 0)
    {
        return 0u;
    }

    if (report_length > I2C_HID64_REPORT_DESCRIPTOR_BYTES)
    {
        report_length = I2C_HID64_REPORT_DESCRIPTOR_BYTES;
    }

    for (index = 0u; (index + 1u) < report_length; ++index)
    {
        if (report[index] == 0x85u)
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 i2c_hid64_read_report_descriptor(u32 address, u32 report_register, u32 report_length)
{
    if ((report_register == 0u) || (report_length == 0u))
    {
        return 0u;
    }

    if (report_length > I2C_HID64_REPORT_DESCRIPTOR_BYTES)
    {
        report_length = I2C_HID64_REPORT_DESCRIPTOR_BYTES;
    }

    i2c_hid64_zero(g_i2c_hid64_report_descriptor, sizeof(g_i2c_hid64_report_descriptor));
    return i2c_hid64_read_register(
        address,
        (u16)report_register,
        g_i2c_hid64_report_descriptor,
        report_length);
}

static u32 i2c_hid64_probe_descriptor(u32 address, u16 descriptor_register)
{
    u32 hid_descriptor_length;
    u32 hid_version;
    u32 report_descriptor_length;
    u32 report_descriptor_register;
    u32 input_register;
    u32 max_input_length;
    u32 command_register;
    u32 data_register;

    i2c_hid64_zero(g_i2c_hid64_descriptor, sizeof(g_i2c_hid64_descriptor));
    if (i2c_hid64_read_register(
            address,
            descriptor_register,
            g_i2c_hid64_descriptor,
            I2C_HID64_DESCRIPTOR_BYTES) == 0u)
    {
        return 0u;
    }

    hid_descriptor_length = i2c_hid64_u16(&g_i2c_hid64_descriptor[0]);
    hid_version = i2c_hid64_u16(&g_i2c_hid64_descriptor[2]);
    report_descriptor_length = i2c_hid64_u16(&g_i2c_hid64_descriptor[4]);
    report_descriptor_register = i2c_hid64_u16(&g_i2c_hid64_descriptor[6]);
    input_register = i2c_hid64_u16(&g_i2c_hid64_descriptor[8]);
    max_input_length = i2c_hid64_u16(&g_i2c_hid64_descriptor[10]);
    command_register = i2c_hid64_u16(&g_i2c_hid64_descriptor[16]);
    data_register = i2c_hid64_u16(&g_i2c_hid64_descriptor[18]);
    if ((hid_descriptor_length < I2C_HID64_DESCRIPTOR_MIN_LENGTH)
        || (i2c_hid64_hid_version_valid(hid_version) == 0u)
        || (input_register == 0u)
        || (command_register == 0u)
        || (max_input_length < 8u))
    {
        return 0u;
    }

    g_i2c_hid64_address = address;
    g_i2c_hid64_descriptor_register = descriptor_register;
    g_i2c_hid64_report_descriptor_register = report_descriptor_register;
    g_i2c_hid64_report_descriptor_length = report_descriptor_length;
    g_i2c_hid64_input_register = input_register;
    g_i2c_hid64_command_register = command_register;
    g_i2c_hid64_data_register = data_register;
    g_i2c_hid64_max_input_length = max_input_length;
    g_i2c_hid64_device_found = 1u;
    i2c_hid64_log_status("descriptor found");
    return 1u;
}

void i2c_hid64_init(void)
{
    u64 physical_base;
    u64 pointer_physical_base = 0ull;
    u32 address_index;
    u32 register_index;
    u32 pointer_candidate_index;
    u32 pointer_candidate_count;
    u32 flags;
    u32 pointer_flags;
    u32 keyboard_ready = 0u;
    u32 keyboard_address = 0u;
    u32 keyboard_descriptor_register = 0u;
    u32 keyboard_report_descriptor_register = 0u;
    u32 keyboard_report_descriptor_length = 0u;
    u32 keyboard_input_register = 0u;
    u32 keyboard_command_register = 0u;
    u32 keyboard_data_register = 0u;
    u32 keyboard_max_input_length = 0u;
    u64 keyboard_physical_base = 0ull;

    g_i2c_hid64_controller_present = pci64_lpss_i2c_hid_found();
    g_i2c_hid64_mapped = 0u;
    g_i2c_hid64_initialized = 0u;
    g_i2c_hid64_device_found = 0u;
    g_i2c_hid64_disabled = 0u;
    g_i2c_hid64_error = 0u;
    g_i2c_hid64_report_count = 0u;
    g_i2c_hid64_address = 0u;
    g_i2c_hid64_descriptor_register = 0u;
    g_i2c_hid64_report_descriptor_register = 0u;
    g_i2c_hid64_report_descriptor_length = 0u;
    g_i2c_hid64_input_register = 0u;
    g_i2c_hid64_command_register = 0u;
    g_i2c_hid64_data_register = 0u;
    g_i2c_hid64_max_input_length = 0u;
    g_i2c_hid64_pointer_found = 0u;
    g_i2c_hid64_pointer_kind = I2C_HID64_POINTER_KIND_NONE;
    g_i2c_hid64_pointer_address = 0u;
    g_i2c_hid64_pointer_descriptor_register = 0u;
    g_i2c_hid64_pointer_report_descriptor_register = 0u;
    g_i2c_hid64_pointer_report_descriptor_length = 0u;
    g_i2c_hid64_pointer_input_register = 0u;
    g_i2c_hid64_pointer_command_register = 0u;
    g_i2c_hid64_pointer_max_input_length = 0u;
    g_i2c_hid64_pointer_report_has_id = 0u;
    g_i2c_hid64_pointer_report_count = 0u;
    g_i2c_hid64_pointer_error = 0u;
    g_i2c_hid64_mapped_physical_base = 0ull;
    g_i2c_hid64_keyboard_physical_base = 0ull;
    g_i2c_hid64_pointer_physical_base = 0ull;
    g_i2c_hid64_active_virtual_base = I2C_HID64_MAP_VIRTUAL_BASE;
    g_i2c_hid64_keyboard_virtual_base = 0ull;
    g_i2c_hid64_pointer_virtual_base = 0ull;

    if (g_i2c_hid64_controller_present == 0u)
    {
        return;
    }

    flags = pci64_lpss_i2c_mmio_flags();
    pointer_candidate_count = pci64_lpss_i2c_pointer_candidate_count();
    physical_base = ((u64)pci64_lpss_i2c_base_high() << 32) | (u64)pci64_lpss_i2c_base_low();

    if (i2c_hid64_mmio_flags_valid(flags) == 0u)
    {
        g_i2c_hid64_error = 1u;
    }
    else if (i2c_hid64_prepare_controller(I2C_HID64_MAP_VIRTUAL_BASE, physical_base) == 0u)
    {
        g_i2c_hid64_error = (g_i2c_hid64_error == 0u) ? 2u : g_i2c_hid64_error;
    }
    else
    {
        for (address_index = 0u; address_index < I2C_HID64_PRIMARY_ADDRESS_COUNT; ++address_index)
        {
            for (register_index = 0u; register_index < I2C_HID64_DESCRIPTOR_REGISTER_COUNT; ++register_index)
            {
                if (i2c_hid64_probe_descriptor(
                        g_i2c_hid64_primary_addresses[address_index],
                        g_i2c_hid64_descriptor_registers[register_index]) != 0u)
                {
                    u32 found_address = g_i2c_hid64_address;
                    u32 found_descriptor_register = g_i2c_hid64_descriptor_register;
                    u32 found_report_descriptor_register = g_i2c_hid64_report_descriptor_register;
                    u32 found_report_descriptor_length = g_i2c_hid64_report_descriptor_length;
                    u32 found_command_register = g_i2c_hid64_command_register;
                    u32 is_mouse = 0u;
                    u32 is_touchpad = 0u;
                    u32 has_report_id = 0u;

                    if (i2c_hid64_read_report_descriptor(
                            found_address,
                            found_report_descriptor_register,
                            found_report_descriptor_length) != 0u)
                    {
                        is_mouse = i2c_hid64_report_has_usage(
                            g_i2c_hid64_report_descriptor,
                            found_report_descriptor_length,
                            0x01u,
                            0x02u);
                        is_touchpad = i2c_hid64_report_has_usage(
                            g_i2c_hid64_report_descriptor,
                            found_report_descriptor_length,
                            0x0Du,
                            0x05u);
                        has_report_id = i2c_hid64_report_has_report_id(
                            g_i2c_hid64_report_descriptor,
                            found_report_descriptor_length);
                    }

                    if (i2c_hid64_send_command_to(
                            found_address,
                            found_command_register,
                            I2C_HID64_COMMAND_SET_POWER,
                            0u) == 0u)
                    {
                        continue;
                    }
                    if (i2c_hid64_send_command_to(
                            found_address,
                            found_command_register,
                            I2C_HID64_COMMAND_RESET,
                            0u) == 0u)
                    {
                        continue;
                    }
                    i2c_hid64_wait_reset_settle();

                    if ((keyboard_ready == 0u) && (is_mouse == 0u) && (is_touchpad == 0u))
                    {
                        (void)i2c_hid64_probe_descriptor(found_address, (u16)found_descriptor_register);
                        keyboard_address = g_i2c_hid64_address;
                        keyboard_descriptor_register = g_i2c_hid64_descriptor_register;
                        keyboard_report_descriptor_register = g_i2c_hid64_report_descriptor_register;
                        keyboard_report_descriptor_length = g_i2c_hid64_report_descriptor_length;
                        keyboard_input_register = g_i2c_hid64_input_register;
                        keyboard_command_register = g_i2c_hid64_command_register;
                        keyboard_data_register = g_i2c_hid64_data_register;
                        keyboard_max_input_length = g_i2c_hid64_max_input_length;
                        keyboard_physical_base = physical_base;
                        g_i2c_hid64_keyboard_virtual_base = g_i2c_hid64_active_virtual_base;
                        keyboard_ready = 1u;
                        i2c_hid64_log_status("keyboard ready");
                    }
                    else if ((g_i2c_hid64_pointer_found == 0u)
                        && ((is_mouse != 0u) || (is_touchpad != 0u)))
                    {
                        (void)i2c_hid64_probe_descriptor(found_address, (u16)found_descriptor_register);
                        g_i2c_hid64_pointer_found = 1u;
                        g_i2c_hid64_pointer_kind = (is_mouse != 0u)
                            ? I2C_HID64_POINTER_KIND_MOUSE
                            : I2C_HID64_POINTER_KIND_TOUCHPAD;
                        g_i2c_hid64_pointer_address = g_i2c_hid64_address;
                        g_i2c_hid64_pointer_descriptor_register = g_i2c_hid64_descriptor_register;
                        g_i2c_hid64_pointer_report_descriptor_register =
                            g_i2c_hid64_report_descriptor_register;
                        g_i2c_hid64_pointer_report_descriptor_length =
                            g_i2c_hid64_report_descriptor_length;
                        g_i2c_hid64_pointer_input_register = g_i2c_hid64_input_register;
                        g_i2c_hid64_pointer_command_register = g_i2c_hid64_command_register;
                        g_i2c_hid64_pointer_max_input_length = g_i2c_hid64_max_input_length;
                        g_i2c_hid64_pointer_report_has_id = has_report_id;
                        g_i2c_hid64_pointer_physical_base = physical_base;
                        g_i2c_hid64_pointer_virtual_base = g_i2c_hid64_active_virtual_base;
                        i2c_hid64_log_status(
                            (g_i2c_hid64_pointer_kind == I2C_HID64_POINTER_KIND_MOUSE)
                                ? "pointer mouse ready"
                                : "touchpad ready");
                    }

                    if ((keyboard_ready != 0u) && (g_i2c_hid64_pointer_found != 0u))
                    {
                        break;
                    }
                }
                if (g_i2c_hid64_error == 4u)
                {
                    g_i2c_hid64_error = 0u;
                }
                if (g_i2c_hid64_disabled != 0u)
                {
                    break;
                }
            }
            if ((g_i2c_hid64_disabled != 0u)
                || ((keyboard_ready != 0u) && (g_i2c_hid64_pointer_found != 0u)))
            {
                break;
            }
        }
    }

    for (pointer_candidate_index = 0u;
         (g_i2c_hid64_pointer_found == 0u) && (pointer_candidate_index < pointer_candidate_count);
         ++pointer_candidate_index)
    {
        pointer_flags = pci64_lpss_i2c_pointer_candidate_mmio_flags(pointer_candidate_index);
        pointer_physical_base =
            ((u64)pci64_lpss_i2c_pointer_candidate_base_high(pointer_candidate_index) << 32)
            | (u64)pci64_lpss_i2c_pointer_candidate_base_low(pointer_candidate_index);

        if ((i2c_hid64_mmio_flags_valid(pointer_flags) == 0u)
            || (pointer_physical_base == 0ull))
        {
            g_i2c_hid64_pointer_error = (g_i2c_hid64_pointer_error == 0u)
                ? 1u
                : g_i2c_hid64_pointer_error;
            continue;
        }

        if (i2c_hid64_prepare_controller(
                I2C_HID64_SECOND_MAP_VIRTUAL_BASE,
                pointer_physical_base) == 0u)
        {
            g_i2c_hid64_pointer_error = (g_i2c_hid64_pointer_error == 0u)
                ? ((g_i2c_hid64_error != 0u) ? g_i2c_hid64_error : 2u)
                : g_i2c_hid64_pointer_error;
            continue;
        }

        for (address_index = 0u; address_index < I2C_HID64_POINTER_ADDRESS_COUNT; ++address_index)
        {
            for (register_index = 0u; register_index < I2C_HID64_DESCRIPTOR_REGISTER_COUNT; ++register_index)
            {
                if (i2c_hid64_probe_descriptor(
                        g_i2c_hid64_pointer_addresses[address_index],
                        g_i2c_hid64_descriptor_registers[register_index]) != 0u)
                {
                    u32 found_address = g_i2c_hid64_address;
                    u32 found_descriptor_register = g_i2c_hid64_descriptor_register;
                    u32 found_report_descriptor_register = g_i2c_hid64_report_descriptor_register;
                    u32 found_report_descriptor_length = g_i2c_hid64_report_descriptor_length;
                    u32 found_command_register = g_i2c_hid64_command_register;
                    u32 is_mouse = 0u;
                    u32 is_touchpad = 0u;
                    u32 has_report_id = 0u;
                    u32 tentative_touchpad = 0u;

                    if (i2c_hid64_read_report_descriptor(
                            found_address,
                            found_report_descriptor_register,
                            found_report_descriptor_length) != 0u)
                    {
                        is_mouse = i2c_hid64_report_has_usage(
                            g_i2c_hid64_report_descriptor,
                            found_report_descriptor_length,
                            0x01u,
                            0x02u);
                        is_touchpad = i2c_hid64_report_has_usage(
                            g_i2c_hid64_report_descriptor,
                            found_report_descriptor_length,
                            0x0Du,
                            0x05u);
                        has_report_id = i2c_hid64_report_has_report_id(
                            g_i2c_hid64_report_descriptor,
                            found_report_descriptor_length);
                    }

                    if ((is_mouse == 0u) && (is_touchpad == 0u))
                    {
                        is_touchpad = 1u;
                        tentative_touchpad = 1u;
                    }
                    if (i2c_hid64_send_command_to(
                            found_address,
                            found_command_register,
                            I2C_HID64_COMMAND_SET_POWER,
                            0u) == 0u)
                    {
                        continue;
                    }
                    if (i2c_hid64_send_command_to(
                            found_address,
                            found_command_register,
                            I2C_HID64_COMMAND_RESET,
                            0u) == 0u)
                    {
                        continue;
                    }
                    i2c_hid64_wait_reset_settle();

                    (void)i2c_hid64_probe_descriptor(found_address, (u16)found_descriptor_register);
                    g_i2c_hid64_pointer_found = 1u;
                    g_i2c_hid64_pointer_kind = (is_mouse != 0u)
                        ? I2C_HID64_POINTER_KIND_MOUSE
                        : I2C_HID64_POINTER_KIND_TOUCHPAD;
                    g_i2c_hid64_pointer_address = g_i2c_hid64_address;
                    g_i2c_hid64_pointer_descriptor_register = g_i2c_hid64_descriptor_register;
                    g_i2c_hid64_pointer_report_descriptor_register =
                        g_i2c_hid64_report_descriptor_register;
                    g_i2c_hid64_pointer_report_descriptor_length =
                        g_i2c_hid64_report_descriptor_length;
                    g_i2c_hid64_pointer_input_register = g_i2c_hid64_input_register;
                    g_i2c_hid64_pointer_command_register = g_i2c_hid64_command_register;
                    g_i2c_hid64_pointer_max_input_length = g_i2c_hid64_max_input_length;
                    g_i2c_hid64_pointer_report_has_id = has_report_id;
                    g_i2c_hid64_pointer_physical_base = pointer_physical_base;
                    g_i2c_hid64_pointer_virtual_base = g_i2c_hid64_active_virtual_base;
                    g_i2c_hid64_pointer_error = 0u;
                    i2c_hid64_log_status(
                        (tentative_touchpad != 0u)
                            ? "second controller tentative touchpad ready"
                            : (g_i2c_hid64_pointer_kind == I2C_HID64_POINTER_KIND_MOUSE)
                            ? "second controller pointer mouse ready"
                            : "second controller touchpad ready");
                    break;
                }
                if (g_i2c_hid64_error == 4u)
                {
                    g_i2c_hid64_error = 0u;
                }
                if (g_i2c_hid64_disabled != 0u)
                {
                    break;
                }
            }
            if ((g_i2c_hid64_pointer_found != 0u) || (g_i2c_hid64_disabled != 0u))
            {
                break;
            }
        }

        if ((g_i2c_hid64_pointer_found == 0u) && (g_i2c_hid64_disabled != 0u))
        {
            g_i2c_hid64_disabled = 0u;
            g_i2c_hid64_initialized = 0u;
        }
    }
    if ((keyboard_ready != 0u) || (g_i2c_hid64_pointer_found != 0u))
    {
        g_i2c_hid64_disabled = 0u;
    }

    if (keyboard_ready != 0u)
    {
        if (g_i2c_hid64_error >= 100u)
        {
            g_i2c_hid64_error = 0u;
        }
        g_i2c_hid64_address = keyboard_address;
        g_i2c_hid64_descriptor_register = keyboard_descriptor_register;
        g_i2c_hid64_report_descriptor_register = keyboard_report_descriptor_register;
        g_i2c_hid64_report_descriptor_length = keyboard_report_descriptor_length;
        g_i2c_hid64_input_register = keyboard_input_register;
        g_i2c_hid64_command_register = keyboard_command_register;
        g_i2c_hid64_data_register = keyboard_data_register;
        g_i2c_hid64_max_input_length = keyboard_max_input_length;
        g_i2c_hid64_keyboard_physical_base = keyboard_physical_base;
        if (g_i2c_hid64_keyboard_virtual_base == 0ull)
        {
            g_i2c_hid64_keyboard_virtual_base = g_i2c_hid64_active_virtual_base;
        }
        g_i2c_hid64_device_found = 1u;
        return;
    }

    g_i2c_hid64_device_found = 0u;
    g_i2c_hid64_error = (g_i2c_hid64_error == 0u) ? 3u : g_i2c_hid64_error;
    serial_write_string("[x64] I2C HID no boot keyboard descriptor\n");
}

void i2c_hid64_poll_keyboard(void)
{
    u32 read_bytes;
    u32 report_length;

    if ((g_i2c_hid64_initialized == 0u)
        || (g_i2c_hid64_device_found == 0u)
        || (g_i2c_hid64_disabled != 0u))
    {
        return;
    }

    if ((g_i2c_hid64_keyboard_physical_base == 0ull)
        || (i2c_hid64_select_virtual(g_i2c_hid64_keyboard_virtual_base) == 0u))
    {
        return;
    }

    read_bytes = g_i2c_hid64_max_input_length;
    if (read_bytes > sizeof(g_i2c_hid64_report))
    {
        read_bytes = sizeof(g_i2c_hid64_report);
    }
    if (read_bytes < 8u)
    {
        read_bytes = 8u;
    }

    i2c_hid64_zero(g_i2c_hid64_report, sizeof(g_i2c_hid64_report));
    if (i2c_hid64_read_register(
            g_i2c_hid64_address,
            (u16)g_i2c_hid64_input_register,
            g_i2c_hid64_report,
            read_bytes) == 0u)
    {
        return;
    }

    report_length = i2c_hid64_u16(g_i2c_hid64_report);
    if (report_length > read_bytes)
    {
        report_length = read_bytes;
    }
    if ((report_length >= 10u) && (read_bytes >= 10u))
    {
        input64_accept_usb_hid_boot_report(&g_i2c_hid64_report[2], 8u);
        ++g_i2c_hid64_report_count;
    }
    else if (read_bytes >= 8u)
    {
        input64_accept_usb_hid_boot_report(g_i2c_hid64_report, 8u);
        ++g_i2c_hid64_report_count;
    }
}

static u32 i2c_hid64_payload_u16(const u8 *payload, u32 payload_length, u32 offset)
{
    if ((payload == 0) || ((offset + 1u) >= payload_length))
    {
        return 0u;
    }

    return (u32)payload[offset] | ((u32)payload[offset + 1u] << 8);
}

static u32 i2c_hid64_touchpad_contact_count(const u8 *payload, u32 payload_length, u32 base)
{
    if ((base + 9u) < payload_length && payload[base + 9u] <= 5u)
    {
        return payload[base + 9u];
    }
    if ((base + 8u) < payload_length && payload[base + 8u] <= 5u)
    {
        return payload[base + 8u];
    }
    if ((base + 6u) < payload_length && payload[base + 6u] <= 5u)
    {
        return payload[base + 6u];
    }
    if ((base + 5u) < payload_length && payload[base + 5u] <= 5u)
    {
        return payload[base + 5u];
    }
    if ((payload_length != 0u) && (payload[payload_length - 1u] <= 5u))
    {
        return payload[payload_length - 1u];
    }

    return 0u;
}

static u32 i2c_hid64_touchpad_try_contact_sample(
    const u8 *payload,
    u32 payload_length,
    u32 base,
    u32 has_contact_id)
{
    u32 flags;
    u32 contact_id = 0u;
    u32 coordinate_offset;
    u32 x;
    u32 y;
    u32 count;
    u32 active;
    u32 buttons;

    if ((payload == 0) || (base >= payload_length))
    {
        return 0u;
    }

    coordinate_offset = base + ((has_contact_id != 0u) ? 2u : 1u);
    if ((coordinate_offset + 3u) >= payload_length)
    {
        return 0u;
    }

    flags = payload[base];
    if (has_contact_id != 0u)
    {
        contact_id = payload[base + 1u] & 0x0Fu;
        if (contact_id != 0u)
        {
            return 0u;
        }
    }

    x = i2c_hid64_payload_u16(payload, payload_length, coordinate_offset);
    y = i2c_hid64_payload_u16(payload, payload_length, coordinate_offset + 2u);
    count = i2c_hid64_touchpad_contact_count(payload, payload_length, base);
    active = (((flags & 0x01u) != 0u) || (count != 0u)) ? 1u : 0u;
    buttons = ((flags & 0x04u) != 0u) ? 1u : 0u;

    if ((active != 0u) && (x == 0u) && (y == 0u))
    {
        return 0u;
    }

    input64_accept_i2c_hid_touchpad_sample(x, y, active, buttons);
    return 1u;
}

static u32 i2c_hid64_touchpad_try_coordinate_scan(const u8 *payload, u32 payload_length)
{
    static const u8 candidate_offsets[] = { 3u, 4u, 5u, 6u, 2u, 1u, 0u, 7u, 8u };
    u32 index;

    if ((payload == 0) || (payload_length < 5u))
    {
        return 0u;
    }

    for (index = 0u; index < (u32)sizeof(candidate_offsets); ++index)
    {
        u32 offset = (u32)candidate_offsets[index];
        u32 x;
        u32 y;
        u32 buttons;

        if ((offset + 3u) >= payload_length)
        {
            continue;
        }

        x = i2c_hid64_payload_u16(payload, payload_length, offset);
        y = i2c_hid64_payload_u16(payload, payload_length, offset + 2u);
        if (((x == 0u) && (y == 0u)) || (x > 32767u) || (y > 32767u))
        {
            continue;
        }

        buttons = (payload[0] & 0x07u);
        input64_accept_i2c_hid_touchpad_sample(x, y, 1u, buttons);
        return 1u;
    }

    return 0u;
}

static u32 i2c_hid64_accept_touchpad_report(const u8 *payload, u32 payload_length)
{
    u32 base = 0u;

    if ((payload == 0) || (payload_length < 5u))
    {
        return 0u;
    }

    if ((g_i2c_hid64_pointer_report_has_id != 0u)
        || ((payload_length >= 7u) && (payload[0] > 0x0Fu)))
    {
        base = 1u;
    }

    if (i2c_hid64_touchpad_try_contact_sample(payload, payload_length, base, 1u) != 0u)
    {
        return 1u;
    }

    if (i2c_hid64_touchpad_try_contact_sample(payload, payload_length, base, 0u) != 0u)
    {
        return 1u;
    }

    return i2c_hid64_touchpad_try_coordinate_scan(payload, payload_length);
}

void i2c_hid64_poll_pointer(void)
{
    u32 read_bytes;
    u32 report_length;
    const u8 *payload;
    u32 payload_length;

    if ((g_i2c_hid64_initialized == 0u)
        || (g_i2c_hid64_pointer_found == 0u)
        || (g_i2c_hid64_pointer_kind == I2C_HID64_POINTER_KIND_NONE)
        || (g_i2c_hid64_disabled != 0u))
    {
        return;
    }

    if ((g_i2c_hid64_pointer_physical_base == 0ull)
        || (i2c_hid64_select_virtual(g_i2c_hid64_pointer_virtual_base) == 0u))
    {
        return;
    }

    read_bytes = g_i2c_hid64_pointer_max_input_length;
    if (read_bytes > sizeof(g_i2c_hid64_pointer_report))
    {
        read_bytes = sizeof(g_i2c_hid64_pointer_report);
    }
    if (read_bytes < 5u)
    {
        read_bytes = 5u;
    }

    i2c_hid64_zero(g_i2c_hid64_pointer_report, sizeof(g_i2c_hid64_pointer_report));
    if (i2c_hid64_read_register(
            g_i2c_hid64_pointer_address,
            (u16)g_i2c_hid64_pointer_input_register,
            g_i2c_hid64_pointer_report,
            read_bytes) == 0u)
    {
        g_i2c_hid64_pointer_error = (g_i2c_hid64_pointer_error == 0u)
            ? g_i2c_hid64_error
            : g_i2c_hid64_pointer_error;
        return;
    }

    report_length = i2c_hid64_u16(g_i2c_hid64_pointer_report);
    if (report_length > read_bytes)
    {
        report_length = read_bytes;
    }
    if ((report_length >= 5u) && (read_bytes >= 5u))
    {
        payload = &g_i2c_hid64_pointer_report[2];
        payload_length = report_length - 2u;
    }
    else
    {
        payload = g_i2c_hid64_pointer_report;
        payload_length = read_bytes;
    }

    if ((g_i2c_hid64_pointer_kind == I2C_HID64_POINTER_KIND_TOUCHPAD)
        && (i2c_hid64_accept_touchpad_report(payload, payload_length) != 0u))
    {
        ++g_i2c_hid64_pointer_report_count;
    }
    else if ((g_i2c_hid64_pointer_kind == I2C_HID64_POINTER_KIND_MOUSE)
        && (payload_length >= 3u))
    {
        if ((payload[0] & 0xF8u) == 0u)
        {
            input64_accept_usb_hid_mouse_report(payload, payload_length);
            ++g_i2c_hid64_pointer_report_count;
        }
        else if ((payload_length >= 4u) && ((payload[1] & 0xF8u) == 0u))
        {
            input64_accept_usb_hid_mouse_report(&payload[1], payload_length - 1u);
            ++g_i2c_hid64_pointer_report_count;
        }
    }
}

u32 i2c_hid64_controller_present(void)
{
    return g_i2c_hid64_controller_present;
}

u32 i2c_hid64_device_found(void)
{
    return g_i2c_hid64_device_found;
}

u32 i2c_hid64_report_count(void)
{
    return g_i2c_hid64_report_count;
}

u32 i2c_hid64_error(void)
{
    return g_i2c_hid64_error;
}

u32 i2c_hid64_pointer_found(void)
{
    return g_i2c_hid64_pointer_found;
}

u32 i2c_hid64_primary_probe_address_count(void)
{
    return I2C_HID64_PRIMARY_ADDRESS_COUNT;
}

u32 i2c_hid64_pointer_probe_address_count(void)
{
    return I2C_HID64_POINTER_ADDRESS_COUNT;
}

u32 i2c_hid64_pointer_kind(void)
{
    return g_i2c_hid64_pointer_kind;
}

u32 i2c_hid64_pointer_address(void)
{
    return g_i2c_hid64_pointer_address;
}

u32 i2c_hid64_pointer_descriptor_register(void)
{
    return g_i2c_hid64_pointer_descriptor_register;
}

u32 i2c_hid64_pointer_report_descriptor_register(void)
{
    return g_i2c_hid64_pointer_report_descriptor_register;
}

u32 i2c_hid64_pointer_report_descriptor_length(void)
{
    return g_i2c_hid64_pointer_report_descriptor_length;
}

u32 i2c_hid64_pointer_input_register(void)
{
    return g_i2c_hid64_pointer_input_register;
}

u32 i2c_hid64_pointer_command_register(void)
{
    return g_i2c_hid64_pointer_command_register;
}

u32 i2c_hid64_pointer_max_input_length(void)
{
    return g_i2c_hid64_pointer_max_input_length;
}

u32 i2c_hid64_pointer_report_has_id(void)
{
    return g_i2c_hid64_pointer_report_has_id;
}

u32 i2c_hid64_pointer_report_count(void)
{
    return g_i2c_hid64_pointer_report_count;
}

u32 i2c_hid64_pointer_error(void)
{
    if (g_i2c_hid64_pointer_error != 0u)
    {
        return g_i2c_hid64_pointer_error;
    }

    return 0u;
}

#endif
