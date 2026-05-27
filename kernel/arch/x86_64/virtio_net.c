#include "virtio_net_x64.h"

#include "e1000e_x64.h"
#include "paging_x64.h"
#include "pit.h"

#define VIRTIO_NET64_MAP_VIRTUAL_BASE 0xFFFFFFFF901A0000ull
#define VIRTIO_NET64_PAGE_BYTES 4096u
#define VIRTIO_NET64_MAP_PAGES 16u
#define VIRTIO_NET64_QUEUE_SIZE 8u
#define VIRTIO_NET64_RX_BUFFER_BYTES 2048u
#define VIRTIO_NET64_TX_BUFFER_BYTES 2048u
#define VIRTIO_NET64_HEADER_BYTES 12u
#define VIRTIO_NET64_ETHERNET_MIN_BYTES 60u
#define VIRTIO_NET64_ARP_FRAME_BYTES 42u
#define VIRTIO_NET64_IPV4_HEADER_BYTES 20u
#define VIRTIO_NET64_UDP_HEADER_BYTES 8u
#define VIRTIO_NET64_TCP_HEADER_BYTES 20u
#define VIRTIO_NET64_DHCP_FIXED_BYTES 236u
#define VIRTIO_NET64_DHCP_COOKIE_BYTES 4u
#define VIRTIO_NET64_DHCP_BASE_BYTES \
    (VIRTIO_NET64_DHCP_FIXED_BYTES + VIRTIO_NET64_DHCP_COOKIE_BYTES)
#define VIRTIO_NET64_POLL_BUDGET 5000000u
#define VIRTIO_NET64_TX_POLL_TICKS 50u
#define VIRTIO_NET64_RX_POLL_TICKS 150u
#define VIRTIO_NET64_WAIT_SPIN_BUDGET 10000000u
#define VIRTIO_NET64_RX_PROCESS_BUDGET 1024u

#define VIRTIO_NET64_F_MAC 5u
#define VIRTIO_NET64_F_VERSION_1 32u

#define VIRTIO_NET64_DHCP_XID 0x4C4F5344u
#define VIRTIO_NET64_DHCP_DISCOVER 1u
#define VIRTIO_NET64_DHCP_OFFER 2u
#define VIRTIO_NET64_DHCP_REQUEST 3u
#define VIRTIO_NET64_DHCP_ACK 5u
#define VIRTIO_NET64_DNS_XID 0x4C53u
#define VIRTIO_NET64_DNS_SOURCE_PORT 53530u
#define VIRTIO_NET64_HTTP_SOURCE_PORT 49153u
#define VIRTIO_NET64_HTTP_INITIAL_SEQ 0x4C4F5348u
#define VIRTIO_NET64_DNS_RETRY_COUNT 5u
#define VIRTIO_NET64_DNS_RETRY_WAIT_TICKS 3u
#define VIRTIO_NET64_TCP_STAGE_NONE 0u
#define VIRTIO_NET64_TCP_STAGE_SYNACK 1u
#define VIRTIO_NET64_TCP_STAGE_RESPONSE 2u
#define VIRTIO_NET64_TCP_STAGE_FINACK 3u
#define VIRTIO_NET64_BACKEND_NONE 0u
#define VIRTIO_NET64_BACKEND_VIRTIO 1u
#define VIRTIO_NET64_BACKEND_E1000E 2u
#define VIRTIO_NET64_HTTP_CAPTURE_BYTES 4096u

#define VIRTIO_NET64_STATUS_ACKNOWLEDGE 0x01u
#define VIRTIO_NET64_STATUS_DRIVER 0x02u
#define VIRTIO_NET64_STATUS_DRIVER_OK 0x04u
#define VIRTIO_NET64_STATUS_FEATURES_OK 0x08u

#define VIRTIO_NET64_COMMON_DEVICE_FEATURE_SELECT 0x00u
#define VIRTIO_NET64_COMMON_DEVICE_FEATURE 0x04u
#define VIRTIO_NET64_COMMON_DRIVER_FEATURE_SELECT 0x08u
#define VIRTIO_NET64_COMMON_DRIVER_FEATURE 0x0Cu
#define VIRTIO_NET64_COMMON_DEVICE_STATUS 0x14u
#define VIRTIO_NET64_COMMON_QUEUE_SELECT 0x16u
#define VIRTIO_NET64_COMMON_QUEUE_SIZE 0x18u
#define VIRTIO_NET64_COMMON_QUEUE_ENABLE 0x1Cu
#define VIRTIO_NET64_COMMON_QUEUE_NOTIFY_OFF 0x1Eu
#define VIRTIO_NET64_COMMON_QUEUE_DESC 0x20u
#define VIRTIO_NET64_COMMON_QUEUE_DRIVER 0x28u
#define VIRTIO_NET64_COMMON_QUEUE_DEVICE 0x30u

#define VIRTIO_NET64_DESC_F_NEXT 0x0001u
#define VIRTIO_NET64_DESC_F_WRITE 0x0002u

struct virtio_net64_desc
{
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
};

struct virtio_net64_avail
{
    u16 flags;
    u16 idx;
    u16 ring[VIRTIO_NET64_QUEUE_SIZE];
    u16 used_event;
};

struct virtio_net64_used_elem
{
    u32 id;
    u32 len;
};

struct virtio_net64_used
{
    u16 flags;
    u16 idx;
    struct virtio_net64_used_elem ring[VIRTIO_NET64_QUEUE_SIZE];
    u16 avail_event;
};

static u32 g_virtio_net_address = 0xFFFFFFFFu;
static u32 g_virtio_net_vendor_device = 0u;
static u32 g_virtio_net_class = 0u;
static u32 g_virtio_net_bar = 0u;
static u32 g_virtio_net_base_low = 0u;
static u32 g_virtio_net_base_high = 0u;
static u32 g_virtio_net_span_hint = 0u;
static u32 g_virtio_net_common_offset = 0u;
static u32 g_virtio_net_notify_offset = 0u;
static u32 g_virtio_net_device_offset = 0u;
static u32 g_virtio_net_notify_multiplier = 0u;
static u32 g_virtio_net_flags = 0u;
static u32 g_virtio_net_token = 0u;

static u32 g_virtio_net_mapped = 0u;
static u32 g_virtio_net_common = 0u;
static u32 g_virtio_net_notify = 0u;
static u32 g_virtio_net_device_config = 0u;
static u32 g_virtio_net_status_ack = 0u;
static u32 g_virtio_net_status_driver = 0u;
static u32 g_virtio_net_features_ok = 0u;
static u32 g_virtio_net_driver_ok = 0u;
static u32 g_virtio_net_mac_nonzero = 0u;
static u32 g_virtio_net_rx_queue = 0u;
static u32 g_virtio_net_tx_queue = 0u;
static u32 g_virtio_net_rx_buffers = 0u;
static u32 g_virtio_net_tx = 0u;
static u32 g_virtio_net_rx = 0u;
static u32 g_virtio_net_arp_reply = 0u;
static u32 g_virtio_net_arp_ip = 0u;
static u32 g_virtio_net_unavailable = 1u;
static u32 g_virtio_net_error = 0u;
static u32 g_virtio_net_rx_notify_off = 0u;
static u32 g_virtio_net_tx_notify_off = 0u;
static u32 g_virtio_net_rx_seen = 0u;
static u32 g_virtio_net_dhcp_discover = 0u;
static u32 g_virtio_net_dhcp_offer = 0u;
static u32 g_virtio_net_dhcp_request = 0u;
static u32 g_virtio_net_dhcp_ack = 0u;
static u32 g_virtio_net_dhcp_ip = 0u;
static u32 g_virtio_net_dhcp_gateway = 0u;
static u32 g_virtio_net_dhcp_dns = 0u;
static u32 g_virtio_net_dhcp_lease = 0u;
static u32 g_virtio_net_dhcp_server = 0u;
static u32 g_virtio_net_dhcp_subnet = 0u;
static u32 g_virtio_net_dhcp_unavailable = 1u;
static u32 g_virtio_net_dhcp_error = 0u;
static u32 g_virtio_net_dns_query = 0u;
static u32 g_virtio_net_dns_response = 0u;
static u32 g_virtio_net_dns_rcode = 0u;
static u32 g_virtio_net_dns_resolved = 0u;
static u32 g_virtio_net_dns_unavailable = 1u;
static u32 g_virtio_net_dns_error = 0u;
static u32 g_virtio_net_http_connected = 0u;
static u32 g_virtio_net_http_sent = 0u;
static u32 g_virtio_net_http_status = 0u;
static u32 g_virtio_net_http_response_bytes = 0u;
static u32 g_virtio_net_http_capture_bytes = 0u;
static u32 g_virtio_net_http_unavailable = 1u;
static u32 g_virtio_net_http_error = 0u;
static u32 g_virtio_net_arp_wait_ip = 0u;
static u32 g_virtio_net_tcp_stage = VIRTIO_NET64_TCP_STAGE_NONE;
static u32 g_virtio_net_tcp_local_seq = VIRTIO_NET64_HTTP_INITIAL_SEQ;
static u32 g_virtio_net_tcp_remote_next = 0u;
static u32 g_virtio_net_tcp_synack = 0u;
static u32 g_virtio_net_tcp_ack_due = 0u;
static u32 g_virtio_net_tcp_fin_seen = 0u;
static u32 g_virtio_net_backend = VIRTIO_NET64_BACKEND_NONE;
static u8 g_virtio_net_mac[6];
static u8 g_virtio_net_arp_mac[6];
static u8 g_virtio_net_http_capture[VIRTIO_NET64_HTTP_CAPTURE_BYTES];
static u8 g_virtio_net_arp_wait_mac[6];
static u8 g_virtio_net_dns_mac[6];

static struct virtio_net64_desc g_virtio_net_rx_desc[VIRTIO_NET64_QUEUE_SIZE] __attribute__((aligned(4096)));
static struct virtio_net64_avail g_virtio_net_rx_avail __attribute__((aligned(4096)));
static struct virtio_net64_used g_virtio_net_rx_used __attribute__((aligned(4096)));
static struct virtio_net64_desc g_virtio_net_tx_desc[VIRTIO_NET64_QUEUE_SIZE] __attribute__((aligned(4096)));
static struct virtio_net64_avail g_virtio_net_tx_avail __attribute__((aligned(4096)));
static struct virtio_net64_used g_virtio_net_tx_used __attribute__((aligned(4096)));
static u8 g_virtio_net_rx_data[VIRTIO_NET64_QUEUE_SIZE][VIRTIO_NET64_RX_BUFFER_BYTES] __attribute__((aligned(4096)));
static u8 g_virtio_net_tx_data[VIRTIO_NET64_TX_BUFFER_BYTES] __attribute__((aligned(4096)));

static u64 virtio_net64_virtual_to_physical(const void *address)
{
    return paging64_kernel_physical_alias(address);
}

static void virtio_net64_fence(void)
{
    __asm__ __volatile__("mfence" ::: "memory");
}

static u32 virtio_net64_ticks_elapsed(u32 start, u32 ticks)
{
    return ((u32)(pit_get_ticks() - start) >= ticks) ? 1u : 0u;
}

static void virtio_net64_zero(void *address, u32 byte_count)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static u32 virtio_net64_base_valid(void)
{
    return ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_PRESENT) != 0u)
        && ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_MEMORY_BAR) != 0u)
        && ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_BASE_NONZERO) != 0u)
        && ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_PAGE_ALIGNED) != 0u)
        && ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_COMMON_CAP) != 0u)
        && ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_NOTIFY_CAP) != 0u)
        && ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_DEVICE_CAP) != 0u);
}

static volatile u8 *virtio_net64_ptr(u32 absolute_offset)
{
    return (volatile u8 *)(u64)(VIRTIO_NET64_MAP_VIRTUAL_BASE + (u64)absolute_offset);
}

static u8 virtio_net64_read8(u32 absolute_offset)
{
    return *virtio_net64_ptr(absolute_offset);
}

static u16 virtio_net64_read16(u32 absolute_offset)
{
    return *(volatile u16 *)(void *)virtio_net64_ptr(absolute_offset);
}

static u32 virtio_net64_read32(u32 absolute_offset)
{
    return *(volatile u32 *)(void *)virtio_net64_ptr(absolute_offset);
}

static void virtio_net64_write8(u32 absolute_offset, u8 value)
{
    u32 page = absolute_offset / VIRTIO_NET64_PAGE_BYTES;

    if (page >= VIRTIO_NET64_MAP_PAGES)
    {
        g_virtio_net_error = (g_virtio_net_error == 0u) ? 20u : g_virtio_net_error;
        return;
    }

    if (paging64_kernel_mmio_write_window_open(page) == 0u)
    {
        g_virtio_net_error = (g_virtio_net_error == 0u) ? 21u : g_virtio_net_error;
        return;
    }

    *virtio_net64_ptr(absolute_offset) = value;
    virtio_net64_fence();
    (void)paging64_kernel_mmio_write_window_close(page);
}

static void virtio_net64_write16(u32 absolute_offset, u16 value)
{
    u32 page = absolute_offset / VIRTIO_NET64_PAGE_BYTES;

    if (page >= VIRTIO_NET64_MAP_PAGES)
    {
        g_virtio_net_error = (g_virtio_net_error == 0u) ? 22u : g_virtio_net_error;
        return;
    }

    if (paging64_kernel_mmio_write_window_open(page) == 0u)
    {
        g_virtio_net_error = (g_virtio_net_error == 0u) ? 23u : g_virtio_net_error;
        return;
    }

    *(volatile u16 *)(void *)virtio_net64_ptr(absolute_offset) = value;
    virtio_net64_fence();
    (void)paging64_kernel_mmio_write_window_close(page);
}

static void virtio_net64_write32(u32 absolute_offset, u32 value)
{
    u32 page = absolute_offset / VIRTIO_NET64_PAGE_BYTES;

    if (page >= VIRTIO_NET64_MAP_PAGES)
    {
        g_virtio_net_error = (g_virtio_net_error == 0u) ? 24u : g_virtio_net_error;
        return;
    }

    if (paging64_kernel_mmio_write_window_open(page) == 0u)
    {
        g_virtio_net_error = (g_virtio_net_error == 0u) ? 25u : g_virtio_net_error;
        return;
    }

    *(volatile u32 *)(void *)virtio_net64_ptr(absolute_offset) = value;
    virtio_net64_fence();
    (void)paging64_kernel_mmio_write_window_close(page);
}

static void virtio_net64_write64(u32 absolute_offset, u64 value)
{
    virtio_net64_write32(absolute_offset, (u32)(value & 0xFFFFFFFFull));
    virtio_net64_write32(absolute_offset + 4u, (u32)(value >> 32));
}

static u32 virtio_net64_common_reg(u32 register_offset)
{
    return g_virtio_net_common_offset + register_offset;
}

static u32 virtio_net64_notify_absolute(u32 queue_notify_off)
{
    return g_virtio_net_notify_offset + (queue_notify_off * g_virtio_net_notify_multiplier);
}

static void virtio_net64_add_status(u8 bits)
{
    u8 status = virtio_net64_read8(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_STATUS));
    status = (u8)(status | bits);
    virtio_net64_write8(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_STATUS), status);
}

static u32 virtio_net64_feature_bit(u32 bit)
{
    u32 select = bit / 32u;
    u32 mask = 1u << (bit & 31u);

    virtio_net64_write32(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_FEATURE_SELECT), select);
    return (virtio_net64_read32(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_FEATURE)) & mask) != 0u
        ? 1u
        : 0u;
}

static void virtio_net64_write_driver_feature(u32 bit)
{
    u32 select = bit / 32u;
    u32 mask = 1u << (bit & 31u);

    virtio_net64_write32(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DRIVER_FEATURE_SELECT), select);
    virtio_net64_write32(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DRIVER_FEATURE), mask);
}

static void virtio_net64_read_mac(void)
{
    u32 index;
    u32 nonzero = 0u;

    for (index = 0u; index < 6u; ++index)
    {
        g_virtio_net_mac[index] =
            virtio_net64_read8(g_virtio_net_device_offset + index);
        if (g_virtio_net_mac[index] != 0u)
        {
            nonzero = 1u;
        }
    }

    g_virtio_net_mac_nonzero = nonzero;
}

static u32 virtio_net64_setup_queue(
    u16 index,
    struct virtio_net64_desc *desc,
    struct virtio_net64_avail *avail,
    struct virtio_net64_used *used,
    u32 *notify_off)
{
    u32 max_size;

    virtio_net64_write16(
        virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_SELECT),
        index);
    max_size = virtio_net64_read16(virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_SIZE));
    if (max_size < VIRTIO_NET64_QUEUE_SIZE)
    {
        return 0u;
    }

    virtio_net64_zero(desc, sizeof(struct virtio_net64_desc) * VIRTIO_NET64_QUEUE_SIZE);
    virtio_net64_zero(avail, sizeof(struct virtio_net64_avail));
    virtio_net64_zero(used, sizeof(struct virtio_net64_used));
    virtio_net64_write16(
        virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_SIZE),
        VIRTIO_NET64_QUEUE_SIZE);
    virtio_net64_write64(
        virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_DESC),
        virtio_net64_virtual_to_physical(desc));
    virtio_net64_write64(
        virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_DRIVER),
        virtio_net64_virtual_to_physical(avail));
    virtio_net64_write64(
        virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_DEVICE),
        virtio_net64_virtual_to_physical(used));
    *notify_off = virtio_net64_read16(virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_NOTIFY_OFF));
    virtio_net64_write16(
        virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_ENABLE),
        1u);
    return virtio_net64_read16(virtio_net64_common_reg(VIRTIO_NET64_COMMON_QUEUE_ENABLE)) != 0u
        ? 1u
        : 0u;
}

static void virtio_net64_stage_rx_buffers(void)
{
    u32 index;

    for (index = 0u; index < VIRTIO_NET64_QUEUE_SIZE; ++index)
    {
        virtio_net64_zero(g_virtio_net_rx_data[index], VIRTIO_NET64_RX_BUFFER_BYTES);
        g_virtio_net_rx_desc[index].addr =
            virtio_net64_virtual_to_physical(g_virtio_net_rx_data[index]);
        g_virtio_net_rx_desc[index].len = VIRTIO_NET64_RX_BUFFER_BYTES;
        g_virtio_net_rx_desc[index].flags = VIRTIO_NET64_DESC_F_WRITE;
        g_virtio_net_rx_desc[index].next = 0u;
        g_virtio_net_rx_avail.ring[index] = (u16)index;
    }

    virtio_net64_fence();
    g_virtio_net_rx_avail.idx = VIRTIO_NET64_QUEUE_SIZE;
    virtio_net64_fence();
    g_virtio_net_rx_buffers = VIRTIO_NET64_QUEUE_SIZE;
}

static void virtio_net64_notify_queue(u16 queue_index, u32 queue_notify_off)
{
    virtio_net64_write16(
        virtio_net64_notify_absolute(queue_notify_off),
        queue_index);
}

static void virtio_net64_put16be(u8 *bytes, u32 offset, u16 value)
{
    bytes[offset] = (u8)(value >> 8);
    bytes[offset + 1u] = (u8)(value & 0xFFu);
}

static void virtio_net64_put32be(u8 *bytes, u32 offset, u32 value)
{
    bytes[offset] = (u8)(value >> 24);
    bytes[offset + 1u] = (u8)(value >> 16);
    bytes[offset + 2u] = (u8)(value >> 8);
    bytes[offset + 3u] = (u8)(value & 0xFFu);
}

static u16 virtio_net64_get16be(const u8 *bytes, u32 offset)
{
    return (u16)(((u16)bytes[offset] << 8) | (u16)bytes[offset + 1u]);
}

static u32 virtio_net64_get32be(const u8 *bytes, u32 offset)
{
    return ((u32)bytes[offset] << 24) |
        ((u32)bytes[offset + 1u] << 16) |
        ((u32)bytes[offset + 2u] << 8) |
        (u32)bytes[offset + 3u];
}

static u16 virtio_net64_ipv4_checksum(const u8 *bytes, u32 byte_count)
{
    u32 sum = 0u;
    u32 index;

    for (index = 0u; index < byte_count; index += 2u)
    {
        u16 word = (u16)((u16)bytes[index] << 8);
        if ((index + 1u) < byte_count)
        {
            word = (u16)(word | bytes[index + 1u]);
        }
        sum += word;
        while ((sum >> 16) != 0u)
        {
            sum = (sum & 0xFFFFu) + (sum >> 16);
        }
    }

    return (u16)(~sum & 0xFFFFu);
}

static u32 virtio_net64_checksum_add_word(u32 sum, u16 word)
{
    sum += word;
    while ((sum >> 16) != 0u)
    {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }

    return sum;
}

static u32 virtio_net64_checksum_add_bytes(u32 sum, const u8 *bytes, u32 byte_count)
{
    u32 index;

    for (index = 0u; index < byte_count; index += 2u)
    {
        u16 word = (u16)((u16)bytes[index] << 8);
        if ((index + 1u) < byte_count)
        {
            word = (u16)(word | bytes[index + 1u]);
        }
        sum = virtio_net64_checksum_add_word(sum, word);
    }

    return sum;
}

static u16 virtio_net64_transport_checksum(
    u32 source_ip,
    u32 dest_ip,
    u8 protocol,
    const u8 *payload,
    u16 payload_bytes)
{
    u32 sum = 0u;

    sum = virtio_net64_checksum_add_word(sum, (u16)(source_ip >> 16));
    sum = virtio_net64_checksum_add_word(sum, (u16)(source_ip & 0xFFFFu));
    sum = virtio_net64_checksum_add_word(sum, (u16)(dest_ip >> 16));
    sum = virtio_net64_checksum_add_word(sum, (u16)(dest_ip & 0xFFFFu));
    sum = virtio_net64_checksum_add_word(sum, protocol);
    sum = virtio_net64_checksum_add_word(sum, payload_bytes);
    sum = virtio_net64_checksum_add_bytes(sum, payload, payload_bytes);
    return (u16)(~sum & 0xFFFFu);
}

static void virtio_net64_copy_mac(u8 *dest, const u8 *source)
{
    u32 index;

    for (index = 0u; index < 6u; ++index)
    {
        dest[index] = source[index];
    }
}

static u32 virtio_net64_transmit_current_frame(u32 frame_bytes)
{
    volatile struct virtio_net64_used *used = (volatile struct virtio_net64_used *)&g_virtio_net_tx_used;
    u32 start_idx = used->idx;
    u32 start_ticks = pit_get_ticks();
    u32 payload_bytes = frame_bytes;
    u32 poll;

    if (payload_bytes < VIRTIO_NET64_ETHERNET_MIN_BYTES)
    {
        payload_bytes = VIRTIO_NET64_ETHERNET_MIN_BYTES;
    }

    if (g_virtio_net_backend == VIRTIO_NET64_BACKEND_E1000E)
    {
        return e1000e64_transmit_frame(
            &g_virtio_net_tx_data[VIRTIO_NET64_HEADER_BYTES],
            payload_bytes);
    }

    virtio_net64_zero(g_virtio_net_tx_desc, sizeof(g_virtio_net_tx_desc));
    g_virtio_net_tx_desc[0].addr = virtio_net64_virtual_to_physical(g_virtio_net_tx_data);
    g_virtio_net_tx_desc[0].len = VIRTIO_NET64_HEADER_BYTES + payload_bytes;
    g_virtio_net_tx_desc[0].flags = 0u;
    g_virtio_net_tx_desc[0].next = 0u;
    g_virtio_net_tx_avail.ring[g_virtio_net_tx_avail.idx % VIRTIO_NET64_QUEUE_SIZE] = 0u;
    virtio_net64_fence();
    ++g_virtio_net_tx_avail.idx;
    virtio_net64_fence();
    virtio_net64_notify_queue(1u, g_virtio_net_tx_notify_off);

    for (poll = 0u; poll < VIRTIO_NET64_POLL_BUDGET; ++poll)
    {
        if (used->idx != start_idx)
        {
            return 1u;
        }
        if (virtio_net64_ticks_elapsed(start_ticks, VIRTIO_NET64_TX_POLL_TICKS) != 0u)
        {
            break;
        }
    }

    return 0u;
}

static void virtio_net64_build_arp_request(void)
{
    u8 *frame = &g_virtio_net_tx_data[VIRTIO_NET64_HEADER_BYTES];
    u32 index;

    virtio_net64_zero(g_virtio_net_tx_data, sizeof(g_virtio_net_tx_data));
    for (index = 0u; index < 6u; ++index)
    {
        frame[index] = 0xFFu;
        frame[6u + index] = g_virtio_net_mac[index];
    }
    virtio_net64_put16be(frame, 12u, 0x0806u);
    virtio_net64_put16be(frame, 14u, 0x0001u);
    virtio_net64_put16be(frame, 16u, 0x0800u);
    frame[18u] = 6u;
    frame[19u] = 4u;
    virtio_net64_put16be(frame, 20u, 0x0001u);
    for (index = 0u; index < 6u; ++index)
    {
        frame[22u + index] = g_virtio_net_mac[index];
        frame[32u + index] = 0u;
    }
    frame[28u] = 10u;
    frame[29u] = 0u;
    frame[30u] = 2u;
    frame[31u] = 15u;
    frame[38u] = 10u;
    frame[39u] = 0u;
    frame[40u] = 2u;
    frame[41u] = 2u;
}

static u32 virtio_net64_transmit_arp(void)
{
    virtio_net64_build_arp_request();
    return virtio_net64_transmit_current_frame(VIRTIO_NET64_ARP_FRAME_BYTES);
}

static u32 virtio_net64_poll_rx_match(
    u32 (*parser)(const u8 *bytes, u32 byte_count),
    u32 error_code);

static void virtio_net64_wait_ticks(u32 ticks)
{
    u32 target;
    u32 guard = 0u;

    if (ticks == 0u)
    {
        return;
    }

    target = pit_get_ticks() + ticks;
    while ((pit_get_ticks() < target) && (guard < VIRTIO_NET64_WAIT_SPIN_BUDGET))
    {
        ++guard;
    }
}

static u32 virtio_net64_parse_arp_reply(const u8 *bytes, u32 byte_count)
{
    const u8 *frame;
    u32 index;

    if (byte_count < (VIRTIO_NET64_HEADER_BYTES + VIRTIO_NET64_ARP_FRAME_BYTES))
    {
        return 0u;
    }

    frame = &bytes[VIRTIO_NET64_HEADER_BYTES];
    if ((virtio_net64_get16be(frame, 12u) != 0x0806u)
        || (virtio_net64_get16be(frame, 20u) != 0x0002u)
        || (frame[28u] != 10u)
        || (frame[29u] != 0u)
        || (frame[30u] != 2u)
        || (frame[31u] != 2u))
    {
        return 0u;
    }

    for (index = 0u; index < 6u; ++index)
    {
        g_virtio_net_arp_mac[index] = frame[22u + index];
    }
    g_virtio_net_arp_ip =
        ((u32)frame[28u] << 24) |
        ((u32)frame[29u] << 16) |
        ((u32)frame[30u] << 8) |
        (u32)frame[31u];
    return 1u;
}

static void virtio_net64_build_arp_request_for(u32 target_ip, u32 sender_ip)
{
    u8 *frame = &g_virtio_net_tx_data[VIRTIO_NET64_HEADER_BYTES];
    u32 index;

    virtio_net64_zero(g_virtio_net_tx_data, sizeof(g_virtio_net_tx_data));
    for (index = 0u; index < 6u; ++index)
    {
        frame[index] = 0xFFu;
        frame[6u + index] = g_virtio_net_mac[index];
    }
    virtio_net64_put16be(frame, 12u, 0x0806u);
    virtio_net64_put16be(frame, 14u, 0x0001u);
    virtio_net64_put16be(frame, 16u, 0x0800u);
    frame[18u] = 6u;
    frame[19u] = 4u;
    virtio_net64_put16be(frame, 20u, 0x0001u);
    for (index = 0u; index < 6u; ++index)
    {
        frame[22u + index] = g_virtio_net_mac[index];
        frame[32u + index] = 0u;
    }
    virtio_net64_put32be(frame, 28u, sender_ip);
    virtio_net64_put32be(frame, 38u, target_ip);
}

static u32 virtio_net64_parse_waited_arp_reply(const u8 *bytes, u32 byte_count)
{
    const u8 *frame;
    u32 index;

    if (byte_count < (VIRTIO_NET64_HEADER_BYTES + VIRTIO_NET64_ARP_FRAME_BYTES))
    {
        return 0u;
    }

    frame = &bytes[VIRTIO_NET64_HEADER_BYTES];
    if ((virtio_net64_get16be(frame, 12u) != 0x0806u)
        || (virtio_net64_get16be(frame, 20u) != 0x0002u)
        || (virtio_net64_get32be(frame, 28u) != g_virtio_net_arp_wait_ip))
    {
        return 0u;
    }

    for (index = 0u; index < 6u; ++index)
    {
        g_virtio_net_arp_wait_mac[index] = frame[22u + index];
    }
    return 1u;
}

static u32 virtio_net64_resolve_ipv4_mac(u32 target_ip, u8 *dest_mac)
{
    if ((target_ip == g_virtio_net_arp_ip) && (target_ip != 0u))
    {
        virtio_net64_copy_mac(dest_mac, g_virtio_net_arp_mac);
        return 1u;
    }

    g_virtio_net_arp_wait_ip = target_ip;
    virtio_net64_zero(g_virtio_net_arp_wait_mac, sizeof(g_virtio_net_arp_wait_mac));
    virtio_net64_build_arp_request_for(target_ip, g_virtio_net_dhcp_ip);
    if (virtio_net64_transmit_current_frame(VIRTIO_NET64_ARP_FRAME_BYTES) == 0u)
    {
        return 0u;
    }

    if (virtio_net64_poll_rx_match(virtio_net64_parse_waited_arp_reply, 0u) == 0u)
    {
        return 0u;
    }

    virtio_net64_copy_mac(dest_mac, g_virtio_net_arp_wait_mac);
    return 1u;
}

static void virtio_net64_requeue_rx_buffer(u32 id)
{
    if (id >= VIRTIO_NET64_QUEUE_SIZE)
    {
        return;
    }

    g_virtio_net_rx_avail.ring[g_virtio_net_rx_avail.idx % VIRTIO_NET64_QUEUE_SIZE] = (u16)id;
    virtio_net64_fence();
    ++g_virtio_net_rx_avail.idx;
    virtio_net64_fence();
    virtio_net64_notify_queue(0u, g_virtio_net_rx_notify_off);
}

static u32 virtio_net64_poll_rx_match_e1000e(
    u32 (*parser)(const u8 *bytes, u32 byte_count),
    u32 error_code)
{
    u32 poll;
    u32 start_ticks = pit_get_ticks();

    for (poll = 0u; poll < VIRTIO_NET64_POLL_BUDGET; ++poll)
    {
        u32 frame_bytes = 0u;
        if (e1000e64_poll_receive(
                &g_virtio_net_rx_data[0][VIRTIO_NET64_HEADER_BYTES],
                VIRTIO_NET64_RX_BUFFER_BYTES - VIRTIO_NET64_HEADER_BYTES,
                &frame_bytes) != 0u)
        {
            g_virtio_net_rx = 1u;
            if (parser(g_virtio_net_rx_data[0], frame_bytes + VIRTIO_NET64_HEADER_BYTES) != 0u)
            {
                return 1u;
            }
        }
        if (virtio_net64_ticks_elapsed(start_ticks, VIRTIO_NET64_RX_POLL_TICKS) != 0u)
        {
            break;
        }
    }

    if ((error_code != 0u) && (g_virtio_net_dhcp_error == 0u))
    {
        g_virtio_net_dhcp_error = error_code;
    }
    return 0u;
}

static u32 virtio_net64_poll_rx_match(
    u32 (*parser)(const u8 *bytes, u32 byte_count),
    u32 error_code)
{
    volatile struct virtio_net64_used *used = (volatile struct virtio_net64_used *)&g_virtio_net_rx_used;
    u32 start_ticks = pit_get_ticks();
    u32 poll;
    u32 processed = 0u;

    if (g_virtio_net_backend == VIRTIO_NET64_BACKEND_E1000E)
    {
        return virtio_net64_poll_rx_match_e1000e(parser, error_code);
    }

    for (poll = 0u; poll < VIRTIO_NET64_POLL_BUDGET; ++poll)
    {
        if ((poll & 0xFFFFu) == 0u)
        {
            virtio_net64_notify_queue(0u, g_virtio_net_rx_notify_off);
        }

        while ((g_virtio_net_rx_seen != used->idx)
            && (processed < VIRTIO_NET64_RX_PROCESS_BUDGET))
        {
            u32 slot = g_virtio_net_rx_seen % VIRTIO_NET64_QUEUE_SIZE;
            u32 id = used->ring[slot].id;
            u32 len = used->ring[slot].len;
            u32 matched = 0u;

            ++g_virtio_net_rx_seen;
            ++processed;
            if (id < VIRTIO_NET64_QUEUE_SIZE)
            {
                g_virtio_net_rx = 1u;
                matched = parser(g_virtio_net_rx_data[id], len);
                virtio_net64_requeue_rx_buffer(id);
                if (matched != 0u)
                    return 1u;
            }
        }

        if (processed >= VIRTIO_NET64_RX_PROCESS_BUDGET)
        {
            break;
        }
        if (virtio_net64_ticks_elapsed(start_ticks, VIRTIO_NET64_RX_POLL_TICKS) != 0u)
        {
            break;
        }
    }

    if ((error_code != 0u) && (g_virtio_net_dhcp_error == 0u))
    {
        g_virtio_net_dhcp_error = error_code;
    }
    return 0u;
}

static u32 virtio_net64_poll_arp_reply(void)
{
    return virtio_net64_poll_rx_match(virtio_net64_parse_arp_reply, 0u);
}

static void virtio_net64_build_ipv4_udp_header(
    u8 *frame,
    u16 ip_id,
    u32 source_ip,
    u32 dest_ip,
    u16 source_port,
    u16 dest_port,
    u16 udp_payload_bytes)
{
    u8 *ip = &frame[14u];
    u8 *udp = &frame[14u + VIRTIO_NET64_IPV4_HEADER_BYTES];
    u16 ip_total =
        (u16)(VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_UDP_HEADER_BYTES + udp_payload_bytes);
    u16 udp_total = (u16)(VIRTIO_NET64_UDP_HEADER_BYTES + udp_payload_bytes);
    u16 checksum;

    ip[0] = 0x45u;
    ip[1] = 0u;
    virtio_net64_put16be(ip, 2u, ip_total);
    virtio_net64_put16be(ip, 4u, ip_id);
    virtio_net64_put16be(ip, 6u, 0u);
    ip[8] = 64u;
    ip[9] = 17u;
    virtio_net64_put16be(ip, 10u, 0u);
    virtio_net64_put32be(ip, 12u, source_ip);
    virtio_net64_put32be(ip, 16u, dest_ip);
    checksum = virtio_net64_ipv4_checksum(ip, VIRTIO_NET64_IPV4_HEADER_BYTES);
    virtio_net64_put16be(ip, 10u, checksum);

    virtio_net64_put16be(udp, 0u, source_port);
    virtio_net64_put16be(udp, 2u, dest_port);
    virtio_net64_put16be(udp, 4u, udp_total);
    virtio_net64_put16be(udp, 6u, 0u);
}

static u32 virtio_net64_build_dhcp_packet(u32 message_type)
{
    u8 *frame = &g_virtio_net_tx_data[VIRTIO_NET64_HEADER_BYTES];
    u8 *dhcp = &frame[14u + VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_UDP_HEADER_BYTES];
    u32 option = 240u;
    u32 index;
    const u16 dhcp_bytes = 300u;

    virtio_net64_zero(g_virtio_net_tx_data, sizeof(g_virtio_net_tx_data));
    for (index = 0u; index < 6u; ++index)
    {
        frame[index] = 0xFFu;
        frame[6u + index] = g_virtio_net_mac[index];
    }
    virtio_net64_put16be(frame, 12u, 0x0800u);

    dhcp[0] = 1u;
    dhcp[1] = 1u;
    dhcp[2] = 6u;
    dhcp[3] = 0u;
    virtio_net64_put32be(dhcp, 4u, VIRTIO_NET64_DHCP_XID);
    virtio_net64_put16be(dhcp, 8u, 0u);
    virtio_net64_put16be(dhcp, 10u, 0x8000u);
    for (index = 0u; index < 6u; ++index)
    {
        dhcp[28u + index] = g_virtio_net_mac[index];
    }
    virtio_net64_put32be(dhcp, VIRTIO_NET64_DHCP_FIXED_BYTES, 0x63825363u);

    dhcp[option++] = 53u;
    dhcp[option++] = 1u;
    dhcp[option++] = (u8)message_type;

    if (message_type == VIRTIO_NET64_DHCP_REQUEST)
    {
        dhcp[option++] = 50u;
        dhcp[option++] = 4u;
        virtio_net64_put32be(dhcp, option, g_virtio_net_dhcp_ip);
        option += 4u;

        dhcp[option++] = 54u;
        dhcp[option++] = 4u;
        virtio_net64_put32be(dhcp, option, g_virtio_net_dhcp_server);
        option += 4u;
    }

    dhcp[option++] = 55u;
    dhcp[option++] = 5u;
    dhcp[option++] = 1u;
    dhcp[option++] = 3u;
    dhcp[option++] = 6u;
    dhcp[option++] = 51u;
    dhcp[option++] = 54u;

    dhcp[option++] = 57u;
    dhcp[option++] = 2u;
    virtio_net64_put16be(dhcp, option, 1500u);
    option += 2u;

    dhcp[option++] = 61u;
    dhcp[option++] = 7u;
    dhcp[option++] = 1u;
    for (index = 0u; index < 6u; ++index)
    {
        dhcp[option++] = g_virtio_net_mac[index];
    }

    dhcp[option++] = 255u;

    virtio_net64_build_ipv4_udp_header(
        frame,
        (message_type == VIRTIO_NET64_DHCP_DISCOVER) ? 0x4C44u : 0x4C45u,
        0u,
        0xFFFFFFFFu,
        68u,
        67u,
        dhcp_bytes);

    return 14u + VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_UDP_HEADER_BYTES + dhcp_bytes;
}

static u32 virtio_net64_transmit_dhcp(u32 message_type)
{
    u32 frame_bytes = virtio_net64_build_dhcp_packet(message_type);
    return virtio_net64_transmit_current_frame(frame_bytes);
}

static u32 virtio_net64_parse_dhcp_message(
    const u8 *bytes,
    u32 byte_count,
    u32 expected_type)
{
    const u8 *frame;
    const u8 *ip;
    const u8 *udp;
    const u8 *dhcp;
    u32 ip_header_bytes;
    u32 udp_length;
    u32 dhcp_length;
    u32 option;
    u32 message_type = 0u;
    u32 server = 0u;
    u32 router = 0u;
    u32 dns = 0u;
    u32 lease = 0u;
    u32 subnet = 0u;
    u32 yiaddr;

    if (byte_count < (VIRTIO_NET64_HEADER_BYTES + 14u + VIRTIO_NET64_IPV4_HEADER_BYTES
            + VIRTIO_NET64_UDP_HEADER_BYTES + VIRTIO_NET64_DHCP_BASE_BYTES))
    {
        return 0u;
    }

    frame = &bytes[VIRTIO_NET64_HEADER_BYTES];
    if (virtio_net64_get16be(frame, 12u) != 0x0800u)
    {
        return 0u;
    }

    ip = &frame[14u];
    ip_header_bytes = (u32)(ip[0] & 0x0Fu) * 4u;
    if (((ip[0] >> 4) != 4u) || (ip_header_bytes < VIRTIO_NET64_IPV4_HEADER_BYTES)
        || (ip[9] != 17u))
    {
        return 0u;
    }

    if (byte_count < (VIRTIO_NET64_HEADER_BYTES + 14u + ip_header_bytes
            + VIRTIO_NET64_UDP_HEADER_BYTES + VIRTIO_NET64_DHCP_BASE_BYTES))
    {
        return 0u;
    }

    udp = &frame[14u + ip_header_bytes];
    if ((virtio_net64_get16be(udp, 0u) != 67u)
        || (virtio_net64_get16be(udp, 2u) != 68u))
    {
        return 0u;
    }

    udp_length = virtio_net64_get16be(udp, 4u);
    if (udp_length < (VIRTIO_NET64_UDP_HEADER_BYTES + VIRTIO_NET64_DHCP_BASE_BYTES))
    {
        return 0u;
    }

    dhcp = &udp[VIRTIO_NET64_UDP_HEADER_BYTES];
    dhcp_length = udp_length - VIRTIO_NET64_UDP_HEADER_BYTES;
    if ((dhcp[0] != 2u)
        || (dhcp[1] != 1u)
        || (dhcp[2] != 6u)
        || (virtio_net64_get32be(dhcp, 4u) != VIRTIO_NET64_DHCP_XID)
        || (virtio_net64_get32be(dhcp, VIRTIO_NET64_DHCP_FIXED_BYTES) != 0x63825363u))
    {
        return 0u;
    }

    yiaddr = virtio_net64_get32be(dhcp, 16u);
    option = 240u;
    while (option < dhcp_length)
    {
        u8 code = dhcp[option++];
        u8 length;

        if (code == 0u)
        {
            continue;
        }
        if (code == 255u)
        {
            break;
        }
        if (option >= dhcp_length)
        {
            break;
        }
        length = dhcp[option++];
        if ((option + length) > dhcp_length)
        {
            break;
        }

        if ((code == 53u) && (length >= 1u))
        {
            message_type = dhcp[option];
        }
        else if ((code == 54u) && (length >= 4u))
        {
            server = virtio_net64_get32be(dhcp, option);
        }
        else if ((code == 1u) && (length >= 4u))
        {
            subnet = virtio_net64_get32be(dhcp, option);
        }
        else if ((code == 3u) && (length >= 4u))
        {
            router = virtio_net64_get32be(dhcp, option);
        }
        else if ((code == 6u) && (length >= 4u))
        {
            dns = virtio_net64_get32be(dhcp, option);
        }
        else if ((code == 51u) && (length >= 4u))
        {
            lease = virtio_net64_get32be(dhcp, option);
        }

        option += length;
    }

    if ((message_type != expected_type) || (yiaddr == 0u))
    {
        return 0u;
    }

    g_virtio_net_dhcp_ip = yiaddr;
    if (server != 0u)
        g_virtio_net_dhcp_server = server;
    if (router != 0u)
        g_virtio_net_dhcp_gateway = router;
    else if (g_virtio_net_dhcp_gateway == 0u)
        g_virtio_net_dhcp_gateway = g_virtio_net_dhcp_server;
    if (dns != 0u)
        g_virtio_net_dhcp_dns = dns;
    if (lease != 0u)
        g_virtio_net_dhcp_lease = lease;
    if (subnet != 0u)
        g_virtio_net_dhcp_subnet = subnet;

    if (expected_type == VIRTIO_NET64_DHCP_OFFER)
        g_virtio_net_dhcp_offer = 1u;
    if (expected_type == VIRTIO_NET64_DHCP_ACK)
        g_virtio_net_dhcp_ack = 1u;
    return 1u;
}

static u32 virtio_net64_parse_dhcp_offer(const u8 *bytes, u32 byte_count)
{
    return virtio_net64_parse_dhcp_message(bytes, byte_count, VIRTIO_NET64_DHCP_OFFER);
}

static u32 virtio_net64_parse_dhcp_ack(const u8 *bytes, u32 byte_count)
{
    return virtio_net64_parse_dhcp_message(bytes, byte_count, VIRTIO_NET64_DHCP_ACK);
}

static void virtio_net64_run_dhcp(void)
{
    if (g_virtio_net_arp_reply == 0u)
    {
        g_virtio_net_dhcp_unavailable = 1u;
        g_virtio_net_dhcp_error = 1u;
        return;
    }

    g_virtio_net_dhcp_discover = virtio_net64_transmit_dhcp(VIRTIO_NET64_DHCP_DISCOVER);
    if (g_virtio_net_dhcp_discover == 0u)
    {
        g_virtio_net_dhcp_unavailable = 1u;
        g_virtio_net_dhcp_error = 2u;
        return;
    }

    if (virtio_net64_poll_rx_match(virtio_net64_parse_dhcp_offer, 3u) == 0u)
    {
        g_virtio_net_dhcp_unavailable = 1u;
        return;
    }

    if ((g_virtio_net_dhcp_ip == 0u) || (g_virtio_net_dhcp_server == 0u))
    {
        g_virtio_net_dhcp_unavailable = 1u;
        g_virtio_net_dhcp_error = 4u;
        return;
    }

    g_virtio_net_dhcp_request = virtio_net64_transmit_dhcp(VIRTIO_NET64_DHCP_REQUEST);
    if (g_virtio_net_dhcp_request == 0u)
    {
        g_virtio_net_dhcp_unavailable = 1u;
        g_virtio_net_dhcp_error = 5u;
        return;
    }

    if (virtio_net64_poll_rx_match(virtio_net64_parse_dhcp_ack, 6u) == 0u)
    {
        g_virtio_net_dhcp_unavailable = 1u;
        return;
    }

    if (g_virtio_net_dhcp_gateway == 0u)
    {
        g_virtio_net_dhcp_gateway = g_virtio_net_dhcp_server;
    }

    g_virtio_net_dhcp_unavailable = 0u;
    g_virtio_net_dhcp_error = 0u;
}

static u32 virtio_net64_dns_skip_name(const u8 *dns, u32 dns_length, u32 offset)
{
    u32 guard;

    for (guard = 0u; guard < 64u; ++guard)
    {
        u8 length;

        if (offset >= dns_length)
        {
            return dns_length + 1u;
        }

        length = dns[offset];
        if (length == 0u)
        {
            return offset + 1u;
        }
        if ((length & 0xC0u) == 0xC0u)
        {
            return ((offset + 1u) < dns_length) ? (offset + 2u) : (dns_length + 1u);
        }
        if ((length & 0xC0u) != 0u)
        {
            return dns_length + 1u;
        }

        offset += 1u + (u32)length;
    }

    return dns_length + 1u;
}

static u32 virtio_net64_build_dns_query(void)
{
    u8 *frame = &g_virtio_net_tx_data[VIRTIO_NET64_HEADER_BYTES];
    u8 *dns = &frame[14u + VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_UDP_HEADER_BYTES];
    u32 offset = 12u;
    u32 index;
    u16 dns_bytes;

    virtio_net64_zero(g_virtio_net_tx_data, sizeof(g_virtio_net_tx_data));
    for (index = 0u; index < 6u; ++index)
    {
        frame[index] = g_virtio_net_dns_mac[index];
        frame[6u + index] = g_virtio_net_mac[index];
    }
    virtio_net64_put16be(frame, 12u, 0x0800u);

    virtio_net64_put16be(dns, 0u, VIRTIO_NET64_DNS_XID);
    virtio_net64_put16be(dns, 2u, 0x0100u);
    virtio_net64_put16be(dns, 4u, 1u);
    virtio_net64_put16be(dns, 6u, 0u);
    virtio_net64_put16be(dns, 8u, 0u);
    virtio_net64_put16be(dns, 10u, 0u);

    dns[offset++] = 7u;
    dns[offset++] = 'e';
    dns[offset++] = 'x';
    dns[offset++] = 'a';
    dns[offset++] = 'm';
    dns[offset++] = 'p';
    dns[offset++] = 'l';
    dns[offset++] = 'e';
    dns[offset++] = 3u;
    dns[offset++] = 'c';
    dns[offset++] = 'o';
    dns[offset++] = 'm';
    dns[offset++] = 0u;
    virtio_net64_put16be(dns, offset, 1u);
    offset += 2u;
    virtio_net64_put16be(dns, offset, 1u);
    offset += 2u;

    dns_bytes = (u16)offset;
    virtio_net64_build_ipv4_udp_header(
        frame,
        0x4C53u,
        g_virtio_net_dhcp_ip,
        g_virtio_net_dhcp_dns,
        VIRTIO_NET64_DNS_SOURCE_PORT,
        53u,
        dns_bytes);

    return 14u + VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_UDP_HEADER_BYTES + dns_bytes;
}

static u32 virtio_net64_parse_dns_response(const u8 *bytes, u32 byte_count)
{
    const u8 *frame;
    const u8 *ip;
    const u8 *udp;
    const u8 *dns;
    u32 ip_header_bytes;
    u32 udp_length;
    u32 dns_length;
    u32 offset;
    u32 question;
    u32 answer;
    u16 flags;
    u16 qd_count;
    u16 an_count;

    if (byte_count < (VIRTIO_NET64_HEADER_BYTES + 14u + VIRTIO_NET64_IPV4_HEADER_BYTES
            + VIRTIO_NET64_UDP_HEADER_BYTES + 12u))
    {
        return 0u;
    }

    frame = &bytes[VIRTIO_NET64_HEADER_BYTES];
    if (virtio_net64_get16be(frame, 12u) != 0x0800u)
    {
        return 0u;
    }

    ip = &frame[14u];
    ip_header_bytes = (u32)(ip[0] & 0x0Fu) * 4u;
    if (((ip[0] >> 4) != 4u) || (ip_header_bytes < VIRTIO_NET64_IPV4_HEADER_BYTES)
        || (ip[9] != 17u)
        || (virtio_net64_get32be(ip, 12u) != g_virtio_net_dhcp_dns)
        || (virtio_net64_get32be(ip, 16u) != g_virtio_net_dhcp_ip))
    {
        return 0u;
    }

    udp = &frame[14u + ip_header_bytes];
    if ((virtio_net64_get16be(udp, 0u) != 53u)
        || (virtio_net64_get16be(udp, 2u) != VIRTIO_NET64_DNS_SOURCE_PORT))
    {
        return 0u;
    }

    udp_length = virtio_net64_get16be(udp, 4u);
    if (udp_length < (VIRTIO_NET64_UDP_HEADER_BYTES + 12u))
    {
        return 0u;
    }

    dns = &udp[VIRTIO_NET64_UDP_HEADER_BYTES];
    dns_length = udp_length - VIRTIO_NET64_UDP_HEADER_BYTES;
    if ((virtio_net64_get16be(dns, 0u) != VIRTIO_NET64_DNS_XID)
        || ((virtio_net64_get16be(dns, 2u) & 0x8000u) == 0u))
    {
        return 0u;
    }

    flags = virtio_net64_get16be(dns, 2u);
    g_virtio_net_dns_response = 1u;
    g_virtio_net_dns_rcode = (u32)(flags & 0x000Fu);
    if (g_virtio_net_dns_rcode != 0u)
    {
        return 1u;
    }

    qd_count = virtio_net64_get16be(dns, 4u);
    an_count = virtio_net64_get16be(dns, 6u);
    offset = 12u;
    for (question = 0u; question < qd_count; ++question)
    {
        offset = virtio_net64_dns_skip_name(dns, dns_length, offset);
        if ((offset + 4u) > dns_length)
        {
            return 1u;
        }
        offset += 4u;
    }

    for (answer = 0u; answer < an_count; ++answer)
    {
        u16 type;
        u16 klass;
        u16 rdlength;

        offset = virtio_net64_dns_skip_name(dns, dns_length, offset);
        if ((offset + 10u) > dns_length)
        {
            return 1u;
        }
        type = virtio_net64_get16be(dns, offset);
        klass = virtio_net64_get16be(dns, offset + 2u);
        rdlength = virtio_net64_get16be(dns, offset + 8u);
        offset += 10u;
        if ((offset + rdlength) > dns_length)
        {
            return 1u;
        }
        if ((type == 1u) && (klass == 1u) && (rdlength == 4u))
        {
            g_virtio_net_dns_resolved = virtio_net64_get32be(dns, offset);
            return 1u;
        }
        offset += rdlength;
    }

    return 1u;
}

static void virtio_net64_run_dns(void)
{
    u32 attempt;

    if ((g_virtio_net_dhcp_unavailable != 0u) || (g_virtio_net_dhcp_dns == 0u))
    {
        g_virtio_net_dns_unavailable = 1u;
        g_virtio_net_dns_error = 1u;
        return;
    }

    if (virtio_net64_resolve_ipv4_mac(g_virtio_net_dhcp_dns, g_virtio_net_dns_mac) == 0u)
    {
        if (g_virtio_net_arp_ip == 0u)
        {
            g_virtio_net_dns_unavailable = 1u;
            g_virtio_net_dns_error = 2u;
            return;
        }
        virtio_net64_copy_mac(g_virtio_net_dns_mac, g_virtio_net_arp_mac);
    }

    for (attempt = 0u; attempt < VIRTIO_NET64_DNS_RETRY_COUNT; ++attempt)
    {
        g_virtio_net_dns_response = 0u;
        g_virtio_net_dns_rcode = 0u;
        g_virtio_net_dns_resolved = 0u;

        g_virtio_net_dns_query = virtio_net64_transmit_current_frame(virtio_net64_build_dns_query());
        if (g_virtio_net_dns_query == 0u)
        {
            virtio_net64_wait_ticks(VIRTIO_NET64_DNS_RETRY_WAIT_TICKS);
            continue;
        }

        if ((virtio_net64_poll_rx_match(virtio_net64_parse_dns_response, 0u) != 0u)
            && (g_virtio_net_dns_rcode == 0u)
            && (g_virtio_net_dns_resolved != 0u))
        {
            break;
        }

        virtio_net64_wait_ticks(VIRTIO_NET64_DNS_RETRY_WAIT_TICKS);
    }

    if (g_virtio_net_dns_query == 0u)
    {
        g_virtio_net_dns_unavailable = 1u;
        g_virtio_net_dns_error = 3u;
        return;
    }

    if (g_virtio_net_dns_response == 0u)
    {
        g_virtio_net_dns_unavailable = 1u;
        g_virtio_net_dns_error = 4u;
        return;
    }

    if ((g_virtio_net_dns_rcode != 0u) || (g_virtio_net_dns_resolved == 0u))
    {
        g_virtio_net_dns_unavailable = 1u;
        g_virtio_net_dns_error = 5u;
        return;
    }

    g_virtio_net_dns_unavailable = 0u;
    g_virtio_net_dns_error = 0u;
}

static void virtio_net64_build_ipv4_tcp_header(
    u8 *frame,
    u16 ip_id,
    u32 source_ip,
    u32 dest_ip,
    u16 source_port,
    u16 dest_port,
    u32 seq,
    u32 ack,
    u8 flags,
    u16 tcp_payload_bytes)
{
    u8 *ip = &frame[14u];
    u8 *tcp = &frame[14u + VIRTIO_NET64_IPV4_HEADER_BYTES];
    u16 ip_total =
        (u16)(VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_TCP_HEADER_BYTES + tcp_payload_bytes);
    u16 tcp_total = (u16)(VIRTIO_NET64_TCP_HEADER_BYTES + tcp_payload_bytes);
    u16 checksum;

    ip[0] = 0x45u;
    ip[1] = 0u;
    virtio_net64_put16be(ip, 2u, ip_total);
    virtio_net64_put16be(ip, 4u, ip_id);
    virtio_net64_put16be(ip, 6u, 0x4000u);
    ip[8] = 64u;
    ip[9] = 6u;
    virtio_net64_put16be(ip, 10u, 0u);
    virtio_net64_put32be(ip, 12u, source_ip);
    virtio_net64_put32be(ip, 16u, dest_ip);
    checksum = virtio_net64_ipv4_checksum(ip, VIRTIO_NET64_IPV4_HEADER_BYTES);
    virtio_net64_put16be(ip, 10u, checksum);

    virtio_net64_put16be(tcp, 0u, source_port);
    virtio_net64_put16be(tcp, 2u, dest_port);
    virtio_net64_put32be(tcp, 4u, seq);
    virtio_net64_put32be(tcp, 8u, ack);
    tcp[12u] = 0x50u;
    tcp[13u] = flags;
    virtio_net64_put16be(tcp, 14u, 8192u);
    virtio_net64_put16be(tcp, 16u, 0u);
    virtio_net64_put16be(tcp, 18u, 0u);
    checksum = virtio_net64_transport_checksum(source_ip, dest_ip, 6u, tcp, tcp_total);
    virtio_net64_put16be(tcp, 16u, checksum);
}

static u32 virtio_net64_transmit_tcp_packet(u8 flags, const u8 *payload, u16 payload_bytes)
{
    u8 *frame = &g_virtio_net_tx_data[VIRTIO_NET64_HEADER_BYTES];
    u8 *tcp_payload = &frame[14u + VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_TCP_HEADER_BYTES];
    u32 index;

    virtio_net64_zero(g_virtio_net_tx_data, sizeof(g_virtio_net_tx_data));
    for (index = 0u; index < 6u; ++index)
    {
        frame[index] = g_virtio_net_arp_mac[index];
        frame[6u + index] = g_virtio_net_mac[index];
    }
    virtio_net64_put16be(frame, 12u, 0x0800u);
    for (index = 0u; index < payload_bytes; ++index)
    {
        tcp_payload[index] = payload[index];
    }

    virtio_net64_build_ipv4_tcp_header(
        frame,
        (u16)(0x4800u + (g_virtio_net_tcp_local_seq & 0x00FFu)),
        g_virtio_net_dhcp_ip,
        g_virtio_net_dns_resolved,
        VIRTIO_NET64_HTTP_SOURCE_PORT,
        80u,
        g_virtio_net_tcp_local_seq,
        g_virtio_net_tcp_remote_next,
        flags,
        payload_bytes);

    return virtio_net64_transmit_current_frame(
        14u + VIRTIO_NET64_IPV4_HEADER_BYTES + VIRTIO_NET64_TCP_HEADER_BYTES + payload_bytes);
}

static u32 virtio_net64_parse_http_status(const u8 *payload, u32 payload_bytes)
{
    if ((payload_bytes >= 12u)
        && (payload[0] == 'H')
        && (payload[1] == 'T')
        && (payload[2] == 'T')
        && (payload[3] == 'P')
        && (payload[4] == '/')
        && (payload[5] == '1')
        && (payload[6] == '.')
        && (payload[8] == ' ')
        && (payload[9] >= '0') && (payload[9] <= '9')
        && (payload[10] >= '0') && (payload[10] <= '9')
        && (payload[11] >= '0') && (payload[11] <= '9'))
    {
        return ((u32)(payload[9] - '0') * 100u)
            + ((u32)(payload[10] - '0') * 10u)
            + (u32)(payload[11] - '0');
    }

    return 0u;
}

static void virtio_net64_capture_http_payload(const u8 *payload, u32 payload_bytes)
{
    u32 index;
    u32 remaining;
    u32 copy_bytes;

    if ((payload == 0) || (payload_bytes == 0u)
        || (g_virtio_net_http_capture_bytes >= VIRTIO_NET64_HTTP_CAPTURE_BYTES))
    {
        return;
    }

    remaining = VIRTIO_NET64_HTTP_CAPTURE_BYTES - g_virtio_net_http_capture_bytes;
    copy_bytes = (payload_bytes < remaining) ? payload_bytes : remaining;
    for (index = 0u; index < copy_bytes; ++index)
    {
        g_virtio_net_http_capture[g_virtio_net_http_capture_bytes + index] = payload[index];
    }
    g_virtio_net_http_capture_bytes += copy_bytes;
}

static u32 virtio_net64_parse_tcp_segment(const u8 *bytes, u32 byte_count)
{
    const u8 *frame;
    const u8 *ip;
    const u8 *tcp;
    const u8 *payload;
    u32 ip_header_bytes;
    u32 ip_total;
    u32 tcp_header_bytes;
    u32 tcp_payload_bytes;
    u32 seq;
    u32 ack;
    u8 flags;

    if (byte_count < (VIRTIO_NET64_HEADER_BYTES + 14u + VIRTIO_NET64_IPV4_HEADER_BYTES
            + VIRTIO_NET64_TCP_HEADER_BYTES))
    {
        return 0u;
    }

    frame = &bytes[VIRTIO_NET64_HEADER_BYTES];
    if (virtio_net64_get16be(frame, 12u) != 0x0800u)
    {
        return 0u;
    }

    ip = &frame[14u];
    ip_header_bytes = (u32)(ip[0] & 0x0Fu) * 4u;
    if (((ip[0] >> 4) != 4u) || (ip_header_bytes < VIRTIO_NET64_IPV4_HEADER_BYTES)
        || (ip[9] != 6u)
        || (virtio_net64_get32be(ip, 12u) != g_virtio_net_dns_resolved)
        || (virtio_net64_get32be(ip, 16u) != g_virtio_net_dhcp_ip))
    {
        return 0u;
    }

    ip_total = virtio_net64_get16be(ip, 2u);
    tcp = &frame[14u + ip_header_bytes];
    if ((virtio_net64_get16be(tcp, 0u) != 80u)
        || (virtio_net64_get16be(tcp, 2u) != VIRTIO_NET64_HTTP_SOURCE_PORT))
    {
        return 0u;
    }

    tcp_header_bytes = (u32)(tcp[12u] >> 4) * 4u;
    if ((tcp_header_bytes < VIRTIO_NET64_TCP_HEADER_BYTES)
        || (ip_total < (ip_header_bytes + tcp_header_bytes)))
    {
        return 0u;
    }

    seq = virtio_net64_get32be(tcp, 4u);
    ack = virtio_net64_get32be(tcp, 8u);
    flags = tcp[13u];
    payload = &tcp[tcp_header_bytes];
    tcp_payload_bytes = ip_total - ip_header_bytes - tcp_header_bytes;

    if (g_virtio_net_tcp_stage == VIRTIO_NET64_TCP_STAGE_SYNACK)
    {
        if (((flags & 0x12u) == 0x12u) && (ack == (VIRTIO_NET64_HTTP_INITIAL_SEQ + 1u)))
        {
            g_virtio_net_tcp_remote_next = seq + 1u;
            g_virtio_net_tcp_synack = 1u;
            return 1u;
        }
        return 0u;
    }

    if (g_virtio_net_tcp_stage == VIRTIO_NET64_TCP_STAGE_RESPONSE)
    {
        if ((tcp_payload_bytes != 0u) && (seq == g_virtio_net_tcp_remote_next))
        {
            if (g_virtio_net_http_status == 0u)
            {
                g_virtio_net_http_status =
                    virtio_net64_parse_http_status(payload, tcp_payload_bytes);
            }
            virtio_net64_capture_http_payload(payload, tcp_payload_bytes);
            g_virtio_net_http_response_bytes += tcp_payload_bytes;
            g_virtio_net_tcp_remote_next += tcp_payload_bytes;
            g_virtio_net_tcp_ack_due = 1u;
        }
        if ((flags & 0x01u) != 0u)
        {
            if (seq == g_virtio_net_tcp_remote_next)
            {
                ++g_virtio_net_tcp_remote_next;
            }
            g_virtio_net_tcp_fin_seen = 1u;
            g_virtio_net_tcp_ack_due = 1u;
        }
        return (g_virtio_net_tcp_ack_due != 0u) ? 1u : 0u;
    }

    if (g_virtio_net_tcp_stage == VIRTIO_NET64_TCP_STAGE_FINACK)
    {
        return (((flags & 0x10u) != 0u) && (ack == g_virtio_net_tcp_local_seq)) ? 1u : 0u;
    }

    return 0u;
}

static u32 virtio_net64_transmit_tcp_ack(void)
{
    return virtio_net64_transmit_tcp_packet(0x10u, (const u8 *)"", 0u);
}

static void virtio_net64_run_http(void)
{
    static const u8 request[] =
        "GET / HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n"
        "\r\n";
    u32 pass;

    if ((g_virtio_net_dns_unavailable != 0u) || (g_virtio_net_dns_resolved == 0u))
    {
        g_virtio_net_http_unavailable = 1u;
        g_virtio_net_http_error = 1u;
        return;
    }

    g_virtio_net_tcp_local_seq = VIRTIO_NET64_HTTP_INITIAL_SEQ;
    g_virtio_net_tcp_remote_next = 0u;
    g_virtio_net_tcp_stage = VIRTIO_NET64_TCP_STAGE_SYNACK;
    if (virtio_net64_transmit_tcp_packet(0x02u, (const u8 *)"", 0u) == 0u)
    {
        g_virtio_net_http_unavailable = 1u;
        g_virtio_net_http_error = 2u;
        return;
    }
    ++g_virtio_net_tcp_local_seq;

    for (pass = 0u; (pass < 4u) && (g_virtio_net_tcp_synack == 0u); ++pass)
    {
        (void)virtio_net64_poll_rx_match(virtio_net64_parse_tcp_segment, 0u);
    }

    if (g_virtio_net_tcp_synack == 0u)
    {
        g_virtio_net_http_unavailable = 1u;
        g_virtio_net_http_error = 3u;
        return;
    }

    if (virtio_net64_transmit_tcp_ack() == 0u)
    {
        g_virtio_net_http_unavailable = 1u;
        g_virtio_net_http_error = 4u;
        return;
    }
    g_virtio_net_http_connected = 1u;

    if (virtio_net64_transmit_tcp_packet(
            0x18u,
            request,
            (u16)(sizeof(request) - 1u)) == 0u)
    {
        g_virtio_net_http_unavailable = 1u;
        g_virtio_net_http_error = 5u;
        return;
    }
    g_virtio_net_tcp_local_seq += (u32)(sizeof(request) - 1u);
    g_virtio_net_http_sent = 1u;

    g_virtio_net_tcp_stage = VIRTIO_NET64_TCP_STAGE_RESPONSE;
    for (pass = 0u; pass < 12u; ++pass)
    {
        g_virtio_net_tcp_ack_due = 0u;
        if (virtio_net64_poll_rx_match(virtio_net64_parse_tcp_segment, 0u) == 0u)
        {
            continue;
        }
        if (g_virtio_net_tcp_ack_due != 0u)
        {
            (void)virtio_net64_transmit_tcp_ack();
        }
        if (g_virtio_net_tcp_fin_seen != 0u)
        {
            break;
        }
    }

    if ((g_virtio_net_http_response_bytes == 0u) || (g_virtio_net_http_status == 0u))
    {
        g_virtio_net_http_unavailable = 1u;
        g_virtio_net_http_error = 6u;
        return;
    }

    g_virtio_net_tcp_stage = VIRTIO_NET64_TCP_STAGE_FINACK;
    if (virtio_net64_transmit_tcp_packet(0x11u, (const u8 *)"", 0u) != 0u)
    {
        ++g_virtio_net_tcp_local_seq;
        (void)virtio_net64_poll_rx_match(virtio_net64_parse_tcp_segment, 0u);
    }

    g_virtio_net_http_unavailable = 0u;
    g_virtio_net_http_error = 0u;
}

void virtio_net64_register_candidate(
    u32 address,
    u32 vendor_device,
    u32 class_register,
    u32 bar,
    u32 base_low,
    u32 base_high,
    u32 span_hint,
    u32 common_offset,
    u32 notify_offset,
    u32 device_offset,
    u32 notify_multiplier,
    u32 flags,
    u32 token)
{
    g_virtio_net_address = address;
    g_virtio_net_vendor_device = vendor_device;
    g_virtio_net_class = class_register;
    g_virtio_net_bar = bar;
    g_virtio_net_base_low = base_low;
    g_virtio_net_base_high = base_high;
    g_virtio_net_span_hint = span_hint;
    g_virtio_net_common_offset = common_offset;
    g_virtio_net_notify_offset = notify_offset;
    g_virtio_net_device_offset = device_offset;
    g_virtio_net_notify_multiplier = (notify_multiplier != 0u) ? notify_multiplier : 1u;
    g_virtio_net_flags = flags;
    g_virtio_net_token = token;
    g_virtio_net_mapped = 0u;
    g_virtio_net_common = 0u;
    g_virtio_net_notify = 0u;
    g_virtio_net_device_config = 0u;
    g_virtio_net_status_ack = 0u;
    g_virtio_net_status_driver = 0u;
    g_virtio_net_features_ok = 0u;
    g_virtio_net_driver_ok = 0u;
    g_virtio_net_mac_nonzero = 0u;
    g_virtio_net_rx_queue = 0u;
    g_virtio_net_tx_queue = 0u;
    g_virtio_net_rx_buffers = 0u;
    g_virtio_net_tx = 0u;
    g_virtio_net_rx = 0u;
    g_virtio_net_arp_reply = 0u;
    g_virtio_net_arp_ip = 0u;
    g_virtio_net_unavailable = ((flags & VIRTIO_NET64_MMIO_FLAG_PRESENT) != 0u) ? 0u : 1u;
    g_virtio_net_error = 0u;
    g_virtio_net_rx_notify_off = 0u;
    g_virtio_net_tx_notify_off = 0u;
    g_virtio_net_rx_seen = 0u;
    g_virtio_net_dhcp_discover = 0u;
    g_virtio_net_dhcp_offer = 0u;
    g_virtio_net_dhcp_request = 0u;
    g_virtio_net_dhcp_ack = 0u;
    g_virtio_net_dhcp_ip = 0u;
    g_virtio_net_dhcp_gateway = 0u;
    g_virtio_net_dhcp_dns = 0u;
    g_virtio_net_dhcp_lease = 0u;
    g_virtio_net_dhcp_server = 0u;
    g_virtio_net_dhcp_subnet = 0u;
    g_virtio_net_dhcp_unavailable = ((flags & VIRTIO_NET64_MMIO_FLAG_PRESENT) != 0u) ? 0u : 1u;
    g_virtio_net_dhcp_error = 0u;
    g_virtio_net_dns_query = 0u;
    g_virtio_net_dns_response = 0u;
    g_virtio_net_dns_rcode = 0u;
    g_virtio_net_dns_resolved = 0u;
    g_virtio_net_dns_unavailable = ((flags & VIRTIO_NET64_MMIO_FLAG_PRESENT) != 0u) ? 0u : 1u;
    g_virtio_net_dns_error = 0u;
    g_virtio_net_http_connected = 0u;
    g_virtio_net_http_sent = 0u;
    g_virtio_net_http_status = 0u;
    g_virtio_net_http_response_bytes = 0u;
    g_virtio_net_http_capture_bytes = 0u;
    g_virtio_net_http_unavailable = ((flags & VIRTIO_NET64_MMIO_FLAG_PRESENT) != 0u) ? 0u : 1u;
    g_virtio_net_http_error = 0u;
    g_virtio_net_arp_wait_ip = 0u;
    g_virtio_net_tcp_stage = VIRTIO_NET64_TCP_STAGE_NONE;
    g_virtio_net_tcp_local_seq = VIRTIO_NET64_HTTP_INITIAL_SEQ;
    g_virtio_net_tcp_remote_next = 0u;
    g_virtio_net_tcp_synack = 0u;
    g_virtio_net_tcp_ack_due = 0u;
    g_virtio_net_tcp_fin_seen = 0u;
    g_virtio_net_backend = VIRTIO_NET64_BACKEND_NONE;
    virtio_net64_zero(g_virtio_net_mac, sizeof(g_virtio_net_mac));
    virtio_net64_zero(g_virtio_net_arp_mac, sizeof(g_virtio_net_arp_mac));
    virtio_net64_zero(g_virtio_net_arp_wait_mac, sizeof(g_virtio_net_arp_wait_mac));
    virtio_net64_zero(g_virtio_net_dns_mac, sizeof(g_virtio_net_dns_mac));
    virtio_net64_zero(g_virtio_net_http_capture, sizeof(g_virtio_net_http_capture));

    (void)g_virtio_net_address;
    (void)g_virtio_net_vendor_device;
    (void)g_virtio_net_class;
    (void)g_virtio_net_bar;
    (void)g_virtio_net_span_hint;
    (void)g_virtio_net_token;
}

static void virtio_net64_reset_exchange_state(u32 present)
{
    g_virtio_net_tx = 0u;
    g_virtio_net_rx = 0u;
    g_virtio_net_arp_reply = 0u;
    g_virtio_net_arp_ip = 0u;
    g_virtio_net_unavailable = (present != 0u) ? 0u : 1u;
    g_virtio_net_error = 0u;
    g_virtio_net_rx_seen = 0u;
    g_virtio_net_dhcp_discover = 0u;
    g_virtio_net_dhcp_offer = 0u;
    g_virtio_net_dhcp_request = 0u;
    g_virtio_net_dhcp_ack = 0u;
    g_virtio_net_dhcp_ip = 0u;
    g_virtio_net_dhcp_gateway = 0u;
    g_virtio_net_dhcp_dns = 0u;
    g_virtio_net_dhcp_lease = 0u;
    g_virtio_net_dhcp_server = 0u;
    g_virtio_net_dhcp_subnet = 0u;
    g_virtio_net_dhcp_unavailable = (present != 0u) ? 0u : 1u;
    g_virtio_net_dhcp_error = 0u;
    g_virtio_net_dns_query = 0u;
    g_virtio_net_dns_response = 0u;
    g_virtio_net_dns_rcode = 0u;
    g_virtio_net_dns_resolved = 0u;
    g_virtio_net_dns_unavailable = (present != 0u) ? 0u : 1u;
    g_virtio_net_dns_error = 0u;
    g_virtio_net_http_connected = 0u;
    g_virtio_net_http_sent = 0u;
    g_virtio_net_http_status = 0u;
    g_virtio_net_http_response_bytes = 0u;
    g_virtio_net_http_capture_bytes = 0u;
    g_virtio_net_http_unavailable = (present != 0u) ? 0u : 1u;
    g_virtio_net_http_error = 0u;
    g_virtio_net_arp_wait_ip = 0u;
    g_virtio_net_tcp_stage = VIRTIO_NET64_TCP_STAGE_NONE;
    g_virtio_net_tcp_local_seq = VIRTIO_NET64_HTTP_INITIAL_SEQ;
    g_virtio_net_tcp_remote_next = 0u;
    g_virtio_net_tcp_synack = 0u;
    g_virtio_net_tcp_ack_due = 0u;
    g_virtio_net_tcp_fin_seen = 0u;
    virtio_net64_zero(g_virtio_net_arp_mac, sizeof(g_virtio_net_arp_mac));
    virtio_net64_zero(g_virtio_net_arp_wait_mac, sizeof(g_virtio_net_arp_wait_mac));
    virtio_net64_zero(g_virtio_net_dns_mac, sizeof(g_virtio_net_dns_mac));
    virtio_net64_zero(g_virtio_net_http_capture, sizeof(g_virtio_net_http_capture));
}

static void virtio_net64_run_probe_exchange(void)
{
    g_virtio_net_tx = virtio_net64_transmit_arp();
    if (g_virtio_net_tx == 0u)
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 7u;
        return;
    }

    g_virtio_net_arp_reply = virtio_net64_poll_arp_reply();
    if (g_virtio_net_arp_reply == 0u)
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 8u;
        return;
    }

    virtio_net64_run_dhcp();
    virtio_net64_run_dns();
    virtio_net64_run_http();

    g_virtio_net_unavailable = 0u;
    g_virtio_net_error = 0u;
}

void virtio_net64_init(void)
{
    u64 physical_base;
    u8 status;
    u32 index;

    if (e1000e64_found() != 0u)
    {
        g_virtio_net_backend = VIRTIO_NET64_BACKEND_E1000E;
        virtio_net64_reset_exchange_state(1u);
        if (e1000e64_init_backend(g_virtio_net_mac) == 0u)
        {
            g_virtio_net_unavailable = 1u;
            g_virtio_net_error = 30u;
            return;
        }

        g_virtio_net_mapped = e1000e64_mapped();
        g_virtio_net_common = 0u;
        g_virtio_net_notify = 0u;
        g_virtio_net_device_config = 0u;
        g_virtio_net_status_ack = 1u;
        g_virtio_net_status_driver = 1u;
        g_virtio_net_features_ok = 1u;
        g_virtio_net_driver_ok = 1u;
        g_virtio_net_mac_nonzero = e1000e64_mac_nonzero();
        g_virtio_net_rx_queue = e1000e64_rx_queue();
        g_virtio_net_tx_queue = e1000e64_tx_queue();
        g_virtio_net_rx_buffers = e1000e64_rx_buffers();
        for (index = 0u; index < 6u; ++index)
        {
            g_virtio_net_mac[index] = e1000e64_mac()[index];
        }
        virtio_net64_run_probe_exchange();
        return;
    }

    if (virtio_net64_base_valid() == 0u)
    {
        g_virtio_net_unavailable = 1u;
        return;
    }

    g_virtio_net_backend = VIRTIO_NET64_BACKEND_VIRTIO;
    physical_base = ((u64)g_virtio_net_base_high << 32) | (u64)g_virtio_net_base_low;
    if (paging64_install_kernel_mmio_mapping(
            VIRTIO_NET64_MAP_VIRTUAL_BASE,
            physical_base,
            VIRTIO_NET64_MAP_PAGES) == 0u)
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 1u;
        return;
    }

    g_virtio_net_mapped = 1u;
    g_virtio_net_common = 1u;
    g_virtio_net_notify = 1u;
    g_virtio_net_device_config = 1u;

    virtio_net64_write8(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_STATUS), 0u);
    virtio_net64_add_status(VIRTIO_NET64_STATUS_ACKNOWLEDGE);
    g_virtio_net_status_ack =
        (virtio_net64_read8(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_STATUS))
            & VIRTIO_NET64_STATUS_ACKNOWLEDGE) != 0u ? 1u : 0u;
    virtio_net64_add_status(VIRTIO_NET64_STATUS_DRIVER);
    g_virtio_net_status_driver =
        (virtio_net64_read8(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_STATUS))
            & VIRTIO_NET64_STATUS_DRIVER) != 0u ? 1u : 0u;

    if ((virtio_net64_feature_bit(VIRTIO_NET64_F_MAC) == 0u)
        || (virtio_net64_feature_bit(VIRTIO_NET64_F_VERSION_1) == 0u))
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 2u;
        return;
    }

    virtio_net64_write_driver_feature(VIRTIO_NET64_F_MAC);
    virtio_net64_write_driver_feature(VIRTIO_NET64_F_VERSION_1);
    virtio_net64_add_status(VIRTIO_NET64_STATUS_FEATURES_OK);
    status = virtio_net64_read8(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_STATUS));
    if ((status & VIRTIO_NET64_STATUS_FEATURES_OK) == 0u)
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 3u;
        return;
    }
    g_virtio_net_features_ok = 1u;
    virtio_net64_read_mac();
    if (g_virtio_net_mac_nonzero == 0u)
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 4u;
        return;
    }

    g_virtio_net_rx_queue = virtio_net64_setup_queue(
        0u,
        g_virtio_net_rx_desc,
        &g_virtio_net_rx_avail,
        &g_virtio_net_rx_used,
        &g_virtio_net_rx_notify_off);
    g_virtio_net_tx_queue = virtio_net64_setup_queue(
        1u,
        g_virtio_net_tx_desc,
        &g_virtio_net_tx_avail,
        &g_virtio_net_tx_used,
        &g_virtio_net_tx_notify_off);
    if ((g_virtio_net_rx_queue == 0u) || (g_virtio_net_tx_queue == 0u))
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 5u;
        return;
    }

    virtio_net64_add_status(VIRTIO_NET64_STATUS_DRIVER_OK);
    g_virtio_net_driver_ok =
        (virtio_net64_read8(virtio_net64_common_reg(VIRTIO_NET64_COMMON_DEVICE_STATUS))
            & VIRTIO_NET64_STATUS_DRIVER_OK) != 0u ? 1u : 0u;
    if (g_virtio_net_driver_ok == 0u)
    {
        g_virtio_net_unavailable = 1u;
        g_virtio_net_error = 6u;
        return;
    }

    virtio_net64_stage_rx_buffers();
    virtio_net64_notify_queue(0u, g_virtio_net_rx_notify_off);
    virtio_net64_run_probe_exchange();
}

u32 virtio_net64_found(void)
{
    return ((g_virtio_net_flags & VIRTIO_NET64_MMIO_FLAG_PRESENT) != 0u) ? 1u : 0u;
}

u64 virtio_net64_bar_base(void)
{
    return ((u64)g_virtio_net_base_high << 32) | (u64)g_virtio_net_base_low;
}

u32 virtio_net64_mapped(void)
{
    return g_virtio_net_mapped;
}

u32 virtio_net64_common(void)
{
    return g_virtio_net_common;
}

u32 virtio_net64_notify(void)
{
    return g_virtio_net_notify;
}

u32 virtio_net64_device_config(void)
{
    return g_virtio_net_device_config;
}

u32 virtio_net64_status_ack(void)
{
    return g_virtio_net_status_ack;
}

u32 virtio_net64_status_driver(void)
{
    return g_virtio_net_status_driver;
}

u32 virtio_net64_features_ok(void)
{
    return g_virtio_net_features_ok;
}

u32 virtio_net64_driver_ok(void)
{
    return g_virtio_net_driver_ok;
}

u32 virtio_net64_mac_nonzero(void)
{
    return g_virtio_net_mac_nonzero;
}

const u8 *virtio_net64_mac(void)
{
    return g_virtio_net_mac;
}

u32 virtio_net64_rx_queue(void)
{
    return g_virtio_net_rx_queue;
}

u32 virtio_net64_tx_queue(void)
{
    return g_virtio_net_tx_queue;
}

u32 virtio_net64_rx_buffers(void)
{
    return g_virtio_net_rx_buffers;
}

u32 virtio_net64_tx(void)
{
    return g_virtio_net_tx;
}

u32 virtio_net64_rx(void)
{
    return g_virtio_net_rx;
}

u32 virtio_net64_arp_reply(void)
{
    return g_virtio_net_arp_reply;
}

const u8 *virtio_net64_arp_mac(void)
{
    return g_virtio_net_arp_mac;
}

u32 virtio_net64_arp_ip(void)
{
    return g_virtio_net_arp_ip;
}

u32 virtio_net64_dhcp_discover(void)
{
    return g_virtio_net_dhcp_discover;
}

u32 virtio_net64_dhcp_offer(void)
{
    return g_virtio_net_dhcp_offer;
}

u32 virtio_net64_dhcp_request(void)
{
    return g_virtio_net_dhcp_request;
}

u32 virtio_net64_dhcp_ack(void)
{
    return g_virtio_net_dhcp_ack;
}

u32 virtio_net64_dhcp_ip(void)
{
    return g_virtio_net_dhcp_ip;
}

u32 virtio_net64_dhcp_gateway(void)
{
    return g_virtio_net_dhcp_gateway;
}

u32 virtio_net64_dhcp_dns(void)
{
    return g_virtio_net_dhcp_dns;
}

u32 virtio_net64_dhcp_lease(void)
{
    return g_virtio_net_dhcp_lease;
}

u32 virtio_net64_dhcp_unavailable(void)
{
    return g_virtio_net_dhcp_unavailable;
}

u32 virtio_net64_dhcp_error(void)
{
    return g_virtio_net_dhcp_error;
}

u32 virtio_net64_dns_query(void)
{
    return g_virtio_net_dns_query;
}

u32 virtio_net64_dns_response(void)
{
    return g_virtio_net_dns_response;
}

u32 virtio_net64_dns_rcode(void)
{
    return g_virtio_net_dns_rcode;
}

u32 virtio_net64_dns_resolved(void)
{
    return g_virtio_net_dns_resolved;
}

u32 virtio_net64_dns_unavailable(void)
{
    return g_virtio_net_dns_unavailable;
}

u32 virtio_net64_dns_error(void)
{
    return g_virtio_net_dns_error;
}

u32 virtio_net64_http_connected(void)
{
    return g_virtio_net_http_connected;
}

u32 virtio_net64_http_sent(void)
{
    return g_virtio_net_http_sent;
}

u32 virtio_net64_http_status(void)
{
    return g_virtio_net_http_status;
}

u32 virtio_net64_http_response_bytes(void)
{
    return g_virtio_net_http_response_bytes;
}

u32 virtio_net64_http_captured_bytes(void)
{
    return g_virtio_net_http_capture_bytes;
}

u32 virtio_net64_http_copy_response(u8 *destination, u32 capacity)
{
    u32 index;
    u32 copy_bytes;

    if ((destination == 0) || (capacity == 0u))
    {
        return 0u;
    }

    copy_bytes = (g_virtio_net_http_capture_bytes < capacity)
        ? g_virtio_net_http_capture_bytes
        : capacity;
    for (index = 0u; index < copy_bytes; ++index)
    {
        destination[index] = g_virtio_net_http_capture[index];
    }

    return copy_bytes;
}

u32 virtio_net64_http_unavailable(void)
{
    return g_virtio_net_http_unavailable;
}

u32 virtio_net64_http_error(void)
{
    return g_virtio_net_http_error;
}

u32 virtio_net64_fs_authority(void)
{
    return 0u;
}

u32 virtio_net64_storage_authority(void)
{
    return 0u;
}

u32 virtio_net64_ambient_authority(void)
{
    return 0u;
}

u32 virtio_net64_unavailable(void)
{
    return g_virtio_net_unavailable;
}

u32 virtio_net64_error(void)
{
    return g_virtio_net_error;
}
