#include "fs_x64.h"

#include "capability_x64.h"
#include "launch_x64.h"
#include "mmio_x64.h"
#include "principal_x64.h"
#include "ramfs.h"
#include "runtime_image_x64.h"
#include "services.h"
#include "services_x64.h"

struct fs64_node_capability
{
    u32 active;
    u32 handle;
    u32 node_id;
    u32 rights;
    u32 parent_handle;
    u32 owner_id;
    u32 runtime_generation;
    u32 runtime_token;
    u32 storage_descriptor_selector;
    u32 path_byte_count;
    u8 path[64];
};

enum
{
    FS64_TABLE_LIMIT = 32,
    FS64_HANDLE_BASE = 0x6600u,
    FS64_PATH_SCRATCH_BYTES = 256u,
    FS64_IO_SCRATCH_BYTES = 256u,
    FS64_STORED_PATH_BYTES = 64u,
    FS64_KERNEL_HIGH_BASE_LOW32 = 0x80000000u,
    FS64_KERNEL_HIGH_BASE_HIGH32 = 0xFFFFFFFFu
};

static struct fs64_node_capability g_fs64_caps[FS64_TABLE_LIMIT];
static u8 g_path_scratch[FS64_PATH_SCRATCH_BYTES];
static u8 g_io_scratch[FS64_IO_SCRATCH_BYTES];
static u32 g_next_handle = FS64_HANDLE_BASE;
static u32 g_initialized = 0u;
static u32 g_open_count = 0u;
static u32 g_create_count = 0u;
static u32 g_list_count = 0u;
static u32 g_read_count = 0u;
static u32 g_write_count = 0u;
static u32 g_stat_count = 0u;
static u32 g_revoke_count = 0u;
static u32 g_denial_count = 0u;
static u32 g_stale_denial_count = 0u;

static void fs64_zero(void *address, u32 byte_count)
{
    u8 *bytes = (u8 *)address;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        bytes[index] = 0u;
    }
}

static void fs64_copy(void *destination, const void *source, u32 byte_count)
{
    u8 *dest = (u8 *)destination;
    const u8 *src = (const u8 *)source;
    u32 index;

    for (index = 0u; index < byte_count; ++index)
    {
        dest[index] = src[index];
    }
}

static u32 fs64_deny(void)
{
    ++g_denial_count;
    return FS64_INVALID_HANDLE;
}

static int fs64_owner_is_valid(u32 owner_id)
{
    if (principal64_is_active(owner_id) != 0u)
    {
        return 1;
    }

    ++g_denial_count;
    return 0;
}

static int fs64_range_overflows(u64 address, u32 byte_count)
{
    u64 end;

    if (byte_count == 0u)
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return end < address;
}

static int fs64_address_is_kernel_high(u64 address, u32 byte_count)
{
    u64 end;

    if (fs64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return ((u32)(address >> 32) >= FS64_KERNEL_HIGH_BASE_HIGH32)
        && ((u32)address >= FS64_KERNEL_HIGH_BASE_LOW32)
        && (end >= address);
}

static int fs64_address_is_user_stack(u64 address, u32 byte_count)
{
    u64 stack_base = (u64)(LAUNCH64_USER_STACK_TOP - LAUNCH64_USER_STACK_BYTES);
    u64 stack_top = (u64)LAUNCH64_USER_STACK_TOP;
    u64 end;

    if (fs64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= stack_base) && (end <= stack_top);
}

static int fs64_address_is_user_image(u64 address, u32 byte_count)
{
    u64 image_base = (u64)LAUNCH64_USER_IMAGE_BASE;
    u64 image_end = image_base + (u64)runtime64_transfer_image_size();
    u64 end;

    if (fs64_range_overflows(address, byte_count))
    {
        return 0;
    }

    end = address + (u64)byte_count;
    return (address >= image_base) && (end <= image_end);
}

static int fs64_address_readable(u64 address, u32 byte_count)
{
    if (byte_count == 0u)
    {
        return 1;
    }

    if (address == 0ull)
    {
        return 0;
    }

    return fs64_address_is_kernel_high(address, byte_count)
        || fs64_address_is_user_stack(address, byte_count)
        || fs64_address_is_user_image(address, byte_count);
}

static int fs64_address_writable(u64 address, u32 byte_count)
{
    if (byte_count == 0u)
    {
        return 1;
    }

    if (address == 0ull)
    {
        return 0;
    }

    return fs64_address_is_kernel_high(address, byte_count)
        || fs64_address_is_user_stack(address, byte_count);
}

static int fs64_copy_from_address(u64 source_address, u8 *destination, u32 byte_count)
{
    if ((destination == 0) || !fs64_address_readable(source_address, byte_count))
    {
        ++g_denial_count;
        return 0;
    }

    fs64_copy(destination, (const void *)source_address, byte_count);
    return 1;
}

static int fs64_copy_to_address(u64 destination_address, const u8 *source, u32 byte_count)
{
    if ((source == 0) || !fs64_address_writable(destination_address, byte_count))
    {
        ++g_denial_count;
        return 0;
    }

    fs64_copy((void *)destination_address, source, byte_count);
    return 1;
}

static u32 fs64_storage_descriptor_selector(const u8 *path, u32 path_byte_count)
{
    return mmio64_fs_shell_descriptor_selector_by_path(path, path_byte_count);
}

static u32 fs64_rights_for_node(u32 node_id)
{
    if (ramfs_node_is_directory(node_id))
    {
        return FS64_RIGHT_LIST
            | FS64_RIGHT_CREATE
            | FS64_RIGHT_STAT
            | FS64_RIGHT_RENAME
            | FS64_RIGHT_DELETE
            | FS64_RIGHT_DELEGATE;
    }

    return FS64_RIGHT_READ
        | FS64_RIGHT_WRITE
        | FS64_RIGHT_STAT
        | FS64_RIGHT_DELEGATE;
}

static void fs64_clear_capability(struct fs64_node_capability *record)
{
    if (record == 0)
    {
        return;
    }

    record->active = 0u;
    record->node_id = 0u;
    record->rights = 0u;
    record->parent_handle = 0u;
    record->owner_id = 0u;
    record->runtime_generation = 0u;
    record->runtime_token = 0u;
    record->storage_descriptor_selector = 0u;
    record->path_byte_count = 0u;
    fs64_zero(record->path, FS64_STORED_PATH_BYTES);
}

static void fs64_store_path(
    struct fs64_node_capability *record,
    const u8 *path,
    u32 path_byte_count)
{
    u32 index;
    u32 limit;

    if (record == 0)
    {
        return;
    }

    record->path_byte_count = 0u;
    fs64_zero(record->path, FS64_STORED_PATH_BYTES);
    if ((path == 0) || (path_byte_count == 0u))
    {
        return;
    }

    limit = (path_byte_count < (FS64_STORED_PATH_BYTES - 1u))
        ? path_byte_count
        : (FS64_STORED_PATH_BYTES - 1u);
    for (index = 0u; index < limit; ++index)
    {
        record->path[index] = path[index];
    }
    record->path_byte_count = limit;
}

static struct fs64_node_capability *fs64_find_live(u32 handle)
{
    u32 index;

    for (index = 0u; index < FS64_TABLE_LIMIT; ++index)
    {
        if ((g_fs64_caps[index].active != 0u) && (g_fs64_caps[index].handle == handle))
        {
            return &g_fs64_caps[index];
        }
    }

    return 0;
}

static struct fs64_node_capability *fs64_find_free(void)
{
    u32 index;

    for (index = 0u; index < FS64_TABLE_LIMIT; ++index)
    {
        if (g_fs64_caps[index].active == 0u)
        {
            return &g_fs64_caps[index];
        }
    }

    return 0;
}

static int fs64_runtime_is_current(struct fs64_node_capability *record)
{
    u32 manifest_index;

    if ((record == 0) || (record->runtime_token == 0u))
    {
        return 1;
    }

    manifest_index = launch64_manifest_by_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS);
    if ((manifest_index != LAUNCH64_INVALID_MANIFEST)
        && (launch64_manifest_runtime_generation(manifest_index) == record->runtime_generation)
        && (launch64_manifest_accepts_runtime_token(manifest_index, record->runtime_token) != 0u))
    {
        return 1;
    }

    fs64_clear_capability(record);
    ++g_stale_denial_count;
    ++g_denial_count;
    return 0;
}

static struct fs64_node_capability *fs64_resolve_node_capability(
    u32 handle,
    u32 required_rights,
    u32 owner_id)
{
    struct fs64_node_capability *record;

    if (!fs64_owner_is_valid(owner_id))
    {
        return 0;
    }

    record = fs64_find_live(handle);
    if ((record == 0)
        || (record->owner_id != owner_id)
        || ((record->rights & required_rights) != required_rights)
        || !ramfs_node_exists(record->node_id))
    {
        ++g_denial_count;
        return 0;
    }

    if (!fs64_runtime_is_current(record))
    {
        return 0;
    }

    return record;
}

static int fs64_resolve_base_node(
    u32 base_capability_handle,
    u32 required_rights,
    u32 owner_id,
    u32 *node_id_out,
    u32 *runtime_generation_out,
    u32 *runtime_token_out,
    u32 *parent_handle_out)
{
    struct fs64_node_capability *base_node;
    u32 endpoint_id;
    u32 ramfs_endpoint;

    if ((node_id_out == 0)
        || (runtime_generation_out == 0)
        || (runtime_token_out == 0)
        || (parent_handle_out == 0))
    {
        ++g_denial_count;
        return 0;
    }

    *node_id_out = 0u;
    *runtime_generation_out = 0u;
    *runtime_token_out = 0u;
    *parent_handle_out = 0u;

    base_node = fs64_find_live(base_capability_handle);
    if (base_node != 0)
    {
        if ((base_node->owner_id != owner_id)
            || ((base_node->rights & required_rights) != required_rights)
            || !ramfs_node_is_directory(base_node->node_id)
            || !fs64_runtime_is_current(base_node))
        {
            ++g_denial_count;
            return 0;
        }

        *node_id_out = base_node->node_id;
        *runtime_generation_out = base_node->runtime_generation;
        *runtime_token_out = base_node->runtime_token;
        *parent_handle_out = base_node->handle;
        return 1;
    }

    endpoint_id = capability64_route(
        base_capability_handle,
        CAPABILITY64_RIGHT_SEND,
        owner_id);
    ramfs_endpoint = services64_resolve_endpoint_class(SERVICE_ENDPOINT_CLASS_RAMFS);
    if (endpoint_id != ramfs_endpoint)
    {
        ++g_denial_count;
        return 0;
    }

    *node_id_out = ramfs_root_node();
    *runtime_generation_out = capability64_runtime_generation(base_capability_handle, owner_id);
    *runtime_token_out = capability64_runtime_token(base_capability_handle, owner_id);
    return 1;
}

static u32 fs64_grant_node(
    u32 node_id,
    u32 owner_id,
    u32 parent_handle,
    u32 runtime_generation,
    u32 runtime_token,
    u32 storage_descriptor_selector,
    const u8 *path,
    u32 path_byte_count)
{
    struct fs64_node_capability *record;

    if (!fs64_owner_is_valid(owner_id) || !ramfs_node_exists(node_id))
    {
        return fs64_deny();
    }

    record = fs64_find_free();
    if (record == 0)
    {
        return fs64_deny();
    }

    record->active = 1u;
    record->handle = g_next_handle++;
    record->node_id = node_id;
    record->rights = fs64_rights_for_node(node_id);
    record->parent_handle = parent_handle;
    record->owner_id = owner_id;
    record->runtime_generation = runtime_generation;
    record->runtime_token = runtime_token;
    record->storage_descriptor_selector = storage_descriptor_selector;
    fs64_store_path(record, path, path_byte_count);
    return record->handle;
}

void fs64_init(void)
{
    u32 index;

    ramfs_init();
    for (index = 0u; index < FS64_TABLE_LIMIT; ++index)
    {
        g_fs64_caps[index].active = 0u;
        g_fs64_caps[index].handle = 0u;
        fs64_clear_capability(&g_fs64_caps[index]);
    }

    fs64_zero(g_path_scratch, sizeof(g_path_scratch));
    fs64_zero(g_io_scratch, sizeof(g_io_scratch));
    g_next_handle = FS64_HANDLE_BASE;
    g_open_count = 0u;
    g_create_count = 0u;
    g_list_count = 0u;
    g_read_count = 0u;
    g_write_count = 0u;
    g_stat_count = 0u;
    g_revoke_count = 0u;
    g_denial_count = 0u;
    g_stale_denial_count = 0u;
    g_initialized = 1u;
}

static void fs64_ensure_init(void)
{
    if (g_initialized == 0u)
    {
        fs64_init();
    }
}

static int fs64_load_path_argument(u64 path_address, const u8 *trusted_path, u32 path_byte_count)
{
    fs64_zero(g_path_scratch, sizeof(g_path_scratch));
    if (trusted_path != 0)
    {
        fs64_copy(g_path_scratch, trusted_path, path_byte_count);
        return 1;
    }

    return fs64_copy_from_address(path_address, g_path_scratch, path_byte_count);
}

static u32 fs64_open_common(
    u32 base_capability_handle,
    u64 path_address,
    const u8 *trusted_path,
    u32 path_byte_count,
    u32 owner_id)
{
    u32 base_node_id;
    u32 runtime_generation;
    u32 runtime_token;
    u32 parent_handle;
    u32 node_id;

    fs64_ensure_init();
    if ((path_byte_count == 0u) || (path_byte_count >= FS64_PATH_SCRATCH_BYTES))
    {
        return fs64_deny();
    }

    if (!fs64_resolve_base_node(
            base_capability_handle,
            FS64_RIGHT_LIST,
            owner_id,
            &base_node_id,
            &runtime_generation,
            &runtime_token,
            &parent_handle))
    {
        return FS64_INVALID_HANDLE;
    }

    if (!fs64_load_path_argument(path_address, trusted_path, path_byte_count))
    {
        return FS64_INVALID_HANDLE;
    }

    if (!ramfs_open(base_node_id, g_path_scratch, path_byte_count, &node_id))
    {
        u32 imported_bytes = 0u;

        if ((mmio64_nvme_fat_shell_read_file(
                    g_path_scratch,
                    path_byte_count,
                    g_io_scratch,
                    FS64_IO_SCRATCH_BYTES,
                    owner_id,
                    &imported_bytes) == 0u)
            || (imported_bytes == 0u)
            || !ramfs_create(
                base_node_id,
                g_path_scratch,
                path_byte_count,
                RAMFS_NODE_FILE,
                &node_id)
            || (ramfs_write(node_id, 0u, g_io_scratch, imported_bytes) != imported_bytes))
        {
            return fs64_deny();
        }
    }

    ++g_open_count;
    return fs64_grant_node(
        node_id,
        owner_id,
        parent_handle,
        runtime_generation,
        runtime_token,
        fs64_storage_descriptor_selector(g_path_scratch, path_byte_count),
        g_path_scratch,
        path_byte_count);
}

u32 fs64_open(u32 base_capability_handle, u64 path_address, u32 path_byte_count, u32 owner_id)
{
    return fs64_open_common(base_capability_handle, path_address, 0, path_byte_count, owner_id);
}

u32 fs64_open_kernel(
    u32 base_capability_handle,
    const u8 *path,
    u32 path_byte_count,
    u32 owner_id)
{
    if (path == 0)
    {
        return fs64_deny();
    }

    return fs64_open_common(base_capability_handle, 0u, path, path_byte_count, owner_id);
}

static u32 fs64_create_common(
    u32 base_capability_handle,
    u64 path_address,
    const u8 *trusted_path,
    u32 path_byte_count,
    u32 node_type,
    u32 owner_id)
{
    u32 base_node_id;
    u32 runtime_generation;
    u32 runtime_token;
    u32 parent_handle;
    u32 node_id;

    fs64_ensure_init();
    if ((path_byte_count == 0u) || (path_byte_count >= FS64_PATH_SCRATCH_BYTES))
    {
        return fs64_deny();
    }

    if (!fs64_resolve_base_node(
            base_capability_handle,
            FS64_RIGHT_CREATE,
            owner_id,
            &base_node_id,
            &runtime_generation,
            &runtime_token,
            &parent_handle))
    {
        return FS64_INVALID_HANDLE;
    }

    if (!fs64_load_path_argument(path_address, trusted_path, path_byte_count))
    {
        return FS64_INVALID_HANDLE;
    }

    if (!ramfs_create(base_node_id, g_path_scratch, path_byte_count, node_type, &node_id))
    {
        return fs64_deny();
    }

    ++g_create_count;
    return fs64_grant_node(
        node_id,
        owner_id,
        parent_handle,
        runtime_generation,
        runtime_token,
        0u,
        g_path_scratch,
        path_byte_count);
}

u32 fs64_create(
    u32 base_capability_handle,
    u64 path_address,
    u32 path_byte_count,
    u32 node_type,
    u32 owner_id)
{
    return fs64_create_common(
        base_capability_handle,
        path_address,
        0,
        path_byte_count,
        node_type,
        owner_id);
}

u32 fs64_create_kernel(
    u32 base_capability_handle,
    const u8 *path,
    u32 path_byte_count,
    u32 node_type,
    u32 owner_id)
{
    if (path == 0)
    {
        return fs64_deny();
    }

    return fs64_create_common(
        base_capability_handle,
        0u,
        path,
        path_byte_count,
        node_type,
        owner_id);
}

u32 fs64_list(u32 node_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    struct fs64_node_capability *record;
    u32 actual_count;

    fs64_ensure_init();
    if ((byte_capacity == 0u) || (byte_capacity > FS64_IO_SCRATCH_BYTES))
    {
        byte_capacity = FS64_IO_SCRATCH_BYTES;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_LIST,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    fs64_zero(g_io_scratch, sizeof(g_io_scratch));
    actual_count = ramfs_list(record->node_id, g_io_scratch, byte_capacity);
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    if (!fs64_copy_to_address(output_address, g_io_scratch, actual_count))
    {
        return FS64_INVALID_HANDLE;
    }

    ++g_list_count;
    return actual_count;
}

u32 fs64_list_kernel(
    u32 node_capability_handle,
    u8 *output,
    u32 byte_capacity,
    u32 owner_id)
{
    struct fs64_node_capability *record;
    u32 actual_count;

    fs64_ensure_init();
    if ((output == 0) || (byte_capacity == 0u) || (byte_capacity > FS64_IO_SCRATCH_BYTES))
    {
        byte_capacity = FS64_IO_SCRATCH_BYTES;
    }

    if (output == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_LIST,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    fs64_zero(output, byte_capacity);
    actual_count = ramfs_list(record->node_id, output, byte_capacity);
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    ++g_list_count;
    return actual_count;
}

u32 fs64_read(
    u32 node_capability_handle,
    u64 output_address,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id)
{
    struct fs64_node_capability *record;
    u32 actual_count;

    fs64_ensure_init();
    if ((byte_count == 0u) || (byte_count > FS64_IO_SCRATCH_BYTES))
    {
        byte_count = FS64_IO_SCRATCH_BYTES;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_READ,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    fs64_zero(g_io_scratch, sizeof(g_io_scratch));
    if (record->storage_descriptor_selector != 0u)
    {
        actual_count = mmio64_fs_shell_read_descriptor(
            record->storage_descriptor_selector,
            file_offset,
            g_io_scratch,
            byte_count,
            owner_id);
    }
    else
    {
        actual_count = ramfs_read(record->node_id, file_offset, g_io_scratch, byte_count);
    }
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    if (!fs64_copy_to_address(output_address, g_io_scratch, actual_count))
    {
        return FS64_INVALID_HANDLE;
    }

    ++g_read_count;
    return actual_count;
}

u32 fs64_read_kernel(
    u32 node_capability_handle,
    u8 *output,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id)
{
    struct fs64_node_capability *record;
    u32 actual_count;

    fs64_ensure_init();
    if ((output == 0) || (byte_count == 0u) || (byte_count > FS64_IO_SCRATCH_BYTES))
    {
        byte_count = FS64_IO_SCRATCH_BYTES;
    }

    if (output == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_READ,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    fs64_zero(output, byte_count);
    if (record->storage_descriptor_selector != 0u)
    {
        actual_count = mmio64_fs_shell_read_descriptor(
            record->storage_descriptor_selector,
            file_offset,
            output,
            byte_count,
            owner_id);
    }
    else
    {
        actual_count = ramfs_read(record->node_id, file_offset, output, byte_count);
    }
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    ++g_read_count;
    return actual_count;
}

u32 fs64_write(
    u32 node_capability_handle,
    u64 input_address,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id)
{
    struct fs64_node_capability *record;
    u32 actual_count;

    fs64_ensure_init();
    if ((byte_count == 0u) || (byte_count > FS64_IO_SCRATCH_BYTES))
    {
        byte_count = FS64_IO_SCRATCH_BYTES;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_WRITE,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    fs64_zero(g_io_scratch, sizeof(g_io_scratch));
    if (!fs64_copy_from_address(input_address, g_io_scratch, byte_count))
    {
        return FS64_INVALID_HANDLE;
    }

    actual_count = ramfs_write(record->node_id, file_offset, g_io_scratch, byte_count);
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    if ((file_offset == 0u)
        && (actual_count != 0u)
        && (record->path_byte_count != 0u))
    {
        (void)mmio64_nvme_fat_shell_write_file(
            record->path,
            record->path_byte_count,
            g_io_scratch,
            actual_count,
            owner_id);
    }

    ++g_write_count;
    return actual_count;
}

u32 fs64_write_kernel(
    u32 node_capability_handle,
    const u8 *input,
    u32 file_offset,
    u32 byte_count,
    u32 owner_id)
{
    struct fs64_node_capability *record;
    u32 actual_count;

    fs64_ensure_init();
    if ((input == 0) || (byte_count == 0u) || (byte_count > FS64_IO_SCRATCH_BYTES))
    {
        byte_count = FS64_IO_SCRATCH_BYTES;
    }

    if (input == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_WRITE,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    actual_count = ramfs_write(record->node_id, file_offset, input, byte_count);
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    if ((file_offset == 0u)
        && (actual_count != 0u)
        && (record->path_byte_count != 0u))
    {
        (void)mmio64_nvme_fat_shell_write_file(
            record->path,
            record->path_byte_count,
            input,
            actual_count,
            owner_id);
    }

    ++g_write_count;
    return actual_count;
}

u32 fs64_stat(u32 node_capability_handle, u64 output_address, u32 byte_capacity, u32 owner_id)
{
    struct fs64_node_capability *record;
    struct ramfs_stat stat;
    u32 actual_count;

    fs64_ensure_init();
    if ((byte_capacity == 0u) || (byte_capacity > FS64_IO_SCRATCH_BYTES))
    {
        byte_capacity = FS64_IO_SCRATCH_BYTES;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_STAT,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    if (!ramfs_stat(record->node_id, &stat))
    {
        return fs64_deny();
    }

    fs64_zero(g_io_scratch, sizeof(g_io_scratch));
    actual_count = ramfs_format_stat(&stat, g_io_scratch, byte_capacity);
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    if (!fs64_copy_to_address(output_address, g_io_scratch, actual_count))
    {
        return FS64_INVALID_HANDLE;
    }

    ++g_stat_count;
    return actual_count;
}

u32 fs64_stat_kernel(
    u32 node_capability_handle,
    u8 *output,
    u32 byte_capacity,
    u32 owner_id)
{
    struct fs64_node_capability *record;
    struct ramfs_stat stat;
    u32 actual_count;

    fs64_ensure_init();
    if ((output == 0) || (byte_capacity == 0u) || (byte_capacity > FS64_IO_SCRATCH_BYTES))
    {
        byte_capacity = FS64_IO_SCRATCH_BYTES;
    }

    if (output == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    record = fs64_resolve_node_capability(
        node_capability_handle,
        FS64_RIGHT_STAT,
        owner_id);
    if (record == 0)
    {
        return FS64_INVALID_HANDLE;
    }

    if (!ramfs_stat(record->node_id, &stat))
    {
        return fs64_deny();
    }

    fs64_zero(output, byte_capacity);
    actual_count = ramfs_format_stat(&stat, output, byte_capacity);
    if (actual_count == 0xFFFFFFFFu)
    {
        return fs64_deny();
    }

    ++g_stat_count;
    return actual_count;
}

u32 fs64_revoke(u32 node_capability_handle, u32 owner_id)
{
    struct fs64_node_capability *record;
    u32 index;

    fs64_ensure_init();
    record = fs64_resolve_node_capability(node_capability_handle, 0u, owner_id);
    if (record == 0)
    {
        return 0u;
    }

    fs64_clear_capability(record);
    ++g_revoke_count;

    for (index = 0u; index < FS64_TABLE_LIMIT; ++index)
    {
        if ((g_fs64_caps[index].active != 0u)
            && (g_fs64_caps[index].parent_handle == node_capability_handle))
        {
            fs64_clear_capability(&g_fs64_caps[index]);
            ++g_revoke_count;
        }
    }

    return 1u;
}

static u32 fs64_rename_common(
    u32 base_capability_handle,
    u64 path_address,
    const u8 *trusted_path_pair,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id)
{
    u32 base_node_id;
    u32 runtime_generation;
    u32 runtime_token;
    u32 parent_handle;
    u32 destination_offset;
    u32 copy_byte_count;

    fs64_ensure_init();
    if ((source_path_byte_count == 0u)
        || (destination_path_byte_count == 0u)
        || (source_path_byte_count >= FS64_PATH_SCRATCH_BYTES)
        || (destination_path_byte_count >= FS64_PATH_SCRATCH_BYTES))
    {
        return 0u;
    }

    destination_offset = source_path_byte_count + 1u;
    copy_byte_count = destination_offset + destination_path_byte_count;
    if (copy_byte_count > FS64_PATH_SCRATCH_BYTES)
    {
        return 0u;
    }

    if (!fs64_resolve_base_node(
            base_capability_handle,
            FS64_RIGHT_RENAME,
            owner_id,
            &base_node_id,
            &runtime_generation,
            &runtime_token,
            &parent_handle))
    {
        return 0u;
    }

    (void)runtime_generation;
    (void)runtime_token;
    (void)parent_handle;
    fs64_zero(g_path_scratch, sizeof(g_path_scratch));
    if (trusted_path_pair != 0)
    {
        fs64_copy(g_path_scratch, trusted_path_pair, copy_byte_count);
    }
    else if (!fs64_copy_from_address(path_address, g_path_scratch, copy_byte_count))
    {
        return 0u;
    }

    return ramfs_rename(
        base_node_id,
        g_path_scratch,
        source_path_byte_count,
        g_path_scratch + destination_offset,
        destination_path_byte_count) ? 1u : fs64_deny();
}

u32 fs64_rename(
    u32 base_capability_handle,
    u64 path_address,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id)
{
    return fs64_rename_common(
        base_capability_handle,
        path_address,
        0,
        source_path_byte_count,
        destination_path_byte_count,
        owner_id);
}

u32 fs64_rename_kernel(
    u32 base_capability_handle,
    const u8 *path_pair,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id)
{
    if (path_pair == 0)
    {
        return 0u;
    }

    return fs64_rename_common(
        base_capability_handle,
        0u,
        path_pair,
        source_path_byte_count,
        destination_path_byte_count,
        owner_id);
}

static u32 fs64_move_common(
    u32 source_base_capability_handle,
    u32 destination_base_capability_handle,
    u64 path_address,
    const u8 *trusted_path_pair,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id)
{
    u32 source_base_node_id;
    u32 destination_base_node_id;
    u32 runtime_generation;
    u32 runtime_token;
    u32 parent_handle;
    u32 destination_offset;
    u32 copy_byte_count;

    fs64_ensure_init();
    if ((source_path_byte_count == 0u)
        || (destination_path_byte_count == 0u)
        || (source_path_byte_count >= FS64_PATH_SCRATCH_BYTES)
        || (destination_path_byte_count >= FS64_PATH_SCRATCH_BYTES))
    {
        return 0u;
    }

    destination_offset = source_path_byte_count + 1u;
    copy_byte_count = destination_offset + destination_path_byte_count;
    if (copy_byte_count > FS64_PATH_SCRATCH_BYTES)
    {
        return 0u;
    }

    if (!fs64_resolve_base_node(
            source_base_capability_handle,
            FS64_RIGHT_RENAME,
            owner_id,
            &source_base_node_id,
            &runtime_generation,
            &runtime_token,
            &parent_handle))
    {
        return 0u;
    }

    if (!fs64_resolve_base_node(
            destination_base_capability_handle,
            FS64_RIGHT_RENAME,
            owner_id,
            &destination_base_node_id,
            &runtime_generation,
            &runtime_token,
            &parent_handle))
    {
        return 0u;
    }

    (void)runtime_generation;
    (void)runtime_token;
    (void)parent_handle;
    fs64_zero(g_path_scratch, sizeof(g_path_scratch));
    if (trusted_path_pair != 0)
    {
        fs64_copy(g_path_scratch, trusted_path_pair, copy_byte_count);
    }
    else if (!fs64_copy_from_address(path_address, g_path_scratch, copy_byte_count))
    {
        return 0u;
    }

    return ramfs_move(
        source_base_node_id,
        g_path_scratch,
        source_path_byte_count,
        destination_base_node_id,
        g_path_scratch + destination_offset,
        destination_path_byte_count) ? 1u : fs64_deny();
}

u32 fs64_move(
    u32 source_base_capability_handle,
    u32 destination_base_capability_handle,
    u64 path_address,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id)
{
    return fs64_move_common(
        source_base_capability_handle,
        destination_base_capability_handle,
        path_address,
        0,
        source_path_byte_count,
        destination_path_byte_count,
        owner_id);
}

u32 fs64_move_kernel(
    u32 source_base_capability_handle,
    u32 destination_base_capability_handle,
    const u8 *path_pair,
    u32 source_path_byte_count,
    u32 destination_path_byte_count,
    u32 owner_id)
{
    if (path_pair == 0)
    {
        return 0u;
    }

    return fs64_move_common(
        source_base_capability_handle,
        destination_base_capability_handle,
        0u,
        path_pair,
        source_path_byte_count,
        destination_path_byte_count,
        owner_id);
}

static u32 fs64_delete_common(
    u32 base_capability_handle,
    u64 path_address,
    const u8 *trusted_path,
    u32 path_byte_count,
    u32 owner_id)
{
    u32 base_node_id;
    u32 runtime_generation;
    u32 runtime_token;
    u32 parent_handle;

    fs64_ensure_init();
    if ((path_byte_count == 0u) || (path_byte_count >= FS64_PATH_SCRATCH_BYTES))
    {
        return 0u;
    }

    if (!fs64_resolve_base_node(
            base_capability_handle,
            FS64_RIGHT_DELETE,
            owner_id,
            &base_node_id,
            &runtime_generation,
            &runtime_token,
            &parent_handle))
    {
        return 0u;
    }

    (void)runtime_generation;
    (void)runtime_token;
    (void)parent_handle;
    if (!fs64_load_path_argument(path_address, trusted_path, path_byte_count))
    {
        return 0u;
    }

    return ramfs_delete(base_node_id, g_path_scratch, path_byte_count) ? 1u : fs64_deny();
}

u32 fs64_delete(
    u32 base_capability_handle,
    u64 path_address,
    u32 path_byte_count,
    u32 owner_id)
{
    return fs64_delete_common(base_capability_handle, path_address, 0, path_byte_count, owner_id);
}

u32 fs64_delete_kernel(
    u32 base_capability_handle,
    const u8 *path,
    u32 path_byte_count,
    u32 owner_id)
{
    if (path == 0)
    {
        return 0u;
    }

    return fs64_delete_common(base_capability_handle, 0u, path, path_byte_count, owner_id);
}

u32 fs64_node_rights(u32 node_capability_handle, u32 owner_id)
{
    struct fs64_node_capability *record;

    fs64_ensure_init();
    record = fs64_resolve_node_capability(node_capability_handle, 0u, owner_id);
    return (record != 0) ? record->rights : 0u;
}

u32 fs64_node_owner(u32 node_capability_handle, u32 owner_id)
{
    struct fs64_node_capability *record;

    fs64_ensure_init();
    record = fs64_resolve_node_capability(node_capability_handle, 0u, owner_id);
    return (record != 0) ? record->owner_id : 0u;
}

u32 fs64_live_count(void)
{
    u32 index;
    u32 count = 0u;

    fs64_ensure_init();
    for (index = 0u; index < FS64_TABLE_LIMIT; ++index)
    {
        if (g_fs64_caps[index].active != 0u)
        {
            ++count;
        }
    }

    return count;
}

u32 fs64_open_count(void)
{
    return g_open_count;
}

u32 fs64_create_count(void)
{
    return g_create_count;
}

u32 fs64_list_count(void)
{
    return g_list_count;
}

u32 fs64_read_count(void)
{
    return g_read_count;
}

u32 fs64_write_count(void)
{
    return g_write_count;
}

u32 fs64_stat_count(void)
{
    return g_stat_count;
}

u32 fs64_revoke_count(void)
{
    return g_revoke_count;
}

u32 fs64_denial_count(void)
{
    return g_denial_count;
}

u32 fs64_stale_denial_count(void)
{
    return g_stale_denial_count;
}
