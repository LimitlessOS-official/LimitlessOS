#ifndef LIMITLESS_INPUT_H
#define LIMITLESS_INPUT_H

#include "types.h"

void input_init(void);
void input_handle_keyboard_interrupt(void);
u32 input_read(u8 *destination_bytes, u32 byte_capacity);
u32 input_pending_byte_count(void);

#endif
