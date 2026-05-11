#ifndef LIMITLESS_E1000E_X64_H
#define LIMITLESS_E1000E_X64_H

#include "types.h"

#define E1000E64_MMIO_FLAG_PRESENT 0x00000001u
#define E1000E64_MMIO_FLAG_MEMORY_BAR 0x00000002u
#define E1000E64_MMIO_FLAG_64BIT_BAR 0x00000004u
#define E1000E64_MMIO_FLAG_BASE_NONZERO 0x00000008u
#define E1000E64_MMIO_FLAG_PAGE_ALIGNED 0x00000010u
#define E1000E64_MMIO_FLAG_BROKER_PRIVATE 0x00000020u

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
    u32 token);
u32 e1000e64_init_backend(u8 *mac_out);
u32 e1000e64_transmit_frame(const u8 *frame, u32 frame_bytes);
u32 e1000e64_poll_receive(u8 *dest, u32 capacity, u32 *frame_bytes);

u32 e1000e64_found(void);
u64 e1000e64_bar_base(void);
u32 e1000e64_mapped(void);
u32 e1000e64_reset(void);
u32 e1000e64_rx_queue(void);
u32 e1000e64_tx_queue(void);
u32 e1000e64_rx_buffers(void);
u32 e1000e64_tx(void);
u32 e1000e64_rx(void);
u32 e1000e64_link_up(void);
u32 e1000e64_mac_nonzero(void);
const u8 *e1000e64_mac(void);
u32 e1000e64_fs_authority(void);
u32 e1000e64_storage_authority(void);
u32 e1000e64_ambient_authority(void);
u32 e1000e64_unavailable(void);
u32 e1000e64_error(void);

#endif
