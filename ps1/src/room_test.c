/* room_test.c - the PS1 house renderer's harness layouts (owner: H3;
 * harness/scripts/home.txt, hcmd.c `room-test N`).
 *
 * room.c draws the scene's view with one of these fixed layouts instead
 * while g_room_test is set: the fps gates and the parity pairs. On the game
 * disc this file is a part of the house's overlay group of its own
 * (ps1/tools/ovl_plan.py PARTS "roomtest", room_int.h), so it costs RAM
 * only while a harness script uses it.
 */
#include "r3d.h"

#ifdef DOOMO_HAVE_CORE
#include <string.h>
#include "doomo/palette.h"
#include "doomo/furn.h"
#include "doomo/home.h"
#include "doomo/portrait.h"
#include "../../core/src/home_int.h"      /* HomeRoom, templates */
#include "../../core/gen/furn_ids.h"
#include "room_int.h"

#pragma GCC optimize ("Os")          /* harness only: small, not fast */

/* ---- harness layouts (hcmd.c `room-test N`) ---------------------------------------- */
static void put_obj(HomeRoom *r, int furn, int x, int y, int rot, int z4, int pa, int pb)
{
    HomeObj *o = &r->obj[r->nobj++];
    o->w0 = HO_W0(furn, rot, z4 > 0);
    o->pos = HO_POS(x, y);
    o->z = (uint8_t)z4;
    o->pa = (uint8_t)(pa < 0 ? FURN_PAINT_DEFAULT : pa);
    o->pb = (uint8_t)(pb < 0 ? FURN_PAINT_DEFAULT : pb);
}

/* The parity layout of core/tests/test_home_render.c (as gba/src/room.c). */
static void parity_room(HomeRoom *r)
{
    home_room_template(HOME_ROOM_LIVING, r);
    put_obj(r, FURN_CLOUD_BED, 4, 1, 0, 0, -1, -1);
    put_obj(r, FURN_MARSHMALLOW_SOFA, 1, 4, 0, 0, PAINT_LAVENDER, -1);
    put_obj(r, FURN_TEACUP_TABLE, 2, 2, 0, 0, -1, -1);
    put_obj(r, FURN_CUPCAKE_LAMP, 5, 4, 0, 0, -1, -1);
    put_obj(r, FURN_DAISY_RUG, 3, 3, 0, 0, -1, -1);
    put_obj(r, FURN_BUNNY_BEANBAG, 4, 4, 1, 0, PAINT_SKY, -1);
    put_obj(r, FURN_RAINBOW_BOOKCASE, 2, 0, 0, 0, -1, -1);
    put_obj(r, FURN_HEART_MIRROR, 3, 0, 0, 6, -1, -1);
    put_obj(r, FURN_PIPS_BEE_DRAWING, 0, 3, 1, 8, -1, -1);
    put_obj(r, FURN_JELLYBEAN_JAR, 2, 2, 0, 5, -1, -1);
    put_obj(r, FURN_HORNED_POOP_PLUSH, 0, 5, 0, 0, -1, -1);
    put_obj(r, FURN_POTTED_FERN, 5, 2, 0, 0, -1, -1);
}

/* STEADY: the parity room + 3 animated pieces + 3 walking citizens (the
 * scene's own player, cursor and ghost stay); ORBIT: the same while the yaw
 * turns 12 units a tick; CELLAR: 8x8, 40 pieces, dim; DROP: STEADY with
 * the bed in its soft DROP squash; DEFEND: 8x8, 24 pieces + 10 walkers
 * (Hold the Door's wave 3 crowd); PARITY*: the parity room at a frozen time
 * with 2 citizens, an animated piece, the cursor and a squashed piece, drawn
 * by the single full pass (_LIVE) or through the backdrop (_BD): the
 * harness compares the two shots pixel for pixel. */
void room_test_render(const DoomoRoomView *v)
{
    static const uint16_t cellar_set[8] = {
        FURN_CUPCAKE_LAMP, FURN_POTTED_FERN, FURN_TEACUP_TABLE, FURN_BUNNY_BEANBAG,
        FURN_HORNED_POOP_PLUSH, FURN_JELLYBEAN_JAR, FURN_LAVA_LAMP, FURN_TUBE_TV };
    HomeRoom rr;
    DoomoRoomObj objs[HOME_OBJ_MAX + 4];
    DoomoRoomActor acts[12];
    DoomoRoomView tv = *v;
    uint32_t t = v->time;
    int i, n = 0, na = 0, walkers = 0, mode = g_room_test, force = -1;
    int parity = mode >= ROOM_TEST_PARITY_LIVE;
    if (parity) {
        t = mode >= ROOM_TEST_PARITY2_LIVE ? 97 : 40;
        force = (mode == ROOM_TEST_PARITY_LIVE || mode == ROOM_TEST_PARITY2_LIVE) ? RM_LIVE : RM_FILL;
    }
    if (mode == ROOM_TEST_CELLAR || mode == ROOM_TEST_DEFEND) {
        int want = mode == ROOM_TEST_CELLAR ? HOME_OBJ_MAX : 24;
        tv.w = tv.d = 8;
        home_room_template(HOME_ROOM_CELLAR, &rr);
        for (i = 0; i < rr.nobj && n < want; i++) home_obj_view(&rr.obj[i], &objs[n++]);
        for (i = 0; n < want; i++) {
            memset(&objs[n], 0, sizeof(objs[n]));
            objs[n].furn = cellar_set[i & 7];
            objs[n].x = (uint8_t)(1 + (i % 7));
            objs[n].y = (uint8_t)(1 + (i / 7) % 7);
            objs[n].rot_anim = RO_ROT_ANIM(i & 3, (t >> 3) & 1);
            objs[n].paint_a = (uint8_t)(i % 5 == 0 ? PAINT_MINT : 0xFF);
            objs[n].paint_b = 0xFF;
            n++;
        }
        tv.daylight = 6;
        walkers = mode == ROOM_TEST_DEFEND ? 10 : 0;
    } else {
        parity_room(&rr);
        tv.w = (uint8_t)HR_W(&rr);
        tv.d = (uint8_t)HR_D(&rr);
        tv.wallpaper = home_room_defs[HOME_ROOM_LIVING].wallpaper;
        tv.floor = home_room_defs[HOME_ROOM_LIVING].floor;
        tv.wall_paint = home_room_defs[HOME_ROOM_LIVING].wall_paint;
        tv.floor_paint = home_room_defs[HOME_ROOM_LIVING].floor_paint;
        tv.room_kind = ROOMK_WALLS;
        for (i = 0; i < rr.nobj; i++) home_obj_view(&rr.obj[i], &objs[n++]);
        {
            static const uint8_t ax[3] = { 0, 1, 5 }, ay[3] = { 1, 2, 0 };
            static const uint16_t af[3] = { FURN_TUBE_TV, FURN_LAVA_LAMP, FURN_LULLA_NIGHTLIGHT };
            for (i = 0; i < (parity ? 1 : 3); i++) {
                memset(&objs[n], 0, sizeof(objs[n]));
                objs[n].furn = af[i];
                objs[n].x = ax[i];
                objs[n].y = ay[i];
                objs[n].rot_anim = RO_ROT_ANIM(i == 0 ? 1 : 0, (t >> 3) & 1);
                objs[n].paint_a = objs[n].paint_b = 0xFF;
                objs[n].flags = RO_FULLBRIGHT;
                n++;
            }
        }
        if (mode == ROOM_TEST_DROP || parity) {
            int b = parity ? 2 : 0;             /* parity: the sofa; else the bed */
            objs[b].sq_x = 20;                  /* the soft DROP's mid-squash, 1.16 / 0.84 */
            objs[b].sq_y = -20;
        }
        if (mode == ROOM_TEST_ORBIT) tv.yaw = (int16_t)((t * 12) & 4095);
        walkers = parity ? 2 : 3;
    }
    if (parity) {
        /* the player standing still (so it rests in the backdrop) with a
         * walker passing behind it: its clip draws it again, held pose */
        memset(&acts[na], 0, sizeof(acts[na]));
        acts[na].img = bodyspr_image(DOOMO_PLAYER_ID, 0, 0);
        acts[na].x = INT2FIX(1);
        acts[na].y = INT2FIX(3) + FIX_ONE / 2;
        acts[na].sx = acts[na].sy = 256;
        acts[na].flags = RA_PLAYER;
        na++;
        tv.time = (uint16_t)t;
        tv.cursor_x = (int8_t)(t == 40 ? 3 : 1);
        tv.cursor_y = (int8_t)(t == 40 ? 2 : 4);
        memset(tv.sel_mask, 0, sizeof(tv.sel_mask));
        tv.zoom = 0;
        tv.tint = TINT_NONE;
        tv.yaw = 0;
    } else {
        for (i = 0; i < v->nobj && n < HOME_OBJ_MAX + 4; i++)
            if (v->objs[i].flags & RO_GHOST) objs[n++] = v->objs[i];
        for (i = 0; i < v->nact && na < 2; i++) acts[na++] = v->actors[i];
    }
    for (i = 0; i < walkers && na < 12; i++) {
        uint32_t ph = (t * (uint32_t)(3 + i)) % 160u;
        fix16 p = (fix16)(ph < 80 ? ph : 160 - ph) * (FIX_ONE / 16);
        memset(&acts[na], 0, sizeof(acts[na]));
        acts[na].img = bodyspr_image(1 + i % 3, ph < 80 ? 0 : 3, (int)(t >> 2) & 3);
        acts[na].x = (i & 1) ? INT2FIX(1) / 2 + p : INT2FIX(i % 5) + FIX_ONE / 2;
        acts[na].y = (i & 1) ? INT2FIX(i % 5) + FIX_ONE / 2 : INT2FIX(1) / 2 + p;
        acts[na].sx = acts[na].sy = 256;
        acts[na].flags = (uint8_t)((ph < 80 ? 0 : BB_FLIPX) | BB_SHADOW);
        na++;
    }
    tv.objs = objs;
    tv.nobj = n;
    tv.actors = acts;
    tv.nact = na;
    room_render(&tv, force);
}

#endif /* DOOMO_HAVE_CORE */
