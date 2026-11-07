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
    bne     plat_rect_nz                @ (not taken on the Mode 4 path)
    .global plat_rect_m4
plat_rect_m4:                           @ the Mode 4 path (plat_rect_slow)
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
    ldreq   r12, =vid_rect_full         @ ROM (THUMB): interworking jump
    bxeq    r12
    b       vid_rect
@ gba_nodraw != 0: a skipped catch-up tick draws nothing; G5: a tile frame
@ (GBA_NODRAW_CANVAS) fills into the canvas (ARM, the IWRAM 2D group, loaded
@ then); the estimate and a frame's first draw take the slow path (ROM,
@ THUMB: canvas.c), which calls plat_rect_m4 back
plat_rect_nz:
    cmp     r12, #1
    bxeq    lr
    cmp     r12, #2
    ldreq   r12, =cv_rect_fast
    bxeq    r12
    @ G7: panels up (GBA_NODRAW_PANEL, panel.c): a rect that cannot touch
    @ them (pn_box, their bounding box; the whole screen while one is drawn)
    @ takes the Mode 4 path as if nothing were up
    cmp     r12, #5
    bne     2f
    push    {r4-r7}
    ldr     r12, =pn_box
    ldmia   r12, {r4-r7}                @ x0, y0, x1, y1
    add     r12, r0, r2                 @ x + w
    cmp     r12, r4
    cmpgt   r6, r0                      @ x + w > x0 && x1 > x
    addgt   r12, r1, r3                 @ y + h
    cmpgt   r12, r5
    cmpgt   r7, r1                      @ && y + h > y0 && y1 > y
    pop     {r4-r7}
    ble     plat_rect_m4
2:  ldr     r12, =plat_rect_slow
    bx      r12
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
    sub     r12, r2, r0                 @ width
    cmp     r12, #1
    bne     1f
    sub     r12, r3, r1
    cmp     r12, #1
    bne     1f
    @ one pixel (dots, sparkles, stars, checker fall-backs: the UI's most
    @ frequent rect): a halfword read-modify-write, no stack frame
    ldr     r2, =gba_back
    ldr     r2, [r2]
    rsb     r3, r1, r1, lsl #4          @ y0 * 15
    add     r2, r2, r3, lsl #4          @ + y0 * 240
    add     r2, r2, r0                  @ byte address of the pixel
    ldr     r3, [sp]                    @ colour
    and     r3, r3, #0xFF
    tst     r0, #1
    bic     r2, r2, #1                  @ its halfword
    ldrh    r12, [r2]
    andeq   r12, r12, #0xFF00           @ even x: low byte
    orreq   r12, r12, r3
    andne   r12, r12, #0xFF             @ odd x: high byte
    orrne   r12, r12, r3, lsl #8
    strh    r12, [r2]
    bx      lr
1:  sub     r12, r2, r0                 @ narrow spans (< 32 px): word masks
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

@ void pn_span(volatile uint16_t *d, const volatile uint16_t *s, int x0, int x1)
@ G7 (panel.c): pixels [x0, x1) of one row, from the front page's row s
@ to the back page's row d. VRAM ignores byte stores: an odd first / last
@ pixel is a halfword read-modify-write, the halfwords between are one
@ 16-bit DMA (the CPU waits for it).
    .section .iwram, "ax", %progbits
    .arm
    .align 2
    .global pn_span
    .type   pn_span, %function
pn_span:
    push    {r4-r6}
    mov     r4, r2, asr #1              @ a = x0 / 2
    mov     r5, r3, asr #1              @ b = x1 / 2
    tst     r2, #1
    beq     1f
    mov     r6, r4, lsl #1              @ odd x0: the high byte of d[a]
    ldrh    r12, [r1, r6]
    ldrh    r2, [r0, r6]
    and     r12, r12, #0xFF00
    and     r2, r2, #0xFF
    orr     r2, r2, r12
    strh    r2, [r0, r6]
    add     r4, r4, #1
1:  subs    r6, r5, r4                  @ halfwords [a, b)
    ble     2f
    ldr     r12, =0x040000D4            @ DMA3SAD, DAD, CNT
    add     r2, r1, r4, lsl #1
    str     r2, [r12]
    add     r2, r0, r4, lsl #1
    str     r2, [r12, #4]
    orr     r6, r6, #0x80000000         @ enable, 16-bit, immediate
    str     r6, [r12, #8]
2:  tst     r3, #1
    beq     3f
    mov     r6, r5, lsl #1              @ odd x1: the low byte of d[b]
    ldrh    r12, [r1, r6]
    ldrh    r2, [r0, r6]
    and     r12, r12, #0xFF
    and     r2, r2, #0xFF00
    orr     r2, r2, r12
    strh    r2, [r0, r6]
3:  pop     {r4-r6}
    bx      lr
    .size   pn_span, . - pn_span
    .ltorg

@ void r3d_up2(uint16_t *base, int hw, int vh2)
@ G7 (r3d.c, Options "3D: Faster"): the 3D view drawn at half resolution
@ into the top left of its rect (vh2 rows of hw halfwords, row stride 240
@ bytes, base word aligned) doubled in place: source row r goes to rows 2r
@ and 2r + 1, each pixel twice. Rows from the bottom up and pixels from the
@ right, so no source pixel is overwritten before it is read.
    .section .iwram, "ax", %progbits
    .arm
    .align 2
    .global r3d_up2
    .type   r3d_up2, %function
r3d_up2:
    push    {r4-r8, lr}
    subs    r3, r2, #1                  @ r = vh2 - 1
    blt     9f
    mov     lr, #240
1:  mla     r5, r3, lr, r0              @ source row r
    add     r6, r3, r3
    mla     r6, lr, r6, r0              @ row 2r (Rd != Rm: ARMv4 MLA rule)
    add     r7, r6, #240                @ row 2r + 1
    add     r5, r5, r1, lsl #1          @ their ends
    add     r6, r6, r1, lsl #2
    add     r7, r7, r1, lsl #2
    mov     r8, r1
2:  ldrh    r12, [r5, #-2]!             @ pixels a (low), b (high)
    and     r4, r12, #0xFF
    orr     r4, r4, r4, lsl #8          @ a a
    mov     r12, r12, lsr #8
    orr     r12, r12, r12, lsl #8       @ b b
    orr     r4, r4, r12, lsl #16        @ a a b b
    str     r4, [r6, #-4]!
    str     r4, [r7, #-4]!
    subs    r8, r8, #1
    bne     2b
    subs    r3, r3, #1
    bge     1b
9:  pop     {r4-r8, lr}
    bx      lr
    .size   r3d_up2, . - r3d_up2

@ BLIT_TAIL loop: the pair v (r3) is in hand, p r1, end r2. Zero: skip. Both
@ pixels set: one halfword store (post-incremented). One pixel: the only read
@ -modify-write. Falls to .Lbl_mdone at the end of the row.
    .macro BLIT_TAIL L
    cmp     r3, #0
    beq     8f
    tst     r3, #0xFF00
    tstne   r3, #0xFF
    strneh  r3, [r1], #2
    cmpne   r1, r2
    bne     \L
    cmp     r1, r2
    beq     .Lbl_mdone
    tst     r3, #0xFF
    ldrh    r4, [r1]
    andne   r4, r4, #0xFF00
    andeq   r4, r4, #0xFF
    orr     r4, r4, r3
    strh    r4, [r1]
8:  add     r1, r1, #2
    cmp     r1, r2
    bne     \L
    b       .Lbl_mdone
    .endm

@ ---------------------------------------------------------------------------
@ void vid_blit(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color)
@ The common case (scale 1, no flip: cached chibis, building sprites, shape
@ images) in ARM; anything scaled or flipped goes on to vid_blit_c (video.iwram.c).
@ Exactly the pixels the C loop wrote (docs/STUDIO_30FPS_PLAN.md, option A):
@ pairs are walked in VRAM halfwords; an opaque pair is stored with no VRAM
@ read, a transparent pair is skipped, an edge pair is the only read-modify-
@ write. Silhouettes use the same tail. Register map:
@   r0 s (source)   r1 p (VRAM)   r2 end of the middle pairs   r3 pair v
@   r4 scratch   r5 prev pixel / scratch   r6 rows left   r7 image w
@   r8 source base (pix + (yy - y) * w - x)   r9 VRAM row base (+ yy * 240)
@   r10 colour   r11 x0   r12 x1 (clipped)   lr silhouette flag
@ A pixel at screen X is at r8 + X in the source and r9 + X in VRAM.
    .section .iwram, "ax", %progbits
    .arm
    .align 2
    .global vid_blit
    .type   vid_blit, %function
vid_blit:
    ldr     r12, [sp]                   @ flags
    cmp     r3, #1
    tsteq   r12, #1                     @ IMG_FLIPX
    bne     vid_blit_c                  @ scaled or flipped: the C path
    stmfd   sp!, {r4-r11, lr}
    ldrh    r7, [r2]                    @ w
    ldrh    r6, [r2, #2]                @ h
    ldr     r8, [r2, #4]                @ pix
    ldr     lr, [sp, #36]               @ flags
    ldr     r10, [sp, #40]              @ colour
    and     lr, lr, #4                  @ IMG_SILHOUETTE
    add     r4, r0, r7
    cmp     r4, #240
    movgt   r4, #240                    @ x1
    bic     r5, r0, r0, asr #31         @ x0
    cmp     r5, r4
    bge     .Lbl_done
    add     r9, r1, r6
    cmp     r9, #160
    movgt   r9, #160                    @ y1
    bic     r3, r1, r1, asr #31         @ y0
    cmp     r3, r9
    bge     .Lbl_done
    sub     r6, r9, r3                  @ rows
    rsb     r9, r3, r3, lsl #4          @ 15 * y0
    sub     r3, r3, r1                  @ y0 - y
    mul     r12, r3, r7
    add     r8, r8, r12
    sub     r8, r8, r0                  @ source base
    ldr     r12, =gba_back
    ldr     r12, [r12]
    add     r9, r12, r9, lsl #4         @ VRAM row base (240 * y0)
    mov     r11, r5                     @ x0
    mov     r12, r4                     @ x1
.Lbl_row:
    tst     r11, #1                     @ odd x0: only the right pixel of its pair
    beq     .Lbl_nolead
    ldrb    r5, [r8, r11]
    cmp     r5, #0
    beq     .Lbl_nolead
    cmp     lr, #0
    movne   r5, r10
    add     r4, r9, r11
    sub     r4, r4, #1
    ldrh    r3, [r4]
    and     r3, r3, #0xFF
    orr     r3, r3, r5, lsl #8
    strh    r3, [r4]
.Lbl_nolead:
    add     r0, r11, #1
    bic     r0, r0, #1                  @ first whole pair
    bic     r2, r12, #1                 @ end of the whole pairs
    add     r1, r9, r0
    add     r2, r9, r2
    add     r0, r8, r0
    cmp     r1, r2
    bhs     .Lbl_mdone
    cmp     lr, #0
    bne     .Lbl_sil
    tst     r0, #1
    bne     .Lbl_odd
.Lbl_al:                                @ aligned source: a halfword a pair
    ldrh    r3, [r0], #2
    BLIT_TAIL .Lbl_al
.Lbl_odd:                               @ odd source: aligned halfwords, each pair is
    sub     r0, r0, #1                  @ the high byte of one and the low byte of the next
    ldrh    r5, [r0], #2
    mov     r5, r5, lsr #8
.Lbl_od:
    ldrh    r4, [r0], #2
    and     r3, r4, #0xFF
    orr     r3, r5, r3, lsl #8
    mov     r5, r4, lsr #8
    BLIT_TAIL .Lbl_od
.Lbl_sil:                               @ silhouette: opaque pixels become the colour
    ldrb    r3, [r0], #1
    ldrb    r4, [r0], #1
    cmp     r3, #0
    movne   r3, r10
    cmp     r4, #0
    movne   r4, r10, lsl #8
    orr     r3, r3, r4
    BLIT_TAIL .Lbl_sil
.Lbl_mdone:
    tst     r12, #1                     @ odd x1: only the left pixel of its pair
    beq     .Lbl_nodrow
    sub     r4, r12, #1
    ldrb    r5, [r8, r4]
    cmp     r5, #0
    beq     .Lbl_nodrow
    cmp     lr, #0
    movne   r5, r10
    add     r4, r9, r4
    ldrh    r3, [r4]
    and     r3, r3, #0xFF00
    orr     r3, r3, r5
    strh    r3, [r4]
.Lbl_nodrow:
    add     r8, r8, r7
    add     r9, r9, #240
    subs    r6, r6, #1
    bne     .Lbl_row
.Lbl_done:
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
    .ltorg
    .size   vid_blit, . - vid_blit
