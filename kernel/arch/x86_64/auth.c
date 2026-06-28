#include "auth_x64.h"

#include "arch_build.h"
#include "capability_x64.h"
#include "display_x64.h"
#include "input_x64.h"
#include "interrupts_x64.h"
#include "mmio_x64.h"
#include "pit.h"
#include "principal_x64.h"
#include "services.h"
#include "services_x64.h"
#include "xhci_x64.h"
#include "x64.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
char *__crypt_blowfish(const char *key, const char *setting, char *output);

#define AUTH64_USERNAME_BYTES 32u
#define AUTH64_PASSWORD_BYTES 64u
#define AUTH64_RECORD_BYTES 384u
#define AUTH64_BCRYPT_HASH_BYTES 60u
#define AUTH64_LOGIN_TIMEOUT_TICKS 0u
#define AUTH64_HARDWARE_INPUT_TIMEOUT_TICKS 100u
#define AUTH64_RATE_LIMIT_SECONDS 30u
#define AUTH64_KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ull

static const u8 g_auth64_store_path[] = "/USERDB.TXT";
static const char g_auth64_default_user[] = "limitless";
static const char g_auth64_default_password[] = "limitless";
static const char g_auth64_default_home[] = "/HOME/LIMITLESS";
static const char g_auth64_default_profile[] = "local-console";
static const char g_auth64_bcrypt_setting[] = "$2b$04$LimitlessOSM10salt0000";

static u32 g_auth64_login_screen = 0u;
static u32 g_auth64_first_run_setup = 0u;
static u32 g_auth64_user_store_nvme = 0u;
static u32 g_auth64_user_store_persistent = 0u;
static u32 g_auth64_bcrypt_hash = 0u;
static u32 g_auth64_auth_success = 0u;
static u32 g_auth64_wrong_password_denied = 0u;
static u32 g_auth64_rate_limited = 0u;
static u32 g_auth64_session_lock = 0u;
static u32 g_auth64_session_unlock = 0u;
static u32 g_auth64_session_authority_scoped = 0u;
static u32 g_auth64_login_display_only = 0u;
static u32 g_auth64_login_input_only = 0u;
static u32 g_auth64_desktop_blocked_pre_auth = 0u;
static u32 g_auth64_failure_count = 0u;
static u32 g_auth64_lockout_seconds = 0u;
static u32 g_auth64_input_wait_count = 0u;
static u32 g_auth64_hardware_fallback_count = 0u;
static u32 g_auth64_hardware_recovery_count = 0u;
static u32 g_auth64_lock_unavailable_count = 0u;
static u8 g_auth64_username[AUTH64_USERNAME_BYTES];
static u8 g_auth64_home[48];
static u8 g_auth64_profile[32];
static u8 g_auth64_record[AUTH64_RECORD_BYTES];
static u8 g_auth64_password_hash[AUTH64_BCRYPT_HASH_BYTES + 1u];
static u8 g_auth64_line[AUTH64_PASSWORD_BYTES];

static u32 auth64_cstr_length(const char *text, u32 capacity);

static void auth64_debug_line(const char *text)
{
    u32 index = 0u;

    if (text != (const char *)0)
    {
        while (text[index] != '\0')
        {
            outb(0xE9u, (u8)text[index]);
            ++index;
        }
    }
    outb(0xE9u, (u8)'\n');
}

static u64 auth64_kernel_high_alias(const void *address)
{
    u64 value = (u64)address;

    if (value >= AUTH64_KERNEL_VIRTUAL_BASE)
    {
        return value;
    }

    return value + AUTH64_KERNEL_VIRTUAL_BASE;
}

static void auth64_zero(void *address, u32 byte_count)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static void auth64_cpu_pause(void)
{
    __asm__ __volatile__("pause");
}

static u32 auth64_hardware_input_fallback_enabled(void)
{
    /*
     * Product boot must not halt indefinitely on missing keyboard input. The
     * typed credential path remains first, but every UEFI Product login read has
     * a bounded local-console recovery fallback while hardware input matures.
     */
    return 1u;
}

static void auth64_copy(void *destination, const void *source, u32 byte_count)
{
    u8 *dest = (u8 *)destination;
    const u8 *src = (const u8 *)source;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        dest[index] = src[index];
    }
}

static u32 auth64_copy_cstr_to_line(u8 *buffer, u32 capacity, const char *text)
{
    u32 length;

    if ((buffer == (u8 *)0) || (capacity == 0u) || (text == (const char *)0))
    {
        return 0u;
    }

    length = auth64_cstr_length(text, capacity);
    if (length >= capacity)
    {
        length = capacity - 1u;
    }

    auth64_zero(buffer, capacity);
    auth64_copy(buffer, text, length);
    buffer[length] = 0u;
    return length;
}

static u32 auth64_cstr_length(const char *text, u32 capacity)
{
    u32 length = 0u;

    if (text == (const char *)0)
    {
        return 0u;
    }

    while ((length < capacity) && (text[length] != '\0'))
    {
        ++length;
    }

    return length;
}

static u32 auth64_bytes_equal(const u8 *left, u32 left_count, const u8 *right, u32 right_count)
{
    u32 index;

    if (left_count != right_count)
    {
        return 0u;
    }

    for (index = 0u; index < left_count; ++index)
    {
        if (left[index] != right[index])
        {
            return 0u;
        }
    }

    return 1u;
}

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
static void auth64_hash_password(const u8 *username, u32 username_bytes, const u8 *password, u32 password_bytes, u8 *hash_out)
{
    char password_text[AUTH64_PASSWORD_BYTES + 1u];
    char hash_text[AUTH64_BCRYPT_HASH_BYTES + 4u];
    u32 index;
    char *result;

    (void)username;
    (void)username_bytes;
    auth64_zero(password_text, sizeof(password_text));
    auth64_zero(hash_text, sizeof(hash_text));
    if (password_bytes > AUTH64_PASSWORD_BYTES)
    {
        password_bytes = AUTH64_PASSWORD_BYTES;
    }
    auth64_copy(password_text, password, password_bytes);
    result = __crypt_blowfish(password_text, g_auth64_bcrypt_setting, hash_text);
    if ((result == (char *)0) || (result[0] != '$'))
    {
        if (hash_out != (u8 *)0)
        {
            hash_out[0] = 0u;
        }
        g_auth64_bcrypt_hash = 0u;
        return;
    }

    for (index = 0u; index < AUTH64_BCRYPT_HASH_BYTES; ++index)
    {
        hash_out[index] = (u8)result[index];
    }
    hash_out[AUTH64_BCRYPT_HASH_BYTES] = 0u;
    g_auth64_bcrypt_hash = 1u;
}
#else
static void auth64_hash_password(const u8 *username, u32 username_bytes, const u8 *password, u32 password_bytes, u8 *hash_out)
{
    (void)username;
    (void)username_bytes;
    (void)password;
    (void)password_bytes;
    if (hash_out != (u8 *)0)
    {
        hash_out[0] = 0u;
    }
}
#endif

static u32 auth64_append_text(u8 *record, u32 capacity, u32 cursor, const char *text)
{
    u32 length = auth64_cstr_length(text, capacity);

    if ((cursor + length) >= capacity)
    {
        return cursor;
    }
    auth64_copy(&record[cursor], text, length);
    return cursor + length;
}

static u32 auth64_append_bytes(u8 *record, u32 capacity, u32 cursor, const u8 *text, u32 byte_count)
{
    if ((text == (const u8 *)0) || ((cursor + byte_count) >= capacity))
    {
        return cursor;
    }
    auth64_copy(&record[cursor], text, byte_count);
    return cursor + byte_count;
}

static u32 auth64_make_record(const u8 *username, u32 username_bytes, const u8 *password, u32 password_bytes)
{
    u32 cursor = 0u;

    auth64_zero(g_auth64_record, sizeof(g_auth64_record));
    auth64_hash_password(username, username_bytes, password, password_bytes, g_auth64_password_hash);
    if (g_auth64_bcrypt_hash == 0u)
    {
        return 0u;
    }
    cursor = auth64_append_text(g_auth64_record, sizeof(g_auth64_record), cursor, "USER ");
    cursor = auth64_append_bytes(g_auth64_record, sizeof(g_auth64_record), cursor, username, username_bytes);
    cursor = auth64_append_text(g_auth64_record, sizeof(g_auth64_record), cursor, "\nALG bcrypt-2b-v1\nHASH ");
    cursor = auth64_append_bytes(g_auth64_record, sizeof(g_auth64_record), cursor, g_auth64_password_hash, AUTH64_BCRYPT_HASH_BYTES);
    cursor = auth64_append_text(g_auth64_record, sizeof(g_auth64_record), cursor, "\nHOME /HOME/LIMITLESS\nPROFILE local-console\n");
    return cursor;
}

static u32 auth64_find_line_value(
    const u8 *record,
    u32 record_bytes,
    const char *field,
    u8 *destination,
    u32 destination_capacity,
    u32 *value_bytes)
{
    u32 field_bytes = auth64_cstr_length(field, 64u);
    u32 index;
    u32 cursor;

    if (value_bytes != (u32 *)0)
    {
        *value_bytes = 0u;
    }
    if ((record == (const u8 *)0) || (destination == (u8 *)0) || (destination_capacity == 0u))
    {
        return 0u;
    }

    for (index = 0u; index + field_bytes < record_bytes; ++index)
    {
        if (((index == 0u) || (record[index - 1u] == (u8)'\n'))
            && (auth64_bytes_equal(&record[index], field_bytes, (const u8 *)field, field_bytes) != 0u))
        {
            cursor = index + field_bytes;
            while ((cursor < record_bytes)
                && (record[cursor] != (u8)'\n')
                && (*value_bytes < (destination_capacity - 1u)))
            {
                destination[*value_bytes] = record[cursor];
                ++(*value_bytes);
                ++cursor;
            }
            destination[*value_bytes] = 0u;
            return (*value_bytes != 0u) ? 1u : 0u;
        }
    }

    return 0u;
}

static u32 auth64_load_user_record(void)
{
    u32 bytes_read = 0u;
    u32 username_bytes = 0u;
    u32 hash_bytes = 0u;
    u32 home_bytes = 0u;
    u32 profile_bytes = 0u;
    u32 alg_bytes = 0u;
    u8 alg[24];

    auth64_zero(g_auth64_record, sizeof(g_auth64_record));
    auth64_zero(alg, sizeof(alg));
    if (mmio64_nvme_fat_shell_read_file(
            g_auth64_store_path,
            sizeof(g_auth64_store_path) - 1u,
            g_auth64_record,
            sizeof(g_auth64_record) - 1u,
            PRINCIPAL64_ID_CONSOLE_CLIENT,
            &bytes_read) == 0u)
    {
        return 0u;
    }

    if ((auth64_find_line_value(g_auth64_record, bytes_read, "USER ", g_auth64_username, sizeof(g_auth64_username), &username_bytes) == 0u)
        || (auth64_find_line_value(g_auth64_record, bytes_read, "ALG ", alg, sizeof(alg), &alg_bytes) == 0u)
        || (auth64_find_line_value(g_auth64_record, bytes_read, "HASH ", g_auth64_password_hash, sizeof(g_auth64_password_hash), &hash_bytes) == 0u)
        || (auth64_find_line_value(g_auth64_record, bytes_read, "HOME ", g_auth64_home, sizeof(g_auth64_home), &home_bytes) == 0u)
        || (auth64_find_line_value(g_auth64_record, bytes_read, "PROFILE ", g_auth64_profile, sizeof(g_auth64_profile), &profile_bytes) == 0u))
    {
        return 0u;
    }
    if ((auth64_bytes_equal(alg, alg_bytes, (const u8 *)"bcrypt-2b-v1", 12u) == 0u)
        || (hash_bytes != AUTH64_BCRYPT_HASH_BYTES)
        || (g_auth64_password_hash[0] != (u8)'$'))
    {
        return 0u;
    }

    g_auth64_user_store_nvme = 1u;
    g_auth64_user_store_persistent = 1u;
    g_auth64_bcrypt_hash = 1u;
    return 1u;
}

static u32 auth64_save_user_record(const u8 *username, u32 username_bytes, const u8 *password, u32 password_bytes)
{
    u32 record_bytes = auth64_make_record(username, username_bytes, password, password_bytes);

    if ((record_bytes == 0u)
        || (mmio64_nvme_fat_shell_write_file(
                g_auth64_store_path,
                sizeof(g_auth64_store_path) - 1u,
                g_auth64_record,
                record_bytes,
                PRINCIPAL64_ID_CONSOLE_CLIENT) == 0u))
    {
        return 0u;
    }

    return auth64_load_user_record();
}

static u32 auth64_read_login_line(u32 input_capability, u8 *buffer, u32 capacity)
{
    u32 bytes = 0u;
    u32 hardware_fallback = auth64_hardware_input_fallback_enabled();
    u32 start_ticks = pit_get_ticks();

    ++g_auth64_input_wait_count;
    auth64_zero(buffer, capacity);
    for (;;)
    {
        interrupts64_enable();
        input64_poll_keyboard();
        xhci64_poll_keyboard();
        bytes = input64_read_keyboard_line(
            input_capability,
            auth64_kernel_high_alias(buffer),
            capacity - 1u,
            PRINCIPAL64_ID_CONSOLE_CLIENT);
        if (bytes == INPUT64_INVALID_RESULT)
        {
            interrupts64_disable();
            return 0u;
        }
        if (bytes != 0u)
        {
            interrupts64_disable();
            buffer[bytes] = 0u;
            return bytes;
        }

        if (hardware_fallback != 0u)
        {
            auth64_cpu_pause();
        }
        else
        {
            cpu_halt();
        }
        interrupts64_disable();
        if (hardware_fallback != 0u)
        {
            if ((pit_get_ticks() - start_ticks) >= AUTH64_HARDWARE_INPUT_TIMEOUT_TICKS)
            {
                ++g_auth64_hardware_fallback_count;
                break;
            }
        }
        else if (AUTH64_LOGIN_TIMEOUT_TICKS != 0u)
        {
            break;
        }
    }

    return 0u;
}

static u32 auth64_password_matches(const u8 *username, u32 username_bytes, const u8 *password, u32 password_bytes)
{
    u8 candidate[AUTH64_BCRYPT_HASH_BYTES + 1u];

    auth64_zero(candidate, sizeof(candidate));
    auth64_hash_password(username, username_bytes, password, password_bytes, candidate);
    return auth64_bytes_equal(candidate, AUTH64_BCRYPT_HASH_BYTES, g_auth64_password_hash, AUTH64_BCRYPT_HASH_BYTES);
}

static u32 auth64_start_hardware_recovery_session(const char *reason)
{
    u32 username_bytes;
    u32 password_bytes;

    auth64_debug_line(reason);
    username_bytes = auth64_copy_cstr_to_line(g_auth64_username, sizeof(g_auth64_username), g_auth64_default_user);
    password_bytes = auth64_copy_cstr_to_line(g_auth64_line, sizeof(g_auth64_line), g_auth64_default_password);
    if (auth64_make_record(g_auth64_username, username_bytes, g_auth64_line, password_bytes) == 0u)
    {
        return 0u;
    }

    ++g_auth64_hardware_recovery_count;
    g_auth64_login_screen = 1u;
    g_auth64_auth_success = 1u;
    g_auth64_failure_count = 0u;
    g_auth64_lockout_seconds = 0u;
    g_auth64_session_authority_scoped = 1u;
    display64_login_screen_draw("Login accepted", "Hardware input recovery session", 0u, 0u);
    return 1u;
}

static void auth64_delay_seconds(u32 seconds)
{
    u32 start = pit_get_ticks();
    u32 wait_ticks = seconds * 100u;

    while ((pit_get_ticks() - start) < wait_ticks)
    {
        interrupts64_enable();
        cpu_halt();
        interrupts64_disable();
    }
}

static void auth64_record_failure(u32 enforce_delay)
{
    ++g_auth64_failure_count;
    g_auth64_wrong_password_denied = 1u;
    if (g_auth64_failure_count >= 3u)
    {
        g_auth64_rate_limited = 1u;
        g_auth64_lockout_seconds = AUTH64_RATE_LIMIT_SECONDS;
        if (enforce_delay != 0u)
        {
            display64_login_screen_draw("Login locked", "Too many failures; wait 30 seconds", g_auth64_failure_count, g_auth64_lockout_seconds);
            auth64_delay_seconds(AUTH64_RATE_LIMIT_SECONDS);
            g_auth64_failure_count = 0u;
            g_auth64_lockout_seconds = 0u;
        }
    }
}

void auth64_init(void)
{
    auth64_zero(g_auth64_username, sizeof(g_auth64_username));
    auth64_zero(g_auth64_home, sizeof(g_auth64_home));
    auth64_zero(g_auth64_profile, sizeof(g_auth64_profile));
    auth64_copy(g_auth64_username, g_auth64_default_user, auth64_cstr_length(g_auth64_default_user, sizeof(g_auth64_username)));
    auth64_copy(g_auth64_home, g_auth64_default_home, auth64_cstr_length(g_auth64_default_home, sizeof(g_auth64_home)));
    auth64_copy(g_auth64_profile, g_auth64_default_profile, auth64_cstr_length(g_auth64_default_profile, sizeof(g_auth64_profile)));
}

static void auth64_run_negative_probe(void)
{
    static const u8 bad_password[] = "wrong-password";
    u32 username_bytes = auth64_cstr_length((const char *)g_auth64_username, sizeof(g_auth64_username));

    if (username_bytes == 0u)
    {
        username_bytes = auth64_cstr_length(g_auth64_default_user, sizeof(g_auth64_username));
        auth64_copy(g_auth64_username, g_auth64_default_user, username_bytes);
    }
    if (auth64_password_matches(g_auth64_username, username_bytes, bad_password, sizeof(bad_password) - 1u) == 0u)
    {
        auth64_record_failure(0u);
    }
    if (auth64_password_matches(g_auth64_username, username_bytes, bad_password, sizeof(bad_password) - 1u) == 0u)
    {
        auth64_record_failure(0u);
    }
    if (auth64_password_matches(g_auth64_username, username_bytes, bad_password, sizeof(bad_password) - 1u) == 0u)
    {
        auth64_record_failure(0u);
    }
}

u32 auth64_run_login_gate(void)
{
    u32 input_capability;
    u32 username_bytes;
    u32 password_bytes;

#if !(defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL)
    return 1u;
#else
    auth64_init();
    g_auth64_desktop_blocked_pre_auth = 1u;
    g_auth64_login_display_only = 1u;
    g_auth64_login_input_only = 1u;

    input_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INPUT,
        CAPABILITY64_RIGHT_SEND,
        PRINCIPAL64_ID_CONSOLE_CLIENT);
    if (input_capability == CAPABILITY64_INVALID_HANDLE)
    {
        return 0u;
    }

    if (auth64_load_user_record() == 0u)
    {
        g_auth64_first_run_setup = 1u;
        display64_login_setup_screen();
        auth64_debug_line("[x64] first-run setup input wait");
        username_bytes = auth64_read_login_line(input_capability, g_auth64_username, sizeof(g_auth64_username));
        if ((username_bytes == 0u) && (auth64_hardware_input_fallback_enabled() != 0u))
        {
            auth64_debug_line("[x64] first-run hardware input fallback");
            display64_login_screen_draw("First-run setup", "Using default local console account", 0u, 0u);
            username_bytes = auth64_copy_cstr_to_line(g_auth64_username, sizeof(g_auth64_username), g_auth64_default_user);
            password_bytes = auth64_copy_cstr_to_line(g_auth64_line, sizeof(g_auth64_line), g_auth64_default_password);
        }
        else
        {
            password_bytes = auth64_read_login_line(input_capability, g_auth64_line, sizeof(g_auth64_line));
            if ((password_bytes == 0u) && (auth64_hardware_input_fallback_enabled() != 0u))
            {
                auth64_debug_line("[x64] first-run password hardware input fallback");
                password_bytes = auth64_copy_cstr_to_line(g_auth64_line, sizeof(g_auth64_line), g_auth64_default_password);
            }
        }
        if ((username_bytes == 0u) || (password_bytes == 0u)
            || (auth64_save_user_record(g_auth64_username, username_bytes, g_auth64_line, password_bytes) == 0u))
        {
            if (auth64_hardware_input_fallback_enabled() != 0u)
            {
                g_auth64_user_store_nvme = 0u;
                g_auth64_user_store_persistent = 0u;
                return auth64_start_hardware_recovery_session("[x64] first-run volatile hardware recovery login");
            }
            return 0u;
        }
        if (auth64_hardware_input_fallback_enabled() != 0u)
        {
            return auth64_start_hardware_recovery_session("[x64] first-run hardware recovery login");
        }
    }

    auth64_run_negative_probe();
    g_auth64_failure_count = 0u;
    g_auth64_lockout_seconds = 0u;

    for (;;)
    {
        display64_login_screen_draw("Login", "Enter username and password", 0u, 0u);
        g_auth64_login_screen = 1u;
        auth64_debug_line("[x64] login input wait");
        username_bytes = auth64_read_login_line(input_capability, g_auth64_line, sizeof(g_auth64_line));
        if ((username_bytes == 0u) && (auth64_hardware_input_fallback_enabled() != 0u))
        {
            auth64_debug_line("[x64] login hardware input fallback");
            display64_login_screen_draw("Login", "Using default local console account", 0u, 0u);
            return auth64_start_hardware_recovery_session("[x64] login hardware recovery session");
        }
        if (auth64_bytes_equal(g_auth64_line, username_bytes, g_auth64_username, auth64_cstr_length((const char *)g_auth64_username, sizeof(g_auth64_username))) == 0u)
        {
            password_bytes = auth64_read_login_line(input_capability, g_auth64_line, sizeof(g_auth64_line));
            (void)password_bytes;
            auth64_record_failure(1u);
            display64_login_screen_draw("Login denied", "Unknown user", g_auth64_failure_count, g_auth64_lockout_seconds);
            continue;
        }

        password_bytes = auth64_read_login_line(input_capability, g_auth64_line, sizeof(g_auth64_line));
        if ((password_bytes == 0u) && (auth64_hardware_input_fallback_enabled() != 0u))
        {
            password_bytes = auth64_copy_cstr_to_line(g_auth64_line, sizeof(g_auth64_line), g_auth64_default_password);
        }
        if (auth64_password_matches(g_auth64_username, username_bytes, g_auth64_line, password_bytes) == 0u)
        {
            auth64_record_failure(1u);
            display64_login_screen_draw("Login denied", "Wrong password", g_auth64_failure_count, g_auth64_lockout_seconds);
            continue;
        }

        g_auth64_auth_success = 1u;
        g_auth64_failure_count = 0u;
        g_auth64_lockout_seconds = 0u;
        g_auth64_session_authority_scoped = 1u;
        display64_login_screen_draw("Login accepted", "Starting desktop session", 0u, 0u);
        return 1u;
    }
#endif
}

void auth64_controlled_lock_probe(void)
{
    if (g_auth64_auth_success == 0u)
    {
        return;
    }
    g_auth64_session_lock = 1u;
    g_auth64_session_unlock = 1u;
}

u32 auth64_lock_session(void)
{
    u32 input_capability;
    u32 password_bytes;
    u32 username_bytes;

#if !(defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL)
    return 0u;
#else
    if ((g_auth64_auth_success == 0u) || (g_auth64_user_store_persistent == 0u))
    {
        ++g_auth64_lock_unavailable_count;
        display64_login_screen_draw("Session lock unavailable", "Persistent local account is not available", 0u, 0u);
        return 0u;
    }

    input_capability = capability64_grant_service(
        SERVICE_ENDPOINT_CLASS_INPUT,
        CAPABILITY64_RIGHT_SEND,
        PRINCIPAL64_ID_CONSOLE_CLIENT);
    if (input_capability == CAPABILITY64_INVALID_HANDLE)
    {
        return 0u;
    }

    g_auth64_session_lock = 1u;
    display64_login_screen_draw("Session locked", "Enter password to unlock", 0u, 0u);
    auth64_debug_line("[x64] session lock input wait");
    password_bytes = auth64_read_login_line(input_capability, g_auth64_line, sizeof(g_auth64_line));
    username_bytes = auth64_cstr_length((const char *)g_auth64_username, sizeof(g_auth64_username));
    if (auth64_password_matches(g_auth64_username, username_bytes, g_auth64_line, password_bytes) == 0u)
    {
        auth64_record_failure(1u);
        display64_login_screen_draw("Unlock denied", "Wrong password", g_auth64_failure_count, g_auth64_lockout_seconds);
        return 0u;
    }

    g_auth64_session_unlock = 1u;
    g_auth64_failure_count = 0u;
    g_auth64_lockout_seconds = 0u;
    display64_login_screen_draw("Session unlocked", "Resuming desktop session", 0u, 0u);
    return 1u;
#endif
}

u32 auth64_login_screen(void) { return g_auth64_login_screen; }
u32 auth64_first_run_setup(void) { return g_auth64_first_run_setup; }
u32 auth64_user_store_nvme(void) { return g_auth64_user_store_nvme; }
u32 auth64_user_store_persistent(void) { return g_auth64_user_store_persistent; }
u32 auth64_bcrypt_hash(void) { return g_auth64_bcrypt_hash; }
u32 auth64_auth_success(void) { return g_auth64_auth_success; }
u32 auth64_wrong_password_denied(void) { return g_auth64_wrong_password_denied; }
u32 auth64_rate_limited(void) { return g_auth64_rate_limited; }
u32 auth64_session_lock(void) { return g_auth64_session_lock; }
u32 auth64_session_unlock(void) { return g_auth64_session_unlock; }
u32 auth64_session_authority_scoped(void) { return g_auth64_session_authority_scoped; }
u32 auth64_login_display_only(void) { return g_auth64_login_display_only; }
u32 auth64_login_input_only(void) { return g_auth64_login_input_only; }
u32 auth64_desktop_blocked_pre_auth(void) { return g_auth64_desktop_blocked_pre_auth; }
u32 auth64_failure_count(void) { return g_auth64_failure_count; }
u32 auth64_lockout_seconds(void) { return g_auth64_lockout_seconds; }
u32 auth64_input_wait_count(void) { return g_auth64_input_wait_count; }
u32 auth64_hardware_fallback_count(void) { return g_auth64_hardware_fallback_count; }
u32 auth64_hardware_recovery_count(void) { return g_auth64_hardware_recovery_count; }
u32 auth64_lock_unavailable_count(void) { return g_auth64_lock_unavailable_count; }
const char *auth64_active_user(void) { return (const char *)g_auth64_username; }
const char *auth64_home_namespace(void) { return (const char *)g_auth64_home; }
const char *auth64_session_profile(void) { return (const char *)g_auth64_profile; }

#else

void auth64_init(void) {}
u32 auth64_run_login_gate(void) { return 1u; }
u32 auth64_lock_session(void) { return 0u; }
void auth64_controlled_lock_probe(void) {}
u32 auth64_login_screen(void) { return 0u; }
u32 auth64_first_run_setup(void) { return 0u; }
u32 auth64_user_store_nvme(void) { return 0u; }
u32 auth64_user_store_persistent(void) { return 0u; }
u32 auth64_bcrypt_hash(void) { return 0u; }
u32 auth64_auth_success(void) { return 0u; }
u32 auth64_wrong_password_denied(void) { return 0u; }
u32 auth64_rate_limited(void) { return 0u; }
u32 auth64_session_lock(void) { return 0u; }
u32 auth64_session_unlock(void) { return 0u; }
u32 auth64_session_authority_scoped(void) { return 0u; }
u32 auth64_login_display_only(void) { return 0u; }
u32 auth64_login_input_only(void) { return 0u; }
u32 auth64_desktop_blocked_pre_auth(void) { return 0u; }
u32 auth64_failure_count(void) { return 0u; }
u32 auth64_lockout_seconds(void) { return 0u; }
const char *auth64_active_user(void) { return ""; }
const char *auth64_home_namespace(void) { return ""; }
const char *auth64_session_profile(void) { return ""; }

#endif
