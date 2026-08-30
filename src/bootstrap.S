/*
 * DOL-loadable bootstrap for the GUTE52 debug-menu payload.
 *
 * Text3 is loaded in the otherwise unused range immediately after retail
 * text2.  The injector forces OSInit's existing ArenaHi fallback to
 * 0x81780000, so this stub can copy the final-address-linked payload to that
 * exclusive arena boundary before normal startup.  Cache synchronization is
 * required because the destination is executed later by the UI render hook.
 */

    .section .bootstrap,"ax",@progbits
    .align 5
    .global bootstrap_entry
    .type bootstrap_entry,@function

bootstrap_entry:
    lis     3,payload_source@h
    ori     3,3,payload_source@l
    lis     4,payload_destination@h
    ori     4,4,payload_destination@l
    lis     5,payload_copy_size@h
    ori     5,5,payload_copy_size@l
    srwi    5,5,2
    mtctr   5

.Lcopy_payload:
    lwz     6,0(3)
    stw     6,0(4)
    addi    3,3,4
    addi    4,4,4
    bdnz    .Lcopy_payload

    lis     3,payload_destination@h
    ori     3,3,payload_destination@l
    lis     4,payload_copy_end@h
    ori     4,4,payload_copy_end@l

.Lflush_data:
    dcbf    0,3
    addi    3,3,32
    cmplw   0,3,4
    blt     .Lflush_data
    sync

    lis     3,payload_destination@h
    ori     3,3,payload_destination@l

.Linvalidate_instructions:
    icbi    0,3
    addi    3,3,32
    cmplw   0,3,4
    blt     .Linvalidate_instructions
    sync
    isync

    lis     12,original_entry@h
    ori     12,12,original_entry@l
    mtctr   12
    bctr

    .size bootstrap_entry,.-bootstrap_entry
