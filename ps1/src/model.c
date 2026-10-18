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
 * Everything is boxes, blobs and pyramids (prims.c): each face is lit (fixed sun + cell
 * light) and drawn as gouraud triangles with back-face culling, depth
 * sorted into the same ordering table as the level.
 */
#include <string.h>
#include "prims.h"

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
    PrimSwing leg_l, leg_r, arm_l, gun_arm;
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

    prim_xf.px = (bb->x >> 16) - c->px;
    prim_xf.py = (bb->y >> 16) - c->py;
    prim_xf.pz = (bb->z >> 16) - c->pz;
    dist2 = prim_xf.px * prim_xf.px + prim_xf.py * prim_xf.py;
    if (dist2 < 24 * 24)
        return 1;             /* camera inside the body: draw nothing */
    prim_xf.fx = icos(ang);
    prim_xf.fy = isin(ang);
    prim_xf.rx = -prim_xf.fy;
    prim_xf.ry = prim_xf.fx;
    prim_xf.light = light & 0xff;
    if (prim_xf.light < 48) prim_xf.light = 48;

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
    prim_box(-U(4), 0, U(7), U(3), U(3), U(5), bottom, &leg_l, 0);
    prim_box(U(4), 0, U(7), U(3), U(3), U(5), bottom, &leg_r, 0);
    prim_box(-U(4), U(1), U(2), U(3) + 8, U(4), U(2), shoes, &leg_l, 0);
    prim_box(U(4), U(1), U(2), U(3) + 8, U(4), U(2), shoes, &leg_r, 0);
    /* body (chubby plush torso) */
    prim_blob(0, 0, U(20) + bob, U(9), U(7), U(9), top, 0);
    prim_blob(0, 0, U(13) + bob, U(8), U(6), U(4), bottom, 0);                    /* waist */
    /* free arm + hand */
    prim_box(-U(10), 0, U(22) + bob, U(2) + 8, U(2) + 8, U(6), top, &arm_l, 0);
    prim_box(-U(10), 0, U(15) + bob, U(2) + 8, U(2) + 8, U(2), skin, &arm_l, 0);
    /* gun arm, hand and the toy gun */
    prim_box(U(10), 0, U(22) + bob, U(2) + 8, U(2) + 8, U(6), top, &gun_arm, 0);
    prim_box(U(10), 0, U(15) + bob, U(2) + 8, U(2) + 8, U(2), skin, &gun_arm, 0);
    prim_box(U(10), U(4) - recoil * 2, U(13) + bob, U(2), U(7), U(2) + 8, gun, &gun_arm, -1);
    prim_box(U(10), U(1) - recoil * 2, U(10) + bob, U(1) + 8, U(2), U(3), gun_d, &gun_arm, -1);
    prim_box(U(10), U(11) - recoil * 2, U(14) + bob, U(1), U(1), U(1), r3_rgb(255, 120, 160), &gun_arm, -1);
    /* tail */
    if (L->tail != TAIL_NONE) {
        int long_tail = L->tail == TAIL_CAT || L->tail == TAIL_FOX || L->tail == TAIL_DEVIL ||
                        L->tail == TAIL_LIZARD || L->tail == TAIL_MOUSE || L->tail == TAIL_RACCOON_RINGED;
        if (long_tail)
            prim_box(0, -U(9), U(15) + bob + ((isin(s_phase * 2 & 4095) * U(1)) >> 12),
                U(1) + 8, U(5), U(1) + 8, hair, NULL, 0);
        else
            prim_box(0, -U(7), U(15) + bob, U(3), U(2), U(3), r3_rgb(250, 250, 250), NULL, 0);
    }
    /* wings: two thin panels on the back */
    if (L->wings != WINGS_NONE) {
        uint32_t wc = L->wings == WINGS_ANGEL ? r3_rgb(250, 250, 255) :
                      L->wings == WINGS_BUTTERFLY ? pal_rgb(core_look_paint(L->hair_color, 2)) :
                      r3_rgb(70, 40, 80);
        int flap = (isin((s_phase * 2) & 4095) * U(2)) >> 12;
        prim_box(-U(8), -U(8), U(26) + bob + flap, U(6), 8, U(6), wc, NULL, 0);
        prim_box(U(8), -U(8), U(26) + bob + flap, U(6), 8, U(6), wc, NULL, 0);
    }
    /* head: big and round-ish (a box with a smaller "cheek" box) */
    {
        int hz = U(39) + bob + (bob >> 1);
        prim_blob(0, 0, hz, U(13), U(11), U(11), skin, 0);
        /* face, on the front (+y) surface of the head */
        prim_box(-U(5), U(10), hz, U(1) + 8, 8, U(2) + 8, eye, NULL, -2);
        prim_box(U(5), U(10), hz, U(1) + 8, 8, U(2) + 8, eye, NULL, -2);
        prim_box(-U(4) - 8, U(10) + 6, hz + U(1), 8, 8, 8, r3_rgb(255, 255, 255), NULL, -3);
        prim_box(U(5) + 8, U(10) + 6, hz + U(1), 8, 8, 8, r3_rgb(255, 255, 255), NULL, -3);
        if (L->cheeks) {
            prim_box(-U(9), U(8), hz - U(3), U(2), 8, U(1), r3_rgb(255, 140, 170), NULL, -2);
            prim_box(U(9), U(8), hz - U(3), U(2), 8, U(1), r3_rgb(255, 140, 170), NULL, -2);
        }
        prim_box(0, U(10) + 8, hz - U(4), U(1) + 8, 8, 8, skin_d, NULL, -2);         /* mouth */
        /* hair: a soft cap over the top and back of the head */
        if (L->hair_style != HAIR_BALD) {
            prim_blob(0, -U(2), hz + U(4), U(13) + 12, U(11), U(8), hair, -1);
            prim_blob(0, -U(6), hz - U(1), U(12), U(6), U(9), hair_d, 0);
            if (L->hair_style == HAIR_TWINTAILS || L->hair_style == HAIR_PIGTAILS) {
                prim_box(-U(13), -U(3), hz - U(2), U(2), U(2), U(6), hair, NULL, 0);
                prim_box(U(13), -U(3), hz - U(2), U(2), U(2), U(6), hair, NULL, 0);
            }
        }
        /* ears by species */
        switch (L->ears) {
        case EARS_CAT: case EARS_FOX: case EARS_POINTY: case EARS_BAT:
            prim_pyramid(-U(7), 0, hz + U(10), U(3), U(7), 0, hair, 0);
            prim_pyramid(U(7), 0, hz + U(10), U(3), U(7), 0, hair, 0);
            break;
        case EARS_BUNNY:
            prim_box(-U(5), 0, hz + U(17), U(2), U(1) + 8, U(8), skin, NULL, 0);
            prim_box(U(5), 0, hz + U(17), U(2), U(1) + 8, U(8), skin, NULL, 0);
            break;
        case EARS_BEAR: case EARS_MOUSE: case EARS_RACCOON: case EARS_SHEEP:
            prim_box(-U(10), 0, hz + U(9), U(3), U(1) + 8, U(3), hair, NULL, 0);
            prim_box(U(10), 0, hz + U(9), U(3), U(1) + 8, U(3), hair, NULL, 0);
            break;
        case EARS_FLOPPY_PUP: case EARS_DOG:
            prim_box(-U(12), 0, hz + U(1), U(1) + 8, U(3), U(6), hair_d, NULL, 0);
            prim_box(U(12), 0, hz + U(1), U(1) + 8, U(3), U(6), hair_d, NULL, 0);
            break;
        case EARS_ELF:
            prim_pyramid(-U(12), 0, hz + U(1), U(2), U(5), -U(2), skin, 0);
            prim_pyramid(U(12), 0, hz + U(1), U(2), U(5), -U(2), skin, 0);
            break;
        default:
            break;
        }
        /* horns */
        if (L->horns != HORNS_NONE) {
            uint32_t hc = pal_rgb(core_look_paint(L->horn_color, 1));
            int hh = L->horns == HORNS_NUBS ? U(3) : L->horns == HORNS_DEVIL_TALL ? U(9) : U(6);
            prim_pyramid(-U(5), U(3), hz + U(11), U(2), hh, U(1), hc, -1);
            prim_pyramid(U(5), U(3), hz + U(11), U(2), hh, U(1), hc, -1);
        }
        /* halo */
        if (L->halo != HALO_NONE)
            prim_box(0, 0, hz + U(17), U(7), U(7), 8, r3_rgb(255, 230, 120), NULL, 0);
    }
    return 1;
}
