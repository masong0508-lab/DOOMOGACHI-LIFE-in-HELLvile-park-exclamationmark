@ video_loops.s - the 2D layer's span fill, ARM in IWRAM (owner: gba).

    .section .iwram, "ax", %progbits
    .arm
    .align 2

@ void plat_rect(int x, int y, int w, int h, uint8_t color)   (platform.h)
@ Clips against the screen and tail-calls the fill: vid_rect_full (DMA)
@ for full-width rects, else vid_rect. Nothing is drawn on a skipped
@ catch-up tick (gba_nodraw); colour index 0 means PAL_BLACK. The colour
@ argument's stack slot is handed on (rewritten) to the fill routine.
    .global plat_rect
    .type   plat_rect, %function
plat_rect:
    ldr     r12, =gba_nodraw
    ldr     r12, [r12]
    cmp     r12, #0
    bxne    lr
    add     r2, r0, r2                  @ x1
    add     r3, r1, r3                  @ y1
    bic     r0, r0, r0, asr #31         @ x0 = max(x, 0)
    bic     r1, r1, r1, asr #31         @ y0 = max(y, 0)
    cmp     r2, #240
    movgt   r2, #240
    cmp     r3, #160
    movgt   r3, #160
    cmp     r2, r0
    cmpgt   r3, r1
    bxle    lr                          @ nothing on screen
    ldrb    r12, [sp]                   @ colour
    cmp     r12, #0
    moveq   r12, #1                     @ PAL_BLACK
    str     r12, [sp]
    cmp     r0, #0
    cmpeq   r2, #240
    beq     vid_rect_full
    b       vid_rect
    .size   plat_rect, . - plat_rect
    .ltorg

@ void vid_rect(int x0, int y0, int x1, int y1, uint32_t color)
@ Fill [x0, x1) x [y0, y1) of the back page (already clipped, non-empty).
@ Mode 4 VRAM takes halfword / word writes only: an odd first or last
@ pixel is a halfword read-modify-write; the middle is halfwords to word
@ alignment, then 16-byte stm bursts and words. The core's UI draws
@ thousands of small spans a frame, so the per-row overhead is kept low.
    .global vid_rect
    .type   vid_rect, %function
vid_rect:
    sub     r12, r2, r0                 @ narrow spans (< 32 px): word masks
    cmp     r12, #32
    blt     vid_rect_narrow
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

@ void vid_shade_words(uint32_t *p, int n, const uint8_t *lut)
@ Remap every byte of n >= 1 aligned VRAM words through lut (the
@ colormap row, ideally a copy in IWRAM): the middle of plat_shade_rect.
@ Two words per iteration through ldm/stm; byte lookups use the barrel
@ shifter, so each pixel is one and + one ldrb (about 6 cycles a pixel).
    .global vid_shade_words
    .type   vid_shade_words, %function
vid_shade_words:
    stmfd   sp!, {r4-r8, lr}
    mov     r12, #0xFF
    subs    r1, r1, #2
    blt     2f
1:  ldmia   r0, {r3, r4}
    and     r5, r12, r3
    ldrb    r5, [r2, r5]
    and     r6, r12, r3, lsr #8
    ldrb    r6, [r2, r6]
    and     r7, r12, r3, lsr #16
    ldrb    r7, [r2, r7]
    ldrb    r8, [r2, r3, lsr #24]
    orr     r5, r5, r6, lsl #8
    orr     r5, r5, r7, lsl #16
    orr     r3, r5, r8, lsl #24
    and     r5, r12, r4
    ldrb    r5, [r2, r5]
    and     r6, r12, r4, lsr #8
    ldrb    r6, [r2, r6]
    and     r7, r12, r4, lsr #16
    ldrb    r7, [r2, r7]
    ldrb    r8, [r2, r4, lsr #24]
    orr     r5, r5, r6, lsl #8
    orr     r5, r5, r7, lsl #16
    orr     r4, r5, r8, lsl #24
    stmia   r0!, {r3, r4}
    subs    r1, r1, #2
    bge     1b
2:  adds    r1, r1, #2                  @ 0 or 1 word left
    beq     3f
    ldr     r3, [r0]
    and     r5, r12, r3
    ldrb    r5, [r2, r5]
    and     r6, r12, r3, lsr #8
    ldrb    r6, [r2, r6]
    and     r7, r12, r3, lsr #16
    ldrb    r7, [r2, r7]
    ldrb    r8, [r2, r3, lsr #24]
    orr     r5, r5, r6, lsl #8
    orr     r5, r5, r7, lsl #16
    orr     r3, r5, r8, lsl #24
    str     r3, [r0]
3:  ldmfd   sp!, {r4-r8, lr}
    bx      lr
    .size   vid_shade_words, . - vid_shade_words

@ vid_rect for spans narrower than 32 pixels (the UI's typical rect: an
@ outline column, a corner band, a dot). Mode 4 VRAM takes word writes, so
@ each row is: the first word through a byte mask (read-modify-write),
@ whole words, the last word through a byte mask. No halfword alignment
@ steps: a span inside one word costs one masked read-modify-write per row.
    .type   vid_rect_narrow, %function
vid_rect_narrow:
    stmfd   sp!, {r4-r11, lr}
    ldr     r12, [sp, #36]              @ color
    and     r12, r12, #0xFF
    orr     r12, r12, r12, lsl #8
    orr     r12, r12, r12, lsl #16      @ colour in all four bytes
    sub     r3, r3, r1                  @ rows
    ldr     r4, =gba_back
    ldr     r4, [r4]
    rsb     r5, r1, r1, lsl #4          @ y0 * 15
    add     r4, r4, r5, lsl #4          @ + y0 * 240
    mov     r5, r0, lsr #2              @ first word
    sub     r6, r2, #1
    mov     r6, r6, lsr #2              @ last word
    add     r4, r4, r5, lsl #2          @ address of the first word, row y0
    and     r7, r0, #3
    mov     r7, r7, lsl #3
    mvn     r8, #0
    mov     r8, r8, lsl r7              @ head mask: bytes x0 & 3 .. 3
    sub     r7, r2, #1
    and     r7, r7, #3
    rsb     r7, r7, #3
    mov     r7, r7, lsl #3
    mvn     r9, #0
    mov     r9, r9, lsr r7              @ tail mask: bytes 0 .. (x1 - 1) & 3
    subs    r10, r6, r5                 @ words after the first
    bne     .Ln_multi
    and     r8, r8, r9                  @ one word: both masks
    and     r9, r12, r8
.Ln_single:
    ldr     r7, [r4]
    bic     r7, r7, r8
    orr     r7, r7, r9
    str     r7, [r4], #240
    subs    r3, r3, #1
    bne     .Ln_single
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
.Ln_multi:
    and     r11, r12, r8                @ head colour bits
    and     lr, r12, r9                 @ tail colour bits
    sub     r10, r10, #1                @ whole words in between
.Ln_row:
    mov     r5, r4
    ldr     r7, [r5]
    bic     r7, r7, r8
    orr     r7, r7, r11
    str     r7, [r5], #4
    movs    r6, r10
    beq     2f
1:  str     r12, [r5], #4
    subs    r6, r6, #1
    bne     1b
2:  ldr     r7, [r5]
    bic     r7, r7, r9
    orr     r7, r7, lr
    str     r7, [r5]
    add     r4, r4, #240
    subs    r3, r3, #1
    bne     .Ln_row
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
    .size   vid_rect_narrow, . - vid_rect_narrow
    .ltorg
