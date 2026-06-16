#ifndef LIMITLESS_BOOT_MEDIA_X64_H
#define LIMITLESS_BOOT_MEDIA_X64_H

#include "boot_info.h"
#include "types.h"

#define BOOT_MEDIA64_ERROR_NONE 0u
#define BOOT_MEDIA64_ERROR_UNAVAILABLE 1u
#define BOOT_MEDIA64_ERROR_NOT_FOUND 2u
#define BOOT_MEDIA64_ERROR_CAPACITY 3u
#define BOOT_MEDIA64_ERROR_ARGUMENT 4u

void boot_media64_init(const struct boot_info *boot_info);
u32 boot_media64_available(void);
u32 boot_media64_has_file(const u8 *path, u32 path_bytes);
u32 boot_media64_read_file(const u8 *path, u32 path_bytes, u8 *buffer, u32 capacity, u32 *bytes_read);
u32 boot_media64_last_error(void);
u32 boot_media64_last_bytes(void);
u32 boot_media64_last_capacity(void);
u32 boot_media64_app_bytes(void);
u32 boot_media64_interp_bytes(void);
u32 boot_media64_flags(void);
u32 boot_media64_status(void);

#endif
