    .section .text.arena_wrapper,"ax",@progbits
    .align 2
    .global usm_gc_arena_lo_wrapper
    .type usm_gc_arena_lo_wrapper,@function
usm_gc_arena_lo_wrapper:
    lis     4,__payload_end@ha
    addi    4,4,__payload_end@l
    cmplw   3,4
    bge     1f
    mr      3,4
1:
    lis     12,0x802f
    ori     12,12,0x4c58
    mtctr   12
    bctr
    .size usm_gc_arena_lo_wrapper,.-usm_gc_arena_lo_wrapper
