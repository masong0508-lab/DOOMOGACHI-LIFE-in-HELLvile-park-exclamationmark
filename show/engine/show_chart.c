/* show_chart.c - the boot test chart (owner: show; spec 2.1, M0-T6, AT2.1).
 *
 * What show.exe draws when no episode is loaded. Everything on it is the
 * game's own drawing: palette bands, plat_rect / plat_text, and a bust
 * (px=2), a figure (px=1) and a chibi (px=2) from the portrait engine,
 * cycling the expressions so the blink and the cache are visibly live.
 * It marks the 320x180 picture (screen lines 30-209), the 90 % title-safe
 * box, the Wall line at x = 160, a 0-255 palette grid and an 8 px ruler,
 * and draws the black matte bars last. Fully deterministic: fixed look
 * seeds, the frame counter is the only clock.
 */
#include <stdio.h>
#include "doomo/doomo.h"
#include "show/show_game.h"

#define Y0      SHOW_CANVAS_Y                   /* picture top on screen   */
#define Y1      (SHOW_CANVAS_Y + SHOW_CANVAS_H) /* one past its bottom     */
#define CHART_RID 0                             /* doomo_town.res[0]       */
#define EXPR_HOLD 45                            /* frames per expression   */
#define FLOOR_Y   172                           /* where the cast stands   */

static const char *const k_expr[EXPR_COUNT] = {
    "neutral", "happy", "sad", "angry", "love", "shock",
    "sleepy", "sick", "possessed", "dead", "talk"
};

static DoomoResident s_fig;     /* the figure's look (figure_image takes it) */

void show_chart_init(void)
{
    /* two fixed looks: the bust / chibi live in town slot 0 (town_res() is
     * how the portrait engine finds residents), the figure is passed in */
    DoomoResident *r = &doomo_town.res[CHART_RID];
    resident_default(r);
    resident_randomize_look(r, 0x5101u);
    r->status = RES_ALIVE;
    resident_default(&s_fig);
    resident_randomize_look(&s_fig, 0x5102u);
    s_fig.status = RES_ALIVE;
    portrait_cache_reset();
}

static void frame_rect(int x, int y, int w, int h, uint8_t c)
{
    plat_rect(x, y, w, 1, c);
    plat_rect(x, y + h - 1, w, 1, c);
    plat_rect(x, y, 1, h, c);
    plat_rect(x + w - 1, y, 1, h, c);
}

static void corner(int x, int y, int dx, int dy, uint8_t c)
{
    plat_rect(dx > 0 ? x : x - 7, y, 8, 1, c);
    plat_rect(x, dy > 0 ? y : y - 7, 1, 8, c);
}

static void text_c(int y, const char *s, uint8_t c)
{
    plat_text(SHOW_CANVAS_W / 2 - ui_text_width(s) / 2, y, s, c);
}

void show_chart_draw(uint32_t frame)
{
    const DoomoImage *im;
    char buf[48];
    int i, expr = (int)((frame / EXPR_HOLD) % EXPR_COUNT);

    plat_clear(PAL_BLACK);
    /* the picture: an 8-step palette band sky (never Gouraud, spec 2.3) */
    for (i = 0; i < 8; i++) {
        int y = Y0 + i * SHOW_CANVAS_H / 8, y2 = Y0 + (i + 1) * SHOW_CANVAS_H / 8;
        plat_rect(0, y, SHOW_CANVAS_W, y2 - y, PAL_RAMP(RAMP_VAPE, 7 - i));
    }
    /* the floor the cast stands on */
    plat_rect(0, FLOOR_Y, SHOW_CANVAS_W, Y1 - FLOOR_Y, PAL_RAMP(RAMP_PASTEL_PINK, 2));
    plat_rect(0, FLOOR_Y, SHOW_CANVAS_W, 1, PAL_RAMP(RAMP_PASTEL_PINK, 5));

    /* title-safe box (90 %) and the Wall line */
    frame_rect(SHOW_SAFE_X, Y0 + SHOW_SAFE_Y, SHOW_CANVAS_W - 2 * SHOW_SAFE_X,
               SHOW_CANVAS_H - 2 * SHOW_SAFE_Y, PAL_UI_GOLD);
    for (i = Y0; i < Y1; i += 8)
        plat_rect(SHOW_WALL_X, i, 1, 4, PAL_UI_RED);

    /* the title card band */
    plat_rect(SHOW_SAFE_X + 1, Y0 + SHOW_SAFE_Y + 1, SHOW_CANVAS_W - 2 * SHOW_SAFE_X - 2, 24,
              PAL_RAMP(RAMP_VAPE, 0));
    text_c(Y0 + 12, "DOOMOGACHI LIFE in HELLvile Park!", PAL_WHITE);
    text_c(Y0 + 23, "show.exe test chart  320x180 16:9  30 fps", PAL_UI_GOLD);

    /* the palette, index 0-255, 16 x 16 cells of 5 x 5 */
    frame_rect(23, 69, 82, 82, PAL_WHITE);
    for (i = 0; i < 256; i++)
        plat_rect(24 + (i & 15) * 5, 70 + (i >> 4) * 5, 5, 5, (uint8_t)i);
    plat_text(24, 155, "palette 0-255", PAL_RAMP(RAMP_VAPE, 0));

    /* the portrait engine: figure px=1, bust px=2, chibi px=2 */
    im = figure_image(&s_fig.look, RES_ALIVE, expr);
    if (im) plat_image(108, FLOOR_Y - im->h, im, 1, 0, 0);
    im = portrait_image(CHART_RID, expr);
    if (im) plat_image(172, FLOOR_Y - 2 * im->h, im, 2, 0, 0);
    im = bodyspr_image(CHART_RID, (int)((frame / 90) & 3), (int)((frame >> 3) & 3));
    if (im) plat_image(272, FLOOR_Y - 2 * im->h, im, 2, 0, 0);

    /* labels and the frame counter (bottom band, inside title-safe) */
    snprintf(buf, sizeof buf, "expr %s", k_expr[expr]);
    plat_text(SHOW_SAFE_X + 4, 177, buf, PAL_RAMP(RAMP_VAPE, 0));
    snprintf(buf, sizeof buf, "f %05u", (unsigned)frame);
    plat_text(SHOW_CANVAS_W - SHOW_SAFE_X - 4 - ui_text_width(buf), 177, buf,
              PAL_RAMP(RAMP_VAPE, 0));
    /* an 8 px ruler, every 32 px long */
    for (i = SHOW_SAFE_X; i <= SHOW_CANVAS_W - SHOW_SAFE_X; i += 8)
        plat_rect(i, Y1 - SHOW_SAFE_Y - ((i - SHOW_SAFE_X) % 32 ? 3 : 6), 1,
                  (i - SHOW_SAFE_X) % 32 ? 3 : 6, PAL_WHITE);

    /* the picture's corners, then the matte bars last (spec 2.1) */
    corner(0, Y0, 1, 1, PAL_WHITE);
    corner(SHOW_CANVAS_W - 1, Y0, -1, 1, PAL_WHITE);
    corner(0, Y1 - 1, 1, -1, PAL_WHITE);
    corner(SHOW_CANVAS_W - 1, Y1 - 1, -1, -1, PAL_WHITE);
    plat_rect(0, 0, SHOW_CANVAS_W, Y0, PAL_BLACK);
    plat_rect(0, Y1, SHOW_CANVAS_W, 240 - Y1, PAL_BLACK);

    if (frame == 1 || frame % 300 == 0)
        printf("show: chart frame %u\n", (unsigned)frame);
}
