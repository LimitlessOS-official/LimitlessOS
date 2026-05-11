#ifndef LIMITLESS_PACKAGE_STORE_H
#define LIMITLESS_PACKAGE_STORE_H

#include "types.h"

struct package_store_signer_record
{
    u32 id;
    const char *name;
    u32 verification_token;
};

struct package_store_manifest_record
{
    u32 source_slot;
    u32 package_id;
    const char *package_name;
    u32 package_version;
    u32 signer_id;
    u32 signature_token;
    u32 trust_flags;
    u32 launch_authority_mask;
    u32 max_instances;
    u32 expected_image_size;
    u32 expected_image_checksum;
    u32 executable_id;
    const char *name;
    const char *process_name;
    const char *profile_name;
    const char *peer_endpoint_name;
    const char *policy_endpoint_name;
    u32 allowed_endpoint_role_mask;
    u32 allowed_service_class_mask;
    u32 scheduler_class;
    u32 scheduler_weight;
    u32 scheduler_latency_target_ticks;
    u32 scheduler_io_wakeup_deadline_ticks;
    u32 capability_admission_limit;
    u32 launch_role;
    u32 payload_slot;
};

void package_store_init(void);
int package_store_ready(void);
u32 package_store_signer_count(void);
u32 package_store_manifest_count(void);
int package_store_read_signer(u32 index, struct package_store_signer_record *out_record);
int package_store_read_manifest(u32 index, struct package_store_manifest_record *out_record);
int package_store_read_payload(u32 payload_slot, const u8 **start_out, const u8 **end_out);

#endif
