@ ow_loops.s - overworld copy loop, ARM in IWRAM (owner: gba).

    .section .iwram, "ax", %progbits
    .arm
    .align 2

@ void ow_out(uint16_t *dst, const uint8_t *src, int n)
@ Copy n pixels (n even, > 0) from src (any byte alignment) to VRAM at dst
@ (halfword aligned). Word loads + barrel-shifter realignment; reads up
@ to 4 bytes past the end of src.
    .global ow_out
    .type   ow_out, %function
ow_out:
    stmfd   sp!, {r4-r7}
    and     r3, r1, #3
    bic     r1, r1, #3
    movs    r3, r3, lsl #3              @ misalignment in bits
    ldr     r4, [r1], #4
    beq     .Lo_aligned
    rsb     r5, r3, #32
1:  subs    r2, r2, #4
    blt     2f
    ldr     r6, [r1], #4
    mov     r7, r4, lsr r3
    orr     r7, r7, r6, lsl r5
    strh    r7, [r0], #2
    mov     r7, r7, lsr #16
    strh    r7, [r0], #2
    mov     r4, r6
    b       1b
2:  adds    r2, r2, #4                  @ 0 or 2 pixels left
    beq     9f
    ldr     r6, [r1]
    mov     r7, r4, lsr r3
    orr     r7, r7, r6, lsl r5
    strh    r7, [r0], #2
    b       9f
.Lo_aligned:
3:  subs    r2, r2, #4
    blt     4f
    strh    r4, [r0], #2
    mov     r7, r4, lsr #16
    ldr     r4, [r1], #4
    strh    r7, [r0], #2
    b       3b
4:  adds    r2, r2, #4
    strneh  r4, [r0], #2
9:  ldmfd   sp!, {r4-r7}
    bx      lr
    .size   ow_out, . - ow_out

@ void ow_tile16(uint16_t *dst, const uint8_t *src, int rows)
@ Copy an unclipped 16-pixel-wide block: rows x 16 bytes from src (word
@ aligned, stride 16) to VRAM at dst (halfword aligned, stride 240 bytes).
    .global ow_tile16
    .type   ow_tile16, %function
ow_tile16:
    stmfd   sp!, {r4-r8}
    tst     r0, #2
    bne     2f
    @ dst word aligned: 4 words per row
1:  ldmia   r1!, {r4-r7}
    stmia   r0, {r4-r7}
    add     r0, r0, #240
    subs    r2, r2, #1
    bne     1b
    ldmfd   sp!, {r4-r8}
    bx      lr
    @ dst at +2: halfword, 3 realigned words, halfword
2:  ldmia   r1!, {r4-r7}
    strh    r4, [r0]
    mov     r4, r4, lsr #16
    orr     r4, r4, r5, lsl #16
    mov     r5, r5, lsr #16
    orr     r5, r5, r6, lsl #16
    mov     r6, r6, lsr #16
    orr     r6, r6, r7, lsl #16
    add     r8, r0, #2
    stmia   r8, {r4-r6}
    mov     r7, r7, lsr #16
    strh    r7, [r0, #14]
    add     r0, r0, #240
    subs    r2, r2, #1
    bne     2b
    ldmfd   sp!, {r4-r8}
    bx      lr
    .size   ow_tile16, . - ow_tile16
