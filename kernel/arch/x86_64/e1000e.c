#include "e1000e_x64.h"

#include "paging_x64.h"

#define E1000E64_MAP_VIRTUAL_BASE 0xFFFFFFFF901C0000ull
#define E1000E64_KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ull
#define E1000E64_PAGE_BYTES 4096u
#define E1000E64_MAP_PAGES 32u
#define E1000E64_QUEUE_SIZE 8u
#define E1000E64_BUFFER_BYTES 2048u
#define E1000E64_ETHERNET_MIN_BYTES 60u
#define E1000E64_POLL_BUDGET 5000000u

#define E1000E64_REG_CTRL 0x0000u
#define E1000E64_REG_STATUS 0x0008u
#define E1000E64_REG_IMC 0x00D8u
#define E1000E64_REG_RCTL 0x0100u
#define E1000E64_REG_TCTL 0x0400u
#define E1000E64_REG_TIPG 0x0410u
#define E1000E64_REG_RDBAL 0x2800u
#define E1000E64_REG_RDBAH 0x2804u
#define E1000E64_REG_RDLEN 0x2808u
#define E1000E64_REG_RDH 0x2810u
#define E1000E64_REG_RDT 0x2818u
#define E1000E64_REG_TDBAL 0x3800u
#define E1000E64_REG_TDBAH 0x3804u
#define E1000E64_REG_TDLEN 0x3808u
#define E1000E64_REG_TDH 0x3810u
#define E1000E64_REG_TDT 0x3818u
#define E1000E64_REG_RAL0 0x5400u
#define E1000E64_REG_RAH0 0x5404u

#define E1000E64_CTRL_SLU 0x00000040u
#define E1000E64_CTRL_RST 0x04000000u
#define E1000E64_STATUS_LU 0x00000002u
#define E1000E64_RCTL_EN 0x00000002u
#define E1000E64_RCTL_UPE 0x00000008u
#define E1000E64_RCTL_BAM 0x00008000u
#define E1000E64_RCTL_SECRC 0x04000000u
#define E1000E64_TCTL_EN 0x00000002u
#define E1000E64_TCTL_PSP 0x00000008u
#define E1000E64_RX_STATUS_DD 0x01u
#define E1000E64_RX_STATUS_EOP 0x02u
#define E1000E64_TX_CMD_EOP 0x01u
#define E1000E64_TX_CMD_IFCS 0x02u
#define E1000E64_TX_CMD_RS 0x08u
#define E1000E64_TX_STATUS_DD 0x01u

struct e1000e64_rx_desc
{
    u64 address;
    u16 length;
    u16 checksum;
    u8 status;
    u8 errors;
    u16 special;
} __attribute__((packed));

struct e1000e64_tx_desc
{
    u64 address;
    u16 length;
    u8 cso;
    u8 cmd;
    u8 status;
    u8 css;
    u16 special;
} __attribute__((packed));

static u32 g_e1000e_address = 0xFFFFFFFFu;
static u32 g_e1000e_vendor_device = 0u;
static u32 g_e1000e_class = 0u;
static u32 g_e1000e_bar0 = 0u;
static u32 g_e1000e_bar1 = 0u;
static u32 g_e1000e_base_low = 0u;
static u32 g_e1000e_base_high = 0u;
static u32 g_e1000e_span_hint = 0u;
static u32 g_e1000e_flags = 0u;
static u32 g_e1000e_token = 0u;
static u32 g_e1000e_mapped = 0u;
static u32 g_e1000e_reset = 0u;
static u32 g_e1000e_rx_queue = 0u;
static u32 g_e1000e_tx_queue = 0u;
static u32 g_e1000e_rx_buffers = 0u;
static u32 g_e1000e_tx = 0u;
static u32 g_e1000e_rx = 0u;
static u32 g_e1000e_link_up = 0u;
static u32 g_e1000e_mac_nonzero = 0u;
static u32 g_e1000e_unavailable = 1u;
static u32 g_e1000e_error = 0u;
static u32 g_e1000e_rx_head = 0u;
static u32 g_e1000e_tx_tail = 0u;
static u8 g_e1000e_mac[6];

static struct e1000e64_rx_desc g_e1000e_rx_desc[E1000E64_QUEUE_SIZE] __attribute__((aligned(4096)));
static struct e1000e64_tx_desc g_e1000e_tx_desc[E1000E64_QUEUE_SIZE] __attribute__((aligned(4096)));
static u8 g_e1000e_rx_data[E1000E64_QUEUE_SIZE][E1000E64_BUFFER_BYTES] __attribute__((aligned(4096)));
static u8 g_e1000e_tx_data[E1000E64_QUEUE_SIZE][E1000E64_BUFFER_BYTES] __attribute__((aligned(4096)));

static u64 e1000e64_virtual_to_physical(const void *address)
{
    u64 value = (u64)address;

    if (value >= E1000E64_KERNEL_VIRTUAL_BASE)
    {
        return value - E1000E64_KERNEL_VIRTUAL_BASE;
    }

    return value;
}

static void e1000e64_fence(void)
{
    __asm__ __volatile__("mfence" ::: "memory");
}

static void e1000e64_zero(void *address, u32 byte_count)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static void e1000e64_copy(u8 *dest, const u8 *source, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        dest[index] = source[index];
    }
}

static u32 e1000e64_base_valid(void)
{
    return ((g_e1000e_flags & E1000E64_MMIO_FLAG_PRESENT) != 0u)
        && ((g_e1000e_flags & E1000E64_MMIO_FLAG_MEMORY_BAR) != 0u)
        && ((g_e1000e_flags & E1000E64_MMIO_FLAG_BASE_NONZERO) != 0u)
        && ((g_e1000e_flags & E1000E64_MMIO_FLAG_PAGE_ALIGNED) != 0u);
}

static volatile u8 *e1000e64_ptr(u32 offset)
{
    return (volatile u8 *)(u64)(E1000E64_MAP_VIRTUAL_BASE + (u64)offset);
}

static u32 e1000e64_read32(u32 offset)
{
    return *(volatile u32 *)(void *)e1000e64_ptr(offset);
}

static void e1000e64_write32(u32 offset, u32 value)
{
    u32 page = offset / E1000E64_PAGE_BYTES;

    if (page >= E1000E64_MAP_PAGES)
    {
        g_e1000e_error = (g_e1000e_error == 0u) ? 10u : g_e1000e_error;
        return;
    }

    if (paging64_kernel_mmio_write_window_open(page) == 0u)
    {
        g_e1000e_error = (g_e1000e_error == 0u) ? 11u : g_e1000e_error;
        return;
    }

    *(volatile u32 *)(void *)e1000e64_ptr(offset) = value;
    e1000e64_fence();
    (void)paging64_kernel_mmio_write_window_close(page);
}

static void e1000e64_read_mac(void)
{
    u32 ral = e1000e64_read32(E1000E64_REG_RAL0);
    u32 rah = e1000e64_read32(E1000E64_REG_RAH0);
    u32 index;
    u32 nonzero = 0u;

    g_e1000e_mac[0] = (u8)(ral & 0xFFu);
    g_e1000e_mac[1] = (u8)((ral >> 8) & 0xFFu);
    g_e1000e_mac[2] = (u8)((ral >> 16) & 0xFFu);
    g_e1000e_mac[3] = (u8)((ral >> 24) & 0xFFu);
    g_e1000e_mac[4] = (u8)(rah & 0xFFu);
    g_e1000e_mac[5] = (u8)((rah >> 8) & 0xFFu);
    for (index = 0u; index < 6u; ++index)
    {
        if (g_e1000e_mac[index] != 0u)
        {
            nonzero = 1u;
        }
    }

    g_e1000e_mac_nonzero = nonzero;
    if (nonzero != 0u)
    {
        e1000e64_write32(E1000E64_REG_RAL0, ral);
        e1000e64_write32(E1000E64_REG_RAH0, (rah & 0x0000FFFFu) | 0x80000000u);
    }
}

static u32 e1000e64_reset_controller(void)
{
    u32 control = e1000e64_read32(E1000E64_REG_CTRL);
    u32 poll;

    e1000e64_write32(E1000E64_REG_IMC, 0xFFFFFFFFu);
    e1000e64_write32(E1000E64_REG_RCTL, 0u);
    e1000e64_write32(E1000E64_REG_TCTL, 0u);
    e1000e64_write32(E1000E64_REG_CTRL, control | E1000E64_CTRL_RST);

    for (poll = 0u; poll < 1000000u; ++poll)
    {
        if ((e1000e64_read32(E1000E64_REG_CTRL) & E1000E64_CTRL_RST) == 0u)
        {
            g_e1000e_reset = 1u;
            return 1u;
        }
    }

    return 0u;
}

static void e1000e64_setup_rx(void)
{
    u32 index;

    e1000e64_zero(g_e1000e_rx_desc, sizeof(g_e1000e_rx_desc));
    for (index = 0u; index < E1000E64_QUEUE_SIZE; ++index)
    {
        e1000e64_zero(g_e1000e_rx_data[index], E1000E64_BUFFER_BYTES);
        g_e1000e_rx_desc[index].address =
            e1000e64_virtual_to_physical(g_e1000e_rx_data[index]);
    }

    g_e1000e_rx_head = 0u;
    e1000e64_write32(E1000E64_REG_RDBAL, (u32)e1000e64_virtual_to_physical(g_e1000e_rx_desc));
    e1000e64_write32(E1000E64_REG_RDBAH, (u32)(e1000e64_virtual_to_physical(g_e1000e_rx_desc) >> 32));
    e1000e64_write32(E1000E64_REG_RDLEN, sizeof(g_e1000e_rx_desc));
    e1000e64_write32(E1000E64_REG_RDH, 0u);
    e1000e64_write32(E1000E64_REG_RDT, E1000E64_QUEUE_SIZE - 1u);
    e1000e64_write32(
        E1000E64_REG_RCTL,
        E1000E64_RCTL_EN | E1000E64_RCTL_UPE | E1000E64_RCTL_BAM | E1000E64_RCTL_SECRC);
    g_e1000e_rx_queue = 1u;
    g_e1000e_rx_buffers = E1000E64_QUEUE_SIZE;
}

static void e1000e64_setup_tx(void)
{
    e1000e64_zero(g_e1000e_tx_desc, sizeof(g_e1000e_tx_desc));
    e1000e64_zero(g_e1000e_tx_data, sizeof(g_e1000e_tx_data));
    g_e1000e_tx_tail = 0u;
    e1000e64_write32(E1000E64_REG_TDBAL, (u32)e1000e64_virtual_to_physical(g_e1000e_tx_desc));
    e1000e64_write32(E1000E64_REG_TDBAH, (u32)(e1000e64_virtual_to_physical(g_e1000e_tx_desc) >> 32));
    e1000e64_write32(E1000E64_REG_TDLEN, sizeof(g_e1000e_tx_desc));
    e1000e64_write32(E1000E64_REG_TDH, 0u);
    e1000e64_write32(E1000E64_REG_TDT, 0u);
    e1000e64_write32(E1000E64_REG_TIPG, 0x0060200Au);
    e1000e64_write32(
        E1000E64_REG_TCTL,
        E1000E64_TCTL_EN | E1000E64_TCTL_PSP | (0x10u << 4) | (0x40u << 12));
    g_e1000e_tx_queue = 1u;
}

void e1000e64_register_candidate(
    u32 address,
    u32 vendor_device,
    u32 class_register,
    u32 bar0,
    u32 bar1,
    u32 base_low,
    u32 base_high,
    u32 span_hint,
    u32 flags,
    u32 token)
{
    g_e1000e_address = address;
    g_e1000e_vendor_device = vendor_device;
    g_e1000e_class = class_register;
    g_e1000e_bar0 = bar0;
    g_e1000e_bar1 = bar1;
    g_e1000e_base_low = base_low;
    g_e1000e_base_high = base_high;
    g_e1000e_span_hint = span_hint;
    g_e1000e_flags = flags;
    g_e1000e_token = token;
    g_e1000e_mapped = 0u;
    g_e1000e_reset = 0u;
    g_e1000e_rx_queue = 0u;
    g_e1000e_tx_queue = 0u;
    g_e1000e_rx_buffers = 0u;
    g_e1000e_tx = 0u;
    g_e1000e_rx = 0u;
    g_e1000e_link_up = 0u;
    g_e1000e_mac_nonzero = 0u;
    g_e1000e_unavailable = ((flags & E1000E64_MMIO_FLAG_PRESENT) != 0u) ? 0u : 1u;
    g_e1000e_error = 0u;
    g_e1000e_rx_head = 0u;
    g_e1000e_tx_tail = 0u;
    e1000e64_zero(g_e1000e_mac, sizeof(g_e1000e_mac));

    (void)g_e1000e_address;
    (void)g_e1000e_vendor_device;
    (void)g_e1000e_class;
    (void)g_e1000e_bar0;
    (void)g_e1000e_bar1;
    (void)g_e1000e_span_hint;
    (void)g_e1000e_token;
}

u32 e1000e64_init_backend(u8 *mac_out)
{
    u64 physical_base;
    u32 index;

    if (e1000e64_base_valid() == 0u)
    {
        g_e1000e_unavailable = 1u;
        return 0u;
    }

    physical_base = ((u64)g_e1000e_base_high << 32) | (u64)g_e1000e_base_low;
    if (paging64_install_kernel_mmio_mapping(
            E1000E64_MAP_VIRTUAL_BASE,
            physical_base,
            E1000E64_MAP_PAGES) == 0u)
    {
        g_e1000e_unavailable = 1u;
        g_e1000e_error = 1u;
        return 0u;
    }
    g_e1000e_mapped = 1u;

    if (e1000e64_reset_controller() == 0u)
    {
        g_e1000e_unavailable = 1u;
        g_e1000e_error = 2u;
        return 0u;
    }

    e1000e64_write32(E1000E64_REG_CTRL, e1000e64_read32(E1000E64_REG_CTRL) | E1000E64_CTRL_SLU);
    e1000e64_read_mac();
    if (g_e1000e_mac_nonzero == 0u)
    {
        g_e1000e_unavailable = 1u;
        g_e1000e_error = 3u;
        return 0u;
    }

    e1000e64_setup_rx();
    e1000e64_setup_tx();
    g_e1000e_link_up =
        ((e1000e64_read32(E1000E64_REG_STATUS) & E1000E64_STATUS_LU) != 0u) ? 1u : 0u;
    for (index = 0u; index < 6u; ++index)
    {
        mac_out[index] = g_e1000e_mac[index];
    }

    g_e1000e_unavailable = 0u;
    g_e1000e_error = 0u;
    return 1u;
}

u32 e1000e64_transmit_frame(const u8 *frame, u32 frame_bytes)
{
    struct e1000e64_tx_desc *desc;
    u32 slot;
    u32 payload_bytes = frame_bytes;
    u32 poll;
    u32 next_tail;

    if ((g_e1000e_unavailable != 0u) || (g_e1000e_tx_queue == 0u))
    {
        return 0u;
    }
    if (payload_bytes < E1000E64_ETHERNET_MIN_BYTES)
    {
        payload_bytes = E1000E64_ETHERNET_MIN_BYTES;
    }
    if (payload_bytes > E1000E64_BUFFER_BYTES)
    {
        g_e1000e_error = (g_e1000e_error == 0u) ? 4u : g_e1000e_error;
        return 0u;
    }

    slot = g_e1000e_tx_tail % E1000E64_QUEUE_SIZE;
    desc = &g_e1000e_tx_desc[slot];
    e1000e64_zero(g_e1000e_tx_data[slot], E1000E64_BUFFER_BYTES);
    e1000e64_copy(g_e1000e_tx_data[slot], frame, frame_bytes);
    desc->address = e1000e64_virtual_to_physical(g_e1000e_tx_data[slot]);
    desc->length = (u16)payload_bytes;
    desc->cso = 0u;
    desc->cmd = E1000E64_TX_CMD_EOP | E1000E64_TX_CMD_IFCS | E1000E64_TX_CMD_RS;
    desc->status = 0u;
    desc->css = 0u;
    desc->special = 0u;
    e1000e64_fence();

    next_tail = (slot + 1u) % E1000E64_QUEUE_SIZE;
    e1000e64_write32(E1000E64_REG_TDT, next_tail);
    g_e1000e_tx_tail = next_tail;

    for (poll = 0u; poll < E1000E64_POLL_BUDGET; ++poll)
    {
        if ((desc->status & E1000E64_TX_STATUS_DD) != 0u)
        {
            g_e1000e_tx = 1u;
            return 1u;
        }
    }

    g_e1000e_error = (g_e1000e_error == 0u) ? 5u : g_e1000e_error;
    return 0u;
}

u32 e1000e64_poll_receive(u8 *dest, u32 capacity, u32 *frame_bytes)
{
    struct e1000e64_rx_desc *desc;
    u32 slot;
    u32 length;

    if ((g_e1000e_unavailable != 0u) || (g_e1000e_rx_queue == 0u))
    {
        return 0u;
    }

    slot = g_e1000e_rx_head % E1000E64_QUEUE_SIZE;
    desc = &g_e1000e_rx_desc[slot];
    if ((desc->status & E1000E64_RX_STATUS_DD) == 0u)
    {
        return 0u;
    }

    length = desc->length;
    if ((desc->status & E1000E64_RX_STATUS_EOP) == 0u)
    {
        length = 0u;
    }
    if (length > capacity)
    {
        length = capacity;
    }
    if (length != 0u)
    {
        e1000e64_copy(dest, g_e1000e_rx_data[slot], length);
        *frame_bytes = length;
        g_e1000e_rx = 1u;
    }

    desc->length = 0u;
    desc->checksum = 0u;
    desc->status = 0u;
    desc->errors = 0u;
    desc->special = 0u;
    e1000e64_fence();
    e1000e64_write32(E1000E64_REG_RDT, slot);
    g_e1000e_rx_head = (slot + 1u) % E1000E64_QUEUE_SIZE;
    return (length != 0u) ? 1u : 0u;
}

u32 e1000e64_found(void)
{
    return ((g_e1000e_flags & E1000E64_MMIO_FLAG_PRESENT) != 0u) ? 1u : 0u;
}

u64 e1000e64_bar_base(void)
{
    return ((u64)g_e1000e_base_high << 32) | (u64)g_e1000e_base_low;
}

u32 e1000e64_mapped(void)
{
    return g_e1000e_mapped;
}

u32 e1000e64_reset(void)
{
    return g_e1000e_reset;
}

u32 e1000e64_rx_queue(void)
{
    return g_e1000e_rx_queue;
}

u32 e1000e64_tx_queue(void)
{
    return g_e1000e_tx_queue;
}

u32 e1000e64_rx_buffers(void)
{
    return g_e1000e_rx_buffers;
}

u32 e1000e64_tx(void)
{
    return g_e1000e_tx;
}

u32 e1000e64_rx(void)
{
    return g_e1000e_rx;
}

u32 e1000e64_link_up(void)
{
    return g_e1000e_link_up;
}

u32 e1000e64_mac_nonzero(void)
{
    return g_e1000e_mac_nonzero;
}

const u8 *e1000e64_mac(void)
{
    return g_e1000e_mac;
}

u32 e1000e64_fs_authority(void)
{
    return 0u;
}

u32 e1000e64_storage_authority(void)
{
    return 0u;
}

u32 e1000e64_ambient_authority(void)
{
    return 0u;
}

u32 e1000e64_unavailable(void)
{
    return g_e1000e_unavailable;
}

u32 e1000e64_error(void)
{
    return g_e1000e_error;
}
