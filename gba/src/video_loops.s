@ video_loops.s - the 2D layer's span fill, ARM in IWRAM (owner: gba).

    .section .iwram, "ax", %progbits
    .arm
    .align 2

@ void vid_rect(int x0, int y0, int x1, int y1, uint32_t color)
@ Fill [x0, x1) x [y0, y1) of the back page (already clipped, non-empty).
@ Mode 4 VRAM takes halfword / word writes only: an odd first or last
@ pixel is a halfword read-modify-write; the middle is halfwords to word
@ alignment, then 16-byte stm bursts and words. The core's UI draws
@ thousands of small spans a frame, so the per-row overhead is kept low.
    .global vid_rect
    .type   vid_rect, %function
vid_rect:
    stmfd   sp!, {r4-r9, lr}
    ldr     r12, [sp, #28]              @ color
    and     r12, r12, #0xFF
    sub     r3, r3, r1                  @ rows
    sub     r2, r2, r0                  @ width
    ldr     r4, =gba_back
    ldr     r4, [r4]
    rsb     r5, r1, r1, lsl #4          @ y0 * 15
    add     r4, r4, r5, lsl #4          @ + y0 * 240
    add     r4, r4, r0                  @ byte address of (x0, y0)
    mov     r0, r12, lsl #8             @ c << 8  (odd first pixel)
    mov     r1, r12                     @ c       (odd last pixel)
    orr     r12, r12, r12, lsl #8
    orr     r12, r12, r12, lsl #16      @ c in all four bytes
    mov     r8, r12
    mov     r9, r12
    mov     lr, r12
.Lr_row:
    mov     r6, r2                      @ n
    tst     r4, #1
    moveq   r5, r4
    beq     1f
    sub     r5, r4, #1                  @ odd x: high byte of the halfword
    ldrh    r7, [r5]
    and     r7, r7, #0xFF
    orr     r7, r7, r0
    strh    r7, [r5], #2
    subs    r6, r6, #1
    beq     .Lr_next
1:  cmp     r6, #2
    blt     .Lr_tail
    tst     r5, #2                      @ halfword to word alignment
    strneh  r12, [r5], #2
    subne   r6, r6, #2
    subs    r7, r6, #16
    blt     3f
2:  stmia   r5!, {r8, r9, r12, lr}
    subs    r7, r7, #16
    bge     2b
3:  add     r6, r7, #16                 @ 0..15 left
    movs    r7, r6, lsr #2
    beq     .Lr_half
4:  str     r12, [r5], #4
    subs    r7, r7, #1
    bne     4b
.Lr_half:
    tst     r6, #2
    strneh  r12, [r5], #2
.Lr_tail:
    tst     r6, #1                      @ odd last pixel: low byte
    beq     .Lr_next
    ldrh    r7, [r5]
    and     r7, r7, #0xFF00
    orr     r7, r7, r1
    strh    r7, [r5]
.Lr_next:
    add     r4, r4, #240
    subs    r3, r3, #1
    bgt     .Lr_row
    ldmfd   sp!, {r4-r9, lr}
    bx      lr
    .ltorg
