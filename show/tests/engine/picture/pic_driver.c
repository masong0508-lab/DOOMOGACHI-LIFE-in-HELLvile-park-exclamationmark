/* pic_driver.c - the lane-2 test driver (ENGINE lane 2): runs one "PIC1" shot
 * blob (showtk/picture/blob.py) through the lane-2 modules exactly as the
 * player's shot loop will (spec 8.1), and prints one P-log line per frame.
 * Built twice: on the host (pic_host.c, a software raster, for exact-number
 * tests in seconds) and on the PS1 (ps1/, show.exe's own sources with this
 * driver in place of show_game.c, for the contact sheets).
 */
#include <stdio.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "show/show_l2.h"
#include "show/show_cam.h"
#include "show/show_fx.h"
#include "show/show_strokes.h"
#include "show/show_text.h"

static int s_frames;

/* the test's prop/actor slots come from the blob */
int show_stage_slot_xy(int slot, int *x, int *y) { return show_l2_slot(slot, x, y); }

int pic_init(const void *blob, unsigned len)
{
    camfx_reset();
    s_frames = show_l2_bind(blob, len);
    return s_frames;
}

/* 16 bits of t in screen rows 0-3 (the open matte above the picture), so a
 * screenshot names its own frame: white = 1, black = 0, bit 15 first */
static void stamp(int t)
{
    int b;
    for (b = 0; b < 16; b++)
        plat_rect(b * 4, 0, 4, 4, ((t >> (15 - b)) & 1) ? PAL_WHITE : PAL_BLACK);
}

void pic_frame(int t, int draw)
{
    const ShowCam *c;
    int i, n, k, d, tot;
    ShowClip clip;
    camfx_tick();
    show_l2_events(t);
    show_l2_frame_begin(t);
    if (draw) {
        plat_rect(0, 0, 320, 240, PAL_BLACK);
        show_l2_draw(t);
        stamp(t);
    }
    c = show_cam();
    printf("PIC t=%d cam=%ld,%ld,%ld shake=%d,%d iris=%d fade=%d flash=%d",
           t, (long)c->x, (long)c->y, (long)c->s, c->shake_x, c->shake_y,
           camfx_iris_radius(), camfx_fade_level(), camfx_flash_level());
    for (i = 0; i < show_l2_planes(); i++) {
        ShowXform xf;
        show_l2_plane_xform(i, &xf);
        printf(" p%d=%ld,%ld,%ld", i, (long)xf.ox, (long)xf.oy, (long)xf.s);
    }
    for (i = 0; i < 8; i++) {
        k = show_text_glyphs(i, t, &n);
        if (!n) break;
        printf(" g%d=%d/%d", i, k, n);
    }
    for (i = 0; i < 8; i++) {
        d = show_strokes_revealed(0, i, t, &tot);
        if (!tot) break;
        printf(" s%d=%d/%d", i, d, tot);
    }
    if (show_fx_split_rect(t, &clip)) printf(" split=%d,%d,%d,%d", clip.x, clip.y, clip.w, clip.h);
    printf("\n");
}
