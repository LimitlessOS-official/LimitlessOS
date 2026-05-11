[bits 16]
[org 0x7C00]

KERNEL_LOAD_SEG equ 0x1000
KERNEL_LOAD_OFFSET equ 0x0000
KERNEL_LOAD_ADDR equ 0x10000
STACK_TOP equ 0x90000
BOOT_INFO_ADDR equ 0x9000

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

CODE_SEG equ 0x08
DATA_SEG equ 0x10

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    call load_kernel
    call prepare_boot_info

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp CODE_SEG:protected_mode_entry

check_extensions:
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc boot_error
    cmp bx, 0xAA55
    jne boot_error
    test cx, 0x1
    jz boot_error
    ret

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

    mov dword [BOOT_INFO_ADDR + BOOT_INFO_ARCH_BITS], 32
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_BOOT_FLAGS], 0x00000001
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_PAGE_TABLE_ROOT], 0
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_IDENTITY_MAP_BYTES], 0
    ret

load_kernel:
    mov word [load_segment], KERNEL_LOAD_SEG
    mov ax, [kernel_sector_count]
    mov [load_remaining], ax
    mov dword [load_lba], 1
    cmp byte [boot_drive], 0x80
    jb .load_chs
    call check_extensions
.load_edd:
.load_loop:
    mov dx, [load_remaining]
    test dx, dx
    jz .done

    mov ax, dx
    cmp ax, 127
    jbe .chunk_ready
    mov ax, 127

.chunk_ready:
    mov [load_chunk], ax
    mov [dap_sector_count], ax
    mov word [dap_buffer_offset], KERNEL_LOAD_OFFSET
    mov ax, [load_segment]
    mov [dap_buffer_segment], ax
    mov eax, [load_lba]
    mov [dap_lba_low], eax
    mov dword [dap_lba_high], 0

    mov si, disk_address_packet
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc boot_error

    mov ax, [load_chunk]
    sub [load_remaining], ax
    movzx eax, ax
    add dword [load_lba], eax
    mov ax, [load_chunk]
    shl ax, 5
    add [load_segment], ax

    jmp .load_loop
.load_chs:
    mov dx, [load_remaining]
    test dx, dx
    jz .done

    mov ax, [load_segment]
    mov es, ax
    xor bx, bx
    mov ax, [load_lba]
    call lba_to_chs
    mov dl, [boot_drive]
    mov ah, 0x02
    mov al, 0x01
    int 0x13
    jc boot_error

    dec word [load_remaining]
    inc word [load_lba]
    add word [load_segment], 0x20
    jmp .load_chs
.done:
    ret

lba_to_chs:
    push bx
    xor dx, dx
    mov bx, 18
    div bx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, 2
    div bx
    mov dh, dl
    mov ch, al
    shl ah, 6
    or cl, ah
    pop bx
    ret

boot_error:
    mov si, boot_error_message
    call print_string
.hang:
    cli
    hlt
    jmp .hang

print_string:
    lodsb
    test al, al
    jz .done
    mov dx, 0x00E9
    out dx, al
    jmp print_string
.done:
    ret

[bits 32]
protected_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, STACK_TOP

    mov eax, BOOT_INFO_ADDR
    mov ebx, KERNEL_LOAD_ADDR
    jmp ebx

[bits 16]
boot_drive db 0
boot_error_message db "E", 0
kernel_sector_marker db "KSCT"
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
load_lba:
    dd 0x00000001
load_remaining:
    dw 0
load_chunk:
    dw 0
load_segment:
    dw KERNEL_LOAD_SEG

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
