@ crt0.s - Game Boy Advance boot code for DOOMOGACHI LIFE (owner: gba).
@
@ No devkitPro: this is the whole runtime start-up.
@   * the cartridge header (branch, Nintendo logo, title, codes; the
@     complement checksum is fixed by tools/gbafix.py after linking)
@   * stacks: IRQ 0x03007FA0 (BIOS default), user/system 0x03007D00
@     (gives the IRQ handler 672 bytes; see gba.ld __iwram_stack_limit)
@   * copies .iwram (ARM hot code + IWRAM data) and .data (EWRAM) from ROM,
@     clears .bss (EWRAM) and .iwram_bss
@   * installs the master IRQ handler (ARM, IWRAM) that acknowledges the
@     interrupt for VBlankIntrWait and dispatches to C handlers
@   * jumps to main() (THUMB, ROM)
@ The "FLASH1M_V103" tag lets emulators autodetect the 128 KB flash save.

    .section .crt0, "ax"
    .arm
    .align 2
    .global _start
_start:
    b       rom_start

    @ ---- 0x04: Nintendo logo (156 bytes, checked by the BIOS) ----------
    .byte 0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,0x3D,0x84,0x82,0x0A
    .byte 0x84,0xE4,0x09,0xAD,0x11,0x24,0x8B,0x98,0xC0,0x81,0x7F,0x21
    .byte 0xA3,0x52,0xBE,0x19,0x93,0x09,0xCE,0x20,0x10,0x46,0x4A,0x4A
    .byte 0xF8,0x27,0x31,0xEC,0x58,0xC7,0xE8,0x33,0x82,0xE3,0xCE,0xBF
    .byte 0x85,0xF4,0xDF,0x94,0xCE,0x4B,0x09,0xC1,0x94,0x56,0x8A,0xC0
    .byte 0x13,0x72,0xA7,0xFC,0x9F,0x84,0x4D,0x73,0xA3,0xCA,0x9A,0x61
    .byte 0x58,0x97,0xA3,0x27,0xFC,0x03,0x98,0x76,0x23,0x1D,0xC7,0x61
    .byte 0x03,0x04,0xAE,0x56,0xBF,0x38,0x84,0x00,0x40,0xA7,0x0E,0xFD
    .byte 0xFF,0x52,0xFE,0x03,0x6F,0x95,0x30,0xF1,0x97,0xFB,0xC0,0x85
    .byte 0x60,0xD6,0x80,0x25,0xA9,0x63,0xBE,0x03,0x01,0x4E,0x38,0xE2
    .byte 0xF9,0xA2,0x34,0xFF,0xBB,0x3E,0x03,0x44,0x78,0x00,0x90,0xCB
    .byte 0x88,0x11,0x3A,0x94,0x65,0xC0,0x7C,0x63,0x87,0xF0,0x3C,0xAF
    .byte 0xD6,0x25,0xE4,0x8B,0x38,0x0A,0xAC,0x72,0x21,0xD4,0xF8,0x07

    @ ---- 0xA0: title (12), game code (4), maker code (2) ---------------
    .ascii "DOOMOGACHI"
    .byte 0, 0
    .ascii "BDME"
    .ascii "DM"
    .byte 0x96                  @ fixed value
    .byte 0x00                  @ main unit code
    .byte 0x00                  @ device type
    .byte 0,0,0,0,0,0,0         @ reserved
    .byte 0x00                  @ software version
    .byte 0x00                  @ complement check (gbafix.py)
    .byte 0, 0                  @ reserved

    @ ---- 0xC0: code ----------------------------------------------------
rom_start:
    @ IRQ mode stack
    mov     r0, #0x12
    msr     cpsr_c, r0
    ldr     sp, =0x03007FA0
    @ system mode stack (user code runs in system mode: same regs as user,
    @ but allowed to change mode for the IRQ handler if ever needed)
    mov     r0, #0x1F
    msr     cpsr_c, r0
    ldr     sp, =__sp_usr

    @ interrupts off while we set up
    mov     r0, #0x04000000
    mov     r1, #0
    str     r1, [r0, #0x208]            @ IME = 0

    @ ROM wait states: WS0 3/1, prefetch on, SRAM 8 cycles (flash needs it)
    ldr     r1, =0x4317
    add     r2, r0, #0x200
    strh    r1, [r2, #4]

    @ copy .iwram (code + data) ROM -> IWRAM
    ldr     r0, =__iwram_lma
    ldr     r1, =__iwram_start
    ldr     r2, =__iwram_end
    bl      copy_words
    @ copy .data ROM -> EWRAM
    ldr     r0, =__data_lma
    ldr     r1, =__data_start
    ldr     r2, =__data_end
    bl      copy_words
    @ clear .iwram_bss
    ldr     r0, =__iwram_bss_start
    ldr     r1, =__iwram_bss_end
    bl      clear_words
    @ clear .bss (EWRAM)
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    bl      clear_words
    @ paint the free IWRAM below the main stack (high-water mark: main.c)
    ldr     r0, =__iwram_bss_end
    sub     r1, sp, #64
    ldr     r2, =0x5AC35AC3
4:  cmp     r0, r1
    strlo   r2, [r0], #4
    blo     4b

    @ master IRQ handler
    ldr     r0, =0x03007FFC
    ldr     r1, =gba_irq_master
    str     r1, [r0]

    @ main() is THUMB code in ROM
    ldr     r0, =main
    mov     lr, pc
    bx      r0
    @ main returned: halt forever
1:  b       1b

@ r0 = src, r1 = dst, r2 = dst end (all word aligned by the linker script)
copy_words:
    cmp     r1, r2
    bxhs    lr
2:  ldr     r3, [r0], #4
    str     r3, [r1], #4
    cmp     r1, r2
    blo     2b
    bx      lr

@ r0 = start, r1 = end
clear_words:
    mov     r2, #0
    cmp     r0, r1
    bxhs    lr
3:  str     r2, [r0], #4
    cmp     r0, r1
    blo     3b
    bx      lr

    .pool

    @ Save type tag for emulators (must be word aligned, anywhere in ROM).
    .align 2
    .ascii "FLASH1M_V103"
    .byte 0, 0, 0, 0
    .align 2

@ ---------------------------------------------------------------------
@ Master IRQ handler. The BIOS calls it in IRQ mode (ARM) after pushing
@ r0-r3, r12, lr on the IRQ stack. We acknowledge every pending enabled
@ interrupt (REG_IF and the BIOS mirror at 0x03007FF8 used by
@ VBlankIntrWait), then call gba_irq_dispatch(flags) - C, ARM, IWRAM -
@ with IRQs still disabled (no nesting needed).
@ ---------------------------------------------------------------------
    .section .iwram, "ax", %progbits
    .arm
    .align 2
    .global gba_irq_master
gba_irq_master:
    mov     r3, #0x04000000
    add     r3, r3, #0x200
    ldr     r2, [r3]                    @ r2 = IE | (IF << 16)
    and     r0, r2, r2, lsr #16         @ r0 = IE & IF
    strh    r0, [r3, #2]                @ acknowledge in REG_IF
    ldr     r1, =0x03007FF8
    ldrh    r2, [r1]
    orr     r2, r2, r0
    strh    r2, [r1]                    @ BIOS IF mirror
    stmfd   sp!, {r0, lr}               @ 2 words: keeps sp 8-byte aligned
    ldr     r1, =gba_irq_dispatch
    mov     lr, pc
    bx      r1
    ldmfd   sp!, {r0, lr}
    bx      lr
    .pool
