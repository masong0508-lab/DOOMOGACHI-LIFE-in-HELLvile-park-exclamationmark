/* show_strokes.h - the STROKES plane (WP5; spec 8.3, 9.4): live crayon
 * lines with a reveal by arc length and the +-1 px boil on twos. Owner:
 * ENGINE lane 2.
 *
 * A stroke is a polyline (or several, split by pen-ups) in the plane's own
 * pixel grid: at px=2 one stroke pixel is a 2x2 block, exactly like the
 * plate's crayon lines next to it. It is rasterised on the PS1 each frame
 * (Bresenham, w pixels wide, with the dash pattern of a crayon's tooth) and
 * drawn with plat_rect, which the PS1's 2D layer batches into runs. Nothing
 * is a GPU line, so a px=2 stroke has the plate's chunky pixels.
 *
 * Reveal: a stroke with t0 != SK_ALWAYS draws nothing before t0, the first
 * ceil(N * (t - t0 + 1) / (t1 - t0 + 1)) of its N path pixels during
 * [t0, t1], and all of them from t1 on: it reaches 100 % ON frame t1 (AT5.2).
 * Gaps count as path, so a dashed line reveals at an even speed.
 *
 * Payload (one per STROKES plane, 4-byte aligned, little-endian):
 *   ShowStrokeSet, ShowStroke[nstrokes], ShowStrokePt[npts]
 */
#ifndef SHOW_STROKES_H
#define SHOW_STROKES_H

#include <stdint.h>

struct ShowXform;

#define SK_ALWAYS   0xFFFF              /* ShowStroke.t0: no reveal, always whole   */
#define SK_PEN_UP   (-32768)            /* ShowStrokePt.x: the next point starts a new line */
enum { SK_HIDDEN = 1, SK_NOBOIL = 2 };  /* ShowStroke.flags                         */

typedef struct ShowStrokeSet {          /* 12 B                                     */
    uint16_t nstrokes, npts;
    uint32_t id;                        /* the plane's id: the boil seed            */
    uint8_t  boil;                      /* boil period (0 = follow the shot's look) */
    uint8_t  flags, pad[2];
} ShowStrokeSet;

typedef struct ShowStroke {             /* 16 B                                     */
    uint16_t first, npts;               /* points [first, first + npts)             */
    uint16_t t0, t1;                    /* reveal span, shot frames, inclusive      */
    uint8_t  color, color2;             /* nib; a second shade for the crayon's tooth (0 = none) */
    uint8_t  width;                     /* 1..3 plane px                            */
    uint8_t  gap_on, gap_off;           /* dash pattern, plane px (0 = solid)       */
    uint8_t  flags;                     /* SK_*                                     */
    uint16_t pad;
} ShowStroke;

typedef struct ShowStrokePt { int16_t x, y; } ShowStrokePt;   /* plane px          */

/* Bind a STROKES plane's payload (it must stay valid for the shot). Returns
 * a handle (>= 0) or -1. Up to SHOW_STROKE_PLANES planes per shot. */
#define SHOW_STROKE_PLANES 4
void show_strokes_reset(void);
int  show_strokes_bind(const void *payload, uint32_t bytes);
/* draw plane h at shot frame t under its plane transform */
void show_strokes_draw(int h, int t, const struct ShowXform *xf);
/* path pixels of stroke s at frame t: drawn so far, and the stroke's total
 * (the foley follows it: FOL_crayon_scratch follow=<stroke>) */
int  show_strokes_revealed(int h, int s, int t, int *total);
/* SOP: show or hide a stroke from now on */
void show_strokes_show(int h, int s, int on);

#endif
