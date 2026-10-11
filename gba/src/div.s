@ div.s - integer division in ARM code, running from IWRAM (owner: gba).
@
@ The ARM7TDMI has no divide instruction. libgcc's division helpers are
@ ARM code that would run from the 16-bit cartridge bus (about 4x slower
@ than IWRAM), and its 64-bit division is generic C. The core divides a
@ lot (UI shapes, fixed point), so these replace them: our objects are
@ linked before libgcc.a, so its members defining these symbols are never
@ pulled in.
@
@ 32-bit: quotient bit count found by a 5-step binary search, then a jump
@ into a fully unrolled shift-subtract sequence, 3 cycles per quotient bit.
@ 64-bit signed (__aeabi_ldivmod, int64_t '/' and '%'): operands that fit
@ in 32 bits take the 32-bit path; anything else goes to libgcc's
@ __udivmoddi4.
@ Division by zero returns quotient 0 (remainder = numerator) instead of
@ trapping.

    .section .iwram, "ax", %progbits
    .arm
    .align 2

@ ---------------------------------------------------------------------------
@ core: r0 = n, r1 = d (unsigned, d != 0) -> r0 = n / d, r1 = n % d
@ clobbers r2, r3, r12
    .type   gba_udivmod, %function
gba_udivmod:                        @ named for the profiler
.Ludivmod_core:
    cmp     r0, r1
    movlo   r1, r0
    movlo   r0, #0
    bxlo    lr
    @ k = largest shift with (n >> k) >= d  (0..31)
    mov     r2, #0
    cmp     r1, r0, lsr #16
    movls   r2, #16
    add     r12, r2, #8
    cmp     r1, r0, lsr r12
    movls   r2, r12
    add     r12, r2, #4
    cmp     r1, r0, lsr r12
    movls   r2, r12
    add     r12, r2, #2
    cmp     r1, r0, lsr r12
    movls   r2, r12
    add     r12, r2, #1
    cmp     r1, r0, lsr r12
    movls   r2, r12
    @ jump to step k: entry = .Lsteps + (31 - k) * 12
    rsb     r2, r2, #31
    add     r2, r2, r2, lsl #1
    mov     r3, #0
    add     pc, pc, r2, lsl #2
    nop
.Lsteps:
    .irp    b, 31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
    cmp     r0, r1, lsl #\b
    subcs   r0, r0, r1, lsl #\b
    adc     r3, r3, r3
    .endr
    mov     r1, r0
    mov     r0, r3
    bx      lr

@ ---------------------------------------------------------------------------
@ unsigned
    .global __aeabi_uidiv
    .type   __aeabi_uidiv, %function
    .global __aeabi_uidivmod
    .type   __aeabi_uidivmod, %function
    .global __udivsi3
    .type   __udivsi3, %function
__aeabi_uidiv:
__aeabi_uidivmod:
__udivsi3:
    cmp     r1, #0
    moveq   r1, r0
    moveq   r0, #0
    bxeq    lr
    b       .Ludivmod_core

@ ---------------------------------------------------------------------------
@ signed: quotient truncates toward zero, remainder has the sign of n
    .global __aeabi_idiv
    .type   __aeabi_idiv, %function
    .global __aeabi_idivmod
    .type   __aeabi_idivmod, %function
    .global __divsi3
    .type   __divsi3, %function
__aeabi_idiv:
__aeabi_idivmod:
__divsi3:
    cmp     r1, #0
    moveq   r1, r0
    moveq   r0, #0
    bxeq    lr
.Lsdiv:
    @ fast path: both non-negative
    orrs    r2, r0, r1
    bpl     .Ludivmod_core
    stmfd   sp!, {r4, lr}
    @ r4 bit 0: negate quotient, bit 1: negate remainder
    mov     r4, #0
    cmp     r0, #0
    rsblt   r0, r0, #0
    movlt   r4, #3
    cmp     r1, #0
    rsblt   r1, r1, #0
    eorlt   r4, r4, #1
    bl      .Ludivmod_core
    tst     r4, #1
    rsbne   r0, r0, #0
    tst     r4, #2
    rsbne   r1, r1, #0
    ldmfd   sp!, {r4, lr}
    bx      lr

@ ---------------------------------------------------------------------------
@ int64_t: r1:r0 = n, r3:r2 = d -> r1:r0 = n / d, r3:r2 = n % d
    .global __aeabi_ldivmod
    .type   __aeabi_ldivmod, %function
__aeabi_ldivmod:
    cmp     r1, r0, asr #31
    cmpeq   r3, r2, asr #31
    bne     .Lldiv_slow
    @ both fit in 32 bits; INT32_MIN / -1 does not
    cmp     r2, #0
    beq     .Lldiv_zero
    cmn     r2, #1
    cmpeq   r0, #0x80000000
    beq     .Lldiv_slow
    stmfd   sp!, {lr}
    mov     r1, r2
    bl      .Lsdiv
    mov     r2, r1
    mov     r3, r1, asr #31
    mov     r1, r0, asr #31
    ldmfd   sp!, {lr}
    bx      lr
.Lldiv_zero:
    cmp     r3, #0
    bne     .Lldiv_slow
    mov     r2, r0                  @ remainder = n, quotient = 0
    mov     r3, r1
    mov     r0, #0
    mov     r1, #0
    bx      lr
.Lldiv_slow:
    @ |n| / |d| through libgcc's __udivmoddi4(n, d, &rem), signs fixed after
    stmfd   sp!, {r4, r5, r6, lr}  @ 16 + 16 bytes: stack stays 8-aligned
    sub     sp, sp, #16             @ [sp] = &rem argument, [sp+8] = rem
    mov     r4, #0
    cmp     r1, #0
    bge     1f
    rsbs    r0, r0, #0
    rsc     r1, r1, #0
    mov     r4, #3
1:  cmp     r3, #0
    bge     2f
    rsbs    r2, r2, #0
    rsc     r3, r3, #0
    eor     r4, r4, #1
2:  add     r5, sp, #8
    str     r5, [sp]
    ldr     r12, =__udivmoddi4
    mov     lr, pc
    bx      r12
    ldr     r2, [sp, #8]
    ldr     r3, [sp, #12]
    tst     r4, #1
    beq     3f
    rsbs    r0, r0, #0
    rsc     r1, r1, #0
3:  tst     r4, #2
    beq     4f
    rsbs    r2, r2, #0
    rsc     r3, r3, #0
4:  add     sp, sp, #16
    ldmfd   sp!, {r4, r5, r6, lr}
    bx      lr
    .ltorg
