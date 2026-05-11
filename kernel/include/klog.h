#ifndef LIMITLESS_KLOG_H
#define LIMITLESS_KLOG_H

#include "types.h"

void klog_write_string(const char *text);
void klog_write_bytes(const u8 *bytes, u32 length);
void klog_write_line(const char *text);
void klog_write_dec_u32(u32 value);
void klog_write_hex_u32(u32 value);
void klog_newline(void);

#endif
