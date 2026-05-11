#ifndef LIMITLESS_PIC_H
#define LIMITLESS_PIC_H

#include "types.h"

void pic_initialize(u8 master_mask, u8 slave_mask);
void pic_disable(void);
void pic_send_eoi(u8 irq);

#endif
