/* et_bench.iwram.c - memory timing micro-benchmarks logged at engine-test
 * boot (owner: gba). Cycles per access as the emulator / hardware sees them. */
#include "gba_plat.h"

void et_bench(void);

static uint32_t b_vram(uint16_t *p, int n)
{
    uint32_t t = gba_cycles();
    uint16_t *e = p + 19200;
    do { *p = (uint16_t)n; p += 120; if (p >= e) p -= 19199; } while (--n);
    return gba_cycles() - t;
}

static uint32_t b_loop(int n)
{
    uint32_t t = gba_cycles();
    uint32_t a = 0;
    do { a += (uint32_t)n; } while (--n);
    return gba_cycles() - t + (a & 0);
}

static uint32_t b_read(const volatile uint8_t *s, int n)
{
    uint32_t t = gba_cycles(), a = 0;
    int i;
    for (i = 0; i < n; i++) a += s[(i * 37) & 4095];
    return gba_cycles() - t + (a & 0);
}

static uint8_t iw[4096];

void et_bench(void)
{
    extern const int32_t gba_sin16[];
    uint32_t loop = b_loop(10000);
    uint32_t vram = b_vram((uint16_t *)0x06000000, 10000);
    uint32_t rom = b_read((const volatile uint8_t *)gba_sin16, 10000);
    uint32_t iwr = b_read(iw, 10000);
    uint32_t ew = b_read((const volatile uint8_t *)0x02010000, 10000);
    gba_logf("BENCH x10000: loop=%u vram_strh_loop=%u rom_ldrb_loop=%u iwram_ldrb_loop=%u ewram_ldrb_loop=%u",
             loop, vram, rom, iwr, ew);
}
