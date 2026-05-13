#ifndef LIMITLESS_AUTH_X64_H
#define LIMITLESS_AUTH_X64_H

#include "types.h"

void auth64_init(void);
u32 auth64_run_login_gate(void);
u32 auth64_lock_session(void);
void auth64_controlled_lock_probe(void);

u32 auth64_login_screen(void);
u32 auth64_first_run_setup(void);
u32 auth64_user_store_nvme(void);
u32 auth64_user_store_persistent(void);
u32 auth64_bcrypt_hash(void);
u32 auth64_auth_success(void);
u32 auth64_wrong_password_denied(void);
u32 auth64_rate_limited(void);
u32 auth64_session_lock(void);
u32 auth64_session_unlock(void);
u32 auth64_session_authority_scoped(void);
u32 auth64_login_display_only(void);
u32 auth64_login_input_only(void);
u32 auth64_desktop_blocked_pre_auth(void);
u32 auth64_failure_count(void);
u32 auth64_lockout_seconds(void);
const char *auth64_active_user(void);
const char *auth64_home_namespace(void);
const char *auth64_session_profile(void);

#endif
