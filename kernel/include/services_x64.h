#ifndef LIMITLESS_SERVICES_X64_H
#define LIMITLESS_SERVICES_X64_H

#include "types.h"

void services64_init(void);
u32 services64_count(void);
u32 services64_resolve_endpoint_class(u32 endpoint_class);
const char *services64_endpoint_name(u32 endpoint_id);
u32 services64_capabilities_for_endpoint(u32 endpoint_id);
u32 services64_endpoint_is_delegable(u32 endpoint_id);
u32 services64_package_version(void);
u32 services64_package_signer_count(void);
u32 services64_package_manifest_count(void);
u32 services64_package_payload_count(void);
u32 services64_package_checksum(void);
u32 services64_package_valid(void);

#endif
