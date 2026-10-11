/* video_int.h - 2D pixel loops shared by video.c and video.iwram.c
 * (owner: gba). Coordinates are already clipped where noted. */
#ifndef GBA_VIDEO_INT_H
#define GBA_VIDEO_INT_H

#include "gba_plat.h"
#include "doomo/font.h"

extern uint8_t gba_font_rows[FONT_LAST - FONT_FIRST + 1][8];   /* bit c = column c */

/* clipped: x0 < x1 <= 240, y0 < y1 <= 160 */
void vid_rect(int x0, int y0, int x1, int y1, uint32_t color) IWRAM_CALL;
void vid_shade(int x0, int y0, int x1, int y1, const uint8_t *cm) IWRAM_CALL;
/* on screen */
void vid_pixel(int x, int y, uint32_t color) IWRAM_CALL;
/* unclipped (clips itself) */
void vid_text(int x, int y, const char *s, uint32_t color) IWRAM_CALL;
void vid_blit(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color) IWRAM_CALL;

#endif
