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

    .bss ALIGN(16) :
    {
        *(COMMON)
        *(.bss*)
    }

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
