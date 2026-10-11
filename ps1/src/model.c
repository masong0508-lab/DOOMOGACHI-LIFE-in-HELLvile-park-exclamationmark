/* model.c - the player's own citizen as a low-poly 3D model (third person).
 *
 * Instead of the flat billboard, the PS1 builds a chibi plush-toy figure
 * from the resident's look (town_res(doomo_player.resident_id)->look):
 * big round-ish head in the skin colour with eyes, glints, blush and mouth;
 * hair cap and back hair; species ears (cat / fox / bunny / bear / mouse /
 * pup / pointy); horns; halo; top, bottom and shoes in their paint colours;
 * a tail and wings when the look has them; and a chunky toy gun in the
 * right hand. It runs (leg / arm swing, body bob, head bounce), turns to
 * the aim angle, and the gun kicks back with the weapon recoil.
 *
 * Everything is boxes and pyramids: each face is lit (fixed sun + cell
 * light) and drawn as gouraud triangles with back-face culling, depth
 * sorted into the same ordering table as the level.
 */
#include <string.h>
#include "r3d.h"

typedef struct Xf {
    int32_t px, py, pz;      /* model origin (feet), world units relative to camera */
    int fx, fy;              /* facing (4.12) */
    int rx, ry;              /* right (4.12) */
    uint32_t light;          /* 0..255 grey of the cell light */
} Xf;

static Xf s_xf;

/* local (x right, y forward, z up) -> camera-relative world */
static void to_world(int lx, int ly, int lz, R3V *o)
{
    o->x = (int16_t)(s_xf.px + ((lx * s_xf.rx + ly * s_xf.fx) >> 12));
    o->y = (int16_t)(s_xf.py + ((lx * s_xf.ry + ly * s_xf.fy) >> 12));
    o->z = (int16_t)(s_xf.pz + lz);
}

/* light a face: normal in local space (unit 4.12) */
static uint32_t lit(uint32_t rgb, int nx, int ny, int nz)
{
    /* world normal (yaw only) */
    int wx = (nx * s_xf.rx + ny * s_xf.fx) >> 12;
    int wy = (nx * s_xf.ry + ny * s_xf.fy) >> 12;
    /* sun from the south-east, high up: (0.35, 0.45, 0.82) */
    int d = (wx * 1434 + wy * 1843 + nz * 3359) >> 12;
    int f = 176 + (d > 0 ? (d * 110) >> 12 : (d * 36) >> 12);    /* 140..286 */
    uint32_t r = ((rgb & 0xff) * (uint32_t)f) >> 8;
    uint32_t g = (((rgb >> 8) & 0xff) * (uint32_t)f) >> 8;
    uint32_t b = (((rgb >> 16) & 0xff) * (uint32_t)f) >> 8;
    r = (r * s_xf.light) >> 7; g = (g * s_xf.light) >> 7; b = (b * s_xf.light) >> 7;
    return r3_rgb((int)r, (int)g, (int)b);
}

/* local point with an optional swing (rotation about the local x axis
 * through pivot height pz0 / depth py0), in 1/16 units for precision */
typedef struct Swing { int s, c, py0, pz0; } Swing;

static void pt(int lx16, int ly16, int lz16, const Swing *sw, R3V *o)
{
    int ly = ly16, lz = lz16;
    if (sw) {
        int dy = ly - sw->py0, dz = lz - sw->pz0;
        ly = sw->py0 + ((dy * sw->c - dz * sw->s) >> 12);
        lz = sw->pz0 + ((dy * sw->s + dz * sw->c) >> 12);
    }
    to_world(lx16 >> 4, ly >> 4, lz >> 4, o);
}

static void face(R3V *a, R3V *b, R3V *c, R3V *d, uint32_t col, int bias)
{
    /* a b / d c around: quad in Z order (a, b, d, c) */
    R3V q[4];
    q[0] = *a; q[1] = *b; q[2] = *d; q[3] = *c;
    q[0].c = q[1].c = q[2].c = q[3].c = col;
    r3_quad_g(q, bias);
}

/* axis-aligned box in model space: centre (cx,cy,cz), half sizes, all in
 * 1/16 units; sw rotates it around a joint */
static void box(int cx, int cy, int cz, int hx, int hy, int hz, uint32_t rgb,
                const Swing *sw, int bias)
{
    R3V v[8];
    int i;
    for (i = 0; i < 8; i++)
        pt(cx + ((i & 1) ? hx : -hx), cy + ((i & 2) ? hy : -hy), cz + ((i & 4) ? hz : -hz),
           sw, &v[i]);
    /* faces wound counter-clockwise seen from outside */
    {
        int ny = sw ? sw->c : 4096, nz = sw ? sw->s : 0;
        face(&v[4], &v[5], &v[7], &v[6], lit(rgb, 0, -nz, ny), bias);         /* top    */
        face(&v[2], &v[3], &v[1], &v[0], lit(rgb, 0, nz, -ny), bias);         /* bottom */
        face(&v[6], &v[7], &v[3], &v[2], lit(rgb, 0, ny, nz), bias);          /* front  */
        face(&v[5], &v[4], &v[0], &v[1], lit(rgb, 0, -ny, -nz), bias);        /* back   */
        face(&v[7], &v[5], &v[1], &v[3], lit(rgb, 4096, 0, 0), bias);         /* right  */
        face(&v[4], &v[6], &v[2], &v[0], lit(rgb, -4096, 0, 0), bias);        /* left   */
    }
}

/* A smooth plush "blob": an ellipsoid with 8 longitudes and rings at
 * -90/-45/0/+45/+90 degrees, lit per vertex from its own normal (smooth
 * gouraud), so heads and tummies look round and soft. Centre and radii in
 * 1/16 units. squash flattens the bottom (sits on the body). */
static void blob(int cx, int cy, int cz, int rx, int ry, int rz, uint32_t rgb, int bias)
{
    static const int16_t k_cos8[8] = { 4096, 2896, 0, -2896, -4096, -2896, 0, 2896 };
    static const int16_t k_sin8[8] = { 0, 2896, 4096, 2896, 0, -2896, -4096, -2896 };
    static const int16_t k_lat_c[3] = { 2896, 4096, 2896 };   /* -45, 0, +45 */
    static const int16_t k_lat_s[3] = { -2896, 0, 2896 };
    R3V ring[3][8], pole[2];
    int i, j;
    for (j = 0; j < 3; j++)
        for (i = 0; i < 8; i++) {
            int nx = (k_cos8[i] * k_lat_c[j]) >> 12, ny = (k_sin8[i] * k_lat_c[j]) >> 12;
            int nz = k_lat_s[j];
            pt(cx + ((rx * nx) >> 12), cy + ((ry * ny) >> 12), cz + ((rz * nz) >> 12), NULL,
               &ring[j][i]);
            ring[j][i].c = lit(rgb, nx, ny, nz);
        }
    pt(cx, cy, cz - rz, NULL, &pole[0]);
    pole[0].c = lit(rgb, 0, 0, -4096);
    pt(cx, cy, cz + rz, NULL, &pole[1]);
    pole[1].c = lit(rgb, 0, 0, 4096);
    for (i = 0; i < 8; i++) {
        int k = (i + 1) & 7;
        R3V q[4], a, b, c;
        /* bands (counter-clockwise from outside) */
        for (j = 0; j < 2; j++) {
            q[0] = ring[j + 1][i]; q[1] = ring[j + 1][k];
            q[2] = ring[j][i];     q[3] = ring[j][k];
            r3_quad_g(q, bias);
        }
        a = ring[2][i]; b = ring[2][k]; c = pole[1];
        r3_tri_g(&a, &b, &c, bias);
        a = ring[0][k]; b = ring[0][i]; c = pole[0];
        r3_tri_g(&a, &b, &c, bias);
    }
}

/* square pyramid (ears, horns): base centre, half base, height (1/16) */
static void pyramid(int cx, int cy, int cz, int hb, int h, int lean_y, uint32_t rgb, int bias)
{
    R3V b[4], t;
    int i;
    for (i = 0; i < 4; i++)
        pt(cx + ((i == 1 || i == 2) ? hb : -hb), cy + ((i >= 2) ? hb : -hb), cz, NULL, &b[i]);
    pt(cx, cy + lean_y, cz + h, NULL, &t);
    {
        R3V a0, a1, a2;
        int k;
        for (k = 0; k < 4; k++) {
            static const int8_t nrm[4][2] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };
            a0 = b[k]; a1 = b[(k + 1) & 3]; a2 = t;
            a0.c = a1.c = a2.c = lit(rgb, nrm[k][0] * 3000, nrm[k][1] * 3000, 2800);
            r3_tri_g(&a0, &a1, &a2, bias);
        }
    }
}

/* ---- the look ----------------------------------------------------------------- */
static uint32_t pal_rgb(uint8_t idx) { return g_rgb[idx]; }

static DoomoLook s_default_look;
static int s_default_ready;
static const DoomoLook *s_look_override;

/* engine test: show this look instead of the player's (NULL = player) */
void model_set_look(const DoomoLook *l) { s_look_override = l; }

static const DoomoLook *player_look(void)
{
    DoomoPlayerState *p = core_player();
    const DoomoResident *r;
    if (s_look_override)
        return s_look_override;
    if (p) {
        r = core_resident(p->resident_id);
        if (r && r->status != RES_EMPTY)
            return &r->look;
    }
    /* no town yet (engine test, bring-up): a premade citizen, or our own */
    r = core_premade(ROSTER_ID_PIP);
    if (r)
        return &r->look;
    if (!s_default_ready) {
        memset(&s_default_look, 0, sizeof(s_default_look));
        s_default_look.species = SPECIES_BUNNYKIN;
        s_default_look.skin = 2;
        s_default_look.ears = EARS_BUNNY;
        s_default_look.hair_style = HAIR_BOB;
        s_default_look.hair_color = PAINT_BUBBLEGUM;
        s_default_look.eye_color = PAINT_GRAPE;
        s_default_look.top = TOP_HOODIE;
        s_default_look.top_color = PAINT_MINT;
        s_default_look.bottom = BOTTOM_SHORTS;
        s_default_look.bottom_color = PAINT_LAVENDER;
        s_default_look.shoes = SHOES_SNEAKERS;
        s_default_look.shoes_color = PAINT_CHERRY;
        s_default_look.horns = HORNS_DEVIL;
        s_default_look.horn_color = PAINT_CHERRY;
        s_default_look.tail = TAIL_BUNNY;
        s_default_look.cheeks = 1;
        s_default_ready = 1;
    }
    return &s_default_look;
}

/* ---- small math (no core dependency) --------------------------------------------- */
static uint32_t isqrt32(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

/* angle (0..4095) of the vector (x, y) */
static int atan2_4096(int y, int x)
{
    int lo = 0, hi = 1024, oct;
    int ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, a;
    if (!ax && !ay)
        return 0;
    /* angle in the first quadrant: smallest a with tan(a) >= ay/ax */
    while (hi - lo > 1) {
        int mid = (lo + hi) >> 1;
        if (isin(mid) * ax >= icos(mid) * ay) hi = mid; else lo = mid;
    }
    a = hi;
    oct = (x < 0) * 2 + (y < 0);
    switch (oct) {
    case 0: return a;
    case 1: return (4096 - a) & 4095;
    case 2: return 2048 - a;
    default: return 2048 + a;
    }
}

/* ---- animation state ------------------------------------------------------------ */
static int32_t s_lastx, s_lasty;
static int     s_phase, s_speed;
static int     s_have;

#define U(v) ((v) * 16)        /* model units -> 1/16 */

int model_player_draw(const DoomoView3D *v, const DoomoBillboard *bb, uint32_t light)
{
    const DoomoLook *L = player_look();
    DoomoPlayerState *ps = core_player();
    R3Cam *c = &g_cam;
    int ang, sw_s, sw_c, bob, recoil = 0, dist2;
    uint32_t skin, skin_d, top, bottom, shoes, hair, hair_d, eye, gun, gun_d;
    Swing leg_l, leg_r, arm_l, gun_arm;
    int32_t dx, dy;
    (void)v;
    if (!c->ot)
        return 0;
    /* movement since the last frame drives the run cycle */
    dx = (bb->x - s_lastx) >> 12;           /* 1/16 units */
    dy = (bb->y - s_lasty) >> 12;
    if (!s_have || dx > 4096 || dx < -4096 || dy > 4096 || dy < -4096)
        dx = dy = 0;
    s_have = 1;
    s_lastx = bb->x;
    s_lasty = bb->y;
    {
        int sp = (int)isqrt32((uint32_t)(dx * dx + dy * dy));    /* 1/16 units per frame */
        s_speed += (sp - s_speed) / 3;
    }
    s_phase = (s_phase + 40 + s_speed * 6) & 4095;
    /* facing: the player's aim, else the movement, else away from the camera */
    if (ps)
        ang = ps->angle & 4095;
    else if (s_speed > 8)
        ang = atan2_4096(dy, dx);
    else
        ang = c->ang;
    if (ps)
        recoil = ps->recoil > 0 ? (ps->recoil > 64 ? 64 : ps->recoil) : 0;

    s_xf.px = (bb->x >> 16) - c->px;
    s_xf.py = (bb->y >> 16) - c->py;
    s_xf.pz = (bb->z >> 16) - c->pz;
    dist2 = s_xf.px * s_xf.px + s_xf.py * s_xf.py;
    if (dist2 < 24 * 24)
        return 1;             /* camera inside the body: draw nothing */
    s_xf.fx = icos(ang);
    s_xf.fy = isin(ang);
    s_xf.rx = -s_xf.fy;
    s_xf.ry = s_xf.fx;
    s_xf.light = light & 0xff;
    if (s_xf.light < 48) s_xf.light = 48;

    skin = pal_rgb(core_look_skin(L, 1));
    skin_d = pal_rgb(core_look_skin(L, 0));
    top = pal_rgb(core_look_paint(L->top_color, 1));
    bottom = pal_rgb(core_look_paint(L->bottom_color, 1));
    shoes = pal_rgb(core_look_paint(L->shoes_color, 0));
    hair = pal_rgb(core_look_paint(L->hair_color, 1));
    hair_d = pal_rgb(core_look_paint(L->hair_color, 0));
    eye = pal_rgb(core_look_paint(L->eye_color, 0));
    gun = r3_rgb(96, 96, 112);
    gun_d = r3_rgb(40, 40, 52);
    if (L->top == TOP_NONE) top = skin;
    if (L->bottom == BOTTOM_NONE) bottom = skin_d;
    if (L->shoes == SHOES_BARE) shoes = skin_d;

    /* run cycle: legs and the free arm swing, the body bobs */
    sw_s = (isin(s_phase) * (s_speed > 60 ? 60 : s_speed)) / 60;     /* sin of swing */
    {
        int amp = (sw_s * 560) >> 12;         /* up to ~50 degrees */
        leg_l.s = isin(amp); leg_l.c = icos(amp); leg_l.py0 = 0; leg_l.pz0 = U(12);
        leg_r.s = -leg_l.s; leg_r.c = leg_l.c; leg_r.py0 = 0; leg_r.pz0 = U(12);
        arm_l.s = -leg_l.s; arm_l.c = leg_l.c; arm_l.py0 = 0; arm_l.pz0 = U(27);
    }
    sw_c = sw_s < 0 ? -sw_s : sw_s;
    bob = (sw_c * U(2)) >> 12;
    /* gun arm points forward (~80 degrees), kicked up by the recoil */
    gun_arm.s = isin(-900 - recoil * 6);
    gun_arm.c = icos(-900 - recoil * 6);
    gun_arm.py0 = 0;
    gun_arm.pz0 = U(27);

    /* legs + shoes */
    box(-U(4), 0, U(7), U(3), U(3), U(5), bottom, &leg_l, 0);
    box(U(4), 0, U(7), U(3), U(3), U(5), bottom, &leg_r, 0);
    box(-U(4), U(1), U(2), U(3) + 8, U(4), U(2), shoes, &leg_l, 0);
    box(U(4), U(1), U(2), U(3) + 8, U(4), U(2), shoes, &leg_r, 0);
    /* body (chubby plush torso) */
    blob(0, 0, U(20) + bob, U(9), U(7), U(9), top, 0);
    blob(0, 0, U(13) + bob, U(8), U(6), U(4), bottom, 0);                    /* waist */
    /* free arm + hand */
    box(-U(10), 0, U(22) + bob, U(2) + 8, U(2) + 8, U(6), top, &arm_l, 0);
    box(-U(10), 0, U(15) + bob, U(2) + 8, U(2) + 8, U(2), skin, &arm_l, 0);
    /* gun arm, hand and the toy gun */
    box(U(10), 0, U(22) + bob, U(2) + 8, U(2) + 8, U(6), top, &gun_arm, 0);
    box(U(10), 0, U(15) + bob, U(2) + 8, U(2) + 8, U(2), skin, &gun_arm, 0);
    box(U(10), U(4) - recoil * 2, U(13) + bob, U(2), U(7), U(2) + 8, gun, &gun_arm, -1);
    box(U(10), U(1) - recoil * 2, U(10) + bob, U(1) + 8, U(2), U(3), gun_d, &gun_arm, -1);
    box(U(10), U(11) - recoil * 2, U(14) + bob, U(1), U(1), U(1), r3_rgb(255, 120, 160), &gun_arm, -1);
    /* tail */
    if (L->tail != TAIL_NONE) {
        int long_tail = L->tail == TAIL_CAT || L->tail == TAIL_FOX || L->tail == TAIL_DEVIL ||
                        L->tail == TAIL_LIZARD || L->tail == TAIL_MOUSE || L->tail == TAIL_RACCOON_RINGED;
        if (long_tail)
            box(0, -U(9), U(15) + bob + ((isin(s_phase * 2 & 4095) * U(1)) >> 12),
                U(1) + 8, U(5), U(1) + 8, hair, NULL, 0);
        else
            box(0, -U(7), U(15) + bob, U(3), U(2), U(3), r3_rgb(250, 250, 250), NULL, 0);
    }
    /* wings: two thin panels on the back */
    if (L->wings != WINGS_NONE) {
        uint32_t wc = L->wings == WINGS_ANGEL ? r3_rgb(250, 250, 255) :
                      L->wings == WINGS_BUTTERFLY ? pal_rgb(core_look_paint(L->hair_color, 2)) :
                      r3_rgb(70, 40, 80);
        int flap = (isin((s_phase * 2) & 4095) * U(2)) >> 12;
        box(-U(8), -U(8), U(26) + bob + flap, U(6), 8, U(6), wc, NULL, 0);
        box(U(8), -U(8), U(26) + bob + flap, U(6), 8, U(6), wc, NULL, 0);
    }
    /* head: big and round-ish (a box with a smaller "cheek" box) */
    {
        int hz = U(39) + bob + (bob >> 1);
        blob(0, 0, hz, U(13), U(11), U(11), skin, 0);
        /* face, on the front (+y) surface of the head */
        box(-U(5), U(10), hz, U(1) + 8, 8, U(2) + 8, eye, NULL, -2);
        box(U(5), U(10), hz, U(1) + 8, 8, U(2) + 8, eye, NULL, -2);
        box(-U(4) - 8, U(10) + 6, hz + U(1), 8, 8, 8, r3_rgb(255, 255, 255), NULL, -3);
        box(U(5) + 8, U(10) + 6, hz + U(1), 8, 8, 8, r3_rgb(255, 255, 255), NULL, -3);
        if (L->cheeks) {
            box(-U(9), U(8), hz - U(3), U(2), 8, U(1), r3_rgb(255, 140, 170), NULL, -2);
            box(U(9), U(8), hz - U(3), U(2), 8, U(1), r3_rgb(255, 140, 170), NULL, -2);
        }
        box(0, U(10) + 8, hz - U(4), U(1) + 8, 8, 8, skin_d, NULL, -2);         /* mouth */
        /* hair: a soft cap over the top and back of the head */
        if (L->hair_style != HAIR_BALD) {
            blob(0, -U(2), hz + U(4), U(13) + 12, U(11), U(8), hair, -1);
            blob(0, -U(6), hz - U(1), U(12), U(6), U(9), hair_d, 0);
            if (L->hair_style == HAIR_TWINTAILS || L->hair_style == HAIR_PIGTAILS) {
                box(-U(13), -U(3), hz - U(2), U(2), U(2), U(6), hair, NULL, 0);
                box(U(13), -U(3), hz - U(2), U(2), U(2), U(6), hair, NULL, 0);
            }
        }
        /* ears by species */
        switch (L->ears) {
        case EARS_CAT: case EARS_FOX: case EARS_POINTY: case EARS_BAT:
            pyramid(-U(7), 0, hz + U(10), U(3), U(7), 0, hair, 0);
            pyramid(U(7), 0, hz + U(10), U(3), U(7), 0, hair, 0);
            break;
        case EARS_BUNNY:
            box(-U(5), 0, hz + U(17), U(2), U(1) + 8, U(8), skin, NULL, 0);
            box(U(5), 0, hz + U(17), U(2), U(1) + 8, U(8), skin, NULL, 0);
            break;
        case EARS_BEAR: case EARS_MOUSE: case EARS_RACCOON: case EARS_SHEEP:
            box(-U(10), 0, hz + U(9), U(3), U(1) + 8, U(3), hair, NULL, 0);
            box(U(10), 0, hz + U(9), U(3), U(1) + 8, U(3), hair, NULL, 0);
            break;
        case EARS_FLOPPY_PUP: case EARS_DOG:
            box(-U(12), 0, hz + U(1), U(1) + 8, U(3), U(6), hair_d, NULL, 0);
            box(U(12), 0, hz + U(1), U(1) + 8, U(3), U(6), hair_d, NULL, 0);
            break;
        case EARS_ELF:
            pyramid(-U(12), 0, hz + U(1), U(2), U(5), -U(2), skin, 0);
            pyramid(U(12), 0, hz + U(1), U(2), U(5), -U(2), skin, 0);
            break;
        default:
            break;
        }
        /* horns */
        if (L->horns != HORNS_NONE) {
            uint32_t hc = pal_rgb(core_look_paint(L->horn_color, 1));
            int hh = L->horns == HORNS_NUBS ? U(3) : L->horns == HORNS_DEVIL_TALL ? U(9) : U(6);
            pyramid(-U(5), U(3), hz + U(11), U(2), hh, U(1), hc, -1);
            pyramid(U(5), U(3), hz + U(11), U(2), hh, U(1), hc, -1);
        }
        /* halo */
        if (L->halo != HALO_NONE)
            box(0, 0, hz + U(17), U(7), U(7), 8, r3_rgb(255, 230, 120), NULL, 0);
    }
    return 1;
}
