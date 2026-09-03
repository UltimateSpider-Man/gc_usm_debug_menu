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
        . = ALIGN(4);
        __ds4_mapping_start = .;
        KEEP(*(.ds4_mapping))
        __ds4_mapping_end = .;
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
    ASSERT(ADDR(.payload) + SIZEOF(.payload) <= 0x81792000,
           "payload overlaps the first fixed auxiliary cache")
    ASSERT(__ds4_mapping_end - __ds4_mapping_start == 69,
           "DS4 mapping must contain exactly one 23-entry descriptor")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.gnu.attributes)
    }
}
