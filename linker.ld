OUTPUT_ARCH(powerpc)
ENTRY(debug_menu_hook)

SECTIONS
{
    . = 0x81780000;
    .payload : ALIGN(32)
    {
        KEEP(*(.hook))
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.sdata .sdata.*)
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        /* Force the zero-initialized state into the raw payload image. */
        BYTE(0)
        . = ALIGN(32);
    }

    ASSERT(ADDR(.payload) == 0x81780000,
           "payload moved away from its reserved MEM1 base")
    ASSERT(debug_menu_hook == 0x81780000,
           "debug_menu_hook must remain the first payload instruction")
    ASSERT(SIZEOF(.payload) <= 0x80000,
           "payload exceeds the reserved 512 KiB MEM1 region")
    ASSERT(ADDR(.payload) + SIZEOF(.payload) <= 0x81800000,
           "payload crosses the physical end of MEM1")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.gnu.attributes)
    }
}
