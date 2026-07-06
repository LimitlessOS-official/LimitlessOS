#ifndef LIMITLESS_HARDWARE_REGISTRY_X64_H
#define LIMITLESS_HARDWARE_REGISTRY_X64_H

#include "types.h"

#define HARDWARE64_REGISTRY_MAX_DEVICES 32u

#define HARDWARE64_CLASS_PLATFORM 1u
#define HARDWARE64_CLASS_DISPLAY 2u
#define HARDWARE64_CLASS_INPUT 3u
#define HARDWARE64_CLASS_STORAGE 4u
#define HARDWARE64_CLASS_USB 5u
#define HARDWARE64_CLASS_NETWORK 6u

#define HARDWARE64_BINDING_NONE 0u
#define HARDWARE64_BINDING_CANDIDATE 1u
#define HARDWARE64_BINDING_BOUND 2u
#define HARDWARE64_BINDING_DEFERRED 3u
#define HARDWARE64_BINDING_UNSUPPORTED 4u
#define HARDWARE64_BINDING_FAILED 5u

void hardware64_registry_refresh(u32 hardware_capability_handle, u32 owner_id);
u32 hardware64_registry_refresh_count(void);
u32 hardware64_registry_limit(void);
u32 hardware64_registry_count(void);
u32 hardware64_registry_overflow_count(void);
u32 hardware64_registry_token(void);
u32 hardware64_registry_pci_device_count(void);
u32 hardware64_registry_pci_query_denial_count(void);
u32 hardware64_registry_acpi_table_count(void);
u32 hardware64_registry_display_device_count(void);
u32 hardware64_registry_input_device_count(void);
u32 hardware64_registry_storage_device_count(void);
u32 hardware64_registry_usb_controller_count(void);
u32 hardware64_registry_network_device_count(void);
u32 hardware64_registry_driver_bound_count(void);
u32 hardware64_registry_driver_candidate_count(void);
u32 hardware64_registry_driver_deferred_count(void);
u32 hardware64_registry_driver_unsupported_count(void);
u32 hardware64_registry_driver_failed_count(void);
u32 hardware64_registry_record_active(u32 index);
u32 hardware64_registry_record_class(u32 index);
u32 hardware64_registry_record_subclass(u32 index);
u32 hardware64_registry_record_binding(u32 index);
u32 hardware64_registry_record_source(u32 index);
u32 hardware64_registry_record_address(u32 index);
u32 hardware64_registry_record_flags(u32 index);
u32 hardware64_registry_record_token(u32 index);

#endif
