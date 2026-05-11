#include "paging.h"

#include "klog.h"
#include "memory.h"
#include "x86.h"

enum
{
    PAGE_SIZE = 4096u,
    PAGE_DIRECTORY_ENTRIES = 1024u,
    PAGE_TABLE_ENTRIES = 1024u,
    PAGE_TABLE_SPAN = PAGE_SIZE * PAGE_TABLE_ENTRIES,
    USER_PDE_INDEX = PAGING_USER_BASE / PAGE_TABLE_SPAN,
    USER_CODE_PTE_INDEX = 0u,
    USER_STACK_PTE_INDEX = 15u,
    PAGING_FLAG_PRESENT = 0x001u,
    PAGING_FLAG_WRITABLE = 0x002u,
    PAGING_FLAG_USER = 0x004u,
    PAGING_MAX_IDENTITY_BYTES = 64u * 1024u * 1024u
};

static u32 *page_directory = NULL;
static u32 *page_tables = NULL;
static u32 mapped_bytes = 0;
static u32 kernel_table_count = 0;
static int paging_enabled = 0;

static u32 align_up(u32 value, u32 alignment)
{
    u32 remainder;

    if (alignment <= 1)
    {
        return value;
    }

    remainder = value % alignment;
    if (remainder == 0)
    {
        return value;
    }

    return value + alignment - remainder;
}

void paging_init(u32 requested_identity_bytes)
{
    u32 table_count;
    u32 page_index;
    u32 table_index;
    u32 table_offset;
    u32 physical_address = 0;

    if (requested_identity_bytes < PAGE_TABLE_SPAN)
    {
        requested_identity_bytes = PAGE_TABLE_SPAN;
    }

    requested_identity_bytes = align_up(requested_identity_bytes, PAGE_SIZE);
    if (requested_identity_bytes > PAGING_MAX_IDENTITY_BYTES)
    {
        requested_identity_bytes = PAGING_MAX_IDENTITY_BYTES;
    }

    table_count = align_up(requested_identity_bytes, PAGE_TABLE_SPAN) / PAGE_TABLE_SPAN;
    kernel_table_count = table_count;
    page_directory = (u32 *)memory_early_alloc(PAGE_SIZE, PAGE_SIZE);
    page_tables = (u32 *)memory_early_alloc(table_count * PAGE_SIZE, PAGE_SIZE);

    if ((page_directory == NULL) || (page_tables == NULL))
    {
        klog_write_line("[paging] failed to allocate page tables");
        cpu_cli();
        cpu_halt_forever();
    }

    for (page_index = 0; page_index < PAGE_DIRECTORY_ENTRIES; ++page_index)
    {
        page_directory[page_index] = 0;
    }

    for (table_index = 0; table_index < table_count; ++table_index)
    {
        u32 *table = page_tables + (table_index * PAGE_TABLE_ENTRIES);

        for (table_offset = 0; table_offset < PAGE_TABLE_ENTRIES; ++table_offset)
        {
            table[table_offset] = physical_address | PAGING_FLAG_PRESENT | PAGING_FLAG_WRITABLE;
            physical_address += PAGE_SIZE;
        }

        page_directory[table_index] = ((u32)table) | PAGING_FLAG_PRESENT | PAGING_FLAG_WRITABLE;
    }

    write_cr3((u32)page_directory);
    write_cr0(read_cr0() | 0x80000000u);
    mapped_bytes = table_count * PAGE_TABLE_SPAN;
    paging_enabled = 1;
}

int paging_mark_user_page(u32 virtual_address)
{
    u32 table_index;
    u32 entry_index;
    u32 *table;

    if ((page_directory == NULL) || (page_tables == NULL))
    {
        return -1;
    }

    if (virtual_address >= mapped_bytes)
    {
        return -1;
    }

    table_index = virtual_address / PAGE_TABLE_SPAN;
    entry_index = (virtual_address / PAGE_SIZE) % PAGE_TABLE_ENTRIES;
    table = page_tables + (table_index * PAGE_TABLE_ENTRIES);

    table[entry_index] |= PAGING_FLAG_USER | PAGING_FLAG_WRITABLE | PAGING_FLAG_PRESENT;
    page_directory[table_index] |= PAGING_FLAG_USER | PAGING_FLAG_WRITABLE | PAGING_FLAG_PRESENT;
    invlpg((const void *)virtual_address);
    return 0;
}

u32 paging_create_user_space(const u32 *code_pages, u32 code_page_count, u32 stack_physical)
{
    u32 *directory;
    u32 *user_table;
    u32 index;
    u32 directory_frame;
    u32 user_table_frame;

    if ((page_directory == NULL)
        || (code_pages == NULL)
        || (code_page_count == 0u)
        || (code_page_count >= USER_STACK_PTE_INDEX))
    {
        return 0;
    }

    directory_frame = memory_claim_frame();
    user_table_frame = memory_claim_frame();
    if ((directory_frame == 0xFFFFFFFFu) || (user_table_frame == 0xFFFFFFFFu))
    {
        if (directory_frame != 0xFFFFFFFFu)
        {
            memory_release_frame(directory_frame);
        }

        if (user_table_frame != 0xFFFFFFFFu)
        {
            memory_release_frame(user_table_frame);
        }

        return 0;
    }

    directory = (u32 *)directory_frame;
    user_table = (u32 *)user_table_frame;

    for (index = 0; index < PAGE_DIRECTORY_ENTRIES; ++index)
    {
        directory[index] = 0;
    }

    for (index = 0; index < kernel_table_count; ++index)
    {
        directory[index] = page_directory[index];
    }

    for (index = 0; index < PAGE_TABLE_ENTRIES; ++index)
    {
        user_table[index] = 0;
    }

    for (index = 0; index < code_page_count; ++index)
    {
        user_table[USER_CODE_PTE_INDEX + index] =
            code_pages[index] | PAGING_FLAG_PRESENT | PAGING_FLAG_WRITABLE | PAGING_FLAG_USER;
    }

    user_table[USER_STACK_PTE_INDEX] = stack_physical | PAGING_FLAG_PRESENT | PAGING_FLAG_WRITABLE | PAGING_FLAG_USER;
    directory[USER_PDE_INDEX] = ((u32)user_table) | PAGING_FLAG_PRESENT | PAGING_FLAG_WRITABLE | PAGING_FLAG_USER;
    return (u32)directory;
}

void paging_destroy_user_space(u32 page_directory_address)
{
    u32 *directory;
    u32 user_table_entry;
    u32 user_table_physical;

    if (page_directory_address == 0u)
    {
        return;
    }

    directory = (u32 *)page_directory_address;
    user_table_entry = directory[USER_PDE_INDEX];
    if ((user_table_entry & PAGING_FLAG_PRESENT) != 0u)
    {
        user_table_physical = user_table_entry & 0xFFFFF000u;
        if (user_table_physical != 0u)
        {
            memory_release_frame(user_table_physical);
        }
    }

    memory_release_frame(page_directory_address);
}

void paging_switch_address_space(u32 page_directory_address)
{
    if (page_directory_address != 0u)
    {
        write_cr3(page_directory_address);
    }
}

u32 paging_get_mapped_bytes(void)
{
    return mapped_bytes;
}

u32 paging_get_page_directory_address(void)
{
    return (u32)page_directory;
}

int paging_is_enabled(void)
{
    return paging_enabled;
}
