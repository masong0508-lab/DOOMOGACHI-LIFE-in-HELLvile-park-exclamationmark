@ mem.s - fast memcpy / memset in ARM code, running from IWRAM (owner: gba).
@
@ These replace newlib's THUMB/ROM versions (our objects are linked
@ before libc.a, so libc's members are never pulled in). The core clears
@ and copies big structures (doomo_town, doomo_map) with them.
@ Word-aligned blocks move 32 bytes per ldm/stm pair. Calls from THUMB
@ ROM code reach IWRAM through linker-generated long-branch veneers.
@ (Never use these on VRAM with odd sizes / addresses: VRAM ignores byte
@ writes. The platform uses DMA for VRAM.)

    .section .iwram, "ax", %progbits
    .arm
    .align 2

@ void *memcpy(void *dst, const void *src, size_t n)
    .global memcpy
    .type   memcpy, %function
memcpy:
    mov     r12, r0                 @ r12 = running dst, r0 = return value
    cmp     r2, #16
    blo     .Lcpy_bytes
    eor     r3, r12, r1
    tst     r3, #3
    bne     .Lcpy_half_check
    @ co-aligned: copy bytes until dst is word aligned
.Lcpy_align:
    tst     r12, #3
    beq     .Lcpy_words
    ldrb    r3, [r1], #1
    strb    r3, [r12], #1
    sub     r2, r2, #1
    b       .Lcpy_align
.Lcpy_words:
    cmp     r2, #32
    blo     .Lcpy_word_tail
    stmfd   sp!, {r4-r10}
.Lcpy_block:
    ldmia   r1!, {r3-r10}
    stmia   r12!, {r3-r10}
    sub     r2, r2, #32
    cmp     r2, #32
    bhs     .Lcpy_block
    ldmfd   sp!, {r4-r10}
.Lcpy_word_tail:
    cmp     r2, #4
    blo     .Lcpy_bytes
    ldr     r3, [r1], #4
    str     r3, [r12], #4
    sub     r2, r2, #4
    b       .Lcpy_word_tail
.Lcpy_half_check:
    tst     r3, #1
    bne     .Lcpy_bytes
    @ co-aligned on halfwords
    tst     r12, #1
    ldrneb  r3, [r1], #1
    strneb  r3, [r12], #1
    subne   r2, r2, #1
.Lcpy_halves:
    cmp     r2, #2
    blo     .Lcpy_bytes
    ldrh    r3, [r1], #2
    strh    r3, [r12], #2
    sub     r2, r2, #2
    b       .Lcpy_halves
.Lcpy_bytes:
    cmp     r2, #0
    bxeq    lr
.Lcpy_byte_loop:
    ldrb    r3, [r1], #1
    strb    r3, [r12], #1
    subs    r2, r2, #1
    bne     .Lcpy_byte_loop
    bx      lr
    .size   memcpy, . - memcpy

@ void *memset(void *dst, int c, size_t n)
    .global memset
    .type   memset, %function
memset:
    mov     r12, r0
    and     r1, r1, #0xFF
    orr     r1, r1, r1, lsl #8
    orr     r1, r1, r1, lsl #16
    cmp     r2, #8
    blo     .Lset_bytes
.Lset_align:
    tst     r12, #3
    beq     .Lset_words
    strb    r1, [r12], #1
    sub     r2, r2, #1
    b       .Lset_align
.Lset_words:
    cmp     r2, #32
    blo     .Lset_word_tail
    stmfd   sp!, {r4-r9}
    mov     r3, r1
    mov     r4, r1
    mov     r5, r1
    mov     r6, r1
    mov     r7, r1
    mov     r8, r1
    mov     r9, r1
.Lset_block:
    stmia   r12!, {r1, r3-r9}
    sub     r2, r2, #32
    cmp     r2, #32
    bhs     .Lset_block
    ldmfd   sp!, {r4-r9}
.Lset_word_tail:
    cmp     r2, #4
    blo     .Lset_bytes
    str     r1, [r12], #4
    sub     r2, r2, #4
    b       .Lset_word_tail
.Lset_bytes:
    cmp     r2, #0
    bxeq    lr
.Lset_byte_loop:
    strb    r1, [r12], #1
    subs    r2, r2, #1
    bne     .Lset_byte_loop
    bx      lr
    .size   memset, . - memset
