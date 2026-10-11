/* gba_hw.h - Game Boy Advance hardware registers and helpers (owner: gba).
 * No devkitPro / libgba: everything the platform layer touches is here. */
#ifndef GBA_HW_H
#define GBA_HW_H

#include <stdint.h>

typedef volatile uint8_t  vu8;
typedef volatile uint16_t vu16;
typedef volatile uint32_t vu32;
typedef volatile int16_t  vs16;

#define REG8(a)  (*(vu8 *)(a))
#define REG16(a) (*(vu16 *)(a))
#define REG32(a) (*(vu32 *)(a))

/* ---- memory ---------------------------------------------------------- */
#define MEM_EWRAM   0x02000000u
#define MEM_IWRAM   0x03000000u
#define MEM_IO      0x04000000u
#define MEM_PAL     0x05000000u
#define MEM_VRAM    0x06000000u
#define MEM_OAM     0x07000000u
#define MEM_ROM     0x08000000u
#define MEM_SRAM    0x0E000000u

#define BG_PALETTE  ((vu16 *)MEM_PAL)
#define OBJ_PALETTE ((vu16 *)(MEM_PAL + 0x200))
#define VRAM_PAGE0  ((vu16 *)MEM_VRAM)
#define VRAM_PAGE1  ((vu16 *)(MEM_VRAM + 0xA000))

/* ---- display ---------------------------------------------------------- */
#define REG_DISPCNT   REG16(0x04000000)
#define REG_DISPSTAT  REG16(0x04000004)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG2CNT    REG16(0x0400000C)
#define REG_BG2PA     REG16(0x04000020)
#define REG_BG2PB     REG16(0x04000022)
#define REG_BG2PC     REG16(0x04000024)
#define REG_BG2PD     REG16(0x04000026)
#define REG_BG2X      REG32(0x04000028)
#define REG_BG2Y      REG32(0x0400002C)
#define REG_MOSAIC    REG16(0x0400004C)
#define REG_BLDCNT    REG16(0x04000050)
#define REG_BLDALPHA  REG16(0x04000052)
#define REG_BLDY      REG16(0x04000054)

#define DCNT_MODE4    0x0004
#define DCNT_PAGE     0x0010
#define DCNT_OAM_HBL  0x0020
#define DCNT_BLANK    0x0080
#define DCNT_BG2      0x0400
#define DCNT_OBJ      0x1000

#define DSTAT_VBL_IRQ 0x0008
#define DSTAT_HBL_IRQ 0x0010
#define DSTAT_VCT_IRQ 0x0020

/* ---- sound ------------------------------------------------------------- */
#define REG_SND1SWEEP REG16(0x04000060)
#define REG_SND1CNT   REG16(0x04000062)
#define REG_SND1FREQ  REG16(0x04000064)
#define REG_SND2CNT   REG16(0x04000068)
#define REG_SND2FREQ  REG16(0x0400006C)
#define REG_SND3SEL   REG16(0x04000070)
#define REG_SND3CNT   REG16(0x04000072)
#define REG_SND3FREQ  REG16(0x04000074)
#define REG_SND4CNT   REG16(0x04000078)
#define REG_SND4FREQ  REG16(0x0400007C)
#define REG_SNDDMGCNT REG16(0x04000080)
#define REG_SNDDSCNT  REG16(0x04000082)
#define REG_SNDSTAT   REG16(0x04000084)
#define REG_SNDBIAS   REG16(0x04000088)
#define REG_WAVE_RAM  ((vu32 *)0x04000090)
#define REG_FIFO_A    REG32(0x040000A0)
#define REG_FIFO_B    REG32(0x040000A4)

/* ---- DMA ----------------------------------------------------------------- */
#define REG_DMA0SAD   REG32(0x040000B0)
#define REG_DMA0DAD   REG32(0x040000B4)
#define REG_DMA0CNT   REG32(0x040000B8)
#define REG_DMA1SAD   REG32(0x040000BC)
#define REG_DMA1DAD   REG32(0x040000C0)
#define REG_DMA1CNT   REG32(0x040000C4)
#define REG_DMA1CNT_H REG16(0x040000C6)
#define REG_DMA2SAD   REG32(0x040000C8)
#define REG_DMA2DAD   REG32(0x040000CC)
#define REG_DMA2CNT   REG32(0x040000D0)
#define REG_DMA3SAD   REG32(0x040000D4)
#define REG_DMA3DAD   REG32(0x040000D8)
#define REG_DMA3CNT   REG32(0x040000DC)

#define DMA_DST_INC    (0u << 21)
#define DMA_DST_DEC    (1u << 21)
#define DMA_DST_FIXED  (2u << 21)
#define DMA_SRC_INC    (0u << 23)
#define DMA_SRC_DEC    (1u << 23)
#define DMA_SRC_FIXED  (2u << 23)
#define DMA_REPEAT     (1u << 25)
#define DMA_16         (0u << 26)
#define DMA_32         (1u << 26)
#define DMA_AT_NOW     (0u << 28)
#define DMA_AT_VBLANK  (1u << 28)
#define DMA_AT_HBLANK  (2u << 28)
#define DMA_AT_FIFO    (3u << 28)
#define DMA_IRQ        (1u << 30)
#define DMA_ENABLE     (1u << 31)

/* ---- timers ------------------------------------------------------------- */
#define REG_TM0D      REG16(0x04000100)
#define REG_TM0CNT    REG16(0x04000102)
#define REG_TM1D      REG16(0x04000104)
#define REG_TM1CNT    REG16(0x04000106)
#define REG_TM2D      REG16(0x04000108)
#define REG_TM2CNT    REG16(0x0400010A)
#define REG_TM3D      REG16(0x0400010C)
#define REG_TM3CNT    REG16(0x0400010E)
#define TM_FREQ_1     0x0000
#define TM_FREQ_64    0x0001
#define TM_FREQ_256   0x0002
#define TM_FREQ_1024  0x0003
#define TM_CASCADE    0x0004
#define TM_IRQ        0x0040
#define TM_ENABLE     0x0080

/* ---- keys ----------------------------------------------------------------- */
#define REG_KEYINPUT  REG16(0x04000130)
#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define KEY_R      0x0100
#define KEY_L      0x0200
#define KEY_MASK   0x03FF

/* ---- interrupts / system --------------------------------------------------- */
#define REG_IE        REG16(0x04000200)
#define REG_IF        REG16(0x04000202)
#define REG_WAITCNT   REG16(0x04000204)
#define REG_IME       REG16(0x04000208)
#define BIOS_IF       REG16(0x03007FF8)   /* IntrWait acknowledge mirror */
#define IRQ_VECTOR    REG32(0x03007FFC)

#define IRQ_VBLANK  0x0001
#define IRQ_HBLANK  0x0002
#define IRQ_VCOUNT  0x0004
#define IRQ_TIMER0  0x0008
#define IRQ_TIMER1  0x0010
#define IRQ_TIMER2  0x0020
#define IRQ_TIMER3  0x0040
#define IRQ_DMA1    0x0200
#define IRQ_KEYPAD  0x1000

/* mGBA debug log (ignored by real hardware). */
#define REG_MGBA_ENABLE REG16(0x04FFF780)
#define REG_MGBA_FLAGS  REG16(0x04FFF700)
#define MGBA_STRING     ((char *)0x04FFF600)

/* ---- code placement --------------------------------------------------------- */
/* Hot code: ARM state, copied to IWRAM (32-bit, zero wait) at boot.
 * long_call because IWRAM is out of BL range from ROM. */
#define IWRAM_CODE __attribute__((section(".iwram"), target("arm"), long_call, noinline))
/* Prototype attribute for functions that live in IWRAM (or ROM) and are
 * called from the other region: out of BL range, so call through a register. */
#define IWRAM_CALL __attribute__((long_call))
#define ROM_CALL   __attribute__((long_call))
/* Cold code inside a *.iwram.c file (init routines run once at boot):
 * stays in ROM (gba.ld places .text.rom.* in ROM before the .iwram rule),
 * so it does not take IWRAM from the main stack. Callers still use
 * IWRAM_CALL / long_call prototypes, which reach ROM just as well. */
#define ROM_COLD   __attribute__((section(".text.rom"), noinline))
#define IWRAM_DATA __attribute__((section(".iwram_data")))
#define IWRAM_BSS  __attribute__((section(".iwram_bss")))
#define EWRAM_BSS  __attribute__((section(".ewram_bss")))
#define NOINIT     __attribute__((section(".noinit")))
#define ALIGN4     __attribute__((aligned(4)))

static inline void dma3_copy32(void *dst, const void *src, uint32_t words) {
    REG_DMA3SAD = (uint32_t)src;
    REG_DMA3DAD = (uint32_t)dst;
    REG_DMA3CNT = words | DMA_32 | DMA_ENABLE;
}
static inline void dma3_copy16(void *dst, const void *src, uint32_t halves) {
    REG_DMA3SAD = (uint32_t)src;
    REG_DMA3DAD = (uint32_t)dst;
    REG_DMA3CNT = halves | DMA_16 | DMA_ENABLE;
}
/* Fill with a 32-bit value (the value must live in memory for DMA). */
static inline void dma3_fill32(void *dst, const volatile uint32_t *val, uint32_t words) {
    REG_DMA3SAD = (uint32_t)val;
    REG_DMA3DAD = (uint32_t)dst;
    REG_DMA3CNT = words | DMA_32 | DMA_SRC_FIXED | DMA_ENABLE;
}

#if defined(__thumb__)
#define GBA_SWI(n) __asm__ volatile("swi " #n ::: "r0", "r1", "r2", "r3", "memory")
#else
#define GBA_SWI(n) __asm__ volatile("swi " #n " << 16" ::: "r0", "r1", "r2", "r3", "memory")
#endif
static inline void vblank_intr_wait(void) { GBA_SWI(0x05); }

#endif
