#ifndef LIMITLESS_CAPABILITY_X64_H
#define LIMITLESS_CAPABILITY_X64_H

#include "types.h"
#include "principal_x64.h"

#define CAPABILITY64_INVALID_HANDLE 0xFFFFFFFFu
#define CAPABILITY64_RIGHT_SEND 0x00000001u
#define CAPABILITY64_RIGHT_DELEGATE 0x00000002u
#define CAPABILITY64_RIGHT_QUERY 0x00000004u
#define CAPABILITY64_OWNER_SYSTEM PRINCIPAL64_ID_SYSTEM
#define CAPABILITY64_OWNER_POLICY_CLIENT PRINCIPAL64_ID_POLICY_CLIENT
#define CAPABILITY64_OWNER_POLICY_WORKER PRINCIPAL64_ID_POLICY_WORKER
#define CAPABILITY64_OWNER_CONSOLE_CLIENT PRINCIPAL64_ID_CONSOLE_CLIENT
#define CAPABILITY64_OWNER_CONSOLE_WORKER PRINCIPAL64_ID_CONSOLE_WORKER
#define CAPABILITY64_LEASE_PERMANENT 0xFFFFFFFFu
#define CAPABILITY64_DELEGATE_LEASE_TICKS 2u
#define CAPABILITY64_CONTEXT(caller, recipient) \
    ((((caller) & 0xFFFFu) << 16) | ((recipient) & 0xFFFFu))

void capability64_init(void);
u32 capability64_grant_service(u32 endpoint_class, u32 requested_rights, u32 owner_id);
u32 capability64_delegate(u32 source_handle, u32 requested_rights, u32 owner_context);
u32 capability64_delegate_persistent(u32 source_handle, u32 requested_rights, u32 owner_context);
u32 capability64_route(u32 handle, u32 required_rights, u32 caller_owner_id);
u32 capability64_revoke(u32 handle, u32 caller_owner_id);
u32 capability64_target_endpoint(u32 handle, u32 caller_owner_id);
u32 capability64_rights(u32 handle, u32 caller_owner_id);
u32 capability64_parent(u32 handle, u32 caller_owner_id);
u32 capability64_owner(u32 handle, u32 caller_owner_id);
u32 capability64_expiry_tick(u32 handle, u32 caller_owner_id);
u32 capability64_runtime_generation(u32 handle, u32 caller_owner_id);
u32 capability64_runtime_token(u32 handle, u32 caller_owner_id);
u32 capability64_live_count(void);
u32 capability64_live_for_endpoint_class(u32 endpoint_class);
u32 capability64_revoke_endpoint_class(u32 endpoint_class);
u32 capability64_grant_count(void);
u32 capability64_delegate_count(void);
u32 capability64_route_count(void);
u32 capability64_revoke_count(void);
u32 capability64_cascade_revoke_count(void);
u32 capability64_expiration_count(void);
u32 capability64_owner_denial_count(void);
u32 capability64_principal_denial_count(void);
u32 capability64_runtime_stale_denial_count(void);
u32 capability64_denial_count(void);

#endif
