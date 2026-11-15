/* vmucard.c - the card shown while the VMU is read or written (DC10A; owner: dreamcast).
 *
 * Before DC10A the card was two flat stripes and outlined text, a style no other screen had. Now it is
 * the core's cute window on the same pink ground: the title, a still bar (the fill is the only thing that
 * moves), and one plain line under it. Portable: no KallistiOS, so the host test can draw it. */
#include "vmucard.h"
#include "doomo/platform.h"
#include "doomo/palette.h"
#include "doomo/ui.h"

static void centred(int y, const char *s, uint8_t col)
{
    int x = DC_CARD_X + (DC_CARD_W - ui_text_width(s)) / 2;
    if (x < DC_CARD_X + 4) x = DC_CARD_X + 4;
    ui_text_clip(x, y, s, col, DC_CARD_X + 4, DC_CARD_X + DC_CARD_W - 4);
}

void dc_card_paint(const char *title, const char *line, int done, int total)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    plat_clear(PAL_UI_PINK);
    ui_glint_hold(1);                    /* calm: the window's sliding glint would restart on every picture */
    ui_window(DC_CARD_X, DC_CARD_Y, DC_CARD_W, DC_CARD_H, WIN_CUTE);
    ui_glint_hold(0);
    if (title) centred(DC_CARD_Y + 14, title, st->text);
    if (total > 0) ui_bar_still(DC_CARD_BAR_X, DC_CARD_BAR_Y, DC_CARD_BAR_W, DC_CARD_BAR_H,
                                done < total ? done : total, total, st->accent);
    if (line) centred(DC_CARD_Y + 58, line, st->text_dim);
}
