#ifndef LIMITLESS_PACKAGE_SIGNING_X64_H
#define LIMITLESS_PACKAGE_SIGNING_X64_H

#include "types.h"

#ifdef LIMITLESS_X64_UEFI_KERNEL
void package_signing64_init(void);
u32 package_signing64_verify_archive(void);
u32 package_signing64_verify_payload(u32 slot, const void *payload, u32 payload_size, u32 payload_checksum);
u32 package_signing64_signed(void);
u32 package_signing64_verified(void);
u32 package_signing64_invalid_denied(void);
u32 package_signing64_missing_sig_denied(void);
u32 package_signing64_checksum_mismatch_denied(void);
u32 package_signing64_wrong_owner_denied(void);
u32 package_signing64_stale_token_denied(void);
u32 package_signing64_install_scoped(void);
u32 package_signing64_update_check(void);
u32 package_signing64_update_index_verified(void);
u32 package_signing64_update_rollback_denied(void);
u32 package_signing64_update_no_ambient(void);
#else
static inline void package_signing64_init(void) {}
static inline u32 package_signing64_verify_archive(void) { return 1u; }
static inline u32 package_signing64_verify_payload(u32 slot, const void *payload, u32 payload_size, u32 payload_checksum)
{
    (void)slot;
    (void)payload;
    (void)payload_size;
    (void)payload_checksum;
    return 1u;
}
static inline u32 package_signing64_signed(void) { return 0u; }
static inline u32 package_signing64_verified(void) { return 0u; }
static inline u32 package_signing64_invalid_denied(void) { return 0u; }
static inline u32 package_signing64_missing_sig_denied(void) { return 0u; }
static inline u32 package_signing64_checksum_mismatch_denied(void) { return 0u; }
static inline u32 package_signing64_wrong_owner_denied(void) { return 0u; }
static inline u32 package_signing64_stale_token_denied(void) { return 0u; }
static inline u32 package_signing64_install_scoped(void) { return 0u; }
static inline u32 package_signing64_update_check(void) { return 0u; }
static inline u32 package_signing64_update_index_verified(void) { return 0u; }
static inline u32 package_signing64_update_rollback_denied(void) { return 0u; }
static inline u32 package_signing64_update_no_ambient(void) { return 0u; }
#endif

#endif
