#include "ramfs.h"

enum
{
    RAMFS_NODE_ROOT = 1u,
    RAMFS_NODE_README = 2u,
    RAMFS_NODE_SERVICES = 3u,
    RAMFS_NODE_APPS = 5u,
    RAMFS_NODE_HELLO = 6u,
    RAMFS_NODE_ECHO_APP = 7u,
    RAMFS_NODE_LS_APP = 8u,
    RAMFS_NODE_CAT_APP = 9u,
    RAMFS_NODE_STAT_APP = 10u,
    RAMFS_NODE_MKDIR_APP = 11u,
    RAMFS_NODE_WRITE_APP = 12u,
    RAMFS_NODE_RENAME_APP = 13u,
    RAMFS_NODE_MOVE_APP = 14u,
    RAMFS_NODE_APPEND_APP = 15u,
    RAMFS_NODE_DELETE_APP = 16u,
    RAMFS_NODE_SAY_APP = 17u,
    RAMFS_NODE_SHOW_APP = 18u,
    RAMFS_NODE_LIST_APP = 19u,
    RAMFS_NODE_MAKE_APP = 20u,
    RAMFS_NODE_PUT_APP = 21u,
    RAMFS_NODE_SWAP_APP = 22u,
    RAMFS_NODE_SHIFT_APP = 23u,
    RAMFS_NODE_ASK_APP = 24u,
    RAMFS_NODE_TOUCH_APP = 25u,
    RAMFS_NODE_COPY_APP = 26u,
    RAMFS_NODE_APPS_INDEX = 27u,
    RAMFS_NODE_LIMIT = 64u,
    RAMFS_NAME_CAPACITY = 32u,
    RAMFS_FILE_CAPACITY = 256u,
    RAMFS_LIST_ERROR = 0xFFFFFFFFu
};

struct ramfs_node
{
    u32 id;
    u32 parent_id;
    u32 type;
    u8 active;
    char name[RAMFS_NAME_CAPACITY];
    u8 contents[RAMFS_FILE_CAPACITY];
    u32 length;
};

struct ramfs_seed_node
{
    u32 id;
    u32 parent_id;
    u32 type;
    const char *name;
    const u8 *contents;
    u32 length;
};

static const u8 README_CONTENT[] =
    "LimitlessOS bootstrap ramfs\n"
    "This userspace shell is reading files through capability-checked handles.\n";

static const u8 SERVICES_CONTENT[] =
    "services:\n"
    "- ai-policy\n"
    "- console\n"
    "- input\n"
    "- ramfs\n";

static const u8 HELLO_CONTENT[] =
    "hello from /APPS/HELLO.TXT\n";

static const u8 APPS_INDEX_CONTENT[] =
    "LimitlessOS /APPS index\n"
    "*.APP files are launcher descriptors.\n"
    "Use 'apps', 'help <command>', or 'info <command>' to inspect them.\n";

static const u8 ECHO_APP_CONTENT[] =
    "12\n1\n1\n3\n"
    "echo <text> - print text through delegated console\n"
    "text\n";

static const u8 LS_APP_CONTENT[] =
    "3\n3\n2\n3\n"
    "ls [path] - list directory entries from cwd or a given path\n"
    "filesystem\n";

static const u8 CAT_APP_CONTENT[] =
    "4\n3\n2\n3\n"
    "cat <path> - print file contents\n"
    "filesystem\n";

static const u8 STAT_APP_CONTENT[] =
    "7\n3\n2\n3\n"
    "stat <path> - show file or directory metadata\n"
    "filesystem\n";

static const u8 MKDIR_APP_CONTENT[] =
    "5\n3\n2\n1\n"
    "mkdir <path> - create a directory tree\n"
    "filesystem\n";

static const u8 WRITE_APP_CONTENT[] =
    "6\n11\n3\n1\n"
    "write <path> <text> - replace file contents\n"
    "filesystem\n";

static const u8 RENAME_APP_CONTENT[] =
    "8\n19\n4\n1\n"
    "rename <from> <to> - rename within one directory\n"
    "filesystem\n";

static const u8 MOVE_APP_CONTENT[] =
    "11\n23\n5\n1\n"
    "move <source> <dest> - move across directories\n"
    "filesystem\n";

static const u8 APPEND_APP_CONTENT[] =
    "9\n11\n3\n1\n"
    "append <path> <text> - append text to a file\n"
    "filesystem\n";

static const u8 DELETE_APP_CONTENT[] =
    "10\n3\n2\n1\n"
    "delete <path> - remove a file or empty directory\n"
    "filesystem\n";

static const u8 SAY_APP_CONTENT[] =
    "12\n1\n1\n3\n"
    "say <text> - alias for echo\n"
    "aliases\n";

static const u8 SHOW_APP_CONTENT[] =
    "4\n3\n2\n3\n"
    "show <path> - alias for cat\n"
    "aliases\n";

static const u8 LIST_APP_CONTENT[] =
    "3\n3\n2\n3\n"
    "list [path] - alias for ls\n"
    "aliases\n";

static const u8 MAKE_APP_CONTENT[] =
    "5\n3\n2\n1\n"
    "make <path> - alias for mkdir\n"
    "aliases\n";

static const u8 PUT_APP_CONTENT[] =
    "6\n11\n3\n1\n"
    "put <path> <text> - alias for write\n"
    "aliases\n";

static const u8 SWAP_APP_CONTENT[] =
    "8\n19\n4\n1\n"
    "swap <from> <to> - alias for rename\n"
    "aliases\n";

static const u8 SHIFT_APP_CONTENT[] =
    "11\n23\n5\n1\n"
    "shift <source> <dest> - alias for move\n"
    "aliases\n";

static const u8 ASK_APP_CONTENT[] =
    "13\n1\n1\n7\n"
    "ask <prompt> - print a prompt and read delegated input\n"
    "interactive\n";

static const u8 TOUCH_APP_CONTENT[] =
    "14\n3\n2\n1\n"
    "touch <path> - create an empty file if missing\n"
    "filesystem\n";

static const u8 COPY_APP_CONTENT[] =
    "15\n23\n5\n1\n"
    "copy <source> <dest> - copy a file across directories\n"
    "filesystem\n";

static const struct ramfs_seed_node seed_nodes[] = {
    { RAMFS_NODE_ROOT, 0u, RAMFS_NODE_DIRECTORY, "/", NULL, 0u },
    { RAMFS_NODE_README, RAMFS_NODE_ROOT, RAMFS_NODE_FILE, "README.TXT", README_CONTENT, sizeof(README_CONTENT) - 1u },
    { RAMFS_NODE_SERVICES, RAMFS_NODE_ROOT, RAMFS_NODE_FILE, "SERVICES.TXT", SERVICES_CONTENT, sizeof(SERVICES_CONTENT) - 1u },
    { RAMFS_NODE_APPS, RAMFS_NODE_ROOT, RAMFS_NODE_DIRECTORY, "APPS", NULL, 0u },
    { RAMFS_NODE_HELLO, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "HELLO.TXT", HELLO_CONTENT, sizeof(HELLO_CONTENT) - 1u },
    { RAMFS_NODE_ECHO_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "ECHO.APP", ECHO_APP_CONTENT, sizeof(ECHO_APP_CONTENT) - 1u },
    { RAMFS_NODE_LS_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "LS.APP", LS_APP_CONTENT, sizeof(LS_APP_CONTENT) - 1u },
    { RAMFS_NODE_CAT_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "CAT.APP", CAT_APP_CONTENT, sizeof(CAT_APP_CONTENT) - 1u },
    { RAMFS_NODE_STAT_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "STAT.APP", STAT_APP_CONTENT, sizeof(STAT_APP_CONTENT) - 1u },
    { RAMFS_NODE_MKDIR_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "MKDIR.APP", MKDIR_APP_CONTENT, sizeof(MKDIR_APP_CONTENT) - 1u },
    { RAMFS_NODE_WRITE_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "WRITE.APP", WRITE_APP_CONTENT, sizeof(WRITE_APP_CONTENT) - 1u },
    { RAMFS_NODE_RENAME_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "RENAME.APP", RENAME_APP_CONTENT, sizeof(RENAME_APP_CONTENT) - 1u },
    { RAMFS_NODE_MOVE_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "MOVE.APP", MOVE_APP_CONTENT, sizeof(MOVE_APP_CONTENT) - 1u },
    { RAMFS_NODE_APPEND_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "APPEND.APP", APPEND_APP_CONTENT, sizeof(APPEND_APP_CONTENT) - 1u },
    { RAMFS_NODE_DELETE_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "DELETE.APP", DELETE_APP_CONTENT, sizeof(DELETE_APP_CONTENT) - 1u },
    { RAMFS_NODE_SAY_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "SAY.APP", SAY_APP_CONTENT, sizeof(SAY_APP_CONTENT) - 1u },
    { RAMFS_NODE_SHOW_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "SHOW.APP", SHOW_APP_CONTENT, sizeof(SHOW_APP_CONTENT) - 1u },
    { RAMFS_NODE_LIST_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "LIST.APP", LIST_APP_CONTENT, sizeof(LIST_APP_CONTENT) - 1u },
    { RAMFS_NODE_MAKE_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "MAKE.APP", MAKE_APP_CONTENT, sizeof(MAKE_APP_CONTENT) - 1u },
    { RAMFS_NODE_PUT_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "PUT.APP", PUT_APP_CONTENT, sizeof(PUT_APP_CONTENT) - 1u },
    { RAMFS_NODE_SWAP_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "SWAP.APP", SWAP_APP_CONTENT, sizeof(SWAP_APP_CONTENT) - 1u },
    { RAMFS_NODE_SHIFT_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "SHIFT.APP", SHIFT_APP_CONTENT, sizeof(SHIFT_APP_CONTENT) - 1u },
    { RAMFS_NODE_ASK_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "ASK.APP", ASK_APP_CONTENT, sizeof(ASK_APP_CONTENT) - 1u },
    { RAMFS_NODE_TOUCH_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "TOUCH.APP", TOUCH_APP_CONTENT, sizeof(TOUCH_APP_CONTENT) - 1u },
    { RAMFS_NODE_COPY_APP, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "COPY.APP", COPY_APP_CONTENT, sizeof(COPY_APP_CONTENT) - 1u },
    { RAMFS_NODE_APPS_INDEX, RAMFS_NODE_APPS, RAMFS_NODE_FILE, "INDEX.TXT", APPS_INDEX_CONTENT, sizeof(APPS_INDEX_CONTENT) - 1u }
};

static struct ramfs_node nodes[RAMFS_NODE_LIMIT];

static u32 text_length(const char *text)
{
    u32 length = 0u;

    if (text == NULL)
    {
        return 0u;
    }

    while (text[length] != '\0')
    {
        ++length;
    }

    return length;
}

static void bytes_zero(u8 *bytes, u32 length)
{
    u32 index;

    if (bytes == NULL)
    {
        return;
    }

    for (index = 0u; index < length; ++index)
    {
        bytes[index] = 0u;
    }
}

static void bytes_copy(u8 *destination, const u8 *source, u32 length)
{
    u32 index;

    if ((destination == NULL) || (source == NULL))
    {
        return;
    }

    for (index = 0u; index < length; ++index)
    {
        destination[index] = source[index];
    }
}

static void text_copy(char *destination, u32 destination_capacity, const char *source)
{
    u32 index = 0u;

    if ((destination == NULL) || (destination_capacity == 0u))
    {
        return;
    }

    while ((index + 1u) < destination_capacity
        && (source != NULL)
        && (source[index] != '\0'))
    {
        destination[index] = source[index];
        ++index;
    }

    destination[index] = '\0';
}

static int bytes_append(
    u8 *destination_bytes,
    u32 byte_capacity,
    u32 *written_out,
    const u8 *source_bytes,
    u32 source_length)
{
    u32 index;

    if ((destination_bytes == NULL)
        || (written_out == NULL)
        || ((source_length != 0u) && (source_bytes == NULL)))
    {
        return 0;
    }

    if ((*written_out + source_length) > byte_capacity)
    {
        return 0;
    }

    for (index = 0u; index < source_length; ++index)
    {
        destination_bytes[*written_out + index] = source_bytes[index];
    }

    *written_out += source_length;
    return 1;
}

static int text_append(
    u8 *destination_bytes,
    u32 byte_capacity,
    u32 *written_out,
    const char *source_text)
{
    return bytes_append(
        destination_bytes,
        byte_capacity,
        written_out,
        (const u8 *)source_text,
        text_length(source_text));
}

static int decimal_append(
    u8 *destination_bytes,
    u32 byte_capacity,
    u32 *written_out,
    u32 value)
{
    u8 digits[10];
    u32 digit_count = 0u;
    u32 index;

    if (value == 0u)
    {
        digits[digit_count++] = (u8)'0';
    }
    else
    {
        while (value != 0u)
        {
            digits[digit_count++] = (u8)('0' + (value % 10u));
            value /= 10u;
        }
    }

    for (index = 0u; index < (digit_count / 2u); ++index)
    {
        u8 swap = digits[index];
        digits[index] = digits[digit_count - index - 1u];
        digits[digit_count - index - 1u] = swap;
    }

    return bytes_append(destination_bytes, byte_capacity, written_out, digits, digit_count);
}

static int text_equals_segment(const char *text, const u8 *segment, u32 segment_length)
{
    u32 index;

    if (text == NULL)
    {
        return 0;
    }

    for (index = 0u; index < segment_length; ++index)
    {
        if ((u8)text[index] != segment[index])
        {
            return 0;
        }
    }

    return text[segment_length] == '\0';
}

static struct ramfs_node *ramfs_find_node(u32 node_id)
{
    u32 index;

    for (index = 0u; index < RAMFS_NODE_LIMIT; ++index)
    {
        if (nodes[index].active && (nodes[index].id == node_id))
        {
            return &nodes[index];
        }
    }

    return NULL;
}

static struct ramfs_node *ramfs_find_child(
    const struct ramfs_node *parent,
    const u8 *segment,
    u32 segment_length)
{
    u32 index;

    if ((parent == NULL) || (segment == NULL) || (segment_length == 0u))
    {
        return NULL;
    }

    for (index = 0u; index < RAMFS_NODE_LIMIT; ++index)
    {
        if (nodes[index].active
            && (nodes[index].parent_id == parent->id)
            && text_equals_segment(nodes[index].name, segment, segment_length))
        {
            return &nodes[index];
        }
    }

    return NULL;
}

static u32 ramfs_child_count(const struct ramfs_node *parent)
{
    u32 index;
    u32 count = 0u;

    if ((parent == NULL) || (parent->type != RAMFS_NODE_DIRECTORY))
    {
        return 0u;
    }

    for (index = 0u; index < RAMFS_NODE_LIMIT; ++index)
    {
        if (nodes[index].active && (nodes[index].parent_id == parent->id))
        {
            ++count;
        }
    }

    return count;
}

static int ramfs_parent_chain_contains(u32 node_id, u32 ancestor_id)
{
    while (node_id != 0u)
    {
        struct ramfs_node *node;

        if (node_id == ancestor_id)
        {
            return 1;
        }

        node = ramfs_find_node(node_id);
        if (node == NULL)
        {
            break;
        }

        node_id = node->parent_id;
    }

    return 0;
}

static struct ramfs_node *ramfs_allocate_node(void)
{
    u32 index;

    for (index = 0u; index < RAMFS_NODE_LIMIT; ++index)
    {
        if (!nodes[index].active)
        {
            bytes_zero((u8 *)&nodes[index], sizeof(nodes[index]));
            nodes[index].active = 1u;
            nodes[index].id = index + 1u;
            return &nodes[index];
        }
    }

    return NULL;
}

static struct ramfs_node *ramfs_create_child(
    struct ramfs_node *parent,
    const u8 *name_bytes,
    u32 name_length,
    u32 node_type)
{
    struct ramfs_node *node;
    u32 index;

    if ((parent == NULL)
        || (name_bytes == NULL)
        || (name_length == 0u)
        || (name_length >= RAMFS_NAME_CAPACITY)
        || (node_type == RAMFS_NODE_NONE))
    {
        return NULL;
    }

    node = ramfs_allocate_node();
    if (node == NULL)
    {
        return NULL;
    }

    node->parent_id = parent->id;
    node->type = node_type;
    for (index = 0u; index < name_length; ++index)
    {
        node->name[index] = (char)name_bytes[index];
    }

    node->name[name_length] = '\0';
    node->length = 0u;
    return node;
}

static void ramfs_seed_node_copy(const struct ramfs_seed_node *seed)
{
    struct ramfs_node *node;

    if (seed == NULL)
    {
        return;
    }

    node = ramfs_find_node(seed->id);
    if (node == NULL)
    {
        node = ramfs_allocate_node();
        if (node == NULL)
        {
            return;
        }
    }

    bytes_zero((u8 *)node, sizeof(*node));
    node->active = 1u;
    node->id = seed->id;
    node->parent_id = seed->parent_id;
    node->type = seed->type;
    text_copy(node->name, RAMFS_NAME_CAPACITY, seed->name);
    if ((seed->contents != NULL) && (seed->length <= RAMFS_FILE_CAPACITY))
    {
        bytes_copy(node->contents, seed->contents, seed->length);
        node->length = seed->length;
    }
}

static int ramfs_walk_parent(
    u32 base_node_id,
    const u8 *path_bytes,
    u32 path_length,
    struct ramfs_node **parent_out,
    const u8 **leaf_name_out,
    u32 *leaf_length_out)
{
    struct ramfs_node *current;
    u32 offset = 0u;

    if (parent_out != NULL)
    {
        *parent_out = NULL;
    }

    if (leaf_name_out != NULL)
    {
        *leaf_name_out = NULL;
    }

    if (leaf_length_out != NULL)
    {
        *leaf_length_out = 0u;
    }

    if ((path_bytes == NULL) || (path_length == 0u))
    {
        return 0;
    }

    current = ramfs_find_node(base_node_id);
    if ((current == NULL) || (current->type != RAMFS_NODE_DIRECTORY))
    {
        return 0;
    }

    if (path_bytes[0] == '/')
    {
        if (current->id != RAMFS_NODE_ROOT)
        {
            return 0;
        }

        current = ramfs_find_node(RAMFS_NODE_ROOT);
        while ((offset < path_length) && (path_bytes[offset] == '/'))
        {
            ++offset;
        }
    }

    while (offset < path_length)
    {
        const u8 *segment;
        u32 segment_length;
        u32 next_offset;
        struct ramfs_node *child;

        while ((offset < path_length) && (path_bytes[offset] == '/'))
        {
            ++offset;
        }

        if (offset >= path_length)
        {
            break;
        }

        segment = &path_bytes[offset];
        while ((offset < path_length) && (path_bytes[offset] != '/'))
        {
            ++offset;
        }

        segment_length = (u32)(&path_bytes[offset] - segment);
        next_offset = offset;
        while ((next_offset < path_length) && (path_bytes[next_offset] == '/'))
        {
            ++next_offset;
        }

        if ((segment_length == 1u) && (segment[0] == '.'))
        {
            offset = next_offset;
            continue;
        }

        if (next_offset >= path_length)
        {
            if (parent_out != NULL)
            {
                *parent_out = current;
            }

            if (leaf_name_out != NULL)
            {
                *leaf_name_out = segment;
            }

            if (leaf_length_out != NULL)
            {
                *leaf_length_out = segment_length;
            }

            return segment_length != 0u;
        }

        child = ramfs_find_child(current, segment, segment_length);
        if ((child == NULL) || (child->type != RAMFS_NODE_DIRECTORY))
        {
            return 0;
        }

        current = child;
        offset = next_offset;
    }

    return 0;
}

void ramfs_init(void)
{
    u32 index;

    bytes_zero((u8 *)nodes, sizeof(nodes));
    for (index = 0u; index < (sizeof(seed_nodes) / sizeof(seed_nodes[0])); ++index)
    {
        ramfs_seed_node_copy(&seed_nodes[index]);
    }
}

u32 ramfs_root_node(void)
{
    return RAMFS_NODE_ROOT;
}

int ramfs_node_exists(u32 node_id)
{
    return ramfs_find_node(node_id) != NULL;
}

int ramfs_node_is_directory(u32 node_id)
{
    struct ramfs_node *node = ramfs_find_node(node_id);
    return (node != NULL) && (node->type == RAMFS_NODE_DIRECTORY);
}

const char *ramfs_node_name(u32 node_id)
{
    struct ramfs_node *node = ramfs_find_node(node_id);
    return (node == NULL) ? "unknown-node" : node->name;
}

int ramfs_open(u32 base_node_id, const u8 *path_bytes, u32 path_length, u32 *node_id_out)
{
    struct ramfs_node *current;
    u32 offset = 0u;

    if (node_id_out != NULL)
    {
        *node_id_out = 0u;
    }

    current = ramfs_find_node(base_node_id);
    if ((current == NULL) || (current->type != RAMFS_NODE_DIRECTORY))
    {
        return 0;
    }

    if ((path_bytes == NULL) && (path_length != 0u))
    {
        return 0;
    }

    if ((path_length != 0u) && (path_bytes[0] == '/'))
    {
        if (current->id != RAMFS_NODE_ROOT)
        {
            return 0;
        }

        current = ramfs_find_node(RAMFS_NODE_ROOT);
        while ((offset < path_length) && (path_bytes[offset] == '/'))
        {
            ++offset;
        }
    }

    while (offset < path_length)
    {
        u32 segment_start = offset;
        u32 segment_length;
        struct ramfs_node *child;

        while ((offset < path_length) && (path_bytes[offset] != '/'))
        {
            ++offset;
        }

        segment_length = offset - segment_start;
        if ((segment_length == 1u) && (path_bytes[segment_start] == '.'))
        {
            while ((offset < path_length) && (path_bytes[offset] == '/'))
            {
                ++offset;
            }

            continue;
        }

        child = ramfs_find_child(current, &path_bytes[segment_start], segment_length);
        if (child == NULL)
        {
            return 0;
        }

        current = child;
        while ((offset < path_length) && (path_bytes[offset] == '/'))
        {
            ++offset;
        }
    }

    if (node_id_out != NULL)
    {
        *node_id_out = current->id;
    }

    return 1;
}

int ramfs_create(
    u32 base_node_id,
    const u8 *path_bytes,
    u32 path_length,
    u32 node_type,
    u32 *node_id_out)
{
    struct ramfs_node *parent;
    struct ramfs_node *existing;
    struct ramfs_node *created;
    const u8 *leaf_name;
    u32 leaf_length;

    if (node_id_out != NULL)
    {
        *node_id_out = 0u;
    }

    if ((node_type != RAMFS_NODE_DIRECTORY) && (node_type != RAMFS_NODE_FILE))
    {
        return 0;
    }

    if (!ramfs_walk_parent(
            base_node_id,
            path_bytes,
            path_length,
            &parent,
            &leaf_name,
            &leaf_length))
    {
        return 0;
    }

    existing = ramfs_find_child(parent, leaf_name, leaf_length);
    if (existing != NULL)
    {
        if ((existing->type == node_type) && (node_id_out != NULL))
        {
            *node_id_out = existing->id;
        }

        return existing->type == node_type;
    }

    created = ramfs_create_child(parent, leaf_name, leaf_length, node_type);
    if (created == NULL)
    {
        return 0;
    }

    if (node_id_out != NULL)
    {
        *node_id_out = created->id;
    }

    return 1;
}

u32 ramfs_list(u32 node_id, u8 *destination_bytes, u32 byte_capacity)
{
    struct ramfs_node *directory = ramfs_find_node(node_id);
    u32 index;
    u32 written = 0u;

    if ((directory == NULL)
        || (directory->type != RAMFS_NODE_DIRECTORY)
        || (destination_bytes == NULL)
        || (byte_capacity == 0u))
    {
        return RAMFS_LIST_ERROR;
    }

    for (index = 0u; index < RAMFS_NODE_LIMIT; ++index)
    {
        u32 name_length;
        u32 copy_index;

        if (!nodes[index].active || (nodes[index].parent_id != directory->id))
        {
            continue;
        }

        name_length = text_length(nodes[index].name);
        if ((written + name_length + 1u) > byte_capacity)
        {
            break;
        }

        for (copy_index = 0u; copy_index < name_length; ++copy_index)
        {
            destination_bytes[written++] = (u8)nodes[index].name[copy_index];
        }

        if (nodes[index].type == RAMFS_NODE_DIRECTORY)
        {
            if (written >= byte_capacity)
            {
                break;
            }

            destination_bytes[written++] = (u8)'/';
        }

        if (written >= byte_capacity)
        {
            break;
        }

        destination_bytes[written++] = (u8)'\n';
    }

    return written;
}

u32 ramfs_read(u32 node_id, u32 file_offset, u8 *destination_bytes, u32 byte_capacity)
{
    struct ramfs_node *file = ramfs_find_node(node_id);
    u32 remaining;
    u32 index;

    if ((file == NULL)
        || (file->type != RAMFS_NODE_FILE)
        || (destination_bytes == NULL)
        || (file_offset > file->length))
    {
        return RAMFS_LIST_ERROR;
    }

    remaining = file->length - file_offset;
    if (remaining > byte_capacity)
    {
        remaining = byte_capacity;
    }

    for (index = 0u; index < remaining; ++index)
    {
        destination_bytes[index] = file->contents[file_offset + index];
    }

    return remaining;
}

u32 ramfs_write(u32 node_id, u32 file_offset, const u8 *source_bytes, u32 byte_count)
{
    struct ramfs_node *file = ramfs_find_node(node_id);
    u32 available_bytes;
    u32 actual_count;
    u32 fill_index;
    u32 copy_index;

    if ((file == NULL)
        || (file->type != RAMFS_NODE_FILE)
        || (source_bytes == NULL)
        || (file_offset > RAMFS_FILE_CAPACITY))
    {
        return RAMFS_LIST_ERROR;
    }

    available_bytes = RAMFS_FILE_CAPACITY - file_offset;
    actual_count = byte_count;
    if (actual_count > available_bytes)
    {
        actual_count = available_bytes;
    }

    if (file_offset > file->length)
    {
        for (fill_index = file->length; fill_index < file_offset; ++fill_index)
        {
            file->contents[fill_index] = 0u;
        }
    }

    for (copy_index = 0u; copy_index < actual_count; ++copy_index)
    {
        file->contents[file_offset + copy_index] = source_bytes[copy_index];
    }

    if ((file_offset + actual_count) > file->length)
    {
        file->length = file_offset + actual_count;
    }

    return actual_count;
}

int ramfs_move(
    u32 source_base_node_id,
    const u8 *source_path_bytes,
    u32 source_path_length,
    u32 destination_base_node_id,
    const u8 *destination_path_bytes,
    u32 destination_path_length)
{
    struct ramfs_node *source_parent;
    struct ramfs_node *destination_parent;
    struct ramfs_node *source_node;
    struct ramfs_node *destination_existing;
    const u8 *source_leaf;
    const u8 *destination_leaf;
    u32 source_leaf_length;
    u32 destination_leaf_length;
    u32 index;

    if (!ramfs_walk_parent(
            source_base_node_id,
            source_path_bytes,
            source_path_length,
            &source_parent,
            &source_leaf,
            &source_leaf_length))
    {
        return 0;
    }

    if (!ramfs_walk_parent(
            destination_base_node_id,
            destination_path_bytes,
            destination_path_length,
            &destination_parent,
            &destination_leaf,
            &destination_leaf_length))
    {
        return 0;
    }

    if ((destination_leaf_length == 0u) || (destination_leaf_length >= RAMFS_NAME_CAPACITY))
    {
        return 0;
    }

    source_node = ramfs_find_child(source_parent, source_leaf, source_leaf_length);
    if (source_node == NULL)
    {
        return 0;
    }

    destination_existing = ramfs_find_child(
        destination_parent,
        destination_leaf,
        destination_leaf_length);
    if (destination_existing != NULL)
    {
        return destination_existing->id == source_node->id;
    }

    if ((source_node->type == RAMFS_NODE_DIRECTORY)
        && ramfs_parent_chain_contains(destination_parent->id, source_node->id))
    {
        return 0;
    }

    source_node->parent_id = destination_parent->id;
    bytes_zero((u8 *)source_node->name, sizeof(source_node->name));
    for (index = 0u; index < destination_leaf_length; ++index)
    {
        source_node->name[index] = (char)destination_leaf[index];
    }

    source_node->name[destination_leaf_length] = '\0';
    return 1;
}

int ramfs_rename(
    u32 base_node_id,
    const u8 *source_path_bytes,
    u32 source_path_length,
    const u8 *destination_path_bytes,
    u32 destination_path_length)
{
    return ramfs_move(
        base_node_id,
        source_path_bytes,
        source_path_length,
        base_node_id,
        destination_path_bytes,
        destination_path_length);
}

int ramfs_delete(
    u32 base_node_id,
    const u8 *path_bytes,
    u32 path_length)
{
    struct ramfs_node *parent;
    struct ramfs_node *target;
    const u8 *leaf_name;
    u32 leaf_length;

    if (!ramfs_walk_parent(
            base_node_id,
            path_bytes,
            path_length,
            &parent,
            &leaf_name,
            &leaf_length))
    {
        return 0;
    }

    target = ramfs_find_child(parent, leaf_name, leaf_length);
    if ((target == NULL) || (target->id == RAMFS_NODE_ROOT))
    {
        return 0;
    }

    if ((target->type == RAMFS_NODE_DIRECTORY) && (ramfs_child_count(target) != 0u))
    {
        return 0;
    }

    bytes_zero((u8 *)target, sizeof(*target));
    return 1;
}

int ramfs_stat(u32 node_id, struct ramfs_stat *stat_out)
{
    struct ramfs_node *node = ramfs_find_node(node_id);

    if ((node == NULL) || (stat_out == NULL))
    {
        return 0;
    }

    bytes_zero((u8 *)stat_out, sizeof(*stat_out));
    stat_out->node_type = node->type;
    stat_out->byte_length = node->length;
    if (node->type == RAMFS_NODE_DIRECTORY)
    {
        stat_out->child_count = ramfs_child_count(node);
    }

    return 1;
}

u32 ramfs_format_stat(const struct ramfs_stat *stat, u8 *destination_bytes, u32 byte_capacity)
{
    u32 written = 0u;

    if ((stat == NULL) || (destination_bytes == NULL) || (byte_capacity == 0u))
    {
        return RAMFS_LIST_ERROR;
    }

    if (stat->node_type == RAMFS_NODE_DIRECTORY)
    {
        if (!text_append(destination_bytes, byte_capacity, &written, "type=dir entries=")
            || !decimal_append(destination_bytes, byte_capacity, &written, stat->child_count)
            || !text_append(destination_bytes, byte_capacity, &written, "\n"))
        {
            return RAMFS_LIST_ERROR;
        }

        return written;
    }

    if (stat->node_type == RAMFS_NODE_FILE)
    {
        if (!text_append(destination_bytes, byte_capacity, &written, "type=file size=")
            || !decimal_append(destination_bytes, byte_capacity, &written, stat->byte_length)
            || !text_append(destination_bytes, byte_capacity, &written, "\n"))
        {
            return RAMFS_LIST_ERROR;
        }

        return written;
    }

    if (!text_append(destination_bytes, byte_capacity, &written, "type=unknown\n"))
    {
        return RAMFS_LIST_ERROR;
    }

    return written;
}
