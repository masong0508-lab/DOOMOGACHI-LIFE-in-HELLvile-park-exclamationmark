/* portrait_host.c - the game's portrait/figure/chibi engine on the host, for
 * look-dev model sheets (show ART track; review only, never master pixels).
 *
 * It links the game's own core/src/portrait*.c (with portrait_head.c taken
 * from a build-time copy carrying the show's mouth-override hunk (d) of
 * SHOW_ENGINE_SPEC.md sec.4.3) and draws exactly what the PS1 player will
 * draw from the same 48-byte DoomoLook.  Built by showtk/art/cast/host.py into
 * show/build/art_cast/portrait_host.
 *
 *   portrait_host JOBS OUT
 *
 * JOBS is a sequence of 64-byte records:
 *   0..47  DoomoLook (resident.h order)
 *   48     kind: 0 bust 48x56, 1 figure 48x80, 2 chibi 16x24, 3 fighter 32x48,
 *                4 pet 32x32 (look ignored; arg0 = form, -1 egg, -2 ghost; HOST_PET builds),
 *                5 generate: bytes 0-3 = seed (LE), byte 4 = hellborn; returns the
 *                  48-byte look of resident_randomize_look() as a 48x1 "image"
 *   49     status RES_*
 *   50     expr EXPR_* (fighter: view 0..4)
 *   51     mouth override MOUTH_* or 0xFF (= -1, the engine default)
 *   52-53  anim tick, little endian (blink: anim % 96 >= 92)
 *   54     chibi dir 0..3 / fighter anim 0..6 / pet frame
 *   55     chibi frame 0..3 / fighter weapon frame (0xFF = none)
 *   56     arg0 (signed; pet form)
 *   57..63 reserved (0)
 * OUT gets 768 bytes of palette (256 x RGB888, the master palette decoded by
 * palette_rgb) and then, per job, u16 w, u16 h (LE) and w*h index bytes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/palette.h"
#include "doomo/town.h"
#include "doomo/pet.h"

/* the show's override variables (show_actor.c defines them in the player) */
int8_t portrait_show_mouth = -1;
int8_t portrait_show_blink = -1;

/* what game.c / town.c would define; the direct render calls never use them */
uint32_t doomo_frame_count(void) { return 0; }
uint32_t doomo_image_new_uid(void) { static uint32_t u = 1; return u++; }
DoomoResident *town_res(int id) { (void)id; return NULL; }

static uint8_t buf[80 * 80];

static void put16(FILE *f, unsigned v)
{
    fputc((int)(v & 255), f);
    fputc((int)((v >> 8) & 255), f);
}

int main(int argc, char **argv)
{
    FILE *in, *out;
    unsigned char rec[64];
    int i;
    if (argc != 3) {
        fprintf(stderr, "usage: portrait_host JOBS OUT\n");
        return 2;
    }
    in = fopen(argv[1], "rb");
    out = fopen(argv[2], "wb");
    if (!in || !out) {
        fprintf(stderr, "portrait_host: cannot open %s or %s\n", argv[1], argv[2]);
        return 2;
    }
    palette_init();
    for (i = 0; i < 256; i++) {
        uint8_t r, g, b;
        palette_rgb((uint8_t)i, &r, &g, &b);
        fputc(r, out); fputc(g, out); fputc(b, out);
    }
    while (fread(rec, 1, sizeof rec, in) == sizeof rec) {
        DoomoLook l;
        int kind = rec[48], status = rec[49], expr = rec[50];
        int mouth = rec[51] == 0xFF ? -1 : rec[51];
        int anim = rec[52] | (rec[53] << 8);
        int a = rec[54], b = rec[55], w, h;
        int arg0 = (signed char)rec[56];
        memcpy(&l, rec, sizeof l);
        memset(buf, 0, sizeof buf);
        portrait_show_mouth = (int8_t)mouth;
        switch (kind) {
        case 0: w = PORTRAIT_W; h = PORTRAIT_H; portrait_render(&l, status, expr, anim, buf); break;
        case 1: w = FIGURE_W; h = FIGURE_H; figure_render(&l, status, expr, anim, buf); break;
        case 2: w = BODYSPR_W; h = BODYSPR_H; bodyspr_render(&l, status, a & 3, b & 3, buf); break;
        case 3: w = FIGHTER_W; h = FIGHTER_H;
                fighter_render(&l, status, expr % 5, a % 7, b == 0xFF ? -1 : b, buf); break;
#ifdef HOST_PET
        case 4: w = 32; h = 32; pet_render(arg0, a, buf); break;
#endif
        case 5: {   /* crowd look generation: resident_randomize_look(), the PS1's own */
            DoomoResident r;
            uint32_t seed = (uint32_t)rec[0] | ((uint32_t)rec[1] << 8) |
                            ((uint32_t)rec[2] << 16) | ((uint32_t)rec[3] << 24);
            memset(&r, 0, sizeof r);
            r.flags = rec[4] ? RF_HELLBORN : 0;
            resident_randomize_look(&r, seed);
            w = 48; h = 1;
            memcpy(buf, &r.look, sizeof r.look);
            break; }
        default:
            fprintf(stderr, "portrait_host: unknown kind %d\n", kind);
            return 2;
        }
        (void)arg0;
        portrait_show_mouth = -1;
        put16(out, (unsigned)w);
        put16(out, (unsigned)h);
        fwrite(buf, 1, (size_t)(w * h), out);
    }
    fclose(in);
    fclose(out);
    return 0;
}
