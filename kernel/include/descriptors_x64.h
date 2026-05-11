#ifndef LIMITLESS_DESCRIPTORS_X64_H
#define LIMITLESS_DESCRIPTORS_X64_H

#include "types.h"

#define DESCRIPTORS64_KERNEL_CODE_SELECTOR 0x0018u
#define DESCRIPTORS64_KERNEL_DATA_SELECTOR 0x0020u
#define DESCRIPTORS64_USER_DATA_SELECTOR 0x002Bu
#define DESCRIPTORS64_USER_CODE_SELECTOR 0x0033u
#define DESCRIPTORS64_TSS_SELECTOR 0x0038u

#define DESCRIPTORS64_STATE_GDT_INSTALLED 0x00000001u
#define DESCRIPTORS64_STATE_TSS_PRESENT 0x00000002u
#define DESCRIPTORS64_STATE_TSS_LOADED 0x00000004u
#define DESCRIPTORS64_STATE_KERNEL_CODE_ACTIVE 0x00000008u
#define DESCRIPTORS64_STATE_KERNEL_DATA_ACTIVE 0x00000010u
#define DESCRIPTORS64_STATE_USER_SELECTORS_PRESENT 0x00000020u
#define DESCRIPTORS64_STATE_SYSCALL_PAIR_READY 0x00000040u

void descriptors64_init(void);
u32 descriptors64_state(void);
u32 descriptors64_gdt_token(void);
u32 descriptors64_tss_token(void);
u32 descriptors64_installed(void);
u32 descriptors64_tss_loaded(void);
u32 descriptors64_user_selectors_ready(void);
u16 descriptors64_kernel_code_selector(void);
u16 descriptors64_kernel_data_selector(void);
u16 descriptors64_user_code_selector(void);
u16 descriptors64_user_data_selector(void);
u16 descriptors64_tss_selector(void);
u16 descriptors64_sysret_selector_base(void);
u64 descriptors64_tss_rsp0(void);
u64 descriptors64_syscall_star_plan(void);

#endif
