/* vmucard.h - the card shown while the VMU is read or written (DC10A; owner: dreamcast).
 *
 * dc_card_paint() draws one picture of it with the core's own window, bar and text (the same look as
 * the menus; no glint, nothing that moves but the bar). It is plain 2D calls, so the host test draws
 * it too (dc/tests/gfx2d). vmusave.c wraps it in dc2d_side_begin / end. */
#ifndef DC_VMUCARD_H
#define DC_VMUCARD_H

/* title: what is happening ("Saving your game..."); line: the one plain instruction or reason, or NULL;
 * done / total: the bar (total <= 0: no bar). */
void dc_card_paint(const char *title, const char *line, int done, int total);

/* the card's window, for the test */
#define DC_CARD_X 8
#define DC_CARD_Y 72
#define DC_CARD_W 304
#define DC_CARD_H 88
#define DC_CARD_BAR_X 52
#define DC_CARD_BAR_Y 108
#define DC_CARD_BAR_W 216
#define DC_CARD_BAR_H 8

#endif
