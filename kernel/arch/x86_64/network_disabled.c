#include "e1000e_x64.h"
#include "virtio_net_x64.h"

static const u8 g_disabled_mac[6] = {0, 0, 0, 0, 0, 0};

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
    (void)address;
    (void)vendor_device;
    (void)class_register;
    (void)bar;
    (void)base_low;
    (void)base_high;
    (void)span_hint;
    (void)common_offset;
    (void)notify_offset;
    (void)device_offset;
    (void)notify_multiplier;
    (void)flags;
    (void)token;
}

void virtio_net64_init(void) {}

u32 virtio_net64_found(void) { return 0u; }
u64 virtio_net64_bar_base(void) { return 0ull; }
u32 virtio_net64_mapped(void) { return 0u; }
u32 virtio_net64_common(void) { return 0u; }
u32 virtio_net64_notify(void) { return 0u; }
u32 virtio_net64_device_config(void) { return 0u; }
u32 virtio_net64_status_ack(void) { return 0u; }
u32 virtio_net64_status_driver(void) { return 0u; }
u32 virtio_net64_features_ok(void) { return 0u; }
u32 virtio_net64_driver_ok(void) { return 0u; }
u32 virtio_net64_mac_nonzero(void) { return 0u; }
const u8 *virtio_net64_mac(void) { return g_disabled_mac; }
u32 virtio_net64_rx_queue(void) { return 0u; }
u32 virtio_net64_tx_queue(void) { return 0u; }
u32 virtio_net64_rx_buffers(void) { return 0u; }
u32 virtio_net64_tx(void) { return 0u; }
u32 virtio_net64_rx(void) { return 0u; }
u32 virtio_net64_arp_reply(void) { return 0u; }
const u8 *virtio_net64_arp_mac(void) { return g_disabled_mac; }
u32 virtio_net64_arp_ip(void) { return 0u; }
u32 virtio_net64_dhcp_discover(void) { return 0u; }
u32 virtio_net64_dhcp_offer(void) { return 0u; }
u32 virtio_net64_dhcp_request(void) { return 0u; }
u32 virtio_net64_dhcp_ack(void) { return 0u; }
u32 virtio_net64_dhcp_ip(void) { return 0u; }
u32 virtio_net64_dhcp_gateway(void) { return 0u; }
u32 virtio_net64_dhcp_dns(void) { return 0u; }
u32 virtio_net64_dhcp_lease(void) { return 0u; }
u32 virtio_net64_dhcp_unavailable(void) { return 1u; }
u32 virtio_net64_dhcp_error(void) { return 0u; }
u32 virtio_net64_dns_query(void) { return 0u; }
u32 virtio_net64_dns_response(void) { return 0u; }
u32 virtio_net64_dns_rcode(void) { return 0u; }
u32 virtio_net64_dns_resolved(void) { return 0u; }
u32 virtio_net64_dns_unavailable(void) { return 1u; }
u32 virtio_net64_dns_error(void) { return 0u; }
u32 virtio_net64_http_connected(void) { return 0u; }
u32 virtio_net64_http_sent(void) { return 0u; }
u32 virtio_net64_http_status(void) { return 0u; }
u32 virtio_net64_http_response_bytes(void) { return 0u; }
u32 virtio_net64_http_unavailable(void) { return 1u; }
u32 virtio_net64_http_error(void) { return 0u; }
u32 virtio_net64_fs_authority(void) { return 0u; }
u32 virtio_net64_storage_authority(void) { return 0u; }
u32 virtio_net64_ambient_authority(void) { return 0u; }
u32 virtio_net64_unavailable(void) { return 1u; }
u32 virtio_net64_error(void) { return 0u; }

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
    (void)address;
    (void)vendor_device;
    (void)class_register;
    (void)bar0;
    (void)bar1;
    (void)base_low;
    (void)base_high;
    (void)span_hint;
    (void)flags;
    (void)token;
}

u32 e1000e64_init_backend(u8 *mac_out)
{
    u32 index;

    if (mac_out != 0)
    {
        for (index = 0u; index < 6u; ++index)
        {
            mac_out[index] = 0u;
        }
    }

    return 0u;
}

u32 e1000e64_transmit_frame(const u8 *frame, u32 frame_bytes)
{
    (void)frame;
    (void)frame_bytes;
    return 0u;
}

u32 e1000e64_poll_receive(u8 *dest, u32 capacity, u32 *frame_bytes)
{
    (void)dest;
    (void)capacity;
    if (frame_bytes != 0)
    {
        *frame_bytes = 0u;
    }
    return 0u;
}

u32 e1000e64_found(void) { return 0u; }
u64 e1000e64_bar_base(void) { return 0ull; }
u32 e1000e64_mapped(void) { return 0u; }
u32 e1000e64_reset(void) { return 0u; }
u32 e1000e64_rx_queue(void) { return 0u; }
u32 e1000e64_tx_queue(void) { return 0u; }
u32 e1000e64_rx_buffers(void) { return 0u; }
u32 e1000e64_tx(void) { return 0u; }
u32 e1000e64_rx(void) { return 0u; }
u32 e1000e64_link_up(void) { return 0u; }
u32 e1000e64_mac_nonzero(void) { return 0u; }
const u8 *e1000e64_mac(void) { return g_disabled_mac; }
u32 e1000e64_fs_authority(void) { return 0u; }
u32 e1000e64_storage_authority(void) { return 0u; }
u32 e1000e64_ambient_authority(void) { return 0u; }
u32 e1000e64_unavailable(void) { return 1u; }
u32 e1000e64_error(void) { return 0u; }
