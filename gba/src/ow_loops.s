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

@ (G2) void ow_spr_rows(OwSprJob *j)
@ A sprite's rows (ow.iwram.c ow_sprite): j->rows rows of j->n pixels
@ (even, > 0) to VRAM from j->dst (halfword aligned, + 240 bytes per row),
@ texels from j->src (any alignment, + j->stride per row). A row's texels
@ are read four per word (aligned loads, shifted together) and ANDed with
@ j->tmask[y & 1] (the translucency checker); the row's first texel is
@ dropped when j->head is 0xFF and its last quad (or pair) is ANDed with
@ j->tail (the clipped odd edge columns). Pixel k of row y goes through
@ table j->q[y & 3][k & 3] (its 4x4 class, ow_int.h); texel 0 is
@ transparent. A quad of transparent texels costs one word read, an
@ opaque pair is one halfword store, a half-opaque one a read-modify-
@ write. j->dst, j->src, j->rows and j->phase (y & 3) are consumed. Reads
@ up to 4 bytes before and 8 bytes past a row.
    .equ    OSJ_DST, 0
    .equ    OSJ_SRC, 4
    .equ    OSJ_N, 8
    .equ    OSJ_ROWS, 12
    .equ    OSJ_STRIDE, 16
    .equ    OSJ_PHASE, 20
    .equ    OSJ_TMASK, 24
    .equ    OSJ_Q, 32
    .equ    OSJ_HEAD, 96
    .equ    OSJ_TAIL, 100

    @ one pixel pair from the low 16 bits of r11 (a = bits 0-7, b = 8-15)
    @ to [r0, #off]; the half-opaque cases are out of line (OW_PAIR_COLD)
    .macro OW_PAIR qa, qb, off, lbl
    ands    r12, r11, #0xFF
    beq     \lbl\()_a0
    tst     r11, #0xFF00
    ldrb    r12, [\qa, r12]
    beq     \lbl\()_b0
    and     lr, r11, #0xFF00
    ldrb    lr, [\qb, lr, lsr #8]
    orr     r12, r12, lr, lsl #8
    strh    r12, [r0, #\off]
\lbl\()_done:
    .endm
    .macro OW_PAIR_COLD qb, off, lbl
\lbl\()_b0:                             @ a only (r12 = qa[a])
    ldrh    lr, [r0, #\off]
    and     lr, lr, #0xFF00
    orr     r12, r12, lr
    strh    r12, [r0, #\off]
    b       \lbl\()_done
\lbl\()_a0:                             @ b only, or neither
    ands    lr, r11, #0xFF00
    beq     \lbl\()_done
    ldrb    lr, [\qb, lr, lsr #8]
    ldrh    r12, [r0, #\off]
    and     r12, r12, #0xFF
    orr     r12, r12, lr, lsl #8
    strh    r12, [r0, #\off]
    b       \lbl\()_done
    .endm

    .global ow_spr_rows
    .type   ow_spr_rows, %function
ow_spr_rows:
    stmfd   sp!, {r0, r4-r11, lr}       @ [sp] = j
.Lsr_row:
    ldr     lr, [sp]
    ldr     r12, [lr, #OSJ_PHASE]
    add     r6, lr, #OSJ_Q
    add     r6, r6, r12, lsl #4
    ldmia   r6, {r6-r9}                 @ this row's four tables
    and     r5, r12, #1
    add     r5, lr, r5, lsl #2
    ldr     r5, [r5, #OSJ_TMASK]
    ldr     r0, [lr, #OSJ_DST]
    ldr     r1, [lr, #OSJ_SRC]
    ldr     r2, [lr, #OSJ_N]
    ldr     r11, [lr, #OSJ_HEAD]
    and     r3, r1, #3
    mov     r3, r3, lsl #3              @ the first texel's bit offset
    bic     r1, r1, #3
    rsb     r10, r3, #32                @ (offset 0: lsl #32 gives 0)
    ldr     r4, [r1], #4                @ the word holding texel 0
    bic     r4, r4, r11, lsl r3         @ (head: texel 0 dropped)
1:  ldr     r12, [r1], #4
    mov     r11, r4, lsr r3
    orr     r11, r11, r12, lsl r10      @ texels k..k+3
    mov     r4, r12
    and     r11, r11, r5
    cmp     r2, #4
    ble     .Lsr_tail                   @ the last quad or pair
.Lsr_body:
    cmp     r11, #0
    beq     8f
    OW_PAIR r6, r7, 0, .Lsp0
    movs    r11, r11, lsr #16
    beq     8f
    OW_PAIR r8, r9, 2, .Lsp1
8:  add     r0, r0, #4
    subs    r2, r2, #4
    bgt     1b
    @ next row
    ldr     lr, [sp]
    ldmia   lr, {r0-r5}                 @ dst, src, n, rows, stride, phase
    add     r0, r0, #240
    add     r1, r1, r4
    add     r5, r5, #1
    and     r5, r5, #3
    subs    r3, r3, #1
    stmia   lr, {r0-r5}
    bgt     .Lsr_row
    ldmfd   sp!, {r0, r4-r11, lr}
    bx      lr
.Lsr_tail:
    ldr     lr, [sp]
    ldr     lr, [lr, #OSJ_TAIL]
    and     r11, r11, lr
    b       .Lsr_body
    OW_PAIR_COLD r7, 0, .Lsp0
    OW_PAIR_COLD r9, 2, .Lsp1
    .size   ow_spr_rows, . - ow_spr_rows
