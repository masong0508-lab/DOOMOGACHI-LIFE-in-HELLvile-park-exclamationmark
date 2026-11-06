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

@ (V4b) void ow_flip_row(uint8_t *d, const uint8_t *s, int n)
@ d[k] = s[-k] for k < n (n even, > 0): one mirrored row for ow_spr_rows.
@ d is word aligned, s any alignment. The source is read as aligned words
@ walking down (a REV of each realigned word: four texels per read instead
@ of one byte read each, which in EWRAM / ROM is most of the cost); whole
@ words are written (n rounded up to 4) and the source is read up to 7
@ bytes past s and 3 before s - n + 1. The columns it computes beyond the
@ sprite's clipped edges are the ones ow_sprite masks off (head / tail).
    .global ow_flip_row
    .type   ow_flip_row, %function
ow_flip_row:
    stmfd   sp!, {r4-r6, lr}
    sub     r1, r1, #3                  @ lowest address of the first four texels
    and     r3, r1, #3
    bic     r1, r1, #3
    mov     r3, r3, lsl #3              @ misalignment in bits
    rsb     r12, r3, #32                @ (offset 0: lsl #32 gives 0)
    ldr     r4, [r1, #4]                @ the word above the current one
1:  ldr     r5, [r1], #-4
    mov     r6, r5, lsr r3
    orr     r6, r6, r4, lsl r12         @ four texels, ascending
    mov     r4, r5
    eor     r5, r6, r6, ror #16         @ r6 = REV(r6)
    bic     r5, r5, #0xFF0000
    mov     r6, r6, ror #8
    eor     r6, r6, r5, lsr #8
    str     r6, [r0], #4
    subs    r2, r2, #4
    bgt     1b
    ldmfd   sp!, {r4-r6, pc}            @ (ARM caller: ow_sprite)
    .size   ow_flip_row, . - ow_flip_row

@ (G2) void ow_spr_rows(OwSprJob *j)
@ A sprite's rows (ow.iwram.c ow_sprite): j->rows rows of j->n pixels
@ (even, > 0) to VRAM from j->dst (halfword aligned, + 240 bytes per row),
@ texels from j->src (any alignment, + j->stride per row). A row's texels
@ are read four per word (aligned loads, shifted together); the row's
@ first texel is dropped when j->head is 0xFF and its last quad (or pair)
@ is ANDed with j->tail (the clipped odd edge columns). Pixel k of row y
@ goes through table j->q[y & 3][k & 3] (its 4x4 class, ow_int.h); texel 0
@ is transparent. A quad of transparent texels costs one word read, an
@ opaque pair is one halfword store, a half-opaque one a read-modify-
@ write. The quads before the last go two per loop (the word registers
@ trade places, no moves). j->dst, j->src, j->rows and j->phase (y & 3)
@ are consumed. Reads up to 4 bytes before and 8 bytes past a row.
    .equ    OSJ_DST, 0
    .equ    OSJ_SRC, 4
    .equ    OSJ_N, 8
    .equ    OSJ_ROWS, 12
    .equ    OSJ_STRIDE, 16
    .equ    OSJ_PHASE, 20
    .equ    OSJ_Q, 24
    .equ    OSJ_HEAD, 88
    .equ    OSJ_TAIL, 92

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

    @ r0 dst, r1 the next source word, r2 quads before the last one, r3 the
    @ first texel's bit offset, r10 32 - r3 (offset 0: lsl #32 gives 0),
    @ r4 / r5 the source words, r6-r9 the row's tables, r11 four texels
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
    ldr     r0, [lr, #OSJ_DST]
    ldr     r1, [lr, #OSJ_SRC]
    ldr     r2, [lr, #OSJ_N]
    ldr     r11, [lr, #OSJ_HEAD]
    and     r3, r1, #3
    mov     r3, r3, lsl #3
    bic     r1, r1, #3
    rsb     r10, r3, #32
    add     r2, r2, #3
    mov     r2, r2, lsr #2
    subs    r2, r2, #1                  @ quads before the last
    ldr     r4, [r1], #4                @ the word holding texel 0
    bic     r4, r4, r11, lsl r3         @ (head: texel 0 dropped)
    beq     .Lsr_last
    @ quad A: texels from r4:r5
1:  ldr     r5, [r1], #4
    mov     r11, r4, lsr r3
    orrs    r11, r11, r5, lsl r10
.Lsr_a:
    beq     3f
    OW_PAIR r6, r7, 0, .Lsa0
    movs    r11, r11, lsr #16
    beq     3f
    OW_PAIR r8, r9, 2, .Lsa1
3:  subs    r2, r2, #1
    bmi     .Lsr_next                   @ that was the row's last quad
    beq     .Lsr_last_b
    @ quad B: texels from r5:r4
    ldr     r4, [r1], #4
    mov     r11, r5, lsr r3
    orrs    r11, r11, r4, lsl r10
    beq     4f
    OW_PAIR r6, r7, 4, .Lsb0
    movs    r11, r11, lsr #16
    beq     4f
    OW_PAIR r8, r9, 6, .Lsb1
4:  add     r0, r0, #8
    subs    r2, r2, #1
    bne     1b
    @ the last quad (or pair): texels from r4, ANDed with the tail mask,
    @ drawn by quad A's code (r2 = 0: it then leaves the row)
.Lsr_last:
    ldr     r5, [r1]
    mov     r11, r4, lsr r3
    orr     r11, r11, r5, lsl r10
    ldr     lr, [sp]
    ldr     lr, [lr, #OSJ_TAIL]
    ands    r11, r11, lr
    mov     r2, #0
    b       .Lsr_a
.Lsr_last_b:
    mov     r4, r5
    add     r0, r0, #4
    b       .Lsr_last
    @ next row
.Lsr_next:
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
    OW_PAIR_COLD r7, 0, .Lsa0
    OW_PAIR_COLD r9, 2, .Lsa1
    OW_PAIR_COLD r7, 4, .Lsb0
    OW_PAIR_COLD r9, 6, .Lsb1
    .size   ow_spr_rows, . - ow_spr_rows
