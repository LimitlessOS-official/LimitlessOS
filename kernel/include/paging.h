#ifndef LIMITLESS_PAGING_H
#define LIMITLESS_PAGING_H

#include "types.h"

#define PAGING_USER_BASE 0x40000000u
#define PAGING_USER_CODE_VIRTUAL PAGING_USER_BASE
#define PAGING_USER_STACK_TOP (PAGING_USER_BASE + 0x00010000u)

void paging_init(u32 requested_identity_bytes);
int paging_mark_user_page(u32 virtual_address);
u32 paging_create_user_space(const u32 *code_pages, u32 code_page_count, u32 stack_physical);
void paging_destroy_user_space(u32 page_directory_address);
void paging_switch_address_space(u32 page_directory_address);
u32 paging_get_mapped_bytes(void);
u32 paging_get_page_directory_address(void);
int paging_is_enabled(void);

#endif
