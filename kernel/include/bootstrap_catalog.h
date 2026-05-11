#ifndef LIMITLESS_BOOTSTRAP_CATALOG_H
#define LIMITLESS_BOOTSTRAP_CATALOG_H

#include "types.h"
#include "package_store_generated.h"

enum
{
    LIMITLESS_BOOTSTRAP_CATALOG_MAGIC = 0x504B4753u,
    LIMITLESS_BOOTSTRAP_CATALOG_VERSION = 2u,
    LIMITLESS_BOOTSTRAP_CATALOG_HEADER_SIZE = 28u,
    LIMITLESS_BOOTSTRAP_CATALOG_MAGIC_OFFSET = 0u,
    LIMITLESS_BOOTSTRAP_CATALOG_VERSION_OFFSET = 4u,
    LIMITLESS_BOOTSTRAP_CATALOG_SIGNER_COUNT_OFFSET = 8u,
    LIMITLESS_BOOTSTRAP_CATALOG_MANIFEST_COUNT_OFFSET = 12u,
    LIMITLESS_BOOTSTRAP_CATALOG_PAYLOAD_COUNT_OFFSET = 16u,
    LIMITLESS_BOOTSTRAP_CATALOG_STRING_BYTES_OFFSET = 20u,
    LIMITLESS_BOOTSTRAP_CATALOG_CHECKSUM_OFFSET = 24u
};

struct bootstrap_catalog_summary
{
    u32 magic;
    u32 version;
    u32 signer_count;
    u32 manifest_count;
    u32 payload_count;
    u32 string_bytes;
    u32 archive_checksum;
    u32 archive_size;
};

static inline u32 bootstrap_catalog_read_u32(const u8 *address)
{
    return (u32)address[0]
        | ((u32)address[1] << 8u)
        | ((u32)address[2] << 16u)
        | ((u32)address[3] << 24u);
}

static inline int bootstrap_catalog_read_summary(struct bootstrap_catalog_summary *out_summary)
{
    const u8 *archive = package_store_generated_archive;

    if (out_summary == NULL)
    {
        return 0;
    }

    if (PACKAGE_STORE_GENERATED_ARCHIVE_SIZE < LIMITLESS_BOOTSTRAP_CATALOG_HEADER_SIZE)
    {
        return 0;
    }

    out_summary->magic = bootstrap_catalog_read_u32(archive + LIMITLESS_BOOTSTRAP_CATALOG_MAGIC_OFFSET);
    out_summary->version = bootstrap_catalog_read_u32(archive + LIMITLESS_BOOTSTRAP_CATALOG_VERSION_OFFSET);
    out_summary->signer_count = bootstrap_catalog_read_u32(archive + LIMITLESS_BOOTSTRAP_CATALOG_SIGNER_COUNT_OFFSET);
    out_summary->manifest_count = bootstrap_catalog_read_u32(archive + LIMITLESS_BOOTSTRAP_CATALOG_MANIFEST_COUNT_OFFSET);
    out_summary->payload_count = bootstrap_catalog_read_u32(archive + LIMITLESS_BOOTSTRAP_CATALOG_PAYLOAD_COUNT_OFFSET);
    out_summary->string_bytes = bootstrap_catalog_read_u32(archive + LIMITLESS_BOOTSTRAP_CATALOG_STRING_BYTES_OFFSET);
    out_summary->archive_checksum = bootstrap_catalog_read_u32(archive + LIMITLESS_BOOTSTRAP_CATALOG_CHECKSUM_OFFSET);
    out_summary->archive_size = PACKAGE_STORE_GENERATED_ARCHIVE_SIZE;

    return 1;
}

static inline int bootstrap_catalog_is_valid(const struct bootstrap_catalog_summary *summary)
{
    if (summary == NULL)
    {
        return 0;
    }

    return (summary->magic == LIMITLESS_BOOTSTRAP_CATALOG_MAGIC)
        && (summary->version == LIMITLESS_BOOTSTRAP_CATALOG_VERSION)
        && (summary->archive_checksum == PACKAGE_STORE_GENERATED_ARCHIVE_CHECKSUM);
}

#endif
