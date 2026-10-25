/* show_slug.c - slug cards (owner: ENGINE lane 1; WP2, M0-T3).
 *
 * A slug shot (SHWR_SLUG) is the animatic's placeholder for a beat: a card
 * in the beat's colour with the act and beat number, the beat's title and
 * two lines of description, the shot id, and a running frame counter and
 * timecode, so the 22:00 runtime can be watched (and timed) before a single
 * shot is staged. Drawn on SHOW_L_TEXT, over whatever planes the shot has.
 */
#include <stdio.h>
#include "show_core.h"

static const ShwSlug *s_slug;

static void slug_reset(void) { s_slug = NULL; }

static void slug_prep(const ShowShot *s)
{
    (void)s;
    s_slug = (const ShwSlug *)show_chunk_find(SHWR_SLUG, NULL, NULL);
}

static void tc(char *buf, int n, uint32_t f)
{
    snprintf(buf, (size_t)n, "%02u:%02u:%02u:%02u", (unsigned)(f / 108000u),
             (unsigned)(f / 1800u % 60u), (unsigned)(f / 30u % 60u), (unsigned)(f % 30u));
}

static void slug_draw(int layer, int t)
{
    const ShowShot *sh = show_shot();
    char buf[64], tcb[16];
    int y = SHOW_Y(0), w, bar;
    if (layer != SHOW_L_TEXT || !s_slug) return;

    /* the card: border, act/beat band, title (x2), two lines, progress bar */
    plat_rect(SHOW_SAFE_X, y + SHOW_SAFE_Y, SHOW_CANVAS_W - 2 * SHOW_SAFE_X, 1, s_slug->fg);
    plat_rect(SHOW_SAFE_X, y + SHOW_CANVAS_H - SHOW_SAFE_Y - 1, SHOW_CANVAS_W - 2 * SHOW_SAFE_X, 1,
              s_slug->fg);
    if (s_slug->act)
        snprintf(buf, sizeof buf, "ACT %u  BEAT %u", (unsigned)s_slug->act, (unsigned)s_slug->beat);
    else
        snprintf(buf, sizeof buf, "COLD OPEN  BEAT %u", (unsigned)s_slug->beat);
    plat_text(SHOW_SAFE_X + 4, y + SHOW_SAFE_Y + 6, buf, s_slug->fg);
    snprintf(buf, sizeof buf, "%.8s", sh->ix ? sh->ix->id : "");
    plat_text(SHOW_CANVAS_W - SHOW_SAFE_X - 4 - ui_text_width(buf), y + SHOW_SAFE_Y + 6, buf, s_slug->fg);

    w = ui_text_big_width(show_str(s_slug->title), 2);
    if (w > SHOW_CANVAS_W - 2 * SHOW_SAFE_X - 8)
        plat_text(SHOW_CANVAS_W / 2 - ui_text_width(show_str(s_slug->title)) / 2, y + 62,
                  show_str(s_slug->title), PAL_WHITE);
    else
        ui_text_big(SHOW_CANVAS_W / 2 - w / 2, y + 56, show_str(s_slug->title), 2, PAL_WHITE,
                    PAL_BLACK);
    plat_text(SHOW_CANVAS_W / 2 - ui_text_width(show_str(s_slug->line1)) / 2, y + 90,
              show_str(s_slug->line1), s_slug->fg);
    plat_text(SHOW_CANVAS_W / 2 - ui_text_width(show_str(s_slug->line2)) / 2, y + 102,
              show_str(s_slug->line2), s_slug->fg);

    /* frame counter, episode timecode, and the shot's progress */
    tc(tcb, sizeof tcb, show_episode_frame());
    snprintf(buf, sizeof buf, "+%03d/%03d  %s", t, sh->len, tcb);
    plat_text(SHOW_CANVAS_W / 2 - ui_text_width(buf) / 2, y + 142, buf, PAL_WHITE);
    bar = sh->len > 1 ? (SHOW_CANVAS_W - 2 * SHOW_SAFE_X - 8) * t / (sh->len - 1) : 0;
    plat_rect(SHOW_SAFE_X + 4, y + 156, SHOW_CANVAS_W - 2 * SHOW_SAFE_X - 8, 3, PAL_BLACK);
    plat_rect(SHOW_SAFE_X + 4, y + 156, bar, 3, s_slug->fg);
}

const ShowModule show_mod_slug = {
    "slug", SHOW_API_VERSION, 0, slug_reset, slug_prep, NULL, NULL, slug_draw, NULL, NULL
};
