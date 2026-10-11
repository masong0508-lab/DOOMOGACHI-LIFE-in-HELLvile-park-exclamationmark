/* flash.iwram.c - low-level FLASH chip access (ARM, IWRAM; owner: gba).
 *
 * The cartridge flash sits at 0x0E000000 on an 8-bit bus: only byte
 * accesses. Command sequences and status polling run from IWRAM (never
 * from ROM, which shares the game pak bus). Supported chips:
 *   128 KB: Macronix MX29L010 (C2 09), Sanyo LE26FV10N1TS (62 13)
 *    64 KB: Panasonic MN63F805MNP (32 1B), SST 39VF512 (BF D4),
 *           Macronix MX29L512 (C2 1C), Atmel AT29LV512 (1F 3D, 128-byte
 *           pages, no separate erase)
 * Timeouts are measured in vblanks (the VBlank IRQ keeps running).
 */
#include "gba_plat.h"
#include "flash_int.h"

#define FL ((volatile uint8_t *)0x0E000000)

static inline void fl_cmd(uint8_t c)
{
    FL[0x5555] = 0xAA;
    FL[0x2AAA] = 0x55;
    FL[0x5555] = c;
}

uint16_t fl_read_id(void)
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

void fl_bank(int bank)
{
    fl_cmd(0xB0);
    FL[0] = (uint8_t)bank;
}

/* wait until the byte at off reads `want` (or timeout). 1 = ok */
static int fl_wait(uint32_t off, uint8_t want, uint32_t vbl_timeout)
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

int fl_erase_sector(uint32_t off, int macronix)
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

int fl_program(uint32_t off, const uint8_t *src, uint32_t n, int macronix)
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
int fl_program_atmel_page(uint32_t off, const uint8_t *src)
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

void fl_read(uint32_t off, uint8_t *dst, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) dst[i] = FL[off + i];
}
