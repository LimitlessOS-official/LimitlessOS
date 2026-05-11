#ifndef LIMITLESS_VIRTIO_NET_X64_H
#define LIMITLESS_VIRTIO_NET_X64_H

#include "types.h"

#define VIRTIO_NET64_MMIO_FLAG_PRESENT 0x00000001u
#define VIRTIO_NET64_MMIO_FLAG_MEMORY_BAR 0x00000002u
#define VIRTIO_NET64_MMIO_FLAG_64BIT_BAR 0x00000004u
#define VIRTIO_NET64_MMIO_FLAG_BASE_NONZERO 0x00000008u
#define VIRTIO_NET64_MMIO_FLAG_PAGE_ALIGNED 0x00000010u
#define VIRTIO_NET64_MMIO_FLAG_COMMON_CAP 0x00000020u
#define VIRTIO_NET64_MMIO_FLAG_NOTIFY_CAP 0x00000040u
#define VIRTIO_NET64_MMIO_FLAG_DEVICE_CAP 0x00000080u
#define VIRTIO_NET64_MMIO_FLAG_BROKER_PRIVATE 0x00000100u

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
    u32 token);
void virtio_net64_init(void);

u32 virtio_net64_found(void);
u64 virtio_net64_bar_base(void);
u32 virtio_net64_mapped(void);
u32 virtio_net64_common(void);
u32 virtio_net64_notify(void);
u32 virtio_net64_device_config(void);
u32 virtio_net64_status_ack(void);
u32 virtio_net64_status_driver(void);
u32 virtio_net64_features_ok(void);
u32 virtio_net64_driver_ok(void);
u32 virtio_net64_mac_nonzero(void);
const u8 *virtio_net64_mac(void);
u32 virtio_net64_rx_queue(void);
u32 virtio_net64_tx_queue(void);
u32 virtio_net64_rx_buffers(void);
u32 virtio_net64_tx(void);
u32 virtio_net64_rx(void);
u32 virtio_net64_arp_reply(void);
const u8 *virtio_net64_arp_mac(void);
u32 virtio_net64_arp_ip(void);
u32 virtio_net64_dhcp_discover(void);
u32 virtio_net64_dhcp_offer(void);
u32 virtio_net64_dhcp_request(void);
u32 virtio_net64_dhcp_ack(void);
u32 virtio_net64_dhcp_ip(void);
u32 virtio_net64_dhcp_gateway(void);
u32 virtio_net64_dhcp_dns(void);
u32 virtio_net64_dhcp_lease(void);
u32 virtio_net64_dhcp_unavailable(void);
u32 virtio_net64_dhcp_error(void);
u32 virtio_net64_dns_query(void);
u32 virtio_net64_dns_response(void);
u32 virtio_net64_dns_rcode(void);
u32 virtio_net64_dns_resolved(void);
u32 virtio_net64_dns_unavailable(void);
u32 virtio_net64_dns_error(void);
u32 virtio_net64_http_connected(void);
u32 virtio_net64_http_sent(void);
u32 virtio_net64_http_status(void);
u32 virtio_net64_http_response_bytes(void);
u32 virtio_net64_http_unavailable(void);
u32 virtio_net64_http_error(void);
u32 virtio_net64_fs_authority(void);
u32 virtio_net64_storage_authority(void);
u32 virtio_net64_ambient_authority(void);
u32 virtio_net64_unavailable(void);
u32 virtio_net64_error(void);

#endif
