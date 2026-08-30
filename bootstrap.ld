OUTPUT_ARCH(powerpc)
ENTRY(bootstrap_entry)

SECTIONS
{
    . = 0x80626000;
    .bootstrap : ALIGN(32)
    {
        KEEP(*(.bootstrap))
        . = ALIGN(32);
        payload_source = .;
    }

    payload_destination = 0x81780000;
    payload_copy_end = payload_destination + payload_copy_size;
    original_entry = 0x80003100;

    ASSERT(bootstrap_entry == 0x80626000,
           "bootstrap entry moved away from the staging base")
    ASSERT((payload_copy_size & 3) == 0,
           "runtime payload size must be word aligned")
    ASSERT(payload_copy_size > 0,
           "runtime payload must not be empty")
    ASSERT(payload_copy_size <= 0x80000,
           "runtime payload exceeds the reserved 512 KiB region")
    ASSERT(payload_copy_end <= 0x81800000,
           "runtime payload crosses the physical end of MEM1")
    ASSERT(payload_source + payload_copy_size <= 0x80636000,
           "staged image crosses the retail startup stack")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.gnu.attributes)
    }
}
