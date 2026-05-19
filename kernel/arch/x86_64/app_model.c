#include "app_model_x64.h"

#include "capability_x64.h"
#include "launch_x64.h"
#include "principal_x64.h"
#include "services.h"

#define APP64_DESCRIPTOR_AUTH_NETWORK 0x00000020u
#define APP64_DESCRIPTOR_AUTH_CONSOLE 0x00000040u
#define APP64_NETHELLO_EXECUTABLE_ID 20u
#define APP64_NATIVE_MAPPED_BYTES 4096u
#define APP64_NATIVE_NAME_BYTES 16u

struct app_model64_native_record
{
    u32 state;
    u32 flags;
    u32 token;
    u32 owner_id;
    u32 descriptor_bytes;
    u32 binary_bytes;
    u32 binary_checksum;
    u32 expected_checksum;
    u32 mapped_bytes;
    u32 entry_rip;
    u32 entry_rsp;
    u32 entry_selectors;
    u32 entry_rflags;
    u32 map_token;
    u32 network_capability;
    u32 socket_handle;
    u32 recv_bytes;
    u32 exit_result;
    u32 exit_aux;
    u32 name_token;
    u32 executable_id;
    u32 authority_mask;
    u32 argument_policy;
    u32 launch_binding;
    u32 capability_mask;
    u32 payload_slot;
    u32 entry_result;
    u32 success_result;
    u32 binary_path_verified;
    u8 name[APP64_NATIVE_NAME_BYTES];
    u32 name_bytes;
};

static struct app_model64_native_record g_native_app;
static u32 g_app_model64_active = 0u;
static u32 g_app_model64_active_owner = 0u;

static u32 app_model64_ascii_lower(u32 ch)
{
    if ((ch >= (u32)'A') && (ch <= (u32)'Z'))
    {
        return ch + ((u32)'a' - (u32)'A');
    }

    return ch;
}

static u32 app_model64_mix_token(u32 token, u32 value)
{
    u32 index;

    for (index = 0u; index < 4u; ++index)
    {
        token ^= (value >> (index * 8u)) & 0xFFu;
        token *= 16777619u;
    }

    return token;
}

static u32 app_model64_hash_bytes(const u8 *bytes, u32 byte_count)
{
    u32 token = 2166136261u;
    u32 index;

    if (bytes == (const u8 *)0)
    {
        return 0u;
    }

    for (index = 0u; index < byte_count; ++index)
    {
        token ^= bytes[index];
        token *= 16777619u;
    }

    return (token != 0u) ? token : 1u;
}

static u32 app_model64_make_token(void)
{
    u32 token = 2166136261u;

    token = app_model64_mix_token(token, g_native_app.state);
    token = app_model64_mix_token(token, g_native_app.flags);
    token = app_model64_mix_token(token, g_native_app.owner_id);
    token = app_model64_mix_token(token, g_native_app.descriptor_bytes);
    token = app_model64_mix_token(token, g_native_app.binary_bytes);
    token = app_model64_mix_token(token, g_native_app.binary_checksum);
    token = app_model64_mix_token(token, g_native_app.map_token);
    token = app_model64_mix_token(token, g_native_app.name_token);
    token = app_model64_mix_token(token, g_native_app.executable_id);
    token = app_model64_mix_token(token, g_native_app.authority_mask);
    token = app_model64_mix_token(token, g_native_app.capability_mask);
    token = app_model64_mix_token(token, g_native_app.payload_slot);
    token = app_model64_mix_token(token, g_native_app.exit_result);
    token = app_model64_mix_token(token, g_native_app.exit_aux);
    return (token != 0u) ? token : 1u;
}

static void app_model64_reset_native(void)
{
    u32 index;

    g_native_app.state = APP64_NETHELLO_STATE_UNREQUESTED;
    g_native_app.flags = 0u;
    g_native_app.token = 0u;
    g_native_app.owner_id = 0u;
    g_native_app.descriptor_bytes = 0u;
    g_native_app.binary_bytes = 0u;
    g_native_app.binary_checksum = 0u;
    g_native_app.expected_checksum = 0u;
    g_native_app.mapped_bytes = 0u;
    g_native_app.entry_rip = 0u;
    g_native_app.entry_rsp = 0u;
    g_native_app.entry_selectors = 0u;
    g_native_app.entry_rflags = 0u;
    g_native_app.map_token = 0u;
    g_native_app.network_capability = CAPABILITY64_INVALID_HANDLE;
    g_native_app.socket_handle = 0u;
    g_native_app.recv_bytes = 0u;
    g_native_app.exit_result = 0u;
    g_native_app.exit_aux = 0u;
    g_native_app.name_token = 0u;
    g_native_app.executable_id = 0u;
    g_native_app.authority_mask = 0u;
    g_native_app.argument_policy = 0u;
    g_native_app.launch_binding = 0u;
    g_native_app.capability_mask = 0u;
    g_native_app.payload_slot = 0u;
    g_native_app.entry_result = 0u;
    g_native_app.success_result = 0u;
    g_native_app.binary_path_verified = 0u;
    g_native_app.name_bytes = 0u;
    for (index = 0u; index < APP64_NATIVE_NAME_BYTES; ++index)
    {
        g_native_app.name[index] = 0u;
    }
}

static void app_model64_mark_native_unavailable(u32 payload_slot)
{
    app_model64_reset_native();
    g_native_app.state = APP64_NETHELLO_STATE_UNAVAILABLE;
    g_native_app.flags = APP64_NETHELLO_FLAG_REQUESTED
        | APP64_NETHELLO_FLAG_UNAVAILABLE;
    g_native_app.payload_slot = payload_slot;
    g_native_app.expected_checksum =
        launch64_payload_checksum_by_slot(payload_slot);
    g_native_app.token = APP64_NETHELLO_FLAG_UNAVAILABLE;
}

static u32 app_model64_line_decimal(
    const u8 *descriptor,
    u32 descriptor_bytes,
    u32 *offset,
    u32 *value_out)
{
    u32 value = 0u;
    u32 cursor;
    u32 digit_count = 0u;

    if ((descriptor == (const u8 *)0)
        || (offset == (u32 *)0)
        || (value_out == (u32 *)0)
        || (*offset >= descriptor_bytes))
    {
        return 0u;
    }

    cursor = *offset;
    while (cursor < descriptor_bytes)
    {
        u8 ch = descriptor[cursor];

        if ((ch >= (u8)'0') && (ch <= (u8)'9'))
        {
            value = (value * 10u) + (u32)(ch - (u8)'0');
            ++digit_count;
            ++cursor;
            continue;
        }

        if (ch == (u8)'\r')
        {
            ++cursor;
            continue;
        }

        if (ch == (u8)'\n')
        {
            ++cursor;
            break;
        }

        return 0u;
    }

    if (digit_count == 0u)
    {
        return 0u;
    }

    *offset = cursor;
    *value_out = value;
    return 1u;
}

static u32 app_model64_line_span(
    const u8 *descriptor,
    u32 descriptor_bytes,
    u32 *offset,
    u32 *start_out,
    u32 *length_out)
{
    u32 cursor;
    u32 start;
    u32 length;

    if ((descriptor == (const u8 *)0)
        || (offset == (u32 *)0)
        || (start_out == (u32 *)0)
        || (length_out == (u32 *)0)
        || (*offset >= descriptor_bytes))
    {
        return 0u;
    }

    start = *offset;
    cursor = start;
    while ((cursor < descriptor_bytes)
        && (descriptor[cursor] != (u8)'\n')
        && (descriptor[cursor] != (u8)'\r'))
    {
        ++cursor;
    }

    length = cursor - start;
    while ((cursor < descriptor_bytes)
        && ((descriptor[cursor] == (u8)'\n')
            || (descriptor[cursor] == (u8)'\r')))
    {
        ++cursor;
    }

    *offset = cursor;
    *start_out = start;
    *length_out = length;
    return (length != 0u) ? 1u : 0u;
}

static u32 app_model64_span_equals(
    const u8 *descriptor,
    u32 start,
    u32 length,
    const u8 *text,
    u32 text_length)
{
    u32 index;

    if ((descriptor == (const u8 *)0) || (text == (const u8 *)0)
        || (length != text_length))
    {
        return 0u;
    }

    for (index = 0u; index < length; ++index)
    {
        if (app_model64_ascii_lower(descriptor[start + index])
            != app_model64_ascii_lower(text[index]))
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 app_model64_line_value(
    const u8 *descriptor,
    u32 line_start,
    u32 line_length,
    const char *key,
    u32 *value_start_out,
    u32 *value_length_out)
{
    u32 key_length;
    u32 index;

    if ((descriptor == (const u8 *)0)
        || (key == 0)
        || (value_start_out == (u32 *)0)
        || (value_length_out == (u32 *)0))
    {
        return 0u;
    }

    key_length = 0u;
    while (key[key_length] != '\0')
    {
        ++key_length;
    }

    if ((key_length + 1u) > line_length)
    {
        return 0u;
    }

    for (index = 0u; index < key_length; ++index)
    {
        if (app_model64_ascii_lower(descriptor[line_start + index])
            != app_model64_ascii_lower((u32)key[index]))
        {
            return 0u;
        }
    }
    if (descriptor[line_start + key_length] != (u8)'=')
    {
        return 0u;
    }

    *value_start_out = line_start + key_length + 1u;
    *value_length_out = line_length - key_length - 1u;
    return (*value_length_out != 0u) ? 1u : 0u;
}

static u32 app_model64_parse_u32_span(
    const u8 *descriptor,
    u32 start,
    u32 length,
    u32 *value_out)
{
    u32 value = 0u;
    u32 index = 0u;
    u32 base = 10u;

    if ((descriptor == (const u8 *)0)
        || (value_out == (u32 *)0)
        || (length == 0u))
    {
        return 0u;
    }

    if ((length > 2u)
        && (descriptor[start] == (u8)'0')
        && ((descriptor[start + 1u] == (u8)'x')
            || (descriptor[start + 1u] == (u8)'X')))
    {
        base = 16u;
        index = 2u;
    }

    if (index >= length)
    {
        return 0u;
    }

    while (index < length)
    {
        u32 digit;
        u32 ch = descriptor[start + index];

        if ((ch >= (u32)'0') && (ch <= (u32)'9'))
        {
            digit = ch - (u32)'0';
        }
        else if ((base == 16u)
            && (app_model64_ascii_lower(ch) >= (u32)'a')
            && (app_model64_ascii_lower(ch) <= (u32)'f'))
        {
            digit = app_model64_ascii_lower(ch) - (u32)'a' + 10u;
        }
        else
        {
            return 0u;
        }

        if (digit >= base)
        {
            return 0u;
        }
        value = (value * base) + digit;
        ++index;
    }

    *value_out = value;
    return 1u;
}

static u32 app_model64_token_list_has(
    const u8 *descriptor,
    u32 start,
    u32 length,
    const char *token)
{
    u32 cursor = 0u;
    u32 token_length = 0u;

    if ((descriptor == (const u8 *)0) || (token == 0))
    {
        return 0u;
    }

    while (token[token_length] != '\0')
    {
        ++token_length;
    }

    while (cursor < length)
    {
        u32 token_start;
        u32 token_bytes;

        while ((cursor < length)
            && ((descriptor[start + cursor] == (u8)',')
                || (descriptor[start + cursor] == (u8)' ')
                || (descriptor[start + cursor] == (u8)'\t')))
        {
            ++cursor;
        }

        token_start = cursor;
        while ((cursor < length)
            && (descriptor[start + cursor] != (u8)',')
            && (descriptor[start + cursor] != (u8)' ')
            && (descriptor[start + cursor] != (u8)'\t'))
        {
            ++cursor;
        }

        token_bytes = cursor - token_start;
        if ((token_bytes == token_length)
            && (app_model64_span_equals(
                    descriptor,
                    start + token_start,
                    token_bytes,
                    (const u8 *)token,
                    token_length) != 0u))
        {
            return 1u;
        }
    }

    return 0u;
}

static u32 app_model64_binary_name_matches(
    const u8 *app_name,
    u32 app_name_bytes,
    const u8 *descriptor,
    u32 value_start,
    u32 value_length)
{
    static const u8 suffix[] = {'.', 'B', 'I', 'N'};
    u32 index;

    if ((app_name == (const u8 *)0)
        || (descriptor == (const u8 *)0)
        || (app_name_bytes == 0u)
        || ((app_name_bytes + (u32)sizeof(suffix)) != value_length))
    {
        return 0u;
    }

    for (index = 0u; index < app_name_bytes; ++index)
    {
        if (app_model64_ascii_lower(app_name[index])
            != app_model64_ascii_lower(descriptor[value_start + index]))
        {
            return 0u;
        }
    }
    for (index = 0u; index < (u32)sizeof(suffix); ++index)
    {
        if (app_model64_ascii_lower(suffix[index])
            != app_model64_ascii_lower(descriptor[value_start + app_name_bytes + index]))
        {
            return 0u;
        }
    }

    return 1u;
}

static u32 app_model64_parse_native_descriptor(
    const u8 *app_name,
    u32 app_name_bytes,
    const u8 *descriptor,
    u32 descriptor_bytes)
{
    u32 offset = 0u;
    u32 usage_start = 0u;
    u32 usage_length = 0u;
    u32 category_start = 0u;
    u32 category_length = 0u;
    u32 saw_name = 0u;
    u32 saw_binary = 0u;
    u32 saw_payload = 0u;
    u32 saw_entry = 0u;
    u32 saw_success = 0u;
    u32 saw_capabilities = 0u;

    if ((app_name == (const u8 *)0)
        || (app_name_bytes == 0u)
        || (app_name_bytes > APP64_NATIVE_NAME_BYTES)
        || (descriptor == (const u8 *)0)
        || (descriptor_bytes == 0u)
        || (app_model64_line_decimal(
                descriptor,
                descriptor_bytes,
                &offset,
                &g_native_app.executable_id) == 0u)
        || (app_model64_line_decimal(
                descriptor,
                descriptor_bytes,
                &offset,
                &g_native_app.authority_mask) == 0u)
        || (app_model64_line_decimal(
                descriptor,
                descriptor_bytes,
                &offset,
                &g_native_app.argument_policy) == 0u)
        || (app_model64_line_decimal(
                descriptor,
                descriptor_bytes,
                &offset,
                &g_native_app.launch_binding) == 0u)
        || (app_model64_line_span(
                descriptor,
                descriptor_bytes,
                &offset,
                &usage_start,
                &usage_length) == 0u)
        || (app_model64_line_span(
                descriptor,
                descriptor_bytes,
                &offset,
                &category_start,
                &category_length) == 0u))
    {
        return 0u;
    }

    if ((g_native_app.executable_id == 0u)
        || (g_native_app.authority_mask == 0u)
        || (g_native_app.argument_policy == 0u)
        || (g_native_app.launch_binding == 0u)
        || (usage_length == 0u)
        || (category_length == 0u))
    {
        return 0u;
    }

    while (offset < descriptor_bytes)
    {
        u32 line_start = 0u;
        u32 line_length = 0u;
        u32 value_start = 0u;
        u32 value_length = 0u;

        if (app_model64_line_span(
                descriptor,
                descriptor_bytes,
                &offset,
                &line_start,
                &line_length) == 0u)
        {
            break;
        }

        if (app_model64_line_value(
                descriptor,
                line_start,
                line_length,
                "name",
                &value_start,
                &value_length) != 0u)
        {
            if (app_model64_span_equals(
                    descriptor,
                    value_start,
                    value_length,
                    app_name,
                    app_name_bytes) == 0u)
            {
                return 0u;
            }
            saw_name = 1u;
            continue;
        }

        if (app_model64_line_value(
                descriptor,
                line_start,
                line_length,
                "binary",
                &value_start,
                &value_length) != 0u)
        {
            if (app_model64_binary_name_matches(
                    app_name,
                    app_name_bytes,
                    descriptor,
                    value_start,
                    value_length) == 0u)
            {
                return 0u;
            }
            saw_binary = 1u;
            g_native_app.binary_path_verified = 1u;
            continue;
        }

        if (app_model64_line_value(
                descriptor,
                line_start,
                line_length,
                "payload-slot",
                &value_start,
                &value_length) != 0u)
        {
            if (app_model64_parse_u32_span(
                    descriptor,
                    value_start,
                    value_length,
                    &g_native_app.payload_slot) == 0u)
            {
                return 0u;
            }
            saw_payload = 1u;
            continue;
        }

        if (app_model64_line_value(
                descriptor,
                line_start,
                line_length,
                "entry-result",
                &value_start,
                &value_length) != 0u)
        {
            if (app_model64_parse_u32_span(
                    descriptor,
                    value_start,
                    value_length,
                    &g_native_app.entry_result) == 0u)
            {
                return 0u;
            }
            saw_entry = 1u;
            continue;
        }

        if (app_model64_line_value(
                descriptor,
                line_start,
                line_length,
                "success-result",
                &value_start,
                &value_length) != 0u)
        {
            if (app_model64_parse_u32_span(
                    descriptor,
                    value_start,
                    value_length,
                    &g_native_app.success_result) == 0u)
            {
                return 0u;
            }
            saw_success = 1u;
            continue;
        }

        if (app_model64_line_value(
                descriptor,
                line_start,
                line_length,
                "capabilities",
                &value_start,
                &value_length) != 0u)
        {
            if (app_model64_token_list_has(
                    descriptor,
                    value_start,
                    value_length,
                    "console") != 0u)
            {
                g_native_app.capability_mask |= APP64_NATIVE_CAPABILITY_CONSOLE;
            }
            if (app_model64_token_list_has(
                    descriptor,
                    value_start,
                    value_length,
                    "network") != 0u)
            {
                g_native_app.capability_mask |= APP64_NATIVE_CAPABILITY_NETWORK;
            }
            if ((app_model64_token_list_has(
                    descriptor,
                    value_start,
                    value_length,
                    "filesystem") != 0u)
                || (app_model64_token_list_has(
                    descriptor,
                    value_start,
                    value_length,
                    "ramfs") != 0u))
            {
                g_native_app.capability_mask |= APP64_NATIVE_CAPABILITY_FILESYSTEM;
            }
            if ((app_model64_token_list_has(
                    descriptor,
                    value_start,
                    value_length,
                    "storage") != 0u)
                || (app_model64_token_list_has(
                    descriptor,
                    value_start,
                    value_length,
                    "block") != 0u))
            {
                g_native_app.capability_mask |= APP64_NATIVE_CAPABILITY_STORAGE;
            }
            saw_capabilities = 1u;
        }
    }

    if ((saw_name == 0u)
        || (saw_binary == 0u)
        || (saw_payload == 0u)
        || (saw_entry == 0u)
        || (saw_success == 0u)
        || (saw_capabilities == 0u)
        || (g_native_app.payload_slot == 0u)
        || (g_native_app.entry_result == 0u)
        || (g_native_app.success_result == 0u)
        || (g_native_app.capability_mask == 0u))
    {
        return 0u;
    }
    if (((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_CONSOLE) != 0u)
        && ((g_native_app.authority_mask & APP64_DESCRIPTOR_AUTH_CONSOLE) == 0u))
    {
        return 0u;
    }
    if (((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_NETWORK) != 0u)
        && ((g_native_app.authority_mask & APP64_DESCRIPTOR_AUTH_NETWORK) == 0u))
    {
        return 0u;
    }

    if ((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_CONSOLE) != 0u)
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_CONSOLE_DECLARED;
    }
    if ((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_NETWORK) != 0u)
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_NETWORK_DECLARED;
    }

    (void)usage_start;
    (void)category_start;
    return 1u;
}

void app_model64_init(void)
{
    app_model64_reset_native();
    g_app_model64_active = 0u;
    g_app_model64_active_owner = 0u;
}

void app_model64_mark_nethello_unavailable(void)
{
    app_model64_mark_native_unavailable(LAUNCH64_DISK_NETHELLO_PAYLOAD_SLOT);
}

u32 app_model64_stage_native_app(
    const u8 *app_name,
    u32 app_name_bytes,
    const u8 *descriptor,
    u32 descriptor_bytes,
    const void *binary,
    u32 binary_bytes,
    u32 binary_checksum,
    u32 owner_id)
{
    u32 map_token;
    u32 index;

    app_model64_reset_native();
    g_native_app.flags = APP64_NETHELLO_FLAG_REQUESTED;
    g_native_app.owner_id = owner_id;

    if ((owner_id != PRINCIPAL64_ID_CONSOLE_CLIENT)
        || (principal64_is_active(owner_id) == 0u)
        || (app_name == (const u8 *)0)
        || (app_name_bytes == 0u)
        || (app_name_bytes > APP64_NATIVE_NAME_BYTES)
        || (descriptor == (const u8 *)0)
        || (descriptor_bytes == 0u)
        || (binary == 0)
        || (binary_bytes == 0u)
        || (binary_checksum == 0u))
    {
        app_model64_mark_native_unavailable(0u);
        return 0u;
    }

    for (index = 0u; index < app_name_bytes; ++index)
    {
        g_native_app.name[index] = app_name[index];
    }
    g_native_app.name_bytes = app_name_bytes;
    g_native_app.name_token = app_model64_hash_bytes(app_name, app_name_bytes);

    g_native_app.descriptor_bytes = descriptor_bytes;
    g_native_app.flags |= APP64_NETHELLO_FLAG_DESCRIPTOR_READ;
    if (app_model64_parse_native_descriptor(
            app_name,
            app_name_bytes,
            descriptor,
            descriptor_bytes) == 0u)
    {
        app_model64_mark_native_unavailable(0u);
        return 0u;
    }
    g_native_app.flags |= APP64_NETHELLO_FLAG_DESCRIPTOR_PARSED;
    g_native_app.expected_checksum =
        launch64_payload_checksum_by_slot(g_native_app.payload_slot);

    g_native_app.binary_bytes = binary_bytes;
    g_native_app.binary_checksum = binary_checksum;
    g_native_app.flags |= APP64_NETHELLO_FLAG_BINARY_READ;
    if ((binary_bytes != launch64_payload_size_by_slot(g_native_app.payload_slot))
        || (binary_checksum != g_native_app.expected_checksum))
    {
        app_model64_mark_native_unavailable(g_native_app.payload_slot);
        return 0u;
    }
    g_native_app.flags |= APP64_NETHELLO_FLAG_CHECKSUM_VERIFIED;

    map_token = launch64_stage_disk_flat_binary(
        g_native_app.payload_slot,
        binary,
        binary_bytes,
        APP64_NATIVE_MAPPED_BYTES,
        g_native_app.entry_result,
        &g_native_app.entry_rip,
        &g_native_app.entry_rsp,
        &g_native_app.entry_selectors,
        &g_native_app.entry_rflags,
        &g_native_app.map_token);
    if (map_token == 0u)
    {
        app_model64_mark_native_unavailable(g_native_app.payload_slot);
        return 0u;
    }

    g_native_app.map_token = map_token;
    g_native_app.mapped_bytes = APP64_NATIVE_MAPPED_BYTES;
    g_native_app.flags |= APP64_NETHELLO_FLAG_MAPPED
        | APP64_NETHELLO_FLAG_AMBIENT_GATED;
    g_native_app.state = APP64_NETHELLO_STATE_READY;
    g_native_app.token = app_model64_make_token();
    return g_native_app.token;
}

u32 app_model64_stage_nethello(
    const u8 *descriptor,
    u32 descriptor_bytes,
    const void *binary,
    u32 binary_bytes,
    u32 binary_checksum,
    u32 owner_id)
{
    static const u8 name[] = {'N', 'E', 'T', 'H', 'E', 'L', 'L', 'O'};

    return app_model64_stage_native_app(
        name,
        (u32)sizeof(name),
        descriptor,
        descriptor_bytes,
        binary,
        binary_bytes,
        binary_checksum,
        owner_id);
}

u32 app_model64_begin_nethello_user(void)
{
    if ((g_native_app.state != APP64_NETHELLO_STATE_READY)
        || ((g_native_app.flags & APP64_NETHELLO_FLAG_MAPPED) == 0u)
        || (g_native_app.owner_id != PRINCIPAL64_ID_CONSOLE_CLIENT))
    {
        return 0u;
    }

    g_app_model64_active = 1u;
    g_app_model64_active_owner = g_native_app.owner_id;
    return 1u;
}

void app_model64_end_nethello_user(void)
{
    g_app_model64_active = 0u;
    g_app_model64_active_owner = 0u;
}

u32 app_model64_effective_owner(u32 requested_owner)
{
    if ((g_app_model64_active != 0u)
        && (requested_owner == g_app_model64_active_owner))
    {
        return g_app_model64_active_owner;
    }

    return requested_owner;
}

u32 app_model64_capability_request_allowed(
    u32 endpoint_class,
    u32 requested_rights,
    u32 owner_id)
{
    u32 allowed_rights = CAPABILITY64_RIGHT_SEND | CAPABILITY64_RIGHT_QUERY;

    if (g_app_model64_active == 0u)
    {
        return 1u;
    }

    if ((owner_id != g_app_model64_active_owner)
        || ((requested_rights & ~allowed_rights) != 0u))
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_AMBIENT_GATED;
        return 0u;
    }

    if (endpoint_class == SERVICE_ENDPOINT_CLASS_CONSOLE)
    {
        if ((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_CONSOLE) != 0u)
        {
            return 1u;
        }
    }
    else if (endpoint_class == SERVICE_ENDPOINT_CLASS_NETWORK)
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_NETWORK_CAP_REQUESTED;
        if ((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_NETWORK) != 0u)
        {
            return 1u;
        }
    }
    else if (endpoint_class == SERVICE_ENDPOINT_CLASS_RAMFS)
    {
        if ((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_FILESYSTEM) != 0u)
        {
            return 1u;
        }
        g_native_app.flags |= APP64_NETHELLO_FLAG_FS_DENIED
            | APP64_NETHELLO_FLAG_AMBIENT_GATED;
        return 0u;
    }
    else if ((endpoint_class == SERVICE_ENDPOINT_CLASS_BLOCK)
        || (endpoint_class == SERVICE_ENDPOINT_CLASS_HARDWARE))
    {
        if ((g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_STORAGE) != 0u)
        {
            return 1u;
        }
        g_native_app.flags |= APP64_NETHELLO_FLAG_STORAGE_DENIED
            | APP64_NETHELLO_FLAG_AMBIENT_GATED;
        return 0u;
    }

    g_native_app.flags |= APP64_NETHELLO_FLAG_AMBIENT_GATED;
    return 0u;
}

void app_model64_record_network_capability(u32 handle)
{
    if ((g_app_model64_active != 0u)
        && (handle != CAPABILITY64_INVALID_HANDLE))
    {
        g_native_app.network_capability = handle;
        g_native_app.flags |= APP64_NETHELLO_FLAG_NETWORK_CAP_GRANTED;
    }
}

void app_model64_record_socket_open(u32 socket_handle)
{
    if ((g_app_model64_active != 0u)
        && (socket_handle != 0u)
        && (socket_handle != CAPABILITY64_INVALID_HANDLE))
    {
        g_native_app.socket_handle = socket_handle;
        g_native_app.flags |= APP64_NETHELLO_FLAG_SOCKET_OPEN;
    }
}

void app_model64_record_recv_status(u32 byte_count)
{
    if ((g_app_model64_active != 0u) && (byte_count != 0u))
    {
        g_native_app.recv_bytes = byte_count;
        g_native_app.flags |= APP64_NETHELLO_FLAG_RECV_STATUS;
    }
}

void app_model64_record_send(u32 byte_count)
{
    if ((g_app_model64_active != 0u) && (byte_count == 0u))
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_SEND_DENIED;
    }
}

void app_model64_record_close(u32 closed)
{
    if ((g_app_model64_active != 0u) && (closed != 0u))
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_SOCKET_CLOSED;
    }
}

u32 app_model64_record_native_launch(u32 result, u32 aux)
{
    g_native_app.exit_result = result;
    g_native_app.exit_aux = aux;
    if (g_native_app.state == APP64_NETHELLO_STATE_READY)
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_LAUNCHED;
    }

    if ((result == g_native_app.success_result)
        && (g_native_app.success_result != 0u))
    {
        g_native_app.flags |= APP64_NETHELLO_FLAG_HELLO_COMPLETED
            | APP64_NETHELLO_FLAG_SYSCALL_BRIDGE;
        g_native_app.state = APP64_NETHELLO_STATE_COMPLETED;
    }

    g_native_app.token = app_model64_make_token();
    return g_native_app.token;
}

u32 app_model64_record_nethello_launch(u32 result, u32 aux)
{
    return app_model64_record_native_launch(result, aux);
}

u32 app_model64_native_name_token(void) { return g_native_app.name_token; }
u32 app_model64_native_executable_id(void) { return g_native_app.executable_id; }
u32 app_model64_native_authority_mask(void) { return g_native_app.authority_mask; }
u32 app_model64_native_capability_mask(void) { return g_native_app.capability_mask; }
u32 app_model64_native_payload_slot(void) { return g_native_app.payload_slot; }
u32 app_model64_native_entry_result(void) { return g_native_app.entry_result; }
u32 app_model64_native_success_result(void) { return g_native_app.success_result; }
u32 app_model64_native_binary_path_verified(void) { return g_native_app.binary_path_verified; }

u32 app_model64_nethello_token(void) { return g_native_app.token; }
u32 app_model64_nethello_state(void) { return g_native_app.state; }
u32 app_model64_nethello_flags(void) { return g_native_app.flags; }
u32 app_model64_nethello_owner(void) { return g_native_app.owner_id; }
u32 app_model64_nethello_descriptor_bytes(void) { return g_native_app.descriptor_bytes; }
u32 app_model64_nethello_binary_bytes(void) { return g_native_app.binary_bytes; }
u32 app_model64_nethello_checksum(void) { return g_native_app.binary_checksum; }
u32 app_model64_nethello_expected_checksum(void) { return g_native_app.expected_checksum; }
u32 app_model64_nethello_mapped_bytes(void) { return g_native_app.mapped_bytes; }
u32 app_model64_nethello_entry_rip(void) { return g_native_app.entry_rip; }
u32 app_model64_nethello_entry_rsp(void) { return g_native_app.entry_rsp; }
u32 app_model64_nethello_entry_selectors(void) { return g_native_app.entry_selectors; }
u32 app_model64_nethello_entry_rflags(void) { return g_native_app.entry_rflags; }
u32 app_model64_nethello_exit_result(void) { return g_native_app.exit_result; }
u32 app_model64_nethello_exit_aux(void) { return g_native_app.exit_aux; }

u32 app_model64_nethello_descriptor_read(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_DESCRIPTOR_READ) ? 1u : 0u;
}

u32 app_model64_nethello_descriptor_parsed(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_DESCRIPTOR_PARSED) ? 1u : 0u;
}

u32 app_model64_nethello_binary_read(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_BINARY_READ) ? 1u : 0u;
}

u32 app_model64_nethello_checksum_verified(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_CHECKSUM_VERIFIED) ? 1u : 0u;
}

u32 app_model64_nethello_mapped(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_MAPPED) ? 1u : 0u;
}

u32 app_model64_nethello_launched(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_LAUNCHED) ? 1u : 0u;
}

u32 app_model64_nethello_hello_completed(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_HELLO_COMPLETED) ? 1u : 0u;
}

u32 app_model64_nethello_network_cap_requested(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_NETWORK_CAP_REQUESTED) ? 1u : 0u;
}

u32 app_model64_nethello_network_cap_granted(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_NETWORK_CAP_GRANTED) ? 1u : 0u;
}

u32 app_model64_nethello_socket_opened(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_SOCKET_OPEN) ? 1u : 0u;
}

u32 app_model64_nethello_recv_status(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_RECV_STATUS) ? 1u : 0u;
}

u32 app_model64_nethello_send_denied(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_SEND_DENIED) ? 1u : 0u;
}

u32 app_model64_nethello_socket_closed(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_SOCKET_CLOSED) ? 1u : 0u;
}

u32 app_model64_nethello_fs_denied(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_FS_DENIED) ? 1u : 0u;
}

u32 app_model64_nethello_storage_denied(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_STORAGE_DENIED) ? 1u : 0u;
}

u32 app_model64_nethello_syscall_bridge(void)
{
    return (g_native_app.flags & APP64_NETHELLO_FLAG_SYSCALL_BRIDGE) ? 1u : 0u;
}

u32 app_model64_nethello_fs_authority(void)
{
    return (g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_FILESYSTEM) ? 1u : 0u;
}

u32 app_model64_nethello_storage_authority(void)
{
    return (g_native_app.capability_mask & APP64_NATIVE_CAPABILITY_STORAGE) ? 1u : 0u;
}

u32 app_model64_nethello_ambient_authority(void)
{
    return 0u;
}
