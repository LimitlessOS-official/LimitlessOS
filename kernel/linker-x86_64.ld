ENTRY(_start)

SECTIONS
{
    . = 0x10000;
    __kernel_start = .;
    ___kernel_start = .;

    .text ALIGN(16) :
    {
        *(.text*)
    }

    .rodata ALIGN(16) :
    {
        *(.rdata*)
        *(.rodata*)
    }

    .data ALIGN(16) :
    {
        *(.data*)
    }

    . = ALIGN(16);
    . = 0x100000;

    __bss_start = .;
    ___bss_start = .;

    .bss ALIGN(16) :
    {
        *(COMMON)
        *(.bss*)
    }

    __bss_end = .;
    ___bss_end = .;

    /DISCARD/ :
    {
        *(.comment*)
        *(.drectve*)
        *(.eh_frame*)
        *(.pdata*)
        *(.xdata*)
    }

    __kernel_end = .;
    ___kernel_end = .;
}
