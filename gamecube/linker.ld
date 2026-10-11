OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)
ENTRY(usm_gc_arena_lo_wrapper)

MEMORY
{
    payload (rwx) : ORIGIN = 0x80636040, LENGTH = 0x011C9FC0
}

PHDRS
{
    payload PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = ORIGIN(payload);
    __payload_start = .;

    .text : ALIGN(4)
    {
        /* These callers run while ArenaLo is being established. Keep the
           wrapper at the first payload address and prevent --gc-sections
           from discarding externally patched entry points. */
        KEEP(*(.text.usm_gc_arena_lo_wrapper))
        KEEP(*(.text.arena_wrapper))
        KEEP(*(.arena_wrapper))
        KEEP(*(.text.usm_debug_menu_hook))
        KEEP(*(.text.usm_debug_menu_update_hook))
        KEEP(*(.text.usm_debug_menu_pad_read_hook))
        KEEP(*(.text.payload_hook))
        KEEP(*(.text.usm_gc_*))

        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    } > payload :payload

    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
        *(.sdata2)
        *(.sdata2.*)
        *(.gnu.linkonce.r.*)
    } > payload :payload

    .data : ALIGN(4)
    {
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        *(.got)
        *(.got2)
        *(.gnu.linkonce.d.*)
    } > payload :payload

    .init_array : ALIGN(4)
    {
        KEEP(*(.preinit_array))
        KEEP(*(.preinit_array.*))
        KEEP(*(.init_array))
        KEEP(*(.init_array.*))
        KEEP(*(.ctors))
        KEEP(*(.ctors.*))
        KEEP(*(.fini_array))
        KEEP(*(.fini_array.*))
        KEEP(*(.dtors))
        KEEP(*(.dtors.*))
    } > payload :payload

    .bss (NOLOAD) : ALIGN(32)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        . = ALIGN(32);
        __bss_end = .;
        __payload_end = .;
    } > payload :payload

    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.gcc_except_table)
        *(.interp)
        *(.dynamic)
        *(.dynsym)
        *(.dynstr)
        *(.hash)
        *(.gnu.hash)
    }

    ASSERT(DEFINED(usm_gc_arena_lo_wrapper),
           "missing required symbol usm_gc_arena_lo_wrapper")
    ASSERT(DEFINED(usm_debug_menu_hook),
           "missing required symbol usm_debug_menu_hook")
    ASSERT(usm_gc_arena_lo_wrapper == ORIGIN(payload),
           "usm_gc_arena_lo_wrapper must be the first payload function")
    ASSERT(SIZEOF(.init_array) == 0,
           "static constructors/destructors are unsupported by this payload")
    ASSERT(__payload_end <= ORIGIN(payload) + LENGTH(payload),
           "GameCube payload exceeds the reserved MEM1 range")
}
