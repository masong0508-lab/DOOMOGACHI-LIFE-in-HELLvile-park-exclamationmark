/* show_selftest.c - AT1.3: the show's portrait images are the game's
 * (owner: show; built only with -DSHOW_SELFTEST=ON, never in a master).
 *
 * For 20 roster looks x the 11 expressions it prints the CRC32 of every
 * bust (portrait_image) and figure (figure_image), plus the 4 chibi
 * directions (bodyspr_image), on the PS1's TTY, one look per frame. The
 * same file is built twice by show/tests/engine/test_wp1.py: once with the
 * show's patched portrait copies (-DPC_BLOCKS=240, overrides at -1) and
 * once with the game's unpatched files (-DSHOW_PORTRAIT_ORIGINAL=ON); the
 * two logs must be identical.
 *
 * The patched build then also checks the override channels: a mouth or a
 * blink override must change the image, and going back to -1 must give
 * the original CRC again (the key fold of hunk (b): no stale cache entry).
 */
#include <stdio.h>
#include "doomo/doomo.h"
#include "show/show_game.h"
#ifndef SHOW_PORTRAIT_ORIGINAL
#include "show/show_portrait.h"
#endif

#define NLOOKS   20
#define CLOCK_T  1000u      /* one fixed portrait clock for every fetch */

static uint32_t s_tab[256];
static uint32_t s_all;      /* running CRC over every printed CRC */
static int      s_n;
static int      s_done;

static uint32_t crc32_of(const uint8_t *p, unsigned n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) c = s_tab[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return ~c;
}

static uint32_t crc_img(const DoomoImage *im)
{
    return im && im->pix ? crc32_of(im->pix, (unsigned)im->w * im->h) : 0;
}

static void emit(char kind, int look, int k, uint32_t crc)
{
    uint8_t b[4];
    b[0] = (uint8_t)crc; b[1] = (uint8_t)(crc >> 8); b[2] = (uint8_t)(crc >> 16); b[3] = (uint8_t)(crc >> 24);
    s_all = ~crc32_of(b, 4) ^ (s_all * 31u);
    s_n++;
    printf("selftest: %c %02d %02d %08x\n", kind, look, k, (unsigned)crc);
}

void show_selftest_init(void)
{
    uint32_t i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        s_tab[i] = c;
    }
    for (i = 0; i < NLOOKS; i++) {
        doomo_town.res[i] = *roster_premade((int)i);
        doomo_town.res[i].status = RES_ALIVE;
    }
    portrait_cache_reset();
    printf("selftest: portrait begin looks=%d exprs=%d build=%s\n", NLOOKS, EXPR_COUNT,
#ifdef SHOW_PORTRAIT_ORIGINAL
           "game"
#else
           "show"
#endif
    );
}

#ifndef SHOW_PORTRAIT_ORIGINAL
static void overrides(void)
{
    int i, md = 0, mr = 0, bd = 0, br = 0, cd = 0, cr = 0;
    for (i = 0; i < NLOOKS; i++) {
        uint32_t base, o;
        int m = (i * 7 + 3) % MOUTH_COUNT;
        portrait_cache_clock(CLOCK_T, 6);
        base = crc_img(portrait_image(i, EXPR_NEUTRAL));
        show_portrait_overrides(m, -1);
        o = crc_img(portrait_image(i, EXPR_NEUTRAL));
        show_portrait_overrides(-1, -1);
        md += o != base;
        mr += crc_img(portrait_image(i, EXPR_NEUTRAL)) == base;
        show_portrait_overrides(-1, 0);                 /* forced open ... */
        o = crc_img(portrait_image(i, EXPR_NEUTRAL));
        show_portrait_overrides(-1, 1);                 /* ... vs forced shut */
        bd += o != crc_img(portrait_image(i, EXPR_NEUTRAL));
        show_portrait_overrides(-1, -1);
        br += crc_img(portrait_image(i, EXPR_NEUTRAL)) == base;
        base = crc_img(bodyspr_image(i, 0, 0));
        show_portrait_overrides(m, -1);
        o = crc_img(bodyspr_image(i, 0, 0));
        show_portrait_overrides(-1, -1);
        cd += o != base;
        cr += crc_img(bodyspr_image(i, 0, 0)) == base;
    }
    printf("selftest: override mouth differs %d/%d restored %d/%d; blink differs %d/%d "
           "restored %d/%d; chibi mouth differs %d/%d restored %d/%d\n",
           md, NLOOKS, mr, NLOOKS, bd, NLOOKS, br, NLOOKS, cd, NLOOKS, cr, NLOOKS);
}
#endif

int show_selftest_frame(uint32_t frame)
{
    int i = (int)frame - 1, e, d;
    if (s_done) return 0;
    if (i < NLOOKS) {
        for (e = 0; e < EXPR_COUNT; e++) {
            portrait_cache_clock(CLOCK_T, 6);
            emit('P', i, e, crc_img(portrait_image(i, e)));
            portrait_cache_clock(CLOCK_T, 6);
            emit('F', i, e, crc_img(figure_image(&doomo_town.res[i].look, RES_ALIVE, e)));
        }
        for (d = 0; d < 4; d++) {
            portrait_cache_clock(CLOCK_T, 6);
            emit('C', i, d, crc_img(bodyspr_image(i, d, 0)));
        }
        plat_clear(PAL_UI_PURPLE);
        return 1;
    }
    printf("selftest: portrait n=%d all=%08x\n", s_n, (unsigned)s_all);
#ifndef SHOW_PORTRAIT_ORIGINAL
    overrides();
#endif
    printf("selftest: portrait end\n");
    portrait_cache_reset();
    s_done = 1;
    return 0;
}
