/* portrait_hair.c - hair, ears, horns, hats, face accessories and halos
 * (owner: portrait). Drawn in normalised head space (see portrait_head.c).
 */
#include "portrait_int.h"
#include "doomo/resident.h"

#define FRONT_VIEW (phs.h->yaw < 3)
#define BACK_VIEW  (phs.h->yaw >= 3)

/* clip to rows above/below a head-space y (pixels) */
static void clip_above(int ny) { pc_clip(0, 0, pcv.w, (hy(ny) + 8) >> 4); }
static void clip_below(int ny) { pc_clip(0, (hy(ny) + 8) >> 4, pcv.w, pcv.h); }

/* ======================================================================
 * hair
 * ==================================================================== */
static void hair_mat(PCtx *c, int flags)
{
    pc_part(&c->hair, SH_BALL, flags);
    hHeadFrame(40);
}

/* anime shine band + optional dyed streaks, masked to the hair part */
static void hair_shine(PCtx *c, int lo, int hi_part, int y)
{
    const DoomoLook *l = c->l;
    int x, k;
    uint8_t s1 = c->hair.c[4], s2 = c->hair.c[3];
    if (phs.h->lod == LOD_TINY) return;
    pc_mask_none();
    pc_mask_range(lo, hi_part);
    if (l->hair_highlight != LOOK_NO_COLOR) { s1 = c->hairhi.c[3]; s2 = c->hairhi.c[2]; }
    for (k = -3; k <= 3; k++) {
        int nx = k * 62;
        x = hx(nx) >> 4;
        {
            int yy = (hy(y + (k * k * 6)) >> 4);
            pc_px(x, yy, s1);
            pc_px(x + 1, yy, s2);
            if (phs.h->lod == LOD_BIG) pc_px(x, yy + 1, s2);
        }
    }
    if (l->hair_highlight != LOOK_NO_COLOR) {
        /* a few dyed strands sweeping down one side + coloured tips */
        pc_decal(&c->hairhi, SH_BALL);
        hHeadFrame(40);
        hB(-60, -270, -150, -200, -170, -70, 18, 12, 4);
        hB(-10, -270, -80, -190, -100, -90, 14, 10, 4);
        hB(120, -250, 200, -160, 230, -40, 14, 10, 4);
        hE(-280, 300, 70, 60);
        hE(280, 300, 70, 60);
    }
    pc_mask_none();
}

static void cap(int top_clip, int grow)
{
    clip_above(top_clip);
    hE(0, -10, 256 + grow, 256 + grow / 2);
    pc_noclip();
}

/* radial spike at angle a (0..255 turns), from radius r0 to r1 */
static void radial_spike(int a, int r0, int r1, int w)
{
    int cx = p_cos(a), sy = p_sin(a);
    int px = -sy, py = cx;
    hT((cx * r0 + px * w) / 256, (sy * r0 + py * w) / 256,
       (cx * r0 - px * w) / 256, (sy * r0 - py * w) / 256,
       cx * r1 / 256, sy * r1 / 256);
}

static void bangs_soft(int y)
{
    /* three rounded tufts over the forehead */
    hT(-230, -150, -70, -170, -170, y);
    hT(-110, -180, 60, -180, -30, y + 20);
    hT(20, -180, 220, -150, 140, y + 6);
}

static void hair_tie(PCtx *c, int x, int y, int r)
{
    PMat m;
    int col = c->l->accessory_color;
    if (c->l->hair_highlight != LOOK_NO_COLOR) col = c->l->hair_highlight;
    pm_paint(&m, col);
    pc_part(&m, SH_BALL, 0);
    hE(x, y, r, r);
}

/* back layer: everything that hangs behind the head (front views) or is
 * seen across the back (back views draw it in front instead). */
static void hair_back_shapes(PCtx *c)
{
    int st = c->l->hair_style, k;
    switch (st) {
    case HAIR_BOB:
        clip_above(170);
        hE(0, 20, 304, 280);
        pc_noclip();
        break;
    case HAIR_LONG:
        pc_rrect(hx(0), hy(160), hr(316), hry(390), hr(120));
        break;
    case HAIR_PONYTAIL:
        phs.z = -120;
        hB(160, -210, 380, -80, 330, 330, 74, 40, 6);
        phs.z = 0;
        break;
    case HAIR_TWINTAILS:
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hB(230, -150, 400, 0, 350, 300, 90, 50, 6);
            hE(340, 120, 90, 150);
        }
        phs.s = 1;
        break;
    case HAIR_AFRO:
        pc_part(&c->hair, SH_SOFT, PF_FUZZ);
        hHeadFrame(120);
        hE(0, -60, 370, 320);
        for (k = 0; k < 12; k++) {
            int a = 128 + k * 11;
            hE(p_cos(a) * 340 / 256, -60 + p_sin(a) * 296 / 256, 70, 70);
        }
        break;
    case HAIR_CURLY:
        for (k = 0; k < 11; k++) {
            int a = 120 + k * 13;
            hE(p_cos(a) * 280 / 256, -10 + p_sin(a) * 270 / 256, 72, 72);
        }
        break;
    case HAIR_MULLET:
        hQ(-250, -40, 250, -40, 300, 330, -300, 330);
        hE(0, 330, 300, 60);
        break;
    case HAIR_LOCS:
        for (k = -4; k <= 4; k++) {
            int x = k * 68;
            hC(x, -200, x * 120 / 100, 330 + (k & 1) * 40, 40, 34);
        }
        break;
    case HAIR_HIME:
        hQ(-310, -100, 310, -100, 310, 470, -310, 470);
        hE(0, -60, 300, 240);
        break;
    case HAIR_WAVY:
        hE(0, -40, 300, 250);
        for (phs.s = -1; phs.s <= 1; phs.s += 2)
            for (k = 0; k < 6; k++)
                hE(270 + ((k & 1) ? 34 : 0), -20 + k * 80, 70, 56);
        phs.s = 1;
        hE(0, 300, 260, 140);
        break;
    case HAIR_WILD:
        pc_part(&c->hair, SH_BALL, PF_FUZZ);
        hHeadFrame(80);
        hE(0, -30, 290, 280);
        for (k = 0; k < 11; k++) {
            static const int16_t len[11] = { 400, 360, 420, 380, 440, 400, 430, 370, 410, 390, 360 };
            radial_spike(96 + k * 13, 200, len[k], 70);
        }
        break;
    case HAIR_ODANGO:
        for (phs.s = -1; phs.s <= 1; phs.s += 2) hB(200, -220, 380, 0, 330, 340, 60, 36, 6);
        phs.s = 1;
        break;
    case HAIR_DRILLS:
        hE(0, -30, 290, 270);
        break;
    case HAIR_TENTACLES:
        for (k = -3; k <= 3; k++) {
            int x = k * 90;
            hB(x, -150, x * 150 / 100, 120, x * 130 / 100 + (k < 0 ? -40 : 40), 360, 44, 22, 6);
        }
        break;
    case HAIR_SLIME_DRIP:
        hE(0, -20, 290, 270);
        break;
    case HAIR_PIGTAILS:
        break;
    case HAIR_BRAIDS:
        hE(0, -20, 282, 268);
        break;
    case HAIR_SIDE_SHAVE:
        hQ(60, -100, 300, -100, 330, 300, 120, 300);
        break;
    case HAIR_EMO_FRINGE:
        hE(0, 0, 290, 280);
        break;
    case HAIR_CROWN_BRAID:
    case HAIR_SHORT: case HAIR_SPIKY: case HAIR_BUN: case HAIR_POMPADOUR:
    case HAIR_BOWL: case HAIR_MESSY: case HAIR_FLAME: case HAIR_LIBERTY_SPIKES:
    default:
        break;
    }
}

void ph_hair_back(PCtx *c)
{
    int st = c->l->hair_style;
    phs.c = c; phs.h = &c->h; phs.s = 1; phs.z = -100;
    if (st == HAIR_BALD) return;
    if (BACK_VIEW) return;              /* drawn by ph_hair_front instead */
    hair_mat(c, 0);
    {
        int first = pcv.part;
        hair_back_shapes(c);
        if (st == HAIR_LONG || st == HAIR_HIME || st == HAIR_WAVY || st == HAIR_BOB)
            hair_shine(c, first, pcv.part, -150);
    }
    phs.z = 0;
}

static void flame_tongue(PCtx *c, int x, int base, int h, int w)
{
    int wob = ((c->anim >> 2) + x) & 3;
    int tx = x + (wob == 1 ? 30 : (wob == 3 ? -30 : 0));
    hT(x - w, base, x + w, base, tx, base - h);
    hE(x, base, w, w * 3 / 4);
}

/* cap + bangs + top pieces (in front of the head) */
static void hair_front_shapes(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->hair_style, k;
    int fv = FRONT_VIEW;
    if (!fv) {
        /* back of the head: a full cap down to the nape + the back shapes */
        if (st == HAIR_BUZZ || st == HAIR_SLICKED || st == HAIR_MOHAWK || st == HAIR_SIDE_SHAVE)
            cap(40, 6);
        else if (st != HAIR_LIBERTY_SPIKES && st != HAIR_FLAME)
            cap(150, 24);
        hair_back_shapes(c);
    }
    switch (st) {
    case HAIR_BUZZ: {
        int y, x;
        if (fv) cap(-110, 6);
        /* stubbly edge */
        for (x = -14; x <= 14; x++) {
            y = (hy(-110) >> 4);
            if (x & 1) pc_px((hx(0) >> 4) + x, y, c->hair.c[1]);
        }
        break; }
    case HAIR_SHORT:
        if (fv) { cap(-90, 20); bangs_soft(-40); }
        hT(-262, -100, -200, -160, -262, 30);
        hT(262, -100, 200, -160, 262, 30);
        break;
    case HAIR_SPIKY:
        cap(-80, 22);
        for (k = 0; k < 7; k++) radial_spike(144 + k * 16, 200, 380 + (k & 1) * 40, 72);
        if (fv) {
            hT(-200, -160, -90, -170, -170, -30);
            hT(-60, -180, 60, -180, 0, -40);
            hT(90, -170, 200, -160, 170, -30);
        }
        break;
    case HAIR_MOHAWK:
        pc_rrect(hx(0), hy(-200), hr(56), hry(110), hr(40));
        for (k = 0; k < 4; k++) hT(-56, -230 + k * 40, 56, -230 + k * 40, 0, -380 + k * 30);
        break;
    case HAIR_BOB:
        if (fv) {
            clip_above(-80);
            hE(0, -20, 282, 270);
            pc_noclip();
            for (phs.s = -1; phs.s <= 1; phs.s += 2) {
                hC(250, -120, 262, 120, 52, 60);
                hE(250, 130, 66, 50);
            }
            phs.s = 1;
        }
        break;
    case HAIR_LONG:
        if (fv) {
            cap(-100, 24);
            hT(0, -270, -280, -40, -40, -150);
            hT(0, -270, 280, -40, 40, -150);
            hC(-262, -80, -280, 330, 52, 40);
            hC(262, -80, 280, 330, 52, 40);
        }
        break;
    case HAIR_PONYTAIL:
        cap(-90, 20);
        if (fv) {
            hT(-240, -160, 120, -200, -180, -30);
            hT(60, -190, 230, -150, 200, -60);
        }
        hair_tie(c, 170, -200, 34);
        if (!fv) hB(0, -100, 30, 100, 0, 330, 70, 40, 5);
        break;
    case HAIR_TWINTAILS:
        cap(-80, 20);
        if (fv) bangs_soft(-30);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) hair_tie(c, 220, -170, 30);
        phs.s = 1;
        break;
    case HAIR_BUN:
        cap(-100, 14);
        hair_mat(c, 0);
        hE(0, -290, 108, 96);
        if (fv) {
            hT(-262, -90, -200, -140, -250, 40);
            hT(262, -90, 200, -140, 250, 40);
        }
        hair_tie(c, 0, -206, 26);
        break;
    case HAIR_AFRO:
        if (fv) {
            pc_part(&c->hair, SH_SOFT, PF_FUZZ);
            hHeadFrame(120);
            cap(-90, 30);
            for (k = -3; k <= 3; k++) hE(k * 70, -100 - (k * k) * 3, 50, 44);
        }
        break;
    case HAIR_CURLY:
        cap(-110, 24);
        if (fv) for (k = -3; k <= 3; k++) hE(k * 72, -120 - (k * k) * 4, 52, 50);
        break;
    case HAIR_MULLET:
        cap(-110, 18);
        if (fv) {
            for (k = 0; k < 5; k++) hT(-200 + k * 80, -160, -120 + k * 80, -160, -160 + k * 80, -80);
        }
        break;
    case HAIR_EMO_FRINGE:
        cap(-100, 22);
        if (fv) {
            hT(-240, -200, 240, -150, 200, 160);
            hT(-100, -180, 240, -100, 40, 100);
            hC(220, -100, 250, 200, 46, 30);
        }
        break;
    case HAIR_POMPADOUR:
        cap(-120, 10);
        hE(-10, -270, 210, 120);
        hE(60, -250, 160, 110);
        if (fv) hT(-60, -200, 120, -180, 60, -120);
        break;
    case HAIR_BRAIDS:
        cap(-100, 18);
        if (fv) {
            PMat p;
            pm_flat(&p, c->hair.c[1]);
            pc_decal(&p, SH_FLAT);
            hL(0, -260, 0, -120, c->hair.c[0]);
            pc_same(&c->hair, SH_BALL);
        }
        hair_mat(c, 0);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            for (k = 0; k < 6; k++) hE(240 + (k & 1) * 14, -30 + k * 66, 50, 44);
        }
        phs.s = 1;
        for (phs.s = -1; phs.s <= 1; phs.s += 2) hair_tie(c, 248, 380, 26);
        phs.s = 1;
        break;
    case HAIR_SIDE_SHAVE: {
        int y, x;
        clip_above(-90);
        pc_clip(hx(-60) >> 4, 0, pcv.w, (hy(-90) + 8) >> 4);
        hE(0, -10, 270, 266);
        pc_noclip();
        hQ(-60, -260, 260, -200, 300, 100, 180, 120);
        /* stubble on the shaved side */
        for (y = -200; y <= -60; y += 30)
            for (x = -240; x <= -100; x += 30)
                hP(x + ((y / 30) & 1) * 15, y, c->hair.c[1]);
        break; }
    case HAIR_WILD:
        if (fv) {
            cap(-80, 30);
            for (k = 0; k < 5; k++) hT(-220 + k * 100, -170, -150 + k * 100, -170, -200 + k * 100 + (k & 1) * 30, -30);
        }
        break;
    case HAIR_LOCS:
        cap(-110, 20);
        if (fv) {
            hC(-120, -170, -150, -40, 34, 28);
            hC(20, -180, 40, -60, 34, 28);
            hC(150, -170, 180, -50, 34, 28);
            hC(-262, -100, -290, 300, 40, 34);
            hC(262, -100, 290, 300, 40, 34);
        }
        break;
    case HAIR_PIGTAILS:
        cap(-90, 20);
        if (fv) bangs_soft(-40);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hair_mat(c, 0);
            hE(320, 190, 80, 100);
            hT(270, 140, 380, 160, 360, 330);
            hair_tie(c, 262, 140, 26);
        }
        phs.s = 1;
        break;
    case HAIR_HIME:
        if (fv) {
            clip_above(-90);
            hE(0, -20, 282, 272);
            pc_noclip();
            for (phs.s = -1; phs.s <= 1; phs.s += 2) {
                hC(262, -120, 262, 150, 48, 48);
                hQ(214, 150, 310, 150, 310, 170, 214, 170);
            }
            phs.s = 1;
        }
        break;
    case HAIR_WAVY:
        cap(-100, 22);
        if (fv) {
            hT(-250, -180, 120, -220, -200, -20);
            hE(-210, -40, 60, 70);
            hC(250, -100, 290, 260, 50, 40);
        }
        break;
    case HAIR_BOWL:
        clip_above(-40);
        hE(0, -30, 290, 280);
        pc_noclip();
        break;
    case HAIR_LIBERTY_SPIKES:
        cap(-130, 6);
        for (k = 0; k < 5; k++) radial_spike(150 + k * 14, 180, 520, 56);
        break;
    case HAIR_SLICKED: {
        cap(-120, 4);
        if (fv) hT(-50, -200, 50, -200, 0, -110);
        pc_decal(&c->hair, SH_FLAT);
        hL(-120, -230, -150, -140, c->hair.c[3]);
        hL(0, -250, 0, -170, c->hair.c[3]);
        hL(120, -230, 150, -140, c->hair.c[3]);
        break; }
    case HAIR_MESSY:
        cap(-90, 22);
        hT(-240, -200, -150, -230, -320, -260);
        hT(150, -240, 220, -190, 300, -300);
        hT(-40, -260, 40, -260, 30, -340);
        if (fv) {
            hT(-200, -160, -40, -170, -160, -20);
            hT(-30, -170, 160, -170, 70, -40);
            hT(150, -150, 250, -110, 250, -10);
        }
        break;
    case HAIR_ODANGO:
        cap(-90, 18);
        if (fv) bangs_soft(-40);
        hair_mat(c, 0);
        hE(-180, -250, 92, 88);
        hE(180, -250, 92, 88);
        break;
    case HAIR_DRILLS:
        cap(-90, 22);
        if (fv) bangs_soft(-30);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hair_mat(c, 0);
            for (k = 0; k < 5; k++) hE(300, -40 + k * 76, 80 - k * 8, 46);
            pc_decal(&c->hair, SH_FLAT);
            for (k = 0; k < 5; k++) hL(240 + k * 4, -60 + k * 76, 350 - k * 8, -20 + k * 76, c->hair.c[1]);
        }
        phs.s = 1;
        break;
    case HAIR_FLAME: {
        PMat core;
        cap(-90, 16);
        for (k = -2; k <= 2; k++) flame_tongue(c, k * 90, -200 + p_abs(k) * 30, 200 - p_abs(k) * 30, 70);
        if (fv) bangs_soft(-60);
        /* glowing core and hot tips */
        pm_ramp(&core, RAMP_LAVA, 5);
        if (c->l->hair_highlight != LOOK_NO_COLOR) core = c->hairhi;
        pc_mask_none();
        pc_mask_add(pcv.part);
        pc_decal(&core, SH_SOFT);
        hHeadFrame(60);
        for (k = -2; k <= 2; k++) hE(k * 90, -230 + p_abs(k) * 30, 40, 70);
        pc_mask_none();
        for (k = -2; k <= 2; k++) {
            int wob = (((c->anim >> 2) + k) & 1) ? 20 : -20;
            hP(k * 90 + wob, -390 + p_abs(k) * 60, PAL_RAMP(RAMP_LAVA, 7));
        }
        break; }
    case HAIR_TENTACLES:
        cap(-100, 20);
        if (fv) {
            hB(-200, -120, -280, 60, -230, 220, 40, 22, 5);
            hB(200, -120, 280, 60, 230, 220, 40, 22, 5);
            hB(-40, -180, -100, -60, -40, -10, 34, 20, 4);
        }
        break;
    case HAIR_SLIME_DRIP:
        pc_part(&c->hair, SH_GLOSS, 0);
        hHeadFrame(40);
        cap(-90, 20);
        hC(-180, -120, -180, 30, 40, 34);
        hC(-40, -120, -40, -40, 30, 26);
        hC(110, -120, 110, 60, 36, 32);
        hC(230, -100, 230, 0, 30, 26);
        break;
    case HAIR_CROWN_BRAID:
        cap(-110, 10);
        if (fv) bangs_soft(-60);
        hair_mat(c, 0);
        for (k = 0; k < 9; k++) {
            int a = 136 + k * 12;
            hE(p_cos(a) * 250 / 256, -40 + p_sin(a) * 230 / 256, 50, 40);
        }
        break;
    default:
        break;
    }
}

void ph_hair_front(PCtx *c)
{
    int st = c->l->hair_style, first;
    PHead hh;
    phs.c = c; phs.h = &c->h; phs.s = 1; phs.z = 120;
    if (st == HAIR_BALD) {
        /* a little shine on bald heads (not on fur) */
        if (!c->critter && c->h.lod == LOD_BIG && c->l->species == SPECIES_HUMAN) {
            hP(-100, -200, c->skin.c[4]);
            hP(-80, -210, c->skin.c[4]);
        }
        phs.z = 0;
        return;
    }
    /* hair sits a touch higher than the skull: more face, more volume */
    hh = c->h;
    if (FRONT_VIEW) hh.cy -= hh.ry * 9 / 100;
    phs.h = &hh;
    hair_mat(c, st == HAIR_AFRO || st == HAIR_WILD ? PF_FUZZ : 0);
    first = pcv.part;
    hair_front_shapes(c);
    hair_shine(c, first, pcv.part, -190);
    phs.h = &c->h;
    phs.z = 0;
}

/* ======================================================================
 * ears
 * ==================================================================== */
static void ear_inner(PCtx *c, uint8_t col)
{
    PMat m;
    (void)c;
    pm_flat(&m, col);
    pc_decal(&m, SH_FLAT);
}

static void ear_tip(PCtx *c)
{
    PMat m;
    pm_paint(&m, c->l->marking_color);
    pc_mask_none();
    pc_mask_add(pcv.part);
    pc_decal(&m, SH_BALL);
}

static void one_ear(PCtx *c, int st, int front)
{
    uint8_t pink = PAL_RAMP(RAMP_PASTEL_PINK, 5), pink2 = PAL_RAMP(RAMP_PASTEL_PINK, 4);
    int tip = c->l->marking == MARK_FUR_TIPPED_EARS;
    int tw = (phs.s > 0 && c->twitch) ? 30 : 0;     /* ear twitch */
    if (c->creepy) { pink = c->skin.c[1]; pink2 = c->skin.c[0]; }
    pc_part(&c->skin, SH_BALL, c->critter ? PF_FUZZ : 0);
    switch (st) {
    case EARS_NORMAL:
        if (front) break;
        hE(250, 30, 56, 80);
        ear_inner(c, c->skin.c[1]);
        hL(250, 0, 260, 60, c->skin.c[1]);
        break;
    case EARS_POINTY:
        if (front) break;
        hT(220, -60, 230, 110, 400 - tw, -150 + tw);
        ear_inner(c, c->skin.c[1]);
        hT(250, -20, 252, 60, 340 - tw, -100 + tw);
        break;
    case EARS_ELF:
        if (front) break;
        hT(220, -40, 230, 120, 470 - tw, -280 + tw);
        ear_inner(c, c->skin.c[1]);
        hT(250, 0, 252, 80, 400 - tw, -210 + tw);
        break;
    case EARS_CAT:
        if (front) break;
        hT(-20 + 80, -250, 240, -120, 210 - tw / 2, -400 + tw);
        ear_inner(c, pink);
        hT(100, -220, 210, -150, 200 - tw / 2, -330 + tw);
        if (tip) { ear_tip(c); hE(210, -390, 50, 50); pc_mask_none(); }
        break;
    case EARS_DOG:
        if (front) break;
        hT(60, -240, 250, -110, 280, -360);
        pc_same(&c->skin, SH_BALL);
        hT(230, -360, 290, -350, 340, -230 + tw);
        ear_inner(c, c->skin.c[1]);
        hT(120, -210, 230, -140, 260, -300);
        if (tip) { ear_tip(c); hE(320, -260, 60, 60); pc_mask_none(); }
        break;
    case EARS_BAT:
        if (front) break;
        hT(20, -230, 250, -80, 340 - tw, -440 + tw);
        ear_inner(c, pink2);
        hT(90, -200, 230, -110, 300 - tw, -360 + tw);
        hL(140, -170, 290, -360, c->skin.c[1]);
        if (tip) { ear_tip(c); hE(330, -420, 50, 50); pc_mask_none(); }
        break;
    case EARS_FIN: {
        PMat mem;
        if (front) break;
        pm_from3(&mem, c->skin.c[2], c->skin.c[3], c->skin.c[4]);
        pc_part(&mem, SH_SOFT, 0);
        hT(220, -100, 230, 120, 380, -180 + tw);
        hT(230, 0, 230, 130, 360, 120);
        pc_decal(&c->skin, SH_FLAT);
        hL(240, -40, 360, -160, c->skin.c[1]);
        hL(240, 30, 360, 0, c->skin.c[1]);
        hL(240, 90, 340, 110, c->skin.c[1]);
        break; }
    case EARS_BUNNY: {
        int flop = phs.s > 0 && (c->l->species == SPECIES_BUNNYKIN || 1) ? tw : 0;
        if (front) break;
        hFrame(140, -460, 80, 220);
        hC(110, -200, 150 + flop, -620 + flop, 70, 62);
        hE(150 + flop, -620 + flop, 64, 50);
        ear_inner(c, pink);
        hC(118, -260, 150 + flop, -580 + flop, 30, 26);
        if (tip) { ear_tip(c); hE(150 + flop, -640 + flop, 70, 60); pc_mask_none(); }
        break; }
    case EARS_FOX: {
        PMat fl;
        if (front) break;
        hT(40, -250, 250, -100, 250 - tw / 2, -470 + tw);
        pm_paint(&fl, PAINT_WHITE);
        if (c->l->marking >= MARK_FUR_MUZZLE) pm_paint(&fl, c->l->marking_color);
        pc_decal(&fl, SH_SOFT);
        hT(90, -210, 220, -130, 240 - tw / 2, -380 + tw);
        hT(120, -200, 170, -150, 120, -280);
        if (tip) { ear_tip(c); hE(250 - tw / 2, -440 + tw, 60, 70); pc_mask_none(); }
        else {
            PMat dk;
            pm_from3(&dk, c->skin.c[0], c->skin.c[0], c->skin.c[1]);
            pc_mask_none(); pc_mask_add(pcv.part);
            pc_decal(&dk, SH_FLAT);
            hE(250 - tw / 2, -460 + tw, 40, 50);
            pc_mask_none();
        }
        break; }
    case EARS_BEAR:
        if (front) break;
        hE(180, -220, 96, 96);
        ear_inner(c, c->skin.c[1]);
        hE(180, -220, 50, 50);
        if (tip) { ear_tip(c); hE(180, -300, 90, 40); pc_mask_none(); }
        break;
    case EARS_MOUSE:
        if (front) break;
        hE(240 - tw / 3, -200 + tw / 3, 150, 150);
        ear_inner(c, pink);
        hE(240 - tw / 3, -200 + tw / 3, 96, 96);
        if (tip) { ear_tip(c); hE(260, -320, 130, 50); pc_mask_none(); }
        break;
    case EARS_SHEEP:
        if (front) break;
        hFrame(320, 0, 110, 60);
        hC(220, -40, 380, 30 + tw / 2, 50, 44);
        ear_inner(c, pink);
        hC(250, -30, 360, 26 + tw / 2, 18, 18);
        if (tip) { ear_tip(c); hE(390, 40, 50, 50); pc_mask_none(); }
        break;
    case EARS_FLOPPY_PUP: {
        PMat m = c->skin;
        if (!front) break;
        if (c->l->hair_style != HAIR_BALD) m = c->hair;
        pc_part(&m, SH_BALL, c->critter ? PF_FUZZ : 0);
        hFrame(250, 30, 110, 260);
        hC(200, -180, 270 + tw / 2, 180, 80, 92);
        hE(270 + tw / 2, 190, 96, 70);
        if (tip) { ear_tip(c); hE(270 + tw / 2, 220, 110, 70); pc_mask_none(); }
        break; }
    case EARS_OWL_TUFTS:
        if (front) break;
        hT(80, -230, 200, -170, 300 - tw, -410 + tw);
        hT(150, -210, 230, -150, 340 - tw, -320 + tw);
        pc_decal(&c->skin, SH_FLAT);
        hL(140, -220, 270, -370, c->skin.c[1]);
        if (tip) { ear_tip(c); hE(300, -400, 50, 50); pc_mask_none(); }
        break;
    case EARS_RACCOON: {
        PMat w;
        if (front) break;
        hT(70, -240, 250, -120, 210 - tw / 2, -380 + tw);
        hE(190, -270, 80, 80);
        pm_paint(&w, PAINT_WHITE);
        if (c->l->marking >= MARK_FUR_MUZZLE) pm_paint(&w, c->l->marking_color);
        pc_decal(&w, SH_SOFT);
        hE(190, -265, 60, 60);
        ear_inner(c, c->skin.c[0]);
        hE(190, -255, 36, 40);
        break; }
    case EARS_NONE:
    default:
        break;
    }
}

void ph_ears(PCtx *c, int front)
{
    int st = c->l->ears;
    phs.c = c; phs.h = &c->h; phs.z = 0;
    if (st == EARS_NONE) return;
    /* hats with their own ears replace real top ears */
    for (phs.s = -1; phs.s <= 1; phs.s += 2) one_ear(c, st, front);
    phs.s = 1;
    pc_mask_none();
}

/* ======================================================================
 * horns
 * ==================================================================== */
static int HS;      /* horn scale Q8 */
#define HSC(v) ((v) * HS / 256)

static void horn_mat(PCtx *c)
{
    pc_part(&c->horn, SH_BALL, 0);
}

static void ridges(int ax, int ay, int bx, int by, int n, uint8_t col)
{
    int k;
    for (k = 1; k <= n; k++) {
        int x = ax + (bx - ax) * k / (n + 1), y = ay + (by - ay) * k / (n + 1);
        hP(x, y, col);
        hP(x + 14, y + 4, col);
    }
}

static void one_horn(PCtx *c, int st)
{
    int bx = 130, by = -220;
    switch (st) {
    case HORNS_NUBS:
        horn_mat(c);
        hE(bx, by - HSC(20), HSC(56), HSC(56));
        hT(bx - HSC(56), by, bx + HSC(56), by, bx + HSC(10), by - HSC(100));
        break;
    case HORNS_RAM:
        horn_mat(c);
        hB(180, -200, 330 + HSC(60), -260, 320 + HSC(40), -20, HSC(60), HSC(44), 6);
        hB(320 + HSC(40), -20, 300, 60, 250, -40, HSC(44), HSC(26), 4);
        ridges(220, -220, 330, -80, 4, c->horn.c[1]);
        break;
    case HORNS_BULL:
        horn_mat(c);
        hB(200, -150, 360 + HSC(80), -140, 380 + HSC(100), -280 - HSC(140), HSC(54), HSC(14), 6);
        break;
    case HORNS_DEVIL:
        horn_mat(c);
        hB(110, -210, 190 + HSC(30), -300 - HSC(40), 140 + HSC(20), -320 - HSC(160), HSC(50), HSC(10), 5);
        break;
    case HORNS_UNICORN: {
        int k, h = HSC(380);
        if (phs.s < 0) break;
        horn_mat(c);
        pc_tri(hx(-60), hy(-220), hx(60), hy(-220), hx(0), hy(-220 - h));
        pc_decal(&c->horn, SH_FLAT);
        for (k = 1; k < 5; k++) {
            int y = -220 - h * k / 5, w = 60 - 60 * k / 5;
            hL(-w, y + 16, w, y - 10, c->horn.c[1]);
        }
        break; }
    case HORNS_ANTLERS:
        horn_mat(c);
        hB(120, -220, 240, -320, 300 + HSC(40), -460 - HSC(80), HSC(34), HSC(20), 5);
        hC(200, -300, 120 + HSC(10), -420 - HSC(60), HSC(24), HSC(14));
        hC(260, -370, 400 + HSC(40), -420 - HSC(40), HSC(22), HSC(12));
        hC(230, -260, 330 + HSC(30), -280, HSC(20), HSC(12));
        break;
    case HORNS_GOAT:
        horn_mat(c);
        hB(90, -230, 160, -420 - HSC(80), 320 + HSC(60), -330 - HSC(20), HSC(52), HSC(14), 6);
        ridges(110, -260, 220, -400, 3, c->horn.c[1]);
        break;
    case HORNS_ONI: {
        int k, h = HSC(240);
        horn_mat(c);
        hC(110, -210, 120 + HSC(20), -210 - h, HSC(54), HSC(16));
        pc_decal(&c->horn, SH_FLAT);
        for (k = 1; k <= 2; k++) hL(80, -210 - h * k / 3, 150, -210 - h * k / 3 - 10, c->horn.c[0]);
        break; }
    case HORNS_SPIRAL: {
        int k, h = HSC(320);
        horn_mat(c);
        pc_tri(hx(90), hy(-210), hx(190), hy(-210), hx(170 + HSC(30)), hy(-210 - h));
        pc_decal(&c->horn, SH_FLAT);
        for (k = 1; k < 5; k++) {
            int y = -210 - h * k / 5;
            hL(100 + k * 14, y + 12, 180 - k * 6, y - 14, c->horn.c[1]);
        }
        break; }
    case HORNS_BROKEN:
        horn_mat(c);
        if (phs.s < 0) {
            hB(110, -210, 190 + HSC(30), -300 - HSC(40), 140 + HSC(20), -320 - HSC(160), HSC(50), HSC(10), 5);
        } else {
            hC(120, -210, 140, -290, HSC(50), HSC(44));
            pc_decal(&c->horn, SH_FLAT);
            hT(80, -300, 200, -300, 150, -260);
            hL(100, -290, 160, -270, c->horn.c[0]);
            hL(160, -270, 190, -300, c->horn.c[0]);
        }
        break;
    case HORNS_CROWN_SPIKES: {
        int k;
        if (phs.s < 0) break;
        horn_mat(c);
        for (k = -2; k <= 2; k++) {
            int x = k * 90, y = -230 + p_abs(k) * 22;
            hT(x - 40, y + 20, x + 40, y + 20, x, y - HSC(120 - p_abs(k) * 20));
        }
        break; }
    case HORNS_ANTENNAE: {
        PMat tipm;
        pm_paint(&tipm, c->l->horn_color);
        horn_mat(c);
        hB(60, -220, 90, -360, 200 + HSC(40), -420 - HSC(60), HSC(14) + 8, HSC(10) + 6, 4);
        pc_part(&tipm, SH_GLOSS, 0);
        hE(200 + HSC(40), -420 - HSC(60), HSC(40), HSC(40));
        break; }
    case HORNS_DEVIL_TALL:
        horn_mat(c);
        hB(130, -210, 300 + HSC(60), -360 - HSC(60), 200 + HSC(30), -500 - HSC(200), HSC(58), HSC(10), 6);
        break;
    case HORNS_MANY: {
        static const int16_t hx5[3] = { 70, 190, 250 };
        static const int16_t hy5[3] = { -240, -200, -100 };
        int k;
        horn_mat(c);
        for (k = 0; k < 3; k++) {
            int x = hx5[k], y = hy5[k], s = HSC(70 - k * 10);
            hT(x - s / 2, y + 10, x + s / 2, y + 10, x + s / 3, y - s * 3 / 2);
        }
        break; }
    case HORNS_HALO_HORNS:
        horn_mat(c);
        hB(130, -210, 300 + HSC(40), -420, 0, -420 - HSC(60), HSC(46), HSC(22), 6);
        break;
    default:
        break;
    }
}

void ph_horns(PCtx *c)
{
    int st = c->l->horns;
    phs.c = c; phs.h = &c->h; phs.z = 0;
    if (st == HORNS_NONE) return;
    HS = 180 + c->l->horn_size * 10;
    for (phs.s = -1; phs.s <= 1; phs.s += 2) one_horn(c, st);
    phs.s = 1;
    if (st == HORNS_HALO_HORNS) {
        PMat g;
        pm_paint(&g, PAINT_LEMON);
        pc_part(&g, SH_GLOSS, 0);
        hE(0, -420 - HSC(60), 70, 70);
        pc_decal(&c->horn, SH_FLAT);
        hE(0, -420 - HSC(60), 34, 34);
    }
}

/* ======================================================================
 * hats
 * ==================================================================== */
static void brim(int y, int w, int h)
{
    hE(0, y, w, h);
}

void ph_hat(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->hat, k;
    PMat m = c->hat, d, w, g;
    int fv = FRONT_VIEW;
    phs.c = c; phs.h = &c->h; phs.s = 1; phs.z = 0;
    if (st == HAT_NONE) return;
    pm_from3(&d, m.c[0], m.c[1], m.c[2]);
    pm_paint(&w, PAINT_WHITE);
    pm_paint(&g, PAINT_LEMON);
    switch (st) {
    case HAT_CAP:
        pc_part(&m, SH_BALL, 0);
        clip_above(-120);
        hE(0, -150, 266, 170);
        pc_noclip();
        if (fv) {
            pc_part(&d, SH_HGRAD, 0);
            hFrame(0, -120, 300, 60);
            hQ(-250, -140, 250, -140, 300, -90, -300, -90);
        } else {
            pc_part(&d, SH_HGRAD, 0);
            hQ(-200, -130, 200, -130, 150, -100, -150, -100);
        }
        pc_part(&w, SH_BALL, 0);
        hE(0, -300, 30, 24);
        pc_decal(&m, SH_FLAT);
        hL(0, -300, 0, -140, m.c[1]);
        break;
    case HAT_BEANIE:
        pc_part(&m, SH_BALL, 0);
        clip_above(-80);
        hE(0, -120, 272, 220);
        pc_noclip();
        pc_part(&d, SH_BALL, 0);
        pc_rrect(hx(0), hy(-100), hr(276), hry(46), hr(30));
        pc_decal(&d, SH_FLAT);
        for (k = -5; k <= 5; k++) hL(k * 48, -130, k * 48, -70, d.c[0]);
        pc_part(&w, SH_SOFT, PF_FUZZ);
        hE(0, -350, 66, 62);
        break;
    case HAT_TOP_HAT:
        pc_part(&m, SH_BALL, 0);
        brim(-200, 300, 52);
        pc_part(&m, SH_BALL, 0);
        pc_rrect(hx(0), hy(-370), hr(180), hry(170), hr(30));
        {
            PMat band;
            pm_paint(&band, l->hat_color == PAINT_CHERRY ? PAINT_JET : PAINT_CHERRY);
            pc_part(&band, SH_BALL, 0);
            pc_rrect(hx(0), hy(-240), hr(182), hry(32), hr(8));
        }
        break;
    case HAT_WITCH:
        pc_part(&m, SH_BALL, 0);
        brim(-190, 380, 66);
        pc_part(&m, SH_BALL, 0);
        hFrame(40, -380, 200, 220);
        hT(-180, -210, 180, -210, 80, -470);
        hB(80, -470, 140, -560, 260, -560, 40, 16, 4);
        pc_part(&g, SH_BALL, 0);
        pc_rrect(hx(0), hy(-230), hr(180), hry(28), hr(8));
        pc_decal(&d, SH_FLAT);
        pc_rrect(hx(0), hy(-230), hr(30), hry(20), hr(4));
        break;
    case HAT_CROWN: {
        PMat gem;
        pm_paint(&gem, l->hat_color);
        pc_part(&g, SH_GLOSS, 0);
        pc_rrect(hx(0), hy(-240), hr(190), hry(44), hr(10));
        for (k = -2; k <= 2; k++) hT(k * 80 - 40, -270, k * 80 + 40, -270, k * 80, -380 + p_abs(k) * 20);
        pc_part(&gem, SH_GLOSS, 0);
        hE(0, -240, 34, 30);
        hE(-120, -240, 24, 22);
        hE(120, -240, 24, 22);
        for (k = -2; k <= 2; k++) hP(k * 80, -380 + p_abs(k) * 20, PAL_WHITE);
        break; }
    case HAT_PROPELLER: {
        int a = (c->anim * 3) & 7;
        pc_part(&m, SH_BALL, 0);
        clip_above(-110);
        hE(0, -130, 266, 200);
        pc_noclip();
        pc_mask_none(); pc_mask_add(pcv.part);
        pc_decal(&g, SH_BALL);
        hHeadFrame(0);
        hQ(-60, -330, 60, -330, 100, -110, -100, -110);
        pc_mask_none();
        pc_part(&g, SH_BALL, 0);
        hC(0, -330, 0, -380, 14, 14);
        pc_part(&m, SH_BALL, 0);
        hE((a & 1) ? 100 : 150, -390, (a & 1) ? 100 : 150, 24);
        hE((a & 1) ? -100 : -150, -390, (a & 1) ? 100 : 150, 24);
        break; }
    case HAT_COWBOY:
        pc_part(&m, SH_BALL, 0);
        hE(0, -200, 410, 70);
        hE(-330, -250, 80, 60);
        hE(330, -250, 80, 60);
        pc_part(&m, SH_BALL, 0);
        pc_rrect(hx(0), hy(-300), hr(190), hry(110), hr(80));
        pc_decal(&d, SH_FLAT);
        hL(-40, -390, 0, -350, d.c[0]);
        hL(0, -350, 40, -390, d.c[0]);
        pc_part(&d, SH_BALL, 0);
        pc_rrect(hx(0), hy(-225), hr(192), hry(24), hr(6));
        break;
    case HAT_BERET:
        pc_part(&m, SH_SOFT, 0);
        hFrame(60, -220, 300, 120);
        hE(60, -230, 280, 110);
        hE(-150, -170, 120, 60);
        pc_part(&d, SH_BALL, 0);
        hE(60, -350, 24, 30);
        break;
    case HAT_MARINE_HELMET:
        pc_part(&m, SH_GLOSS, 0);
        clip_above(-40);
        hE(0, -90, 296, 250);
        pc_noclip();
        pc_part(&d, SH_BALL, 0);
        pc_rrect(hx(0), hy(-50), hr(300), hry(30), hr(16));
        pc_decal(&d, SH_FLAT);
        hE(-120, -220, 30, 22);
        hE(90, -180, 40, 26);
        hE(10, -280, 24, 18);
        break;
    case HAT_HORNED_HELMET: {
        PMat bone;
        pm_paint(&bone, PAINT_BONE);
        pc_part(&bone, SH_BALL, 0);
        for (phs.s = -1; phs.s <= 1; phs.s += 2)
            hB(200, -170, 400, -190, 420, -380, 54, 14, 5);
        phs.s = 1;
        pc_part(&m, SH_GLOSS, 0);
        clip_above(-70);
        hE(0, -110, 280, 220);
        pc_noclip();
        pc_part(&g, SH_BALL, 0);
        pc_rrect(hx(0), hy(-80), hr(284), hry(30), hr(12));
        pc_rrect(hx(0), hy(-220), hr(28), hry(130), hr(8));
        for (k = -2; k <= 2; k++) hP(k * 110, -80, g.c[4]);
        break; }
    case HAT_BUNNY_EARS: {
        uint8_t pink = PAL_RAMP(RAMP_PASTEL_PINK, 5);
        pc_part(&m, SH_BALL, 0);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hC(90, -220, 150, -560, 64, 56);
            hE(150, -560, 58, 46);
        }
        phs.s = 1;
        pc_decal(&m, SH_FLAT);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            PMat p; pm_flat(&p, pink); pc_decal(&p, SH_FLAT);
            hC(98, -260, 148, -520, 26, 22);
        }
        phs.s = 1;
        pc_part(&d, SH_BALL, 0);
        clip_above(-150);
        hE(0, -60, 270, 200);
        pc_noclip();
        pc_clip(0, (hy(-230) + 8) >> 4, pcv.w, pcv.h);
        pc_noclip();
        break; }
    case HAT_CAT_EARS: {
        PMat p;
        pm_ramp(&p, RAMP_PASTEL_PINK, 5);
        pc_part(&m, SH_BALL, 0);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) hT(60, -230, 240, -150, 190, -420);
        phs.s = 1;
        pc_decal(&p, SH_FLAT);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) hT(110, -220, 200, -170, 180, -340);
        phs.s = 1;
        pc_part(&d, SH_BALL, 0);
        hC(-230, -120, -120, -228, 22, 22);
        hC(-120, -228, 120, -228, 22, 22);
        hC(120, -228, 230, -120, 22, 22);
        break; }
    case HAT_HEADPHONES: {
        PMat cup = m;
        pc_part(&d, SH_BALL, 0);
        hB(-250, -60, 0, -380, 250, -60, 30, 30, 8);
        pc_part(&cup, SH_GLOSS, 0);
        hE(-262, 20, 70, 100);
        hE(262, 20, 70, 100);
        pc_part(&w, SH_SOFT, 0);
        hE(-300, 20, 30, 70);
        hE(300, 20, 30, 70);
        break; }
    case HAT_BANDANA:
        pc_part(&m, SH_BALL, 0);
        clip_above(-90);
        hE(0, -60, 274, 250);
        pc_noclip();
        hT(230, -140, 380, -60, 330, 20);
        hT(240, -110, 380, -140, 350, -40);
        pc_mask_none(); pc_mask_add(pcv.part);
        pc_decal(&w, SH_FLAT);
        for (k = -4; k <= 4; k++) hP(k * 60, -150 - (k & 1) * 50, PAL_WHITE);
        pc_mask_none();
        break;
    case HAT_CHEF:
        pc_part(&w, SH_SOFT, 0);
        pc_rrect(hx(0), hy(-240), hr(200), hry(60), hr(14));
        pc_part(&w, SH_SOFT, 0);
        hFrame(0, -420, 260, 160);
        hE(-130, -400, 130, 120);
        hE(130, -400, 130, 120);
        hE(0, -470, 150, 130);
        pc_decal(&w, SH_FLAT);
        hL(-80, -300, -80, -200, w.c[1]);
        hL(80, -300, 80, -200, w.c[1]);
        pc_part(&m, SH_BALL, 0);
        pc_rrect(hx(0), hy(-205), hr(200), hry(22), hr(6));
        break;
    case HAT_PARTY: {
        PMat s2;
        pm_paint(&s2, PAINT_LEMON);
        pc_part(&m, SH_BALL, 0);
        hT(-120, -220, 120, -220, 40, -560);
        pc_mask_none(); pc_mask_add(pcv.part);
        pc_decal(&s2, SH_BALL);
        hHeadFrame(0);
        hQ(-110, -330, 110, -360, 110, -300, -110, -270);
        hQ(-40, -450, 80, -470, 80, -420, -40, -400);
        pc_mask_none();
        pc_part(&w, SH_SOFT, PF_FUZZ);
        hE(40, -570, 50, 48);
        break; }
    case HAT_NUN_VEIL: {
        PMat veil;
        pm_paint(&veil, PAINT_JET);
        if (l->hat_color != PAINT_WHITE) veil = m;
        pc_part(&veil, SH_BALL, 0);
        if (fv) {
            clip_above(-110);
            hE(0, -20, 300, 290);
            pc_noclip();
            hQ(-300, -120, -230, -120, -250, 380, -360, 380);
            hQ(300, -120, 230, -120, 250, 380, 360, 380);
        } else {
            hE(0, 30, 300, 290);
            hQ(-300, 0, 300, 0, 360, 380, -360, 380);
        }
        pc_part(&w, SH_SOFT, 0);
        clip_below(-180);
        clip_above(-100);
        pc_clip(0, (hy(-180) + 8) >> 4, pcv.w, (hy(-100) + 8) >> 4);
        hE(0, -20, 284, 270);
        pc_noclip();
        break; }
    case HAT_FEZ: {
        PMat t;
        pm_paint(&t, PAINT_LEMON);
        pc_part(&m, SH_BALL, 0);
        hQ(-150, -210, 150, -210, 110, -390, -110, -390);
        hE(0, -390, 110, 26);
        pc_part(&t, SH_BALL, 0);
        hC(0, -390, 120, -360, 10, 10);
        hC(120, -360, 140, -240, 10, 14);
        break; }
    case HAT_FLOWER_CROWN: {
        static const uint8_t fc[3] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_LAVENDER };
        PMat leaf, pet, ctr;
        pm_paint(&leaf, PAINT_MINT);
        pm_paint(&ctr, PAINT_LEMON);
        pc_part(&leaf, SH_BALL, 0);
        for (k = -3; k <= 3; k++) hE(k * 76 + 30, -214 + (k * k) * 5, 30, 18);
        for (k = -3; k <= 3; k++) {
            int x = k * 76, y = -230 + (k * k) * 6;
            pm_paint(&pet, (k & 1) ? fc[(k + 3) % 3] : l->hat_color);
            pc_part(&pet, SH_SOFT, 0);
            hE(x - 22, y, 26, 24); hE(x + 22, y, 26, 24);
            hE(x, y - 22, 26, 24); hE(x, y + 20, 26, 24);
            pc_decal(&ctr, SH_FLAT);
            hE(x, y, 14, 14);
        }
        break; }
    case HAT_TIARA: {
        PMat s, gem;
        pm_paint(&s, PAINT_SILVER);
        pm_paint(&gem, l->hat_color);
        pc_part(&s, SH_GLOSS, 0);
        hB(-200, -170, 0, -290, 200, -170, 20, 20, 6);
        hT(-50, -260, 50, -260, 0, -370);
        hT(-150, -220, -90, -240, -130, -300);
        hT(150, -220, 90, -240, 130, -300);
        pc_part(&gem, SH_GLOSS, 0);
        hE(0, -290, 36, 40);
        hP(-14, -300, PAL_WHITE);
        break; }
    case HAT_SKULL_CAP: {
        PMat bone;
        pc_part(&m, SH_BALL, 0);
        clip_above(-130);
        hE(0, -120, 262, 200);
        pc_noclip();
        pm_paint(&bone, PAINT_BONE);
        pc_part(&bone, SH_SOFT, PF_NOCAST);
        hE(0, -230, 44, 40);
        pc_decal(&d, SH_FLAT);
        hP(-16, -230, PAL_PAINT(PAINT_JET, 1));
        hP(16, -230, PAL_PAINT(PAINT_JET, 1));
        break; }
    case HAT_TRAFFIC_CONE: {
        PMat o, s;
        pm_paint(&o, PAINT_TANGERINE);
        pm_paint(&s, l->hat_color == PAINT_TANGERINE ? PAINT_WHITE : l->hat_color);
        pc_part(&o, SH_BALL, 0);
        pc_rrect(hx(0), hy(-210), hr(220), hry(30), hr(8));
        pc_part(&o, SH_BALL, 0);
        hFrame(0, -400, 160, 220);
        hT(-150, -230, 150, -230, 0, -620);
        pc_mask_none(); pc_mask_add(pcv.part);
        pc_decal(&s, SH_BALL);
        hFrame(0, -400, 160, 220);
        hQ(-130, -300, 130, -300, 110, -350, -110, -350);
        hQ(-80, -430, 80, -430, 60, -480, -60, -480);
        pc_mask_none();
        break; }
    case HAT_BUCKET:
        pc_part(&m, SH_BALL, 0);
        hQ(-310, -130, 310, -130, 220, -200, -220, -200);
        hE(0, -130, 312, 40);
        pc_part(&m, SH_BALL, 0);
        clip_above(-180);
        hE(0, -200, 230, 170);
        pc_noclip();
        pc_decal(&d, SH_FLAT);
        hL(-220, -200, 220, -200, d.c[0]);
        break;
    default:
        break;
    }
    pc_mask_none();
}

/* ======================================================================
 * face accessories
 * ==================================================================== */
static void eye_pos(PCtx *c, int s, int *X, int *Y)
{
    ph_eye_geom(c, c->l->eye_count == 1 ? 0 : s, X, Y, NULL, NULL);
}

static int eye_r(PCtx *c)
{
    int x, y, r;
    ph_eye_geom(c, 1, &x, &y, NULL, &r);
    return r;
}

void ph_face_acc(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->accessory, X, Y, X2, Y2, r, k;
    PMat m = c->acc, dk, w;
    phs.c = c; phs.h = &c->h; phs.s = 1; phs.z = 0;
    if (BACK_VIEW && st != ACC_EYEPATCH && st != ACC_BANDAGES && st != ACC_EARRINGS) return;
    r = eye_r(c);
    pm_flat(&dk, PAL_PAINT(PAINT_JET, 1));
    pm_paint(&w, PAINT_WHITE);
    switch (st) {
    case ACC_GLASSES: {
        int s;
        for (s = -1; s <= 1; s += 2) {
            int x, y, rr = r + 30;
            eye_pos(c, s, &X, &Y);
            for (y = -(rr >> 4) - 1; y <= (rr >> 4) + 1; y++)
                for (x = -(rr >> 4) - 1; x <= (rr >> 4) + 1; x++) {
                    int d2 = (x * 16) * (x * 16) + (y * 16) * (y * 16);
                    if (d2 <= (rr + 8) * (rr + 8) && d2 > (rr - 8) * (rr - 8))
                        pc_px((X >> 4) + x, (Y >> 4) + y, m.c[1]);
                }
            pc_px((X >> 4) - (rr >> 5), (Y >> 4) - (rr >> 5), PAL_WHITE);
        }
        eye_pos(c, -1, &X, &Y); eye_pos(c, 1, &X2, &Y2);
        pc_line((X + r + 30) >> 4, (Y >> 4) - 1, (X2 - r - 30) >> 4, (Y2 >> 4) - 1, m.c[1]);
        break; }
    case ACC_SUNGLASSES: {
        int s;
        for (s = -1; s <= 1; s += 2) {
            eye_pos(c, s, &X, &Y);
            pc_part(&dk, SH_FLAT, 0);
            pc_rrect(X, Y, r + 34, r * 3 / 4 + 20, 24);
            pc_px((X >> 4) - 1, (Y >> 4) - 1, PAL_GRAY);
            pc_px((X >> 4) - 2, (Y >> 4), PAL_GRAY_D);
        }
        eye_pos(c, -1, &X, &Y); eye_pos(c, 1, &X2, &Y2);
        pc_line(X >> 4, (Y - r / 2) >> 4, X2 >> 4, (Y2 - r / 2) >> 4, m.c[0]);
        pc_line((X - r - 40) >> 4, (Y - r / 2) >> 4, (X - r - 60) >> 4, (Y - r / 2 - 10) >> 4, m.c[0]);
        break; }
    case ACC_MONOCLE: {
        int x, y, rr = r + 32;
        eye_pos(c, 1, &X, &Y);
        for (y = -(rr >> 4) - 1; y <= (rr >> 4) + 1; y++)
            for (x = -(rr >> 4) - 1; x <= (rr >> 4) + 1; x++) {
                int d2 = (x * 16) * (x * 16) + (y * 16) * (y * 16);
                if (d2 <= (rr + 8) * (rr + 8) && d2 > (rr - 10) * (rr - 10))
                    pc_px((X >> 4) + x, (Y >> 4) + y, PAL_PAINT(PAINT_HONEY, 1));
            }
        pc_px((X >> 4) - 1, (Y >> 4) - 2, PAL_WHITE);
        for (k = 0; k < 8; k++) pc_px((X >> 4) + (rr >> 4) + (k >> 2), (Y >> 4) + 1 + k, (k & 1) ? PAL_PAINT(PAINT_HONEY, 2) : PAL_PAINT(PAINT_HONEY, 0));
        break; }
    case ACC_EYEPATCH:
        if (FRONT_VIEW) {
            eye_pos(c, -1, &X, &Y);
            pc_part(&dk, SH_BALL, 0);
            pc_ellipse(X, Y, r + 30, r + 24);
        }
        pc_decal(&dk, SH_FLAT);
        hL(-280, -160, 280, -20, PAL_PAINT(PAINT_JET, 1));
        break;
    case ACC_MASK:
        pc_part(&m, SH_SOFT, 0);
        hFrame(0, 120, 240, 160);
        pc_rrect(hx(0), hy(130), hr(190), hry(100), hr(60));
        pc_decal(&m, SH_FLAT);
        hL(-150, 100, 150, 100, m.c[1]);
        hL(-150, 150, 150, 150, m.c[1]);
        hL(-190, 90, -260, 30, m.c[1]);
        hL(190, 90, 260, 30, m.c[1]);
        break;
    case ACC_BANDAGES:
        pc_part(&w, SH_SOFT, PF_NOCAST);
        hHeadFrame(10);
        hQ(-270, -170, 270, -110, 270, -60, -270, -120);
        pc_decal(&w, SH_FLAT);
        hL(-200, -140, 200, -100, w.c[1]);
        if (FRONT_VIEW) {
            PMat pl;
            pm_paint(&pl, PAINT_PEACH);
            pc_part(&pl, SH_SOFT, PF_NOCAST);
            hQ(80, 30, 170, 0, 190, 40, 100, 70);
        }
        break;
    case ACC_EARRINGS: {
        PMat gm;
        pm_paint(&gm, l->accessory_color);
        pc_part(&gm, SH_GLOSS, 0);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hE(258, 140, 30, 30);
            hE(258, 210, 22, 36);
        }
        phs.s = 1;
        break; }
    case ACC_NOSE_RING: {
        int ny = (-14 + l->eye_height * 6 + 120) / 2 + 12;
        if (species_has_muzzle(l->species)) ny = 50;
        ph_proj(phs.h, 20, ny + 20, 1, &X, &Y);
        pc_px(X >> 4, Y >> 4, m.c[2]);
        pc_px((X >> 4) + 1, (Y >> 4) + 1, m.c[3]);
        pc_px(X >> 4, (Y >> 4) + 2, m.c[2]);
        pc_px((X >> 4) - 1, (Y >> 4) + 1, m.c[1]);
        break; }
    case ACC_PACIFIER:
        ph_proj(phs.h, 0, 136, 1, &X, &Y);
        pc_part(&m, SH_GLOSS, 0);
        pc_rrect(X, Y, PX(3), PX(2), 16);
        pc_part(&w, SH_BALL, 0);
        pc_ellipse(X, Y + PX(1), PX(1) + 6, PX(1) + 6);
        pc_decal(&m, SH_FLAT);
        pc_px(X >> 4, (Y >> 4) + 1, m.c[1]);
        break;
    case ACC_GAS_MASK: {
        PMat gm, lens;
        pm_ramp(&gm, RAMP_STEEL, 3);
        pm_paint(&lens, l->accessory_color);
        pc_part(&gm, SH_BALL, 0);
        hFrame(0, 60, 260, 220);
        pc_rrect(hx(0), hy(80), hr(220), hry(170), hr(110));
        pc_part(&lens, SH_GLOSS, 0);
        eye_pos(c, -1, &X, &Y);
        pc_ellipse(X, Y, r + 28, r + 28);
        eye_pos(c, 1, &X, &Y);
        pc_ellipse(X, Y, r + 28, r + 28);
        pc_part(&gm, SH_BALL, 0);
        hE(0, 220, 90, 80);
        pc_decal(&gm, SH_FLAT);
        for (k = -1; k <= 1; k++) hL(k * 30, 190, k * 30, 250, gm.c[0]);
        break; }
    case ACC_NONE:
    default:
        break;
    }
    pc_mask_none();
}

int species_has_muzzle(int sp)
{
    switch (sp) {
    case SPECIES_CATFOLK: case SPECIES_MOUSEKIN: case SPECIES_BUNNYKIN:
    case SPECIES_BEARKIN: case SPECIES_PUPKIN: case SPECIES_SHEEPKIN:
    case SPECIES_FOXKIN: case SPECIES_RACCOONKIN: case SPECIES_HELLHOUND:
        return 1;
    default:
        return 0;
    }
}

/* ======================================================================
 * halo
 * ==================================================================== */
int ph_top_extent(const DoomoLook *l)
{
    int top = 256;
    switch (l->ears) {
    case EARS_BUNNY: top = 680; break;
    case EARS_FOX: top = 480; break;
    case EARS_BAT: top = 460; break;
    case EARS_CAT: case EARS_RACCOON: case EARS_OWL_TUFTS: case EARS_DOG: top = 420; break;
    case EARS_MOUSE: top = 360; break;
    default: break;
    }
    switch (l->hat) {
    case HAT_TOP_HAT: top = DOOMO_MAX(top, 540); break;
    case HAT_WITCH: top = DOOMO_MAX(top, 580); break;
    case HAT_CHEF: top = DOOMO_MAX(top, 600); break;
    case HAT_PARTY: case HAT_TRAFFIC_CONE: top = DOOMO_MAX(top, 620); break;
    case HAT_BUNNY_EARS: top = DOOMO_MAX(top, 620); break;
    default: if (l->hat) top = DOOMO_MAX(top, 400); break;
    }
    if (l->horns == HORNS_DEVIL_TALL || l->horns == HORNS_UNICORN || l->horns == HORNS_ANTLERS)
        top = DOOMO_MAX(top, 560 + l->horn_size * 8);
    else if (l->horns) top = DOOMO_MAX(top, 420 + l->horn_size * 6);
    if (l->hair_style == HAIR_LIBERTY_SPIKES) top = DOOMO_MAX(top, 520);
    if (l->hair_style == HAIR_AFRO || l->hair_style == HAIR_SPIKY || l->hair_style == HAIR_WILD)
        top = DOOMO_MAX(top, 420);
    return top;
}

void ph_halo(PCtx *c)
{
    int st = c->l->halo, k;
    int top = ph_top_extent(c->l);
    int y = -(top > 420 ? 420 : top) - 70;
    PMat g;
    phs.c = c; phs.h = &c->h; phs.s = 1; phs.z = 0;
    if (st == HALO_NONE) return;
    y += (c->breath ? 10 : 0);
    pm_paint(&g, PAINT_LEMON);
    switch (st) {
    case HALO_HALO: {
        int x, yy, rx = hr(170), ry = hry(48);
        int cx = hx(0), cy = hy(y);
        for (yy = -(ry >> 4) - 2; yy <= (ry >> 4) + 2; yy++)
            for (x = -(rx >> 4) - 2; x <= (rx >> 4) + 2; x++) {
                int u = (x * 16 * 256) / DOOMO_MAX(rx, 1), v = (yy * 16 * 256) / DOOMO_MAX(ry, 1);
                int d = u * u + v * v;
                if (d <= 65536 + 34000 && d >= 65536 - 30000)
                    pc_fx((cx >> 4) + x, (cy >> 4) + yy, yy < 0 ? g.c[4] : (yy == 0 ? g.c[3] : g.c[2]));
            }
        pc_fx((cx >> 4) - (rx >> 4) + 1, (cy >> 4) - 1, PAL_WHITE);
        break; }
    case HALO_BROKEN: {
        int x, yy, rx = hr(170), ry = hry(48);
        int cx = hx(0), cy = hy(y + 20);
        for (yy = -(ry >> 4) - 2; yy <= (ry >> 4) + 2; yy++)
            for (x = -(rx >> 4) - 2; x <= (rx >> 4) + 2; x++) {
                int u = (x * 16 * 256) / DOOMO_MAX(rx, 1), v = ((yy - x / 4) * 16 * 256) / DOOMO_MAX(ry, 1);
                int d = u * u + v * v;
                if (x > 2 && x < 5) continue;
                if (d <= 65536 + 34000 && d >= 65536 - 30000)
                    pc_fx((cx >> 4) + x, (cy >> 4) + yy, x > 4 ? PAL_GRAY : (yy <= 0 ? g.c[3] : g.c[1]));
            }
        break; }
    case HALO_FLAME_RING:
        for (k = 0; k < 14; k++) {
            int a = k * 256 / 14 + c->anim * 2;
            int x = hx(p_cos(a) * 180 / 256) >> 4, yy = hy(y + p_sin(a) * 50 / 256) >> 4;
            int fl = ((k + (c->anim >> 2)) & 1);
            pc_fx(x, yy, PAL_RAMP(RAMP_LAVA, 5));
            pc_fx(x, yy - 1, PAL_RAMP(RAMP_LAVA, 6 + fl));
            if (fl) pc_fx(x, yy - 2, PAL_RAMP(RAMP_LAVA, 4));
        }
        break;
    case HALO_STARS:
        for (k = 0; k < 5; k++) {
            int a = k * 51 + c->anim * 3;
            int x = hx(p_cos(a) * 200 / 256) >> 4, yy = hy(y + p_sin(a) * 60 / 256) >> 4;
            ph_star(x, yy, 1, (k & 1) ? PAL_UI_GOLD : PAL_WHITE);
            pcv.id[DOOMO_CLAMP(yy, 0, pcv.h - 1) * pcv.w + DOOMO_CLAMP(x, 0, pcv.w - 1)] = PID_FX;
        }
        break;
    case HALO_THORNS:
        for (k = 0; k < 18; k++) {
            int a = k * 256 / 18;
            int x = hx(p_cos(a) * 180 / 256) >> 4, yy = hy(y + p_sin(a) * 50 / 256) >> 4;
            pc_fx(x, yy, PAL_RAMP(RAMP_WOOD, 1));
            if (k % 3 == 0) pc_fx(x, yy - 1, PAL_RAMP(RAMP_WOOD, 2));
            if (k % 3 == 1) pc_fx(x + 1, yy + 1, PAL_RAMP(RAMP_BLOOD, 4));
        }
        break;
    default:
        break;
    }
}
