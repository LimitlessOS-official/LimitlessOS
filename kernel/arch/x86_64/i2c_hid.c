#include "i2c_hid_x64.h"

#include "input_x64.h"
#include "paging_x64.h"
#include "pci_x64.h"
#include "pit.h"
#include "serial.h"

#define I2C_HID64_MAP_VIRTUAL_BASE 0xFFFFFFFF901E0000ull
#define I2C_HID64_MAP_PAGES 1u
#define I2C_HID64_PAGE_BYTES 4096u
#define I2C_HID64_POLL_LIMIT 250000u
#define I2C_HID64_TIMEOUT_TICKS 1u

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

#define I2C_HID64_ADDRESS_COUNT 2u
#define I2C_HID64_DESCRIPTOR_REGISTER_COUNT 2u
#define I2C_HID64_DESCRIPTOR_BYTES 30u
#define I2C_HID64_DESCRIPTOR_MIN_LENGTH 30u
#define I2C_HID64_REPORT_BYTES 16u
#define I2C_HID64_REPORT_DESCRIPTOR_BYTES 128u
#define I2C_HID64_TRANSFER_BYTES 128u
#define I2C_HID64_RESET_WAIT_TICKS 3u
#define I2C_HID64_RESET_WAIT_POLL_LIMIT 750000u
#define I2C_HID64_COMMAND_RESET 0x01u
#define I2C_HID64_COMMAND_SET_POWER 0x08u
#define I2C_HID64_POINTER_KIND_NONE 0u
#define I2C_HID64_POINTER_KIND_MOUSE 1u
#define I2C_HID64_POINTER_KIND_TOUCHPAD_UNSUPPORTED 2u

static const u8 g_i2c_hid64_addresses[I2C_HID64_ADDRESS_COUNT] = { 0x15u, 0x2Cu };
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
static u32 g_i2c_hid64_pointer_report_count = 0u;
static u32 g_i2c_hid64_pointer_error = 0u;
static u8 g_i2c_hid64_descriptor[I2C_HID64_DESCRIPTOR_BYTES];
static u8 g_i2c_hid64_report[I2C_HID64_REPORT_BYTES];
static u8 g_i2c_hid64_report_descriptor[I2C_HID64_REPORT_DESCRIPTOR_BYTES];
static u8 g_i2c_hid64_pointer_report[I2C_HID64_REPORT_BYTES];

static volatile u32 *i2c_hid64_reg(u32 offset)
{
    return (volatile u32 *)(u64)(I2C_HID64_MAP_VIRTUAL_BASE + (u64)offset);
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
    u64 virtual_address = I2C_HID64_MAP_VIRTUAL_BASE + (u64)offset;

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

static u32 i2c_hid64_deadline_expired(u32 start_ticks, u32 poll)
{
    if ((pit_get_ticks() - start_ticks) >= I2C_HID64_TIMEOUT_TICKS)
    {
        return 1u;
    }

    return (poll >= I2C_HID64_POLL_LIMIT) ? 1u : 0u;
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
    u32 index;

    if ((buffer == 0) || (byte_count == 0u) || (byte_count > I2C_HID64_TRANSFER_BYTES))
    {
        return 0u;
    }

    start_ticks = pit_get_ticks();
    if (i2c_hid64_set_target(address, start_ticks) == 0u)
    {
        return 0u;
    }

    if (i2c_hid64_send_data_cmd((u32)(reg & 0xFFu), start_ticks, 140u) == 0u
        || i2c_hid64_send_data_cmd((u32)(reg >> 8), start_ticks, 142u) == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        u32 command = I2C_DW_DATA_CMD_READ;
        if (index == 0u)
        {
            command |= I2C_DW_DATA_CMD_RESTART;
        }
        if ((index + 1u) == byte_count)
        {
            command |= I2C_DW_DATA_CMD_STOP;
        }
        if (i2c_hid64_send_data_cmd(command, start_ticks, 144u) == 0u)
        {
            return 0u;
        }
    }

    for (index = 0u; index < byte_count; ++index)
    {
        if (i2c_hid64_wait_status(I2C_DW_STATUS_RFNE, 1u, start_ticks, 146u) == 0u)
        {
            return 0u;
        }
        buffer[index] = (u8)(i2c_hid64_read32(I2C_DW_IC_DATA_CMD) & 0xFFu);
    }

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
        || (hid_version != 0x0001u)
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
    u32 address_index;
    u32 register_index;
    u32 flags;
    u32 keyboard_ready = 0u;
    u32 keyboard_address = 0u;
    u32 keyboard_descriptor_register = 0u;
    u32 keyboard_report_descriptor_register = 0u;
    u32 keyboard_report_descriptor_length = 0u;
    u32 keyboard_input_register = 0u;
    u32 keyboard_command_register = 0u;
    u32 keyboard_data_register = 0u;
    u32 keyboard_max_input_length = 0u;

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
    g_i2c_hid64_pointer_report_count = 0u;
    g_i2c_hid64_pointer_error = 0u;

    if (g_i2c_hid64_controller_present == 0u)
    {
        return;
    }

    flags = pci64_lpss_i2c_mmio_flags();
    if ((flags & (PCI64_LPSS_I2C_MMIO_FLAG_MEMORY_BAR
            | PCI64_LPSS_I2C_MMIO_FLAG_BASE_NONZERO
            | PCI64_LPSS_I2C_MMIO_FLAG_PAGE_ALIGNED))
        != (PCI64_LPSS_I2C_MMIO_FLAG_MEMORY_BAR
            | PCI64_LPSS_I2C_MMIO_FLAG_BASE_NONZERO
            | PCI64_LPSS_I2C_MMIO_FLAG_PAGE_ALIGNED))
    {
        g_i2c_hid64_error = 1u;
        return;
    }

    physical_base = ((u64)pci64_lpss_i2c_base_high() << 32) | (u64)pci64_lpss_i2c_base_low();
    if (paging64_install_kernel_mmio_mapping(
            I2C_HID64_MAP_VIRTUAL_BASE,
            physical_base,
            I2C_HID64_MAP_PAGES) == 0u)
    {
        g_i2c_hid64_error = 2u;
        return;
    }

    g_i2c_hid64_mapped = 1u;
    if (i2c_hid64_program_controller() == 0u)
    {
        return;
    }

    g_i2c_hid64_initialized = 1u;
    for (address_index = 0u; address_index < I2C_HID64_ADDRESS_COUNT; ++address_index)
    {
        for (register_index = 0u; register_index < I2C_HID64_DESCRIPTOR_REGISTER_COUNT; ++register_index)
        {
            if (i2c_hid64_probe_descriptor(
                    g_i2c_hid64_addresses[address_index],
                    g_i2c_hid64_descriptor_registers[register_index]) != 0u)
            {
                u32 found_address = g_i2c_hid64_address;
                u32 found_descriptor_register = g_i2c_hid64_descriptor_register;
                u32 found_report_descriptor_register = g_i2c_hid64_report_descriptor_register;
                u32 found_report_descriptor_length = g_i2c_hid64_report_descriptor_length;
                u32 found_command_register = g_i2c_hid64_command_register;
                u32 is_mouse = 0u;
                u32 is_touchpad = 0u;

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
                        : I2C_HID64_POINTER_KIND_TOUCHPAD_UNSUPPORTED;
                    g_i2c_hid64_pointer_address = g_i2c_hid64_address;
                    g_i2c_hid64_pointer_descriptor_register = g_i2c_hid64_descriptor_register;
                    g_i2c_hid64_pointer_report_descriptor_register =
                        g_i2c_hid64_report_descriptor_register;
                    g_i2c_hid64_pointer_report_descriptor_length =
                        g_i2c_hid64_report_descriptor_length;
                    g_i2c_hid64_pointer_input_register = g_i2c_hid64_input_register;
                    g_i2c_hid64_pointer_command_register = g_i2c_hid64_command_register;
                    g_i2c_hid64_pointer_max_input_length = g_i2c_hid64_max_input_length;
                    i2c_hid64_log_status(
                        (g_i2c_hid64_pointer_kind == I2C_HID64_POINTER_KIND_MOUSE)
                            ? "pointer mouse ready"
                            : "touchpad descriptor found unsupported report");
                }

                if ((keyboard_ready != 0u) && (g_i2c_hid64_pointer_found != 0u))
                {
                    break;
                }
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

    if (keyboard_ready != 0u)
    {
        g_i2c_hid64_address = keyboard_address;
        g_i2c_hid64_descriptor_register = keyboard_descriptor_register;
        g_i2c_hid64_report_descriptor_register = keyboard_report_descriptor_register;
        g_i2c_hid64_report_descriptor_length = keyboard_report_descriptor_length;
        g_i2c_hid64_input_register = keyboard_input_register;
        g_i2c_hid64_command_register = keyboard_command_register;
        g_i2c_hid64_data_register = keyboard_data_register;
        g_i2c_hid64_max_input_length = keyboard_max_input_length;
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

void i2c_hid64_poll_pointer(void)
{
    u32 read_bytes;
    u32 report_length;
    const u8 *payload;
    u32 payload_length;

    if ((g_i2c_hid64_initialized == 0u)
        || (g_i2c_hid64_pointer_found == 0u)
        || (g_i2c_hid64_pointer_kind != I2C_HID64_POINTER_KIND_MOUSE)
        || (g_i2c_hid64_disabled != 0u))
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

    if (payload_length >= 3u)
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

    return (g_i2c_hid64_pointer_kind == I2C_HID64_POINTER_KIND_TOUCHPAD_UNSUPPORTED)
        ? 201u
        : 0u;
}
