/* portrait_body.c - plush bodies, outfits, tails, wings and body
 * accessories (owner: portrait). Coordinates are Q4 (1/16 px). */
#include "portrait_int.h"
#include "doomo/resident.h"

typedef struct PBody {
    int view, yaw, lod;
    int cx, ty, by;         /* torso centre x, top y, crotch y          */
    int sw, hw;             /* shoulder / hip half width                */
    int waist;              /* waist line y                             */
    int ground;
    int leg_r, arm_r, paw_r;
    int shx[2], shy[2];     /* shoulders  (0 = viewer's left)           */
    int hdx[2], hdy[2];     /* hands                                    */
    int hpx[2], hpy[2];     /* hips                                     */
    int ftx[2], fty[2];     /* ankles                                   */
    int unit;               /* Q4 per 256 units for tails / wings       */
    int tailx, taily, tails;/* tail anchor + side (+1 right, -1 left)   */
    int wingx, wingy;       /* right wing anchor (mirrored for left)    */
    int neckx, necky;
    int neck;               /* chin line y (set once the head is placed) */
    int nolegs;             /* slime / ghost / cacoblob                 */
    int belly;              /* belly bulge (Q4)                         */
    int hunch;
} PBody;

static PBody B;

/* body-type modifiers: shoulder%, hip%, torso height%, legs%, arm%, belly */
static const int16_t body_mod[BODY_COUNT][6] = {
    /* AVERAGE   */ { 100, 100, 100, 100, 100, 0 },
    /* ROUND     */ { 112, 124, 96, 80, 100, 40 },
    /* LANKY     */ { 84, 82, 118, 135, 86, 0 },
    /* STOCKY    */ { 124, 118, 88, 74, 118, 10 },
    /* TINY      */ { 80, 80, 74, 70, 84, 10 },
    /* HUNCHED   */ { 104, 96, 90, 92, 100, 0 },
    /* BUFF      */ { 140, 98, 106, 96, 150, 0 },
    /* NOODLE    */ { 70, 70, 120, 140, 66, 0 },
    /* PEAR      */ { 84, 136, 100, 90, 92, 20 },
    /* TOP_HEAVY */ { 142, 78, 104, 92, 120, 0 },
};

static int mag_or1(int m) { return m > 0 ? m : 1; }

static int species_nolegs(int sp)
{
    return sp == SPECIES_SLIME || sp == SPECIES_GHOST || sp == SPECIES_CACOBLOB;
}

/* ---------------------------------------------------------------------- */
void pb_layout(PCtx *c, int vk, int byaw, int step, int anim)
{
    const DoomoLook *l = c->l;
    const int16_t *bm = body_mod[l->body < BODY_COUNT ? l->body : 0];
    int build = l->build, height = l->height;
    int H, legs, s;
    (void)anim;
    memset(&B, 0, sizeof(B));
    B.view = vk;
    B.yaw = byaw;
    B.lod = vk == VK_CHIBI ? LOD_TINY : (vk == VK_FIGHTER ? LOD_MID : LOD_BIG);
    B.nolegs = species_nolegs(l->species) || c->ghost;
    B.hunch = l->body == BODY_HUNCHED;
    c->view_kind = vk;
    c->byaw = byaw;
    c->step = step;
    switch (vk) {
    case VK_PORTRAIT:
        B.cx = PX(24);
        B.sw = PX(10) + build * 9 + (bm[0] - 100) * 3;
        B.hw = B.sw;
        B.ty = PX(41) + (B.hunch ? PX(2) : 0) - (height - 7) * 3 + (c->breath ? -8 : 0);
        B.by = PX(70);
        B.waist = PX(66);
        B.ground = PX(80);
        B.arm_r = 44 + build * 2 + (bm[4] - 100) / 3;
        B.paw_r = 42 + build;
        B.unit = PX(20);
        break;
    case VK_FIGURE:
    default:
        legs = (PX(13) + height * 6) * bm[3] / 100;
        H = (PX(12) + height * 4) * bm[2] / 100;
        B.ground = PX(77);
        B.sw = (PX(8) + build * 4) * bm[0] / 100;
        B.hw = (PX(8) + build * 4) * bm[1] / 100;
        B.leg_r = 40 + build * 2;
        B.arm_r = (36 + build * 2) * bm[4] / 100;
        B.paw_r = 40 + build * 2;
        B.unit = PX(18);
        B.cx = PX(24);
        goto common;
    case VK_FIGHTER:
        legs = (PX(4) + height * 2) * bm[3] / 100;
        H = (PX(9) + height * 3) * bm[2] / 100;
        B.ground = PX(47);
        B.sw = (PX(6) + build * 2) * bm[0] / 100;
        B.hw = (PX(6) + build * 2) * bm[1] / 100;
        B.leg_r = 30 + build;
        B.arm_r = (28 + build) * bm[4] / 100;
        B.paw_r = 30 + build;
        B.unit = PX(11);
        B.cx = PX(16);
        goto common;
    case VK_CHIBI:
        legs = (PX(2) + height * 1) * bm[3] / 100;
        H = (PX(5) + height * 1) * bm[2] / 100;
        B.ground = PX(23);
        B.sw = (PX(4) + build) * bm[0] / 100;
        B.hw = (PX(4) + build) * bm[1] / 100;
        B.leg_r = 18 + build / 2;
        B.arm_r = (16 + build / 2) * bm[4] / 100;
        B.paw_r = 18;
        B.unit = PX(6);
        B.cx = PX(8);
    common:
        if (B.nolegs) legs = legs / 3;
        if (!B.nolegs && (l->shoes == SHOES_PLATFORM || l->shoes == SHOES_ROLLERSKATES))
            B.ground -= vk == VK_CHIBI ? 8 : (vk == VK_FIGHTER ? PX(1) : PX(2));
        B.by = B.ground - legs - (vk == VK_CHIBI ? 8 : 16);
        if ((step == 2 || step == -2) && vk != VK_PORTRAIT) B.by -= 16;
        else if (step && vk != VK_PORTRAIT) B.by -= (vk == VK_CHIBI ? 0 : 8);
        B.ty = B.by - H + (c->breath ? 8 : 0);
        if (B.hunch) B.ty += H / 8;
        B.waist = B.by - PCT(H, 30);
        break;
    }
    if (byaw == 2) { B.sw = PCT(B.sw, 80); B.hw = PCT(B.hw, 80); }
    if (l->species == SPECIES_CACOBLOB && vk != VK_PORTRAIT) {
        /* a tiny floating body under the big ball of a head */
        B.sw = PCT(B.sw, 62); B.hw = PCT(B.hw, 62);
        B.ty += PCT((B.by - B.ty), 30);
        B.arm_r = PCT(B.arm_r, 70);
        B.paw_r = PCT(B.paw_r, 75);
    }
    B.belly = bm[5] * B.sw / 100;
    /* anchors */
    for (s = 0; s < 2; s++) {
        int sg = s ? 1 : -1;
        int shoulder_in = vk == VK_CHIBI ? 8 : PX(2);
        B.shx[s] = B.cx + sg * (B.sw - shoulder_in);
        B.shy[s] = B.ty + (vk == VK_CHIBI ? 16 : PX(3));
        B.hpx[s] = B.cx + sg * (PCT(B.hw, 45));
        B.hpy[s] = B.by - (vk == VK_CHIBI ? 4 : 8);
        B.ftx[s] = B.hpx[s];
        B.fty[s] = B.ground - (vk == VK_CHIBI ? 8 : PCT((B.leg_r + 12), 128) - 2);
        if (vk == VK_PORTRAIT) {
            B.hdx[s] = B.cx + sg * PX(8);
            B.hdy[s] = PX(55);
        } else {
            B.hdx[s] = B.cx + sg * (B.sw + (vk == VK_CHIBI ? 4 : 12));
            B.hdy[s] = B.ty + PCT((B.by - B.ty), 78);
        }
    }
    if (byaw == 2) {
        /* profile: arms and legs swing along x, near limbs on the left.
         * |step| 1 = stride, 2 = passing (one foot tucked up) */
        int stride = (vk == VK_CHIBI ? 24 : (vk == VK_FIGHTER ? 40 : 56));
        int dir = step < 0 ? -1 : (step > 0 ? 1 : 0), mag = step < 0 ? -step : step;
        B.shx[0] = B.shx[1] = B.cx + (vk == VK_CHIBI ? 0 : 8);
        B.hpx[0] = B.hpx[1] = B.cx;
        if (mag == 2) {
            B.ftx[0] = B.cx - dir * stride / 3;
            B.ftx[1] = B.cx + dir * stride / 3;
            B.fty[dir < 0 ? 1 : 0] -= vk == VK_CHIBI ? 16 : 40;
        } else {
            B.ftx[0] = B.cx - dir * stride;
            B.ftx[1] = B.cx + dir * stride;
        }
        B.hdx[0] = B.cx + dir * stride / mag_or1(mag);
        B.hdx[1] = B.cx - dir * stride / mag_or1(mag);
        B.hdy[0] = B.hdy[1] = B.ty + PCT((B.by - B.ty), 72);
    } else if (step && vk != VK_PORTRAIT) {
        /* front / back walk: alternate lifting a foot, swing arms */
        int mag = step < 0 ? -step : step;
        int lift = (vk == VK_CHIBI ? 16 : 24) * mag;
        B.fty[step < 0 ? 0 : 1] -= lift;
        B.hdy[step < 0 ? 1 : 0] -= lift / 2;
    }
    if (B.hunch && vk != VK_PORTRAIT) {
        B.hdy[0] += PX(1); B.hdy[1] += PX(1);
    }
    B.tails = 1;
    B.tailx = B.cx + PCT(B.hw, 60);
    B.taily = B.by - (vk == VK_CHIBI ? 16 : PX(3));
    if (vk == VK_PORTRAIT) { B.tailx = PX(35); B.taily = PX(52); B.unit = PX(14); }
    if (vk == VK_FIGURE) B.unit = PX(15);
    if (byaw == 2) { B.tailx = B.cx + PCT(B.hw, 80); }
    B.wingx = B.cx + PCT(B.sw, 45);
    B.wingy = B.ty + (vk == VK_CHIBI ? 16 : (vk == VK_PORTRAIT ? -PX(1) : PX(3)));
    B.neckx = B.cx;
    B.necky = B.ty;
    c->bx = B.cx; c->by = (B.ty + B.by) / 2;
    c->bw = B.sw; c->bh = (B.by - B.ty) / 2;
    c->ground = B.ground;
    c->legs = B.ground - B.by;
}

int pb_head_y(PCtx *c, int ry)
{
    (void)c;
    return B.ty - PCT(ry, 62) + (B.hunch ? PX(2) : 0);
}

int pb_torso_top(void) { return B.ty; }
void pb_set_neck(int y) { B.neck = y; }

/* the part of the torso that is not hidden behind the head */
static void vis_area(int *ty, int *H)
{
    int t = DOOMO_MAX(B.ty, B.neck - 8);
    *ty = t;
    *H = B.view == VK_PORTRAIT ? PX(15) : DOOMO_MAX(B.by - t, 16);
}

void pb_shift(int dy)
{
    int s;
    B.ty += dy; B.by += dy; B.waist += dy; B.ground += dy;
    for (s = 0; s < 2; s++) {
        B.shy[s] += dy; B.hdy[s] += dy / 2; B.hpy[s] += dy; B.fty[s] += dy;
    }
    B.wingy += dy; B.necky += dy;
}
int pb_ground(void) { return B.ground; }
int pb_nolegs(void) { return B.nolegs; }
void pb_hand(int s, int *x, int *y) { *x = B.hdx[s]; *y = B.hdy[s]; }
void pb_set_hand(int s, int x, int y) { B.hdx[s] = x; B.hdy[s] = y; }

/* ---------------------------------------------------------------------- */
/* outfit classification */
static int top_is_long(int t)
{
    switch (t) {
    case TOP_DRESS: case TOP_ROBE: case TOP_KIMONO: case TOP_LAB_COAT:
    case TOP_CLOAK: case TOP_PRIEST: case TOP_MAID: case TOP_HAZMAT:
    case TOP_SPACESUIT: case TOP_OVERALLS: case TOP_PAJAMAS:
        return 1;
    default:
        return 0;
    }
}

/* 0 none, 1 short sleeve, 2 long sleeve, 3 wide sleeve */
static int top_sleeves(int t)
{
    switch (t) {
    case TOP_NONE: case TOP_TANK: case TOP_CROP_TOP: case TOP_OVERALLS:
        return 0;
    case TOP_TSHIRT: case TOP_BAND_TEE: case TOP_NURSE: case TOP_DRESS:
    case TOP_MAID: case TOP_SWEATER_VEST:
        return 1;
    case TOP_ROBE: case TOP_KIMONO: case TOP_CLOAK: case TOP_PRIEST:
        return 3;
    default:
        return 2;
    }
}

static void top_mat(PCtx *c, PMat *m)
{
    *m = c->top;
    switch (c->l->top) {
    case TOP_LAB_COAT: pm_paint(m, PAINT_WHITE); break;
    case TOP_PLATE_ARMOR: pm_ramp(m, RAMP_STEEL, 5); break;
    case TOP_SPACESUIT: pm_paint(m, PAINT_WHITE); break;
    case TOP_NURSE: pm_paint(m, c->l->top_color == PAINT_CHERRY ? PAINT_WHITE : PAINT_WHITE); break;
    default: break;
    }
}

static int top_shade(int t)
{
    switch (t) {
    case TOP_ARMOR: case TOP_PLATE_ARMOR: case TOP_LEATHER_JACKET: case TOP_SPACESUIT:
        return SH_GLOSS;
    case TOP_SWEATER: case TOP_HOODIE: case TOP_PAJAMAS: case TOP_DRESS: case TOP_MAID:
        return SH_SOFT;
    default:
        return SH_BALL;
    }
}

static int sleeve_mat(PCtx *c, PMat *m)
{
    /* what colour the upper arm is: top colour, a white shirt, or skin */
    int t = c->l->top;
    if (t == TOP_SWEATER_VEST || t == TOP_OVERALLS) { pm_paint(m, PAINT_WHITE); return 1; }
    if (t == TOP_LAB_COAT || t == TOP_SPACESUIT) { pm_paint(m, PAINT_WHITE); return 2; }
    if (t == TOP_PLATE_ARMOR) { pm_ramp(m, RAMP_STEEL, 5); return 2; }
    top_mat(c, m);
    return top_sleeves(t);
}

/* ---------------------------------------------------------------------- */
/* legs and feet */

/* shoes / paws: x,y = ankle; the sole sits on B.ground (minus any lift) */
static void foot(PCtx *c, int s, int x, int y)
{
    const DoomoLook *l = c->l;
    int sh = l->shoes, r = B.leg_r + 12, k;
    int side = B.yaw == 2 ? -1 : 0;     /* profile: toes point left */
    int fw = PCT(r, 115) + (side ? r / 2 : 0), fx = x + side * r / 3;
    int fh = PCT(r, 64), fy = y + fh - 6;  /* foot ellipse centre (below the ankle) */
    int sole = fy + fh;                 /* bottom of the foot */
    int big = B.lod == LOD_BIG;
    PMat m = c->shoe, sl, w, dk;
    pm_paint(&w, PAINT_WHITE);
    pm_from3(&sl, PAL_PAINT(PAINT_WHITE, 0), PAL_PAINT(PAINT_WHITE, 1), PAL_PAINT(PAINT_WHITE, 2));
    if (l->shoes_color == PAINT_WHITE) pm_from3(&sl, PAL_PAINT(PAINT_SILVER, 0), PAL_PAINT(PAINT_SILVER, 1), PAL_PAINT(PAINT_SILVER, 2));
    pm_from3(&dk, m.c[0], m.c[1], m.c[2]);
    if (B.lod == LOD_TINY) {
        PMat p = c->skin;
        if (l->marking == MARK_FUR_SOCKS) pm_paint(&p, l->marking_color);
        pc_part(sh == SHOES_BARE || sh == SHOES_CLAWS || sh == SHOES_SANDALS ? &p : &m, SH_BALL, 0);
        pc_ellipse(fx, y + 4, fw, r * 3 / 4);
        switch (sh) {
        case SHOES_ROLLERSKATES: pc_px(fx >> 4, (y >> 4) + 2, PAL_PAINT(PAINT_LEMON, 1)); break;
        case SHOES_SLIPPERS: pc_px((fx >> 4), (y - r) >> 4, m.c[3]); break;
        case SHOES_BOOTS: case SHOES_COMBAT: pc_part(&m, SH_BALL, 0); pc_capsule(x, y - 24, x, y, r - 4, r - 4); break;
        case SHOES_PLATFORM: case SHOES_SNEAKERS: pc_px(fx >> 4, (y + r * 3 / 4) >> 4, PAL_WHITE); break;
        case SHOES_CLAWS: pc_px(fx >> 4, (y + r) >> 4, PAL_PAINT(PAINT_BONE, 2)); break;
        case SHOES_SANDALS: pc_px(fx >> 4, (y + r / 2) >> 4, m.c[1]); break;
        case SHOES_HOOVES: pc_px(fx >> 4, (y + r / 2) >> 4, PAL_PAINT(PAINT_JET, 1)); break;
        default: break;
        }
        return;
    }
    switch (sh) {
    case SHOES_BARE:
    default: {
        PMat p = c->skin;
        if (l->marking == MARK_FUR_SOCKS) pm_paint(&p, l->marking_color);
        pc_part(&p, SH_BALL, c->critter ? PF_FUZZ : 0);
        pc_ellipse(fx, fy, fw, fh + 4);
        if (c->critter && big) {
            for (k = -1; k <= 1; k += 2) pc_px((fx >> 4) + k, (sole - 16) >> 4, p.c[0]);
        }
        break; }
    case SHOES_SNEAKERS:
        pc_part(&m, SH_BALL, 0);
        pc_ellipse(fx, fy - 4, fw, fh);
        pc_part(&sl, SH_FLAT, 0);
        pc_rrect(fx, sole - 12, fw + 4, 14, 10);
        pc_decal(&w, SH_SOFT);
        pc_ellipse(fx + side * r / 2, sole - 26, PCT(fw, 55), 16);
        if (big) {
            pc_px((x >> 4) - 1, (fy - fh / 2) >> 4, PAL_WHITE);
            pc_px((x >> 4) + 1, (fy - fh / 2) >> 4, PAL_WHITE);
        }
        break;
    case SHOES_BOOTS:
        pc_part(&m, SH_BALL, 0);
        pc_capsule(x, y - PX(5), x, fy, r - 4, r);
        pc_ellipse(fx, fy, fw, fh);
        pc_part(&m, SH_SOFT, 0);
        pc_rrect(x, y - PX(5), r + 8, 20, 10);
        pc_decal(&dk, SH_FLAT);
        pc_line((fx - fw + 8) >> 4, (sole - 8) >> 4, (fx + fw - 8) >> 4, (sole - 8) >> 4, m.c[0]);
        break;
    case SHOES_SANDALS: {
        PMat p = c->skin;
        if (l->marking == MARK_FUR_SOCKS) pm_paint(&p, l->marking_color);
        pc_part(&p, SH_BALL, c->critter ? PF_FUZZ : 0);
        pc_ellipse(fx, fy - 4, fw - 8, fh);
        pc_part(&m, SH_FLAT, 0);
        pc_rrect(fx, sole - 8, fw + 4, 12, 8);
        pc_decal(&m, SH_FLAT);
        pc_line((fx - fw + 12) >> 4, (fy - 8) >> 4, (fx + fw - 12) >> 4, (fy - 8) >> 4, m.c[1]);
        if (big) pc_line((fx - fw + 16) >> 4, (fy + 12) >> 4, (fx + fw - 16) >> 4, (fy + 12) >> 4, m.c[1]);
        break; }
    case SHOES_HEELS:
        pc_part(&m, SH_GLOSS, 0);
        pc_ellipse(fx - side * 8, fy - 8, fw - 12, fh - 8);
        pc_tri(fx - fw + 12, fy, fx + fw - 12, fy, fx, sole + 4);
        pc_part(&dk, SH_BALL, 0);
        pc_capsule(x + (side ? r / 2 : fw / 2), fy, x + (side ? r / 2 : fw / 2), sole, 8, 6);
        if (!side) pc_capsule(x - fw / 2, fy, x - fw / 2, sole, 8, 6);
        break;
    case SHOES_CLOGS: {
        PMat wood;
        pm_paint(&wood, PAINT_CARAMEL);
        pc_part(&wood, SH_BALL, 0);
        pc_rrect(fx, sole - 16, fw + 10, 22, 14);
        pc_part(&m, SH_BALL, 0);
        pc_ellipse(fx, fy - 12, fw, fh - 4);
        pc_decal(&wood, SH_FLAT);
        pc_line((fx - fw) >> 4, (sole - 22) >> 4, (fx + fw) >> 4, (sole - 22) >> 4, wood.c[3]);
        break; }
    case SHOES_COMBAT: {
        PMat blk;
        pm_flat(&blk, PAL_PAINT(PAINT_JET, 1));
        pc_part(&dk, SH_BALL, 0);
        pc_capsule(x, y - PX(4), x, fy, r, r);
        pc_ellipse(fx, fy - 4, fw + 6, fh);
        pc_part(&blk, SH_FLAT, 0);
        pc_rrect(fx, sole - 10, fw + 10, 18, 6);
        for (k = 1; k <= 3; k++) pc_px(x >> 4, (fy - k * 20) >> 4, (k & 1) ? PAL_WHITE : PAL_GRAY_L);
        break; }
    case SHOES_SLIPPERS: {
        PMat pk;
        pm_ramp(&pk, RAMP_PASTEL_PINK, 5);
        pc_part(&m, SH_SOFT, PF_FUZZ);
        pc_ellipse(fx, fy, fw + 12, fh + 8);
        pc_ellipse(fx - fw / 2, fy - fh - 8, 12, fh * 2 / 3);
        pc_ellipse(fx + fw / 2, fy - fh - 8, 12, fh * 2 / 3);
        pc_decal(&pk, SH_FLAT);
        pc_px((fx >> 4), (fy >> 4) + 1, pk.c[1]);
        if (big) {
            pc_px((fx >> 4) - 2, fy >> 4, PAL_PAINT(PAINT_JET, 1));
            pc_px((fx >> 4) + 2, fy >> 4, PAL_PAINT(PAINT_JET, 1));
        }
        break; }
    case SHOES_HOOVES: {
        PMat hv, fur = c->skin;
        pm_from3(&hv, PAL_PAINT(PAINT_JET, 0), PAL_PAINT(PAINT_CHARCOAL, 1), PAL_PAINT(PAINT_CHARCOAL, 2));
        if (l->shoes_color != PAINT_WHITE) pm_from3(&hv, m.c[0], m.c[1], m.c[2]);
        pc_part(&hv, SH_GLOSS, 0);
        pc_rrect(fx, fy + 8, fw - 8, fh, 16);
        pc_decal(&hv, SH_FLAT);
        pc_line(fx >> 4, (fy + 8) >> 4, fx >> 4, (sole - 4) >> 4, hv.c[0]);
        pc_part(&fur, SH_SOFT, PF_FUZZ);
        pc_ellipse(x, fy - 16, fw, 22);
        pc_tri(x - fw, fy - 16, x - fw / 2, fy - 8, x - fw / 2 - 16, fy + 16);
        pc_tri(x + fw, fy - 16, x + fw / 2, fy - 8, x + fw / 2 + 16, fy + 16);
        break; }
    case SHOES_ROLLERSKATES: {
        PMat wh;
        pm_paint(&wh, PAINT_LEMON);
        pc_part(&m, SH_BALL, 0);
        pc_capsule(x, y - PX(3), x, fy, r - 4, r - 2);
        pc_ellipse(fx, fy - 6, fw, fh - 4);
        pc_part(&sl, SH_FLAT, 0);
        pc_rrect(fx, sole - 12, fw + 2, 10, 6);
        pc_part(&wh, SH_GLOSS, 0);
        pc_ellipse(fx - fw / 2, sole + 8, 18, 18);
        pc_ellipse(fx + fw / 2, sole + 8, 18, 18);
        break; }
    case SHOES_PLATFORM:
        pc_part(&sl, SH_BALL, 0);
        pc_rrect(fx, sole - 4, fw + 6, 28, 10);
        pc_part(&m, SH_GLOSS, 0);
        pc_ellipse(fx, fy - 24, fw, fh - 4);
        pc_decal(&sl, SH_FLAT);
        pc_line((fx - fw) >> 4, (sole - 4) >> 4, (fx + fw) >> 4, (sole - 4) >> 4, sl.c[1]);
        break;
    case SHOES_CLAWS: {
        PMat cl, p = c->skin;
        pm_paint(&cl, PAINT_BONE);
        if (l->marking == MARK_FUR_SOCKS) pm_paint(&p, l->marking_color);
        pc_part(&p, SH_BALL, c->critter ? PF_FUZZ : 0);
        pc_ellipse(fx, fy - 4, fw, fh + 2);
        pc_part(&cl, SH_GLOSS, 0);
        for (k = -1; k <= 1; k++) {
            int tx = fx + k * fw / 2 + side * 12;
            pc_tri(tx - 10, sole - 20, tx + 10, sole - 20, tx + side * 10, sole + 6);
        }
        break; }
    }
    (void)s;
}

static void legs(PCtx *c)
{
    const DoomoLook *l = c->l;
    int s, bt = l->bottom;
    PMat skin = c->skin, bm = c->bot;
    int long_top = top_is_long(l->top);
    if (B.nolegs || B.view == VK_PORTRAIT) return;
    if (l->species == SPECIES_SKELETON) pm_paint(&skin, PAINT_BONE);
    for (s = 0; s < 2; s++) {
        int x0 = B.hpx[s], y0 = B.hpy[s], x1 = B.ftx[s], y1 = B.fty[s];
        int r = B.leg_r, cover = 0;    /* 0 skin, 1 full, 2 upper half */
        if (B.yaw == 2 && s == 1) continue;     /* far leg drawn first below */
        switch (bt) {
        case BOTTOM_PANTS: case BOTTOM_RIPPED_JEANS: case BOTTOM_CARGO:
        case BOTTOM_SWEATS: case BOTTOM_LEGGINGS: cover = 1; break;
        case BOTTOM_SHORTS: cover = 2; break;
        default: cover = 0; break;
        }
        if (l->top == TOP_OVERALLS || l->top == TOP_HAZMAT || l->top == TOP_SPACESUIT ||
            l->top == TOP_PAJAMAS) { cover = 1; bm = c->top; if (l->top == TOP_SPACESUIT) pm_paint(&bm, PAINT_WHITE); }
        if (l->species == SPECIES_SKELETON && cover != 1) {
            pc_part(&skin, SH_BALL, 0);
            pc_capsule(x0, y0, x1, y1, r / 2 + 2, r / 2 + 2);
            pc_ellipse(x0 + (x1 - x0) / 2, y0 + (y1 - y0) / 2, r / 2 + 8, r / 2 + 6);
        } else {
            pc_part(&skin, SH_BALL, c->critter ? PF_FUZZ : 0);
            pc_capsule(x0, y0, x1, y1, r, r);
            if (l->species == SPECIES_ROBOT && B.lod == LOD_BIG)
                pc_line((x0 - r) >> 4, ((y0 + y1) / 2) >> 4, (x0 + r) >> 4, ((y0 + y1) / 2) >> 4, skin.c[0]);
            if (l->species == SPECIES_MUMMY && B.lod == LOD_BIG) {
                int y;
                for (y = y0; y < y1; y += 40) pc_line((x0 - r) >> 4, y >> 4, (x0 + r) >> 4, (y + 12) >> 4, skin.c[1]);
            }
        }
        if (cover && !long_top) {
            int yb = cover == 1 ? y1 - 8 : y0 + (y1 - y0) / 2;
            int rr = r + (bt == BOTTOM_SWEATS || bt == BOTTOM_CARGO ? 10 : (bt == BOTTOM_LEGGINGS ? -2 : 4));
            pc_part(&bm, SH_BALL, 0);
            if (bt == BOTTOM_KILT) pc_pattern(p_pattern_tiles[PATTERN_PLAID], &c->patm);
            pc_capsule(x0, y0, x1 + (x1 - x0) * 0, yb, rr, rr);
            pc_pattern(NULL, NULL);
            if (bt == BOTTOM_RIPPED_JEANS && B.lod != LOD_TINY) {
                PMat sk = skin;
                pc_decal(&sk, SH_FLAT);
                pc_ellipse(x0, (y0 + yb) / 2 + 8, r / 2, 12);
                pc_px(x0 >> 4, ((y0 + yb) / 2 + 8) >> 4, skin.c[1]);
            }
            if (bt == BOTTOM_CARGO && B.lod != LOD_TINY) {
                pc_part(&bm, SH_BALL, 0);
                pc_rrect(x0 + (s ? r : -r), (y0 + yb) / 2, 18, 26, 6);
            }
            if (bt == BOTTOM_SWEATS && B.lod != LOD_TINY) {
                PMat st;
                pm_paint(&st, PAINT_WHITE);
                pc_decal(&st, SH_FLAT);
                pc_line((x0 + (s ? rr - 8 : -rr + 8)) >> 4, y0 >> 4, (x1 + (s ? rr - 8 : -rr + 8)) >> 4, (yb - 8) >> 4, PAL_WHITE);
            }
        }
        foot(c, s, x1, y1);
    }
}

static void far_leg_profile(PCtx *c)
{
    /* in profile the far leg sits behind the body */
    const DoomoLook *l = c->l;
    PMat m = c->skin;
    if (B.nolegs || B.view == VK_PORTRAIT || B.yaw != 2) return;
    if (l->bottom == BOTTOM_PANTS || l->bottom == BOTTOM_RIPPED_JEANS || l->bottom == BOTTOM_CARGO ||
        l->bottom == BOTTOM_SWEATS || l->bottom == BOTTOM_LEGGINGS) m = c->bot;
    pc_part(&m, SH_BALL, 0);
    pc_capsule(B.hpx[1], B.hpy[1], B.ftx[1], B.fty[1], B.leg_r, B.leg_r);
    foot(c, 1, B.ftx[1], B.fty[1]);
    pc_darken_part(pcv.part);
}

/* ---------------------------------------------------------------------- */
/* torso */
static void torso_shape(int grow, int belly)
{
    int H = B.by - B.ty;
    int cy1 = B.ty + PCT(H, 40), cy2 = B.ty + PCT(H, 66);
    pc_frame(B.cx, (B.ty + B.by) / 2, DOOMO_MAX(B.sw, B.hw) + grow + 16, H / 2 + grow + 16);
    pc_ellipse(B.cx, cy1, B.sw + grow, PCT(H, 42) + grow);
    pc_ellipse(B.cx, cy2, B.hw + grow + belly / 2, PCT(H, 36) + grow + belly / 3);
    if (B.view == VK_PORTRAIT) {
        pc_rrect(B.cx, B.by, B.sw + grow, H / 2 + grow, B.sw / 2);
    }
}

static void long_skirt(PCtx *c, const PMat *m, int to_ground)
{
    int H = B.by - B.ty;
    int top = B.waist, bot = to_ground ? B.ground - PX(1) : B.by + PCT((B.ground - B.by), 55);
    int w0 = B.hw, w1 = B.hw + PCT(H, 30);
    if (B.view == VK_PORTRAIT) return;
    pc_part(m, SH_BALL, 0);
    pc_frame(B.cx, (top + bot) / 2, w1 + 8, (bot - top) / 2 + 8);
    pc_quad(B.cx - w0, top, B.cx + w0, top, B.cx + w1, bot, B.cx - w1, bot);
    pc_ellipse(B.cx, bot - 8, w1, 24);
    (void)c;
}

static void bottoms(PCtx *c)
{
    const DoomoLook *l = c->l;
    int bt = l->bottom;
    PMat m = c->bot;
    int H = B.by - B.ty;
    if (B.view == VK_PORTRAIT) return;
    if (top_is_long(l->top) && bt != BOTTOM_TUTU) return;
    if (B.nolegs && bt != BOTTOM_SKIRT && bt != BOTTOM_TUTU && bt != BOTTOM_LONG_SKIRT) return;
    switch (bt) {
    case BOTTOM_NONE:
        return;
    case BOTTOM_SKIRT: case BOTTOM_KILT: {
        int top = B.waist, bot = B.by + PCT((B.ground - B.by), 45);
        int w1 = B.hw + PCT(H, 24);
        pc_part(&m, SH_BALL, 0);
        if (bt == BOTTOM_KILT) pc_pattern(p_pattern_tiles[PATTERN_PLAID], &c->patm);
        pc_frame(B.cx, (top + bot) / 2, w1 + 8, (bot - top) / 2 + 8);
        pc_quad(B.cx - B.hw, top, B.cx + B.hw, top, B.cx + w1, bot, B.cx - w1, bot);
        pc_pattern(NULL, NULL);
        if (B.lod != LOD_TINY) {
            int k;
            pc_decal(&m, SH_FLAT);
            for (k = -1; k <= 1; k++)
                pc_line((B.cx + k * B.hw / 2) >> 4, (top + 16) >> 4, (B.cx + k * w1 * 2 / 3) >> 4, (bot - 4) >> 4, m.c[1]);
        }
        return; }
    case BOTTOM_LONG_SKIRT:
        long_skirt(c, &m, 1);
        return;
    case BOTTOM_ROBE:
        long_skirt(c, &m, 1);
        pc_decal(&m, SH_FLAT);
        pc_line(B.cx >> 4, B.waist >> 4, B.cx >> 4, (B.ground - 24) >> 4, m.c[1]);
        return;
    case BOTTOM_TUTU: {
        int k, y = B.waist + PCT(H, 12), w = B.hw + PCT(H, 34);
        pc_part(&m, SH_SOFT, PF_FUZZ);
        pc_frame(B.cx, y, w + 8, PCT(H, 16) + 8);
        pc_ellipse(B.cx, y, w, PCT(H, 15) + 6);
        for (k = -3; k <= 3; k++) pc_ellipse(B.cx + k * w / 4, y + PCT(H, 12), w / 5 + 4, PCT(H, 9) + 4);
        return; }
    default:
        break;
    }
    /* hips in the bottom colour (pants, shorts, jeans, cargo, leggings, sweats) */
    pc_part(&m, SH_BALL, 0);
    pc_clip(0, (B.waist + 8) >> 4, pcv.w, pcv.h);
    torso_shape(bt == BOTTOM_SWEATS || bt == BOTTOM_CARGO ? 8 : 2, B.belly);
    pc_noclip();
    if (B.lod != LOD_TINY && B.yaw < 3) {
        /* fly seam / belt */
        pc_decal(&m, SH_FLAT);
        if (bt != BOTTOM_LEGGINGS && bt != BOTTOM_SWEATS)
            pc_line(B.cx >> 4, (B.waist + 16) >> 4, B.cx >> 4, (B.by - 4) >> 4, m.c[1]);
        if (bt == BOTTOM_SWEATS) {
            pc_px((B.cx >> 4) - 1, (B.waist + 24) >> 4, PAL_WHITE);
            pc_px((B.cx >> 4) + 1, (B.waist + 36) >> 4, PAL_WHITE);
        }
    }
}

static void chest_fluff(PCtx *c)
{
    PMat m;
    const DoomoLook *l = c->l;
    int y = B.ty + (B.view == VK_CHIBI ? 8 : PX(1)), w = PCT(B.sw, 45);
    if (!c->critter && l->species != SPECIES_MOTHKIN && l->marking != MARK_FUR_BELLY) return;
    if (B.lod == LOD_TINY || B.yaw >= 3) return;
    if ((l->top == TOP_SPACESUIT || l->top == TOP_HAZMAT || l->top == TOP_PLATE_ARMOR ||
         l->accessory == ACC_SCARF) && l->marking != MARK_FUR_BELLY) return;
    y = DOOMO_MAX(B.ty, B.neck - 24);
    if (l->marking == MARK_FUR_BELLY || l->marking == MARK_FUR_MUZZLE) pm_paint(&m, l->marking_color);
    else pm_from3(&m, c->skin.c[2], c->skin.c[3], c->skin.c[4]);
    pc_part(&m, SH_SOFT, PF_FUZZ | PF_NOCAST);
    pc_frame(B.cx, y + 24, w + 16, w);
    pc_tri(B.cx - w, y, B.cx + w, y, B.cx, y + w + w / 2);
    pc_tri(B.cx - w, y, B.cx - w / 3, y + 8, B.cx - w / 2 - 8, y + w);
    pc_tri(B.cx + w / 3, y + 8, B.cx + w, y, B.cx + w / 2 + 8, y + w);
}

static void mothkin_ruff(PCtx *c)
{
    PMat m;
    int k, y = DOOMO_MAX(B.ty + PX(1), B.neck - 8), w = PCT(B.sw, 85);
    if (c->l->species != SPECIES_MOTHKIN || B.yaw >= 3) return;
    pm_paint(&m, PAINT_WHITE);
    if (c->l->marking_color != PAINT_WHITE && c->l->marking >= MARK_FUR_MUZZLE) pm_paint(&m, c->l->marking_color);
    pc_part(&m, SH_SOFT, PF_FUZZ);
    pc_frame(B.cx, y, w + 16, w / 2 + 16);
    pc_ellipse(B.cx, y, w, w / 2 + 8);
    for (k = -3; k <= 3; k++)
        pc_tri(B.cx + k * w / 4 - w / 6, y + 8, B.cx + k * w / 4 + w / 6, y + 8, B.cx + k * w / 4, y + w / 2 + w / 4 + 8);
}

/* torso-surface details (buttons, prints, pockets, plates, aprons). Sizes
 * are fractions of the shoulder half width so they scale to every view. */
#define SW(n) PCT(B.sw, n)
static void dot(int x, int y, uint8_t c) { pc_px(x >> 4, y >> 4, c); }
static void vline(int x, int y0, int y1, uint8_t c) { pc_line(x >> 4, y0 >> 4, x >> 4, y1 >> 4, c); }
static void seg(int x0, int y0, int x1, int y1, uint8_t c) { pc_line(x0 >> 4, y0 >> 4, x1 >> 4, y1 >> 4, c); }

static void top_details(PCtx *c, const PMat *m, int first)
{
    const DoomoLook *l = c->l;
    int t = l->top, k;
    int cx = B.cx, ty, H, bot;
    int big = B.lod == LOD_BIG, tiny = B.lod == LOD_TINY;
    PMat w, d, acc, gold;
    vis_area(&ty, &H);
    bot = ty + H;
    pm_paint(&w, PAINT_WHITE);
    pm_from3(&d, m->c[0], m->c[1], m->c[2]);
    pm_paint(&acc, l->accessory_color == l->top_color ? PAINT_CHERRY : l->accessory_color);
    pm_paint(&gold, PAINT_LEMON);
    if (B.yaw >= 3) return;
    pc_mask_none();
    pc_mask_range(first, pcv.part);
    switch (t) {
    case TOP_HOODIE:
        if (!tiny) {
            /* kangaroo pocket */
            int py = ty + PCT(H, 58), pw = SW(55), ph = DOOMO_MAX(PCT(H, 22), 20);
            pc_decal(&d, SH_FLAT);
            pc_quad(cx - pw, py + ph, cx + pw, py + ph, cx + pw - ph / 2, py - ph / 2, cx - pw + ph / 2, py - ph / 2);
            pc_decal(m, SH_SOFT);
            pc_quad(cx - pw + 12, py + ph - 10, cx + pw - 12, py + ph - 10, cx + pw - ph / 2 - 4, py - ph / 2 + 12,
                    cx - pw + ph / 2 + 4, py - ph / 2 + 12);
        }
        break;
    case TOP_SWEATER: case TOP_SWEATER_VEST: {
        int y, x, step = big ? PX(2) : PX(3);
        if (tiny) break;
        for (y = ty + PX(2) + (t == TOP_SWEATER_VEST ? SW(30) : 0); y < bot - 8; y += step)
            for (x = cx - SW(80); x <= cx + SW(80); x += PX(2)) {
                dot(x, y, m->c[1]);
                dot(x + 16, y + 16, m->c[1]);
            }
        break; }
    case TOP_SUIT: {
        /* shirt V, tie, lapels, buttons */
        int vy = ty + PCT(H, 45);
        pc_decal(&w, SH_SOFT);
        pc_tri(cx - SW(38), ty, cx + SW(38), ty, cx, vy);
        pc_decal(&acc, SH_BALL);
        if (!tiny) {
            pc_tri(cx - SW(10) - 4, ty + 6, cx + SW(10) + 4, ty + 6, cx, ty + SW(14) + 8);
            pc_quad(cx - SW(8), ty + SW(12), cx + SW(8), ty + SW(12), cx + SW(12), vy + 8, cx - SW(12), vy + 8);
        } else dot(cx, ty + 16, acc.c[2]);
        pc_decal(&d, SH_FLAT);
        seg(cx - SW(40), ty, cx - 8, vy, m->c[0]);
        seg(cx + SW(40), ty, cx + 8, vy, m->c[0]);
        if (!tiny) { dot(cx, vy + PX(2), m->c[0]); if (big) dot(cx, vy + PX(5), m->c[0]); }
        break; }
    case TOP_BAND_TEE: {
        /* a little skull print */
        PMat lg;
        int y = ty + PCT(H, 45), r = DOOMO_MAX(SW(30), 16);
        pm_paint(&lg, l->pattern_color == l->top_color ? PAINT_WHITE : l->pattern_color);
        pc_decal(&lg, SH_SOFT);
        pc_ellipse(cx, y, r, PCT(r, 85));
        pc_ellipse(cx, y + r * 7 / 10, r * 6 / 10, r * 4 / 10);
        if (!tiny) {
            uint8_t k2 = PAL_PAINT(PAINT_JET, 1);
            dot(cx - r / 2, y, k2); dot(cx + r / 2, y, k2);
            if (big) { dot(cx - r / 2 + 16, y, k2); dot(cx + r / 2 - 16, y, k2); dot(cx, y + r / 2, k2); }
        }
        break; }
    case TOP_DRESS: case TOP_MAID: {
        /* bow / ribbon at the waist (figure) */
        if (B.view != VK_PORTRAIT) {
            pc_decal(&acc, SH_BALL);
            pc_rrect(cx, B.waist, B.hw, tiny ? 8 : 14, 6);
        }
        if (t == TOP_MAID) {
            /* white apron */
            int ay = ty + PCT(H, 30);
            pc_decal(&w, SH_SOFT);
            pc_rrect(cx, (ay + bot) / 2 + 8, SW(48), (bot - ay) / 2, 12);
            if (big) { dot(cx - SW(48), ay + 8, PAL_GRAY_L); dot(cx + SW(48), ay + 8, PAL_GRAY_L); }
        }
        break; }
    case TOP_ROBE: case TOP_KIMONO: {
        PMat sash;
        pm_paint(&sash, t == TOP_KIMONO ? (l->accessory_color == l->top_color ? PAINT_LEMON : l->accessory_color)
                                        : PAINT_LEMON);
        pc_decal(&sash, SH_BALL);
        if (B.view != VK_PORTRAIT) {
            pc_frame(cx, B.waist, B.hw, 24);
            pc_rrect(cx, B.waist, B.hw + 8, tiny ? 8 : DOOMO_MAX(PCT(H, 10), 14), 6);
        } else {
            pc_frame(cx, bot - PX(3), B.sw, 32);
            pc_rrect(cx, bot - PX(3), B.sw + 8, PX(2), 6);
        }
        break; }
    case TOP_ARMOR: {
        PMat steel;
        pm_ramp(&steel, RAMP_STEEL, 5);
        pc_decal(&steel, SH_GLOSS);
        pc_frame(cx, ty + H / 3, B.sw, H / 2);
        pc_rrect(cx, ty + PCT(H, 34), SW(66), PCT(H, 24), SW(30));
        if (!tiny) {
            vline(cx, ty + PCT(H, 14), ty + PCT(H, 54), steel.c[1]);
            dot(cx - SW(40), ty + PCT(H, 34), steel.c[4]);
        }
        break; }
    case TOP_LEATHER_JACKET: {
        PMat shirt;
        pm_paint(&shirt, l->pattern_color != l->top_color ? l->pattern_color : PAINT_WHITE);
        pc_decal(&shirt, SH_SOFT);
        pc_quad(cx - SW(30), ty, cx + SW(30), ty, cx + SW(22), bot, cx - SW(22), bot);
        pc_decal(&d, SH_FLAT);
        seg(cx - SW(30), ty, cx - SW(22), bot, m->c[0]);
        seg(cx + SW(30), ty, cx + SW(22), bot, m->c[0]);
        if (!tiny) {
            for (k = 1; k <= 3; k++) dot(cx - SW(42), ty + H * k / 5, PAL_GRAY_L);
            dot(cx + SW(46), ty + H * 2 / 5, m->c[4]);
        }
        break; }
    case TOP_OVERALLS: {
        PMat shirt;
        pm_paint(&shirt, PAINT_WHITE);
        pc_decal(&shirt, SH_SOFT);
        pc_clip(0, 0, pcv.w, (ty + PCT(H, 30)) >> 4);
        torso_shape(0, 0);
        pc_noclip();
        pc_mask_none();
        pc_mask_range(first, pcv.part);
        pc_decal(m, SH_BALL);
        pc_rrect(cx, ty + PCT(H, 34), SW(56), PCT(H, 16), 8);
        pc_decal(&d, SH_FLAT);
        seg(cx - SW(56), ty, cx - SW(44), ty + PCT(H, 22), m->c[1]);
        seg(cx + SW(56), ty, cx + SW(44), ty + PCT(H, 22), m->c[1]);
        if (!tiny) {
            pc_decal(&gold, SH_BALL);
            pc_ellipse(cx - SW(40), ty + PCT(H, 24), 14, 14);
            pc_ellipse(cx + SW(40), ty + PCT(H, 24), 14, 14);
        }
        break; }
    case TOP_LAB_COAT: {
        PMat in = c->top, pen;
        pm_paint(&pen, PAINT_COBALT);
        pc_decal(&in, SH_BALL);
        pc_quad(cx - SW(30), ty, cx + SW(30), ty, cx + SW(10), ty + PCT(H, 55), cx - SW(10), ty + PCT(H, 55));
        seg(cx - SW(32), ty, cx, ty + PCT(H, 58), w.c[0]);
        seg(cx + SW(32), ty, cx, ty + PCT(H, 58), w.c[0]);
        vline(cx, ty + PCT(H, 58), bot, w.c[0]);
        if (!tiny) {
            int px = cx + SW(56), py = ty + PCT(H, 40);
            seg(px - 24, py, px + 24, py, w.c[0]);
            dot(px + 8, py - 16, pen.c[2]);
            dot(px + 8, py - 32, pen.c[3]);
        }
        break; }
    case TOP_CLOAK:
        pc_decal(&d, SH_FLAT);
        vline(cx, ty + 16, bot, m->c[0]);
        break;
    case TOP_PRIEST:
        if (!tiny && B.view != VK_PORTRAIT) {
            int y = ty + PCT(H, 50);
            vline(cx, y - 32, y + 32, PAL_UI_GOLD);
            seg(cx - 16, y - 16, cx + 16, y - 16, PAL_UI_GOLD);
        }
        break;
    case TOP_NURSE: {
        int y = ty + PCT(H, 38), x = cx + SW(36), r = big ? PX(2) : PX(1);
        uint8_t cr = l->top_color == PAINT_WHITE ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(l->top_color, 1);
        seg(x - r, y, x + r, y, cr);
        vline(x, y - r, y + r, cr);
        if (big) { seg(x - r, y + 16, x + r, y + 16, cr); vline(x + 16, y - r, y + r, cr); }
        break; }
    case TOP_SPACESUIT: {
        PMat panel;
        int y = ty + PCT(H, 42);
        pm_ramp(&panel, RAMP_STEEL, 2);
        pc_decal(&panel, SH_FLAT);
        pc_rrect(cx, y, SW(40), DOOMO_MAX(PCT(H, 14), 12), 8);
        if (!tiny) {
            dot(cx - SW(20), y, PAL_PAINT(PAINT_CHERRY, 2));
            dot(cx, y, PAL_PAINT(PAINT_LIME, 2));
            dot(cx + SW(20), y, PAL_PAINT(l->top_color, 2));
        }
        break; }
    case TOP_PLATE_ARMOR: {
        int y;
        PMat trim;
        pm_paint(&trim, l->top_color);
        if (!tiny) for (y = ty + PCT(H, 30); y < bot; y += DOOMO_MAX(PCT(H, 20), 24))
            seg(cx - SW(80), y, cx + SW(80), y, m->c[0]);
        pc_decal(&trim, SH_BALL);
        pc_ellipse(cx, ty + PCT(H, 22), DOOMO_MAX(SW(18), 12), DOOMO_MAX(SW(18), 12));
        break; }
    case TOP_HAZMAT: {
        int y = ty + PCT(H, 40), r = big ? 3 : 2;
        uint8_t k2 = PAL_PAINT(PAINT_JET, 1);
        if (tiny) break;
        dot(cx, y - PX(r) / 2 - 8, k2);
        dot(cx - PX(r) / 2 - 4, y + PX(r) / 3, k2);
        dot(cx + PX(r) / 2 + 4, y + PX(r) / 3, k2);
        dot(cx, y, PAL_PAINT(PAINT_JET, 2));
        if (big) {
            dot(cx, y - PX(r) / 2 - 24, k2);
            dot(cx - PX(r) / 2 - 20, y + PX(r) / 3 + 12, k2);
            dot(cx + PX(r) / 2 + 20, y + PX(r) / 3 + 12, k2);
        }
        break; }
    case TOP_PAJAMAS: {
        if (!tiny) {
            for (k = 1; k <= 3; k++) dot(cx + 12, ty + H * k / 4, PAL_WHITE);
            vline(cx - 4, ty + 24, bot - 8, m->c[1]);
            if (l->pattern == PATTERN_SOLID && big) {
                /* sleepy little stars */
                dot(cx - SW(50), ty + PCT(H, 40), PAL_UI_GOLD);
                dot(cx + SW(56), ty + PCT(H, 70), PAL_UI_GOLD);
                dot(cx - SW(30), ty + PCT(H, 85), PAL_UI_GOLD);
            }
        }
        break; }
    case TOP_CROP_TOP:
        if (!tiny) seg(cx - SW(70), ty + PCT(H, 40), cx + SW(70), ty + PCT(H, 40), m->c[1]);
        break;
    case TOP_TSHIRT:
        if (big && B.view != VK_PORTRAIT) seg(cx - SW(60), bot - 16, cx + SW(60), bot - 16, m->c[1]);
        break;
    default:
        break;
    }
    pc_mask_none();
}

/* collars and necklines, drawn after the head so they tuck under the chin */
static void collar(PCtx *c)
{
    const DoomoLook *l = c->l;
    int t = l->top, cx = B.cx, y = B.neck - 8, tiny = B.lod == LOD_TINY, big = B.lod == LOD_BIG;
    PMat m, w, d, gold;
    if (B.yaw >= 3 || t == TOP_NONE) return;
    top_mat(c, &m);
    pm_paint(&w, PAINT_WHITE);
    pm_from3(&d, m.c[0], m.c[1], m.c[2]);
    pm_paint(&gold, PAINT_LEMON);
    switch (t) {
    case TOP_TSHIRT: case TOP_BAND_TEE: case TOP_NURSE:
        if (tiny) break;
        pc_part(&d, SH_BALL, 0);
        pc_clip(0, (y + 4) >> 4, pcv.w, pcv.h);
        pc_ellipse(cx, y, SW(42), SW(18) + 8);
        pc_noclip();
        if (t == TOP_NURSE) {
            pc_part(&w, SH_SOFT, 0);
            pc_tri(cx - SW(44), y, cx - 8, y + 4, cx - SW(30), y + SW(26));
            pc_tri(cx + SW(44), y, cx + 8, y + 4, cx + SW(30), y + SW(26));
        }
        break;
    case TOP_HOODIE: {
        /* the hood rolled around the neck + drawstrings */
        pc_part(&m, SH_SOFT, 0);
        pc_frame(cx, y, SW(80), SW(30));
        pc_clip(0, (y - 8) >> 4, pcv.w, pcv.h);
        pc_ellipse(cx - SW(42), y, SW(36), SW(24) + 8);
        pc_ellipse(cx + SW(42), y, SW(36), SW(24) + 8);
        pc_noclip();
        if (!tiny) {
            int k, len = big ? 4 : 2;
            for (k = 0; k < len; k++) {
                pc_px((cx - SW(18)) >> 4, (y >> 4) + 1 + k, k == len - 1 ? PAL_GRAY_L : PAL_WHITE);
                pc_px((cx + SW(18)) >> 4, (y >> 4) + 1 + k, k == len - 1 ? PAL_GRAY_L : PAL_WHITE);
            }
        }
        break; }
    case TOP_SWEATER: {
        int k;
        pc_part(&d, SH_SOFT, 0);
        pc_rrect(cx, y + 8, SW(46), tiny ? 12 : SW(16) + 10, 16);
        if (!tiny) for (k = -2; k <= 2; k++) pc_px((cx + k * SW(18)) >> 4, (y + 8) >> 4, m.c[0]);
        break; }
    case TOP_DRESS: case TOP_PAJAMAS: case TOP_SWEATER_VEST: case TOP_MAID: {
        /* round Peter Pan collar / pointed shirt collar */
        PMat cl = w;
        if (t == TOP_PAJAMAS) pm_from3(&cl, m.c[1], m.c[2], m.c[3]);
        pc_part(&cl, SH_SOFT, 0);
        if (t == TOP_DRESS || t == TOP_MAID) {
            pc_ellipse(cx - SW(24), y + 4, SW(26), SW(18) + 6);
            pc_ellipse(cx + SW(24), y + 4, SW(26), SW(18) + 6);
            if (t == TOP_DRESS && !tiny) {
                PMat bow;
                pm_paint(&bow, l->accessory_color == l->top_color ? PAINT_CHERRY : l->accessory_color);
                pc_part(&bow, SH_BALL, 0);
                pc_tri(cx, y + SW(14), cx - SW(22), y + 4, cx - SW(22), y + SW(28));
                pc_tri(cx, y + SW(14), cx + SW(22), y + 4, cx + SW(22), y + SW(28));
                pc_ellipse(cx, y + SW(14), 12, 12);
            }
        } else {
            pc_tri(cx - SW(46), y - 4, cx - 8, y + 8, cx - SW(26), y + SW(34));
            pc_tri(cx + SW(46), y - 4, cx + 8, y + 8, cx + SW(26), y + SW(34));
        }
        break; }
    case TOP_SUIT: case TOP_LAB_COAT: case TOP_LEATHER_JACKET: {
        /* lapel tips at the collar */
        PMat lp = t == TOP_LAB_COAT ? w : d;
        if (tiny) break;
        pc_part(&lp, SH_BALL, 0);
        pc_tri(cx - SW(52), y - 4, cx - SW(22), y + 4, cx - SW(40), y + SW(38));
        pc_tri(cx + SW(52), y - 4, cx + SW(22), y + 4, cx + SW(40), y + SW(38));
        break; }
    case TOP_ROBE: case TOP_KIMONO: {
        /* crossed collar: white inner band + dark outer edge */
        PMat in = w;
        if (t == TOP_ROBE) pm_from3(&in, m.c[2], m.c[3], m.c[4]);
        pc_part(&in, SH_SOFT, 0);
        pc_quad(cx - SW(40), y - 4, cx - SW(24), y - 4, cx + SW(14), y + SW(52), cx - 4, y + SW(56));
        pc_part(&d, SH_BALL, 0);
        pc_quad(cx + SW(40), y - 4, cx + SW(24), y - 4, cx - SW(6), y + SW(40), cx + 8, y + SW(46));
        break; }
    case TOP_ARMOR: case TOP_PLATE_ARMOR: {
        PMat steel;
        pm_ramp(&steel, RAMP_STEEL, t == TOP_ARMOR ? 4 : 5);
        pc_part(&steel, SH_GLOSS, 0);
        pc_rrect(cx, y + 6, SW(46), tiny ? 10 : SW(16) + 8, 12);
        break; }
    case TOP_CLOAK:
        pc_part(&gold, SH_GLOSS, 0);
        pc_ellipse(cx, y + 12, tiny ? 10 : SW(16) + 6, tiny ? 10 : SW(16) + 6);
        break;
    case TOP_PRIEST:
        pc_part(&d, SH_BALL, 0);
        pc_rrect(cx, y + 4, SW(40), tiny ? 10 : SW(14) + 8, 10);
        pc_part(&w, SH_FLAT, 0);
        pc_rrect(cx, y + 8, tiny ? 8 : SW(10) + 6, tiny ? 8 : SW(10) + 6, 4);
        break;
    case TOP_SPACESUIT: {
        PMat ring;
        pm_ramp(&ring, RAMP_STEEL, 5);
        pc_part(&ring, SH_GLOSS, 0);
        pc_rrect(cx, y + 6, SW(58), tiny ? 12 : SW(18) + 10, 16);
        break; }
    case TOP_HAZMAT:
        pc_part(&d, SH_BALL, 0);
        pc_rrect(cx, y + 6, SW(52), tiny ? 12 : SW(16) + 10, 16);
        break;
    case TOP_OVERALLS: case TOP_TANK: case TOP_CROP_TOP: default:
        break;
    }
}

static void hood_back(PCtx *c)
{
    /* the hood (and cloak hood) bunched up behind the neck */
    PMat m;
    int t = c->l->top;
    if (t != TOP_HOODIE && t != TOP_CLOAK && t != TOP_HAZMAT) return;
    top_mat(c, &m);
    if (t == TOP_HAZMAT) return;
    pc_part(&m, SH_SOFT, 0);
    pc_ellipse(B.cx, B.ty + 8, PCT(B.sw, 80), B.view == VK_CHIBI ? 24 : PX(4));
}

static void torso(PCtx *c)
{
    const DoomoLook *l = c->l;
    int t = l->top, sp = l->species, first;
    PMat m, sk = c->skin;
    int H = B.by - B.ty;
    if (sp == SPECIES_SKELETON) pm_paint(&sk, PAINT_BONE);
    /* bare torso / body underneath */
    if (t == TOP_NONE || t == TOP_TANK || t == TOP_CROP_TOP || sp == SPECIES_SLIME ||
        sp == SPECIES_GHOST || c->ghost) {
        int sh = sp == SPECIES_SLIME ? SH_GLOSS : SH_BALL;
        pc_part(&c->skin, sh, c->critter ? PF_FUZZ : 0);
        if (B.nolegs && B.view != VK_PORTRAIT) {
            /* slime goo / ghost wisp / floaty caco body */
            pc_frame(B.cx, (B.ty + B.ground) / 2, B.hw + 24, (B.ground - B.ty) / 2 + 16);
            torso_shape(0, B.belly);
            if (sp == SPECIES_SLIME && !c->ghost) {
                pc_ellipse(B.cx, B.ground - PX(2), B.hw + PX(2), PX(2) + 8);
                pc_capsule(B.cx - B.hw, B.by - 8, B.cx - B.hw - 8, B.ground - PX(2), 24, 30);
            } else if (sp == SPECIES_GHOST || c->ghost) {
                int wob = (c->anim >> 3) & 1 ? 16 : -16;
                pc_bezier(B.cx, B.by - 16, B.cx + B.hw / 2 + wob, B.by + PX(4), B.cx + B.hw + wob, B.ground - PX(4),
                          PCT(B.hw, 70), 16, 6);
            }
        } else {
            torso_shape(0, B.belly);
        }
        if (B.lod != LOD_TINY && B.yaw < 3) {
            PMat bl;
            if (l->marking == MARK_FUR_BELLY) pm_paint(&bl, l->marking_color);
            else pm_from3(&bl, sk.c[2], sk.c[3], sk.c[4]);
            if (sp == SPECIES_SKELETON) {
                int y;
                pc_decal(&sk, SH_FLAT);
                for (y = B.ty + PX(3); y < B.by - PX(2); y += PX(2))
                    pc_line((B.cx - PCT(B.sw, 60)) >> 4, y >> 4, (B.cx + PCT(B.sw, 60)) >> 4, y >> 4, sk.c[0]);
                pc_line(B.cx >> 4, (B.ty >> 4) + 2, B.cx >> 4, (B.by >> 4) - 2, sk.c[1]);
            } else if (sp == SPECIES_ROBOT) {
                PMat pn;
                pm_ramp(&pn, RAMP_STEEL, 2);
                pc_decal(&pn, SH_FLAT);
                pc_rrect(B.cx, B.ty + PCT(H, 45), PCT(B.sw, 45), PCT(H, 20), 12);
                pc_px(B.cx >> 4, (B.ty + PCT(H, 45)) >> 4, PAL_PAINT(PAINT_LIME, 2));
            } else if (c->critter || l->marking == MARK_FUR_BELLY || sp == SPECIES_FROGLING) {
                pc_decal(&bl, SH_SOFT);
                pc_frame(B.cx, B.ty + PCT(H, 62), PCT(B.hw, 60) + 8, PCT(H, 34) + 8);
                pc_ellipse(B.cx, B.ty + PCT(H, 62), PCT(B.hw, 58) + B.belly / 3, PCT(H, 32));
            } else if (B.view != VK_PORTRAIT && t != TOP_TANK) {
                pc_px(B.cx >> 4, (B.ty + PCT(H, 70)) >> 4, sk.c[1]);
            }
            if (sp == SPECIES_PLUSHIE) {
                int y;
                for (y = B.ty + PX(2); y < B.by; y += PX(2)) pc_px(B.cx >> 4, y >> 4, sk.c[0]);
            }
            if (sp == SPECIES_OWLKIN && B.lod == LOD_BIG) {
                int y;
                for (y = B.ty + PX(4); y < B.by - PX(2); y += PX(3)) {
                    pc_px((B.cx >> 4) - 2, y >> 4, sk.c[1]); pc_px((B.cx >> 4) - 1, (y >> 4) + 1, sk.c[1]);
                    pc_px((B.cx >> 4) + 2, y >> 4, sk.c[1]); pc_px((B.cx >> 4) + 1, (y >> 4) + 1, sk.c[1]);
                }
            }
        }
        if (t == TOP_NONE || sp == SPECIES_SLIME || sp == SPECIES_GHOST) return;
    }
    top_mat(c, &m);
    pc_part(&m, top_shade(t), 0);
    first = pcv.part;
    if (l->pattern != PATTERN_SOLID) pc_pattern(p_pattern_tiles[l->pattern % PATTERN_COUNT], &c->patm);
    if (t == TOP_TANK) {
        /* body in the top colour below the scoop neck */
        pc_clip(0, (B.ty + PCT(H, 22)) >> 4, pcv.w, pcv.h);
        torso_shape(2, B.belly);
        pc_noclip();
        pc_pattern(NULL, NULL);
        pc_part(&m, SH_FLAT, 0);
        pc_capsule(B.cx - PCT(B.sw, 50), B.ty, B.cx - PCT(B.sw, 45), B.ty + PCT(H, 25), 10, 10);
        pc_capsule(B.cx + PCT(B.sw, 50), B.ty, B.cx + PCT(B.sw, 45), B.ty + PCT(H, 25), 10, 10);
    } else if (t == TOP_CROP_TOP) {
        pc_clip(0, 0, pcv.w, (B.ty + PCT(H, 42)) >> 4);
        torso_shape(2, 0);
        pc_noclip();
    } else {
        int grow = (t == TOP_SPACESUIT || t == TOP_HAZMAT || t == TOP_PLATE_ARMOR || t == TOP_CLOAK) ? 16 :
                   (t == TOP_SWEATER || t == TOP_HOODIE ? 8 : 2);
        if (!top_is_long(t)) pc_clip(0, 0, pcv.w, (B.waist + 24) >> 4);
        torso_shape(grow, B.belly);
        pc_noclip();
        if (t == TOP_CLOAK && B.view != VK_PORTRAIT) {
            pc_quad(B.cx - B.sw - 16, B.ty + 16, B.cx + B.sw + 16, B.ty + 16,
                    B.cx + B.hw + PX(2), B.by + (B.ground - B.by) / 2, B.cx - B.hw - PX(2), B.by + (B.ground - B.by) / 2);
        }
    }
    pc_pattern(NULL, NULL);
    if (top_is_long(t) && B.view != VK_PORTRAIT && t != TOP_OVERALLS && t != TOP_PAJAMAS &&
        t != TOP_HAZMAT && t != TOP_SPACESUIT && t != TOP_CLOAK) {
        int to_ground = t == TOP_ROBE || t == TOP_PRIEST || t == TOP_KIMONO;
        if (l->pattern != PATTERN_SOLID) {
            pc_part(&m, SH_BALL, 0);
            pc_pattern(p_pattern_tiles[l->pattern % PATTERN_COUNT], &c->patm);
            {
                int top = B.waist, bot = to_ground ? B.ground - PX(1) : B.by + PCT((B.ground - B.by), 55);
                int w1 = B.hw + PCT(H, 30);
                pc_frame(B.cx, (top + bot) / 2, w1 + 8, (bot - top) / 2 + 8);
                pc_quad(B.cx - B.hw, top, B.cx + B.hw, top, B.cx + w1, bot, B.cx - w1, bot);
                pc_ellipse(B.cx, bot - 8, w1, 24);
            }
            pc_pattern(NULL, NULL);
        } else {
            long_skirt(c, &m, to_ground);
        }
    }
    top_details(c, &m, first);
}

/* ---------------------------------------------------------------------- */
/* arms */
static void paw(PCtx *c, int x, int y, int r)
{
    const DoomoLook *l = c->l;
    PMat p = c->skin;
    int t = l->top;
    if (l->marking == MARK_FUR_SOCKS) pm_paint(&p, l->marking_color);
    if (l->species == SPECIES_SKELETON) pm_paint(&p, PAINT_BONE);
    if (t == TOP_HAZMAT || t == TOP_SPACESUIT || t == TOP_PLATE_ARMOR) {
        p = c->top;
        if (t == TOP_SPACESUIT) pm_paint(&p, PAINT_LEMON);
        if (t == TOP_PLATE_ARMOR) pm_ramp(&p, RAMP_STEEL, 4);
    }
    pc_part(&p, SH_BALL, c->critter ? PF_FUZZ : 0);
    pc_ellipse(x, y, r, r);
    if (B.lod == LOD_BIG && (c->critter || l->species == SPECIES_PLUSHIE) && r >= 40) {
        pc_px((x >> 4) - 1, (y + r / 2) >> 4, p.c[1]);
        pc_px((x >> 4) + 1, (y + r / 2) >> 4, p.c[1]);
    }
}

static void arm(PCtx *c, int s, int behind)
{
    const DoomoLook *l = c->l;
    int x0 = B.shx[s], y0 = B.shy[s], x1 = B.hdx[s], y1 = B.hdy[s];
    int r = B.arm_r, kind;
    PMat sl, sk = c->skin;
    if (l->species == SPECIES_SKELETON) pm_paint(&sk, PAINT_BONE);
    kind = sleeve_mat(c, &sl);
    if (l->top == TOP_CLOAK && B.view != VK_PORTRAIT) return;
    /* the arm itself */
    if (kind == 2 || kind == 3) {
        pc_part(&sl, SH_BALL, 0);
        pc_capsule(x0, y0, x1, y1, r + (kind == 3 ? 6 : 2), r + (kind == 3 ? 18 : 2));
    } else {
        if (l->species == SPECIES_SKELETON) {
            pc_part(&sk, SH_BALL, 0);
            pc_capsule(x0, y0, x1, y1, r / 2 + 2, r / 2 + 2);
        } else {
            pc_part(&sk, SH_BALL, c->critter ? PF_FUZZ : 0);
            pc_capsule(x0, y0, x1, y1, r, r);
        }
        if (kind == 1) {
            pc_part(&sl, top_shade(l->top), 0);
            if (l->pattern != PATTERN_SOLID && l->top != TOP_SWEATER_VEST && l->top != TOP_OVERALLS)
                pc_pattern(p_pattern_tiles[l->pattern % PATTERN_COUNT], &c->patm);
            if (l->top == TOP_DRESS || l->top == TOP_MAID)
                pc_ellipse(x0, y0 + 8, r + 16, r + 12);
            else
                pc_capsule(x0, y0, x0 + PCT((x1 - x0), 45), y0 + PCT((y1 - y0), 45), r + 6, r + 6);
            pc_pattern(NULL, NULL);
        }
    }
    if (l->species == SPECIES_ROBOT && B.lod == LOD_BIG)
        pc_px(((x0 + x1) / 2) >> 4, ((y0 + y1) / 2) >> 4, c->skin.c[0]);
    if (l->top == TOP_ARMOR || l->top == TOP_PLATE_ARMOR) {
        PMat pa;
        if (l->top == TOP_ARMOR) pm_ramp(&pa, RAMP_STEEL, 5); else pm_paint(&pa, l->top_color);
        pc_part(&pa, SH_GLOSS, 0);
        pc_ellipse(x0, y0, r + 12, r + 4);
    }
    paw(c, x1, y1, B.paw_r);
    if (behind) {
        int k;
        for (k = 0; k < 3; k++) pc_darken_part(pcv.part - k);
    }
}

/* ---------------------------------------------------------------------- */
/* tails (anchor at the hip, unit B.unit, side B.tails) */
static int TX, TY, TU, TS;
static int tx(int n) { return TX + TS * n * TU / 256; }
static int ty_(int n) { return TY + n * TU / 256; }
static int tr(int n) { return n * TU / 256; }
static void tB(int ax, int ay, int bx, int by, int cx, int cy, int r0, int r1, int seg)
{
    pc_bezier(tx(ax), ty_(ay), tx(bx), ty_(by), tx(cx), ty_(cy), tr(r0), tr(r1), seg);
}
static void tE(int x, int y, int rx, int ry) { pc_ellipse(tx(x), ty_(y), tr(rx), tr(ry)); }
static void tT(int ax, int ay, int bx, int by, int cx, int cy)
{
    pc_tri(tx(ax), ty_(ay), tx(bx), ty_(by), tx(cx), ty_(cy));
}

void pb_draw_tail(PCtx *c, int front)
{
    const DoomoLook *l = c->l;
    int st = l->tail, w = c->wag ? 40 : -20, k;
    PMat m = c->skin, tip;
    int back_view = c->byaw >= 3;
    if (st == TAIL_NONE) return;
    if (front != back_view) return;
    TX = B.tailx; TY = B.taily; TU = B.unit; TS = B.tails;
    if (c->byaw == 2) TS = 1;
    if (back_view) { TX = B.cx + (B.view == VK_CHIBI ? 0 : 8); TY = B.by - (B.view == VK_CHIBI ? 8 : PX(2)); }
    if (B.view == VK_PORTRAIT) w = w / 2;
    if (!back_view && c->byaw != 2 &&
        (st == TAIL_BEAR_NUB || st == TAIL_BUNNY || st == TAIL_SHEEP_PUFF)) {
        /* little round tails would hide behind the body: let them peek */
        TX += B.view == VK_PORTRAIT ? PX(5) : PCT(B.hw, 55);
        TY -= B.view == VK_PORTRAIT ? PX(1) : 0;
    }
    pm_paint(&tip, PAINT_WHITE);
    if (l->marking >= MARK_FUR_MUZZLE) pm_paint(&tip, l->marking_color);
    switch (st) {
    case TAIL_DEVIL:
        pc_part(&m, SH_BALL, 0);
        tB(0, 0, 220 + w, -20, 200, -260 + w, 22, 16, 6);
        tT(130, -270 + w, 270, -270 + w, 200, -380 + w);
        tE(160, -250 + w, 40, 30);
        tE(240, -250 + w, 40, 30);
        break;
    case TAIL_CAT:
        pc_part(&m, SH_BALL, c->critter ? PF_FUZZ : 0);
        tB(0, 0, 260, 20, 200 + w, -300, 34, 30, 7);
        tB(200 + w, -300, 180 + w, -380, 260 + w, -400, 30, 24, 3);
        if (l->marking == MARK_FUR_TABBY) {
            pc_decal(&c->skin, SH_FLAT);
            for (k = 0; k < 3; k++) {
                int yy = -60 - k * 90;
                pc_line(tx(150 + k * 20) >> 4, ty_(yy) >> 4, tx(230 + k * 10) >> 4, ty_(yy + 10) >> 4, c->skin.c[0]);
            }
        }
        break;
    case TAIL_FOX:
        pc_part(&m, SH_SOFT, PF_FUZZ);
        pc_frame(tx(170), ty_(-160), tr(170), tr(200));
        tB(0, 0, 280 + w, 0, 220 + w, -260, 70, 96, 6);
        tT(180 + w, -260, 320 + w, -220, 300 + w, -400);
        pc_part(&tip, SH_SOFT, PF_FUZZ);
        tE(230 + w, -310, 80, 84);
        tT(170 + w, -320, 290 + w, -320, 260 + w, -450);
        break;
    case TAIL_LIZARD:
        pc_part(&m, SH_BALL, 0);
        tB(0, -20, 300, 60, 420 + w, -80, 80, 12, 7);
        pc_decal(&m, SH_FLAT);
        for (k = 1; k < 5; k++) pc_px(tx(k * 80) >> 4, ty_(-30 + k * 6) >> 4, m.c[1]);
        break;
    case TAIL_BUNNY:
        pc_part(&tip, SH_SOFT, PF_FUZZ);
        tE(40, -40, 76, 70);
        tT(-20, -80, 40, -110, -10, -150);
        tT(60, -100, 120, -60, 140, -130);
        break;
    case TAIL_SPIKED: {
        PMat sp;
        pm_paint(&sp, l->horn_color);
        pc_part(&sp, SH_BALL, 0);
        for (k = 1; k <= 4; k++) {
            int x = k * 80, y = -10 - k * 10 - (k * k) * 4;
            tT(x - 30, y - 30, x + 30, y - 30, x + 6, y - 110 + k * 10);
        }
        pc_part(&m, SH_BALL, 0);
        tB(0, -10, 260, 40, 400 + w, -140, 84, 16, 7);
        break; }
    case TAIL_TENTACLE: {
        PMat sk;
        pm_from3(&sk, m.c[2], m.c[3], m.c[4]);
        pc_part(&m, SH_BALL, 0);
        tB(0, 0, 240, 100, 280 + w, -140, 46, 18, 7);
        tB(280 + w, -140, 300 + w, -260, 200 + w, -220, 18, 12, 4);
        pc_decal(&sk, SH_FLAT);
        for (k = 1; k <= 3; k++) pc_px(tx(60 + k * 60) >> 4, ty_(40 + k * 4) >> 4, sk.c[3]);
        break; }
    case TAIL_SKELETAL: {
        PMat bone;
        pm_paint(&bone, PAINT_BONE);
        pc_part(&bone, SH_BALL, 0);
        for (k = 0; k < 7; k++) {
            int t = k * 256 / 7, it = 256 - t;
            int x = (2 * it * t * 260 + t * t * (220 + w)) / 65536;
            int y = (2 * it * t * 20 + t * t * (-280)) / 65536;
            tE(x, y, 34 - k * 2, 28 - k * 2);
        }
        break; }
    case TAIL_FLAME: {
        PMat fl;
        pm_ramp(&fl, RAMP_LAVA, 4);
        pc_part(&m, SH_BALL, 0);
        tB(0, 0, 200, 0, 220, -160, 34, 22, 5);
        pc_part(&fl, SH_SOFT, PF_NOLINE);
        tE(220, -200, 70, 80);
        tT(160, -220, 280, -220, 220 + w, -400);
        tT(170, -200, 220, -240, 140 - w / 2, -330);
        pm_ramp(&fl, RAMP_LAVA, 6);
        pc_decal(&fl, SH_FLAT);
        tE(220, -200, 34, 40);
        break; }
    case TAIL_BEAR_NUB:
        pc_part(&m, SH_BALL, PF_FUZZ);
        tE(20, -20, 56, 50);
        break;
    case TAIL_PUP_WAGGY:
        pc_part(&m, SH_BALL, PF_FUZZ);
        tB(0, 0, 170 + w, -40, 140 + w * 2, -240, 40, 30, 6);
        pc_part(&tip, SH_SOFT, PF_FUZZ);
        tE(140 + w * 2, -250, 42, 46);
        break;
    case TAIL_MOUSE: {
        PMat pk;
        pm_ramp(&pk, RAMP_PASTEL_PINK, 5);
        pc_part(&pk, SH_BALL, 0);
        tB(0, 0, 300, 80, 380 + w, -140, 14, 10, 8);
        tB(380 + w, -140, 400 + w, -240, 320 + w, -240, 10, 8, 3);
        break; }
    case TAIL_SHEEP_PUFF:
        pc_part(&tip, SH_SOFT, PF_FUZZ);
        tE(30, -30, 60, 52);
        tE(90, -60, 50, 46);
        tE(60, -100, 48, 44);
        break;
    case TAIL_RACCOON_RINGED: {
        PMat dk;
        pm_from3(&dk, PAL_PAINT(PAINT_CHARCOAL, 0), PAL_PAINT(PAINT_CHARCOAL, 1), PAL_PAINT(PAINT_CHARCOAL, 2));
        if (l->marking >= MARK_FUR_MUZZLE && l->marking_color != PAINT_WHITE) pm_paint(&dk, l->marking_color);
        pc_part(&m, SH_SOFT, PF_FUZZ);
        pc_frame(tx(160), ty_(-150), tr(180), tr(200));
        tB(0, 0, 260 + w, 0, 220 + w, -280, 64, 72, 6);
        pc_mask_none(); pc_mask_add(pcv.part);
        pc_decal(&dk, SH_SOFT);
        pc_frame(tx(160), ty_(-150), tr(180), tr(200));
        for (k = 0; k < 3; k++) {
            int y = -60 - k * 90;
            int x = k == 0 ? 210 + w / 2 : 250 + w;
            tT(x - 120, y + 20, x + 120, y - 10, x + 120, y + 40);
            tT(x - 120, y + 20, x + 120, y + 40, x - 120, y + 60);
        }
        tE(225 + w, -300, 70, 50);
        pc_mask_none();
        break; }
    case TAIL_FEATHER_FAN: {
        PMat fm = c->skin, f2;
        pm_from3(&f2, m.c[1], m.c[2], m.c[3]);
        if (l->marking >= MARK_FUR_MUZZLE) pm_paint(&f2, l->marking_color);
        for (k = -2; k <= 2; k++) {
            int a = 192 + k * 18 + (c->wag ? 4 : 0);
            pc_part((k & 1) ? &f2 : &fm, SH_BALL, 0);
            pc_capsule(tx(0), ty_(0), tx(p_cos(a) * 220 / 256 * TS + 60), ty_(p_sin(a) * 260 / 256), tr(24), tr(46));
        }
        break; }
    default:
        break;
    }
}

/* ---------------------------------------------------------------------- */
/* wings (anchor at the shoulder blade, mirrored per side) */
static int WX, WY, WU, WS;
static int wx(int n) { return WX + WS * n * WU / 256; }
static int wy(int n) { return WY + n * WU / 256; }
static int wr(int n) { return n * WU / 256; }

static void wing_one(PCtx *c, int st, int flap)
{
    const DoomoLook *l = c->l;
    PMat m, bone, mem;
    int k;
    pm_paint(&bone, PAINT_BONE);
    switch (st) {
    case WINGS_BAT: case WINGS_TINY_BAT: case WINGS_DRAGON: case WINGS_BONE: {
        int sc = st == WINGS_TINY_BAT ? 150 : (st == WINGS_DRAGON ? 300 : 256);
        int up = flap ? -60 : 0;
        int ex = 340 * sc / 256, ey = (-260 + up) * sc / 256;
        if (st == WINGS_DRAGON) pm_paint(&m, l->horn_color);
        else m = c->skin;
        pm_from3(&mem, m.c[0], m.c[1], m.c[2]);
        if (st == WINGS_BAT || st == WINGS_TINY_BAT) pm_from3(&mem, PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_GRAPE, 2));
        if (st == WINGS_BAT && l->skin >= 8) pm_from3(&mem, c->skin.c[0], c->skin.c[1], c->skin.c[2]);
        if (st != WINGS_BONE) {
            pc_part(&mem, SH_BALL, 0);
            pc_frame(wx(ex / 2), wy(ey / 2 + 40), wr(ex / 2 + 30), wr(-ey / 2 + 60));
            pc_tri(wx(0), wy(0), wx(ex), wy(ey), wx(PCT(ex, 105)), wy(ey / 3 + 40));
            pc_tri(wx(0), wy(0), wx(PCT(ex, 105)), wy(ey / 3 + 40), wx(PCT(ex, 75)), wy(80 * sc / 256));
            pc_tri(wx(0), wy(0), wx(PCT(ex, 75)), wy(80 * sc / 256), wx(PCT(ex, 35)), wy(120 * sc / 256));
            /* scallops */
            pc_ellipse(wx(PCT(ex, 90)), wy(ey / 3 + 60), wr(40 * sc / 256), wr(40 * sc / 256));
            pc_ellipse(wx(PCT(ex, 55)), wy(90 * sc / 256), wr(40 * sc / 256), wr(34 * sc / 256));
            if (st == WINGS_DRAGON) {
                PMat spk;
                pm_paint(&spk, l->horn_color);
                pc_part(&spk, SH_BALL, 0);
                for (k = 1; k <= 3; k++)
                    pc_tri(wx(ex * k / 4 - 20), wy(ey * k / 4), wx(ex * k / 4 + 20), wy(ey * k / 4 - 10), wx(ex * k / 4), wy(ey * k / 4 - 60));
            }
        }
        pc_part(st == WINGS_BONE ? &bone : &m, SH_BALL, 0);
        pc_capsule(wx(0), wy(0), wx(ex), wy(ey), wr(20 * sc / 256) + 6, wr(14 * sc / 256) + 4);
        if (B.lod != LOD_TINY || st == WINGS_BONE) {
            pc_capsule(wx(ex), wy(ey), wx(PCT(ex, 105)), wy(ey / 3 + 40), wr(10 * sc / 256) + 4, 8);
            pc_capsule(wx(ex), wy(ey), wx(PCT(ex, 75)), wy(80 * sc / 256), wr(10 * sc / 256) + 4, 8);
            pc_capsule(wx(ex), wy(ey), wx(PCT(ex, 35)), wy(120 * sc / 256), wr(10 * sc / 256) + 4, 8);
        }
        pc_part(st == WINGS_BONE ? &bone : &m, SH_BALL, 0);
        pc_tri(wx(ex - 20), wy(ey), wx(ex + 20), wy(ey), wx(ex + 10), wy(ey - 50));
        break; }
    case WINGS_ANGEL: {
        int up = flap ? -50 : 0;
        pm_paint(&m, PAINT_WHITE);
        pc_part(&m, SH_SOFT, 0);
        pc_frame(wx(170), wy(-110 + up), wr(200), wr(170));
        pc_ellipse(wx(150), wy(-130 + up), wr(160), wr(120));
        for (k = 0; k < 4; k++) {
            int x = 70 + k * 70, y = -20 + up + k * 10 - k * k * 6;
            pc_ellipse(wx(x), wy(y + 50), wr(46), wr(70 - k * 6));
        }
        pc_ellipse(wx(300), wy(-200 + up), wr(70), wr(60));
        pc_decal(&m, SH_FLAT);
        for (k = 0; k < 3; k++)
            pc_line(wx(100 + k * 70) >> 4, wy(-60 + up) >> 4, wx(110 + k * 70) >> 4, wy(30 + up - k * 10) >> 4, m.c[1]);
        break; }
    case WINGS_BUTTERFLY: {
        PMat p2;
        int up = flap ? -40 : 0;
        pm_paint(&m, l->marking_color);
        pm_paint(&p2, l->hair_color);
        if (l->marking_color == l->hair_color) pm_paint(&p2, PAINT_LEMON);
        pc_part(&m, SH_SOFT, 0);
        pc_ellipse(wx(170), wy(-170 + up), wr(150), wr(130));
        pc_ellipse(wx(140), wy(50), wr(100), wr(90));
        pc_decal(&p2, SH_SOFT);
        pc_ellipse(wx(200), wy(-200 + up), wr(60), wr(54));
        pc_ellipse(wx(150), wy(60), wr(40), wr(36));
        pc_decal(&m, SH_FLAT);
        pc_px(wx(200) >> 4, wy(-200 + up) >> 4, PAL_WHITE);
        break; }
    case WINGS_FLY: {
        int up = flap ? -40 : 0;
        pm_from3(&m, PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_CYAN, 2), PAL_WHITE);
        pc_part(&m, SH_SOFT, 0);
        pc_capsule(wx(0), wy(0), wx(260), wy(-260 + up), wr(30), wr(70));
        pc_capsule(wx(0), wy(20), wx(280), wy(-80 + up / 2), wr(24), wr(56));
        pc_decal(&m, SH_FLAT);
        pc_line(wx(20) >> 4, wy(-10) >> 4, wx(250) >> 4, wy(-250 + up) >> 4, PAL_PAINT(PAINT_SKY, 0));
        pc_line(wx(20) >> 4, wy(20) >> 4, wx(270) >> 4, wy(-80 + up / 2) >> 4, PAL_PAINT(PAINT_SKY, 0));
        break; }
    case WINGS_MECH: {
        PMat st2, gl;
        int up = flap ? -30 : 0;
        pm_ramp(&st2, RAMP_STEEL, 4);
        pm_paint(&gl, PAINT_CYAN);
        for (k = 0; k < 3; k++) {
            pc_part(&st2, SH_GLOSS, 0);
            pc_quad(wx(20), wy(-20 + k * 40), wx(280 - k * 40), wy(-260 + up + k * 90),
                    wx(320 - k * 40), wy(-220 + up + k * 90), wx(40), wy(20 + k * 40));
        }
        pc_part(&gl, SH_GLOSS, PF_NOLINE);
        pc_ellipse(wx(60), wy(40), wr(30), wr(30));
        break; }
    case WINGS_FLAME: {
        PMat fl;
        int up = flap ? -40 : 0;
        pm_ramp(&fl, RAMP_LAVA, 4);
        pc_part(&fl, SH_SOFT, 0);
        pc_frame(wx(160), wy(-140), wr(200), wr(200));
        for (k = 0; k < 4; k++) {
            int x = 60 + k * 70, h = 300 - k * 40 + ((c->anim >> 2) + k) % 2 * 40;
            pc_tri(wx(x - 50), wy(40), wx(x + 50), wy(30), wx(x + 40), wy(-h + up));
        }
        pm_ramp(&fl, RAMP_LAVA, 6);
        pc_decal(&fl, SH_FLAT);
        pc_ellipse(wx(120), wy(-40), wr(50), wr(60));
        break; }
    default:
        break;
    }
}

void pb_draw_wings(PCtx *c, int front)
{
    int st = c->l->wings, back_view = c->byaw >= 3;
    int flap = c->wag;
    if (st == WINGS_NONE) return;
    if (front != back_view) return;
    WU = B.unit;
    WY = B.wingy;
    if (B.view == VK_PORTRAIT) WU = PX(19);
    for (WS = -1; WS <= 1; WS += 2) {
        if (c->byaw == 2 && WS < 0) continue;
        WX = B.cx + WS * PCT(B.sw, 40);
        if (c->byaw == 2) { WX = B.cx + B.sw / 2; }
        wing_one(c, st, flap);
    }
}

/* ---------------------------------------------------------------------- */
/* accessories worn on the body */
static void neck_acc(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->accessory, k;
    int cx = B.cx, ty, H;
    int tiny = B.lod == LOD_TINY;
    PMat m = c->acc, gold, dk;
    vis_area(&ty, &H);
    ty -= tiny ? 0 : 8;
    pm_paint(&gold, PAINT_LEMON);
    pm_flat(&dk, PAL_PAINT(PAINT_JET, 1));
    if (B.yaw >= 3 && st != ACC_SCARF && st != ACC_CHAINS) return;
    switch (st) {
    case ACC_BOWTIE: {
        int bw = tiny ? 24 : SW(34) + 8, bh = tiny ? 16 : SW(22) + 6, y = ty + (tiny ? 8 : 16);
        pc_part(&m, SH_BALL, 0);
        pc_tri(cx, y, cx - bw, y - bh, cx - bw, y + bh);
        pc_tri(cx, y, cx + bw, y - bh, cx + bw, y + bh);
        pc_part(&m, SH_GLOSS, 0);
        pc_ellipse(cx, y, tiny ? 8 : 18, tiny ? 8 : 18);
        break; }
    case ACC_CROSS_NECKLACE: case ACC_SKULL_NECKLACE: case ACC_ROSARY: {
        int py = ty + (B.view == VK_PORTRAIT ? PX(8) : PCT(H, 40));
        uint8_t ch = st == ACC_ROSARY ? m.c[2] : gold.c[2], ch2 = st == ACC_ROSARY ? m.c[3] : gold.c[1];
        for (k = -5; k <= 5; k++) {
            int x = cx + k * SW(60) / 5, y = ty + 4 + (25 - k * k) * (py - ty - 8) / 25;
            pc_px(x >> 4, y >> 4, (k & 1) ? ch2 : ch);
            if (st == ACC_ROSARY && !tiny) pc_px((x >> 4) + 1, y >> 4, m.c[1]);
        }
        if (st == ACC_SKULL_NECKLACE) {
            PMat bone;
            pm_paint(&bone, PAINT_BONE);
            pc_part(&bone, SH_BALL, 0);
            pc_ellipse(cx, py + 16, tiny ? 12 : SW(20) + 10, tiny ? 10 : SW(18) + 8);
            if (!tiny) {
                pc_px((cx >> 4) - 1, (py + 16) >> 4, PAL_PAINT(PAINT_JET, 1));
                pc_px((cx >> 4) + 1, (py + 16) >> 4, PAL_PAINT(PAINT_JET, 1));
                pc_px(cx >> 4, ((py + 16) >> 4) + 2, bone.c[0]);
            }
        } else {
            PMat cm = st == ACC_ROSARY ? m : gold;
            int r = tiny ? 1 : (B.lod == LOD_BIG ? 3 : 2);
            pc_part(&cm, SH_GLOSS, 0);
            pc_rect(cx - 10, py, cx + 10, py + PX(r * 2));
            pc_rect(cx - PX(r) + 2, py + PX(r) - 10, cx + PX(r) - 2, py + PX(r) + 10);
        }
        break; }
    case ACC_SCARF:
        pc_part(&m, SH_SOFT, 0);
        pc_rrect(cx, ty + 4, PCT(B.sw, 80), tiny ? 12 : PX(2), tiny ? 8 : 24);
        pc_part(&m, SH_SOFT, 0);
        pc_quad(cx + B.sw / 4, ty + 8, cx + B.sw / 2, ty + 8, cx + B.sw / 2 + 8, ty + PCT(H, 55), cx + B.sw / 4 - 8, ty + PCT(H, 55));
        if (!tiny) {
            pc_decal(&m, SH_FLAT);
            pc_line((cx + B.sw / 4) >> 4, (ty + PCT(H, 50)) >> 4, (cx + B.sw / 2) >> 4, (ty + PCT(H, 50)) >> 4, m.c[0]);
        }
        break;
    case ACC_CHAINS: {
        PMat st2;
        pm_ramp(&st2, RAMP_STEEL, 5);
        for (k = -3; k <= 3; k++) {
            int x = cx + k * B.sw / 4, y = ty + 8 + (9 - k * k) * 6;
            pc_part(&st2, SH_GLOSS, 0);
            if (k & 1) pc_ellipse(x, y, tiny ? 10 : 22, tiny ? 8 : 14);
            else pc_ellipse(x, y, tiny ? 8 : 14, tiny ? 10 : 20);
        }
        break; }
    case ACC_CAPE: {
        PMat clasp = gold;
        pc_part(&clasp, SH_GLOSS, 0);
        pc_ellipse(cx - B.sw / 2, ty + 8, tiny ? 10 : 20, tiny ? 10 : 20);
        pc_ellipse(cx + B.sw / 2, ty + 8, tiny ? 10 : 20, tiny ? 10 : 20);
        break; }
    case ACC_BACKPACK: {
        pc_part(&m, SH_BALL, 0);
        pc_capsule(cx - PCT(B.sw, 55), ty, cx - PCT(B.sw, 50), ty + PCT(H, 55), tiny ? 8 : 14, tiny ? 8 : 14);
        pc_capsule(cx + PCT(B.sw, 55), ty, cx + PCT(B.sw, 50), ty + PCT(H, 55), tiny ? 8 : 14, tiny ? 8 : 14);
        break; }
    case ACC_TEDDY: {
        /* a little plush bear hugged in the paws */
        PMat t;
        int x = cx, y = B.view == VK_PORTRAIT ? PX(50) : ty + PCT(H, 50);
        int r = B.view == VK_PORTRAIT ? PX(8) : (tiny ? 28 : PCT(B.sw, 70));
        pm_paint(&t, l->accessory_color);
        pc_part(&t, SH_SOFT, PF_FUZZ);
        pc_ellipse(x - r * 6 / 10, y - r * 8 / 10, r * 3 / 10, r * 3 / 10);
        pc_ellipse(x + r * 6 / 10, y - r * 8 / 10, r * 3 / 10, r * 3 / 10);
        pc_ellipse(x, y - r / 3, r * 6 / 10, r * 5 / 10);
        pc_ellipse(x, y + r / 2, r * 7 / 10, r * 6 / 10);
        if (!tiny) {
            pc_px((x >> 4) - (r >> 6) - 1, (y - r / 3) >> 4, PAL_PAINT(PAINT_JET, 1));
            pc_px((x >> 4) + (r >> 6) + 1, (y - r / 3) >> 4, PAL_PAINT(PAINT_JET, 1));
            pc_px(x >> 4, ((y - r / 3) >> 4) + 1, t.c[0]);
        }
        break; }
    default:
        break;
    }
}

static void back_acc(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->accessory;
    PMat m = c->acc;
    if (st == ACC_CAPE) {
        int bot = B.view == VK_PORTRAIT ? PX(60) : B.by + PCT((B.ground - B.by), 70);
        pc_part(&m, SH_BALL, 0);
        pc_frame(B.cx, (B.ty + bot) / 2, B.sw + PX(3), (bot - B.ty) / 2 + 8);
        pc_quad(B.cx - B.sw - 8, B.ty + 8, B.cx + B.sw + 8, B.ty + 8, B.cx + B.sw + PX(4), bot, B.cx - B.sw - PX(4), bot);
        if (c->byaw >= 3) {
            pc_decal(&m, SH_FLAT);
            pc_line(B.cx >> 4, (B.ty >> 4) + 2, B.cx >> 4, (bot >> 4) - 1, m.c[1]);
        }
    } else if (st == ACC_BACKPACK) {
        int H = B.by - B.ty;
        pc_part(&m, SH_BALL, 0);
        if (c->byaw >= 3) {
            pc_rrect(B.cx, B.ty + PCT(H, 45), PCT(B.sw, 75), PCT(H, 38), 24);
            pc_decal(&m, SH_FLAT);
            pc_rrect(B.cx, B.ty + PCT(H, 58), PCT(B.sw, 50), PCT(H, 14), 12);
        } else {
            pc_rrect(B.cx + (c->byaw == 2 ? B.sw : 0), B.ty + PCT(H, 40), B.sw + 16, PCT(H, 38), 24);
        }
    }
}

/* ---------------------------------------------------------------------- */
void pb_draw_behind(PCtx *c)
{
    if (c->byaw < 3) back_acc(c);
    pb_draw_wings(c, 0);
    pb_draw_tail(c, 0);
    if (c->byaw < 3) hood_back(c);
}

void pb_draw_body(PCtx *c)
{
    const DoomoLook *l = c->l;
    int far = B.yaw == 2;
    if (far) {
        /* far arm + far leg behind the body */
        far_leg_profile(c);
        arm(c, 1, 1);
    }
    legs(c);
    bottoms(c);
    torso(c);
    if (!far && B.view != VK_PORTRAIT) {
        arm(c, 0, 0);
        arm(c, 1, 0);
    }
    if (B.view == VK_PORTRAIT) {
        int s;
        /* bust: shoulders + paws resting together at the chest */
        for (s = 0; s < 2; s++) {
            PMat sl, sk = c->skin;
            int kind = sleeve_mat(c, &sl);
            int x0 = B.shx[s], y0 = B.shy[s] + PX(2);
            if (l->top == TOP_CLOAK) { sl = c->top; kind = 2; }
            pc_part(kind >= 2 ? &sl : &sk, kind >= 2 ? top_shade(l->top) : SH_BALL,
                    c->critter && kind < 2 ? PF_FUZZ : 0);
            pc_capsule(x0, y0, B.hdx[s] + (s ? PX(2) : -PX(2)), B.hdy[s] - PX(1), B.arm_r, B.arm_r + 8);
            if (kind == 1) {
                pc_part(&sl, top_shade(l->top), 0);
                if (l->pattern != PATTERN_SOLID) pc_pattern(p_pattern_tiles[l->pattern % PATTERN_COUNT], &c->patm);
                pc_ellipse(x0, y0, B.arm_r + 16, B.arm_r + 20);
                pc_pattern(NULL, NULL);
            }
            if (l->top == TOP_ARMOR || l->top == TOP_PLATE_ARMOR) {
                PMat pa;
                if (l->top == TOP_ARMOR) pm_ramp(&pa, RAMP_STEEL, 5); else pm_paint(&pa, l->top_color);
                pc_part(&pa, SH_GLOSS, 0);
                pc_ellipse(x0, y0 - 8, B.arm_r + 24, B.arm_r + 8);
            }
        }
    }
    if (B.view == VK_PORTRAIT) {
        if (l->accessory != ACC_TEDDY) {
            paw(c, B.hdx[0], B.hdy[0], B.paw_r);
            paw(c, B.hdx[1], B.hdy[1], B.paw_r);
        }
    }
    if (far) arm(c, 0, 0);
}

/* in front of the chin: chest fluff, collar ruff, neckwear, a hugged teddy */
void pb_draw_neck(PCtx *c)
{
    collar(c);
    chest_fluff(c);
    mothkin_ruff(c);
    neck_acc(c);
    if (B.view == VK_PORTRAIT && c->l->accessory == ACC_TEDDY) {
        paw(c, B.hdx[0] - PX(4), B.hdy[0] - PX(1), B.paw_r);
        paw(c, B.hdx[1] + PX(4), B.hdy[1] - PX(1), B.paw_r);
    }
}

void pb_draw_front(PCtx *c)
{
    /* things in front of everything else in back views */
    if (c->byaw >= 3) {
        back_acc(c);
        pb_draw_wings(c, 1);
        pb_draw_tail(c, 1);
    }
}
