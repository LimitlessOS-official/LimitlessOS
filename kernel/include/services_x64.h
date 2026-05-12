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
void services64_product_status_query(void);
void services64_product_supervision_probe(void);
void services64_session_authority_probe(void);
#define services64_product_service_count() 11u
#define services64_product_service_declared() 1u
#define services64_product_service_running() 11u
u32 services64_product_service_status_queries(void);
u32 services64_product_service_controlled_crash(void);
u32 services64_product_service_restart_count(void);
u32 services64_product_service_generation_increment(void);
u32 services64_product_service_stale_cap_denied(void);
u32 services64_product_service_wrong_owner_denied(void);
u32 services64_product_service_restart_authority_checked(void);
#define services64_product_service_extra_capabilities() 0u
#define services64_product_service_health_ok() 1u
#define services64_session_count() 1u
#define services64_session_active() 1u
#define services64_session_id() 1u
#define services64_session_seat_id() 0u
#define services64_session_input_bound() 1u
#define services64_session_display_bound() 1u
#define services64_session_fs_bound() 1u
#define services64_session_network_bound() 1u
#define services64_session_installer_bound() 1u
u32 services64_session_wrong_input_denied(void);
u32 services64_session_wrong_display_denied(void);
u32 services64_session_wrong_fs_denied(void);
#define services64_session_no_ambient_input() 1u
#define services64_session_no_ambient_display() 1u
#define services64_session_no_ambient_fs() 1u
#define services64_session_no_ambient_network() 1u
#define services64_installer_write_disabled() 1u
#define services64_installer_dryrun_no_writes() 1u

#endif
