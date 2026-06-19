#include "capability_x64.h"

#include "launch_x64.h"
#include "pit.h"
#include "principal_x64.h"
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
#include "persona_audit_x64.h"
#include "persona_x64.h"
#include "process_x64.h"
#endif
#include "services_x64.h"

/*
 * O.1 adds persona capability attenuation to the existing broker without
 * exposing the private process64_record layout. Persona-owned capabilities are
 * minted through capability64_grant_service_for_process(), carry the process
 * persona mask, and deny delegation into a different persona while recording a
 * capability-denied audit event. The scaffold checkpoint proves same-persona
 * delegation works, cross-persona transfer is denied, and legacy untagged
 * grants remain behavior-compatible.
 */

struct capability64_record
{
    u32 active;
    u32 handle;
    u32 endpoint_class;
    u32 endpoint_id;
    u32 rights;
    u32 parent_handle;
    u32 owner_id;
    u32 expiry_tick;
    u32 runtime_generation;
    u32 runtime_token;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 persona_type;
    u32 persona_mask;
#endif
};

enum
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    CAPABILITY64_TABLE_LIMIT = 64,
#else
    CAPABILITY64_TABLE_LIMIT = 32,
#endif
    CAPABILITY64_HANDLE_BASE = 0x6400u
};

static struct capability64_record g_capabilities[CAPABILITY64_TABLE_LIMIT];
static u32 g_next_handle = CAPABILITY64_HANDLE_BASE;
static u32 g_grant_count = 0u;
static u32 g_delegate_count = 0u;
static u32 g_route_count = 0u;
static u32 g_revoke_count = 0u;
static u32 g_cascade_revoke_count = 0u;
static u32 g_expiration_count = 0u;
static u32 g_owner_denial_count = 0u;
static u32 g_principal_denial_count = 0u;
static u32 g_runtime_stale_denial_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 g_persona_denial_count = 0u;
static u32 g_persona_transfer_denial_count = 0u;
#endif
static u32 g_denial_count = 0u;

static u32 capability64_normalize_owner(u32 owner_id)
{
    return (owner_id != 0u) ? owner_id : CAPABILITY64_OWNER_SYSTEM;
}

static int capability64_principal_is_valid(u32 owner_id)
{
    if (principal64_is_active(capability64_normalize_owner(owner_id)) != 0u)
    {
        return 1;
    }

    ++g_principal_denial_count;
    ++g_denial_count;
    return 0;
}

static u32 capability64_context_caller(u32 owner_context)
{
    u32 caller = (owner_context >> 16) & 0xFFFFu;

    return capability64_normalize_owner(caller);
}

static u32 capability64_context_recipient(u32 owner_context)
{
    u32 recipient = owner_context & 0xFFFFu;

    if (recipient != 0u)
    {
        return recipient;
    }

    return capability64_context_caller(owner_context);
}

static void capability64_clear(struct capability64_record *record)
{
    record->active = 0u;
    record->endpoint_class = 0u;
    record->endpoint_id = 0u;
    record->rights = 0u;
    record->parent_handle = 0u;
    record->owner_id = 0u;
    record->expiry_tick = 0u;
    record->runtime_generation = 0u;
    record->runtime_token = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    record->persona_type = CAPABILITY64_PERSONA_UNTAGGED;
    record->persona_mask = 0u;
#endif
}

static int capability64_record_expired(const struct capability64_record *record)
{
    if (record->expiry_tick == CAPABILITY64_LEASE_PERMANENT)
    {
        return 0;
    }

    return pit_get_ticks() >= record->expiry_tick;
}

static struct capability64_record *capability64_find_live(u32 handle)
{
    u32 index;

    for (index = 0u; index < CAPABILITY64_TABLE_LIMIT; ++index)
    {
        if ((g_capabilities[index].active != 0u) && (g_capabilities[index].handle == handle))
        {
            if (capability64_record_expired(&g_capabilities[index]))
            {
                capability64_clear(&g_capabilities[index]);
                ++g_expiration_count;
                return NULL;
            }

            return &g_capabilities[index];
        }
    }

    return NULL;
}

static struct capability64_record *capability64_find_free(void)
{
    u32 index;

    for (index = 0u; index < CAPABILITY64_TABLE_LIMIT; ++index)
    {
        if (g_capabilities[index].active == 0u)
        {
            return &g_capabilities[index];
        }
    }

    return NULL;
}

static int capability64_record_is_owned_by(
    const struct capability64_record *record,
    u32 caller_owner_id)
{
    if (record->owner_id == capability64_normalize_owner(caller_owner_id))
    {
        return 1;
    }

    ++g_owner_denial_count;
    ++g_denial_count;
    return 0;
}

static void capability64_bind_runtime(
    struct capability64_record *record,
    u32 endpoint_class)
{
    u32 manifest_index;

    if (record == NULL)
    {
        return;
    }

    record->runtime_generation = 0u;
    record->runtime_token = 0u;

    manifest_index = launch64_manifest_by_endpoint_class(endpoint_class);
    if (manifest_index == LAUNCH64_INVALID_MANIFEST)
    {
        return;
    }

    record->runtime_generation = launch64_manifest_runtime_generation(manifest_index);
    record->runtime_token = launch64_manifest_runtime_token(manifest_index);
}

static int capability64_record_runtime_is_current(struct capability64_record *record)
{
    u32 manifest_index;

    if ((record == NULL) || (record->runtime_token == 0u))
    {
        return 1;
    }

    manifest_index = launch64_manifest_by_endpoint_class(record->endpoint_class);
    if ((manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_generation(manifest_index) == record->runtime_generation)
        && (launch64_manifest_accepts_runtime_token(manifest_index, record->runtime_token) != 0u))
    {
        return 1;
    }

    capability64_clear(record);
    ++g_runtime_stale_denial_count;
    ++g_denial_count;
    return 0;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static u32 capability64_persona_pid_for_owner(u32 owner_id)
{
    return process64_pid_for_principal(capability64_normalize_owner(owner_id));
}

static void capability64_audit_persona_denial(
    u32 recipient_owner_id,
    u32 endpoint_class)
{
    u32 recipient_pid = capability64_persona_pid_for_owner(recipient_owner_id);

    if (recipient_pid == PROCESS64_INVALID_PID)
    {
        return;
    }

    (void)persona_audit64_record(
        recipient_pid,
        (u8)PERSONA_AUDIT64_EVENT_CAPABILITY_DENIED,
        (u16)endpoint_class,
        PERSONA_AUDIT64_RESULT_DENY,
        0ull);
}

static int capability64_record_allows_recipient_persona(
    const struct capability64_record *record,
    u32 recipient_owner_id)
{
    u32 recipient_pid;
    u32 recipient_mask;

    if ((record == NULL) || (record->persona_mask == 0u))
    {
        return 1;
    }

    recipient_pid = capability64_persona_pid_for_owner(recipient_owner_id);
    recipient_mask = (recipient_pid != PROCESS64_INVALID_PID)
        ? persona64_capability_mask(recipient_pid)
        : 0u;

    if ((recipient_mask != 0u) && ((record->persona_mask & recipient_mask) != 0u))
    {
        return 1;
    }

    ++g_persona_denial_count;
    ++g_persona_transfer_denial_count;
    ++g_denial_count;
    capability64_audit_persona_denial(recipient_owner_id, record->endpoint_class);
    return 0;
}
#endif

void capability64_init(void)
{
    u32 index;

    for (index = 0u; index < CAPABILITY64_TABLE_LIMIT; ++index)
    {
        g_capabilities[index].active = 0u;
        g_capabilities[index].handle = 0u;
        g_capabilities[index].endpoint_class = 0u;
        g_capabilities[index].endpoint_id = 0u;
        g_capabilities[index].rights = 0u;
        g_capabilities[index].parent_handle = 0u;
        g_capabilities[index].owner_id = 0u;
        g_capabilities[index].expiry_tick = 0u;
        g_capabilities[index].runtime_generation = 0u;
        g_capabilities[index].runtime_token = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        g_capabilities[index].persona_type = CAPABILITY64_PERSONA_UNTAGGED;
        g_capabilities[index].persona_mask = 0u;
#endif
    }

    g_next_handle = CAPABILITY64_HANDLE_BASE;
    g_grant_count = 0u;
    g_delegate_count = 0u;
    g_route_count = 0u;
    g_revoke_count = 0u;
    g_cascade_revoke_count = 0u;
    g_expiration_count = 0u;
    g_owner_denial_count = 0u;
    g_principal_denial_count = 0u;
    g_runtime_stale_denial_count = 0u;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    g_persona_denial_count = 0u;
    g_persona_transfer_denial_count = 0u;
#endif
    g_denial_count = 0u;
}

static u32 capability64_grant_service_tagged(
    u32 endpoint_class,
    u32 requested_rights,
    u32 owner_id
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    ,
    u32 persona_type,
    u32 persona_mask
#endif
    )
{
    u32 endpoint_id = services64_resolve_endpoint_class(endpoint_class);
    u32 allowed_rights = CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY;
    struct capability64_record *record;

    owner_id = capability64_normalize_owner(owner_id);
    if (!capability64_principal_is_valid(owner_id))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }

    if (endpoint_id == 0xFFFFFFFFu)
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    if (services64_endpoint_is_delegable(endpoint_id) != 0u)
    {
        allowed_rights |= CAPABILITY64_RIGHT_DELEGATE;
    }

    if (requested_rights == 0u)
    {
        requested_rights = CAPABILITY64_RIGHT_SEND;
    }

    if ((requested_rights & ~allowed_rights) != 0u)
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    record = capability64_find_free();
    if (record == NULL)
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    record->active = 1u;
    record->handle = g_next_handle++;
    record->endpoint_class = endpoint_class;
    record->endpoint_id = endpoint_id;
    record->rights = requested_rights;
    record->parent_handle = 0u;
    record->owner_id = owner_id;
    record->expiry_tick = CAPABILITY64_LEASE_PERMANENT;
    capability64_bind_runtime(record, endpoint_class);
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    record->persona_type = persona_type;
    record->persona_mask = persona_mask;
#endif
    ++g_grant_count;

    return record->handle;
}

u32 capability64_grant_service(u32 endpoint_class, u32 requested_rights, u32 owner_id)
{
    return capability64_grant_service_tagged(
        endpoint_class,
        requested_rights,
        owner_id
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
        ,
        CAPABILITY64_PERSONA_UNTAGGED,
        0u
#endif
        );
}

u32 capability64_grant_service_for_process(u32 endpoint_class, u32 requested_rights, u32 pid)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    u32 owner_id = process64_principal(pid);
    u32 persona_type = persona64_type(pid);
    u32 persona_mask = persona64_capability_mask(pid);

    if ((owner_id == 0u)
        || (persona_type >= PERSONA64_TYPE_COUNT)
        || (persona_mask == 0u))
    {
        ++g_persona_denial_count;
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    return capability64_grant_service_tagged(
        endpoint_class,
        requested_rights,
        owner_id,
        persona_type,
        persona_mask);
#else
    (void)endpoint_class;
    (void)requested_rights;
    (void)pid;
    ++g_denial_count;
    return CAPABILITY64_INVALID_HANDLE;
#endif
}

static u32 capability64_delegate_with_expiry(
    u32 source_handle,
    u32 requested_rights,
    u32 owner_context,
    u32 expiry_tick)
{
    struct capability64_record *source = capability64_find_live(source_handle);
    struct capability64_record *record;
    u32 caller_owner_id = capability64_context_caller(owner_context);
    u32 recipient_owner_id = capability64_context_recipient(owner_context);
    u32 delegated_rights;

    if (!capability64_principal_is_valid(caller_owner_id)
        || !capability64_principal_is_valid(recipient_owner_id))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }

    if (source == NULL)
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    if (!capability64_record_is_owned_by(source, caller_owner_id))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }

    if ((source->rights & CAPABILITY64_RIGHT_DELEGATE) == 0u)
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    if (requested_rights == 0u)
    {
        requested_rights = source->rights & ~CAPABILITY64_RIGHT_DELEGATE;
    }

    if ((requested_rights == 0u)
        || ((requested_rights & CAPABILITY64_RIGHT_DELEGATE) != 0u)
        || ((requested_rights & source->rights) != requested_rights))
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    delegated_rights = requested_rights & ~CAPABILITY64_RIGHT_DELEGATE;
    if (delegated_rights == 0u)
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    if (!capability64_record_allows_recipient_persona(source, recipient_owner_id))
    {
        return CAPABILITY64_INVALID_HANDLE;
    }
#endif

    record = capability64_find_free();
    if (record == NULL)
    {
        ++g_denial_count;
        return CAPABILITY64_INVALID_HANDLE;
    }

    record->active = 1u;
    record->handle = g_next_handle++;
    record->endpoint_class = source->endpoint_class;
    record->endpoint_id = source->endpoint_id;
    record->rights = delegated_rights;
    record->parent_handle = source->handle;
    record->owner_id = recipient_owner_id;
    record->expiry_tick = expiry_tick;
    record->runtime_generation = source->runtime_generation;
    record->runtime_token = source->runtime_token;
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    record->persona_type = source->persona_type;
    record->persona_mask = source->persona_mask;
#endif
    ++g_grant_count;
    ++g_delegate_count;

    return record->handle;
}

u32 capability64_delegate(u32 source_handle, u32 requested_rights, u32 owner_context)
{
    return capability64_delegate_with_expiry(
        source_handle,
        requested_rights,
        owner_context,
        pit_get_ticks() + CAPABILITY64_DELEGATE_LEASE_TICKS);
}

u32 capability64_delegate_persistent(u32 source_handle, u32 requested_rights, u32 owner_context)
{
    return capability64_delegate_with_expiry(
        source_handle,
        requested_rights,
        owner_context,
        CAPABILITY64_LEASE_PERMANENT);
}

u32 capability64_route(u32 handle, u32 required_rights, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0xFFFFFFFFu;
    }

    if (required_rights == 0u)
    {
        required_rights = CAPABILITY64_RIGHT_SEND;
    }

    if ((record == NULL) || ((record->rights & required_rights) != required_rights))
    {
        ++g_denial_count;
        return 0xFFFFFFFFu;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0xFFFFFFFFu;
    }

    if (!capability64_record_runtime_is_current(record))
    {
        return 0xFFFFFFFFu;
    }

    ++g_route_count;
    return record->endpoint_id;
}

u32 capability64_revoke(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);
    u32 index;

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0u;
    }

    capability64_clear(record);
    ++g_revoke_count;

    for (index = 0u; index < CAPABILITY64_TABLE_LIMIT; ++index)
    {
        if ((g_capabilities[index].active != 0u) && (g_capabilities[index].parent_handle == handle))
        {
            capability64_clear(&g_capabilities[index]);
            ++g_revoke_count;
            ++g_cascade_revoke_count;
        }
    }

    return 1u;
}

u32 capability64_target_endpoint(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0xFFFFFFFFu;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0xFFFFFFFFu;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0xFFFFFFFFu;
    }

    if (!capability64_record_runtime_is_current(record))
    {
        return 0xFFFFFFFFu;
    }

    return record->endpoint_id;
}

u32 capability64_rights(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0u;
    }

    return record->rights;
}

u32 capability64_parent(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0u;
    }

    return record->parent_handle;
}

u32 capability64_owner(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0u;
    }

    return record->owner_id;
}

u32 capability64_expiry_tick(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0u;
    }

    return record->expiry_tick;
}

u32 capability64_runtime_generation(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id)
        || !capability64_record_runtime_is_current(record))
    {
        return 0u;
    }

    return record->runtime_generation;
}

u32 capability64_runtime_token(u32 handle, u32 caller_owner_id)
{
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id)
        || !capability64_record_runtime_is_current(record))
    {
        return 0u;
    }

    return record->runtime_token;
}

u32 capability64_persona_tag(u32 handle, u32 caller_owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return CAPABILITY64_PERSONA_UNTAGGED;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return CAPABILITY64_PERSONA_UNTAGGED;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return CAPABILITY64_PERSONA_UNTAGGED;
    }

    return record->persona_type;
#else
    (void)handle;
    (void)caller_owner_id;
    return CAPABILITY64_PERSONA_UNTAGGED;
#endif
}

u32 capability64_persona_mask(u32 handle, u32 caller_owner_id)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    struct capability64_record *record = capability64_find_live(handle);

    if (!capability64_principal_is_valid(caller_owner_id))
    {
        return 0u;
    }

    if (record == NULL)
    {
        ++g_denial_count;
        return 0u;
    }

    if (!capability64_record_is_owned_by(record, caller_owner_id))
    {
        return 0u;
    }

    return record->persona_mask;
#else
    (void)handle;
    (void)caller_owner_id;
    return 0u;
#endif
}

u32 capability64_live_count(void)
{
    u32 index;
    u32 count = 0u;

    for (index = 0u; index < CAPABILITY64_TABLE_LIMIT; ++index)
    {
        if (g_capabilities[index].active != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 capability64_live_for_endpoint_class(u32 endpoint_class)
{
    u32 endpoint_id = services64_resolve_endpoint_class(endpoint_class);
    u32 index;
    u32 count = 0u;

    if (endpoint_id == 0xFFFFFFFFu)
    {
        return 0u;
    }

    for (index = 0u; index < CAPABILITY64_TABLE_LIMIT; ++index)
    {
        if (g_capabilities[index].active == 0u)
        {
            continue;
        }

        if (capability64_record_expired(&g_capabilities[index]))
        {
            capability64_clear(&g_capabilities[index]);
            ++g_expiration_count;
            continue;
        }

        if (g_capabilities[index].endpoint_id == endpoint_id)
        {
            ++count;
        }
    }

    return count;
}

u32 capability64_revoke_endpoint_class(u32 endpoint_class)
{
    u32 endpoint_id = services64_resolve_endpoint_class(endpoint_class);
    u32 index;
    u32 revoked = 0u;

    if (endpoint_id == 0xFFFFFFFFu)
    {
        return 0u;
    }

    for (index = 0u; index < CAPABILITY64_TABLE_LIMIT; ++index)
    {
        u32 handle;
        u32 child_index;

        if (g_capabilities[index].active == 0u)
        {
            continue;
        }

        if (capability64_record_expired(&g_capabilities[index]))
        {
            capability64_clear(&g_capabilities[index]);
            ++g_expiration_count;
            continue;
        }

        if (g_capabilities[index].endpoint_id != endpoint_id)
        {
            continue;
        }

        handle = g_capabilities[index].handle;
        capability64_clear(&g_capabilities[index]);
        ++g_revoke_count;
        ++revoked;

        for (child_index = 0u; child_index < CAPABILITY64_TABLE_LIMIT; ++child_index)
        {
            if ((g_capabilities[child_index].active != 0u)
                && (g_capabilities[child_index].parent_handle == handle))
            {
                capability64_clear(&g_capabilities[child_index]);
                ++g_revoke_count;
                ++g_cascade_revoke_count;
                ++revoked;
            }
        }
    }

    return revoked;
}

u32 capability64_grant_count(void)
{
    return g_grant_count;
}

u32 capability64_delegate_count(void)
{
    return g_delegate_count;
}

u32 capability64_route_count(void)
{
    return g_route_count;
}

u32 capability64_revoke_count(void)
{
    return g_revoke_count;
}

u32 capability64_cascade_revoke_count(void)
{
    return g_cascade_revoke_count;
}

u32 capability64_expiration_count(void)
{
    return g_expiration_count;
}

u32 capability64_owner_denial_count(void)
{
    return g_owner_denial_count;
}

u32 capability64_principal_denial_count(void)
{
    return g_principal_denial_count;
}

u32 capability64_runtime_stale_denial_count(void)
{
    return g_runtime_stale_denial_count;
}

u32 capability64_persona_denial_count(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_persona_denial_count;
#else
    return 0u;
#endif
}

u32 capability64_persona_transfer_denial_count(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    return g_persona_transfer_denial_count;
#else
    return 0u;
#endif
}

u32 capability64_denial_count(void)
{
    return g_denial_count;
}
