/* video_int.h - 2D pixel loops shared by video.c and video.iwram.c
 * (owner: gba). Coordinates are already clipped where noted. */
#ifndef GBA_VIDEO_INT_H
#define GBA_VIDEO_INT_H

#include "gba_plat.h"
#include "doomo/font.h"

extern uint8_t gba_font_rows[FONT_LAST - FONT_FIRST + 1][8];   /* bit c = column c */
/* The glyph rows of character ch (FONT_FIRST..FONT_LAST, bit c = column
 * c): every text path (Mode 4 vid_text / vid_text_dil and the G5 canvas)
 * reads glyphs through this. Today it is the core font transposed at boot
 * (gba_font_rows). Hook for L4's per-language glyph pages: when a
 * language brings its own page for 0xB0..0xFF, map ch to that page's rows
 * here, and both display modes follow. */
static inline const uint8_t *vid_glyph_rows(unsigned ch)
{
    return gba_font_rows[ch - FONT_FIRST];
}
/* the outline mask of one glyph for plat_text_outline: 11 rows of a 7-wide
 * box at (-1, -1) from the glyph (video.c) */
void vid_glyph_dilate(const uint8_t *g, uint16_t out[11]) __attribute__((long_call));

/* clipped: x0 < x1 <= 240, y0 < y1 <= 160 (vid_rect_full: x0 = 0, x1 = 240) */
void vid_rect_full(int x0, int y0, int x1, int y1, uint32_t color) IWRAM_CALL;
void vid_rect(int x0, int y0, int x1, int y1, uint32_t color) IWRAM_CALL;
void vid_shade(int x0, int y0, int x1, int y1, const uint8_t *cm) IWRAM_CALL;
/* n >= 1 aligned words of the back page, every byte through lut (asm) */
void vid_shade_words(uint32_t *p, int n, const uint8_t *lut) IWRAM_CALL;
/* on screen */
void vid_pixel(int x, int y, uint32_t color) IWRAM_CALL;
/* unclipped (clips itself) */
void vid_text(int x, int y, const char *s, uint32_t color) IWRAM_CALL;
/* the outline pass of plat_text_outline (video.c, ROM) */
void vid_text_dil(int x, int y, const char *s, uint32_t color);
extern const uint32_t gba_nibmask[16];     /* bit i set = byte i of a word */
void vid_blit(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color) IWRAM_CALL;
/* (SP6) scale 2, no flip, through the IWRAM UI group's loop (blit2.c): only while iwram_ovl_current() ==
 * OVL_UI. sil != 0: every opaque pixel in that colour (IMG_SILHOUETTE). Unclipped (clips itself). */
void vid_blit2(int x, int y, const DoomoImage *img, uint32_t sil);

#endif
