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

@ void r3d_sky_loop(uint16_t *p, int n, const uint8_t *col, int v2,
@                   const uint8_t *cm)
@ Sky column: n > 0 rows from p. col = sky + u (row stride 256). v2 is the
@ sky row times 2 (two screen rows per sky row), already clamped to the
@ valid range for all n rows by the caller. cm = 0: no colour mapping.
    .global r3d_sky_loop
    .type   r3d_sky_loop, %function
r3d_sky_loop:
    stmfd   sp!, {r4-r6}
    ldr     r4, [sp, #12]               @ cm (0 = none)
    tst     r3, #1
    beq     2f
    @ odd v2: the second row of a sky texel first
    mov     r5, r3, lsr #1
    ldrb    r5, [r2, r5, lsl #8]
    cmp     r4, #0
    ldrneb  r5, [r4, r5]
    add     r3, r3, #1
    orr     r5, r5, r5, lsl #8
    strh    r5, [r0], #240
    subs    r1, r1, #1
    beq     9f
2:  subs    r1, r1, #2
    blt     8f
    cmp     r4, #0
    beq     5f
3:  mov     r5, r3, lsr #1
    ldrb    r5, [r2, r5, lsl #8]
    add     r3, r3, #2
    ldrb    r5, [r4, r5]
    subs    r1, r1, #2
    orr     r5, r5, r5, lsl #8
    strh    r5, [r0], #240
    strh    r5, [r0], #240
    bge     3b
    b       8f
5:  mov     r5, r3, lsr #1
    ldrb    r5, [r2, r5, lsl #8]
    add     r3, r3, #2
    subs    r1, r1, #2
    orr     r5, r5, r5, lsl #8
    strh    r5, [r0], #240
    strh    r5, [r0], #240
    bge     5b
8:  adds    r1, r1, #2
    beq     9f
    @ one row left
    mov     r5, r3, lsr #1
    ldrb    r5, [r2, r5, lsl #8]
    cmp     r4, #0
    ldrneb  r5, [r4, r5]
    orr     r5, r5, r5, lsl #8
    strh    r5, [r0], #240
9:  ldmfd   sp!, {r4-r6}
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
@ free); texel = tex[v_int * 64 + u_int]. The distance
@ light is refreshed (every 8 rows): D = (K >> 8) * hd (8 frac bits), level = L -
@ fade[D >> 13], and rows beyond `far` (same units) use the texture's
@ average colour (no far-away shimmer).
@ R3DFlat: 0 p, 4 n, 8 kp, 12 kdir (+4 / -4), 16 u0, 20 v0, 24 A, 28 B,
@          32 tex, 36 cm (16 x 256 colormap base), 40 L, 44 hd, 48 fade,
@          52 far, 56 avg
    .macro FLATLOOP kstep, name
\name\()_group:
    ldr     r10, [r2]                   @ K of the first row of the group
    ldr     r11, [lr, #44]              @ hd
    mov     r10, r10, lsr #8
    mul     r3, r10, r11                @ D, 8 frac bits
    mov     r10, r3, lsr #13            @ 32-unit buckets
    cmp     r10, #127
    movhi   r10, #127
    ldr     r12, [lr, #48]              @ fade table
    ldrb    r10, [r12, r10]
    ldr     r12, [lr, #40]              @ L
    subs    r10, r12, r10
    movmi   r10, #0
    add     r11, r9, r10, lsl #8        @ r11 = lit colormap row
    ldr     r12, [lr, #52]              @ far
    cmp     r3, r12
    bhi     \name\()_far
\name\()_px:
    ldr     r10, [r2], #\kstep          @ K
    mla     r3, r10, r6, r4             @ u
    mla     r12, r10, r7, r5            @ v
    mov     r3, r3, lsr #26
    mov     r12, r12, lsr #26
    add     r3, r3, r12, lsl #6
    ldrb    r3, [r8, r3]
    ldrb    r3, [r11, r3]
    subs    r1, r1, #1
    orr     r3, r3, r3, lsl #8
    strh    r3, [r0], #240
    beq     .Lfl_done
    tst     r1, #7
    bne     \name\()_px
    b       \name\()_group
\name\()_far:
    ldr     r3, [lr, #56]               @ average texel
    ldrb    r3, [r11, r3]
    orr     r3, r3, r3, lsl #8
\name\()_farpx:
    strh    r3, [r0], #240
    add     r2, r2, #\kstep
    subs    r1, r1, #1
    beq     .Lfl_done
    tst     r1, #7
    bne     \name\()_farpx
    b       \name\()_group
    .endm

    .global r3d_flat_loop
    .type   r3d_flat_loop, %function
r3d_flat_loop:
    stmfd   sp!, {r4-r11, lr}
    mov     lr, r0                      @ lr = struct
    ldmia   lr, {r0-r9}                 @ p n kp kdir u0 v0 A B tex cm
    cmp     r3, #0
    blt     .Lfl_ceil
    FLATLOOP 4, .Lfl_floor
.Lfl_ceil:
    FLATLOOP -4, .Lfl_ceiling
.Lfl_done:
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
    .size   r3d_flat_loop, . - r3d_flat_loop
