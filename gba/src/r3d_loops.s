@ r3d_loops.s - the raycaster's pixel loops, hand-scheduled ARM in IWRAM
@ (owner: gba). Mode 4, 2-pixel-wide columns: every pixel is one
@ halfword (both bytes the same colour), so VRAM never needs a
@ read-modify-write. Loads are interleaved so no ldrb result is used by
@ the very next instruction.

    .section .iwram, "ax", %progbits
    .arm
    .align 2

@ void r3d_face_loop(uint16_t *p, int n, const uint8_t *texcol, int32_t v,
@                    int32_t dv, const uint8_t *cm)
@ Textured vertical span: n > 0 rows starting at p (stride 240 bytes).
@ v / dv: texture row in 16.16 (wraps at 64). texcol = texture + u.
    .global r3d_face_loop
    .type   r3d_face_loop, %function
r3d_face_loop:
    stmfd   sp!, {r4-r8}
    ldr     r4, [sp, #20]               @ dv
    ldr     r5, [sp, #24]               @ cm
    mov     r7, #0xFC0                  @ 63 << 6
    tst     r1, #1
    beq     2f
    @ odd count: one pixel first
    and     r6, r7, r3, lsr #10
    ldrb    r6, [r2, r6]
    add     r3, r3, r4
    ldrb    r6, [r5, r6]
    subs    r1, r1, #1
    orr     r6, r6, r6, lsl #8
    strh    r6, [r0], #240
    beq     9f
2:  and     r6, r7, r3, lsr #10
    add     r3, r3, r4
    and     r8, r7, r3, lsr #10
    add     r3, r3, r4
    ldrb    r6, [r2, r6]
    ldrb    r8, [r2, r8]
    ldrb    r6, [r5, r6]
    ldrb    r8, [r5, r8]
    orr     r6, r6, r6, lsl #8
    orr     r8, r8, r8, lsl #8
    strh    r6, [r0], #240
    strh    r8, [r0], #240
    subs    r1, r1, #2
    bne     2b
9:  ldmfd   sp!, {r4-r8}
    bx      lr
    .size   r3d_face_loop, . - r3d_face_loop

@ void r3d_sky_loop(uint16_t *p, int n, const uint8_t *col, int32_t v16,
@                   int32_t dv16, const uint8_t *cm)
@ Sky column: n > 0 rows from p. col = sky + u (row stride 256); sky row =
@ v16 >> 16, v16 += dv16 per screen row (the caller keeps it inside the
@ texture for all n rows). cm = 0: no colour mapping (untinted sky).
    .global r3d_sky_loop
    .type   r3d_sky_loop, %function
r3d_sky_loop:
    stmfd   sp!, {r4-r6}
    ldr     r4, [sp, #12]               @ dv16
    ldr     r6, [sp, #16]               @ cm (0 = none)
    cmp     r6, #0
    beq     5f
1:  mov     r5, r3, lsr #16
    ldrb    r5, [r2, r5, lsl #8]
    add     r3, r3, r4
    ldrb    r5, [r6, r5]
    subs    r1, r1, #1
    orr     r5, r5, r5, lsl #8
    strh    r5, [r0], #240
    bne     1b
    ldmfd   sp!, {r4-r6}
    bx      lr
5:  mov     r5, r3, lsr #16
    ldrb    r5, [r2, r5, lsl #8]
    add     r3, r3, r4
    subs    r1, r1, #1
    orr     r5, r5, r5, lsl #8
    strh    r5, [r0], #240
    bne     5b
    ldmfd   sp!, {r4-r6}
    bx      lr
    .size   r3d_sky_loop, . - r3d_sky_loop

@ void r3d_span_loop(uint16_t *dst, int n, uint32_t pos, uint32_t step,
@                    const uint8_t *tex, const uint8_t *cm)
@ Floor / ceiling span along a screen row: n > 0 pixels (halfwords).
@ pos / step pack u and v (Doom style): u = bits 26-31 (frac 16-25),
@ v = bits 10-15 (frac 0-9); texel = tex[v * 64 + u].
    .global r3d_span_loop
    .type   r3d_span_loop, %function
r3d_span_loop:
    stmfd   sp!, {r4-r9}
    ldr     r4, [sp, #24]               @ tex
    ldr     r5, [sp, #28]               @ cm
    mov     r9, #0xFC0
    tst     r0, #2
    beq     2f
    @ dst not word aligned: one pixel first
    and     r6, r9, r2, lsr #4
    orr     r6, r6, r2, lsr #26
    ldrb    r6, [r4, r6]
    add     r2, r2, r3
    ldrb    r6, [r5, r6]
    subs    r1, r1, #1
    orr     r6, r6, r6, lsl #8
    strh    r6, [r0], #2
    beq     9f
2:  subs    r1, r1, #2
    blt     8f
3:  and     r6, r9, r2, lsr #4
    orr     r6, r6, r2, lsr #26
    add     r2, r2, r3
    and     r7, r9, r2, lsr #4
    orr     r7, r7, r2, lsr #26
    add     r2, r2, r3
    ldrb    r6, [r4, r6]
    ldrb    r7, [r4, r7]
    ldrb    r6, [r5, r6]
    ldrb    r7, [r5, r7]
    orr     r6, r6, r6, lsl #8
    orr     r7, r7, r7, lsl #8
    orr     r6, r6, r7, lsl #16
    str     r6, [r0], #4
    subs    r1, r1, #2
    bge     3b
8:  adds    r1, r1, #2
    beq     9f
    @ one pixel left
    and     r6, r9, r2, lsr #4
    orr     r6, r6, r2, lsr #26
    ldrb    r6, [r4, r6]
    ldrb    r6, [r5, r6]
    orr     r6, r6, r6, lsl #8
    strh    r6, [r0], #2
9:  ldmfd   sp!, {r4-r9}
    bx      lr
    .size   r3d_span_loop, . - r3d_span_loop

@ void r3d_fill_loop(uint16_t *p, int n, uint32_t c2, int stride_bytes)
@ Solid halfword column / row fill.
    .global r3d_fill_loop
    .type   r3d_fill_loop, %function
r3d_fill_loop:
1:  strh    r2, [r0], r3
    subs    r1, r1, #1
    bne     1b
    bx      lr
    .size   r3d_fill_loop, . - r3d_fill_loop

@ int r3d_walk(R3DWalk *w)
@ DDA fast path: step the ray through the grid until the next cell's raw
@ 8 bytes differ from the region's (wall/textures/floor_h, ceil_h/light,
@ or a door/lift flag), the ray leaves the map, or reaches max distance.
@ R3DWalk layout (r3d_int.h): 0 sdx, 4 sdy, 8 ddx, 12 ddy, 16 cx, 20 cy,
@ 24 idx8, 28 ix8, 32 iy8, 36 cells, 40 c0, 44 c1, 48 maxd,
@ out: 52 d, 56 w0, 60 w1, 64 side.
@ Returns 0 = next cell differs, 1 = left the map, 2 = max distance.
    .global r3d_walk
    .type   r3d_walk, %function
r3d_walk:
    stmfd   sp!, {r4-r11, lr}
    ldr     r12, [r0, #48]
    stmfd   sp!, {r0, r12}              @ [sp] = w, [sp, #4] = maxd
    ldmia   r0, {r0-r11}
.Lw_loop:
    cmp     r0, r1
    bhs     .Lw_y
    @ ---- x step
    ldr     r12, [sp, #4]
    cmp     r0, r12
    bhs     .Lw_far_x
    subs    r4, r4, #1
    bmi     .Lw_out_x
    add     r6, r6, r7
    add     r12, r9, r6
    add     r0, r0, r2
    ldmia   r12, {r12, lr}
    cmp     r12, r10
    bne     .Lw_diff_x
    eor     r12, lr, r11
    movs    r12, r12, lsl #14           @ ceil_h, light, door/lift flags
    beq     .Lw_loop
    mov     r12, r10
.Lw_diff_x:
    sub     r10, r0, r2                 @ d = boundary distance
    mov     r11, #0
    b       .Lw_ret_diff
.Lw_y:
    ldr     r12, [sp, #4]
    cmp     r1, r12
    bhs     .Lw_far_y
    subs    r5, r5, #1
    bmi     .Lw_out_y
    add     r6, r6, r8
    add     r12, r9, r6
    add     r1, r1, r3
    ldmia   r12, {r12, lr}
    cmp     r12, r10
    bne     .Lw_diff_y
    eor     r12, lr, r11
    movs    r12, r12, lsl #14
    beq     .Lw_loop
    mov     r12, r10
.Lw_diff_y:
    sub     r10, r1, r3
    mov     r11, #1
.Lw_ret_diff:
    ldr     r9, [sp]
    stmia   r9, {r0-r6}
    add     r9, r9, #52
    stmia   r9, {r10, r12, lr}          @ d, w0, w1
    str     r11, [r9, #12]              @ side
    mov     r0, #0
    add     sp, sp, #8
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
.Lw_far_x:
    mov     r10, r0
    mov     r11, #0
    mov     r12, #2
    b       .Lw_ret
.Lw_far_y:
    mov     r10, r1
    mov     r11, #1
    mov     r12, #2
    b       .Lw_ret
.Lw_out_x:
    mov     r4, #0
    mov     r10, r0
    mov     r11, #0
    mov     r12, #1
    b       .Lw_ret
.Lw_out_y:
    mov     r5, #0
    mov     r10, r1
    mov     r11, #1
    mov     r12, #1
.Lw_ret:
    ldr     r9, [sp]
    stmia   r9, {r0-r6}
    str     r10, [r9, #52]
    str     r11, [r9, #64]
    mov     r0, r12
    add     sp, sp, #8
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
    .size   r3d_walk, . - r3d_walk

@ void r3d_flat_loop(R3DFlat *f)
@ Floor / ceiling cast down one column: f->n > 0 rows from f->p (stride
@ 240 bytes). Per row: K = *kp (16.16 proj/(m+.5)), kp += +-4 (floors walk
@ the table up, ceilings down); u = u0 + K*A, v = v0 + K*B with 26
@ fractional bits, so the top 6 bits are the texel (wraps mod 64 for
@ free); texel = tex[v_int * 64 + u_int]. u and v are exact every 4th
@ row and linear in between (the perspective error over 4 rows is far
@ below a texel wherever texels are bigger than a row; nearer the horizon
@ the texture is minified and aliased anyway). The distance light is
@ refreshed every 16 rows (when the rows left reach a multiple of 16): D = (K >> 8) * hd (8 frac bits), level = L -
@ fade[D >> 13]; rows beyond `far` (same units) use the texture's
@ average colour (no far-away shimmer).
@ R3DFlat: 0 p, 4 n, 8 kp, 12 kdir (+4 / -4), 16 u0, 20 v0, 24 A, 28 B,
@          32 tex, 36 cm (16 x 256 colormap base), 40 L, 44 hd, 48 fade,
@          52 far, 56 avg
@ Registers: r0 p, r12 rows left, r2 kp (the current row), r4 u0, r5 v0,
@ r6 A, r7 B, r8 tex, r11 lit colormap row, r3 / r10 u / v of the current
@ row, r1 / r9 du / dv, lr scratch (FL reloaded at each light refresh).
@ The K table has guard entries on both sides (one block may read up to
@ 4 rows past the end of the run).
    .equ FLL_CM,    36
    .equ FLL_L,     40
    .equ FLL_HD,    44
    .equ FLL_FADE,  48
    .equ FLL_FAR,   52
    .equ FLL_AVG,   56
    .macro FLATLOOP kstep, name
\name\()_group:
    ldr     lr, =FL
    ldr     r10, [r2]                   @ K of the current row
    ldr     r11, [lr, #FLL_HD]
    mov     r1, r10, lsr #8
    mul     r3, r1, r11                 @ D, 8 frac bits
    mov     r1, r3, lsr #13             @ 32-unit buckets
    cmp     r1, #127
    movhi   r1, #127
    ldr     r11, [lr, #FLL_FADE]
    ldrb    r1, [r11, r1]
    ldr     r11, [lr, #FLL_L]
    sub     r1, r11, r1
    cmp     r1, #1
    movlt   r1, #1
    ldr     r11, [lr, #FLL_CM]
    add     r11, r11, r1, lsl #8        @ lit colormap row
    ldr     r1, [lr, #FLL_FAR]
    cmp     r3, r1
    bhi     \name\()_far
    mla     r3, r10, r6, r4             @ exact u, v of the current row
    mla     r10, r7, r10, r5
\name\()_single:
    @ single rows until the count is a multiple of 4
    tst     r12, #3
    beq     \name\()_blk
    mov     lr, r10, lsr #26
    mov     lr, lr, lsl #6
    add     lr, lr, r3, lsr #26
    ldrb    lr, [r8, lr]
    ldr     r9, [r2, #\kstep]!
    ldrb    lr, [r11, lr]
    mla     r3, r9, r6, r4
    mla     r10, r7, r9, r5
    orr     lr, lr, lr, lsl #8
    strh    lr, [r0], #240
    subs    r12, r12, #1
    beq     .Lfl_done
    tst     r12, #15
    beq     \name\()_group
    b       \name\()_single
\name\()_blk:
    @ 4 rows: exact at both ends, linear in between
    ldr     r9, [r2, #4*\kstep]!
    mla     r1, r9, r6, r4
    mla     r9, r7, r9, r5
    sub     r1, r1, r3
    mov     r1, r1, asr #2              @ du
    sub     r9, r9, r10
    mov     r9, r9, asr #2              @ dv
    .rept 4
    mov     lr, r10, lsr #26
    mov     lr, lr, lsl #6
    add     lr, lr, r3, lsr #26
    ldrb    lr, [r8, lr]
    add     r3, r3, r1
    add     r10, r10, r9
    ldrb    lr, [r11, lr]
    orr     lr, lr, lr, lsl #8
    strh    lr, [r0], #240
    .endr
    subs    r12, r12, #4
    beq     .Lfl_done
    tst     r12, #12
    bne     \name\()_blk
    b       \name\()_group
\name\()_far:
    ldr     r3, [lr, #FLL_AVG]          @ average texel
    ldrb    r3, [r11, r3]
    orr     r3, r3, r3, lsl #8
\name\()_farpx:
    strh    r3, [r0], #240
    add     r2, r2, #\kstep
    subs    r12, r12, #1
    beq     .Lfl_done
    tst     r12, #15
    bne     \name\()_farpx
    b       \name\()_group
    .endm

    .global r3d_flat_loop
    .type   r3d_flat_loop, %function
r3d_flat_loop:
    stmfd   sp!, {r4-r11, lr}
    ldmia   r0, {r0-r8}                 @ p n kp kdir u0 v0 A B tex
    mov     r12, r1
    cmp     r3, #0
    blt     .Lfl_ceil
    FLATLOOP 4, .Lfl_floor
.Lfl_ceil:
    FLATLOOP -4, .Lfl_ceiling
.Lfl_done:
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
    .ltorg
    .size   r3d_flat_loop, . - r3d_flat_loop

@ ---------------------------------------------------------------------------
@ int r3d_boundary_asm(ColState *s, int res)
@ One boundary of the column pass, in assembly (the C boundary() in
@ r3d.iwram.c is the reference and handles what is left: door / lift
@ cells and roofed cells seen from under the sky). res 0 = the next cell
@ (wk.w0 / wk.w1) differs from the region, res 1 = the ray left the map.
@   open next cell : close the floor run and/or the ceiling run when their
@                    properties change, riser, upper face, take it over
@   solid with a top lower than the ceiling (or under the sky) : a block,
@                    draw its side and walk over its top
@   solid otherwise (or outside) : ceiling / sky, wall face, column done
@ then push a clip checkpoint if the window changed and test for the end.
@ Returns 0 = keep walking, 1 = column finished, 2 = not handled.
    .equ CS_D,      52
    .equ CS_W0,     56
    .equ CS_W1,     60
    .equ CS_SIDE,   64
    .equ CS_C0,     40
    .equ CS_C1,     44
    .equ CS_RDX,    68
    .equ CS_RDY,    72
    .equ CS_STX,    76
    .equ CS_STY,    80
    .equ CS_FZ4,    84
    .equ CS_CZ4,    88
    .equ CS_FTEX,   92
    .equ CS_CTEX,   96
    .equ CS_LIGHT,  100
    .equ CS_SKY,    104
    .equ CS_YTOP,   108
    .equ CS_YBOT,   112
    .equ CS_C,      116
    .equ CS_NCP,    120
    .equ CS_COL,    124
    .equ CS_CM,     128
    .equ CS_CPS,    132
    .equ CS_EZ4,    136
    .equ CS_HOR12R, 140
    .equ CS_PROJ,   144
    .equ CS_ETOP,   148
    .equ CS_EX,     152
    .equ CS_EY,     156
    .equ CS_HOR,    160
    .equ CS_INVPROJ, 164
    .equ CS_EZ,     168
    .equ CS_D16,    172
    .equ CS_K8,     176
    .equ CS_AMB,    180
    .equ FL_P,      0
    .equ FL_N,      4
    .equ FL_KP,     8
    .equ FL_KDIR,   12
    .equ FL_A,      24
    .equ FL_B,      28
    .equ FL_TEX,    32
    .equ FL_L,      40
    .equ FL_HD,     44
    .equ FL_AVG,    56
    .equ MAXCP,     14
    .equ TEX_SKY,   255

    .global r3d_boundary_asm
    .type   r3d_boundary_asm, %function
r3d_boundary_asm:
    stmfd   sp!, {r4-r11, lr}
    sub     sp, sp, #12                 @ [sp] old ytop, [sp+4] old ybot, [sp+8] done
    mov     r11, r0
    cmp     r1, #0
    movne   r4, #1                      @ outside the map: solid, texture 1
    movne   r5, #0
    ldreq   r4, [r11, #CS_W0]
    ldreq   r5, [r11, #CS_W1]
    mov     r9, r1                      @ r9 = res
    tst     r5, #0x30000                @ door / lift -> C
    bne     .Lba_slow
    tst     r4, #0xFF
    bne     1f
    @ open: a roofed cell seen from under the sky -> C
    ldr     r0, [r11, #CS_SKY]
    cmp     r0, #0
    beq     1f
    mov     r1, r4, lsr #16
    and     r1, r1, #0xFF
    cmp     r1, #TEX_SKY
    bne     .Lba_slow
1:
    @ ---- scale at this boundary
    ldr     r0, [r11, #CS_D]
    mov     r0, r0, lsl #6
    str     r0, [r11, #CS_D16]
    mov     r1, r0, lsr #13
    ldr     r2, =32767
    cmp     r1, r2
    movhi   r1, r2
    ldr     r2, =gba_recip8
    ldr     r1, [r2, r1, lsl #2]
    ldr     r2, [r11, #CS_PROJ]
    mul     r10, r1, r2
    mov     r10, r10, lsr #15           @ r10 = k8
    ldr     r8, [r11, #CS_EZ4]          @ r8 = ez4
    ldr     r0, [r11, #CS_YTOP]
    ldr     r1, [r11, #CS_YBOT]
    mov     r2, #0
    stmia   sp, {r0, r1, r2}
    tst     r4, #0xFF
    bne     .Lba_solid

    @ ======== open next cell ========
    mov     r6, r4, asr #24
    mov     r6, r6, lsl #7              @ r6 = nfz4
    ldr     r7, [r11, #CS_FZ4]          @ r7 = region fz4
    @ floor run closes when floor height, texture or light change
    cmp     r6, r7
    bne     2f
    ldr     r0, [r11, #CS_FTEX]
    mov     r1, r4, lsr #8
    and     r1, r1, #0xFF
    cmp     r0, r1
    bne     2f
    ldr     r0, [r11, #CS_LIGHT]
    mov     r1, r5, lsr #8
    ands    r1, r1, #0xFF
    ldreq   r1, [r11, #CS_AMB]
    cmp     r0, r1
    beq     3f
2:  bl      .Lba_close_floor
3:
    @ ceiling run (never under the sky)
    ldr     r0, [r11, #CS_SKY]
    cmp     r0, #0
    bne     5f
    mov     r1, r4, lsr #16
    and     r1, r1, #0xFF               @ nctex
    cmp     r1, #TEX_SKY
    beq     4f
    mov     r2, r5, lsl #24
    mov     r2, r2, asr #17             @ ncz4 = (int8)ceil_h << 7
    ldr     r3, [r11, #CS_CZ4]
    cmp     r2, r3
    bne     4f
    ldr     r3, [r11, #CS_CTEX]
    cmp     r1, r3
    bne     4f
    ldr     r0, [r11, #CS_LIGHT]
    mov     r1, r5, lsr #8
    ands    r1, r1, #0xFF
    ldreq   r1, [r11, #CS_AMB]
    cmp     r0, r1
    beq     5f
4:  bl      .Lba_close_ceil
5:
    @ riser: the next floor is higher
    cmp     r6, r7
    ble     6f
    ldr     r2, [r11, #CS_YTOP]
    ldr     r3, [r11, #CS_YBOT]
    cmp     r2, r3
    bge     6f
    mov     r0, r6
    bl      .Lba_rowof                  @ yr
    ldr     r2, [r11, #CS_YTOP]
    cmp     r0, r2
    movlt   r0, r2                      @ y0 = max(yr, ytop)
    mov     r9, r0
    mov     r0, r7
    bl      .Lba_rowof                  @ yf
    ldr     r3, [r11, #CS_YBOT]
    cmp     r0, r3
    movgt   r0, r3                      @ y1 = min(yf, ybot)
    mov     r1, r0
    mov     r0, r9
    cmp     r0, r3
    strlt   r0, [r11, #CS_YBOT]         @ if (y0 < ybot) ybot = y0
    cmp     r0, r1
    bge     6f
    mov     r2, r4, lsr #8
    and     r2, r2, #0xFF               @ next floor texture
    ldr     r3, [r11, #CS_LIGHT]
    sub     r3, r3, #1                  @ step faces: one level darker
    bl      .Lba_face
6:
    @ upper face: the next ceiling is lower (region not under the sky)
    ldr     r0, [r11, #CS_SKY]
    cmp     r0, #0
    bne     7f
    ldr     r2, [r11, #CS_YTOP]
    ldr     r3, [r11, #CS_YBOT]
    cmp     r2, r3
    bge     7f
    mov     r0, r4, lsr #16
    and     r0, r0, #0xFF
    cmp     r0, #TEX_SKY                @ the ceiling opens to the sky: no face
    beq     7f
    mov     r0, r5, lsl #24
    mov     r0, r0, asr #17             @ ncz4
    ldr     r1, [r11, #CS_CZ4]
    cmp     r0, r1
    bge     7f
    bl      .Lba_rowof                  @ yu
    ldr     r3, [r11, #CS_YBOT]
    cmp     r0, r3
    movgt   r0, r3                      @ y1 = min(yu, ybot)
    mov     r1, r0
    ldr     r0, [r11, #CS_YTOP]
    cmp     r0, r1
    bge     7f
    str     r1, [r11, #CS_YTOP]
    mov     r2, r4, lsr #16
    and     r2, r2, #0xFF
    ldr     r3, [r11, #CS_LIGHT]
    sub     r3, r3, #1
    bl      .Lba_face
7:
    @ the next cell is the region now
    str     r6, [r11, #CS_FZ4]
    mov     r0, r5, lsl #24
    mov     r0, r0, asr #17
    str     r0, [r11, #CS_CZ4]
    mov     r0, r4, lsr #8
    and     r0, r0, #0xFF
    str     r0, [r11, #CS_FTEX]
    mov     r0, r4, lsr #16
    and     r0, r0, #0xFF
    str     r0, [r11, #CS_CTEX]
    cmp     r0, #TEX_SKY
    moveq   r0, #1
    movne   r0, #0
    str     r0, [r11, #CS_SKY]
    mov     r0, r5, lsr #8
    ands    r0, r0, #0xFF
    ldreq   r0, [r11, #CS_AMB]
    str     r0, [r11, #CS_LIGHT]
    str     r4, [r11, #CS_C0]
    str     r5, [r11, #CS_C1]
    b       .Lba_checkpoint

    @ ======== solid next cell (or outside) ========
.Lba_solid:
    bl      .Lba_close_floor
    ldr     r0, [r11, #CS_SKY]
    cmp     r0, #0
    beq     .Lba_wall
    @ ---- under the sky: a block as tall as its ceil_h, else 256 units
    ldr     r7, [r11, #CS_FZ4]
    add     r6, r7, #(256 << 4)         @ default top
    cmp     r9, #0
    bne     1f
    mov     r0, r4, asr #24             @ fh
    mov     r1, r5, lsl #24
    mov     r1, r1, asr #24             @ ch
    cmp     r1, r0
    movgt   r6, r1, lsl #7              @ explicit top
1:  cmp     r6, r7
    ble     2f
    mov     r0, r6
    bl      .Lba_rowof                  @ yt
    ldr     r2, [r11, #CS_YTOP]
    cmp     r0, r2
    movlt   r0, r2                      @ y0 = max(yt, ytop)
    mov     r1, r0
    mov     r0, r7
    bl      .Lba_rowof                  @ yf
    ldr     r3, [r11, #CS_YBOT]
    cmp     r0, r3
    movgt   r0, r3                      @ y1 = min(yf, ybot)
    mov     r12, r0
    mov     r0, r1
    mov     r1, r12
    cmp     r0, r3
    strlt   r0, [r11, #CS_YBOT]
    cmp     r0, r1
    bge     2f
    and     r2, r4, #0xFF               @ wall texture
    ldr     r3, [r11, #CS_LIGHT]
    bl      .Lba_face
2:  cmp     r9, #0
    beq     3f
    @ the map's edge: sky above, column done
    ldr     r0, [r11, #CS_YTOP]
    ldr     r1, [r11, #CS_YBOT]
    cmp     r0, r1
    blt     .Lba_edge_sky
    b       .Lba_mark_done
3:  @ walk across its roof
    str     r6, [r11, #CS_FZ4]
    mov     r0, r4, lsr #8
    ands    r0, r0, #0xFF
    andeq   r0, r4, #0xFF               @ roof texture: floor_tex or the wall's
    str     r0, [r11, #CS_FTEX]
    mov     r0, r5, lsr #8
    ands    r0, r0, #0xFF
    ldreq   r0, [r11, #CS_AMB]
    str     r0, [r11, #CS_LIGHT]
    str     r4, [r11, #CS_C0]           @ identical blocks behind walk fast
    str     r5, [r11, #CS_C1]
    b       .Lba_checkpoint

.Lba_edge_sky:
    str     r1, [r11, #CS_YTOP]
    bl      .Lba_sky
    b       .Lba_mark_done

.Lba_wall:
    @ ---- under a roof: a wall up to the ceiling closes the column
    bl      .Lba_close_ceil
    ldr     r0, [r11, #CS_YTOP]
    ldr     r1, [r11, #CS_YBOT]
    cmp     r0, r1
    bge     .Lba_mark_done
    str     r1, [r11, #CS_YTOP]
    and     r2, r4, #0xFF
    ldr     r3, [r11, #CS_LIGHT]
    bl      .Lba_face
.Lba_mark_done:
    ldr     r1, [r11, #CS_YBOT]
    str     r1, [r11, #CS_YTOP]
    mov     r0, #1
    str     r0, [sp, #8]                @ done

    @ ======== checkpoint + end tests ========
.Lba_checkpoint:
    ldr     r2, [r11, #CS_YTOP]
    ldr     r3, [r11, #CS_YBOT]
    ldmia   sp, {r0, r1}
    cmp     r2, r0
    cmpeq   r3, r1
    beq     4f
    ldr     r0, [r11, #CS_NCP]
    ldr     r1, [r11, #CS_CPS]
    cmp     r0, #MAXCP
    addlt   r12, r0, #1
    strlt   r12, [r11, #CS_NCP]
    movge   r0, #MAXCP - 1
    add     r1, r1, r0, lsl #2
    ldr     r12, [r11, #CS_D16]
    mov     r12, r12, lsr #13           @ d8
    cmp     r12, #0x10000
    movhs   r12, #0xFF00
    orrhs   r12, r12, #0xFF
    strh    r12, [r1]
    cmp     r2, #255
    movgt   r12, #255
    movle   r12, r2
    cmp     r12, #0
    movlt   r12, #0
    strb    r12, [r1, #2]
    cmp     r3, #255
    movgt   r12, #255
    movle   r12, r3
    cmp     r12, #0
    movlt   r12, #0
    strb    r12, [r1, #3]
4:  ldr     r0, [sp, #8]
    cmp     r0, #0
    bne     .Lba_done
    cmp     r2, r3
    bge     .Lba_done
    @ under the sky: done when the open rows are above the tallest top
    ldr     r0, [r11, #CS_SKY]
    cmp     r0, #0
    beq     .Lba_cont
    ldr     r0, [r11, #CS_ETOP]
    cmp     r0, #0x80000000
    beq     .Lba_cont
    bl      .Lba_rowof
    cmp     r3, r0
    ble     .Lba_done
.Lba_cont:
    mov     r0, #0
    b       .Lba_ret
.Lba_done:
    mov     r0, #1
    b       .Lba_ret
.Lba_slow:
    mov     r0, #2
.Lba_ret:
    add     sp, sp, #12
    ldmfd   sp!, {r4-r11, lr}
    bx      lr

@ ---- r0 = z4 -> r0 = row edge (uses r8 = ez4, r10 = k8); keeps r1-r11
.Lba_rowof:
    sub     r0, r8, r0
    ldr     r12, [r11, #CS_HOR12R]
    mla     r0, r10, r0, r12
    mov     r0, r0, asr #12
    bx      lr

@ ---- close the region's floor run (keeps r4-r11)
.Lba_close_floor:
    ldr     r0, [r11, #CS_FZ4]
    cmp     r0, r8
    bxge    lr                          @ eye at / below the floor: invisible
    stmfd   sp!, {r4, lr}
    bl      .Lba_rowof                  @ yf
    ldr     r12, [r11, #CS_YBOT]
    cmp     r0, r12
    bge     1f
    ldr     r3, [r11, #CS_YTOP]
    cmp     r0, r3
    movge   r3, r0                      @ y0 = tp = max(yf, ytop)
    str     r3, [r11, #CS_YBOT]
    ldr     r0, [r11, #CS_FZ4]
    ldr     r1, [r11, #CS_FTEX]
    ldr     r2, [r11, #CS_LIGHT]
    bl      .Lba_flat                   @ rows [tp, old ybot)
1:  ldmfd   sp!, {r4, lr}
    bx      lr

@ ---- close the region's ceiling run (keeps r4-r11)
.Lba_close_ceil:
    ldr     r0, [r11, #CS_CZ4]
    cmp     r0, r8
    bxle    lr
    stmfd   sp!, {r4, lr}
    bl      .Lba_rowof                  @ yc
    ldr     r3, [r11, #CS_YTOP]
    cmp     r0, r3
    ble     1f
    ldr     r12, [r11, #CS_YBOT]
    cmp     r0, r12
    movgt   r0, r12                     @ bt = min(yc, ybot)
    mov     r12, r0
    str     r12, [r11, #CS_YTOP]
    ldr     r0, [r11, #CS_CZ4]
    ldr     r1, [r11, #CS_CTEX]
    ldr     r2, [r11, #CS_LIGHT]
    bl      .Lba_flat                   @ rows [old ytop, bt)
1:  ldmfd   sp!, {r4, lr}
    bx      lr

@ ---- flat: r0 = z4, r1 = tex, r2 = light, r3 = y0, r12 = y1 (keeps r4-r11)
.Lba_flat:
    cmp     r3, r12
    bxge    lr
    stmfd   sp!, {r4-r7, lr}
    ldr     lr, =FL
    sub     r4, r12, r3
    str     r4, [lr, #FL_N]
    ldr     r4, [r11, #CS_COL]
    add     r4, r4, r3, lsl #8
    sub     r4, r4, r3, lsl #4          @ col + y0 * 240
    str     r4, [lr, #FL_P]
    str     r2, [lr, #FL_L]
    subs    r5, r8, r0                  @ hd4 = ez4 - z4
    ldr     r4, [r11, #CS_HOR]
    movgt   r6, #4                      @ floor
    subgt   r4, r3, r4                  @ m0 = y0 - hor
    mvnle   r6, #3                      @ ceiling: kdir = -4
    suble   r4, r4, r3
    suble   r4, r4, #1                  @ m0 = hor - 1 - y0
    rsble   r5, r5, #0
    str     r6, [lr, #FL_KDIR]
    cmp     r4, #0
    movlt   r4, #0
    cmp     r4, #255
    movgt   r4, #255
    ldr     r6, =r3d_rowk_buf + 16
    add     r4, r6, r4, lsl #2
    str     r4, [lr, #FL_KP]
    cmp     r5, #0x4000
    movge   r5, #0x4000
    subge   r5, r5, #1
    ldr     r6, [r11, #CS_RDX]
    mul     r7, r5, r6
    mov     r7, r7, asr #10
    str     r7, [lr, #FL_A]
    ldr     r6, [r11, #CS_RDY]
    mul     r7, r5, r6
    mov     r7, r7, asr #10
    str     r7, [lr, #FL_B]
    mov     r7, r5, asr #4
    add     r7, r7, #1
    str     r7, [lr, #FL_HD]
    mov     r4, r1                      @ texture id
    mov     r0, r1
    bl      .Lba_texof
    ldr     lr, =FL
    str     r0, [lr, #FL_TEX]
    mov     r0, r4
    bl      .Lba_avgof
    ldr     lr, =FL
    str     r0, [lr, #FL_AVG]
    mov     r0, lr
    bl      r3d_flat_loop
    ldmfd   sp!, {r4-r7, lr}
    bx      lr

@ ---- face: r0 = y0, r1 = y1 (y0 < y1), r2 = tex id, r3 = light (keeps r4-r11)
.Lba_face:
    stmfd   sp!, {r4-r9, lr}
    mov     r4, r0                      @ y0
    mov     r5, r1                      @ y1
    mov     r9, r3                      @ light
    mov     r0, r2
    bl      .Lba_texof
    mov     r6, r0                      @ texture
    @ u = hit coordinate along the boundary
    ldr     r2, [r11, #CS_D16]
    ldr     r3, [r11, #CS_SIDE]
    cmp     r3, #0
    ldreq   r0, [r11, #CS_RDY]
    ldrne   r0, [r11, #CS_RDX]
    smull   r12, r1, r0, r2
    mov     r12, r12, lsr #16
    orr     r12, r12, r1, lsl #16
    ldreq   r1, [r11, #CS_EY]
    ldrne   r1, [r11, #CS_EX]
    add     r12, r12, r1
    mov     r12, r12, lsr #16
    and     r12, r12, #63
    ldreq   r1, [r11, #CS_STX]
    ldrne   r1, [r11, #CS_STY]
    rsbne   r1, r1, #0
    cmp     r1, #0
    rsblt   r12, r12, #63
    add     r6, r6, r12                 @ texture column
    @ light - side - fade[d16 >> 21] -> colormap row
    sub     r9, r9, r3
    mov     r1, r2, lsr #21
    cmp     r1, #127
    movhi   r1, #127
    ldr     r0, =r3d_fade
    ldrb    r1, [r0, r1]
    sub     r9, r9, r1
    cmp     r9, #1
    movlt   r9, #1
    cmp     r9, #15
    movgt   r9, #15
    ldr     r0, [r11, #CS_CM]
    add     r9, r0, r9, lsl #8
    @ dv = d16 / proj, v = ((y0 - hor) * 2 + 1) * (dv / 2) - ez
    ldr     r0, [r11, #CS_INVPROJ]
    umull   r1, r7, r2, r0              @ r7 = dv
    ldr     r2, [r11, #CS_HOR]
    sub     r2, r4, r2
    mov     r2, r2, lsl #1
    add     r2, r2, #1
    mov     r1, r7, lsr #1
    mul     r3, r2, r1
    ldr     r2, [r11, #CS_EZ]
    sub     r3, r3, r2                  @ v
    stmfd   sp!, {r7, r9}               @ args 5, 6: dv, cm
    ldr     r0, [r11, #CS_COL]
    add     r0, r0, r4, lsl #8
    sub     r0, r0, r4, lsl #4          @ p = col + y0 * 240
    sub     r1, r5, r4                  @ n
    mov     r2, r6
    bl      r3d_face_loop
    add     sp, sp, #8
    ldmfd   sp!, {r4-r9, lr}
    bx      lr

@ ---- sky: rows [r0, r1) (keeps r4-r11)
.Lba_sky:
    stmfd   sp!, {r4, lr}
    ldr     r12, [r11, #CS_CM]
    str     r12, [sp, #-8]!             @ arg 5 (keeps sp 8-aligned)
    mov     r3, r1
    mov     r2, r0
    ldr     r0, [r11, #CS_COL]
    ldr     r1, [r11, #CS_C]
    bl      r3d_draw_sky
    add     sp, sp, #8
    ldmfd   sp!, {r4, lr}
    bx      lr

@ r0 = texture id -> r0 = pixels (r3d_texc cache, else r3d_texture()).
.Lba_texof:
    and     r0, r0, #63
    ldr     r12, =r3d_texc
    ldr     r12, [r12, r0, lsl #2]
    cmp     r12, #0
    movne   r0, r12
    bxne    lr
    stmfd   sp!, {r1-r3, lr}
    ldr     r12, =r3d_texture
    mov     lr, pc
    bx      r12
    ldmfd   sp!, {r1-r3, lr}
    bx      lr

@ r0 = texture id -> r0 = average texel (r3d_avgc cache, else r3d_texavg()).
.Lba_avgof:
    and     r0, r0, #63
    ldr     r12, =r3d_avgc
    ldrb    r12, [r12, r0]
    cmp     r12, #0
    movne   r0, r12
    bxne    lr
    stmfd   sp!, {r1-r3, lr}
    ldr     r12, =r3d_texavg
    mov     lr, pc
    bx      r12
    ldmfd   sp!, {r1-r3, lr}
    bx      lr
    .pool
    .size   r3d_boundary_asm, . - r3d_boundary_asm

@ ---------------------------------------------------------------------------
@ void r3d_sprite_loop(uint16_t *p, int n, const uint8_t *src, uint32_t vb,
@                      uint32_t dvb, const uint8_t *cm, int stride)
@ One sprite column: n > 0 rows from p (stride bytes apart: 240, or 480
@ for the translucent checkerboard). src = image + u; vb / dvb = texture
@ row * image width in 16.16 (so texel = src[vb >> 16]); index 0 is
@ transparent, everything else goes through the colormap row cm.
    .global r3d_sprite_loop
    .type   r3d_sprite_loop, %function
r3d_sprite_loop:
    stmfd   sp!, {r4-r7}
    ldr     r4, [sp, #16]               @ dvb
    ldr     r5, [sp, #20]               @ cm
    ldr     r6, [sp, #24]               @ stride
1:  mov     r7, r3, lsr #16
    ldrb    r7, [r2, r7]
    add     r3, r3, r4
    cmp     r7, #0
    ldrneb  r7, [r5, r7]
    orrne   r7, r7, r7, lsl #8
    strneh  r7, [r0]
    add     r0, r0, r6
    subs    r1, r1, #1
    bne     1b
    ldmfd   sp!, {r4-r7}
    bx      lr
    .size   r3d_sprite_loop, . - r3d_sprite_loop
