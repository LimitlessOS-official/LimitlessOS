[bits 16]
[org 0x7C00]

KERNEL_LOAD_SEG equ 0x1000
KERNEL_LOAD_OFFSET equ 0x0000
KERNEL_LOAD_ADDR equ 0x00010000
KERNEL_HIGH_HALF_ADDR equ 0xFFFFFFFF80010000
BOOT_INFO_ADDR equ 0x9000

PML4_ADDR equ 0x00001000
PDPT_ADDR equ 0x00002000
PD_ADDR equ 0x00003000
PDPT_HIGH_ADDR equ 0x00004000

HIGH_HALF_PML4_INDEX equ 511
HIGH_HALF_PDPT_INDEX equ 510

STACK_TOP32 equ 0x00090000
STACK_TOP64 equ 0x0009F000

CODE32_SEG equ 0x08
DATA32_SEG equ 0x10
CODE64_SEG equ 0x18
DATA64_SEG equ 0x20

EFER_MSR equ 0xC0000080
CR0_PG equ 0x80000000
CR0_MP equ 0x00000002
CR0_EM equ 0x00000004
CR4_PAE equ 0x00000020
CR4_OSFXSR equ 0x00000200
CR4_OSXMMEXCPT equ 0x00000400
PAGE_PRESENT equ 0x01
PAGE_WRITABLE equ 0x02
PAGE_LARGE equ 0x80
LARGE_PAGE_BYTES equ 0x00200000
IDENTITY_MAP_BYTES equ 0x01000000
IDENTITY_MAP_ENTRIES equ IDENTITY_MAP_BYTES / LARGE_PAGE_BYTES

BOOT_INFO_MAGIC equ 0x42534F4C
BOOT_INFO_BOOT_DRIVE equ 0x04
BOOT_INFO_CONVENTIONAL_KB equ 0x08
BOOT_INFO_EXTENDED_KB equ 0x0C
BOOT_INFO_KERNEL_LOAD_ADDR equ 0x10
BOOT_INFO_KERNEL_SECTOR_COUNT equ 0x14
BOOT_INFO_ARCH_BITS equ 0x18
BOOT_INFO_BOOT_FLAGS equ 0x1C
BOOT_INFO_PAGE_TABLE_ROOT equ 0x20
BOOT_INFO_IDENTITY_MAP_BYTES equ 0x24

start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    call load_kernel
    call prepare_boot_info

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE32_SEG:protected_mode_entry

prepare_boot_info:
    mov dword [BOOT_INFO_ADDR], BOOT_INFO_MAGIC

    xor eax, eax
    mov al, [boot_drive]
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_BOOT_DRIVE], eax

    int 0x12
    movzx eax, ax
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_CONVENTIONAL_KB], eax

    xor eax, eax
    mov ah, 0x88
    int 0x15
    jc .extended_memory_unavailable
    movzx eax, ax
    jmp .store_extended_memory
.extended_memory_unavailable:
    xor eax, eax
.store_extended_memory:
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_EXTENDED_KB], eax

    mov dword [BOOT_INFO_ADDR + BOOT_INFO_KERNEL_LOAD_ADDR], KERNEL_LOAD_ADDR

    xor eax, eax
    mov ax, [kernel_sector_count]
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_KERNEL_SECTOR_COUNT], eax

    mov dword [BOOT_INFO_ADDR + BOOT_INFO_ARCH_BITS], 64
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_BOOT_FLAGS], 0x0000001F
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_PAGE_TABLE_ROOT], PML4_ADDR
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_IDENTITY_MAP_BYTES], IDENTITY_MAP_BYTES
    ret

load_kernel:
    mov ax, [kernel_sector_count]
    mov [remaining_sectors], ax
    mov word [dap_buffer_segment], KERNEL_LOAD_SEG
    mov word [dap_lba_low], 1

read_kernel_chunk:
    mov ax, [remaining_sectors]
    test ax, ax
    jz load_kernel_done
    cmp ax, 127
    jbe .use_remaining
    mov ax, 127

.use_remaining:
    mov [dap_sector_count], ax
    push ax
    call read_dap
    pop ax

    sub [remaining_sectors], ax
    add [dap_lba_low], ax
    shl ax, 5
    add [dap_buffer_segment], ax
    jmp read_kernel_chunk

load_kernel_done:
    ret

read_dap:
    mov si, disk_address_packet
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc boot_error
    ret

boot_error:
    cli
    hlt
    jmp boot_error

[bits 32]
protected_mode_entry:
    mov ax, DATA32_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, STACK_TOP32

    xor eax, eax
    mov edi, PML4_ADDR
    mov ecx, (4096 * 4) / 4
    rep stosd

    mov dword [PML4_ADDR], PDPT_ADDR | PAGE_PRESENT | PAGE_WRITABLE
    mov dword [PML4_ADDR + (HIGH_HALF_PML4_INDEX * 8)], PDPT_HIGH_ADDR | PAGE_PRESENT | PAGE_WRITABLE
    mov dword [PDPT_ADDR], PD_ADDR | PAGE_PRESENT | PAGE_WRITABLE
    mov dword [PDPT_HIGH_ADDR + (HIGH_HALF_PDPT_INDEX * 8)], PD_ADDR | PAGE_PRESENT | PAGE_WRITABLE
    mov edi, PD_ADDR
    mov al, PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE
    mov cl, IDENTITY_MAP_ENTRIES
    cdq
.map_identity_window:
    stosd
    xchg eax, edx
    stosd
    xchg eax, edx
    add eax, LARGE_PAGE_BYTES
    loop .map_identity_window

    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    mov ecx, EFER_MSR
    rdmsr
    or eax, 0x00000100
    wrmsr

    mov eax, PML4_ADDR
    mov cr3, eax

    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax

    jmp CODE64_SEG:long_mode_entry

[bits 64]
long_mode_entry:
    mov ax, DATA64_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, STACK_TOP64

    mov rax, cr0
    and rax, ~CR0_EM
    or rax, CR0_MP
    mov cr0, rax

    mov rax, cr4
    or rax, CR4_OSFXSR | CR4_OSXMMEXCPT
    mov cr4, rax
    fninit

    mov ecx, BOOT_INFO_ADDR
    mov rax, KERNEL_HIGH_HALF_ADDR
    jmp rax

[bits 16]
boot_drive db 0
kernel_sector_marker db "KS64"
kernel_sector_count dw 1

align 4
disk_address_packet:
    db 0x10
    db 0x00
dap_sector_count:
    dw 1
dap_buffer_offset:
    dw KERNEL_LOAD_OFFSET
dap_buffer_segment:
    dw KERNEL_LOAD_SEG
dap_lba_low:
    dd 0x00000001
dap_lba_high:
    dd 0x00000000
remaining_sectors:
    dw 0

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
