/* flash_ram.c - low-level FLASH chip access (ARM; owner: gba).
 *
 * The cartridge flash sits at 0x0E000000 on an 8-bit bus: only byte
 * accesses. Command sequences and status polling must not run from ROM,
 * which shares the game pak bus, so these routines run from RAM - but not
 * from IWRAM for good (G0, task #78: they were 1,092 B the main stack
 * needed). They are ARM code linked into ONE ROM section, .text.flashram,
 * between __flash_ram_start and __flash_ram_end (gba.ld), and
 * save_flash.c copies that block onto the stack for the duration of each
 * save / load call and calls the copy (FL(), flash_int.h): the block is
 * position independent (calls between its functions are relative BLs, the
 * only absolute addresses - the flash, gba_vblanks, REG_IME - come from
 * its literal pools), and nothing outside it may be called from here.
 *
 * Supported chips:
 *   128 KB: Macronix MX29L010 (C2 09), Sanyo LE26FV10N1TS (62 13)
 *    64 KB: Panasonic MN63F805MNP (32 1B), SST 39VF512 (BF D4),
 *           Macronix MX29L512 (C2 1C), Atmel AT29LV512 (1F 3D, 128-byte
 *           pages, no separate erase)
 * Timeouts are measured in vblanks (the VBlank IRQ keeps running).
 */
#include "gba_plat.h"
#include "flash_int.h"

#define FL ((volatile uint8_t *)0x0E000000)

/* every function of the block: ARM, in the copied section, never inlined
 * into code outside it */
#define FLASH_RAM __attribute__((section(".text.flashram"), target("arm"), noinline))

#define fl_cmd(c) do { FL[0x5555] = 0xAA; FL[0x2AAA] = 0x55; FL[0x5555] = (c); } while (0)

FLASH_RAM uint16_t fl_read_id(void)
{
    uint16_t id;
    volatile int i;
    fl_cmd(0x90);
    for (i = 0; i < 2000; i++) { }            /* ID mode settle (~20 us+) */
    id = (uint16_t)(FL[0] | (FL[1] << 8));
    fl_cmd(0xF0);                             /* exit ID mode */
    for (i = 0; i < 2000; i++) { }
    return id;
}

FLASH_RAM void fl_bank(int bank)
{
    fl_cmd(0xB0);
    FL[0] = (uint8_t)bank;
}

/* wait until the byte at off reads `want` (or timeout). 1 = ok. Inlined
 * into its callers (all in the block): a call per programmed byte cost
 * 0.75 % of a save. */
__attribute__((section(".text.flashram"), target("arm"), always_inline))
static inline int fl_wait(uint32_t off, uint8_t want, uint32_t vbl_timeout)
{
    uint32_t start = gba_vblanks;
    uint32_t spins = 0;
    for (;;) {
        if (FL[off] == want) return 1;
        if (++spins > 64 && gba_vblanks - start > vbl_timeout) break;
        if (spins > 4000000u) break;           /* IRQs off: hard cap */
    }
    return FL[off] == want;
}

FLASH_RAM int fl_erase_sector(uint32_t off, int macronix)
{
    int ok;
    fl_cmd(0x80);
    FL[0x5555] = 0xAA;
    FL[0x2AAA] = 0x55;
    FL[off] = 0x30;
    ok = fl_wait(off, 0xFF, 30);              /* 4 KB erase: <= ~0.5 s */
    if (!ok && macronix) FL[off] = 0xF0;      /* Macronix: reset after timeout */
    return ok;
}

FLASH_RAM int fl_program(uint32_t off, const uint8_t *src, uint32_t n, int macronix)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint8_t b = src[i];
        if (b == 0xFF) continue;              /* erased already */
        fl_cmd(0xA0);
        FL[off + i] = b;
        if (!fl_wait(off + i, b, 3)) {
            if (macronix) FL[off + i] = 0xF0;
            return 0;
        }
    }
    /* verify */
    for (i = 0; i < n; i++)
        if (FL[off + i] != src[i]) return 0;
    return 1;
}

/* Atmel: 128-byte page write (erase is implicit). */
FLASH_RAM int fl_program_atmel_page(uint32_t off, const uint8_t *src)
{
    int i;
    uint16_t ime = REG_IME;
    REG_IME = 0;                              /* the 128 bytes must be back to back */
    fl_cmd(0xA0);
    for (i = 0; i < 128; i++) FL[off + i] = src[i];
    REG_IME = ime;
    if (!fl_wait(off + 127, src[127], 3)) return 0;
    for (i = 0; i < 128; i++)
        if (FL[off + i] != src[i]) return 0;
    return 1;
}

FLASH_RAM void fl_read(uint32_t off, uint8_t *dst, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) dst[i] = FL[off + i];
}
